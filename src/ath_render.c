#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/render.h"
#include "ath_env.h"

static JSValue athena_initrender(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	float aspect, fov = 0.2f, near = 0.1f, far = 2000.0f;
	JS_ToFloat32(ctx, &aspect, argv[0]);
	if (argc > 1) {
		JS_ToFloat32(ctx, &fov, argv[1]);

		if (argc > 2) {
			JS_ToFloat32(ctx, &near, argv[2]);
			JS_ToFloat32(ctx, &far, argv[3]);
		}
	}
  	init3D(aspect, fov, near, far);
	return JS_UNDEFINED;
}

static JSValue athena_newvertex(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	float x, y, z, s, t, n1, n2, n3, r, g, b, a;

	JS_ToFloat32(ctx, &x, argv[0]);
	JS_ToFloat32(ctx, &y, argv[1]);
	JS_ToFloat32(ctx, &z, argv[2]);

	JS_ToFloat32(ctx, &n1, argv[3]);
	JS_ToFloat32(ctx, &n2, argv[4]);
	JS_ToFloat32(ctx, &n3, argv[5]);

	JS_ToFloat32(ctx, &s, argv[6]);
	JS_ToFloat32(ctx, &t, argv[7]);

	JS_ToFloat32(ctx, &r, argv[8]);
	JS_ToFloat32(ctx, &g, argv[9]);
	JS_ToFloat32(ctx, &b, argv[10]);
	JS_ToFloat32(ctx, &a, argv[11]);


	JSValue obj = JS_NewObject(ctx);

	JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, x), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, y), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, z), JS_PROP_C_W_E);

	JS_DefinePropertyValueStr(ctx, obj, "n1", JS_NewFloat32(ctx, n1), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "n2", JS_NewFloat32(ctx, n2), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "n3", JS_NewFloat32(ctx, n3), JS_PROP_C_W_E);

	JS_DefinePropertyValueStr(ctx, obj, "s", JS_NewFloat32(ctx, s), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "t", JS_NewFloat32(ctx, t), JS_PROP_C_W_E);

	JS_DefinePropertyValueStr(ctx, obj, "r", JS_NewFloat32(ctx, r), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "g", JS_NewFloat32(ctx, g), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "b", JS_NewFloat32(ctx, b), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "a", JS_NewFloat32(ctx, a), JS_PROP_C_W_E);

	return obj;
}

static const JSCFunctionListEntry render_funcs[] = {
    JS_CFUNC_DEF( "setView",     2,           athena_initrender),
	JS_CFUNC_DEF( "vertex",      12,           athena_newvertex),

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
  JS_CFUNC_DEF("update", 0, athena_camupdate),
  JS_CFUNC_DEF("type", 1, athena_camtype),

  JS_PROP_INT32_DEF("DEFAULT", CAMERA_DEFAULT, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF("LOOKAT", CAMERA_LOOKAT, JS_PROP_CONFIGURABLE ),
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
    "RenderObject",
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

static JSValue athena_settexture(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int tex_idx;
	model* m = JS_GetOpaque2(ctx, this_val, js_object_class_id);

	JS_ToUint32(ctx, &tex_idx, argv[0]);

	if (m->textures[tex_idx]) {
		UnloadTexture(&(m->textures[tex_idx]));

		free(m->textures[tex_idx]->Mem);
		m->textures[tex_idx]->Mem = NULL;

		if(m->textures[tex_idx]->Clut != NULL)
		{
			free(m->textures[tex_idx]->Clut);
			m->textures[tex_idx]->Clut = NULL;
		}
	}

	GSTEXTURE *tex = malloc(sizeof(GSTEXTURE));
	const char *tex_path = JS_ToCString(ctx, argv[1]); // Texture filename
	load_image(tex, tex_path, true);
	tex->Filter = GS_FILTER_LINEAR;

	m->textures[tex_idx] = tex;
	m->tex_count = (m->tex_count < (tex_idx+1)? (tex_idx+1) : m->tex_count);

	if (argc > 2) {
		int range;
		JS_ToUint32(ctx, &range, argv[2]);
		if (range == -1) {
			range = m->indexCount;
		}

		m->tex_ranges[tex_idx] = range;
	}

	return JS_UNDEFINED;
}


static JSValue js_object_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    model* m = JS_GetOpaque2(ctx, this_val, js_object_class_id);
    if (!m)
        return JS_EXCEPTION;
		
    if (magic == 0) {
		JSValue array = JS_NewArray(ctx);

		for (int i = 0; i < m->indexCount; i++) {
			JSValue obj = JS_NewObject(ctx);
		
			JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, m->positions[i][0]), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, m->positions[i][1]), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, m->positions[i][2]), JS_PROP_C_W_E);
		
			JS_DefinePropertyValueStr(ctx, obj, "n1", JS_NewFloat32(ctx, m->normals[i][0]), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "n2", JS_NewFloat32(ctx, m->normals[i][1]), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "n3", JS_NewFloat32(ctx, m->normals[i][2]), JS_PROP_C_W_E);
		
			JS_DefinePropertyValueStr(ctx, obj, "s", JS_NewFloat32(ctx, m->texcoords[i][0]), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "t", JS_NewFloat32(ctx, m->texcoords[i][1]), JS_PROP_C_W_E);
		
			JS_DefinePropertyValueStr(ctx, obj, "r", JS_NewFloat32(ctx, m->colours[i][0]), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "g", JS_NewFloat32(ctx, m->colours[i][1]), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "b", JS_NewFloat32(ctx, m->colours[i][2]), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "a", JS_NewFloat32(ctx, m->colours[i][3]), JS_PROP_C_W_E);

			JS_DefinePropertyValueUint32(ctx, array, i, obj, JS_PROP_C_W_E);
		}
		
		return array;
	}
}

