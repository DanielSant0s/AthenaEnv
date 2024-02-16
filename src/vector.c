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


float v3dlength(Vector3D *v) {
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

void v3dsetLength(Vector3D *v, float newLength) {
    v3dnormalize(v);
    v->x *= newLength;
    v->y *= newLength;
    v->z *= newLength;
}
