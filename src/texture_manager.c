// 
// A Texture manager for Athena
// 
// ----------------------------------------------------------------------
// Original authors of the code took from gsKit:
// Copyright 2017 - Rick "Maximus32" Gaiser <rgaiser@gmail.com>
// Copyright 2004 - Chris "Neovanglist" Gilbert <Neovanglist@LainOS.org>
// Licenced under Academic Free License version 2.0
// 
// Changes to work with Athena multi-path DMA manager:
// 2025 - Daniel Santos
//

#include <stdio.h>
#include <malloc.h>
#include <kernel.h>

#include <graphics.h>
#include <owl_packet.h>

#include <debug.h>

#include <texture_manager.h>

#define VIF1_MARK_CLEAN 65535

#define TEXTURE_UPLOAD_QUEUE_SIZE 2048

//#define MAX_TEXTURE_PACKET_SIZE 640
#define MAX_TEXTURE_PACKET_SIZE 36864

uint32_t *VIF1_MARK = (uint32_t *)(0x10003C30);

struct SVramBlock {
	unsigned int iStart;
	unsigned int iSize;
	unsigned int iUseCount;
	unsigned int iUseCountPrev;
	unsigned int bLocked;

	GSSURFACE * tex;
	struct SVramBlock * pNext;
	struct SVramBlock * pPrev;
};

static struct SVramBlock * __head = NULL;
static int texture_upload_callback_id = -1;

static int texture_upload_queue_top = 0;

GSSURFACE *texture_upload_queue[TEXTURE_UPLOAD_QUEUE_SIZE] = { NULL };

owl_packet* async_upload_packet = NULL;

owl_qword async_upload_packet_buffer[owl_packet_size(MAX_TEXTURE_PACKET_SIZE)] qw_aligned;

static inline unsigned int align_to_page(unsigned int addr) {
	return (addr + 8191) & ~8191;
}

void texture_send(u32 *mem, int width, int height, u32 tbp, u32 psm, u32 tbw, u8 clut)
{
	//int packet_size;
	int packets;
	int remain;
	int qwc;

	uint32_t tex_size = athena_surface_size(width, height, psm);

	qwc = (tex_size / 16) + (tex_size % 16? 1 : 0);

	packets = qwc / GS_GIF_BLOCKSIZE;
	remain  = qwc % GS_GIF_BLOCKSIZE;

	//if(clut == GS_CLUT_TEXTURE)
	//	packet_size = (6+(packets * 3)+remain);
	//else
	//	packet_size = (9+(packets * 3)+remain);

	//if(remain > 0)
	//	packet_size += 3;

	owl_add_cnt_tag_fill(async_upload_packet, 5);

	owl_add_tag(async_upload_packet, GIF_AD, GIFTAG(4, 0, 0, 0, 0, 1));

	owl_add_tag(async_upload_packet, GS_BITBLTBUF, GS_SETREG_BITBLTBUF(0, 0, 0, tbp/256, tbw, psm));
	owl_add_tag(async_upload_packet, GS_TRXPOS, GS_SETREG_TRXPOS(0, 0, 0, 0, 0));
	owl_add_tag(async_upload_packet, GS_TRXREG, GS_SETREG_TRXREG(width, height));
	owl_add_tag(async_upload_packet, GS_TRXDIR, GS_SETREG_TRXDIR(0));

	while (packets-- > 0)
	{
		owl_add_tag(async_upload_packet, 0, DMA_TAG( 1, 0, DMA_CNT, 0, 0, 0));
		owl_add_tag(async_upload_packet, 0, GIFTAG( GS_GIF_BLOCKSIZE, 0, 0, 0, GSKIT_GIF_FLG_IMAGE, 0 ));
		owl_add_tag(async_upload_packet, 0, DMA_TAG( GS_GIF_BLOCKSIZE, 0, DMA_REF, 0, (u32)mem, 0 ));

		mem += (GS_GIF_BLOCKSIZE * 4);
	}

	if (remain > 0) {
		owl_add_tag(async_upload_packet, 0, DMA_TAG( 1, 0, DMA_CNT, 0, 0, 0));
		owl_add_tag(async_upload_packet, 0, GIFTAG( remain, 0, 0, 0, GSKIT_GIF_FLG_IMAGE, 0 ));
		owl_add_tag(async_upload_packet, 0, DMA_TAG( remain, 0, DMA_REF, 0, (u32)mem, 0 ));
	}

	//if(clut != GS_CLUT_TEXTURE)
	//{
	//	owl_add_end_tag(custom_packet, 2);
	//	owl_add_tag(custom_packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));
	//	owl_add_tag(custom_packet, GS_TEXFLUSH, 0);
	//} else {
	//	owl_add_end_tag(custom_packet, 0);
	//}
}

