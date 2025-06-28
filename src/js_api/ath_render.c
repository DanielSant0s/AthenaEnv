#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <render.h>
#include <ath_env.h>

static JSClassID js_render_data_class_id;

typedef struct {
	athena_render_data m;
	JSValue *textures;
} JSRenderData;

static void athena_render_data_dtor(JSRuntime *rt, JSValue val){
	JSRenderData* ro = JS_GetOpaque(val, js_render_data_class_id);

	if (!ro)
		return;

	if (ro->m.indices)
		free(ro->m.indices); 

	if (ro->m.positions)
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

	if (ro->m.skin_data)
		free(ro->m.skin_data);

	if (ro->m.skeleton)
		free(ro->m.skeleton);

	if (ro->m.skeleton->bones)
		free(ro->m.skeleton->bones);

	//printf("%d textures\n", ro->m.texture_count);

	//for (int i = 0; i < ro->m.texture_count; i++) {
	//	if (!((JSImageData*)JS_GetOpaque(ro->textures[i], get_img_class_id()))->path) {
	//		printf("Freeing %d from mesh\n", i);
	//		JS_FreeValueRT(rt, ro->textures[i]);
	//	}
	//}

	if (ro->m.textures)
		free(ro->m.textures);

	if (ro->textures)
		free(ro->textures);

	js_free_rt(rt, ro);

	JS_SetOpaque(val, NULL);
}

static const char* vert_attributes[] = {
	"positions",
	"normals",
	"texcoords",
	"colors",
};

