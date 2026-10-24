/*
 * Star Fox, in C: the port of main.js. A rail shooter on the 3D renderer, with
 * a hangar of ships, parts, colours and upgrades bought with the credits each
 * run earns. Every mesh is built in code with its lighting baked into the
 * vertex colours; the voxel asteroids rebuild theirs as shots carve them.
 */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <athena/athena.h>
#include "athenaos/app.h"

// ---------------------------------------------------------------------------
// Palette / tuning
// ---------------------------------------------------------------------------
#define STAR_COUNT 56
#define STAR_SPREAD_X 90.0f
#define STAR_SPREAD_Y 55.0f
#define STAR_Z_NEAR 150.0f
#define STAR_Z_FAR 340.0f
#define MAX_PARTICLES 72
#define MAX_PLANETS 4
#define BOUNDS_X 6.0f
#define BOUNDS_Y_LOW -2.4f
#define BOUNDS_Y_HIGH 3.4f
#define SHIP_ACCEL 0.16f
#define BASE_SPEED 0.85f
#define MAX_BULLETS 40
#define MAX_ENEMIES 10
#define VOXEL_GRID 5
#define VOXEL_MAX 16
#define VOXEL_CELL 0.30f
#define VOXEL_DESTROY_AT 0
#define MAX_FX 12

enum { STATE_MENU, STATE_PLAY, STATE_OVER, STATE_HANGAR };

static Color SPACE, C_WHITE, C_CYAN, C_YELLOW, C_RED, C_GREEN, C_DIM;

static float CW, CH;

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline int maxi(int a, int b) { return a > b ? a : b; }

// ---------------------------------------------------------------------------
// Roguelike catalogs
// ---------------------------------------------------------------------------
typedef struct { float cx, cy, cz, sx, sy, sz; int accent; } Box;

typedef struct {
    int cooldown, bullets;
    float spread;
    int damage, lives;
    float speed;
} Stats;

typedef struct {
    const char *id, *name;
    int price;
    Stats base;
    const Box *body;
    int body_count;
} Hull;

typedef struct {
    const char *id, *name;
    int price;
    Stats mod;
    const Box *mesh;
    int mesh_count;
} Part;

typedef struct {
    const char *id, *name;
    int price;
    float primary[3], accent[3];
} ShipColor;

typedef struct { const char *id, *name; int max, base_cost; } UpgradeDef;

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const Box SCOUT_BODY[] = {
    { 0.0f, 0.0f, 0.0f, 0.5f, 0.28f, 1.6f, 0 },
    { 0.0f, 0.14f, 0.32f, 0.24f, 0.14f, 0.5f, 1 },
    { -0.66f, 0.14f, -0.35f, 0.07f, 0.4f, 0.42f, 0 },
    { 0.66f, 0.14f, -0.35f, 0.07f, 0.4f, 0.42f, 0 },
    { 0.0f, 0.28f, -0.72f, 0.07f, 0.5f, 0.4f, 0 },
};
static const Box RAPTOR_BODY[] = {
    { 0.0f, 0.0f, 0.0f, 0.42f, 0.22f, 1.5f, 0 },
    { 0.0f, 0.12f, 0.28f, 0.2f, 0.12f, 0.45f, 1 },
    { 0.0f, 0.22f, -0.65f, 0.06f, 0.45f, 0.35f, 0 },
};
static const Box TITAN_BODY[] = {
    { 0.0f, 0.0f, 0.0f, 0.62f, 0.34f, 1.75f, 0 },
    { 0.0f, 0.16f, 0.35f, 0.28f, 0.16f, 0.55f, 1 },
    { -0.72f, 0.12f, -0.2f, 0.08f, 0.35f, 0.5f, 0 },
    { 0.72f, 0.12f, -0.2f, 0.08f, 0.35f, 0.5f, 0 },
};

static const Hull SHIPS[] = {
    { "scout", "Scout", 0, { 8, 1, 0.0f, 1, 3, 1.0f }, SCOUT_BODY, COUNT(SCOUT_BODY) },
    { "raptor", "Raptor", 120, { 6, 1, 0.0f, 1, 2, 1.15f }, RAPTOR_BODY, COUNT(RAPTOR_BODY) },
    { "titan", "Titan", 200, { 10, 1, 0.0f, 2, 5, 0.82f }, TITAN_BODY, COUNT(TITAN_BODY) },
};
#define SHIP_COUNT COUNT(SHIPS)

static const Box WING_STD[] = {
    { -1.0f, 0.0f, -0.1f, 1.4f, 0.06f, 0.6f, 0 },
    { 1.0f, 0.0f, -0.1f, 1.4f, 0.06f, 0.6f, 0 },
    { -1.75f, 0.0f, -0.15f, 0.3f, 0.1f, 0.45f, 1 },
    { 1.75f, 0.0f, -0.15f, 0.3f, 0.1f, 0.45f, 1 },
};
static const Box WING_WIDE[] = {
    { -1.2f, 0.0f, -0.05f, 1.75f, 0.05f, 0.55f, 0 },
    { 1.2f, 0.0f, -0.05f, 1.75f, 0.05f, 0.55f, 0 },
};
static const Box WING_DART[] = {
    { -0.85f, 0.0f, -0.2f, 1.0f, 0.04f, 0.7f, 0 },
    { 0.85f, 0.0f, -0.2f, 1.0f, 0.04f, 0.7f, 0 },
};
static const Box ENGINE_STD[] = { { 0.0f, 0.0f, -0.86f, 0.34f, 0.22f, 0.2f, 0 } };
static const Box ENGINE_TURBO[] = { { 0.0f, 0.0f, -0.95f, 0.42f, 0.28f, 0.28f, 1 } };
static const Box ENGINE_HEAVY[] = { { 0.0f, 0.0f, -0.88f, 0.48f, 0.3f, 0.32f, 0 } };
static const Box CANNON_TWIN[] = {
    { -0.35f, -0.05f, 0.5f, 0.12f, 0.12f, 0.35f, 1 },
    { 0.35f, -0.05f, 0.5f, 0.12f, 0.12f, 0.35f, 1 },
};
static const Box CANNON_RAIL[] = { { 0.0f, -0.08f, 0.65f, 0.18f, 0.14f, 0.55f, 1 } };
static const Box NOSE_STD[] = {
    { 0.0f, 0.0f, 1.0f, 0.3f, 0.18f, 0.55f, 0 },
    { 0.0f, -0.02f, 1.35f, 0.14f, 0.1f, 0.3f, 1 },
};
static const Box NOSE_PROBE[] = {
    { 0.0f, 0.0f, 1.15f, 0.22f, 0.14f, 0.75f, 0 },
    { 0.0f, 0.0f, 1.55f, 0.08f, 0.08f, 0.35f, 1 },
};
static const Box NOSE_BLUNT[] = { { 0.0f, 0.0f, 0.95f, 0.38f, 0.22f, 0.5f, 0 } };

#define PART_SLOT_COUNT 4
#define PARTS_PER_SLOT 3
static const char *const PART_SLOTS[PART_SLOT_COUNT] = { "wing", "engine", "cannon", "nose" };
static const char *const PART_SLOTS_UPPER[PART_SLOT_COUNT] = { "WING", "ENGINE", "CANNON", "NOSE" };

// mod fields: cooldown, bullets, spread, damage, lives, speed
static const Part PARTS[PART_SLOT_COUNT][PARTS_PER_SLOT] = {
    {
        { "std", "Std Wings", 0, { 0 }, WING_STD, COUNT(WING_STD) },
        { "wide", "Wide Wings", 80, { 0, 0, 0.0f, 0, 1, -0.05f }, WING_WIDE, COUNT(WING_WIDE) },
        { "dart", "Dart Wings", 65, { -1, 0, 0.0f, 0, 0, 0.12f }, WING_DART, COUNT(WING_DART) },
    },
    {
        { "std", "Std Engine", 0, { 0 }, ENGINE_STD, COUNT(ENGINE_STD) },
        { "turbo", "Turbo", 90, { -2, 0, 0.0f, 0, 0, 0.18f }, ENGINE_TURBO, COUNT(ENGINE_TURBO) },
        { "heavy", "Heavy", 70, { 0, 0, 0.0f, 0, 1, -0.08f }, ENGINE_HEAVY, COUNT(ENGINE_HEAVY) },
    },
    {
        { "std", "Pulse Laser", 0, { 0 }, NULL, 0 },
        { "twin", "Twin Mount", 100, { 0, 1, 0.35f, 0, 0, 0.0f }, CANNON_TWIN, COUNT(CANNON_TWIN) },
        { "rail", "Rail Gun", 110, { 2, 0, 0.0f, 1, 0, 0.0f }, CANNON_RAIL, COUNT(CANNON_RAIL) },
    },
    {
        { "std", "Std Nose", 0, { 0 }, NOSE_STD, COUNT(NOSE_STD) },
        { "probe", "Probe Nose", 55, { 0, 0, 0.0f, 1, 0, 0.0f }, NOSE_PROBE, COUNT(NOSE_PROBE) },
        { "blunt", "Blunt Nose", 45, { 0, 0, 0.0f, 0, 1, 0.0f }, NOSE_BLUNT, COUNT(NOSE_BLUNT) },
    },
};

static const ShipColor COLORS[] = {
    { "arwing", "Arwing", 0, { 0.72f, 0.76f, 0.86f }, { 0.35f, 0.78f, 0.98f } },
    { "crimson", "Crimson", 40, { 0.82f, 0.22f, 0.18f }, { 1.0f, 0.45f, 0.15f } },
    { "jade", "Jade", 40, { 0.22f, 0.72f, 0.48f }, { 0.55f, 1.0f, 0.72f } },
    { "void", "Void", 55, { 0.28f, 0.26f, 0.38f }, { 0.72f, 0.35f, 0.95f } },
    { "gold", "Gold", 80, { 0.85f, 0.72f, 0.22f }, { 1.0f, 0.92f, 0.45f } },
};
#define COLOR_COUNT COUNT(COLORS)

enum { UP_FIRERATE, UP_MULTISHOT, UP_DAMAGE, UP_HULL, UPGRADE_COUNT };
static const UpgradeDef UPGRADE_DEFS[UPGRADE_COUNT] = {
    { "firerate", "Fire Rate", 5, 45 },
    { "multishot", "Multi-Shot", 3, 70 },
    { "damage", "Damage", 4, 55 },
    { "hull", "Hull", 3, 60 },
};

static const char *const HANGAR_TABS[] = { "SHIPS", "PARTS", "COLORS", "UPGRADES", "LAUNCH" };
#define HANGAR_TAB_COUNT 5

// ---------------------------------------------------------------------------
// Save (same JSON as the script's starfox_save.json)
// ---------------------------------------------------------------------------
// Catalog entries are kept by index; ownership is one bit per entry.
typedef struct { int ship, part[PART_SLOT_COUNT], color; } Loadout;

typedef struct {
    int credits, best;
    unsigned owned_ships, owned_parts[PART_SLOT_COUNT], owned_colors;
    int upgrades[UPGRADE_COUNT];
    Loadout loadout;
} Save;

static Save save;
static char save_path[300];

static void default_save(Save *s) {
    memset(s, 0, sizeof(*s));
    s->owned_ships = 1;
    for (int i = 0; i < PART_SLOT_COUNT; i++)
        s->owned_parts[i] = 1;
    s->owned_colors = 1;
}

