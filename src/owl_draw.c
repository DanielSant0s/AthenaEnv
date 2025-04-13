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

#include <owl_packet.h>

const int16_t OWL_XYOFFSET[8] __attribute__((aligned(16))) = { 2048, 2048, 0, 0, 2048, 2048, 0, 0 };
const uint16_t OWL_XYMAX[8] __attribute__((aligned(16))) =   { 4095, 4095, 0, 0, 4095, 4095, 0, 0 };

const int16_t OWL_XYOFFSET_FIXED[8] __attribute__((aligned(16))) = { 2048 << 4, 2048 << 4, 0, 0, 2048 << 4, 2048 << 4, 0, 0 };
const uint16_t OWL_XYMAX_FIXED[8] __attribute__((aligned(16))) =   { 4095 << 4, 4095 << 4, 0, 0, 4095 << 4, 4095 << 4, 0, 0 };

void drawPixel(float x, float y, Color color)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 7);

	owl_add_cnt_tag_fill(packet, 6); // 4 quadwords for vif
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | 5); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

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

void drawLine(float x, float y, float x2, float y2, Color color) 
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 7);

	owl_add_cnt_tag_fill(packet, 6); // 4 quadwords for vif
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | 5); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

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


void drawRect(float x, float y, int width, int height, Color color)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 7);

	owl_add_cnt_tag_fill(packet, 6); // 4 quadwords for vif
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | 5); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

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

void drawTriangle(float x, float y, float x2, float y2, float x3, float y3, Color color)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 8);

	owl_add_cnt_tag_fill(packet, 7); // 4 quadwords for vif
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | 6); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

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

void drawTriangle_gouraud(float x, float y, float x2, float y2, float x3, float y3, Color color, Color color2, Color color3)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 9);

	owl_add_cnt_tag_fill(packet, 8); // 4 quadwords for vif
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | 7); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(2, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

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

void drawQuad(float x, float y, float x2, float y2, float x3, float y3, float x4, float y4, Color color)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 8);

	owl_add_cnt_tag_fill(packet, 7); // 4 quadwords for vif
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | 6); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

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

void drawQuad_gouraud(float x, float y, float x2, float y2, float x3, float y3, float x4, float y4, Color color, Color color2, Color color3, Color color4)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 10);

	owl_add_cnt_tag_fill(packet, 9); // 4 quadwords for vif
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | 8); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(2, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

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

void drawCircle(float x, float y, float radius, u64 color, u8 filled)
{
	int fill_factor = (18 + (int)(!filled));
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, (7 + fill_factor));

	owl_add_cnt_tag_fill(packet, (6 + fill_factor)); // 4 quadwords for vif
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | (5 + fill_factor)); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(3, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

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
