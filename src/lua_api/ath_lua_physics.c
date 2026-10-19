#include <stdlib.h>
#include <string.h>

#include "ath_lua.h"
#include "ath_lua_math.h"
#include "ath_lua_render.h"
#include "ath_lua_physics.h"

#include <physics.h>
#include <athena/physics_facade.h>

/*
 * Physics. Same surface as js_api/ath_physics.c with Lua conventions:
 *  - vectors come back as multiple values: local x, y, z = body:getPosition()
 *  - rotations are 1-based 9-element tables (row-major 3x3)
 *  - contacts are tables { position = Vector3, normal = Vector3, depth, geom1, geom2 }
 * Native objects map 1:1 to Lua objects through a weak cache, so geom:getBody()
 * and contact.geom1 return the same object the script created (the JS version
 * wraps them again, which double-frees the body on GC).
 */

#define WORLD_CLASS "PhysicsWorld"
#define SPACE_CLASS "PhysicsSpace"
#define GEOM_CLASS "PhysicsGeom"
#define BODY_CLASS "PhysicsBody"
#define JOINT_CLASS "PhysicsJoint"
#define JGROUP_CLASS "PhysicsJointGroup"

#define MAX_CONTACTS 64

typedef struct {
    void *ptr;
} LuaPhysObj;

static const char cache_key = 0;

static void cache_put(lua_State *L, void *ptr, int obj)
{
    obj = lua_absindex(L, obj);
    lua_rawgetp(L, LUA_REGISTRYINDEX, &cache_key);
    lua_pushvalue(L, obj);
    lua_rawsetp(L, -2, ptr);
    lua_pop(L, 1);
}

