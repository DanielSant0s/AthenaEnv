#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>
#include <ode/ode.h>

#define MAX_CONTACTS 64

JSClassID js_geom_class_id;
static JSClassID js_space_class_id;
static JSClassID js_world_class_id;

JSClassID js_body_class_id;
static JSClassID js_joint_class_id;
static JSClassID js_joint_group_class_id;

static void js_space_finalizer(JSRuntime *rt, JSValue val) {
    JSSpace *space = JS_GetOpaque(val, js_space_class_id);
    if (space && space->space) {
        dSpaceDestroy(space->space);
        free(space);
    }
}

static void js_geom_finalizer(JSRuntime *rt, JSValue val) {
    JSGeom *geom = JS_GetOpaque(val, js_geom_class_id);
    if (geom && geom->geom) {
        dGeomDestroy(geom->geom);
        free(geom);
    }
}

static void js_world_finalizer(JSRuntime *rt, JSValue val) {
    JSWorld *world = JS_GetOpaque(val, js_world_class_id);
    if (world && world->world) {
        dWorldDestroy(world->world);
        free(world);
    }
}

static void js_body_finalizer(JSRuntime *rt, JSValue val) {
    JSBody *body = JS_GetOpaque(val, js_body_class_id);
    if (body && body->body) {
        dBodyDestroy(body->body);
        free(body);
    }
}

static void js_joint_finalizer(JSRuntime *rt, JSValue val) {
    JSJoint *joint = JS_GetOpaque(val, js_joint_class_id);
    if (joint && joint->joint) {
        dJointDestroy(joint->joint);
        free(joint);
    }
}

static void js_joint_group_finalizer(JSRuntime *rt, JSValue val) {
    JSJointGroup *group = JS_GetOpaque(val, js_joint_group_class_id);
    if (group && group->group) {
        dJointGroupDestroy(group->group);
        free(group);
    }
}

static JSClassDef js_space_class = {
    "ODESpace",
    .finalizer = js_space_finalizer,
};

static JSClassDef js_geom_class = {
    "ODEGeom",
    .finalizer = js_geom_finalizer,
};

static JSClassDef js_world_class = {
    "ODEWorld",
    .finalizer = js_world_finalizer,
};

static JSClassDef js_body_class = {
    "ODEBody",
    .finalizer = js_body_finalizer,
};

static JSClassDef js_joint_class = {
    "ODEJoint",
    .finalizer = js_joint_finalizer,
};

static JSClassDef js_joint_group_class = {
    "ODEJointGroup",
    .finalizer = js_joint_group_finalizer,
};

static JSValue js_ode_cleanup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    dCloseODE();
    return JS_UNDEFINED;
}

static JSValue js_world_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = malloc(sizeof(JSWorld));
    if (!world) {
        return JS_EXCEPTION;
    }
    
    world->world = dWorldCreate();
    if (!world->world) {
        free(world);
        return JS_ThrowInternalError(ctx, "Failed to create ODE world");
    }
    
    JSValue obj = JS_NewObjectClass(ctx, js_world_class_id);
    if (JS_IsException(obj)) {
        dWorldDestroy(world->world);
        free(world);
        return obj;
    }
    
    JS_SetOpaque(obj, world);
    return obj;
}

static JSValue js_world_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    
    if (world->world) {
        dWorldDestroy(world->world);
        world->world = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue js_space_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = malloc(sizeof(JSSpace));
    if (!space) {
        return JS_EXCEPTION;
    }

    space->parent = NULL;

    if (argc > 0) {
        JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
        space->parent = space->space;
    }

    space->space = dHashSpaceCreate(space->parent);
    if (!space->space) {
        free(space);
        return JS_ThrowInternalError(ctx, "Failed to create ODE space");
    }
    
    JSValue obj = JS_NewObjectClass(ctx, js_space_class_id);
    if (JS_IsException(obj)) {
        dSpaceDestroy(space->space);
        free(space);
        return obj;
    }
    
    JS_SetOpaque(obj, space);
    return obj;
}

