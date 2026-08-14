#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>

#ifdef ATHENA_BOX2D
#include <box2d/box2d.h>
#include "ath_box2d.h"

typedef struct JSB2CastShapeContext {
    JSContext *ctx;
    JSB2World *world;
    JSValue array;
    uint32_t count;
} JSB2CastShapeContext;

typedef struct JSB2MoverContext {
    JSContext *ctx;
    JSB2World *world;
    JSValue array;
    uint32_t count;
} JSB2MoverContext;


JSValue js_b2world_cast_ray(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 4) return JS_ThrowSyntaxError(ctx, "originX, originY, translationX, translationY required");

    double ox, oy, tx, ty;
    if (!js_b2_to_float64_strict(ctx, argv[0], &ox)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &oy)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[2], &tx)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[3], &ty)) return JS_EXCEPTION;

    b2QueryFilter filter = b2DefaultQueryFilter();
    if (argc >= 5) {
        js_b2_parse_query_filter(ctx, argv[4], &filter);
    }

    b2RayResult result = b2World_CastRayClosest(w->worldId, (b2Vec2){(float)ox, (float)oy}, (b2Vec2){(float)tx, (float)ty}, filter);
    if (!result.hit) return JS_NULL;

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "point", js_b2_from_vec2(ctx, result.point));
    JS_SetPropertyStr(ctx, obj, "normal", js_b2_from_vec2(ctx, result.normal));
    JS_SetPropertyStr(ctx, obj, "fraction", JS_NewFloat64(ctx, result.fraction));
    JS_SetPropertyStr(ctx, obj, "shape", js_b2_wrap_shape(ctx, w, result.shapeId));
    return obj;
}
static float js_b2_cast_shape_callback(b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void *context) {
 JSB2CastShapeContext *cc = (JSB2CastShapeContext *)context;
 JSValue obj = JS_NewObject(cc->ctx);
 JS_SetPropertyStr(cc->ctx, obj, "shape", js_b2_wrap_shape(cc->ctx, cc->world, shapeId));
 JS_SetPropertyStr(cc->ctx, obj, "point", js_b2_from_vec2(cc->ctx, point));
 JS_SetPropertyStr(cc->ctx, obj, "normal", js_b2_from_vec2(cc->ctx, normal));
 JS_SetPropertyStr(cc->ctx, obj, "fraction", JS_NewFloat64(cc->ctx, fraction));
 JS_SetPropertyUint32(cc->ctx, cc->array, cc->count++, obj);
 return 1.0f;
}

JSValue js_b2world_cast_shape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 4) return JS_ThrowSyntaxError(ctx, "points, radius, translationX and translationY required");
    if (!JS_IsArray(ctx, argv[0])) return JS_ThrowTypeError(ctx, "points must be an array");

    double radius = 0.0;
    JS_ToFloat64(ctx, &radius, argv[1]);

    double tx, ty;
    JS_ToFloat64(ctx, &tx, argv[2]);
    JS_ToFloat64(ctx, &ty, argv[3]);

    JSValue lenVal = JS_GetPropertyStr(ctx, argv[0], "length");
    int32_t count = 0;
    JS_ToInt32(ctx, &count, lenVal);
    JS_FreeValue(ctx, lenVal);

    if (count < 1 || count > B2_MAX_POLYGON_VERTICES) {
        return JS_ThrowRangeError(ctx, "proxy must have between 1 and %d points", B2_MAX_POLYGON_VERTICES);
    }

    b2Vec2 points[B2_MAX_POLYGON_VERTICES];
    for (int32_t i = 0; i < count; i++) {
        JSValue el = JS_GetPropertyUint32(ctx, argv[0], i);
        points[i] = js_b2_to_vec2(ctx, el, (b2Vec2){0.0f, 0.0f});
        JS_FreeValue(ctx, el);
    }

    b2ShapeProxy proxy = b2MakeProxy(points, count, (float)radius);

    b2QueryFilter filter = b2DefaultQueryFilter();
    if (argc >= 5) {
        js_b2_parse_query_filter(ctx, argv[4], &filter);
    }

    JSB2CastShapeContext cc = { .ctx = ctx, .world = w, .array = JS_NewArray(ctx), .count = 0 };
    b2World_CastShape(w->worldId, (b2Vec2){0, 0}, &proxy, (b2Vec2){(float)tx, (float)ty}, filter, js_b2_cast_shape_callback, &cc);
    return cc.array;
}

