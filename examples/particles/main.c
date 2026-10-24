/*
 * Native 2D particles, in C: the port of main.js.
 * Move the emitter with the left stick/D-pad. Cross: burst. Circle: toggle smoke.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <athena/athena.h>
#include "athenaos/app.h"

static const AthenaOSHint HINTS[] = {
    { "L-stick", "Move" }, { "cross", "Sparks" }, { "circle", "Smoke" }, { "R3", "Clear" },
};

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int particles_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Native 2D particles", "2d", HINTS, 4);

    AthenaImage *atlas = athena_image_create("render/fx_atlas.png", true);
    athena_image_set_filter(atlas, GS_FILTER_LINEAR);

    // Screen space: no depth test, no sorting.
    AthenaParticles *smoke = particles_create(512, false, false, 42978);
    AthenaParticleRenderer *smoke_renderer = athena_particles_renderer_create(smoke, atlas, GS_ALPHA_BLEND_NORMAL, false);
    AthenaParticleEffect e;
    particles_effect_defaults(&e);
    e.frame[0] = 0; e.frame[1] = 0; e.frame[2] = 32; e.frame[3] = 32;
    e.life_min = 0.7f; e.life_max = 1.4f; e.size_min = 10; e.size_max = 20; e.scale_end = 3;
    e.velocity_min[0] = -15; e.velocity_min[1] = -60; e.velocity_min[2] = 0;
    e.velocity_max[0] = 15; e.velocity_max[1] = -25; e.velocity_max[2] = 0;
    e.spread[0] = 5; e.spread[1] = 2; e.spread[2] = 0;
    e.drag = 0.7f;
    e.color[0] = 100; e.color[1] = 110; e.color[2] = 128; e.color[3] = 60;
    e.color_end[0] = 100; e.color_end[1] = 110; e.color_end[2] = 128; e.color_end[3] = 0;
    e.fade_in = 0.1f;
    const int dust = particles_add_emitter(smoke, &e);
    const float dust_rate = 45;

    AthenaParticles *sparks = particles_create(512, false, false, 42978);
    AthenaParticleRenderer *sparks_renderer = athena_particles_renderer_create(sparks, atlas, GS_ALPHA_BLEND_ADD, false);
    particles_effect_defaults(&e);
    e.frame[0] = 32; e.frame[1] = 0; e.frame[2] = 32; e.frame[3] = 32;
    e.life_min = 0.25f; e.life_max = 0.8f; e.size_min = 3; e.size_max = 7; e.scale_end = 0.3f;
    e.velocity_min[0] = -180; e.velocity_min[1] = -230; e.velocity_min[2] = 0;
    e.velocity_max[0] = 180; e.velocity_max[1] = 50; e.velocity_max[2] = 0;
    e.acceleration[0] = 0; e.acceleration[1] = 320; e.acceleration[2] = 0;
    e.color[0] = 180; e.color[1] = 90; e.color[2] = 20; e.color[3] = 128;
    e.color_end[0] = 128; e.color_end[1] = 20; e.color_end[2] = 4; e.color_end[3] = 0;
    const int burst = particles_add_emitter(sparks, &e);

    AthenaPad *pad = athena_pad_open(0);
    AthenaTimer *timer = athena_timer_create();
    float x = ui.W / 2.0f, y = ui.H * 0.65f;
    bool running = true;
    particles_position(smoke, dust, (const float[3]){ x, y, 0 });
    particles_start(smoke, dust, dust_rate);
    athena_screen_set_frame_counter(true);

    for (;;) {
        const float dt = fminf((float)athena_timer_get_time(timer) / CLOCKS_PER_SEC, 0.05f);
        athena_timer_reset(timer);
        athena_pad_update(pad);
        if (athenaos_home_requested(pad))
            break;
        float dx = pad->lx / 128.0f, dy = pad->ly / 128.0f;
        if (fabsf(dx) < 0.15f) dx = 0;
        if (fabsf(dy) < 0.15f) dy = 0;
        if (athena_pad_pressed(pad, PAD_LEFT)) dx = -1;
        if (athena_pad_pressed(pad, PAD_RIGHT)) dx = 1;
        if (athena_pad_pressed(pad, PAD_UP)) dy = -1;
        if (athena_pad_pressed(pad, PAD_DOWN)) dy = 1;
        x = clampf(x + dx * 200 * dt, 0, ui.W);
        y = clampf(y + dy * 200 * dt, 0, ui.H);
        const float p[3] = { x, y, 0 };
        particles_position(smoke, dust, p);
        if (athena_pad_just_pressed(pad, PAD_CROSS))
            particles_burst(sparks, burst, 96, p);
        if (athena_pad_just_pressed(pad, PAD_CIRCLE)) {
            running = !running;
            if (running) particles_start(smoke, dust, dust_rate);
            else particles_stop(smoke, dust);
        }
        if (athena_pad_just_pressed(pad, PAD_R3)) {
            particles_clear(smoke, false, 0);
            particles_clear(sparks, false, 0);
        }
        particles_update(smoke, dt);
        particles_update(sparks, dt);
        athena_screen_clear(athena_color_rgba(12, 14, 20, 128));
        athena_particles_draw(smoke_renderer);
        athena_particles_draw(sparks_renderer);
        athena_draw_line(x - 5, y, x + 5, y, athena_color_rgba(128, 128, 128, 128));
        athena_draw_line(x, y - 5, x, y + 5, athena_color_rgba(128, 128, 128, 128));
        snprintf(ui.status, sizeof(ui.status), "%.1f FPS", athena_screen_get_fps(60));
        char l0[48], l1[48];
        snprintf(l0, sizeof(l0), "%u particles", (unsigned)(particles_stats(smoke).active + particles_stats(sparks).active));
        snprintf(l1, sizeof(l1), "Smoke %s", running ? "ON" : "OFF");
        const char *lines[] = { l0, l1 };
        athenaos_hud(&ui, lines, 2, 0);
        athenaos_chrome(&ui);
        athena_screen_flip();
    }

    athena_particles_renderer_destroy(smoke_renderer);
    athena_particles_renderer_destroy(sparks_renderer);
    particles_destroy(smoke);
    particles_destroy(sparks);
    return 0;
}

ATHENAOS_APP(particles, particles_main)
