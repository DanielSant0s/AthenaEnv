#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>

#include "include/render.h"

#include "include/dbgprintf.h"

eCameraTypes camera_type = CAMERA_DEFAULT;

VECTOR camera_position = { 0.00f, 0.00f, 0.00f, 0.00f };
VECTOR camera_target =   { 0.00f, 0.00f, 0.00f, 0.00f };
VECTOR camera_yd =       { 0.00f, 1.00f, 0.00f, 0.00f };
VECTOR camera_zd =       { 0.00f, 0.00f, 1.00f, 0.00f };
VECTOR camera_up =       { 0.00f, 1.00f, 0.00f, 0.00f };
VECTOR camera_rotation = { 0.00f, 0.00f, 0.00f, 0.00f };
VECTOR local_up =        { 0.00f, 1.00f, 0.00f, 1.00f };

static MATRIX *world_view;

void initCamera(MATRIX* wv) {
    world_view = wv;
}

VECTOR* getCameraPosition() {
    return &camera_position;
}

void setCameraType(eCameraTypes type) {
	camera_type = type;
}

void cameraUpdate() {
	switch (camera_type) {
		case CAMERA_DEFAULT:
			RotCameraMatrix(world_view, camera_position, camera_zd, camera_yd, camera_rotation);
			break;
		case CAMERA_LOOKAT:
			LookAtCameraMatrix(world_view, camera_position, camera_target, camera_up);
			break;
	}
	dbgprintf("Camera matrix:\n");
	dbgprintf("%f %f %f %f\n", world_view[0], world_view[1], world_view[2], world_view[3]);
	dbgprintf("%f %f %f %f\n", world_view[4], world_view[5], world_view[6], world_view[7]);
	dbgprintf("%f %f %f %f\n", world_view[8], world_view[9], world_view[10], world_view[11]);
	dbgprintf("%f %f %f %f\n", world_view[12], world_view[13], world_view[14], world_view[15]);
}

void setCameraPosition(float x, float y, float z){
	camera_position[0] = x;
	camera_position[1] = y;
	camera_position[2] = z;
	camera_position[3] = 0.00f;
}

void setCameraTarget(float x, float y, float z){
	VECTOR tmp_target = { x, y, z, 0.0f };

	SubVector(camera_target, camera_target, tmp_target);
	SubVector(camera_position, camera_position, camera_target);

	camera_target[0] = x;
	camera_target[1] = y;
	camera_target[2] = z;
	camera_target[3] = 0.00f;
}

void setCameraRotation(float x, float y, float z){
	camera_rotation[0] = x;
	camera_rotation[1] = y;
	camera_rotation[2] = z;
	camera_rotation[3] = 1.00f;
}

typedef struct {
    float w, x, y, z;
} Quat;

static Quat Quat_rotation(float rotation, VECTOR axis) {
    Quat result;

	Normalize(axis, axis);

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
	SubVector(dir, camera_target, camera_position);

	VECTOR dy = {0.0f, 1.0f, 0.0f, 1.0f};

	Quat r = Quat_rotation(yaw, dy);
	rotate(dir, dir, r);
	rotate(local_up, local_up, r);

	VECTOR right;
	OuterProduct(right, dir, local_up);
	Normalize(right, right);

	r = Quat_rotation(pitch, right);
	rotate(dir, dir, r);

	OuterProduct(local_up, right, dir);
	Normalize(local_up, local_up);

	if(local_up[1] >= 0.0f) {
		camera_up[1] = 1.0f; 
	} else {
		camera_up[1] = -1.0f;
	}

	AddVector(camera_target, camera_position, dir);
}

void orbitCamera(float yaw, float pitch)
{
	VECTOR dir;
	SubVector(dir, camera_target, camera_position);

	VECTOR dy = {0.0f, 1.0f, 0.0f, 1.0f};

	Quat r = Quat_rotation(yaw, dy);
	rotate(dir, dir, r);
	rotate(local_up, local_up, r);

	VECTOR right;
	OuterProduct(right, dir, local_up);
	Normalize(right, right);

	r = Quat_rotation(-pitch, right);
	rotate(dir, dir, r);

	OuterProduct(local_up, right, dir);
	Normalize(local_up, local_up);

	if(local_up[1] >= 0.0f) {
		camera_up[1] = 1.0f; 
	} else {
		camera_up[1] = -1.0f;
	}

	SubVector(camera_position, camera_target, dir);
}

void dollyCamera(float dist)
{
	VECTOR dir;
	SubVector(dir, camera_target, camera_position);
	SetLenVector(dir, dist);

	AddVector(camera_position, camera_position, dir);
	AddVector(camera_target, camera_target, dir);
}

void zoomCamera(float dist)
{
	VECTOR dir;
	SubVector(dir, camera_target, camera_position);
	float curdist = LenVector(dir);

	if(dist >= curdist)
		dist = curdist - 0.01f;

	SetLenVector(dir, dist);

	AddVector(camera_position, camera_position, dir);
}

void panCamera(float x, float y)
{
	VECTOR dir;
	SubVector(dir, camera_target, camera_position);
	Normalize(dir, dir);

	VECTOR right;
	OuterProduct(right, dir, camera_up);
	Normalize(right, right);
	
	OuterProduct(local_up, right, dir);
	Normalize(local_up, local_up);

	VECTOR work0, work1;
	ScaleVector(work0, right, x);
	ScaleVector(work1, local_up, y);

	AddVector(dir, work0, work1);

	AddVector(camera_position, camera_position, dir);
	AddVector(camera_target, camera_target, dir);
}