JSValue js_b2world_cast_circle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 5) return JS_ThrowSyntaxError(ctx, "x, y, radius, translationX and translationY required");

    double x, y, radius, tx, ty;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    JS_ToFloat64(ctx, &radius, argv[2]);
    JS_ToFloat64(ctx, &tx, argv[3]);
    JS_ToFloat64(ctx, &ty, argv[4]);

    b2Vec2 point = {(float)x, (float)y};
    b2ShapeProxy proxy = b2MakeProxy(&point, 1, (float)radius);

    b2QueryFilter filter = b2DefaultQueryFilter();
    if (argc >= 6) {
        js_b2_parse_query_filter(ctx, argv[5], &filter);
    }

    JSB2CastShapeContext cc = { .ctx = ctx, .world = w, .array = JS_NewArray(ctx), .count = 0 };
    b2World_CastShape(w->worldId, (b2Vec2){0, 0}, &proxy, (b2Vec2){(float)tx, (float)ty}, filter, js_b2_cast_shape_callback, &cc);
    return cc.array;
}

JSValue js_b2world_cast_capsule(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 7) return JS_ThrowSyntaxError(ctx, "x1, y1, x2, y2, radius, translationX and translationY required");

    double x1, y1, x2, y2, radius, tx, ty;
    JS_ToFloat64(ctx, &x1, argv[0]);
    JS_ToFloat64(ctx, &y1, argv[1]);
    JS_ToFloat64(ctx, &x2, argv[2]);
    JS_ToFloat64(ctx, &y2, argv[3]);
    JS_ToFloat64(ctx, &radius, argv[4]);
    JS_ToFloat64(ctx, &tx, argv[5]);
    JS_ToFloat64(ctx, &ty, argv[6]);

    b2Vec2 points[2] = {{(float)x1, (float)y1}, {(float)x2, (float)y2}};
    b2ShapeProxy proxy = b2MakeProxy(points, 2, (float)radius);

    b2QueryFilter filter = b2DefaultQueryFilter();
    if (argc >= 8) {
        js_b2_parse_query_filter(ctx, argv[7], &filter);
    }

    JSB2CastShapeContext cc = { .ctx = ctx, .world = w, .array = JS_NewArray(ctx), .count = 0 };
    b2World_CastShape(w->worldId, (b2Vec2){0, 0}, &proxy, (b2Vec2){(float)tx, (float)ty}, filter, js_b2_cast_shape_callback, &cc);
    return cc.array;
}

JSValue js_b2world_cast_polygon(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 4) return JS_ThrowSyntaxError(ctx, "vertices, radius, translationX and translationY required");
    if (!JS_IsArray(ctx, argv[0])) return JS_ThrowTypeError(ctx, "vertices must be an array");

    double radius = 0.0;
    JS_ToFloat64(ctx, &radius, argv[1]);

    double tx, ty;
    JS_ToFloat64(ctx, &tx, argv[2]);
    JS_ToFloat64(ctx, &ty, argv[3]);

    JSValue lenVal = JS_GetPropertyStr(ctx, argv[0], "length");
    int32_t count = 0;
    JS_ToInt32(ctx, &count, lenVal);
    JS_FreeValue(ctx, lenVal);

    if (count < 3 || count > B2_MAX_POLYGON_VERTICES) {
        return JS_ThrowRangeError(ctx, "polygon must have between 3 and %d vertices", B2_MAX_POLYGON_VERTICES);
    }

    b2Vec2 points[B2_MAX_POLYGON_VERTICES];
    for (int32_t i = 0; i < count; i++) {
        JSValue el = JS_GetPropertyUint32(ctx, argv[0], i);
        bool ok = js_b2_to_vec2_strict(ctx, el, i, &points[i]);
        JS_FreeValue(ctx, el);
        if (!ok) return JS_EXCEPTION;
    }

    b2ShapeProxy proxy = b2MakeProxy(points, count, (float)radius);

    b2QueryFilter filter = b2DefaultQueryFilter();
    if (argc >= 5) {
        js_b2_parse_query_filter(ctx, argv[4], &filter);
    }

    JSB2CastShapeContext cc = { .ctx = ctx, .world = w, .array = JS_NewArray(ctx), .count = 0 };
    b2World_CastShape(w->worldId, (b2Vec2){0, 0}, &proxy, (b2Vec2){(float)tx, (float)ty}, filter, js_b2_cast_shape_callback, &cc);
    return cc.array;
}

