#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <graphics.h>
#include <ath_env.h>

#include <macros.h>

static JSValue athena_point_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    float x, y;
    Color color;

    JS_ToFloat32(ctx, &x, argv[0]);
    JS_ToFloat32(ctx, &y, argv[1]);
	JS_ToUint32(ctx, &color, argv[2]);

	draw_point(x, y, color);
	return JS_UNDEFINED;
}

static JSValue athena_line_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    float x1, y1, x2, y2;
    Color color;

    JS_ToFloat32(ctx, &x1, argv[0]);
    JS_ToFloat32(ctx, &y1, argv[1]);
    JS_ToFloat32(ctx, &x2, argv[2]);
    JS_ToFloat32(ctx, &y2, argv[3]);
	JS_ToUint32(ctx, &color, argv[4]);

	draw_line(x1, y1, x2, y2, color);
	return JS_UNDEFINED;
}

static JSValue athena_triangle_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    float x1, y1, x2, y2, x3, y3;
    Color color1, color2, color3;

    JS_ToFloat32(ctx, &x1, argv[0]);
    JS_ToFloat32(ctx, &y1, argv[1]);
    JS_ToFloat32(ctx, &x2, argv[2]);
    JS_ToFloat32(ctx, &y2, argv[3]);
    JS_ToFloat32(ctx, &x3, argv[4]);
    JS_ToFloat32(ctx, &y3, argv[5]);
	JS_ToUint32(ctx, &color1, argv[6]);

    if(argc == 7){
        draw_triangle(x1, y1, x2, y2, x3, y3, color1);
    } else {
        JS_ToUint32(ctx, &color2, argv[7]);
        JS_ToUint32(ctx, &color3, argv[8]);

        draw_triangle_gouraud(x1, y1, x2, y2, x3, y3, color1, color2, color3);
    }
	
	return JS_UNDEFINED;
}

static JSValue athena_quad_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    float x1, y1, x2, y2, x3, y3, x4, y4;
    Color color1, color2, color3, color4;

    JS_ToFloat32(ctx, &x1, argv[0]);
    JS_ToFloat32(ctx, &y1, argv[1]);
    JS_ToFloat32(ctx, &x2, argv[2]);
    JS_ToFloat32(ctx, &y2, argv[3]);
    JS_ToFloat32(ctx, &x3, argv[4]);
    JS_ToFloat32(ctx, &y3, argv[5]);
    JS_ToFloat32(ctx, &x4, argv[6]);
    JS_ToFloat32(ctx, &y4, argv[7]);
	JS_ToUint32(ctx, &color1, argv[8]);

    if(argc == 9){
        draw_quad(x1, y1, x2, y2, x3, y3, x4, y4, color1);
    } else {
        JS_ToUint32(ctx, &color2, argv[9]);
	    JS_ToUint32(ctx, &color3, argv[10]);
	    JS_ToUint32(ctx, &color4, argv[11]);

        draw_quad_gouraud(x1, y1, x2, y2, x3, y3, x4, y4, color1, color2, color3, color4);
    }
	
	return JS_UNDEFINED;
}

static JSValue athena_rect_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    float x, y, w, h;
    Color color;

    JS_ToFloat32(ctx, &x, argv[0]);
    JS_ToFloat32(ctx, &y, argv[1]);
    JS_ToFloat32(ctx, &w, argv[2]);
    JS_ToFloat32(ctx, &h, argv[3]);
	JS_ToUint32(ctx, &color, argv[4]);

	draw_sprite(x, y, w, h, color);
	return JS_UNDEFINED;
}

static JSValue athena_circle_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    bool filled = true;
    float x, y, r;
    Color color;

    JS_ToFloat32(ctx, &x, argv[0]);
    JS_ToFloat32(ctx, &y, argv[1]);
    JS_ToFloat32(ctx, &r, argv[2]);
    JS_ToUint32(ctx, &color, argv[3]);
    if (argc == 5) filled = JS_ToBool(ctx, argv[4]);

	draw_circle(x, y, r, color, filled);
	return JS_UNDEFINED;
}

static JSClassID js_prim_list_class_id;


typedef enum {
    PRIM_TYPE_POINT,
    PRIM_TYPE_LINE,
    PRIM_TYPE_LINESTRIP,
    PRIM_TYPE_TRIANGLE,
    PRIM_TYPE_TRISTRIP,
    PRIM_TYPE_TRIFAN,
    PRIM_TYPE_SPRITE
} ePrimitiveTypes;

typedef struct {
    ePrimitiveTypes type;
    int shade;

    void *vert_list;
    uint32_t size;

    JSValue texture_handle;
    GSSURFACE *texture;
} JSPrimitiveList;

