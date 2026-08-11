#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>

#ifdef ATHENA_BOX2D
#include <box2d/box2d.h>
#include "ath_box2d.h"

static void js_b2world_free_all_userdata(JSB2World *w) {
    JSB2UserDataBox *box = (JSB2UserDataBox *)w->userDataList;
    while (box) {
        JSB2UserDataBox *next = box->next;
        JS_FreeValueRT(box->rt, box->value);
        free(box);
        box = next;
    }
    w->userDataList = NULL;
}

static void js_b2world_finalizer(JSRuntime *rt, JSValue val) {
    JSB2World *w = (JSB2World *)JS_GetOpaque(val, js_b2world_class_id);
    if (w) {
        if (b2World_IsValid(w->worldId)) {
            js_b2world_free_all_userdata(w);
            js_b2_wrapper_cache_remove_world(w);
            b2DestroyWorld(w->worldId);
            w->worldId = (b2WorldId){0};
        }
        free(w);
    }
}

static JSClassDef js_b2world_class = {
    "B2World",
    .finalizer = js_b2world_finalizer,
};

static JSValue js_b2world_step(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");

    double timeStep = 1.0 / 60.0;
    int32_t subStepCount = 4;

    if (argc >= 1) {
        if (!js_b2_to_float64_strict(ctx, argv[0], &timeStep)) return JS_EXCEPTION;
    }
    if (argc >= 2) {
        if (!js_b2_to_int32_strict(ctx, argv[1], &subStepCount)) return JS_EXCEPTION;
    }

    b2World_Step(w->worldId, (float)timeStep, subStepCount);
    return JS_UNDEFINED;
}

static JSValue js_b2world_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_FALSE;

    js_b2world_free_all_userdata(w);
    js_b2_wrapper_cache_remove_world(w);
    b2DestroyWorld(w->worldId);
    w->worldId = (b2WorldId){0};
    return JS_TRUE;
}

static JSValue js_b2world_is_valid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque(this_val, js_b2world_class_id);
    if (!w) return JS_FALSE;
    return JS_NewBool(ctx, b2World_IsValid(w->worldId));
}

static JSValue js_b2world_create_body(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");

    b2BodyDef bodyDef = b2DefaultBodyDef();

    if (argc >= 1 && JS_IsObject(argv[0])) {
        JSValue val;

        val = JS_GetPropertyStr(ctx, argv[0], "type");
        if (!JS_IsUndefined(val)) {
            int32_t type = b2_staticBody;
            JS_ToInt32(ctx, &type, val);
            bodyDef.type = (b2BodyType)type;
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "position");
        if (JS_IsObject(val)) {
            JSValue vx = JS_GetPropertyStr(ctx, val, "x");
            JSValue vy = JS_GetPropertyStr(ctx, val, "y");
            double x = 0.0, y = 0.0;
            if (!JS_IsUndefined(vx)) JS_ToFloat64(ctx, &x, vx);
            if (!JS_IsUndefined(vy)) JS_ToFloat64(ctx, &y, vy);
            bodyDef.position = (b2Vec2){(float)x, (float)y};
            JS_FreeValue(ctx, vx);
            JS_FreeValue(ctx, vy);
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "angle");
        if (JS_IsUndefined(val)) {
            val = JS_GetPropertyStr(ctx, argv[0], "rotation");
        }
        if (!JS_IsUndefined(val)) {
            double angle = 0.0;
            JS_ToFloat64(ctx, &angle, val);
            bodyDef.rotation = b2MakeRot((float)angle);
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "linearVelocity");
        if (JS_IsObject(val)) {
            JSValue vx = JS_GetPropertyStr(ctx, val, "x");
            JSValue vy = JS_GetPropertyStr(ctx, val, "y");
            double x = 0.0, y = 0.0;
            if (!JS_IsUndefined(vx)) JS_ToFloat64(ctx, &x, vx);
            if (!JS_IsUndefined(vy)) JS_ToFloat64(ctx, &y, vy);
            bodyDef.linearVelocity = (b2Vec2){(float)x, (float)y};
            JS_FreeValue(ctx, vx);
            JS_FreeValue(ctx, vy);
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "angularVelocity");
        if (!JS_IsUndefined(val)) {
            double w_val = 0.0;
            JS_ToFloat64(ctx, &w_val, val);
            bodyDef.angularVelocity = (float)w_val;
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "linearDamping");
        if (!JS_IsUndefined(val)) {
            double ld = 0.0;
            JS_ToFloat64(ctx, &ld, val);
            bodyDef.linearDamping = (float)ld;
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "angularDamping");
        if (!JS_IsUndefined(val)) {
            double ad = 0.0;
            JS_ToFloat64(ctx, &ad, val);
            bodyDef.angularDamping = (float)ad;
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "gravityScale");
        if (!JS_IsUndefined(val)) {
            double gs = 1.0;
            JS_ToFloat64(ctx, &gs, val);
            bodyDef.gravityScale = (float)gs;
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "fixedRotation");
        if (!JS_IsUndefined(val)) {
            bodyDef.motionLocks.angularZ = JS_ToBool(ctx, val);
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "isBullet");
        if (!JS_IsUndefined(val)) {
            bodyDef.isBullet = JS_ToBool(ctx, val);
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "enableSleep");
        if (!JS_IsUndefined(val)) {
            bodyDef.enableSleep = JS_ToBool(ctx, val);
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "isAwake");
        if (!JS_IsUndefined(val)) {
            bodyDef.isAwake = JS_ToBool(ctx, val);
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "isEnabled");
        if (!JS_IsUndefined(val)) {
            bodyDef.isEnabled = JS_ToBool(ctx, val);
            JS_FreeValue(ctx, val);
        }
    }

    b2BodyId bodyId = b2CreateBody(w->worldId, &bodyDef);
    return js_b2_wrap_body(ctx, w, bodyId);
}

