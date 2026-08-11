#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>

#ifdef ATHENA_BOX2D
#include <box2d/box2d.h>
#include "ath_box2d.h"

static void js_b2shape_finalizer(JSRuntime *rt, JSValue val) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque(val, js_b2shape_class_id);
    if (s) {
        if (s->world && b2Shape_IsValid(s->shapeId)) {
            js_b2_wrapper_cache_remove(s->world, JSB2_WRAPPER_SHAPE, b2StoreShapeId(s->shapeId));
        }
        free(s);
    }
}

static JSClassDef js_b2shape_class = {
    "B2Shape",
    .finalizer = js_b2shape_finalizer,
};

static const char *js_b2_shape_type_name(b2ShapeType type) {
    switch (type) {
        case b2_circleShape: return "circle";
        case b2_capsuleShape: return "capsule";
        case b2_segmentShape: return "segment";
        case b2_polygonShape: return "polygon";
        case b2_chainSegmentShape: return "chainSegment";
        default: return "unknown";
    }
}

static JSValue js_b2shape_is_valid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque(this_val, js_b2shape_class_id);
    if (!s) return JS_FALSE;
    return JS_NewBool(ctx, b2Shape_IsValid(s->shapeId));
}

static JSValue js_b2shape_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_FALSE;

    bool updateBodyMass = true;
    if (argc >= 1) updateBodyMass = JS_ToBool(ctx, argv[0]);

    void *userData = b2Shape_GetUserData(s->shapeId);
    if (userData) js_b2_free_userdata_box(userData);

    uint64_t shapeCacheId = b2StoreShapeId(s->shapeId);
    b2DestroyShape(s->shapeId, updateBodyMass);
    s->shapeId = (b2ShapeId){0};
    js_b2_wrapper_cache_remove(s->world, JSB2_WRAPPER_SHAPE, shapeCacheId);
    return JS_TRUE;
}

static JSValue js_b2shape_get_type(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    return JS_NewString(ctx, js_b2_shape_type_name(b2Shape_GetType(s->shapeId)));
}

static JSValue js_b2shape_get_body(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    return js_b2_wrap_body(ctx, s->world, b2Shape_GetBody(s->shapeId));
}

static JSValue js_b2shape_get_friction(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    return JS_NewFloat64(ctx, b2Shape_GetFriction(s->shapeId));
}

static JSValue js_b2shape_set_friction(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "friction required");
    
    double f;
    if (!js_b2_to_float64_strict(ctx, argv[0], &f)) return JS_EXCEPTION;
    if (!js_b2_validate_friction(ctx, f)) return JS_EXCEPTION;
    b2Shape_SetFriction(s->shapeId, (float)f);
    return JS_UNDEFINED;
}

static JSValue js_b2shape_get_restitution(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    return JS_NewFloat64(ctx, b2Shape_GetRestitution(s->shapeId));
}

static JSValue js_b2shape_set_restitution(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "restitution required");
    double r;
    if (!js_b2_to_float64_strict(ctx, argv[0], &r)) return JS_EXCEPTION;
    if (!js_b2_validate_restitution(ctx, r)) return JS_EXCEPTION;
    b2Shape_SetRestitution(s->shapeId, (float)r);
    return JS_UNDEFINED;
}

static JSValue js_b2shape_get_density(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    return JS_NewFloat64(ctx, b2Shape_GetDensity(s->shapeId));
}

static JSValue js_b2shape_set_density(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "density required");
    
    double d;
    if (!js_b2_to_float64_strict(ctx, argv[0], &d)) return JS_EXCEPTION;
    if (!js_b2_validate_density(ctx, d)) return JS_EXCEPTION;
    
    bool updateBodyMass = true;
    if (argc >= 2) {
        if (!js_b2_to_bool_strict(ctx, argv[1], &updateBodyMass)) return JS_EXCEPTION;
    }
    b2Shape_SetDensity(s->shapeId, (float)d, updateBodyMass);
    return JS_UNDEFINED;
}

static JSValue js_b2shape_is_sensor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    return JS_NewBool(ctx, b2Shape_IsSensor(s->shapeId));
}

