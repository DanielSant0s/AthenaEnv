#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ath_lua.h"
#include "ath_lua_math.h"

#include <athena/vec2.h>
#include <athena/vec3.h>
#include <athena/vec4.h>
#include <athena/matrix.h>

/*
 * Vector2/Vector3 hold their floats inline. Vector4 and Matrix4 point at
 * facade-allocated (VU-aligned) storage, or at engine memory when pushed as a
 * view (bone matrices, object transforms); views retain their owner object.
 *
 * Besides the JS method names (add, sub, mul, div, equals...), Lua gets real
 * operators: a + b, a - b, a * b (component-wise, or by a number), a / b,
 * -a, a == b, tostring(a). Matrix4 elements are m[1]..m[16].
 */

/* -------------------------------------------------------------- Vector2 -- */

#define VEC2 "Vector2"
#define VEC3 "Vector3"

static AthenaVec2 *check_vec2(lua_State *L, int idx)
{
    return athena_lua_check_object(L, idx, VEC2);
}

static AthenaVec2 *push_vec2(lua_State *L, float x, float y)
{
    AthenaVec2 *v = athena_lua_new_object(L, VEC2, sizeof(AthenaVec2));
    v->x = x;
    v->y = y;
    return v;
}

static int l_vec2_new(lua_State *L)
{
    push_vec2(L, athena_lua_optfloat(L, 1, 0), athena_lua_optfloat(L, 2, 0));
    return 1;
}

static int get_v2_x(lua_State *L) { lua_pushnumber(L, check_vec2(L, 1)->x); return 1; }
static int get_v2_y(lua_State *L) { lua_pushnumber(L, check_vec2(L, 1)->y); return 1; }
static int set_v2_x(lua_State *L) { check_vec2(L, 1)->x = athena_lua_checkfloat(L, 2); return 0; }
static int set_v2_y(lua_State *L) { check_vec2(L, 1)->y = athena_lua_checkfloat(L, 2); return 0; }

static int l_vec2_norm(lua_State *L) { lua_pushnumber(L, athena_vec2_norm(check_vec2(L, 1))); return 1; }
static int l_vec2_dot(lua_State *L) { lua_pushnumber(L, athena_vec2_dot(check_vec2(L, 1), check_vec2(L, 2))); return 1; }
static int l_vec2_cross(lua_State *L) { lua_pushnumber(L, athena_vec2_cross(check_vec2(L, 1), check_vec2(L, 2))); return 1; }
static int l_vec2_dist(lua_State *L) { lua_pushnumber(L, athena_vec2_dist(check_vec2(L, 1), check_vec2(L, 2))); return 1; }
static int l_vec2_distsqr(lua_State *L) { lua_pushnumber(L, athena_vec2_distsqr(check_vec2(L, 1), check_vec2(L, 2))); return 1; }

static int l_vec2_tostring(lua_State *L)
{
    char buf[64];
    athena_vec2_tostring(check_vec2(L, 1), buf, sizeof(buf));
    lua_pushstring(L, buf);
    return 1;
}

/* Operand that may be a vector or a number (broadcast). */
static AthenaVec2 to_vec2_operand(lua_State *L, int idx)
{
    if (lua_type(L, idx) == LUA_TNUMBER) {
        float f = (float)lua_tonumber(L, idx);
        return (AthenaVec2){ f, f };
    }
    return *check_vec2(L, idx);
}

#define VEC2_OP(name, op) \
    static int l_vec2_##name(lua_State *L) { \
        AthenaVec2 a = to_vec2_operand(L, 1), b = to_vec2_operand(L, 2); \
        push_vec2(L, a.x op b.x, a.y op b.y); return 1; }

VEC2_OP(add, +)
VEC2_OP(sub, -)
VEC2_OP(mul, *)
VEC2_OP(div, /)

static int l_vec2_unm(lua_State *L)
{
    AthenaVec2 *v = check_vec2(L, 1);
    push_vec2(L, -v->x, -v->y);
    return 1;
}

