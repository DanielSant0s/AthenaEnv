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
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 2) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
    } else {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        duk_get_prop_string(ctx, 2, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
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
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 2) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
    } else if (argc == 3) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        duk_get_prop_string(ctx, 2, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
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
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
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
        duk_get_prop_string(ctx, 4, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
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
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 4) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
    } else if (argc == 5) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        duk_get_prop_string(ctx, 4, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    }

	drawLine(x1, y1, x2, y2, color);
	return 0;
}



duk_ret_t athena_triangle_ctor(duk_context *ctx){
    int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1 && argc != 3 && argc != 6 && argc != 7 && argc != 9) return duk_generic_error(ctx, "wrong number of arguments"); 

    Color color1 = (Color)0x80808080;
    Color color2 = (Color)0;
    Color color3 = (Color)0;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    float x3 = 0.0f;
    float y3 = 0.0f;

    if (argc == 1){
        color1 = duk_get_uint(ctx, 0);
    } else if (argc == 3) {
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 1, "\xff""\xff""rgba");
		color2 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 2, "\xff""\xff""rgba");
		color3 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 6) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        x3 = duk_get_number(ctx, 4);
        y3 = duk_get_number(ctx, 5);
    } else if (argc == 7) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        x3 = duk_get_number(ctx, 4);
        y3 = duk_get_number(ctx, 5);
        duk_get_prop_string(ctx, 6, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 9) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        x3 = duk_get_number(ctx, 4);
        y3 = duk_get_number(ctx, 5);
        duk_get_prop_string(ctx, 6, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 7, "\xff""\xff""rgba");
		color2 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 8, "\xff""\xff""rgba");
		color3 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
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

	duk_push_number(ctx, (float)(x3));
    duk_put_prop_string(ctx, -2, "x3");

	duk_push_number(ctx, (float)(y3));
    duk_put_prop_string(ctx, -2, "y3");

	duk_push_uint(ctx, (uint32_t)(color1));
    duk_put_prop_string(ctx, -2, "color1");

	duk_push_uint(ctx, (uint32_t)(color2));
    duk_put_prop_string(ctx, -2, "color2");

	duk_push_uint(ctx, (uint32_t)(color3));
    duk_put_prop_string(ctx, -2, "color3");

    duk_push_c_function(ctx, athena_shape_dtor, 1);
    duk_set_finalizer(ctx, -2);

	return 0;
}

duk_ret_t athena_triangle_draw(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1 && argc != 3 && argc != 6 && argc != 7 && argc != 9) return duk_generic_error(ctx, "wrong number of arguments"); 

    float x1 = get_obj_float(ctx, -1, "x1");
    float y1 = get_obj_float(ctx, -1, "y1");
    float x2 = get_obj_float(ctx, -1, "x2");
    float y2 = get_obj_float(ctx, -1, "y2");
    float x3 = get_obj_float(ctx, -1, "x3");
    float y3 = get_obj_float(ctx, -1, "y3");
    Color color1 = get_obj_uint(ctx, -1, "color1");
    Color color2 = get_obj_uint(ctx, -1, "color2");
    Color color3 = get_obj_uint(ctx, -1, "color3");

    if (argc == 1){
        color1 = duk_get_uint(ctx, 0);
    } else if (argc == 3) {
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 1, "\xff""\xff""rgba");
		color2 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 2, "\xff""\xff""rgba");
		color3 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 6) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        x3 = duk_get_number(ctx, 4);
        y3 = duk_get_number(ctx, 5);
    } else if (argc == 7) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        x3 = duk_get_number(ctx, 4);
        y3 = duk_get_number(ctx, 5);
        duk_get_prop_string(ctx, 6, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 9) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        x3 = duk_get_number(ctx, 4);
        y3 = duk_get_number(ctx, 5);
        duk_get_prop_string(ctx, 6, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 7, "\xff""\xff""rgba");
		color2 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 8, "\xff""\xff""rgba");
		color3 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    }

    if(color2 == 0 || color3 == 0){
        drawTriangle(x1, y1, x2, y2, x3, y3, color1);
    } else {
        drawTriangle_gouraud(x1, y1, x2, y2, x3, y3, color1, color2, color3);
    }
	
	return 0;
}

