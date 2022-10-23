#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "ath_env.h"

duk_ret_t athena_shape_dtor(duk_context *ctx){
	return 0;
}

duk_ret_t athena_point_ctor(duk_context *ctx){
    int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1 && argc != 2 && argc != 3) return duk_generic_error(ctx, "wrong number of arguments"); 

    Color color = (Color)0x80808080;
    float x = 0.0f;
    float y = 0.0f;

    if (argc == 1){
        color = duk_get_uint(ctx, 0);
    } else if (argc == 2) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
    } else {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        color = duk_get_uint(ctx, 2);
    }

    duk_push_this(ctx);

	duk_push_number(ctx, (float)(x));
    duk_put_prop_string(ctx, -2, "x");

	duk_push_number(ctx, (float)(y));
    duk_put_prop_string(ctx, -2, "y");

	duk_push_uint(ctx, (uint32_t)(color));
    duk_put_prop_string(ctx, -2, "color");

    duk_push_c_function(ctx, athena_shape_dtor, 1);
    duk_set_finalizer(ctx, -2);

	return 0;
}

duk_ret_t athena_point_draw(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1 && argc != 2 && argc != 3) return duk_generic_error(ctx, "wrong number of arguments"); 

    float x = get_obj_float(ctx, -1, "x");
    float y = get_obj_float(ctx, -1, "y");
    Color color = get_obj_uint(ctx, -1, "color");

    if (argc == 1){
        color = duk_get_uint(ctx, 0);
    } else if (argc == 2) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
    } else if (argc == 3) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        color = duk_get_uint(ctx, 2);
    }

	drawPixel(x, y, color);
	return 0;
}

duk_ret_t athena_line_ctor(duk_context *ctx){
    int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1 && argc != 4 && argc != 5) return duk_generic_error(ctx, "wrong number of arguments"); 

    Color color = (Color)0x80808080;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;

    if (argc == 1){
        color = duk_get_uint(ctx, 0);
    } else if (argc == 4) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
    } else {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        color = duk_get_uint(ctx, 4);
    }

    duk_push_this(ctx);

	duk_push_number(ctx, (float)(x1));
    duk_put_prop_string(ctx, -2, "x1");

	duk_push_number(ctx, (float)(y1));
    duk_put_prop_string(ctx, -2, "y1");

	duk_push_number(ctx, (float)(x2));
    duk_put_prop_string(ctx, -2, "x2");

	duk_push_number(ctx, (float)(y2));
    duk_put_prop_string(ctx, -2, "y2");

	duk_push_uint(ctx, (uint32_t)(color));
    duk_put_prop_string(ctx, -2, "color");

    duk_push_c_function(ctx, athena_shape_dtor, 1);
    duk_set_finalizer(ctx, -2);

	return 0;
}

duk_ret_t athena_line_draw(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1 && argc != 4 && argc != 5) return duk_generic_error(ctx, "wrong number of arguments"); 

    float x1 = get_obj_float(ctx, -1, "x1");
    float y1 = get_obj_float(ctx, -1, "y1");
    float x2 = get_obj_float(ctx, -1, "x2");
    float y2 = get_obj_float(ctx, -1, "y2");
    Color color = get_obj_uint(ctx, -1, "color");

    if (argc == 1){
        color = duk_get_uint(ctx, 0);
    } else if (argc == 2) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
    } else if (argc == 3) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        color = duk_get_uint(ctx, 4);
    }

	drawLine(x1, y1, x2, y2, color);
	return 0;
}

/*

duk_ret_t athena_triangle(duk_context *ctx){
	int argc = duk_get_top(ctx);

	if (argc != 7 && argc != 9) return duk_generic_error(ctx, "drawTriangle takes 7 or 9 arguments");

	float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
    float x2 = duk_get_number(ctx, 2);
    float y2 = duk_get_number(ctx, 3);
	float x3 = duk_get_number(ctx, 4);
    float y3 = duk_get_number(ctx, 5);
    Color color =  duk_get_uint(ctx, 6);

	if (argc == 7) drawTriangle(x, y, x2, y2, x3, y3, color);

	if (argc == 9) {
		Color color2 =  duk_get_uint(ctx, 7);
		Color color3 =  duk_get_uint(ctx, 8);
		drawTriangle_gouraud(x, y, x2, y2, x3, y3, color, color2, color3);
	}
	return 0;
}

duk_ret_t athena_quad(duk_context *ctx){
	int argc = duk_get_top(ctx);

	if (argc != 9 && argc != 12) return duk_generic_error(ctx, "drawQuad takes 9 or 12 arguments");

	float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
    float x2 = duk_get_number(ctx, 2);
    float y2 = duk_get_number(ctx, 3);
	float x3 = duk_get_number(ctx, 4);
    float y3 = duk_get_number(ctx, 5);
	float x4 = duk_get_number(ctx, 6);
    float y4 = duk_get_number(ctx, 7);
    Color color = duk_get_uint(ctx, 8);

	if (argc == 9) drawQuad(x, y, x2, y2, x3, y3, x4, y4, color);

	if (argc == 12) {
		Color color2 = duk_get_uint(ctx, 9);
		Color color3 = duk_get_uint(ctx, 10);
		Color color4 = duk_get_uint(ctx, 11);
		drawQuad_gouraud(x, y, x2, y2, x3, y3, x4, y4, color, color2, color3, color4);
	}
	return 0;
}

duk_ret_t athena_rect(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 5) return duk_generic_error(ctx, "drawRect takes 5 arguments");
	float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
    float width = duk_get_number(ctx, 2);
    float height = duk_get_number(ctx, 3);
    Color color =  duk_get_uint(ctx, 4);

	drawRect(x, y, width, height, color);

	return 0;
}

duk_ret_t athena_circle(duk_context *ctx){
	int argc = duk_get_top(ctx);

	if (argc != 4 && argc != 5) return duk_generic_error(ctx, "drawCircle takes 4 or 5 arguments");

	float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
    float radius = duk_get_number(ctx, 2);
    Color color = duk_get_uint(ctx, 3);

	bool filling = (argc == 5);
    int filled = filling? duk_get_number(ctx, 4) : 1;

	drawCircle(x, y, radius, color, filled);

	return 0;
}
*/

void athena_register_shape(duk_context *ctx, duk_c_function ctor, duk_c_function draw, const char* name){
    duk_push_c_function(ctx, ctor, DUK_VARARGS);
    duk_push_object(ctx);
    duk_push_c_function(ctx, draw, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "draw");
    duk_put_prop_string(ctx, -2, "prototype");
    duk_put_global_string(ctx, name);
}

void shape_init(duk_context *ctx) {
    athena_register_shape(ctx, athena_point_ctor, athena_point_draw, "Point");
    athena_register_shape(ctx, athena_line_ctor, athena_line_draw, "Line");
}

void athena_shape_init(duk_context* ctx){
	shape_init(ctx);
}