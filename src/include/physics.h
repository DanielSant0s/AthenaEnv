#ifndef ATHENA_PHYSICS_H
#define ATHENA_PHYSICS_H

#include <stdbool.h>
#include <stdint.h>
#include <render.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ATH_PHYS_MAX_CONTACTS 64

typedef struct AthenaPhysicsWorld      AthenaPhysicsWorld;
typedef struct AthenaPhysicsSpace      AthenaPhysicsSpace;
typedef struct AthenaPhysicsBody       AthenaPhysicsBody;
typedef struct AthenaPhysicsGeom       AthenaPhysicsGeom;
typedef struct AthenaPhysicsTriMesh    AthenaPhysicsTriMesh;
typedef struct AthenaPhysicsJoint      AthenaPhysicsJoint;
typedef struct AthenaPhysicsJointGroup AthenaPhysicsJointGroup;

typedef enum {
    ATH_GEOM_SPHERE = 0,
    ATH_GEOM_BOX,
    ATH_GEOM_PLANE,
    ATH_GEOM_RAY,
    ATH_GEOM_TRIMESH,
    ATH_GEOM_TRANSFORM,
    ATH_GEOM_SPACE
} AthenaGeomType;

typedef enum {
    ATH_JOINT_BALL = 0,
    ATH_JOINT_HINGE,
    ATH_JOINT_SLIDER,
    ATH_JOINT_HINGE2,
    ATH_JOINT_UNIVERSAL,
    ATH_JOINT_FIXED,
    ATH_JOINT_NULL,
    ATH_JOINT_AMOTOR,
    ATH_JOINT_CONTACT
} AthenaJointType;

/* 16-byte aligned contact point produced by narrowphase collision */
typedef struct AthenaContact {
    VECTOR pos;        /* [x, y, z, 1.0f] */
    VECTOR normal;     /* [nx, ny, nz, 0.0f] pointing from g2 to g1 (or surface normal for rays) */
    float  depth;      /* penetration depth (or ray hit distance) */
    float  mu;         /* friction coefficient (default 0.5f) */
    float  bounce;     /* restitution (default 0.1f) */
    float  soft_cfm;   /* constraint force mixing (default 0.01f) */
    AthenaPhysicsGeom *g1;
    AthenaPhysicsGeom *g2;
    uint32_t tri_index;
    uint32_t _pad;
} AthenaContact __attribute__((aligned(16)));

typedef void (*AthenaNearCallback)(void *data, AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2);

/* Compact 32-byte BVH node (2 quadwords) for VU0 LQC2 streaming */
typedef struct AthenaBVHNode {
    VECTOR aabb_min;   /* [min_x, min_y, min_z, *(float*)&left_or_first] */
    VECTOR aabb_max;   /* [max_x, max_y, max_z, *(float*)&tri_count]     */
} AthenaBVHNode __attribute__((aligned(16)));

/* Pre-cooked 64-byte triangle (4 quadwords) for branchless VU0 Moller-Trumbore & SAT */
typedef struct AthenaTri {
    VECTOR v0;         /* vertex 0 [x, y, z, 0] */
    VECTOR e1;         /* v1 - v0  [x, y, z, 0] */
    VECTOR e2;         /* v2 - v0  [x, y, z, 0] */
    VECTOR normal;     /* unit face normal [nx, ny, nz, d] */
} AthenaTri __attribute__((aligned(16)));

struct AthenaPhysicsTriMesh {
    AthenaTri     *tris;
    AthenaBVHNode *nodes;
    uint32_t       tri_count;
    uint32_t       node_count;
    uint32_t       ref_count;
    VECTOR         local_min;
    VECTOR         local_max;
} __attribute__((aligned(16)));

struct AthenaPhysicsBody {
    VECTOR position;       /* [x, y, z, 1] */
    VECTOR quat;           /* [qx, qy, qz, qw] */
    MATRIX rotation;       /* 4x4 padded rotation matrix (rows 0..2 = R, row 3 = [0,0,0,1]) */
    VECTOR linear_vel;     /* [vx, vy, vz, 0] */
    VECTOR angular_vel;    /* [wx, wy, wz, 0] */
    VECTOR force_acc;      /* [fx, fy, fz, 0] */
    VECTOR torque_acc;     /* [tx, ty, tz, 0] */
    VECTOR inv_inertia_local; /* diagonal local inverse inertia [Ixx^-1, Iyy^-1, Izz^-1, 0] */
    MATRIX inv_inertia_world; /* R * diag(inv_I) * R^T */

