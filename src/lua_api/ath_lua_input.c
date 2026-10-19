#include "ath_lua.h"

#ifdef ATHENA_KEYBOARD
#include <athena/keyboard.h>
#include <ps2kbd.h>
#endif
#ifdef ATHENA_MOUSE
#include <athena/mouse.h>
#include <ps2mouse.h>
#endif
#ifdef ATHENA_REMOTE
#include <athena/remote.h>
#endif
#ifdef ATHENA_CAMERA
#include <athena/camera.h>
#endif

#ifdef ATHENA_KEYBOARD
/* ------------------------------------------------------------- Keyboard -- */

static int l_kbd_init(lua_State *L) { lua_pushinteger(L, athena_keyboard_open()); return 1; }
static int l_kbd_deinit(lua_State *L) { lua_pushinteger(L, athena_keyboard_deinit()); return 1; }
static int l_kbd_reset_keymap(lua_State *L) { lua_pushinteger(L, athena_keyboard_reset_keymap()); return 1; }

static int l_kbd_get(lua_State *L)
{
    char key = 0;
    athena_keyboard_read(&key);
    lua_pushinteger(L, key);
    return 1;
}

static int l_kbd_get_raw(lua_State *L)
{
    uint8_t state = 0, key = 0;
    if (athena_keyboard_read_raw(&state, &key) <= 0)
        return 0;
    lua_createtable(L, 0, 2);
    athena_lua_set_int(L, "code", key);
    athena_lua_set_bool(L, "pressed", state == PS2KBD_RAWKEY_DOWN);
    return 1;
}

static int l_kbd_set_read_mode(lua_State *L)
{
    lua_pushinteger(L, athena_keyboard_set_read_mode(athena_lua_checku32(L, 1)));
    return 1;
}

static int l_kbd_set_repeat_rate(lua_State *L)
{
    lua_pushinteger(L, athena_keyboard_set_repeat_rate(athena_lua_checku32(L, 1)));
    return 1;
}

static int l_kbd_set_blocking_mode(lua_State *L)
{
    lua_pushinteger(L, athena_keyboard_set_blocking_mode(athena_lua_checku32(L, 1)));
    return 1;
}

/* setKeymap(keymap, shiftKeymap [, keycap]) with Buffers or strings. */
static int l_kbd_set_keymap(lua_State *L)
{
    uint32_t km_size, shift_size, cap_size = 0;
    const uint8_t *keymap = athena_lua_to_bytes(L, 1, &km_size);
    const uint8_t *shift = athena_lua_to_bytes(L, 2, &shift_size);
    const uint8_t *keycap = lua_isnoneornil(L, 3) ? NULL : athena_lua_to_bytes(L, 3, &cap_size);

    if (!keymap || km_size < PS2KBD_KEYMAP_SIZE || !shift || shift_size < PS2KBD_KEYMAP_SIZE)
        return luaL_error(L, "setKeymap: keymap and shiftKeymap must be %d bytes", PS2KBD_KEYMAP_SIZE);
    if (!lua_isnoneornil(L, 3) && (!keycap || cap_size < PS2KBD_KEYMAP_SIZE))
        return luaL_error(L, "setKeymap: keycap must be %d bytes", PS2KBD_KEYMAP_SIZE);

    lua_pushinteger(L, athena_keyboard_set_keymap(keymap, shift, keycap));
    return 1;
}

static const luaL_Reg kbd_funcs[] = {
    { "init", l_kbd_init }, { "get", l_kbd_get }, { "getRaw", l_kbd_get_raw },
    { "setReadMode", l_kbd_set_read_mode }, { "setRepeatRate", l_kbd_set_repeat_rate },
    { "setBlockingMode", l_kbd_set_blocking_mode }, { "setKeymap", l_kbd_set_keymap },
    { "resetKeymap", l_kbd_reset_keymap }, { "deinit", l_kbd_deinit },
    { NULL, NULL }
};

static const athena_lua_const kbd_consts[] = {
    { "READMODE_NORMAL", PS2KBD_READMODE_NORMAL }, { "READMODE_RAW", PS2KBD_READMODE_RAW },
    { "KEYMAP_SIZE", PS2KBD_KEYMAP_SIZE },
    { "KEY_LEFT_CTRL", 0xE0 }, { "KEY_LEFT_SHIFT", 0xE1 }, { "KEY_LEFT_ALT", 0xE2 }, { "KEY_LEFT_GUI", 0xE3 },
    { "KEY_RIGHT_CTRL", 0xE4 }, { "KEY_RIGHT_SHIFT", 0xE5 }, { "KEY_RIGHT_ALT", 0xE6 }, { "KEY_RIGHT_GUI", 0xE7 },
    { NULL, 0 }
};
#endif

#ifdef ATHENA_MOUSE
/* ---------------------------------------------------------------- Mouse -- */

