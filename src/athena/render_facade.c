#include <stdlib.h>
#include <string.h>

#include <kernel.h>

#include <athena/render_facade.h>

void athena_render_module_init(void)
{
    render_init();
}

void athena_render_module_begin(void)
{
    render_begin();
}

void athena_render_module_set_view(float fov, float near, float far, float width, float height)
{
    render_set_view(fov, near, far, width, height);
}

const render_stats_t *athena_render_module_get_stats(void)
{
    return render_get_stats();
}

void athena_render_module_reset_stats(void)
{
    render_reset_stats();
}

AthenaRenderData *athena_render_data_create(void)
{
    AthenaRenderData *rd = calloc(1, sizeof(*rd));
    if (!rd)
        return NULL;

    for (int i = 0; i < 4; i++)
        rd->owns_vertices[i] = true;

    return rd;
}

AthenaRenderData *athena_render_data_load(const char *path, GSSURFACE *texture)
{
    AthenaRenderData *rd;

    if (!path)
        return NULL;

    rd = athena_render_data_create();
    if (!rd)
        return NULL;

    loadModel(&rd->m, path, texture);
    athena_render_data_apply_defaults(rd);
    FlushCache(WRITEBACK_DCACHE);
    return rd;
}

void athena_render_data_destroy(AthenaRenderData *rd)
{
    VECTOR **attribute_ptrs[4];

    if (!rd)
        return;

    if (rd->m.indices)
        free(rd->m.indices);

    attribute_ptrs[0] = &rd->m.positions;
    attribute_ptrs[1] = &rd->m.normals;
    attribute_ptrs[2] = &rd->m.texcoords;
    attribute_ptrs[3] = &rd->m.colours;

    for (int i = 0; i < 4; i++) {
        if (*attribute_ptrs[i] && rd->owns_vertices[i])
            free(*attribute_ptrs[i]);
    }

    // Compact VIF-unpack cache (render_cook_compact_vertices) -- always
    // owned internally, never shared/borrowed like the float attributes.
    free(rd->m.compact_positions);
    free(rd->m.compact_normals);
    free(rd->m.compact_colors);
    free(rd->m.compact_uvs);
    free(rd->m.compact_group_base);

    // Baked DMA_CALL chains (render_build_chain): 3 pass_state slots plus the
    // reflection pass. Same always-owned-internally rule as compact_* above.
    athena_chain_cache *slots[10] = {
        &rd->m.chain[0][0], &rd->m.chain[0][1], &rd->m.chain[0][2],
        &rd->m.chain[1][0], &rd->m.chain[1][1], &rd->m.chain[1][2],
        &rd->m.chain[2][0], &rd->m.chain[2][1], &rd->m.chain[2][2],
        &rd->m.ref_chain
    };
    for (int i = 0; i < 10; i++) {
        free(slots[i]->buffer);
        free(slots[i]->chunk_offset);
        free(slots[i]->tex_giftag);
    }

    if (rd->m.materials)
        free(rd->m.materials);

    if (rd->m.material_indices)
        free(rd->m.material_indices);

    if (rd->m.skin_data)
        free(rd->m.skin_data);

    if (rd->m.skeleton) {
        if (rd->m.skeleton->bones)
            free(rd->m.skeleton->bones);
        free(rd->m.skeleton);
    }

    if (rd->m.textures)
        free(rd->m.textures);
}

void athena_render_data_free(AthenaRenderData *rd)
{
    if (!rd)
        return;

    athena_render_data_destroy(rd);
    free(rd);
}

