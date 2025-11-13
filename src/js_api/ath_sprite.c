#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <graphics.h>
#include <tile_render.h>
#include <ath_env.h>

static JSClassID js_tilemap_descriptor_class_id;
static JSClassID js_tilemap_instance_class_id;

typedef struct {
	athena_tilemap_descriptor descriptor;
	JSValue textures_array;
} JSTilemapDescriptor;

typedef struct {
	JSValue descriptor_obj;
	JSTilemapDescriptor *descriptor;
	JSValue sprite_buffer;
	athena_sprite_data *sprites;
	uint32_t sprite_count;
} JSTilemapInstance;

static void js_free_value_safe(JSContext *ctx, JSValue value);

static void instance_release_ctx(JSContext *ctx, JSTilemapInstance *instance) {
	if (!instance)
		return;
	js_free_value_safe(ctx, instance->descriptor_obj);
	js_free_value_safe(ctx, instance->sprite_buffer);
	js_free(ctx, instance);
}

static void js_free_value_safe(JSContext *ctx, JSValue value) {
	if (!JS_IsUndefined(value)) {
		JS_FreeValue(ctx, value);
	}
}

static void js_assign_float_prop(JSContext *ctx, JSValue obj, const char *name, float *out) {
	JSValue val = JS_GetPropertyStr(ctx, obj, name);
	if (!JS_IsUndefined(val))
		JS_ToFloat32(ctx, out, val);
	JS_FreeValue(ctx, val);
}

static void js_assign_uint32_prop(JSContext *ctx, JSValue obj, const char *name, uint32_t *out) {
	JSValue val = JS_GetPropertyStr(ctx, obj, name);
	if (!JS_IsUndefined(val))
		JS_ToUint32(ctx, out, val);
	JS_FreeValue(ctx, val);
}

static void js_assign_int32_prop(JSContext *ctx, JSValue obj, const char *name, int32_t *out) {
	JSValue val = JS_GetPropertyStr(ctx, obj, name);
	if (!JS_IsUndefined(val))
		JS_ToInt32(ctx, out, val);
	JS_FreeValue(ctx, val);
}

static void js_assign_uint64_prop(JSContext *ctx, JSValue obj, const char *name, uint64_t *out) {
	JSValue val = JS_GetPropertyStr(ctx, obj, name);
	if (!JS_IsUndefined(val)) {
		int64_t tmp = 0;
		JS_ToInt64(ctx, &tmp, val);
		*out = (uint64_t)tmp;
	}
	JS_FreeValue(ctx, val);
}

static uint8_t *get_buffer_data(JSContext *ctx, JSValueConst val, size_t *length) {
	uint8_t *ptr = JS_GetArrayBuffer(ctx, length, val);
	if (ptr)
		return ptr;

	size_t byte_offset = 0;
	size_t byte_length = 0;
	size_t bytes_per_element = 0;
	JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, val, &byte_offset, &byte_length, &bytes_per_element);
	if (JS_IsException(array_buffer))
		return NULL;

	size_t buffer_size = 0;
	uint8_t *data = JS_GetArrayBuffer(ctx, &buffer_size, array_buffer);
	JS_FreeValue(ctx, array_buffer);
	if (!data)
		return NULL;

	if (byte_offset + byte_length > buffer_size)
		return NULL;

	*length = byte_length;
	return data + byte_offset;
}

