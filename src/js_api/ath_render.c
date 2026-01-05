#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>

#include <render.h>
#include <render_batch.h>
#include <render_scene.h>
#include <render_async_loader.h>
#include <stdio.h>
#include <ath_env.h>
#include <kernel.h>

static JSClassID js_render_data_class_id;
static JSClassID js_render_batch_class_id;
static JSClassID js_scene_node_class_id;
static JSClassID js_render_async_loader_class_id;
/* Removed unused ctor caches; classes are exported via their modules in ath_env */
static JSValue g_scene_node_ctor = JS_UNDEFINED;
static JSValue g_async_loader_ctor = JS_UNDEFINED;

typedef struct {
	athena_render_data m;
	JSValue *textures;
	JSValue vertex_buffers[4];
	bool owns_vertices[4];
} JSRenderData;

typedef struct {
    athena_batch *batch;
    JSValue *refs; // keep JS RenderObject alive
    uint32_t ref_count;
    uint32_t ref_capacity;
} JSRenderBatch;

typedef struct {
    JSValue *items;
    uint32_t count;
    uint32_t capacity;
} JSValueList;

typedef struct {
    athena_scene_node *node;
    JSValueList attachments; // JS refs
    JSValueList children;    // JS refs
    JSValue parent;
    JSContext *ctx;
} JSSceneNode;

typedef struct {
    athena_async_loader *native;              // cooperative single-thread
    uint32_t jobs_per_step;
} JSRenderAsyncLoader;

typedef struct LoaderThunk {
    JSContext *ctx;
    JSValue cb;
    char *path_c; // C string copy of path to avoid JS refcounting across threads/queues
} LoaderThunk;

static JSValue js_wrap_render_data(JSContext *ctx, athena_render_data *m);

// Single-threaded bridge: builds JS objects and frees the thunk here.
static void loader_bridge_cb(athena_render_data *data, void *user) {
    LoaderThunk *t = (LoaderThunk *)user;
    if (!t) return;
    JSRuntime *rt = JS_GetRuntime(t->ctx);
    JS_UpdateStackTop(rt);

    JSValue rd = js_wrap_render_data(t->ctx, data);
    if (!JS_IsException(rd)) {
        // Build a fresh JS string from C path to avoid refcount issues
        JSValue path_arg = JS_NewString(t->ctx, t->path_c ? t->path_c : "");
        JSValue args[2] = { path_arg, rd };
        JSValue ret = JS_Call(t->ctx, t->cb, JS_UNDEFINED, 2, args);
        JS_FreeValue(t->ctx, ret);
        JS_FreeValue(t->ctx, rd);
        JS_FreeValue(t->ctx, path_arg);
    }

    JS_FreeValue(t->ctx, t->cb);
    if (t->path_c) free(t->path_c);
    js_free(t->ctx, t);
}

// Worker-thread bridge: creates JS objects and calls callback; thunk is freed
// by process_wk via cleanup after callback completes (do not free here).
/* worker variant removed */

// Multithreaded bridge: must not free the thunk here; process_mt() will do it
// on the main thread after callback returns to avoid races with clear/destroy.
// MT variant removed; we only use the cooperative single-thread bridge above.

// user cleanup for pending jobs (clear/destroy)
// Cleanup when running on the JS thread (have a valid JSContext)
static void loader_thunk_cleanup_js(void *user) {
    if (!user) return;
    LoaderThunk *t = (LoaderThunk *)user;
    if (t->ctx) {
        JS_FreeValue(t->ctx, t->cb);
        if (t->path_c) free(t->path_c);
        js_free(t->ctx, t);
    } else {
        free(t);
    }
}

// Cleanup when only a JSRuntime is safe to use (e.g., GC finalizer)
static void loader_thunk_cleanup_rt(void *user) {
    if (!user) return;
    LoaderThunk *t = (LoaderThunk *)user;
    if (t->ctx) {
        JSRuntime *rt = JS_GetRuntime(t->ctx);
        JS_FreeValueRT(rt, t->cb);
        if (t->path_c) free(t->path_c);
        js_free_rt(rt, t);
    } else {
        free(t);
    }
}

