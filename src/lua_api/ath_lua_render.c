#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <kernel.h>

#include "ath_lua.h"
#include "ath_lua_graphics.h"
#include "ath_lua_math.h"
#include "ath_lua_render.h"

#include <render.h>
#include <lz4_ps2.h>
#include <athena/render_facade.h>
#include <athena/anim3d.h>
#include <athena/lights.h>
#include <athena/camera3d.h>

#ifdef ATHENA_PHYSICS
#include <physics.h>
#include "ath_lua_physics.h"
#endif

/*
 * 3D rendering. Mirrors js_api/ath_render.c; the differences are Lua-isms:
 *  - vertex attributes are Buffers (16 bytes per vertex) or flat number
 *    tables (x, y, z, w per vertex); `vertices` returns Buffer views;
 *  - lists (materials, bones, textures, bounds) are 1-based tables;
 *  - AnimCollection is indexed 1..n or by name and yields Animation handles.
 * Object lifetimes are kept with athena_lua_retain instead of JS refcounts.
 */

#define RENDERDATA_CLASS "RenderData"
#define BATCH_CLASS "Batch"
#define SCENENODE_CLASS "SceneNode"
#define LOADER_CLASS "AsyncLoader"
#define ANIMCOL_CLASS "AnimCollection"
#define ANIM_CLASS "Animation"

typedef struct {
    AthenaRenderData *native;
} LuaRenderData;

typedef struct {
    AthenaRenderObject *native;
} LuaRenderObject;

typedef struct {
    AthenaRenderBatch *native;
} LuaBatch;

typedef struct {
    athena_scene_node *node;
} LuaSceneNode;

typedef struct {
    AthenaAsyncLoader *native;
    uint32_t jobs_per_step;
} LuaLoader;

typedef struct {
    AthenaAnim3dCollection *collection;
} LuaAnimCollection;

typedef struct {
    athena_animation *anim;
} LuaAnim;

static const char *vert_attributes[] = { "positions", "normals", "texcoords", "colors" };

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ------------------------------------------------------------ helpers -- */

static AthenaRenderData *check_rd(lua_State *L, int idx)
{
    LuaRenderData *rd = athena_lua_check_object(L, idx, RENDERDATA_CLASS);
    if (!rd->native)
        luaL_error(L, "RenderData was freed");
    return rd->native;
}

AthenaRenderObject *athena_lua_check_render_object(lua_State *L, int idx)
{
    LuaRenderObject *ro = athena_lua_check_object(L, idx, ATHENA_LUA_RENDEROBJECT);
    if (!ro->native)
        luaL_error(L, "RenderObject was freed");
    return ro->native;
}

static athena_animation *check_anim(lua_State *L, int idx)
{
    if (lua_islightuserdata(L, idx))
        return lua_touserdata(L, idx);
    return ((LuaAnim *)athena_lua_check_object(L, idx, ANIM_CLASS))->anim;
}

static void push_xyz(lua_State *L, const float *v)
{
    lua_createtable(L, 0, 3);
    athena_lua_set_num(L, "x", v[0]);
    athena_lua_set_num(L, "y", v[1]);
    athena_lua_set_num(L, "z", v[2]);
}

/* Reads {x, y, z} (table or Vector3); missing fields keep their value. */
static void read_xyz(lua_State *L, int idx, float *v)
{
    AthenaVec3 *vec = athena_lua_test_vec3(L, idx);
    if (vec) {
        v[0] = vec->x;
        v[1] = vec->y;
        v[2] = vec->z;
        return;
    }
    luaL_checktype(L, idx, LUA_TTABLE);
    athena_lua_field_float(L, idx, "x", &v[0]);
    athena_lua_field_float(L, idx, "y", &v[1]);
    athena_lua_field_float(L, idx, "z", &v[2]);
}

static void push_rgb(lua_State *L, const float *v)
{
    lua_createtable(L, 0, 3);
    athena_lua_set_num(L, "r", v[0]);
    athena_lua_set_num(L, "g", v[1]);
    athena_lua_set_num(L, "b", v[2]);
}

static void read_rgb_field(lua_State *L, int idx, const char *key, float *v)
{
    if (lua_getfield(L, idx, key) == LUA_TTABLE) {
        int t = lua_gettop(L);
        athena_lua_field_float(L, t, "r", &v[0]);
        athena_lua_field_float(L, t, "g", &v[1]);
        athena_lua_field_float(L, t, "b", &v[2]);
        v[3] = 1.0f;
    }
    lua_pop(L, 1);
}

/* Like read_rgb_field but clamps to [0, 1] and zeroes missing channels
 * (updateMaterial semantics in JS). */
static void read_rgb_clamped(lua_State *L, int idx, const char *key, float *v)
{
    if (lua_getfield(L, idx, key) == LUA_TTABLE) {
        int t = lua_gettop(L);
        float r = 0, g = 0, b = 0;
        athena_lua_field_float(L, t, "r", &r);
        athena_lua_field_float(L, t, "g", &g);
        athena_lua_field_float(L, t, "b", &b);
        v[0] = clampf(r, 0, 1);
        v[1] = clampf(g, 0, 1);
        v[2] = clampf(b, 0, 1);
        v[3] = 1.0f;
    }
    lua_pop(L, 1);
}

/* Vertex attribute from a Buffer or a flat float table. Returns a fresh
 * 16-byte aligned copy (or the Buffer memory itself when share is set) and
 * the byte size. */
static void *read_attribute(lua_State *L, int idx, bool share, uint32_t *size, bool *shared)
{
    *shared = false;
    AthenaLuaBuffer *buf = athena_lua_test_object(L, idx, ATHENA_LUA_BUFFER);
    if (buf) {
        *size = buf->size;
        if (share && ((uintptr_t)buf->data & 15) == 0) {
            *shared = true;
            return buf->data;
        }
        void *copy = memalign(16, buf->size);
        memcpy(copy, buf->data, buf->size);
        return copy;
    }

    luaL_checktype(L, idx, LUA_TTABLE);
    lua_Integer n = luaL_len(L, idx);
    *size = (uint32_t)n * sizeof(float);
    float *data = memalign(16, *size ? *size : 16);
    for (lua_Integer i = 0; i < n; i++) {
        lua_geti(L, idx, i + 1);
        data[i] = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    return data;
}

static void clamp_material_indices(athena_render_data *m)
{
    if (!m->material_indices || m->material_index_count <= 0 || m->index_count == 0)
        return;
    uint32_t last = m->index_count - 1;
    for (int i = 0; i < m->material_index_count; i++)
        if (m->material_indices[i].end > last)
            m->material_indices[i].end = last;
}

static void apply_materials(lua_State *L, AthenaRenderData *rd, int idx)
{
    athena_render_data *m = &rd->m;
    int count = (int)luaL_len(L, idx);
    if (count == 0)
        return;

    if (count > m->material_count) {
        ath_mat *mats = memalign(16, count * sizeof(ath_mat));
        memset(mats, 0, count * sizeof(ath_mat));
        if (m->materials) {
            memcpy(mats, m->materials, m->material_count * sizeof(ath_mat));
            free(m->materials);
        }
        m->materials = mats;
    }

    for (int i = 0; i < count; i++) {
        lua_geti(L, idx, i + 1);
        int t = lua_gettop(L);
        ath_mat *mat = &m->materials[i];

        read_rgb_field(L, t, "ambient", mat->ambient);
        read_rgb_field(L, t, "diffuse", mat->diffuse);
        read_rgb_field(L, t, "emission", mat->emission);
        read_rgb_field(L, t, "transmittance", mat->transmittance);
        athena_lua_field_float(L, t, "shininess", &mat->shininess);
        athena_lua_field_float(L, t, "refraction", &mat->refraction);
        read_rgb_field(L, t, "transmission_filter", mat->transmission_filter);
        athena_lua_field_float(L, t, "disolve", &mat->disolve);

        mat->texture_id = mat->ref_texture_id = mat->bump_texture_id = mat->decal_texture_id = -1;
        athena_lua_field_int(L, t, "texture_id", &mat->texture_id);
        athena_lua_field_int(L, t, "ref_texture_id", &mat->ref_texture_id);
        athena_lua_field_int(L, t, "bump_texture_id", &mat->bump_texture_id);
        athena_lua_field_int(L, t, "decal_texture_id", &mat->decal_texture_id);

        if (mat->ref_texture_id != -1)
            m->attributes.has_refmap = true;
        if (mat->bump_texture_id != -1)
            m->attributes.has_bumpmap = true;
        if (mat->decal_texture_id != -1)
            m->attributes.has_decal = true;

        lua_pop(L, 1);
    }

    m->material_count = count;
}

/* Material groups are vertex ranges: {index = first, ["end"] = last} (inclusive). */
static void apply_material_indices(lua_State *L, AthenaRenderData *rd, int idx)
{
    athena_render_data *m = &rd->m;
    int count = (int)luaL_len(L, idx);
    if (count == 0)
        return;

    if (count > m->material_index_count)
        m->material_indices = realloc(m->material_indices, count * sizeof(material_index));

    for (int i = 0; i < count; i++) {
        lua_geti(L, idx, i + 1);
        athena_lua_field_u32(L, -1, "index", &m->material_indices[i].index);
        athena_lua_field_u32(L, -1, "end", &m->material_indices[i].end);
        lua_pop(L, 1);
    }

    m->material_index_count = count;
    clamp_material_indices(m);
}

/* Default single material for script-built geometry (see the JS binding for
 * why diffuse starts at 0). */
static void init_script_material(athena_render_data *m)
{
    m->materials = memalign(16, sizeof(ath_mat));
    memset(m->materials, 0, sizeof(ath_mat));
    m->material_count = 1;

    m->material_indices = malloc(sizeof(material_index));
    m->material_index_count = 1;

    ath_mat *mat = &m->materials[0];
    init_vector(mat->ambient);
    init_vector(mat->specular);
    init_vector(mat->emission);
    init_vector(mat->transmittance);
    init_vector(mat->transmission_filter);
    mat->shininess = 1.0f;
    mat->refraction = 1.0f;
    mat->disolve = 1.0f;
    mat->texture_id = mat->bump_texture_id = mat->decal_texture_id = mat->ref_texture_id = -1;

    m->attributes.has_decal = false;
    m->attributes.has_refmap = false;
    m->attributes.has_bumpmap = false;

    m->material_indices[0].index = 0;
    m->material_indices[0].end = m->index_count ? m->index_count - 1 : 0;
}

/* rd textures live in the object's field table as a 1-based `textures` table. */
static void push_textures_table(lua_State *L, int rd_idx)
{
    rd_idx = lua_absindex(L, rd_idx);
    lua_getfield(L, rd_idx, "textures");
    if (lua_istable(L, -1))
        return;
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, rd_idx, "textures");
}

