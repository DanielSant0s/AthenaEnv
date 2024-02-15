#ifndef ATHENA_VECTORS_H
#define ATHENA_VECTORS_H

typedef struct {
    float x;
    float y;
    float z;
    float w; 
} Vector3D;

void v3dnormalize(Vector3D *v);
Vector3D v3dcross(Vector3D *v1, Vector3D *v2);
Vector3D v3dsub(Vector3D *v1, Vector3D *v2);
Vector3D v3dadd(Vector3D *v1, Vector3D *v2);
float v3dlength(Vector3D *v);
void v3dsetLength(Vector3D *v, float newLength);

#endif