static int l_vec2_eq(lua_State *L)
{
    AthenaVec2 *a = check_vec2(L, 1), *b = check_vec2(L, 2);
    lua_pushboolean(L, a->x == b->x && a->y == b->y);
    return 1;
}

static int l_vec2_unpack(lua_State *L)
{
    AthenaVec2 *v = check_vec2(L, 1);
    lua_pushnumber(L, v->x);
    lua_pushnumber(L, v->y);
    return 2;
}

static const luaL_Reg vec2_methods[] = {
    { "norm", l_vec2_norm }, { "dot", l_vec2_dot }, { "cross", l_vec2_cross },
    { "dist", l_vec2_dist }, { "distsqr", l_vec2_distsqr }, { "toString", l_vec2_tostring },
    { "add", l_vec2_add }, { "sub", l_vec2_sub }, { "mul", l_vec2_mul }, { "div", l_vec2_div },
    { "equals", l_vec2_eq }, { "unpack", l_vec2_unpack },
    { NULL, NULL }
};

static const athena_lua_prop vec2_props[] = {
    { "x", get_v2_x, set_v2_x }, { "y", get_v2_y, set_v2_y },
    { NULL, NULL, NULL }
};

static const luaL_Reg vec2_meta[] = {
    { "__add", l_vec2_add }, { "__sub", l_vec2_sub }, { "__mul", l_vec2_mul }, { "__div", l_vec2_div },
    { "__unm", l_vec2_unm }, { "__eq", l_vec2_eq },
    { NULL, NULL }
};

static const athena_lua_class vec2_class = {
    .name = VEC2, .methods = vec2_methods, .props = vec2_props,
    .tostring = l_vec2_tostring, .meta = vec2_meta,
};

/* -------------------------------------------------------------- Vector3 -- */

static AthenaVec3 *check_vec3(lua_State *L, int idx)
{
    return athena_lua_check_object(L, idx, VEC3);
}

AthenaVec3 *athena_lua_push_vec3(lua_State *L, float x, float y, float z)
{
    AthenaVec3 *v = athena_lua_new_object(L, VEC3, sizeof(AthenaVec3));
    v->x = x;
    v->y = y;
    v->z = z;
    return v;
}

AthenaVec3 *athena_lua_check_vec3(lua_State *L, int idx)
{
    return check_vec3(L, idx);
}

AthenaVec3 *athena_lua_test_vec3(lua_State *L, int idx)
{
    return athena_lua_test_object(L, idx, VEC3);
}

static int l_vec3_new(lua_State *L)
{
    athena_lua_push_vec3(L, athena_lua_optfloat(L, 1, 0), athena_lua_optfloat(L, 2, 0), athena_lua_optfloat(L, 3, 0));
    return 1;
}

static int get_v3_x(lua_State *L) { lua_pushnumber(L, check_vec3(L, 1)->x); return 1; }
static int get_v3_y(lua_State *L) { lua_pushnumber(L, check_vec3(L, 1)->y); return 1; }
static int get_v3_z(lua_State *L) { lua_pushnumber(L, check_vec3(L, 1)->z); return 1; }
static int set_v3_x(lua_State *L) { check_vec3(L, 1)->x = athena_lua_checkfloat(L, 2); return 0; }
static int set_v3_y(lua_State *L) { check_vec3(L, 1)->y = athena_lua_checkfloat(L, 2); return 0; }
static int set_v3_z(lua_State *L) { check_vec3(L, 1)->z = athena_lua_checkfloat(L, 2); return 0; }

static int l_vec3_norm(lua_State *L) { lua_pushnumber(L, athena_vec3_norm(check_vec3(L, 1))); return 1; }
static int l_vec3_dot(lua_State *L) { lua_pushnumber(L, athena_vec3_dot(check_vec3(L, 1), check_vec3(L, 2))); return 1; }
static int l_vec3_dist(lua_State *L) { lua_pushnumber(L, athena_vec3_dist(check_vec3(L, 1), check_vec3(L, 2))); return 1; }
static int l_vec3_distsqr(lua_State *L) { lua_pushnumber(L, athena_vec3_distsqr(check_vec3(L, 1), check_vec3(L, 2))); return 1; }

