#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>

#ifdef ATHENA_BOX2D
#include <box2d/box2d.h>
#include "ath_box2d.h"

static void js_b2joint_finalizer(JSRuntime *rt, JSValue val) {
    JSB2Joint *j = (JSB2Joint *)JS_GetOpaque(val, js_b2joint_class_id);
    if (j) {
        if (j->world && b2Joint_IsValid(j->jointId)) {
            js_b2_wrapper_cache_remove(j->world, JSB2_WRAPPER_JOINT, b2StoreJointId(j->jointId));
        }
        free(j);
    }
}

static JSClassDef js_b2joint_class = {
    "B2Joint",
    .finalizer = js_b2joint_finalizer,
};

static bool js_b2_validate_limit_range(JSContext *ctx, double lower, double upper) {
    if (!isfinite(lower) || !isfinite(upper)) {
        JS_ThrowRangeError(ctx, "limits must be finite");
        return false;
    }

    if (lower > upper) {
        JS_ThrowRangeError(ctx, "lower limit must be <= upper limit");
        return false;
    }

    return true;
}

static const char *js_b2_joint_type_name(b2JointType type) {
    switch (type) {
        case b2_distanceJoint: return "distance";
        case b2_filterJoint: return "filter";
        case b2_motorJoint: return "motor";
        case b2_prismaticJoint: return "prismatic";
        case b2_revoluteJoint: return "revolute";
        case b2_weldJoint: return "weld";
        case b2_wheelJoint: return "wheel";
        default: return "unknown";
    }
}

#define JS_B2JOINT_GET(argv0) \
    JSB2Joint *j = (JSB2Joint *)JS_GetOpaque2(ctx, this_val, js_b2joint_class_id); \
    if (!j || !b2Joint_IsValid(j->jointId)) return JS_ThrowTypeError(ctx, "Invalid Box2D Joint");

#define JS_B2JOINT_REQUIRE(kindCheck, name) \
    if (!(kindCheck)) return JS_ThrowTypeError(ctx, "This method only applies to %s joints, not a %s joint", \
        name, js_b2_joint_type_name(b2Joint_GetType(j->jointId)));

static JSValue js_b2joint_is_valid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Joint *j = (JSB2Joint *)JS_GetOpaque(this_val, js_b2joint_class_id);
    if (!j) return JS_FALSE;
    return JS_NewBool(ctx, b2Joint_IsValid(j->jointId));
}

static JSValue js_b2joint_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSB2Joint *j = (JSB2Joint *)JS_GetOpaque2(ctx, this_val, js_b2joint_class_id);
    if (!j || !b2Joint_IsValid(j->jointId)) return JS_FALSE;

    bool wake = true;
    if (argc >= 1) wake = JS_ToBool(ctx, argv[0]);

    void *userData = b2Joint_GetUserData(j->jointId);
    if (userData) js_b2_free_userdata_box(userData);

    uint64_t jointCacheId = b2StoreJointId(j->jointId);
    b2DestroyJoint(j->jointId, wake);
    j->jointId = (b2JointId){0};
    js_b2_wrapper_cache_remove(j->world, JSB2_WRAPPER_JOINT, jointCacheId);
    return JS_TRUE;
}

static JSValue js_b2joint_get_type(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    return JS_NewString(ctx, js_b2_joint_type_name(b2Joint_GetType(j->jointId)));
}

static JSValue js_b2joint_get_body_a(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    return js_b2_wrap_body(ctx, j->world, b2Joint_GetBodyA(j->jointId));
}

static JSValue js_b2joint_get_body_b(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    return js_b2_wrap_body(ctx, j->world, b2Joint_GetBodyB(j->jointId));
}

static JSValue js_b2joint_get_user_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    return js_b2_get_userdata_box(ctx, b2Joint_GetUserData(j->jointId));
}

static JSValue js_b2joint_set_user_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "value required");
    void *old = b2Joint_GetUserData(j->jointId);
    if (old) js_b2_free_userdata_box(old);
    b2Joint_SetUserData(j->jointId, js_b2_box_userdata(ctx, j->world, argv[0]));
    return JS_UNDEFINED;
}

