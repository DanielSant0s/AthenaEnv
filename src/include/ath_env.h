#ifndef ATH_ENV_H
#define ATH_ENV_H

#include <athena_core.h>
#include <dbgprintf.h>
#include <macros.h>

#ifdef ATHENA_JS

#include <setjmp.h>
#include "../quickjs/quickjs-libc.h"
#include <ath_bindings.h>

JSModuleDef *athena_push_module(JSContext *ctx, JSModuleInitFunc *func,
                                const JSCFunctionListEntry *func_list, int len,
                                const char *module_name);

const char *run_script(const char *script, bool isBuffer);
void destroy_vm(JSContext *ctx);
jmp_buf *get_reset_buf(void);

JSModuleDef *athena_system_init(JSContext *ctx);
JSModuleDef *athena_iop_init(JSContext *ctx);
JSModuleDef *athena_archive_init(JSContext *ctx);
JSModuleDef *athena_timer_init(JSContext *ctx);
JSModuleDef *athena_box2d_init(JSContext* ctx);
JSModuleDef *athena_task_init(JSContext *ctx);
JSModuleDef *athena_pads_init(JSContext *ctx);
JSModuleDef *athena_mutex_init(JSContext *ctx);
void athena_task_free(JSContext *ctx);

JSModuleDef *athena_vector_init(JSContext *ctx);
JSModuleDef *athena_vector4_init(JSContext *ctx);
JSModuleDef *athena_matrix_init(JSContext *ctx);
JSModuleDef *athena_physics_init(JSContext *ctx);

#ifdef ATHENA_GRAPHICS
JSModuleDef *athena_render_init(JSContext *ctx);
JSModuleDef *athena_lights_init(JSContext *ctx);
JSModuleDef *athena_3dcamera_init(JSContext *ctx);
JSModuleDef *athena_anim_3d_init(JSContext *ctx);
JSModuleDef *athena_screen_init(JSContext *ctx);
JSModuleDef *athena_color_init(JSContext *ctx);
JSModuleDef *athena_shape_init(JSContext *ctx);
JSModuleDef *athena_font_init(JSContext *ctx);
JSModuleDef *athena_image_init(JSContext *ctx);
JSModuleDef *athena_imagelist_init(JSContext *ctx);
JSModuleDef *athena_shadows_init(JSContext *ctx);
JSModuleDef *athena_ode_init(JSContext *ctx);
JSModuleDef *athena_tilemap_init(JSContext *ctx);
JSModuleDef *athena_webview_init(JSContext *ctx);
#endif

#ifdef ATHENA_NETWORK
JSModuleDef *athena_socket_init(JSContext *ctx);
JSModuleDef *athena_network_init(JSContext *ctx);
JSModuleDef *athena_request_init(JSContext *ctx);
JSModuleDef *athena_ws_init(JSContext *ctx);
#endif

#ifdef ATHENA_KEYBOARD
JSModuleDef *athena_keyboard_init(JSContext *ctx);
#endif

#ifdef ATHENA_MOUSE
JSModuleDef *athena_mouse_init(JSContext *ctx);
#endif

#ifdef ATHENA_REMOTE
JSModuleDef *athena_remote_init(JSContext *ctx);
#endif

#ifdef ATHENA_AUDIO
JSModuleDef *athena_sound_init(JSContext *ctx);
#endif

#ifdef ATHENA_CAMERA
JSModuleDef *athena_camera_init(JSContext *ctx);
#endif

#ifdef ATHENA_NATIVE_COMPILER
JSModuleDef *athena_native_init(JSContext *ctx);
void athena_native_cleanup(void);
#endif

#ifdef ATHENA_MPEG_VIDEO
JSModuleDef *athena_mpeg_init(JSContext *ctx);
#endif

void athena_js_register_modules(JSContext *ctx);
const char *athena_js_globals_prelude(void);

#endif /* ATHENA_JS */

#endif /* ATH_ENV_H */
