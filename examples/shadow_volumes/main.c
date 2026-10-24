/*
 * Shadow volumes, in C: the port of main.js.
 *
 * Stencil shadow volumes: each crate's silhouette, pushed away from the light,
 * counted into the red channel (+1 front faces, -1 back faces). Unlike the
 * projected shadows, a shadow lands on anything inside the volume -- the floor,
 * another crate, the crate itself.
 */

#include <math.h>
#include <stdio.h>
#include <unistd.h>

#include <athena/athena.h>
#include <shadow_volumes.h>
#include "athenaos/app.h"

static const AthenaOSHint HINTS[] = { { "L-stick", "Light" }, { "cross", "Shadows" }, { "circle", "Spin" } };

#define FLOOR 9.0f
#define FLOOR_Y -1.0f

static const struct { float x, y, z, ry; } CRATES[] = {
    { -2.6f, 0.0f, 0.8f, 0.3f },
    { 2.4f, 0.0f, -1.2f, -0.5f },
    { 0.0f, 0.0f, -0.2f, 0.0f },
    { 0.3f, 2.7f, 0.4f, 0.7f }, // floats over the middle one
};
#define CRATE_COUNT 4

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void set_xyz(AthenaRenderObject *ro, float *field, float x, float y, float z) {
    field[0] = x; field[1] = y; field[2] = z;
    update_object_space(athena_render_object_native(ro));
}

