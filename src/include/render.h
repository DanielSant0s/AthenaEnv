#ifndef ATHENA_RENDER_H
#define ATHENA_RENDER_H

#include "graphics.h"

//3D math

typedef struct {
	VECTOR direction[4];
	VECTOR ambient[4];
	VECTOR diffuse[4];
} LightData;

typedef enum {
	ATHENA_LIGHT_DIRECTION,
	ATHENA_LIGHT_AMBIENT,
	ATHENA_LIGHT_DIFFUSE,
} eLightAttributes;

#define BATCH_SIZE 48

int clip_bounding_box(MATRIX local_clip, VECTOR *bounding_box);

void calculate_vertices_clipped(VECTOR *output,  int count, VECTOR *vertices, MATRIX local_screen);

int draw_convert_xyz(xyz_t *output, float x, float y, int z, int count, vertex_f_t *vertices);

unsigned int get_max_z(GSGLOBAL* gsGlobal);

int athena_process_xyz_rgbaq(GSPRIMPOINT *output, GSGLOBAL* gsGlobal, int count, color_f_t *colours, vertex_f_t *vertices);

int athena_process_xyz_rgbaq_st(GSPRIMSTQPOINT *output, GSGLOBAL* gsGlobal, int count, color_f_t *colours, vertex_f_t *vertices, texel_f_t *coords);

void vu0_calculate_colours(VECTOR *output, int count, VECTOR *colours, VECTOR *lights);

void vu0_vector_clamp(VECTOR v0, VECTOR v1, float min, float max);

void vu0_build_lights(VECTOR *output, int count, VECTOR *normals, LightData* lights);

float vu0_innerproduct(VECTOR v0, VECTOR v1);

void athena_set_tw_th(const GSTEXTURE *Texture, int *tw, int *th);

void athena_line_goraud_3d(GSGLOBAL *gsGlobal, float x1, float y1, int iz1, float x2, float y2, int iz2, u64 color1, u64 color2);


#endif