static JSValue athena_render_data_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
	JSImageData *image;
	JSValue obj = JS_UNDEFINED;
    JSValue proto;

    JSRenderData* ro = js_mallocz(ctx, sizeof(JSRenderData));
	
    if (!ro)
        return JS_EXCEPTION;

	if (JS_IsObject(argv[0])) {
		memset(ro, 0, sizeof(JSRenderData));

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

		ro->m.attributes.has_decal = false;
		ro->m.attributes.has_refmap = false;
		ro->m.attributes.has_bumpmap = false;

		ro->m.materials[0].shininess = 1.0f;
		ro->m.materials[0].refraction = 1.0f;
		ro->m.materials[0].disolve = 1.0f;

		ro->m.materials[0].texture_id = -1;
    	ro->m.materials[0].bump_texture_id = -1;  
		ro->m.materials[0].decal_texture_id = -1;  
    	ro->m.materials[0].ref_texture_id = -1; 

		ro->m.material_indices[0].index = 0;
		ro->m.material_indices[0].end = ro->m.index_count;

		if(argc > 1) {
			JS_DupValue(ctx, argv[1]);
			image = JS_GetOpaque2(ctx, argv[1], get_img_class_id());

			image->tex->Filter = GS_FILTER_LINEAR;

			ro->m.textures = malloc(sizeof(GSSURFACE*));
			ro->textures = malloc(sizeof(JSValue));

			ro->m.textures[0] = image->tex;
			ro->m.texture_count = 1;

			ro->m.materials[0].texture_id = image->tex;

			ro->textures[0] = argv[1];
		}

		ro->m.tristrip = false;
		if (argc > 2) 
			ro->m.tristrip = JS_ToBool(ctx, argv[2]);
	
		ro->m.pipeline = PL_DEFAULT;

		goto register_3d_render_data;
	}

	const char *file_tbo = JS_ToCString(ctx, argv[0]); // athena_render_data filename

	// Loading texture
	if(argc > 1) {
		image = JS_GetOpaque2(ctx, argv[1], get_img_class_id());

		ro->textures = malloc(sizeof(JSValue));

		ro->textures[0] = argv[1];

		image->tex->Filter = GS_FILTER_LINEAR;

		loadModel(&ro->m, file_tbo, image->tex);

	} else if (argc > 0) {
		loadModel(&ro->m, file_tbo, NULL);

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

register_3d_render_data:
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    obj = JS_NewObjectProtoClass(ctx, proto, js_render_data_class_id);

	ro->m.attributes.accurate_clipping = 1;
	ro->m.attributes.face_culling = CULL_FACE_BACK;
	ro->m.attributes.texture_mapping = 1;
	ro->m.attributes.shade_model = 1;

	FlushCache(WRITEBACK_DCACHE);

	if (ro->m.skin_data) {
		JSValue bones = JS_NewArray(ctx);

		// TODO: create a class for bones so they can be passed by reference
		for (int i = 0; i < ro->m.skeleton->bone_count; i++) {
			JSValue bone = JS_NewObject(ctx);

			JS_DefinePropertyValueStr(ctx, bone, "name",      JS_NewString(ctx, ro->m.skeleton->bones[i].name), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, bone, "parent_id", JS_NewInt32(ctx, ro->m.skeleton->bones[i].parent_id), JS_PROP_C_W_E);

    		JSValue bone_matrix = JS_NewObjectClass(ctx, get_matrix4_class_id());

    		JS_SetOpaque(bone_matrix, &ro->m.skeleton->bones[i].inverse_bind);

			JS_DefinePropertyValueStr(ctx, bone, "inverse_bind", bone_matrix, JS_PROP_C_W_E);


			JSValue bone_position = JS_NewObjectClass(ctx, get_vector4_class_id());

			JS_SetOpaque(bone_position, &ro->m.skeleton->bones[i].position);

			JS_DefinePropertyValueStr(ctx, bone, "position", bone_position, JS_PROP_C_W_E);

			JSValue bone_rotation = JS_NewObjectClass(ctx, get_vector4_class_id());

			JS_SetOpaque(bone_rotation, &ro->m.skeleton->bones[i].rotation);

			JS_DefinePropertyValueStr(ctx, bone, "rotation", bone_rotation, JS_PROP_C_W_E);

			JSValue bone_scale = JS_NewObjectClass(ctx, get_vector4_class_id());

			JS_SetOpaque(bone_scale, &ro->m.skeleton->bones[i].scale);

			JS_DefinePropertyValueStr(ctx, bone, "scale", bone_scale, JS_PROP_C_W_E);

			JS_DefinePropertyValueUint32(ctx, bones, i, bone, JS_PROP_C_W_E);
		}

		JS_DefinePropertyValueStr(ctx, obj, "bones", bones, JS_PROP_C_W_E);
	}

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

static JSClassDef js_render_data_class = {
    "RenderData",
    .finalizer = athena_render_data_dtor,
}; 

static JSValue athena_rdfree(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	athena_render_data_dtor(JS_GetRuntime(ctx), this_val);

	return JS_UNDEFINED;
}

static JSValue athena_getpipeline(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSRenderData* ro = JS_GetOpaque2(ctx, this_val, js_render_data_class_id);
    if (!ro)
        return JS_EXCEPTION;

	return JS_NewUint32(ctx, ro->m.pipeline);
}

static JSValue athena_setpipeline(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
	eRenderPipelines pipeline;

    JSRenderData* ro = JS_GetOpaque2(ctx, this_val, js_render_data_class_id);
    if (!ro)
        return JS_EXCEPTION;

	JS_ToUint32(ctx, &pipeline, val);

	ro->m.pipeline = pipeline;

	return JS_UNDEFINED;
}

static JSValue athena_settexture(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t tex_idx;
	JSRenderData* ro = JS_GetOpaque2(ctx, this_val, js_render_data_class_id);

	JS_ToUint32(ctx, &tex_idx, argv[0]);

	if (ro->m.texture_count < (tex_idx+1)) {
		ro->m.textures = realloc(ro->m.textures, sizeof(GSSURFACE*)*(ro->m.texture_count+1));
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
	JSRenderData* ro = JS_GetOpaque2(ctx, this_val, js_render_data_class_id);

	JS_ToUint32(ctx, &tex_idx, argv[0]);

	if (ro->m.texture_count > tex_idx) {
		JS_DupValue(ctx, ro->textures[tex_idx]);
		return ro->textures[tex_idx];
	}

	return JS_UNDEFINED;
}

static JSValue athena_pushtexture(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSRenderData* ro = JS_GetOpaque2(ctx, this_val, js_render_data_class_id);

	uint32_t tex_idx = ro->m.texture_count++;

	ro->m.textures = realloc(ro->m.textures, sizeof(GSSURFACE*)*(ro->m.texture_count));
	ro->textures =   realloc(ro->textures,   sizeof(JSValue)*(ro->m.texture_count));
	ro->m.textures[tex_idx] = NULL;

	if (ro->m.textures[tex_idx]) {
		//JS_FreeValue(ctx, ro->textures[tex_idx]);
	}

	JS_DupValue(ctx, argv[0]);
	JSImageData* image = JS_GetOpaque2(ctx, argv[0], get_img_class_id());

	ro->textures[tex_idx] = argv[0];
	ro->m.textures[tex_idx] = image->tex;

	JSValue tex_arr = JS_GetPropertyStr(ctx, this_val, "textures");
	JS_DefinePropertyValueUint32(ctx, tex_arr, tex_idx, ro->textures[tex_idx], JS_PROP_C_W_E);
	JS_FreeValue(ctx, tex_arr);

	return JS_NewUint32(ctx, tex_idx);
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

static JSValue JS_NewMaterial(JSContext *ctx, VECTOR v) {
	JSValue vec = JS_NewObject(ctx);
	JS_DefinePropertyValueStr(ctx, vec, "r", JS_NewFloat32(ctx, v[0]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, vec, "g", JS_NewFloat32(ctx, v[1]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, vec, "b", JS_NewFloat32(ctx, v[2]), JS_PROP_C_W_E);

	return vec;
}

static void JS_ToMaterial(JSContext *ctx, VECTOR v, JSValue vec) {
	JS_ToFloat32(ctx, &v[0], JS_GetPropertyStr(ctx, vec, "r"));
	JS_ToFloat32(ctx, &v[1], JS_GetPropertyStr(ctx, vec, "g"));
	JS_ToFloat32(ctx, &v[2], JS_GetPropertyStr(ctx, vec, "b"));
	v[3] = 1.0f;

	JS_FreeValue(ctx, vec);
}

static JSValue js_render_data_get(JSContext *ctx, JSValueConst this_val, int magic) {
    JSRenderData* ro = JS_GetOpaque2(ctx, this_val, js_render_data_class_id);
    if (!ro)
        return JS_EXCEPTION;

	switch (magic) {
		case 0:
			{
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
			}
		case 1:
			{
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
					JS_DefinePropertyValueStr(ctx, obj, "ref_texture_id", JS_NewInt32(ctx, ro->m.materials[i].ref_texture_id), JS_PROP_C_W_E);
					JS_DefinePropertyValueStr(ctx, obj, "bump_texture_id", JS_NewInt32(ctx, ro->m.materials[i].bump_texture_id), JS_PROP_C_W_E);
					JS_DefinePropertyValueStr(ctx, obj, "decal_texture_id", JS_NewInt32(ctx, ro->m.materials[i].decal_texture_id), JS_PROP_C_W_E);

					JS_DefinePropertyValueUint32(ctx, arr, i, obj, JS_PROP_C_W_E);
				}

				return arr;
			}
		case 2:
			{
				JSValue arr = JS_NewArray(ctx);

				for (int i = 0; i < ro->m.material_index_count; i++) {
					JSValue obj = JS_NewObject(ctx);

					JS_DefinePropertyValueStr(ctx, obj, "index", JS_NewUint32(ctx, ro->m.material_indices[i].index), JS_PROP_C_W_E);
					JS_DefinePropertyValueStr(ctx, obj, "end",   JS_NewUint32(ctx, ro->m.material_indices[i].end),   JS_PROP_C_W_E);

					JS_DefinePropertyValueUint32(ctx, arr, i, obj, JS_PROP_C_W_E);
				}

				return arr;
			}
		case 3:
			return JS_NewBool(ctx,   ro->m.attributes.accurate_clipping);
		case 4:
			return JS_NewFloat32(ctx,   ro->m.attributes.face_culling);
		case 5:
			return JS_NewBool(ctx,   ro->m.attributes.texture_mapping);
		case 6:
			return JS_NewUint32(ctx,   ro->m.attributes.shade_model);
		case 7:
			return JS_NewUint32(ctx, ro->m.index_count);
		case 8:
			{
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
			break;
	}

	return JS_UNDEFINED;
}

static JSValue js_render_data_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    JSRenderData* ro = JS_GetOpaque2(ctx, this_val, js_render_data_class_id);
    if (!ro)
        return JS_EXCEPTION;

	switch (magic) {
		case 0:
			{
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

				FlushCache(WRITEBACK_DCACHE);
			}
			break;
		case 1:
			{
				uint32_t material_count = 0;
				int has_refmap = 0;

				JS_ToUint32(ctx, &material_count, JS_GetPropertyStr(ctx, val, "length"));

				if (material_count > ro->m.material_count) {
					ro->m.materials = realloc(ro->m.materials, material_count);
				}

				for (int i = 0; i < material_count; i++) {
					JSValue obj = JS_GetPropertyUint32(ctx, val, i);

					JS_ToMaterial(ctx, &ro->m.materials[i].ambient,             JS_GetPropertyStr(ctx, obj, "ambient"));
					JS_ToMaterial(ctx, &ro->m.materials[i].diffuse,             JS_GetPropertyStr(ctx, obj, "diffuse"));
					//JS_ToMaterial(ctx, &ro->m.materials[i].specular,            JS_GetPropertyStr(ctx, obj, "specular"));
					JS_ToMaterial(ctx, &ro->m.materials[i].emission,            JS_GetPropertyStr(ctx, obj, "emission"));
					JS_ToMaterial(ctx, &ro->m.materials[i].transmittance,       JS_GetPropertyStr(ctx, obj, "transmittance"));
					JS_ToFloat32(ctx,  &ro->m.materials[i].shininess,           JS_GetPropertyStr(ctx, obj, "shininess"));
					JS_ToFloat32(ctx,  &ro->m.materials[i].refraction,          JS_GetPropertyStr(ctx, obj, "refraction"));
					JS_ToMaterial(ctx, &ro->m.materials[i].transmission_filter, JS_GetPropertyStr(ctx, obj, "transmission_filter"));
					JS_ToFloat32(ctx,  &ro->m.materials[i].disolve,             JS_GetPropertyStr(ctx, obj, "disolve"));

					JS_ToInt32(ctx,    &ro->m.materials[i].texture_id,          JS_GetPropertyStr(ctx, obj, "texture_id"));
					JS_ToInt32(ctx,    &ro->m.materials[i].ref_texture_id,          JS_GetPropertyStr(ctx, obj, "ref_texture_id"));

					if (ro->m.materials[i].ref_texture_id != -1 && !ro->m.attributes.has_refmap) {
						ro->m.attributes.has_refmap = true;
					}

					JS_ToInt32(ctx,    &ro->m.materials[i].bump_texture_id,          JS_GetPropertyStr(ctx, obj, "bump_texture_id"));

					if (ro->m.materials[i].bump_texture_id != -1 && !ro->m.attributes.has_bumpmap) {
						ro->m.attributes.has_bumpmap = true;
					}

					JS_ToInt32(ctx,    &ro->m.materials[i].decal_texture_id,          JS_GetPropertyStr(ctx, obj, "decal_texture_id"));

					if (ro->m.materials[i].decal_texture_id != -1 && !ro->m.attributes.has_decal) {
						ro->m.attributes.has_decal = true;
					}

					JS_FreeValue(ctx, obj);
				}

				ro->m.material_count = material_count;

				FlushCache(WRITEBACK_DCACHE);
			}
			break;
		case 2:
			{
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

				FlushCache(WRITEBACK_DCACHE);
			}
			break;
		case 3:
			ro->m.attributes.accurate_clipping = JS_ToBool(ctx, val);
			break;
		case 4:
			JS_ToFloat32(ctx, &ro->m.attributes.face_culling, val);
			break;
		case 5:
			ro->m.attributes.texture_mapping = JS_ToBool(ctx, val);
			break;
		case 6:
			{
				uint32_t shade_model;
				JS_ToUint32(ctx, &shade_model, val);

				ro->m.attributes.shade_model = shade_model;
			}
			break;
		case 7:
			break;
		case 8:
			for (int i = 0; i < 8; i++) {
				JSValue vertex = JS_GetPropertyUint32(ctx, val, i);

				JS_ToFloat32(ctx, &ro->m.bounding_box[i][0], JS_GetPropertyStr(ctx, vertex, "x"));
				JS_ToFloat32(ctx, &ro->m.bounding_box[i][1], JS_GetPropertyStr(ctx, vertex, "y"));
				JS_ToFloat32(ctx, &ro->m.bounding_box[i][2], JS_GetPropertyStr(ctx, vertex, "z"));
				ro->m.bounding_box[i][3] = 1.0f;

				JS_FreeValue(ctx, vertex);
			}

			FlushCache(WRITEBACK_DCACHE);

			break;
	}

    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_render_data_proto_funcs[] = {
	JS_CFUNC_DEF("setTexture",  3,  athena_settexture),
	JS_CFUNC_DEF("getTexture",  1,  athena_gettexture),

	JS_CFUNC_DEF("pushTexture",  1,  athena_pushtexture),

	JS_CFUNC_DEF("free",  0,  athena_rdfree),

	JS_CGETSET_MAGIC_DEF("vertices",          js_render_data_get, js_render_data_set, 0),
	JS_CGETSET_MAGIC_DEF("materials",         js_render_data_get, js_render_data_set, 1),
	JS_CGETSET_MAGIC_DEF("material_indices",  js_render_data_get, js_render_data_set, 2),
	JS_CGETSET_MAGIC_DEF("accurate_clipping", js_render_data_get, js_render_data_set, 3),
	JS_CGETSET_MAGIC_DEF("face_culling",      js_render_data_get, js_render_data_set, 4),
	JS_CGETSET_MAGIC_DEF("texture_mapping",   js_render_data_get, js_render_data_set, 5),
	JS_CGETSET_MAGIC_DEF("shade_model",       js_render_data_get, js_render_data_set, 6),
	JS_CGETSET_MAGIC_DEF("size",              js_render_data_get, js_render_data_set, 7),
	JS_CGETSET_MAGIC_DEF("bounds",            js_render_data_get, js_render_data_set, 8),
	JS_CGETSET_MAGIC_DEF("pipeline",          athena_getpipeline, athena_setpipeline, 0),

};

JSClassID js_render_object_class_id;

static void athena_render_object_dtor(JSRuntime *rt, JSValue val){
	JSRenderObject* ro = JS_GetOpaque(val, js_render_object_class_id);

    if (!ro)
        return;

	if (ro->obj.bones) {
		free(ro->obj.bones);
		free(ro->obj.bone_matrices);
	}

	js_free_rt(rt, ro);
	
	JS_SetOpaque(val, NULL);
}

static JSValue athena_play_anim(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_render_object_class_id);

	athena_animation *anim = NULL;

	JS_ToUint32(ctx, &anim, argv[0]);

	ro->obj.anim_controller.current = anim;
	ro->obj.anim_controller.is_playing = false;
	ro->obj.anim_controller.loop = JS_ToBool(ctx, argv[1]);

	return JS_UNDEFINED;
}

static JSValue athena_is_playing(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_render_object_class_id);

	athena_animation *anim = NULL;

	if (argc > 0)
		JS_ToUint32(ctx, &anim, argv[0]);

	return JS_NewBool(ctx, (ro->obj.anim_controller.current && (ro->obj.anim_controller.current == anim)) );
}

static JSValue athena_render_object_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
	JSValue obj = JS_UNDEFINED;
    JSValue proto;

    JSRenderObject* ro = js_mallocz(ctx, sizeof(JSRenderObject));
	
    if (!ro)
        return JS_EXCEPTION;

    JSRenderData* rd = JS_GetOpaque2(ctx, argv[0], js_render_data_class_id);
    if (!rd)
        return JS_EXCEPTION;

	new_render_object(&ro->obj, &rd->m);

    JSValue transform_matrix = JS_UNDEFINED;

    transform_matrix = JS_NewObjectClass(ctx, get_matrix4_class_id());

    JS_SetOpaque(transform_matrix, &ro->obj.transform);

register_3d_object_data:
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    obj = JS_NewObjectProtoClass(ctx, proto, js_render_object_class_id);

	JS_DefinePropertyValueStr(ctx, obj, "transform", transform_matrix, JS_PROP_C_W_E);

	if (ro->obj.data->skin_data) {
		JSValue bone_transforms = JS_NewArray(ctx);
		JSValue bone_matrices = JS_NewArray(ctx);

		// TODO: create a class for bones so they can be passed by reference
		for (int i = 0; i < ro->obj.data->skeleton->bone_count; i++) {
			JSValue bone = JS_NewObject(ctx);

			JSValue bone_transform = JS_NewObjectClass(ctx, get_matrix4_class_id());

			JS_SetOpaque(bone_transform, &ro->obj.bones[i].transform);

			JS_DefinePropertyValueStr(ctx, bone, "transform", bone_transform, JS_PROP_C_W_E);

			JSValue bone_position = JS_NewObjectClass(ctx, get_vector4_class_id());

			JS_SetOpaque(bone_position, &ro->obj.bones[i].position);

			JS_DefinePropertyValueStr(ctx, bone, "position", bone_position, JS_PROP_C_W_E);

			JSValue bone_rotation = JS_NewObjectClass(ctx, get_vector4_class_id());

			JS_SetOpaque(bone_rotation, &ro->obj.bones[i].rotation);

			JS_DefinePropertyValueStr(ctx, bone, "rotation", bone_rotation, JS_PROP_C_W_E);

			JSValue bone_scale = JS_NewObjectClass(ctx, get_vector4_class_id());

			JS_SetOpaque(bone_scale, &ro->obj.bones[i].scale);

			JS_DefinePropertyValueStr(ctx, bone, "scale", bone_scale, JS_PROP_C_W_E);

			JS_DefinePropertyValueUint32(ctx, bone_transforms, i, bone, JS_PROP_C_W_E);

    		JSValue bone_matrix = JS_NewObjectClass(ctx, get_matrix4_class_id());

    		JS_SetOpaque(bone_matrix, &ro->obj.bone_matrices[i]);

			JS_DefinePropertyValueUint32(ctx, bone_matrices, i, bone_matrix, JS_PROP_C_W_E);
		}

		JS_DefinePropertyValueStr(ctx, obj, "bone_matrices", bone_matrices, JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, obj, "bones", bone_transforms, JS_PROP_C_W_E);

		JS_DefinePropertyValueStr(ctx, obj, "playAnim", JS_NewCFunction2(ctx, athena_play_anim, "playAnim", 2, JS_CFUNC_generic, 0), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, obj, "isPlayingAnim", JS_NewCFunction2(ctx, athena_is_playing, "isPlayingAnim", 1, JS_CFUNC_generic, 0), JS_PROP_C_W_E);
	}

    JS_FreeValue(ctx, proto);
    JS_SetOpaque(obj, ro);

    return obj;
}

static JSClassDef js_render_object_class = {
    "RenderObject",
    .finalizer = athena_render_object_dtor,
}; 

static JSValue athena_drawfree(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	athena_render_object_dtor(JS_GetRuntime(ctx), this_val);

	return JS_UNDEFINED;
}

static JSValue athena_drawobject(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_render_object_class_id);

	render_object(&ro->obj);

	return JS_UNDEFINED;
}

static JSValue athena_drawbbox(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Color color;

	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_render_object_class_id);

	JS_ToUint32(ctx, &color,  argv[0]);
	
	draw_bbox(&ro->obj, color);

	return JS_UNDEFINED;
}