static int l_vec3_cross(lua_State *L)
{
    AthenaVec3 *a = check_vec3(L, 1), *b = check_vec3(L, 2);
    athena_lua_push_vec3(L, a->y * b->z - a->z * b->y, a->z * b->x - a->x * b->z, a->x * b->y - a->y * b->x);
    return 1;
}

static int l_vec3_tostring(lua_State *L)
{
    char buf[96];
    athena_vec3_tostring(check_vec3(L, 1), buf, sizeof(buf));
    lua_pushstring(L, buf);
    return 1;
}

static AthenaVec3 to_vec3_operand(lua_State *L, int idx)
{
    if (lua_type(L, idx) == LUA_TNUMBER) {
        float f = (float)lua_tonumber(L, idx);
        return (AthenaVec3){ f, f, f };
    }
    return *check_vec3(L, idx);
}

#define VEC3_OP(name, op) \
    static int l_vec3_##name(lua_State *L) { \
        AthenaVec3 a = to_vec3_operand(L, 1), b = to_vec3_operand(L, 2); \
        athena_lua_push_vec3(L, a.x op b.x, a.y op b.y, a.z op b.z); return 1; }

VEC3_OP(add, +)
VEC3_OP(sub, -)
VEC3_OP(mul, *)
VEC3_OP(div, /)

static int l_vec3_unm(lua_State *L)
{
    AthenaVec3 *v = check_vec3(L, 1);
    athena_lua_push_vec3(L, -v->x, -v->y, -v->z);
    return 1;
}

static int l_vec3_eq(lua_State *L)
{
    AthenaVec3 *a = check_vec3(L, 1), *b = check_vec3(L, 2);
    lua_pushboolean(L, a->x == b->x && a->y == b->y && a->z == b->z);
    return 1;
}

static int l_vec3_normalize(lua_State *L)
{
    AthenaVec3 *v = check_vec3(L, 1);
    float n = athena_vec3_norm(v);
    if (n > 0.0f)
        athena_lua_push_vec3(L, v->x / n, v->y / n, v->z / n);
    else
        athena_lua_push_vec3(L, 0, 0, 0);
    return 1;
}

static int l_vec3_unpack(lua_State *L)
{
    AthenaVec3 *v = check_vec3(L, 1);
    lua_pushnumber(L, v->x);
    lua_pushnumber(L, v->y);
    lua_pushnumber(L, v->z);
    return 3;
}

static const luaL_Reg vec3_methods[] = {
    { "norm", l_vec3_norm }, { "dot", l_vec3_dot }, { "cross", l_vec3_cross },
    { "dist", l_vec3_dist }, { "distsqr", l_vec3_distsqr }, { "toString", l_vec3_tostring },
    { "add", l_vec3_add }, { "sub", l_vec3_sub }, { "mul", l_vec3_mul }, { "div", l_vec3_div },
    { "equals", l_vec3_eq }, { "normalize", l_vec3_normalize }, { "unpack", l_vec3_unpack },
    { NULL, NULL }
};

static const athena_lua_prop vec3_props[] = {
    { "x", get_v3_x, set_v3_x }, { "y", get_v3_y, set_v3_y }, { "z", get_v3_z, set_v3_z },
    { NULL, NULL, NULL }
};

static const luaL_Reg vec3_meta[] = {
    { "__add", l_vec3_add }, { "__sub", l_vec3_sub }, { "__mul", l_vec3_mul }, { "__div", l_vec3_div },
    { "__unm", l_vec3_unm }, { "__eq", l_vec3_eq },
    { NULL, NULL }
};

