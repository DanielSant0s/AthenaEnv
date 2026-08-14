#include <ath_env.h>
#include <stdio.h>

#include <athena/vec4.h>

static JSClassID js_vector4_class_id;

JSClassID get_vector4_class_id() {
	return js_vector4_class_id;
}

static void js_vector4_finalizer(JSRuntime *rt, JSValue val) {
    AthenaVec4 *s = JS_GetOpaque(val, js_vector4_class_id);
    athena_vec4_destroy(s);
}

static JSValue js_vector4_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    float x, y, z, w;
    AthenaVec4 *s;
    JSValue obj = JS_UNDEFINED;

    if (JS_ToFloat32(ctx, &x, argv[0]) || JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2]) || JS_ToFloat32(ctx, &w, argv[3]))
        return JS_EXCEPTION;

    s = athena_vec4_create(x, y, z, w);
    if (!s)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_vector4_class_id);
    if (JS_IsException(obj)) {
        athena_vec4_destroy(s);
        return obj;
    }
    JS_SetOpaque(obj, s);
    return obj;
}

static JSValue js_vector4_get_xyz(JSContext *ctx, JSValueConst this_val, int magic) {
    AthenaVec4 *s = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    if (!s)
        return JS_EXCEPTION;
    switch (magic) {
        case 0: return JS_NewFloat32(ctx, athena_vec4_get_x(s));
        case 1: return JS_NewFloat32(ctx, athena_vec4_get_y(s));
        case 2: return JS_NewFloat32(ctx, athena_vec4_get_z(s));
        default: return JS_NewFloat32(ctx, athena_vec4_get_w(s));
    }
}

static JSValue js_vector4_set_xyz(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    AthenaVec4 *s = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    float v;
    if (!s || JS_ToFloat32(ctx, &v, val))
        return JS_EXCEPTION;
    switch (magic) {
        case 0: athena_vec4_set_x(s, v); break;
        case 1: athena_vec4_set_y(s, v); break;
        case 2: athena_vec4_set_z(s, v); break;
        default: athena_vec4_set_w(s, v); break;
    }
    return JS_UNDEFINED;
}

static JSValue js_vector4_norm(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    AthenaVec4 *v = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    if (!v) return JS_EXCEPTION;
    return JS_NewFloat32(ctx, athena_vec4_norm(v));
}

static JSValue js_vector4_dotproduct(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    AthenaVec4 *v1 = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    AthenaVec4 *v2 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    if (!v1 || !v2) return JS_EXCEPTION;
    return JS_NewFloat32(ctx, athena_vec4_dot(v1, v2));
}

static JSValue js_vector4_dist(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    AthenaVec4 *v1 = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    AthenaVec4 *v2 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    if (!v1 || !v2) return JS_EXCEPTION;
    return JS_NewFloat32(ctx, athena_vec4_distance(v1, v2));
}

static JSValue js_vector4_distsqr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    AthenaVec4 *v1 = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    AthenaVec4 *v2 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    if (!v1 || !v2) return JS_EXCEPTION;
    return JS_NewFloat32(ctx, athena_vec4_distance2(v1, v2));
}

static JSValue js_vector4_tostring(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char str[64];
    AthenaVec4 *v = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    if (!v) return JS_EXCEPTION;
    athena_vec4_tostring(v, str, sizeof(str));
    return JS_NewString(ctx, str);
}

static JSValue js_vector4_wrap_binary(JSContext *ctx, AthenaVec4 *(*op)(const AthenaVec4 *, const AthenaVec4 *),
                                      JSValueConst new_target, int argc, JSValueConst *argv) {
    AthenaVec4 *v1 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    AthenaVec4 *v2 = JS_GetOpaque2(ctx, argv[1], js_vector4_class_id);
    AthenaVec4 *result;
    JSValue obj;
    if (!v1 || !v2) return JS_EXCEPTION;
    result = op(v1, v2);
    if (!result) return JS_EXCEPTION;
    obj = JS_NewObjectClass(ctx, js_vector4_class_id);
    if (JS_IsException(obj)) { athena_vec4_destroy(result); return obj; }
    JS_SetOpaque(obj, result);
    return obj;
}

static JSValue js_vector4_add(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    return js_vector4_wrap_binary(ctx, athena_vec4_add, new_target, argc, argv);
}
static JSValue js_vector4_sub(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    return js_vector4_wrap_binary(ctx, athena_vec4_sub, new_target, argc, argv);
}
static JSValue js_vector4_mul(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    return js_vector4_wrap_binary(ctx, athena_vec4_mul, new_target, argc, argv);
}
static JSValue js_vector4_div(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    return js_vector4_wrap_binary(ctx, athena_vec4_div, new_target, argc, argv);
}

