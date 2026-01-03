/*
 * AthenaEnv Native Compiler - Int64 Runtime Header
 * 
 * Copyright (c) 2025 AthenaEnv Project
 * 
 * Software emulation of 64-bit multiply/divide/modulo for MIPS R5900.
 */

#ifndef INT64_RUNTIME_H
#define INT64_RUNTIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 64-bit signed operations */
int64_t __dmult_i64(int64_t a, int64_t b);
int64_t __ddiv_i64(int64_t dividend, int64_t divisor);
int64_t __dmod_i64(int64_t dividend, int64_t divisor);

/* 64-bit unsigned operations */
uint64_t __dmult_u64(uint64_t a, uint64_t b);
uint64_t __ddiv_u64(uint64_t dividend, uint64_t divisor);
uint64_t __dmod_u64(uint64_t dividend, uint64_t divisor);

#ifdef __cplusplus
}
#endif

#endif /* INT64_RUNTIME_H */
