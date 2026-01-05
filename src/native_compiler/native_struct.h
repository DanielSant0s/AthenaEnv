/**
 * native_struct.h - Structure definitions for Native.struct
 * 
 * Allows JavaScript to define and access C-like structures.
 */

#ifndef NATIVE_STRUCT_H
#define NATIVE_STRUCT_H

#include <stdint.h>
#include <stddef.h>
#include "native_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum fields per struct */
#define MAX_STRUCT_FIELDS 32

/* Maximum nested depth */
#define MAX_STRUCT_DEPTH 4

/* Field flags */
#define FIELD_FLAG_ARRAY    0x01  /* Field is an array */
#define FIELD_FLAG_NESTED   0x02  /* Field is a nested struct */

/**
 * Struct field definition
 */
typedef struct NativeStructField {
    char name[32];              /* Field name */
    NativeType type;            /* Field type (primitive or NATIVE_TYPE_PTR for nested) */
    uint16_t offset;            /* Byte offset from struct base */
    uint16_t size;              /* Size in bytes */
    uint16_t array_count;       /* Number of elements (1 for non-arrays) */
    uint8_t flags;              /* FIELD_FLAG_* */
    uint8_t alignment;          /* Required alignment */
    struct NativeStructDef *nested_def;  /* For nested structs */
} NativeStructField;

/**
 * Struct type definition
 */
typedef struct NativeStructDef {
    char name[64];              /* Struct type name */
    uint16_t field_count;       /* Number of fields */
    uint16_t size;              /* Total size in bytes (aligned) */
    uint16_t alignment;         /* Struct alignment requirement */
    uint16_t ref_count;         /* Reference count for GC */
    NativeStructField fields[MAX_STRUCT_FIELDS];
} NativeStructDef;

/**
 * Struct instance (allocated memory)
 */
typedef struct NativeStructInstance {
    NativeStructDef *def;       /* Type definition */
    void *data;                 /* Pointer to struct data */
    uint8_t flags;              /* Instance flags */
    uint8_t managed;            /* 1 = GC managed, 0 = manual */
} NativeStructInstance;

/*
 * API Functions
 */

/* Create a new struct definition */
NativeStructDef *native_struct_create_def(const char *name);

/* Add a primitive field */
int native_struct_add_field(NativeStructDef *def, const char *name, 
                            NativeType type, int array_count);

/* Add a nested struct field */
int native_struct_add_nested(NativeStructDef *def, const char *name,
                             NativeStructDef *nested_def, int array_count);

/* Finalize the struct definition (calculate offsets, alignment) */
int native_struct_finalize(NativeStructDef *def);

/* Create a new instance of a struct */
NativeStructInstance *native_struct_alloc(NativeStructDef *def, int managed);

/* Free a struct instance */
void native_struct_free(NativeStructInstance *inst);

/* Get field by name */
NativeStructField *native_struct_get_field(NativeStructDef *def, const char *name);

/* Get field offset by name (returns -1 if not found) */
int native_struct_get_offset(NativeStructDef *def, const char *name);

/* Read a field value */
int32_t native_struct_read_i32(NativeStructInstance *inst, const char *field);
float native_struct_read_f32(NativeStructInstance *inst, const char *field);
int64_t native_struct_read_i64(NativeStructInstance *inst, const char *field);

/* Write a field value */
void native_struct_write_i32(NativeStructInstance *inst, const char *field, int32_t value);
void native_struct_write_f32(NativeStructInstance *inst, const char *field, float value);
void native_struct_write_i64(NativeStructInstance *inst, const char *field, int64_t value);

/* ============================================
 * Global Struct Registry
 * For compiler field access lookup
 * ============================================ */

#define MAX_REGISTERED_STRUCTS 64

/* Register a struct definition globally */
int native_struct_register(NativeStructDef *def);

/* Find a struct by name */
NativeStructDef *native_struct_find(const char *name);

/* Get all registered struct defs (for compiler) */
NativeStructDef **native_struct_get_all(int *count);

/* Clear the registry */
void native_struct_clear_registry(void);

/* native_type_size and native_type_alignment are declared in native_compiler.h */

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_STRUCT_H */
