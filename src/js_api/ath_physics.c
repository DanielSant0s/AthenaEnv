#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>
#include <ath_bindings.h>
#include <athena/physics_facade.h>
#include <physics.h>

#define MAX_CONTACTS 64

JSClassID js_geom_class_id;
JSClassID js_space_class_id;
static JSClassID js_world_class_id;

JSClassID js_body_class_id;
static JSClassID js_joint_class_id;
static JSClassID js_joint_group_class_id;

static void js_space_finalizer(JSRuntime *rt, JSValue val) {
    JSSpace *space = JS_GetOpaque(val, js_space_class_id);
    if (space) {
        if (space->space) {
            athena_physics_space_destroy(space->space);
            space->space = NULL;
        }
        free(space);
    }
}

static void js_geom_finalizer(JSRuntime *rt, JSValue val) {
    JSGeom *geom = JS_GetOpaque(val, js_geom_class_id);
    if (geom) {
        if (geom->geom) {
            athena_physics_geom_destroy(geom->geom);
            geom->geom = NULL;
        }
        free(geom);
    }
}

static void js_world_finalizer(JSRuntime *rt, JSValue val) {
    JSWorld *world = JS_GetOpaque(val, js_world_class_id);
    if (world) {
        if (world->world) {
            athena_physics_world_destroy(world->world);
            world->world = NULL;
        }
        free(world);
    }
}

static void js_body_finalizer(JSRuntime *rt, JSValue val) {
    JSBody *body = JS_GetOpaque(val, js_body_class_id);
    if (body) {
        if (body->body) {
            athena_physics_body_destroy(body->body);
            body->body = NULL;
        }
        free(body);
    }
}

static void js_joint_finalizer(JSRuntime *rt, JSValue val) {
    JSJoint *joint = JS_GetOpaque(val, js_joint_class_id);
    if (joint) {
        if (joint->joint) {
            athena_physics_joint_destroy(joint->joint);
            joint->joint = NULL;
        }
        free(joint);
    }
}

static void js_joint_group_finalizer(JSRuntime *rt, JSValue val) {
    JSJointGroup *group = JS_GetOpaque(val, js_joint_group_class_id);
    if (group) {
        if (group->group) {
            athena_physics_joint_group_destroy(group->group);
            group->group = NULL;
        }
        free(group);
    }
}

static JSClassDef js_space_class = {
    "PhysicsSpace",
    .finalizer = js_space_finalizer,
};

static JSClassDef js_geom_class = {
    "PhysicsGeom",
    .finalizer = js_geom_finalizer,
};

static JSClassDef js_world_class = {
    "PhysicsWorld",
    .finalizer = js_world_finalizer,
};

static JSClassDef js_body_class = {
    "PhysicsBody",
    .finalizer = js_body_finalizer,
};

static JSClassDef js_joint_class = {
    "PhysicsJoint",
    .finalizer = js_joint_finalizer,
};

static JSClassDef js_joint_group_class = {
    "PhysicsJointGroup",
    .finalizer = js_joint_group_finalizer,
};

static JSValue js_physics_cleanup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    athena_physics_cleanup();
    return JS_UNDEFINED;
}

static JSValue js_world_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *wrapper = malloc(sizeof(JSWorld));
    if (!wrapper)
        return JS_EXCEPTION;

    wrapper->world = athena_physics_world_create();
    if (!wrapper->world) {
        free(wrapper);
        return JS_ThrowInternalError(ctx, "Failed to create physics world");
    }

    JSValue obj = JS_NewObjectClass(ctx, js_world_class_id);
    if (JS_IsException(obj)) {
        athena_physics_world_destroy(wrapper->world);
        free(wrapper);
        return obj;
    }

    JS_SetOpaque(obj, wrapper);
    return obj;
}

static JSValue js_world_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    if (world && world->world) {
        athena_physics_world_destroy(world->world);
        world->world = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue js_space_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *wrapper = malloc(sizeof(JSSpace));
    AthenaPhysicsSpace *parent = NULL;

    if (!wrapper)
        return JS_EXCEPTION;

    if (argc > 0) {
        JSSpace *parent_wrap = JS_GetOpaque(argv[0], js_space_class_id);
        if (parent_wrap)
            parent = parent_wrap->space;
    }

    wrapper->parent = parent;
    wrapper->space = athena_physics_space_create(parent);
    if (!wrapper->space) {
        free(wrapper);
        return JS_ThrowInternalError(ctx, "Failed to create physics space");
    }

    JSValue obj = JS_NewObjectClass(ctx, js_space_class_id);
    if (JS_IsException(obj)) {
        athena_physics_space_destroy(wrapper->space);
        free(wrapper);
        return obj;
    }

    JS_SetOpaque(obj, wrapper);
    return obj;
}

static JSValue js_space_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(this_val, js_space_class_id);

    if (space && space->space) {
        athena_physics_space_destroy(space->space);
        space->space = NULL;
    }
    return JS_UNDEFINED;
}

/* Ray-specific functions */
static JSValue js_geom_ray_set_length(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    if (!geom || !geom->geom) return JS_EXCEPTION;

    float length;
    if (JS_ToFloat32(ctx, &length, argv[0])) {
        return JS_EXCEPTION;
    }

    athena_physics_ray_set_length(geom->geom, length);
    return JS_UNDEFINED;
}

static JSValue js_geom_ray_get_length(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    if (!geom || !geom->geom) return JS_EXCEPTION;

    float length = athena_physics_ray_get_length(geom->geom);
    return JS_NewFloat32(ctx, length);
}

static JSValue js_geom_ray_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    if (!geom || !geom->geom) return JS_EXCEPTION;

    float px, py, pz, dx, dy, dz;
    if (JS_ToFloat32(ctx, &px, argv[0]) ||
        JS_ToFloat32(ctx, &py, argv[1]) ||
        JS_ToFloat32(ctx, &pz, argv[2]) ||
        JS_ToFloat32(ctx, &dx, argv[3]) ||
        JS_ToFloat32(ctx, &dy, argv[4]) ||
        JS_ToFloat32(ctx, &dz, argv[5])) {
        return JS_EXCEPTION;
    }

    athena_physics_ray_set(geom->geom, px, py, pz, dx, dy, dz);
    return JS_UNDEFINED;
}

static JSValue js_geom_ray_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    if (!geom || !geom->geom) return JS_EXCEPTION;

    VECTOR start, dir;
    athena_physics_ray_get(geom->geom, start, dir);

    JSValue result = JS_NewObject(ctx);

    JSValue start_arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, start_arr, 0, JS_NewFloat32(ctx, start[0]));
    JS_SetPropertyUint32(ctx, start_arr, 1, JS_NewFloat32(ctx, start[1]));
    JS_SetPropertyUint32(ctx, start_arr, 2, JS_NewFloat32(ctx, start[2]));
    JS_SetPropertyStr(ctx, result, "start", start_arr);

    JSValue dir_arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, dir_arr, 0, JS_NewFloat32(ctx, dir[0]));
    JS_SetPropertyUint32(ctx, dir_arr, 1, JS_NewFloat32(ctx, dir[1]));
    JS_SetPropertyUint32(ctx, dir_arr, 2, JS_NewFloat32(ctx, dir[2]));
    JS_SetPropertyStr(ctx, result, "direction", dir_arr);

    return result;
}