static int shadow_volumes_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Shadow volumes", "3d", HINTS, 3);

    athena_screen_set_frame_counter(true);
    athena_screen_set_vsync(true);

    AthenaVideoMode mode;
    athena_screen_get_mode(&mode);
    mode.zbuffering = true;
    mode.psm = GS_PSM_CT32;
    mode.psmz = GS_ZBUF_24; // the scene's red waits in the top byte of Z24
    athena_screen_set_mode(&mode);
    athena_screen_init_buffers();

    athenaos_loading(&ui, "Loading assets...", "Crates");

    athena_render_module_init();
    athena_render_module_set_view(60.0f, 1.0f, 400.0f, 0, 0);

    chdir("render");

    // Floor: two triangles, facing up.
    const VECTOR floor_pos[6] = {
        { -FLOOR, FLOOR_Y, -FLOOR, 1 }, { -FLOOR, FLOOR_Y, FLOOR, 1 }, { FLOOR, FLOOR_Y, FLOOR, 1 },
        { -FLOOR, FLOOR_Y, -FLOOR, 1 }, { FLOOR, FLOOR_Y, FLOOR, 1 }, { FLOOR, FLOOR_Y, -FLOOR, 1 },
    };
    VECTOR floor_nrm[6], floor_uv[6], floor_col[6];
    for (int i = 0; i < 6; i++) {
        floor_nrm[i][0] = 0; floor_nrm[i][1] = 1; floor_nrm[i][2] = 0; floor_nrm[i][3] = 1;
        floor_uv[i][0] = floor_uv[i][1] = floor_uv[i][2] = floor_uv[i][3] = 0;
        floor_col[i][0] = 0.55f; floor_col[i][1] = 0.55f; floor_col[i][2] = 0.6f; floor_col[i][3] = 1;
    }
    AthenaRenderData *floor_data = athena_render_data_from_vertices(floor_pos, floor_nrm, floor_uv, floor_col, 6);
    athena_render_data_native(floor_data)->attributes.texture_mapping = 0;
    athena_render_data_native(floor_data)->attributes.face_culling = CULL_FACE_NONE;
    AthenaRenderObject *floor_object = athena_render_object_create(floor_data);

    // Not frozen: a volume reads the crate's positions once, to find its edges.
    AthenaRenderData *crate = athena_render_data_load("box_bump.gltf", NULL);
    AthenaRenderObject *crates[CRATE_COUNT];
    ath_shadow_volume *volumes[CRATE_COUNT];
    for (int i = 0; i < CRATE_COUNT; i++) {
        crates[i] = athena_render_object_create(crate);
        athena_object_data *o = athena_render_object_native(crates[i]);
        set_xyz(crates[i], o->position, CRATES[i].x, CRATES[i].y, CRATES[i].z);
        set_xyz(crates[i], o->rotation, 0.0f, CRATES[i].ry, 0.0f);
    }
    for (int i = 0; i < CRATE_COUNT; i++) {
        volumes[i] = shadow_volume_create(athena_render_object_native(crates[i]));
        if (volumes[i])
            volumes[i]->extrusion = 30.0f;
    }

    athena_camera3d_position(0.0f, 6.0f, 12.0f);
    athena_camera3d_target(0.0f, 0.0f, 0.0f);

    const int light = athena_lights_new();
    athena_lights_set(light, ATHENA_LIGHT_AMBIENT, 0.25f, 0.25f, 0.28f);
    athena_lights_set(light, ATHENA_LIGHT_DIFFUSE, 0.85f, 0.82f, 0.75f);

    AthenaPad *pad = athena_pad_open(0);
    const Color sky = athena_color_rgba(52, 58, 70, 128);

    bool shadows_on = true, spin = true;
    float light_angle = 0.8f, light_pitch = 0.9f, float_angle = 0.7f;

    for (;;) {
        athena_screen_clear(sky);
        athena_render_module_begin();
        athena_pad_update(pad);

        if (athenaos_home_requested(pad))
            break;
        if (athena_pad_just_pressed(pad, PAD_CROSS)) shadows_on = !shadows_on;
        if (athena_pad_just_pressed(pad, PAD_CIRCLE)) spin = !spin;

        const float lx = (pad->lx > 25 || pad->lx < -25) ? pad->lx / 2048.0f : 0;
        const float ly = (pad->ly > 25 || pad->ly < -25) ? pad->ly / 2048.0f : 0;
        if (lx != 0 || ly != 0) {
            light_angle += lx;
            light_pitch = clampf(light_pitch - ly, 0.25f, 1.45f);
        } else {
            light_angle += 0.01f;
        }
        if (spin) float_angle += 0.012f;
        set_xyz(crates[3], athena_render_object_native(crates[3])->rotation, 0.4f, float_angle, 0.2f);

        // Direction towards the light.
        const float cp = cosf(light_pitch);
        athena_lights_set(light, ATHENA_LIGHT_DIRECTION, sinf(light_angle) * cp, sinf(light_pitch), cosf(light_angle) * cp);

        athena_camera3d_update();
        athena_screen_set_param(DEPTH_TEST_ENABLE, true);
        athena_screen_set_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);

        athena_render_object_draw(floor_object);
        for (int i = 0; i < CRATE_COUNT; i++)
            athena_render_object_draw(crates[i]);

        if (shadows_on && shadow_volumes_begin(NULL)) {
            for (int i = 0; i < CRATE_COUNT; i++)
                shadow_volume_render(volumes[i]);
            shadow_volumes_end(0.55f);
        }

        athena_screen_set_param(DEPTH_TEST_ENABLE, false);

        const prof_report *prof = athena_screen_get_profile();
        snprintf(ui.status, sizeof(ui.status), "%.1f FPS", athena_screen_get_fps(360));
        char l0[96], l1[96];
        snprintf(l0, sizeof(l0), "CROSS  Shadows: %s  (%d volumes, %u edges each)",
                 shadows_on ? "ON" : "OFF", CRATE_COUNT, volumes[0] ? (unsigned)volumes[0]->edge_count : 0u);
        snprintf(l1, sizeof(l1), "frame %.1f ms  |  render %.1f  |  gpu %.1f",
                 prof->frame_ms, prof->section_ms[PROF_RENDER], prof->section_ms[PROF_GPU_WAIT]);
        const char *lines[] = { l0, l1 };
        athenaos_hud(&ui, lines, 2, 0);
        athenaos_chrome(&ui);

        athena_screen_flip();
    }
    return 0;
}

ATHENAOS_APP(shadow_volumes, shadow_volumes_main)