/**
 * @brief Upload macroblock-formatted texture data to VRAM
 * Data is in 16x16 pixel block format (used by MPEG decoder)
 */
void texture_send_macroblock(u32 *mem, int width, int height, u32 tbp, u32 psm, u32 tbw)
{
	int mb_width = width >> 4;   // Number of macroblocks in width
	int mb_height = height >> 4; // Number of macroblocks in height
	u8* frame_ptr = (u8*)mem;
	
	// Process each macroblock
	for (int mb_y = 0; mb_y < mb_height; mb_y++) {
		for (int mb_x = 0; mb_x < mb_width; mb_x++) {
			int dest_x = mb_x * 16;
			int dest_y = mb_y * 16;
			
			// Setup transfer: CNT tag with 5 qwords of register data
			owl_add_cnt_tag_fill(async_upload_packet, 5);
			
			// GIF AD tag: 4 registers to set
			owl_add_tag(async_upload_packet, GIF_AD, GIFTAG(4, 0, 0, 0, 0, 1));
			
			// Set destination buffer (VRAM address, buffer width, pixel format)
			owl_add_tag(async_upload_packet, GS_BITBLTBUF, 
				GS_SETREG_BITBLTBUF(0, 0, 0, tbp / 256, tbw, psm));
			
			// Set transfer position in destination (x, y in VRAM)
			owl_add_tag(async_upload_packet, GS_TRXPOS, 
				GS_SETREG_TRXPOS(0, 0, dest_x, dest_y, 0));
			
			// Set transfer size (16x16 pixels)
			owl_add_tag(async_upload_packet, GS_TRXREG, 
				GS_SETREG_TRXREG(16, 16));
			
			// Set transfer direction (host to local = upload)
			owl_add_tag(async_upload_packet, GS_TRXDIR, 
				GS_SETREG_TRXDIR(0));
			
			// Send the actual pixel data for this macroblock
			// Each macroblock is 16x16 = 256 pixels * 4 bytes = 1024 bytes = 64 quadwords
			owl_add_tag(async_upload_packet, 0, DMA_TAG(1, 0, DMA_CNT, 0, 0, 0));
			owl_add_tag(async_upload_packet, 0, GIFTAG(64, 0, 0, 0, GSKIT_GIF_FLG_IMAGE, 0));
			owl_add_tag(async_upload_packet, 0, DMA_TAG(64, 0, DMA_REF, 0, (u32)frame_ptr, 0));
			
			// Move to next macroblock in source buffer
			frame_ptr += 1024;  // 16x16 * 4 bytes
		}
	}
}

