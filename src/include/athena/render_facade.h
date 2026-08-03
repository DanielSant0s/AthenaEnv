#ifndef ATHENA_RENDER_FACADE_H
#define ATHENA_RENDER_FACADE_H

#include <stdbool.h>
#include <stdint.h>

#include <graphics.h>
#include <render.h>
#include <render_batch.h>
#include <render_scene.h>
#include <render_async_loader.h>

typedef struct AthenaRenderData {
    athena_render_data m;
    bool owns_vertices[4];
} AthenaRenderData;

typedef struct AthenaRenderObject {
    athena_object_data obj;
} AthenaRenderObject;

typedef struct AthenaRenderBatch {
    athena_batch *batch;
} AthenaRenderBatch;

typedef struct AthenaSceneNode {
    athena_scene_node *node;
} AthenaSceneNode;

typedef struct AthenaAsyncLoader {
    athena_async_loader *loader;
} AthenaAsyncLoader;

void athena_render_module_init(void);
void athena_render_module_begin(void);
void athena_render_module_set_view(float fov, float near, float far, float width, float height);
const render_stats_t *athena_render_module_get_stats(void);
void athena_render_module_reset_stats(void);

AthenaRenderData *athena_render_data_create(void);
AthenaRenderData *athena_render_data_load(const char *path, GSSURFACE *texture);
void athena_render_data_destroy(AthenaRenderData *rd);
void athena_render_data_free(AthenaRenderData *rd);
AthenaRenderData *athena_render_data_clone(const AthenaRenderData *src);
void athena_render_data_apply_defaults(AthenaRenderData *rd);
athena_render_data *athena_render_data_native(AthenaRenderData *rd);

void athena_render_data_set_pipeline(AthenaRenderData *rd, eRenderPipelines pipeline);
eRenderPipelines athena_render_data_get_pipeline(const AthenaRenderData *rd);
void athena_render_data_invalidate_compact_cache(AthenaRenderData *rd);
void athena_render_data_invalidate_chain_cache(AthenaRenderData *rd);
void athena_render_data_freeze(AthenaRenderData *rd);
int athena_render_data_set_texture(AthenaRenderData *rd, uint32_t index, GSSURFACE *tex);
GSSURFACE *athena_render_data_get_texture(const AthenaRenderData *rd, uint32_t index);
uint32_t athena_render_data_push_texture(AthenaRenderData *rd, GSSURFACE *tex);

AthenaRenderObject *athena_render_object_create(AthenaRenderData *data);
void athena_render_object_destroy(AthenaRenderObject *ro);
void athena_render_object_draw(AthenaRenderObject *ro);
void athena_render_object_draw_bbox(AthenaRenderObject *ro, Color color);
athena_object_data *athena_render_object_native(AthenaRenderObject *ro);
void athena_render_object_play_anim(AthenaRenderObject *ro, athena_animation *anim, bool loop);
bool athena_render_object_is_playing_anim(const AthenaRenderObject *ro, const athena_animation *anim);

AthenaRenderBatch *athena_render_batch_create(int auto_sort);
void athena_render_batch_destroy(AthenaRenderBatch *batch);
int athena_render_batch_add(AthenaRenderBatch *batch, AthenaRenderObject *obj);
void athena_render_batch_clear(AthenaRenderBatch *batch);
unsigned int athena_render_batch_render(AthenaRenderBatch *batch);
unsigned int athena_render_batch_size(const AthenaRenderBatch *batch);

AthenaSceneNode *athena_scene_node_create(void);
void athena_scene_node_destroy(AthenaSceneNode *node);
void athena_scene_node_add_child(AthenaSceneNode *parent, AthenaSceneNode *child);
void athena_scene_node_remove_child(AthenaSceneNode *parent, AthenaSceneNode *child);
void athena_scene_node_attach(AthenaSceneNode *node, AthenaRenderObject *obj);
void athena_scene_node_detach(AthenaSceneNode *node, AthenaRenderObject *obj);
void athena_scene_node_update(AthenaSceneNode *root);
athena_scene_node *athena_scene_node_native(AthenaSceneNode *node);

AthenaAsyncLoader *athena_async_loader_create(unsigned int jobs_per_step);
void athena_async_loader_destroy(AthenaAsyncLoader *loader);
void athena_async_loader_destroy_with(AthenaAsyncLoader *loader, athena_loader_user_cleanup cleanup);
int athena_async_loader_enqueue(AthenaAsyncLoader *loader, const char *path, GSSURFACE *tex,
                                athena_loader_cb cb, void *user);
unsigned int athena_async_loader_process(AthenaAsyncLoader *loader, unsigned int budget);
void athena_async_loader_clear(AthenaAsyncLoader *loader);
void athena_async_loader_clear_with(AthenaAsyncLoader *loader, athena_loader_user_cleanup cleanup);
unsigned int athena_async_loader_queue_size(AthenaAsyncLoader *loader);
unsigned int athena_async_loader_jobs_per_step(AthenaAsyncLoader *loader);
void athena_async_loader_set_jobs_per_step(AthenaAsyncLoader *loader, unsigned int value);

#endif /* ATHENA_RENDER_FACADE_H */
