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

    while (!list->should_stop) {
        WaitSema(list->sema_id);

        if (list->should_stop)
            break;

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

    SignalSema(list->done_sema_id);
    exit_task();
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
    if (list->sema_id < 0) {
        free(list);
        return NULL;
    }

    ee_sema_t done_sema;
    done_sema.init_count = 0;
    done_sema.max_count = 1;
    done_sema.option = 0;
    list->done_sema_id = CreateSema(&done_sema);
    if (list->done_sema_id < 0) {
        DeleteSema(list->sema_id);
        free(list);
        return NULL;
    }

    list->mutex_id = create_mutex();
    if (list->mutex_id < 0) {
        DeleteSema(list->sema_id);
        DeleteSema(list->done_sema_id);
        free(list);
        return NULL;
    }

    list->should_stop = false;

    int task = create_task("AsyncImage: Loading Thread", (void *)athena_image_list_thread, 4096, 16);
    if (task < 0) {
        DeleteSema(list->sema_id);
        DeleteSema(list->done_sema_id);
        delete_mutex(list->mutex_id);
        free(list);
        return NULL;
    }
    init_task(task, list);
    list->thread_id = task;

    return list;
}

void athena_image_list_destroy(AthenaImageList *list) {
    if (!list)
        return;

    list->should_stop = true;
    SignalSema(list->sema_id);

    WaitSema(list->done_sema_id);

    DeleteSema(list->sema_id);
    DeleteSema(list->done_sema_id);
    delete_mutex(list->mutex_id);

    if (list->list)
        free(list->list);
    free(list);
}

int athena_image_list_append(AthenaImageList *list, AthenaImage *image) {
    if (!list || !image)
        return -1;

    lock_mutex(list->mutex_id);

    AthenaImage **aux = realloc(list->list, (list->size + 1) * sizeof(AthenaImage *));
    if (!aux) {
        unlock_mutex(list->mutex_id);
        return -1;
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
