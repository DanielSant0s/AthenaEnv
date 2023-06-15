#include "ath_env.h"
#include <libkbd.h>

static JSValue athena_kbd_init(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	return JS_NewInt32(ctx, PS2KbdInit());
}

static JSValue athena_kbd_get(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	char key = 0;

	PS2KbdRead(&key);
	return JS_NewInt32(ctx, key);
}

static JSValue athena_kbd_setrepeatrate(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t msec;

	JS_ToUint32(ctx, &msec, argv[0]);
	return JS_NewInt32(ctx, PS2KbdSetRepeatRate(msec));
}

static JSValue athena_kbd_setblockingmode(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t mode;

	JS_ToUint32(ctx, &mode, argv[0]);
	return JS_NewInt32(ctx, PS2KbdSetBlockingMode(mode));
}

static JSValue athena_kbd_deinit(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	return JS_NewInt32(ctx, PS2KbdClose());
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("init", 0, athena_kbd_init),
    JS_CFUNC_DEF("get", 0, athena_kbd_get),
	JS_CFUNC_DEF("setRepeatRate", 1, athena_kbd_setrepeatrate),
	JS_CFUNC_DEF("setBlockingMode", 1, athena_kbd_setblockingmode),
	JS_CFUNC_DEF("deinit", 0, athena_kbd_deinit),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_keyboard_init(JSContext* ctx){
    return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Keyboard");
}