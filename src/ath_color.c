#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "ath_env.h"

static JSValue athena_color_ctor(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t r, g, b, a = 0x80;
	JS_ToUint32(ctx, &r, argv[0]);
	JS_ToUint32(ctx, &g, argv[1]);
	JS_ToUint32(ctx, &b, argv[2]);
	if (argc == 4) JS_ToUint32(ctx, &a, argv[3]);

	return JS_NewUint32(ctx, r | (g << 8) | (b << 16) | (a << 24));
}

static JSValue athena_getR(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t color;
	JS_ToUint32(ctx, &color, argv[0]);
	return JS_NewUint32(ctx, R(color));
}

static JSValue athena_getG(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t color;
	JS_ToUint32(ctx, &color, argv[0]);
	return JS_NewUint32(ctx, G(color));
}

static JSValue athena_getB(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t color;
	JS_ToUint32(ctx, &color, argv[0]);
	return JS_NewUint32(ctx, B(color));
}

static JSValue athena_getA(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t color;
	JS_ToUint32(ctx, &color, argv[0]);
	return JS_NewUint32(ctx, A(color));
}

static JSValue athena_setR(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t color, r;
	JS_ToUint32(ctx, &color, argv[0]);
	JS_ToUint32(ctx, &r, argv[1]);
	return JS_NewUint32(ctx, (r | (G(color) << 8) | (B(color) << 16) | (A(color) << 24)));
}

static JSValue athena_setG(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t color, g;
	JS_ToUint32(ctx, &color, argv[0]);
	JS_ToUint32(ctx, &g, argv[1]);
	return JS_NewUint32(ctx, (R(color) | (g << 8) | (B(color) << 16) | (A(color) << 24)));
}

static JSValue athena_setB(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t color, b;
	JS_ToUint32(ctx, &color, argv[0]);
	JS_ToUint32(ctx, &b, argv[1]);
	return JS_NewUint32(ctx, (R(color) | (G(color) << 8) | (b << 16) | (A(color) << 24)));
}

static JSValue athena_setA(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t color, a;
	JS_ToUint32(ctx, &color, argv[0]);
	JS_ToUint32(ctx, &a, argv[1]);
	return JS_NewUint32(ctx, (R(color) | (G(color) << 8) | (B(color) << 16) | (a << 24)));
}

static const JSCFunctionListEntry color_funcs[] = {
	JS_CFUNC_DEF("new",  4, athena_color_ctor),
	JS_CFUNC_DEF("getR", 1, athena_getR),
	JS_CFUNC_DEF("getG", 1, athena_getG),
	JS_CFUNC_DEF("getB", 1, athena_getB),
	JS_CFUNC_DEF("getA", 1, athena_getA),
	JS_CFUNC_DEF("setR", 2, athena_setR),
	JS_CFUNC_DEF("setG", 2, athena_setG),
	JS_CFUNC_DEF("setB", 2, athena_setB),
	JS_CFUNC_DEF("setA", 2, athena_setA),
};

static int color_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, color_funcs, countof(color_funcs));
}

JSModuleDef *athena_color_init(JSContext* ctx){
    return athena_push_module(ctx, color_init, color_funcs, countof(color_funcs), "Color");
}