static JSValue js_b2world_create_distance_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_create_revolute_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_create_prismatic_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_create_weld_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_create_wheel_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_create_motor_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_create_filter_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_get_gravity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_set_gravity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_enable_continuous(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_is_continuous_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_set_restitution_threshold(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_get_restitution_threshold(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_set_maximum_linear_speed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_b2world_get_maximum_linear_speed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

static const JSCFunctionListEntry js_b2world_proto_funcs[] = {
    JS_CFUNC_DEF("step", 2, js_b2world_step),
    JS_CFUNC_DEF("createBody", 1, js_b2world_create_body),
    JS_CFUNC_DEF("createDistanceJoint", 3, js_b2world_create_distance_joint),
    JS_CFUNC_DEF("createRevoluteJoint", 3, js_b2world_create_revolute_joint),
    JS_CFUNC_DEF("createPrismaticJoint", 3, js_b2world_create_prismatic_joint),
    JS_CFUNC_DEF("createWeldJoint", 3, js_b2world_create_weld_joint),
    JS_CFUNC_DEF("createWheelJoint", 3, js_b2world_create_wheel_joint),
    JS_CFUNC_DEF("createMotorJoint", 3, js_b2world_create_motor_joint),
    JS_CFUNC_DEF("createFilterJoint", 2, js_b2world_create_filter_joint),
    JS_CFUNC_DEF("getGravity", 0, js_b2world_get_gravity),
    JS_CFUNC_DEF("setGravity", 2, js_b2world_set_gravity),
    JS_CFUNC_DEF("enableContinuous", 1, js_b2world_enable_continuous),
    JS_CFUNC_DEF("isContinuousEnabled", 0, js_b2world_is_continuous_enabled),
    JS_CFUNC_DEF("setRestitutionThreshold", 1, js_b2world_set_restitution_threshold),
    JS_CFUNC_DEF("getRestitutionThreshold", 0, js_b2world_get_restitution_threshold),
    JS_CFUNC_DEF("setMaximumLinearSpeed", 1, js_b2world_set_maximum_linear_speed),
    JS_CFUNC_DEF("getMaximumLinearSpeed", 0, js_b2world_get_maximum_linear_speed),
    JS_CFUNC_DEF("castRay", 5, js_b2world_cast_ray),
    JS_CFUNC_DEF("raycastAll", 5, js_b2world_raycast_all),
    JS_CFUNC_DEF("queryAABB", 5, js_b2world_query_aabb),
    JS_CFUNC_DEF("overlapShape", 3, js_b2world_overlap_shape),
    JS_CFUNC_DEF("overlapCircle", 4, js_b2world_overlap_circle),
    JS_CFUNC_DEF("overlapCapsule", 6, js_b2world_overlap_capsule),
    JS_CFUNC_DEF("overlapPolygon", 3, js_b2world_overlap_polygon),
    JS_CFUNC_DEF("castShape", 5, js_b2world_cast_shape),
    JS_CFUNC_DEF("castCircle", 6, js_b2world_cast_circle),
    JS_CFUNC_DEF("castCapsule", 8, js_b2world_cast_capsule),
    JS_CFUNC_DEF("castPolygon", 5, js_b2world_cast_polygon),
    JS_CFUNC_DEF("castMover", 9, js_b2world_cast_mover),
    JS_CFUNC_DEF("collideMover", 8, js_b2world_collide_mover),
    JS_CFUNC_DEF("getContactEvents", 0, js_b2world_get_contact_events),
    JS_CFUNC_DEF("getSensorEvents", 0, js_b2world_get_sensor_events),
    JS_CFUNC_DEF("getBodyEvents", 0, js_b2world_get_body_events),
    JS_CFUNC_DEF("explode", 6, js_b2world_explode),
    JS_CFUNC_DEF("destroy", 0, js_b2world_destroy),
    JS_CFUNC_DEF("isValid", 0, js_b2world_is_valid),
};

static int js_b2_require_body(JSContext *ctx, JSValueConst val, b2BodyId *out) {
    JSB2Body *b = (JSB2Body *)JS_GetOpaque2(ctx, val, js_b2body_class_id);
    if (!b || !b2Body_IsValid(b->bodyId)) return 0;
    *out = b->bodyId;
    return 1;
}

static void js_b2_apply_joint_base_options(JSContext *ctx, JSB2World *world, JSValueConst opts, b2JointDef *base) {
    if (!JS_IsObject(opts)) return;
    JSValue val;

    val = JS_GetPropertyStr(ctx, opts, "collideConnected");
    if (!JS_IsUndefined(val)) {
        base->collideConnected = JS_ToBool(ctx, val);
        JS_FreeValue(ctx, val);
    }

    val = JS_GetPropertyStr(ctx, opts, "userData");
    if (!JS_IsUndefined(val)) {
        base->userData = js_b2_box_userdata(ctx, world, val);
        JS_FreeValue(ctx, val);
    }
}

static JSValue js_b2world_create_distance_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "bodyA and bodyB required");

    b2BodyId bodyA, bodyB;
    if (!js_b2_require_body(ctx, argv[0], &bodyA) || !js_b2_require_body(ctx, argv[1], &bodyB))
        return JS_ThrowTypeError(ctx, "bodyA and bodyB must be valid Box2D bodies");

    b2DistanceJointDef def = b2DefaultDistanceJointDef();
    def.base.bodyIdA = bodyA;
    def.base.bodyIdB = bodyB;

    JSValueConst opts = argc >= 3 ? argv[2] : JS_UNDEFINED;
    if (JS_IsObject(opts)) {
        JSValue anchor = JS_GetPropertyStr(ctx, opts, "anchor");
        JSValue anchorA = JS_GetPropertyStr(ctx, opts, "anchorA");
        JSValue anchorB = JS_GetPropertyStr(ctx, opts, "anchorB");
        if (JS_IsObject(anchor)) {
            b2Vec2 worldAnchor = js_b2_to_vec2(ctx, anchor, (b2Vec2){0, 0});
            def.base.localFrameA.p = b2Body_GetLocalPoint(bodyA, worldAnchor);
            def.base.localFrameB.p = b2Body_GetLocalPoint(bodyB, worldAnchor);
        } else {
            if (JS_IsObject(anchorA)) def.base.localFrameA.p = js_b2_to_vec2(ctx, anchorA, (b2Vec2){0, 0});
            if (JS_IsObject(anchorB)) def.base.localFrameB.p = js_b2_to_vec2(ctx, anchorB, (b2Vec2){0, 0});
        }
        JS_FreeValue(ctx, anchor);
        JS_FreeValue(ctx, anchorA);
        JS_FreeValue(ctx, anchorB);

        JSValue val;
        val = JS_GetPropertyStr(ctx, opts, "length");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.length = (float)d; } else {
            def.length = b2Distance(b2Body_GetPosition(bodyA), b2Body_GetPosition(bodyB));
        }
        JS_FreeValue(ctx, val);

        val = JS_GetPropertyStr(ctx, opts, "enableSpring");
        if (!JS_IsUndefined(val)) { def.enableSpring = JS_ToBool(ctx, val); } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "hertz");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.hertz = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "dampingRatio");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.dampingRatio = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "enableLimit");
        if (!JS_IsUndefined(val)) { def.enableLimit = JS_ToBool(ctx, val); } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "minLength");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.minLength = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "maxLength");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.maxLength = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "enableMotor");
        if (!JS_IsUndefined(val)) { def.enableMotor = JS_ToBool(ctx, val); } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "maxMotorForce");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.maxMotorForce = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "motorSpeed");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.motorSpeed = (float)d; } JS_FreeValue(ctx, val);
    } else {
        def.length = b2Distance(b2Body_GetPosition(bodyA), b2Body_GetPosition(bodyB));
    }
    js_b2_apply_joint_base_options(ctx, w, opts, &def.base);

    return js_b2_wrap_joint(ctx, w, b2CreateDistanceJoint(w->worldId, &def));
}

