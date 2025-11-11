#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>
#include <matrix.h>
#include <dbgprintf.h>

#include <owl_packet.h>

#include <mpg_manager.h>

#include <texture_manager.h>

#include <vector.h>

#include <tile_render.h>

register_vu_program(VU1Draw2D_TileList);

vu_mpg *vu1_tile_list = NULL;

void tile_render_init() {
    vu1_tile_list = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw2D_TileList), embed_vu_code_size(VU1Draw2D_TileList), VECTOR_UNIT_1, false);
}

void tile_render_begin() {
    vu1_set_double_buffer_settings(2, 452); 
}

void tile_render_render(athena_tilemap_data *tilemap, float x, float y, float zindex) {
    //uint32_t mpg_addr = vu_mpg_preload(vu1_tile_list, true);
}