static JSValue js_geom_ray_set_params(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    if (!geom || !geom->geom) return JS_EXCEPTION;

    int firstContact, backfaceCull;
    if (JS_ToInt32(ctx, &firstContact, argv[0]) ||
        JS_ToInt32(ctx, &backfaceCull, argv[1])) {
        return JS_EXCEPTION;
    }

    athena_physics_ray_set_params(geom->geom, firstContact, backfaceCull);
    return JS_UNDEFINED;
}

static JSValue js_geom_ray_get_params(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    if (!geom || !geom->geom) return JS_EXCEPTION;

    int firstContact = 0, backfaceCull = 0;
    athena_physics_ray_get_params(geom->geom, &firstContact, &backfaceCull);

    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "firstContact", JS_NewBool(ctx, firstContact));
    JS_SetPropertyStr(ctx, result, "backfaceCull", JS_NewBool(ctx, backfaceCull));

    return result;
}

static JSValue js_geom_ray_set_closest_hit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    if (!geom || !geom->geom) return JS_EXCEPTION;

    int closestHit;
    if (JS_ToInt32(ctx, &closestHit, argv[0])) {
        return JS_EXCEPTION;
    }

    athena_physics_ray_set_closest_hit(geom->geom, closestHit);
    return JS_UNDEFINED;
}

static JSValue js_geom_ray_get_closest_hit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    if (!geom || !geom->geom) return JS_EXCEPTION;

    int closestHit = athena_physics_ray_get_closest_hit(geom->geom);
    return JS_NewBool(ctx, closestHit);
}

static JSValue wrap_new_geom(JSContext *ctx, AthenaPhysicsGeom *native_geom, AthenaPhysicsSpace *parent_space) {
    if (!native_geom) {
        return JS_ThrowInternalError(ctx, "Failed to create geometry");
    }

    JSGeom *geom = malloc(sizeof(JSGeom));
    if (!geom) {
        athena_physics_geom_destroy(native_geom);
        return JS_EXCEPTION;
    }

    geom->geom = native_geom;
    geom->parent_space = parent_space;

    JSValue obj = JS_NewObjectClass(ctx, js_geom_class_id);
    if (JS_IsException(obj)) {
        athena_physics_geom_destroy(native_geom);
        free(geom);
        return obj;
    }

    geom->js_obj = obj;
    JS_SetOpaque(obj, geom);
    athena_physics_geom_set_data(native_geom, geom);
    return obj;
}

static JSValue js_geom_create_ray(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);

    float length;
    if (JS_ToFloat32(ctx, &length, argv[1])) {
        return JS_EXCEPTION;
    }

    AthenaPhysicsSpace *sp = space ? space->space : NULL;
    AthenaPhysicsGeom *g = athena_physics_geom_create_ray(sp, length);
    JSValue obj = wrap_new_geom(ctx, g, sp);
    if (JS_IsException(obj)) return obj;

    JS_SetPropertyStr(ctx, obj, "raySetLength", JS_NewCFunction(ctx, js_geom_ray_set_length, "raySetLength", 1));
    JS_SetPropertyStr(ctx, obj, "rayGetLength", JS_NewCFunction(ctx, js_geom_ray_get_length, "rayGetLength", 0));
    JS_SetPropertyStr(ctx, obj, "raySet", JS_NewCFunction(ctx, js_geom_ray_set, "raySet", 6));
    JS_SetPropertyStr(ctx, obj, "rayGet", JS_NewCFunction(ctx, js_geom_ray_get, "rayGet", 0));
    JS_SetPropertyStr(ctx, obj, "raySetParams", JS_NewCFunction(ctx, js_geom_ray_set_params, "raySetParams", 2));
    JS_SetPropertyStr(ctx, obj, "rayGetParams", JS_NewCFunction(ctx, js_geom_ray_get_params, "rayGetParams", 0));
    JS_SetPropertyStr(ctx, obj, "raySetClosestHit", JS_NewCFunction(ctx, js_geom_ray_set_closest_hit, "raySetClosestHit", 1));
    JS_SetPropertyStr(ctx, obj, "rayGetClosestHit", JS_NewCFunction(ctx, js_geom_ray_get_closest_hit, "rayGetClosestHit", 0));

    return obj;
}

static JSValue js_geom_create_box(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);

    float width, height, depth;
    if (JS_ToFloat32(ctx, &width, argv[1]) ||
        JS_ToFloat32(ctx, &height, argv[2]) ||
        JS_ToFloat32(ctx, &depth, argv[3])) {
        return JS_EXCEPTION;
    }

    AthenaPhysicsSpace *sp = space ? space->space : NULL;
    AthenaPhysicsGeom *g = athena_physics_geom_create_box(sp, width, height, depth);
    return wrap_new_geom(ctx, g, sp);
}

static JSValue js_geom_create_sphere(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);

    float radius;
    if (JS_ToFloat32(ctx, &radius, argv[1])) {
        return JS_EXCEPTION;
    }

    AthenaPhysicsSpace *sp = space ? space->space : NULL;
    AthenaPhysicsGeom *g = athena_physics_geom_create_sphere(sp, radius);
    return wrap_new_geom(ctx, g, sp);
}

static JSValue js_geom_create_from_render_object(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    JSRenderObject *ro = JS_GetOpaque(argv[1], js_render_object_class_id);
    athena_object_data *od;

    if (!ro || !ro->native)
        return JS_ThrowTypeError(ctx, "expected RenderObject");

    od = athena_render_object_native(ro->native);
    if (!od || !od->data)
        return JS_ThrowTypeError(ctx, "RenderObject has no mesh data");

    AthenaPhysicsSpace *sp = space ? space->space : NULL;
    AthenaPhysicsGeom *g = athena_physics_geom_create_from_render_object(sp, od);
    return wrap_new_geom(ctx, g, sp);
}

static JSValue js_geom_create_plane(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);

    float a, b, c, d;
    if (JS_ToFloat32(ctx, &a, argv[1]) ||
        JS_ToFloat32(ctx, &b, argv[2]) ||
        JS_ToFloat32(ctx, &c, argv[3]) ||
        JS_ToFloat32(ctx, &d, argv[4])) {
        return JS_EXCEPTION;
    }

    AthenaPhysicsSpace *sp = space ? space->space : NULL;
    AthenaPhysicsGeom *g = athena_physics_geom_create_plane(sp, a, b, c, d);
    return wrap_new_geom(ctx, g, sp);
}

static JSValue js_geom_create_transform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    JSGeom *target_geom = JS_GetOpaque(argv[1], js_geom_class_id);

    AthenaPhysicsSpace *sp = space ? space->space : NULL;
    AthenaPhysicsGeom *child = target_geom ? target_geom->geom : NULL;
    AthenaPhysicsGeom *g = athena_physics_geom_create_transform(sp, child);
    return wrap_new_geom(ctx, g, sp);
}