static void wrap_native_textures(lua_State *L, int rd_idx, AthenaRenderData *rd)
{
    if (rd->m.texture_count <= 0 || !rd->m.textures)
        return;
    push_textures_table(L, rd_idx);
    for (int i = 0; i < rd->m.texture_count; i++) {
        AthenaImage *image = athena_image_wrap(rd->m.textures[i], true);
        if (!image)
            luaL_error(L, "out of memory");
        athena_lua_push_image(L, image, true);
        lua_rawseti(L, -2, i + 1);
    }
    lua_pop(L, 1);
}

static void push_bind_bones(lua_State *L, int rd_idx, AthenaRenderData *rd)
{
    athena_skeleton *sk = rd->m.skeleton;
    rd_idx = lua_absindex(L, rd_idx);
    lua_createtable(L, sk->bone_count, 0);
    for (int i = 0; i < sk->bone_count; i++) {
        lua_createtable(L, 0, 6);
        athena_lua_set_str(L, "name", sk->bones[i].name);
        athena_lua_set_int(L, "parent_id", sk->bones[i].parent_id);
        athena_lua_push_matrix_view(L, (float *)&sk->bones[i].inverse_bind, rd_idx);
        lua_setfield(L, -2, "inverse_bind");
        athena_lua_push_vec4_view(L, (float *)&sk->bones[i].position, rd_idx);
        lua_setfield(L, -2, "position");
        athena_lua_push_vec4_view(L, (float *)&sk->bones[i].rotation, rd_idx);
        lua_setfield(L, -2, "rotation");
        athena_lua_push_vec4_view(L, (float *)&sk->bones[i].scale, rd_idx);
        lua_setfield(L, -2, "scale");
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, rd_idx, "bones");
}

/* ---------------------------------------------------------- RenderData -- */

/* RenderData(path [, texture]) or RenderData(vertexList [, texture [, tristrip]]) */
static int l_rd_new(lua_State *L)
{
    lua_settop(L, 3); /* optional args must not alias the new object */
    LuaRenderData *ud = athena_lua_new_object(L, RENDERDATA_CLASS, sizeof(LuaRenderData));
    int self = lua_gettop(L);
    AthenaImage *image = lua_isnoneornil(L, 2) ? NULL : athena_lua_check_image(L, 2);

    if (lua_istable(L, 1)) {
        AthenaRenderData *rd = athena_render_data_create();
        if (!rd)
            return luaL_error(L, "out of memory");
        ud->native = rd;

        VECTOR **attrs[] = { &rd->m.positions, &rd->m.normals, &rd->m.texcoords, &rd->m.colours };
        bool share = false;
        athena_lua_field_bool(L, 1, "shareBuffers", &share);

        for (int i = 0; i < 4; i++) {
            if (lua_getfield(L, 1, vert_attributes[i]) != LUA_TNIL) {
                uint32_t size;
                bool shared;
                *attrs[i] = read_attribute(L, -1, share, &size, &shared);
                rd->owns_vertices[i] = !shared;
                if (shared)
                    athena_lua_retain(L, self, -1, NULL);
                if (i == 0)
                    rd->m.index_count = size / sizeof(VECTOR);
            }
            lua_pop(L, 1);
        }

        calculate_bbox(&rd->m);
        init_script_material(&rd->m);

        if (lua_getfield(L, 1, "materials") == LUA_TTABLE)
            apply_materials(L, rd, lua_gettop(L));
        lua_pop(L, 1);
        if (lua_getfield(L, 1, "material_indices") == LUA_TTABLE)
            apply_material_indices(L, rd, lua_gettop(L));
        lua_pop(L, 1);

        if (image) {
            image->tex->Filter = GS_FILTER_LINEAR;
            athena_render_data_set_texture(rd, 0, image->tex);
            if (rd->m.materials[0].texture_id == -1)
                rd->m.materials[0].texture_id = 0;
            push_textures_table(L, self);
            lua_pushvalue(L, 2);
            lua_rawseti(L, -2, 1);
            lua_pop(L, 1);
        }

        rd->m.tristrip = athena_lua_tobool(L, 3);
        rd->m.pipeline = PL_DEFAULT;
    } else {
        const char *path = luaL_checkstring(L, 1);
        if (image)
            image->tex->Filter = GS_FILTER_LINEAR;
        AthenaRenderData *rd = athena_render_data_load(path, image ? image->tex : NULL);
        if (!rd)
            return luaL_error(L, "could not load model '%s'", path);
        ud->native = rd;

        if (image) {
            push_textures_table(L, self);
            lua_pushvalue(L, 2);
            lua_rawseti(L, -2, 1);
            lua_pop(L, 1);
        } else {
            wrap_native_textures(L, self, rd);
        }
    }

    AthenaRenderData *rd = ud->native;
    rd->m.attributes.accurate_clipping = 1;
    rd->m.attributes.face_culling = CULL_FACE_BACK;
    rd->m.attributes.texture_mapping = 1;
    rd->m.attributes.shade_model = 1;

    FlushCache(WRITEBACK_DCACHE);

    if (rd->m.skin_data && rd->m.skeleton)
        push_bind_bones(L, self, rd);

    lua_settop(L, self);
    return 1;
}

/* Wraps a native render data produced by the async loader (ownership moves). */
static void push_rd_from_native(lua_State *L, athena_render_data *m)
{
    LuaRenderData *ud = athena_lua_new_object(L, RENDERDATA_CLASS, sizeof(LuaRenderData));
    int self = lua_gettop(L);
    AthenaRenderData *rd = athena_render_data_create();
    if (!rd) {
        free(m);
        luaL_error(L, "out of memory");
    }
    memcpy(&rd->m, m, sizeof(athena_render_data));
    free(m);
    ud->native = rd;

    athena_render_data_apply_defaults(rd);
    wrap_native_textures(L, self, rd);
    FlushCache(WRITEBACK_DCACHE);
}

