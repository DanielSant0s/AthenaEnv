/*
 * AthenaEnv Native Compiler - Type Definitions
 * 
 * Copyright (c) 2025 AthenaEnv Project
 * 
 * Type system for the AOT native compiler.
 */

#ifndef NATIVE_TYPES_H
#define NATIVE_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Native type enumeration
 */
typedef enum NativeType {
    NATIVE_TYPE_UNKNOWN = 0,
    NATIVE_TYPE_VOID,
    
    /* Integer types */
    NATIVE_TYPE_INT32,
    NATIVE_TYPE_UINT32,
    NATIVE_TYPE_INT64,
    NATIVE_TYPE_UINT64,
    NATIVE_TYPE_BOOL,
    
    /* Floating point types */
    NATIVE_TYPE_FLOAT32,
    
    /* Array types (fixed-size) */
    NATIVE_TYPE_INT32_ARRAY,
    NATIVE_TYPE_UINT32_ARRAY,
    NATIVE_TYPE_FLOAT32_ARRAY,
    
    /* Dynamic array types */
    NATIVE_TYPE_DYNAMIC_INT32_ARRAY,
    NATIVE_TYPE_DYNAMIC_UINT32_ARRAY,
    NATIVE_TYPE_DYNAMIC_FLOAT32_ARRAY,
    
    /* String types */
    NATIVE_TYPE_STRING,         /* Owned string (ptr, length, capacity) */
    NATIVE_TYPE_STRING_VIEW,    /* Non-owning string view (ptr, length) */
    
    /* Pointer type */
    NATIVE_TYPE_PTR,
    
    NATIVE_TYPE_COUNT
} NativeType;

/*
 * Type checking macros
 */
#define NATIVE_TYPE_IS_INT(t) \
    ((t) == NATIVE_TYPE_INT32 || (t) == NATIVE_TYPE_UINT32 || \
     (t) == NATIVE_TYPE_INT64 || (t) == NATIVE_TYPE_UINT64 || (t) == NATIVE_TYPE_BOOL)

#define NATIVE_TYPE_IS_INT64(t) \
    ((t) == NATIVE_TYPE_INT64 || (t) == NATIVE_TYPE_UINT64)

#define NATIVE_TYPE_IS_FLOAT(t) \
    ((t) == NATIVE_TYPE_FLOAT32)

#define NATIVE_TYPE_IS_ARRAY(t) \
    ((t) == NATIVE_TYPE_INT32_ARRAY || \
     (t) == NATIVE_TYPE_UINT32_ARRAY || \
     (t) == NATIVE_TYPE_FLOAT32_ARRAY)

#define NATIVE_TYPE_IS_DYNAMIC_ARRAY(t) \
    ((t) == NATIVE_TYPE_DYNAMIC_INT32_ARRAY || \
     (t) == NATIVE_TYPE_DYNAMIC_UINT32_ARRAY || \
     (t) == NATIVE_TYPE_DYNAMIC_FLOAT32_ARRAY)

#define NATIVE_TYPE_IS_STRING(t) \
    ((t) == NATIVE_TYPE_STRING || (t) == NATIVE_TYPE_STRING_VIEW)

/*
 * Function signature
 */
#define MAX_NATIVE_ARGS 8

typedef struct NativeFuncSignature {
    NativeType arg_types[MAX_NATIVE_ARGS];
    int arg_count;
    NativeType return_type;
} NativeFuncSignature;

/*
 * Compiled native function
 */
typedef struct NativeFunc {
    void *code_ptr;              /* Pointer to generated machine code */
    size_t code_size;             /* Size of generated code in bytes */
    NativeFuncSignature sig;      /* Function signature */
    bool is_valid;                /* Whether the function is valid */
} NativeFunc;

/*
 * Type check result
 */
typedef enum TypeCheckResult {
    TYPE_CHECK_OK = 0,
    TYPE_CHECK_ERROR = 1,
    TYPE_CHECK_UNKNOWN = 2
} TypeCheckResult;

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_TYPES_H */

