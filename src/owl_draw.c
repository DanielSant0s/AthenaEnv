#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>

#include <time.h>

#include <graphics.h>
#include <athena_math.h>
#include <dbgprintf.h>
#include <fntsys.h>

#include <texture_manager.h>

#include <owl_packet.h>

const int16_t OWL_XYOFFSET[8] qw_aligned = { 2048, 2048, 0, 0, 2048, 2048, 0, 0 };
const uint16_t OWL_XYMAX[8] qw_aligned =   { 4095, 4095, 0, 0, 4095, 4095, 0, 0 };

const int16_t OWL_XYOFFSET_FIXED[8] qw_aligned = { ftoi4(int, 2048), ftoi4(int, 2048), 0, 0, ftoi4(int, 2048), ftoi4(int, 2048), 0, 0 };
const uint16_t OWL_XYMAX_FIXED[8] qw_aligned =   { ftoi4(int, 4095), ftoi4(int, 4095), 0, 0, ftoi4(int, 4095), ftoi4(int, 4095), 0, 0 };

void draw_point_list(float x, float y, prim_point *list, int list_size)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 5+list_size);

	owl_add_cnt_tag_fill(packet, 4+list_size); 
	owl_add_direct(packet, 3+list_size);
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_PRIM, VU_GS_PRIM(GS_PRIM_PRIM_POINT, 0, 0, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_RGBAQ) << 0 | (uint64_t)(GS_XYZ2) << 4), 
					   	VU_GS_GIFTAG(list_size, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 2)
						);

	for (int i = 0; i < list_size; i++) {
		owl_add_rgba_xy(packet, list[i].rgba, x + list[i].x, y + list[i].y); 
	}
} 

void draw_line_list(float x, float y, prim_line *list, int list_size) 
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 3+(list_size*2));

	owl_add_cnt_tag_fill(packet, 2+(list_size*2)); 
	owl_add_direct(packet, 1+(list_size*2));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_PRIM)  << 0 | (uint64_t)(GS_RGBAQ)  << 4 | (uint64_t)(GS_XYZ2) << 8 | (uint64_t)(GS_XYZ2) << 12), 
					   	VU_GS_GIFTAG(list_size, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 4)
						);

	uint64_t prim = VU_GS_PRIM(GS_PRIM_PRIM_LINE, 0, 0, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0);

	for (int i = 0; i < list_size; i++) {
		owl_add_tag(packet, list[i].rgba, prim);
		owl_add_xy_2x(packet, x + list[i].x, y + list[i].y, x + list[i].x2, y + list[i].y2);
	}
}

void draw_line_gouraud_list(float x, float y, prim_gouraud_line *list, int list_size) 
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 5+(list_size*2));

	owl_add_cnt_tag_fill(packet, 4+(list_size*2)); 
	owl_add_direct(packet, 3+(list_size*2));
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_PRIM, VU_GS_PRIM(GS_PRIM_PRIM_LINE, 1, 0, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_XYZ2) << 4), 
					   	VU_GS_GIFTAG(list_size*2, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 2)
						);

	for (int i = 0; i < list_size; i++) {
		owl_add_rgba_xy(packet, list[i].rgba,  x + list[i].x,  y + list[i].y); 
		owl_add_rgba_xy(packet, list[i].rgba2, x + list[i].x2, y + list[i].y2); 
	}
}

void draw_triangle_list(float x, float y, prim_triangle *list, int list_size) {
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 5+(list_size*2));

	owl_add_cnt_tag_fill(packet, 4+(list_size*2)); 
	owl_add_direct(packet, 3+(list_size*2));
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_PRIM, VU_GS_PRIM(GS_PRIM_PRIM_TRIANGLE, 0, 0, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_RGBAQ) << 0 | (uint64_t)(GS_XYZ2) << 4 | (uint64_t)(GS_XYZ2) << 8 | (uint64_t)(GS_XYZ2) << 12), 
					   	VU_GS_GIFTAG(list_size, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 4)
						);


	for (int i = 0; i < list_size; i++) {
		owl_add_ulong(packet, list[i].rgba); owl_add_xy(packet, x + list[i].x, y + list[i].y);
		owl_add_xy_2x(packet, x + list[i].x2, y + list[i].y2, x + list[i].x3, y + list[i].y3);
	}
}