static JSValue js_space_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    
    if (space->space) {
        dSpaceDestroy(space->space);
        space->space = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue js_geom_create_box(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    
    float width, height, depth;
    if (JS_ToFloat32(ctx, &width, argv[1]) ||
        JS_ToFloat32(ctx, &height, argv[2]) ||
        JS_ToFloat32(ctx, &depth, argv[3])) {
        return JS_EXCEPTION;
    }
    
    JSGeom *geom = malloc(sizeof(JSGeom));
    if (!geom) {
        return JS_EXCEPTION;
    }
    
    geom->geom = dCreateBox(space? space->space : NULL, width, height, depth);
    geom->parent_space = space? space->space : NULL;
    
    if (!geom->geom) {
        free(geom);
        return JS_ThrowInternalError(ctx, "Failed to create box geometry");
    }
    
    JSValue obj = JS_NewObjectClass(ctx, js_geom_class_id);
    if (JS_IsException(obj)) {
        dGeomDestroy(geom->geom);
        free(geom);
        return obj;
    }
    
    JS_SetOpaque(obj, geom);
    return obj;
}

static JSValue js_geom_create_sphere(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    
    float radius;
    if (JS_ToFloat32(ctx, &radius, argv[1])) {
        return JS_EXCEPTION;
    }
    
    JSGeom *geom = malloc(sizeof(JSGeom));
    if (!geom) {
        return JS_EXCEPTION;
    }
    
    geom->geom = dCreateSphere(space? space->space : NULL, radius);
    geom->parent_space = space? space->space : NULL;
    
    if (!geom->geom) {
        free(geom);
        return JS_ThrowInternalError(ctx, "Failed to create sphere geometry");
    }
    
    JSValue obj = JS_NewObjectClass(ctx, js_geom_class_id);
    if (JS_IsException(obj)) {
        dGeomDestroy(geom->geom);
        free(geom);
        return obj;
    }
    
    JS_SetOpaque(obj, geom);
    return obj;
}

static JSValue js_geom_create_from_render_object(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    
    JSRenderObject *ro = JS_GetOpaque(argv[1], js_render_object_class_id);

    dTriMeshDataID tm_id = dGeomTriMeshDataCreate();
    
    dGeomTriMeshDataBuildSingle(tm_id, ro->obj.data->positions, sizeof(VECTOR), ro->obj.data->index_count, ro->obj.data->indices, ro->obj.data->index_count, 3*sizeof(uint32_t));

    JSGeom *geom = malloc(sizeof(JSGeom));
    if (!geom) {
        return JS_EXCEPTION;
    }
    
    geom->geom = dCreateTriMesh(space? space->space : NULL, tm_id, NULL, NULL, NULL);
    dGeomSetData(geom->geom, tm_id); 

    geom->parent_space = space? space->space : NULL;
    
    if (!geom->geom) {
        free(geom);
        return JS_ThrowInternalError(ctx, "Failed to create trimesh");
    }
    
    JSValue obj = JS_NewObjectClass(ctx, js_geom_class_id);
    if (JS_IsException(obj)) {
        dGeomDestroy(geom->geom);
        free(geom);
        return obj;
    }
    
    JS_SetOpaque(obj, geom);
    return obj;
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
    
    JSGeom *geom = malloc(sizeof(JSGeom));
    if (!geom) {
        return JS_EXCEPTION;
    }
    
    geom->geom = dCreatePlane(space? space->space : NULL, a, b, c, d);
    geom->parent_space = space? space->space : NULL;
    
    if (!geom->geom) {
        free(geom);
        return JS_ThrowInternalError(ctx, "Failed to create plane geometry");
    }
    
    JSValue obj = JS_NewObjectClass(ctx, js_geom_class_id);
    if (JS_IsException(obj)) {
        dGeomDestroy(geom->geom);
        free(geom);
        return obj;
    }
    
    JS_SetOpaque(obj, geom);
    return obj;
}

static JSValue js_geom_create_transform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    
    JSGeom *geom = malloc(sizeof(JSGeom));
    if (!geom) {
        return JS_EXCEPTION;
    }

    JSGeom *target_geom = JS_GetOpaque(argv[1], js_geom_class_id);
    
    geom->geom = dCreateGeomTransform(space->space);
    dGeomTransformSetGeom(geom->geom, target_geom->geom);
    geom->parent_space = space->space;
    
    if (!geom->geom) {
        free(geom);
        return JS_ThrowInternalError(ctx, "Failed to create transform geometry");
    }
    
    JSValue obj = JS_NewObjectClass(ctx, js_geom_class_id);
    if (JS_IsException(obj)) {
        dGeomDestroy(geom->geom);
        free(geom);
        return obj;
    }
    
    JS_SetOpaque(obj, geom);
    return obj;
}

