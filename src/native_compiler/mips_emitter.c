/*
 * AthenaEnv Native Compiler - MIPS R5900 Code Emitter
 * 
 * Copyright (c) 2025 AthenaEnv Project
 * 
 * Implementation of MIPS R5900 instruction emission for the PS2 EE.
 */

 #include "mips_emitter.h"
 #include <stdlib.h>
 #include <string.h>
 #include <stdio.h>
 
 #ifdef PS2
 #include <kernel.h>
 #endif
 
 /* ============================================
  * MIPS Instruction Encoding
  * ============================================ */
 
 /* Opcode fields */
 #define OP_SPECIAL  0x00
 #define OP_REGIMM   0x01
 #define OP_J        0x02
 #define OP_JAL      0x03
 #define OP_BEQ      0x04
 #define OP_BNE      0x05
 #define OP_BLEZ     0x06
 #define OP_BGTZ     0x07
 #define OP_ADDI     0x08
 #define OP_ADDIU    0x09
 #define OP_SLTI     0x0A
 #define OP_SLTIU    0x0B
 #define OP_ANDI     0x0C
 #define OP_ORI      0x0D
 #define OP_XORI     0x0E
 #define OP_LUI      0x0F
 #define OP_COP1     0x11
 #define OP_LB       0x20
 #define OP_LH       0x21
 #define OP_LW       0x23
 #define OP_LBU      0x24
 #define OP_LHU      0x25
#define OP_SB       0x28
#define OP_SH       0x29
#define OP_SWL      0x2A
#define OP_SW       0x2B
#define OP_SWR      0x2E
#define OP_LWC1     0x31
#define OP_SWC1     0x39
 
 /* SPECIAL function codes */
 #define FN_SLL      0x00
 #define FN_SRL      0x02
 #define FN_SRA      0x03
 #define FN_SLLV     0x04
 #define FN_SRLV     0x06
 #define FN_SRAV     0x07
 #define FN_JR       0x08
 #define FN_JALR     0x09
 #define FN_MFHI     0x10
 #define FN_MFLO     0x12
 #define FN_MULT     0x18
 #define FN_MULTU    0x19
 #define FN_DIV      0x1A
 #define FN_DIVU     0x1B
 #define FN_ADD      0x20
 #define FN_ADDU     0x21
 #define FN_SUB      0x22
 #define FN_SUBU     0x23
 #define FN_AND      0x24
 #define FN_OR       0x25
 #define FN_XOR      0x26
 #define FN_NOR      0x27
 #define FN_SLT      0x2A
 #define FN_SLTU     0x2B
 
 /* MIPS64 SPECIAL function codes */
 #define FN_DSLLV    0x14
 #define FN_DSRLV    0x16
 #define FN_DSRAV    0x17
 /* Note: DMULT/DDIV not available on R5900 - use MMI PMULTW or software emulation */
 #define FN_DADD     0x2C
 #define FN_DADDU    0x2D
 #define FN_DSUB     0x2E
 #define FN_DSUBU    0x2F
 #define FN_DSLL     0x38
 #define FN_DSRL     0x3A
 #define FN_DSRA     0x3B
 #define FN_DSLL32   0x3C
 #define FN_DSRL32   0x3E
 #define FN_DSRA32   0x3F
 
 /* MIPS64 I-type opcodes */
 #define OP_DADDI    0x18
 #define OP_DADDIU   0x19
 #define OP_LD       0x37
 #define OP_SD       0x3F
 
 /* REGIMM rt codes */
 #define RT_BLTZ     0x00
 #define RT_BGEZ     0x01
 
 /* COP1 fmt codes */
 #define FMT_MFC1    0x00
 #define FMT_CFC1    0x02
 #define FMT_MTC1    0x04
 #define FMT_CTC1    0x06
 #define FMT_BC1     0x08
 #define FMT_S       0x10
 #define FMT_W       0x14
 
 /* COP1 function codes (for FMT_S) */
 #define FN_ADD_S    0x00
 #define FN_SUB_S    0x01
 #define FN_MUL_S    0x02
 #define FN_DIV_S    0x03
 #define FN_SQRT_S   0x04
 #define FN_ABS_S    0x05
 #define FN_MOV_S    0x06
 #define FN_NEG_S    0x07
 #define FN_CVT_W_S  0x24
#define FN_MAX_S    0x28  /* R5900 specific: MAX.S */
#define FN_MIN_S    0x29  /* R5900 specific: MIN.S */
#define FN_C_EQ_S   0x32
#define FN_C_LT_S   0x34  /* Fixed: was 0x3C */
#define FN_C_LE_S   0x36  /* Fixed: was 0x3E */
 
 /* COP1 function codes (for FMT_W) */
 #define FN_CVT_S_W  0x20
 
 /* BC1 codes */
 #define BC1F_CODE   0x00
 #define BC1T_CODE   0x01
 
 /* Fixup types */
 #define FIXUP_BRANCH    0   /* 16-bit relative branch */
 #define FIXUP_JUMP      1   /* 26-bit absolute jump */
 
 /* ============================================
  * Instruction Builders
  * ============================================ */
 
 /* R-type: opcode(6) | rs(5) | rt(5) | rd(5) | sa(5) | function(6) */
 static inline uint32_t encode_r_type(int op, int rs, int rt, int rd, int sa, int fn) {
     return ((op & 0x3F) << 26) | ((rs & 0x1F) << 21) | ((rt & 0x1F) << 16) |
            ((rd & 0x1F) << 11) | ((sa & 0x1F) << 6) | (fn & 0x3F);
 }
 
 /* I-type: opcode(6) | rs(5) | rt(5) | immediate(16) */
 static inline uint32_t encode_i_type(int op, int rs, int rt, int16_t imm) {
     return ((op & 0x3F) << 26) | ((rs & 0x1F) << 21) | ((rt & 0x1F) << 16) |
            ((uint16_t)imm & 0xFFFF);
 }
 
 /* J-type: opcode(6) | target(26) */
 static inline uint32_t encode_j_type(int op, uint32_t target) {
     return ((op & 0x3F) << 26) | (target & 0x03FFFFFF);
 }
 
 /* COP1 R-type: opcode(6) | fmt(5) | ft(5) | fs(5) | fd(5) | function(6) */
 static inline uint32_t encode_cop1_r(int fmt, int ft, int fs, int fd, int fn) {
     return ((OP_COP1) << 26) | ((fmt & 0x1F) << 21) | ((ft & 0x1F) << 16) |
            ((fs & 0x1F) << 11) | ((fd & 0x1F) << 6) | (fn & 0x3F);
 }
 
 /* COP1 branch */
 static inline uint32_t encode_cop1_bc(int nd, int tf, int16_t offset) {
     return ((OP_COP1) << 26) | ((FMT_BC1) << 21) | ((nd & 1) << 17) | 
            ((tf & 1) << 16) | ((uint16_t)offset & 0xFFFF);
 }
 
 /* ============================================
  * Emitter Lifecycle
  * ============================================ */
 
 int mips_emitter_init(MipsEmitter *em) {
     if (!em) return -1;
     
     memset(em, 0, sizeof(MipsEmitter));
     
     /* Allocate code buffer - aligned for cache operations */
     size_t buf_size = MIPS_CODE_BUFFER_SIZE * sizeof(uint32_t);
 #ifdef PS2
     em->buffer.code = (uint32_t *)memalign(64, buf_size);
 #else
     em->buffer.code = (uint32_t *)malloc(buf_size);
 #endif
     
     if (!em->buffer.code) {
         em->has_error = true;
         em->error_msg = "Failed to allocate code buffer";
         return -1;
     }
     
     em->buffer.capacity = MIPS_CODE_BUFFER_SIZE;
    em->buffer.count = 0;
    em->buffer.function_start = 0;
     
     /* Initialize labels */
     for (int i = 0; i < MAX_LABELS; i++) {
         em->labels[i].offset = -1;
         em->labels[i].resolved = false;
     }
     
     /* Reserve registers that shouldn't be allocated */
     /* $zero, $at, $k0, $k1, $gp, $sp, $fp, $ra */
     em->gpr_used = (1 << REG_ZERO) | (1 << REG_AT) | (1 << REG_K0) | 
                    (1 << REG_K1) | (1 << REG_GP) | (1 << REG_SP) | 
                    (1 << REG_FP) | (1 << REG_RA);
     
     em->fpr_used = 0;
     em->stack_offset = 0;
     
     return 0;
 }
 
 void mips_emitter_free(MipsEmitter *em) {
     if (em && em->buffer.code) {
         free(em->buffer.code);
         em->buffer.code = NULL;
     }
 }
 