static JSValue js_render_object_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_render_object_class_id);
    if (!ro)
        return JS_EXCEPTION;

	switch (magic) {
		case 0:
			{
				JSValue obj = JS_NewObject(ctx);

				JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, ro->obj.position[0]), JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, ro->obj.position[1]), JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, ro->obj.position[2]), JS_PROP_C_W_E);
				
				return obj;
			}
		case 1:
			{
				JSValue obj = JS_NewObject(ctx);

				JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, ro->obj.rotation[0]), JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, ro->obj.rotation[1]), JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, ro->obj.rotation[2]), JS_PROP_C_W_E);
				
				return obj;
			}
		case 2:
			{
				JSValue obj = JS_NewObject(ctx);

				JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, ro->obj.scale[0]), JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, ro->obj.scale[1]), JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, ro->obj.scale[2]), JS_PROP_C_W_E);
				
				return obj;
			}
	}

	return JS_UNDEFINED;
}

static JSValue js_render_object_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_render_object_class_id);
    if (!ro)
        return JS_EXCEPTION;

	switch (magic) {
		case 0:
			JS_ToFloat32(ctx, &ro->obj.position[0], JS_GetPropertyStr(ctx, val, "x"));
			JS_ToFloat32(ctx, &ro->obj.position[1], JS_GetPropertyStr(ctx, val, "y"));
			JS_ToFloat32(ctx, &ro->obj.position[2], JS_GetPropertyStr(ctx, val, "z"));
			break;
		case 1:
			JS_ToFloat32(ctx, &ro->obj.rotation[0], JS_GetPropertyStr(ctx, val, "x"));
			JS_ToFloat32(ctx, &ro->obj.rotation[1], JS_GetPropertyStr(ctx, val, "y"));
			JS_ToFloat32(ctx, &ro->obj.rotation[2], JS_GetPropertyStr(ctx, val, "z"));
			break;
		case 2:
			JS_ToFloat32(ctx, &ro->obj.scale[0], JS_GetPropertyStr(ctx, val, "x"));
			JS_ToFloat32(ctx, &ro->obj.scale[1], JS_GetPropertyStr(ctx, val, "y"));
			JS_ToFloat32(ctx, &ro->obj.scale[2], JS_GetPropertyStr(ctx, val, "z"));
			break;
	}

	update_object_space(&ro->obj);

    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_render_object_proto_funcs[] = {
    JS_CFUNC_DEF("render",        0,  athena_drawobject),
	JS_CFUNC_DEF("renderBounds",  0,    athena_drawbbox),
	JS_CFUNC_DEF("free",  0,    athena_drawfree),

	JS_CGETSET_MAGIC_DEF("position",          js_render_object_get, js_render_object_set, 0),
	JS_CGETSET_MAGIC_DEF("rotation",          js_render_object_get, js_render_object_set, 1),
	JS_CGETSET_MAGIC_DEF("scale",             js_render_object_get, js_render_object_set, 2),
};