static JSValue build_layout_object(JSContext *ctx) {
	const athena_tilemap_layout *layout = tile_render_layout();
	JSValue layout_obj = JS_NewObject(ctx);
	JSValue offsets = JS_NewObject(ctx);
	if (JS_IsException(layout_obj) || JS_IsException(offsets)) {
		js_free_value_safe(ctx, layout_obj);
		js_free_value_safe(ctx, offsets);
		return JS_EXCEPTION;
	}

	JS_SetPropertyStr(ctx, layout_obj, "stride", JS_NewUint32(ctx, layout->stride));
	JS_SetPropertyStr(ctx, offsets, "x", JS_NewUint32(ctx, layout->offset_x));
	JS_SetPropertyStr(ctx, offsets, "y", JS_NewUint32(ctx, layout->offset_y));
	JS_SetPropertyStr(ctx, offsets, "w", JS_NewUint32(ctx, layout->offset_w));
	JS_SetPropertyStr(ctx, offsets, "h", JS_NewUint32(ctx, layout->offset_h));
	JS_SetPropertyStr(ctx, offsets, "u1", JS_NewUint32(ctx, layout->offset_u1));
	JS_SetPropertyStr(ctx, offsets, "v1", JS_NewUint32(ctx, layout->offset_v1));
	JS_SetPropertyStr(ctx, offsets, "u2", JS_NewUint32(ctx, layout->offset_u2));
	JS_SetPropertyStr(ctx, offsets, "v2", JS_NewUint32(ctx, layout->offset_v2));
	JS_SetPropertyStr(ctx, offsets, "r", JS_NewUint32(ctx, layout->offset_r));
	JS_SetPropertyStr(ctx, offsets, "g", JS_NewUint32(ctx, layout->offset_g));
	JS_SetPropertyStr(ctx, offsets, "b", JS_NewUint32(ctx, layout->offset_b));
	JS_SetPropertyStr(ctx, offsets, "a", JS_NewUint32(ctx, layout->offset_a));
	JS_SetPropertyStr(ctx, offsets, "zindex", JS_NewUint32(ctx, layout->offset_zindex));
	JS_SetPropertyStr(ctx, layout_obj, "offsets", offsets);

	return layout_obj;
}

static int fill_materials_from_array(JSContext *ctx, JSValue array, athena_sprite_material **out_materials, uint32_t *out_count) {
	uint32_t count = 0;
	JSValue length_prop = JS_GetPropertyStr(ctx, array, "length");
	if (JS_IsException(length_prop))
		return -1;
	JS_ToUint32(ctx, &count, length_prop);
	JS_FreeValue(ctx, length_prop);
	if (!count)
		return 0;

	athena_sprite_material *materials = malloc(sizeof(athena_sprite_material) * count);
	if (!materials)
		return -1;

	for (uint32_t i = 0; i < count; i++) {
		JSValue material = JS_GetPropertyUint32(ctx, array, i);
		if (JS_IsException(material)) {
			free(materials);
			return -1;
		}
		if (!JS_IsObject(material)) {
			JS_FreeValue(ctx, material);
			free(materials);
			return -1;
		}

		js_assign_int32_prop(ctx, material, "texture_index", &materials[i].texture_index);
		js_assign_uint64_prop(ctx, material, "blend_mode", &materials[i].blend_mode);
		js_assign_uint32_prop(ctx, material, "end_offset", &materials[i].end);
		JS_FreeValue(ctx, material);
	}

	*out_materials = materials;
	*out_count = count;
	return 0;
}

static int fill_materials_from_buffer(JSContext *ctx, JSValue buffer, athena_sprite_material **out_materials, uint32_t *out_count) {
	size_t length = 0;
	uint8_t *ptr = get_buffer_data(ctx, buffer, &length);
	if (!ptr) {
		JS_ThrowTypeError(ctx, "materials buffer must be ArrayBuffer or TypedArray");
		return -1;
	}

	if (length % sizeof(athena_sprite_material) != 0) {
		JS_ThrowRangeError(ctx, "invalid material buffer size");
		return -1;
	}

	uint32_t count = length / sizeof(athena_sprite_material);
	athena_sprite_material *materials = malloc(length);
	if (!materials)
		return -1;

	memcpy(materials, ptr, length);
	*out_materials = materials;
	*out_count = count;
	return 0;
}