int upload_texture_handler(int cause) { 
	DIntr();

	if (cause == INTC_VIF1) {
		uint32_t tex_id = *VIF1_MARK;

		*VIF1_MARK = VIF1_MARK_CLEAN;

		if (texture_upload_queue[tex_id]) {
			GSSURFACE *tex = texture_upload_queue[tex_id];
			texture_upload_queue[tex_id] = NULL;

			

			if ((tex->VramClut & TRANSFER_REQUEST_MASK) == TRANSFER_REQUEST_MASK) {
				tex->VramClut &= ~TRANSFER_REQUEST_MASK;

				if (tex->PSM == GS_PSM_T8) {
					texture_send(tex->Clut, 16, 16, tex->VramClut, tex->ClutPSM, 1, GS_CLUT_PALLETE);

				} else if (tex->PSM == GS_PSM_T4) {
					texture_send(tex->Clut, 8,  2, tex->VramClut, tex->ClutPSM, 1, GS_CLUT_PALLETE);
				}
			}

			if ((tex->Vram & TRANSFER_REQUEST_MASK) == TRANSFER_REQUEST_MASK) {
				tex->Vram &= ~TRANSFER_REQUEST_MASK;

				athena_calculate_tbw(tex);
				
				// Select upload function based on data format
				if (tex->Macroblock) {
					texture_send_macroblock(tex->Mem, tex->Width, tex->Height, tex->Vram, tex->PSM, tex->TBW);
				} else {
					texture_send(tex->Mem, tex->Width, tex->Height, tex->Vram, tex->PSM, tex->TBW, (tex->Clut ? GS_CLUT_TEXTURE : GS_CLUT_NONE));
				}
			}

			owl_add_end_tag(async_upload_packet, 2);
			owl_add_tag(async_upload_packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));
			owl_add_tag(async_upload_packet, GS_TEXFLUSH, 0);

			owl_send_packet(async_upload_packet);
		}

		VIF1_FBRST->STC = true;
	}

	ExitHandler();	
	return 0;
}

#define VIF0_ERR        ((uint32_t *)(0x10003820))
#define VIF1_ERR        ((uint32_t *)(0x10003c20))
#define VIF0_ERR_ME0_M      (0x01<<1)
#define VIF1_ERR_ME0_M      (0x01<<1)

void add_texture_upload_handler(int (*upload_callback)())
{
	DIntr();

	texture_upload_callback_id = AddIntcHandler(INTC_VIF1, upload_callback, -1);
	*VIF0_ERR = VIF0_ERR_ME0_M;
	*VIF1_ERR = VIF1_ERR_ME0_M;
	EnableIntc(INTC_VIF1);

	EIntr();
}

void remove_texture_upload_handler()
{
	DIntr();

	DisableIntc(INTC_VIF1);

	RemoveIntcHandler(INTC_VIF1, texture_upload_callback_id);

	EIntr();
}



void texture_upload(GSCONTEXT *gsGlobal, GSSURFACE *Texture)
{
	athena_calculate_tbw(Texture);

	if (Texture->PSM == GS_PSM_T8)
	{
		//texture_send(Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_TEXTURE, NULL);
		//texture_send(Texture->Clut, 16, 16, Texture->VramClut, Texture->ClutPSM, 1, GS_CLUT_PALLETE, NULL);

	}
	else if (Texture->PSM == GS_PSM_T4)
	{
		//texture_send(Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_TEXTURE, NULL);
		//texture_send(Texture->Clut, 8,  2, Texture->VramClut, Texture->ClutPSM, 1, GS_CLUT_PALLETE, NULL);
	}
	else
	{
		//texture_send(Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_NONE, NULL);
	}
}

//---------------------------------------------------------------------------
// Private functions

//---------------------------------------------------------------------------
static inline struct SVramBlock *
_blockCreate(unsigned int start, unsigned int size)
{
	struct SVramBlock * block;


	block = malloc(sizeof(struct SVramBlock));
	
	block->iStart = start;
	block->iSize = size;
	block->iUseCount = 0;
	block->iUseCountPrev = 0;
	block->bLocked = 0;

	block->tex = NULL;
	block->pNext = NULL;
	block->pPrev = NULL;

	return block;
}

//---------------------------------------------------------------------------
static inline void
_blockInsertAfter(struct SVramBlock * block, struct SVramBlock * next)
{
	next->pNext = block->pNext;
	next->pPrev = block;
	if(block->pNext != NULL) {
		block->pNext->pPrev = next;
	}
		
	block->pNext = next;
}

