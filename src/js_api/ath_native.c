/*
 * AthenaEnv Native Compiler - JavaScript API
 * 
 * Copyright (c) 2025 AthenaEnv Project
 * 
 * Provides the Native.compile() API for compiling JavaScript functions
 * to native MIPS R5900 code on PS2.
 */

/* int64/uint64 cross the JS boundary as Numbers now that BigInt is gone
 * along with CONFIG_BIGNUM. Exact up to 2^53; beyond that the low bits are
 * lost. Nothing ships using 64-bit natives today, and the compiler's own
 * 64-bit multiply/divide/modulo are already emulated and lossy on the
 * R5900 (no DMULT/DDIV), so this does not regress anything that worked. */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ath_env.h>
#include <athena/native_facade.h>
#include "../native_compiler/native_compiler.h"
#include "../native_compiler/native_array.h"

#ifdef PS2
#include <timer.h>
#endif

/* Global compiler instance managed by native_facade */

/* Forward declarations */
static JSValue js_native_call_wrapper(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv,
                                           int magic, JSValue *func_data);

/*
 * Native.compile(signature, function)
 * 
 * Compiles a JavaScript function to native MIPS R5900 code.
 * 
 * signature: {
 *   args: ['int', 'Float32Array', ...],
 *   returns: 'int' | 'void' | 'float' | ...
 * }
 * 
 * Returns a callable native function handle.
 */
static JSValue js_native_compile(JSContext *ctx, JSValue this_val, 
                                      int argc, JSValueConst *argv) {
    if (argc < 2) {
        return JS_ThrowSyntaxError(ctx, "Native.compile requires signature and function");
    }
    
    JSValueConst sig_obj = argv[0];
    JSValueConst js_func = argv[1];
    
    /* Validate function argument */
    if (!JS_IsFunction(ctx, js_func)) {
        return JS_ThrowTypeError(ctx, "Second argument must be a function");
    }
    
    /* Parse signature */
    NativeFuncSignature sig;
    memset(&sig, 0, sizeof(sig));
    
    /* Get 'args' array */
    JSValue args_val = JS_GetPropertyStr(ctx, sig_obj, "args");
    if (!JS_IsArray(ctx, args_val)) {
        JS_FreeValue(ctx, args_val);
        return JS_ThrowTypeError(ctx, "signature.args must be an array");
    }
    
    /* Parse argument types */
    JSValue args_len_val = JS_GetPropertyStr(ctx, args_val, "length");
    int32_t args_len;
    JS_ToInt32(ctx, &args_len, args_len_val);
    JS_FreeValue(ctx, args_len_val);
    
    if (args_len > MAX_NATIVE_ARGS) {
        JS_FreeValue(ctx, args_val);
        return JS_ThrowRangeError(ctx, "Too many arguments (max %d)", MAX_NATIVE_ARGS);
    }
    
    sig.arg_count = args_len;
    
    /* Array to store struct definitions for each argument (if any) */
    struct NativeStructDef *arg_struct_defs[MAX_NATIVE_ARGS] = {0};
    
    for (int i = 0; i < args_len; i++) {
        JSValue arg_type = JS_GetPropertyUint32(ctx, args_val, i);
        
        /* Check if arg_type is a string (primitive type), a one-element array
         * (struct array: [StructType]), or an object (struct constructor).
         * The array check MUST come before the generic object check below -
         * arrays are objects in QuickJS too, and a bare struct constructor's
         * _structDef check would otherwise run against the array itself and
         * fail with a confusing error. */
        if (JS_IsArray(ctx, arg_type)) {
            JSValue len_val = JS_GetPropertyStr(ctx, arg_type, "length");
            int32_t len = 0;
            JS_ToInt32(ctx, &len, len_val);
            JS_FreeValue(ctx, len_val);

            if (len != 1) {
                JS_FreeValue(ctx, arg_type);
                JS_FreeValue(ctx, args_val);
                return JS_ThrowTypeError(ctx,
                    "Struct array argument at index %d must be [StructType] (exactly one element)", i);
            }

            JSValue elem = JS_GetPropertyUint32(ctx, arg_type, 0);
            JSValue def_val = JS_GetPropertyStr(ctx, elem, "_structDef");
            if (JS_IsUndefined(def_val)) {
                JS_FreeValue(ctx, def_val);
                JS_FreeValue(ctx, elem);
                JS_FreeValue(ctx, arg_type);
                JS_FreeValue(ctx, args_val);
                return JS_ThrowTypeError(ctx,
                    "Struct array argument at index %d must wrap a valid struct type", i);
            }

            sig.arg_types[i] = NATIVE_TYPE_STRUCT_ARRAY;
            int64_t def_ptr;
            if (JS_ToInt64(ctx, &def_ptr, def_val) == 0 && def_ptr != 0) {
                arg_struct_defs[i] = (struct NativeStructDef *)(uintptr_t)def_ptr;
            }
            JS_FreeValue(ctx, def_val);
            JS_FreeValue(ctx, elem);
        } else if (JS_IsString(arg_type)) {
            /* Primitive type like 'int', 'float', 'ptr' or special 'self' */
            const char *type_str = JS_ToCString(ctx, arg_type);
            
            if (!type_str) {
                JS_FreeValue(ctx, arg_type);
                JS_FreeValue(ctx, args_val);
                return JS_ThrowTypeError(ctx, "Invalid argument type at index %d", i);
            }
            
            /* Check for 'self' keyword - indicates struct method that needs deferred compilation */
            if (strcmp(type_str, "self") == 0) {
                JS_FreeCString(ctx, type_str);
                JS_FreeValue(ctx, arg_type);
                JS_FreeValue(ctx, args_val);
                
                /* Return deferred compilation marker object */
                JSValue deferred = JS_NewObject(ctx);
                JS_DefinePropertyValueStr(ctx, deferred, "_deferred", JS_TRUE, 0);
                JS_DefinePropertyValueStr(ctx, deferred, "_signature", JS_DupValue(ctx, sig_obj), 0);
                JS_DefinePropertyValueStr(ctx, deferred, "_function", JS_DupValue(ctx, js_func), 0);
                return deferred;
            }
            
            sig.arg_types[i] = parse_type_name(type_str);
            
            if (sig.arg_types[i] == NATIVE_TYPE_UNKNOWN) {
                JS_FreeCString(ctx, type_str);
                JS_FreeValue(ctx, arg_type);
                JS_FreeValue(ctx, args_val);
                return JS_ThrowTypeError(ctx, "Unknown type '%s' at index %d", type_str, i);
            }
            
            JS_FreeCString(ctx, type_str);
        } else if (JS_IsFunction(ctx, arg_type) || JS_IsObject(arg_type)) {
            /* Struct constructor or struct reference object - extract NativeStructDef */
            /* Check for _structDef property */
            JSValue def_val = JS_GetPropertyStr(ctx, arg_type, "_structDef");
            if (!JS_IsUndefined(def_val)) {
                sig.arg_types[i] = NATIVE_TYPE_PTR;  /* Structs are passed as pointers */
                
                int64_t def_ptr;
                if (JS_ToInt64(ctx, &def_ptr, def_val) == 0 && def_ptr != 0) {
                    arg_struct_defs[i] = (struct NativeStructDef *)(uintptr_t)def_ptr;
                }
                JS_FreeValue(ctx, def_val);
            } else {
                JS_FreeValue(ctx, def_val);
                JS_FreeValue(ctx, arg_type);
                JS_FreeValue(ctx, args_val);
                return JS_ThrowTypeError(ctx, "Object at index %d is not a valid struct type", i);
            }
        } else {
            JS_FreeValue(ctx, arg_type);
            JS_FreeValue(ctx, args_val);
            return JS_ThrowTypeError(ctx, "Argument type at index %d must be a string or struct constructor", i);
        }
        
        JS_FreeValue(ctx, arg_type);
    }
    JS_FreeValue(ctx, args_val);
    
    /* Get 'returns' type */
    JSValue returns_val = JS_GetPropertyStr(ctx, sig_obj, "returns");
    if (!JS_IsUndefined(returns_val)) {
        const char *ret_str = JS_ToCString(ctx, returns_val);
        if (ret_str) {
            sig.return_type = parse_type_name(ret_str);
            JS_FreeCString(ctx, ret_str);
        }
    } else {
        sig.return_type = NATIVE_TYPE_VOID;
    }
    JS_FreeValue(ctx, returns_val);
    
    /* Initialize compiler if needed */
    if (athena_native_compiler_init(ctx) < 0)
        return JS_ThrowInternalError(ctx, "Failed to initialize compiler");

    NativeCompiler *compiler = athena_native_compiler_get();
    for (int i = 0; i < args_len && i < NC_MAX_LOCALS; i++)
        compiler->local_struct_defs[i] = arg_struct_defs[i];

    CompileResult result = athena_native_compile_function(ctx, js_func, &sig);

    if (!result.success) {
        return JS_ThrowInternalError(ctx, "Compilation failed: %s",
                                     result.error_msg ? result.error_msg : "unknown error");
    }
    
    /* Create a wrapper object to hold the native function */
    NativeFunc *func = (NativeFunc *)malloc(sizeof(NativeFunc));
    if (!func) {
        native_func_free(&result.func);
        return JS_ThrowInternalError(ctx, "Failed to allocate native function");
    }
    *func = result.func;
    
    /* Register for nested compiled-to-compiled calls via constant pool */
    native_register_compiled_function(compiler, func->code_ptr, &func->sig);
    
    /* Return the native function handle as an opaque pointer */
    JSValue handle = JS_NewUint32(ctx, (uint32_t)(uintptr_t)func);
    
    /* Create wrapper function that calls native code */
    /* Store handle for the wrapper */
    JSValue wrapper_data[1];
    wrapper_data[0] = handle;
    
    JSValue wrapper = JS_NewCFunctionData(ctx,
        (JSCFunctionData *)js_native_call_wrapper, 
        sig.arg_count, 0, 1, wrapper_data);
    
    /* Mark as native compiled function for detection in struct methods */
    JS_DefinePropertyValueStr(ctx, wrapper, "_isNative", JS_TRUE, 0);
    JS_DefinePropertyValueStr(ctx, wrapper, "_nativeHandle", 
                              JS_NewUint32(ctx, (uint32_t)(uintptr_t)func), 0);
    
    return wrapper;
}