static int l_mouse_init(lua_State *L) { lua_pushinteger(L, athena_mouse_open()); return 1; }

static int l_mouse_get(lua_State *L)
{
    AthenaMouseData data;
    athena_mouse_read(&data);
    lua_createtable(L, 0, 4);
    athena_lua_set_int(L, "x", data.x);
    athena_lua_set_int(L, "y", data.y);
    athena_lua_set_int(L, "wheel", data.wheel);
    athena_lua_set_u32(L, "buttons", data.buttons);
    return 1;
}

static int l_mouse_set_boundary(lua_State *L)
{
    lua_pushinteger(L, athena_mouse_set_boundary((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2),
                                                 (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4)));
    return 1;
}

static int l_mouse_get_boundary(lua_State *L)
{
    int minx = 0, maxx = 0, miny = 0, maxy = 0;
    athena_mouse_get_boundary(&minx, &maxx, &miny, &maxy);
    lua_createtable(L, 0, 4);
    athena_lua_set_int(L, "minX", minx);
    athena_lua_set_int(L, "maxX", maxx);
    athena_lua_set_int(L, "minY", miny);
    athena_lua_set_int(L, "maxY", maxy);
    return 1;
}

static int l_mouse_get_mode(lua_State *L) { lua_pushinteger(L, athena_mouse_get_mode()); return 1; }
static int l_mouse_set_mode(lua_State *L) { lua_pushinteger(L, athena_mouse_set_mode(athena_lua_checku32(L, 1))); return 1; }
static int l_mouse_get_accel(lua_State *L) { lua_pushnumber(L, athena_mouse_get_accel()); return 1; }
static int l_mouse_set_accel(lua_State *L) { lua_pushinteger(L, athena_mouse_set_accel(athena_lua_checkfloat(L, 1))); return 1; }
static int l_mouse_set_pos(lua_State *L) { lua_pushinteger(L, athena_mouse_set_position((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2))); return 1; }
static int l_mouse_get_threshold(lua_State *L) { lua_pushinteger(L, athena_mouse_get_threshold()); return 1; }
static int l_mouse_set_threshold(lua_State *L) { lua_pushinteger(L, athena_mouse_set_threshold(athena_lua_checku32(L, 1))); return 1; }
static int l_mouse_get_dbl(lua_State *L) { lua_pushinteger(L, athena_mouse_get_double_click_time()); return 1; }
static int l_mouse_set_dbl(lua_State *L) { lua_pushinteger(L, athena_mouse_set_double_click_time(athena_lua_checku32(L, 1))); return 1; }
static int l_mouse_get_version(lua_State *L) { lua_pushinteger(L, athena_mouse_get_version()); return 1; }
static int l_mouse_enumerate(lua_State *L) { lua_pushinteger(L, athena_mouse_enumerate()); return 1; }
static int l_mouse_reset(lua_State *L) { lua_pushinteger(L, athena_mouse_reset()); return 1; }

static const luaL_Reg mouse_funcs[] = {
    { "init", l_mouse_init }, { "get", l_mouse_get },
    { "setBoundary", l_mouse_set_boundary }, { "getBoundary", l_mouse_get_boundary },
    { "getMode", l_mouse_get_mode }, { "setMode", l_mouse_set_mode },
    { "getAccel", l_mouse_get_accel }, { "setAccel", l_mouse_set_accel },
    { "setPosition", l_mouse_set_pos },
    { "getThreshold", l_mouse_get_threshold }, { "setThreshold", l_mouse_set_threshold },
    { "getDoubleClickTime", l_mouse_get_dbl }, { "setDoubleClickTime", l_mouse_set_dbl },
    { "getVersion", l_mouse_get_version }, { "enumerate", l_mouse_enumerate }, { "reset", l_mouse_reset },
    { NULL, NULL }
};

static const athena_lua_const mouse_consts[] = {
    { "READMODE_DIFF", PS2MOUSE_READMODE_DIFF }, { "READMODE_ABS", PS2MOUSE_READMODE_ABS },
    { "BTN1", PS2MOUSE_BTN1 }, { "BTN2", PS2MOUSE_BTN2 }, { "BTN3", PS2MOUSE_BTN3 },
    { "BTN1_DBL", PS2MOUSE_BTN1DBL }, { "BTN2_DBL", PS2MOUSE_BTN2DBL }, { "BTN3_DBL", PS2MOUSE_BTN3DBL },
    { NULL, 0 }
};
#endif

#ifdef ATHENA_REMOTE
/* --------------------------------------------------------------- Remote -- */

#define REMOTE_CLASS "Remote"

typedef struct {
    AthenaRemote *remote;
} LuaRemote;

