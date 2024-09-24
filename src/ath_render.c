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

static int light_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, light_funcs, countof(light_funcs));
}

static int camera_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, camera_funcs, countof(camera_funcs));
}

static JSClassID js_object_class_id;

typedef struct {
	model m;
	JSValue *textures;
} JSRenderObject;

static void athena_object_dtor(JSRuntime *rt, JSValue val){
	JSRenderObject* ro = JS_GetOpaque(val, js_object_class_id);

	free(ro->m.positions);
	
	if (ro->m.colours) {
		free(ro->m.colours);
	}
    	
    free(ro->m.normals);
    free(ro->m.texcoords);
	free(ro->m.materials);
	free(ro->m.material_indices);

	//printf("%d textures\n", ro->m.texture_count);

	//for (int i = 0; i < ro->m.texture_count; i++) {
	//	if (!((JSImageData*)JS_GetOpaque(ro->textures[i], get_img_class_id()))->path) {
	//		printf("Freeing %d from mesh\n", i);
	//		JS_FreeValueRT(rt, ro->textures[i]);
	//	}
	//}

	free(ro->m.textures);
	free(ro->m.materials);
	free(ro->textures);

	js_free_rt(rt, ro);
}


static JSValue athena_object_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
	JSImageData *image;
	JSValue obj = JS_UNDEFINED;
    JSValue proto;

    JSRenderObject* ro = js_mallocz(ctx, sizeof(JSRenderObject));
	
    if (!ro)
        return JS_EXCEPTION;

	if (JS_IsObject(argv[0])) {
		memset(ro, 0, sizeof(JSRenderObject));

		uint32_t size = 0;
		JSValue vert_arr;
		void *tmp_vert_ptr = NULL;

		vert_arr = JS_GetPropertyStr(ctx, argv[0], "positions");

		tmp_vert_ptr = JS_GetArrayBuffer(ctx, &size, JS_GetTypedArrayBuffer(ctx, vert_arr, NULL, NULL, NULL));

		ro->m.index_count = size/sizeof(VECTOR);

		if (tmp_vert_ptr) {
			ro->m.positions = malloc(size);
			memcpy(ro->m.positions, tmp_vert_ptr, size);
		}

		vert_arr = JS_GetPropertyStr(ctx, argv[0], "normals");

		tmp_vert_ptr = JS_GetArrayBuffer(ctx, &size, JS_GetTypedArrayBuffer(ctx, vert_arr, NULL, NULL, NULL));

		if (tmp_vert_ptr) {
			ro->m.normals = malloc(size);
			memcpy(ro->m.normals, tmp_vert_ptr, size);
		}

		vert_arr = JS_GetPropertyStr(ctx, argv[0], "texcoords");

		tmp_vert_ptr = JS_GetArrayBuffer(ctx, &size, JS_GetTypedArrayBuffer(ctx, vert_arr, NULL, NULL, NULL));

		if (tmp_vert_ptr) {
			ro->m.texcoords = malloc(size);
			memcpy(ro->m.texcoords, tmp_vert_ptr, size);
		}

		vert_arr = JS_GetPropertyStr(ctx, argv[0], "colors");

		tmp_vert_ptr = JS_GetArrayBuffer(ctx, &size, JS_GetTypedArrayBuffer(ctx, vert_arr, NULL, NULL, NULL));

		if (tmp_vert_ptr) {
			ro->m.colours = malloc(size);
			memcpy(ro->m.colours, tmp_vert_ptr, size);
		}

		if(argc > 1) {
			JS_DupValue(ctx, argv[1]);
			image = JS_GetOpaque2(ctx, argv[1], get_img_class_id());

			image->tex->Filter = GS_FILTER_LINEAR;
	
			ro->m.materials = (ath_mat *)malloc(sizeof(ath_mat));
			ro->m.material_count = 1;

			ro->m.material_indices = (material_index *)malloc(sizeof(material_index));
			ro->m.material_index_count = 1;

			init_vector(ro->m.materials[0].ambient);
			init_vector(ro->m.materials[0].diffuse);
			init_vector(ro->m.materials[0].specular);
			init_vector(ro->m.materials[0].emission);
			init_vector(ro->m.materials[0].transmittance);
			init_vector(ro->m.materials[0].transmission_filter);

			ro->m.materials[0].shininess = 1.0f;
			ro->m.materials[0].refraction = 1.0f;
			ro->m.materials[0].disolve = 1.0f;

			ro->m.materials[0].texture = image->tex;
			ro->m.material_indices[0].index = 0;
			ro->m.material_indices[0].end = ro->m.index_count;

			ro->m.textures = malloc(sizeof(GSTEXTURE*));
			ro->textures = malloc(sizeof(JSValue));

			ro->m.textures[0] = image->tex;
			ro->m.texture_count = 1;

			ro->textures[0] = argv[1];
		}

		ro->m.tristrip = false;
		if (argc > 2) 
			ro->m.tristrip = JS_ToBool(ctx, argv[2]);
	
		ro->m.pipeline = athena_render_set_pipeline(&ro->m, PL_PVC);

		goto register_3d_object;
	}

	const char *file_tbo = JS_ToCString(ctx, argv[0]); // Model filename

	// Loading texture
	if(argc > 1) {
		image = JS_GetOpaque2(ctx, argv[1], get_img_class_id());

		ro->textures = malloc(sizeof(JSValue));

		ro->textures[0] = argv[1];

		image->tex->Filter = GS_FILTER_LINEAR;

		loadOBJ(&ro->m, file_tbo, image->tex);

	} else if (argc > 0) {
		loadOBJ(&ro->m, file_tbo, NULL);

		ro->textures = malloc(sizeof(JSValue)*ro->m.texture_count);

		for (int i = 0; i < ro->m.texture_count; i++) {
			JSImageData* image;
    		JSValue img_obj = JS_UNDEFINED;

			image = js_mallocz(ctx, sizeof(*image));
    		if (!image)
    		    return JS_EXCEPTION;

			image->delayed = true;
			image->tex = ro->m.textures[i];

			image->loaded = true;
			image->width = image->tex->Width;
			image->height = image->tex->Height;
			image->endx = image->tex->Width;
			image->endy = image->tex->Height;

			image->startx = 0.0f;
			image->starty = 0.0f;
			image->angle = 0.0f;
			image->color = 0x80808080;

    		img_obj = JS_NewObjectClass(ctx, get_img_class_id());    
    		JS_SetOpaque(img_obj, image);

			ro->textures[i] = img_obj;

			//JS_DupValue(ctx, img_obj);
		}
	} 

