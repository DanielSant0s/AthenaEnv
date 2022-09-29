#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "duktape/duktape.h"
#include "duktape/duk_console.h"
#include "duktape/duk_module_node.h"
#include "include/taskman.h"

extern char boot_path[255];

const char* runScript(const char* script, bool isBuffer );
void push_athena_module(duk_c_function func, const char *key);

void athena_system_init(duk_context* ctx);
void athena_render_init(duk_context* ctx);
void athena_screen_init(duk_context* ctx);
void athena_graphics_init(duk_context* ctx);
void athena_imagelist_init(duk_context* ctx);
void athena_pads_init(duk_context* ctx);
void athena_sound_init(duk_context* ctx);
void athena_timer_init(duk_context* ctx);
void athena_task_init(duk_context* ctx);

float get_obj_float(duk_context* ctx, duk_idx_t idx, const char* key);
uint32_t get_obj_uint(duk_context* ctx, duk_idx_t idx, const char* key);
bool get_obj_boolean(duk_context* ctx, duk_idx_t idx, const char* key);