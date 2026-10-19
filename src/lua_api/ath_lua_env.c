#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <kernel.h>

#include <erl.h>
#include <memory.h>
#include <taskman.h>

#include "ath_lua.h"
#include <athena/pad.h>

#ifdef ATHENA_GRAPHICS
#include <graphics.h>
#include <athena/screen.h>
#endif

/*
 * Lua runtime: VM lifetime, module loading and the main loop.
 *
 * The loop mirrors js_std_loop(): after the main chunk returns, it keeps
 * running while there is a render function (Screen.display), pad events, or
 * pending timers/coroutines from the prelude scheduler.
 */

#define LUAFILE_NOTFOUND -5656

enum { PRESSED_EVENT, JUSTPRESSED_EVENT, NONPRESSED_EVENT };

typedef struct {
    int ref;
    uint32_t buttons;
    int flavour;
} lua_pad_event;

static lua_State *g_L = NULL;

static size_t mem_used = 0;
static size_t mem_limit = 0;

static char error_buf[4096];
static bool has_error = false;
static bool reload_requested = false;

static int render_ref = LUA_NOREF;
static int tick_ref = LUA_NOREF;

static lua_pad_event pad_events[64];
static int total_pad_events = 0;
static AthenaPad *input_handler = NULL;

/* Unique address used as the error object that unwinds the VM on reload. */
static const char reload_sentinel = 0;

lua_State *athena_lua_state(void)
{
    return g_L;
}

void athena_lua_record_error(const char *msg)
{
    if (has_error)
        return;
    has_error = true;
    /* trailing newline: the error screen printf()s this and a partial last
     * line would sit in the stdout buffer */
    snprintf(error_buf, sizeof(error_buf), "%s\n", msg ? msg : "unknown Lua error");
}

bool athena_lua_is_reload_error(lua_State *L, int idx)
{
    return lua_touserdata(L, idx) == (void *)&reload_sentinel;
}

int athena_lua_request_reload(lua_State *L, const char *script)
{
    set_default_script(script);
    reload_requested = true;
    lua_pushlightuserdata(L, (void *)&reload_sentinel);
    return lua_error(L);
}

static void *lua_ps2_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    size_t old = ptr ? osize : 0;

    if (nsize == 0) {
        free(ptr);
        mem_used -= old;
        return NULL;
    }

    /* Returning NULL makes Lua run an emergency full GC and retry. */
    if (mem_limit && mem_used - old + nsize > mem_limit)
        return NULL;

    void *n = realloc(ptr, nsize);
    if (n)
        mem_used = mem_used - old + nsize;
    return n;
}

size_t athena_lua_mem_used(void)
{
    return mem_used;
}

/* --- render loop & pad events ------------------------------------------- */

void athena_lua_set_render_loop(lua_State *L, int idx)
{
    athena_lua_unref(L, render_ref);
    render_ref = athena_lua_ref(L, idx);
}

void athena_lua_set_input_event_handler(void *pad)
{
    input_handler = (AthenaPad *)pad;
}

void athena_lua_forget_input_event_handler(void *pad)
{
    if (input_handler == pad)
        input_handler = NULL;
}

int athena_lua_new_input_event(lua_State *L, int buttons, int idx, int flavour)
{
    luaL_checktype(L, idx, LUA_TFUNCTION);
    for (int i = 0; i < 64; i++) {
        if (pad_events[i].ref == LUA_NOREF) {
            pad_events[i].ref = athena_lua_ref(L, idx);
            pad_events[i].buttons = buttons;
            pad_events[i].flavour = flavour;
            total_pad_events++;
            return i;
        }
    }
    return -1;
}

void athena_lua_delete_input_event(lua_State *L, int id)
{
    if (id < 0 || id >= 64 || pad_events[id].ref == LUA_NOREF)
        return;
    athena_lua_unref(L, pad_events[id].ref);
    pad_events[id].ref = LUA_NOREF;
    pad_events[id].buttons = 0;
    pad_events[id].flavour = PRESSED_EVENT;
    total_pad_events--;
}

static void reset_loop_state(void)
{
    render_ref = LUA_NOREF;
    tick_ref = LUA_NOREF;
    for (int i = 0; i < 64; i++) {
        pad_events[i].ref = LUA_NOREF;
        pad_events[i].buttons = 0;
        pad_events[i].flavour = PRESSED_EVENT;
    }
    total_pad_events = 0;
    input_handler = NULL;
}