// A minimal JSON reader: just enough to walk the save object.
static const char *json_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    return p;
}

static const char *json_skip(const char *p) {
    p = json_ws(p);
    if (*p == '"') {
        for (p++; *p && *p != '"'; p++)
            if (*p == '\\' && p[1])
                p++;
        return *p ? p + 1 : p;
    }
    if (*p == '{' || *p == '[') {
        int depth = 0;
        for (; *p; p++) {
            if (*p == '"') {
                p = json_skip(p) - 1;
            } else if (*p == '{' || *p == '[') {
                depth++;
            } else if (*p == '}' || *p == ']') {
                if (--depth == 0)
                    return p + 1;
            }
        }
        return p;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']')
        p++;
    return p;
}

// The value of obj[key], obj pointing at its '{'; NULL when absent.
static const char *json_member(const char *obj, const char *key) {
    if (!obj)
        return NULL;
    const char *p = json_ws(obj);
    if (*p != '{')
        return NULL;
    p++;
    const size_t key_len = strlen(key);
    for (;;) {
        p = json_ws(p);
        if (*p != '"')
            return NULL;
        const char *name = p + 1;
        p = json_skip(p);
        const bool match = (size_t)(p - 1 - name) == key_len && !strncmp(name, key, key_len);
        p = json_ws(p);
        if (*p != ':')
            return NULL;
        p = json_ws(p + 1);
        if (match)
            return p;
        p = json_ws(json_skip(p));
        if (*p != ',')
            return NULL;
        p++;
    }
}

static bool json_string_is(const char *p, const char *s) {
    if (!p || *p != '"')
        return false;
    const size_t n = strlen(s);
    return !strncmp(p + 1, s, n) && p[1 + n] == '"';
}

static int json_int(const char *p, int fallback) {
    return (p && (*p == '-' || (*p >= '0' && *p <= '9'))) ? (int)strtol(p, NULL, 10) : fallback;
}

// A string's index among ids[] (stride bytes apart), or -1.
static int find_id(const char *p, const void *table, size_t stride, int count) {
    for (int i = 0; i < count; i++) {
        const char *id = *(const char *const *)((const char *)table + stride * i);
        if (json_string_is(p, id))
            return i;
    }
    return -1;
}

// An array of ids into an ownership mask.
static unsigned json_owned(const char *arr, const void *table, size_t stride, int count) {
    unsigned mask = 0;
    const char *p = json_ws(arr) + 1;
    for (;;) {
        p = json_ws(p);
        if (*p != '"')
            break;
        const int i = find_id(p, table, stride, count);
        if (i >= 0)
            mask |= 1u << i;
        p = json_ws(json_skip(p));
        if (*p != ',')
            break;
        p++;
    }
    return mask;
}

static void load_save(void) {
    default_save(&save);
    FILE *f = fopen(save_path, "r");
    if (!f)
        return;
    static char text[2048];
    const size_t n = fread(text, 1, sizeof(text) - 1, f);
    fclose(f);
    text[n] = '\0';
    // The script reads the first line only.
    char *nl = strchr(text, '\n');
    if (nl)
        *nl = '\0';
    const char *raw = json_ws(text);
    if (*raw != '{')
        return;

    save.credits = json_int(json_member(raw, "credits"), save.credits);
    save.best = json_int(json_member(raw, "best"), save.best);
    const char *v;
    if ((v = json_member(raw, "ownedShips")) && *v == '[')
        save.owned_ships = json_owned(v, &SHIPS[0].id, sizeof(Hull), SHIP_COUNT);
    if ((v = json_member(raw, "ownedParts")) && *v == '{') {
        for (int i = 0; i < PART_SLOT_COUNT; i++) {
            const char *list = json_member(v, PART_SLOTS[i]);
            if (list && *list == '[')
                save.owned_parts[i] = json_owned(list, &PARTS[i][0].id, sizeof(Part), PARTS_PER_SLOT);
        }
    }
    if ((v = json_member(raw, "ownedColors")) && *v == '[')
        save.owned_colors = json_owned(v, &COLORS[0].id, sizeof(ShipColor), COLOR_COUNT);
    if ((v = json_member(raw, "upgrades")) && *v == '{') {
        for (int i = 0; i < UPGRADE_COUNT; i++)
            save.upgrades[i] = json_int(json_member(v, UPGRADE_DEFS[i].id), 0);
    }
    if ((v = json_member(raw, "loadout")) && *v == '{') {
        // Replaced whole, as in the script: unknown ids fall back to the first entry.
        int i = find_id(json_member(v, "ship"), &SHIPS[0].id, sizeof(Hull), SHIP_COUNT);
        save.loadout.ship = i < 0 ? 0 : i;
        for (int s = 0; s < PART_SLOT_COUNT; s++) {
            i = find_id(json_member(v, PART_SLOTS[s]), &PARTS[s][0].id, sizeof(Part), PARTS_PER_SLOT);
            save.loadout.part[s] = i < 0 ? 0 : i;
        }
        i = find_id(json_member(v, "color"), &COLORS[0].id, sizeof(ShipColor), COLOR_COUNT);
        save.loadout.color = i < 0 ? 0 : i;
    }
}

static void write_owned(FILE *f, unsigned mask, const void *table, size_t stride, int count) {
    bool first = true;
    fputc('[', f);
    for (int i = 0; i < count; i++) {
        if (!(mask & (1u << i)))
            continue;
        fprintf(f, "%s\"%s\"", first ? "" : ",", *(const char *const *)((const char *)table + stride * i));
        first = false;
    }
    fputc(']', f);
}

static void save_game(void) {
    FILE *f = fopen(save_path, "w");
    if (!f)
        return;
    fprintf(f, "{\"credits\":%d,\"best\":%d,\"ownedShips\":", save.credits, save.best);
    write_owned(f, save.owned_ships, &SHIPS[0].id, sizeof(Hull), SHIP_COUNT);
    fputs(",\"ownedParts\":{", f);
    for (int i = 0; i < PART_SLOT_COUNT; i++) {
        fprintf(f, "%s\"%s\":", i ? "," : "", PART_SLOTS[i]);
        write_owned(f, save.owned_parts[i], &PARTS[i][0].id, sizeof(Part), PARTS_PER_SLOT);
    }
    fputs("},\"ownedColors\":", f);
    write_owned(f, save.owned_colors, &COLORS[0].id, sizeof(ShipColor), COLOR_COUNT);
    fputs(",\"upgrades\":{", f);
    for (int i = 0; i < UPGRADE_COUNT; i++)
        fprintf(f, "%s\"%s\":%d", i ? "," : "", UPGRADE_DEFS[i].id, save.upgrades[i]);
    const Loadout *l = &save.loadout;
    fprintf(f, "},\"loadout\":{\"ship\":\"%s\",\"wing\":\"%s\",\"engine\":\"%s\",\"cannon\":\"%s\",\"nose\":\"%s\",\"color\":\"%s\"}}",
            SHIPS[l->ship].id, PARTS[0][l->part[0]].id, PARTS[1][l->part[1]].id, PARTS[2][l->part[2]].id,
            PARTS[3][l->part[3]].id, COLORS[l->color].id);
    fclose(f);
}

static int upgrade_cost(int key, int level) { return UPGRADE_DEFS[key].base_cost + level * 35; }

static Stats compute_stats(const Save *sv) {
    Stats s = SHIPS[sv->loadout.ship].base;

    for (int i = 0; i < PART_SLOT_COUNT; i++) {
        const Stats *m = &PARTS[i][sv->loadout.part[i]].mod;
        s.cooldown += m->cooldown;
        s.bullets += m->bullets;
        s.spread += m->spread;
        s.damage += m->damage;
        s.lives += m->lives;
        s.speed += m->speed;
    }

    const int *up = sv->upgrades;
    s.cooldown = maxi(3, s.cooldown - up[UP_FIRERATE]);
    s.bullets += up[UP_MULTISHOT];
    if (up[UP_MULTISHOT] > 0 && s.bullets > 1)
        s.spread = fmaxf(s.spread, 0.22f + up[UP_MULTISHOT] * 0.14f);
    s.damage += up[UP_DAMAGE];
    s.lives += up[UP_HULL];

    s.cooldown = maxi(3, s.cooldown);
    s.bullets = maxi(1, s.bullets);
    s.damage = maxi(1, s.damage);
    s.lives = maxi(1, s.lives);
    s.speed = fmaxf(0.5f, s.speed);
    return s;
}

// ---------------------------------------------------------------------------
// Mesh builder with baked directional shading
// ---------------------------------------------------------------------------
typedef struct { float x, y, z; } V3;

typedef struct {
    VECTOR *pos, *nrm, *uv, *col;
    int count, cap;
    bool lit;
} Mesh;

// Light direction normalize(0.35, 0.9, -0.28), baked as in the script.
static float face_light(float nx, float ny, float nz) {
    const float d = nx * 0.3481080f + ny * 0.8951347f + nz * -0.2784864f;
    return 0.4f + 0.6f * (d > 0.0f ? d : 0.0f);
}

static VECTOR *grow(VECTOR *old, int count, int cap) {
    VECTOR *p = memalign(16, cap * sizeof(VECTOR));
    if (old) {
        memcpy(p, old, count * sizeof(VECTOR));
        free(old);
    }
    return p;
}

static void mesh_init(Mesh *b, bool lit) {
    memset(b, 0, sizeof(*b));
    b->lit = lit;
}

static void mesh_free(Mesh *b) {
    free(b->pos);
    free(b->nrm);
    free(b->uv);
    free(b->col);
    memset(b, 0, sizeof(*b));
}

static void set4(VECTOR v, float x, float y, float z, float w) { v[0] = x; v[1] = y; v[2] = z; v[3] = w; }

static void mesh_vertex(Mesh *b, V3 p, float nx, float ny, float nz, float r, float g, float bl, float a) {
    if (b->count == b->cap) {
        const int cap = b->cap ? b->cap * 2 : 96;
        b->pos = grow(b->pos, b->count, cap);
        b->nrm = grow(b->nrm, b->count, cap);
        b->uv = grow(b->uv, b->count, cap);
        b->col = grow(b->col, b->count, cap);
        b->cap = cap;
    }
    const int i = b->count++;
    set4(b->pos[i], p.x, p.y, p.z, 1.0f);
    set4(b->nrm[i], nx, ny, nz, 0.0f);
    set4(b->uv[i], 0.0f, 0.0f, 1.0f, 1.0f);
    set4(b->col[i], r, g, bl, a);
}

static void mesh_tri(Mesh *b, V3 v0, V3 v1, V3 v2, float nx, float ny, float nz, float r, float g, float bl, float a) {
    float cr = r, cg = g, cb = bl;
    if (b->lit) {
        const float l = face_light(nx, ny, nz);
        cr = clampf(r * l, 0.0f, 1.0f);
        cg = clampf(g * l, 0.0f, 1.0f);
        cb = clampf(bl * l, 0.0f, 1.0f);
    }
    mesh_vertex(b, v0, nx, ny, nz, cr, cg, cb, a);
    mesh_vertex(b, v1, nx, ny, nz, cr, cg, cb, a);
    mesh_vertex(b, v2, nx, ny, nz, cr, cg, cb, a);
}