static JSValue js_geom_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    
    if (geom->geom) {
        dGeomDestroy(geom->geom);
        geom->geom = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue js_geom_set_position(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    
    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }
    
    dGeomSetPosition(geom->geom, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_geom_set_rotation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);

    if (!JS_IsArray(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "Expected array for rotation matrix");
    }
     
    dMatrix3 R;
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
    
    dGeomSetRotation(geom->geom, R);
    return JS_UNDEFINED;
}

static JSValue js_geom_get_position(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);

    const dReal *pos = dGeomGetPosition(geom->geom);
    JSValue arr = JS_NewArray(ctx);
    
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat32(ctx, pos[0]));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat32(ctx, pos[1]));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewFloat32(ctx, pos[2]));
    
    return arr;
}

static JSValue js_geom_get_rotation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    
    const dReal *rot = dGeomGetRotation(geom->geom);
    JSValue arr = JS_NewArray(ctx);
    
    for (int i = 0; i < 9; i++) {
        JS_SetPropertyUint32(ctx, arr, i, JS_NewFloat32(ctx, rot[i]));
    }
    
    return arr;
}

typedef struct {
    JSContext *ctx;
    JSValue callback;
    JSValue contacts_array;
} collision_data;

static void collision_callback(void *data, dGeomID o1, dGeomID o2) {
    collision_data *cdata = (collision_data*)data;
    
    dContact contact[MAX_CONTACTS];
    int n = dCollide(o1, o2, MAX_CONTACTS, &contact[0].geom, sizeof(dContact));
    
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            JSValue contact_obj = JS_NewObject(cdata->ctx);

            JSValue pos_arr = JS_NewArray(cdata->ctx);
            JS_SetPropertyUint32(cdata->ctx, pos_arr, 0, JS_NewFloat32(cdata->ctx, contact[i].geom.pos[0]));
            JS_SetPropertyUint32(cdata->ctx, pos_arr, 1, JS_NewFloat32(cdata->ctx, contact[i].geom.pos[1]));
            JS_SetPropertyUint32(cdata->ctx, pos_arr, 2, JS_NewFloat32(cdata->ctx, contact[i].geom.pos[2]));
            JS_SetPropertyStr(cdata->ctx, contact_obj, "position", pos_arr);

            JSValue normal_arr = JS_NewArray(cdata->ctx);
            JS_SetPropertyUint32(cdata->ctx, normal_arr, 0, JS_NewFloat32(cdata->ctx, contact[i].geom.normal[0]));
            JS_SetPropertyUint32(cdata->ctx, normal_arr, 1, JS_NewFloat32(cdata->ctx, contact[i].geom.normal[1]));
            JS_SetPropertyUint32(cdata->ctx, normal_arr, 2, JS_NewFloat32(cdata->ctx, contact[i].geom.normal[2]));
            JS_SetPropertyStr(cdata->ctx, contact_obj, "normal", normal_arr);

            JS_SetPropertyStr(cdata->ctx, contact_obj, "depth", JS_NewFloat32(cdata->ctx, contact[i].geom.depth));

            JSValue length_val = JS_GetPropertyStr(cdata->ctx, cdata->contacts_array, "length");
            uint32_t length;
            JS_ToUint32(cdata->ctx, &length, length_val);
            JS_SetPropertyUint32(cdata->ctx, cdata->contacts_array, length, contact_obj);
            JS_FreeValue(cdata->ctx, length_val);
        }

        if (!JS_IsUndefined(cdata->callback)) {
            JSValue args[1] = { cdata->contacts_array };
            JS_Call(cdata->ctx, cdata->callback, JS_UNDEFINED, 1, args);
        }
    }
}

static JSValue js_space_collide(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    
    collision_data cdata;
    cdata.ctx = ctx;
    cdata.callback = (argc > 1) ? argv[1] : JS_UNDEFINED;
    cdata.contacts_array = JS_NewArray(ctx);
    
    dSpaceCollide(space->space, &cdata, collision_callback);
    
    return cdata.contacts_array;
}