static JSValue js_b2shape_enable_sensor_events(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "flag required");
    
    bool flag;
    if (!js_b2_to_bool_strict(ctx, argv[0], &flag)) return JS_EXCEPTION;
    b2Shape_EnableSensorEvents(s->shapeId, flag);
    return JS_UNDEFINED;
}

static JSValue js_b2shape_are_sensor_events_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    return JS_NewBool(ctx, b2Shape_AreSensorEventsEnabled(s->shapeId));
}

static JSValue js_b2shape_enable_contact_events(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "flag required");
    
    bool flag;
    if (!js_b2_to_bool_strict(ctx, argv[0], &flag)) return JS_EXCEPTION;
    b2Shape_EnableContactEvents(s->shapeId, flag);
    return JS_UNDEFINED;
}

static JSValue js_b2shape_are_contact_events_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    return JS_NewBool(ctx, b2Shape_AreContactEventsEnabled(s->shapeId));
}

static JSValue js_b2shape_enable_hit_events(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "flag required");
    
    bool flag;
    if (!js_b2_to_bool_strict(ctx, argv[0], &flag)) return JS_EXCEPTION;
    b2Shape_EnableHitEvents(s->shapeId, flag);
    return JS_UNDEFINED;
}

static JSValue js_b2shape_are_hit_events_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    return JS_NewBool(ctx, b2Shape_AreHitEventsEnabled(s->shapeId));
}

static JSValue js_b2shape_get_filter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");

    b2Filter filter = b2Shape_GetFilter(s->shapeId);
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "categoryBits", JS_NewInt64(ctx, (int64_t)filter.categoryBits));
    JS_SetPropertyStr(ctx, obj, "maskBits", JS_NewInt64(ctx, (int64_t)filter.maskBits));
    JS_SetPropertyStr(ctx, obj, "groupIndex", JS_NewInt32(ctx, filter.groupIndex));
    return obj;
}

static JSValue js_b2shape_set_filter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (argc < 1 || !JS_IsObject(argv[0])) return JS_ThrowSyntaxError(ctx, "filter object required");

    b2Filter filter = b2Shape_GetFilter(s->shapeId);
    JSValue cat = JS_GetPropertyStr(ctx, argv[0], "categoryBits");
    JSValue mask = JS_GetPropertyStr(ctx, argv[0], "maskBits");
    JSValue grp = JS_GetPropertyStr(ctx, argv[0], "groupIndex");
    if (!JS_IsUndefined(cat)) {
        int64_t c;
        if (!js_b2_to_int64_strict(ctx, cat, &c)) {
            JS_FreeValue(ctx, cat);
            JS_FreeValue(ctx, mask);
            JS_FreeValue(ctx, grp);
            return JS_EXCEPTION;
        }
        filter.categoryBits = (uint64_t)c;
    }
    if (!JS_IsUndefined(mask)) {
        int64_t m;
        JS_ToInt64(ctx, &m, mask);
        filter.maskBits = (uint64_t)m;
    }
    if (!JS_IsUndefined(grp)) {
        int32_t g;
        JS_ToInt32(ctx, &g, grp);
        filter.groupIndex = g;
    }
    JS_FreeValue(ctx, cat);
    JS_FreeValue(ctx, mask);
    JS_FreeValue(ctx, grp);
    b2Shape_SetFilter(s->shapeId, filter);
    return JS_UNDEFINED;
}

static JSValue js_b2shape_get_user_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    return js_b2_get_userdata_box(ctx, b2Shape_GetUserData(s->shapeId));
}

static JSValue js_b2shape_set_user_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "value required");

    void *old = b2Shape_GetUserData(s->shapeId);
    if (old) js_b2_free_userdata_box(old);
    b2Shape_SetUserData(s->shapeId, js_b2_box_userdata(ctx, s->world, argv[0]));
    return JS_UNDEFINED;
}

static JSValue js_b2shape_test_point(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "x and y required");

    double x, y;
    if (!js_b2_to_float64_strict(ctx, argv[0], &x)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &y)) return JS_EXCEPTION;
    return JS_NewBool(ctx, b2Shape_TestPoint(s->shapeId, (b2Vec2){(float)x, (float)y}));
}

