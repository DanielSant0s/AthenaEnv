#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "include/fntsys.h"
#include "ath_env.h"


float get_obj_float(duk_context* ctx, duk_idx_t idx, const char* key){
	duk_push_this(ctx);
    duk_get_prop_string(ctx, idx, key);
	return duk_to_number(ctx, idx);
}

uint32_t get_obj_uint(duk_context* ctx, duk_idx_t idx, const char* key){
	duk_push_this(ctx);
    duk_get_prop_string(ctx, idx, key);
	return duk_to_uint(ctx, idx);
}

static duk_ret_t athena_image_dtor(duk_context *ctx){

    // The object to delete is passed as first argument instead
    duk_get_prop_string(ctx, 0, "\xff""\xff""deleted");

    bool deleted = duk_to_boolean(ctx, -1);
    duk_pop(ctx);

	if(!deleted){
		duk_get_prop_string(ctx, 0, "\xff""\xff""data");
		GSTEXTURE* source = (GSTEXTURE*)duk_to_uint(ctx, -1);
		duk_pop(ctx);

		printf("Freeing Image! Handle: 0x%x\n", (unsigned int)source);

		UnloadTexture(source);

		free(source->Mem);
		source->Mem = NULL;

		if(source->Clut != NULL)
		{
			free(source->Clut);
			source->Clut = NULL;
		}

		free(source);
		source = NULL;

        duk_push_boolean(ctx, true);
        duk_put_prop_string(ctx, 0, "\xff""\xff""deleted");
	}

	return 0;
}

static duk_ret_t athena_image_ctor(duk_context *ctx) {
	int argc = duk_get_top(ctx);

    if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;

    duk_push_this(ctx);

	const char* text = duk_get_string(ctx, 0);
	bool delayed = true;
	if (argc == 2) delayed = duk_get_boolean(ctx, 1);
	GSTEXTURE* image = load_image(text, delayed);
	if (image == NULL) duk_generic_error(ctx, "Failed to load image %s.", text);

	duk_push_uint(ctx, (void*)(image));
    duk_put_prop_string(ctx, -2, "\xff""\xff""data");

	duk_push_number(ctx, (float)(image->Width));
    duk_put_prop_string(ctx, -2, "width");

	duk_push_number(ctx, (float)(image->Height));
    duk_put_prop_string(ctx, -2, "height");

	duk_push_number(ctx, (float)(0.0f));
    duk_put_prop_string(ctx, -2, "startx");

	duk_push_number(ctx, (float)(0.0f));
    duk_put_prop_string(ctx, -2, "starty");

	duk_push_number(ctx, (float)(image->Width));
    duk_put_prop_string(ctx, -2, "endx");

	duk_push_number(ctx, (float)(image->Height));
    duk_put_prop_string(ctx, -2, "endy");

	duk_push_number(ctx, (float)(0.0f));
    duk_put_prop_string(ctx, -2, "angle");

	duk_push_uint(ctx, (uint32_t)(0x80808080));
    duk_put_prop_string(ctx, -2, "color");

	duk_push_uint(ctx, (uint32_t)(GS_FILTER_NEAREST));
    duk_put_prop_string(ctx, -2, "filter");

    duk_push_boolean(ctx, false);
    duk_put_prop_string(ctx, -2, "\xff""\xff""deleted");

    duk_push_c_function(ctx, athena_image_dtor, 1);
    duk_set_finalizer(ctx, -2);

    return 0;
}


static duk_ret_t athena_image_draw(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 2 && argc != 0) return duk_generic_error(ctx, "draw takes x and y arguments(2)!");

	float x = 0.0f;
	float y = 0.0f;

	if(argc == 2){
		x = duk_get_number(ctx, 0);
		y = duk_get_number(ctx, 1);
	}

  	GSTEXTURE* source = (GSTEXTURE*)get_obj_uint(ctx, -1, "\xff""\xff""data");
	float width = get_obj_float(ctx, -1, "width");
	float height = get_obj_float(ctx, -1, "height");
	float startx = get_obj_float(ctx, -1, "startx");
	float starty = get_obj_float(ctx, -1, "starty");
	float endx = get_obj_float(ctx, -1, "endx");
	float endy = get_obj_float(ctx, -1, "endy");
	float angle = get_obj_float(ctx, -1, "angle");
	Color color = (Color)get_obj_uint(ctx, -1, "color");
	source->Filter = get_obj_uint(ctx, -1, "filter");

	if(angle != 0.0f){
		drawImageRotate(source, x, y, width, height, startx, starty, endx, endy, angle, color);
	} else {
		drawImage(source, x, y, width, height, startx, starty, endx, endy, color);
	}

	return 0;
}


void image_init(duk_context *ctx) {
    duk_push_c_function(ctx, athena_image_ctor, DUK_VARARGS);

    duk_push_object(ctx);

    duk_push_c_function(ctx, athena_image_draw, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "draw");

    duk_put_prop_string(ctx, -2, "prototype");

    duk_put_global_string(ctx, "Image");
}


static duk_ret_t athena_fontload(duk_context *ctx){
	if (duk_get_top(ctx) != 1) return duk_generic_error(ctx, "wrong number of arguments"); 
	const char* path = duk_get_string(ctx, 0);
	GSFONT* font = loadFont(path);
	if (font == NULL) return duk_generic_error(ctx, "Error loading font (invalid magic).");
	duk_push_uint(ctx, (void*)(font));
	return 1;
}

