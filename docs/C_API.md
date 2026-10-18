# AthenaEnv Pure-C API

AthenaEnv exposes a uniform pure-C API under `src/include/athena/`, aggregated by `athena/athena.h`. QuickJS bindings in `src/js_api/` are thin wrappers over this layer.

## Conventions

- **Functions**: `athena_<module>_<verb>()` (snake_case)
- **Types**: `Athena<Module>` (PascalCase): `AthenaPad`, `AthenaImage`, `AthenaFont`, etc.
- **Constants**: `ATHENA_<MODULE>_<NAME>`
- **Stateful objects**: `create`/`destroy` or `open`/`close` pairs; constructors return `NULL` on failure

## Quick start

```c
#include <athena/athena.h>

int main(void) {
    AthenaPad *pad = athena_pad_open(0);
    athena_pad_update(pad);
    if (athena_pad_pressed(pad, PAD_CROSS))
        /* ... */;
    athena_pad_close(pad);
    return 0;
}
```

## Module reference

| Module | Header | Key types | Key functions |
|--------|--------|-----------|---------------|
| Image | `athena/image.h` | `AthenaImage` | `athena_image_create`, `athena_image_draw`, `athena_image_destroy` |
| ImageList | `athena/image_list.h` | `AthenaImageList` | `athena_image_list_create`, `athena_image_list_append`, `athena_image_list_process` |
| Pad | `athena/pad.h` | `AthenaPad` | `athena_pad_open`, `athena_pad_update`, `athena_pad_pressed` |
| Timer | `athena/timer.h` | `AthenaTimer` | `athena_timer_create`, `athena_timer_get_time`, `athena_timer_pause` |
| Mutex | `athena/mutex.h` | `AthenaMutex` | `athena_mutex_create`, `athena_mutex_lock` |
| Task | `athena/task.h` | — | `athena_thread_create`, `athena_thread_start`, `athena_thread_kill` |
| Color | `athena/color.h` | — | `athena_color_rgba`, `athena_color_get_r` |
| Draw | `athena/draw.h` | — | `athena_draw_point`, `athena_draw_line`, `athena_draw_rect` |
| Vec2/3/4 | `athena/vec2.h` etc. | `AthenaVec2` etc. | `athena_vec2_create`, `athena_vec3_dot` |
| Matrix | `athena/matrix.h` | `AthenaMatrix4` | `athena_matrix4_create`, `athena_matrix4_multiply` |
| Screen | `athena/screen.h` | — | `athena_screen_flip`, `athena_screen_clear` |
| Sprite | `athena/sprite.h` | `AthenaTilemapDescriptor` | tilemap/sprite batch APIs |
| Particles | `particles.h`, `athena/particles.h` | `AthenaParticles`, `AthenaParticleEffect`, `AthenaParticleRenderer` | `particles_create`, `particles_burst`, `particles_update`, `athena_particles_draw` |
| Font | `athena/font.h` | `AthenaFont` | `athena_font_load`, `athena_font_print` |
| Camera3D | `athena/camera3d.h` | `AthenaCamera3dState` | `athena_camera3d_position`, `athena_camera3d_orbit` |
| Lights | `athena/lights.h` | — | `athena_lights_new`, `athena_lights_set` |
| Render | `athena/render_facade.h` | `AthenaRenderObject` etc. | render data/object/batch/scene/async |
| Sound | `athena/sound.h` | `AthenaSound`, `AthenaSfx` | `athena_sound_load`, `athena_sfx_play` |
| Video | `athena/video.h` | `AthenaVideo` | `athena_video_open`, `athena_video_update` |
| System | `athena/system_facade.h` | `AthenaDirEntry` | `athena_system_list_dir`, `athena_system_sleep` |
| IOP | `athena/iop_facade.h` | — | `athena_iop_load_module`, `athena_iop_reset` |
| Archive | `athena/archive.h` | `AthenaArchive` | `athena_archive_open`, `athena_archive_extract` |
| Network | `athena/net.h` | — | `athena_net_init_dhcp`, `athena_net_set_config` |
| Request | `athena/request.h` | `AthenaRequest` | `athena_request_create`, `athena_request_perform` |
| Socket | `athena/socket.h` | `AthenaSocket` | `athena_socket_create`, `athena_socket_connect` |
| WebSocket | `athena/websocket.h` | `AthenaWebSocket` | `athena_websocket_connect`, `athena_websocket_send` |
| Native | `athena/native_facade.h` | — | native compiler AOT APIs |
| Physics | `physics.h`, `athena/physics_facade.h` | `AthenaPhysicsWorld`, `AthenaPhysicsSpace`, `AthenaPhysicsBody`, `AthenaPhysicsGeom`, `AthenaPhysicsJoint` | VU0/MMI rigid-body dynamics, BVH collision, raycasting, and joints |

## Build modes

| Target | Command | Result |
|--------|---------|--------|
| Full (JS + C) | `make` | `bin/athena.elf` + `lib/libathena.a` + `lib/libathena_js.a` |
| C-only example | `make capp` | `bin/athena_capp.elf` |
| Libraries only | `make libs` | Static archives in `lib/` |
| No JS | `make ATHENA_JS=0` | Core without QuickJS |

Verify decoupling:

```bash
bash scripts/verify-decouple.sh
```

## JavaScript bindings

When `ATHENA_JS=1` (default), `src/js_api/ath_*.c` modules delegate to the C facade. JS-only concerns (class IDs, `JSValue` ref-keeping) live in `src/js_api/ath_bindings.h`. VM lifecycle is in `src/js_api/ath_env.c`.
