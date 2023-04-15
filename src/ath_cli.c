#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <debug.h>

#include "ath_env.h"

// void scr_putchar(int x, int y, u32 color, int ch);
// int scr_getX(void);
// int scr_getY(void);

static JSValue athena_print(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	scr_printf(JS_ToCString(ctx, argv[0]));
	return JS_UNDEFINED;
}

static JSValue athena_setxy(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int x, y;
	JS_ToInt32(ctx, &x, argv[0]);
	JS_ToInt32(ctx, &y, argv[1]);
	scr_setXY(x, y);
	return JS_UNDEFINED;
}

static JSValue athena_clear(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	scr_clear();
	return JS_UNDEFINED;
}

static JSValue athena_setbgcolor(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t color;
	JS_ToUint32(ctx, &color, argv[0]);
	scr_setbgcolor(color);
	return JS_UNDEFINED;
}

static JSValue athena_setfontcolor(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t color;
	JS_ToUint32(ctx, &color, argv[0]);
	scr_setfontcolor(color);
	return JS_UNDEFINED;
}

static JSValue athena_setcursorcolor(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t color;
	JS_ToUint32(ctx, &color, argv[0]);
	scr_setcursorcolor(color);
	return JS_UNDEFINED;
}

static JSValue athena_setcursor(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	scr_setCursor(JS_ToBool(ctx, argv[0]));
	return JS_UNDEFINED;
}

static JSValue athena_getx(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	return JS_NewInt32(ctx, scr_getX());
}

static JSValue athena_gety(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	return JS_NewInt32(ctx, scr_getY());
}

/*static JSValue athena_getcursor(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	return JS_NewBool(ctx, scr_getCursor());
}*/

static const JSCFunctionListEntry console_funcs[] = {
	JS_CFUNC_DEF("print",  1, athena_print),
	JS_CFUNC_DEF("setCoords", 2, athena_setxy),
	JS_CFUNC_DEF("clear", 0, athena_clear),
	JS_CFUNC_DEF("setBGColor", 1, athena_setbgcolor),
	JS_CFUNC_DEF("setFontColor", 1, athena_setfontcolor),
	JS_CFUNC_DEF("setCursorColor", 1, athena_setcursorcolor),
	JS_CFUNC_DEF("setCursor", 1, athena_setcursor),
	JS_CFUNC_DEF("getX", 0, athena_getx),
	JS_CFUNC_DEF("getY", 0, athena_gety),
	//JS_CFUNC_DEF("getCursor", 0, athena_getcursor),
};

static int console_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, console_funcs, countof(console_funcs));
}

JSModuleDef *athena_console_init(JSContext* ctx){
    return athena_push_module(ctx, console_init, console_funcs, countof(console_funcs), "Console");
}