static inline struct SVramBlock *
_blockSplitFreeAligned(struct SVramBlock * block, unsigned int size, unsigned int aligned_start)
{
	struct SVramBlock * pNewBlock;
	struct SVramBlock * pAlignmentBlock = NULL;

	if (aligned_start > block->iStart) {
		pAlignmentBlock = _blockCreate(block->iStart, aligned_start - block->iStart);
		_blockInsertAfter(block, pAlignmentBlock);

		block->iStart = aligned_start;
		block->iSize -= (aligned_start - block->iStart);
	}

	if (block->iSize > size) {
		pNewBlock = _blockCreate(block->iStart + size, block->iSize - size);
		block->iSize = size;
		_blockInsertAfter(block, pNewBlock);
	}

	return block;
}

//---------------------------------------------------------------------------
static inline struct SVramBlock *
_blockRemove(struct SVramBlock * block)
{
	struct SVramBlock * next = block->pNext;

	if(block->pPrev != NULL)
		block->pPrev->pNext = block->pNext;
	if(block->pNext != NULL)
		block->pNext->pPrev = block->pPrev;

	free(block);

	return next;
}

//---------------------------------------------------------------------------
static inline struct SVramBlock *
_blockSplitFree(struct SVramBlock * block, unsigned int size)
{
	struct SVramBlock * pNewBlock;

	// Create second block with leftover size
	pNewBlock = _blockCreate(block->iStart + size, block->iSize - size);
	// Shrink first block
	block->iSize = size;
	// Insert second block after first block
	_blockInsertAfter(block, pNewBlock);

	return block;
}

//---------------------------------------------------------------------------
static inline struct SVramBlock *
_blockMergeFree(struct SVramBlock * block)
{
	// Search backwards to the first free block
	while ((block->pPrev != NULL) && (block->pPrev->tex == NULL))
		block = block->pPrev;

	// Merge free blocks
	while((block->pNext != NULL) && (block->pNext->tex == NULL)) {
		block->iSize += block->pNext->iSize;
		_blockRemove(block->pNext);
	}

	return block;
}

//---------------------------------------------------------------------------
// How often is this vram block used, mainly based on predictions
static inline unsigned int
_blockGetWeight(struct SVramBlock * block)
{
	unsigned int weight = 0;

	if ((block != NULL) && (block->tex != NULL)) {
		if (block->bLocked) {
			return 0xFFFFFFFF;
		}

		if(block->iUseCount == block->iUseCountPrev) {
			// Prediction:
			// - This frame: done
			// - Next frame: needed
			weight += block->iUseCountPrev;
		}
		else if(block->iUseCount < block->iUseCountPrev) {
			// Prediction:
			// - This frame: needed
			weight += block->iUseCountPrev - block->iUseCount;
			// - Next frame: needed
			weight += block->iUseCountPrev;
		}
		else {
			// Prediction:
			// - This frame: unsure
			weight += 1;
			// - Next frame: needed
			weight += block->iUseCount;
		}
	}

	return weight;
}

//---------------------------------------------------------------------------
// Simple block allocator
static inline struct SVramBlock *
_blockAlloc(unsigned int size, bool page_aligned)
{
	struct SVramBlock * block = NULL;
	unsigned int weight = 0;
	unsigned int aligned_start;
	unsigned int required_size;

	for (block = __head; block != NULL; block = block->pNext) {
		if (block->tex == NULL) {
			if (page_aligned) {
				aligned_start = align_to_page(block->iStart);
				required_size = size + (aligned_start - block->iStart);
				if (block->iSize >= required_size) {
					break;
				}
			} else if (block->iSize >= size) {
				break;
			}
		}
	}

	while (block == NULL) {
		for (block = __head; block != NULL; block = block->pNext) {
			if ((block->tex != NULL) && !block->bLocked && (_blockGetWeight(block) <= weight)) {
				block->tex = NULL;
				block = _blockMergeFree(block);
				
				if (page_aligned) {
					aligned_start = align_to_page(block->iStart);
					required_size = size + (aligned_start - block->iStart);
					if (block->iSize >= required_size) {
						break;
					}
				} else if (block->iSize >= size) {
					break;
				}
			}
		}
		weight++;
	}

	if (block != NULL) {
		if (page_aligned) {
			aligned_start = align_to_page(block->iStart);
			block = _blockSplitFreeAligned(block, size, aligned_start);
		} else {
			block = _blockSplitFree(block, size);
		}
	}

	return block;
}

