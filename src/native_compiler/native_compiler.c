/*
 * AthenaEnv Native Compiler - Main Implementation
 * 
 * Copyright (c) 2025 AthenaEnv Project
 * 
 * AOT compiler that translates QuickJS bytecode to native MIPS R5900 code.
 */

#include "native_compiler.h"
#include "native_struct.h"   /* For struct field access support */
#include "int64_runtime.h"   /* For int64 emulation */
#include "native_string.h"   /* For native string operations */
#include "native_array.h"    /* For dynamic arrays */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Extern from ath_native.c for struct instance detection */
extern JSClassID js_struct_class_id;
extern JSClassID js_struct_instance_array_class_id;

#ifdef PS2
#include <kernel.h>
#endif

/* Enable SHORT_OPCODES for the short versions of common opcodes */
#define SHORT_OPCODES 1

/* Debug trace layer for the compiler pipeline.
 * Build with -DNC_TRACE=1 to get step-by-step logging of the pre-scan, the
 * bytecode->IR decode loop, register/local allocation, and the IR->MIPS emit
 * loop. Each line is flushed immediately so the last line printed before a
 * hang pinpoints the culprit. Off by default - it prints several lines per
 * compiled function, which drowns out a program's own output. */
#ifndef NC_TRACE
#define NC_TRACE 1
#endif

#if NC_TRACE
#define NC_LOG(...) do { printf("[NC] " __VA_ARGS__); printf("\n"); fflush(stdout); } while (0)
#else
#define NC_LOG(...) do {} while (0)
#endif

/* Define OP_FMT_* enum values first, like QuickJS does */
enum {
    #define FMT(f) OP_FMT_ ## f,
    #include "../quickjs/quickjs-opcode.h"
    #undef FMT
};

/* QuickJS internal opcode definitions */
/* Match QuickJS enum structure exactly: only DEF in main enum, def is ignored */
#define FMT(f)
#define DEF(id, size, n_pop, n_push, f) OP_##id,
#define def(id, size, n_pop, n_push, f)  /* def opcodes are temporary, not in main enum */
enum {
#include "../quickjs/quickjs-opcode.h"
    OP_COUNT, /* excluding temporary opcodes */
    /* temporary opcodes : overlap with the short opcodes */
    OP_TEMP_START = OP_nop + 1,
    OP___dummy = OP_TEMP_START - 1,
#define FMT(f)
#define DEF(id, size, n_pop, n_push, f)  /* DEF is ignored in temp enum */
#define def(id, size, n_pop, n_push, f) OP_##id,
#include "../quickjs/quickjs-opcode.h"
#undef def
#undef DEF
#undef FMT
    OP_TEMP_END,
};
#undef def
#undef DEF
#undef FMT

#if SHORT_OPCODES
/* After the final compilation pass, short opcodes are used. Their
   opcodes overlap with the temporary opcodes which cannot appear in
   the final bytecode. Their description is after the temporary
   opcodes in opcode_info[]. */
#define short_opcode_info(op)           \
    opcode_info[(op) >= OP_TEMP_START ? \
                (op) + (OP_TEMP_END - OP_TEMP_START) : (op)]
#else
#define short_opcode_info(op) opcode_info[op]
#endif


const char *op_to_cstr(int op)
{
    static const char *const op_names[] = {
        [OP_invalid]                = "OP_invalid",
        [OP_push_i32]               = "OP_push_i32",
        [OP_push_const]             = "OP_push_const",
        [OP_fclosure]               = "OP_fclosure",
        [OP_push_atom_value]        = "OP_push_atom_value",
        [OP_private_symbol]         = "OP_private_symbol",
        [OP_undefined]              = "OP_undefined",
        [OP_null]                   = "OP_null",
        [OP_push_this]              = "OP_push_this",
        [OP_push_false]             = "OP_push_false",
        [OP_push_true]              = "OP_push_true",
        [OP_object]                 = "OP_object",
        [OP_special_object]         = "OP_special_object",
        [OP_rest]                   = "OP_rest",
        [OP_drop]                   = "OP_drop",
        [OP_nip]                    = "OP_nip",
        [OP_nip1]                   = "OP_nip1",
        [OP_dup]                    = "OP_dup",
        [OP_dup1]                   = "OP_dup1",
        [OP_dup2]                   = "OP_dup2",
        [OP_dup3]                   = "OP_dup3",
        [OP_insert2]                = "OP_insert2",
        [OP_insert3]                = "OP_insert3",
        [OP_insert4]                = "OP_insert4",
        [OP_perm3]                  = "OP_perm3",
        [OP_perm4]                  = "OP_perm4",
        [OP_perm5]                  = "OP_perm5",
        [OP_swap]                   = "OP_swap",
        [OP_swap2]                  = "OP_swap2",
        [OP_rot3l]                  = "OP_rot3l",
        [OP_rot3r]                  = "OP_rot3r",
        [OP_rot4l]                  = "OP_rot4l",
        [OP_rot5l]                  = "OP_rot5l",
        [OP_call_constructor]       = "OP_call_constructor",
        [OP_call]                   = "OP_call",
        [OP_tail_call]              = "OP_tail_call",
        [OP_call_method]            = "OP_call_method",
        [OP_tail_call_method]       = "OP_tail_call_method",
        [OP_array_from]             = "OP_array_from",
        [OP_apply]                  = "OP_apply",
        [OP_return]                 = "OP_return",
        [OP_return_undef]           = "OP_return_undef",
        [OP_check_ctor_return]      = "OP_check_ctor_return",
        [OP_check_ctor]             = "OP_check_ctor",
        [OP_check_brand]            = "OP_check_brand",
        [OP_add_brand]              = "OP_add_brand",
        [OP_return_async]           = "OP_return_async",
        [OP_throw]                  = "OP_throw",
        [OP_throw_error]            = "OP_throw_error",
        [OP_eval]                   = "OP_eval",
        [OP_apply_eval]             = "OP_apply_eval",
        [OP_regexp]                 = "OP_regexp",
        [OP_get_super]              = "OP_get_super",
        [OP_import]                 = "OP_import",
        [OP_check_var]              = "OP_check_var",
        [OP_get_var_undef]          = "OP_get_var_undef",
        [OP_get_var]                = "OP_get_var",
        [OP_put_var]                = "OP_put_var",
        [OP_put_var_init]           = "OP_put_var_init",
        [OP_put_var_strict]         = "OP_put_var_strict",
        [OP_get_ref_value]          = "OP_get_ref_value",
        [OP_put_ref_value]          = "OP_put_ref_value",
        [OP_define_var]             = "OP_define_var",
        [OP_check_define_var]       = "OP_check_define_var",
        [OP_define_func]            = "OP_define_func",
        [OP_get_field]              = "OP_get_field",
        [OP_get_field2]             = "OP_get_field2",
        [OP_put_field]              = "OP_put_field",
        [OP_get_private_field]      = "OP_get_private_field",
        [OP_put_private_field]      = "OP_put_private_field",
        [OP_define_private_field]   = "OP_define_private_field",
        [OP_get_array_el]           = "OP_get_array_el",
        [OP_get_array_el2]          = "OP_get_array_el2",
        [OP_put_array_el]           = "OP_put_array_el",
        [OP_get_super_value]        = "OP_get_super_value",
        [OP_put_super_value]        = "OP_put_super_value",
        [OP_define_field]           = "OP_define_field",
        [OP_set_name]               = "OP_set_name",
        [OP_set_name_computed]      = "OP_set_name_computed",
        [OP_set_proto]              = "OP_set_proto",
        [OP_set_home_object]        = "OP_set_home_object",
        [OP_define_array_el]        = "OP_define_array_el",
        [OP_append]                 = "OP_append",
        [OP_copy_data_properties]   = "OP_copy_data_properties",
        [OP_define_method]          = "OP_define_method",
        [OP_define_method_computed] = "OP_define_method_computed",
        [OP_define_class]           = "OP_define_class",
        [OP_define_class_computed]  = "OP_define_class_computed",
        [OP_get_loc]                = "OP_get_loc",
        [OP_put_loc]                = "OP_put_loc",
        [OP_set_loc]                = "OP_set_loc",
        [OP_get_arg]                = "OP_get_arg",
        [OP_put_arg]                = "OP_put_arg",
        [OP_set_arg]                = "OP_set_arg",
        [OP_get_var_ref]            = "OP_get_var_ref",
        [OP_put_var_ref]            = "OP_put_var_ref",
        [OP_set_var_ref]            = "OP_set_var_ref",
        [OP_set_loc_uninitialized]  = "OP_set_loc_uninitialized",
        [OP_get_loc_check]          = "OP_get_loc_check",
        [OP_put_loc_check]          = "OP_put_loc_check",
        [OP_put_loc_check_init]     = "OP_put_loc_check_init",
        [OP_get_var_ref_check]      = "OP_get_var_ref_check",
        [OP_put_var_ref_check]      = "OP_put_var_ref_check",
        [OP_put_var_ref_check_init] = "OP_put_var_ref_check_init",
        [OP_close_loc]              = "OP_close_loc",
        [OP_if_false]               = "OP_if_false",
        [OP_if_true]                = "OP_if_true",
        [OP_goto]                   = "OP_goto",
        [OP_catch]                  = "OP_catch",
        [OP_gosub]                  = "OP_gosub",
        [OP_ret]                    = "OP_ret",
        [OP_to_object]              = "OP_to_object",
        [OP_to_propkey]             = "OP_to_propkey",
        [OP_to_propkey2]            = "OP_to_propkey2",
        [OP_with_get_var]           = "OP_with_get_var",
        [OP_with_put_var]           = "OP_with_put_var",
        [OP_with_delete_var]        = "OP_with_delete_var",
        [OP_with_make_ref]          = "OP_with_make_ref",
        [OP_with_get_ref]           = "OP_with_get_ref",
        [OP_with_get_ref_undef]     = "OP_with_get_ref_undef",
        [OP_make_loc_ref]           = "OP_make_loc_ref",
        [OP_make_arg_ref]           = "OP_make_arg_ref",
        [OP_make_var_ref_ref]       = "OP_make_var_ref_ref",
        [OP_make_var_ref]           = "OP_make_var_ref",
        [OP_for_in_start]           = "OP_for_in_start",
        [OP_for_of_start]           = "OP_for_of_start",
        [OP_for_await_of_start]     = "OP_for_await_of_start",
        [OP_for_in_next]            = "OP_for_in_next",
        [OP_for_of_next]            = "OP_for_of_next",
        [OP_iterator_check_object]  = "OP_iterator_check_object",
        [OP_iterator_get_value_done]= "OP_iterator_get_value_done",
        [OP_iterator_close]         = "OP_iterator_close",
        [OP_iterator_close_return]  = "OP_iterator_close_return",
        [OP_iterator_next]          = "OP_iterator_next",
        [OP_iterator_call]          = "OP_iterator_call",
        [OP_initial_yield]          = "OP_initial_yield",
        [OP_yield]                  = "OP_yield",
        [OP_yield_star]             = "OP_yield_star",
        [OP_async_yield_star]       = "OP_async_yield_star",
        [OP_await]                  = "OP_await",
        [OP_neg]                    = "OP_neg",
        [OP_plus]                   = "OP_plus",
        [OP_dec]                    = "OP_dec",
        [OP_inc]                    = "OP_inc",
        [OP_post_dec]               = "OP_post_dec",
        [OP_post_inc]               = "OP_post_inc",
        [OP_dec_loc]                = "OP_dec_loc",
        [OP_inc_loc]                = "OP_inc_loc",
        [OP_add_loc]                = "OP_add_loc",
        [OP_not]                    = "OP_not",
        [OP_lnot]                   = "OP_lnot",
        [OP_typeof]                 = "OP_typeof",
        [OP_delete]                 = "OP_delete",
        [OP_delete_var]             = "OP_delete_var",
        [OP_mul]                    = "OP_mul",
        [OP_div]                    = "OP_div",
        [OP_mod]                    = "OP_mod",
        [OP_add]                    = "OP_add",
        [OP_sub]                    = "OP_sub",
        [OP_pow]                    = "OP_pow",
        [OP_shl]                    = "OP_shl",
        [OP_sar]                    = "OP_sar",
        [OP_shr]                    = "OP_shr",
        [OP_lt]                     = "OP_lt",
        [OP_lte]                    = "OP_lte",
        [OP_gt]                     = "OP_gt",
        [OP_gte]                    = "OP_gte",
        [OP_instanceof]             = "OP_instanceof",
        [OP_in]                     = "OP_in",
        [OP_eq]                     = "OP_eq",
        [OP_neq]                    = "OP_neq",
        [OP_strict_eq]              = "OP_strict_eq",
        [OP_strict_neq]             = "OP_strict_neq",
        [OP_and]                    = "OP_and",
        [OP_xor]                    = "OP_xor",
        [OP_or]                     = "OP_or",
        [OP_is_undefined_or_null]   = "OP_is_undefined_or_null",
#ifdef CONFIG_BIGNUM
        /* Only emitted by QuickJS when bignum support is compiled in. */
        [OP_mul_pow10]              = "OP_mul_pow10",
        [OP_math_mod]               = "OP_math_mod",
#endif
        [OP_nop]                    = "OP_nop",
        [OP_push_minus1]            = "OP_push_minus1",
        [OP_push_0]                 = "OP_push_0",
        [OP_push_1]                 = "OP_push_1",
        [OP_push_2]                 = "OP_push_2",
        [OP_push_3]                 = "OP_push_3",
        [OP_push_4]                 = "OP_push_4",
        [OP_push_5]                 = "OP_push_5",
        [OP_push_6]                 = "OP_push_6",
        [OP_push_7]                 = "OP_push_7",
        [OP_push_i8]                = "OP_push_i8",
        [OP_push_i16]               = "OP_push_i16",
        [OP_push_const8]            = "OP_push_const8",
        [OP_fclosure8]              = "OP_fclosure8",
        [OP_push_empty_string]      = "OP_push_empty_string",
        [OP_get_loc8]               = "OP_get_loc8",
        [OP_put_loc8]               = "OP_put_loc8",
        [OP_set_loc8]               = "OP_set_loc8",
        [OP_get_loc0]               = "OP_get_loc0",
        [OP_get_loc1]               = "OP_get_loc1",
        [OP_get_loc2]               = "OP_get_loc2",
        [OP_get_loc3]               = "OP_get_loc3",
        [OP_put_loc0]               = "OP_put_loc0",
        [OP_put_loc1]               = "OP_put_loc1",
        [OP_put_loc2]               = "OP_put_loc2",
        [OP_put_loc3]               = "OP_put_loc3",
        [OP_set_loc0]               = "OP_set_loc0",
        [OP_set_loc1]               = "OP_set_loc1",
        [OP_set_loc2]               = "OP_set_loc2",
        [OP_set_loc3]               = "OP_set_loc3",
        [OP_get_arg0]               = "OP_get_arg0",
        [OP_get_arg1]               = "OP_get_arg1",
        [OP_get_arg2]               = "OP_get_arg2",
        [OP_get_arg3]               = "OP_get_arg3",
        [OP_put_arg0]               = "OP_put_arg0",
        [OP_put_arg1]               = "OP_put_arg1",
        [OP_put_arg2]               = "OP_put_arg2",
        [OP_put_arg3]               = "OP_put_arg3",
        [OP_set_arg0]               = "OP_set_arg0",
        [OP_set_arg1]               = "OP_set_arg1",
        [OP_set_arg2]               = "OP_set_arg2",
        [OP_set_arg3]               = "OP_set_arg3",
        [OP_get_var_ref0]           = "OP_get_var_ref0",
        [OP_get_var_ref1]           = "OP_get_var_ref1",
        [OP_get_var_ref2]           = "OP_get_var_ref2",
        [OP_get_var_ref3]           = "OP_get_var_ref3",
        [OP_put_var_ref0]           = "OP_put_var_ref0",
        [OP_put_var_ref1]           = "OP_put_var_ref1",
        [OP_put_var_ref2]           = "OP_put_var_ref2",
        [OP_put_var_ref3]           = "OP_put_var_ref3",
        [OP_set_var_ref0]           = "OP_set_var_ref0",
        [OP_set_var_ref1]           = "OP_set_var_ref1",
        [OP_set_var_ref2]           = "OP_set_var_ref2",
        [OP_set_var_ref3]           = "OP_set_var_ref3",
        [OP_get_length]             = "OP_get_length",
        [OP_if_false8]              = "OP_if_false8",
        [OP_if_true8]               = "OP_if_true8",
        [OP_goto8]                  = "OP_goto8",
        [OP_goto16]                 = "OP_goto16",
        [OP_call0]                  = "OP_call0",
        [OP_call1]                  = "OP_call1",
        [OP_call2]                  = "OP_call2",
        [OP_call3]                  = "OP_call3",
        [OP_is_undefined]           = "OP_is_undefined",
        [OP_is_null]                = "OP_is_null",
        [OP_typeof_is_undefined]    = "OP_typeof_is_undefined",
        [OP_typeof_is_function]     = "OP_typeof_is_function",
    };

    if (op < 0 || op >= (int)(sizeof(op_names) / sizeof(op_names[0])))
        return "OP_UNKNOWN";

    if (op_names[op] == NULL)
        return "OP_UNKNOWN";

    return op_names[op];
}

/* Forward declarations */
static int emit_ir_instruction(NativeCompiler *nc, const IRInstr *instr, const int *block_to_label);
static void emit_function_prologue(NativeCompiler *nc, const NativeFuncSignature *sig);
static void emit_function_epilogue(NativeCompiler *nc, const NativeFuncSignature *sig);

/* Build opcode size table the same way QuickJS does */
/* Match QuickJS opcode_info structure: only DEF, not def */
/* The array size is OP_COUNT + (OP_TEMP_END - OP_TEMP_START) to include temp opcodes */

typedef struct JSOpCode {
    #ifdef DUMP_BYTECODE
        const char *name;
    #endif
        uint8_t size; /* in bytes */
        /* the opcodes remove n_pop items from the top of the stack, then
           pushes n_push items */
        uint8_t n_pop;
        uint8_t n_push;
        uint8_t fmt;
    } JSOpCode;

static const JSOpCode opcode_info[OP_COUNT + (OP_TEMP_END - OP_TEMP_START)] = {
    #define FMT(f)
    #define DEF(id, size, n_pop, n_push, f) { size, n_pop, n_push, OP_FMT_ ## f },
    /* Do NOT define `def` here: quickjs-opcode.h falls back to `def -> DEF`, which
     * fills the trailing (temp/short opcode) region of the array with real sizes.
     * short_opcode_info() indexes short opcodes (op >= OP_TEMP_START) into that
     * region; leaving it zeroed makes their size 0 and hangs the decode loops. */
    #include "../quickjs/quickjs-opcode.h"
    #undef DEF
    #undef def
    #undef FMT
    /* The array has OP_COUNT entries for DEF opcodes (positions 0..OP_COUNT-1).
     * It also has space for (OP_TEMP_END - OP_TEMP_START) temporary opcodes
     * (positions OP_COUNT..OP_COUNT + (OP_TEMP_END - OP_TEMP_START) - 1), but these
     * are not initialized because temp opcodes don't appear in final bytecode.
     * When SHORT_OPCODES is defined, short opcodes have enum values that may overlap
     * with temp opcode values. The short_opcode_info macro correctly maps them by
     * checking if op >= OP_TEMP_START and adding (OP_TEMP_END - OP_TEMP_START) to
     * the index, which maps short opcodes to their correct positions in the array. */
};

/* Helper function to get opcode info using short_opcode_info macro */
static inline const JSOpCode *get_opcode_info(uint8_t op) {
    return &short_opcode_info(op);
}

/* Object provenance follows the bytecode operand stack, not the last local
 * loaded. In `particles[index]`, that local is index; in `a.x = b.x`, it is b.
 * Keep this analysis separate from numeric inference: all scalar values are
 * equivalent here, while struct/array pointers retain their schema through
 * locals, nested indexing, calls, and control-flow joins. */
typedef struct NCObjectType {
    NativeType type;
    NativeStructDef *def;
} NCObjectType;

typedef struct NCObjectFlow {
    int *offsets;
    NCObjectType *values;
    NCObjectType locals[NC_MAX_LOCALS];
} NCObjectFlow;

static bool nc_object_merge(NCObjectType *dst, NCObjectType src) {
    if (src.type == NATIVE_TYPE_UNKNOWN ||
        (dst->type == src.type && dst->def == src.def)) return false;
    if (dst->type != NATIVE_TYPE_UNKNOWN) {
        src.type = NATIVE_TYPE_VOID; /* incompatible object types */
        src.def = NULL;
    }
    if (dst->type == src.type && dst->def == src.def) return false;
    *dst = src;
    return true;
}

static int nc_bytecode_local(uint8_t op, const uint8_t *p, int argc,
                             bool *load, bool *store) {
    *load = *store = false;
    switch (op) {
        case OP_get_arg: *load = true; return p[0] | (p[1] << 8);
        case OP_put_arg: case OP_set_arg:
            *store = true; return p[0] | (p[1] << 8);
        case OP_get_loc: case OP_get_loc_check:
            *load = true; return argc + (p[0] | (p[1] << 8));
        case OP_put_loc: case OP_set_loc: case OP_put_loc_check:
        case OP_put_loc_check_init:
            *store = true; return argc + (p[0] | (p[1] << 8));
        case OP_get_loc8: *load = true; return argc + p[0];
        case OP_put_loc8: case OP_set_loc8:
            *store = true; return argc + p[0];
        default:
            if (op >= OP_get_arg0 && op <= OP_get_arg3) {
                *load = true; return op - OP_get_arg0;
            }
            if (op >= OP_put_arg0 && op <= OP_put_arg3) {
                *store = true; return op - OP_put_arg0;
            }
            if (op >= OP_set_arg0 && op <= OP_set_arg3) {
                *store = true; return op - OP_set_arg0;
            }
            if (op >= OP_get_loc0 && op <= OP_get_loc3) {
                *load = true; return argc + op - OP_get_loc0;
            }
            if (op >= OP_put_loc0 && op <= OP_put_loc3) {
                *store = true; return argc + op - OP_put_loc0;
            }
            if (op >= OP_set_loc0 && op <= OP_set_loc3) {
                *store = true; return argc + op - OP_set_loc0;
            }
            return -1;
    }
}

static int nc_bytecode_successors(const uint8_t *bc, size_t pc,
                                  size_t len, int successors[2]) {
    uint8_t op = bc[pc];
    const JSOpCode *info = get_opcode_info(op);
    int next = (int)pc + info->size;
    bool jump = op == OP_goto || op == OP_goto8 || op == OP_goto16;
    bool branch = op == OP_if_false || op == OP_if_true ||
                  op == OP_if_false8 || op == OP_if_true8;
    if (jump || branch) {
        int32_t rel;
        if (info->fmt == OP_FMT_label8) rel = (int8_t)bc[pc + 1];
        else if (info->fmt == OP_FMT_label16)
            rel = (int16_t)(bc[pc + 1] | (bc[pc + 2] << 8));
        else
            rel = (int32_t)((uint32_t)bc[pc + 1] | ((uint32_t)bc[pc + 2] << 8) |
                           ((uint32_t)bc[pc + 3] << 16) | ((uint32_t)bc[pc + 4] << 24));
        successors[0] = (int)pc + 1 + rel;
        successors[1] = branch && next < (int)len ? next : -1;
        return branch ? 2 : 1;
    }
    if (op == OP_return || op == OP_return_undef || op == OP_tail_call ||
        op == OP_tail_call_method || op == OP_throw || op == OP_throw_error)
        return 0;
    successors[0] = next < (int)len ? next : -1;
    return 1;
}

static int nc_bytecode_pops(uint8_t op, const uint8_t *p) {
    const JSOpCode *info = get_opcode_info(op);
    int n = info->n_pop;
    if (info->fmt == OP_FMT_npop || info->fmt == OP_FMT_npop_u16)
        n += p[0] | (p[1] << 8);
    else if (info->fmt == OP_FMT_npopx)
        n += op - OP_call0;
    return n;
}

static const NativeStructField *nc_object_field(JSContext *ctx, NCObjectType obj,
                                         const uint8_t *p) {
    if (!ctx || !obj.def || obj.type != NATIVE_TYPE_PTR) return NULL;
    JSAtom atom = (JSAtom)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                          ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
    const char *name = JS_AtomToCString(ctx, atom);
    const NativeStructField *field = NULL;
    if (name) {
        for (int i = 0; i < obj.def->field_count; i++) {
            if (!strcmp(obj.def->fields[i].name, name)) {
                field = &obj.def->fields[i];
                break;
            }
        }
        JS_FreeCString(ctx, name);
    }
    return field;
}

static int nc_analyze_objects(NativeCompiler *nc, const uint8_t *bc,
                              size_t len, const NativeFuncSignature *sig,
                              NCObjectFlow *flow) {
    int *depths = malloc((len + 1) * sizeof(int));
    int *queue = malloc((len + 1) * sizeof(int));
    flow->offsets = malloc((len + 1) * sizeof(int));
    if (!depths || !queue || !flow->offsets) goto oom;
    for (size_t pc = 0; pc <= len; pc++) depths[pc] = flow->offsets[pc] = -1;
    depths[0] = 0;
    int head = 0, tail = 0, total = 0;
    queue[tail++] = 0;
    while (head < tail) {
        int pc = queue[head++], succ[2];
        const JSOpCode *info = get_opcode_info(bc[pc]);
        if (!info->size || pc + info->size > len) goto invalid;
        int pops = nc_bytecode_pops(bc[pc], bc + pc + 1);
        int exit = depths[pc] - pops + info->n_push;
        if (depths[pc] < pops || exit > NC_MAX_STACK_DEPTH) goto invalid;
        flow->offsets[pc] = total;
        total += depths[pc];
        int count = nc_bytecode_successors(bc, pc, len, succ);
        for (int s = 0; s < count; s++) {
            int target = succ[s];
            if (target == -1) continue;
            if (target < 0 || target >= len) goto invalid;
            if (depths[target] >= 0) {
                if (depths[target] != exit) goto invalid;
            } else {
                depths[target] = exit;
                queue[tail++] = target;
            }
        }
    }
    flow->values = calloc(total ? total : 1, sizeof(NCObjectType));
    if (!flow->values) goto oom;
    NCObjectType locals[NC_MAX_LOCALS] = {{0}};
    NCObjectType scalar = {NATIVE_TYPE_INT32, NULL};
    for (int i = 0; i < sig->arg_count; i++) {
        locals[i].type = sig->arg_types[i];
        locals[i].def = nc->local_struct_defs[i];
        if (!locals[i].def && !NATIVE_TYPE_IS_ARRAY(locals[i].type) &&
            !NATIVE_TYPE_IS_DYNAMIC_ARRAY(locals[i].type) &&
            !NATIVE_TYPE_IS_STRING(locals[i].type)) locals[i] = scalar;
    }
    /* Monotone fixed point: UNKNOWN -> concrete -> incompatible. Stack slots
     * occupy only the actual bytecode depth, rather than len * MAX_DEPTH. */
    bool changed;
    do {
        changed = false;
        for (int q = 0; q < tail; q++) {
            int pc = queue[q], depth = depths[pc], succ[2];
            uint8_t op = bc[pc];
            const uint8_t *p = bc + pc + 1;
            const JSOpCode *info = get_opcode_info(op);
            int pops = nc_bytecode_pops(op, p), keep = depth - pops;
            NCObjectType stack[NC_MAX_STACK_DEPTH];
            memcpy(stack, flow->values + flow->offsets[pc], depth * sizeof(*stack));
            NCObjectType a = pops ? stack[keep] : scalar;
            NCObjectType top = depth ? stack[depth - 1] : scalar;
            for (int i = 0; i < info->n_push; i++) stack[keep + i] = scalar;
            bool load, store;
            int local = nc_bytecode_local(op, p, sig->arg_count, &load, &store);
            if (local >= 0 && local < NC_MAX_LOCALS) {
                if (load) stack[keep] = locals[local];
                if (store) {
                    changed |= nc_object_merge(&locals[local], top);
                    if (info->n_push) stack[keep] = top;
                }
            } else if (op == OP_get_array_el || op == OP_get_array_el2) {
                NCObjectType result = scalar;
                if (a.type == NATIVE_TYPE_STRUCT_ARRAY) {
                    result.type = NATIVE_TYPE_PTR; result.def = a.def;
                } else if (a.type == NATIVE_TYPE_UNKNOWN || a.type == NATIVE_TYPE_VOID)
                    result = a;
                stack[keep] = result;
                if (op == OP_get_array_el2) { stack[keep] = a; stack[keep + 1] = result; }
            } else if (op == OP_get_field) {
                const NativeStructField *field = nc_object_field(nc->js_ctx, a, p);
                if (field && (field->flags & FIELD_FLAG_ARRAY)) {
                    stack[keep].type = field->type == NATIVE_TYPE_FLOAT32 ? NATIVE_TYPE_FLOAT32_ARRAY :
                        field->type == NATIVE_TYPE_UINT32 ? NATIVE_TYPE_UINT32_ARRAY : NATIVE_TYPE_INT32_ARRAY;
                } else if (a.type == NATIVE_TYPE_UNKNOWN) stack[keep] = a;
            } else if (op == OP_get_field2) {
                stack[keep] = a; /* receiver survives until call_method */
            } else if (op == OP_dup) {
                stack[keep] = stack[keep + 1] = top;
            } else if (op == OP_dup1) {
                stack[keep] = a; stack[keep + 1] = a; stack[keep + 2] = top;
            } else if (op == OP_dup2 || op == OP_dup3) {
                int n = op == OP_dup2 ? 2 : 3;
                memcpy(stack + keep, flow->values + flow->offsets[pc] + keep, n * sizeof(*stack));
                memcpy(stack + keep + n, stack + keep, n * sizeof(*stack));
            } else if (op == OP_swap) {
                stack[keep] = top; stack[keep + 1] = a;
            } else if (op == OP_nip) {
                stack[keep] = top;
            }
            int exit = keep + info->n_push;
            int count = nc_bytecode_successors(bc, pc, len, succ);
            for (int s = 0; s < count; s++) {
                if (succ[s] < 0) continue;
                NCObjectType *dst = flow->values + flow->offsets[succ[s]];
                for (int i = 0; i < exit; i++) changed |= nc_object_merge(&dst[i], stack[i]);
            }
        }
    } while (changed);
    memcpy(flow->locals, locals, sizeof(locals));
    /* Convert offsets to receiver locations for the decoder. */
    for (int q = 0; q < tail; q++) {
        int pc = queue[q], pops = get_opcode_info(bc[pc])->n_pop;
        if (pops && depths[pc] >= pops) flow->offsets[pc] += depths[pc] - pops;
        else flow->offsets[pc] = -1;
    }
    free(queue); free(depths);
    return 0;
invalid:
    snprintf(nc->error_msg, sizeof(nc->error_msg), "Invalid bytecode operand stack in object analysis");
    nc->has_error = true;
    free(queue); free(depths);
    return -1;
oom:
    snprintf(nc->error_msg, sizeof(nc->error_msg), "Out of memory analyzing native object types");
    nc->has_error = true;
    free(queue); free(depths);
    return -1;
}

/* ============================================
 * Native Compiler Helpers
 * ============================================ */

static bool nc_is_dynamic_array(NativeType t) {
    return t == NATIVE_TYPE_DYNAMIC_INT32_ARRAY ||
           t == NATIVE_TYPE_DYNAMIC_UINT32_ARRAY ||
           t == NATIVE_TYPE_DYNAMIC_FLOAT32_ARRAY;
}

static NativeType nc_dynamic_elem(NativeType t) {
    switch (t) {
        case NATIVE_TYPE_DYNAMIC_UINT32_ARRAY: return NATIVE_TYPE_UINT32;
        case NATIVE_TYPE_DYNAMIC_FLOAT32_ARRAY: return NATIVE_TYPE_FLOAT32;
        default: return NATIVE_TYPE_INT32;
    }
}

static NativeType nc_get_local_type(const IRFunction *ir, int idx) {
    if (ir && idx >= 0 && idx < ir->local_count)
        return ir->local_types[idx];
    return NATIVE_TYPE_UNKNOWN;
}

static NativeType nc_infer_array_elem_type(NativeCompiler *nc, const IRFunction *ir) {
    if (nc->pending_intrinsic_elem_type != NATIVE_TYPE_UNKNOWN)
        return nc->pending_intrinsic_elem_type;
    if (nc->last_loaded_local >= 0) {
        NativeType lt = nc_get_local_type(ir, nc->last_loaded_local);
        if (nc_is_dynamic_array(lt))
            return nc_dynamic_elem(lt);
    }
    for (int i = 0; ir && i < ir->sig.arg_count; i++) {
        if (nc_is_dynamic_array(ir->sig.arg_types[i]))
            return nc_dynamic_elem(ir->sig.arg_types[i]);
    }
    return NATIVE_TYPE_INT32;
}

static void nc_free_string_literals(NativeCompiler *nc) {
    if (!nc) return;
    for (int i = 0; i < nc->compile_string_count; i++)
        free(nc->compile_string_literals[i]);
    free(nc->compile_string_literals);
    nc->compile_string_literals = NULL;
    nc->compile_string_count = 0;
    nc->compile_string_capacity = 0;
}

static int nc_add_string_literal(NativeCompiler *nc, const char *str) {
    if (!nc || !str) return -1;
    if (nc->compile_string_count >= nc->compile_string_capacity) {
        int new_cap = nc->compile_string_capacity ? nc->compile_string_capacity * 2 : 8;
        char **new_arr = (char **)realloc(nc->compile_string_literals,
                                          sizeof(char *) * new_cap);
        if (!new_arr) return -1;
        nc->compile_string_literals = new_arr;
        nc->compile_string_capacity = new_cap;
    }
    char *copy = strdup(str);
    if (!copy) return -1;
    int idx = nc->compile_string_count++;
    nc->compile_string_literals[idx] = copy;
    return idx;
}

static void nc_emit_string_new(NativeCompiler *nc, IRFunction *ir, int block, int lit_idx) {
    IRInstr instr = { .op = IR_STRING_NEW, .type = NATIVE_TYPE_STRING };
    instr.operand.i32 = lit_idx;
    ir_emit(ir, block, &instr);
}

static void nc_emit_cpool_const(NativeCompiler *nc, IRFunction *ir, int block, int idx) {
    if (nc->js_ctx && nc->cpool && idx >= 0 && idx < nc->cpool_count) {
        JSValueConst val = nc->cpool[idx];
        if (JS_IsString(val)) {
            const char *s = JS_ToCString(nc->js_ctx, val);
            if (s) {
                int lit = nc_add_string_literal(nc, s);
                JS_FreeCString(nc->js_ctx, s);
                if (lit >= 0) {
                    nc_emit_string_new(nc, ir, block, lit);
                    return;
                }
            }
        } else if (JS_IsNumber(val)) {
            double d;
            if (JS_ToFloat64(nc->js_ctx, &d, val) == 0) {
                if (d == (double)(int32_t)d) {
                    ir_emit_const_i32(ir, block, (int32_t)d);
                } else {
                    ir_emit_const_f32(ir, block, (float)d);
                }
                return;
            }
        }
    }
    ir_emit_const_i32(ir, block, 0);
}

static void *nc_resolve_cpool_func_ptr(NativeCompiler *nc, int cpool_idx) {
    if (!nc->js_ctx || !nc->cpool || cpool_idx < 0 || cpool_idx >= nc->cpool_count)
        return NULL;
    JSValueConst val = nc->cpool[cpool_idx];
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return NULL;
    JSValue handle = JS_GetPropertyStr(nc->js_ctx, val, "_nativeHandle");
    if (JS_IsException(handle) || JS_IsUndefined(handle)) {
        JS_FreeValue(nc->js_ctx, handle);
        return NULL;
    }
    uint32_t h;
    if (JS_ToUint32(nc->js_ctx, &h, handle) != 0) {
        JS_FreeValue(nc->js_ctx, handle);
        return NULL;
    }
    JS_FreeValue(nc->js_ctx, handle);
    NativeFunc *func = (NativeFunc *)(uintptr_t)h;
    if (!func || !func->is_valid || !func->code_ptr)
        return NULL;
    nc->pending_call_sig = func->sig;
    nc->has_pending_call_sig = true;
    return func->code_ptr;
}

static void *nc_resolve_closure_func_ptr(NativeCompiler *nc, int var_ref_idx) {
    if (!nc->js_ctx || var_ref_idx < 0 || var_ref_idx >= nc->closure_var_count)
        return NULL;
    JSValueConst val = JS_GetFunctionClosureVarValue(nc->js_ctx, nc->current_js_func, var_ref_idx);
    if (JS_VALUE_GET_TAG(val) != JS_TAG_OBJECT)
        return NULL;
    JSValue handle = JS_GetPropertyStr(nc->js_ctx, val, "_nativeHandle");
    if (JS_IsException(handle) || JS_IsUndefined(handle)) {
        JS_FreeValue(nc->js_ctx, handle);
        return NULL;
    }
    uint32_t h;
    if (JS_ToUint32(nc->js_ctx, &h, handle) != 0) {
        JS_FreeValue(nc->js_ctx, handle);
        return NULL;
    }
    JS_FreeValue(nc->js_ctx, handle);
    NativeFunc *func = (NativeFunc *)(uintptr_t)h;
    if (!func || !func->is_valid || !func->code_ptr)
        return NULL;
    nc->pending_call_sig = func->sig;
    nc->has_pending_call_sig = true;
    return func->code_ptr;
}

static void nc_emit_func_ptr_const(NativeCompiler *nc, IRFunction *ir, int block, void *code_ptr) {
    IRInstr instr = { .op = IR_CONST_I32, .type = NATIVE_TYPE_PTR };
    instr.operand.i32 = (int32_t)(intptr_t)code_ptr;
    ir_emit(ir, block, &instr);
}

/*
 * If nc_resolve_closure_func_ptr/nc_resolve_cpool_func_ptr just resolved the
 * callee for this call/tail-call, stamp its real signature onto the IR
 * instruction (return type + per-argument types) instead of leaving the
 * hardcoded INT32 defaults. Without this, calling a float-returning nested
 * function (e.g. lerp/clamp) from a plain (non-tail) call reads the result
 * from $v0 instead of $f0, and literal arguments (which are IR_CONST_I32
 * regardless of an `f` suffix) get passed as raw mismatched bits instead of
 * being converted to what the callee actually expects - see IR_CALL's
 * codegen, which uses arg_types[] to decide.
 */
static void nc_apply_pending_call_sig(NativeCompiler *nc, IRInstr *call) {
    if (!nc->has_pending_call_sig) return;
    call->type = nc->pending_call_sig.return_type;
    call->operand.call.ret_type = nc->pending_call_sig.return_type;
    int n = call->operand.call.arg_count;
    if (n > 8) n = 8;
    for (int i = 0; i < n; i++) {
        call->operand.call.arg_types[i] = nc->pending_call_sig.arg_types[i];
    }
    nc->has_pending_call_sig = false;
}

static void *nc_array_push_fn(NativeType elem) {
    switch (elem) {
        case NATIVE_TYPE_UINT32: return (void *)native_array_push_u32;
        case NATIVE_TYPE_FLOAT32: return (void *)native_array_push_f32_bits;
        default: return (void *)native_array_push_i32;
    }
}

static void *nc_array_pop_fn(NativeType elem) {
    switch (elem) {
        case NATIVE_TYPE_UINT32: return (void *)native_array_pop_u32;
        case NATIVE_TYPE_FLOAT32: return (void *)native_array_pop_f32;
        default: return (void *)native_array_pop_i32;
    }
}

static void *nc_array_get_fn(NativeType elem) {
    switch (elem) {
        case NATIVE_TYPE_UINT32: return (void *)native_array_get_u32;
        case NATIVE_TYPE_FLOAT32: return (void *)native_array_get_f32;
        default: return (void *)native_array_get_i32;
    }
}

static void *nc_array_set_fn(NativeType elem) {
    switch (elem) {
        case NATIVE_TYPE_UINT32: return (void *)native_array_set_u32;
        case NATIVE_TYPE_FLOAT32: return (void *)native_array_set_f32;
        default: return (void *)native_array_set_i32;
    }
}

static void *nc_array_insert_fn(NativeType elem) {
    (void)elem;
    return (void *)native_array_insert_i32;
}

static bool nc_emit_array_string_intrinsic(NativeCompiler *nc, IRFunction *ir_out,
                                           int current_block, IROp op, int nargs) {
    NativeType elem = nc_infer_array_elem_type(nc, ir_out);
    IRInstr instr = { .op = op, .type = NATIVE_TYPE_BOOL };
    instr.operand.field.field_type = elem;

    /* call_method / tail_call_method pass nargs = explicit JS args only.
     * `this` is already on the IR stack from get_arg0 (or equivalent). */
    switch (op) {
        case IR_ARRAY_LENGTH:
            if (nargs != 0) return false;
            instr.type = NATIVE_TYPE_INT32;
            break;
        case IR_ARRAY_PUSH:
            if (nargs != 1) return false;
            break;
        case IR_ARRAY_POP:
            if (nargs != 0) return false;
            instr.type = elem;
            break;
        case IR_ARRAY_CLEAR:
            if (nargs != 0) return false;
            instr.type = NATIVE_TYPE_VOID;
            break;
        case IR_ARRAY_RESIZE:
        case IR_ARRAY_RESERVE:
        case IR_ARRAY_REMOVE:
            if (nargs != 1) return false;
            break;
        case IR_STRING_LENGTH:
            if (nargs != 0) return false;
            instr.type = NATIVE_TYPE_INT32;
            break;
        case IR_STRING_SLICE:
            if (nargs != 2) return false;
            instr.type = NATIVE_TYPE_STRING;
            break;
        case IR_STRING_FIND:
            if (nargs != 1) return false;
            instr.type = NATIVE_TYPE_INT32;
            break;
        case IR_STRING_TO_UPPER:
        case IR_STRING_TO_LOWER:
        case IR_STRING_TRIM:
            if (nargs != 0) return false;
            instr.type = NATIVE_TYPE_STRING;
            break;
        case IR_STRING_REPLACE:
            if (nargs != 2) return false;
            instr.type = NATIVE_TYPE_STRING;
            break;
        default:
            return false;
    }

    ir_emit(ir_out, current_block, &instr);
    return true;
}

static bool nc_match_array_string_method(NativeCompiler *nc, const IRFunction *ir,
                                         const char *name) {
    nc->pending_intrinsic_elem_type = NATIVE_TYPE_UNKNOWN;
    if (strcmp(name, "push") == 0) {
        nc->pending_intrinsic_op = IR_ARRAY_PUSH;
        return true;
    }
    if (strcmp(name, "pop") == 0) {
        nc->pending_intrinsic_op = IR_ARRAY_POP;
        return true;
    }
    if (strcmp(name, "clear") == 0) {
        nc->pending_intrinsic_op = IR_ARRAY_CLEAR;
        return true;
    }
    if (strcmp(name, "resize") == 0) {
        nc->pending_intrinsic_op = IR_ARRAY_RESIZE;
        return true;
    }
    if (strcmp(name, "reserve") == 0) {
        nc->pending_intrinsic_op = IR_ARRAY_RESERVE;
        return true;
    }
    if (strcmp(name, "remove") == 0) {
        nc->pending_intrinsic_op = IR_ARRAY_REMOVE;
        return true;
    }
    if (strcmp(name, "slice") == 0) {
        nc->pending_intrinsic_op = IR_STRING_SLICE;
        return true;
    }
    if (strcmp(name, "indexOf") == 0) {
        nc->pending_intrinsic_op = IR_STRING_FIND;
        return true;
    }
    if (strcmp(name, "toUpperCase") == 0) {
        nc->pending_intrinsic_op = IR_STRING_TO_UPPER;
        return true;
    }
    if (strcmp(name, "toLowerCase") == 0) {
        nc->pending_intrinsic_op = IR_STRING_TO_LOWER;
        return true;
    }
    if (strcmp(name, "trim") == 0) {
        nc->pending_intrinsic_op = IR_STRING_TRIM;
        return true;
    }
    if (strcmp(name, "replace") == 0) {
        nc->pending_intrinsic_op = IR_STRING_REPLACE;
        return true;
    }
    if (strcmp(name, "length") == 0) {
        NativeType lt = nc_get_local_type(ir, nc->last_loaded_local);
        if (lt == NATIVE_TYPE_STRING)
            nc->pending_intrinsic_op = IR_STRING_LENGTH;
        else if (nc_is_dynamic_array(lt))
            nc->pending_intrinsic_op = IR_ARRAY_LENGTH;
        else
            return false;
        return true;
    }
    return false;
}

/* ============================================
 * IR Function Management
 * ============================================ */

void ir_func_init(IRFunction *ir) {
    if (!ir) return;
    
    memset(ir, 0, sizeof(IRFunction));
    
    ir->blocks = (IRBasicBlock *)malloc(sizeof(IRBasicBlock) * 16);
    ir->block_capacity = 16;
    ir->block_count = 0;
    
    for (int i = 0; i < NC_MAX_LOCALS; i++) {
        ir->local_types[i] = NATIVE_TYPE_UNKNOWN;
    }
    
    /* Note: sig is NOT initialized here - it should be set by the caller
     * after calling ir_func_init, or before calling allocate_locals */
}

void ir_func_free(IRFunction *ir) {
    if (!ir) return;
    
    for (int i = 0; i < ir->block_count; i++) {
        if (ir->blocks[i].instrs) {
            free(ir->blocks[i].instrs);
        }
    }
    
    if (ir->blocks) {
        free(ir->blocks);
        ir->blocks = NULL;
    }
    
    ir->block_count = 0;
    ir->block_capacity = 0;
}

int ir_create_block(IRFunction *ir) {
    if (!ir) return -1;
    
    if (ir->block_count >= NC_MAX_BASIC_BLOCKS)
        return -1;
    
    if (ir->block_count >= ir->block_capacity) {
        int new_cap = ir->block_capacity * 2;
        IRBasicBlock *new_blocks = (IRBasicBlock *)realloc(ir->blocks, 
                                        sizeof(IRBasicBlock) * new_cap);
        if (!new_blocks) return -1;
        ir->blocks = new_blocks;
        ir->block_capacity = new_cap;
    }
    
    int idx = ir->block_count++;
    IRBasicBlock *block = &ir->blocks[idx];
    
    block->instrs = (IRInstr *)malloc(sizeof(IRInstr) * 64);
    block->instr_capacity = 64;
    block->instr_count = 0;
    block->label_id = idx;
    block->successors[0] = -1;
    block->successors[1] = -1;
    block->entry_stack_depth = -1;
    
    return idx;
}

/*
 * True if a block might still fall through to whatever bytecode comes right
 * after it (empty block, or its last instruction isn't an unconditional
 * jump/return). False for a block that has already emitted a terminating
 * instruction - such a block never actually reaches the next sequential PC
 * at runtime even though the raw bytecode decoder's pc keeps advancing past
 * it (e.g. a ternary's then-arm ends in an unconditional jump to the merge
 * point, and the bytes immediately following it are the unrelated else-arm,
 * reached only via a separate explicit branch, never by falling off the
 * then-arm). Used to decide whether recording a successors[1] fallthrough
 * edge for a block is actually true, instead of just recording whatever
 * bytecode happens to sit at the next PC.
 */
static bool block_falls_through(const IRFunction *ir, int block_idx) {
    if (block_idx < 0 || block_idx >= ir->block_count) return false;
    const IRBasicBlock *block = &ir->blocks[block_idx];
    if (block->instr_count == 0) return true;
    IROp last_op = block->instrs[block->instr_count - 1].op;
    return last_op != IR_JUMP && last_op != IR_RETURN && last_op != IR_RETURN_VOID;
}

void ir_emit(IRFunction *ir, int block_idx, const IRInstr *instr) {
    if (!ir || block_idx < 0 || block_idx >= ir->block_count || !instr) return;
    
    IRBasicBlock *block = &ir->blocks[block_idx];
    
    if (block->instr_count >= block->instr_capacity) {
        int new_cap = block->instr_capacity * 2;
        IRInstr *new_instrs = (IRInstr *)realloc(block->instrs, 
                                    sizeof(IRInstr) * new_cap);
        if (!new_instrs) return;
        block->instrs = new_instrs;
        block->instr_capacity = new_cap;
    }
    
    block->instrs[block->instr_count++] = *instr;
}

/* Convenience IR emitters */
void ir_emit_const_i32(IRFunction *ir, int block, int32_t val) {
    IRInstr instr = { .op = IR_CONST_I32, .type = NATIVE_TYPE_INT32 };
    instr.operand.i32 = val;
    ir_emit(ir, block, &instr);
}

void ir_emit_const_f32(IRFunction *ir, int block, float val) {
    IRInstr instr = { .op = IR_CONST_F32, .type = NATIVE_TYPE_FLOAT32 };
    instr.operand.f32 = val;
    ir_emit(ir, block, &instr);
}

void ir_emit_load_local(IRFunction *ir, int block, int idx, NativeType type) {
    IRInstr instr = { .op = IR_LOAD_LOCAL, .type = type, .source_local_idx = (int16_t)idx };
    instr.operand.local_idx = idx;
    ir_emit(ir, block, &instr);
}

void ir_emit_store_local(IRFunction *ir, int block, int idx, NativeType type) {
    IRInstr instr = { .op = IR_STORE_LOCAL, .type = type };
    instr.operand.local_idx = idx;
    ir_emit(ir, block, &instr);
}

void ir_emit_binop(IRFunction *ir, int block, IROp op) {
    IRInstr instr = { .op = op, .type = NATIVE_TYPE_UNKNOWN };
    ir_emit(ir, block, &instr);
}

void ir_emit_jump(IRFunction *ir, int block, int target) {
    IRInstr instr = { .op = IR_JUMP, .type = NATIVE_TYPE_VOID };
    instr.operand.label_id = target;
    ir_emit(ir, block, &instr);
    ir->blocks[block].successors[0] = target;
}

void ir_emit_jump_if(IRFunction *ir, int block, IROp op, int target) {
    IRInstr instr = { .op = op, .type = NATIVE_TYPE_VOID };
    instr.operand.label_id = target;
    ir_emit(ir, block, &instr);
    ir->blocks[block].successors[0] = target;
    /* successors[1] (the fallthrough) is deliberately NOT set here. Every
     * caller creates its own fallthrough block and assigns it right after
     * this returns. The old `block + 1` guess was only ever right by luck -
     * the fallthrough block is not necessarily the next index once nested
     * if/else bodies get numbered - and it silently overwrote a correct
     * value at the one call site that assigned before calling. */
}

void ir_emit_return(IRFunction *ir, int block, NativeType type) {
    IRInstr instr;
    if (type == NATIVE_TYPE_VOID) {
        instr.op = IR_RETURN_VOID;
    } else {
        instr.op = IR_RETURN;
    }
    instr.type = type;
    ir_emit(ir, block, &instr);
}

/* ============================================
 * IR Optimization - Strength Reduction
 * ============================================ */

/* Helper: Check if value is power of 2 and return log2 */
static int is_power_of_2(int32_t val) {
    if (val <= 0) return -1;
    if ((val & (val - 1)) != 0) return -1;
    
    int shift = 0;
    while ((val >> shift) != 1) shift++;
    return shift;
}

/*
 * Optimize IR with strength reduction.
 * Replaces expensive operations with cheaper equivalents:
 * - MUL by power-of-2 -> SHL
 * - DIV by power-of-2 -> SAR (for signed)
 */
static int optimize_ir_strength_reduction(IRFunction *ir) {
    int optimized = 0;
    
    for (int b = 0; b < ir->block_count; b++) {
        IRBasicBlock *block = &ir->blocks[b];
        
        for (int i = 0; i < block->instr_count - 1; i++) {
            IRInstr *prev = &block->instrs[i];
            IRInstr *curr = &block->instrs[i + 1];
            
            /* Pattern: CONST_I32(power_of_2) + MUL_I32 -> replace with SHL */
            if (prev->op == IR_CONST_I32 && curr->op == IR_MUL_I32) {
                int shift = is_power_of_2(prev->operand.i32);
                if (shift >= 0) {
                    /* Replace CONST with the shift amount */
                    prev->operand.i32 = shift;
                    /* Replace MUL with SHL */
                    curr->op = IR_SHL;
                    optimized++;
                }
            }
            
            /* Pattern: CONST_I32(power_of_2) + DIV_I32 -> replace with SAR (signed) */
            if (prev->op == IR_CONST_I32 && curr->op == IR_DIV_I32) {
                int shift = is_power_of_2(prev->operand.i32);
                if (shift >= 0) {
                    prev->operand.i32 = shift;
                    curr->op = IR_SAR;
                    optimized++;
                }
            }
        }
    }
    
    return optimized;
}

/*
 * Dead Code Elimination.
 * Removes instructions whose results are never used.
 * Uses backward analysis to identify which values are actually needed.
 */
static int optimize_ir_dead_code(IRFunction *ir) {
    int eliminated = 0;
    /* A value can be consumed in a successor (short-circuit expressions and
     * ternaries do this routinely). Looking only for a later consumer in this
     * block cannot prove a constant dead. Remove explicit unused values only;
     * both the push and its matching drop must go to preserve stack depth. */
    for (int b = 0; b < ir->block_count; b++) {
        IRBasicBlock *block = &ir->blocks[b];
        for (int i = 0; i + 1 < block->instr_count; i++) {
            IROp op = block->instrs[i].op;
            if ((op == IR_CONST_I32 || op == IR_CONST_F32 || op == IR_CONST_I64) &&
                block->instrs[i + 1].op == IR_DROP) {
                block->instrs[i].op = IR_NOP;
                block->instrs[i + 1].op = IR_NOP;
                eliminated += 2;
                i++;
            }
        }
    }
    return eliminated;
}

/*
 * Constant Folding Optimization.
 * Evaluates constant expressions at compile time:
 * - CONST_I32(a) + CONST_I32(b) + ADD_I32 -> CONST_I32(a+b)
 * - Similar for SUB, MUL, DIV, AND, OR, XOR, SHL, etc.
 */
static int optimize_ir_constant_folding(IRFunction *ir) {
    int optimized = 0;
    bool changed = true;
    
    while (changed) {
        changed = false;
        
        for (int b = 0; b < ir->block_count; b++) {
            IRBasicBlock *block = &ir->blocks[b];
            
            for (int i = 0; i < block->instr_count - 2; i++) {
                IRInstr *i1 = &block->instrs[i];
                IRInstr *i2 = &block->instrs[i + 1];
                IRInstr *i3 = &block->instrs[i + 2];
                
                /* Pattern: CONST_I32 + CONST_I32 + binary_op */
                if (i1->op == IR_CONST_I32 && i2->op == IR_CONST_I32) {
                    int32_t a = i1->operand.i32;
                    int32_t b = i2->operand.i32;
                    int32_t result = 0;
                    bool can_fold = true;
                    
                    switch (i3->op) {
                        case IR_ADD_I32: result = a + b; break;
                        case IR_SUB_I32: result = a - b; break;
                        case IR_MUL_I32: result = a * b; break;
                        case IR_DIV_I32: 
                            if (b != 0) result = a / b; 
                            else can_fold = false;
                            break;
                        case IR_MOD_I32:
                            if (b != 0) result = a % b;
                            else can_fold = false;
                            break;
                        case IR_AND: result = a & b; break;
                        case IR_OR:  result = a | b; break;
                        case IR_XOR: result = a ^ b; break;
                        case IR_SHL: result = a << (b & 31); break;
                        case IR_SAR: result = a >> (b & 31); break;
                        case IR_SHR: result = (int32_t)((uint32_t)a >> (b & 31)); break;
                        case IR_LT_I32: result = a < b ? 1 : 0; break;
                        case IR_LE_I32: result = a <= b ? 1 : 0; break;
                        case IR_GT_I32: result = a > b ? 1 : 0; break;
                        case IR_GE_I32: result = a >= b ? 1 : 0; break;
                        case IR_EQ_I32: result = a == b ? 1 : 0; break;
                        case IR_NE_I32: result = a != b ? 1 : 0; break;
                        default: can_fold = false; break;
                    }
                    
                    if (can_fold) {
                        /* Replace first CONST with result */
                        i1->operand.i32 = result;
                        /* Replace second CONST and binary op with NOP */
                        i2->op = IR_NOP;
                        i3->op = IR_NOP;
                        optimized++;
                        changed = true;
                    }
                }
                
                /* Pattern: CONST_F32 + CONST_F32 + float binary_op */
                if (i1->op == IR_CONST_F32 && i2->op == IR_CONST_F32) {
                    float a = i1->operand.f32;
                    float b = i2->operand.f32;
                    float result = 0.0f;
                    bool can_fold = true;
                    
                    switch (i3->op) {
                        case IR_ADD_F32: result = a + b; break;
                        case IR_SUB_F32: result = a - b; break;
                        case IR_MUL_F32: result = a * b; break;
                        case IR_DIV_F32: 
                            if (b != 0.0f) result = a / b; 
                            else can_fold = false;
                            break;
                        default: can_fold = false; break;
                    }
                    
                    if (can_fold) {
                        i1->operand.f32 = result;
                        i2->op = IR_NOP;
                        i3->op = IR_NOP;
                        optimized++;
                        changed = true;
                    }
                }
                
                /* Pattern: CONST_I32 + CONST_I32 + float binary_op (type promotion) */
                /* QuickJS sometimes generates this pattern */
                if (i1->op == IR_CONST_I32 && i2->op == IR_CONST_I32) {
                    float a = (float)i1->operand.i32;
                    float b = (float)i2->operand.i32;
                    float result = 0.0f;
                    bool can_fold = true;
                    
                    switch (i3->op) {
                        case IR_ADD_F32: result = a + b; break;
                        case IR_SUB_F32: result = a - b; break;
                        case IR_MUL_F32: result = a * b; break;
                        case IR_DIV_F32: 
                            if (b != 0.0f) result = a / b; 
                            else can_fold = false;
                            break;
                        default: can_fold = false; break;
                    }
                    
                    if (can_fold) {
                        /* Convert to float constant */
                        i1->op = IR_CONST_F32;
                        i1->operand.f32 = result;
                        i2->op = IR_NOP;
                        i3->op = IR_NOP;
                        optimized++;
                        changed = true;
                    }
                }
            }
        }
    }
    
    return optimized;
}

/*
 * Loop Increment Optimization.
 * Detects patterns like: LOAD_LOCAL(i) + CONST_I32(n) + ADD_I32 + STORE_LOCAL(i)
 * and replaces with: IR_ADD_LOCAL_CONST(i, n)
 * This generates more efficient code: LW + ADDIU + SW instead of LW + ADDIU + ADDU + SW
 */
static int optimize_ir_loop_increments(IRFunction *ir) {
    int optimized = 0;
    
    for (int b = 0; b < ir->block_count; b++) {
        IRBasicBlock *block = &ir->blocks[b];
        
        /* Need at least 4 instructions for this pattern */
        for (int i = 0; i < block->instr_count - 3; i++) {
            IRInstr *i1 = &block->instrs[i];
            IRInstr *i2 = &block->instrs[i + 1];
            IRInstr *i3 = &block->instrs[i + 2];
            IRInstr *i4 = &block->instrs[i + 3];
            
            /* Pattern: LOAD_LOCAL + CONST_I32 + ADD_I32 + STORE_LOCAL (same local) */
            if (i1->op == IR_LOAD_LOCAL &&
                i2->op == IR_CONST_I32 &&
                i3->op == IR_ADD_I32 &&
                i4->op == IR_STORE_LOCAL &&
                i1->operand.local_idx == i4->operand.local_idx) {
                
                int local_idx = i1->operand.local_idx;
                int32_t const_val = i2->operand.i32;
                
                /* Replace with IR_ADD_LOCAL_CONST */
                i1->op = IR_ADD_LOCAL_CONST;
                i1->operand.local_add.local_idx = local_idx;
                i1->operand.local_add.const_val = const_val;
                
                /* Mark remaining instructions as NOP */
                i2->op = IR_NOP;
                i3->op = IR_NOP;
                i4->op = IR_NOP;
                
                optimized++;
            }
        }
    }
    
    return optimized;
}

/*
 * Loop Invariant Code Motion (LICM).
 * Detects computations in loops that don't depend on loop variables
 * and hoists them to before the loop.
 * 
 * Pattern detected:
 * - CONST_I32/CONST_F32 inside a block that has a backward jump (loop)
 * - The constant value is invariant (same every iteration)
 * 
 * This is a simplified version that just looks for constants in loop blocks.
 */
static int optimize_ir_licm(IRFunction *ir) {
    int hoisted = 0;
    
    /* Identify loop blocks by looking for backward jumps */
    for (int b = 0; b < ir->block_count; b++) {
        IRBasicBlock *block = &ir->blocks[b];
        bool is_loop_body = false;
        
        /* Check if this block has a backward jump (jump to earlier block) */
        for (int i = 0; i < block->instr_count; i++) {
            IRInstr *instr = &block->instrs[i];
            if ((instr->op == IR_JUMP || instr->op == IR_JUMP_IF_TRUE || 
                 instr->op == IR_JUMP_IF_FALSE) &&
                instr->operand.label_id <= b) {
                is_loop_body = true;
                break;
            }
        }
        
        if (!is_loop_body || block->instr_count < 3) continue;
        
        /* Look for consecutive CONST + binop patterns that could be folded
         * In a loop, repeated CONST loads can be hoisted, but our current
         * architecture doesn't easily support moving between blocks.
         * 
         * Instead, we mark duplicate constants for CSE awareness */
        
        /* For now, just identify and count potential hoisting opportunities */
        int const_count = 0;
        for (int i = 0; i < block->instr_count; i++) {
            IRInstr *instr = &block->instrs[i];
            if (instr->op == IR_CONST_I32 || instr->op == IR_CONST_F32) {
                const_count++;
            }
        }
        
        /* If there are multiple constants in a loop block, it might benefit
         * from hoisting. This is a placeholder for future enhancement. */
        if (const_count > 2) {
            /* Mark as optimization candidate - future: actually hoist */
            hoisted += 0;  /* Placeholder - actual hoisting would increment */
        }
    }
    
    return hoisted;
}

/*
 * Common Subexpression Elimination (CSE).
 * Detects repeated computations and reuses cached results.
 * Currently handles: CONST_I32, CONST_F32 deduplication.
 */
static int optimize_ir_cse(IRFunction *ir) {
    int eliminated = 0;
    
    for (int b = 0; b < ir->block_count; b++) {
        IRBasicBlock *block = &ir->blocks[b];
        
        /* Track last seen constants per block */
        typedef struct {
            IROp op;
            union { int32_t i32; float f32; } val;
            int instr_idx;
            bool valid;
        } CachedConst;
        
        CachedConst cached[16];  /* Small cache for recent constants */
        int cache_count = 0;
        
        for (int i = 0; i < block->instr_count; i++) {
            IRInstr *instr = &block->instrs[i];
            
            /* Check for duplicate constants */
            if (instr->op == IR_CONST_I32 || instr->op == IR_CONST_F32) {
                bool found = false;
                
                for (int c = 0; c < cache_count && !found; c++) {
                    if (cached[c].valid && cached[c].op == instr->op) {
                        if (instr->op == IR_CONST_I32 && 
                            cached[c].val.i32 == instr->operand.i32) {
                            /* Duplicate int constant - could mark for CSE */
                            /* For now, just track for potential DUP optimizations */
                            found = true;
                        } else if (instr->op == IR_CONST_F32 && 
                                   cached[c].val.f32 == instr->operand.f32) {
                            /* Duplicate float constant */
                            found = true;
                        }
                    }
                }
                
                /* Add to cache */
                if (cache_count < 16) {
                    cached[cache_count].op = instr->op;
                    if (instr->op == IR_CONST_I32) {
                        cached[cache_count].val.i32 = instr->operand.i32;
                    } else {
                        cached[cache_count].val.f32 = instr->operand.f32;
                    }
                    cached[cache_count].instr_idx = i;
                    cached[cache_count].valid = true;
                    cache_count++;
                }
            }
            
            /* Invalidate cache on control flow or stores */
            if (instr->op == IR_STORE_LOCAL || instr->op == IR_STORE_ARRAY ||
                instr->op == IR_JUMP || instr->op == IR_JUMP_IF_TRUE ||
                instr->op == IR_JUMP_IF_FALSE || instr->op == IR_CALL ||
                instr->op == IR_TAIL_CALL || instr->op == IR_CALL_C_FUNC) {
                cache_count = 0;  /* Clear cache */
            }
        }
    }
    
    return eliminated;
}

/*
 * Compact IR by removing IR_NOP instructions.
 * This allows subsequent optimization passes to see consecutive instructions.
 */
static void compact_ir(IRFunction *ir) {
    for (int b = 0; b < ir->block_count; b++) {
        IRBasicBlock *block = &ir->blocks[b];
        int write_idx = 0;
        
        for (int read_idx = 0; read_idx < block->instr_count; read_idx++) {
            if (block->instrs[read_idx].op != IR_NOP) {
                if (write_idx != read_idx) {
                    block->instrs[write_idx] = block->instrs[read_idx];
                }
                write_idx++;
            }
        }
        
        block->instr_count = write_idx;
    }
}

/* ============================================
 * Bytecode to IR Translation
 * ============================================ */
/*
 * Reorder IR blocks in execution order using depth-first search.
 * This ensures blocks are emitted in the correct execution order,
 * not creation order, which is critical for correct control flow.
 */
static void reorder_blocks_by_execution(IRFunction *ir) {
    if (!ir || ir->block_count <= 1) return;
    
    /* Allocate visited array and new block order array */
    int *visited = (int *)calloc(ir->block_count, sizeof(int));
    IRBasicBlock *ordered = (IRBasicBlock *)malloc(ir->block_count * sizeof(IRBasicBlock));
    int ordered_count = 0;
    
    if (!visited || !ordered) {
        free(visited);
        free(ordered);
        return; /* Keep original order on allocation failure */
    }
    
    /* DFS stack for iterative traversal */
    int *stack = (int *)malloc(ir->block_count * sizeof(int));
    int stack_top = 0;
    
    if (!stack) {
        free(visited);
        free(ordered);
        return;
    }
    
    /* Push block 0 to start DFS */
    stack[stack_top++] = 0;
    
    /* Iterative DFS */
    while (stack_top > 0) {
        int block_idx = stack[--stack_top];
        
        /* Skip if already visited */
        if (visited[block_idx]) continue;
        
        /* Mark as visited and add to ordered list */
        visited[block_idx] = 1;
        ordered[ordered_count++] = ir->blocks[block_idx];
        
        /* Push successors to stack (in reverse order so they're processed in correct order) */
        IRBasicBlock *block = &ir->blocks[block_idx];
        
        /* Check last instruction for jumps */
        if (block->instr_count > 0) {
            IRInstr *last = &block->instrs[block->instr_count - 1];
            
            if (last->op == IR_JUMP) {
                /* Unconditional jump - push target */
                int target = last->operand.label_id;
                if (target >= 0 && target < ir->block_count && !visited[target]) {
                    stack[stack_top++] = target;
                }
            } else if (last->op == IR_JUMP_IF_FALSE || last->op == IR_JUMP_IF_TRUE) {
                /* Conditional jump - push both fallthrough and target */
                int target = last->operand.label_id;
                
                /* Push target first (will be processed second due to stack) */
                if (target >= 0 && target < ir->block_count && !visited[target]) {
                    stack[stack_top++] = target;
                }
                
                /* Push fallthrough (next block) second (will be processed first) */
            int fallthrough = ir->blocks[block_idx].successors[1];
            if (fallthrough >= 0 && fallthrough < ir->block_count && !visited[fallthrough]) {
                stack[stack_top++] = fallthrough;
            }
            } else if (last->op != IR_RETURN && last->op != IR_RETURN_VOID) {
                /* No explicit jump - fallthrough to next block */
                int fallthrough = block_idx + 1;
                if (fallthrough < ir->block_count && !visited[fallthrough]) {
                    stack[stack_top++] = fallthrough;
                }
            }
        } else {
            /* Empty block - fallthrough to next */
            int fallthrough = block_idx + 1;
            if (fallthrough < ir->block_count && !visited[fallthrough]) {
                stack[stack_top++] = fallthrough;
            }
        }
    }
    
    /* Add any unreachable blocks at the end (defensive programming) */
    for (int i = 0; i < ir->block_count; i++) {
        if (!visited[i]) {
            ordered[ordered_count++] = ir->blocks[i];
        }
    }
    
    /* Replace blocks array with reordered version */
    memcpy(ir->blocks, ordered, ordered_count * sizeof(IRBasicBlock));
    
    /* Update block label IDs to reflect new positions */
    for (int i = 0; i < ordered_count; i++) {
        ir->blocks[i].label_id = i;
    }
    
    /* Update all jump targets in instructions to point to new block indices */
    int *old_to_new = (int *)malloc(ir->block_count * sizeof(int));
    if (old_to_new) {
        /* Build mapping from old block IDs to new positions
         * We need to do this BEFORE updating label_ids */
        for (int i = 0; i < ir->block_count; i++) {
            old_to_new[i] = -1; /* Initialize */
        }
        
        for (int i = 0; i < ordered_count; i++) {
            int original_id = ordered[i].label_id;
            if (original_id >= 0 && original_id < ir->block_count) {
                old_to_new[original_id] = i;
            }
        }
        
        /* Now update block label IDs to reflect new positions */
        for (int i = 0; i < ordered_count; i++) {
            ir->blocks[i].label_id = i;
        }
        
        /* Update all jump instructions */
        for (int i = 0; i < ordered_count; i++) {
            IRBasicBlock *block = &ir->blocks[i];
            for (int j = 0; j < block->instr_count; j++) {
                IRInstr *instr = &block->instrs[j];
                if (instr->op == IR_JUMP || instr->op == IR_JUMP_IF_FALSE || instr->op == IR_JUMP_IF_TRUE) {
                    int old_target = instr->operand.label_id;
                    if (old_target >= 0 && old_target < ir->block_count && old_to_new[old_target] >= 0) {
                        instr->operand.label_id = old_to_new[old_target];
                    }
                }
            }
            
            /* Update successors */
            for (int j = 0; j < 2; j++) {
                int old_succ = block->successors[j];
                if (old_succ >= 0 && old_succ < ir->block_count && old_to_new[old_succ] >= 0) {
                    block->successors[j] = old_to_new[old_succ];
                } else {
                    block->successors[j] = -1; /* Invalid or removed */
                }
            }
        }
        
        free(old_to_new);
    } else {
        /* If allocation failed, at least update label IDs */
        for (int i = 0; i < ordered_count; i++) {
            ir->blocks[i].label_id = i;
        }
    }
    
    /* Cleanup */
    free(visited);
    free(ordered);
    free(stack);
}

/*
 * Translate QuickJS bytecode to IR.
 * This handles a subset of opcodes suitable for numeric computation.
 */
static int bytecode_to_ir_impl(NativeCompiler *nc, const uint8_t *bytecode,
                               size_t bytecode_len, IRFunction *ir_out,
                               const NCObjectFlow *objects) {
    if (!nc || !bytecode || !ir_out) return -1;
    
    /* Preserve signature before initializing (ir_func_init zeros everything) */
    NativeFuncSignature saved_sig = ir_out->sig;
    
    ir_func_free(ir_out);
    ir_func_init(ir_out);
    
    /* Restore signature after initialization */
    ir_out->sig = saved_sig;
    for (int i = 0; i < NC_MAX_LOCALS; i++) {
        NativeType t = objects->locals[i].type;
        if (t != NATIVE_TYPE_UNKNOWN && t != NATIVE_TYPE_VOID && t != NATIVE_TYPE_INT32)
            ir_out->local_types[i] = t;
    }
    
    /* Initialize pending intrinsic state (IR_NOP is not 0, so explicit init needed) */
    nc->pending_intrinsic_op = IR_NOP;
    nc->pending_intrinsic_entry = NULL;
    nc->pending_intrinsic_elem_type = NATIVE_TYPE_UNKNOWN;
    nc->pending_intrinsic_depth = 0;
    
    /* Note: local_struct_defs is populated by ath_native.c with per-argument struct defs
     * before calling native_compile_function. Do NOT overwrite here. */
    
    /* Initialize field access tracking */
    nc->last_loaded_local = -1;
    for (int i = 0; i < NC_MAX_STACK_DEPTH; i++) {
        nc->stack_local_source[i] = -1;
    }
    
    /* CRITICAL FIX: Use full PC for label mapping instead of just last 8 bits.
     * The original code used (pc & 0xFF) which causes collisions when bytecode > 256 bytes.
     * This was causing loops to jump to wrong locations, especially in nested loops. */
    
    /* Allocate dynamic arrays for jump targets and label mapping */
    #define MAX_BYTECODE_SIZE 4096
    size_t map_size = bytecode_len < MAX_BYTECODE_SIZE ? bytecode_len : MAX_BYTECODE_SIZE;
    
    /* First pass: identify all jump targets */
    int *jump_targets = (int *)calloc(map_size, sizeof(int));
    if (!jump_targets) {
        nc->has_error = true;
        snprintf(nc->error_msg, sizeof(nc->error_msg), "Failed to allocate jump_targets");
        return -1;
    }
    
    NC_LOG("prescan: begin bytecode_len=%zu map_size=%zu", bytecode_len, map_size);
    size_t scan_pc = 0;
    while (scan_pc < bytecode_len) {
        uint8_t op = bytecode[scan_pc++];
        int op_size = get_opcode_info(op)->size;
        NC_LOG("prescan: pc=%zu op=%u size=%d", scan_pc - 1, op, op_size);
        
        if (op == OP_goto8 || op == OP_goto16 || op == OP_goto) {
            size_t target_pc;
            if (op == OP_goto8) {
                /* Relative to the offset byte's own position, not the byte
                 * after it - see the matching fix/comment on OP_goto8 in the
                 * main decode loop below for why. */
                int8_t offset = (int8_t)bytecode[scan_pc++];
                target_pc = (scan_pc - 1) + offset;
            } else if (op == OP_goto16) {
                int16_t offset = (int16_t)(bytecode[scan_pc] | (bytecode[scan_pc+1] << 8));
                scan_pc += 2;
                target_pc = (scan_pc - 2) + offset;
            } else {
                int32_t offset = (int32_t)(bytecode[scan_pc] | (bytecode[scan_pc+1] << 8) |
                                          (bytecode[scan_pc+2] << 16) | (bytecode[scan_pc+3] << 24));
                scan_pc += 4;
                target_pc = (scan_pc - 4) + offset;
            }
            
            if (target_pc < bytecode_len && target_pc < map_size) {
                /* Find the start of the opcode that contains target_pc */
                size_t opcode_start = target_pc;
                if (target_pc > 0) {
                    /* Search backwards to find the opcode that contains target_pc.
                     * NOTE: check_pc is size_t; iterate with a +1 bias so that
                     * reaching 0 doesn't underflow into SIZE_MAX (infinite loop). */
                    size_t search_limit = (target_pc > 10) ? (target_pc - 10) : 0;
                    /* +1 bias: check_pc-- makes the first body iteration use
                     * check_pc==target_pc itself (any instruction trivially
                     * "contains" its own start byte, size>=1) before searching
                     * backwards. Without this, the loop's first candidate was
                     * target_pc-1, so a target_pc that IS already a valid,
                     * aligned opcode start (the common case) was never even
                     * considered - the search would walk into a PRECEDING
                     * instruction's operand bytes instead, misinterpret that
                     * garbage byte as an opcode, and could land on a
                     * completely wrong block if that fake "opcode" happened to
                     * report a size reaching past target_pc. This produced a
                     * real infinite loop: a loop's back-edge jump landing on
                     * the function's entry block instead of the loop-condition
                     * block re-initializes every local (including the loop
                     * counter) every "iteration", so the exit condition never
                     * becomes true. */
                    for (size_t check_pc = target_pc + 1; check_pc-- > search_limit; ) {
                        uint8_t check_op = bytecode[check_pc];
                        int check_size = get_opcode_info(check_op)->size;
                        if (check_pc + check_size > target_pc) {
                            /* This opcode contains target_pc */
                            opcode_start = check_pc;
                            break;
                        }
                    }
                }
                jump_targets[opcode_start] = 1;  /* FIX: Use full PC, not & 0xFF */
            }
        } else if (op == OP_if_false8 || op == OP_if_true8 || op == OP_if_false || op == OP_if_true) {
            size_t target_pc;
            if (op == OP_if_false8 || op == OP_if_true8) {
                /* Same "relative to the offset byte itself" fix as OP_goto8. */
                int8_t offset = (int8_t)bytecode[scan_pc++];
                target_pc = (scan_pc - 1) + offset;
            } else {
                int32_t offset = (int32_t)(bytecode[scan_pc] | (bytecode[scan_pc+1] << 8) |
                                          (bytecode[scan_pc+2] << 16) | (bytecode[scan_pc+3] << 24));
                scan_pc += 4;
                target_pc = (scan_pc - 4) + offset;
            }

            if (target_pc < bytecode_len && target_pc < map_size) {
                size_t opcode_start = target_pc;
                if (target_pc > 0) {
                    size_t search_limit = (target_pc > 10) ? (target_pc - 10) : 0;
                    /* +1 bias: check_pc-- makes the first body iteration use
                     * check_pc==target_pc itself (any instruction trivially
                     * "contains" its own start byte, size>=1) before searching
                     * backwards. Without this, the loop's first candidate was
                     * target_pc-1, so a target_pc that IS already a valid,
                     * aligned opcode start (the common case) was never even
                     * considered - the search would walk into a PRECEDING
                     * instruction's operand bytes instead, misinterpret that
                     * garbage byte as an opcode, and could land on a
                     * completely wrong block if that fake "opcode" happened to
                     * report a size reaching past target_pc. This produced a
                     * real infinite loop: a loop's back-edge jump landing on
                     * the function's entry block instead of the loop-condition
                     * block re-initializes every local (including the loop
                     * counter) every "iteration", so the exit condition never
                     * becomes true. */
                    for (size_t check_pc = target_pc + 1; check_pc-- > search_limit; ) {
                        uint8_t check_op = bytecode[check_pc];
                        int check_size = get_opcode_info(check_op)->size;
                        if (check_pc + check_size > target_pc) {
                            opcode_start = check_pc;
                            break;
                        }
                    }
                }
                jump_targets[opcode_start] = 1;  /* FIX: Use full PC, not & 0xFF */
            }
        } else {
            /* Guard: never let a bogus size-0 opcode move scan_pc backwards. */
            scan_pc += (op_size > 1) ? (size_t)(op_size - 1) : 0;
        }
    }
    
    /* Create initial basic block */
    int current_block = ir_create_block(ir_out);
    if (current_block < 0) {
        free(jump_targets);
        return -1;
    }
    
    size_t pc = 0;
    int *label_map = (int *)malloc(map_size * sizeof(int));  /* FIX: Use full-size map */
    if (!label_map) {
        nc->has_error = true;
        snprintf(nc->error_msg, sizeof(nc->error_msg), "Failed to allocate label_map");
        free(jump_targets);
        return -1;
    }
    
    /* Initialize all labels to -1 */
    for (size_t i = 0; i < map_size; i++) {
        label_map[i] = -1;
    }
    
    /* Map PC 0 to the initial block */
    label_map[0] = current_block;
    
    NC_LOG("decode: begin bytecode_len=%zu", bytecode_len);
    while (pc < bytecode_len) {
        uint8_t op = bytecode[pc++];
        int op_size = get_opcode_info(op)->size;
        NC_LOG("decode: pc=%zu op=%u size=%d block=%d", pc - 1, op, op_size, current_block);
        
        /* Map current PC to current block (for future jumps to this location) */
        size_t current_pc = pc - 1;
        
        /* FIX: Use full PC instead of & 0xFF */
        if (current_pc < map_size) {
            /* If this PC is a jump target, we need to create a new block for it */
            if (jump_targets[current_pc]) {
                if (label_map[current_pc] < 0) {
                    /* First time we see this jump target, create a new block.
                     * We got here by simple sequential fallthrough from
                     * prev_block (no explicit goto/if_false - those already
                     * set successors[0]/[1] themselves), so prev_block's
                     * successors[1] must record this transition, exactly like
                     * the "already processed" branch below does. Missing this
                     * left successors[1] at ir_create_block()'s default of -1,
                     * which made compute_block_entry_depths() (see ir_to_native)
                     * treat prev_block as having no successor at all - hiding
                     * every value it pushed for a later merge block from the
                     * stack-depth propagation. */
                    int prev_block = current_block;
                    current_block = ir_create_block(ir_out);
                    label_map[current_pc] = current_block;
                    if (block_falls_through(ir_out, prev_block)) {
                        ir_out->blocks[prev_block].successors[1] = current_block;
                    }
                } else if (label_map[current_pc] != current_block) {
                    /* This PC is a jump target that was already processed in a different block.
                     * We need to switch to that block to split the current block.
                     *
                     * We're reaching it by falling off the end of current_block's
                     * bytecode (no explicit goto/if_false got us here - those
                     * already set successors[0]/[1] themselves), so current_block
                     * genuinely falls through into it at runtime. ir_create_block()
                     * defaulted current_block's successors[1] to current_block+1,
                     * which is very often just wrong here (e.g. a ternary's
                     * else-branch is discovered as a NEW, higher-numbered block
                     * while its merge point - the then-branch's goto target - was
                     * already discovered earlier as a LOWER-numbered one; the
                     * final code layout is strictly by block index, so "the next
                     * index" and "the block we actually continue into" are two
                     * different things). ir_to_native only inserts an explicit
                     * jump when successors[1] says to, and leaves alone any block
                     * that already ends in an unconditional jump/return - so
                     * fixing this up here is always correct, never redundant. */
                    int prev_block = current_block;
                    current_block = label_map[current_pc];
                    if (block_falls_through(ir_out, prev_block)) {
                        ir_out->blocks[prev_block].successors[1] = current_block;
                    }
                }
                /* If label_map[current_pc] == current_block, we're already in the correct block */
            } else if (label_map[current_pc] < 0) {
                /* First time we see this PC, map it to current block */
                label_map[current_pc] = current_block;
            } else if (label_map[current_pc] != current_block) {
                /* This PC is a target from a previous jump, switch to that block -
                 * same fallthrough-successor fixup as above. */
                int prev_block = current_block;
                current_block = label_map[current_pc];
                if (block_falls_through(ir_out, prev_block)) {
                    ir_out->blocks[prev_block].successors[1] = current_block;
                }
            }
        }
        
        /* Map all bytes of this opcode to the current block (for jumps to operands) */
        for (int i = 1; i < op_size && (current_pc + i) < bytecode_len && (current_pc + i) < map_size; i++) {
            size_t operand_pc = current_pc + i;  /* FIX: Use full PC */
            if (label_map[operand_pc] < 0) {
                label_map[operand_pc] = current_block;
            }
        }
        
        NCObjectType object = {0};
        if (objects->offsets[current_pc] >= 0)
            object = objects->values[objects->offsets[current_pc]];

        switch (op) {
            /* Push integer constants */
            case OP_push_i32: {
                int32_t val = (int32_t)(bytecode[pc] | (bytecode[pc+1] << 8) |
                              (bytecode[pc+2] << 16) | (bytecode[pc+3] << 24));
                pc += 4;
                ir_emit_const_i32(ir_out, current_block, val);
                break;
            }
            
            /* Short push opcodes */
            #ifdef SHORT_OPCODES
            case OP_push_minus1: ir_emit_const_i32(ir_out, current_block, -1); break;
            case OP_push_1: ir_emit_const_i32(ir_out, current_block, 1); break;
            case OP_push_2: ir_emit_const_i32(ir_out, current_block, 2); break;
            case OP_push_3: ir_emit_const_i32(ir_out, current_block, 3); break;
            case OP_push_4: ir_emit_const_i32(ir_out, current_block, 4); break;
            case OP_push_5: ir_emit_const_i32(ir_out, current_block, 5); break;
            case OP_push_6: ir_emit_const_i32(ir_out, current_block, 6); break;
            case OP_push_7: ir_emit_const_i32(ir_out, current_block, 7); break;
            
            case OP_push_i8: {
                int8_t val = (int8_t)bytecode[pc++];
                ir_emit_const_i32(ir_out, current_block, val);
                break;
            }
            
            case OP_push_i16: {
                int16_t val = (int16_t)(bytecode[pc] | (bytecode[pc+1] << 8));
                pc += 2;
                ir_emit_const_i32(ir_out, current_block, val);
                break;
            }
            #endif
            
            /* Short push opcodes - handle even if SHORT_OPCODES not defined (bytecode may use them) */
            case OP_push_0: 
                ir_emit_const_i32(ir_out, current_block, 0); 
                break;
            
            /* Other push opcodes (always available) */
            case OP_undefined:
                ir_emit_const_i32(ir_out, current_block, 0);  /* undefined treated as 0 */
                break;
                
            case OP_push_false:
                ir_emit_const_i32(ir_out, current_block, 0);
                break;
                
            case OP_push_true:
                ir_emit_const_i32(ir_out, current_block, 1);
                break;
            
            /* Argument access - arguments are stored at the beginning of locals */
            case OP_get_arg: {
                if (pc + 1 >= bytecode_len) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "get_arg: bytecode overflow at pc=%zu", pc);
                    return -1;
                }
                uint16_t idx = bytecode[pc] | (bytecode[pc+1] << 8);
                pc += 2;
                if (idx < NC_MAX_LOCALS && idx < 256) {  /* Sanity check: max 256 locals */
                    ir_emit_load_local(ir_out, current_block, idx, ir_out->local_types[idx]);
                    nc->last_loaded_local = idx;  /* Track for struct field access */
                    if (idx >= ir_out->local_count) ir_out->local_count = idx + 1;
                } else {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "get_arg: invalid index %d at pc=%zu", idx, pc - 2);
                    return -1;
                }
                break;
            }
            
            case OP_put_arg: {
                if (pc + 1 >= bytecode_len) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "put_arg: bytecode overflow at pc=%zu", pc);
                    return -1;
                }
                uint16_t idx = bytecode[pc] | (bytecode[pc+1] << 8);
                pc += 2;
                if (idx < NC_MAX_LOCALS && idx < 256) {
                    ir_emit_store_local(ir_out, current_block, idx, NATIVE_TYPE_UNKNOWN);
                    if (idx >= ir_out->local_count) ir_out->local_count = idx + 1;
                } else {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "put_arg: invalid index %d at pc=%zu", idx, pc - 2);
                    return -1;
                }
                break;
            }
            
            /* Short opcodes for argument access (only available if SHORT_OPCODES is defined) */
            /* CRITICAL: These must come BEFORE OP_get_loc8 in the switch statement!
             * The QuickJS output shows OP_get_arg0=0xd1 and OP_get_arg1=0xd2,
             * which means these opcodes have the same values as OP_get_loc8 and OP_put_loc8.
             * By checking these cases first, we ensure they are matched correctly. */
            #ifdef SHORT_OPCODES
            case OP_get_arg0: ir_emit_load_local(ir_out, current_block, 0, ir_out->local_types[0]); nc->last_loaded_local = 0; break;
            case OP_get_arg1: ir_emit_load_local(ir_out, current_block, 1, ir_out->local_types[1]); nc->last_loaded_local = 1; break;
            case OP_get_arg2: ir_emit_load_local(ir_out, current_block, 2, ir_out->local_types[2]); nc->last_loaded_local = 2; break;
            case OP_get_arg3: ir_emit_load_local(ir_out, current_block, 3, ir_out->local_types[3]); nc->last_loaded_local = 3; break;
            
            case OP_put_arg0: ir_emit_store_local(ir_out, current_block, 0, NATIVE_TYPE_UNKNOWN); break;
            case OP_put_arg1: ir_emit_store_local(ir_out, current_block, 1, NATIVE_TYPE_UNKNOWN); break;
            case OP_put_arg2: ir_emit_store_local(ir_out, current_block, 2, NATIVE_TYPE_UNKNOWN); break;
            case OP_put_arg3: ir_emit_store_local(ir_out, current_block, 3, NATIVE_TYPE_UNKNOWN); break;
            
            case OP_set_arg0: case OP_set_arg1: case OP_set_arg2: case OP_set_arg3: {
                int idx = op - OP_set_arg0;
                IRInstr dup_instr = { .op = IR_DUP, .type = NATIVE_TYPE_UNKNOWN };
                ir_emit(ir_out, current_block, &dup_instr);
                ir_emit_store_local(ir_out, current_block, idx, NATIVE_TYPE_UNKNOWN);
                break;
            }
            #endif
            
            /* Local variable access - locals come after arguments */
            case OP_get_loc: {
                /* CRITICAL: OP_get_loc accesses var_buf[idx], not arg_buf[idx]. */
                if (pc + 1 >= bytecode_len) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "get_loc: bytecode overflow at pc=%zu", pc);
                    return -1;
                }
                uint16_t var_idx = bytecode[pc] | (bytecode[pc+1] << 8);
                pc += 2;
                /* Map var_buf[idx] to local[arg_count + idx] */
                int local_idx = ir_out->sig.arg_count + var_idx;
                if (local_idx < NC_MAX_LOCALS && local_idx < 256) {
                    ir_emit_load_local(ir_out, current_block, local_idx, ir_out->local_types[local_idx]);
                    if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                } else {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "get_loc: invalid mapped index %d (var_idx=%d, arg_count=%d) at pc=%zu", 
                            local_idx, var_idx, ir_out->sig.arg_count, pc - 2);
                    return -1;
                }
                break;
            }
            
            case OP_put_loc: {
                /* CRITICAL: OP_put_loc accesses var_buf[idx], not arg_buf[idx]. */
                if (pc + 1 >= bytecode_len) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "put_loc: bytecode overflow at pc=%zu", pc);
                    return -1;
                }
                uint16_t var_idx = bytecode[pc] | (bytecode[pc+1] << 8);
                pc += 2;
                /* Map var_buf[idx] to local[arg_count + idx] */
                int local_idx = ir_out->sig.arg_count + var_idx;
                if (local_idx < NC_MAX_LOCALS && local_idx < 256) {
                    ir_emit_store_local(ir_out, current_block, local_idx, NATIVE_TYPE_UNKNOWN);
                    if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                } else {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "put_loc: invalid mapped index %d (var_idx=%d, arg_count=%d) at pc=%zu", 
                            local_idx, var_idx, ir_out->sig.arg_count, pc - 2);
                    return -1;
                }
                break;
            }
            
            case OP_set_loc: {
                /* CRITICAL: OP_set_loc accesses var_buf[idx], not arg_buf[idx]. */
                uint16_t var_idx = bytecode[pc] | (bytecode[pc+1] << 8);
                pc += 2;
                /* Map var_buf[idx] to local[arg_count + idx] */
                int local_idx = ir_out->sig.arg_count + var_idx;
                if (local_idx < NC_MAX_LOCALS) {
                    /* set_loc leaves value on stack, unlike put_loc */
                    /* store, then reload: see OP_set_loc0 */
                    ir_emit_store_local(ir_out, current_block, local_idx, NATIVE_TYPE_UNKNOWN);
                    ir_emit_load_local(ir_out, current_block, local_idx, ir_out->local_types[local_idx]);
                    if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                }
                break;
            }
            
            /* According to opcodes.txt: OP_set_loc_uninitialized=97, OP_get_loc_check=98, OP_put_loc_check=99 */
            case OP_set_loc_uninitialized: {
                /* Set a local variable to uninitialized state
                 * CRITICAL: OP_set_loc_uninitialized accesses var_buf[idx], not arg_buf[idx].
                 * In our system, arguments are in local[0..arg_count-1] and
                 * variables are in local[arg_count..arg_count+var_count-1].
                 * So we need to add arg_count to the index. */
                if (pc + 1 >= bytecode_len) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "set_loc_uninitialized: bytecode overflow at pc=%zu", pc);
                    return -1;
                }
                uint16_t var_idx = bytecode[pc] | (bytecode[pc+1] << 8);
                pc += 2;
                /* Map var_buf[idx] to local[arg_count + idx] */
                int local_idx = ir_out->sig.arg_count + var_idx;
                /* Mark local as uninitialized - for now just track it */
                if (local_idx < NC_MAX_LOCALS) {
                    if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                }
                break;
            }
            
            case OP_get_loc_check: {
                /* Get local with check - same as get_loc but with validation
                 * CRITICAL: OP_get_loc_check accesses var_buf[idx], not arg_buf[idx].
                 * In our system, arguments are in local[0..arg_count-1] and
                 * variables are in local[arg_count..arg_count+var_count-1].
                 * So we need to add arg_count to the index. */
                if (pc + 1 >= bytecode_len) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "get_loc_check: bytecode overflow at pc=%zu", pc);
                    return -1;
                }
                uint16_t var_idx = bytecode[pc] | (bytecode[pc+1] << 8);
                pc += 2;
                /* Map var_buf[idx] to local[arg_count + idx] */
                int local_idx = ir_out->sig.arg_count + var_idx;
                if (local_idx < NC_MAX_LOCALS && local_idx < 256) {
                    ir_emit_load_local(ir_out, current_block, local_idx, ir_out->local_types[local_idx]);
                    if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                } else {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "get_loc_check: invalid mapped index %d (var_idx=%d, arg_count=%d) at pc=%zu", 
                            local_idx, var_idx, ir_out->sig.arg_count, pc - 2);
                    return -1;
                }
                break;
            }
            
            case OP_put_loc_check: {
                /* Put local with check - same as put_loc but with validation
                 * CRITICAL: OP_put_loc_check accesses var_buf[idx], not arg_buf[idx].
                 * In our system, arguments are in local[0..arg_count-1] and
                 * variables are in local[arg_count..arg_count+var_count-1].
                 * So we need to add arg_count to the index. */
                if (pc + 1 >= bytecode_len) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "put_loc_check: bytecode overflow at pc=%zu", pc);
                    return -1;
                }
                uint16_t var_idx = bytecode[pc] | (bytecode[pc+1] << 8);
                pc += 2;
                /* Map var_buf[idx] to local[arg_count + idx] */
                int local_idx = ir_out->sig.arg_count + var_idx;
                if (local_idx < NC_MAX_LOCALS && local_idx < 256) {
                    ir_emit_store_local(ir_out, current_block, local_idx, NATIVE_TYPE_UNKNOWN);
                    if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                } else {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "put_loc_check: invalid mapped index %d (var_idx=%d, arg_count=%d) at pc=%zu", 
                            local_idx, var_idx, ir_out->sig.arg_count, pc - 2);
                    return -1;
                }
                break;
            }
            
            /* Short opcodes for local variable access - handle even if SHORT_OPCODES not defined */
            /* CRITICAL: OP_put_loc0 accesses var_buf[0], not arg_buf[0]. */
            case OP_put_loc0: {
                int local_idx = ir_out->sig.arg_count + 0;  /* var_buf[0] -> local[arg_count + 0] */
                /* like get_loc: the IR must know this local exists */
                if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                ir_emit_store_local(ir_out, current_block, local_idx, NATIVE_TYPE_UNKNOWN);
                break;
            }
            
            /* Short opcodes for local variable access */
            #ifdef SHORT_OPCODES
            /* CRITICAL: OP_get_loc0-3, OP_put_loc1-3, OP_set_loc0-3 access var_buf[idx], not arg_buf[idx]. */
            case OP_get_loc0: {
                int local_idx = ir_out->sig.arg_count + 0;
                /* like get_loc: the IR must know this local exists */
                if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                ir_emit_load_local(ir_out, current_block, local_idx, ir_out->local_types[local_idx]);
                break;
            }
            case OP_get_loc1: {
                int local_idx = ir_out->sig.arg_count + 1;
                /* like get_loc: the IR must know this local exists */
                if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                ir_emit_load_local(ir_out, current_block, local_idx, ir_out->local_types[local_idx]);
                break;
            }
            case OP_get_loc2: {
                int local_idx = ir_out->sig.arg_count + 2;
                /* like get_loc: the IR must know this local exists */
                if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                ir_emit_load_local(ir_out, current_block, local_idx, ir_out->local_types[local_idx]);
                break;
            }
            case OP_get_loc3: {
                int local_idx = ir_out->sig.arg_count + 3;
                /* like get_loc: the IR must know this local exists */
                if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                ir_emit_load_local(ir_out, current_block, local_idx, ir_out->local_types[local_idx]);
                break;
            }
            
            case OP_put_loc1: {
                int local_idx = ir_out->sig.arg_count + 1;
                /* like get_loc: the IR must know this local exists */
                if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                ir_emit_store_local(ir_out, current_block, local_idx, NATIVE_TYPE_UNKNOWN);
                break;
            }
            case OP_put_loc2: {
                int local_idx = ir_out->sig.arg_count + 2;
                /* like get_loc: the IR must know this local exists */
                if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                ir_emit_store_local(ir_out, current_block, local_idx, NATIVE_TYPE_UNKNOWN);
                break;
            }
            case OP_put_loc3: {
                int local_idx = ir_out->sig.arg_count + 3;
                /* like get_loc: the IR must know this local exists */
                if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                ir_emit_store_local(ir_out, current_block, local_idx, NATIVE_TYPE_UNKNOWN);
                break;
            }
            
            case OP_set_loc0: case OP_set_loc1: case OP_set_loc2: case OP_set_loc3: {
                int var_idx = op - OP_set_loc0;
                int local_idx = ir_out->sig.arg_count + var_idx;
                /* like get_loc: the IR must know this local exists */
                if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                /* set_loc = put_loc + get_loc: store, then reload the local
                   (the dup+store form miscompiled once let/const lost their
                   TDZ checks and QuickJS started fusing put_loc/get_loc) */
                ir_emit_store_local(ir_out, current_block, local_idx, NATIVE_TYPE_UNKNOWN);
                ir_emit_load_local(ir_out, current_block, local_idx, ir_out->local_types[local_idx]);
                break;
            }
            
            case OP_get_loc8: {
                if (pc >= bytecode_len) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), 
                             "OP_get_loc8: bytecode overflow at pc=%zu", pc);
                    return -1;
                }
                
                uint8_t idx = bytecode[pc++];
                
                /* CRITICAL: QuickJS uses OP_get_loc8 to access var_buf[idx], NOT arg_buf[idx].
                 * 
                 * IMPORTANT OBSERVATION: The bytecode shows index 210 (0xd2) which is exactly OP_put_loc8.
                 * This clearly indicates that:
                 * 1. The bytecode is corrupted OR
                 * 2. We are reading the bytecode incorrectly OR
                 * 3. There is a bug in QuickJS generating incorrect bytecode
                 * 
                 * QuickJS should NOT use OP_get_loc8 for arguments - it should use OP_get_arg
                 * or OP_get_arg0-3. The fact that we're seeing OP_get_loc8 with index 210 (an opcode)
                 * suggests a fundamental problem in reading or generating the bytecode. */
                
                /* Check if the index is a valid opcode (clearly an error) */
                if (idx == OP_put_loc8 || idx == OP_set_loc8 || 
                    (idx >= OP_get_arg0 && idx <= OP_set_arg3) ||
                    (idx >= OP_get_loc0 && idx <= OP_set_loc3)) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), 
                             "OP_get_loc8: index %d (0x%02x) is a valid opcode! "
                             "This indicates bytecode corruption or read error. "
                             "QuickJS should use OP_get_arg for arguments, not OP_get_loc8. "
                             "(arg_count=%d, var_count=%d, pc=%zu, bytecode: 0x%02x 0x%02x 0x%02x 0x%02x)", 
                             idx, idx, nc->js_arg_count, nc->js_var_count, pc - 1,
                             pc >= 1 ? bytecode[pc-1] : 0,
                             pc < bytecode_len ? bytecode[pc] : 0,
                             pc + 1 < bytecode_len ? bytecode[pc+1] : 0,
                             pc + 2 < bytecode_len ? bytecode[pc+2] : 0);
                    return -1;
                }
                
                if (idx >= NC_MAX_LOCALS) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), 
                             "OP_get_loc8: invalid index %d (0x%02x) - exceeds maximum %d "
                             "(arg_count=%d, var_count=%d, pc=%zu). "
                             "OP_get_loc8 only accesses var_buf, not arg_buf!", 
                             idx, idx, NC_MAX_LOCALS, nc->js_arg_count, nc->js_var_count, pc - 1);
                    return -1;
                }
                
                /* OP_get_loc8 accesses var_buf[idx] in QuickJS.
                 * Map var_buf[idx] to local[arg_count + idx] */
                int local_idx = ir_out->sig.arg_count + idx;
                if (local_idx < NC_MAX_LOCALS) {
                    ir_emit_load_local(ir_out, current_block, local_idx, ir_out->local_types[local_idx]);
                    if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                } else {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), 
                             "OP_get_loc8: mapped index %d (var_idx=%d, arg_count=%d) exceeds maximum %d", 
                             local_idx, idx, ir_out->sig.arg_count, NC_MAX_LOCALS);
                    return -1;
                }
                break;
            }
            
            case OP_put_loc8: {
                /* CRITICAL: OP_put_loc8 accesses var_buf[idx], not arg_buf[idx]. */
                uint8_t var_idx = bytecode[pc++];
                int local_idx = ir_out->sig.arg_count + var_idx;
                if (local_idx < NC_MAX_LOCALS) {
                    ir_emit_store_local(ir_out, current_block, local_idx, NATIVE_TYPE_UNKNOWN);
                    if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                } else {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), 
                             "OP_put_loc8: mapped index %d (var_idx=%d, arg_count=%d) exceeds maximum %d", 
                             local_idx, var_idx, ir_out->sig.arg_count, NC_MAX_LOCALS);
                    return -1;
                }
                break;
            }
            
            case OP_set_loc8: {
                /* CRITICAL: OP_set_loc8 accesses var_buf[idx], not arg_buf[idx]. */
                uint8_t var_idx = bytecode[pc++];
                int local_idx = ir_out->sig.arg_count + var_idx;
                if (local_idx < NC_MAX_LOCALS) {
                    /* store, then reload: see OP_set_loc0 */
                    ir_emit_store_local(ir_out, current_block, local_idx, NATIVE_TYPE_UNKNOWN);
                    ir_emit_load_local(ir_out, current_block, local_idx, ir_out->local_types[local_idx]);
                    if (local_idx >= ir_out->local_count) ir_out->local_count = local_idx + 1;
                } else {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), 
                             "OP_set_loc8: mapped index %d (var_idx=%d, arg_count=%d) exceeds maximum %d", 
                             local_idx, var_idx, ir_out->sig.arg_count, NC_MAX_LOCALS);
                    return -1;
                }
                break;
            }
            #endif
            
            /* Arithmetic operations */
            case OP_add: ir_emit_binop(ir_out, current_block, IR_ADD_I32); break;
            case OP_sub: ir_emit_binop(ir_out, current_block, IR_SUB_I32); break;
            case OP_mul: ir_emit_binop(ir_out, current_block, IR_MUL_I32); break;
            case OP_div: ir_emit_binop(ir_out, current_block, IR_DIV_I32); break;
            case OP_mod: ir_emit_binop(ir_out, current_block, IR_MOD_I32); break;
            case OP_neg: ir_emit_binop(ir_out, current_block, IR_NEG_I32); break;
            case OP_plus: {
                /* Unary plus - convert to number (no-op for native numeric types) */
                /* Value stays on stack unchanged */
                break;
            }
            case OP_pow: {
                /* Power/exponentiation - not directly supported in IR, use multiplication approximation */
                /* For now, treat as multiplication (will need proper implementation later) */
                ir_emit_binop(ir_out, current_block, IR_MUL_I32);
                break;
            }
#ifdef CONFIG_BIGNUM
            /* These two opcodes only exist in a bignum-enabled QuickJS. */
            case OP_mul_pow10: {
                /* Multiply by power of 10 - not directly supported, use regular multiplication */
                /* For now, treat as regular multiplication */
                ir_emit_binop(ir_out, current_block, IR_MUL_I32);
                break;
            }
            case OP_math_mod: {
                /* Mathematical modulo (handles negative numbers differently) */
                /* For now, use regular modulo */
                ir_emit_binop(ir_out, current_block, IR_MOD_I32);
                break;
            }
#endif
            
            /* Bitwise operations */
            case OP_and: ir_emit_binop(ir_out, current_block, IR_AND); break;
            case OP_or:  ir_emit_binop(ir_out, current_block, IR_OR); break;
            case OP_xor: ir_emit_binop(ir_out, current_block, IR_XOR); break;
            case OP_not: ir_emit_binop(ir_out, current_block, IR_NOT); break;
            case OP_shl: ir_emit_binop(ir_out, current_block, IR_SHL); break;
            case OP_sar: ir_emit_binop(ir_out, current_block, IR_SAR); break;
            case OP_shr: ir_emit_binop(ir_out, current_block, IR_SHR); break;
            
            /* Logical operations */
            case OP_lnot: {
                /* Logical NOT - convert to boolean then negate */
                /* For native code, treat as bitwise NOT of boolean conversion */
                ir_emit_binop(ir_out, current_block, IR_NOT);
                break;
            }
            
            /* Comparison operations */
            case OP_lt:  ir_emit_binop(ir_out, current_block, IR_LT_I32); break;
            case OP_lte: ir_emit_binop(ir_out, current_block, IR_LE_I32); break;
            case OP_gt:  ir_emit_binop(ir_out, current_block, IR_GT_I32); break;
            case OP_gte: ir_emit_binop(ir_out, current_block, IR_GE_I32); break;
            case OP_eq:  ir_emit_binop(ir_out, current_block, IR_EQ_I32); break;
            case OP_neq: ir_emit_binop(ir_out, current_block, IR_NE_I32); break;
            case OP_strict_eq: {
                /* Strict equality (===) - same as regular equality for native numeric types */
                ir_emit_binop(ir_out, current_block, IR_EQ_I32);
                break;
            }
            case OP_strict_neq: {
                /* Strict inequality (!==) - same as regular inequality for native numeric types */
                ir_emit_binop(ir_out, current_block, IR_NE_I32);
                break;
            }
            case OP_instanceof: {
                /* instanceof - not supported for native code, push false */
                IRInstr drop = { .op = IR_DROP, .type = NATIVE_TYPE_VOID };
                ir_emit(ir_out, current_block, &drop);
                ir_emit(ir_out, current_block, &drop);
                ir_emit_const_i32(ir_out, current_block, 0);
                break;
            }
            case OP_in: {
                /* in operator - not supported for native code, push false */
                IRInstr drop = { .op = IR_DROP, .type = NATIVE_TYPE_VOID };
                ir_emit(ir_out, current_block, &drop);
                ir_emit(ir_out, current_block, &drop);
                ir_emit_const_i32(ir_out, current_block, 0);
                break;
            }
            case OP_is_undefined_or_null: {
                /* Check if value is undefined or null - for native code, always false */
                IRInstr drop = { .op = IR_DROP, .type = NATIVE_TYPE_VOID };
                ir_emit(ir_out, current_block, &drop);
                ir_emit_const_i32(ir_out, current_block, 0);
                break;
            }
            
            /* Increment/decrement */
            case OP_inc:
            case OP_post_inc:
                ir_emit_const_i32(ir_out, current_block, 1);
                ir_emit_binop(ir_out, current_block, IR_ADD_I32);
                break;
                
            case OP_dec:
            case OP_post_dec:
                ir_emit_const_i32(ir_out, current_block, 1);
                ir_emit_binop(ir_out, current_block, IR_SUB_I32);
                break;
            
            /* Local variable increment/decrement */
            case OP_inc_loc: {
                if (pc >= bytecode_len) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "inc_loc: bytecode overflow at pc=%zu", pc);
                    return -1;
                }
                /* loc8 operand: a var_buf index, mapped like get_loc */
                int idx = ir_out->sig.arg_count + bytecode[pc];
                pc += 1;
                if (idx < NC_MAX_LOCALS) {
                    ir_emit_load_local(ir_out, current_block, idx, ir_out->local_types[idx]);
                    ir_emit_const_i32(ir_out, current_block, 1);
                    ir_emit_binop(ir_out, current_block, IR_ADD_I32);
                    ir_emit_store_local(ir_out, current_block, idx, NATIVE_TYPE_UNKNOWN);
                    if (idx >= ir_out->local_count) ir_out->local_count = idx + 1;
                }
                break;
            }
            
            case OP_dec_loc: {
                if (pc >= bytecode_len) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "dec_loc: bytecode overflow at pc=%zu", pc);
                    return -1;
                }
                /* loc8 operand: a var_buf index, mapped like get_loc */
                int idx = ir_out->sig.arg_count + bytecode[pc];
                pc += 1;
                if (idx < NC_MAX_LOCALS) {
                    ir_emit_load_local(ir_out, current_block, idx, ir_out->local_types[idx]);
                    ir_emit_const_i32(ir_out, current_block, 1);
                    ir_emit_binop(ir_out, current_block, IR_SUB_I32);
                    ir_emit_store_local(ir_out, current_block, idx, NATIVE_TYPE_UNKNOWN);
                    if (idx >= ir_out->local_count) ir_out->local_count = idx + 1;
                }
                break;
            }
            
            case OP_add_loc: {
                if (pc >= bytecode_len) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), "add_loc: bytecode overflow at pc=%zu", pc);
                    return -1;
                }
                /* loc8 operand: a var_buf index, mapped like get_loc */
                int idx = ir_out->sig.arg_count + bytecode[pc];
                pc += 1;
                if (idx < NC_MAX_LOCALS) {
                    /* Stack: value -> add to local[idx] */
                    ir_emit_load_local(ir_out, current_block, idx, ir_out->local_types[idx]);
                    ir_emit_binop(ir_out, current_block, IR_ADD_I32);
                    ir_emit_store_local(ir_out, current_block, idx, NATIVE_TYPE_UNKNOWN);
                    if (idx >= ir_out->local_count) ir_out->local_count = idx + 1;
                }
                break;
            }
            
            /* Stack operations */
            case OP_dup: {
                IRInstr instr = { .op = IR_DUP, .type = NATIVE_TYPE_UNKNOWN };
                ir_emit(ir_out, current_block, &instr);
                break;
            }
            
            case OP_dup1: {
                /* Duplicate second element: a b -> a a b */
                /* For now, treat as regular dup */
                IRInstr instr = { .op = IR_DUP, .type = NATIVE_TYPE_UNKNOWN };
                ir_emit(ir_out, current_block, &instr);
                break;
            }
            
            case OP_dup2: {
                /* Duplicate two elements: a b -> a b a b */
                /* For now, duplicate twice */
                IRInstr instr = { .op = IR_DUP, .type = NATIVE_TYPE_UNKNOWN };
                ir_emit(ir_out, current_block, &instr);
                ir_emit(ir_out, current_block, &instr);
                break;
            }
            
            case OP_dup3: {
                /* Duplicate three elements: a b c -> a b c a b c */
                /* For now, duplicate three times */
                IRInstr instr = { .op = IR_DUP, .type = NATIVE_TYPE_UNKNOWN };
                ir_emit(ir_out, current_block, &instr);
                ir_emit(ir_out, current_block, &instr);
                ir_emit(ir_out, current_block, &instr);
                break;
            }
            
            case OP_drop: {
                IRInstr instr = { .op = IR_DROP, .type = NATIVE_TYPE_VOID };
                ir_emit(ir_out, current_block, &instr);
                break;
            }
            
            case OP_nip: {
                /* Remove second element: a b -> b */
                IRInstr swap = { .op = IR_SWAP, .type = NATIVE_TYPE_VOID };
                ir_emit(ir_out, current_block, &swap);
                IRInstr drop = { .op = IR_DROP, .type = NATIVE_TYPE_VOID };
                ir_emit(ir_out, current_block, &drop);
                break;
            }
            
            case OP_nip1: {
                /* Remove third element: a b c -> b c */
                /* For now, swap and drop */
                IRInstr swap = { .op = IR_SWAP, .type = NATIVE_TYPE_VOID };
                ir_emit(ir_out, current_block, &swap);
                IRInstr drop = { .op = IR_DROP, .type = NATIVE_TYPE_VOID };
                ir_emit(ir_out, current_block, &drop);
                break;
            }
            
            case OP_swap: {
                IRInstr instr = { .op = IR_SWAP, .type = NATIVE_TYPE_VOID };
                ir_emit(ir_out, current_block, &instr);
                break;
            }
            
            case OP_swap2: {
                /* Swap two pairs: a b c d -> c d a b */
                /* For now, treat as two swaps */
                IRInstr swap = { .op = IR_SWAP, .type = NATIVE_TYPE_VOID };
                ir_emit(ir_out, current_block, &swap);
                ir_emit(ir_out, current_block, &swap);
                break;
            }
            
            /* Control flow */
            case OP_if_false: {
                int32_t offset = (int32_t)(bytecode[pc] | (bytecode[pc+1] << 8) |
                                 (bytecode[pc+2] << 16) | (bytecode[pc+3] << 24));
                pc += 4;
                size_t target_pc = (pc - 4) + offset;  /* offset is relative to opcode start */
                
                /* Get or create block for target PC */
                int target;
                if (target_pc < bytecode_len && target_pc < map_size && label_map[target_pc] >= 0) {
                    target = label_map[target_pc];
                } else {
                    target = ir_create_block(ir_out);
                    if (target_pc < bytecode_len && target_pc < map_size) {
                        label_map[target_pc] = target;
                    }
                }
                
                /* Force a new block for fallthrough to ensure correct CFG */
                int fallthrough = ir_create_block(ir_out);
                       
                ir_emit_jump_if(ir_out, current_block, IR_JUMP_IF_FALSE, target);

                /* Set successors for current block (AFTER emit to avoid overwrite) */
                ir_out->blocks[current_block].successors[0] = target;      /* Jump target */
                ir_out->blocks[current_block].successors[1] = fallthrough; /* Fallthrough */
                
                /* Switch to fallthrough block */
                current_block = fallthrough;
                break;
            }
            
            case OP_if_true: {
                int32_t offset = (int32_t)(bytecode[pc] | (bytecode[pc+1] << 8) |
                                 (bytecode[pc+2] << 16) | (bytecode[pc+3] << 24));
                pc += 4;
                size_t target_pc = (pc - 4) + offset;  /* offset is relative to opcode start */
                
                /* Get or create block for target PC */
                int target;
                if (target_pc < bytecode_len && target_pc < map_size && label_map[target_pc] >= 0) {
                    target = label_map[target_pc];
                } else {
                    target = ir_create_block(ir_out);
                    if (target_pc < bytecode_len && target_pc < map_size) {
                        label_map[target_pc] = target;
                    }
                }
                
                /* Force a new block for fallthrough */
                int fallthrough = ir_create_block(ir_out);
                
                ir_emit_jump_if(ir_out, current_block, IR_JUMP_IF_TRUE, target);

                /* Set successors AFTER the emit, like the other three branch
                 * sites do. Setting them first meant ir_emit_jump_if's own
                 * successors[1] = block + 1 default clobbered the real
                 * fallthrough: for `if (x) continue;` inside a loop with a
                 * large body, the continue target got block+1 while the body
                 * was numbered later, so both successors pointed at the
                 * continue target, the body block ended up with no incoming
                 * edge at all, and ir_to_native emitted it as dead code. */
                ir_out->blocks[current_block].successors[0] = target;
                ir_out->blocks[current_block].successors[1] = fallthrough;
                
                /* Switch to fallthrough block */
                current_block = fallthrough;
                break;
            }
            
            case OP_goto: {
                int32_t offset = (int32_t)(bytecode[pc] | (bytecode[pc+1] << 8) |
                                 (bytecode[pc+2] << 16) | (bytecode[pc+3] << 24));
                pc += 4;
                size_t target_pc = (pc - 4) + offset;  /* offset is relative to opcode start */
                
                /* Get or create block for target PC */
                int target;
                if (target_pc < bytecode_len && target_pc < map_size && label_map[target_pc] >= 0) {
                    target = label_map[target_pc];
                } else {
                    target = ir_create_block(ir_out);
                    if (target_pc < bytecode_len && target_pc < map_size) {
                        label_map[target_pc] = target;
                    }
                }
                
                ir_emit_jump(ir_out, current_block, target);
                
                /* CRITICAL FIX: Same logic as OP_goto8 */
                int found_existing = 0;
                if (pc < bytecode_len && pc < map_size) {
                    for (size_t check_pc = pc; check_pc < pc + 5 && check_pc < bytecode_len && check_pc < map_size; check_pc++) {
                        if (label_map[check_pc] >= 0) {
                            current_block = label_map[check_pc];
                            found_existing = 1;
                            if (check_pc != pc) label_map[pc] = current_block;
                            break;
                        }
                    }
                }
                if (!found_existing) {
                    current_block = ir_create_block(ir_out);
                    if (pc < bytecode_len && pc < map_size) label_map[pc] = current_block;
                }
                break;
            }
            
            #ifdef SHORT_OPCODES
            case OP_if_false8: {
                int8_t offset = (int8_t)bytecode[pc++];
                /* OP_if_false8 offset is relative to the byte AFTER the opcode (pc after increment) */
                /* CORRECTION: It seems to be relative to the offset byte itself (pc - 1) */
                size_t target_pc = (pc - 1) + offset;
                
                /* STRATEGY CHANGE: Don't create blocks for future targets!
                 * Just check if one exists. If it doesn't, we'll emit a jump to a label ID
                 * that will be resolved later when we actually reach that PC.
                 * This prevents blocks from being emitted in the wrong order. */
                int target = -1;
                if (target_pc < bytecode_len && target_pc < map_size && label_map[target_pc] >= 0) {
                    target = label_map[target_pc];
                } else {
                    /* Target doesn't exist yet - create a placeholder block that will be filled later */
                    target = ir_create_block(ir_out);
                    if (target_pc < bytecode_len && target_pc < map_size) {
                        label_map[target_pc] = target;
                    }
                }
                
                /* Force a new block for fallthrough to ensure correct CFG */
                int fallthrough = ir_create_block(ir_out);
                
                ir_emit_jump_if(ir_out, current_block, IR_JUMP_IF_FALSE, target);

                /* Set successors for current block (AFTER emit) */
                ir_out->blocks[current_block].successors[0] = target;      /* Jump target */
                ir_out->blocks[current_block].successors[1] = fallthrough; /* Fallthrough */
                
                /* Switch to fallthrough block */
                current_block = fallthrough;
                break;
            }
            
            case OP_if_true8: {
                int8_t offset = (int8_t)bytecode[pc++];
                /* CORRECTION: It seems to be relative to the offset byte itself (pc - 1) */
                size_t target_pc = (pc - 1) + offset;
                
                /* Get or create block for target PC */
                int target;
                if (target_pc < bytecode_len && target_pc < map_size && label_map[target_pc] >= 0) {
                    target = label_map[target_pc];
                } else {
                    target = ir_create_block(ir_out);
                    if (target_pc < bytecode_len && target_pc < map_size) {
                        label_map[target_pc] = target;
                    }
                }
                
                /* Force a new block for fallthrough */
                int fallthrough = ir_create_block(ir_out);
                
                ir_emit_jump_if(ir_out, current_block, IR_JUMP_IF_TRUE, target);

                /* Set successors (AFTER emit) */
                ir_out->blocks[current_block].successors[0] = target;
                ir_out->blocks[current_block].successors[1] = fallthrough;
                
                /* Switch to fallthrough block */
                current_block = fallthrough;
                break;
            }
            
            case OP_goto8: {
                int8_t offset = (int8_t)bytecode[pc++];
                /* Relative to the offset byte's OWN position (pc - 1), not the
                 * byte after it - matches the interpreter (quickjs.c CASE(OP_goto8):
                 * pc[0] is read without pre-advancing past it) and mirrors the
                 * identical fix already applied to if_false8/if_true8 below.
                 * The old `pc + offset` here landed 1 byte past the true target -
                 * e.g. for a ternary whose "then" arm jumps over the "else" arm's
                 * tail, that off-by-one skipped past the shared put_loc storing
                 * the ternary's result, leaving it stranded on the operand stack
                 * for that arm instead of merged into the local both arms use. */
                size_t target_pc = (pc - 1) + offset;
                
                /* Find the start of the opcode that contains target_pc */
                size_t opcode_start = target_pc;
                if (target_pc < bytecode_len && target_pc > 0) {
                    /* Search backwards to find the opcode that contains target_pc.
                     * Iterate with a +1 bias so reaching 0 doesn't underflow (infinite loop). */
                    size_t search_limit = (target_pc > 10) ? (target_pc - 10) : 0;
                    /* +1 bias: check_pc-- makes the first body iteration use
                     * check_pc==target_pc itself (any instruction trivially
                     * "contains" its own start byte, size>=1) before searching
                     * backwards. Without this, the loop's first candidate was
                     * target_pc-1, so a target_pc that IS already a valid,
                     * aligned opcode start (the common case) was never even
                     * considered - the search would walk into a PRECEDING
                     * instruction's operand bytes instead, misinterpret that
                     * garbage byte as an opcode, and could land on a
                     * completely wrong block if that fake "opcode" happened to
                     * report a size reaching past target_pc. This produced a
                     * real infinite loop: a loop's back-edge jump landing on
                     * the function's entry block instead of the loop-condition
                     * block re-initializes every local (including the loop
                     * counter) every "iteration", so the exit condition never
                     * becomes true. */
                    for (size_t check_pc = target_pc + 1; check_pc-- > search_limit; ) {
                        uint8_t check_op = bytecode[check_pc];
                        int check_size = get_opcode_info(check_op)->size;
                        if (check_pc + check_size > target_pc) {
                            /* This opcode contains target_pc */
                            opcode_start = check_pc;
                            break;
                        }
                    }
                }
                
                /* Get the block for the opcode start (should be mapped from first pass) */
                int target = -1;
                if (opcode_start < bytecode_len && opcode_start < map_size) {
                    if (label_map[opcode_start] >= 0) {
                        target = label_map[opcode_start];
                    }
                }
                
                if (target < 0) {
                    /* No block found, create new one */
                    target = ir_create_block(ir_out);
                    if (opcode_start < bytecode_len && opcode_start < map_size) {
                        label_map[opcode_start] = target;
                    }
                }
                ir_emit_jump(ir_out, current_block, target);
                
                /* CRITICAL FIX: After goto, check if there's ALREADY a label for the next PC
                 * or any nearby PC (within next 5 bytes). This prevents creating orphan blocks
                 * when an if_false has already created a label for a future PC.
                 * Example: if_false creates block for PC=54, then goto at PC=51 should use
                 * that same block for PC=53 instead of creating a new one. */
                int found_existing = 0;
                if (pc < bytecode_len && pc < map_size) {
                    /* Check current PC and next few PCs for existing labels */
                    for (size_t check_pc = pc; check_pc < pc + 5 && check_pc < bytecode_len && check_pc < map_size; check_pc++) {
                        if (label_map[check_pc] >= 0) {
                            current_block = label_map[check_pc];
                            found_existing = 1;
                            /* Also map current PC to this block */
                            if (check_pc != pc) {
                                label_map[pc] = current_block;
                            }
                            break;
                        }
                    }
                }
                
                if (!found_existing) {
                    current_block = ir_create_block(ir_out);
                    if (pc < bytecode_len && pc < map_size) {
                        label_map[pc] = current_block;
                    }
                }
                break;
            }
            
            case OP_goto16: {
                int16_t offset = (int16_t)(bytecode[pc] | (bytecode[pc+1] << 8));
                pc += 2;
                /* Relative to the offset bytes' OWN start (pc - 2), same fix as
                 * OP_goto8 above - see that comment for why. */
                size_t target_pc = (pc - 2) + offset;
                
                /* Get or create block for target PC */
                int target;
                if (target_pc < bytecode_len && target_pc < map_size && label_map[target_pc] >= 0) {
                    target = label_map[target_pc];
                } else {
                    target = ir_create_block(ir_out);
                    if (target_pc < bytecode_len && target_pc < map_size) {
                        label_map[target_pc] = target;
                    }
                }
                
                ir_emit_jump(ir_out, current_block, target);
                
                /* CRITICAL FIX: Same logic as OP_goto8 */
                int found_existing = 0;
                if (pc < bytecode_len && pc < map_size) {
                    for (size_t check_pc = pc; check_pc < pc + 5 && check_pc < bytecode_len && check_pc < map_size; check_pc++) {
                        if (label_map[check_pc] >= 0) {
                            current_block = label_map[check_pc];
                            found_existing = 1;
                            if (check_pc != pc) label_map[pc] = current_block;
                            break;
                        }
                    }
                }
                if (!found_existing) {
                    current_block = ir_create_block(ir_out);
                    if (pc < bytecode_len && pc < map_size) label_map[pc] = current_block;
                }
                break;
            }
            
            /* get_length - used for array.length */
            case OP_get_length: {
                IRInstr instr = { .op = IR_ARRAY_LENGTH, .type = NATIVE_TYPE_INT32 };
                ir_emit(ir_out, current_block, &instr);
                break;
            }
            #endif
            
            /* Return */
            case OP_return: {
                ir_emit_return(ir_out, current_block, NATIVE_TYPE_INT32);
                current_block = ir_create_block(ir_out);
                break;
            }
            
            case OP_return_undef: {
                ir_emit_return(ir_out, current_block, NATIVE_TYPE_VOID);
                current_block = ir_create_block(ir_out);
                break;
            }
            
            /* Array access */
            case OP_get_array_el:
            case OP_get_array_el2: {
                if (object.type == NATIVE_TYPE_STRUCT_ARRAY && object.def) {
                    IRInstr instr = { .op = IR_ARRAY_ELEM_ADDR, .type = NATIVE_TYPE_PTR };
                    instr.operand.array_elem.elem_size = (int32_t)object.def->size;
                    ir_emit(ir_out, current_block, &instr);
                } else if (nc_is_dynamic_array(object.type)) {
                    IRInstr instr = {
                        .op = IR_ARRAY_GET,
                        .type = nc_dynamic_elem(object.type),
                        .operand.field.field_type = nc_dynamic_elem(object.type)
                    };
                    ir_emit(ir_out, current_block, &instr);
                } else {
                    if (object.type == NATIVE_TYPE_VOID) {
                        nc->has_error = true;
                        snprintf(nc->error_msg, sizeof(nc->error_msg),
                                 "Ambiguous array type at pc=%zu", current_pc);
                        return -1;
                    }
                    IRInstr instr = { .op = IR_LOAD_ARRAY, .type = NATIVE_TYPE_UNKNOWN };
                    ir_emit(ir_out, current_block, &instr);
                }
                break;
            }

            case OP_put_array_el: {
                NativeType lt = object.type;
                if (object.type == NATIVE_TYPE_STRUCT_ARRAY) {
                    /* Whole-element assignment (arr[i] = someStruct) is out of
                     * scope: it would need a memcpy-shaped op. Fail the compile
                     * instead of silently falling through to the scalar
                     * IR_STORE_ARRAY path, which would corrupt memory. */
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg),
                             "Whole-element assignment to a struct array is not "
                             "supported - assign individual fields instead "
                             "(arr[i].field = value)");
                    return -1;
                } else if (nc_is_dynamic_array(lt)) {
                    IRInstr instr = {
                        .op = IR_ARRAY_SET,
                        .type = NATIVE_TYPE_VOID,
                        .operand.field.field_type = nc_dynamic_elem(lt)
                    };
                    ir_emit(ir_out, current_block, &instr);
                } else {
                    IRInstr instr = { .op = IR_STORE_ARRAY, .type = NATIVE_TYPE_VOID };
                    ir_emit(ir_out, current_block, &instr);
                }
                break;
            }
            
            /* Type conversion */
            case OP_to_propkey2: {
                /* For native code, property keys are just integers, so this is a no-op */
                /* The value stays on stack unchanged */
                break;
            }
            
            case OP_to_object: {
                /* Convert to object - not supported for native code, push placeholder */
                ir_emit_const_i32(ir_out, current_block, 0);
                break;
            }
            
            case OP_to_propkey: {
                /* Convert to property key - same as to_propkey2 for native code */
                break;
            }
            
            case OP_typeof: {
                /* typeof operator - not supported for native code, push placeholder */
                IRInstr drop = { .op = IR_DROP, .type = NATIVE_TYPE_VOID };
                ir_emit(ir_out, current_block, &drop);
                ir_emit_const_i32(ir_out, current_block, 0);
                break;
            }
            
            case OP_delete: {
                /* delete operator - not supported for native code, push true */
                IRInstr drop = { .op = IR_DROP, .type = NATIVE_TYPE_VOID };
                ir_emit(ir_out, current_block, &drop);
                ir_emit(ir_out, current_block, &drop);
                ir_emit_const_i32(ir_out, current_block, 1);
                break;
            }
            
            case OP_delete_var: {
                /* delete variable - not supported for native code, push true */
                IRInstr drop = { .op = IR_DROP, .type = NATIVE_TYPE_VOID };
                ir_emit(ir_out, current_block, &drop);
                ir_emit_const_i32(ir_out, current_block, 1);
                break;
            }
            
            /* NOP and similar no-effect opcodes */
            case OP_nop:
                break;
            
            /* Temporary opcodes (OP_TEMP_START to OP_TEMP_END) overlap with short opcodes.
             * They should never appear in final bytecode as they are removed during compilation.
             * If we encounter them, handle in default case below. */
            
            /* Push constant from constant pool */
            case OP_push_const: {
                uint32_t idx = (uint32_t)(bytecode[pc] | (bytecode[pc+1] << 8) |
                                          (bytecode[pc+2] << 16) | (bytecode[pc+3] << 24));
                pc += 4;
                nc_emit_cpool_const(nc, ir_out, current_block, (int)idx);
                break;
            }

            /* Push atom string value (string literals) */
            case OP_push_atom_value: {
                uint32_t atom = (uint32_t)(bytecode[pc] | (bytecode[pc+1] << 8) |
                                           (bytecode[pc+2] << 16) | (bytecode[pc+3] << 24));
                pc += 4;
                bool emitted = false;
                if (nc->js_ctx) {
                    const char *s = JS_AtomToCString(nc->js_ctx, (JSAtom)atom);
                    if (s) {
                        int lit = nc_add_string_literal(nc, s);
                        JS_FreeCString(nc->js_ctx, s);
                        if (lit >= 0) {
                            nc_emit_string_new(nc, ir_out, current_block, lit);
                            emitted = true;
                        }
                    }
                }
                if (!emitted)
                    ir_emit_const_i32(ir_out, current_block, 0);
                break;
            }
            
            /* Special object - used at function start for arguments object */
            case OP_special_object: {
                pc += 1;  /* 1-byte argument type */
                /* Push undefined/null as placeholder */
                ir_emit_const_i32(ir_out, current_block, 0);
                break;
            }
            
            /* Null/undefined - common values */
            case OP_null:
                ir_emit_const_i32(ir_out, current_block, 0);
                break;
                
            /* Push this - for methods, but we don't support 'this' */
            case OP_push_this:
                ir_emit_const_i32(ir_out, current_block, 0);
                break;
                
            /* Object literal - not supported, push placeholder */
            case OP_object:
                ir_emit_const_i32(ir_out, current_block, 0);
                break;
            
            /* Closure - resolve compiled native functions from constant pool */
            case OP_fclosure: {
                uint32_t idx = (uint32_t)(bytecode[pc] | (bytecode[pc+1] << 8) |
                                          (bytecode[pc+2] << 16) | (bytecode[pc+3] << 24));
                pc += 4;
                void *code_ptr = nc_resolve_cpool_func_ptr(nc, (int)idx);
                if (code_ptr)
                    nc_emit_func_ptr_const(nc, ir_out, current_block, code_ptr);
                else
                    ir_emit_const_i32(ir_out, current_block, 0);
                break;
            }

            /* Closure variable read: push the captured value. Used for nested
             * native calls where the callee is captured from an outer scope.
             * Operand is a 2-byte var_ref index. */
            case OP_get_var_ref:
            case OP_get_var_ref_check: {
                uint16_t vidx = (uint16_t)(bytecode[pc] | (bytecode[pc+1] << 8));
                pc += 2;
                void *code_ptr = nc_resolve_closure_func_ptr(nc, (int)vidx);
                if (code_ptr) {
                    nc_emit_func_ptr_const(nc, ir_out, current_block, code_ptr);
                } else {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg),
                             "Closure variable %d is not a compiled native function "
                             "(only nested Native.compile() calls are supported)", vidx);
                    return -1;
                }
                break;
            }
            
            /* OP_get_field - property access: obj -> value
             * OP_get_field2 - for method calls like Math.sqrt(): obj -> obj, value
             * Format: 1 byte opcode + 4 bytes atom */
            case OP_get_field: {
                const uint8_t *atom_bytes = bytecode + pc;
                pc += 4;
                const NativeStructField *field = nc_object_field(nc->js_ctx, object, atom_bytes);
                if (field) {
                    NativeType type = field->type;
                    IROp field_op = IR_LOAD_FIELD_DYN;
                    if (field->flags & FIELD_FLAG_ARRAY) {
                        field_op = IR_LOAD_FIELD_ADDR;
                        type = field->type == NATIVE_TYPE_FLOAT32 ? NATIVE_TYPE_FLOAT32_ARRAY :
                            field->type == NATIVE_TYPE_UINT32 ? NATIVE_TYPE_UINT32_ARRAY : NATIVE_TYPE_INT32_ARRAY;
                    }
                    IRInstr instr = {
                        .op = field_op, .type = type,
                        .operand.field = { .offset = field->offset, .local_idx = -1,
                                           .field_type = field->type }
                    };
                    ir_emit(ir_out, current_block, &instr);
                } else {
                    JSAtom atom = (JSAtom)((uint32_t)atom_bytes[0] | ((uint32_t)atom_bytes[1] << 8) |
                        ((uint32_t)atom_bytes[2] << 16) | ((uint32_t)atom_bytes[3] << 24));
                    const char *name = JS_AtomToCString(nc->js_ctx, atom);
                    bool length = name && !strcmp(name, "length");
                    JS_FreeCString(nc->js_ctx, name);
                    if (length && (NATIVE_TYPE_IS_ARRAY(object.type) || nc_is_dynamic_array(object.type) ||
                                   object.type == NATIVE_TYPE_STRING || object.type == NATIVE_TYPE_STRING_VIEW)) {
                        IRInstr instr = { .op = NATIVE_TYPE_IS_STRING(object.type) ? IR_STRING_LENGTH : IR_ARRAY_LENGTH,
                                          .type = NATIVE_TYPE_INT32 };
                        ir_emit(ir_out, current_block, &instr);
                    } else {
                        nc->has_error = true;
                        snprintf(nc->error_msg, sizeof(nc->error_msg),
                                 "Unknown or ambiguous native field receiver at pc=%zu", current_pc);
                        return -1;
                    }
                }
                break;
            }

            case OP_get_field2: {
                /* Read atom (4 bytes, little-endian) */
                uint32_t atom = bytecode[pc] | (bytecode[pc+1] << 8) | 
                                (bytecode[pc+2] << 16) | (bytecode[pc+3] << 24);
                pc += 4;
                
                /* Check if this is an intrinsic or registered C function */
                nc->pending_intrinsic_op = IR_NOP;
                nc->pending_intrinsic_entry = NULL;
                nc->pending_intrinsic_elem_type = NATIVE_TYPE_UNKNOWN;
                nc->pending_intrinsic_is_method = false;

                /* Method calls on a dynamically-indexed struct-array element
                 * (arr[i].method()) are out of scope for this pass - pop
                 * rather than leave it pushed, so a later, unrelated
                 * OP_get_field/OP_put_field doesn't consume a stale entry. */

                if (nc->js_ctx) {
                    const char *name = JS_AtomToCString(nc->js_ctx, (JSAtom)atom);
                    if (name) {
                        /* Check for native FPU intrinsics (Math.xxx) */
                        if (strcmp(name, "sqrt") == 0) {
                            nc->pending_intrinsic_op = IR_SQRT_F32;
                        } else if (strcmp(name, "abs") == 0) {
                            nc->pending_intrinsic_op = IR_ABS_F32;
                        } else if (strcmp(name, "min") == 0) {
                            nc->pending_intrinsic_op = IR_MIN_F32;
                        } else if (strcmp(name, "max") == 0) {
                            nc->pending_intrinsic_op = IR_MAX_F32;
                        } else if (strcmp(name, "clamp") == 0) {
                            nc->pending_intrinsic_op = IR_CLAMP_F32;
                        } else if (strcmp(name, "sign") == 0) {
                            nc->pending_intrinsic_op = IR_SIGN_F32;
                        } else if (strcmp(name, "fround") == 0) {
                            nc->pending_intrinsic_op = IR_FROUND_F32;
                        } else if (strcmp(name, "imul") == 0) {
                            nc->pending_intrinsic_op = IR_IMUL_I32;
                        } else if (strcmp(name, "fma") == 0) {
                            nc->pending_intrinsic_op = IR_FMA_F32;
                        } else if (strcmp(name, "lerp") == 0) {
                            nc->pending_intrinsic_op = IR_LERP_F32;
                        } else if (strcmp(name, "saturate") == 0) {
                            nc->pending_intrinsic_op = IR_SATURATE_F32;
                        } else if (strcmp(name, "step") == 0) {
                            nc->pending_intrinsic_op = IR_STEP_F32;
                        } else if (strcmp(name, "smoothstep") == 0) {
                            nc->pending_intrinsic_op = IR_SMOOTHSTEP_F32;
                        } else if (strcmp(name, "inversesqrt") == 0 || strcmp(name, "rsqrt") == 0) {
                            nc->pending_intrinsic_op = IR_RSQRT_F32;
                        } else if (!nc_match_array_string_method(nc, ir_out, name)) {
                            /* Look up in native function registry */
                            char full_name[64];
                            snprintf(full_name, sizeof(full_name), "Math.%s", name);
                            NativeFuncEntry *entry = native_lookup_function(nc, full_name);
                            if (entry) {
                                nc->pending_intrinsic_op = IR_CALL_C_FUNC;
                                nc->pending_intrinsic_entry = entry;
                            }
                        } else {
                            /* arr.push()/str.slice()-style instance method: the
                             * receiver just loaded (a real local, not a global
                             * placeholder) is the intrinsic's first operand and
                             * must stay on the eval stack - see the DROP below. */
                            nc->pending_intrinsic_is_method = true;
                        }
                        JS_FreeCString(nc->js_ctx, name);
                    }
                }
                
                /* Don't emit any IR for intrinsics - the entry is consumed by
                 * the matching call_method/tail_call_method below. Push rather
                 * than leave it in the scalar fields, so an intrinsic nested in
                 * another one's arguments (Math.atan2(x, Math.sqrt(y))) doesn't
                 * clobber the outer one - see pending_intrinsic_stack. */
                if (nc->pending_intrinsic_op != IR_NOP) {
                    /* Drop the global-object placeholder that OP_get_var
                     * pushed for `Math`. QuickJS's call_method pops the object
                     * and the function along with the arguments, but we resolve
                     * the callee at compile time and emit no IR for it - so
                     * without this the placeholder was simply left behind.
                     *
                     * A single leftover sat harmlessly below the arguments, but
                     * a nested call (Math.atan2(a, Math.sqrt(b))) left the
                     * inner one BETWEEN the outer call's arguments: atan2 read
                     * the placeholder 0 as its first argument and returned 0.
                     * With Math.imul the leftover became a second return value
                     * that overwrote the real one in $v0.
                     *
                     * [reg] arr.push()/str.slice()-style instance methods hit
                     * this same "pending_intrinsic_op != IR_NOP" branch, but
                     * their receiver isn't a placeholder - it's a real local
                     * (arr) that IR_ARRAY_PUSH etc. still need to pop as their
                     * first operand. Dropping it here desynced the eval-stack
                     * accounting from the actual live values: the next
                     * IR_LOAD_LOCAL reused the register the dropped receiver
                     * was still sitting in (silently clobbering it), and the
                     * intrinsic's second POP_REG() then underflowed the stack
                     * index, picking up an unrelated, uninitialized register
                     * as the "array" pointer - an Address Error on real
                     * hardware the moment that garbage pointer got deref'd. */
                    if (!nc->pending_intrinsic_is_method) {
                        IRInstr drop = { .op = IR_DROP, .type = NATIVE_TYPE_VOID };
                        ir_emit(ir_out, current_block, &drop);
                    }

                    if (nc->pending_intrinsic_depth >= NC_MAX_PENDING_INTRINSICS) {
                        nc->has_error = true;
                        snprintf(nc->error_msg, sizeof(nc->error_msg),
                                 "Too many nested intrinsic calls (max %d)",
                                 NC_MAX_PENDING_INTRINSICS);
                        return -1;
                    }
                    int d = nc->pending_intrinsic_depth++;
                    nc->pending_intrinsic_stack[d].op = nc->pending_intrinsic_op;
                    nc->pending_intrinsic_stack[d].entry = nc->pending_intrinsic_entry;
                    nc->pending_intrinsic_stack[d].elem_type = nc->pending_intrinsic_elem_type;
                }
                break;
            }
            
            case OP_put_field: {
                const NativeStructField *field = nc_object_field(nc->js_ctx, object, bytecode + pc);
                pc += 4;
                if (!field || (field->flags & FIELD_FLAG_ARRAY)) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg),
                             "Unknown or ambiguous native field store at pc=%zu", current_pc);
                    return -1;
                }
                /* Consume the actual LHS pointer, which remains below the RHS
                 * on the operand stack even if the RHS calls native/C code. */
                IRInstr instr = {
                    .op = IR_STORE_FIELD_DYN, .type = NATIVE_TYPE_VOID,
                    .operand.field = { .offset = field->offset, .local_idx = -1,
                                       .field_type = field->type }
                };
                ir_emit(ir_out, current_block, &instr);
                break;
            }

            /* OP_call_method - call a method with nargs
             * Format: 1 byte opcode + 2 bytes nargs */
            case OP_call_method: {
                uint16_t nargs = bytecode[pc] | (bytecode[pc+1] << 8);
                pc += 2;
                
                bool emitted_intrinsic = false;
                
                /* Check if we have a pending intrinsic from OP_get_field2 */
                if (nc->pending_intrinsic_depth > 0 && nargs >= 0 && nargs <= 8) {
                    int d = --nc->pending_intrinsic_depth;
                    IROp intrinsic_op = nc->pending_intrinsic_stack[d].op;
                    NativeFuncEntry *entry = nc->pending_intrinsic_stack[d].entry;
                    nc->pending_intrinsic_elem_type = nc->pending_intrinsic_stack[d].elem_type;
                    
                    /* Native FPU intrinsics (sqrt, abs, sign, fround) - only for single-arg */
                    if ((intrinsic_op == IR_SQRT_F32 || intrinsic_op == IR_ABS_F32 || intrinsic_op == IR_SIGN_F32 || intrinsic_op == IR_FROUND_F32) && nargs == 1) {
                        IRInstr instr = { .op = intrinsic_op, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Variadic min/max - emit N-1 chained binary operations */
                    else if ((intrinsic_op == IR_MIN_F32 || intrinsic_op == IR_MAX_F32) && nargs >= 2) {
                        /* Generate N-1 chained min/max operations:
                         * min(a,b,c,d) -> min(min(min(a,b),c),d) */
                        for (int i = 0; i < nargs - 1; i++) {
                            IRInstr instr = { .op = intrinsic_op, .type = NATIVE_TYPE_FLOAT32 };
                            ir_emit(ir_out, current_block, &instr);
                        }
                        emitted_intrinsic = true;
                    }
                    /* imul - requires exactly 2 args */
                    else if (intrinsic_op == IR_IMUL_I32 && nargs == 2) {
                        IRInstr instr = { .op = IR_IMUL_I32, .type = NATIVE_TYPE_INT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Clamp - requires exactly 3 args: value, min, max */
                    else if (intrinsic_op == IR_CLAMP_F32 && nargs == 3) {
                        IRInstr instr = { .op = IR_CLAMP_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* FMA - requires exactly 3 args: a, b, c */
                    else if (intrinsic_op == IR_FMA_F32 && nargs == 3) {
                        IRInstr instr = { .op = IR_FMA_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Lerp - requires exactly 3 args: a, b, t */
                    else if (intrinsic_op == IR_LERP_F32 && nargs == 3) {
                        IRInstr instr = { .op = IR_LERP_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Saturate - requires exactly 1 arg */
                    else if (intrinsic_op == IR_SATURATE_F32 && nargs == 1) {
                        IRInstr instr = { .op = IR_SATURATE_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Step - requires exactly 2 args: edge, x */
                    else if (intrinsic_op == IR_STEP_F32 && nargs == 2) {
                        IRInstr instr = { .op = IR_STEP_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Smoothstep - requires exactly 3 args: e0, e1, x */
                    else if (intrinsic_op == IR_SMOOTHSTEP_F32 && nargs == 3) {
                        IRInstr instr = { .op = IR_SMOOTHSTEP_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Rsqrt - requires exactly 1 arg */
                    else if (intrinsic_op == IR_RSQRT_F32 && nargs == 1) {
                        IRInstr instr = { .op = IR_RSQRT_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Generic C function call - supports 1-8 args */
                    else if (intrinsic_op == IR_CALL_C_FUNC && entry && entry->func_ptr && entry->sig.arg_count == nargs) {
                        IRInstr instr = { .op = IR_CALL_C_FUNC, .type = entry->sig.return_type };
                        instr.operand.call.func_ptr = entry->func_ptr;
                        instr.operand.call.arg_count = nargs;
                        instr.operand.call.ret_type = entry->sig.return_type;
                        /* Copy argument types from signature */
                        for (int i = 0; i < nargs && i < 8; i++) {
                            instr.operand.call.arg_types[i] = entry->sig.arg_types[i];
                        }
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    else if (nc_emit_array_string_intrinsic(nc, ir_out, current_block,
                                                            intrinsic_op, nargs)) {
                        emitted_intrinsic = true;
                    }
                    
                    /* Clear pending intrinsic */
                    nc->pending_intrinsic_op = IR_NOP;
                    nc->pending_intrinsic_entry = NULL;
                    nc->pending_intrinsic_elem_type = NATIVE_TYPE_UNKNOWN;
                }
                
                if (!emitted_intrinsic) {
                    /* No intrinsic matched. Emitting a generic indirect IR_CALL
                     * here would be a silent miscompile: an indirect call pops a
                     * callee pointer off the eval stack, but a method call never
                     * pushes one (Math.xxx resolves at compile time, and nothing
                     * else is supported), so it would jump to whatever value
                     * happened to be on top. Fail the compile instead. */
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg),
                             "Unsupported method call with %d argument(s) at pc=%zu "
                             "(only Math.* intrinsics and registered C functions "
                             "are callable as methods)", nargs, current_pc);
                    return -1;
                }
                break;
            }
            
            /* OP_get_var - load a global variable like "Math"
             * Format: 1 byte opcode + 4 bytes atom */
            case OP_get_var: {
                /* Skip the atom - just push a placeholder for the global object */
                pc += 4;
                ir_emit_const_i32(ir_out, current_block, 0);  /* Placeholder for global object */
                break;
            }
            
            /* OP_tail_call_method - tail call version of call_method
             * Format: 1 byte opcode + 2 bytes nargs
             * Same intrinsic detection logic as call_method */
            case OP_tail_call_method: {
                uint16_t nargs = bytecode[pc] | (bytecode[pc+1] << 8);
                pc += 2;
                
                bool emitted_intrinsic = false;
                
                /* Check if we have a pending intrinsic from OP_get_field2 */
                if (nc->pending_intrinsic_depth > 0 && nargs >= 0 && nargs <= 8) {
                    int d = --nc->pending_intrinsic_depth;
                    IROp intrinsic_op = nc->pending_intrinsic_stack[d].op;
                    NativeFuncEntry *entry = nc->pending_intrinsic_stack[d].entry;
                    nc->pending_intrinsic_elem_type = nc->pending_intrinsic_stack[d].elem_type;
                    
                    /* Native FPU intrinsics (sqrt, abs, sign, fround) - only for single-arg */
                    if ((intrinsic_op == IR_SQRT_F32 || intrinsic_op == IR_ABS_F32 || intrinsic_op == IR_SIGN_F32 || intrinsic_op == IR_FROUND_F32) && nargs == 1) {
                        IRInstr instr = { .op = intrinsic_op, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Variadic min/max - emit N-1 chained binary operations */
                    else if ((intrinsic_op == IR_MIN_F32 || intrinsic_op == IR_MAX_F32) && nargs >= 2) {
                        /* Generate N-1 chained min/max operations:
                         * min(a,b,c,d) -> min(min(min(a,b),c),d) */
                        for (int i = 0; i < nargs - 1; i++) {
                            IRInstr instr = { .op = intrinsic_op, .type = NATIVE_TYPE_FLOAT32 };
                            ir_emit(ir_out, current_block, &instr);
                        }
                        emitted_intrinsic = true;
                    }
                    /* imul - requires exactly 2 args */
                    else if (intrinsic_op == IR_IMUL_I32 && nargs == 2) {
                        IRInstr instr = { .op = IR_IMUL_I32, .type = NATIVE_TYPE_INT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Clamp - requires exactly 3 args: value, min, max */
                    else if (intrinsic_op == IR_CLAMP_F32 && nargs == 3) {
                        IRInstr instr = { .op = IR_CLAMP_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* FMA - requires exactly 3 args: a, b, c */
                    else if (intrinsic_op == IR_FMA_F32 && nargs == 3) {
                        IRInstr instr = { .op = IR_FMA_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Lerp - requires exactly 3 args: a, b, t */
                    else if (intrinsic_op == IR_LERP_F32 && nargs == 3) {
                        IRInstr instr = { .op = IR_LERP_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Saturate - requires exactly 1 arg */
                    else if (intrinsic_op == IR_SATURATE_F32 && nargs == 1) {
                        IRInstr instr = { .op = IR_SATURATE_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Step - requires exactly 2 args: edge, x */
                    else if (intrinsic_op == IR_STEP_F32 && nargs == 2) {
                        IRInstr instr = { .op = IR_STEP_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Smoothstep - requires exactly 3 args: e0, e1, x */
                    else if (intrinsic_op == IR_SMOOTHSTEP_F32 && nargs == 3) {
                        IRInstr instr = { .op = IR_SMOOTHSTEP_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Rsqrt - requires exactly 1 arg */
                    else if (intrinsic_op == IR_RSQRT_F32 && nargs == 1) {
                        IRInstr instr = { .op = IR_RSQRT_F32, .type = NATIVE_TYPE_FLOAT32 };
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    /* Generic C function call - supports 1-8 args */
                    else if (intrinsic_op == IR_CALL_C_FUNC && entry && entry->func_ptr && entry->sig.arg_count == nargs) {
                        IRInstr instr = { .op = IR_CALL_C_FUNC, .type = entry->sig.return_type };
                        instr.operand.call.func_ptr = entry->func_ptr;
                        instr.operand.call.arg_count = nargs;
                        instr.operand.call.ret_type = entry->sig.return_type;
                        /* Copy argument types from signature */
                        for (int i = 0; i < nargs && i < 8; i++) {
                            instr.operand.call.arg_types[i] = entry->sig.arg_types[i];
                        }
                        ir_emit(ir_out, current_block, &instr);
                        emitted_intrinsic = true;
                    }
                    else if (nc_emit_array_string_intrinsic(nc, ir_out, current_block,
                                                            intrinsic_op, nargs)) {
                        emitted_intrinsic = true;
                    }
                    
                    /* Clear pending intrinsic */
                    nc->pending_intrinsic_op = IR_NOP;
                    nc->pending_intrinsic_entry = NULL;
                    nc->pending_intrinsic_elem_type = NATIVE_TYPE_UNKNOWN;
                }
                
                if (emitted_intrinsic) {
                    /* tail_call_method ends the function — return the intrinsic result */
                    ir_emit_return(ir_out, current_block, ir_out->sig.return_type);
                } else {
                    /* Same reasoning as the OP_call_method fallback above: an
                     * indirect tail call would jump to a callee pointer that was
                     * never pushed. This is exactly how Math.atan2(a, Math.sqrt(b))
                     * used to end up doing JR on a float's bit pattern. */
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg),
                             "Unsupported method call with %d argument(s) at pc=%zu "
                             "(only Math.* intrinsics and registered C functions "
                             "are callable as methods)", nargs, current_pc);
                    return -1;
                }
                break;
            }
            
            /* Rest parameter - skip */
            case OP_rest: {
                pc += 2;  /* 2-byte argument */
                ir_emit_const_i32(ir_out, current_block, 0);
                break;
            }
            
            /* Call opcodes - emit IR_CALL for native function calls */
            case OP_call: {
                uint16_t nargs = bytecode[pc] | (bytecode[pc+1] << 8);
                pc += 2;
                /* Emit IR_CALL - assumes function pointer and args are on stack */
                IRInstr call = {
                    .op = IR_CALL,
                    .type = NATIVE_TYPE_INT32  /* Default return type */
                };
                call.operand.call.arg_count = nargs;
                call.operand.call.func_ptr = NULL;  /* Indirect call */
                nc_apply_pending_call_sig(nc, &call);
                ir_emit(ir_out, current_block, &call);
                break;
            }

            #ifdef SHORT_OPCODES
            /* Short call opcodes - emit IR_CALL with 0-3 args */
            case OP_call0:
            case OP_call1:
            case OP_call2:
            case OP_call3: {
                int nargs = op - OP_call0;
                IRInstr call = {
                    .op = IR_CALL,
                    .type = NATIVE_TYPE_INT32
                };
                call.operand.call.arg_count = nargs;
                call.operand.call.func_ptr = NULL;
                nc_apply_pending_call_sig(nc, &call);
                ir_emit(ir_out, current_block, &call);
                break;
            }

            /* Tail call - same as call but uses IR_TAIL_CALL for TCO */
            case OP_tail_call: {
                uint16_t nargs = bytecode[pc] | (bytecode[pc+1] << 8);
                pc += 2;
                IRInstr tail_call = {
                    .op = IR_TAIL_CALL,
                    .type = NATIVE_TYPE_INT32
                };
                tail_call.operand.call.arg_count = nargs;
                tail_call.operand.call.func_ptr = NULL;
                nc_apply_pending_call_sig(nc, &tail_call);
                ir_emit(ir_out, current_block, &tail_call);
                break;
            }

            /* push_const8 - 1-byte constant pool index */
            case OP_push_const8: {
                uint8_t idx = bytecode[pc++];
                nc_emit_cpool_const(nc, ir_out, current_block, idx);
                break;
            }
            
            /* fclosure8 - 1-byte closure index */
            case OP_fclosure8: {
                uint8_t idx = bytecode[pc++];
                void *code_ptr = nc_resolve_cpool_func_ptr(nc, idx);
                if (code_ptr)
                    nc_emit_func_ptr_const(nc, ir_out, current_block, code_ptr);
                else
                    ir_emit_const_i32(ir_out, current_block, 0);
                break;
            }
            
            /* push_empty_string - create empty NativeString constant */
            case OP_push_empty_string: {
                int lit = nc_add_string_literal(nc, "");
                if (lit >= 0)
                    nc_emit_string_new(nc, ir_out, current_block, lit);
                else
                    ir_emit_const_i32(ir_out, current_block, 0);
                break;
            }
                
            /* is_undefined, is_null - type checks */
            case OP_is_undefined:
            case OP_is_null:
                /* Pop value, push boolean result (0 for now) */
                ir_emit_const_i32(ir_out, current_block, 0);
                break;
            #endif
            
            default: {
                /* Try to handle as SHORT opcode dynamically */
                #ifdef SHORT_OPCODES
                /* Check if it's a SHORT opcode for get_arg/get_loc */
                if (op >= OP_get_arg0 && op <= OP_get_arg3) {
                    int idx = op - OP_get_arg0;
                    ir_emit_load_local(ir_out, current_block, idx, ir_out->local_types[idx]);
                    if (idx >= ir_out->local_count) ir_out->local_count = idx + 1;
                    break;
                }
                if (op >= OP_get_loc0 && op <= OP_get_loc3) {
                    int idx = op - OP_get_loc0;
                    ir_emit_load_local(ir_out, current_block, idx, ir_out->local_types[idx]);
                    if (idx >= ir_out->local_count) ir_out->local_count = idx + 1;
                    break;
                }
                if (op >= OP_get_var_ref0 && op <= OP_get_var_ref3) {
                    int vidx = op - OP_get_var_ref0;
                    void *code_ptr = nc_resolve_closure_func_ptr(nc, vidx);
                    if (code_ptr) {
                        nc_emit_func_ptr_const(nc, ir_out, current_block, code_ptr);
                        break;
                    }
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg),
                             "Closure variable %d is not a compiled native function "
                             "(only nested Native.compile() calls are supported)", vidx);
                    return -1;
                }
                if (op >= OP_put_arg0 && op <= OP_put_arg3) {
                    int idx = op - OP_put_arg0;
                    ir_emit_store_local(ir_out, current_block, idx, NATIVE_TYPE_UNKNOWN);
                    if (idx >= ir_out->local_count) ir_out->local_count = idx + 1;
                    break;
                }
                if (op >= OP_put_loc0 && op <= OP_put_loc3) {
                    int idx = op - OP_put_loc0;
                    ir_emit_store_local(ir_out, current_block, idx, NATIVE_TYPE_UNKNOWN);
                    if (idx >= ir_out->local_count) ir_out->local_count = idx + 1;
                    break;
                }
                
                #endif
                
                /* Check if it's a temporary opcode (should not appear in final bytecode) */
                if (op >= OP_TEMP_START && op < OP_TEMP_END) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg), 
                             "Temporary opcode 0x%02x [%s] at pc=%zu (should not appear in final bytecode)", 
                             op, op_to_cstr(op), pc - 1);
                    return -1;
                }
                
                /* Unsupported opcode - fail compilation */
                nc->has_error = true;
                snprintf(nc->error_msg, sizeof(nc->error_msg),
                         "Unsupported bytecode opcode 0x%02x [%s] at pc=%zu",
                         op, op_to_cstr(op), pc - 1);
                free(jump_targets);
                free(label_map);
                return -1;
            }
        }
    }
    
    /* No auto-return needed:
     * - Normal functions have explicit IR_RETURN in bytecode
     * - Intrinsic-only functions leave result directly in $f0 (floats) or $v0 (ints)
     * Adding auto-return here breaks intrinsics by POPing garbage from empty stack */
    
    /* FIX: Reorder blocks by execution flow to ensure correct assembly generation.
     * This fixes the issue where blocks created early (like exit blocks from if_false)
     * are emitted before the loop body, causing incorrect control flow. */
    reorder_blocks_by_execution(ir_out);
    
    /* FIX: Free allocated arrays now that we're done translating */
    free(jump_targets);
    free(label_map);
    
    return 0;
}

int bytecode_to_ir(NativeCompiler *nc, const uint8_t *bytecode,
                   size_t bytecode_len, IRFunction *ir_out) {
    if (!nc || !bytecode || !bytecode_len || !ir_out) return -1;
    NCObjectFlow objects = {0};
    int status = nc_analyze_objects(nc, bytecode, bytecode_len, &ir_out->sig, &objects);
    if (status == 0)
        status = bytecode_to_ir_impl(nc, bytecode, bytecode_len, ir_out, &objects);
    free(objects.values);
    free(objects.offsets);
    return status;
}

/* ============================================
 * IR to Native Code Generation
 * ============================================ */

/*
 * Detect if function contains loops (backward jumps)
 */
static bool detect_loops(const IRFunction *ir) {
    for (int b = 0; b < ir->block_count; b++) {
        IRBasicBlock *block = &ir->blocks[b];
        
        for (int i = 0; i < block->instr_count; i++) {
            IRInstr *instr = &block->instrs[i];
            
            /* Check for backward jumps (jump to earlier block = loop) */
            if (instr->op == IR_JUMP || instr->op == IR_JUMP_IF_TRUE || instr->op == IR_JUMP_IF_FALSE) {
                int target_block = instr->operand.label_id;
                if (target_block <= b) {  /* Backward jump = loop */
                    return true;
                }
            }
        }
    }
    
    return false;
}

/*
 * Scan the IR for calls (to other native functions) that pass more than 4
 * arguments. Args 4+ of such a call are staged on the stack at [$sp + i*4]
 * right before the call (see IR_CALL codegen), so this function's own frame
 * must reserve enough space at the very bottom ($fp+0) to hold them without
 * clobbering its own locals. Returns the number of bytes to reserve (0 if
 * this function makes no such call).
 */
static bool detect_leaf_function(const IRFunction *ir);

static int compute_max_outgoing_arg_bytes(const IRFunction *ir) {
    /* O32 callees may spill their register arguments into the caller's
     * 16-byte home area, even with only one argument. Keep it separate from
     * our spilled pointers and locals for every function that emits calls. */
    int max_bytes = detect_leaf_function(ir) ? 0 : 16;
    for (int b = 0; b < ir->block_count; b++) {
        const IRBasicBlock *block = &ir->blocks[b];
        for (int i = 0; i < block->instr_count; i++) {
            const IRInstr *instr = &block->instrs[i];
            if (instr->op == IR_CALL || instr->op == IR_TAIL_CALL ||
                instr->op == IR_CALL_C_FUNC) {
                int argc = instr->operand.call.arg_count;
                if (argc > 4) {
                    int bytes = argc * 4;
                    if (bytes > max_bytes) max_bytes = bytes;
                }
            }
        }
    }
    return max_bytes;
}

/*
 * Map local variable index to register or stack slot.
 * Strategy:
 * - Arguments 0-3: use $a0-$a3
 * - Local variables (if function has loops): use $s0-$s7 for int32 types
 * - Everything else: stack
 */
static bool detect_leaf_function(const IRFunction *ir);

static void allocate_locals(NativeCompiler *nc, const IRFunction *ir) {
    /* MipsEmitter *em = &nc->emitter; - Unused */
    
    /* Initialize local_regs array to -1 (not in register) */
    for (int i = 0; i < NC_MAX_LOCALS; i++) {
        nc->local_regs[i] = -1;
        nc->local_stack_offs[i] = -1;
    }
    
    /* Use argument registers for first 4 args */
    const MipsReg arg_regs[] = { REG_A0, REG_A1, REG_A2, REG_A3 };

    /* Reserve [0, outgoing_arg_bytes) at the very bottom of the frame for
     * staging args 4+ of any >4-arg call this function makes (see IR_CALL).
     * Everything else below is allocated starting above that reservation. */
    int outgoing_arg_bytes = compute_max_outgoing_arg_bytes(ir);
    int stack_slot = outgoing_arg_bytes;
    nc->used_saved_regs = 0;  /* Reset bitmask; eval stack marks $s0-$s7 later */
    
    /* Detect loops in function */
    nc->has_loops = detect_loops(ir);

    /* Non-leaf functions call C runtime helpers / other native functions, which
     * clobber the caller-saved argument registers $a0-$a3. If an argument is used
     * after such a call, reading it back from $a0-$a3 would return garbage. Spill
     * arguments to stack slots in that case so they have a stable home. The eval
     * operand stack occupies $s0-$s7, so we can't home them there. */
    bool spill_args = !detect_leaf_function(ir);
    
    /* Process all locals declared in IR */
    for (int i = 0; i < ir->local_count; i++) {
        if (i < 4 && i < ir->sig.arg_count) {
            if (spill_args) {
                /* Spill arg to stack (prologue stores $a0-$a3 there).
                 * Arguments arrive in registers, so each occupies at least one
                 * 32-bit slot. Pointer-like types (string/array/ptr) report a
                 * size of 0 from native_type_size, so clamp to 4 bytes to avoid
                 * two args sharing the same stack offset. */
                size_t size = native_type_size(ir->local_types[i]);
                if (size < 4) size = 4;
                nc->local_regs[i] = -1;
                nc->local_stack_offs[i] = stack_slot;
                stack_slot += (size + 3) & ~3;  /* Align to 4 bytes */
            } else {
                /* Leaf function: keep args in their registers */
                nc->local_regs[i] = (int)arg_regs[i];
                nc->local_stack_offs[i] = -1;
            }
        } else if (i >= 4 && i < ir->sig.arg_count) {
            /* Stack-passed arguments (args 4+) - passed by caller above our frame.
             * Mark with sentinel -2 and calculate actual offset later in prologue
             * when frame_size is known. Offset will be: frame_size + i*4 (see
             * the fixup in emit_function_prologue for why it's i*4, not (i-4)*4). */
            nc->local_regs[i] = -1;
            nc->local_stack_offs[i] = -2;  /* Sentinel: stack-passed arg */
        } else {
            /* Allocate on stack ($s0-$s7 are reserved for the eval operand stack) */
            nc->local_regs[i] = -1;
            nc->local_stack_offs[i] = stack_slot;

            /* Determine size based on type. Pointer-like types (string/array/ptr)
             * report a size of 0 from native_type_size (see the matching clamp
             * above for argument locals) - clamp here too so a string/array
             * local doesn't share its stack slot with whatever comes next. */
            size_t size = native_type_size(ir->local_types[i]);
            if (size < 4) size = 4;
            stack_slot += (size + 3) & ~3;  /* Align to 4 bytes */
        }
    }
    
    /* CRITICAL FIX: Allocate stack space for temp variables beyond local_count
     * The IR may reference local[idx] where idx >= local_count (temp variables).
     * Ensure these have valid stack offsets too. */
    for (int i = ir->local_count; i < NC_MAX_LOCALS; i++) {
        if (nc->local_stack_offs[i] == -1 && nc->local_regs[i] == -1) {
            /* Not yet allocated - give it a stack slot */
            nc->local_regs[i] = -1;
            nc->local_stack_offs[i] = stack_slot;
            stack_slot += 4;  /* Default to 32-bit slot */
        }
    }
    
    nc->emitter.stack_offset = stack_slot;
    
    NC_LOG("allocate_locals: local_count=%d, stack_offset=%d", ir->local_count, stack_slot);
    for (int i = 0; i < ir->local_count && i < 4; i++) {
        NC_LOG("  local[%d]: reg=%d, stack_offs=%d", i, nc->local_regs[i], nc->local_stack_offs[i]);
    }
    
    /* Determine if we need the frame pointer */
    /* We need it if any locals are on the stack OR we use saved registers OR we have stack-passed args */
    nc->needs_fp = (stack_slot > 0 || nc->used_saved_regs != 0 || ir->sig.arg_count > 4);
}

/*
 * Detect if function is a leaf (doesn't call other functions).
 * Leaf functions don't need to save/restore $ra.
 */
/* True if an IR op lowers to a JALR into the C runtime (or another native
 * function). Such ops clobber the caller-saved argument registers, so a
 * function containing any of them is not a leaf. Note the inline ops
 * (IR_ARRAY_GET/SET/LENGTH, IR_STRING_LENGTH) do NOT emit a call. */
static bool ir_op_emits_call(IROp op) {
    switch (op) {
        case IR_CALL:
        case IR_CALL_C_FUNC:
        case IR_TAIL_CALL:
        /* Dynamic array ops backed by the C runtime */
        case IR_ARRAY_NEW:
        case IR_ARRAY_PUSH:
        case IR_ARRAY_POP:
        case IR_ARRAY_INSERT:
        case IR_ARRAY_REMOVE:
        case IR_ARRAY_RESIZE:
        case IR_ARRAY_RESERVE:
        case IR_ARRAY_CLEAR:
        /* String ops backed by the C runtime */
        case IR_STRING_NEW:
        case IR_STRING_CONCAT:
        case IR_STRING_SLICE:
        case IR_STRING_COMPARE:
        case IR_STRING_EQUALS:
        case IR_STRING_NE:
        case IR_STRING_FIND:
        case IR_STRING_REPLACE:
        case IR_STRING_TO_UPPER:
        case IR_STRING_TO_LOWER:
        case IR_STRING_TRIM:
            return true;
        default:
            return false;
    }
}

static bool detect_leaf_function(const IRFunction *ir) {
    for (int b = 0; b < ir->block_count; b++) {
        const IRBasicBlock *block = &ir->blocks[b];
        for (int i = 0; i < block->instr_count; i++) {
            if (ir_op_emits_call(block->instrs[i].op)) {
                return false;  /* Has a runtime call - not a leaf */
            }
        }
    }
    return true;  /* No calls - leaf function */
}

/*
 * Emit function prologue (stack frame setup).
 */
static void emit_function_prologue(NativeCompiler *nc, const NativeFuncSignature *sig) {
    MipsEmitter *em = &nc->emitter;
    
    /* Count how many $s registers we need to save */
    const MipsReg saved_regs[] = { REG_S0, REG_S1, REG_S2, REG_S3, REG_S4, REG_S5, REG_S6, REG_S7 };
    int saved_reg_count = 0;
    for (int i = 0; i < 8; i++) {
        if (nc->used_saved_regs & (1 << i)) {
            saved_reg_count++;
        }
    }
    
    /* Calculate frame size: locals + $ra + $fp + saved $s registers */
    int frame_size = nc->emitter.stack_offset + 8 + (saved_reg_count * 4);  /* +8 for $ra and $fp */
    
    /* Align stack to 16 bytes */
    frame_size = (frame_size + 15) & ~15;
    
    const MipsReg arg_regs[] = { REG_A0, REG_A1, REG_A2, REG_A3 };
    const MipsReg temp_regs[] = { REG_T0, REG_T1, REG_T2, REG_T3 };
    
    /* First, check if we need to save any arguments temporarily */
    bool need_temp_save = false;
    for (int i = 0; i < sig->arg_count && i < 4; i++) {
        if (nc->local_regs[i] != arg_regs[i] && nc->local_regs[i] >= 0) {
            need_temp_save = true;
            break;
        }
        if (nc->local_regs[i] < 0) {
            need_temp_save = true;
            break;
        }
    }
    
    /* Only save arguments if we need to move them somewhere else */
    /* Note: $t0-$t7 are scratch; eval stack uses callee-saved $s0-$s7 instead */
    if (need_temp_save) {
        for (int i = 0; i < sig->arg_count && i < 4; i++) {
            mips_move(em, temp_regs[i], arg_regs[i]);
        }
    }
    
    /* Set up stack frame */
    /* Only allocate stack if we need it (leaf without locals can skip) */
    if (!nc->is_leaf || nc->needs_fp || sig->arg_count > 4) {
        mips_addiu(em, REG_SP, REG_SP, -frame_size);
    }
    
    /* Only save $ra for non-leaf functions */
    if (!nc->is_leaf) {
        mips_sw(em, REG_RA, frame_size - 4, REG_SP);
    }
    
    /* Only save/setup $fp if we need it */
    if (nc->needs_fp) {
        mips_sw(em, REG_FP, frame_size - 8, REG_SP);
        mips_move(em, REG_FP, REG_SP);
    }
    
    /* Save used $s registers (after $ra and $fp) */
    if (saved_reg_count > 0) {
        int offset = frame_size - 12;  /* Start after $ra(- 4) and $fp(-8) */
        for (int i = 0; i < 8; i++) {
            if (nc->used_saved_regs & (1 << i)) {
                mips_sw(em, saved_regs[i], offset, REG_SP);
                offset -= 4;
            }
        }
    }
    
    /* Move arguments to their final locations */
    for (int i = 0; i < sig->arg_count && i < 4; i++) {
        if (nc->local_regs[i] == arg_regs[i]) {
            /* Argument stays in A0-A3 - no move needed if we didn't save it */
            if (need_temp_save) {
                mips_move(em, arg_regs[i], temp_regs[i]);
            }
        } else if (nc->local_regs[i] >= 0) {
            /* Move to allocated register (including $s registers) */
            mips_move(em, nc->local_regs[i], need_temp_save ? temp_regs[i] : arg_regs[i]);
        } else {
            /* Save to stack */
            mips_sw(em, need_temp_save ? temp_regs[i] : arg_regs[i], 
                    nc->local_stack_offs[i], REG_FP);
        }
    }
    
    /* Fix up stack-passed argument offsets (args 4+).
     * These were marked with sentinel -2 in allocate_locals.
     *
     * The 32-bit MIPS calling convention this compiler targets reserves a
     * 16-byte (4-word) argument save area for $a0-$a3 at the bottom of the
     * outgoing argument block, even though those first 4 args arrive in
     * registers. Stack-passed args (index 4+) are placed immediately above
     * that reserved area, so arg i sits at byte offset i*4 from the
     * caller's $sp at the call site - not (i-4)*4. Getting this wrong reads
     * whatever the caller spilled into its a0-a3 save slots instead of the
     * real argument (e.g. arg 6 silently returning arg 2's value). */
    for (int i = 4; i < sig->arg_count && i < NC_MAX_LOCALS; i++) {
        if (nc->local_stack_offs[i] == -2) {
            nc->local_stack_offs[i] = frame_size + i * 4;
        }
    }
    
    /* Store the frame size for epilogue */
    nc->emitter.stack_offset = frame_size;
}

/*
 * Emit function epilogue (return).
 */
static void emit_function_epilogue(NativeCompiler *nc, const NativeFuncSignature *sig) {
    MipsEmitter *em = &nc->emitter;
    int frame_size = nc->emitter.stack_offset;
    
    /* Restore used $s registers (before restoring $ra/$fp) */
    const MipsReg saved_regs[] = { REG_S0, REG_S1, REG_S2, REG_S3, REG_S4, REG_S5, REG_S6, REG_S7 };
    int saved_reg_count = 0;
    for (int i = 0; i < 8; i++) {
        if (nc->used_saved_regs & (1 << i)) {
            saved_reg_count++;
        }
    }
    
    if (saved_reg_count > 0) {
        int offset = frame_size - 12;  /* Start after $ra and $fp */
        for (int i = 0; i < 8; i++) {
            if (nc->used_saved_regs & (1 << i)) {
                mips_lw(em, saved_regs[i], offset, REG_SP);
                offset -= 4;
            }
        }
    }
    
    /* Restore RA (only for non-leaf) */
    if (!nc->is_leaf) {
        mips_lw(em, REG_RA, frame_size - 4, REG_SP);
    }
    
    /* Restore FP (only if we saved it) */
    if (nc->needs_fp) {
        mips_lw(em, REG_FP, frame_size - 8, REG_SP);
    }
    
    /* For leaf functions without frame, try to fill delay slot with previous instruction */
    if (nc->is_leaf && !nc->needs_fp) {
        /* Look at previous non-NOP instruction */
        size_t prev_idx = em->buffer.count - 1;
        size_t start = em->buffer.function_start;
        
        /* Skip NOPs to find actual instruction */
        while (prev_idx > start && em->buffer.code[prev_idx] == 0) {
            prev_idx--;
        }
        
        uint32_t prev_instr = em->buffer.code[prev_idx];
        uint32_t opcode = (prev_instr >> 26) & 0x3F;
        
        /* Check if it's a safe instruction to move into delay slot
         * Safe = not a branch/jump, doesn't use $ra, simple ALU/load immediate */
        bool safe_to_move = false;
        
        /* ADDIU (opcode 0x09) is safe if it doesn't involve $ra */
        if (opcode == 0x09) {  /* ADDIU */
            uint32_t rs = (prev_instr >> 21) & 0x1F;
            uint32_t rt = (prev_instr >> 16) & 0x1F;
            if (rs != REG_RA && rt != REG_RA) {
                safe_to_move = true;
            }
        }
        /* R-type ALU ops (opcode 0x00) are safe except JR/JALR/MFLO/MFHI */
        else if (opcode == 0x00) {
            uint32_t funct = prev_instr & 0x3F;
            /* Exclude: JR (0x08), JALR (0x09), MFHI (0x10), MFLO (0x12) */
            if (funct != 0x08 && funct != 0x09 && funct != 0x10 && funct != 0x12) {
                uint32_t rs = (prev_instr >> 21) & 0x1F;
                uint32_t rt = (prev_instr >> 16) & 0x1F;
                uint32_t rd = (prev_instr >> 11) & 0x1F;
                if (rs != REG_RA && rt != REG_RA && rd != REG_RA) {
                    safe_to_move = true;
                }
            }
        }
        
        if (safe_to_move && prev_instr != 0) {  /* Not a NOP */
            /* Move previous instruction into delay slot */
            em->buffer.code[prev_idx] = 0;  /* Replace with NOP */
            mips_jr(em, REG_RA);
            mips_emit_raw(em, prev_instr);  /* Previous instruction in delay slot */
            return;
        }
    }
    
    /* Default: JR with appropriate delay slot */
    mips_jr(em, REG_RA);
    
    /* Only restore stack if we allocated it */
    if (!nc->is_leaf || nc->needs_fp) {
        mips_addiu(em, REG_SP, REG_SP, frame_size);  /* In delay slot */
    } else {
        mips_nop(em);  /* Delay slot */
    }
}

/*
 * Push a new value onto the eval operand stack ($s0-$s7), checking capacity
 * right at the push instead of blanket-rejecting at the top of every
 * instruction. The old top-of-function check rejected ANY instruction
 * (including IR_CALL/IR_TAIL_CALL) whenever stack_top was already 8, even
 * though calls POP their callee + all args before pushing a single result -
 * i.e. they reduce usage. That capped real nested-call argument capacity at
 * 7 instead of 8 (funcptr + up to 7 args), and rejected calls that were
 * about to resolve themselves. Checking here instead only blocks a genuine
 * push overflow, wherever it happens in an instruction's expansion.
 */
static MipsReg nc_push_reg(NativeCompiler *nc) {
    if (nc->stack_top >= 8) {
        if (!nc->has_error) {
            nc->has_error = true;
            snprintf(nc->error_msg, sizeof(nc->error_msg),
                     "Stack overflow: stack_top=%d (max 8)", nc->stack_top);
        }
        return (MipsReg)(REG_S0 + 7);  /* has_error aborts the compile below */
    }
    return (MipsReg)(REG_S0 + nc->stack_top++);
}

/*
 * Emit code for a single IR instruction.
 * Uses a simple stack-based approach with registers.
 * block_to_label: mapping from block index to MIPS label ID (can be NULL for instructions that don't need it)
 */
static int emit_ir_instruction(NativeCompiler *nc, const IRInstr *instr, const int *block_to_label) {
    MipsEmitter *em = &nc->emitter;

    /* Log all IR instructions */
    /* op_names removed to fix unused variable warning */

    /* Eval operand stack: $s0-$s7 (callee-saved, preserved across C runtime calls) */
    if (nc->stack_top < 0) {
        nc->stack_top = 0;  /* Reset if needed */
    }
    if (nc->stack_depth < 0) {
        nc->stack_depth = 0;  /* Reset if needed */
    }

    #define STACK_REG(n) (REG_S0 + (n))
    #define PUSH_REG() nc_push_reg(nc)
    #define POP_REG() (STACK_REG(--nc->stack_top))
    #define PEEK_REG() (STACK_REG(nc->stack_top - 1))

    /* Type stack helpers - track types on the operand stack */
    #define PUSH_TYPE(t) do { \
        if (nc->stack_depth < NC_MAX_STACK_DEPTH) { \
            nc->stack_types[nc->stack_depth++] = (t); \
        } \
    } while(0)
    #define POP_TYPE() ((nc->stack_depth > 0) ? nc->stack_types[--nc->stack_depth] : NATIVE_TYPE_UNKNOWN)
    #define PEEK_TYPE(n) ((nc->stack_depth > (n) && (n) >= 0) ? nc->stack_types[nc->stack_depth - 1 - (n)] : NATIVE_TYPE_UNKNOWN)
    
    NC_LOG("emit: ir_op=%d type=%d stack_top=%d stack_depth=%d",
           instr->op, instr->type, nc->stack_top, nc->stack_depth);
    
    switch (instr->op) {
        case IR_NOP:
            /* No operation - used by optimization passes */
            break;
            
        case IR_CONST_I32: {
            MipsReg dst = PUSH_REG();
            mips_li(em, dst, instr->operand.i32);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        case IR_CONST_F32: {
            /* Load float constant via integer register */
            MipsReg tmp = PUSH_REG();
            union { float f; int32_t i; } u;
            u.f = instr->operand.f32;
            mips_li(em, tmp, u.i);
            /* For actual use, move to FPU register */
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        case IR_CONST_I64: {
            /* Load 64-bit constant into register
             * Use LUI + ORI for upper 32 bits, then DSLL32 + ORI for lower 32 bits
             * For small values that fit in 32 bits, just sign-extend */
            MipsReg dst = PUSH_REG();
            int64_t val = instr->operand.i64;
            
            if (val >= INT32_MIN && val <= INT32_MAX) {
                /* Fits in 32 bits - use LI and sign-extend with DADDU */
                mips_li(em, dst, (int32_t)val);
                /* The value is already sign-extended in the 64-bit register */
            } else {
                /* Need full 64-bit load */
                int32_t upper = (int32_t)(val >> 32);
                int32_t lower = (int32_t)(val & 0xFFFFFFFF);
                
                /* Load upper 32 bits */
                mips_li(em, dst, upper);
                /* Shift left by 32 */
                mips_dsll32(em, dst, dst, 0);
                /* Add lower 32 bits */
                if (lower != 0) {
                    MipsReg tmp = mips_alloc_gpr(em);
                    mips_li(em, tmp, lower);
                    mips_daddu(em, dst, dst, tmp);
                    mips_free_gpr(em, tmp);
                }
            }
            PUSH_TYPE(NATIVE_TYPE_INT64);
            break;
        }
        
        case IR_LOAD_LOCAL: {
            int idx = instr->operand.local_idx;
            int src_reg = (int)nc->local_regs[idx];  /* Cast to signed int for comparison */
            
            if (idx >= 2) {
                NC_LOG("IR_LOAD_LOCAL: idx=%d, reg=%d, offset=%d",
                       idx, src_reg, nc->local_stack_offs[idx]);
            }
            
            /* CRITICAL: Always allocate a NEW stack register for the loaded value.
             * Never reuse the source register, even if the local is in a register.
             * This prevents aliasing issues where modifying the stack value would
             * also modify the local variable's register. */
            MipsReg dst = PUSH_REG();
            
            if (src_reg >= 0) {
                /* Already in register - copy to stack register */
                mips_move(em, dst, (MipsReg)src_reg);
            } else {
                /* TEMPORARY WORKAROUND: Allow -1 offset for now
                 * TODO: Fix allocate_locals to properly allocate temp variables in struct methods */
                /*
                int offset = nc->local_stack_offs[idx];
                if (offset < 0) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg),
                             "IR_LOAD_LOCAL: invalid stack offset %d for local %d", offset, idx);
                    return -1;
                }
                */
                /* Load from stack */
                mips_lw(em, dst, nc->local_stack_offs[idx], REG_FP);
            }
            
            /* Push the type of the local variable */
            NativeType local_type = (idx < nc->ir_func.local_count) ? nc->ir_func.local_types[idx] : NATIVE_TYPE_UNKNOWN;
            PUSH_TYPE(local_type);
            
            /* Track which local this stack item came from using IR metadata */
            if (instr->source_local_idx >= 0 && nc->stack_depth > 0 && nc->stack_depth <= NC_MAX_STACK_DEPTH) {
                nc->stack_local_source[nc->stack_depth - 1] = instr->source_local_idx;
            }
            break;
        }
        
        case IR_STORE_LOCAL: {
            int idx = instr->operand.local_idx;
            MipsReg src = POP_REG();
            NativeType src_type = POP_TYPE();  /* Pop source type */
            
            /* FIX: Get the declared type of the destination local variable */
            NativeType dst_type = NATIVE_TYPE_UNKNOWN;
            if (idx < nc->ir_func.local_count) {
                dst_type = nc->ir_func.local_types[idx];
            }
            
            /* FIX: Convert if there's a type mismatch */
            if (src_type == NATIVE_TYPE_FLOAT32 && dst_type == NATIVE_TYPE_INT32) {
                /* Float bits -> Int bits (Truncate) */
                MipsFpuReg fpu = mips_alloc_fpr(em);
                mips_mtc1(em, src, fpu);        /* Move float bits to FPU */
                mips_cvt_w_s(em, fpu, fpu);     /* Convert Single(Float) to Word(Int) */
                mips_mfc1(em, src, fpu);        /* Move int bits back */
                mips_free_fpr(em, fpu);
            }
            else if (src_type == NATIVE_TYPE_INT32 && dst_type == NATIVE_TYPE_FLOAT32) {
                /* Int bits -> Float bits */
                MipsFpuReg fpu = mips_alloc_fpr(em);
                mips_mtc1(em, src, fpu);
                mips_cvt_s_w(em, fpu, fpu);     /* Convert Word(Int) to Single(Float) */
                mips_mfc1(em, src, fpu);
                mips_free_fpr(em, fpu);
            }
            
            if (nc->local_regs[idx] >= 0) {
                NC_LOG("IR_STORE_LOCAL: idx=%d -> MOVE to reg %d", idx, nc->local_regs[idx]);
                /* Store to register */
                mips_move(em, nc->local_regs[idx], src);
            } else {
                NC_LOG("IR_STORE_LOCAL: idx=%d -> SW to offset %d (reg=%d)",
                       idx, nc->local_stack_offs[idx], nc->local_regs[idx]);
                /* TEMPORARY WORKAROUND: Allow -1 offset for now */
                /*
                int offset = nc->local_stack_offs[idx];
                if (offset < 0) {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg),
                             "IR_STORE_LOCAL: invalid stack offset %d for local %d", offset, idx);
                    return -1;
                }
                */
                /* Store to stack */
                mips_sw(em, src, nc->local_stack_offs[idx], REG_FP);
            }
            break;
        }
        
        case IR_ADD_LOCAL_CONST: {
            /* Optimized local += const: generates LW + ADDIU + SW instead of LW + ADDIU + ADDU + SW */
            int idx = instr->operand.local_add.local_idx;
            int32_t const_val = instr->operand.local_add.const_val;
            
            if (nc->local_regs[idx] >= 0) {
                /* Local in register - just ADDIU */
                MipsReg reg = nc->local_regs[idx];
                if (const_val >= -32768 && const_val <= 32767) {
                    mips_addiu(em, reg, reg, (int16_t)const_val);
                } else {
                    /* Large constant - need temp register (avoid $at clobbering) */
                    MipsReg tmp = PUSH_REG();
                    mips_li(em, tmp, const_val);
                    mips_addu(em, reg, reg, tmp);
                    POP_REG();
                }
            } else {
                /* Local on stack - LW + ADDIU + SW 
                 * CRITICAL: Use PUSH_REG instead of $at to avoid clobbering array index calcs */
                int offset = nc->local_stack_offs[idx];
                MipsReg tmp = PUSH_REG();  /* Allocate temp from T0-T7 */
                mips_lw(em, tmp, offset, REG_FP);
                if (const_val >= -32768 && const_val <= 32767) {
                    mips_addiu(em, tmp, tmp, (int16_t)const_val);
                } else {
                    /* Large constant - need another temp */
                    MipsReg tmp2 = PUSH_REG();
                    mips_li(em, tmp2, const_val);
                    mips_addu(em, tmp, tmp, tmp2);
                    POP_REG();  /* Free tmp2 */
                }
                mips_sw(em, tmp, offset, REG_FP);
                POP_REG();  /* Free tmp */
            }
            break;
        }
        
        case IR_LOAD_ARRAY: {
            /* Stack: [base, index] -> [value]
             * QuickJS: sp[-2] = base, sp[-1] = index
             * LIFO: index is on top, so POP_REG() gets index first */
            MipsReg idx_reg = POP_REG();   /* Pop index first (was sp[-1]) */
            NativeType idx_type = POP_TYPE();  /* Pop index type */
            MipsReg base_reg = POP_REG();  /* Pop base second (was sp[-2]) */
            NativeType base_type = POP_TYPE();  /* Pop base type (array type) */
            
            /* Dynamic arrays pass a NativeDynamicArray* struct; the element buffer
             * is at arr->data (offset 0). Typed arrays pass the buffer directly. */
            if (NATIVE_TYPE_IS_DYNAMIC_ARRAY(base_type)) {
                mips_lw(em, base_reg, 0, base_reg);  /* base = arr->data */
            }
            
            /* Calculate offset: idx * 4 (assuming 32-bit elements) */
            mips_sll(em, REG_AT, idx_reg, 2);
            mips_addu(em, REG_AT, base_reg, REG_AT);
            
            /* Determine element type from array type */
            NativeType elem_type = native_array_element_type(base_type);
            if (elem_type == NATIVE_TYPE_UNKNOWN) {
                elem_type = instr->type;  /* Fall back to inferred type */
            }
            
            /* Check if we need to load float or integer */
            if (elem_type == NATIVE_TYPE_FLOAT32 || instr->type == NATIVE_TYPE_FLOAT32) {
                /* Load float: use LWC1 and move to FPU register, then back to integer reg */
                MipsFpuReg fpu_reg = mips_alloc_fpr(em);
                mips_lwc1(em, fpu_reg, 0, REG_AT);
                MipsReg dst = PUSH_REG();
                mips_mfc1(em, dst, fpu_reg);
                mips_free_fpr(em, fpu_reg);
                PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            } else {
                /* Load integer */
                MipsReg dst = PUSH_REG();
                mips_lw(em, dst, 0, REG_AT);
                PUSH_TYPE(elem_type != NATIVE_TYPE_UNKNOWN ? elem_type : NATIVE_TYPE_INT32);
            }
            break;
        }

        case IR_ARRAY_ELEM_ADDR: {
            /* Struct array indexing (arr[i] where arr is a struct-array
             * argument): push the ADDRESS of the i-th element, not a loaded
             * scalar - a struct element can't be loaded into one register.
             * Stack: [base, index] -> [element_addr], same push/pop order as
             * IR_LOAD_ARRAY above (base pushed first, index on top).
             *
             * elem_size (sizeof the struct) is a compile-time constant but
             * general - e.g. a 3-float struct is 12 bytes, not a power of 2 -
             * so this always does a real MULT/MFLO like IR_MUL_I32, rather
             * than the shift-based strength reduction peephole (which only
             * fires for a literal IR_CONST_I32 immediately preceding an
             * IR_MUL_I32 in the bytecode stream; the constant here is baked
             * into this op's operand instead, so that peephole never sees it). */
            MipsReg idx_reg = POP_REG();
            NativeType idx_type = POP_TYPE(); (void)idx_type;
            MipsReg base_reg = POP_REG();
            NativeType base_type = POP_TYPE(); (void)base_type;

            /* PUSH_REG() reclaims the eval-stack slot base_reg just vacated,
             * so a freshly-pushed dst can be the SAME physical register as
             * base_reg. Finish every read of base_reg (and idx_reg) into a
             * scratch GPR - one that can't alias either, since mips_alloc_gpr
             * comes from a separate temp pool - before calling PUSH_REG(),
             * or the later add would silently read back its own just-written
             * result instead of the original base pointer. */
            MipsReg size_reg = mips_alloc_gpr(em);
            mips_li(em, size_reg, instr->operand.array_elem.elem_size);
            mips_mult(em, idx_reg, size_reg);
            mips_mflo(em, size_reg);
            mips_addu(em, size_reg, size_reg, base_reg);

            MipsReg dst = PUSH_REG();
            mips_move(em, dst, size_reg);
            mips_free_gpr(em, size_reg);
            PUSH_TYPE(NATIVE_TYPE_PTR);
            break;
        }

        case IR_STORE_ARRAY: {
            /* Stack: [base, index, value] -> []
             * QuickJS: sp[-3] = base, sp[-2] = index, sp[-1] = value
             * LIFO: value is on top, so POP_REG() gets value first
             * 
             * PROBLEM: In loops like result[i] = a[i] + b[i], QuickJS pushes
             * result and i early, but they get consumed during the calculation.
             * When IR_STORE_ARRAY executes, the stack may only have the value.
             * 
             * SOLUTION: If stack doesn't have 3 values, reload base and index
             * from memory (from their original storage locations).
             */
            
            MipsReg val_reg, idx_reg, base_reg;
            NativeType val_type, idx_type, base_type;
            
            /* Pop value first (always on top) */
            if (nc->stack_top < 1) {
                nc->has_error = true;
                snprintf(nc->error_msg, sizeof(nc->error_msg),
                         "IR_STORE_ARRAY: Stack underflow (need at least value)");
                return -1;
            }
            val_reg = POP_REG();
            val_type = POP_TYPE();
            
            /* Check if we have base and index on stack */
            if (nc->stack_top >= 2) {
                /* Stack has both base and index - use them */
                NC_LOG("IR_STORE_ARRAY: using values from stack (stack_top=%d)", nc->stack_top);
                idx_reg = POP_REG();
                idx_type = POP_TYPE();
                base_reg = POP_REG();
                base_type = POP_TYPE();
            } else {
                NC_LOG("IR_STORE_ARRAY: reloading from memory (stack_top=%d)", nc->stack_top);
                /* Stack doesn't have base and index - reload from memory */
                /* Find the destination array or struct pointer (last array/ptr argument) */
                int base_arg_idx = -1;
                for (int i = nc->ir_func.sig.arg_count - 1; i >= 0 && i < 4; i--) {
                    if (NATIVE_TYPE_IS_ARRAY(nc->ir_func.sig.arg_types[i]) ||
                        nc->ir_func.sig.arg_types[i] == NATIVE_TYPE_PTR) {
                        base_arg_idx = i;
                        break;
                    }
                }
                
                const MipsReg arg_regs[] = { REG_A0, REG_A1, REG_A2, REG_A3 };
                base_reg = REG_T6;
                idx_reg = REG_T7;
                
                if (base_arg_idx >= 0 && base_arg_idx < 4) {
                    /* Reload base from argument register */
                    mips_move(em, base_reg, arg_regs[base_arg_idx]);
                    base_type = nc->ir_func.sig.arg_types[base_arg_idx];
                } else {
                    nc->has_error = true;
                    snprintf(nc->error_msg, sizeof(nc->error_msg),
                             "IR_STORE_ARRAY: Could not find array base argument");
                    return -1;
                }
                
                /* Find index variable (first INT32 local variable after arguments) */
                int idx_local_idx = -1;
                for (int i = nc->ir_func.sig.arg_count; i < nc->ir_func.local_count; i++) {
                    if (nc->ir_func.local_types[i] == NATIVE_TYPE_INT32 ||
                        nc->ir_func.local_types[i] == NATIVE_TYPE_UINT32 ||
                        nc->ir_func.local_types[i] == NATIVE_TYPE_BOOL) {
                        idx_local_idx = i;
                        break;
                    }
                }
                
                if (idx_local_idx >= 0) {
                    /* Reload index from local variable */
                    if (nc->local_regs[idx_local_idx] < 0) {
                        mips_lw(em, idx_reg, nc->local_stack_offs[idx_local_idx], REG_FP);
                    } else {
                        mips_move(em, idx_reg, nc->local_regs[idx_local_idx]);
                    }
                    idx_type = nc->ir_func.local_types[idx_local_idx];
                } else {
                    /* Index not found - use value from stack if available */
                    if (nc->stack_top >= 1) {
                        idx_reg = POP_REG();
                        idx_type = POP_TYPE();
                    } else {
                        nc->has_error = true;
                        snprintf(nc->error_msg, sizeof(nc->error_msg),
                                 "IR_STORE_ARRAY: Could not find index");
                        return -1;
                    }
                }
                
                /* Discard any remaining values from stack */
                while (nc->stack_top > 0) {
                    POP_REG();
                    POP_TYPE();
                }
                
                /* CRITICAL: Reload index IMMEDIATELY before calculating address.
                 * The index may have been incremented since it was first loaded,
                 * so we must reload it from its storage location right before use. */
                if (idx_local_idx >= 0) {
                    if (nc->local_regs[idx_local_idx] < 0) {
                        mips_lw(em, idx_reg, nc->local_stack_offs[idx_local_idx], REG_FP);
                    } else {
                        mips_move(em, idx_reg, nc->local_regs[idx_local_idx]);
                    }
                }
            }
            
            /* Dynamic arrays pass a NativeDynamicArray* struct; the element buffer
             * is at arr->data (offset 0). Typed arrays pass the buffer directly. */
            if (NATIVE_TYPE_IS_DYNAMIC_ARRAY(base_type)) {
                mips_lw(em, base_reg, 0, base_reg);  /* base = arr->data */
            }
            
            /* Calculate address: base + (idx * 4) */
            mips_sll(em, REG_AT, idx_reg, 2);  /* idx * 4 */
            mips_addu(em, REG_AT, base_reg, REG_AT);  /* base + (idx * 4) */
            
            /* Determine element type from array type */
            NativeType elem_type = native_array_element_type(base_type);
            
            /* If base_type is unknown, try to infer from value type */
            if (elem_type == NATIVE_TYPE_UNKNOWN) {
                if (val_type == NATIVE_TYPE_FLOAT32) {
                    elem_type = NATIVE_TYPE_FLOAT32;
                } else {
                    /* Fall back to checking function signature */
                    const NativeFuncSignature *sig = &nc->ir_func.sig;
                    bool has_float32_array = false;
                    for (int i = 0; i < sig->arg_count; i++) {
                        if (sig->arg_types[i] == NATIVE_TYPE_FLOAT32_ARRAY) {
                            has_float32_array = true;
                            break;
                        }
                    }
                    if (val_type == NATIVE_TYPE_FLOAT32 && has_float32_array) {
                        elem_type = NATIVE_TYPE_FLOAT32;
                    } else {
                        elem_type = val_type != NATIVE_TYPE_UNKNOWN ? val_type : NATIVE_TYPE_INT32;
                    }
                }
            }
            
            /* Store the value */
            if (elem_type == NATIVE_TYPE_FLOAT32) {
                /* Store float: move value from integer register to FPU register, then use SWC1 */
                MipsFpuReg fpu_reg = mips_alloc_fpr(em);
                mips_mtc1(em, val_reg, fpu_reg);
                
                /* FIX: If the original value was integer, CONVERT to float before storing */
                if (val_type == NATIVE_TYPE_INT32 || val_type == NATIVE_TYPE_UINT32 || val_type == NATIVE_TYPE_BOOL) {
                    mips_cvt_s_w(em, fpu_reg, fpu_reg);
                }
                
                mips_swc1(em, fpu_reg, 0, REG_AT);
                mips_free_fpr(em, fpu_reg);
            } else {
                /* Store integer */
                mips_sw(em, val_reg, 0, REG_AT);
            }
            break;
        }
        
        /* Struct field address - push address of array field for IR_LOAD_ARRAY */
        case IR_LOAD_FIELD_ADDR: {
            /* For array fields like mat.m, we need to push the base address
             * of the array (struct_ptr + field_offset) for subsequent
             * IR_LOAD_ARRAY or IR_STORE_ARRAY operations.
             */
            int16_t offset = instr->operand.field.offset;
            MipsReg base_reg = POP_REG();
            (void)POP_TYPE();

            /* Push (base + offset) as the array base address */
            MipsReg dst = PUSH_REG();
            if (offset == 0) {
                mips_move(em, dst, base_reg);
            } else {
                mips_addiu(em, dst, base_reg, offset);
            }
            PUSH_TYPE(instr->type);  /* Push array type (e.g., FLOAT32_ARRAY) for IR_LOAD_ARRAY */
            break;
        }
        
        /* Integer arithmetic */
        case IR_ADD_I32: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            /* Check if both operands are float - use FPU if so */
            if (t1 == NATIVE_TYPE_FLOAT32 && t2 == NATIVE_TYPE_FLOAT32) {
                /* Float addition using FPU */
                MipsFpuReg f1 = mips_alloc_fpr(em);
                MipsFpuReg f2 = mips_alloc_fpr(em);
                MipsFpuReg fdst = mips_alloc_fpr(em);
                
                mips_mtc1(em, r1, f1);
                mips_mtc1(em, r2, f2);
                mips_add_s(em, fdst, f1, f2);
                
                MipsReg dst = PUSH_REG();
                mips_mfc1(em, dst, fdst);
                
                mips_free_fpr(em, f1);
                mips_free_fpr(em, f2);
                mips_free_fpr(em, fdst);
                
                PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            } else {
                /* Integer addition */
                MipsReg dst = PUSH_REG();
                mips_addu(em, dst, r1, r2);
                PUSH_TYPE(NATIVE_TYPE_INT32);
            }
            break;
        }
        
        case IR_SUB_I32: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            mips_subu(em, dst, r1, r2);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        case IR_MUL_I32: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            mips_mult(em, r1, r2);
            mips_mflo(em, dst);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        case IR_DIV_I32: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            mips_div(em, r1, r2);
            mips_mflo(em, dst);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        case IR_MOD_I32: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            mips_div(em, r1, r2);
            mips_mfhi(em, dst);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        case IR_NEG_I32: {
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            mips_subu(em, dst, REG_ZERO, r1);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        /* ============================================
         * 64-bit Integer Arithmetic (MIPS64)
         * ============================================ */
        
        case IR_ADD_I64: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            mips_daddu(em, dst, r1, r2);
            PUSH_TYPE(NATIVE_TYPE_INT64);
            break;
        }
        
        case IR_SUB_I64: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            mips_dsubu(em, dst, r1, r2);
            PUSH_TYPE(NATIVE_TYPE_INT64);
            break;
        }
        
        case IR_NEG_I64: {
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            mips_dsubu(em, dst, REG_ZERO, r1);
            PUSH_TYPE(NATIVE_TYPE_INT64);
            break;
        }
        
        case IR_SHL_I64: {
            MipsReg r2 = POP_REG();  /* Shift amount */
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();  /* Value to shift */
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            /* Use variable shift if amount is in register */
            mips_dsllv(em, dst, r1, r2);
            PUSH_TYPE(NATIVE_TYPE_INT64);
            break;
        }
        
        case IR_SHR_I64: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            mips_dsrlv(em, dst, r1, r2);  /* Logical shift right */
            PUSH_TYPE(NATIVE_TYPE_INT64);
            break;
        }
        
        case IR_SAR_I64: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            mips_dsrav(em, dst, r1, r2);  /* Arithmetic shift right */
            PUSH_TYPE(NATIVE_TYPE_INT64);
            break;
        }
        
        /* ============================================
         * Int64 Emulated Operations (DMULT/DDIV not available on R5900)
         * FIX: Must save $ra before JALR since caller expects to return
         * ============================================ */
        
        case IR_MUL_I64: {
            /* 64-bit multiplication via runtime function
             * CRITICAL: Save $ra on stack before JALR, restore after */
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            /* Save $ra on stack (4 bytes below current $sp) */
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            /* Setup arguments */
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            
            /* Load function address and call */
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)__dmult_i64 >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)__dmult_i64 & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            /* Restore $ra and stack */
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            /* Result in $v0 */
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_INT64);
            break;
        }
        
        case IR_DIV_I64: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)__ddiv_i64 >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)__ddiv_i64 & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_INT64);
            break;
        }
        
        case IR_MOD_I64: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)__dmod_i64 >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)__dmod_i64 & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_INT64);
            break;
        }

        /* ============================================
         * String Operations
         * ============================================ */

        case IR_STRING_NEW: {
            int lit_idx = instr->operand.i32;
            if (lit_idx < 0 || lit_idx >= nc->compile_string_count ||
                !nc->compile_string_literals) {
                nc->has_error = true;
                snprintf(nc->error_msg, sizeof(nc->error_msg),
                         "Invalid string literal index %d", lit_idx);
                return -1;
            }
            const char *lit = nc->compile_string_literals[lit_idx];
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            mips_la(em, REG_A0, (void *)lit);
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_string_from_cstr >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_string_from_cstr & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_STRING);
            break;
        }
        
        case IR_STRING_CONCAT: {
            /* Concatenate two strings: result = native_string_concat(a, b) */
            MipsReg r2 = POP_REG();  /* string b */
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();  /* string a */
            NativeType t1 = POP_TYPE();
            
            /* Save $ra */
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            /* Setup args */
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            
            /* Call native_string_concat */
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_string_concat >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_string_concat & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            /* Restore $ra */
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_STRING);
            break;
        }
        
        case IR_STRING_LENGTH: {
            /* Get string length - just read the length field from NativeString struct */
            /* struct NativeString { char *data; size_t length; size_t capacity; } */
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            MipsReg dst = PUSH_REG();
            mips_lw(em, dst, 4, r1);  /* Load length field (offset 4) */
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        case IR_STRING_COMPARE: {
            /* Compare two strings: result = native_string_compare(a, b) */
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_string_compare >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_string_compare & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        case IR_STRING_EQUALS: {
            /* Check equality: result = native_string_equals(a, b) */
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_string_equals >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_string_equals & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_BOOL);
            break;
        }

        case IR_STRING_NE: {
            MipsReg r2 = POP_REG();
            POP_TYPE();
            MipsReg r1 = POP_REG();
            POP_TYPE();
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_string_equals >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_string_equals & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            MipsReg dst = PUSH_REG();
            mips_xori(em, dst, REG_V0, 1);
            PUSH_TYPE(NATIVE_TYPE_BOOL);
            break;
        }
        
        case IR_STRING_SLICE: {
            /* Get substring: result = native_string_slice(str, start, end) */
            MipsReg r3 = POP_REG();  /* end */
            NativeType t3 = POP_TYPE();
            MipsReg r2 = POP_REG();  /* start */
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();  /* str */
            NativeType t1 = POP_TYPE();
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            mips_move(em, REG_A2, r3);
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_string_slice >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_string_slice & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_STRING);
            break;
        }
        
        case IR_STRING_TO_UPPER: {
            /* Convert to uppercase: result = native_string_to_upper(str) */
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_string_to_upper >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_string_to_upper & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_STRING);
            break;
        }
        
        case IR_STRING_TO_LOWER: {
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_string_to_lower >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_string_to_lower & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_STRING);
            break;
        }
        
        case IR_STRING_TRIM: {
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_string_trim >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_string_trim & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_STRING);
            break;
        }
        
        case IR_STRING_FIND: {
            MipsReg r2 = POP_REG();  /* substr */
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();  /* str */
            NativeType t1 = POP_TYPE();
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_string_find >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_string_find & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }

        case IR_STRING_REPLACE: {
            MipsReg r3 = POP_REG();
            POP_TYPE();
            MipsReg r2 = POP_REG();
            POP_TYPE();
            MipsReg r1 = POP_REG();
            POP_TYPE();
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            mips_move(em, REG_A2, r3);
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_string_replace >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_string_replace & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_STRING);
            break;
        }

        /* ============================================
         * Dynamic Array Operations
         * ============================================ */

        case IR_ARRAY_NEW: {
            NativeType elem = instr->operand.field.field_type;
            int32_t cap = instr->operand.i32;
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            mips_li(em, REG_A1, cap);
            mips_li(em, REG_A0, (int32_t)elem);
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_array_new >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_array_new & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            if (elem == NATIVE_TYPE_UINT32)
                PUSH_TYPE(NATIVE_TYPE_DYNAMIC_UINT32_ARRAY);
            else if (elem == NATIVE_TYPE_FLOAT32)
                PUSH_TYPE(NATIVE_TYPE_DYNAMIC_FLOAT32_ARRAY);
            else
                PUSH_TYPE(NATIVE_TYPE_DYNAMIC_INT32_ARRAY);
            break;
        }
        
        case IR_ARRAY_LENGTH: {
            /* Get array length - read length field from NativeDynamicArray struct */
            /* struct { void *data; size_t length; size_t capacity; ... } */
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            MipsReg dst = PUSH_REG();
            mips_lw(em, dst, 4, r1);  /* Load length field (offset 4) */
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        case IR_ARRAY_PUSH: {
            /* Push element: native_array_push_*(arr, value) */
            MipsReg r2 = POP_REG();  /* value */
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();  /* arr */
            NativeType t1 = POP_TYPE();
            NativeType elem = instr->operand.field.field_type;
            void *push_fn = nc_array_push_fn(elem);
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            /* native_array_push_f32_bits takes the value as raw 32-bit float
             * pattern in a GPR. If the source value is an integer, reinterpret
             * it as a float (numeric convert) so the stored bits are correct. */
            if (elem == NATIVE_TYPE_FLOAT32 &&
                (t2 == NATIVE_TYPE_INT32 || t2 == NATIVE_TYPE_UINT32 || t2 == NATIVE_TYPE_BOOL)) {
                MipsFpuReg fpu = mips_alloc_fpr(em);
                mips_mtc1(em, r2, fpu);
                mips_cvt_s_w(em, fpu, fpu);
                mips_mfc1(em, REG_A1, fpu);
                mips_free_fpr(em, fpu);
            } else {
                mips_move(em, REG_A1, r2);
            }
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)push_fn >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)push_fn & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_BOOL);
            break;
        }

        case IR_ARRAY_POP: {
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            NativeType elem = instr->operand.field.field_type;
            void *pop_fn = nc_array_pop_fn(elem);
            /* [reg] this frame was -12, which desyncs $sp from the 8-byte
             * alignment the MIPS N32 ABI requires at every call boundary
             * (every other call site in this file uses -8). pop_fn is a
             * GCC-compiled C function; a misaligned $sp faults the moment it
             * does any 8-byte-aligned access relative to it. */
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_A1, REG_SP, 0);
            mips_move(em, REG_A0, r1);
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)pop_fn >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)pop_fn & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_lw(em, REG_V0, 0, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(elem);
            break;
        }

        case IR_ARRAY_GET: {
            MipsReg idx = POP_REG();
            POP_TYPE();
            MipsReg arr = POP_REG();
            POP_TYPE();
            NativeType elem = instr->operand.field.field_type;
            /* one register: first arr->data, then the element - it is the
               result left on the stack (it used to be a second register,
               popped right away, so the result was the data pointer) */
            MipsReg dst = PUSH_REG();
            mips_lw(em, dst, 0, arr);
            mips_sll(em, REG_AT, idx, 2);
            mips_addu(em, REG_AT, dst, REG_AT);
            if (elem == NATIVE_TYPE_FLOAT32) {
                MipsFpuReg f = mips_alloc_fpr(em);
                mips_lwc1(em, f, 0, REG_AT);
                mips_mfc1(em, dst, f);
                mips_free_fpr(em, f);
            } else {
                mips_lw(em, dst, 0, REG_AT);
            }
            PUSH_TYPE(elem);
            break;
        }

        case IR_ARRAY_SET: {
            MipsReg val = POP_REG();
            POP_TYPE();
            MipsReg idx = POP_REG();
            POP_TYPE();
            MipsReg arr = POP_REG();
            POP_TYPE();
            NativeType elem = instr->operand.field.field_type;
            MipsReg data = PUSH_REG();
            mips_lw(em, data, 0, arr);
            mips_sll(em, REG_AT, idx, 2);
            mips_addu(em, REG_AT, data, REG_AT);
            if (elem == NATIVE_TYPE_FLOAT32) {
                MipsFpuReg f = mips_alloc_fpr(em);
                mips_mtc1(em, val, f);
                mips_swc1(em, f, 0, REG_AT);
                mips_free_fpr(em, f);
            } else {
                mips_sw(em, val, 0, REG_AT);
            }
            POP_REG();
            break;
        }

        case IR_ARRAY_INSERT: {
            MipsReg val = POP_REG();
            POP_TYPE();
            MipsReg idx = POP_REG();
            POP_TYPE();
            MipsReg arr = POP_REG();
            POP_TYPE();
            void *insert_fn = nc_array_insert_fn(instr->operand.field.field_type);
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            mips_move(em, REG_A0, arr);
            mips_move(em, REG_A1, idx);
            mips_move(em, REG_A2, val);
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)insert_fn >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)insert_fn & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_BOOL);
            break;
        }
        
        case IR_ARRAY_CLEAR: {
            /* Clear array: native_array_clear(arr) */
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_array_clear >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_array_clear & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            break;
        }
        
        case IR_ARRAY_RESIZE: {
            /* Resize array: native_array_resize(arr, new_length) */
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_array_resize >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_array_resize & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_BOOL);
            break;
        }
        
        case IR_ARRAY_RESERVE: {
            /* Reserve capacity: native_array_reserve(arr, capacity) */
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_array_reserve >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_array_reserve & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_BOOL);
            break;
        }
        
        case IR_ARRAY_REMOVE: {
            /* Remove element: native_array_remove(arr, index) */
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)native_array_remove >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)native_array_remove & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_BOOL);
            break;
        }

        /* ============================================
         * Type Conversions
         * ============================================ */
        
        case IR_I32_TO_I64: {
            /* Sign-extend 32-bit to 64-bit
             * On MIPS64, loading a 32-bit value already sign-extends */
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            /* Simple move - the register already holds sign-extended value */
            mips_addu(em, dst, r1, REG_ZERO);  /* Copy value */
            PUSH_TYPE(NATIVE_TYPE_INT64);
            break;
        }
        
        case IR_I64_TO_I32: {
            /* Truncate 64-bit to 32-bit - just use lower 32 bits */
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            /* Lower 32 bits are already in the register */
            mips_addu(em, dst, r1, REG_ZERO);  /* Copy value */
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        /* Note: I64_TO_F32 and F32_TO_I64 conversions need FPU support
         * which may require additional implementation */
        
        /* ============================================
         * Struct Field Access
         * ============================================ */
        
        case IR_LOAD_FIELD: {
            /* Load a field from a struct pointer in a local variable
             * Uses base + offset addressing */
            int base_local = instr->operand.field.local_idx;
            int16_t offset = instr->operand.field.offset;
            NativeType field_type = instr->operand.field.field_type;
            
            /* Get base pointer from local */
            MipsReg base_reg = mips_alloc_gpr(em);
            int base_src_reg = (int)nc->local_regs[base_local];
           
            if (base_src_reg >= 0) {
                mips_move(em, base_reg, (MipsReg)base_src_reg);
            } else {
                mips_lw(em, base_reg, nc->local_stack_offs[base_local], REG_FP);
            }
            
            MipsReg dst = PUSH_REG();
            
            /* Load based on field type */
            switch (field_type) {
                case NATIVE_TYPE_INT32:
                case NATIVE_TYPE_UINT32:
                case NATIVE_TYPE_BOOL:
                case NATIVE_TYPE_PTR:
                    mips_lw(em, dst, offset, base_reg);
                    break;
                case NATIVE_TYPE_FLOAT32:
                    /* Load float to GPR (will be interpreted/moved to FPU as needed) */
                    mips_lw(em, dst, offset, base_reg);
                    break;
                case NATIVE_TYPE_INT64:
                case NATIVE_TYPE_UINT64:
                    mips_ld(em, dst, offset, base_reg);
                    break;
                default:
                    mips_lw(em, dst, offset, base_reg);
                    break;
            }
            
            mips_free_gpr(em, base_reg);
            PUSH_TYPE(field_type);
            break;
        }
        
        case IR_STORE_FIELD: {
            /* Store a value to a struct field
             * Stack: [value] -> []
             * Base pointer comes from local variable (like IR_LOAD_FIELD) */
            int base_local = instr->operand.field.local_idx;
            int16_t offset = instr->operand.field.offset;
            NativeType field_type = instr->operand.field.field_type;
            
            /* Pop the value to store */
            MipsReg value_reg = POP_REG();
            NativeType value_type = POP_TYPE();

            /* Whole-number float literals (e.g. `5.0f`) reach the compiler
             * as the same compact integer-push opcode as a plain int
             * (IR_CONST_I32, not IR_CONST_F32 - see the file-level [reg]
             * comment on this class of bug). Reinterpreting int 5's raw bits
             * as float32 gives a denormal near zero, not 5.0 - only 0 is a
             * degenerate case where the int and float32 bit patterns happen
             * to coincide, which is why this went uncaught until a non-zero
             * literal was stored through a struct field. Convert the VALUE
             * (CVT.S.W), don't just move the bits. */
            if (field_type == NATIVE_TYPE_FLOAT32 &&
                (value_type == NATIVE_TYPE_INT32 || value_type == NATIVE_TYPE_UINT32 ||
                 value_type == NATIVE_TYPE_BOOL)) {
                MipsFpuReg fconv = mips_alloc_fpr(em);
                mips_mtc1(em, value_reg, fconv);
                mips_cvt_s_w(em, fconv, fconv);
                mips_mfc1(em, value_reg, fconv);
                mips_free_fpr(em, fconv);
            }

            /* Get base pointer - use local_regs directly if available to avoid conflicts */
            MipsReg base_reg;
            bool need_free = false;
            int base_src_reg = (int)nc->local_regs[base_local];
           
            if (base_src_reg >= 0) {
                /* Base is already in a register - use it directly! */
                base_reg = (MipsReg)base_src_reg;
            } else {
                /* Base is on stack - need to load it into temp register */
                base_reg = mips_alloc_gpr(em);
                need_free = true;
                mips_lw(em, base_reg, nc->local_stack_offs[base_local], REG_FP);
            }
            
            /* Store based on field type */
            switch (field_type) {
                case NATIVE_TYPE_INT32:
                case NATIVE_TYPE_UINT32:
                case NATIVE_TYPE_BOOL:
                case NATIVE_TYPE_PTR:
                case NATIVE_TYPE_FLOAT32:
                    mips_sw(em, value_reg, offset, base_reg);
                    break;
                case NATIVE_TYPE_INT64:
                case NATIVE_TYPE_UINT64:
                    mips_sd(em, value_reg, offset, base_reg);
                    break;
                default:
                    mips_sw(em, value_reg, offset, base_reg);
                    break;
            }
            
            if (need_free) {
                mips_free_gpr(em, base_reg);
            }
            break;
        }

        case IR_LOAD_FIELD_DYN: {
            /* Load a field through a struct-array element address that is a
             * genuine runtime value on the eval stack (pushed by
             * IR_ARRAY_ELEM_ADDR), unlike IR_LOAD_FIELD which always
             * re-derives its base pointer from a fixed local/argument slot.
             * Stack: [base_ptr] -> [value] */
            int16_t offset = instr->operand.field.offset;
            NativeType field_type = instr->operand.field.field_type;

            MipsReg base_reg = POP_REG();
            NativeType base_type = POP_TYPE(); (void)base_type;

            MipsReg dst = PUSH_REG();
            switch (field_type) {
                case NATIVE_TYPE_INT32:
                case NATIVE_TYPE_UINT32:
                case NATIVE_TYPE_BOOL:
                case NATIVE_TYPE_PTR:
                    mips_lw(em, dst, offset, base_reg);
                    break;
                case NATIVE_TYPE_FLOAT32:
                    mips_lw(em, dst, offset, base_reg);
                    break;
                case NATIVE_TYPE_INT64:
                case NATIVE_TYPE_UINT64:
                    mips_ld(em, dst, offset, base_reg);
                    break;
                default:
                    mips_lw(em, dst, offset, base_reg);
                    break;
            }
            PUSH_TYPE(field_type);
            break;
        }

        case IR_STORE_FIELD_DYN: {
            /* Store a field through a struct-array element address that is a
             * genuine runtime value on the eval stack, unlike IR_STORE_FIELD
             * which resolves its base from a fixed local/argument slot and
             * therefore only ever pops the value. Here the base pointer has
             * no such fallback - IR_ARRAY_ELEM_ADDR is the only place it was
             * ever computed - so both the value and the base MUST come off
             * the stack. Stack: [base_ptr, value] -> [] (value on top, same
             * order OP_put_field's bytecode pushes them in). */
            int16_t offset = instr->operand.field.offset;
            NativeType field_type = instr->operand.field.field_type;

            MipsReg value_reg = POP_REG();
            NativeType value_type = POP_TYPE();
            MipsReg base_reg = POP_REG();
            NativeType base_type = POP_TYPE(); (void)base_type;

            /* Same whole-number-float-literal fixup as IR_STORE_FIELD above -
             * see the comment there. */
            if (field_type == NATIVE_TYPE_FLOAT32 &&
                (value_type == NATIVE_TYPE_INT32 || value_type == NATIVE_TYPE_UINT32 ||
                 value_type == NATIVE_TYPE_BOOL)) {
                MipsFpuReg fconv = mips_alloc_fpr(em);
                mips_mtc1(em, value_reg, fconv);
                mips_cvt_s_w(em, fconv, fconv);
                mips_mfc1(em, value_reg, fconv);
                mips_free_fpr(em, fconv);
            }

            switch (field_type) {
                case NATIVE_TYPE_INT32:
                case NATIVE_TYPE_UINT32:
                case NATIVE_TYPE_BOOL:
                case NATIVE_TYPE_PTR:
                case NATIVE_TYPE_FLOAT32:
                    mips_sw(em, value_reg, offset, base_reg);
                    break;
                case NATIVE_TYPE_INT64:
                case NATIVE_TYPE_UINT64:
                    mips_sd(em, value_reg, offset, base_reg);
                    break;
                default:
                    mips_sw(em, value_reg, offset, base_reg);
                    break;
            }
            break;
        }

        /* Float arithmetic */
        case IR_ADD_F32:
        case IR_SUB_F32:
        case IR_MUL_F32:
        case IR_DIV_F32: {
            /* Use FPU registers */
            MipsFpuReg f1 = mips_alloc_fpr(em);
            MipsFpuReg f2 = mips_alloc_fpr(em);
            MipsFpuReg fd = mips_alloc_fpr(em);
            
            MipsReg r2 = POP_REG();  /* Second operand (top of stack) */
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();  /* First operand */
            NativeType t1 = POP_TYPE();
            
            /* Move registers to FPU */
            mips_mtc1(em, r1, f1);
            mips_mtc1(em, r2, f2);
            
            /* FIX: Check types and convert inputs to float if they are integers */
            if (t1 == NATIVE_TYPE_INT32 || t1 == NATIVE_TYPE_UINT32 || t1 == NATIVE_TYPE_BOOL) {
                mips_cvt_s_w(em, f1, f1);
            }
            if (t2 == NATIVE_TYPE_INT32 || t2 == NATIVE_TYPE_UINT32 || t2 == NATIVE_TYPE_BOOL) {
                mips_cvt_s_w(em, f2, f2);
            }
            
            /* Perform operation */
            switch (instr->op) {
                case IR_ADD_F32: mips_add_s(em, fd, f1, f2); break;
                case IR_SUB_F32: mips_sub_s(em, fd, f1, f2); break;
                case IR_MUL_F32: mips_mul_s(em, fd, f1, f2); break;
                case IR_DIV_F32: mips_div_s(em, fd, f1, f2); break;
                default: break;
            }
            
            /* Move result back to integer register */
            MipsReg dst = PUSH_REG();
            mips_mfc1(em, dst, fd);
            
            /* Free FPU registers */
            mips_free_fpr(em, f1);
            mips_free_fpr(em, f2);
            mips_free_fpr(em, fd);
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        /* Float sqrt (unary) */
        case IR_SQRT_F32: {
            MipsReg src = POP_REG();
            NativeType src_type = POP_TYPE();
            
            /* Move to FPU register $f0 */
            mips_mtc1(em, src, FPU_F0);
            
            /* Convert to float if int */
            if (src_type == NATIVE_TYPE_INT32 || src_type == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F0, FPU_F0);
            }
            
            /* SQRT.S - store result in $f0 (return register for floats) */
            mips_sqrt_s(em, FPU_F0, FPU_F0);
            
            /* For return: result stays in $f0 (float ABI)
             * But also copy to integer stack for compatibility with existing code */
            MipsReg dst = PUSH_REG();
            mips_mfc1(em, dst, FPU_F0);
            
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        /* Float negate (unary) - Native MIPS NEG.S.
         * Unary minus decodes as IR_NEG_I32 and type inference promotes it to
         * IR_NEG_F32 for float operands; without a case here that promotion
         * turned into a hard "Unsupported IR opcode: 26" at codegen time. */
        case IR_NEG_F32: {
            MipsReg src = POP_REG();
            NativeType src_type = POP_TYPE();

            mips_mtc1(em, src, FPU_F0);
            if (NATIVE_TYPE_IS_INT(src_type)) {
                mips_cvt_s_w(em, FPU_F0, FPU_F0);
            }
            mips_neg_s(em, FPU_F0, FPU_F0);

            MipsReg dst = PUSH_REG();
            mips_mfc1(em, dst, FPU_F0);

            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }

        /* Float abs (unary) - Native MIPS ABS.S */
        case IR_ABS_F32: {
            MipsReg src = POP_REG();
            NativeType src_type = POP_TYPE();
            
            /* Move to FPU register $f0 */
            mips_mtc1(em, src, FPU_F0);
            
            /* Convert to float if int */
            if (src_type == NATIVE_TYPE_INT32 || src_type == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F0, FPU_F0);
            }
            
            /* ABS.S - store result in $f0 */
            mips_abs_s(em, FPU_F0, FPU_F0);
            
            /* Copy to integer stack for compatibility */
            MipsReg dst = PUSH_REG();
            mips_mfc1(em, dst, FPU_F0);
            
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        /* Float min (binary) - R5900 has native MIN.S instruction */
        case IR_MIN_F32: {
            MipsReg r2 = POP_REG();  /* Second operand (on top of stack) */
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();  /* First operand */
            NativeType t1 = POP_TYPE();
            
            /* Move both to FPU registers */
            mips_mtc1(em, r1, FPU_F0);
            mips_mtc1(em, r2, FPU_F1);
            
            /* Convert to float if int */
            if (t1 == NATIVE_TYPE_INT32 || t1 == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F0, FPU_F0);
            }
            if (t2 == NATIVE_TYPE_INT32 || t2 == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F1, FPU_F1);
            }
            
            /* R5900 MIN.S: $f0 = min($f0, $f1) */
            mips_min_s(em, FPU_F0, FPU_F0, FPU_F1);
            
            /* Copy result to integer stack */
            MipsReg dst = PUSH_REG();
            mips_mfc1(em, dst, FPU_F0);
            
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        /* Float max (binary) - R5900 has native MAX.S instruction */
        case IR_MAX_F32: {
            MipsReg r2 = POP_REG();  /* Second operand (on top of stack) */
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();  /* First operand */
            NativeType t1 = POP_TYPE();
            
            /* Move both to FPU registers */
            mips_mtc1(em, r1, FPU_F0);
            mips_mtc1(em, r2, FPU_F1);
            
            /* Convert to float if int */
            if (t1 == NATIVE_TYPE_INT32 || t1 == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F0, FPU_F0);
            }
            if (t2 == NATIVE_TYPE_INT32 || t2 == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F1, FPU_F1);
            }
            
            /* R5900 MAX.S: $f0 = max($f0, $f1) */
            mips_max_s(em, FPU_F0, FPU_F0, FPU_F1);
            
            /* Copy result to integer stack */
            MipsReg dst = PUSH_REG();
            mips_mfc1(em, dst, FPU_F0);
            
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        /* Float clamp (3-args: value, min, max) - Using R5900 MIN.S and MAX.S */
        case IR_CLAMP_F32: {
            /* Stack has: [value, min, max] with max on top
             * We compute: max(min, min(value, max)) */
            MipsReg r_max = POP_REG();  /* max (top of stack) */
            NativeType t_max = POP_TYPE();
            MipsReg r_min = POP_REG();  /* min */
            NativeType t_min = POP_TYPE();
            MipsReg r_val = POP_REG();  /* value */
            NativeType t_val = POP_TYPE();
            
            /* Load all three values to FPU registers */
            mips_mtc1(em, r_val, FPU_F0);  /* value -> $f0 */
            mips_mtc1(em, r_min, FPU_F1);  /* min -> $f1 */
            mips_mtc1(em, r_max, FPU_F2);  /* max -> $f2 */
            
            /* Convert to float if int */
            if (t_val == NATIVE_TYPE_INT32 || t_val == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F0, FPU_F0);
            }
            if (t_min == NATIVE_TYPE_INT32 || t_min == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F1, FPU_F1);
            }
            if (t_max == NATIVE_TYPE_INT32 || t_max == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F2, FPU_F2);
            }
            
            /* Clamp: max(min, min(value, max))
             * Step 1: $f0 = min(value, max) = min($f0, $f2) */
            mips_min_s(em, FPU_F0, FPU_F0, FPU_F2);
            
            /* Step 2: $f0 = max(min, $f0) = max($f1, $f0) */
            mips_max_s(em, FPU_F0, FPU_F1, FPU_F0);
            
            /* Copy result back to integer stack */
            MipsReg dst = PUSH_REG();
            mips_mfc1(em, dst, FPU_F0);
            
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        /* Float sign (1-arg: x) - Returns -1, 0, or 1 based on sign */
        case IR_SIGN_F32: {
            MipsReg r_val = POP_REG();
            NativeType t_val = POP_TYPE();
            
            /* Load value to FPU */
            mips_mtc1(em, r_val, FPU_F0);
            
            /* Convert to float if int */
            if (t_val == NATIVE_TYPE_INT32 || t_val == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F0, FPU_F0);
            }
            
            /* Load 0.0 into $f1 for comparison */
            MipsReg r_zero = PUSH_REG();
            mips_lui(em, r_zero, 0);
            mips_mtc1(em, r_zero, FPU_F1);
            POP_REG();  /* Free temp register */
            
            /* Allocate destination register for result (used by all branches) */
            MipsReg dst = PUSH_REG();
            
            /* Create labels for branches */
            int zero_label = mips_label_create(em);
            int neg_label = mips_label_create(em);
            int end_label = mips_label_create(em);
            
            /* Compare with 0: C.EQ.S $f0, $f1 (check if value == 0) */
            mips_c_eq_s(em, FPU_F0, FPU_F1);
            mips_nop(em);  /* Pipeline hazard delay */
            
            /* Branch if equal to zero */
            mips_bc1t(em, zero_label);
            mips_nop(em);  /* Branch delay slot */
            
            /* Not zero - check if negative: C.LT.S $f0, $f1 (check if value < 0) */
            mips_c_lt_s(em, FPU_F0, FPU_F1);
            mips_nop(em);  /* Pipeline hazard delay */
            
            /* Branch if negative */
            mips_bc1t(em, neg_label);
            mips_nop(em);  /* Branch delay slot */
            
            /* Positive: return 1.0 */
            mips_lui(em, dst, 0x3F80);  /* Load 0x3F800000 = 1.0f into dst */
            mips_mtc1(em, dst, FPU_F0);  /* Move to FPU */
            mips_mfc1(em, dst, FPU_F0);  /* Move back to dst (ensures proper format) */
            mips_beq(em, REG_ZERO, REG_ZERO, end_label);  /* Jump to end */
            mips_nop(em);  /* Branch delay slot */
            
            /* Negative: return -1.0 */
            mips_label_bind(em, neg_label);
            mips_lui(em, dst, 0xBF80);  /* Load 0xBF800000 = -1.0f into dst */
            mips_mtc1(em, dst, FPU_F0);  /* Move to FPU */
            mips_mfc1(em, dst, FPU_F0);  /* Move back to dst */
            mips_beq(em, REG_ZERO, REG_ZERO, end_label);  /* Jump to end */
            mips_nop(em);  /* Branch delay slot */
            
            /* Zero: return 0.0 */
            mips_label_bind(em, zero_label);
            mips_lui(em, dst, 0);  /* Load 0x00000000 = 0.0f into dst */
            mips_mtc1(em, dst, FPU_F0);  /* Move to FPU */
            mips_mfc1(em, dst, FPU_F0);  /* Move back to dst */
            
            /* End label */
            mips_label_bind(em, end_label);
            
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        /* Float round to float32 - On PS2, all floats are already float32, so this is a noop */
        case IR_FROUND_F32: {
            /* Just pass through - value is already on stack as float32 */
            /* No code generation needed, value already in correct format */
            break;
        }
        
        /* Integer multiply (32-bit) - Uses MULT + MFLO */
        case IR_IMUL_I32: {
            /* Same shape as IR_MUL_I32 above: MFLO straight into the stack
             * register. This used to route the result through $v0 and then
             * copy it out, which is both pointless and fragile - $v0 is the
             * return register, not scratch - and Math.imul() came back 0. */
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE(); (void)t2;
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE(); (void)t1;
            MipsReg dst = PUSH_REG();
            mips_mult(em, r1, r2);
            mips_mflo(em, dst);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        /* Fused multiply-add: a*b+c - Uses MUL.S + ADD.S */
        case IR_FMA_F32: {
            /* Stack has: [a, b, c] with c on top */
            MipsReg r_c = POP_REG();
            NativeType t_c = POP_TYPE();
            MipsReg r_b = POP_REG();
            NativeType t_b = POP_TYPE();
            MipsReg r_a = POP_REG();
            NativeType t_a = POP_TYPE();
            
            /* Load all values to FPU */
            mips_mtc1(em, r_a, FPU_F0);  /* a -> $f0 */
            mips_mtc1(em, r_b, FPU_F1);  /* b -> $f1 */
            mips_mtc1(em, r_c, FPU_F2);  /* c -> $f2 */
            
            /* Convert to float if int */
            if (t_a == NATIVE_TYPE_INT32 || t_a == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F0, FPU_F0);
            }
            if (t_b == NATIVE_TYPE_INT32 || t_b == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F1, FPU_F1);
            }
            if (t_c == NATIVE_TYPE_INT32 || t_c == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F2, FPU_F2);
            }
            
            /* FMA:  a*b+c
             * MUL.S $f0, $f0, $f1  ->  $f0 = a * b
             * ADD.S $f0, $f0, $f2  ->  $f0 = (a*b) + c */
            mips_mul_s(em, FPU_F0, FPU_F0, FPU_F1);
            mips_add_s(em, FPU_F0, FPU_F0, FPU_F2);
            
            /* Move result back to integer stack */
            MipsReg dst = PUSH_REG();
            mips_mfc1(em, dst, FPU_F0);
            
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        /* Linear interpolation: lerp(a, b, t) = a + (b - a) * t */
        case IR_LERP_F32: {
            /* Stack has: [a, b, t] with t on top */
            MipsReg r_t = POP_REG();
            NativeType t_t = POP_TYPE();
            MipsReg r_b = POP_REG();
            NativeType t_b = POP_TYPE();
            MipsReg r_a = POP_REG();
            NativeType t_a = POP_TYPE();
            
            /* Load all values to FPU */
            mips_mtc1(em, r_a, FPU_F0);  /* a -> $f0 */
            mips_mtc1(em, r_b, FPU_F1);  /* b -> $f1 */
            mips_mtc1(em, r_t, FPU_F2);  /* t -> $f2 */
            
            /* Convert to float if int */
            if (t_a == NATIVE_TYPE_INT32 || t_a == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F0, FPU_F0);
            }
            if (t_b == NATIVE_TYPE_INT32 || t_b == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F1, FPU_F1);
            }
            if (t_t == NATIVE_TYPE_INT32 || t_t == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F2, FPU_F2);
            }
            
            /* lerp(a, b, t) = a + (b - a) * t
             * $f3 = b - a
             * $f3 = $f3 * t
             * $f0 = a + $f3 */
            mips_sub_s(em, FPU_F3, FPU_F1, FPU_F0);  /* $f3 = b - a */
            mips_mul_s(em, FPU_F3, FPU_F3, FPU_F2);  /* $f3 = (b-a) * t */
            mips_add_s(em, FPU_F0, FPU_F0, FPU_F3);  /* $f0 = a + (b-a)*t */
            
            /* Move result back to integer stack */
            MipsReg dst = PUSH_REG();
            mips_mfc1(em, dst, FPU_F0);
            
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        /* Saturate: clamp value to [0, 1] */
        case IR_SATURATE_F32: {
            MipsReg r_val = POP_REG();
            NativeType t_val = POP_TYPE();
            
            /* Load value to FPU */
            mips_mtc1(em, r_val, FPU_F0);
            
            /* Convert to float if int */
            if (t_val == NATIVE_TYPE_INT32 || t_val == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F0, FPU_F0);
            }
            
            /* Load 0.0 and 1.0 constants */
            MipsReg tmp = PUSH_REG();
            mips_lui(em, tmp, 0);        /* 0.0f = 0x00000000 */
            mips_mtc1(em, tmp, FPU_F1);  /* $f1 = 0.0 */
            mips_lui(em, tmp, 0x3F80);   /* 1.0f = 0x3F800000 */
            mips_mtc1(em, tmp, FPU_F2);  /* $f2 = 1.0 */
            POP_REG();  /* Free tmp */
            
            /* Clamp: max(0, min(value, 1)) using R5900 MIN.S/MAX.S */
            mips_min_s(em, FPU_F0, FPU_F0, FPU_F2);  /* $f0 = min(val, 1.0) */
            mips_max_s(em, FPU_F0, FPU_F0, FPU_F1);  /* $f0 = max($f0, 0.0) */
            
            /* Move result back */
            MipsReg dst = PUSH_REG();
            mips_mfc1(em, dst, FPU_F0);
            
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        /* Step function: step(edge, x) = x >= edge ? 1.0 : 0.0 */
        case IR_STEP_F32: {
            /* Stack has: [edge, x] with x on top */
            MipsReg r_x = POP_REG();
            NativeType t_x = POP_TYPE();
            MipsReg r_edge = POP_REG();
            NativeType t_edge = POP_TYPE();
            
            /* Load to FPU */
            mips_mtc1(em, r_edge, FPU_F0);  /* edge -> $f0 */
            mips_mtc1(em, r_x, FPU_F1);     /* x -> $f1 */
            
            /* Convert to float if int */
            if (t_edge == NATIVE_TYPE_INT32 || t_edge == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F0, FPU_F0);
            }
            if (t_x == NATIVE_TYPE_INT32 || t_x == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F1, FPU_F1);
            }
            
            /* Load 0.0 and 1.0 constants to FPU */
            MipsReg tmp = PUSH_REG();
            mips_lui(em, tmp, 0);        /* 0.0f */
            mips_mtc1(em, tmp, FPU_F2);  /* $f2 = 0.0 */
            mips_lui(em, tmp, 0x3F80);   /* 1.0f */
            mips_mtc1(em, tmp, FPU_F3);  /* $f3 = 1.0 */
            POP_REG();
            
            /* Compare: x < edge using C.LT.S */
            int step_one_label = mips_label_create(em);
            int end_label = mips_label_create(em);
            
            mips_c_lt_s(em, FPU_F1, FPU_F0);  /* x < edge */
            mips_nop(em);
            mips_bc1f(em, step_one_label);  /* If NOT (x < edge), i.e. x >= edge, return 1 */
            mips_nop(em);
            
            /* x < edge: result = 0.0 */
            mips_mov_s(em, FPU_F0, FPU_F2);  /* $f0 = 0.0 */
            mips_beq(em, REG_ZERO, REG_ZERO, end_label);
            mips_nop(em);
            
            /* x >= edge: result = 1.0 */
            mips_label_bind(em, step_one_label);
            mips_mov_s(em, FPU_F0, FPU_F3);  /* $f0 = 1.0 */
            
            mips_label_bind(em, end_label);
            
            /* Move result from FPU to integer stack */
            MipsReg dst = PUSH_REG();
            mips_mfc1(em, dst, FPU_F0);
            
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        /* Reciprocal sqrt: rsqrt(x) = 1.0 / sqrt(x) */
        case IR_RSQRT_F32: {
            MipsReg r_val = POP_REG();
            NativeType t_val = POP_TYPE();
            
            /* Load value to FPU */
            mips_mtc1(em, r_val, FPU_F0);
            
            /* Convert to float if int */
            if (t_val == NATIVE_TYPE_INT32 || t_val == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F0, FPU_F0);
            }
            
            /* Load 1.0 constant */
            MipsReg tmp = PUSH_REG();
            mips_lui(em, tmp, 0x3F80);   /* 1.0f */
            mips_mtc1(em, tmp, FPU_F1);
            POP_REG();
            
            /* SQRT.S $f0, $f0 -> DIV.S $f0, $f1, $f0 */
            mips_sqrt_s(em, FPU_F0, FPU_F0);
            mips_div_s(em, FPU_F0, FPU_F1, FPU_F0);
            
            /* Move result back */
            MipsReg dst = PUSH_REG();
            mips_mfc1(em, dst, FPU_F0);
            
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        /* Smoothstep: smoothstep(e0, e1, x) = Hermite interpolation
         * t = clamp((x - e0) / (e1 - e0), 0, 1)
         * return t * t * (3 - 2 * t) */
        case IR_SMOOTHSTEP_F32: {
            /* Stack has: [e0, e1, x] with x on top */
            MipsReg r_x = POP_REG();
            NativeType t_x = POP_TYPE();
            MipsReg r_e1 = POP_REG();
            NativeType t_e1 = POP_TYPE();
            MipsReg r_e0 = POP_REG();
            NativeType t_e0 = POP_TYPE();
            
            /* Load all values to FPU */
            mips_mtc1(em, r_e0, FPU_F0);  /* e0 -> $f0 */
            mips_mtc1(em, r_e1, FPU_F1);  /* e1 -> $f1 */
            mips_mtc1(em, r_x, FPU_F2);   /* x -> $f2 */
            
            /* Convert to float if int */
            if (t_e0 == NATIVE_TYPE_INT32 || t_e0 == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F0, FPU_F0);
            }
            if (t_e1 == NATIVE_TYPE_INT32 || t_e1 == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F1, FPU_F1);
            }
            if (t_x == NATIVE_TYPE_INT32 || t_x == NATIVE_TYPE_UINT32) {
                mips_cvt_s_w(em, FPU_F2, FPU_F2);
            }
            
            /* Load constants: 0.0, 1.0, 2.0, 3.0 */
            MipsReg tmp = PUSH_REG();
            mips_lui(em, tmp, 0);        /* 0.0f */
            mips_mtc1(em, tmp, FPU_F4);  /* $f4 = 0.0 */
            mips_lui(em, tmp, 0x3F80);   /* 1.0f */
            mips_mtc1(em, tmp, FPU_F5);  /* $f5 = 1.0 */
            mips_lui(em, tmp, 0x4000);   /* 2.0f */
            mips_mtc1(em, tmp, FPU_F6);  /* $f6 = 2.0 */
            mips_lui(em, tmp, 0x4040);   /* 3.0f */
            mips_mtc1(em, tmp, FPU_F7);  /* $f7 = 3.0 */
            POP_REG();
            
            /* Step 1: t = (x - e0) / (e1 - e0)
             * $f3 = x - e0
             * $f1 = e1 - e0
             * $f3 = $f3 / $f1 */
            mips_sub_s(em, FPU_F3, FPU_F2, FPU_F0);  /* $f3 = x - e0 */
            mips_sub_s(em, FPU_F1, FPU_F1, FPU_F0);  /* $f1 = e1 - e0 */
            mips_div_s(em, FPU_F3, FPU_F3, FPU_F1);  /* $f3 = (x-e0)/(e1-e0) = t_raw */
            
            /* Step 2: t = clamp(t_raw, 0, 1) */
            mips_max_s(em, FPU_F3, FPU_F3, FPU_F4);  /* t = max(t_raw, 0) */
            mips_min_s(em, FPU_F3, FPU_F3, FPU_F5);  /* t = min(t, 1) */
            
            /* Step 3: result = t * t * (3 - 2*t)
             * $f0 = t * t
             * $f1 = 2 * t
             * $f1 = 3 - $f1
             * $f0 = $f0 * $f1 */
            mips_mul_s(em, FPU_F0, FPU_F3, FPU_F3);  /* $f0 = t * t */
            mips_mul_s(em, FPU_F1, FPU_F6, FPU_F3);  /* $f1 = 2 * t */
            mips_sub_s(em, FPU_F1, FPU_F7, FPU_F1);  /* $f1 = 3 - 2*t */
            mips_mul_s(em, FPU_F0, FPU_F0, FPU_F1);  /* $f0 = t*t * (3 - 2*t) */
            
            /* Move result back */
            MipsReg dst = PUSH_REG();
            mips_mfc1(em, dst, FPU_F0);
            
            PUSH_TYPE(NATIVE_TYPE_FLOAT32);
            break;
        }
        
        /* Generic C function call - calls any registered native function with 1-8 args */
        case IR_CALL_C_FUNC: {
            void *func_ptr = instr->operand.call.func_ptr;
            uint8_t arg_count = instr->operand.call.arg_count;
            
            if (!func_ptr || arg_count == 0 || arg_count > 8) {
                /* Invalid call - push dummy value */
                MipsReg dst = PUSH_REG();
                mips_addu(em, dst, REG_ZERO, REG_ZERO);
                PUSH_TYPE(NATIVE_TYPE_FLOAT32);
                break;
            }
            
            /* MIPS N32 ABI: Float args in consecutive regs $f12-$f19 (up to 8) */
            static const MipsFpuReg fpu_arg_regs[] = { 
                FPU_F12, FPU_F13, FPU_F14, FPU_F15, 
                FPU_F16, FPU_F17, FPU_F18, FPU_F19 
            };
            
            /* Pop arguments from stack - last arg is on top of stack
             * For call(a, b): stack has [a, b] with b on top
             * Pop b first (into arg_regs[1]), then a (into arg_regs[0]) */
            MipsReg arg_regs[8];
            NativeType arg_types[8];
            
            for (int i = arg_count - 1; i >= 0; i--) {
                arg_regs[i] = POP_REG();
                arg_types[i] = POP_TYPE();
            }
            
            /* Move arguments to FPU registers and convert if needed */
            for (int i = 0; i < arg_count; i++) {
                mips_mtc1(em, arg_regs[i], fpu_arg_regs[i]);
                
                /* Convert int to float if needed */
                if (arg_types[i] == NATIVE_TYPE_INT32 || arg_types[i] == NATIVE_TYPE_UINT32) {
                    mips_cvt_s_w(em, fpu_arg_regs[i], fpu_arg_regs[i]);
                }
            }
            
            /* Load function address into $t9 and call */
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)func_ptr >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)func_ptr & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);  /* Delay slot */
            
            /* Normal JS calls retain a result slot even for void callees. */
            MipsReg dst = PUSH_REG();
            NativeType ret_type = instr->operand.call.ret_type;
            if (ret_type == NATIVE_TYPE_VOID) mips_li(em, dst, 0);
            else if (ret_type == NATIVE_TYPE_FLOAT32) mips_mfc1(em, dst, FPU_F0);
            else mips_move(em, dst, REG_V0);
            PUSH_TYPE(ret_type == NATIVE_TYPE_VOID ? NATIVE_TYPE_INT32 : ret_type);
            break;
        }
        
        /* Bitwise operations consume numeric values, not float bit patterns.
         * Keep the type stack in sync with the GPR stack as for arithmetic. */
        case IR_AND: case IR_OR: case IR_XOR:
        case IR_SHL: case IR_SHR: case IR_SAR: {
            MipsReg r2 = POP_REG(); NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG(); NativeType t1 = POP_TYPE();
            MipsReg operands[2] = {r1, r2};
            NativeType types[2] = {t1, t2};
            for (int i = 0; i < 2; i++) {
                if (types[i] == NATIVE_TYPE_FLOAT32) {
                    MipsFpuReg f = mips_alloc_fpr(em);
                    mips_mtc1(em, operands[i], f);
                    mips_cvt_w_s(em, f, f);
                    mips_mfc1(em, operands[i], f);
                    mips_free_fpr(em, f);
                }
            }
            MipsReg dst = PUSH_REG();
            switch (instr->op) {
                case IR_AND: mips_and(em, dst, r1, r2); break;
                case IR_OR: mips_or(em, dst, r1, r2); break;
                case IR_XOR: mips_xor(em, dst, r1, r2); break;
                case IR_SHL: mips_sllv(em, dst, r1, r2); break;
                case IR_SHR: mips_srlv(em, dst, r1, r2); break;
                default: mips_srav(em, dst, r1, r2); break;
            }
            PUSH_TYPE(instr->op == IR_SHR ? NATIVE_TYPE_UINT32 : NATIVE_TYPE_INT32);
            break;
        }
        case IR_NOT: {
            MipsReg src = POP_REG(); NativeType type = POP_TYPE();
            if (type == NATIVE_TYPE_FLOAT32) {
                MipsFpuReg f = mips_alloc_fpr(em);
                mips_mtc1(em, src, f);
                mips_cvt_w_s(em, f, f);
                mips_mfc1(em, src, f);
                mips_free_fpr(em, f);
            }
            MipsReg dst = PUSH_REG();
            mips_nor(em, dst, src, REG_ZERO);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }

        /* Comparisons */
        case IR_LT_I32: {
            MipsReg r2 = POP_REG();  /* Top of stack (second operand) */
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();  /* Second from top (first operand) */
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            /* In QuickJS, for expression "a < b", the stack has [a, b] where b is on top.
             * When we POP: r2 = b (top), r1 = a (below).
             * For (a < b), we need SLT(dst, a, b) = SLT(dst, r1, r2).
             * mips_slt(rd, rs, rt) generates SLT rd, rs, rt = (rs < rt).
             * So mips_slt(dst, r1, r2) generates SLT dst, r1, r2 = (r1 < r2) = (a < b) ✓ */
            mips_slt(em, dst, r1, r2);
            PUSH_TYPE(NATIVE_TYPE_INT32);  /* Comparison result is boolean (int32) */
            break;
        }
        
        case IR_LT_U32: {
            MipsReg r2 = POP_REG();  /* Top of stack (second operand) */
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();  /* Second from top (first operand) */
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            /* Stack has [r1, r2] where r2 is top. For r1 < r2, use SLTU(dst, r1, r2) */
            mips_sltu(em, dst, r1, r2);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        case IR_GT_I32: {
            MipsReg r2 = POP_REG();  /* Top of stack (second operand) */
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();  /* Second from top (first operand) */
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            /* Stack has [r1, r2] where r2 is top. For r1 > r2, use SLT(dst, r2, r1) = (r2 < r1) */
            mips_slt(em, dst, r2, r1);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        case IR_LE_I32: {
            MipsReg r2 = POP_REG();  /* Top of stack (second operand) */
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();  /* Second from top (first operand) */
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            /* Stack has [r1, r2] where r2 is top. For r1 <= r2, use !(r2 < r1) = !SLT(r2, r1) */
            /* Compute (r2 < r1) in $at, then invert it */
            mips_slt(em, REG_AT, r2, r1);
            mips_xori(em, dst, REG_AT, 1);  /* Invert: dst = !(r2 < r1) = (r1 <= r2) */
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        case IR_GE_I32: {
            MipsReg r2 = POP_REG();  /* Top of stack (second operand) */
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();  /* Second from top (first operand) */
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            /* Stack has [r1, r2] where r2 is top. For r1 >= r2, use !(r1 < r2) = !SLT(r1, r2) */
            /* Compute (r1 < r2) in $at, then invert it */
            mips_slt(em, REG_AT, r1, r2);
            mips_xori(em, dst, REG_AT, 1);  /* Invert: dst = !(r1 < r2) = (r1 >= r2) */
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        case IR_EQ_I32: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            /* r1 == r2 -> (r1 ^ r2) == 0 -> SLTU(1, r1^r2) */
            mips_xor(em, REG_AT, r1, r2);
            mips_sltiu(em, dst, REG_AT, 1);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        case IR_NE_I32: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg dst = PUSH_REG();
            /* r1 != r2 -> (r1 ^ r2) != 0 -> SLTU(0, r1^r2) */
            mips_xor(em, REG_AT, r1, r2);
            mips_sltu(em, dst, REG_ZERO, REG_AT);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }

        case IR_LE_U32: {
            MipsReg r2 = POP_REG();
            POP_TYPE();
            MipsReg r1 = POP_REG();
            POP_TYPE();
            MipsReg dst = PUSH_REG();
            mips_sltu(em, REG_AT, r2, r1);
            mips_xori(em, dst, REG_AT, 1);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }

        case IR_GT_U32: {
            MipsReg r2 = POP_REG();
            POP_TYPE();
            MipsReg r1 = POP_REG();
            POP_TYPE();
            MipsReg dst = PUSH_REG();
            mips_sltu(em, dst, r2, r1);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }

        case IR_GE_U32: {
            MipsReg r2 = POP_REG();
            POP_TYPE();
            MipsReg r1 = POP_REG();
            POP_TYPE();
            MipsReg dst = PUSH_REG();
            mips_sltu(em, REG_AT, r1, r2);
            mips_xori(em, dst, REG_AT, 1);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }

        case IR_EQ_F32:
        case IR_NE_F32:
        case IR_LT_F32:
        case IR_LE_F32:
        case IR_GT_F32:
        case IR_GE_F32: {
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsFpuReg f0 = mips_alloc_fpr(em);
            MipsFpuReg f1 = mips_alloc_fpr(em);
            mips_mtc1(em, r1, f0);
            mips_mtc1(em, r2, f1);
            /* Same literal-operand fixup as ADD_F32/etc: this op was
             * promoted to the float variant because at least one side is
             * float, but the OTHER side may still be a raw IR_CONST_I32
             * (e.g. `v > -28` - -28 reaches here as an integer regardless
             * of intent). Without this, its bits get compared as float
             * garbage instead of the numeric value. */
            if (NATIVE_TYPE_IS_INT(t1)) mips_cvt_s_w(em, f0, f0);
            if (NATIVE_TYPE_IS_INT(t2)) mips_cvt_s_w(em, f1, f1);
            MipsReg dst = PUSH_REG();
            int true_label = mips_label_create(em);
            int end_label = mips_label_create(em);
            if (instr->op == IR_EQ_F32) {
                mips_c_eq_s(em, f0, f1);
            } else if (instr->op == IR_LT_F32 || instr->op == IR_GT_F32) {
                if (instr->op == IR_LT_F32)
                    mips_c_lt_s(em, f0, f1);
                else
                    mips_c_lt_s(em, f1, f0);
            } else if (instr->op == IR_LE_F32 || instr->op == IR_GE_F32) {
                if (instr->op == IR_LE_F32)
                    mips_c_le_s(em, f0, f1);
                else
                    mips_c_le_s(em, f1, f0);
            } else {
                mips_c_eq_s(em, f0, f1);
            }
            mips_nop(em);
            mips_bc1t(em, true_label);
            mips_nop(em);
            mips_li(em, dst, (instr->op == IR_NE_F32) ? 1 : 0);
            mips_beq(em, REG_ZERO, REG_ZERO, end_label);
            mips_nop(em);
            mips_label_bind(em, true_label);
            mips_li(em, dst, (instr->op == IR_NE_F32) ? 0 : 1);
            mips_label_bind(em, end_label);
            mips_free_fpr(em, f1);
            mips_free_fpr(em, f0);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }

        case IR_EQ_I64:
        case IR_NE_I64:
        case IR_LT_I64:
        case IR_LE_I64:
        case IR_GT_I64:
        case IR_GE_I64:
        case IR_LT_U64:
        case IR_LE_U64:
        case IR_GT_U64:
        case IR_GE_U64: {
            void *cmp_fn = NULL;
            switch (instr->op) {
                case IR_EQ_I64: cmp_fn = (void *)__deq_i64; break;
                case IR_NE_I64: cmp_fn = (void *)__dne_i64; break;
                case IR_LT_I64: cmp_fn = (void *)__dlt_i64; break;
                case IR_LE_I64: cmp_fn = (void *)__dle_i64; break;
                case IR_GT_I64: cmp_fn = (void *)__dgt_i64; break;
                case IR_GE_I64: cmp_fn = (void *)__dge_i64; break;
                case IR_LT_U64: cmp_fn = (void *)__dlt_u64; break;
                case IR_LE_U64: cmp_fn = (void *)__dle_u64; break;
                case IR_GT_U64: cmp_fn = (void *)__dgt_u64; break;
                case IR_GE_U64: cmp_fn = (void *)__dge_u64; break;
                default: break;
            }
            MipsReg r2 = POP_REG();
            POP_TYPE();
            MipsReg r1 = POP_REG();
            POP_TYPE();
            mips_addiu(em, REG_SP, REG_SP, -8);
            mips_sw(em, REG_RA, 4, REG_SP);
            mips_move(em, REG_A0, r1);
            mips_move(em, REG_A1, r2);
            mips_lui(em, REG_T9, ((uint32_t)(uintptr_t)cmp_fn >> 16) & 0xFFFF);
            mips_ori(em, REG_T9, REG_T9, (uint32_t)(uintptr_t)cmp_fn & 0xFFFF);
            mips_jalr(em, REG_RA, REG_T9);
            mips_nop(em);
            mips_lw(em, REG_RA, 4, REG_SP);
            mips_addiu(em, REG_SP, REG_SP, 8);
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, REG_V0);
            PUSH_TYPE(NATIVE_TYPE_INT32);
            break;
        }
        
        /* Control flow */
        case IR_JUMP: {
            int target_block = instr->operand.label_id;
            /* Translate block index to MIPS label ID */
            int label = (block_to_label && target_block >= 0) ? block_to_label[target_block] : target_block;
            mips_j(em, label);
            mips_nop(em);  /* Delay slot */
            break;
        }
        
        case IR_JUMP_IF_TRUE: {
            MipsReg cond = POP_REG();
            NativeType cond_type = POP_TYPE();  /* CRITICAL: Also pop the type! */
            int target_block = instr->operand.label_id;
            /* Translate block index to MIPS label ID */
            int label = (block_to_label && target_block >= 0) ? block_to_label[target_block] : target_block;
            mips_bne(em, cond, REG_ZERO, label);
            mips_nop(em);  /* Delay slot */
            break;
        }
        
        case IR_JUMP_IF_FALSE: {
            MipsReg cond = POP_REG();
            NativeType cond_type = POP_TYPE(); (void)cond_type;  /* CRITICAL: Also pop the type! */
            int target_block = instr->operand.label_id;
            /* Translate block index to MIPS label ID */
            int label = (block_to_label && target_block >= 0) ? block_to_label[target_block] : target_block;
            mips_beq(em, cond, REG_ZERO, label);
            mips_nop(em);  /* Delay slot */
            break;
        }
        
        case IR_LABEL:
            mips_label_bind(em, instr->operand.label_id);
            break;
        
        case IR_CALL: {
            /* Native function call
             * Stack layout: [args..., func_ptr] -> [result]
             * MIPS calling convention: $a0-$a3 for first 4 args, rest on stack
             * at [$sp + i*4] for arg index i (see allocate_locals's
             * outgoing_arg_bytes reservation and the stack-passed-arg offset
             * fixup in emit_function_prologue - both must agree with this).
             */
            int nargs = instr->operand.call.arg_count;
            void *func_ptr = instr->operand.call.func_ptr;
            if (nargs > 8) nargs = 8;  /* eval stack depth caps args at 8 */

            /* QuickJS stack layout for a call is [func_ptr, arg0, arg1, ...] with
             * the last argument on top. Pop ALL arguments (not just the first 4 -
             * they're LIFO, so popping fewer than nargs would grab the LAST
             * args instead of the first, and leave the rest stranded on the
             * eval stack). */
            const MipsReg arg_regs[] = { REG_A0, REG_A1, REG_A2, REG_A3 };
            MipsReg arg_vals[8];
            NativeType arg_src_types[8];

            for (int i = nargs - 1; i >= 0; i--) {
                arg_vals[i] = POP_REG();
                arg_src_types[i] = POP_TYPE();
            }

            /* Literal args (e.g. `-1.0f`) reach here as IR_CONST_I32 - see
             * IR_RETURN - so convert any argument whose tracked type doesn't
             * match what the resolved callee actually expects (known only
             * when nc_apply_pending_call_sig managed to stamp arg_types[];
             * NATIVE_TYPE_UNKNOWN there means we don't know, so don't touch
             * it - safer to pass raw bits than guess wrong). */
            for (int i = 0; i < nargs; i++) {
                NativeType expected = instr->operand.call.arg_types[i];
                if (expected == NATIVE_TYPE_FLOAT32 && NATIVE_TYPE_IS_INT(arg_src_types[i])) {
                    MipsFpuReg fpu = mips_alloc_fpr(em);
                    mips_mtc1(em, arg_vals[i], fpu);
                    mips_cvt_s_w(em, fpu, fpu);
                    mips_mfc1(em, arg_vals[i], fpu);
                    mips_free_fpr(em, fpu);
                } else if (NATIVE_TYPE_IS_INT(expected) && arg_src_types[i] == NATIVE_TYPE_FLOAT32) {
                    MipsFpuReg fpu = mips_alloc_fpr(em);
                    mips_mtc1(em, arg_vals[i], fpu);
                    mips_cvt_w_s(em, fpu, fpu);
                    mips_mfc1(em, arg_vals[i], fpu);
                    mips_free_fpr(em, fpu);
                }
            }

            MipsReg target;
            if (func_ptr != NULL) {
                /* Direct call to known function - load into $t9 (temp) */
                target = REG_T9;
                mips_la(em, target, func_ptr);
            } else {
                /* Indirect call - function pointer is on stack, below the args */
                target = POP_REG();
                POP_TYPE();
            }

            int reg_args = (nargs < 4) ? nargs : 4;

            /* Move to arg registers (sources are $s regs, dests are $a regs, no hazard) */
            for (int i = 0; i < reg_args; i++) {
                if (arg_vals[i] != arg_regs[i]) {
                    mips_move(em, arg_regs[i], arg_vals[i]);
                }
            }

            /* Stage args 4+ on the stack, in our own frame's reserved
             * outgoing-arg area (offset 0 from $sp/$fp - they're equal for
             * our whole function body). The callee reads them at
             * $fp_callee + frame_size_callee + i*4, which resolves to the
             * same absolute address since $fp_callee == our $sp here. */
            for (int i = 4; i < nargs; i++) {
                mips_sw(em, arg_vals[i], i * 4, REG_SP);
            }

            /* Mark function as non-leaf since we're making a call */
            nc->is_leaf = false;

            /* Call the function (JALR $ra, target) */
            mips_jalr(em, REG_RA, target);
            mips_nop(em);  /* Delay slot */
            
            /* Push result - floats in $f0, ints in $v0 */
            MipsReg result = PUSH_REG();
            if (instr->operand.call.ret_type == NATIVE_TYPE_VOID) {
                mips_li(em, result, 0);
            } else if (instr->type == NATIVE_TYPE_FLOAT32) {
                /* Float return is in $f0 - move to GPR */
                mips_mfc1(em, result, FPU_F0);
            } else {
                /* Int return is in $v0 */
                if (result != REG_V0) {
                    mips_move(em, result, REG_V0);
                }
            }
            PUSH_TYPE(instr->type);
            break;
        }
        
        case IR_TAIL_CALL: {
            /* Tail call optimization - reuse current stack frame
             * Instead of: JALR + return, we do: restore frame + JR target
             */
            int nargs = instr->operand.call.arg_count;
            void *func_ptr = instr->operand.call.func_ptr;
            if (nargs > 8) nargs = 8;  /* eval stack depth caps args at 8 */

            /* QuickJS stack layout is [func_ptr, arg0, ...]; pop ALL args first
             * (LIFO - popping fewer than nargs would grab the LAST args instead
             * of the first, same hazard as IR_CALL), then the function pointer
             * below them. */
            const MipsReg arg_regs[] = { REG_A0, REG_A1, REG_A2, REG_A3 };
            MipsReg arg_vals[8];
            NativeType arg_src_types[8];

            for (int i = nargs - 1; i >= 0; i--) {
                arg_vals[i] = POP_REG();
                arg_src_types[i] = POP_TYPE();
            }

            /* Same literal-argument fixup as IR_CALL - see there for why. */
            for (int i = 0; i < nargs; i++) {
                NativeType expected = instr->operand.call.arg_types[i];
                if (expected == NATIVE_TYPE_FLOAT32 && NATIVE_TYPE_IS_INT(arg_src_types[i])) {
                    MipsFpuReg fpu = mips_alloc_fpr(em);
                    mips_mtc1(em, arg_vals[i], fpu);
                    mips_cvt_s_w(em, fpu, fpu);
                    mips_mfc1(em, arg_vals[i], fpu);
                    mips_free_fpr(em, fpu);
                } else if (NATIVE_TYPE_IS_INT(expected) && arg_src_types[i] == NATIVE_TYPE_FLOAT32) {
                    MipsFpuReg fpu = mips_alloc_fpr(em);
                    mips_mtc1(em, arg_vals[i], fpu);
                    mips_cvt_w_s(em, fpu, fpu);
                    mips_mfc1(em, arg_vals[i], fpu);
                    mips_free_fpr(em, fpu);
                }
            }

            /* Resolve target into $t9. $t9 is a temp not restored by the epilogue,
             * so it survives the $s0-$s7 restore below (an $s reg would be clobbered). */
            if (func_ptr != NULL) {
                mips_la(em, REG_T9, func_ptr);
            } else {
                MipsReg src = POP_REG();
                POP_TYPE();
                if (src != REG_T9) {
                    mips_move(em, REG_T9, src);
                }
            }
            MipsReg target = REG_T9;

            int reg_args = (nargs < 4) ? nargs : 4;
            for (int i = 0; i < reg_args; i++) {
                if (arg_vals[i] != arg_regs[i]) {
                    mips_move(em, arg_regs[i], arg_vals[i]);
                }
            }

            int frame_size = nc->emitter.stack_offset;

            /* Stage args 4+ at the callee's expected location AFTER this
             * frame is torn down: $sp_after_restore + i*4, i.e.
             * $sp_current + frame_size + i*4 (write now, using an immediate
             * that already bakes in frame_size, since we haven't restored
             * $sp yet). This reuses memory that was either our own incoming
             * stack args (already consumed into registers above) or space
             * our caller staged for calling us - safe because every call
             * reachable from a JS entry point has at least
             * MAX_NATIVE_ARGS(8)*4 bytes reserved there (native_func_call
             * always calls through a fixed 8-argument C function pointer).
             * Must run before the $s0-$s7 restore below, which would
             * otherwise clobber the arg_vals registers being read here. */
            for (int i = 4; i < nargs; i++) {
                mips_sw(em, arg_vals[i], frame_size + i * 4, REG_SP);
            }

            /* Restore $s registers if any */
            const MipsReg saved_regs[] = { REG_S0, REG_S1, REG_S2, REG_S3, REG_S4, REG_S5, REG_S6, REG_S7 };
            if (nc->used_saved_regs) {
                int offset = frame_size - 12;
                for (int i = 0; i < 8; i++) {
                    if (nc->used_saved_regs & (1 << i)) {
                        mips_lw(em, saved_regs[i], offset, REG_SP);
                        offset -= 4;
                    }
                }
            }
            
            /* Restore $ra (the caller's return address) so the tail-called
             * function returns directly to our caller. The prologue saved it at
             * frame_size - 4 for non-leaf functions (any function with a call is
             * non-leaf). Without this, the callee would return into the middle of
             * this function and re-run the epilogue with a corrupted stack. */
            if (!nc->is_leaf) {
                mips_lw(em, REG_RA, frame_size - 4, REG_SP);
            }
            
            /* Restore $fp if needed */
            if (nc->needs_fp) {
                mips_lw(em, REG_FP, frame_size - 8, REG_SP);
            }
            
            /* Restore stack pointer */
            if (!nc->is_leaf || nc->needs_fp) {
                mips_addiu(em, REG_SP, REG_SP, frame_size);
            }
            
            /* Jump to target (NOT JALR - don't save return address) */
            mips_jr(em, target);
            mips_nop(em);  /* Delay slot */
            break;
        }
        
        case IR_RETURN: {
            MipsReg val = POP_REG();
            NativeType val_type = POP_TYPE();
            
            /* MIPS Calling Convention:
             * - Integer returns go in $v0
             * - Float returns go in $f0
             */
            NativeType ret_type = nc->ir_func.sig.return_type;
            
            if (ret_type == NATIVE_TYPE_FLOAT32) {
                /* Float return - value must go to $f0 */
                mips_mtc1(em, val, FPU_F0);
                /* Whole-number float literals (e.g. `6.0f`, `-1.0f`) reach here
                 * as IR_CONST_I32, not IR_CONST_F32: QuickJS's bytecode uses
                 * the same compact integer-push opcode for any integral
                 * numeric literal regardless of the source's `f` suffix, so
                 * that distinction is already lost by the time we see the
                 * bytecode. If val_type says this value is still an integer,
                 * MTC1 alone would just reinterpret its bit pattern as a
                 * float (e.g. int -1 == 0xFFFFFFFF == a NaN as float bits) -
                 * numerically convert it instead. */
                if (NATIVE_TYPE_IS_INT(val_type)) {
                    mips_cvt_s_w(em, FPU_F0, FPU_F0);
                }
            } else {
                /* Integer return - value must go to $v0.
                 * Mirror of the float case above: if the value on the stack is
                 * a float, moving it as-is hands the caller the raw bit
                 * pattern. Returning 3.9f from an int-returning function used
                 * to yield 1081711002, which is exactly bits(3.9f). Convert
                 * numerically (truncating toward zero, like a JS int cast). */
                if (NATIVE_TYPE_IS_FLOAT(val_type)) {
                    mips_mtc1(em, val, FPU_F0);
                    mips_cvt_w_s(em, FPU_F0, FPU_F0);
                    mips_mfc1(em, REG_V0, FPU_F0);
                } else if (val == REG_AT) {
                    MipsReg tmp = REG_T7;
                    mips_move(em, tmp, REG_AT);
                    mips_move(em, REG_V0, tmp);
                } else if (val != REG_V0) {
                    mips_move(em, REG_V0, val);
                }
            }
            
            /* Jump to epilogue, but skip if epilogue is right after us
             * (optimization: avoid redundant J to next instruction)
             * 
             * We check if skip_epilogue_jump is set, which means this block's
             * IR_RETURN is the last code before epilogue.
             */
            if (!nc->skip_epilogue_jump) {
                mips_j(em, nc->epilogue_label);
                mips_nop(em);  /* CRITICAL: Delay slot - prevents next instruction from executing */
            }
            break;
        }

        
        case IR_RETURN_VOID:
            /* Other branch arms may be laid out after this return. Falling
             * through into them can repeat side effects or loop forever. */
            if (!nc->skip_epilogue_jump) {
                mips_j(em, nc->epilogue_label);
                mips_nop(em);
            }
            break;
        
        /* Stack ops */
        case IR_DUP: {
            MipsReg src = PEEK_REG();
            NativeType src_type = PEEK_TYPE(0);  /* Get type of top element */
            MipsReg dst = PUSH_REG();
            mips_move(em, dst, src);
            PUSH_TYPE(src_type);  /* CRITICAL: Also duplicate the type! */
            break;
        }
        
        case IR_DROP:
            POP_REG();
            POP_TYPE();  /* Also pop type */
            break;
        
        case IR_SWAP: {
            MipsReg r1 = POP_REG();
            NativeType t1 = POP_TYPE();
            MipsReg r2 = POP_REG();
            NativeType t2 = POP_TYPE();
            /* Swap using XOR */
            mips_xor(em, r1, r1, r2);
            mips_xor(em, r2, r1, r2);
            mips_xor(em, r1, r1, r2);
            PUSH_REG();  /* r2 */
            PUSH_TYPE(t1);  /* Push t1 (was r1's type) */
            PUSH_REG();  /* r1 */
            PUSH_TYPE(t2);  /* Push t2 (was r2's type) */
            
            /* Also swap stack_local_source tracking for struct field access */
            if (nc->stack_depth >= 2) {
                int temp_source = nc->stack_local_source[nc->stack_depth - 1];
                nc->stack_local_source[nc->stack_depth - 1] = nc->stack_local_source[nc->stack_depth - 2];
                nc->stack_local_source[nc->stack_depth - 2] = temp_source;
            }
            break;
        }
        
        default:
            nc->has_error = true;
            snprintf(nc->error_msg, sizeof(nc->error_msg),
                     "Unsupported IR opcode: %d", instr->op);
            return -1;
    }

    /* nc_push_reg() (used by PUSH_REG() above) can set has_error mid-switch
     * without unwinding immediately, since it's called from deep inside each
     * case rather than at a single entry point. Catch that here so overflow
     * still aborts compilation instead of silently continuing with a
     * clamped/aliased register. */
    if (nc->has_error) {
        return -1;
    }

    return 0;

    #undef STACK_REG
    #undef PUSH_REG
    #undef POP_REG
    #undef PEEK_REG
}

/*
 * Net effect of a single IR instruction on the eval operand stack
 * (pushes minus pops). Mirrors the push/pop pattern documented on each
 * IROp in native_compiler.h and already implemented ad hoc throughout
 * emit_ir_instruction (PUSH_REG/POP_REG call counts) - used only to
 * compute per-block ENTRY stack depths (see compute_block_entry_depths
 * below), never to drive codegen directly, so an occasional imprecise
 * case here can't corrupt already-working straight-line code: it would
 * only affect the entry depth of a block that's reached by more than one
 * path, which is exactly the scenario this exists to fix.
 */
static int ir_op_stack_delta(const IRInstr *instr) {
    switch (instr->op) {
        /* Push 1, pop nothing */
        case IR_CONST_I32: case IR_CONST_F32: case IR_CONST_I64:
        case IR_LOAD_LOCAL:
        case IR_STRING_NEW:
        case IR_ARRAY_NEW:
        case IR_DUP:
            return 1;

        /* Pop 1, push nothing */
        case IR_STORE_LOCAL:
        case IR_STORE_FIELD:
        case IR_DROP:
        case IR_JUMP_IF_TRUE:
        case IR_JUMP_IF_FALSE:
        case IR_RETURN:
            return -1;

        /* Pop 3, push nothing */
        case IR_STORE_ARRAY:
            return -3;

        /* Struct array field access - deltas below match the ACTUAL
         * MIPS codegen for these three ops exactly (verified against the
         * POP_REG/PUSH_REG call counts in each case), not copied from the
         * (possibly stale, see IR_LOAD_FIELD/IR_STORE_FIELD above) doc
         * comments on the IR op enum. This matters more here than for most
         * ops: the primary use case for struct arrays is a loop body, which
         * is exactly the multi-predecessor-block scenario this table exists
         * to get right. */
        case IR_ARRAY_ELEM_ADDR:  /* pop base, pop index -> push element addr */
            return -1;
        case IR_LOAD_FIELD_DYN:   /* pop base ptr -> push value */
            return 0;
        case IR_STORE_FIELD_DYN:  /* pop base ptr, pop value -> push nothing */
            return -2;

        /* Pop 1, push 1 (unary ops, conversions, field load) */
        case IR_NEG_I32: case IR_NEG_I64: case IR_NEG_F32:
        case IR_SQRT_F32: case IR_ABS_F32: case IR_SIGN_F32:
        case IR_FROUND_F32: case IR_SATURATE_F32: case IR_RSQRT_F32:
        case IR_STRING_LENGTH: case IR_STRING_TO_UPPER:
        case IR_STRING_TO_LOWER: case IR_STRING_TRIM:
        case IR_ARRAY_POP: case IR_ARRAY_LENGTH:
        case IR_NOT:
        case IR_I32_TO_F32: case IR_F32_TO_I32:
        case IR_I32_TO_I64: case IR_I64_TO_I32:
        case IR_I64_TO_F32: case IR_F32_TO_I64:
            return 0;

        /* Legacy local-based load pushes a value; dynamic field addresses
         * replace their runtime base pointer with the resulting address. */
        case IR_LOAD_FIELD:
            return 1;
        case IR_LOAD_FIELD_ADDR:
            return 0;

        /* Pop 3, push 1 */
        case IR_CLAMP_F32: case IR_FMA_F32: case IR_LERP_F32:
        case IR_SMOOTHSTEP_F32: case IR_STRING_SLICE: case IR_STRING_REPLACE:
            return -2;

        /* Pop 0, push 0 (control flow markers, or ops acting on a local
         * by index rather than the stack) */
        case IR_JUMP: case IR_LABEL: case IR_NOP:
        case IR_RETURN_VOID:
        case IR_ADD_LOCAL_CONST:
            return 0;

        case IR_SWAP:
            return 0;

        case IR_ARRAY_CLEAR:
            return -1;

        case IR_CALL: case IR_TAIL_CALL: case IR_CALL_C_FUNC: {
            /* Indirect calls (func_ptr == NULL) also pop the callee
             * reference itself, pushed by a preceding IR_CONST_I32 (see
             * nc_emit_func_ptr_const) - direct calls (IR_CALL_C_FUNC, or
             * any IR_CALL/IR_TAIL_CALL with a known func_ptr) don't. */
            int n = instr->operand.call.arg_count;
            int callee_pop = (instr->op != IR_CALL_C_FUNC && instr->operand.call.func_ptr == NULL) ? 1 : 0;
            int pushes = instr->op == IR_TAIL_CALL ? 0 : 1;
            return pushes - n - callee_pop;
        }

        /* Everything else covered by the ops above's comments: binary
         * arithmetic/bitwise/comparison ops (pop 2, push 1). This is the
         * majority of IROp, so treat it as the default rather than
         * listing all ~50 of them. */
        default:
            return -1;
    }
}

/*
 * Compute, for every block, the eval-stack depth guaranteed to be present
 * when control reaches it - used so ir_to_native's per-block reset (below)
 * doesn't blindly assume 0 for blocks that are legitimately entered with a
 * value already on the stack (e.g. a ternary whose two branches each leave
 * exactly one value before jumping/falling into their shared continuation -
 * QuickJS bytecode guarantees every such merge point sees a consistent
 * depth from all of its predecessors, since that's a basic soundness
 * requirement of stack-machine bytecode, so it's always safe to adopt
 * whichever predecessor's exit depth we discover first).
 *
 * Block 0 (function entry) always starts empty. Propagation runs as a
 * fixed-point over at most block_count passes: a predecessor can have a
 * HIGHER array index than its successor (e.g. the "else" arm of a ternary,
 * discovered/numbered after the merge point it flows into), so a single
 * forward sweep isn't enough. Any block never reached this way (dead code,
 * or a loop header whose only predecessor is its own later back-edge)
 * defaults to 0, matching the previous unconditional behavior exactly.
 */
static void compute_block_entry_depths(const IRFunction *ir, int *entry_depth) {
    for (int b = 0; b < ir->block_count; b++) entry_depth[b] = -1;
    if (ir->block_count > 0) entry_depth[0] = 0;

    for (int pass = 0; pass <= ir->block_count; pass++) {
        bool changed = false;
        for (int b = 0; b < ir->block_count; b++) {
            if (entry_depth[b] < 0) continue;  /* not reached yet this pass */
            int depth = entry_depth[b];
            const IRBasicBlock *block = &ir->blocks[b];
            for (int i = 0; i < block->instr_count; i++) {
                depth += ir_op_stack_delta(&block->instrs[i]);
                if (depth < 0) depth = 0;  /* defensive clamp, shouldn't happen */
            }
            for (int s = 0; s < 2; s++) {
                int succ = block->successors[s];
                if (succ >= 0 && succ < ir->block_count && entry_depth[succ] < 0) {
                    entry_depth[succ] = depth;
                    changed = true;
                }
            }
        }
        if (!changed) break;
    }

    for (int b = 0; b < ir->block_count; b++) {
        if (entry_depth[b] < 0) entry_depth[b] = 0;
    }
}

static bool nc_edge_needs_conversion(const NativeCompiler *nc,
                                     const IRBasicBlock *target, int depth) {
    for (int i = 0; i < depth && i < target->entry_stack_depth; i++) {
        if (target->entry_stack_types[i] == NATIVE_TYPE_FLOAT32 &&
            NATIVE_TYPE_IS_INT(nc->stack_types[i])) return true;
    }
    return false;
}

static void nc_emit_edge_conversions(NativeCompiler *nc, const IRBasicBlock *target) {
    for (int i = 0; i < nc->stack_depth && i < target->entry_stack_depth && i < 8; i++) {
        if (target->entry_stack_types[i] == NATIVE_TYPE_FLOAT32 &&
            NATIVE_TYPE_IS_INT(nc->stack_types[i])) {
            MipsEmitter *em = &nc->emitter;
            MipsFpuReg f = mips_alloc_fpr(em);
            MipsReg value = (MipsReg)(REG_S0 + i);
            mips_mtc1(em, value, f);
            mips_cvt_s_w(em, f, f);
            mips_mfc1(em, value, f);
            mips_free_fpr(em, f);
        }
    }
}

/*
 * Generate native code from IR function.
 */
int ir_to_native(NativeCompiler *nc, const IRFunction *ir) {
    if (!nc || !ir) return -1;
    
    NC_LOG("ir_to_native: block_count=%d local_count=%d ret_type=%d",
           ir->block_count, ir->local_count, ir->sig.return_type);
    
    if (ir->block_count > NC_MAX_BASIC_BLOCKS) {
        nc->has_error = true;
        snprintf(nc->error_msg, sizeof(nc->error_msg),
                 "Too many basic blocks (%d, max %d)", ir->block_count, NC_MAX_BASIC_BLOCKS);
        return -1;
    }
    
    MipsEmitter *em = &nc->emitter;
    
    /* Reset emitter for new function */
    mips_emitter_reset(em);
    
    /* Reset operand stack */
    nc->stack_top = 0;
    
    /* Create a label for the function epilogue */
    nc->epilogue_label = mips_label_create(em);
    if (nc->epilogue_label < 0) return -1;
    nc->stack_depth = 0;
    
    /* Allocate locals */
    allocate_locals(nc, ir);

    /* $s0-$s7: eval operand stack (callee-saved, survive C runtime calls) */
    nc->used_saved_regs |= 0xFF;
    nc->needs_fp = true;
    
    /* Create labels for each basic block and map block index to label ID */
    int block_to_label[NC_MAX_BASIC_BLOCKS];
    for (int b = 0; b < ir->block_count; b++) {
        block_to_label[b] = mips_label_create(em);
        if (block_to_label[b] < 0) {
            printf("ERROR: Failed to create label for block %d\n", b);
            return -1;
        }
    }
    
    /* Detect if this is a leaf function (no calls) */
    nc->is_leaf = detect_leaf_function(ir);
    
    /* Emit function prologue */
    emit_function_prologue(nc, &ir->sig);
    
    /* First pass: identify which blocks are targets of branches/jumps */
    bool is_branch_target[NC_MAX_BASIC_BLOCKS];
    for (int i = 0; i < NC_MAX_BASIC_BLOCKS; i++) {
        is_branch_target[i] = false;
    }
    for (int b = 0; b < ir->block_count; b++) {
        IRBasicBlock *block = &ir->blocks[b];
        for (int i = 0; i < block->instr_count; i++) {
            IRInstr *instr = &block->instrs[i];
            if (instr->op == IR_JUMP || instr->op == IR_JUMP_IF_TRUE || instr->op == IR_JUMP_IF_FALSE) {
                int target_block = instr->operand.label_id;
                if (target_block >= 0 && target_block < ir->block_count) {
                    is_branch_target[target_block] = true;
                }
            }
        }
    }
    
    /* Find the last block with a return */
    int last_return_block = -1;
    for (int b = ir->block_count - 1; b >= 0; b--) {
        IRBasicBlock *block = &ir->blocks[b];
        for (int i = block->instr_count - 1; i >= 0; i--) {
            if (block->instrs[i].op == IR_RETURN || block->instrs[i].op == IR_RETURN_VOID) {
                last_return_block = b;
                break;
            }
        }
        if (last_return_block >= 0) break;
    }
    /* If no return found, use the last block */
    if (last_return_block < 0) {
        last_return_block = ir->block_count - 1;
    }
    
    /* Real per-block entry stack depth (a branch target isn't necessarily
     * empty - e.g. a ternary's two arms each leave one value before
     * jumping/falling into their shared merge block). Replaces the old
     * unconditional reset-to-0 below. */
    int block_entry_depth[NC_MAX_BASIC_BLOCKS];
    compute_block_entry_depths(ir, block_entry_depth);

    /* Emit code for each basic block */
    for (int b = 0; b < ir->block_count; b++) {
        /* Bind block label using the mapped label ID */
        mips_label_bind(em, block_to_label[b]);

        IRBasicBlock *block = &ir->blocks[b];
        NC_LOG("emit: block %d/%d instr_count=%d", b, ir->block_count, block->instr_count);

        /* Reset type stack when entering a branch target, to whatever
         * depth is actually guaranteed to be present there. */
        {
            int depth = block_entry_depth[b];
            if (depth < 0) depth = 0;
            if (depth > 8) depth = 8;  /* $s0-$s7 physical register cap, see nc_push_reg */
            nc->stack_top = depth;
            nc->stack_depth = depth;
            for (int i = 0; i < depth; i++)
                nc->stack_types[i] = i < block->entry_stack_depth ?
                    block->entry_stack_types[i] : NATIVE_TYPE_UNKNOWN;
        }
        
        /* If block is empty and is a branch target, we need to handle it carefully.
         * Don't jump to last_return_block directly - instead, find the next non-empty
         * block that contains a return, or fall through to the next block. */
        if (block->instr_count == 0 && is_branch_target[b]) {
            /* Find the next block with a return, starting from this block */
            int target_block = -1;
            for (int next = b + 1; next < ir->block_count; next++) {
                IRBasicBlock *next_block = &ir->blocks[next];
                for (int i = 0; i < next_block->instr_count; i++) {
                    if (next_block->instrs[i].op == IR_RETURN || 
                        next_block->instrs[i].op == IR_RETURN_VOID) {
                        target_block = next;
                        break;
                    }
                }
                if (target_block >= 0) break;
            }
            
            /* If no return found in subsequent blocks, use last_return_block */
            if (target_block < 0) {
                target_block = last_return_block;
            }
            
            if (target_block >= 0 && target_block < ir->block_count) {
                int target_label = block_to_label[target_block];
                mips_j(em, target_label);
                mips_nop(em);  /* Delay slot */
            }
            /* Check if we need an explicit jump to the fallthrough block */
            /* This happens if the fallthrough block is not the next physical block */
            int fallthrough = block->successors[1];
            
            if (fallthrough >= 0 && fallthrough < ir->block_count && fallthrough != b + 1) {
                /* Check if the last instruction was already an unconditional jump or return */
                bool needs_jump = true;
                if (block->instr_count > 0) {
                    IROp last_op = block->instrs[block->instr_count - 1].op;
                    if (last_op == IR_JUMP || last_op == IR_RETURN || last_op == IR_RETURN_VOID) {
                        needs_jump = false;
                    }
                }
                
                if (needs_jump) {
                    mips_j(em, block_to_label[fallthrough]);
                    mips_nop(em); /* Delay slot */
                }
            }
        } else {
            /* Check if this block ends with a conditional jump - if so, the next block
             * should be the fall-through (body of loop), not a jump to exit */
            bool has_conditional_jump = false;
            int last_instr_idx = block->instr_count - 1;
            if (last_instr_idx >= 0) {
                IRInstr *last_instr = &block->instrs[last_instr_idx];
                if (last_instr->op == IR_JUMP_IF_FALSE || last_instr->op == IR_JUMP_IF_TRUE) {
                    has_conditional_jump = true;
                }
            }
            nc->skip_epilogue_jump = false;  /* Default to false, will set true for last IR_RETURN */
            
            /* Emit instructions */
            for (int i = 0; i < block->instr_count; i++) {
                IRInstr *instr = &block->instrs[i];
                IRInstr temp_instr = *instr;  /* Copy to avoid modifying original */
                
                /* Register targeting optimization
                 * Load constants directly to $v0 when followed by RETURN, saving 1 instruction.
                 * SAFETY: Only apply in the last block to avoid interfering with loops. */
                bool value_targeting_applied = false;
                bool is_last_block = (b == ir->block_count - 1);
                
                if (is_last_block && ir->sig.return_type == NATIVE_TYPE_INT32 &&
                    i + 1 < block->instr_count) {
                    IROp next_op = block->instrs[i + 1].op;
                    if (next_op == IR_RETURN) {
                        /* This value will be returned - emit directly to $v0 */
                        if (temp_instr.op == IR_CONST_I32) {
                            /* Load constant directly into $v0 instead of temp reg */
                            MipsEmitter *em = &nc->emitter;
                            if (temp_instr.op == IR_CONST_I32) {
                                mips_li(em, REG_V0, temp_instr.operand.i32);
                            } else {
                                union { float f; int32_t i; } u;
                                u.f = temp_instr.operand.f32;
                                mips_li(em, REG_V0, u.i);
                            }
                            /* Skip the next IR_RETURN since value is already in $v0 */
                            i++;  /* Skip IR_RETURN */
                            value_targeting_applied = true;
                        }
                    }
                }
                
                if (value_targeting_applied) {
                    continue;  /* Skip normal emission - we handled both CONST and RETURN */
                }
                
                /* Set skip_epilogue_jump if this is IR_RETURN and no more real code follows */
                if (temp_instr.op == IR_RETURN || temp_instr.op == IR_RETURN_VOID) {
                    bool all_empty_after = true;
                    /* Check remaining instructions in this block */
                    for (int j = i + 1; j < block->instr_count && all_empty_after; j++) {
                        IROp op = block->instrs[j].op;
                        /* IR_NOP, IR_RETURN, IR_RETURN_VOID are effectively "empty" 
                         * because IR_RETURN won't generate J if it's the last */
                        if (op != IR_NOP && op != IR_RETURN && op != IR_RETURN_VOID) {
                            all_empty_after = false;
                        }
                    }
                    /* Check all subsequent blocks */
                    for (int next = b + 1; next < ir->block_count && all_empty_after; next++) {
                        for (int j = 0; j < ir->blocks[next].instr_count && all_empty_after; j++) {
                            IROp op = ir->blocks[next].instrs[j].op;
                            if (op != IR_NOP && op != IR_RETURN && op != IR_RETURN_VOID) {
                                all_empty_after = false;
                            }
                        }
                    }
                    nc->skip_epilogue_jump = all_empty_after;
                }
                
                /* Map label IDs in jump instructions */
                int original_block_id = -1;
                if (temp_instr.op == IR_JUMP || temp_instr.op == IR_JUMP_IF_TRUE || temp_instr.op == IR_JUMP_IF_FALSE) {
                    original_block_id = temp_instr.operand.label_id;
                    if (original_block_id >= 0 && original_block_id < ir->block_count) {
                        int mapped_label = block_to_label[original_block_id];
                        temp_instr.operand.label_id = mapped_label;
                    }
                }

                if (original_block_id >= 0 && original_block_id < ir->block_count) {
                    const IRBasicBlock *target = &ir->blocks[original_block_id];
                    if (temp_instr.op == IR_JUMP) {
                        nc_emit_edge_conversions(nc, target);
                    } else if (nc_edge_needs_conversion(nc, target, nc->stack_depth - 1)) {
                        /* Conversions on the taken edge must not alter values
                         * on the other edge. Keep the condition's delay slot
                         * empty, then emit a small edge adapter. */
                        int adapter = mips_label_create(em), after = mips_label_create(em);
                        int destination = temp_instr.operand.label_id;
                        temp_instr.operand.label_id = adapter;
                        if (emit_ir_instruction(nc, &temp_instr, NULL) < 0) return -1;
                        mips_j(em, after); mips_nop(em);
                        mips_label_bind(em, adapter);
                        nc_emit_edge_conversions(nc, target);
                        mips_j(em, destination); mips_nop(em);
                        mips_label_bind(em, after);
                        continue;
                    }
                }
                
                /* CRITICAL FIX REVISED: 
                 * Only skip the explicit jump if it targets the IMMEDIATELY FOLLOWING physical block.
                 * If the target is elsewhere (e.g. jumping over an Exit block), we MUST emit the jump.
                 */
                if (temp_instr.op == IR_JUMP && has_conditional_jump && i == last_instr_idx) {
                    /* Check if the jump target is the immediately following physical block (b + 1) */
                    int next_physical_label = -1;
                    if (b + 1 < ir->block_count) {
                        next_physical_label = block_to_label[b + 1];
                    }
                    
                    /* If the jump points to where we would naturally fall through, skip it */
                    if (temp_instr.operand.label_id == next_physical_label) {
                        continue;
                    }
                    
                    /* Otherwise, EMIT the jump (necessary to skip intermediate block) */
                }
                
                if (emit_ir_instruction(nc, &temp_instr, NULL) < 0) {
                    NC_LOG("ir_to_native: emit_ir_instruction failed at block=%d instr=%d", b, i);
                    return -1;
                }
            }
            
            /* CRITICAL FIX: check if we need an explicit jump to the semantic
             * fallthrough block. This handles cases where the physical block
             * order doesn't match the semantic fallthrough (e.g., Exit block
             * placed before Body block).
             *
             * This used to only fire when the block's last instruction was a
             * conditional jump (IR_JUMP_IF_FALSE/TRUE), but a block can just
             * as easily end on an ordinary instruction with no jump at all -
             * e.g. one arm of a ternary whose result feeds a later call
             * ends on an IR_STORE_LOCAL, then naturally falls through to
             * wherever the bytecode's merge point was discovered as a block
             * (see bytecode_to_ir's main decode loop, which now sets
             * successors[1] on exactly this kind of fallthrough transition).
             * Any block whose last instruction ISN'T already an
             * unconditional terminator (IR_JUMP/IR_RETURN/IR_RETURN_VOID -
             * those already went where they need to) needs this same check,
             * not just ones ending in a conditional jump. */
            if (block->instr_count > 0) {
                IROp last_op = block->instrs[block->instr_count - 1].op;
                bool already_terminates = (last_op == IR_JUMP ||
                                           last_op == IR_RETURN ||
                                           last_op == IR_RETURN_VOID);
                if (!already_terminates) {
                    int semantic_fallthrough = block->successors[1];
                    int physical_next = b + 1;
                    if (semantic_fallthrough >= 0 && semantic_fallthrough < ir->block_count)
                        nc_emit_edge_conversions(nc, &ir->blocks[semantic_fallthrough]);
                    if (semantic_fallthrough >= 0 &&
                        semantic_fallthrough < ir->block_count &&
                        semantic_fallthrough != physical_next) {
                        /* Physical next block is different from semantic fallthrough.
                         * We need an explicit jump to the correct block. */
                        mips_j(em, block_to_label[semantic_fallthrough]);
                        mips_nop(em);  /* Delay slot */
                    }
                }
            }
        }
    }
    
    /* Bind the special epilogue label */
    mips_label_bind(em, nc->epilogue_label);
    
    /* Each IR_RETURN writes the ABI result before jumping here. The stack
     * left by the last emitted block may belong to a different runtime path. */
    
    /* Emit function epilogue */
    emit_function_epilogue(nc, &ir->sig);
    
    /* Finalize code (resolve fixups, flush cache) */
    return mips_emitter_finalize(em);
}

/* ============================================
 * Compiler API
 * ============================================ */

/*
 * Register standard Math functions that can be called from compiled JS code.
 */
static void native_register_math_functions(NativeCompiler *nc) {
    /* Single-argument float -> float functions */
    static NativeFuncSignature float_to_float = {
        .arg_count = 1,
        .arg_types = { NATIVE_TYPE_FLOAT32 },
        .return_type = NATIVE_TYPE_FLOAT32
    };
    
    /* Two-argument float -> float functions */
    static NativeFuncSignature float2_to_float = {
        .arg_count = 2,
        .arg_types = { NATIVE_TYPE_FLOAT32, NATIVE_TYPE_FLOAT32 },
        .return_type = NATIVE_TYPE_FLOAT32
    };
    
    /* Single-argument int -> int functions */
    static NativeFuncSignature int_to_int = {
        .arg_count = 1,
        .arg_types = { NATIVE_TYPE_INT32 },
        .return_type = NATIVE_TYPE_INT32
    };
    
    /* Basic Math functions (also available as native FPU instructions) */
    native_register_function(nc, "Math.sqrt", (void*)sqrtf, &float_to_float);
    native_register_function(nc, "Math.abs", (void*)fabsf, &float_to_float);
    
    /* Exponential and logarithmic functions */
    native_register_function(nc, "Math.exp", (void*)expf, &float_to_float);
    native_register_function(nc, "Math.log", (void*)logf, &float_to_float);
    native_register_function(nc, "Math.log10", (void*)log10f, &float_to_float);
    native_register_function(nc, "Math.log2", (void*)log2f, &float_to_float);
    native_register_function(nc, "Math.expm1", (void*)expm1f, &float_to_float);
    native_register_function(nc, "Math.log1p", (void*)log1pf, &float_to_float);
    
    /* Trigonometric functions */
    native_register_function(nc, "Math.sin", (void*)sinf, &float_to_float);
    native_register_function(nc, "Math.cos", (void*)cosf, &float_to_float);
    native_register_function(nc, "Math.tan", (void*)tanf, &float_to_float);
    native_register_function(nc, "Math.asin", (void*)asinf, &float_to_float);
    native_register_function(nc, "Math.acos", (void*)acosf, &float_to_float);
    native_register_function(nc, "Math.atan", (void*)atanf, &float_to_float);
    
    /* Hyperbolic functions */
    native_register_function(nc, "Math.sinh", (void*)sinhf, &float_to_float);
    native_register_function(nc, "Math.cosh", (void*)coshf, &float_to_float);
    native_register_function(nc, "Math.tanh", (void*)tanhf, &float_to_float);
    native_register_function(nc, "Math.asinh", (void*)asinhf, &float_to_float);
    native_register_function(nc, "Math.acosh", (void*)acoshf, &float_to_float);
    native_register_function(nc, "Math.atanh", (void*)atanhf, &float_to_float);
    
    /* Rounding functions */
    native_register_function(nc, "Math.floor", (void*)floorf, &float_to_float);
    native_register_function(nc, "Math.ceil", (void*)ceilf, &float_to_float);
    native_register_function(nc, "Math.round", (void*)roundf, &float_to_float);
    native_register_function(nc, "Math.trunc", (void*)truncf, &float_to_float);
    
    /* Other single-arg functions */
    native_register_function(nc, "Math.cbrt", (void*)cbrtf, &float_to_float);       // Cube root
    
    /* Two-argument Math functions */
    native_register_function(nc, "Math.pow", (void*)powf, &float2_to_float);
    native_register_function(nc, "Math.atan2", (void*)atan2f, &float2_to_float);
    native_register_function(nc, "Math.hypot", (void*)hypotf, &float2_to_float);
    native_register_function(nc, "Math.min", (void*)fminf, &float2_to_float);
    native_register_function(nc, "Math.max", (void*)fmaxf, &float2_to_float);
    
    /* Integer abs (int -> int) */
    native_register_function(nc, "abs", (void*)abs, &int_to_int);
}

int native_compiler_init(NativeCompiler *nc, JSContext *ctx) {
    if (!nc) return -1;
    
    memset(nc, 0, sizeof(NativeCompiler));
    nc->js_ctx = ctx;
    
    if (mips_emitter_init(&nc->emitter) < 0) {
        return -1;
    }
    
    /* Register standard math functions */
    native_register_math_functions(nc);
    
    return 0;
}

void native_compiler_free(NativeCompiler *nc) {
    if (!nc) return;
    
    mips_emitter_free(&nc->emitter);
    ir_func_free(&nc->ir_func);
}

/*
 * Extract bytecode from a QuickJS function.
 * Uses the official JS_GetFunctionBytecode API from quickjs.h
 */
static int extract_js_bytecode(NativeCompiler *nc, JSValueConst js_func,
                                const uint8_t **out_bytecode, size_t *out_len,
                                int *out_arg_count, int *out_var_count) {
    JSFunctionBytecodeInfo info;
    int ret;
    
    /* Use the QuickJS API to get bytecode info */
    ret = JS_GetFunctionBytecodeInfo(nc->js_ctx, js_func, &info);
    if (ret != 0) {
        return ret;
    }
    
    /* Extract bytecode info. The interpreter's bytecode carries
     * superinstructions; the compiler decodes the plain sequence, from a
     * copy (kept until the next compilation). */
    {
        static uint8_t *plain_buf;
        static size_t plain_cap;
        if (info.bytecode_len > plain_cap) {
            uint8_t *nb = realloc(plain_buf, info.bytecode_len);
            if (!nb)
                return -3;
            plain_buf = nb;
            plain_cap = info.bytecode_len;
        }
        memcpy(plain_buf, info.bytecode, info.bytecode_len);
        JS_UnfuseBytecode(plain_buf, info.bytecode_len);
        *out_bytecode = plain_buf;
    }
    *out_len = info.bytecode_len;
    *out_arg_count = info.arg_count;
    *out_var_count = info.var_count;
    nc->cpool = info.cpool;
    nc->cpool_count = info.cpool_count;
    nc->closure_var_count = info.closure_var_count;
    
    return 0;
}

CompileResult native_compile_function(NativeCompiler *nc, JSValueConst js_func,
                                       const NativeFuncSignature *sig) {
    CompileResult result = { .success = false, .error_msg = NULL };
    
    if (!nc || !sig) {
        result.error_msg = "Invalid arguments";
        return result;
    }
    
    /* Reset compiler state */
    nc->has_error = false;
    nc->error_msg[0] = '\0';
    nc_free_string_literals(nc);
    nc->cpool = NULL;
    nc->cpool_count = 0;
    nc->current_js_func = js_func;
    nc->closure_var_count = 0;
    nc->has_pending_call_sig = false;
    ir_func_free(&nc->ir_func);
    
    /* Initialize IR function (this zeros the sig, but we'll set it below) */
    ir_func_init(&nc->ir_func);
    
    /* Extract bytecode from the JS function */
    const uint8_t *bytecode = NULL;
    size_t bytecode_len = 0;
    int js_arg_count = 0, js_var_count = 0;
    
    int extract_result = extract_js_bytecode(nc, js_func, 
                                              &bytecode, &bytecode_len,
                                              &js_arg_count, &js_var_count);
    
    if (extract_result != 0) {
        switch (extract_result) {
            case -1:
                result.error_msg = "Not a bytecode function (C function or bound function?)";
                break;
            case -2:
                result.error_msg = "Invalid function value";
                break;
            default:
                result.error_msg = "Failed to extract bytecode";
        }
        return result;
    }
    
    if (!bytecode || bytecode_len == 0) {
        result.error_msg = "Empty bytecode";
        return result;
    }
    
    nc->ir_func.sig = *sig;
    
    /* Store bytecode info for use during parsing */
    nc->js_arg_count = js_arg_count;
    nc->js_var_count = js_var_count;
    
    /* Parse bytecode to IR */
    NC_LOG("compile: START args=%d vars=%d bytecode_len=%zu", js_arg_count, js_var_count, bytecode_len);
    int parse_result = bytecode_to_ir(nc, bytecode, bytecode_len, &nc->ir_func);
    NC_LOG("compile: bytecode_to_ir returned %d", parse_result);
    if (parse_result < 0)
        NC_LOG("compile: error: %s", nc->error_msg);
    if (parse_result < 0) {
        /* If nc->error_msg was set, use it; otherwise create generic message */
        if (nc->error_msg[0] == '\0') {
            snprintf(nc->error_msg, sizeof(nc->error_msg),
                     "Bytecode parsing failed (code=%d, bytecode_len=%zu, blocks=%d)",
                     parse_result, bytecode_len, nc->ir_func.block_count);
        }
        result.error_msg = nc->error_msg;
        nc_free_string_literals(nc);
        return result;
    }
    
    /* Initialize locals from signature - do this BEFORE type inference */
    if (nc->ir_func.local_count < sig->arg_count) {
        nc->ir_func.local_count = sig->arg_count;
    }
    for (int i = 0; i < sig->arg_count; i++) {
        nc->ir_func.local_types[i] = sig->arg_types[i];
    }
    
    /* Infer types */
    int infer_result = infer_types(&nc->ir_func, sig);
    if (infer_result < 0) {
        snprintf(nc->error_msg, sizeof(nc->error_msg),
                 "Type inference failed (code=%d, blocks=%d, locals=%d)",
                 infer_result, nc->ir_func.block_count, nc->ir_func.local_count);
        result.error_msg = nc->error_msg;
        nc_free_string_literals(nc);
        return result;
    }
    
    /* Validate types */
    TypeCheckResult tc = validate_types(&nc->ir_func, sig);
    if (tc != TYPE_CHECK_OK) {
        /* Count total instructions for debug */
        int total_instrs = 0;
        for (int b = 0; b < nc->ir_func.block_count; b++) {
            total_instrs += nc->ir_func.blocks[b].instr_count;
        }
        
        snprintf(nc->error_msg, sizeof(nc->error_msg),
                 "Type validation failed (result=%d, blocks=%d, instrs=%d, args=%d, ret=%d)",
                 tc, nc->ir_func.block_count, total_instrs, sig->arg_count, sig->return_type);
        result.error_msg = nc->error_msg;
        nc_free_string_literals(nc);
        return result;
    }
    
    /* IR optimization passes - run multiple times for cascading optimizations */
    int total_optimized = 0;
    for (int pass = 0; pass < 5; pass++) {
        int opt = 0;
        opt += optimize_ir_constant_folding(&nc->ir_func);
        opt += optimize_ir_strength_reduction(&nc->ir_func);
        opt += optimize_ir_loop_increments(&nc->ir_func);  /* Re-enabled with $at fix */
        opt += optimize_ir_dead_code(&nc->ir_func);  /* Dead code elimination */
        opt += optimize_ir_cse(&nc->ir_func);  /* Common subexpression elimination */
        
        if (opt == 0) break;  /* No more optimizations found */
        
        total_optimized += opt;
        compact_ir(&nc->ir_func);  /* Remove NOPs for next pass */
    }
    
    /* Generate native code */
    if (ir_to_native(nc, &nc->ir_func) < 0) {
        NC_LOG("native_compile_function: ir_to_native failed, error_msg='%s'", nc->error_msg);
        result.error_msg = nc->error_msg[0] ? nc->error_msg : "Code generation failed";
        nc_free_string_literals(nc);
        return result;
    }
    NC_LOG("compile: ir_to_native OK, code_size=%zu bytes", nc->emitter.buffer.count * 4);
    
    /* Run peephole optimizations */
    mips_emitter_peephole_optimize(&nc->emitter);
    
    /* Finalize code (resolve labels, flush cache) */
    if (mips_emitter_finalize(&nc->emitter) < 0) {
        result.error_msg = nc->emitter.error_msg ? nc->emitter.error_msg : "Code finalization failed";
        nc_free_string_literals(nc);
        return result;
    }
    
    /* Dump generated assembly for debugging */
#if NC_TRACE
    /* Full disassembly of every compiled function - invaluable when chasing a
     * miscompile, far too noisy otherwise. Build with -DNC_TRACE=1 to get it. */
    mips_emitter_dump(&nc->emitter, "Compiled Function");
#endif
    
    /* Package result - The emitter now uses arena-style allocation:
     * Each function's code stays permanently in the buffer at its position.
     * mips_emitter_get_code returns pointer to this function's start. */
    size_t code_size;
    void *code_ptr = mips_emitter_get_code(&nc->emitter, &code_size);
    if (!code_ptr || code_size == 0) {
        result.error_msg = "No code generated";
        nc_free_string_literals(nc);
        return result;
    }
    
    result.success = true;
    result.func.code_ptr = code_ptr;  /* Points into arena buffer */
    result.func.code_size = code_size;
    result.func.sig = *sig;
    result.func.is_valid = true;
    result.func.owned_literals = nc->compile_string_literals;
    result.func.owned_literal_count = nc->compile_string_count;
    nc->compile_string_literals = NULL;
    nc->compile_string_count = 0;
    nc->compile_string_capacity = 0;
    
    return result;
}

void native_func_free(NativeFunc *func) {
    if (!func) return;
    
    if (func->owned_literals) {
        for (int i = 0; i < func->owned_literal_count; i++)
            free(func->owned_literals[i]);
        free(func->owned_literals);
        func->owned_literals = NULL;
        func->owned_literal_count = 0;
    }
    
    /* Code is managed by emitter's arena buffer, don't free it */
    func->code_ptr = NULL;
    func->code_size = 0;
    func->is_valid = false;
}

/*
 * Register standard Math functions that can be called from compiled JS code.
 * These are looked up by native_lookup_function during bytecode translation.
 */
static void native_register_math_functions(NativeCompiler *nc);  /* Forward decl */

/*
 * Register a native C function that can be called from compiled JS code.
 * Returns 0 on success, -1 on error (registry full).
 */
int native_register_function(NativeCompiler *nc, const char *name,
                              void *func_ptr, const NativeFuncSignature *sig) {
    if (!nc || !name || !func_ptr || !sig) return -1;
    
    if (nc->native_func_count >= NC_MAX_NATIVE_FUNCS) {
        return -1;  /* Registry full */
    }
    
    NativeFuncEntry *entry = &nc->native_funcs[nc->native_func_count++];
    entry->name = name;  /* Caller must ensure name stays valid */
    entry->func_ptr = func_ptr;
    entry->sig = *sig;
    
    return 0;
}

/*
 * Look up a registered native function by name.
 * Returns NULL if not found.
 */
NativeFuncEntry *native_lookup_function(NativeCompiler *nc, const char *name) {
    if (!nc || !name) return NULL;
    
    for (int i = 0; i < nc->native_func_count; i++) {
        if (strcmp(nc->native_funcs[i].name, name) == 0) {
            return &nc->native_funcs[i];
        }
    }
    return NULL;
}

int native_register_compiled_function(NativeCompiler *nc, void *code_ptr,
                                      const NativeFuncSignature *sig) {
    (void)nc;
    (void)code_ptr;
    (void)sig;
    /* Compiled functions are resolved via constant pool + _nativeHandle at compile time */
    return 0;
}

/*
 * Call a compiled native function.
 * Converts JS arguments and invokes the native code.
 */
/* Number arguments: the int and float32 values the interpreter produces are
   read inline, anything else takes the full conversion */
static inline int nc_arg_float(JSContext *ctx, float *f, JSValueConst v) {
    int tag = JS_VALUE_GET_TAG(v);
    if (tag == JS_CUSTOM_TAG_FLOAT32) {
        *f = JS_VALUE_GET_FLOAT32(v);
        return 0;
    }
    if (tag == JS_TAG_INT) {
        *f = (float)JS_VALUE_GET_INT(v);
        return 0;
    }
    return JS_ToFloat32(ctx, f, v);
}

static inline int nc_arg_int(JSContext *ctx, int32_t *i, JSValueConst v) {
    if (JS_VALUE_GET_TAG(v) == JS_TAG_INT) {
        *i = JS_VALUE_GET_INT(v);
        return 0;
    }
    return JS_ToInt32(ctx, i, v);
}

/* the general case: arrays, pointers, strings, 64-bit values */
static __attribute__((noinline)) JSValue native_func_call_general(JSContext *ctx, NativeFunc *func, int argc, JSValueConst *argv) {
    if (!func || !func->is_valid || !func->code_ptr) {
        return JS_ThrowTypeError(ctx, "Invalid native function");
    }
    
    const NativeFuncSignature *sig = &func->sig;
    
    /* Validate argument count */
    if (argc < sig->arg_count) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    
    /* Convert arguments based on signature */
    int32_t int_args[MAX_NATIVE_ARGS];
    float float_args[MAX_NATIVE_ARGS];
    void *ptr_args[MAX_NATIVE_ARGS];
    
    for (int i = 0; i < sig->arg_count; i++) {
        switch (sig->arg_types[i]) {
            case NATIVE_TYPE_INT32:
            case NATIVE_TYPE_UINT32:
            case NATIVE_TYPE_BOOL:
                if (nc_arg_int(ctx, &int_args[i], argv[i]) < 0) {
                    return JS_EXCEPTION;
                }
                break;
                
            case NATIVE_TYPE_INT64:
            case NATIVE_TYPE_UINT64: {
                /* For now, convert from int32 to int64
                 * TODO: Add proper BigInt support for full 64-bit values */
                int32_t val32;
                if (JS_ToInt32(ctx, &val32, argv[i]) < 0) {
                    return JS_EXCEPTION;
                }
                int_args[i] = val32;  /* Will be sign-extended by MIPS64 */
                break;
            }
                
            case NATIVE_TYPE_FLOAT32:
                /* straight to float: going through a double is software
                   emulated on the EE */
                if (nc_arg_float(ctx, &float_args[i], argv[i]) < 0) {
                    return JS_EXCEPTION;
                }
                break;
            
            case NATIVE_TYPE_INT32_ARRAY:
            case NATIVE_TYPE_UINT32_ARRAY:
            case NATIVE_TYPE_FLOAT32_ARRAY:
            case NATIVE_TYPE_DYNAMIC_INT32_ARRAY:
            case NATIVE_TYPE_DYNAMIC_UINT32_ARRAY:
            case NATIVE_TYPE_DYNAMIC_FLOAT32_ARRAY: {
                /* Typed arrays pass buffer pointer; dynamic arrays pass NativeDynamicArray* */
                if (NATIVE_TYPE_IS_DYNAMIC_ARRAY(sig->arg_types[i])) {
                    int32_t ptr_val;
                    if (JS_ToInt32(ctx, &ptr_val, argv[i]) == 0) {
                        ptr_args[i] = (void *)(intptr_t)ptr_val;
                    } else {
                        return JS_ThrowTypeError(ctx, "Invalid dynamic array argument at index %d", i);
                    }
                    break;
                }
                size_t offset, length, elem_size;
                JSValue buffer = JS_GetTypedArrayBuffer(ctx, argv[i], &offset, &length, &elem_size);
                if (JS_IsException(buffer)) {
                    return JS_EXCEPTION;
                }
                size_t buf_size;
                void *base_ptr = JS_GetArrayBuffer(ctx, &buf_size, buffer);
                if (!base_ptr) {
                    JS_FreeValue(ctx, buffer);
                    return JS_ThrowTypeError(ctx, "Invalid typed array");
                }
                ptr_args[i] = (uint8_t *)base_ptr + offset;
                JS_FreeValue(ctx, buffer);
                break;
            }
            
            case NATIVE_TYPE_PTR: {
                /* Check if it's a struct instance (has _data opaque property) */
                /* Try to get NativeStructInstance from object opaque */
                NativeStructInstance *inst = (NativeStructInstance *)JS_GetOpaque(argv[i], js_struct_class_id);
                if (inst && inst->data) {
                    ptr_args[i] = inst->data;
                } else {
                    /* Fallback: try to get as integer pointer value */
                    int32_t ptr_val;
                    if (JS_ToInt32(ctx, &ptr_val, argv[i]) == 0) {
                        ptr_args[i] = (void *)(intptr_t)ptr_val;
                    } else {
                        return JS_ThrowTypeError(ctx, "Invalid pointer argument at index %d", i);
                    }
                }
                break;
            }

            case NATIVE_TYPE_STRUCT_ARRAY: {
                /* StructType.array(N) result: a js_struct_instance_array_class_id
                 * object whose opaque is the NativeStructInstance for the whole
                 * contiguous block. Pass its base pointer, same as NATIVE_TYPE_PTR -
                 * the compiled function does its own base+index*size addressing
                 * (IR_ARRAY_ELEM_ADDR), it doesn't need the count from here (the
                 * caller passes that as a separate int arg, same convention as
                 * Float32Array+int). */
                NativeStructInstance *inst =
                    (NativeStructInstance *)JS_GetOpaque(argv[i], js_struct_instance_array_class_id);
                if (!inst || !inst->data) {
                    return JS_ThrowTypeError(ctx, "Expected a struct array (StructType.array(n)) at argument %d", i);
                }
                ptr_args[i] = inst->data;
                break;
            }

            case NATIVE_TYPE_STRING: {
                /* Convert JS string to NativeString* */
                const char *str = JS_ToCString(ctx, argv[i]);
                if (!str) {
                    return JS_ThrowTypeError(ctx, "Expected string at argument %d", i);
                }
                NativeString *native_str = native_string_from_cstr(str);
                JS_FreeCString(ctx, str);
                if (!native_str) {
                    return JS_ThrowOutOfMemory(ctx);
                }
                ptr_args[i] = native_str;
                break;
            }
            
            default:
                return JS_ThrowTypeError(ctx, "Unsupported argument type");
        }
    }
    
    /* Call the native function */
    /* This uses the MIPS N32 ABI calling convention:
     * - Arguments passed in $a0-$a7 (first 8 arguments)
     * - Return value in $v0/$v1
     * - Pointers are 32 bits (n32, not n64)
     * - Stack aligned to 16 bytes
     * - Saved registers ($s0-$s7) preserved by callee
     * - Temporary registers ($t0-$t9) are scratch (not preserved)
     */
    typedef int32_t (*native_func_8_t)(int32_t, int32_t, int32_t, int32_t, 
                                       int32_t, int32_t, int32_t, int32_t);
    native_func_8_t native_fn = (native_func_8_t)func->code_ptr;
    
    /* Call with up to 8 integer arguments */
    /* For floats, we need to pass the bitwise representation as int32_t */
    int32_t args[8] = {0};
    
    for (int i = 0; i < sig->arg_count && i < 8; i++) {
        if (NATIVE_TYPE_IS_ARRAY(sig->arg_types[i]) ||
            NATIVE_TYPE_IS_DYNAMIC_ARRAY(sig->arg_types[i]) ||
            sig->arg_types[i] == NATIVE_TYPE_PTR ||
            sig->arg_types[i] == NATIVE_TYPE_STRUCT_ARRAY ||
            sig->arg_types[i] == NATIVE_TYPE_STRING) {
            args[i] = (int32_t)(intptr_t)ptr_args[i];
        } else if (sig->arg_types[i] == NATIVE_TYPE_FLOAT32) {
            union { float f; int32_t v; } u;
            u.f = float_args[i];
            args[i] = u.v;
        } else {
            args[i] = int_args[i];
        }
    }
    
    /* Call the native function based on return type */
    if (sig->return_type == NATIVE_TYPE_FLOAT32) {
        /* Call and get float result from $f0 (proper MIPS ABI) */
        typedef float (*native_float_func_8_t)(int32_t, int32_t, int32_t, int32_t,
                                                int32_t, int32_t, int32_t, int32_t);
        native_float_func_8_t float_fn = (native_float_func_8_t)func->code_ptr;
        float result = float_fn(args[0], args[1], args[2], args[3],
                                 args[4], args[5], args[6], args[7]);
        return JS_NewFloat32(ctx, result);
    }
    
    /* For int64, the result is in $v0 as a 64-bit value */
    if (sig->return_type == NATIVE_TYPE_INT64 || sig->return_type == NATIVE_TYPE_UINT64) {
        typedef int64_t (*native_int64_func_8_t)(int32_t, int32_t, int32_t, int32_t,
                                                  int32_t, int32_t, int32_t, int32_t);
        native_int64_func_8_t int64_fn = (native_int64_func_8_t)func->code_ptr;
        int64_t result64 = int64_fn(args[0], args[1], args[2], args[3],
                                     args[4], args[5], args[6], args[7]);
        
        /* Returned as a Number: BigInt went away with CONFIG_BIGNUM. Exact up
         * to 2^53, lossy above that. The 64-bit multiply/divide/modulo this
         * compiler emits are already emulated and lossy on the R5900 anyway
         * (the CPU has no DMULT/DDIV), so 64-bit natives were never exact. */
        if (sig->return_type == NATIVE_TYPE_INT64) {
            return JS_NewFloat64(ctx, (double)result64);
        } else {
            return JS_NewFloat64(ctx, (double)(uint64_t)result64);
        }
    }
    
    int32_t result = native_fn(args[0], args[1], args[2], args[3],
                               args[4], args[5], args[6], args[7]);
    
    /* Convert result based on return type */
    switch (sig->return_type) {
        case NATIVE_TYPE_VOID:
            return JS_UNDEFINED;
        case NATIVE_TYPE_INT32:
            return JS_NewInt32(ctx, result);
        case NATIVE_TYPE_UINT32:
            return JS_NewUint32(ctx, (uint32_t)result);
        case NATIVE_TYPE_BOOL:
            return JS_NewBool(ctx, result != 0);
        case NATIVE_TYPE_STRING: {
            /* Convert NativeString* result to JS string */
            NativeString *native_str = (NativeString *)(intptr_t)result;
            if (native_str && native_str->data) {
                JSValue js_str = JS_NewStringLen(ctx, native_str->data, native_str->length);
                native_string_free(native_str);  /* Free the native string */
                return js_str;
            }
            return JS_NewString(ctx, "");
        }
        default:
            return JS_UNDEFINED;
    }
}

JSValue native_func_call(JSContext *ctx, NativeFunc *func, int argc, JSValueConst *argv) {
    const NativeFuncSignature *sig;
    if (!func || !func->is_valid || !func->code_ptr || argc < func->sig.arg_count)
        return native_func_call_general(ctx, func, argc, argv);  /* reports the error */
    sig = &func->sig;

    /* The common case (helpers like clamp, lerp, hit tests): only number
     * arguments and a number/void result. They go straight into the
     * argument registers (floats as their bit pattern), in one pass. */
    if (sig->return_type == NATIVE_TYPE_FLOAT32 || sig->return_type == NATIVE_TYPE_INT32 ||
        sig->return_type == NATIVE_TYPE_VOID || sig->return_type == NATIVE_TYPE_BOOL ||
        sig->return_type == NATIVE_TYPE_UINT32) {
        int32_t sargs[8];
        int n = sig->arg_count, i;
        for (i = 0; i < n; i++) {
            NativeType t = sig->arg_types[i];
            if (t == NATIVE_TYPE_FLOAT32) {
                union { float f; int32_t v; } u;
                if (nc_arg_float(ctx, &u.f, argv[i]) < 0)
                    return JS_EXCEPTION;
                sargs[i] = u.v;
            } else if (t == NATIVE_TYPE_INT32 || t == NATIVE_TYPE_UINT32 || t == NATIVE_TYPE_BOOL) {
                if (nc_arg_int(ctx, &sargs[i], argv[i]) < 0)
                    return JS_EXCEPTION;
            } else {
                break;
            }
        }
        if (i == n) {
            for (; i < 8; i++)
                sargs[i] = 0;
            if (sig->return_type == NATIVE_TYPE_FLOAT32) {
                typedef float (*nc_float_fn_t)(int32_t, int32_t, int32_t, int32_t,
                                               int32_t, int32_t, int32_t, int32_t);
                float r = ((nc_float_fn_t)func->code_ptr)(sargs[0], sargs[1], sargs[2], sargs[3],
                                                          sargs[4], sargs[5], sargs[6], sargs[7]);
                return JS_NewFloat32(ctx, r);
            } else {
                typedef int32_t (*nc_int_fn_t)(int32_t, int32_t, int32_t, int32_t,
                                               int32_t, int32_t, int32_t, int32_t);
                int32_t r = ((nc_int_fn_t)func->code_ptr)(sargs[0], sargs[1], sargs[2], sargs[3],
                                                          sargs[4], sargs[5], sargs[6], sargs[7]);
                switch (sig->return_type) {
                case NATIVE_TYPE_INT32: return JS_NewInt32(ctx, r);
                case NATIVE_TYPE_UINT32: return JS_NewUint32(ctx, (uint32_t)r);
                case NATIVE_TYPE_BOOL: return JS_NewBool(ctx, r != 0);
                default: return JS_UNDEFINED;
                }
            }
        }
    }
    return native_func_call_general(ctx, func, argc, argv);
}