void mips_emitter_reset(MipsEmitter *em) {
    if (!em) return;
    
    /* ARENA MODE: Don't reset count - keep previous code.
     * Instead, record where the new function starts. */
    em->buffer.function_start = em->buffer.count;
    em->fixup_count = 0;
    
    /* Reset all labels to unused state */
    for (int i = 0; i < MAX_LABELS; i++) {
        em->labels[i].offset = -1;  /* -1 = unused */
        em->labels[i].resolved = false;
    }
     
     em->gpr_used = (1 << REG_ZERO) | (1 << REG_AT) | (1 << REG_K0) | 
                    (1 << REG_K1) | (1 << REG_GP) | (1 << REG_SP) | 
                    (1 << REG_FP) | (1 << REG_RA);
     em->fpr_used = 0;
     em->stack_offset = 0;
     em->has_error = false;
     em->error_msg = NULL;
 }
 
 void *mips_emitter_get_code(MipsEmitter *em, size_t *out_size) {
     if (!em || !em->buffer.code) return NULL;
     
     if (out_size) {
         /* Return size of current function only */
         *out_size = (em->buffer.count - em->buffer.function_start) * sizeof(uint32_t);
     }
     
     /* Return pointer to start of current function, not entire buffer */
     return em->buffer.code + em->buffer.function_start;
 }
 
 /*
 * Peephole Optimizer
 * 
 * Performs local optimizations on the generated code.
 * Safe to run before label resolution (replaces with NOPs, doesn't remove).
 */

/* Helper: Check if instruction is a NOP */
static inline bool is_nop(uint32_t instr) {
    return instr == 0 || instr == 0x00000000;
}

/* Helper: Check if instruction is a branch/jump (has delay slot) */
static inline bool is_branch_or_jump(uint32_t instr) {
    uint32_t opcode = (instr >> 26) & 0x3F;
    uint32_t funct = instr & 0x3F;
    
    /* J, JAL */
    if (opcode == 0x02 || opcode == 0x03) return true;
    /* BEQ, BNE, BLEZ, BGTZ */
    if (opcode >= 0x04 && opcode <= 0x07) return true;
    /* REGIMM (BLTZ, BGEZ, etc) */
    if (opcode == 0x01) return true;
    /* JR, JALR */
    if (opcode == 0x00 && (funct == 0x08 || funct == 0x09)) return true;
    
    return false;
}

/* Helper: Extract register fields from R-type instruction */
static inline void decode_r_type(uint32_t instr, uint32_t *rs, uint32_t *rt, uint32_t *rd, uint32_t *funct) {
    if (rs) *rs = (instr >> 21) & 0x1F;
    if (rt) *rt = (instr >> 16) & 0x1F;
    if (rd) *rd = (instr >> 11) & 0x1F;
    if (funct) *funct = instr & 0x3F;
}

/* Helper: Check if ADDU rd, rs, $zero (move pattern) */
static inline bool is_move_addu(uint32_t instr, uint32_t *dst, uint32_t *src) {
    uint32_t opcode = (instr >> 26) & 0x3F;
    uint32_t rs, rt, rd, funct;
    decode_r_type(instr, &rs, &rt, &rd, &funct);
    
    if (opcode == 0 && funct == 0x21 && rt == 0) {
        if (dst) *dst = rd;
        if (src) *src = rs;
        return true;
    }
    return false;
}

/* Helper: Check if instruction is safe to move into branch delay slot */
static inline bool is_safe_for_branch_delay_slot(uint32_t instr, uint32_t branch_rs, uint32_t branch_rt) {
    if (instr == 0) return false;  /* Already NOP */
    
    uint32_t op = (instr >> 26) & 0x3F;
    
    /* R-type instructions (opcode 0x00) */
    if (op == 0x00) {
        uint32_t funct = instr & 0x3F;
        uint32_t rs = (instr >> 21) & 0x1F;
        uint32_t rt = (instr >> 16) & 0x1F;
        uint32_t rd = (instr >> 11) & 0x1F;
        
        /* Don't move jumps/branches */
        if (funct == 0x08 || funct == 0x09) return false;  /* JR, JALR */
        
        /* Don't move if it writes to branch condition registers */
        if (rd == branch_rs || rd == branch_rt) return false;
        
        /* Safe R-type ALU operation */
        return true;
    }
    
    /* I-type ALU instructions */
    if (op == 0x09 || op == 0x0C || op == 0x0D || op == 0x0E || op == 0x0A || op == 0x0B) {
        /* ADDIU, ANDI, ORI, XORI, SLTI, SLTIU */
        uint32_t rt = (instr >> 16) & 0x1F;
        
        /* Don't move if it writes to branch condition registers */
        if (rt == branch_rs || rt == branch_rt) return false;
        
        return true;
    }
    
    /* Conservative: don't move loads, stores, or unknown instructions */
    return false;
}

/* Helper: Decode COP1 R-type instruction (FPU operations) */
static inline void decode_cop1_r(uint32_t instr, uint32_t *fmt, uint32_t *ft, 
                                  uint32_t *fs, uint32_t *fd, uint32_t *funct) {
    if (fmt) *fmt = (instr >> 21) & 0x1F;
    if (ft) *ft = (instr >> 16) & 0x1F;
    if (fs) *fs = (instr >> 11) & 0x1F;
    if (fd) *fd = (instr >> 6) & 0x1F;
    if (funct) *funct = instr & 0x3F;
}

/* Helper: Check if instruction is COP1 FMT.S (single-precision) */
static inline bool is_cop1_fmt_s(uint32_t instr) {
    uint32_t opcode = (instr >> 26) & 0x3F;
    uint32_t fmt = (instr >> 21) & 0x1F;
    return (opcode == OP_COP1) && (fmt == FMT_S);
}

/* Helper: Check if MOV.S fd, fs */
static inline bool is_mov_s(uint32_t instr, uint32_t *fd, uint32_t *fs) {
    if (!is_cop1_fmt_s(instr)) return false;
    uint32_t funct = instr & 0x3F;
    if (funct == FN_MOV_S) {
        if (fd) *fd = (instr >> 6) & 0x1F;
        if (fs) *fs = (instr >> 11) & 0x1F;
        return true;
    }
    return false;
}

/* Helper: Check if MUL.S fd, fs, ft */
static inline bool is_mul_s(uint32_t instr, uint32_t *fd, uint32_t *fs, uint32_t *ft) {
    if (!is_cop1_fmt_s(instr)) return false;
    uint32_t funct = instr & 0x3F;
    if (funct == FN_MUL_S) {
        if (fd) *fd = (instr >> 6) & 0x1F;
        if (fs) *fs = (instr >> 11) & 0x1F;
        if (ft) *ft = (instr >> 16) & 0x1F;
        return true;
    }
    return false;
}

/* Helper: Check if ADD.S fd, fs, ft */
static inline bool is_add_s(uint32_t instr, uint32_t *fd, uint32_t *fs, uint32_t *ft) {
    if (!is_cop1_fmt_s(instr)) return false;
    uint32_t funct = instr & 0x3F;
    if (funct == FN_ADD_S) {
        if (fd) *fd = (instr >> 6) & 0x1F;
        if (fs) *fs = (instr >> 11) & 0x1F;
        if (ft) *ft = (instr >> 16) & 0x1F;
        return true;
    }
    return false;
}

/* MADD.S encoding for R5900: opcode=COP1, fmt=S, ft, fs, fd, funct=0x1C */
#define FN_MADD_S  0x1C

