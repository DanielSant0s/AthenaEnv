#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "ath_env.h"

static JSClassID js_image_class_id;

JSClassID get_img_class_id(){
	return js_image_class_id;
}

static int append_img(JSImageData* img, JSImgList* list)
{
    JSImageData** aux = malloc((list->size+1)*sizeof(JSImageData*));

	if(list->size > 0){
		memcpy(aux, list->list, list->size*sizeof(JSImageData*));
		free(list->list);
	}
    
    list->list = aux;
	list->list[list->size] = img;
    list->size++;
    
    return 0;
}

static JSValue athena_image_isloaded(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSImageData *image = JS_GetOpaque2(ctx, this_val, js_image_class_id);

	return (image->loaded? JS_TRUE : JS_FALSE);
	
}

static void athena_image_dtor(JSRuntime *rt, JSValue val){
		JSImageData *image = JS_GetOpaque(val, js_image_class_id);

		UnloadTexture(&(image->tex));

		free(image->tex.Mem);
		image->tex.Mem = NULL;

		if(image->tex.Clut != NULL)
		{
			free(image->tex.Clut);
			image->tex.Clut = NULL;
		}

		js_free_rt(rt, image);
	}

static JSValue athena_image_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
	if (argc != 1 && argc != 2 && argc != 3) return JS_ThrowSyntaxError(ctx, "Image takes 1, 2 or 3 arguments");

    JSImageData* image;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

	image = js_mallocz(ctx, sizeof(*image));
    if (!image)
        return JS_EXCEPTION;

	image->tex.Width = 0;
	image->tex.Height = 0;

	image->path = JS_ToCString(ctx, argv[0]);

	image->delayed = true;
	if (argc > 1) image->delayed = JS_ToBool(ctx, argv[1]);
	if (argc > 2) {
		append_img(image, JS_GetOpaque2(ctx, argv[2], get_imglist_class_id()));
		image->loaded = false;
		goto register_obj;
	}

	load_image(&(image->tex), image->path, image->delayed);

	image->loaded = true;
	image->width = image->tex.Width;
	image->height = image->tex.Height;
	image->endx = image->tex.Width;
	image->endy = image->tex.Height;

 register_obj:
	image->startx = 0.0f;
	image->starty = 0.0f;
	image->angle = 0.0f;
	image->color = 0x80808080;
	image->tex.Filter = GS_FILTER_NEAREST;

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, js_image_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;
    JS_SetOpaque(obj, image);
    return obj;

 fail:
    js_free(ctx, image);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue athena_image_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	float x, y;

	JSImageData *image = JS_GetOpaque2(ctx, this_val, js_image_class_id);

	JS_ToFloat32(ctx, &x, argv[0]);
	JS_ToFloat32(ctx, &y, argv[1]);

	if(image->angle != 0.0f){
		drawImageRotate(&(image->tex), x, y, image->width, image->height, image->startx, image->starty, image->endx, image->endy, image->angle, image->color);
	} else {
		drawImage(&(image->tex), x, y, image->width, image->height, image->startx, image->starty, image->endx, image->endy, image->color);
	}

	return JS_UNDEFINED;
}

static JSValue athena_image_optimize(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	JSImageData *image = JS_GetOpaque2(ctx, this_val, js_image_class_id);

	switch(image->tex.PSM) {
		case GS_PSM_CT24:
			gsKit_texture_to_psm16(&(image->tex));
			return JS_NewBool(ctx, true);
			break;
		default:
			return JS_NewBool(ctx, false);
	}

	return JS_UNDEFINED;
}


static JSValue js_image_get(JSContext *ctx, JSValueConst this_val, int magic)
{
	JSValue val = JS_UNDEFINED;
    JSImageData *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
	
    if (!s){
		return JS_EXCEPTION;
	}

	switch(magic) {
		case 0:
			val = JS_NewFloat32(ctx, (float)s->width);
			break;
		case 1:
			val = JS_NewFloat32(ctx, (float)s->height);
			break;
		case 2:
			val = JS_NewFloat32(ctx, s->startx);
			break;
		case 3:
			val = JS_NewFloat32(ctx, s->starty);
			break;
		case 4:
			val = JS_NewFloat32(ctx, s->endx);
			break;
		case 5:
			val = JS_NewFloat32(ctx, s->endy);
			break;
		case 6:
			val = JS_NewFloat32(ctx, s->angle);
			break;
	}

	return val;

}

static JSValue js_image_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    JSImageData *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    float v;

    if (!s || JS_ToFloat32(ctx, &v, val)){
		return JS_EXCEPTION;
	}

	switch(magic) {
		case 0:
			s->width = v;
			break;
		case 1:
			s->height = v;
			break;
		case 2:
			s->startx = v;
			break;
		case 3:
			s->starty = v;
			break;
		case 4:
			s->endx = v;
			break;
		case 5:
			s->endy = v;
			break;
		case 6:
			s->angle = v;
			break;
	}

    return JS_UNDEFINED;
}

