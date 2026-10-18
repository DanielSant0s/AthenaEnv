#ifndef ATHENA_PARTICLES_RENDER_H
#define ATHENA_PARTICLES_RENDER_H
#include <particles.h>
#include <athena/image.h>
typedef struct AthenaParticleRenderer AthenaParticleRenderer;
AthenaParticleRenderer *athena_particles_renderer_create(AthenaParticles *system, AthenaImage *image,
    uint64_t blend, bool depth_test);
void athena_particles_renderer_destroy(AthenaParticleRenderer *renderer);
uint32_t athena_particles_draw(AthenaParticleRenderer *renderer);
#endif
