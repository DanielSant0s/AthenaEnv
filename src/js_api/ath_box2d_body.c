#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>

#ifdef ATHENA_BOX2D
#include <box2d/box2d.h>
#include "ath_box2d.h"

static void js_b2body_finalizer(JSRuntime *rt, JSValue val) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque(val, js_b2body_class_id);
    if (b) {
        if (b->world && b2Body_IsValid(b->bodyId)) {
            js_b2_wrapper_cache_remove(b->world, JSB2_WRAPPER_BODY, b2StoreBodyId(b->bodyId));
        }
        free(b);
    }
}

static JSClassDef js_b2body_class = {
    "B2Body",
    .finalizer = js_b2body_finalizer,
};

static JSValue js_b2body_get_type(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    return JS_NewInt32(ctx, (int32_t)b2Body_GetType(b->bodyId));
}

static JSValue js_b2body_set_type(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "Type required");

    int32_t type;
    if (!js_b2_to_int32_strict(ctx, argv[0], &type)) return JS_EXCEPTION;
    b2Body_SetType(b->bodyId, (b2BodyType)type);
    return JS_UNDEFINED;
}

static JSValue js_b2body_create_chain(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1 || !JS_IsArray(ctx, argv[0])) return JS_ThrowSyntaxError(ctx, "points array required");

    JSValue lenVal = JS_GetPropertyStr(ctx, argv[0], "length");
    int32_t count = 0;
    JS_ToInt32(ctx, &count, lenVal);
    JS_FreeValue(ctx, lenVal);

    if (count < 2) {
        return JS_ThrowRangeError(ctx, "chain must have at least 2 points");
    }

    b2Vec2 *points = (b2Vec2 *)malloc(sizeof(b2Vec2) * count);
    if (!points) return JS_ThrowOutOfMemory(ctx);

    for (int32_t i = 0; i < count; i++) {
        JSValue el = JS_GetPropertyUint32(ctx, argv[0], i);
        points[i] = js_b2_to_vec2(ctx, el, (b2Vec2){0.0f, 0.0f});
        JS_FreeValue(ctx, el);
    }

    b2ChainDef chainDef = b2DefaultChainDef();
    chainDef.points = points;
    chainDef.count = count;

    if (argc >= 2 && JS_IsObject(argv[1])) {
        JSValue val;

        val = JS_GetPropertyStr(ctx, argv[1], "isLoop");
        if (!JS_IsUndefined(val)) {
            chainDef.isLoop = JS_ToBool(ctx, val);
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[1], "filter");
        if (JS_IsObject(val)) {
            js_b2_parse_collision_filter(ctx, val, &chainDef.filter);
        }
        JS_FreeValue(ctx, val);
    }

    b2ChainId chainId = b2CreateChain(b->bodyId, &chainDef);
    free(points);

    return js_b2_wrap_chain(ctx, b->world, chainId);
}

static JSValue js_b2body_get_position(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");

    b2Vec2 pos = b2Body_GetPosition(b->bodyId);
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, pos.x));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, pos.y));
    return obj;
}

static JSValue js_b2body_set_position(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "x and y required");

    double x, y;
    if (!js_b2_to_float64_strict(ctx, argv[0], &x)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &y)) return JS_EXCEPTION;

    b2Rot rot = b2Body_GetRotation(b->bodyId);
    b2Body_SetTransform(b->bodyId, (b2Vec2){(float)x, (float)y}, rot);
    return JS_UNDEFINED;
}

static JSValue js_b2body_get_angle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");

    b2Rot rot = b2Body_GetRotation(b->bodyId);
    float angle = atan2f(rot.s, rot.c);
    return JS_NewFloat64(ctx, angle);
}

static JSValue js_b2body_set_transform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 3) return JS_ThrowSyntaxError(ctx, "x, y, and angle required");

    double x, y, angle;
    if (!js_b2_to_float64_strict(ctx, argv[0], &x)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &y)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[2], &angle)) return JS_EXCEPTION;

    b2Body_SetTransform(b->bodyId, (b2Vec2){(float)x, (float)y}, b2MakeRot((float)angle));
    return JS_UNDEFINED;
}

static JSValue js_b2body_get_linear_velocity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");

    b2Vec2 vel = b2Body_GetLinearVelocity(b->bodyId);
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, vel.x));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, vel.y));
    return obj;
}