static inline float clampf(float value, float min, float max) {
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

static inline bool js_value_is_same_object(JSValueConst a, JSValueConst b) {
	if (JS_VALUE_GET_TAG(a) != JS_TAG_OBJECT || JS_VALUE_GET_TAG(b) != JS_TAG_OBJECT)
		return false;
	return JS_VALUE_GET_OBJ(a) == JS_VALUE_GET_OBJ(b);
}

static void js_value_list_init(JSValueList *list) {
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static void js_value_list_free(JSContext *ctx, JSValueList *list) {
	for (uint32_t i = 0; i < list->count; i++)
		JS_FreeValue(ctx, list->items[i]);
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->capacity = 0;
}

static int js_value_list_reserve(JSContext *ctx, JSValueList *list, uint32_t extra) {
	uint32_t needed = list->count + extra;
	if (needed <= list->capacity)
		return 0;
	uint32_t new_cap = list->capacity? list->capacity * 2 : 4;
	while (new_cap < needed)
		new_cap *= 2;
	JSValue *items = realloc(list->items, sizeof(JSValue) * new_cap);
	if (!items)
		return -1;
	list->items = items;
	list->capacity = new_cap;
	return 0;
}

static int js_value_list_push(JSContext *ctx, JSValueList *list, JSValue value) {
	if (js_value_list_reserve(ctx, list, 1) != 0)
		return -1;
	list->items[list->count++] = JS_DupValue(ctx, value);
	return 0;
}

static void js_value_list_remove(JSContext *ctx, JSValueList *list, JSValue value) {
	int32_t idx = -1;
	for (uint32_t i = 0; i < list->count; i++) {
		if (js_value_is_same_object(list->items[i], value)) {
			idx = (int32_t)i;
			break;
		}
	}
	if (idx < 0)
		return;
	JS_FreeValue(ctx, list->items[idx]);
	memmove(&list->items[idx], &list->items[idx+1], (list->count-idx-1)*sizeof(JSValue));
	list->count--;
}

static void athena_render_data_dtor(JSRuntime *rt, JSValue val){
	JSRenderData* ro = JS_GetOpaque(val, js_render_data_class_id);

	if (!ro)
		return;

	if (ro->m.indices)
		free(ro->m.indices); 

	VECTOR **attribute_ptrs[] = {
		&ro->m.positions,
		&ro->m.normals,
		&ro->m.texcoords,
		&ro->m.colours
	};

	for (int i = 0; i < 4; i++) {
		if (*attribute_ptrs[i] && ro->owns_vertices[i]) {
			free(*attribute_ptrs[i]);
		}

		if (!JS_IsUndefined(ro->vertex_buffers[i])) {
			JS_FreeValueRT(rt, ro->vertex_buffers[i]);
		}
	}
	
	if (ro->m.materials)
		free(ro->m.materials);
	
	if (ro->m.material_indices)
		free(ro->m.material_indices);

	if (ro->m.skin_data)
		free(ro->m.skin_data);

	if (ro->m.skeleton) {
		if (ro->m.skeleton->bones)
			free(ro->m.skeleton->bones);

		free(ro->m.skeleton);
	}

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

	for (int i = 0; i < 4; i++) {
		ro->vertex_buffers[i] = JS_UNDEFINED;
		ro->owns_vertices[i] = true;
	}

	if (JS_IsObject(argv[0])) {
		memset(ro, 0, sizeof(JSRenderData));

		JSValue vert_arr, ta_buf;
		bool share_buffers = false;
		
		VECTOR** attributes_ptr[] = {
			&ro->m.positions,
			&ro->m.normals,
			&ro->m.texcoords,
			&ro->m.colours
		};

		JSValue share_prop = JS_GetPropertyStr(ctx, argv[0], "shareBuffers");
		if (!JS_IsUndefined(share_prop))
			share_buffers = JS_ToBool(ctx, share_prop);
		JS_FreeValue(ctx, share_prop);

		for (int i = 0; i < 4; i++) {
			vert_arr = JS_GetPropertyStr(ctx, argv[0], vert_attributes[i]);
			if (JS_IsUndefined(vert_arr))
				continue;

			size_t byte_offset = 0;
			size_t byte_length = 0;
			size_t bytes_per_element = 0;

			ta_buf = JS_GetTypedArrayBuffer(ctx, vert_arr, &byte_offset, &byte_length, &bytes_per_element);

			uint8_t *buffer_ptr = NULL;
			size_t data_size = 0;
			JSValue data_ref = JS_UNDEFINED;

			if (!JS_IsException(ta_buf)) {
				size_t backing_size = 0;
				uint8_t *backing = JS_GetArrayBuffer(ctx, &backing_size, ta_buf);
				if (backing && byte_offset + byte_length <= backing_size) {
					buffer_ptr = backing + byte_offset;
					data_size = byte_length;
					data_ref = ta_buf;
				}
			} else {
				buffer_ptr = JS_GetArrayBuffer(ctx, &data_size, vert_arr);
				data_ref = vert_arr;
			}

			if (buffer_ptr && data_size) {
				if (!i)
					ro->m.index_count = data_size/sizeof(VECTOR);

				if (share_buffers && !JS_IsUndefined(data_ref)) {
					*attributes_ptr[i] = (VECTOR*)buffer_ptr;
					ro->owns_vertices[i] = false;
					ro->vertex_buffers[i] = JS_DupValue(ctx, data_ref);
				} else {
					*attributes_ptr[i] = malloc(data_size);
					memcpy(*attributes_ptr[i], buffer_ptr, data_size);
					ro->owns_vertices[i] = true;
				}
			}

			if (!JS_IsException(ta_buf))
				JS_FreeValue(ctx, ta_buf);

			JS_FreeValue(ctx, vert_arr);
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

// Create a JS RenderData wrapping an existing athena_render_data*. Ownership of
// the struct is transferred to the JS object (internal pointers freed by dtor).
static JSValue js_wrap_render_data(JSContext *ctx, athena_render_data *m) {
    JSRenderData* ro = js_mallocz(ctx, sizeof(JSRenderData));
    if (!ro) {
        free(m);
        return JS_EXCEPTION;
    }

    // move the struct into JS wrapper (shallow move)
    memcpy(&ro->m, m, sizeof(athena_render_data));
    free(m);

    JSValue obj = JS_NewObjectClass(ctx, js_render_data_class_id);
    if (JS_IsException(obj)) {
        // manual cleanup mirroring destructor
        if (ro->m.indices) free(ro->m.indices);
        if (ro->m.positions) free(ro->m.positions);
        if (ro->m.colours) free(ro->m.colours);
        if (ro->m.normals) free(ro->m.normals);
        if (ro->m.texcoords) free(ro->m.texcoords);
        if (ro->m.materials) free(ro->m.materials);
        if (ro->m.material_indices) free(ro->m.material_indices);
        if (ro->m.skin_data) free(ro->m.skin_data);
        if (ro->m.skeleton) {
            if (ro->m.skeleton->bones) free(ro->m.skeleton->bones);
            free(ro->m.skeleton);
        }
        if (ro->m.textures) free(ro->m.textures);
        js_free(ctx, ro);
        return JS_EXCEPTION;
    }

    // Ensure sane defaults like the RenderData constructor
    ro->m.pipeline = PL_DEFAULT;
    ro->m.attributes.accurate_clipping = 1;
    ro->m.attributes.face_culling = CULL_FACE_BACK;
    ro->m.attributes.texture_mapping = 1;
    ro->m.attributes.shade_model = 1; // gouraud

    // Wrap native textures into JS Image objects for 'textures' array
    if (ro->m.texture_count > 0 && ro->m.textures) {
        ro->textures = malloc(sizeof(JSValue)*ro->m.texture_count);
        JSValue tex_arr = JS_NewArray(ctx);
        for (int i = 0; i < ro->m.texture_count; i++) {
            JSImageData* image = js_mallocz(ctx, sizeof(*image));
            JSValue img_obj = JS_NewObjectClass(ctx, get_img_class_id());
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
            JS_SetOpaque(img_obj, image);
            ro->textures[i] = img_obj;
            JS_DefinePropertyValueUint32(ctx, tex_arr, i, img_obj, JS_PROP_C_W_E);
        }
        JS_DefinePropertyValueStr(ctx, obj, "textures", tex_arr, JS_PROP_C_W_E);
    }

    ro->m.tristrip = ro->m.tristrip;
    JS_SetOpaque(obj, ro);

    // Flush dcache so VIF/DMAC see coherent vertex/material data
    FlushCache(WRITEBACK_DCACHE);
    return obj;
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

static void js_apply_color_prop(JSContext *ctx, JSValue obj, const char *name, VECTOR color) {
	JSValue prop = JS_GetPropertyStr(ctx, obj, name);
	if (!JS_IsObject(prop)) {
		JS_FreeValue(ctx, prop);
		return;
	}

	float r = 0, g = 0, b = 0;
	JSValue cr = JS_GetPropertyStr(ctx, prop, "r");
	JSValue cg = JS_GetPropertyStr(ctx, prop, "g");
	JSValue cb = JS_GetPropertyStr(ctx, prop, "b");

	if (!JS_IsUndefined(cr)) JS_ToFloat32(ctx, &r, cr);
	if (!JS_IsUndefined(cg)) JS_ToFloat32(ctx, &g, cg);
	if (!JS_IsUndefined(cb)) JS_ToFloat32(ctx, &b, cb);

	color[0] = clampf(r, 0.0f, 1.0f);
	color[1] = clampf(g, 0.0f, 1.0f);
	color[2] = clampf(b, 0.0f, 1.0f);
	color[3] = 1.0f;

	JS_FreeValue(ctx, cr);
	JS_FreeValue(ctx, cg);
	JS_FreeValue(ctx, cb);
	JS_FreeValue(ctx, prop);
}

static JSValue athena_renderdata_update_material(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSRenderData* ro = JS_GetOpaque2(ctx, this_val, js_render_data_class_id);
	if (!ro)
		return JS_EXCEPTION;

	if (argc < 2 || !JS_IsNumber(argv[0]) || !JS_IsObject(argv[1]))
		return JS_ThrowTypeError(ctx, "expected (index, object)");

	uint32_t index = 0;
	JS_ToUint32(ctx, &index, argv[0]);
	if (index >= ro->m.material_count)
		return JS_ThrowRangeError(ctx, "material index out of range");

	JSValue props = argv[1];
	ath_mat *mat = &ro->m.materials[index];

	js_apply_color_prop(ctx, props, "ambient", mat->ambient);
	js_apply_color_prop(ctx, props, "diffuse", mat->diffuse);
	js_apply_color_prop(ctx, props, "specular", mat->specular);
	js_apply_color_prop(ctx, props, "emission", mat->emission);
	js_apply_color_prop(ctx, props, "transmittance", mat->transmittance);
	js_apply_color_prop(ctx, props, "transmission_filter", mat->transmission_filter);

	JSValue shininess = JS_GetPropertyStr(ctx, props, "shininess");
	if (!JS_IsUndefined(shininess)) {
		float value = mat->shininess;
		JS_ToFloat32(ctx, &value, shininess);
		mat->shininess = clampf(value, 0.0f, 255.0f);
	}
	JS_FreeValue(ctx, shininess);

	JSValue refraction = JS_GetPropertyStr(ctx, props, "refraction");
	if (!JS_IsUndefined(refraction)) {
		float value = mat->refraction;
		JS_ToFloat32(ctx, &value, refraction);
		mat->refraction = clampf(value, 0.1f, 4.0f);
	}
	JS_FreeValue(ctx, refraction);

	JSValue dissolve = JS_GetPropertyStr(ctx, props, "disolve");
	if (!JS_IsUndefined(dissolve)) {
		float value = mat->disolve;
		JS_ToFloat32(ctx, &value, dissolve);
		mat->disolve = clampf(value, 0.0f, 1.0f);
	}
	JS_FreeValue(ctx, dissolve);

	JSValue bump_scale = JS_GetPropertyStr(ctx, props, "bump_scale");
	if (!JS_IsUndefined(bump_scale)) {
		float value = mat->bump_scale;
		JS_ToFloat32(ctx, &value, bump_scale);
		mat->bump_scale = clampf(value, -10.0f, 10.0f);
	}
	JS_FreeValue(ctx, bump_scale);

	FlushCache(WRITEBACK_DCACHE);

	return JS_NewUint32(ctx, index);
}

// Clone RenderData: shares geometry buffers but creates independent materials
static JSValue athena_renderdata_clone(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	JSRenderData* src = JS_GetOpaque2(ctx, this_val, js_render_data_class_id);
	if (!src)
		return JS_EXCEPTION;

	JSRenderData* ro = js_mallocz(ctx, sizeof(JSRenderData));
	if (!ro)
		return JS_EXCEPTION;

	// Share geometry buffers (reference same memory)
	ro->m.index_count = src->m.index_count;
	ro->m.indices = src->m.indices; // shared
	ro->m.positions = src->m.positions; // shared
	ro->m.normals = src->m.normals; // shared
	ro->m.texcoords = src->m.texcoords; // shared
	ro->m.colours = src->m.colours; // shared
	ro->m.skin_data = src->m.skin_data; // shared
	ro->m.skeleton = src->m.skeleton; // shared
	ro->m.tristrip = src->m.tristrip;

	// Mark as not owning vertices (prevent double-free)
	for (int i = 0; i < 4; i++) {
		ro->owns_vertices[i] = false;
		ro->vertex_buffers[i] = JS_UNDEFINED;
	}

	// Clone materials (independent copy)
	if (src->m.material_count > 0) {
		ro->m.material_count = src->m.material_count;
		ro->m.materials = malloc(sizeof(ath_mat) * ro->m.material_count);
		memcpy(ro->m.materials, src->m.materials, sizeof(ath_mat) * ro->m.material_count);
	}

	// Clone material indices (independent copy)
	if (src->m.material_index_count > 0) {
		ro->m.material_index_count = src->m.material_index_count;
		ro->m.material_indices = malloc(sizeof(material_index) * ro->m.material_index_count);
		memcpy(ro->m.material_indices, src->m.material_indices, sizeof(material_index) * ro->m.material_index_count);
	}

	// Share textures (reference same pointers)
	ro->m.texture_count = src->m.texture_count;
	if (src->m.texture_count > 0) {
		ro->m.textures = malloc(sizeof(GSSURFACE*) * ro->m.texture_count);
		memcpy(ro->m.textures, src->m.textures, sizeof(GSSURFACE*) * ro->m.texture_count);

		ro->textures = malloc(sizeof(JSValue) * ro->m.texture_count);
		for (int i = 0; i < ro->m.texture_count; i++) {
			ro->textures[i] = JS_DupValue(ctx, src->textures[i]);
		}
	}

	// Copy bounding box
	memcpy(ro->m.bounding_box, src->m.bounding_box, sizeof(ro->m.bounding_box));

	// Copy attributes and pipeline
	ro->m.pipeline = src->m.pipeline;
	ro->m.attributes = src->m.attributes;

	// Create JS object
	JSValue obj = JS_NewObjectClass(ctx, js_render_data_class_id);
	if (JS_IsException(obj)) {
		if (ro->m.materials) free(ro->m.materials);
		if (ro->m.material_indices) free(ro->m.material_indices);
		if (ro->m.textures) free(ro->m.textures);
		if (ro->textures) {
			for (int i = 0; i < ro->m.texture_count; i++)
				JS_FreeValue(ctx, ro->textures[i]);
			free(ro->textures);
		}
		js_free(ctx, ro);
		return JS_EXCEPTION;
	}

	// Keep reference to source to prevent geometry from being freed
	JS_DefinePropertyValueStr(ctx, obj, "__source", JS_DupValue(ctx, this_val), 0);

	JS_SetOpaque(obj, ro);
	return obj;
}

static const JSCFunctionListEntry js_render_data_proto_funcs[] = {
	JS_CFUNC_DEF("setTexture",  3,  athena_settexture),
	JS_CFUNC_DEF("getTexture",  1,  athena_gettexture),

	JS_CFUNC_DEF("pushTexture",  1,  athena_pushtexture),

	JS_CFUNC_DEF("free",  0,  athena_rdfree),
	JS_CFUNC_DEF("dispose",  0,  athena_rdfree),

	JS_CFUNC_DEF("clone",  0,  athena_renderdata_clone),

	JS_CFUNC_DEF("updateMaterial",  2,  athena_renderdata_update_material),

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
    if (!rd) {
        js_free(ctx, ro);
        return JS_EXCEPTION;
    }

	new_render_object(&ro->obj, &rd->m);

    JSValue transform_matrix = JS_UNDEFINED;

    transform_matrix = JS_NewObjectClass(ctx, get_matrix4_class_id());

    JS_SetOpaque(transform_matrix, &ro->obj.transform);

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

    // Keep a strong reference to the source RenderData to prevent premature GC
    JS_DefinePropertyValueStr(ctx, obj, "__render_data", JS_DupValue(ctx, argv[0]), JS_PROP_C_W_E);

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

#ifdef ATHENA_ODE
static JSValue athena_ro_collision(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_render_object_class_id);

	if (!JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
		JSGeom *geom = JS_GetOpaque(argv[0], js_geom_class_id);
		ro->obj.collision = geom->geom;
		ro->obj.update_collision = updateGeomPosRot;

		update_object_space(&ro->obj);
		
	} else {
		ro->obj.collision = NULL;
		ro->obj.update_collision = NULL;
	}

	return JS_UNDEFINED;
}

static JSValue athena_ro_physics(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_render_object_class_id);

	if (!JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
		JSBody *body = JS_GetOpaque(argv[0], js_body_class_id);
		ro->obj.physics = body->body;
		ro->obj.update_physics = updateBodyPosRot;

		update_object_space(&ro->obj);
	} else {
		ro->obj.physics = NULL;
		ro->obj.update_physics = NULL;
	}

	return JS_UNDEFINED;
}
#endif

static JSValue athena_drawbbox(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    Color color;

    JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_render_object_class_id);

	{
		uint32_t c32 = 0;
		JS_ToUint32(ctx, &c32, argv[0]);
		color = (Color)c32;
	}
	
	draw_bbox(&ro->obj, color);

	return JS_UNDEFINED;
}


static JSValue js_render_object_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_render_object_class_id);
    if (!ro) {
        return JS_EXCEPTION;
    }

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
    if (!ro) {
        return JS_EXCEPTION;
    }

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

// Get bone transform by name
static JSValue athena_ro_get_bone_transform(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	JSRenderObject* ro = JS_GetOpaque2(ctx, this_val, js_render_object_class_id);
	if (!ro)
		return JS_EXCEPTION;

	if (!ro->obj.data || !ro->obj.data->skeleton || !ro->obj.bones)
		return JS_UNDEFINED;

	const char *name = JS_ToCString(ctx, argv[0]);
	if (!name)
		return JS_EXCEPTION;

	athena_skeleton *skeleton = ro->obj.data->skeleton;
	int bone_id = -1;

	for (int i = 0; i < skeleton->bone_count; i++) {
		if (strcmp(skeleton->bones[i].name, name) == 0) {
			bone_id = i;
			break;
		}
	}

	JS_FreeCString(ctx, name);

	if (bone_id < 0)
		return JS_UNDEFINED;

	// Return bone transform info
	JSValue result = JS_NewObject(ctx);

	// World-space transform matrix
	JSValue bone_matrix = JS_NewObjectClass(ctx, get_matrix4_class_id());
	JS_SetOpaque(bone_matrix, &ro->obj.bone_matrices[bone_id]);
	JS_DefinePropertyValueStr(ctx, result, "matrix", bone_matrix, JS_PROP_C_W_E);

	// Local-space position
	JSValue pos = JS_NewObject(ctx);
	JS_DefinePropertyValueStr(ctx, pos, "x", JS_NewFloat32(ctx, ro->obj.bones[bone_id].position[0]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, pos, "y", JS_NewFloat32(ctx, ro->obj.bones[bone_id].position[1]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, pos, "z", JS_NewFloat32(ctx, ro->obj.bones[bone_id].position[2]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, result, "position", pos, JS_PROP_C_W_E);

	// Local-space rotation (quaternion)
	JSValue rot = JS_NewObject(ctx);
	JS_DefinePropertyValueStr(ctx, rot, "x", JS_NewFloat32(ctx, ro->obj.bones[bone_id].rotation[0]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, rot, "y", JS_NewFloat32(ctx, ro->obj.bones[bone_id].rotation[1]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, rot, "z", JS_NewFloat32(ctx, ro->obj.bones[bone_id].rotation[2]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, rot, "w", JS_NewFloat32(ctx, ro->obj.bones[bone_id].rotation[3]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, result, "rotation", rot, JS_PROP_C_W_E);

	// Bone index
	JS_DefinePropertyValueStr(ctx, result, "index", JS_NewInt32(ctx, bone_id), JS_PROP_C_W_E);

	return result;
}

static const JSCFunctionListEntry js_render_object_proto_funcs[] = {
    JS_CFUNC_DEF("render",        0,  athena_drawobject),
	JS_CFUNC_DEF("renderBounds",  0,    athena_drawbbox),
	JS_CFUNC_DEF("free",  0,    athena_drawfree),
	JS_CFUNC_DEF("dispose",  0,    athena_drawfree),

	JS_CFUNC_DEF("getBoneTransform",  1,  athena_ro_get_bone_transform),

	#ifdef ATHENA_ODE
	JS_CFUNC_DEF("setCollision",        1,  athena_ro_collision),
	JS_CFUNC_DEF("setPhysics",        1,  athena_ro_physics),
	#endif

	JS_CGETSET_MAGIC_DEF("position",          js_render_object_get, js_render_object_set, 0),
	JS_CGETSET_MAGIC_DEF("rotation",          js_render_object_get, js_render_object_set, 1),
	JS_CGETSET_MAGIC_DEF("scale",             js_render_object_get, js_render_object_set, 2)
};

typedef struct {
	JSRenderObject *ro;
} RenderBatchEntry;

static void render_batch_free(JSRuntime *rt, JSValue val) {
    JSRenderBatch *jb = JS_GetOpaque(val, js_render_batch_class_id);
    if (!jb)
        return;

    for (uint32_t i = 0; i < jb->ref_count; i++) {
        JS_FreeValueRT(rt, jb->refs[i]);
    }
    free(jb->refs);
    if (jb->batch)
        athena_batch_destroy(jb->batch);
    js_free_rt(rt, jb);
    JS_SetOpaque(val, NULL);
}

static int render_batch_ensure(JSContext *ctx, JSRenderBatch *batch, uint32_t extra) {
    uint32_t needed = batch->ref_count + extra;
    if (needed <= batch->ref_capacity)
        return 0;
    uint32_t new_cap = batch->ref_capacity? batch->ref_capacity * 2 : 8;
    while (new_cap < needed)
        new_cap *= 2;

    JSValue *new_items = realloc(batch->refs, sizeof(JSValue) * new_cap);
    if (!new_items)
        return -1;
    batch->refs = new_items;
    batch->ref_capacity = new_cap;
    return 0;
}

static JSValue athena_render_batch_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSRenderBatch *jb = js_mallocz(ctx, sizeof(JSRenderBatch));
    if (!jb)
        return JS_EXCEPTION;

    jb->batch = athena_batch_create();
    if (!jb->batch) {
        js_free(ctx, jb);
        return JS_EXCEPTION;
    }

    int auto_sort = 1;
    if (argc > 0 && JS_IsObject(argv[0])) {
        JSValue auto_sort_val = JS_GetPropertyStr(ctx, argv[0], "autoSort");
        if (!JS_IsUndefined(auto_sort_val))
            auto_sort = JS_ToBool(ctx, auto_sort_val);
        JS_FreeValue(ctx, auto_sort_val);
    }
    athena_batch_set_sort(jb->batch, NULL, auto_sort);

    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    JSValue obj = JS_NewObjectProtoClass(ctx, proto, js_render_batch_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj)) {
        athena_batch_destroy(jb->batch);
        js_free(ctx, jb);
        return obj;
    }

    JS_SetOpaque(obj, jb);
    return obj;
}

static JSValue athena_render_batch_add(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSRenderBatch *jb = JS_GetOpaque2(ctx, this_val, js_render_batch_class_id);
    if (!jb)
        return JS_EXCEPTION;
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "expected RenderObject");

    JSRenderObject* ro = JS_GetOpaque2(ctx, argv[0], js_render_object_class_id);
    if (!ro)
        return JS_EXCEPTION;

    if (athena_batch_add(jb->batch, &ro->obj) < 0)
        return JS_ThrowInternalError(ctx, "batch add failed");

    if (render_batch_ensure(ctx, jb, 1) != 0)
        return JS_ThrowInternalError(ctx, "out of memory");
    jb->refs[jb->ref_count++] = JS_DupValue(ctx, argv[0]);
    return JS_NewUint32(ctx, jb->batch->count);
}

static JSValue athena_render_batch_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSRenderBatch *jb = JS_GetOpaque2(ctx, this_val, js_render_batch_class_id);
    if (!jb)
        return JS_EXCEPTION;

    for (uint32_t i = 0; i < jb->ref_count; i++) {
        JS_FreeValue(ctx, jb->refs[i]);
    }
    jb->ref_count = 0;
    athena_batch_clear(jb->batch);
    return JS_UNDEFINED;
}

/* render_batch_compare no longer used (sorting handled in C core) */

static JSValue athena_render_batch_render(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSRenderBatch *jb = JS_GetOpaque2(ctx, this_val, js_render_batch_class_id);
    if (!jb)
        return JS_EXCEPTION;
    unsigned int n = athena_batch_render(jb->batch);
    return JS_NewUint32(ctx, n);
}

static JSValue athena_render_batch_size(JSContext *ctx, JSValueConst this_val, int magic) {
    JSRenderBatch *jb = JS_GetOpaque2(ctx, this_val, js_render_batch_class_id);
    if (!jb)
        return JS_EXCEPTION;
    return JS_NewUint32(ctx, jb->batch->count);
}

static const JSCFunctionListEntry js_render_batch_proto_funcs[] = {
	JS_CFUNC_DEF("add", 1, athena_render_batch_add),
	JS_CFUNC_DEF("clear", 0, athena_render_batch_clear),
	JS_CFUNC_DEF("render", 0, athena_render_batch_render),
	JS_CGETSET_MAGIC_DEF("size", athena_render_batch_size, NULL, 0),
};

static JSClassDef js_render_batch_class = {
	"RenderBatch",
	.finalizer = render_batch_free,
};

static void scene_node_free(JSRuntime *rt, JSValue val) {
	JSSceneNode *node = JS_GetOpaque(val, js_scene_node_class_id);
	if (!node)
		return;

	JSContext *ctx = node->ctx;
	if (ctx) {
		js_value_list_free(ctx, &node->attachments);
		js_value_list_free(ctx, &node->children);
		JS_FreeValue(ctx, node->parent);
	} else {
		for (uint32_t i = 0; i < node->attachments.count; i++)
			JS_FreeValueRT(rt, node->attachments.items[i]);
		free(node->attachments.items);
		for (uint32_t i = 0; i < node->children.count; i++)
			JS_FreeValueRT(rt, node->children.items[i]);
		free(node->children.items);
		JS_FreeValueRT(rt, node->parent);
	}

	if (node->node)
		athena_scene_node_destroy(node->node);
	js_free_rt(rt, node);
	JS_SetOpaque(val, NULL);
}

static void scene_node_init_transform(JSSceneNode *node) {
	node->node->position[0] = 0.0f;
	node->node->position[1] = 0.0f;
	node->node->position[2] = 0.0f;
	node->node->position[3] = 1.0f;
	node->node->rotation[0] = 0.0f;
	node->node->rotation[1] = 0.0f;
	node->node->rotation[2] = 0.0f;
	node->node->rotation[3] = 1.0f;
	node->node->scale[0] = 1.0f;
	node->node->scale[1] = 1.0f;
	node->node->scale[2] = 1.0f;
	node->node->scale[3] = 1.0f;
}

static JSValue athena_scene_node_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
	JSSceneNode *node = js_mallocz(ctx, sizeof(JSSceneNode));
	if (!node)
		return JS_EXCEPTION;

    node->ctx = ctx;
    node->node = athena_scene_node_create();
    if (!node->node) {
        js_free(ctx, node);
        return JS_EXCEPTION;
    }
    js_value_list_init(&node->attachments);
	js_value_list_init(&node->children);
	node->parent = JS_UNDEFINED;
	scene_node_init_transform(node);

	JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	JSValue obj = JS_NewObjectProtoClass(ctx, proto, js_scene_node_class_id);
	JS_FreeValue(ctx, proto);
	if (JS_IsException(obj)) {
		js_free(ctx, node);
		return obj;
	}

	JS_SetOpaque(obj, node);
	return obj;
}

static void scene_node_detach_parent(JSContext *ctx, JSSceneNode *node, JSValue node_val) {
	if (JS_IsUndefined(node->parent))
		return;
    JSSceneNode *parent = JS_GetOpaque(node->parent, js_scene_node_class_id);
    if (parent) {
        js_value_list_remove(ctx, &parent->children, node_val);
        if (parent->node)
            athena_scene_node_remove_child(parent->node, node->node);
    }
	JS_FreeValue(ctx, node->parent);
	node->parent = JS_UNDEFINED;
}

static JSValue js_scene_node_add_child(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSSceneNode *node = JS_GetOpaque2(ctx, this_val, js_scene_node_class_id);
	if (!node)
		return JS_EXCEPTION;
	if (argc < 1)
		return JS_ThrowTypeError(ctx, "expected SceneNode");

	JSSceneNode *child = JS_GetOpaque2(ctx, argv[0], js_scene_node_class_id);
	if (!child)
		return JS_EXCEPTION;
	if (node == child)
		return JS_ThrowRangeError(ctx, "cannot parent node to itself");

	scene_node_detach_parent(ctx, child, argv[0]);

    if (js_value_list_push(ctx, &node->children, argv[0]) != 0)
        return JS_ThrowInternalError(ctx, "out of memory");

    athena_scene_node_add_child(node->node, child->node);
    child->parent = JS_DupValue(ctx, this_val);
	return JS_NewUint32(ctx, node->children.count);
}

static JSValue js_scene_node_remove_child(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSSceneNode *node = JS_GetOpaque2(ctx, this_val, js_scene_node_class_id);
	if (!node)
		return JS_EXCEPTION;
	if (argc < 1)
		return JS_ThrowTypeError(ctx, "expected SceneNode");

	JSSceneNode *child = JS_GetOpaque2(ctx, argv[0], js_scene_node_class_id);
	if (!child)
		return JS_EXCEPTION;

	js_value_list_remove(ctx, &node->children, argv[0]);
    if (js_value_is_same_object(child->parent, this_val)) {
        JS_FreeValue(ctx, child->parent);
        child->parent = JS_UNDEFINED;
    }
    athena_scene_node_remove_child(node->node, child->node);
    return JS_NewUint32(ctx, node->children.count);
}

static JSValue js_scene_node_attach(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSSceneNode *node = JS_GetOpaque2(ctx, this_val, js_scene_node_class_id);
	if (!node)
		return JS_EXCEPTION;
	if (argc < 1)
		return JS_ThrowTypeError(ctx, "expected RenderObject");

	JSRenderObject *ro = JS_GetOpaque2(ctx, argv[0], js_render_object_class_id);
	if (!ro)
		return JS_EXCEPTION;

	js_value_list_remove(ctx, &node->attachments, argv[0]);
	if (js_value_list_push(ctx, &node->attachments, argv[0]) != 0)
		return JS_ThrowInternalError(ctx, "out of memory");
	athena_scene_node_attach(node->node, &ro->obj);
	return JS_NewUint32(ctx, node->attachments.count);
}

static JSValue js_scene_node_detach(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSSceneNode *node = JS_GetOpaque2(ctx, this_val, js_scene_node_class_id);
	if (!node)
		return JS_EXCEPTION;

	if (argc == 0) {
		js_value_list_free(ctx, &node->attachments);
		js_value_list_init(&node->attachments);
		athena_scene_node_detach(node->node, NULL);
		return JS_NewUint32(ctx, 0);
	}

	JSRenderObject *ro = JS_GetOpaque2(ctx, argv[0], js_render_object_class_id);
	js_value_list_remove(ctx, &node->attachments, argv[0]);
	if (ro)
		athena_scene_node_detach(node->node, &ro->obj);
	return JS_NewUint32(ctx, node->attachments.count);
}

static JSValue scene_node_prop_get(JSContext *ctx, JSValueConst this_val, int magic) {
	JSSceneNode *node = JS_GetOpaque2(ctx, this_val, js_scene_node_class_id);
	if (!node)
		return JS_EXCEPTION;

    JSValue obj = JS_NewObject(ctx);
    switch (magic) {
        case 0:
            JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, node->node->position[0]), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, node->node->position[1]), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, node->node->position[2]), JS_PROP_C_W_E);
            break;
        case 1:
            JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, node->node->rotation[0]), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, node->node->rotation[1]), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, node->node->rotation[2]), JS_PROP_C_W_E);
            break;
        case 2:
            JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, node->node->scale[0]), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, node->node->scale[1]), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, node->node->scale[2]), JS_PROP_C_W_E);
            break;
    }
    return obj;
}