static void mesh_quad(Mesh *b, V3 v0, V3 v1, V3 v2, V3 v3, float nx, float ny, float nz, float r, float g, float bl, float a) {
    mesh_tri(b, v0, v1, v2, nx, ny, nz, r, g, bl, a);
    mesh_tri(b, v0, v2, v3, nx, ny, nz, r, g, bl, a);
}

static void mesh_box(Mesh *b, float cx, float cy, float cz, float sx, float sy, float sz, float r, float g, float bl, float a) {
    const float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
    const V3 A = { cx - hx, cy - hy, cz - hz }, B = { cx + hx, cy - hy, cz - hz };
    const V3 C = { cx + hx, cy + hy, cz - hz }, D = { cx - hx, cy + hy, cz - hz };
    const V3 E = { cx - hx, cy - hy, cz + hz }, F = { cx + hx, cy - hy, cz + hz };
    const V3 G = { cx + hx, cy + hy, cz + hz }, H = { cx - hx, cy + hy, cz + hz };

    mesh_quad(b, E, F, G, H, 0.0f, 0.0f, 1.0f, r, g, bl, a);
    mesh_quad(b, B, A, D, C, 0.0f, 0.0f, -1.0f, r, g, bl, a);
    mesh_quad(b, F, B, C, G, 1.0f, 0.0f, 0.0f, r, g, bl, a);
    mesh_quad(b, A, E, H, D, -1.0f, 0.0f, 0.0f, r, g, bl, a);
    mesh_quad(b, H, G, C, D, 0.0f, 1.0f, 0.0f, r, g, bl, a);
    mesh_quad(b, A, B, F, E, 0.0f, -1.0f, 0.0f, r, g, bl, a);
}

static V3 normalize3(float x, float y, float z) {
    const float len = sqrtf(x * x + y * y + z * z);
    if (len < 0.0001f)
        return (V3){ 0.0f, 0.0f, 1.0f };
    return (V3){ x / len, y / len, z / len };
}

// Hemisphere built on the XY disc (z = 0), bulging toward +Z — billboarded to camera each frame.
static V3 sph(float lat, float lon) {
    const float cl = cosf(lat);
    return (V3){ cl * cosf(lon), cl * sinf(lon), sinf(lat) };
}

static void mesh_dome(Mesh *b, float br, float bg, float bb, float hr, float hg, float hb, int rings, int sectors) {
    for (int i = 0; i < rings; i++) {
        const float lat0 = ((float)M_PI * 0.5f * i) / rings;
        const float lat1 = ((float)M_PI * 0.5f * (i + 1)) / rings;
        for (int j = 0; j < sectors; j++) {
            const float lon0 = ((float)M_PI * 2.0f * j) / sectors;
            const float lon1 = ((float)M_PI * 2.0f * (j + 1)) / sectors;

            const V3 v00 = sph(lat0, lon0), v01 = sph(lat0, lon1);
            const V3 v10 = sph(lat1, lon0), v11 = sph(lat1, lon1);

            // Two corners share each ring's colour; the face takes the average.
            const float t0 = sinf(lat0), t1 = sinf(lat1);
            const float cr = (lerpf(br, hr, t0) + lerpf(br, hr, t1)) * 0.5f;
            const float cg = (lerpf(bg, hg, t0) + lerpf(bg, hg, t1)) * 0.5f;
            const float cb = (lerpf(bb, hb, t0) + lerpf(bb, hb, t1)) * 0.5f;

            const V3 n0 = normalize3(v00.x, v00.y, v00.z);
            const V3 n1 = normalize3(v11.x, v11.y, v11.z);

            mesh_tri(b, v00, v01, v11, n0.x, n0.y, n0.z, cr, cg, cb, 1.0f);
            mesh_tri(b, v00, v11, v10, n1.x, n1.y, n1.z, cr, cg, cb, 1.0f);
        }
    }
}

static void mesh_voxel_face(Mesh *b, float cx, float cy, float cz, float hs, int face, float r, float g, float bl, float a) {
    switch (face) {
        case 0:
            mesh_quad(b, (V3){ cx + hs, cy - hs, cz - hs }, (V3){ cx + hs, cy + hs, cz - hs },
                      (V3){ cx + hs, cy + hs, cz + hs }, (V3){ cx + hs, cy - hs, cz + hs }, 1.0f, 0.0f, 0.0f, r, g, bl, a);
            break;
        case 1:
            mesh_quad(b, (V3){ cx - hs, cy + hs, cz - hs }, (V3){ cx - hs, cy - hs, cz - hs },
                      (V3){ cx - hs, cy - hs, cz + hs }, (V3){ cx - hs, cy + hs, cz + hs }, -1.0f, 0.0f, 0.0f, r, g, bl, a);
            break;
        case 2:
            mesh_quad(b, (V3){ cx - hs, cy + hs, cz - hs }, (V3){ cx + hs, cy + hs, cz - hs },
                      (V3){ cx + hs, cy + hs, cz + hs }, (V3){ cx - hs, cy + hs, cz + hs }, 0.0f, 1.0f, 0.0f, r, g, bl, a);
            break;
        case 3:
            mesh_quad(b, (V3){ cx + hs, cy - hs, cz - hs }, (V3){ cx - hs, cy - hs, cz - hs },
                      (V3){ cx - hs, cy - hs, cz + hs }, (V3){ cx + hs, cy - hs, cz + hs }, 0.0f, -1.0f, 0.0f, r, g, bl, a);
            break;
        case 4:
            mesh_quad(b, (V3){ cx - hs, cy - hs, cz + hs }, (V3){ cx + hs, cy - hs, cz + hs },
                      (V3){ cx + hs, cy + hs, cz + hs }, (V3){ cx - hs, cy + hs, cz + hs }, 0.0f, 0.0f, 1.0f, r, g, bl, a);
            break;
        default:
            mesh_quad(b, (V3){ cx + hs, cy - hs, cz - hs }, (V3){ cx - hs, cy - hs, cz - hs },
                      (V3){ cx - hs, cy + hs, cz - hs }, (V3){ cx + hs, cy + hs, cz - hs }, 0.0f, 0.0f, -1.0f, r, g, bl, a);
            break;
    }
}

// Unlit pipeline, no culling: the colours already carry the shading.
static AthenaRenderData *mesh_build(Mesh *b) {
    AthenaRenderData *data = athena_render_data_from_vertices(b->pos, b->nrm, b->uv, b->col, b->count);
    athena_render_data *m = athena_render_data_native(data);
    m->pipeline = PL_NO_LIGHTS;
    m->attributes.face_culling = CULL_FACE_NONE;
    mesh_free(b);
    return data;
}

static void add_boxes(Mesh *b, const Box *boxes, int count, const ShipColor *col) {
    for (int i = 0; i < count; i++) {
        const Box *bx = &boxes[i];
        const float *c = bx->accent ? col->accent : col->primary;
        mesh_box(b, bx->cx, bx->cy, bx->cz, bx->sx, bx->sy, bx->sz, c[0], c[1], c[2], 1.0f);
    }
}

static AthenaRenderData *build_ship_mesh(const Loadout *l) {
    const Hull *hull = &SHIPS[l->ship];
    const ShipColor *col = &COLORS[l->color];
    Mesh b;
    mesh_init(&b, true);
    add_boxes(&b, hull->body, hull->body_count, col);
    for (int i = 0; i < PART_SLOT_COUNT; i++) {
        const Part *p = &PARTS[i][l->part[i]];
        add_boxes(&b, p->mesh, p->mesh_count, col);
    }
    return mesh_build(&b);
}

// Every ship built so far, by loadout, as the script caches them.
typedef struct { Loadout key; AthenaRenderObject *obj; } ShipEntry;
static ShipEntry *ship_cache;
static int ship_cache_count, ship_cache_cap;
static AthenaRenderObject *ship;

static void rebuild_ship(void) {
    for (int i = 0; i < ship_cache_count; i++) {
        if (!memcmp(&ship_cache[i].key, &save.loadout, sizeof(Loadout))) {
            ship = ship_cache[i].obj;
            return;
        }
    }
    if (ship_cache_count == ship_cache_cap) {
        ship_cache_cap = ship_cache_cap ? ship_cache_cap * 2 : 8;
        ship_cache = realloc(ship_cache, ship_cache_cap * sizeof(ShipEntry));
    }
    ShipEntry *e = &ship_cache[ship_cache_count++];
    e->key = save.loadout;
    e->obj = athena_render_object_create(build_ship_mesh(&save.loadout));
    ship = e->obj;
}

// ---------------------------------------------------------------------------
// Rotations
// ---------------------------------------------------------------------------
static V3 rotate_x(V3 v, float a) {
    const float c = cosf(a), s = sinf(a);
    return (V3){ v.x, v.y * c - v.z * s, v.y * s + v.z * c };
}

static V3 rotate_y(V3 v, float a) {
    const float c = cosf(a), s = sinf(a);
    return (V3){ v.x * c + v.z * s, v.y, -v.x * s + v.z * c };
}

static V3 rotate_z(V3 v, float a) {
    const float c = cosf(a), s = sinf(a);
    return (V3){ v.x * c - v.y * s, v.x * s + v.y * c, v.z };
}

static V3 world_to_local(float wx, float wy, float wz, float ex, float ey, float ez, float sc, float rx, float ry, float rz) {
    V3 v = { (wx - ex) / sc, (wy - ey) / sc, (wz - ez) / sc };
    v = rotate_z(v, -rz);
    v = rotate_y(v, -ry);
    return rotate_x(v, -rx);
}

static V3 local_to_world(float lx, float ly, float lz, float ex, float ey, float ez, float sc, float rx, float ry, float rz) {
    V3 v = rotate_x((V3){ lx, ly, lz }, rx);
    v = rotate_y(v, ry);
    v = rotate_z(v, rz);
    return (V3){ ex + v.x * sc, ey + v.y * sc, ez + v.z * sc };
}

// ---------------------------------------------------------------------------
// Voxel asteroids — procedural grid, mesh rebuilt when voxels are carved away
// ---------------------------------------------------------------------------
static int voxel_hash(int ix, int iy, int iz, int seed) {
    return (int)(((uint32_t)ix * 73856093u ^ (uint32_t)iy * 19349663u ^ (uint32_t)iz * 83492791u ^ (uint32_t)seed) & 255);
}

typedef struct {
    int seed;
    uint8_t grid[VOXEL_GRID * VOXEL_GRID * VOXEL_GRID];
    int voxel_count;
    AthenaRenderData *mesh;
} Voxel;

static bool in_grid(int x, int y, int z) {
    return x >= 0 && y >= 0 && z >= 0 && x < VOXEL_GRID && y < VOXEL_GRID && z < VOXEL_GRID;
}

static int vox_get(const Voxel *v, int x, int y, int z) {
    return in_grid(x, y, z) ? v->grid[x + y * VOXEL_GRID + z * VOXEL_GRID * VOXEL_GRID] : 0;
}

static void vox_set(Voxel *v, int x, int y, int z, int on) {
    if (!in_grid(x, y, z))
        return;
    uint8_t *cell = &v->grid[x + y * VOXEL_GRID + z * VOXEL_GRID * VOXEL_GRID];
    if (on && !*cell) v->voxel_count++;
    else if (!on && *cell) v->voxel_count--;
    *cell = on ? 1 : 0;
}

