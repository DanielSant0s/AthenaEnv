
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <graphics.h>
#include <fntsys.h>
#include <ath_env.h>

enum {
    image_font,
    truetype_font
} FontTypes;

bool truetypefnt_loaded = false;

enum {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
} FontAlign;

typedef struct {
    uint32_t type;
    GSFONT* data;
    int id;
    Color color;
    float scale;

    float outline;
    Color outline_color;
    float dropshadow;
    Color dropshadow_color;
} JSFontData;

typedef struct {
    JSFontData* font_data;
    const char* text;
    uint8_t align;
    Coords size;
} JSFontRenderData;

static JSClassID js_font_class_id;
static JSClassID js_fontrender_class_id;

static void athena_font_dtor(JSRuntime *rt, JSValue val){
    JSFontData *font = JS_GetOpaque(val, js_font_class_id);

    if (font->type == image_font) {
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

        if (strcmp(path, "default") == 0) {
            font->id = 0;
            font->type = truetype_font;
            if (!truetypefnt_loaded) {
                fntLoadDefault(NULL);
                fntSetCharSize(font->id, FNTSYS_CHAR_SIZE*64, FNTSYS_CHAR_SIZE*64);
                truetypefnt_loaded = true;
            }
            
        } else {
            dbgprintf("%s\n", path);
            font->id = fntLoadFile(path);
            font->type = truetype_font;

            if (font->id == -1) {
                font->data = loadFont(path);
                if (font->data == NULL) return JS_EXCEPTION;
                font->type = image_font;
            } else {
                fntSetCharSize(font->id, FNTSYS_CHAR_SIZE*64, FNTSYS_CHAR_SIZE*64);
            }
        }
        
        JS_FreeCString(ctx, path);
    }

    font->color = 0x80808080;
    font->scale = 1.0f;

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

    JSFontData *font = JS_GetOpaque2(ctx, this_val, js_font_class_id);

    JS_ToFloat32(ctx, &x, argv[0]);
	JS_ToFloat32(ctx, &y, argv[1]);
    
    const char* text = JS_ToCString(ctx, argv[2]);

    if (font->type == image_font){
	    printFontText(font->data, text, x, y, font->scale, font->color);
    } else {
        fntRenderStringPlus(font->id, x, y, 0, 0, 0, text, font->scale, font->color, font->outline, font->outline_color, font->dropshadow, font->dropshadow_color);
    }

    JS_FreeCString(ctx, text);

	return JS_UNDEFINED;
}

static JSValue athena_font_gettextsize(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    Coords size;
    JSValue obj;

    JSFontData *font = JS_GetOpaque2(ctx, this_val, js_font_class_id);
    const char* str = JS_ToCString(ctx, argv[0]);

    if (font->type == truetype_font) {
        size = fntGetTextSize(font->id, str);
    } else {
        size.width = strlen(str) * font->scale * 0.68f * 26.0f;
        size.height = 26.0f * font->scale;
    }

    JS_FreeCString(ctx, str);

    obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "width", JS_NewUint32(ctx, size.width), JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(ctx, obj, "height", JS_NewUint32(ctx, size.height), JS_PROP_C_W_E);

	return obj;
}

static JSValue athena_font_get_scale(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSFontData *s = JS_GetOpaque2(ctx, this_val, js_font_class_id);
    if (!s)
        return JS_EXCEPTION;
    

    switch (magic) {
        case 0:
            return JS_NewFloat32(ctx, s->scale); 
        case 1:
            return JS_NewFloat32(ctx, s->outline); 
        case 2:
            return JS_NewFloat32(ctx, s->dropshadow); 
    }

    return JS_UNDEFINED;
}

static JSValue athena_font_set_scale(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    JSFontData *s = JS_GetOpaque2(ctx, this_val, js_font_class_id);
    float scale;

    if (!s)
        return JS_EXCEPTION;
    if (JS_ToFloat32(ctx, &scale, val))
        return JS_EXCEPTION;

    

    switch (magic) {
        case 0:
            s->scale = scale;
            break;
        case 1:
            s->outline = scale;
            break;
        case 2:
            s->dropshadow = scale;
            break;
    }

    return JS_UNDEFINED;
}

