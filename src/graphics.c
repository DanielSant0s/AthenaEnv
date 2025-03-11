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

#include <pad.h>

static const u64 BLACK_RGBAQ   = GS_SETREG_RGBAQ(0x00,0x00,0x00,0x80,0x00);

#define RENDER_QUEUE_PER_POOLSIZE 1024 * 32 // 256K of persistent renderqueue
/* Size of Oneshot drawbuffer (Double Buffered, so it uses this size * 2) */
#define RENDER_QUEUE_OS_POOLSIZE 1024 * 32 // 2048K of oneshot renderqueue

GSGLOBAL *gsGlobal = NULL;

void (*flipScreen)();

static bool vsync = true;
static bool perf = false;
static bool hires = false;
static int vsync_sema_id = 0;
static clock_t curtime = 0;
static float fps = 0.0f;

static int frames = 0;
static int frame_interval = -1;

#define OWL_PACKET_BUFFER_SIZE 1024 * 256

static uint8_t owl_packet_buffer[OWL_PACKET_BUFFER_SIZE] __attribute__((aligned(128)));

void page_clear(Color color) {
	const uint32_t page_count = gsGlobal->Width * (gsGlobal->Height) / 2048;

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 11+(page_count*2));

	owl_add_cnt_tag(packet, 10+(page_count*2), 0);

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(9+(page_count*2), 0, VIF_DIRECT, 0)); // 3 giftags

	owl_add_tag(packet, GIF_AD, VU_GS_GIFTAG(2, 1, NULL, 1, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
	//owl_add_tag(packet, GS_SCISSOR_1, GS_SETREG_SCISSOR(0, 64 - 1, 0, 2048 - 1));
	owl_add_tag(packet, GS_XYOFFSET_1, GS_SETREG_XYOFFSET(0, 0));

	// Clear

	owl_add_tag(packet, GIF_AD | (GIF_AD << 4), VU_GS_GIFTAG(page_count + 1, 1, NULL, 1, VU_GS_PRIM(GS_PRIM_PRIM_SPRITE, 0, 0, 0, 0, 0, 0, gsGlobal->PrimContext, 0), 0, 2));
	owl_add_tag(packet, GS_RGBAQ, color);
	owl_add_tag(packet, GIF_NOP, 0);

	int b = 0;
	for (int i = 0; i < gsGlobal->Width; i += 64)
	{
		for (int j = 0; j < gsGlobal->Height; j += 32)
		{
			owl_add_tag(packet, GS_XYZ2, GS_SETREG_XYZ(i << 4, j << 4, 0));
			owl_add_tag(packet, GS_XYZ2, GS_SETREG_XYZ((i + 64) << 4, (j + 32) << 4, 0));
		}
	}

	owl_add_tag(packet, GIF_AD, VU_GS_GIFTAG(2, 1, NULL, 0, 0, 0, 1));
	
	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST(0, 1, 0x80, 0, 0, 0, 1, 2));
	owl_add_tag(packet, GS_XYOFFSET_1, GS_SETREG_XYOFFSET(gsGlobal->OffsetX, gsGlobal->OffsetY));
}

void clearScreen(Color color)
{
	page_clear(color);
}

float FPSCounter(int interval)
{
	frame_interval = interval;
	return fps;
}

GSFONT* loadFont(const char* path){
	int file = open(path, O_RDONLY, 0777);
	uint16_t magic;
	read(file, &magic, 2);
	close(file);
	GSFONT* font = NULL;
	if (magic == 0x4D42) {
		font = gsKit_init_font(GSKIT_FTYPE_BMP_DAT, (char*)path);
		gsKit_font_upload(gsGlobal, font);
	} else if (magic == 0x4246) {
		font = gsKit_init_font(GSKIT_FTYPE_FNT, (char*)path);
		gsKit_font_upload(gsGlobal, font);
	} else if (magic == 0x5089) { 
		font = gsKit_init_font(GSKIT_FTYPE_PNG_DAT, (char*)path);
		gsKit_font_upload(gsGlobal, font);
	}

	return font;
}

void printFontText(GSFONT* font, const char* text, float x, float y, float scale, Color color)
{
	gsKit_set_test(gsGlobal, GS_ATEST_ON);
	gsKit_font_print_scaled(gsGlobal, font, x-0.5f, y-0.5f, 1, scale, color, text);
}

