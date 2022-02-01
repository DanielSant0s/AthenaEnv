#include "graphics.h"


typedef union TexCoord { 
    struct {
        float s, t;
    };
    u64 word;
} __attribute__((packed, aligned(8))) TexCoord;

//3D math

extern int clip_bounding_box(MATRIX local_clip, VECTOR *bounding_box);

extern void RotTransPersClipGsColN(vertex_f_t *output, MATRIX local_screen, VECTOR *verts, VECTOR *normals,
VECTOR *texels, VECTOR *colours, MATRIX local_light, MATRIX light_color, int count);

extern void calculate_vertices_no_clip(VECTOR *output,  int count, VECTOR *vertices, MATRIX local_screen);

extern void calculate_vertices_clipped(VECTOR *output,  int count, VECTOR *vertices, MATRIX local_screen);

extern int draw_convert_rgbq(color_t *output, int count, vertex_f_t *vertices, color_f_t *colours, unsigned char alpha);

extern int draw_convert_rgbaq(color_t *output, int count, vertex_f_t *vertices, color_f_t *colours);

extern int draw_convert_st(texel_t *output, int count, vertex_f_t *vertices, texel_f_t *coords);

extern int draw_convert_xyz(xyz_t *output, float x, float y, int z, int count, vertex_f_t *vertices);

//polygon drawing

extern void gsKit_prim_triangle_goraud_texture_3d_st(
    GSGLOBAL *gsGlobal, GSTEXTURE *Texture,
    float x1, float y1, int iz1, float u1, float v1,
    float x2, float y2, int iz2, float u2, float v2,
    float x3, float y3, int iz3, float u3, float v3,
    u64 color1, u64 color2, u64 color3
);

extern void gsKit_prim_triangle_gouraud_3d_fog(
    GSGLOBAL *gsGlobal, float x1, float y1, int iz1,
    float x2, float y2, int iz2,
    float x3, float y3, int iz3,
    u64 color1, u64 color2, u64 color3,
    u8 fog1, u8 fog2, u8 fog3
);

extern void gsKit_prim_triangle_goraud_texture_3d_st_fog(
    GSGLOBAL *gsGlobal, GSTEXTURE *Texture,
    float x1, float y1, int iz1, float u1, float v1,
    float x2, float y2, int iz2, float u2, float v2,
    float x3, float y3, int iz3, float u3, float v3,
    u64 color1, u64 color2, u64 color3,
    u8 fog1, u8 fog2, u8 fog3
);