void draw_triangle_gouraud_list(float x, float y, prim_gouraud_triangle *list, int list_size) {
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 5+(list_size*3));

	owl_add_cnt_tag_fill(packet, 4+(list_size*3)); 
	owl_add_direct(packet, 3+(list_size*3));
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_PRIM, VU_GS_PRIM(GS_PRIM_PRIM_TRIANGLE, 1, 0, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_XYZ2) << 4), 
					   	VU_GS_GIFTAG(list_size*3, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 2)
						);

	for (int i = 0; i < list_size; i++) {
		owl_add_rgba_xy(packet, list[i].rgba,  x + list[i].x,  y + list[i].y); 
		owl_add_rgba_xy(packet, list[i].rgba2, x + list[i].x2, y + list[i].y2); 
		owl_add_rgba_xy(packet, list[i].rgba3, x + list[i].x3, y + list[i].y3); 
	}
}

void draw_tex_triangle_list(GSSURFACE* source, float x, float y, prim_tex_triangle *list, int list_size) {
    int texture_id = texture_manager_bind(gsGlobal, source, true);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, (texture_id != -1? 11 : 7)+(list_size*3));

	owl_add_cnt_tag(packet, (texture_id != -1? 10 : 6)+(list_size*3), 0); 

	if (texture_id != -1) {
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSH, 0));
		owl_add_uint(packet, VIF_CODE(2, 0, VIF_DIRECT, 0));

		owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));
		owl_add_tag(packet, GIF_NOP, 0);

		owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
		owl_add_uint(packet, VIF_CODE(texture_id, 0, VIF_MARK, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 1));
	}

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
	owl_add_uint(packet, VIF_CODE(5+(list_size*3), 0, VIF_DIRECT, 0)); 
	
	owl_add_tag(packet, GIF_AD, GIFTAG(3, 1, 0, 0, 0, 1));

	int tw, th;
	athena_set_tw_th(source, &tw, &th);

	owl_add_tag(packet, 
		GS_TEX0_1, 
		GS_SETREG_TEX0((source->Vram & ~TRANSFER_REQUEST_MASK)/256, 
					  source->TBW, 
					  source->PSM,
					  tw, th, 
					  gsGlobal->PrimAlphaEnable, 
					  COLOR_MODULATE,
					  (source->VramClut & ~TRANSFER_REQUEST_MASK)/256, 
					  source->ClutPSM, 
					  0, 0, 
					  source->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
	);
	
	owl_add_tag(packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, source->Filter, source->Filter, 0, 0, 0));

	owl_add_tag(packet, GS_PRIM, VU_GS_PRIM(GS_PRIM_PRIM_TRIANGLE, 0, 1, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_UV)  << 0 | (uint64_t)(GS_XYZ2) << 4), 
					   	VU_GS_GIFTAG(list_size*3, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 2)
						);

	for (int i = 0; i < list_size; i++) {
		owl_add_xy_uv(packet, x + list[i].x,  y + list[i].y,  list[i].u,  list[i].v); 
		owl_add_xy_uv(packet, x + list[i].x2, y + list[i].y2, list[i].u2, list[i].v2); 
		owl_add_xy_uv(packet, x + list[i].x3, y + list[i].y3, list[i].u3, list[i].v3); 
	}
}

