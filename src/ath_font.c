
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "include/fntsys.h"
#include "ath_env.h"

const unsigned int osdsys_font = 0;
const unsigned int image_font = 1;
const unsigned int truetype_font = 2;

duk_ret_t athena_font_dtor(duk_context *ctx){
    duk_get_prop_string(ctx, 0, "\xff""\xff""type");
    unsigned int type = duk_to_uint(ctx, -1);
    duk_pop(ctx);

    if (type == 1) {
        duk_get_prop_string(ctx, 0, "\xff""\xff""data");
		GSFONT* font = (GSFONT*)duk_to_uint(ctx, -1);
		duk_pop(ctx);

	    unloadFont(font);
    } else {
        duk_get_prop_string(ctx, 0, "\xff""\xff""data");
		int fontid = duk_to_int(ctx, -1);
        fntRelease(fontid);
    }

	return 0;
}

duk_ret_t athena_font_ctor(duk_context *ctx){
    int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1) return duk_generic_error(ctx, "wrong number of arguments"); 
    if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;

    duk_push_this(ctx);

    if (argc == 1) {
        const char* path = duk_get_string(ctx, 0);

        int handle = fntLoadFile(path);

        if (handle != -1){
            duk_push_int(ctx, handle);
            duk_put_prop_string(ctx, -2, "\xff""\xff""data");

            duk_push_uint(ctx, truetype_font);

        } else {
            GSFONT* font = loadFont(path);
            if (font == NULL) return duk_generic_error(ctx, "Error loading font (invalid magic).");

            duk_push_uint(ctx, font);
            duk_put_prop_string(ctx, -2, "\xff""\xff""data");

            duk_push_uint(ctx, image_font);
        }
    } else {
        duk_push_uint(ctx, osdsys_font);
    }

    duk_put_prop_string(ctx, -2, "\xff""\xff""type");

	duk_push_uint(ctx, (uint32_t)(0x80808080));
    duk_put_prop_string(ctx, -2, "color");

    duk_push_number(ctx, (float)(1.0f));
    duk_put_prop_string(ctx, -2, "\xff""\xff""scale");

    duk_push_c_function(ctx, athena_font_dtor, 1);
    duk_set_finalizer(ctx, -2);

	return 0;
}

duk_ret_t athena_font_print(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 3) return duk_generic_error(ctx, "wrong number of arguments");

    unsigned int type = get_obj_uint(ctx, -1, "\xff""\xff""type");
	Color color = get_obj_uint(ctx, -1, "color");
    float scale =  get_obj_float(ctx, -1, "\xff""\xff""scale");
    float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
    const char* text = duk_get_string(ctx, 2);

    if (type == 1){
        GSFONT* font = (GSFONT*)get_obj_uint(ctx, -1, "\xff""\xff""data");
	    printFontText(font, text, x, y, scale, color);
    } else if (type == 0){
        printFontMText(text, x, y, scale, color);
    } else {
        int fontid = get_obj_int(ctx, -1, "\xff""\xff""data");
        fntRenderString(fontid, x, y, 0, 0, 0, text, color);
    }

	return 0;
}

duk_ret_t athena_font_setscale(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments");

    unsigned int type = get_obj_uint(ctx, -1, "\xff""\xff""type");

    float scale = duk_get_number(ctx, 0);

    duk_push_this(ctx);
    duk_push_number(ctx, (float)(scale));
    duk_put_prop_string(ctx, -2, "\xff""\xff""scale");

    if (type == truetype_font){
        int fontid = get_obj_int(ctx, -1, "\xff""\xff""data");
        fntSetCharSize(fontid, FNTSYS_CHAR_SIZE*64*scale, FNTSYS_CHAR_SIZE*64*scale);
    }

	return 0;
}

void font_init(duk_context *ctx) {
    duk_push_c_function(ctx, athena_font_ctor, DUK_VARARGS);

    duk_push_object(ctx);

    duk_push_c_function(ctx, athena_font_print, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "print");

    duk_push_c_function(ctx, athena_font_setscale, 1);
    duk_put_prop_string(ctx, -2, "setScale");

    duk_put_prop_string(ctx, -2, "prototype");

    duk_put_global_string(ctx, "Font");
}

void athena_font_init(duk_context* ctx){
	font_init(ctx);
    fntInit();
}