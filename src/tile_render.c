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

#define BATCH_SIZE_2D 50

static float offset_camera[4] = {2047.35f, 2047.35f, 0.0f, 0.0f};

void tile_render_set_camera(float x, float y) {
    offset_camera[2] = x;
    offset_camera[3] = y;
}


static void bake_giftags(owl_packet *packet, bool texture_mapping, int mat_id) {
	prim_reg_t prim_data = {
		.PRIM = GS_PRIM_PRIM_SPRITE,
		.IIP = 0,
		.TME = texture_mapping,
		.FGE = gsGlobal->PrimFogEnable,
		.ABE = gsGlobal->PrimAlphaEnable,
		.AA1 = gsGlobal->PrimAAEnable,
		.FST = 1,
		.CTXT = gsGlobal->PrimContext,
		.FIX = 0
	};

	giftag_t prim_tag = {
		.NLOOP = 0,
		.EOP = 1,
		.PRE = 1,
		.PRIM = prim_data.data,
		.FLG = 0,
		.NREG = 5
	};

	owl_add_unpack_data_cnt(packet, 0, 1, 1);
	owl_add_ulong(packet, prim_tag.data);
	owl_add_ulong(packet, (((u64)GS_RGBAQ) << 0 | ((u64)GS_UV) << 4 | ((u64)GS_XYZ2) << 8 | ((u64)GS_UV) << 12 | ((u64)GS_XYZ2) << 16));
}

void tile_render_render(athena_tilemap_data *tilemap, float x, float y, float zindex) {
    int batch_size = BATCH_SIZE_2D;
    int mpg_addr = vu_mpg_preload(vu1_tile_list, true);

    owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 3);

    owl_add_unpack_data_cnt(packet, 0, 2, 0);
    owl_add_uquad_ptr(packet, offset_camera);
    owl_add_uquad_ptr(packet, &((float []) { x, y, zindex, 0.0f }));


	int last_index = -1;
	GSSURFACE* tex = NULL;
	int texture_id;
	for(int i = 0; i < tilemap->material_count; i++) {
		bool texture_mapping = (tilemap->materials[i].texture_index != -1);

		if (texture_mapping) {
			GSSURFACE *cur_tex = NULL;
			cur_tex = tilemap->textures[tilemap->materials[i].texture_index];

			if (cur_tex != tex) {
				texture_id = texture_manager_bind(gsGlobal, cur_tex, true);
				tex = cur_tex;
			}
		}

        athena_sprite_data *sprites = &tilemap->sprites[last_index+1];

		int idxs_to_draw = (tilemap->materials[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, texture_mapping? 14 : 5);    

			int count = batch_size;
			if (idxs_to_draw < batch_size)
			{
				count = idxs_to_draw;
			}

			if (texture_mapping) {
				append_texture_tags(packet, tex, texture_id, COLOR_MODULATE);
			}
  
			bake_giftags(packet, texture_mapping, i);

			owl_add_unpack_data_ref(packet, 1, &sprites[idxs_drawn], count*4, 1);

			owl_add_cnt_tag(packet, texture_mapping? 5 : 1, owl_vif_code_double(VIF_CODE(0, 0, VIF_NOP, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

			if (texture_mapping) {
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
				owl_add_uint(packet, VIF_CODE(3, 0, VIF_DIRECT, 0)); 

				owl_add_tag(packet, GIF_AD, GIFTAG(2, 1, 0, 0, 0, 1));

				int tw, th;
				athena_set_tw_th(tex, &tw, &th);

				owl_add_tag(packet, 
					GS_TEX0_1+gsGlobal->PrimContext, 
					GS_SETREG_TEX0((tex->Vram & ~TRANSFER_REQUEST_MASK)/256, 
								  tex->TBW, 
								  tex->PSM,
								  tw, th, 
								  gsGlobal->PrimAlphaEnable, 
								  COLOR_MODULATE,
								  (tex->VramClut & ~TRANSFER_REQUEST_MASK)/256, 
								  tex->ClutPSM, 
								  0, 0, 
								  tex->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
				);

				owl_add_tag(packet, GS_TEX1_1+gsGlobal->PrimContext, GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0));
			}
			
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));  
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(packet, VIF_CODE(count, 0, VIF_ITOP, 0));
			owl_add_uint(packet, VIF_CODE(mpg_addr, 0, (last_index == -1? VIF_MSCALF : VIF_MSCNT), 0)); 

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		last_index = tilemap->materials[i].end;
	}

	owl_query_packet(CHANNEL_VIF1, 1);

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSH, 0), VIF_CODE(0, 0, VIF_FLUSH, 0)));

}