static JSValue athena_r_init(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
  	render_init();
	return JS_UNDEFINED;
}

static JSValue athena_set_view(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	float fov = 60.0f, near = 1.0f, far = 2000.0f, width = 0.0f, height = 0.0f;

	if (argc > 0) {
		JS_ToFloat32(ctx, &fov, argv[0]);

		if (argc > 1) {
			JS_ToFloat32(ctx, &near, argv[1]);
			JS_ToFloat32(ctx, &far, argv[2]);

			if (argc > 3) {
				JS_ToFloat32(ctx, &width, argv[3]);
				JS_ToFloat32(ctx, &height, argv[4]);
			}
		}
	}

  	render_set_view(fov, near, far, width, height);
	return JS_UNDEFINED;
}

static JSValue athena_newmaterial(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	JSValue obj = JS_NewObject(ctx);

	JS_DefinePropertyValueStr(ctx, obj, "ambient",             argv[0], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "diffuse",             argv[1], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "specular",            argv[2], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "emission",            argv[3], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "transmittance",       argv[4], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "shininess",           argv[5], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "refraction",          argv[6], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "transmission_filter", argv[7], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "disolve",             argv[8], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "texture_id",          argv[9], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "bump_texture_id",          argv[10], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "ref_texture_id",          argv[11], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "decal_texture_id",          argv[12], JS_PROP_C_W_E);

	return obj;
}