void unloadFont(GSFONT* font)
{
	texture_manager_free(gsGlobal, font->Texture);
	// clut was pointing to static memory, so do not free
	font->Texture->Clut = NULL;
	// mem was pointing to 'TexBase', so do not free
	font->Texture->Mem = NULL;
	// free texture
	free(font->Texture);
	font->Texture = NULL;

	if (font->RawData != NULL)
		free(font->RawData);

	free(font);
	font = NULL;
}

int getFreeVRAM(){
	return (4096 - (gsGlobal->CurrentPointer / 1024));
}

void drawImage(GSTEXTURE* source, float x, float y, float width, float height, float startx, float starty, float endx, float endy, Color color)
{
    int texture_id = texture_manager_bind(gsGlobal, source, true);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, texture_id != -1? 16 : 12);

	owl_add_cnt_tag(packet, texture_id != -1? 15 : 11, 0); // 4 quadwords for vif

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
	owl_add_uint(packet, VIF_CODE(10, 0, VIF_DIRECT, 0)); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(3, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

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
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_UV) << 4 | (uint64_t)(GS_XYZ2) << 8 | (uint64_t)(GS_UV) << 12 | (uint64_t)(GS_XYZ2) << 16), 
					   	VU_GS_GIFTAG(1, 
							1, NO_CUSTOM_DATA, 1, 
							VU_GS_PRIM(GS_PRIM_PRIM_SPRITE, 
									   0, 1, 
									   gsGlobal->PrimFogEnable, 
									   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0),
    						0, 5)
						);

	owl_add_tag(packet, (uint64_t)((B(color)) | ((uint64_t)(A(color)) << 32)), (uint64_t)((R(color)) | ((uint64_t)G(color) << 32)));

	owl_add_tag(packet, 0, GS_SETREG_STQ( (int)(startx) << 4, (int)(starty) << 4 ));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y) << 4)) << 32));
	
	owl_add_tag(packet, 0, GS_SETREG_STQ( (int)(endx) << 4, (int)(endy) << 4 ));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(width+x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(height+y) << 4)) << 32));
}


void drawImageRotate(GSTEXTURE* source, float x, float y, float width, float height, float startx, float starty, float endx, float endy, float angle, Color color){

	float c = cosf(angle);
	float s = sinf(angle);

	x += width/2;
	y += height/2;

    int texture_id = texture_manager_bind(gsGlobal, source, true);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, texture_id != -1? 20 : 16);

	owl_add_cnt_tag(packet, texture_id != -1? 19 : 15, 0); // 4 quadwords for vif

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
	owl_add_uint(packet, VIF_CODE(14, 0, VIF_DIRECT, 0)); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(4, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

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

	owl_add_tag(packet, (uint64_t)((B(color)) | ((uint64_t)(A(color)) << 32)), (uint64_t)((R(color)) | ((uint64_t)G(color) << 32)));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y) << 4)) << 32));
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

	owl_add_tag(packet, (uint64_t)((B(color)) | ((uint64_t)(A(color)) << 32)), (uint64_t)((R(color)) | ((uint64_t)G(color) << 32)));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y) << 4)) << 32));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x2) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y2) << 4)) << 32));
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

	owl_add_tag(packet, (uint64_t)((B(color)) | ((uint64_t)(A(color)) << 32)), (uint64_t)((R(color)) | ((uint64_t)G(color) << 32)));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y) << 4)) << 32));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x+width) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y+height) << 4)) << 32));
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

	owl_add_tag(packet, (uint64_t)((B(color)) | ((uint64_t)(A(color)) << 32)), (uint64_t)((R(color)) | ((uint64_t)G(color) << 32)));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y) << 4)) << 32));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x2) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y2) << 4)) << 32));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x3) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y3) << 4)) << 32));
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

	owl_add_tag(packet, (uint64_t)((B(color)) | ((uint64_t)(A(color)) << 32)), (uint64_t)((R(color)) | ((uint64_t)G(color) << 32)));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y) << 4)) << 32));

	owl_add_tag(packet, (uint64_t)((B(color2)) | ((uint64_t)(A(color2)) << 32)), (uint64_t)((R(color2)) | ((uint64_t)G(color2) << 32)));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x2) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y2) << 4)) << 32));

	owl_add_tag(packet, (uint64_t)((B(color3)) | ((uint64_t)(A(color3)) << 32)), (uint64_t)((R(color3)) | ((uint64_t)G(color3) << 32)));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x3) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y3) << 4)) << 32));
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

	owl_add_tag(packet, (uint64_t)((B(color)) | ((uint64_t)(A(color)) << 32)), (uint64_t)((R(color)) | ((uint64_t)G(color) << 32)));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y) << 4)) << 32));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x2) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y2) << 4)) << 32));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x3) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y3) << 4)) << 32));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x4) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y4) << 4)) << 32));
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

	owl_add_tag(packet, (uint64_t)((B(color)) | ((uint64_t)(A(color)) << 32)), (uint64_t)((R(color)) | ((uint64_t)G(color) << 32)));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y) << 4)) << 32));

	owl_add_tag(packet, (uint64_t)((B(color2)) | ((uint64_t)(A(color2)) << 32)), (uint64_t)((R(color2)) | ((uint64_t)G(color2) << 32)));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x2) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y2) << 4)) << 32));

	owl_add_tag(packet, (uint64_t)((B(color3)) | ((uint64_t)(A(color3)) << 32)), (uint64_t)((R(color3)) | ((uint64_t)G(color3) << 32)));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x3) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y3) << 4)) << 32));

	owl_add_tag(packet, (uint64_t)((B(color4)) | ((uint64_t)(A(color4)) << 32)), (uint64_t)((R(color4)) | ((uint64_t)G(color4) << 32)));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(x4) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(y4) << 4)) << 32));
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
		owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)((cosf(a * (M_PI*2)/36) * radius) + x) << 4)) | 
							  ((uint64_t)((int)gsGlobal->OffsetY+((int)((sinf(a * (M_PI*2)/36) * radius) + y) << 4)) << 32));
	}

	if (!filled) {
		owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(radius + x) << 4)) | 
							  ((uint64_t)((int)gsGlobal->OffsetY+((int)(y) << 4)) << 32));
	}
}

