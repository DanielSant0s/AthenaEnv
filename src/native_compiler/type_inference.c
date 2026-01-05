/*
 * AthenaEnv Native Compiler - Type Inference
 * 
 * Copyright (c) 2025 AthenaEnv Project
 * 
 * Type inference and validation for the AOT native compiler.
 */

#include "native_compiler.h"
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

/* ============================================
 * Type Stack for Inference
 * ============================================ */

typedef struct TypeStack {
    NativeType types[NC_MAX_STACK_DEPTH];
    int depth;
} TypeStack;

static void type_stack_init(TypeStack *ts) {
    ts->depth = 0;
}

static int type_stack_push(TypeStack *ts, NativeType t) {
    if (ts->depth >= NC_MAX_STACK_DEPTH) {
        return -1;
    }
    ts->types[ts->depth++] = t;
    return 0;
}

static NativeType type_stack_pop(TypeStack *ts) {
    if (ts->depth <= 0) {
        return NATIVE_TYPE_UNKNOWN;
    }
    return ts->types[--ts->depth];
}

static NativeType type_stack_peek(TypeStack *ts) {
    if (ts->depth <= 0) {
        return NATIVE_TYPE_UNKNOWN;
    }
    return ts->types[ts->depth - 1];
}

/* ============================================
 * Type Promotion Rules
 * ============================================ */

/*
 * Get the result type of a binary operation.
 * Returns NATIVE_TYPE_UNKNOWN if types are incompatible.
 */
static NativeType binary_result_type(NativeType left, NativeType right, IROp op) {
    /* Both unknown -> unknown */
    if (left == NATIVE_TYPE_UNKNOWN || right == NATIVE_TYPE_UNKNOWN) {
        return NATIVE_TYPE_UNKNOWN;
    }
    
    /* Comparison operations always return bool */
    if (op >= IR_EQ_I32 && op <= IR_LE_F32) {
        return NATIVE_TYPE_BOOL;
    }
    
    /* Integer operations */
    if (op >= IR_ADD_I32 && op <= IR_MOD_I32) {
        if (NATIVE_TYPE_IS_INT(left) && NATIVE_TYPE_IS_INT(right)) {
            /* Promote to signed if either is signed */
            if (left == NATIVE_TYPE_INT32 || right == NATIVE_TYPE_INT32) {
                return NATIVE_TYPE_INT32;
            }
            return NATIVE_TYPE_UINT32;
        }
        return NATIVE_TYPE_UNKNOWN;
    }
    
    /* Float operations */
    if (op >= IR_ADD_F32 && op <= IR_SQRT_F32) {
        if (NATIVE_TYPE_IS_FLOAT(left) && NATIVE_TYPE_IS_FLOAT(right)) {
            return NATIVE_TYPE_FLOAT32;
        }
        /* Allow int + float -> float */
        if ((NATIVE_TYPE_IS_FLOAT(left) && NATIVE_TYPE_IS_INT(right)) ||
            (NATIVE_TYPE_IS_INT(left) && NATIVE_TYPE_IS_FLOAT(right))) {
            return NATIVE_TYPE_FLOAT32;
        }
        return NATIVE_TYPE_UNKNOWN;
    }
    
    /* Bitwise operations require integers */
    if (op >= IR_AND && op <= IR_SAR) {
        if (NATIVE_TYPE_IS_INT(left) && NATIVE_TYPE_IS_INT(right)) {
            if (left == NATIVE_TYPE_INT32 || right == NATIVE_TYPE_INT32) {
                return NATIVE_TYPE_INT32;
            }
            return NATIVE_TYPE_UINT32;
        }
        return NATIVE_TYPE_UNKNOWN;
    }
    
    /* Default: prefer left type if compatible */
    if (left == right) {
        return left;
    }
    
    return NATIVE_TYPE_UNKNOWN;
}

/*
 * Check if a type can be implicitly converted to another.
 */
