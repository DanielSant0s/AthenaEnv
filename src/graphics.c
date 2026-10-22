#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>

#include <time.h>

#include <graphics.h>
#include <lz4_ps2.h>
#include <render.h>
#include <athena_math.h>
#include <dbgprintf.h>
#include <fntsys.h>

#include <owl_packet.h>
#include <profiler.h>

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

#define OWL_PACKET_BUFFER_SIZE 8192

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

	athena_texture_ensure_uncompressed(Texture);
	if (!Texture->Mem)
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
	Texture->UncompressedSize = (uint32_t)size;
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

void gs_clut_swizzle_csm1(uint32_t *dst_csm1, const uint32_t *src_linear) {
    if (dst_csm1 == src_linear) {
        for (int base = 0; base < 256; base += 32) {
            for (int k = 0; k < 8; k++) {
                uint32_t tmp = dst_csm1[base + 8 + k];
                dst_csm1[base + 8 + k]  = dst_csm1[base + 16 + k];
                dst_csm1[base + 16 + k] = tmp;
            }
        }
    } else {
        for (int i = 0; i < 256; i++) {
            int rem = i & 0x18;
            int dst_idx = (rem == 0x08 || rem == 0x10) ? (i ^ 0x18) : i;
            dst_csm1[dst_idx] = src_linear[i];
        }
    }
}

static GSSURFACE s_depth_temp_buffer = {0};
static GSSURFACE s_identity_clut = {0};
static uint32_t s_identity_clut_mem[256] __attribute__((aligned(16)));

void gs_upload_clut_inline(GSSURFACE *pal, const uint32_t *csm1_entries) {
    if (!pal || !csm1_entries) return;

    if (pal->Vram == 0) {
        void *saved_mem = pal->Mem;
        pal->Width = 16;
        pal->Height = 16;
        pal->PSM = GS_PSM_CT32;
        pal->TBW = 1;
        pal->Mem = NULL; // Keep Mem NULL during bind so texture_manager_bind only allocates VRAM
        texture_manager_bind(gsGlobal, pal, false);
        texture_manager_lock(pal);
        pal->Mem = saved_mem;
    }

    pal->Vram &= ~TRANSFER_REQUEST_MASK;

    if (pal->Mem && (const void *)pal->Mem != (const void *)csm1_entries) {
        memcpy(pal->Mem, csm1_entries, 256 * sizeof(uint32_t));
    }

    uint32_t cbp = (pal->Vram & ~TRANSFER_REQUEST_MASK) / 256;

    // Inline 74-quadword VIF1 DIRECT transfer (1 KB = 64 qwords of PSMCT32 palette data)
    owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 74);

    owl_add_cnt_tag(packet, 73, 0);
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
    owl_add_uint(packet, VIF_CODE(72, 0, VIF_DIRECT, 0));

    owl_add_tag(packet, GIF_AD, GIF_SET_TAG(4, 0, 0, 0, 0, 1));
    owl_add_tag(packet, GS_BITBLTBUF, GS_SETREG_BITBLTBUF(0, 0, 0, cbp, 1, GS_PSM_CT32));
    owl_add_tag(packet, GS_TRXPOS, GS_SETREG_TRXPOS(0, 0, 0, 0, 0));
    owl_add_tag(packet, GS_TRXREG, GS_SETREG_TRXREG(16, 16));
    owl_add_tag(packet, GS_TRXDIR, GS_SETREG_TRXDIR(0));

    owl_add_tag(packet, 0, GIF_SET_TAG(64, 0, 0, 0, 2, 0));

    const __uint128_t *qsrc = (const __uint128_t *)csm1_entries;
    for (int q = 0; q < 64; q++) {
        owl_add_uquad_ptr(packet, (__uint128_t *)&qsrc[q]);
    }

    owl_add_tag(packet, GIF_AD, GIF_SET_TAG(1, 1, 0, 0, 0, 1));
    owl_add_tag(packet, GS_TEXFLUSH, 1);
}

static uint32_t gs_ensure_depth_temp_buffer(void) {
    if (s_depth_temp_buffer.Vram == 0) {
        memset(&s_depth_temp_buffer, 0, sizeof(s_depth_temp_buffer));
        s_depth_temp_buffer.Width = 64;
        s_depth_temp_buffer.Height = 32;
        s_depth_temp_buffer.PSM = GS_PSM_CT32;
        s_depth_temp_buffer.TBW = 1;
        s_depth_temp_buffer.Mem = NULL;
        s_depth_temp_buffer.PageAligned = true;
        s_depth_temp_buffer.Filter = GS_FILTER_NEAREST;
        texture_manager_lock_and_bind(gsGlobal, &s_depth_temp_buffer, false);
    }
    return (s_depth_temp_buffer.Vram & ~TRANSFER_REQUEST_MASK) / 256;
}

// Channel copy, one 64x32 page at a time -- ported from gtavl_plugins/rw_se/src/secore.c
// (performChannelCopy).
//
// A page is 96 sprites (384 qw of UV/XYZ2), and those sprites are the same for
// every page: the UVs are page-relative, and the only per-page difference was
// the screen position baked into each XYZ2. That position now comes from
// XYOFFSET instead -- the sprites are stored page-local plus CC_ORIGIN, and
// each page sets XYOFFSET = CC_ORIGIN - block, so the GS lands them on the same
// pixels as before. The sprite body is therefore built once (cc_body) and every
// page DMA_REFs it; per page the EE only writes the ~10 qw register header.
// Rebuilding the whole 399 qw packet for all 140 pages of each pass used to
// cost ~7 ms of EE time per frame with depth fog on.
//
// Only the row pattern depends on the channel (green/alpha start on the odd
// pattern), so there are two bodies. The horizontal 8-texel offset of blue and
// alpha is done by CLAMP region repeat, not the UVs.
#define CC_ORIGIN 2048
#define CC_BODY_QWC (1 + 96 * 4) // sprite GIF tag + 96 sprites

static owl_qword cc_body[2][CC_BODY_QWC] __attribute__((aligned(64)));
static bool cc_body_ready = false;

static void cc_build_bodies(void) {
    for (int odd_first = 0; odd_first < 2; odd_first++) {
        owl_packet body = { 0 };
        body.ptr = cc_body[odd_first];

        // PRE = 0: the header writes PRIM through A+D, so the tag (and with it
        // the body) does not depend on alpha blending or the context.
        owl_add_tag(&body, (GS_UV) | (GS_XYZ2 << 4) | (GS_UV << 8) | (GS_XYZ2 << 12),
                    GIF_SET_TAG(96, 1, 0, 0, 0, 4));

        for (int y = 0; y < 32; y += 2) {
            uint64_t y0 = (uint64_t)((y + CC_ORIGIN) << 4) << 32;
            uint64_t y1 = (uint64_t)((y + 2 + CC_ORIGIN) << 4) << 32;
            if (((y % 4) == 0) ^ (odd_first == 1)) { // Even (4 16x2 sprites)
                for (int x = 0; x < 64; x += 16) {
                    owl_add_tag(&body, 0, GS_SETREG_STQ(8 + ((8 + x * 2) << 4), 8 + ((y * 2) << 4)));
                    owl_add_tag(&body, 1, (uint64_t)((x + CC_ORIGIN) << 4) | y0);
                    owl_add_tag(&body, 0, GS_SETREG_STQ(8 + ((24 + x * 2) << 4), 8 + ((2 + y * 2) << 4)));
                    owl_add_tag(&body, 1, (uint64_t)((x + 16 + CC_ORIGIN) << 4) | y1);
                }
            } else { // Odd (Eight 8x2 sprites)
                for (int x = 0; x < 64; x += 8) {
                    owl_add_tag(&body, 0, GS_SETREG_STQ(8 + ((4 + x * 2) << 4), 8 + ((y * 2) << 4)));
                    owl_add_tag(&body, 1, (uint64_t)((x + CC_ORIGIN) << 4) | y0);
                    owl_add_tag(&body, 0, GS_SETREG_STQ(8 + ((12 + x * 2) << 4), 8 + ((2 + y * 2) << 4)));
                    owl_add_tag(&body, 1, (uint64_t)((x + 8 + CC_ORIGIN) << 4) | y1);
                }
            }
        }
    }

    SyncDCache(cc_body, (uint8_t *)cc_body + sizeof(cc_body) - 1);
    cc_body_ready = true;
}

