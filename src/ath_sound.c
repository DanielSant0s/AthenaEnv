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

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("setFormat", 3, athena_setformat),
	JS_CFUNC_DEF("setVolume", 1, athena_setvolume),
	JS_CFUNC_DEF("setADPCMVolume", 2, athena_setadpcmvolume),
	JS_CFUNC_DEF("loadADPCM", 1, athena_loadadpcm),
	JS_CFUNC_DEF("playADPCM", 2, athena_playadpcm)
};

static int module_init(JSContext *ctx, JSModuleDef *m){
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_sound_init(JSContext* ctx){
	return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Sound");

}