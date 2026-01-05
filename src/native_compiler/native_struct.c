/**
 * native_struct.c - Structure implementation for Native.struct
 */

#include "native_struct.h"
#include "native_compiler.h"  /* For native_type_size, native_type_alignment */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Align offset to boundary
 */
static uint16_t align_offset(uint16_t offset, uint16_t alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
}

/*
 * Create a new struct definition
 */
NativeStructDef *native_struct_create_def(const char *name) {
    NativeStructDef *def = (NativeStructDef *)calloc(1, sizeof(NativeStructDef));
    if (!def) return NULL;
    
    if (name) {
        strncpy(def->name, name, sizeof(def->name) - 1);
    }
    def->field_count = 0;
    def->size = 0;
    def->alignment = 1;
    def->ref_count = 1;
    
    return def;
}

/*
 * Add a primitive field to struct
 */
int native_struct_add_field(NativeStructDef *def, const char *name, 
                            NativeType type, int array_count) {
    if (!def || !name) return -1;
    if (def->field_count >= MAX_STRUCT_FIELDS) return -2;
    if (array_count < 1) array_count = 1;
    
    NativeStructField *field = &def->fields[def->field_count];
    
    strncpy(field->name, name, sizeof(field->name) - 1);
    field->type = type;
    field->array_count = array_count;
    field->size = native_type_size(type) * array_count;
    field->alignment = native_type_alignment(type);
    field->flags = (array_count > 1) ? FIELD_FLAG_ARRAY : 0;
    field->nested_def = NULL;
    field->offset = 0;  /* Will be calculated in finalize */
    
    def->field_count++;
    return 0;
}

/*
 * Add a nested struct field
 */
int native_struct_add_nested(NativeStructDef *def, const char *name,
                             NativeStructDef *nested_def, int array_count) {
    if (!def || !name || !nested_def) return -1;
    if (def->field_count >= MAX_STRUCT_FIELDS) return -2;
    if (array_count < 1) array_count = 1;
    
    NativeStructField *field = &def->fields[def->field_count];
    
    strncpy(field->name, name, sizeof(field->name) - 1);
    field->type = NATIVE_TYPE_PTR;  /* Marker for nested */
    field->array_count = array_count;
    field->size = nested_def->size * array_count;
    field->alignment = nested_def->alignment;
    field->flags = FIELD_FLAG_NESTED | ((array_count > 1) ? FIELD_FLAG_ARRAY : 0);
    field->nested_def = nested_def;
    field->offset = 0;
    
    nested_def->ref_count++;
    def->field_count++;
    return 0;
}

/*
 * Finalize struct definition - calculate offsets and total size
 */
int native_struct_finalize(NativeStructDef *def) {
    if (!def) return -1;
    
    uint16_t current_offset = 0;
    uint16_t max_alignment = 1;
    
    for (int i = 0; i < def->field_count; i++) {
        NativeStructField *field = &def->fields[i];
        
        /* Align current offset */
        current_offset = align_offset(current_offset, field->alignment);
        field->offset = current_offset;
        
        /* Advance offset */
        current_offset += field->size;
        
        /* Track max alignment */
        if (field->alignment > max_alignment) {
            max_alignment = field->alignment;
        }
    }
    
    /* Align final size to struct alignment */
    def->size = align_offset(current_offset, max_alignment);
    def->alignment = max_alignment;
    
    return 0;
}

/*
 * Allocate a struct instance
 */
NativeStructInstance *native_struct_alloc(NativeStructDef *def, int managed) {
    if (!def) return NULL;
    
    NativeStructInstance *inst = (NativeStructInstance *)calloc(1, sizeof(NativeStructInstance));
    if (!inst) return NULL;
    
    /* Allocate aligned memory for struct data */
    inst->data = aligned_alloc(def->alignment, def->size);
    if (!inst->data) {
        free(inst);
        return NULL;
    }
    
    memset(inst->data, 0, def->size);
    inst->def = def;
    inst->managed = managed ? 1 : 0;
    inst->flags = 0;
    
    def->ref_count++;
    return inst;
}

