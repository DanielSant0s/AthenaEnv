/*
 * AthenaEnv Native Compiler - JavaScript API
 * 
 * Copyright (c) 2025 AthenaEnv Project
 * 
 * Provides the Native.compile() API for compiling JavaScript functions
 * to native MIPS R5900 code on PS2.
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ath_env.h>
#include "../native_compiler/native_compiler.h"

#ifdef PS2
#include <timer.h>
#endif

/* Global compiler instance */
static NativeCompiler *g_compiler = NULL;

/* Forward declaration for type parsing */
extern NativeType parse_type_name(const char *name);

/* Forward declarations */
static JSValue athena_native_call_wrapper(JSContext *ctx, JSValueConst this_val,
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
static JSValue athena_native_compile(JSContext *ctx, JSValue this_val, 
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
        
        /* Check if arg_type is a string (primitive type) or an object (struct constructor) */
        if (JS_IsString(arg_type)) {
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
    if (!g_compiler) {
        g_compiler = (NativeCompiler *)malloc(sizeof(NativeCompiler));
        if (!g_compiler) {
            return JS_ThrowInternalError(ctx, "Failed to allocate compiler");
        }
        if (native_compiler_init(g_compiler, ctx) < 0) {
            free(g_compiler);
            g_compiler = NULL;
            return JS_ThrowInternalError(ctx, "Failed to initialize compiler");
        }

    }
    
    /* Copy struct definitions for each argument to compiler for field access resolution */
    for (int i = 0; i < args_len && i < NC_MAX_LOCALS; i++) {
        g_compiler->local_struct_defs[i] = arg_struct_defs[i];
    }
    
    /* Compile the function */
    CompileResult result = native_compile_function(g_compiler, js_func, &sig);
    
    if (!result.success) {
        return JS_ThrowInternalError(ctx, "Compilation failed: %s", 
                                     result.error_msg ? result.error_msg : "unknown error");
    }
    
    /* Create a wrapper object to hold the native function */
    NativeFunc *func = (NativeFunc *)malloc(sizeof(NativeFunc));
    if (!func) {
        return JS_ThrowInternalError(ctx, "Failed to allocate native function");
    }
    *func = result.func;
    
    /* Return the native function handle as an opaque pointer */
    JSValue handle = JS_NewUint32(ctx, (uint32_t)(uintptr_t)func);
    
    /* Create wrapper function that calls native code */
    /* Store handle for the wrapper */
    JSValue wrapper_data[1];
    wrapper_data[0] = handle;
    
    JSValue wrapper = JS_NewCFunctionData(ctx, 
        (JSCFunctionData *)athena_native_call_wrapper, 
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
static JSValue athena_native_call_wrapper(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv,
                                           int magic, JSValue *func_data) {
    /* Get native function handle from closure data */
    uint32_t handle;
    JS_ToUint32(ctx, &handle, func_data[0]);
    
    NativeFunc *func = (NativeFunc *)(uintptr_t)handle;
    
    if (!func || !func->is_valid) {
        return JS_ThrowTypeError(ctx, "Invalid native function");
    }
    
    /* Call the native function */
    return native_func_call(ctx, func, argc, argv);
}

/*
 * Native.isSupported()
 * 
 * Returns true if native compilation is supported on this platform.
 */
static JSValue athena_native_is_supported(JSContext *ctx, JSValue this_val,
                                           int argc, JSValueConst *argv) {
#ifdef PS2
    return JS_TRUE;
#else
    return JS_FALSE;
#endif
}

/*
 * Native.free(func)
 * 
 * Frees a compiled native function.
 */
static JSValue athena_native_free(JSContext *ctx, JSValue this_val,
                                   int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowSyntaxError(ctx, "Native.free requires a function handle");
    }
    
    uint32_t handle;
    if (JS_ToUint32(ctx, &handle, argv[0]) < 0) {
        return JS_ThrowTypeError(ctx, "Invalid function handle");
    }
    
    NativeFunc *func = (NativeFunc *)(uintptr_t)handle;
    if (func) {
        native_func_free(func);
        free(func);
    }
    
    return JS_UNDEFINED;
}

/*
 * Native.getInfo(func)
 * 
 * Returns information about a compiled native function.
 */
static JSValue athena_native_get_info(JSContext *ctx, JSValue this_val,
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
    
    JSValue info = JS_NewObject(ctx);
    
    JS_SetPropertyStr(ctx, info, "codeSize", JS_NewInt32(ctx, (int32_t)func->code_size));
    JS_SetPropertyStr(ctx, info, "argCount", JS_NewInt32(ctx, func->sig.arg_count));
    JS_SetPropertyStr(ctx, info, "returnType", 
                      JS_NewString(ctx, native_type_name(func->sig.return_type)));
    
    /* Add argument types array */
    JSValue args = JS_NewArray(ctx);
    for (int i = 0; i < func->sig.arg_count; i++) {
        JS_SetPropertyUint32(ctx, args, i, 
                            JS_NewString(ctx, native_type_name(func->sig.arg_types[i])));
    }
    JS_SetPropertyStr(ctx, info, "argTypes", args);
    
    return info;
}

/*
 * Native.benchmark(func, iterations)
 * 
 * Benchmarks a native function by calling it multiple times.
 * Returns execution time in milliseconds.
 */
static JSValue athena_native_benchmark(JSContext *ctx, JSValue this_val,
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
    
    /* Prepare empty arguments */
    JSValue empty_args[1] = { JS_UNDEFINED };
    
    /* Get start time */
    clock_t start = clock();
    
    /* Run iterations */
    for (int i = 0; i < iterations; i++) {
        JSValue result = JS_Call(ctx, func, JS_UNDEFINED, 0, empty_args);
        JS_FreeValue(ctx, result);
    }
    
    /* Get end time and calculate elapsed */
    clock_t end = clock();
    double elapsed_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    
    return JS_NewFloat64(ctx, elapsed_ms);
}

/*
 * Native.disassemble(func)
 * 
 * Returns a string containing MIPS assembly for a compiled native function.
 * Useful for debugging and understanding generated code.
 */
static JSValue athena_native_disassemble(JSContext *ctx, JSValue this_val,
                                          int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_ThrowSyntaxError(ctx, "Native.disassemble requires a function");
    }
    
    /* Get the function through the wrapper - extract handle from closure data */
    /* Since the user passes the wrapper function, we need to call Native.getInfo-style lookup */
    /* For now, we'll access the code through the global compiler's last function */
    
    if (!g_compiler) {
        return JS_ThrowTypeError(ctx, "No native compiler initialized");
    }
    
    /* Build disassembly string from the emitter's last compiled function */
    MipsEmitter *em = &g_compiler->emitter;
    
    if (!em->buffer.code || em->buffer.count == 0) {
        return JS_ThrowTypeError(ctx, "No compiled code available");
    }
    
    /* Build disassembly string */
    char *result = (char *)malloc(16384);  /* 16KB buffer for disassembly */
    if (!result) {
        return JS_ThrowInternalError(ctx, "Out of memory");
    }
    
    int pos = 0;
    size_t start = em->buffer.function_start;
    size_t count = em->buffer.count;
    uint32_t base_addr = (uint32_t)(uintptr_t)em->buffer.code;
    
    pos += snprintf(result + pos, 16384 - pos, 
                   "=== MIPS Disassembly ===\n");
    pos += snprintf(result + pos, 16384 - pos, 
                   "Code size: %zu instructions (%zu bytes)\n",
                   count - start, (count - start) * 4);
    pos += snprintf(result + pos, 16384 - pos, 
                   "Base address: 0x%08X\n\n", base_addr);
    
    /* Disassemble each instruction */
    for (size_t i = start; i < count && pos < 16000; i++) {
        uint32_t instr = em->buffer.code[i];
        uint32_t addr = base_addr + (i * 4);
        
        /* Decode instruction fields */
        uint32_t opcode = (instr >> 26) & 0x3F;
        uint32_t rs = (instr >> 21) & 0x1F;
        uint32_t rt = (instr >> 16) & 0x1F;
        uint32_t rd = (instr >> 11) & 0x1F;
        uint32_t shamt = (instr >> 6) & 0x1F;
        uint32_t funct = instr & 0x3F;
        int16_t imm = (int16_t)(instr & 0xFFFF);
        
        /* Register names */
        static const char *reg_names[] = {
            "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
            "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
            "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
            "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
        };
        
        char mnemonic[32] = "???";
        char operands[64] = "";
        
        /* Decode based on opcode */
        if (opcode == 0x00) {  /* SPECIAL */
            switch (funct) {
                case 0x00: strcpy(mnemonic, "SLL"); snprintf(operands, 64, "%s, %s, %d", reg_names[rd], reg_names[rt], shamt); break;
                case 0x02: strcpy(mnemonic, "SRL"); snprintf(operands, 64, "%s, %s, %d", reg_names[rd], reg_names[rt], shamt); break;
                case 0x03: strcpy(mnemonic, "SRA"); snprintf(operands, 64, "%s, %s, %d", reg_names[rd], reg_names[rt], shamt); break;
                case 0x08: strcpy(mnemonic, "JR"); snprintf(operands, 64, "%s", reg_names[rs]); break;
                case 0x09: strcpy(mnemonic, "JALR"); snprintf(operands, 64, "%s, %s", reg_names[rd], reg_names[rs]); break;
                case 0x18: strcpy(mnemonic, "MULT"); snprintf(operands, 64, "%s, %s", reg_names[rs], reg_names[rt]); break;
                case 0x12: strcpy(mnemonic, "MFLO"); snprintf(operands, 64, "%s", reg_names[rd]); break;
                case 0x21: strcpy(mnemonic, "ADDU"); snprintf(operands, 64, "%s, %s, %s", reg_names[rd], reg_names[rs], reg_names[rt]); break;
                case 0x23: strcpy(mnemonic, "SUBU"); snprintf(operands, 64, "%s, %s, %s", reg_names[rd], reg_names[rs], reg_names[rt]); break;
                case 0x24: strcpy(mnemonic, "AND"); snprintf(operands, 64, "%s, %s, %s", reg_names[rd], reg_names[rs], reg_names[rt]); break;
                case 0x25: strcpy(mnemonic, "OR"); snprintf(operands, 64, "%s, %s, %s", reg_names[rd], reg_names[rs], reg_names[rt]); break;
                case 0x26: strcpy(mnemonic, "XOR"); snprintf(operands, 64, "%s, %s, %s", reg_names[rd], reg_names[rs], reg_names[rt]); break;
                case 0x2A: strcpy(mnemonic, "SLT"); snprintf(operands, 64, "%s, %s, %s", reg_names[rd], reg_names[rs], reg_names[rt]); break;
                case 0x2B: strcpy(mnemonic, "SLTU"); snprintf(operands, 64, "%s, %s, %s", reg_names[rd], reg_names[rs], reg_names[rt]); break;
                default: snprintf(mnemonic, 32, "SPECIAL_%02X", funct); break;
            }
        } else if (opcode == 0x11) {  /* COP1 (FPU) */
            uint32_t fmt = rs;
            uint32_t ft = rt;
            uint32_t fs = (instr >> 11) & 0x1F;
            uint32_t fd = (instr >> 6) & 0x1F;
            uint32_t fn = instr & 0x3F;
            
            if (fmt == 0x10) {  /* FMT_S */
                switch (fn) {
                    case 0x00: strcpy(mnemonic, "ADD.S"); snprintf(operands, 64, "$f%d, $f%d, $f%d", fd, fs, ft); break;
                    case 0x01: strcpy(mnemonic, "SUB.S"); snprintf(operands, 64, "$f%d, $f%d, $f%d", fd, fs, ft); break;
                    case 0x02: strcpy(mnemonic, "MUL.S"); snprintf(operands, 64, "$f%d, $f%d, $f%d", fd, fs, ft); break;
                    case 0x03: strcpy(mnemonic, "DIV.S"); snprintf(operands, 64, "$f%d, $f%d, $f%d", fd, fs, ft); break;
                    case 0x04: strcpy(mnemonic, "SQRT.S"); snprintf(operands, 64, "$f%d, $f%d", fd, fs); break;
                    case 0x05: strcpy(mnemonic, "ABS.S"); snprintf(operands, 64, "$f%d, $f%d", fd, fs); break;
                    case 0x06: strcpy(mnemonic, "MOV.S"); snprintf(operands, 64, "$f%d, $f%d", fd, fs); break;
                    case 0x28: strcpy(mnemonic, "MAX.S"); snprintf(operands, 64, "$f%d, $f%d, $f%d", fd, fs, ft); break;
                    case 0x29: strcpy(mnemonic, "MIN.S"); snprintf(operands, 64, "$f%d, $f%d, $f%d", fd, fs, ft); break;
                    case 0x32: strcpy(mnemonic, "C.EQ.S"); snprintf(operands, 64, "$f%d, $f%d", fs, ft); break;
                    case 0x34: strcpy(mnemonic, "C.LT.S"); snprintf(operands, 64, "$f%d, $f%d", fs, ft); break;
                    default: snprintf(mnemonic, 32, "FPU_%02X", fn); break;
                }
            } else if (fmt == 0x04) {  /* MTC1 */
                strcpy(mnemonic, "MTC1");
                snprintf(operands, 64, "%s, $f%d", reg_names[rt], fs);
            } else if (fmt == 0x00) {  /* MFC1 */
                strcpy(mnemonic, "MFC1");
                snprintf(operands, 64, "%s, $f%d", reg_names[rt], fs);
            } else if (fmt == 0x08) {  /* BC1 */
                strcpy(mnemonic, (rt & 1) ? "BC1T" : "BC1F");
                snprintf(operands, 64, "%d", imm);
            } else {
                snprintf(mnemonic, 32, "COP1_%02X", fmt);
            }
        } else {
            switch (opcode) {
                case 0x04: strcpy(mnemonic, "BEQ"); snprintf(operands, 64, "%s, %s, %d", reg_names[rs], reg_names[rt], imm); break;
                case 0x05: strcpy(mnemonic, "BNE"); snprintf(operands, 64, "%s, %s, %d", reg_names[rs], reg_names[rt], imm); break;
                case 0x09: strcpy(mnemonic, "ADDIU"); snprintf(operands, 64, "%s, %s, %d", reg_names[rt], reg_names[rs], imm); break;
                case 0x0C: strcpy(mnemonic, "ANDI"); snprintf(operands, 64, "%s, %s, 0x%04X", reg_names[rt], reg_names[rs], (uint16_t)imm); break;
                case 0x0D: strcpy(mnemonic, "ORI"); snprintf(operands, 64, "%s, %s, 0x%04X", reg_names[rt], reg_names[rs], (uint16_t)imm); break;
                case 0x0E: strcpy(mnemonic, "XORI"); snprintf(operands, 64, "%s, %s, 0x%04X", reg_names[rt], reg_names[rs], (uint16_t)imm); break;
                case 0x0F: strcpy(mnemonic, "LUI"); snprintf(operands, 64, "%s, 0x%04X", reg_names[rt], (uint16_t)imm); break;
                case 0x23: strcpy(mnemonic, "LW"); snprintf(operands, 64, "%s, %d(%s)", reg_names[rt], imm, reg_names[rs]); break;
                case 0x2B: strcpy(mnemonic, "SW"); snprintf(operands, 64, "%s, %d(%s)", reg_names[rt], imm, reg_names[rs]); break;
                case 0x31: strcpy(mnemonic, "LWC1"); snprintf(operands, 64, "$f%d, %d(%s)", rt, imm, reg_names[rs]); break;
                case 0x39: strcpy(mnemonic, "SWC1"); snprintf(operands, 64, "$f%d, %d(%s)", rt, imm, reg_names[rs]); break;
                default: snprintf(mnemonic, 32, "OP_%02X", opcode); break;
            }
        }
        
        pos += snprintf(result + pos, 16384 - pos, 
                       "%04zX: 0x%08X  %-8s %s\n", 
                       (i - start) * 4, instr, mnemonic, operands);
    }
    
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
                desc->value = JS_NewBigInt64(ctx, *(int64_t *)ptr);
                break;
            case NATIVE_TYPE_UINT64:
                desc->value = JS_NewBigUint64(ctx, *(uint64_t *)ptr);
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
            if (JS_ToBigInt64(ctx, &v, value) < 0) {
                int32_t v32;
                if (JS_ToInt32(ctx, &v32, value) < 0) return -1;
                v = v32;
            }
            *(int64_t *)ptr = v;
            break;
        }
        case NATIVE_TYPE_UINT64: {
            int64_t v;
            if (JS_ToBigInt64(ctx, &v, value) < 0) {
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
            return JS_NewBigInt64(ctx, *(int64_t *)ptr);
        case NATIVE_TYPE_UINT64:
            return JS_NewBigUint64(ctx, *(uint64_t *)ptr);
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
            if (JS_ToBigInt64(ctx, &v, val) < 0) {
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
    printf("[DEBUG] js_struct_constructor: methods_obj isObject=%d isUndefined=%d\n", 
           JS_IsObject(methods_obj), JS_IsUndefined(methods_obj));
    
    if (JS_IsObject(methods_obj)) {
        JSPropertyEnum *method_props = NULL;
        uint32_t method_count = 0;
        
        if (JS_GetOwnPropertyNames(ctx, &method_props, &method_count, methods_obj,
                                    JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) >= 0) {
            printf("[DEBUG] Found %u methods in methods_obj\n", method_count);
            
            for (uint32_t i = 0; i < method_count; i++) {
                JSValue method_val = JS_GetProperty(ctx, methods_obj, method_props[i].atom);
                const char *method_name = JS_AtomToCString(ctx, method_props[i].atom);
                printf("[DEBUG] Processing method: %s\n", method_name ? method_name : "???");
                
                /* Check for deferred compilation marker (has 'self' arg) */
                JSValue is_deferred = JS_GetPropertyStr(ctx, method_val, "_deferred");
                int deferred_bool = JS_ToBool(ctx, is_deferred);
                printf("[DEBUG] Method %s: _deferred=%d\n", method_name ? method_name : "???", deferred_bool);
                
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
                                printf("[DEBUG] Replacing 'self' with struct_ref, def=%p\n", (void*)def);
                                
                                JSValue struct_ref = JS_NewObject(ctx);
                                JS_SetPropertyStr(ctx, struct_ref, "_structDef", 
                                                  JS_NewInt64(ctx, (int64_t)(uintptr_t)def));
                                printf("[DEBUG] Created struct_ref with _structDef=%lld\n", (long long)(uintptr_t)def);
                                
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
                    JSValue compiled = athena_native_compile(ctx, JS_UNDEFINED, 2, compile_args);
                    printf("[DEBUG] Method %s: compilation %s\n", method_name ? method_name : "???",
                           JS_IsException(compiled) ? "FAILED" : "SUCCESS");
                    
                    JS_FreeValue(ctx, new_sig);
                    JS_FreeValue(ctx, orig_func);
                    
                    if (JS_IsException(compiled)) {
                        /* Get exception message to debug */
                        JSValue exception = JS_GetException(ctx);
                        const char *err_msg = JS_ToCString(ctx, exception);
                        printf("[DEBUG ERROR] Compilation failed for %s: %s\n",
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
                    printf("[DEBUG] Created wrapper for method %s\n", method_name ? method_name : "???");
                    
                    JS_DefinePropertyValue(ctx, obj, method_props[i].atom,
                                          wrapper, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
                    printf("[DEBUG] Bound method %s to instance\n", method_name ? method_name : "???");
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
static JSValue athena_native_struct(JSContext *ctx, JSValue this_val,
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
    
    /* Free our reference to def_holder - JS_NewCFunctionData has its own ref */
    JS_FreeValue(ctx, def_holder);
    
    return constructor;
}

/* Module function list */
static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("compile", 2, athena_native_compile),
    JS_CFUNC_DEF("isSupported", 0, athena_native_is_supported),
    JS_CFUNC_DEF("free", 1, athena_native_free),
    JS_CFUNC_DEF("getInfo", 1, athena_native_get_info),
    JS_CFUNC_DEF("benchmark", 2, athena_native_benchmark),
    JS_CFUNC_DEF("disassemble", 0, athena_native_disassemble),
    JS_CFUNC_DEF("struct", 1, athena_native_struct),
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
    if (g_compiler) {
        native_compiler_free(g_compiler);
        free(g_compiler);
        g_compiler = NULL;
    }
}

