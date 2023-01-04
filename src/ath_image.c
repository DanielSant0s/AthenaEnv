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

int append_img(AsyncImage* img, ImgList* list)
{
    AsyncImage** aux = malloc((list->size+1)*sizeof(AsyncImage*));

	if(list->size > 0){
		memcpy(aux, list->list, list->size*sizeof(AsyncImage*));
		free(list->list);
	}
    
    list->list = aux;
	list->list[list->size] = img;
    list->size++;
    
    return 0;
}

static int load_img_async(GSTEXTURE* image, const char* path, bool delayed, ImgList* list) {
	AsyncImage* async_img = malloc(sizeof(AsyncImage));

	async_img->path = path;
	async_img->handle = image;
	async_img->delayed = delayed;

	append_img(async_img, list);

	return 0;
}

/*

static JSValue athena_image_isloaded(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if(argc != 0) return JS_ThrowSyntaxError(ctx, "isLoaded takes no arguments");

	bool loaded = get_obj_boolean(ctx, -1, "\xff""\xff""loaded");
	GSTEXTURE* testimg = (GSTEXTURE*)get_obj_uint(ctx, -1, "\xff""\xff""data");

	if(loaded){
		duk_push_boolean(ctx, true);
		return 1;
	} else {
		if (testimg->Width != 0){
			duk_push_this(ctx);

		    duk_push_boolean(ctx, true);
    		duk_put_prop_string(ctx, -2, "\xff""\xff""loaded");

				duk_push_number(ctx, (float)(testimg->Width));
    			duk_put_prop_string(ctx, -2, "width");

				duk_push_number(ctx, (float)(testimg->Height));
    			duk_put_prop_string(ctx, -2, "height");

				duk_push_number(ctx, (float)(testimg->Width));
    			duk_put_prop_string(ctx, -2, "endx");

				duk_push_number(ctx, (float)(testimg->Height));
    			duk_put_prop_string(ctx, -2, "endy");

			duk_push_boolean(ctx, true);
			return 1;
		}
		duk_push_boolean(ctx, false);
		return 1;
	}
	
}

*/

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

	const char* text = JS_ToCString(ctx, argv[0]);

	bool delayed = true;
	if (argc > 1) delayed = JS_ToBool(ctx, argv[1]);

	load_image(&(image->tex), text, delayed);

	image->width = image->tex.Width;
	image->height = image->tex.Height;
	image->endx = image->tex.Width;
	image->endy = image->tex.Height;
	image->startx = 0.0;
	image->starty = 0.0;
	image->angle = 0.0;
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
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "draw takes x and y arguments(2)!");

	double x, y;

	JSImageData *image = JS_GetOpaque2(ctx, this_val, js_image_class_id);

	JS_ToFloat64(ctx, &x, argv[0]);
	JS_ToFloat64(ctx, &y, argv[1]);

	if(image->angle != 0.0){
		drawImageRotate(&(image->tex), x, y, image->width, image->height, image->startx, image->starty, image->endx, image->endy, image->angle, image->color);
	} else {
		drawImage(&(image->tex), x, y, image->width, image->height, image->startx, image->starty, image->endx, image->endy, image->color);
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
			val = JS_NewFloat64(ctx, (double)s->width);
			break;
		case 1:
			val = JS_NewFloat64(ctx, (double)s->height);
			break;
		case 2:
			val = JS_NewFloat64(ctx, s->startx);
			break;
		case 3:
			val = JS_NewFloat64(ctx, s->starty);
			break;
		case 4:
			val = JS_NewFloat64(ctx, s->endx);
			break;
		case 5:
			val = JS_NewFloat64(ctx, s->endy);
			break;
		case 6:
			val = JS_NewFloat64(ctx, s->angle);
			break;
	}

	return val;

}

static JSValue js_image_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    JSImageData *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    double v;

    if (!s || JS_ToFloat64(ctx, &v, val)){
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

	return JS_NewUint32(ctx, (magic == 0? s->color : s->tex.Filter));
}

static JSValue athena_image_set_uint(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    JSImageData *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    uint32_t value;
    if (!s)
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &value, val))
        return JS_EXCEPTION;
	
	if(magic == 0){
		s->color = value;
	} else {
		s->tex.Filter = value;
	}

    return JS_UNDEFINED;
}

static JSClassDef js_image_class = {
    "Image",
    .finalizer = athena_image_dtor,
}; 

static const JSCFunctionListEntry js_image_proto_funcs[] = {
    JS_CFUNC_DEF("draw", 2, athena_image_draw),
	JS_CGETSET_MAGIC_DEF("width", js_image_get, js_image_set, 0),
	JS_CGETSET_MAGIC_DEF("height", js_image_get, js_image_set, 1),
	JS_CGETSET_MAGIC_DEF("startx", js_image_get, js_image_set, 2),
	JS_CGETSET_MAGIC_DEF("starty", js_image_get, js_image_set, 3),
	JS_CGETSET_MAGIC_DEF("endx", js_image_get, js_image_set, 4),
	JS_CGETSET_MAGIC_DEF("endy", js_image_get, js_image_set, 5),
	JS_CGETSET_MAGIC_DEF("angle", js_image_get, js_image_set, 6),
	JS_CGETSET_MAGIC_DEF("color", athena_image_get_uint, athena_image_set_uint, 0),
	JS_CGETSET_MAGIC_DEF("filter", athena_image_get_uint, athena_image_set_uint, 1),
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