void draw_tex_triangle_gouraud_list(GSSURFACE* source, float x, float y, prim_tex_gouraud_triangle *list, int list_size) {
    int texture_id = texture_manager_bind(gsGlobal, source, true);

	uint32_t packet_list_size = ceilf(list_size*4.5f);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, (texture_id != -1? 11 : 7)+packet_list_size);

	owl_add_cnt_tag(packet, (texture_id != -1? 10 : 6)+packet_list_size, 0); 

	if (texture_id != -1) {
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSH, 0));
		owl_add_uint(packet, VIF_CODE(2, 0, VIF_DIRECT, 0));

		owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));
		owl_add_tag(packet, GIF_NOP, 0);

		owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
		owl_add_uint(packet, VIF_CODE(texture_id, 0, VIF_MARK, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 1));
	}

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
	owl_add_uint(packet, VIF_CODE(5+packet_list_size, 0, VIF_DIRECT, 0)); 
	
	owl_add_tag(packet, GIF_AD, GIFTAG(3, 1, 0, 0, 0, 1));

	int tw, th;
	athena_set_tw_th(source, &tw, &th);

	owl_add_tag(packet, 
		GS_TEX0_1, 
		GS_SETREG_TEX0((source->Vram & ~TRANSFER_REQUEST_MASK)/256, 
					  source->TBW, 
					  source->PSM,
					  tw, th, 
					  gsGlobal->PrimAlphaEnable, 
					  COLOR_MODULATE,
					  (source->VramClut & ~TRANSFER_REQUEST_MASK)/256, 
					  source->ClutPSM, 
					  0, 0, 
					  source->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
	);
	
	owl_add_tag(packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, source->Filter, source->Filter, 0, 0, 0));

	owl_add_tag(packet, GS_PRIM, VU_GS_PRIM(GS_PRIM_PRIM_TRIANGLE, 0, 1, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_UV)  << 4 | (uint64_t)(GS_XYZ2) << 8), 
					   	VU_GS_GIFTAG(list_size*3, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 3)
						);

	for (int i = 0; i < list_size; i++) {
		owl_add_ulong(packet, list[i].rgba);
		owl_add_uv(packet, list[i].u, list[i].v);
		owl_add_xy(packet, x+list[i].x, y+list[i].y);

		owl_add_ulong(packet, list[i].rgba2);
		owl_add_uv(packet, list[i].u2, list[i].v2);
		owl_add_xy(packet, x+list[i].x2, y+list[i].y2);

		owl_add_ulong(packet, list[i].rgba3);
		owl_add_uv(packet, list[i].u3, list[i].v3);
		owl_add_xy(packet, x+list[i].x3, y+list[i].y3);
	}

	owl_align_packet(packet);
}

void draw_image_list(GSSURFACE* source, float x, float y, prim_tex_sprite *list, int list_size)
{
    int texture_id = texture_manager_bind(gsGlobal, source, true);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, texture_id != -1? (10+(list_size*3)) : (6+(list_size*3)));

	owl_add_cnt_tag(packet, texture_id != -1? (9+(list_size*3)) : (5+(list_size*3)), 0); 

	if (texture_id != -1) {
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSH, 0));
		owl_add_uint(packet, VIF_CODE(2, 0, VIF_DIRECT, 0));

		owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));
		owl_add_tag(packet, GIF_NOP, 0);

		owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
		owl_add_uint(packet, VIF_CODE(texture_id, 0, VIF_MARK, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 1));
	}

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
	owl_add_uint(packet, VIF_CODE(4+(list_size*3), 0, VIF_DIRECT, 0)); 
	
	owl_add_tag(packet, GIF_AD, GIFTAG(2, 1, 0, 0, 0, 1));

	int tw, th;
	athena_set_tw_th(source, &tw, &th);

	owl_add_tag(packet, 
		GS_TEX0_1, 
		GS_SETREG_TEX0((source->Vram & ~TRANSFER_REQUEST_MASK)/256, 
					  source->TBW, 
					  source->PSM,
					  tw, th, 
					  gsGlobal->PrimAlphaEnable, 
					  COLOR_MODULATE,
					  (source->VramClut & ~TRANSFER_REQUEST_MASK)/256, 
					  source->ClutPSM, 
					  0, 0, 
					  source->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
	);
	
	owl_add_tag(packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, source->Filter, source->Filter, 0, 0, 0));

	owl_add_tag(packet, 
		((uint64_t)(GS_PRIM) << 0 | (uint64_t)(GS_RGBAQ) << 4 | (uint64_t)(GS_UV) << 8 | (uint64_t)(GS_XYZ2) << 12 | (uint64_t)(GS_UV) << 16 | (uint64_t)(GS_XYZ2) << 20), 
			VU_GS_GIFTAG(list_size, 
			 1, NO_CUSTOM_DATA, 0, 
			 0,
			 1, 6) // REGLIST
		 );

	uint64_t prim = VU_GS_PRIM(GS_PRIM_PRIM_SPRITE, 0, 1, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0);

	for (int i = 0; i < list_size; i++) {
		owl_add_tag(packet, list[i].rgba, prim);

		owl_add_xy_uv_2x(
			packet, 
			x + list[i].x, 
			y + list[i].y, 
			list[i].u1, 
			list[i].v1, 
			x + (list[i].w + list[i].x), 
			y + (list[i].h + list[i].y),
			list[i].u2, 
			list[i].v2
		);
	}
}