AthenaRenderData *athena_render_data_clone(const AthenaRenderData *src)
{
    AthenaRenderData *rd;

    if (!src)
        return NULL;

    rd = calloc(1, sizeof(*rd));
    if (!rd)
        return NULL;

    rd->m.index_count = src->m.index_count;
    rd->m.indices = src->m.indices;
    rd->m.positions = src->m.positions;
    rd->m.normals = src->m.normals;
    rd->m.texcoords = src->m.texcoords;
    rd->m.colours = src->m.colours;
    rd->m.skin_data = src->m.skin_data;
    rd->m.skeleton = src->m.skeleton;
    rd->m.tristrip = src->m.tristrip;
    rd->m.frozen = src->m.frozen;

    for (int i = 0; i < 4; i++)
        rd->owns_vertices[i] = false;

    if (src->m.material_count > 0) {
        rd->m.material_count = src->m.material_count;
        rd->m.materials = malloc(sizeof(ath_mat) * rd->m.material_count);
        memcpy(rd->m.materials, src->m.materials, sizeof(ath_mat) * rd->m.material_count);
    }

    if (src->m.material_index_count > 0) {
        rd->m.material_index_count = src->m.material_index_count;
        rd->m.material_indices = malloc(sizeof(material_index) * rd->m.material_index_count);
        memcpy(rd->m.material_indices, src->m.material_indices,
               sizeof(material_index) * rd->m.material_index_count);
    }

    rd->m.texture_count = src->m.texture_count;
    if (src->m.texture_count > 0) {
        rd->m.textures = malloc(sizeof(GSSURFACE *) * rd->m.texture_count);
        memcpy(rd->m.textures, src->m.textures, sizeof(GSSURFACE *) * rd->m.texture_count);
    }

    memcpy(rd->m.bounding_box, src->m.bounding_box, sizeof(rd->m.bounding_box));
    rd->m.pipeline = src->m.pipeline;
    rd->m.attributes = src->m.attributes;

    return rd;
}

void athena_render_data_apply_defaults(AthenaRenderData *rd)
{
    if (!rd)
        return;

    rd->m.attributes.accurate_clipping = 1;
    rd->m.attributes.face_culling = CULL_FACE_BACK;
    rd->m.attributes.texture_mapping = 1;
    rd->m.attributes.shade_model = 1;
    rd->m.pipeline = PL_DEFAULT;
}

athena_render_data *athena_render_data_native(AthenaRenderData *rd)
{
    return rd ? &rd->m : NULL;
}

void athena_render_data_set_pipeline(AthenaRenderData *rd, eRenderPipelines pipeline)
{
    if (rd)
        rd->m.pipeline = pipeline;
}

eRenderPipelines athena_render_data_get_pipeline(const AthenaRenderData *rd)
{
    return rd ? rd->m.pipeline : PL_NO_LIGHTS;
}

// Call after mutating positions/normals/texcoords/colors in place (their
// exposed ArrayBuffers) so the next draw picks up the change -- the compact
// VU wire buffers are otherwise cached across draws for performance.
void athena_render_data_invalidate_compact_cache(AthenaRenderData *rd)
{
    if (rd)
        render_invalidate_compact_cache(&rd->m);
}

// Call after mutating materials/material_indices/texture_mapping, or
// anything else that changes the mesh's chunk structure or the addresses
// baked into the pre-cooked DMA_CALL chain -- see render_invalidate_chain_
// cache()'s doc comment in render.h.
void athena_render_data_invalidate_chain_cache(AthenaRenderData *rd)
{
    if (rd)
        render_invalidate_chain_cache(&rd->m);
}

// Frees positions/normals/texcoords/colours (~64 bytes/vertex) once a mesh
// is done being edited, keeping only the ~24 bytes/vertex compact VU wire
// cache. Reassigning "vertices" afterward un-freezes automatically. Not for
// meshes still needed by ODE trimesh collision or similar float-source
// consumers.
void athena_render_data_freeze(AthenaRenderData *rd)
{
    if (rd)
        render_freeze_compact_vertices(&rd->m);
}