register_3d_object:
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    obj = JS_NewObjectProtoClass(ctx, proto, js_object_class_id);

	if (ro->m.texture_count > 0) {
		JSValue tex_arr = JS_NewArray(ctx);
		for (int i = 0; i < ro->m.texture_count; i++) {
			if (((JSImageData*)JS_GetOpaque(ro->textures[i], get_img_class_id()))->path)
				JS_DupValue(ctx, ro->textures[i]);
			JS_DefinePropertyValueUint32(ctx, tex_arr, i, ro->textures[i], JS_PROP_C_W_E);
		}
		JS_DefinePropertyValueStr(ctx, obj, "textures", tex_arr, JS_PROP_C_W_E);
	}

    JS_FreeValue(ctx, proto);
    JS_SetOpaque(obj, ro);

    return obj;
}

static JSClassDef js_object_class = {
    "RenderObject",
    .finalizer = athena_object_dtor,
}; 

static JSValue athena_drawobject(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	float pos_x, pos_y, pos_z, rot_x, rot_y, rot_z;

	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_object_class_id);

	JS_ToFloat32(ctx, &pos_x, argv[0]);
	JS_ToFloat32(ctx, &pos_y, argv[1]);
	JS_ToFloat32(ctx, &pos_z, argv[2]);
	JS_ToFloat32(ctx, &rot_x, argv[3]);
	JS_ToFloat32(ctx, &rot_y, argv[4]);
	JS_ToFloat32(ctx, &rot_z, argv[5]);
	
	ro->m.render(&ro->m, pos_x, pos_y, pos_z, rot_x, rot_y, rot_z);

	return JS_UNDEFINED;
}

static JSValue athena_drawbbox(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	float pos_x, pos_y, pos_z, rot_x, rot_y, rot_z;
	Color color;

	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_object_class_id);

	JS_ToFloat32(ctx, &pos_x, argv[0]);
	JS_ToFloat32(ctx, &pos_y, argv[1]);
	JS_ToFloat32(ctx, &pos_z, argv[2]);
	JS_ToFloat32(ctx, &rot_x, argv[3]);
	JS_ToFloat32(ctx, &rot_y, argv[4]);
	JS_ToFloat32(ctx, &rot_z, argv[5]);
	JS_ToUint32(ctx, &color,  argv[6]);
	
	draw_bbox(&ro->m, pos_x, pos_y, pos_z, rot_x, rot_y, rot_z, color);

	return JS_UNDEFINED;
}

