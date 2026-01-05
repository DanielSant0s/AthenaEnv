#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <ath_env.h>

#include <lockman.h>

static JSClassID js_mutex_class_id;

static void athena_mutex_dtor(JSRuntime *rt, JSValue val) {  
    int id = (int)JS_GetOpaque(val, js_mutex_class_id);

    if (id != -1) {
        delete_mutex(id);
        JS_SetOpaque(val, ((int)-1));
    }
}

static JSValue athena_mutex_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

    int id = create_mutex();

    obj = JS_NewObjectClass(ctx, js_mutex_class_id);
    if (JS_IsException(obj))
        goto fail;

    JS_SetOpaque(obj, (void*)id);

    return obj;

 fail:
    js_free(ctx, id);
    return JS_EXCEPTION;
}

static JSValue athena_lock(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    int id = (int)JS_GetOpaque(this_val, js_mutex_class_id);

    lock_mutex(id);

    return JS_UNDEFINED;
}

static JSValue athena_unlock(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    int id = (int)JS_GetOpaque(this_val, js_mutex_class_id);

    unlock_mutex(id);

    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_mutex_proto_funcs[] = {
    JS_CFUNC_DEF("lock", 0, athena_lock),
    JS_CFUNC_DEF("unlock", 0, athena_unlock),
};

static JSClassDef js_mutex_class = {
    "Mutex",
    .finalizer = athena_mutex_dtor,
}; 

static int mutex_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue mutex_proto, mutex_class;
    
    /* create the mutex class */
    JS_NewClassID(&js_mutex_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_mutex_class_id, &js_mutex_class);

    mutex_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, mutex_proto, js_mutex_proto_funcs, countof(js_mutex_proto_funcs));
    
    mutex_class = JS_NewCFunction2(ctx, athena_mutex_ctor, "Mutex", 2, JS_CFUNC_constructor_or_func, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, mutex_class, mutex_proto);
    JS_SetClassProto(ctx, js_mutex_class_id, mutex_proto);
      
    //return 0;
    return JS_SetModuleExport(ctx, m, "default", mutex_class);
}

JSModuleDef *athena_mutex_init(JSContext* ctx) {
    JSModuleDef *m = JS_NewCModule(ctx, "Mutex", mutex_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "default");
}


