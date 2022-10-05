#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "ath_env.h"

static int imgThread(void* data)
{
	ImgList* list = data;

	while(true){
		WaitSema(list->sema_id);
		for(int i = 0; i < list->size; i++) load_image(list->list[i]->handle, list->list[i]->path, list->list[i]->delayed);
		free(list->list);
		list->size = 0;
	}

	return 0;
}

static duk_ret_t athena_imagelist_dtor(duk_context *ctx){

    duk_get_prop_string(ctx, 0, "\xff""\xff""deleted");
    bool deleted = duk_to_boolean(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, 0, "\xff""\xff""handle");
    ImgList* list = duk_to_uint(ctx, -1);
    duk_pop(ctx);

	if(!deleted){
		kill_task(list->thread_id);
		DeleteSema(list->sema_id);
		if(list->size > 0) free(list->list);
		free(list);

        duk_push_boolean(ctx, true);
        duk_put_prop_string(ctx, 0, "\xff""\xff""deleted");
	}

	return 0;
}

static duk_ret_t athena_imagelist_ctor(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if(argc != 0) return duk_generic_error(ctx, "new ImageAsync() takes no arguments");

    ee_sema_t sema; sema.init_count = 0; sema.max_count = 1; sema.option = 0;
    int sema_id = CreateSema(&sema);

	ImgList* list = malloc(sizeof(ImgList));
	list->size = 0;
	list->sema_id = sema_id;

	int task = create_task("AsyncImage: Loading Thread", (void*)imgThread, 4096, 16);
	init_task(task, list);

	list->thread_id = task;

    duk_push_this(ctx);

	duk_push_uint(ctx, list);
    duk_put_prop_string(ctx, -2, "\xff""\xff""handle");


    duk_push_boolean(ctx, false);
    duk_put_prop_string(ctx, -2, "\xff""\xff""deleted");

    duk_push_c_function(ctx, athena_imagelist_dtor, 1);
    duk_set_finalizer(ctx, -2);

	return 0;
}

static duk_ret_t athena_imagelist_process(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if(argc != 0) return duk_generic_error(ctx, "ImageList.process() takes no arguments");

	ImgList* handle = get_obj_uint(ctx, -1, "\xff""\xff""handle");
	SignalSema(handle->sema_id);

	return 0;
}

void athena_imagelist_init(duk_context *ctx) {
    duk_push_c_function(ctx, athena_imagelist_ctor, DUK_VARARGS);

    duk_push_object(ctx);

    duk_push_c_function(ctx, athena_imagelist_process, 0);
    duk_put_prop_string(ctx, -2, "process");

    duk_put_prop_string(ctx, -2, "prototype");

    duk_put_global_string(ctx, "ImageList");
}