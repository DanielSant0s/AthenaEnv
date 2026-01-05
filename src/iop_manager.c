#include <iop_manager.h>
#include <stdlib.h>
#include <string.h>

#include <sifrpc.h>
#include <loadfile.h>
#include <iopheap.h>
#include <iopcontrol.h>
#include <smod.h>

#include <sbv_patches.h>
#include <smem.h>

#include <dbgprintf.h>

#define MODULE_REGISTRY_SIZE 64

module_entry module_registry[MODULE_REGISTRY_SIZE] = { 0 };

uint32_t registry_entries = 0;

const uint8_t default_incompatibility_id_mask[4] = {
    EMPTY_ENTRY,
    EMPTY_ENTRY,
    EMPTY_ENTRY,
    EMPTY_ENTRY
};

module_entry *iopman_register_module(char* name, void *data, uint32_t size, uint8_t dependencies[4], void *init_func, void *end_func) {
    module_registry[registry_entries].id = registry_entries;

    module_registry[registry_entries].name = name;
    module_registry[registry_entries].data = data;
    module_registry[registry_entries].size = size;

    memcpy(&module_registry[registry_entries].dependencies, dependencies, 4);
    memcpy(&module_registry[registry_entries].incompatibilities, &default_incompatibility_id_mask, 4);
    
    module_registry[registry_entries].init = (iopman_func)init_func;
    module_registry[registry_entries].end = (iopman_func)end_func;

    return &module_registry[registry_entries++];
}

void iopman_add_incompatible_module(module_entry *module, module_entry *incompatibility) {
    for (uint8_t i = 0; i < 4; i++) {
        if (module->incompatibilities[i] == EMPTY_ENTRY) {
            module->incompatibilities[i] = incompatibility->id;
            for (uint8_t j = 0; j < 4; i++) {
                if (incompatibility->incompatibilities[j] == EMPTY_ENTRY) {
                    incompatibility->incompatibilities[j] = module->id;
                    return;
                }
            }
            return;
        }
    }
}

static module_entry *incompatible_module = NULL;

module_entry *iopman_get_incompatible_module() {
    return incompatible_module;
}

int iopman_load_module(module_entry *module, int arglen, char *args) {
    if (module->started)
        return MODULE_STATUS_LOADED;

    incompatible_module = NULL;
    for (uint8_t i = 0; i < 4; i++) {
        if (module->dependencies[i] != EMPTY_ENTRY)
            iopman_load_module(&module_registry[module->dependencies[i]], 0, NULL);

        if (module->incompatibilities[i] != EMPTY_ENTRY) {
            if (module_registry[module->incompatibilities[i]].started) {
                incompatible_module = &module_registry[module->incompatibilities[i]];
                return MODULE_STATUS_INCOMPATIBILITY;
            }
        }
    }

    int arg_length = arglen;
    char *arg_data = args;

    if (!arg_length || !arg_data) {
        arg_length = module->arglen;
        arg_data = module->args;
    }

    int id = 0, ret = 0;

    if (module->prepare) {
        if (module->prepare(module))
            return MODULE_STATUS_ERROR;
    }
        
    if (!module->size) {
        char *file_path = module->data;
        id = SifExecModuleFile(file_path, arg_length, arg_data, &ret);
    } else {
        id = SifExecModuleBuffer(module->data, module->size, arg_length, arg_data, &ret);
    }

    module->started = (id > 0 && ret != 1);

    if (module->init && id > 0) 
        module->init(module);

    return !(id > 0);
}

module_entry *iopman_search_module(const char *name) {
    for (int i = 0; i < registry_entries; i++) {
        if (!strcmp(module_registry[i].name, name)) {
            return &module_registry[i];
        }
    }

    return NULL;
}

module_entry *iopman_get_module(uint8_t id) {
    if (id < registry_entries) {
        return &module_registry[id];
    }

    return NULL;
}

module_entry *iopman_get_modules(uint32_t *top) {
    if (top) 
        *top = registry_entries;

    return module_registry;
}

void iopman_reset() {
    for (int i = 0; i < registry_entries; i++) {
        if (module_registry[i].started) {
            if (module_registry[i].end) {
                module_registry[i].end(&module_registry[i]);
            }
            module_registry[i].started = false;
        }
    }

    dbgprintf("AthenaEnv: Starting IOP Reset...\n");
    SifInitRpc(0);
    while (!SifIopReset("", 0)){};

    while (!SifIopSync()){};
    SifInitRpc(0);
    dbgprintf("AthenaEnv: IOP reset done.\n");

    SifLoadFileInit();

    // install sbv patch fix
    dbgprintf("AthenaEnv: Installing SBV Patches...\n");
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
}

void iopman_modules_apply(iopman_func func) {
    for (int i = 0; i < registry_entries; i++) {
        func(&module_registry[i]);
    }
}