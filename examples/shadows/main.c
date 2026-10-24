/*
 * Shadows demo, in C: the port of main.js.
 *
 * A skinned character and a crate cast projected shadows: each is drawn into
 * an offscreen target from a light-aligned camera, then a projector grid,
 * raycast onto the scene's collision mesh, lays the silhouette on the ground.
 * Plus a triangle-strip ribbon that checks face culling's strip parity.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <athena/athena.h>
#include "athenaos/app.h"

#define PI 3.14159265f

static const AthenaOSHint HINTS[] = {
    { "L-stick", "Orbit" }, { "L2/R2", "Zoom" }, { "cross", "Animation" }, { "square", "Cull" }, { "circle", "Clip" },
};

#define RIBBON_QUADS 8

static void set_xyz(AthenaRenderObject *ro, float *field, float x, float y, float z) {
    field[0] = x; field[1] = y; field[2] = z;
    update_object_space(athena_render_object_native(ro));
}

// world.stepWithContacts(space, jgroup, dt) of the script binding.
typedef struct { AthenaPhysicsWorld *world; AthenaPhysicsJointGroup *group; } ContactData;

static void near_callback(void *data, AthenaPhysicsGeom *o1, AthenaPhysicsGeom *o2) {
    ContactData *cd = (ContactData *)data;
    AthenaPhysicsBody *b1 = athena_physics_geom_get_body(o1), *b2 = athena_physics_geom_get_body(o2);
    if (!b1 && !b2)
        return;
    if (b1 && b2 && athena_physics_bodies_connected_excluding(b1, b2, ATH_JOINT_CONTACT))
        return;
    AthenaContact contacts[ATH_PHYS_MAX_CONTACTS] __attribute__((aligned(16)));
    const int n = athena_physics_collide(o1, o2, ATH_PHYS_MAX_CONTACTS, contacts);
    for (int i = 0; i < n; i++) {
        contacts[i].mu = 0.5f;
        contacts[i].bounce = 0.1f;
        contacts[i].soft_cfm = 0.01f;
        athena_physics_joint_attach(athena_physics_joint_create_contact(cd->world, cd->group, &contacts[i]), b1, b2);
    }
}

static void world_step(AthenaPhysicsWorld *world, AthenaPhysicsSpace *space, AthenaPhysicsJointGroup *group, float dt) {
    ContactData cd = { world, group };
    athena_physics_space_collide(space, &cd, near_callback);
    athena_physics_world_step(world, dt);
    athena_physics_joint_group_empty(group);
}

// Points the camera at (tx, ty, tz) from one unit back along the light.
static void light_camera(const float light_dir[3], float tx, float ty, float tz) {
    const float len = sqrtf(light_dir[0] * light_dir[0] + light_dir[1] * light_dir[1] + light_dir[2] * light_dir[2]);
    const float dx = len > 0 ? light_dir[0] / len : 0.0f;
    const float dy = len > 0 ? light_dir[1] / len : 1.0f;
    const float dz = len > 0 ? light_dir[2] / len : 0.0f;
    const float dist = 1.0f;
    athena_camera3d_position(tx - dx * dist, ty - dy * dist, tz - dz * dist);
    athena_camera3d_target(tx, ty, tz);
    athena_camera3d_update();
}

// The projector's position, as the script's projector.position setter.
static void projector_position(AthenaShadowProjector *p, float x, float y, float z) {
    athena_object_data *o = athena_shadow_projector_object(p);
    o->position[0] = x; o->position[1] = y; o->position[2] = z;
    athena_shadow_projector_sync_transform(p);
}

// Untextured, flat, unlit (the silhouette pass) or the regular look.
static void silhouette(AthenaRenderData *data, bool on) {
    athena_render_data *m = athena_render_data_native(data);
    m->attributes.texture_mapping = on ? 0 : 1;
    m->attributes.shade_model = on ? 0 : 1;
    m->pipeline = on ? PL_NO_LIGHTS : PL_DEFAULT;
}

static int shadows_main(int argc, char **argv) {
    (void)argc; (void)argv;

    // Before chdir("render"): the UI font and settings live in the boot folder.
    AthenaOSApp ui;
    athenaos_app_init(&ui, "Shadows demo", "3d", HINTS, 5);

    athena_screen_set_frame_counter(true);

    AthenaVideoMode mode;
    athena_screen_get_mode(&mode);
    mode.zbuffering = true;
    mode.psmz = GS_ZBUF_16S;
    athena_screen_set_mode(&mode);
    athena_screen_init_buffers();

    AthenaImage *depth_buffer = athena_screen_get_buffer(DEPTH_BUFFER);

    athena_render_module_init();
    athena_render_module_set_view(60.0f, 1.0f, 4000.0f, 0, 0);

    // Shadow render target (offscreen)
    AthenaImage *shadow_rt = athena_image_create_empty(true);
    shadow_rt->tex->Filter = GS_FILTER_LINEAR;
    shadow_rt->tex->PageAligned = 1; // renderable
    shadow_rt->tex->PSM = GS_PSM_CT32;
    shadow_rt->tex->Width = shadow_rt->tex->Height = 256;
    shadow_rt->width = shadow_rt->height = 256;
    shadow_rt->endx = shadow_rt->endy = 256;
    athena_image_lock(shadow_rt);

    // Shadow texture (outside render/ root)
    AthenaImage *blob_tex = athena_image_create("simple_atlas.png", true);
    athena_image_set_filter(blob_tex, GS_FILTER_LINEAR);

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
    // positions/normals/etc, and this mesh isn't used with physics trimesh
    // collision -- safe to reclaim its float source now that setup is done.
    athena_render_data_freeze(skin);

    AthenaRenderObject *skin_object = athena_render_object_create(skin);
    set_xyz(skin_object, athena_render_object_native(skin_object)->rotation, PI / 2, 0.0f, 0.0f);
    athena_render_object_play_anim_blend(skin_object, athena_anim3d_get(anims, 4), false, 0.15f);

    AthenaPhysicsWorld *world = athena_physics_world_create();
    AthenaPhysicsSpace *space = athena_physics_space_create(NULL);
    AthenaPhysicsJointGroup *jgroup = athena_physics_joint_group_create();

    // Shadow projector for the skinned character; the light direction is
    // shared between the projector and the shadow camera.
    const float light_dir[3] = { 0.0f, 1.0f, 1.0f };
    AthenaShadowProjector *proj = athena_shadow_projector_create(shadow_rt->tex);
    athena_shadow_projector_set_size(proj, 2.0f, 2.0f);
    athena_shadow_projector_set_grid(proj, 12, 12);
    athena_shadow_projector_set_light_dir(proj, light_dir[0], light_dir[1], light_dir[2]);
    athena_shadow_projector_set_bias(proj, -0.02f);
    athena_shadow_projector_set_color(proj, 0.0f, 0.0f, 0.0f, 0.65f);
    athena_shadow_projector_set_blend(proj, SHADOW_BLEND_DARKEN);
    athena_shadow_projector_enable_raycast(proj, space, 12.0f, 4);
    athena_shadow_projector_set_light_offset(proj, 1.0f);
    athena_shadow_projector_set_uv_rect(proj, 1.0f, 0.0f, 0.0f, 1.0f);

    athena_screen_switch_context();
    athena_screen_set_buffer(DRAW_BUFFER, shadow_rt, 0);
    athena_screen_set_buffer(DEPTH_BUFFER, depth_buffer, 1); // context 2 needs no Z buffer: masked
    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
    athena_screen_switch_context();

    AthenaRenderData *scene = athena_render_data_load("scene.gltf", NULL);
    athena_render_data_native(scene)->attributes.face_culling = CULL_FACE_BACK;
    athena_render_data_native(scene)->attributes.accurate_clipping = 1;
    athena_render_data_set_pipeline(scene, PL_DEFAULT);
    GSSURFACE *scene_tex = athena_render_data_get_texture(scene, 0);
    if (scene_tex)
        scene_tex->Filter = GS_FILTER_LINEAR;
    AthenaRenderObject *scene_object = athena_render_object_create(scene);
    // Not frozen: the projector raycasts onto a trimesh built from its positions.
    athena_physics_geom_create_from_render_object(space, athena_render_object_native(scene_object));

    AthenaRenderData *box = athena_render_data_load("box_bump.gltf", NULL);
    athena_render_data_native(box)->attributes.face_culling = CULL_FACE_BACK;
    athena_render_data_freeze(box); // no trimesh collision for it
    AthenaRenderObject *box_object = athena_render_object_create(box);
    set_xyz(box_object, athena_render_object_native(box_object)->position, 2.0f, 0.2f, 2.0f);
    set_xyz(box_object, athena_render_object_native(box_object)->scale, 0.2f, 0.2f, 0.2f);

    // --- Tristrip + face culling test ---
    // A vertical ribbon built as ONE triangle strip: consecutive triangles share
    // two vertices, so their winding alternates. That alternation is what face
    // culling has to keep in step with -- if the parity is wrong, half the quads
    // vanish and the ribbon comes out STRIPED. Right, it is all-or-nothing: solid
    // from the front, gone once you orbit past its edge.
    //
    // SQUARE toggles culling, CIRCLE toggles accurate clipping (the two live in
    // different VU paths, and each has its own copy of the parity flip).
    enum { RIBBON_VERTICES = (RIBBON_QUADS + 1) * 2 };
    VECTOR ribbon_pos[RIBBON_VERTICES], ribbon_col[RIBBON_VERTICES];
    for (int i = 0; i <= RIBBON_QUADS; i++) {
        const float x = -4.0f + (8.0f * i) / RIBBON_QUADS;
        // Top vertex before bottom, so the first triangle comes out counter-
        // clockwise towards +Z, where the camera starts: front-facing.
        float *top = ribbon_pos[i * 2], *bottom = ribbon_pos[i * 2 + 1];
        top[0] = x; top[1] = 4.0f; top[2] = -3.0f; top[3] = 1.0f;
        bottom[0] = x; bottom[1] = 1.0f; bottom[2] = -3.0f; bottom[3] = 1.0f;
        // Alternating column colors: makes it obvious which quads survived.
        const bool warm = (i % 2) == 0;
        for (int k = 0; k < 2; k++) {
            float *c = ribbon_col[i * 2 + k];
            c[0] = warm ? 0.95f : 0.15f; c[1] = 0.35f; c[2] = warm ? 0.15f : 0.95f; c[3] = 1.0f;
        }
    }
    // No normals and no texcoords: PL_NO_LIGHTS reads neither.
    AthenaRenderData *ribbon = athena_render_data_from_vertices(ribbon_pos, NULL, NULL, ribbon_col, RIBBON_VERTICES);
    athena_render_data *rib = athena_render_data_native(ribbon);
    rib->tristrip = true;
    rib->pipeline = PL_NO_LIGHTS;
    rib->attributes.texture_mapping = 0;
    rib->attributes.accurate_clipping = 0;
    rib->attributes.face_culling = CULL_FACE_BACK;
    AthenaRenderObject *ribbon_object = athena_render_object_create(ribbon);
    bool ribbon_culling = true, ribbon_clipping = false;

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
    const Color clear_rt = athena_color_rgba(0, 0, 0, 0);

    athena_screen_set_param(ALPHA_TEST_ENABLE, false);
    athena_screen_set_param(ALPHA_TEST_METHOD, ALPHA_LESS);
    athena_screen_set_param(ALPHA_TEST_REF, 0x80);
    athena_screen_set_param(DEPTH_TEST_ENABLE, true);
    athena_screen_set_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);

    bool switch_anim = false;
    athena_object_data *skin_o = athena_render_object_native(skin_object);
    athena_object_data *box_o = athena_render_object_native(box_object);

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
        if (athena_pad_just_pressed(pad, PAD_SQUARE)) {
            ribbon_culling = !ribbon_culling;
            rib->attributes.face_culling = ribbon_culling ? CULL_FACE_BACK : CULL_FACE_NONE;
        }
        if (athena_pad_just_pressed(pad, PAD_CIRCLE)) {
            ribbon_clipping = !ribbon_clipping;
            rib->attributes.accurate_clipping = ribbon_clipping;
        }

        athena_screen_set_param(DEPTH_TEST_ENABLE, false);
        athena_image_draw(sky, 0, 0);

        // Offscreen pass: the skin into shadow_rt from a light-aligned camera.
        AthenaCamera3dState main_cam;
        athena_camera3d_save(&main_cam);
        athena_screen_switch_context();
        athena_draw_rect(0, 0, shadow_rt->width, shadow_rt->height, clear_rt);
        athena_render_module_set_view(60.0f, 1.0f, 4000.0f, shadow_rt->width, shadow_rt->height);
        light_camera(light_dir, skin_o->position[0], skin_o->position[1], skin_o->position[2] + 0.1f);
        silhouette(skin, true);
        athena_render_object_draw(skin_object);
        athena_camera3d_restore(&main_cam);
        athena_camera3d_update();
        athena_screen_switch_context();
        athena_render_module_set_view(60.0f, 1.0f, 4000.0f, 0, 0);

        athena_screen_set_param(DEPTH_TEST_ENABLE, true);
        athena_screen_set_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);

        athena_render_object_draw(scene_object);
        athena_render_object_draw(ribbon_object);

        // The projector follows the character on XZ, projected on the ground (y = 0).
        projector_position(proj, skin_o->position[0], 0.0f, skin_o->position[2]);
        athena_shadow_projector_set_light_offset(proj, 1.0f);
        athena_shadow_projector_render(proj);
        world_step(world, space, jgroup, 0.01f);

        silhouette(skin, false);
        athena_render_object_draw(skin_object);

        // The crate, the same way.
        athena_screen_switch_context();
        athena_draw_rect(0, 0, shadow_rt->width, shadow_rt->height, clear_rt);
        athena_render_module_set_view(60.0f, 1.0f, 4000.0f, shadow_rt->width, shadow_rt->height);
        light_camera(light_dir, box_o->position[0], box_o->position[1], box_o->position[2]);
        silhouette(box, true);
        set_xyz(box_object, box_o->scale, 0.23f, 0.23f, 0.23f);
        athena_render_object_draw(box_object);
        athena_screen_switch_context();
        athena_camera3d_restore(&main_cam);
        athena_camera3d_update();
        athena_render_module_set_view(60.0f, 1.0f, 4000.0f, 0, 0);

        projector_position(proj, box_o->position[0], 0.0f, box_o->position[2]);
        athena_shadow_projector_set_light_offset(proj, 1.2f);
        athena_shadow_projector_render(proj);
        world_step(world, space, jgroup, 0.01f);

        silhouette(box, false);
        set_xyz(box_object, box_o->scale, 0.2f, 0.2f, 0.2f);
        athena_render_object_draw(box_object);

        athena_screen_set_param(DEPTH_TEST_ENABLE, false);

        snprintf(ui.status, sizeof(ui.status), "%.1f FPS", athena_screen_get_fps(360));
        char l0[96], l1[96];
        snprintf(l0, sizeof(l0), "%u vertices  |  %s  |  VRAM %d KB",
                 (unsigned)athena_render_data_native(skin)->index_count, free_mem, free_vram / 1024);
        snprintf(l1, sizeof(l1), "Tristrip ribbon: %d quads  |  cull %s  |  clip %s",
                 RIBBON_QUADS, ribbon_culling ? "BACK" : "NONE", ribbon_clipping ? "ACCURATE" : "FAST");
        const char *lines[] = { l0, l1, "Ribbon solid or gone = ok, striped = wrong parity" };
        athenaos_hud(&ui, lines, 3, 0);
        athenaos_chrome(&ui);

        athena_screen_flip();
    }
    return 0;
}

ATHENAOS_APP(shadows, shadows_main)