static JSValue js_geom_collide(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom1 = JS_GetOpaque(argv[0], js_geom_class_id);
    if (!geom1) geom1 = JS_GetOpaque(argv[0], js_space_class_id);
    JSGeom *geom2 = JS_GetOpaque(argv[1], js_geom_class_id);
    if (!geom2) geom2 = JS_GetOpaque(argv[1], js_space_class_id);
    
    dContact contact[MAX_CONTACTS];
    int n = dCollide(geom1->geom, geom2->geom, MAX_CONTACTS, &contact[0].geom, sizeof(dContact));
    
    JSValue contacts_array = JS_NewArray(ctx);
    
    for (int i = 0; i < n; i++) {
        JSValue contact_obj = JS_NewObject(ctx);

        JSValue pos_arr = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, pos_arr, 0, JS_NewFloat32(ctx, contact[i].geom.pos[0]));
        JS_SetPropertyUint32(ctx, pos_arr, 1, JS_NewFloat32(ctx, contact[i].geom.pos[1]));
        JS_SetPropertyUint32(ctx, pos_arr, 2, JS_NewFloat32(ctx, contact[i].geom.pos[2]));
        JS_SetPropertyStr(ctx, contact_obj, "position", pos_arr);

        JSValue normal_arr = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, normal_arr, 0, JS_NewFloat32(ctx, contact[i].geom.normal[0]));
        JS_SetPropertyUint32(ctx, normal_arr, 1, JS_NewFloat32(ctx, contact[i].geom.normal[1]));
        JS_SetPropertyUint32(ctx, normal_arr, 2, JS_NewFloat32(ctx, contact[i].geom.normal[2]));
        JS_SetPropertyStr(ctx, contact_obj, "normal", normal_arr);

        JS_SetPropertyStr(ctx, contact_obj, "depth", JS_NewFloat32(ctx, contact[i].geom.depth));
        
        JS_SetPropertyUint32(ctx, contacts_array, i, contact_obj);
    }
    
    return contacts_array;
}

static void rot_vector_to_matrix(VECTOR vec, dMatrix3 result) {
    float angle = sqrtf(vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2]);

    float nx = vec[0] / angle;
    float ny = vec[1] / angle;
    float nz = vec[2] / angle;

    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);
    float one_minus_cos = 1.0f - cos_angle;

    result[0] = cos_angle + nx*nx*one_minus_cos;          
    result[1] = nx*ny*one_minus_cos - nz*sin_angle;       
    result[2] = nx*nz*one_minus_cos + ny*sin_angle;       
    
    result[4] = ny*nx*one_minus_cos + nz*sin_angle;       
    result[5] = cos_angle + ny*ny*one_minus_cos;          
    result[6] = ny*nz*one_minus_cos - nx*sin_angle;       
    
    result[8] = nz*nx*one_minus_cos - ny*sin_angle;       
    result[9] = nz*ny*one_minus_cos + nx*sin_angle;       
    result[10] = cos_angle + nz*nz*one_minus_cos;         
}

void updateGeomPosRot(athena_object_data *obj) {
    dGeomID geom = (dGeomID)obj->collision;

    dGeomSetPosition(geom, obj->position[0], obj->position[1], obj->position[2]);

    dMatrix3 R;
    rot_vector_to_matrix(obj->rotation, R);
    
    dGeomSetRotation(geom, R);
}

static JSValue js_world_set_gravity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    
    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }
    
    dWorldSetGravity(world->world, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_world_get_gravity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    
    dVector3 gravity;
    dWorldGetGravity(world->world, gravity);
    
    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat32(ctx, gravity[0]));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat32(ctx, gravity[1]));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewFloat32(ctx, gravity[2]));
    
    return arr;
}

static JSValue js_world_set_cfm(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    
    float cfm;
    if (JS_ToFloat32(ctx, &cfm, argv[0])) {
        return JS_EXCEPTION;
    }
    
    dWorldSetCFM(world->world, cfm);
    return JS_UNDEFINED;
}

static JSValue js_world_set_erp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    
    float erp;
    if (JS_ToFloat32(ctx, &erp, argv[0])) {
        return JS_EXCEPTION;
    }
    
    dWorldSetERP(world->world, erp);
    return JS_UNDEFINED;
}

static JSValue js_world_step(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    
    float step_size;
    if (JS_ToFloat32(ctx, &step_size, argv[0])) {
        return JS_EXCEPTION;
    }
    
    dWorldStep(world->world, step_size);
    return JS_UNDEFINED;
}