/*
 * Free a struct instance
 */
void native_struct_free(NativeStructInstance *inst) {
    if (!inst) return;
    
    if (inst->data) {
        free(inst->data);
    }
    
    if (inst->def) {
        inst->def->ref_count--;
    }
    
    free(inst);
}

/*
 * Get field by name
 */
NativeStructField *native_struct_get_field(NativeStructDef *def, const char *name) {
    if (!def || !name) return NULL;
    
    for (int i = 0; i < def->field_count; i++) {
        if (strcmp(def->fields[i].name, name) == 0) {
            return &def->fields[i];
        }
    }
    return NULL;
}

/*
 * Get field offset by name
 */
int native_struct_get_offset(NativeStructDef *def, const char *name) {
    NativeStructField *field = native_struct_get_field(def, name);
    if (!field) return -1;
    return field->offset;
}

/*
 * Read/Write field values
 */
int32_t native_struct_read_i32(NativeStructInstance *inst, const char *field_name) {
    NativeStructField *field = native_struct_get_field(inst->def, field_name);
    if (!field) return 0;
    return *(int32_t *)((uint8_t *)inst->data + field->offset);
}

float native_struct_read_f32(NativeStructInstance *inst, const char *field_name) {
    NativeStructField *field = native_struct_get_field(inst->def, field_name);
    if (!field) return 0.0f;
    return *(float *)((uint8_t *)inst->data + field->offset);
}

int64_t native_struct_read_i64(NativeStructInstance *inst, const char *field_name) {
    NativeStructField *field = native_struct_get_field(inst->def, field_name);
    if (!field) return 0;
    return *(int64_t *)((uint8_t *)inst->data + field->offset);
}

void native_struct_write_i32(NativeStructInstance *inst, const char *field_name, int32_t value) {
    NativeStructField *field = native_struct_get_field(inst->def, field_name);
    if (!field) return;
    *(int32_t *)((uint8_t *)inst->data + field->offset) = value;
}

void native_struct_write_f32(NativeStructInstance *inst, const char *field_name, float value) {
    NativeStructField *field = native_struct_get_field(inst->def, field_name);
    if (!field) return;
    *(float *)((uint8_t *)inst->data + field->offset) = value;
}

void native_struct_write_i64(NativeStructInstance *inst, const char *field_name, int64_t value) {
    NativeStructField *field = native_struct_get_field(inst->def, field_name);
    if (!field) return;
    *(int64_t *)((uint8_t *)inst->data + field->offset) = value;
}

/* ============================================
 * Global Struct Registry
 * ============================================ */

static NativeStructDef *g_struct_registry[MAX_REGISTERED_STRUCTS] = {0};
static int g_struct_count = 0;

int native_struct_register(NativeStructDef *def) {
    if (!def || g_struct_count >= MAX_REGISTERED_STRUCTS) return -1;
    
    /* Check if already registered */
    for (int i = 0; i < g_struct_count; i++) {
        if (g_struct_registry[i] == def) return i;
    }
    
    g_struct_registry[g_struct_count] = def;
    return g_struct_count++;
}

NativeStructDef *native_struct_find(const char *name) {
    if (!name) return NULL;
    
    for (int i = 0; i < g_struct_count; i++) {
        if (g_struct_registry[i] && strcmp(g_struct_registry[i]->name, name) == 0) {
            return g_struct_registry[i];
        }
    }
    return NULL;
}

NativeStructDef **native_struct_get_all(int *count) {
    if (count) *count = g_struct_count;
    return g_struct_registry;
}

void native_struct_clear_registry(void) {
    g_struct_count = 0;
    memset(g_struct_registry, 0, sizeof(g_struct_registry));
}
