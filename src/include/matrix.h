#ifndef MATRIX_H
#define MATRIX_H

#include <render.h>
#include <math.h>

typedef struct {
    void (*identity)(MATRIX);
    void (*multiply)(MATRIX, MATRIX, MATRIX);
    void (*inverse)(MATRIX, MATRIX);
    void (*apply)(VECTOR, MATRIX, VECTOR);
    void (*transpose)(MATRIX, MATRIX);
    void (*copy)(MATRIX, MATRIX);

    void (*translate)(MATRIX, MATRIX, VECTOR);
    void (*rotate)(MATRIX, MATRIX, VECTOR);
    void (*scale)(MATRIX, MATRIX, VECTOR);

    // (out, position, rotation quaternion, scale) -- fused TRS, column-vector
    // layout (translation in .w of rows 0..2), same as create_transform_matrix.
    void (*from_trs)(MATRIX, VECTOR, VECTOR, VECTOR);

    // (out, position, euler rotation, scale) -- fused scale*Rz*Ry*Rx with the
    // translation in row 3, replacing identity/rotate/scale/translate. Reduces
    // the rotation in place, as rotate() did.
    void (*trs_euler)(MATRIX, VECTOR, VECTOR, VECTOR);

    int (*equals)(MATRIX, MATRIX);
} matrix_ops;

extern matrix_ops *matrix_functions;

// Merges into lo/hi the AABB of box[0..7] transformed by each of the
// bone_count matrices in palette. Caller seeds lo/hi.
void vu0_bounds_from_palette(VECTOR lo, VECTOR hi, MATRIX *palette, uint32_t bone_count, VECTOR *box);

#endif