static JSValue js_b2body_set_linear_velocity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "vx and vy required");

    double vx, vy;
    if (!js_b2_to_float64_strict(ctx, argv[0], &vx)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &vy)) return JS_EXCEPTION;

    b2Body_SetLinearVelocity(b->bodyId, (b2Vec2){(float)vx, (float)vy});
    return JS_UNDEFINED;
}

static JSValue js_b2body_get_angular_velocity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");

    float w = b2Body_GetAngularVelocity(b->bodyId);
    return JS_NewFloat64(ctx, w);
}

static JSValue js_b2body_set_angular_velocity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "Angular velocity required");

    double w;
    if (!js_b2_to_float64_strict(ctx, argv[0], &w)) return JS_EXCEPTION;
    b2Body_SetAngularVelocity(b->bodyId, (float)w);
    return JS_UNDEFINED;
}

static JSValue js_b2body_apply_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 4) return JS_ThrowSyntaxError(ctx, "fx, fy, px, py required");

    double fx, fy, px, py;
    if (!js_b2_to_float64_strict(ctx, argv[0], &fx)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &fy)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[2], &px)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[3], &py)) return JS_EXCEPTION;

    b2Body_ApplyForce(b->bodyId, (b2Vec2){(float)fx, (float)fy}, (b2Vec2){(float)px, (float)py}, true);
    return JS_UNDEFINED;
}

static JSValue js_b2body_apply_force_to_center(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "fx and fy required");

    double fx, fy;
    if (!js_b2_to_float64_strict(ctx, argv[0], &fx)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &fy)) return JS_EXCEPTION;

    b2Body_ApplyForceToCenter(b->bodyId, (b2Vec2){(float)fx, (float)fy}, true);
    return JS_UNDEFINED;
}

static JSValue js_b2body_apply_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "Torque required");

    double torque;
    if (!js_b2_to_float64_strict(ctx, argv[0], &torque)) return JS_EXCEPTION;

    b2Body_ApplyTorque(b->bodyId, (float)torque, true);
    return JS_UNDEFINED;
}

static JSValue js_b2body_apply_linear_impulse(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 4) return JS_ThrowSyntaxError(ctx, "ix, iy, px, py required");

    double ix, iy, px, py;
    if (!js_b2_to_float64_strict(ctx, argv[0], &ix)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &iy)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[2], &px)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[3], &py)) return JS_EXCEPTION;

    b2Body_ApplyLinearImpulse(b->bodyId, (b2Vec2){(float)ix, (float)iy}, (b2Vec2){(float)px, (float)py}, true);
    return JS_UNDEFINED;
}

static JSValue js_b2body_apply_linear_impulse_to_center(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "ix and iy required");

    double ix, iy;
    if (!js_b2_to_float64_strict(ctx, argv[0], &ix)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &iy)) return JS_EXCEPTION;

    b2Body_ApplyLinearImpulseToCenter(b->bodyId, (b2Vec2){(float)ix, (float)iy}, true);
    return JS_UNDEFINED;
}

static JSValue js_b2body_apply_angular_impulse(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "Impulse required");

    double impulse;
    if (!js_b2_to_float64_strict(ctx, argv[0], &impulse)) return JS_EXCEPTION;

    b2Body_ApplyAngularImpulse(b->bodyId, (float)impulse, true);
    return JS_UNDEFINED;
}

static JSValue js_b2body_get_mass(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");

    float mass = b2Body_GetMass(b->bodyId);
    return JS_NewFloat64(ctx, mass);
}

static JSValue js_b2body_get_linear_damping(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");

    float d = b2Body_GetLinearDamping(b->bodyId);
    return JS_NewFloat64(ctx, d);
}

static JSValue js_b2body_set_linear_damping(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "Damping required");

    double d;
    if (!js_b2_to_float64_strict(ctx, argv[0], &d)) return JS_EXCEPTION;
    if (!js_b2_validate_damping(ctx, d)) return JS_EXCEPTION;
    b2Body_SetLinearDamping(b->bodyId, (float)d);
    return JS_UNDEFINED;
}

static JSValue js_b2body_get_angular_damping(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");

    float d = b2Body_GetAngularDamping(b->bodyId);
    return JS_NewFloat64(ctx, d);
}

