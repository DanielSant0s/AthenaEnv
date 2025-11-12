#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <graphics.h>
#include <tile_render.h>
#include <ath_env.h>

static JSClassID js_tilemap_class_id;

typedef struct {
	athena_tilemap_data m;
	JSValue textures_array;
} JSTilemapData;

static void athena_tilemap_dtor(JSRuntime *rt, JSValue val) {
	JSTilemapData *tm = JS_GetOpaque(val, js_tilemap_class_id);

	if (!tm)
		return;

	if (tm->m.textures)
		free(tm->m.textures);

	if (tm->m.sprites)
		free(tm->m.sprites);

	if (tm->m.materials)
		free(tm->m.materials);

	JS_FreeValueRT(rt, tm->textures_array);

	js_free_rt(rt, tm);

	JS_SetOpaque(val, NULL);
}

static JSValue athena_tilemap_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSTilemapData* tilemap;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

	tilemap = js_mallocz(ctx, sizeof(*tilemap));
    if (!tilemap)
        return JS_EXCEPTION;

	JSValue tilemap_data = argv[0];

	if (!JS_IsObject(tilemap_data))
		goto fail;

	JSValue textures_array = JS_GetPropertyStr(ctx, tilemap_data, "textures");

	JS_ToUint32(ctx, &tilemap->m.texture_count, JS_GetPropertyStr(ctx, textures_array, "length"));

	tilemap->m.textures = malloc(sizeof(GSSURFACE*)*tilemap->m.texture_count);
	tilemap->textures_array = JS_NewArray(ctx);

	for (int i = 0; i < tilemap->m.texture_count; i++) {
		JSValue texture = JS_GetPropertyUint32(ctx, textures_array, i);

		if (JS_IsString(texture)) {
            const char *texture_path = JS_ToCString(ctx, texture);
            JSImageData* image = js_mallocz(ctx, sizeof(*image));

            JSValue img_obj = JS_UNDEFINED;

            image->tex = malloc(sizeof(GSSURFACE));
            load_image(image->tex, texture_path, true);

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

    		img_obj = JS_NewObjectClass(ctx, get_img_class_id());    
    		JS_SetOpaque(img_obj, image);

            JS_SetPropertyUint32(ctx, tilemap->textures_array, i, img_obj);

            tilemap->m.textures[i] = image->tex;

            JS_FreeCString(ctx, texture_path);
        } else {
            JSImageData* image = JS_GetOpaque2(ctx, texture, get_img_class_id());

            if (!image) goto fail;

            JS_SetPropertyUint32(ctx, tilemap->textures_array, i, texture);

            tilemap->m.textures[i] = image->tex;

            JS_DupValue(ctx, texture);
        }
	}

    JSValue sprites_array = JS_GetPropertyStr(ctx, tilemap_data, "sprites");

    JS_ToUint32(ctx, &tilemap->m.sprite_count, JS_GetPropertyStr(ctx, sprites_array, "length"));

    tilemap->m.sprites = malloc(sizeof(athena_sprite_data)*tilemap->m.sprite_count);

    for (int i = 0; i < tilemap->m.sprite_count; i++) {
        JSValue sprite = JS_GetPropertyUint32(ctx, sprites_array, i);

        JS_ToFloat32(ctx, &tilemap->m.sprites[i].x, JS_GetPropertyStr(ctx, sprite, "x"));
        JS_ToFloat32(ctx, &tilemap->m.sprites[i].y, JS_GetPropertyStr(ctx, sprite, "y"));
        JS_ToFloat32(ctx, &tilemap->m.sprites[i].zindex, JS_GetPropertyStr(ctx, sprite, "zindex"));

        JS_ToFloat32(ctx, &tilemap->m.sprites[i].w, JS_GetPropertyStr(ctx, sprite, "w"));
        JS_ToFloat32(ctx, &tilemap->m.sprites[i].h, JS_GetPropertyStr(ctx, sprite, "h"));
        
        JS_ToFloat32(ctx, &tilemap->m.sprites[i].u1, JS_GetPropertyStr(ctx, sprite, "u1"));
        JS_ToFloat32(ctx, &tilemap->m.sprites[i].v1, JS_GetPropertyStr(ctx, sprite, "v1"));
        JS_ToFloat32(ctx, &tilemap->m.sprites[i].u2, JS_GetPropertyStr(ctx, sprite, "u2"));
        JS_ToFloat32(ctx, &tilemap->m.sprites[i].v2, JS_GetPropertyStr(ctx, sprite, "v2"));

        JS_ToUint32(ctx, &tilemap->m.sprites[i].r, JS_GetPropertyStr(ctx, sprite, "r"));
        JS_ToUint32(ctx, &tilemap->m.sprites[i].g, JS_GetPropertyStr(ctx, sprite, "g"));
        JS_ToUint32(ctx, &tilemap->m.sprites[i].b, JS_GetPropertyStr(ctx, sprite, "b"));
        JS_ToUint32(ctx, &tilemap->m.sprites[i].a, JS_GetPropertyStr(ctx, sprite, "a"));
    }

	JSValue materials_array = JS_GetPropertyStr(ctx, tilemap_data, "materials");

    JS_ToUint32(ctx, &tilemap->m.material_count, JS_GetPropertyStr(ctx, materials_array, "length"));

    tilemap->m.materials = malloc(sizeof(athena_sprite_material)*tilemap->m.material_count);

    for (int i = 0; i < tilemap->m.material_count; i++) {
        JSValue material = JS_GetPropertyUint32(ctx, materials_array, i);

        JS_ToInt32(ctx, &tilemap->m.materials[i].texture_index, JS_GetPropertyStr(ctx, material, "texture_index"));
        JS_ToInt64(ctx, &tilemap->m.materials[i].blend_mode, JS_GetPropertyStr(ctx, material, "blend_mode"));
        JS_ToUint32(ctx, &tilemap->m.materials[i].end, JS_GetPropertyStr(ctx, material, "end_offset"));
    }

 register_obj:

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, js_tilemap_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;
    JS_SetOpaque(obj, tilemap);
    return obj;

 fail:
    js_free(ctx, tilemap);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue athena_tilemap_free(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	athena_tilemap_dtor(JS_GetRuntime(ctx), this_val);

	return JS_UNDEFINED;
}