static JSValue js_b2joint_set_collide_connected(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "flag required");
    b2Joint_SetCollideConnected(j->jointId, JS_ToBool(ctx, argv[0]));
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_collide_connected(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    return JS_NewBool(ctx, b2Joint_GetCollideConnected(j->jointId));
}

static JSValue js_b2joint_wake_bodies(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2Joint_WakeBodies(j->jointId);
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_constraint_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    return js_b2_from_vec2(ctx, b2Joint_GetConstraintForce(j->jointId));
}

static JSValue js_b2joint_get_constraint_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    return JS_NewFloat64(ctx, b2Joint_GetConstraintTorque(j->jointId));
}

static JSValue js_b2joint_enable_spring(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "flag required");
    bool flag = JS_ToBool(ctx, argv[0]);
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: b2DistanceJoint_EnableSpring(j->jointId, flag); break;
        case b2_revoluteJoint: b2RevoluteJoint_EnableSpring(j->jointId, flag); break;
        case b2_prismaticJoint: b2PrismaticJoint_EnableSpring(j->jointId, flag); break;
        case b2_wheelJoint: b2WheelJoint_EnableSpring(j->jointId, flag); break;
        default: return JS_ThrowTypeError(ctx, "enableSpring() does not apply to %s joints", js_b2_joint_type_name(type));
    }
    return JS_UNDEFINED;
}

static JSValue js_b2joint_is_spring_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: return JS_NewBool(ctx, b2DistanceJoint_IsSpringEnabled(j->jointId));
        case b2_revoluteJoint: return JS_NewBool(ctx, b2RevoluteJoint_IsSpringEnabled(j->jointId));
        case b2_prismaticJoint: return JS_NewBool(ctx, b2PrismaticJoint_IsSpringEnabled(j->jointId));
        case b2_wheelJoint: return JS_NewBool(ctx, b2WheelJoint_IsSpringEnabled(j->jointId));
        default: return JS_ThrowTypeError(ctx, "isSpringEnabled() does not apply to %s joints", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_set_spring_hertz(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "hertz required");
    double hz;
    JS_ToFloat64(ctx, &hz, argv[0]);
    if (!js_b2_validate_hertz(ctx, hz)) return JS_EXCEPTION;
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: b2DistanceJoint_SetSpringHertz(j->jointId, (float)hz); break;
        case b2_revoluteJoint: b2RevoluteJoint_SetSpringHertz(j->jointId, (float)hz); break;
        case b2_prismaticJoint: b2PrismaticJoint_SetSpringHertz(j->jointId, (float)hz); break;
        case b2_wheelJoint: b2WheelJoint_SetSpringHertz(j->jointId, (float)hz); break;
        default: return JS_ThrowTypeError(ctx, "setSpringHertz() does not apply to %s joints", js_b2_joint_type_name(type));
    }
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_spring_hertz(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: return JS_NewFloat64(ctx, b2DistanceJoint_GetSpringHertz(j->jointId));
        case b2_revoluteJoint: return JS_NewFloat64(ctx, b2RevoluteJoint_GetSpringHertz(j->jointId));
        case b2_prismaticJoint: return JS_NewFloat64(ctx, b2PrismaticJoint_GetSpringHertz(j->jointId));
        case b2_wheelJoint: return JS_NewFloat64(ctx, b2WheelJoint_GetSpringHertz(j->jointId));
        default: return JS_ThrowTypeError(ctx, "getSpringHertz() does not apply to %s joints", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_set_spring_damping_ratio(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "dampingRatio required");
    double r;
    JS_ToFloat64(ctx, &r, argv[0]);
    if (!js_b2_validate_non_negative(ctx, "dampingRatio", r)) return JS_EXCEPTION;
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: b2DistanceJoint_SetSpringDampingRatio(j->jointId, (float)r); break;
        case b2_revoluteJoint: b2RevoluteJoint_SetSpringDampingRatio(j->jointId, (float)r); break;
        case b2_prismaticJoint: b2PrismaticJoint_SetSpringDampingRatio(j->jointId, (float)r); break;
        case b2_wheelJoint: b2WheelJoint_SetSpringDampingRatio(j->jointId, (float)r); break;
        default: return JS_ThrowTypeError(ctx, "setSpringDampingRatio() does not apply to %s joints", js_b2_joint_type_name(type));
    }
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_spring_damping_ratio(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: return JS_NewFloat64(ctx, b2DistanceJoint_GetSpringDampingRatio(j->jointId));
        case b2_revoluteJoint: return JS_NewFloat64(ctx, b2RevoluteJoint_GetSpringDampingRatio(j->jointId));
        case b2_prismaticJoint: return JS_NewFloat64(ctx, b2PrismaticJoint_GetSpringDampingRatio(j->jointId));
        case b2_wheelJoint: return JS_NewFloat64(ctx, b2WheelJoint_GetSpringDampingRatio(j->jointId));
        default: return JS_ThrowTypeError(ctx, "getSpringDampingRatio() does not apply to %s joints", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_enable_limit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "flag required");
    bool flag = JS_ToBool(ctx, argv[0]);
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: b2DistanceJoint_EnableLimit(j->jointId, flag); break;
        case b2_revoluteJoint: b2RevoluteJoint_EnableLimit(j->jointId, flag); break;
        case b2_prismaticJoint: b2PrismaticJoint_EnableLimit(j->jointId, flag); break;
        case b2_wheelJoint: b2WheelJoint_EnableLimit(j->jointId, flag); break;
        default: return JS_ThrowTypeError(ctx, "enableLimit() does not apply to %s joints", js_b2_joint_type_name(type));
    }
    return JS_UNDEFINED;
}