static JSValue scene_node_prop_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
	JSSceneNode *node = JS_GetOpaque2(ctx, this_val, js_scene_node_class_id);
	if (!node)
		return JS_EXCEPTION;

	float x = 0, y = 0, z = 0;
	JSValue vx = JS_GetPropertyStr(ctx, val, "x");
	JSValue vy = JS_GetPropertyStr(ctx, val, "y");
	JSValue vz = JS_GetPropertyStr(ctx, val, "z");
	if (!JS_IsUndefined(vx)) JS_ToFloat32(ctx, &x, vx);
	if (!JS_IsUndefined(vy)) JS_ToFloat32(ctx, &y, vy);
	if (!JS_IsUndefined(vz)) JS_ToFloat32(ctx, &z, vz);
	JS_FreeValue(ctx, vx);
	JS_FreeValue(ctx, vy);
	JS_FreeValue(ctx, vz);

    switch (magic) {
        case 0:
            node->node->position[0] = x;
            node->node->position[1] = y;
            node->node->position[2] = z;
            break;
        case 1:
            node->node->rotation[0] = x;
            node->node->rotation[1] = y;
            node->node->rotation[2] = z;
            break;
        case 2:
            node->node->scale[0] = x;
            node->node->scale[1] = y;
            node->node->scale[2] = z;
            break;
    }

	return JS_UNDEFINED;
}