static AthenaRemote *check_remote(lua_State *L, int idx)
{
    LuaRemote *r = athena_lua_check_object(L, idx, REMOTE_CLASS);
    if (!r->remote)
        luaL_error(L, "remote was freed");
    return r->remote;
}

static int check_remote_port(lua_State *L, int idx)
{
    int port = (int)luaL_optinteger(L, idx, 1);
    if (port < 0 || port > 1)
        luaL_argerror(L, idx, "wrong port number");
    return port;
}

static int l_remote_init(lua_State *L) { lua_pushinteger(L, athena_remote_open()); return 1; }
static int l_remote_deinit(lua_State *L) { lua_pushinteger(L, athena_remote_close()); return 1; }
static int l_remote_is_active(lua_State *L) { lua_pushboolean(L, athena_remote_is_active(check_remote_port(L, 1))); return 1; }

static int l_remote_get(lua_State *L)
{
    AthenaRemote *remote = athena_remote_get(check_remote_port(L, 1));
    if (!remote)
        return luaL_error(L, "could not open remote");
    LuaRemote *r = athena_lua_new_object(L, REMOTE_CLASS, sizeof(LuaRemote));
    r->remote = remote;
    return 1;
}

static int l_remote_gc(lua_State *L)
{
    LuaRemote *r = athena_lua_check_object(L, 1, REMOTE_CLASS);
    if (r->remote)
        athena_remote_free(r->remote);
    r->remote = NULL;
    return 0;
}

static int l_remote_update(lua_State *L) { athena_remote_update(check_remote(L, 1)); return 0; }
static int l_remote_pressed(lua_State *L) { lua_pushboolean(L, athena_remote_pressed(check_remote(L, 1), athena_lua_checku32(L, 2))); return 1; }
static int l_remote_just_pressed(lua_State *L) { lua_pushboolean(L, athena_remote_just_pressed(check_remote(L, 1), athena_lua_checku32(L, 2))); return 1; }
static int l_remote_released(lua_State *L) { lua_pushboolean(L, athena_remote_released(check_remote(L, 1))); return 1; }
static int get_remote_button(lua_State *L) { athena_lua_pushu32(L, check_remote(L, 1)->button); return 1; }
static int get_remote_old_button(lua_State *L) { athena_lua_pushu32(L, check_remote(L, 1)->old_button); return 1; }
static int get_remote_status(lua_State *L) { athena_lua_pushu32(L, check_remote(L, 1)->status); return 1; }

static const luaL_Reg remote_methods[] = {
    { "update", l_remote_update }, { "pressed", l_remote_pressed },
    { "justPressed", l_remote_just_pressed }, { "released", l_remote_released },
    { NULL, NULL }
};

static const athena_lua_prop remote_props[] = {
    { "button", get_remote_button, NULL },
    { "old_button", get_remote_old_button, NULL },
    { "status", get_remote_status, NULL },
    { NULL, NULL, NULL }
};

static const athena_lua_class remote_class = {
    .name = REMOTE_CLASS, .methods = remote_methods, .props = remote_props, .gc = l_remote_gc,
};

static const luaL_Reg remote_funcs[] = {
    { "init", l_remote_init }, { "deinit", l_remote_deinit },
    { "get", l_remote_get }, { "isActive", l_remote_is_active },
    { NULL, NULL }
};

#define RC(name) { #name, (int32_t)RM_##name }

static const athena_lua_const remote_consts[] = {
    RC(INIT), RC(READY), RC(KEYPRESSED), RC(NOREMOTE), RC(RELEASED), RC(IDLE),
    RC(DVD_ZERO), RC(DVD_ONE), RC(DVD_TWO), RC(DVD_THREE), RC(DVD_FOUR), RC(DVD_FIVE),
    RC(DVD_SIX), RC(DVD_SEVEN), RC(DVD_EIGHT), RC(DVD_NINE), RC(DVD_ENTER), RC(DVD_BROWSE),
    RC(DVD_SET), RC(DVD_RETURN), RC(DVD_CLEAR), RC(DVD_SOURCE), RC(DVD_CHUP), RC(DVD_CHDOWN),
    RC(DVD_REC), RC(DVD_TITLE), RC(DVD_MENU), RC(DVD_PROGRAM), RC(DVD_TIME), RC(DVD_ATOB),
    RC(DVD_REPEAT), RC(DVD_PREV), RC(DVD_NEXT), RC(DVD_PLAY), RC(DVD_STOP), RC(DVD_PAUSE),
    RC(DVD_SCAN_BACK), RC(DVD_SCAN_FORW), RC(DVD_SHUFFLE), RC(DVD_STEP_BACK), RC(DVD_STEP_FORWARD),
    RC(DVD_SLOW_BACK), RC(DVD_SLOW_FORW), RC(DVD_DISPLAY), RC(DVD_SUBTITLE), RC(DVD_SUBTITLE_ON_OFF),
    RC(DVD_AUDIO), RC(DVD_ANGLE), RC(DVD_UP), RC(DVD_DOWN), RC(DVD_LEFT), RC(DVD_RIGHT),
    RC(DVD_OPEN_CLOSE), RC(DVD_POWER), RC(DVD_SEARCH_MODE), RC(DVD_SET_UP),
    RC(PS2_SELECT), RC(PS2_L3), RC(PS2_R3), RC(PS2_START), RC(PS2_UP), RC(PS2_RIGHT),
    RC(PS2_DOWN), RC(PS2_LEFT), RC(PS2_L2), RC(PS2_R2), RC(PS2_L1), RC(PS2_R1),
    RC(PS2_TRIANGLE), RC(PS2_CIRCLE), RC(PS2_CROSS), RC(PS2_SQUARE),
    RC(PS2_POWER), RC(PS2_EJECT), RC(PS2_RESET), RC(PS2_POWERON), RC(PS2_POWEROFF), RC(PS2_NOLIGHT),
    { NULL, 0 }
};
#endif

