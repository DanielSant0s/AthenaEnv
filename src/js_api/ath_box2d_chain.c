#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <ath_env.h>

#ifdef ATHENA_BOX2D

#include <box2d/box2d.h>
#include "ath_box2d.h"


static void js_b2chain_finalizer(JSRuntime *rt, JSValue val)
{
    JSB2Chain *c =
        (JSB2Chain *)JS_GetOpaque(val, js_b2chain_class_id);

    if (c)
    {
        if (b2Chain_IsValid(c->chainId))
        {
            if (c->world)
            {
                uint64_t chainCacheId =
                    b2StoreChainId(c->chainId);

                js_b2_wrapper_cache_remove(
                    c->world,
                    JSB2_WRAPPER_CHAIN,
                    chainCacheId
                );
            }

            b2DestroyChain(c->chainId);
        }

        free(c);
    }
}


static JSClassDef js_b2chain_class = {
    "B2Chain",
    .finalizer = js_b2chain_finalizer,
};


static JSValue js_b2chain_is_valid(
    JSContext *ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst *argv)
{
    JSB2Chain *c =
        (JSB2Chain *)JS_GetOpaque(
            this_val,
            js_b2chain_class_id
        );

    if (!c)
        return JS_FALSE;

    return JS_NewBool(
        ctx,
        b2Chain_IsValid(c->chainId)
    );
}


static JSValue js_b2chain_destroy(
    JSContext *ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst *argv)
{
    JSB2Chain *c =
        (JSB2Chain *)JS_GetOpaque2(
            ctx,
            this_val,
            js_b2chain_class_id
        );

    if (!c || !b2Chain_IsValid(c->chainId))
        return JS_FALSE;

    uint64_t chainCacheId =
        b2StoreChainId(c->chainId);

    b2DestroyChain(c->chainId);

    c->chainId = (b2ChainId){0};

    js_b2_wrapper_cache_remove(
        c->world,
        JSB2_WRAPPER_CHAIN,
        chainCacheId
    );

    return JS_TRUE;
}

static JSValue js_b2chain_get_body(
    JSContext *ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst *argv)
{
    JSB2Chain *c =
        (JSB2Chain *)JS_GetOpaque2(
            ctx,
            this_val,
            js_b2chain_class_id
        );

    if (!c || !b2Chain_IsValid(c->chainId))
        return JS_ThrowTypeError(
            ctx,
            "Invalid Box2D Chain"
        );

    b2ShapeId shapeId;

    int n =
        b2Chain_GetSegments(
            c->chainId,
            &shapeId,
            1
        );

    if (n <= 0 || !b2Shape_IsValid(shapeId))
        return JS_ThrowInternalError(
            ctx,
            "Box2D Chain has no segments"
        );

    b2BodyId bodyId =
        b2Shape_GetBody(shapeId);

    return js_b2_wrap_body(
        ctx,
        c->world,
        bodyId
    );
}

static JSValue js_b2chain_get_shapes(
    JSContext *ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst *argv)
{
    JSB2Chain *c =
        (JSB2Chain *)JS_GetOpaque2(
            ctx,
            this_val,
            js_b2chain_class_id
        );

    if (!c || !b2Chain_IsValid(c->chainId))
        return JS_ThrowTypeError(
            ctx,
            "Invalid Box2D Chain"
        );

    int count =
        b2Chain_GetSegmentCount(c->chainId);

    JSValue arr =
        JS_NewArray(ctx);

    if (count <= 0)
        return arr;

    b2ShapeId *segments =
        (b2ShapeId *)malloc(
            sizeof(b2ShapeId) * (size_t)count
        );

    if (!segments)
        return JS_ThrowOutOfMemory(ctx);

    int n =
        b2Chain_GetSegments(
            c->chainId,
            segments,
            count
        );

    for (int i = 0; i < n; ++i)
    {
        if (!b2Shape_IsValid(segments[i]))
            continue;

        JS_SetPropertyUint32(
            ctx,
            arr,
            i,
            js_b2_wrap_shape(
                ctx,
                c->world,
                segments[i]
            )
        );
    }

    free(segments);

    return arr;
}

static const JSCFunctionListEntry js_b2chain_proto_funcs[] = {
    JS_CFUNC_DEF(
        "isValid",
        0,
        js_b2chain_is_valid
    ),

    JS_CFUNC_DEF(
        "destroy",
        0,
        js_b2chain_destroy
    ),

    JS_CFUNC_DEF(
        "getBody",
        0,
        js_b2chain_get_body
    ),

    JS_CFUNC_DEF(
        "getShapes",
        0,
        js_b2chain_get_shapes
    ),
};


void js_b2chain_register(JSContext *ctx)
{
    JS_NewClassID(
        &js_b2chain_class_id
    );

    JS_NewClass(
        JS_GetRuntime(ctx),
        js_b2chain_class_id,
        &js_b2chain_class
    );

    JSValue proto =
        JS_NewObject(ctx);

    JS_SetPropertyFunctionList(
        ctx,
        proto,
        js_b2chain_proto_funcs,
        countof(js_b2chain_proto_funcs)
    );

    JS_SetClassProto(
        ctx,
        js_b2chain_class_id,
        proto
    );
}


#endif