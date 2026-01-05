/*
 * AthenaEnv Native Compiler - Dynamic Array Implementation
 * 
 * Copyright (c) 2025 AthenaEnv Project
 */

#include "native_array.h"
#include <stdlib.h>
#include <string.h>

#define ARRAY_INITIAL_CAPACITY 16
#define ARRAY_GROWTH_FACTOR 2

/* ============================================
 * Utility Functions
 * ============================================ */

size_t native_array_element_size(NativeType type) {
    switch (type) {
        case NATIVE_TYPE_INT32:
        case NATIVE_TYPE_UINT32:
        case NATIVE_TYPE_FLOAT32:
            return 4;
        case NATIVE_TYPE_INT64:
        case NATIVE_TYPE_UINT64:
            return 8;
        case NATIVE_TYPE_BOOL:
            return 1;
        case NATIVE_TYPE_PTR:
            return sizeof(void*);
        default:
            return 0;
    }
}

/* ============================================
 * Array Lifecycle
 * ============================================ */

NativeDynamicArray* native_array_new(NativeType element_type, size_t initial_capacity) {
    NativeDynamicArray *arr = (NativeDynamicArray*)malloc(sizeof(NativeDynamicArray));
    if (!arr) return NULL;
    
    if (initial_capacity < ARRAY_INITIAL_CAPACITY) {
        initial_capacity = ARRAY_INITIAL_CAPACITY;
    }
    
    size_t elem_size = native_array_element_size(element_type);
    if (elem_size == 0) {
        free(arr);
        return NULL;
    }
    
    arr->data = malloc(initial_capacity * elem_size);
    if (!arr->data) {
        free(arr);
        return NULL;
    }
    
    arr->length = 0;
    arr->capacity = initial_capacity;
    arr->element_type = element_type;
    arr->element_size = elem_size;
    
    return arr;
}

void native_array_free(NativeDynamicArray *arr) {
    if (arr) {
        if (arr->data) free(arr->data);
        free(arr);
    }
}

NativeDynamicArray* native_array_clone(const NativeDynamicArray *arr) {
    if (!arr) return NULL;
    
    NativeDynamicArray *clone = native_array_new(arr->element_type, arr->capacity);
    if (!clone) return NULL;
    
    memcpy(clone->data, arr->data, arr->length * arr->element_size);
    clone->length = arr->length;
    
    return clone;
}

/* ============================================
 * Array Operations
 * ============================================ */

bool native_array_reserve(NativeDynamicArray *arr, size_t new_capacity) {
    if (!arr || new_capacity <= arr->capacity) return true;
    
    void *new_data = realloc(arr->data, new_capacity * arr->element_size);
    if (!new_data) return false;
    
    arr->data = new_data;
    arr->capacity = new_capacity;
    return true;
}

static bool ensure_capacity(NativeDynamicArray *arr, size_t required) {
    if (arr->capacity >= required) return true;
    
    size_t new_capacity = arr->capacity * ARRAY_GROWTH_FACTOR;
    if (new_capacity < required) {
        new_capacity = required * ARRAY_GROWTH_FACTOR;
    }
    
    return native_array_reserve(arr, new_capacity);
}

bool native_array_resize(NativeDynamicArray *arr, size_t new_length) {
    if (!arr) return false;
    
    if (new_length > arr->capacity) {
        if (!ensure_capacity(arr, new_length)) return false;
    }
    
    // Zero out new elements if growing
    if (new_length > arr->length) {
        size_t added = new_length - arr->length;
        memset((char*)arr->data + arr->length * arr->element_size, 0, added * arr->element_size);
    }
    
    arr->length = new_length;
    return true;
}

/* Push operations */

bool native_array_push_i32(NativeDynamicArray *arr, int32_t value) {
    if (!arr || arr->element_type != NATIVE_TYPE_INT32) return false;
    if (!ensure_capacity(arr, arr->length + 1)) return false;
    
    ((int32_t*)arr->data)[arr->length++] = value;
    return true;
}

bool native_array_push_u32(NativeDynamicArray *arr, uint32_t value) {
    if (!arr || arr->element_type != NATIVE_TYPE_UINT32) return false;
    if (!ensure_capacity(arr, arr->length + 1)) return false;
    
    ((uint32_t*)arr->data)[arr->length++] = value;
    return true;
}