static void dispatch_pad_events(lua_State *L)
{
    if (!input_handler || !total_pad_events)
        return;

    athena_pad_update(input_handler);

    for (int i = 0; i < 64 && !has_error; i++) {
        if (pad_events[i].ref == LUA_NOREF)
            continue;

        bool trigger = false;
        uint32_t b = pad_events[i].buttons;
        switch (pad_events[i].flavour) {
            case PRESSED_EVENT:
                trigger = input_handler->btns & b;
                break;
            case JUSTPRESSED_EVENT:
                trigger = (input_handler->btns & b) && !(input_handler->old_btns & b);
                break;
            case NONPRESSED_EVENT:
                trigger = !(input_handler->btns & b);
                break;
        }

        if (trigger)
            athena_lua_call_ref(L, pad_events[i].ref, 0, 0);
    }
}

static bool ticking = false;

static bool run_tick(lua_State *L)
{
    if (tick_ref == LUA_NOREF || ticking)
        return false;

    ticking = true;
    bool ok = athena_lua_call_ref(L, tick_ref, 0, 1);
    ticking = false;
    if (!ok)
        return false;

    bool pending = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return pending;
}

/* Runs due timers and ready tasks from inside a script's own loop (called by
 * Screen.flip), so `while true do ... Screen.flip() end` scripts get
 * os.setTimeout and async/await too. Errors propagate to the caller. */
void athena_lua_pump(lua_State *L)
{
    if (tick_ref == LUA_NOREF || ticking)
        return;

    ticking = true;
    lua_rawgeti(L, LUA_REGISTRYINDEX, tick_ref);
    int status = lua_pcall(L, 0, 0, 0);
    ticking = false;
    if (status != LUA_OK)
        lua_error(L);
}

static void main_loop(lua_State *L)
{
    for (;;) {
        bool pending = run_tick(L);
        if (has_error || reload_requested)
            break;

        if (render_ref != LUA_NOREF) {
#ifdef ATHENA_GRAPHICS
            clearScreen(athena_screen_get_clear_color());
#endif
            athena_lua_call_ref(L, render_ref, 0, 0);
#ifdef ATHENA_GRAPHICS
            flipScreen();
#endif
        }

        if (has_error || reload_requested)
            break;

        dispatch_pad_events(L);

        if (has_error || reload_requested)
            break;

        if (render_ref == LUA_NOREF && !total_pad_events) {
            if (!pending)
                break;
            usleep(1000);
        }
    }
}

/* --- os extensions ------------------------------------------------------ */

static int l_os_millis(lua_State *L)
{
    uint64_t ms = ((uint64_t)clock() * 1000) / CLOCKS_PER_SEC;
    lua_pushinteger(L, (lua_Integer)(uint32_t)ms);
    return 1;
}

static int l_os_chdir(lua_State *L)
{
    lua_pushinteger(L, chdir(luaL_checkstring(L, 1)));
    return 1;
}

static int l_os_getcwd(lua_State *L)
{
    char buf[256];
    if (!getcwd(buf, sizeof(buf)))
        return 0;
    lua_pushstring(L, buf);
    return 1;
}

static int l_os_sleepms(lua_State *L)
{
    usleep((useconds_t)luaL_checkinteger(L, 1) * 1000);
    return 0;
}

static int l_set_tick(lua_State *L)
{
    athena_lua_unref(L, tick_ref);
    tick_ref = athena_lua_ref(L, 1);
    return 0;
}

static const luaL_Reg os_ext[] = {
    { "millis", l_os_millis },
    { "chdir",  l_os_chdir },
    { "getcwd", l_os_getcwd },
    { "sleepms", l_os_sleepms },
    { NULL, NULL }
};

/* Lua-side runtime pieces (src/lua_api/lua/*.lua, embedded by scripts/lua2c.py):
 * json.lua -> JSON global and require "json"; prelude.lua -> timers, the
 * async/await scheduler, console.log and io helpers. */
extern const char athena_lua_src_json[];
extern const size_t athena_lua_src_json_size;
extern const char athena_lua_src_prelude[];
extern const size_t athena_lua_src_prelude_size;

