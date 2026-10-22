/*
 * Statistical PC sampler: a timer-counter handler records the interrupted
 * program counter at a fixed rate. The raw samples (little-endian u32 PCs)
 * are written to a file and resolved on the host with addr2line against an
 * unstripped ELF (make EE_STRIP=true), e.g. scripts/profile_report.py.
 */

#include <kernel.h>
#include <timer.h>
#include <stdio.h>
#include <stdlib.h>

#include <athena/sampler.h>

#define SAMPLER_MAX_SAMPLES (64 * 1024)

static u32 *samples = NULL;
static volatile u32 sample_count = 0;
static s32 counter_id = -1;
static u64 period = 0;

static u64 sampler_handler(s32 id, u64 scheduled_time, u64 actual_time, void *arg, void *pc_value)
{
    if (sample_count < SAMPLER_MAX_SAMPLES)
        samples[sample_count++] = (u32)pc_value;
    return period; /* delta to the next sample, in BUSCLK */
}

int athena_sampler_start(int hz)
{
    if (counter_id >= 0)
        return -1;
    if (hz <= 0)
        hz = 1000;

    if (!samples)
        samples = malloc(SAMPLER_MAX_SAMPLES * sizeof(u32));
    if (!samples)
        return -1;

    sample_count = 0;
    period = kBUSCLK / hz; /* the kernel clamps it to >= 0x3999 (~10 kHz) */

    counter_id = AllocTimerCounter();
    if (counter_id < 0)
        return -1;

    SetTimerHandler(counter_id, period, sampler_handler, NULL);
    StartTimerCounter(counter_id);
    return 0;
}

int athena_sampler_stop(const char *path)
{
    if (counter_id < 0)
        return -1;

    StopTimerCounter(counter_id);
    SetTimerHandler(counter_id, 0, NULL, NULL);
    FreeTimerCounter(counter_id);
    counter_id = -1;

    int count = (int)sample_count;
    if (path) {
        FILE *f = fopen(path, "wb");
        if (!f)
            return -1;
        fwrite(samples, sizeof(u32), count, f);
        fclose(f);
    }
    return count;
}