/*
 * Internal wrapper function that calls native code.
 * Called when user invokes a compiled native function.
 */
static JSValue js_native_call_wrapper(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv,
                                           int magic, JSValue *func_data) {
    /* Get native function handle from closure data */
    uint32_t handle;
    JS_ToUint32(ctx, &handle, func_data[0]);
    
    NativeFunc *func = (NativeFunc *)(uintptr_t)handle;
    
    if (!func || !func->is_valid) {
        return JS_ThrowTypeError(ctx, "Invalid native function");
    }
    
    return athena_native_func_invoke(ctx, func, argc, argv);
}

/*
 * Native.isSupported()
 * 
 * Returns true if native compilation is supported on this platform.
 */
static JSValue js_native_is_supported(JSContext *ctx, JSValue this_val,
                                           int argc, JSValueConst *argv) {
    return athena_native_is_supported() ? JS_TRUE : JS_FALSE;
}

/*
 * Native.free(func)
 * 
 * Frees a compiled native function.
 */
static JSValue js_native_free(JSContext *ctx, JSValue this_val,
                                   int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowSyntaxError(ctx, "Native.free requires a function handle");
    }
    
    uint32_t handle;
    if (JS_ToUint32(ctx, &handle, argv[0]) < 0) {
        return JS_ThrowTypeError(ctx, "Invalid function handle");
    }
    
    NativeFunc *func = (NativeFunc *)(uintptr_t)handle;
    athena_native_func_release(func);
    
    return JS_UNDEFINED;
}

/*
 * Native.getInfo(func)
 * 
 * Returns information about a compiled native function.
 */
static JSValue js_native_get_info(JSContext *ctx, JSValue this_val,
                                       int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowSyntaxError(ctx, "Native.getInfo requires a function handle");
    }
    
    uint32_t handle;
    if (JS_ToUint32(ctx, &handle, argv[0]) < 0) {
        return JS_ThrowTypeError(ctx, "Invalid function handle");
    }
    
    NativeFunc *func = (NativeFunc *)(uintptr_t)handle;
    if (!func || !func->is_valid) {
        return JS_ThrowTypeError(ctx, "Invalid native function");
    }
    
    AthenaNativeFuncInfo info;
    if (athena_native_func_query_info(func, &info) != 0)
        return JS_ThrowTypeError(ctx, "Invalid native function");

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "codeSize", JS_NewInt32(ctx, info.code_size));
    JS_SetPropertyStr(ctx, obj, "argCount", JS_NewInt32(ctx, info.arg_count));
    JS_SetPropertyStr(ctx, obj, "returnType", JS_NewString(ctx, info.return_type));

    JSValue args = JS_NewArray(ctx);
    for (int i = 0; i < info.arg_count; i++)
        JS_SetPropertyUint32(ctx, args, i, JS_NewString(ctx, info.arg_types[i]));
    JS_SetPropertyStr(ctx, obj, "argTypes", args);
    athena_native_func_info_free(&info);

    return obj;
}

/*
 * Native.benchmark(func, iterations)
 * 
 * Benchmarks a native function by calling it multiple times.
 * Returns execution time in milliseconds.
 */
static JSValue js_native_benchmark(JSContext *ctx, JSValue this_val,
                                        int argc, JSValueConst *argv) {
    if (argc < 2) {
        return JS_ThrowSyntaxError(ctx, "Native.benchmark requires function and iterations");
    }
    
    /* Get the wrapper function */
    JSValueConst func = argv[0];
    if (!JS_IsFunction(ctx, func)) {
        return JS_ThrowTypeError(ctx, "First argument must be a function");
    }
    
    int32_t iterations;
    if (JS_ToInt32(ctx, &iterations, argv[1]) < 0 || iterations <= 0) {
        return JS_ThrowRangeError(ctx, "iterations must be a positive integer");
    }
    
    double elapsed_ms = athena_native_benchmark(ctx, func, iterations);
    return JS_NewFloat64(ctx, elapsed_ms);
}

