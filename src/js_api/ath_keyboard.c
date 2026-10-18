#include <ath_env.h>
#include <athena/keyboard.h>
#include <ps2kbd.h>

static JSValue js_keyboard_init(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    return JS_NewInt32(ctx, athena_keyboard_open());
}

static JSValue js_keyboard_get(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    char key = 0;
    athena_keyboard_read(&key);
    return JS_NewInt32(ctx, key);
}

static JSValue js_keyboard_getraw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    uint8_t state = 0, key = 0;
    int ret = athena_keyboard_read_raw(&state, &key);

    if (ret <= 0)
        return JS_NULL;

    JSValue obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "code", JS_NewUint32(ctx, key), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(ctx, obj, "pressed", JS_NewBool(ctx, state == PS2KBD_RAWKEY_DOWN), JS_PROP_C_W_E);
    return obj;
}

static JSValue js_keyboard_setreadmode(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    uint32_t mode;
    JS_ToUint32(ctx, &mode, argv[0]);
    return JS_NewInt32(ctx, athena_keyboard_set_read_mode(mode));
}

static JSValue js_keyboard_setrepeatrate(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    uint32_t msec;
    JS_ToUint32(ctx, &msec, argv[0]);
    return JS_NewInt32(ctx, athena_keyboard_set_repeat_rate(msec));
}

static JSValue js_keyboard_setblockingmode(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    uint32_t mode;
    JS_ToUint32(ctx, &mode, argv[0]);
    return JS_NewInt32(ctx, athena_keyboard_set_blocking_mode(mode));
}

static JSValue js_keyboard_setkeymap(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    size_t keymap_size = 0, shift_size = 0, keycap_size = 0;
    uint8_t *keymap = JS_GetArrayBuffer(ctx, &keymap_size, argv[0]);
    uint8_t *shiftkeymap = JS_GetArrayBuffer(ctx, &shift_size, argv[1]);
    uint8_t *keycap = NULL;

    if (argc > 2 && !JS_IsUndefined(argv[2]))
        keycap = JS_GetArrayBuffer(ctx, &keycap_size, argv[2]);

    if (!keymap || keymap_size < PS2KBD_KEYMAP_SIZE || !shiftkeymap || shift_size < PS2KBD_KEYMAP_SIZE)
        return JS_ThrowInternalError(ctx, "setKeymap: keymap and shiftKeymap must be %d-byte ArrayBuffers.", PS2KBD_KEYMAP_SIZE);

    if (keycap && keycap_size < PS2KBD_KEYMAP_SIZE)
        return JS_ThrowInternalError(ctx, "setKeymap: keycap must be a %d-byte ArrayBuffer.", PS2KBD_KEYMAP_SIZE);

    return JS_NewInt32(ctx, athena_keyboard_set_keymap(keymap, shiftkeymap, keycap));
}

static JSValue js_keyboard_resetkeymap(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    return JS_NewInt32(ctx, athena_keyboard_reset_keymap());
}

static JSValue js_keyboard_deinit(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    return JS_NewInt32(ctx, athena_keyboard_deinit());
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("init", 0, js_keyboard_init),
    JS_CFUNC_DEF("get", 0, js_keyboard_get),
    JS_CFUNC_DEF("getRaw", 0, js_keyboard_getraw),
    JS_CFUNC_DEF("setReadMode", 1, js_keyboard_setreadmode),
    JS_CFUNC_DEF("setRepeatRate", 1, js_keyboard_setrepeatrate),
    JS_CFUNC_DEF("setBlockingMode", 1, js_keyboard_setblockingmode),
    JS_CFUNC_DEF("setKeymap", 2, js_keyboard_setkeymap),
    JS_CFUNC_DEF("resetKeymap", 0, js_keyboard_resetkeymap),
    JS_CFUNC_DEF("deinit", 0, js_keyboard_deinit),
    JS_PROP_INT32_DEF("READMODE_NORMAL", PS2KBD_READMODE_NORMAL, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("READMODE_RAW", PS2KBD_READMODE_RAW, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("KEYMAP_SIZE", PS2KBD_KEYMAP_SIZE, JS_PROP_CONFIGURABLE),
    /* Raw key codes follow the USB HID Keyboard/Keypad Usage Page (0x07);
       modifier keys are reported as ordinary raw key events in this range. */
    JS_PROP_INT32_DEF("KEY_LEFT_CTRL", 0xE0, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("KEY_LEFT_SHIFT", 0xE1, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("KEY_LEFT_ALT", 0xE2, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("KEY_LEFT_GUI", 0xE3, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("KEY_RIGHT_CTRL", 0xE4, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("KEY_RIGHT_SHIFT", 0xE5, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("KEY_RIGHT_ALT", 0xE6, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("KEY_RIGHT_GUI", 0xE7, JS_PROP_CONFIGURABLE),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

#ifdef DYNAMIC_ATHENA_KEYBOARD
char *erl_id = "athena_keyboard";
char *erl_dependancies[] = {
    "libkbd",
    0
};

JSModuleDef *js_init_module(JSContext *ctx)
#else
JSModuleDef *athena_keyboard_init(JSContext *ctx)
#endif
{
    return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Keyboard");
}
