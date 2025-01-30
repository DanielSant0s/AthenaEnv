#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/render.h"
#include "ath_env.h"

static JSClassID js_object_class_id;

typedef struct {
	model m;
	JSValue *textures;
} JSRenderObject;

static void athena_object_dtor(JSRuntime *rt, JSValue val){
	JSRenderObject* ro = JS_GetOpaque(val, js_object_class_id);

	if (ro->m.colours)
		free(ro->m.positions);
	
	if (ro->m.colours)
		free(ro->m.colours);

	if (ro->m.normals)	
    	free(ro->m.normals);

	if (ro->m.texcoords)
    	free(ro->m.texcoords);
	
	if (ro->m.materials)
		free(ro->m.materials);
	
	if (ro->m.material_indices)
		free(ro->m.material_indices);

	//printf("%d textures\n", ro->m.texture_count);

	for (int i = 0; i < ro->m.texture_count; i++) {
		if (!((JSImageData*)JS_GetOpaque(ro->textures[i], get_img_class_id()))->path) {
			printf("Freeing %d from mesh\n", i);
			JS_FreeValueRT(rt, ro->textures[i]);
		}
	}

	if (ro->m.textures)
		free(ro->m.textures);

	if (ro->textures)
		free(ro->textures);

	js_free_rt(rt, ro);
}

static const char* vert_attributes[] = {
	"positions",
	"normals",
	"texcoords",
	"colors"
};

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
		JSValue vert_arr, ta_buf;
		void *tmp_vert_ptr = NULL;
		
		VECTOR** attributes_ptr[] = {
			&ro->m.positions,
			&ro->m.normals,
			&ro->m.texcoords,
			&ro->m.colours
		};

		for (int i = 0; i < 4; i++) {
			vert_arr = JS_GetPropertyStr(ctx, argv[0], vert_attributes[i]);

			ta_buf = JS_GetTypedArrayBuffer(ctx, vert_arr, NULL, NULL, NULL);

			tmp_vert_ptr = JS_GetArrayBuffer(ctx, &size, ((ta_buf != JS_EXCEPTION)? ta_buf : vert_arr));

			if (!i)
				ro->m.index_count = size/sizeof(VECTOR);

			if (tmp_vert_ptr) {
				*attributes_ptr[i] = malloc(size);
				memcpy(*attributes_ptr[i], tmp_vert_ptr, size);
			}			

			JS_FreeValue(ctx, ((ta_buf != JS_EXCEPTION)? ta_buf : vert_arr));
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

			ro->m.materials[0].texture_id = -1;
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
	uint32_t tex_idx;
	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_object_class_id);

	JS_ToUint32(ctx, &tex_idx, argv[0]);

	if (ro->m.texture_count < (tex_idx+1)) {
		ro->m.textures = realloc(ro->m.textures, sizeof(GSTEXTURE*)*(ro->m.texture_count+1));
		ro->textures =   realloc(ro->textures,   sizeof(JSValue)*(ro->m.texture_count+1));
		ro->m.textures[tex_idx] = NULL;
	}

	if (ro->m.textures[tex_idx]) {
		//JS_FreeValue(ctx, ro->textures[tex_idx]);
	}

	JS_DupValue(ctx, argv[1]);
	JSImageData* image = JS_GetOpaque2(ctx, argv[1], get_img_class_id());

	ro->textures[tex_idx] = argv[1];
	ro->m.textures[tex_idx] = image->tex;

	JSValue tex_arr = JS_GetPropertyStr(ctx, this_val, "textures");
	JS_DefinePropertyValueUint32(ctx, tex_arr, tex_idx, ro->textures[tex_idx], JS_PROP_C_W_E);
	JS_FreeValue(ctx, tex_arr);

	ro->m.texture_count = (ro->m.texture_count < (tex_idx+1)? (tex_idx+1) : ro->m.texture_count);

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