static JSValue js_world_quick_step(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    
    float step_size;
    if (JS_ToFloat32(ctx, &step_size, argv[0])) {
        return JS_EXCEPTION;
    }
    
    dWorldQuickStep(world->world, step_size);
    return JS_UNDEFINED;
}

static JSValue js_world_set_quick_step_iterations(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    
    uint32_t iterations;
    if (JS_ToUint32(ctx, &iterations, argv[0])) {
        return JS_EXCEPTION;
    }
    
    dWorldSetQuickStepNumIterations(world->world, iterations);
    return JS_UNDEFINED;
}


typedef struct {
    dWorldID world;
    dJointGroupID joint_group;
} physics_world_data;

static void contact_callback(void *data, dGeomID o1, dGeomID o2) {
    physics_world_data *cdata = (physics_world_data*)data;
    
    dContact contact[MAX_CONTACTS];
    int n = dCollide(o1, o2, MAX_CONTACTS, &contact[0].geom, sizeof(dContact));
    
    if (n > 0) {
        dBodyID b1 = dGeomGetBody(o1);
        dBodyID b2 = dGeomGetBody(o2);
        
        if (b1 && b2 && dAreConnectedExcluding(b1, b2, dJointTypeContact)) {
            return;
        }
        
        for (int i = 0; i < n; i++) {
            contact[i].surface.mode = dContactBounce | dContactSoftCFM;
            contact[i].surface.mu = 0.5;
            contact[i].surface.bounce = 0.1;
            contact[i].surface.bounce_vel = 0.1;
            contact[i].surface.soft_cfm = 0.01;
            
            dJointID c = dJointCreateContact(cdata->world, cdata->joint_group, &contact[i]);
            dJointAttach(c, b1, b2);
        }
    }
}

static JSValue js_world_step_with_contacts(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(this_val, js_world_class_id);
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    JSJointGroup *contact_group = JS_GetOpaque(argv[1], js_joint_group_class_id);
    
    float step_size;
    if (JS_ToFloat32(ctx, &step_size, argv[2])) {
        return JS_EXCEPTION;
    }
    
    physics_world_data cdata;
    cdata.world = world->world;
    cdata.joint_group = contact_group->group;
    
    dSpaceCollide(space->space, &cdata, contact_callback);
    dWorldStep(world->world, step_size);
    dJointGroupEmpty(contact_group->group);
    
    return JS_UNDEFINED;
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

static JSValue js_body_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSWorld *world = JS_GetOpaque(argv[0], js_world_class_id);
    
    JSBody *body = malloc(sizeof(JSBody));
    if (!body) {
        return JS_EXCEPTION;
    }
    
    body->body = dBodyCreate(world->world);
    body->parent_world = world->world;
    
    if (!body->body) {
        free(body);
        return JS_ThrowInternalError(ctx, "Failed to create ODE body");
    }
    
    JSValue obj = JS_NewObjectClass(ctx, js_body_class_id);
    if (JS_IsException(obj)) {
        dBodyDestroy(body->body);
        free(body);
        return obj;
    }
    
    JS_SetOpaque(obj, body);
    return obj;
}

static JSValue js_body_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    if (body->body) {
        dBodyDestroy(body->body);
        body->body = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue js_body_set_position(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }
    
    dBodySetPosition(body->body, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_body_get_position(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    const dReal *pos = dBodyGetPosition(body->body);
    JSValue arr = JS_NewArray(ctx);
    
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat32(ctx, pos[0]));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat32(ctx, pos[1]));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewFloat32(ctx, pos[2]));
    
    return arr;
}

static JSValue js_body_set_rotation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    if (!JS_IsArray(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "Expected array for rotation matrix");
    }
    
    dMatrix3 R;
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
    
    dBodySetRotation(body->body, R);
    return JS_UNDEFINED;
}

static JSValue js_body_get_rotation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    const dReal *rot = dBodyGetRotation(body->body);
    JSValue arr = JS_NewArray(ctx);
    
    for (int i = 0; i < 9; i++) {
        JS_SetPropertyUint32(ctx, arr, i, JS_NewFloat32(ctx, rot[i]));
    }
    
    return arr;
}

