#include <ath_env.h>
#include <math.h>
#include <stdio.h>

#include <render.h>
#include <vector.h>

static JSClassID js_vector4_class_id;

JSClassID get_vector4_class_id() {
	return js_vector4_class_id;
}

static void js_vector4_finalizer(JSRuntime *rt, JSValue val)
{
    VUVECTOR *s = JS_GetOpaque(val, js_vector4_class_id);
    /* Note: 's' can be NULL in case JS_SetOpaque() was not called */
    js_free_rt(rt, s);
}

static JSValue js_vector4_ctor(JSContext *ctx,
                             JSValueConst new_target,
                             int argc, JSValueConst *argv)
{
    VUVECTOR *s;
    JSValue obj = JS_UNDEFINED;
    
    obj = JS_NewObjectClass(ctx, js_vector4_class_id);
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
    if (JS_ToFloat32(ctx, &s->w, argv[3]))
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


static JSValue js_vector4_get_xyz(JSContext *ctx, JSValueConst this_val, int magic)
{
    VUVECTOR *s = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    if (!s)
        return JS_EXCEPTION;

    return JS_NewFloat32(ctx, s->f[magic]);
}

static JSValue js_vector4_set_xyz(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    VUVECTOR *s = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    float v;
    if (!s)
        return JS_EXCEPTION;
    if (JS_ToFloat32(ctx, &v, val))
        return JS_EXCEPTION;
    
    s->f[magic] = v;

    return JS_UNDEFINED;
}

static JSValue js_vector4_norm(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    VUVECTOR *v = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    if (!v)
        return JS_EXCEPTION;
    return JS_NewFloat32(ctx, vector_functions->get_length(v->f));
}

static JSValue js_vector4_dotproduct(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    VUVECTOR *v1 = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    VUVECTOR *v2 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);

    if (!v1 || !v2)
        return JS_EXCEPTION;

    return JS_NewFloat32(ctx, vector_functions->dot(v1->f, v2->f));
}

static JSValue js_vector4_dist(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    VUVECTOR *v1 = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    VUVECTOR *v2 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    float dx = v2->x - v1->x;
    float dy = v2->y - v1->y;
    float dz = v2->z - v1->z;
    float dw = v2->w - v1->w;

    return JS_NewFloat32(ctx, sqrtf(dx * dx + dy * dy + dz * dz + dw * dw));
}

static JSValue js_vector4_distsqr(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    VUVECTOR *v1 = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    VUVECTOR *v2 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    float dx = v2->x - v1->x;
    float dy = v2->y - v1->y;
    float dz = v2->z - v1->z;
    float dw = v2->w - v1->w;

    return JS_NewFloat32(ctx, (dx * dx + dy * dy + dz * dz + dw * dw));
}

static JSValue js_vector4_tostring(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    char str[32];
    VUVECTOR *v = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);

    if (!v)
        return JS_EXCEPTION;

    sprintf(str, "{x:%g, y:%g, z:%g, w:%g}", v->x, v->y, v->z, v->w);
    return JS_NewString(ctx, str);
}

