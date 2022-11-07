#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include "ath_env.h"

// if the timer is running:
// measuredTime is the value of the last call to getCurrentMilliseconds
// offset is the value of startTime
//
// if the timer is stopped:
// measuredTime is 0
// offset is the value time() returns on stopped timers

typedef struct{
	bool isPlaying;
	clock_t tick;
} Timer;

duk_ret_t athena_newT(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 0) return duk_generic_error(ctx, "wrong number of arguments");
	Timer* new_timer = (Timer*)malloc(sizeof(Timer));
	new_timer->tick = clock();
	new_timer->isPlaying = true;
	duk_push_uint(ctx, (uint32_t)new_timer);
	return 1;
}

duk_ret_t athena_time(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments");
	Timer* src = (Timer*)duk_get_uint(ctx, 0);
	if (src->isPlaying){
		duk_push_int(ctx, (clock() - src->tick));
	}else{
		duk_push_int(ctx, src->tick);
	}
	return 1;
}

duk_ret_t athena_pause(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments");
	Timer* src = (Timer*)duk_get_uint(ctx, 0);
	if (src->isPlaying){
		src->isPlaying = false;
		src->tick = (clock()-src->tick);
	}
	return 0;
}

duk_ret_t athena_resume(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments");
	Timer* src = (Timer*)duk_get_uint(ctx, 0);
	if (!src->isPlaying){
		src->isPlaying = true;
		src->tick = (clock()-src->tick);
	}
	return 0;
}

duk_ret_t athena_reset(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments");
	Timer* src = (Timer*)duk_get_uint(ctx, 0);
	if (src->isPlaying) src->tick = clock();
	else src->tick = 0;
	return 0;
}

duk_ret_t athena_set(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 2) return duk_generic_error(ctx, "wrong number of arguments");
	Timer* src = (Timer*)duk_get_uint(ctx, 0);
	uint32_t val = (uint32_t)duk_get_int(ctx, 1);
	if (src->isPlaying) src->tick = clock() + val;
	else src->tick = val;
	return 0;
}

duk_ret_t athena_wisPlaying(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments");
	Timer* src = (Timer*)duk_get_uint(ctx, 0);
	duk_push_boolean(ctx, src->isPlaying);
	return 1;
}

duk_ret_t athena_destroy(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments");
	Timer* timer = (Timer*)duk_get_uint(ctx, 0);
	free(timer);
	return 1;
}

DUK_EXTERNAL duk_ret_t dukopen_timer(duk_context *ctx) {
  const duk_function_list_entry module_funcs[] = {
      { "new",        	    athena_newT,            DUK_VARARGS },
      { "getTime",    		athena_time,            DUK_VARARGS },
      { "setTime",    		athena_set,             DUK_VARARGS },
      { "destroy",    	    athena_destroy,         DUK_VARARGS },
      { "pause",      		athena_pause,           DUK_VARARGS },
      { "resume",     		athena_resume,          DUK_VARARGS },
      { "reset",      		athena_reset,           DUK_VARARGS },
      { "isPlaying",  	    athena_wisPlaying,      DUK_VARARGS },
      {NULL, NULL, 0}
    };

  duk_push_object(ctx);  /* module result */
  duk_put_function_list(ctx, -1, module_funcs);

  return 1;  /* return module value */
}

void athena_timer_init(duk_context* ctx){
	push_athena_module(dukopen_timer,   	  "Timer");
}