static JSValue import_texture(JSContext *ctx, JSValue texture_val, JSValue textures_array, GSSURFACE **textures, uint32_t index) {
	if (JS_IsString(texture_val)) {
		const char *texture_path = JS_ToCString(ctx, texture_val);
		if (!texture_path)
			return JS_EXCEPTION;

		JSImageData *image = js_mallocz(ctx, sizeof(*image));
		if (!image) {
			JS_FreeCString(ctx, texture_path);
			return JS_EXCEPTION;
		}

		JSValue img_obj = JS_NewObjectClass(ctx, get_img_class_id());
		if (JS_IsException(img_obj)) {
			js_free(ctx, image);
			JS_FreeCString(ctx, texture_path);
			return img_obj;
		}

		image->tex = malloc(sizeof(GSSURFACE));
		if (!image->tex) {
			js_free(ctx, image);
			JS_FreeCString(ctx, texture_path);
			JS_FreeValue(ctx, img_obj);
			return JS_EXCEPTION;
		}

		load_image(image->tex, texture_path, true);
		JS_FreeCString(ctx, texture_path);
		image->delayed = true;
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
		JS_SetPropertyUint32(ctx, textures_array, index, img_obj);
		textures[index] = image->tex;
		return JS_UNDEFINED;
	}

	JSImageData *image = JS_GetOpaque2(ctx, texture_val, get_img_class_id());
	if (!image)
		return JS_EXCEPTION;

	JS_SetPropertyUint32(ctx, textures_array, index, texture_val);
	JS_DupValue(ctx, texture_val);
	textures[index] = image->tex;
	return JS_UNDEFINED;
}

static void descriptor_release(JSRuntime *rt, JSTilemapDescriptor *descriptor);

static void descriptor_release_ctx(JSContext *ctx, JSTilemapDescriptor *descriptor) {
	descriptor_release(JS_GetRuntime(ctx), descriptor);
}

static void descriptor_release(JSRuntime *rt, JSTilemapDescriptor *descriptor) {
	if (!descriptor)
		return;
	if (descriptor->descriptor.textures)
		free(descriptor->descriptor.textures);
	if (descriptor->descriptor.materials)
		free(descriptor->descriptor.materials);
	JS_FreeValueRT(rt, descriptor->textures_array);
	js_free_rt(rt, descriptor);
}

static void athena_tilemap_descriptor_dtor(JSRuntime *rt, JSValue val) {
	JSTilemapDescriptor *descriptor = JS_GetOpaque(val, js_tilemap_descriptor_class_id);
	if (!descriptor)
		return;
	descriptor_release(rt, descriptor);
	JS_SetOpaque(val, NULL);
}

static JSValue athena_tilemap_descriptor_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
	JSTilemapDescriptor *descriptor = js_mallocz(ctx, sizeof(*descriptor));
	if (!descriptor)
		return JS_EXCEPTION;
	descriptor->textures_array = JS_UNDEFINED;

	JSValue opts = argc > 0 ? argv[0] : JS_UNDEFINED;
	if (!JS_IsObject(opts)) {
		descriptor_release_ctx(ctx, descriptor);
		return JS_ThrowTypeError(ctx, "descriptor options must be object");
	}

	JSValue textures = JS_GetPropertyStr(ctx, opts, "textures");
	if (JS_IsException(textures)) {
		descriptor_release_ctx(ctx, descriptor);
		return textures;
	}
	if (JS_IsArray(ctx, textures)) {
		JSValue length_prop = JS_GetPropertyStr(ctx, textures, "length");
		JS_ToUint32(ctx, &descriptor->descriptor.texture_count, length_prop);
		JS_FreeValue(ctx, length_prop);
		if (descriptor->descriptor.texture_count) {
			descriptor->descriptor.textures = malloc(sizeof(GSSURFACE *) * descriptor->descriptor.texture_count);
			if (!descriptor->descriptor.textures) {
				JS_FreeValue(ctx, textures);
				descriptor_release_ctx(ctx, descriptor);
				return JS_EXCEPTION;
			}
			descriptor->textures_array = JS_NewArray(ctx);
			if (JS_IsException(descriptor->textures_array)) {
				JS_FreeValue(ctx, textures);
				descriptor_release_ctx(ctx, descriptor);
				return descriptor->textures_array;
			}
			for (uint32_t i = 0; i < descriptor->descriptor.texture_count; i++) {
				JSValue texture_val = JS_GetPropertyUint32(ctx, textures, i);
				if (JS_IsUndefined(texture_val)) {
					JS_FreeValue(ctx, texture_val);
					continue;
				}
				if (JS_IsException(import_texture(ctx, texture_val, descriptor->textures_array, descriptor->descriptor.textures, i))) {
					JS_FreeValue(ctx, texture_val);
					JS_FreeValue(ctx, textures);
					descriptor_release_ctx(ctx, descriptor);
					return JS_EXCEPTION;
				}
				JS_FreeValue(ctx, texture_val);
			}
		}
	}
	JS_FreeValue(ctx, textures);

	JSValue materials = JS_GetPropertyStr(ctx, opts, "materials");
	if (JS_IsException(materials)) {
		descriptor_release_ctx(ctx, descriptor);
		return materials;
	}
	if (!JS_IsUndefined(materials)) {
		if (JS_IsArray(ctx, materials)) {
			if (fill_materials_from_array(ctx, materials, &descriptor->descriptor.materials, &descriptor->descriptor.material_count) < 0) {
				JS_FreeValue(ctx, materials);
				descriptor_release_ctx(ctx, descriptor);
				return JS_EXCEPTION;
			}
		} else {
			if (fill_materials_from_buffer(ctx, materials, &descriptor->descriptor.materials, &descriptor->descriptor.material_count) < 0) {
				JS_FreeValue(ctx, materials);
				descriptor_release_ctx(ctx, descriptor);
				return JS_EXCEPTION;
			}
		}
	}
	JS_FreeValue(ctx, materials);

	JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	if (JS_IsException(proto)) {
		descriptor_release_ctx(ctx, descriptor);
		return proto;
	}

	JSValue obj = JS_NewObjectProtoClass(ctx, proto, js_tilemap_descriptor_class_id);
	JS_FreeValue(ctx, proto);
	if (JS_IsException(obj)) {
		descriptor_release_ctx(ctx, descriptor);
		return obj;
	}

	JS_SetOpaque(obj, descriptor);
	return obj;
}

