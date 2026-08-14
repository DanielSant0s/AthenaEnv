#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include <athena/image.h>
#include <graphics.h>
#include <texture_manager.h>
#include <athena/image_list.h>
#include <ath_bindings.h>

static JSClassID js_image_class_id;

JSClassID get_img_class_id(void) {
    return js_image_class_id;
}

static void js_image_dtor(JSRuntime *rt, JSValue val) {
    AthenaImage *image = JS_GetOpaque(val, js_image_class_id);
    if (!image)
        return;
    athena_image_destroy(image);
    JS_SetOpaque(val, NULL);
}

static JSValue js_image_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    AthenaImage *image = NULL;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;
    const char *path = NULL;

    if (argc > 0)
        path = JS_ToCString(ctx, argv[0]);

    image = path ? athena_image_create(path, true) : athena_image_create_empty(true);
    if (path)
        JS_FreeCString(ctx, path);

    if (!image)
        return JS_EXCEPTION;

    if (argc > 1) {
        AthenaImageList *list = JS_GetOpaque2(ctx, argv[1], get_imglist_class_id());
        if (list)
            athena_image_list_append(list, image);
    }

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
    athena_image_destroy(image);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_image_isloaded(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    AthenaImage *image = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    return athena_image_is_loaded(image) ? JS_TRUE : JS_FALSE;
}

static JSValue js_image_free(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    js_image_dtor(JS_GetRuntime(ctx), this_val);
    return JS_UNDEFINED;
}

static inline int get_optional_float(JSContext *ctx, JSValue obj, const char *prop, float *out)
{
    JSValue v = JS_GetPropertyStr(ctx, obj, prop);
    if (JS_IsUndefined(v) || JS_IsNull(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (JS_ToFloat32(ctx, out, v)) {
        JS_FreeValue(ctx, v);
        return -1;
    }
    JS_FreeValue(ctx, v);
    return 0;
}

static inline int get_optional_uint32(JSContext *ctx, JSValue obj, const char *prop, uint32_t *out)
{
    JSValue v = JS_GetPropertyStr(ctx, obj, prop);
    if (JS_IsUndefined(v) || JS_IsNull(v)) {
        JS_FreeValue(ctx, v);
        return 0;
    }
    if (JS_ToUint32(ctx, out, v)) {
        JS_FreeValue(ctx, v);
        return -1;
    }
    JS_FreeValue(ctx, v);
    return 0;
}

static JSValue js_image_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    AthenaImage *image = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    
    if (!image) {
        return JS_ThrowTypeError(ctx, "invalid Image object");
    }
    
    if (!image->loaded) {
        return JS_ThrowInternalError(ctx, "image not loaded");
    }
    
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "draw requires at least (x, y)");
    }
    
    float x, y;
    if (JS_ToFloat32(ctx, &x, argv[0]) || JS_ToFloat32(ctx, &y, argv[1])) {
        return JS_EXCEPTION;
    }
    
    float width  = image->width;
    float height = image->height;
    float startx = image->startx;
    float starty = image->starty;
    float endx   = image->endx;
    float endy   = image->endy;
    float angle  = image->angle;
    uint32_t color = image->color;
    
    if (argc > 2 && JS_IsObject(argv[2]) && !JS_IsArray(ctx, argv[2])) {
        if (get_optional_float(ctx, argv[2], "width",  &width)  < 0 ||
            get_optional_float(ctx, argv[2], "height", &height) < 0 ||
            get_optional_float(ctx, argv[2], "startx", &startx) < 0 ||
            get_optional_float(ctx, argv[2], "starty", &starty) < 0 ||
            get_optional_float(ctx, argv[2], "endx",   &endx)   < 0 ||
            get_optional_float(ctx, argv[2], "endy",   &endy)   < 0 ||
            get_optional_float(ctx, argv[2], "angle",  &angle)  < 0 ||
            get_optional_uint32(ctx, argv[2], "color", &color)  < 0)
        {
            return JS_EXCEPTION;
        }
    }

    athena_image_draw(image, x, y, width, height, startx, starty, endx, endy, angle, color);
    return JS_UNDEFINED;
}

static JSValue js_image_lock(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    AthenaImage *image = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    return JS_NewBool(ctx, athena_image_lock(image));
}

static JSValue js_image_unlock(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    AthenaImage *image = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    return JS_NewBool(ctx, athena_image_unlock(image));
}

static JSValue js_image_locked(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    AthenaImage *image = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    return JS_NewBool(ctx, athena_image_locked(image));
}

static JSValue js_image_optimize(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    AthenaImage *image = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    return JS_NewBool(ctx, athena_image_optimize(image));
}