int mips_emitter_peephole_optimize(MipsEmitter *em) {

    if (!em || !em->buffer.code) return -1;
    
    uint32_t *code = em->buffer.code;
    size_t count = em->buffer.count;
    size_t start = em->buffer.function_start;
    
    int optimized_count = 0;
    int pass = 0;
    bool changed = true;
    
    /* Run multiple passes until no more optimizations */
    while (changed && pass < 5) {
        changed = false;
        pass++;
        
        for (size_t i = start; i < count; i++) {
            uint32_t instr = code[i];
            
            /* Skip NOPs for single-instruction patterns */
            if (is_nop(instr)) continue;
            
            /* Pattern 1: Redundant move to self
             * ADDU $rx, $rx, $zero -> NOP
             */
            uint32_t dst, src;
            if (is_move_addu(instr, &dst, &src) && dst == src) {
                code[i] = 0;
                optimized_count++;
                changed = true;
                continue;
            }
            
            /* Two-instruction patterns */
            if (i + 1 < count) {
                uint32_t instr2 = code[i + 1];
                
                /* Pattern 2: Consecutive NOPs (not in delay slot) -> keep one
                 * NOP; NOP -> NOP (second already NOP, mark first as checked)
                 */
                
                /* Pattern 3: MFLO/MFHI $tx + ADDU $rd, $tx, $zero -> MFLO/MFHI $rd
                 * Fuse multiply/divide result directly to target register
                 */
                uint32_t move_dst, move_src;
                if (is_move_addu(instr2, &move_dst, &move_src)) {
                    uint32_t rs1, rt1, rd1, funct1;
                    decode_r_type(instr, &rs1, &rt1, &rd1, &funct1);
                    uint32_t opcode1 = (instr >> 26) & 0x3F;
                    
                    /* MFLO: opcode=0, funct=0x12 */
                    /* MFHI: opcode=0, funct=0x10 */
                    if (opcode1 == 0 && (funct1 == 0x12 || funct1 == 0x10)) {
                        if (rd1 == move_src) {
                            /* Rewrite MFLO/MFHI to target move_dst directly */
                            code[i] = (instr & 0xFFFF07FF) | (move_dst << 11);
                            code[i + 1] = 0;  /* NOP */
                            optimized_count++;
                            changed = true;
                            i++;
                            continue;
                        }
                    }
                    
                    /* Pattern 4: Any ALU op to $tx + ADDU $rd, $tx, $zero
                     * If $tx is only used here, redirect ALU to $rd
                     * (For now, only safe for specific patterns)
                     */
                    
                    /* ADDU/SUBU $tx, $ra, $rb + ADDU $rd, $tx, $zero */
                    if (opcode1 == 0 && (funct1 == 0x21 || funct1 == 0x23)) {
                        if (rd1 == move_src && rd1 != move_dst) {
                            /* Rewrite ALU op to target move_dst directly */
                            code[i] = (instr & 0xFFFF07FF) | (move_dst << 11);
                            code[i + 1] = 0;
                            optimized_count++;
                            changed = true;
                            i++;
                            continue;
                        }
                    }
                    
                    /* Pattern 5: AND/OR/XOR $tx, $ra, $rb + ADDU $rd, $tx, $zero */
                    if (opcode1 == 0 && (funct1 == 0x24 || funct1 == 0x25 || funct1 == 0x26)) {
                        if (rd1 == move_src && rd1 != move_dst) {
                            code[i] = (instr & 0xFFFF07FF) | (move_dst << 11);
                            code[i + 1] = 0;
                            optimized_count++;
                            changed = true;
                            i++;
                            continue;
                        }
                    }
                    
                    /* Pattern 6: SLLV/SRLV/SRAV $tx, ... + ADDU $rd, $tx, $zero */
                    if (opcode1 == 0 && (funct1 == 0x04 || funct1 == 0x06 || funct1 == 0x07)) {
                        if (rd1 == move_src && rd1 != move_dst) {
                            code[i] = (instr & 0xFFFF07FF) | (move_dst << 11);
                            code[i + 1] = 0;
                            optimized_count++;
                            changed = true;
                            i++;
                            continue;
                        }
                    }
                }
                
                /* Get opcode for I-type patterns */
                uint32_t op1 = (instr >> 26) & 0x3F;
                uint32_t op2 = (instr2 >> 26) & 0x3F;
                
                /* Pattern 7a: ADDIU $tx, $rs, imm + ADDU $rd, $tx, $zero
                 * ADDIU is I-type (opcode 0x09), so needs different handling
                 * CRITICAL: Only apply if instr2 is actually an ADDU move!
                 */
                if (op1 == 0x09 && is_move_addu(instr2, &move_dst, &move_src)) {  /* ADDIU */
                    uint32_t tx = (instr >> 16) & 0x1F;  /* rt field for I-type */
                    if (tx == move_src && tx != move_dst) {
                        /* Rewrite ADDIU to target move_dst directly */
                        code[i] = (instr & 0xFFE0FFFF) | (move_dst << 16);
                        code[i + 1] = 0;
                        optimized_count++;
                        changed = true;
                        i++;
                        continue;
                    }
                }
                
                /* Pattern 7b: LUI $tx, imm + ADDU $rd, $tx, $zero 
                 * CRITICAL: Only apply if instr2 is actually an ADDU move! */
                if (op1 == 0x0F && is_move_addu(instr2, &move_dst, &move_src)) {  /* LUI */
                    uint32_t tx = (instr >> 16) & 0x1F;
                    if (tx == move_src && tx != move_dst) {
                        code[i] = (instr & 0xFFE0FFFF) | (move_dst << 16);
                        code[i + 1] = 0;
                        optimized_count++;
                        changed = true;
                        i++;
                        continue;
                    }
                }
                
                /* Pattern 8: Store then load same location -> eliminate load
                 * SW $rx, off($sp); LW $rx, off($sp) -> SW only
                 */
                uint32_t op1_sw = (instr >> 26) & 0x3F;
                uint32_t op2_lw = (instr2 >> 26) & 0x3F;
                if (op1_sw == 0x2B && op2_lw == 0x23) {  /* SW and LW */
                    /* Check if same register and same offset/base */
                    if ((instr & 0x03FFFFFF) == (instr2 & 0x03FFFFFF)) {
                        uint32_t rt1 = (instr >> 16) & 0x1F;
                        uint32_t rt2 = (instr2 >> 16) & 0x1F;
                        if (rt1 == rt2) {
                            code[i + 1] = 0;  /* Remove redundant load */
                            optimized_count++;
                            changed = true;
                            i++;
                            continue;
                        }
                    }
                }
                
                /* Pattern 9: NOP + J -> move NOP into J's delay slot if possible
                 * NOP; J target -> J target; NOP (swap)
                 * This doesn't save instructions but makes it cleaner
                 */
                
                /* Pattern 10: NOP + non-branch instruction -> eliminate NOP
                 * We can eliminate NOPs that aren't in delay slots
                 * Check if previous instruction is a branch/jump
                 */
                
                /* Pattern 11: Safe instruction + BEQ/BNE + NOP -> BEQ/BNE + Safe instruction
                 * Fill branch delay slots with useful instructions */
                if (i >= start + 1 && i + 1 < count) {
                    uint32_t branch_instr = code[i];
                    uint32_t delay_slot = code[i + 1];
                    
                    /* Check if delay_slot is NOP */
                    if (delay_slot == 0) {
                        uint32_t op = (branch_instr >> 26) & 0x3F;
                        
                        /* Check if current is BEQ (0x04) or BNE (0x05) */
                        if (op == 0x04 || op == 0x05) {
                            /* Extract branch condition registers */
                            uint32_t rs = (branch_instr >> 21) & 0x1F;
                            uint32_t rt = (branch_instr >> 16) & 0x1F;
                            
                            /* Look at previous instruction */
                            uint32_t prev_instr = code[i - 1];
                            
                            /* Check if previous instruction is safe to move */
                            if (is_safe_for_branch_delay_slot(prev_instr, rs, rt)) {
                                /* Move previous instruction into delay slot */
                                code[i - 1] = 0;  /* Replace with NOP */
                                code[i + 1] = prev_instr;  /* Move to delay slot */
                                optimized_count++;
                                changed = true;
                                i++;  /* Skip the delay slot we just filled */
                                continue;
                            }
                        }
                    }
                }
                
                /* ============================================
                 * FPU Peephole Patterns
                 * ============================================ */
                
                /* Pattern 12: MOV.S $fx, $fx -> NOP (redundant self-move) */
                uint32_t mov_fd, mov_fs;
                if (is_mov_s(instr, &mov_fd, &mov_fs) && mov_fd == mov_fs) {
                    code[i] = 0;  /* NOP */
                    optimized_count++;
                    changed = true;
                    continue;
                }
                
                /* Pattern 13: MUL.S $ft, $fs1, $fs2 + ADD.S $fd, $ft, $fs3 -> MADD.S $fd, $fs3, $fs1, $fs2
                 * Fused multiply-add: MADD.S fd, fs, ft <=> fd = ACC + (fs * ft)
                 * R5900 MADD.S uses accumulator, so this is approximate
                 * For now, just detect the pattern for potential future optimization
                 */
                uint32_t mul_fd, mul_fs, mul_ft;
                uint32_t add_fd, add_fs, add_ft;
                if (is_mul_s(instr, &mul_fd, &mul_fs, &mul_ft) && 
                    is_add_s(instr2, &add_fd, &add_fs, &add_ft)) {
                    /* Check if ADD uses the MUL result as one operand */
                    if (add_fs == mul_fd || add_ft == mul_fd) {
                        /* This is a candidate for MADD.S - log for now
                         * Full implementation requires MULA.S + MADD.S sequence
                         * which uses the FPU accumulator register
                         */
                        /* optimized_count++; // Not actually optimizing yet */
                    }
                }
                
                /* Pattern 14: SWC1 $fx, off($sp) + LWC1 $fx, off($sp) -> SWC1 only
                 * Redundant FPU load after store to same location
                 */
                if (op1 == OP_SWC1 && op2 == OP_LWC1) {
                    /* Check if same register, offset, and base */
                    if ((instr & 0x03FFFFFF) == (instr2 & 0x03FFFFFF)) {
                        code[i + 1] = 0;  /* Remove redundant load */
                        optimized_count++;
                        changed = true;
                        i++;
                        continue;
                    }
                }
                
                /* Pattern 15: MTC1 $rx, $fx + MFC1 $rx, $fx -> MTC1 only
                 * Redundant move from coprocessor after move to
                 */
                if (op1 == OP_COP1 && op2 == OP_COP1) {
                    uint32_t fmt1 = (instr >> 21) & 0x1F;
                    uint32_t fmt2 = (instr2 >> 21) & 0x1F;
                    if (fmt1 == FMT_MTC1 && fmt2 == FMT_MFC1) {
                        uint32_t rt1 = (instr >> 16) & 0x1F;
                        uint32_t rt2 = (instr2 >> 16) & 0x1F;
                        uint32_t fs1 = (instr >> 11) & 0x1F;
                        uint32_t fs2 = (instr2 >> 11) & 0x1F;
                        if (rt1 == rt2 && fs1 == fs2) {
                            code[i + 1] = 0;  /* Remove redundant MFC1 */
                            optimized_count++;
                            changed = true;
                            i++;
                            continue;
                        }
                    }
                }
            }
        }
    }
    
    return optimized_count;
}

