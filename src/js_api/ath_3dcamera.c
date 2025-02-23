#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <render.h>
#include <ath_env.h>

static JSValue athena_camposition(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 3) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	float x, y, z;
	JS_ToFloat32(ctx, &x, argv[0]);
	JS_ToFloat32(ctx, &y, argv[1]);
	JS_ToFloat32(ctx, &z, argv[2]);
	
	setCameraPosition(x, y, z);

	return JS_UNDEFINED;
}


static JSValue athena_camrotation(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 3) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	float x, y, z;
	JS_ToFloat32(ctx, &x, argv[0]);
	JS_ToFloat32(ctx, &y, argv[1]);
	JS_ToFloat32(ctx, &z, argv[2]);
	
	setCameraRotation(x, y, z);

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

static JSValue athena_camtype(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	eCameraTypes type;

	JS_ToUint32(ctx, &type, argv[0]);

	setCameraType(type);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry camera_funcs[] = {
  JS_CFUNC_DEF("position", 3, athena_camposition),
  JS_CFUNC_DEF("rotation", 3, athena_camrotation),

  JS_CFUNC_DEF("target", 3, athena_camtarget),
  JS_CFUNC_DEF("orbit", 2, athena_camorbit),
  JS_CFUNC_DEF("turn", 2, athena_camturn),
  JS_CFUNC_DEF("dolly", 1, athena_camdolly),
  JS_CFUNC_DEF("zoom", 1, athena_camzoom),
  JS_CFUNC_DEF("pan", 2, athena_campan),

  JS_CFUNC_DEF("update", 0, athena_camupdate),
  JS_CFUNC_DEF("type", 1, athena_camtype),

  JS_PROP_INT32_DEF("DEFAULT", CAMERA_DEFAULT, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF("LOOKAT", CAMERA_LOOKAT, JS_PROP_CONFIGURABLE ),
};

static int camera_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, camera_funcs, countof(camera_funcs));
}

JSModuleDef *athena_3dcamera_init(JSContext* ctx){
	return athena_push_module(ctx, camera_init, camera_funcs, countof(camera_funcs), "Camera");

}