//---------------------------------------------------------------------------
// Public functions

//---------------------------------------------------------------------------
void texture_manager_init(GSCONTEXT *gsGlobal)
{
	struct SVramBlock * block = __head;

	// Delete all blocks (if present)
	while(block != NULL)
		block = _blockRemove(block);

	// Allocate the initial free block
	__head = _blockCreate(0, 4*1024*1024);

	*VIF1_MARK = VIF1_MARK_CLEAN;

	if (texture_upload_callback_id == -1) {
		add_texture_upload_handler(upload_texture_handler);
	}

	async_upload_packet = owl_create_packet(CHANNEL_GIF, owl_packet_size(MAX_TEXTURE_PACKET_SIZE), async_upload_packet_buffer);
}

//---------------------------------------------------------------------------

int texture_manager_push(GSSURFACE *tex) {
	int id = texture_upload_queue_top;
	texture_upload_queue[id] = tex;

	texture_upload_queue_top = ( texture_upload_queue_top + 1 ) & (TEXTURE_UPLOAD_QUEUE_SIZE-1);

	return id;
}


int texture_manager_bind(GSCONTEXT *gsGlobal, GSSURFACE *tex, bool async) {
	struct SVramBlock * block;
	unsigned int ttransfer = 0;
	unsigned int ctransfer = 0;
	unsigned int tsize;
	unsigned int csize = 0;
	unsigned int cwidth = 16;
	unsigned int cheight = 16;

	for (block = __head; block != NULL; block = block->pNext) {
		if(block->tex == tex)
			break;
	}

	tsize = athena_vram_surface_size(tex->Width, tex->Height, tex->PSM);
	if (tex->Clut != NULL) {
		cwidth  = (tex->PSM == GS_PSM_T8) ? 16 : 8;
		cheight = (tex->PSM == GS_PSM_T8) ? 16 : 2;
		csize   = athena_vram_surface_size(cwidth, cheight, tex->ClutPSM);
	}

	if (block == NULL) {
		block = _blockAlloc(tsize + csize, tex->PageAligned);
		block->tex = tex;
		block->iUseCount = 0;
		block->iUseCountPrev = 1;
		block->bLocked = 0;

		tex->Vram = 0;
		tex->VramClut = 0;
	}

	if (tex->Vram == 0)
		ttransfer = 1;

	if (tex->VramClut == 0)
		ctransfer = 1;

	if (ttransfer) {
		if (tex->PageAligned) {
			tex->Vram = align_to_page(block->iStart);
		} else {
			tex->Vram = block->iStart;
		}
		
		athena_calculate_tbw(tex);

		if (tex->Mem) {
			SyncDCache(tex->Mem, (u8 *)(tex->Mem) + tsize);

			if (async) {
				tex->Vram |= TRANSFER_REQUEST_MASK;
			} else {
				// Sync upload - select function based on data format
				if (tex->Macroblock) {
					texture_send_macroblock(tex->Mem, tex->Width, tex->Height, tex->Vram, tex->PSM, tex->TBW);
				} else {
					texture_send(tex->Mem, tex->Width, tex->Height, tex->Vram, tex->PSM, tex->TBW, tex->Clut ? GS_CLUT_TEXTURE : GS_CLUT_NONE);
				}
			}
		} else {
			ttransfer = 0;
		}
	}

	if (ctransfer) {
		if (tex->PageAligned) {
			tex->VramClut = align_to_page(block->iStart) + tsize;
		} else {
			tex->VramClut = block->iStart + tsize;
		}

		if (tex->Clut) {
			SyncDCache(tex->Clut, (u8 *)(tex->Clut) + csize);

			if (async) {
				tex->VramClut |= TRANSFER_REQUEST_MASK;
			} else {
				//texture_send(tex->Clut, cwidth, cheight, tex->VramClut, tex->ClutPSM, 1, GS_CLUT_PALLETE, NULL);
			}
		} else {
			ctransfer = 0;
		}
	}

	block->iUseCount++;

	if (async)
		return (ttransfer|ctransfer)? texture_manager_push(tex) : -1;

	return (ttransfer|ctransfer);
}