#ifdef ATHENA_CAMERA
/* --------------------------------------------------------------- Camera -- */

static int l_cam_init(lua_State *L) { athena_camera_startup((int)luaL_optinteger(L, 1, 0)); return 0; }
static int l_cam_count(lua_State *L) { lua_pushinteger(L, athena_camera_device_count()); return 1; }
static int l_cam_open(lua_State *L) { lua_pushinteger(L, athena_camera_open((int)luaL_optinteger(L, 1, 0))); return 1; }
static int l_cam_close(lua_State *L) { lua_pushinteger(L, athena_camera_close((int)luaL_checkinteger(L, 1))); return 1; }
static int l_cam_status(lua_State *L) { lua_pushinteger(L, athena_camera_status((int)luaL_checkinteger(L, 1))); return 1; }
static int l_cam_bandwidth(lua_State *L) { lua_pushinteger(L, athena_camera_set_bandwidth((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2))); return 1; }
static int l_cam_packet(lua_State *L) { lua_pushinteger(L, athena_camera_read_packet((int)luaL_checkinteger(L, 1))); return 1; }
static int l_cam_led(lua_State *L) { lua_pushinteger(L, athena_camera_set_led((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2))); return 1; }

/* setConfig(devid [, width, height [, x, y [, framerate [, hdiv, vdiv]]]]) */
static int l_cam_set_config(lua_State *L)
{
    PS2CAM_DEVICE_CONFIG cfg;
    cfg.ssize = sizeof(cfg);
    cfg.mask = CAM_CONFIG_MASK_DIMENSION | CAM_CONFIG_MASK_DIVIDER | CAM_CONFIG_MASK_OFFSET | CAM_CONFIG_MASK_FRAMERATE;
    cfg.width = (int)luaL_optinteger(L, 2, 640);
    cfg.height = (int)luaL_optinteger(L, 3, 480);
    cfg.x_offset = (int)luaL_optinteger(L, 4, 0);
    cfg.y_offset = (int)luaL_optinteger(L, 5, 0);
    cfg.framerate = (int)luaL_optinteger(L, 6, 30);
    cfg.h_divider = (int)luaL_optinteger(L, 7, 0);
    cfg.v_divider = (int)luaL_optinteger(L, 8, 0);
    athena_camera_set_config((int)luaL_checkinteger(L, 1), &cfg);
    return 0;
}

static const luaL_Reg cam_funcs[] = {
    { "init", l_cam_init }, { "count", l_cam_count }, { "open", l_cam_open }, { "close", l_cam_close },
    { "getStatus", l_cam_status }, { "setBandwidth", l_cam_bandwidth }, { "readPacket", l_cam_packet },
    { "setLED", l_cam_led }, { "setConfig", l_cam_set_config },
    { NULL, NULL }
};
#endif

void athena_lua_open_input(lua_State *L)
{
#ifdef ATHENA_KEYBOARD
    athena_lua_new_module(L, kbd_funcs, kbd_consts, NULL);
    athena_lua_set_global_module(L, "Keyboard");
#endif
#ifdef ATHENA_MOUSE
    athena_lua_new_module(L, mouse_funcs, mouse_consts, NULL);
    athena_lua_set_global_module(L, "Mouse");
#endif
#ifdef ATHENA_REMOTE
    athena_lua_register_class(L, &remote_class);
    athena_lua_new_module(L, remote_funcs, remote_consts, NULL);
    athena_lua_set_global_module(L, "Remote");
#endif
#ifdef ATHENA_CAMERA
    athena_lua_new_module(L, cam_funcs, NULL, NULL);
    /* "Camera" is the 3D camera module (in JS the 3D one shadows this one). */
    athena_lua_set_global_module(L, "EyeToy");
#endif
    (void)L;
}