// Emits one page. Leaves CLAMP/FRAME/ALPHA/XYOFFSET/PRIM changed: call
// gs_channel_copy_restore() once after the last page of the pass. flush puts a
// FLUSHA in front of the page -- only the first page of a pass needs one (to
// order it after the 3D work); the pages themselves are all PATH2, in order.
static void gs_perform_channel_copy_page(
    uint32_t channelIn,
    uint32_t channelOutMask,
    uint32_t blockX,
    uint32_t blockY,
    uint32_t source_tbp,
    uint32_t width,
    uint32_t height,
    uint32_t palette_tbp,
    bool abe,
    uint64_t blend_equation,
    uint32_t destFrameBase,
    uint32_t destTBW,
    uint32_t destPSM,
    bool flush)
{
    if (!cc_body_ready)
        cc_build_bodies();

    GSSURFACE tmp_surf;
    tmp_surf.Width = width;
    tmp_surf.Height = height;
    int tw, th;
    athena_set_tw_th(&tmp_surf, &tw, &th);

    int ctxt = gsGlobal->PrimContext;
    int hdr_regs = abe ? 7 : 6;
    int hdr_qwc = 1 + hdr_regs;

    // For the BLUE and ALPHA channels, we need to offset our 'U's by 8 texels
    const uint32_t horz_block_offset = (channelIn == CHANNEL_SHUFFLE_BLUE || channelIn == CHANNEL_SHUFFLE_ALPHA);
    // For the GREEN and ALPHA channels, we need to offset our 'T's by 2 texels
    const uint32_t vert_block_offset = (channelIn == CHANNEL_SHUFFLE_GREEN || channelIn == CHANNEL_SHUFFLE_ALPHA);

    const uint32_t clamp_horz = horz_block_offset ? 8 : 0;
    const uint32_t clamp_vert = vert_block_offset ? 2 : 0;
    const uint32_t frame_mask = ~channelOutMask;

    owl_packet *packet = owl_query_packet(CHANNEL_VIF1, hdr_qwc + 2);

    owl_add_cnt_tag(packet, hdr_qwc,
                    owl_vif_code_double(VIF_CODE(hdr_qwc, 0, VIF_DIRECT, 0),
                                        VIF_CODE(0, 0, flush ? VIF_FLUSHA : VIF_NOP, 0)));

    owl_add_tag(packet, GIF_AD, GIF_SET_TAG(hdr_regs, 1, 0, 0, 0, 1));
    owl_add_tag(packet, GS_XYOFFSET_1 + ctxt, GS_SETREG_XYOFFSET_1((CC_ORIGIN - blockX) << 4, (CC_ORIGIN - blockY) << 4));
    owl_add_tag(packet, GS_TEX0_1 + ctxt, GS_SETREG_TEX0(source_tbp, 1, GS_PSM_T8, tw, th, 1, COLOR_DECAL, palette_tbp, GS_PSM_CT32, 0, 0, 1));
    owl_add_tag(packet, GS_CLAMP_1 + ctxt, GS_SETREG_CLAMP(3, 3, 0xF7, clamp_horz, 0xFD, clamp_vert));
    owl_add_tag(packet, GS_TEXFLUSH, 1);
    owl_add_tag(packet, GS_FRAME_1 + ctxt, GS_SETREG_FRAME(destFrameBase, destTBW, destPSM, frame_mask));
    if (abe) {
        owl_add_tag(packet, GS_ALPHA_1 + ctxt, blend_equation);
    }
    owl_add_tag(packet, GS_PRIM, GS_SETREG_PRIM(GS_PRIM_SPRITE, 0, 1, 0, abe ? 1 : 0, 0, 1, ctxt, 0));

    owl_add_tag(packet,
                owl_vif_code_double(VIF_CODE(CC_BODY_QWC, 0, VIF_DIRECT, 0), VIF_CODE(0, 0, VIF_NOP, 0)),
                DMA_TAG(CC_BODY_QWC, 0, DMA_REF, 0, cc_body[vert_block_offset], 0));
}