static JSValue athena_tilemap_descriptor_get_materialCount(JSContext *ctx, JSValueConst this_val, int magic) {
	JSTilemapDescriptor *descriptor = JS_GetOpaque2(ctx, this_val, js_tilemap_descriptor_class_id);
	if (!descriptor)
		return JS_EXCEPTION;
	return JS_NewUint32(ctx, descriptor->descriptor.material_count);
}

static int instance_apply_buffer(JSContext *ctx, JSTilemapInstance *instance, JSValue buffer_val) {
	size_t length = 0;
	uint8_t *ptr = get_buffer_data(ctx, buffer_val, &length);
	if (!ptr)
		return -1;
	if (length % sizeof(athena_sprite_data) != 0) {
		JS_ThrowRangeError(ctx, "sprite buffer length mismatch");
		return -1;
	}

	js_free_value_safe(ctx, instance->sprite_buffer);
	instance->sprite_buffer = JS_DupValue(ctx, buffer_val);
	instance->sprites = (athena_sprite_data *)ptr;
	instance->sprite_count = length / sizeof(athena_sprite_data);
	return 0;
}

static void athena_tilemap_instance_dtor(JSRuntime *rt, JSValue val) {
	JSTilemapInstance *instance = JS_GetOpaque(val, js_tilemap_instance_class_id);
	if (!instance)
		return;
	JS_FreeValueRT(rt, instance->descriptor_obj);
	JS_FreeValueRT(rt, instance->sprite_buffer);
	js_free_rt(rt, instance);
	JS_SetOpaque(val, NULL);
}