static JSValue js_geom_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);

    if (geom && geom->geom) {
        athena_physics_geom_destroy(geom->geom);
        geom->geom = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue js_geom_set_position(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    if (!geom || !geom->geom) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_geom_set_position(geom->geom, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_geom_set_rotation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    if (!geom || !geom->geom) return JS_EXCEPTION;

    if (!JS_IsArray(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "Expected array for rotation matrix");
    }

    float R[9];
    for (int i = 0; i < 9; i++) {
        JSValue val = JS_GetPropertyUint32(ctx, argv[0], i);
        float v;
        if (JS_ToFloat32(ctx, &v, val)) {
            JS_FreeValue(ctx, val);
            return JS_EXCEPTION;
        }
        R[i] = v;
        JS_FreeValue(ctx, val);
    }

    athena_physics_geom_set_rotation(geom->geom, R);
    return JS_UNDEFINED;
}

static JSValue js_geom_get_position(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    if (!geom || !geom->geom) return JS_EXCEPTION;

    float pos[3] = {0, 0, 0};
    athena_physics_geom_get_position(geom->geom, &pos[0], &pos[1], &pos[2]);

    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat32(ctx, pos[0]));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat32(ctx, pos[1]));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewFloat32(ctx, pos[2]));

    return arr;
}

static JSValue js_geom_get_rotation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    if (!geom || !geom->geom) return JS_EXCEPTION;

    float rot[9] = {0};
    athena_physics_geom_get_rotation(geom->geom, rot);

    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < 9; i++) {
        JS_SetPropertyUint32(ctx, arr, i, JS_NewFloat32(ctx, rot[i]));
    }

    return arr;
}

static JSValue build_contact_js_object(JSContext *ctx, const AthenaContact *c) {
    JSValue contact_obj = JS_NewObject(ctx);

    JSValue pos_arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, pos_arr, 0, JS_NewFloat32(ctx, c->pos[0]));
    JS_SetPropertyUint32(ctx, pos_arr, 1, JS_NewFloat32(ctx, c->pos[1]));
    JS_SetPropertyUint32(ctx, pos_arr, 2, JS_NewFloat32(ctx, c->pos[2]));
    JS_SetPropertyStr(ctx, contact_obj, "position", pos_arr);

    JSValue normal_arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, normal_arr, 0, JS_NewFloat32(ctx, c->normal[0]));
    JS_SetPropertyUint32(ctx, normal_arr, 1, JS_NewFloat32(ctx, c->normal[1]));
    JS_SetPropertyUint32(ctx, normal_arr, 2, JS_NewFloat32(ctx, c->normal[2]));
    JS_SetPropertyStr(ctx, contact_obj, "normal", normal_arr);

    JS_SetPropertyStr(ctx, contact_obj, "depth", JS_NewFloat32(ctx, c->depth));

    JSGeom *jg1 = c->g1 ? (JSGeom *)athena_physics_geom_get_data(c->g1) : NULL;
    JSGeom *jg2 = c->g2 ? (JSGeom *)athena_physics_geom_get_data(c->g2) : NULL;
    JS_SetPropertyStr(ctx, contact_obj, "geom1", jg1 ? JS_DupValue(ctx, jg1->js_obj) : JS_NULL);
    JS_SetPropertyStr(ctx, contact_obj, "geom2", jg2 ? JS_DupValue(ctx, jg2->js_obj) : JS_NULL);

    return contact_obj;
}

typedef struct {
    JSContext *ctx;
    JSValue callback;
    JSValue contacts_array;
    uint32_t contact_index;
} collision_data;

static void collision_callback(void *data, AthenaPhysicsGeom *o1, AthenaPhysicsGeom *o2) {
    collision_data *cdata = (collision_data*)data;

    AthenaContact contacts[MAX_CONTACTS] __attribute__((aligned(16)));
    int n = athena_physics_collide(o1, o2, MAX_CONTACTS, contacts);

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            JSValue contact_obj = build_contact_js_object(cdata->ctx, &contacts[i]);
            JS_SetPropertyUint32(cdata->ctx, cdata->contacts_array, cdata->contact_index++, JS_DupValue(cdata->ctx, contact_obj));

            if (!JS_IsUndefined(cdata->callback)) {
                JSValue args[1] = { contact_obj };
                JSValue ret = JS_Call(cdata->ctx, cdata->callback, JS_UNDEFINED, 1, args);
                JS_FreeValue(cdata->ctx, ret);
            }
            JS_FreeValue(cdata->ctx, contact_obj);
        }
    }
}

static JSValue js_space_collide(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(this_val, js_space_class_id);
    if (!space || !space->space) return JS_EXCEPTION;

    collision_data cdata;
    cdata.ctx = ctx;
    cdata.callback = (argc > 0) ? argv[0] : JS_UNDEFINED;
    cdata.contacts_array = JS_NewArray(ctx);
    cdata.contact_index = 0;

    athena_physics_space_collide(space->space, &cdata, collision_callback);

    return cdata.contacts_array;
}

static AthenaPhysicsGeom *extract_geom_or_space(JSValue val) {
    JSGeom *g = JS_GetOpaque(val, js_geom_class_id);
    if (g && g->geom) return g->geom;
    JSSpace *s = JS_GetOpaque(val, js_space_class_id);
    if (s && s->space) return &s->space->geom_wrapper;
    return NULL;
}

static JSValue js_geom_collide(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    AthenaPhysicsGeom *g1 = extract_geom_or_space(argv[0]);
    AthenaPhysicsGeom *g2 = extract_geom_or_space(argv[1]);
    if (!g1 || !g2) return JS_NewArray(ctx);

    AthenaContact contacts[MAX_CONTACTS] __attribute__((aligned(16)));
    int n = athena_physics_collide(g1, g2, MAX_CONTACTS, contacts);

    JSValue contacts_array = JS_NewArray(ctx);
    for (int i = 0; i < n; i++) {
        JSValue contact_obj = build_contact_js_object(ctx, &contacts[i]);
        JS_SetPropertyUint32(ctx, contacts_array, i, contact_obj);
    }

    return contacts_array;
}

static JSValue js_world_set_gravity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    if (!world || !world->world) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_world_set_gravity(world->world, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_world_get_gravity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    if (!world || !world->world) return JS_EXCEPTION;

    float gravity[3] = {0, 0, 0};
    athena_physics_world_get_gravity_vec(world->world, gravity);

    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat32(ctx, gravity[0]));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat32(ctx, gravity[1]));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewFloat32(ctx, gravity[2]));

    return arr;
}

static JSValue js_world_set_cfm(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    if (!world || !world->world) return JS_EXCEPTION;

    float cfm;
    if (JS_ToFloat32(ctx, &cfm, argv[0])) {
        return JS_EXCEPTION;
    }

    athena_physics_world_set_cfm(world->world, cfm);
    return JS_UNDEFINED;
}

static JSValue js_world_set_erp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    if (!world || !world->world) return JS_EXCEPTION;

    float erp;
    if (JS_ToFloat32(ctx, &erp, argv[0])) {
        return JS_EXCEPTION;
    }

    athena_physics_world_set_erp(world->world, erp);
    return JS_UNDEFINED;
}

