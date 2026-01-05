#include <stdlib.h>
#include <string.h>

#include <render.h>
#include <render_async_loader.h>

static int ensure_cap_jobs(athena_async_loader *l, unsigned int need) {
    if (need <= l->capacity) return 0;
    unsigned int c = l->capacity ? l->capacity * 2 : 4;
    while (c < need) c *= 2;
    void *p = realloc(l->jobs, c * sizeof(athena_async_job));
    if (!p) return -1;
    l->jobs = (athena_async_job*)p;
    l->capacity = c;
    return 0;
}

athena_async_loader *athena_async_loader_create(unsigned int jobs_per_step) {
    athena_async_loader *l = (athena_async_loader*)malloc(sizeof(*l));
    if (!l) return NULL;
    l->jobs = NULL;
    l->count = 0;
    l->capacity = 0;
    l->jobs_per_step = (jobs_per_step == 0) ? 1 : jobs_per_step;
    return l;
}

void athena_async_loader_destroy(athena_async_loader *l) {
    if (!l) return;
    // Ensure user data is cleaned if someone forgot to clear
    for (unsigned int i = 0; i < l->count; i++) {
        // No default cleanup at core level; caller should have cleared with cleanup via binding.
        // We just drop references here.
    }
    free(l->jobs);
    free(l);
}

int athena_async_enqueue(athena_async_loader *l, const char *path, GSSURFACE *tex, athena_loader_cb cb, void *user) {
    if (!l || !path) return -1;
    if (ensure_cap_jobs(l, l->count + 1) != 0) return -1;
    athena_async_job *job = &l->jobs[l->count++];
    memset(job, 0, sizeof(*job));
    strncpy(job->path, path, sizeof(job->path)-1);
    job->texture = tex;
    job->callback = cb;
    job->user = user;
    return (int)l->count;
}

unsigned int athena_async_process(athena_async_loader *l, unsigned int budget) {
    if (!l || l->count == 0) return 0;
    unsigned int steps = budget ? budget : l->jobs_per_step;
    if (steps == 0) steps = 1;
    unsigned int processed = 0;
    while (processed < steps && l->count > 0) {
        athena_async_job job = l->jobs[0];
        memmove(&l->jobs[0], &l->jobs[1], (l->count - 1) * sizeof(athena_async_job));
        l->count--;

        athena_render_data *rd = (athena_render_data*)malloc(sizeof(athena_render_data));
        if (rd) {
            memset(rd, 0, sizeof(*rd));
            loadModel(rd, job.path, job.texture);
        }

        if (job.callback)
            job.callback(rd, job.user);

        processed++;
    }
    return processed;
}

void athena_async_clear(athena_async_loader *l) {
    if (!l) return;
    l->count = 0;
}

void athena_async_clear_with(athena_async_loader *l, athena_loader_user_cleanup cleanup) {
    if (!l) return;
    if (cleanup) {
        for (unsigned int i = 0; i < l->count; i++) {
            if (l->jobs[i].user)
                cleanup(l->jobs[i].user);
        }
    }
    l->count = 0;
}

unsigned int athena_async_queue_size(athena_async_loader *l) {
    return l ? l->count : 0;
}

unsigned int athena_async_jobs_per_step(athena_async_loader *l) {
    return l ? l->jobs_per_step : 0;
}

void athena_async_set_jobs_per_step(athena_async_loader *l, unsigned int v) {
    if (!l) return;
    l->jobs_per_step = (v == 0) ? 1 : v;
}

void athena_async_destroy_with(athena_async_loader *l, athena_loader_user_cleanup cleanup) {
    if (!l) return;
    athena_async_clear_with(l, cleanup);
    athena_async_loader_destroy(l);
}
