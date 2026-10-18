#ifndef ATHENA_IMAGE_H
#define ATHENA_IMAGE_H

#include <stdbool.h>
#include <stdint.h>

#include <graphics.h>

typedef struct AthenaImage {
    const char *path;
    GSSURFACE *tex;
    bool delayed;
    bool loaded;
    bool owns_surface;
    Color color;
    float width;
    float height;
    float startx;
    float starty;
    float endx;
    float endy;
    float angle;
} AthenaImage;

AthenaImage *athena_image_create(const char *path, bool delayed);
AthenaImage *athena_image_create_empty(bool delayed);
AthenaImage *athena_image_wrap(GSSURFACE *tex, bool delayed);
void athena_image_destroy(AthenaImage *image);
void athena_image_free(AthenaImage *image);

bool athena_image_is_loaded(const AthenaImage *image);
void athena_image_draw(
                        AthenaImage *image, 
                        float x, float y,
                        float width, float height,
                        float startx, float starty,
                        float endx, float endy,
                        float angle, uint32_t color
);
bool athena_image_lock(AthenaImage *image);
bool athena_image_unlock(AthenaImage *image);
bool athena_image_locked(const AthenaImage *image);
bool athena_image_optimize(AthenaImage *image);
void athena_image_copy_vram_block(AthenaImage *src, int src_x, int src_y,
                                  AthenaImage *dst, int dst_x, int dst_y);

void athena_image_set_dimensions(AthenaImage *image, float width, float height);
void athena_image_set_src_rect(AthenaImage *image, float startx, float starty, float endx, float endy);
void athena_image_set_angle(AthenaImage *image, float angle);
void athena_image_set_color(AthenaImage *image, Color color);
void athena_image_set_filter(AthenaImage *image, uint32_t filter);

#endif /* ATHENA_IMAGE_H */
