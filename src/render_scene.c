#include <stdlib.h>
#include <string.h>

#include <matrix.h>
#include <render.h>
#include <render_scene.h>

static void v_set(VECTOR v, float x, float y, float z, float w) {
    v[0] = x; v[1] = y; v[2] = z; v[3] = w;
}

athena_scene_node *athena_scene_node_create(void) {
    athena_scene_node *n = (athena_scene_node*)malloc(sizeof(*n));
    if (!n) return NULL;
    matrix_functions->identity(n->world);
    v_set(n->position, 0.0f, 0.0f, 0.0f, 1.0f);
    v_set(n->rotation, 0.0f, 0.0f, 0.0f, 1.0f);
    v_set(n->scale,    1.0f, 1.0f, 1.0f, 1.0f);
    n->children = NULL; n->child_count = 0; n->child_capacity = 0;
    n->attachments = NULL; n->attach_count = 0; n->attach_capacity = 0;
    n->parent = NULL;
    return n;
}

static void list_remove_ptr(void **arr, unsigned int *count, void *ptr) {
    for (unsigned int i = 0; i < *count; i++) {
        if (arr[i] == ptr) {
            memmove(&arr[i], &arr[i+1], (*count - i - 1) * sizeof(void*));
            (*count)--;
            return;
        }
    }
}

void athena_scene_node_destroy(athena_scene_node *n) {
    if (!n) return;
    for (unsigned int i = 0; i < n->child_count; i++) {
        athena_scene_node_destroy(n->children[i]);
    }
    free(n->children);
    free(n->attachments);
    free(n);
}

static int ensure_cap(void **buf, unsigned int *cap, unsigned int need) {
    if (need <= *cap) return 0;
    unsigned int c = *cap ? *cap * 2 : 4;
    while (c < need) c *= 2;
    void *p = realloc(*buf, c * sizeof(void*));
    if (!p) return -1;
    *buf = p; *cap = c; return 0;
}

void athena_scene_node_add_child(athena_scene_node *parent, athena_scene_node *child) {
    if (!parent || !child || parent == child) return;
    if (child->parent) {
        athena_scene_node_remove_child(child->parent, child);
    }
    if (ensure_cap((void**)&parent->children, &parent->child_capacity, parent->child_count + 1) != 0)
        return;
    parent->children[parent->child_count++] = child;
    child->parent = parent;
}

void athena_scene_node_remove_child(athena_scene_node *parent, athena_scene_node *child) {
    if (!parent || !child) return;
    list_remove_ptr((void**)parent->children, &parent->child_count, child);
    if (child->parent == parent)
        child->parent = NULL;
}

void athena_scene_node_attach(athena_scene_node *node, athena_object_data *obj) {
    if (!node || !obj) return;
    if (ensure_cap((void**)&node->attachments, &node->attach_capacity, node->attach_count + 1) != 0)
        return;
    node->attachments[node->attach_count++] = obj;
}

void athena_scene_node_detach(athena_scene_node *node, athena_object_data *obj) {
    if (!node) return;
    if (!obj) {
        node->attach_count = 0;
        return;
    }
    list_remove_ptr((void**)node->attachments, &node->attach_count, obj);
}

static void scene_apply_recursive(athena_scene_node *node, const MATRIX parent, int has_parent) {
    MATRIX local, world;
    matrix_functions->identity(local);
    matrix_functions->rotate(local, local, node->rotation);
    matrix_functions->scale(local, local, node->scale);
    matrix_functions->translate(local, local, node->position);

    if (has_parent) matrix_functions->multiply(world, parent, local);
    else            matrix_functions->copy(world, local);

    matrix_functions->copy(node->world, world);

    for (unsigned int i = 0; i < node->attach_count; i++) {
        athena_object_data *obj = node->attachments[i];
        if (!obj) continue;
        matrix_functions->copy(obj->transform, world);
        obj->position[0] = world[3];
        obj->position[1] = world[7];
        obj->position[2] = world[11];
        obj->position[3] = 1.0f;
        if (obj->update_collision)
            obj->update_collision(obj);
    }

    for (unsigned int i = 0; i < node->child_count; i++) {
        scene_apply_recursive(node->children[i], world, 1);
    }
}

void athena_scene_update(athena_scene_node *root) {
    if (!root) return;
    scene_apply_recursive(root, NULL, 0);
}

