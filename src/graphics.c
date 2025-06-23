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

#include <texture_manager.h>

static const u64 BLACK_RGBAQ   = GS_SETREG_RGBAQ(0x00,0x00,0x00,0x80,0x00);

GSCONTEXT *gsGlobal = NULL;

GSSURFACE draw_buffer;
GSSURFACE display_buffer;
GSSURFACE depth_buffer;

GSSURFACE *cur_screen_buffer[3] = { NULL };
GSSURFACE *main_screen_buffer[3] = { NULL };

void (*flipScreen)();

static bool vsync = true;
static bool perf = false;
static bool hires = false;
static int vsync_sema_id = 0;
static clock_t curtime = 0;
static float fps = 0.0f;

static int frames = 0;
static int frame_interval = -1;

#define OWL_PACKET_BUFFER_SIZE 2048

static owl_qword owl_packet_buffer[OWL_PACKET_BUFFER_SIZE] qw_aligned = { 0 };

uint64_t gs_reg_cache[GS_CACHE_SIZE];

const uint8_t gs_reg_map[] = {
	GS_REG_TEX0,
	GS_REG_TEX0_2,
	GS_REG_CLAMP,
	GS_REG_CLAMP_2,
	GS_REG_TEX1,
	GS_REG_TEX1_2,
	GS_REG_TEX2,
	GS_REG_TEX2_2,
	GS_REG_XYOFFSET,
	GS_REG_XYOFFSET_2,
	GS_REG_PRMODECONT,
	GS_REG_PRMODE,
	GS_REG_TEXCLUT,
	GS_REG_MIPTBP1,
	GS_REG_MIPTBP1_2,
	GS_REG_MIPTBP2,
	GS_REG_MIPTBP2_2,
	GS_REG_TEXA,
	GS_REG_SCISSOR,
	GS_REG_SCISSOR_2,
	GS_REG_ALPHA,
	GS_REG_ALPHA_2,
	GS_REG_DIMX,
	GS_REG_DTHE,
	GS_REG_COLCLAMP,
	GS_REG_TEST,
	GS_REG_TEST_2,
	GS_REG_PABE,
	GS_REG_FBA,
	GS_REG_FBA_2,
	GS_REG_FRAME,
	GS_REG_FRAME_2,
	GS_REG_ZBUF,
	GS_REG_ZBUF_2
};

// TODO: USE MMI INSTEAD!

static inline uint8_t
from8to5(uint8_t c, const int8_t dither)
{
	if((c+dither) > 255)
		return 255>>3;
	else if((c+dither) < 0)
		return 0;
	else
		return (c+dither)>>3;
}

static inline uint16_t
fromRGBto16(const int8_t dither, uint8_t r, uint8_t g, uint8_t b)
{
	// A1 B5 G5 R5
	return 0x8000 | (from8to5(b, dither)<<10) | (from8to5(g, dither)<<5) | from8to5(r, dither);
}

static inline uint16_t
from24to16(const int8_t dither, uint8_t *c)
{
	return fromRGBto16(dither, c[0], c[1], c[2]);
}

void athena_texture_optimize(GSSURFACE *Texture)
{
	int x, y;
	const int8_t dither_matrix[16] = {-4,2,-3,3,0,-2,1,-1,-3,3,-4,2,1,-1,0,-2};
	size_t size;
	uint16_t *pixels16;
	uint8_t  *pixels24;

	// Only 24bit to 16bit supported
	if (Texture->PSM != GS_PSM_CT24)
		return;

	size = Texture->Width * Texture->Height * 2;
	pixels16 = (uint16_t*)memalign(128, size);
	pixels24 = (uint8_t *)Texture->Mem;

	for(y=0; y < Texture->Height; y++) {
		for(x=0; x < Texture->Width; x++) {
			int i = y * Texture->Width + x;
			int8_t dither = dither_matrix[(y&3)*4+(x&3)];
			pixels16[i] = from24to16(dither, &pixels24[i*3]);
		}
	}

	free(Texture->Mem);
	Texture->Mem = (void*)pixels16;
	Texture->PSM = GS_PSM_CT16S;
}

uint32_t athena_surface_size(int width, int height, int psm)
{
	switch (psm) {
		case GS_PSM_CT32:  
		case GS_PSM_CT24:
		case GS_PSMZ_32:  
		case GS_PSMZ_24:
			return (width*height*4);
		case GS_PSM_CT16:  
		case GS_PSM_CT16S:
		case GS_PSMZ_16:  
		case GS_PSMZ_16S:
			return (width*height*2);
		case GS_PSM_T8:    
			return (width*height  );
		case GS_PSM_T4:    
			return (width*height/2);
	}

	return -1;
}

