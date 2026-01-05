#include <ath_env.h>
#include <sound.h>

static JSValue athena_set_volume(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int volume;

	JS_ToInt32(ctx, &volume, argv[0]);

	sound_setvolume(volume);
	
	return JS_UNDEFINED;
}

static JSValue athena_find_channel(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	return JS_NewInt32(ctx, sound_sfx_find_channel());
}

static JSClassID js_sound_stream_class_id;

typedef struct {
	SoundStream *sound;
} JSSoundStream;

static void athena_sound_stream_dtor(JSRuntime *rt, JSValue val) {
	JSSoundStream *snd = JS_GetOpaque(val, js_sound_stream_class_id);

	if (!snd)
		return;

	sound_free(snd->sound);

	JS_SetOpaque(val, NULL);
}

static JSValue athena_sound_stream_load(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSSoundStream* snd;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

    obj = JS_NewObjectClass(ctx, js_sound_stream_class_id);
	if (JS_IsException(obj))
        goto stream_fail;
		snd = js_mallocz(ctx, sizeof(*snd));
    if (!snd)
        return JS_EXCEPTION;

	const char* path = JS_ToCString(ctx, argv[0]);

	snd->sound = sound_load(path);

    JS_SetOpaque(obj, snd);
    return obj;

 stream_fail:
    js_free(ctx, snd);
    return JS_EXCEPTION;
}

static JSValue athena_sound_stream_play(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	JSSoundStream* snd = JS_GetOpaque2(ctx, this_val, js_sound_stream_class_id);

	sound_play(snd->sound);
	
	return JS_UNDEFINED;
}

static JSValue athena_sound_stream_free(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	athena_sound_stream_dtor(JS_GetRuntime(ctx), this_val);

	return JS_UNDEFINED;
}

static JSValue athena_is_playing(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSSoundStream* snd = JS_GetOpaque2(ctx, this_val, js_sound_stream_class_id);
	return JS_NewBool(ctx, is_sound_playing(snd->sound));
}

static JSValue athena_get_duration(JSContext *ctx, JSValueConst this_val){
	JSSoundStream* snd = JS_GetOpaque2(ctx, this_val, js_sound_stream_class_id);

	return JS_NewInt32(ctx, sound_get_duration(snd->sound));
}

static JSValue athena_sound_stream_pause(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSSoundStream* snd = JS_GetOpaque2(ctx, this_val, js_sound_stream_class_id);

	sound_pause();
	return JS_UNDEFINED;
}

static JSValue athena_rewind(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSSoundStream* snd = JS_GetOpaque2(ctx, this_val, js_sound_stream_class_id);

	sound_rewind(snd->sound);
	return JS_UNDEFINED;
}

static JSValue athena_set_position(JSContext *ctx, JSValueConst this_val, JSValue val){
    JSSoundStream* snd = JS_GetOpaque2(ctx, this_val, js_sound_stream_class_id);
    uint32_t position;

    JS_ToUint32(ctx, &position, val);
    sound_set_position(snd->sound, position);
    return JS_UNDEFINED;
}

static JSValue athena_get_position(JSContext *ctx, JSValueConst this_val){
    JSSoundStream* snd = JS_GetOpaque2(ctx, this_val, js_sound_stream_class_id);

    return JS_NewInt32(ctx, sound_get_position(snd->sound));
}

static JSValue athena_set_loop(JSContext *ctx, JSValueConst this_val, JSValue val){
    JSSoundStream* snd = JS_GetOpaque2(ctx, this_val, js_sound_stream_class_id);
    uint32_t position;

    snd->sound->loop = JS_ToBool(ctx, val);
    return JS_UNDEFINED;
}

static JSValue athena_get_loop(JSContext *ctx, JSValueConst this_val){
    JSSoundStream* snd = JS_GetOpaque2(ctx, this_val, js_sound_stream_class_id);

    return JS_NewBool(ctx, snd->sound->loop);
}

static const JSCFunctionListEntry js_sound_stream_proto_funcs[] = {
	JS_CFUNC_DEF("play", 0, athena_sound_stream_play),
	JS_CFUNC_DEF("free", 0, athena_sound_stream_free),
	JS_CFUNC_DEF("pause", 0, athena_sound_stream_pause),
	JS_CFUNC_DEF("playing", 0, athena_is_playing),
	JS_CFUNC_DEF("rewind", 0, athena_rewind),

	JS_CGETSET_DEF("loop", athena_get_loop, athena_set_loop),
	JS_CGETSET_DEF("position", athena_get_position, athena_set_position),
	JS_CGETSET_DEF("length", athena_get_duration, NULL),
};

static JSClassDef js_sound_stream_class = {
    "Stream",
    .finalizer = athena_sound_stream_dtor,
};

static JSClassID js_sound_sfx_class_id;

typedef struct {
	Sfx *sound;
} JSSoundEffect;

static void athena_sound_sfx_dtor(JSRuntime *rt, JSValue val) {
	JSSoundEffect *snd = JS_GetOpaque(val, js_sound_sfx_class_id);

	if (!snd)
		return;

	sound_sfx_free(snd->sound);

	JS_SetOpaque(val, NULL);
}

