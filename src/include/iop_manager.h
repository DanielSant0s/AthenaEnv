#ifndef IOP_MANAGER_H
#define IOP_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define EMPTY_ENTRY 0xFF

#define iopman_define_module(module)               \
    extern unsigned char module##_irx[] __attribute__((aligned(16))); \
    extern unsigned int size_##module##_irx

typedef int (*iopman_func)(void *);

typedef struct module_entry {
    char *name;
    uint8_t id;

    void *data;
    uint32_t size;

    int arglen;
    char *args;

    uint8_t dependencies[4];
    uint8_t incompatibilities[4];

    iopman_func prepare;
    void *prepare_args;

    iopman_func init;
    void *init_args;

    iopman_func end;
    void *end_args;

    bool start_at_boot;
    bool started;
} module_entry;

module_entry *iopman_search_module(const char *name);

module_entry *iopman_get_module(uint8_t id);

module_entry *iopman_get_modules(uint32_t *top);

module_entry *iopman_register_module(char* name, void *data, uint32_t size, uint8_t dependencies[4], void *init_func, void *end_func);

#define iopman_register_module_buffer(name, module, dependencies, init_func, end_func)               \
    iopman_register_module(name, module##_irx, size_##module##_irx, dependencies, init_func, end_func)

#define iopman_register_module_file(name, path, dependencies, init_func, end_func)               \
    iopman_register_module(name, (void*)path, 0, dependencies, init_func, end_func)

#define iopman_set_module_args(module, arg_len, arg) \
    module->arglen = arg_len; \
    module->args = arg

#define iopman_set_prepare_function(module, func) module->prepare = func 

#define iopman_start_module_at_boot(module) module->start_at_boot = true 

void iopman_add_incompatible_module(module_entry *module, module_entry *incompatibility);

module_entry *iopman_get_incompatible_module();

#define MODULE_STATUS_INCOMPATIBILITY -1
#define MODULE_STATUS_LOADED 0
#define MODULE_STATUS_ERROR 1

int iopman_load_module(module_entry *module, int arglen, char *args);

void iopman_reset();

void iopman_modules_apply(iopman_func func);

#endif