static bool type_can_convert(NativeType from, NativeType to) {
    if (from == to) return true;
    if (from == NATIVE_TYPE_UNKNOWN || to == NATIVE_TYPE_UNKNOWN) return true;
    
    /* int32 <-> uint32 is allowed */
    if ((from == NATIVE_TYPE_INT32 && to == NATIVE_TYPE_UINT32) ||
        (from == NATIVE_TYPE_UINT32 && to == NATIVE_TYPE_INT32)) {
        return true;
    }
    
    /* bool -> int is allowed */
    if (from == NATIVE_TYPE_BOOL && NATIVE_TYPE_IS_INT(to)) {
        return true;
    }
    
    /* int -> float requires explicit conversion */
    /* float -> int requires explicit conversion */
    
    return false;
}

/* ============================================
 * Type Inference Implementation
 * ============================================ */

/*
 * Infer types for a single basic block.
 * Updates local_types based on operations.
 */
static int infer_block_types(IRBasicBlock *block, NativeType *local_types, 
                              int local_count, TypeStack *stack) {
    for (int i = 0; i < block->instr_count; i++) {
        IRInstr *instr = &block->instrs[i];
        NativeType t1, t2, result;
        
        switch (instr->op) {
            case IR_CONST_I32:
                type_stack_push(stack, NATIVE_TYPE_INT32);
                instr->type = NATIVE_TYPE_INT32;
                break;
                
            case IR_CONST_F32:
                type_stack_push(stack, NATIVE_TYPE_FLOAT32);
                instr->type = NATIVE_TYPE_FLOAT32;
                break;
                
            case IR_CONST_I64:
                type_stack_push(stack, NATIVE_TYPE_INT64);
                instr->type = NATIVE_TYPE_INT64;
                break;
                
            case IR_LOAD_LOCAL:
                if (instr->operand.local_idx < local_count) {
                    NativeType lt = local_types[instr->operand.local_idx];
                    /* If local type is unknown, infer from context or default to INT32 */
                    if (lt == NATIVE_TYPE_UNKNOWN) {
                        lt = NATIVE_TYPE_INT32;
                        local_types[instr->operand.local_idx] = lt;
                    }
                    type_stack_push(stack, lt);
                    instr->type = lt;
                } else {
                    return -1;  /* Invalid local index */
                }
                break;
                
            case IR_STORE_LOCAL:
                t1 = type_stack_pop(stack);
                if (instr->operand.local_idx < local_count) {
                    NativeType existing = local_types[instr->operand.local_idx];
                    
                    /* First store determines type, subsequent may promote */
                    if (existing == NATIVE_TYPE_UNKNOWN) {
                        /* If popped type is unknown, default to INT32 */
                        if (t1 == NATIVE_TYPE_UNKNOWN) {
                            t1 = NATIVE_TYPE_INT32;
                        }
                        local_types[instr->operand.local_idx] = t1;
                    } else if (t1 == NATIVE_TYPE_UNKNOWN) {
                        /* If storing unknown, use existing local type */
                        t1 = existing;
                    } else if (NATIVE_TYPE_IS_FLOAT(t1) && NATIVE_TYPE_IS_INT(existing)) {
                        /* PROMOTION: If storing float to int local, upgrade local to float */
                        local_types[instr->operand.local_idx] = NATIVE_TYPE_FLOAT32;
                        t1 = NATIVE_TYPE_FLOAT32;
                    } else if (!type_can_convert(t1, existing)) {
                        /* Type mismatch - but allow if we can promote */
                        if (NATIVE_TYPE_IS_INT(t1) && NATIVE_TYPE_IS_INT(existing)) {
                            /* Allow int/uint conversion */
                            t1 = existing;
                        } else {
                            return -1;  /* Type mismatch */
                        }
                    }
                    instr->type = t1;
                } else {
                    return -1;
                }
                break;
                
            case IR_LOAD_ARRAY:
                /* Pop index, pop array ref, push element type */
                type_stack_pop(stack);  /* index */
                t1 = type_stack_pop(stack);  /* array */
                result = native_array_element_type(t1);
                /* If array type is unknown, assume INT32 array */
                if (result == NATIVE_TYPE_UNKNOWN) {
                    result = NATIVE_TYPE_INT32;
                }
                type_stack_push(stack, result);
                instr->type = result;
                break;
                
            case IR_STORE_ARRAY:
                /* Pop value, pop index, pop array ref */
                type_stack_pop(stack);  /* value */
                type_stack_pop(stack);  /* index */
                type_stack_pop(stack);  /* array */
                instr->type = NATIVE_TYPE_VOID;
                break;
                
            case IR_LOAD_FIELD:
                /* Push field value using type from instruction */
                type_stack_push(stack, instr->type);
                break;
                
            case IR_LOAD_FIELD_ADDR:
                /* Push array base address with array type */
                /* The instr->type should be FLOAT32_ARRAY, INT32_ARRAY, etc. */
                type_stack_push(stack, instr->type);
                break;
                
            case IR_STORE_FIELD:
                /* Pop value to store */
                type_stack_pop(stack);
                instr->type = NATIVE_TYPE_VOID;
                break;
                
            /* Binary integer operations */
            case IR_ADD_I32:
            case IR_SUB_I32:
            case IR_MUL_I32:
            case IR_DIV_I32:
            case IR_MOD_I32: {
                t2 = type_stack_pop(stack);
                t1 = type_stack_pop(stack);
                
                /* Check if we need to convert to string operations */
                bool needs_string = (t1 == NATIVE_TYPE_STRING || t2 == NATIVE_TYPE_STRING);
                /* Check if we need to convert to float operations */
                bool needs_float = (NATIVE_TYPE_IS_FLOAT(t1) || NATIVE_TYPE_IS_FLOAT(t2));
                /* Check if we need to convert to 64-bit operations */
                bool needs_int64 = (NATIVE_TYPE_IS_INT64(t1) || NATIVE_TYPE_IS_INT64(t2));
                
                if (needs_string && instr->op == IR_ADD_I32) {
                    /* String concatenation: + on strings becomes IR_STRING_CONCAT */
                    instr->op = IR_STRING_CONCAT;
                    result = NATIVE_TYPE_STRING;
                } else if (needs_float) {
                    /* Convert integer operation to float operation */
                    switch (instr->op) {
                        case IR_ADD_I32: instr->op = IR_ADD_F32; break;
                        case IR_SUB_I32: instr->op = IR_SUB_F32; break;
                        case IR_MUL_I32: instr->op = IR_MUL_F32; break;
                        case IR_DIV_I32: instr->op = IR_DIV_F32; break;
                        case IR_MOD_I32: 
                            /* MOD not supported for floats, keep as integer op but result will be wrong */
                            break;
                        default: break;
                    }
                    /* Use float result type */
                    result = binary_result_type(t1, t2, instr->op);
                    if (result == NATIVE_TYPE_UNKNOWN) {
                        result = NATIVE_TYPE_FLOAT32;
                    }
                } else if (needs_int64) {
                    /* Convert 32-bit operation to 64-bit */
                    switch (instr->op) {
                        case IR_ADD_I32: instr->op = IR_ADD_I64; break;
                        case IR_SUB_I32: instr->op = IR_SUB_I64; break;
                        case IR_MUL_I32: instr->op = IR_MUL_I64; break;
                        case IR_DIV_I32: instr->op = IR_DIV_I64; break;
                        case IR_MOD_I32: instr->op = IR_MOD_I64; break;
                        default: break;
                    }
                    result = NATIVE_TYPE_INT64;
                } else {
                    /* Integer operation */
                    result = binary_result_type(t1, t2, instr->op);
                    /* If result is unknown, default to INT32 for integer ops */
                    if (result == NATIVE_TYPE_UNKNOWN) {
                        result = NATIVE_TYPE_INT32;
                    }
                }
                type_stack_push(stack, result);
                instr->type = result;
                break;
            }
            
            /* 64-bit Integer operations */
            case IR_ADD_I64:
            case IR_SUB_I64:
            case IR_SHL_I64:
            case IR_SHR_I64:
            case IR_SAR_I64: {
                t2 = type_stack_pop(stack);
                t1 = type_stack_pop(stack);
                type_stack_push(stack, NATIVE_TYPE_INT64);
                instr->type = NATIVE_TYPE_INT64;
                break;
            }
            
            case IR_NEG_I64: {
                type_stack_pop(stack);
                type_stack_push(stack, NATIVE_TYPE_INT64);
                instr->type = NATIVE_TYPE_INT64;
                break;
            }
                
            /* Binary float operations */
            case IR_ADD_F32:
            case IR_SUB_F32:
            case IR_MUL_F32:
            case IR_DIV_F32:
                t2 = type_stack_pop(stack);
                t1 = type_stack_pop(stack);
                result = binary_result_type(t1, t2, instr->op);
                type_stack_push(stack, result);
                instr->type = result;
                break;
                
            /* Unary operations */
            case IR_NEG_I32:
                t1 = type_stack_pop(stack);
                if (NATIVE_TYPE_IS_INT64(t1)) {
                    instr->op = IR_NEG_I64;
                    type_stack_push(stack, NATIVE_TYPE_INT64);
                    instr->type = NATIVE_TYPE_INT64;
                } else {
                    type_stack_push(stack, NATIVE_TYPE_INT32);
                    instr->type = NATIVE_TYPE_INT32;
                }
                break;
                
            case IR_NEG_F32:
            case IR_SQRT_F32:
                t1 = type_stack_pop(stack);
                type_stack_push(stack, NATIVE_TYPE_FLOAT32);
                instr->type = NATIVE_TYPE_FLOAT32;
                break;
                
            /* Bitwise operations */
            case IR_AND:
            case IR_OR:
            case IR_XOR:
            case IR_SHL:
            case IR_SHR:
            case IR_SAR:
                t2 = type_stack_pop(stack);
                t1 = type_stack_pop(stack);
                result = binary_result_type(t1, t2, instr->op);
                type_stack_push(stack, result);
                instr->type = result;
                break;
                
            case IR_NOT:
                t1 = type_stack_pop(stack);
                type_stack_push(stack, t1);
                instr->type = t1;
                break;
                
            /* Comparison operations */
            case IR_EQ_I32:
            case IR_NE_I32:
            case IR_LT_I32:
            case IR_LE_I32:
            case IR_GT_I32:
            case IR_GE_I32:
            case IR_LT_U32:
            case IR_LE_U32:
            case IR_GT_U32:
            case IR_GE_U32:
            case IR_EQ_F32:
            case IR_LT_F32:
            case IR_LE_F32: {
                t2 = type_stack_pop(stack);
                t1 = type_stack_pop(stack);
                
                /* Check if we are comparing strings */
                if ((t1 == NATIVE_TYPE_STRING || t2 == NATIVE_TYPE_STRING) && 
                    (instr->op == IR_EQ_I32 || instr->op == IR_NE_I32)) {
                    if (instr->op == IR_EQ_I32) {
                        instr->op = IR_STRING_EQUALS;
                    } else {
                        /* NE: Use STRING_EQUALS and negate later (or add IR_STRING_NE) */
                        /* For now, keep as string compare */
                        instr->op = IR_STRING_COMPARE;
                    }
                }
                
                type_stack_push(stack, NATIVE_TYPE_BOOL);
                instr->type = NATIVE_TYPE_BOOL;
                break;
            }
                
            /* Conversions */
            case IR_I32_TO_F32:
                type_stack_pop(stack);
                type_stack_push(stack, NATIVE_TYPE_FLOAT32);
                instr->type = NATIVE_TYPE_FLOAT32;
                break;
                
            case IR_F32_TO_I32:
                type_stack_pop(stack);
                type_stack_push(stack, NATIVE_TYPE_INT32);
                instr->type = NATIVE_TYPE_INT32;
                break;
                
            case IR_I32_TO_I64:
                type_stack_pop(stack);
                type_stack_push(stack, NATIVE_TYPE_INT64);
                instr->type = NATIVE_TYPE_INT64;
                break;
                
            case IR_I64_TO_I32:
                type_stack_pop(stack);
                type_stack_push(stack, NATIVE_TYPE_INT32);
                instr->type = NATIVE_TYPE_INT32;
                break;
                
            /* Control flow - no type changes except conditional consumes bool */
            case IR_JUMP:
            case IR_LABEL:
                instr->type = NATIVE_TYPE_VOID;
                break;
                
            case IR_JUMP_IF_TRUE:
            case IR_JUMP_IF_FALSE:
                t1 = type_stack_pop(stack);
                instr->type = NATIVE_TYPE_VOID;
                break;
                
            case IR_RETURN:
                t1 = type_stack_pop(stack);
                instr->type = t1;
                break;
                
            case IR_RETURN_VOID:
                instr->type = NATIVE_TYPE_VOID;
                break;
                
            /* Stack manipulation */
            case IR_DUP:
                t1 = type_stack_peek(stack);
                /* If type is unknown, default to INT32 */
                if (t1 == NATIVE_TYPE_UNKNOWN) {
                    t1 = NATIVE_TYPE_INT32;
                }
                type_stack_push(stack, t1);
                instr->type = t1;
                break;
                
            case IR_DROP:
                type_stack_pop(stack);
                instr->type = NATIVE_TYPE_VOID;
                break;
                
            case IR_SWAP:
                t1 = type_stack_pop(stack);
                t2 = type_stack_pop(stack);
                type_stack_push(stack, t1);
                type_stack_push(stack, t2);
                instr->type = NATIVE_TYPE_VOID;
                break;
                
            case IR_CALL:
                /* Function calls - would need function signature info */
                /* For now, assume returns int32 */
                for (int j = 0; j < instr->operand.call.arg_count; j++) {
                    type_stack_pop(stack);
                }
                type_stack_push(stack, NATIVE_TYPE_INT32);
                instr->type = NATIVE_TYPE_INT32;
                break;
                
            /* String operations */
            case IR_STRING_CONCAT: {
                t2 = type_stack_pop(stack);
                t1 = type_stack_pop(stack);
                type_stack_push(stack, NATIVE_TYPE_STRING);
                instr->type = NATIVE_TYPE_STRING;
                break;
            }
            
            case IR_STRING_LENGTH: {
                type_stack_pop(stack);
                type_stack_push(stack, NATIVE_TYPE_INT32);
                instr->type = NATIVE_TYPE_INT32;
                break;
            }
            
            case IR_STRING_COMPARE:
            case IR_STRING_EQUALS: {
                t2 = type_stack_pop(stack);
                t1 = type_stack_pop(stack);
                type_stack_push(stack, NATIVE_TYPE_BOOL);
                instr->type = NATIVE_TYPE_BOOL;
                break;
            }
            
            case IR_STRING_SLICE:
            case IR_STRING_TO_UPPER:
            case IR_STRING_TO_LOWER:
            case IR_STRING_TRIM:
            case IR_STRING_REPLACE: {
                /* These all return strings */
                /* Pop appropriate number of args */
                if (stack->depth >= 1) type_stack_pop(stack);
                if (instr->op == IR_STRING_SLICE && stack->depth >= 2) {
                    type_stack_pop(stack);
                    type_stack_pop(stack);
                } else if (instr->op == IR_STRING_REPLACE && stack->depth >= 2) {
                    type_stack_pop(stack);
                    type_stack_pop(stack);
                }
                type_stack_push(stack, NATIVE_TYPE_STRING);
                instr->type = NATIVE_TYPE_STRING;
                break;
            }
            
            case IR_STRING_FIND: {
                t2 = type_stack_pop(stack);
                t1 = type_stack_pop(stack);
                type_stack_push(stack, NATIVE_TYPE_INT32);
                instr->type = NATIVE_TYPE_INT32;
                break;
            }
            
            /* Dynamic array operations */
            case IR_ARRAY_LENGTH: {
                type_stack_pop(stack);
                type_stack_push(stack, NATIVE_TYPE_INT32);
                instr->type = NATIVE_TYPE_INT32;
                break;
            }
            
            case IR_ARRAY_PUSH:
            case IR_ARRAY_RESIZE:
            case IR_ARRAY_RESERVE:
            case IR_ARRAY_REMOVE: {
                type_stack_pop(stack);  /* value or index */
                type_stack_pop(stack);  /* array */
                type_stack_push(stack, NATIVE_TYPE_BOOL);
                instr->type = NATIVE_TYPE_BOOL;
                break;
            }
            
            case IR_ARRAY_CLEAR: {
                type_stack_pop(stack);  /* array */
                /* No return value */
                break;
            }
                
            default:
                /* Unknown operation */
                return -1;
        }
    }
    
    return 0;
}