static JSValue athena_prim_list_ctor(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    JSPrimitiveList* list_obj;
    JSValue obj = JS_UNDEFINED, vert_ab = JS_UNDEFINED;
    JSValue proto;

    obj = JS_NewObjectClass(ctx, js_prim_list_class_id);
	if (JS_IsException(obj))
        goto fail;
    list_obj = js_mallocz(ctx, sizeof(*list_obj));

    JS_ToUint32(ctx, &list_obj->type, argv[0]);

    JS_ToUint32(ctx, &list_obj->shade, argv[1]);

    JS_ToUint32(ctx, &list_obj->size, JS_GetPropertyStr(ctx, argv[2], "length"));

    switch (list_obj->type) {
        case PRIM_TYPE_POINT:
        {
            prim_point *list = list_obj->vert_list = malloc(sizeof(prim_point)*list_obj->size);

            for (uint32_t i = 0; i < list_obj->size; i++) {
                JSValue prim_obj = JS_GetPropertyUint32(ctx, argv[2], i);
                JS_ToFloat32(ctx, &list[i].x, JS_GetPropertyStr(ctx, prim_obj, "x"));
                JS_ToFloat32(ctx, &list[i].y, JS_GetPropertyStr(ctx, prim_obj, "y"));
        
                JS_ToUint32(ctx, &list[i].rgba, JS_GetPropertyStr(ctx, prim_obj, "rgba"));
        
                JS_FreeValue(ctx, prim_obj);
            }
        }
        break;
        case PRIM_TYPE_SPRITE:
        {
            prim_tex_sprite *list = list_obj->vert_list = malloc(sizeof(prim_tex_sprite)*list_obj->size);

            for (uint32_t i = 0; i < list_obj->size; i++) {
                JSValue prim_obj = JS_GetPropertyUint32(ctx, argv[2], i);
                JS_ToFloat32(ctx, &list[i].x, JS_GetPropertyStr(ctx, prim_obj, "x"));
                JS_ToFloat32(ctx, &list[i].y, JS_GetPropertyStr(ctx, prim_obj, "y"));
                JS_ToFloat32(ctx, &list[i].w, JS_GetPropertyStr(ctx, prim_obj, "w"));
                JS_ToFloat32(ctx, &list[i].h, JS_GetPropertyStr(ctx, prim_obj, "h"));
        
                JS_ToFloat32(ctx, &list[i].u1, JS_GetPropertyStr(ctx, prim_obj, "u1"));
                JS_ToFloat32(ctx, &list[i].v1, JS_GetPropertyStr(ctx, prim_obj, "v1"));
                JS_ToFloat32(ctx, &list[i].u2, JS_GetPropertyStr(ctx, prim_obj, "u2"));
                JS_ToFloat32(ctx, &list[i].v2, JS_GetPropertyStr(ctx, prim_obj, "v2"));
        
                JS_ToUint32(ctx, &list[i].rgba, JS_GetPropertyStr(ctx, prim_obj, "rgba"));
        
                JS_FreeValue(ctx, prim_obj);
            }
        }
        break;
    }

    list_obj->texture = NULL;
    list_obj->texture_handle = JS_UNDEFINED;
    if (argc > 3) {
        list_obj->texture_handle = argv[3];
        list_obj->texture = ((JSImageData*)JS_GetOpaque2(ctx, argv[3], get_img_class_id()))->tex;
    }

    JS_SetOpaque(obj, list_obj);
    return obj;

fail:
    js_free(ctx, list_obj);
    return JS_EXCEPTION;
}

static JSValue athena_prim_list_draw(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	float x, y;

	JSPrimitiveList *list = JS_GetOpaque2(ctx, this_val, js_prim_list_class_id);

	JS_ToFloat32(ctx, &x, argv[0]);
	JS_ToFloat32(ctx, &y, argv[1]);

    switch (list->type) {
        case PRIM_TYPE_POINT:
            draw_point_list(x, y, list->vert_list, list->size);
            break;
        case PRIM_TYPE_SPRITE:
            draw_image_list(list->texture, x, y, list->vert_list, list->size);
            break;
    }

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_prim_list_proto_funcs[] = {
    JS_CFUNC_DEF("draw", 2, athena_prim_list_draw),
};

static JSClassDef js_prim_list_class = {
    "PrimitiveList",
    //.finalizer = js_prim_list_finalizer,
};

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("point", 3, athena_point_draw),
	JS_CFUNC_DEF("line", 5, athena_line_draw),
	JS_CFUNC_DEF("triangle", 9, athena_triangle_draw),
	JS_CFUNC_DEF("quad", 12, athena_quad_draw),
	JS_CFUNC_DEF("rect", 5, athena_rect_draw),
    JS_CFUNC_DEF("circle", 5, athena_circle_draw),

    JS_CFUNC_DEF("list", 4, athena_prim_list_ctor),

    ATHENA_PROP_INT32(PRIM_TYPE_POINT),
    ATHENA_PROP_INT32(PRIM_TYPE_LINE),
    ATHENA_PROP_INT32(PRIM_TYPE_LINESTRIP),
    ATHENA_PROP_INT32(PRIM_TYPE_TRIANGLE),
    ATHENA_PROP_INT32(PRIM_TYPE_TRISTRIP),
    ATHENA_PROP_INT32(PRIM_TYPE_TRIFAN),
    ATHENA_PROP_INT32(PRIM_TYPE_SPRITE),

    ATHENA_PROP_INT32(SHADE_FLAT),
    ATHENA_PROP_INT32(SHADE_GOURAUD)
};

static int module_init(JSContext *ctx, JSModuleDef *m){
    JSValue proto;

    /* the class ID is created once */
    JS_NewClassID(&js_prim_list_class_id);
    /* the class is created once per runtime */
    JS_NewClass(JS_GetRuntime(ctx), js_prim_list_class_id, &js_prim_list_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_prim_list_proto_funcs,
                               countof(js_prim_list_proto_funcs));
    JS_SetClassProto(ctx, js_prim_list_class_id, proto);

    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_shape_init(JSContext* ctx){
	return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Draw");

}