void draw_image(GSSURFACE* source, float x, float y, float width, float height, float startx, float starty, float endx, float endy, Color color)
{
	if (source == cur_screen_buffer[0] || source == cur_screen_buffer[1] || source == cur_screen_buffer[2]) {
		flush_gs_texcache();
	}

    int texture_id = texture_manager_bind(gsGlobal, source, true);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, texture_id != -1? 13 : 9);

	owl_add_cnt_tag(packet, texture_id != -1? 12 : 8, 0); 

	if (texture_id != -1) {
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSH, 0));
		owl_add_uint(packet, VIF_CODE(2, 0, VIF_DIRECT, 0));

		owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));
		owl_add_tag(packet, GIF_NOP, 0);

		owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
		owl_add_uint(packet, VIF_CODE(texture_id, 0, VIF_MARK, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 1));
	}

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
	owl_add_uint(packet, VIF_CODE(7, 0, VIF_DIRECT, 0)); 
	
	owl_add_tag(packet, GIF_AD, GIFTAG(2, 1, 0, 0, 0, 1));

	int tw, th;
	athena_set_tw_th(source, &tw, &th);

	owl_add_tag(packet, 
		GS_TEX0_1, 
		GS_SETREG_TEX0((source->Vram & ~TRANSFER_REQUEST_MASK)/256, 
					  source->TBW, 
					  source->PSM,
					  tw, th, 
					  gsGlobal->PrimAlphaEnable, 
					  COLOR_MODULATE,
					  (source->VramClut & ~TRANSFER_REQUEST_MASK)/256, 
					  source->ClutPSM, 
					  0, 0, 
					  source->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
	);
	
	owl_add_tag(packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, source->Filter, source->Filter, 0, 0, 0));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_PRIM) << 0 | (uint64_t)(GS_RGBAQ) << 4 | (uint64_t)(GS_UV) << 8 | (uint64_t)(GS_XYZ2) << 12 | (uint64_t)(GS_UV) << 16 | (uint64_t)(GS_XYZ2) << 20), 
					   	VU_GS_GIFTAG(1, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 6) // REGLIST
						);

	owl_add_tag(packet, color, VU_GS_PRIM(GS_PRIM_PRIM_SPRITE, 0, 1, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

    owl_add_xy_uv_2x(packet, x, 
                             y, 
                             startx, 
                             starty, 
                             width+x, 
                             height+y,
                             endx, 
                             endy);
}

void draw_image_rotate(GSSURFACE* source, float x, float y, float width, float height, float startx, float starty, float endx, float endy, float angle, Color color){

	float c = cosf(angle);
	float s = sinf(angle);

	x += width/2;
	y += height/2;

    int texture_id = texture_manager_bind(gsGlobal, source, true);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, texture_id != -1? 19 : 15);

	owl_add_cnt_tag(packet, texture_id != -1? 18 : 14, 0); 

	if (texture_id != -1) {
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSH, 0));
		owl_add_uint(packet, VIF_CODE(2, 0, VIF_DIRECT, 0));

		owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));
		owl_add_tag(packet, GIF_NOP, 0);

		owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
		owl_add_uint(packet, VIF_CODE(texture_id, 0, VIF_MARK, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 1));
	}

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
	owl_add_uint(packet, VIF_CODE(13, 0, VIF_DIRECT, 0)); 
	
	owl_add_tag(packet, GIF_AD, GIFTAG(3, 1, 0, 0, 0, 1));

	int tw, th;
	athena_set_tw_th(source, &tw, &th);

	owl_add_tag(packet, 
		GS_TEX0_1, 
		GS_SETREG_TEX0((source->Vram & ~TRANSFER_REQUEST_MASK)/256, 
					  source->TBW, 
					  source->PSM,
					  tw, th, 
					  gsGlobal->PrimAlphaEnable, 
					  COLOR_MODULATE,
					  (source->VramClut & ~TRANSFER_REQUEST_MASK)/256, 
					  source->ClutPSM, 
					  0, 0, 
					  source->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
	);
	
	owl_add_tag(packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, source->Filter, source->Filter, 0, 0, 0));

	owl_add_tag(packet, GS_RGBAQ, color);

	owl_add_tag(packet, 
					   ((uint64_t)(GS_UV) << 0 | (uint64_t)(GS_XYZ2) << 4), 
					   	VU_GS_GIFTAG(4, 
							1, NO_CUSTOM_DATA, 1, 
							VU_GS_PRIM(GS_PRIM_PRIM_TRISTRIP, 
									   0, 1, 
									   gsGlobal->PrimFogEnable, 
									   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0),
    						0, 2)
						);

	owl_add_tag(packet, 0, GS_SETREG_STQ( (int)(startx) << 4, (int)(starty) << 4 ));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)((-width/2)*c - (-height/2)*s+x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)((-height/2)*c + (-width/2)*s+y) << 4)) << 32));

	owl_add_tag(packet, 0, GS_SETREG_STQ( (int)(startx) << 4, (int)(endy) << 4 ));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)((-width/2)*c - height/2*s+x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(height/2*c + (-width/2)*s+y) << 4)) << 32));

	owl_add_tag(packet, 0, GS_SETREG_STQ( (int)(endx) << 4, (int)(starty) << 4 ));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(width/2*c - (-height/2)*s+x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)((-height/2)*c + width/2*s+y) << 4)) << 32));

	owl_add_tag(packet, 0, GS_SETREG_STQ( (int)(endx) << 4, (int)(endy) << 4 ));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(width/2*c - height/2*s+x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(height/2*c + width/2*s+y) << 4)) << 32));
}