// Puts back what the pages of a channel-copy pass overwrote.
static void gs_channel_copy_restore(bool abe) {
    int ctxt = gsGlobal->PrimContext;
    int tail_regs = abe ? 4 : 3;
    int gif_qwc = 1 + tail_regs;

    owl_packet *packet = owl_query_packet(CHANNEL_VIF1, gif_qwc + 1);
    owl_add_cnt_tag(packet, gif_qwc,
                    owl_vif_code_double(VIF_CODE(gif_qwc, 0, VIF_DIRECT, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

    owl_add_tag(packet, GIF_AD, GIF_SET_TAG(tail_regs, 1, 0, 0, 0, 1));
    owl_add_tag(packet, GS_CLAMP_1 + ctxt, gs_reg_cache[GS_CACHE_CLAMP + ctxt]);
    owl_add_tag(packet, GS_FRAME_1 + ctxt, gs_reg_cache[GS_CACHE_FRAME + ctxt]);
    if (abe) {
        owl_add_tag(packet, GS_ALPHA_1 + ctxt, gs_reg_cache[GS_CACHE_ALPHA + ctxt]);
    }
    owl_add_tag(packet, GS_XYOFFSET_1 + ctxt, gs_reg_cache[GS_CACHE_XYOFFSET + ctxt]);
}

void gs_channel_shuffle_ex(GSSURFACE *dst, GSSURFACE *source, GSSURFACE *pal, const gs_channel_shuffle_config *cfg) {
    if (!pal) return;

    draw_buffer.Vram = gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1];
    draw_buffer.Width = gsGlobal->Width;
    draw_buffer.Height = gsGlobal->Height;
    draw_buffer.PSM = gsGlobal->PSM;
    draw_buffer.TBW = gsGlobal->Width / 64;

    depth_buffer.Vram = gsGlobal->ZBuffer;
    depth_buffer.Width = gsGlobal->Width;
    depth_buffer.Height = gsGlobal->Height;
    depth_buffer.PSM = gsGlobal->PSMZ + 0x30;
    depth_buffer.TBW = gsGlobal->Width / 64;

    if (!source) source = &depth_buffer;
    if (!dst)    dst    = &draw_buffer;

    if (pal->Vram == 0 || (pal->Vram & TRANSFER_REQUEST_MASK)) {
        if (pal->Mem) {
            gs_upload_clut_inline(pal, pal->Mem);
        } else {
            return;
        }
    }

    const uint32_t in = cfg ? cfg->in_channel : CHANNEL_SHUFFLE_BLUE;
    const uint32_t out = (cfg && cfg->out_mask != 0) ? cfg->out_mask : CHANNEL_SHUFFLE_ALL;

    int ctxt = gsGlobal->PrimContext;
    bool abe = cfg ? cfg->alpha_blend : false;
    uint64_t alpha_eq = (cfg && cfg->blend_equation) ? cfg->blend_equation : GS_ALPHA_BLEND_NORMAL;
    bool ate = cfg ? cfg->alpha_test : false;
    uint8_t ztst = (cfg && cfg->z_test_enable) ? cfg->z_test_method : DEPTH_ALWAYS;

    uint32_t src_tbp  = (source->Vram & ~TRANSFER_REQUEST_MASK) / 256;
    uint32_t src_tbw  = source->TBW ? source->TBW : ((source->Width + 63) / 64);
    uint32_t pal_addr = (pal->Vram & ~TRANSFER_REQUEST_MASK) / 256;
    uint32_t dst_fbp  = (dst->Vram & ~TRANSFER_REQUEST_MASK) / 8192;
    uint32_t dst_tbw  = dst->TBW ? dst->TBW : ((dst->Width + 63) / 64);

    // Check if source is Morton-ordered (PSMZ32 / PSMZ24 have inverted block layout block ^ 0x18)
    // and therefore needs per-page PSMZ -> linear PSM_32 BITBLT through s_depth_temp_buffer
    // before sampling as PSMT8 (exactly as in gtavl_plugins/src/postfx.c CSkyEdgePostEffects_CopyBufferBlendChannels).
    bool is_morton_ordered = (source == &depth_buffer) || ((source->PSM & 0x30) == 0x30);
    uint32_t buf_addr = 0;
    if (is_morton_ordered) {
        buf_addr = gs_ensure_depth_temp_buffer();
    }

    // Set TEST_1 / ZBUF_1 / TEX1_1 for the pass
    owl_packet *setup_pkt = owl_query_packet(CHANNEL_VIF1, 6);
    owl_add_cnt_tag(setup_pkt, 5, 0);
    owl_add_uint(setup_pkt, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(setup_pkt, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(setup_pkt, VIF_CODE(0, 0, VIF_FLUSHA, 0));
    owl_add_uint(setup_pkt, VIF_CODE(4, 0, VIF_DIRECT, 0));
    owl_add_tag(setup_pkt, GIF_AD, GIF_SET_TAG(3, 1, 0, 0, 0, 1));
    owl_add_tag(setup_pkt, GS_TEX1_1 + ctxt, GS_SETREG_TEX1(0, 0, GS_FILTER_NEAREST, GS_FILTER_NEAREST, 0, 0, 0));
    owl_add_tag(setup_pkt, GS_ZBUF_1 + ctxt, GS_SETREG_ZBUF_1(gsGlobal->ZBuffer / 8192, gsGlobal->PSMZ, 1));
    owl_add_tag(setup_pkt, GS_TEST_1 + ctxt, GS_SETREG_TEST(ate ? 1 : 0, ALPHA_GREATER, 0, ALPHA_FAIL_NO_UPDATE, 0, 0, 1, ztst));

    uint32_t startX = cfg ? (cfg->x & ~63u) : 0;
    uint32_t startY = cfg ? (cfg->y & ~31u) : 0;
    uint32_t endX   = (cfg && cfg->width)  ? (startX + cfg->width)  : dst->Width;
    uint32_t endY   = (cfg && cfg->height) ? (startY + cfg->height) : dst->Height;

    for (uint32_t y = startY; y < endY; y += 32) {
        uint32_t page_row = y >> 5;
        for (uint32_t x = startX; x < endX; x += 64) {
            uint32_t page_col = x >> 6;
            uint32_t page = (page_row * src_tbw + page_col) << 5;
            uint32_t copy_source;

            if (is_morton_ordered) {
                owl_packet *blit_pkt = owl_query_packet(CHANNEL_VIF1, 7);
                owl_add_cnt_tag(blit_pkt, 6, 0);
                owl_add_uint(blit_pkt, VIF_CODE(0, 0, VIF_NOP, 0));
                owl_add_uint(blit_pkt, VIF_CODE(0, 0, VIF_NOP, 0));
                owl_add_uint(blit_pkt, VIF_CODE(0, 0, VIF_FLUSHA, 0));
                owl_add_uint(blit_pkt, VIF_CODE(5, 0, VIF_DIRECT, 0));

                owl_add_tag(blit_pkt, GIF_AD, GIF_SET_TAG(4, 1, 0, 0, 0, 1));
                owl_add_tag(blit_pkt, GS_BITBLTBUF, GS_SETREG_BITBLTBUF(src_tbp + page, 1, source->PSM, buf_addr, 1, GS_PSM_CT32));
                owl_add_tag(blit_pkt, GS_TRXPOS, GS_SETREG_TRXPOS(0, 0, 0, 0, 0));
                owl_add_tag(blit_pkt, GS_TRXREG, GS_SETREG_TRXREG(64, 32));
                owl_add_tag(blit_pkt, GS_TRXDIR, 2);

                copy_source = buf_addr;
            } else {
                copy_source = src_tbp + page;
            }

            gs_perform_channel_copy_page(
                in, out, x, y, copy_source,
                source->Width, source->Height, pal_addr,
                abe, alpha_eq, dst_fbp, dst_tbw, dst->PSM, false);
        }
    }

    gs_channel_copy_restore(abe);

    // Restore TEST_1 and ZBUF_1 from cache
    owl_packet *restore_pkt = owl_query_packet(CHANNEL_VIF1, 5);
    owl_add_cnt_tag(restore_pkt, 4, 0);
    owl_add_uint(restore_pkt, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(restore_pkt, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(restore_pkt, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(restore_pkt, VIF_CODE(3, 0, VIF_DIRECT, 0));
    owl_add_tag(restore_pkt, GIF_AD, GIF_SET_TAG(2, 1, 0, 0, 0, 1));
    owl_add_tag(restore_pkt, GS_ZBUF_1 + ctxt, gs_reg_cache[GS_CACHE_ZBUF + ctxt]);
    owl_add_tag(restore_pkt, GS_TEST_1 + ctxt, gs_reg_cache[GS_CACHE_TEST + ctxt]);
}

void gs_channel_shuffle(GSSURFACE *dst, uint32_t in_channel, uint32_t out_mask, GSSURFACE *source, GSSURFACE *pal, bool alpha_blend, uint64_t blend_equation) {
    gs_channel_shuffle_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.in_channel     = in_channel;
    cfg.out_mask       = out_mask ? out_mask : CHANNEL_SHUFFLE_ALL;
    cfg.alpha_blend    = alpha_blend;
    cfg.blend_equation = blend_equation ? blend_equation : GS_ALPHA_BLEND_NORMAL;
    cfg.alpha_test     = alpha_blend;
    cfg.z_test_enable  = false;
    cfg.z_test_method  = DEPTH_ALWAYS;
    gs_channel_shuffle_ex(dst, source, pal, &cfg);
}

void gs_channel_shuffle_slow(GSSURFACE *dst, uint32_t in, uint32_t out, uint32_t blockX, uint32_t blockY, GSSURFACE *source, uint32_t width, uint32_t height, GSSURFACE *pal) {
    gs_channel_shuffle_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.in_channel = in;
    cfg.out_mask   = out ? out : CHANNEL_SHUFFLE_ALL;
    cfg.x          = blockX;
    cfg.y          = blockY;
    cfg.width      = width;
    cfg.height     = height;
    gs_channel_shuffle_ex(dst, source, pal, &cfg);
}

static uint32_t gs_select_depth_msb_channel(float *out_byte_scale, float *out_screen_scale_z) {
    uint64_t max_z = get_max_z(gsGlobal);
    float screen_scale_z = (float)max_z * 0.5f;
    if (out_screen_scale_z) *out_screen_scale_z = screen_scale_z;

    switch (gsGlobal->PSMZ) {
        case GS_ZBUF_32:
            if (out_byte_scale) *out_byte_scale = 16777216.0f; // 2^24
            return CHANNEL_SHUFFLE_ALPHA; // Z[31:24]
        case GS_ZBUF_24:
            if (out_byte_scale) *out_byte_scale = 65536.0f;    // 2^16
            return CHANNEL_SHUFFLE_BLUE;  // Z[23:16]
        case GS_ZBUF_16:
        case GS_ZBUF_16S:
        default:
            if (out_byte_scale) *out_byte_scale = 256.0f;      // 2^8
            return CHANNEL_SHUFFLE_GREEN; // Z[15:8]
    }
}

void gs_build_depth_fog_lut(uint32_t *dst_csm1, const gs_depth_fog_config *cfg) {
    if (!dst_csm1 || !cfg) return;

    float near_z = (cfg->near_z > 0.001f) ? cfg->near_z : render_get_near();
    float far_z  = (cfg->far_z  > near_z) ? cfg->far_z  : render_get_far();
    float start  = (cfg->start  > 0.0f)   ? cfg->start  : (near_z * 8.0f);
    float end    = (cfg->end    > start)  ? cfg->end    : (far_z * 0.5f);
    float density = (cfg->density > 0.0f) ? cfg->density : 1.5f;
    float sky_fog = cfg->sky_fog;
    if (sky_fog < 0.0f) sky_fog = 0.0f;
    if (sky_fog > 1.0f) sky_fog = 1.0f;

    uint32_t rgb = (uint32_t)(cfg->color & 0x00FFFFFFu);
    uint32_t max_alpha = (uint32_t)((cfg->color >> 24) & 0xFFu);
    if (max_alpha == 0) max_alpha = 0x80u;

    float byte_scale = 65536.0f;
    float screen_scale_z = 4194304.0f;
    gs_select_depth_msb_channel(&byte_scale, &screen_scale_z);

    int shift = cfg->shift;
    if (shift < 0) shift = 0;
    if (shift > 7) shift = 7;
    if (shift > 0)
        byte_scale /= (float)(1 << shift);

    uint32_t linear_lut[256];

    // Entry 0 corresponds to Z MSB == 0 (unwritten Z-buffer = skybox / beyond far horizon)
    uint32_t sky_a = (uint32_t)(sky_fog * (float)max_alpha + 0.5f);
    if (sky_a > 0x80u) sky_a = 0x80u;
    linear_lut[0] = rgb | (sky_a << 24);

    float inv_range = 1.0f / (end - start);

    for (int b = 1; b < 256; b++) {
        // Reconstruct representative hardware Z from MSB byte b
        float z_gs = ((float)b + 0.5f) * byte_scale;
        if (z_gs > screen_scale_z * 2.0f) {
            z_gs = screen_scale_z * 2.0f;
        }

        // Convert hardware Z back to reversed-Z NDC in [-1, +1] (+1 at near, -1 at far)
        float z_ndc = (z_gs / screen_scale_z) - 1.0f;
        float denom = (far_z + near_z) + z_ndc * (far_z - near_z);
        float d = (denom > 0.0001f) ? ((2.0f * far_z * near_z) / denom) : far_z;
        if (d < near_z) d = near_z;
        if (d > far_z)  d = far_z;

        float factor = 0.0f;
        if (d > start) {
            float t = (d - start) * inv_range;
            if (t < 0.0f) t = 0.0f;

            switch (cfg->curve) {
                case FOG_CURVE_EXP:
                    factor = 1.0f - expf(-density * t);
                    break;
                case FOG_CURVE_EXP2: {
                    float dt = density * t;
                    factor = 1.0f - expf(-(dt * dt));
                    break;
                }
                case FOG_CURVE_LINEAR:
                default:
                    factor = (t > 1.0f) ? 1.0f : t;
                    break;
            }
        }

        if (factor < 0.0f) factor = 0.0f;
        if (factor > 1.0f) factor = 1.0f;

        uint32_t a = (uint32_t)(factor * (float)max_alpha + 0.5f);
        if (a > 0x80u) a = 0x80u;

        linear_lut[b] = rgb | (a << 24);
    }

    gs_clut_swizzle_csm1(dst_csm1, linear_lut);
}

static GSSURFACE s_depth_fog_lut = {0};
static uint32_t s_depth_fog_clut_mem[256] __attribute__((aligned(16)));
static gs_depth_fog_config s_last_fog_cfg = {0};
static int s_last_fog_psmz = -1;
static bool s_depth_fog_dirty = true;

static void gs_bind_depth_surface(void) {
    depth_buffer.Vram = gsGlobal->ZBuffer;
    depth_buffer.Width = gsGlobal->Width;
    depth_buffer.Height = gsGlobal->Height;
    depth_buffer.PSM = gsGlobal->PSMZ + 0x30;
    depth_buffer.TBW = gsGlobal->Width / 64;
}

static GSSURFACE *gs_identity_clut(void) {
    if (s_identity_clut.Vram == 0 || (s_identity_clut.Vram & TRANSFER_REQUEST_MASK)) {
        uint32_t linear[256];
        for (int i = 0; i < 256; i++)
            linear[i] = 0x01010101u * (uint32_t)i;

        gs_clut_swizzle_csm1(s_identity_clut_mem, linear);
        memset(&s_identity_clut, 0, sizeof(s_identity_clut));
        s_identity_clut.Width = 16;
        s_identity_clut.Height = 16;
        s_identity_clut.PSM = GS_PSM_CT32;
        s_identity_clut.TBW = 1;
        s_identity_clut.Mem = s_identity_clut_mem;
        s_identity_clut.Filter = GS_FILTER_NEAREST;
        gs_upload_clut_inline(&s_identity_clut, s_identity_clut_mem);
    }
    return &s_identity_clut;
}

// Z-to-Z channel copy. Morton unswizzle stays off: the PSMT8 read and the PSMCT32
// write share CT32 block order, so the PSMZ permutation cancels and the byte lands
// on the same pixel. Matches postfx.c calls that pass is_morton_ordered = 0.
static void gs_depth_channel_copy(uint32_t in_channel, uint32_t out_mask, bool abe, uint64_t blend) {
    gs_bind_depth_surface();
    GSSURFACE *pal = gs_identity_clut();
    if (!pal || pal->Vram == 0)
        return;

    uint32_t src_tbp = (depth_buffer.Vram & ~TRANSFER_REQUEST_MASK) / 256;
    uint32_t src_tbw = depth_buffer.TBW ? depth_buffer.TBW : 1;
    uint32_t pal_addr = (pal->Vram & ~TRANSFER_REQUEST_MASK) / 256;
    uint32_t dst_fbp = (depth_buffer.Vram & ~TRANSFER_REQUEST_MASK) / 8192;
    uint32_t width = depth_buffer.Width;
    uint32_t height = depth_buffer.Height;

    bool first = true;
    for (uint32_t y = 0; y < height; y += 32) {
        uint32_t page_row = y >> 5;
        for (uint32_t x = 0; x < width; x += 64) {
            uint32_t page_col = x >> 6;
            uint32_t page = (page_row * src_tbw + page_col) << 5;
            gs_perform_channel_copy_page(
                in_channel, out_mask, x, y, src_tbp + page,
                width, height, pal_addr,
                abe, blend, dst_fbp, src_tbw, GS_PSM_CT32, first);
            first = false;
        }
    }

    gs_channel_copy_restore(abe);
}

static void gs_depth_pass_begin(void) {
    int ctxt = gsGlobal->PrimContext;
    uint32_t zbp = (gsGlobal->ZBuffer & ~TRANSFER_REQUEST_MASK) / 8192;
    int gif_qwc = 4;

    owl_packet *packet = owl_query_packet(CHANNEL_VIF1, gif_qwc + 2);
    owl_add_cnt_tag(packet, gif_qwc + 1, 0);
    owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
    owl_add_uint(packet, VIF_CODE(gif_qwc, 0, VIF_DIRECT, 0));

    owl_add_tag(packet, GIF_AD, GIF_SET_TAG(3, 1, 0, 0, 0, 1));
    owl_add_tag(packet, GS_TEST_1 + ctxt, GS_SETREG_TEST(0, 0, 0, 0, 0, 0, 1, DEPTH_ALWAYS));
    owl_add_tag(packet, GS_ZBUF_1 + ctxt, GS_SETREG_ZBUF_1(zbp, gsGlobal->PSMZ, 1));
    owl_add_tag(packet, GS_XYOFFSET_1 + ctxt, GS_SETREG_XYOFFSET_1(0, 0));
}

static void gs_depth_pass_end(void) {
    int ctxt = gsGlobal->PrimContext;
    int gif_qwc = 11;

    owl_packet *packet = owl_query_packet(CHANNEL_VIF1, gif_qwc + 2);
    owl_add_cnt_tag(packet, gif_qwc + 1, 0);
    owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(packet, VIF_CODE(gif_qwc, 0, VIF_DIRECT, 0));

    owl_add_tag(packet, GIF_AD, GIF_SET_TAG(10, 1, 0, 0, 0, 1));
    owl_add_tag(packet, GS_TEST_1 + ctxt, gs_reg_cache[GS_CACHE_TEST + ctxt]);
    owl_add_tag(packet, GS_ZBUF_1 + ctxt, gs_reg_cache[GS_CACHE_ZBUF + ctxt]);
    owl_add_tag(packet, GS_XYOFFSET_1 + ctxt, gs_reg_cache[GS_CACHE_XYOFFSET + ctxt]);
    owl_add_tag(packet, GS_FRAME_1 + ctxt, gs_reg_cache[GS_CACHE_FRAME + ctxt]);
    owl_add_tag(packet, GS_ALPHA_1 + ctxt, gs_reg_cache[GS_CACHE_ALPHA + ctxt]);
    owl_add_tag(packet, GS_CLAMP_1 + ctxt, gs_reg_cache[GS_CACHE_CLAMP + ctxt]);
    owl_add_tag(packet, GS_SCISSOR_1 + ctxt, gs_reg_cache[GS_CACHE_SCISSOR + ctxt]);
    owl_add_tag(packet, GS_COLCLAMP, gs_reg_cache[GS_CACHE_COLCLAMP]);
    owl_add_tag(packet, GS_PABE, gs_reg_cache[GS_CACHE_PABE]);
    owl_add_tag(packet, GS_TEXA, GS_SETREG_TEXA(0x80, 0, 0x80));
}

// Full-screen passes drawn as one sprite per 64x32 page instead of a single
// screen-sized sprite. A big sprite walks the whole width of the screen per
// scanline, so with a 1:1 texture (the Z bitshift reads the very buffer it
// writes) every few pixels cross into another page on both the read and the
// write side. Page-sized blocks keep both within one page at a time.
//
// The block list only depends on the resolution, so it is built once (and
// again when the resolution changes) and every pass DMA_REFs it: per pass the
// EE writes just the register header. PRE = 0: PRIM comes from the header.
// UV is included for the textured passes; an untextured pass ignores it.
static owl_qword *fs_blocks = NULL;
static int fs_blocks_w = 0, fs_blocks_h = 0;
static uint32_t fs_blocks_qwc = 0;

static void fs_blocks_ensure(int width, int height) {
    if (fs_blocks && fs_blocks_w == width && fs_blocks_h == height)
        return;

    if (fs_blocks) {
        // An earlier pass may still be reading the old list.
        owl_wait_inflight();
        free(fs_blocks);
    }

    uint32_t pages = (uint32_t)(((width + 63) / 64) * ((height + 31) / 32));
    fs_blocks_qwc = 1 + pages * 4;
    fs_blocks = (owl_qword *)memalign(64, fs_blocks_qwc * sizeof(owl_qword));
    fs_blocks_w = width;
    fs_blocks_h = height;

    owl_packet list = { 0 };
    list.ptr = fs_blocks;
    owl_add_tag(&list, (GS_UV) | (GS_XYZ2 << 4) | (GS_UV << 8) | (GS_XYZ2 << 12),
                GIF_SET_TAG(pages, 1, 0, 0, 0, 4));

    for (int y = 0; y < height; y += 32) {
        int y1 = (y + 32 < height) ? y + 32 : height;
        for (int x = 0; x < width; x += 64) {
            int x1 = (x + 64 < width) ? x + 64 : width;
            // PACKED UV puts V at bit 32 (GS_SETREG_STQ layout), not at bit
            // 16 like the A+D register form GS_SETREG_UV builds.
            owl_add_tag(&list, 0, GS_SETREG_STQ(x << 4, y << 4));
            owl_add_tag(&list, 0, (uint64_t)(x << 4) | ((uint64_t)(y << 4) << 32));
            owl_add_tag(&list, 0, GS_SETREG_STQ(x1 << 4, y1 << 4));
            owl_add_tag(&list, 0, (uint64_t)(x1 << 4) | ((uint64_t)(y1 << 4) << 32));
        }
    }

    SyncDCache(fs_blocks, (uint8_t *)fs_blocks + fs_blocks_qwc * sizeof(owl_qword) - 1);
}

// Draws the block list with whatever state the GS currently has.
static void fs_blocks_draw(void) {
    owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 1);
    owl_add_tag(packet,
                owl_vif_code_double(VIF_CODE(fs_blocks_qwc, 0, VIF_DIRECT, 0), VIF_CODE(0, 0, VIF_NOP, 0)),
                DMA_TAG(fs_blocks_qwc, 0, DMA_REF, 0, fs_blocks, 0));
}

// Z-buffer bitshift from gtavl_plugins postfx.c (CSkyEdgePostEffects_ZBufferBitShift).
// 24-bit Z lives in R[7:0] G[15:8] B[23:16]. Each pass doubles blue (B += B) and then
// stitches the saved green bits into the bottom of blue, so the byte a later CLUT
// samples spans the depth band instead of a handful of MSB steps.
void gs_depth_bitshift(int shift_bits) {
    if (!gsGlobal || !gsGlobal->ZBuffering || !gsGlobal->ZBuffer)
        return;
    if (gsGlobal->PSMZ != GS_ZBUF_24 && gsGlobal->PSMZ != GS_ZBUF_32)
        return;
    if (shift_bits < 1)
        return;
    if (shift_bits > 7)
        shift_bits = 7;

    gs_bind_depth_surface();

    int ctxt = gsGlobal->PrimContext;
    uint32_t zbp = (depth_buffer.Vram & ~TRANSFER_REQUEST_MASK) / 8192;
    uint32_t z_tbp = (depth_buffer.Vram & ~TRANSFER_REQUEST_MASK) / 256;
    uint32_t tbw = depth_buffer.TBW ? depth_buffer.TBW : 1;
    int width = (int)depth_buffer.Width;
    int height = (int)depth_buffer.Height;
    int stitch = 128 >> shift_bits;
    if (stitch < 1)
        stitch = 1;

    gs_depth_pass_begin();

    gs_depth_channel_copy(CHANNEL_SHUFFLE_GREEN, CHANNEL_SHUFFLE_ALPHA, false, 0);

    uint64_t add_blend = ALPHA_EQUATION(SRC_RGB, ZERO_RGB, ALPHA_FIX, DST_RGB, 0x80);
    uint64_t prim = GS_SETREG_PRIM(GS_PRIM_SPRITE, 0, 1, 0, 1, 0, 1, ctxt, 0);

    fs_blocks_ensure(width, height);

    // Pass-invariant state once...
    int setup_regs = 5;
    owl_packet *packet = owl_query_packet(CHANNEL_VIF1, setup_regs + 3);
    owl_add_cnt_tag(packet, setup_regs + 2, 0);
    owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
    owl_add_uint(packet, VIF_CODE(setup_regs + 1, 0, VIF_DIRECT, 0));

    owl_add_tag(packet, GIF_AD, GIF_SET_TAG(setup_regs, 1, 0, 0, 0, 1));
    owl_add_tag(packet, GS_XYOFFSET_1 + ctxt, GS_SETREG_XYOFFSET_1(0, 0));
    owl_add_tag(packet, GS_SCISSOR_1 + ctxt, GS_SETREG_SCISSOR_1(0, width - 1, 0, height - 1));
    owl_add_tag(packet, GS_CLAMP_1 + ctxt, GS_SETREG_CLAMP(2, 2, 0, width - 1, 0, height - 1));
    owl_add_tag(packet, GS_COLCLAMP, GS_SETREG_COLCLAMP(255));
    owl_add_tag(packet, GS_PABE, 0);

    // ...then per pass the texture/blend state and the page block list.
    for (int i = 0; i < shift_bits; i++) {
        int pass_regs = 8;
        packet = owl_query_packet(CHANNEL_VIF1, pass_regs + 2);
        owl_add_cnt_tag(packet, pass_regs + 1,
                        owl_vif_code_double(VIF_CODE(pass_regs + 1, 0, VIF_DIRECT, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

        owl_add_tag(packet, GIF_AD, GIF_SET_TAG(pass_regs, 1, 0, 0, 0, 1));
        owl_add_tag(packet, GS_FRAME_1 + ctxt, GS_SETREG_FRAME(zbp, tbw, GS_PSMZ_32, 0xff00ffff));
        owl_add_tag(packet, GS_TEXFLUSH, 1);
        owl_add_tag(packet, GS_TEX0_1 + ctxt, GS_SETREG_TEX0(z_tbp, tbw, GS_PSMZ_32, 10, 10, 0, COLOR_MODULATE, 0, 0, 0, 0, 0));
        owl_add_tag(packet, GS_TEX1_1 + ctxt, GS_SETREG_TEX1(0, 0, GS_FILTER_NEAREST, GS_FILTER_NEAREST, 0, 0, 0));
        owl_add_tag(packet, GS_TEXA, GS_SETREG_TEXA(0, 0, 128));
        owl_add_tag(packet, GS_ALPHA_1 + ctxt, add_blend);
        owl_add_tag(packet, GS_PRIM, prim);
        owl_add_tag(packet, GS_RGBAQ, GS_SETREG_RGBAQ(128, 128, 128, 128, 0));

        fs_blocks_draw();
    }

    gs_depth_channel_copy(
        CHANNEL_SHUFFLE_ALPHA, CHANNEL_SHUFFLE_BLUE, true,
        ALPHA_EQUATION(SRC_RGB, ZERO_RGB, ALPHA_FIX, DST_RGB, stitch));

    gs_depth_pass_end();
}

// Invert Z blue only (frame mask 0xff00ffff). Paired with the bitshift when a
// following pass wants far = 255 and near = 0.
void gs_depth_invert_blue(void) {
    if (!gsGlobal || !gsGlobal->ZBuffering || !gsGlobal->ZBuffer)
        return;
    if (gsGlobal->PSMZ != GS_ZBUF_24 && gsGlobal->PSMZ != GS_ZBUF_32)
        return;

    gs_bind_depth_surface();

    int ctxt = gsGlobal->PrimContext;
    uint32_t zbp = (depth_buffer.Vram & ~TRANSFER_REQUEST_MASK) / 8192;
    uint32_t tbw = depth_buffer.TBW ? depth_buffer.TBW : 1;
    int width = (int)depth_buffer.Width;
    int height = (int)depth_buffer.Height;
    gs_depth_pass_begin();

    fs_blocks_ensure(width, height);

    int nregs = 6;
    owl_packet *packet = owl_query_packet(CHANNEL_VIF1, nregs + 3);
    owl_add_cnt_tag(packet, nregs + 2, 0);
    owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
    owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
    owl_add_uint(packet, VIF_CODE(nregs + 1, 0, VIF_DIRECT, 0));

    owl_add_tag(packet, GIF_AD, GIF_SET_TAG(nregs, 1, 0, 0, 0, 1));
    owl_add_tag(packet, GS_XYOFFSET_1 + ctxt, GS_SETREG_XYOFFSET_1(0, 0));
    owl_add_tag(packet, GS_SCISSOR_1 + ctxt, GS_SETREG_SCISSOR_1(0, width - 1, 0, height - 1));
    owl_add_tag(packet, GS_FRAME_1 + ctxt, GS_SETREG_FRAME(zbp, tbw, GS_PSMZ_24, 0xff00ffff));
    owl_add_tag(packet, GS_ALPHA_1 + ctxt, ALPHA_EQUATION(SRC_RGB, DST_RGB, ALPHA_FIX, ZERO_RGB, 128));
    owl_add_tag(packet, GS_PRIM, GS_SETREG_PRIM(GS_PRIM_SPRITE, 0, 0, 0, 1, 0, 0, ctxt, 0));
    owl_add_tag(packet, GS_RGBAQ, GS_SETREG_RGBAQ(255, 255, 255, 128, 0));

    fs_blocks_draw();

    gs_depth_pass_end();
}

void gs_depth_fog(const gs_depth_fog_config *cfg) {
    if (!cfg || !gsGlobal || !gsGlobal->ZBuffering) return;

    gs_depth_fog_config resolved = *cfg;
    if (resolved.near_z <= 0.001f) resolved.near_z = render_get_near();
    if (resolved.far_z  <= resolved.near_z) resolved.far_z = render_get_far();

    bool changed = s_depth_fog_dirty ||
                   (s_depth_fog_lut.Vram == 0) ||
                   (s_last_fog_psmz != gsGlobal->PSMZ) ||
                   (resolved.color   != s_last_fog_cfg.color) ||
                   (resolved.start   != s_last_fog_cfg.start) ||
                   (resolved.end     != s_last_fog_cfg.end) ||
                   (resolved.near_z  != s_last_fog_cfg.near_z) ||
                   (resolved.far_z   != s_last_fog_cfg.far_z) ||
                   (resolved.density != s_last_fog_cfg.density) ||
                   (resolved.sky_fog != s_last_fog_cfg.sky_fog) ||
                   (resolved.curve   != s_last_fog_cfg.curve) ||
                   (resolved.shift   != s_last_fog_cfg.shift);

    if (changed) {
        s_depth_fog_lut.Width   = 16;
        s_depth_fog_lut.Height  = 16;
        s_depth_fog_lut.PSM     = GS_PSM_CT32;
        s_depth_fog_lut.TBW     = 1;
        s_depth_fog_lut.Mem     = s_depth_fog_clut_mem;
        s_depth_fog_lut.Filter  = GS_FILTER_NEAREST;

        gs_build_depth_fog_lut(s_depth_fog_clut_mem, &resolved);
        gs_upload_clut_inline(&s_depth_fog_lut, s_depth_fog_clut_mem);

        s_last_fog_cfg   = resolved;
        s_last_fog_psmz  = gsGlobal->PSMZ;
        s_depth_fog_dirty = false;
    }

    if (resolved.shift > 0)
        gs_depth_bitshift(resolved.shift);

    uint32_t msb_channel = gs_select_depth_msb_channel(NULL, NULL);

    gs_channel_shuffle_config shuf;
    memset(&shuf, 0, sizeof(shuf));
    shuf.in_channel     = msb_channel;
    shuf.out_mask       = CHANNEL_SHUFFLE_RGB;
    shuf.alpha_blend    = true;
    shuf.blend_equation = GS_ALPHA_BLEND_NORMAL;
    shuf.alpha_test     = true;
    shuf.z_test_enable  = false;
    shuf.z_test_method  = DEPTH_ALWAYS;

    gs_channel_shuffle_ex(&draw_buffer, &depth_buffer, &s_depth_fog_lut, &shuf);
}

void set_screen_buffer(eScreenBuffers id, GSSURFACE *buf, uint32_t mask) {
	if (buf == &draw_buffer) {
		draw_buffer.Vram = gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1];
	} else if (buf == &display_buffer) {
		display_buffer.Vram = gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer];
	} else if (buf == &depth_buffer) {
		depth_buffer.Vram = gsGlobal->ZBuffer;
	}
	buf->Mask = mask;
	switch (id) {
		case DRAW_BUFFER:
			set_register(GS_CACHE_FRAME+gsGlobal->PrimContext, GS_SETREG_FRAME_1( (buf->Vram & ~TRANSFER_REQUEST_MASK) / 8192, buf->TBW, buf->PSM, buf->Mask ));
			break;
		case DISPLAY_BUFFER:
			break;
		case DEPTH_BUFFER:
			set_register(GS_CACHE_ZBUF+gsGlobal->PrimContext, GS_SETREG_ZBUF_1( (buf->Vram & ~TRANSFER_REQUEST_MASK) / 8192, buf->PSM-0x30, buf->Mask ));
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
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
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
	gs_reg_cache[GS_CACHE_FRAME] = GS_SETREG_FRAME_1( draw_buffer.Vram / 8192, gsGlobal->Width / 64, gsGlobal->PSM, draw_buffer.Mask );

	// Context 1
	owl_add_tag(packet, GS_SCISSOR_1, gs_reg_cache[GS_CACHE_SCISSOR]);
	owl_add_tag(packet, GS_FRAME_1, gs_reg_cache[GS_CACHE_FRAME]);
}

/* Copy of sync_screen_flip, but without the 'flip' */
static void sync_screen(GSCONTEXT *gsGlobal)
{
   prof_push(PROF_VSYNC);
   if (!gsGlobal->FirstFrame) WaitSema(vsync_sema_id);
   while (PollSema(vsync_sema_id) >= 0)
   	;
   prof_pop();
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

		/* what fits between the title and the restart hint: a long
		   backtrace (deep recursion) would run over it */
		char shown[1024];
		{
			const char *src = errMsg;
			int len = 0, lines = 0;
			while (*src && len < (int)sizeof(shown) - 8) {
				if (*src == '\n' && ++lines == 15)
					break;
				shown[len++] = *src++;
			}
			if (*src)
				len += sprintf(shown + len, "\n  ...");
			shown[len] = '\0';
		}

		int slot = fntLoadFile(NULL);
		fntSetCharSize(slot, FNTSYS_CHAR_SIZE*64, FNTSYS_CHAR_SIZE*64);

		set_screen_param(DEPTH_TEST_ENABLE, false);

    	while (!isButtonPressed(PAD_START)) {
			clearScreen(color);
			fntRenderString(slot, 15, 15, 0, 625, 448, "AthenaEnv ERROR!", 1.2f, color2);
			fntRenderString(slot, 15, 80, 0, 625, 448, shown, 0.8f, color2);
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

	prof_frame_end();
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

	prof_frame_end();

	processFrameCounter();
}

// Frame overlap: the flip kicks the frame's last chain and returns right away,
// leaving the wait for the GS to the first point that actually needs it -- the
// next chain handed to the DMAC (present_pending_frame, via owl's pre-kick
// hook). Until then the EE runs the next frame's game logic, physics and packet
// building while VIF1/VU1/GS are still drawing this one, so a frame costs
// max(EE, GPU) instead of EE + GPU.
//
// The next frame draws into the buffer that is on screen right now, so none of
// its GS work may start before DISPFB has moved to the buffer just finished.
// Its packets can be built in the meantime; they just are not sent.
//
// What the EE must not do during that window is rewrite memory the in-flight
// chain DMA_REFs. Chains and shadow slots wait on their own generation; bone
// palettes too (render_object); destroyed images, glyph atlases and video
// frames go through owl_wait_inflight(); and JS ArrayBuffers that cannot be
// guarded (tilemap sprites) mark the chain with owl_mark_volatile_refs(), which
// makes the flip drain it before returning.
static bool frame_overlap = true;
static bool present_pending = false;
static bool present_vsync = false;
static uint32_t present_fb = 0;

static void present_pending_frame(void)
{
	if (!present_pending)
		return;

	present_pending = false;

	prof_push(PROF_GPU_WAIT);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);
	dmaKit_wait(DMA_CHANNEL_GIF, 0);

	if (present_vsync)
		sync_screen(gsGlobal);

	if (!gsGlobal->FirstFrame)
		while (!(GS_CSR_FINISH));

	prof_pop();

	GS_SETREG_CSR_FINISH(1);

	gsGlobal->FirstFrame = GS_SETTING_OFF;

	GS_SET_DISPFB2(present_fb / 8192, gsGlobal->Width / 64, gsGlobal->PSM, 0, 0);
}

void graphics_finish_previous_frame(void)
{
	present_pending_frame();
}

void graphics_sync(void)
{
	owl_flush_packet();
	present_pending_frame();
	owl_wait_inflight();
}

void setFrameOverlap(bool enable)
{
	if (!enable)
		graphics_sync();

	frame_overlap = enable;
}

bool getFrameOverlap(void)
{
	return frame_overlap;
}

static void flip_double_buffered(bool with_vsync)
{
	set_finish();

	// Also presents the previous frame first, if nothing in this one was big
	// enough to flush the ring on its own.
	owl_flush_packet();

	present_fb = gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1];
	present_vsync = with_vsync;
	present_pending = true;

	if (!frame_overlap || owl_last_sent_has_volatile_refs())
		present_pending_frame();

	// Retarget the EE side (draw_buffer, the FRAME register queued by
	// setactive) at the other buffer now. setactive lands in the next chain,
	// which the pre-kick hook holds back until the present above has happened.
	gsGlobal->ActiveBuffer ^= 1;
	setactive(gsGlobal);

	texture_manager_nextFrame(gsGlobal);

	prof_frame_end();
}

static void flipScreenDoubleBuffering()
{
	flip_double_buffered(true);
}

static void flipScreenDoubleBufferingPerf()
{
	flip_double_buffered(true);

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

	prof_frame_end();
}

static void flipScreenSingleBufferingPerfNoVSync()
{
	//gsKit_queue_exec(gsGlobal);
	owl_flush_packet();

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dmaKit_wait(DMA_CHANNEL_GIF, 0);

	texture_manager_nextFrame(gsGlobal);

	prof_frame_end();

	processFrameCounter();
}

static void flipScreenDoubleBufferingNoVSync()
{
	flip_double_buffered(false);
}

static void flipScreenDoubleBufferingPerfNoVSync()
{
	flip_double_buffered(false);

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

	GS_SET_BGCOLOR(0, 0, 0);

    EIntr(); //enable interrupts

	owl_packet *packet = owl_query_packet(CHANNEL_GIF, size+2);

	owl_add_cnt_tag(packet, size+1, 0);

	owl_add_tag(packet, GIF_AD, GIFTAG( size, 1, 0, 0, 0, 1 ));

	owl_add_tag(packet, GS_PRMODECONT, 1);

	gs_reg_cache[GS_CACHE_FRAME] = GS_SETREG_FRAME_1( gsGlobal->ScreenBuffer[0] / 8192, gsGlobal->Width / 64, gsGlobal->PSM, 0 );
	owl_add_tag(packet, GS_FRAME, gs_reg_cache[GS_CACHE_FRAME]);

	gs_reg_cache[GS_CACHE_XYOFFSET] = GS_SETREG_XYOFFSET_1( gsGlobal->OffsetX, gsGlobal->OffsetY);
	owl_add_tag(packet, GS_XYOFFSET_1, gs_reg_cache[GS_CACHE_XYOFFSET]);

	gs_reg_cache[GS_CACHE_SCISSOR] = GS_SETREG_SCISSOR_1( 0, gsGlobal->Width - 1, 0, gsGlobal->Height - 1 );
	owl_add_tag(packet, GS_SCISSOR_1, gs_reg_cache[GS_CACHE_SCISSOR]);

	gs_reg_cache[GS_CACHE_TEST] = GS_SETREG_TEST(0, 1, 0x80, 0, 0, 0, 1, DEPTH_ALWAYS);
	owl_add_tag(packet, GS_TEST_1, gs_reg_cache[GS_CACHE_TEST]);

	gs_reg_cache[GS_CACHE_CLAMP] = GS_SETREG_CLAMP(GS_CMODE_REPEAT, GS_CMODE_REPEAT, 0, 0, 0, 0);
	owl_add_tag(packet, GS_CLAMP_1, gs_reg_cache[GS_CACHE_CLAMP]);

	if(gsGlobal->ZBuffering == GS_SETTING_ON)
	{
	    if((gsGlobal->PSM == GS_PSM_CT16) && (gsGlobal->PSMZ != GS_ZBUF_16))
            gsGlobal->PSMZ = GS_ZBUF_16; // seems only non-S 16-bit z depth works with this mode
        if((gsGlobal->PSM != GS_PSM_CT16) && (gsGlobal->PSMZ == GS_ZBUF_16))
            gsGlobal->PSMZ = GS_ZBUF_16S; // other depths don't seem to work with 16-bit non-S z depth

		gs_reg_cache[GS_CACHE_ZBUF] = GS_SETREG_ZBUF_1( gsGlobal->ZBuffer / 8192, gsGlobal->PSMZ, 0 );
	} else {
		gs_reg_cache[GS_CACHE_ZBUF] = GS_SETREG_ZBUF_1( 0, gsGlobal->PSM, 1 );
	}
	owl_add_tag(packet, GS_ZBUF_1, gs_reg_cache[GS_CACHE_ZBUF]);

	gs_reg_cache[GS_CACHE_COLCLAMP] = GS_SETREG_COLCLAMP( 255 );
	owl_add_tag(packet, GS_COLCLAMP, gs_reg_cache[GS_CACHE_COLCLAMP]);

	gs_reg_cache[GS_CACHE_FRAME_2] = GS_SETREG_FRAME_1( gsGlobal->ScreenBuffer[1] / 8192, gsGlobal->Width / 64, gsGlobal->PSM, 0 );
	owl_add_tag(packet, GS_FRAME_2, gs_reg_cache[GS_CACHE_FRAME_2]);

	gs_reg_cache[GS_CACHE_XYOFFSET_2] = gs_reg_cache[GS_CACHE_XYOFFSET];
	owl_add_tag(packet, GS_XYOFFSET_2, gs_reg_cache[GS_CACHE_XYOFFSET_2]);

	gs_reg_cache[GS_CACHE_SCISSOR_2] = gs_reg_cache[GS_CACHE_SCISSOR];
	owl_add_tag(packet, GS_SCISSOR_2, gs_reg_cache[GS_CACHE_SCISSOR_2]);

	gs_reg_cache[GS_CACHE_TEST_2] = gs_reg_cache[GS_CACHE_TEST];
	owl_add_tag(packet, GS_TEST_2, gs_reg_cache[GS_CACHE_TEST_2]);

	gs_reg_cache[GS_CACHE_CLAMP_2] = gs_reg_cache[GS_CACHE_CLAMP];
	owl_add_tag(packet, GS_CLAMP_2, gs_reg_cache[GS_CACHE_CLAMP_2]);

	gs_reg_cache[GS_CACHE_ZBUF_2] = gs_reg_cache[GS_CACHE_ZBUF];
	owl_add_tag(packet, GS_ZBUF_2, gs_reg_cache[GS_CACHE_ZBUF_2]);

	gs_reg_cache[GS_CACHE_ALPHA] = GS_ALPHA_BLEND_NORMAL;
	owl_add_tag(packet, GS_ALPHA_1, gs_reg_cache[GS_CACHE_ALPHA]);

	gs_reg_cache[GS_CACHE_ALPHA_2] = gs_reg_cache[GS_CACHE_ALPHA];
	owl_add_tag(packet, GS_ALPHA_2, gs_reg_cache[GS_CACHE_ALPHA_2]);

	owl_add_tag(packet, GS_DIMX, 
		GS_SETREG_DIMX(gsGlobal->DitherMatrix[0],gsGlobal->DitherMatrix[1],
                gsGlobal->DitherMatrix[2],gsGlobal->DitherMatrix[3],gsGlobal->DitherMatrix[4],
                gsGlobal->DitherMatrix[5],gsGlobal->DitherMatrix[6],gsGlobal->DitherMatrix[7],
                gsGlobal->DitherMatrix[8],gsGlobal->DitherMatrix[9],gsGlobal->DitherMatrix[10],
                gsGlobal->DitherMatrix[11],gsGlobal->DitherMatrix[12],gsGlobal->DitherMatrix[13],
                gsGlobal->DitherMatrix[14],gsGlobal->DitherMatrix[15]));

	owl_add_tag(packet, GS_TEXA, GS_SETREG_TEXA(0x80, 0, 0x80));

	if((gsGlobal->Dithering == GS_SETTING_ON) && ((gsGlobal->PSM == GS_PSM_CT16) || (gsGlobal->PSM == GS_PSM_CT16S))) {
		owl_add_tag(packet, GS_DTHE, 1);
	}

	owl_flush_packet();

}

GSCONTEXT *temp_init_global()
{
    //int8_t dither_matrix[16] = {-4,2,-3,3,0,-2,1,-1,-3,3,-4,2,1,-1,0,-2};
    int8_t dither_matrix[16] = {4,2,5,3,0,6,1,7,5,3,4,2,1,7,0,6}; //different matrix
    int i = 0;

	GSCONTEXT *gsGlobal = calloc(1,sizeof(GSCONTEXT));

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
	s_depth_fog_lut.Vram = 0;
	s_depth_temp_buffer.Vram = 0;
	s_identity_clut.Vram = 0;
	s_depth_fog_dirty = true;

   	draw_buffer.Width = gsGlobal->Width;
	draw_buffer.Height = gsGlobal->Height;
	draw_buffer.PSM = gsGlobal->PSM;
    draw_buffer.ClutPSM = GS_PSM_CT32;

	draw_buffer.Mem = NULL;
	draw_buffer.Clut = NULL;
	draw_buffer.Vram = 0;
	draw_buffer.VramClut = 0;
	draw_buffer.PageAligned = true;
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
	display_buffer.PageAligned = true;
	display_buffer.Filter = GS_FILTER_NEAREST;

	if (gsGlobal->DoubleBuffering) {
		texture_manager_lock_and_bind(gsGlobal, &display_buffer, false);
	}

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

	if (gsGlobal->ZBuffering) {
		texture_manager_lock_and_bind(gsGlobal, &depth_buffer, false);
	}

	gsGlobal->ScreenBuffer[0] = draw_buffer.Vram;
	gsGlobal->ScreenBuffer[1] = display_buffer.Vram;
	gsGlobal->ZBuffer = depth_buffer.Vram;
}

void setVideoMode(s16 mode, int width, int height, int psm, s16 interlace, s16 field, bool zbuffering, int psmz, bool double_buffering, uint8_t pass_count) {
	// Everything below reallocates VRAM and swaps the flip function out from
	// under a frame that may still be drawing.
	graphics_sync();

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
 
void init_graphics() {
	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_VIF0);
	dmaKit_chan_init(DMA_CHANNEL_VIF1);
	dmaKit_wait(DMA_CHANNEL_GIF, 0);
	dmaKit_wait(DMA_CHANNEL_VIF0, 0);
	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	ee_sema_t sema; 
    sema.init_count = 0;
    sema.max_count = 1;
    sema.option = 0;
    vsync_sema_id = CreateSema(&sema);

	owl_init(owl_packet_buffer, OWL_PACKET_BUFFER_SIZE);
	owl_set_pre_kick_hook(present_pending_frame);

	gsGlobal = temp_init_global();

	//gsGlobal->OffsetX = (int)((2048.0f-(gsGlobal->Width/2)) * 16.0f);
	//gsGlobal->OffsetY = (int)((2048.0f-(gsGlobal->Height/2)) * 16.0f);

	gsGlobal->PSM  = GS_PSM_CT24;
	gsGlobal->PSMZ = GS_ZBUF_16S;
	gsGlobal->ZBuffering = GS_SETTING_OFF;
	gsGlobal->DoubleBuffering = GS_SETTING_ON;
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	gsGlobal->Dithering = GS_SETTING_OFF;

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

	for (int i = 0; i < 2; i++) {
    	clearScreen(BLACK_RGBAQ);	
		flipScreen();
	}
}

void graphicWaitVblankStart(){
	*GS_CSR = *GS_CSR & 8;
	while(!(*GS_CSR & 8));
}