static JSValue js_b2joint_is_limit_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: return JS_NewBool(ctx, b2DistanceJoint_IsLimitEnabled(j->jointId));
        case b2_revoluteJoint: return JS_NewBool(ctx, b2RevoluteJoint_IsLimitEnabled(j->jointId));
        case b2_prismaticJoint: return JS_NewBool(ctx, b2PrismaticJoint_IsLimitEnabled(j->jointId));
        case b2_wheelJoint: return JS_NewBool(ctx, b2WheelJoint_IsLimitEnabled(j->jointId));
        default: return JS_ThrowTypeError(ctx, "isLimitEnabled() does not apply to %s joints", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_set_limits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "lower and upper required");
    double lower, upper;
    JS_ToFloat64(ctx, &lower, argv[0]);
    JS_ToFloat64(ctx, &upper, argv[1]);
    
    if (!js_b2_validate_limit_range(ctx, lower, upper)) return JS_EXCEPTION;
    
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: b2DistanceJoint_SetLengthRange(j->jointId, (float)lower, (float)upper); break;
        case b2_revoluteJoint: b2RevoluteJoint_SetLimits(j->jointId, (float)lower, (float)upper); break;
        case b2_prismaticJoint: b2PrismaticJoint_SetLimits(j->jointId, (float)lower, (float)upper); break;
        case b2_wheelJoint: b2WheelJoint_SetLimits(j->jointId, (float)lower, (float)upper); break;
        default: return JS_ThrowTypeError(ctx, "setLimits() does not apply to %s joints", js_b2_joint_type_name(type));
    }
    return JS_UNDEFINED;
}
static JSValue js_b2joint_get_limits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    float lower = 0.0f, upper = 0.0f;
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint:
            lower = b2DistanceJoint_GetMinLength(j->jointId);
            upper = b2DistanceJoint_GetMaxLength(j->jointId);
            break;
        case b2_revoluteJoint:
            lower = b2RevoluteJoint_GetLowerLimit(j->jointId);
            upper = b2RevoluteJoint_GetUpperLimit(j->jointId);
            break;
        case b2_prismaticJoint:
            lower = b2PrismaticJoint_GetLowerLimit(j->jointId);
            upper = b2PrismaticJoint_GetUpperLimit(j->jointId);
            break;
        case b2_wheelJoint:
            lower = b2WheelJoint_GetLowerLimit(j->jointId);
            upper = b2WheelJoint_GetUpperLimit(j->jointId);
            break;
        default: return JS_ThrowTypeError(ctx, "getLimits() does not apply to %s joints", js_b2_joint_type_name(type));
    }
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "lower", JS_NewFloat64(ctx, lower));
    JS_SetPropertyStr(ctx, obj, "upper", JS_NewFloat64(ctx, upper));
    return obj;
}