static JSValue js_image_get(JSContext *ctx, JSValueConst this_val, int magic) {
    AthenaImage *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    if (!s)
        return JS_EXCEPTION;
    switch (magic) {
        case 0: return JS_NewFloat32(ctx, s->width);
        case 1: return JS_NewFloat32(ctx, s->height);
        case 2: return JS_NewFloat32(ctx, s->startx);
        case 3: return JS_NewFloat32(ctx, s->starty);
        case 4: return JS_NewFloat32(ctx, s->endx);
        case 5: return JS_NewFloat32(ctx, s->endy);
        case 6: return JS_NewFloat32(ctx, s->angle);
    }
    return JS_UNDEFINED;
}

static JSValue js_image_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    AthenaImage *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    float v;
    if (!s || JS_ToFloat32(ctx, &v, val))
        return JS_EXCEPTION;
    switch (magic) {
        case 0: s->width = v; break;
        case 1: s->height = v; break;
        case 2: s->startx = v; break;
        case 3: s->starty = v; break;
        case 4: s->endx = v; break;
        case 5: s->endy = v; break;
        case 6: s->angle = v; break;
    }
    return JS_UNDEFINED;
}

static JSValue js_image_get_uint(JSContext *ctx, JSValueConst this_val, int magic) {
    AthenaImage *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    if (!s)
        return JS_EXCEPTION;
    switch (magic) {
        case 0: return JS_NewUint32(ctx, s->color);
        case 1: return JS_NewUint32(ctx, s->tex->Filter);
        case 2: return JS_NewUint32(ctx, athena_surface_size(s->tex->Width, s->tex->Height, s->tex->PSM));
        case 3:
            switch (s->tex->PSM) {
                case GS_PSM_T4: return JS_NewUint32(ctx, 4);
                case GS_PSM_T8: return JS_NewUint32(ctx, 8);
                case GS_PSM_CT16:
                case GS_PSM_CT16S:
                case GS_PSMZ_16S: return JS_NewUint32(ctx, 16);
                case GS_PSM_CT24:
                case GS_PSMZ_24: return JS_NewUint32(ctx, 24);
                case GS_PSM_CT32:
                case GS_PSMZ_32: return JS_NewUint32(ctx, 32);
            }
            break;
        case 4: return JS_NewBool(ctx, s->delayed);
        case 5:
            if (s->tex->Mem)
                return JS_NewArrayBuffer(ctx, s->tex->Mem,
                    athena_surface_size(s->tex->Width, s->tex->Height, s->tex->PSM),
                    NULL, NULL, 1);
            break;
        case 6:
            if (s->tex->PSM == GS_PSM_T4)
                return JS_NewArrayBuffer(ctx, s->tex->Clut, athena_surface_size(8, 2, GS_PSM_CT32), NULL, NULL, 1);
            if (s->tex->PSM == GS_PSM_T8)
                return JS_NewArrayBuffer(ctx, s->tex->Clut, athena_surface_size(16, 16, GS_PSM_CT32), NULL, NULL, 1);
            break;
        case 7: return JS_NewUint32(ctx, s->tex->Width);
        case 8: return JS_NewUint32(ctx, s->tex->Height);
        case 9: return JS_NewUint32(ctx, s->tex->PageAligned);
    }
    return JS_UNDEFINED;
}

static JSValue js_image_set_uint(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    AthenaImage *s = JS_GetOpaque2(ctx, this_val, js_image_class_id);
    uint32_t value, arr_size;
    if (!s || JS_ToUint32(ctx, &value, val))
        return JS_EXCEPTION;
    switch (magic) {
        case 0: s->color = value; break;
        case 1: s->tex->Filter = value; break;
        case 3:
            switch (value) {
                case 4: s->tex->PSM = GS_PSM_T4; break;
                case 8: s->tex->PSM = GS_PSM_T8; break;
                case 16: s->tex->PSM = GS_PSM_CT16S; break;
                case 24: s->tex->PSM = GS_PSM_CT24; break;
                case 32: s->tex->PSM = GS_PSM_CT32; break;
                case 18: s->tex->PSM = GS_PSMZ_16S; break;
                case 26: s->tex->PSM = GS_PSMZ_24; break;
                case 34: s->tex->PSM = GS_PSMZ_32; break;
            }
            break;
        case 5:
            if (s->tex->Delayed) {
                void *pixels = JS_GetArrayBuffer(ctx, &arr_size, val);
                if (pixels != s->tex->Mem) {
                    if (s->tex->Mem)
                        free(s->tex->Mem);
                    s->tex->Mem = pixels;
                }
            }
            break;
        case 6:
            if (s->tex->Clut && s->tex->Delayed) {
                void *palette = JS_GetArrayBuffer(ctx, &arr_size, val);
                if (palette != s->tex->Clut) {
                    free(s->tex->Clut);
                    s->tex->Clut = palette;
                }
            }
            break;
        case 7: s->tex->Width = value; break;
        case 8: s->tex->Height = value; break;
        case 9: s->tex->PageAligned = value; break;
    }
    return JS_UNDEFINED;
}