static JSValue athena_sound_sfx_load(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSSoundEffect* snd;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

    obj = JS_NewObjectClass(ctx, js_sound_sfx_class_id);
	if (JS_IsException(obj))
        goto sfx_fail;
		snd = js_mallocz(ctx, sizeof(*snd));
    if (!snd)
        return JS_EXCEPTION;

	const char* path = JS_ToCString(ctx, argv[0]);

	snd->sound = sound_sfx_load(path);

    JS_SetOpaque(obj, snd);
    return obj;

 sfx_fail:
    js_free(ctx, snd);
    return JS_EXCEPTION;
}

static JSValue athena_sound_sfx_play(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	JSSoundEffect* snd = JS_GetOpaque2(ctx, this_val, js_sound_sfx_class_id);
	int channel = -1;

	if (argc > 0)
		JS_ToInt32(ctx, &channel, argv[0]);

	int ret = sound_sfx_play(channel, snd->sound);
	
	return JS_NewInt32(ctx, ret);
}

static JSValue athena_sound_sfx_free(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	athena_sound_sfx_dtor(JS_GetRuntime(ctx), this_val);

	return JS_UNDEFINED;
}

static JSValue athena_sound_sfx_get_duration(JSContext *ctx, JSValueConst this_val){
	JSSoundEffect* snd = JS_GetOpaque2(ctx, this_val, js_sound_sfx_class_id);

	return JS_NewInt32(ctx, sound_sfx_length(snd->sound));
}

static JSValue athena_sound_sfx_is_playing(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	JSSoundEffect* snd = JS_GetOpaque2(ctx, this_val, js_sound_sfx_class_id);
	int channel = -1;

	JS_ToInt32(ctx, &channel, argv[0]);

	return JS_NewBool(ctx, sound_sfx_is_playing(snd->sound, channel));
}

static JSValue athena_sound_sfx_get_prop(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSSoundEffect *snd = JS_GetOpaque2(ctx, this_val, js_sound_sfx_class_id);
    if (!snd)
        return JS_EXCEPTION;

	switch (magic) {
		case 0:
			return JS_NewUint32(ctx, snd->sound->volume);
		case 1:
			return JS_NewUint32(ctx, snd->sound->pan);
		case 2:
			return JS_NewBool(ctx, snd->sound->sound.loop);
		case 3:
			return JS_NewInt32(ctx, sound_sfx_get_pitch(snd->sound));
	}

	return JS_UNDEFINED;
}

static JSValue athena_sound_sfx_set_prop(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    JSSoundEffect *snd = JS_GetOpaque2(ctx, this_val, js_sound_sfx_class_id);
    if (!snd)
        return JS_EXCEPTION;

    int value = 0;

    if (JS_ToInt32(ctx, &value, val))
        return JS_EXCEPTION;

	switch (magic) {
		case 0:
			snd->sound->volume = value;
			break;
		case 1:
			snd->sound->pan = value;
			break;
		case 2:
			snd->sound->sound.loop = value;
			break;
		case 3:
			sound_sfx_set_pitch(snd->sound, value);
			break;
	}

    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_sound_sfx_proto_funcs[] = {
	JS_CFUNC_DEF("play", 1, athena_sound_sfx_play),
	JS_CFUNC_DEF("free", 0, athena_sound_sfx_free),

	JS_CFUNC_DEF("playing", 1, athena_sound_sfx_is_playing),

	JS_CGETSET_DEF("length", athena_sound_sfx_get_duration, NULL),

	JS_CGETSET_MAGIC_DEF("volume", athena_sound_sfx_get_prop, athena_sound_sfx_set_prop, 0),
	JS_CGETSET_MAGIC_DEF("pan",    athena_sound_sfx_get_prop, athena_sound_sfx_set_prop, 1),
	JS_CGETSET_MAGIC_DEF("loop",   athena_sound_sfx_get_prop, athena_sound_sfx_set_prop, 2),
	JS_CGETSET_MAGIC_DEF("pitch",  athena_sound_sfx_get_prop, athena_sound_sfx_set_prop, 3),
};

static JSClassDef js_sound_sfx_class = {
    "Sfx",
    .finalizer = athena_sound_sfx_dtor,
};

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("setVolume",   1, athena_set_volume),
	JS_CFUNC_DEF("findChannel", 0, athena_find_channel),

	JS_CFUNC_SPECIAL_DEF("Stream", 1, constructor_or_func, athena_sound_stream_load),
	JS_CFUNC_SPECIAL_DEF("Sfx",    1, constructor_or_func, athena_sound_sfx_load),
};

static int module_init(JSContext *ctx, JSModuleDef *m) {
    JSValue proto;

    /* the class ID is created once */
    JS_NewClassID(&js_sound_stream_class_id);
    /* the class is created once per runtime */
    JS_NewClass(JS_GetRuntime(ctx), js_sound_stream_class_id, &js_sound_stream_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_sound_stream_proto_funcs,
                               countof(js_sound_stream_proto_funcs));
    JS_SetClassProto(ctx, js_sound_stream_class_id, proto);

    /* the class ID is created once */
    JS_NewClassID(&js_sound_sfx_class_id);
    /* the class is created once per runtime */
    JS_NewClass(JS_GetRuntime(ctx), js_sound_sfx_class_id, &js_sound_sfx_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_sound_sfx_proto_funcs,
                               countof(js_sound_sfx_proto_funcs));
    JS_SetClassProto(ctx, js_sound_sfx_class_id, proto);

    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_sound_init(JSContext* ctx){
	return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Sound");

}