static const athena_lua_class vec3_class = {
    .name = VEC3, .methods = vec3_methods, .props = vec3_props,
    .tostring = l_vec3_tostring, .meta = vec3_meta,
};

/* -------------------------------------------------------------- Vector4 -- */

typedef struct {
    AthenaVec4 *v;
    bool owned;
} LuaVec4;

static AthenaVec4 *check_vec4(lua_State *L, int idx)
{
    return ((LuaVec4 *)athena_lua_check_object(L, idx, ATHENA_LUA_VEC4))->v;
}

AthenaVec4 *athena_lua_check_vec4(lua_State *L, int idx)
{
    return check_vec4(L, idx);
}

static void push_vec4_owned(lua_State *L, AthenaVec4 *v)
{
    if (!v)
        luaL_error(L, "out of memory");
    LuaVec4 *u = athena_lua_new_object(L, ATHENA_LUA_VEC4, sizeof(LuaVec4));
    u->v = v;
    u->owned = true;
}

void athena_lua_push_vec4_view(lua_State *L, float *data, int owner)
{
    owner = lua_absindex(L, owner);
    LuaVec4 *u = athena_lua_new_object(L, ATHENA_LUA_VEC4, sizeof(LuaVec4));
    u->v = (AthenaVec4 *)data;
    u->owned = false;
    athena_lua_retain(L, -1, owner, "__owner");
}

static int l_vec4_new(lua_State *L)
{
    push_vec4_owned(L, athena_vec4_create(athena_lua_optfloat(L, 1, 0), athena_lua_optfloat(L, 2, 0),
                                          athena_lua_optfloat(L, 3, 0), athena_lua_optfloat(L, 4, 0)));
    return 1;
}

static int l_vec4_gc(lua_State *L)
{
    LuaVec4 *u = athena_lua_check_object(L, 1, ATHENA_LUA_VEC4);
    if (u->owned && u->v)
        athena_vec4_destroy(u->v);
    u->v = NULL;
    return 0;
}

#define VEC4_COMP(c) \
    static int get_v4_##c(lua_State *L) { lua_pushnumber(L, check_vec4(L, 1)->v.c); return 1; } \
    static int set_v4_##c(lua_State *L) { check_vec4(L, 1)->v.c = athena_lua_checkfloat(L, 2); return 0; }

VEC4_COMP(x)
VEC4_COMP(y)
VEC4_COMP(z)
VEC4_COMP(w)

static int l_vec4_norm(lua_State *L) { lua_pushnumber(L, athena_vec4_norm(check_vec4(L, 1))); return 1; }
static int l_vec4_dot(lua_State *L) { lua_pushnumber(L, athena_vec4_dot(check_vec4(L, 1), check_vec4(L, 2))); return 1; }
static int l_vec4_dist(lua_State *L) { lua_pushnumber(L, athena_vec4_distance(check_vec4(L, 1), check_vec4(L, 2))); return 1; }
static int l_vec4_dist2(lua_State *L) { lua_pushnumber(L, athena_vec4_distance2(check_vec4(L, 1), check_vec4(L, 2))); return 1; }
static int l_vec4_eq(lua_State *L) { lua_pushboolean(L, athena_vec4_equals(check_vec4(L, 1), check_vec4(L, 2))); return 1; }
static int l_vec4_cross(lua_State *L) { push_vec4_owned(L, athena_vec4_cross(check_vec4(L, 1), check_vec4(L, 2))); return 1; }

static int l_vec4_tostring(lua_State *L)
{
    char buf[128];
    athena_vec4_tostring(check_vec4(L, 1), buf, sizeof(buf));
    lua_pushstring(L, buf);
    return 1;
}

static AthenaVec4 *to_vec4_operand(lua_State *L, int idx, AthenaVec4 *tmp)
{
    if (lua_type(L, idx) == LUA_TNUMBER) {
        float f = (float)lua_tonumber(L, idx);
        tmp->v.x = tmp->v.y = tmp->v.z = tmp->v.w = f;
        return tmp;
    }
    return check_vec4(L, idx);
}