static JSValue js_b2body_set_angular_damping(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "Damping required");

    double d;
    if (!js_b2_to_float64_strict(ctx, argv[0], &d)) return JS_EXCEPTION;
    if (!js_b2_validate_damping(ctx, d)) return JS_EXCEPTION;
    b2Body_SetAngularDamping(b->bodyId, (float)d);
    return JS_UNDEFINED;
}

static JSValue js_b2body_get_gravity_scale(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");

    float scale = b2Body_GetGravityScale(b->bodyId);
    return JS_NewFloat64(ctx, scale);
}

static JSValue js_b2body_set_gravity_scale(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "Gravity scale required");

    double scale;
    if (!js_b2_to_float64_strict(ctx, argv[0], &scale)) return JS_EXCEPTION;
    if (!js_b2_validate_non_negative(ctx, "gravityScale", scale)) return JS_EXCEPTION;
    b2Body_SetGravityScale(b->bodyId, (float)scale);
    return JS_UNDEFINED;
}

static JSValue js_b2body_is_awake(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_FALSE;

    return JS_NewBool(ctx, b2Body_IsAwake(b->bodyId));
}

static JSValue js_b2body_set_awake(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "Awake flag required");

    bool awake;
    if (!js_b2_to_bool_strict(ctx, argv[0], &awake)) return JS_EXCEPTION;
    b2Body_SetAwake(b->bodyId, awake);
    return JS_UNDEFINED;
}

static JSValue js_b2body_is_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_FALSE;

    return JS_NewBool(ctx, b2Body_IsEnabled(b->bodyId));
}

static JSValue js_b2body_set_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "Enabled flag required");

    bool enabled;
    if (!js_b2_to_bool_strict(ctx, argv[0], &enabled)) return JS_EXCEPTION;
    if (enabled) {
        b2Body_Enable(b->bodyId);
    } else {
        b2Body_Disable(b->bodyId);
    }
    return JS_UNDEFINED;
}

static JSValue js_b2body_is_fixed_rotation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_FALSE;

    b2MotionLocks locks = b2Body_GetMotionLocks(b->bodyId);
    return JS_NewBool(ctx, locks.angularZ);
}

static JSValue js_b2body_set_fixed_rotation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "Flag required");

    bool flag;
    if (!js_b2_to_bool_strict(ctx, argv[0], &flag)) return JS_EXCEPTION;
    b2MotionLocks locks = b2Body_GetMotionLocks(b->bodyId);
    locks.angularZ = flag;
    b2Body_SetMotionLocks(b->bodyId, locks);
    return JS_UNDEFINED;
}

static JSValue js_b2body_is_valid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque(this_val, js_b2body_class_id);
    if (!b) return JS_FALSE;
    return JS_NewBool(ctx, b2Body_IsValid(b->bodyId));
}

static JSValue js_b2body_create_circle_shape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1 || !JS_IsObject(argv[0]) || JS_IsNull(argv[0]))
        return JS_ThrowTypeError(ctx, "expected an object with { radius, center?, density?, ... }");

    double radius = 1.0;
    double density = 1.0;
    b2Vec2 center = {0.0f, 0.0f};

    JSValue radiusVal = JS_GetPropertyStr(ctx, argv[0], "radius");
    if (JS_IsUndefined(radiusVal)) {
        JS_FreeValue(ctx, radiusVal);
        return JS_ThrowTypeError(ctx, "radius is required");
    }
    if (!js_b2_to_float64_strict(ctx, radiusVal, &radius)) {
        JS_FreeValue(ctx, radiusVal);
        return JS_EXCEPTION;
    }
    if (!js_b2_validate_radius(ctx, radius)) {
        JS_FreeValue(ctx, radiusVal);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, radiusVal);

    JSValue centerVal = JS_GetPropertyStr(ctx, argv[0], "center");
    if (JS_IsObject(centerVal)) center = js_b2_to_vec2(ctx, centerVal, center);
    JS_FreeValue(ctx, centerVal);

    JSValue densityVal = JS_GetPropertyStr(ctx, argv[0], "density");
    if (!JS_IsUndefined(densityVal)) {
        if (!js_b2_to_float64_strict(ctx, densityVal, &density)) {
            JS_FreeValue(ctx, densityVal);
            return JS_EXCEPTION;
        }
        if (!js_b2_validate_density(ctx, density)) {
            JS_FreeValue(ctx, densityVal);
            return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, densityVal);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = (float)density;
    if (!js_b2_apply_shape_options(ctx, argv[0], &shapeDef)) {
        return JS_EXCEPTION;
    }

    b2Circle circle = { .center = center, .radius = (float)radius };
    b2ShapeId shapeId = b2CreateCircleShape(b->bodyId, &shapeDef, &circle);

    return js_b2_wrap_shape(ctx, b->world, shapeId);
}

