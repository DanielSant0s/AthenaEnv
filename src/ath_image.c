#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "ath_env.h"


int append_img(AsyncImage* img, ImgList* list)
{
    AsyncImage** aux = malloc((list->size+1)*sizeof(AsyncImage*));

	if(list->size > 0){
		memcpy(aux, list->list, list->size*sizeof(AsyncImage*));
		free(list->list);
	}
    
    list->list = aux;
	list->list[list->size] = img;
    list->size++;
    
    return 0;
}

static int load_img_async(GSTEXTURE* image, const char* path, bool delayed, ImgList* list) {
	AsyncImage* async_img = malloc(sizeof(AsyncImage));

	async_img->path = path;
	async_img->handle = image;
	async_img->delayed = delayed;

	append_img(async_img, list);

	return 0;
}


static duk_ret_t athena_image_isloaded(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if(argc != 0) return duk_generic_error(ctx, "isLoaded takes no arguments");

	bool loaded = get_obj_boolean(ctx, -1, "\xff""\xff""loaded");
	GSTEXTURE* testimg = get_obj_uint(ctx, -1, "\xff""\xff""data");

	if(loaded){
		duk_push_boolean(ctx, true);
		return 1;
	} else {
		if (testimg->Width != 0){
			duk_push_this(ctx);

		    duk_push_boolean(ctx, true);
    		duk_put_prop_string(ctx, -2, "\xff""\xff""loaded");

				duk_push_number(ctx, (float)(testimg->Width));
    			duk_put_prop_string(ctx, -2, "width");

				duk_push_number(ctx, (float)(testimg->Height));
    			duk_put_prop_string(ctx, -2, "height");

				duk_push_number(ctx, (float)(testimg->Width));
    			duk_put_prop_string(ctx, -2, "endx");

				duk_push_number(ctx, (float)(testimg->Height));
    			duk_put_prop_string(ctx, -2, "endy");

			duk_push_boolean(ctx, true);
			return 1;
		}
		duk_push_boolean(ctx, false);
		return 1;
	}
	
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
	if (argc != 1 && argc != 2 && argc != 3) return duk_generic_error(ctx, "Image takes 1, 2 or 3 arguments");
    if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;

	GSTEXTURE* image = malloc(sizeof(GSTEXTURE));
	image->Width = 0;
	image->Height = 0;

    duk_push_this(ctx);

	const char* text = duk_get_string(ctx, 0);

	bool delayed = true;
	if (argc > 1) delayed = duk_get_boolean(ctx, 1);

	if(argc > 2) {
		duk_get_prop_string(ctx, 2, "\xff""\xff""handle");
		ImgList* list = duk_get_uint(ctx, -1);
		duk_pop(ctx);

		load_img_async(image, text, delayed, list);

		duk_push_boolean(ctx, false);
    	duk_put_prop_string(ctx, -2, "\xff""\xff""loaded");

	} else {
		load_image(image, text, delayed);
		if (image == NULL) duk_generic_error(ctx, "Failed to load image %s.", text);

		duk_push_number(ctx, (float)(image->Width));
    	duk_put_prop_string(ctx, -2, "width");

		duk_push_number(ctx, (float)(image->Height));
    	duk_put_prop_string(ctx, -2, "height");

		duk_push_number(ctx, (float)(image->Width));
    	duk_put_prop_string(ctx, -2, "endx");

		duk_push_number(ctx, (float)(image->Height));
    	duk_put_prop_string(ctx, -2, "endy");
		
	}

	duk_push_uint(ctx, image);
    duk_put_prop_string(ctx, -2, "\xff""\xff""data");

	duk_push_number(ctx, (float)(0.0f));
    duk_put_prop_string(ctx, -2, "startx");

	duk_push_number(ctx, (float)(0.0f));
    duk_put_prop_string(ctx, -2, "starty");

	duk_push_number(ctx, (float)(0.0f));
    duk_put_prop_string(ctx, -2, "angle");

    duk_push_this(ctx);
	duk_push_uint(ctx, (uint32_t)(0x80808080));
    duk_put_prop_string(ctx, -2, "\xff""\xff""rgba");

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

    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "color");
    duk_get_prop_string(ctx, -1, "\xff""\xff""rgba");
	Color color = duk_get_uint(ctx, -1);
	duk_pop(ctx);
	
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
    duk_put_prop_string(ctx, -2, "ready");

    duk_put_prop_string(ctx, -2, "prototype");

    duk_put_global_string(ctx, "Image");

	duk_push_uint(ctx, GS_FILTER_NEAREST);
	duk_put_global_string(ctx, "NEAREST");

	duk_push_uint(ctx, GS_FILTER_LINEAR);
	duk_put_global_string(ctx, "LINEAR");

	duk_push_boolean(ctx, false);
	duk_put_global_string(ctx, "VRAM");

	duk_push_boolean(ctx, true);
	duk_put_global_string(ctx, "RAM");
}

void athena_image_init(duk_context* ctx){
	image_init(ctx);
}
