#include <ath_env.h>
#include <stdio.h>
#include <stdlib.h>

#include <athena/matrix.h>

static JSClassID js_matrix4_class_id;

JSClassID get_matrix4_class_id(){
	return js_matrix4_class_id;
}

/* Views (bone/transform matrices of render objects) share the Matrix4
 * prototype but point into engine memory: a class of their own whose
 * finalizer frees nothing. */
static JSClassID js_matrix4_view_class_id;

static void js_matrix4_finalizer(JSRuntime *rt, JSValue val) {
    AthenaMatrix4 *s = JS_GetOpaque(val, js_matrix4_class_id);
    athena_matrix4_destroy(s);
}

static AthenaMatrix4 *matrix4_opaque(JSContext *ctx, JSValueConst val) {
    AthenaMatrix4 *m = JS_GetOpaque(val, js_matrix4_class_id);
    return m ? m : JS_GetOpaque2(ctx, val, js_matrix4_view_class_id);
}

JSValue athena_js_matrix4_view(JSContext *ctx, void *data, JSValueConst owner) {
    JSValue obj = JS_NewObjectClass(ctx, js_matrix4_view_class_id);
    if (JS_IsException(obj))
        return obj;
    JS_SetOpaque(obj, data);
    /* keeps the memory's owner alive as long as the view */
    if (JS_IsObject(owner))
        JS_DefinePropertyValueStr(ctx, obj, "__owner", JS_DupValue(ctx, owner), 0);
    return obj;
}

static JSValue js_matrix4_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    AthenaMatrix4 *s;
    JSValue obj = JS_UNDEFINED;

    if (argc > 0) {
        float values[ATHENA_MATRIX4_LENGTH];
        for (int i = 0; i < ATHENA_MATRIX4_LENGTH; i++)
            JS_ToFloat32(ctx, &values[i], argv[i]);
        s = athena_matrix4_create_from(values);
    } else {
        s = athena_matrix4_create();
    }

    if (!s)
        return JS_EXCEPTION;

    obj = JS_NewObjectClass(ctx, js_matrix4_class_id);
    if (JS_IsException(obj)) {
        athena_matrix4_destroy(s);
        return obj;
    }
    JS_SetOpaque(obj, s);
    return obj;
}

static JSValue js_matrix4_tostring(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char str[256];
    AthenaMatrix4 *m = matrix4_opaque(ctx, this_val);
    if (!m) return JS_EXCEPTION;
    athena_matrix4_tostring(m, str, sizeof(str));
    return JS_NewString(ctx, str);
}

static JSValue js_matrix4_toarray(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    AthenaMatrix4 *m = matrix4_opaque(ctx, this_val);
    float values[ATHENA_MATRIX4_LENGTH];
    if (!m) return JS_EXCEPTION;
    JSValue arr = JS_NewArray(ctx);
    athena_matrix4_to_array(m, values);
    for (int i = 0; i < ATHENA_MATRIX4_LENGTH; i++)
        JS_DefinePropertyValueUint32(ctx, arr, i, JS_NewFloat32(ctx, values[i]), JS_PROP_C_W_E);
    return arr;
}

static JSValue js_matrix4_fromarray(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    AthenaMatrix4 *m = matrix4_opaque(ctx, this_val);
    float values[ATHENA_MATRIX4_LENGTH];
    if (!m) return JS_EXCEPTION;
    for (int i = 0; i < ATHENA_MATRIX4_LENGTH; i++) {
        JSValue val = JS_GetPropertyUint32(ctx, argv[0], i);
        JS_ToFloat32(ctx, &values[i], val);
        JS_FreeValue(ctx, val);
    }
    athena_matrix4_from_array(m, values);
    return JS_DupValue(ctx, this_val); /* returned values are owned by the caller */
}