static void vox_rebuild_mesh(Voxel *v) {
    Mesh b;
    mesh_init(&b, true);
    const float hs = VOXEL_CELL * 0.5f;
    const float half = VOXEL_GRID * VOXEL_CELL * 0.5f;
    const int G = VOXEL_GRID, GG = G * G;
    const uint8_t *grid = v->grid;

    for (int z = 0; z < G; z++) {
        const int z_base = z * GG;
        const float zp = (z + 0.5f) * VOXEL_CELL - half;
        for (int y = 0; y < G; y++) {
            const int row = z_base + y * G;
            const float yp = (y + 0.5f) * VOXEL_CELL - half;
            for (int x = 0; x < G; x++) {
                if (!grid[row + x])
                    continue;
                const float xp = (x + 0.5f) * VOXEL_CELL - half;
                const float t = voxel_hash(x, y, z, v->seed) / 255.0f;
                const float r = 0.40f + 0.24f * t, g = 0.36f + 0.20f * t, bl = 0.32f + 0.18f * t;

                if (x + 1 >= G || !grid[row + x + 1]) mesh_voxel_face(&b, xp, yp, zp, hs, 0, r, g, bl, 1.0f);
                if (x - 1 < 0 || !grid[row + x - 1]) mesh_voxel_face(&b, xp, yp, zp, hs, 1, r, g, bl, 1.0f);
                if (y + 1 >= G || !grid[row + G + x]) mesh_voxel_face(&b, xp, yp, zp, hs, 2, r, g, bl, 1.0f);
                if (y - 1 < 0 || !grid[row - G + x]) mesh_voxel_face(&b, xp, yp, zp, hs, 3, r, g, bl, 1.0f);
                if (z + 1 >= G || !grid[row + GG + x]) mesh_voxel_face(&b, xp, yp, zp, hs, 4, r, g, bl, 1.0f);
                if (z - 1 < 0 || !grid[row - GG + x]) mesh_voxel_face(&b, xp, yp, zp, hs, 5, r, g, bl, 1.0f);
            }
        }
    }

    if (!v->mesh) {
        v->mesh = mesh_build(&b);
        return;
    }
    athena_render_data_set_vertices(v->mesh, b.pos, b.nrm, b.uv, b.col, b.count);
    mesh_free(&b);
}

static void vox_reset(Voxel *v, int seed) {
    static const int dirs[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

    v->seed = seed;
    memset(v->grid, 0, sizeof(v->grid));
    v->voxel_count = 0;

    const int c = VOXEL_GRID / 2;
    const int target = 6 + (voxel_hash(c, c, c, seed) % (VOXEL_MAX - 5));
    vox_set(v, c, c, c, 1);

    int fails = 0;
    while (v->voxel_count < target && fails < 80) {
        int pick = voxel_hash(seed, v->voxel_count, fails, 0) % v->voxel_count;
        bool found = false;
        int ox = 0, oy = 0, oz = 0;

        for (int z = 0; z < VOXEL_GRID && !found; z++) {
            for (int y = 0; y < VOXEL_GRID && !found; y++) {
                for (int x = 0; x < VOXEL_GRID; x++) {
                    if (!vox_get(v, x, y, z))
                        continue;
                    if (pick == 0) {
                        ox = x; oy = y; oz = z;
                        found = true;
                        break;
                    }
                    pick--;
                }
            }
        }
        if (!found)
            break;

        const int di = voxel_hash(ox, oy, oz, seed + fails) % 6;
        const int nx = ox + dirs[di][0], ny = oy + dirs[di][1], nz = oz + dirs[di][2];
        if (vox_get(v, nx, ny, nz) || !in_grid(nx, ny, nz)) {
            fails++;
            continue;
        }
        vox_set(v, nx, ny, nz, 1);
        fails = 0;
    }
    vox_rebuild_mesh(v);
}

// Squared distance from a cell's centre to a point in the asteroid's local space.
static float voxel_d2(int ix, int iy, int iz, V3 l) {
    const float half = VOXEL_GRID * VOXEL_CELL * 0.5f;
    const float dx = (ix + 0.5f) * VOXEL_CELL - half - l.x;
    const float dy = (iy + 0.5f) * VOXEL_CELL - half - l.y;
    const float dz = (iz + 0.5f) * VOXEL_CELL - half - l.z;
    return dx * dx + dy * dy + dz * dz;
}

// The radius a cell contributes to the collision sphere: centre distance plus half-diagonal.
static float voxel_radius(int ix, int iy, int iz) {
    const float half = VOXEL_GRID * VOXEL_CELL * 0.5f;
    const float dx = (ix + 0.5f) * VOXEL_CELL - half;
    const float dy = (iy + 0.5f) * VOXEL_CELL - half;
    const float dz = (iz + 0.5f) * VOXEL_CELL - half;
    return sqrtf(dx * dx + dy * dy + dz * dz) + VOXEL_CELL * 0.866f;
}

static float vox_collision_radius(const Voxel *v) {
    float max_dist = 0.0f;
    if (v->voxel_count <= 0)
        return 0.0f;
    for (int z = 0; z < VOXEL_GRID; z++)
        for (int y = 0; y < VOXEL_GRID; y++)
            for (int x = 0; x < VOXEL_GRID; x++)
                if (vox_get(v, x, y, z)) {
                    const float d = voxel_radius(x, y, z);
                    if (d > max_dist)
                        max_dist = d;
                }
    return max_dist;
}

static void spawn_voxel_debris(float x, float y, float z);

typedef struct { float ex, ey, ez, sc, rx, ry, rz; int debris; } CarveCtx;

static int remove_at(Voxel *v, int x, int y, int z, CarveCtx *c) {
    if (!vox_get(v, x, y, z))
        return 0;
    if (c->debris < 6) {
        const float half = VOXEL_GRID * VOXEL_CELL * 0.5f;
        const V3 w = local_to_world((x + 0.5f) * VOXEL_CELL - half, (y + 0.5f) * VOXEL_CELL - half,
                                    (z + 0.5f) * VOXEL_CELL - half, c->ex, c->ey, c->ez, c->sc, c->rx, c->ry, c->rz);
        spawn_voxel_debris(w.x, w.y, w.z);
        c->debris++;
    }
    vox_set(v, x, y, z, 0);
    return 1;
}

// The cell nearest to a local point, false when the grid is empty.
static bool vox_nearest(const Voxel *v, V3 l, int out[3]) {
    float best = 1e9f;
    bool found = false;
    for (int z = 0; z < VOXEL_GRID; z++)
        for (int y = 0; y < VOXEL_GRID; y++)
            for (int x = 0; x < VOXEL_GRID; x++) {
                if (!vox_get(v, x, y, z))
                    continue;
                const float d2 = voxel_d2(x, y, z, l);
                if (d2 < best) {
                    best = d2;
                    out[0] = x; out[1] = y; out[2] = z;
                    found = true;
                }
            }
    return found;
}

// A shot landed: the cells within the blast go, or the nearest one; each point
// of damage past the first takes one more nearest cell.
static int vox_carve_world(Voxel *v, float wx, float wy, float wz, float ex, float ey, float ez, float sc,
                           float rx, float ry, float rz, int dmg) {
    if (dmg <= 0)
        dmg = 1;
    const V3 local = world_to_local(wx, wy, wz, ex, ey, ez, sc, rx, ry, rz);
    const float hit_slop2 = VOXEL_CELL * VOXEL_CELL * 0.8f;
    int removed = 0;
    int best[3];
    if (!vox_nearest(v, local, best))
        return 0;

    CarveCtx c = { ex, ey, ez, sc, rx, ry, rz, 0 };
    for (int z = 0; z < VOXEL_GRID; z++)
        for (int y = 0; y < VOXEL_GRID; y++)
            for (int x = 0; x < VOXEL_GRID; x++)
                if (vox_get(v, x, y, z) && voxel_d2(x, y, z, local) <= hit_slop2)
                    removed += remove_at(v, x, y, z, &c);

    if (removed == 0)
        removed += remove_at(v, best[0], best[1], best[2], &c);

    for (int extra = 1; extra < dmg && v->voxel_count > VOXEL_DESTROY_AT; extra++) {
        int near[3];
        if (!vox_nearest(v, local, near))
            break;
        removed += remove_at(v, near[0], near[1], near[2], &c);
    }

    if (removed > 0)
        vox_rebuild_mesh(v);
    return removed;
}

// ---------------------------------------------------------------------------
// Meshes
// ---------------------------------------------------------------------------
static AthenaRenderData *build_thruster(void) {
    Mesh b;
    mesh_init(&b, false);
    mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.24f, 0.18f, 0.5f, 1.0f, 0.6f, 0.15f, 1.0f);
    mesh_box(&b, 0.0f, 0.0f, -0.28f, 0.12f, 0.1f, 0.4f, 1.0f, 0.9f, 0.5f, 1.0f);
    return mesh_build(&b);
}

static AthenaRenderData *build_enemy(void) {
    Mesh b;
    mesh_init(&b, true);
    mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.85f, 0.42f, 0.9f, 0.85f, 0.2f, 0.18f, 1.0f);    // body
    mesh_box(&b, 0.0f, 0.0f, -0.5f, 0.32f, 0.32f, 0.45f, 1.0f, 0.45f, 0.15f, 1.0f);  // front intake
    mesh_box(&b, 0.0f, 0.14f, 0.35f, 0.22f, 0.16f, 0.4f, 0.55f, 0.12f, 0.12f, 1.0f); // hump
    mesh_box(&b, -0.85f, 0.0f, 0.0f, 0.7f, 0.08f, 0.5f, 0.72f, 0.18f, 0.16f, 1.0f);  // left wing
    mesh_box(&b, 0.85f, 0.0f, 0.0f, 0.7f, 0.08f, 0.5f, 0.72f, 0.18f, 0.16f, 1.0f);   // right wing
    mesh_box(&b, -1.2f, 0.0f, 0.0f, 0.22f, 0.28f, 0.3f, 0.95f, 0.75f, 0.15f, 1.0f);  // left tip
    mesh_box(&b, 1.2f, 0.0f, 0.0f, 0.22f, 0.28f, 0.3f, 0.95f, 0.75f, 0.15f, 1.0f);   // right tip
    return mesh_build(&b);
}

static AthenaRenderData *build_bullet(void) {
    Mesh b;
    mesh_init(&b, false);
    mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.12f, 0.12f, 1.1f, 0.35f, 1.0f, 0.55f, 1.0f);
    mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.05f, 0.05f, 0.5f, 0.9f, 1.0f, 0.9f, 1.0f);
    return mesh_build(&b);
}

static AthenaRenderData *build_explosion(void) {
    Mesh b;
    mesh_init(&b, false);
    mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.28f, 0.28f, 0.28f, 1.0f, 0.98f, 0.88f, 1.0f);
    mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.62f, 0.62f, 0.62f, 1.0f, 0.82f, 0.18f, 1.0f);
    mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.95f, 0.95f, 0.95f, 0.85f, 0.28f, 0.08f, 1.0f);
    return mesh_build(&b);
}

static AthenaRenderData *build_star(void) {
    Mesh b;
    mesh_init(&b, false);
    mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.42f, 0.42f, 0.42f, 1.0f, 1.0f, 1.0f, 1.0f);
    return mesh_build(&b);
}

static AthenaRenderData *build_planet_dome(float br, float bg, float bb, float hr, float hg, float hb) {
    Mesh b;
    mesh_init(&b, false);
    mesh_dome(&b, br, bg, bb, hr, hg, hb, 10, 18);
    return mesh_build(&b);
}

