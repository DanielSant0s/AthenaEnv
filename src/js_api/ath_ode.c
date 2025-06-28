#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>
#include <ode/ode.h>

typedef struct {
    dSpaceID space;
} JSSpace;

typedef struct {
    dGeomID geom;
    dSpaceID parent_space;
} JSGeom;

typedef struct {
    dWorldID world;
} JSWorld;

#define MAX_CONTACTS 64

static void js_space_finalizer(JSRuntime *rt, JSValue val) {
    JSSpace *space = JS_GetOpaque(val, 0);
    if (space && space->space) {
        dSpaceDestroy(space->space);
        free(space);
    }
}

static void js_geom_finalizer(JSRuntime *rt, JSValue val) {
    JSGeom *geom = JS_GetOpaque(val, 0);
    if (geom && geom->geom) {
        dGeomDestroy(geom->geom);
        free(geom);
    }
}

static void js_world_finalizer(JSRuntime *rt, JSValue val) {
    JSWorld *world = JS_GetOpaque(val, 0);
    if (world && world->world) {
        dWorldDestroy(world->world);
        free(world);
    }
}

static JSClassID js_space_class_id;
static JSClassID js_geom_class_id;
static JSClassID js_world_class_id;

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
    JSWorld *world = JS_GetOpaque(argv[0], js_world_class_id);
    if (!world) {
        return JS_ThrowTypeError(ctx, "Expected ODEWorld object");
    }
    
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

    space->space = dHashSpaceCreate(0);
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
    if (!space) {
        return JS_ThrowTypeError(ctx, "Expected ODESpace object");
    }
    
    if (space->space) {
        dSpaceDestroy(space->space);
        space->space = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue js_geom_create_box(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 4) {
        return JS_ThrowSyntaxError(ctx, "Expected 4 arguments: space, width, height, depth");
    }
    
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    if (!space) {
        return JS_ThrowTypeError(ctx, "Expected ODESpace object");
    }
    
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
    
    geom->geom = dCreateBox(space->space, width, height, depth);
    geom->parent_space = space->space;
    
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
    if (argc < 2) {
        return JS_ThrowSyntaxError(ctx, "Expected 2 arguments: space, radius");
    }
    
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    if (!space) {
        return JS_ThrowTypeError(ctx, "Expected ODESpace object");
    }
    
    float radius;
    if (JS_ToFloat32(ctx, &radius, argv[1])) {
        return JS_EXCEPTION;
    }
    
    JSGeom *geom = malloc(sizeof(JSGeom));
    if (!geom) {
        return JS_EXCEPTION;
    }
    
    geom->geom = dCreateSphere(space->space, radius);
    geom->parent_space = space->space;
    
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
    
    geom->geom = dCreateTriMesh(space->space, tm_id, NULL, NULL, NULL);
    dGeomSetData(geom->geom, tm_id); 

    geom->parent_space = space->space;
    
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

static JSValue js_geom_create_plane(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 5) {
        return JS_ThrowSyntaxError(ctx, "Expected 5 arguments: space, a, b, c, d");
    }
    
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    if (!space) {
        return JS_ThrowTypeError(ctx, "Expected ODESpace object");
    }
    
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
    
    geom->geom = dCreatePlane(space->space, a, b, c, d);
    geom->parent_space = space->space;
    
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

static JSValue js_geom_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(argv[0], js_geom_class_id);
    if (!geom) {
        return JS_ThrowTypeError(ctx, "Expected ODEGeom object");
    }
    
    if (geom->geom) {
        dGeomDestroy(geom->geom);
        geom->geom = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue js_geom_set_position(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 4) {
        return JS_ThrowSyntaxError(ctx, "Expected 4 arguments: geom, x, y, z");
    }
    
    JSGeom *geom = JS_GetOpaque(argv[0], js_geom_class_id);
    if (!geom || !geom->geom) {
        return JS_ThrowTypeError(ctx, "Expected valid ODEGeom object");
    }
    
    float x, y, z;
    if (JS_ToFloat32(ctx, &x, argv[1]) ||
        JS_ToFloat32(ctx, &y, argv[2]) ||
        JS_ToFloat32(ctx, &z, argv[3])) {
        return JS_EXCEPTION;
    }
    
    dGeomSetPosition(geom->geom, x, y, z);
    return JS_UNDEFINED;
}

static JSValue js_geom_set_rotation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2) {
        return JS_ThrowSyntaxError(ctx, "Expected 2 arguments: geom, rotation_matrix");
    }
    
    JSGeom *geom = JS_GetOpaque(argv[0], js_geom_class_id);
    if (!geom || !geom->geom) {
        return JS_ThrowTypeError(ctx, "Expected valid ODEGeom object");
    }

    if (!JS_IsArray(ctx, argv[1])) {
        return JS_ThrowTypeError(ctx, "Expected array for rotation matrix");
    }
    
    dMatrix3 R;
    for (int i = 0; i < 9; i++) {
        JSValue val = JS_GetPropertyUint32(ctx, argv[1], i);
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
    JSGeom *geom = JS_GetOpaque(argv[0], js_geom_class_id);
    if (!geom || !geom->geom) {
        return JS_ThrowTypeError(ctx, "Expected valid ODEGeom object");
    }
    
    const dReal *pos = dGeomGetPosition(geom->geom);
    JSValue arr = JS_NewArray(ctx);
    
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat32(ctx, pos[0]));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat32(ctx, pos[1]));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewFloat32(ctx, pos[2]));
    
    return arr;
}