duk_ret_t athena_quad_ctor(duk_context *ctx){
    int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1 && argc != 4 && argc != 8 && argc != 9 && argc != 12) return duk_generic_error(ctx, "wrong number of arguments"); 

    Color color1 = (Color)0x80808080;
    Color color2 = (Color)0;
    Color color3 = (Color)0;
    Color color4 = (Color)0;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    float x3 = 0.0f;
    float y3 = 0.0f;
    float x4 = 0.0f;
    float y4 = 0.0f;

    if (argc == 1){
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 4) {
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 1, "\xff""\xff""rgba");
		color2 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 2, "\xff""\xff""rgba");
		color3 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 3, "\xff""\xff""rgba");
		color4 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 8) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        x3 = duk_get_number(ctx, 4);
        y3 = duk_get_number(ctx, 5);
        x4 = duk_get_number(ctx, 6);
        y4 = duk_get_number(ctx, 7);
    } else if (argc == 9) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        x3 = duk_get_number(ctx, 4);
        y3 = duk_get_number(ctx, 5);
        x4 = duk_get_number(ctx, 6);
        y4 = duk_get_number(ctx, 7);
        duk_get_prop_string(ctx, 8, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 12) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        x3 = duk_get_number(ctx, 4);
        y3 = duk_get_number(ctx, 5);
        x4 = duk_get_number(ctx, 6);
        y4 = duk_get_number(ctx, 7);
        duk_get_prop_string(ctx, 8, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 9, "\xff""\xff""rgba");
		color2 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 10, "\xff""\xff""rgba");
		color3 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 11, "\xff""\xff""rgba");
		color4 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
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

	duk_push_number(ctx, (float)(x3));
    duk_put_prop_string(ctx, -2, "x3");

	duk_push_number(ctx, (float)(y3));
    duk_put_prop_string(ctx, -2, "y3");

	duk_push_number(ctx, (float)(x4));
    duk_put_prop_string(ctx, -2, "x4");

	duk_push_number(ctx, (float)(y4));
    duk_put_prop_string(ctx, -2, "y4");

	duk_push_uint(ctx, (uint32_t)(color1));
    duk_put_prop_string(ctx, -2, "color1");

	duk_push_uint(ctx, (uint32_t)(color2));
    duk_put_prop_string(ctx, -2, "color2");

	duk_push_uint(ctx, (uint32_t)(color3));
    duk_put_prop_string(ctx, -2, "color3");

	duk_push_uint(ctx, (uint32_t)(color4));
    duk_put_prop_string(ctx, -2, "color4");

    duk_push_c_function(ctx, athena_shape_dtor, 1);
    duk_set_finalizer(ctx, -2);

	return 0;
}

duk_ret_t athena_quad_draw(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1 && argc != 4 && argc != 8 && argc != 9 && argc != 12) return duk_generic_error(ctx, "wrong number of arguments"); 
    float x1 = get_obj_float(ctx, -1, "x1");
    float y1 = get_obj_float(ctx, -1, "y1");
    float x2 = get_obj_float(ctx, -1, "x2");
    float y2 = get_obj_float(ctx, -1, "y2");
    float x3 = get_obj_float(ctx, -1, "x3");
    float y3 = get_obj_float(ctx, -1, "y3");
    float x4 = get_obj_float(ctx, -1, "x4");
    float y4 = get_obj_float(ctx, -1, "y4");
    Color color1 = get_obj_uint(ctx, -1, "color1");
    Color color2 = get_obj_uint(ctx, -1, "color2");
    Color color3 = get_obj_uint(ctx, -1, "color3");
    Color color4 = get_obj_uint(ctx, -1, "color4");

    if (argc == 1){
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 4) {
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 1, "\xff""\xff""rgba");
		color2 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 2, "\xff""\xff""rgba");
		color3 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 3, "\xff""\xff""rgba");
		color4 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 8) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        x3 = duk_get_number(ctx, 4);
        y3 = duk_get_number(ctx, 5);
        x4 = duk_get_number(ctx, 6);
        y4 = duk_get_number(ctx, 7);
    } else if (argc == 9) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        x3 = duk_get_number(ctx, 4);
        y3 = duk_get_number(ctx, 5);
        x4 = duk_get_number(ctx, 6);
        y4 = duk_get_number(ctx, 7);
        duk_get_prop_string(ctx, 8, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 12) {
        x1 = duk_get_number(ctx, 0);
        y1 = duk_get_number(ctx, 1);
        x2 = duk_get_number(ctx, 2);
        y2 = duk_get_number(ctx, 3);
        x3 = duk_get_number(ctx, 4);
        y3 = duk_get_number(ctx, 5);
        x4 = duk_get_number(ctx, 6);
        y4 = duk_get_number(ctx, 7);
        duk_get_prop_string(ctx, 8, "\xff""\xff""rgba");
		color1 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 9, "\xff""\xff""rgba");
		color2 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 10, "\xff""\xff""rgba");
		color3 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        duk_get_prop_string(ctx, 11, "\xff""\xff""rgba");
		color4 = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    }

    if(color2 == 0 || color3 == 0 || color4 == 0){
        drawQuad(x1, y1, x2, y2, x3, y3, x4, y4, color1);
    } else {
        drawQuad_gouraud(x1, y1, x2, y2, x3, y3, x4, y4, color1, color2, color3, color4);
    }
	
	return 0;
}