static JSValue js_b2body_create_box_shape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1 || !JS_IsObject(argv[0]) || JS_IsNull(argv[0]))
        return JS_ThrowTypeError(ctx, "expected an object with { halfWidth, halfHeight, center?, angle?, density?, ... }");

    double hx = 0.0, hy = 0.0;
    double density = 1.0;
    double angle = 0.0;
    b2Vec2 center = {0.0f, 0.0f};
    bool hasOffset = false;

    JSValue hwVal = JS_GetPropertyStr(ctx, argv[0], "halfWidth");
    JSValue hhVal = JS_GetPropertyStr(ctx, argv[0], "halfHeight");
    if (JS_IsUndefined(hwVal) || JS_IsUndefined(hhVal)) {
        JS_FreeValue(ctx, hwVal);
        JS_FreeValue(ctx, hhVal);
        return JS_ThrowTypeError(ctx, "halfWidth and halfHeight are required");
    }
    JS_ToFloat64(ctx, &hx, hwVal);
    JS_ToFloat64(ctx, &hy, hhVal);
    JS_FreeValue(ctx, hwVal);
    JS_FreeValue(ctx, hhVal);

    if (!js_b2_validate_half_dimension(ctx, "halfWidth", hx)) return JS_EXCEPTION;
    if (!js_b2_validate_half_dimension(ctx, "halfHeight", hy)) return JS_EXCEPTION;

    JSValue centerVal = JS_GetPropertyStr(ctx, argv[0], "center");
    if (JS_IsObject(centerVal)) {
        center = js_b2_to_vec2(ctx, centerVal, center);
        hasOffset = true;
    }
    JS_FreeValue(ctx, centerVal);

    JSValue angleVal = JS_GetPropertyStr(ctx, argv[0], "angle");
    if (!JS_IsUndefined(angleVal)) {
        JS_ToFloat64(ctx, &angle, angleVal);
        hasOffset = true;
    }
    JS_FreeValue(ctx, angleVal);

    JSValue densityVal = JS_GetPropertyStr(ctx, argv[0], "density");
    if (!JS_IsUndefined(densityVal)) {
        JS_ToFloat64(ctx, &density, densityVal);
        if (!js_b2_validate_density(ctx, density)) {
            JS_FreeValue(ctx, densityVal);
            return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, densityVal);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = (float)density;
    if (!js_b2_apply_shape_options(ctx, argv[0], &shapeDef)) {
        return JS_EXCEPTION;
    }

    b2Polygon box = hasOffset
        ? b2MakeOffsetBox((float)hx, (float)hy, center, b2MakeRot((float)angle))
        : b2MakeBox((float)hx, (float)hy);
    b2ShapeId shapeId = b2CreatePolygonShape(b->bodyId, &shapeDef, &box);

    return js_b2_wrap_shape(ctx, b->world, shapeId);
}