int mips_emitter_finalize(MipsEmitter *em) {
     if (!em || em->has_error) return -1;
     
     /* Resolve all fixups */
     for (int i = 0; i < em->fixup_count; i++) {
         MipsFixup *fix = &em->fixups[i];
         MipsLabel *label = &em->labels[fix->label_id];
         
         if (!label->resolved) {
             em->has_error = true;
             em->error_msg = "Unresolved label";
             printf("ERROR: Unresolved label ID %d (fixup at offset %zu)\n", fix->label_id, fix->instr_offset);
             return -1;
         }
         
         uint32_t *instr = &em->buffer.code[fix->instr_offset];
         
         if (fix->fixup_type == FIXUP_BRANCH) {
             /* Calculate relative offset (in instructions, from delay slot) */
             int32_t offset = label->offset - (int32_t)fix->instr_offset - 1;
             if (offset < -32768 || offset > 32767) {
                 em->has_error = true;
                 em->error_msg = "Branch target out of range";
                 return -1;
             }
             /* Patch the immediate field */
             *instr = (*instr & 0xFFFF0000) | ((uint16_t)offset & 0xFFFF);
        } else if (fix->fixup_type == FIXUP_JUMP) {
            /* Calculate absolute target (26-bit word address) */
            /* For J instruction, target is word address (instruction offset / 4) */
            /* The code is executed from the buffer, so use buffer address */
            void *code_base = em->buffer.code;
            /* label->offset is in instructions, convert to bytes then to absolute address */
            /* Note: label->offset is relative to the start of emitted code (buffer.count=0) */
            /* Calculate target address: code_base + label_offset_in_bytes */
            uint32_t target_addr = (uint32_t)(uintptr_t)code_base + (label->offset * sizeof(uint32_t));
            /* J instruction uses word address (divide by 4) */
            uint32_t target = (target_addr >> 2) & 0x03FFFFFF;
            
            *instr = (*instr & 0xFC000000) | target;
        }
     }
     
     /* NOTE: J redundancy elimination disabled due to memory corruption bug.
      * The issue was that writing 0x00000000 to code[i] only cleared
      * the opcode field (upper byte) instead of the entire instruction.
      * This needs deeper investigation - possibly a cache coherency issue
      * or the optimization should be done at IR level instead.
      */
     
     /* Flush instruction cache */
 #ifdef PS2
     FlushCache(0);  /* Flush data cache */
     FlushCache(2);  /* Invalidate instruction cache */
 #endif
     
     return 0;
 }
 
 /* ============================================
  * Label Management
  * ============================================ */
 
int mips_label_create(MipsEmitter *em) {
    if (!em) return -1;
    
    for (int i = 0; i < MAX_LABELS; i++) {
        /* Find a label that is not in use (offset == -1 means unused) */
        if (em->labels[i].offset == -1 && !em->labels[i].resolved) {
            /* Mark as in use (offset = -2 means created but not yet bound) */
            em->labels[i].offset = -2;
            em->labels[i].resolved = false;
            return i;
        }
    }
    
    em->has_error = true;
    em->error_msg = "Too many labels";
    return -1;
}
 
