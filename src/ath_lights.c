#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/render.h"
#include "ath_env.h"

static JSValue athena_newlight(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	return JS_NewUint32(ctx, NewLight());
}

static JSValue athena_setlight(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 5) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	float x, y, z;
	int id, attr;

	JS_ToInt32(ctx, &id, argv[0]);
	JS_ToInt32(ctx, &attr, argv[1]);
	JS_ToFloat32(ctx, &x, argv[2]);
	JS_ToFloat32(ctx, &y, argv[3]);
	JS_ToFloat32(ctx, &z, argv[4]);
	

	SetLightAttribute(id, x, y, z, attr);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry light_funcs[] = {
  JS_CFUNC_DEF( "new",  	0, athena_newlight),
  JS_CFUNC_DEF( "set",  	5, athena_setlight),
  JS_PROP_INT32_DEF("AMBIENT", ATHENA_LIGHT_AMBIENT, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF("DIFFUSE", ATHENA_LIGHT_DIFFUSE, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF("SPECULAR", ATHENA_LIGHT_SPECULAR, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF("DIRECTION", ATHENA_LIGHT_DIRECTION, JS_PROP_CONFIGURABLE ),
};

static int light_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, light_funcs, countof(light_funcs));
}

JSModuleDef *athena_lights_init(JSContext* ctx){
	return athena_push_module(ctx, light_init, light_funcs, countof(light_funcs), "Lights");

}