static JSValue js_b2body_create_polygon_shape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1 || !JS_IsObject(argv[0]) || JS_IsNull(argv[0]))
        return JS_ThrowTypeError(ctx, "expected an object with { vertices, density?, radius?, ... }");

    double density = 1.0;
    double radius = 0.0;

    JSValue ownedVertices = JS_GetPropertyStr(ctx, argv[0], "vertices");
    if (!JS_IsArray(ctx, ownedVertices)) {
        JS_FreeValue(ctx, ownedVertices);
        return JS_ThrowTypeError(ctx, "vertices array is required");
    }

    JSValue densityVal = JS_GetPropertyStr(ctx, argv[0], "density");
    if (!JS_IsUndefined(densityVal)) {
        JS_ToFloat64(ctx, &density, densityVal);
        if (!js_b2_validate_density(ctx, density)) {
            JS_FreeValue(ctx, densityVal);
            JS_FreeValue(ctx, ownedVertices);
            return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, densityVal);

    JSValue radiusVal = JS_GetPropertyStr(ctx, argv[0], "radius");
    if (!JS_IsUndefined(radiusVal)) {
        JS_ToFloat64(ctx, &radius, radiusVal);
        if (radius < 0.0) {
            JS_ThrowRangeError(ctx, "radius must be >= 0 (got %f)", radius);
            JS_FreeValue(ctx, radiusVal);
            JS_FreeValue(ctx, ownedVertices);
            return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, radiusVal);

    JSValue lenVal = JS_GetPropertyStr(ctx, ownedVertices, "length");
    int32_t count = 0;
    JS_ToInt32(ctx, &count, lenVal);
    JS_FreeValue(ctx, lenVal);

    if (count < 3 || count > B2_MAX_POLYGON_VERTICES) {
        JS_FreeValue(ctx, ownedVertices);
        return JS_ThrowRangeError(ctx, "polygon must have between 3 and %d vertices", B2_MAX_POLYGON_VERTICES);
    }

    b2Vec2 points[B2_MAX_POLYGON_VERTICES];
    for (int32_t i = 0; i < count; i++) {
        JSValue el = JS_GetPropertyUint32(ctx, ownedVertices, i);
        bool ok = js_b2_to_vec2_strict(ctx, el, i, &points[i]);
        JS_FreeValue(ctx, el);
        if (!ok) {
            JS_FreeValue(ctx, ownedVertices);
            return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, ownedVertices);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = (float)density;
    if (!js_b2_apply_shape_options(ctx, argv[0], &shapeDef)) {
        return JS_EXCEPTION;
    }

    b2Hull hull = b2ComputeHull(points, count);
    if (hull.count == 0) {
        return JS_ThrowTypeError(ctx, "Invalid polygon vertices (degenerate or non-convex hull)");
    }
    b2Polygon polygon = b2MakePolygon(&hull, (float)radius);
    b2ShapeId shapeId = b2CreatePolygonShape(b->bodyId, &shapeDef, &polygon);

    return js_b2_wrap_shape(ctx, b->world, shapeId);
}

static JSValue js_b2body_create_capsule_shape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1 || !JS_IsObject(argv[0]) || JS_IsNull(argv[0]))
        return JS_ThrowTypeError(ctx, "expected an object with { point1, point2, radius, density?, ... }");

    b2Vec2 p1 = {0.0f, 0.0f}, p2 = {0.0f, 0.0f};
    double radius = 0.0;
    double density = 1.0;

    JSValue p1Val = JS_GetPropertyStr(ctx, argv[0], "point1");
    JSValue p2Val = JS_GetPropertyStr(ctx, argv[0], "point2");
    bool ok = js_b2_to_vec2_strict(ctx, p1Val, 0, &p1) &&
              js_b2_to_vec2_strict(ctx, p2Val, 1, &p2);
    JS_FreeValue(ctx, p1Val);
    JS_FreeValue(ctx, p2Val);

    JSValue radiusVal = JS_GetPropertyStr(ctx, argv[0], "radius");
    if (!ok) {
        JS_FreeValue(ctx, radiusVal);
        return JS_EXCEPTION;
    }
    if (JS_IsUndefined(radiusVal)) {
        JS_FreeValue(ctx, radiusVal);
        return JS_ThrowTypeError(ctx, "radius is required");
    }
    JS_ToFloat64(ctx, &radius, radiusVal);
    JS_FreeValue(ctx, radiusVal);

    if (!js_b2_validate_radius(ctx, radius)) return JS_EXCEPTION;

    JSValue densityVal = JS_GetPropertyStr(ctx, argv[0], "density");
    if (!JS_IsUndefined(densityVal)) {
        JS_ToFloat64(ctx, &density, densityVal);
        if (!js_b2_validate_density(ctx, density)) {
            JS_FreeValue(ctx, densityVal);
            return JS_EXCEPTION;
        }
    }
    JS_FreeValue(ctx, densityVal);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = (float)density;
    if (!js_b2_apply_shape_options(ctx, argv[0], &shapeDef)) {
        return JS_EXCEPTION;
    }

    b2Capsule capsule = { .center1 = p1, .center2 = p2, .radius = (float)radius };
    b2ShapeId shapeId = b2CreateCapsuleShape(b->bodyId, &shapeDef, &capsule);

    return js_b2_wrap_shape(ctx, b->world, shapeId);
}