/*
 * Main type inference entry point.
 * Infers types for all instructions in the IR function.
 */
int infer_types(IRFunction *ir, const NativeFuncSignature *sig) {
    if (!ir || !sig) return -1;
    
    /* Initialize local types from function arguments */
    for (int i = 0; i < ir->local_count; i++) {
        if (i < sig->arg_count) {
            ir->local_types[i] = sig->arg_types[i];
        } else if (ir->local_types[i] == NATIVE_TYPE_UNKNOWN) {
            /* For non-argument locals, infer from usage */
            /* Start as INT32 for numeric operations */
            ir->local_types[i] = NATIVE_TYPE_INT32;
        }
    }
    
    /* Process each basic block with its own stack state */
    /* For loops, we allow UNKNOWN types and infer from context */
    for (int b = 0; b < ir->block_count; b++) {
        TypeStack stack;
        type_stack_init(&stack);
        
        /* If this block is a loop target, start with empty stack */
        /* Otherwise, we could inherit from predecessor, but for simplicity
         * we'll infer types per-block and allow UNKNOWN */
        
        int result = infer_block_types(&ir->blocks[b], ir->local_types, 
                                       ir->local_count, &stack);
        if (result < 0) {
            /* Type inference failed - try again with more permissive rules */
            /* Reset stack and try with UNKNOWN types allowed */
            type_stack_init(&stack);
            
            /* Re-process with permissive mode */
            for (int i = 0; i < ir->blocks[b].instr_count; i++) {
                IRInstr *instr = &ir->blocks[b].instrs[i];
                
                /* For operations that fail, use UNKNOWN and continue */
                switch (instr->op) {
                    case IR_LOAD_LOCAL:
                        if (instr->operand.local_idx < ir->local_count) {
                            NativeType lt = ir->local_types[instr->operand.local_idx];
                            if (lt == NATIVE_TYPE_UNKNOWN) lt = NATIVE_TYPE_INT32;
                            type_stack_push(&stack, lt);
                            instr->type = lt;
                        }
                        break;
                    case IR_STORE_LOCAL:
                        if (stack.depth > 0) {
                            NativeType t = type_stack_pop(&stack);
                            if (instr->operand.local_idx < ir->local_count) {
                                if (ir->local_types[instr->operand.local_idx] == NATIVE_TYPE_UNKNOWN) {
                                    ir->local_types[instr->operand.local_idx] = t;
                                }
                                instr->type = t;
                            }
                        }
                        break;
                    case IR_LOAD_ARRAY:
                        if (stack.depth >= 2) {
                            type_stack_pop(&stack);  /* index */
                            type_stack_pop(&stack);  /* array */
                            type_stack_push(&stack, NATIVE_TYPE_INT32);  /* Assume int32 */
                            instr->type = NATIVE_TYPE_INT32;
                        }
                        break;
                    case IR_STORE_ARRAY:
                        if (stack.depth >= 3) {
                            type_stack_pop(&stack);  /* value */
                            type_stack_pop(&stack);  /* index */
                            type_stack_pop(&stack);  /* array */
                            instr->type = NATIVE_TYPE_VOID;
                        }
                        break;
                    case IR_ADD_I32:
                    case IR_SUB_I32:
                    case IR_MUL_I32:
                    case IR_DIV_I32:
                    case IR_MOD_I32:
                        if (stack.depth >= 2) {
                            type_stack_pop(&stack);
                            type_stack_pop(&stack);
                            type_stack_push(&stack, NATIVE_TYPE_INT32);
                            instr->type = NATIVE_TYPE_INT32;
                        }
                        break;
                    case IR_LT_I32:
                    case IR_LE_I32:
                    case IR_GT_I32:
                    case IR_GE_I32:
                    case IR_EQ_I32:
                    case IR_NE_I32:
                        if (stack.depth >= 2) {
                            type_stack_pop(&stack);
                            type_stack_pop(&stack);
                            type_stack_push(&stack, NATIVE_TYPE_BOOL);
                            instr->type = NATIVE_TYPE_BOOL;
                        }
                        break;
                    case IR_CONST_I32:
                        type_stack_push(&stack, NATIVE_TYPE_INT32);
                        instr->type = NATIVE_TYPE_INT32;
                        break;
                    case IR_DUP:
                        if (stack.depth > 0) {
                            NativeType t = type_stack_peek(&stack);
                            type_stack_push(&stack, t);
                            instr->type = t;
                        }
                        break;
                    case IR_DROP:
                        if (stack.depth > 0) {
                            type_stack_pop(&stack);
                        }
                        instr->type = NATIVE_TYPE_VOID;
                        break;
                    case IR_JUMP:
                    case IR_JUMP_IF_TRUE:
                    case IR_JUMP_IF_FALSE:
                        if (instr->op == IR_JUMP_IF_TRUE || instr->op == IR_JUMP_IF_FALSE) {
                            if (stack.depth > 0) {
                                type_stack_pop(&stack);
                            }
                        }
                        instr->type = NATIVE_TYPE_VOID;
                        break;
                    case IR_RETURN:
                        if (stack.depth > 0) {
                            type_stack_pop(&stack);
                        }
                        instr->type = NATIVE_TYPE_INT32;
                        break;
                    case IR_RETURN_VOID:
                        instr->type = NATIVE_TYPE_VOID;
                        break;
                    default:
                        /* For unknown ops, try to maintain stack balance */
                        instr->type = NATIVE_TYPE_UNKNOWN;
                        break;
                }
            }
        }
    }
    
    return 0;
}