static JSValue athena_newmaterialindex(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	JSValue obj = JS_NewObject(ctx);
	JS_DefinePropertyValueStr(ctx, obj, "index", argv[0], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "end",   argv[1], JS_PROP_C_W_E);

	return obj;
}

static JSValue athena_materialcolor(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	JSValue obj = JS_NewObject(ctx);
	JS_DefinePropertyValueStr(ctx, obj, "r", argv[0], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "g", argv[1], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "b", argv[2], JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "a", (argc > 3? argv[3] : JS_NewFloat32(ctx, 1.0f)), JS_PROP_C_W_E);

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
	JS_CFUNC_DEF( "init",            0,                athena_r_init),
    JS_CFUNC_DEF( "setView",         6,                athena_set_view),
	JS_CFUNC_DEF( "vertexList",      6,                 athena_newvertex),
	JS_CFUNC_DEF( "materialColor",   3,             athena_materialcolor),
	JS_CFUNC_DEF( "material",        0,               athena_newmaterial),
	JS_CFUNC_DEF( "materialIndex",   2,          athena_newmaterialindex),

	JS_PROP_INT32_DEF("PL_NO_LIGHTS",                       PL_NO_LIGHTS, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PL_DEFAULT",                           PL_DEFAULT, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PL_SPECULAR",                         PL_SPECULAR, JS_PROP_CONFIGURABLE),
	JS_PROP_FLOAT_DEF("CULL_FACE_NONE",                   CULL_FACE_NONE, JS_PROP_CONFIGURABLE),
	JS_PROP_FLOAT_DEF("CULL_FACE_BACK",                   CULL_FACE_BACK, JS_PROP_CONFIGURABLE),
	JS_PROP_FLOAT_DEF("CULL_FACE_FRONT",                 CULL_FACE_FRONT, JS_PROP_CONFIGURABLE),
};

