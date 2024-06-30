#include "ath_env.h"
#include <math.h>
#include <stdio.h>

#include "include/render.h"

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

typedef struct {
    float x;
    float y;
    float z;
} Vector3;

static JSClassID js_vector3_class_id;

static void js_vector3_finalizer(JSRuntime *rt, JSValue val)
{
    Vector3 *s = JS_GetOpaque(val, js_vector3_class_id);
    /* Note: 's' can be NULL in case JS_SetOpaque() was not called */
    js_free_rt(rt, s);
}

static JSValue js_vector3_ctor(JSContext *ctx,
                             JSValueConst new_target,
                             int argc, JSValueConst *argv)
{
    Vector3 *s;
    JSValue obj = JS_UNDEFINED;
    
    obj = JS_NewObjectClass(ctx, js_vector3_class_id);
    if (JS_IsException(obj))
        goto fail;


    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;
    if (JS_ToFloat32(ctx, &s->x, argv[0]))
        goto fail;
    if (JS_ToFloat32(ctx, &s->y, argv[1]))
        goto fail;
    if (JS_ToFloat32(ctx, &s->z, argv[2]))
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


static JSValue js_vector3_get_xyz(JSContext *ctx, JSValueConst this_val, int magic)
{
    Vector3 *s = JS_GetOpaque2(ctx, this_val, js_vector3_class_id);
    if (!s)
        return JS_EXCEPTION;
    if (magic == 0) {
        return JS_NewFloat32(ctx, s->x);
    } else if (magic == 1) {
        return JS_NewFloat32(ctx, s->y);
    } else {
        return JS_NewFloat32(ctx, s->z);
    }
}

static JSValue js_vector3_set_xyz(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    Vector3 *s = JS_GetOpaque2(ctx, this_val, js_vector2_class_id);
    float v;
    if (!s)
        return JS_EXCEPTION;
    if (JS_ToFloat32(ctx, &v, val))
        return JS_EXCEPTION;
    if (magic == 0) {
        s->x = v;
    } else if (magic == 1) {
        s->y = v;
    } else {
        s->z = v;
    }
    return JS_UNDEFINED;
}

static JSValue js_vector3_norm(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    Vector3 *v = JS_GetOpaque2(ctx, this_val, js_vector3_class_id);
    if (!v)
        return JS_EXCEPTION;
    return JS_NewFloat32(ctx, sqrtf(v->x * v->x + v->y * v->y + v->z * v->z));
}

static JSValue js_vector3_dotproduct(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    Vector3 *v1 = JS_GetOpaque2(ctx, this_val, js_vector3_class_id);
    Vector3 *v2 = JS_GetOpaque2(ctx, argv[0], js_vector3_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;
    return JS_NewFloat32(ctx, (v1->x * v2->x + v1->y * v2->y + v1->z * v2->z));
}

static JSValue js_vector3_dist(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    Vector3 *v1 = JS_GetOpaque2(ctx, this_val, js_vector3_class_id);
    Vector3 *v2 = JS_GetOpaque2(ctx, argv[0], js_vector3_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    float dx = v2->x - v1->x;
    float dy = v2->y - v1->y;
    float dz = v2->z - v1->z;
    return JS_NewFloat32(ctx, sqrtf(dx * dx + dy * dy + dz * dz));
}

static JSValue js_vector3_distsqr(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    Vector3 *v1 = JS_GetOpaque2(ctx, this_val, js_vector3_class_id);
    Vector3 *v2 = JS_GetOpaque2(ctx, argv[0], js_vector3_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    float dx = v2->x - v1->x;
    float dy = v2->y - v1->y;
    float dz = v2->z - v1->z;
    return JS_NewFloat32(ctx, (dx * dx + dy * dy + dz * dz));
}

static JSValue js_vector3_tostring(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    char str[32];
    Vector3 *v = JS_GetOpaque2(ctx, this_val, js_vector3_class_id);

    if (!v)
        return JS_EXCEPTION;

    sprintf(str, "{x:%g, y:%g, z:%g}", v->x, v->y, v->z);
    return JS_NewString(ctx, str);
}

static JSValue js_vector3_add(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    Vector3 *s;
    JSValue obj = JS_UNDEFINED;

    Vector3 *v1 = JS_GetOpaque2(ctx, argv[0], js_vector3_class_id);
    Vector3 *v2 = JS_GetOpaque2(ctx, argv[1], js_vector3_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector3_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    s->x = v1->x + v2->x;
    s->y = v1->y + v2->y;
    s->z = v1->z + v2->z;

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_vector3_sub(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    Vector3 *s;
    JSValue obj = JS_UNDEFINED;

    Vector3 *v1 = JS_GetOpaque2(ctx, argv[0], js_vector3_class_id);
    Vector3 *v2 = JS_GetOpaque2(ctx, argv[1], js_vector3_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector3_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    s->x = v1->x - v2->x;
    s->y = v1->y - v2->y;
    s->z = v1->z - v2->z;

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_vector3_mul(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    Vector3 *s;
    JSValue obj = JS_UNDEFINED;

    Vector3 *v1 = JS_GetOpaque2(ctx, argv[0], js_vector3_class_id);
    Vector3 *v2 = JS_GetOpaque2(ctx, argv[1], js_vector3_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector3_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    s->x = v1->x * v2->x;
    s->y = v1->y * v2->y;
    s->z = v1->z * v2->z;

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_vector3_div(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    Vector3 *s;
    JSValue obj = JS_UNDEFINED;

    Vector3 *v1 = JS_GetOpaque2(ctx, argv[0], js_vector3_class_id);
    Vector3 *v2 = JS_GetOpaque2(ctx, argv[1], js_vector3_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector3_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    s->x = v1->x / v2->x;
    s->y = v1->y / v2->y;
    s->z = v1->z / v2->z;

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_vector3_cross(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    Vector3 *s;
    JSValue obj = JS_UNDEFINED;

    Vector3 *v1 = JS_GetOpaque2(ctx, argv[0], js_vector3_class_id);
    Vector3 *v2 = JS_GetOpaque2(ctx, argv[1], js_vector3_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector3_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    s->x = v1->y * v2->z - v1->z * v2->y;
    s->y = v1->z * v2->x - v1->x * v2->z;
    s->z = v1->x * v2->y - v1->y * v2->x;

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static const JSCFunctionListEntry js_vector3_funcs[] = {
    JS_CFUNC_DEF("new", 3, js_vector3_ctor),
    JS_CFUNC_DEF("add", 2, js_vector3_add),
    JS_CFUNC_DEF("sub", 2, js_vector3_sub),
    JS_CFUNC_DEF("mul", 2, js_vector3_mul),
    JS_CFUNC_DEF("div", 2, js_vector3_div),
    JS_CFUNC_DEF("cross", 2, js_vector3_cross),
};

static JSClassDef js_vector3_class = {
    "Vector3",
    .finalizer = js_vector3_finalizer,
}; 

static const JSCFunctionListEntry js_vector3_proto_funcs[] = {
    JS_CFUNC_DEF("norm", 0, js_vector3_norm),
    JS_CFUNC_DEF("dot", 1, js_vector3_dotproduct),
    JS_CFUNC_DEF("dist", 1, js_vector3_dist),
    JS_CFUNC_DEF("distsqr", 1, js_vector3_distsqr),
    JS_CFUNC_DEF("toString", 0, js_vector3_tostring),
    JS_CGETSET_MAGIC_DEF("x", js_vector3_get_xyz, js_vector3_set_xyz, 0),
    JS_CGETSET_MAGIC_DEF("y", js_vector3_get_xyz, js_vector3_set_xyz, 1),
    JS_CGETSET_MAGIC_DEF("z", js_vector3_get_xyz, js_vector3_set_xyz, 2)
};

static int js_vector3_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue vector3_proto, vector3_class;
    
    /* create the Vector3 class */
    JS_NewClassID(&js_vector3_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_vector3_class_id, &js_vector3_class);

    vector3_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, vector3_proto, js_vector3_proto_funcs, countof(js_vector3_proto_funcs));
    
    //vector2_class = JS_NewCFunction2(ctx, js_vector3_ctor, "xy", 2, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    //JS_SetConstructor(ctx, vector3_class, vector2_proto);
    JS_SetClassProto(ctx, js_vector3_class_id, vector3_proto);
                      
    //JS_SetModuleExport(ctx, m, "Vector3", vector3_class);
    return JS_SetModuleExportList(ctx, m, js_vector3_funcs, countof(js_vector3_funcs));
}


JSModuleDef *athena_vector_init(JSContext *ctx)
{
    athena_push_module(ctx, js_vector3_init, js_vector3_funcs, countof(js_vector3_funcs), "Vector3");
    return athena_push_module(ctx, js_vector2_init, js_vector2_funcs, countof(js_vector2_funcs), "Vector2");
}
