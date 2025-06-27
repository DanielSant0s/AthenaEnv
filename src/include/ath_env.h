#ifndef ATH_ENV_H
#define ATH_ENV_H

#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <setjmp.h>

#include "../quickjs/quickjs-libc.h"

#ifdef ATHENA_GRAPHICS
#include <graphics.h>
#endif

#include <dbgprintf.h>

#include <macros.h>

#define ATHENA_PROP_INT32(item) 	JS_PROP_INT32_DEF(stringify(item),    item,   JS_PROP_CONFIGURABLE )

extern bool boot_logo, dark_mode;

#ifdef ATHENA_GRAPHICS
typedef struct {
	const char* path;
    GSSURFACE *tex;
	bool delayed;
	bool loaded;
    Color color;
	float width;
	float height;
	float startx;
	float starty;
	float endx;
	float endy;
    float angle;
} JSImageData;

typedef struct JSImgList {
    JSImageData** list;
	int size;
	int sema_id;
	int thread_id;
} JSImgList;

JSClassID get_img_class_id();
JSClassID get_imglist_class_id();
#endif

JSClassID get_matrix4_class_id();
JSClassID get_vector2_class_id();
JSClassID get_vector3_class_id();
JSClassID get_vector4_class_id();

#define countof(x) (sizeof(x) / sizeof((x)[0]))

extern char boot_path[255];

void init_bootlogo();
bool bootlogo_finished();

void poweroffHandler(void *arg);

const char* run_script(const char* script, bool isBuffer );
void destroy_vm(JSContext* ctx);
jmp_buf *get_reset_buf();
void set_default_script(const char* path);

JSModuleDef *athena_push_module(JSContext* ctx, JSModuleInitFunc *func, const JSCFunctionListEntry *func_list, int len, const char* module_name);

JSModuleDef *athena_system_init(JSContext* ctx);
JSModuleDef *athena_iop_init(JSContext* ctx);
JSModuleDef *athena_archive_init(JSContext* ctx);
JSModuleDef *athena_timer_init(JSContext* ctx);
JSModuleDef *athena_task_init(JSContext* ctx);
JSModuleDef *athena_pads_init(JSContext* ctx);

JSModuleDef *athena_vector_init(JSContext *ctx);
JSModuleDef *athena_vector4_init(JSContext *ctx);
JSModuleDef *athena_matrix_init(JSContext *ctx);
JSModuleDef *athena_physics_init(JSContext *ctx);

#ifdef ATHENA_GRAPHICS
JSModuleDef *athena_render_init(JSContext* ctx);
JSModuleDef *athena_lights_init(JSContext* ctx);
JSModuleDef *athena_3dcamera_init(JSContext* ctx);
JSModuleDef *athena_anim_3d_init(JSContext* ctx);
JSModuleDef *athena_screen_init(JSContext* ctx);
JSModuleDef *athena_color_init(JSContext* ctx);
JSModuleDef *athena_shape_init(JSContext* ctx);
JSModuleDef *athena_font_init(JSContext* ctx);
JSModuleDef *athena_image_init(JSContext* ctx);
JSModuleDef *athena_imagelist_init(JSContext* ctx);
JSModuleDef *athena_imagelist_init(JSContext* ctx);
JSModuleDef *athena_ode_init(JSContext* ctx);
#endif

#ifdef ATHENA_NETWORK
JSModuleDef *athena_socket_init(JSContext* ctx);
JSModuleDef *athena_network_init(JSContext* ctx);
JSModuleDef *athena_request_init(JSContext *ctx);
JSModuleDef *athena_ws_init(JSContext *ctx);
#endif

#ifdef ATHENA_KEYBOARD
JSModuleDef *athena_keyboard_init(JSContext* ctx);
#endif

#ifdef ATHENA_MOUSE
JSModuleDef *athena_mouse_init(JSContext* ctx);
#endif

#ifdef ATHENA_AUDIO
JSModuleDef *athena_sound_init(JSContext* ctx);
#endif

#ifdef ATHENA_CAMERA
JSModuleDef *athena_camera_init(JSContext* ctx);
#endif



#endif