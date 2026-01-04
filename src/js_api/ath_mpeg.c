/**
 * @file ath_mpeg.c
 * @brief JavaScript Video class bindings for MPEG playback
 */

#include <stdlib.h>
#include <string.h>

#include <ath_env.h>
#include <graphics.h>
#include <mpeg_player.h>

static JSClassID js_video_class_id;

// Get Video class ID for external use
JSClassID get_video_class_id(void) {
    return js_video_class_id;
}

// Video class destructor
static void athena_video_dtor(JSRuntime *rt, JSValue val) {
    MPEGPlayer* player = JS_GetOpaque(val, js_video_class_id);
    if (player) {
        mpeg_player_destroy(player);
    }
}

// Video constructor: new Video(path)
static JSValue athena_video_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Video constructor requires path argument");
    }
    
    const char* path = JS_ToCString(ctx, argv[0]);
    if (!path) {
        return JS_EXCEPTION;
    }
    
    MPEGPlayer* player = mpeg_player_create(path);
    JS_FreeCString(ctx, path);
    
    if (!player) {
        return JS_ThrowInternalError(ctx, "Failed to create MPEG player");
    }
    
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto)) {
        mpeg_player_destroy(player);
        return proto;
    }
    
    JSValue obj = JS_NewObjectProtoClass(ctx, proto, js_video_class_id);
    JS_FreeValue(ctx, proto);
    
    if (JS_IsException(obj)) {
        mpeg_player_destroy(player);
        return obj;
    }
    
    JS_SetOpaque(obj, player);
    
    return obj;
}

// Video.play()
static JSValue athena_video_play(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    
    mpeg_player_play(player);
    return JS_UNDEFINED;
}

// Video.pause()
static JSValue athena_video_pause(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    
    mpeg_player_pause(player);
    return JS_UNDEFINED;
}

// Video.stop()
static JSValue athena_video_stop(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    
    mpeg_player_stop(player);
    return JS_UNDEFINED;
}

// Video.update() - call each frame for automatic frame timing
static JSValue athena_video_update(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    
    bool new_frame = mpeg_player_update(player);
    return JS_NewBool(ctx, new_frame);
}

// Video.draw(x, y, [w, [h]])
static JSValue athena_video_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    
    double x = 0, y = 0, w = 0, h = 0;
    
    if (argc >= 1) JS_ToFloat64(ctx, &x, argv[0]);
    if (argc >= 2) JS_ToFloat64(ctx, &y, argv[1]);
    if (argc >= 3) JS_ToFloat64(ctx, &w, argv[2]);
    if (argc >= 4) JS_ToFloat64(ctx, &h, argv[3]);
    
    mpeg_player_draw(player, (float)x, (float)y, (float)w, (float)h);
    
    return JS_UNDEFINED;
}

// Video.free()
static JSValue athena_video_free(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (player) {
        mpeg_player_destroy(player);
        JS_SetOpaque(this_val, NULL);
    }
    return JS_UNDEFINED;
}

// Property getter for video.width
static JSValue athena_video_get_width(JSContext *ctx, JSValueConst this_val) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    return JS_NewInt32(ctx, mpeg_player_get_width(player));
}

// Property getter for video.height
static JSValue athena_video_get_height(JSContext *ctx, JSValueConst this_val) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    return JS_NewInt32(ctx, mpeg_player_get_height(player));
}

// Property getter for video.fps
static JSValue athena_video_get_fps(JSContext *ctx, JSValueConst this_val) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    return JS_NewFloat64(ctx, mpeg_player_get_fps(player));
}

// Property getter for video.ready
static JSValue athena_video_get_ready(JSContext *ctx, JSValueConst this_val) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    return JS_NewBool(ctx, mpeg_player_is_ready(player));
}

// Property getter for video.ended
static JSValue athena_video_get_ended(JSContext *ctx, JSValueConst this_val) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    return JS_NewBool(ctx, mpeg_player_is_ended(player));
}

// Property getter for video.playing
static JSValue athena_video_get_playing(JSContext *ctx, JSValueConst this_val) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    return JS_NewBool(ctx, mpeg_player_get_state(player) == MPEG_STATE_PLAYING);
}

// Property getter/setter for video.loop
static JSValue athena_video_get_loop(JSContext *ctx, JSValueConst this_val) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    return JS_NewBool(ctx, player->loop);
}