static JSValue js_b2world_create_revolute_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "bodyA and bodyB required");

    b2BodyId bodyA, bodyB;
    if (!js_b2_require_body(ctx, argv[0], &bodyA) || !js_b2_require_body(ctx, argv[1], &bodyB))
        return JS_ThrowTypeError(ctx, "bodyA and bodyB must be valid Box2D bodies");

    b2RevoluteJointDef def = b2DefaultRevoluteJointDef();
    def.base.bodyIdA = bodyA;
    def.base.bodyIdB = bodyB;

    JSValueConst opts = argc >= 3 ? argv[2] : JS_UNDEFINED;
    if (JS_IsObject(opts)) {
        JSValue anchor = JS_GetPropertyStr(ctx, opts, "anchor");
        if (JS_IsObject(anchor)) {
            b2Vec2 worldAnchor = js_b2_to_vec2(ctx, anchor, (b2Vec2){0, 0});
            def.base.localFrameA.p = b2Body_GetLocalPoint(bodyA, worldAnchor);
            def.base.localFrameB.p = b2Body_GetLocalPoint(bodyB, worldAnchor);
        }
        JS_FreeValue(ctx, anchor);

        JSValue val;
        val = JS_GetPropertyStr(ctx, opts, "enableSpring");
        if (!JS_IsUndefined(val)) { def.enableSpring = JS_ToBool(ctx, val); } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "hertz");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.hertz = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "dampingRatio");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.dampingRatio = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "enableLimit");
        if (!JS_IsUndefined(val)) { def.enableLimit = JS_ToBool(ctx, val); } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "lowerAngle");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.lowerAngle = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "upperAngle");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.upperAngle = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "enableMotor");
        if (!JS_IsUndefined(val)) { def.enableMotor = JS_ToBool(ctx, val); } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "motorSpeed");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.motorSpeed = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "maxMotorTorque");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.maxMotorTorque = (float)d; } JS_FreeValue(ctx, val);
    }
    js_b2_apply_joint_base_options(ctx, w, opts, &def.base);

    return js_b2_wrap_joint(ctx, w, b2CreateRevoluteJoint(w->worldId, &def));
}