uint32_t athena_vram_surface_size(int width, int height, int psm)
{
	int widthBlocks, heightBlocks;
	int widthAlign, heightAlign;

	// Calculate the number of blocks width and height
	// A block is 256 bytes in size
	switch (psm) {
		case GS_PSM_CT32:
		case GS_PSM_CT24:
		case GS_PSMZ_32:
		case GS_PSMZ_24:
			// 1 block = 8x8 pixels
			widthBlocks  = (width  + 7) / 8;
			heightBlocks = (height + 7) / 8;
			break;
		case GS_PSM_CT16:
		case GS_PSM_CT16S:
		case GS_PSMZ_16:
		case GS_PSMZ_16S:
			// 1 block = 16x8 pixels
			widthBlocks  = (width  + 15) / 16;
			heightBlocks = (height +  7) /  8;
			break;
		case GS_PSM_T8:
			// 1 block = 16x16 pixels
			widthBlocks  = (width  + 15) / 16;
			heightBlocks = (height + 15) / 16;
			break;
		case GS_PSM_T4:
			// 1 block = 32x16 pixels
			widthBlocks  = (width  + 31) / 32;
			heightBlocks = (height + 15) / 16;
			break;
		default:
			printf("gsKit: unsupported PSM %d\n", psm);
			return -1;
	}

	// Calculate the minimum block alignment
	if(psm == GS_PSM_CT32 || psm == GS_PSM_CT24 || psm == GS_PSMZ_32 || psm == GS_PSMZ_24 || psm == GS_PSM_T8) {
		// 8x4 blocks in a page.
		// block traversing order:
		// 0145....
		// 2367....
		// ........
		// ........
		if(widthBlocks <= 2 && heightBlocks <= 1) {
			widthAlign = 1;
			heightAlign = 1;
		}
		else if(widthBlocks <= 4 && heightBlocks <= 2) {
			widthAlign = 2;
			heightAlign = 2;
		}
		else if(widthBlocks <= 8 && heightBlocks <= 4) {
			widthAlign = 4;
			heightAlign = 4;
		}
		else {
			widthAlign = 8;
			heightAlign = 4;
		}
	}
	else if(psm == GS_PSM_CT16 || psm == GS_PSMZ_16 || psm == GS_PSM_T4) {
		// 4x8 blocks in a page.
		// block traversing order:
		// 02..
		// 13..
		// 46..
		// 57..
		// ....
		// ....
		// ....
		// ....
		if(widthBlocks <= 1 && heightBlocks <= 2) {
			widthAlign = 1;
			heightAlign = 1;
		}
		else if(widthBlocks <= 2 && heightBlocks <= 4) {
			widthAlign = 2;
			heightAlign = 2;
		}
		else if(widthBlocks <= 4 && heightBlocks <= 8) {
			widthAlign = 4;
			heightAlign = 4;
		}
		else {
			widthAlign = 4;
			heightAlign = 8;
		}
	}
	else /* if(psm == GS_PSM_CT16S) */ {
		// 4x8 blocks in a page.
		// block traversing order:
		// 02..
		// 13..
		// ....
		// ....
		// 46..
		// 57..
		// ....
		// ....
		if(widthBlocks <= 1 && heightBlocks <= 2) {
			widthAlign = 1;
			heightAlign = 1;
		}
		else if(widthBlocks <= 2 && heightBlocks <= 2) {
			widthAlign = 2;
			heightAlign = 2;
		}
		else if(widthBlocks <= 2 && heightBlocks <= 8) {
			widthAlign = 2;
			heightAlign = 8;
		}
		else {
			widthAlign = 4;
			heightAlign = 8;
		}
	}

	widthBlocks  = (-widthAlign)  & (widthBlocks  + widthAlign  - 1);
	heightBlocks = (-heightAlign) & (heightBlocks + heightAlign - 1);

	return widthBlocks * heightBlocks * 256;
}

void athena_calculate_tbw(GSSURFACE *Texture)
{
	if(Texture->PSM == GS_PSM_T8 || Texture->PSM == GS_PSM_T4)
	{
		Texture->TBW = (-GS_VRAM_TBWALIGN_CLUT)&(Texture->Width+GS_VRAM_TBWALIGN_CLUT-1);
		if(Texture->TBW / 64 > 0)
			Texture->TBW = (Texture->TBW / 64);
		else
			Texture->TBW = 1;
	}
	else
	{
		Texture->TBW = (-GS_VRAM_TBWALIGN)&(Texture->Width+GS_VRAM_TBWALIGN-1);
		if(Texture->TBW / 64 > 0)
			Texture->TBW = (Texture->TBW / 64);
		else
			Texture->TBW = 1;
	}
}

