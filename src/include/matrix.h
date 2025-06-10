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

    int (*equals)(MATRIX, MATRIX);
} matrix_ops;

extern matrix_ops *matrix_functions;

#endif