static JSValue js_b2body_create_segment_shape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1 || !JS_IsObject(argv[0]) || JS_IsNull(argv[0]))
        return JS_ThrowTypeError(ctx, "expected an object with { point1, point2, ... }");

    b2Vec2 p1 = {0.0f, 0.0f}, p2 = {0.0f, 0.0f};

    JSValue p1Val = JS_GetPropertyStr(ctx, argv[0], "point1");
    JSValue p2Val = JS_GetPropertyStr(ctx, argv[0], "point2");
    bool ok = js_b2_to_vec2_strict(ctx, p1Val, 0, &p1) &&
              js_b2_to_vec2_strict(ctx, p2Val, 1, &p2);
    JS_FreeValue(ctx, p1Val);
    JS_FreeValue(ctx, p2Val);
    if (!ok) return JS_EXCEPTION;

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    js_b2_apply_shape_options(ctx, argv[0], &shapeDef);

    b2Segment segment = { .point1 = p1, .point2 = p2 };
    b2ShapeId shapeId = b2CreateSegmentShape(b->bodyId, &shapeDef, &segment);

    return js_b2_wrap_shape(ctx, b->world, shapeId);
}

static JSValue js_b2body_destroy(
    JSContext *ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst *argv
) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(
        ctx,
        this_val,
        js_b2body_class_id
    );

    if (!b || !b2Body_IsValid(b->bodyId)) {
        return JS_FALSE;
    }

    b2ShapeId *shapes = NULL;
    b2JointId *joints = NULL;

    const int shapeCount = b2Body_GetShapeCount(b->bodyId);
    const int jointCount = b2Body_GetJointCount(b->bodyId);

    if (shapeCount > 0) {
        shapes = malloc(sizeof(*shapes) * (size_t)shapeCount);
        if (!shapes) {
            return JS_ThrowOutOfMemory(ctx);
        }
    }

    if (jointCount > 0) {
        joints = malloc(sizeof(*joints) * (size_t)jointCount);
        if (!joints) {
            free(shapes);
            return JS_ThrowOutOfMemory(ctx);
        }
    }

    const int shapeCountActual =
        shapeCount > 0
            ? b2Body_GetShapes(b->bodyId, shapes, shapeCount)
            : 0;

    const int jointCountActual =
        jointCount > 0
            ? b2Body_GetJoints(b->bodyId, joints, jointCount)
            : 0;

    for (int i = 0; i < shapeCountActual; ++i) {
        const uint64_t shapeCacheId = b2StoreShapeId(shapes[i]);
        void *userData = b2Shape_GetUserData(shapes[i]);

        if (userData) {
            js_b2_free_userdata_box(userData);
        }

        js_b2_wrapper_cache_remove(
            b->world,
            JSB2_WRAPPER_SHAPE,
            shapeCacheId
        );
    }

    for (int i = 0; i < jointCountActual; ++i) {
        const uint64_t jointCacheId = b2StoreJointId(joints[i]);
        void *userData = b2Joint_GetUserData(joints[i]);

        if (userData) {
            js_b2_free_userdata_box(userData);
        }

        js_b2_wrapper_cache_remove(
            b->world,
            JSB2_WRAPPER_JOINT,
            jointCacheId
        );
    }

    void *userData = b2Body_GetUserData(b->bodyId);
    if (userData) {
        js_b2_free_userdata_box(userData);
    }

    const uint64_t bodyCacheId = b2StoreBodyId(b->bodyId);

    b2DestroyBody(b->bodyId);

    b->bodyId = (b2BodyId){0};

    js_b2_wrapper_cache_remove(
        b->world,
        JSB2_WRAPPER_BODY,
        bodyCacheId
    );

    free(shapes);
    free(joints);

    return JS_TRUE;
}

static JSValue js_b2body_get_user_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    return js_b2_get_userdata_box(ctx, b2Body_GetUserData(b->bodyId));
}

static JSValue js_b2body_set_user_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "value required");

    void *old = b2Body_GetUserData(b->bodyId);
    if (old) js_b2_free_userdata_box(old);
    b2Body_SetUserData(b->bodyId, js_b2_box_userdata(ctx, b->world, argv[0]));
    return JS_UNDEFINED;
}

