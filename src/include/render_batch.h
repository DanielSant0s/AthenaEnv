#ifndef ATHENA_RENDER_BATCH_H
#define ATHENA_RENDER_BATCH_H

#include <render.h>

typedef int (*athena_batch_cmp_fn)(const athena_object_data* a, const athena_object_data* b);

typedef struct athena_batch {
    athena_object_data **objects;
    unsigned int count;
    unsigned int capacity;
    int auto_sort;
    athena_batch_cmp_fn comparator;
} athena_batch;

athena_batch *athena_batch_create(void);
void athena_batch_destroy(athena_batch *b);
void athena_batch_clear(athena_batch *b);
int  athena_batch_add(athena_batch *b, athena_object_data *obj);
void athena_batch_set_sort(athena_batch *b, athena_batch_cmp_fn cmp, int auto_sort);
unsigned int athena_batch_render(athena_batch *b);

#endif