/*
 * Native.disassemble(func)
 * 
 * Returns a string containing MIPS assembly for a compiled native function.
 * Useful for debugging and understanding generated code.
 */
static JSValue js_native_disassemble(JSContext *ctx, JSValue this_val,
                                          int argc, JSValueConst *argv) {
    (void)argv;
    if (argc < 1)
        return JS_ThrowSyntaxError(ctx, "Native.disassemble requires a function");

    NativeCompiler *compiler = athena_native_compiler_get();
    if (!compiler)
        return JS_ThrowTypeError(ctx, "No native compiler initialized");

    char *result = athena_native_disassemble(compiler);
    if (!result)
        return JS_ThrowTypeError(ctx, "No compiled code available");

    JSValue js_result = JS_NewString(ctx, result);
    free(result);
    return js_result;
}

/* ============================================
 * Native.struct Implementation
 * ============================================ */

#include "../native_compiler/native_struct.h"

/* JS Class ID for struct instances - exported for native_compiler.c */
JSClassID js_struct_class_id = 0;

/* JS Class ID for struct definitions (type holder) */
static JSClassID js_struct_def_class_id = 0;

/* Struct instance finalizer */
static void js_struct_finalizer(JSRuntime *rt, JSValue val) {
    NativeStructInstance *inst = JS_GetOpaque(val, js_struct_class_id);
    if (inst && inst->managed) {
        native_struct_free(inst);
    }
}

/* Struct definition finalizer */
static void js_struct_def_finalizer(JSRuntime *rt, JSValue val) {
    NativeStructDef *def = JS_GetOpaque(val, js_struct_def_class_id);
    if (def) {
        def->ref_count--;
        if (def->ref_count <= 0) {
            free(def);
        }
    }
}

/* Struct instance class definition */
static JSClassDef js_struct_class = {
    "NativeStruct",
    .finalizer = js_struct_finalizer,
};

/* Get pointer to struct data for native calls */
static JSValue js_struct_ptr(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    NativeStructInstance *inst = JS_GetOpaque(this_val, js_struct_class_id);
    if (!inst || !inst->data) return JS_UNDEFINED;
    
    /* Return pointer as integer (for passing to native functions as ptr type) */
    return JS_NewInt32(ctx, (int32_t)(intptr_t)inst->data);
}

/* Struct definition class definition */
static JSClassDef js_struct_def_class = {
    "NativeStructDef",
    .finalizer = js_struct_def_finalizer,
};

/* ============================================
 * NativeStructArray - Array field view
 * Allows mat.m[0] style access for array fields
 * ============================================ */

/* JS Class ID for array views */
static JSClassID js_struct_array_class_id = 0;

/* Array view data - holds reference to struct instance and field info */
typedef struct {
    NativeStructInstance *inst;  /* Parent struct instance */
    NativeStructField *field;    /* Field definition */
    JSValue parent_ref;          /* Reference to parent JS object to prevent GC */
} NativeStructArrayView;

/* Array view finalizer */
static void js_struct_array_finalizer(JSRuntime *rt, JSValue val) {
    NativeStructArrayView *view = JS_GetOpaque(val, js_struct_array_class_id);
    if (view) {
        JS_FreeValueRT(rt, view->parent_ref);
        free(view);
    }
}

/* Array view get_own_property - handles numeric index access */
static int js_struct_array_get_own_property(JSContext *ctx,
                                             JSPropertyDescriptor *desc,
                                             JSValueConst obj, JSAtom prop) {
    NativeStructArrayView *view = JS_GetOpaque(obj, js_struct_array_class_id);
    if (!view || !view->inst || !view->field) return 0;
    
    /* Check if prop is a numeric index */
    const char *prop_str = JS_AtomToCString(ctx, prop);
    if (!prop_str) return 0;
    
    /* Check for "length" property */
    if (strcmp(prop_str, "length") == 0) {
        JS_FreeCString(ctx, prop_str);
        if (desc) {
            desc->flags = JS_PROP_ENUMERABLE;
            desc->value = JS_NewInt32(ctx, view->field->array_count);
            desc->getter = JS_UNDEFINED;
            desc->setter = JS_UNDEFINED;
        }
        return 1;
    }
    
    /* Try to parse as integer index */
    char *endptr;
    long idx = strtol(prop_str, &endptr, 10);
    JS_FreeCString(ctx, prop_str);
    
    if (*endptr != '\0') return 0;  /* Not a valid number */
    if (idx < 0 || idx >= view->field->array_count) return 0;  /* Out of bounds */
    
    /* Calculate pointer to element */
    size_t elem_size = native_type_size(view->field->type);
    uint8_t *ptr = (uint8_t *)view->inst->data + view->field->offset + (idx * elem_size);
    
    if (desc) {
        desc->flags = JS_PROP_WRITABLE | JS_PROP_ENUMERABLE;
        desc->getter = JS_UNDEFINED;
        desc->setter = JS_UNDEFINED;
        
        /* Read value based on type */
        switch (view->field->type) {
            case NATIVE_TYPE_INT32:
                desc->value = JS_NewInt32(ctx, *(int32_t *)ptr);
                break;
            case NATIVE_TYPE_UINT32:
                desc->value = JS_NewUint32(ctx, *(uint32_t *)ptr);
                break;
            case NATIVE_TYPE_FLOAT32:
                desc->value = JS_NewFloat64(ctx, *(float *)ptr);
                break;
            case NATIVE_TYPE_INT64:
                desc->value = JS_NewFloat64(ctx, (double)*(int64_t *)ptr);
                break;
            case NATIVE_TYPE_UINT64:
                desc->value = JS_NewFloat64(ctx, (double)*(uint64_t *)ptr);
                break;
            default:
                desc->value = JS_UNDEFINED;
        }
    }
    return 1;
}

