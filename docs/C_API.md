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
| Video | `athena/video.h` | `AthenaVideo` | `athena_video_create`, `athena_video_update`, `athena_video_draw` |
| System | `athena/system_facade.h` | `AthenaDirEntry`, `AthenaMemoryStats` | `athena_system_list_dir`, `athena_system_sleep`, `athena_system_get_memory_stats` |
| LZ4 / EE compaction | `lz4_ps2.h` | `AthenaMemCompressConfig`, `AthenaMemCompressStats` | `athena_lz4_compress`, `athena_lz4_decompress`, `athena_lz4_unpack_blob`, `athena_mem_compress_tick`, `athena_mem_compact_now`, `athena_mem_compress_set_config` |
| IOP | `athena/iop_facade.h` | — | `athena_iop_load_module`, `athena_iop_reset` |
| Archive | `athena/archive.h` | `AthenaArchive` | `athena_archive_open`, `athena_archive_list`, `athena_archive_extract_all`, `athena_archive_untar`, `athena_archive_close` |
| Network | `athena/net.h` | `AthenaNetConfig` | `athena_net_init`, `athena_net_init_dhcp`, `athena_net_get_config`, `athena_net_deinit` |
| Request | `athena/request.h` | `AthenaRequest` | `athena_request_create`, `athena_request_get`, `athena_request_post`, `athena_request_download`, `athena_request_run_async`, `athena_request_poll` |
| Socket | `athena/socket.h` | `AthenaSocket` | `athena_socket_create`, `athena_socket_connect` |
| WebSocket | `athena/websocket.h` | `AthenaWebSocket` | `athena_websocket_connect`, `athena_websocket_send` |
| Native | `athena/native_facade.h` | — | native compiler AOT APIs |
| Physics | `physics.h`, `athena/physics_facade.h` | `AthenaPhysicsWorld`, `AthenaPhysicsSpace`, `AthenaPhysicsBody`, `AthenaPhysicsGeom`, `AthenaPhysicsJoint` | VU0/MMI rigid-body dynamics, BVH collision, raycasting, and joints |

## EE RAM compaction

`lz4_ps2.h` is the R5900 LZ4 block codec and the automatic compactor. It is on by default: `athena_mem_compress_tick()` runs from `texture_manager_nextFrame()` with a 350 µs budget, and `malloc` / `realloc` / `calloc` / `memalign` call `athena_mem_compact_now()` and retry when an allocation returns `NULL`.

`athena_system_get_memory_stats()` copies the live counters into `AthenaMemoryStats` (`saved_bytes`, `compressed_textures`, `tracked_textures`, `compressed_meshes`, `tracked_meshes`, `oom_rescues`, …). `athena_mem_compress_set_config()` changes the budget and the idle-frame thresholds. `athena_lz4_compress()` / `athena_lz4_decompress()` are the raw codec; the hash table lives in scratchpad `0x70003000`–`0x70003FFF`. Embedded `.data` blobs generated by `scripts/bin2c_lz4.py` (`ALZ4` header) are unpacked on demand via `athena_lz4_unpack_blob()`.

Textures and meshes register themselves. A GS-resident texture's EE pixel copy, and a mesh's float attributes after the VU1 compact buffers are cooked, are the buffers that get replaced. `athena_texture_ensure_uncompressed()` and `athena_mesh_ensure_uncompressed()` put the raw bytes back. The JavaScript side of the same system is [Automatic EE RAM compaction](CORE_API.md#automatic-ee-ram-compaction).

## Build modes

| Target | Command | Result |
|--------|---------|--------|
| Full (JS + C) | `make` | `bin/athena.elf` + `lib/libathena.a` + `lib/libathena_js.a` |
| C-only example | `make capp` | `bin/athena_capp.elf` |
| Every C app | `make demos` | `bin/athena_demos.elf`: each `examples/<app>/main.c`, picked with `--app=<app>` |
| One C app alone | `make capp CAPP_NAME=physics_collision` | `bin/athena_physics_collision.elf` from `examples/physics_collision/main.c` |
| Libraries only | `make libs` | Static archives in `lib/` |
| No JS | `make ATHENA_JS=0` | Core without QuickJS |

Verify decoupling:

```bash
bash scripts/verify-decouple.sh
```

## C apps and the AthenaOS SDK

An example's C version lives next to its scripts, in `examples/<app>/main.c`. `examples/common/athenaos/app.h` is the C side of `athenaos/app.js`: the same top bar, stats cards and footer hints (`AthenaOSApp`, `athenaos_chrome`, `athenaos_hud`, ...), Triangle for Home, and the `ATHENAOS_APP(id, main)` entry macro:

```c
#include "athenaos/app.h"

static const AthenaOSHint hints[] = { { "cross", "Jump" } };

static int my_main(int argc, char **argv) {
    AthenaOSApp ui;
    AthenaPad *pad = athena_pad_open(0);
    athenaos_app_init(&ui, "My demo", "3d", hints, 1);
    for (;;) {
        athena_pad_update(pad);
        if (athenaos_home_requested(pad))
            return 0;           // back to the shell that launched it
        /* ...draw... */
        athenaos_chrome(&ui);
        athena_screen_flip();
    }
}
ATHENAOS_APP(my_demo, my_main)
```

`make demos` links every C app into `bin/athena_demos.elf` (`ATHENAOS_DEMOS` turns `ATHENAOS_APP` into a registration). It runs the app named by `--app=<id>`; when the app returns it loads `--return=<elf>` with `--return-script=<script>`, which is how the AthenaOS shells offer "Open in C" (R1 in an app's details) and get the user back. Without `--app` it shows a launcher of the C apps. `make capp CAPP_NAME=<app>` still builds a single app as its own ELF.

## JavaScript bindings

When `ATHENA_JS=1` (default), `src/js_api/ath_*.c` modules delegate to the C facade. JS-only concerns (class IDs, `JSValue` ref-keeping) live in `src/js_api/ath_bindings.h`. VM lifecycle is in `src/js_api/ath_env.c`.