static JSValue athena_scene_node_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSceneNode *node = JS_GetOpaque2(ctx, this_val, js_scene_node_class_id);
    if (!node)
        return JS_EXCEPTION;

    athena_scene_update(node->node);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_scene_node_proto_funcs[] = {
	JS_CFUNC_DEF("addChild", 1, js_scene_node_add_child),
	JS_CFUNC_DEF("removeChild", 1, js_scene_node_remove_child),
	JS_CFUNC_DEF("attach", 1, js_scene_node_attach),
	JS_CFUNC_DEF("detach", 1, js_scene_node_detach),
	JS_CFUNC_DEF("update", 0, athena_scene_node_update),
	JS_CGETSET_MAGIC_DEF("position", scene_node_prop_get, scene_node_prop_set, 0),
	JS_CGETSET_MAGIC_DEF("rotation", scene_node_prop_get, scene_node_prop_set, 1),
	JS_CGETSET_MAGIC_DEF("scale", scene_node_prop_get, scene_node_prop_set, 2),
};

static JSClassDef js_scene_node_class = {
	"SceneNode",
	.finalizer = scene_node_free,
};

static void render_async_loader_free(JSRuntime *rt, JSValue val) {
    JSRenderAsyncLoader *loader = JS_GetOpaque(val, js_render_async_loader_class_id);
    if (!loader)
        return;
    if (loader->native) {
        // Cleanup any pending thunks before destroying the native loader
        athena_async_clear_with(loader->native, loader_thunk_cleanup_rt);
        athena_async_loader_destroy(loader->native);
    }
    js_free_rt(rt, loader);
    JS_SetOpaque(val, NULL);
}

