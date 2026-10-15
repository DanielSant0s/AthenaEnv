#ifndef ATHENA_SHADOWS_FACADE_H
#define ATHENA_SHADOWS_FACADE_H

#include <graphics.h>
#include <shadows.h>

typedef struct AthenaShadowProjector {
    ath_shadow_projector proj;
} AthenaShadowProjector;

AthenaShadowProjector *athena_shadow_projector_create(GSSURFACE *texture);
void athena_shadow_projector_free(AthenaShadowProjector *p);

void athena_shadow_projector_set_transform(AthenaShadowProjector *p, const float matrix[16]);
void athena_shadow_projector_set_size(AthenaShadowProjector *p, float width, float height);
void athena_shadow_projector_set_grid(AthenaShadowProjector *p, int grid_x, int grid_z);
void athena_shadow_projector_set_light_dir(AthenaShadowProjector *p, float x, float y, float z);
void athena_shadow_projector_set_bias(AthenaShadowProjector *p, float bias);
void athena_shadow_projector_set_light_offset(AthenaShadowProjector *p, float dist);
void athena_shadow_projector_set_slope_limit(AthenaShadowProjector *p, float max_slope_cos);
void athena_shadow_projector_set_color(AthenaShadowProjector *p, float r, float g, float b, float a);
void athena_shadow_projector_set_blend(AthenaShadowProjector *p, int mode);
void athena_shadow_projector_set_uv_rect(AthenaShadowProjector *p, float u0, float v0, float u1, float v1);
void athena_shadow_projector_enable_raycast(AthenaShadowProjector *p, AthenaPhysicsSpace *space, float ray_length, int enable);

void athena_shadow_projector_render(AthenaShadowProjector *p);
void athena_shadow_projector_sync_transform(AthenaShadowProjector *p);

athena_object_data *athena_shadow_projector_object(AthenaShadowProjector *p);

#endif /* ATHENA_SHADOWS_FACADE_H */
