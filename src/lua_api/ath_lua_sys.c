#include <string.h>
#include <stdlib.h>
#include <kernel.h>

#include "ath_lua.h"
#include <taskman.h>
#include <athena/iop_facade.h>
#include <athena/mutex.h>
#include <athena/task.h>

#ifdef ATHENA_AUDIO
#include <athena/sound.h>
#endif

/* ------------------------------------------------------------------ IOP -- */

/* init/end callbacks registered from Lua; refs are stored in module->*_args. */
static int iop_call_ref(void *ref)
{
    lua_State *L = athena_lua_state();
    if (L && ref) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, (int)(intptr_t)ref);
        lua_call(L, 0, 0);
    }
    return 0;
}

static int iop_init_cb(module_entry *module)
{
    return iop_call_ref(module->init_args);
}

static int iop_end_cb(module_entry *module)
{
    return iop_call_ref(module->end_args);
}

static iopman_func read_iop_callback(lua_State *L, int opts, const char *key, iopman_func lua_cb, int *ref)
{
    iopman_func fn = NULL;
    *ref = LUA_NOREF;

    int type = lua_getfield(L, opts, key);
    if (type == LUA_TFUNCTION) {
        *ref = luaL_ref(L, LUA_REGISTRYINDEX); /* pops */
        return lua_cb;
    }
    if (type == LUA_TNUMBER)
        fn = (iopman_func)athena_lua_checku32(L, -1);
    lua_pop(L, 1);
    return fn;
}