static JSValue js_b2shape_get_aabb(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");

    b2AABB aabb = b2Shape_GetAABB(s->shapeId);
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "lowerX", JS_NewFloat64(ctx, aabb.lowerBound.x));
    JS_SetPropertyStr(ctx, obj, "lowerY", JS_NewFloat64(ctx, aabb.lowerBound.y));
    JS_SetPropertyStr(ctx, obj, "upperX", JS_NewFloat64(ctx, aabb.upperBound.x));
    JS_SetPropertyStr(ctx, obj, "upperY", JS_NewFloat64(ctx, aabb.upperBound.y));
    return obj;
}

static JSValue js_b2shape_get_circle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (b2Shape_GetType(s->shapeId) != b2_circleShape) return JS_ThrowTypeError(ctx, "Shape is not a circle");

    b2Circle circle = b2Shape_GetCircle(s->shapeId);
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "center", js_b2_from_vec2(ctx, circle.center));
    JS_SetPropertyStr(ctx, obj, "radius", JS_NewFloat64(ctx, circle.radius));
    return obj;
}

static JSValue js_b2shape_get_capsule(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (b2Shape_GetType(s->shapeId) != b2_capsuleShape) return JS_ThrowTypeError(ctx, "Shape is not a capsule");

    b2Capsule capsule = b2Shape_GetCapsule(s->shapeId);
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "center1", js_b2_from_vec2(ctx, capsule.center1));
    JS_SetPropertyStr(ctx, obj, "center2", js_b2_from_vec2(ctx, capsule.center2));
    JS_SetPropertyStr(ctx, obj, "radius", JS_NewFloat64(ctx, capsule.radius));
    return obj;
}

static JSValue js_b2shape_get_polygon(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (b2Shape_GetType(s->shapeId) != b2_polygonShape) return JS_ThrowTypeError(ctx, "Shape is not a polygon");

    b2Polygon polygon = b2Shape_GetPolygon(s->shapeId);

    JSValue vertices = JS_NewArray(ctx);
    JSValue normals = JS_NewArray(ctx);
    for (int i = 0; i < polygon.count; i++) {
        JS_SetPropertyUint32(ctx, vertices, i, js_b2_from_vec2(ctx, polygon.vertices[i]));
        JS_SetPropertyUint32(ctx, normals, i, js_b2_from_vec2(ctx, polygon.normals[i]));
    }

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "vertices", vertices);
    JS_SetPropertyStr(ctx, obj, "normals", normals);
    JS_SetPropertyStr(ctx, obj, "centroid", js_b2_from_vec2(ctx, polygon.centroid));
    JS_SetPropertyStr(ctx, obj, "radius", JS_NewFloat64(ctx, polygon.radius));
    JS_SetPropertyStr(ctx, obj, "count", JS_NewInt32(ctx, polygon.count));
    return obj;
}

static JSValue js_b2shape_get_segment(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (b2Shape_GetType(s->shapeId) != b2_segmentShape) return JS_ThrowTypeError(ctx, "Shape is not a segment");

    b2Segment segment = b2Shape_GetSegment(s->shapeId);
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "point1", js_b2_from_vec2(ctx, segment.point1));
    JS_SetPropertyStr(ctx, obj, "point2", js_b2_from_vec2(ctx, segment.point2));
    return obj;
}

static JSValue js_b2shape_get_chain_segment(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (b2Shape_GetType(s->shapeId) != b2_chainSegmentShape) return JS_ThrowTypeError(ctx, "Shape is not a chain segment");

    b2ChainSegment chainSegment = b2Shape_GetChainSegment(s->shapeId);

    JSValue segmentObj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, segmentObj, "point1", js_b2_from_vec2(ctx, chainSegment.segment.point1));
    JS_SetPropertyStr(ctx, segmentObj, "point2", js_b2_from_vec2(ctx, chainSegment.segment.point2));

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "ghost1", js_b2_from_vec2(ctx, chainSegment.ghost1));
    JS_SetPropertyStr(ctx, obj, "segment", segmentObj);
    JS_SetPropertyStr(ctx, obj, "ghost2", js_b2_from_vec2(ctx, chainSegment.ghost2));
    JS_SetPropertyStr(ctx, obj, "chain", js_b2_wrap_chain(ctx, s->world, b2Shape_GetParentChain(s->shapeId)));
    return obj;
}