static JSValue js_b2world_create_prismatic_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "bodyA and bodyB required");

    b2BodyId bodyA, bodyB;
    if (!js_b2_require_body(ctx, argv[0], &bodyA) || !js_b2_require_body(ctx, argv[1], &bodyB))
        return JS_ThrowTypeError(ctx, "bodyA and bodyB must be valid Box2D bodies");

    b2PrismaticJointDef def = b2DefaultPrismaticJointDef();
    def.base.bodyIdA = bodyA;
    def.base.bodyIdB = bodyB;

    JSValueConst opts = argc >= 3 ? argv[2] : JS_UNDEFINED;
    if (JS_IsObject(opts)) {
        JSValue anchor = JS_GetPropertyStr(ctx, opts, "anchor");
        if (JS_IsObject(anchor)) {
            b2Vec2 worldAnchor = js_b2_to_vec2(ctx, anchor, (b2Vec2){0, 0});
            def.base.localFrameA.p = b2Body_GetLocalPoint(bodyA, worldAnchor);
            def.base.localFrameB.p = b2Body_GetLocalPoint(bodyB, worldAnchor);
        }
        JS_FreeValue(ctx, anchor);

        JSValue axis = JS_GetPropertyStr(ctx, opts, "axis");
        if (JS_IsObject(axis)) {
            b2Vec2 a = js_b2_to_vec2(ctx, axis, (b2Vec2){1.0f, 0.0f});
            def.base.localFrameA.q = b2MakeRotFromUnitVector(b2Normalize(a));
        }
        JS_FreeValue(ctx, axis);

        JSValue val;
        val = JS_GetPropertyStr(ctx, opts, "enableSpring");
        if (!JS_IsUndefined(val)) { def.enableSpring = JS_ToBool(ctx, val); } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "hertz");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.hertz = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "dampingRatio");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.dampingRatio = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "enableLimit");
        if (!JS_IsUndefined(val)) { def.enableLimit = JS_ToBool(ctx, val); } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "lowerTranslation");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.lowerTranslation = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "upperTranslation");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.upperTranslation = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "enableMotor");
        if (!JS_IsUndefined(val)) { def.enableMotor = JS_ToBool(ctx, val); } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "motorSpeed");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.motorSpeed = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "maxMotorForce");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.maxMotorForce = (float)d; } JS_FreeValue(ctx, val);
    }
    js_b2_apply_joint_base_options(ctx, w, opts, &def.base);

    return js_b2_wrap_joint(ctx, w, b2CreatePrismaticJoint(w->worldId, &def));
}

