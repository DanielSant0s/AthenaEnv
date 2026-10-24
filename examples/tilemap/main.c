/*
 * Tilemap, in C: the port of main.js. A 6x4 grid of sprites from one atlas,
 * waving and changing color every frame, drawn by VU1 in one call.
 */

#include <math.h>
#include <stdio.h>

#include <athena/athena.h>
#include <owl_packet.h>
#include "athenaos/app.h"

#define TILES_X 6
#define TILES_Y 4
#define TILE_SIZE 48
#define SPRITE_COUNT (TILES_X * TILES_Y)

static int tilemap_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Tilemap", "2d", NULL, 0);
    AthenaPad *pad = athena_pad_open(0);
    athena_screen_set_frame_counter(true);

    athena_sprite_data sprites[SPRITE_COUNT];
    float base[SPRITE_COUNT][2];
    for (int y = 0; y < TILES_Y; y++) {
        for (int x = 0; x < TILES_X; x++) {
            athena_sprite_data *s = &sprites[y * TILES_X + x];
            const float atlas_x = (x % 2) * 64, atlas_y = (y % 2) * 64;
            *s = (athena_sprite_data){
                .x = x * TILE_SIZE, .y = y * TILE_SIZE, .w = TILE_SIZE, .h = TILE_SIZE,
                .u1 = atlas_x, .v1 = atlas_y, .u2 = atlas_x + 64, .v2 = atlas_y + 64,
                .r = 180, .g = 180, .b = 180, .a = 200, .zindex = 1,
            };
            base[y * TILES_X + x][0] = s->x;
            base[y * TILES_X + x][1] = s->y;
        }
    }

    AthenaImage *texture = athena_image_create("simple_atlas.png", true);
    const athena_sprite_material material = {
        .texture_index = 0,
        .blend_mode = athena_screen_alpha_equation(ZERO_RGB, SRC_RGB, SRC_ALPHA, DST_RGB, 0),
        .end = SPRITE_COUNT - 1,
    };
    AthenaTilemapDescriptor *descriptor = athena_sprite_descriptor_create(&texture, 1, &material, 1);
    AthenaTilemapInstance *tile_map = athena_sprite_instance_create(descriptor, sprites, SPRITE_COUNT, true);
    uint32_t count;
    athena_sprite_data *live = athena_sprite_instance_get_sprites(tile_map, &count);

    float time = 0;
    athena_screen_set_param(DEPTH_TEST_ENABLE, true);
    athena_sprite_init();

    for (;;) {
        athena_screen_clear(athena_color_rgba(200, 200, 200, 128));
        athena_pad_update(pad);

        athena_sprite_begin();

        if (athenaos_home_requested(pad))
            break;

        time += 0.05f;
        for (int i = 0; i < SPRITE_COUNT; i++) {
            const int row = i / TILES_X, col = i % TILES_X;
            const float wave = sinf(time + col * 0.5f) * 6;
            const float lift = cosf(time * 0.8f + row * 0.6f) * 4;
            live[i].x = base[i][0] + wave;
            live[i].y = base[i][1] + lift;

            const float pulse = sinf(time + i * 0.15f) * 0.5f + 0.5f;
            const int hue = 120 + (int)floorf(pulse * 100);
            live[i].r = hue;
            live[i].g = (uint32_t)(200 - hue / 2.0f);
            live[i].b = 220;
        }

        athena_sprite_instance_render(tile_map, 80, 60, 0);

        snprintf(ui.status, sizeof(ui.status), "%.1f FPS", athena_screen_get_fps(60));
        char line[48];
        snprintf(line, sizeof(line), "%d sprites, one VU1 draw", SPRITE_COUNT);
        const char *lines[] = { line };
        athenaos_hud(&ui, lines, 1, 0);
        athenaos_chrome(&ui);

        athena_screen_flip();
    }

    owl_wait_inflight();
    athena_sprite_instance_destroy(tile_map);
    athena_sprite_descriptor_destroy(descriptor);
    return 0;
}

ATHENAOS_APP(tilemap, tilemap_main)
