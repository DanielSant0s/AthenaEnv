#ifndef RENDER_H
#define RENDER_H

#include "graphics.h"

//3D math

int clip_bounding_box(MATRIX local_clip, VECTOR *bounding_box);

void calculate_vertices_clipped(VECTOR *output,  int count, VECTOR *vertices, MATRIX local_screen);

int draw_convert_xyz(xyz_t *output, float x, float y, int z, int count, vertex_f_t *vertices);

model* prepare_cube(const char* path, GSTEXTURE* Texture);

void draw_cube(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

unsigned int get_max_z(GSGLOBAL* gsGlobal);

int athena_process_xyz_rgbaq(GSPRIMPOINT *output, GSGLOBAL* gsGlobal, int count, color_f_t *colours, vertex_f_t *vertices);

int athena_process_xyz_rgbaq_st(GSPRIMSTQPOINT *output, GSGLOBAL* gsGlobal, int count, color_f_t *colours, vertex_f_t *vertices, texel_f_t *coords);

#endif