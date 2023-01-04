#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "quickjs/quickjs-libc.h"

#include "duktape/duktape.h"
#include "duktape/duk_console.h"
#include "duktape/duk_module_node.h"
#include "include/taskman.h"
#include "include/graphics.h"

typedef struct {
    GSTEXTURE tex;
    Color color;
	double width;
	double height;
	double startx;
	double starty;
	double endx;
	double endy;
    double angle;
} JSImageData;

JSClassID get_img_class_id();

#define countof(x) (sizeof(x) / sizeof((x)[0]))

extern char boot_path[255];

const char* runScript(const char* script, bool isBuffer );

void athena_new_function(duk_context *ctx, duk_c_function func, const char* name);

JSModuleDef *athena_push_module(JSContext* ctx, JSModuleInitFunc *func, const JSCFunctionListEntry *func_list, int len, const char* module_name);

//JSModuleDef *athena_system_init(JSContext* ctx);
JSModuleDef *athena_render_init(JSContext* ctx);
JSModuleDef *athena_screen_init(JSContext* ctx);
JSModuleDef *athena_color_init(JSContext* ctx);
JSModuleDef *athena_shape_init(JSContext* ctx);
JSModuleDef *athena_font_init(JSContext* ctx);
JSModuleDef *athena_image_init(JSContext* ctx);
//JSModuleDef *athena_imagelist_init(JSContext* ctx);
JSModuleDef *athena_socket_init(JSContext* ctx);
JSModuleDef *athena_network_init(JSContext* ctx);
JSModuleDef *athena_pads_init(JSContext* ctx);
JSModuleDef *athena_sound_init(JSContext* ctx);
JSModuleDef *athena_timer_init(JSContext* ctx);
JSModuleDef *athena_task_init(JSContext* ctx);

float get_obj_float(duk_context* ctx, duk_idx_t idx, const char* key);
uint32_t get_obj_uint(duk_context* ctx, duk_idx_t idx, const char* key);
int get_obj_int(duk_context* ctx, duk_idx_t idx, const char* key);
bool get_obj_boolean(duk_context* ctx, duk_idx_t idx, const char* key);