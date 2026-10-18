#ifndef ATHENA_PARTICLES_H
#define ATHENA_PARTICLES_H
#include <stdbool.h>
#include <stdint.h>
#include <sprite_data.h>

#define ATHENA_PARTICLE_MAX_CAPACITY 16384
#define ATHENA_PARTICLE_MAX_EMITTERS 64

typedef struct {
    float life_min, life_max, size_min, size_max, scale_end, aspect;
    float velocity_min[3], velocity_max[3], spread[3], acceleration[3], drag;
    float color[4], color_end[4], fade_in, fade_out;
    float frame[4], frame_rate;
    uint32_t frame_count, frame_columns;
    bool frame_loop;
} AthenaParticleEffect;

typedef struct {
    float matrix[16], width, height, focal, z_scale;
} AthenaParticleView;

typedef struct {
    uint32_t capacity, active, visible, emitted, dropped;
    uint32_t batches;
    bool radix, sorted;
} AthenaParticleStats;

typedef struct AthenaParticles AthenaParticles;
void particles_effect_defaults(AthenaParticleEffect *effect);
bool particles_effect_valid(const AthenaParticleEffect *effect);
AthenaParticles *particles_create(uint32_t capacity, bool world, bool sort, uint32_t seed);
void particles_destroy(AthenaParticles *system);
int particles_add_emitter(AthenaParticles *system, const AthenaParticleEffect *effect);
bool particles_configure(AthenaParticles *system, uint32_t emitter, const AthenaParticleEffect *effect);
bool particles_position(AthenaParticles *system, uint32_t emitter, const float position[3]);
bool particles_start(AthenaParticles *system, uint32_t emitter, float rate);
void particles_stop(AthenaParticles *system, uint32_t emitter);
uint32_t particles_burst(AthenaParticles *system, uint32_t emitter, uint32_t count, const float *position);
bool particles_update(AthenaParticles *system, float dt);
void particles_clear(AthenaParticles *system, bool reset_seed, uint32_t seed);
uint32_t particles_prepare(AthenaParticles *system, const AthenaParticleView *view, athena_sprite_data *output);
AthenaParticleStats particles_stats(const AthenaParticles *system);
bool particles_world(const AthenaParticles *system);
#endif