static duk_ret_t athena_print(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 5 && argc != 6) return duk_generic_error(ctx, "wrong number of arguments");
	GSFONT* font = (GSFONT*)duk_get_uint(ctx, 0);
    float x = duk_get_number(ctx, 1);
	float y = duk_get_number(ctx, 2);
    float scale =  duk_get_number(ctx, 3);
    const char* text = duk_get_string(ctx, 4);
	Color color = 0x80808080;
	if (argc == 6) color = duk_get_int(ctx, 5);
	printFontText(font, text, x, y, scale, color);
	return 0;
}

static duk_ret_t athena_fontunload(duk_context *ctx){
	int argc = duk_get_top(ctx); 
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments"); 
	GSFONT* font = (GSFONT*)duk_get_uint(ctx, 0);
	unloadFont(font);
	return 0;
}

static duk_ret_t athena_ftinit(duk_context *ctx){
	if (duk_get_top(ctx) != 0) return duk_generic_error(ctx, "wrong number of arguments"); 
	fntInit();
	return 0;
}

static duk_ret_t athena_ftload(duk_context *ctx){
	if (duk_get_top(ctx) != 1) return duk_generic_error(ctx, "wrong number of arguments"); 
	const char* fontpath = duk_get_string(ctx, 0);
	int fntHandle = fntLoadFile(fontpath);
    duk_push_int(ctx, fntHandle);
	return 1;
}

static duk_ret_t athena_ftSetPixelSize(duk_context *ctx) {
	if (duk_get_top(ctx) != 3) return duk_generic_error(ctx, "wrong number of arguments"); 
	int fontid = duk_get_int(ctx, 0);
	int width = duk_get_number(ctx, 1);
	int height = duk_get_number(ctx, 2);
	fntSetPixelSize(fontid, width, height);
	return 0;
}


static duk_ret_t athena_ftSetCharSize(duk_context *ctx) {
	if (duk_get_top(ctx) != 3) return duk_generic_error(ctx, "wrong number of arguments"); 
	int fontid = duk_get_int(ctx, 0);
	int width = duk_get_int(ctx, 1);
	int height = duk_get_int(ctx, 2); 
	fntSetCharSize(fontid, width, height);
	return 0;
}

static duk_ret_t athena_ftprint(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 7 && argc != 8) return duk_generic_error(ctx, "wrong number of arguments");
	int fontid = duk_get_int(ctx, 0);
    int x = duk_get_int(ctx, 1);
	int y = duk_get_int(ctx, 2);
	int alignment = duk_get_int(ctx, 3);
	int width = duk_get_int(ctx, 4); 
	int height = duk_get_int(ctx, 5); 
    const char* text = duk_get_string(ctx, 6);
	Color color = 0x80808080;
	if (argc == 8) color = duk_get_uint(ctx, 7);
	fntRenderString(fontid, x, y, alignment, width, height, text, color);
	return 0;
}

static duk_ret_t athena_ftunload(duk_context *ctx){
	int argc = duk_get_top(ctx); 
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments"); 
	int fontid = duk_get_int(ctx, 0);
	fntRelease(fontid);
	return 0;
}


static duk_ret_t athena_ftend(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 0) return duk_generic_error(ctx, "wrong number of arguments");
	fntEnd();
	return 0;
}

static duk_ret_t athena_fmload(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 0) return duk_generic_error(ctx, "wrong number of arguments");
	loadFontM();
	return 0;
}

static duk_ret_t athena_fmprint(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	//if (argc != 3) return duk_generic_error(ctx, "wrong number of arguments");
    float x = duk_get_number(ctx, 0);
	float y = duk_get_number(ctx, 1);
    float scale =  duk_get_number(ctx, 2);
    const char* text = duk_get_string(ctx, 3);
	Color color = 0x80808080;
	if (argc == 5) color =  duk_get_uint(ctx, 4);
	printFontMText(text, x, y, scale, color);
	return 0;
}

static duk_ret_t athena_fmunload(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 0) return duk_generic_error(ctx, "wrong number of arguments");
	unloadFontM();
	return 0;
}


DUK_EXTERNAL duk_ret_t dukopen_font(duk_context *ctx) {
	const duk_function_list_entry module_funcs[] = {
		//FreeType functions
		{ "ftInit",            		athena_ftinit,				0 },
		{ "ftLoad",            		athena_ftload,				1 },
		{ "ftSetPixelSize",    		athena_ftSetPixelSize,		3 },
		{ "ftSetCharSize", 	   		athena_ftSetCharSize,		3 },
		{ "ftPrint",         		athena_ftprint,	  DUK_VARARGS },
		{ "ftUnload",           	athena_ftunload,			1 },
		{ "ftEnd",           	    athena_ftend,				0 },
		//gsFont function
		{ "load",	                athena_fontload,			1 },
		{ "print",                  athena_print,	  DUK_VARARGS },
		{ "unload",                	athena_fontunload,			1 },
		//gsFontM function
		{ "fmLoad",            		athena_fmload,				0 }, 
		{ "fmPrint",           		athena_fmprint,	  DUK_VARARGS }, 
		{ "fmUnload",         	    athena_fmunload,			0 }, 
		{ NULL, NULL, 0 }
	};

  duk_push_object(ctx);  /* module result */
  duk_put_function_list(ctx, -1, module_funcs);

  return 1;  /* return module value */
}

void athena_graphics_init(duk_context* ctx){
	//push_athena_module(dukopen_graphics,   "Graphics");
	image_init(ctx);
	push_athena_module(dukopen_font,   	  	   "Font");

	duk_push_uint(ctx, GS_FILTER_NEAREST);
	duk_put_global_string(ctx, "NEAREST");

	duk_push_uint(ctx, GS_FILTER_LINEAR);
	duk_put_global_string(ctx, "LINEAR");
}