void draw_point(float x, float y, Color color)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 5);

	owl_add_cnt_tag_fill(packet, 4); 
	owl_add_direct(packet, 3);

	owl_add_tag(packet, 
					   ((uint64_t)(GIF_NOP) << 0 | (uint64_t)(GS_PRIM) << 4 | (uint64_t)(GS_RGBAQ) << 8 | (uint64_t)(GS_XYZ2) << 12), 
					   	VU_GS_GIFTAG(1, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 4)
						);

	owl_add_tag(packet, VU_GS_PRIM(GS_PRIM_PRIM_POINT, 0, 0, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0), 0);

	owl_add_rgba_xy(packet, color, x, y); 
} 

void draw_line(float x, float y, float x2, float y2, Color color) 
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 5);

	owl_add_cnt_tag_fill(packet, 4); 
	owl_add_direct(packet, 3);

	owl_add_tag(packet, 
					   ((uint64_t)(GS_PRIM)  << 0 | (uint64_t)(GS_RGBAQ)  << 4 | (uint64_t)(GS_XYZ2) << 8 | (uint64_t)(GS_XYZ2) << 12), 
					   	VU_GS_GIFTAG(1, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 4)
						);

	owl_add_tag(packet, color, VU_GS_PRIM(GS_PRIM_PRIM_LINE, 0, 0, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

	owl_add_xy_2x(packet, x, y, x2, y2);
}


void draw_sprite(float x, float y, int width, int height, Color color)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 5);

	owl_add_cnt_tag_fill(packet, 4);
	owl_add_direct(packet, 3);

	owl_add_tag(packet, 
		((uint64_t)(GS_PRIM)  << 0 | (uint64_t)(GS_RGBAQ)  << 4 | (uint64_t)(GS_XYZ2) << 8 | (uint64_t)(GS_XYZ2) << 12), 
					   	VU_GS_GIFTAG(1, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 4)
						);

	owl_add_tag(packet, color, VU_GS_PRIM(GS_PRIM_PRIM_SPRITE, 0, 0, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

	owl_add_xy_2x(packet, x, y, x+width, y+height);
}

void draw_triangle(float x, float y, float x2, float y2, float x3, float y3, Color color)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 6);

	owl_add_cnt_tag_fill(packet, 5); 
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | 4); 

	owl_add_tag(packet, 
					   ((uint64_t)(GS_PRIM)  << 0 | (uint64_t)(GS_RGBAQ)  << 4 | (uint64_t)(GS_XYZ2) << 8 | (uint64_t)(GS_XYZ2) << 12 | (uint64_t)(GS_XYZ2) << 16), 
					   	VU_GS_GIFTAG(1, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 6)
						);

	owl_add_tag(packet, color, VU_GS_PRIM(GS_PRIM_PRIM_TRIANGLE, 0, 0, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

	owl_add_xy_2x(packet, x, y, x2, y2);

	owl_add_xy_2x(packet, x3, y3, 0, 0);
}

void draw_triangle_gouraud(float x, float y, float x2, float y2, float x3, float y3, Color color, Color color2, Color color3)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 8);

	owl_add_cnt_tag_fill(packet, 7); 
	owl_add_direct(packet, 6);
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_PRIM, VU_GS_PRIM(GS_PRIM_PRIM_TRIANGLE, 1, 0, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_XYZ2) << 4), 
					   	VU_GS_GIFTAG(3, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 2)
						);

	owl_add_rgba_xy(packet, color,  x,  y); 
	owl_add_rgba_xy(packet, color2, x2, y2); 
	owl_add_rgba_xy(packet, color3, x3, y3); 
}