static JSValue js_object_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    model* m = JS_GetOpaque2(ctx, this_val, js_object_class_id);
    if (!m)
        return JS_EXCEPTION;

    if (magic == 0) {
		free(m->positions);
    	free(m->colours);
    	free(m->normals);
    	free(m->texcoords);

		int length;
		JS_ToUint32(ctx, &length, JS_GetPropertyStr(ctx, val, "length"));

		m->indexCount = length;
		m->positions = malloc(sizeof(VECTOR)*m->indexCount);
		m->normals = malloc(sizeof(VECTOR)*m->indexCount);
		m->texcoords = malloc(sizeof(VECTOR)*m->indexCount);
		m->colours = malloc(sizeof(VECTOR)*m->indexCount);

		for (int i = 0; i < m->indexCount; i++) {
			JSValue vertex = JS_GetPropertyUint32(ctx, val, i);

			JS_ToFloat32(ctx, &m->positions[i][0], JS_GetPropertyStr(ctx, vertex, "x"));
			JS_ToFloat32(ctx, &m->positions[i][1], JS_GetPropertyStr(ctx, vertex, "y"));
			JS_ToFloat32(ctx, &m->positions[i][2], JS_GetPropertyStr(ctx, vertex, "z"));
			m->positions[i][3] = 1.0f;

			JS_ToFloat32(ctx, &m->normals[i][0], JS_GetPropertyStr(ctx, vertex, "n1"));
			JS_ToFloat32(ctx, &m->normals[i][1], JS_GetPropertyStr(ctx, vertex, "n2"));
			JS_ToFloat32(ctx, &m->normals[i][2], JS_GetPropertyStr(ctx, vertex, "n3"));
			m->normals[i][3] = 1.0f;

			JS_ToFloat32(ctx, &m->texcoords[i][0], JS_GetPropertyStr(ctx, vertex, "s"));
			JS_ToFloat32(ctx, &m->texcoords[i][1], JS_GetPropertyStr(ctx, vertex, "t"));
			m->texcoords[i][2] = 1.0f;
			m->texcoords[i][3] = 1.0f;

			JS_ToFloat32(ctx, &m->colours[i][0], JS_GetPropertyStr(ctx, vertex, "r"));
			JS_ToFloat32(ctx, &m->colours[i][1], JS_GetPropertyStr(ctx, vertex, "g"));
			JS_ToFloat32(ctx, &m->colours[i][2], JS_GetPropertyStr(ctx, vertex, "b"));
			JS_ToFloat32(ctx, &m->colours[i][3], JS_GetPropertyStr(ctx, vertex, "a"));
		}
	}
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_object_proto_funcs[] = {
    JS_CFUNC_DEF("draw", 6, athena_drawobject),
	JS_CFUNC_DEF("drawBounds", 7, athena_drawbbox),
	JS_CFUNC_DEF("setPipeline", 1, athena_setpipeline ),
	JS_CFUNC_DEF("getPipeline", 0, athena_getpipeline ),
	JS_CFUNC_DEF("setTexture", 3, athena_settexture ),
	JS_CGETSET_MAGIC_DEF("vertices", js_object_get, js_object_set, 0),
};

