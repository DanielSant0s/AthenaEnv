#include "ath_env.h"
#include <math.h>
#include <stdio.h>

typedef struct {
    float x;
    float y;
} Vector2;

static JSClassID js_vector2_class_id;

static void js_vector2_finalizer(JSRuntime *rt, JSValue val)
{
    Vector2 *s = JS_GetOpaque(val, js_vector2_class_id);
    /* Note: 's' can be NULL in case JS_SetOpaque() was not called */
    js_free_rt(rt, s);
}

static JSValue js_vector2_ctor(JSContext *ctx,
                             JSValueConst new_target,
                             int argc, JSValueConst *argv)
{
    Vector2 *s;
    JSValue obj = JS_UNDEFINED;
    
    obj = JS_NewObjectClass(ctx, js_vector2_class_id);
    if (JS_IsException(obj))
        goto fail;


    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;
    if (JS_ToFloat32(ctx, &s->x, argv[0]))
        goto fail;
    if (JS_ToFloat32(ctx, &s->y, argv[1]))
        goto fail;
    /* using new_target to get the prototype is necessary when the
       class is extended. */

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_vector2_get_xy(JSContext *ctx, JSValueConst this_val, int magic)
{
    Vector2 *s = JS_GetOpaque2(ctx, this_val, js_vector2_class_id);
    if (!s)
        return JS_EXCEPTION;
    if (magic == 0)
        return JS_NewFloat32(ctx, s->x);
    else
        return JS_NewFloat32(ctx, s->y);
}

static JSValue js_vector2_set_xy(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    Vector2 *s = JS_GetOpaque2(ctx, this_val, js_vector2_class_id);
    float v;
    if (!s)
        return JS_EXCEPTION;
    if (JS_ToFloat32(ctx, &v, val))
        return JS_EXCEPTION;
    if (magic == 0)
        s->x = v;
    else
        s->y = v;
    return JS_UNDEFINED;
}

static JSValue js_vector2_norm(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    Vector2 *s = JS_GetOpaque2(ctx, this_val, js_vector2_class_id);
    if (!s)
        return JS_EXCEPTION;
    return JS_NewFloat32(ctx, sqrtf(s->x * s->x + s->y * s->y));
}

static JSValue js_vector2_dotproduct(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    Vector2 *v1 = JS_GetOpaque2(ctx, this_val, js_vector2_class_id);
    Vector2 *v2 = JS_GetOpaque2(ctx, argv[0], js_vector2_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;
    return JS_NewFloat32(ctx, (v1->x * v2->x + v1->y * v2->y));
}

static JSValue js_vector2_crossproduct(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    Vector2 *v1 = JS_GetOpaque2(ctx, this_val, js_vector2_class_id);
    Vector2 *v2 = JS_GetOpaque2(ctx, argv[0], js_vector2_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;
    return JS_NewFloat32(ctx, (v1->x * v2->y - v1->y * v2->x));
}

static JSValue js_vector2_distance(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    Vector2 *v1 = JS_GetOpaque2(ctx, this_val, js_vector2_class_id);
    Vector2 *v2 = JS_GetOpaque2(ctx, argv[0], js_vector2_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;
    return JS_NewFloat32(ctx, sqrtf((v1->x - v2->x) * (v1->x - v2->x) + (v1->y - v2->y) * (v1->y - v2->y)));
}

static JSValue js_vector2_distancesqr(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    Vector2 *v1 = JS_GetOpaque2(ctx, this_val, js_vector2_class_id);
    Vector2 *v2 = JS_GetOpaque2(ctx, argv[0], js_vector2_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;
    return JS_NewFloat32(ctx, ((v1->x - v2->x) * (v1->x - v2->x) + (v1->y - v2->y) * (v1->y - v2->y)));
}

static JSValue js_vector2_tostring(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    char str[32];
    Vector2 *v = JS_GetOpaque2(ctx, this_val, js_vector2_class_id);

    if (!v)
        return JS_EXCEPTION;

    sprintf(str, "{x:%g, y:%g}", v->x, v->y);
    return JS_NewString(ctx, str);
}

static JSValue js_vector2_add(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    Vector2 *s;
    JSValue obj = JS_UNDEFINED;

    Vector2 *v1 = JS_GetOpaque2(ctx, argv[0], js_vector2_class_id);
    Vector2 *v2 = JS_GetOpaque2(ctx, argv[1], js_vector2_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector2_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;
    
    s->x = v1->x + v2->x;
    s->y = v1->y + v2->y;

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_vector2_sub(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    Vector2 *s;
    JSValue obj = JS_UNDEFINED;

    Vector2 *v1 = JS_GetOpaque2(ctx, argv[0], js_vector2_class_id);
    Vector2 *v2 = JS_GetOpaque2(ctx, argv[1], js_vector2_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector2_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;
    
    s->x = v1->x - v2->x;
    s->y = v1->y - v2->y;

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}


static JSValue js_vector2_mul(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    Vector2 *s;
    JSValue obj = JS_UNDEFINED;

    Vector2 *v1 = JS_GetOpaque2(ctx, argv[0], js_vector2_class_id);
    Vector2 *v2 = JS_GetOpaque2(ctx, argv[1], js_vector2_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector2_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;
    
    s->x = v1->x * v2->x;
    s->y = v1->y * v2->y;

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}


static JSValue js_vector2_div(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    Vector2 *s;
    JSValue obj = JS_UNDEFINED;

    Vector2 *v1 = JS_GetOpaque2(ctx, argv[0], js_vector2_class_id);
    Vector2 *v2 = JS_GetOpaque2(ctx, argv[1], js_vector2_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector2_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;
    
    s->x = v1->x / v2->x;
    s->y = v1->y / v2->y;

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}


static const JSCFunctionListEntry js_vector2_funcs[] = {
    JS_CFUNC_DEF("new", 2, js_vector2_ctor),
    JS_CFUNC_DEF("add", 2, js_vector2_add),
    JS_CFUNC_DEF("sub", 2, js_vector2_sub),
    JS_CFUNC_DEF("mul", 2, js_vector2_mul),
    JS_CFUNC_DEF("div", 2, js_vector2_div),
};

static JSClassDef js_vector2_class = {
    "Vector2",
    .finalizer = js_vector2_finalizer,
}; 

static const JSCFunctionListEntry js_vector2_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("x", js_vector2_get_xy, js_vector2_set_xy, 0),
    JS_CGETSET_MAGIC_DEF("y", js_vector2_get_xy, js_vector2_set_xy, 1),
    JS_CFUNC_DEF("norm", 0, js_vector2_norm),
    JS_CFUNC_DEF("dot", 1, js_vector2_dotproduct),
    JS_CFUNC_DEF("cross", 1, js_vector2_crossproduct),
    JS_CFUNC_DEF("dist", 1, js_vector2_distance),
    JS_CFUNC_DEF("distsqr", 1, js_vector2_distancesqr),

    JS_CFUNC_DEF("toString", 0, js_vector2_tostring),
};

static int js_vector2_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue vector2_proto, vector2_class;
    
    /* create the Vector2 class */
    JS_NewClassID(&js_vector2_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_vector2_class_id, &js_vector2_class);

    vector2_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, vector2_proto, js_vector2_proto_funcs, countof(js_vector2_proto_funcs));
    
    //vector2_class = JS_NewCFunction2(ctx, js_vector2_ctor, "xy", 2, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    //JS_SetConstructor(ctx, vector2_class, vector2_proto);
    JS_SetClassProto(ctx, js_vector2_class_id, vector2_proto);
                      
    //JS_SetModuleExport(ctx, m, "Vector2", vector2_class);
    return JS_SetModuleExportList(ctx, m, js_vector2_funcs, countof(js_vector2_funcs));
}

JSModuleDef *athena_vector_init(JSContext *ctx)
{
    return athena_push_module(ctx, js_vector2_init, js_vector2_funcs, countof(js_vector2_funcs), "Vector2");
}
