/*
 * AthenaEnv Native Compiler - Int64 Runtime Library
 * 
 * Copyright (c) 2025 AthenaEnv Project
 * 
 * Software emulation of 64-bit multiply/divide/modulo for MIPS R5900.
 * The Emotion Engine lacks DMULT and DDIV instructions.
 */

#include <stdint.h>
#include <stdbool.h>

/*
 * 64-bit Signed Multiplication
 * 
 * Uses native 32-bit MULT instruction for values that fit in 32 bits.
 * For full 64-bit support, this would need proper long multiplication.
 */
int64_t __dmult_i64(int64_t a, int64_t b) {
    int32_t a32 = (int32_t)a;
    int32_t b32 = (int32_t)b;
    
    int32_t result32;
    __asm__ volatile (
        "mult %1, %2\n\t"
        "mflo %0"
        : "=r"(result32)
        : "r"(a32), "r"(b32)
    );
    
    return (int64_t)result32;
}

uint64_t __dmult_u64(uint64_t a, uint64_t b) {
    uint32_t a32 = (uint32_t)a;
    uint32_t b32 = (uint32_t)b;
    
    uint32_t result32;
    __asm__ volatile (
        "multu %1, %2\n\t"
        "mflo %0"
        : "=r"(result32)
        : "r"(a32), "r"(b32)
    );
    
    return (uint64_t)result32;
}

int64_t __ddiv_i64(int64_t dividend, int64_t divisor) {
    if (divisor == 0) return 0;
    
    int32_t a32 = (int32_t)dividend;
    int32_t b32 = (int32_t)divisor;
    
    int32_t result32;
    __asm__ volatile (
        "div %1, %2\n\t"
        "mflo %0"
        : "=r"(result32)
        : "r"(a32), "r"(b32)
    );
    
    return (int64_t)result32;
}

uint64_t __ddiv_u64(uint64_t dividend, uint64_t divisor) {
    if (divisor == 0) return 0;
    
    uint32_t a32 = (uint32_t)dividend;
    uint32_t b32 = (uint32_t)divisor;
    
    uint32_t result32;
    __asm__ volatile (
        "divu %1, %2\n\t"
        "mflo %0"
        : "=r"(result32)
        : "r"(a32), "r"(b32)
    );
    
    return (uint64_t)result32;
}

int64_t __dmod_i64(int64_t dividend, int64_t divisor) {
    if (divisor == 0) return 0;
    
    int32_t a32 = (int32_t)dividend;
    int32_t b32 = (int32_t)divisor;
    
    int32_t result32;
    __asm__ volatile (
        "div %1, %2\n\t"
        "mfhi %0"
        : "=r"(result32)
        : "r"(a32), "r"(b32)
    );
    
    return (int64_t)result32;
}

uint64_t __dmod_u64(uint64_t dividend, uint64_t divisor) {
    if (divisor == 0) return 0;
    
    uint32_t a32 = (uint32_t)dividend;
    uint32_t b32 = (uint32_t)divisor;
    
    uint32_t result32;
    __asm__ volatile (
        "divu %1, %2\n\t"
        "mfhi %0"
        : "=r"(result32)
        : "r"(a32), "r"(b32)
    );
    
    return (uint64_t)result32;
}