#define VEC4_OP(name) \
    static int l_vec4_##name(lua_State *L) { \
        AthenaVec4 *ta = athena_vec4_create(0, 0, 0, 0), *tb = athena_vec4_create(0, 0, 0, 0); \
        AthenaVec4 *r = athena_vec4_##name(to_vec4_operand(L, 1, ta), to_vec4_operand(L, 2, tb)); \
        athena_vec4_destroy(ta); athena_vec4_destroy(tb); \
        push_vec4_owned(L, r); return 1; }

VEC4_OP(add)
VEC4_OP(sub)
VEC4_OP(mul)
VEC4_OP(div)

static int l_vec4_unm(lua_State *L)
{
    AthenaVec4 *v = check_vec4(L, 1);
    push_vec4_owned(L, athena_vec4_create(-v->v.x, -v->v.y, -v->v.z, -v->v.w));
    return 1;
}

static int l_vec4_unpack(lua_State *L)
{
    AthenaVec4 *v = check_vec4(L, 1);
    lua_pushnumber(L, v->v.x);
    lua_pushnumber(L, v->v.y);
    lua_pushnumber(L, v->v.z);
    lua_pushnumber(L, v->v.w);
    return 4;
}

static const luaL_Reg vec4_methods[] = {
    { "norm", l_vec4_norm }, { "dot", l_vec4_dot }, { "cross", l_vec4_cross },
    { "distance", l_vec4_dist }, { "distance2", l_vec4_dist2 }, { "toString", l_vec4_tostring },
    { "add", l_vec4_add }, { "sub", l_vec4_sub }, { "mul", l_vec4_mul }, { "div", l_vec4_div },
    { "equals", l_vec4_eq }, { "unpack", l_vec4_unpack },
    { NULL, NULL }
};

static const athena_lua_prop vec4_props[] = {
    { "x", get_v4_x, set_v4_x }, { "y", get_v4_y, set_v4_y },
    { "z", get_v4_z, set_v4_z }, { "w", get_v4_w, set_v4_w },
    { NULL, NULL, NULL }
};

static const luaL_Reg vec4_meta[] = {
    { "__add", l_vec4_add }, { "__sub", l_vec4_sub }, { "__mul", l_vec4_mul }, { "__div", l_vec4_div },
    { "__unm", l_vec4_unm }, { "__eq", l_vec4_eq },
    { NULL, NULL }
};

static const athena_lua_class vec4_class = {
    .name = ATHENA_LUA_VEC4, .methods = vec4_methods, .props = vec4_props,
    .gc = l_vec4_gc, .tostring = l_vec4_tostring, .meta = vec4_meta,
};

/* -------------------------------------------------------------- Matrix4 -- */

typedef struct {
    AthenaMatrix4 *m;
    bool owned;
} LuaMatrix4;

AthenaMatrix4 *athena_lua_check_matrix(lua_State *L, int idx)
{
    return ((LuaMatrix4 *)athena_lua_check_object(L, idx, ATHENA_LUA_MATRIX4))->m;
}

static void push_matrix_owned(lua_State *L, AthenaMatrix4 *m)
{
    if (!m)
        luaL_error(L, "out of memory");
    LuaMatrix4 *u = athena_lua_new_object(L, ATHENA_LUA_MATRIX4, sizeof(LuaMatrix4));
    u->m = m;
    u->owned = true;
}

void athena_lua_push_matrix_view(lua_State *L, float *data, int owner)
{
    owner = lua_absindex(L, owner);
    LuaMatrix4 *u = athena_lua_new_object(L, ATHENA_LUA_MATRIX4, sizeof(LuaMatrix4));
    u->m = (AthenaMatrix4 *)data;
    u->owned = false;
    athena_lua_retain(L, -1, owner, "__owner");
}