static JSValue athena_tilemap_render(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	float x, y, zindex = 0;

	JSTilemapData *tilemap = JS_GetOpaque2(ctx, this_val, js_tilemap_class_id);

	JS_ToFloat32(ctx, &x, argv[0]);
	JS_ToFloat32(ctx, &y, argv[1]);
    if (argc > 2) {
        JS_ToFloat32(ctx, &zindex, argv[2]);
    }

    tile_render_render(&tilemap->m, x, y, zindex);

	return JS_UNDEFINED;
}

static JSValue js_tilemap_get(JSContext *ctx, JSValueConst this_val, int magic) {
	JSTilemapData *tilemap = JS_GetOpaque2(ctx, this_val, js_tilemap_class_id);

	switch (magic) {
		case 0:
            return JS_NewArrayBuffer(ctx, tilemap->m.sprites, tilemap->m.sprite_count*sizeof(athena_sprite_data), NULL, NULL, 1);
		case 1:
            return JS_NewArrayBuffer(ctx, tilemap->m.materials, tilemap->m.material_count*sizeof(athena_sprite_material), NULL, NULL, 1);
		case 2:
            return tilemap->textures_array;
            //return JS_NewArrayBuffer(ctx, tilemap->m.textures, tilemap->m.texture_count, NULL, NULL, 1);
        default:
            break;
	}

    return JS_UNDEFINED;
}