    float  mass;
    float  inv_mass;
    float  linear_damping;
    float  angular_damping;
    bool   enabled;
    int    index;          /* solver island index */

    AthenaPhysicsWorld *world;
    AthenaPhysicsBody  *next;
    AthenaPhysicsBody  *prev;
    void               *user_data;
} __attribute__((aligned(16)));

struct AthenaPhysicsGeom {
    VECTOR position;       /* local/standalone position [x, y, z, 1] */
    MATRIX rotation;       /* local/standalone rotation matrix */
    VECTOR scale;          /* [sx, sy, sz, 0] */
    VECTOR aabb_min;       /* world-space AABB min [x, y, z, 0] */
    VECTOR aabb_max;       /* world-space AABB max [x, y, z, 0] */

    AthenaGeomType       type;
    int                  aabb_dirty;
    AthenaPhysicsBody   *body;
    AthenaPhysicsSpace  *space;
    AthenaPhysicsGeom   *next;
    AthenaPhysicsGeom   *prev;
    void                *user_data;
    AthenaPhysicsTriMesh *trimesh;
    AthenaPhysicsSpace   *subspace;

    union {
        struct {
            float radius;
        } sphere;

        struct {
            VECTOR half_extents; /* [hx, hy, hz, 0] */
        } box;

        struct {
            VECTOR normal;       /* [a, b, c, d] where ax + by + cz = d */
        } plane;

        struct {
            VECTOR dir;          /* unit direction [dx, dy, dz, 0] */
            float  length;
            int    first_contact;
            int    backface_cull;
            int    closest_hit;
        } ray;

        struct {
            AthenaPhysicsGeom *child;
        } transform;
    };
} __attribute__((aligned(16)));

struct AthenaPhysicsSpace {
    AthenaPhysicsSpace *parent;
    AthenaPhysicsGeom   geom_wrapper; /* allows a space to be inserted into a parent space */
    AthenaPhysicsGeom  *geoms;
    uint32_t            geom_count;
} __attribute__((aligned(16)));

struct AthenaPhysicsJoint {
    AthenaJointType          type;
    AthenaPhysicsWorld      *world;
    AthenaPhysicsJointGroup *group;
    AthenaPhysicsBody       *body1;
    AthenaPhysicsBody       *body2;

    VECTOR anchor1_local;  /* local anchor on body1 (or world if body1 == NULL) */
    VECTOR anchor2_local;  /* local anchor on body2 (or world if body2 == NULL) */
    VECTOR axis1_local;    /* primary axis */
    VECTOR axis2_local;    /* secondary axis */
    VECTOR axis3_local;    /* tertiary axis (AMotor) */
    VECTOR qrel_init;      /* initial relative orientation for fixed/hinge/universal */

    float  motor_torque[3];
    float  amotor_angles[3];
    int    amotor_rel[3];
    int    amotor_num_axes;
    int    amotor_mode;

    AthenaContact       contact; /* used when type == ATH_JOINT_CONTACT */
    bool                from_pool;

    AthenaPhysicsJoint *next;
    AthenaPhysicsJoint *prev;
    AthenaPhysicsJoint *group_next;
} __attribute__((aligned(16)));

struct AthenaPhysicsJointGroup {
    AthenaPhysicsJoint *joints;
    uint32_t            count;
    AthenaPhysicsJoint *pool;
    uint32_t            pool_cap;
    uint32_t            pool_used;
};

struct AthenaPhysicsWorld {
    VECTOR              gravity;    /* [gx, gy, gz, 0] */
    float               cfm;
    float               erp;
    int                 solver_iterations;
    AthenaPhysicsBody  *bodies;
    AthenaPhysicsJoint *joints;
    uint32_t            body_count;
    uint32_t            joint_count;
    void               *solver_rows;
    int                 solver_rows_cap;
} __attribute__((aligned(16)));

/* Fast raycast result for shadow projectors and direct ray queries */
typedef struct AthenaRaycastHit {
    union {
        VECTOR pos;
        VECTOR position;
    };
    VECTOR   normal;
    float    depth;
    int      has_hit;
    AthenaPhysicsGeom *geom;
    int      tri_index;
} AthenaRaycastHit __attribute__((aligned(16)));

/* Core lifecycle */
void athena_physics_cleanup(void);

