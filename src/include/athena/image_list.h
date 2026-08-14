#ifndef ATHENA_IMAGE_LIST_H
#define ATHENA_IMAGE_LIST_H

#include <athena/image.h>

typedef struct AthenaImageList {
    AthenaImage **list;
    int size;
    int sema_id;
    int thread_id;
    int mutex_id;
} AthenaImageList;

AthenaImageList *athena_image_list_create(void);
void athena_image_list_destroy(AthenaImageList *list);
int athena_image_list_append(AthenaImageList *list, AthenaImage *image);
void athena_image_list_process(AthenaImageList *list);

#endif /* ATHENA_IMAGE_LIST_H */