void gs_copy_block(GSSURFACE *src, int src_x, int src_y, GSSURFACE *dst, int dst_x, int dst_y) {
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 6);
 
	owl_add_cnt_tag(packet, 5, owl_vif_code_double(VIF_CODE(5, 0, VIF_DIRECT, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

    owl_add_tag(packet, GIF_AD, GIFTAG(4, 1, 0, 0, 0, 1));
    
    owl_add_tag(packet, GS_BITBLTBUF, GS_SETREG_BITBLTBUF(src->Vram / 256, 
                                            src->TBW, 
                                            src->PSM, 
                                            dst->Vram / 256, 
                                            dst->TBW, 
                                            dst->PSM));

    owl_add_tag(packet, GS_TRXPOS, GS_SETREG_TRXPOS(src_x, src_y, dst_x, dst_y, 0)); 
    
    owl_add_tag(packet, GS_TRXREG, GS_SETREG_TRXREG(dst->Width, dst->Height)); 
    
    owl_add_tag(packet, GS_TRXDIR, 2); 
}

void gs_channel_shuffle_slow(GSSURFACE *dst, uint32_t in, uint32_t out, uint32_t blockX, uint32_t blockY, GSSURFACE *source, uint32_t width, uint32_t height, GSSURFACE *pal) {
    int tw, th;

	// For the BLUE and ALPHA channels, we need to offset our 'U's by 8 texels
	const uint32_t horz_block_offset = (in == CHANNEL_SHUFFLE_BLUE || in == CHANNEL_SHUFFLE_ALPHA);
	// For the GREEN and ALPHA channels, we need to offset our 'T's by 2 texels
	const uint32_t vert_block_offset = (in == CHANNEL_SHUFFLE_GREEN || in == CHANNEL_SHUFFLE_ALPHA);

	const uint32_t clamp_horz = horz_block_offset ? 8 : 0;
	const uint32_t clamp_vert = vert_block_offset ? 2 : 0;

    owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 502);

	owl_add_cnt_tag(packet, 501, 0);

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(500, 0, VIF_DIRECT, 0)); 

    owl_add_tag(packet, GIF_AD, GIF_SET_TAG(5, 1, 0, 0, 0, 1));

    owl_add_tag(packet, GS_XYOFFSET_1, GS_SETREG_XYOFFSET_1(0, 0));

    athena_set_tw_th(width, height, &tw, &th);

    owl_add_tag(packet, GS_TEX0_1, GS_SETREG_TEX0(source->Vram/256, source->TBW, GS_PSM_T8, tw, th, 1, COLOR_DECAL, pal->Vram/256, GS_PSM_CT32, 0, 0, 1));

    owl_add_tag(packet, GS_CLAMP_1, GS_SETREG_CLAMP(3, 3, 0xF7, clamp_horz, 0xFD, clamp_vert));

    owl_add_tag(packet, GS_TEXFLUSH, 1);

    owl_add_tag(packet, GS_FRAME_1, GS_SETREG_FRAME(dst->Vram/8192, dst->TBW, GS_PSM_CT32, ~out));

    owl_add_tag(packet, (GS_UV) | (GS_XYZ2 << 4) | (GS_UV << 8) | (GS_XYZ2 << 12), 
             GIF_SET_TAG(96, 1, 1, GS_SETREG_PRIM(GS_PRIM_SPRITE, 0, 1, 0, 0, 0, 1, 0, 0), 0, 4)
    );

    int y;
	for (y = 0; y < 32; y += 2)
	{
		if (((y % 4) == 0) ^ (vert_block_offset == 1)) // Even (4 16x2 sprites)
		{
            int x;
			for (x = 0; x < 64; x += 16)
			{
				// UV
                owl_add_tag(packet, 0, GS_SETREG_STQ(8 + ((8 + x * 2) << 4), 8 + ((y * 2) << 4)));
				// XYZ2
                owl_add_tag(packet, 1, (uint64_t)((x + blockX) << 4) | ((uint64_t)((y + blockY) << 4) << 32));
				// UV
                owl_add_tag(packet, 0, GS_SETREG_STQ(8 + ((24 + x * 2) << 4), 8 + ((2 + y * 2) << 4)));
				// XYZ2
                owl_add_tag(packet, 1, (uint64_t)((x + 16 + blockX) << 4) | ((uint64_t)((y + 2 + blockY) << 4) << 32));
			}
		}
		else // Odd (Eight 8x2 sprites)
		{
            int x;
			for (x = 0; x < 64; x += 8)
			{
				// UV
                owl_add_tag(packet, 0, GS_SETREG_STQ(8 + ((4 + x * 2) << 4), 8 + ((y * 2) << 4)));
				// XYZ2
                owl_add_tag(packet, 1, (uint64_t)((x + blockX) << 4) | ((uint64_t)((y + blockY) << 4) << 32));
				// UV
                owl_add_tag(packet, 0, GS_SETREG_STQ(8 + ((12 + x * 2) << 4), 8 + ((2 + y * 2) << 4)));
				// XYZ2
                owl_add_tag(packet, 1, (uint64_t)((x + 8 + blockX) << 4) | ((uint64_t)((y + 2 + blockY) << 4) << 32));
			}
		}
	}

    owl_add_tag(packet, GIF_AD, GIF_SET_TAG(3, 1, 0, 0, 0, 1));

    owl_add_tag(packet, GS_CLAMP_1, gs_reg_cache[GS_CACHE_CLAMP]);

    owl_add_tag(packet, GS_FRAME_1, gs_reg_cache[GS_CACHE_FRAME]);

    owl_add_tag(packet, GS_XYOFFSET_1, gs_reg_cache[GS_CACHE_XYOFFSET]);
}

