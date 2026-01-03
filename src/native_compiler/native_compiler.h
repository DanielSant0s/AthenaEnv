/*
 * AthenaEnv Native Compiler - Main Header
 * 
 * Copyright (c) 2025 AthenaEnv Project
 * 
 * AOT compiler that translates QuickJS bytecode to native MIPS R5900 code.
 */

 #ifndef NATIVE_COMPILER_H
 #define NATIVE_COMPILER_H
 
 #include <stdint.h>
 #include <stddef.h>
 #include <stdbool.h>
 
 #include "native_types.h"
 #include "mips_emitter.h"
 #include "../quickjs/quickjs.h"
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 /*
  * Compiler configuration
  */
 #define NC_MAX_LOCALS       32      /* Maximum local variables */
 #define NC_MAX_STACK_DEPTH  64      /* Maximum operand stack depth */
 #define NC_MAX_BASIC_BLOCKS 128     /* Maximum basic blocks in IR */
 
 /*
  * Intermediate Representation (IR) opcodes
  * Simplified opcodes for easier code generation
  */
 typedef enum IROp {
    /* Constants */
    IR_CONST_I32,       /* Push int32 constant */
    IR_CONST_F32,       /* Push float32 constant */
     
     /* Local variable access */
     IR_LOAD_LOCAL,      /* Load local variable */
     IR_STORE_LOCAL,     /* Store local variable */
     
     /* Array access */
     IR_LOAD_ARRAY,      /* Load from typed array */
     IR_STORE_ARRAY,     /* Store to typed array */
     
     /* Arithmetic (integer) */
     IR_ADD_I32,
     IR_SUB_I32,
     IR_MUL_I32,
     IR_DIV_I32,
     IR_MOD_I32,
     IR_NEG_I32,
     
     /* Arithmetic (64-bit integer) */
     IR_CONST_I64,       /* Push int64 constant */
     IR_ADD_I64,         /* 64-bit add (DADD) */
     IR_SUB_I64,         /* 64-bit subtract (DSUB) */
     IR_MUL_I64,         /* 64-bit multiply (emulated __dmult_i64) */
     IR_DIV_I64,         /* 64-bit divide (emulated __ddiv_i64) */
     IR_MOD_I64,         /* 64-bit modulo (emulated __dmod_i64) */
     IR_NEG_I64,         /* 64-bit negate (DSUB from zero) */
     IR_SHL_I64,         /* 64-bit shift left (DSLL) */
     IR_SHR_I64,         /* 64-bit logical shift right (DSRL) */
     IR_SAR_I64,         /* 64-bit arithmetic shift right (DSRA) */
     
     /* Arithmetic (float) */
     IR_ADD_F32,
     IR_SUB_F32,
     IR_MUL_F32,
     IR_DIV_F32,
     IR_NEG_F32,
     IR_SQRT_F32,  /* Native MIPS SQRT.S */
     IR_ABS_F32,   /* Native MIPS ABS.S */
     IR_MIN_F32,   /* Inline min: pop 2, push min using C.LT.S */
     IR_MAX_F32,   /* Inline max: pop 2, push max using C.LT.S */
     IR_CLAMP_F32, /* Inline clamp: pop 3 (value, min, max), push clamped value */
     IR_SIGN_F32,  /* Inline sign: pop 1, push -1/0/1 based on sign */
     IR_FROUND_F32, /* Float32 round: pop 1, push float32 (noop on PS2) */
     IR_IMUL_I32,  /* 32-bit integer multiply: pop 2, push low 32 bits */
     IR_FMA_F32,   /* Fused multiply-add: pop 3 (a, b, c), push a*b+c using MADD.S */
     IR_LERP_F32,  /* Linear interpolation: pop 3 (a, b, t), push a + (b - a) * t */
     IR_SATURATE_F32, /* Clamp to [0, 1]: pop 1, push clamped value */
     IR_STEP_F32,  /* Step function: pop 2 (edge, x), push x >= edge ? 1 : 0 */
     IR_SMOOTHSTEP_F32, /* Smooth interpolation: pop 3 (e0, e1, x), push smooth result */
     IR_RSQRT_F32, /* Reciprocal sqrt: pop 1, push 1/sqrt(x) */
     
     /* String operations */
     IR_STRING_NEW,       /* Create new string from constant */
     IR_STRING_CONCAT,    /* Concatenate two strings */
     IR_STRING_SLICE,     /* Get substring [start, end) */
     IR_STRING_LENGTH,    /* Get string length */
     IR_STRING_COMPARE,   /* Compare two strings (-1, 0, 1) */
     IR_STRING_EQUALS,    /* Check string equality (bool) */
     IR_STRING_FIND,      /* Find substring (returns index or -1) */
     IR_STRING_REPLACE,   /* Replace substring */
     IR_STRING_TO_UPPER,  /* Convert to uppercase */
     IR_STRING_TO_LOWER,  /* Convert to lowercase */
     IR_STRING_TRIM,      /* Trim whitespace */
     
     /* Dynamic array operations */
     IR_ARRAY_NEW,        /* Create new dynamic array */
     IR_ARRAY_PUSH,       /* Push element to end */
     IR_ARRAY_POP,        /* Pop element from end */
     IR_ARRAY_GET,        /* Get element at index */
     IR_ARRAY_SET,        /* Set element at index */
     IR_ARRAY_INSERT,     /* Insert element at index */
     IR_ARRAY_REMOVE,     /* Remove element at index */
     IR_ARRAY_RESIZE,     /* Resize array */
     IR_ARRAY_RESERVE,    /* Reserve capacity */
     IR_ARRAY_CLEAR,      /* Clear array (length = 0) */
     IR_ARRAY_LENGTH,     /* Get array length */
     
     /* Generic C function call (for any registered native function) */
     IR_CALL_C_FUNC,
     
     /* Bitwise */
     IR_AND,
     IR_OR,
     IR_XOR,
     IR_NOT,
     IR_SHL,
     IR_SHR,
     IR_SAR,
     
     /* Comparison */
     IR_EQ_I32,
     IR_NE_I32,
     IR_LT_I32,
     IR_LE_I32,
     IR_GT_I32,
     IR_GE_I32,
     IR_LT_U32,          /* Unsigned comparisons */
     IR_LE_U32,
     IR_GT_U32,
     IR_GE_U32,
     
     IR_EQ_F32,
     IR_LT_F32,
     IR_LE_F32,
     
     /* 64-bit Comparison */
     IR_EQ_I64,
     IR_NE_I64,
     IR_LT_I64,          /* Signed comparison */
     IR_LE_I64,
     IR_GT_I64,
     IR_GE_I64,
     IR_LT_U64,          /* Unsigned comparison */
     IR_LE_U64,
     IR_GT_U64,
     IR_GE_U64,
     
     /* Conversion */
     IR_I32_TO_F32,
     IR_F32_TO_I32,
     IR_I32_TO_I64,      /* Sign-extend 32 to 64 */
     IR_I64_TO_I32,      /* Truncate 64 to 32 */
     IR_I64_TO_F32,      /* 64-bit int to float (precision loss) */
     IR_F32_TO_I64,      /* Float to 64-bit int */
     
     /* Struct field access */
     IR_LOAD_FIELD,      /* Load field: pop base ptr, push value at offset */
     IR_STORE_FIELD,     /* Store field: pop value, pop base ptr, write at offset */
     IR_LOAD_FIELD_ADDR, /* Load field address: push base + offset (for array fields) */
     
     /* Control flow */
     IR_JUMP,            /* Unconditional jump */
     IR_JUMP_IF_TRUE,    /* Conditional jump if true */
     IR_JUMP_IF_FALSE,   /* Conditional jump if false */
     IR_LABEL,           /* Label marker */
     
     /* Function */
     IR_CALL,            /* Call native function */
     IR_TAIL_CALL,       /* Tail call (reuses current stack frame) */
     IR_RETURN,          /* Return from function */
     IR_RETURN_VOID,     /* Return void */
     
     /* Stack manipulation */
     IR_DUP,
     IR_DROP,
     IR_SWAP,
     
     /* Optimized compound operations */
     IR_ADD_LOCAL_CONST, /* Add constant to local variable (i = i + n) */
     
     /* Optimization markers - keep at end */
     IR_NOP,             /* No operation - used by optimization passes */
     
     IR_OP_COUNT
 } IROp;
 
 /*
  * IR instruction
  */
 typedef struct IRInstr {
     IROp op;
     NativeType type;        /* Type of result */
     union {
         int32_t i32;        /* Integer immediate */
         int64_t i64;        /* 64-bit integer immediate */
         float f32;          /* Float immediate */
         int local_idx;      /* Local variable index */
         int label_id;       /* Label/branch target */
         struct {
            void *func_ptr;         /* Pointer to C function */
            uint8_t arg_count;      /* Number of arguments (1-8) */
            NativeType arg_types[8]; /* Argument types (N32 ABI: $f12-$f19) */
            NativeType ret_type;    /* Return type */
        } call;
         struct {
             int local_idx;
             int32_t const_val;
         } local_add;  /* For IR_ADD_LOCAL_CONST */
         struct {
             int16_t offset;      /* Byte offset from base pointer */
             int16_t local_idx;   /* Local variable holding the base pointer */
             NativeType field_type;  /* Type of the field (for load/store size) */
         } field;  /* For IR_LOAD_FIELD / IR_STORE_FIELD */
     } operand;
    
    /* Track which local this instruction loaded (for struct field access)
     * Set to the local_idx when IR_LOAD_LOCAL is emitted, -1 otherwise */
    int16_t source_local_idx;
 } IRInstr;
 
 /*
  * Basic block in the IR
  */
 typedef struct IRBasicBlock {
     IRInstr *instrs;
     int instr_count;
     int instr_capacity;
     int label_id;
     int successors[2];      /* Branch targets (-1 = none) */
 } IRBasicBlock;
 
 /*
  * IR function representation
  */
 typedef struct IRFunction {
     IRBasicBlock *blocks;
     int block_count;
     int block_capacity;
     
     NativeFuncSignature sig;
     
     /* Local variable info */
     NativeType local_types[NC_MAX_LOCALS];
     int local_count;
 } IRFunction;
 
 /*
  * Native C function registry entry
  */
 #define NC_MAX_NATIVE_FUNCS 64
 
 typedef struct NativeFuncEntry {
     const char *name;           /* Function name (e.g., "Math.sin") */
     void *func_ptr;             /* Direct pointer to C function */
     NativeFuncSignature sig;    /* Argument and return types */
 } NativeFuncEntry;
 
 /*
  * Compilation context
  */
 typedef struct NativeCompiler {
     JSContext *js_ctx;          /* QuickJS context */
     MipsEmitter emitter;        /* Code emitter */
     
     IRFunction ir_func;         /* Current IR function */
     
     /* Type inference state */
     NativeType stack_types[NC_MAX_STACK_DEPTH];
     int stack_depth;
     
    /* Register allocation */
    int local_regs[NC_MAX_LOCALS];  /* Register assigned to each local */
    int local_stack_offs[NC_MAX_LOCALS]; /* Stack offset for spilled locals */
    
    /* Code generation state */
    int stack_top;  /* Current operand stack depth (for register allocation) */
    
    /* Bytecode info (from QuickJS) */
    int js_arg_count;  /* Number of arguments from bytecode */
    int js_var_count;  /* Number of local variables from bytecode */
    
    /* Error handling */
    bool has_error;
    char error_msg[256];
    
    /* Shared labels */
    int epilogue_label;
    
    /* Optimization flags */
    bool is_leaf;  /* True if function doesn't call other functions */
    bool needs_fp; /* True if function needs frame pointer (has stack locals) */
    bool skip_epilogue_jump;  /* True if IR_RETURN should skip J to epilogue */
    uint8_t used_saved_regs;  /* Bitmask of used $s0-$s7 registers (bit 0 = $s0, etc) */
    bool has_loops;           /* True if function contains loops (backward jumps) */
    
    /* Pending intrinsic call from OP_get_field2 - avoids emitting IR markers */
    NativeFuncEntry *pending_intrinsic_entry;  /* Function entry for C calls, NULL if none */
    IROp pending_intrinsic_op;                 /* IR opcode (IR_SQRT_F32, IR_ABS_F32, or IR_CALL_C_FUNC) */
    
    /* Native C function registry */
     NativeFuncEntry native_funcs[NC_MAX_NATIVE_FUNCS];
     int native_func_count;
     
     /* Struct type tracking for field access */
     struct NativeStructDef *local_struct_defs[NC_MAX_LOCALS];  /* Struct def for each struct-typed local */
     int last_loaded_local;  /* Index of last loaded local (for getter) */
     int last_put_field_target;  /* Index of target for next put_field (not reset by getters) */
     
     /* Track which local each stack item came from for field access resolution */
     int stack_local_source[NC_MAX_STACK_DEPTH];  /* Which local does each stack item come from? -1 if unknown */
 } NativeCompiler;
 
 /*
  * Compilation result
  */
 typedef struct CompileResult {
     bool success;
     NativeFunc func;
     const char *error_msg;
 } CompileResult;
 
 /* ============================================
  * Compiler API
  * ============================================ */
 
 /* Initialize the native compiler */
 int native_compiler_init(NativeCompiler *nc, JSContext *ctx);
 
 /* Free compiler resources */
 void native_compiler_free(NativeCompiler *nc);
 
 /* Register a native C function that can be called from compiled code */
 int native_register_function(
     NativeCompiler *nc,
     const char *name,
     void *func_ptr,
     const NativeFuncSignature *sig
 );
 
 /* Look up a registered native function by name */
 NativeFuncEntry *native_lookup_function(NativeCompiler *nc, const char *name);
 
 /* Compile a JavaScript function to native code */
 CompileResult native_compile_function(
     NativeCompiler *nc,
     JSValueConst js_func,
     const NativeFuncSignature *sig
 );
 
 /* Free a compiled native function */
 void native_func_free(NativeFunc *func);
 
 /* Call a compiled native function */
 JSValue native_func_call(
     JSContext *ctx,
     NativeFunc *func,
     int argc,
     JSValueConst *argv
 );
 
 /* ============================================
  * IR Building API
  * ============================================ */
 
 /* Initialize IR function */
 void ir_func_init(IRFunction *ir);
 
 /* Free IR function */
 void ir_func_free(IRFunction *ir);
 
 /* Create a new basic block */
 int ir_create_block(IRFunction *ir);
 
 /* Add instruction to current block */
 void ir_emit(IRFunction *ir, int block_idx, const IRInstr *instr);
 
 /* Convenience emitters */
 void ir_emit_const_i32(IRFunction *ir, int block, int32_t val);
 void ir_emit_const_f32(IRFunction *ir, int block, float val);
 void ir_emit_load_local(IRFunction *ir, int block, int idx, NativeType type);
 void ir_emit_store_local(IRFunction *ir, int block, int idx, NativeType type);
 void ir_emit_binop(IRFunction *ir, int block, IROp op);
 void ir_emit_jump(IRFunction *ir, int block, int target);
 void ir_emit_jump_if(IRFunction *ir, int block, IROp op, int target);
 void ir_emit_return(IRFunction *ir, int block, NativeType type);
 
 /* ============================================
  * Bytecode to IR Translation
  * ============================================ */
 
 /* Parse QuickJS function bytecode into IR */
 int bytecode_to_ir(
     NativeCompiler *nc,
     const uint8_t *bytecode,
     size_t bytecode_len,
     IRFunction *ir_out
 );
 
 /* ============================================
  * IR to Native Code Generation
  * ============================================ */
 
 /* Generate MIPS code from IR */
 int ir_to_native(
     NativeCompiler *nc,
     const IRFunction *ir
 );
 
/* ============================================
 * Type Inference
 * ============================================ */

/* Infer types for IR function */
int infer_types(IRFunction *ir, const NativeFuncSignature *sig);

/* Validate types match signature */
TypeCheckResult validate_types(
    const IRFunction *ir,
    const NativeFuncSignature *sig
);

/* Parse a type name string into NativeType */
NativeType parse_type_name(const char *name);

/* Convert NativeType to string name */
const char *native_type_name(NativeType type);

/* Get size in bytes for a NativeType */
size_t native_type_size(NativeType type);

/* Get alignment requirement for a NativeType */
size_t native_type_alignment(NativeType type);

/* Get element type for an array type */
NativeType native_array_element_type(NativeType array_type);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_COMPILER_H */
 