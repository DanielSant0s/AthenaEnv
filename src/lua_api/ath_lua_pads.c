#include "ath_lua.h"
#include <athena/pad.h>
#include <pad.h>

#define PAD_CLASS "Pad"

typedef struct {
    AthenaPad *pad;
} LuaPad;

enum { PRESSED_EVENT, JUSTPRESSED_EVENT, NONPRESSED_EVENT };

static AthenaPad *check_pad(lua_State *L, int idx)
{
    LuaPad *p = athena_lua_check_object(L, idx, PAD_CLASS);
    if (!p->pad)
        luaL_error(L, "pad is closed");
    return p->pad;
}

static int check_port(lua_State *L, int idx)
{
    int port = (int)luaL_optinteger(L, idx, 0);
    if (port < 0 || port > 1)
        luaL_argerror(L, idx, "wrong port number");
    return port;
}

static int l_pad_get(lua_State *L)
{
    int port = check_port(L, 1);
    AthenaPad *pad = athena_pad_open(port);
    if (!pad)
        return luaL_error(L, "could not open pad %d", port);

    LuaPad *p = athena_lua_new_object(L, PAD_CLASS, sizeof(LuaPad));
    p->pad = pad;
    return 1;
}

static int l_pad_gc(lua_State *L)
{
    LuaPad *p = athena_lua_check_object(L, 1, PAD_CLASS);
    if (p->pad) {
        athena_lua_forget_input_event_handler(p->pad);
        athena_pad_close(p->pad);
        p->pad = NULL;
    }
    return 0;
}

static int l_pad_gettype(lua_State *L)
{
    lua_pushinteger(L, athena_pad_type(check_port(L, 1)));
    return 1;
}

static int l_pad_getactivetype(lua_State *L)
{
    lua_pushinteger(L, athena_pad_active_type(check_port(L, 1)));
    return 1;
}

/* setMode([port,] mode, lock) */
static int l_pad_setmode(lua_State *L)
{
    int port = 0, base = 1;
    if (lua_gettop(L) >= 3) {
        port = check_port(L, 1);
        base = 2;
    }
    int mode = (int)luaL_checkinteger(L, base);
    bool lock = athena_lua_tobool(L, base + 1);
    lua_pushinteger(L, athena_pad_set_mode(port, mode, lock));
    return 1;
}

static int l_pad_getstate(lua_State *L)
{
    lua_pushinteger(L, athena_pad_state(check_port(L, 1)));
    return 1;
}

/* getPressure([port,] button) */
static int l_pad_getpressure(lua_State *L)
{
    int port = 0, base = 1;
    if (lua_gettop(L) >= 2) {
        port = check_port(L, 1);
        base = 2;
    }
    lua_pushinteger(L, athena_pad_pressure(port, athena_lua_checku32(L, base)));
    return 1;
}

/* rumble([port,] small, big) */
static int l_pad_rumble(lua_State *L)
{
    int port = 0, base = 1;
    if (lua_gettop(L) >= 3) {
        port = check_port(L, 1);
        base = 2;
    }
    athena_pad_rumble(port, (char)luaL_checkinteger(L, base), (char)luaL_checkinteger(L, base + 1));
    return 0;
}

/* setLED([port,] r, g, b) */
static int l_pad_setled(lua_State *L)
{
    int port = 0, base = 1;
    if (lua_gettop(L) >= 4) {
        port = check_port(L, 1);
        base = 2;
    }
    athena_pad_set_led(port, (uint8_t)luaL_checkinteger(L, base), (uint8_t)luaL_checkinteger(L, base + 1),
                       (uint8_t)luaL_checkinteger(L, base + 2));
    return 0;
}

static int l_pad_isactive(lua_State *L)
{
    lua_pushboolean(L, athena_pad_is_active(check_port(L, 1)));
    return 1;
}

static int l_pad_getconnected(lua_State *L)
{
    lua_newtable(L);
    int n = 0;
    for (int port = 0; port < 2; port++) {
        if (athena_pad_is_active(port)) {
            lua_pushinteger(L, port);
            lua_rawseti(L, -2, ++n);
        }
    }
    return 1;
}

static int l_pad_getconnectedcount(lua_State *L)
{
    lua_pushinteger(L, athena_pad_connected_count());
    return 1;
}

static int l_pad_newevent(lua_State *L)
{
    int buttons = (int)athena_lua_checku32(L, 1);
    int flavour = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, athena_lua_new_input_event(L, buttons, 3, flavour));
    return 1;
}

static int l_pad_deleteevent(lua_State *L)
{
    athena_lua_delete_input_event(L, (int)luaL_checkinteger(L, 1));
    return 0;
}

static int l_pad_update(lua_State *L)
{
    athena_pad_update(check_pad(L, 1));
    return 0;
}

static int l_pad_pressed(lua_State *L)
{
    lua_pushboolean(L, athena_pad_pressed(check_pad(L, 1), (int)athena_lua_checku32(L, 2)));
    return 1;
}

static int l_pad_justpressed(lua_State *L)
{
    lua_pushboolean(L, athena_pad_just_pressed(check_pad(L, 1), (int)athena_lua_checku32(L, 2)));
    return 1;
}