void set_screen_buffer(eScreenBuffers id, GSSURFACE *buf, uint32_t mask) {
	switch (id) {
		case DRAW_BUFFER:
			set_register(GS_CACHE_FRAME+gsGlobal->PrimContext, GS_SETREG_FRAME_1( buf->Vram / 8192, buf->TBW, buf->PSM, mask ));
			break;
		case DISPLAY_BUFFER:
			break;
		case DEPTH_BUFFER:
			set_register(GS_CACHE_ZBUF+gsGlobal->PrimContext, GS_SETREG_ZBUF_1( buf->Vram / 8192, buf->PSM-0x30, mask ));
			break;
	}

	if (!gsGlobal->PrimContext) {
		cur_screen_buffer[id] = buf;
	}
}

int screen_switch_context() {
	gsGlobal->PrimContext ^= 1;
	return gsGlobal->PrimContext;
}

void set_screen_param(uint8_t param, uint64_t value) {
	test_reg test = { .data = get_register(GS_CACHE_TEST+gsGlobal->PrimContext) };

	switch (param) {
		case ALPHA_TEST_ENABLE:
			test.fields.alpha_test_enabled = (bool)value;
			break;
		case ALPHA_TEST_METHOD:
			test.fields.alpha_test_method = (int)value;
			break;
		case ALPHA_TEST_REF:
			test.fields.alpha_test_ref = (uint8_t)value;
			break;
		case ALPHA_TEST_FAIL:
			test.fields.alpha_fail_processing = (int)value;
			break;
		case DST_ALPHA_TEST_ENABLE:
			test.fields.dest_alpha_test_enabled = (bool)value;
			break;
		case DST_ALPHA_TEST_METHOD:
			test.fields.dest_alpha_test_method = (int)value;
			break;
		case DEPTH_TEST_ENABLE:
			if (value) {
				test.fields.depth_test_enabled = (bool)value;
			} else {
				test.fields.depth_test_enabled = true;
				test.fields.depth_test_method = DEPTH_ALWAYS;
			}
			break;
		case DEPTH_TEST_METHOD:
			test.fields.depth_test_method = (int)value;
			break;
		case ALPHA_BLEND_EQUATION: 
			set_register(GS_CACHE_ALPHA+gsGlobal->PrimContext, value);
			return;
		case SCISSOR_BOUNDS:
			set_register(GS_CACHE_SCISSOR+gsGlobal->PrimContext, value);
			return;
		case PIXEL_ALPHA_BLEND_ENABLE:
			set_register(GS_CACHE_PABE, value);
			return;
		case COLOR_CLAMP_MODE:
			set_register(GS_CACHE_COLCLAMP, value);
			return;
	}

	set_register(GS_CACHE_TEST+gsGlobal->PrimContext, test.data);
}

uint64_t get_screen_param(uint8_t param) {
	test_reg test = { .data = get_register(GS_CACHE_TEST+gsGlobal->PrimContext) };

	switch (param) {
		case ALPHA_TEST_ENABLE:
			return test.fields.alpha_test_enabled;
		case ALPHA_TEST_METHOD:
			return test.fields.alpha_test_method;
		case ALPHA_TEST_REF:
			return test.fields.alpha_test_ref;
		case ALPHA_TEST_FAIL:
			return test.fields.alpha_fail_processing;
		case DST_ALPHA_TEST_ENABLE:
			return test.fields.dest_alpha_test_enabled;
		case DST_ALPHA_TEST_METHOD:
			return test.fields.dest_alpha_test_method;
		case DEPTH_TEST_ENABLE:
			if (test.fields.depth_test_enabled && (test.fields.depth_test_method == DEPTH_ALWAYS)) {
				return false;
			}
			return test.fields.depth_test_enabled;
		case DEPTH_TEST_METHOD:
			return test.fields.depth_test_method;
		case ALPHA_BLEND_EQUATION:
			return get_register(GS_CACHE_ALPHA+gsGlobal->PrimContext);
		case SCISSOR_BOUNDS:
			return get_register(GS_CACHE_SCISSOR+gsGlobal->PrimContext);
		case PIXEL_ALPHA_BLEND_ENABLE:
			return get_register(GS_CACHE_PABE);
		case COLOR_CLAMP_MODE:
			return get_register(GS_CACHE_COLCLAMP);
	}
}

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

void set_register(int reg_id, uint64_t data) {
	if (gs_reg_cache[reg_id] == data) return;

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 4);
 
	owl_add_cnt_tag(packet, 3, 0);

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(2, 0, VIF_DIRECT, 0)); 

	owl_add_tag(packet, GIF_AD, VU_GS_GIFTAG(1, 1, NULL, 0, 0, 0, 1));

	owl_add_tag(packet, gs_reg_map[reg_id], data);

	gs_reg_cache[reg_id] = data;
}

void flush_gs_texcache() {
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 4);
 
	owl_add_cnt_tag(packet, 3, 0);

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(2, 0, VIF_DIRECT, 0)); 

	owl_add_tag(packet, GIF_AD, VU_GS_GIFTAG(1, 1, NULL, 0, 0, 0, 1));

	owl_add_tag(packet, GS_TEXFLUSH, 1);
}