static JSValue js_tilemap_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    JSTilemapData *tilemap = JS_GetOpaque2(ctx, this_val, js_tilemap_class_id);

    switch (magic)
    {
        case 0:
            {
                uint32_t tmp_size = 0;
                athena_sprite_data *tmp_data = JS_GetArrayBuffer(ctx, &tmp_size, val);

                if (tmp_data) {
                    if (tmp_data != tilemap->m.sprites) {
                        free(tilemap->m.sprites);
                    }

                    tilemap->m.sprites = tmp_data;
                    tilemap->m.sprite_count = tmp_size/sizeof(athena_sprite_data);

                    return JS_UNDEFINED;
                }

                JSValue sprites_array = val;

                JS_ToUint32(ctx, &tilemap->m.sprite_count, JS_GetPropertyStr(ctx, sprites_array, "length"));

                if (tilemap->m.sprites) free(tilemap->m.sprites);

                tilemap->m.sprites = malloc(sizeof(athena_sprite_data)*tilemap->m.sprite_count);

                for (int i = 0; i < tilemap->m.sprite_count; i++) {
                    JSValue sprite = JS_GetPropertyUint32(ctx, sprites_array, i);
                
                    JS_ToFloat32(ctx, &tilemap->m.sprites[i].x, JS_GetPropertyStr(ctx, sprite, "x"));
                    JS_ToFloat32(ctx, &tilemap->m.sprites[i].y, JS_GetPropertyStr(ctx, sprite, "y"));
                    JS_ToFloat32(ctx, &tilemap->m.sprites[i].zindex, JS_GetPropertyStr(ctx, sprite, "zindex"));
                
                    JS_ToFloat32(ctx, &tilemap->m.sprites[i].w, JS_GetPropertyStr(ctx, sprite, "w"));
                    JS_ToFloat32(ctx, &tilemap->m.sprites[i].h, JS_GetPropertyStr(ctx, sprite, "h"));

                    JS_ToFloat32(ctx, &tilemap->m.sprites[i].u1, JS_GetPropertyStr(ctx, sprite, "u1"));
                    JS_ToFloat32(ctx, &tilemap->m.sprites[i].v1, JS_GetPropertyStr(ctx, sprite, "v1"));
                    JS_ToFloat32(ctx, &tilemap->m.sprites[i].u2, JS_GetPropertyStr(ctx, sprite, "u2"));
                    JS_ToFloat32(ctx, &tilemap->m.sprites[i].v2, JS_GetPropertyStr(ctx, sprite, "v2"));
                
                    JS_ToUint32(ctx, &tilemap->m.sprites[i].r, JS_GetPropertyStr(ctx, sprite, "r"));
                    JS_ToUint32(ctx, &tilemap->m.sprites[i].r, JS_GetPropertyStr(ctx, sprite, "g"));
                    JS_ToUint32(ctx, &tilemap->m.sprites[i].r, JS_GetPropertyStr(ctx, sprite, "b"));
                    JS_ToUint32(ctx, &tilemap->m.sprites[i].r, JS_GetPropertyStr(ctx, sprite, "a"));
                }
            }
            break;
        case 1:
            {
                uint32_t tmp_size = 0;
                athena_sprite_material *tmp_data = JS_GetArrayBuffer(ctx, &tmp_size, val);

                if (tmp_data) {
                    if (tmp_data != tilemap->m.sprites) {
                        free(tilemap->m.sprites);
                    }

                    tilemap->m.materials = tmp_data;
                    tilemap->m.material_count = tmp_size/sizeof(athena_sprite_material);

                    return JS_UNDEFINED;
                }

	            JSValue materials_array = val;

                JS_ToUint32(ctx, &tilemap->m.material_count, JS_GetPropertyStr(ctx, materials_array, "length"));

                if (tilemap->m.materials) free(tilemap->m.materials);

                tilemap->m.materials = malloc(sizeof(athena_sprite_material)*tilemap->m.material_count);

                for (int i = 0; i < tilemap->m.material_count; i++) {
                    JSValue material = JS_GetPropertyUint32(ctx, materials_array, i);
                
                    JS_ToInt32(ctx, &tilemap->m.materials[i].texture_index, JS_GetPropertyStr(ctx, material, "texture_index"));
                    JS_ToInt64(ctx, &tilemap->m.materials[i].blend_mode, JS_GetPropertyStr(ctx, material, "blend_mode"));
                    JS_ToUint32(ctx, &tilemap->m.materials[i].end, JS_GetPropertyStr(ctx, material, "end_offset"));
                }
            }
            break;
        case 2:
            {

	            JSValue textures_array = val;

	            JS_ToUint32(ctx, &tilemap->m.texture_count, JS_GetPropertyStr(ctx, textures_array, "length"));

                if (tilemap->m.textures) free(tilemap->m.textures);

                JS_FreeValue(ctx, tilemap->textures_array);

	            tilemap->m.textures = malloc(sizeof(GSSURFACE*)*tilemap->m.texture_count);
	            tilemap->textures_array = JS_NewArray(ctx);

	            for (int i = 0; i < tilemap->m.texture_count; i++) {
	            	JSValue texture = JS_GetPropertyUint32(ctx, textures_array, i);
                
	            	if (JS_IsString(texture)) {
                        const char *texture_path = JS_ToCString(ctx, texture);
                        JSImageData* image = js_mallocz(ctx, sizeof(*image));
                    
                        JSValue img_obj = JS_UNDEFINED;
                    
                        image->tex = malloc(sizeof(GSSURFACE));
                        load_image(image->tex, texture_path, true);
                    
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
                    
                		img_obj = JS_NewObjectClass(ctx, get_img_class_id());    
                		JS_SetOpaque(img_obj, image);
                    
                        JS_SetPropertyUint32(ctx, tilemap->textures_array, i, img_obj);
                    
                        tilemap->m.textures[i] = image->tex;
                    
                        JS_FreeCString(ctx, texture_path);
                    } else {
                        JSImageData* image = JS_GetOpaque2(ctx, texture, get_img_class_id());
                    
                        JS_SetPropertyUint32(ctx, tilemap->textures_array, i, texture);
                    
                        tilemap->m.textures[i] = image->tex;
                    
                        JS_DupValue(ctx, texture);
                    }
	            }
            }
            break;
        
        default:
            break;
    }


    return JS_UNDEFINED;
}