static JSValue js_world_step(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    if (!world || !world->world) return JS_EXCEPTION;

    float step_size;
    if (JS_ToFloat32(ctx, &step_size, argv[0])) {
        return JS_EXCEPTION;
    }

    athena_physics_world_step(world->world, step_size);
    return JS_UNDEFINED;
}

static JSValue js_world_quick_step(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    if (!world || !world->world) return JS_EXCEPTION;

    float step_size;
    if (JS_ToFloat32(ctx, &step_size, argv[0])) {
        return JS_EXCEPTION;
    }

    athena_physics_world_quick_step(world->world, step_size);
    return JS_UNDEFINED;
}

static JSValue js_world_set_quick_step_iterations(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    if (!world || !world->world) return JS_EXCEPTION;

    uint32_t iterations;
    if (JS_ToUint32(ctx, &iterations, argv[0])) {
        return JS_EXCEPTION;
    }

    athena_physics_world_set_quick_step_iterations(world->world, (int)iterations);
    return JS_UNDEFINED;
}

typedef struct {
    AthenaPhysicsWorld *world;
    AthenaPhysicsJointGroup *joint_group;

    JSContext *ctx;
    JSValue callback;
    JSValue contacts_array;
    uint32_t contact_index;
} physics_world_data;

static void contact_callback(void *data, AthenaPhysicsGeom *o1, AthenaPhysicsGeom *o2) {
    physics_world_data *cdata = (physics_world_data*)data;

    AthenaPhysicsBody *b1 = athena_physics_geom_get_body(o1);
    AthenaPhysicsBody *b2 = athena_physics_geom_get_body(o2);

    if (!b1 && !b2) {
        return;
    }

    if (b1 && b2 && athena_physics_bodies_connected_excluding(b1, b2, ATH_JOINT_CONTACT)) {
        return;
    }

    AthenaContact contacts[MAX_CONTACTS] __attribute__((aligned(16)));
    int n = athena_physics_collide(o1, o2, MAX_CONTACTS, contacts);

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            contacts[i].mu = 0.5f;
            contacts[i].bounce = 0.1f;
            contacts[i].soft_cfm = 0.01f;

            JSValue contact_obj = build_contact_js_object(cdata->ctx, &contacts[i]);
            JS_SetPropertyUint32(cdata->ctx, cdata->contacts_array, cdata->contact_index++, JS_DupValue(cdata->ctx, contact_obj));

            if (!JS_IsUndefined(cdata->callback)) {
                JSValue args[1] = { contact_obj };
                JSValue ret = JS_Call(cdata->ctx, cdata->callback, JS_UNDEFINED, 1, args);
                JS_FreeValue(cdata->ctx, ret);
            }
            JS_FreeValue(cdata->ctx, contact_obj);

            AthenaPhysicsJoint *c = athena_physics_joint_create_contact(cdata->world, cdata->joint_group, &contacts[i]);
            athena_physics_joint_attach(c, b1, b2);
        }
    }
}

static JSValue js_world_step_with_contacts(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    JSJointGroup *contact_group = JS_GetOpaque(argv[1], js_joint_group_class_id);
    if (!world || !world->world || !space || !space->space || !contact_group) return JS_EXCEPTION;

    float step_size;
    if (JS_ToFloat32(ctx, &step_size, argv[2])) {
        return JS_EXCEPTION;
    }

    physics_world_data cdata;
    cdata.world = world->world;
    cdata.joint_group = contact_group->group;
    cdata.contacts_array = JS_NewArray(ctx);
    cdata.callback = (argc > 3) ? argv[3] : JS_UNDEFINED;
    cdata.ctx = ctx;
    cdata.contact_index = 0;

    athena_physics_space_collide(space->space, &cdata, contact_callback);
    athena_physics_world_step(world->world, step_size);
    athena_physics_joint_group_empty(contact_group->group);

    return cdata.contacts_array;
}

static JSValue js_body_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(argv[0], js_world_class_id);
    JSBody *wrapper;

    if (!world || !world->world)
        return JS_EXCEPTION;

    wrapper = malloc(sizeof(JSBody));
    if (!wrapper)
        return JS_EXCEPTION;

    wrapper->parent_world = world->world;
    wrapper->body = athena_physics_body_create(world->world);
    if (!wrapper->body) {
        free(wrapper);
        return JS_ThrowInternalError(ctx, "Failed to create physics body");
    }

    JSValue obj = JS_NewObjectClass(ctx, js_body_class_id);
    if (JS_IsException(obj)) {
        athena_physics_body_destroy(wrapper->body);
        free(wrapper);
        return obj;
    }

    JS_SetOpaque(obj, wrapper);
    return obj;
}

static JSValue js_body_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);

    if (body && body->body) {
        athena_physics_body_destroy(body->body);
        body->body = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue js_body_set_position(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (!body || !body->body) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_body_set_position(body->body, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_body_get_position(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (!body || !body->body) return JS_EXCEPTION;

    float pos[3] = {0, 0, 0};
    athena_physics_body_get_position_vec(body->body, pos);
    JSValue arr = JS_NewArray(ctx);

    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat32(ctx, pos[0]));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat32(ctx, pos[1]));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewFloat32(ctx, pos[2]));

    return arr;
}

static JSValue js_body_set_rotation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (!body || !body->body) return JS_EXCEPTION;

    if (!JS_IsArray(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "Expected array for rotation matrix");
    }

    float R[9];
    for (int i = 0; i < 9; i++) {
        JSValue val = JS_GetPropertyUint32(ctx, argv[0], i);
        float v;
        if (JS_ToFloat32(ctx, &v, val)) {
            JS_FreeValue(ctx, val);
            return JS_EXCEPTION;
        }
        R[i] = v;
        JS_FreeValue(ctx, val);
    }

    athena_physics_body_set_rotation(body->body, R);
    return JS_UNDEFINED;
}

static JSValue js_body_get_rotation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (!body || !body->body) return JS_EXCEPTION;

    float rot[9] = {0};
    athena_physics_body_get_rotation(body->body, rot);
    JSValue arr = JS_NewArray(ctx);

    for (int i = 0; i < 9; i++) {
        JS_SetPropertyUint32(ctx, arr, i, JS_NewFloat32(ctx, rot[i]));
    }

    return arr;
}

static JSValue js_body_set_linear_vel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (!body || !body->body) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_body_set_linear_vel(body->body, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_body_get_linear_vel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (!body || !body->body) return JS_EXCEPTION;

    float vel[3] = {0, 0, 0};
    athena_physics_body_get_linear_vel_vec(body->body, vel);
    JSValue arr = JS_NewArray(ctx);

    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat32(ctx, vel[0]));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat32(ctx, vel[1]));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewFloat32(ctx, vel[2]));

    return arr;
}

static JSValue js_body_set_angular_vel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (!body || !body->body) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_body_set_angular_vel(body->body, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_body_get_angular_vel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (!body || !body->body) return JS_EXCEPTION;

    float vel[3] = {0, 0, 0};
    athena_physics_body_get_angular_vel_vec(body->body, vel);
    JSValue arr = JS_NewArray(ctx);

    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat32(ctx, vel[0]));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat32(ctx, vel[1]));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewFloat32(ctx, vel[2]));

    return arr;
}

