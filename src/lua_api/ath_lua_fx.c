#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include "ath_lua.h"
#include "ath_lua_graphics.h"

#include <graphics.h>
#include <athena/sprite.h>
#include <athena/particles.h>
#include <athena/shadows_facade.h>
#include <particles.h>
#include <sprite_data.h>

#ifdef ATHENA_PHYSICS
#include "ath_lua_physics.h"
#endif

/* ------------------------------------------------------------- Shadows -- */

#define PROJECTOR_CLASS "ShadowProjector"

typedef struct {
    AthenaShadowProjector *proj;
} LuaProjector;

static AthenaShadowProjector *check_proj(lua_State *L, int idx)
{
    LuaProjector *p = athena_lua_check_object(L, idx, PROJECTOR_CLASS);
    if (!p->proj)
        luaL_error(L, "ShadowProjector was freed");
    return p->proj;
}

static int l_proj_new(lua_State *L)
{
    AthenaImage *img = lua_isnoneornil(L, 1) ? NULL : athena_lua_check_image(L, 1);
    LuaProjector *p = athena_lua_new_object(L, PROJECTOR_CLASS, sizeof(LuaProjector));
    p->proj = athena_shadow_projector_create(img ? img->tex : NULL);
    if (!p->proj)
        return luaL_error(L, "could not create shadow projector");
    if (img)
        athena_lua_retain(L, -1, 1, "__texture");
    return 1;
}

static int l_proj_free(lua_State *L)
{
    LuaProjector *p = athena_lua_check_object(L, 1, PROJECTOR_CLASS);
    if (p->proj)
        athena_shadow_projector_free(p->proj);
    p->proj = NULL;
    return 0;
}

#define F(i) athena_lua_checkfloat(L, i)

/* setTransform(matrix) -- a Matrix4 or a 16-number table. */
static int l_proj_set_transform(lua_State *L)
{
    AthenaShadowProjector *p = check_proj(L, 1);
    float m[16];
    if (lua_istable(L, 2)) {
        for (int i = 0; i < 16; i++) {
            lua_geti(L, 2, i + 1);
            m[i] = (float)luaL_checknumber(L, -1);
            lua_pop(L, 1);
        }
    } else {
        for (int i = 0; i < 16; i++) {
            lua_pushinteger(L, i + 1);
            lua_gettable(L, 2); /* Matrix4 __index, 1-based */
            m[i] = (float)luaL_checknumber(L, -1);
            lua_pop(L, 1);
        }
    }
    athena_shadow_projector_set_transform(p, m);
    return 0;
}

static int l_proj_set_size(lua_State *L) { athena_shadow_projector_set_size(check_proj(L, 1), F(2), F(3)); return 0; }
static int l_proj_set_grid(lua_State *L) { athena_shadow_projector_set_grid(check_proj(L, 1), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3)); return 0; }
static int l_proj_set_light_dir(lua_State *L) { athena_shadow_projector_set_light_dir(check_proj(L, 1), F(2), F(3), F(4)); return 0; }
static int l_proj_set_bias(lua_State *L) { athena_shadow_projector_set_bias(check_proj(L, 1), F(2)); return 0; }
static int l_proj_set_light_offset(lua_State *L) { athena_shadow_projector_set_light_offset(check_proj(L, 1), F(2)); return 0; }
static int l_proj_set_slope(lua_State *L) { athena_shadow_projector_set_slope_limit(check_proj(L, 1), F(2)); return 0; }
static int l_proj_set_color(lua_State *L) { athena_shadow_projector_set_color(check_proj(L, 1), F(2), F(3), F(4), F(5)); return 0; }
static int l_proj_set_blend(lua_State *L) { athena_shadow_projector_set_blend(check_proj(L, 1), (int)luaL_checkinteger(L, 2)); return 0; }
static int l_proj_set_uv_rect(lua_State *L) { athena_shadow_projector_set_uv_rect(check_proj(L, 1), F(2), F(3), F(4), F(5)); return 0; }
static int l_proj_render(lua_State *L) { athena_shadow_projector_render(check_proj(L, 1)); return 0; }

/* {x, y, width, height} on screen of the next render(), or nil off-screen */
static int l_proj_screen_bounds(lua_State *L)
{
    int r[4];
    if (!athena_shadow_projector_screen_bounds(check_proj(L, 1), r)) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 4);
    lua_pushinteger(L, r[0]); lua_setfield(L, -2, "x");
    lua_pushinteger(L, r[1]); lua_setfield(L, -2, "y");
    lua_pushinteger(L, r[2] - r[0]); lua_setfield(L, -2, "width");
    lua_pushinteger(L, r[3] - r[1]); lua_setfield(L, -2, "height");
    return 1;
}