uint64_t get_register(int reg_id) {
	return gs_reg_cache[reg_id];
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

	owl_add_tag(packet, GS_TEST_1+gsGlobal->PrimContext, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, 1)); // Ignore cache because it is a single operation
	//owl_add_tag(packet, GS_SCISSOR_1, GS_SETREG_SCISSOR(0, 64 - 1, 0, 2048 - 1));
	owl_add_tag(packet, GS_XYOFFSET_1+gsGlobal->PrimContext, GS_SETREG_XYOFFSET(0, 0));

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
	
	owl_add_tag(packet, GS_TEST_1, get_register(GS_CACHE_TEST+gsGlobal->PrimContext));
	owl_add_tag(packet, GS_XYOFFSET_1, get_register(GS_CACHE_XYOFFSET+gsGlobal->PrimContext));
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

int getFreeVRAM(int mode) {
	switch (mode) {
		case VRAM_SIZE:
			return 4*1024*1024;
		case VRAM_USED_TOTAL:
			return texture_manager_used_memory();
		case VRAM_USED_STATIC:
			return texture_manager_get_locked_memory();
		case VRAM_USED_DYNAMIC:
			return texture_manager_get_unlocked_memory();
	}

	return 0;
}

int GetInterlacedFrameMode()
{
    if ((gsGlobal->Interlace == GS_INTERLACED) && (gsGlobal->Field == GS_FRAME))
        return 1;

    return 0;
}
GSCONTEXT *getGSGLOBAL(){ return gsGlobal; }

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

void setactive(GSCONTEXT *gsGlobal)
{
	owl_packet *packet = owl_query_packet(DMA_CHANNEL_VIF1, 5);

	owl_add_cnt_tag(packet, 4, 0);

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(3, 0, VIF_DIRECT, 0)); // 3 giftags

	owl_add_tag(packet, GIF_AD, VU_GS_GIFTAG(2, 1, NULL, 0, 0, 0, 1));

	draw_buffer.Vram = gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1];
	display_buffer.Vram = gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer];

	gs_reg_cache[GS_CACHE_SCISSOR] = GS_SETREG_SCISSOR_1( 0, gsGlobal->Width - 1, 0, gsGlobal->Height - 1 );
	gs_reg_cache[GS_CACHE_FRAME] = GS_SETREG_FRAME_1( draw_buffer.Vram / 8192, gsGlobal->Width / 64, gsGlobal->PSM, 0 );

	// Context 1
	owl_add_tag(packet, GS_SCISSOR_1, gs_reg_cache[GS_CACHE_SCISSOR]);
	owl_add_tag(packet, GS_FRAME_1, gs_reg_cache[GS_CACHE_FRAME]);
}

/* Copy of sync_screen_flip, but without the 'flip' */
static void sync_screen(GSCONTEXT *gsGlobal)
{
   if (!gsGlobal->FirstFrame) WaitSema(vsync_sema_id);
   while (PollSema(vsync_sema_id) >= 0)
   	;
}

/* Copy of sync_screen_flip, but without the 'sync' */
static void flip_screen(GSCONTEXT *gsGlobal)
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

    if (errMsg)
    {
        printf("AthenaEnv ERROR!\n%s", errMsg);

        if (strstr(errMsg, "EvalError")) {
            color = GS_SETREG_RGBAQ(0x56,0x71,0x7D,0x80,0x00);
        } else if (strstr(errMsg, "SyntaxError")) {
            color = GS_SETREG_RGBAQ(0x20,0x60,0xB0,0x80,0x00);
        } else if (strstr(errMsg, "TypeError")) {
            color = GS_SETREG_RGBAQ(0x3b,0x81,0x32,0x80,0x00);
        } else if (strstr(errMsg, "ReferenceError")) {
            color = GS_SETREG_RGBAQ(0xE5,0xDE,0x00,0x80,0x00);
        } else if (strstr(errMsg, "RangeError")) {
            color = GS_SETREG_RGBAQ(0xD0,0x31,0x3D,0x80,0x00);
        } else if (strstr(errMsg, "InternalError")) {
            color = GS_SETREG_RGBAQ(0x8A,0x00,0xC2,0x80,0x00);
        } else if (strstr(errMsg, "URIError")) {
            color = GS_SETREG_RGBAQ(0xFF,0x78,0x1F,0x80,0x00);
        } else if (strstr(errMsg, "AggregateError")) {
            color = GS_SETREG_RGBAQ(0xE2,0x61,0x9F,0x80,0x00);
        } else if (strstr(errMsg, "AthenaError")) {
            color = GS_SETREG_RGBAQ(0x70,0x29,0x63,0x80,0x00);
        }

        if(dark_mode) {
            color2 = color;
            color = GS_SETREG_RGBAQ(0x20,0x20,0x20,0x80,0x00);
        } else {
            color2 = GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00);
        }

		int slot = fntLoadFile(NULL);
		fntSetCharSize(slot, FNTSYS_CHAR_SIZE*64, FNTSYS_CHAR_SIZE*64);

		set_screen_param(DEPTH_TEST_ENABLE, false);

    	while (!isButtonPressed(PAD_START)) {
			clearScreen(color);
			fntRenderString(slot, 15, 15, 0, 625, 448, "AthenaEnv ERROR!", 1.2f, color2);
			fntRenderString(slot, 15, 80, 0, 625, 448, errMsg, 0.8f, color2);
			fntRenderString(slot, 15, 400, 0, 625, 448, "Press [start] to restart", 0.8f, color2);
			flipScreen();
		} 

		set_screen_param(DEPTH_TEST_ENABLE, true);
		set_screen_param(DEPTH_TEST_METHOD, DEPTH_GEQUAL);
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
	sync_screen(gsGlobal);

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

	if(!gsGlobal->FirstFrame)
		while(!(GS_CSR_FINISH));

	GS_SETREG_CSR_FINISH(1);

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

