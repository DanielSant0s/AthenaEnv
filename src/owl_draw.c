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
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_XYZ2) << 4), 
					   	VU_GS_GIFTAG(1, 
							1, NO_CUSTOM_DATA, 1, 
							VU_GS_PRIM(GS_PRIM_PRIM_POINT, 
									   0, 0, 
									   gsGlobal->PrimFogEnable, 
									   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0),
    						0, 2)
						);

	owl_add_color(packet, color);
 
	owl_add_xy(packet, x, y);
} 

void drawLine(float x, float y, float x2, float y2, Color color) 
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
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_XYZ2) << 4 | (uint64_t)(GS_XYZ2) << 8), 
					   	VU_GS_GIFTAG(1, 
							1, NO_CUSTOM_DATA, 1, 
							VU_GS_PRIM(GS_PRIM_PRIM_LINE, 
									   0, 0, 
									   gsGlobal->PrimFogEnable, 
									   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0),
    						0, 3)
						);

	owl_add_color(packet, color); 

	owl_add_xy(packet, x, y);
	owl_add_xy(packet, x2, y2); 
}


void drawRect(float x, float y, int width, int height, Color color)
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
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_XYZ2) << 4 | (uint64_t)(GS_XYZ2) << 8), 
					   	VU_GS_GIFTAG(1, 
							1, NO_CUSTOM_DATA, 1, 
							VU_GS_PRIM(GS_PRIM_PRIM_SPRITE, 
									   0, 0, 
									   gsGlobal->PrimFogEnable, 
									   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0),
    						0, 3)
						);

	owl_add_color(packet, color);

	owl_add_xy(packet, x, y);

	owl_add_xy(packet, x+width, y+height);
}

void drawTriangle(float x, float y, float x2, float y2, float x3, float y3, Color color)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 9);

	owl_add_cnt_tag_fill(packet, 8); // 4 quadwords for vif
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | 7); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_XYZ2) << 4 | (uint64_t)(GS_XYZ2) << 8 | (uint64_t)(GS_XYZ2) << 12), 
					   	VU_GS_GIFTAG(1, 
							1, NO_CUSTOM_DATA, 1, 
							VU_GS_PRIM(GS_PRIM_PRIM_TRIANGLE, 
									   0, 0, 
									   gsGlobal->PrimFogEnable, 
									   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0),
    						0, 4)
						);

	owl_add_color(packet, color);

	owl_add_xy(packet, x, y);

	owl_add_xy(packet, x2, y2);

	owl_add_xy(packet, x3, y3);
}

void drawTriangle_gouraud(float x, float y, float x2, float y2, float x3, float y3, Color color, Color color2, Color color3)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 11);

	owl_add_cnt_tag_fill(packet, 10); // 4 quadwords for vif
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | 9); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_XYZ2) << 4), 
					   	VU_GS_GIFTAG(3, 
							1, NO_CUSTOM_DATA, 1, 
							VU_GS_PRIM(GS_PRIM_PRIM_TRIANGLE, 
									   1, 0, 
									   gsGlobal->PrimFogEnable, 
									   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0),
    						0, 2)
						);

	owl_add_color(packet, color);

	owl_add_xy(packet, x, y);

	owl_add_color(packet, color2);

	owl_add_xy(packet, x2, y2);

	owl_add_color(packet, color3);

	owl_add_xy(packet, x3, y3);
}

void drawQuad(float x, float y, float x2, float y2, float x3, float y3, float x4, float y4, Color color)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 10);

	owl_add_cnt_tag_fill(packet, 9); // 4 quadwords for vif
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | 8); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_XYZ2) << 4 | (uint64_t)(GS_XYZ2) << 8 | (uint64_t)(GS_XYZ2) << 12 | (uint64_t)(GS_XYZ2) << 16), 
					   	VU_GS_GIFTAG(1, 
							1, NO_CUSTOM_DATA, 1, 
							VU_GS_PRIM(GS_PRIM_PRIM_TRISTRIP, 
									   0, 0, 
									   gsGlobal->PrimFogEnable, 
									   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0),
    						0, 5)
						);

	owl_add_color(packet, color);

	owl_add_xy(packet, x, y);

	owl_add_xy(packet, x2, y2);

	owl_add_xy(packet, x3, y3);

	owl_add_xy(packet, x4, y4);
}

void drawQuad_gouraud(float x, float y, float x2, float y2, float x3, float y3, float x4, float y4, Color color, Color color2, Color color3, Color color4)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 13);

	owl_add_cnt_tag_fill(packet, 12); // 4 quadwords for vif
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | 11); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_XYZ2) << 4), 
					   	VU_GS_GIFTAG(4, 
							1, NO_CUSTOM_DATA, 1, 
							VU_GS_PRIM(GS_PRIM_PRIM_TRISTRIP, 
									   1, 0, 
									   gsGlobal->PrimFogEnable, 
									   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0),
    						0, 2)
						);

	owl_add_color(packet, color);

	owl_add_xy(packet, x, y);

	owl_add_color(packet, color2);

	owl_add_xy(packet, x2, y2);

	owl_add_color(packet, color3);

	owl_add_xy(packet, x3, y3);

	owl_add_color(packet, color4);

	owl_add_xy(packet, x4, y4);
}

void drawCircle(float x, float y, float radius, u64 color, u8 filled)
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, (42 + (int)(!filled)));

	owl_add_cnt_tag_fill(packet, (41 + (int)(!filled))); // 4 quadwords for vif
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, VIF_NOP);
	owl_add_uint(packet, (VIF_DIRECT << 24) | (40 + (int)(!filled))); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(2, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

	owl_add_tag(packet, GS_RGBAQ, color);

	owl_add_tag(packet, 
					   ((uint64_t)(GS_XYZ2) << 0), 
					   	VU_GS_GIFTAG((36 + (int)(!filled)), 
							1, NO_CUSTOM_DATA, 1, 
							VU_GS_PRIM(filled? GS_PRIM_PRIM_TRIFAN : GS_PRIM_PRIM_LINESTRIP, 
									   0, 0, 
									   gsGlobal->PrimFogEnable, 
									   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0),
    						0, 1)
						);

	for (int a = 0; a < 36; a++) {
		owl_add_xy(packet, (athena_cosf(a * (M_PI*2)/36) * radius) + x, (athena_sinf(a * (M_PI*2)/36) * radius) + y);
	}

	if (!filled) {
		owl_add_xy(packet, radius + x, y);
	}
}
