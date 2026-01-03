/*
 * AthenaEnv Native Compiler - MIPS R5900 Code Emitter
 * 
 * Copyright (c) 2025 AthenaEnv Project
 * 
 * Generates native MIPS R5900 machine code for the PS2 Emotion Engine.
 * Reference: ps2tek documentation for instruction encoding.
 */

 #ifndef MIPS_EMITTER_H
 #define MIPS_EMITTER_H
 
 #include <stdint.h>
 #include <stddef.h>
 #include <stdbool.h>
 #include "native_types.h"
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 /*
  * MIPS R5900 General Purpose Registers
  */
 typedef enum MipsReg {
     REG_ZERO = 0,   /* Always 0 */
     REG_AT = 1,     /* Assembler temporary */
     REG_V0 = 2,     /* Return value */
     REG_V1 = 3,
     REG_A0 = 4,     /* Arguments */
     REG_A1 = 5,
     REG_A2 = 6,
     REG_A3 = 7,
     REG_T0 = 8,     /* Temporaries */
     REG_T1 = 9,
     REG_T2 = 10,
     REG_T3 = 11,
     REG_T4 = 12,
     REG_T5 = 13,
     REG_T6 = 14,
     REG_T7 = 15,
     REG_S0 = 16,    /* Saved temporaries */
     REG_S1 = 17,
     REG_S2 = 18,
     REG_S3 = 19,
     REG_S4 = 20,
     REG_S5 = 21,
     REG_S6 = 22,
     REG_S7 = 23,
     REG_T8 = 24,
     REG_T9 = 25,
     REG_K0 = 26,    /* Reserved for kernel */
     REG_K1 = 27,
     REG_GP = 28,    /* Global pointer */
     REG_SP = 29,    /* Stack pointer */
     REG_FP = 30,    /* Frame pointer */
     REG_RA = 31,    /* Return address */
     REG_COUNT
 } MipsReg;
 
 /*
  * MIPS FPU Registers (COP1)
  */
 typedef enum MipsFpuReg {
     FPU_F0 = 0,
     FPU_F1, FPU_F2, FPU_F3, FPU_F4, FPU_F5, FPU_F6, FPU_F7,
     FPU_F8, FPU_F9, FPU_F10, FPU_F11, FPU_F12, FPU_F13, FPU_F14, FPU_F15,
     FPU_F16, FPU_F17, FPU_F18, FPU_F19, FPU_F20, FPU_F21, FPU_F22, FPU_F23,
     FPU_F24, FPU_F25, FPU_F26, FPU_F27, FPU_F28, FPU_F29, FPU_F30, FPU_F31,
     FPU_REG_COUNT
 } MipsFpuReg;
 
 /*
  * Code buffer for emitting instructions
  */
 #define MIPS_CODE_BUFFER_SIZE (16 * 1024)  /* 16KB default */
 
 typedef struct MipsCodeBuffer {
     uint32_t *code;         /* Instruction buffer */
     size_t capacity;        /* Max instructions */
     size_t count;           /* Current instruction count */
     size_t function_start;  /* Start of current function (arena mode) */
     size_t label_count;     /* Number of labels defined */
 } MipsCodeBuffer;
 
 /*
  * Label for branch targets
  */
 typedef struct MipsLabel {
     int32_t offset;         /* Offset in instructions (-1 = unresolved) */
     bool resolved;
 } MipsLabel;
 
 /*
  * Forward reference for patching
  */
 typedef struct MipsFixup {
     size_t instr_offset;    /* Instruction to patch */
     int label_id;           /* Target label */
     int fixup_type;         /* Type of fixup */
 } MipsFixup;
 
 #define MAX_LABELS 256
 #define MAX_FIXUPS 512
 
 typedef struct MipsEmitter {
     MipsCodeBuffer buffer;
     MipsLabel labels[MAX_LABELS];
     MipsFixup fixups[MAX_FIXUPS];
     int fixup_count;
     
     /* Register allocation state */
     uint32_t gpr_used;      /* Bitmask of used GPRs */
     uint32_t fpr_used;      /* Bitmask of used FPRs */
     int stack_offset;       /* Current stack offset */
     
     /* Error state */
     bool has_error;
     const char *error_msg;
 } MipsEmitter;
 
 /* ============================================
  * Emitter lifecycle
  * ============================================ */
 
 /* Initialize a new emitter */
 int mips_emitter_init(MipsEmitter *em);
 
 /* Free emitter resources */
 void mips_emitter_free(MipsEmitter *em);
 
 /* Reset emitter for new function */
 void mips_emitter_reset(MipsEmitter *em);
 
 /* Get generated code pointer and size */
 void *mips_emitter_get_code(MipsEmitter *em, size_t *out_size);
 
 /* Run peephole optimizations on generated code */
 int mips_emitter_peephole_optimize(MipsEmitter *em);
 
 /* Finalize code (resolve labels, flush cache) */
 int mips_emitter_finalize(MipsEmitter *em);
 
 /* ============================================
  * Label management
  * ============================================ */
 
 /* Create a new label (returns label ID) */
 int mips_label_create(MipsEmitter *em);
 
 /* Bind label to current position */
 void mips_label_bind(MipsEmitter *em, int label_id);
 
 /* ============================================
  * Register allocation helpers
  * ============================================ */
 
 /* Allocate a temporary GPR */
 MipsReg mips_alloc_gpr(MipsEmitter *em);
 
 /* Allocate a temporary FPR */
 MipsFpuReg mips_alloc_fpr(MipsEmitter *em);
 
 /* Free a GPR */
 void mips_free_gpr(MipsEmitter *em, MipsReg reg);
 
 /* Free an FPR */
 void mips_free_fpr(MipsEmitter *em, MipsFpuReg reg);
 
 /* ============================================
  * MIPS Instruction Emitters
  * ============================================ */
 
 /* --- R-Type Instructions --- */
 
 /* ADD rd, rs, rt */
 void mips_add(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);
 
 /* ADDU rd, rs, rt (no overflow trap) */
 void mips_addu(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);
 
 /* SUB rd, rs, rt */
 void mips_sub(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);
 
 /* SUBU rd, rs, rt */
 void mips_subu(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);
 
 /* AND rd, rs, rt */
 void mips_and(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);
 
 /* OR rd, rs, rt */
 void mips_or(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);
 
 /* XOR rd, rs, rt */
 void mips_xor(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);
 
 /* NOR rd, rs, rt */
 void mips_nor(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);
 
 /* SLT rd, rs, rt (set on less than) */
 void mips_slt(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);
 
 /* SLTU rd, rs, rt (unsigned) */
 void mips_sltu(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);
 
 /* SLL rd, rt, sa (shift left logical) */
 void mips_sll(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa);
 
 /* SRL rd, rt, sa (shift right logical) */
 void mips_srl(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa);
 
 /* SRA rd, rt, sa (shift right arithmetic) */
 void mips_sra(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa);
 
 /* SLLV rd, rt, rs (variable shift) */
 void mips_sllv(MipsEmitter *em, MipsReg rd, MipsReg rt, MipsReg rs);
 
 /* SRLV rd, rt, rs */
 void mips_srlv(MipsEmitter *em, MipsReg rd, MipsReg rt, MipsReg rs);
 
 /* SRAV rd, rt, rs */
 void mips_srav(MipsEmitter *em, MipsReg rd, MipsReg rt, MipsReg rs);
 
 /* MULT rs, rt (result in HI/LO) */
 void mips_mult(MipsEmitter *em, MipsReg rs, MipsReg rt);
 
 /* MULTU rs, rt (unsigned) */
 void mips_multu(MipsEmitter *em, MipsReg rs, MipsReg rt);
 
 /* DIV rs, rt (quotient in LO, remainder in HI) */
 void mips_div(MipsEmitter *em, MipsReg rs, MipsReg rt);
 
 /* DIVU rs, rt */
 void mips_divu(MipsEmitter *em, MipsReg rs, MipsReg rt);
 
 /* MFHI rd (move from HI) */
 void mips_mfhi(MipsEmitter *em, MipsReg rd);
 
 /* MFLO rd (move from LO) */
 void mips_mflo(MipsEmitter *em, MipsReg rd);
 
 /* JR rs (jump register) */
 void mips_jr(MipsEmitter *em, MipsReg rs);
 
 /* JALR rd, rs (jump and link register) */