static JSValue js_b2shape_get_closest_point(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Shape *s = (JSB2Shape *)JS_GetOpaque2(ctx, this_val, js_b2shape_class_id);
    if (!s || !b2Shape_IsValid(s->shapeId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Shape");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "x and y required");

    double x, y;
    if (!js_b2_to_float64_strict(ctx, argv[0], &x)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &y)) return JS_EXCEPTION;

    b2Vec2 closest = b2Shape_GetClosestPoint(s->shapeId, (b2Vec2){(float)x, (float)y});
    return js_b2_from_vec2(ctx, closest);
}

static const JSCFunctionListEntry js_b2shape_proto_funcs[] = {
    JS_CFUNC_DEF("isValid", 0, js_b2shape_is_valid),
    JS_CFUNC_DEF("destroy", 0, js_b2shape_destroy),
    JS_CFUNC_DEF("getType", 0, js_b2shape_get_type),
    JS_CFUNC_DEF("getBody", 0, js_b2shape_get_body),
    JS_CFUNC_DEF("getFriction", 0, js_b2shape_get_friction),
    JS_CFUNC_DEF("setFriction", 1, js_b2shape_set_friction),
    JS_CFUNC_DEF("getRestitution", 0, js_b2shape_get_restitution),
    JS_CFUNC_DEF("setRestitution", 1, js_b2shape_set_restitution),
    JS_CFUNC_DEF("getDensity", 0, js_b2shape_get_density),
    JS_CFUNC_DEF("setDensity", 1, js_b2shape_set_density),
    JS_CFUNC_DEF("isSensor", 0, js_b2shape_is_sensor),
    JS_CFUNC_DEF("enableSensorEvents", 1, js_b2shape_enable_sensor_events),
    JS_CFUNC_DEF("areSensorEventsEnabled", 0, js_b2shape_are_sensor_events_enabled),
    JS_CFUNC_DEF("enableContactEvents", 1, js_b2shape_enable_contact_events),
    JS_CFUNC_DEF("areContactEventsEnabled", 0, js_b2shape_are_contact_events_enabled),
    JS_CFUNC_DEF("enableHitEvents", 1, js_b2shape_enable_hit_events),
    JS_CFUNC_DEF("areHitEventsEnabled", 0, js_b2shape_are_hit_events_enabled),
    JS_CFUNC_DEF("getFilter", 0, js_b2shape_get_filter),
    JS_CFUNC_DEF("setFilter", 1, js_b2shape_set_filter),
    JS_CFUNC_DEF("getUserData", 0, js_b2shape_get_user_data),
    JS_CFUNC_DEF("setUserData", 1, js_b2shape_set_user_data),
    JS_CFUNC_DEF("testPoint", 2, js_b2shape_test_point),
    JS_CFUNC_DEF("getAABB", 0, js_b2shape_get_aabb),
    JS_CFUNC_DEF("getCircle", 0, js_b2shape_get_circle),
    JS_CFUNC_DEF("getCapsule", 0, js_b2shape_get_capsule),
    JS_CFUNC_DEF("getPolygon", 0, js_b2shape_get_polygon),
    JS_CFUNC_DEF("getSegment", 0, js_b2shape_get_segment),
    JS_CFUNC_DEF("getChainSegment", 0, js_b2shape_get_chain_segment),
    JS_CFUNC_DEF("getClosestPoint", 2, js_b2shape_get_closest_point),
};


void js_b2shape_register(JSContext *ctx) {
    JS_NewClassID(&js_b2shape_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_b2shape_class_id, &js_b2shape_class);
    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_b2shape_proto_funcs, countof(js_b2shape_proto_funcs));
    JS_SetClassProto(ctx, js_b2shape_class_id, proto);
}

#endif