enum { P_BOOST, P_SPARK, P_EMBER, P_FLASH, P_SMOKE };

static AthenaRenderData *build_particle(int kind) {
    Mesh b;
    mesh_init(&b, false);
    if (kind == P_BOOST) {
        mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.06f, 0.06f, 0.35f, 1.0f, 0.72f, 0.18f, 1.0f);
        mesh_box(&b, 0.0f, 0.0f, -0.12f, 0.03f, 0.03f, 0.22f, 1.0f, 0.95f, 0.55f, 1.0f);
    } else if (kind == P_SPARK) {
        mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.025f, 0.025f, 0.42f, 1.0f, 0.95f, 0.55f, 1.0f);
        mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.012f, 0.012f, 0.65f, 1.0f, 1.0f, 0.85f, 1.0f);
    } else if (kind == P_FLASH) {
        mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.07f, 0.07f, 0.07f, 1.0f, 0.98f, 0.82f, 1.0f);
        mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.03f, 0.03f, 0.28f, 1.0f, 1.0f, 0.92f, 1.0f);
    } else if (kind == P_SMOKE) {
        mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.09f, 0.09f, 0.09f, 0.35f, 0.32f, 0.38f, 1.0f);
        mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.06f, 0.06f, 0.06f, 0.22f, 0.20f, 0.24f, 1.0f);
    } else {
        mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.06f, 0.06f, 0.06f, 1.0f, 0.62f, 0.18f, 1.0f);
        mesh_box(&b, 0.0f, 0.0f, 0.0f, 0.04f, 0.04f, 0.04f, 0.85f, 0.45f, 0.12f, 1.0f);
    }
    return mesh_build(&b);
}

typedef struct { float br, bg, bb, hr, hg, hb, x, y, z, sc; } PlanetDef;
static const PlanetDef PLANET_DEFS[MAX_PLANETS] = {
    { 0.22f, 0.16f, 0.42f, 0.55f, 0.42f, 0.82f, -34.0f, 3.5f, 165.0f, 2.4f },
    { 0.18f, 0.22f, 0.38f, 0.38f, 0.48f, 0.72f, 38.0f, 11.0f, 235.0f, 1.8f },
    { 0.32f, 0.14f, 0.12f, 0.72f, 0.38f, 0.22f, 14.0f, 14.5f, 145.0f, 1.4f },
    { 0.12f, 0.18f, 0.28f, 0.28f, 0.36f, 0.58f, -22.0f, 7.5f, 205.0f, 1.1f },
};

// ---------------------------------------------------------------------------
// Scene objects (RenderObjects are reused; positions are assigned each frame)
// ---------------------------------------------------------------------------
typedef struct { AthenaRenderObject *obj; float x, y, z, par, sc; } Star;
typedef struct { AthenaRenderObject *obj; float x, y, z, sc; } Planet;
typedef struct {
    AthenaRenderObject *obj;
    int kind, max_life;
    float spin;
    float x, y, z, vx, vy, vz, life, sc;   // alive while life > 0
} Particle;
typedef struct { AthenaRenderObject *obj; float x, y, z, pz; int damage; bool alive; } Bullet;
typedef struct {
    AthenaRenderObject *fighter, *rock;
    Voxel voxel;
    bool active;
    int type;
    float x, y, z, rx, ry, rz, spin, weave, phase;
    int hp;
    float radius, sc;
} Enemy;
typedef struct { AthenaRenderObject *obj; bool alive; float x, y, z; int life, max_life; float base_sc, spin; } Fx;

static AthenaRenderObject *thruster;
static Star stars[STAR_COUNT];
static Planet planets[MAX_PLANETS];
static Particle particles[MAX_PARTICLES];
static Bullet bullets[MAX_BULLETS];
static Enemy enemies[MAX_ENEMIES];
static Fx fx[MAX_FX];

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
static int state, score, lives, wave, fire_timer, spawn_timer, invuln, fire_side, frame;
static float shake, menu_spin;
static Stats run_stats;
static int run_earned, hang_tab, hang_sel, part_slot, hang_nav_lock;
static float ship_x, ship_y, vel_x, vel_y, roll, pitch, speed;
static AthenaPad *pad;

#define CAM_X 0.0f
#define CAM_Y 1.4f
#define CAM_Z -9.0f
#define LOOK_X 0.0f
#define LOOK_Y 0.0f
#define LOOK_Z 24.0f

static float stick(int v) {
    if (v > -28 && v < 28)
        return 0.0f;
    return clampf(v / 128.0f, -1.0f, 1.0f);
}

static void reset_game(void) {
    run_stats = compute_stats(&save);
    score = 0;
    lives = run_stats.lives;
    wave = 1;
    fire_timer = 0;
    spawn_timer = 45;
    invuln = 0;
    shake = 0.0f;
    ship_x = ship_y = 0.0f;
    vel_x = vel_y = 0.0f;
    roll = pitch = 0.0f;
    speed = BASE_SPEED * run_stats.speed;
    run_earned = 0;

    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].alive = false;
    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
    for (int i = 0; i < MAX_FX; i++) fx[i].alive = false;
    for (int i = 0; i < MAX_PARTICLES; i++) particles[i].life = 0.0f;
    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i].x = athena_randomf(-STAR_SPREAD_X, STAR_SPREAD_X);
        stars[i].y = athena_randomf(-STAR_SPREAD_Y, STAR_SPREAD_Y);
        stars[i].z = athena_randomf(STAR_Z_NEAR, STAR_Z_FAR);
    }
    for (int i = 0; i < MAX_PLANETS; i++) {
        planets[i].x = PLANET_DEFS[i].x;
        planets[i].y = PLANET_DEFS[i].y;
        planets[i].z = PLANET_DEFS[i].z;
    }
}

static void spawn_enemy(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (e->active)
            continue;
        e->active = true;
        e->type = athena_randomi(0, 100) < 32 ? 1 : 0;
        e->x = athena_randomf(-BOUNDS_X, BOUNDS_X);
        e->y = athena_randomf(BOUNDS_Y_LOW + 0.5f, BOUNDS_Y_HIGH - 0.5f);
        e->z = athena_randomf(95.0f, 130.0f);
        e->rx = athena_randomf(0.0f, 6.28f);
        e->ry = athena_randomf(0.0f, 6.28f);
        e->rz = athena_randomf(0.0f, 6.28f);
        e->spin = athena_randomf(-0.05f, 0.05f);
        e->weave = athena_randomf(0.008f, 0.03f);
        e->phase = athena_randomf(0.0f, 6.28f);
        e->hp = 1;
        if (e->type == 1) {
            e->sc = athena_randomf(0.85f, 1.25f);
            vox_reset(&e->voxel, athena_randomi(1, 999999));
            e->radius = vox_collision_radius(&e->voxel) * e->sc;
        } else {
            e->radius = 0.9f;
            e->sc = 1.0f;
        }
        return;
    }
}

static void award_run_credits(void) {
    run_earned = score / 10 + wave * 25;
    save.credits += run_earned;
    if (score > save.best)
        save.best = score;
    save_game();
}

static void fire_bullet(void) {
    const int n = run_stats.bullets;
    const float spread = run_stats.spread;
    for (int b = 0; b < n; b++) {
        const float t = (n == 1) ? 0.5f : (float)b / (n - 1);
        const float off = (t - 0.5f) * spread * 2.0f;
        const float wx = (fire_side > 0 ? 1.6f : -1.6f) + off;
        for (int i = 0; i < MAX_BULLETS; i++) {
            Bullet *bd = &bullets[i];
            if (bd->alive)
                continue;
            bd->alive = true;
            bd->x = ship_x + wx;
            bd->y = ship_y;
            bd->z = 1.2f;
            bd->pz = 1.2f;
            bd->damage = run_stats.damage;
            break;
        }
    }
    fire_side = -fire_side;
}

static bool spawn_particle(int kind, float x, float y, float z, float vx, float vy, float vz, int life, float sc) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &particles[i];
        if (p->life > 0.0f || p->kind != kind)
            continue;
        p->x = x; p->y = y; p->z = z;
        p->vx = vx; p->vy = vy; p->vz = vz;
        p->life = (float)life;
        p->sc = sc;
        p->max_life = life;
        p->spin = athena_randomf(-0.4f, 0.4f);
        return true;
    }
    return false;
}

static void spawn_voxel_debris(float x, float y, float z) {
    const float ang = athena_randomf(0.0f, 6.28f);
    const float spd = athena_randomf(0.06f, 0.22f);
    spawn_particle(P_EMBER,
                   x + athena_randomf(-0.06f, 0.06f), y + athena_randomf(-0.06f, 0.06f), z + athena_randomf(-0.06f, 0.06f),
                   cosf(ang) * spd, athena_randomf(0.02f, 0.14f), sinf(ang) * spd,
                   athena_randomi(12, 24), athena_randomf(0.25f, 0.55f));
}

static V3 burst_dir(float spd) {
    const float yaw = athena_randomf(0.0f, 6.283185f);
    const float p = athena_randomf(-0.95f, 0.95f);
    const float cp = cosf(p);
    return (V3){ cosf(yaw) * cp * spd, sinf(p) * spd, sinf(yaw) * cp * spd };
}

static void spawn_explosion_particles(float x, float y, float z, bool big) {
    const int flash_count = big ? 8 : 4;
    const int spark_count = big ? 18 : 10;
    const int ember_count = big ? 10 : 6;
    const int smoke_count = big ? 6 : 3;

    for (int n = 0; n < flash_count; n++) {
        const V3 d = burst_dir(athena_randomf(0.04f, big ? 0.18f : 0.12f));
        spawn_particle(P_FLASH,
                       x + athena_randomf(-0.08f, 0.08f), y + athena_randomf(-0.08f, 0.08f), z + athena_randomf(-0.08f, 0.08f),
                       d.x, d.y, d.z, athena_randomi(6, 12), athena_randomf(0.7f, 1.3f));
    }
    for (int n = 0; n < spark_count; n++) {
        const float spd = athena_randomf(big ? 0.22f : 0.14f, big ? 0.58f : 0.38f);
        const V3 d = burst_dir(spd);
        spawn_particle(P_SPARK,
                       x + athena_randomf(-0.12f, 0.12f), y + athena_randomf(-0.12f, 0.12f), z + athena_randomf(-0.12f, 0.12f),
                       d.x, d.y, d.z, athena_randomi(14, big ? 28 : 22), athena_randomf(0.45f, 1.0f));
    }
    for (int n = 0; n < ember_count; n++) {
        V3 d = burst_dir(athena_randomf(0.08f, big ? 0.32f : 0.22f));
        d.y += athena_randomf(0.02f, 0.12f);
        spawn_particle(P_EMBER,
                       x + athena_randomf(-0.16f, 0.16f), y + athena_randomf(-0.16f, 0.16f), z + athena_randomf(-0.16f, 0.16f),
                       d.x, d.y, d.z, athena_randomi(18, big ? 36 : 28), athena_randomf(0.35f, 0.85f));
    }
    for (int n = 0; n < smoke_count; n++) {
        V3 d = burst_dir(athena_randomf(0.03f, big ? 0.14f : 0.09f));
        d.y += athena_randomf(0.04f, 0.16f);
        spawn_particle(P_SMOKE,
                       x + athena_randomf(-0.1f, 0.1f), y + athena_randomf(-0.1f, 0.1f), z + athena_randomf(-0.1f, 0.1f),
                       d.x, d.y * 0.6f, d.z, athena_randomi(22, big ? 40 : 32), athena_randomf(0.55f, big ? 1.4f : 1.0f));
    }
}