static JSClassDef js_image_class = {
    "Image",
    .finalizer = js_image_dtor,
};

static const JSCFunctionListEntry js_image_proto_funcs[] = {
    JS_CFUNC_DEF("draw", 2, js_image_draw),
    JS_CFUNC_DEF("ready", 0, js_image_isloaded),
    JS_CFUNC_DEF("optimize", 0, js_image_optimize),
    JS_CFUNC_DEF("free", 0, js_image_free),
    JS_CFUNC_DEF("lock", 0, js_image_lock),
    JS_CFUNC_DEF("unlock", 0, js_image_unlock),
    JS_CFUNC_DEF("locked", 0, js_image_locked),
    JS_CGETSET_MAGIC_DEF("width", js_image_get, js_image_set, 0),
    JS_CGETSET_MAGIC_DEF("height", js_image_get, js_image_set, 1),
    JS_CGETSET_MAGIC_DEF("startx", js_image_get, js_image_set, 2),
    JS_CGETSET_MAGIC_DEF("starty", js_image_get, js_image_set, 3),
    JS_CGETSET_MAGIC_DEF("endx", js_image_get, js_image_set, 4),
    JS_CGETSET_MAGIC_DEF("endy", js_image_get, js_image_set, 5),
    JS_CGETSET_MAGIC_DEF("angle", js_image_get, js_image_set, 6),
    JS_CGETSET_MAGIC_DEF("color", js_image_get_uint, js_image_set_uint, 0),
    JS_CGETSET_MAGIC_DEF("filter", js_image_get_uint, js_image_set_uint, 1),
    JS_CGETSET_MAGIC_DEF("size", js_image_get_uint, js_image_set_uint, 2),
    JS_CGETSET_MAGIC_DEF("bpp", js_image_get_uint, js_image_set_uint, 3),
    JS_CGETSET_MAGIC_DEF("pixels", js_image_get_uint, js_image_set_uint, 5),
    JS_CGETSET_MAGIC_DEF("palette", js_image_get_uint, js_image_set_uint, 6),
    JS_CGETSET_MAGIC_DEF("texWidth", js_image_get_uint, js_image_set_uint, 7),
    JS_CGETSET_MAGIC_DEF("texHeight", js_image_get_uint, js_image_set_uint, 8),
    JS_CGETSET_MAGIC_DEF("renderable", js_image_get_uint, js_image_set_uint, 9),
};

static JSValue js_image_copy_block(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    int src_x, src_y, dst_x, dst_y;
    AthenaImage *src = JS_GetOpaque2(ctx, argv[0], js_image_class_id);
    AthenaImage *dst = JS_GetOpaque2(ctx, argv[3], js_image_class_id);
    JS_ToInt32(ctx, &src_x, argv[1]);
    JS_ToInt32(ctx, &src_y, argv[2]);
    JS_ToInt32(ctx, &dst_x, argv[4]);
    JS_ToInt32(ctx, &dst_y, argv[5]);
    athena_image_copy_vram_block(src, src_x, src_y, dst, dst_x, dst_y);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_image_funcs[] = {
    JS_CFUNC_DEF("copyVRAMBlock", 6, js_image_copy_block),
};

static int js_image_init(JSContext *ctx, JSModuleDef *m) {
    JSValue image_proto, image_class;
    JS_NewClassID(&js_image_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_image_class_id, &js_image_class);
    image_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, image_proto, js_image_proto_funcs, countof(js_image_proto_funcs));
    image_class = JS_NewCFunction2(ctx, js_image_ctor, "Image", 2, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, image_class, image_proto);
    JS_SetClassProto(ctx, js_image_class_id, image_proto);
    JS_SetPropertyFunctionList(ctx, image_class, js_image_funcs, countof(js_image_funcs));
    JS_SetModuleExport(ctx, m, "Image", image_class);
    return 0;
}

JSModuleDef *athena_image_init(JSContext *ctx) {
    JSModuleDef *m = JS_NewCModule(ctx, "Image", js_image_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "Image");
    return m;
}