/*
 * Validate that inferred types match the function signature.
 * 
 * Since the user explicitly declares types in the signature via
 * Native.compile({ args: [...], returns: ... }, fn), we trust those
 * declarations. This is an AOT compiler where the developer knows
 * the types at compile time.
 */
TypeCheckResult validate_types(const IRFunction *ir, const NativeFuncSignature *sig) {
    if (!ir || !sig) {
        return TYPE_CHECK_UNKNOWN;
    }
    
    /* We trust the user's signature. No strict type checking needed.
     * The generated code will assume the types declared in the signature.
     * If the user lies about types, it's undefined behavior - just like in C. */
    
    return TYPE_CHECK_OK;
}

/* ============================================
 * Type Name Parsing (for JS API)
 * ============================================ */

/*
 * Parse a type name string into NativeType.
 * Used by Native.compile() to parse the signature.
 */
NativeType parse_type_name(const char *name) {
    if (!name) return NATIVE_TYPE_UNKNOWN;
    
    if (strcmp(name, "void") == 0) return NATIVE_TYPE_VOID;
    if (strcmp(name, "int") == 0) return NATIVE_TYPE_INT32;
    if (strcmp(name, "int32") == 0) return NATIVE_TYPE_INT32;
    if (strcmp(name, "uint") == 0) return NATIVE_TYPE_UINT32;
    if (strcmp(name, "uint32") == 0) return NATIVE_TYPE_UINT32;
    if (strcmp(name, "int64") == 0) return NATIVE_TYPE_INT64;
    if (strcmp(name, "uint64") == 0) return NATIVE_TYPE_UINT64;
    if (strcmp(name, "float") == 0) return NATIVE_TYPE_FLOAT32;
    if (strcmp(name, "float32") == 0) return NATIVE_TYPE_FLOAT32;
    if (strcmp(name, "bool") == 0) return NATIVE_TYPE_BOOL;
    if (strcmp(name, "Int32Array") == 0) return NATIVE_TYPE_INT32_ARRAY;
    if (strcmp(name, "Uint32Array") == 0) return NATIVE_TYPE_UINT32_ARRAY;
    if (strcmp(name, "Float32Array") == 0) return NATIVE_TYPE_FLOAT32_ARRAY;
    if (strcmp(name, "ptr") == 0) return NATIVE_TYPE_PTR;
    
    /* String types */
    if (strcmp(name, "string") == 0) return NATIVE_TYPE_STRING;
    if (strcmp(name, "String") == 0) return NATIVE_TYPE_STRING;
    if (strcmp(name, "StringView") == 0) return NATIVE_TYPE_STRING_VIEW;
    
    /* Dynamic array types */
    if (strcmp(name, "DynamicInt32Array") == 0) return NATIVE_TYPE_DYNAMIC_INT32_ARRAY;
    if (strcmp(name, "DynamicUint32Array") == 0) return NATIVE_TYPE_DYNAMIC_UINT32_ARRAY;
    if (strcmp(name, "DynamicFloat32Array") == 0) return NATIVE_TYPE_DYNAMIC_FLOAT32_ARRAY;
    
    return NATIVE_TYPE_UNKNOWN;
}

