#include <stdlib.h>
#include <string.h>

#include <athena/image.h>
#include <athena/video.h>
#include <ath_bindings.h>
#include <graphics.h>
#include <mpeg_player.h>

static JSClassID js_video_class_id;

typedef struct {
    AthenaVideo *video;
} JSVideoHolder;

JSClassID get_video_class_id(void) {
    return js_video_class_id;
}

static void js_video_dtor(JSRuntime *rt, JSValue val) {
    JSVideoHolder *holder = JS_GetOpaque(val, js_video_class_id);
    if (holder) {
        athena_video_destroy(holder->video);
        js_free_rt(rt, holder);
    }
}

static JSValue js_video_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSVideoHolder *holder;
    const char* path;
    JSValue proto;
    JSValue obj;

    if (argc < 1)
        return JS_ThrowTypeError(ctx, "Video constructor requires path argument");

    path = JS_ToCString(ctx, argv[0]);
    if (!path)
        return JS_EXCEPTION;

    holder = js_mallocz(ctx, sizeof(*holder));
    if (!holder) {
        JS_FreeCString(ctx, path);
        return JS_EXCEPTION;
    }

    holder->video = athena_video_create(path);
    JS_FreeCString(ctx, path);

    if (!holder->video) {
        js_free(ctx, holder);
        return JS_ThrowInternalError(ctx, "Failed to create MPEG player");
    }

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto)) {
        athena_video_destroy(holder->video);
        js_free(ctx, holder);
        return proto;
    }

    obj = JS_NewObjectProtoClass(ctx, proto, js_video_class_id);
    JS_FreeValue(ctx, proto);

    if (JS_IsException(obj)) {
        athena_video_destroy(holder->video);
        js_free(ctx, holder);
        return obj;
    }

    JS_SetOpaque(obj, holder);
    return obj;
}

static JSValue js_video_play(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!holder) return JS_EXCEPTION;
    athena_video_play(holder->video);
    return JS_UNDEFINED;
}

static JSValue js_video_pause(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!holder) return JS_EXCEPTION;
    athena_video_pause(holder->video);
    return JS_UNDEFINED;
}

static JSValue js_video_stop(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!holder) return JS_EXCEPTION;
    athena_video_stop(holder->video);
    return JS_UNDEFINED;
}

static JSValue js_video_update(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!holder) return JS_EXCEPTION;
    return JS_NewBool(ctx, athena_video_update(holder->video));
}

static JSValue js_video_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    double x = 0, y = 0, w = 0, h = 0;

    if (!holder) return JS_EXCEPTION;

    if (argc >= 1) JS_ToFloat64(ctx, &x, argv[0]);
    if (argc >= 2) JS_ToFloat64(ctx, &y, argv[1]);
    if (argc >= 3) JS_ToFloat64(ctx, &w, argv[2]);
    if (argc >= 4) JS_ToFloat64(ctx, &h, argv[3]);

    athena_video_draw(holder->video, (float)x, (float)y, (float)w, (float)h);
    return JS_UNDEFINED;
}

static JSValue js_video_free(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    js_video_dtor(JS_GetRuntime(ctx), this_val);
    JS_SetOpaque(this_val, NULL);
    return JS_UNDEFINED;
}

static JSValue js_video_get_width(JSContext *ctx, JSValueConst this_val) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!holder) return JS_EXCEPTION;
    return JS_NewInt32(ctx, athena_video_get_width(holder->video));
}

static JSValue js_video_get_height(JSContext *ctx, JSValueConst this_val) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!holder) return JS_EXCEPTION;
    return JS_NewInt32(ctx, athena_video_get_height(holder->video));
}

static JSValue js_video_get_fps(JSContext *ctx, JSValueConst this_val) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!holder) return JS_EXCEPTION;
    return JS_NewFloat64(ctx, athena_video_get_fps(holder->video));
}

static JSValue js_video_get_ready(JSContext *ctx, JSValueConst this_val) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!holder) return JS_EXCEPTION;
    return JS_NewBool(ctx, athena_video_is_ready(holder->video));
}

static JSValue js_video_get_ended(JSContext *ctx, JSValueConst this_val) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!holder) return JS_EXCEPTION;
    return JS_NewBool(ctx, athena_video_is_ended(holder->video));
}

static JSValue js_video_get_playing(JSContext *ctx, JSValueConst this_val) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!holder) return JS_EXCEPTION;
    return JS_NewBool(ctx, athena_video_is_playing(holder->video));
}

