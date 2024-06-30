#include "ath_env.h"
#include "include/sound.h"

static JSValue athena_setvolume(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int slot, volume;

	JS_ToInt32(ctx, &volume, argv[0]);

	if(argc == 2) {
		JS_ToInt32(ctx, &slot, argv[1]);
		sound_setadpcmvolume(slot, volume);
	} else {
		sound_setvolume(volume);
	}
	
	return JS_UNDEFINED;
}

static JSValue athena_load(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Sound* snd;
	const char* path = JS_ToCString(ctx, argv[0]);

	FILE* f = fopen(path, "rb");
	uint32_t magic;
	fread(&magic, 1, 4, f);	
	fclose(f);

	switch (magic) {
		case 0x5367674F: /* OGG */
			snd = load_ogg(path);
			break;
		case 0x46464952: /* WAV */
			snd = load_wav(path);
			break;
		case 0x4D435041: /* ADPCM (with header) */  
			snd = malloc(sizeof(Sound));
			snd->fp = sound_loadadpcm(path);
			snd->type = ADPCM_AUDIO;
			break;
		default:
			return JS_UNDEFINED;
	}

	return JS_NewUint32(ctx, snd);
}

static JSValue athena_play(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int slot = 0;
	Sound* snd;

	JS_ToUint32(ctx, &snd, argv[0]);

	switch (snd->type) {
		case WAV_AUDIO:
			play_wav(snd);
			break;
		case OGG_AUDIO:
			play_ogg(snd);
			break;
		case ADPCM_AUDIO:
			if (argc == 2)
				JS_ToInt32(ctx, &slot, argv[1]);
			sound_playadpcm(slot, snd->fp);
			break;
	}
	
	return JS_UNDEFINED;
}

static JSValue athena_free(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Sound* snd;
	JS_ToUint32(ctx, &snd, argv[0]);
	sound_free(snd);
	return JS_UNDEFINED;
}

static JSValue athena_deinit(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	sound_deinit();
	return JS_UNDEFINED;
}

static JSValue athena_isplaying(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	return JS_NewBool(ctx, is_sound_playing());
}

static JSValue athena_get_duration(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Sound* snd;

	JS_ToUint32(ctx, &snd, argv[0]);
	return JS_NewInt32(ctx, sound_get_duration(snd));
}

static JSValue athena_repeat(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	set_sound_repeat(JS_ToBool(ctx, argv[0]));
	return JS_UNDEFINED;
}

static JSValue athena_pause(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Sound* snd;
	JS_ToUint32(ctx, &snd, argv[0]);
	if (snd->type != ADPCM_AUDIO)
		sound_pause();
	return JS_UNDEFINED;
}

static JSValue athena_resume(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Sound* snd;
	JS_ToUint32(ctx, &snd, argv[0]);
	if (snd->type != ADPCM_AUDIO)
		sound_resume(snd);
	return JS_UNDEFINED;
}

static JSValue athena_restart(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	sound_restart();
	return JS_UNDEFINED;
}

static JSValue athena_setposition(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    Sound* snd;
    uint32_t position;
    JS_ToUint32(ctx, &snd, argv[0]);
    JS_ToUint32(ctx, &position, argv[1]);
    sound_set_position(snd, position);
    return JS_UNDEFINED;
}

static JSValue athena_getposition(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    Sound* snd;
    JS_ToUint32(ctx, &snd, argv[0]);
    return JS_NewInt32(ctx, sound_get_position(snd));
}

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("setVolume", 2, athena_setvolume),
	JS_CFUNC_DEF("load", 1, athena_load),
	JS_CFUNC_DEF("play", 2, athena_play),
	JS_CFUNC_DEF("free", 1, athena_free),
	JS_CFUNC_DEF("deinit", 0, athena_deinit),
	JS_CFUNC_DEF("isPlaying", 0, athena_isplaying),
	JS_CFUNC_DEF("getDuration", 1, athena_get_duration),
	JS_CFUNC_DEF("repeat", 1, athena_repeat),
	JS_CFUNC_DEF("pause", 1, athena_pause),
	JS_CFUNC_DEF("resume", 1, athena_resume),
	JS_CFUNC_DEF("restart", 0, athena_restart),
	JS_CFUNC_DEF("setPosition", 2, athena_setposition),
	JS_CFUNC_DEF("getPosition", 1, athena_getposition),
};

static int module_init(JSContext *ctx, JSModuleDef *m){
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_sound_init(JSContext* ctx){
	return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Sound");

}