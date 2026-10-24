/*
 * Tilemap Dynamic, in C: the port of main.js. Up to 1024 sprite slots in one
 * tilemap: sprites spawn, wobble, fade and die by writing their slots, a few
 * per frame.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <athena/athena.h>
#include <owl_packet.h>
#include "athenaos/app.h"

#define PI 3.14159265f
#define TILE_SIZE 16
#define MAX_SPRITES 1024

static const AthenaOSHint HINTS[] = { { "cross", "Spawn" }, { "circle", "Remove" }, { "SELECT", "Restart" } };

typedef struct {
    bool alive;
    float life, phase, speed;
    float target[2];
} Slot;

static Slot slots[MAX_SPRITES];
static athena_sprite_data *live;

static float rnd(void) { return (float)rand() / ((float)RAND_MAX + 1.0f); }

static void hide_sprite(int i) {
    live[i].w = 0;
    live[i].h = 0;
    live[i].a = 0;
    slots[i].alive = false;
}

static void init_slot(int i, float x, float y) {
    Slot *slot = &slots[i];
    slot->alive = true;
    slot->life = 4 + rnd() * 6;
    slot->phase = rnd() * PI * 2;
    slot->speed = 0.5f + rnd();
    slot->target[0] = x;
    slot->target[1] = y;

    live[i].x = x;
    live[i].y = y;
    live[i].zindex = 1;
    live[i].w = TILE_SIZE;
    live[i].h = TILE_SIZE;

    const float atlas_x = rnd() > 0.5f ? 64 : 0, atlas_y = rnd() > 0.5f ? 64 : 0;
    live[i].u1 = atlas_x;
    live[i].v1 = atlas_y;
    live[i].u2 = atlas_x + 64;
    live[i].v2 = atlas_y + 64;

    const int tint = 120 + (int)floorf(rnd() * 120);
    live[i].r = tint;
    live[i].g = (uint32_t)(220 - tint / 2.0f);
    live[i].b = 200;
    live[i].a = 255;
}

static void spawn_random(void) {
    const float x = 40 + rnd() * 600, y = 40 + rnd() * 400;
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (!slots[i].alive) {
            init_slot(i, x, y);
            return;
        }
    }
}

static int alive_indices(int *out) {
    int n = 0;
    for (int i = 0; i < MAX_SPRITES; i++)
        if (slots[i].alive)
            out[n++] = i;
    return n;
}

static void remove_random(void) {
    static int alive[MAX_SPRITES];
    const int n = alive_indices(alive);
    if (n)
        hide_sprite(alive[(int)(rnd() * n)]);
}

static int tilemap_dynamic_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Tilemap Dynamic", "2d", HINTS, 3);
    AthenaPad *pad = athena_pad_open(0);
    athena_screen_set_frame_counter(true);

    AthenaImage *texture = athena_image_create("simple_atlas.png", true);
    const athena_sprite_material material = {
        .texture_index = 0,
        .blend_mode = athena_screen_alpha_equation(ZERO_RGB, SRC_RGB, SRC_ALPHA, DST_RGB, 0),
        .end = MAX_SPRITES - 1,
    };
    AthenaTilemapDescriptor *descriptor = athena_sprite_descriptor_create(&texture, 1, &material, 1);

    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
    athena_sprite_init();

restart:;
    // The script restarts itself with std.reload(); here, a fresh buffer and slots.
    athena_sprite_data *buffer = athena_sprite_buffer_create(MAX_SPRITES);
    AthenaTilemapInstance *tile_map = athena_sprite_instance_create(descriptor, buffer, MAX_SPRITES, true);
    free(buffer);
    uint32_t count;
    live = athena_sprite_instance_get_sprites(tile_map, &count);
    memset(slots, 0, sizeof(slots));

    float time = 0, spawn_accumulator = 0;
    static int alive[MAX_SPRITES];

    for (;;) {
        athena_screen_clear(athena_color_rgba(200, 200, 200, 128));
        athena_pad_update(pad);

        athena_sprite_begin();

        if (athenaos_home_requested(pad))
            break;
        if (athena_pad_just_pressed(pad, PAD_SELECT)) {
            owl_wait_inflight(); // the last frame's draw may still be reading the sprites
            athena_sprite_instance_destroy(tile_map);
            goto restart;
        }
        if (athena_pad_just_pressed(pad, PAD_CROSS))
            spawn_random();
        if (athena_pad_just_pressed(pad, PAD_CIRCLE))
            remove_random();

        time += 0.016f;
        spawn_accumulator += 0.016f;
        if (spawn_accumulator > 0.35f) {
            spawn_random();
            spawn_accumulator = 0;
        }

        int n = alive_indices(alive);
        const int updates = n < 10 ? n : 10;
        for (int u = 0; u < updates; u++) {
            const int pick_index = (int)(rnd() * n);
            const int pick = alive[pick_index];
            memmove(&alive[pick_index], &alive[pick_index + 1], (n - pick_index - 1) * sizeof(int));
            n--;
            Slot *slot = &slots[pick];

            slot->life -= 0.016f;
            slot->phase += slot->speed * 0.05f;

            live[pick].x = slot->target[0] + sinf(slot->phase) * 6;
            live[pick].y = slot->target[1] + cosf(slot->phase * 1.3f) * 4;

            const float pulse = (sinf(time * 3 + pick * 0.25f) + 1) * 0.5f;
            live[pick].a = 160 + (int)floorf(pulse * 80);

            if (slot->life <= 0)
                hide_sprite(pick);
        }

        athena_sprite_instance_render(tile_map, 20, 20, 0);

        snprintf(ui.status, sizeof(ui.status), "%.1f FPS", athena_screen_get_fps(60));
        char line[48];
        snprintf(line, sizeof(line), "%d / %d sprites alive", alive_indices(alive), MAX_SPRITES);
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

ATHENAOS_APP(tilemap_dynamic, tilemap_dynamic_main)
