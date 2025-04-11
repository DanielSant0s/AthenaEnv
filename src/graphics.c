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

#include <osd_config.h>
#include <rom0_info.h>

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

#define OWL_PACKET_BUFFER_SIZE 1024

static owl_qword owl_packet_buffer[OWL_PACKET_BUFFER_SIZE] __attribute__((aligned(16))) = { 0 };

void set_finish()
{
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 4);
 
	owl_add_cnt_tag(packet, 3, 0);

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(2, 0, VIF_DIRECT, 0)); // 3 giftags

	owl_add_tag(packet, GIF_AD, VU_GS_GIFTAG(1, 1, NULL, 0, 0, 0, 1));

	owl_add_tag(packet, GS_FINISH, 0);
}

void set_alpha_blend_mode(uint64_t alpha_equation) {
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 4);
 
	owl_add_cnt_tag(packet, 3, 0);

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(2, 0, VIF_DIRECT, 0)); // 3 giftags

	owl_add_tag(packet, GIF_AD, VU_GS_GIFTAG(1, 1, NULL, 0, 0, 0, 1));

	owl_add_tag(packet, GS_ALPHA_1, alpha_equation);
}

void page_clear(Color color) {
	const uint32_t page_count = gsGlobal->Width * (gsGlobal->Height) / 2048;

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 11+page_count);

	owl_add_cnt_tag(packet, 10+page_count, 0);

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(9+page_count, 0, VIF_DIRECT, 0)); // 3 giftags

	owl_add_tag(packet, GIF_AD, VU_GS_GIFTAG(4, 1, NULL, 1, 0, 0, 1));

	owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1));
	//owl_add_tag(packet, GS_SCISSOR_1, GS_SETREG_SCISSOR(0, 64 - 1, 0, 2048 - 1));
	owl_add_tag(packet, GS_XYOFFSET_1, GS_SETREG_XYOFFSET(0, 0));

	// Clear
	owl_add_tag(packet, GS_RGBAQ, color);
	owl_add_tag(packet, GS_PRIM, VU_GS_PRIM(GS_PRIM_PRIM_SPRITE, 0, 0, 0, 0, 0, 0, gsGlobal->PrimContext, 0));

	owl_add_tag(packet, GS_XYZ2 | (GS_XYZ2 << 4), VU_GS_GIFTAG(page_count, 1, NULL, 1, 0, 1, 2));

	int b = 0;
	for (int i = 0; i < gsGlobal->Width; i += 64)
	{
		for (int j = 0; j < gsGlobal->Height; j += 32)
		{
			asm volatile ( 	
				"pcpyld   $7,     %[xy1],        %[xy2]      \n"
				"psllh    $7,     $7,            4           \n"
				"sq       $7,     0x00(%[ptr])               \n"
				"daddiu   %[ptr], %[ptr],        0x10        \n" // packet->ptr++;
				 : [ptr] "+r" (packet->ptr) : [xy1] "r" (GS_SETREG_XYZ(i, j, 0)), [xy2] "r" (GS_SETREG_XYZ(i+64, j+32, 0)): "$7", "memory");

			//owl_add_tag(packet, GS_XYZ2, GS_SETREG_XYZ(i << 4, j << 4, 0));
			//owl_add_tag(packet, GS_XYZ2, GS_SETREG_XYZ((i + 64) << 4, (j + 32) << 4, 0));
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
	texture_manager_free(font->Texture);
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

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, texture_id != -1? 14 : 10);

	owl_add_cnt_tag(packet, texture_id != -1? 13 : 9, 0); // 4 quadwords for vif

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
	owl_add_uint(packet, VIF_CODE(8, 0, VIF_DIRECT, 0)); // 3 giftags
	
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

int GetInterlacedFrameMode()
{
    if ((gsGlobal->Interlace == GS_INTERLACED) && (gsGlobal->Field == GS_FRAME))
        return 1;

    return 0;
}
GSGLOBAL *getGSGLOBAL(){ return gsGlobal; }

static void switchFlipScreenFunction();

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

void setactive(GSGLOBAL *gsGlobal)
{
	owl_packet *packet = owl_query_packet(DMA_CHANNEL_VIF1, 7);

	owl_add_cnt_tag(packet, 6, 0);

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(5, 0, VIF_DIRECT, 0)); // 3 giftags

	owl_add_tag(packet, GIF_AD, VU_GS_GIFTAG(4, 1, NULL, 0, 0, 0, 1));

	// Context 1

	owl_add_tag(packet, GS_SCISSOR_1, GS_SETREG_SCISSOR_1( 0, gsGlobal->Width - 1, 0, gsGlobal->Height - 1 ));
	owl_add_tag(packet, GS_FRAME_1, GS_SETREG_FRAME_1( gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1] / 8192, gsGlobal->Width / 64, gsGlobal->PSM, 0 ));

	// Context 2

	owl_add_tag(packet, GS_SCISSOR_2, GS_SETREG_SCISSOR_1( 0, gsGlobal->Width - 1, 0, gsGlobal->Height - 1 ));
	owl_add_tag(packet, GS_FRAME_2, GS_SETREG_FRAME_1( gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1] / 8192, gsGlobal->Width / 64, gsGlobal->PSM, 0 ));
}