static JSValue athena_render_async_loader_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSRenderAsyncLoader *loader = js_mallocz(ctx, sizeof(JSRenderAsyncLoader));
    if (!loader)
        return JS_EXCEPTION;

    uint32_t jobs_per_step = 1;
    if (argc > 0 && JS_IsObject(argv[0])) {
        JSValue v = JS_GetPropertyStr(ctx, argv[0], "jobsPerStep");
        if (!JS_IsUndefined(v))
            JS_ToUint32(ctx, &jobs_per_step, v);
        JS_FreeValue(ctx, v);
        if (jobs_per_step == 0) jobs_per_step = 1;
    }

    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    JSValue obj = JS_NewObjectProtoClass(ctx, proto, js_render_async_loader_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj)) {
        js_free(ctx, loader);
        return obj;
    }

    loader->jobs_per_step = jobs_per_step;
    loader->native = athena_async_loader_create(jobs_per_step);
    if (!loader->native) { js_free(ctx, loader); return JS_EXCEPTION; }
    JS_SetOpaque(obj, loader);
    return obj;
}

/* legacy no-op removed */

static JSValue athena_render_async_loader_enqueue(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSRenderAsyncLoader *loader = JS_GetOpaque2(ctx, this_val, js_render_async_loader_class_id);
    if (!loader)
        return JS_EXCEPTION;
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "expected (path, callback[, texture])");

    JSValue path = argv[0];
    JSValue callback = argv[1];
    JSValue texture = argc > 2? argv[2] : JS_UNDEFINED;

    if (!JS_IsString(path))
        return JS_ThrowTypeError(ctx, "path must be string");
    if (!JS_IsFunction(ctx, callback))
        return JS_ThrowTypeError(ctx, "callback must be function");

    const char *cpath = JS_ToCString(ctx, path);
    if (!cpath)
        return JS_EXCEPTION;

    GSSURFACE *tex_ptr = NULL;
    if (!JS_IsUndefined(texture) && !JS_IsNull(texture)) {
        JSImageData* image = JS_GetOpaque2(ctx, texture, get_img_class_id());
        if (image)
            tex_ptr = image->tex;
    }

    LoaderThunk *thunk = js_mallocz(ctx, sizeof(LoaderThunk));
    if (!thunk) {
        JS_FreeCString(ctx, cpath);
        return JS_EXCEPTION;
    }
    thunk->ctx = ctx;
    thunk->cb = JS_DupValue(ctx, callback);
    thunk->path_c = strdup(cpath);

    int qsz = athena_async_enqueue(loader->native, cpath, tex_ptr, loader_bridge_cb, thunk);
    JS_FreeCString(ctx, cpath);
    if (qsz < 0) {
        JS_FreeValue(ctx, thunk->cb);
        if (thunk->path_c) free(thunk->path_c);
        js_free(ctx, thunk);
        return JS_ThrowInternalError(ctx, "enqueue failed");
    }
    return JS_NewUint32(ctx, (uint32_t)qsz);
}

