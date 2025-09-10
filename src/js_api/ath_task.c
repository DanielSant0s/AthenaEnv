#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <ath_env.h>

#include <taskman.h>

typedef struct {
    int id;
    JSContext *ctx;
    JSValue func;
    bool exit;
} thread_info_t;

static JSClassID js_thread_class_id;

void worker_thread(void *arg) {
    thread_info_t *tinfo = (thread_info_t *)arg;

    JSRuntime *rt = JS_GetRuntime(tinfo->ctx);

    JS_UpdateStackTop(rt);

    JSValue ret, func1;

    func1 = JS_DupValueRT(rt, tinfo->func);
    ret = JS_Call(tinfo->ctx, func1, JS_UNDEFINED, 0, NULL);

    JS_FreeValueRT(rt, func1);
    JS_FreeValueRT(rt, ret);

    if (tinfo->exit) {
        exit_task();
    }
}

static JSValue athena_newtask(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    thread_info_t *tinfo;
    
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

    obj = JS_NewObjectClass(ctx, js_thread_class_id);
	if (JS_IsException(obj))
        goto fail;
        tinfo = js_mallocz(ctx, sizeof(*tinfo));
    if (!tinfo)
        return JS_EXCEPTION;

    tinfo->ctx = ctx;
    tinfo->func = JS_DupValue(ctx, argv[0]);
    tinfo->exit = true;

    if (argc > 1) {
        tinfo->exit = JS_ToBool(ctx, argv[1]);
    }

	tinfo->id = create_task("Athena: Worker thread", worker_thread, 16000, 16);

    if (tinfo->id < 0) {
        return JS_ThrowInternalError(ctx, "Failed to create thread");
    }

    JS_SetOpaque(obj, tinfo);
    return obj;

 fail:
    js_free(ctx, tinfo);
    return JS_EXCEPTION;
}

static JSValue athena_killtask(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
  	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments.");
	int task;
	JS_ToInt32(ctx, &task, argv[0]);

	kill_task(task);

	return JS_UNDEFINED;
}

static JSValue athena_gettasklist(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
      if (argc != 0) return JS_ThrowSyntaxError(ctx, "wrong number of arguments.");
    JSValue array, obj;

    Task* tasks = get_tasks();

    array = JS_NewArray(ctx);

    for(int i = 0; i < MAX_THREADS; i++){
        if (!is_invalid_task(&tasks[i])){
            obj = JS_NewObject(ctx);
            JS_DefinePropertyValueStr(ctx, obj, "id", JS_NewUint32(ctx, tasks[i].id), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "name", JS_NewString(ctx, tasks[i].title), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "status", JS_NewInt32(ctx, tasks[i].status), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "stack", JS_NewUint32(ctx, tasks[i].stack_size), JS_PROP_C_W_E);
            JS_DefinePropertyValueUint32(ctx, array, i, obj, JS_PROP_C_W_E);
        }
    }

    return array;
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("new", 1, athena_newtask),
	JS_CFUNC_DEF("get", 0, athena_gettasklist),
	JS_CFUNC_DEF("kill", 1, athena_killtask),
};

static JSValue athena_start_thread(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    thread_info_t *tinfo = JS_GetOpaque2(ctx, this_val, js_thread_class_id);

    init_task(tinfo->id, tinfo);

    return JS_UNDEFINED;
}

static JSValue athena_stop_thread(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    thread_info_t *tinfo = JS_GetOpaque2(ctx, this_val, js_thread_class_id);
    kill_task(tinfo->id);

    return JS_UNDEFINED;
}

static JSValue athena_exit_set(JSContext *ctx, JSValueConst this_val, JSValue val){
    thread_info_t *tinfo = JS_GetOpaque2(ctx, this_val, js_thread_class_id);

    tinfo->exit = JS_ToBool(ctx, val);
    return JS_UNDEFINED;
}

static JSValue athena_exit_get(JSContext *ctx, JSValueConst this_val){
    thread_info_t *tinfo = JS_GetOpaque2(ctx, this_val, js_thread_class_id);

    return JS_NewBool(ctx, tinfo->exit);
}


static const JSCFunctionListEntry js_thread_proto_funcs[] = {
    JS_CFUNC_DEF("start", 0, athena_start_thread),
    JS_CFUNC_DEF("stop", 0, athena_stop_thread),
    JS_CGETSET_DEF("exit", athena_exit_get, athena_exit_set)
};

static void athena_thread_free(JSRuntime *rt, thread_info_t *thread) {
    free_task(thread->id);

    JS_FreeValueRT(rt, thread->func);
    js_free_rt(rt, thread);
}

static void js_thread_finalizer(JSRuntime *rt, JSValue val) {
    thread_info_t *tinfo = JS_GetOpaque(val, js_thread_class_id);

    if (tinfo) {
        athena_thread_free(rt, tinfo);
        JS_SetOpaque(val, NULL);
    }

    
}

void js_thread_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) {
    thread_info_t *tinfo = JS_GetOpaque(val, js_thread_class_id);

    if (tinfo) {
        JS_MarkValue(rt, tinfo->func, mark_func);
    }
}

static JSClassDef js_thread_class = {
    "Thread",
    .finalizer = js_thread_finalizer,
    .gc_mark = js_thread_mark,

};

static int task_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue proto;

    /* the class ID is created once */
    JS_NewClassID(&js_thread_class_id);
    /* the class is created once per runtime */
    JS_NewClass(JS_GetRuntime(ctx), js_thread_class_id, &js_thread_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_thread_proto_funcs, countof(js_thread_proto_funcs));
    JS_SetClassProto(ctx, js_thread_class_id, proto);

    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_task_init(JSContext* ctx) {
    return athena_push_module(ctx, task_init, module_funcs, countof(module_funcs), "Threads");
}

void athena_task_free(JSContext *ctx) {
}