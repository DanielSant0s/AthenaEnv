#ifndef ATH_BINDINGS_H
#define ATH_BINDINGS_H

#include "../quickjs/quickjs-libc.h"
#include <render.h>

#ifdef ATHENA_PHYSICS
#include <physics.h>
#include <athena/physics_facade.h>
#endif

#define ATHENA_PROP_INT32(item) JS_PROP_INT32_DEF(stringify(item), item, JS_PROP_CONFIGURABLE)

#ifdef ATHENA_AUDIO
#include <athena/sound.h>

typedef struct {
    AthenaSound *sound;
} JSSoundStream;

typedef struct {
    AthenaSfx *sfx;
} JSSoundEffect;
#endif

#ifdef ATHENA_GRAPHICS
#include <shadows.h>
#include <fntsys.h>
#include <athena/shadows_facade.h>
#include <athena/render_facade.h>

typedef struct {
    AthenaRenderObject *native;
} JSRenderObject;

typedef struct {
    AthenaShadowProjector *proj;
} JSShadowProjector;

JSClassID get_img_class_id(void);
JSClassID get_imglist_class_id(void);
extern JSClassID js_render_object_class_id;
#endif

#ifdef ATHENA_PHYSICS
typedef struct {
    AthenaPhysicsWorld *world;
} JSWorld;

typedef struct {
    AthenaPhysicsSpace *space;
    AthenaPhysicsSpace *parent;
} JSSpace;

typedef struct {
    AthenaPhysicsGeom  *geom;
    AthenaPhysicsSpace *parent_space;
    JSValue             js_obj;
} JSGeom;

typedef struct {
    AthenaPhysicsBody  *body;
    AthenaPhysicsWorld *parent_world;
    bool                borrowed; /* wraps a body owned by another wrapper: never destroyed */
} JSBody;

typedef struct {
    AthenaPhysicsJoint *joint;
    AthenaPhysicsWorld *parent_world;
} JSJoint;

typedef struct {
    AthenaPhysicsJointGroup *group;
} JSJointGroup;

extern JSClassID js_geom_class_id;
extern JSClassID js_body_class_id;
extern JSClassID js_space_class_id;
#endif

JSClassID get_matrix4_class_id(void);
/* Matrix4/Vector4 over engine-owned memory: never freed by the GC; `owner`
 * (an object, or JS_UNDEFINED) is kept alive as long as the view. */
JSValue athena_js_matrix4_view(JSContext *ctx, void *data, JSValueConst owner);
JSValue athena_js_vector4_view(JSContext *ctx, void *data, JSValueConst owner);
JSClassID get_vector2_class_id(void);
JSClassID get_vector3_class_id(void);
JSClassID get_vector4_class_id(void);

#define countof(x) (sizeof(x) / sizeof((x)[0]))

#endif /* ATH_BINDINGS_H */