static JSValue athena_tilemap_instance_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
	JSTilemapInstance *instance = js_mallocz(ctx, sizeof(*instance));
	if (!instance)
		return JS_EXCEPTION;
	instance->descriptor_obj = JS_UNDEFINED;
	instance->sprite_buffer = JS_UNDEFINED;

	JSValue opts = argc > 0 ? argv[0] : JS_UNDEFINED;
	if (!JS_IsObject(opts)) {
		instance_release_ctx(ctx, instance);
		return JS_ThrowTypeError(ctx, "instance options must be object");
	}

	JSValue descriptor_val = JS_GetPropertyStr(ctx, opts, "descriptor");
	if (JS_IsException(descriptor_val)) {
		instance_release_ctx(ctx, instance);
		return descriptor_val;
	}
	JSTilemapDescriptor *descriptor = JS_GetOpaque2(ctx, descriptor_val, js_tilemap_descriptor_class_id);
	if (!descriptor) {
		JS_FreeValue(ctx, descriptor_val);
		instance_release_ctx(ctx, instance);
		return JS_EXCEPTION;
	}
	instance->descriptor = descriptor;
	instance->descriptor_obj = descriptor_val;

	JSValue sprite_buffer = JS_GetPropertyStr(ctx, opts, "spriteBuffer");
	if (JS_IsException(sprite_buffer)) {
		instance_release_ctx(ctx, instance);
		return sprite_buffer;
	}
	if (!JS_IsUndefined(sprite_buffer)) {
		if (instance_apply_buffer(ctx, instance, sprite_buffer) < 0) {
			JS_FreeValue(ctx, sprite_buffer);
			instance_release_ctx(ctx, instance);
			return JS_EXCEPTION;
		}
	}
	JS_FreeValue(ctx, sprite_buffer);

	JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	if (JS_IsException(proto)) {
		instance_release_ctx(ctx, instance);
		return proto;
	}

	JSValue obj = JS_NewObjectProtoClass(ctx, proto, js_tilemap_instance_class_id);
	JS_FreeValue(ctx, proto);
	if (JS_IsException(obj)) {
		instance_release_ctx(ctx, instance);
		return obj;
	}

	JS_SetOpaque(obj, instance);
	return obj;
}

static JSValue athena_tilemap_instance_render(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSTilemapInstance *instance = JS_GetOpaque2(ctx, this_val, js_tilemap_instance_class_id);
	if (!instance)
		return JS_EXCEPTION;
	if (!instance->descriptor || !instance->sprites)
		return JS_ThrowInternalError(ctx, "instance missing descriptor or sprite buffer");

	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	JS_ToFloat32(ctx, &x, argc > 0 ? argv[0] : JS_UNDEFINED);
	JS_ToFloat32(ctx, &y, argc > 1 ? argv[1] : JS_UNDEFINED);
	if (argc > 2)
		JS_ToFloat32(ctx, &z, argv[2]);

	tile_render_render(&instance->descriptor->descriptor, instance->sprites, x, y, z);
	return JS_UNDEFINED;
}

static JSValue athena_tilemap_instance_replace_buffer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSTilemapInstance *instance = JS_GetOpaque2(ctx, this_val, js_tilemap_instance_class_id);
	if (!instance)
		return JS_EXCEPTION;
	if (argc < 1)
		return JS_ThrowTypeError(ctx, "replaceSpriteBuffer expects buffer argument");
	if (instance_apply_buffer(ctx, instance, argv[0]) < 0)
		return JS_EXCEPTION;
	return JS_UNDEFINED;
}

static JSValue athena_tilemap_instance_get_buffer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSTilemapInstance *instance = JS_GetOpaque2(ctx, this_val, js_tilemap_instance_class_id);
	if (!instance)
		return JS_EXCEPTION;
	if (JS_IsUndefined(instance->sprite_buffer))
		return JS_UNDEFINED;
	return JS_DupValue(ctx, instance->sprite_buffer);
}

static JSValue athena_tilemap_instance_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSTilemapInstance *instance = JS_GetOpaque2(ctx, this_val, js_tilemap_instance_class_id);
	if (!instance)
		return JS_EXCEPTION;
	if (!instance->sprites)
		return JS_ThrowInternalError(ctx, "instance has no sprite buffer");
	if (argc < 2)
		return JS_ThrowTypeError(ctx, "updateSprites expects dstOffset and source buffer");

	uint32_t dst_offset = 0;
	JS_ToUint32(ctx, &dst_offset, argv[0]);
	size_t src_length = 0;
	uint8_t *src_ptr = get_buffer_data(ctx, argv[1], &src_length);
	if (!src_ptr)
		return JS_EXCEPTION;
	if (src_length % sizeof(athena_sprite_data) != 0)
		return JS_ThrowRangeError(ctx, "source buffer misaligned");

	uint32_t sprite_count = src_length / sizeof(athena_sprite_data);
	uint32_t copy_count = sprite_count;
	if (argc > 2) {
		JS_ToUint32(ctx, &copy_count, argv[2]);
		if (copy_count > sprite_count)
			copy_count = sprite_count;
	}

	if ((uint64_t)dst_offset + copy_count > instance->sprite_count)
		return JS_ThrowRangeError(ctx, "updateSprites exceeds buffer");

	memcpy(&instance->sprites[dst_offset], src_ptr, copy_count * sizeof(athena_sprite_data));
	return JS_UNDEFINED;
}