/* IOP.newModule(name, pathOrBuffer [, { deps = {...}, init = fn|ptr, end = fn|ptr }]) */
static int l_iop_new_module(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    void *data;
    size_t size = 0;
    uint8_t deps[4] = { EMPTY_ENTRY, EMPTY_ENTRY, EMPTY_ENTRY, EMPTY_ENTRY };
    iopman_func init = NULL, end = NULL;
    int init_ref = LUA_NOREF, end_ref = LUA_NOREF;

    if (lua_type(L, 2) == LUA_TSTRING) {
        data = (void *)strdup(lua_tostring(L, 2)); /* path; size 0 means "file" */
    } else {
        uint32_t len;
        const uint8_t *bytes = athena_lua_to_bytes(L, 2, &len);
        if (!bytes)
            return luaL_argerror(L, 2, "path or Buffer expected");
        /* The registry keeps the pointer: copy so it outlives the Buffer. */
        data = malloc(len);
        memcpy(data, bytes, len);
        size = len;
    }

    if (lua_istable(L, 3)) {
        if (lua_getfield(L, 3, "deps") == LUA_TTABLE) {
            int n = (int)luaL_len(L, -1);
            for (int i = 0; i < n && i < 4; i++) {
                lua_geti(L, -1, i + 1);
                deps[i] = (uint8_t)luaL_checkinteger(L, -1);
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);

        init = read_iop_callback(L, 3, "init", iop_init_cb, &init_ref);
        end = read_iop_callback(L, 3, "end", iop_end_cb, &end_ref);
    }

    module_entry *module = athena_iop_register_module(name, data, size, deps, init, end);
    if (!module)
        return luaL_error(L, "newModule: IOP module registry is full");

    if (init_ref != LUA_NOREF)
        module->init_args = (void *)(intptr_t)init_ref;
    if (end_ref != LUA_NOREF)
        module->end_args = (void *)(intptr_t)end_ref;

    athena_lua_pushu32(L, (uint32_t)module);
    return 1;
}

static module_entry *check_iop_module(lua_State *L, int idx)
{
    module_entry *module = (lua_type(L, idx) == LUA_TSTRING)
        ? athena_iop_get_module_by_name(lua_tostring(L, idx))
        : (module_entry *)athena_lua_checku32(L, idx);
    if (!module)
        luaL_error(L, "IOP module not found");
    return module;
}

static int l_iop_load_module(lua_State *L)
{
    module_entry *module = check_iop_module(L, 1);
    size_t len = 0;
    const char *args = luaL_optlstring(L, 2, NULL, &len);

    int ret = athena_iop_load_module(module, (int)len, args);
    if (ret == MODULE_STATUS_INCOMPATIBILITY)
        return luaL_error(L, "loadModule: %s is incompatible with %s, which is loaded. Keep or reset IOP.",
                          module->name, iopman_get_incompatible_module()->name);
    if (ret == MODULE_STATUS_ERROR)
        return luaL_error(L, "loadModule: error while loading %s", module->name);
    return 0;
}

static int l_iop_get_module(lua_State *L)
{
    module_entry *module = (lua_type(L, 1) == LUA_TSTRING)
        ? athena_iop_get_module_by_name(lua_tostring(L, 1))
        : athena_iop_get_module_by_id((uint8_t)luaL_checkinteger(L, 1));
    if (!module)
        return 0;
    athena_lua_pushu32(L, (uint32_t)module);
    return 1;
}

static int l_iop_get_modules(lua_State *L)
{
    AthenaIopModuleListing *listing = athena_iop_list_modules();
    lua_newtable(L);
    if (!listing)
        return 1;
    for (int i = 0; i < listing->count; i++) {
        lua_createtable(L, 0, 2);
        athena_lua_set_str(L, "name", listing->modules[i].name);
        athena_lua_set_int(L, "id", listing->modules[i].id);
        lua_rawseti(L, -2, i + 1);
    }
    athena_iop_module_listing_free(listing);
    return 1;
}

static int l_iop_reset(lua_State *L)
{
    athena_iop_reset();
    return 0;
}

static int l_iop_memory(lua_State *L)
{
    AthenaIopMemoryStats stats;
    athena_iop_get_memory_stats(&stats);
    lua_createtable(L, 0, 2);
    athena_lua_set_int(L, "free", stats.free);
    athena_lua_set_int(L, "used", stats.used);
    return 1;
}

static const luaL_Reg iop_funcs[] = {
    { "newModule", l_iop_new_module },
    { "loadModule", l_iop_load_module },
    { "getModule", l_iop_get_module },
    { "getModules", l_iop_get_modules },
    { "reset", l_iop_reset },
    { "getMemoryStats", l_iop_memory },
    { NULL, NULL }
};

/* ---------------------------------------------------------------- Mutex -- */

#define MUTEX_CLASS "Mutex"

typedef struct {
    AthenaMutex *mutex;
} LuaMutex;

static AthenaMutex *check_mutex(lua_State *L, int idx)
{
    LuaMutex *m = athena_lua_check_object(L, idx, MUTEX_CLASS);
    if (!m->mutex)
        luaL_error(L, "mutex was destroyed");
    return m->mutex;
}

static int l_mutex_new(lua_State *L)
{
    AthenaMutex *mutex = athena_mutex_create();
    if (!mutex)
        return luaL_error(L, "could not create mutex");
    LuaMutex *m = athena_lua_new_object(L, MUTEX_CLASS, sizeof(LuaMutex));
    m->mutex = mutex;
    return 1;
}

static int l_mutex_gc(lua_State *L)
{
    LuaMutex *m = athena_lua_check_object(L, 1, MUTEX_CLASS);
    if (m->mutex)
        athena_mutex_destroy(m->mutex);
    m->mutex = NULL;
    return 0;
}

static int l_mutex_lock(lua_State *L)
{
    athena_mutex_lock(check_mutex(L, 1));
    return 0;
}

static int l_mutex_unlock(lua_State *L)
{
    athena_mutex_unlock(check_mutex(L, 1));
    return 0;
}

static const luaL_Reg mutex_methods[] = {
    { "lock", l_mutex_lock },
    { "unlock", l_mutex_unlock },
    { NULL, NULL }
};

static const athena_lua_class mutex_class = {
    .name = MUTEX_CLASS, .methods = mutex_methods, .gc = l_mutex_gc,
};

/* --------------------------------------------------------------- Thread --
 * Lua states are not thread-safe, so Thread runs its function as a task of
 * the coroutine scheduler instead of on a separate EE thread: it advances
 * whenever it yields (sleep, await, Thread.yield). Blocking native work
 * (file copies, async loaders, network) still runs on EE threads in C.
 * Thread.list/kill operate on the real EE task table, as in JS. */

static int l_thread_list(lua_State *L)
{
    Task *tasks = get_tasks();
    lua_newtable(L);
    int n = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (is_invalid_task(&tasks[i]))
            continue;
        lua_createtable(L, 0, 4);
        athena_lua_set_int(L, "id", tasks[i].id);
        athena_lua_set_str(L, "name", tasks[i].title);
        athena_lua_set_int(L, "status", tasks[i].status);
        athena_lua_set_int(L, "stack", tasks[i].stack_size);
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}

static int l_thread_kill(lua_State *L)
{
    athena_thread_kill((int)luaL_checkinteger(L, 1));
    return 0;
}

static const char thread_impl[] =
"local Thread = ...\n"
"local nextId = 1\n"
"local mt = { __index = {} }\n"
"local methods = mt.__index\n"
"function methods:start()\n"
"  if self.task and not self.task.done then return end\n"
"  self.task = async(self.fn)\n"
"end\n"
"function methods:stop()\n"
"  if self.task then self.task.done = true end\n"
"end\n"
"function methods:running() return self.task ~= nil and not self.task.done end\n"
"local function new(fn, name)\n"
"  assert(type(fn) == 'function', 'Thread(fn [, name]) expects a function')\n"
"  local t = setmetatable({ fn = fn, name = name or 'Athena: Worker thread', id = -nextId }, mt)\n"
"  nextId = nextId + 1\n"
"  return t\n"
"end\n"
"Thread.new = new\n"
"function Thread.yield() return coroutine.yield() end\n"
"setmetatable(Thread, { __call = function(_, ...) return new(...) end })\n";

static const luaL_Reg thread_funcs[] = {
    { "list", l_thread_list },
    { "kill", l_thread_kill },
    { NULL, NULL }
};

#ifdef ATHENA_AUDIO
/* ---------------------------------------------------------------- Sound -- */

#define STREAM_CLASS "Sound.Stream"
#define SFX_CLASS "Sound.Sfx"

static void anchor_playing(lua_State *L, int idx, bool on);

typedef struct {
    AthenaSound *sound;
} LuaStream;

typedef struct {
    AthenaSfx *sfx;
} LuaSfx;

static AthenaSound *check_stream(lua_State *L, int idx)
{
    LuaStream *s = athena_lua_check_object(L, idx, STREAM_CLASS);
    if (!s->sound)
        luaL_error(L, "sound stream was freed");
    return s->sound;
}

static AthenaSfx *check_sfx(lua_State *L, int idx)
{
    LuaSfx *s = athena_lua_check_object(L, idx, SFX_CLASS);
    if (!s->sfx)
        luaL_error(L, "sound effect was freed");
    return s->sfx;
}

static int l_sound_set_volume(lua_State *L)
{
    athena_sound_set_volume((int)luaL_checkinteger(L, 1));
    return 0;
}

static int l_sound_find_channel(lua_State *L)
{
    lua_pushinteger(L, athena_sound_find_channel());
    return 1;
}

static int l_stream_new(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    AthenaSound *sound = athena_sound_load(path);
    if (!sound)
        return luaL_error(L, "could not load sound '%s'", path);
    LuaStream *s = athena_lua_new_object(L, STREAM_CLASS, sizeof(LuaStream));
    s->sound = sound;
    return 1;
}

static int l_stream_gc(lua_State *L)
{
    LuaStream *s = athena_lua_check_object(L, 1, STREAM_CLASS);
    if (s->sound)
        athena_sound_free(s->sound);
    s->sound = NULL;
    return 0;
}

static int l_stream_free(lua_State *L)
{
    anchor_playing(L, 1, false);
    LuaStream *s = athena_lua_check_object(L, 1, STREAM_CLASS);
    if (s->sound)
        athena_sound_free(s->sound);
    s->sound = NULL;
    return 0;
}

/* A playing sound must not be collected just because the script dropped its
 * last reference (in JS, module-level consts live for the whole script). */
static const char playing_key = 0;

static void anchor_playing(lua_State *L, int idx, bool on)
{
    if (lua_rawgetp(L, LUA_REGISTRYINDEX, &playing_key) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_rawsetp(L, LUA_REGISTRYINDEX, &playing_key);
    }
    lua_pushvalue(L, idx);
    if (on)
        lua_pushboolean(L, 1);
    else
        lua_pushnil(L);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

static int l_stream_play(lua_State *L) { athena_sound_play(check_stream(L, 1)); anchor_playing(L, 1, true); return 0; }
static int l_stream_pause(lua_State *L) { check_stream(L, 1); athena_sound_pause(); anchor_playing(L, 1, false); return 0; }
static int l_stream_rewind(lua_State *L) { athena_sound_rewind(check_stream(L, 1)); return 0; }
static int l_stream_playing(lua_State *L) { lua_pushboolean(L, athena_sound_is_playing(check_stream(L, 1))); return 1; }

static int get_stream_loop(lua_State *L) { lua_pushboolean(L, athena_sound_get_loop(check_stream(L, 1))); return 1; }
static int set_stream_loop(lua_State *L) { athena_sound_set_loop(check_stream(L, 1), athena_lua_tobool(L, 2)); return 0; }
static int get_stream_position(lua_State *L) { lua_pushinteger(L, athena_sound_get_position(check_stream(L, 1))); return 1; }
static int set_stream_position(lua_State *L) { athena_sound_set_position(check_stream(L, 1), athena_lua_checku32(L, 2)); return 0; }
static int get_stream_length(lua_State *L) { lua_pushinteger(L, athena_sound_get_duration(check_stream(L, 1))); return 1; }

static const luaL_Reg stream_methods[] = {
    { "play", l_stream_play }, { "free", l_stream_free }, { "pause", l_stream_pause },
    { "playing", l_stream_playing }, { "rewind", l_stream_rewind },
    { NULL, NULL }
};

static const athena_lua_prop stream_props[] = {
    { "loop", get_stream_loop, set_stream_loop },
    { "position", get_stream_position, set_stream_position },
    { "length", get_stream_length, NULL },
    { NULL, NULL, NULL }
};

static const athena_lua_class stream_class = {
    .name = STREAM_CLASS, .methods = stream_methods, .props = stream_props, .gc = l_stream_gc,
};

static int l_sfx_new(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    AthenaSfx *sfx = athena_sfx_load(path);
    if (!sfx)
        return luaL_error(L, "could not load sound effect '%s'", path);
    LuaSfx *s = athena_lua_new_object(L, SFX_CLASS, sizeof(LuaSfx));
    s->sfx = sfx;
    return 1;
}

static int l_sfx_gc(lua_State *L)
{
    LuaSfx *s = athena_lua_check_object(L, 1, SFX_CLASS);
    if (s->sfx)
        athena_sfx_free(s->sfx);
    s->sfx = NULL;
    return 0;
}

static int l_sfx_free(lua_State *L)
{
    anchor_playing(L, 1, false);
    LuaSfx *s = athena_lua_check_object(L, 1, SFX_CLASS);
    if (s->sfx)
        athena_sfx_free(s->sfx);
    s->sfx = NULL;
    return 0;
}

static int l_sfx_play(lua_State *L)
{
    anchor_playing(L, 1, true);
    lua_pushinteger(L, athena_sfx_play(check_sfx(L, 1), (int)luaL_optinteger(L, 2, -1)));
    return 1;
}

static int l_sfx_playing(lua_State *L)
{
    lua_pushboolean(L, athena_sfx_is_playing(check_sfx(L, 1), (int)luaL_optinteger(L, 2, -1)));
    return 1;
}

static int get_sfx_length(lua_State *L) { lua_pushinteger(L, athena_sfx_get_length(check_sfx(L, 1))); return 1; }
static int get_sfx_volume(lua_State *L) { lua_pushinteger(L, athena_sfx_get_volume(check_sfx(L, 1))); return 1; }
static int set_sfx_volume(lua_State *L) { athena_sfx_set_volume(check_sfx(L, 1), athena_lua_checku32(L, 2)); return 0; }
static int get_sfx_pan(lua_State *L) { lua_pushinteger(L, athena_sfx_get_pan(check_sfx(L, 1))); return 1; }
static int set_sfx_pan(lua_State *L) { athena_sfx_set_pan(check_sfx(L, 1), athena_lua_checku32(L, 2)); return 0; }
static int get_sfx_loop(lua_State *L) { lua_pushboolean(L, athena_sfx_get_loop(check_sfx(L, 1))); return 1; }
static int set_sfx_loop(lua_State *L) { athena_sfx_set_loop(check_sfx(L, 1), athena_lua_tobool(L, 2)); return 0; }
static int get_sfx_pitch(lua_State *L) { lua_pushinteger(L, athena_sfx_get_pitch(check_sfx(L, 1))); return 1; }
static int set_sfx_pitch(lua_State *L) { athena_sfx_set_pitch(check_sfx(L, 1), (int)luaL_checkinteger(L, 2)); return 0; }

static const luaL_Reg sfx_methods[] = {
    { "play", l_sfx_play }, { "free", l_sfx_free }, { "playing", l_sfx_playing },
    { NULL, NULL }
};

static const athena_lua_prop sfx_props[] = {
    { "length", get_sfx_length, NULL },
    { "volume", get_sfx_volume, set_sfx_volume },
    { "pan", get_sfx_pan, set_sfx_pan },
    { "loop", get_sfx_loop, set_sfx_loop },
    { "pitch", get_sfx_pitch, set_sfx_pitch },
    { NULL, NULL, NULL }
};

static const athena_lua_class sfx_class = {
    .name = SFX_CLASS, .methods = sfx_methods, .props = sfx_props, .gc = l_sfx_gc,
};

static const luaL_Reg sound_funcs[] = {
    { "setVolume", l_sound_set_volume },
    { "findChannel", l_sound_find_channel },
    { "Stream", l_stream_new },
    { "Sfx", l_sfx_new },
    { NULL, NULL }
};
#endif

void athena_lua_open_sys(lua_State *L)
{
    athena_lua_new_module(L, iop_funcs, NULL, NULL);
    athena_lua_set_global_module(L, "IOP");

    athena_lua_register_class(L, &mutex_class);
    athena_lua_new_module(L, NULL, NULL, l_mutex_new);
    athena_lua_set_global_module(L, "Mutex");

    athena_lua_new_module(L, thread_funcs, NULL, NULL);
    if (luaL_loadbuffer(L, thread_impl, sizeof(thread_impl) - 1, "=Thread") != LUA_OK)
        lua_error(L);
    lua_pushvalue(L, -2);
    lua_call(L, 1, 0);
    athena_lua_set_global_module(L, "Thread");

#ifdef ATHENA_AUDIO
    athena_lua_register_class(L, &stream_class);
    athena_lua_register_class(L, &sfx_class);
    athena_lua_new_module(L, sound_funcs, NULL, NULL);
    athena_lua_set_global_module(L, "Sound");
#endif
}