/* Copy of sync_screen_flip, but without the 'flip' */
static void sync_screen(GSGLOBAL *gsGlobal)
{
   if (!gsGlobal->FirstFrame) WaitSema(vsync_sema_id);
   while (PollSema(vsync_sema_id) >= 0)
   	;
}

/* Copy of sync_screen_flip, but without the 'sync' */
static void flip_screen(GSGLOBAL *gsGlobal)
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

   setactive(gsGlobal);
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
			fntRenderString(0, 15, 15, 0, 625, 448, "AthenaEnv ERROR!", 0.8f, color2);
			fntRenderString(0, 15, 80, 0, 625, 448, errMsg, 0.8f, color2);
			fntRenderString(0, 15, 400, 0, 625, 448, "Press [start] to restart", 0.8f, color2);
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
	sync_screen(gsGlobal);
	//gsKit_queue_exec(gsGlobal);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dmaKit_wait(DMA_CHANNEL_GIF, 0);

	texture_manager_nextFrame(gsGlobal);

	
}

static void flipScreenSingleBufferingPerf()
{
	owl_flush_packet();

	//gsKit_set_finish(gsGlobal);
	sync_screen(gsGlobal);

	//gsKit_queue_exec(gsGlobal);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dmaKit_wait(DMA_CHANNEL_GIF, 0);

	texture_manager_nextFrame(gsGlobal);

	processFrameCounter();

	
}

static void flipScreenDoubleBuffering()
{	
	dmaKit_wait(DMA_CHANNEL_GIF, 0);
	dmaKit_wait(DMA_CHANNEL_VIF1, 0);
	
	set_finish();

	owl_flush_packet();

	sync_screen(gsGlobal);

	if(!gsGlobal->FirstFrame)
		while(!(GS_CSR_FINISH));

	GS_SETREG_CSR_FINISH(1);

	gsGlobal->FirstFrame = GS_SETTING_OFF;

	flip_screen(gsGlobal);
	//gsKit_queue_exec(gsGlobal);
	
	texture_manager_nextFrame(gsGlobal);

	
}

static void flipScreenDoubleBufferingPerf()
{	
	owl_flush_packet();

	//gsKit_set_finish(gsGlobal);

	sync_screen(gsGlobal);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dmaKit_wait(DMA_CHANNEL_GIF, 0);

	gsKit_finish();

	flip_screen(gsGlobal);
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

	flip_screen(gsGlobal);
	//gsKit_queue_exec(gsGlobal);	

	texture_manager_nextFrame(gsGlobal);

	
}