static JSValue js_b2joint_enable_motor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "flag required");
    bool flag = JS_ToBool(ctx, argv[0]);
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: b2DistanceJoint_EnableMotor(j->jointId, flag); break;
        case b2_revoluteJoint: b2RevoluteJoint_EnableMotor(j->jointId, flag); break;
        case b2_prismaticJoint: b2PrismaticJoint_EnableMotor(j->jointId, flag); break;
        case b2_wheelJoint: b2WheelJoint_EnableMotor(j->jointId, flag); break;
        default: return JS_ThrowTypeError(ctx, "enableMotor() does not apply to %s joints", js_b2_joint_type_name(type));
    }
    return JS_UNDEFINED;
}

static JSValue js_b2joint_is_motor_enabled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: return JS_NewBool(ctx, b2DistanceJoint_IsMotorEnabled(j->jointId));
        case b2_revoluteJoint: return JS_NewBool(ctx, b2RevoluteJoint_IsMotorEnabled(j->jointId));
        case b2_prismaticJoint: return JS_NewBool(ctx, b2PrismaticJoint_IsMotorEnabled(j->jointId));
        case b2_wheelJoint: return JS_NewBool(ctx, b2WheelJoint_IsMotorEnabled(j->jointId));
        default: return JS_ThrowTypeError(ctx, "isMotorEnabled() does not apply to %s joints", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_set_motor_speed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "speed required");
    double v;
    JS_ToFloat64(ctx, &v, argv[0]);
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: b2DistanceJoint_SetMotorSpeed(j->jointId, (float)v); break;
        case b2_revoluteJoint: b2RevoluteJoint_SetMotorSpeed(j->jointId, (float)v); break;
        case b2_prismaticJoint: b2PrismaticJoint_SetMotorSpeed(j->jointId, (float)v); break;
        case b2_wheelJoint: b2WheelJoint_SetMotorSpeed(j->jointId, (float)v); break;
        default: return JS_ThrowTypeError(ctx, "setMotorSpeed() does not apply to %s joints", js_b2_joint_type_name(type));
    }
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_motor_speed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: return JS_NewFloat64(ctx, b2DistanceJoint_GetMotorSpeed(j->jointId));
        case b2_revoluteJoint: return JS_NewFloat64(ctx, b2RevoluteJoint_GetMotorSpeed(j->jointId));
        case b2_prismaticJoint: return JS_NewFloat64(ctx, b2PrismaticJoint_GetMotorSpeed(j->jointId));
        case b2_wheelJoint: return JS_NewFloat64(ctx, b2WheelJoint_GetMotorSpeed(j->jointId));
        default: return JS_ThrowTypeError(ctx, "getMotorSpeed() does not apply to %s joints", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_set_max_motor_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "force required");
    double f;
    JS_ToFloat64(ctx, &f, argv[0]);
    if (!js_b2_validate_non_negative(ctx, "maxMotorForce", f)) return JS_EXCEPTION;
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: b2DistanceJoint_SetMaxMotorForce(j->jointId, (float)f); break;
        case b2_prismaticJoint: b2PrismaticJoint_SetMaxMotorForce(j->jointId, (float)f); break;
        default: return JS_ThrowTypeError(ctx, "setMaxMotorForce() only applies to distance/prismatic joints, not a %s joint", js_b2_joint_type_name(type));
    }
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_max_motor_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: return JS_NewFloat64(ctx, b2DistanceJoint_GetMaxMotorForce(j->jointId));
        case b2_prismaticJoint: return JS_NewFloat64(ctx, b2PrismaticJoint_GetMaxMotorForce(j->jointId));
        default: return JS_ThrowTypeError(ctx, "getMaxMotorForce() only applies to distance/prismatic joints, not a %s joint", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_get_motor_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_distanceJoint: return JS_NewFloat64(ctx, b2DistanceJoint_GetMotorForce(j->jointId));
        case b2_prismaticJoint: return JS_NewFloat64(ctx, b2PrismaticJoint_GetMotorForce(j->jointId));
        default: return JS_ThrowTypeError(ctx, "getMotorForce() only applies to distance/prismatic joints, not a %s joint", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_set_max_motor_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "torque required");
    double t;
    JS_ToFloat64(ctx, &t, argv[0]);
    if (!js_b2_validate_non_negative(ctx, "maxMotorTorque", t)) return JS_EXCEPTION;
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_revoluteJoint: b2RevoluteJoint_SetMaxMotorTorque(j->jointId, (float)t); break;
        case b2_wheelJoint: b2WheelJoint_SetMaxMotorTorque(j->jointId, (float)t); break;
        default: return JS_ThrowTypeError(ctx, "setMaxMotorTorque() only applies to revolute/wheel joints, not a %s joint", js_b2_joint_type_name(type));
    }
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_max_motor_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_revoluteJoint: return JS_NewFloat64(ctx, b2RevoluteJoint_GetMaxMotorTorque(j->jointId));
        case b2_wheelJoint: return JS_NewFloat64(ctx, b2WheelJoint_GetMaxMotorTorque(j->jointId));
        default: return JS_ThrowTypeError(ctx, "getMaxMotorTorque() only applies to revolute/wheel joints, not a %s joint", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_get_motor_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_revoluteJoint: return JS_NewFloat64(ctx, b2RevoluteJoint_GetMotorTorque(j->jointId));
        case b2_wheelJoint: return JS_NewFloat64(ctx, b2WheelJoint_GetMotorTorque(j->jointId));
        default: return JS_ThrowTypeError(ctx, "getMotorTorque() only applies to revolute/wheel joints, not a %s joint", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_get_angle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_revoluteJoint, "revolute");
    return JS_NewFloat64(ctx, b2RevoluteJoint_GetAngle(j->jointId));
}

