#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "ath_env.h"

static JSValue athena_point_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    float x, y;
    Color color;

    JS_ToFloat32(ctx, &x, argv[0]);
    JS_ToFloat32(ctx, &y, argv[1]);
	JS_ToUint32(ctx, &color, argv[2]);

	drawPixel(x, y, color);
	return JS_UNDEFINED;
}

static JSValue athena_line_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    float x1, y1, x2, y2;
    Color color;

    JS_ToFloat32(ctx, &x1, argv[0]);
    JS_ToFloat32(ctx, &y1, argv[1]);
    JS_ToFloat32(ctx, &x2, argv[2]);
    JS_ToFloat32(ctx, &y2, argv[3]);
	JS_ToUint32(ctx, &color, argv[4]);

	drawLine(x1, y1, x2, y2, color);
	return JS_UNDEFINED;
}

static JSValue athena_triangle_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    float x1, y1, x2, y2, x3, y3;
    Color color1, color2, color3;

    JS_ToFloat32(ctx, &x1, argv[0]);
    JS_ToFloat32(ctx, &y1, argv[1]);
    JS_ToFloat32(ctx, &x2, argv[2]);
    JS_ToFloat32(ctx, &y2, argv[3]);
    JS_ToFloat32(ctx, &x3, argv[4]);
    JS_ToFloat32(ctx, &y3, argv[5]);
	JS_ToUint32(ctx, &color1, argv[6]);

    if(argc == 7){
        drawTriangle(x1, y1, x2, y2, x3, y3, color1);
    } else {
        JS_ToUint32(ctx, &color2, argv[7]);
        JS_ToUint32(ctx, &color3, argv[8]);

        drawTriangle_gouraud(x1, y1, x2, y2, x3, y3, color1, color2, color3);
    }
	
	return JS_UNDEFINED;
}

static JSValue athena_quad_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    float x1, y1, x2, y2, x3, y3, x4, y4;
    Color color1, color2, color3, color4;

    JS_ToFloat32(ctx, &x1, argv[0]);
    JS_ToFloat32(ctx, &y1, argv[1]);
    JS_ToFloat32(ctx, &x2, argv[2]);
    JS_ToFloat32(ctx, &y2, argv[3]);
    JS_ToFloat32(ctx, &x3, argv[4]);
    JS_ToFloat32(ctx, &y3, argv[5]);
    JS_ToFloat32(ctx, &x4, argv[6]);
    JS_ToFloat32(ctx, &y4, argv[7]);
	JS_ToUint32(ctx, &color1, argv[8]);

    if(argc == 9){
        drawQuad(x1, y1, x2, y2, x3, y3, x4, y4, color1);
    } else {
        JS_ToUint32(ctx, &color2, argv[9]);
	    JS_ToUint32(ctx, &color3, argv[10]);
	    JS_ToUint32(ctx, &color4, argv[11]);

        drawQuad_gouraud(x1, y1, x2, y2, x3, y3, x4, y4, color1, color2, color3, color4);
    }
	
	return JS_UNDEFINED;
}

static JSValue athena_rect_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    float x, y, w, h;
    Color color;

    JS_ToFloat32(ctx, &x, argv[0]);
    JS_ToFloat32(ctx, &y, argv[1]);
    JS_ToFloat32(ctx, &w, argv[2]);
    JS_ToFloat32(ctx, &h, argv[3]);
	JS_ToUint32(ctx, &color, argv[4]);

	drawRect(x, y, w, h, color);
	return JS_UNDEFINED;
}

static JSValue athena_circle_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    bool filled = true;
    float x, y, r;
    Color color;

    JS_ToFloat32(ctx, &x, argv[0]);
    JS_ToFloat32(ctx, &y, argv[1]);
    JS_ToFloat32(ctx, &r, argv[2]);
    JS_ToUint32(ctx, &color, argv[3]);
    if (argc == 5) filled = JS_ToBool(ctx, argv[4]);

	drawCircle(x, y, r, color, filled);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("point", 3, athena_point_draw),
	JS_CFUNC_DEF("line", 5, athena_line_draw),
	JS_CFUNC_DEF("triangle", 9, athena_triangle_draw),
	JS_CFUNC_DEF("quad", 12, athena_quad_draw),
	JS_CFUNC_DEF("rect", 5, athena_rect_draw),
    JS_CFUNC_DEF("circle", 5, athena_circle_draw)
};

static int module_init(JSContext *ctx, JSModuleDef *m){
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_shape_init(JSContext* ctx){
	return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Draw");

}