/* Array view set_property - handles numeric index assignment */
static int js_struct_array_set_property(JSContext *ctx, JSValueConst obj,
                                         JSAtom prop, JSValueConst value,
                                         JSValueConst receiver, int flags) {
    NativeStructArrayView *view = JS_GetOpaque(obj, js_struct_array_class_id);
    if (!view || !view->inst || !view->field) return -1;
    
    /* Parse index */
    const char *prop_str = JS_AtomToCString(ctx, prop);
    if (!prop_str) return -1;
    
    char *endptr;
    long idx = strtol(prop_str, &endptr, 10);
    JS_FreeCString(ctx, prop_str);
    
    if (*endptr != '\0') return -1;  /* Not a valid number */
    if (idx < 0 || idx >= view->field->array_count) {
        JS_ThrowRangeError(ctx, "Array index %ld out of bounds (0-%d)", 
                          idx, view->field->array_count - 1);
        return -1;
    }
    
    /* Calculate pointer to element */
    size_t elem_size = native_type_size(view->field->type);
    uint8_t *ptr = (uint8_t *)view->inst->data + view->field->offset + (idx * elem_size);
    
    /* Write value based on type */
    switch (view->field->type) {
        case NATIVE_TYPE_INT32: {
            int32_t v;
            if (JS_ToInt32(ctx, &v, value) < 0) return -1;
            *(int32_t *)ptr = v;
            break;
        }
        case NATIVE_TYPE_UINT32: {
            uint32_t v;
            if (JS_ToUint32(ctx, &v, value) < 0) return -1;
            *(uint32_t *)ptr = v;
            break;
        }
        case NATIVE_TYPE_FLOAT32: {
            double d;
            if (JS_ToFloat64(ctx, &d, value) < 0) return -1;
            *(float *)ptr = (float)d;
            break;
        }
        case NATIVE_TYPE_INT64: {
            int64_t v;
            double dv;
            if (JS_ToFloat64(ctx, &dv, value) == 0) {
                v = (int64_t)dv;
            } else {
                int32_t v32;
                if (JS_ToInt32(ctx, &v32, value) < 0) return -1;
                v = v32;
            }
            *(int64_t *)ptr = v;
            break;
        }
        case NATIVE_TYPE_UINT64: {
            int64_t v;
            double dv;
            if (JS_ToFloat64(ctx, &dv, value) == 0) {
                v = (int64_t)(uint64_t)dv;
            } else {
                uint32_t v32;
                if (JS_ToUint32(ctx, &v32, value) < 0) return -1;
                v = (int64_t)v32;
            }
            *(uint64_t *)ptr = (uint64_t)v;
            break;
        }
        default:
            return -1;
    }
    return 1;
}

/* Exotic methods for array-like behavior */
static const JSClassExoticMethods js_struct_array_exotic = {
    .get_own_property = js_struct_array_get_own_property,
    .define_own_property = js_struct_array_set_property,
};

/* Array view class definition */
static JSClassDef js_struct_array_class = {
    "NativeStructArray",
    .finalizer = js_struct_array_finalizer,
    .exotic = &js_struct_array_exotic,
};

/* Helper: Create array view for a field */
static JSValue js_struct_create_array_view(JSContext *ctx, JSValueConst parent,
                                            NativeStructInstance *inst,
                                            NativeStructField *field) {
    /* Ensure class is registered */
    if (js_struct_array_class_id == 0) {
        JS_NewClassID(&js_struct_array_class_id);
        JS_NewClass(JS_GetRuntime(ctx), js_struct_array_class_id, &js_struct_array_class);
    }
    
    /* Allocate view data */
    NativeStructArrayView *view = malloc(sizeof(NativeStructArrayView));
    if (!view) return JS_ThrowOutOfMemory(ctx);
    
    view->inst = inst;
    view->field = field;
    view->parent_ref = JS_DupValue(ctx, parent);  /* Keep parent alive */
    
    /* Create JS object */
    JSValue obj = JS_NewObjectClass(ctx, js_struct_array_class_id);
    if (JS_IsException(obj)) {
        JS_FreeValue(ctx, view->parent_ref);
        free(view);
        return obj;
    }
    
    JS_SetOpaque(obj, view);
    return obj;
}

/* Struct instance getter */
static JSValue js_struct_get(JSContext *ctx, JSValueConst this_val, int magic) {
    NativeStructInstance *inst = JS_GetOpaque(this_val, js_struct_class_id);
    if (!inst || !inst->def) return JS_UNDEFINED;
    
    if (magic < 0 || magic >= inst->def->field_count) return JS_UNDEFINED;
    
    NativeStructField *field = &inst->def->fields[magic];
    
    /* If field is an array, return an array view */
    if (field->flags & FIELD_FLAG_ARRAY) {
        return js_struct_create_array_view(ctx, this_val, inst, field);
    }
    
    uint8_t *ptr = (uint8_t *)inst->data + field->offset;
    
    switch (field->type) {
        case NATIVE_TYPE_INT32:
            return JS_NewInt32(ctx, *(int32_t *)ptr);
        case NATIVE_TYPE_UINT32:
            return JS_NewUint32(ctx, *(uint32_t *)ptr);
        case NATIVE_TYPE_FLOAT32:
            return JS_NewFloat64(ctx, *(float *)ptr);
        case NATIVE_TYPE_INT64:
            return JS_NewFloat64(ctx, (double)*(int64_t *)ptr);
        case NATIVE_TYPE_UINT64:
            return JS_NewFloat64(ctx, (double)*(uint64_t *)ptr);
        default:
            return JS_UNDEFINED;
    }
}

/* Struct instance setter */
static JSValue js_struct_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic) {
    NativeStructInstance *inst = JS_GetOpaque(this_val, js_struct_class_id);
    if (!inst || !inst->def) return JS_UNDEFINED;
    
    if (magic < 0 || magic >= inst->def->field_count) return JS_UNDEFINED;
    
    NativeStructField *field = &inst->def->fields[magic];
    uint8_t *ptr = (uint8_t *)inst->data + field->offset;
    
    switch (field->type) {
        case NATIVE_TYPE_INT32: {
            int32_t v;
            if (JS_ToInt32(ctx, &v, val) < 0) return JS_EXCEPTION;
            *(int32_t *)ptr = v;
            break;
        }
        case NATIVE_TYPE_UINT32: {
            uint32_t v;
            if (JS_ToUint32(ctx, &v, val) < 0) return JS_EXCEPTION;
            *(uint32_t *)ptr = v;
            break;
        }
        case NATIVE_TYPE_FLOAT32: {
            double d;
            if (JS_ToFloat64(ctx, &d, val) < 0) return JS_EXCEPTION;
            *(float *)ptr = (float)d;
            break;
        }
        case NATIVE_TYPE_INT64: {
            int64_t v;
            double dv;
            if (JS_ToFloat64(ctx, &dv, val) == 0) {
                v = (int64_t)dv;
            } else {
                /* Fallback to int32 */
                int32_t v32;
                if (JS_ToInt32(ctx, &v32, val) < 0) return JS_EXCEPTION;
                v = v32;
            }
            *(int64_t *)ptr = v;
            break;
        }
        default:
            break;
    }
    return JS_UNDEFINED;
}

/* ============================================
 * NativeStructInstanceArray - StructType.array(N)
 * A contiguous run of N struct instances (see native_struct_alloc_array),
 * indexable from JS (arr[i] -> a live view over element i, sharing storage)
 * and passable directly as a Native.compile [StructType] arg. Unrelated to
 * NativeStructArrayView above, which is a view over an ARRAY FIELD *inside*
 * one struct instance (e.g. mat.m[i]), not an array of struct instances.
 * ============================================ */