static JSValue athena_video_set_loop(JSContext *ctx, JSValueConst this_val, JSValue val) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    player->loop = JS_ToBool(ctx, val);
    return JS_UNDEFINED;
}

// Property getter for video.frame (internal Image reference)
static JSValue athena_video_get_frame(JSContext *ctx, JSValueConst this_val) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    
    GSSURFACE* texture = mpeg_player_get_texture(player);
    if (!texture) {
        return JS_NULL;
    }
    
    // Create a wrapper Image object that references the internal texture
    // This uses the existing JSImageData structure from ath_env.h
    JSClassID img_class_id = get_img_class_id();
    
    // Create JSImageData wrapper using the existing structure
    // Note: JSImageData has tex field (GSSURFACE*) per ath_env.h
    JSImageData* img_data = malloc(sizeof(JSImageData));
    if (!img_data) {
        return JS_ThrowOutOfMemory(ctx);
    }
    
    // Initialize the wrapper - it references the video's texture
    img_data->path = NULL;
    img_data->tex = texture;
    img_data->delayed = false;
    img_data->loaded = true;
    img_data->color = 0x80808080;  // White with full alpha
    img_data->width = (float)player->width;
    img_data->height = (float)player->height;
    img_data->startx = 0;
    img_data->starty = 0;
    img_data->endx = (float)player->width;
    img_data->endy = (float)player->height;
    img_data->angle = 0;
    
    JSValue img_obj = JS_NewObjectClass(ctx, img_class_id);
    
    if (JS_IsException(img_obj)) {
        free(img_data);
        return img_obj;
    }
    
    JS_SetOpaque(img_obj, img_data);
    
    return img_obj;
}

// Property getter for video.currentFrame
static JSValue athena_video_get_current_frame(JSContext *ctx, JSValueConst this_val) {
    MPEGPlayer* player = JS_GetOpaque2(ctx, this_val, js_video_class_id);
    if (!player) return JS_EXCEPTION;
    return JS_NewInt32(ctx, player->current_frame);
}

// Class definition
static JSClassDef js_video_class = {
    "Video",
    .finalizer = athena_video_dtor,
};

// Method list
static const JSCFunctionListEntry js_video_proto_funcs[] = {
    JS_CFUNC_DEF("play", 0, athena_video_play),
    JS_CFUNC_DEF("pause", 0, athena_video_pause),
    JS_CFUNC_DEF("stop", 0, athena_video_stop),
    JS_CFUNC_DEF("update", 0, athena_video_update),
    JS_CFUNC_DEF("draw", 4, athena_video_draw),
    JS_CFUNC_DEF("free", 0, athena_video_free),
    
    JS_CGETSET_DEF("width", athena_video_get_width, NULL),
    JS_CGETSET_DEF("height", athena_video_get_height, NULL),
    JS_CGETSET_DEF("fps", athena_video_get_fps, NULL),
    JS_CGETSET_DEF("ready", athena_video_get_ready, NULL),
    JS_CGETSET_DEF("ended", athena_video_get_ended, NULL),
    JS_CGETSET_DEF("playing", athena_video_get_playing, NULL),
    JS_CGETSET_DEF("loop", athena_video_get_loop, athena_video_set_loop),
    JS_CGETSET_DEF("frame", athena_video_get_frame, NULL),
    JS_CGETSET_DEF("currentFrame", athena_video_get_current_frame, NULL),
};

// Module initialization
static int video_init(JSContext *ctx, JSModuleDef *m) {
    JSValue proto, class_obj;
    
    JS_NewClassID(&js_video_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_video_class_id, &js_video_class);
    
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_video_proto_funcs, 
                               sizeof(js_video_proto_funcs) / sizeof(js_video_proto_funcs[0]));
    
    class_obj = JS_NewCFunction2(ctx, athena_video_ctor, "Video", 1, 
                                  JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, class_obj, proto);
    JS_SetClassProto(ctx, js_video_class_id, proto);
    
    JS_SetModuleExport(ctx, m, "Video", class_obj);
    
    return 0;
}

// Module definition function
JSModuleDef* athena_mpeg_init(JSContext* ctx) {
    JSModuleDef* m = JS_NewCModule(ctx, "Video", video_init);
    if (!m) return NULL;
    
    JS_AddModuleExport(ctx, m, "Video");
    
    return m;
}
