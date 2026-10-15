#ifndef ATHENA_H
#define ATHENA_H

/*
 * Umbrella header for the pure-C AthenaEnv framework API.
 * QuickJS bindings are optional and live in ath_env.h (ATHENA_JS).
 */

#include <athena_core.h>
#include <system.h>
#include <athena/system_facade.h>
#include <memory.h>
#include <taskman.h>
#include <iop_manager.h>
#include <athena/iop_facade.h>
#include <athena/archive.h>
#include <pad.h>
#include <athena/pad.h>
#include <athena/timer.h>
#include <athena/mutex.h>
#include <athena/task.h>
#include <matrix.h>
#include <vector.h>
#include <athena_math.h>
#include <ee_tools.h>
#include <strUtils.h>
#include <excepHandler.h>

#ifdef ATHENA_GRAPHICS
#include <graphics.h>
#include <texture_manager.h>
#include <render.h>
#include <render_batch.h>
#include <render_scene.h>
#include <render_async_loader.h>
#include <shadows.h>
#include <fntsys.h>
#include <tile_render.h>
#include <athena/color.h>
#include <athena/draw.h>
#include <athena/vec2.h>
#include <athena/vec3.h>
#include <athena/vec4.h>
#include <athena/matrix.h>
#include <athena/screen.h>
#include <athena/sprite.h>
#include <athena/image.h>
#include <athena/image_list.h>
#include <athena/font.h>
#include <athena/camera3d.h>
#include <athena/lights.h>
#include <athena/anim3d.h>
#include <athena/shadows_facade.h>
#include <athena/render_facade.h>
#endif

#ifdef ATHENA_AUDIO
#include <sound.h>
#include <athena/sound.h>
#endif

#ifdef ATHENA_NETWORK
#include <network.h>
#include <ath_net.h>
#include <athena/net.h>
#include <athena/request.h>
#include <athena/socket.h>
#include <athena/websocket.h>
#endif

#ifdef ATHENA_MPEG_VIDEO
#include <mpeg_player.h>
#include <mpg_manager.h>
#include <athena/video.h>
#endif

#ifdef ATHENA_PHYSICS
#include <physics.h>
#include <athena/physics_facade.h>
#endif

#ifdef ATHENA_KEYBOARD
#include <athena/keyboard.h>
#endif

#ifdef ATHENA_MOUSE
#include <athena/mouse.h>
#endif

#ifdef ATHENA_CAMERA
#include <athena/camera.h>
#endif

#ifdef ATHENA_NATIVE_COMPILER
#include <native_compiler.h>
#include <athena/native_facade.h>
#endif

#endif /* ATHENA_H */