/* World */
AthenaPhysicsWorld *athena_physics_world_create(void);
void athena_physics_world_destroy(AthenaPhysicsWorld *world);
void athena_physics_world_set_gravity(AthenaPhysicsWorld *world, float x, float y, float z);
void athena_physics_world_get_gravity(const AthenaPhysicsWorld *world, VECTOR out);
void athena_physics_world_set_cfm(AthenaPhysicsWorld *world, float cfm);
float athena_physics_world_get_cfm(const AthenaPhysicsWorld *world);
void athena_physics_world_set_erp(AthenaPhysicsWorld *world, float erp);
float athena_physics_world_get_erp(const AthenaPhysicsWorld *world);
void athena_physics_world_step(AthenaPhysicsWorld *world, float step_size);
void athena_physics_world_quick_step(AthenaPhysicsWorld *world, float step_size);
void athena_physics_world_set_quick_step_iterations(AthenaPhysicsWorld *world, int iterations);

/* Space */
AthenaPhysicsSpace *athena_physics_space_create(AthenaPhysicsSpace *parent);
void athena_physics_space_destroy(AthenaPhysicsSpace *space);
void athena_physics_space_add(AthenaPhysicsSpace *space, AthenaPhysicsGeom *geom);
void athena_physics_space_remove(AthenaPhysicsSpace *space, AthenaPhysicsGeom *geom);
void athena_physics_space_collide(AthenaPhysicsSpace *space, void *data, AthenaNearCallback callback);
void athena_physics_space_collide2(AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2, void *data, AthenaNearCallback callback);

/* Fast direct raycast against space (bypasses callback overhead for shadow grids & game queries) */
int athena_physics_space_raycast(AthenaPhysicsSpace *space, const VECTOR origin, const VECTOR dir,
                                 float max_len, int backface_cull, AthenaRaycastHit *out_hit);

/* Body */
AthenaPhysicsBody *athena_physics_body_create(AthenaPhysicsWorld *world);
void athena_physics_body_destroy(AthenaPhysicsBody *body);
void athena_physics_body_set_position(AthenaPhysicsBody *body, float x, float y, float z);
void athena_physics_body_get_position(const AthenaPhysicsBody *body, float *x, float *y, float *z);
void athena_physics_body_set_rotation(AthenaPhysicsBody *body, const float *r9_or_r12);
void athena_physics_body_get_rotation(const AthenaPhysicsBody *body, float *out_r9);
void athena_physics_body_set_linear_vel(AthenaPhysicsBody *body, float x, float y, float z);
void athena_physics_body_get_linear_vel(const AthenaPhysicsBody *body, float *x, float *y, float *z);
void athena_physics_body_set_angular_vel(AthenaPhysicsBody *body, float x, float y, float z);
void athena_physics_body_get_angular_vel(const AthenaPhysicsBody *body, float *x, float *y, float *z);
void athena_physics_body_set_mass(AthenaPhysicsBody *body, float mass);
void athena_physics_body_set_mass_box(AthenaPhysicsBody *body, float density_or_mass, float lx, float ly, float lz);
void athena_physics_body_set_mass_sphere(AthenaPhysicsBody *body, float density_or_mass, float radius);
void athena_physics_body_add_force(AthenaPhysicsBody *body, float fx, float fy, float fz);
void athena_physics_body_add_torque(AthenaPhysicsBody *body, float tx, float ty, float tz);
void athena_physics_body_enable(AthenaPhysicsBody *body);
void athena_physics_body_disable(AthenaPhysicsBody *body);
bool athena_physics_body_is_enabled(const AthenaPhysicsBody *body);
int  athena_physics_bodies_connected_excluding(const AthenaPhysicsBody *b1, const AthenaPhysicsBody *b2, AthenaJointType exclude_type);

/* Geometry */
AthenaPhysicsGeom *athena_physics_geom_create_sphere(AthenaPhysicsSpace *space, float radius);
AthenaPhysicsGeom *athena_physics_geom_create_box(AthenaPhysicsSpace *space, float lx, float ly, float lz);
AthenaPhysicsGeom *athena_physics_geom_create_plane(AthenaPhysicsSpace *space, float a, float b, float c, float d);
AthenaPhysicsGeom *athena_physics_geom_create_ray(AthenaPhysicsSpace *space, float length);
AthenaPhysicsGeom *athena_physics_geom_create_transform(AthenaPhysicsSpace *space, AthenaPhysicsGeom *child);
AthenaPhysicsGeom *athena_physics_geom_create_trimesh(AthenaPhysicsSpace *space, const VECTOR *verts, uint32_t vert_count, const uint32_t *indices, uint32_t index_count);
AthenaPhysicsGeom *athena_physics_geom_create_from_render_object(AthenaPhysicsSpace *space, athena_object_data *obj);
void athena_physics_geom_destroy(AthenaPhysicsGeom *geom);