static void spawn_fx(float x, float y, float z, bool big) {
    for (int i = 0; i < MAX_FX; i++) {
        Fx *f = &fx[i];
        if (f->alive)
            continue;
        f->alive = true;
        f->x = x; f->y = y; f->z = z;
        f->max_life = big ? 30 : 20;
        f->life = f->max_life;
        f->base_sc = big ? 0.45f : 0.28f;
        f->spin = athena_randomf(-0.25f, 0.25f);
        spawn_explosion_particles(x, y, z, big);
        if (big)
            shake = fmaxf(shake, 0.55f);
        return;
    }
}

// The ship pivots about its own origin (z = 0), so anything mounted down the
// tail sweeps an arc in Y as the ship pitches: a point (0,0,-d) goes to
// y = d*sin(pitch), z = -d*cos(pitch).
static V3 engine_mount(float d) { return (V3){ 0.0f, d * sinf(pitch), -d * cosf(pitch) }; }

static void spawn_boost_particles(float sx, float sy, float sz, float boost_power) {
    const int count = boost_power > 0.5f ? 3 : 1;
    for (int n = 0; n < count; n++) {
        spawn_particle(P_BOOST,
                       sx + athena_randomf(-0.12f, 0.12f), sy + athena_randomf(-0.08f, 0.08f), sz + athena_randomf(-0.05f, 0.05f),
                       athena_randomf(-0.04f, 0.04f), athena_randomf(-0.03f, 0.03f),
                       -athena_randomf(0.35f, 0.75f) - boost_power * 0.4f,
                       athena_randomi(8, 16), athena_randomf(0.5f, 1.1f) + boost_power * 0.5f);
    }
}

static bool hit_r(float ax, float ay, float az, float bx, float by, float bz, float rr) {
    const float dx = ax - bx, dy = ay - by, dz = az - bz;
    return (dx * dx + dy * dy + dz * dz) < rr * rr;
}

static bool hit(float ax, float ay, float az, float ar, float bx, float by, float bz, float br) {
    return hit_r(ax, ay, az, bx, by, bz, ar + br);
}

// A fast bullet can skip clean over a small rock in one frame: sweep the step.
static float hit_bullet_along_z(float bx, float by, float bz, float prev_z, float rr, float ex, float ey, float ez) {
    const int steps = 6;
    for (int s = 0; s <= steps; s++) {
        const float z = (s == steps) ? bz : lerpf(prev_z, bz, s / (float)steps);
        if (hit_r(bx, by, z, ex, ey, ez, rr))
            return z;
    }
    return -1.0f;
}

static float fx_scale(float life, float max_life, float base_sc) {
    const float prog = 1.0f - life / max_life;
    const float pop = sinf(prog * 3.141592f);
    return base_sc * (0.35f + pop * 2.4f) * (1.0f - prog * 0.35f);
}

// Planet billboards: rotate a +Z-facing hemisphere so its pole looks at the camera.
static float billboard_yaw(float px, float pz, float cx, float cz) { return atan2f(cx - px, cz - pz); }

static float billboard_pitch(float px, float py, float pz, float cx, float cy, float cz) {
    const float dx = cx - px, dy = cy - py, dz = cz - pz;
    return atan2f(-dy, sqrtf(dx * dx + dz * dz));
}

