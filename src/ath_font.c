
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "include/graphics.h"
#include "include/fntsys.h"
#include "ath_env.h"

const uint32_t osdsys_font = 0;
const uint32_t image_font = 1;
const uint32_t truetype_font = 2;

typedef struct {
    uint32_t type;
    GSFONT* data;
    int id;
    Color color;
    float scale;
} JSFontData;

static JSClassID js_font_class_id;

static void athena_font_dtor(JSRuntime *rt, JSValue val){
    JSFontData *font = JS_GetOpaque(val, js_font_class_id);

    if (font->type == 1) {
	    unloadFont(font->data);
    } else {
        fntRelease(font->id);
    }

    js_free_rt(rt, font);
}

static JSValue athena_font_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv){

    JSFontData* font;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

    font = js_mallocz(ctx, sizeof(*font));
    if (!font)
        return JS_EXCEPTION;

    if (argc == 1) {
        const char* path = JS_ToCString(ctx, argv[0]);
        dbgprintf("%s\n", path);
        font->id = fntLoadFile(path);
        font->type = truetype_font;

        if (font->id == -1) {
            font->data = loadFont(path);
            if (font->data == NULL) return JS_EXCEPTION;
            font->type = image_font;
        }
    } else {
        font->type = osdsys_font;
    }

    font->color = 0x80808080;
    font->scale = 1.0;

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, js_font_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;
    JS_SetOpaque(obj, font);
    return obj;

 fail:
    js_free(ctx, font);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue athena_font_print(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    float x, y;
	if (argc != 3) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

    JSFontData *font = JS_GetOpaque2(ctx, this_val, js_font_class_id);

    JS_ToFloat32(ctx, &x, argv[0]);
	JS_ToFloat32(ctx, &y, argv[1]);
    const char* text = JS_ToCString(ctx, argv[2]);

    if (font->type == 1){
	    printFontText(font->data, text, x, y, font->scale, font->color);
    } else if (font->type == 0){
        printFontMText(text, x, y, font->scale, font->color);
    } else {
        fntRenderString(font->id, x, y, 0, 0, 0, text, font->color);
    }

	return JS_UNDEFINED;
}

static JSValue athena_font_gettextsize(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    Coords size;
    JSValue obj;

    JSFontData *font = JS_GetOpaque2(ctx, this_val, js_font_class_id);

    if (font->type == 2) {
        size = fntGetTextSize(font->id, JS_ToCString(ctx, argv[0]));
    }

    obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "width", JS_NewUint32(ctx, size.width), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(ctx, obj, "height", JS_NewUint32(ctx, size.height), JS_PROP_C_W_E);

	return obj;
}

static JSValue athena_font_get_scale(JSContext *ctx, JSValueConst this_val)
{
    JSFontData *s = JS_GetOpaque2(ctx, this_val, js_font_class_id);
    if (!s)
        return JS_EXCEPTION;
    return JS_NewFloat32(ctx, s->scale);
}

static JSValue athena_font_set_scale(JSContext *ctx, JSValueConst this_val, JSValue val)
{
    JSFontData *s = JS_GetOpaque2(ctx, this_val, js_font_class_id);
    float scale;
    if (!s)
        return JS_EXCEPTION;
    if (JS_ToFloat32(ctx, &scale, val))
        return JS_EXCEPTION;
    s->scale = scale;
    if (s->type == truetype_font){
        fntSetCharSize(s->id, FNTSYS_CHAR_SIZE*64*scale, FNTSYS_CHAR_SIZE*64*scale);
    }

    return JS_UNDEFINED;
}

static JSValue athena_font_get_color(JSContext *ctx, JSValueConst this_val)
{
    JSFontData *s = JS_GetOpaque2(ctx, this_val, js_font_class_id);
    if (!s)
        return JS_EXCEPTION;
    return JS_NewUint32(ctx, s->color);
}

static JSValue athena_font_set_color(JSContext *ctx, JSValueConst this_val, JSValue val)
{
    JSFontData *s = JS_GetOpaque2(ctx, this_val, js_font_class_id);
    Color color;
    if (!s)
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &color, val))
        return JS_EXCEPTION;
    s->color = color;
    return JS_UNDEFINED;
}

static JSClassDef js_font_class = {
    "Font",
    .finalizer = athena_font_dtor,
}; 

static const JSCFunctionListEntry js_font_proto_funcs[] = {
    //JS_CGETSET_MAGIC_DEF("x", js_point_get_xy, js_point_set_xy, 0),
    JS_CGETSET_DEF("scale", athena_font_get_scale, athena_font_set_scale),
    JS_CGETSET_DEF("color", athena_font_get_color, athena_font_set_color),
    JS_CFUNC_DEF("print", 3, athena_font_print),
    JS_CFUNC_DEF("getTextSize", 1, athena_font_gettextsize),
};

static int font_init(JSContext *ctx, JSModuleDef *m) {
    JSValue font_proto, font_class;
    
    /* create the Point class */
    JS_NewClassID(&js_font_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_font_class_id, &js_font_class);

    font_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, font_proto, js_font_proto_funcs, countof(js_font_proto_funcs));
    
    font_class = JS_NewCFunction2(ctx, athena_font_ctor, "Font", 2, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, font_class, font_proto);
    JS_SetClassProto(ctx, js_font_class_id, font_proto);
                      
    JS_SetModuleExport(ctx, m, "Font", font_class);

    fntInit();
    return 0;
}

JSModuleDef *athena_font_init(JSContext *ctx)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, "Font", font_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "Font");
    return m;
}