void mips_label_bind(MipsEmitter *em, int label_id) {
    if (!em || label_id < 0 || label_id >= MAX_LABELS) return;
    
    /* Bind label to current code offset */
    em->labels[label_id].offset = (int32_t)em->buffer.count;
    em->labels[label_id].resolved = true;
}
 
 /* ============================================
  * Register Allocation
  * ============================================ */
 
 MipsReg mips_alloc_gpr(MipsEmitter *em) {
     if (!em) return REG_ZERO;
     
     /* Try to allocate from temporaries first ($t0-$t9) */
     for (int r = REG_T0; r <= REG_T7; r++) {
         if (!(em->gpr_used & (1 << r))) {
             em->gpr_used |= (1 << r);
             return (MipsReg)r;
         }
     }
     for (int r = REG_T8; r <= REG_T9; r++) {
         if (!(em->gpr_used & (1 << r))) {
             em->gpr_used |= (1 << r);
             return (MipsReg)r;
         }
     }
     
     /* Try saved registers ($s0-$s7) */
     for (int r = REG_S0; r <= REG_S7; r++) {
         if (!(em->gpr_used & (1 << r))) {
             em->gpr_used |= (1 << r);
             return (MipsReg)r;
         }
     }
     
     em->has_error = true;
     em->error_msg = "Out of GPR registers";
     return REG_ZERO;
 }
 
 MipsFpuReg mips_alloc_fpr(MipsEmitter *em) {
     if (!em) return FPU_F0;
     
     /* Allocate from $f0-$f31 (skip $f12-$f15 which are argument registers) */
     for (int r = FPU_F0; r < FPU_F12; r++) {
         if (!(em->fpr_used & (1 << r))) {
             em->fpr_used |= (1 << r);
             return (MipsFpuReg)r;
         }
     }
     for (int r = FPU_F16; r < FPU_REG_COUNT; r++) {
         if (!(em->fpr_used & (1 << r))) {
             em->fpr_used |= (1 << r);
             return (MipsFpuReg)r;
         }
     }
     
     em->has_error = true;
     em->error_msg = "Out of FPU registers";
     return FPU_F0;
 }
 
 void mips_free_gpr(MipsEmitter *em, MipsReg reg) {
     if (!em || reg == REG_ZERO) return;
     em->gpr_used &= ~(1 << reg);
 }
 
 void mips_free_fpr(MipsEmitter *em, MipsFpuReg reg) {
     if (!em) return;
     em->fpr_used &= ~(1 << reg);
 }
 
 /* ============================================
  * Instruction Emission Helpers
  * ============================================ */
 
 void mips_emit_raw(MipsEmitter *em, uint32_t instr) {
     if (!em || em->has_error) return;
     
     if (em->buffer.count >= em->buffer.capacity) {
         em->has_error = true;
         em->error_msg = "Code buffer overflow";
         return;
     }
     
     em->buffer.code[em->buffer.count++] = instr;
 }
 
 size_t mips_get_offset(MipsEmitter *em) {
     return em ? em->buffer.count : 0;
 }
 
 static void add_fixup(MipsEmitter *em, int label_id, int type) {
     if (!em || em->fixup_count >= MAX_FIXUPS) {
         if (em) {
             em->has_error = true;
             em->error_msg = "Too many fixups";
         }
         return;
     }
     
     MipsFixup *fix = &em->fixups[em->fixup_count++];
     fix->instr_offset = em->buffer.count;
     fix->label_id = label_id;
     fix->fixup_type = type;
 }
 
 /* ============================================
  * R-Type Instructions
  * ============================================ */
 
 void mips_add(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_ADD));
 }
 
 void mips_addu(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_ADDU));
 }
 
 void mips_sub(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_SUB));
 }
 
 void mips_subu(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_SUBU));
 }
 
 void mips_and(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_AND));
 }
 
 void mips_or(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_OR));
 }
 
 void mips_xor(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_XOR));
 }
 
 void mips_nor(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_NOR));
 }
 
 void mips_slt(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_SLT));
 }
 
 void mips_sltu(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_SLTU));
 }
 
 void mips_sll(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, 0, rt, rd, sa & 0x1F, FN_SLL));
 }
 
 void mips_srl(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, 0, rt, rd, sa & 0x1F, FN_SRL));
 }
 
 void mips_sra(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, 0, rt, rd, sa & 0x1F, FN_SRA));
 }
 
 void mips_sllv(MipsEmitter *em, MipsReg rd, MipsReg rt, MipsReg rs) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_SLLV));
 }
 
 void mips_srlv(MipsEmitter *em, MipsReg rd, MipsReg rt, MipsReg rs) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_SRLV));
 }
 
 void mips_srav(MipsEmitter *em, MipsReg rd, MipsReg rt, MipsReg rs) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_SRAV));
 }
 
 void mips_mult(MipsEmitter *em, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, 0, 0, FN_MULT));
 }
 
 void mips_multu(MipsEmitter *em, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, 0, 0, FN_MULTU));
 }
 
 void mips_div(MipsEmitter *em, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, 0, 0, FN_DIV));
 }
 
 void mips_divu(MipsEmitter *em, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, 0, 0, FN_DIVU));
 }
 
 void mips_mfhi(MipsEmitter *em, MipsReg rd) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, 0, 0, rd, 0, FN_MFHI));
 }
 
 void mips_mflo(MipsEmitter *em, MipsReg rd) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, 0, 0, rd, 0, FN_MFLO));
 }
 
 void mips_jr(MipsEmitter *em, MipsReg rs) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, 0, 0, 0, FN_JR));
 }
 
 void mips_jalr(MipsEmitter *em, MipsReg rd, MipsReg rs) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, 0, rd, 0, FN_JALR));
 }
 
 /* ============================================
  * MIPS64 Instructions (R5900)
  * ============================================ */
 
 void mips_dadd(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_DADD));
 }
 
 void mips_daddu(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_DADDU));
 }
 
 void mips_dsub(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_DSUB));
 }
 
 void mips_dsubu(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_DSUBU));
 }
 
 void mips_dsll(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, 0, rt, rd, sa & 0x1F, FN_DSLL));
 }
 
 void mips_dsrl(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, 0, rt, rd, sa & 0x1F, FN_DSRL));
 }
 
 void mips_dsra(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, 0, rt, rd, sa & 0x1F, FN_DSRA));
 }
 
 void mips_dsll32(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, 0, rt, rd, sa & 0x1F, FN_DSLL32));
 }
 
 void mips_dsrl32(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, 0, rt, rd, sa & 0x1F, FN_DSRL32));
 }
 
 void mips_dsra32(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, 0, rt, rd, sa & 0x1F, FN_DSRA32));
 }
 
 void mips_dsllv(MipsEmitter *em, MipsReg rd, MipsReg rt, MipsReg rs) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_DSLLV));
 }
 
 void mips_dsrlv(MipsEmitter *em, MipsReg rd, MipsReg rt, MipsReg rs) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_DSRLV));
 }
 
 void mips_dsrav(MipsEmitter *em, MipsReg rd, MipsReg rt, MipsReg rs) {
     mips_emit_raw(em, encode_r_type(OP_SPECIAL, rs, rt, rd, 0, FN_DSRAV));
 }
 
 /* Note: DMULT/DDIV not implemented on R5900
  * For 64-bit multiply: use MULT + shifts or MMI PMULTW
  * For 64-bit divide: implement software division
  */
 
 void mips_ld(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs) {
     mips_emit_raw(em, encode_i_type(OP_LD, rs, rt, offset));
 }
 
 void mips_sd(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs) {
     mips_emit_raw(em, encode_i_type(OP_SD, rs, rt, offset));
 }
 
 void mips_daddi(MipsEmitter *em, MipsReg rt, MipsReg rs, int16_t imm) {
     mips_emit_raw(em, encode_i_type(OP_DADDI, rs, rt, imm));
 }
 
 void mips_daddiu(MipsEmitter *em, MipsReg rt, MipsReg rs, int16_t imm) {
     mips_emit_raw(em, encode_i_type(OP_DADDIU, rs, rt, imm));
 }
 
 /* ============================================
  * I-Type Instructions
  * ============================================ */
 
 void mips_addi(MipsEmitter *em, MipsReg rt, MipsReg rs, int16_t imm) {
     mips_emit_raw(em, encode_i_type(OP_ADDI, rs, rt, imm));
 }
 
 void mips_addiu(MipsEmitter *em, MipsReg rt, MipsReg rs, int16_t imm) {
     mips_emit_raw(em, encode_i_type(OP_ADDIU, rs, rt, imm));
 }
 
 void mips_andi(MipsEmitter *em, MipsReg rt, MipsReg rs, uint16_t imm) {
     mips_emit_raw(em, encode_i_type(OP_ANDI, rs, rt, (int16_t)imm));
 }
 
 void mips_ori(MipsEmitter *em, MipsReg rt, MipsReg rs, uint16_t imm) {
     mips_emit_raw(em, encode_i_type(OP_ORI, rs, rt, (int16_t)imm));
 }
 
 void mips_xori(MipsEmitter *em, MipsReg rt, MipsReg rs, uint16_t imm) {
     mips_emit_raw(em, encode_i_type(OP_XORI, rs, rt, (int16_t)imm));
 }
 
 void mips_slti(MipsEmitter *em, MipsReg rt, MipsReg rs, int16_t imm) {
     mips_emit_raw(em, encode_i_type(OP_SLTI, rs, rt, imm));
 }
 
 void mips_sltiu(MipsEmitter *em, MipsReg rt, MipsReg rs, int16_t imm) {
     mips_emit_raw(em, encode_i_type(OP_SLTIU, rs, rt, imm));
 }
 
 void mips_lui(MipsEmitter *em, MipsReg rt, uint16_t imm) {
     mips_emit_raw(em, encode_i_type(OP_LUI, 0, rt, (int16_t)imm));
 }
 
 void mips_lw(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs) {
     mips_emit_raw(em, encode_i_type(OP_LW, rs, rt, offset));
 }
 
void mips_sw(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs) {
    mips_emit_raw(em, encode_i_type(OP_SW, rs, rt, offset));
}

/* SWL rt, offset(rs) - Store Word Left (unaligned-safe) */
void mips_swl(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs) {
    mips_emit_raw(em, encode_i_type(OP_SWL, rs, rt, offset));
}

/* SWR rt, offset(rs) - Store Word Right (unaligned-safe) */
void mips_swr(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs) {
    mips_emit_raw(em, encode_i_type(OP_SWR, rs, rt, offset));
}

