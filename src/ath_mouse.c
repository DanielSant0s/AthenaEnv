#include "ath_env.h"
#include <libmouse.h>

static JSValue athena_mouse_init_f(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    int ret = PS2MouseInit();
    PS2MouseReset();
	return JS_NewInt32(ctx, ret);
}

static JSValue athena_mouse_get(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    JSValue obj;
	PS2MouseData data;

	PS2MouseRead(&data);

    obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewInt32(ctx, data.x), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewInt32(ctx, data.y), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(ctx, obj, "wheel", JS_NewInt32(ctx, data.wheel), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(ctx, obj, "buttons", JS_NewUint32(ctx, data.buttons), JS_PROP_C_W_E);

	return obj;
}

static JSValue athena_mouse_setboundary(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int minx, maxx, miny, maxy;

    JS_ToInt32(ctx, &minx, argv[0]);
    JS_ToInt32(ctx, &maxx, argv[1]);
    JS_ToInt32(ctx, &miny, argv[2]);
    JS_ToInt32(ctx, &maxy, argv[3]);

	return JS_NewInt32(ctx, PS2MouseSetBoundary(minx, maxx, miny, maxy));
}

static JSValue athena_mouse_getmode(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	return JS_NewUint32(ctx, PS2MouseGetReadMode());
}

static JSValue athena_mouse_setmode(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    uint32_t mode;
    JS_ToUint32(ctx, &mode, argv[0]);
	return JS_NewUint32(ctx, PS2MouseSetReadMode(mode));
}

static JSValue athena_mouse_getaccel(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	return JS_NewFloat32(ctx, PS2MouseGetAccel());
}

static JSValue athena_mouse_setaccel(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    float accel;
    JS_ToFloat32(ctx, &accel, argv[0]);
	return JS_NewInt32(ctx, PS2MouseSetAccel(accel));
}

static JSValue athena_mouse_setpos(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    int x, y;
    JS_ToInt32(ctx, &x, argv[0]);
    JS_ToInt32(ctx, &y, argv[1]);
	return JS_NewInt32(ctx, PS2MouseSetPosition(x, y));
}



static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("init", 0, athena_mouse_init_f),
    JS_CFUNC_DEF("get", 0, athena_mouse_get),
    JS_CFUNC_DEF("setBoundary", 4, athena_mouse_setboundary),
    JS_CFUNC_DEF("getMode", 0, athena_mouse_getmode),
    JS_CFUNC_DEF("setMode", 1, athena_mouse_setmode),
    JS_CFUNC_DEF("getAccel", 0, athena_mouse_getaccel),
    JS_CFUNC_DEF("setAccel", 1, athena_mouse_setaccel),
    JS_CFUNC_DEF("setPosition", 1, athena_mouse_setpos),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_mouse_init(JSContext* ctx){
    return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Mouse");
}

/*
int PS2MouseSetThres(u32 thres);
u32 PS2MouseGetThres();
int PS2MouseGetBoundary(int *minx, int *maxx, int *miny, int *maxy);
int PS2MouseSetPosition(int x, int y); 
int PS2MouseReset();
u32 PS2MouseEnum();
int PS2MouseSetDblClickTime(u32 msec);
u32 PS2MouseGetDblClickTIme();
u32 PS2MouseGetVersion();
*/