int athena_render_data_set_texture(AthenaRenderData *rd, uint32_t index, GSSURFACE *tex)
{
    if (!rd)
        return -1;

    if (rd->m.texture_count < (index + 1)) {
        GSSURFACE **textures = realloc(rd->m.textures, sizeof(GSSURFACE *) * (index + 1));
        if (!textures)
            return -1;
        rd->m.textures = textures;
        for (uint32_t i = rd->m.texture_count; i <= index; i++)
            rd->m.textures[i] = NULL;
        rd->m.texture_count = index + 1;
    }

    rd->m.textures[index] = tex;
    if (rd->m.texture_count < (index + 1))
        rd->m.texture_count = index + 1;

    return 0;
}

GSSURFACE *athena_render_data_get_texture(const AthenaRenderData *rd, uint32_t index)
{
    if (!rd || index >= rd->m.texture_count)
        return NULL;

    return rd->m.textures[index];
}

uint32_t athena_render_data_push_texture(AthenaRenderData *rd, GSSURFACE *tex)
{
    uint32_t index;

    if (!rd)
        return 0;

    index = rd->m.texture_count++;
    rd->m.textures = realloc(rd->m.textures, sizeof(GSSURFACE *) * rd->m.texture_count);
    if (rd->m.textures)
        rd->m.textures[index] = tex;

    return index;
}

AthenaRenderObject *athena_render_object_create(AthenaRenderData *data)
{
    AthenaRenderObject *ro;

    if (!data)
        return NULL;

    ro = calloc(1, sizeof(*ro));
    if (!ro)
        return NULL;

    new_render_object(&ro->obj, &data->m);
    return ro;
}

void athena_render_object_destroy(AthenaRenderObject *ro)
{
    if (!ro)
        return;

    if (ro->obj.bones) {
        free(ro->obj.bones);
        free(ro->obj.bone_matrices);
    }

    free(ro->obj.skinned_bounds);

    free(ro);
}

void athena_render_object_draw(AthenaRenderObject *ro)
{
    if (ro)
        render_object(&ro->obj);
}

void athena_render_object_draw_bbox(AthenaRenderObject *ro, Color color)
{
    if (ro)
        draw_bbox(&ro->obj, color);
}

athena_object_data *athena_render_object_native(AthenaRenderObject *ro)
{
    return ro ? &ro->obj : NULL;
}

void athena_render_object_play_anim(AthenaRenderObject *ro, athena_animation *anim, bool loop)
{
    if (!ro)
        return;

    ro->obj.anim_controller.current = anim;
    ro->obj.anim_controller.is_playing = false;
    ro->obj.anim_controller.loop = loop;
}

bool athena_render_object_is_playing_anim(const AthenaRenderObject *ro, const athena_animation *anim)
{
    if (!ro || !ro->obj.anim_controller.current)
        return false;

    if (anim)
        return ro->obj.anim_controller.current == anim;

    return true;
}

AthenaRenderBatch *athena_render_batch_create(int auto_sort)
{
    AthenaRenderBatch *batch = calloc(1, sizeof(*batch));
    if (!batch)
        return NULL;

    batch->batch = athena_batch_create();
    if (!batch->batch) {
        free(batch);
        return NULL;
    }

    athena_batch_set_sort(batch->batch, NULL, auto_sort);
    return batch;
}

void athena_render_batch_destroy(AthenaRenderBatch *batch)
{
    if (!batch)
        return;

    if (batch->batch)
        athena_batch_destroy(batch->batch);

    free(batch);
}

int athena_render_batch_add(AthenaRenderBatch *batch, AthenaRenderObject *obj)
{
    if (!batch || !batch->batch || !obj)
        return -1;

    return athena_batch_add(batch->batch, &obj->obj);
}

void athena_render_batch_clear(AthenaRenderBatch *batch)
{
    if (batch && batch->batch)
        athena_batch_clear(batch->batch);
}

unsigned int athena_render_batch_render(AthenaRenderBatch *batch)
{
    return (batch && batch->batch) ? athena_batch_render(batch->batch) : 0;
}

unsigned int athena_render_batch_size(const AthenaRenderBatch *batch)
{
    return (batch && batch->batch) ? batch->batch->count : 0;
}

