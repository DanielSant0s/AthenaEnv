#ifndef ATHENA_SPRITE_DATA_H
#define ATHENA_SPRITE_DATA_H
#include <stdint.h>
/* Four qwords consumed by the VU1 tile-list microprogram. */
typedef struct {
    float x, y, w, h;
    float u1, v1, u2, v2;
    uint32_t r, g, b, a;
    uint32_t pad0, pad1;
    float zindex;
    uint32_t pad2;
} athena_sprite_data;
#endif
