#include <particles.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

typedef struct { float p[3], v[3], age, life, size; uint32_t emitter; } Particle;
typedef struct { AthenaParticleEffect effect; float position[3], rate, damping; double carry; bool running; } Emitter;
typedef struct { float x, y, w, h, z, age; uint32_t particle, alpha; } Projected;
typedef union { float depth; uint32_t bits; } Key;
struct AthenaParticles {
    uint32_t capacity, count, emitter_count, seed, emitted, dropped;
    bool world, sort;
    Particle *pool;
    Projected *projected;
    Key *keys;
    uint32_t *order, *scratch, histogram[256];
    Emitter emitters[ATHENA_PARTICLE_MAX_EMITTERS];
    AthenaParticleStats stats;
};

static float clampf(float x, float lo, float hi) { return fmaxf(lo, fminf(x, hi)); }
static float random01(AthenaParticles *s) {
    s->seed = s->seed * 1664525u + 1013904223u;
    return (s->seed >> 8) * (1.0f / 16777216.0f);
}
static float range(AthenaParticles *s, float a, float b) { return a + (b - a) * random01(s); }
static uint32_t add_sat(uint32_t a, uint32_t b) { return b > UINT32_MAX - a ? UINT32_MAX : a + b; }
void particles_effect_defaults(AthenaParticleEffect *e) {
    memset(e, 0, sizeof(*e));
    e->life_min = e->life_max = 1.0f;
    e->size_min = e->size_max = 16.0f;
    e->scale_end = e->aspect = 1.0f;
    for (int i = 0; i < 4; i++) e->color[i] = e->color_end[i] = 128;
    e->color_end[3] = 0;
    e->frame[2] = e->frame[3] = 16;
    e->frame_count = e->frame_columns = 1;
}
bool particles_effect_valid(const AthenaParticleEffect *e) {
    if (!e || !isfinite(e->life_min) || !isfinite(e->life_max) || e->life_min <= 0 || e->life_max < e->life_min ||
        !isfinite(e->size_min) || !isfinite(e->size_max) || e->size_min <= 0 || e->size_max < e->size_min ||
        !isfinite(e->scale_end) || e->scale_end < 0 || !isfinite(e->aspect) || e->aspect <= 0 ||
        !isfinite(e->drag) || e->drag < 0 || !isfinite(e->fade_in) || !isfinite(e->fade_out) ||
        e->fade_in < 0 || e->fade_in > 1 || e->fade_out < 0 || e->fade_out > 1 ||
        !isfinite(e->frame_rate) || e->frame_rate < 0 || e->frame_count < 1 || e->frame_count > 4096 ||
        e->frame_columns < 1 || e->frame_columns > 4096) return false;
    for (int i = 0; i < 3; i++) if (!isfinite(e->velocity_min[i]) || !isfinite(e->velocity_max[i]) ||
        e->velocity_max[i] < e->velocity_min[i] || !isfinite(e->spread[i]) || e->spread[i] < 0 || !isfinite(e->acceleration[i])) return false;
    for (int i = 0; i < 4; i++) if (!isfinite(e->color[i]) || !isfinite(e->color_end[i]) ||
        e->color[i] < 0 || e->color[i] > 255 || e->color_end[i] < 0 || e->color_end[i] > 255 || !isfinite(e->frame[i])) return false;
    return e->frame[0] >= 0 && e->frame[1] >= 0 && e->frame[2] >= 1 && e->frame[3] >= 1;
}
AthenaParticles *particles_create(uint32_t capacity, bool world, bool sort, uint32_t seed) {
    if (!capacity || capacity > ATHENA_PARTICLE_MAX_CAPACITY) return NULL;
    AthenaParticles *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->capacity = capacity; s->world = world; s->sort = sort; s->seed = seed;
    s->pool = calloc(capacity, sizeof(*s->pool));
    s->projected = calloc(capacity, sizeof(*s->projected));
    s->keys = calloc(capacity, sizeof(*s->keys));
    s->order = calloc(capacity, sizeof(*s->order));
    s->scratch = calloc(capacity, sizeof(*s->scratch));
    if (!s->pool || !s->projected || !s->keys || !s->order || !s->scratch) { particles_destroy(s); return NULL; }
    return s;
}
void particles_destroy(AthenaParticles *s) {
    if (!s) return;
    free(s->pool); free(s->projected); free(s->keys); free(s->order); free(s->scratch); free(s);
}
int particles_add_emitter(AthenaParticles *s, const AthenaParticleEffect *e) {
    if (!s || !particles_effect_valid(e) || s->emitter_count == ATHENA_PARTICLE_MAX_EMITTERS) return -1;
    uint32_t id = s->emitter_count++;
    s->emitters[id].effect = *e;
    return (int)id;
}
bool particles_configure(AthenaParticles *s, uint32_t id, const AthenaParticleEffect *e) {
    if (!s || id >= s->emitter_count || !particles_effect_valid(e)) return false;
    s->emitters[id].effect = *e;
    return true;
}
bool particles_position(AthenaParticles *s, uint32_t id, const float p[3]) {
    if (!s || id >= s->emitter_count || !p) return false;
    for (int i = 0; i < 3; i++) if (!isfinite(p[i])) return false;
    memcpy(s->emitters[id].position, p, sizeof(float) * 3);
    return true;
}
bool particles_start(AthenaParticles *s, uint32_t id, float rate) {
    if (!s || id >= s->emitter_count || !isfinite(rate) || rate < 0 || rate > 1000000) return false;
    s->emitters[id].rate = rate; s->emitters[id].running = true;
    return true;
}
void particles_stop(AthenaParticles *s, uint32_t id) {
    if (!s || id >= s->emitter_count) return;
    s->emitters[id].running = false; s->emitters[id].carry = 0;
}
uint32_t particles_burst(AthenaParticles *s, uint32_t id, uint32_t count, const float *position) {
    if (!s || id >= s->emitter_count) return 0;
    Emitter *em = &s->emitters[id]; const AthenaParticleEffect *e = &em->effect;
    const float *pos = position ? position : em->position;
    for (int j = 0; j < 3; j++) if (!isfinite(pos[j])) return 0;
    uint32_t actual = count;
    if (actual > s->capacity - s->count) actual = s->capacity - s->count;
    s->dropped = add_sat(s->dropped, count - actual); s->emitted = add_sat(s->emitted, actual);
    for (uint32_t i = 0; i < actual; i++) {
        Particle *p = &s->pool[s->count++];
        for (int j = 0; j < 3; j++) {
            p->p[j] = pos[j] + range(s, -e->spread[j], e->spread[j]);
            p->v[j] = range(s, e->velocity_min[j], e->velocity_max[j]);
        }
        p->age = 0; p->life = range(s, e->life_min, e->life_max);
        p->size = range(s, e->size_min, e->size_max); p->emitter = id;
    }
    return actual;
}
bool particles_update(AthenaParticles *s, float dt) {
    if (!s || !isfinite(dt) || dt < 0) return false;
    if (dt == 0) return true;
    for (uint32_t i = 0; i < s->emitter_count; i++)
        s->emitters[i].damping = expf(-s->emitters[i].effect.drag * dt);
    uint32_t kept = 0;
    for (uint32_t i = 0; i < s->count; i++) {
        Particle p = s->pool[i]; p.age += dt;
        if (p.age >= p.life) continue;
        const AthenaParticleEffect *e = &s->emitters[p.emitter].effect;
        float damping = s->emitters[p.emitter].damping;
        bool valid = true;
        for (int j = 0; j < 3; j++) {
            p.p[j] += p.v[j] * dt;
            p.v[j] = (p.v[j] + e->acceleration[j] * dt) * damping;
            valid &= isfinite(p.p[j]) && isfinite(p.v[j]);
        }
        if (valid) s->pool[kept++] = p;
    }
    s->count = kept;
    // Age existing particles first: continuous newborns are rendered at age 0.
    // Excess requests are dropped, not saved as an unbounded catch-up backlog.
    for (uint32_t i = 0; i < s->emitter_count; i++) {
        Emitter *e = &s->emitters[i];
        if (!e->running || e->rate == 0) continue;
        double requested = e->carry + (double)e->rate * dt;
        double whole = floor(requested);
        e->carry = requested - whole;
        particles_burst(s, i, whole > UINT32_MAX ? UINT32_MAX : (uint32_t)whole, NULL);
    }
    return true;
}
void particles_clear(AthenaParticles *s, bool reset_seed, uint32_t seed) {
    if (!s) return;
    s->count = s->emitted = s->dropped = 0; memset(&s->stats, 0, sizeof(s->stats));
    for (uint32_t i = 0; i < s->emitter_count; i++) s->emitters[i].carry = 0;
    if (reset_seed) s->seed = seed;
}
static void sort_indices(AthenaParticles *s, uint32_t n) {
    s->stats.radix = n > 256;
    uint32_t *from = s->order, *to = s->scratch;
    if (s->stats.radix) {
        for (uint32_t shift = 0; shift < 32; shift += 8) {
            memset(s->histogram, 0, sizeof(s->histogram));
            for (uint32_t i = 0; i < n; i++) s->histogram[(s->keys[from[i]].bits >> shift) & 255]++;
            uint32_t offset = 0;
            for (int b = 255; b >= 0; b--) { uint32_t size = s->histogram[b]; s->histogram[b] = offset; offset += size; }
            for (uint32_t i = 0; i < n; i++) {
                uint32_t id = from[i], b = (s->keys[id].bits >> shift) & 255;
                to[s->histogram[b]++] = id;
            }
            uint32_t *tmp = from; from = to; to = tmp;
        }
    } else {
        for (uint32_t width = 1; width < n; width *= 2) {
            for (uint32_t left = 0; left < n; left += width * 2) {
                uint32_t mid = left + width < n ? left + width : n;
                uint32_t end = mid + width < n ? mid + width : n;
                uint32_t a = left, b = mid, k = left;
                while (a < mid && b < end) {
                    if (s->keys[from[a]].depth >= s->keys[from[b]].depth) to[k++] = from[a++];
                    else to[k++] = from[b++];
                }
                while (a < mid) to[k++] = from[a++];
                while (b < end) to[k++] = from[b++];
            }
            uint32_t *tmp = from; from = to; to = tmp;
        }
        if (from != s->order) memcpy(s->order, from, n * sizeof(uint32_t));
    }
}
uint32_t particles_prepare(AthenaParticles *s, const AthenaParticleView *view, athena_sprite_data *out) {
    if (!s) return 0;
    s->stats.visible = s->stats.batches = 0; s->stats.radix = false;
    if (!view || !out || !isfinite(view->width) || !isfinite(view->height) ||
        !isfinite(view->z_scale) || view->z_scale <= 0 || view->width <= 0 || view->height <= 0) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < s->count; i++) {
        const Particle *p = &s->pool[i]; const AthenaParticleEffect *e = &s->emitters[p->emitter].effect;
        float age = clampf(p->age / p->life, 0, 1);
        float alpha = e->color[3] + (e->color_end[3] - e->color[3]) * age;
        if (e->fade_in > 0) alpha *= fminf(1, age / e->fade_in);
        if (e->fade_out > 0) alpha *= fminf(1, (1 - age) / e->fade_out);
        if (alpha < 1) continue;
        float h = p->size * (1 + (e->scale_end - 1) * age), w = h * e->aspect;
        float x = p->p[0], y = p->p[1], z = clampf(p->p[2], 0, view->z_scale * 2), depth = view->z_scale * 2 - z;
        if (s->world) {
            const float *m = view->matrix;
            float cx = x * m[0] + y * m[4] + p->p[2] * m[8] + m[12];
            float cy = x * m[1] + y * m[5] + p->p[2] * m[9] + m[13];
            float cz = x * m[2] + y * m[6] + p->p[2] * m[10] + m[14];
            float cw = x * m[3] + y * m[7] + p->p[2] * m[11] + m[15];
            if (!isfinite(cw) || !isfinite(cz) || cw <= 0 || cz < -cw || cz > cw) continue;
            float inv = 1.0f / cw;
            x = (cx * inv + 1) * view->width * 0.5f;
            y = (cy * inv + 1) * view->height * 0.5f;
            w *= view->focal * inv; h *= view->focal * inv;
            z = (cz * inv + 1) * view->z_scale; depth = cw;
        }
        x -= w * 0.5f; y -= h * 0.5f;
        if (!isfinite(x) || !isfinite(y) || !isfinite(w) || !isfinite(h) || w < 0.5f || h < 0.5f ||
            x + w < 0 || y + h < 0 || x >= view->width || y >= view->height) continue;
        s->projected[n] = (Projected){x, y, w, h, z, age, i, (uint32_t)clampf(alpha, 0, 255)};
        s->keys[n].depth = depth; s->order[n] = n; n++;
    }
    s->stats.radix = false;
    if (s->sort && n > 1) sort_indices(s, n);
    for (uint32_t i = 0; i < n; i++) {
        const Projected *q = &s->projected[s->order[i]];
        const Particle *p = &s->pool[q->particle]; const AthenaParticleEffect *e = &s->emitters[p->emitter].effect;
        athena_sprite_data *o = &out[i]; memset(o, 0, sizeof(*o));
        o->x = q->x; o->y = q->y; o->w = q->w; o->h = q->h; o->zindex = q->z;
        uint32_t f = 0;
        if (e->frame_count > 1 && e->frame_rate > 0) {
            double frame = floor((double)p->age * e->frame_rate);
            f = e->frame_loop ? (uint32_t)fmod(frame, e->frame_count) :
                frame >= e->frame_count ? e->frame_count - 1 : (uint32_t)frame;
        }
        float u = e->frame[0] + (f % e->frame_columns) * e->frame[2];
        float v = e->frame[1] + (f / e->frame_columns) * e->frame[3];
        o->u1 = u + 0.5f; o->v1 = v + 0.5f; o->u2 = u + e->frame[2] - 0.5f; o->v2 = v + e->frame[3] - 0.5f;
        // Clip geometry and UVs together before GS fixed-point conversion.
        // Large/near billboards must never wrap around the GS coordinate range.
        float left = fmaxf(q->x, 0), top = fmaxf(q->y, 0);
        float right = fminf(q->x + q->w, view->width), bottom = fminf(q->y + q->h, view->height);
        float du = o->u2 - o->u1, dv = o->v2 - o->v1;
        o->u2 = o->u1 + du * ((right - q->x) / q->w);
        o->v2 = o->v1 + dv * ((bottom - q->y) / q->h);
        o->u1 += du * ((left - q->x) / q->w); o->v1 += dv * ((top - q->y) / q->h);
        o->x = left; o->y = top; o->w = right - left; o->h = bottom - top;
        o->r = (uint32_t)clampf(e->color[0] + (e->color_end[0] - e->color[0]) * q->age, 0, 255);
        o->g = (uint32_t)clampf(e->color[1] + (e->color_end[1] - e->color[1]) * q->age, 0, 255);
        o->b = (uint32_t)clampf(e->color[2] + (e->color_end[2] - e->color[2]) * q->age, 0, 255); o->a = q->alpha;
    }
    s->stats.visible = n; s->stats.batches = (n + 49) / 50;
    return n;
}
AthenaParticleStats particles_stats(const AthenaParticles *s) {
    AthenaParticleStats r = {0}; if (!s) return r;
    r = s->stats; r.capacity = s->capacity; r.active = s->count; r.emitted = s->emitted; r.dropped = s->dropped; r.sorted = s->sort;
    return r;
}
bool particles_world(const AthenaParticles *s) { return s && s->world; }
