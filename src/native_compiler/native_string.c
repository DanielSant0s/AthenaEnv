/*
 * AthenaEnv Native Compiler - Native String Library Implementation
 * 
 * Copyright (c) 2025 AthenaEnv Project
 */

#include "native_string.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define STRING_INITIAL_CAPACITY 16
#define STRING_GROWTH_FACTOR 2

/* ============================================
 * String Lifecycle
 * ============================================ */

NativeString* native_string_new(size_t initial_capacity) {
    NativeString *str = (NativeString*)malloc(sizeof(NativeString));
    if (!str) return NULL;
    
    if (initial_capacity < STRING_INITIAL_CAPACITY) {
        initial_capacity = STRING_INITIAL_CAPACITY;
    }
    
    str->data = (char*)malloc(initial_capacity);
    if (!str->data) {
        free(str);
        return NULL;
    }
    
    str->data[0] = '\0';
    str->length = 0;
    str->capacity = initial_capacity;
    
    return str;
}

NativeString* native_string_from_cstr(const char *cstr) {
    if (!cstr) return native_string_new(0);
    
    size_t len = strlen(cstr);
    NativeString *str = native_string_new(len + 1);
    if (!str) return NULL;
    
    memcpy(str->data, cstr, len + 1);
    str->length = len;
    
    return str;
}

void native_string_free(NativeString *str) {
    if (str) {
        if (str->data) free(str->data);
        free(str);
    }
}

NativeString* native_string_clone(const NativeString *str) {
    if (!str) return NULL;
    return native_string_from_cstr(str->data);
}

/* ============================================
 * String Operations
 * ============================================ */

static bool ensure_capacity(NativeString *str, size_t required) {
    if (str->capacity >= required) return true;
    
    size_t new_capacity = str->capacity * STRING_GROWTH_FACTOR;
    if (new_capacity < required) {
        new_capacity = required * STRING_GROWTH_FACTOR;
    }
    
    char *new_data = (char*)realloc(str->data, new_capacity);
    if (!new_data) return false;
    
    str->data = new_data;
    str->capacity = new_capacity;
    return true;
}

NativeString* native_string_concat(const NativeString *a, const NativeString *b) {
    if (!a || !b) return NULL;
    
    size_t total_len = a->length + b->length;
    NativeString *result = native_string_new(total_len + 1);
    if (!result) return NULL;
    
    memcpy(result->data, a->data, a->length);
    memcpy(result->data + a->length, b->data, b->length + 1);
    result->length = total_len;
    
    return result;
}

void native_string_append(NativeString *dest, const NativeString *src) {
    if (!dest || !src) return;
    
    size_t new_len = dest->length + src->length;
    if (!ensure_capacity(dest, new_len + 1)) return;
    
    memcpy(dest->data + dest->length, src->data, src->length + 1);
    dest->length = new_len;
}

NativeString* native_string_slice(const NativeString *str, size_t start, size_t end) {
    if (!str || start >= str->length) {
        return native_string_new(0);
    }
    
    if (end > str->length) end = str->length;
    if (start >= end) return native_string_new(0);
    
    size_t slice_len = end - start;
    NativeString *result = native_string_new(slice_len + 1);
    if (!result) return NULL;
    
    memcpy(result->data, str->data + start, slice_len);
    result->data[slice_len] = '\0';
    result->length = slice_len;
    
    return result;
}

int native_string_compare(const NativeString *a, const NativeString *b) {
    if (!a || !b) return 0;
    return strcmp(a->data, b->data);
}

bool native_string_equals(const NativeString *a, const NativeString *b) {
    if (!a || !b) return false;
    if (a->length != b->length) return false;
    return memcmp(a->data, b->data, a->length) == 0;
}

char native_string_char_at(const NativeString *str, size_t index) {
    if (!str || index >= str->length) return '\0';
    return str->data[index];
}

int native_string_find(const NativeString *str, const NativeString *substr) {
    if (!str || !substr || substr->length > str->length) return -1;
    
    char *found = strstr(str->data, substr->data);
    if (!found) return -1;
    
    return (int)(found - str->data);
}

NativeString* native_string_replace(const NativeString *str, const NativeString *find, const NativeString *replace) {
    if (!str || !find || !replace || find->length == 0) {
        return native_string_clone(str);
    }
    
    // Count occurrences
    int count = 0;
    const char *pos = str->data;
    while ((pos = strstr(pos, find->data)) != NULL) {
        count++;
        pos += find->length;
    }
    
    if (count == 0) return native_string_clone(str);
    
    // Calculate new length
    size_t new_len = str->length + count * (replace->length - find->length);
    NativeString *result = native_string_new(new_len + 1);
    if (!result) return NULL;
    
    // Perform replacement
    const char *src = str->data;
    char *dst = result->data;
    
    while ((pos = strstr(src, find->data)) != NULL) {
        size_t prefix_len = pos - src;
        memcpy(dst, src, prefix_len);
        dst += prefix_len;
        
        memcpy(dst, replace->data, replace->length);
        dst += replace->length;
        
        src = pos + find->length;
    }
    
    // Copy remaining
    strcpy(dst, src);
    result->length = new_len;
    
    return result;
}

NativeString* native_string_to_upper(const NativeString *str) {
    if (!str) return NULL;
    
    NativeString *result = native_string_clone(str);
    if (!result) return NULL;
    
    for (size_t i = 0; i < result->length; i++) {
        result->data[i] = toupper(result->data[i]);
    }
    
    return result;
}

NativeString* native_string_to_lower(const NativeString *str) {
    if (!str) return NULL;
    
    NativeString *result = native_string_clone(str);
    if (!result) return NULL;
    
    for (size_t i = 0; i < result->length; i++) {
        result->data[i] = tolower(result->data[i]);
    }
    
    return result;
}

NativeString* native_string_trim(const NativeString *str) {
    if (!str || str->length == 0) {
        return native_string_new(0);
    }
    
    // Find start
    size_t start = 0;
    while (start < str->length && isspace(str->data[start])) {
        start++;
    }
    
    // Find end
    size_t end = str->length;
    while (end > start && isspace(str->data[end - 1])) {
        end--;
    }
    
    return native_string_slice(str, start, end);
}

/* ============================================
 * String View Operations
 * ============================================ */

NativeStringView native_string_view(const NativeString *str) {
    NativeStringView view = {NULL, 0};
    if (str) {
        view.data = str->data;
        view.length = str->length;
    }
    return view;
}

NativeStringView native_string_view_from_cstr(const char *cstr) {
    NativeStringView view = {NULL, 0};
    if (cstr) {
        view.data = cstr;
        view.length = strlen(cstr);
    }
    return view;
}

int native_string_view_compare(NativeStringView a, NativeStringView b) {
    size_t min_len = a.length < b.length ? a.length : b.length;
    int cmp = memcmp(a.data, b.data, min_len);
    
    if (cmp != 0) return cmp;
    
    if (a.length < b.length) return -1;
    if (a.length > b.length) return 1;
    return 0;
}