static JSValue js_body_set_mass(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (!body || !body->body) return JS_EXCEPTION;

    float mass;
    if (JS_ToFloat32(ctx, &mass, argv[0])) {
        return JS_EXCEPTION;
    }

    athena_physics_body_set_mass(body->body, mass);
    return JS_UNDEFINED;
}

static JSValue js_body_set_mass_box(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (!body || !body->body) return JS_EXCEPTION;

    float mass, lx, ly, lz;
    if (JS_ToFloat32(ctx, &mass, argv[0]) ||
        JS_ToFloat32(ctx, &lx, argv[1]) ||
        JS_ToFloat32(ctx, &ly, argv[2]) ||
        JS_ToFloat32(ctx, &lz, argv[3])) {
        return JS_EXCEPTION;
    }

    athena_physics_body_set_mass_box(body->body, mass, lx, ly, lz);
    return JS_UNDEFINED;
}

static JSValue js_body_set_mass_sphere(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (!body || !body->body) return JS_EXCEPTION;

    float mass, radius;
    if (JS_ToFloat32(ctx, &mass, argv[0]) ||
        JS_ToFloat32(ctx, &radius, argv[1])) {
        return JS_EXCEPTION;
    }

    athena_physics_body_set_mass_sphere(body->body, mass, radius);
    return JS_UNDEFINED;
}

static JSValue js_body_add_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (!body || !body->body) return JS_EXCEPTION;

    float fx, fy, fz;
    if (JS_ToFloat32(ctx, &fx, argv[0]) ||
        JS_ToFloat32(ctx, &fy, argv[1]) ||
        JS_ToFloat32(ctx, &fz, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_body_add_force(body->body, fx, fy, fz);
    return JS_UNDEFINED;
}

static JSValue js_body_add_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (!body || !body->body) return JS_EXCEPTION;

    float fx, fy, fz;
    if (JS_ToFloat32(ctx, &fx, argv[0]) ||
        JS_ToFloat32(ctx, &fy, argv[1]) ||
        JS_ToFloat32(ctx, &fz, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_body_add_torque(body->body, fx, fy, fz);
    return JS_UNDEFINED;
}

static JSValue js_body_enable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (body && body->body) athena_physics_body_enable(body->body);
    return JS_UNDEFINED;
}

static JSValue js_body_disable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    if (body && body->body) athena_physics_body_disable(body->body);
    return JS_UNDEFINED;
}

static JSValue js_body_is_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    return JS_NewBool(ctx, body && body->body ? athena_physics_body_is_enabled(body->body) : false);
}

static JSValue js_geom_set_body(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    JSBody *body = JS_GetOpaque(argv[0], js_body_class_id);
    if (!geom || !geom->geom) return JS_EXCEPTION;

    athena_physics_geom_set_body(geom->geom, body ? body->body : NULL);
    return JS_UNDEFINED;
}

static JSValue js_geom_get_body(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    if (!geom || !geom->geom) return JS_NULL;

    AthenaPhysicsBody *body_id = athena_physics_geom_get_body(geom->geom);
    if (!body_id) {
        return JS_NULL;
    }

    JSBody *body = malloc(sizeof(JSBody));
    if (!body) {
        return JS_EXCEPTION;
    }

    body->body = body_id;
    body->parent_world = body_id->world;

    JSValue obj = JS_NewObjectClass(ctx, js_body_class_id);
    if (JS_IsException(obj)) {
        free(body);
        return obj;
    }

    JS_SetOpaque(obj, body);
    return obj;
}

static JSValue js_joint_group_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJointGroup *group = malloc(sizeof(JSJointGroup));
    if (!group) {
        return JS_EXCEPTION;
    }

    group->group = athena_physics_joint_group_create();
    if (!group->group) {
        free(group);
        return JS_ThrowInternalError(ctx, "Failed to create joint group");
    }

    JSValue obj = JS_NewObjectClass(ctx, js_joint_group_class_id);
    if (JS_IsException(obj)) {
        athena_physics_joint_group_destroy(group->group);
        free(group);
        return obj;
    }

    JS_SetOpaque(obj, group);
    return obj;
}

static JSValue js_joint_group_empty(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJointGroup *group = JS_GetOpaque(this_val, js_joint_group_class_id);
    if (group && group->group) athena_physics_joint_group_empty(group->group);
    return JS_UNDEFINED;
}

static JSValue js_joint_group_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJointGroup *group = JS_GetOpaque(this_val, js_joint_group_class_id);

    if (group && group->group) {
        athena_physics_joint_group_destroy(group->group);
        group->group = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue js_joint_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    JSJoint *joint = malloc(sizeof(JSJoint));
    if (!joint) {
        return JS_EXCEPTION;
    }

    JSWorld *world = JS_GetOpaque(argv[0], js_world_class_id);
    JSJointGroup *group = (argc > 1 && !JS_IsNull(argv[1]) && !JS_IsUndefined(argv[1]))
                        ? JS_GetOpaque(argv[1], js_joint_group_class_id)
                        : NULL;

    AthenaPhysicsWorld *w = world ? world->world : NULL;
    AthenaPhysicsJointGroup *jg = group ? group->group : NULL;

    joint->joint = athena_physics_joint_create(w, jg, (AthenaJointType)magic);
    joint->parent_world = w;

    if (!joint->joint) {
        free(joint);
        return JS_ThrowInternalError(ctx, "Failed to create joint");
    }

    JSValue obj = JS_NewObjectClass(ctx, js_joint_class_id);
    if (JS_IsException(obj)) {
        athena_physics_joint_destroy(joint->joint);
        free(joint);
        return obj;
    }

    JS_SetOpaque(obj, joint);
    return obj;
}