inline JSValue JS_NewVector(JSContext *ctx, VECTOR v) {
	JSValue vec = JS_NewObject(ctx);
	JS_DefinePropertyValueStr(ctx, vec, "x", JS_NewFloat32(ctx, v[0]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, vec, "y", JS_NewFloat32(ctx, v[1]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, vec, "z", JS_NewFloat32(ctx, v[2]), JS_PROP_C_W_E);

	return vec;
}

inline void JS_ToVector(JSContext *ctx, VECTOR v, JSValue vec) {
	JS_ToFloat32(ctx, &v[0], JS_GetPropertyStr(ctx, vec, "x"));
	JS_ToFloat32(ctx, &v[1], JS_GetPropertyStr(ctx, vec, "y"));
	JS_ToFloat32(ctx, &v[2], JS_GetPropertyStr(ctx, vec, "z"));
	v[3] = 1.0f;

	JS_FreeValue(ctx, vec);
}

inline JSValue JS_NewMaterial(JSContext *ctx, VECTOR v) {
	JSValue vec = JS_NewObject(ctx);
	JS_DefinePropertyValueStr(ctx, vec, "r", JS_NewFloat32(ctx, v[0]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, vec, "g", JS_NewFloat32(ctx, v[1]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, vec, "b", JS_NewFloat32(ctx, v[2]), JS_PROP_C_W_E);

	return vec;
}

inline void JS_ToMaterial(JSContext *ctx, VECTOR v, JSValue vec) {
	JS_ToFloat32(ctx, &v[0], JS_GetPropertyStr(ctx, vec, "r"));
	JS_ToFloat32(ctx, &v[1], JS_GetPropertyStr(ctx, vec, "g"));
	JS_ToFloat32(ctx, &v[2], JS_GetPropertyStr(ctx, vec, "b"));
	v[3] = 1.0f;

	JS_FreeValue(ctx, vec);
}

static JSValue js_object_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_object_class_id);
    if (!ro)
        return JS_EXCEPTION;
		
    if (magic == 0) {
		JSValue obj = JS_NewObject(ctx);

		if (ro->m.positions)
			JS_DefinePropertyValueStr(ctx, obj, "positions", JS_NewArrayBuffer(ctx, ro->m.positions, ro->m.index_count*sizeof(VECTOR), NULL, NULL, false), JS_PROP_C_W_E);

		if (ro->m.normals)
			JS_DefinePropertyValueStr(ctx, obj, "normals",   JS_NewArrayBuffer(ctx, ro->m.normals, ro->m.index_count*sizeof(VECTOR), NULL, NULL, false), JS_PROP_C_W_E);

		if (ro->m.texcoords)
			JS_DefinePropertyValueStr(ctx, obj, "texcoords", JS_NewArrayBuffer(ctx, ro->m.texcoords, ro->m.index_count*sizeof(VECTOR), NULL, NULL, false), JS_PROP_C_W_E);

		if (ro->m.colours)
			JS_DefinePropertyValueStr(ctx, obj, "colors",    JS_NewArrayBuffer(ctx, ro->m.colours, ro->m.index_count*sizeof(VECTOR), NULL, NULL, false), JS_PROP_C_W_E);

		return obj;
	} else if (magic == 1) {
		JSValue arr = JS_NewArray(ctx);

		for (int i = 0; i < ro->m.material_count; i++) {
			JSValue obj = JS_NewObject(ctx);

			JS_DefinePropertyValueStr(ctx, obj, "ambient", JS_NewMaterial(ctx, &ro->m.materials[i].ambient), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "diffuse", JS_NewMaterial(ctx, &ro->m.materials[i].diffuse), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "specular", JS_NewMaterial(ctx, &ro->m.materials[i].specular), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "emission", JS_NewMaterial(ctx, &ro->m.materials[i].emission), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "transmittance", JS_NewMaterial(ctx, &ro->m.materials[i].transmittance), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "shininess", JS_NewFloat32(ctx, ro->m.materials[i].shininess), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "refraction", JS_NewFloat32(ctx, ro->m.materials[i].refraction), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "transmission_filter", JS_NewMaterial(ctx, &ro->m.materials[i].transmission_filter), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "disolve", JS_NewFloat32(ctx, ro->m.materials[i].disolve), JS_PROP_C_W_E);

			JS_DefinePropertyValueStr(ctx, obj, "texture_id", JS_NewInt32(ctx, ro->m.materials[i].texture_id), JS_PROP_C_W_E);

			JS_DefinePropertyValueUint32(ctx, arr, i, obj, JS_PROP_C_W_E);
		}

		return arr;
	} else if (magic == 2) {
		JSValue arr = JS_NewArray(ctx);

		for (int i = 0; i < ro->m.material_index_count; i++) {
			JSValue obj = JS_NewObject(ctx);

			JS_DefinePropertyValueStr(ctx, obj, "index", JS_NewUint32(ctx, ro->m.material_indices[i].index), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "end",   JS_NewUint32(ctx, ro->m.material_indices[i].end),   JS_PROP_C_W_E);

			JS_DefinePropertyValueUint32(ctx, arr, i, obj, JS_PROP_C_W_E);
		}

		return arr;
	} else if (magic == 7) {
		return JS_NewUint32(ctx, ro->m.index_count);
	} else if (magic == 8) {
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
		uint32_t size = 0;
		JSValue vert_arr, ta_buf;
		void *tmp_vert_ptr = NULL;

		VECTOR** attributes_ptr[] = {
			&ro->m.positions,
			&ro->m.normals,
			&ro->m.texcoords,
			&ro->m.colours
		};

		for (int i = 0; i < 4; i++) {
			if (*attributes_ptr[i])
				free(*attributes_ptr[i]);

			vert_arr = JS_GetPropertyStr(ctx, val, vert_attributes[i]);

			ta_buf = JS_GetTypedArrayBuffer(ctx, vert_arr, NULL, NULL, NULL);

			tmp_vert_ptr = JS_GetArrayBuffer(ctx, &size, ((ta_buf != JS_EXCEPTION)? ta_buf : vert_arr));

			if (!i)
				ro->m.index_count = size/sizeof(VECTOR);

			if (tmp_vert_ptr) {
				*attributes_ptr[i] = malloc(size);
				memcpy(*attributes_ptr[i], tmp_vert_ptr, size);
			}			
		}

	} else if (magic == 1) {
		uint32_t material_count = 0;

		JS_ToUint32(ctx, &material_count, JS_GetPropertyStr(ctx, val, "length"));

		if (material_count > ro->m.material_count) {
			ro->m.materials = realloc(ro->m.materials, material_count);
		}

		for (int i = 0; i < material_count; i++) {
			JSValue obj = JS_GetPropertyUint32(ctx, val, i);

			JS_ToMaterial(ctx, &ro->m.materials[i].ambient,             JS_GetPropertyStr(ctx, obj, "ambient"));
			JS_ToMaterial(ctx, &ro->m.materials[i].diffuse,             JS_GetPropertyStr(ctx, obj, "diffuse"));
			JS_ToMaterial(ctx, &ro->m.materials[i].specular,            JS_GetPropertyStr(ctx, obj, "specular"));
			JS_ToMaterial(ctx, &ro->m.materials[i].emission,            JS_GetPropertyStr(ctx, obj, "emission"));
			JS_ToMaterial(ctx, &ro->m.materials[i].transmittance,       JS_GetPropertyStr(ctx, obj, "transmittance"));
			JS_ToFloat32(ctx,  &ro->m.materials[i].shininess,           JS_GetPropertyStr(ctx, obj, "shininess"));
			JS_ToFloat32(ctx,  &ro->m.materials[i].refraction,          JS_GetPropertyStr(ctx, obj, "refraction"));
			JS_ToMaterial(ctx, &ro->m.materials[i].transmission_filter, JS_GetPropertyStr(ctx, obj, "transmission_filter"));
			JS_ToFloat32(ctx,  &ro->m.materials[i].disolve,             JS_GetPropertyStr(ctx, obj, "disolve"));

			JS_ToInt32(ctx,    &ro->m.materials[i].texture_id,          JS_GetPropertyStr(ctx, obj, "texture_id"));

			JS_FreeValue(ctx, obj);
		}

		ro->m.material_count = material_count;

	} else if (magic == 2) {
		uint32_t material_index_count = 0;

		JS_ToUint32(ctx, &material_index_count, JS_GetPropertyStr(ctx, val, "length"));

		if (material_index_count > ro->m.material_index_count) {
			ro->m.material_indices = realloc(ro->m.material_indices, material_index_count);
		}

		for (int i = 0; i < material_index_count; i++) {
			JSValue obj = JS_GetPropertyUint32(ctx, val, i);

			JS_ToUint32(ctx, &ro->m.material_indices[i].index, JS_GetPropertyStr(ctx, obj, "index"));
			JS_ToUint32(ctx, &ro->m.material_indices[i].end,   JS_GetPropertyStr(ctx, obj, "end"));

			JS_FreeValue(ctx, obj);
		}

		ro->m.material_index_count = material_index_count;

	} else if (magic == 8) {

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
    JS_CFUNC_DEF("draw",        6,  athena_drawobject),
	JS_CFUNC_DEF("drawBounds",  7,    athena_drawbbox),
	JS_CFUNC_DEF("setPipeline", 1, athena_setpipeline),
	JS_CFUNC_DEF("getPipeline", 0, athena_getpipeline),
	JS_CFUNC_DEF("setTexture",  3,  athena_settexture),
	JS_CFUNC_DEF("getTexture",  1,  athena_gettexture),

	JS_CGETSET_MAGIC_DEF("vertices",         js_object_get, js_object_set, 0),
	JS_CGETSET_MAGIC_DEF("materials",        js_object_get, js_object_set, 1),
	JS_CGETSET_MAGIC_DEF("material_indices", js_object_get, js_object_set, 2),
	JS_CGETSET_MAGIC_DEF("size",             js_object_get, js_object_set, 7),
	JS_CGETSET_MAGIC_DEF("bounds",           js_object_get, js_object_set, 8),
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

	JS_DefinePropertyValueStr(ctx, obj, "texture_id", JS_NewInt32(ctx, -1), JS_PROP_C_W_E);

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

	JS_PROP_INT32_DEF("PL_NO_LIGHTS_COLORS",         PL_NO_LIGHTS_COLORS, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PL_NO_LIGHTS_COLORS_TEX", PL_NO_LIGHTS_COLORS_TEX, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PL_NO_LIGHTS",                       PL_NO_LIGHTS, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PL_NO_LIGHTS_TEX",               PL_NO_LIGHTS_TEX, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PL_DEFAULT",                           PL_DEFAULT, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PL_DEFAULT_NO_TEX",             PL_DEFAULT_NO_TEX, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PL_SPECULAR",                         PL_SPECULAR, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PL_SPECULAR_NO_TEX",           PL_SPECULAR_NO_TEX, JS_PROP_CONFIGURABLE),
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

	return athena_push_module(ctx, render_init, render_funcs, countof(render_funcs), "Render");
}