static JSValue athena_render_async_loader_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSRenderAsyncLoader *loader = JS_GetOpaque2(ctx, this_val, js_render_async_loader_class_id);
    if (!loader)
        return JS_EXCEPTION;
    athena_async_clear_with(loader->native, loader_thunk_cleanup_js);
    return JS_UNDEFINED;
}

static JSValue athena_render_async_loader_process(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSRenderAsyncLoader *loader = JS_GetOpaque2(ctx, this_val, js_render_async_loader_class_id);
    if (!loader)
        return JS_EXCEPTION;

    // Process completed jobs; after delivering, kick the worker to leave wait state

    uint32_t budget = loader->jobs_per_step;
    if (argc > 0) {
        JS_ToUint32(ctx, &budget, argv[0]);
        if (budget == 0) budget = loader->jobs_per_step;
    }
    unsigned int qsz_before = athena_async_queue_size(loader->native);
    unsigned int processed = athena_async_process(loader->native, budget);
    unsigned int qsz_after = athena_async_queue_size(loader->native);
    return JS_NewUint32(ctx, processed);
}

static JSValue athena_render_async_loader_size(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSRenderAsyncLoader *loader = JS_GetOpaque2(ctx, this_val, js_render_async_loader_class_id);
    if (!loader) return JS_EXCEPTION;
    return JS_NewUint32(ctx, athena_async_queue_size(loader->native));
}