static void set_buffer_attributes(GSCONTEXT *gsGlobal)
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

void init_screen(GSCONTEXT *gsGlobal)
{
	u64	*p_data;
	u64	*p_store;
	int	size = 18;

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

	p_data = p_store = (u64 *)gsGlobal->dma_misc;

	*p_data++ = GIF_TAG( size - 1, 1, 0, 0, GSKIT_GIF_FLG_PACKED, 1 );
	*p_data++ = GIF_AD;

	*p_data++ = 1;
	*p_data++ = GS_PRMODECONT;

	*p_data++ = gs_reg_cache[GS_CACHE_FRAME] = GS_SETREG_FRAME_1( gsGlobal->ScreenBuffer[0] / 8192, gsGlobal->Width / 64, gsGlobal->PSM, 0 );
	*p_data++ = GS_FRAME_1;

	*p_data++ = gs_reg_cache[GS_CACHE_XYOFFSET] = GS_SETREG_XYOFFSET_1( gsGlobal->OffsetX,
					  gsGlobal->OffsetY);
	*p_data++ = GS_XYOFFSET_1;

	*p_data++ = gs_reg_cache[GS_CACHE_SCISSOR] = GS_SETREG_SCISSOR_1( 0, gsGlobal->Width - 1, 0, gsGlobal->Height - 1 );
	*p_data++ = GS_SCISSOR_1;

	*p_data++ = gs_reg_cache[GS_CACHE_TEST] = GS_SETREG_TEST(0, 1, 0x80, 0, 0, 0, 1, 2);

	*p_data++ = GS_TEST_1;

	*p_data++ = gs_reg_cache[GS_CACHE_CLAMP] = GS_SETREG_CLAMP(GS_CMODE_REPEAT, GS_CMODE_REPEAT, 0, 0, 0, 0);

	*p_data++ = GS_CLAMP_1;

	if(gsGlobal->ZBuffering == GS_SETTING_ON)
	{
	    if((gsGlobal->PSM == GS_PSM_CT16) && (gsGlobal->PSMZ != GS_ZBUF_16))
            gsGlobal->PSMZ = GS_ZBUF_16; // seems only non-S 16-bit z depth works with this mode
        if((gsGlobal->PSM != GS_PSM_CT16) && (gsGlobal->PSMZ == GS_ZBUF_16))
            gsGlobal->PSMZ = GS_ZBUF_16S; // other depths don't seem to work with 16-bit non-S z depth

		*p_data++ = gs_reg_cache[GS_CACHE_ZBUF] = GS_SETREG_ZBUF_1( gsGlobal->ZBuffer / 8192, gsGlobal->PSMZ, 0 );
	} else {
		*p_data++ = gs_reg_cache[GS_CACHE_ZBUF] = GS_SETREG_ZBUF_1( 0, gsGlobal->PSM, 1 );
	}
	*p_data++ = GS_ZBUF_1;

	*p_data++ = gs_reg_cache[GS_CACHE_COLCLAMP] = GS_SETREG_COLCLAMP( 255 );
	*p_data++ = GS_COLCLAMP;

	*p_data++ = gs_reg_cache[GS_CACHE_FRAME_2] = GS_SETREG_FRAME_1( gsGlobal->ScreenBuffer[1] / 8192, gsGlobal->Width / 64, gsGlobal->PSM, 0 );
	*p_data++ = GS_FRAME_2;

	*p_data++ = gs_reg_cache[GS_CACHE_XYOFFSET];
	*p_data++ = GS_XYOFFSET_2;

	*p_data++ = gs_reg_cache[GS_CACHE_SCISSOR_2] = gs_reg_cache[GS_CACHE_SCISSOR];
	*p_data++ = GS_SCISSOR_2;

	*p_data++ = gs_reg_cache[GS_CACHE_TEST_2] = gs_reg_cache[GS_CACHE_TEST];
	*p_data++ = GS_TEST_2;

	*p_data++ = gs_reg_cache[GS_CACHE_CLAMP];
	*p_data++ = GS_CLAMP_2;

	*p_data++ = gs_reg_cache[GS_CACHE_ZBUF_2] = gs_reg_cache[GS_CACHE_ZBUF];
	*p_data++ = GS_ZBUF_2;

	*p_data++ = gs_reg_cache[GS_CACHE_ALPHA] = GS_ALPHA_BLEND_NORMAL;
	*p_data++ = GS_ALPHA_1;

	*p_data++ = gs_reg_cache[GS_CACHE_ALPHA_2] = gs_reg_cache[GS_CACHE_ALPHA];
	*p_data++ = GS_ALPHA_2;

	*p_data++ = GS_SETREG_DIMX(gsGlobal->DitherMatrix[0],gsGlobal->DitherMatrix[1],
                gsGlobal->DitherMatrix[2],gsGlobal->DitherMatrix[3],gsGlobal->DitherMatrix[4],
                gsGlobal->DitherMatrix[5],gsGlobal->DitherMatrix[6],gsGlobal->DitherMatrix[7],
                gsGlobal->DitherMatrix[8],gsGlobal->DitherMatrix[9],gsGlobal->DitherMatrix[10],
                gsGlobal->DitherMatrix[11],gsGlobal->DitherMatrix[12],gsGlobal->DitherMatrix[13],
                gsGlobal->DitherMatrix[14],gsGlobal->DitherMatrix[15]); // 4x4 dither matrix

	*p_data++ = GS_DIMX;

	*p_data++ = GS_SETREG_TEXA(0x80, 0, 0x80);
	*p_data++ = GS_TEXA;

	if((gsGlobal->Dithering == GS_SETTING_ON) && ((gsGlobal->PSM == GS_PSM_CT16) || (gsGlobal->PSM == GS_PSM_CT16S))) {
        *p_data++ = 1;
        *p_data++ = GS_DTHE;
	}

	dmaKit_send_ucab(DMA_CHANNEL_GIF, p_store, size);
	dmaKit_wait_fast();
}

