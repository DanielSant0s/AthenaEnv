#ifndef VECTOR_H
#define VECTOR_H

#include <render.h>
#include <math.h>

typedef struct {
    void (*add)(VECTOR, VECTOR, VECTOR);
    void (*sub)(VECTOR, VECTOR, VECTOR);
    void (*mul)(VECTOR, VECTOR, VECTOR);
    void (*div)(VECTOR, VECTOR, VECTOR);

    float (*dot)(VECTOR, VECTOR);
    void (*cross)(VECTOR, VECTOR, VECTOR);

    float (*get_length)(VECTOR);
    void (*set_length)(VECTOR, float);

    void (*normalize)(VECTOR, VECTOR);
    void (*scale)(VECTOR, VECTOR, float);
    void (*clamp)(VECTOR, VECTOR, float, float);
    void (*interpolate)(VECTOR, VECTOR, VECTOR, float);

    int (*equals)(VECTOR, VECTOR);
    void (*copy)(VECTOR, VECTOR);
} vector_ops;

extern vector_ops *vector_functions;

#endif