/* JS Class ID for the array wrapper itself (opaque = NativeStructInstance*,
 * the one covering the whole block, with count == N). Individual elements
 * (arr[i]) are plain js_struct_class_id objects wrapping a
 * native_struct_view_at() instance - see js_struct_make_element_view. */
JSClassID js_struct_instance_array_class_id = 0;

static void js_struct_instance_array_finalizer(JSRuntime *rt, JSValue val) {
    NativeStructInstance *inst = JS_GetOpaque(val, js_struct_instance_array_class_id);
    if (inst) {
        native_struct_free(inst);  /* frees the whole contiguous block */
    }
}

/* Build a live view object for element `elem_data` of `def`-typed elements,
 * reusing js_struct_get/js_struct_set unchanged (same class ID, same magic-
 * indexed getter/setter installation as js_struct_constructor). `parent` is
 * kept alive via a hidden property, since elem_data points INTO parent's
 * buffer rather than owning its own copy. */
static JSValue js_struct_make_element_view(JSContext *ctx, JSValueConst parent,
                                            NativeStructDef *def, void *elem_data) {
    NativeStructInstance *inst = native_struct_view_at(def, elem_data);
    if (!inst) return JS_ThrowOutOfMemory(ctx);

    JSValue obj = JS_NewObjectClass(ctx, js_struct_class_id);
    if (JS_IsException(obj)) {
        native_struct_free(inst);
        return obj;
    }
    JS_SetOpaque(obj, inst);

    for (int i = 0; i < def->field_count; i++) {
        JSAtom atom = JS_NewAtom(ctx, def->fields[i].name);
        JS_DefinePropertyGetSet(ctx, obj, atom,
            JS_NewCFunctionMagic(ctx, (JSCFunctionMagic *)js_struct_get, def->fields[i].name, 0, JS_CFUNC_getter_magic, i),
            JS_NewCFunctionMagic(ctx, (JSCFunctionMagic *)js_struct_set, def->fields[i].name, 1, JS_CFUNC_setter_magic, i),
            0);
        JS_FreeAtom(ctx, atom);
    }

    JS_DefinePropertyValueStr(ctx, obj, "__native_array_parent",
                              JS_DupValue(ctx, parent), 0);
    return obj;
}

/* Exotic get_own_property - handles numeric index access (arr[i]) and
 * "length" on the array wrapper. No define_own_property/set trap: whole-
 * element assignment (arr[i] = x) is out of scope, matching the native-
 * compiler side (OP_put_array_el errors on this for struct arrays) -
 * individual fields remain writable through the element view's own
 * js_struct_set getters/setters. */
static int js_struct_instance_array_get_own_property(JSContext *ctx,
                                                       JSPropertyDescriptor *desc,
                                                       JSValueConst obj, JSAtom prop) {
    NativeStructInstance *arr_inst = JS_GetOpaque(obj, js_struct_instance_array_class_id);
    if (!arr_inst || !arr_inst->def || !arr_inst->data) return 0;

    const char *prop_str = JS_AtomToCString(ctx, prop);
    if (!prop_str) return 0;

    if (strcmp(prop_str, "length") == 0) {
        JS_FreeCString(ctx, prop_str);
        if (desc) {
            desc->flags = JS_PROP_ENUMERABLE;
            desc->value = JS_NewInt32(ctx, (int32_t)arr_inst->count);
            desc->getter = JS_UNDEFINED;
            desc->setter = JS_UNDEFINED;
        }
        return 1;
    }

    char *endptr;
    long idx = strtol(prop_str, &endptr, 10);
    JS_FreeCString(ctx, prop_str);

    if (*endptr != '\0') return 0;  /* not a numeric index */
    if (idx < 0 || (uint32_t)idx >= arr_inst->count) return 0;  /* out of bounds */

    if (desc) {
        /* Reuse a cached element view instead of allocating a fresh one on
         * every access. Each view costs ~13 allocations (object + one getter
         * + one setter closure per field + one atom per field) - fine for an
         * occasional read, but arr[i].field from plain JS (as opposed to
         * inside Native.compile, where it's just pointer arithmetic - see
         * IR_ARRAY_ELEM_ADDR) at real per-frame game-loop volume turned into
         * a real allocator/GC bottleneck.
         *
         * Cached under a plain (non-exotic) property on the array wrapper,
         * so it's resolved by normal property lookup (find_own_property in
         * quickjs.c) before this exotic get_own_property is ever consulted -
         * no risk of re-entering this function for "__view_cache" itself. */
        JSValue cache = JS_GetPropertyStr(ctx, obj, "__view_cache");
        if (JS_IsUndefined(cache)) {
            cache = JS_NewArray(ctx);
            JS_DefinePropertyValueStr(ctx, obj, "__view_cache", JS_DupValue(ctx, cache), 0);
        }
        JSValue view = JS_GetPropertyUint32(ctx, cache, (uint32_t)idx);
        if (JS_IsUndefined(view)) {
            void *elem_data = (uint8_t *)arr_inst->data + (size_t)idx * arr_inst->def->size;
            view = js_struct_make_element_view(ctx, obj, arr_inst->def, elem_data);
            JS_SetPropertyUint32(ctx, cache, (uint32_t)idx, JS_DupValue(ctx, view));
        }
        JS_FreeValue(ctx, cache);

        desc->flags = JS_PROP_ENUMERABLE;
        desc->getter = JS_UNDEFINED;
        desc->setter = JS_UNDEFINED;
        desc->value = view;
    }
    return 1;
}

static const JSClassExoticMethods js_struct_instance_array_exotic = {
    .get_own_property = js_struct_instance_array_get_own_property,
};

static JSClassDef js_struct_instance_array_class = {
    "NativeStructInstanceArray",
    .finalizer = js_struct_instance_array_finalizer,
    .exotic = &js_struct_instance_array_exotic,
};

/* StructType.array(count) - allocates `count` contiguous instances.
 * func_data[0] = def_holder (same holder js_struct_constructor uses). */
static JSValue js_struct_array_factory(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv,
                                        int magic, JSValue *func_data) {
    NativeStructDef *def = JS_GetOpaque(func_data[0], js_struct_def_class_id);
    if (!def) return JS_ThrowTypeError(ctx, "Invalid struct type");

    int32_t count = 0;
    if (argc < 1 || JS_ToInt32(ctx, &count, argv[0]) < 0 || count < 1) {
        return JS_ThrowTypeError(ctx, "StructType.array(count) requires count >= 1");
    }

    if (js_struct_instance_array_class_id == 0) {
        JS_NewClassID(&js_struct_instance_array_class_id);
        JS_NewClass(JS_GetRuntime(ctx), js_struct_instance_array_class_id, &js_struct_instance_array_class);
    }

    NativeStructInstance *inst = native_struct_alloc_array(def, (uint32_t)count, 1);
    if (!inst) return JS_ThrowOutOfMemory(ctx);

    JSValue obj = JS_NewObjectClass(ctx, js_struct_instance_array_class_id);
    if (JS_IsException(obj)) {
        native_struct_free(inst);
        return obj;
    }
    JS_SetOpaque(obj, inst);
    return obj;
}