void athena_physics_geom_set_position(AthenaPhysicsGeom *geom, float x, float y, float z);
void athena_physics_geom_get_position(const AthenaPhysicsGeom *geom, float *x, float *y, float *z);
void athena_physics_geom_set_rotation(AthenaPhysicsGeom *geom, const float *r9_or_r12);
void athena_physics_geom_get_rotation(const AthenaPhysicsGeom *geom, float *out_r9);
void athena_physics_geom_set_body(AthenaPhysicsGeom *geom, AthenaPhysicsBody *body);
AthenaPhysicsBody *athena_physics_geom_get_body(const AthenaPhysicsGeom *geom);
void athena_physics_geom_set_data(AthenaPhysicsGeom *geom, void *data);
void *athena_physics_geom_get_data(const AthenaPhysicsGeom *geom);

/* Ray geometry helpers */
void  athena_physics_ray_set_length(AthenaPhysicsGeom *ray, float length);
float athena_physics_ray_get_length(const AthenaPhysicsGeom *ray);
void  athena_physics_ray_set(AthenaPhysicsGeom *ray, float px, float py, float pz, float dx, float dy, float dz);
void  athena_physics_ray_get(const AthenaPhysicsGeom *ray, VECTOR start, VECTOR dir);
void  athena_physics_ray_set_params(AthenaPhysicsGeom *ray, int first_contact, int backface_cull);
void  athena_physics_ray_get_params(const AthenaPhysicsGeom *ray, int *first_contact, int *backface_cull);
void  athena_physics_ray_set_closest_hit(AthenaPhysicsGeom *ray, int closest_hit);
int   athena_physics_ray_get_closest_hit(const AthenaPhysicsGeom *ray);

/* Narrowphase collision */
int athena_physics_collide(AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2, int max_contacts, AthenaContact *contacts);

/* Joints & JointGroup */
AthenaPhysicsJointGroup *athena_physics_joint_group_create(void);
void athena_physics_joint_group_empty(AthenaPhysicsJointGroup *group);
void athena_physics_joint_group_destroy(AthenaPhysicsJointGroup *group);

AthenaPhysicsJoint *athena_physics_joint_create(AthenaPhysicsWorld *world, AthenaPhysicsJointGroup *group, AthenaJointType type);
AthenaPhysicsJoint *athena_physics_joint_create_contact(AthenaPhysicsWorld *world, AthenaPhysicsJointGroup *group, const AthenaContact *contact);
void athena_physics_joint_destroy(AthenaPhysicsJoint *joint);
void athena_physics_joint_attach(AthenaPhysicsJoint *joint, AthenaPhysicsBody *b1, AthenaPhysicsBody *b2);
AthenaPhysicsBody *athena_physics_joint_get_body(const AthenaPhysicsJoint *joint, int index);

void  athena_physics_joint_set_ball_anchor(AthenaPhysicsJoint *joint, float x, float y, float z);
void  athena_physics_joint_get_ball_anchor(const AthenaPhysicsJoint *joint, VECTOR out);
void  athena_physics_joint_get_ball_anchor2(const AthenaPhysicsJoint *joint, VECTOR out);

void  athena_physics_joint_set_hinge_anchor(AthenaPhysicsJoint *joint, float x, float y, float z);
void  athena_physics_joint_set_hinge_axis(AthenaPhysicsJoint *joint, float x, float y, float z);
void  athena_physics_joint_add_hinge_torque(AthenaPhysicsJoint *joint, float torque);
void  athena_physics_joint_get_hinge_anchor(const AthenaPhysicsJoint *joint, VECTOR out);
void  athena_physics_joint_get_hinge_anchor2(const AthenaPhysicsJoint *joint, VECTOR out);
void  athena_physics_joint_get_hinge_axis(const AthenaPhysicsJoint *joint, VECTOR out);
float athena_physics_joint_get_hinge_angle(const AthenaPhysicsJoint *joint);
float athena_physics_joint_get_hinge_angle_rate(const AthenaPhysicsJoint *joint);