static void sprite_buffer_free(JSRuntime *rt, void *opaque, void *ptr) {
	if (ptr)
		js_free_rt(rt, ptr);
}

static JSValue tilemap_sprite_buffer_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	uint32_t sprite_count = 0;
	if (argc < 1)
		return JS_ThrowTypeError(ctx, "create(count) requires count");
	JS_ToUint32(ctx, &sprite_count, argv[0]);
	size_t length = sprite_count * sizeof(athena_sprite_data);
	uint8_t *data = js_mallocz(ctx, length);
	if (!data)
		return JS_EXCEPTION;
	JSValue buffer = JS_NewArrayBuffer(ctx, data, length, sprite_buffer_free, NULL, 0);
	if (JS_IsException(buffer)) {
		js_free(ctx, data);
		return buffer;
	}
	return buffer;
}

static JSValue tilemap_sprite_buffer_from_objects(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_ThrowTypeError(ctx, "fromObjects requires array");
	JSValue arr = argv[0];
	if (!JS_IsArray(ctx, arr))
		return JS_ThrowTypeError(ctx, "fromObjects expects array");

	JSValue length_prop = JS_GetPropertyStr(ctx, arr, "length");
	if (JS_IsException(length_prop))
		return length_prop;
	uint32_t count = 0;
	JS_ToUint32(ctx, &count, length_prop);
	JS_FreeValue(ctx, length_prop);
	size_t length = count * sizeof(athena_sprite_data);
	uint8_t *data = js_mallocz(ctx, length);
	if (!data)
		return JS_EXCEPTION;

	athena_sprite_data *sprites = (athena_sprite_data *)data;
	for (uint32_t i = 0; i < count; i++) {
		JSValue sprite = JS_GetPropertyUint32(ctx, arr, i);
		if (JS_IsException(sprite)) {
			js_free(ctx, data);
			return sprite;
		}
		if (!JS_IsObject(sprite)) {
			JS_FreeValue(ctx, sprite);
			js_free(ctx, data);
			return JS_ThrowTypeError(ctx, "sprite entry must be object");
		}
		js_assign_float_prop(ctx, sprite, "x", &sprites[i].x);
		js_assign_float_prop(ctx, sprite, "y", &sprites[i].y);
		js_assign_float_prop(ctx, sprite, "zindex", &sprites[i].zindex);
		js_assign_float_prop(ctx, sprite, "w", &sprites[i].w);
		js_assign_float_prop(ctx, sprite, "h", &sprites[i].h);
		js_assign_float_prop(ctx, sprite, "u1", &sprites[i].u1);
		js_assign_float_prop(ctx, sprite, "v1", &sprites[i].v1);
		js_assign_float_prop(ctx, sprite, "u2", &sprites[i].u2);
		js_assign_float_prop(ctx, sprite, "v2", &sprites[i].v2);
		js_assign_uint32_prop(ctx, sprite, "r", &sprites[i].r);
		js_assign_uint32_prop(ctx, sprite, "g", &sprites[i].g);
		js_assign_uint32_prop(ctx, sprite, "b", &sprites[i].b);
		js_assign_uint32_prop(ctx, sprite, "a", &sprites[i].a);
		JS_FreeValue(ctx, sprite);
	}

	JSValue buffer = JS_NewArrayBuffer(ctx, data, length, sprite_buffer_free, NULL, 0);
	if (JS_IsException(buffer)) {
		js_free(ctx, data);
		return buffer;
	}
	return buffer;
}

static JSValue athena_set_camera(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	float src_x = 0.0f;
	float src_y = 0.0f;
	JS_ToFloat32(ctx, &src_x, argv[0]);
	JS_ToFloat32(ctx, &src_y, argv[1]);
	tile_render_set_camera(src_x, src_y);
	return JS_UNDEFINED;
}

static JSValue athena_init(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	tile_render_init();
	return JS_UNDEFINED;
}