static JSClassDef js_tilemap_class = {
    "TileMap",
    .finalizer = athena_tilemap_dtor,
}; 

static const JSCFunctionListEntry js_tilemap_proto_funcs[] = {
    JS_CFUNC_DEF("render", 3, athena_tilemap_render),
	JS_CFUNC_DEF("free", 0, athena_tilemap_free),

    JS_CGETSET_MAGIC_DEF("sprites", js_tilemap_get, js_tilemap_set, 0),
    JS_CGETSET_MAGIC_DEF("materials", js_tilemap_get, js_tilemap_set, 1),
    JS_CGETSET_MAGIC_DEF("textures", js_tilemap_get, js_tilemap_set, 2),

	// JS_CGETSET_MAGIC_DEF("width", js_tilemap_get, js_tilemap_set, 0),
};

static JSValue athena_set_camera(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	float src_x, src_y;

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

static const JSCFunctionListEntry js_tilemap_funcs[] = {
    JS_CFUNC_DEF("init", 0, athena_init),
    JS_CFUNC_DEF("begin", 0, athena_begin),
    JS_CFUNC_DEF("setCamera", 2, athena_set_camera),
};

static int tilemap_init(JSContext *ctx, JSModuleDef *m) {
    JSValue tilemap_proto, tilemap_class;
    
    /* create the Point class */
    JS_NewClassID(&js_tilemap_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_tilemap_class_id, &js_tilemap_class);

    tilemap_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, tilemap_proto, js_tilemap_proto_funcs, countof(js_tilemap_proto_funcs));
    
    tilemap_class = JS_NewCFunction2(ctx, athena_tilemap_ctor, "TileMap", 2, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, tilemap_class, tilemap_proto);
    JS_SetClassProto(ctx, js_tilemap_class_id, tilemap_proto);

	JS_SetPropertyFunctionList(ctx, tilemap_class, js_tilemap_funcs, countof(js_tilemap_funcs));
                      
    JS_SetModuleExport(ctx, m, "default", tilemap_class);
    return 0;
}

JSModuleDef *athena_tilemap_init(JSContext* ctx){
    JSModuleDef *m;
    m = JS_NewCModule(ctx, "TileMap", tilemap_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "default");
    return m;
}