static JSValue js_body_set_linear_vel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }
    
    dBodySetLinearVel(body->body, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_body_get_linear_vel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    const dReal *vel = dBodyGetLinearVel(body->body);
    JSValue arr = JS_NewArray(ctx);
    
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat32(ctx, vel[0]));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat32(ctx, vel[1]));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewFloat32(ctx, vel[2]));
    
    return arr;
}

static JSValue js_body_set_angular_vel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[0]) ||
        JS_ToFloat32(ctx, &y, argv[1]) ||
        JS_ToFloat32(ctx, &z, argv[2])) {
        return JS_EXCEPTION;
    }
    
    dBodySetAngularVel(body->body, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_body_get_angular_vel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    const dReal *vel = dBodyGetAngularVel(body->body);
    JSValue arr = JS_NewArray(ctx);
    
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat32(ctx, vel[0]));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat32(ctx, vel[1]));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewFloat32(ctx, vel[2]));
    
    return arr;
}

static JSValue js_body_set_mass(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    float mass;
    if (JS_ToFloat32(ctx, &mass, argv[0])) {
        return JS_EXCEPTION;
    }
    
    dMass m;
    dMassSetSphere(&m, 1.0, 1.0);
    dMassAdjust(&m, mass);
    dBodySetMass(body->body, &m);
    return JS_UNDEFINED;
}

static JSValue js_body_set_mass_box(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    float mass, lx, ly, lz;
    if (JS_ToFloat32(ctx, &mass, argv[0]) ||
        JS_ToFloat32(ctx, &lx, argv[1]) ||
        JS_ToFloat32(ctx, &ly, argv[2]) ||
        JS_ToFloat32(ctx, &lz, argv[3])) {
        return JS_EXCEPTION;
    }
    
    dMass m;
    dMassSetBox(&m, 1.0, lx, ly, lz);
    dMassAdjust(&m, mass);
    dBodySetMass(body->body, &m);
    return JS_UNDEFINED;
}

static JSValue js_body_set_mass_sphere(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    float mass, radius;
    if (JS_ToFloat32(ctx, &mass, argv[0]) ||
        JS_ToFloat32(ctx, &radius, argv[1])) {
        return JS_EXCEPTION;
    }
    
    dMass m;
    dMassSetSphere(&m, 1.0, radius);
    dMassAdjust(&m, mass);
    dBodySetMass(body->body, &m);
    return JS_UNDEFINED;
}

static JSValue js_body_add_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    float fx, fy, fz;
    if (JS_ToFloat32(ctx, &fx, argv[0]) ||
        JS_ToFloat32(ctx, &fy, argv[1]) ||
        JS_ToFloat32(ctx, &fz, argv[2])) {
        return JS_EXCEPTION;
    }
    
    dBodyAddForce(body->body, fx, fy, fz);
    return JS_UNDEFINED;
}

static JSValue js_body_add_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    
    float fx, fy, fz;
    if (JS_ToFloat32(ctx, &fx, argv[0]) ||
        JS_ToFloat32(ctx, &fy, argv[1]) ||
        JS_ToFloat32(ctx, &fz, argv[2])) {
        return JS_EXCEPTION;
    }
    
    dBodyAddTorque(body->body, fx, fy, fz);
    return JS_UNDEFINED;
}

static JSValue js_body_enable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    dBodyEnable(body->body);
    return JS_UNDEFINED;
}

static JSValue js_body_disable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    dBodyDisable(body->body);
    return JS_UNDEFINED;
}

static JSValue js_body_is_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSBody *body = JS_GetOpaque(this_val, js_body_class_id);
    return JS_NewBool(ctx, dBodyIsEnabled(body->body));
}

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

static JSValue js_geom_set_body(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    JSBody *body = JS_GetOpaque(argv[0], js_body_class_id);
    
    dGeomSetBody(geom->geom, body->body);
    return JS_UNDEFINED;
}

static JSValue js_geom_get_body(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(this_val, js_geom_class_id);
    
    dBodyID body_id = dGeomGetBody(geom->geom);
    if (!body_id) {
        return JS_NULL;
    }
    
    JSBody *body = malloc(sizeof(JSBody));
    if (!body) {
        return JS_EXCEPTION;
    }
    
    body->body = body_id;
    body->parent_world = NULL;
    
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
    
    group->group = dJointGroupCreate(0);
    
    if (!group->group) {
        free(group);
        return JS_ThrowInternalError(ctx, "Failed to create joint group");
    }
    
    JSValue obj = JS_NewObjectClass(ctx, js_joint_group_class_id);
    if (JS_IsException(obj)) {
        dJointGroupDestroy(group->group);
        free(group);
        return obj;
    }
    
    JS_SetOpaque(obj, group);
    return obj;
}