void mips_jalr(MipsEmitter *em, MipsReg rd, MipsReg rs);

/* --- MIPS64 Instructions (R5900) --- */

/* DADD rd, rs, rt (64-bit add) */
void mips_dadd(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);

/* DADDU rd, rs, rt (64-bit add unsigned) */
void mips_daddu(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);

/* DSUB rd, rs, rt (64-bit subtract) */
void mips_dsub(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);

/* DSUBU rd, rs, rt (64-bit subtract unsigned) */
void mips_dsubu(MipsEmitter *em, MipsReg rd, MipsReg rs, MipsReg rt);

/* DSLL rd, rt, sa (64-bit shift left logical) */
void mips_dsll(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa);

/* DSRL rd, rt, sa (64-bit shift right logical) */
void mips_dsrl(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa);

/* DSRA rd, rt, sa (64-bit shift right arithmetic) */
void mips_dsra(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa);

/* DSLL32 rd, rt, sa (64-bit shift left by 32+sa) */
void mips_dsll32(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa);

/* DSRL32 rd, rt, sa (64-bit shift right logical by 32+sa) */
void mips_dsrl32(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa);

/* DSRA32 rd, rt, sa (64-bit shift right arithmetic by 32+sa) */
void mips_dsra32(MipsEmitter *em, MipsReg rd, MipsReg rt, int sa);