static JSValue athena_setpipeline(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int pipeline;
	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_object_class_id);

	JS_ToUint32(ctx, &pipeline, argv[0]);

	return JS_NewUint32(ctx, athena_render_set_pipeline(&ro->m, pipeline));
}

static JSValue athena_getpipeline(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int pipeline;
	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_object_class_id);

	return JS_NewUint32(ctx, ro->m.pipeline);
}

static JSValue athena_settexture(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int tex_idx;
	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_object_class_id);

	JS_ToUint32(ctx, &tex_idx, argv[0]);

	if (ro->m.textures[tex_idx]) {
		JS_FreeValue(ctx, ro->textures[tex_idx]);
	}

	JS_DupValue(ctx, argv[1]);
	JSImageData* image = JS_GetOpaque2(ctx, argv[1], get_img_class_id());

	/*if (ro->m.texture_count < (tex_idx+1)) {
		ro->m.textures =   realloc(ro->m.textures,   sizeof(GSTEXTURE*)*(ro->m.texture_count+1));
		ro->m.tex_ranges = realloc(ro->m.tex_ranges,        sizeof(int)*(ro->m.texture_count+1));
	}

	ro->textures[tex_idx] = argv[1];
	ro->m.textures[tex_idx] = image->tex;

	JSValue tex_arr = JS_GetPropertyStr(ctx, this_val, "textures");
	JS_DefinePropertyValueUint32(ctx, tex_arr, tex_idx, ro->textures[tex_idx], JS_PROP_C_W_E);

	ro->m.texture_count = (ro->m.texture_count < (tex_idx+1)? (tex_idx+1) : ro->m.texture_count);

	if (argc > 2) {
		int range;
		JS_ToUint32(ctx, &range, argv[2]);
		if (range == -1) {
			range = ro->m.index_count;
		}

		ro->m.tex_ranges[tex_idx] = range;
	}*/

	return JS_UNDEFINED;
}

static JSValue athena_gettexture(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int tex_idx;
	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_object_class_id);

	JS_ToUint32(ctx, &tex_idx, argv[0]);

	if (ro->m.texture_count > tex_idx) {
		JS_DupValue(ctx, ro->textures[tex_idx]);
		return ro->textures[tex_idx];
	}

	return JS_UNDEFINED;
}



static JSValue js_object_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_object_class_id);
    if (!ro)
        return JS_EXCEPTION;
		
    if (magic == 0) {
		JSValue obj = JS_NewObject(ctx);

		if (ro->m.positions)
			JS_DefinePropertyValueStr(ctx, obj, "positions", JS_NewArrayBufferCopy(ctx, ro->m.positions, ro->m.index_count*sizeof(VECTOR)), JS_PROP_C_W_E);

		if (ro->m.normals)
			JS_DefinePropertyValueStr(ctx, obj, "normals",   JS_NewArrayBufferCopy(ctx, ro->m.normals, ro->m.index_count*sizeof(VECTOR)), JS_PROP_C_W_E);

		if (ro->m.texcoords)
			JS_DefinePropertyValueStr(ctx, obj, "texcoords", JS_NewArrayBufferCopy(ctx, ro->m.texcoords, ro->m.index_count*sizeof(VECTOR)), JS_PROP_C_W_E);

		if (ro->m.colours)
			JS_DefinePropertyValueStr(ctx, obj, "colors",    JS_NewArrayBufferCopy(ctx, ro->m.colours, ro->m.index_count*sizeof(VECTOR)), JS_PROP_C_W_E);


		//JS_DefinePropertyValueStr(ctx, obj, "materials",        argv[4], JS_PROP_C_W_E);
		//JS_DefinePropertyValueStr(ctx, obj, "material_indices", argv[5], JS_PROP_C_W_E);

		return obj;
	} else if (magic == 1) {
		return JS_NewUint32(ctx, ro->m.index_count);
	} else if (magic == 2) {
		JSValue array = JS_NewArray(ctx);

		for (int i = 0; i < 8; i++) {
			JSValue obj = JS_NewObject(ctx);
		
			JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, ro->m.bounding_box[i][0]), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, ro->m.bounding_box[i][1]), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, ro->m.bounding_box[i][2]), JS_PROP_C_W_E);
			JS_DefinePropertyValueUint32(ctx, array, i, obj, JS_PROP_C_W_E);
		}
		
		return array;
	}
}

