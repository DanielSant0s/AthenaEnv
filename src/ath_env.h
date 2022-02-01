#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "duktape/duktape.h"
#include "duktape/duk_console.h"
#include "duktape/duk_module_node.h"

extern char boot_path[255];

extern const char* runScript(const char* script, bool isBuffer );
extern void push_athena_module(duk_c_function func, const char *key);

extern void athena_system_init(duk_context* ctx);
extern void athena_render_init(duk_context* ctx);
extern void athena_screen_init(duk_context* ctx);
extern void athena_graphics_init(duk_context* ctx);
extern void athena_pads_init(duk_context* ctx);
extern void athena_sound_init(duk_context* ctx);
extern void athena_timer_init(duk_context* ctx);