void mips_lb(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs) {
     mips_emit_raw(em, encode_i_type(OP_LB, rs, rt, offset));
 }
 
 void mips_lbu(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs) {
     mips_emit_raw(em, encode_i_type(OP_LBU, rs, rt, offset));
 }
 
 void mips_sb(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs) {
     mips_emit_raw(em, encode_i_type(OP_SB, rs, rt, offset));
 }
 
 void mips_lh(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs) {
     mips_emit_raw(em, encode_i_type(OP_LH, rs, rt, offset));
 }
 
 void mips_lhu(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs) {
     mips_emit_raw(em, encode_i_type(OP_LHU, rs, rt, offset));
 }
 
 void mips_sh(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs) {
     mips_emit_raw(em, encode_i_type(OP_SH, rs, rt, offset));
 }
 
 /* ============================================
  * Branch Instructions
  * ============================================ */
 
 void mips_beq(MipsEmitter *em, MipsReg rs, MipsReg rt, int label_id) {
     add_fixup(em, label_id, FIXUP_BRANCH);
     mips_emit_raw(em, encode_i_type(OP_BEQ, rs, rt, 0));
 }
 
 void mips_bne(MipsEmitter *em, MipsReg rs, MipsReg rt, int label_id) {
     add_fixup(em, label_id, FIXUP_BRANCH);
     mips_emit_raw(em, encode_i_type(OP_BNE, rs, rt, 0));
 }
 
 void mips_bgtz(MipsEmitter *em, MipsReg rs, int label_id) {
     add_fixup(em, label_id, FIXUP_BRANCH);
     mips_emit_raw(em, encode_i_type(OP_BGTZ, rs, 0, 0));
 }
 
 void mips_blez(MipsEmitter *em, MipsReg rs, int label_id) {
     add_fixup(em, label_id, FIXUP_BRANCH);
     mips_emit_raw(em, encode_i_type(OP_BLEZ, rs, 0, 0));
 }
 
 void mips_bltz(MipsEmitter *em, MipsReg rs, int label_id) {
     add_fixup(em, label_id, FIXUP_BRANCH);
     mips_emit_raw(em, encode_i_type(OP_REGIMM, rs, RT_BLTZ, 0));
 }
 
 void mips_bgez(MipsEmitter *em, MipsReg rs, int label_id) {
     add_fixup(em, label_id, FIXUP_BRANCH);
     mips_emit_raw(em, encode_i_type(OP_REGIMM, rs, RT_BGEZ, 0));
 }
 
 /* ============================================
  * Jump Instructions
  * ============================================ */
 
 void mips_j(MipsEmitter *em, int label_id) {
     add_fixup(em, label_id, FIXUP_JUMP);
     mips_emit_raw(em, encode_j_type(OP_J, 0));
 }
 
 void mips_jal(MipsEmitter *em, int label_id) {
     add_fixup(em, label_id, FIXUP_JUMP);
     mips_emit_raw(em, encode_j_type(OP_JAL, 0));
 }
 
 /* ============================================
  * FPU Instructions (COP1)
  * ============================================ */
 
 void mips_lwc1(MipsEmitter *em, MipsFpuReg ft, int16_t offset, MipsReg rs) {
     mips_emit_raw(em, encode_i_type(OP_LWC1, rs, ft, offset));
 }
 
 void mips_swc1(MipsEmitter *em, MipsFpuReg ft, int16_t offset, MipsReg rs) {
     mips_emit_raw(em, encode_i_type(OP_SWC1, rs, ft, offset));
 }
 
 void mips_mtc1(MipsEmitter *em, MipsReg rt, MipsFpuReg fs) {
     mips_emit_raw(em, encode_cop1_r(FMT_MTC1, rt, fs, 0, 0));
 }
 
 void mips_mfc1(MipsEmitter *em, MipsReg rt, MipsFpuReg fs) {
     mips_emit_raw(em, encode_cop1_r(FMT_MFC1, rt, fs, 0, 0));
 }
 
void mips_add_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs, MipsFpuReg ft) {
    uint32_t encoded = encode_cop1_r(FMT_S, ft, fs, fd, FN_ADD_S);
    mips_emit_raw(em, encoded);
}
 
 void mips_sub_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs, MipsFpuReg ft) {
     mips_emit_raw(em, encode_cop1_r(FMT_S, ft, fs, fd, FN_SUB_S));
 }
 
 void mips_mul_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs, MipsFpuReg ft) {
     mips_emit_raw(em, encode_cop1_r(FMT_S, ft, fs, fd, FN_MUL_S));
 }
 
 void mips_div_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs, MipsFpuReg ft) {
     mips_emit_raw(em, encode_cop1_r(FMT_S, ft, fs, fd, FN_DIV_S));
 }
 
 void mips_sqrt_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs) {
     mips_emit_raw(em, encode_cop1_r(FMT_S, 0, fs, fd, FN_SQRT_S));
 }
 
 void mips_abs_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs) {
     mips_emit_raw(em, encode_cop1_r(FMT_S, 0, fs, fd, FN_ABS_S));
 }
 
 void mips_neg_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs) {
     mips_emit_raw(em, encode_cop1_r(FMT_S, 0, fs, fd, FN_NEG_S));
 }
 
 void mips_mov_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs) {
     mips_emit_raw(em, encode_cop1_r(FMT_S, 0, fs, fd, FN_MOV_S));
 }
 
 void mips_cvt_s_w(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs) {
     mips_emit_raw(em, encode_cop1_r(FMT_W, 0, fs, fd, FN_CVT_S_W));
 }
 
 void mips_cvt_w_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs) {
    mips_emit_raw(em, encode_cop1_r(FMT_S, 0, fs, fd, FN_CVT_W_S));
}

/* R5900 specific: MIN.S fd, fs, ft - fd = min(fs, ft) */
void mips_min_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs, MipsFpuReg ft) {
    mips_emit_raw(em, encode_cop1_r(FMT_S, ft, fs, fd, FN_MIN_S));
}

/* R5900 specific: MAX.S fd, fs, ft - fd = max(fs, ft) */
void mips_max_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs, MipsFpuReg ft) {
    mips_emit_raw(em, encode_cop1_r(FMT_S, ft, fs, fd, FN_MAX_S));
}
 
 void mips_c_eq_s(MipsEmitter *em, MipsFpuReg fs, MipsFpuReg ft) {
     mips_emit_raw(em, encode_cop1_r(FMT_S, ft, fs, 0, FN_C_EQ_S));
 }
 
 void mips_c_lt_s(MipsEmitter *em, MipsFpuReg fs, MipsFpuReg ft) {
     mips_emit_raw(em, encode_cop1_r(FMT_S, ft, fs, 0, FN_C_LT_S));
 }
 
 void mips_c_le_s(MipsEmitter *em, MipsFpuReg fs, MipsFpuReg ft) {
     mips_emit_raw(em, encode_cop1_r(FMT_S, ft, fs, 0, FN_C_LE_S));
 }
 
 void mips_bc1t(MipsEmitter *em, int label_id) {
     add_fixup(em, label_id, FIXUP_BRANCH);
     mips_emit_raw(em, encode_cop1_bc(0, BC1T_CODE, 0));
 }
 
 void mips_bc1f(MipsEmitter *em, int label_id) {
     add_fixup(em, label_id, FIXUP_BRANCH);
     mips_emit_raw(em, encode_cop1_bc(0, BC1F_CODE, 0));
 }
 
 /* ============================================
  * Pseudo-Instructions
  * ============================================ */
 
 void mips_nop(MipsEmitter *em) {
     mips_sll(em, REG_ZERO, REG_ZERO, 0);
 }
 
 void mips_li(MipsEmitter *em, MipsReg rt, int32_t imm) {
     if (imm >= -32768 && imm <= 32767) {
         /* Use ADDIU for small values */
         mips_addiu(em, rt, REG_ZERO, (int16_t)imm);
     } else if ((imm & 0xFFFF) == 0) {
         /* Upper 16 bits only */
         mips_lui(em, rt, (uint16_t)(imm >> 16));
     } else {
         /* Full 32-bit value: LUI + ORI */
         mips_lui(em, rt, (uint16_t)(imm >> 16));
         mips_ori(em, rt, rt, (uint16_t)(imm & 0xFFFF));
     }
 }
 
 void mips_la(MipsEmitter *em, MipsReg rt, void *addr) {
     mips_li(em, rt, (int32_t)(uintptr_t)addr);
 }
 
void mips_move(MipsEmitter *em, MipsReg rd, MipsReg rs) {
    if (rd != rs) {
        mips_addu(em, rd, rs, REG_ZERO);
    }
}

/* ============================================
 * Disassembly / Debug Functions
 * ============================================ */

static const char *reg_names[] = {
    "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
};

static const char *get_reg_name(MipsReg reg) {
    if (reg < REG_COUNT) {
        return reg_names[reg];
    }
    return "?";
}

static void get_fpu_reg_name_str(int fpu_reg, char *buf, size_t buf_size) {
    snprintf(buf, buf_size, "$f%d", fpu_reg);
}

