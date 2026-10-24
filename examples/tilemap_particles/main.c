/*
 * Tilemap Particles, in C: the port of main.js. Untextured tilemap sprites as
 * particles; Square/Circle grow and shrink the active slots, which rebuilds the
 * tilemap with a shorter or longer material range.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <athena/athena.h>
#include <owl_packet.h>
#include "athenaos/app.h"

#define PI 3.14159265f
#define MAX_PARTICLES 256
#define INITIAL_PARTICLES 32
#define TILE_SIZE 16

static const AthenaOSHint HINTS[] = {
    { "cross", "Burst" }, { "square", "More" }, { "circle", "Fewer" }, { "SELECT", "Restart" },
};

typedef struct {
    bool alive;
    float pos[2], vel[2];
    float size, life;
} Particle;

static Particle particles[MAX_PARTICLES];
static int active_count;
static AthenaTilemapDescriptor *descriptor;
static AthenaTilemapInstance *tile_map;
static athena_sprite_data *live;

static float rnd(void) { return (float)rand() / ((float)RAND_MAX + 1.0f); }

static AthenaTilemapDescriptor *build_descriptor(uint32_t end_index) {
    const athena_sprite_material material = {
        .texture_index = -1,
        .blend_mode = athena_screen_alpha_equation(SRC_RGB, DST_RGB, SRC_ALPHA, DST_RGB, 0),
        .end = end_index,
    };
    return athena_sprite_descriptor_create(NULL, 0, &material, 1);
}

static uint32_t random_channel(int base, int spread) {
    const int v = base + (int)floorf((rnd() - 0.5f) * spread);
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

static void set_inactive(int i) {
    particles[i].alive = false;
    live[i].a = 0;
}

static void init_particle(int i, float x, float y, float vx, float vy) {
    Particle *p = &particles[i];
    p->alive = true;
    p->pos[0] = x; p->pos[1] = y;
    p->vel[0] = vx; p->vel[1] = vy;
    p->life = 1.5f + rnd() * 1.5f;
    p->size = 12 + rnd() * 10;

    const uint32_t r = random_channel(100, 80), g = random_channel(60, 60), b = random_channel(128, 40);
    live[i].x = x;
    live[i].y = y;
    live[i].zindex = 0.5f;
    live[i].w = p->size;
    live[i].h = p->size;
    live[i].r = r;
    live[i].g = g;
    live[i].b = b;
    live[i].a = 255;
}

static void spawn_burst(int count) {
    for (int n = 0; n < count; n++) {
        int idx = -1;
        for (int i = 0; i < active_count; i++)
            if (!particles[i].alive) { idx = i; break; }
        if (idx == -1)
            return;
        const float speed = 30 + rnd() * 90, angle = rnd() * PI * 2;
        init_particle(idx, 220, 140, cosf(angle) * speed, sinf(angle) * speed);
    }
}

// A new descriptor and instance for the new range, from a copy of the sprites.
static void rebuild(int count, const athena_sprite_data *from, int from_count) {
    athena_sprite_data *buffer = athena_sprite_buffer_create(count);
    if (from)
        memcpy(buffer, from, (from_count < count ? from_count : count) * sizeof(athena_sprite_data));
    AthenaTilemapDescriptor *new_descriptor = build_descriptor(count - 1);
    AthenaTilemapInstance *new_map = athena_sprite_instance_create(new_descriptor, buffer, count, true);
    free(buffer);
    // The last frame's draw may still be reading the old sprites.
    owl_wait_inflight();
    if (tile_map) athena_sprite_instance_destroy(tile_map);
    if (descriptor) athena_sprite_descriptor_destroy(descriptor);
    descriptor = new_descriptor;
    tile_map = new_map;
    uint32_t n;
    live = athena_sprite_instance_get_sprites(tile_map, &n);
}

static void resize_active_count(int delta) {
    int new_count = active_count + delta;
    new_count = new_count < 1 ? 1 : (new_count > MAX_PARTICLES ? MAX_PARTICLES : new_count);
    if (new_count == active_count)
        return;
    for (int i = new_count; i < active_count; i++)
        set_inactive(i);
    const int old_count = active_count;
    active_count = new_count;
    rebuild(active_count, live, old_count);
}

static int tilemap_particles_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Tilemap Particles", "2d", HINTS, 4);
    AthenaPad *pad = athena_pad_open(0);
    athena_screen_set_frame_counter(true);

    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
    athena_sprite_init();

restart:
    // The script restarts itself with std.reload(); here, fresh slots and buffer.
    memset(particles, 0, sizeof(particles));
    active_count = INITIAL_PARTICLES;
    {
        // The script's first buffer holds MAX_PARTICLES sprites for a shorter range.
        athena_sprite_data *buffer = athena_sprite_buffer_create(MAX_PARTICLES);
        AthenaTilemapDescriptor *first = build_descriptor(active_count - 1);
        AthenaTilemapInstance *map = athena_sprite_instance_create(first, buffer, MAX_PARTICLES, true);
        free(buffer);
        owl_wait_inflight();
        if (tile_map) athena_sprite_instance_destroy(tile_map);
        if (descriptor) athena_sprite_descriptor_destroy(descriptor);
        descriptor = first;
        tile_map = map;
        uint32_t n;
        live = athena_sprite_instance_get_sprites(tile_map, &n);
    }

    for (;;) {
        athena_screen_clear(athena_color_rgba(70, 5, 150, 128));
        athena_pad_update(pad);

        athena_sprite_begin();

        if (athenaos_home_requested(pad))
            break;
        if (athena_pad_just_pressed(pad, PAD_SELECT))
            goto restart;
        if (athena_pad_just_pressed(pad, PAD_CROSS))
            spawn_burst(8);
        if (athena_pad_just_pressed(pad, PAD_SQUARE))
            resize_active_count(32);
        if (athena_pad_just_pressed(pad, PAD_CIRCLE))
            resize_active_count(-32);

        for (int i = 0; i < active_count; i++) {
            Particle *p = &particles[i];
            if (!p->alive)
                continue;
            p->life -= 0.016f;
            if (p->life <= 0) {
                set_inactive(i);
                continue;
            }
            p->vel[1] += 30 * 0.016f;
            p->vel[0] *= 0.98f;
            p->vel[1] *= 0.98f;
            p->pos[0] += p->vel[0] * 0.016f;
            p->pos[1] += p->vel[1] * 0.016f;
            live[i].x = p->pos[0];
            live[i].y = p->pos[1];
            const float fade = fmaxf(0, p->life / 3);
            live[i].a = (uint32_t)floorf(255 * fade);
        }

        athena_sprite_instance_render(tile_map, 0, 0, 0);

        snprintf(ui.status, sizeof(ui.status), "%.1f FPS", athena_screen_get_fps(60));
        char line[48];
        snprintf(line, sizeof(line), "%d particle slots active", active_count);
        const char *lines[] = { line };
        athenaos_hud(&ui, lines, 1, 0);
        athenaos_chrome(&ui);
        athena_screen_flip();
    }

    owl_wait_inflight();
    athena_sprite_instance_destroy(tile_map);
    athena_sprite_descriptor_destroy(descriptor);
    tile_map = NULL;
    descriptor = NULL;
    return 0;
}

ATHENAOS_APP(tilemap_particles, tilemap_particles_main)
