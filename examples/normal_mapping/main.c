/*
 * Normal mapping, in C: the port of main.js.
 *
 * DOT3 normal mapping after "DOT3 Normal Mapping on the PS2" (Mikkelsen, 2004).
 * Dungeon: a corner of brick walls and stone slabs, plus an octagonal pillar --
 * a few big triangles that Gouraud can only light flat, while DOT3 brings out
 * every brick, chip and crack as the light sweeps across. Side by side:
 * Gouraud | 2-pass | 4-pass, as crates and as low-poly spheres -- a flat face
 * gets the same light vector at every vertex, so only a curved surface shows
 * the 2-pass light losing length between vertices. Stress: nine crates, all in
 * one mode, for the cost of each. The light circles the objects; the left stick
 * moves it by hand.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <athena/athena.h>
#include <normalmap.h>
#include "athenaos/app.h"

#define PI 3.14159265f

static const AthenaOSHint HINTS[] = {
    { "L-stick", "Light" }, { "cross", "Layout" }, { "square", "Mode" }, { "circle", "Spin" },
};

static const int MODES[] = { DOT3_OFF, DOT3_2PASS, DOT3_4PASS };
static const char *const MODE_NAMES[] = { "Gouraud + emboss", "DOT3 2-pass", "DOT3 4-pass" };
#define MODE_COUNT 3

enum { DUNGEON, SIDE, SPHERES, STRESS, LAYOUT_COUNT };
static const char *const LAYOUT_NAMES[] = { "dungeon", "crates side by side", "spheres side by side", "9 crates" };

// --- Geometry built in code ---

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

// The mesh as one RenderData, diffuse and normal map on its single material.
static AthenaRenderData *mesh_data(Mesh *m, AthenaImage *diffuse, AthenaImage *normal, int mode) {
    AthenaRenderData *data = athena_render_data_from_vertices(m->pos, m->nrm, m->uv, m->col, m->count);
    athena_render_data *n = athena_render_data_native(data);
    n->attributes.face_culling = CULL_FACE_NONE;
    n->materials[0].diffuse[0] = n->materials[0].diffuse[1] = n->materials[0].diffuse[2] = 1.0f;
    n->materials[0].diffuse[3] = 1.0f;
    n->materials[0].texture_id = athena_render_data_push_texture(data, diffuse->tex);
    n->materials[0].normal_texture_id = athena_render_data_push_texture(data, normal->tex);
    n->attributes.normal_mapping = mode;
    return data;
}

static void mesh_free(Mesh *m) {
    free(m->pos); free(m->nrm); free(m->uv); free(m->col);
    memset(m, 0, sizeof(*m));
}

// A UV sphere as a triangle list, the crate maps wrapped twice around it.
static void sphere_put(Mesh *m, int r, int s, int rings, int segments, float radius) {
    const float theta = PI * r / rings, phi = 2 * PI * s / segments;
    const float n[3] = { sinf(theta) * cosf(phi), cosf(theta), sinf(theta) * sinf(phi) };
    const float p[3] = { n[0] * radius, n[1] * radius, n[2] * radius };
    mesh_put(m, p, n, 2.0f * s / segments, (float)r / rings);
}

static AthenaRenderData *make_sphere(int rings, int segments, float radius, int mode,
                                     AthenaImage *diffuse, AthenaImage *normal) {
    Mesh m = { 0 };
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segments; s++) {
            sphere_put(&m, r, s, rings, segments, radius);
            sphere_put(&m, r + 1, s, rings, segments, radius);
            sphere_put(&m, r + 1, s + 1, rings, segments, radius);
            sphere_put(&m, r, s, rings, segments, radius);
            sphere_put(&m, r + 1, s + 1, rings, segments, radius);
            sphere_put(&m, r, s + 1, rings, segments, radius);
        }
    }
    AthenaRenderData *data = mesh_data(&m, diffuse, normal, mode);
    mesh_free(&m);
    return data;
}

// A rectangle from its top-left corner o and the edges u (rightwards) and v
// (downwards): texture u along u, texture v along v, repeated ru x rv times.
static void quad(Mesh *m, const float o[3], const float u[3], const float v[3], const float n[3], float ru, float rv) {
    static const float st[6][2] = { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 0, 0 }, { 1, 1 }, { 1, 0 } };
    for (int i = 0; i < 6; i++) {
        const float s = st[i][0], t = st[i][1];
        const float p[3] = { o[0] + u[0] * s + v[0] * t, o[1] + u[1] * s + v[1] * t, o[2] + u[2] * s + v[2] * t };
        mesh_put(m, p, n, s * ru, t * rv);
    }
}

// The pillar: 8 sides, smooth normals, the bricks once around.
static void pillar_side(Mesh *m, float x, float z, float radius, float height, int k, float t) {
    const float a = 2 * PI * k / 8;
    const float n[3] = { sinf(a), 0, cosf(a) };
    const float p[3] = { x + n[0] * radius, height * (1 - t), z + n[2] * radius };
    mesh_put(m, p, n, k / 8.0f, t * height / 4);
}

static void pillar(Mesh *m, float x, float z, float radius, float height) {
    for (int k = 0; k < 8; k++) {
        pillar_side(m, x, z, radius, height, k, 0); pillar_side(m, x, z, radius, height, k, 1);
        pillar_side(m, x, z, radius, height, k + 1, 1); pillar_side(m, x, z, radius, height, k, 0);
        pillar_side(m, x, z, radius, height, k + 1, 1); pillar_side(m, x, z, radius, height, k + 1, 0);
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

// --- Automatic benchmark ---
// Cycles the stress layout through the three modes, BENCH_PHASE_MS each, and
// prints the averaged frame profile of every phase. VSync is off meanwhile, so
// the frame time is what the frame really costs.
#define BENCHMARK 0
#define BENCH_PHASE_MS 10000.0f
#define BENCH_WARMUP_MS 2000.0f

static int normal_mapping_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Normal mapping", "3d", HINTS, 4);

    athena_screen_set_frame_counter(true);

    AthenaVideoMode vmode;
    athena_screen_get_mode(&vmode);
    vmode.zbuffering = true;
    vmode.psm = GS_PSM_CT32;
    vmode.psmz = GS_ZBUF_24; // DOT3 keeps its scratch byte in the top of Z24
    athena_screen_set_mode(&vmode);
    athena_screen_init_buffers();

    athenaos_loading(&ui, "Loading assets...", "Bricks, slabs and crates");

    athena_render_module_init();
    athena_render_module_set_view(60.0f, 1.0f, 400.0f, 0, 0);

    chdir("render");

    // One RenderData per mode: the mode is a property of the data.
    AthenaRenderData *crates[MODE_COUNT];
    for (int i = 0; i < MODE_COUNT; i++) {
        crates[i] = athena_render_data_load("box_bump.gltf", NULL);
        athena_render_data_native(crates[i])->attributes.normal_mapping = MODES[i];
    }

    AthenaImage *diffuse_map = load_linear("box_diffuse.png");
    AthenaImage *normal_map = load_linear("box_normalmap.png");
    AthenaRenderData *spheres[MODE_COUNT];
    for (int i = 0; i < MODE_COUNT; i++)
        spheres[i] = make_sphere(8, 12, 1.5f, MODES[i], diffuse_map, normal_map);

    // --- Dungeon ---
    // Procedural, tileable 256x256 maps (bricks and stone slabs) whose relief is
    // what Gouraud cannot show: flat albedo, all the shape in the normal map.
    AthenaImage *brick_diffuse = load_linear("bricks_diffuse.png");
    AthenaImage *brick_normal = load_linear("bricks_normal.png");
    AthenaImage *tile_diffuse = load_linear("tiles_diffuse.png");
    AthenaImage *tile_normal = load_linear("tiles_normal.png");

    const float ROOM = 4.0f, WALL = 4.0f;
    AthenaRenderObject *dungeon[MODE_COUNT][2];
    for (int i = 0; i < MODE_COUNT; i++) {
        Mesh walls = { 0 }, floor = { 0 };
        quad(&walls, (float[3]){ -ROOM, WALL, -ROOM }, (float[3]){ 2 * ROOM, 0, 0 }, (float[3]){ 0, -WALL, 0 },
             (float[3]){ 0, 0, 1 }, 2, 1);   // back wall
        quad(&walls, (float[3]){ -ROOM, WALL, ROOM }, (float[3]){ 0, 0, -2 * ROOM }, (float[3]){ 0, -WALL, 0 },
             (float[3]){ 1, 0, 0 }, 2, 1);   // left wall
        pillar(&walls, 1.4f, -1.2f, 0.6f, WALL);
        quad(&floor, (float[3]){ -ROOM, 0, -ROOM }, (float[3]){ 2 * ROOM, 0, 0 }, (float[3]){ 0, 0, 2 * ROOM },
             (float[3]){ 0, 1, 0 }, 2, 2);   // floor
        dungeon[i][0] = athena_render_object_create(mesh_data(&walls, brick_diffuse, brick_normal, MODES[i]));
        dungeon[i][1] = athena_render_object_create(mesh_data(&floor, tile_diffuse, tile_normal, MODES[i]));
        mesh_free(&walls);
        mesh_free(&floor);
    }

    AthenaRenderObject *side_objects[MODE_COUNT], *sphere_objects[MODE_COUNT];
    for (int i = 0; i < MODE_COUNT; i++) {
        side_objects[i] = athena_render_object_create(crates[i]);
        set_xyz(side_objects[i], athena_render_object_native(side_objects[i])->position, (i - 1) * 3.2f, 0.0f, 0.0f);
        sphere_objects[i] = athena_render_object_create(spheres[i]);
        set_xyz(sphere_objects[i], athena_render_object_native(sphere_objects[i])->position, (i - 1) * 3.4f, 0.0f, 0.0f);
    }

    // 3x3 crates per mode; the stress layout shows the current mode's nine.
    AthenaRenderObject *stress[MODE_COUNT][9];
    for (int mode = 0; mode < MODE_COUNT; mode++) {
        for (int i = 0; i < 9; i++) {
            stress[mode][i] = athena_render_object_create(crates[mode]);
            set_xyz(stress[mode][i], athena_render_object_native(stress[mode][i])->position,
                    ((i % 3) - 1) * 3.0f, ((i / 3) - 1) * 2.6f, -2.0f);
        }
    }

    athena_camera3d_position(0.0f, 0.0f, 9.0f);
    athena_camera3d_target(0.0f, 0.0f, 0.0f);

    const int light = athena_lights_new();
    athena_lights_set(light, ATHENA_LIGHT_AMBIENT, 0.18f, 0.18f, 0.18f);
    athena_lights_set(light, ATHENA_LIGHT_DIFFUSE, 0.9f, 0.9f, 0.9f);

    AthenaPad *pad = athena_pad_open(0);
    const Color gray = athena_color_rgba(40, 40, 40, 128);

    int layout = DUNGEON, shown_mode = 2;
    bool spin = true;
    float light_angle = 0.6f, light_pitch = 0.5f, crate_angle = 0.4f;
    float sweep = 0.01f; // dungeon: the light swings across the walls, low and grazing

    athena_screen_set_vsync(!BENCHMARK);
    int bench_phase = 0, bench_samples = 0, bench_frames = 0;
    double bench_sum[5] = { 0 };
    AthenaTimer *bench_timer = athena_timer_create();
    if (BENCHMARK) {
        printf("[bench] automatic benchmark on: 3 phases, %d s each\n", (int)(BENCH_PHASE_MS / 1000));
        layout = STRESS;
        shown_mode = bench_phase;
    }

    for (;;) {
        athena_screen_clear(gray);
        athena_render_module_begin();
        athena_pad_update(pad);

        if (athenaos_home_requested(pad))
            break;

        if (!BENCHMARK) {
            if (athena_pad_just_pressed(pad, PAD_CROSS)) layout = (layout + 1) % LAYOUT_COUNT;
            if (athena_pad_just_pressed(pad, PAD_SQUARE)) shown_mode = (shown_mode + 1) % MODE_COUNT;
            if (athena_pad_just_pressed(pad, PAD_CIRCLE)) spin = !spin;
        }

        const float lx = (pad->lx > 25 || pad->lx < -25) ? pad->lx / 2048.0f : 0;
        const float ly = (pad->ly > 25 || pad->ly < -25) ? pad->ly / 2048.0f : 0;
        if (lx != 0 || ly != 0) {
            light_angle += lx;
            light_pitch = fmaxf(-1.2f, fminf(1.2f, light_pitch - ly));
        } else if (layout == DUNGEON) {
            if (fabsf(light_angle) > 1.25f) sweep = light_angle > 0 ? -0.01f : 0.01f;
            light_angle += sweep;
        } else {
            light_angle += 0.012f;
        }
        if (spin) crate_angle += 0.006f;

        // Direction towards the light.
        const float cp = cosf(light_pitch);
        athena_lights_set(light, ATHENA_LIGHT_DIRECTION, sinf(light_angle) * cp, sinf(light_pitch), cosf(light_angle) * cp);

        if (layout == DUNGEON) {
            athena_camera3d_position(1.0f, 2.2f, 6.0f);
            athena_camera3d_target(-0.6f, 1.4f, -2.0f);
        } else {
            athena_camera3d_position(0.0f, 0.0f, 9.0f);
            athena_camera3d_target(0.0f, 0.0f, 0.0f);
        }
        athena_camera3d_update();
        athena_screen_set_param(DEPTH_TEST_ENABLE, true);
        athena_screen_set_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);

        if (layout == DUNGEON) {
            athena_render_object_draw(dungeon[shown_mode][0]);
            athena_render_object_draw(dungeon[shown_mode][1]);
        } else {
            AthenaRenderObject **objects = layout == SIDE ? side_objects : layout == SPHERES ? sphere_objects : stress[shown_mode];
            const int count = layout == STRESS ? 9 : MODE_COUNT;
            for (int i = 0; i < count; i++) {
                set_xyz(objects[i], athena_render_object_native(objects[i])->rotation, 0.35f, crate_angle, 0.0f);
                athena_render_object_draw(objects[i]);
            }
        }

        athena_screen_set_param(DEPTH_TEST_ENABLE, false);

        const prof_report *prof = athena_screen_get_profile();
        snprintf(ui.status, sizeof(ui.status), "%.1f FPS", athena_screen_get_fps(360));
        char l0[96], l1[96], l2[96];
        snprintf(l0, sizeof(l0), "CROSS  Layout: %s", LAYOUT_NAMES[layout]);
        if (layout == SIDE || layout == SPHERES)
            snprintf(l1, sizeof(l1), "%s  |  %s  |  %s", MODE_NAMES[0], MODE_NAMES[1], MODE_NAMES[2]);
        else
            snprintf(l1, sizeof(l1), "SQUARE  Mode: %s", MODE_NAMES[shown_mode]);
        snprintf(l2, sizeof(l2), "frame %.1f ms  |  render %.1f  |  gpu %.1f",
                 prof->frame_ms, prof->section_ms[PROF_RENDER], prof->section_ms[PROF_GPU_WAIT]);
        const char *lines[] = { l0, l1, l2 };
        athenaos_hud(&ui, lines, 3, 0);
        athenaos_chrome(&ui);

        athena_screen_flip();

        if (BENCHMARK) {
            const float elapsed = (float)athena_timer_get_time(bench_timer) * 1000.0f / CLOCKS_PER_SEC;
            bench_frames++;
            if (elapsed >= BENCH_WARMUP_MS && bench_frames > 60) {
                const prof_report *r = athena_screen_get_profile();
                bench_sum[0] += r->frame_ms;
                bench_sum[1] += r->section_ms[PROF_SCRIPT];
                bench_sum[2] += r->section_ms[PROF_RENDER];
                bench_sum[3] += r->section_ms[PROF_GPU_WAIT];
                bench_sum[4] += r->section_ms[PROF_VSYNC];
                bench_samples++;
            }
            if (elapsed >= BENCH_PHASE_MS && bench_samples > 0) {
                printf("[bench] %s x9 | %.1f fps | frame %.1f | script %.1f | render %.1f | gpu wait %.1f | vsync %.1f (ms)\n",
                       MODE_NAMES[bench_phase], 1000.0 / (bench_sum[0] / bench_samples), bench_sum[0] / bench_samples,
                       bench_sum[1] / bench_samples, bench_sum[2] / bench_samples, bench_sum[3] / bench_samples,
                       bench_sum[4] / bench_samples);
                bench_phase = (bench_phase + 1) % MODE_COUNT;
                shown_mode = bench_phase;
                memset(bench_sum, 0, sizeof(bench_sum));
                bench_samples = bench_frames = 0;
                athena_timer_reset(bench_timer);
            }
        }
    }
    return 0;
}

ATHENAOS_APP(normal_mapping, normal_mapping_main)
