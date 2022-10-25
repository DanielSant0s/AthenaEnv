#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "ath_env.h"

duk_ret_t athena_point_draw(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 3) return duk_generic_error(ctx, "wrong number of arguments"); 

    float x = duk_get_number(ctx, 0);
    float y = duk_get_number(ctx, 1);
    Color color = duk_get_uint(ctx, 2);

	drawPixel(x, y, color);
	return 0;
}

duk_ret_t athena_line_draw(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 5) return duk_generic_error(ctx, "wrong number of arguments"); 

    float x1 = duk_get_number(ctx, 0);
    float y1 = duk_get_number(ctx, 1);
    float x2 = duk_get_number(ctx, 2);
    float y2 = duk_get_number(ctx, 3);
    Color color = duk_get_uint(ctx, 4);

	drawLine(x1, y1, x2, y2, color);
	return 0;
}

duk_ret_t athena_triangle_draw(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 7 && argc != 9) return duk_generic_error(ctx, "wrong number of arguments"); 

    float x1 = duk_get_number(ctx, 0);
    float y1 = duk_get_number(ctx, 1);
    float x2 = duk_get_number(ctx, 2);
    float y2 = duk_get_number(ctx, 3);
    float x3 = duk_get_number(ctx, 4);
    float y3 = duk_get_number(ctx, 5);
	Color color1 = duk_get_uint(ctx, 6);

    if(argc == 7){
        drawTriangle(x1, y1, x2, y2, x3, y3, color1);
    } else {
        Color color2 = duk_get_uint(ctx, 7);
	    Color color3 = duk_get_uint(ctx, 8);
        drawTriangle_gouraud(x1, y1, x2, y2, x3, y3, color1, color2, color3);
    }
	
	return 0;
}

duk_ret_t athena_quad_draw(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 9 && argc != 12) return duk_generic_error(ctx, "wrong number of arguments"); 

    float x1 = duk_get_number(ctx, 0);
    float y1 = duk_get_number(ctx, 1);
    float x2 = duk_get_number(ctx, 2);
    float y2 = duk_get_number(ctx, 3);
    float x3 = duk_get_number(ctx, 4);
    float y3 = duk_get_number(ctx, 5);
    float x4 = duk_get_number(ctx, 6);
    float y4 = duk_get_number(ctx, 7);
	Color color1 = duk_get_uint(ctx, 8);

    if(argc == 9){
        drawQuad(x1, y1, x2, y2, x3, y3, x4, y4, color1);
    } else {
        Color color2 = duk_get_uint(ctx, 9);
	    Color color3 = duk_get_uint(ctx, 10);
	    Color color4 = duk_get_uint(ctx, 11);
        drawQuad_gouraud(x1, y1, x2, y2, x3, y3, x4, y4, color1, color2, color3, color4);
    }
	
	return 0;
}

duk_ret_t athena_rect_draw(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 5) return duk_generic_error(ctx, "wrong number of arguments"); 

    float x = duk_get_number(ctx, 0);
    float y = duk_get_number(ctx, 1);
    float w = duk_get_number(ctx, 2);
    float h = duk_get_number(ctx, 3);
	Color color = duk_get_uint(ctx, 4);

	drawRect(x, y, w, h, color);
	return 0;
}

duk_ret_t athena_circle_draw(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 4 && argc != 5) return duk_generic_error(ctx, "wrong number of arguments"); 

    float x = duk_get_number(ctx, 0);
    float y = duk_get_number(ctx, 1);
    float r = duk_get_number(ctx, 2);
    bool filled = true;
    Color color = duk_get_uint(ctx, 3);
    if (argc == 5) filled = duk_to_boolean(ctx, 4);

	drawCircle(x, y, r, color, filled);
	return 0;
}

void athena_shape_init(duk_context* ctx){
	athena_new_function(ctx, athena_point_draw, "drawPoint");
    athena_new_function(ctx, athena_line_draw, "drawLine");
    athena_new_function(ctx, athena_triangle_draw, "drawTriangle");
    athena_new_function(ctx, athena_quad_draw, "drawQuad");
    athena_new_function(ctx, athena_rect_draw, "drawRect");
    athena_new_function(ctx, athena_circle_draw, "drawCircle");
}