AthenaSceneNode *athena_scene_node_create(void)
{
    AthenaSceneNode *node = calloc(1, sizeof(*node));
    if (!node)
        return NULL;

    node->node = render_scene_node_create();
    if (!node->node) {
        free(node);
        return NULL;
    }

    return node;
}

void athena_scene_node_destroy(AthenaSceneNode *node)
{
    if (!node)
        return;

    if (node->node)
        render_scene_node_destroy(node->node);

    free(node);
}

void athena_scene_node_add_child(AthenaSceneNode *parent, AthenaSceneNode *child)
{
    if (parent && parent->node && child && child->node)
        render_scene_node_add_child(parent->node, child->node);
}

void athena_scene_node_remove_child(AthenaSceneNode *parent, AthenaSceneNode *child)
{
    if (parent && parent->node && child && child->node)
        render_scene_node_remove_child(parent->node, child->node);
}

void athena_scene_node_attach(AthenaSceneNode *node, AthenaRenderObject *obj)
{
    if (node && node->node && obj)
        render_scene_node_attach(node->node, &obj->obj);
}

void athena_scene_node_detach(AthenaSceneNode *node, AthenaRenderObject *obj)
{
    if (node && node->node)
        render_scene_node_detach(node->node, obj ? &obj->obj : NULL);
}

void athena_scene_node_update(AthenaSceneNode *root)
{
    if (root && root->node)
        render_scene_update(root->node);
}

athena_scene_node *athena_scene_node_native(AthenaSceneNode *node)
{
    return node ? node->node : NULL;
}

AthenaAsyncLoader *athena_async_loader_create(unsigned int jobs_per_step)
{
    AthenaAsyncLoader *loader = calloc(1, sizeof(*loader));
    if (!loader)
        return NULL;

    if (jobs_per_step == 0)
        jobs_per_step = 1;

    loader->loader = render_async_loader_create(jobs_per_step);
    if (!loader->loader) {
        free(loader);
        return NULL;
    }

    return loader;
}

void athena_async_loader_destroy(AthenaAsyncLoader *loader)
{
    if (!loader)
        return;

    if (loader->loader)
        render_async_loader_destroy(loader->loader);

    free(loader);
}

void athena_async_loader_destroy_with(AthenaAsyncLoader *loader, athena_loader_user_cleanup cleanup)
{
    if (!loader)
        return;

    if (loader->loader)
        athena_async_destroy_with(loader->loader, cleanup);

    free(loader);
}

int athena_async_loader_enqueue(AthenaAsyncLoader *loader, const char *path, GSSURFACE *tex,
                                athena_loader_cb cb, void *user)
{
    if (!loader || !loader->loader)
        return -1;

    return athena_async_enqueue(loader->loader, path, tex, cb, user);
}

unsigned int athena_async_loader_process(AthenaAsyncLoader *loader, unsigned int budget)
{
    if (!loader || !loader->loader)
        return 0;

    return athena_async_process(loader->loader, budget);
}

void athena_async_loader_clear(AthenaAsyncLoader *loader)
{
    if (loader && loader->loader)
        athena_async_clear(loader->loader);
}

void athena_async_loader_clear_with(AthenaAsyncLoader *loader, athena_loader_user_cleanup cleanup)
{
    if (loader && loader->loader)
        athena_async_clear_with(loader->loader, cleanup);
}

unsigned int athena_async_loader_queue_size(AthenaAsyncLoader *loader)
{
    return (loader && loader->loader) ? athena_async_queue_size(loader->loader) : 0;
}

unsigned int athena_async_loader_jobs_per_step(AthenaAsyncLoader *loader)
{
    return (loader && loader->loader) ? athena_async_jobs_per_step(loader->loader) : 0;
}

void athena_async_loader_set_jobs_per_step(AthenaAsyncLoader *loader, unsigned int value)
{
    if (loader && loader->loader)
        athena_async_set_jobs_per_step(loader->loader, value);
}
