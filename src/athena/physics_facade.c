#include <stdlib.h>
#include <string.h>

#include <athena/physics_facade.h>

void athena_physics_world_get_gravity_vec(const AthenaPhysicsWorld *world, float out[3])
{
    VECTOR g;
    if (!world || !out)
        return;

    athena_physics_world_get_gravity(world, g);
    out[0] = g[0];
    out[1] = g[1];
    out[2] = g[2];
}

void athena_physics_body_get_position_vec(const AthenaPhysicsBody *body, float out[3])
{
    if (!body || !out)
        return;

    athena_physics_body_get_position(body, &out[0], &out[1], &out[2]);
}

void athena_physics_body_get_linear_vel_vec(const AthenaPhysicsBody *body, float out[3])
{
    if (!body || !out)
        return;

    athena_physics_body_get_linear_vel(body, &out[0], &out[1], &out[2]);
}

void athena_physics_body_get_angular_vel_vec(const AthenaPhysicsBody *body, float out[3])
{
    if (!body || !out)
        return;

    athena_physics_body_get_angular_vel(body, &out[0], &out[1], &out[2]);
}