void InvalidateTexture(GSTEXTURE *txt)
{
    texture_manager_invalidate(gsGlobal, txt);
}

void UnloadTexture(GSTEXTURE *txt)
{
	texture_manager_free(gsGlobal, txt);
	
}

int GetInterlacedFrameMode()
{
    if ((gsGlobal->Interlace == GS_INTERLACED) && (gsGlobal->Field == GS_FRAME))
        return 1;

    return 0;
}
GSGLOBAL *getGSGLOBAL(){ return gsGlobal; }

static void switchFlipScreenFunction();

void fntDrawQuad(rm_quad_t *q)
{
    int texture_id = texture_manager_bind(gsGlobal, q->txt, true);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, texture_id != -1? 16 : 12);

	owl_add_cnt_tag(packet, texture_id != -1? 15 : 11, 0); // 4 quadwords for vif

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
	owl_add_uint(packet, VIF_CODE(10, 0, VIF_DIRECT, 0)); // 3 giftags
	
	owl_add_tag(packet, GIF_AD, GIFTAG(3, 1, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

	int tw, th;
	athena_set_tw_th(q->txt, &tw, &th);

	owl_add_tag(packet, 
		GS_TEX0_1, 
		GS_SETREG_TEX0((q->txt->Vram & ~TRANSFER_REQUEST_MASK)/256, 
					  q->txt->TBW, 
					  q->txt->PSM,
					  tw, th, 
					  gsGlobal->PrimAlphaEnable, 
					  COLOR_MODULATE,
					  (q->txt->VramClut & ~TRANSFER_REQUEST_MASK)/256, 
					  q->txt->ClutPSM, 
					  0, 0, 
					  q->txt->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
	);
	
	owl_add_tag(packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, q->txt->Filter, q->txt->Filter, 0, 0, 0));

	owl_add_tag(packet, 
					   ((uint64_t)(GS_RGBAQ)  << 0 | (uint64_t)(GS_UV) << 4 | (uint64_t)(GS_XYZ2) << 8 | (uint64_t)(GS_UV) << 12 | (uint64_t)(GS_XYZ2) << 16), 
					   	VU_GS_GIFTAG(1, 
							1, NO_CUSTOM_DATA, 1, 
							VU_GS_PRIM(GS_PRIM_PRIM_SPRITE, 
									   0, 1, 
									   gsGlobal->PrimFogEnable, 
									   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0),
    						0, 5)
						);

	owl_add_tag(packet, (uint64_t)((B(q->color)) | ((uint64_t)(A(q->color)) << 32)), (uint64_t)((R(q->color)) | ((uint64_t)G(q->color) << 32)));

	owl_add_tag(packet, 0, GS_SETREG_STQ( (int)(q->ul.u) << 4, (int)(q->ul.v) << 4 ));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(q->ul.x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(q->ul.y) << 4)) << 32));
	
	owl_add_tag(packet, 0, GS_SETREG_STQ( (int)(q->br.u) << 4, (int)(q->br.v) << 4 ));

	owl_add_tag(packet, 1, (uint64_t)((int)gsGlobal->OffsetX+((int)(q->br.x) << 4)) | ((uint64_t)((int)gsGlobal->OffsetY+((int)(q->br.y) << 4)) << 32));
}

