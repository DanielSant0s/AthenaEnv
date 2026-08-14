#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <athena/image.h>
#include <graphics.h>
#include <texture_manager.h>

static GSSURFACE *athena_image_alloc_surface(void) {
    GSSURFACE *tex = calloc(1, sizeof(GSSURFACE));
    if (!tex)
        return NULL;

    tex->Delayed = true;
    tex->Filter = GS_FILTER_NEAREST;
    return tex;
}

AthenaImage *athena_image_create_empty(bool delayed) {
    AthenaImage *image = calloc(1, sizeof(AthenaImage));
    if (!image)
        return NULL;

    image->tex = athena_image_alloc_surface();
    if (!image->tex) {
        free(image);
        return NULL;
    }

    image->delayed = delayed;
    image->loaded = false;
    image->owns_surface = true;
    image->startx = 0.0f;
    image->starty = 0.0f;
    image->angle = 0.0f;
    image->color = 0x80808080;
    return image;
}

AthenaImage *athena_image_create(const char *path, bool delayed) {
    AthenaImage *image = athena_image_create_empty(delayed);
    if (!image)
        return NULL;

    if (path) {
        image->path = path;
        load_image(image->tex, path, delayed);
        image->loaded = true;
        image->width = (float)image->tex->Width;
        image->height = (float)image->tex->Height;
        image->endx = (float)image->tex->Width;
        image->endy = (float)image->tex->Height;
    }

    return image;
}

AthenaImage *athena_image_wrap(GSSURFACE *tex, bool delayed) {
    AthenaImage *image = calloc(1, sizeof(AthenaImage));
    if (!image)
        return NULL;

    image->tex = tex;
    image->delayed = delayed;
    image->loaded = true;
    image->owns_surface = false;
    if (tex) {
        image->width = (float)tex->Width;
        image->height = (float)tex->Height;
        image->endx = (float)tex->Width;
        image->endy = (float)tex->Height;
    }
    image->startx = 0.0f;
    image->starty = 0.0f;
    image->angle = 0.0f;
    image->color = 0x80808080;
    return image;
}

void athena_image_destroy(AthenaImage *image) {
    if (!image)
        return;

    if (image->tex && image->owns_surface) {
        texture_manager_free(image->tex);
        if (image->tex->Mem) {
            free(image->tex->Mem);
            image->tex->Mem = NULL;
        }
        if (image->tex->Clut) {
            free(image->tex->Clut);
            image->tex->Clut = NULL;
        }
        free(image->tex);
        image->tex = NULL;
    }

    free(image);
}

void athena_image_free(AthenaImage *image) {
    athena_image_destroy(image);
}

bool athena_image_is_loaded(const AthenaImage *image) {
    return image && image->loaded;
}

void athena_image_draw(
                        AthenaImage *image, 
                        float x, float y,
                        float width, float height,
                        float startx, float starty,
                        float endx, float endy,
                        float angle, uint32_t color
) {
    if (!image || !image->tex)
        return;

    if (angle != 0.0f) {
        draw_image_rotate(image->tex, x, y, width, height,
                          startx, starty, endx, endy,
                          angle, color);
    } else {
        draw_image(image->tex, x, y, width, height,
                   startx, starty, endx, endy,
                   color);
    }
}

bool athena_image_lock(AthenaImage *image) {
    if (!image || !image->tex)
        return false;

    if (!image->tex->Mem)
        texture_manager_bind(gsGlobal, image->tex, true);

    return texture_manager_lock(image->tex) != 0;
}

bool athena_image_unlock(AthenaImage *image) {
    if (!image || !image->tex)
        return false;

    return texture_manager_unlock(image->tex) != 0;
}

bool athena_image_locked(const AthenaImage *image) {
    if (!image || !image->tex)
        return false;

    return texture_manager_is_locked(image->tex) != 0;
}

bool athena_image_optimize(AthenaImage *image) {
    if (!image || !image->tex)
        return false;

    if (image->tex->PSM == GS_PSM_CT24) {
        athena_texture_optimize(image->tex);
        return true;
    }

    return false;
}

void athena_image_copy_vram_block(AthenaImage *src, int src_x, int src_y,
                                  AthenaImage *dst, int dst_x, int dst_y) {
    if (!src || !dst || !src->tex || !dst->tex)
        return;

    gs_copy_block(src->tex, src_x, src_y, dst->tex, dst_x, dst_y);
}

void athena_image_set_dimensions(AthenaImage *image, float width, float height) {
    if (!image)
        return;
    image->width = width;
    image->height = height;
}

void athena_image_set_src_rect(AthenaImage *image, float startx, float starty, float endx, float endy) {
    if (!image)
        return;
    image->startx = startx;
    image->starty = starty;
    image->endx = endx;
    image->endy = endy;
}

void athena_image_set_angle(AthenaImage *image, float angle) {
    if (!image)
        return;
    image->angle = angle;
}

void athena_image_set_color(AthenaImage *image, Color color) {
    if (!image)
        return;
    image->color = color;
}

void athena_image_set_filter(AthenaImage *image, uint32_t filter) {
    if (!image || !image->tex)
        return;
    image->tex->Filter = filter;
}
