#ifndef ATHENA_SAMPLER_H
#define ATHENA_SAMPLER_H

/* Statistical PC sampler (profiling aid). Samples the interrupted PC `hz`
 * times per second; stop() writes the raw u32 PCs to `path` (NULL discards
 * them) and returns the sample count, or -1 on error. */
int athena_sampler_start(int hz);
int athena_sampler_stop(const char *path);

#endif
