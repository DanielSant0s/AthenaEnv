#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <render.h>
#include <ath_env.h>

static JSValue athena_camsave(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    athena_camera_state state;
    cameraSave(&state);

    JSValue obj = JS_NewObject(ctx);

    JSValue pos = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, pos, "x", JS_NewFloat32(ctx, state.position[0]));
    JS_SetPropertyStr(ctx, pos, "y", JS_NewFloat32(ctx, state.position[1]));
    JS_SetPropertyStr(ctx, pos, "z", JS_NewFloat32(ctx, state.position[2]));
    JS_SetPropertyStr(ctx, obj, "position", pos);

    JSValue tgt = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, tgt, "x", JS_NewFloat32(ctx, state.target[0]));
    JS_SetPropertyStr(ctx, tgt, "y", JS_NewFloat32(ctx, state.target[1]));
    JS_SetPropertyStr(ctx, tgt, "z", JS_NewFloat32(ctx, state.target[2]));
    JS_SetPropertyStr(ctx, obj, "target", tgt);

    JSValue up = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, up, "x", JS_NewFloat32(ctx, state.up[0]));
    JS_SetPropertyStr(ctx, up, "y", JS_NewFloat32(ctx, state.up[1]));
    JS_SetPropertyStr(ctx, up, "z", JS_NewFloat32(ctx, state.up[2]));
    JS_SetPropertyStr(ctx, obj, "up", up);

    JSValue lup = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, lup, "x", JS_NewFloat32(ctx, state.local_up[0]));
    JS_SetPropertyStr(ctx, lup, "y", JS_NewFloat32(ctx, state.local_up[1]));
    JS_SetPropertyStr(ctx, lup, "z", JS_NewFloat32(ctx, state.local_up[2]));
    JS_SetPropertyStr(ctx, obj, "local_up", lup);

    return obj;
}

static JSValue athena_camrestore(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "expected state object");
    JSValue state_obj = argv[0];
    athena_camera_state state;

    JSValue pos = JS_GetPropertyStr(ctx, state_obj, "position");
    JS_ToFloat32(ctx, &state.position[0], JS_GetPropertyStr(ctx, pos, "x"));
    JS_ToFloat32(ctx, &state.position[1], JS_GetPropertyStr(ctx, pos, "y"));
    JS_ToFloat32(ctx, &state.position[2], JS_GetPropertyStr(ctx, pos, "z"));
    state.position[3] = 0.0f;
    JS_FreeValue(ctx, pos);

    JSValue tgt = JS_GetPropertyStr(ctx, state_obj, "target");
    JS_ToFloat32(ctx, &state.target[0], JS_GetPropertyStr(ctx, tgt, "x"));
    JS_ToFloat32(ctx, &state.target[1], JS_GetPropertyStr(ctx, tgt, "y"));
    JS_ToFloat32(ctx, &state.target[2], JS_GetPropertyStr(ctx, tgt, "z"));
    state.target[3] = 0.0f;
    JS_FreeValue(ctx, tgt);

    JSValue up = JS_GetPropertyStr(ctx, state_obj, "up");
    if (!JS_IsUndefined(up) && !JS_IsNull(up)) {
        JS_ToFloat32(ctx, &state.up[0], JS_GetPropertyStr(ctx, up, "x"));
        JS_ToFloat32(ctx, &state.up[1], JS_GetPropertyStr(ctx, up, "y"));
        JS_ToFloat32(ctx, &state.up[2], JS_GetPropertyStr(ctx, up, "z"));
        state.up[3] = 0.0f;
    } else {
        state.up[0] = 0.0f; state.up[1] = 1.0f; state.up[2] = 0.0f; state.up[3] = 0.0f;
    }
    JS_FreeValue(ctx, up);

    JSValue lup = JS_GetPropertyStr(ctx, state_obj, "local_up");
    if (!JS_IsUndefined(lup) && !JS_IsNull(lup)) {
        JS_ToFloat32(ctx, &state.local_up[0], JS_GetPropertyStr(ctx, lup, "x"));
        JS_ToFloat32(ctx, &state.local_up[1], JS_GetPropertyStr(ctx, lup, "y"));
        JS_ToFloat32(ctx, &state.local_up[2], JS_GetPropertyStr(ctx, lup, "z"));
        state.local_up[3] = 1.0f;
    } else {
        state.local_up[0] = 0.0f; state.local_up[1] = 1.0f; state.local_up[2] = 0.0f; state.local_up[3] = 1.0f;
    }
    JS_FreeValue(ctx, lup);

    cameraRestore(&state);
    return JS_UNDEFINED;
}
static JSValue athena_camposition(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 3) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	float x, y, z;
	JS_ToFloat32(ctx, &x, argv[0]);
	JS_ToFloat32(ctx, &y, argv[1]);
	JS_ToFloat32(ctx, &z, argv[2]);
	
	setCameraPosition(x, y, z);

	return JS_UNDEFINED;
}


static JSValue athena_camtarget(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 3) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	float x, y, z;
	JS_ToFloat32(ctx, &x, argv[0]);
	JS_ToFloat32(ctx, &y, argv[1]);
	JS_ToFloat32(ctx, &z, argv[2]);
	
	setCameraTarget(x, y, z);

	return JS_UNDEFINED;
}

static JSValue athena_camorbit(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	float yaw, pitch;
	JS_ToFloat32(ctx, &yaw, argv[0]);
	JS_ToFloat32(ctx, &pitch, argv[1]);
	
	orbitCamera(yaw, pitch);

	return JS_UNDEFINED;
}

static JSValue athena_camturn(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	float yaw, pitch;
	JS_ToFloat32(ctx, &yaw, argv[0]);
	JS_ToFloat32(ctx, &pitch, argv[1]);
	
	turnCamera(yaw, pitch);

	return JS_UNDEFINED;
}

static JSValue athena_campan(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	float x, y;
	JS_ToFloat32(ctx, &x, argv[0]);
	JS_ToFloat32(ctx, &y, argv[1]);
	
	panCamera(x, y);

	return JS_UNDEFINED;
}

static JSValue athena_camdolly(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	float dist;
	JS_ToFloat32(ctx, &dist, argv[0]);
	
	dollyCamera(dist);

	return JS_UNDEFINED;
}

static JSValue athena_camzoom(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	float dist;
	JS_ToFloat32(ctx, &dist, argv[0]);
	
	zoomCamera(dist);

	return JS_UNDEFINED;
}

static JSValue athena_camupdate(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	cameraUpdate();

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry camera_funcs[] = {
  JS_CFUNC_DEF("position", 3, athena_camposition),

  JS_CFUNC_DEF("target", 3, athena_camtarget),
  JS_CFUNC_DEF("orbit", 2, athena_camorbit),
  JS_CFUNC_DEF("turn", 2, athena_camturn),
  JS_CFUNC_DEF("dolly", 1, athena_camdolly),
  JS_CFUNC_DEF("zoom", 1, athena_camzoom),
  JS_CFUNC_DEF("pan", 2, athena_campan),

  JS_CFUNC_DEF("save", 0, athena_camsave),
  JS_CFUNC_DEF("restore", 1, athena_camrestore),

  JS_CFUNC_DEF("update", 0, athena_camupdate),
};

static int camera_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, camera_funcs, countof(camera_funcs));
}

JSModuleDef *athena_3dcamera_init(JSContext* ctx){
	return athena_push_module(ctx, camera_init, camera_funcs, countof(camera_funcs), "Camera");

}