void athena_lua_push_matrix_copy(lua_State *L, const float *data)
{
    push_matrix_owned(L, athena_matrix4_create_from(data));
}

/* Matrix4() identity, Matrix4(m1..m16), or Matrix4({m1..m16}) */
static int l_matrix_new(lua_State *L)
{
    float values[ATHENA_MATRIX4_LENGTH];

    if (lua_istable(L, 1)) {
        for (int i = 0; i < ATHENA_MATRIX4_LENGTH; i++) {
            lua_geti(L, 1, i + 1);
            values[i] = (float)luaL_checknumber(L, -1);
            lua_pop(L, 1);
        }
        push_matrix_owned(L, athena_matrix4_create_from(values));
    } else if (lua_gettop(L) >= ATHENA_MATRIX4_LENGTH) {
        for (int i = 0; i < ATHENA_MATRIX4_LENGTH; i++)
            values[i] = athena_lua_checkfloat(L, i + 1);
        push_matrix_owned(L, athena_matrix4_create_from(values));
    } else {
        push_matrix_owned(L, athena_matrix4_create());
    }
    return 1;
}

static int l_matrix_gc(lua_State *L)
{
    LuaMatrix4 *u = athena_lua_check_object(L, 1, ATHENA_LUA_MATRIX4);
    if (u->owned && u->m)
        athena_matrix4_destroy(u->m);
    u->m = NULL;
    return 0;
}

static int l_matrix_tostring(lua_State *L)
{
    char buf[512];
    athena_matrix4_tostring(athena_lua_check_matrix(L, 1), buf, sizeof(buf));
    lua_pushstring(L, buf);
    return 1;
}