static JSValue athena_image_get_uint(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSImageData *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    if (!s)
        return JS_EXCEPTION;

	switch (magic) {
		case 0:
			return JS_NewUint32(ctx, s->color);
		case 1:
			return JS_NewUint32(ctx, s->tex.Filter);
		case 2:
			return JS_NewUint32(ctx, gsKit_texture_size_ee(s->tex.Width, s->tex.Height, s->tex.PSM));
		case 3:
			switch (s->tex.PSM) {
				case GS_PSM_T4:
					return JS_NewUint32(ctx, 4);
				case GS_PSM_T8:
					return JS_NewUint32(ctx, 8);
				case GS_PSM_CT16:
				case GS_PSM_CT16S:
					return JS_NewUint32(ctx, 16);
				case GS_PSM_CT24:
					return JS_NewUint32(ctx, 24);
				case GS_PSM_CT32:
					return JS_NewUint32(ctx, 32);
			}
		case 4:
			return JS_NewBool(ctx, s->delayed);
		case 5:
			return JS_NewArrayBuffer(ctx, s->tex.Mem, gsKit_texture_size_ee(s->tex.Width, s->tex.Height, s->tex.PSM), NULL, NULL, 1);
		case 6:
			if (s->tex.PSM == GS_PSM_T4) {
				return JS_NewArrayBuffer(ctx, s->tex.Clut, gsKit_texture_size_ee(8, 2, GS_PSM_CT32), NULL, NULL, 1);
			} else if (s->tex.PSM == GS_PSM_T8) {
					return JS_NewArrayBuffer(ctx, s->tex.Clut, gsKit_texture_size_ee(16, 16, GS_PSM_CT32), NULL, NULL, 1);
			}
			
	}

	return JS_UNDEFINED;
}

static JSValue athena_image_set_uint(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    JSImageData *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    uint32_t value, arr_size;
    if (!s)
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &value, val))
        return JS_EXCEPTION;

	switch (magic) {
		case 0:
			s->color = value;
			break;
		case 1:
			s->tex.Filter = value;
			break;
		case 5:
			if (s->tex.Delayed) {
				void *pixels = JS_GetArrayBuffer(ctx, &arr_size, val);
				if (pixels != s->tex.Mem) {
					free(s->tex.Mem);
					s->tex.Mem = pixels;
				}
			}
			break;
		case 6:
			if (s->tex.Clut && s->tex.Delayed) {
				void *palette = JS_GetArrayBuffer(ctx, &arr_size, val);
				if (palette != s->tex.Clut) {
					free(s->tex.Clut);
					s->tex.Clut = palette;
				}
			}
			break;
			

	}

    return JS_UNDEFINED;
}

static JSClassDef js_image_class = {
    "Image",
    .finalizer = athena_image_dtor,
}; 

static const JSCFunctionListEntry js_image_proto_funcs[] = {
    JS_CFUNC_DEF("draw", 2, athena_image_draw),
	JS_CFUNC_DEF("ready", 0, athena_image_isloaded),
	JS_CFUNC_DEF("optimize", 0, athena_image_optimize),
	JS_CGETSET_MAGIC_DEF("width", js_image_get, js_image_set, 0),
	JS_CGETSET_MAGIC_DEF("height", js_image_get, js_image_set, 1),
	JS_CGETSET_MAGIC_DEF("startx", js_image_get, js_image_set, 2),
	JS_CGETSET_MAGIC_DEF("starty", js_image_get, js_image_set, 3),
	JS_CGETSET_MAGIC_DEF("endx", js_image_get, js_image_set, 4),
	JS_CGETSET_MAGIC_DEF("endy", js_image_get, js_image_set, 5),
	JS_CGETSET_MAGIC_DEF("angle", js_image_get, js_image_set, 6),
	JS_CGETSET_MAGIC_DEF("color", athena_image_get_uint, athena_image_set_uint, 0),
	JS_CGETSET_MAGIC_DEF("filter", athena_image_get_uint, athena_image_set_uint, 1),
	JS_CGETSET_MAGIC_DEF("size", athena_image_get_uint, athena_image_set_uint, 2),
	JS_CGETSET_MAGIC_DEF("bpp", athena_image_get_uint, athena_image_set_uint, 3),
	JS_CGETSET_MAGIC_DEF("delayed", athena_image_get_uint, athena_image_set_uint, 4),
	JS_CGETSET_MAGIC_DEF("pixels", athena_image_get_uint, athena_image_set_uint, 5),
	JS_CGETSET_MAGIC_DEF("palette", athena_image_get_uint, athena_image_set_uint, 6),
};

static int image_init(JSContext *ctx, JSModuleDef *m) {
    JSValue image_proto, image_class;
    
    /* create the Point class */
    JS_NewClassID(&js_image_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_image_class_id, &js_image_class);

    image_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, image_proto, js_image_proto_funcs, countof(js_image_proto_funcs));
    
    image_class = JS_NewCFunction2(ctx, athena_image_ctor, "Image", 2, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, image_class, image_proto);
    JS_SetClassProto(ctx, js_image_class_id, image_proto);
                      
    JS_SetModuleExport(ctx, m, "Image", image_class);
    return 0;
}

JSModuleDef *athena_image_init(JSContext* ctx){
    JSModuleDef *m;
    m = JS_NewCModule(ctx, "Image", image_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "Image");
    return m;
}
