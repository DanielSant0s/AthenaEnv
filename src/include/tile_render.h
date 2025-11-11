#ifndef ATHENA_TILE_RENDER_H
#define ATHENA_TILE_RENDER_H

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <graphics.h>
#include <render.h>

typedef struct {
	float x;
	float y;
	float zindex;

	float w;
	float h;

	float u1;
	float v1;
	float u2;
	float v2;

	uint32_t color;
} athena_sprite_data;

typedef struct {
	int texture_index;

	uint64_t blend_mode;

	uint32_t end;
} athena_sprite_material;

typedef struct {
	GSSURFACE **textures;
	uint32_t texture_count;

	athena_sprite_data *sprites;
	uint32_t sprite_count;

	athena_sprite_material *materials;
	uint32_t material_count;
} athena_tilemap_data;

void tile_render_init();
void tile_render_begin();
void tile_render_render(athena_tilemap_data *tilemap, float x, float y, float zindex);

#endif