static JSValue js_b2world_create_weld_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "bodyA and bodyB required");

    b2BodyId bodyA, bodyB;
    if (!js_b2_require_body(ctx, argv[0], &bodyA) || !js_b2_require_body(ctx, argv[1], &bodyB))
        return JS_ThrowTypeError(ctx, "bodyA and bodyB must be valid Box2D bodies");

    b2WeldJointDef def = b2DefaultWeldJointDef();
    def.base.bodyIdA = bodyA;
    def.base.bodyIdB = bodyB;
    def.base.localFrameA.q = b2InvMulRot(b2Body_GetRotation(bodyA), b2Body_GetRotation(bodyB));

    JSValueConst opts = argc >= 3 ? argv[2] : JS_UNDEFINED;
    if (JS_IsObject(opts)) {
        JSValue anchor = JS_GetPropertyStr(ctx, opts, "anchor");
        if (JS_IsObject(anchor)) {
            b2Vec2 worldAnchor = js_b2_to_vec2(ctx, anchor, (b2Vec2){0, 0});
            def.base.localFrameA.p = b2Body_GetLocalPoint(bodyA, worldAnchor);
            def.base.localFrameB.p = b2Body_GetLocalPoint(bodyB, worldAnchor);
        }
        JS_FreeValue(ctx, anchor);

        JSValue val;
        val = JS_GetPropertyStr(ctx, opts, "linearHertz");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.linearHertz = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "angularHertz");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.angularHertz = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "linearDampingRatio");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.linearDampingRatio = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "angularDampingRatio");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.angularDampingRatio = (float)d; } JS_FreeValue(ctx, val);
    }
    js_b2_apply_joint_base_options(ctx, w, opts, &def.base);

    return js_b2_wrap_joint(ctx, w, b2CreateWeldJoint(w->worldId, &def));
}