static JSValue athena_font_get_color(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSFontData *s = JS_GetOpaque2(ctx, this_val, js_font_class_id);
    if (!s)
        return JS_EXCEPTION;

    switch (magic) {
        case 0:
            return JS_NewUint32(ctx, s->color);
        case 1:
            return JS_NewUint32(ctx, s->outline_color);
        case 2:
            return JS_NewUint32(ctx, s->dropshadow_color);
    }
    
    return JS_UNDEFINED;
}

static JSValue athena_font_set_color(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    JSFontData *s = JS_GetOpaque2(ctx, this_val, js_font_class_id);
    Color color;
    if (!s)
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &color, val))
        return JS_EXCEPTION;

    switch (magic) {
        case 0:
            s->color = color;
            break;
        case 1:
            s->outline_color = color;
            break;
        case 2:
            s->dropshadow_color = color;
            break;
    }

    
    return JS_UNDEFINED;
}

static JSValue athena_render_font(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv){
    JSFontRenderData* render_data;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

    obj = JS_NewObjectClass(ctx, js_fontrender_class_id);
	if (!JS_IsException(obj)) {
	    render_data = js_mallocz(ctx, sizeof(*render_data));
        if (!render_data)
            return JS_EXCEPTION;

        render_data->font_data = JS_GetOpaque2(ctx, this_val, js_font_class_id);
        render_data->text = JS_ToCString(ctx, argv[0]);

        if (argc == 2) {
            JS_ToUint32(ctx, &render_data->align, argv[1]);
        }

        if (render_data->font_data->type == truetype_font) {
            render_data->size = fntGetTextSize(render_data->font_data->id, render_data->text);
        } else if {
            render_data->size.width = strlen(render_data->text) * render_data->font_data->scale * 0.68f * 26.0f;
            render_data->size.height = 26.0f * render_data->font_data->scale;
        }

        JS_SetOpaque(obj, render_data);
        return obj;
    }

    js_free(ctx, render_data);
    return JS_EXCEPTION;
}

static JSValue athena_fontrender_print(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    float x, y;

    JSFontRenderData *render_data = JS_GetOpaque2(ctx, this_val, js_fontrender_class_id);

    JS_ToFloat32(ctx, &x, argv[0]);
	JS_ToFloat32(ctx, &y, argv[1]);

    if (render_data->font_data->type == image_font){
	    printFontText(render_data->font_data->data, render_data->text, x, y, render_data->font_data->scale, render_data->font_data->color);
    } else {
        fntRenderString(render_data->font_data->id, x, y, 0, 0, 0, render_data->text, render_data->font_data->scale, render_data->font_data->color);
    }

	return JS_UNDEFINED;
}

static JSClassDef js_fontrender_class = {
    "FontRender",
    //.finalizer = js_std_file_finalizer,
}; 

static const JSCFunctionListEntry js_fontrender_proto_funcs[] = {
    JS_CFUNC_DEF("print", 2, athena_fontrender_print),
};

static JSClassDef js_font_class = {
    "Font",
    .finalizer = athena_font_dtor,
}; 

static const JSCFunctionListEntry js_font_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("scale", athena_font_get_scale, athena_font_set_scale, 0),
    JS_CGETSET_MAGIC_DEF("outline", athena_font_get_scale, athena_font_set_scale, 1),
    JS_CGETSET_MAGIC_DEF("dropshadow", athena_font_get_scale, athena_font_set_scale, 2),

    JS_CGETSET_MAGIC_DEF("color", athena_font_get_color, athena_font_set_color, 0),
    JS_CGETSET_MAGIC_DEF("outline_color", athena_font_get_color, athena_font_set_color, 1),
    JS_CGETSET_MAGIC_DEF("dropshadow_color", athena_font_get_color, athena_font_set_color, 2),

    JS_CFUNC_DEF("print", 3, athena_font_print),
    JS_CFUNC_DEF("render", 3, athena_render_font),
    JS_CFUNC_DEF("getTextSize", 1, athena_font_gettextsize),
};

static void js_renderfont_init(JSContext *ctx)
{
    JSValue proto;

    /* the class ID is created once */
    JS_NewClassID(&js_fontrender_class_id);
    /* the class is created once per runtime */
    JS_NewClass(JS_GetRuntime(ctx), js_fontrender_class_id, &js_fontrender_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_fontrender_proto_funcs,
                               countof(js_fontrender_proto_funcs));
    JS_SetClassProto(ctx, js_fontrender_class_id, proto);
}

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

    js_renderfont_init(ctx);

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