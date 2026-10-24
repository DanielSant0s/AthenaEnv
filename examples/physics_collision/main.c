/*
 * Physics demo, in C: the port of bin/physics_collision.js on the pure-C API
 * (no QuickJS). Same scene, same rules, same numbers -- it is meant to be
 * compared side by side with the script.
 *
 * Rigid bodies on the VU0 physics (crates, monkey balls, a sphere player),
 * the Sponza courtyard as box colliders, a skinned Mixamo character that walks,
 * jumps, kicks (live foot bone vs. the props) and dances, projected silhouette
 * shadows with exact overlaps, native particles, and the depth fog / DOF /
 * channel shuffle post-passes.
 *
 * Build: make demos (into bin/athena_demos.elf, launched from the shell), or alone:
 * make capp CAPP_NAME=physics_collision -> bin/athena_physics_collision.elf.
 * Runs from bin/, like the script: fonts/ and render/ are next to the ELF.
 *
 * Controls (port 0):
 *   L-stick / D-pad  walk           R1        run
 *   R-stick          orbit camera   L2 / R2   zoom out / in
 *   Cross            jump           Square    kick
 *   Circle           throw a ball   L1        next dance
 *   Select           post-fx preset Start     reset the sandbox
 *   L3               shadows        R3        particle density
 *   Triangle         home
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <athena/athena.h>
#include "athenaos/app.h"

#if defined(ATHENA_GRAPHICS) && defined(ATHENA_PHYSICS)

#define PI 3.14159265f

static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline int mini(int a, int b) { return a < b ? a : b; }
static inline int maxi(int a, int b) { return a > b ? a : b; }

// Color.new(r, g, b, a): alpha 128 is opaque.
#define RGBA(r, g, b, a) athena_color_rgba((r), (g), (b), (a))

// The footer's hints; the SDK appends Triangle "Home".
static const AthenaOSHint HINTS[] = {
    { "L-stick", "Walk" }, { "R1", "Run" }, { "cross", "Jump" }, { "square", "Kick" },
    { "circle", "Throw" }, { "L1", "Dance" },
};
#define HINT_COUNT ((int)(sizeof(HINTS) / sizeof(HINTS[0])))

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------

#define PLAYER_RADIUS 0.34f
static const float PLAYER_START[3] = { 0.0f, 1.0f, 6.5f };

#define CRATE_HALF 0.38f
#define CRATE_SIZE (CRATE_HALF * 2.0f)
#define BALL_RADIUS 0.45f

#define RIGHT_FOOT_BONE 22 // mixamorig:RightFoot in Twerk.gltf
#define RIGHT_TOE_BONE  23 // mixamorig:RightToeBase in Twerk.gltf

static const float crateSpawns[][3] = {
    // Bottom row of courtyard pyramid
    { -0.82f, CRATE_HALF + 0.02f, 0.5f },
    {  0.00f, CRATE_HALF + 0.02f, 0.5f },
    {  0.82f, CRATE_HALF + 0.02f, 0.5f },
    // Middle row of courtyard pyramid
    { -0.41f, CRATE_HALF * 3.0f + 0.05f, 0.5f },
    {  0.41f, CRATE_HALF * 3.0f + 0.05f, 0.5f },
    // Top crate of courtyard pyramid
    {  0.00f, CRATE_HALF * 5.0f + 0.08f, 0.5f },
    // Tier-1 North Terrace Ledge Crates (Y = 1.05m, kickable off the balcony!)
    { -2.25f, 1.05f + CRATE_HALF + 0.03f, -8.5f },
    {  2.25f, 1.05f + CRATE_HALF + 0.03f, -8.5f },
    // Tier-2 Upper Terrace Pedestal Crates (Y = 1.95m & Y = 2.55m)
    { -2.10f, 1.95f + CRATE_HALF + 0.03f, -12.6f },
    {  2.10f, 1.95f + CRATE_HALF + 0.03f, -12.6f },
    {  0.00f, 2.55f + CRATE_HALF + 0.03f, -12.95f },
};
#define CRATE_COUNT ((int)(sizeof(crateSpawns) / sizeof(crateSpawns[0])))

static const float ballSpawns[][3] = {
    { -1.8f, 1.5f, 3.5f },
    {  1.8f, 1.5f, 3.5f },
    {  0.0f, 2.2f, -9.0f },
};
#define BALL_COUNT ((int)(sizeof(ballSpawns) / sizeof(ballSpawns[0])))
#define PROP_COUNT (CRATE_COUNT + BALL_COUNT)

// Crates first, then balls, as allProps in the script.
typedef struct {
    AthenaRenderObject *obj;
    AthenaRenderData *data;
    AthenaPhysicsGeom *geom;
    AthenaPhysicsBody *body;
    const float *spawn;
    float fx_previous_vy;
    float fx_impact_cooldown;
} Prop;

static const char *const EMOTES[] = { "samba", "gangnam", "hiphop", "twerk", "capoeira" };
#define EMOTE_COUNT 5

// --- Particles: one shared atlas/pool for dust, wood chips and stone grit ---
static const int FX_LEVELS[] = { 0, 128, 256, 512, 1024 };
#define FX_LEVEL_COUNT 5
#define FX_SEED 42978u

// --- Silhouette shadows ---
#define SHADOW_PROP_LIMIT 6
#define SHADOW_DISTANCE 20.0f
#define SHADOW_RAY_LENGTH 12.0f
#define SHADOW_CAMERA_DISTANCE 32.0f
#define SHADOW_OPACITY 0.42f
enum { SHADOW_OFF, SHADOW_EXACT, SHADOW_FAST, SHADOW_MODE_COUNT };
static const char *const SHADOW_MODE_NAMES[] = { "OFF", "ON", "ON (fast overlaps)" };

typedef struct {
    AthenaRenderObject *obj;
    AthenaRenderData *data;
    AthenaShadowProjector *projector;
    float size, x, y, z;
    int x0, y0, x1, y1;
    int group;
} ShadowCaster;

typedef struct {
    ShadowCaster *members[SHADOW_PROP_LIMIT + 1];
    int count;
    int x0, y0, x1, y1;
} ShadowGroup;

typedef struct {
    int prop;
    float x, y, z, distance;
} ShadowCandidate;

// --- Post-fx presets (SELECT) ---
#define DOF_SHIFT 3
enum { FX_FOG, FX_DOF, FX_DOF_FOG, FX_LUT, FX_SHIFT, FX_OFF };
typedef struct {
    const char *name;
    int type;
    int strength;
    Color color;
    float start, end;
    eFogCurve curve;
    float density, sky_fog;
    int shift;
} FogPreset;

// Colors are filled in at startup (athena_color_rgba is a function).
static FogPreset FOG_PRESETS[] = {
    { "MIST FOG (LINEAR)",      FX_FOG,     0,   0, 8.5f, 36.0f, FOG_CURVE_LINEAR, 1.5f,  0.35f, 4 },
    { "DEPTH OF FIELD (DOF)",   FX_DOF,     124, 0, 0, 0,        FOG_CURVE_LINEAR, 1.5f,  0.0f,  0 },
    { "DOF + MIST FOG",         FX_DOF_FOG, 118, 0, 9.0f, 36.0f, FOG_CURVE_LINEAR, 1.5f,  0.35f, DOF_SHIFT },
    { "SILENT HILL (EXP2)",     FX_FOG,     0,   0, 5.5f, 24.0f, FOG_CURVE_EXP2,   1.85f, 0.85f, 4 },
    { "GOLDEN DUSK (EXP)",      FX_FOG,     0,   0, 7.5f, 32.0f, FOG_CURVE_EXP,    1.6f,  0.45f, 4 },
    { "Z-BUFFER SHUFFLE LUT",   FX_LUT,     0,   0, 0, 0,        FOG_CURVE_LINEAR, 1.5f,  0.0f,  0 },
    { "NORMALIZED Z (SHIFT 4)", FX_SHIFT,   0,   0, 0, 0,        FOG_CURVE_LINEAR, 1.5f,  0.0f,  4 },
    { "POSTFX OFF",             FX_OFF,     0,   0, 0, 0,        FOG_CURVE_LINEAR, 1.5f,  0.0f,  0 },
};
#define FOG_PRESET_COUNT ((int)(sizeof(FOG_PRESETS) / sizeof(FOG_PRESETS[0])))

// --- Automatic benchmark ---
// Cycles through lighting on/off x post-fx on/off, BENCH_PHASE_MS each, and
// prints the averaged frame profile of every phase (the first BENCH_WARMUP_MS
// and 60 frames of a phase are skipped). Same phases and log line as the
// script, so the two can be compared directly. 0 to play normally.
#define BENCHMARK 0
#define BENCH_PHASE_MS 10000.0f
#define BENCH_WARMUP_MS 2000.0f
static const struct { const char *name; bool lights, postfx; } BENCH_PHASES[] = {
    { "lights ON  | postfx ON ", true,  true  },
    { "lights ON  | postfx OFF", true,  false },
    { "lights OFF | postfx ON ", false, true  },
    { "lights OFF | postfx OFF", false, false },
};

typedef struct {
    AthenaOSApp ui;
    AthenaPad *pad;
    int width, height;
    AthenaImage *fbuf, *zbuf;
    AthenaImage *sky;

    // Particles
    AthenaImage *fx_atlas;
    AthenaParticles *fx;
    AthenaParticleRenderer *fx_renderer;
    int fx_dust, fx_wood, fx_stone;
    int fx_level;
    float fx_next_step;

    // Physics
    AthenaPhysicsWorld *world;
    AthenaPhysicsJointGroup *jgroup;
    AthenaPhysicsSpace *space;
    AthenaPhysicsSpace *shadow_space; // static receivers only: a caster must not raycast onto itself

    // Scene
    AthenaRenderData *scene;
    AthenaRenderObject *scene_object;

    // Player
    AthenaAnim3dCollection *anims;
    AthenaRenderData *skin;
    AthenaRenderObject *skin_object;
    AthenaPhysicsGeom *player_geom;
    AthenaPhysicsBody *player_body;
    float player_yaw, walk_cycle;
    const char *current_anim;
    int emote_idx;
    bool emote_active;
    int anim_timer, jump_windup, jump_cooldown, ground_grace;
    int kick_phase, kick_hits, kick_cooldown;
    bool kick_hit[PROP_COUNT];
    char status_text[48];
    int status_timer;

    // Props
    AthenaRenderData *box, *monkey;
    AthenaImage *monkey_tex;
    Prop props[PROP_COUNT];
    int next_ball;

    // Camera
    float cam_yaw, cam_pitch, cam_dist;
    float cam_target[3];

    // Shadows
    AthenaImage *shadow_rt;
    AthenaImage *shadow_identity_lut, *shadow_mask_lut;
    uint64_t shadow_copy_blend, shadow_silhouette_blend, shadow_scene_blend, shadow_darken_blend;
    Color shadow_clear, shadow_coverage_clear, shadow_stencil_clear;
    float shadow_light[3];
    float shadow_transform[16];
    AthenaShadowProjector *player_shadow;
    AthenaShadowProjector *prop_shadows[SHADOW_PROP_LIMIT];
    ShadowCandidate shadow_candidates[PROP_COUNT];
    AthenaCamera3dState shadow_view;
    int shadow_mode, shadow_draws, shadow_overlap_groups;
    ShadowCaster shadow_casters[SHADOW_PROP_LIMIT + 1];
    int shadow_caster_count;
    ShadowGroup shadow_groups[SHADOW_PROP_LIMIT + 1];
    int shadow_group_count;

    // Post-fx
    int fog_preset;
    AthenaImage *dof_mask_lut, *depth_heatmap_lut, *depth_normalized_lut;

    // Benchmark
    int bench_phase;
    bool bench_postfx;
    double bench_sum[7];
    double bench_fast_clip;
    int bench_samples, bench_frames;
    AthenaTimer *bench_timer;
} Demo;

static void set_status(Demo *d, const char *text, int frames) {
    snprintf(d->status_text, sizeof(d->status_text), "%s", text);
    d->status_timer = frames;
}

static athena_object_data *native(AthenaRenderObject *ro) { return athena_render_object_native(ro); }

static void object_set(AthenaRenderObject *ro, float *field, float x, float y, float z) {
    field[0] = x; field[1] = y; field[2] = z;
    update_object_space(native(ro));
}
#define SET_POSITION(ro, x, y, z) object_set((ro), native(ro)->position, (x), (y), (z))
#define SET_ROTATION(ro, x, y, z) object_set((ro), native(ro)->rotation, (x), (y), (z))
#define SET_SCALE(ro, x, y, z)    object_set((ro), native(ro)->scale, (x), (y), (z))

// obj.setPhysics(body): the object follows the body's transform.
static void object_set_physics(AthenaRenderObject *ro, AthenaPhysicsBody *body) {
    athena_object_data *obj = native(ro);
    obj->physics = body;
    obj->update_physics = updateBodyPosRot;
    update_object_space(obj);
}

static void play_anim(Demo *d, const char *name, bool loop) {
    athena_animation *anim = athena_anim3d_find(d->anims, name);
    athena_render_object_play_anim_blend(d->skin_object, anim, loop, 0.15f);
    d->current_anim = name;
}

static bool has_anim(Demo *d, const char *name) { return athena_anim3d_find(d->anims, name) != NULL; }

// ---------------------------------------------------------------------------
// Particles
// ---------------------------------------------------------------------------

static int fx_emitter(Demo *d, const float frame[4], float life_min, float life_max, float size_min, float size_max,
                      float scale_end, const float vmin[3], const float vmax[3], const float accel[3], float drag,
                      const float spread[3], const float color[4], const float color_end[4], float fade_in) {
    AthenaParticleEffect e;
    particles_effect_defaults(&e);
    memcpy(e.frame, frame, sizeof(e.frame));
    e.life_min = life_min; e.life_max = life_max;
    e.size_min = size_min; e.size_max = size_max;
    e.scale_end = scale_end;
    memcpy(e.velocity_min, vmin, sizeof(e.velocity_min));
    memcpy(e.velocity_max, vmax, sizeof(e.velocity_max));
    memcpy(e.acceleration, accel, sizeof(e.acceleration));
    e.drag = drag;
    if (spread) memcpy(e.spread, spread, sizeof(e.spread));
    memcpy(e.color, color, sizeof(e.color));
    memcpy(e.color_end, color_end, sizeof(e.color_end));
    if (fade_in > 0.0f) e.fade_in = fade_in;
    return particles_add_emitter(d->fx, &e);
}

static void fx_init(Demo *d) {
    d->fx_atlas = athena_image_create("fx_atlas.png", true);
    athena_image_set_filter(d->fx_atlas, GS_FILTER_LINEAR);
    d->fx = particles_create(1024, true, true, FX_SEED);
    d->fx_renderer = athena_particles_renderer_create(d->fx, d->fx_atlas, GS_ALPHA_BLEND_NORMAL, true);

    static const float dust_frame[4] = { 0, 0, 32, 32 }, chip_frame[4] = { 32, 0, 32, 32 };
    static const float dust_accel[3] = { 0, -0.12f, 0 }, gravity[3] = { 0, -9, 0 };
    d->fx_dust = fx_emitter(d, dust_frame, 0.45f, 0.9f, 0.14f, 0.28f, 3.0f,
        (const float[3]){ -0.4f, 0.15f, -0.4f }, (const float[3]){ 0.4f, 0.65f, 0.4f }, dust_accel, 1.8f,
        (const float[3]){ 0.08f, 0.02f, 0.08f },
        (const float[4]){ 128, 118, 100, 45 }, (const float[4]){ 128, 118, 100, 0 }, 0.08f);
    d->fx_wood = fx_emitter(d, chip_frame, 0.35f, 0.8f, 0.025f, 0.065f, 0.65f,
        (const float[3]){ -1.8f, 1, -1.8f }, (const float[3]){ 1.8f, 3, 1.8f }, gravity, 0.0f, NULL,
        (const float[4]){ 128, 90, 55, 128 }, (const float[4]){ 128, 90, 55, 0 }, 0.0f);
    d->fx_stone = fx_emitter(d, chip_frame, 0.3f, 0.65f, 0.02f, 0.055f, 0.65f,
        (const float[3]){ -1.5f, 0.6f, -1.5f }, (const float[3]){ 1.5f, 2, 1.5f }, gravity, 0.0f, NULL,
        (const float[4]){ 120, 118, 112, 128 }, (const float[4]){ 120, 118, 112, 0 }, 0.0f);
    d->fx_level = 1;
    d->fx_next_step = 0.0f;
}

static void fx_reset(Demo *d, int level) {
    d->fx_level = level;
    d->fx_next_step = 0.0f;
    particles_clear(d->fx, true, FX_SEED);
}

// material: 0 = dust, 1 = wood fragments + dust, 2 = stone grit + dust.
static void fx_burst(Demo *d, float x, float y, float z, int count, int material) {
    const int budget = FX_LEVELS[d->fx_level];
    const int active = (int)particles_stats(d->fx).active;
    count = mini(budget - active, (int)ceilf(count * budget / 128.0f));
    if (count <= 0)
        return;
    const float p[3] = { x, y, z };
    const int dust = material == 0 ? count : (int)ceilf(count / 3.0f);
    particles_burst(d->fx, d->fx_dust, dust, p);
    if (count > dust)
        particles_burst(d->fx, material == 1 ? d->fx_wood : d->fx_stone, count - dust, p);
}

static void fx_render(Demo *d) {
    // Same fixed simulation step as the rigid-body world.
    particles_update(d->fx, 0.016f);
    athena_particles_draw(d->fx_renderer);
}

static void fx_prop_impacts(Demo *d) {
    for (int i = 0; i < PROP_COUNT; i++) {
        Prop *prop = &d->props[i];
        if (d->fx_level == 0) {
            prop->fx_previous_vy = 0;
            continue;
        }
        prop->fx_impact_cooldown -= 0.016f;
        float v[3];
        athena_physics_body_get_linear_vel(prop->body, &v[0], &v[1], &v[2]);
        // A falling prop abruptly loses downward speed at a real collision.
        // Resting contacts, gravity and the upward throw impulse do not emit.
        if (prop->fx_previous_vy < -2.2f && v[1] - prop->fx_previous_vy > 1.8f && prop->fx_impact_cooldown <= 0) {
            float p[3];
            athena_physics_body_get_position(prop->body, &p[0], &p[1], &p[2]);
            const bool crate = i < CRATE_COUNT;
            fx_burst(d, p[0], p[1] - (crate ? CRATE_HALF : BALL_RADIUS) + 0.025f, p[2], 16, crate ? 1 : 2);
            prop->fx_impact_cooldown = 0.2f;
        }
        prop->fx_previous_vy = v[1];
    }
}

// ---------------------------------------------------------------------------
// Physics
// ---------------------------------------------------------------------------

static void add_static_box(Demo *d, float x0, float y0, float z0, float x1, float y1, float z1) {
    AthenaPhysicsGeom *g = athena_physics_geom_create_box(d->space, x1 - x0, y1 - y0, z1 - z0);
    athena_physics_geom_set_position(g, 0.5f * (x0 + x1), 0.5f * (y0 + y1), 0.5f * (z0 + z1));
    AthenaPhysicsGeom *receiver = athena_physics_geom_create_box(d->shadow_space, x1 - x0, y1 - y0, z1 - z0);
    athena_physics_geom_set_position(receiver, 0.5f * (x0 + x1), 0.5f * (y0 + y1), 0.5f * (z0 + z1));
}

// Watertight courtyard collision from native boxes (Crytek/Khronos Sponza
// Palace Atrium + walkable north terrace).
static void build_static_world(Demo *d) {
    // 1. Courtyard floor slab. Top face is Y = 0, matching the visual floor.
    // A finite box (not an infinite plane) so the broadphase AABB stays in range.
    add_static_box(d, -8.53f, -0.80f, -19.10f, 8.53f, 0.0f, 19.10f);

    // 2. Outer Sponza Palace Boundary Walls (X = ±8.53m, Z = ±19.10m)
    add_static_box(d, -9.53f, 0.0f, -20.0f, -8.53f, 12.0f,  20.0f); // West Outer Wall
    add_static_box(d,  8.53f, 0.0f, -20.0f,  9.53f, 12.0f,  20.0f); // East Outer Wall
    add_static_box(d, -9.53f, 0.0f, -20.1f,  9.53f, 12.0f, -19.1f); // North Outer Wall
    add_static_box(d, -9.53f, 0.0f,  19.1f,  9.53f, 12.0f,  20.1f); // South Outer Wall

    // 3. 8 Ground-Floor Colonnade Columns + 4 Corner Piers (exact Khronos Sponza coordinates)
    static const float colZ[] = { -7.775f, -2.534f, 2.610f, 7.775f };
    for (int i = 0; i < 4; i++) {
        const float cz = colZ[i];
        add_static_box(d, -3.735f, 0.0f, cz - 0.36f, -3.015f, 3.20f, cz + 0.36f); // West Column
        add_static_box(d,  3.043f, 0.0f, cz - 0.36f,  3.765f, 3.20f, cz + 0.36f); // East Column
    }
    static const float pierZ[] = { -13.139f, 13.114f };
    for (int i = 0; i < 2; i++) {
        const float pz = pierZ[i];
        add_static_box(d, -3.763f, 0.0f, pz - 0.57f, -2.618f, 3.20f, pz + 0.57f); // West Corner Pier
        add_static_box(d,  2.619f, 0.0f, pz - 0.57f,  3.764f, 3.20f, pz + 0.57f); // East Corner Pier
    }

    // 4. 8 Courtyard Stone Planters
    static const float planterPos[][2] = {
        { -2.62f, -12.58f }, { 2.68f, -12.58f },
        { -2.64f,  -2.52f }, { 2.63f,  -2.52f },
        { -2.65f,   2.58f }, { 2.63f,   2.58f },
        { -2.65f,  12.58f }, { 2.63f,  12.58f },
    };
    for (int i = 0; i < 8; i++) {
        const float px = planterPos[i][0], pz = planterPos[i][1];
        add_static_box(d, px - 0.42f, 0.0f, pz - 0.42f, px + 0.42f, 0.92f, pz + 0.42f);
    }

    // 5. North Terrace Multi-Level Stone Course (Steps + Platforms + Trophy Pedestal)
    // Central Approach Staircase (5 stone steps from Z = -4.20 to -7.80, Y = 0.0 to 1.05m)
    for (int s = 0; s < 5; s++) {
        const float zFront = -4.20f - s * (3.60f / 5.0f);
        const float yTop   = (s + 1) * (1.05f / 5.0f);
        add_static_box(d, -1.60f, 0.0f, -7.80f, 1.60f, yTop, zFront);
    }
    // Tier-1 Lower North Terrace Platform (Y = 1.05m)
    add_static_box(d, -3.10f, 0.0f, -10.20f, 3.10f, 1.05f, -7.80f);

    // Twin Side Staircases up to Tier-2 (4 stone steps from Z = -9.60 to -12.10, Y = 1.05 to 1.95m)
    for (int s = 0; s < 4; s++) {
        const float zFront = -9.60f - s * (2.50f / 4.0f);
        const float yTop   = 1.05f + (s + 1) * (0.90f / 4.0f);
        add_static_box(d, -2.95f, 0.0f, -12.10f, -1.65f, yTop, zFront);
        add_static_box(d,  1.65f, 0.0f, -12.10f,  2.95f, yTop, zFront);
    }
    // Tier-2 Upper North Balcony (Y = 1.95m)
    add_static_box(d, -3.10f, 0.0f, -13.60f, 3.10f, 1.95f, -12.10f);

    // Tier-3 Central Trophy Pedestal (Y = 2.55m)
    add_static_box(d, -1.15f, 1.95f, -13.55f, 1.15f, 2.55f, -12.25f);
}

// world.stepWithContacts(space, jgroup, dt): one contact joint per contact
// point, the same friction/bounce/softness as the script binding.
typedef struct { AthenaPhysicsWorld *world; AthenaPhysicsJointGroup *group; } ContactData;

static void near_callback(void *data, AthenaPhysicsGeom *o1, AthenaPhysicsGeom *o2) {
    ContactData *cd = (ContactData *)data;
    AthenaPhysicsBody *b1 = athena_physics_geom_get_body(o1);
    AthenaPhysicsBody *b2 = athena_physics_geom_get_body(o2);
    if (!b1 && !b2)
        return;
    if (b1 && b2 && athena_physics_bodies_connected_excluding(b1, b2, ATH_JOINT_CONTACT))
        return;

    AthenaContact contacts[ATH_PHYS_MAX_CONTACTS] __attribute__((aligned(16)));
    int n = athena_physics_collide(o1, o2, ATH_PHYS_MAX_CONTACTS, contacts);
    for (int i = 0; i < n; i++) {
        contacts[i].mu = 0.5f;
        contacts[i].bounce = 0.1f;
        contacts[i].soft_cfm = 0.01f;
        AthenaPhysicsJoint *c = athena_physics_joint_create_contact(cd->world, cd->group, &contacts[i]);
        athena_physics_joint_attach(c, b1, b2);
    }
}

static void world_step(Demo *d, float dt) {
    ContactData cd = { d->world, d->jgroup };
    athena_physics_space_collide(d->space, &cd, near_callback);
    athena_physics_world_step(d->world, dt);
    athena_physics_joint_group_empty(d->jgroup);
}

static void reset_body(AthenaPhysicsBody *body, const float *p) {
    static const float identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    athena_physics_body_set_position(body, p[0], p[1], p[2]);
    athena_physics_body_set_linear_vel(body, 0, 0, 0);
    athena_physics_body_set_angular_vel(body, 0, 0, 0);
    athena_physics_body_set_rotation(body, identity);
}

static void reset_sandbox(Demo *d) {
    reset_body(d->player_body, PLAYER_START);
    d->player_yaw = PI;
    d->jump_windup = 0;
    d->ground_grace = 6;
    d->kick_phase = 0;
    for (int i = 0; i < PROP_COUNT; i++) {
        d->props[i].fx_previous_vy = 0;
        d->props[i].fx_impact_cooldown = 0;
        reset_body(d->props[i].body, d->props[i].spawn);
    }
    set_status(d, "SANDBOX RESET!", 90);
}

static void create_prop(Demo *d, Prop *prop, AthenaRenderData *data, const float *sp, bool crate) {
    prop->data = data;
    prop->spawn = sp;
    prop->obj = athena_render_object_create(data);
    if (crate) {
        SET_SCALE(prop->obj, CRATE_HALF, CRATE_HALF, CRATE_HALF);
        prop->geom = athena_physics_geom_create_box(d->space, CRATE_SIZE, CRATE_SIZE, CRATE_SIZE);
        prop->body = athena_physics_body_create(d->world);
        athena_physics_body_set_mass_box(prop->body, 0.8f, CRATE_SIZE, CRATE_SIZE, CRATE_SIZE);
    } else {
        SET_SCALE(prop->obj, BALL_RADIUS, BALL_RADIUS, BALL_RADIUS);
        prop->geom = athena_physics_geom_create_sphere(d->space, BALL_RADIUS);
        prop->body = athena_physics_body_create(d->world);
        athena_physics_body_set_mass_sphere(prop->body, 0.7f, BALL_RADIUS);
    }
    SET_POSITION(prop->obj, sp[0], sp[1], sp[2]);
    athena_physics_geom_set_body(prop->geom, prop->body);
    athena_physics_body_set_position(prop->body, sp[0], sp[1], sp[2]);
    object_set_physics(prop->obj, prop->body);
}

// ---------------------------------------------------------------------------
// Dynamic silhouette shadows (L3: exact -> fast overlaps -> off)
// One 128x128 target is consumed immediately after each light-camera draw.
// Framebuffer red is reused for coverage and the unused Z24 byte to save the
// scene's red; coverage and the final darkening both stay at framebuffer
// resolution. Only the player and the six nearest props cast shadows.
// ---------------------------------------------------------------------------

static AthenaShadowProjector *create_shadow_projector(Demo *d, float size, int grid) {
    AthenaShadowProjector *p = athena_shadow_projector_create(d->shadow_rt->tex);
    athena_shadow_projector_set_light_dir(p, d->shadow_light[0], d->shadow_light[1], d->shadow_light[2]);
    athena_shadow_projector_set_light_offset(p, 0);
    // The projector applies -lightDir*bias: negative lifts toward the sun.
    athena_shadow_projector_set_bias(p, -0.018f);
    athena_shadow_projector_set_slope_limit(p, 0.45f);
    // Opacity rides in the vertex alpha, so the texture alpha a draw reads is
    // already the darkening both blends below need.
    athena_shadow_projector_set_color(p, 1, 1, 1, SHADOW_OPACITY);
    athena_shadow_projector_set_size(p, size, size);
    // Color is baked when geometry is built, so set it before set_grid().
    athena_shadow_projector_set_grid(p, grid, grid);
    athena_shadow_projector_enable_raycast(p, d->shadow_space, SHADOW_RAY_LENGTH, 1);
    return p;
}

static void shadows_init(Demo *d, const float sun[3]) {
    AthenaImage *rt = athena_image_create_empty(true);
    rt->tex->PageAligned = 1; // renderable
    rt->tex->PSM = GS_PSM_CT32;
    rt->tex->Width = rt->tex->Height = 128;
    rt->width = rt->height = 128;
    rt->endx = rt->endy = 128;
    rt->tex->Filter = GS_FILTER_LINEAR;
    if (!athena_image_lock(rt))
        printf("physics demo: unable to reserve the shadow render target\n");
    d->shadow_rt = rt;

    // Constant RGB makes additive coverage linear in filtered texture alpha.
    d->shadow_clear = RGBA(128, 128, 128, 0);
    d->shadow_coverage_clear = RGBA(127, 127, 127, 128);
    d->shadow_stencil_clear = RGBA(0, 0, 0, 0);
    d->shadow_copy_blend = athena_screen_alpha_equation(SRC_RGB, ZERO_RGB, ALPHA_FIX, ZERO_RGB, 128);
    d->shadow_silhouette_blend = athena_screen_alpha_equation(ZERO_RGB, ZERO_RGB, ALPHA_FIX, DST_RGB, 0);
    d->shadow_scene_blend = athena_screen_alpha_equation(SRC_RGB, ZERO_RGB, DST_ALPHA, ZERO_RGB, 0);
    d->shadow_darken_blend = athena_screen_alpha_equation(DST_RGB, ZERO_RGB, DST_ALPHA, ZERO_RGB, 0);

    uint32_t lut[256];
    for (int v = 0; v < 256; v++)
        lut[v] = RGBA(v, v, v, v);
    d->shadow_identity_lut = athena_screen_create_shuffle_lut(lut);
    // Each shadow adds 128 * SHADOW_OPACITY * coverage to red (127 base); the
    // sum is capped at full opacity, so overlaps darken once. Convert it to
    // light transmittance in alpha without resampling any pixels.
    for (int v = 0; v < 256; v++) {
        float darken = minf(maxf(0.0f, v - 127.0f), 128.0f * SHADOW_OPACITY);
        lut[v] = RGBA(0, 0, 0, (int)lroundf(128.0f - darken));
    }
    d->shadow_mask_lut = athena_screen_create_shuffle_lut(lut);

    const float len = sqrtf(sun[0] * sun[0] + sun[1] * sun[1] + sun[2] * sun[2]);
    const float lx = sun[0] / len, ly = sun[1] / len, lz = sun[2] / len;
    d->shadow_light[0] = lx; d->shadow_light[1] = ly; d->shadow_light[2] = lz;
    const float horiz = sqrtf(lx * lx + lz * lz);
    // The projector's X/Z axes match the light camera's right/down axes.
    // Compensate the existing projector's -lightDir.y placement convention.
    const float t[16] = {
        lz / horiz, 0, -lx / horiz, 0,
        lx, ly, lz, 0,
        lx * ly / horiz, -horiz, lz * ly / horiz, 0,
        0, 0, 0, 1,
    };
    memcpy(d->shadow_transform, t, sizeof(t));

    d->player_shadow = create_shadow_projector(d, 3.2f, 6);
    // One projector per shadowed prop: screen_bounds() builds a projector's
    // grid and keeps it for its next render(), so each caster needs its own.
    for (int i = 0; i < SHADOW_PROP_LIMIT; i++)
        d->prop_shadows[i] = create_shadow_projector(d, 1.65f, 4);
    for (int i = 0; i < PROP_COUNT; i++)
        d->shadow_candidates[i].prop = i;
    athena_camera3d_save(&d->shadow_view);
    d->shadow_mode = SHADOW_EXACT;

    // Reserve context 2 for silhouette generation; its Z writes stay masked.
    athena_screen_switch_context();
    athena_screen_set_buffer(DRAW_BUFFER, d->shadow_rt, 0);
    athena_screen_set_buffer(DEPTH_BUFFER, d->zbuf, 1);
    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
    athena_screen_set_param(ALPHA_TEST_ENABLE, false);
    athena_screen_set_param(SCISSOR_BOUNDS, GS_SETREG_SCISSOR_1(0, 127, 0, 127));
    // Overwrite RGB and alpha, including a transparent clear, instead of
    // blending new silhouettes into the previous caster's target contents.
    athena_screen_set_param(ALPHA_BLEND_EQUATION, d->shadow_copy_blend);
    athena_screen_switch_context();
    athena_screen_set_buffer(DEPTH_BUFFER, d->zbuf, 0);
}

// Places a caster's projector under it and queues it when its shadow is on screen.
static void place_caster_shadow(Demo *d, AthenaRenderObject *obj, AthenaRenderData *data,
                                AthenaShadowProjector *projector, float size, float x, float y, float z) {
    const float *l = d->shadow_light;
    // Start just above the whole caster, including the grid's lower corners.
    const float ray_offset = (SHADOW_RAY_LENGTH - size) * 0.5f;
    d->shadow_transform[12] = x - l[0] * ray_offset;
    d->shadow_transform[13] = y - l[1] * ray_offset + l[1];
    d->shadow_transform[14] = z - l[2] * ray_offset;
    athena_shadow_projector_set_transform(projector, d->shadow_transform);
    int b[4];
    if (!athena_shadow_projector_screen_bounds(projector, b))
        return;
    ShadowCaster *c = &d->shadow_casters[d->shadow_caster_count++];
    c->obj = obj; c->data = data; c->projector = projector;
    c->size = size; c->x = x; c->y = y; c->z = z;
    c->x0 = b[0]; c->y0 = b[1]; c->x1 = b[2]; c->y1 = b[3];
}

// Casters whose bounds intersect, directly or through another caster. Only
// shadows that can overlap need more than one pass over the scene.
static void group_shadow_casters(Demo *d) {
    for (int i = 0; i < d->shadow_caster_count; i++)
        d->shadow_casters[i].group = -1;
    d->shadow_group_count = 0;
    for (int i = 0; i < d->shadow_caster_count; i++) {
        if (d->shadow_casters[i].group >= 0)
            continue;
        ShadowGroup *g = &d->shadow_groups[d->shadow_group_count];
        d->shadow_casters[i].group = d->shadow_group_count++;
        g->members[0] = &d->shadow_casters[i];
        g->count = 1;
        // Members join while the loop runs, so their neighbours join too.
        for (int m = 0; m < g->count; m++) {
            ShadowCaster *a = g->members[m];
            for (int j = i + 1; j < d->shadow_caster_count; j++) {
                ShadowCaster *b = &d->shadow_casters[j];
                if (b->group < 0 && a->x0 < b->x1 && b->x0 < a->x1 && a->y0 < b->y1 && b->y0 < a->y1) {
                    b->group = a->group;
                    g->members[g->count++] = b;
                }
            }
        }
        g->x0 = g->members[0]->x0; g->y0 = g->members[0]->y0;
        g->x1 = g->members[0]->x1; g->y1 = g->members[0]->y1;
        for (int m = 1; m < g->count; m++) {
            const ShadowCaster *c = g->members[m];
            g->x0 = mini(g->x0, c->x0); g->y0 = mini(g->y0, c->y0);
            g->x1 = maxi(g->x1, c->x1); g->y1 = maxi(g->y1, c->y1);
        }
    }
}

static void render_caster_shadow(Demo *d, const ShadowCaster *c) {
    const float *l = d->shadow_light;
    athena_screen_switch_context();
    athena_draw_rect(0, 0, 128, 128, d->shadow_clear);
    // Preserve the constant white RGB; the mesh only supplies silhouette alpha.
    athena_screen_set_param(ALPHA_BLEND_EQUATION, d->shadow_silhouette_blend);
    // A distant narrow-FOV light camera approximates directional projection.
    athena_render_module_set_view(2.0f * atanf(c->size / (2.0f * SHADOW_CAMERA_DISTANCE)) * 180.0f / PI, 1, 64, 128, 128);
    athena_camera3d_position(c->x + l[0] * SHADOW_CAMERA_DISTANCE,
        c->y + l[1] * SHADOW_CAMERA_DISTANCE, c->z + l[2] * SHADOW_CAMERA_DISTANCE);
    athena_camera3d_target(c->x, c->y, c->z);
    athena_camera3d_update();
    // Reuse the scene object/mesh for the light pass: untextured, flat, unlit.
    athena_render_data *m = athena_render_data_native(c->data);
    const int texture_mapping = m->attributes.texture_mapping;
    const int shade_model = m->attributes.shade_model;
    const eRenderPipelines pipeline = m->pipeline;
    m->attributes.texture_mapping = 0;
    m->attributes.shade_model = 0;
    m->pipeline = PL_NO_LIGHTS;
    athena_render_object_draw(c->obj);
    m->attributes.texture_mapping = texture_mapping;
    m->attributes.shade_model = shade_model;
    m->pipeline = pipeline;
    athena_screen_set_param(ALPHA_BLEND_EQUATION, d->shadow_copy_blend);
    athena_screen_switch_context();
    athena_render_module_set_view(60, 2, 250, 0, 0);
    athena_camera3d_restore(&d->shadow_view);
    athena_camera3d_update();
    // Drawn from the grid place_caster_shadow() built.
    athena_shadow_projector_render(c->projector);
    d->shadow_draws++;
}

static void alpha_test_greater_zero(void) {
    athena_screen_set_param(ALPHA_TEST_ENABLE, true);
    athena_screen_set_param(ALPHA_TEST_METHOD, ALPHA_GREATER);
    athena_screen_set_param(ALPHA_TEST_REF, 0);
    athena_screen_set_param(ALPHA_TEST_FAIL, ALPHA_FAIL_NO_UPDATE);
}

// Multiplies the scene straight from each caster's projector: all a shadow
// alone on screen needs. With stencil (FAST overlaps), alpha bit 7 marks every
// pixel already darkened so an overlap darkens once.
static void render_shadow_group_direct(Demo *d, const ShadowGroup *g, bool stencil, uint64_t depth_test, uint64_t depth_method) {
    athena_screen_set_param(ALPHA_TEST_ENABLE, false);
    athena_screen_set_param(DST_ALPHA_TEST_ENABLE, false);
    if (stencil) {
        athena_screen_set_param(DEPTH_TEST_ENABLE, false);
        athena_screen_set_param(ALPHA_BLEND_EQUATION, d->shadow_copy_blend);
        athena_screen_set_buffer(DRAW_BUFFER, d->fbuf, 0x00FFFFFF);
        athena_draw_rect(g->x0, g->y0, g->x1 - g->x0, g->y1 - g->y0, d->shadow_stencil_clear);
        athena_screen_set_buffer(DRAW_BUFFER, d->fbuf, 0);
        athena_screen_set_param(DST_ALPHA_TEST_ENABLE, true);
        athena_screen_set_param(DST_ALPHA_TEST_METHOD, DEST_ALPHA_ZERO);
        athena_screen_set_param(ALPHA_CORRECTION, true);
    } else {
        // RGB only: the scene's alpha stays as it was.
        athena_screen_set_buffer(DRAW_BUFFER, d->fbuf, 0xFF000000);
    }
    athena_screen_set_param(DEPTH_TEST_ENABLE, depth_test);
    athena_screen_set_param(DEPTH_TEST_METHOD, depth_method);
    alpha_test_greater_zero();
    for (int i = 0; i < g->count; i++) {
        athena_shadow_projector_set_blend(g->members[i]->projector, SHADOW_BLEND_MULTIPLY);
        render_caster_shadow(d, g->members[i]);
    }
    if (stencil) {
        athena_screen_set_param(ALPHA_CORRECTION, false);
        athena_screen_set_param(DST_ALPHA_TEST_ENABLE, false);
    }
}

static void shuffle(GSSURFACE *dst, GSSURFACE *src, AthenaImage *lut, uint32_t in, uint32_t out,
                    bool blend, uint64_t equation, const int region[4]) {
    gs_channel_shuffle_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.in_channel = in;
    cfg.out_mask = out;
    cfg.alpha_blend = blend;
    cfg.blend_equation = blend ? equation : GS_ALPHA_BLEND_NORMAL;
    cfg.alpha_test = false;
    cfg.z_test_method = DEPTH_ALWAYS;
    if (region) {
        cfg.x = region[0]; cfg.y = region[1]; cfg.width = region[2]; cfg.height = region[3];
    }
    gs_channel_shuffle_ex(dst, src, lut->tex, &cfg);
}

// Overlapping shadows, resolved exactly: red becomes a coverage buffer that
// every shadow of the group adds to, then the scene is darkened once by it.
static void render_shadow_group_coverage(Demo *d, const ShadowGroup *g, uint64_t depth_test, uint64_t depth_method) {
    // Whole GS pages (64x32): the channel shuffles work page by page, and every
    // pass has to cover exactly the same pixels.
    int region[4];
    region[0] = g->x0 & ~63;
    region[1] = g->y0 & ~31;
    region[2] = mini(d->width, (g->x1 + 63) & ~63) - region[0];
    region[3] = mini(d->height, (g->y1 + 31) & ~31) - region[1];

    // A CT32 view of Z24: its unused alpha byte, with depth's low 24 bits intact.
    GSSURFACE zraw = *d->zbuf->tex;
    zraw.PSM = GS_PSM_CT32;

    // The scene has already been shaded once. Save red in the Z24 alpha byte.
    athena_screen_set_param(DST_ALPHA_TEST_ENABLE, false);
    athena_screen_set_param(ALPHA_TEST_ENABLE, false);
    shuffle(&zraw, d->fbuf->tex, d->shadow_identity_lut, CHANNEL_SHUFFLE_RED, CHANNEL_SHUFFLE_ALPHA, false, 0, region);
    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
    athena_screen_set_param(ALPHA_BLEND_EQUATION, d->shadow_copy_blend);
    athena_screen_set_buffer(DRAW_BUFFER, d->fbuf, 0xFFFFFF00);
    athena_draw_rect(region[0], region[1], region[2], region[3], d->shadow_coverage_clear);
    athena_screen_set_param(DEPTH_TEST_ENABLE, depth_test);
    athena_screen_set_param(DEPTH_TEST_METHOD, depth_method);

    // Red += filtered coverage, clamped to 255. Partial edges remain
    // continuous and order independent.
    alpha_test_greater_zero();
    for (int i = 0; i < g->count; i++) {
        athena_shadow_projector_set_blend(g->members[i]->projector, SHADOW_BLEND_ADD);
        render_caster_shadow(d, g->members[i]);
    }
    shuffle(main_screen_buffer[DRAW_BUFFER], main_screen_buffer[DRAW_BUFFER], d->shadow_mask_lut,
            CHANNEL_SHUFFLE_RED, CHANNEL_SHUFFLE_ALPHA, false, 0, region);
    // Restore saved red multiplied by transmittance. Read exactly the same
    // physical layout used above; a normal Z read would permute the blocks.
    shuffle(main_screen_buffer[DRAW_BUFFER], &zraw, d->shadow_identity_lut,
            CHANNEL_SHUFFLE_ALPHA, CHANNEL_SHUFFLE_RED, true, d->shadow_scene_blend, region);
    athena_screen_set_param(ALPHA_TEST_ENABLE, false);
    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
    athena_screen_set_buffer(DRAW_BUFFER, d->fbuf, 0xFF0000FF);
    athena_screen_set_param(ALPHA_BLEND_EQUATION, d->shadow_darken_blend);
    athena_draw_rect(region[0], region[1], region[2], region[3], d->shadow_coverage_clear);
}

static int candidate_order(const void *a, const void *b) {
    const float da = ((const ShadowCandidate *)a)->distance, db = ((const ShadowCandidate *)b)->distance;
    return da < db ? -1 : da > db ? 1 : 0;
}

static void render_dynamic_shadows(Demo *d, float px, float py, float pz, float cam_x, float cam_y, float cam_z) {
    d->shadow_draws = 0;
    d->shadow_overlap_groups = 0;
    if (d->shadow_mode == SHADOW_OFF)
        return;
    // The follow camera has the standard world-up.
    d->shadow_view.position[0] = cam_x; d->shadow_view.position[1] = cam_y; d->shadow_view.position[2] = cam_z;
    d->shadow_view.target[0] = d->cam_target[0];
    d->shadow_view.target[1] = d->cam_target[1];
    d->shadow_view.target[2] = d->cam_target[2];

    d->shadow_caster_count = 0;
    place_caster_shadow(d, d->skin_object, d->skin, d->player_shadow, 3.2f, px, py - PLAYER_RADIUS + 0.9f, pz);
    for (int i = 0; i < PROP_COUNT; i++) {
        ShadowCandidate *c = &d->shadow_candidates[i];
        athena_physics_body_get_position(d->props[c->prop].body, &c->x, &c->y, &c->z);
        const float dx = c->x - px, dz = c->z - pz;
        c->distance = dx * dx + dz * dz;
    }
    qsort(d->shadow_candidates, PROP_COUNT, sizeof(ShadowCandidate), candidate_order);
    for (int i = 0; i < SHADOW_PROP_LIMIT; i++) {
        const ShadowCandidate *c = &d->shadow_candidates[i];
        if (c->distance > SHADOW_DISTANCE * SHADOW_DISTANCE)
            break;
        const Prop *prop = &d->props[c->prop];
        place_caster_shadow(d, prop->obj, prop->data, d->prop_shadows[i], 1.65f, c->x, c->y, c->z);
    }
    if (d->shadow_caster_count == 0)
        return;
    group_shadow_casters(d);

    const uint64_t alpha_test = athena_screen_get_param(ALPHA_TEST_ENABLE);
    const uint64_t alpha_method = athena_screen_get_param(ALPHA_TEST_METHOD);
    const uint64_t alpha_ref = athena_screen_get_param(ALPHA_TEST_REF);
    const uint64_t alpha_fail = athena_screen_get_param(ALPHA_TEST_FAIL);
    const uint64_t destination_test = athena_screen_get_param(DST_ALPHA_TEST_ENABLE);
    const uint64_t destination_method = athena_screen_get_param(DST_ALPHA_TEST_METHOD);
    const uint64_t depth_test = athena_screen_get_param(DEPTH_TEST_ENABLE);
    const uint64_t depth_method = athena_screen_get_param(DEPTH_TEST_METHOD);
    const uint64_t blend = athena_screen_get_param(ALPHA_BLEND_EQUATION);
    const uint64_t clamp = athena_screen_get_param(COLOR_CLAMP_MODE);
    const uint64_t pixel_blend = athena_screen_get_param(PIXEL_ALPHA_BLEND_ENABLE);
    athena_screen_set_buffer(DEPTH_BUFFER, d->zbuf, 1);

    athena_screen_set_param(COLOR_CLAMP_MODE, 1);
    athena_screen_set_param(PIXEL_ALPHA_BLEND_ENABLE, false);
    for (int i = 0; i < d->shadow_group_count; i++) {
        const ShadowGroup *g = &d->shadow_groups[i];
        if (g->count == 1) {
            render_shadow_group_direct(d, g, false, depth_test, depth_method);
            continue;
        }
        d->shadow_overlap_groups++;
        if (d->shadow_mode == SHADOW_FAST)
            render_shadow_group_direct(d, g, true, depth_test, depth_method);
        else
            render_shadow_group_coverage(d, g, depth_test, depth_method);
    }

    athena_screen_set_param(ALPHA_CORRECTION, false);
    athena_screen_set_param(DST_ALPHA_TEST_ENABLE, destination_test);
    athena_screen_set_param(DST_ALPHA_TEST_METHOD, destination_method);
    athena_screen_set_param(ALPHA_TEST_ENABLE, alpha_test);
    athena_screen_set_param(ALPHA_TEST_METHOD, alpha_method);
    athena_screen_set_param(ALPHA_TEST_REF, alpha_ref);
    athena_screen_set_param(ALPHA_TEST_FAIL, alpha_fail);
    athena_screen_set_param(DEPTH_TEST_ENABLE, depth_test);
    athena_screen_set_param(DEPTH_TEST_METHOD, depth_method);
    athena_screen_set_param(ALPHA_BLEND_EQUATION, blend);
    athena_screen_set_param(COLOR_CLAMP_MODE, clamp);
    athena_screen_set_param(PIXEL_ALPHA_BLEND_ENABLE, pixel_blend);
    athena_screen_set_buffer(DRAW_BUFFER, d->fbuf, 0);
    athena_screen_set_buffer(DEPTH_BUFFER, d->zbuf, 0);
}

static void render_opaque_scene(Demo *d) {
    athena_render_object_draw(d->scene_object);
    for (int i = 0; i < PROP_COUNT; i++)
        athena_render_object_draw(d->props[i].obj);
    athena_render_object_draw(d->skin_object);
}

// ---------------------------------------------------------------------------
// Post-fx: depth fog, depth of field, Z-buffer channel shuffles
// ---------------------------------------------------------------------------

// Depth-of-field alpha mask: reconstructs the eye-space distance d (meters)
// from the shifted Z24 MSB byte b. Keeps the foreground and player (~2m..9.5m)
// sharp (alpha 0) and ramps the background (9.5m..26m+) up to alpha 128.
static uint32_t dof_mask_entry(int b) {
    if (b == 0) return RGBA(0, 0, 0, 128); // Sky / far horizon: full blur

    const float byte_scale = 65536.0f / (1 << DOF_SHIFT);
    const float screen_scale_z = 4194304.0f;
    const float near_z = 2.0f, far_z = 250.0f;

    const float z_gs = minf(screen_scale_z * 2.0f, (b + 0.5f) * byte_scale);
    const float z_ndc = (z_gs / screen_scale_z) - 1.0f;
    const float denom = (far_z + near_z) + z_ndc * (far_z - near_z);
    const float dist = denom > 0.0001f ? (2.0f * far_z * near_z) / denom : far_z;

    const float focus_end = 9.5f, blur_full = 26.0f;
    if (dist <= focus_end) return RGBA(0, 0, 0, 0);

    const float t = minf(1.0f, (dist - focus_end) / (blur_full - focus_end));
    const float smooth = t * t * (3.0f - 2.0f * t);
    return RGBA(0, 0, 0, mini(128, (int)floorf(smooth * 128.0f + 0.5f)));
}

static uint32_t depth_lut_entry(int b, float range) {
    if (b == 0) return RGBA(15, 18, 35, 115);
    const float norm = minf(1.0f, b / range);
    const int r = mini(255, (int)floorf(norm * 510.0f));
    const int g = mini(255, maxi(0, (int)floorf((norm - 0.25f) * 360.0f)));
    const int bl = mini(255, maxi(40, (int)floorf((1.0f - norm) * 255.0f)));
    return RGBA(r, g, bl, 118);
}

static void postfx_init(Demo *d) {
    FOG_PRESETS[0].color = RGBA(170, 125, 100, 110);
    FOG_PRESETS[2].color = RGBA(182, 198, 225, 108);
    FOG_PRESETS[3].color = RGBA(195, 200, 208, 124);
    FOG_PRESETS[4].color = RGBA(235, 165, 105, 110);

    uint32_t lut[256];
    for (int b = 0; b < 256; b++) lut[b] = dof_mask_entry(b);
    d->dof_mask_lut = athena_screen_create_shuffle_lut(lut);
    // In Z24, the MSB byte b is in [1..128] (the courtyard spans b = 4..128).
    for (int b = 0; b < 256; b++) lut[b] = depth_lut_entry(b, 64.0f);
    d->depth_heatmap_lut = athena_screen_create_shuffle_lut(lut);
    for (int b = 0; b < 256; b++) lut[b] = depth_lut_entry(b, 160.0f);
    d->depth_normalized_lut = athena_screen_create_shuffle_lut(lut);
}

// Draws img over [0, dst_w) x [0, dst_h) as vertical strips of strip_w pixels
// instead of one screen-sized sprite, so the GS stays within one texture page
// and one frame page column at a time. The texel mapping is the same
// continuous [u0, u1] x [v0, v1] a single sprite would use, evaluated at each
// strip edge, so there are no seams.
static void draw_in_strips(AthenaImage *img, float dst_w, float dst_h, float u0, float v0, float u1, float v1, float strip_w) {
    const float du = (u1 - u0) / dst_w;
    img->starty = v0;
    img->endy = v1;
    img->height = dst_h;
    for (float x = 0; x < dst_w; x += strip_w) {
        const float x1 = minf(x + strip_w, dst_w);
        img->startx = u0 + x * du;
        img->endx = u0 + x1 * du;
        img->width = x1 - x;
        athena_image_draw(img, x, 0);
    }
    img->startx = u0;
    img->endx = u1;
}

static void render_depth_of_field(Demo *d, int strength, bool already_shifted) {
    const float w = d->width, h = d->height;
    const float half_w = d->width >> 1, half_h = d->height >> 1;
    AthenaImage *fbuf = d->fbuf, *zbuf = d->zbuf;

    // 1. Shift Z24 up by DOF_SHIFT bits to expand depth precision around the focal plane
    if (!already_shifted)
        gs_depth_bitshift(DOF_SHIFT);

    // 2. Channel shuffle: Z-buffer blue MSB -> framebuffer alpha only (RGB untouched)
    shuffle(main_screen_buffer[DRAW_BUFFER], main_screen_buffer[DEPTH_BUFFER], d->dof_mask_lut,
            CHANNEL_SHUFFLE_BLUE, CHANNEL_SHUFFLE_ALPHA, false, 0, NULL);

    // 3. Downsample the framebuffer (RGB + DOF alpha) 2x2 into Z-buffer scratch
    // space (half-res), with Z writes masked so drawing into zbuf doesn't write
    // Z = 0 over the blurred RGB+A.
    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
    athena_screen_set_param(ALPHA_TEST_ENABLE, false);
    athena_screen_set_buffer(DEPTH_BUFFER, zbuf, 1);

    zbuf->tex->PSM = GS_PSM_CT32; // treat Z-buffer memory as a 32-bit RGBA surface
    athena_screen_set_buffer(DRAW_BUFFER, zbuf, 0);
    athena_screen_set_param(ALPHA_BLEND_EQUATION, athena_screen_alpha_equation(SRC_RGB, ZERO_RGB, ALPHA_FIX, ZERO_RGB, 0x80));

    fbuf->tex->Filter = GS_FILTER_LINEAR;
    fbuf->color = RGBA(128, 128, 128, 128);
    // 32-pixel strips: each reads one 64-texel source page column (2:1).
    draw_in_strips(fbuf, half_w, half_h, 0.5f, 0.5f, w - 0.5f, h - 0.5f, 32);

    // 4. Upsample the half-res scratch -> framebuffer RGB, weighted by the blurred alpha mask
    athena_screen_set_buffer(DRAW_BUFFER, fbuf, 0xFF000000); // mask alpha, write RGB only
    athena_screen_set_param(ALPHA_BLEND_EQUATION, athena_screen_alpha_equation(SRC_RGB, DST_RGB, SRC_ALPHA, DST_RGB, 0));

    zbuf->tex->Filter = GS_FILTER_LINEAR;
    zbuf->color = RGBA(128, 128, 128, strength);
    // 64-pixel strips: one frame page column each (reading 32 source texels).
    draw_in_strips(zbuf, w, h, 0.5f, 0.5f, half_w - 0.5f, half_h - 0.5f, 64);

    // 5. Restore the Z-buffer format, Z writes, draw buffer mask and default blend
    fbuf->width = w; fbuf->height = h;
    zbuf->width = w; zbuf->height = h;
    zbuf->tex->PSM = GS_PSMZ_24;
    athena_screen_set_buffer(DEPTH_BUFFER, zbuf, 0);
    athena_screen_set_buffer(DRAW_BUFFER, fbuf, 0);
    athena_screen_set_param(ALPHA_BLEND_EQUATION, GS_ALPHA_BLEND_NORMAL);
}

static void depth_fog(const FogPreset *p) {
    gs_depth_fog_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.color = p->color;
    cfg.start = p->start;
    cfg.end = p->end;
    cfg.density = p->density;
    cfg.sky_fog = p->sky_fog;
    cfg.curve = p->curve;
    cfg.shift = p->shift;
    athena_screen_depth_fog(&cfg);
}

static void depth_visualize(AthenaImage *lut) {
    gs_channel_shuffle_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.in_channel = CHANNEL_SHUFFLE_BLUE;
    cfg.out_mask = CHANNEL_SHUFFLE_RGB;
    cfg.alpha_blend = true;
    cfg.blend_equation = GS_ALPHA_BLEND_NORMAL;
    cfg.alpha_test = true;
    cfg.z_test_method = DEPTH_ALWAYS;
    gs_channel_shuffle_ex(main_screen_buffer[DRAW_BUFFER], main_screen_buffer[DEPTH_BUFFER], lut->tex, &cfg);
}

static void render_postfx(Demo *d) {
    const FogPreset *p = &FOG_PRESETS[d->fog_preset];
    if (!d->bench_postfx)
        return; // benchmark phase with post-fx off
    switch (p->type) {
        case FX_FOG:
            depth_fog(p);
            break;
        case FX_DOF:
            render_depth_of_field(d, p->strength, false);
            break;
        case FX_DOF_FOG:
            depth_fog(p);
            render_depth_of_field(d, p->strength, true);
            break;
        case FX_LUT:
            depth_visualize(d->depth_heatmap_lut);
            break;
        case FX_SHIFT:
            gs_depth_bitshift(p->shift);
            depth_visualize(d->depth_normalized_lut);
            break;
    }
}

// ---------------------------------------------------------------------------
// Benchmark
// ---------------------------------------------------------------------------

static float elapsed_ms(const AthenaTimer *t) {
    return (float)athena_timer_get_time(t) * 1000.0f / CLOCKS_PER_SEC;
}

static void bench_apply(Demo *d) {
    athena_render_data_set_pipeline(d->scene, BENCH_PHASES[d->bench_phase].lights ? PL_DEFAULT : PL_NO_LIGHTS);
    d->bench_postfx = BENCH_PHASES[d->bench_phase].postfx;
    memset(d->bench_sum, 0, sizeof(d->bench_sum));
    d->bench_fast_clip = 0;
    d->bench_samples = d->bench_frames = 0;
    athena_timer_reset(d->bench_timer);
}

static void bench_tick(Demo *d) {
    const float elapsed = elapsed_ms(d->bench_timer);
    d->bench_frames++;
    if (elapsed >= BENCH_WARMUP_MS && d->bench_frames > 60) {
        const prof_report *r = athena_screen_get_profile();
        d->bench_sum[0] += r->frame_ms;
        for (int k = 0; k < PROF_COUNT; k++)
            d->bench_sum[1 + k] += r->section_ms[k];
        d->bench_fast_clip += athena_render_module_get_stats()->fast_clip_groups;
        d->bench_samples++;
    }
    if (elapsed < BENCH_PHASE_MS || d->bench_samples == 0)
        return;
    double avg[7];
    for (int k = 0; k < 7; k++)
        avg[k] = d->bench_sum[k] / d->bench_samples;
    AthenaMemoryStats mem;
    athena_system_get_memory_stats(&mem);
    printf("[bench] %s | %.1f fps | frame %.1f | script %.1f | render %.1f | anim %.1f | phys %.1f | "
           "gpu wait %.1f | vsync %.1f (ms) | fast clip groups %.1f | RAM %u KB | LZ4 saved %.1f KB "
           "(tex %u/%u, mesh %u/%u)\n",
           BENCH_PHASES[d->bench_phase].name, 1000.0 / avg[0], avg[0], avg[1 + PROF_SCRIPT], avg[1 + PROF_RENDER],
           avg[1 + PROF_ANIM], avg[1 + PROF_PHYSICS], avg[1 + PROF_GPU_WAIT], avg[1 + PROF_VSYNC],
           d->bench_fast_clip / d->bench_samples, (unsigned)(mem.used / 1024), mem.saved_bytes / 1024.0,
           (unsigned)mem.compressed_textures, (unsigned)mem.tracked_textures,
           (unsigned)mem.compressed_meshes, (unsigned)mem.tracked_meshes);
    d->bench_phase = (d->bench_phase + 1) % (int)(sizeof(BENCH_PHASES) / sizeof(BENCH_PHASES[0]));
    bench_apply(d);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

static void demo_init(Demo *d) {
    memset(d, 0, sizeof(*d));

    // Before chdir("render"): the UI font lives in the boot folder.
    athenaos_app_init(&d->ui, "Physics demo", "3d", HINTS, HINT_COUNT);
    athena_screen_set_frame_counter(true);

    AthenaVideoMode mode;
    athena_screen_get_mode(&mode);
    mode.psm = GS_PSM_CT32;
    mode.zbuffering = true;
    mode.psmz = GS_ZBUF_24;
    athena_screen_set_mode(&mode);
    d->width = mode.width;
    d->height = mode.height;

    athena_screen_init_buffers();
    d->fbuf = athena_screen_get_buffer(DRAW_BUFFER);
    d->zbuf = athena_screen_get_buffer(DEPTH_BUFFER);

    athena_render_module_init();
    athena_render_module_set_view(60.0f, 2.0f, 250.0f, 0, 0);

    // Assets are in the "render" folder.
    chdir("render");

    d->sky = athena_image_create("sky.png", true);
    d->sky->width = d->width;
    d->sky->height = d->height;

    fx_init(d);

    // --- Physics world & broadphase spaces ---
    d->world = athena_physics_world_create();
    d->jgroup = athena_physics_joint_group_create();
    d->space = athena_physics_space_create(NULL);
    d->shadow_space = athena_physics_space_create(NULL);
    athena_physics_world_set_gravity(d->world, 0.0f, -18.0f, 0.0f);

    // --- Static environment ---
    d->scene = athena_render_data_load("sponza_strip.gltf", NULL);
    athena_render_data_native(d->scene)->attributes.face_culling = CULL_FACE_BACK;
    athena_render_data_native(d->scene)->attributes.accurate_clipping = 1;
    athena_render_data_set_pipeline(d->scene, PL_DEFAULT);
    for (int i = 0; i < 8; i++) {
        GSSURFACE *tex = athena_render_data_get_texture(d->scene, i);
        if (tex) tex->Filter = GS_FILTER_LINEAR;
    }
    d->scene_object = athena_render_object_create(d->scene);
    build_static_world(d);

    // --- Player character (3rd-person walking simulator) ---
    d->anims = athena_anim3d_load("Twerk.gltf");
    d->skin = athena_render_data_load("Twerk.gltf", NULL);
    athena_render_data_native(d->skin)->attributes.accurate_clipping = 1;
    athena_render_data_native(d->skin)->attributes.face_culling = CULL_FACE_BACK;
    // Specular (Blinn-Phong per vertex) on the dynamic props and the player:
    // smooth, dense meshes where the highlights read well. The Sponza stays
    // diffuse -- its huge low-poly triangles only turn per-vertex highlights
    // into stray bright faces.
    athena_render_data_set_pipeline(d->skin, PL_SPECULAR);
    d->skin_object = athena_render_object_create(d->skin);

    d->player_geom = athena_physics_geom_create_sphere(d->space, PLAYER_RADIUS);
    d->player_body = athena_physics_body_create(d->world);
    athena_physics_body_set_mass_sphere(d->player_body, 2.5f, PLAYER_RADIUS);
    athena_physics_geom_set_body(d->player_geom, d->player_body);
    athena_physics_body_set_position(d->player_body, PLAYER_START[0], PLAYER_START[1], PLAYER_START[2]);

    d->player_yaw = PI; // facing -Z down the atrium toward the crate pyramid and the north terrace
    d->emote_idx = -1;
    d->ground_grace = 6;
    set_status(d, "SPONZA ATRIUM READY", 120);

    SET_POSITION(d->skin_object, PLAYER_START[0], PLAYER_START[1] - PLAYER_RADIUS, PLAYER_START[2]);
    SET_ROTATION(d->skin_object, PI / 2, 0.0f, d->player_yaw);
    play_anim(d, has_anim(d, "idle") ? "idle" : "twerk", true);

    // --- Dynamic crates (courtyard pyramid + north terrace) and monkey balls ---
    d->box = athena_render_data_load("box_bump.gltf", NULL);
    athena_render_data_native(d->box)->attributes.face_culling = CULL_FACE_BACK;
    athena_render_data_set_pipeline(d->box, PL_SPECULAR);
    for (int i = 0; i < CRATE_COUNT; i++)
        create_prop(d, &d->props[i], d->box, crateSpawns[i], true);

    d->monkey_tex = athena_image_create("monkey.png", true);
    d->monkey_tex->tex->Filter = GS_FILTER_LINEAR;
    d->monkey = athena_render_data_load("monkey_strip.gltf", d->monkey_tex->tex);
    athena_render_data_native(d->monkey)->attributes.face_culling = CULL_FACE_BACK;
    athena_render_data_set_pipeline(d->monkey, PL_SPECULAR);
    for (int i = 0; i < BALL_COUNT; i++)
        create_prop(d, &d->props[CRATE_COUNT + i], d->monkey, ballSpawns[i], false);

    // --- Third-person follow camera ---
    d->cam_yaw = 0.0f;
    d->cam_pitch = 0.34f;
    d->cam_dist = 5.8f;
    d->cam_target[0] = PLAYER_START[0];
    d->cam_target[1] = PLAYER_START[1] + 0.8f;
    d->cam_target[2] = PLAYER_START[2];

    // --- Architectural atrium lighting (sunlight + warm colonnade fill) ---
    static const float SUN_DIR[3] = { 0.35f, 0.90f, 0.45f };
    int light = athena_lights_new();
    athena_lights_set(light, ATHENA_LIGHT_DIRECTION, SUN_DIR[0], SUN_DIR[1], SUN_DIR[2]);
    athena_lights_set(light, ATHENA_LIGHT_AMBIENT,   0.26f, 0.27f, 0.30f);
    athena_lights_set(light, ATHENA_LIGHT_DIFFUSE,   0.78f, 0.74f, 0.66f);
    athena_lights_set(light, ATHENA_LIGHT_SPECULAR,  1.00f, 1.00f, 1.00f); // white sun highlight

    int fill = athena_lights_new();
    athena_lights_set(fill, ATHENA_LIGHT_DIRECTION, -0.45f, 0.55f, -0.70f);
    athena_lights_set(fill, ATHENA_LIGHT_AMBIENT,    0.05f, 0.04f, 0.03f);
    athena_lights_set(fill, ATHENA_LIGHT_DIFFUSE,    0.34f, 0.27f, 0.20f);
    athena_lights_set(fill, ATHENA_LIGHT_SPECULAR,   0.70f, 0.50f, 0.30f); // warm second highlight

    shadows_init(d, SUN_DIR);
    postfx_init(d);

    d->pad = athena_pad_open(0);
    d->bench_postfx = true;
    d->bench_timer = athena_timer_create();
    if (BENCHMARK) {
        printf("[bench] automatic benchmark on: 4 phases, %d s each\n", (int)(BENCH_PHASE_MS / 1000));
        bench_apply(d);
    }

    athena_screen_set_param(ALPHA_TEST_ENABLE, false);
    athena_screen_set_param(ALPHA_TEST_METHOD, ALPHA_LESS);
    athena_screen_set_param(ALPHA_TEST_REF, 0x80);
    athena_screen_set_param(DEPTH_TEST_ENABLE, true);
    athena_screen_set_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

static float stick(int v, float scale) { return (v > 25 || v < -25) ? v / scale : 0.0f; }

// Returns false when the user asked to leave.
static bool demo_frame(Demo *d) {
    AthenaPad *pad = d->pad;
    athena_screen_clear(RGBA(180, 180, 220, 128));
    athena_pad_update(pad);

    if (athenaos_home_requested(pad))
        return false;

    if (!BENCHMARK && athena_pad_just_pressed(pad, PAD_R3)) fx_reset(d, (d->fx_level + 1) % FX_LEVEL_COUNT);
    if (athena_pad_just_pressed(pad, PAD_L3)) d->shadow_mode = (d->shadow_mode + 1) % SHADOW_MODE_COUNT;

    if (athena_pad_just_pressed(pad, PAD_START)) {
        reset_sandbox(d);
        fx_reset(d, d->fx_level);
    }

    if (athena_pad_just_pressed(pad, PAD_SELECT)) {
        char text[48];
        d->fog_preset = (d->fog_preset + 1) % FOG_PRESET_COUNT;
        snprintf(text, sizeof(text), "POST-FX: %s", FOG_PRESETS[d->fog_preset].name);
        set_status(d, text, 110);
    }

    // --- 1. Camera orbit & zoom (right stick + L2/R2) ---
    const float rx = stick(pad->rx, 1024.0f), ry = stick(pad->ry, 1024.0f);
    d->cam_yaw -= rx * 0.18f;
    d->cam_pitch += ry * 0.14f;
    if (d->cam_pitch < 0.08f) d->cam_pitch = 0.08f;
    if (d->cam_pitch > 1.15f) d->cam_pitch = 1.15f;

    if (athena_pad_pressed(pad, PAD_R2)) {
        d->cam_dist -= 0.12f;
        if (d->cam_dist < 2.8f) d->cam_dist = 2.8f;
    } else if (athena_pad_pressed(pad, PAD_L2)) {
        d->cam_dist += 0.12f;
        if (d->cam_dist > 13.0f) d->cam_dist = 13.0f;
    }

    // --- 2. Player movement (left stick or D-pad) ---
    float stick_x = stick(pad->lx, 128.0f), stick_y = stick(pad->ly, 128.0f);
    if (athena_pad_pressed(pad, PAD_LEFT))  stick_x = -1.0f;
    if (athena_pad_pressed(pad, PAD_RIGHT)) stick_x =  1.0f;
    if (athena_pad_pressed(pad, PAD_UP))    stick_y = -1.0f;
    if (athena_pad_pressed(pad, PAD_DOWN))  stick_y =  1.0f;

    float input_mag = sqrtf(stick_x * stick_x + stick_y * stick_y);
    if (input_mag > 1.0f) {
        stick_x /= input_mag;
        stick_y /= input_mag;
        input_mag = 1.0f;
    }

    float px, py, pz, vx, vy, vz;
    athena_physics_body_get_position(d->player_body, &px, &py, &pz);
    athena_physics_body_get_linear_vel(d->player_body, &vx, &vy, &vz);

    // Respawn safety if the player ever falls out of bounds
    if (py < -8.0f) {
        reset_sandbox(d);
        px = PLAYER_START[0]; py = PLAYER_START[1]; pz = PLAYER_START[2];
        vx = vy = vz = 0.0f;
    }

    if (d->jump_cooldown > 0) d->jump_cooldown--;
    if (d->kick_cooldown > 0) d->kick_cooldown--;
    if (d->status_timer > 0) d->status_timer--;
    if (d->anim_timer > 0) d->anim_timer--;

    const bool on_floor = (d->ground_grace > 0) || (py <= PLAYER_RADIUS + 0.12f && fabsf(vy) < 1.8f);

    // Standing / balancing on top of a crate or ball
    bool on_prop = false;
    if (fabsf(vy) < 2.2f && d->jump_cooldown < 30) {
        for (int i = 0; i < PROP_COUNT; i++) {
            float bp[3];
            athena_physics_body_get_position(d->props[i].body, &bp[0], &bp[1], &bp[2]);
            const float dx = px - bp[0], dz = pz - bp[2], dy = (py - PLAYER_RADIUS) - bp[1];
            if ((dx * dx + dz * dz) < 0.72f * 0.72f && dy > 0.15f && dy < 0.75f) {
                on_prop = true;
                break;
            }
        }
    }

    const bool is_grounded = on_floor || on_prop;
    const bool is_balancing = on_prop;
    const bool is_sprinting = athena_pad_pressed(pad, PAD_R1);
    const float move_speed = is_sprinting ? 5.2f : 2.2f;

    // Camera-relative horizontal basis
    const float fwd_x = -sinf(d->cam_yaw), fwd_z = -cosf(d->cam_yaw);
    const float right_x = cosf(d->cam_yaw), right_z = -sinf(d->cam_yaw);
    const float move_x = stick_x * right_x + (-stick_y) * fwd_x;
    const float move_z = stick_x * right_z + (-stick_y) * fwd_z;

    if (input_mag > 0.08f) {
        const float accel = is_grounded ? 0.35f : 0.12f;
        vx += (move_x * move_speed - vx) * accel;
        vz += (move_z * move_speed - vz) * accel;

        // Smoothly rotate the character toward the movement direction
        float diff = atan2f(-move_x, move_z) - d->player_yaw;
        while (diff >  PI) diff -= 2.0f * PI;
        while (diff < -PI) diff += 2.0f * PI;
        d->player_yaw += diff * 0.22f;

        // Gentle camera auto-align when not manually orbiting
        if (rx == 0.0f && ry == 0.0f)
            d->cam_yaw -= stick_x * 0.012f;

        d->walk_cycle += (is_sprinting ? 0.32f : 0.20f) * input_mag;
        d->emote_active = false;
    } else if (is_grounded) {
        vx *= 0.72f;
        vz *= 0.72f;
    }

    // --- 3. Actions: jump (cross), kick (square), throw (circle), dance (L1) ---
    if (athena_pad_just_pressed(pad, PAD_CROSS) && is_grounded && d->jump_cooldown == 0 && d->jump_windup == 0) {
        d->jump_windup = 26; // ~0.43s squat/wind-up before leaving the ground
        d->jump_cooldown = 50;
        d->emote_active = false;
        play_anim(d, has_anim(d, "jump") ? "jump" : "capoeira", false);
        d->anim_timer = 68;
        set_status(d, "CHARGING JUMP...", 45);
    }

    if (d->jump_windup > 0) {
        d->jump_windup--;
        if (d->jump_windup == 0) {
            vy = 7.4f;
            d->ground_grace = 0;
            set_status(d, "JUMP!", 45);
            fx_burst(d, px, py - PLAYER_RADIUS + 0.025f, pz, 8, 0);
        }
    }

    // Square: standing melee kick, with a wind-up and live right-foot bone collision
    if (athena_pad_just_pressed(pad, PAD_SQUARE) && d->kick_cooldown == 0 && d->kick_phase == 0) {
        d->kick_cooldown = 58;
        d->kick_phase = 1;
        d->kick_hits = 0;
        memset(d->kick_hit, 0, sizeof(d->kick_hit));
        d->emote_active = false;
        play_anim(d, has_anim(d, "kick") ? "kick" : "capoeira", false);
        d->anim_timer = 64;
        set_status(d, "WINDING UP KICK...", 45);
    }

    if (d->kick_phase > 0) {
        d->kick_phase++;

        // Damp horizontal movement during the wind-up and strike so the foot plants cleanly
        if (is_grounded && d->kick_phase < 48) {
            vx *= 0.65f;
            vz *= 0.65f;
        }

        // Active foot strike window (frames 26..45 of the Mixamo kick animation)
        if (d->kick_phase >= 26 && d->kick_phase <= 45) {
            const float cz = cosf(d->player_yaw), sz = sinf(d->player_yaw);
            const float face_x = -sz, face_z = cz;

            // Live Mixamo RightToeBase (bone 23) & RightFoot (bone 22) transforms
            float sx = -0.06f, sy = 0.98f, sz_bone = -1.15f;
            athena_object_data *skin = native(d->skin_object);
            const athena_render_data *skin_data = athena_render_data_native(d->skin);
            if (skin->bones && skin_data->skeleton && skin_data->skeleton->bone_count > RIGHT_TOE_BONE) {
                const float *toe = skin->bones[RIGHT_TOE_BONE].transform;
                const float *foot = skin->bones[RIGHT_FOOT_BONE].transform;
                sx = 0.6f * toe[3] + 0.4f * foot[3];
                sy = 0.6f * toe[7] + 0.4f * foot[7];
                sz_bone = 0.6f * toe[11] + 0.4f * foot[11];
            }

            // Skeleton-space foot -> world space
            const float foot_x = px + (sx * cz - sy * sz);
            const float foot_y = (py - PLAYER_RADIUS) - sz_bone;
            const float foot_z = pz + (sx * sz + sy * cz);

            for (int i = 0; i < PROP_COUNT; i++) {
                if (d->kick_hit[i])
                    continue;
                AthenaPhysicsBody *b = d->props[i].body;
                float bp[3];
                athena_physics_body_get_position(b, &bp[0], &bp[1], &bp[2]);
                const float dx = bp[0] - foot_x, dy = bp[1] - foot_y, dz = bp[2] - foot_z;
                const float horiz_sq = dx * dx + dz * dz;

                // Horizontal foot proximity and the leg sweep's vertical band
                if (horiz_sq < 1.15f * 1.15f && fabsf(dy) < 1.25f) {
                    const float horiz = sqrtf(horiz_sq) + 0.001f;
                    // Between the kick arc's forward and the foot-to-object contact normal
                    float dir_x = face_x * 0.65f + (dx / horiz) * 0.35f;
                    float dir_z = face_z * 0.65f + (dz / horiz) * 0.35f;
                    const float dir_len = sqrtf(dir_x * dir_x + dir_z * dir_z) + 0.001f;
                    dir_x /= dir_len;
                    dir_z /= dir_len;

                    const float strength = 13.5f;
                    float bv[3];
                    athena_physics_body_get_linear_vel(b, &bv[0], &bv[1], &bv[2]);
                    athena_physics_body_set_linear_vel(b, bv[0] + dir_x * strength, bv[1] + 5.2f, bv[2] + dir_z * strength);
                    athena_physics_body_set_angular_vel(b, dir_z * 10.0f, 4.0f, -dir_x * 10.0f);

                    d->kick_hit[i] = true;
                    d->kick_hits++;
                    fx_burst(d, foot_x, foot_y, foot_z, 20, i < CRATE_COUNT ? 1 : 2);
                    char text[48];
                    snprintf(text, sizeof(text), "KICK IMPACT! (%d HIT)", d->kick_hits);
                    set_status(d, text, 60);
                }
            }
        }

        if (d->kick_phase >= 52) {
            if (d->kick_hits == 0)
                set_status(d, "KICK!", 30);
            d->kick_phase = 0;
        }
    }

    // Circle: throw / bowl a monkey ball forward
    if (athena_pad_just_pressed(pad, PAD_CIRCLE)) {
        AthenaPhysicsBody *b = d->props[CRATE_COUNT + d->next_ball].body;
        d->next_ball = (d->next_ball + 1) % BALL_COUNT;

        const float face_x = -sinf(d->player_yaw), face_z = cosf(d->player_yaw);
        athena_physics_body_set_position(b, px + face_x * 0.95f, (py - PLAYER_RADIUS) + 1.05f, pz + face_z * 0.95f);
        athena_physics_body_set_linear_vel(b, face_x * 15.5f + vx * 0.4f, 2.8f, face_z * 15.5f + vz * 0.4f);
        athena_physics_body_set_angular_vel(b, face_z * 12.0f, 4.0f, -face_x * 12.0f);

        set_status(d, "MONKEY BALL LAUNCHED!", 60);
    }

    // L1: cycle the Mixamo dance emotes
    if (athena_pad_just_pressed(pad, PAD_L1)) {
        char text[48];
        d->emote_idx = (d->emote_idx + 1) % EMOTE_COUNT;
        d->emote_active = true;
        d->anim_timer = 0;
        play_anim(d, EMOTES[d->emote_idx], true);
        snprintf(text, sizeof(text), "DANCE: %s", EMOTES[d->emote_idx]);
        for (char *c = text; *c; c++)
            if (*c >= 'a' && *c <= 'z') *c -= 'a' - 'A';
        set_status(d, text, 60);
    }

    // Keep the animation looping to match the movement state
    if (d->anim_timer == 0) {
        const char *desired = "idle";
        if (!is_grounded)
            desired = has_anim(d, "fall") ? "fall" : "idle";
        else if (is_balancing && input_mag < 0.35f && has_anim(d, "balance"))
            desired = "balance";
        else if (input_mag > 0.08f)
            desired = is_sprinting ? "run" : "walk";
        else if (d->emote_active)
            desired = EMOTES[d->emote_idx];
        if (!has_anim(d, desired))
            desired = "idle";
        if (strcmp(d->current_anim, desired) || !athena_render_object_is_playing_anim(d->skin_object, NULL))
            play_anim(d, desired, true);
    }

    // Apply the player's velocity and lock the body's rotation
    const float pre_step_vy = vy;
    athena_physics_body_set_linear_vel(d->player_body, vx, vy, vz);
    athena_physics_body_set_angular_vel(d->player_body, 0.0f, 0.0f, 0.0f);

    // --- 4. Step the world and resolve collisions ---
    world_step(d, 0.016f);

    // Multi-level ground & ramp contact detection: free fall changes vy by
    // -18.0 * 0.016 = -0.288 per step. If normal forces from the floor, the
    // terrace or the ramps counteract gravity, refresh ground_grace.
    float post_vel[3];
    athena_physics_body_get_linear_vel(d->player_body, &post_vel[0], &post_vel[1], &post_vel[2]);
    const float post_vy = post_vel[1];
    if (d->jump_cooldown < 38 && (post_vy - pre_step_vy) > -0.18f && post_vy > -2.4f && post_vy < 2.8f) {
        if (!is_grounded && pre_step_vy < -2.0f)
            fx_burst(d, px, py - PLAYER_RADIUS + 0.025f, pz, 14, 0);
        d->ground_grace = 6;
    } else if (d->ground_grace > 0) {
        d->ground_grace--;
    }

    // Sync the player's mesh
    athena_physics_body_get_position(d->player_body, &px, &py, &pz);
    if (is_grounded && input_mag > 0.08f && d->walk_cycle >= d->fx_next_step) {
        const float side = ((int)floorf(d->walk_cycle / PI) % 2 == 0) ? -1.0f : 1.0f;
        fx_burst(d, px + cosf(d->player_yaw) * 0.18f * side, py - PLAYER_RADIUS + 0.025f,
                 pz + sinf(d->player_yaw) * 0.18f * side, is_sprinting ? 5 : 3, 0);
        d->fx_next_step = d->walk_cycle + PI;
    }
    fx_prop_impacts(d);

    SET_POSITION(d->skin_object, px, py - PLAYER_RADIUS, pz);
    SET_ROTATION(d->skin_object, PI / 2, 0.0f, d->player_yaw);

    // --- 5. Third-person follow camera ---
    d->cam_target[0] += (px - d->cam_target[0]) * 0.18f;
    d->cam_target[1] += ((py - PLAYER_RADIUS + 1.15f) - d->cam_target[1]) * 0.14f;
    d->cam_target[2] += (pz - d->cam_target[2]) * 0.18f;

    const float horiz_dist = d->cam_dist * cosf(d->cam_pitch);
    const float cam_x = d->cam_target[0] + sinf(d->cam_yaw) * horiz_dist;
    const float cam_y = maxf(0.55f, d->cam_target[1] + sinf(d->cam_pitch) * d->cam_dist);
    const float cam_z = d->cam_target[2] + cosf(d->cam_yaw) * horiz_dist;

    athena_camera3d_target(d->cam_target[0], d->cam_target[1], d->cam_target[2]);
    athena_camera3d_position(cam_x, cam_y, cam_z);
    athena_camera3d_update();

    // --- 6. Render ---
    athena_render_module_begin();
    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
    athena_image_draw(d->sky, 0, 0);
    athena_screen_set_param(DEPTH_TEST_ENABLE, true);
    athena_screen_set_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);
    render_opaque_scene(d);
    // Hand the scene to the GPU now: the shadow casters' raycasts and setup
    // that follow take a while on the EE, and the queued scene would otherwise
    // only go out once the shadow passes fill the packet buffer -- the GPU
    // idle all that time, then the EE waiting for it.
    athena_screen_flush();
    render_dynamic_shadows(d, px, py, pz, cam_x, cam_y, cam_z);
    // World-space billboards use scene depth, then receive the same fog/DOF.
    fx_render(d);

    // --- 6.5. Depth fog / DOF / channel shuffle post-pass ---
    render_postfx(d);

    // --- 7. HUD ---
    athena_screen_set_param(DEPTH_TEST_ENABLE, false);

    const char *state = !is_grounded
        ? (!strcmp(d->current_anim, "jump") ? "JUMPING" : "FALLING")
        : (d->kick_phase > 0
            ? "KICKING"
            : (is_balancing && input_mag < 0.35f
                ? "BALANCING"
                : (input_mag > 0.08f
                    ? (is_sprinting ? "SPRINTING" : "WALKING")
                    : (d->emote_active ? "DANCING" : "IDLE"))));

    snprintf(d->ui.status, sizeof(d->ui.status), "%.1f FPS", athena_screen_get_fps(360));

    const prof_report *prof = athena_screen_get_profile();
    AthenaMemoryStats mem;
    athena_system_get_memory_stats(&mem);
    const AthenaParticleStats fx = particles_stats(d->fx);
    char anim_upper[24];
    snprintf(anim_upper, sizeof(anim_upper), "%s", d->current_anim);
    for (char *c = anim_upper; *c; c++)
        if (*c >= 'a' && *c <= 'z') *c -= 'a' - 'A';

    // Left card: what the player is doing and the world toggles.
    char world[6][96];
    const char *world_lines[6];
    int n = 0;
    snprintf(world[n++], 96, "%s (%s)", state, anim_upper);
    snprintf(world[n++], 96, "SELECT  Fog: %s", FOG_PRESETS[d->fog_preset].name);
    snprintf(world[n++], 96, "R3  FX density: %d  (%u active, %u visible, %u batches)",
             FX_LEVELS[d->fx_level], (unsigned)fx.active, (unsigned)fx.visible, (unsigned)fx.batches);
    snprintf(world[n++], 96, "L3  Shadows: %s  (%d casters, %d overlapping)",
             SHADOW_MODE_NAMES[d->shadow_mode], d->shadow_draws, d->shadow_overlap_groups);
    snprintf(world[n++], 96, "START  Reset sandbox  |  Fast clip: %u",
             (unsigned)athena_render_module_get_stats()->fast_clip_groups);
    if (d->status_timer > 0)
        snprintf(world[n++], 96, ">> %s", d->status_text);
    for (int i = 0; i < n; i++)
        world_lines[i] = world[i];
    athenaos_hud(&d->ui, world_lines, n, 0);

    // Right card: frame profile and memory compaction.
    char perf[6][64];
    const char *perf_lines[6];
    snprintf(perf[0], 64, "frame %.1f ms", prof->frame_ms);
    snprintf(perf[1], 64, "script %.1f  render %.1f", prof->section_ms[PROF_SCRIPT], prof->section_ms[PROF_RENDER]);
    snprintf(perf[2], 64, "anim %.1f  phys %.1f", prof->section_ms[PROF_ANIM], prof->section_ms[PROF_PHYSICS]);
    snprintf(perf[3], 64, "gpu %.1f  vsync %.1f", prof->section_ms[PROF_GPU_WAIT], prof->section_ms[PROF_VSYNC]);
    snprintf(perf[4], 64, "RAM %u KB  LZ4 -%.1f KB (%.2fx)", (unsigned)(mem.used / 1024), mem.saved_bytes / 1024.0f,
             mem.compressed_bytes > 0 ? (float)mem.uncompressed_bytes / mem.compressed_bytes : 1.0f);
    snprintf(perf[5], 64, "Tex %u/%u  Mesh %u/%u  OOM %u",
             (unsigned)mem.compressed_textures, (unsigned)mem.tracked_textures,
             (unsigned)mem.compressed_meshes, (unsigned)mem.tracked_meshes, (unsigned)mem.oom_rescues);
    for (int i = 0; i < 6; i++)
        perf_lines[i] = perf[i];
    athenaos_hud(&d->ui, perf_lines, 6, ATHENAOS_HUD_RIGHT | ATHENAOS_HUD_BOTTOM);

    athenaos_chrome(&d->ui);

    if (BENCHMARK)
        bench_tick(d);
    athena_screen_flip();
    return true;
}

static int physics_demo_main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    while (!bootlogo_finished() && boot_logo)
        usleep(1000);

    static Demo demo;
    demo_init(&demo);
    while (demo_frame(&demo))
        ;

    athena_screen_clear(RGBA(0, 0, 0, 128));
    athena_screen_flip();
    return 0;
}

#else

static int physics_demo_main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("physics demo: needs ATHENA_GRAPHICS and ATHENA_PHYSICS\n");
    return 1;
}

#endif /* ATHENA_GRAPHICS && ATHENA_PHYSICS */

ATHENAOS_APP(physics_collision, physics_demo_main)
