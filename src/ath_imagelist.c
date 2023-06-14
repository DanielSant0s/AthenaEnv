#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "include/taskman.h"
#include "ath_env.h"

static JSClassID js_imagelist_class_id;

JSClassID get_imglist_class_id(){
	return js_imagelist_class_id;
}

static int imgThread(void* data)
{
	JSImgList* list = data;

	while(true){
		WaitSema(list->sema_id);
		for(int i = 0; i < list->size; i++) {
			load_image(&(list->list[i]->tex), list->list[i]->path, list->list[i]->delayed);
			list->list[i]->width = list->list[i]->tex.Width;
			list->list[i]->height = list->list[i]->tex.Height;
			list->list[i]->endx = list->list[i]->tex.Width;
			list->list[i]->endy = list->list[i]->tex.Height;
			list->list[i]->loaded = true;
		}
		free(list->list);
		list->size = 0;
	}

	return 0;
}

static void athena_imagelist_dtor(JSRuntime *rt, JSValue val){

    JSImgList *list = JS_GetOpaque(val, js_imagelist_class_id);

	kill_task(list->thread_id);
	DeleteSema(list->sema_id);
	if(list->size > 0) free(list->list);
	
	js_free_rt(rt, list);
}

static JSValue athena_imagelist_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv){
	JSImgList* list;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

	list = js_mallocz(ctx, sizeof(*list));
    if (!list)
        return JS_EXCEPTION;

    ee_sema_t sema; sema.init_count = 0; sema.max_count = 1; sema.option = 0;
    int sema_id = CreateSema(&sema);

	list->size = 0;
	list->sema_id = sema_id;

	int task = create_task("AsyncImage: Loading Thread", (void*)imgThread, 4096, 16);
	init_task(task, list);

	list->thread_id = task;

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, js_imagelist_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;
    JS_SetOpaque(obj, list);
    return obj;

 fail:
    js_free(ctx, list);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue athena_imagelist_process(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSImgList* handle = JS_GetOpaque2(ctx, this_val, js_imagelist_class_id);
	SignalSema(handle->sema_id);

	return JS_UNDEFINED;
}


static JSClassDef js_imagelist_class = {
    "ImageList",
    .finalizer = athena_imagelist_dtor,
}; 

static const JSCFunctionListEntry js_imagelist_proto_funcs[] = {
    JS_CFUNC_DEF("process", 0, athena_imagelist_process ),
};

static int js_imagelist_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue imagelist_proto, imagelist_class;
    
    // create the Point class 
    JS_NewClassID(&js_imagelist_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_imagelist_class_id, &js_imagelist_class);

    imagelist_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, imagelist_proto, js_imagelist_proto_funcs, countof(js_imagelist_proto_funcs));
    
    imagelist_class = JS_NewCFunction2(ctx, athena_imagelist_ctor, "ImageList", 0, JS_CFUNC_constructor, 0);
    // set proto.constructor and ctor.prototype 
    JS_SetConstructor(ctx, imagelist_class, imagelist_proto);
    JS_SetClassProto(ctx, js_imagelist_class_id, imagelist_proto);
                      
    JS_SetModuleExport(ctx, m, "ImageList", imagelist_class);
    return 0;
}

JSModuleDef *athena_imagelist_init(JSContext *ctx)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, "ImageList", js_imagelist_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "ImageList");

    dbgprintf("AthenaEnv: %s module pushed at 0x%x\n", "ImageList", m);
    return m;
}