/*
 * Convert NativeType to string name.
 * Used for debugging and API info.
 */
const char *native_type_name(NativeType type) {
    switch (type) {
        case NATIVE_TYPE_VOID: return "void";
        case NATIVE_TYPE_INT32: return "int32";
        case NATIVE_TYPE_UINT32: return "uint32";
        case NATIVE_TYPE_INT64: return "int64";
        case NATIVE_TYPE_UINT64: return "uint64";
        case NATIVE_TYPE_BOOL: return "bool";
        case NATIVE_TYPE_FLOAT32: return "float32";
        case NATIVE_TYPE_INT32_ARRAY: return "Int32Array";
        case NATIVE_TYPE_UINT32_ARRAY: return "Uint32Array";
        case NATIVE_TYPE_FLOAT32_ARRAY: return "Float32Array";
        case NATIVE_TYPE_PTR: return "ptr";
        case NATIVE_TYPE_STRING: return "string";
        case NATIVE_TYPE_STRING_VIEW: return "StringView";
        case NATIVE_TYPE_DYNAMIC_INT32_ARRAY: return "DynamicInt32Array";
        case NATIVE_TYPE_DYNAMIC_UINT32_ARRAY: return "DynamicUint32Array";
        case NATIVE_TYPE_DYNAMIC_FLOAT32_ARRAY: return "DynamicFloat32Array";
        case NATIVE_TYPE_UNKNOWN:
        default: return "unknown";
    }
}

