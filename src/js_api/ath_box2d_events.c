#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>

#ifdef ATHENA_BOX2D
#include <box2d/box2d.h>
#include "ath_box2d.h"

JSValue js_b2world_get_contact_events(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");

    b2ContactEvents events = b2World_GetContactEvents(w->worldId);
    JSValue result = JS_NewObject(ctx);

    JSValue begin = JS_NewArray(ctx);
    for (int i = 0; i < events.beginCount; i++) {
        JSValue e = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, e, "shapeA", js_b2_wrap_shape(ctx, w, events.beginEvents[i].shapeIdA));
        JS_SetPropertyStr(ctx, e, "shapeB", js_b2_wrap_shape(ctx, w, events.beginEvents[i].shapeIdB));
        JS_SetPropertyUint32(ctx, begin, i, e);
    }
    JS_SetPropertyStr(ctx, result, "begin", begin);

    JSValue end = JS_NewArray(ctx);
    for (int i = 0; i < events.endCount; i++) {
        JSValue e = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, e, "shapeA", js_b2_wrap_shape(ctx, w, events.endEvents[i].shapeIdA));
        JS_SetPropertyStr(ctx, e, "shapeB", js_b2_wrap_shape(ctx, w, events.endEvents[i].shapeIdB));
        JS_SetPropertyUint32(ctx, end, i, e);
    }
    JS_SetPropertyStr(ctx, result, "end", end);

    JSValue hit = JS_NewArray(ctx);
    for (int i = 0; i < events.hitCount; i++) {
        JSValue e = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, e, "shapeA", js_b2_wrap_shape(ctx, w, events.hitEvents[i].shapeIdA));
        JS_SetPropertyStr(ctx, e, "shapeB", js_b2_wrap_shape(ctx, w, events.hitEvents[i].shapeIdB));
        JS_SetPropertyStr(ctx, e, "point", js_b2_from_vec2(ctx, events.hitEvents[i].point));
        JS_SetPropertyStr(ctx, e, "normal", js_b2_from_vec2(ctx, events.hitEvents[i].normal));
        JS_SetPropertyStr(ctx, e, "approachSpeed", JS_NewFloat64(ctx, events.hitEvents[i].approachSpeed));
        JS_SetPropertyUint32(ctx, hit, i, e);
    }
    JS_SetPropertyStr(ctx, result, "hit", hit);

    return result;
}

JSValue js_b2world_get_sensor_events(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");

    b2SensorEvents events = b2World_GetSensorEvents(w->worldId);
    JSValue result = JS_NewObject(ctx);

    JSValue begin = JS_NewArray(ctx);
    for (int i = 0; i < events.beginCount; i++) {
        JSValue e = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, e, "sensor", js_b2_wrap_shape(ctx, w, events.beginEvents[i].sensorShapeId));
        JS_SetPropertyStr(ctx, e, "visitor", js_b2_wrap_shape(ctx, w, events.beginEvents[i].visitorShapeId));
        JS_SetPropertyUint32(ctx, begin, i, e);
    }
    JS_SetPropertyStr(ctx, result, "begin", begin);

    JSValue end = JS_NewArray(ctx);
    for (int i = 0; i < events.endCount; i++) {
        JSValue e = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, e, "sensor", js_b2_wrap_shape(ctx, w, events.endEvents[i].sensorShapeId));
        JS_SetPropertyStr(ctx, e, "visitor", js_b2_wrap_shape(ctx, w, events.endEvents[i].visitorShapeId));
        JS_SetPropertyUint32(ctx, end, i, e);
    }
    JS_SetPropertyStr(ctx, result, "end", end);

    return result;
}

JSValue js_b2world_get_body_events(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");

    b2BodyEvents events = b2World_GetBodyEvents(w->worldId);
    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < events.moveCount; i++) {
        b2BodyMoveEvent *ev = &events.moveEvents[i];
        JSValue e = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, e, "body", js_b2_wrap_body(ctx, w, ev->bodyId));
        JS_SetPropertyStr(ctx, e, "x", JS_NewFloat64(ctx, ev->transform.p.x));
        JS_SetPropertyStr(ctx, e, "y", JS_NewFloat64(ctx, ev->transform.p.y));
        JS_SetPropertyStr(ctx, e, "angle", JS_NewFloat64(ctx, atan2f(ev->transform.q.s, ev->transform.q.c)));
        JS_SetPropertyStr(ctx, e, "fellAsleep", JS_NewBool(ctx, ev->fellAsleep));
        JS_SetPropertyUint32(ctx, arr, i, e);
    }
    return arr;
}

JSValue js_b2world_explode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 5) return JS_ThrowSyntaxError(ctx, "x, y, radius, falloff and impulsePerLength required");

    double x, y, radius, falloff, impulse;
    if (!js_b2_to_float64_strict(ctx, argv[0], &x)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &y)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[2], &radius)) return JS_EXCEPTION;
    if (!js_b2_validate_radius(ctx, radius)) return JS_EXCEPTION;
    
    if (!js_b2_to_float64_strict(ctx, argv[3], &falloff)) return JS_EXCEPTION;
    if (!js_b2_validate_non_negative(ctx, "falloff", falloff)) return JS_EXCEPTION;
    
    if (!js_b2_to_float64_strict(ctx, argv[4], &impulse)) return JS_EXCEPTION;
    if (!js_b2_validate_non_negative(ctx, "impulsePerLength", impulse)) return JS_EXCEPTION;

    b2ExplosionDef def = b2DefaultExplosionDef();
    def.position = (b2Vec2){(float)x, (float)y};
    def.radius = (float)radius;
    def.falloff = (float)falloff;
    def.impulsePerLength = (float)impulse;
    if (argc >= 6) {
        int64_t mask;
        if (!js_b2_to_int64_strict(ctx, argv[5], &mask)) return JS_EXCEPTION;
        def.maskBits = (uint64_t)mask;
    }

    b2World_Explode(w->worldId, &def);
    return JS_UNDEFINED;
}

#endif