static bool js_b2_mover_callback(b2ShapeId shapeId, b2Plane plane, void *context) {
    JSB2MoverContext *mc = (JSB2MoverContext *)context;
    JSValue obj = JS_NewObject(mc->ctx);
    JS_SetPropertyStr(mc->ctx, obj, "shape", js_b2_wrap_shape(mc->ctx, mc->world, shapeId));
    JS_SetPropertyStr(mc->ctx, obj, "normal", js_b2_from_vec2(mc->ctx, plane.normal));
    JS_SetPropertyStr(mc->ctx, obj, "offset", JS_NewFloat64(mc->ctx, plane.offset));
    JS_SetPropertyUint32(mc->ctx, mc->array, mc->count++, obj);
    return true;
}

JSValue js_b2world_cast_mover(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 9) return JS_ThrowSyntaxError(ctx, "x, y, x1, y1, x2, y2, radius, tx, ty required");

    double x, y, x1, y1, x2, y2, radius, tx, ty;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    JS_ToFloat64(ctx, &x1, argv[2]);
    JS_ToFloat64(ctx, &y1, argv[3]);
    JS_ToFloat64(ctx, &x2, argv[4]);
    JS_ToFloat64(ctx, &y2, argv[5]);
    JS_ToFloat64(ctx, &radius, argv[6]);
    JS_ToFloat64(ctx, &tx, argv[7]);
    JS_ToFloat64(ctx, &ty, argv[8]);

    b2Capsule mover = {
        .center1 = {(float)x1, (float)y1},
        .center2 = {(float)x2, (float)y2},
        .radius = (float)radius
    };

    b2QueryFilter filter = b2DefaultQueryFilter();
    if (argc >= 10) {
        js_b2_parse_query_filter(ctx, argv[9], &filter);
    }

    float fraction = b2World_CastMover(w->worldId, (b2Vec2){(float)x, (float)y}, &mover,
                                       (b2Vec2){(float)tx, (float)ty}, filter);
    return JS_NewFloat64(ctx, fraction);
}

JSValue js_b2world_collide_mover(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 7) return JS_ThrowSyntaxError(ctx, "x, y, x1, y1, x2, y2, radius required");

    double x, y, x1, y1, x2, y2, radius;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    JS_ToFloat64(ctx, &x1, argv[2]);
    JS_ToFloat64(ctx, &y1, argv[3]);
    JS_ToFloat64(ctx, &x2, argv[4]);
    JS_ToFloat64(ctx, &y2, argv[5]);
    JS_ToFloat64(ctx, &radius, argv[6]);

    b2Capsule mover = {
        .center1 = {(float)x1, (float)y1},
        .center2 = {(float)x2, (float)y2},
        .radius = (float)radius
    };

    b2QueryFilter filter = b2DefaultQueryFilter();
    if (argc >= 8) {
        js_b2_parse_query_filter(ctx, argv[7], &filter);
    }

    JSB2MoverContext mc = { .ctx = ctx, .world = w, .array = JS_NewArray(ctx), .count = 0 };
    b2World_CollideMover(w->worldId, (b2Vec2){(float)x, (float)y}, &mover, filter, js_b2_mover_callback, &mc);
    return mc.array;
}

#endif