#ifdef ATHENA_PHYSICS
static int l_proj_enable_raycast(lua_State *L)
{
    AthenaShadowProjector *p = check_proj(L, 1);
    AthenaPhysicsSpace *space = lua_isnoneornil(L, 2) ? NULL : athena_lua_check_space(L, 2);
    athena_shadow_projector_enable_raycast(p, space, athena_lua_optfloat(L, 4, 1000.0f), athena_lua_tobool(L, 3));
    if (space)
        athena_lua_retain(L, 1, 2, "__space");
    return 0;
}
#endif

#define PROJ_VEC_PROP(name) \
    static int get_proj_##name(lua_State *L) { \
        athena_object_data *o = athena_shadow_projector_object(check_proj(L, 1)); \
        lua_createtable(L, 0, 3); \
        athena_lua_set_num(L, "x", o->name[0]); athena_lua_set_num(L, "y", o->name[1]); \
        athena_lua_set_num(L, "z", o->name[2]); return 1; } \
    static int set_proj_##name(lua_State *L) { \
        AthenaShadowProjector *p = check_proj(L, 1); \
        athena_object_data *o = athena_shadow_projector_object(p); \
        luaL_checktype(L, 2, LUA_TTABLE); \
        athena_lua_field_float(L, 2, "x", &o->name[0]); athena_lua_field_float(L, 2, "y", &o->name[1]); \
        athena_lua_field_float(L, 2, "z", &o->name[2]); \
        athena_shadow_projector_sync_transform(p); return 0; }

PROJ_VEC_PROP(position)
PROJ_VEC_PROP(rotation)
PROJ_VEC_PROP(scale)

static const athena_lua_prop proj_props[] = {
    { "position", get_proj_position, set_proj_position },
    { "rotation", get_proj_rotation, set_proj_rotation },
    { "scale", get_proj_scale, set_proj_scale },
    { NULL, NULL, NULL }
};

static const luaL_Reg proj_methods[] = {
    { "setTransform", l_proj_set_transform }, { "setSize", l_proj_set_size }, { "setGrid", l_proj_set_grid },
    { "setLightDir", l_proj_set_light_dir }, { "setBias", l_proj_set_bias },
    { "setLightOffset", l_proj_set_light_offset }, { "setSlopeLimit", l_proj_set_slope },
    { "setColor", l_proj_set_color }, { "setBlend", l_proj_set_blend }, { "setUVRect", l_proj_set_uv_rect },
#ifdef ATHENA_PHYSICS
    { "enableRaycast", l_proj_enable_raycast },
#endif
    { "render", l_proj_render }, { "screenBounds", l_proj_screen_bounds }, { "free", l_proj_free },
    { NULL, NULL }
};

static const athena_lua_class proj_class = {
    .name = PROJECTOR_CLASS, .methods = proj_methods, .props = proj_props, .gc = l_proj_free,
};

static const athena_lua_const shadow_consts[] = {
    { "SHADOW_BLEND_DARKEN", SHADOW_BLEND_DARKEN }, { "SHADOW_BLEND_ALPHA", SHADOW_BLEND_ALPHA },
    { "SHADOW_BLEND_ADD", SHADOW_BLEND_ADD }, { "SHADOW_BLEND_MULTIPLY", SHADOW_BLEND_MULTIPLY },
    { NULL, 0 }
};

/* ------------------------------------------------------------- TileMap -- */

#define DESCRIPTOR_CLASS "TileMapDescriptor"
#define INSTANCE_CLASS "TileMapInstance"

typedef struct {
    AthenaTilemapDescriptor *descriptor;
} LuaDescriptor;

typedef struct {
    AthenaTilemapInstance *instance;
} LuaInstance;

static AthenaTilemapDescriptor *check_descriptor(lua_State *L, int idx)
{
    LuaDescriptor *d = athena_lua_check_object(L, idx, DESCRIPTOR_CLASS);
    if (!d->descriptor)
        luaL_error(L, "TileMap descriptor was freed");
    return d->descriptor;
}

static AthenaTilemapInstance *check_instance(lua_State *L, int idx)
{
    LuaInstance *i = athena_lua_check_object(L, idx, INSTANCE_CLASS);
    if (!i->instance)
        luaL_error(L, "TileMap instance was freed");
    return i->instance;
}