/* Native method wrapper - calls native function with struct ptr as arg0
 * func_data[0] = original native function
 * func_data[1] = struct instance (obj)
 */
static JSValue js_native_method_wrapper(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv,
                                         int magic, JSValue *func_data) {
    JSValue native_func = func_data[0];
    
    /* Get struct pointer from this_val */
    NativeStructInstance *inst = JS_GetOpaque(this_val, js_struct_class_id);
    if (!inst || !inst->data) {
        return JS_ThrowTypeError(ctx, "Method called on invalid struct instance");
    }
    
    /* Create new argv with struct pointer as first argument */
    int new_argc = argc + 1;
    JSValue *new_argv = js_malloc(ctx, sizeof(JSValue) * new_argc);
    if (!new_argv) {
        return JS_ThrowOutOfMemory(ctx);
    }
    
    /* First arg is struct pointer (as uint32 for native code) */
    new_argv[0] = JS_NewUint32(ctx, (uint32_t)(uintptr_t)inst->data);
    
    /* Copy remaining args */
    for (int i = 0; i < argc; i++) {
        new_argv[i + 1] = argv[i];
    }
    
    /* Call the native function */
    JSValue result = JS_Call(ctx, native_func, JS_UNDEFINED, new_argc, new_argv);
    
    /* Free struct pointer arg (we created it) */
    JS_FreeValue(ctx, new_argv[0]);
    js_free(ctx, new_argv);
    
    return result;
}