/* PRIVATE METHODS */
static int vsync_handler()
{
   iSignalSema(vsync_sema_id);

   ExitHandler();
   return 0;
}

void setVSync(bool vsync_flag){ 
	vsync = vsync_flag;
	switchFlipScreenFunction();
}

void toggleFrameCounter(bool enable){ 
	perf = enable;
	switchFlipScreenFunction();
}

/* Copy of gsKit_sync_flip, but without the 'flip' */
static void gsKit_sync(GSGLOBAL *gsGlobal)
{
   if (!gsGlobal->FirstFrame) WaitSema(vsync_sema_id);
   while (PollSema(vsync_sema_id) >= 0)
   	;
}

/* Copy of gsKit_sync_flip, but without the 'sync' */
static void gsKit_flip(GSGLOBAL *gsGlobal)
{
   if (!gsGlobal->FirstFrame)
   {
      if (gsGlobal->DoubleBuffering == GS_SETTING_ON)
      {
         GS_SET_DISPFB2( gsGlobal->ScreenBuffer[
               gsGlobal->ActiveBuffer & 1] / 8192,
               gsGlobal->Width / 64, gsGlobal->PSM, 0, 0 );

         gsGlobal->ActiveBuffer ^= 1;
      }

   }

   gsKit_setactive(gsGlobal);
}

void athena_error_screen(const char* errMsg, bool dark_mode) {
    uint64_t color = GS_SETREG_RGBAQ(0x20,0x20,0x20,0x80,0x00);
    uint64_t color2 = GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00);

    if (errMsg != NULL)
    {
        printf("AthenaEnv ERROR!\n%s", errMsg);

        if (strstr(errMsg, "EvalError") != NULL) {
            color = GS_SETREG_RGBAQ(0x56,0x71,0x7D,0x80,0x00);
        } else if (strstr(errMsg, "SyntaxError") != NULL) {
            color = GS_SETREG_RGBAQ(0x20,0x60,0xB0,0x80,0x00);
        } else if (strstr(errMsg, "TypeError") != NULL) {
            color = GS_SETREG_RGBAQ(0x3b,0x81,0x32,0x80,0x00);
        } else if (strstr(errMsg, "ReferenceError") != NULL) {
            color = GS_SETREG_RGBAQ(0xE5,0xDE,0x00,0x80,0x00);
        } else if (strstr(errMsg, "RangeError") != NULL) {
            color = GS_SETREG_RGBAQ(0xD0,0x31,0x3D,0x80,0x00);
        } else if (strstr(errMsg, "InternalError") != NULL) {
            color = GS_SETREG_RGBAQ(0x8A,0x00,0xC2,0x80,0x00);
        } else if (strstr(errMsg, "URIError") != NULL) {
            color = GS_SETREG_RGBAQ(0xFF,0x78,0x1F,0x80,0x00);
        } else if (strstr(errMsg, "AggregateError") != NULL) {
            color = GS_SETREG_RGBAQ(0xE2,0x61,0x9F,0x80,0x00);
        } else if (strstr(errMsg, "AthenaError") != NULL) {
            color = GS_SETREG_RGBAQ(0x70,0x29,0x63,0x80,0x00);
        }

        if(dark_mode) {
            color2 = color;
            color = GS_SETREG_RGBAQ(0x20,0x20,0x20,0x80,0x00);
        } else {
            color2 = GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00);
        }

		fntLoadDefault(NULL);
		fntSetCharSize(0, FNTSYS_CHAR_SIZE*64, FNTSYS_CHAR_SIZE*64);

    	while (!isButtonPressed(PAD_START)) {
			clearScreen(color);
			fntRenderString(0, 15, 15, 0, 640, 448, "AthenaEnv ERROR!", 0.8f, color2);
			fntRenderString(0, 15, 80, 0, 640, 448, errMsg, 0.8f, color2);
			fntRenderString(0, 15, 400, 0, 640, 448, "Press [start] to restart", 0.8f, color2);
			flipScreen();
		} 
    }
}

