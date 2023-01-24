#include "ath_env.h"
#include "include/sound.h"

static JSValue athena_setformat(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int bits, freq, channels;
	if (argc != 3) return JS_ThrowSyntaxError(ctx, "setFormat takes 3 arguments");

	JS_ToInt32(ctx, &bits, argv[0]);
	JS_ToInt32(ctx, &freq, argv[1]);
	JS_ToInt32(ctx, &channels, argv[2]);

	sound_setformat(bits, freq, channels);
	return JS_UNDEFINED;
}

static JSValue athena_setvolume(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int volume;
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "setVolume takes only 1 argument");

	JS_ToInt32(ctx, &volume, argv[0]);

	sound_setvolume(volume);
	return JS_UNDEFINED;
}

static JSValue athena_setadpcmvolume(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int slot, volume;
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "setADPCMVolume takes 2 arguments");

	JS_ToInt32(ctx, &slot, argv[0]);
	JS_ToInt32(ctx, &volume, argv[1]);

	sound_setadpcmvolume(slot, volume);
	return JS_UNDEFINED;
}

static JSValue athena_loadadpcm(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "loadADPCM takes only 1 argument");
	return JS_NewUint32(ctx, (uint32_t*)sound_loadadpcm(JS_ToCString(ctx, argv[0])));
}

static JSValue athena_playadpcm(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int slot;
	uint32_t sample;
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "playADPCM takes 2 arguments");

	JS_ToInt32(ctx, &slot, argv[0]);
	JS_ToUint32(ctx, &sample, argv[1]);

	sound_playadpcm(slot, (audsrv_adpcm_t *)sample);
	return JS_UNDEFINED;
}

static JSValue athena_getadpcmduration(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	audsrv_adpcm_t * sample;

	JS_ToUint32(ctx, &sample, argv[0]);
	return JS_NewInt32(ctx, sound_get_adpcm_duration(sample));
}

static JSValue athena_freeadpcm(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	audsrv_adpcm_t * sample;

	JS_ToUint32(ctx, &sample, argv[0]);
	sound_freeadpcm(sample);
	return JS_UNDEFINED;
}

static JSValue athena_loadplay(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	ogg_load_play(JS_ToCString(ctx, argv[0]));
	return JS_UNDEFINED;
}

static JSValue athena_stop(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	ogg_unload_stop();
	return JS_UNDEFINED;
}

static JSValue athena_isplaying(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	return JS_NewBool(ctx, is_ogg_playing());
}

static JSValue athena_repeat(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	set_ogg_repeat(JS_ToBool(ctx, argv[0]));
	return JS_UNDEFINED;
}

static JSValue athena_pause(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	ogg_pause();
	return JS_UNDEFINED;
}

static JSValue athena_resume(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	ogg_resume();
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("setFormat", 3, athena_setformat),
	JS_CFUNC_DEF("setVolume", 1, athena_setvolume),
	JS_CFUNC_DEF("setADPCMVolume", 2, athena_setadpcmvolume),
	JS_CFUNC_DEF("loadADPCM", 1, athena_loadadpcm),
	JS_CFUNC_DEF("playADPCM", 2, athena_playadpcm),
	JS_CFUNC_DEF("getADPCMDuration", 1, athena_getadpcmduration),
	JS_CFUNC_DEF("freeADPCM", 1, athena_freeadpcm),

	JS_CFUNC_DEF("loadPlay", 1, athena_loadplay),
	JS_CFUNC_DEF("stop", 1, athena_stop),
	JS_CFUNC_DEF("isPlaying", 0, athena_isplaying),
	JS_CFUNC_DEF("repeat", 1, athena_repeat),
	JS_CFUNC_DEF("pause", 0, athena_pause),
	JS_CFUNC_DEF("resume", 0, athena_resume),
};

static int module_init(JSContext *ctx, JSModuleDef *m){
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_sound_init(JSContext* ctx){
	return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Sound");

}