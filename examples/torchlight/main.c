/*
 * Torchlight, in C: the port of main.js.
 *
 * Unified lighting, PS2 style: one flickering torch moves through a closed room.
 * Every surface is DOT3 normal mapped against it, and the pillars and crates
 * cast stencil shadow volumes from it -- onto the walls, the floor and each
 * other. The light is a point light, so both the relief and the shadows swing
 * as it passes.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <athena/athena.h>
#include <normalmap.h>
#include <shadow_volumes.h>
#include "athenaos/app.h"

#define PI 3.14159265f

static const AthenaOSHint HINTS[] = {
    { "L-stick", "Look" }, { "cross", "Shadows" }, { "square", "Mode" }, { "circle", "Torch" },
};

static const int MODES[] = { DOT3_OFF, DOT3_2PASS, DOT3_4PASS };
static const char *const MODE_NAMES[] = { "Gouraud", "DOT3 2-pass", "DOT3 4-pass" };
#define MODE_COUNT 3

// --- Geometry ---

typedef struct {
    VECTOR *pos, *nrm, *uv, *col;
    uint32_t count, cap;
} Mesh;

static void mesh_put(Mesh *m, const float p[3], const float n[3], float u, float v) {
    if (m->count == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 64;
        m->pos = realloc(m->pos, m->cap * sizeof(VECTOR));
        m->nrm = realloc(m->nrm, m->cap * sizeof(VECTOR));
        m->uv = realloc(m->uv, m->cap * sizeof(VECTOR));
        m->col = realloc(m->col, m->cap * sizeof(VECTOR));
    }
    const uint32_t i = m->count++;
    m->pos[i][0] = p[0]; m->pos[i][1] = p[1]; m->pos[i][2] = p[2]; m->pos[i][3] = 1;
    m->nrm[i][0] = n[0]; m->nrm[i][1] = n[1]; m->nrm[i][2] = n[2]; m->nrm[i][3] = 1;
    m->uv[i][0] = u; m->uv[i][1] = v; m->uv[i][2] = 1; m->uv[i][3] = 1;
    m->col[i][0] = m->col[i][1] = m->col[i][2] = m->col[i][3] = 0;
}

// The mesh as one RenderData, drawn with diffuse modulated by the torch and
// normal mapped with normal. The mesh's arrays are freed.
static AthenaRenderData *mesh_data(Mesh *m, AthenaImage *diffuse, AthenaImage *normal) {
    AthenaRenderData *data = athena_render_data_from_vertices(m->pos, m->nrm, m->uv, m->col, m->count);
    athena_render_data *n = athena_render_data_native(data);
    n->materials[0].diffuse[0] = n->materials[0].diffuse[1] = n->materials[0].diffuse[2] = 1.0f;
    n->materials[0].diffuse[3] = 1.0f;
    n->materials[0].texture_id = athena_render_data_push_texture(data, diffuse->tex);
    n->materials[0].normal_texture_id = athena_render_data_push_texture(data, normal->tex);
    free(m->pos); free(m->nrm); free(m->uv); free(m->col);
    memset(m, 0, sizeof(*m));
    return data;
}

static void cross(const float a[3], const float b[3], float out[3]) {
    const float c[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
    const float l = sqrtf(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
    out[0] = c[0] / l; out[1] = c[1] / l; out[2] = c[2] / l;
}

// A rectangle from its top-left corner o and the edges u (rightwards) and v
// (downwards), seen from the front; the texture repeated ru x rv times.
static void quad(Mesh *m, const float o[3], const float u[3], const float v[3], float ru, float rv) {
    static const float st[6][2] = { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 0, 0 }, { 1, 1 }, { 1, 0 } };
    float n[3];
    cross(v, u, n);
    for (int i = 0; i < 6; i++) {
        const float s = st[i][0], t = st[i][1];
        const float p[3] = { o[0] + u[0] * s + v[0] * t, o[1] + u[1] * s + v[1] * t, o[2] + u[2] * s + v[2] * t };
        mesh_put(m, p, n, s * ru, t * rv);
    }
}

// An octagonal pillar around the origin, capped at both ends: a shadow volume
// needs a closed mesh. Smooth side normals, the bricks once around.
static void ring(float radius, int k, float y, float out[3]) {
    const float a = 2 * PI * k / 8;
    out[0] = sinf(a) * radius; out[1] = y; out[2] = cosf(a) * radius;
}

static void side(Mesh *m, float radius, float height, int k, float t) {
    const float a = 2 * PI * k / 8;
    float p[3];
    ring(radius, k, height * (1 - t), p);
    const float n[3] = { sinf(a), 0, cosf(a) };
    mesh_put(m, p, n, k / 8.0f, t * height / 4);
}

static void pillar(Mesh *m, float radius, float height) {
    static const float up[3] = { 0, 1, 0 }, down[3] = { 0, -1, 0 };
    for (int k = 0; k < 8; k++) {
        side(m, radius, height, k, 0); side(m, radius, height, k, 1); side(m, radius, height, k + 1, 1);
        side(m, radius, height, k, 0); side(m, radius, height, k + 1, 1); side(m, radius, height, k + 1, 0);
        // Caps, wound to face up and down.
        float a[3], b[3];
        mesh_put(m, (float[3]){ 0, height, 0 }, up, 0.5f, 0.5f);
        ring(radius, k, height, a); mesh_put(m, a, up, 0.5f, 0);
        ring(radius, k + 1, height, b); mesh_put(m, b, up, 0.6f, 0);
        mesh_put(m, (float[3]){ 0, 0, 0 }, down, 0.5f, 0.5f);
        ring(radius, k + 1, 0, a); mesh_put(m, a, down, 0.6f, 0);
        ring(radius, k, 0, b); mesh_put(m, b, down, 0.5f, 0);
    }
}

static AthenaImage *load_linear(const char *path) {
    AthenaImage *img = athena_image_create(path, true);
    athena_image_set_filter(img, GS_FILTER_LINEAR);
    return img;
}

static void set_xyz(AthenaRenderObject *ro, float *field, float x, float y, float z) {
    field[0] = x; field[1] = y; field[2] = z;
    update_object_space(athena_render_object_native(ro));
}

// Flicker: a few incommensurate sines, cheap and never repeating visibly.
static float flicker(float t) {
    return 0.86f + 0.07f * sinf(t * 17.0f) + 0.05f * sinf(t * 23.7f + 1.3f) + 0.03f * sinf(t * 41.3f + 0.4f);
}

// The torch circles the room between the pillars and the crates, weaving in
// and out and low enough to throw long shadows up the walls.
static float torch_angle(float t) { return t * 0.3f; }
static void torch_path(float t, float out[3]) {
    const float a = torch_angle(t), r = 4.6f + 0.4f * sinf(t * 0.7f);
    out[0] = sinf(a) * r; out[1] = 1.3f + 0.3f * sinf(t * 0.53f); out[2] = cosf(a) * r;
}

static int fire_emitter(AthenaParticles *fire, float life_min, float life_max, float size_min, float size_max,
                        float scale_end, const float vmin[3], const float vmax[3], const float spread[3],
                        const float accel[3], const float color[4], const float color_end[4]) {
    AthenaParticleEffect e;
    particles_effect_defaults(&e);
    e.frame[0] = 0; e.frame[1] = 0; e.frame[2] = 32; e.frame[3] = 32;
    e.life_min = life_min; e.life_max = life_max;
    e.size_min = size_min; e.size_max = size_max;
    e.scale_end = scale_end;
    memcpy(e.velocity_min, vmin, sizeof(e.velocity_min));
    memcpy(e.velocity_max, vmax, sizeof(e.velocity_max));
    if (spread) memcpy(e.spread, spread, sizeof(e.spread));
    if (accel) memcpy(e.acceleration, accel, sizeof(e.acceleration));
    memcpy(e.color, color, sizeof(e.color));
    memcpy(e.color_end, color_end, sizeof(e.color_end));
    return particles_add_emitter(fire, &e);
}

#define CAM_RADIUS 6.6f
#define CAM_HEIGHT 3.0f

static int torchlight_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Torchlight", "3d", HINTS, 4);

    athena_screen_set_frame_counter(true);
    athena_screen_set_vsync(true);

    AthenaVideoMode vmode;
    athena_screen_get_mode(&vmode);
    vmode.zbuffering = true;
    vmode.psm = GS_PSM_CT32;
    vmode.psmz = GS_ZBUF_24; // DOT3 and the shadow volumes both keep scratch in the top of Z24
    athena_screen_set_mode(&vmode);
    athena_screen_init_buffers();

    athenaos_loading(&ui, "Loading assets...", "Bricks, slabs and crates");

    athena_render_module_init();
    athena_render_module_set_view(60.0f, 0.5f, 200.0f, 0, 0);

    chdir("render");

    int mode = 2;
    AthenaImage *brick_diffuse = load_linear("bricks_diffuse.png");
    AthenaImage *brick_normal = load_linear("bricks_normal.png");
    AthenaImage *tile_diffuse = load_linear("tiles_diffuse.png");
    AthenaImage *tile_normal = load_linear("tiles_normal.png");

    // A brick tile is 4 x 4 units (8 rows), a slab 2 x 2.
    const float R = 8.0f, H = 5.0f;
    Mesh m = { 0 };
    quad(&m, (float[3]){ -R, H, -R }, (float[3]){ 2 * R, 0, 0 }, (float[3]){ 0, -H, 0 }, 2 * R / 4, H / 4);   // north, facing +z
    quad(&m, (float[3]){ R, H, R }, (float[3]){ -2 * R, 0, 0 }, (float[3]){ 0, -H, 0 }, 2 * R / 4, H / 4);    // south, facing -z
    quad(&m, (float[3]){ -R, H, R }, (float[3]){ 0, 0, -2 * R }, (float[3]){ 0, -H, 0 }, 2 * R / 4, H / 4);   // west, facing +x
    quad(&m, (float[3]){ R, H, -R }, (float[3]){ 0, 0, 2 * R }, (float[3]){ 0, -H, 0 }, 2 * R / 4, H / 4);    // east, facing -x
    AthenaRenderData *walls = mesh_data(&m, brick_diffuse, brick_normal);
    quad(&m, (float[3]){ -R, 0, -R }, (float[3]){ 2 * R, 0, 0 }, (float[3]){ 0, 0, 2 * R }, 2 * R / 4, 2 * R / 4);
    AthenaRenderData *floor = mesh_data(&m, tile_diffuse, tile_normal);
    quad(&m, (float[3]){ -R, H, R }, (float[3]){ 2 * R, 0, 0 }, (float[3]){ 0, 0, -2 * R }, 2 * R / 4, 2 * R / 4);
    AthenaRenderData *ceiling = mesh_data(&m, tile_diffuse, tile_normal);
    pillar(&m, 0.55f, H);
    AthenaRenderData *pillar_data = mesh_data(&m, brick_diffuse, brick_normal);
    AthenaRenderData *crate_data = athena_render_data_load("box_bump.gltf", NULL);

    AthenaRenderData *datas[] = { walls, floor, ceiling, pillar_data, crate_data };
    #define APPLY_MODE() for (int i = 0; i < 5; i++) athena_render_data_native(datas[i])->attributes.normal_mapping = MODES[mode]
    APPLY_MODE();

    AthenaRenderObject *room[3] = {
        athena_render_object_create(walls), athena_render_object_create(floor), athena_render_object_create(ceiling),
    };

    // Casters: everything inside the room. Not frozen -- each volume reads its
    // caster's positions once. Pillars within 3.2 of the centre, crates in the
    // corners: the torch circles between the two, the camera outside it.
    static const struct { int crate; float x, y, z, ry; } PLACES[] = {
        { 0, -2.2f, 0, -1.4f, 0.0f }, { 0, 2.2f, 0, 1.4f, 0.4f },
        { 1, 6.6f, 1.0f, -6.6f, 0.3f }, { 1, -6.6f, 1.0f, 6.6f, -0.5f }, { 1, -6.6f, 3.0f, 6.6f, 0.2f },
        { 1, 6.4f, 1.0f, 6.2f, 0.9f }, { 1, 0.3f, 3.0f, -0.5f, 0.0f },
    };
    enum { CASTER_COUNT = 7 };
    AthenaRenderObject *casters[CASTER_COUNT];
    for (int i = 0; i < CASTER_COUNT; i++) {
        casters[i] = athena_render_object_create(PLACES[i].crate ? crate_data : pillar_data);
        athena_object_data *o = athena_render_object_native(casters[i]);
        set_xyz(casters[i], o->position, PLACES[i].x, PLACES[i].y, PLACES[i].z);
        set_xyz(casters[i], o->rotation, 0, PLACES[i].ry, 0);
    }
    AthenaRenderObject *floater = casters[CASTER_COUNT - 1];
    set_xyz(floater, athena_render_object_native(floater)->scale, 0.6f, 0.6f, 0.6f);

    athena_object_data *scene[3 + CASTER_COUNT];
    for (int i = 0; i < 3; i++) scene[i] = athena_render_object_native(room[i]);
    for (int i = 0; i < CASTER_COUNT; i++) scene[3 + i] = athena_render_object_native(casters[i]);

    ath_shadow_volume *volumes[CASTER_COUNT];
    for (int i = 0; i < CASTER_COUNT; i++) {
        volumes[i] = shadow_volume_create(athena_render_object_native(casters[i]));
        if (volumes[i]) volumes[i]->extrusion = 30.0f;
    }

    // --- The torch ---
    const int light = athena_lights_new();
    athena_lights_set(light, ATHENA_LIGHT_AMBIENT, 0.08f, 0.08f, 0.1f);
    athena_lights_set(light, ATHENA_LIGHT_ATTENUATION, 0, 0, 0.02f);

    // Fire: additive camera-facing puffs from the soft cell of the shared FX
    // atlas, and a few embers.
    AthenaImage *fx_atlas = load_linear("fx_atlas.png");
    AthenaParticles *fire = particles_create(256, true, true, 1234);
    AthenaParticleRenderer *fire_renderer = athena_particles_renderer_create(fire, fx_atlas, GS_ALPHA_BLEND_ADD, true);
    const int flame = fire_emitter(fire, 0.25f, 0.5f, 0.22f, 0.34f, 0.2f,
        (const float[3]){ -0.15f, 0.6f, -0.15f }, (const float[3]){ 0.15f, 1.2f, 0.15f }, (const float[3]){ 0.05f, 0.02f, 0.05f },
        NULL, (const float[4]){ 128, 80, 30, 90 }, (const float[4]){ 110, 30, 8, 0 });
    const int embers = fire_emitter(fire, 0.6f, 1.2f, 0.03f, 0.05f, 0.3f,
        (const float[3]){ -0.4f, 0.5f, -0.4f }, (const float[3]){ 0.4f, 1.6f, 0.4f }, NULL,
        (const float[3]){ 0, -0.6f, 0 }, (const float[4]){ 128, 90, 30, 128 }, (const float[4]){ 128, 40, 10, 0 });
    particles_start(fire, flame, 90);
    particles_start(fire, embers, 12);

    // --- Camera ---
    // The camera follows the torch around the room from across it, outside its
    // path, looking at the flame: the torch stays in view and the pillars throw
    // their shadows towards the camera. The stick swings it round.
    float yaw = PI - 0.55f, pitch = 0.0f;

    AthenaPad *pad = athena_pad_open(0);
    const Color black = athena_color_rgba(0, 0, 0, 128);
    AthenaTimer *timer = athena_timer_create();
    bool shadows_on = true, torch_moving = true;
    float torch_t = 0.0f, last_time = 0.0f;

    for (;;) {
        athena_screen_clear(black);
        athena_render_module_begin();
        athena_pad_update(pad);

        if (athenaos_home_requested(pad)) {
            athena_particles_renderer_destroy(fire_renderer);
            particles_destroy(fire);
            break;
        }

        if (athena_pad_just_pressed(pad, PAD_CROSS)) shadows_on = !shadows_on;
        if (athena_pad_just_pressed(pad, PAD_SQUARE)) {
            mode = (mode + 1) % MODE_COUNT;
            APPLY_MODE();
        }
        if (athena_pad_just_pressed(pad, PAD_CIRCLE)) torch_moving = !torch_moving;

        const float now = (float)athena_timer_get_time(timer) / CLOCKS_PER_SEC;
        const float dt = fminf(0.1f, fmaxf(0, now - last_time));
        last_time = now;
        if (torch_moving) torch_t += dt;

        const float lx = (pad->lx > 25 || pad->lx < -25) ? pad->lx / 128.0f : 0;
        const float ly = (pad->ly > 25 || pad->ly < -25) ? pad->ly / 128.0f : 0;
        yaw += lx * dt * 1.6f;
        pitch = fmaxf(-1.0f, fminf(1.0f, pitch - ly * dt * 1.5f));

        float t[3];
        torch_path(torch_t, t);
        const float ca = torch_angle(torch_t) + yaw;
        athena_camera3d_position(sinf(ca) * CAM_RADIUS, CAM_HEIGHT, cosf(ca) * CAM_RADIUS);
        athena_camera3d_target(t[0] * 0.7f, 1.4f + pitch * 2.0f, t[2] * 0.7f);

        const float f = flicker(now);
        const float jitter = 0.03f * sinf(now * 29.0f);
        athena_lights_set(light, ATHENA_LIGHT_POSITION, t[0] + jitter, t[1], t[2] - jitter);
        athena_lights_set(light, ATHENA_LIGHT_DIFFUSE, 1.7f * f, 1.3f * f, 0.85f * f);

        set_xyz(floater, athena_render_object_native(floater)->rotation, 0.5f, now * 0.4f, 0.3f);

        athena_camera3d_update();
        athena_screen_set_param(DEPTH_TEST_ENABLE, true);
        athena_screen_set_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);

        for (int i = 0; i < 3; i++) athena_render_object_draw(room[i]);
        for (int i = 0; i < CASTER_COUNT; i++) athena_render_object_draw(casters[i]);

        // Shadowed means ambient only: those pixels are drawn again without the
        // torch, the way Doom 3 lights a scene.
        if (shadows_on && shadow_volumes_begin(NULL)) {
            for (int i = 0; i < CASTER_COUNT; i++) shadow_volume_render(volumes[i]);
            shadow_volumes_end_relight(scene, 3 + CASTER_COUNT);
        }

        particles_position(fire, flame, (const float[3]){ t[0], t[1] - 0.05f, t[2] });
        particles_position(fire, embers, t);
        particles_update(fire, dt);
        athena_particles_draw(fire_renderer);

        athena_screen_set_param(DEPTH_TEST_ENABLE, false);

        const prof_report *prof = athena_screen_get_profile();
        snprintf(ui.status, sizeof(ui.status), "%.1f FPS", athena_screen_get_fps(360));
        char l0[96], l1[96];
        snprintf(l0, sizeof(l0), "SQUARE  %s   CROSS  Shadows: %s (%d volumes)",
                 MODE_NAMES[mode], shadows_on ? "ON" : "OFF", CASTER_COUNT);
        snprintf(l1, sizeof(l1), "frame %.1f ms  |  render %.1f  |  gpu %.1f",
                 prof->frame_ms, prof->section_ms[PROF_RENDER], prof->section_ms[PROF_GPU_WAIT]);
        const char *lines[] = { l0, l1 };
        athenaos_hud(&ui, lines, 2, 0);
        athenaos_chrome(&ui);

        athena_screen_flip();
    }
    return 0;
}

ATHENAOS_APP(torchlight, torchlight_main)
