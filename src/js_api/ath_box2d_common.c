#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>

#ifdef ATHENA_BOX2D
#include <box2d/box2d.h>
#include "ath_box2d.h"

JSClassID js_b2world_class_id;
JSClassID js_b2body_class_id;
JSClassID js_b2shape_class_id;
JSClassID js_b2joint_class_id;
JSClassID js_b2chain_class_id;

#define JSB2_WRAPPER_CACHE_BUCKET_COUNT 64

typedef struct JSB2WrapperCacheEntry {
    JSB2World *world;
    JSB2WrapperType type;
    uint64_t id;
    JSRuntime *rt;
    JSValue object;
    struct JSB2WrapperCacheEntry *next;
} JSB2WrapperCacheEntry;

static JSB2WrapperCacheEntry *js_b2_wrapper_cache[JSB2_WRAPPER_CACHE_BUCKET_COUNT];

static uint32_t js_b2_wrapper_cache_hash(JSB2World *world, JSB2WrapperType type, uint64_t id)
{
    uintptr_t w = (uintptr_t)world;
    uint64_t x = id ^ ((uint64_t)w >> 4) ^ ((uint64_t)type * 0x9E3779B97F4A7C15ULL);
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return (uint32_t)(x % JSB2_WRAPPER_CACHE_BUCKET_COUNT);
}

