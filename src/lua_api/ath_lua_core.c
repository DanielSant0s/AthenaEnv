#include <string.h>
#include <stdio.h>

#include "ath_lua.h"

/* --- objects --------------------------------------------------------------- */

static int obj_index(lua_State *L)
{
    /* upvalue 1: methods, upvalue 2: getters */
    lua_pushvalue(L, 2);
    if (lua_rawget(L, lua_upvalueindex(1)) != LUA_TNIL)
        return 1;
    lua_pop(L, 1);

    lua_pushvalue(L, 2);
    if (lua_rawget(L, lua_upvalueindex(2)) == LUA_TFUNCTION) {
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        return 1;
    }
    lua_pop(L, 1);

    if (lua_getiuservalue(L, 1, 1) == LUA_TTABLE) {
        lua_pushvalue(L, 2);
        lua_rawget(L, -2);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

static int obj_newindex(lua_State *L)
{
    /* upvalue 1: setters, upvalue 2: getters */
    lua_pushvalue(L, 2);
    if (lua_rawget(L, lua_upvalueindex(1)) == LUA_TFUNCTION) {
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 3);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            /* Setters are anonymous C functions: name the property instead. */
            if (lua_type(L, -1) == LUA_TSTRING)
                luaL_gsub(L, lua_tostring(L, -1), "bad argument #2 to '?'",
                          lua_pushfstring(L, "bad value for property '%s'", lua_tostring(L, 2)));
            return lua_error(L);
        }
        return 0;
    }
    lua_pop(L, 1);

    lua_pushvalue(L, 2);
    if (lua_rawget(L, lua_upvalueindex(2)) != LUA_TNIL)
        return luaL_error(L, "property '%s' is read-only", lua_tostring(L, 2));
    lua_pop(L, 1);

    if (lua_getiuservalue(L, 1, 1) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setiuservalue(L, 1, 1);
    }

    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_rawset(L, -3);
    return 0;
}

static int obj_default_tostring(lua_State *L)
{
    lua_getfield(L, lua_upvalueindex(1), "__name");
    lua_pushfstring(L, "%s: %p", lua_tostring(L, -1), lua_touserdata(L, 1));
    return 1;
}

void athena_lua_register_class(lua_State *L, const athena_lua_class *def)
{
    luaL_newmetatable(L, def->name);
    int mt = lua_gettop(L);

    lua_newtable(L); /* methods */
    if (def->methods)
        luaL_setfuncs(L, def->methods, 0);
    int methods = lua_gettop(L);

    lua_newtable(L); /* getters */
    int getters = lua_gettop(L);
    lua_newtable(L); /* setters */
    int setters = lua_gettop(L);

    if (def->props) {
        for (const athena_lua_prop *p = def->props; p->name; p++) {
            if (p->get) {
                lua_pushcfunction(L, p->get);
                lua_setfield(L, getters, p->name);
            }
            if (p->set) {
                lua_pushcfunction(L, p->set);
                lua_setfield(L, setters, p->name);
            }
        }
    }

    lua_pushvalue(L, methods);
    lua_pushvalue(L, getters);
    lua_pushcclosure(L, obj_index, 2);
    lua_setfield(L, mt, "__index");

    lua_pushvalue(L, setters);
    lua_pushvalue(L, getters);
    lua_pushcclosure(L, obj_newindex, 2);
    lua_setfield(L, mt, "__newindex");

    if (def->gc) {
        lua_pushcfunction(L, def->gc);
        lua_setfield(L, mt, "__gc");
    }

    if (def->tostring) {
        lua_pushcfunction(L, def->tostring);
    } else {
        lua_pushvalue(L, mt);
        lua_pushcclosure(L, obj_default_tostring, 1);
    }
    lua_setfield(L, mt, "__tostring");

    if (def->meta) {
        lua_pushvalue(L, mt);
        luaL_setfuncs(L, def->meta, 0);
        lua_pop(L, 1);
    }

    lua_settop(L, mt - 1);
}

void *athena_lua_new_object(lua_State *L, const char *class_name, size_t size)
{
    void *ud = lua_newuserdatauv(L, size, 1);
    memset(ud, 0, size);
    luaL_setmetatable(L, class_name);
    return ud;
}

void *athena_lua_check_object(lua_State *L, int idx, const char *class_name)
{
    return luaL_checkudata(L, idx, class_name);
}

void *athena_lua_test_object(lua_State *L, int idx, const char *class_name)
{
    return luaL_testudata(L, idx, class_name);
}

/* --- modules --------------------------------------------------------------- */

static int module_call(lua_State *L)
{
    lua_CFunction ctor = lua_tocfunction(L, lua_upvalueindex(1));
    lua_remove(L, 1); /* the module table itself */
    return ctor(L);
}

void athena_lua_set_consts(lua_State *L, int idx, const athena_lua_const *consts)
{
    idx = lua_absindex(L, idx);
    for (const athena_lua_const *c = consts; c && c->name; c++) {
        lua_pushinteger(L, c->value);
        lua_setfield(L, idx, c->name);
    }
}

void athena_lua_new_module(lua_State *L, const luaL_Reg *funcs, const athena_lua_const *consts, lua_CFunction ctor)
{
    lua_newtable(L);
    if (funcs)
        luaL_setfuncs(L, funcs, 0);
    athena_lua_set_consts(L, -1, consts);

    if (ctor) {
        lua_pushcfunction(L, ctor);
        lua_setfield(L, -2, "new");

        lua_newtable(L);
        lua_pushcfunction(L, ctor);
        lua_pushcclosure(L, module_call, 1);
        lua_setfield(L, -2, "__call");
        lua_setmetatable(L, -2);
    }
}