//---------------------------------------------------------------------------
int texture_manager_lock(GSSURFACE *tex)
{
	struct SVramBlock * block;

	for (block = __head; block != NULL; block = block->pNext) {
		if(block->tex == tex) {
			block->bLocked = 1;
			return 1;  
		}
	}
	
	return 0;  
}

//---------------------------------------------------------------------------
int texture_manager_unlock(GSSURFACE *tex)
{
	struct SVramBlock * block;

	for (block = __head; block != NULL; block = block->pNext) {
		if(block->tex == tex) {
			block->bLocked = 0;
			return 1;
		}
	}
	
	return 0;  
}

int texture_manager_is_locked(GSSURFACE *tex)
{
	struct SVramBlock * block;

	for (block = __head; block != NULL; block = block->pNext) {
		if(block->tex == tex) {
			return block->bLocked;
		}
	}
	
	return 0;  
}

int texture_manager_lock_and_bind(GSCONTEXT *gsGlobal, GSSURFACE *tex, bool async)
{
	int result = texture_manager_bind(gsGlobal, tex, async);
	
	if (result >= 0) {
		texture_manager_lock(tex);
	}
	
	return result;
}

int texture_manager_get_locked_count()
{
	struct SVramBlock * block;
	int count = 0;

	for (block = __head; block != NULL; block = block->pNext) {
		if ((block->tex != NULL) && block->bLocked) {
			count++;
		}
	}
	
	return count;
}

unsigned int texture_manager_get_locked_memory()
{
	struct SVramBlock * block;
	unsigned int total = 0;

	for (block = __head; block != NULL; block = block->pNext) {
		if ((block->tex != NULL) && block->bLocked) {
			total += block->iSize;
		}
	}
	
	return total;
}

unsigned int texture_manager_get_unlocked_memory()
{
	struct SVramBlock * block;
	unsigned int total = 0;

	for (block = __head; block != NULL; block = block->pNext) {
		if ((block->tex != NULL) && !block->bLocked) {
			total += block->iSize;
		}
	}
	
	return total;
}

unsigned int texture_manager_used_memory()
{
	struct SVramBlock * block;
	unsigned int total = 0;

	for (block = __head; block != NULL; block = block->pNext) {
		if (block->tex != NULL) {
			total += block->iSize;
		}
	}
	
	return total;
}

void texture_manager_invalidate(GSSURFACE *tex)
{
	tex->Vram = 0;
	tex->VramClut = 0;
}

//---------------------------------------------------------------------------
void texture_manager_free(GSSURFACE * tex)
{
	struct SVramBlock * block;

	// Locate texture
	for (block = __head; block != NULL; block = block->pNext) {
		if(block->tex == tex) {
			// Free block
			block->tex = NULL;
			block->bLocked = 0;
			_blockMergeFree(block);
			tex->Vram = 0;
			tex->VramClut = 0;
			break;
		}
	}
}

//---------------------------------------------------------------------------
void texture_manager_nextFrame(GSCONTEXT * gsGlobal)
{
	struct SVramBlock * block;

	// Register use count
	for(block = __head; block != NULL; block = block->pNext) {
		if(block->tex != NULL) {
			block->iUseCountPrev = block->iUseCount;
			block->iUseCount = 0;
		}
	}
}