/* DSLLV rd, rt, rs (64-bit variable shift left) */
void mips_dsllv(MipsEmitter *em, MipsReg rd, MipsReg rt, MipsReg rs);

/* DSRLV rd, rt, rs (64-bit variable shift right logical) */
void mips_dsrlv(MipsEmitter *em, MipsReg rd, MipsReg rt, MipsReg rs);

/* DSRAV rd, rt, rs (64-bit variable shift right arithmetic) */
void mips_dsrav(MipsEmitter *em, MipsReg rd, MipsReg rt, MipsReg rs);

/* ============================================
 * 64-bit Multiply/Divide Notes:
 * DMULT/DMULTU/DDIV/DDIVU are NOT available on R5900!
 * Alternatives:
 * - Use MMI PMULTW for packed 32x32 multiply
 * - Use MULT + shifts for 64-bit multiply emulation  
 * - Implement software division for 64-bit
 * ============================================ */

/* LD rt, offset(rs) (load doubleword) */
void mips_ld(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs);

/* SD rt, offset(rs) (store doubleword) */
void mips_sd(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs);

/* DADDI rt, rs, imm (64-bit add immediate) */
void mips_daddi(MipsEmitter *em, MipsReg rt, MipsReg rs, int16_t imm);

/* DADDIU rt, rs, imm (64-bit add immediate unsigned) */
void mips_daddiu(MipsEmitter *em, MipsReg rt, MipsReg rs, int16_t imm);
 
 /* --- I-Type Instructions --- */
 
 /* ADDI rt, rs, imm */
 void mips_addi(MipsEmitter *em, MipsReg rt, MipsReg rs, int16_t imm);
 
 /* ADDIU rt, rs, imm */
 void mips_addiu(MipsEmitter *em, MipsReg rt, MipsReg rs, int16_t imm);
 
 /* ANDI rt, rs, imm */
 void mips_andi(MipsEmitter *em, MipsReg rt, MipsReg rs, uint16_t imm);
 
 /* ORI rt, rs, imm */
 void mips_ori(MipsEmitter *em, MipsReg rt, MipsReg rs, uint16_t imm);
 
 /* XORI rt, rs, imm */
 void mips_xori(MipsEmitter *em, MipsReg rt, MipsReg rs, uint16_t imm);
 
 /* SLTI rt, rs, imm */
 void mips_slti(MipsEmitter *em, MipsReg rt, MipsReg rs, int16_t imm);
 
 /* SLTIU rt, rs, imm */
 void mips_sltiu(MipsEmitter *em, MipsReg rt, MipsReg rs, int16_t imm);
 
 /* LUI rt, imm (load upper immediate) */
 void mips_lui(MipsEmitter *em, MipsReg rt, uint16_t imm);
 
 /* LW rt, offset(rs) */
 void mips_lw(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs);
 
/* SW rt, offset(rs) */
void mips_sw(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs);

/* SWL rt, offset(rs) - Store Word Left (unaligned-safe) */
void mips_swl(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs);

/* SWR rt, offset(rs) - Store Word Right (unaligned-safe) */
void mips_swr(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs);

