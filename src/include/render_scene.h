#ifndef ATHENA_RENDER_SCENE_H
#define ATHENA_RENDER_SCENE_H

#include <render.h>

typedef struct athena_scene_node {
    MATRIX world;
    VECTOR position;
    VECTOR rotation;
    VECTOR scale;

    struct athena_scene_node **children;
    unsigned int child_count;
    unsigned int child_capacity;

    athena_object_data **attachments;
    unsigned int attach_count;
    unsigned int attach_capacity;

    struct athena_scene_node *parent;
} athena_scene_node;

athena_scene_node *athena_scene_node_create(void);
void athena_scene_node_destroy(athena_scene_node *n);

void athena_scene_node_add_child(athena_scene_node *parent, athena_scene_node *child);
void athena_scene_node_remove_child(athena_scene_node *parent, athena_scene_node *child);

void athena_scene_node_attach(athena_scene_node *node, athena_object_data *obj);
void athena_scene_node_detach(athena_scene_node *node, athena_object_data *obj); // obj==NULL clears all

void athena_scene_update(athena_scene_node *root);

#endif

