#ifndef ATHENA_RENDER_ASYNC_LOADER_H
#define ATHENA_RENDER_ASYNC_LOADER_H

#include <render.h>

typedef void (*athena_loader_cb)(athena_render_data *data, void *user);

typedef struct athena_async_job {
    char path[256];
    GSSURFACE *texture; // optional, may be NULL
    athena_loader_cb callback; // optional, may be NULL
    void *user; // user data for callback
} athena_async_job;

typedef struct athena_async_loader {
    athena_async_job *jobs;
    unsigned int count;
    unsigned int capacity;
    unsigned int jobs_per_step;
} athena_async_loader;

typedef void (*athena_loader_user_cleanup)(void *user);

athena_async_loader *athena_async_loader_create(unsigned int jobs_per_step);
void athena_async_loader_destroy(athena_async_loader *l);

int  athena_async_enqueue(athena_async_loader *l, const char *path, GSSURFACE *tex, athena_loader_cb cb, void *user);
unsigned int athena_async_process(athena_async_loader *l, unsigned int budget);
void athena_async_clear(athena_async_loader *l);

// Clears pending jobs and invokes a user cleanup callback per job (if provided).
void athena_async_clear_with(athena_async_loader *l, athena_loader_user_cleanup cleanup);

// Telemetry helpers
unsigned int athena_async_queue_size(athena_async_loader *l);
unsigned int athena_async_jobs_per_step(athena_async_loader *l);
void athena_async_set_jobs_per_step(athena_async_loader *l, unsigned int v);

// Destroy with cleanup of pending user payloads
void athena_async_destroy_with(athena_async_loader *l, athena_loader_user_cleanup cleanup);

#endif