/* LB rt, offset(rs) */
 void mips_lb(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs);
 
 /* LBU rt, offset(rs) */
 void mips_lbu(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs);
 
 /* SB rt, offset(rs) */
 void mips_sb(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs);
 
 /* LH rt, offset(rs) */
 void mips_lh(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs);
 
 /* LHU rt, offset(rs) */
 void mips_lhu(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs);
 
 /* SH rt, offset(rs) */
 void mips_sh(MipsEmitter *em, MipsReg rt, int16_t offset, MipsReg rs);
 
 /* --- Branch Instructions --- */
 
 /* BEQ rs, rt, label */
 void mips_beq(MipsEmitter *em, MipsReg rs, MipsReg rt, int label_id);
 
 /* BNE rs, rt, label */
 void mips_bne(MipsEmitter *em, MipsReg rs, MipsReg rt, int label_id);
 
 /* BGTZ rs, label */
 void mips_bgtz(MipsEmitter *em, MipsReg rs, int label_id);
 
 /* BLEZ rs, label */
 void mips_blez(MipsEmitter *em, MipsReg rs, int label_id);
 
 /* BLTZ rs, label */
 void mips_bltz(MipsEmitter *em, MipsReg rs, int label_id);
 
 /* BGEZ rs, label */
 void mips_bgez(MipsEmitter *em, MipsReg rs, int label_id);
 
 /* --- Jump Instructions --- */
 
 /* J target */
 void mips_j(MipsEmitter *em, int label_id);
 
 /* JAL target */
 void mips_jal(MipsEmitter *em, int label_id);
 
 /* --- FPU Instructions (COP1) --- */
 
 /* LWC1 ft, offset(rs) */
 void mips_lwc1(MipsEmitter *em, MipsFpuReg ft, int16_t offset, MipsReg rs);
 
 /* SWC1 ft, offset(rs) */
 void mips_swc1(MipsEmitter *em, MipsFpuReg ft, int16_t offset, MipsReg rs);
 
 /* MTC1 rt, fs (move to coprocessor 1) */
 void mips_mtc1(MipsEmitter *em, MipsReg rt, MipsFpuReg fs);
 
 /* MFC1 rt, fs (move from coprocessor 1) */
 void mips_mfc1(MipsEmitter *em, MipsReg rt, MipsFpuReg fs);
 
 /* ADD.S fd, fs, ft */
 void mips_add_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs, MipsFpuReg ft);
 
 /* SUB.S fd, fs, ft */
 void mips_sub_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs, MipsFpuReg ft);
 
 /* MUL.S fd, fs, ft */
 void mips_mul_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs, MipsFpuReg ft);
 
 /* DIV.S fd, fs, ft */
 void mips_div_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs, MipsFpuReg ft);
 
 /* SQRT.S fd, fs */
 void mips_sqrt_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs);
 
 /* ABS.S fd, fs */
 void mips_abs_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs);
 
 /* NEG.S fd, fs */
 void mips_neg_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs);
 
 /* MOV.S fd, fs */
 void mips_mov_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs);
 
 /* CVT.S.W fd, fs (convert word to single) */
 void mips_cvt_s_w(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs);
 
 /* CVT.W.S fd, fs (convert single to word) */
 void mips_cvt_w_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs);
 
 /* C.EQ.S fs, ft (compare equal) */
 void mips_c_eq_s(MipsEmitter *em, MipsFpuReg fs, MipsFpuReg ft);
 
 /* C.LT.S fs, ft (compare less than) */
 void mips_c_lt_s(MipsEmitter *em, MipsFpuReg fs, MipsFpuReg ft);
 
 /* C.LE.S fs, ft (compare less or equal) */
 void mips_c_le_s(MipsEmitter *em, MipsFpuReg fs, MipsFpuReg ft);
 
 /* BC1T label (branch if FPU condition true) */
 void mips_bc1t(MipsEmitter *em, int label_id);
 
 /* BC1F label (branch if FPU condition false) */
 void mips_bc1f(MipsEmitter *em, int label_id);
 
 /* MIN.S fd, fs, ft - R5900 specific: fd = min(fs, ft) */
 void mips_min_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs, MipsFpuReg ft);
 
 /* MAX.S fd, fs, ft - R5900 specific: fd = max(fs, ft) */
 void mips_max_s(MipsEmitter *em, MipsFpuReg fd, MipsFpuReg fs, MipsFpuReg ft);
 
 /* --- Pseudo-instructions --- */
 
 /* NOP (encoded as SLL $zero, $zero, 0) */
 void mips_nop(MipsEmitter *em);
 
 /* LI rt, imm32 (load 32-bit immediate) */
 void mips_li(MipsEmitter *em, MipsReg rt, int32_t imm);
 
 /* LA rt, addr (load address) */
 void mips_la(MipsEmitter *em, MipsReg rt, void *addr);
 
 /* MOVE rd, rs (copy register) */
 void mips_move(MipsEmitter *em, MipsReg rd, MipsReg rs);
 
 /* --- Utility functions --- */
 
 /* Emit raw 32-bit instruction */
 void mips_emit_raw(MipsEmitter *em, uint32_t instr);
 
/* Get current code offset (in instructions) */
size_t mips_get_offset(MipsEmitter *em);

/* Dump generated assembly code for debugging */
void mips_emitter_dump(MipsEmitter *em, const char *label);

#ifdef __cplusplus
}
#endif

#endif /* MIPS_EMITTER_H */