static JSValue js_matrix4_mul(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    AthenaMatrix4 *v1 = matrix4_opaque(ctx, argv[0]);
    AthenaMatrix4 *v2 = matrix4_opaque(ctx, argv[1]);
    AthenaMatrix4 *result;
    JSValue obj;
    if (!v1 || !v2) return JS_EXCEPTION;
    result = athena_matrix4_multiply(v1, v2);
    if (!result) return JS_EXCEPTION;
    obj = JS_NewObjectClass(ctx, js_matrix4_class_id);
    if (JS_IsException(obj)) { athena_matrix4_destroy(result); return obj; }
    JS_SetOpaque(obj, result);
    return obj;
}

static JSValue js_matrix4_eq(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    AthenaMatrix4 *m1 = matrix4_opaque(ctx, argv[0]);
    AthenaMatrix4 *m2 = matrix4_opaque(ctx, argv[1]);
    if (!m1 || !m2) return JS_EXCEPTION;
    return JS_NewBool(ctx, athena_matrix4_equals(m1, m2));
}

static JSValue js_matrix4_copy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    AthenaMatrix4 *m1 = matrix4_opaque(ctx, this_val);
    AthenaMatrix4 *m2 = matrix4_opaque(ctx, argv[0]);
    if (!m1 || !m2) return JS_EXCEPTION;
    athena_matrix4_copy(m1, m2);
    return JS_DupValue(ctx, this_val); /* returned values are owned by the caller */
}

static JSValue js_matrix4_identity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    AthenaMatrix4 *m1 = matrix4_opaque(ctx, this_val);
    if (!m1) return JS_EXCEPTION;
    athena_matrix4_identity(m1);
    return JS_DupValue(ctx, this_val); /* returned values are owned by the caller */
}

static JSValue js_matrix4_transpose(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    AthenaMatrix4 *m1 = matrix4_opaque(ctx, this_val);
    if (!m1) return JS_EXCEPTION;
    athena_matrix4_transpose(m1);
    return JS_DupValue(ctx, this_val); /* returned values are owned by the caller */
}

static JSValue js_matrix4_invert(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    AthenaMatrix4 *m1 = matrix4_opaque(ctx, this_val);
    if (!m1) return JS_EXCEPTION;
    athena_matrix4_invert(m1);
    return JS_DupValue(ctx, this_val); /* returned values are owned by the caller */
}

static JSValue js_matrix4_clone(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    AthenaMatrix4 *m1 = matrix4_opaque(ctx, this_val);
    AthenaMatrix4 *result;
    JSValue obj;
    if (!m1) return JS_EXCEPTION;
    result = athena_matrix4_clone(m1);
    if (!result) return JS_EXCEPTION;
    obj = JS_NewObjectClass(ctx, js_matrix4_class_id);
    if (JS_IsException(obj)) { athena_matrix4_destroy(result); return obj; }
    JS_SetOpaque(obj, result);
    return obj;
}

static JSValue js_matrix4_get_prop(JSContext *ctx, JSValueConst obj, JSAtom prop, JSValueConst receiver) {
    AthenaMatrix4 *m = matrix4_opaque(ctx, obj);
    const char *prop_name = JS_AtomToCString(ctx, prop);
    char *endptr;
    long index;
    if (!prop_name) return JS_EXCEPTION;
    index = strtol(prop_name, &endptr, 10);
    JS_FreeCString(ctx, prop_name);
    if (*endptr == '\0' && index >= 0 && index < ATHENA_MATRIX4_LENGTH)
        return JS_NewFloat32(ctx, athena_matrix4_get(m, (int)index));
    return JS_UNDEFINED;
}

static int js_matrix4_set_prop(JSContext *ctx, JSValueConst obj, JSAtom prop, JSValueConst val, JSValueConst receiver, int flags) {
    AthenaMatrix4 *m = matrix4_opaque(ctx, obj);
    const char *prop_name = JS_AtomToCString(ctx, prop);
    char *endptr;
    long index;
    float value;
    if (!prop_name) return -1;
    index = strtol(prop_name, &endptr, 10);
    JS_FreeCString(ctx, prop_name);
    if (*endptr == '\0' && index >= 0 && index < ATHENA_MATRIX4_LENGTH) {
        JS_ToFloat32(ctx, &value, val);
        athena_matrix4_set(m, (int)index, value);
        return 1;
    }
    return 2;
}