inline void processFrameCounter()
{
	if (frames > frame_interval && frame_interval != -1) {
		clock_t prevtime = curtime;
		curtime = clock();

		fps = ((float)(frame_interval)) / (((float)(curtime - prevtime)) / ((float)CLOCKS_PER_SEC));

		frames = 0;
	}
	frames++;
}

static void flipScreenSingleBuffering()
{
	owl_flush_packet();
	//gsKit_set_finish(gsGlobal);
	gsKit_sync(gsGlobal);
	//gsKit_queue_exec(gsGlobal);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dmaKit_wait(DMA_CHANNEL_GIF, 0);

	texture_manager_nextFrame(gsGlobal);

	
}

static void flipScreenSingleBufferingPerf()
{
	owl_flush_packet();

	//gsKit_set_finish(gsGlobal);
	gsKit_sync(gsGlobal);

	//gsKit_queue_exec(gsGlobal);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dmaKit_wait(DMA_CHANNEL_GIF, 0);

	texture_manager_nextFrame(gsGlobal);

	processFrameCounter();

	
}

static void flipScreenDoubleBuffering()
{	
	//gsKit_set_finish(gsGlobal);

	owl_flush_packet();

	gsKit_sync(gsGlobal);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dmaKit_wait(DMA_CHANNEL_GIF, 0);

	gsKit_flip(gsGlobal);
	//gsKit_queue_exec(gsGlobal);

	gsKit_finish();
	
	texture_manager_nextFrame(gsGlobal);

	
}

static void flipScreenDoubleBufferingPerf()
{	
	owl_flush_packet();

	//gsKit_set_finish(gsGlobal);

	gsKit_sync(gsGlobal);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dmaKit_wait(DMA_CHANNEL_GIF, 0);

	gsKit_finish();

	gsKit_flip(gsGlobal);
	//gsKit_queue_exec(gsGlobal);
	
	texture_manager_nextFrame(gsGlobal);

	processFrameCounter();

	
}

//////////////////////////////////////////////////////////////////////////////////////////////

static void flipScreenSingleBufferingNoVSync()
{
	//gsKit_queue_exec(gsGlobal);

	owl_flush_packet();

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dmaKit_wait(DMA_CHANNEL_GIF, 0);

	texture_manager_nextFrame(gsGlobal);

	
}

static void flipScreenSingleBufferingPerfNoVSync()
{
	//gsKit_queue_exec(gsGlobal);
	owl_flush_packet();

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dmaKit_wait(DMA_CHANNEL_GIF, 0);

	texture_manager_nextFrame(gsGlobal);

	processFrameCounter();

	
}

static void flipScreenDoubleBufferingNoVSync()
{	
	owl_flush_packet();

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dmaKit_wait(DMA_CHANNEL_GIF, 0);

	gsKit_finish();

	gsKit_flip(gsGlobal);
	//gsKit_queue_exec(gsGlobal);	

	texture_manager_nextFrame(gsGlobal);

	
}

static void flipScreenDoubleBufferingPerfNoVSync()
{	
	owl_flush_packet();

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dmaKit_wait(DMA_CHANNEL_GIF, 0);

	gsKit_finish();

	gsKit_flip(gsGlobal);
	//gsKit_queue_exec(gsGlobal);

	texture_manager_nextFrame(gsGlobal);

	processFrameCounter();
}

//////////////////////////////////////////////////////////////////////////////////////////////

static void flipScreenHiRes()
{
	owl_flush_packet();

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	gsKit_hires_sync(gsGlobal);
	gsKit_hires_flip(gsGlobal);
	texture_manager_nextFrame(gsGlobal);

	

}

static void flipScreenHiResPerf()
{
	owl_flush_packet();

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	gsKit_hires_sync(gsGlobal);
	gsKit_hires_flip(gsGlobal);
	texture_manager_nextFrame(gsGlobal);

	processFrameCounter();

	
}

