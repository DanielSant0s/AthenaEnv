#include <athena/particles.h>
#include <tile_render.h>
#include <owl_packet.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Engine camera/projection matrices, refreshed by Render.setView/Camera.update. */
extern MATRIX world_screen, view_screen;
extern FIVECTOR screen_scale;
struct AthenaParticleRenderer {
    AthenaParticles *system;
    AthenaImage *image;
    athena_sprite_data *buffers[2];
    uint32_t generation[2], next;
    bool pending[2], depth_test;
    uint64_t blend;
};
AthenaParticleRenderer *athena_particles_renderer_create(AthenaParticles *system, AthenaImage *image,
    uint64_t blend, bool depth_test) {
    if (!system || !image || !image->tex) return NULL;
    AthenaParticleRenderer *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->system = system; r->image = image; r->blend = blend; r->depth_test = depth_test;
    size_t bytes = particles_stats(system).capacity * sizeof(athena_sprite_data);
    for (int i = 0; i < 2; i++) {
        r->buffers[i] = memalign(64, bytes);
        if (!r->buffers[i]) { athena_particles_renderer_destroy(r); return NULL; }
    }
    tile_render_init();
    return r;
}
void athena_particles_renderer_destroy(AthenaParticleRenderer *r) {
    if (!r) return;
    for (int i = 0; i < 2; i++) {
        if (r->pending[i]) owl_wait_generation(r->generation[i]);
        free(r->buffers[i]);
    }
    free(r);
}
uint32_t athena_particles_draw(AthenaParticleRenderer *r) {
    if (!r || !r->image || !r->image->tex) return 0;
    uint32_t slot = r->next;
    if (r->pending[slot]) { owl_wait_generation(r->generation[slot]); r->pending[slot] = false; }
    AthenaParticleView v = {0};
    v.width = gsGlobal->Width; v.height = gsGlobal->Height;
    v.z_scale = (float)get_max_z(gsGlobal) * 0.5f;
    if (particles_world(r->system)) {
        memcpy(v.matrix, world_screen, sizeof(v.matrix));
        v.width = screen_scale.x * 2; v.height = screen_scale.y * 2;
        v.z_scale = screen_scale.z; v.focal = fabsf(view_screen[5]) * v.height * 0.5f;
    }
    uint32_t count = particles_prepare(r->system, &v, r->buffers[slot]);
    if (!count) return 0;
    GSSURFACE *texture = r->image->tex;
    athena_sprite_material material = { .texture_index = 0, .blend_mode = r->blend, .end = count - 1 };
    athena_tilemap_descriptor descriptor = { .textures = &texture, .texture_count = 1,
        .materials = &material, .material_count = 1 };
    int ctxt = gsGlobal->PrimContext;
    uint64_t test = get_register(GS_CACHE_TEST + ctxt), zbuf = get_register(GS_CACHE_ZBUF + ctxt);
    bool alpha = gsGlobal->PrimAlphaEnable, fog = gsGlobal->PrimFogEnable;
    float camera_x, camera_y; tile_render_get_camera(&camera_x, &camera_y);
    uint32_t vu_base, vu_offset; vu1_get_double_buffer_settings(&vu_base, &vu_offset);
    gsGlobal->PrimAlphaEnable = true; gsGlobal->PrimFogEnable = false;
    set_screen_param(ALPHA_TEST_ENABLE, false);
    set_screen_param(DEPTH_TEST_ENABLE, r->depth_test);
    if (r->depth_test) set_screen_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);
    set_register(GS_CACHE_ZBUF + ctxt, zbuf | (1ULL << 32));
    tile_render_begin(); tile_render_set_camera(0, 0);
    // TileMap uses 2047.35; compensate against the GS XYOFFSET of 2048.
    tile_render_render_native(&descriptor, r->buffers[slot], 0.65f, 0.65f, 0);
    vu1_set_double_buffer_settings(vu_base, vu_offset);
    // 2D input/output windows overwrite the static 3D light block. Bone
    // palettes are uploaded per object; the view is invalidated by TileMap.
    if (vu_base == 141 && vu_offset == 342) render_restore_vu_lighting();
    r->generation[slot] = owl_flush_generation(); r->pending[slot] = true;
    r->next ^= 1;
    tile_render_set_camera(camera_x, camera_y);
    gsGlobal->PrimAlphaEnable = alpha; gsGlobal->PrimFogEnable = fog;
    set_register(GS_CACHE_TEST + ctxt, test); set_register(GS_CACHE_ZBUF + ctxt, zbuf);
    return count;
}
