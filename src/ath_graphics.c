#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "include/fntsys.h"
#include "ath_env.h"

static uint32_t asyncDelayed = 2;

static int imgThreadResult = 1;
static GSTEXTURE* imgThreadData = NULL;

static int imgThread(void* data)
{
	char* text = (char*)data;
	imgThreadData = load_image(text, asyncDelayed);
	if (imgThreadData == NULL) 
	{
		imgThreadResult = 1;
		exitkill_task();
		return 0;
	}
	imgThreadResult = 1;
	exitkill_task();
	return 0;
}

static int load_img_async(const char* text, uint32_t delayed) {
	asyncDelayed = delayed-2;

	int task = create_task("Graphics: Loading image", (void*)imgThread, 4096, 16);

	imgThreadResult = 0;
	init_task(task, (void*)text);

	return task;
}

/*
static duk_ret_t athena_getloadstate(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if(argc != 0) return duk_generic_error(ctx, "getLoadState takes no arguments");
	duk_push_int(ctx, (uint32_t)imgThreadResult);
	return 1;
}

*/

static duk_ret_t athena_image_isloaded(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if(argc != 0) return duk_generic_error(ctx, "isLoaded takes no arguments");

	duk_push_this(ctx);

	if (imgThreadData != NULL){

		printf("Image Address: 0x%x\n", imgThreadData);

		GSTEXTURE* image = malloc(sizeof(GSTEXTURE));
		memcpy(image, imgThreadData, sizeof(GSTEXTURE));
		free(imgThreadData);
		imgThreadData = NULL;

		duk_push_uint(ctx, (void*)(image));
    	duk_put_prop_string(ctx, -2, "\xff""\xff""data");

		duk_push_boolean(ctx, true);
	}
	duk_push_boolean(ctx, false);
	return 1;
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
	uint32_t delayed = 1;
	GSTEXTURE* image;
	
	int argc = duk_get_top(ctx);
	if (argc != 1 && argc != 2) return duk_generic_error(ctx, "Image takes 1 or 2 arguments");
    if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;

    duk_push_this(ctx);

	const char* text = duk_get_string(ctx, 0);

	if (argc == 2) delayed = duk_get_uint(ctx, 1);

	if(delayed == 2 || delayed == 3){
		duk_push_int(ctx, load_img_async(text, delayed));
    	duk_put_prop_string(ctx, -2, "task_id");
	} else if(delayed == 0 || delayed == 1){
		image = load_image(text, delayed);
		if (image == NULL){
			duk_generic_error(ctx, "Failed to load image %s.", text);
		}
	} else {
		duk_generic_error(ctx, "Image mode not supported!");
	}

	duk_push_uint(ctx, image);
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

    duk_push_c_function(ctx, athena_image_isloaded, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "isLoaded");

    duk_put_prop_string(ctx, -2, "prototype");

    duk_put_global_string(ctx, "Image");
}

void athena_graphics_init(duk_context* ctx){
	//push_athena_module(dukopen_graphics,   "Graphics");
	image_init(ctx);
	//push_athena_module(dukopen_font,   	  	   "Font");

	duk_push_uint(ctx, GS_FILTER_NEAREST);
	duk_put_global_string(ctx, "NEAREST");

	duk_push_uint(ctx, GS_FILTER_LINEAR);
	duk_put_global_string(ctx, "LINEAR");

	duk_push_uint(ctx, 0);
	duk_put_global_string(ctx, "VRAM");

	duk_push_uint(ctx, 1);
	duk_put_global_string(ctx, "RAM");

	duk_push_uint(ctx, 2);
	duk_put_global_string(ctx, "ASYNC_VRAM");

	duk_push_uint(ctx, 3);
	duk_put_global_string(ctx, "ASYNC_RAM");
}
