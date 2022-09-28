#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "include/fntsys.h"
#include "ath_env.h"

typedef struct AsyncImage {
	const char* path;
	GSTEXTURE* handle;
	bool delayed;
} AsyncImage;

typedef struct ImgList {
    AsyncImage** list;
	int size;
} ImgList;

static ImgList img_list;
static int async_img_sema = -1;

static int imgThread(void* data)
{
	while(true){
		WaitSema(async_img_sema);

		for(int i = 0; i < img_list.size; i++){
			load_image(img_list.list[i]->handle, img_list.list[i]->path, img_list.list[i]->delayed);
		}

		free(img_list.list);
		img_list.size = 0;
		
	}
	return 0;
}

int append_img(AsyncImage* img)
{

    AsyncImage** aux = malloc((img_list.size+1)*sizeof(AsyncImage*));

	if(img_list.size > 0){
		memcpy(aux, img_list.list, img_list.size*sizeof(AsyncImage*));
		free(img_list.list);
	}
    
    img_list.list = aux;

	img_list.list[img_list.size] = img;

    img_list.size++;
    
    return 0;
}

static int load_img_async(GSTEXTURE* image, const char* path, uint32_t delayed) {
	AsyncImage* async_img = malloc(sizeof(AsyncImage));

	async_img->path = path;
	async_img->handle = image;
	async_img->delayed = (delayed == 2? false : true);

	append_img(async_img);

	return 0;
}

static duk_ret_t athena_image_processlist(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if(argc != 0) return duk_generic_error(ctx, "AsyncImg.process() takes no arguments");

	printf("AsyncImg.process() was called successfully!\n");
	SignalSema(async_img_sema);

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
	
	int argc = duk_get_top(ctx);
	if (argc != 1 && argc != 2) return duk_generic_error(ctx, "Image takes 1 or 2 arguments");
    if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;

	GSTEXTURE* image = malloc(sizeof(GSTEXTURE));

    duk_push_this(ctx);

	const char* text = duk_get_string(ctx, 0);

	if (argc == 2) delayed = duk_get_uint(ctx, 1);

	if(delayed == 2 || delayed == 3){
		load_img_async(image, text, delayed);

		duk_push_boolean(ctx, false);
    	duk_put_prop_string(ctx, -2, "\xff""\xff""loaded");

	} else if(delayed == 0 || delayed == 1){
		load_image(image, text, delayed);
		if (image == NULL) duk_generic_error(ctx, "Failed to load image %s.", text);
		
	} else {
		duk_generic_error(ctx, "Image mode not supported!");
	}

	duk_push_uint(ctx, image);
    duk_put_prop_string(ctx, -2, "\xff""\xff""data");

	duk_push_number(ctx, (float)(image->Width));
    duk_put_prop_string(ctx, -2, "width");

	duk_push_number(ctx, (float)(image->Height));
    duk_put_prop_string(ctx, -2, "height");

	duk_push_number(ctx, (float)(image->Width));
    duk_put_prop_string(ctx, -2, "endx");

	duk_push_number(ctx, (float)(image->Height));
    duk_put_prop_string(ctx, -2, "endy");

	duk_push_number(ctx, (float)(0.0f));
    duk_put_prop_string(ctx, -2, "startx");

	duk_push_number(ctx, (float)(0.0f));
    duk_put_prop_string(ctx, -2, "starty");

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

DUK_EXTERNAL duk_ret_t dukopen_image(duk_context *ctx) {
	const duk_function_list_entry module_funcs[] = {
	  { "process",  	athena_image_processlist,		0 },
	  { NULL, NULL, 0 }
	};

  duk_push_object(ctx);  /* module result */
  duk_put_function_list(ctx, -1, module_funcs);

  return 1;  /* return module value */
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

    ee_sema_t sema;
    sema.init_count = 0;
    sema.max_count = 1;
    sema.option = 0;
    async_img_sema = CreateSema(&sema);

	int task = create_task("Image: Async loading", (void*)imgThread, 4096, 16);
	init_task(task, NULL);

	duk_push_int(ctx, task);
    duk_put_global_string(ctx, "ASYNC_IMG_TASKID");

	push_athena_module(dukopen_image, "AsyncImg");

}

void athena_graphics_init(duk_context* ctx){
	image_init(ctx);
}