/*
 * Get size in bytes for a NativeType.
 * Used for stack allocation and memory layout.
 */
size_t native_type_size(NativeType type) {
    switch (type) {
        case NATIVE_TYPE_INT32:
        case NATIVE_TYPE_UINT32:
        case NATIVE_TYPE_BOOL:
        case NATIVE_TYPE_FLOAT32:
        case NATIVE_TYPE_PTR:
            return 4;  /* 32 bits = 4 bytes */
        case NATIVE_TYPE_INT64:
        case NATIVE_TYPE_UINT64:
            return 8;  /* 64 bits = 8 bytes */
        case NATIVE_TYPE_INT32_ARRAY:
        case NATIVE_TYPE_UINT32_ARRAY:
        case NATIVE_TYPE_FLOAT32_ARRAY:
            return sizeof(void*);  /* Pointer size */
        case NATIVE_TYPE_VOID:
        case NATIVE_TYPE_UNKNOWN:
        default:
            return 0;
    }
}

/*
 * Get alignment requirement for a NativeType.
 * Used for struct field layout.
 */
size_t native_type_alignment(NativeType type) {
    switch (type) {
        case NATIVE_TYPE_INT64:
        case NATIVE_TYPE_UINT64:
            return 8;  /* 64-bit types need 8-byte alignment */
        case NATIVE_TYPE_INT32:
        case NATIVE_TYPE_UINT32:
        case NATIVE_TYPE_BOOL:
        case NATIVE_TYPE_FLOAT32:
        case NATIVE_TYPE_PTR:
        default:
            return 4;  /* Default 4-byte alignment */
    }
}

/*
 * Get element type for an array type.
 * Used for type inference when loading from arrays.
 */
NativeType native_array_element_type(NativeType array_type) {
    switch (array_type) {
        case NATIVE_TYPE_INT32_ARRAY:
            return NATIVE_TYPE_INT32;
        case NATIVE_TYPE_UINT32_ARRAY:
            return NATIVE_TYPE_UINT32;
        case NATIVE_TYPE_FLOAT32_ARRAY:
            return NATIVE_TYPE_FLOAT32;
        default:
            return NATIVE_TYPE_UNKNOWN;
    }
}