static int l_pad_seteventhandler(lua_State *L)
{
    athena_lua_set_input_event_handler(check_pad(L, 1));
    return 0;
}

#define PAD_PROP_U32(field) \
    static int get_##field(lua_State *L) { athena_lua_pushu32(L, check_pad(L, 1)->field); return 1; } \
    static int set_##field(lua_State *L) { check_pad(L, 1)->field = athena_lua_checku32(L, 2); return 0; }
#define PAD_PROP_INT(field) \
    static int get_##field(lua_State *L) { lua_pushinteger(L, check_pad(L, 1)->field); return 1; } \
    static int set_##field(lua_State *L) { check_pad(L, 1)->field = (int)luaL_checknumber(L, 2); return 0; }

PAD_PROP_U32(btns)
PAD_PROP_INT(lx)
PAD_PROP_INT(ly)
PAD_PROP_INT(rx)
PAD_PROP_INT(ry)
PAD_PROP_U32(old_btns)
PAD_PROP_INT(old_lx)
PAD_PROP_INT(old_ly)
PAD_PROP_INT(old_rx)
PAD_PROP_INT(old_ry)

static int get_port(lua_State *L)
{
    lua_pushinteger(L, check_pad(L, 1)->port);
    return 1;
}

#define PROP(name) { #name, get_##name, set_##name }

static const athena_lua_prop pad_props[] = {
    PROP(btns), PROP(lx), PROP(ly), PROP(rx), PROP(ry),
    PROP(old_btns), PROP(old_lx), PROP(old_ly), PROP(old_rx), PROP(old_ry),
    { "port", get_port, NULL },
    { NULL, NULL, NULL }
};

static const luaL_Reg pad_methods[] = {
    { "update", l_pad_update },
    { "pressed", l_pad_pressed },
    { "justPressed", l_pad_justpressed },
    { "setEventHandler", l_pad_seteventhandler },
    { NULL, NULL }
};

static const luaL_Reg pad_funcs[] = {
    { "get", l_pad_get },
    { "getType", l_pad_gettype },
    { "getActiveType", l_pad_getactivetype },
    { "setMode", l_pad_setmode },
    { "getState", l_pad_getstate },
    { "getPressure", l_pad_getpressure },
    { "rumble", l_pad_rumble },
    { "setLED", l_pad_setled },
    { "isActive", l_pad_isactive },
    { "getConnected", l_pad_getconnected },
    { "getConnectedCount", l_pad_getconnectedcount },
    { "newEvent", l_pad_newevent },
    { "deleteEvent", l_pad_deleteevent },
    { NULL, NULL }
};

static const athena_lua_const pad_consts[] = {
    { "PRESSED", PRESSED_EVENT }, { "JUST_PRESSED", JUSTPRESSED_EVENT }, { "NONPRESSED", NONPRESSED_EVENT },
    { "SELECT", PAD_SELECT }, { "START", PAD_START }, { "UP", PAD_UP }, { "RIGHT", PAD_RIGHT },
    { "DOWN", PAD_DOWN }, { "LEFT", PAD_LEFT }, { "TRIANGLE", PAD_TRIANGLE }, { "CIRCLE", PAD_CIRCLE },
    { "CROSS", PAD_CROSS }, { "SQUARE", PAD_SQUARE }, { "L1", PAD_L1 }, { "R1", PAD_R1 },
    { "L2", PAD_L2 }, { "R2", PAD_R2 }, { "L3", PAD_L3 }, { "R3", PAD_R3 },
    { "TYPE_NEJICON", PAD_TYPE_NEJICON }, { "TYPE_KONAMIGUN", PAD_TYPE_KONAMIGUN },
    { "TYPE_DIGITAL", PAD_TYPE_DIGITAL }, { "TYPE_ANALOG", PAD_TYPE_ANALOG },
    { "TYPE_NAMCOGUN", PAD_TYPE_NAMCOGUN }, { "TYPE_DUALSHOCK", PAD_TYPE_DUALSHOCK },
    { "TYPE_JOGCON", PAD_TYPE_JOGCON }, { "TYPE_EX_TSURICON", PAD_TYPE_EX_TSURICON },
    { "TYPE_EX_JOGCON", PAD_TYPE_EX_JOGCON },
    { "STATE_DISCONN", PAD_STATE_DISCONN }, { "STATE_FINDPAD", PAD_STATE_FINDPAD },
    { "STATE_FINDCTP1", PAD_STATE_FINDCTP1 }, { "STATE_EXECCMD", PAD_STATE_EXECCMD },
    { "STATE_STABLE", PAD_STATE_STABLE }, { "STATE_ERROR", PAD_STATE_ERROR },
    { "MMODE_DIGITAL", PAD_MMODE_DIGITAL }, { "MMODE_DUALSHOCK", PAD_MMODE_DUALSHOCK },
    { NULL, 0 }
};

static const athena_lua_class pad_class = {
    .name = PAD_CLASS,
    .methods = pad_methods,
    .props = pad_props,
    .gc = l_pad_gc,
};

void athena_lua_open_pads(lua_State *L)
{
    athena_lua_register_class(L, &pad_class);
    athena_lua_new_module(L, pad_funcs, pad_consts, NULL);
    athena_lua_set_global_module(L, "Pads");
}
