#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "ath_env.h"

#include "include/taskman.h"

static JSValue athena_killtask(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
  	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments.");
	int task ;
	JS_ToInt32(ctx, &task, argv[0]) ;

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
	JS_CFUNC_DEF("get", 0, athena_gettasklist),
	JS_CFUNC_DEF("kill", 1, athena_killtask),
};

static int task_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_task_init(JSContext* ctx){
    return athena_push_module(ctx, task_init, module_funcs, countof(module_funcs), "Tasks");
}