/* Sprite buffer argument: a Buffer whose size is a multiple of the record size. */
static athena_sprite_data *check_sprites(lua_State *L, int idx, uint32_t *count)
{
    AthenaLuaBuffer *b = athena_lua_check_object(L, idx, ATHENA_LUA_BUFFER);
    if (b->size % sizeof(athena_sprite_data) != 0)
        luaL_argerror(L, idx, "sprite buffer size must be a multiple of TileMap.layout.stride");
    *count = b->size / sizeof(athena_sprite_data);
    return (athena_sprite_data *)b->data;
}

/* TileMap.Descriptor{ textures = { Image | path, ... }, materials = { {texture_index, blend_mode, end_offset}, ... } | Buffer } */
static int l_descriptor_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    LuaDescriptor *ud = athena_lua_new_object(L, DESCRIPTOR_CLASS, sizeof(LuaDescriptor));
    int self = lua_gettop(L);

    AthenaImage **textures = NULL;
    uint32_t texture_count = 0;
    athena_sprite_material *materials = NULL;
    uint32_t material_count = 0;

    if (lua_getfield(L, 1, "textures") == LUA_TTABLE) {
        int t = lua_gettop(L);
        texture_count = (uint32_t)luaL_len(L, t);
        textures = calloc(texture_count ? texture_count : 1, sizeof(AthenaImage *));
        lua_newtable(L); /* keeps the Image objects alive */
        int keep = lua_gettop(L);
        for (uint32_t i = 0; i < texture_count; i++) {
            lua_geti(L, t, i + 1);
            if (lua_type(L, -1) == LUA_TSTRING) {
                AthenaImage *image = athena_image_create(lua_tostring(L, -1), true);
                if (!image) {
                    free(textures);
                    return luaL_error(L, "could not load texture '%s'", lua_tostring(L, -1));
                }
                lua_pop(L, 1);
                athena_lua_push_image(L, image, true);
            }
            textures[i] = athena_lua_check_image(L, -1);
            lua_rawseti(L, keep, i + 1);
        }
        lua_setfield(L, self, "textures");
    }
    lua_pop(L, 1);

    int type = lua_getfield(L, 1, "materials");
    if (type == LUA_TTABLE) {
        int t = lua_gettop(L);
        material_count = (uint32_t)luaL_len(L, t);
        materials = calloc(material_count ? material_count : 1, sizeof(athena_sprite_material));
        for (uint32_t i = 0; i < material_count; i++) {
            lua_geti(L, t, i + 1);
            int m = lua_gettop(L);
            athena_lua_field_int(L, m, "texture_index", &materials[i].texture_index);
            if (lua_getfield(L, m, "blend_mode") != LUA_TNIL)
                materials[i].blend_mode = athena_lua_check_alpha_equation(L, -1);
            lua_pop(L, 1);
            athena_lua_field_u32(L, m, "end_offset", &materials[i].end);
            lua_pop(L, 1);
        }
    } else if (type != LUA_TNIL) {
        AthenaLuaBuffer *b = athena_lua_check_object(L, -1, ATHENA_LUA_BUFFER);
        if (b->size % sizeof(athena_sprite_material) != 0) {
            free(textures);
            return luaL_error(L, "invalid material buffer size");
        }
        material_count = b->size / sizeof(athena_sprite_material);
        materials = malloc(b->size ? b->size : 1);
        memcpy(materials, b->data, b->size);
    }
    lua_pop(L, 1);

    ud->descriptor = athena_sprite_descriptor_create(textures, texture_count, materials, material_count);
    free(textures);
    free(materials);
    if (!ud->descriptor)
        return luaL_error(L, "could not create tilemap descriptor");

    lua_settop(L, self);
    return 1;
}

static int l_descriptor_free(lua_State *L)
{
    LuaDescriptor *d = athena_lua_check_object(L, 1, DESCRIPTOR_CLASS);
    if (d->descriptor)
        athena_sprite_descriptor_destroy(d->descriptor);
    d->descriptor = NULL;
    return 0;
}

static int get_descriptor_material_count(lua_State *L)
{
    lua_pushinteger(L, athena_sprite_descriptor_material_count(check_descriptor(L, 1)));
    return 1;
}

/* TileMap.Instance{ descriptor = d, spriteBuffer = buf } -- the buffer is used in place. */
static int l_instance_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    LuaInstance *ud = athena_lua_new_object(L, INSTANCE_CLASS, sizeof(LuaInstance));
    int self = lua_gettop(L);

    lua_getfield(L, 1, "descriptor");
    AthenaTilemapDescriptor *descriptor = check_descriptor(L, -1);
    lua_setfield(L, self, "__descriptor");

    athena_sprite_data *sprites = NULL;
    uint32_t count = 0;
    if (lua_getfield(L, 1, "spriteBuffer") != LUA_TNIL) {
        sprites = check_sprites(L, -1, &count);
        lua_pushvalue(L, -1);
        lua_setfield(L, self, "__sprites");
    }
    lua_pop(L, 1);

    ud->instance = athena_sprite_instance_create(descriptor, sprites, count, false);
    if (!ud->instance)
        return luaL_error(L, "could not create tilemap instance");
    lua_settop(L, self);
    return 1;
}