static void step_particles(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &particles[i];
        if (p->life <= 0.0f)
            continue;
        p->life -= 1.0f;
        p->x += p->vx;
        p->y += p->vy;
        p->z += p->vz;

        switch (p->kind) {
            case P_BOOST:
                p->vz -= 0.02f;
                p->sc += (0.15f - p->sc) * 0.12f;
                break;
            case P_SPARK:
                p->vx *= 0.90f;
                p->vy = p->vy * 0.90f - 0.008f;
                p->vz *= 0.90f;
                p->sc += (0.08f - p->sc) * 0.10f;
                break;
            case P_EMBER:
                p->vx *= 0.94f;
                p->vy = p->vy * 0.94f - 0.012f;
                p->vz *= 0.94f;
                p->sc += (0.04f - p->sc) * 0.06f;
                break;
            case P_FLASH:
                p->vx *= 0.86f;
                p->vy *= 0.86f;
                p->vz *= 0.86f;
                p->sc += (0.02f - p->sc) * 0.18f;
                break;
            default:
                p->vx *= 0.96f;
                p->vy = p->vy * 0.96f + 0.006f;
                p->vz *= 0.96f;
                p->sc += (1.6f - p->sc) * 0.04f;
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
static void update_world(float scroll_speed) {
    // Distant stars stay far away and slide sideways (parallax), never approaching the camera.
    for (int i = 0; i < STAR_COUNT; i++) {
        Star *s = &stars[i];
        s->x -= scroll_speed * s->par;
        if (s->x < -STAR_SPREAD_X) {
            s->x += STAR_SPREAD_X * 2.0f;
            s->y = athena_randomf(-STAR_SPREAD_Y, STAR_SPREAD_Y);
            s->z = athena_randomf(STAR_Z_NEAR, STAR_Z_FAR);
        }
    }
    for (int i = 0; i < MAX_PLANETS; i++) {
        planets[i].z -= scroll_speed * 0.35f;
        if (planets[i].z < 60.0f) {
            planets[i].z += 260.0f;
            planets[i].x = PLANET_DEFS[i].x + athena_randomf(-8.0f, 8.0f);
            planets[i].y = PLANET_DEFS[i].y + athena_randomf(-2.5f, 2.5f);
        }
    }
}

static void update_play(void) {
    const float ix = stick(pad->lx);
    const float iy = stick(pad->ly);
    const bool boosting = athena_pad_pressed(pad, PAD_R2);
    const bool braking = athena_pad_pressed(pad, PAD_L2);

    float target_speed = BASE_SPEED * run_stats.speed + wave * 0.03f;
    if (boosting) target_speed *= 2.1f;
    else if (braking) target_speed *= 0.45f;
    speed = lerpf(speed, target_speed, 0.08f);

    vel_x = lerpf(vel_x, -ix * SHIP_ACCEL, 0.35f);
    vel_y = lerpf(vel_y, iy * SHIP_ACCEL * 0.85f, 0.35f);
    ship_x = clampf(ship_x + vel_x, -BOUNDS_X, BOUNDS_X);
    ship_y = clampf(ship_y + vel_y, BOUNDS_Y_LOW, BOUNDS_Y_HIGH);

    roll = lerpf(roll, ix * 0.7f, 0.2f);
    pitch = lerpf(pitch, iy * 0.35f, 0.2f);

    if (fire_timer > 0)
        fire_timer--;
    if ((athena_pad_pressed(pad, PAD_CROSS) || athena_pad_pressed(pad, PAD_R1)) && fire_timer == 0) {
        fire_bullet();
        fire_timer = run_stats.cooldown;
    }

    update_world(speed);

    const float boost_power = clampf((speed - BASE_SPEED) / (BASE_SPEED * 1.2f), 0.0f, 1.0f);
    if (boosting && state == STATE_PLAY) {
        const V3 m = engine_mount(1.35f);
        spawn_boost_particles(ship_x, ship_y + m.y, m.z, boost_power);
    }

    spawn_timer--;
    if (spawn_timer <= 0) {
        spawn_enemy();
        spawn_timer = maxi(14, 42 - wave * 2);
    }
    if (frame % 900 == 0 && frame > 0)
        wave++;

    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &bullets[i];
        if (!b->alive)
            continue;
        b->pz = b->z;
        b->z += 3.4f;
        if (b->z > 135.0f)
            b->alive = false;
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (!e->active)
            continue;

        e->z -= speed * (e->type == 1 ? 0.7f : 1.0f) + 0.15f;
        e->phase += e->weave;
        e->x += sinf(e->phase) * 0.04f;
        e->ry += e->spin;
        e->rx += e->spin * 0.5f;

        if (e->z < -8.0f) {
            e->active = false;
            continue;
        }

        if (invuln <= 0 && hit(ship_x, ship_y, 0.0f, 0.8f, e->x, e->y, e->z, e->radius)) {
            e->active = false;
            lives--;
            invuln = 90;
            shake = 1.0f;
            spawn_fx(ship_x, ship_y, 0.5f, true);
            if (lives <= 0) {
                award_run_credits();
                state = STATE_OVER;
                return;
            }
            continue;
        }

        for (int j = 0; j < MAX_BULLETS; j++) {
            Bullet *bd = &bullets[j];
            if (!bd->alive)
                continue;

            const float bx = bd->x, by = bd->y;
            float bz = bd->z;
            float hit_z = -1.0f;

            if (e->type == 1) {
                const float br = 0.55f;
                const float er = e->radius + 0.25f;
                hit_z = hit_bullet_along_z(bx, by, bz, bd->pz, br + er, e->x, e->y, e->z);
            } else if (hit(bx, by, bz, 0.35f, e->x, e->y, e->z, e->radius)) {
                hit_z = bz;
            }

            if (hit_z < 0.0f)
                continue;

            bz = hit_z;
            const int damage = bd->damage ? bd->damage : 1;
            bd->alive = false;

            if (e->type == 1) {
                const int removed = vox_carve_world(&e->voxel, bx, by, bz, e->x, e->y, e->z, e->sc, e->rx, e->ry, e->rz, damage);
                spawn_fx(bx, by, bz, false);
                score += removed * 8;
                e->radius = vox_collision_radius(&e->voxel) * e->sc;

                if (e->voxel.voxel_count <= VOXEL_DESTROY_AT) {
                    e->active = false;
                    spawn_fx(e->x, e->y, e->z, true);
                    score += 60;
                }
            } else {
                e->hp -= damage;
                if (e->hp <= 0) {
                    e->active = false;
                    spawn_fx(e->x, e->y, e->z, false);
                    score += 100;
                } else {
                    spawn_fx(bx, by, bz, false);
                }
            }
            break;
        }
    }

    for (int i = 0; i < MAX_FX; i++) {
        if (!fx[i].alive)
            continue;
        if (--fx[i].life <= 0)
            fx[i].alive = false;
    }

    step_particles();

    if (invuln > 0)
        invuln--;
    frame++;
}

// ---------------------------------------------------------------------------
// Hangar hub
// ---------------------------------------------------------------------------
static int hangar_list_count(void) {
    switch (hang_tab) {
        case 0: return SHIP_COUNT;
        case 1: return PARTS_PER_SLOT;
        case 2: return COLOR_COUNT;
        case 3: return UPGRADE_COUNT;
        default: return 0;
    }
}

static void hangar_buy_ship(int id) {
    if (!(save.owned_ships & (1u << id))) {
        if (save.credits < SHIPS[id].price)
            return;
        save.credits -= SHIPS[id].price;
        save.owned_ships |= 1u << id;
    }
    save.loadout.ship = id;
    rebuild_ship();
    save_game();
}

static void hangar_buy_part(int slot, int id) {
    if (!(save.owned_parts[slot] & (1u << id))) {
        if (save.credits < PARTS[slot][id].price)
            return;
        save.credits -= PARTS[slot][id].price;
        save.owned_parts[slot] |= 1u << id;
    }
    save.loadout.part[slot] = id;
    rebuild_ship();
    save_game();
}

static void hangar_buy_color(int id) {
    if (!(save.owned_colors & (1u << id))) {
        if (save.credits < COLORS[id].price)
            return;
        save.credits -= COLORS[id].price;
        save.owned_colors |= 1u << id;
    }
    save.loadout.color = id;
    rebuild_ship();
    save_game();
}

static void hangar_buy_upgrade(int key) {
    const int lv = save.upgrades[key];
    if (lv >= UPGRADE_DEFS[key].max)
        return;
    const int cost = upgrade_cost(key, lv);
    if (save.credits < cost)
        return;
    save.credits -= cost;
    save.upgrades[key]++;
    save_game();
}

static void hangar_confirm(void) {
    switch (hang_tab) {
        case 0: hangar_buy_ship(hang_sel); break;
        case 1: hangar_buy_part(part_slot, hang_sel); break;
        case 2: hangar_buy_color(hang_sel); break;
        case 3: hangar_buy_upgrade(hang_sel); break;
        case 4:
            reset_game();
            state = STATE_PLAY;
            break;
    }
}

static void update_hangar(void) {
    if (hang_nav_lock > 0)
        hang_nav_lock--;

    if (athena_pad_just_pressed(pad, PAD_L1)) {
        hang_tab = (hang_tab + HANGAR_TAB_COUNT - 1) % HANGAR_TAB_COUNT;
        hang_sel = 0;
        hang_nav_lock = 12;
    }
    if (athena_pad_just_pressed(pad, PAD_R1)) {
        hang_tab = (hang_tab + 1) % HANGAR_TAB_COUNT;
        hang_sel = 0;
        hang_nav_lock = 12;
    }

    if (hang_tab == 1) {
        if (athena_pad_just_pressed(pad, PAD_LEFT)) {
            part_slot = (part_slot + PART_SLOT_COUNT - 1) % PART_SLOT_COUNT;
            hang_sel = 0;
            hang_nav_lock = 12;
        }
        if (athena_pad_just_pressed(pad, PAD_RIGHT)) {
            part_slot = (part_slot + 1) % PART_SLOT_COUNT;
            hang_sel = 0;
            hang_nav_lock = 12;
        }
    }

    const int count = hangar_list_count();
    if (count > 0 && hang_nav_lock == 0) {
        if (athena_pad_just_pressed(pad, PAD_UP)) {
            hang_sel = (hang_sel + count - 1) % count;
            hang_nav_lock = 10;
        }
        if (athena_pad_just_pressed(pad, PAD_DOWN)) {
            hang_sel = (hang_sel + 1) % count;
            hang_nav_lock = 10;
        }
    }

    if (athena_pad_just_pressed(pad, PAD_CROSS))
        hangar_confirm();
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------
static AthenaFont *font;

static void print(float x, float y, Color color, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
static void print(float x, float y, Color color, const char *fmt, ...) {
    char text[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    athena_font_set_color(font, color);
    athena_font_print(font, x, y, text);
}

// A float as the script prints it: the shortest digits that read back the same.
static const char *num(float v, char *buf, size_t size) {
    for (int prec = 1; prec <= 9; prec++) {
        snprintf(buf, size, "%.*g", prec, v);
        if (strtof(buf, NULL) == v)
            break;
    }
    return buf;
}

static void draw_hangar_ui(void) {
    const float px = floorf(CW * 0.42f);
    const float pw = CW - px - 10;

    athena_draw_rect(0, 0, px, CH, athena_color_rgba(0, 0, 0, 40));
    athena_draw_rect(px, 30, pw, CH - 38, athena_color_rgba(16, 22, 38, 200));

    float tx = px + 6;
    for (int t = 0; t < HANGAR_TAB_COUNT; t++) {
        print(tx, 8, t == hang_tab ? C_YELLOW : C_DIM, "%s", HANGAR_TABS[t]);
        tx += 86;
    }

    print(px + 8, 38, C_CYAN, "CREDITS %d", save.credits);
    print(px + 8, 56, C_DIM, "BEST %d", save.best);

    if (hang_tab == 4) {
        const Stats rs = compute_stats(&save);
        print(px + 8, 88, C_WHITE, "LAUNCH RUN");
        print(px + 8, 112, C_WHITE, "%s / %s", SHIPS[save.loadout.ship].name, COLORS[save.loadout.color].name);
        char spd[16];
        print(px + 8, 136, C_DIM, "Lives %d  Dmg %d  Spd x%s", rs.lives, rs.damage, num(floorf(rs.speed * 100) / 100, spd, sizeof(spd)));
        print(px + 8, 156, C_DIM, "Shots %d  Rate %df", rs.bullets, rs.cooldown);
        print(px + 8, 190, C_YELLOW, "CROSS = Launch");
        print(px + 8, 212, C_DIM, "L1/R1 tabs  TRIANGLE quit");
        return;
    }

    const float list_y = 78, row_h = 22;
    const int max_rows = (int)floorf((CH - list_y - 16) / row_h);

    if (hang_tab == 1)
        print(px + 8, list_y - 18, C_WHITE, "Slot: %s  < >", PART_SLOTS_UPPER[part_slot]);

    for (int i = 0; i < max_rows && i < hangar_list_count(); i++) {
        const float y = list_y + i * row_h;
        const bool sel = i == hang_sel;

        if (sel)
            athena_draw_rect(px + 4, y - 2, pw - 8, row_h - 2, athena_color_rgba(40, 60, 100, 160));

        char label[48] = "";
        int price = 0;
        bool owned = false, equipped = false;

        if (hang_tab == 0) {
            snprintf(label, sizeof(label), "%s", SHIPS[i].name);
            price = SHIPS[i].price;
            owned = save.owned_ships & (1u << i);
            equipped = save.loadout.ship == i;
        } else if (hang_tab == 1) {
            const Part *p = &PARTS[part_slot][i];
            snprintf(label, sizeof(label), "%s", p->name);
            price = p->price;
            owned = save.owned_parts[part_slot] & (1u << i);
            equipped = save.loadout.part[part_slot] == i;
        } else if (hang_tab == 2) {
            snprintf(label, sizeof(label), "%s", COLORS[i].name);
            price = COLORS[i].price;
            owned = save.owned_colors & (1u << i);
            equipped = save.loadout.color == i;
        } else if (hang_tab == 3) {
            const int lv = save.upgrades[i];
            snprintf(label, sizeof(label), "%s %d/%d", UPGRADE_DEFS[i].name, lv, UPGRADE_DEFS[i].max);
            price = lv >= UPGRADE_DEFS[i].max ? 0 : upgrade_cost(i, lv);
            owned = lv >= UPGRADE_DEFS[i].max;
        }

        print(px + 10, y + 2, sel ? C_WHITE : C_DIM, "%s", label);

        if (equipped)
            print(px + pw - 72, y + 2, C_GREEN, "EQUIPPED");
        else if (hang_tab == 3 && owned)
            print(px + pw - 52, y + 2, C_GREEN, "MAX");
        else if (owned)
            print(px + pw - 52, y + 2, C_CYAN, "OWNED");
        else
            print(px + pw - 52, y + 2, save.credits >= price ? C_YELLOW : C_RED, "%dc", price);
    }

    print(px + 8, CH - 22, C_DIM, "CROSS buy/equip  L1/R1 tab");
}

static void draw_backdrop(void) {
    // Subtle vignette only — planets are 3D now.
    athena_draw_rect(0, 0, CW, 28, athena_color_rgba(0, 0, 0, 80));
    athena_draw_rect(0, CH - 28, CW, 28, athena_color_rgba(0, 0, 0, 80));
}

static void draw_lives(void) {
    for (int i = 0; i < lives; i++) {
        const float x = CW - 40 - i * 26, y = 26;
        athena_draw_triangle(x, y + 14, x + 10, y + 14, x + 5, y, C_GREEN);
    }
}

static void draw_boost(bool boosting) {
    const float bw = 120, x = 16, y = CH - 26;
    athena_draw_rect(x, y, bw, 10, athena_color_rgba(30, 40, 60, 128));
    const float frac = clampf((speed - BASE_SPEED * 0.4f) / (BASE_SPEED * 2.1f), 0.0f, 1.0f);
    athena_draw_rect(x, y, bw * frac, 10, boosting ? C_YELLOW : C_CYAN);
    print(x, y - 20, C_DIM, "SPEED");
}

static void draw_hud(void) {
    print(16, 14, C_WHITE, "SCORE %d", score);
    print(16, 34, C_CYAN, "WAVE %d", wave);

    if (state == STATE_MENU) {
        print(CW * 0.5f - 70, 150, C_WHITE, "S T A R   F O X");
        print(CW * 0.5f - 95, 190, C_YELLOW, "Press CROSS for Hangar");
        print(CW * 0.5f - 130, 230, C_DIM, "Left Stick: steer   CROSS/R1: fire");
        print(CW * 0.5f - 130, 250, C_DIM, "R2: boost   L2: brake   TRIANGLE: quit");
    } else if (state == STATE_HANGAR) {
        draw_hangar_ui();
    } else if (state == STATE_OVER) {
        print(CW * 0.5f - 55, 140, C_RED, "RUN OVER");
        print(CW * 0.5f - 65, 170, C_WHITE, "Score %d  Wave %d", score, wave);
        print(CW * 0.5f - 75, 195, C_CYAN, "+%d credits", run_earned);
        print(CW * 0.5f - 55, 218, C_DIM, "Bank %d", save.credits);
        print(CW * 0.5f - 85, 245, C_YELLOW, "CROSS = Hangar");
    } else {
        draw_lives();
        draw_boost(athena_pad_pressed(pad, PAD_R2));
        print(CW - 110, 34, C_CYAN, "%dc", save.credits);
    }

    char fps[16];
    print(CW - 96, 14, C_DIM, "%s FPS", num(athena_screen_get_fps(60), fps, sizeof(fps)));
}

// ---------------------------------------------------------------------------
// Render helpers
// ---------------------------------------------------------------------------
// The transform setters of the script: each rebuilds the object matrix.
static void place(AthenaRenderObject *ro, float x, float y, float z) {
    athena_object_data *o = athena_render_object_native(ro);
    o->position[0] = x; o->position[1] = y; o->position[2] = z;
    update_object_space(o);
}

static void turn(AthenaRenderObject *ro, float x, float y, float z) {
    athena_object_data *o = athena_render_object_native(ro);
    o->rotation[0] = x; o->rotation[1] = y; o->rotation[2] = z;
    update_object_space(o);
}

static void scale(AthenaRenderObject *ro, float x, float y, float z) {
    athena_object_data *o = athena_render_object_native(ro);
    o->scale[0] = x; o->scale[1] = y; o->scale[2] = z;
    update_object_space(o);
}

static void sync_scene(float cam_x, float cam_y, float cam_z) {
    for (int i = 0; i < STAR_COUNT; i++) {
        const Star *s = &stars[i];
        place(s->obj, s->x, s->y, s->z);
        scale(s->obj, s->sc, s->sc, s->sc);
    }
    for (int i = 0; i < MAX_PLANETS; i++) {
        const Planet *p = &planets[i];
        place(p->obj, p->x, p->y, p->z);
        scale(p->obj, p->sc, p->sc, p->sc);
        turn(p->obj, billboard_pitch(p->x, p->y, p->z, cam_x, cam_y, cam_z), billboard_yaw(p->x, p->z, cam_x, cam_z), 0.0f);
    }
    for (int i = 0; i < MAX_BULLETS; i++)
        if (bullets[i].alive)
            place(bullets[i].obj, bullets[i].x, bullets[i].y, bullets[i].z);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &enemies[i];
        if (!e->active)
            continue;
        AthenaRenderObject *o = e->type == 1 ? e->rock : e->fighter;
        place(o, e->x, e->y, e->z);
        turn(o, e->rx, e->ry, e->rz);
        if (e->type == 1)
            scale(o, e->sc, e->sc, e->sc);
    }
    for (int i = 0; i < MAX_FX; i++) {
        const Fx *f = &fx[i];
        if (!f->alive)
            continue;
        const float prog = 1.0f - (float)f->life / f->max_life;
        const float sc = fx_scale(f->life, f->max_life, f->base_sc);
        place(f->obj, f->x, f->y, f->z);
        turn(f->obj, prog * 0.8f, f->spin + prog * 1.6f, 0.0f);
        scale(f->obj, sc, sc, sc);
    }
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &particles[i];
        if (p->life <= 0.0f)
            continue;
        const float fade = p->life / p->max_life;
        const float ease = fade * fade;
        const float s = p->sc * ease;
        const float age = 1.0f - fade;
        place(p->obj, p->x, p->y, p->z);

        switch (p->kind) {
            case P_BOOST:
                turn(p->obj, pitch, 0.0f, roll);
                scale(p->obj, s * 0.7f, s * 0.7f, s * 1.8f);
                break;
            case P_SPARK:
                turn(p->obj, age * 2.1f + p->spin, age * 3.2f, p->spin * 0.5f);
                scale(p->obj, s * 0.35f, s * 0.35f, s * 2.8f);
                break;
            case P_EMBER:
                turn(p->obj, age * 1.4f, age * 2.0f + p->spin, p->spin);
                scale(p->obj, s, s, s);
                break;
            case P_FLASH:
                turn(p->obj, age * 4.0f, age * 5.0f, age * 2.0f);
                scale(p->obj, s * 1.2f, s * 1.2f, s * 1.2f);
                break;
            default:
                turn(p->obj, age * 0.6f, age * 0.9f + p->spin, 0.0f);
                scale(p->obj, s * 1.1f, s * 1.1f, s * 0.85f);
                break;
        }
    }
}

static void render_scene(bool show_ship) {
    for (int i = 0; i < MAX_PLANETS; i++) athena_render_object_draw(planets[i].obj);
    for (int i = 0; i < STAR_COUNT; i++) athena_render_object_draw(stars[i].obj);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &enemies[i];
        if (e->active)
            athena_render_object_draw(e->type == 1 ? e->rock : e->fighter);
    }
    for (int i = 0; i < MAX_BULLETS; i++)
        if (bullets[i].alive) athena_render_object_draw(bullets[i].obj);
    for (int i = 0; i < MAX_FX; i++)
        if (fx[i].alive) athena_render_object_draw(fx[i].obj);
    for (int i = 0; i < MAX_PARTICLES; i++)
        if (particles[i].life > 0.0f) athena_render_object_draw(particles[i].obj);

    if (show_ship) {
        if (state == STATE_PLAY) {
            const float pulse = 0.75f + sinf(frame * 0.6f) * 0.2f + (athena_pad_pressed(pad, PAD_R2) ? 0.6f : 0.0f);
            const V3 m = engine_mount(1.15f);
            place(thruster, ship_x, ship_y + m.y, m.z);
            turn(thruster, pitch, 0.0f, roll);
            scale(thruster, 1.0f, 1.0f, pulse);
            athena_render_object_draw(thruster);
        }
        athena_render_object_draw(ship);
    }
}

// ---------------------------------------------------------------------------
// Setup and main loop
// ---------------------------------------------------------------------------
static void init_scene(void) {
    thruster = athena_render_object_create(build_thruster());
    AthenaRenderData *enemy_mesh = build_enemy();
    AthenaRenderData *bullet_mesh = build_bullet();
    AthenaRenderData *explosion_mesh = build_explosion();
    AthenaRenderData *star_mesh = build_star();
    AthenaRenderData *particle_mesh[5];
    for (int k = 0; k < 5; k++)
        particle_mesh[k] = build_particle(k);

    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i].obj = athena_render_object_create(star_mesh);
        stars[i].x = athena_randomf(-STAR_SPREAD_X, STAR_SPREAD_X);
        stars[i].y = athena_randomf(-STAR_SPREAD_Y, STAR_SPREAD_Y);
        stars[i].z = athena_randomf(STAR_Z_NEAR, STAR_Z_FAR);
        stars[i].par = athena_randomf(0.04f, 0.16f);
        stars[i].sc = athena_randomf(0.35f, 0.95f);
    }

    for (int i = 0; i < MAX_PLANETS; i++) {
        const PlanetDef *d = &PLANET_DEFS[i];
        planets[i].obj = athena_render_object_create(build_planet_dome(d->br, d->bg, d->bb, d->hr, d->hg, d->hb));
        planets[i].x = d->x;
        planets[i].y = d->y;
        planets[i].z = d->z;
        planets[i].sc = d->sc;
    }

    // Slots are dedicated to one kind each: 18 boost, 22 spark, 14 ember, 12 flash, 6 smoke.
    for (int i = 0; i < MAX_PARTICLES; i++) {
        int kind = P_BOOST;
        if (i >= 18 && i < 40) kind = P_SPARK;
        else if (i >= 40 && i < 54) kind = P_EMBER;
        else if (i >= 54 && i < 66) kind = P_FLASH;
        else if (i >= 66) kind = P_SMOKE;
        particles[i].obj = athena_render_object_create(particle_mesh[kind]);
        particles[i].kind = kind;
        particles[i].max_life = 1;
    }

    for (int i = 0; i < MAX_BULLETS; i++)
        bullets[i].obj = athena_render_object_create(bullet_mesh);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        vox_reset(&e->voxel, 1000 + i * 7919);
        e->fighter = athena_render_object_create(enemy_mesh);
        e->rock = athena_render_object_create(e->voxel.mesh);
        e->hp = 1;
        e->radius = 0.9f;
        e->sc = 1.0f;
    }

    for (int i = 0; i < MAX_FX; i++) {
        fx[i].obj = athena_render_object_create(explosion_mesh);
        fx[i].max_life = 1;
        fx[i].base_sc = 1.0f;
    }
}