static JSValue js_vector4_add(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    VUVECTOR *s;
    JSValue obj = JS_UNDEFINED;

    VUVECTOR *v1 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    VUVECTOR *v2 = JS_GetOpaque2(ctx, argv[1], js_vector4_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector4_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    vector_functions->add(s->f, v1->f, v2->f);

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_vector4_sub(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    VUVECTOR *s;
    JSValue obj = JS_UNDEFINED;

    VUVECTOR *v1 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    VUVECTOR *v2 = JS_GetOpaque2(ctx, argv[1], js_vector4_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector4_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    vector_functions->sub(s->f, v1->f, v2->f);

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_vector4_mul(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    VUVECTOR *s;
    JSValue obj = JS_UNDEFINED;

    VUVECTOR *v1 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    VUVECTOR *v2 = JS_GetOpaque2(ctx, argv[1], js_vector4_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector4_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    vector_functions->mul(s->f, v1->f, v2->f);

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_vector4_div(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
    VUVECTOR *s;
    JSValue obj = JS_UNDEFINED;

    VUVECTOR *v1 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    VUVECTOR *v2 = JS_GetOpaque2(ctx, argv[1], js_vector4_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector4_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    vector_functions->div(s->f, v1->f, v2->f);

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_vector4_cross(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    VUVECTOR *s;
    JSValue obj = JS_UNDEFINED;

    VUVECTOR *v1 = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    VUVECTOR *v2 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    if (!v1 || !v2)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector4_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    vector_functions->cross(s->f, v1->f, v2->f);

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_vector4_eq(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    VUVECTOR *m1 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    VUVECTOR *m2 = JS_GetOpaque2(ctx, argv[1], js_vector4_class_id);

    return JS_NewBool(ctx, vector_functions->equals(m1, m2));
}

static JSClassDef js_vector4_class = {
    "Vector4",
    .finalizer = js_vector4_finalizer,
}; 

static const JSCFunctionListEntry js_vector4_proto_funcs[] = {
    JS_CFUNC_DEF("norm", 0, js_vector4_norm),
    JS_CFUNC_DEF("dot", 1, js_vector4_dotproduct),
    JS_CFUNC_DEF("cross", 1, js_vector4_cross),
    JS_CFUNC_DEF("distance", 1, js_vector4_dist),
    JS_CFUNC_DEF("distance2", 1, js_vector4_distsqr),
    JS_CFUNC_DEF("toString", 0, js_vector4_tostring),
    JS_CGETSET_MAGIC_DEF("x", js_vector4_get_xyz, js_vector4_set_xyz, 0),
    JS_CGETSET_MAGIC_DEF("y", js_vector4_get_xyz, js_vector4_set_xyz, 1),
    JS_CGETSET_MAGIC_DEF("z", js_vector4_get_xyz, js_vector4_set_xyz, 2),
    JS_CGETSET_MAGIC_DEF("w", js_vector4_get_xyz, js_vector4_set_xyz, 3)
};

static void js_vector4_init_operators(JSContext *ctx, JSValue proto)
{
    JSValue operatorSet, obj, global;
    JSValue Operators, Symbol;
    JSValue symbol_operatorSet;

    global = JS_GetGlobalObject(ctx);

    Symbol = JS_GetPropertyStr(ctx, global, "Symbol");
    
    symbol_operatorSet = JS_GetPropertyStr(ctx, Symbol, "operatorSet");
    JS_FreeValue(ctx, Symbol);
    
    Operators = JS_GetPropertyStr(ctx, global, "Operators");

    JS_FreeValue(ctx, global);

    JSValue create_func = JS_GetPropertyStr(ctx, Operators, "create");

    obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "+", JS_NewCFunction(ctx, js_vector4_add, "+", 2));
    JS_SetPropertyStr(ctx, obj, "-", JS_NewCFunction(ctx, js_vector4_sub, "-", 2));
    JS_SetPropertyStr(ctx, obj, "*", JS_NewCFunction(ctx, js_vector4_mul, "*", 2));
    JS_SetPropertyStr(ctx, obj, "/", JS_NewCFunction(ctx, js_vector4_div, "/", 2));
    JS_SetPropertyStr(ctx, obj, "==", JS_NewCFunction(ctx, js_vector4_eq, "==", 2));

    JSValueConst args[1] = { obj };
    operatorSet = JS_Call(ctx, create_func, Operators, 1, args);
    
    JS_FreeValue(ctx, create_func);
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, Operators);

    JS_SetProperty(ctx, proto, JS_ValueToAtom(ctx, symbol_operatorSet), operatorSet);

    JS_FreeValue(ctx, symbol_operatorSet);
}

static int js_vector4_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue vector4_proto, vector4_class;
    
    /* create the Vector4 class */
    JS_NewClassID(&js_vector4_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_vector4_class_id, &js_vector4_class);

    vector4_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, vector4_proto, js_vector4_proto_funcs, countof(js_vector4_proto_funcs));
    
    vector4_class = JS_NewCFunction2(ctx, js_vector4_ctor, "Vector4", 4, JS_CFUNC_constructor_or_func, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, vector4_class, vector4_proto);
    JS_SetClassProto(ctx, js_vector4_class_id, vector4_proto);

    js_vector4_init_operators(ctx, vector4_proto);

    return JS_SetModuleExport(ctx, m, "Vector4", vector4_class);
}

JSModuleDef *athena_vector4_init(JSContext *ctx) {
    JSModuleDef *m;

    m = JS_NewCModule(ctx, "Vector4", js_vector4_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "Vector4");
    return m;
}