static JSValue js_b2body_set_target_transform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 4) return JS_ThrowSyntaxError(ctx, "x, y, angle and duration required");

    double x, y, angle, duration;
    if (!js_b2_to_float64_strict(ctx, argv[0], &x)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &y)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[2], &angle)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[3], &duration)) return JS_EXCEPTION;

    b2WorldTransform target;
    target.p = (b2Vec2){(float)x, (float)y};
    target.q = b2MakeRot((float)angle);

    b2Body_SetTargetTransform(
        b->bodyId,
        target,
        (float)duration,
        true
    );
    
    return JS_UNDEFINED;
}

static JSValue js_b2body_get_world_point(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "x and y required");

    double x, y;
    if (!js_b2_to_float64_strict(ctx, argv[0], &x)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &y)) return JS_EXCEPTION;
    return js_b2_from_vec2(ctx, b2Body_GetWorldPoint(b->bodyId, (b2Vec2){(float)x, (float)y}));
}

static JSValue js_b2body_get_local_point(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "x and y required");

    double x, y;
    if (!js_b2_to_float64_strict(ctx, argv[0], &x)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &y)) return JS_EXCEPTION;
    return js_b2_from_vec2(ctx, b2Body_GetLocalPoint(b->bodyId, (b2Vec2){(float)x, (float)y}));
}

static JSValue js_b2body_get_world_center(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    return js_b2_from_vec2(ctx, b2Body_GetWorldCenter(b->bodyId));
}

static JSValue js_b2body_apply_mass_from_shapes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");
    b2Body_ApplyMassFromShapes(b->bodyId);
    return JS_UNDEFINED;
}

static JSValue js_b2body_compute_aabb(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");

    b2AABB aabb = b2Body_ComputeAABB(b->bodyId);
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "lowerX", JS_NewFloat64(ctx, aabb.lowerBound.x));
    JS_SetPropertyStr(ctx, obj, "lowerY", JS_NewFloat64(ctx, aabb.lowerBound.y));
    JS_SetPropertyStr(ctx, obj, "upperX", JS_NewFloat64(ctx, aabb.upperBound.x));
    JS_SetPropertyStr(ctx, obj, "upperY", JS_NewFloat64(ctx, aabb.upperBound.y));
    return obj;
}

static JSValue js_b2body_get_shapes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");

    int count = b2Body_GetShapeCount(b->bodyId);
    JSValue arr = JS_NewArray(ctx);
    if (count <= 0) return arr;

    b2ShapeId *shapes = (b2ShapeId *)malloc(sizeof(b2ShapeId) * count);
    if (!shapes) return JS_ThrowOutOfMemory(ctx);
    int n = b2Body_GetShapes(b->bodyId, shapes, count);
    for (int i = 0; i < n; i++) {
        JS_SetPropertyUint32(ctx, arr, i, js_b2_wrap_shape(ctx, b->world, shapes[i]));
    }
    free(shapes);
    return arr;
}

static JSValue js_b2body_get_joints(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, this_val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Body");

    int count = b2Body_GetJointCount(b->bodyId);
    JSValue arr = JS_NewArray(ctx);
    if (count <= 0) return arr;

    b2JointId *joints = (b2JointId *)malloc(sizeof(b2JointId) * count);
    if (!joints) return JS_ThrowOutOfMemory(ctx);

    int n = b2Body_GetJoints(b->bodyId, joints, count);
    for (int i = 0; i < n; i++) {
        JS_SetPropertyUint32(ctx, arr, i, js_b2_wrap_joint(ctx, b->world, joints[i]));
    }
    free(joints);
    return arr;
}