static int starfox_main(int argc, char **argv) {
    (void)argc; (void)argv;

    SPACE = athena_color_rgba(6, 8, 26, 128);
    C_WHITE = athena_color_rgba(230, 240, 255, 128);
    C_CYAN = athena_color_rgba(120, 220, 255, 128);
    C_YELLOW = athena_color_rgba(255, 214, 90, 128);
    C_RED = athena_color_rgba(255, 90, 90, 128);
    C_GREEN = athena_color_rgba(120, 255, 150, 128);
    C_DIM = athena_color_rgba(120, 140, 180, 128);

    font = athena_font_load("default");
    athena_font_set_scale(font, 0.6f);
    athena_font_set_outline(font, 1.0f, athena_color_rgba(0, 0, 0, 128));

    athena_screen_set_frame_counter(true);
    athena_screen_set_vsync(true);

    AthenaVideoMode mode;
    athena_screen_get_mode(&mode);
    mode.zbuffering = true;
    mode.psm = GS_PSM_CT32;
    mode.psmz = GS_ZBUF_24;
    athena_screen_set_mode(&mode);
    athena_screen_init_buffers();

    athena_render_module_init();
    athena_render_module_set_view(72.0f, 0.5f, 900.0f, 0, 0);

    CW = mode.width;
    CH = mode.height;

    size_t n = strlen(boot_path);
    if (n && boot_path[n - 1] == '/')
        n--;
    snprintf(save_path, sizeof(save_path), "%.*s/starfox_save.json", (int)n, boot_path);

    load_save();
    ship_cache = NULL;
    ship_cache_count = ship_cache_cap = 0;
    rebuild_ship();
    init_scene();

    state = STATE_MENU;
    score = 0;
    lives = 3;
    wave = 1;
    fire_timer = 0;
    spawn_timer = 60;
    invuln = 0;
    fire_side = 1;
    shake = 0.0f;
    frame = 0;
    menu_spin = 0.0f;
    run_stats = compute_stats(&save);
    run_earned = 0;
    hang_tab = hang_sel = part_slot = hang_nav_lock = 0;
    ship_x = ship_y = vel_x = vel_y = roll = pitch = 0.0f;
    speed = BASE_SPEED;

    pad = athena_pad_open(0);

    athena_screen_set_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);

    // Camera target pans by delta each call — set once, never per frame.
    athena_camera3d_position(CAM_X, CAM_Y, CAM_Z);
    athena_camera3d_target(LOOK_X, LOOK_Y, LOOK_Z);
    athena_camera3d_update();

    for (;;) {
        athena_pad_update(pad);

        if (shake > 0.0f)
            shake -= 0.06f;

        if (athena_pad_just_pressed(pad, PAD_TRIANGLE))
            break;

        if (state == STATE_MENU) {
            menu_spin += 0.02f;
            update_world(BASE_SPEED * 0.4f);
            if (athena_pad_just_pressed(pad, PAD_CROSS) || athena_pad_just_pressed(pad, PAD_START)) {
                state = STATE_HANGAR;
                hang_tab = hang_sel = part_slot = 0;
                rebuild_ship();
            }
        } else if (state == STATE_HANGAR) {
            menu_spin += 0.02f;
            update_world(BASE_SPEED * 0.25f);
            update_hangar();
        } else if (state == STATE_OVER) {
            update_world(BASE_SPEED * 0.4f);
            if (athena_pad_just_pressed(pad, PAD_CROSS) || athena_pad_just_pressed(pad, PAD_START)) {
                state = STATE_HANGAR;
                hang_tab = 4;
                hang_sel = 0;
                rebuild_ship();
            }
        } else {
            update_play();
        }

        // Ship transform
        if (state == STATE_PLAY) {
            place(ship, ship_x, ship_y, 0.0f);
            turn(ship, pitch, 0.0f, roll);
        } else {
            place(ship, 0.0f, 0.0f, 0.0f);
            turn(ship, 0.0f, menu_spin, 0.0f);
        }

        // Fixed chase camera — only nudge position for hit-shake.
        float sx = 0.0f, sy = 0.0f;
        if (shake > 0.0f) {
            sx = athena_randomf(-shake, shake) * 0.6f;
            sy = athena_randomf(-shake, shake) * 0.6f;
        }
        const float cam_x = CAM_X + sx, cam_y = CAM_Y + sy, cam_z = CAM_Z;

        sync_scene(cam_x, cam_y, cam_z);

        athena_screen_set_param(DEPTH_TEST_ENABLE, false);
        athena_screen_clear(SPACE);
        draw_backdrop();

        athena_screen_set_param(DEPTH_TEST_ENABLE, true);
        athena_screen_set_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);
        athena_render_module_begin();

        athena_camera3d_position(cam_x, cam_y, cam_z);
        athena_camera3d_update();

        const bool show_ship = state != STATE_PLAY || invuln <= 0 || (invuln / 6) % 2 == 0;
        render_scene(show_ship);

        athena_screen_set_param(DEPTH_TEST_ENABLE, false);
        draw_hud();
        athena_screen_flip();
    }

    athena_font_destroy(font);
    return 0;
}

ATHENAOS_APP(starfox, starfox_main)
