#include <ath_env.h>
#include <math.h>
#include <stdio.h>

#include <render.h>
#include <matrix.h>

static JSClassID js_matrix4_class_id;

static void js_matrix4_finalizer(JSRuntime *rt, JSValue val)
{
    MATRIX *s = JS_GetOpaque(val, js_matrix4_class_id);
    /* Note: 's' can be NULL in case JS_SetOpaque() was not called */
    js_free_rt(rt, s);
}

static JSValue js_matrix4_ctor(JSContext *ctx,
                             JSValueConst new_target,
                             int argc, JSValueConst *argv)
{
    MATRIX *s;
    JSValue obj = JS_UNDEFINED;
    
    obj = JS_NewObjectClass(ctx, js_matrix4_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    if (argc > 0) {
        for (int i = 0; i < 16; i++) {
            JS_ToFloat32(ctx, &(*s[i]), argv[i]);
        }
    } else {
        matrix_functions->identity(s);
    }

    /* using new_target to get the prototype is necessary when the
       class is extended. */

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_matrix4_tostring(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    char str[256];
    MATRIX *m = JS_GetOpaque2(ctx, this_val, js_matrix4_class_id);

    if (!m)
        return JS_EXCEPTION;

    sprintf(str, "[%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f]", 
                (*m)[0], (*m)[1], (*m)[2], (*m)[3], (*m)[4], (*m)[5], (*m)[6], (*m)[7], (*m)[8], (*m)[9], (*m)[10], (*m)[11], (*m)[12], (*m)[13], (*m)[14], (*m)[15]);

    return JS_NewString(ctx, str);
}

static JSValue js_matrix4_toarray(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    MATRIX *m = JS_GetOpaque2(ctx, this_val, js_matrix4_class_id);

    if (!m)
        return JS_EXCEPTION;

    JSValue arr = JS_NewArray(ctx);

    for (int i = 0; i < 16; i++) {
        JS_DefinePropertyValueUint32(ctx, arr, i, JS_NewFloat32(ctx, (*m)[i]), JS_PROP_C_W_E);
    }

    return arr;
}

static JSValue js_matrix4_fromarray(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    MATRIX *m = JS_GetOpaque2(ctx, this_val, js_matrix4_class_id);

    if (!m)
        return JS_EXCEPTION;

    for (int i = 0; i < 16; i++) {
        JSValue val = JS_GetPropertyUint32(ctx, argv[0], i);
        JS_ToFloat32(ctx, &(*m)[i], val);
    }

    return this_val;
}

static JSValue js_matrix4_mul(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    MATRIX *s;
    JSValue obj = JS_UNDEFINED;

    MATRIX *v1 = JS_GetOpaque2(ctx, argv[0], js_matrix4_class_id);
    MATRIX *v2 = JS_GetOpaque2(ctx, argv[1], js_matrix4_class_id);

    obj = JS_NewObjectClass(ctx, js_matrix4_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    matrix_functions->multiply(s, v1, v2);

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_matrix4_eq(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    MATRIX *m1 = JS_GetOpaque2(ctx, argv[0], js_matrix4_class_id);
    MATRIX *m2 = JS_GetOpaque2(ctx, argv[1], js_matrix4_class_id);

    return JS_NewBool(ctx, matrix_functions->equals(m1, m2));
}

static JSValue js_matrix4_copy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    MATRIX *m1 = JS_GetOpaque2(ctx, this_val, js_matrix4_class_id);
    MATRIX *m2 = JS_GetOpaque2(ctx, argv[0], js_matrix4_class_id);

    matrix_functions->copy(m1, m2);

    return this_val;
}

static JSValue js_matrix4_identity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    MATRIX *m1 = JS_GetOpaque2(ctx, this_val, js_matrix4_class_id);

    matrix_functions->identity(m1);

    return this_val;
}

static JSValue js_matrix4_transpose(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    MATRIX *m1 = JS_GetOpaque2(ctx, this_val, js_matrix4_class_id);

    matrix_functions->transpose(m1, m1);

    return this_val;
}

static JSValue js_matrix4_invert(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    MATRIX *m1 = JS_GetOpaque2(ctx, this_val, js_matrix4_class_id);

    matrix_functions->inverse(m1, m1);

    return this_val;
}

static JSValue js_matrix4_clone(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    MATRIX *s;
    JSValue obj = JS_UNDEFINED;

    MATRIX *m1 = JS_GetOpaque2(ctx, this_val, js_matrix4_class_id);

    obj = JS_NewObjectClass(ctx, js_matrix4_class_id);
    if (JS_IsException(obj))
        goto fail;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    matrix_functions->copy(s, m1);

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue matrix4_get_prop(JSContext *ctx, JSValueConst obj, JSAtom prop, JSValueConst receiver) {
    MATRIX *m = JS_GetOpaque2(ctx, obj, js_matrix4_class_id);
    
    const char *prop_name = JS_AtomToCString(ctx, prop);
    if (!prop_name) return JS_EXCEPTION;

    char *endptr;
    long index = strtol(prop_name, &endptr, 10);
    
    if (*endptr == '\0' && index >= 0) { 
        JS_FreeCString(ctx, prop_name);

        if (index < 16) {
            return JS_NewFloat32(ctx, (*m)[index]);
        }
        
        return JS_EXCEPTION;
    }
    
    JS_FreeCString(ctx, prop_name);

    return JS_UNDEFINED;  // Prop not found here, call the default GetProp
}

static int matrix4_set_prop(JSContext *ctx, JSValueConst obj, JSAtom prop, JSValueConst val, JSValueConst receiver, int flags) {
    MATRIX *m = JS_GetOpaque2(ctx, obj, js_matrix4_class_id);
    
    const char *prop_name = JS_AtomToCString(ctx, prop);
    if (!prop_name) return -1;

    char *endptr;
    long index = strtol(prop_name, &endptr, 10);
    
    if (*endptr == '\0' && index >= 0) {
        JS_FreeCString(ctx, prop_name);
        float value;
        JS_ToFloat32(ctx, &value, val);

        if (index < 16) {
            (*m)[index] = value;

            return 1;
        }

        return 0;
    }
    
    JS_FreeCString(ctx, prop_name);

    return 2; // Prop not found here, call the default SetProp
}


static JSClassExoticMethods em = {
    .get_property = matrix4_get_prop,
    .set_property = matrix4_set_prop
};

static JSClassDef js_matrix4_class = {
    "Matrix4",
    .finalizer = js_matrix4_finalizer,
    .exotic = &em
}; 

static const JSCFunctionListEntry js_matrix4_proto_funcs[] = {
    JS_CFUNC_DEF("toString", 0, js_matrix4_tostring),
    JS_CFUNC_DEF("toArray", 0, js_matrix4_toarray),
    JS_CFUNC_DEF("fromArray", 1, js_matrix4_fromarray),

    JS_CFUNC_DEF("clone",     0, js_matrix4_clone),
    JS_CFUNC_DEF("copy",      1, js_matrix4_copy),
    JS_CFUNC_DEF("transpose", 0, js_matrix4_transpose),
    JS_CFUNC_DEF("invert",    0, js_matrix4_invert),
    JS_CFUNC_DEF("identity",  0, js_matrix4_identity),

    JS_PROP_INT32_DEF("length", 16, JS_PROP_CONFIGURABLE),
};

static void js_matrix4_init_operators(JSContext *ctx, JSValue proto)
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
    JS_SetPropertyStr(ctx, obj, "*",  JS_NewCFunction(ctx, js_matrix4_mul, "*", 2));
    JS_SetPropertyStr(ctx, obj, "==", JS_NewCFunction(ctx, js_matrix4_eq, "==", 2));

    JSValueConst args[1] = { obj };
    operatorSet = JS_Call(ctx, create_func, Operators, 1, args);
    
    JS_FreeValue(ctx, create_func);
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, Operators);

    JS_SetProperty(ctx, proto, JS_ValueToAtom(ctx, symbol_operatorSet), operatorSet);

    JS_FreeValue(ctx, symbol_operatorSet);
}

static int js_matrix4_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue matrix4_proto, matrix4_class;
    
    /* create the Matrix4 class */
    JS_NewClassID(&js_matrix4_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_matrix4_class_id, &js_matrix4_class);

    matrix4_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, matrix4_proto, js_matrix4_proto_funcs, countof(js_matrix4_proto_funcs));
    
    matrix4_class = JS_NewCFunction2(ctx, js_matrix4_ctor, "Matrix4", 16, JS_CFUNC_constructor_or_func, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, matrix4_class, matrix4_proto);
    JS_SetClassProto(ctx, js_matrix4_class_id, matrix4_proto);

    js_matrix4_init_operators(ctx, matrix4_proto);

    return JS_SetModuleExport(ctx, m, "Matrix4", matrix4_class);
}

JSModuleDef *athena_matrix_init(JSContext *ctx) {
    JSModuleDef *m;

    m = JS_NewCModule(ctx, "Matrix4", js_matrix4_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "Matrix4");
    return m;
}