static void disassemble_instr(uint32_t instr, char *buf, size_t buf_size) {
    uint8_t op = (instr >> 26) & 0x3F;
    uint8_t rs = (instr >> 21) & 0x1F;
    uint8_t rt = (instr >> 16) & 0x1F;
    uint8_t rd = (instr >> 11) & 0x1F;
    uint8_t shamt = (instr >> 6) & 0x1F;
    uint8_t funct = instr & 0x3F;
    int16_t imm = (int16_t)(instr & 0xFFFF);
    uint32_t target = instr & 0x3FFFFFF;
    
    /* COP1 (FPU) instructions - opcode 0x11 */
    if (op == 0x11) {
        uint8_t fmt = (instr >> 21) & 0x1F;
        uint8_t ft = (instr >> 16) & 0x1F;
        uint8_t fs = (instr >> 11) & 0x1F;
        uint8_t fd = (instr >> 6) & 0x1F;
        
        switch (fmt) {
            case 0x00: { /* MFC1 - Move From Coprocessor 1: rt = fs */
                char fs_buf[8];
                get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                snprintf(buf, buf_size, "MFC1   %s, %s", get_reg_name(ft), fs_buf);
                break;
            }
            case 0x02: { /* CFC1 - Control From Coprocessor 1: rt = fs */
                char fs_buf[8];
                get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                snprintf(buf, buf_size, "CFC1   %s, %s", get_reg_name(ft), fs_buf);
                break;
            }
            case 0x04: { /* MTC1 - Move To Coprocessor 1: fs = rt */
                char fs_buf[8];
                get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                snprintf(buf, buf_size, "MTC1   %s, %s", get_reg_name(ft), fs_buf);
                break;
            }
            case 0x06: { /* CTC1 - Control To Coprocessor 1: fs = rt */
                char fs_buf[8];
                get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                snprintf(buf, buf_size, "CTC1   %s, %s", get_reg_name(ft), fs_buf);
                break;
            }
            case 0x08: { /* BC1 - Branch on Coprocessor 1 condition */
                /* Bits 20-18: cc (condition code), Bit 17: nd, Bit 16: tf */
                int tf = (instr >> 16) & 1;
                if (tf) {
                    snprintf(buf, buf_size, "BC1T   0x%04x", imm);
                } else {
                    snprintf(buf, buf_size, "BC1F   0x%04x", imm);
                }
                break;
            }
            case 0x10: /* S format (single precision) */
                switch (funct) {
                    case 0x00: {
                        char fd_buf[8], fs_buf[8], ft_buf[8];
                        get_fpu_reg_name_str(fd, fd_buf, sizeof(fd_buf));
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        get_fpu_reg_name_str(ft, ft_buf, sizeof(ft_buf));
                        snprintf(buf, buf_size, "ADD.S  %s, %s, %s", fd_buf, fs_buf, ft_buf);
                        break;
                    }
                    case 0x01: {
                        char fd_buf[8], fs_buf[8], ft_buf[8];
                        get_fpu_reg_name_str(fd, fd_buf, sizeof(fd_buf));
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        get_fpu_reg_name_str(ft, ft_buf, sizeof(ft_buf));
                        snprintf(buf, buf_size, "SUB.S  %s, %s, %s", fd_buf, fs_buf, ft_buf);
                        break;
                    }
                    case 0x02: {
                        char fd_buf[8], fs_buf[8], ft_buf[8];
                        get_fpu_reg_name_str(fd, fd_buf, sizeof(fd_buf));
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        get_fpu_reg_name_str(ft, ft_buf, sizeof(ft_buf));
                        snprintf(buf, buf_size, "MUL.S  %s, %s, %s", fd_buf, fs_buf, ft_buf);
                        break;
                    }
                    case 0x03: {
                        char fd_buf[8], fs_buf[8], ft_buf[8];
                        get_fpu_reg_name_str(fd, fd_buf, sizeof(fd_buf));
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        get_fpu_reg_name_str(ft, ft_buf, sizeof(ft_buf));
                        snprintf(buf, buf_size, "DIV.S  %s, %s, %s", fd_buf, fs_buf, ft_buf);
                        break;
                    }
                    case 0x04: {
                        char fd_buf[8], fs_buf[8];
                        get_fpu_reg_name_str(fd, fd_buf, sizeof(fd_buf));
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        snprintf(buf, buf_size, "SQRT.S %s, %s", fd_buf, fs_buf);
                        break;
                    }
                    case 0x05: {
                        char fd_buf[8], fs_buf[8];
                        get_fpu_reg_name_str(fd, fd_buf, sizeof(fd_buf));
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        snprintf(buf, buf_size, "ABS.S  %s, %s", fd_buf, fs_buf);
                        break;
                    }
                    case 0x06: {
                        char fd_buf[8], fs_buf[8];
                        get_fpu_reg_name_str(fd, fd_buf, sizeof(fd_buf));
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        snprintf(buf, buf_size, "MOV.S  %s, %s", fd_buf, fs_buf);
                        break;
                    }
                    case 0x07: {
                        char fd_buf[8], fs_buf[8];
                        get_fpu_reg_name_str(fd, fd_buf, sizeof(fd_buf));
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        snprintf(buf, buf_size, "NEG.S  %s, %s", fd_buf, fs_buf);
                        break;
                    }
                    case 0x24: {
                        char fd_buf[8], fs_buf[8];
                        get_fpu_reg_name_str(fd, fd_buf, sizeof(fd_buf));
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        snprintf(buf, buf_size, "CVT.W.S %s, %s", fd_buf, fs_buf);
                        break;
                    }
                    case 0x32: {
                        char fs_buf[8], ft_buf[8];
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        get_fpu_reg_name_str(ft, ft_buf, sizeof(ft_buf));
                        snprintf(buf, buf_size, "C.EQ.S %s, %s", fs_buf, ft_buf);
                        break;
                    }
                    case 0x34: {  /* C.LT.S - corrected from 0x3C */
                        char fs_buf[8], ft_buf[8];
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        get_fpu_reg_name_str(ft, ft_buf, sizeof(ft_buf));
                        snprintf(buf, buf_size, "C.LT.S %s, %s", fs_buf, ft_buf);
                        break;
                    }
                    case 0x36: {  /* C.LE.S - corrected from 0x3E */
                        char fs_buf[8], ft_buf[8];
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        get_fpu_reg_name_str(ft, ft_buf, sizeof(ft_buf));
                        snprintf(buf, buf_size, "C.LE.S %s, %s", fs_buf, ft_buf);
                        break;
                    }
                    case 0x28: {  /* MAX.S - R5900 specific */
                        char fd_buf[8], fs_buf[8], ft_buf[8];
                        get_fpu_reg_name_str(fd, fd_buf, sizeof(fd_buf));
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        get_fpu_reg_name_str(ft, ft_buf, sizeof(ft_buf));
                        snprintf(buf, buf_size, "MAX.S  %s, %s, %s", fd_buf, fs_buf, ft_buf);
                        break;
                    }
                    case 0x29: {  /* MIN.S - R5900 specific */
                        char fd_buf[8], fs_buf[8], ft_buf[8];
                        get_fpu_reg_name_str(fd, fd_buf, sizeof(fd_buf));
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        get_fpu_reg_name_str(ft, ft_buf, sizeof(ft_buf));
                        snprintf(buf, buf_size, "MIN.S  %s, %s, %s", fd_buf, fs_buf, ft_buf);
                        break;
                    }
                    default: snprintf(buf, buf_size, "COP1.S fmt=0x%02x ft=%d fs=%d fd=%d fn=0x%02x", fmt, ft, fs, fd, funct); break;
                }
                break;
            case 0x14: /* W format */
                switch (funct) {
                    case 0x20: {
                        char fd_buf[8], fs_buf[8];
                        get_fpu_reg_name_str(fd, fd_buf, sizeof(fd_buf));
                        get_fpu_reg_name_str(fs, fs_buf, sizeof(fs_buf));
                        snprintf(buf, buf_size, "CVT.S.W %s, %s", fd_buf, fs_buf);
                        break;
                    }
                    default: snprintf(buf, buf_size, "COP1.W fmt=0x%02x ft=%d fs=%d fd=%d fn=0x%02x", fmt, ft, fs, fd, funct); break;
                }
                break;
            default:
                snprintf(buf, buf_size, "COP1   fmt=0x%02x ft=%d fs=%d fd=%d fn=0x%02x", fmt, ft, fs, fd, funct);
                break;
        }
        return;
    }
    
    if (op == 0) {
        /* R-type */
        switch (funct) {
            case 0x00: snprintf(buf, buf_size, "SLL    %s, %s, %d", get_reg_name(rd), get_reg_name(rt), shamt); break;
            case 0x02: snprintf(buf, buf_size, "SRL    %s, %s, %d", get_reg_name(rd), get_reg_name(rt), shamt); break;
            case 0x03: snprintf(buf, buf_size, "SRA    %s, %s, %d", get_reg_name(rd), get_reg_name(rt), shamt); break;
            case 0x08: snprintf(buf, buf_size, "JR     %s", get_reg_name(rs)); break;
            case 0x09: snprintf(buf, buf_size, "JALR   %s, %s", get_reg_name(rd), get_reg_name(rs)); break;
            case 0x0C: snprintf(buf, buf_size, "SYSCALL"); break;
            case 0x10: snprintf(buf, buf_size, "MFHI   %s", get_reg_name(rd)); break;
            case 0x11: snprintf(buf, buf_size, "MTHI   %s", get_reg_name(rs)); break;
            case 0x12: snprintf(buf, buf_size, "MFLO   %s", get_reg_name(rd)); break;
            case 0x13: snprintf(buf, buf_size, "MTLO   %s", get_reg_name(rs)); break;
            case 0x18: snprintf(buf, buf_size, "MULT   %s, %s", get_reg_name(rs), get_reg_name(rt)); break;
            case 0x19: snprintf(buf, buf_size, "MULTU  %s, %s", get_reg_name(rs), get_reg_name(rt)); break;
            case 0x1A: snprintf(buf, buf_size, "DIV    %s, %s", get_reg_name(rs), get_reg_name(rt)); break;
            case 0x1B: snprintf(buf, buf_size, "DIVU   %s, %s", get_reg_name(rs), get_reg_name(rt)); break;
            case 0x20: snprintf(buf, buf_size, "ADD    %s, %s, %s", get_reg_name(rd), get_reg_name(rs), get_reg_name(rt)); break;
            case 0x21: snprintf(buf, buf_size, "ADDU   %s, %s, %s", get_reg_name(rd), get_reg_name(rs), get_reg_name(rt)); break;
            case 0x22: snprintf(buf, buf_size, "SUB    %s, %s, %s", get_reg_name(rd), get_reg_name(rs), get_reg_name(rt)); break;
            case 0x23: snprintf(buf, buf_size, "SUBU   %s, %s, %s", get_reg_name(rd), get_reg_name(rs), get_reg_name(rt)); break;
            case 0x24: snprintf(buf, buf_size, "AND    %s, %s, %s", get_reg_name(rd), get_reg_name(rs), get_reg_name(rt)); break;
            case 0x25: snprintf(buf, buf_size, "OR     %s, %s, %s", get_reg_name(rd), get_reg_name(rs), get_reg_name(rt)); break;
            case 0x26: snprintf(buf, buf_size, "XOR    %s, %s, %s", get_reg_name(rd), get_reg_name(rs), get_reg_name(rt)); break;
            case 0x27: snprintf(buf, buf_size, "NOR    %s, %s, %s", get_reg_name(rd), get_reg_name(rs), get_reg_name(rt)); break;
            case 0x2A: snprintf(buf, buf_size, "SLT    %s, %s, %s", get_reg_name(rd), get_reg_name(rs), get_reg_name(rt)); break;
            case 0x2B: snprintf(buf, buf_size, "SLTU   %s, %s, %s", get_reg_name(rd), get_reg_name(rs), get_reg_name(rt)); break;
            case 0x2C: snprintf(buf, buf_size, "SLLV   %s, %s, %s", get_reg_name(rd), get_reg_name(rt), get_reg_name(rs)); break;
            case 0x2E: snprintf(buf, buf_size, "SRLV   %s, %s, %s", get_reg_name(rd), get_reg_name(rt), get_reg_name(rs)); break;
            case 0x2F: snprintf(buf, buf_size, "SRAV   %s, %s, %s", get_reg_name(rd), get_reg_name(rt), get_reg_name(rs)); break;
            default: snprintf(buf, buf_size, "R-TYPE op=0x%02x funct=0x%02x", op, funct); break;
        }
    } else if (op == 0x02 || op == 0x03) {
        /* J-type */
        if (op == 0x02) {
            snprintf(buf, buf_size, "J      0x%08X", target << 2);
        } else {
            snprintf(buf, buf_size, "JAL    0x%08X", target << 2);
        }
    } else {
        /* I-type */
        switch (op) {
            case 0x04: snprintf(buf, buf_size, "BEQ    %s, %s, %+d", get_reg_name(rs), get_reg_name(rt), (int)(imm << 2)); break;
            case 0x05: snprintf(buf, buf_size, "BNE    %s, %s, %+d", get_reg_name(rs), get_reg_name(rt), (int)(imm << 2)); break;
            case 0x08: snprintf(buf, buf_size, "ADDI   %s, %s, %d", get_reg_name(rt), get_reg_name(rs), imm); break;
            case 0x09: snprintf(buf, buf_size, "ADDIU  %s, %s, %d", get_reg_name(rt), get_reg_name(rs), imm); break;
            case 0x0A: snprintf(buf, buf_size, "SLTI   %s, %s, %d", get_reg_name(rt), get_reg_name(rs), imm); break;
            case 0x0B: snprintf(buf, buf_size, "SLTIU  %s, %s, %d", get_reg_name(rt), get_reg_name(rs), imm); break;
            case 0x0C: snprintf(buf, buf_size, "ANDI   %s, %s, 0x%04X", get_reg_name(rt), get_reg_name(rs), (uint16_t)imm); break;
            case 0x0D: snprintf(buf, buf_size, "ORI    %s, %s, 0x%04X", get_reg_name(rt), get_reg_name(rs), (uint16_t)imm); break;
            case 0x0E: snprintf(buf, buf_size, "XORI   %s, %s, 0x%04X", get_reg_name(rt), get_reg_name(rs), (uint16_t)imm); break;
            case 0x0F: snprintf(buf, buf_size, "LUI    %s, 0x%04X", get_reg_name(rt), (uint16_t)imm); break;
            case 0x20: snprintf(buf, buf_size, "LB     %s, %d(%s)", get_reg_name(rt), imm, get_reg_name(rs)); break;
            case 0x21: snprintf(buf, buf_size, "LH     %s, %d(%s)", get_reg_name(rt), imm, get_reg_name(rs)); break;
            case 0x23: snprintf(buf, buf_size, "LW     %s, %d(%s)", get_reg_name(rt), imm, get_reg_name(rs)); break;
            case 0x28: snprintf(buf, buf_size, "SB     %s, %d(%s)", get_reg_name(rt), imm, get_reg_name(rs)); break;
            case 0x29: snprintf(buf, buf_size, "SH     %s, %d(%s)", get_reg_name(rt), imm, get_reg_name(rs)); break;
            case 0x2B: snprintf(buf, buf_size, "SW     %s, %d(%s)", get_reg_name(rt), imm, get_reg_name(rs)); break;
            case 0x31: {
                char fpu_buf[8];
                get_fpu_reg_name_str(rt, fpu_buf, sizeof(fpu_buf));
                snprintf(buf, buf_size, "LWC1   %s, %d(%s)", fpu_buf, imm, get_reg_name(rs));
                break;
            }
            case 0x39: {
                char fpu_buf[8];
                get_fpu_reg_name_str(rt, fpu_buf, sizeof(fpu_buf));
                snprintf(buf, buf_size, "SWC1   %s, %d(%s)", fpu_buf, imm, get_reg_name(rs));
                break;
            }
            default: snprintf(buf, buf_size, "I-TYPE op=0x%02x rs=%s rt=%s imm=%d", op, get_reg_name(rs), get_reg_name(rt), imm); break;
        }
    }
}

void mips_emitter_dump(MipsEmitter *em, const char *label) {
    if (!em || !em->buffer.code) return;
    
    /* Use function_start to only dump current function, not entire arena */
    size_t start = em->buffer.function_start;
    size_t func_count = em->buffer.count - start;
    void *func_base = em->buffer.code + start;
    
    printf("\n=== MIPS Assembly Dump: %s ===\n", label ? label : "Function");
    printf("Code size: %zu instructions (%zu bytes)\n", func_count, func_count * 4);
    printf("Code base address: 0x%x\n", (uint32_t)(uintptr_t)func_base);
    printf("Labels: %zu\n", em->buffer.label_count);
    printf("Fixups: %d\n", em->fixup_count);
    printf("\n");
    
    for (size_t i = 0; i < func_count; i++) {
        char instr_str[128];
        disassemble_instr(em->buffer.code[start + i], instr_str, sizeof(instr_str));
        uint32_t instr_addr = (uint32_t)(uintptr_t)func_base + (i * sizeof(uint32_t));
        printf("  %04zu: 0x%08X  %s  [@0x%08X]\n", i, em->buffer.code[start + i], instr_str, instr_addr);
    }
    
    printf("=== End of dump ===\n\n");
}
 