static JSValue athena_object_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
	JSImageData *image;
	JSValue obj = JS_UNDEFINED;
    JSValue proto;

    model* m = js_mallocz(ctx, sizeof(model));
    if (!m)
        return JS_EXCEPTION;

	if (JS_IsArray(ctx, argv[0])) {
		memset(m, 0, sizeof(model));

		int length;
		JS_ToUint32(ctx, &length, JS_GetPropertyStr(ctx, argv[0], "length"));

		m->indexCount = length;
		m->positions = malloc(sizeof(VECTOR)*m->indexCount);
		m->normals = malloc(sizeof(VECTOR)*m->indexCount);
		m->texcoords = malloc(sizeof(VECTOR)*m->indexCount);
		m->colours = malloc(sizeof(VECTOR)*m->indexCount);

		for (int i = 0; i < m->indexCount; i++) {
			JSValue vertex = JS_GetPropertyUint32(ctx, argv[0], i);

			JS_ToFloat32(ctx, &m->positions[i][0], JS_GetPropertyStr(ctx, vertex, "x"));
			JS_ToFloat32(ctx, &m->positions[i][1], JS_GetPropertyStr(ctx, vertex, "y"));
			JS_ToFloat32(ctx, &m->positions[i][2], JS_GetPropertyStr(ctx, vertex, "z"));
			m->positions[i][3] = 1.0f;

			JS_ToFloat32(ctx, &m->normals[i][0], JS_GetPropertyStr(ctx, vertex, "n1"));
			JS_ToFloat32(ctx, &m->normals[i][1], JS_GetPropertyStr(ctx, vertex, "n2"));
			JS_ToFloat32(ctx, &m->normals[i][2], JS_GetPropertyStr(ctx, vertex, "n3"));
			m->normals[i][3] = 1.0f;

			JS_ToFloat32(ctx, &m->texcoords[i][0], JS_GetPropertyStr(ctx, vertex, "s"));
			JS_ToFloat32(ctx, &m->texcoords[i][1], JS_GetPropertyStr(ctx, vertex, "t"));
			m->texcoords[i][2] = 1.0f;
			m->texcoords[i][3] = 1.0f;

			JS_ToFloat32(ctx, &m->colours[i][0], JS_GetPropertyStr(ctx, vertex, "r"));
			JS_ToFloat32(ctx, &m->colours[i][1], JS_GetPropertyStr(ctx, vertex, "g"));
			JS_ToFloat32(ctx, &m->colours[i][2], JS_GetPropertyStr(ctx, vertex, "b"));
			JS_ToFloat32(ctx, &m->colours[i][3], JS_GetPropertyStr(ctx, vertex, "a"));
		}

		if(argc > 1) {
			GSTEXTURE *tex = malloc(sizeof(GSTEXTURE));
			const char *tex_path = JS_ToCString(ctx, argv[1]); // Texture filename
			load_image(tex, tex_path, true);
			tex->Filter = GS_FILTER_LINEAR;
	
			m->textures[0] = tex;
			m->tex_count = 1;
			m->tex_ranges[0] = m->indexCount;
		}

		m->pipeline = athena_render_set_pipeline(m, PL_DEFAULT);

		goto register_3d_object;
	}

	const char *file_tbo = JS_ToCString(ctx, argv[0]); // Model filename
	
	// Loading texture
	if(argc > 1) {
		GSTEXTURE *tex = malloc(sizeof(GSTEXTURE));
		const char *tex_path = JS_ToCString(ctx, argv[1]); // Texture filename
		load_image(tex, tex_path, true);
		tex->Filter = GS_FILTER_LINEAR;

		loadOBJ(m, file_tbo, tex);
	} else if (argc > 0) {
		loadOBJ(m, file_tbo, NULL);
	} 

register_3d_object:
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    obj = JS_NewObjectProtoClass(ctx, proto, js_object_class_id);
    JS_FreeValue(ctx, proto);
    JS_SetOpaque(obj, m);

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
    
    object_class = JS_NewCFunction2(ctx, athena_object_ctor, "RenderObject", 2, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, object_class, object_proto);
    JS_SetClassProto(ctx, js_object_class_id, object_proto);
                      
    JS_SetModuleExport(ctx, m, "RenderObject", object_class);
    return 0;
}

JSModuleDef *athena_render_init(JSContext* ctx){
    JSModuleDef *m;
    m = JS_NewCModule(ctx, "RenderObject", js_object_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "RenderObject");

	athena_push_module(ctx, render_init, render_funcs, countof(render_funcs), "Render");
	athena_push_module(ctx, light_init, light_funcs, countof(light_funcs), "Lights");
    return athena_push_module(ctx, camera_init, camera_funcs, countof(camera_funcs), "Camera");
}