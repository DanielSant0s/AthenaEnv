#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>

#include <render.h>
#include <matrix.h>
#include <vector.h>
#include <dbgprintf.h>

VECTOR camera_position = { 0.00f, 0.00f, 0.00f, 0.00f };
VECTOR camera_target =   { 0.00f, 0.00f, 0.00f, 0.00f };
VECTOR camera_yd =       { 0.00f, 1.00f, 0.00f, 0.00f };
VECTOR camera_zd =       { 0.00f, 0.00f, 1.00f, 0.00f };
VECTOR camera_up =       { 0.00f, 1.00f, 0.00f, 0.00f };
VECTOR local_up =        { 0.00f, 1.00f, 0.00f, 1.00f };

static MATRIX *world_view;
static MATRIX *world_screen;
static MATRIX *view_screen;

void initCamera(MATRIX *ws, MATRIX *wv, MATRIX *vs) {
	world_screen = ws;
    world_view = wv;
	view_screen = vs;
}

VECTOR* getCameraPosition() {
    return &camera_position;
}

void cameraUpdate() {
	LookAtCameraMatrix(world_view, camera_position, camera_target, camera_up);

	matrix_functions->multiply(world_screen, world_view, view_screen);
}

void cameraSave(athena_camera_state *out)
{
    if (!out) return;
    copy_vector(out->position, camera_position);
    copy_vector(out->target,   camera_target);
    copy_vector(out->up,       camera_up);
    copy_vector(out->local_up, local_up);
}

void cameraRestore(const athena_camera_state *state)
{
    if (!state) return;
    copy_vector(camera_position, state->position);
    copy_vector(camera_target,   state->target);
    copy_vector(camera_up,       state->up);
    copy_vector(local_up,        state->local_up);
}

void setCameraPosition(float x, float y, float z){
	camera_position[0] = x;
	camera_position[1] = y;
	camera_position[2] = z;
	camera_position[3] = 0.00f;
}

void setCameraTarget(float x, float y, float z){
	VECTOR tmp_target = { x, y, z, 0.0f };

	vector_functions->sub(camera_target, camera_target, tmp_target);
	vector_functions->sub(camera_position, camera_position, camera_target);

	camera_target[0] = x;
	camera_target[1] = y;
	camera_target[2] = z;
	camera_target[3] = 0.00f;
}

typedef struct {
    float w, x, y, z;
} Quat;

static Quat Quat_rotation(float rotation, VECTOR axis) {
    Quat result;

	vector_functions->normalize(axis, axis);

    float halfAngle = rotation * 0.5f;
    float sinHalfAngle = sinf(halfAngle);

    result.w = cosf(halfAngle);
    result.x = axis[0] * sinHalfAngle;
    result.y = axis[1] * sinHalfAngle;
    result.z = axis[2] * sinHalfAngle;

    return result;
}

static void rotate(VECTOR output, VECTOR input, Quat r) {
    Quat v = {0.0f, input[0], input[1], input[2]};

    Quat conjR = {r.w, -r.x, -r.y, -r.z};

    Quat rotatedV = {
        v.w * conjR.w - v.x * conjR.x - v.y * conjR.y - v.z * conjR.z,
        v.w * conjR.x + v.x * conjR.w + v.y * conjR.z - v.z * conjR.y,
        v.w * conjR.y - v.x * conjR.z + v.y * conjR.w + v.z * conjR.x,
        v.w * conjR.z + v.x * conjR.y - v.y * conjR.x + v.z * conjR.w
    };

    output[0] = rotatedV.x;
	output[1] = rotatedV.y;
	output[2] = rotatedV.z;
}

void turnCamera(float yaw, float pitch)
{
	VECTOR dir;
	vector_functions->sub(dir, camera_target, camera_position);

	VECTOR dy = {0.0f, 1.0f, 0.0f, 1.0f};

	Quat r = Quat_rotation(yaw, dy);
	rotate(dir, dir, r);
	rotate(local_up, local_up, r);

	VECTOR right;
	vector_functions->cross(right, dir, local_up);
	vector_functions->normalize(right, right);

	r = Quat_rotation(pitch, right);
	rotate(dir, dir, r);

	vector_functions->cross(local_up, right, dir);
	vector_functions->normalize(local_up, local_up);

	if(local_up[1] >= 0.0f) {
		camera_up[1] = 1.0f; 
	} else {
		camera_up[1] = -1.0f;
	}

	vector_functions->add(camera_target, camera_position, dir);
}

void orbitCamera(float yaw, float pitch)
{
	VECTOR dir;
	vector_functions->sub(dir, camera_target, camera_position);

	VECTOR dy = {0.0f, 1.0f, 0.0f, 1.0f};

	Quat r = Quat_rotation(yaw, dy);
	rotate(dir, dir, r);
	rotate(local_up, local_up, r);

	VECTOR right;
	vector_functions->cross(right, dir, local_up);
	vector_functions->normalize(right, right);

	r = Quat_rotation(-pitch, right);
	rotate(dir, dir, r);

	vector_functions->cross(local_up, right, dir);
	vector_functions->normalize(local_up, local_up);

	if(local_up[1] >= 0.0f) {
		camera_up[1] = 1.0f; 
	} else {
		camera_up[1] = -1.0f;
	}

	vector_functions->sub(camera_position, camera_target, dir);
}

void dollyCamera(float dist)
{
	VECTOR dir;
	vector_functions->sub(dir, camera_target, camera_position);
	vector_functions->set_length(dir, dist);

	vector_functions->add(camera_position, camera_position, dir);
	vector_functions->add(camera_target, camera_target, dir);
}

void zoomCamera(float dist)
{
	VECTOR dir;
	vector_functions->sub(dir, camera_target, camera_position);
	float curdist = vector_functions->get_length(dir);

	if(dist >= curdist)
		dist = curdist - 0.01f;

	vector_functions->set_length(dir, dist);

	vector_functions->add(camera_position, camera_position, dir);
}

void panCamera(float x, float y)
{
	VECTOR dir;
	vector_functions->sub(dir, camera_target, camera_position);
	vector_functions->normalize(dir, dir);

	VECTOR right;
	vector_functions->cross(right, dir, camera_up);
	vector_functions->normalize(right, right);
	
	vector_functions->cross(local_up, right, dir);
	vector_functions->normalize(local_up, local_up);

	VECTOR work0, work1;
	vector_functions->scale(work0, right, x);
	vector_functions->scale(work1, local_up, y);

	vector_functions->add(dir, work0, work1);

	vector_functions->add(camera_position, camera_position, dir);
	vector_functions->add(camera_target, camera_target, dir);
}