static JSValue js_b2joint_get_translation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_prismaticJoint, "prismatic");
    return JS_NewFloat64(ctx, b2PrismaticJoint_GetTranslation(j->jointId));
}

static JSValue js_b2joint_get_speed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_prismaticJoint, "prismatic");
    return JS_NewFloat64(ctx, b2PrismaticJoint_GetSpeed(j->jointId));
}

static JSValue js_b2joint_get_length(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_distanceJoint, "distance");
    return JS_NewFloat64(ctx, b2DistanceJoint_GetLength(j->jointId));
}

static JSValue js_b2joint_set_length(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_distanceJoint, "distance");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "length required");
    double len;
    JS_ToFloat64(ctx, &len, argv[0]);
    b2DistanceJoint_SetLength(j->jointId, (float)len);
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_current_length(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_distanceJoint, "distance");
    return JS_NewFloat64(ctx, b2DistanceJoint_GetCurrentLength(j->jointId));
}

static JSValue js_b2joint_set_linear_hertz(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "hertz required");
    double hz;
    JS_ToFloat64(ctx, &hz, argv[0]);
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_weldJoint: b2WeldJoint_SetLinearHertz(j->jointId, (float)hz); break;
        case b2_motorJoint: b2MotorJoint_SetLinearHertz(j->jointId, (float)hz); break;
        default: return JS_ThrowTypeError(ctx, "setLinearHertz() only applies to weld/motor joints, not a %s joint", js_b2_joint_type_name(type));
    }
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_linear_hertz(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_weldJoint: return JS_NewFloat64(ctx, b2WeldJoint_GetLinearHertz(j->jointId));
        case b2_motorJoint: return JS_NewFloat64(ctx, b2MotorJoint_GetLinearHertz(j->jointId));
        default: return JS_ThrowTypeError(ctx, "getLinearHertz() only applies to weld/motor joints, not a %s joint", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_set_linear_damping_ratio(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "dampingRatio required");
    double r;
    JS_ToFloat64(ctx, &r, argv[0]);
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_weldJoint: b2WeldJoint_SetLinearDampingRatio(j->jointId, (float)r); break;
        case b2_motorJoint: b2MotorJoint_SetLinearDampingRatio(j->jointId, (float)r); break;
        default: return JS_ThrowTypeError(ctx, "setLinearDampingRatio() only applies to weld/motor joints, not a %s joint", js_b2_joint_type_name(type));
    }
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_linear_damping_ratio(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_weldJoint: return JS_NewFloat64(ctx, b2WeldJoint_GetLinearDampingRatio(j->jointId));
        case b2_motorJoint: return JS_NewFloat64(ctx, b2MotorJoint_GetLinearDampingRatio(j->jointId));
        default: return JS_ThrowTypeError(ctx, "getLinearDampingRatio() only applies to weld/motor joints, not a %s joint", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_set_angular_hertz(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "hertz required");
    double hz;
    JS_ToFloat64(ctx, &hz, argv[0]);
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_weldJoint: b2WeldJoint_SetAngularHertz(j->jointId, (float)hz); break;
        case b2_motorJoint: b2MotorJoint_SetAngularHertz(j->jointId, (float)hz); break;
        default: return JS_ThrowTypeError(ctx, "setAngularHertz() only applies to weld/motor joints, not a %s joint", js_b2_joint_type_name(type));
    }
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_angular_hertz(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_weldJoint: return JS_NewFloat64(ctx, b2WeldJoint_GetAngularHertz(j->jointId));
        case b2_motorJoint: return JS_NewFloat64(ctx, b2MotorJoint_GetAngularHertz(j->jointId));
        default: return JS_ThrowTypeError(ctx, "getAngularHertz() only applies to weld/motor joints, not a %s joint", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_set_angular_damping_ratio(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "dampingRatio required");
    double r;
    JS_ToFloat64(ctx, &r, argv[0]);
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_weldJoint: b2WeldJoint_SetAngularDampingRatio(j->jointId, (float)r); break;
        case b2_motorJoint: b2MotorJoint_SetAngularDampingRatio(j->jointId, (float)r); break;
        default: return JS_ThrowTypeError(ctx, "setAngularDampingRatio() only applies to weld/motor joints, not a %s joint", js_b2_joint_type_name(type));
    }
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_angular_damping_ratio(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    b2JointType type = b2Joint_GetType(j->jointId);
    switch (type) {
        case b2_weldJoint: return JS_NewFloat64(ctx, b2WeldJoint_GetAngularDampingRatio(j->jointId));
        case b2_motorJoint: return JS_NewFloat64(ctx, b2MotorJoint_GetAngularDampingRatio(j->jointId));
        default: return JS_ThrowTypeError(ctx, "getAngularDampingRatio() only applies to weld/motor joints, not a %s joint", js_b2_joint_type_name(type));
    }
}

static JSValue js_b2joint_set_linear_velocity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_motorJoint, "motor");
    if (argc < 2) return JS_ThrowSyntaxError(ctx, "x and y required");
    double x, y;
    JS_ToFloat64(ctx, &x, argv[0]);
    JS_ToFloat64(ctx, &y, argv[1]);
    b2MotorJoint_SetLinearVelocity(j->jointId, (b2Vec2){(float)x, (float)y});
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_linear_velocity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_motorJoint, "motor");
    return js_b2_from_vec2(ctx, b2MotorJoint_GetLinearVelocity(j->jointId));
}