void draw_quad(float x, float y, float x2, float y2, float x3, float y3, float x4, float y4, Color color)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 6);

	owl_add_cnt_tag_fill(packet, 5); 
	owl_add_direct(packet, 4);

	owl_add_tag(packet, 
					   ((uint64_t)(GS_PRIM)  << 0 | 
						(uint64_t)(GS_RGBAQ) << 4 | 
						(uint64_t)(GS_XYZ2) << 8 | 
						(uint64_t)(GS_XYZ2) << 12 | 
						(uint64_t)(GS_XYZ2) << 16 | 
						(uint64_t)(GS_XYZ2) << 20), 
					   	VU_GS_GIFTAG(1, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 6)
						);

	owl_add_tag(packet, color, VU_GS_PRIM(GS_PRIM_PRIM_TRISTRIP, 0, 0, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

	owl_add_xy_2x(packet, x, y, x2, y2);

	owl_add_xy_2x(packet, x3, y3, x4, y4);
}

void draw_quad_gouraud(float x, float y, float x2, float y2, float x3, float y3, float x4, float y4, Color color, Color color2, Color color3, Color color4)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 9);

	owl_add_cnt_tag_fill(packet, 8); 
	owl_add_direct(packet, 7);
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_PRIM, VU_GS_PRIM(GS_PRIM_PRIM_TRISTRIP, 1, 0, gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_XYZ2) << 4), 
					   	VU_GS_GIFTAG(4, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 2)
						);

	owl_add_rgba_xy(packet, color,  x,  y); 
	owl_add_rgba_xy(packet, color2, x2, y2); 
	owl_add_rgba_xy(packet, color3, x3, y3); 
	owl_add_rgba_xy(packet, color4, x4, y4); 
}

void draw_circle(float x, float y, float radius, u64 color, u8 filled)
{
	int fill_factor = (18 + (int)(!filled));
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, (6 + fill_factor));

	owl_add_cnt_tag_fill(packet, (5 + fill_factor)); 
	owl_add_direct(packet, 4 + fill_factor);
	
	owl_add_tag(packet, GIF_AD, GIFTAG(2, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_PRIM, VU_GS_PRIM(filled? GS_PRIM_PRIM_TRIFAN : GS_PRIM_PRIM_LINESTRIP, 0, 0, 
		gsGlobal->PrimFogEnable, gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0));

	owl_add_tag(packet, GS_RGBAQ, color);

	owl_add_tag(packet, 
					   ((uint64_t)(GS_XYZ2) << 0), 
					   	VU_GS_GIFTAG((36 + (int)(!filled)), 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 1)
						);

	for (int a = 0; a < 36; a++) {
		owl_add_xy(packet, (athena_cosf(a * (M_PI*2)/36) * radius) + x, (athena_sinf(a * (M_PI*2)/36) * radius) + y);
	}

	if (!filled) { // using 2x to round it to 38
		owl_add_xy_2x(packet, 
			radius + x, 
			y, 
			0, 
			0
		);
	}
}
