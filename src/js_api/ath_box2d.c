#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>

#ifdef ATHENA_BOX2D
#include <box2d/box2d.h>
#include "ath_box2d.h"

static JSValue js_create_world(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    b2WorldDef worldDef = b2DefaultWorldDef();

    if (argc >= 1 && JS_IsObject(argv[0])) {
        JSValue val;

        val = JS_GetPropertyStr(ctx, argv[0], "gravity");
        if (JS_IsObject(val)) {
            JSValue gx = JS_GetPropertyStr(ctx, val, "x");
            JSValue gy = JS_GetPropertyStr(ctx, val, "y");
            double x = 0.0, y = -9.8;
            if (!JS_IsUndefined(gx)) JS_ToFloat64(ctx, &x, gx);
            if (!JS_IsUndefined(gy)) JS_ToFloat64(ctx, &y, gy);
            worldDef.gravity = (b2Vec2){(float)x, (float)y};
            JS_FreeValue(ctx, gx);
            JS_FreeValue(ctx, gy);
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "enableSleep");
        if (!JS_IsUndefined(val)) {
            worldDef.enableSleep = JS_ToBool(ctx, val);
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "enableContinuous");
        if (!JS_IsUndefined(val)) {
            worldDef.enableContinuous = JS_ToBool(ctx, val);
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "restitutionThreshold");
        if (!JS_IsUndefined(val)) {
            double d = 1.0;
            JS_ToFloat64(ctx, &d, val);
            worldDef.restitutionThreshold = (float)d;
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "hitEventThreshold");
        if (!JS_IsUndefined(val)) {
            double d = 1.0;
            JS_ToFloat64(ctx, &d, val);
            worldDef.hitEventThreshold = (float)d;
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "maximumLinearSpeed");
        if (!JS_IsUndefined(val)) {
            double d = 200.0;
            JS_ToFloat64(ctx, &d, val);
            worldDef.maximumLinearSpeed = (float)d;
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "workerCount");
        if (!JS_IsUndefined(val)) {
            int32_t i = 1;
            JS_ToInt32(ctx, &i, val);
            worldDef.workerCount = i;
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "enqueueTask");
        if (!JS_IsUndefined(val)) {
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "finishTask");
        if (!JS_IsUndefined(val)) {
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "userData");
        if (!JS_IsUndefined(val)) {
            worldDef.userData = js_b2_box_userdata(ctx, NULL, val);
            JS_FreeValue(ctx, val);
        }

        val = JS_GetPropertyStr(ctx, argv[0], "internalAllocator");
        if (!JS_IsUndefined(val)) {
            JS_FreeValue(ctx, val);
        }
    }

    b2WorldId worldId = b2CreateWorld(&worldDef);
    JSB2World *w = (JSB2World *)malloc(sizeof(JSB2World));
    if (!w) {
        b2DestroyWorld(worldId);
        return JS_ThrowOutOfMemory(ctx);
    }
    w->worldId = worldId;
    w->userDataList = NULL;
    if (worldDef.userData) js_b2_userdata_link(w, (JSB2UserDataBox *)worldDef.userData);

    JSValue obj = JS_NewObjectClass(ctx, js_b2world_class_id);
    JS_SetOpaque(obj, w);
    return obj;
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_PROP_STRING_DEF("version", "3.0.0", JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("STATIC_BODY", b2_staticBody, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("KINEMATIC_BODY", b2_kinematicBody, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("DYNAMIC_BODY", b2_dynamicBody, JS_PROP_CONFIGURABLE),
    JS_CFUNC_DEF("createWorld", 1, js_create_world),
};

static int module_init(JSContext *ctx, JSModuleDef *m) {
    js_b2world_register(ctx);
    js_b2body_register(ctx);
    js_b2shape_register(ctx);
    js_b2joint_register(ctx);
    js_b2chain_register(ctx);

    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_box2d_init(JSContext *ctx) {
    return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Box2D");
}

#endif