static JSValue js_b2joint_set_angular_velocity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_motorJoint, "motor");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "velocity required");
    double v;
    JS_ToFloat64(ctx, &v, argv[0]);
    b2MotorJoint_SetAngularVelocity(j->jointId, (float)v);
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_angular_velocity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_motorJoint, "motor");
    return JS_NewFloat64(ctx, b2MotorJoint_GetAngularVelocity(j->jointId));
}

static JSValue js_b2joint_set_max_velocity_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_motorJoint, "motor");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "force required");
    double f;
    JS_ToFloat64(ctx, &f, argv[0]);
    b2MotorJoint_SetMaxVelocityForce(j->jointId, (float)f);
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_max_velocity_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_motorJoint, "motor");
    return JS_NewFloat64(ctx, b2MotorJoint_GetMaxVelocityForce(j->jointId));
}

static JSValue js_b2joint_set_max_velocity_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_motorJoint, "motor");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "torque required");
    double t;
    JS_ToFloat64(ctx, &t, argv[0]);
    b2MotorJoint_SetMaxVelocityTorque(j->jointId, (float)t);
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_max_velocity_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_motorJoint, "motor");
    return JS_NewFloat64(ctx, b2MotorJoint_GetMaxVelocityTorque(j->jointId));
}

static JSValue js_b2joint_set_max_spring_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_motorJoint, "motor");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "force required");
    double f;
    JS_ToFloat64(ctx, &f, argv[0]);
    b2MotorJoint_SetMaxSpringForce(j->jointId, (float)f);
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_max_spring_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_motorJoint, "motor");
    return JS_NewFloat64(ctx, b2MotorJoint_GetMaxSpringForce(j->jointId));
}

