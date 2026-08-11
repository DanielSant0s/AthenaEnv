#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>

#ifdef ATHENA_BOX2D
#include <box2d/box2d.h>
#include "ath_box2d.h"

typedef struct JSB2OverlapContext {
    JSContext *ctx;
    JSB2World *world;
    JSValue array;
    uint32_t count;
} JSB2OverlapContext;

static bool js_b2_overlap_callback(b2ShapeId shapeId, void *context) {
    JSB2OverlapContext *oc = (JSB2OverlapContext *)context;
    JS_SetPropertyUint32(oc->ctx, oc->array, oc->count++, js_b2_wrap_shape(oc->ctx, oc->world, shapeId));
    return true;
}


typedef struct JSB2CastShapeContext {
    JSContext *ctx;
    JSB2World *world;
    JSValue array;
    uint32_t count;
} JSB2CastShapeContext;

static float js_b2_raycast_all_callback(b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void *context) {
    JSB2CastShapeContext *cc = (JSB2CastShapeContext *)context;
    JSValue obj = JS_NewObject(cc->ctx);
    JS_SetPropertyStr(cc->ctx, obj, "shape", js_b2_wrap_shape(cc->ctx, cc->world, shapeId));
    JS_SetPropertyStr(cc->ctx, obj, "point", js_b2_from_vec2(cc->ctx, point));
    JS_SetPropertyStr(cc->ctx, obj, "normal", js_b2_from_vec2(cc->ctx, normal));
    JS_SetPropertyStr(cc->ctx, obj, "fraction", JS_NewFloat64(cc->ctx, fraction));
    JS_SetPropertyUint32(cc->ctx, cc->array, cc->count++, obj);
    return 1.0f;
}

JSValue js_b2world_raycast_all(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
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

    JSB2CastShapeContext cc = { .ctx = ctx, .world = w, .array = JS_NewArray(ctx), .count = 0 };
    b2World_CastRay(w->worldId, (b2Vec2){(float)ox, (float)oy}, (b2Vec2){(float)tx, (float)ty}, filter, js_b2_raycast_all_callback, &cc);
    return cc.array;
}

JSValue js_b2world_query_aabb(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 4) return JS_ThrowSyntaxError(ctx, "lowerX, lowerY, upperX, upperY required");

    double lx, ly, ux, uy;
    if (!js_b2_to_float64_strict(ctx, argv[0], &lx)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &ly)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[2], &ux)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[3], &uy)) return JS_EXCEPTION;

    b2QueryFilter filter = b2DefaultQueryFilter();
    if (argc >= 5) {
        js_b2_parse_query_filter(ctx, argv[4], &filter);
    }

    b2AABB aabb = { .lowerBound = {(float)lx, (float)ly}, .upperBound = {(float)ux, (float)uy} };
    JSB2OverlapContext oc = { .ctx = ctx, .world = w, .array = JS_NewArray(ctx), .count = 0 };
    b2World_OverlapAABB(w->worldId, (b2Vec2){0, 0}, aabb, filter, js_b2_overlap_callback, &oc);
    return oc.array;
}

JSValue js_b2world_overlap_shape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 1 || !JS_IsArray(ctx, argv[0])) return JS_ThrowSyntaxError(ctx, "points array required");

    double radius = 0.0;
    JSValueConst pointsArg = argv[0];
    int filterArgIndex = 1;

    if (argc >= 2 && JS_IsNumber(argv[1])) {
        JS_ToFloat64(ctx, &radius, argv[1]);
        filterArgIndex = 2;
    }

    JSValue lenVal = JS_GetPropertyStr(ctx, pointsArg, "length");
    int32_t count = 0;
    JS_ToInt32(ctx, &count, lenVal);
    JS_FreeValue(ctx, lenVal);

    if (count < 1 || count > B2_MAX_POLYGON_VERTICES) {
        return JS_ThrowRangeError(ctx, "proxy must have between 1 and %d points", B2_MAX_POLYGON_VERTICES);
    }

    b2Vec2 points[B2_MAX_POLYGON_VERTICES];
    for (int32_t i = 0; i < count; i++) {
        JSValue el = JS_GetPropertyUint32(ctx, pointsArg, i);
        points[i] = js_b2_to_vec2(ctx, el, (b2Vec2){0.0f, 0.0f});
        JS_FreeValue(ctx, el);
    }

    b2ShapeProxy proxy = b2MakeProxy(points, count, (float)radius);

    b2QueryFilter filter = b2DefaultQueryFilter();
    if (argc > filterArgIndex) {
        js_b2_parse_query_filter(ctx, argv[filterArgIndex], &filter);
    }

    JSB2OverlapContext oc = { .ctx = ctx, .world = w, .array = JS_NewArray(ctx), .count = 0 };
    b2World_OverlapShape(w->worldId, (b2Vec2){0, 0}, &proxy, filter, js_b2_overlap_callback, &oc);
    return oc.array;
}

