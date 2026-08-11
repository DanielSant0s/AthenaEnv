#ifndef ATH_BOX2D_H
#define ATH_BOX2D_H

#ifdef ATHENA_BOX2D

#include <ath_env.h>
#include <box2d/box2d.h>

extern JSClassID js_b2world_class_id;
extern JSClassID js_b2body_class_id;
extern JSClassID js_b2shape_class_id;
extern JSClassID js_b2joint_class_id;
extern JSClassID js_b2chain_class_id;

typedef struct JSB2World {
    b2WorldId worldId;
    void *userDataList;
} JSB2World;

typedef struct JSB2Body {
    b2BodyId bodyId;
    JSB2World *world;
} JSB2Body;

typedef struct JSB2Shape {
    b2ShapeId shapeId;
    JSB2World *world;
} JSB2Shape;

typedef struct JSB2Joint {
    b2JointId jointId;
    JSB2World *world;
} JSB2Joint;

typedef struct JSB2Chain {
    b2ChainId chainId;
    JSB2World *world;
} JSB2Chain;

typedef struct JSB2UserDataBox {
    JSRuntime *rt;
    JSValue value;
    JSB2World *world;
    struct JSB2UserDataBox *prev;
    struct JSB2UserDataBox *next;
} JSB2UserDataBox;

typedef enum JSB2WrapperType {
    JSB2_WRAPPER_BODY = 1,
    JSB2_WRAPPER_SHAPE,
    JSB2_WRAPPER_JOINT,
    JSB2_WRAPPER_CHAIN,
} JSB2WrapperType;

void js_b2_wrapper_cache_remove(JSB2World *world, JSB2WrapperType type, uint64_t id);
void js_b2_wrapper_cache_remove_world(JSB2World *world);

JSValue js_b2_wrap_body(JSContext *ctx, JSB2World *world, b2BodyId bodyId);
JSValue js_b2_wrap_shape(JSContext *ctx, JSB2World *world, b2ShapeId shapeId);
JSValue js_b2_wrap_joint(JSContext *ctx, JSB2World *world, b2JointId jointId);
JSValue js_b2_wrap_chain(JSContext *ctx, JSB2World *world, b2ChainId chainId);

void *js_b2_box_userdata(JSContext *ctx, JSB2World *world, JSValueConst value);
void js_b2_free_userdata_box(void *ptr);
JSValue js_b2_get_userdata_box(JSContext *ctx, void *ptr);
void js_b2_userdata_link(JSB2World *world, JSB2UserDataBox *box);

bool js_b2_validate_positive(JSContext *ctx, const char *name, double value);
bool js_b2_validate_non_negative(JSContext *ctx, const char *name, double value);
bool js_b2_validate_restitution(JSContext *ctx, double value);
bool js_b2_validate_friction(JSContext *ctx, double value);
bool js_b2_validate_density(JSContext *ctx, double value);
bool js_b2_validate_radius(JSContext *ctx, double value);
bool js_b2_validate_damping(JSContext *ctx, double value);
bool js_b2_validate_hertz(JSContext *ctx, double value);
bool js_b2_validate_half_dimension(JSContext *ctx, const char *name, double value);

b2Vec2 js_b2_to_vec2(JSContext *ctx, JSValueConst obj, b2Vec2 fallback);
bool js_b2_to_vec2_strict(JSContext *ctx, JSValueConst val, int index, b2Vec2 *out);
JSValue js_b2_from_vec2(JSContext *ctx, b2Vec2 v);

bool js_b2_parse_collision_filter(JSContext *ctx, JSValueConst value, b2Filter *filter);
bool js_b2_parse_query_filter(JSContext *ctx, JSValueConst value, b2QueryFilter *filter);

bool js_b2_to_float64_strict(JSContext *ctx, JSValueConst val, double *out);
bool js_b2_to_int32_strict(JSContext *ctx, JSValueConst val, int32_t *out);
bool js_b2_to_int64_strict(JSContext *ctx, JSValueConst val, int64_t *out);
bool js_b2_to_bool_strict(JSContext *ctx, JSValueConst val, bool *out);

bool js_b2_apply_shape_options(JSContext *ctx, JSValueConst opts, b2ShapeDef *shapeDef);

void js_b2world_register(JSContext *ctx);
void js_b2body_register(JSContext *ctx);
void js_b2shape_register(JSContext *ctx);
void js_b2joint_register(JSContext *ctx);
void js_b2chain_register(JSContext *ctx);

JSValue js_b2world_raycast_all(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_query_aabb(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_overlap_shape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_overlap_circle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_overlap_capsule(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_overlap_polygon(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

JSValue js_b2world_cast_ray(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_cast_shape(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_cast_circle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_cast_capsule(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_cast_polygon(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_cast_mover(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_collide_mover(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

JSValue js_b2world_get_contact_events(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_get_sensor_events(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_get_body_events(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_b2world_explode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif

#endif