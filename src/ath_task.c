#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "ath_env.h"

#include "include/taskman.h"

typedef struct {
    JSContext *ctx;
    JSValue func;
    JSValue this_obj;
    int argc;
    JSValue *argv;
    JSValue ret;
} thread_info_t;

void worker_thread(void *arg) {
    thread_info_t *tinfo = (thread_info_t *)arg;

    // Executa a função
    if (JS_IsFunction(tinfo->ctx, tinfo->func)) {
        tinfo->ret = JS_Call(tinfo->ctx, tinfo->func, tinfo->this_obj, tinfo->argc, tinfo->argv);
    }
    
    // Libera a memória alocada para os argumentos
    for (int i = 0; i < tinfo->argc; i++) {
        JS_FreeValue(tinfo->ctx, tinfo->argv[i]);
    }
    free(tinfo->argv);
}

static JSValue athena_newtask(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    // Aloca a memória para a estrutura thread_info_t
    thread_info_t *tinfo = (thread_info_t *)malloc(sizeof(thread_info_t));

    // Salva as informações necessárias para executar a função com pthreads
    tinfo->ctx = ctx;
    tinfo->func = argv[0];
    tinfo->this_obj = this_val;
    tinfo->argc = argc - 1;
    tinfo->argv = (JSValue *)malloc(sizeof(JSValue) * tinfo->argc);
    memcpy(tinfo->argv, argv + 1, sizeof(JSValue) * tinfo->argc);

	int task = create_task("Athena: Task", worker_thread, 16000, 16);
	init_task(task, tinfo);

    if (task < 0) {
        return JS_ThrowInternalError(ctx, "Failed to create thread");
    }

    // Retorna imediatamente
    return JS_UNDEFINED;
}

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
    JS_CFUNC_DEF("new", -1, athena_newtask),
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