static int l_rd_free(lua_State *L)
{
    LuaRenderData *ud = athena_lua_check_object(L, 1, RENDERDATA_CLASS);
    if (ud->native)
        athena_render_data_free(ud->native);
    ud->native = NULL;
    return 0;
}

static int l_rd_freeze(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    for (int i = 0; i < 4; i++) {
        if (!rd->owns_vertices[i]) {
            lua_pushboolean(L, false);
            return 1;
        }
    }
    athena_render_data_freeze(rd);
    lua_pushboolean(L, true);
    return 1;
}

/* setTexture(index, image) -- index is 0-based like material texture ids. */
static int l_rd_set_texture(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    uint32_t idx = athena_lua_checku32(L, 2);
    AthenaImage *image = athena_lua_check_image(L, 3);
    athena_render_data_set_texture(rd, idx, image->tex);
    push_textures_table(L, 1);
    lua_pushvalue(L, 3);
    lua_rawseti(L, -2, idx + 1);
    return 0;
}

static int l_rd_get_texture(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    uint32_t idx = athena_lua_checku32(L, 2);
    if ((int)idx >= rd->m.texture_count)
        return 0;
    push_textures_table(L, 1);
    lua_rawgeti(L, -1, idx + 1);
    return 1;
}

static int l_rd_push_texture(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    AthenaImage *image = athena_lua_check_image(L, 2);
    uint32_t idx = athena_render_data_push_texture(rd, image->tex);
    push_textures_table(L, 1);
    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, idx + 1);
    lua_pushinteger(L, idx);
    return 1;
}