duk_ret_t athena_rect_ctor(duk_context *ctx){
    int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1 && argc != 4 && argc != 5) return duk_generic_error(ctx, "wrong number of arguments"); 

    Color color = (Color)0x80808080;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    if (argc == 1){
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 4) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        w = duk_get_number(ctx, 2);
        h = duk_get_number(ctx, 3);
    } else {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        w = duk_get_number(ctx, 2);
        h = duk_get_number(ctx, 3);
        duk_get_prop_string(ctx, 4, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    }

    duk_push_this(ctx);

	duk_push_number(ctx, (float)(x));
    duk_put_prop_string(ctx, -2, "x");

	duk_push_number(ctx, (float)(y));
    duk_put_prop_string(ctx, -2, "y");

	duk_push_number(ctx, (float)(w));
    duk_put_prop_string(ctx, -2, "w");

	duk_push_number(ctx, (float)(h));
    duk_put_prop_string(ctx, -2, "h");

	duk_push_uint(ctx, (uint32_t)(color));
    duk_put_prop_string(ctx, -2, "color");

    duk_push_c_function(ctx, athena_shape_dtor, 1);
    duk_set_finalizer(ctx, -2);

	return 0;
}


duk_ret_t athena_rect_draw(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1 && argc != 4 && argc != 5) return duk_generic_error(ctx, "wrong number of arguments"); 

    float x = get_obj_float(ctx, -1, "x");
    float y = get_obj_float(ctx, -1, "y");
    float w = get_obj_float(ctx, -1, "w");
    float h = get_obj_float(ctx, -1, "h");
    Color color = get_obj_uint(ctx, -1, "color");

    if (argc == 1){
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 4) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        w = duk_get_number(ctx, 2);
        h = duk_get_number(ctx, 3);
    } else if (argc == 5) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        w = duk_get_number(ctx, 2);
        h = duk_get_number(ctx, 3);
        duk_get_prop_string(ctx, 4, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    }

	drawRect(x, y, w, h, color);
	return 0;
}

duk_ret_t athena_circle_ctor(duk_context *ctx){
    int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1 && argc != 3 && argc != 4 && argc != 5) return duk_generic_error(ctx, "wrong number of arguments"); 

    Color color = (Color)0x80808080;
    float x = 0.0f;
    float y = 0.0f;
    float r = 0.0f;
    bool filled = true;

    if (argc == 1){
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 3) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        r = duk_get_number(ctx, 2);
    } else if (argc == 4) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        r = duk_get_number(ctx, 2);
        duk_get_prop_string(ctx, 3, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 5) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        r = duk_get_number(ctx, 2);
        duk_get_prop_string(ctx, 3, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        filled = duk_to_boolean(ctx, 4);
    }

    duk_push_this(ctx);

	duk_push_number(ctx, (float)(x));
    duk_put_prop_string(ctx, -2, "x");

	duk_push_number(ctx, (float)(y));
    duk_put_prop_string(ctx, -2, "y");

	duk_push_number(ctx, (float)(r));
    duk_put_prop_string(ctx, -2, "r");

	duk_push_uint(ctx, (uint32_t)(color));
    duk_put_prop_string(ctx, -2, "color");

	duk_push_boolean(ctx, filled);
    duk_put_prop_string(ctx, -2, "filled");

    duk_push_c_function(ctx, athena_shape_dtor, 1);
    duk_set_finalizer(ctx, -2);

	return 0;
}

duk_ret_t athena_circle_draw(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1 && argc != 3 && argc != 4 && argc != 5) return duk_generic_error(ctx, "wrong number of arguments"); 

    float x = get_obj_float(ctx, -1, "x");
    float y = get_obj_float(ctx, -1, "y");
    float r = get_obj_float(ctx, -1, "r");
    bool filled = get_obj_float(ctx, -1, "filled");
    Color color = get_obj_uint(ctx, -1, "color");

    if (argc == 1){
        duk_get_prop_string(ctx, 0, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 3) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        r = duk_get_number(ctx, 2);
    } else if (argc == 4) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        r = duk_get_number(ctx, 2);
        duk_get_prop_string(ctx, 3, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
    } else if (argc == 5) {
        x = duk_get_number(ctx, 0);
        y = duk_get_number(ctx, 1);
        r = duk_get_number(ctx, 2);
        duk_get_prop_string(ctx, 3, "\xff""\xff""rgba");
		color = duk_get_uint(ctx, -1);
		duk_pop(ctx);
        filled = duk_to_boolean(ctx, 4);
    }

	drawCircle(x, y, r, color, filled);
	return 0;
}

void athena_register_shape(duk_context *ctx, duk_c_function ctor, duk_c_function draw, const char* name){
    duk_push_c_function(ctx, ctor, DUK_VARARGS);
    duk_push_object(ctx);
    duk_push_c_function(ctx, draw, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "draw");
    duk_put_prop_string(ctx, -2, "prototype");
    duk_put_global_string(ctx, name);
}

void athena_shape_init(duk_context* ctx){
	athena_register_shape(ctx, athena_point_ctor, athena_point_draw, "Point");
    athena_register_shape(ctx, athena_line_ctor, athena_line_draw, "Line");
    athena_register_shape(ctx, athena_triangle_ctor, athena_triangle_draw, "Triangle");
    athena_register_shape(ctx, athena_quad_ctor, athena_quad_draw, "Quad");
    athena_register_shape(ctx, athena_rect_ctor, athena_rect_draw, "Rect");
    athena_register_shape(ctx, athena_circle_ctor, athena_circle_draw, "Circle");
}