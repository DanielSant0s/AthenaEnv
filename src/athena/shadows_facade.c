#include <stdlib.h>
#include <string.h>

#include <athena/shadows_facade.h>

AthenaShadowProjector *athena_shadow_projector_create(GSSURFACE *texture)
{
    AthenaShadowProjector *p = calloc(1, sizeof(*p));
    if (!p)
        return NULL;

    shadow_projector_init(&p->proj, texture);
    return p;
}

void athena_shadow_projector_free(AthenaShadowProjector *p)
{
    if (!p)
        return;

    shadow_projector_free(&p->proj);
    free(p);
}

void athena_shadow_projector_set_transform(AthenaShadowProjector *p, const float matrix[16])
{
    MATRIX m;

    if (!p || !matrix)
        return;

    memcpy(m, matrix, sizeof(MATRIX));
    shadow_projector_set_transform(&p->proj, m);
}

void athena_shadow_projector_set_size(AthenaShadowProjector *p, float width, float height)
{
    if (p)
        shadow_projector_set_size(&p->proj, width, height);
}

void athena_shadow_projector_set_grid(AthenaShadowProjector *p, int grid_x, int grid_z)
{
    if (p)
        shadow_projector_set_grid(&p->proj, grid_x, grid_z);
}

void athena_shadow_projector_set_light_dir(AthenaShadowProjector *p, float x, float y, float z)
{
    if (p)
        shadow_projector_set_light_dir(&p->proj, x, y, z);
}

void athena_shadow_projector_set_bias(AthenaShadowProjector *p, float bias)
{
    if (p)
        shadow_projector_set_bias(&p->proj, bias);
}

void athena_shadow_projector_set_light_offset(AthenaShadowProjector *p, float dist)
{
    if (p)
        shadow_projector_set_light_offset(&p->proj, dist);
}

void athena_shadow_projector_set_slope_limit(AthenaShadowProjector *p, float max_slope_cos)
{
    if (p)
        shadow_projector_set_slope_limit(&p->proj, max_slope_cos);
}

void athena_shadow_projector_set_color(AthenaShadowProjector *p, float r, float g, float b, float a)
{
    if (p)
        shadow_projector_set_color(&p->proj, r, g, b, a);
}

void athena_shadow_projector_set_blend(AthenaShadowProjector *p, int mode)
{
    if (p)
        shadow_projector_set_blend(&p->proj, mode);
}

void athena_shadow_projector_set_uv_rect(AthenaShadowProjector *p, float u0, float v0, float u1, float v1)
{
    if (p)
        shadow_projector_set_uv_rect(&p->proj, u0, v0, u1, v1);
}

void athena_shadow_projector_enable_raycast(AthenaShadowProjector *p, AthenaPhysicsSpace *space, float ray_length, int enable)
{
    if (p)
        shadow_projector_enable_raycast(&p->proj, space, ray_length, enable);
}

void athena_shadow_projector_render(AthenaShadowProjector *p)
{
    if (p)
        shadow_projector_render(&p->proj);
}

void athena_shadow_projector_sync_transform(AthenaShadowProjector *p)
{
    if (!p)
        return;

    shadow_create_transform_matrix(p->proj.transform, p->proj.obj.position,
                                   p->proj.obj.rotation, p->proj.obj.scale);
    p->proj.prepared = NULL;
}

int athena_shadow_projector_screen_bounds(AthenaShadowProjector *p, int out[4])
{
    return p ? shadow_projector_screen_bounds(&p->proj, out) : 0;
}

athena_object_data *athena_shadow_projector_object(AthenaShadowProjector *p)
{
    return p ? &p->proj.obj : NULL;
}
