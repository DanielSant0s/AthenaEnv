#include <stdlib.h>
#include <string.h>

#include <render.h>
#include <render_batch.h>

static int default_batch_cmp(const athena_object_data* a, const athena_object_data* b) {
    if (!a || !b || !a->data || !b->data)
        return 0;
    // pipeline first
    int pa = (int)a->data->pipeline;
    int pb = (int)b->data->pipeline;
    if (pa != pb) return pa - pb;
    // then first material texture id
    int ta = (a->data->material_count > 0) ? a->data->materials[0].texture_id : -1;
    int tb = (b->data->material_count > 0) ? b->data->materials[0].texture_id : -1;
    return ta - tb;
}

athena_batch *athena_batch_create(void) {
    athena_batch *b = (athena_batch*)malloc(sizeof(*b));
    if (!b) return NULL;
    b->objects = NULL;
    b->count = 0;
    b->capacity = 0;
    b->auto_sort = 1;
    b->comparator = default_batch_cmp;
    return b;
}

void athena_batch_destroy(athena_batch *b) {
    if (!b) return;
    free(b->objects);
    free(b);
}

void athena_batch_clear(athena_batch *b) {
    if (!b) return;
    b->count = 0;
}

static int ensure_capacity(athena_batch *b, unsigned int extra) {
    unsigned int need = b->count + extra;
    if (need <= b->capacity) return 0;
    unsigned int cap = b->capacity ? b->capacity * 2 : 8;
    while (cap < need) cap *= 2;
    void *ptr = realloc(b->objects, cap * sizeof(athena_object_data*));
    if (!ptr) return -1;
    b->objects = (athena_object_data**)ptr;
    b->capacity = cap;
    return 0;
}

int athena_batch_add(athena_batch *b, athena_object_data *obj) {
    if (!b || !obj) return -1;
    if (ensure_capacity(b, 1) != 0) return -1;
    b->objects[b->count++] = obj;
    return (int)b->count;
}

void athena_batch_set_sort(athena_batch *b, athena_batch_cmp_fn cmp, int auto_sort) {
    if (!b) return;
    b->comparator = cmp ? cmp : default_batch_cmp;
    b->auto_sort = auto_sort ? 1 : 0;
}

static const athena_batch *g_sort_batch = NULL;
static int qsort_cmp(const void *a, const void *b) {
    const athena_object_data *const *oa = (const athena_object_data*const*)a;
    const athena_object_data *const *ob = (const athena_object_data*const*)b;
    if (!g_sort_batch || !g_sort_batch->comparator) return 0;
    return g_sort_batch->comparator(*oa, *ob);
}

unsigned int athena_batch_render(athena_batch *b) {
    if (!b || b->count == 0) return 0;
    if (b->auto_sort && b->comparator && b->count > 1) {
        g_sort_batch = b;
        qsort(b->objects, b->count, sizeof(athena_object_data*), qsort_cmp);
        g_sort_batch = NULL;
    }
    for (unsigned int i = 0; i < b->count; i++) {
        render_object(b->objects[i]);
    }
    return b->count;
}