static JSValue athena_render_async_loader_get_jps(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSRenderAsyncLoader *loader = JS_GetOpaque2(ctx, this_val, js_render_async_loader_class_id);
    if (!loader) return JS_EXCEPTION;
    return JS_NewUint32(ctx, athena_async_jobs_per_step(loader->native));
}

static JSValue athena_render_async_loader_set_jps(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSRenderAsyncLoader *loader = JS_GetOpaque2(ctx, this_val, js_render_async_loader_class_id);
    if (!loader) return JS_EXCEPTION;
    uint32_t v = loader->jobs_per_step;
    if (argc > 0) JS_ToUint32(ctx, &v, argv[0]);
    athena_async_set_jobs_per_step(loader->native, v);
    loader->jobs_per_step = v ? v : 1;
    return JS_NewUint32(ctx, loader->jobs_per_step);
}

static JSValue athena_render_async_loader_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSRenderAsyncLoader *loader = JS_GetOpaque2(ctx, this_val, js_render_async_loader_class_id);
    if (!loader) return JS_EXCEPTION;
    if (loader->native) {
        athena_async_clear_with(loader->native, loader_thunk_cleanup_js);
        athena_async_loader_destroy(loader->native);
        loader->native = NULL;
    }
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_render_async_loader_proto_funcs[] = {
	JS_CFUNC_DEF("enqueue", 3, athena_render_async_loader_enqueue),
	JS_CFUNC_DEF("clear", 0, athena_render_async_loader_clear),
	JS_CFUNC_DEF("process", 1, athena_render_async_loader_process),
	JS_CFUNC_DEF("destroy", 0, athena_render_async_loader_destroy),
	JS_CFUNC_DEF("size", 0, athena_render_async_loader_size),
	JS_CFUNC_DEF("getJobsPerStep", 0, athena_render_async_loader_get_jps),
	JS_CFUNC_DEF("setJobsPerStep", 1, athena_render_async_loader_set_jps),
};

