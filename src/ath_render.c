#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/render.h"
#include "ath_env.h"

static JSValue athena_initrender(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
  	if (argc != 2 && argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments.");
	float aspect, fov = 0.2f;
	JS_ToFloat32(ctx, &aspect, argv[0]);
	if (argc == 2) {
		JS_ToFloat32(ctx, &fov, argv[1]);
	}
  	init3D(aspect, fov);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry render_funcs[] = {
    JS_CFUNC_DEF( "setView",     2,           athena_initrender),

	JS_PROP_INT32_DEF("PL_NO_LIGHTS_COLORS", PL_NO_LIGHTS_COLORS, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("PL_NO_LIGHTS_COLORS_TEX", PL_NO_LIGHTS_COLORS_TEX, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("PL_NO_LIGHTS", PL_NO_LIGHTS, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("PL_NO_LIGHTS_TEX", PL_NO_LIGHTS_TEX, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("PL_DEFAULT", PL_DEFAULT, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("PL_DEFAULT_NO_TEX", PL_DEFAULT_NO_TEX, JS_PROP_CONFIGURABLE ),
};

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
  JS_CFUNC_DEF( "set",  	5, athena_setlight),
  JS_PROP_INT32_DEF("AMBIENT", ATHENA_LIGHT_AMBIENT, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF("DIFFUSE", ATHENA_LIGHT_DIFFUSE, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF("DIRECTION", ATHENA_LIGHT_DIRECTION, JS_PROP_CONFIGURABLE ),
};


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

static const JSCFunctionListEntry camera_funcs[] = {
  JS_CFUNC_DEF("position", 3, athena_camposition),
  JS_CFUNC_DEF("rotation", 3, athena_camrotation),
};

static int render_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, render_funcs, countof(render_funcs));
}

static int light_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, light_funcs, countof(light_funcs));
}

static int camera_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, camera_funcs, countof(camera_funcs));
}

static JSClassID js_object_class_id;

static void athena_object_dtor(JSRuntime *rt, JSValue val){
	model* m = JS_GetOpaque(val, js_object_class_id);

	free(m->positions);
    free(m->colours);
    free(m->normals);
    free(m->texcoords);

	for (int i = 0; i < m->tex_count; i++) {
		UnloadTexture(&(m->textures[i]));

		free(m->textures[i]->Mem);
		m->textures[i]->Mem = NULL;

		if(m->textures[i]->Clut != NULL)
		{
			free(m->textures[i]->Clut);
			m->textures[i]->Clut = NULL;
		}
	}

	js_free_rt(rt, m);
}

static JSClassDef js_object_class = {
    "WavefrontObj",
    .finalizer = athena_object_dtor,
}; 

static JSValue athena_drawobject(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	float pos_x, pos_y, pos_z, rot_x, rot_y, rot_z;

	model* m = JS_GetOpaque2(ctx, this_val, js_object_class_id);

	JS_ToFloat32(ctx, &pos_x, argv[0]);
	JS_ToFloat32(ctx, &pos_y, argv[1]);
	JS_ToFloat32(ctx, &pos_z, argv[2]);
	JS_ToFloat32(ctx, &rot_x, argv[3]);
	JS_ToFloat32(ctx, &rot_y, argv[4]);
	JS_ToFloat32(ctx, &rot_z, argv[5]);
	
	m->render(m, pos_x, pos_y, pos_z, rot_x, rot_y, rot_z);

	return JS_UNDEFINED;
}

static JSValue athena_drawbbox(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	float pos_x, pos_y, pos_z, rot_x, rot_y, rot_z;
	Color color;

	model* m = JS_GetOpaque2(ctx, this_val, js_object_class_id);

	JS_ToFloat32(ctx, &pos_x, argv[0]);
	JS_ToFloat32(ctx, &pos_y, argv[1]);
	JS_ToFloat32(ctx, &pos_z, argv[2]);
	JS_ToFloat32(ctx, &rot_x, argv[3]);
	JS_ToFloat32(ctx, &rot_y, argv[4]);
	JS_ToFloat32(ctx, &rot_z, argv[5]);
	JS_ToUint32(ctx, &color,  argv[6]);
	
	draw_bbox(m, pos_x, pos_y, pos_z, rot_x, rot_y, rot_z, color);

	return JS_UNDEFINED;
}

static JSValue athena_setpipeline(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int pipeline;
	model* m = JS_GetOpaque2(ctx, this_val, js_object_class_id);

	JS_ToUint32(ctx, &pipeline, argv[0]);

	return JS_NewUint32(ctx, athena_render_set_pipeline(m, pipeline));
}

static JSValue athena_getpipeline(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int pipeline;
	model* m = JS_GetOpaque2(ctx, this_val, js_object_class_id);

	return JS_NewUint32(ctx, m->pipeline);
}

static const JSCFunctionListEntry js_object_proto_funcs[] = {
    JS_CFUNC_DEF("draw", 6, athena_drawobject),
	JS_CFUNC_DEF("drawBounds", 7, athena_drawbbox),
	JS_CFUNC_DEF("setPipeline", 1, athena_setpipeline ),
	JS_CFUNC_DEF("getPipeline", 0, athena_getpipeline ),
};

static JSValue athena_object_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
	JSImageData *image;
	JSValue obj = JS_UNDEFINED;
    JSValue proto;

    model* res_m = js_mallocz(ctx, sizeof(model));
    if (!res_m)
        return JS_EXCEPTION;

	const char *file_tbo = JS_ToCString(ctx, argv[0]); // Model filename
	
	// Loading texture
	if(argc == 2) {
		GSTEXTURE *tex = malloc(sizeof(GSTEXTURE));
		const char *tex_path = JS_ToCString(ctx, argv[1]); // Texture filename
		load_image(tex, tex_path, true);
		tex->Filter = GS_FILTER_LINEAR;

		loadOBJ(res_m, file_tbo, tex);
	} else {
		loadOBJ(res_m, file_tbo, NULL);
	}

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    obj = JS_NewObjectProtoClass(ctx, proto, js_object_class_id);
    JS_FreeValue(ctx, proto);
    JS_SetOpaque(obj, res_m);

    return obj;
}

static int js_object_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue object_proto, object_class;
    
    /* create the Point class */
    JS_NewClassID(&js_object_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_object_class_id, &js_object_class);

    object_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, object_proto, js_object_proto_funcs, countof(js_object_proto_funcs));
    
    object_class = JS_NewCFunction2(ctx, athena_object_ctor, "WavefrontObj", 2, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, object_class, object_proto);
    JS_SetClassProto(ctx, js_object_class_id, object_proto);
                      
    JS_SetModuleExport(ctx, m, "WavefrontObj", object_class);
    return 0;
}

JSModuleDef *athena_render_init(JSContext* ctx){
    JSModuleDef *m;
    m = JS_NewCModule(ctx, "WavefrontObj", js_object_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "WavefrontObj");

	athena_push_module(ctx, render_init, render_funcs, countof(render_funcs), "Render");
	athena_push_module(ctx, light_init, light_funcs, countof(light_funcs), "Lights");
    return athena_push_module(ctx, camera_init, camera_funcs, countof(camera_funcs), "Camera");
}