static int l_instance_free(lua_State *L)
{
    LuaInstance *i = athena_lua_check_object(L, 1, INSTANCE_CLASS);
    if (i->instance)
        athena_sprite_instance_destroy(i->instance);
    i->instance = NULL;
    return 0;
}

static int l_instance_render(lua_State *L)
{
    athena_sprite_instance_render(check_instance(L, 1), athena_lua_optfloat(L, 2, 0), athena_lua_optfloat(L, 3, 0),
                                  athena_lua_optfloat(L, 4, 0));
    return 0;
}

static int l_instance_replace_buffer(lua_State *L)
{
    AthenaTilemapInstance *inst = check_instance(L, 1);
    uint32_t count;
    athena_sprite_data *sprites = check_sprites(L, 2, &count);
    if (athena_sprite_instance_replace_buffer(inst, sprites, count, false) < 0)
        return luaL_error(L, "could not replace sprite buffer");
    lua_pushvalue(L, 2);
    lua_setfield(L, 1, "__sprites");
    return 0;
}

static int l_instance_get_buffer(lua_State *L)
{
    check_instance(L, 1);
    lua_getfield(L, 1, "__sprites");
    return 1;
}

/* updateSprites(dstOffset, source [, count]) -- offsets count sprites, 0-based. */
static int l_instance_update(lua_State *L)
{
    AthenaTilemapInstance *inst = check_instance(L, 1);
    uint32_t dst = athena_lua_checku32(L, 2);
    uint32_t count;
    athena_sprite_data *src = check_sprites(L, 3, &count);
    uint32_t n = athena_lua_optu32(L, 4, count);
    if (n > count)
        n = count;
    if (athena_sprite_instance_update_sprites(inst, dst, src, n) < 0)
        return luaL_error(L, "updateSprites exceeds buffer");
    return 0;
}

static int l_tilemap_init(lua_State *L) { athena_sprite_init(); return 0; }
static int l_tilemap_begin(lua_State *L) { athena_sprite_begin(); return 0; }
static int l_tilemap_set_camera(lua_State *L) { athena_sprite_set_camera(F(1), F(2)); return 0; }

static void push_sprite_buffer(lua_State *L, uint32_t count)
{
    uint32_t size = count * sizeof(athena_sprite_data);
    uint8_t *data = memalign(16, size ? size : 16);
    if (!data)
        luaL_error(L, "out of memory");
    memset(data, 0, size ? size : 16);
    athena_lua_push_buffer(L, data, size, true);
}

static void write_sprite(lua_State *L, int t, athena_sprite_data *s)
{
    athena_lua_field_float(L, t, "x", &s->x);
    athena_lua_field_float(L, t, "y", &s->y);
    athena_lua_field_float(L, t, "zindex", &s->zindex);
    athena_lua_field_float(L, t, "w", &s->w);
    athena_lua_field_float(L, t, "h", &s->h);
    athena_lua_field_float(L, t, "u1", &s->u1);
    athena_lua_field_float(L, t, "v1", &s->v1);
    athena_lua_field_float(L, t, "u2", &s->u2);
    athena_lua_field_float(L, t, "v2", &s->v2);
    athena_lua_field_u32(L, t, "r", &s->r);
    athena_lua_field_u32(L, t, "g", &s->g);
    athena_lua_field_u32(L, t, "b", &s->b);
    athena_lua_field_u32(L, t, "a", &s->a);
}

static int l_spritebuffer_create(lua_State *L)
{
    push_sprite_buffer(L, athena_lua_checku32(L, 1));
    return 1;
}

static int l_spritebuffer_from_objects(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    uint32_t n = (uint32_t)luaL_len(L, 1);
    push_sprite_buffer(L, n);
    athena_sprite_data *sprites = (athena_sprite_data *)((AthenaLuaBuffer *)lua_touserdata(L, -1))->data;
    for (uint32_t i = 0; i < n; i++) {
        lua_geti(L, 1, i + 1);
        luaL_checktype(L, -1, LUA_TTABLE);
        write_sprite(L, lua_gettop(L), &sprites[i]);
        lua_pop(L, 1);
    }
    return 1;
}