static JSClassDef js_render_async_loader_class = {
	"RenderAsyncLoader",
	.finalizer = render_async_loader_free,
};

static int js_scene_node_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue node_proto, node_class;

    JS_NewClassID(&js_scene_node_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_scene_node_class_id, &js_scene_node_class);

    node_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, node_proto, js_scene_node_proto_funcs, countof(js_scene_node_proto_funcs));

    node_class = JS_NewCFunction2(ctx, athena_scene_node_ctor, "SceneNode", 0, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, node_class, node_proto);
    JS_SetClassProto(ctx, js_scene_node_class_id, node_proto);

    JS_SetModuleExport(ctx, m, "SceneNode", node_class);
    return 0;
}

static int js_render_async_loader_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue proto, cls;

    JS_NewClassID(&js_render_async_loader_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_render_async_loader_class_id, &js_render_async_loader_class);

    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_render_async_loader_proto_funcs, countof(js_render_async_loader_proto_funcs));

    cls = JS_NewCFunction2(ctx, athena_render_async_loader_ctor, "AsyncLoader", 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, cls, proto);
    JS_SetClassProto(ctx, js_render_async_loader_class_id, proto);

    JS_SetModuleExport(ctx, m, "AsyncLoader", cls);
    return 0;
}


static JSValue athena_r_init(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
  	render_init();
	return JS_UNDEFINED;
}

static JSValue athena_r_begin(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
  	render_begin();
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

	bool share_buffers = false;
	if (argc > 6) {
		if (JS_IsObject(argv[6])) {
			JSValue share_prop = JS_GetPropertyStr(ctx, argv[6], "shareBuffers");
			if (!JS_IsUndefined(share_prop))
				share_buffers = JS_ToBool(ctx, share_prop);
			JS_FreeValue(ctx, share_prop);
		} else {
			share_buffers = JS_ToBool(ctx, argv[6]);
		}
	}

	if (share_buffers) {
		JS_DefinePropertyValueStr(ctx, obj, "shareBuffers", JS_NewBool(ctx, share_buffers), JS_PROP_C_W_E);
	}

	return obj;
}

static JSValue athena_render_stats(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	const render_stats_t *stats = render_get_stats();
	JSValue obj = JS_NewObject(ctx);

	JS_DefinePropertyValueStr(ctx, obj, "drawCalls", JS_NewUint32(ctx, stats->draw_calls), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "triangles", JS_NewUint32(ctx, stats->triangles), JS_PROP_C_W_E);

	return obj;
}

static JSValue athena_render_reset_stats(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	render_reset_stats();
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry render_funcs[] = {
	JS_CFUNC_DEF( "init",            0,                athena_r_init),
	JS_CFUNC_DEF( "begin",            0,               athena_r_begin),
    JS_CFUNC_DEF( "setView",         6,                athena_set_view),
	JS_CFUNC_DEF( "stats",           0,                athena_render_stats),
	JS_CFUNC_DEF( "resetStats",      0,                athena_render_reset_stats),
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

static int js_render_batch_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue batch_proto, batch_class;

    JS_NewClassID(&js_render_batch_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_render_batch_class_id, &js_render_batch_class);

    batch_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, batch_proto, js_render_batch_proto_funcs, countof(js_render_batch_proto_funcs));

    batch_class = JS_NewCFunction2(ctx, athena_render_batch_ctor, "Batch", 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, batch_class, batch_proto);
    JS_SetClassProto(ctx, js_render_batch_class_id, batch_proto);

    JS_SetModuleExport(ctx, m, "Batch", batch_class);
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

    m = JS_NewCModule(ctx, "RenderBatch", js_render_batch_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "Batch");

    m = JS_NewCModule(ctx, "RenderSceneNode", js_scene_node_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "SceneNode");

    m = JS_NewCModule(ctx, "RenderAsyncLoader", js_render_async_loader_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "AsyncLoader");

	return athena_push_module(ctx, render_module_init, render_funcs, countof(render_funcs), "Render");
}
