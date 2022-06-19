#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "ath_env.h"

duk_ret_t athena_killtask(duk_context *ctx) {
	int argc = duk_get_top(ctx);
  	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments.");

	kill_task(duk_get_int(ctx, 0));

	return 0;
}

duk_ret_t athena_gettasklist(duk_context *ctx) {
	int argc = duk_get_top(ctx);
  	if (argc != 0) return duk_generic_error(ctx, "wrong number of arguments.");
	
	Tasklist* tasks = get_tasklist();

	duk_idx_t arr_idx = duk_push_array(ctx);

	for(int i = 0; tasks->size > i; i++){
		duk_push_object(ctx);
        duk_push_string(ctx, tasks->list[i]->title);
		duk_put_prop_string(ctx, arr_idx+1, "name");
		duk_push_int(ctx, tasks->list[i]->status);
		duk_put_prop_string(ctx, arr_idx+1, "status");
		duk_put_prop_index(ctx, arr_idx, i);
	}

	return 1;
}

DUK_EXTERNAL duk_ret_t dukopen_task(duk_context *ctx) {

	const duk_function_list_entry module_funcs[] = {
	    { "get",           		athena_gettasklist,	DUK_VARARGS },
		{ "kill",           	athena_killtask,	DUK_VARARGS },
	  	{NULL, NULL, 0}
	};

    duk_push_object(ctx);  /* module result */
    duk_put_function_list(ctx, -1, module_funcs);

  	return 1;  /* return module value */
}

void athena_task_init(duk_context* ctx){
	push_athena_module(dukopen_task,   	 "Tasks");

}