static int l_rd_update_material(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    uint32_t index = athena_lua_checku32(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    if ((int)index >= rd->m.material_count)
        return luaL_error(L, "material index out of range");

    ath_mat *mat = &rd->m.materials[index];
    read_rgb_clamped(L, 3, "ambient", mat->ambient);
    read_rgb_clamped(L, 3, "diffuse", mat->diffuse);
    read_rgb_clamped(L, 3, "specular", mat->specular);
    read_rgb_clamped(L, 3, "emission", mat->emission);
    read_rgb_clamped(L, 3, "transmittance", mat->transmittance);
    read_rgb_clamped(L, 3, "transmission_filter", mat->transmission_filter);

    float v;
    if (athena_lua_field_float(L, 3, "shininess", &v)) mat->shininess = clampf(v, 0, 255);
    if (athena_lua_field_float(L, 3, "refraction", &v)) mat->refraction = clampf(v, 0.1f, 4);
    if (athena_lua_field_float(L, 3, "disolve", &v)) mat->disolve = clampf(v, 0, 1);
    if (athena_lua_field_float(L, 3, "bump_scale", &v)) mat->bump_scale = clampf(v, -10, 10);

    FlushCache(WRITEBACK_DCACHE);
    lua_pushinteger(L, index);
    return 1;
}

/* clone([deep]) -- shallow shares geometry (same shape, other material). */
static int l_rd_clone(lua_State *L)
{
    AthenaRenderData *src = check_rd(L, 1);
    bool deep = athena_lua_tobool(L, 2);

    LuaRenderData *ud = athena_lua_new_object(L, RENDERDATA_CLASS, sizeof(LuaRenderData));
    int self = lua_gettop(L);

    AthenaRenderData *rd;
    if (!deep) {
        rd = athena_render_data_clone(src);
        if (!rd)
            return luaL_error(L, "out of memory");
        /* keep the source geometry alive */
        athena_lua_retain(L, self, 1, "__source");
    } else {
        rd = athena_render_data_create();
        if (!rd)
            return luaL_error(L, "out of memory");
        athena_mesh_ensure_uncompressed(&src->m);
        uint32_t n = src->m.index_count;
        rd->m.index_count = n;
        rd->m.skeleton = src->m.skeleton;
        rd->m.tristrip = src->m.tristrip;
        athena_lua_retain(L, self, 1, "__source"); /* skeleton is shared */

        VECTOR **sa[] = { &src->m.positions, &src->m.normals, &src->m.texcoords, &src->m.colours };
        VECTOR **da[] = { &rd->m.positions, &rd->m.normals, &rd->m.texcoords, &rd->m.colours };
        for (int i = 0; i < 4; i++) {
            if (*sa[i]) {
                *da[i] = memalign(16, sizeof(VECTOR) * n);
                memcpy(*da[i], *sa[i], sizeof(VECTOR) * n);
            }
        }
        if (src->m.indices) {
            rd->m.indices = malloc(sizeof(uint32_t) * n);
            memcpy(rd->m.indices, src->m.indices, sizeof(uint32_t) * n);
        }
        if (src->m.skin_data) {
            rd->m.skin_data = memalign(16, sizeof(vertex_skin_data) * n);
            memcpy(rd->m.skin_data, src->m.skin_data, sizeof(vertex_skin_data) * n);
        }
        athena_mem_register_mesh(&rd->m);

        if (src->m.material_count > 0) {
            rd->m.material_count = src->m.material_count;
            rd->m.materials = memalign(16, sizeof(ath_mat) * rd->m.material_count);
            memcpy(rd->m.materials, src->m.materials, sizeof(ath_mat) * rd->m.material_count);
        }
        if (src->m.material_index_count > 0) {
            rd->m.material_index_count = src->m.material_index_count;
            rd->m.material_indices = malloc(sizeof(material_index) * rd->m.material_index_count);
            memcpy(rd->m.material_indices, src->m.material_indices, sizeof(material_index) * rd->m.material_index_count);
        }
        rd->m.texture_count = src->m.texture_count;
        if (src->m.texture_count > 0) {
            rd->m.textures = malloc(sizeof(GSSURFACE *) * rd->m.texture_count);
            memcpy(rd->m.textures, src->m.textures, sizeof(GSSURFACE *) * rd->m.texture_count);
        }
        memcpy(rd->m.bounding_box, src->m.bounding_box, sizeof(rd->m.bounding_box));
        rd->m.pipeline = src->m.pipeline;
        rd->m.attributes = src->m.attributes;
    }
    ud->native = rd;

    /* Share the Image objects too, so they outlive both. */
    lua_getfield(L, 1, "textures");
    if (lua_istable(L, -1)) {
        lua_newtable(L);
        lua_Integer n = luaL_len(L, -2);
        for (lua_Integer i = 1; i <= n; i++) {
            lua_rawgeti(L, -2, i);
            lua_rawseti(L, -2, i);
        }
        lua_setfield(L, self, "textures");
    }
    lua_pop(L, 1);

    lua_settop(L, self);
    return 1;
}

static int get_rd_vertices(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    athena_mesh_pin(&rd->m);
    VECTOR *attrs[] = { rd->m.positions, rd->m.normals, rd->m.texcoords, rd->m.colours };
    uint32_t size = rd->m.index_count * sizeof(VECTOR);

    lua_createtable(L, 0, 4);
    for (int i = 0; i < 4; i++) {
        if (!attrs[i])
            continue;
        athena_lua_push_buffer(L, attrs[i], size, false);
        athena_lua_retain(L, -1, 1, "__owner");
        lua_setfield(L, -2, vert_attributes[i]);
    }
    return 1;
}

static int set_rd_vertices(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    VECTOR **attrs[] = { &rd->m.positions, &rd->m.normals, &rd->m.texcoords, &rd->m.colours };

    athena_mem_unregister_mesh(&rd->m);

    for (int i = 0; i < 4; i++) {
        if (*attrs[i] && rd->owns_vertices[i])
            free(*attrs[i]);
        *attrs[i] = NULL;

        if (lua_getfield(L, 2, vert_attributes[i]) != LUA_TNIL) {
            uint32_t size;
            bool shared;
            *attrs[i] = read_attribute(L, -1, false, &size, &shared);
            rd->owns_vertices[i] = true;
            if (i == 0)
                rd->m.index_count = size / sizeof(VECTOR);
        }
        lua_pop(L, 1);
    }

    rd->m.frozen = false;
    athena_mem_register_mesh(&rd->m);
    athena_render_data_invalidate_compact_cache(rd);
    athena_render_data_invalidate_chain_cache(rd);

    if (rd->m.index_count > 0 && rd->m.material_indices) {
        if (rd->m.material_index_count == 1)
            rd->m.material_indices[0].end = rd->m.index_count - 1;
        else
            clamp_material_indices(&rd->m);
    }

    calculate_bbox(&rd->m);
    FlushCache(WRITEBACK_DCACHE);
    return 0;
}

static int get_rd_materials(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    lua_createtable(L, rd->m.material_count, 0);
    for (int i = 0; i < rd->m.material_count; i++) {
        ath_mat *mat = &rd->m.materials[i];
        lua_createtable(L, 0, 13);
        push_rgb(L, mat->ambient); lua_setfield(L, -2, "ambient");
        push_rgb(L, mat->diffuse); lua_setfield(L, -2, "diffuse");
        push_rgb(L, mat->specular); lua_setfield(L, -2, "specular");
        push_rgb(L, mat->emission); lua_setfield(L, -2, "emission");
        push_rgb(L, mat->transmittance); lua_setfield(L, -2, "transmittance");
        athena_lua_set_num(L, "shininess", mat->shininess);
        athena_lua_set_num(L, "refraction", mat->refraction);
        push_rgb(L, mat->transmission_filter); lua_setfield(L, -2, "transmission_filter");
        athena_lua_set_num(L, "disolve", mat->disolve);
        athena_lua_set_int(L, "texture_id", mat->texture_id);
        athena_lua_set_int(L, "ref_texture_id", mat->ref_texture_id);
        athena_lua_set_int(L, "bump_texture_id", mat->bump_texture_id);
        athena_lua_set_int(L, "decal_texture_id", mat->decal_texture_id);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int set_rd_materials(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    apply_materials(L, rd, 2);
    athena_render_data_invalidate_chain_cache(rd);
    FlushCache(WRITEBACK_DCACHE);
    return 0;
}

static int get_rd_material_indices(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    lua_createtable(L, rd->m.material_index_count, 0);
    for (int i = 0; i < rd->m.material_index_count; i++) {
        lua_createtable(L, 0, 2);
        athena_lua_set_int(L, "index", rd->m.material_indices[i].index);
        athena_lua_set_int(L, "end", rd->m.material_indices[i].end);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int set_rd_material_indices(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    apply_material_indices(L, rd, 2);
    athena_render_data_invalidate_compact_cache(rd);
    athena_render_data_invalidate_chain_cache(rd);
    FlushCache(WRITEBACK_DCACHE);
    return 0;
}

static int get_rd_accurate_clipping(lua_State *L) { lua_pushboolean(L, check_rd(L, 1)->m.attributes.accurate_clipping); return 1; }
static int set_rd_accurate_clipping(lua_State *L) { check_rd(L, 1)->m.attributes.accurate_clipping = athena_lua_tobool(L, 2); return 0; }
static int get_rd_face_culling(lua_State *L) { lua_pushnumber(L, check_rd(L, 1)->m.attributes.face_culling); return 1; }
static int set_rd_face_culling(lua_State *L) { check_rd(L, 1)->m.attributes.face_culling = athena_lua_checkfloat(L, 2); return 0; }
static int get_rd_texture_mapping(lua_State *L) { lua_pushboolean(L, check_rd(L, 1)->m.attributes.texture_mapping); return 1; }
static int set_rd_texture_mapping(lua_State *L) { check_rd(L, 1)->m.attributes.texture_mapping = athena_lua_tobool(L, 2); return 0; }
static int get_rd_shade_model(lua_State *L) { lua_pushinteger(L, check_rd(L, 1)->m.attributes.shade_model); return 1; }
static int set_rd_shade_model(lua_State *L) { check_rd(L, 1)->m.attributes.shade_model = athena_lua_checku32(L, 2); return 0; }
static int get_rd_size(lua_State *L) { lua_pushinteger(L, check_rd(L, 1)->m.index_count); return 1; }
static int get_rd_pipeline(lua_State *L) { lua_pushinteger(L, check_rd(L, 1)->m.pipeline); return 1; }
static int set_rd_pipeline(lua_State *L) { check_rd(L, 1)->m.pipeline = (eRenderPipelines)athena_lua_checku32(L, 2); return 0; }

static int get_rd_bounds(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    lua_createtable(L, 8, 0);
    for (int i = 0; i < 8; i++) {
        push_xyz(L, rd->m.bounding_box[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int set_rd_bounds(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    for (int i = 0; i < 8; i++) {
        lua_geti(L, 2, i + 1);
        read_xyz(L, -1, rd->m.bounding_box[i]);
        rd->m.bounding_box[i][3] = 1.0f;
        lua_pop(L, 1);
    }
    FlushCache(WRITEBACK_DCACHE);
    return 0;
}

#define RD_PROP(name) { #name, get_rd_##name, set_rd_##name }

static const athena_lua_prop rd_props[] = {
    RD_PROP(vertices), RD_PROP(materials), RD_PROP(material_indices),
    RD_PROP(accurate_clipping), RD_PROP(face_culling), RD_PROP(texture_mapping),
    RD_PROP(shade_model), { "size", get_rd_size, NULL }, RD_PROP(bounds), RD_PROP(pipeline),
    { NULL, NULL, NULL }
};

static const luaL_Reg rd_methods[] = {
    { "setTexture", l_rd_set_texture }, { "getTexture", l_rd_get_texture },
    { "pushTexture", l_rd_push_texture }, { "free", l_rd_free }, { "dispose", l_rd_free },
    { "clone", l_rd_clone }, { "updateMaterial", l_rd_update_material }, { "freeze", l_rd_freeze },
    { NULL, NULL }
};

static const athena_lua_class rd_class = {
    .name = RENDERDATA_CLASS, .methods = rd_methods, .props = rd_props, .gc = l_rd_free,
};

/* -------------------------------------------------------- RenderObject -- */

#define ro_data(ro) (athena_render_object_native(ro))

static int l_ro_new(lua_State *L)
{
    AthenaRenderData *rd = check_rd(L, 1);
    LuaRenderObject *ud = athena_lua_new_object(L, ATHENA_LUA_RENDEROBJECT, sizeof(LuaRenderObject));
    int self = lua_gettop(L);

    ud->native = athena_render_object_create(rd);
    if (!ud->native)
        return luaL_error(L, "out of memory");

    athena_lua_retain(L, self, 1, "__render_data");

    athena_object_data *obj = ro_data(ud->native);
    athena_lua_push_matrix_view(L, (float *)&obj->transform, self);
    lua_setfield(L, self, "transform");

    if (obj->data->skeleton) {
        int count = obj->data->skeleton->bone_count;
        lua_createtable(L, count, 0); /* bones */
        lua_createtable(L, count, 0); /* bone_matrices */
        for (int i = 0; i < count; i++) {
            lua_createtable(L, 0, 4);
            athena_lua_push_matrix_view(L, (float *)&obj->bones[i].transform, self);
            lua_setfield(L, -2, "transform");
            athena_lua_push_vec4_view(L, (float *)&obj->bones[i].position, self);
            lua_setfield(L, -2, "position");
            athena_lua_push_vec4_view(L, (float *)&obj->bones[i].rotation, self);
            lua_setfield(L, -2, "rotation");
            athena_lua_push_vec4_view(L, (float *)&obj->bones[i].scale, self);
            lua_setfield(L, -2, "scale");
            lua_rawseti(L, -3, i + 1);

            athena_lua_push_matrix_view(L, (float *)&obj->bone_matrices[i], self);
            lua_rawseti(L, -2, i + 1);
        }
        lua_setfield(L, self, "bone_matrices");
        lua_setfield(L, self, "bones");
    }

    lua_settop(L, self);
    return 1;
}

static int l_ro_free(lua_State *L)
{
    LuaRenderObject *ud = athena_lua_check_object(L, 1, ATHENA_LUA_RENDEROBJECT);
    if (ud->native)
        athena_render_object_destroy(ud->native);
    ud->native = NULL;
    return 0;
}

static int l_ro_render(lua_State *L)
{
    athena_render_object_draw(athena_lua_check_render_object(L, 1));
    return 0;
}

static int l_ro_render_bounds(lua_State *L)
{
    athena_render_object_draw_bbox(athena_lua_check_render_object(L, 1), athena_lua_checku32(L, 2));
    return 0;
}

static int l_ro_play_anim(lua_State *L)
{
    AthenaRenderObject *ro = athena_lua_check_render_object(L, 1);
    athena_animation *anim = check_anim(L, 2);
    athena_render_object_play_anim_blend(ro, anim, athena_lua_tobool(L, 3), athena_lua_optfloat(L, 4, 0.15f));
    if (!lua_islightuserdata(L, 2))
        athena_lua_retain(L, 1, 2, "__anim");
    return 0;
}

static int l_ro_is_playing_anim(lua_State *L)
{
    AthenaRenderObject *ro = athena_lua_check_render_object(L, 1);
    athena_animation *anim = lua_isnoneornil(L, 2) ? NULL : check_anim(L, 2);
    lua_pushboolean(L, athena_render_object_is_playing_anim(ro, anim));
    return 1;
}

static int l_ro_get_bone_transform(lua_State *L)
{
    athena_object_data *obj = ro_data(athena_lua_check_render_object(L, 1));
    const char *name = luaL_checkstring(L, 2);

    if (!obj->data || !obj->data->skeleton || !obj->bones)
        return 0;

    athena_skeleton *sk = obj->data->skeleton;
    int id = -1;
    for (int i = 0; i < sk->bone_count; i++) {
        if (strcmp(sk->bones[i].name, name) == 0) {
            id = i;
            break;
        }
    }
    if (id < 0)
        return 0;

    lua_createtable(L, 0, 4);
    athena_lua_push_matrix_view(L, (float *)&obj->bone_matrices[id], 1);
    lua_setfield(L, -2, "matrix");
    push_xyz(L, obj->bones[id].position);
    lua_setfield(L, -2, "position");
    lua_createtable(L, 0, 4);
    athena_lua_set_num(L, "x", obj->bones[id].rotation[0]);
    athena_lua_set_num(L, "y", obj->bones[id].rotation[1]);
    athena_lua_set_num(L, "z", obj->bones[id].rotation[2]);
    athena_lua_set_num(L, "w", obj->bones[id].rotation[3]);
    lua_setfield(L, -2, "rotation");
    athena_lua_set_int(L, "index", id);
    return 1;
}

#ifdef ATHENA_PHYSICS
static int l_ro_set_collision(lua_State *L)
{
    athena_object_data *obj = ro_data(athena_lua_check_render_object(L, 1));
    if (lua_isnoneornil(L, 2)) {
        obj->collision = NULL;
        obj->update_collision = NULL;
        return 0;
    }
    obj->collision = athena_lua_check_geom(L, 2);
    obj->update_collision = updateGeomPosRot;
    athena_lua_retain(L, 1, 2, "__collision");
    update_object_space(obj);
    return 0;
}

static int l_ro_set_physics(lua_State *L)
{
    athena_object_data *obj = ro_data(athena_lua_check_render_object(L, 1));
    if (lua_isnoneornil(L, 2)) {
        obj->physics = NULL;
        obj->update_physics = NULL;
        return 0;
    }
    obj->physics = athena_lua_check_body(L, 2);
    obj->update_physics = updateBodyPosRot;
    athena_lua_retain(L, 1, 2, "__physics");
    update_object_space(obj);
    return 0;
}
#endif

#define RO_VEC_PROP(name) \
    static int get_ro_##name(lua_State *L) { push_xyz(L, ro_data(athena_lua_check_render_object(L, 1))->name); return 1; } \
    static int set_ro_##name(lua_State *L) { \
        athena_object_data *obj = ro_data(athena_lua_check_render_object(L, 1)); \
        read_xyz(L, 2, obj->name); update_object_space(obj); return 0; }

RO_VEC_PROP(position)
RO_VEC_PROP(rotation)
RO_VEC_PROP(scale)

static int get_ro_frustumCull(lua_State *L) { lua_pushboolean(L, ro_data(athena_lua_check_render_object(L, 1))->frustum_cull); return 1; }
static int set_ro_frustumCull(lua_State *L) { ro_data(athena_lua_check_render_object(L, 1))->frustum_cull = athena_lua_tobool(L, 2); return 0; }

/* Fast setters that avoid building a table per call. */
static int l_ro_set_position(lua_State *L)
{
    athena_object_data *obj = ro_data(athena_lua_check_render_object(L, 1));
    obj->position[0] = athena_lua_checkfloat(L, 2);
    obj->position[1] = athena_lua_checkfloat(L, 3);
    obj->position[2] = athena_lua_checkfloat(L, 4);
    update_object_space(obj);
    return 0;
}

static int l_ro_set_rotation(lua_State *L)
{
    athena_object_data *obj = ro_data(athena_lua_check_render_object(L, 1));
    obj->rotation[0] = athena_lua_checkfloat(L, 2);
    obj->rotation[1] = athena_lua_checkfloat(L, 3);
    obj->rotation[2] = athena_lua_checkfloat(L, 4);
    update_object_space(obj);
    return 0;
}

static int l_ro_set_scale(lua_State *L)
{
    athena_object_data *obj = ro_data(athena_lua_check_render_object(L, 1));
    obj->scale[0] = athena_lua_checkfloat(L, 2);
    obj->scale[1] = athena_lua_checkfloat(L, 3);
    obj->scale[2] = athena_lua_checkfloat(L, 4);
    update_object_space(obj);
    return 0;
}

#define RO_PROP(name) { #name, get_ro_##name, set_ro_##name }

static const athena_lua_prop ro_props[] = {
    RO_PROP(position), RO_PROP(rotation), RO_PROP(scale), RO_PROP(frustumCull),
    { NULL, NULL, NULL }
};

static const luaL_Reg ro_methods[] = {
    { "render", l_ro_render }, { "renderBounds", l_ro_render_bounds },
    { "free", l_ro_free }, { "dispose", l_ro_free },
    { "getBoneTransform", l_ro_get_bone_transform },
    { "playAnim", l_ro_play_anim }, { "isPlayingAnim", l_ro_is_playing_anim },
    { "setPosition", l_ro_set_position }, { "setRotation", l_ro_set_rotation }, { "setScale", l_ro_set_scale },
#ifdef ATHENA_PHYSICS
    { "setCollision", l_ro_set_collision }, { "setPhysics", l_ro_set_physics },
#endif
    { NULL, NULL }
};

static const athena_lua_class ro_class = {
    .name = ATHENA_LUA_RENDEROBJECT, .methods = ro_methods, .props = ro_props, .gc = l_ro_free,
};

/* --------------------------------------------------------------- Batch -- */

static AthenaRenderBatch *check_batch(lua_State *L, int idx)
{
    LuaBatch *b = athena_lua_check_object(L, idx, BATCH_CLASS);
    if (!b->native)
        luaL_error(L, "Batch was freed");
    return b->native;
}

/* Batch({ autoSort = true }) */
static int l_batch_new(lua_State *L)
{
    bool auto_sort = true;
    if (lua_istable(L, 1))
        athena_lua_field_bool(L, 1, "autoSort", &auto_sort);

    LuaBatch *b = athena_lua_new_object(L, BATCH_CLASS, sizeof(LuaBatch));
    b->native = athena_render_batch_create(auto_sort);
    if (!b->native)
        return luaL_error(L, "out of memory");
    return 1;
}

static int l_batch_free(lua_State *L)
{
    LuaBatch *b = athena_lua_check_object(L, 1, BATCH_CLASS);
    if (b->native)
        athena_render_batch_destroy(b->native);
    b->native = NULL;
    return 0;
}

static int l_batch_add(lua_State *L)
{
    AthenaRenderBatch *b = check_batch(L, 1);
    if (athena_render_batch_add(b, athena_lua_check_render_object(L, 2)) < 0)
        return luaL_error(L, "batch add failed");

    /* keep members alive: field "__members" = { [ro] = true } */
    lua_getfield(L, 1, "__members");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, 1, "__members");
    }
    lua_pushvalue(L, 2);
    lua_pushboolean(L, 1);
    lua_rawset(L, -3);

    lua_pushinteger(L, athena_render_batch_size(b));
    return 1;
}

static int l_batch_clear(lua_State *L)
{
    athena_render_batch_clear(check_batch(L, 1));
    lua_pushnil(L);
    lua_setfield(L, 1, "__members");
    return 0;
}

static int l_batch_render(lua_State *L)
{
    lua_pushinteger(L, athena_render_batch_render(check_batch(L, 1)));
    return 1;
}

static int get_batch_size(lua_State *L)
{
    lua_pushinteger(L, athena_render_batch_size(check_batch(L, 1)));
    return 1;
}

static const athena_lua_prop batch_props[] = {
    { "size", get_batch_size, NULL },
    { NULL, NULL, NULL }
};

static const luaL_Reg batch_methods[] = {
    { "add", l_batch_add }, { "clear", l_batch_clear }, { "render", l_batch_render }, { "free", l_batch_free },
    { NULL, NULL }
};

static const athena_lua_class batch_class = {
    .name = BATCH_CLASS, .methods = batch_methods, .props = batch_props, .gc = l_batch_free,
};

/* ----------------------------------------------------------- SceneNode -- */

static athena_scene_node *check_node(lua_State *L, int idx)
{
    LuaSceneNode *n = athena_lua_check_object(L, idx, SCENENODE_CLASS);
    if (!n->node)
        luaL_error(L, "SceneNode was freed");
    return n->node;
}

/* Lua side bookkeeping, mirroring the JS lists: field "__children" and
 * "__attachments" are sets, "__parent" is the parent node object. */
static void node_set(lua_State *L, int node, const char *key, int value, bool add)
{
    lua_getfield(L, node, key);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        if (!add)
            return;
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, node, key);
    }
    lua_pushvalue(L, value);
    if (add)
        lua_pushboolean(L, 1);
    else
        lua_pushnil(L);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

static int node_set_count(lua_State *L, int node, const char *key)
{
    int n = 0;
    lua_getfield(L, node, key);
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            n++;
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return n;
}

static int l_node_new(lua_State *L)
{
    LuaSceneNode *n = athena_lua_new_object(L, SCENENODE_CLASS, sizeof(LuaSceneNode));
    n->node = render_scene_node_create();
    if (!n->node)
        return luaL_error(L, "out of memory");

    athena_scene_node *node = n->node;
    node->position[0] = node->position[1] = node->position[2] = 0.0f;
    node->position[3] = 1.0f;
    node->rotation[0] = node->rotation[1] = node->rotation[2] = 0.0f;
    node->rotation[3] = 1.0f;
    node->scale[0] = node->scale[1] = node->scale[2] = node->scale[3] = 1.0f;
    return 1;
}

static int l_node_free(lua_State *L)
{
    LuaSceneNode *n = athena_lua_check_object(L, 1, SCENENODE_CLASS);
    if (n->node)
        render_scene_node_destroy(n->node);
    n->node = NULL;
    return 0;
}

static void node_detach_parent(lua_State *L, int child)
{
    if (lua_getfield(L, child, "__parent") == LUA_TUSERDATA) {
        int parent = lua_gettop(L);
        node_set(L, parent, "__children", child, false);
        LuaSceneNode *p = athena_lua_check_object(L, parent, SCENENODE_CLASS);
        if (p->node)
            render_scene_node_remove_child(p->node, check_node(L, child));
    }
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_setfield(L, child, "__parent");
}

static int l_node_add_child(lua_State *L)
{
    athena_scene_node *node = check_node(L, 1);
    athena_scene_node *child = check_node(L, 2);
    if (node == child)
        return luaL_error(L, "cannot parent node to itself");

    node_detach_parent(L, 2);
    node_set(L, 1, "__children", 2, true);
    render_scene_node_add_child(node, child);
    lua_pushvalue(L, 1);
    lua_setfield(L, 2, "__parent");

    lua_pushinteger(L, node_set_count(L, 1, "__children"));
    return 1;
}

static int l_node_remove_child(lua_State *L)
{
    athena_scene_node *node = check_node(L, 1);
    athena_scene_node *child = check_node(L, 2);

    node_set(L, 1, "__children", 2, false);
    lua_getfield(L, 2, "__parent");
    if (lua_rawequal(L, -1, 1)) {
        lua_pushnil(L);
        lua_setfield(L, 2, "__parent");
    }
    lua_pop(L, 1);
    render_scene_node_remove_child(node, child);

    lua_pushinteger(L, node_set_count(L, 1, "__children"));
    return 1;
}

static int l_node_attach(lua_State *L)
{
    athena_scene_node *node = check_node(L, 1);
    AthenaRenderObject *ro = athena_lua_check_render_object(L, 2);
    node_set(L, 1, "__attachments", 2, true);
    render_scene_node_attach(node, ro_data(ro));
    lua_pushinteger(L, node_set_count(L, 1, "__attachments"));
    return 1;
}

static int l_node_detach(lua_State *L)
{
    athena_scene_node *node = check_node(L, 1);
    if (lua_isnoneornil(L, 2)) {
        lua_pushnil(L);
        lua_setfield(L, 1, "__attachments");
        render_scene_node_detach(node, NULL);
        lua_pushinteger(L, 0);
        return 1;
    }
    AthenaRenderObject *ro = athena_lua_check_render_object(L, 2);
    node_set(L, 1, "__attachments", 2, false);
    render_scene_node_detach(node, ro_data(ro));
    lua_pushinteger(L, node_set_count(L, 1, "__attachments"));
    return 1;
}

static int l_node_update(lua_State *L)
{
    render_scene_update(check_node(L, 1));
    return 0;
}

#define NODE_VEC_PROP(name) \
    static int get_node_##name(lua_State *L) { push_xyz(L, check_node(L, 1)->name); return 1; } \
    static int set_node_##name(lua_State *L) { read_xyz(L, 2, check_node(L, 1)->name); return 0; }

NODE_VEC_PROP(position)
NODE_VEC_PROP(rotation)
NODE_VEC_PROP(scale)

static const athena_lua_prop node_props[] = {
    { "position", get_node_position, set_node_position },
    { "rotation", get_node_rotation, set_node_rotation },
    { "scale", get_node_scale, set_node_scale },
    { NULL, NULL, NULL }
};

static const luaL_Reg node_methods[] = {
    { "addChild", l_node_add_child }, { "removeChild", l_node_remove_child },
    { "attach", l_node_attach }, { "detach", l_node_detach },
    { "update", l_node_update }, { "free", l_node_free },
    { NULL, NULL }
};

static const athena_lua_class node_class = {
    .name = SCENENODE_CLASS, .methods = node_methods, .props = node_props, .gc = l_node_free,
};

/* --------------------------------------------------------- AsyncLoader -- */

typedef struct {
    int cb_ref;
    char *path;
} LoaderThunk;

static void loader_thunk_free(void *user)
{
    LoaderThunk *t = user;
    if (!t)
        return;
    lua_State *L = athena_lua_state();
    if (L)
        athena_lua_unref(L, t->cb_ref);
    free(t->path);
    free(t);
}

/* Runs on the main thread inside process(): the callback errors propagate
 * through process() like any other Lua error. */
static void loader_bridge_cb(athena_render_data *data, void *user)
{
    LoaderThunk *t = user;
    lua_State *L = athena_lua_state();
    if (!t || !L)
        return;

    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, t->cb_ref);
    lua_pushstring(L, t->path ? t->path : "");
    push_rd_from_native(L, data);
    athena_lua_unref(L, t->cb_ref);
    free(t->path);
    free(t);
    lua_call(L, 2, 0);
    lua_settop(L, top);
}

static AthenaAsyncLoader *check_loader(lua_State *L, int idx)
{
    LuaLoader *l = athena_lua_check_object(L, idx, LOADER_CLASS);
    if (!l->native)
        luaL_error(L, "AsyncLoader was destroyed");
    return l->native;
}

static int l_loader_new(lua_State *L)
{
    uint32_t jps = 1;
    if (lua_istable(L, 1))
        athena_lua_field_u32(L, 1, "jobsPerStep", &jps);
    if (!jps)
        jps = 1;

    LuaLoader *l = athena_lua_new_object(L, LOADER_CLASS, sizeof(LuaLoader));
    l->jobs_per_step = jps;
    l->native = athena_async_loader_create(jps);
    if (!l->native)
        return luaL_error(L, "out of memory");
    return 1;
}

static int l_loader_destroy(lua_State *L)
{
    LuaLoader *l = athena_lua_check_object(L, 1, LOADER_CLASS);
    if (l->native) {
        athena_async_loader_clear_with(l->native, loader_thunk_free);
        athena_async_loader_destroy(l->native);
    }
    l->native = NULL;
    return 0;
}

/* enqueue(path, function(path, renderData) [, texture]) */
static int l_loader_enqueue(lua_State *L)
{
    AthenaAsyncLoader *loader = check_loader(L, 1);
    const char *path = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    AthenaImage *image = lua_isnoneornil(L, 4) ? NULL : athena_lua_check_image(L, 4);
    if (image)
        athena_lua_retain(L, 1, 4, NULL);

    LoaderThunk *t = calloc(1, sizeof(LoaderThunk));
    t->cb_ref = athena_lua_ref(L, 3);
    t->path = strdup(path);

    int qsz = athena_async_loader_enqueue(loader, path, image ? image->tex : NULL, loader_bridge_cb, t);
    if (qsz < 0) {
        loader_thunk_free(t);
        return luaL_error(L, "enqueue failed");
    }
    lua_pushinteger(L, qsz);
    return 1;
}

static int l_loader_clear(lua_State *L)
{
    athena_async_loader_clear_with(check_loader(L, 1), loader_thunk_free);
    return 0;
}

static int l_loader_process(lua_State *L)
{
    LuaLoader *l = athena_lua_check_object(L, 1, LOADER_CLASS);
    AthenaAsyncLoader *loader = check_loader(L, 1);
    uint32_t budget = athena_lua_optu32(L, 2, l->jobs_per_step);
    if (!budget)
        budget = l->jobs_per_step;
    lua_pushinteger(L, athena_async_loader_process(loader, budget));
    return 1;
}

static int l_loader_size(lua_State *L)
{
    lua_pushinteger(L, athena_async_loader_queue_size(check_loader(L, 1)));
    return 1;
}

static int l_loader_get_jps(lua_State *L)
{
    lua_pushinteger(L, athena_async_loader_jobs_per_step(check_loader(L, 1)));
    return 1;
}

static int l_loader_set_jps(lua_State *L)
{
    LuaLoader *l = athena_lua_check_object(L, 1, LOADER_CLASS);
    uint32_t v = athena_lua_optu32(L, 2, l->jobs_per_step);
    athena_async_loader_set_jobs_per_step(check_loader(L, 1), v);
    l->jobs_per_step = v ? v : 1;
    lua_pushinteger(L, l->jobs_per_step);
    return 1;
}

static const luaL_Reg loader_methods[] = {
    { "enqueue", l_loader_enqueue }, { "clear", l_loader_clear }, { "process", l_loader_process },
    { "destroy", l_loader_destroy }, { "size", l_loader_size },
    { "getJobsPerStep", l_loader_get_jps }, { "setJobsPerStep", l_loader_set_jps },
    { NULL, NULL }
};

static const athena_lua_class loader_class = {
    .name = LOADER_CLASS, .methods = loader_methods, .gc = l_loader_destroy,
};

/* ------------------------------------------------------ AnimCollection -- */

static AthenaAnim3dCollection *check_animcol(lua_State *L, int idx)
{
    LuaAnimCollection *c = athena_lua_check_object(L, idx, ANIMCOL_CLASS);
    if (!c->collection)
        luaL_error(L, "AnimCollection was freed");
    return c->collection;
}

static int l_animcol_new(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    LuaAnimCollection *c = athena_lua_new_object(L, ANIMCOL_CLASS, sizeof(LuaAnimCollection));
    c->collection = athena_anim3d_load(path);
    if (!c->collection)
        return luaL_error(L, "could not load animations from '%s'", path);
    return 1;
}

static int l_animcol_free(lua_State *L)
{
    LuaAnimCollection *c = athena_lua_check_object(L, 1, ANIMCOL_CLASS);
    if (c->collection)
        athena_anim3d_free(c->collection);
    c->collection = NULL;
    return 0;
}

static void push_anim(lua_State *L, athena_animation *anim, int owner)
{
    owner = lua_absindex(L, owner);
    LuaAnim *a = athena_lua_new_object(L, ANIM_CLASS, sizeof(LuaAnim));
    a->anim = anim;
    athena_lua_retain(L, -1, owner, "__collection");
}

/* anims[i] (1-based), anims.name or anims["name"]; methods take precedence. */
static int l_animcol_index(lua_State *L)
{
    AthenaAnim3dCollection *col = check_animcol(L, 1);
    athena_animation *anim = NULL;

    if (lua_type(L, 2) == LUA_TNUMBER) {
        lua_Integer i = luaL_checkinteger(L, 2);
        if (i >= 1 && (uint32_t)i <= athena_anim3d_count(col))
            anim = athena_anim3d_get(col, (uint32_t)i - 1);
    } else {
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 2);
        lua_call(L, 2, 1);
        if (!lua_isnil(L, -1))
            return 1;
        lua_pop(L, 1);
        if (lua_type(L, 2) == LUA_TSTRING)
            anim = athena_anim3d_find(col, lua_tostring(L, 2));
    }

    if (!anim)
        return 0;
    push_anim(L, anim, 1);
    return 1;
}

static int l_animcol_len(lua_State *L)
{
    lua_pushinteger(L, athena_anim3d_count(check_animcol(L, 1)));
    return 1;
}

static int l_anim_name(lua_State *L)
{
    LuaAnim *a = athena_lua_check_object(L, 1, ANIM_CLASS);
    lua_pushstring(L, a->anim->name);
    return 1;
}

static const luaL_Reg animcol_methods[] = {
    { "free", l_animcol_free },
    { NULL, NULL }
};

static const luaL_Reg animcol_meta[] = {
    { "__len", l_animcol_len },
    { NULL, NULL }
};

static const athena_lua_class animcol_class = {
    .name = ANIMCOL_CLASS, .methods = animcol_methods, .gc = l_animcol_free, .meta = animcol_meta,
};

static const athena_lua_prop anim_props[] = {
    { "name", l_anim_name, NULL },
    { NULL, NULL, NULL }
};

static const athena_lua_class anim_class = {
    .name = ANIM_CLASS, .props = anim_props,
};

/* -------------------------------------------------------------- Render -- */

static int l_render_init(lua_State *L) { athena_render_module_init(); return 0; }
static int l_render_begin(lua_State *L) { athena_render_module_begin(); return 0; }
static int l_render_reset_stats(lua_State *L) { athena_render_module_reset_stats(); return 0; }

static int l_render_set_view(lua_State *L)
{
    athena_render_module_set_view(athena_lua_optfloat(L, 1, 60.0f), athena_lua_optfloat(L, 2, 1.0f),
                                  athena_lua_optfloat(L, 3, 2000.0f), athena_lua_optfloat(L, 4, 0.0f),
                                  athena_lua_optfloat(L, 5, 0.0f));
    return 0;
}

static int l_render_stats(lua_State *L)
{
    const render_stats_t *s = athena_render_module_get_stats();
    lua_createtable(L, 0, 4);
    athena_lua_set_int(L, "drawCalls", s->draw_calls);
    athena_lua_set_int(L, "triangles", s->triangles);
    athena_lua_set_int(L, "culled", s->objects_culled);
    athena_lua_set_int(L, "fastClip", s->fast_clip_groups);
    return 1;
}

/* vertexList(positions, normals, texcoords, colors, materials, material_indices [, shareBuffers]) */
static int l_render_vertex_list(lua_State *L)
{
    lua_createtable(L, 0, 7);
    for (int i = 0; i < 4; i++) {
        lua_pushvalue(L, i + 1);
        lua_setfield(L, -2, vert_attributes[i]);
    }
    lua_pushvalue(L, 5);
    lua_setfield(L, -2, "materials");
    lua_pushvalue(L, 6);
    lua_setfield(L, -2, "material_indices");

    bool share = false;
    if (lua_istable(L, 7))
        athena_lua_field_bool(L, 7, "shareBuffers", &share);
    else
        share = athena_lua_tobool(L, 7);
    if (share)
        athena_lua_set_bool(L, "shareBuffers", true);
    return 1;
}

static int l_render_material_color(lua_State *L)
{
    lua_Number r = luaL_checknumber(L, 1), g = luaL_checknumber(L, 2), b = luaL_checknumber(L, 3);
    lua_Number a = luaL_optnumber(L, 4, 1.0f); /* read before pushing: the table would alias arg 4 */
    lua_createtable(L, 0, 4);
    athena_lua_set_num(L, "r", r);
    athena_lua_set_num(L, "g", g);
    athena_lua_set_num(L, "b", b);
    athena_lua_set_num(L, "a", a);
    return 1;
}

static int l_render_material(lua_State *L)
{
    static const char *keys[] = {
        "ambient", "diffuse", "specular", "emission", "transmittance", "shininess", "refraction",
        "transmission_filter", "disolve", "texture_id", "bump_texture_id", "ref_texture_id", "decal_texture_id",
    };
    lua_createtable(L, 0, 13);
    for (int i = 0; i < 13; i++) {
        lua_pushvalue(L, i + 1);
        lua_setfield(L, -2, keys[i]);
    }
    return 1;
}

static int l_render_material_index(lua_State *L)
{
    lua_Integer index = luaL_checkinteger(L, 1), end = luaL_checkinteger(L, 2);
    lua_createtable(L, 0, 2);
    athena_lua_set_int(L, "index", index);
    athena_lua_set_int(L, "end", end);
    return 1;
}

static const luaL_Reg render_funcs[] = {
    { "init", l_render_init }, { "begin", l_render_begin }, { "setView", l_render_set_view },
    { "stats", l_render_stats }, { "resetStats", l_render_reset_stats },
    { "vertexList", l_render_vertex_list }, { "materialColor", l_render_material_color },
    { "material", l_render_material }, { "materialIndex", l_render_material_index },
    { NULL, NULL }
};

static const athena_lua_const render_consts[] = {
    { "PL_NO_LIGHTS", PL_NO_LIGHTS }, { "PL_DEFAULT", PL_DEFAULT }, { "PL_SPECULAR", PL_SPECULAR },
    { NULL, 0 }
};

/* -------------------------------------------------------------- Lights -- */

static int l_lights_new(lua_State *L)
{
    lua_pushinteger(L, athena_lights_new());
    return 1;
}

static int l_lights_set(lua_State *L)
{
    athena_lights_set((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2), athena_lua_checkfloat(L, 3),
                      athena_lua_checkfloat(L, 4), athena_lua_checkfloat(L, 5));
    return 0;
}

static const luaL_Reg lights_funcs[] = {
    { "new", l_lights_new }, { "set", l_lights_set },
    { NULL, NULL }
};

static const athena_lua_const lights_consts[] = {
    { "AMBIENT", ATHENA_LIGHT_AMBIENT }, { "DIFFUSE", ATHENA_LIGHT_DIFFUSE },
    { "SPECULAR", ATHENA_LIGHT_SPECULAR }, { "DIRECTION", ATHENA_LIGHT_DIRECTION },
    { "POSITION", ATHENA_LIGHT_POSITION }, { "ATTENUATION", ATHENA_LIGHT_ATTENUATION },
    { NULL, 0 }
};

/* -------------------------------------------------------------- Camera -- */

#define F(i) athena_lua_checkfloat(L, i)

static int l_cam_position(lua_State *L) { athena_camera3d_position(F(1), F(2), F(3)); return 0; }
static int l_cam_target(lua_State *L) { athena_camera3d_target(F(1), F(2), F(3)); return 0; }
static int l_cam_orbit(lua_State *L) { athena_camera3d_orbit(F(1), F(2)); return 0; }
static int l_cam_turn(lua_State *L) { athena_camera3d_turn(F(1), F(2)); return 0; }
static int l_cam_dolly(lua_State *L) { athena_camera3d_dolly(F(1)); return 0; }
static int l_cam_zoom(lua_State *L) { athena_camera3d_zoom(F(1)); return 0; }
static int l_cam_pan(lua_State *L) { athena_camera3d_pan(F(1), F(2)); return 0; }
static int l_cam_update(lua_State *L) { athena_camera3d_update(); return 0; }

#undef F

static int l_cam_save(lua_State *L)
{
    AthenaCamera3dState s;
    athena_camera3d_save(&s);
    lua_createtable(L, 0, 4);
    push_xyz(L, s.position); lua_setfield(L, -2, "position");
    push_xyz(L, s.target); lua_setfield(L, -2, "target");
    push_xyz(L, s.up); lua_setfield(L, -2, "up");
    push_xyz(L, s.local_up); lua_setfield(L, -2, "local_up");
    return 1;
}

static int l_cam_restore(lua_State *L)
{
    AthenaCamera3dState s;
    luaL_checktype(L, 1, LUA_TTABLE);
    memset(&s, 0, sizeof(s));
    s.up[1] = 1.0f;
    s.local_up[1] = 1.0f;
    s.local_up[3] = 1.0f;

    if (lua_getfield(L, 1, "position") != LUA_TNIL) read_xyz(L, -1, s.position);
    lua_pop(L, 1);
    if (lua_getfield(L, 1, "target") != LUA_TNIL) read_xyz(L, -1, s.target);
    lua_pop(L, 1);
    if (lua_getfield(L, 1, "up") != LUA_TNIL) read_xyz(L, -1, s.up);
    lua_pop(L, 1);
    if (lua_getfield(L, 1, "local_up") != LUA_TNIL) read_xyz(L, -1, s.local_up);
    lua_pop(L, 1);

    athena_camera3d_restore(&s);
    return 0;
}

static const luaL_Reg camera_funcs[] = {
    { "position", l_cam_position }, { "target", l_cam_target }, { "orbit", l_cam_orbit },
    { "turn", l_cam_turn }, { "dolly", l_cam_dolly }, { "zoom", l_cam_zoom }, { "pan", l_cam_pan },
    { "save", l_cam_save }, { "restore", l_cam_restore }, { "update", l_cam_update },
    { NULL, NULL }
};

/* ---------------------------------------------------------------- open -- */

static void wrap_animcol_index(lua_State *L)
{
    luaL_getmetatable(L, ANIMCOL_CLASS);
    lua_getfield(L, -1, "__index");
    lua_pushcclosure(L, l_animcol_index, 1);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
}

void athena_lua_open_render(lua_State *L)
{
    athena_lua_new_module(L, render_funcs, render_consts, NULL);
    athena_lua_set_num(L, "CULL_FACE_NONE", CULL_FACE_NONE);
    athena_lua_set_num(L, "CULL_FACE_BACK", CULL_FACE_BACK);
    athena_lua_set_num(L, "CULL_FACE_FRONT", CULL_FACE_FRONT);
    athena_lua_set_global_module(L, "Render");

    athena_lua_register_class(L, &rd_class);
    athena_lua_new_module(L, NULL, NULL, l_rd_new);
    athena_lua_set_global_module(L, RENDERDATA_CLASS);

    athena_lua_register_class(L, &ro_class);
    athena_lua_new_module(L, NULL, NULL, l_ro_new);
    athena_lua_set_global_module(L, ATHENA_LUA_RENDEROBJECT);

    athena_lua_register_class(L, &batch_class);
    athena_lua_new_module(L, NULL, NULL, l_batch_new);
    athena_lua_set_global_module(L, BATCH_CLASS);

    athena_lua_register_class(L, &node_class);
    athena_lua_new_module(L, NULL, NULL, l_node_new);
    athena_lua_set_global_module(L, SCENENODE_CLASS);

    athena_lua_register_class(L, &loader_class);
    athena_lua_new_module(L, NULL, NULL, l_loader_new);
    athena_lua_set_global_module(L, LOADER_CLASS);

    athena_lua_register_class(L, &anim_class);
    athena_lua_register_class(L, &animcol_class);
    wrap_animcol_index(L);
    athena_lua_new_module(L, NULL, NULL, l_animcol_new);
    athena_lua_set_global_module(L, ANIMCOL_CLASS);

    athena_lua_new_module(L, lights_funcs, lights_consts, NULL);
    athena_lua_set_global_module(L, "Lights");

    athena_lua_new_module(L, camera_funcs, NULL, NULL);
    athena_lua_set_global_module(L, "Camera");
}
