#include <ath_env.h>
#include <athena/lights.h>

static JSValue js_newlight(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	return JS_NewUint32(ctx, athena_lights_new());
}

static JSValue js_setlight(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 5) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	float x, y, z;
	int id, attr;

	JS_ToInt32(ctx, &id, argv[0]);
	JS_ToInt32(ctx, &attr, argv[1]);
	JS_ToFloat32(ctx, &x, argv[2]);
	JS_ToFloat32(ctx, &y, argv[3]);
	JS_ToFloat32(ctx, &z, argv[4]);

	athena_lights_set(id, attr, x, y, z);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry light_funcs[] = {
  JS_CFUNC_DEF( "new",  	0, js_newlight),
  JS_CFUNC_DEF( "set",  	5, js_setlight),
  JS_PROP_INT32_DEF("AMBIENT", ATHENA_LIGHT_AMBIENT, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF("DIFFUSE", ATHENA_LIGHT_DIFFUSE, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF("SPECULAR", ATHENA_LIGHT_SPECULAR, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF("DIRECTION", ATHENA_LIGHT_DIRECTION, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF("POSITION", ATHENA_LIGHT_POSITION, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF("ATTENUATION", ATHENA_LIGHT_ATTENUATION, JS_PROP_CONFIGURABLE ),
};

static int js_lights_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, light_funcs, countof(light_funcs));
}

JSModuleDef *athena_lights_init(JSContext* ctx){
	return athena_push_module(ctx, js_lights_init, light_funcs, countof(light_funcs), "Lights");
}