/* Struct constructor - called when user does new StructType() */
static JSValue js_struct_constructor(JSContext *ctx, JSValueConst new_target,
                                      int argc, JSValueConst *argv,
                                      int magic, JSValue *func_data) {
    NativeStructDef *def = JS_GetOpaque(func_data[0], js_struct_def_class_id);  /* Get def from func_data */
    if (!def) return JS_ThrowTypeError(ctx, "Invalid struct type");
    
    /* Create instance */
    NativeStructInstance *inst = native_struct_alloc(def, 1);  /* managed = true */
    if (!inst) return JS_ThrowOutOfMemory(ctx);
    
    /* Create JS object */
    JSValue obj = JS_NewObjectClass(ctx, js_struct_class_id);
    if (JS_IsException(obj)) {
        native_struct_free(inst);
        return obj;
    }
    
    JS_SetOpaque(obj, inst);
    
    /* Add getter/setters for each field */
    for (int i = 0; i < def->field_count; i++) {
        JSAtom atom = JS_NewAtom(ctx, def->fields[i].name);
        JS_DefinePropertyGetSet(ctx, obj, atom,
            JS_NewCFunctionMagic(ctx, (JSCFunctionMagic *)js_struct_get, def->fields[i].name, 0, JS_CFUNC_getter_magic, i),
            JS_NewCFunctionMagic(ctx, (JSCFunctionMagic *)js_struct_set, def->fields[i].name, 1, JS_CFUNC_setter_magic, i),
            0);
        JS_FreeAtom(ctx, atom);
    }
    
    /* Bind methods from func_data[1] if provided */
    JSValue methods_obj = func_data[1];

    if (JS_IsObject(methods_obj)) {
        JSPropertyEnum *method_props = NULL;
        uint32_t method_count = 0;

        if (JS_GetOwnPropertyNames(ctx, &method_props, &method_count, methods_obj,
                                    JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) >= 0) {
            for (uint32_t i = 0; i < method_count; i++) {
                JSValue method_val = JS_GetProperty(ctx, methods_obj, method_props[i].atom);
                const char *method_name = JS_AtomToCString(ctx, method_props[i].atom);

                /* Check for deferred compilation marker (has 'self' arg) */
                JSValue is_deferred = JS_GetPropertyStr(ctx, method_val, "_deferred");
                int deferred_bool = JS_ToBool(ctx, is_deferred);

                if (deferred_bool) {
                    JS_FreeValue(ctx, is_deferred);
                    
                    /* Deferred compilation - compile now with struct type */
                    JSValue orig_sig = JS_GetPropertyStr(ctx, method_val, "_signature");
                    JSValue orig_func = JS_GetPropertyStr(ctx, method_val, "_function");
                    JS_FreeValue(ctx, method_val);
                    
                    /* Get original args array and modify 'self' -> struct constructor */
                    JSValue orig_args = JS_GetPropertyStr(ctx, orig_sig, "args");
                    int64_t args_len = 0;
                    JSValue len_val = JS_GetPropertyStr(ctx, orig_args, "length");
                    JS_ToInt64(ctx, &args_len, len_val);
                    JS_FreeValue(ctx, len_val);
                    
                    /* Create new args array with 'self' replaced by struct ptr info */
                    JSValue new_args = JS_NewArray(ctx);
                    for (int j = 0; j < args_len; j++) {
                        JSValue arg = JS_GetPropertyUint32(ctx, orig_args, j);
                        if (JS_IsString(arg)) {
                            const char *str = JS_ToCString(ctx, arg);
                            if (str && strcmp(str, "self") == 0) {
                                /* Replace 'self' with struct type reference */
                                JS_FreeValue(ctx, arg);
                                
                                /* Create a pseudo-function object with _structDef property
                                 * that athena_native_compile can recognize as struct type */
                                NativeStructDef *def = JS_GetOpaque(func_data[0], js_struct_def_class_id);

                                JSValue struct_ref = JS_NewObject(ctx);
                                JS_SetPropertyStr(ctx, struct_ref, "_structDef",
                                                  JS_NewInt64(ctx, (int64_t)(uintptr_t)def));

                                /* Mark as function so JS_IsFunction check passes in athena_native_compile */
                                arg = struct_ref;
                            }
                            if (str) JS_FreeCString(ctx, str);
                        }
                        JS_SetPropertyUint32(ctx, new_args, j, arg);
                    }
                    JS_FreeValue(ctx, orig_args);
                    
                    /* Create new signature with modified args */
                    JSValue new_sig = JS_NewObject(ctx);
                    JS_SetPropertyStr(ctx, new_sig, "args", new_args);
                    JSValue ret_type = JS_GetPropertyStr(ctx, orig_sig, "returns");
                    JS_SetPropertyStr(ctx, new_sig, "returns", ret_type);
                    JS_FreeValue(ctx, orig_sig);
                    
                    /* Compile with proper signature (self -> ptr with struct def) */
                    JSValue compile_args[2] = { new_sig, orig_func };
                    JSValue compiled = js_native_compile(ctx, JS_UNDEFINED, 2, compile_args);

                    JS_FreeValue(ctx, new_sig);
                    JS_FreeValue(ctx, orig_func);
                    
                    if (JS_IsException(compiled)) {
                        /* Report and skip: the struct still gets its fields, it
                         * just won't carry this method. */
                        JSValue exception = JS_GetException(ctx);
                        const char *err_msg = JS_ToCString(ctx, exception);
                        printf("Native.struct: could not compile method '%s': %s\n",
                               method_name ? method_name : "???",
                               err_msg ? err_msg : "unknown error");
                        if (err_msg) JS_FreeCString(ctx, err_msg);
                        JS_FreeValue(ctx, exception);

                        if (method_name) JS_FreeCString(ctx, method_name);
                        continue;  /* Skip this method on error */
                    }
                    
                    /* Wrap compiled method to pass this ptr as arg0 */
                    JSValue wrapper_data[1] = { compiled };
                    JSValue wrapper = JS_NewCFunctionData(ctx, 
                        (JSCFunctionData *)js_native_method_wrapper,
                        0, 0, 1, wrapper_data);

                    JS_DefinePropertyValue(ctx, obj, method_props[i].atom,
                                          wrapper, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
                } else if (JS_IsFunction(ctx, method_val)) {
                    JS_FreeValue(ctx, is_deferred);
                    
                    /* Check if this is already a native compiled method */
                    JSValue is_native = JS_GetPropertyStr(ctx, method_val, "_isNative");
                    
                    if (JS_ToBool(ctx, is_native)) {
                        /* Native method: wrap to pass struct ptr as arg0 */
                        JS_FreeValue(ctx, is_native);
                        
                        JSValue wrapper_data[1] = { method_val };
                        JSValue wrapper = JS_NewCFunctionData(ctx, 
                            (JSCFunctionData *)js_native_method_wrapper,
                            0, 0, 1, wrapper_data);
                        
                        JS_DefinePropertyValue(ctx, obj, method_props[i].atom,
                                              wrapper, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
                    } else {
                        JS_FreeValue(ctx, is_native);
                        /* Regular JS function */
                        JS_DefinePropertyValue(ctx, obj, method_props[i].atom, 
                                              method_val, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
                    }
                } else {
                    JS_FreeValue(ctx, is_deferred);
                    JS_FreeValue(ctx, method_val);
                }
                
                if (method_name) JS_FreeCString(ctx, method_name);
            }
            
            /* Free property list */
            for (uint32_t i = 0; i < method_count; i++) {
                JS_FreeAtom(ctx, method_props[i].atom);
            }
            js_free(ctx, method_props);
        }
    }
    
    return obj;
}

// Native.struct(definition)
// 
// Creates a struct type constructor.
// Example: const Vec3 = Native.struct({ x: 'float', y: 'float', z: 'float' });
static JSValue js_native_struct(JSContext *ctx, JSValue this_val,
                                    int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "struct requires a definition object");
    }
    
    if (!JS_IsObject(argv[0])) {
        return JS_ThrowTypeError(ctx, "struct definition must be an object");
    }
    
    /* Ensure class is registered */
    if (js_struct_class_id == 0) {
        JS_NewClassID(&js_struct_class_id);
        JS_NewClass(JS_GetRuntime(ctx), js_struct_class_id, &js_struct_class);
    }
    if (js_struct_def_class_id == 0) {
        JS_NewClassID(&js_struct_def_class_id);
        JS_NewClass(JS_GetRuntime(ctx), js_struct_def_class_id, &js_struct_def_class);
    }
    
    /* Create struct definition */
    NativeStructDef *def = native_struct_create_def("anonymous");
    if (!def) return JS_ThrowOutOfMemory(ctx);
    
    /* Parse fields from definition object */
    JSPropertyEnum *props = NULL;
    uint32_t prop_count = 0;
    
    if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, argv[0], 
                                JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
        free(def);
        return JS_EXCEPTION;
    }
    
    for (uint32_t i = 0; i < prop_count; i++) {
        const char *field_name = JS_AtomToCString(ctx, props[i].atom);
        JSValue type_val = JS_GetProperty(ctx, argv[0], props[i].atom);
        const char *type_str = JS_ToCString(ctx, type_val);
        
        if (!field_name || !type_str) {
            JS_FreeValue(ctx, type_val);
            if (field_name) JS_FreeCString(ctx, field_name);
            if (type_str) JS_FreeCString(ctx, type_str);
            continue;
        }
        
        /* Parse type with optional array suffix [N] */
        int array_count = 1;
        char base_type[32];
        strncpy(base_type, type_str, sizeof(base_type) - 1);
        
        char *bracket = strchr(base_type, '[');
        if (bracket) {
            *bracket = '\0';
            array_count = atoi(bracket + 1);
            if (array_count < 1) array_count = 1;
        }
        
        NativeType type = parse_type_name(base_type);
        if (type == NATIVE_TYPE_UNKNOWN) {
            JS_FreeCString(ctx, field_name);
            JS_FreeCString(ctx, type_str);
            JS_FreeValue(ctx, type_val);
            continue;
        }
        
        native_struct_add_field(def, field_name, type, array_count);
        
        JS_FreeCString(ctx, field_name);
        JS_FreeCString(ctx, type_str);
        JS_FreeValue(ctx, type_val);
    }
    
    /* Free property list */
    for (uint32_t i = 0; i < prop_count; i++) {
        JS_FreeAtom(ctx, props[i].atom);
    }
    js_free(ctx, props);
    
    /* Finalize struct layout */
    native_struct_finalize(def);
    
    /* Register struct globally so compiler can look up fields */
    native_struct_register(def);
    
    /* Create constructor function
     * We store the def as an opaque in a holder object, then create a 
     * constructor function that gets the def from func_data
     * func_data[0] = def_holder, func_data[1] = methods object (optional) */
    JSValue def_holder = JS_NewObjectClass(ctx, js_struct_def_class_id);
    JS_SetOpaque(def_holder, def);
    
    /* Check for methods object (second argument) */
    JSValue methods_obj = JS_UNDEFINED;
    if (argc >= 2 && JS_IsObject(argv[1])) {
        methods_obj = JS_DupValue(ctx, argv[1]);
    }
    
    JSValue func_data[2] = { def_holder, methods_obj };
    /* Use JS_NewCFunctionData with constructor flag */
    JSValue constructor = JS_NewCFunctionData(ctx, js_struct_constructor, 0, 0, 2, func_data);
    
    /* Free our method reference since func_data now owns it */
    JS_FreeValue(ctx, methods_obj);
    
    /* Mark as constructor by setting the constructor bit */
    JS_SetConstructorBit(ctx, constructor, 1);
    
    /* Add size property */
    JS_DefinePropertyValueStr(ctx, constructor, "size", 
                              JS_NewInt32(ctx, def->size), JS_PROP_ENUMERABLE);
    
    /* Add _structDef property to allow extraction of NativeStructDef pointer */
    /* This is used by Native.compile to map struct types to their definitions */
    JS_DefinePropertyValueStr(ctx, constructor, "_structDef",
                              JS_NewInt64(ctx, (int64_t)(uintptr_t)def), 0);

    /* StructType.array(N) - allocate N contiguous instances (see
     * js_struct_array_factory). Reuses def_holder the same way the
     * constructor itself does; JS_NewCFunctionData dups its own ref. */
    JSValue array_func_data[1] = { def_holder };
    JSValue array_fn = JS_NewCFunctionData(ctx, (JSCFunctionData *)js_struct_array_factory,
                                            1, 0, 1, array_func_data);
    JS_DefinePropertyValueStr(ctx, constructor, "array", array_fn, JS_PROP_ENUMERABLE);

    /* Free our reference to def_holder - JS_NewCFunctionData has its own ref */
    JS_FreeValue(ctx, def_holder);
    
    return constructor;
}