static JSValue js_geom_get_rotation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSGeom *geom = JS_GetOpaque(argv[0], js_geom_class_id);
    if (!geom || !geom->geom) {
        return JS_ThrowTypeError(ctx, "Expected valid ODEGeom object");
    }
    
    const dReal *rot = dGeomGetRotation(geom->geom);
    JSValue arr = JS_NewArray(ctx);
    
    for (int i = 0; i < 9; i++) {
        JS_SetPropertyUint32(ctx, arr, i, JS_NewFloat32(ctx, rot[i]));
    }
    
    return arr;
}

// Callback para colisões
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
    if (argc < 1) {
        return JS_ThrowSyntaxError(ctx, "Expected at least 1 argument: space");
    }
    
    JSSpace *space = JS_GetOpaque(argv[0], js_space_class_id);
    if (!space || !space->space) {
        return JS_ThrowTypeError(ctx, "Expected valid ODESpace object");
    }
    
    collision_data cdata;
    cdata.ctx = ctx;
    cdata.callback = (argc > 1) ? argv[1] : JS_UNDEFINED;
    cdata.contacts_array = JS_NewArray(ctx);
    
    dSpaceCollide(space->space, &cdata, collision_callback);
    
    return cdata.contacts_array;
}

static JSValue js_geom_collide(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2) {
        return JS_ThrowSyntaxError(ctx, "Expected 2 arguments: geom1, geom2");
    }
    
    JSGeom *geom1 = JS_GetOpaque(argv[0], js_geom_class_id);
    JSGeom *geom2 = JS_GetOpaque(argv[1], js_geom_class_id);
    
    if (!geom1 || !geom1->geom || !geom2 || !geom2->geom) {
        return JS_ThrowTypeError(ctx, "Expected valid ODEGeom objects");
    }
    
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

static const JSCFunctionListEntry js_ode_funcs[] = {
    JS_CFUNC_DEF("cleanup", 0, js_ode_cleanup),
    JS_CFUNC_DEF("createWorld", 0, js_world_create),
    JS_CFUNC_DEF("destroyWorld", 1, js_world_destroy),
    JS_CFUNC_DEF("createSpace", 0, js_space_create),
    JS_CFUNC_DEF("destroySpace", 1, js_space_destroy),
    JS_CFUNC_DEF("fromRenderObject", 2, js_geom_create_from_render_object),
    JS_CFUNC_DEF("createBox", 4, js_geom_create_box),
    JS_CFUNC_DEF("createSphere", 2, js_geom_create_sphere),
    JS_CFUNC_DEF("createPlane", 5, js_geom_create_plane),
    JS_CFUNC_DEF("destroyGeom", 1, js_geom_destroy),
    JS_CFUNC_DEF("setPosition", 4, js_geom_set_position),
    JS_CFUNC_DEF("setRotation", 2, js_geom_set_rotation),
    JS_CFUNC_DEF("getPosition", 1, js_geom_get_position),
    JS_CFUNC_DEF("getRotation", 1, js_geom_get_rotation),
    JS_CFUNC_DEF("spaceCollide", 2, js_space_collide),
    JS_CFUNC_DEF("geomCollide", 2, js_geom_collide),
};

static int js_ode_init_module(JSContext *ctx, JSModuleDef *m) {
    JS_NewClassID(&js_space_class_id);
    JS_NewClassID(&js_geom_class_id);
    JS_NewClassID(&js_world_class_id);
    
    JS_NewClass(JS_GetRuntime(ctx), js_space_class_id, &js_space_class);
    JS_NewClass(JS_GetRuntime(ctx), js_geom_class_id, &js_geom_class);
    JS_NewClass(JS_GetRuntime(ctx), js_world_class_id, &js_world_class);

    JS_SetModuleExportList(ctx, m, js_ode_funcs, countof(js_ode_funcs));
    
    return 0;
}

JSModuleDef *athena_ode_init(JSContext *ctx) {
    return athena_push_module(ctx, js_ode_init_module, js_ode_funcs, countof(js_ode_funcs), "ODE");
}
