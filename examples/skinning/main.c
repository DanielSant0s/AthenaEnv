/*
 * Skinning demo, in C: the port of main.js. A skinned glTF character animated
 * on VU1, on a small set, with an orbit camera.
 */

#include <stdio.h>
#include <unistd.h>

#include <athena/athena.h>
#include "athenaos/app.h"

#define PI 3.14159265f

static const AthenaOSHint HINTS[] = { { "L-stick", "Orbit" }, { "L2/R2", "Zoom" }, { "cross", "Animation" } };

static void set_xyz(AthenaRenderObject *ro, float *field, float x, float y, float z) {
    field[0] = x; field[1] = y; field[2] = z;
    update_object_space(athena_render_object_native(ro));
}

static int skinning_main(int argc, char **argv) {
    (void)argc; (void)argv;

    // Before chdir("render"): the UI font and settings live in the boot folder.
    AthenaOSApp ui;
    athenaos_app_init(&ui, "Skinning demo", "3d", HINTS, 3);

    athena_screen_set_frame_counter(true);

    AthenaVideoMode mode;
    athena_screen_get_mode(&mode);
    mode.zbuffering = true;
    mode.psmz = GS_ZBUF_16S;
    athena_screen_set_mode(&mode);

    athena_render_module_init();
    athena_render_module_set_view(60.0f, 1.0f, 4000.0f, 0, 0);

    chdir("render");

    AthenaImage *sky = athena_image_create("sky.png", true);
    sky->width = mode.width;
    sky->height = mode.height;

    AthenaAnim3dCollection *anims = athena_anim3d_load("Twerk.gltf");
    AthenaRenderData *skin = athena_render_data_load("Twerk.gltf", NULL);
    athena_render_data_native(skin)->attributes.accurate_clipping = 1;
    athena_render_data_native(skin)->attributes.face_culling = CULL_FACE_BACK;
    athena_render_data_set_pipeline(skin, PL_DEFAULT);
    // Skinned meshes animate via bone matrices in the VU, not by touching
    // positions/normals/etc -- safe to reclaim the float source once setup is
    // done. Nothing here uses physics trimesh collision, so every mesh freezes.
    athena_render_data_freeze(skin);

    AthenaRenderObject *skin_object = athena_render_object_create(skin);
    set_xyz(skin_object, athena_render_object_native(skin_object)->rotation, PI / 2, 0.0f, 0.0f);
    athena_render_object_play_anim_blend(skin_object, athena_anim3d_get(anims, 4), false, 0.15f);

    AthenaRenderData *scene = athena_render_data_load("scene.gltf", NULL);
    athena_render_data_native(scene)->attributes.face_culling = CULL_FACE_NONE;
    athena_render_data_set_pipeline(scene, PL_DEFAULT);
    GSSURFACE *scene_tex = athena_render_data_get_texture(scene, 0);
    if (scene_tex)
        scene_tex->Filter = GS_FILTER_LINEAR;
    athena_render_data_freeze(scene);
    AthenaRenderObject *scene_object = athena_render_object_create(scene);

    AthenaRenderData *box = athena_render_data_load("box_bump.gltf", NULL);
    athena_render_data_native(box)->attributes.face_culling = CULL_FACE_NONE;
    athena_render_data_freeze(box);
    AthenaRenderObject *box_object = athena_render_object_create(box);
    set_xyz(box_object, athena_render_object_native(box_object)->position, 2.0f, 0.2f, 2.0f);
    set_xyz(box_object, athena_render_object_native(box_object)->scale, 0.2f, 0.2f, 0.2f);

    athena_camera3d_position(0.0f, 0.0f, 20.0f);
    athena_camera3d_target(0.0f, 1.0f, 0.0f);
    athena_camera3d_orbit(0.0f, 0.5f);

    const int light = athena_lights_new();
    athena_lights_set(light, ATHENA_LIGHT_DIRECTION, 0.0f, 1.0f, 1.0f);
    athena_lights_set(light, ATHENA_LIGHT_AMBIENT, 0.12f, 0.15f, 0.2f);
    athena_lights_set(light, ATHENA_LIGHT_DIFFUSE, 0.5f, 0.5f, 0.5f);
    athena_lights_set(light, ATHENA_LIGHT_SPECULAR, 1.0f, 1.0f, 0.0f);

    AthenaPad *pad = athena_pad_open(0);

    AthenaMemoryStats mem;
    athena_system_get_memory_stats(&mem);
    char free_mem[48];
    snprintf(free_mem, sizeof(free_mem), "RAM: %uMB / %uMB", (unsigned)(mem.used / 1048576), (unsigned)(GetMemorySize() / 1048576));
    const int free_vram = athena_screen_get_free_vram(VRAM_USED_TOTAL);

    const Color gray = athena_color_rgba(180, 180, 220, 128);

    athena_screen_set_param(ALPHA_TEST_ENABLE, false);
    athena_screen_set_param(ALPHA_TEST_METHOD, ALPHA_LESS);
    athena_screen_set_param(ALPHA_TEST_REF, 0x80);
    athena_screen_set_param(DEPTH_TEST_ENABLE, true);
    athena_screen_set_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);

    bool switch_anim = false;

    for (;;) {
        athena_screen_clear(gray);
        athena_camera3d_update();
        athena_pad_update(pad);

        const float lx = ((pad->lx > 25 || pad->lx < -25) ? pad->lx : 0) / 1024.0f;
        const float ly = ((pad->ly > 25 || pad->ly < -25) ? pad->ly : 0) / 1024.0f;
        if (lx != 0 || ly != 0)
            athena_camera3d_orbit(lx, ly);

        if (athena_pad_pressed(pad, PAD_R2))
            athena_camera3d_zoom(0.2f);
        else if (athena_pad_pressed(pad, PAD_L2))
            athena_camera3d_zoom(-0.2f);

        if (athenaos_home_requested(pad))
            break;

        if (athena_pad_just_pressed(pad, PAD_CROSS)) {
            athena_render_object_play_anim_blend(skin_object,
                athena_anim3d_find(anims, switch_anim ? "twerk" : "capoeira"), true, 0.15f);
            switch_anim = !switch_anim;
        }

        athena_screen_set_param(DEPTH_TEST_ENABLE, false);
        athena_image_draw(sky, 0, 0);
        athena_screen_set_param(DEPTH_TEST_ENABLE, true);
        athena_screen_set_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);

        athena_render_object_draw(scene_object);
        athena_render_object_draw(box_object);
        athena_render_object_draw(skin_object);

        athena_screen_set_param(DEPTH_TEST_ENABLE, false);

        snprintf(ui.status, sizeof(ui.status), "%.1f FPS", athena_screen_get_fps(360));
        char line[96];
        snprintf(line, sizeof(line), "%u vertices  |  %s  |  VRAM %d KB",
                 (unsigned)athena_render_data_native(skin)->index_count, free_mem, free_vram / 1024);
        const char *lines[] = { line };
        athenaos_hud(&ui, lines, 1, 0);
        athenaos_chrome(&ui);

        athena_screen_flip();
    }
    return 0;
}

ATHENAOS_APP(skinning, skinning_main)