static JSValue js_b2world_create_wheel_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "bodyA and bodyB required");

    b2BodyId bodyA, bodyB;
    if (!js_b2_require_body(ctx, argv[0], &bodyA) || !js_b2_require_body(ctx, argv[1], &bodyB))
        return JS_ThrowTypeError(ctx, "bodyA and bodyB must be valid Box2D bodies");

    b2WheelJointDef def = b2DefaultWheelJointDef();
    def.base.bodyIdA = bodyA;
    def.base.bodyIdB = bodyB;
    def.base.localFrameA.q = b2MakeRot(0.5f * B2_PI);

    JSValueConst opts = argc >= 3 ? argv[2] : JS_UNDEFINED;
    if (JS_IsObject(opts)) {
        JSValue anchor = JS_GetPropertyStr(ctx, opts, "anchor");
        if (JS_IsObject(anchor)) {
            b2Vec2 worldAnchor = js_b2_to_vec2(ctx, anchor, (b2Vec2){0, 0});
            def.base.localFrameA.p = b2Body_GetLocalPoint(bodyA, worldAnchor);
            def.base.localFrameB.p = b2Body_GetLocalPoint(bodyB, worldAnchor);
        }
        JS_FreeValue(ctx, anchor);

        JSValue axis = JS_GetPropertyStr(ctx, opts, "axis");
        if (JS_IsObject(axis)) {
            b2Vec2 a = js_b2_to_vec2(ctx, axis, (b2Vec2){0.0f, 1.0f});
            def.base.localFrameA.q = b2MakeRotFromUnitVector(b2Normalize(a));
        }
        JS_FreeValue(ctx, axis);

        JSValue val;
        val = JS_GetPropertyStr(ctx, opts, "enableSpring");
        if (!JS_IsUndefined(val)) { def.enableSpring = JS_ToBool(ctx, val); } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "hertz");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.hertz = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "dampingRatio");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.dampingRatio = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "enableLimit");
        if (!JS_IsUndefined(val)) { def.enableLimit = JS_ToBool(ctx, val); } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "lowerTranslation");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.lowerTranslation = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "upperTranslation");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.upperTranslation = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "enableMotor");
        if (!JS_IsUndefined(val)) { def.enableMotor = JS_ToBool(ctx, val); } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "motorSpeed");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.motorSpeed = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "maxMotorTorque");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.maxMotorTorque = (float)d; } JS_FreeValue(ctx, val);
    }
    js_b2_apply_joint_base_options(ctx, w, opts, &def.base);

    return js_b2_wrap_joint(ctx, w, b2CreateWheelJoint(w->worldId, &def));
}

static JSValue js_b2world_create_motor_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "bodyA and bodyB required");

    b2BodyId bodyA, bodyB;
    if (!js_b2_require_body(ctx, argv[0], &bodyA) || !js_b2_require_body(ctx, argv[1], &bodyB))
        return JS_ThrowTypeError(ctx, "bodyA and bodyB must be valid Box2D bodies");

    b2MotorJointDef def = b2DefaultMotorJointDef();
    def.base.bodyIdA = bodyA;
    def.base.bodyIdB = bodyB;

    JSValueConst opts = argc >= 3 ? argv[2] : JS_UNDEFINED;
    if (JS_IsObject(opts)) {
        JSValue val;
        val = JS_GetPropertyStr(ctx, opts, "linearVelocity");
        if (JS_IsObject(val)) def.linearVelocity = js_b2_to_vec2(ctx, val, (b2Vec2){0, 0});
        JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "angularVelocity");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.angularVelocity = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "maxVelocityForce");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.maxVelocityForce = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "maxVelocityTorque");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.maxVelocityTorque = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "linearHertz");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.linearHertz = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "linearDampingRatio");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.linearDampingRatio = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "maxSpringForce");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.maxSpringForce = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "angularHertz");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.angularHertz = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "angularDampingRatio");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.angularDampingRatio = (float)d; } JS_FreeValue(ctx, val);
        val = JS_GetPropertyStr(ctx, opts, "maxSpringTorque");
        if (!JS_IsUndefined(val)) { double d; JS_ToFloat64(ctx, &d, val); def.maxSpringTorque = (float)d; } JS_FreeValue(ctx, val);
    }
    js_b2_apply_joint_base_options(ctx, w, opts, &def.base);

    return js_b2_wrap_joint(ctx, w, b2CreateMotorJoint(w->worldId, &def));
}