static bool load_embedded(lua_State *L, const char *src, size_t size, const char *name)
{
    if (luaL_loadbuffer(L, src, size, name) != LUA_OK) {
        athena_lua_record_error(lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

static bool run_embedded(lua_State *L)
{
    /* JSON first: the prelude's console.log uses it. */
    if (!load_embedded(L, athena_lua_src_json, athena_lua_src_json_size, "=json") || !athena_lua_pcall(L, 0, 1))
        return false;
    lua_pushvalue(L, -1);
    lua_setglobal(L, "JSON");
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_insert(L, -2);
    lua_setfield(L, -2, "json");
    lua_pop(L, 1);

    if (!load_embedded(L, athena_lua_src_prelude, athena_lua_src_prelude_size, "=prelude"))
        return false;
    lua_pushcfunction(L, l_set_tick);
    return athena_lua_pcall(L, 1, 0);
}

/* --- require ------------------------------------------------------------- */

typedef int (*lua_erl_init_fn)(lua_State *L);

/* Loads (or finds, if already loaded) the relocatable `name`, relative to
 * the current directory first and then to the boot path. */
static struct erl_record_t *load_erl_named(const char *name)
{
    char erl_id[64];
    const char *base = name;
    for (const char *p = name; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':')
            base = p + 1;
    snprintf(erl_id, sizeof(erl_id), "%s", base);
    erl_id[strlen(erl_id) - 4] = '\0';

    struct erl_record_t *erl = find_erl(erl_id);
    if (erl)
        return erl;

    FILE *f = fopen(name, "rb");
    if (f) {
        fclose(f);
        return _init_load_erl_from_file((char *)name, erl_id);
    }

    if (!strchr(name, ':') && name[0] != '/' && boot_path[0] != '\0') {
        char full_path[512];
        size_t bp_len = strlen(boot_path);
        int has_sep = bp_len > 0 && (boot_path[bp_len - 1] == '/' || boot_path[bp_len - 1] == ':');
        snprintf(full_path, sizeof(full_path), "%s%s%s", boot_path, has_sep ? "" : "/", name);
        f = fopen(full_path, "rb");
        if (f) {
            fclose(f);
            return _init_load_erl_from_file(full_path, erl_id);
        }
    }
    return NULL;
}

static int erl_searcher(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    size_t len = strlen(name);
    if (len <= 4 || strcmp(name + len - 4, ".erl") != 0) {
        lua_pushstring(L, "not an .erl module");
        return 1;
    }

    /* The bundled subsystems have a Lua build named <name>_lua.erl, so
     * require "athena_network.erl" means the same in JS and Lua scripts.
     * Try that first, then the name as given (user native modules). */
    char lua_name[256];
    snprintf(lua_name, sizeof(lua_name), "%.*s_lua.erl", (int)(len - 4), name);

    struct erl_record_t *erl = load_erl_named(lua_name);
    if (!erl)
        erl = load_erl_named(name);

    if (!erl) {
        lua_pushfstring(L, "could not load relocatable module '%s'", name);
        return 1;
    }

    struct symbol_t *sym = erl_find_local_symbol("lua_init_module", erl);
    if (!sym) {
        lua_pushfstring(L, "'%s' has no lua_init_module entry point (built for JS?)", name);
        return 1;
    }

    lua_pushcfunction(L, (lua_CFunction)(lua_erl_init_fn)sym->address);
    lua_pushstring(L, name);
    return 2;
}

static void setup_package(lua_State *L)
{
    lua_getglobal(L, "package");
    lua_pushstring(L, "?.lua;?/init.lua");
    lua_setfield(L, -2, "path");
    lua_pushstring(L, "");
    lua_setfield(L, -2, "cpath");

    /* searchers: preload, lua files, erl. Drop the C searchers (no dlopen). */
    lua_getfield(L, -1, "searchers");
    lua_pushnil(L); lua_rawseti(L, -2, 4);
    lua_pushcfunction(L, erl_searcher); lua_rawseti(L, -2, 3);
    lua_pop(L, 2);
}

/* --- entry point --------------------------------------------------------- */

static const char *run_once(const char *script)
{
    has_error = false;
    reload_requested = false;
    reset_loop_state();

    mem_used = 0;
    mem_limit = (GetMemorySize() - get_used_memory()) >> 1;

    /* taskman names the main thread after the JS runtime; this one is ours. */
    get_tasks()[0].title = "AthenaEnv: Lua Runtime";

    lua_State *L = lua_newstate(lua_ps2_alloc, NULL);
    if (!L)
        return "AthenaError: Lua state creation";
    g_L = L;

    lua_gc(L, LUA_GCGEN, 0, 0);

    luaL_openlibs(L);
    setup_package(L);

    lua_getglobal(L, "os");
    luaL_setfuncs(L, os_ext, 0);
    lua_pop(L, 1);

    athena_lua_register_modules(L);
    run_embedded(L);

    while (!bootlogo_finished() && boot_logo)
        usleep(1000);

    if (!has_error) {
        int status = luaL_loadfile(L, script);
        if (status == LUA_ERRFILE) {
            lua_pop(L, 1);
            snprintf(error_buf, sizeof(error_buf),
                     "AthenaError: Fail when opening %s\n"
                     "\nTip: If you are on PCSX2, check Host filesystem!\n", script);
            has_error = true;
        } else if (status != LUA_OK) {
            athena_lua_record_error(lua_tostring(L, -1));
            lua_pop(L, 1);
        } else if (athena_lua_pcall(L, 0, 0)) {
            main_loop(L);
        }
    }

    reset_loop_state();
    lua_close(L);
    g_L = NULL;

    return has_error ? error_buf : NULL;
}

const char *athena_lua_run_script(const char *script)
{
    const char *err;
    do {
        err = run_once(script);
        script = athena_get_default_script();
    } while (!err && reload_requested);

    return err;
}
