#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>

#ifdef ATHENA_BOX2D
#include <box2d/box2d.h>
#include "ath_box2d.h"

bool js_b2_validate_positive(JSContext *ctx, const char *name, double value) {
    if (value <= 0.0) {
        JS_ThrowRangeError(ctx, "%s must be > 0", name);
        return false;
    }
    return true;
}

bool js_b2_validate_non_negative(JSContext *ctx, const char *name, double value) {
    if (value < 0.0) {
        JS_ThrowRangeError(ctx, "%s must be >= 0", name);
        return false;
    }
    return true;
}

bool js_b2_validate_restitution(JSContext *ctx, double value) {
    if (value < 0.0 || value > 1.0) {
        JS_ThrowRangeError(ctx, "restitution must be between 0 and 1");
        return false;
    }
    return true;
}

bool js_b2_validate_friction(JSContext *ctx, double value) {
    if (value < 0.0) {
        JS_ThrowRangeError(ctx, "friction must be >= 0");
        return false;
    }
    return true;
}

bool js_b2_validate_density(JSContext *ctx, double value) {
    if (value < 0.0) {
        JS_ThrowRangeError(ctx, "density must be >= 0");
        return false;
    }
    return true;
}

bool js_b2_validate_radius(JSContext *ctx, double value) {
    if (value <= 0.0) {
        JS_ThrowRangeError(ctx, "radius must be > 0");
        return false;
    }
    return true;
}

bool js_b2_validate_damping(JSContext *ctx, double value) {
    if (value < 0.0) {
        JS_ThrowRangeError(ctx, "damping must be >= 0");
        return false;
    }
    return true;
}

bool js_b2_validate_hertz(JSContext *ctx, double value) {
    if (value < 0.0) {
        JS_ThrowRangeError(ctx, "hertz must be >= 0");
        return false;
    }
    return true;
}

bool js_b2_validate_half_dimension(JSContext *ctx, const char *name, double value) {
    if (value <= 0.0) {
        JS_ThrowRangeError(ctx, "%s must be > 0", name);
        return false;
    }
    return true;
}

b2Vec2 js_b2_to_vec2(JSContext *ctx, JSValueConst obj, b2Vec2 fallback) {
    if (!JS_IsObject(obj)) return fallback;
    b2Vec2 v = fallback;
    JSValue vx = JS_GetPropertyStr(ctx, obj, "x");
    JSValue vy = JS_GetPropertyStr(ctx, obj, "y");
    double x = fallback.x, y = fallback.y;
    if (!JS_IsUndefined(vx)) JS_ToFloat64(ctx, &x, vx);
    if (!JS_IsUndefined(vy)) JS_ToFloat64(ctx, &y, vy);
    v.x = (float)x;
    v.y = (float)y;
    JS_FreeValue(ctx, vx);
    JS_FreeValue(ctx, vy);
    return v;
}

bool js_b2_to_vec2_strict(JSContext *ctx, JSValueConst val, int index, b2Vec2 *out) {
    if (!JS_IsObject(val)) {
        JS_ThrowTypeError(ctx, "point at index %d is not a valid vector (expected { x, y })", index);
        return false;
    }

    JSValue vx = JS_GetPropertyStr(ctx, val, "x");
    JSValue vy = JS_GetPropertyStr(ctx, val, "y");
    bool ok = !JS_IsUndefined(vx) && !JS_IsUndefined(vy) && JS_IsNumber(vx) && JS_IsNumber(vy);

    double x = 0.0, y = 0.0;
    if (ok) {
        if (JS_ToFloat64(ctx, &x, vx) < 0 || JS_ToFloat64(ctx, &y, vy) < 0) ok = false;
    }

    JS_FreeValue(ctx, vx);
    JS_FreeValue(ctx, vy);

    if (!ok) {
        JS_ThrowTypeError(ctx, "point at index %d is not a valid vector (expected { x: number, y: number })", index);
        return false;
    }

    if (!isfinite(x) || !isfinite(y)) {
        JS_ThrowTypeError(ctx, "point at index %d has a non-finite coordinate", index);
        return false;
    }

    out->x = (float)x;
    out->y = (float)y;
    return true;
}

JSValue js_b2_from_vec2(JSContext *ctx, b2Vec2 v) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, v.x));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, v.y));
    return obj;
}

bool js_b2_parse_collision_filter(
    JSContext *ctx,
    JSValueConst value,
    b2Filter *filter)
{
    if (!JS_IsObject(value)) {
        return false;
    }

    b2Filter defaultFilter = { .categoryBits = 1, .maskBits = -1, .groupIndex = 0 };
    *filter = defaultFilter;

    JSValue cat = JS_GetPropertyStr(ctx, value, "categoryBits");
    JSValue mask = JS_GetPropertyStr(ctx, value, "maskBits");
    JSValue grp = JS_GetPropertyStr(ctx, value, "groupIndex");

    if (!JS_IsUndefined(cat)) {
        int64_t c;
        JS_ToInt64(ctx, &c, cat);
        filter->categoryBits = (uint64_t)c;
    }

    if (!JS_IsUndefined(mask)) {
        int64_t m;
        JS_ToInt64(ctx, &m, mask);
        filter->maskBits = (uint64_t)m;
    }

    if (!JS_IsUndefined(grp)) {
        int32_t g;
        JS_ToInt32(ctx, &g, grp);
        filter->groupIndex = g;
    }

    JS_FreeValue(ctx, cat);
    JS_FreeValue(ctx, mask);
    JS_FreeValue(ctx, grp);

    return true;
}