JSValue js_b2world_overlap_circle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 3) return JS_ThrowSyntaxError(ctx, "x, y and radius required");

    double x, y, radius;
    if (!js_b2_to_float64_strict(ctx, argv[0], &x)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &y)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[2], &radius)) return JS_EXCEPTION;

    b2Vec2 point = {(float)x, (float)y};
    b2ShapeProxy proxy = b2MakeProxy(&point, 1, (float)radius);

    b2QueryFilter filter = b2DefaultQueryFilter();
    if (argc >= 4) {
        js_b2_parse_query_filter(ctx, argv[3], &filter);
    }

    JSB2OverlapContext oc = { .ctx = ctx, .world = w, .array = JS_NewArray(ctx), .count = 0 };
    b2World_OverlapShape(w->worldId, (b2Vec2){0, 0}, &proxy, filter, js_b2_overlap_callback, &oc);
    return oc.array;
}

JSValue js_b2world_overlap_capsule(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 5) return JS_ThrowSyntaxError(ctx, "x1, y1, x2, y2 and radius required");

    double x1, y1, x2, y2, radius;
    JS_ToFloat64(ctx, &x1, argv[0]);
    JS_ToFloat64(ctx, &y1, argv[1]);
    JS_ToFloat64(ctx, &x2, argv[2]);
    JS_ToFloat64(ctx, &y2, argv[3]);
    JS_ToFloat64(ctx, &radius, argv[4]);

    b2Vec2 points[2] = {{(float)x1, (float)y1}, {(float)x2, (float)y2}};
    b2ShapeProxy proxy = b2MakeProxy(points, 2, (float)radius);

    b2QueryFilter filter = b2DefaultQueryFilter();
    if (argc >= 6) {
        js_b2_parse_query_filter(ctx, argv[5], &filter);
    }

    JSB2OverlapContext oc = { .ctx = ctx, .world = w, .array = JS_NewArray(ctx), .count = 0 };
    b2World_OverlapShape(w->worldId, (b2Vec2){0, 0}, &proxy, filter, js_b2_overlap_callback, &oc);
    return oc.array;
}

JSValue js_b2world_overlap_polygon(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 1 || !JS_IsArray(ctx, argv[0])) return JS_ThrowSyntaxError(ctx, "vertices array required");

    double radius = 0.0;
    JSValueConst vertices = argv[0];
    int filterArgIndex = 1;

    if (argc >= 2 && JS_IsNumber(argv[1])) {
        JS_ToFloat64(ctx, &radius, argv[1]);
        filterArgIndex = 2;
    }

    JSValue lenVal = JS_GetPropertyStr(ctx, vertices, "length");
    int32_t count = 0;
    JS_ToInt32(ctx, &count, lenVal);
    JS_FreeValue(ctx, lenVal);

    if (count < 3 || count > B2_MAX_POLYGON_VERTICES) {
        return JS_ThrowRangeError(ctx, "polygon must have between 3 and %d vertices", B2_MAX_POLYGON_VERTICES);
    }

    b2Vec2 points[B2_MAX_POLYGON_VERTICES];
    for (int32_t i = 0; i < count; i++) {
        JSValue el = JS_GetPropertyUint32(ctx, vertices, i);
        bool ok = js_b2_to_vec2_strict(ctx, el, i, &points[i]);
        JS_FreeValue(ctx, el);
        if (!ok) return JS_EXCEPTION;
    }

    b2ShapeProxy proxy = b2MakeProxy(points, count, (float)radius);

    b2QueryFilter filter = b2DefaultQueryFilter();
    if (argc > filterArgIndex) {
        js_b2_parse_query_filter(ctx, argv[filterArgIndex], &filter);
    }

    JSB2OverlapContext oc = { .ctx = ctx, .world = w, .array = JS_NewArray(ctx), .count = 0 };
    b2World_OverlapShape(w->worldId, (b2Vec2){0, 0}, &proxy, filter, js_b2_overlap_callback, &oc);
    return oc.array;
}

#endif