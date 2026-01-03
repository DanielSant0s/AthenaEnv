/*
 * AthenaEnv Native Compiler - Native String Library
 * 
 * Copyright (c) 2025 AthenaEnv Project
 * 
 * String operations for native compiled code.
 */

#ifndef NATIVE_STRING_H
#define NATIVE_STRING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Native string structure (owned)
 */
typedef struct NativeString {
    char *data;         /* String data (null-terminated) */
    size_t length;      /* Current length (excluding null terminator) */
    size_t capacity;    /* Allocated capacity */
} NativeString;

/*
 * String view (non-owning)
 */
typedef struct NativeStringView {
    const char *data;   /* Pointer to string data */
    size_t length;      /* Length of view */
} NativeStringView;

/* ============================================
 * String Lifecycle
 * ============================================ */

/* Create new string with initial capacity */
NativeString* native_string_new(size_t initial_capacity);

/* Create string from C string */
NativeString* native_string_from_cstr(const char *cstr);

/* Free string */
void native_string_free(NativeString *str);

/* Clone string */
NativeString* native_string_clone(const NativeString *str);

/* ============================================
 * String Operations
 * ============================================ */

/* Concatenate two strings, returns new string */
NativeString* native_string_concat(const NativeString *a, const NativeString *b);

/* Append string to existing string (mutates) */
void native_string_append(NativeString *dest, const NativeString *src);

/* Get substring [start, end) */
NativeString* native_string_slice(const NativeString *str, size_t start, size_t end);

/* Compare strings (returns -1, 0, 1) */
int native_string_compare(const NativeString *a, const NativeString *b);

/* Check equality */
bool native_string_equals(const NativeString *a, const NativeString *b);

/* Get character at index */
char native_string_char_at(const NativeString *str, size_t index);

/* Find substring (returns index or -1) */
int native_string_find(const NativeString *str, const NativeString *substr);

/* Replace all occurrences */
NativeString* native_string_replace(const NativeString *str, const NativeString *find, const NativeString *replace);

/* Convert to uppercase */
NativeString* native_string_to_upper(const NativeString *str);

/* Convert to lowercase */
NativeString* native_string_to_lower(const NativeString *str);

/* Trim whitespace */
NativeString* native_string_trim(const NativeString *str);

/* ============================================
 * String View Operations
 * ============================================ */

/* Create view from string */
NativeStringView native_string_view(const NativeString *str);

/* Create view from C string */
NativeStringView native_string_view_from_cstr(const char *cstr);

/* Compare views */
int native_string_view_compare(NativeStringView a, NativeStringView b);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_STRING_H */