static int render_module_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, render_funcs, countof(render_funcs));
}

static int js_render_data_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue render_data_proto, render_data_class;
    
    /* create the Point class */
    JS_NewClassID(&js_render_data_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_render_data_class_id, &js_render_data_class);

    render_data_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, render_data_proto, js_render_data_proto_funcs, countof(js_render_data_proto_funcs));
    
    render_data_class = JS_NewCFunction2(ctx, athena_render_data_ctor, "RenderData", 2, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, render_data_class, render_data_proto);
    JS_SetClassProto(ctx, js_render_data_class_id, render_data_proto);
                      
    JS_SetModuleExport(ctx, m, "RenderData", render_data_class);
    return 0;
}

static int js_render_object_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue render_object_proto, render_object_class;
    
    /* create the Point class */
    JS_NewClassID(&js_render_object_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_render_object_class_id, &js_render_object_class);

    render_object_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, render_object_proto, js_render_object_proto_funcs, countof(js_render_object_proto_funcs));
    
    render_object_class = JS_NewCFunction2(ctx, athena_render_object_ctor, "RenderObject", 1, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, render_object_class, render_object_proto);
    JS_SetClassProto(ctx, js_render_object_class_id, render_object_proto);
                      
    JS_SetModuleExport(ctx, m, "RenderObject", render_object_class);
    return 0;
}


JSModuleDef *athena_render_init(JSContext* ctx){
    JSModuleDef *m;
    m = JS_NewCModule(ctx, "RenderData", js_render_data_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "RenderData");

    m = JS_NewCModule(ctx, "RenderObject", js_render_object_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "RenderObject");

	return athena_push_module(ctx, render_module_init, render_funcs, countof(render_funcs), "Render");
}