static JSValue js_video_get_loop(JSContext *ctx, JSValueConst this_val) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!holder) return JS_EXCEPTION;
    return JS_NewBool(ctx, athena_video_get_loop(holder->video));
}

static JSValue js_video_set_loop(JSContext *ctx, JSValueConst this_val, JSValue val) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!holder) return JS_EXCEPTION;
    athena_video_set_loop(holder->video, JS_ToBool(ctx, val));
    return JS_UNDEFINED;
}

static JSValue js_video_get_frame(JSContext *ctx, JSValueConst this_val) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    GSSURFACE* texture;
    AthenaImage *img_data;
    JSValue img_obj;
    int width;
    int height;

    if (!holder) return JS_EXCEPTION;

    texture = athena_video_get_texture(holder->video);
    if (!texture)
        return JS_NULL;

    width = athena_video_get_width(holder->video);
    height = athena_video_get_height(holder->video);
    img_data = athena_image_wrap(texture, false);
    if (!img_data)
        return JS_ThrowOutOfMemory(ctx);

    img_data->width = (float)width;
    img_data->height = (float)height;
    img_data->endx = (float)width;
    img_data->endy = (float)height;

    img_obj = JS_NewObjectClass(ctx, get_img_class_id());
    if (JS_IsException(img_obj)) {
        athena_image_destroy(img_data);
        return img_obj;
    }

    JS_SetOpaque(img_obj, img_data);
    return img_obj;
}

static JSValue js_video_get_current_frame(JSContext *ctx, JSValueConst this_val) {
    JSVideoHolder* holder = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!holder) return JS_EXCEPTION;
    return JS_NewInt32(ctx, athena_video_get_current_frame(holder->video));
}

static JSClassDef js_video_class = {
    "Video",
    .finalizer = js_video_dtor,
};

static const JSCFunctionListEntry js_video_proto_funcs[] = {
    JS_CFUNC_DEF("play", 0, js_video_play),
    JS_CFUNC_DEF("pause", 0, js_video_pause),
    JS_CFUNC_DEF("stop", 0, js_video_stop),
    JS_CFUNC_DEF("update", 0, js_video_update),
    JS_CFUNC_DEF("draw", 4, js_video_draw),
    JS_CFUNC_DEF("free", 0, js_video_free),
    JS_CGETSET_DEF("width", js_video_get_width, NULL),
    JS_CGETSET_DEF("height", js_video_get_height, NULL),
    JS_CGETSET_DEF("fps", js_video_get_fps, NULL),
    JS_CGETSET_DEF("ready", js_video_get_ready, NULL),
    JS_CGETSET_DEF("ended", js_video_get_ended, NULL),
    JS_CGETSET_DEF("playing", js_video_get_playing, NULL),
    JS_CGETSET_DEF("loop", js_video_get_loop, js_video_set_loop),
    JS_CGETSET_DEF("frame", js_video_get_frame, NULL),
    JS_CGETSET_DEF("currentFrame", js_video_get_current_frame, NULL),
};

static int js_video_init(JSContext *ctx, JSModuleDef *m) {
    JSValue proto, class_obj;

    if (!js_video_class_id)
        JS_NewClassID(&js_video_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_video_class_id, &js_video_class);

    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_video_proto_funcs,
                               sizeof(js_video_proto_funcs) / sizeof(js_video_proto_funcs[0]));

    class_obj = JS_NewCFunction2(ctx, js_video_ctor, "Video", 1,
                                  JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, class_obj, proto);
    JS_SetClassProto(ctx, js_video_class_id, proto);

#ifdef DYNAMIC_ATHENA_MPEG
    JS_SetModuleExport(ctx, m, "Video", JS_DupValue(ctx, class_obj));
    JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, class_obj));
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, "Video", class_obj);
    JS_FreeValue(ctx, global_obj);
#else
    JS_SetModuleExport(ctx, m, "Video", class_obj);
#endif
    return 0;
}

JSModuleDef* athena_mpeg_init(JSContext* ctx) {
    JSModuleDef* m = JS_NewCModule(ctx, "Video", js_video_init);
    if (!m) return NULL;
    JS_AddModuleExport(ctx, m, "Video");
    return m;
}

#ifdef DYNAMIC_ATHENA_MPEG
char *erl_id = "athena_mpeg";

JSModuleDef *js_init_module(JSContext *ctx) {
    JSModuleDef *m = JS_NewCModule(ctx, "Video", js_video_init);
    if (!m) return NULL;
    JS_AddModuleExport(ctx, m, "Video");
    JS_AddModuleExport(ctx, m, "default");
    return m;
}
#endif