static const JSCFunctionListEntry js_b2body_proto_funcs[] = {
    JS_CFUNC_DEF("getType", 0, js_b2body_get_type),
    JS_CFUNC_DEF("setType", 1, js_b2body_set_type),
    JS_CFUNC_DEF("getPosition", 0, js_b2body_get_position),
    JS_CFUNC_DEF("setPosition", 2, js_b2body_set_position),
    JS_CFUNC_DEF("getAngle", 0, js_b2body_get_angle),
    JS_CFUNC_DEF("setTransform", 3, js_b2body_set_transform),
    JS_CFUNC_DEF("getLinearVelocity", 0, js_b2body_get_linear_velocity),
    JS_CFUNC_DEF("setLinearVelocity", 2, js_b2body_set_linear_velocity),
    JS_CFUNC_DEF("getAngularVelocity", 0, js_b2body_get_angular_velocity),
    JS_CFUNC_DEF("setAngularVelocity", 1, js_b2body_set_angular_velocity),
    JS_CFUNC_DEF("applyForce", 4, js_b2body_apply_force),
    JS_CFUNC_DEF("applyForceToCenter", 2, js_b2body_apply_force_to_center),
    JS_CFUNC_DEF("applyTorque", 1, js_b2body_apply_torque),
    JS_CFUNC_DEF("applyLinearImpulse", 4, js_b2body_apply_linear_impulse),
    JS_CFUNC_DEF("applyLinearImpulseToCenter", 2, js_b2body_apply_linear_impulse_to_center),
    JS_CFUNC_DEF("applyAngularImpulse", 1, js_b2body_apply_angular_impulse),
    JS_CFUNC_DEF("getMass", 0, js_b2body_get_mass),
    JS_CFUNC_DEF("getLinearDamping", 0, js_b2body_get_linear_damping),
    JS_CFUNC_DEF("setLinearDamping", 1, js_b2body_set_linear_damping),
    JS_CFUNC_DEF("getAngularDamping", 0, js_b2body_get_angular_damping),
    JS_CFUNC_DEF("setAngularDamping", 1, js_b2body_set_angular_damping),
    JS_CFUNC_DEF("getGravityScale", 0, js_b2body_get_gravity_scale),
    JS_CFUNC_DEF("setGravityScale", 1, js_b2body_set_gravity_scale),
    JS_CFUNC_DEF("isAwake", 0, js_b2body_is_awake),
    JS_CFUNC_DEF("setAwake", 1, js_b2body_set_awake),
    JS_CFUNC_DEF("isEnabled", 0, js_b2body_is_enabled),
    JS_CFUNC_DEF("setEnabled", 1, js_b2body_set_enabled),
    JS_CFUNC_DEF("isFixedRotation", 0, js_b2body_is_fixed_rotation),
    JS_CFUNC_DEF("setFixedRotation", 1, js_b2body_set_fixed_rotation),
    JS_CFUNC_DEF("setTargetTransform", 4, js_b2body_set_target_transform),
    JS_CFUNC_DEF("createCircleShape", 1, js_b2body_create_circle_shape),
    JS_CFUNC_DEF("createBoxShape", 2, js_b2body_create_box_shape),
    JS_CFUNC_DEF("createPolygonShape", 1, js_b2body_create_polygon_shape),
    JS_CFUNC_DEF("createCapsuleShape", 5, js_b2body_create_capsule_shape),
    JS_CFUNC_DEF("createSegmentShape", 4, js_b2body_create_segment_shape),
    JS_CFUNC_DEF("createChain", 2, js_b2body_create_chain),
    JS_CFUNC_DEF("getShapes", 0, js_b2body_get_shapes),
    JS_CFUNC_DEF("getJoints", 0, js_b2body_get_joints),
    JS_CFUNC_DEF("getUserData", 0, js_b2body_get_user_data),
    JS_CFUNC_DEF("setUserData", 1, js_b2body_set_user_data),
    JS_CFUNC_DEF("getWorldPoint", 2, js_b2body_get_world_point),
    JS_CFUNC_DEF("getLocalPoint", 2, js_b2body_get_local_point),
    JS_CFUNC_DEF("getWorldCenter", 0, js_b2body_get_world_center),
    JS_CFUNC_DEF("applyMassFromShapes", 0, js_b2body_apply_mass_from_shapes),
    JS_CFUNC_DEF("computeAABB", 0, js_b2body_compute_aabb),
    JS_CFUNC_DEF("destroy", 0, js_b2body_destroy),
    JS_CFUNC_DEF("isValid", 0, js_b2body_is_valid),
};


void js_b2body_register(JSContext *ctx) {
    JS_NewClassID(&js_b2body_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_b2body_class_id, &js_b2body_class);
    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_b2body_proto_funcs, countof(js_b2body_proto_funcs));
    JS_SetClassProto(ctx, js_b2body_class_id, proto);
}

#endif