/* Module function list */
/* ============================================
 * Dynamic array JS bindings
 *
 * Compiled functions that take a Dynamic{Int32,Uint32,Float32}Array argument
 * expect a raw pointer to a NativeDynamicArray passed as an integer. These
 * helpers let JS allocate such an array, read it back, and free it so the
 * dynamic-array API can be exercised end to end.
 * ============================================ */

static NativeType dyn_array_type_from_name(const char *name) {
    if (!name) return NATIVE_TYPE_UNKNOWN;
    if (strcmp(name, "int") == 0 || strcmp(name, "int32") == 0)
        return NATIVE_TYPE_INT32;
    if (strcmp(name, "uint") == 0 || strcmp(name, "uint32") == 0)
        return NATIVE_TYPE_UINT32;
    if (strcmp(name, "float") == 0 || strcmp(name, "float32") == 0)
        return NATIVE_TYPE_FLOAT32;
    return NATIVE_TYPE_UNKNOWN;
}

/* Native.createDynamicArray(type, capacity=16) -> integer pointer handle */
static JSValue js_native_create_dyn_array(JSContext *ctx, JSValue this_val,
                                          int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1)
        return JS_ThrowSyntaxError(ctx, "createDynamicArray requires a type");

    const char *type_str = JS_ToCString(ctx, argv[0]);
    if (!type_str)
        return JS_EXCEPTION;

    NativeType elem = dyn_array_type_from_name(type_str);
    JS_FreeCString(ctx, type_str);
    if (elem == NATIVE_TYPE_UNKNOWN)
        return JS_ThrowTypeError(ctx, "Unsupported dynamic array type (use int/uint/float)");

    uint32_t capacity = 16;
    if (argc >= 2)
        JS_ToUint32(ctx, &capacity, argv[1]);

    NativeDynamicArray *arr = native_array_new(elem, capacity);
    if (!arr)
        return JS_ThrowOutOfMemory(ctx);

    return JS_NewUint32(ctx, (uint32_t)(uintptr_t)arr);
}

static NativeDynamicArray *dyn_array_from_arg(JSContext *ctx, JSValueConst val) {
    uint32_t handle;
    if (JS_ToUint32(ctx, &handle, val) != 0)
        return NULL;
    return (NativeDynamicArray *)(uintptr_t)handle;
}

/* Native.dynArrayLength(ptr) -> int */
static JSValue js_native_dyn_array_length(JSContext *ctx, JSValue this_val,
                                          int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1)
        return JS_ThrowSyntaxError(ctx, "dynArrayLength requires a pointer");
    NativeDynamicArray *arr = dyn_array_from_arg(ctx, argv[0]);
    if (!arr)
        return JS_ThrowTypeError(ctx, "Invalid dynamic array pointer");
    return JS_NewInt32(ctx, (int32_t)arr->length);
}

/* Native.dynArrayGet(ptr, index) -> number */
static JSValue js_native_dyn_array_get(JSContext *ctx, JSValue this_val,
                                       int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 2)
        return JS_ThrowSyntaxError(ctx, "dynArrayGet requires (pointer, index)");
    NativeDynamicArray *arr = dyn_array_from_arg(ctx, argv[0]);
    if (!arr)
        return JS_ThrowTypeError(ctx, "Invalid dynamic array pointer");
    uint32_t index;
    if (JS_ToUint32(ctx, &index, argv[1]) != 0)
        return JS_EXCEPTION;
    if (index >= arr->length)
        return JS_ThrowRangeError(ctx, "Index %u out of bounds (length %u)",
                                  index, (uint32_t)arr->length);

    switch (arr->element_type) {
        case NATIVE_TYPE_INT32:
            return JS_NewInt32(ctx, ((int32_t *)arr->data)[index]);
        case NATIVE_TYPE_UINT32:
            return JS_NewUint32(ctx, ((uint32_t *)arr->data)[index]);
        case NATIVE_TYPE_FLOAT32:
            return JS_NewFloat64(ctx, (double)((float *)arr->data)[index]);
        default:
            return JS_ThrowTypeError(ctx, "Unsupported element type");
    }
}

/* Native.dynArrayFree(ptr) -> void */
static JSValue js_native_dyn_array_free(JSContext *ctx, JSValue this_val,
                                        int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1)
        return JS_ThrowSyntaxError(ctx, "dynArrayFree requires a pointer");
    NativeDynamicArray *arr = dyn_array_from_arg(ctx, argv[0]);
    if (arr)
        native_array_free(arr);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("compile", 2, js_native_compile),
    JS_CFUNC_DEF("isSupported", 0, js_native_is_supported),
    JS_CFUNC_DEF("free", 1, js_native_free),
    JS_CFUNC_DEF("getInfo", 1, js_native_get_info),
    JS_CFUNC_DEF("benchmark", 2, js_native_benchmark),
    JS_CFUNC_DEF("disassemble", 0, js_native_disassemble),
    JS_CFUNC_DEF("struct", 1, js_native_struct),
    JS_CFUNC_DEF("createDynamicArray", 2, js_native_create_dyn_array),
    JS_CFUNC_DEF("dynArrayLength", 1, js_native_dyn_array_length),
    JS_CFUNC_DEF("dynArrayGet", 2, js_native_dyn_array_get),
    JS_CFUNC_DEF("dynArrayFree", 1, js_native_dyn_array_free),
};

static int module_init(JSContext *ctx, JSModuleDef *m) {
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

/* Module initialization - called from ath_env.c */
JSModuleDef *athena_native_init(JSContext *ctx) {
    return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Native");
}

/* Cleanup function - should be called on shutdown */
void athena_native_cleanup(void) {
    athena_native_compiler_cleanup();
}

#ifdef DYNAMIC_ATHENA_NATIVE
char *erl_id = "athena_native";

static int erl_module_init(JSContext *ctx, JSModuleDef *m) {
    if (JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs)) != 0)
        return -1;

    JSValue native_obj = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, native_obj, module_funcs, countof(module_funcs));

    JSValue global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, "Native", JS_DupValue(ctx, native_obj));
    JS_FreeValue(ctx, global_obj);

    JS_SetModuleExport(ctx, m, "Native", native_obj);
    return 0;
}

JSModuleDef *js_init_module(JSContext *ctx) {
    JSModuleDef *m = athena_push_module(ctx, erl_module_init, module_funcs, countof(module_funcs), "Native");
    if (m)
        JS_AddModuleExport(ctx, m, "Native");
    return m;
}
#endif