static JSValue js_b2world_create_filter_joint(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "bodyA and bodyB required");

    b2BodyId bodyA, bodyB;
    if (!js_b2_require_body(ctx, argv[0], &bodyA) || !js_b2_require_body(ctx, argv[1], &bodyB))
        return JS_ThrowTypeError(ctx, "bodyA and bodyB must be valid Box2D bodies");

    b2FilterJointDef def = b2DefaultFilterJointDef();
    def.base.bodyIdA = bodyA;
    def.base.bodyIdB = bodyB;

    JSValueConst opts = argc >= 3 ? argv[2] : JS_UNDEFINED;
    if (JS_IsObject(opts)) {
        js_b2_apply_joint_base_options(ctx, w, opts, &def.base);
    }

    return js_b2_wrap_joint(ctx, w, b2CreateFilterJoint(w->worldId, &def));
}


static JSValue js_b2world_get_gravity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    return js_b2_from_vec2(ctx, b2World_GetGravity(w->worldId));
}

static JSValue js_b2world_set_gravity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "x and y required");
    
    double x, y;
    if (!js_b2_to_float64_strict(ctx, argv[0], &x)) return JS_EXCEPTION;
    if (!js_b2_to_float64_strict(ctx, argv[1], &y)) return JS_EXCEPTION;

    b2World_SetGravity(w->worldId, (b2Vec2){(float)x, (float)y});
    return JS_UNDEFINED;
}

static JSValue js_b2world_enable_continuous(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "flag required");
    
    bool flag;
    if (!js_b2_to_bool_strict(ctx, argv[0], &flag)) return JS_EXCEPTION;
    b2World_EnableContinuous(w->worldId, flag);
    return JS_UNDEFINED;
}

static JSValue js_b2world_is_continuous_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    return JS_NewBool(ctx, b2World_IsContinuousEnabled(w->worldId));
}

static JSValue js_b2world_set_restitution_threshold(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "value required");
    
    double v;
    if (!js_b2_to_float64_strict(ctx, argv[0], &v)) return JS_EXCEPTION;
    if (!js_b2_validate_non_negative(ctx, "restitutionThreshold", v)) return JS_EXCEPTION;
    b2World_SetRestitutionThreshold(w->worldId, (float)v);
    return JS_UNDEFINED;
}

static JSValue js_b2world_get_restitution_threshold(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    return JS_NewFloat64(ctx, b2World_GetRestitutionThreshold(w->worldId));
}

static JSValue js_b2world_set_maximum_linear_speed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "value required");
    double v;
    if (!js_b2_to_float64_strict(ctx, argv[0], &v)) return JS_EXCEPTION;
    if (!js_b2_validate_non_negative(ctx, "maximumLinearSpeed", v)) return JS_EXCEPTION;
    b2World_SetMaximumLinearSpeed(w->worldId, (float)v);
    return JS_UNDEFINED;
}

static JSValue js_b2world_get_maximum_linear_speed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2World *w = (JSB2World *)JS_GetOpaque2(ctx, this_val, js_b2world_class_id);
    if (!w || !b2World_IsValid(w->worldId)) return JS_ThrowTypeError(ctx, "Invalid Box2D World");
    return JS_NewFloat64(ctx, b2World_GetMaximumLinearSpeed(w->worldId));
}


void js_b2world_register(JSContext *ctx) {
    JS_NewClassID(&js_b2world_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_b2world_class_id, &js_b2world_class);
    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_b2world_proto_funcs, countof(js_b2world_proto_funcs));
    JS_SetClassProto(ctx, js_b2world_class_id, proto);
}

#endif