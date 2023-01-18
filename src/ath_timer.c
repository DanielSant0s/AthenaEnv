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

static JSValue athena_newT(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	Timer* new_timer = (Timer*)malloc(sizeof(Timer));
	new_timer->tick = clock();
	new_timer->isPlaying = true;
	
	return JS_NewUint32(ctx, (uint32_t)new_timer);
}

static JSValue athena_time(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	Timer* src;
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	JS_ToUint32(ctx, &src, argv[0]);

	if (src->isPlaying) return JS_NewInt32(ctx, (clock() - src->tick));
	return JS_NewInt32(ctx, src->tick);
}

static JSValue athena_pause(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Timer* src;
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	JS_ToUint32(ctx, &src, argv[0]);

	if (src->isPlaying){
		src->isPlaying = false;
		src->tick = (clock()-src->tick);
	}
	return 0;
}

static JSValue athena_resume(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Timer* src;
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	JS_ToUint32(ctx, &src, argv[0]);

	if (!src->isPlaying){
		src->isPlaying = true;
		src->tick = (clock()-src->tick);
	}
	return 0;
}

static JSValue athena_reset(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Timer* src;
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	JS_ToUint32(ctx, &src, argv[0]);

	if (src->isPlaying) src->tick = clock();
	else src->tick = 0;
	return 0;
}

static JSValue athena_set(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Timer* src; uint32_t val;
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	JS_ToUint32(ctx, &src, argv[0]);
	JS_ToUint32(ctx, &val, argv[1]);
	if (src->isPlaying) src->tick = clock() + val;
	else src->tick = val;
	return 0;
}

static JSValue athena_wisPlaying(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Timer* src;
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	JS_ToUint32(ctx, &src, argv[0]);

	return JS_NewBool(ctx, src->isPlaying);
}

static JSValue athena_destroy(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	Timer* src;
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	JS_ToUint32(ctx, &src, argv[0]);

	free(src);
	return 0;
}

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("new", 0, athena_newT),
	JS_CFUNC_DEF("getTime", 1, athena_time),
	JS_CFUNC_DEF("setTime", 2, athena_set),
	JS_CFUNC_DEF("destroy", 1, athena_destroy),
	JS_CFUNC_DEF("pause", 1, athena_pause),
	JS_CFUNC_DEF("resume", 1, athena_resume),
	JS_CFUNC_DEF("reset", 1, athena_reset),
	JS_CFUNC_DEF("isPlaying", 1, athena_wisPlaying)
};

static int module_init(JSContext *ctx, JSModuleDef *m){
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_timer_init(JSContext* ctx){
	return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Timer");

}