#ifndef ATH_LUA_H
#define ATH_LUA_H

/*
 * Lua 5.4 bindings for the AthenaEnv pure-C API (src/include/athena/).
 * Mirrors src/js_api/: one ath_lua_<module>.c per module, all thin wrappers.
 *
 * Lua is built with LUA_32BITS (int32 integers, float numbers) because the EE
 * FPU is single precision only. Colors and other uint32 values travel as
 * (possibly negative) int32 integers; always read them with athena_lua_checku32.
 */

#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>

#include "../lua/lua.h"
#include "../lua/lauxlib.h"
#include "../lua/lualib.h"

#include <athena_core.h>
#include <dbgprintf.h>
#include <macros.h>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

/* --- class / module helpers --------------------------------------------- */

typedef struct {
    const char *name;
    lua_CFunction get;  /* (self) -> value */
    lua_CFunction set;  /* (self, value)   */
} athena_lua_prop;

typedef struct {
    const char *name;
    lua_Integer value;
} athena_lua_const;

typedef struct {
    const char *name;            /* metatable name, also used in error messages */
    const luaL_Reg *methods;     /* may be NULL; NULL-terminated */
    const athena_lua_prop *props;/* may be NULL; NULL-terminated */
    lua_CFunction gc;            /* may be NULL */
    lua_CFunction tostring;      /* may be NULL */
    const luaL_Reg *meta;        /* extra metamethods (__add, __eq...), may be NULL */
} athena_lua_class;

/* Registers the class metatable in the registry under def->name. */
void athena_lua_register_class(lua_State *L, const athena_lua_class *def);

/* Allocates a userdata of `size` bytes with the class metatable attached. The
 * userdata has one user value slot that holds a lazily created table for
 * script-defined fields (obj.foo = 1 on a native object works like in JS). */
void *athena_lua_new_object(lua_State *L, const char *class_name, size_t size);

void *athena_lua_check_object(lua_State *L, int idx, const char *class_name);
void *athena_lua_test_object(lua_State *L, int idx, const char *class_name);

/* Creates a module table with funcs and integer constants, leaves it on the
 * stack. If ctor is not NULL, calling the table (Image(...)) forwards to ctor
 * with the arguments, and `new` is also set to ctor. */
void athena_lua_new_module(lua_State *L, const luaL_Reg *funcs, const athena_lua_const *consts, lua_CFunction ctor);

/* Pops the table on top of the stack into _G[name] and package.loaded[name]. */
void athena_lua_set_global_module(lua_State *L, const char *name);

void athena_lua_set_consts(lua_State *L, int idx, const athena_lua_const *consts);

/* Keeps the value at `val` alive as long as the object at `obj` is alive, by
 * storing it in the object's field table. key == NULL keys by the value's
 * address (for collections: one entry per retained value). */
void athena_lua_retain(lua_State *L, int obj, int val, const char *key);

/* --- value helpers --------------------------------------------------------- */

uint32_t athena_lua_checku32(lua_State *L, int idx);
uint32_t athena_lua_optu32(lua_State *L, int idx, uint32_t def);
void athena_lua_pushu32(lua_State *L, uint32_t v);

/* JS-like truthiness: nil/false/0 are false. Eases porting of 0/1 flags. */
bool athena_lua_tobool(lua_State *L, int idx);

static inline float athena_lua_checkfloat(lua_State *L, int idx) {
    return (float)luaL_checknumber(L, idx);
}

static inline float athena_lua_optfloat(lua_State *L, int idx, float def) {
    return (float)luaL_optnumber(L, idx, def);
}

/* Reads t[key] as a number/bool when present, leaving *out untouched otherwise. */
bool athena_lua_field_u32(lua_State *L, int idx, const char *key, uint32_t *out);
bool athena_lua_field_int(lua_State *L, int idx, const char *key, int *out);
bool athena_lua_field_float(lua_State *L, int idx, const char *key, float *out);
bool athena_lua_field_bool(lua_State *L, int idx, const char *key, bool *out);

void athena_lua_set_int(lua_State *L, const char *key, lua_Integer v);
void athena_lua_set_u32(lua_State *L, const char *key, uint32_t v);
void athena_lua_set_num(lua_State *L, const char *key, lua_Number v);
void athena_lua_set_bool(lua_State *L, const char *key, bool v);
void athena_lua_set_str(lua_State *L, const char *key, const char *v);

/* --- callbacks ------------------------------------------------------------- */

int athena_lua_ref(lua_State *L, int idx);   /* LUA_NOREF if idx is nil */
void athena_lua_unref(lua_State *L, int ref);

/* Calls the function at `ref` with nargs values already pushed. Errors are
 * recorded (see athena_lua_pending_error) instead of propagating, so it is
 * safe to call from C callbacks. Returns false on error. */
bool athena_lua_call_ref(lua_State *L, int ref, int nargs, int nresults);
bool athena_lua_pcall(lua_State *L, int nargs, int nresults);

/* --- runtime --------------------------------------------------------------- */

lua_State *athena_lua_state(void);
/* Runs due timers / ready async tasks; Screen.flip() calls it every frame. */
void athena_lua_pump(lua_State *L);
const char *athena_lua_run_script(const char *path);

void athena_lua_set_render_loop(lua_State *L, int idx);
void athena_lua_set_input_event_handler(void *pad);
void athena_lua_forget_input_event_handler(void *pad);
int athena_lua_new_input_event(lua_State *L, int buttons, int idx, int flavour);
void athena_lua_delete_input_event(lua_State *L, int id);

/* Generic buffer (Lua counterpart of ArrayBuffer), see ath_lua_buffer.c. */
typedef struct {
    uint8_t *data;
    uint32_t size;
    bool owned;     /* free(data) on gc */
} AthenaLuaBuffer;

#define ATHENA_LUA_BUFFER "Buffer"
AthenaLuaBuffer *athena_lua_push_buffer(lua_State *L, void *data, uint32_t size, bool owned);
/* Accepts a Buffer or a string. Returns NULL (and size 0) on other types. */
const uint8_t *athena_lua_to_bytes(lua_State *L, int idx, uint32_t *size);

/* Module openers (ath_lua_registry.c). */
void athena_lua_register_modules(lua_State *L);

#endif /* ATH_LUA_H */