static JSValue athena_begin(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	tile_render_begin();
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_tilemap_descriptor_proto_funcs[] = {
	JS_CGETSET_DEF("materialCount", athena_tilemap_descriptor_get_materialCount, NULL),
};

static const JSCFunctionListEntry js_tilemap_instance_proto_funcs[] = {
	JS_CFUNC_DEF("render", 3, athena_tilemap_instance_render),
	JS_CFUNC_DEF("replaceSpriteBuffer", 1, athena_tilemap_instance_replace_buffer),
	JS_CFUNC_DEF("getSpriteBuffer", 0, athena_tilemap_instance_get_buffer),
	JS_CFUNC_DEF("updateSprites", 3, athena_tilemap_instance_update),
};

static const JSCFunctionListEntry js_tilemap_spritebuffer_funcs[] = {
	JS_CFUNC_DEF("create", 1, tilemap_sprite_buffer_create),
	JS_CFUNC_DEF("fromObjects", 1, tilemap_sprite_buffer_from_objects),
};

static const JSCFunctionListEntry js_tilemap_funcs[] = {
	JS_CFUNC_DEF("init", 0, athena_init),
	JS_CFUNC_DEF("begin", 0, athena_begin),
	JS_CFUNC_DEF("setCamera", 2, athena_set_camera),
};

static int tilemap_module_init(JSContext *ctx, JSModuleDef *m) {
	JS_NewClassID(&js_tilemap_descriptor_class_id);
	JSClassDef descriptor_class = {
		"TileMapDescriptor",
		.finalizer = athena_tilemap_descriptor_dtor,
	};
	JS_NewClass(JS_GetRuntime(ctx), js_tilemap_descriptor_class_id, &descriptor_class);

	JS_NewClassID(&js_tilemap_instance_class_id);
	JSClassDef instance_class = {
		"TileMapInstance",
		.finalizer = athena_tilemap_instance_dtor,
	};
	JS_NewClass(JS_GetRuntime(ctx), js_tilemap_instance_class_id, &instance_class);

	JSValue descriptor_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, descriptor_proto, js_tilemap_descriptor_proto_funcs, countof(js_tilemap_descriptor_proto_funcs));
	JSValue descriptor_ctor = JS_NewCFunction2(ctx, athena_tilemap_descriptor_ctor, "Descriptor", 1, JS_CFUNC_constructor, 0);
	JS_SetConstructor(ctx, descriptor_ctor, descriptor_proto);
	JS_SetClassProto(ctx, js_tilemap_descriptor_class_id, descriptor_proto);

	JSValue instance_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, instance_proto, js_tilemap_instance_proto_funcs, countof(js_tilemap_instance_proto_funcs));
	JSValue instance_ctor = JS_NewCFunction2(ctx, athena_tilemap_instance_ctor, "Instance", 1, JS_CFUNC_constructor, 0);
	JS_SetConstructor(ctx, instance_ctor, instance_proto);
	JS_SetClassProto(ctx, js_tilemap_instance_class_id, instance_proto);

	JSValue spritebuffer_obj = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, spritebuffer_obj, js_tilemap_spritebuffer_funcs, countof(js_tilemap_spritebuffer_funcs));

	JSValue tilemap_obj = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, tilemap_obj, js_tilemap_funcs, countof(js_tilemap_funcs));
	JS_SetPropertyStr(ctx, tilemap_obj, "Descriptor", descriptor_ctor);
	JS_SetPropertyStr(ctx, tilemap_obj, "Instance", instance_ctor);
	JS_SetPropertyStr(ctx, tilemap_obj, "SpriteBuffer", spritebuffer_obj);
	JSValue layout_obj = build_layout_object(ctx);
	if (JS_IsException(layout_obj)) {
		JS_FreeValue(ctx, tilemap_obj);
		return -1;
	}
	JS_SetPropertyStr(ctx, tilemap_obj, "layout", layout_obj);

	JS_SetModuleExport(ctx, m, "default", tilemap_obj);
	return 0;
}

JSModuleDef *athena_tilemap_init(JSContext *ctx) {
	JSModuleDef *m;
	m = JS_NewCModule(ctx, "TileMap", tilemap_module_init);
	if (!m)
		return NULL;
	JS_AddModuleExport(ctx, m, "default");
	return m;
}
