#include <stdlib.h>
#include <string.h>

#include <athena/anim3d.h>

extern void load_gltf_animations(char *path, athena_animation_collection *collection);

AthenaAnim3dCollection *athena_anim3d_load(const char *path)
{
    AthenaAnim3dCollection *col;

    if (!path)
        return NULL;

    col = calloc(1, sizeof(*col));
    if (!col)
        return NULL;

    load_gltf_animations((char *)path, &col->collection);
    return col;
}

void athena_anim3d_free(AthenaAnim3dCollection *col)
{
    if (!col)
        return;

    if (col->collection.animations) {
        for (uint32_t i = 0; i < col->collection.count; i++) {
            athena_animation *anim = &col->collection.animations[i];
            if (anim->bone_animations) {
                for (uint32_t b = 0; b < anim->bone_animation_count; b++) {
                    free(anim->bone_animations[b].position_keys);
                    free(anim->bone_animations[b].rotation_keys);
                    free(anim->bone_animations[b].scale_keys);
                }
                free(anim->bone_animations);
            }
        }
        free(col->collection.animations);
    }

    free(col);
}

uint32_t athena_anim3d_count(const AthenaAnim3dCollection *col)
{
    return col ? col->collection.count : 0;
}

athena_animation *athena_anim3d_get(AthenaAnim3dCollection *col, uint32_t index)
{
    if (!col || index >= col->collection.count)
        return NULL;

    return &col->collection.animations[index];
}

athena_animation *athena_anim3d_find(AthenaAnim3dCollection *col, const char *name)
{
    uint32_t i;

    if (!col || !name)
        return NULL;

    for (i = 0; i < col->collection.count; i++) {
        if (!strcmp(col->collection.animations[i].name, name))
            return &col->collection.animations[i];
    }

    return NULL;
}