static JSValue js_object_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_object_class_id);
    if (!ro)
        return JS_EXCEPTION;

    if (magic == 0) {
		if (ro->m.positions)
			free(ro->m.positions);
		
		if (ro->m.colours)
    		free(ro->m.colours);
		
		if (ro->m.normals)
    		free(ro->m.normals);
		
		if (ro->m.texcoords)
    		free(ro->m.texcoords);

		uint32_t size = 0;
		JSValue vert_arr, ta_buf;
		void *tmp_vert_ptr = NULL;

		vert_arr = JS_GetPropertyStr(ctx, val, "positions");

		ta_buf = JS_GetTypedArrayBuffer(ctx, vert_arr, NULL, NULL, NULL);

		tmp_vert_ptr = JS_GetArrayBuffer(ctx, &size, ((ta_buf != JS_EXCEPTION)? ta_buf : vert_arr));

		ro->m.index_count = size/sizeof(VECTOR);

		if (tmp_vert_ptr) {
			ro->m.positions = malloc(size);
			memcpy(ro->m.positions, tmp_vert_ptr, size);
		}

		vert_arr = JS_GetPropertyStr(ctx, val, "normals");

		ta_buf = JS_GetTypedArrayBuffer(ctx, vert_arr, NULL, NULL, NULL);

		tmp_vert_ptr = JS_GetArrayBuffer(ctx, &size, ((ta_buf != JS_EXCEPTION)? ta_buf : vert_arr));

		if (tmp_vert_ptr) {
			ro->m.normals = malloc(size);
			memcpy(ro->m.normals, tmp_vert_ptr, size);
		}

		vert_arr = JS_GetPropertyStr(ctx, val, "texcoords");

		ta_buf = JS_GetTypedArrayBuffer(ctx, vert_arr, NULL, NULL, NULL);

		tmp_vert_ptr = JS_GetArrayBuffer(ctx, &size, ((ta_buf != JS_EXCEPTION)? ta_buf : vert_arr));

		if (tmp_vert_ptr) {
			ro->m.texcoords = malloc(size);
			memcpy(ro->m.texcoords, tmp_vert_ptr, size);
		}

		vert_arr = JS_GetPropertyStr(ctx, val, "colors");

		ta_buf = JS_GetTypedArrayBuffer(ctx, vert_arr, NULL, NULL, NULL);

		tmp_vert_ptr = JS_GetArrayBuffer(ctx, &size, ((ta_buf != JS_EXCEPTION)? ta_buf : vert_arr));

		if (tmp_vert_ptr) {
			ro->m.colours = malloc(size);
			memcpy(ro->m.colours, tmp_vert_ptr, size);
		}
	} else if (magic == 2) {

		for (int i = 0; i < 8; i++) {
			JSValue vertex = JS_GetPropertyUint32(ctx, val, i);

			JS_ToFloat32(ctx, &ro->m.bounding_box[i][0], JS_GetPropertyStr(ctx, vertex, "x"));
			JS_ToFloat32(ctx, &ro->m.bounding_box[i][1], JS_GetPropertyStr(ctx, vertex, "y"));
			JS_ToFloat32(ctx, &ro->m.bounding_box[i][2], JS_GetPropertyStr(ctx, vertex, "z"));
			ro->m.bounding_box[i][3] = 1.0f;

			JS_FreeValue(ctx, vertex);
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
	JS_CFUNC_DEF("getTexture", 1, athena_gettexture ),
	JS_CGETSET_MAGIC_DEF("vertices", js_object_get, js_object_set, 0),
	JS_CGETSET_MAGIC_DEF("size", js_object_get, js_object_set, 1),
	JS_CGETSET_MAGIC_DEF("bounds", js_object_get, js_object_set, 2),
};

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

static JSValue athena_newmaterial(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	JSValue obj = JS_NewObject(ctx);

	JSValue arr = JS_NewArray(ctx);
	JS_DefinePropertyValueUint32(ctx, arr, 0, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);
	JS_DefinePropertyValueUint32(ctx, arr, 1, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);
	JS_DefinePropertyValueUint32(ctx, arr, 2, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);

	JS_DefinePropertyValueStr(ctx, obj, "ambient", arr, JS_PROP_C_W_E);

	arr = JS_NewArray(ctx);
	JS_DefinePropertyValueUint32(ctx, arr, 0, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);
	JS_DefinePropertyValueUint32(ctx, arr, 1, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);
	JS_DefinePropertyValueUint32(ctx, arr, 2, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);

	JS_DefinePropertyValueStr(ctx, obj, "diffuse", arr, JS_PROP_C_W_E);

	arr = JS_NewArray(ctx);
	JS_DefinePropertyValueUint32(ctx, arr, 0, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);
	JS_DefinePropertyValueUint32(ctx, arr, 1, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);
	JS_DefinePropertyValueUint32(ctx, arr, 2, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);

	JS_DefinePropertyValueStr(ctx, obj, "specular", arr, JS_PROP_C_W_E);

	arr = JS_NewArray(ctx);
	JS_DefinePropertyValueUint32(ctx, arr, 0, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);
	JS_DefinePropertyValueUint32(ctx, arr, 1, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);
	JS_DefinePropertyValueUint32(ctx, arr, 2, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);

	JS_DefinePropertyValueStr(ctx, obj, "emission", arr, JS_PROP_C_W_E);

	arr = JS_NewArray(ctx);
	JS_DefinePropertyValueUint32(ctx, arr, 0, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);
	JS_DefinePropertyValueUint32(ctx, arr, 1, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);
	JS_DefinePropertyValueUint32(ctx, arr, 2, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);

	JS_DefinePropertyValueStr(ctx, obj, "transmittance", arr, JS_PROP_C_W_E);

	JS_DefinePropertyValueStr(ctx, obj, "shininess", JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "refraction", JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);

	arr = JS_NewArray(ctx);
	JS_DefinePropertyValueUint32(ctx, arr, 0, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);
	JS_DefinePropertyValueUint32(ctx, arr, 1, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);
	JS_DefinePropertyValueUint32(ctx, arr, 2, JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);

	JS_DefinePropertyValueStr(ctx, obj, "transmission_filter", arr, JS_PROP_C_W_E);

	JS_DefinePropertyValueStr(ctx, obj, "disolve", JS_NewFloat32(ctx, 1.0f), JS_PROP_C_W_E);

	JS_DefinePropertyValueStr(ctx, obj, "texture", JS_UNDEFINED, JS_PROP_C_W_E);

	return obj;
}

static JSValue athena_newmaterialindex(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	JSValue obj = JS_NewObject(ctx);
	JS_DefinePropertyValueStr(ctx, obj, "index", argv[0], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "end",   argv[1], JS_PROP_C_W_E);

	return obj;
}

static JSValue athena_newvertex(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	JSValue obj = JS_NewObject(ctx);

	JS_DefinePropertyValueStr(ctx, obj, "positions",        argv[0], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "normals",          argv[1], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "texcoords",        argv[2], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "colors",           argv[3], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "materials",        argv[4], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "material_indices", argv[5], JS_PROP_C_W_E);

	return obj;
}

static const JSCFunctionListEntry render_funcs[] = {
    JS_CFUNC_DEF( "setView",         4,                athena_initrender),
	JS_CFUNC_DEF( "vertexList",      6,                 athena_newvertex),
	JS_CFUNC_DEF( "material",        0,               athena_newmaterial),
	JS_CFUNC_DEF( "materialIndex",   2,          athena_newmaterialindex),

	JS_PROP_INT32_DEF("PL_NO_LIGHTS_COLORS", PL_NO_LIGHTS_COLORS, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("PL_NO_LIGHTS_COLORS_TEX", PL_NO_LIGHTS_COLORS_TEX, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("PL_NO_LIGHTS", PL_NO_LIGHTS, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("PL_NO_LIGHTS_TEX", PL_NO_LIGHTS_TEX, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("PL_DEFAULT", PL_DEFAULT, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("PL_DEFAULT_NO_TEX", PL_DEFAULT_NO_TEX, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("PL_SPECULAR", PL_SPECULAR, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("PL_SPECULAR_NO_TEX", PL_SPECULAR_NO_TEX, JS_PROP_CONFIGURABLE ),
};

static int render_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, render_funcs, countof(render_funcs));
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