GSCONTEXT *temp_init_global()
{
    //int8_t dither_matrix[16] = {-4,2,-3,3,0,-2,1,-1,-3,3,-4,2,1,-1,0,-2};
    int8_t dither_matrix[16] = {4,2,5,3,0,6,1,7,5,3,4,2,1,7,0,6}; //different matrix
    int i = 0;

	GSCONTEXT *gsGlobal = calloc(1,sizeof(GSCONTEXT));
	gsGlobal->BGColor = calloc(1,sizeof(GSBGCOLOR));
	gsGlobal->dma_misc = ((uint32_t)memalign(128, 512)) | 0x30000000;

	/* Generic Values */
	_io_driver driver = posixIODriver;
	if(configGetTvScreenTypeWithIODriver(&driver) == 2) gsGlobal->Aspect = GS_ASPECT_16_9;
    else
    gsGlobal->Aspect = GS_ASPECT_4_3;

    gsGlobal->PSM = GS_PSM_CT24;
    gsGlobal->PSMZ = GS_ZBUF_32;

    gsGlobal->Dithering = GS_SETTING_OFF;
    gsGlobal->DoubleBuffering = GS_SETTING_ON;
    gsGlobal->ZBuffering = GS_SETTING_ON;

    // Setup a mode automatically
    gsGlobal->Mode = check_rom();
    gsGlobal->Interlace = GS_INTERLACED;
	gsGlobal->Field = GS_FIELD;
    gsGlobal->Width = 640;

    if(gsGlobal->Mode == GS_MODE_PAL) gsGlobal->Height = 512;
    else gsGlobal->Height = 448;

	gsGlobal->DrawOrder = GS_PER_OS;

	gsGlobal->EvenOrOdd = 0;

	gsGlobal->OffsetX = (int)(2048.0f * 16.0f);
	gsGlobal->OffsetY = (int)(2048.0f * 16.0f);
	gsGlobal->ActiveBuffer = 1;
	gsGlobal->PrimFogEnable = GS_SETTING_OFF;
	gsGlobal->PrimAAEnable = GS_SETTING_OFF;
	gsGlobal->PrimContext = 0;
	gsGlobal->FirstFrame = GS_SETTING_ON;

	for(i = 0; i < 15; i++) {
	    gsGlobal->DitherMatrix[i] = dither_matrix[i];
	}

	/* BGColor Register Values */
	gsGlobal->BGColor->Red = 0x00;
	gsGlobal->BGColor->Green = 0x00;
	gsGlobal->BGColor->Blue = 0x00;

	return gsGlobal;
}

void set_display_offset(GSCONTEXT *gsGlobal, int x, int y)
{
	gsGlobal->StartXOffset = x;
	gsGlobal->StartYOffset = y;

	if (gsGlobal->Interlace == GS_INTERLACED) {
		// Do not change odd/even start position in interlaced mode
		gsGlobal->StartYOffset &= ~1;
	}

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
}