bool native_array_push_f32(NativeDynamicArray *arr, float value) {
    if (!arr || arr->element_type != NATIVE_TYPE_FLOAT32) return false;
    if (!ensure_capacity(arr, arr->length + 1)) return false;
    
    ((float*)arr->data)[arr->length++] = value;
    return true;
}

/* Pop operations */

bool native_array_pop_i32(NativeDynamicArray *arr, int32_t *out_value) {
    if (!arr || arr->length == 0 || arr->element_type != NATIVE_TYPE_INT32) return false;
    
    arr->length--;
    if (out_value) {
        *out_value = ((int32_t*)arr->data)[arr->length];
    }
    return true;
}

bool native_array_pop_u32(NativeDynamicArray *arr, uint32_t *out_value) {
    if (!arr || arr->length == 0 || arr->element_type != NATIVE_TYPE_UINT32) return false;
    
    arr->length--;
    if (out_value) {
        *out_value = ((uint32_t*)arr->data)[arr->length];
    }
    return true;
}

bool native_array_pop_f32(NativeDynamicArray *arr, float *out_value) {
    if (!arr || arr->length == 0 || arr->element_type != NATIVE_TYPE_FLOAT32) return false;
    
    arr->length--;
    if (out_value) {
        *out_value = ((float*)arr->data)[arr->length];
    }
    return true;
}

/* Get operations */

bool native_array_get_i32(const NativeDynamicArray *arr, size_t index, int32_t *out_value) {
    if (!arr || index >= arr->length || arr->element_type != NATIVE_TYPE_INT32) return false;
    
    if (out_value) {
        *out_value = ((int32_t*)arr->data)[index];
    }
    return true;
}

bool native_array_get_u32(const NativeDynamicArray *arr, size_t index, uint32_t *out_value) {
    if (!arr || index >= arr->length || arr->element_type != NATIVE_TYPE_UINT32) return false;
    
    if (out_value) {
        *out_value = ((uint32_t*)arr->data)[index];
    }
    return true;
}

bool native_array_get_f32(const NativeDynamicArray *arr, size_t index, float *out_value) {
    if (!arr || index >= arr->length || arr->element_type != NATIVE_TYPE_FLOAT32) return false;
    
    if (out_value) {
        *out_value = ((float*)arr->data)[index];
    }
    return true;
}

/* Set operations */

bool native_array_set_i32(NativeDynamicArray *arr, size_t index, int32_t value) {
    if (!arr || index >= arr->length || arr->element_type != NATIVE_TYPE_INT32) return false;
    
    ((int32_t*)arr->data)[index] = value;
    return true;
}

bool native_array_set_u32(NativeDynamicArray *arr, size_t index, uint32_t value) {
    if (!arr || index >= arr->length || arr->element_type != NATIVE_TYPE_UINT32) return false;
    
    ((uint32_t*)arr->data)[index] = value;
    return true;
}

bool native_array_set_f32(NativeDynamicArray *arr, size_t index, float value) {
    if (!arr || index >= arr->length || arr->element_type != NATIVE_TYPE_FLOAT32) return false;
    
    ((float*)arr->data)[index] = value;
    return true;
}

/* Insert operation */

bool native_array_insert_i32(NativeDynamicArray *arr, size_t index, int32_t value) {
    if (!arr || index > arr->length || arr->element_type != NATIVE_TYPE_INT32) return false;
    
    if (!ensure_capacity(arr, arr->length + 1)) return false;
    
    // Shift elements right
    if (index < arr->length) {
        memmove(&((int32_t*)arr->data)[index + 1],
                &((int32_t*)arr->data)[index],
                (arr->length - index) * arr->element_size);
    }
    
    ((int32_t*)arr->data)[index] = value;
    arr->length++;
    return true;
}

/* Remove operation */

bool native_array_remove(NativeDynamicArray *arr, size_t index) {
    if (!arr || index >= arr->length) return false;
    
    // Shift elements left
    if (index < arr->length - 1) {
        memmove((char*)arr->data + index * arr->element_size,
                (char*)arr->data + (index + 1) * arr->element_size,
                (arr->length - index - 1) * arr->element_size);
    }
    
    arr->length--;
    return true;
}

/* Clear operation */

void native_array_clear(NativeDynamicArray *arr) {
    if (arr) {
        arr->length = 0;
    }
}