static bool cache_push(lua_State *L, void *ptr)
{
    lua_rawgetp(L, LUA_REGISTRYINDEX, &cache_key);
    lua_rawgetp(L, -1, ptr);
    lua_remove(L, -2);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

static void cache_drop(lua_State *L, void *ptr)
{
    lua_rawgetp(L, LUA_REGISTRYINDEX, &cache_key);
    lua_pushnil(L);
    lua_rawsetp(L, -2, ptr);
    lua_pop(L, 1);
}

static void *push_phys(lua_State *L, const char *cls, void *ptr)
{
    if (!ptr)
        luaL_error(L, "failed to create %s", cls);
    LuaPhysObj *o = athena_lua_new_object(L, cls, sizeof(LuaPhysObj));
    o->ptr = ptr;
    cache_put(L, ptr, -1);
    return ptr;
}

static void *check_phys(lua_State *L, int idx, const char *cls)
{
    LuaPhysObj *o = athena_lua_check_object(L, idx, cls);
    if (!o->ptr)
        luaL_error(L, "%s was freed", cls);
    return o->ptr;
}

static void *opt_phys(lua_State *L, int idx, const char *cls)
{
    return lua_isnoneornil(L, idx) ? NULL : check_phys(L, idx, cls);
}

#define W(i) ((AthenaPhysicsWorld *)check_phys(L, i, WORLD_CLASS))
#define S(i) ((AthenaPhysicsSpace *)check_phys(L, i, SPACE_CLASS))
#define G(i) ((AthenaPhysicsGeom *)check_phys(L, i, GEOM_CLASS))
#define B(i) ((AthenaPhysicsBody *)check_phys(L, i, BODY_CLASS))
#define J(i) ((AthenaPhysicsJoint *)check_phys(L, i, JOINT_CLASS))
#define JG(i) ((AthenaPhysicsJointGroup *)check_phys(L, i, JGROUP_CLASS))
#define F(i) athena_lua_checkfloat(L, i)

AthenaPhysicsGeom *athena_lua_check_geom(lua_State *L, int idx) { return G(idx); }
AthenaPhysicsBody *athena_lua_check_body(lua_State *L, int idx) { return B(idx); }
AthenaPhysicsSpace *athena_lua_check_space(lua_State *L, int idx) { return S(idx); }

#define DESTROYER(name, cls, fn) \
    static int name(lua_State *L) { \
        LuaPhysObj *o = athena_lua_check_object(L, 1, cls); \
        if (o->ptr) { cache_drop(L, o->ptr); fn(o->ptr); o->ptr = NULL; } \
        return 0; }

DESTROYER(l_world_free, WORLD_CLASS, athena_physics_world_destroy)
DESTROYER(l_space_free, SPACE_CLASS, athena_physics_space_destroy)
DESTROYER(l_geom_free, GEOM_CLASS, athena_physics_geom_destroy)
DESTROYER(l_body_free, BODY_CLASS, athena_physics_body_destroy)
DESTROYER(l_joint_free, JOINT_CLASS, athena_physics_joint_destroy)
DESTROYER(l_jgroup_free, JGROUP_CLASS, athena_physics_joint_group_destroy)

static int push3(lua_State *L, const float *v)
{
    lua_pushnumber(L, v[0]);
    lua_pushnumber(L, v[1]);
    lua_pushnumber(L, v[2]);
    return 3;
}

static void read_r9(lua_State *L, int idx, float R[9])
{
    luaL_checktype(L, idx, LUA_TTABLE);
    for (int i = 0; i < 9; i++) {
        lua_geti(L, idx, i + 1);
        R[i] = (float)luaL_checknumber(L, -1);
        lua_pop(L, 1);
    }
}

static int push_r9(lua_State *L, const float R[9])
{
    lua_createtable(L, 9, 0);
    for (int i = 0; i < 9; i++) {
        lua_pushnumber(L, R[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static void push_geom_ref(lua_State *L, AthenaPhysicsGeom *g)
{
    if (!g || !cache_push(L, g))
        lua_pushnil(L);
}

static void push_contact(lua_State *L, const AthenaContact *c)
{
    lua_createtable(L, 0, 5);
    athena_lua_push_vec3(L, c->pos[0], c->pos[1], c->pos[2]);
    lua_setfield(L, -2, "position");
    athena_lua_push_vec3(L, c->normal[0], c->normal[1], c->normal[2]);
    lua_setfield(L, -2, "normal");
    athena_lua_set_num(L, "depth", c->depth);
    push_geom_ref(L, c->g1);
    lua_setfield(L, -2, "geom1");
    push_geom_ref(L, c->g2);
    lua_setfield(L, -2, "geom2");
}

/* ---------------------------------------------------------------- World -- */

static int l_world_new(lua_State *L) { push_phys(L, WORLD_CLASS, athena_physics_world_create()); return 1; }
static int l_world_set_gravity(lua_State *L) { athena_physics_world_set_gravity(W(1), F(2), F(3), F(4)); return 0; }
static int l_world_set_cfm(lua_State *L) { athena_physics_world_set_cfm(W(1), F(2)); return 0; }
static int l_world_set_erp(lua_State *L) { athena_physics_world_set_erp(W(1), F(2)); return 0; }
static int l_world_step(lua_State *L) { athena_physics_world_step(W(1), F(2)); return 0; }
static int l_world_quick_step(lua_State *L) { athena_physics_world_quick_step(W(1), F(2)); return 0; }
static int l_world_set_qs_iter(lua_State *L) { athena_physics_world_set_quick_step_iterations(W(1), (int)luaL_checkinteger(L, 2)); return 0; }

static int l_world_get_gravity(lua_State *L)
{
    float g[3];
    athena_physics_world_get_gravity_vec(W(1), g);
    return push3(L, g);
}

typedef struct {
    lua_State *L;
    AthenaPhysicsWorld *world;
    AthenaPhysicsJointGroup *group;
    int callback;   /* stack index or 0 */
    int contacts;   /* stack index of the result table */
    int count;
} contact_ctx;

/* Collision callbacks run synchronously inside collide()/stepWithContacts(),
 * so errors raised by the Lua callback unwind straight out of the call. */
static void report_contact(contact_ctx *c, const AthenaContact *contact)
{
    lua_State *L = c->L;
    push_contact(L, contact);
    lua_pushvalue(L, -1);
    lua_rawseti(L, c->contacts, ++c->count);
    if (c->callback) {
        lua_pushvalue(L, c->callback);
        lua_insert(L, -2);
        lua_call(L, 1, 0);
    } else {
        lua_pop(L, 1);
    }
}

static void near_callback(void *data, AthenaPhysicsGeom *o1, AthenaPhysicsGeom *o2)
{
    contact_ctx *c = data;
    AthenaContact contacts[MAX_CONTACTS] __attribute__((aligned(16)));
    int n = athena_physics_collide(o1, o2, MAX_CONTACTS, contacts);
    for (int i = 0; i < n; i++)
        report_contact(c, &contacts[i]);
}

static void contact_joint_callback(void *data, AthenaPhysicsGeom *o1, AthenaPhysicsGeom *o2)
{
    contact_ctx *c = data;
    AthenaPhysicsBody *b1 = athena_physics_geom_get_body(o1);
    AthenaPhysicsBody *b2 = athena_physics_geom_get_body(o2);

    if (!b1 && !b2)
        return;
    if (b1 && b2 && athena_physics_bodies_connected_excluding(b1, b2, ATH_JOINT_CONTACT))
        return;

    AthenaContact contacts[MAX_CONTACTS] __attribute__((aligned(16)));
    int n = athena_physics_collide(o1, o2, MAX_CONTACTS, contacts);
    for (int i = 0; i < n; i++) {
        contacts[i].mu = 0.5f;
        contacts[i].bounce = 0.1f;
        contacts[i].soft_cfm = 0.01f;

        if (c->callback)
            report_contact(c, &contacts[i]);
        else
            c->count++;

        AthenaPhysicsJoint *j = athena_physics_joint_create_contact(c->world, c->group, &contacts[i]);
        athena_physics_joint_attach(j, b1, b2);
    }
}

/* world:stepWithContacts(space, contactGroup, dt [, callback]) -> contacts
 * Without a callback, returns the contact count instead of a table. */
static int l_world_step_with_contacts(lua_State *L)
{
    contact_ctx c = { L, W(1), JG(3), 0, 0, 0 };
    AthenaPhysicsSpace *space = S(2);
    float dt = F(4);

    if (!lua_isnoneornil(L, 5)) {
        luaL_checktype(L, 5, LUA_TFUNCTION);
        c.callback = 5;
    }
    lua_newtable(L);
    c.contacts = lua_gettop(L);

    athena_physics_space_collide(space, &c, contact_joint_callback);
    athena_physics_world_step(c.world, dt);
    athena_physics_joint_group_empty(c.group);

    if (!c.callback) {
        lua_pushinteger(L, c.count);
        return 1;
    }
    lua_settop(L, c.contacts);
    return 1;
}

static const luaL_Reg world_methods[] = {
    { "setGravity", l_world_set_gravity }, { "getGravity", l_world_get_gravity },
    { "setCFM", l_world_set_cfm }, { "setERP", l_world_set_erp },
    { "step", l_world_step }, { "quickStep", l_world_quick_step },
    { "setQuickStepIterations", l_world_set_qs_iter }, { "stepWithContacts", l_world_step_with_contacts },
    { "destroyWorld", l_world_free }, { "free", l_world_free },
    { NULL, NULL }
};

/* ---------------------------------------------------------------- Space -- */

static int l_space_new(lua_State *L)
{
    AthenaPhysicsSpace *parent = opt_phys(L, 1, SPACE_CLASS);
    push_phys(L, SPACE_CLASS, athena_physics_space_create(parent));
    if (parent) {
        athena_lua_retain(L, -1, 1, "__parent");
        athena_lua_retain(L, 1, -1, NULL);
    }
    return 1;
}

static int l_space_collide(lua_State *L)
{
    contact_ctx c = { L, NULL, NULL, 0, 0, 0 };
    AthenaPhysicsSpace *space = S(1);
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TFUNCTION);
        c.callback = 2;
    }
    lua_newtable(L);
    c.contacts = lua_gettop(L);
    athena_physics_space_collide(space, &c, near_callback);
    lua_settop(L, c.contacts);
    return 1;
}

static int l_space_add(lua_State *L)
{
    athena_physics_space_add(S(1), G(2));
    athena_lua_retain(L, 1, 2, NULL);
    return 0;
}
static int l_space_remove(lua_State *L) { athena_physics_space_remove(S(1), G(2)); return 0; }

/* space:raycast(ox, oy, oz, dx, dy, dz [, maxLen [, backfaceCull]]) -> hit table or nil
 * (not exposed in JS; the engine uses it for shadow grids). */
static int l_space_raycast(lua_State *L)
{
    VECTOR origin = { F(2), F(3), F(4), 1.0f };
    VECTOR dir = { F(5), F(6), F(7), 0.0f };
    AthenaRaycastHit hit __attribute__((aligned(16)));
    memset(&hit, 0, sizeof(hit));

    if (!athena_physics_space_raycast(S(1), origin, dir, athena_lua_optfloat(L, 8, 1000.0f),
                                      athena_lua_tobool(L, 9), &hit) || !hit.has_hit)
        return 0;

    lua_createtable(L, 0, 5);
    athena_lua_push_vec3(L, hit.pos[0], hit.pos[1], hit.pos[2]);
    lua_setfield(L, -2, "position");
    athena_lua_push_vec3(L, hit.normal[0], hit.normal[1], hit.normal[2]);
    lua_setfield(L, -2, "normal");
    athena_lua_set_num(L, "depth", hit.depth);
    athena_lua_set_int(L, "triangle", hit.tri_index);
    push_geom_ref(L, hit.geom);
    lua_setfield(L, -2, "geom");
    return 1;
}

static const luaL_Reg space_methods[] = {
    { "collide", l_space_collide }, { "add", l_space_add }, { "remove", l_space_remove },
    { "raycast", l_space_raycast }, { "free", l_space_free },
    { NULL, NULL }
};

/* ----------------------------------------------------------------- Geom -- */

static AthenaPhysicsSpace *geom_space_arg(lua_State *L)
{
    return opt_phys(L, 1, SPACE_CLASS);
}

static int finish_geom(lua_State *L, AthenaPhysicsGeom *g)
{
    push_phys(L, GEOM_CLASS, g);
    if (!lua_isnoneornil(L, 1)) {
        /* Natively the space owns its geoms: mirror that so a geom kept
         * only by the space is not collected (and removed) by the GC. */
        athena_lua_retain(L, -1, 1, "__space");
        athena_lua_retain(L, 1, -1, NULL);
    }
    return 1;
}

static int l_geom_box(lua_State *L) { return finish_geom(L, athena_physics_geom_create_box(geom_space_arg(L), F(2), F(3), F(4))); }
static int l_geom_sphere(lua_State *L) { return finish_geom(L, athena_physics_geom_create_sphere(geom_space_arg(L), F(2))); }
static int l_geom_plane(lua_State *L) { return finish_geom(L, athena_physics_geom_create_plane(geom_space_arg(L), F(2), F(3), F(4), F(5))); }
static int l_geom_ray(lua_State *L) { return finish_geom(L, athena_physics_geom_create_ray(geom_space_arg(L), F(2))); }

static int l_geom_transform(lua_State *L)
{
    AthenaPhysicsGeom *child = opt_phys(L, 2, GEOM_CLASS);
    finish_geom(L, athena_physics_geom_create_transform(geom_space_arg(L), child));
    if (child)
        athena_lua_retain(L, -1, 2, "__child");
    return 1;
}

static int l_geom_render_object(lua_State *L)
{
    athena_object_data *od = athena_render_object_native(athena_lua_check_render_object(L, 2));
    if (!od || !od->data)
        return luaL_error(L, "RenderObject has no mesh data");
    finish_geom(L, athena_physics_geom_create_from_render_object(geom_space_arg(L), od));
    athena_lua_retain(L, -1, 2, "__render_object");
    return 1;
}

static int l_geom_set_position(lua_State *L) { athena_physics_geom_set_position(G(1), F(2), F(3), F(4)); return 0; }

static int l_geom_get_position(lua_State *L)
{
    float p[3] = { 0, 0, 0 };
    athena_physics_geom_get_position(G(1), &p[0], &p[1], &p[2]);
    return push3(L, p);
}

static int l_geom_set_rotation(lua_State *L)
{
    float R[9];
    read_r9(L, 2, R);
    athena_physics_geom_set_rotation(G(1), R);
    return 0;
}

static int l_geom_get_rotation(lua_State *L)
{
    float R[9] = { 0 };
    athena_physics_geom_get_rotation(G(1), R);
    return push_r9(L, R);
}

static int l_geom_set_body(lua_State *L)
{
    AthenaPhysicsBody *body = opt_phys(L, 2, BODY_CLASS);
    athena_physics_geom_set_body(G(1), body);
    if (body)
        athena_lua_retain(L, 1, 2, "__body");
    return 0;
}

static int l_geom_get_body(lua_State *L)
{
    AthenaPhysicsBody *body = athena_physics_geom_get_body(G(1));
    if (!body || !cache_push(L, body))
        return 0;
    return 1;
}

/* geomCollide(a, b) -- a and b may be geoms or spaces. */
static AthenaPhysicsGeom *geom_or_space(lua_State *L, int idx)
{
    LuaPhysObj *o = athena_lua_test_object(L, idx, GEOM_CLASS);
    if (o && o->ptr)
        return o->ptr;
    o = athena_lua_test_object(L, idx, SPACE_CLASS);
    if (o && o->ptr)
        return &((AthenaPhysicsSpace *)o->ptr)->geom_wrapper;
    return NULL;
}

static int l_geom_collide(lua_State *L)
{
    AthenaPhysicsGeom *g1 = geom_or_space(L, 1), *g2 = geom_or_space(L, 2);
    lua_newtable(L);
    if (!g1 || !g2)
        return 1;
    AthenaContact contacts[MAX_CONTACTS] __attribute__((aligned(16)));
    int n = athena_physics_collide(g1, g2, MAX_CONTACTS, contacts);
    for (int i = 0; i < n; i++) {
        push_contact(L, &contacts[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

/* Ray geoms */
static int l_ray_set_length(lua_State *L) { athena_physics_ray_set_length(G(1), F(2)); return 0; }
static int l_ray_get_length(lua_State *L) { lua_pushnumber(L, athena_physics_ray_get_length(G(1))); return 1; }
static int l_ray_set(lua_State *L) { athena_physics_ray_set(G(1), F(2), F(3), F(4), F(5), F(6), F(7)); return 0; }
static int l_ray_set_params(lua_State *L) { athena_physics_ray_set_params(G(1), athena_lua_tobool(L, 2), athena_lua_tobool(L, 3)); return 0; }
static int l_ray_set_closest(lua_State *L) { athena_physics_ray_set_closest_hit(G(1), athena_lua_tobool(L, 2)); return 0; }
static int l_ray_get_closest(lua_State *L) { lua_pushboolean(L, athena_physics_ray_get_closest_hit(G(1))); return 1; }

static int l_ray_get(lua_State *L)
{
    VECTOR start, dir;
    athena_physics_ray_get(G(1), start, dir);
    lua_createtable(L, 0, 2);
    athena_lua_push_vec3(L, start[0], start[1], start[2]);
    lua_setfield(L, -2, "start");
    athena_lua_push_vec3(L, dir[0], dir[1], dir[2]);
    lua_setfield(L, -2, "direction");
    return 1;
}

static int l_ray_get_params(lua_State *L)
{
    int first = 0, cull = 0;
    athena_physics_ray_get_params(G(1), &first, &cull);
    lua_createtable(L, 0, 2);
    athena_lua_set_bool(L, "firstContact", first);
    athena_lua_set_bool(L, "backfaceCull", cull);
    return 1;
}

static const luaL_Reg geom_methods[] = {
    { "setPosition", l_geom_set_position }, { "getPosition", l_geom_get_position },
    { "setRotation", l_geom_set_rotation }, { "getRotation", l_geom_get_rotation },
    { "setBody", l_geom_set_body }, { "getBody", l_geom_get_body }, { "free", l_geom_free },
    { "raySetLength", l_ray_set_length }, { "rayGetLength", l_ray_get_length },
    { "raySet", l_ray_set }, { "rayGet", l_ray_get },
    { "raySetParams", l_ray_set_params }, { "rayGetParams", l_ray_get_params },
    { "raySetClosestHit", l_ray_set_closest }, { "rayGetClosestHit", l_ray_get_closest },
    { NULL, NULL }
};

/* ----------------------------------------------------------------- Body -- */

static int l_body_new(lua_State *L)
{
    AthenaPhysicsWorld *world = W(1);
    push_phys(L, BODY_CLASS, athena_physics_body_create(world));
    athena_lua_retain(L, -1, 1, "__world");
    athena_lua_retain(L, 1, -1, NULL);
    return 1;
}

static int l_body_set_position(lua_State *L) { athena_physics_body_set_position(B(1), F(2), F(3), F(4)); return 0; }
static int l_body_set_linear_vel(lua_State *L) { athena_physics_body_set_linear_vel(B(1), F(2), F(3), F(4)); return 0; }
static int l_body_set_angular_vel(lua_State *L) { athena_physics_body_set_angular_vel(B(1), F(2), F(3), F(4)); return 0; }
static int l_body_set_mass(lua_State *L) { athena_physics_body_set_mass(B(1), F(2)); return 0; }
static int l_body_set_mass_box(lua_State *L) { athena_physics_body_set_mass_box(B(1), F(2), F(3), F(4), F(5)); return 0; }
static int l_body_set_mass_sphere(lua_State *L) { athena_physics_body_set_mass_sphere(B(1), F(2), F(3)); return 0; }
static int l_body_add_force(lua_State *L) { athena_physics_body_add_force(B(1), F(2), F(3), F(4)); return 0; }
static int l_body_add_torque(lua_State *L) { athena_physics_body_add_torque(B(1), F(2), F(3), F(4)); return 0; }
static int l_body_enable(lua_State *L) { athena_physics_body_enable(B(1)); return 0; }
static int l_body_disable(lua_State *L) { athena_physics_body_disable(B(1)); return 0; }
static int l_body_enabled(lua_State *L) { lua_pushboolean(L, athena_physics_body_is_enabled(B(1))); return 1; }

static int l_body_get_position(lua_State *L)
{
    float v[3];
    athena_physics_body_get_position_vec(B(1), v);
    return push3(L, v);
}

static int l_body_get_linear_vel(lua_State *L)
{
    float v[3];
    athena_physics_body_get_linear_vel_vec(B(1), v);
    return push3(L, v);
}

static int l_body_get_angular_vel(lua_State *L)
{
    float v[3];
    athena_physics_body_get_angular_vel_vec(B(1), v);
    return push3(L, v);
}

static int l_body_set_rotation(lua_State *L)
{
    float R[9];
    read_r9(L, 2, R);
    athena_physics_body_set_rotation(B(1), R);
    return 0;
}

static int l_body_get_rotation(lua_State *L)
{
    float R[9] = { 0 };
    athena_physics_body_get_rotation(B(1), R);
    return push_r9(L, R);
}

static const luaL_Reg body_methods[] = {
    { "setPosition", l_body_set_position }, { "getPosition", l_body_get_position },
    { "setRotation", l_body_set_rotation }, { "getRotation", l_body_get_rotation },
    { "setLinearVel", l_body_set_linear_vel }, { "getLinearVel", l_body_get_linear_vel },
    { "setAngularVel", l_body_set_angular_vel }, { "getAngularVel", l_body_get_angular_vel },
    { "setMass", l_body_set_mass }, { "setMassBox", l_body_set_mass_box }, { "setMassSphere", l_body_set_mass_sphere },
    { "addForce", l_body_add_force }, { "addTorque", l_body_add_torque },
    { "enable", l_body_enable }, { "disable", l_body_disable }, { "enabled", l_body_enabled },
    { "free", l_body_free },
    { NULL, NULL }
};

/* ---------------------------------------------------------- JointGroup -- */

static int l_jgroup_new(lua_State *L) { push_phys(L, JGROUP_CLASS, athena_physics_joint_group_create()); return 1; }
static int l_jgroup_empty(lua_State *L) { athena_physics_joint_group_empty(JG(1)); return 0; }

static const luaL_Reg jgroup_methods[] = {
    { "empty", l_jgroup_empty }, { "free", l_jgroup_free },
    { NULL, NULL }
};

/* ---------------------------------------------------------------- Joint -- */

static int l_joint_new(lua_State *L)
{
    AthenaJointType type = (AthenaJointType)lua_tointeger(L, lua_upvalueindex(1));
    AthenaPhysicsWorld *world = opt_phys(L, 1, WORLD_CLASS);
    AthenaPhysicsJointGroup *group = opt_phys(L, 2, JGROUP_CLASS);
    push_phys(L, JOINT_CLASS, athena_physics_joint_create(world, group, type));
    if (world) {
        athena_lua_retain(L, -1, 1, "__world");
        athena_lua_retain(L, 1, -1, NULL);
    }
    return 1;
}

static int l_joint_attach(lua_State *L)
{
    AthenaPhysicsBody *b1 = opt_phys(L, 2, BODY_CLASS);
    AthenaPhysicsBody *b2 = opt_phys(L, 3, BODY_CLASS);
    athena_physics_joint_attach(J(1), b1, b2);
    if (b1) athena_lua_retain(L, 1, 2, "__body1");
    if (b2) athena_lua_retain(L, 1, 3, "__body2");
    return 0;
}

#define JOINT_SET3(name, fn) static int name(lua_State *L) { fn(J(1), F(2), F(3), F(4)); return 0; }
#define JOINT_SET1(name, fn) static int name(lua_State *L) { fn(J(1), F(2)); return 0; }
#define JOINT_SET2(name, fn) static int name(lua_State *L) { fn(J(1), F(2), F(3)); return 0; }
#define JOINT_GETV(name, fn) static int name(lua_State *L) { VECTOR v = { 0, 0, 0, 0 }; fn(J(1), v); return push3(L, v); }
#define JOINT_GETF(name, fn) static int name(lua_State *L) { lua_pushnumber(L, fn(J(1))); return 1; }

JOINT_SET3(l_j_set_ball_anchor, athena_physics_joint_set_ball_anchor)
JOINT_SET3(l_j_set_hinge_anchor, athena_physics_joint_set_hinge_anchor)
JOINT_SET3(l_j_set_hinge_axis, athena_physics_joint_set_hinge_axis)
JOINT_SET1(l_j_add_hinge_torque, athena_physics_joint_add_hinge_torque)
JOINT_SET3(l_j_set_slider_axis, athena_physics_joint_set_slider_axis)
JOINT_SET1(l_j_add_slider_force, athena_physics_joint_add_slider_force)
JOINT_SET3(l_j_set_hinge2_anchor, athena_physics_joint_set_hinge2_anchor)
JOINT_SET3(l_j_set_hinge2_axis1, athena_physics_joint_set_hinge2_axis1)
JOINT_SET3(l_j_set_hinge2_axis2, athena_physics_joint_set_hinge2_axis2)
JOINT_SET2(l_j_add_hinge2_torques, athena_physics_joint_add_hinge2_torques)
JOINT_SET3(l_j_set_universal_anchor, athena_physics_joint_set_universal_anchor)
JOINT_SET3(l_j_set_universal_axis1, athena_physics_joint_set_universal_axis1)
JOINT_SET3(l_j_set_universal_axis2, athena_physics_joint_set_universal_axis2)
JOINT_SET2(l_j_add_universal_torques, athena_physics_joint_add_universal_torques)

JOINT_GETV(l_j_get_ball_anchor, athena_physics_joint_get_ball_anchor)
JOINT_GETV(l_j_get_hinge_anchor, athena_physics_joint_get_hinge_anchor)
JOINT_GETV(l_j_get_hinge_axis, athena_physics_joint_get_hinge_axis)
JOINT_GETF(l_j_get_hinge_angle, athena_physics_joint_get_hinge_angle)
JOINT_GETF(l_j_get_hinge_angle_rate, athena_physics_joint_get_hinge_angle_rate)
JOINT_GETF(l_j_get_slider_position, athena_physics_joint_get_slider_position)
JOINT_GETF(l_j_get_slider_position_rate, athena_physics_joint_get_slider_position_rate)
JOINT_GETV(l_j_get_slider_axis, athena_physics_joint_get_slider_axis)
JOINT_GETV(l_j_get_hinge2_anchor, athena_physics_joint_get_hinge2_anchor)
JOINT_GETV(l_j_get_hinge2_axis1, athena_physics_joint_get_hinge2_axis1)
JOINT_GETV(l_j_get_hinge2_axis2, athena_physics_joint_get_hinge2_axis2)
JOINT_GETF(l_j_get_hinge2_angle1, athena_physics_joint_get_hinge2_angle1)
JOINT_GETF(l_j_get_hinge2_angle1_rate, athena_physics_joint_get_hinge2_angle1_rate)
JOINT_GETF(l_j_get_hinge2_angle2_rate, athena_physics_joint_get_hinge2_angle2_rate)
JOINT_GETV(l_j_get_universal_anchor, athena_physics_joint_get_universal_anchor)
JOINT_GETV(l_j_get_universal_axis1, athena_physics_joint_get_universal_axis1)
JOINT_GETV(l_j_get_universal_axis2, athena_physics_joint_get_universal_axis2)
JOINT_GETF(l_j_get_universal_angle1, athena_physics_joint_get_universal_angle1)
JOINT_GETF(l_j_get_universal_angle2, athena_physics_joint_get_universal_angle2)
JOINT_GETF(l_j_get_universal_angle1_rate, athena_physics_joint_get_universal_angle1_rate)
JOINT_GETF(l_j_get_universal_angle2_rate, athena_physics_joint_get_universal_angle2_rate)

static int l_j_set_fixed(lua_State *L) { athena_physics_joint_set_fixed(J(1)); return 0; }
static int l_j_set_amotor_num_axes(lua_State *L) { athena_physics_joint_set_amotor_num_axes(J(1), (int)luaL_checkinteger(L, 2)); return 0; }
static int l_j_set_amotor_axis(lua_State *L) { athena_physics_joint_set_amotor_axis(J(1), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3), F(4), F(5), F(6)); return 0; }
static int l_j_set_amotor_angle(lua_State *L) { athena_physics_joint_set_amotor_angle(J(1), (int)luaL_checkinteger(L, 2), F(3)); return 0; }
static int l_j_set_amotor_mode(lua_State *L) { athena_physics_joint_set_amotor_mode(J(1), (int)luaL_checkinteger(L, 2)); return 0; }
static int l_j_add_amotor_torques(lua_State *L) { athena_physics_joint_add_amotor_torques(J(1), F(2), F(3), F(4)); return 0; }
static int l_j_get_amotor_num_axes(lua_State *L) { lua_pushinteger(L, athena_physics_joint_get_amotor_num_axes(J(1))); return 1; }
static int l_j_get_amotor_axis_rel(lua_State *L) { lua_pushinteger(L, athena_physics_joint_get_amotor_axis_rel(J(1), (int)luaL_checkinteger(L, 2))); return 1; }
static int l_j_get_amotor_angle(lua_State *L) { lua_pushnumber(L, athena_physics_joint_get_amotor_angle(J(1), (int)luaL_checkinteger(L, 2))); return 1; }
static int l_j_get_amotor_angle_rate(lua_State *L) { lua_pushnumber(L, athena_physics_joint_get_amotor_angle_rate(J(1), (int)luaL_checkinteger(L, 2))); return 1; }
static int l_j_get_amotor_mode(lua_State *L) { lua_pushinteger(L, athena_physics_joint_get_amotor_mode(J(1))); return 1; }

static int l_j_get_amotor_axis(lua_State *L)
{
    VECTOR v = { 0, 0, 0, 0 };
    athena_physics_joint_get_amotor_axis(J(1), (int)luaL_checkinteger(L, 2), v);
    return push3(L, v);
}

static const luaL_Reg joint_methods[] = {
    { "free", l_joint_free }, { "attach", l_joint_attach },
    { "setBallAnchor", l_j_set_ball_anchor },
    { "setHingeAnchor", l_j_set_hinge_anchor }, { "setHingeAxis", l_j_set_hinge_axis },
    { "addHingeTorque", l_j_add_hinge_torque },
    { "setSliderAxis", l_j_set_slider_axis }, { "addSliderForce", l_j_add_slider_force },
    { "setHinge2Anchor", l_j_set_hinge2_anchor }, { "setHinge2Axis1", l_j_set_hinge2_axis1 },
    { "setHinge2Axis2", l_j_set_hinge2_axis2 }, { "addHinge2Torques", l_j_add_hinge2_torques },
    { "AddHinge2Torques", l_j_add_hinge2_torques },
    { "setUniversalAnchor", l_j_set_universal_anchor }, { "setUniversalAxis1", l_j_set_universal_axis1 },
    { "setUniversalAxis2", l_j_set_universal_axis2 }, { "setUniversalTorques", l_j_add_universal_torques },
    { "setFixed", l_j_set_fixed },
    { "setAMotorNumAxes", l_j_set_amotor_num_axes }, { "setAMotorAxis", l_j_set_amotor_axis },
    { "setAMotorAngle", l_j_set_amotor_angle }, { "setAMotorMode", l_j_set_amotor_mode },
    { "setAMotorTorques", l_j_add_amotor_torques },
    { "getBallAnchor", l_j_get_ball_anchor },
    { "getHingeAnchor", l_j_get_hinge_anchor }, { "getHingeAxis", l_j_get_hinge_axis },
    { "getHingeAngle", l_j_get_hinge_angle }, { "getHingeAngleRate", l_j_get_hinge_angle_rate },
    { "getSliderPosition", l_j_get_slider_position }, { "getSliderPositionRate", l_j_get_slider_position_rate },
    { "getSliderAxis", l_j_get_slider_axis },
    { "getHinge2Anchor", l_j_get_hinge2_anchor }, { "getHinge2Axis1", l_j_get_hinge2_axis1 },
    { "getHinge2Axis2", l_j_get_hinge2_axis2 }, { "getHinge2Angle1", l_j_get_hinge2_angle1 },
    { "getHinge2Angle1Rate", l_j_get_hinge2_angle1_rate }, { "getHinge2Angle2Rate", l_j_get_hinge2_angle2_rate },
    { "getUniversalAnchor", l_j_get_universal_anchor }, { "getUniversalAxis1", l_j_get_universal_axis1 },
    { "getUniversalAxis2", l_j_get_universal_axis2 }, { "getUniversalAngle1", l_j_get_universal_angle1 },
    { "getUniversalAngle2", l_j_get_universal_angle2 },
    { "getUniversalAngle1Rate", l_j_get_universal_angle1_rate }, { "getUniversalAngle2Rate", l_j_get_universal_angle2_rate },
    { "getUniversalAngle1Rate1", l_j_get_universal_angle1_rate }, { "getUniversalAngle2Rate2", l_j_get_universal_angle2_rate },
    { "getAMotorNumAxes", l_j_get_amotor_num_axes }, { "getAMotorAxis", l_j_get_amotor_axis },
    { "getAMotorAxisRel", l_j_get_amotor_axis_rel }, { "getAMotorAngle", l_j_get_amotor_angle },
    { "getAMotorAngleRate", l_j_get_amotor_angle_rate }, { "getAMotorMode", l_j_get_amotor_mode },
    { NULL, NULL }
};

/* --------------------------------------------------------------- module -- */

static int l_physics_cleanup(lua_State *L)
{
    athena_physics_cleanup();
    return 0;
}

static const luaL_Reg physics_funcs[] = {
    { "cleanup", l_physics_cleanup },
    { "GeomRenderObject", l_geom_render_object }, { "GeomBox", l_geom_box }, { "GeomRay", l_geom_ray },
    { "GeomSphere", l_geom_sphere }, { "GeomPlane", l_geom_plane }, { "GeomTransform", l_geom_transform },
    { "World", l_world_new }, { "Space", l_space_new }, { "Body", l_body_new }, { "JointGroup", l_jgroup_new },
    { "geomCollide", l_geom_collide },
    { NULL, NULL }
};

static const struct { const char *name; AthenaJointType type; } joint_ctors[] = {
    { "JointBall", ATH_JOINT_BALL }, { "JointHinge", ATH_JOINT_HINGE }, { "JointSlider", ATH_JOINT_SLIDER },
    { "JointHinge2", ATH_JOINT_HINGE2 }, { "JointUniversal", ATH_JOINT_UNIVERSAL },
    { "JointFixed", ATH_JOINT_FIXED }, { "JointNull", ATH_JOINT_NULL }, { "JointAMotor", ATH_JOINT_AMOTOR },
};

#define PHYS_CLASS(var, cls, method_list, gc_fn) \
    static const athena_lua_class var = { .name = cls, .methods = method_list, .gc = gc_fn };

PHYS_CLASS(world_class, WORLD_CLASS, world_methods, l_world_free)
PHYS_CLASS(space_class, SPACE_CLASS, space_methods, l_space_free)
PHYS_CLASS(geom_class, GEOM_CLASS, geom_methods, l_geom_free)
PHYS_CLASS(body_class, BODY_CLASS, body_methods, l_body_free)
PHYS_CLASS(joint_class, JOINT_CLASS, joint_methods, l_joint_free)
PHYS_CLASS(jgroup_class, JGROUP_CLASS, jgroup_methods, l_jgroup_free)

void athena_lua_open_physics(lua_State *L)
{
    /* weak-valued native pointer -> Lua object cache */
    lua_newtable(L);
    lua_createtable(L, 0, 1);
    lua_pushstring(L, "v");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &cache_key);

    athena_lua_register_class(L, &world_class);
    athena_lua_register_class(L, &space_class);
    athena_lua_register_class(L, &geom_class);
    athena_lua_register_class(L, &body_class);
    athena_lua_register_class(L, &joint_class);
    athena_lua_register_class(L, &jgroup_class);

    athena_lua_new_module(L, physics_funcs, NULL, NULL);
    for (unsigned i = 0; i < countof(joint_ctors); i++) {
        lua_pushinteger(L, joint_ctors[i].type);
        lua_pushcclosure(L, l_joint_new, 1);
        lua_setfield(L, -2, joint_ctors[i].name);
    }
    athena_lua_set_global_module(L, "Physics");
}