bool js_b2_parse_query_filter(
    JSContext *ctx,
    JSValueConst value,
    b2QueryFilter *filter)
{
    if (!JS_IsObject(value)) {
        return false;
    }

    b2QueryFilter defaultFilter = b2DefaultQueryFilter();
    *filter = defaultFilter;

    JSValue cat = JS_GetPropertyStr(ctx, value, "categoryBits");
    JSValue mask = JS_GetPropertyStr(ctx, value, "maskBits");

    if (!JS_IsUndefined(cat)) {
        int64_t c;
        JS_ToInt64(ctx, &c, cat);
        filter->categoryBits = (uint64_t)c;
    }

    if (!JS_IsUndefined(mask)) {
        int64_t m;
        JS_ToInt64(ctx, &m, mask);
        filter->maskBits = (uint64_t)m;
    }

    JS_FreeValue(ctx, cat);
    JS_FreeValue(ctx, mask);

    return true;
}

bool js_b2_to_float64_strict(JSContext *ctx, JSValueConst val, double *out) {
    if (!JS_IsNumber(val)) {
        JS_ThrowTypeError(ctx, "Expected a number");
        return false;
    }
    
    if (JS_ToFloat64(ctx, out, val) < 0) {
        JS_ThrowTypeError(ctx, "Failed to convert to number");
        return false;
    }
    
    if (!isfinite(*out)) {
        JS_ThrowTypeError(ctx, "Number must be finite");
        return false;
    }
    
    return true;
}

bool js_b2_to_int32_strict(JSContext *ctx, JSValueConst val, int32_t *out) {
    if (!JS_IsNumber(val)) {
        JS_ThrowTypeError(ctx, "Expected an integer");
        return false;
    }
    
    if (JS_ToInt32(ctx, out, val) < 0) {
        JS_ThrowTypeError(ctx, "Failed to convert to integer");
        return false;
    }
    
    return true;
}

bool js_b2_to_int64_strict(JSContext *ctx, JSValueConst val, int64_t *out) {
    if (!JS_IsNumber(val)) {
        JS_ThrowTypeError(ctx, "Expected an integer");
        return false;
    }
    
    if (JS_ToInt64(ctx, out, val) < 0) {
        JS_ThrowTypeError(ctx, "Failed to convert to integer");
        return false;
    }
    
    return true;
}

bool js_b2_to_bool_strict(JSContext *ctx, JSValueConst val, bool *out) {
    if (!JS_IsBool(val) && !JS_IsNumber(val)) {
        JS_ThrowTypeError(ctx, "Expected a boolean");
        return false;
    }
    
    *out = JS_ToBool(ctx, val);
    return true;
}

bool js_b2_apply_shape_options(JSContext *ctx, JSValueConst opts, b2ShapeDef *shapeDef) {
    if (!JS_IsObject(opts)) return true;
    JSValue val;

    val = JS_GetPropertyStr(ctx, opts, "friction");
    if (!JS_IsUndefined(val)) {
        double f = 0.6;
        JS_ToFloat64(ctx, &f, val);
        if (!js_b2_validate_friction(ctx, f)) {
            JS_FreeValue(ctx, val);
            return false;
        }
        shapeDef->material.friction = (float)f;
        JS_FreeValue(ctx, val);
    }

    val = JS_GetPropertyStr(ctx, opts, "restitution");
    if (!JS_IsUndefined(val)) {
        double r = 0.0;
        JS_ToFloat64(ctx, &r, val);
        if (!js_b2_validate_restitution(ctx, r)) {
            JS_FreeValue(ctx, val);
            return false;
        }
        shapeDef->material.restitution = (float)r;
        JS_FreeValue(ctx, val);
    }

    val = JS_GetPropertyStr(ctx, opts, "rollingResistance");
    if (!JS_IsUndefined(val)) {
        double r = 0.0;
        JS_ToFloat64(ctx, &r, val);
        if (!js_b2_validate_non_negative(ctx, "rollingResistance", r)) {
            JS_FreeValue(ctx, val);
            return false;
        }
        shapeDef->material.rollingResistance = (float)r;
        JS_FreeValue(ctx, val);
    }

    val = JS_GetPropertyStr(ctx, opts, "tangentSpeed");
    if (!JS_IsUndefined(val)) {
        double t = 0.0;
        JS_ToFloat64(ctx, &t, val);
        shapeDef->material.tangentSpeed = (float)t;
        JS_FreeValue(ctx, val);
    }

    val = JS_GetPropertyStr(ctx, opts, "isSensor");
    if (!JS_IsUndefined(val)) {
        shapeDef->isSensor = JS_ToBool(ctx, val);
        JS_FreeValue(ctx, val);
    }

    val = JS_GetPropertyStr(ctx, opts, "enableSensorEvents");
    if (!JS_IsUndefined(val)) {
        shapeDef->enableSensorEvents = JS_ToBool(ctx, val);
        JS_FreeValue(ctx, val);
    }

    val = JS_GetPropertyStr(ctx, opts, "enableContactEvents");
    if (!JS_IsUndefined(val)) {
        shapeDef->enableContactEvents = JS_ToBool(ctx, val);
        JS_FreeValue(ctx, val);
    }

    val = JS_GetPropertyStr(ctx, opts, "enableHitEvents");
    if (!JS_IsUndefined(val)) {
        shapeDef->enableHitEvents = JS_ToBool(ctx, val);
        JS_FreeValue(ctx, val);
    }

    val = JS_GetPropertyStr(ctx, opts, "filter");
    if (JS_IsObject(val)) {
        js_b2_parse_collision_filter(ctx, val, &shapeDef->filter);
    }
    JS_FreeValue(ctx, val);

    return true;
}


#endif