static JSClassExoticMethods js_matrix4_exotic = {
    .get_property = js_matrix4_get_prop,
    .set_property = js_matrix4_set_prop
};

static JSClassDef js_matrix4_class = {
    "Matrix4",
    .finalizer = js_matrix4_finalizer,
    .exotic = &js_matrix4_exotic
};

static JSClassDef js_matrix4_view_class = {
    "Matrix4",
    .exotic = &js_matrix4_exotic
};

/* Method forms of the overloaded operators. `*` and `==` used to be registered
 * through Operators.create, which needs QuickJS's operator overloading - only
 * available under CONFIG_BIGNUM, which also drags in BigInt/BigFloat/
 * BigDecimal and libbf. As plain methods they need none of that. The binary
 * helpers read both operands from argv, so the shims prepend `this`. */
static JSValue js_matrix4_mul_method(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv) {
    JSValueConst a[2];
    a[0] = this_val;
    a[1] = (argc > 0) ? argv[0] : JS_UNDEFINED;
    return js_matrix4_mul(ctx, this_val, 2, a);
}

static JSValue js_matrix4_equals_method(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv) {
    JSValueConst a[2];
    a[0] = this_val;
    a[1] = (argc > 0) ? argv[0] : JS_UNDEFINED;
    return js_matrix4_eq(ctx, this_val, 2, a);
}

static const JSCFunctionListEntry js_matrix4_proto_funcs[] = {
    JS_CFUNC_DEF("toString", 0, js_matrix4_tostring),
    JS_CFUNC_DEF("toArray", 0, js_matrix4_toarray),
    JS_CFUNC_DEF("fromArray", 1, js_matrix4_fromarray),
    JS_CFUNC_DEF("clone", 0, js_matrix4_clone),
    JS_CFUNC_DEF("copy", 1, js_matrix4_copy),
    JS_CFUNC_DEF("transpose", 0, js_matrix4_transpose),
    JS_CFUNC_DEF("invert", 0, js_matrix4_invert),
    JS_CFUNC_DEF("identity", 0, js_matrix4_identity),
    JS_PROP_INT32_DEF("length", ATHENA_MATRIX4_LENGTH, JS_PROP_CONFIGURABLE),
    JS_CFUNC_DEF("mul", 1, js_matrix4_mul_method),
    JS_CFUNC_DEF("equals", 1, js_matrix4_equals_method),
};

static int js_matrix4_init(JSContext *ctx, JSModuleDef *m) {
    JSValue matrix4_proto, matrix4_class;
    JS_NewClassID(&js_matrix4_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_matrix4_class_id, &js_matrix4_class);
    matrix4_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, matrix4_proto, js_matrix4_proto_funcs, countof(js_matrix4_proto_funcs));
    matrix4_class = JS_NewCFunction2(ctx, js_matrix4_ctor, "Matrix4", 16, JS_CFUNC_constructor_or_func, 0);
    JS_SetConstructor(ctx, matrix4_class, matrix4_proto);
    JS_SetClassProto(ctx, js_matrix4_class_id, matrix4_proto);
    JS_NewClassID(&js_matrix4_view_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_matrix4_view_class_id, &js_matrix4_view_class);
    JS_SetClassProto(ctx, js_matrix4_view_class_id, JS_DupValue(ctx, matrix4_proto));
    return JS_SetModuleExport(ctx, m, "Matrix4", matrix4_class);
}

JSModuleDef *athena_matrix_init(JSContext *ctx) {
    JSModuleDef *m = JS_NewCModule(ctx, "Matrix4", js_matrix4_init);
    if (!m) return NULL;
    JS_AddModuleExport(ctx, m, "Matrix4");
    return m;
}
