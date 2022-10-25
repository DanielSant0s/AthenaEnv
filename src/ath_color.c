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

	duk_push_uint(ctx, r | (g << 8) | (b << 16) | (a << 24));
	return 1;
}

duk_ret_t athena_getR(duk_context *ctx){
	int color = duk_get_uint(ctx, 0);
	duk_push_uint(ctx, R(color));
	return 1;
}

duk_ret_t athena_getG(duk_context *ctx){
	int color = duk_get_uint(ctx, 0);
	duk_push_uint(ctx, G(color));
	return 1;
}

duk_ret_t athena_getB(duk_context *ctx){
	int color = duk_get_uint(ctx, 0);
	duk_push_uint(ctx, B(color));
	return 1;
}

duk_ret_t athena_getA(duk_context *ctx){
	int color = duk_get_uint(ctx, 0);
	duk_push_uint(ctx, A(color));
	return 1;
}

duk_ret_t athena_setR(duk_context *ctx){
	int color = duk_get_uint(ctx, 0);
	int r = duk_get_uint(ctx, 1);
	duk_push_uint(ctx, (r | (G(color) << 8) | (B(color) << 16) | (A(color) << 24)));
	return 1;
}

duk_ret_t athena_setG(duk_context *ctx){
	int color = duk_get_uint(ctx, 0);
	int g = duk_get_uint(ctx, 1);
	duk_push_uint(ctx, (R(color) | (g << 8) | (B(color) << 16) | (A(color) << 24)));
	return 1;
}

duk_ret_t athena_setB(duk_context *ctx){
	int color = duk_get_uint(ctx, 0);
	int b = duk_get_uint(ctx, 1);
	duk_push_uint(ctx, (R(color) | (G(color) << 8) | (b << 16) | (A(color) << 24)));
	return 1;
}

duk_ret_t athena_setA(duk_context *ctx){
	int color = duk_get_uint(ctx, 0);
	int a = duk_get_uint(ctx, 1);
	duk_push_uint(ctx, (R(color) | (G(color) << 8) | (B(color) << 16) | (a << 24)));
	return 1;
}

DUK_EXTERNAL duk_ret_t athena_open_color(duk_context *ctx) {
	const duk_function_list_entry module_funcs[] = {
		{ "new",      				   		athena_color_ctor,     DUK_VARARGS },
		{ "getR",      				   		athena_getR,           1 },
		{ "getG",      				   		athena_getG,           1 },
		{ "getB",      				   		athena_getB,           1 },
		{ "getA",      				   		athena_getA,           1 },
		{ "setR",      				   		athena_setR,           2 },
		{ "setG",      				   		athena_setG,           2 },
		{ "setB",      				   		athena_setB,           2 },
		{ "setA",      				   		athena_setA,           2 },
		{ NULL, NULL, 0 }
	};

  duk_push_object(ctx);  /* module result */
  duk_put_function_list(ctx, -1, module_funcs);

  return 1;  /* return module value */
}

void athena_color_init(duk_context* ctx){
    push_athena_module(athena_open_color,   	  "Color");
	
}