static JSValue js_joint_group_empty(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJointGroup *group = JS_GetOpaque(argv[0], js_joint_group_class_id);
    dJointGroupEmpty(group->group);
    return JS_UNDEFINED;
}

static JSValue js_joint_group_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSJointGroup *group = JS_GetOpaque(argv[0], js_joint_group_class_id);
    
    if (group->group) {
        dJointGroupDestroy(group->group);
        group->group = NULL;
    }
    return JS_UNDEFINED;
}

void updateBodyPosRot(athena_object_data *obj) {
    dBodyID body = (dBodyID)obj->physics;
    
    const dReal *pos = dBodyGetPosition(body);
    const dReal *rot = dBodyGetRotation(body);
    
    obj->position[0] = pos[0];
    obj->position[1] = pos[1];
    obj->position[2] = pos[2];
    
    dQuaternion q;
    dRtoQ(rot, q);
    
    float angle = 2.0f * acosf(q[0]);
    if (angle > 0.001f) {
        float sin_half = sinf(angle * 0.5f);
        obj->rotation[0] = (q[1] / sin_half) * angle;
        obj->rotation[1] = (q[2] / sin_half) * angle;
        obj->rotation[2] = (q[3] / sin_half) * angle;
    } else {
        obj->rotation[0] = 0;
        obj->rotation[1] = 0;
        obj->rotation[2] = 0;
    }

    update_object_space(obj);
}

static const JSCFunctionListEntry js_geom_proto_funcs[] = {
    JS_CFUNC_DEF("setPosition", 3, js_geom_set_position),
    JS_CFUNC_DEF("setRotation", 1, js_geom_set_rotation),
    JS_CFUNC_DEF("getPosition", 0, js_geom_get_position),
    JS_CFUNC_DEF("getRotation", 0, js_geom_get_rotation),

    JS_CFUNC_DEF("setBody", 1, js_geom_set_body),
    JS_CFUNC_DEF("getBody", 0, js_geom_get_body),

    JS_CFUNC_DEF("free", 0, js_geom_destroy),
};

static const JSCFunctionListEntry js_ode_funcs[] = {
    JS_CFUNC_DEF("cleanup", 0, js_ode_cleanup),
    JS_CFUNC_DEF("World", 0, js_world_create),
    JS_CFUNC_DEF("Space", 0, js_space_create),
    JS_CFUNC_DEF("destroySpace", 1, js_space_destroy),
    JS_CFUNC_DEF("fromRenderObject", 2, js_geom_create_from_render_object),
    JS_CFUNC_DEF("createBox", 4, js_geom_create_box),
    JS_CFUNC_DEF("createSphere", 2, js_geom_create_sphere),
    JS_CFUNC_DEF("createPlane", 5, js_geom_create_plane),
    JS_CFUNC_DEF("createTransform", 2, js_geom_create_transform),
    JS_CFUNC_DEF("spaceCollide", 2, js_space_collide),
    JS_CFUNC_DEF("geomCollide", 2, js_geom_collide),
    
    JS_CFUNC_DEF("Body", 1, js_body_create),
    
    JS_CFUNC_DEF("JointGroup", 0, js_joint_group_create),
    JS_CFUNC_DEF("emptyJointGroup", 1, js_joint_group_empty),
    JS_CFUNC_DEF("destroyJointGroup", 1, js_joint_group_destroy),
};

static int js_ode_init_module(JSContext *ctx, JSModuleDef *m) {
    JSValue proto;

    JS_NewClassID(&js_space_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_space_class_id, &js_space_class);

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

    JS_NewClassID(&js_joint_group_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_joint_group_class_id, &js_joint_group_class);

    JS_SetModuleExportList(ctx, m, js_ode_funcs, countof(js_ode_funcs));
    
    return 0;
}

JSModuleDef *athena_ode_init(JSContext *ctx) {
    return athena_push_module(ctx, js_ode_init_module, js_ode_funcs, countof(js_ode_funcs), "ODE");
}