static int l_matrix_toarray(lua_State *L)
{
    float values[ATHENA_MATRIX4_LENGTH];
    athena_matrix4_to_array(athena_lua_check_matrix(L, 1), values);
    lua_createtable(L, ATHENA_MATRIX4_LENGTH, 0);
    for (int i = 0; i < ATHENA_MATRIX4_LENGTH; i++) {
        lua_pushnumber(L, values[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int l_matrix_fromarray(lua_State *L)
{
    AthenaMatrix4 *m = athena_lua_check_matrix(L, 1);
    float values[ATHENA_MATRIX4_LENGTH];
    luaL_checktype(L, 2, LUA_TTABLE);
    for (int i = 0; i < ATHENA_MATRIX4_LENGTH; i++) {
        lua_geti(L, 2, i + 1);
        values[i] = (float)luaL_checknumber(L, -1);
        lua_pop(L, 1);
    }
    athena_matrix4_from_array(m, values);
    return 0;
}

static int l_matrix_mul(lua_State *L)
{
    push_matrix_owned(L, athena_matrix4_multiply(athena_lua_check_matrix(L, 1), athena_lua_check_matrix(L, 2)));
    return 1;
}

static int l_matrix_eq(lua_State *L)
{
    lua_pushboolean(L, athena_matrix4_equals(athena_lua_check_matrix(L, 1), athena_lua_check_matrix(L, 2)));
    return 1;
}

static int l_matrix_copy(lua_State *L)
{
    athena_matrix4_copy(athena_lua_check_matrix(L, 1), athena_lua_check_matrix(L, 2));
    return 0;
}

static int l_matrix_identity(lua_State *L)
{
    athena_matrix4_identity(athena_lua_check_matrix(L, 1));
    lua_settop(L, 1);
    return 1;
}

static int l_matrix_transpose(lua_State *L)
{
    athena_matrix4_transpose(athena_lua_check_matrix(L, 1));
    lua_settop(L, 1);
    return 1;
}

static int l_matrix_invert(lua_State *L)
{
    athena_matrix4_invert(athena_lua_check_matrix(L, 1));
    lua_settop(L, 1);
    return 1;
}

static int l_matrix_clone(lua_State *L)
{
    push_matrix_owned(L, athena_matrix4_clone(athena_lua_check_matrix(L, 1)));
    return 1;
}

static int l_matrix_len(lua_State *L)
{
    lua_pushinteger(L, ATHENA_MATRIX4_LENGTH);
    return 1;
}

static int get_matrix_length(lua_State *L)
{
    lua_pushinteger(L, ATHENA_MATRIX4_LENGTH);
    return 1;
}

/* Numeric keys index the elements (1..16); everything else goes to the
 * generic class __index stored as upvalue 1. */
static int l_matrix_index(lua_State *L)
{
    if (lua_type(L, 2) == LUA_TNUMBER) {
        lua_Integer i = luaL_checkinteger(L, 2);
        if (i < 1 || i > ATHENA_MATRIX4_LENGTH)
            return luaL_error(L, "matrix index %d out of range 1..16", (int)i);
        lua_pushnumber(L, athena_matrix4_get(athena_lua_check_matrix(L, 1), (int)i - 1));
        return 1;
    }
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, 2, 1);
    return 1;
}

static int l_matrix_newindex(lua_State *L)
{
    if (lua_type(L, 2) == LUA_TNUMBER) {
        lua_Integer i = luaL_checkinteger(L, 2);
        if (i < 1 || i > ATHENA_MATRIX4_LENGTH)
            return luaL_error(L, "matrix index %d out of range 1..16", (int)i);
        athena_matrix4_set(athena_lua_check_matrix(L, 1), (int)i - 1, athena_lua_checkfloat(L, 3));
        return 0;
    }
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, 3, 0);
    return 0;
}

static const luaL_Reg matrix_methods[] = {
    { "toString", l_matrix_tostring }, { "toArray", l_matrix_toarray }, { "fromArray", l_matrix_fromarray },
    { "clone", l_matrix_clone }, { "copy", l_matrix_copy }, { "transpose", l_matrix_transpose },
    { "invert", l_matrix_invert }, { "identity", l_matrix_identity },
    { "mul", l_matrix_mul }, { "equals", l_matrix_eq },
    { NULL, NULL }
};

static const athena_lua_prop matrix_props[] = {
    { "length", get_matrix_length, NULL },
    { NULL, NULL, NULL }
};

static const luaL_Reg matrix_meta[] = {
    { "__mul", l_matrix_mul }, { "__eq", l_matrix_eq }, { "__len", l_matrix_len },
    { NULL, NULL }
};

static const athena_lua_class matrix_class = {
    .name = ATHENA_LUA_MATRIX4, .methods = matrix_methods, .props = matrix_props,
    .gc = l_matrix_gc, .tostring = l_matrix_tostring, .meta = matrix_meta,
};

static void wrap_matrix_access(lua_State *L)
{
    luaL_getmetatable(L, ATHENA_LUA_MATRIX4);
    lua_getfield(L, -1, "__index");
    lua_pushcclosure(L, l_matrix_index, 1);
    lua_setfield(L, -2, "__index");
    lua_getfield(L, -1, "__newindex");
    lua_pushcclosure(L, l_matrix_newindex, 1);
    lua_setfield(L, -2, "__newindex");
    lua_pop(L, 1);
}

void athena_lua_open_math(lua_State *L)
{
    athena_lua_register_class(L, &vec2_class);
    athena_lua_new_module(L, NULL, NULL, l_vec2_new);
    athena_lua_set_global_module(L, VEC2);

    athena_lua_register_class(L, &vec3_class);
    athena_lua_new_module(L, NULL, NULL, l_vec3_new);
    athena_lua_set_global_module(L, VEC3);

    athena_lua_register_class(L, &vec4_class);
    athena_lua_new_module(L, NULL, NULL, l_vec4_new);
    athena_lua_set_global_module(L, ATHENA_LUA_VEC4);

    athena_lua_register_class(L, &matrix_class);
    wrap_matrix_access(L);
    athena_lua_new_module(L, NULL, NULL, l_matrix_new);
    athena_lua_set_global_module(L, ATHENA_LUA_MATRIX4);
}