static JSValue js_vector4_cross(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    AthenaVec4 *v1 = JS_GetOpaque2(ctx, this_val, js_vector4_class_id);
    AthenaVec4 *v2 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    AthenaVec4 *result;
    JSValue obj;
    if (!v1 || !v2) return JS_EXCEPTION;
    result = athena_vec4_cross(v1, v2);
    if (!result) return JS_EXCEPTION;
    obj = JS_NewObjectClass(ctx, js_vector4_class_id);
    if (JS_IsException(obj)) { athena_vec4_destroy(result); return obj; }
    JS_SetOpaque(obj, result);
    return obj;
}

static JSValue js_vector4_eq(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    AthenaVec4 *m1 = JS_GetOpaque2(ctx, argv[0], js_vector4_class_id);
    AthenaVec4 *m2 = JS_GetOpaque2(ctx, argv[1], js_vector4_class_id);
    if (!m1 || !m2) return JS_EXCEPTION;
    return JS_NewBool(ctx, athena_vec4_equals(m1, m2));
}

static JSClassDef js_vector4_class = {
    "Vector4",
    .finalizer = js_vector4_finalizer,
};

/* Method forms of the arithmetic helpers, matching Vector2/Vector3 (see
 * ath_vector.c) and Matrix4 (see ath_matrix.c). These used to be reachable
 * only as overloaded operators (v1 + v2), which required QuickJS's operator
 * overloading support living behind CONFIG_BIGNUM. As plain methods they
 * need none of that. The underlying binary helpers take both operands in
 * argv, so the shims just put `this` in front. */
#define VEC4_BINARY_METHOD(name, cfunc)                                       \
    static JSValue js_vector4_##name##_method(JSContext *ctx, JSValueConst this_val, \
                                              int argc, JSValueConst *argv) {  \
        JSValueConst a[2];                                                    \
        a[0] = this_val;                                                      \
        a[1] = (argc > 0) ? argv[0] : JS_UNDEFINED;                           \
        return cfunc(ctx, this_val, 2, a);                                    \
    }
VEC4_BINARY_METHOD(add, js_vector4_add)
VEC4_BINARY_METHOD(sub, js_vector4_sub)
VEC4_BINARY_METHOD(mul, js_vector4_mul)
VEC4_BINARY_METHOD(div, js_vector4_div)
VEC4_BINARY_METHOD(equals, js_vector4_eq)
#undef VEC4_BINARY_METHOD

static const JSCFunctionListEntry js_vector4_proto_funcs[] = {
    JS_CFUNC_DEF("norm", 0, js_vector4_norm),
    JS_CFUNC_DEF("dot", 1, js_vector4_dotproduct),
    JS_CFUNC_DEF("cross", 1, js_vector4_cross),
    JS_CFUNC_DEF("distance", 1, js_vector4_dist),
    JS_CFUNC_DEF("distance2", 1, js_vector4_distsqr),
    JS_CFUNC_DEF("toString", 0, js_vector4_tostring),
    JS_CFUNC_DEF("add", 1, js_vector4_add_method),
    JS_CFUNC_DEF("sub", 1, js_vector4_sub_method),
    JS_CFUNC_DEF("mul", 1, js_vector4_mul_method),
    JS_CFUNC_DEF("div", 1, js_vector4_div_method),
    JS_CFUNC_DEF("equals", 1, js_vector4_equals_method),
    JS_CGETSET_MAGIC_DEF("x", js_vector4_get_xyz, js_vector4_set_xyz, 0),
    JS_CGETSET_MAGIC_DEF("y", js_vector4_get_xyz, js_vector4_set_xyz, 1),
    JS_CGETSET_MAGIC_DEF("z", js_vector4_get_xyz, js_vector4_set_xyz, 2),
    JS_CGETSET_MAGIC_DEF("w", js_vector4_get_xyz, js_vector4_set_xyz, 3)
};

static int js_vector4_init(JSContext *ctx, JSModuleDef *m) {
    JSValue vector4_proto, vector4_class;
    JS_NewClassID(&js_vector4_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_vector4_class_id, &js_vector4_class);
    vector4_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, vector4_proto, js_vector4_proto_funcs, countof(js_vector4_proto_funcs));
    vector4_class = JS_NewCFunction2(ctx, js_vector4_ctor, "Vector4", 4, JS_CFUNC_constructor_or_func, 0);
    JS_SetConstructor(ctx, vector4_class, vector4_proto);
    JS_SetClassProto(ctx, js_vector4_class_id, vector4_proto);
    return JS_SetModuleExport(ctx, m, "Vector4", vector4_class);
}

JSModuleDef *athena_vector4_init(JSContext *ctx) {
    JSModuleDef *m = JS_NewCModule(ctx, "Vector4", js_vector4_init);
    if (!m) return NULL;
    JS_AddModuleExport(ctx, m, "Vector4");
    return m;
}
