#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "quickjs/quickjs-libc.h"

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

JSModuleDef *athena_push_module(JSContext* ctx, JSModuleInitFunc *func, const JSCFunctionListEntry *func_list, int len, const char* module_name);

JSModuleDef *athena_system_init(JSContext* ctx);
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