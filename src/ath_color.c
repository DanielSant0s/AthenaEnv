#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "ath_env.h"

duk_ret_t athena_color_ctor(duk_context *ctx){
  	int argc = duk_get_top(ctx);
	unsigned int r = duk_get_uint(ctx, 0);
	unsigned int g = duk_get_uint(ctx, 1);
	unsigned int b = duk_get_uint(ctx, 2);
  	unsigned int a = 0x80;
	if (argc == 4) a = duk_get_uint(ctx, 3);

	Color rgba = r | (g << 8) | (b << 16) | (a << 24);

	duk_push_this(ctx);

	duk_push_uint(ctx, (rgba));
	duk_put_prop_string(ctx, -2, "\xff""\xff""rgba");

	return 0;
}


duk_ret_t athena_getR(duk_context *ctx){
	int color = get_obj_uint(ctx, -1, "\xff""\xff""rgba");
	duk_push_uint(ctx, R(color));
	return 1;
}

duk_ret_t athena_getG(duk_context *ctx){
	int color = get_obj_uint(ctx, -1, "\xff""\xff""rgba");
	duk_push_uint(ctx, G(color));
	return 1;
}

duk_ret_t athena_getB(duk_context *ctx){
	int color = get_obj_uint(ctx, -1, "\xff""\xff""rgba");
	duk_push_uint(ctx, B(color));
	return 1;
}

duk_ret_t athena_getA(duk_context *ctx){
	int color = get_obj_uint(ctx, -1, "\xff""\xff""rgba");
	duk_push_uint(ctx, A(color));
	return 1;
}

duk_ret_t athena_setR(duk_context *ctx){
	int color = get_obj_uint(ctx, -1, "\xff""\xff""rgba");

	int r = duk_get_uint(ctx, 0);

	duk_push_this(ctx);

	duk_push_uint(ctx, (r | (G(color) << 8) | (B(color) << 16) | (A(color) << 24)));
    duk_put_prop_string(ctx, -2, "\xff""\xff""rgba");
	return 0;
}

duk_ret_t athena_setG(duk_context *ctx){
	int color = get_obj_uint(ctx, -1, "\xff""\xff""rgba");

	int g = duk_get_uint(ctx, 0);

	duk_push_this(ctx);

	duk_push_uint(ctx, (R(color) | (g << 8) | (B(color) << 16) | (A(color) << 24)));
    duk_put_prop_string(ctx, -2, "\xff""\xff""rgba");
	return 0;
}

duk_ret_t athena_setB(duk_context *ctx){
	int color = get_obj_uint(ctx, -1, "\xff""\xff""rgba");

	int b = duk_get_uint(ctx, 0);

	duk_push_this(ctx);

	duk_push_uint(ctx, (R(color) | (G(color) << 8) | (b << 16) | (A(color) << 24)));
    duk_put_prop_string(ctx, -2, "\xff""\xff""rgba");
	return 0;
}

duk_ret_t athena_setA(duk_context *ctx){
	int color = get_obj_uint(ctx, -1, "\xff""\xff""rgba");

	int a = duk_get_uint(ctx, 0);

	duk_push_this(ctx);

	duk_push_uint(ctx, (R(color) | (G(color) << 8) | (B(color) << 16) | (a << 24)));
    duk_put_prop_string(ctx, -2, "\xff""\xff""rgba");
	return 0;
}

void athena_color_init(duk_context* ctx){
    duk_push_c_function(ctx, athena_color_ctor, DUK_VARARGS);

    duk_push_object(ctx);

    duk_push_c_function(ctx, athena_getR, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "getR");

    duk_push_c_function(ctx, athena_getG, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "getG");

    duk_push_c_function(ctx, athena_getB, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "getB");

    duk_push_c_function(ctx, athena_getA, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "getA");

    duk_push_c_function(ctx, athena_setR, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "setR");

    duk_push_c_function(ctx, athena_setG, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "setG");

    duk_push_c_function(ctx, athena_setB, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "setB");

    duk_push_c_function(ctx, athena_setA, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "setA");

    duk_put_prop_string(ctx, -2, "prototype");

    duk_put_global_string(ctx, "Color");
	
}