static JSValue js_joint_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);

    if (joint && joint->joint) {
        athena_physics_joint_destroy(joint->joint);
        joint->joint = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue js_joint_attach(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    JSBody *body1 = (argc > 0 && !JS_IsNull(argv[0])) ? JS_GetOpaque(argv[0], js_body_class_id) : NULL;
    JSBody *body2 = (argc > 1 && !JS_IsNull(argv[1])) ? JS_GetOpaque(argv[1], js_body_class_id) : NULL;

    athena_physics_joint_attach(joint->joint,
                                body1 ? body1->body : NULL,
                                body2 ? body2->body : NULL);

    return JS_UNDEFINED;
}

static JSValue js_joint_set_ball_anchor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_ball_anchor(joint->joint, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_hinge_anchor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_hinge_anchor(joint->joint, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_hinge_axis(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_hinge_axis(joint->joint, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_joint_add_hinge_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float x;
    if (JS_ToFloat32(ctx, &x, argv[0])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_add_hinge_torque(joint->joint, x);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_slider_axis(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_slider_axis(joint->joint, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_joint_add_slider_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float x;
    if (JS_ToFloat32(ctx, &x, argv[0])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_add_slider_force(joint->joint, x);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_hinge2_anchor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_hinge2_anchor(joint->joint, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_hinge2_axis1(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_hinge2_axis1(joint->joint, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_hinge2_axis2(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_hinge2_axis2(joint->joint, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_joint_add_hinge2_torques(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float t1, t2;
    if (JS_ToFloat32(ctx, &t1, argv[0]) || JS_ToFloat32(ctx, &t2, argv[1])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_add_hinge2_torques(joint->joint, t1, t2);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_universal_anchor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_universal_anchor(joint->joint, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_universal_axis1(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_universal_axis1(joint->joint, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_universal_axis2(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_universal_axis2(joint->joint, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_joint_add_universal_torques(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float t1, t2;
    if (JS_ToFloat32(ctx, &t1, argv[0]) || JS_ToFloat32(ctx, &t2, argv[1])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_add_universal_torques(joint->joint, t1, t2);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_fixed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (joint && joint->joint) athena_physics_joint_set_fixed(joint->joint);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_amotor_num_axes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    int num;
    if (JS_ToInt32(ctx, &num, argv[0])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_amotor_num_axes(joint->joint, num);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_amotor_axis(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    int anum, rel;
    float x, y, z;
    if (JS_ToInt32(ctx, &anum, argv[0]) ||
        JS_ToInt32(ctx, &rel, argv[1]) ||
        JS_ToFloat32(ctx, &x, argv[2]) ||
        JS_ToFloat32(ctx, &y, argv[3]) ||
        JS_ToFloat32(ctx, &z, argv[4])
    ) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_amotor_axis(joint->joint, anum, rel, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_amotor_angle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    int anum;
    float a;
    if (JS_ToInt32(ctx, &anum, argv[0]) ||
        JS_ToFloat32(ctx, &a, argv[1])
    ) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_amotor_angle(joint->joint, anum, a);
    return JS_UNDEFINED;
}

static JSValue js_joint_set_amotor_mode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    int mode;
    if (JS_ToInt32(ctx, &mode, argv[0])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_set_amotor_mode(joint->joint, mode);
    return JS_UNDEFINED;
}

static JSValue js_joint_add_amotor_torques(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    if (!joint || !joint->joint) return JS_EXCEPTION;

    float t1, t2, t3;
    if (JS_ToFloat32(ctx, &t1, argv[0]) || JS_ToFloat32(ctx, &t2, argv[1]) || JS_ToFloat32(ctx, &t3, argv[2])) {
        return JS_EXCEPTION;
    }

    athena_physics_joint_add_amotor_torques(joint->joint, t1, t2, t3);
    return JS_UNDEFINED;
}

static JSValue vec3_to_js_array(JSContext *ctx, const VECTOR v) {
    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat32(ctx, v[0]));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat32(ctx, v[1]));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewFloat32(ctx, v[2]));
    return arr;
}

static JSValue js_joint_get_ball_anchor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    VECTOR v = {0, 0, 0, 0};
    if (joint && joint->joint) athena_physics_joint_get_ball_anchor(joint->joint, v);
    return vec3_to_js_array(ctx, v);
}

static JSValue js_joint_get_hinge_anchor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    VECTOR v = {0, 0, 0, 0};
    if (joint && joint->joint) athena_physics_joint_get_hinge_anchor(joint->joint, v);
    return vec3_to_js_array(ctx, v);
}

static JSValue js_joint_get_hinge_axis(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    VECTOR v = {0, 0, 0, 0};
    if (joint && joint->joint) athena_physics_joint_get_hinge_axis(joint->joint, v);
    return vec3_to_js_array(ctx, v);
}

static JSValue js_joint_get_hinge_angle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    return JS_NewFloat32(ctx, (joint && joint->joint) ? athena_physics_joint_get_hinge_angle(joint->joint) : 0.0f);
}

static JSValue js_joint_get_hinge_angle_rate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    return JS_NewFloat32(ctx, (joint && joint->joint) ? athena_physics_joint_get_hinge_angle_rate(joint->joint) : 0.0f);
}

static JSValue js_joint_get_slider_position(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    return JS_NewFloat32(ctx, (joint && joint->joint) ? athena_physics_joint_get_slider_position(joint->joint) : 0.0f);
}

static JSValue js_joint_get_slider_position_rate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    return JS_NewFloat32(ctx, (joint && joint->joint) ? athena_physics_joint_get_slider_position_rate(joint->joint) : 0.0f);
}

static JSValue js_joint_get_slider_axis(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    VECTOR v = {0, 0, 0, 0};
    if (joint && joint->joint) athena_physics_joint_get_slider_axis(joint->joint, v);
    return vec3_to_js_array(ctx, v);
}

static JSValue js_joint_get_hinge2_anchor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    VECTOR v = {0, 0, 0, 0};
    if (joint && joint->joint) athena_physics_joint_get_hinge2_anchor(joint->joint, v);
    return vec3_to_js_array(ctx, v);
}

static JSValue js_joint_get_hinge2_axis1(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    VECTOR v = {0, 0, 0, 0};
    if (joint && joint->joint) athena_physics_joint_get_hinge2_axis1(joint->joint, v);
    return vec3_to_js_array(ctx, v);
}

static JSValue js_joint_get_hinge2_axis2(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    VECTOR v = {0, 0, 0, 0};
    if (joint && joint->joint) athena_physics_joint_get_hinge2_axis2(joint->joint, v);
    return vec3_to_js_array(ctx, v);
}

static JSValue js_joint_get_hinge2_angle1(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    return JS_NewFloat32(ctx, (joint && joint->joint) ? athena_physics_joint_get_hinge2_angle1(joint->joint) : 0.0f);
}

static JSValue js_joint_get_hinge2_angle1_rate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    return JS_NewFloat32(ctx, (joint && joint->joint) ? athena_physics_joint_get_hinge2_angle1_rate(joint->joint) : 0.0f);
}

static JSValue js_joint_get_hinge2_angle2_rate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    return JS_NewFloat32(ctx, (joint && joint->joint) ? athena_physics_joint_get_hinge2_angle2_rate(joint->joint) : 0.0f);
}

static JSValue js_joint_get_universal_anchor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    VECTOR v = {0, 0, 0, 0};
    if (joint && joint->joint) athena_physics_joint_get_universal_anchor(joint->joint, v);
    return vec3_to_js_array(ctx, v);
}

static JSValue js_joint_get_universal_axis1(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    VECTOR v = {0, 0, 0, 0};
    if (joint && joint->joint) athena_physics_joint_get_universal_axis1(joint->joint, v);
    return vec3_to_js_array(ctx, v);
}

static JSValue js_joint_get_universal_axis2(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    VECTOR v = {0, 0, 0, 0};
    if (joint && joint->joint) athena_physics_joint_get_universal_axis2(joint->joint, v);
    return vec3_to_js_array(ctx, v);
}

static JSValue js_joint_get_universal_angle1(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    return JS_NewFloat32(ctx, (joint && joint->joint) ? athena_physics_joint_get_universal_angle1(joint->joint) : 0.0f);
}

static JSValue js_joint_get_universal_angle2(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    return JS_NewFloat32(ctx, (joint && joint->joint) ? athena_physics_joint_get_universal_angle2(joint->joint) : 0.0f);
}

static JSValue js_joint_get_universal_angle1_rate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    return JS_NewFloat32(ctx, (joint && joint->joint) ? athena_physics_joint_get_universal_angle1_rate(joint->joint) : 0.0f);
}

static JSValue js_joint_get_universal_angle2_rate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    return JS_NewFloat32(ctx, (joint && joint->joint) ? athena_physics_joint_get_universal_angle2_rate(joint->joint) : 0.0f);
}

static JSValue js_joint_get_amotor_num_axes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    return JS_NewInt32(ctx, (joint && joint->joint) ? athena_physics_joint_get_amotor_num_axes(joint->joint) : 0);
}

static JSValue js_joint_get_amotor_axis(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    int anum = 0;
    JS_ToInt32(ctx, &anum, argv[0]);
    VECTOR res = {0, 0, 0, 0};
    if (joint && joint->joint) athena_physics_joint_get_amotor_axis(joint->joint, anum, res);
    return vec3_to_js_array(ctx, res);
}

static JSValue js_joint_get_amotor_axis_rel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    int anum = 0;
    JS_ToInt32(ctx, &anum, argv[0]);
    return JS_NewInt32(ctx, (joint && joint->joint) ? athena_physics_joint_get_amotor_axis_rel(joint->joint, anum) : 0);
}

static JSValue js_joint_get_amotor_angle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    int anum = 0;
    JS_ToInt32(ctx, &anum, argv[0]);
    return JS_NewFloat32(ctx, (joint && joint->joint) ? athena_physics_joint_get_amotor_angle(joint->joint, anum) : 0.0f);
}

static JSValue js_joint_get_amotor_angle_rate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    int anum = 0;
    JS_ToInt32(ctx, &anum, argv[0]);
    return JS_NewFloat32(ctx, (joint && joint->joint) ? athena_physics_joint_get_amotor_angle_rate(joint->joint, anum) : 0.0f);
}

static JSValue js_joint_get_amotor_mode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJoint *joint = JS_GetOpaque(this_val, js_joint_class_id);
    return JS_NewInt32(ctx, (joint && joint->joint) ? athena_physics_joint_get_amotor_mode(joint->joint) : 0);
}

static const JSCFunctionListEntry js_world_proto_funcs[] = {
    JS_CFUNC_DEF("setGravity", 3, js_world_set_gravity),
    JS_CFUNC_DEF("getGravity", 0, js_world_get_gravity),
    JS_CFUNC_DEF("setCFM", 1, js_world_set_cfm),
    JS_CFUNC_DEF("setERP", 1, js_world_set_erp),
    JS_CFUNC_DEF("step", 1, js_world_step),
    JS_CFUNC_DEF("quickStep", 1, js_world_quick_step),
    JS_CFUNC_DEF("setQuickStepIterations", 1, js_world_set_quick_step_iterations),
    JS_CFUNC_DEF("stepWithContacts", 3, js_world_step_with_contacts),

    JS_CFUNC_DEF("destroyWorld", 0, js_world_destroy),
};

static const JSCFunctionListEntry js_body_proto_funcs[] = {
    JS_CFUNC_DEF("setPosition", 3, js_body_set_position),
    JS_CFUNC_DEF("getPosition", 0, js_body_get_position),
    JS_CFUNC_DEF("setRotation", 1, js_body_set_rotation),
    JS_CFUNC_DEF("getRotation", 0, js_body_get_rotation),
    JS_CFUNC_DEF("setLinearVel", 3, js_body_set_linear_vel),
    JS_CFUNC_DEF("getLinearVel", 0, js_body_get_linear_vel),
    JS_CFUNC_DEF("setAngularVel", 3, js_body_set_angular_vel),
    JS_CFUNC_DEF("getAngularVel", 0, js_body_get_angular_vel),
    JS_CFUNC_DEF("setMass", 1, js_body_set_mass),
    JS_CFUNC_DEF("setMassBox", 4, js_body_set_mass_box),
    JS_CFUNC_DEF("setMassSphere", 2, js_body_set_mass_sphere),
    JS_CFUNC_DEF("addForce", 3, js_body_add_force),
    JS_CFUNC_DEF("addTorque", 3, js_body_add_torque),
    JS_CFUNC_DEF("enable", 0, js_body_enable),
    JS_CFUNC_DEF("disable", 0, js_body_disable),
    JS_CFUNC_DEF("enabled", 0, js_body_is_enabled),

    JS_CFUNC_DEF("free", 0, js_body_destroy),
};

static const JSCFunctionListEntry js_geom_proto_funcs[] = {
    JS_CFUNC_DEF("setPosition", 3, js_geom_set_position),
    JS_CFUNC_DEF("setRotation", 1, js_geom_set_rotation),
    JS_CFUNC_DEF("getPosition", 0, js_geom_get_position),
    JS_CFUNC_DEF("getRotation", 0, js_geom_get_rotation),

    JS_CFUNC_DEF("setBody", 1, js_geom_set_body),
    JS_CFUNC_DEF("getBody", 0, js_geom_get_body),

    JS_CFUNC_DEF("free", 0, js_geom_destroy),
};

static const JSCFunctionListEntry js_space_proto_funcs[] = {
    JS_CFUNC_DEF("collide", 1, js_space_collide),
    JS_CFUNC_DEF("free", 0, js_space_destroy),
};

static const JSCFunctionListEntry js_joint_group_proto_funcs[] = {
    JS_CFUNC_DEF("empty", 0, js_joint_group_empty),
    JS_CFUNC_DEF("free", 0, js_joint_group_destroy),
};

static const JSCFunctionListEntry js_joint_proto_funcs[] = {
    JS_CFUNC_DEF("free", 0, js_joint_destroy),
    JS_CFUNC_DEF("attach", 2, js_joint_attach),

    // SET
    JS_CFUNC_DEF("setBallAnchor", 3, js_joint_set_ball_anchor),

    JS_CFUNC_DEF("setHingeAnchor", 3, js_joint_set_hinge_anchor),
    JS_CFUNC_DEF("setHingeAxis", 3, js_joint_set_hinge_axis),
    JS_CFUNC_DEF("addHingeTorque", 1, js_joint_add_hinge_torque),

    JS_CFUNC_DEF("setSliderAxis", 3, js_joint_set_slider_axis),
    JS_CFUNC_DEF("addSliderForce", 1, js_joint_add_slider_force),

    JS_CFUNC_DEF("setHinge2Anchor", 3, js_joint_set_hinge2_anchor),
    JS_CFUNC_DEF("setHinge2Axis1", 3, js_joint_set_hinge2_axis1),
    JS_CFUNC_DEF("setHinge2Axis2", 3, js_joint_set_hinge2_axis2),
    JS_CFUNC_DEF("AddHinge2Torques", 2, js_joint_add_hinge2_torques),

    JS_CFUNC_DEF("setUniversalAnchor", 3, js_joint_set_universal_anchor),
    JS_CFUNC_DEF("setUniversalAxis1", 3, js_joint_set_universal_axis1),
    JS_CFUNC_DEF("setUniversalAxis2", 3, js_joint_set_universal_axis2),
    JS_CFUNC_DEF("setUniversalTorques", 2, js_joint_add_universal_torques),

    JS_CFUNC_DEF("setFixed", 0, js_joint_set_fixed),

    JS_CFUNC_DEF("setAMotorNumAxes", 1, js_joint_set_amotor_num_axes),
    JS_CFUNC_DEF("setAMotorAxis", 5, js_joint_set_amotor_axis),
    JS_CFUNC_DEF("setAMotorAngle", 2, js_joint_set_amotor_angle),
    JS_CFUNC_DEF("setAMotorMode", 1, js_joint_set_amotor_mode),
    JS_CFUNC_DEF("setAMotorTorques", 3, js_joint_add_amotor_torques),

    // GET
    JS_CFUNC_DEF("getBallAnchor", 0, js_joint_get_ball_anchor),

    JS_CFUNC_DEF("getHingeAnchor", 0, js_joint_get_hinge_anchor),
    JS_CFUNC_DEF("getHingeAxis", 0, js_joint_get_hinge_axis),
    JS_CFUNC_DEF("getHingeAngle", 0, js_joint_get_hinge_angle),
    JS_CFUNC_DEF("getHingeAngleRate", 0, js_joint_get_hinge_angle_rate),

    JS_CFUNC_DEF("getSliderPosition", 0, js_joint_get_slider_position),
    JS_CFUNC_DEF("getSliderPositionRate", 0, js_joint_get_slider_position_rate),
    JS_CFUNC_DEF("getSliderAxis", 0, js_joint_get_slider_axis),

    JS_CFUNC_DEF("getHinge2Anchor", 0, js_joint_get_hinge2_anchor),
    JS_CFUNC_DEF("getHinge2Axis1", 0, js_joint_get_hinge2_axis1),
    JS_CFUNC_DEF("getHinge2Axis2", 0, js_joint_get_hinge2_axis2),
    JS_CFUNC_DEF("getHinge2Angle1", 0, js_joint_get_hinge2_angle1),
    JS_CFUNC_DEF("getHinge2Angle1Rate", 0, js_joint_get_hinge2_angle1_rate),
    JS_CFUNC_DEF("getHinge2Angle2Rate", 0, js_joint_get_hinge2_angle2_rate),

    JS_CFUNC_DEF("getUniversalAnchor", 0, js_joint_get_universal_anchor),
    JS_CFUNC_DEF("getUniversalAxis1", 0, js_joint_get_universal_axis1),
    JS_CFUNC_DEF("getUniversalAxis2", 0, js_joint_get_universal_axis2),
    JS_CFUNC_DEF("getUniversalAngle1", 0, js_joint_get_universal_angle1),
    JS_CFUNC_DEF("getUniversalAngle2", 0, js_joint_get_universal_angle2),
    JS_CFUNC_DEF("getUniversalAngle1Rate1", 0, js_joint_get_universal_angle1_rate),
    JS_CFUNC_DEF("getUniversalAngle2Rate2", 0, js_joint_get_universal_angle2_rate),

    JS_CFUNC_DEF("getAMotorNumAxes", 0, js_joint_get_amotor_num_axes),
    JS_CFUNC_DEF("getAMotorAxis", 1, js_joint_get_amotor_axis),
    JS_CFUNC_DEF("getAMotorAxisRel", 1, js_joint_get_amotor_axis_rel),
    JS_CFUNC_DEF("getAMotorAngle", 1, js_joint_get_amotor_angle),
    JS_CFUNC_DEF("getAMotorAngleRate", 1, js_joint_get_amotor_angle_rate),
    JS_CFUNC_DEF("getAMotorMode", 0, js_joint_get_amotor_mode),
};

static const JSCFunctionListEntry js_physics_funcs[] = {
    JS_CFUNC_DEF("cleanup", 0, js_physics_cleanup),

    JS_CFUNC_DEF("GeomRenderObject", 2, js_geom_create_from_render_object),
    JS_CFUNC_DEF("GeomBox", 4, js_geom_create_box),
    JS_CFUNC_DEF("GeomRay", 2, js_geom_create_ray),
    JS_CFUNC_DEF("GeomSphere", 2, js_geom_create_sphere),
    JS_CFUNC_DEF("GeomPlane", 5, js_geom_create_plane),
    JS_CFUNC_DEF("GeomTransform", 2, js_geom_create_transform),

    JS_CFUNC_MAGIC_DEF("JointBall", 2, js_joint_create, ATH_JOINT_BALL),
    JS_CFUNC_MAGIC_DEF("JointHinge", 2, js_joint_create, ATH_JOINT_HINGE),
    JS_CFUNC_MAGIC_DEF("JointSlider", 2, js_joint_create, ATH_JOINT_SLIDER),
    JS_CFUNC_MAGIC_DEF("JointHinge2", 2, js_joint_create, ATH_JOINT_HINGE2),
    JS_CFUNC_MAGIC_DEF("JointUniversal", 2, js_joint_create, ATH_JOINT_UNIVERSAL),
    JS_CFUNC_MAGIC_DEF("JointFixed", 2, js_joint_create, ATH_JOINT_FIXED),
    JS_CFUNC_MAGIC_DEF("JointNull", 2, js_joint_create, ATH_JOINT_NULL),
    JS_CFUNC_MAGIC_DEF("JointAMotor", 2, js_joint_create, ATH_JOINT_AMOTOR),

    JS_CFUNC_DEF("World", 0, js_world_create),
    JS_CFUNC_DEF("Space", 0, js_space_create),
    JS_CFUNC_DEF("Body", 1, js_body_create),
    JS_CFUNC_DEF("JointGroup", 0, js_joint_group_create),

    JS_CFUNC_DEF("geomCollide", 2, js_geom_collide),
};

static int js_physics_init_module(JSContext *ctx, JSModuleDef *m) {
    JSValue proto;

    JS_NewClassID(&js_space_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_space_class_id, &js_space_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_space_proto_funcs, countof(js_space_proto_funcs));
    JS_SetClassProto(ctx, js_space_class_id, proto);

    JS_NewClassID(&js_geom_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_geom_class_id, &js_geom_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_geom_proto_funcs, countof(js_geom_proto_funcs));
    JS_SetClassProto(ctx, js_geom_class_id, proto);

    JS_NewClassID(&js_world_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_world_class_id, &js_world_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_world_proto_funcs, countof(js_world_proto_funcs));
    JS_SetClassProto(ctx, js_world_class_id, proto);

    JS_NewClassID(&js_body_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_body_class_id, &js_body_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_body_proto_funcs, countof(js_body_proto_funcs));
    JS_SetClassProto(ctx, js_body_class_id, proto);

    JS_NewClassID(&js_joint_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_joint_class_id, &js_joint_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_joint_proto_funcs, countof(js_joint_proto_funcs));
    JS_SetClassProto(ctx, js_joint_class_id, proto);

    JS_NewClassID(&js_joint_group_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_joint_group_class_id, &js_joint_group_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_joint_group_proto_funcs, countof(js_joint_group_proto_funcs));
    JS_SetClassProto(ctx, js_joint_group_class_id, proto);

    JS_SetModuleExportList(ctx, m, js_physics_funcs, countof(js_physics_funcs));

    return 0;
}

JSModuleDef *athena_physics_init(JSContext *ctx) {
    return athena_push_module(ctx, js_physics_init_module, js_physics_funcs, countof(js_physics_funcs), "Physics");
}
