#ifndef ATHENA_PHYSICS_FACADE_H
#define ATHENA_PHYSICS_FACADE_H

#include <stdbool.h>
#include <physics.h>
#include <render.h>

void athena_physics_world_get_gravity_vec(const AthenaPhysicsWorld *world, float out[3]);
void athena_physics_body_get_position_vec(const AthenaPhysicsBody *body, float out[3]);
void athena_physics_body_get_linear_vel_vec(const AthenaPhysicsBody *body, float out[3]);
void athena_physics_body_get_angular_vel_vec(const AthenaPhysicsBody *body, float out[3]);

#endif /* ATHENA_PHYSICS_FACADE_H */