static JSValue js_b2joint_set_max_spring_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_motorJoint, "motor");
    if (argc < 1) return JS_ThrowSyntaxError(ctx, "torque required");
    double t;
    JS_ToFloat64(ctx, &t, argv[0]);
    b2MotorJoint_SetMaxSpringTorque(j->jointId, (float)t);
    return JS_UNDEFINED;
}

static JSValue js_b2joint_get_max_spring_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JS_B2JOINT_GET();
    JS_B2JOINT_REQUIRE(b2Joint_GetType(j->jointId) == b2_motorJoint, "motor");
    return JS_NewFloat64(ctx, b2MotorJoint_GetMaxSpringTorque(j->jointId));
}


#undef JS_B2JOINT_GET
#undef JS_B2JOINT_REQUIRE

static const JSCFunctionListEntry js_b2joint_proto_funcs[] = {
    JS_CFUNC_DEF("isValid", 0, js_b2joint_is_valid),
    JS_CFUNC_DEF("destroy", 0, js_b2joint_destroy),
    JS_CFUNC_DEF("getType", 0, js_b2joint_get_type),
    JS_CFUNC_DEF("getBodyA", 0, js_b2joint_get_body_a),
    JS_CFUNC_DEF("getBodyB", 0, js_b2joint_get_body_b),
    JS_CFUNC_DEF("getUserData", 0, js_b2joint_get_user_data),
    JS_CFUNC_DEF("setUserData", 1, js_b2joint_set_user_data),
    JS_CFUNC_DEF("setCollideConnected", 1, js_b2joint_set_collide_connected),
    JS_CFUNC_DEF("getCollideConnected", 0, js_b2joint_get_collide_connected),
    JS_CFUNC_DEF("wakeBodies", 0, js_b2joint_wake_bodies),
    JS_CFUNC_DEF("getConstraintForce", 0, js_b2joint_get_constraint_force),
    JS_CFUNC_DEF("getConstraintTorque", 0, js_b2joint_get_constraint_torque),
    JS_CFUNC_DEF("enableSpring", 1, js_b2joint_enable_spring),
    JS_CFUNC_DEF("isSpringEnabled", 0, js_b2joint_is_spring_enabled),
    JS_CFUNC_DEF("setSpringHertz", 1, js_b2joint_set_spring_hertz),
    JS_CFUNC_DEF("getSpringHertz", 0, js_b2joint_get_spring_hertz),
    JS_CFUNC_DEF("setSpringDampingRatio", 1, js_b2joint_set_spring_damping_ratio),
    JS_CFUNC_DEF("getSpringDampingRatio", 0, js_b2joint_get_spring_damping_ratio),
    JS_CFUNC_DEF("enableLimit", 1, js_b2joint_enable_limit),
    JS_CFUNC_DEF("isLimitEnabled", 0, js_b2joint_is_limit_enabled),
    JS_CFUNC_DEF("setLimits", 2, js_b2joint_set_limits),
    JS_CFUNC_DEF("getLimits", 0, js_b2joint_get_limits),
    JS_CFUNC_DEF("enableMotor", 1, js_b2joint_enable_motor),
    JS_CFUNC_DEF("isMotorEnabled", 0, js_b2joint_is_motor_enabled),
    JS_CFUNC_DEF("setMotorSpeed", 1, js_b2joint_set_motor_speed),
    JS_CFUNC_DEF("getMotorSpeed", 0, js_b2joint_get_motor_speed),
    JS_CFUNC_DEF("setMaxMotorForce", 1, js_b2joint_set_max_motor_force),
    JS_CFUNC_DEF("getMaxMotorForce", 0, js_b2joint_get_max_motor_force),
    JS_CFUNC_DEF("getMotorForce", 0, js_b2joint_get_motor_force),
    JS_CFUNC_DEF("setMaxMotorTorque", 1, js_b2joint_set_max_motor_torque),
    JS_CFUNC_DEF("getMaxMotorTorque", 0, js_b2joint_get_max_motor_torque),
    JS_CFUNC_DEF("getMotorTorque", 0, js_b2joint_get_motor_torque),
    JS_CFUNC_DEF("getAngle", 0, js_b2joint_get_angle),
    JS_CFUNC_DEF("getTranslation", 0, js_b2joint_get_translation),
    JS_CFUNC_DEF("getSpeed", 0, js_b2joint_get_speed),
    JS_CFUNC_DEF("getLength", 0, js_b2joint_get_length),
    JS_CFUNC_DEF("setLength", 1, js_b2joint_set_length),
    JS_CFUNC_DEF("getCurrentLength", 0, js_b2joint_get_current_length),
    JS_CFUNC_DEF("setLinearHertz", 1, js_b2joint_set_linear_hertz),
    JS_CFUNC_DEF("getLinearHertz", 0, js_b2joint_get_linear_hertz),
    JS_CFUNC_DEF("setLinearDampingRatio", 1, js_b2joint_set_linear_damping_ratio),
    JS_CFUNC_DEF("getLinearDampingRatio", 0, js_b2joint_get_linear_damping_ratio),
    JS_CFUNC_DEF("setAngularHertz", 1, js_b2joint_set_angular_hertz),
    JS_CFUNC_DEF("getAngularHertz", 0, js_b2joint_get_angular_hertz),
    JS_CFUNC_DEF("setAngularDampingRatio", 1, js_b2joint_set_angular_damping_ratio),
    JS_CFUNC_DEF("getAngularDampingRatio", 0, js_b2joint_get_angular_damping_ratio),
    JS_CFUNC_DEF("setLinearVelocity", 2, js_b2joint_set_linear_velocity),
    JS_CFUNC_DEF("getLinearVelocity", 0, js_b2joint_get_linear_velocity),
    JS_CFUNC_DEF("setAngularVelocity", 1, js_b2joint_set_angular_velocity),
    JS_CFUNC_DEF("getAngularVelocity", 0, js_b2joint_get_angular_velocity),
    JS_CFUNC_DEF("setMaxVelocityForce", 1, js_b2joint_set_max_velocity_force),
    JS_CFUNC_DEF("getMaxVelocityForce", 0, js_b2joint_get_max_velocity_force),
    JS_CFUNC_DEF("setMaxVelocityTorque", 1, js_b2joint_set_max_velocity_torque),
    JS_CFUNC_DEF("getMaxVelocityTorque", 0, js_b2joint_get_max_velocity_torque),
    JS_CFUNC_DEF("setMaxSpringForce", 1, js_b2joint_set_max_spring_force),
    JS_CFUNC_DEF("getMaxSpringForce", 0, js_b2joint_get_max_spring_force),
    JS_CFUNC_DEF("setMaxSpringTorque", 1, js_b2joint_set_max_spring_torque),
    JS_CFUNC_DEF("getMaxSpringTorque", 0, js_b2joint_get_max_spring_torque),
};


void js_b2joint_register(JSContext *ctx) {
    JS_NewClassID(&js_b2joint_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_b2joint_class_id, &js_b2joint_class);
    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_b2joint_proto_funcs, countof(js_b2joint_proto_funcs));
    JS_SetClassProto(ctx, js_b2joint_class_id, proto);
}

#endif