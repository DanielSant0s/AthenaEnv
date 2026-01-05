/*
 * AthenaEnv Native Compiler - Dynamic Array Library
 * 
 * Copyright (c) 2025 AthenaEnv Project
 * 
 * Dynamic arrays with automatic resizing.
 */

#ifndef NATIVE_ARRAY_H
#define NATIVE_ARRAY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "native_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dynamic array structure
 */
typedef struct NativeDynamicArray {
    void *data;              /* Array data */
    size_t length;           /* Current number of elements */
    size_t capacity;         /* Allocated capacity */
    NativeType element_type; /* Type of elements */
    size_t element_size;     /* Size of each element in bytes */
} NativeDynamicArray;

/* ============================================
 * Array Lifecycle
 * ============================================ */

/* Create new dynamic array */
NativeDynamicArray* native_array_new(NativeType element_type, size_t initial_capacity);

/* Free array */
void native_array_free(NativeDynamicArray *arr);

/* Clone array */
NativeDynamicArray* native_array_clone(const NativeDynamicArray *arr);

/* ============================================
 * Array Operations
 * ============================================ */

/* Reserve capacity (pre-allocate) */
bool native_array_reserve(NativeDynamicArray *arr, size_t new_capacity);

/* Resize array to exact length */
bool native_array_resize(NativeDynamicArray *arr, size_t new_length);

/* Push element to end */
bool native_array_push_i32(NativeDynamicArray *arr, int32_t value);
bool native_array_push_u32(NativeDynamicArray *arr, uint32_t value);
bool native_array_push_f32(NativeDynamicArray *arr, float value);

/* Pop element from end */
bool native_array_pop_i32(NativeDynamicArray *arr, int32_t *out_value);
bool native_array_pop_u32(NativeDynamicArray *arr, uint32_t *out_value);
bool native_array_pop_f32(NativeDynamicArray *arr, float *out_value);

/* Get element at index */
bool native_array_get_i32(const NativeDynamicArray *arr, size_t index, int32_t *out_value);
bool native_array_get_u32(const NativeDynamicArray *arr, size_t index, uint32_t *out_value);
bool native_array_get_f32(const NativeDynamicArray *arr, size_t index, float *out_value);

/* Set element at index */
bool native_array_set_i32(NativeDynamicArray *arr, size_t index, int32_t value);
bool native_array_set_u32(NativeDynamicArray *arr, size_t index, uint32_t value);
bool native_array_set_f32(NativeDynamicArray *arr, size_t index, float value);

/* Insert element at index */
bool native_array_insert_i32(NativeDynamicArray *arr, size_t index, int32_t value);

/* Remove element at index */
bool native_array_remove(NativeDynamicArray *arr, size_t index);

/* Clear array (set length to 0) */
void native_array_clear(NativeDynamicArray *arr);

/* ============================================
 * Utility Functions
 * ============================================ */

/* Get element size for type */
size_t native_array_element_size(NativeType type);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_ARRAY_H */