/* SpriteBuffer.set(buffer, index, fields) -- index is 0-based like updateSprites. */
static int l_spritebuffer_set(lua_State *L)
{
    uint32_t count;
    athena_sprite_data *sprites = check_sprites(L, 1, &count);
    uint32_t i = athena_lua_checku32(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    if (i >= count)
        return luaL_argerror(L, 2, "sprite index out of range");
    write_sprite(L, 3, &sprites[i]);
    return 0;
}

static int l_spritebuffer_get(lua_State *L)
{
    uint32_t count;
    athena_sprite_data *sprites = check_sprites(L, 1, &count);
    uint32_t i = athena_lua_checku32(L, 2);
    if (i >= count)
        return luaL_argerror(L, 2, "sprite index out of range");
    athena_sprite_data *s = &sprites[i];
    lua_createtable(L, 0, 13);
    athena_lua_set_num(L, "x", s->x); athena_lua_set_num(L, "y", s->y); athena_lua_set_num(L, "zindex", s->zindex);
    athena_lua_set_num(L, "w", s->w); athena_lua_set_num(L, "h", s->h);
    athena_lua_set_num(L, "u1", s->u1); athena_lua_set_num(L, "v1", s->v1);
    athena_lua_set_num(L, "u2", s->u2); athena_lua_set_num(L, "v2", s->v2);
    athena_lua_set_int(L, "r", s->r); athena_lua_set_int(L, "g", s->g);
    athena_lua_set_int(L, "b", s->b); athena_lua_set_int(L, "a", s->a);
    return 1;
}

static const athena_lua_prop descriptor_props[] = {
    { "materialCount", get_descriptor_material_count, NULL },
    { NULL, NULL, NULL }
};

static const luaL_Reg descriptor_methods[] = {
    { "free", l_descriptor_free },
    { NULL, NULL }
};

static const athena_lua_class descriptor_class = {
    .name = DESCRIPTOR_CLASS, .methods = descriptor_methods, .props = descriptor_props, .gc = l_descriptor_free,
};

static const luaL_Reg instance_methods[] = {
    { "render", l_instance_render }, { "replaceSpriteBuffer", l_instance_replace_buffer },
    { "getSpriteBuffer", l_instance_get_buffer }, { "updateSprites", l_instance_update },
    { "free", l_instance_free },
    { NULL, NULL }
};

static const athena_lua_class instance_class = {
    .name = INSTANCE_CLASS, .methods = instance_methods, .gc = l_instance_free,
};

static const luaL_Reg tilemap_funcs[] = {
    { "init", l_tilemap_init }, { "begin", l_tilemap_begin }, { "setCamera", l_tilemap_set_camera },
    { "Descriptor", l_descriptor_new }, { "Instance", l_instance_new },
    { NULL, NULL }
};

static const luaL_Reg spritebuffer_funcs[] = {
    { "create", l_spritebuffer_create }, { "fromObjects", l_spritebuffer_from_objects },
    { "set", l_spritebuffer_set }, { "get", l_spritebuffer_get },
    { NULL, NULL }
};

static void push_layout(lua_State *L)
{
    const athena_tilemap_layout *l = athena_sprite_layout();
    lua_createtable(L, 0, 2);
    athena_lua_set_int(L, "stride", l->stride);
    lua_createtable(L, 0, 13);
    athena_lua_set_int(L, "x", l->offset_x); athena_lua_set_int(L, "y", l->offset_y);
    athena_lua_set_int(L, "w", l->offset_w); athena_lua_set_int(L, "h", l->offset_h);
    athena_lua_set_int(L, "u1", l->offset_u1); athena_lua_set_int(L, "v1", l->offset_v1);
    athena_lua_set_int(L, "u2", l->offset_u2); athena_lua_set_int(L, "v2", l->offset_v2);
    athena_lua_set_int(L, "r", l->offset_r); athena_lua_set_int(L, "g", l->offset_g);
    athena_lua_set_int(L, "b", l->offset_b); athena_lua_set_int(L, "a", l->offset_a);
    athena_lua_set_int(L, "zindex", l->offset_zindex);
    lua_setfield(L, -2, "offsets");
}

/* ----------------------------------------------------------- Particles -- */

#define PSYSTEM_CLASS "ParticleSystem"
#define PEMITTER_CLASS "ParticleEmitter"

typedef struct {
    AthenaParticles *system;
    AthenaParticleRenderer *renderer;
    float texture_width, texture_height;
} LuaParticleSystem;

typedef struct {
    uint32_t id;
    AthenaParticleEffect effect;
    float rate;
} LuaParticleEmitter;

static LuaParticleSystem *check_psystem(lua_State *L, int idx)
{
    LuaParticleSystem *s = athena_lua_check_object(L, idx, PSYSTEM_CLASS);
    if (!s->system)
        luaL_error(L, "particle system is disposed");
    return s;
}

/* The emitter keeps its system in field "__system". */
static LuaParticleEmitter *check_emitter(lua_State *L, int idx, LuaParticleSystem **sys)
{
    LuaParticleEmitter *e = athena_lua_check_object(L, idx, PEMITTER_CLASS);
    lua_getfield(L, idx, "__system");
    *sys = check_psystem(L, -1);
    lua_pop(L, 1);
    return e;
}

static float finite_number(lua_State *L, int idx)
{
    float v = (float)luaL_checknumber(L, idx);
    if (!isfinite(v))
        luaL_error(L, "particle values must be finite");
    return v;
}

static void opt_scalar(lua_State *L, int t, const char *key, float *out)
{
    if (lua_getfield(L, t, key) != LUA_TNIL)
        *out = finite_number(L, -1);
    lua_pop(L, 1);
}

static void opt_vector(lua_State *L, int t, const char *key, float *out, int n)
{
    if (lua_getfield(L, t, key) != LUA_TNIL) {
        luaL_checktype(L, -1, LUA_TTABLE);
        if (luaL_len(L, -1) != n)
            luaL_error(L, "'%s' needs %d components", key, n);
        for (int i = 0; i < n; i++) {
            lua_geti(L, -1, i + 1);
            out[i] = finite_number(L, -1);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
}

static void opt_range(lua_State *L, int t, const char *key, float *min, float *max)
{
    int type = lua_getfield(L, t, key);
    if (type == LUA_TNUMBER) {
        *min = *max = finite_number(L, -1);
    } else if (type != LUA_TNIL) {
        float pair[2];
        lua_pop(L, 1);
        opt_vector(L, t, key, pair, 2);
        *min = pair[0];
        *max = pair[1];
        return;
    }
    lua_pop(L, 1);
}

static void opt_uint(lua_State *L, int t, const char *key, uint32_t *out, uint32_t lo, uint32_t hi)
{
    if (lua_getfield(L, t, key) != LUA_TNIL) {
        lua_Integer v = luaL_checkinteger(L, -1);
        if (v < (lua_Integer)lo || (uint32_t)v > hi)
            luaL_error(L, "'%s' outside supported range", key);
        *out = (uint32_t)v;
    }
    lua_pop(L, 1);
}

static void effect_options(lua_State *L, int t, AthenaParticleEffect *e, float *rate, const LuaParticleSystem *s)
{
    luaL_checktype(L, t, LUA_TTABLE);
    opt_range(L, t, "lifetime", &e->life_min, &e->life_max);
    opt_range(L, t, "size", &e->size_min, &e->size_max);
    opt_scalar(L, t, "scaleEnd", &e->scale_end);
    opt_scalar(L, t, "aspect", &e->aspect);
    opt_scalar(L, t, "drag", &e->drag);
    opt_scalar(L, t, "fadeIn", &e->fade_in);
    opt_scalar(L, t, "fadeOut", &e->fade_out);
    opt_scalar(L, t, "frameRate", &e->frame_rate);
    opt_scalar(L, t, "rate", rate);
    opt_vector(L, t, "velocityMin", e->velocity_min, 3);
    opt_vector(L, t, "velocityMax", e->velocity_max, 3);
    opt_vector(L, t, "spread", e->spread, 3);
    opt_vector(L, t, "acceleration", e->acceleration, 3);
    opt_vector(L, t, "color", e->color, 4);
    opt_vector(L, t, "colorEnd", e->color_end, 4);
    opt_vector(L, t, "frame", e->frame, 4);
    opt_uint(L, t, "frameCount", &e->frame_count, 1, 4096);
    opt_uint(L, t, "frameColumns", &e->frame_columns, 1, 4096);
    bool loop = e->frame_loop;
    athena_lua_field_bool(L, t, "frameLoop", &loop);
    e->frame_loop = loop;

    float columns = e->frame_count < e->frame_columns ? e->frame_count : e->frame_columns;
    float rows = (e->frame_count + e->frame_columns - 1) / e->frame_columns;
    if (!particles_effect_valid(e) || *rate < 0 || *rate > 1000000 ||
        e->frame[0] + columns * e->frame[2] > s->texture_width ||
        e->frame[1] + rows * e->frame[3] > s->texture_height)
        luaL_error(L, "invalid particle effect ranges or atlas frames outside texture");
}

/* Particles.System{ texture = Image, space = "screen"|"world", blend = "alpha"|"additive",
 *                   capacity, seed, sort, depthTest } */
static int l_psystem_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    LuaParticleSystem *s = athena_lua_new_object(L, PSYSTEM_CLASS, sizeof(LuaParticleSystem));
    int self = lua_gettop(L);

    lua_getfield(L, 1, "texture");
    AthenaImage *image = athena_lua_check_image(L, -1);
    if (!image->tex || !athena_image_is_loaded(image))
        return luaL_error(L, "particle texture must be loaded");
    lua_setfield(L, self, "__texture");

    bool world = false;
    if (lua_getfield(L, 1, "space") != LUA_TNIL) {
        const char *space = luaL_checkstring(L, -1);
        if (strcmp(space, "world") && strcmp(space, "screen"))
            return luaL_error(L, "particle space must be 'screen' or 'world'");
        world = !strcmp(space, "world");
    }
    lua_pop(L, 1);

    uint64_t equation = GS_ALPHA_BLEND_NORMAL;
    if (lua_getfield(L, 1, "blend") != LUA_TNIL) {
        const char *blend = luaL_checkstring(L, -1);
        if (!strcmp(blend, "additive"))
            equation = GS_ALPHA_BLEND_ADD;
        else if (strcmp(blend, "alpha"))
            return luaL_error(L, "particle blend must be 'alpha' or 'additive'");
    }
    lua_pop(L, 1);

    uint32_t capacity = 512, seed = 42978;
    bool sort = world, depth_test = world;
    opt_uint(L, 1, "capacity", &capacity, 1, ATHENA_PARTICLE_MAX_CAPACITY);
    athena_lua_field_u32(L, 1, "seed", &seed);
    athena_lua_field_bool(L, 1, "sort", &sort);
    athena_lua_field_bool(L, 1, "depthTest", &depth_test);

    s->texture_width = image->tex->Width;
    s->texture_height = image->tex->Height;
    s->system = particles_create(capacity, world, sort, seed);
    if (!s->system)
        return luaL_error(L, "out of memory");
    s->renderer = athena_particles_renderer_create(s->system, image, equation, depth_test);
    if (!s->renderer)
        return luaL_error(L, "out of memory");

    lua_settop(L, self);
    return 1;
}

static int l_psystem_dispose(lua_State *L)
{
    LuaParticleSystem *s = athena_lua_check_object(L, 1, PSYSTEM_CLASS);
    athena_particles_renderer_destroy(s->renderer);
    s->renderer = NULL;
    particles_destroy(s->system);
    s->system = NULL;
    return 0;
}

static int l_psystem_create_emitter(lua_State *L)
{
    lua_settop(L, 2); /* optional args must not alias the new object */
    LuaParticleSystem *s = check_psystem(L, 1);
    LuaParticleEmitter *e = athena_lua_new_object(L, PEMITTER_CLASS, sizeof(LuaParticleEmitter));
    int self = lua_gettop(L);

    particles_effect_defaults(&e->effect);
    e->effect.frame[2] = s->texture_width;
    e->effect.frame[3] = s->texture_height;
    if (!lua_isnoneornil(L, 2))
        effect_options(L, 2, &e->effect, &e->rate, s);

    int id = particles_add_emitter(s->system, &e->effect);
    if (id < 0)
        return luaL_error(L, "particle emitter limit reached (64 per system)");
    e->id = id;
    athena_lua_retain(L, self, 1, "__system");
    lua_settop(L, self);
    return 1;
}

static int l_psystem_update(lua_State *L)
{
    LuaParticleSystem *s = check_psystem(L, 1);
    if (!particles_update(s->system, finite_number(L, 2)))
        return luaL_error(L, "particle dt must be nonnegative");
    return 0;
}

static int l_psystem_draw(lua_State *L)
{
    lua_pushinteger(L, athena_particles_draw(check_psystem(L, 1)->renderer));
    return 1;
}

static int l_psystem_clear(lua_State *L)
{
    LuaParticleSystem *s = check_psystem(L, 1);
    particles_clear(s->system, !lua_isnoneornil(L, 2), athena_lua_optu32(L, 2, 0));
    return 0;
}

static int l_psystem_stats(lua_State *L)
{
    AthenaParticleStats st = particles_stats(check_psystem(L, 1)->system);
    lua_createtable(L, 0, 7);
    athena_lua_set_int(L, "capacity", st.capacity);
    athena_lua_set_int(L, "active", st.active);
    athena_lua_set_int(L, "visible", st.visible);
    athena_lua_set_int(L, "emitted", st.emitted);
    athena_lua_set_int(L, "dropped", st.dropped);
    athena_lua_set_int(L, "batches", st.batches);
    athena_lua_set_str(L, "sort", !st.sorted ? "none" : st.radix ? "radix" : "cached");
    return 1;
}

static int get_psystem_active(lua_State *L)
{
    lua_pushinteger(L, particles_stats(check_psystem(L, 1)->system).active);
    return 1;
}

static int get_psystem_capacity(lua_State *L)
{
    lua_pushinteger(L, particles_stats(check_psystem(L, 1)->system).capacity);
    return 1;
}

static void read_position(lua_State *L, int base, float p[3])
{
    p[0] = finite_number(L, base);
    p[1] = finite_number(L, base + 1);
    p[2] = lua_isnoneornil(L, base + 2) ? 0.0f : finite_number(L, base + 2);
}

static int l_emitter_set_position(lua_State *L)
{
    LuaParticleSystem *s;
    LuaParticleEmitter *e = check_emitter(L, 1, &s);
    float p[3];
    read_position(L, 2, p);
    particles_position(s->system, e->id, p);
    return 0;
}

static int l_emitter_burst(lua_State *L)
{
    LuaParticleSystem *s;
    LuaParticleEmitter *e = check_emitter(L, 1, &s);
    uint32_t count = athena_lua_checku32(L, 2);
    float p[3];
    bool has_pos = !lua_isnoneornil(L, 3);
    if (has_pos)
        read_position(L, 3, p);
    lua_pushinteger(L, particles_burst(s->system, e->id, count, has_pos ? p : NULL));
    return 1;
}

static int l_emitter_start(lua_State *L)
{
    LuaParticleSystem *s;
    LuaParticleEmitter *e = check_emitter(L, 1, &s);
    float rate = lua_isnoneornil(L, 2) ? e->rate : finite_number(L, 2);
    if (!particles_start(s->system, e->id, rate))
        return luaL_error(L, "particle rate must be 0..1000000");
    e->rate = rate;
    return 0;
}

static int l_emitter_stop(lua_State *L)
{
    LuaParticleSystem *s;
    LuaParticleEmitter *e = check_emitter(L, 1, &s);
    particles_stop(s->system, e->id);
    return 0;
}

static int l_emitter_configure(lua_State *L)
{
    LuaParticleSystem *s;
    LuaParticleEmitter *e = check_emitter(L, 1, &s);
    AthenaParticleEffect effect = e->effect;
    float rate = e->rate;
    effect_options(L, 2, &effect, &rate, s);
    particles_configure(s->system, e->id, &effect);
    e->effect = effect;
    e->rate = rate;
    return 0;
}

static const athena_lua_prop psystem_props[] = {
    { "activeCount", get_psystem_active, NULL },
    { "capacity", get_psystem_capacity, NULL },
    { NULL, NULL, NULL }
};

static const luaL_Reg psystem_methods[] = {
    { "createEmitter", l_psystem_create_emitter }, { "update", l_psystem_update }, { "draw", l_psystem_draw },
    { "clear", l_psystem_clear }, { "dispose", l_psystem_dispose }, { "stats", l_psystem_stats },
    { NULL, NULL }
};

static const athena_lua_class psystem_class = {
    .name = PSYSTEM_CLASS, .methods = psystem_methods, .props = psystem_props, .gc = l_psystem_dispose,
};

static const luaL_Reg emitter_methods[] = {
    { "setPosition", l_emitter_set_position }, { "burst", l_emitter_burst }, { "start", l_emitter_start },
    { "stop", l_emitter_stop }, { "configure", l_emitter_configure },
    { NULL, NULL }
};

static const athena_lua_class emitter_class = {
    .name = PEMITTER_CLASS, .methods = emitter_methods,
};

static const luaL_Reg particles_funcs[] = {
    { "System", l_psystem_new },
    { NULL, NULL }
};

void athena_lua_open_fx(lua_State *L)
{
    athena_lua_register_class(L, &proj_class);
    athena_lua_new_module(L, NULL, shadow_consts, NULL);
    lua_pushcfunction(L, l_proj_new);
    lua_setfield(L, -2, "Projector");
    athena_lua_set_global_module(L, "Shadows");

    athena_lua_register_class(L, &descriptor_class);
    athena_lua_register_class(L, &instance_class);
    athena_lua_new_module(L, tilemap_funcs, NULL, NULL);
    athena_lua_new_module(L, spritebuffer_funcs, NULL, NULL);
    lua_setfield(L, -2, "SpriteBuffer");
    push_layout(L);
    lua_setfield(L, -2, "layout");
    athena_lua_set_global_module(L, "TileMap");

    athena_lua_register_class(L, &psystem_class);
    athena_lua_register_class(L, &emitter_class);
    athena_lua_new_module(L, particles_funcs, NULL, NULL);
    athena_lua_set_global_module(L, "Particles");
}