static JSB2WrapperCacheEntry *js_b2_wrapper_cache_find(
    JSB2World *world,
    JSB2WrapperType type,
    uint64_t id)
{
    uint32_t bucket = js_b2_wrapper_cache_hash(world, type, id);
    JSB2WrapperCacheEntry *entry = js_b2_wrapper_cache[bucket];

    while (entry) {
        if (entry->world == world && entry->type == type && entry->id == id) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

static void js_b2_wrapper_cache_insert(
    JSContext *ctx,
    JSB2World *world,
    JSB2WrapperType type,
    uint64_t id,
    JSValue object)
{
    JSB2WrapperCacheEntry *entry = (JSB2WrapperCacheEntry *)malloc(sizeof(JSB2WrapperCacheEntry));

    if (!entry) {
        return;
    }

    entry->world = world;
    entry->type = type;
    entry->id = id;
    entry->rt = JS_GetRuntime(ctx);
    entry->object = JS_DupValue(ctx, object);

    uint32_t bucket = js_b2_wrapper_cache_hash(world, type, id);
    entry->next = js_b2_wrapper_cache[bucket];
    js_b2_wrapper_cache[bucket] = entry;
}

void js_b2_wrapper_cache_remove(
    JSB2World *world,
    JSB2WrapperType type,
    uint64_t id)
{
    if (!world) return;

    uint32_t bucket = js_b2_wrapper_cache_hash(world, type, id);
    JSB2WrapperCacheEntry **link = &js_b2_wrapper_cache[bucket];

    while (*link) {
        JSB2WrapperCacheEntry *entry = *link;
        if (entry->world == world && entry->type == type && entry->id == id) {
            *link = entry->next;
            JS_FreeValueRT(entry->rt, entry->object);
            free(entry);
            return;
        }
        link = &entry->next;
    }
}

static void js_b2_wrapper_cache_detach_world(JSB2World *world)
{
    for (uint32_t bucket = 0; bucket < JSB2_WRAPPER_CACHE_BUCKET_COUNT; ++bucket) {
        JSB2WrapperCacheEntry *entry = js_b2_wrapper_cache[bucket];

        while (entry) {
            if (entry->world == world) {
                switch (entry->type) {
                    case JSB2_WRAPPER_BODY: {
                        JSB2Body *b = (JSB2Body *)JS_GetOpaque(
                            entry->object,
                            js_b2body_class_id
                        );
                        if (b) b->world = NULL;
                        break;
                    }

                    case JSB2_WRAPPER_SHAPE: {
                        JSB2Shape *s = (JSB2Shape *)JS_GetOpaque(
                            entry->object,
                            js_b2shape_class_id
                        );
                        if (s) s->world = NULL;
                        break;
                    }

                    case JSB2_WRAPPER_JOINT: {
                        JSB2Joint *j = (JSB2Joint *)JS_GetOpaque(
                            entry->object,
                            js_b2joint_class_id
                        );
                        if (j) j->world = NULL;
                        break;
                    }

                    case JSB2_WRAPPER_CHAIN: {
                        JSB2Chain *c = (JSB2Chain *)JS_GetOpaque(
                            entry->object,
                            js_b2chain_class_id
                        );
                        if (c) c->world = NULL;
                        break;
                    }
                }
            }

            entry = entry->next;
        }
    }
}

void js_b2_wrapper_cache_remove_world(JSB2World *world)
{
    if (!world) return;

    js_b2_wrapper_cache_detach_world(world);

    for (uint32_t bucket = 0; bucket < JSB2_WRAPPER_CACHE_BUCKET_COUNT; ++bucket) {
        JSB2WrapperCacheEntry **link = &js_b2_wrapper_cache[bucket];

        while (*link) {
            JSB2WrapperCacheEntry *entry = *link;
            if (entry->world == world) {
                *link = entry->next;
                JS_FreeValueRT(entry->rt, entry->object);
                free(entry);
                continue;
            }
            link = &entry->next;
        }
    }
}

static JSValue js_b2_wrapper_cache_get(
    JSContext *ctx,
    JSB2World *world,
    JSB2WrapperType type,
    uint64_t id)
{
    JSB2WrapperCacheEntry *entry = js_b2_wrapper_cache_find(world, type, id);
    if (!entry) return JS_UNDEFINED;
    return JS_DupValue(ctx, entry->object);
}

JSValue js_b2_wrap_body(JSContext *ctx, JSB2World *world, b2BodyId bodyId)
{
    if (!b2Body_IsValid(bodyId)) return JS_NULL;

    uint64_t id = b2StoreBodyId(bodyId);
    JSValue cached = js_b2_wrapper_cache_get(ctx, world, JSB2_WRAPPER_BODY, id);
    if (!JS_IsUndefined(cached)) return cached;

    JSB2Body *b = (JSB2Body *)malloc(sizeof(JSB2Body));
    if (!b) return JS_ThrowOutOfMemory(ctx);

    b->bodyId = bodyId;
    b->world = world;

    JSValue obj = JS_NewObjectClass(ctx, js_b2body_class_id);
    if (JS_IsException(obj)) {
        free(b);
        return obj;
    }

    JS_SetOpaque(obj, b);
    js_b2_wrapper_cache_insert(ctx, world, JSB2_WRAPPER_BODY, id, obj);
    return obj;
}

JSValue js_b2_wrap_shape(JSContext *ctx, JSB2World *world, b2ShapeId shapeId)
{
    if (!b2Shape_IsValid(shapeId)) return JS_NULL;

    uint64_t id = b2StoreShapeId(shapeId);
    JSValue cached = js_b2_wrapper_cache_get(ctx, world, JSB2_WRAPPER_SHAPE, id);
    if (!JS_IsUndefined(cached)) return cached;

    JSB2Shape *s = (JSB2Shape *)malloc(sizeof(JSB2Shape));
    if (!s) return JS_ThrowOutOfMemory(ctx);

    s->shapeId = shapeId;
    s->world = world;

    JSValue obj = JS_NewObjectClass(ctx, js_b2shape_class_id);
    if (JS_IsException(obj)) {
        free(s);
        return obj;
    }

    JS_SetOpaque(obj, s);
    js_b2_wrapper_cache_insert(ctx, world, JSB2_WRAPPER_SHAPE, id, obj);
    return obj;
}

JSValue js_b2_wrap_joint(JSContext *ctx, JSB2World *world, b2JointId jointId)
{
    if (!b2Joint_IsValid(jointId)) return JS_NULL;

    uint64_t id = b2StoreJointId(jointId);
    JSValue cached = js_b2_wrapper_cache_get(ctx, world, JSB2_WRAPPER_JOINT, id);
    if (!JS_IsUndefined(cached)) return cached;

    JSB2Joint *j = (JSB2Joint *)malloc(sizeof(JSB2Joint));
    if (!j) return JS_ThrowOutOfMemory(ctx);

    j->jointId = jointId;
    j->world = world;

    JSValue obj = JS_NewObjectClass(ctx, js_b2joint_class_id);
    if (JS_IsException(obj)) {
        free(j);
        return obj;
    }

    JS_SetOpaque(obj, j);
    js_b2_wrapper_cache_insert(ctx, world, JSB2_WRAPPER_JOINT, id, obj);
    return obj;
}

JSValue js_b2_wrap_chain(JSContext *ctx, JSB2World *world, b2ChainId chainId)
{
    if (!b2Chain_IsValid(chainId)) return JS_NULL;

    uint64_t id = b2StoreChainId(chainId);
    JSValue cached = js_b2_wrapper_cache_get(ctx, world, JSB2_WRAPPER_CHAIN, id);
    if (!JS_IsUndefined(cached)) return cached;

    JSB2Chain *c = (JSB2Chain *)malloc(sizeof(JSB2Chain));
    if (!c) return JS_ThrowOutOfMemory(ctx);

    c->chainId = chainId;
    c->world = world;

    JSValue obj = JS_NewObjectClass(ctx, js_b2chain_class_id);
    if (JS_IsException(obj)) {
        free(c);
        return obj;
    }

    JS_SetOpaque(obj, c);
    js_b2_wrapper_cache_insert(ctx, world, JSB2_WRAPPER_CHAIN, id, obj);
    return obj;
}


#endif