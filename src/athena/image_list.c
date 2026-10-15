#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <kernel.h>

#include <athena/image_list.h>
#include <graphics.h>
#include <lockman.h>
#include <taskman.h>

static int athena_image_list_thread(void *data) {
    AthenaImageList *list = data;

    while (true) {
        WaitSema(list->sema_id);

        lock_mutex(list->mutex_id);
        int count = list->size;
        AthenaImage** batch = list->list;
        list->list = NULL;
        list->size = 0;
        unlock_mutex(list->mutex_id);

        for (int i = 0; i < count; i++) {
            AthenaImage *img = batch[i];
            load_image(img->tex, img->path, img->delayed);
            img->width  = (float)img->tex->Width;
            img->height = (float)img->tex->Height;
            img->endx   = (float)img->tex->Width;
            img->endy   = (float)img->tex->Height;
            img->loaded = true;
        }

        free(batch);
    }

    return 0;
}

AthenaImageList *athena_image_list_create(void) {
    AthenaImageList *list = calloc(1, sizeof(AthenaImageList));
    if (!list)
        return NULL;

    ee_sema_t sema;
    sema.init_count = 0;
    sema.max_count = 1;
    sema.option = 0;
    list->sema_id = CreateSema(&sema);
    list->mutex_id = create_mutex();

    int task = create_task("AsyncImage: Loading Thread", (void *)athena_image_list_thread, 4096, 16);
    init_task(task, list);
    list->thread_id = task;

    return list;
}

void athena_image_list_destroy(AthenaImageList *list) {
    if (!list)
        return;

    kill_task(list->thread_id);
    DeleteSema(list->sema_id);
    delete_mutex(list->mutex_id);
    
    if (list->size > 0)
        free(list->list);
    free(list);
}

int athena_image_list_append(AthenaImageList *list, AthenaImage *image) {
    if (!list || !image)
        return -1;

    lock_mutex(list->mutex_id);

    AthenaImage **aux = malloc((list->size + 1) * sizeof(AthenaImage *));
    if (!aux) {
        unlock_mutex(list->mutex_id);
        return -1;
    }

    if (list->size > 0) {
        memcpy(aux, list->list, list->size * sizeof(AthenaImage *));
        free(list->list);
    }

    list->list = aux;
    list->list[list->size] = image;
    list->size++;
    image->loaded = false;

    unlock_mutex(list->mutex_id);
    return 0;
}

void athena_image_list_process(AthenaImageList *list) {
    if (!list)
        return;
    SignalSema(list->sema_id);
}
