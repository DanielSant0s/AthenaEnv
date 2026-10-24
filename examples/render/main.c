/*
 * Render demo, in C: the port of main.js.
 *
 * A model viewer over the renderer's features: OBJ and glTF meshes, a mesh
 * built in code, decals drawn live into an offscreen target, an environment
 * map, the three lighting pipelines, a scene graph, a sorted render batch and
 * a model streamed in by the async loader.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <athena/athena.h>
#include "athenaos/app.h"

static const AthenaOSHint HINTS[] = {
    { "L-stick", "Rotate" }, { "D-pad", "Model/Pipeline" }, { "cross", "Clip" }, { "square", "Shade" },
    { "circle", "Texture" }, { "R1", "BBox" },
};

static const char *const PIPELINES[] = { "NO_LIGHTS", "DEFAULT", "SPECULAR" };
#define PIPELINE_COUNT 3

#define MODEL_COUNT 7

// What the async loader's callback reaches.
typedef struct {
    AthenaSceneNode *root;
    AthenaRenderBatch *batch;
    AthenaRenderObject *streamed[8];
    int streamed_count;
} Streaming;

// BoxTextured.gltf arrived: wrap it, freeze it and hang it above the others.
static void on_loaded(athena_render_data *m, void *user) {
    Streaming *s = (Streaming *)user;
    // Moved into an AthenaRenderData with the usual defaults, as the script binding does.
    AthenaRenderData *data = athena_render_data_create();
    memcpy(&data->m, m, sizeof(athena_render_data));
    free(m);
    athena_render_data_apply_defaults(data);
    FlushCache(WRITEBACK_DCACHE);
    printf("[Render] loaded: BoxTextured.gltf size: %u\n", (unsigned)data->m.index_count);
    athena_render_data_freeze(data);

    if (s->streamed_count >= 8)
        return;
    AthenaRenderObject *obj = athena_render_object_create(data);
    s->streamed[s->streamed_count++] = obj;

    AthenaSceneNode *node = athena_scene_node_create();
    athena_scene_node *n = athena_scene_node_native(node);
    n->position[0] = 0.0f; n->position[1] = 6.0f + s->streamed_count * 2.5f; n->position[2] = 0.0f;
    athena_scene_node_attach(node, obj);
    athena_scene_node_add_child(s->root, node);
    athena_render_batch_add(s->batch, obj);
}

static AthenaImage *load(const char *path) { return athena_image_create(path, true); }

static int render_main(int argc, char **argv) {
    (void)argc; (void)argv;

    // Before chdir("render"): the UI font and settings live in the boot folder.
    AthenaOSApp ui;
    athenaos_app_init(&ui, "Render demo", "3d", HINTS, 6);

    athena_screen_set_frame_counter(true);
    athena_screen_set_vsync(true);

    AthenaVideoMode mode;
    athena_screen_get_mode(&mode);
    mode.zbuffering = true;
    mode.psm = GS_PSM_CT32;
    mode.psmz = GS_ZBUF_24;
    athena_screen_set_mode(&mode);
    athena_screen_init_buffers();

    // Decals are drawn into this 128x128 target, mapped over the dragon and the monkey.
    AthenaImage *decal_buffer = athena_image_create_empty(true);
    decal_buffer->tex->Filter = GS_FILTER_LINEAR;
    decal_buffer->tex->PSM = GS_PSM_CT32;
    decal_buffer->tex->Width = decal_buffer->tex->Height = 128;
    decal_buffer->tex->PageAligned = 1; // renderable
    decal_buffer->width = decal_buffer->height = 128;
    decal_buffer->endx = decal_buffer->endy = 128;
    athena_image_lock(decal_buffer);

    athena_screen_switch_context();
    athena_screen_set_buffer(DRAW_BUFFER, decal_buffer, 0);
    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
    athena_screen_switch_context();

    athenaos_loading(&ui, "Loading assets...", "Meshes, textures and materials");

    athena_render_module_init();
    athena_render_module_set_view(60.0f, 5.0f, 4000.0f, 0, 0);

    chdir("render");

    // A triangle built in code: x, y, z, adc / normal / s, t, q / r, g, b, a.
    const VECTOR tri_positions[3] = { { 0.5f, 0.5f, 0.5f, 1.0f }, { 0.5f, 0.8f, 0.5f, 1.0f }, { 0.8f, 0.8f, 0.5f, 1.0f } };
    const VECTOR tri_normals[3] = { { 1, 1, 1, 1 }, { 1, 1, 1, 1 }, { 1, 1, 1, 1 } };
    const VECTOR tri_texcoords[3] = { { 0, 0, 1, 1 }, { 1, 0, 1, 1 }, { 1, 1, 1, 1 } };
    const VECTOR tri_colors[3] = { { 1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 1 } };
    AthenaRenderData *listtest = athena_render_data_from_vertices(tri_positions, tri_normals, tri_texcoords, tri_colors, 3);
    athena_render_data *lt = athena_render_data_native(listtest);
    lt->attributes.face_culling = CULL_FACE_NONE;
    lt->materials[0].shininess = 64.0f;
    lt->materials[0].diffuse[0] = 0.7f; lt->materials[0].diffuse[1] = 0.2f;
    lt->materials[0].diffuse[2] = 0.2f; lt->materials[0].diffuse[3] = 1.0f;

    AthenaImage *skytex = load("env.png");
    athena_image_set_filter(skytex, GS_FILTER_LINEAR);

    AthenaRenderData *gltf_box = athena_render_data_load("box_bump.gltf", NULL);
    athena_render_data_freeze(gltf_box); // static prop, no physics trimesh on it

    AthenaImage *dragontex = load("dragon.png");
    dragontex->tex->Filter = GS_FILTER_LINEAR;
    AthenaRenderData *dragonmesh = athena_render_data_load("dragon.obj", dragontex->tex);

    AthenaImage *blood_tex = load("blood.png");
    blood_tex->width = 40;
    blood_tex->height = 40;

    athena_render_data *dm = athena_render_data_native(dragonmesh);
    dm->materials[0].decal_texture_id = athena_render_data_push_texture(dragonmesh, decal_buffer->tex);
    dm->attributes.has_decal = true;
    athena_render_data_freeze(dragonmesh);

    AthenaImage *monkeytex = load("monkey.png");
    monkeytex->tex->Filter = GS_FILTER_LINEAR;
    AthenaRenderData *monkeymesh = athena_render_data_load("monkey.obj", monkeytex->tex);
    athena_render_data *mm = athena_render_data_native(monkeymesh);
    mm->materials[0].decal_texture_id = athena_render_data_push_texture(monkeymesh, decal_buffer->tex);
    mm->attributes.has_decal = true;
    athena_render_data_freeze(monkeymesh);

    AthenaImage *moontex = load("moon.png");

    // The car reflects env.png; its bright red paint turns purple.
    AthenaRenderData *car = athena_render_data_load("Car.obj", NULL);
    const int envmap_tex_id = athena_render_data_push_texture(car, skytex->tex);
    athena_render_data *cm = athena_render_data_native(car);
    for (int i = 0; i < cm->material_count; i++) {
        ath_mat *mat = &cm->materials[i];
        mat->ref_texture_id = envmap_tex_id;
        if (mat->diffuse[0] > 0.4f && mat->diffuse[1] == 0.0f && mat->diffuse[2] == 0.0f) {
            mat->diffuse[0] = 0.25f;
            mat->diffuse[2] = 0.5f;
        }
    }
    cm->attributes.has_refmap = true;
    // Not frozen, as in the script (which once edited its positions live).

    AthenaRenderData *mill = athena_render_data_load("cubes.obj", NULL);
    athena_render_data_set_texture(mill, 1, moontex->tex);
    athena_render_data_freeze(mill);

    AthenaRenderData *boombox = athena_render_data_load("Boombox.obj", NULL);
    GSSURFACE *boombox_tex = athena_render_data_get_texture(boombox, 0);
    if (boombox_tex)
        boombox_tex->Filter = GS_FILTER_LINEAR;
    athena_render_data_freeze(boombox);

    AthenaRenderData *render_data[MODEL_COUNT] = { dragonmesh, gltf_box, monkeymesh, car, listtest, boombox, mill };

    AthenaRenderObject *render_object[MODEL_COUNT];
    for (int i = 0; i < MODEL_COUNT; i++)
        render_object[i] = athena_render_object_create(render_data[i]);

    Streaming streaming = { 0 };
    streaming.root = athena_scene_node_create();
    AthenaSceneNode *object_nodes[MODEL_COUNT];
    for (int i = 0; i < MODEL_COUNT; i++) {
        object_nodes[i] = athena_scene_node_create();
        athena_scene_node_attach(object_nodes[i], render_object[i]);
        athena_scene_node_add_child(streaming.root, object_nodes[i]);
    }

    streaming.batch = athena_render_batch_create(1); // auto sort
    for (int i = 0; i < MODEL_COUNT; i++)
        athena_render_batch_add(streaming.batch, render_object[i]);

    AthenaAsyncLoader *loader = athena_async_loader_create(2);
    printf("[Render] enqueue BoxTextured.gltf\n");
    athena_async_loader_enqueue(loader, "BoxTextured.gltf", NULL, on_loaded, &streaming);

    athena_camera3d_position(0.0f, 0.0f, 35.0f);

    const int light = athena_lights_new();
    athena_lights_set(light, ATHENA_LIGHT_DIRECTION, 0.0f, 0.5f, 1.0f);
    athena_lights_set(light, ATHENA_LIGHT_AMBIENT, 0.12f, 0.15f, 0.2f);
    athena_lights_set(light, ATHENA_LIGHT_DIFFUSE, 0.5f, 0.5f, 0.5f);
    athena_lights_set(light, ATHENA_LIGHT_SPECULAR, 1.0f, 1.0f, 1.0f);

    AthenaPad *pad = athena_pad_open(0);
    int model = 0;
    float savedlx = 0.0f, savedly = 0.0f, savedrz = 0.0f;

    AthenaMemoryStats mem;
    athena_system_get_memory_stats(&mem);
    char free_mem[48];
    snprintf(free_mem, sizeof(free_mem), "RAM: %uMB / %uMB", (unsigned)(mem.used / 1048576), (unsigned)(GetMemorySize() / 1048576));

    const Color gray = athena_color_rgba(40, 40, 40, 128);
    bool bbox = false;
    float decal_x = 32, decal_y = 0;

    for (;;) {
        athena_screen_clear(gray);
        athena_render_module_begin();
        athena_camera3d_update();
        athena_pad_update(pad);
        const unsigned processed = athena_async_loader_process(loader, 2);
        if (processed)
            printf("[Render] processed %u queue: %u\n", processed, athena_async_loader_queue_size(loader));

        const float lx = ((pad->lx > 25 || pad->lx < -25) ? pad->lx : 0) / 4096.0f;
        const float ly = ((pad->ly > 25 || pad->ly < -25) ? pad->ly : 0) / 4096.0f;
        savedlx -= lx;
        savedly -= ly;

        const float rx = ((pad->rx > 25 || pad->rx < -25) ? pad->rx : 0) / 2048.0f;
        const float ry = ((pad->ry > 25 || pad->ry < -25) ? pad->ry : 0) / 2048.0f;

        if (athena_pad_pressed(pad, PAD_R2))
            savedrz -= 0.05f;
        else if (athena_pad_pressed(pad, PAD_L2))
            savedrz += 0.05f;

        if (rx != 0 || ry != 0) {
            decal_x += rx * 32.0f;
            decal_y += ry * 32.0f;
        }

        athena_camera3d_target(0, 0, savedrz);

        athena_render_data *cur = athena_render_data_native(render_data[model]);
        if (athena_pad_just_pressed(pad, PAD_LEFT) && model > 0)
            model--;
        if (athena_pad_just_pressed(pad, PAD_RIGHT) && model < MODEL_COUNT - 1)
            model++;
        cur = athena_render_data_native(render_data[model]);
        if (athena_pad_just_pressed(pad, PAD_UP) && cur->pipeline > 0)
            cur->pipeline--;
        if (athena_pad_just_pressed(pad, PAD_DOWN) && cur->pipeline < PIPELINE_COUNT - 1)
            cur->pipeline++;

        if (athenaos_home_requested(pad)) {
            athena_async_loader_clear(loader);
            athena_async_loader_destroy(loader);
            break;
        }

        if (athena_pad_just_pressed(pad, PAD_R1)) bbox = !bbox;
        if (athena_pad_just_pressed(pad, PAD_SQUARE)) cur->attributes.shade_model ^= 1;
        if (athena_pad_just_pressed(pad, PAD_CIRCLE)) cur->attributes.texture_mapping ^= 1;
        if (athena_pad_just_pressed(pad, PAD_CROSS)) cur->attributes.accurate_clipping ^= 1;

        if (lx != 0 || ly != 0) {
            athena_scene_node *n = athena_scene_node_native(object_nodes[model]);
            n->rotation[0] = savedly; n->rotation[1] = savedlx; n->rotation[2] = 0.0f;
        }

        // The decal: blood drawn into the offscreen target, then the target shown.
        athena_screen_switch_context();
        athena_draw_rect(0, 0, 128, 128, athena_color_rgba(0, 0, 0, 0));
        athena_image_draw(blood_tex, decal_x, decal_y);
        athena_screen_switch_context();
        athena_image_draw(decal_buffer, 0, 100);

        athena_screen_set_param(DEPTH_TEST_ENABLE, true);
        athena_screen_set_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);

        athena_scene_node_update(streaming.root);
        athena_render_batch_render(streaming.batch);

        athena_screen_set_param(DEPTH_TEST_ENABLE, false);

        const render_stats_t *stats = athena_render_module_get_stats();
        snprintf(ui.status, sizeof(ui.status), "%.1f FPS", athena_screen_get_fps(360));
        char l0[96], l1[96], l2[112], l3[96];
        snprintf(l0, sizeof(l0), "Model %d/%d  -  %u vertices", model + 1, MODEL_COUNT, (unsigned)cur->index_count);
        snprintf(l1, sizeof(l1), "Pipeline: %s  |  Draws: %u  |  Tris: %u",
                 PIPELINES[cur->pipeline], (unsigned)stats->draw_calls, (unsigned)stats->triangles);
        snprintf(l2, sizeof(l2), "%s  |  VRAM static %d KB  dynamic %d KB", free_mem,
                 athena_screen_get_free_vram(VRAM_USED_STATIC) / 1024, athena_screen_get_free_vram(VRAM_USED_DYNAMIC) / 1024);
        snprintf(l3, sizeof(l3), "Batch: %u objects  |  Loader queue: %u  |  Streamed: %d",
                 athena_render_batch_size(streaming.batch), athena_async_loader_queue_size(loader), streaming.streamed_count);
        const char *lines[] = { l0, l1, l2, l3 };
        athenaos_hud(&ui, lines, 4, 0);
        athenaos_chrome(&ui);

        athena_screen_flip();
    }
    return 0;
}

ATHENAOS_APP(render, render_main)