static void switchFlipScreenFunction()
{
	if (hires) {
		if(perf) {
			flipScreen = flipScreenHiResPerf;
		} else {
			flipScreen = flipScreenHiRes;
		}
	} else {
		if (vsync) {
			if (gsGlobal->DoubleBuffering == GS_SETTING_OFF) {
				if(perf) {
					flipScreen = flipScreenSingleBufferingPerf;
				} else {
					flipScreen = flipScreenSingleBuffering;
				}
			} else {
				if(perf) {
					flipScreen = flipScreenDoubleBufferingPerf;
				} else {
					flipScreen = flipScreenDoubleBuffering;
				}
			}

		} else {
			if (gsGlobal->DoubleBuffering == GS_SETTING_OFF) {
				if(perf) {
					flipScreen = flipScreenSingleBufferingPerfNoVSync;
				} else {
					flipScreen = flipScreenSingleBufferingNoVSync;
				}
			} else {
				if(perf) {
					flipScreen = flipScreenDoubleBufferingPerfNoVSync;
				} else {
					flipScreen = flipScreenDoubleBufferingNoVSync;
				}
			}

		}
	}
}

void setVideoMode(s16 mode, int width, int height, int psm, s16 interlace, s16 field, bool zbuffering, int psmz, bool double_buffering, uint8_t pass_count) {
	gsGlobal->Mode = mode;
	gsGlobal->Width = width;
	if ((interlace == GS_INTERLACED) && (field == GS_FRAME))
		gsGlobal->Height = height / 2;
	else
		gsGlobal->Height = height;

	gsGlobal->PSM = psm;
	gsGlobal->PSMZ = psmz;

	gsGlobal->ZBuffering = zbuffering;
	gsGlobal->DoubleBuffering = double_buffering;
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	gsGlobal->Dithering = GS_SETTING_OFF;

	gsGlobal->Interlace = interlace;
	gsGlobal->Field = field;

	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);

	dbgprintf("\nGraphics: created video surface of (%d, %d)\n",
		gsGlobal->Width, gsGlobal->Height);

	gsKit_set_clamp(gsGlobal, GS_CMODE_REPEAT);

	if (pass_count > 1) {
		gsKit_hires_init_screen(gsGlobal, pass_count);
		hires = true;
	} else {
		gsKit_init_screen(gsGlobal);
		hires = false;
	}

	texture_manager_init(gsGlobal);

	switchFlipScreenFunction();
	
	gsKit_set_display_offset(gsGlobal, -0.5f, -0.5f);


	gsKit_sync(gsGlobal);
	gsKit_flip(gsGlobal);
}

void init_graphics()
{
	ee_sema_t sema;
    sema.init_count = 0;
    sema.max_count = 1;
    sema.option = 0;
    vsync_sema_id = CreateSema(&sema);

	gsGlobal = gsKit_init_global_custom(RENDER_QUEUE_OS_POOLSIZE, RENDER_QUEUE_PER_POOLSIZE);

	gsGlobal->Mode = gsKit_check_rom();
	if (gsGlobal->Mode == GS_MODE_PAL){
		gsGlobal->Height = 512;
	} else {
		gsGlobal->Height = 448;
	}

	//gsGlobal->OffsetX = (int)((2048.0f-(gsGlobal->Width/2)) * 16.0f);
	//gsGlobal->OffsetY = (int)((2048.0f-(gsGlobal->Height/2)) * 16.0f);

	gsGlobal->PSM  = GS_PSM_CT24;
	gsGlobal->PSMZ = GS_PSMZ_16S;
	gsGlobal->ZBuffering = GS_SETTING_OFF;
	gsGlobal->DoubleBuffering = GS_SETTING_ON;
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	gsGlobal->Dithering = GS_SETTING_OFF;

	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);

	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_VIF1);
	dmaKit_wait(DMA_CHANNEL_GIF, 0);
	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	flipScreen = flipScreenDoubleBuffering;

	dbgprintf("\nGraphics: created %ix%i video surface\n",
		gsGlobal->Width, gsGlobal->Height);

	gsKit_set_clamp(gsGlobal, GS_CMODE_REPEAT);

	gsKit_init_screen(gsGlobal);

	texture_manager_init(gsGlobal);

	gsKit_add_vsync_handler(vsync_handler);

	gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    gsKit_clear(gsGlobal, BLACK_RGBAQ);	
	gsKit_vsync_wait();
	flipScreen();
	gsKit_clear(gsGlobal, BLACK_RGBAQ);	
	gsKit_vsync_wait();
	flipScreen();

	owl_init(owl_packet_buffer, OWL_PACKET_BUFFER_SIZE/sizeof(owl_qword));
}

void graphicWaitVblankStart(){

	gsKit_vsync_wait();

}