void athena_lua_set_global_module(lua_State *L, const char *name)
{
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_pushvalue(L, -2);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
    lua_setglobal(L, name);
}

/* --- values ---------------------------------------------------------------- */

uint32_t athena_lua_checku32(lua_State *L, int idx)
{
    if (lua_isinteger(L, idx))
        return (uint32_t)lua_tointeger(L, idx);

    /* JS_ToUint32 semantics for flags: true -> 1, false -> 0 */
    if (lua_isboolean(L, idx))
        return lua_toboolean(L, idx);

    lua_Number n = luaL_checknumber(L, idx);
    if (n < 0)
        return (uint32_t)(int32_t)n;
    return (uint32_t)n;
}

uint32_t athena_lua_optu32(lua_State *L, int idx, uint32_t def)
{
    return lua_isnoneornil(L, idx) ? def : athena_lua_checku32(L, idx);
}

void athena_lua_pushu32(lua_State *L, uint32_t v)
{
    lua_pushinteger(L, (lua_Integer)(int32_t)v);
}

bool athena_lua_tobool(lua_State *L, int idx)
{
    if (lua_type(L, idx) == LUA_TNUMBER)
        return lua_tonumber(L, idx) != 0;
    return lua_toboolean(L, idx);
}

bool athena_lua_field_u32(lua_State *L, int idx, const char *key, uint32_t *out)
{
    bool found = false;
    if (lua_getfield(L, idx, key) != LUA_TNIL) {
        *out = athena_lua_checku32(L, -1);
        found = true;
    }
    lua_pop(L, 1);
    return found;
}

bool athena_lua_field_int(lua_State *L, int idx, const char *key, int *out)
{
    bool found = false;
    if (lua_getfield(L, idx, key) != LUA_TNIL) {
        *out = (int)luaL_checknumber(L, -1);
        found = true;
    }
    lua_pop(L, 1);
    return found;
}

bool athena_lua_field_float(lua_State *L, int idx, const char *key, float *out)
{
    bool found = false;
    if (lua_getfield(L, idx, key) != LUA_TNIL) {
        *out = (float)luaL_checknumber(L, -1);
        found = true;
    }
    lua_pop(L, 1);
    return found;
}

bool athena_lua_field_bool(lua_State *L, int idx, const char *key, bool *out)
{
    bool found = false;
    if (lua_getfield(L, idx, key) != LUA_TNIL) {
        *out = athena_lua_tobool(L, -1);
        found = true;
    }
    lua_pop(L, 1);
    return found;
}

void athena_lua_set_int(lua_State *L, const char *key, lua_Integer v)
{
    lua_pushinteger(L, v);
    lua_setfield(L, -2, key);
}

void athena_lua_set_u32(lua_State *L, const char *key, uint32_t v)
{
    athena_lua_pushu32(L, v);
    lua_setfield(L, -2, key);
}

void athena_lua_set_num(lua_State *L, const char *key, lua_Number v)
{
    lua_pushnumber(L, v);
    lua_setfield(L, -2, key);
}

void athena_lua_set_bool(lua_State *L, const char *key, bool v)
{
    lua_pushboolean(L, v);
    lua_setfield(L, -2, key);
}

void athena_lua_set_str(lua_State *L, const char *key, const char *v)
{
    lua_pushstring(L, v);
    lua_setfield(L, -2, key);
}

/* --- callbacks ------------------------------------------------------------- */

int athena_lua_ref(lua_State *L, int idx)
{
    if (lua_isnoneornil(L, idx))
        return LUA_NOREF;
    lua_pushvalue(L, idx);
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

void athena_lua_unref(lua_State *L, int ref)
{
    if (ref != LUA_NOREF && ref != LUA_REFNIL)
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
}

extern bool athena_lua_is_reload_error(lua_State *L, int idx);

static int traceback_handler(lua_State *L)
{
    if (athena_lua_is_reload_error(L, 1))
        return 1;

    const char *msg = lua_tostring(L, 1);
    if (!msg) {
        if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING)
            return 1;
        msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1);
    return 1;
}

extern void athena_lua_record_error(const char *msg);

bool athena_lua_pcall(lua_State *L, int nargs, int nresults)
{
    int base = lua_gettop(L) - nargs;
    lua_pushcfunction(L, traceback_handler);
    lua_insert(L, base);

    int status = lua_pcall(L, nargs, nresults, base);
    lua_remove(L, base);

    if (status != LUA_OK) {
        if (!athena_lua_is_reload_error(L, -1))
            athena_lua_record_error(lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool athena_lua_call_ref(lua_State *L, int ref, int nargs, int nresults)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_insert(L, -(nargs + 1));
    return athena_lua_pcall(L, nargs, nresults);
}

/* --- lifetime ------------------------------------------------------------ */

void athena_lua_retain(lua_State *L, int obj, int val, const char *key)
{
    obj = lua_absindex(L, obj);
    val = lua_absindex(L, val);

    if (lua_getiuservalue(L, obj, 1) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setiuservalue(L, obj, 1);
    }

    lua_pushvalue(L, val);
    if (key)
        lua_setfield(L, -2, key);
    else
        lua_rawsetp(L, -2, lua_topointer(L, val));
    lua_pop(L, 1);
}
