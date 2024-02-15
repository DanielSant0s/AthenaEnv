#include <math.h>
#include "include/vector.h"

void v3dnormalize(Vector3D *v) {
    float length = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
    if (length != 0) {
        v->x /= length;
        v->y /= length;
        v->z /= length;
    }
}

Vector3D v3dcross(Vector3D *v1, Vector3D *v2) {
    Vector3D result;
    result.x = v1->y * v2->z - v1->z * v2->y;
    result.y = v1->z * v2->x - v1->x * v2->z;
    result.z = v1->x * v2->y - v1->y * v2->x;
    result.w = 1.0f;
    return result;
}

Vector3D v3dsub(Vector3D *v1, Vector3D *v2) {
    Vector3D result;
    result.x = v1->x - v2->x;
    result.y = v1->y - v2->y;
    result.z = v1->z - v2->z;
    result.w = 1.0f;
    return result;
}

Vector3D v3dadd(Vector3D *v1, Vector3D *v2) {
    Vector3D result;
    result.x = v1->x + v2->x;
    result.y = v1->y + v2->y;
    result.z = v1->z + v2->z;
    result.w = 1.0f;
    return result;
}

float v3dlength(Vector3D *v) {
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

void v3dsetLength(Vector3D *v, float newLength) {
    v3dnormalize(v);
    v->x *= newLength;
    v->y *= newLength;
    v->z *= newLength;
}