void setup_buffer_textures() {
   	draw_buffer.Width = gsGlobal->Width;
	draw_buffer.Height = gsGlobal->Height;
	draw_buffer.PSM = gsGlobal->PSM;
    draw_buffer.ClutPSM = GS_PSM_CT32;

	draw_buffer.Mem = NULL;
	draw_buffer.Clut = NULL;
	draw_buffer.Vram = 0;
	draw_buffer.VramClut = 0;
	depth_buffer.PageAligned = true;
	draw_buffer.Filter = GS_FILTER_NEAREST;

	texture_manager_lock_and_bind(gsGlobal, &draw_buffer, false);

   	display_buffer.Width = gsGlobal->Width;
	display_buffer.Height = gsGlobal->Height;
	display_buffer.PSM = gsGlobal->PSM;
    display_buffer.ClutPSM = GS_PSM_CT32;

	display_buffer.Mem = NULL;
	display_buffer.Clut = NULL;
	display_buffer.Vram = 0;
	display_buffer.VramClut = 0;
	depth_buffer.PageAligned = true;
	display_buffer.Filter = GS_FILTER_NEAREST;

	texture_manager_lock_and_bind(gsGlobal, &display_buffer, false);

   	depth_buffer.Width = gsGlobal->Width;
	depth_buffer.Height = gsGlobal->Height;
	depth_buffer.PSM = gsGlobal->PSMZ+0x30;
    depth_buffer.ClutPSM = GS_PSM_CT32;

	depth_buffer.Mem = NULL;
	depth_buffer.Clut = NULL;
	depth_buffer.Vram = 0;
	depth_buffer.VramClut = 0;
	depth_buffer.PageAligned = true;
	depth_buffer.Filter = GS_FILTER_NEAREST;

	texture_manager_lock_and_bind(gsGlobal, &depth_buffer, false);

	gsGlobal->ScreenBuffer[0] = draw_buffer.Vram;
	gsGlobal->ScreenBuffer[1] = display_buffer.Vram;
	gsGlobal->ZBuffer = depth_buffer.Vram;
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
	gsGlobal->Dithering = GS_SETTING_OFF;

	gsGlobal->Interlace = interlace;
	gsGlobal->Field = field;

	dbgprintf("\nGraphics: created video surface of (%d, %d)\n",
		gsGlobal->Width, gsGlobal->Height);

	texture_manager_init(gsGlobal);

	setup_buffer_textures();

	init_screen(gsGlobal);

	cur_screen_buffer[DRAW_BUFFER] = &draw_buffer;
	cur_screen_buffer[DISPLAY_BUFFER] = &display_buffer;
	cur_screen_buffer[DEPTH_BUFFER] = &depth_buffer;

	main_screen_buffer[DRAW_BUFFER] = &draw_buffer;
	main_screen_buffer[DISPLAY_BUFFER] = &display_buffer;
	main_screen_buffer[DEPTH_BUFFER] = &depth_buffer;

	switchFlipScreenFunction();
	
	set_display_offset(gsGlobal, -0.5f, -0.5f);
}
 
void init_graphics()
{
	ee_sema_t sema; 
    sema.init_count = 0;
    sema.max_count = 1;
    sema.option = 0;
    vsync_sema_id = CreateSema(&sema);

	gsGlobal = temp_init_global();

	//gsGlobal->OffsetX = (int)((2048.0f-(gsGlobal->Width/2)) * 16.0f);
	//gsGlobal->OffsetY = (int)((2048.0f-(gsGlobal->Height/2)) * 16.0f);

	gsGlobal->PSM  = GS_PSM_CT24;
	gsGlobal->PSMZ = GS_ZBUF_16S;
	gsGlobal->ZBuffering = GS_SETTING_OFF;
	gsGlobal->DoubleBuffering = GS_SETTING_ON;
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	gsGlobal->Dithering = GS_SETTING_OFF;

	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_VIF0);
	dmaKit_chan_init(DMA_CHANNEL_VIF1);
	dmaKit_wait(DMA_CHANNEL_GIF, 0);
	dmaKit_wait(DMA_CHANNEL_VIF0, 0);
	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	flipScreen = flipScreenDoubleBuffering;

	dbgprintf("\nGraphics: created %ix%i video surface\n",
		gsGlobal->Width, gsGlobal->Height);

	texture_manager_init(gsGlobal);

	setup_buffer_textures();

	init_screen(gsGlobal);

	cur_screen_buffer[DRAW_BUFFER] = &draw_buffer;
	cur_screen_buffer[DISPLAY_BUFFER] = &display_buffer;
	cur_screen_buffer[DEPTH_BUFFER] = &depth_buffer;

	main_screen_buffer[DRAW_BUFFER] = &draw_buffer;
	main_screen_buffer[DISPLAY_BUFFER] = &display_buffer;
	main_screen_buffer[DEPTH_BUFFER] = &depth_buffer;

	DIntr();
	int callback_id = AddIntcHandler(INTC_VBLANK_S, vsync_handler, 0);
	EnableIntc(INTC_VBLANK_S);
	// Unmask VSync interrupt
	GsPutIMR(GsGetIMR() & ~0x0800);
	EIntr();

	owl_init(owl_packet_buffer, OWL_PACKET_BUFFER_SIZE);

	for (int i = 0; i < 2; i++) {
    	clearScreen(BLACK_RGBAQ);	
		flipScreen();
	}
}

void graphicWaitVblankStart(){
	*GS_CSR = *GS_CSR & 8;
	while(!(*GS_CSR & 8));
}