static void flipScreenDoubleBufferingPerfNoVSync()
{	
	dmaKit_wait(DMA_CHANNEL_GIF, 0);
	dmaKit_wait(DMA_CHANNEL_VIF1, 0);
	
	set_finish();

	owl_flush_packet();

	if(!gsGlobal->FirstFrame)
		while(!(GS_CSR_FINISH));

	GS_SETREG_CSR_FINISH(1);

	gsGlobal->FirstFrame = GS_SETTING_OFF;

	flip_screen(gsGlobal);
	//gsKit_queue_exec(gsGlobal);
	
	texture_manager_nextFrame(gsGlobal);

	processFrameCounter();
}

//////////////////////////////////////////////////////////////////////////////////////////////

static void switchFlipScreenFunction()
{
	{
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

#define posixIODriver { open, close, (int (*)(int, void *, int))read, O_RDONLY }

uint8_t __modelSupportsGetGsDxDyOffset;

short int check_rom(void)
{
	static int default_signal = -1;
	char romname[15];

	if(default_signal < 0)
	{
		_io_driver driver = posixIODriver;
		GetRomNameWithIODriver((char *)romname, &driver);
		romname[14] = '\0';

		//ROMVER string format: VVVVRTYYYYMMDD
		default_signal = (romname[4] == 'E') ? GS_MODE_PAL : GS_MODE_NTSC;
		__modelSupportsGetGsDxDyOffset = (20010608 < atoi(&romname[6]));
	}

	return default_signal;
}

static void set_buffer_attributes(GSGLOBAL *gsGlobal)
{
	int gs_DX, gs_DY, gs_DW, gs_DH;

	gsGlobal->StartXOffset = 0;
	gsGlobal->StartYOffset = 0;

	switch (gsGlobal->Mode) {
		case GS_MODE_NTSC:
			gsGlobal->StartX = 492;
			gsGlobal->StartY = 34;
			gsGlobal->DW = 2880;
			gsGlobal->DH = 480;
			break;
		case GS_MODE_PAL:
			gsGlobal->StartX = 520;
			gsGlobal->StartY = 40;
			gsGlobal->DW = 2880;
			gsGlobal->DH = 576;
			break;
		case GS_MODE_VGA_640_60:
			gsGlobal->StartX = 280;
			gsGlobal->StartY = 18;
			gsGlobal->DW = 1280;
			gsGlobal->DH = 480;
			break;
		case GS_MODE_VGA_640_72:
			gsGlobal->StartX = 330;
			gsGlobal->StartY = 18;
			gsGlobal->DW = 1280;
			gsGlobal->DH = 480;
			break;
		case GS_MODE_VGA_640_75:
			gsGlobal->StartX = 360;
			gsGlobal->StartY = 18;
			gsGlobal->DW = 1280;
			gsGlobal->DH = 480;
			break;
		case GS_MODE_VGA_640_85:
			gsGlobal->StartX = 260;
			gsGlobal->StartY = 18;
			gsGlobal->DW = 1280;
			gsGlobal->DH = 480;
			break;
		case GS_MODE_VGA_800_56:
			gsGlobal->StartX = 450;
			gsGlobal->StartY = 25;
			gsGlobal->DW = 1600;
			gsGlobal->DH = 600;
			break;
		case GS_MODE_VGA_800_60:
		case GS_MODE_VGA_800_72:
			gsGlobal->StartX = 465;
			gsGlobal->StartY = 25;
			gsGlobal->DW = 1600;
			gsGlobal->DH = 600;
			break;
		case GS_MODE_VGA_800_75:
			gsGlobal->StartX = 510;
			gsGlobal->StartY = 25;
			gsGlobal->DW = 1600;
			gsGlobal->DH = 600;
			break;
		case GS_MODE_VGA_800_85:
			gsGlobal->StartX = 500;
			gsGlobal->StartY = 25;
			gsGlobal->DW = 1600;
			gsGlobal->DH = 600;
			break;
		case GS_MODE_VGA_1024_60:
			gsGlobal->StartX = 580;
			gsGlobal->StartY = 30;
			gsGlobal->DW = 2048; // does this really need doubling? can't test
			gsGlobal->DH = 768;
			break;
		case GS_MODE_VGA_1024_70:
			gsGlobal->StartX = 266;
			gsGlobal->StartY = 30;
			gsGlobal->DW = 1024;
			gsGlobal->DH = 768;
			break;
		case GS_MODE_VGA_1024_75:
			gsGlobal->StartX = 260;
			gsGlobal->StartY = 30;
			gsGlobal->DW = 1024;
			gsGlobal->DH = 768;
			break;
		case GS_MODE_VGA_1024_85:
			gsGlobal->StartX = 290;
			gsGlobal->StartY = 30;
			gsGlobal->DW = 1024;
			gsGlobal->DH = 768;
			break;
		case GS_MODE_VGA_1280_60:
		case GS_MODE_VGA_1280_75:
			gsGlobal->StartX = 350;
			gsGlobal->StartY = 40;
			gsGlobal->DW = 1280;
			gsGlobal->DH = 1024;
			break;
		case GS_MODE_DTV_480P:
			gsGlobal->StartX = 232;
			gsGlobal->StartY = 35;
			gsGlobal->DW = 1440;
			gsGlobal->DH = 480; // though rare there are tv's that can handle an interlaced 480p source
			break;
		case GS_MODE_DTV_576P:
			gsGlobal->StartX = 255;
			gsGlobal->StartY = 44;
			gsGlobal->DW = 1440;
			gsGlobal->DH = 576;
			break;
		case GS_MODE_DTV_720P:
			gsGlobal->StartX = 306;
			gsGlobal->StartY = 24;
			gsGlobal->DW = 1280;
			gsGlobal->DH = 720;
			break;
		case GS_MODE_DTV_1080I:
			gsGlobal->StartX = 236;
			gsGlobal->StartY = 38;
			gsGlobal->DW = 1920;
			gsGlobal->DH = 1080;
			break;
	}

	if((gsGlobal->Mode == GS_MODE_NTSC || gsGlobal->Mode == GS_MODE_PAL) && (gsGlobal->Interlace == GS_NONINTERLACED)) {
		// NTSC 240p instead of 480i
		// PAL  288p instead of 576i
		gsGlobal->StartY /= 2;
		gsGlobal->DH /= 2;
	}

	gsGlobal->MagH = (gsGlobal->DW / gsGlobal->Width) - 1; // gsGlobal->DW should be a multiple of the screen width
	gsGlobal->MagV = (gsGlobal->DH / gsGlobal->Height) - 1; // gsGlobal->DH should be a multiple of the screen height

	// For other video modes, other than NTSC and PAL: if this model supports the GetGsDxDy syscall, get the board-specific offsets.
	if(gsGlobal->Mode != GS_MODE_NTSC && gsGlobal->Mode != GS_MODE_PAL)
	{
		check_rom();
		if(__modelSupportsGetGsDxDyOffset)
		{
			_GetGsDxDyOffset(gsGlobal->Mode, &gs_DX, &gs_DY, &gs_DW, &gs_DH);
			gsGlobal->StartX += gs_DX;
			gsGlobal->StartY += gs_DY;
		}
	}

	// Keep the framebuffer in the center of the screen
	gsGlobal->StartX += (gsGlobal->DW - ((gsGlobal->MagH + 1) * gsGlobal->Width )) / 2;
	gsGlobal->StartY += (gsGlobal->DH - ((gsGlobal->MagV + 1) * gsGlobal->Height)) / 2;

	if (gsGlobal->Interlace == GS_INTERLACED) {
		// Do not change odd/even start position in interlaced mode
		gsGlobal->StartY &= ~1;
	}

	// Calculate the actual display width and height
	gsGlobal->DW = (gsGlobal->MagH + 1) * gsGlobal->Width;
	gsGlobal->DH = (gsGlobal->MagV + 1) * gsGlobal->Height;

	if ((gsGlobal->Interlace == GS_INTERLACED) && (gsGlobal->Field == GS_FRAME)) {
		gsGlobal->MagV--;
	}
}

void init_screen(GSGLOBAL *gsGlobal)
{
	u64	*p_data;
	u64	*p_store;
	int	size = 19;

	if((gsGlobal->Dithering == GS_SETTING_ON) &&
	   ((gsGlobal->PSM == GS_PSM_CT16) || (gsGlobal->PSM == GS_PSM_CT16S)))
		size++;

    set_buffer_attributes(gsGlobal);

    GS_RESET();

    __asm__("sync.p; nop;");

    *GS_CSR = 0x00000000; // Clean CSR registers

    GsPutIMR(0x00007F00); // Masks all interrupts

	SetGsCrt(gsGlobal->Interlace, gsGlobal->Mode, gsGlobal->Field);

	// Fix 1080i frame mode
	if ((gsGlobal->Mode == GS_MODE_DTV_1080I) && (gsGlobal->Field == GS_FRAME))
		GS_SET_SMODE2(1, 1, 0);

	gsGlobal->FirstFrame = GS_SETTING_ON;

	if(gsGlobal->ZBuffering == GS_SETTING_OFF)
	{
		gsGlobal->Test->ZTE = GS_SETTING_ON;
		gsGlobal->Test->ZTST = 1;
	}

	DIntr(); // disable interrupts

	GS_SET_PMODE(	0,		// Read Circuit 1
			1,		// Read Circuit 2
			0,		// Use ALP Register for Alpha Blending
			1,		// Alpha Value of Read Circuit 2 for Output Selection
			0,		// Blend Alpha with output of Read Circuit 2
			0x80);		// Alpha Value = 1.0

	GS_SET_DISPFB1(	0,			// Frame Buffer Base Pointer (Address/2048)
			gsGlobal->Width / 64,	// Buffer Width (Address/64)
			gsGlobal->PSM,		// Pixel Storage Format
			0,			// Upper Left X in Buffer
			0);

	GS_SET_DISPFB2(	0,			// Frame Buffer Base Pointer (Address/2048)
			gsGlobal->Width / 64,	// Buffer Width (Address/64)
			gsGlobal->PSM,		// Pixel Storage Format
			0,			// Upper Left X in Buffer
			0);			// Upper Left Y in Buffer

	GS_SET_DISPLAY1(
			gsGlobal->StartX + gsGlobal->StartXOffset,	// X position in the display area (in VCK unit
			gsGlobal->StartY + gsGlobal->StartYOffset,	// Y position in the display area (in Raster u
			gsGlobal->MagH,			// Horizontal Magnification
			gsGlobal->MagV,			// Vertical Magnification
			gsGlobal->DW - 1,	// Display area width
			gsGlobal->DH - 1);		// Display area height

	GS_SET_DISPLAY2(
			gsGlobal->StartX + gsGlobal->StartXOffset,	// X position in the display area (in VCK units)
			gsGlobal->StartY + gsGlobal->StartYOffset,	// Y position in the display area (in Raster units)
			gsGlobal->MagH,			// Horizontal Magnification
			gsGlobal->MagV,			// Vertical Magnification
			gsGlobal->DW - 1,	// Display area width
			gsGlobal->DH - 1);		// Display area height

	GS_SET_BGCOLOR(	gsGlobal->BGColor->Red,		// Red
			gsGlobal->BGColor->Green,	// Green
			gsGlobal->BGColor->Blue);	// Blue

    EIntr(); //enable interrupts

    gsGlobal->CurrentPointer = 0; // reset vram pointer too

	// Context 1
	gsGlobal->ScreenBuffer[0] = gsKit_vram_alloc( gsGlobal, gsKit_texture_size(gsGlobal->Width, gsGlobal->Height, gsGlobal->PSM), GSKIT_ALLOC_SYSBUFFER );

	if(gsGlobal->DoubleBuffering == GS_SETTING_OFF)
	{
		gsGlobal->ScreenBuffer[1] = gsGlobal->ScreenBuffer[0];
	}
	else
		// Context 2
		gsGlobal->ScreenBuffer[1] = gsKit_vram_alloc( gsGlobal, gsKit_texture_size(gsGlobal->Width, gsGlobal->Height, gsGlobal->PSM), GSKIT_ALLOC_SYSBUFFER );


	if(gsGlobal->ZBuffering == GS_SETTING_ON)
		gsGlobal->ZBuffer = gsKit_vram_alloc( gsGlobal, gsKit_texture_size(gsGlobal->Width, gsGlobal->Height, gsGlobal->PSMZ), GSKIT_ALLOC_SYSBUFFER ); // Z Buffer

    gsGlobal->TexturePointer = gsGlobal->CurrentPointer; // first useable address for textures

	p_data = p_store = (u64 *)gsGlobal->dma_misc;

	*p_data++ = GIF_TAG( size - 1, 1, 0, 0, GSKIT_GIF_FLG_PACKED, 1 );
	*p_data++ = GIF_AD;

	*p_data++ = 1;
	*p_data++ = GS_PRMODECONT;

	*p_data++ = GS_SETREG_FRAME_1( gsGlobal->ScreenBuffer[0] / 8192, gsGlobal->Width / 64, gsGlobal->PSM, 0 );
	*p_data++ = GS_FRAME_1;

	*p_data++ = GS_SETREG_XYOFFSET_1( gsGlobal->OffsetX,
					  gsGlobal->OffsetY);
	*p_data++ = GS_XYOFFSET_1;

	*p_data++ = GS_SETREG_SCISSOR_1( 0, gsGlobal->Width - 1, 0, gsGlobal->Height - 1 );
	*p_data++ = GS_SCISSOR_1;

	*p_data++ = GS_SETREG_TEST( gsGlobal->Test->ATE, gsGlobal->Test->ATST,
				gsGlobal->Test->AREF, gsGlobal->Test->AFAIL,
				gsGlobal->Test->DATE, gsGlobal->Test->DATM,
				gsGlobal->Test->ZTE, gsGlobal->Test->ZTST );

	*p_data++ = GS_TEST_1;

	*p_data++ = GS_SETREG_CLAMP(gsGlobal->Clamp->WMS, gsGlobal->Clamp->WMT,
				gsGlobal->Clamp->MINU, gsGlobal->Clamp->MAXU,
				gsGlobal->Clamp->MINV, gsGlobal->Clamp->MAXV);

	*p_data++ = GS_CLAMP_1;

	if(gsGlobal->ZBuffering == GS_SETTING_ON)
	{
	    if((gsGlobal->PSM == GS_PSM_CT16) && (gsGlobal->PSMZ != GS_PSMZ_16))
            gsGlobal->PSMZ = GS_PSMZ_16; // seems only non-S 16-bit z depth works with this mode
        if((gsGlobal->PSM != GS_PSM_CT16) && (gsGlobal->PSMZ == GS_PSMZ_16))
            gsGlobal->PSMZ = GS_PSMZ_16S; // other depths don't seem to work with 16-bit non-S z depth

		*p_data++ = GS_SETREG_ZBUF_1( gsGlobal->ZBuffer / 8192, gsGlobal->PSMZ, 0 );
		*p_data++ = GS_ZBUF_1;
	}
	if(gsGlobal->ZBuffering == GS_SETTING_OFF)
	{
		*p_data++ = GS_SETREG_ZBUF_1( 0, gsGlobal->PSM, 1 );
		*p_data++ = GS_ZBUF_1;
	}

	*p_data++ = GS_SETREG_COLCLAMP( 255 );
	*p_data++ = GS_COLCLAMP;

	*p_data++ = GS_SETREG_FRAME_1( gsGlobal->ScreenBuffer[1] / 8192, gsGlobal->Width / 64, gsGlobal->PSM, 0 );
	*p_data++ = GS_FRAME_2;

	*p_data++ = GS_SETREG_XYOFFSET_1( gsGlobal->OffsetX,
					  gsGlobal->OffsetY);
	*p_data++ = GS_XYOFFSET_2;

	*p_data++ = GS_SETREG_SCISSOR_1( 0, gsGlobal->Width - 1, 0, gsGlobal->Height - 1);
	*p_data++ = GS_SCISSOR_2;

	*p_data++ = GS_SETREG_TEST( gsGlobal->Test->ATE, gsGlobal->Test->ATST,
				gsGlobal->Test->AREF, gsGlobal->Test->AFAIL,
				gsGlobal->Test->DATE, gsGlobal->Test->DATM,
				gsGlobal->Test->ZTE, gsGlobal->Test->ZTST );

	*p_data++ = GS_TEST_2;

	*p_data++ = GS_SETREG_CLAMP(gsGlobal->Clamp->WMS, gsGlobal->Clamp->WMT,
				gsGlobal->Clamp->MINU, gsGlobal->Clamp->MAXU,
				gsGlobal->Clamp->MINV, gsGlobal->Clamp->MAXV);

	*p_data++ = GS_CLAMP_2;

	if(gsGlobal->ZBuffering == GS_SETTING_ON)
	{
		*p_data++ = GS_SETREG_ZBUF_1( gsGlobal->ZBuffer / 8192, gsGlobal->PSMZ, 0 );
		*p_data++ = GS_ZBUF_2;
	}
	if(gsGlobal->ZBuffering == GS_SETTING_OFF)
	{
		*p_data++ = GS_SETREG_ZBUF_1( 0, gsGlobal->PSM, 1 );
		*p_data++ = GS_ZBUF_2;
	}

	*p_data++ = GS_ALPHA_BLEND_NORMAL;
	*p_data++ = GS_ALPHA_1;

	*p_data++ = GS_ALPHA_BLEND_NORMAL;
	*p_data++ = GS_ALPHA_2;

	*p_data++ = GS_SETREG_DIMX(gsGlobal->DitherMatrix[0],gsGlobal->DitherMatrix[1],
                gsGlobal->DitherMatrix[2],gsGlobal->DitherMatrix[3],gsGlobal->DitherMatrix[4],
                gsGlobal->DitherMatrix[5],gsGlobal->DitherMatrix[6],gsGlobal->DitherMatrix[7],
                gsGlobal->DitherMatrix[8],gsGlobal->DitherMatrix[9],gsGlobal->DitherMatrix[10],
                gsGlobal->DitherMatrix[11],gsGlobal->DitherMatrix[12],gsGlobal->DitherMatrix[13],
                gsGlobal->DitherMatrix[14],gsGlobal->DitherMatrix[15]); // 4x4 dither matrix

	*p_data++ = GS_DIMX;

	*p_data++ = GS_SETREG_TEXA(0x00, 0, 0x80);
	*p_data++ = GS_TEXA;

	if((gsGlobal->Dithering == GS_SETTING_ON) && ((gsGlobal->PSM == GS_PSM_CT16) || (gsGlobal->PSM == GS_PSM_CT16S))) {
        *p_data++ = 1;
        *p_data++ = GS_DTHE;
	}

	dmaKit_send_ucab(DMA_CHANNEL_GIF, p_store, size);
	dmaKit_wait_fast();
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

	sync_screen(gsGlobal);
	flip_screen(gsGlobal);
}
 
void init_graphics()
{
	ee_sema_t sema; 
    sema.init_count = 0;
    sema.max_count = 1;
    sema.option = 0;
    vsync_sema_id = CreateSema(&sema);

	gsGlobal = gsKit_init_global_custom(RENDER_QUEUE_OS_POOLSIZE, RENDER_QUEUE_PER_POOLSIZE);

	gsGlobal->Mode = check_rom();
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

	init_screen(gsGlobal);

	//gsKit_set_clamp(gsGlobal, GS_CMODE_REPEAT);

	texture_manager_init(gsGlobal);

	gsKit_add_vsync_handler(vsync_handler);

	owl_init(owl_packet_buffer, OWL_PACKET_BUFFER_SIZE);

	for (int i = 0; i < 2; i++) {
    	clearScreen(BLACK_RGBAQ);	
		flipScreen();
	}
}

void graphicWaitVblankStart(){

	gsKit_vsync_wait();

}