void  athena_physics_joint_set_slider_axis(AthenaPhysicsJoint *joint, float x, float y, float z);
void  athena_physics_joint_add_slider_force(AthenaPhysicsJoint *joint, float force);
void  athena_physics_joint_get_slider_axis(const AthenaPhysicsJoint *joint, VECTOR out);
float athena_physics_joint_get_slider_position(const AthenaPhysicsJoint *joint);
float athena_physics_joint_get_slider_position_rate(const AthenaPhysicsJoint *joint);

void  athena_physics_joint_set_hinge2_anchor(AthenaPhysicsJoint *joint, float x, float y, float z);
void  athena_physics_joint_set_hinge2_axis1(AthenaPhysicsJoint *joint, float x, float y, float z);
void  athena_physics_joint_set_hinge2_axis2(AthenaPhysicsJoint *joint, float x, float y, float z);
void  athena_physics_joint_add_hinge2_torques(AthenaPhysicsJoint *joint, float t1, float t2);
void  athena_physics_joint_get_hinge2_anchor(const AthenaPhysicsJoint *joint, VECTOR out);
void  athena_physics_joint_get_hinge2_anchor2(const AthenaPhysicsJoint *joint, VECTOR out);
void  athena_physics_joint_get_hinge2_axis1(const AthenaPhysicsJoint *joint, VECTOR out);
void  athena_physics_joint_get_hinge2_axis2(const AthenaPhysicsJoint *joint, VECTOR out);
float athena_physics_joint_get_hinge2_angle1(const AthenaPhysicsJoint *joint);
float athena_physics_joint_get_hinge2_angle1_rate(const AthenaPhysicsJoint *joint);
float athena_physics_joint_get_hinge2_angle2_rate(const AthenaPhysicsJoint *joint);

void  athena_physics_joint_set_universal_anchor(AthenaPhysicsJoint *joint, float x, float y, float z);
void  athena_physics_joint_set_universal_axis1(AthenaPhysicsJoint *joint, float x, float y, float z);
void  athena_physics_joint_set_universal_axis2(AthenaPhysicsJoint *joint, float x, float y, float z);
void  athena_physics_joint_add_universal_torques(AthenaPhysicsJoint *joint, float t1, float t2);
void  athena_physics_joint_get_universal_anchor(const AthenaPhysicsJoint *joint, VECTOR out);
void  athena_physics_joint_get_universal_anchor2(const AthenaPhysicsJoint *joint, VECTOR out);
void  athena_physics_joint_get_universal_axis1(const AthenaPhysicsJoint *joint, VECTOR out);
void  athena_physics_joint_get_universal_axis2(const AthenaPhysicsJoint *joint, VECTOR out);
float athena_physics_joint_get_universal_angle1(const AthenaPhysicsJoint *joint);
float athena_physics_joint_get_universal_angle2(const AthenaPhysicsJoint *joint);
float athena_physics_joint_get_universal_angle1_rate(const AthenaPhysicsJoint *joint);
float athena_physics_joint_get_universal_angle2_rate(const AthenaPhysicsJoint *joint);

void  athena_physics_joint_set_fixed(AthenaPhysicsJoint *joint);

void  athena_physics_joint_set_amotor_num_axes(AthenaPhysicsJoint *joint, int num);
void  athena_physics_joint_set_amotor_axis(AthenaPhysicsJoint *joint, int anum, int rel, float x, float y, float z);
void  athena_physics_joint_set_amotor_angle(AthenaPhysicsJoint *joint, int anum, float angle);
void  athena_physics_joint_set_amotor_mode(AthenaPhysicsJoint *joint, int mode);
void  athena_physics_joint_add_amotor_torques(AthenaPhysicsJoint *joint, float t1, float t2, float t3);
int   athena_physics_joint_get_amotor_num_axes(const AthenaPhysicsJoint *joint);
void  athena_physics_joint_get_amotor_axis(const AthenaPhysicsJoint *joint, int anum, VECTOR out);
int   athena_physics_joint_get_amotor_axis_rel(const AthenaPhysicsJoint *joint, int anum);
float athena_physics_joint_get_amotor_angle(const AthenaPhysicsJoint *joint, int anum);
float athena_physics_joint_get_amotor_angle_rate(const AthenaPhysicsJoint *joint, int anum);
int   athena_physics_joint_get_amotor_mode(const AthenaPhysicsJoint *joint);

/* RenderObject sync hooks */
void athena_physics_update_geom_pos_rot(athena_object_data *obj);
void athena_physics_update_body_pos_rot(athena_object_data *obj);

#ifdef __cplusplus
}
#endif

#endif /* ATHENA_PHYSICS_H */
