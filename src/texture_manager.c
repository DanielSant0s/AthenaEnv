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

#define VIF1_INT_INVALID 65535
#define TRANSFER_REQUEST_MASK 0x80000000

struct SVramBlock {
	unsigned int iStart;
	unsigned int iSize;
	unsigned int iUseCount;
	unsigned int iUseCountPrev;

	GSTEXTURE * tex;
	struct SVramBlock * pNext;
	struct SVramBlock * pPrev;
};

static struct SVramBlock * __head = NULL;
static int texture_upload_callback_id = -1;

#define TEXTURE_UPLOAD_QUEUE_SIZE 4096

static int texture_upload_queue_top = 0;

GSTEXTURE *texture_upload_queue[TEXTURE_UPLOAD_QUEUE_SIZE] = { NULL };

uint32_t *VIF1_MARK = (uint32_t *)(0x10003C30);

int upload_texture_handler(int cause) { 
	if (cause == INTC_VIF1) {
		int has_transfer = 0;
		uint32_t tex_id = *VIF1_MARK;

		*VIF1_MARK = 0;

		if (texture_upload_queue[tex_id]) {
			GSTEXTURE *tex = texture_upload_queue[tex_id];
			texture_upload_queue[tex_id] = NULL;

			if ((tex->Vram & TRANSFER_REQUEST_MASK) == TRANSFER_REQUEST_MASK) {
				has_transfer = 1;
				tex->Vram &= ~TRANSFER_REQUEST_MASK;

				gsKit_setup_tbw(tex);

				texture_send(tex->Mem, tex->Width, tex->Height, tex->Vram, tex->PSM, tex->TBW, (tex->Clut? GS_CLUT_TEXTURE : GS_CLUT_NONE));
			}

			if ((tex->VramClut & TRANSFER_REQUEST_MASK) == TRANSFER_REQUEST_MASK) {
				has_transfer = 1;
				tex->VramClut &= ~TRANSFER_REQUEST_MASK;

				if (tex->PSM == GS_PSM_T8) {
					texture_send(tex->Clut, 16, 16, tex->VramClut, tex->ClutPSM, 1, GS_CLUT_PALLETE);

				} else if (tex->PSM == GS_PSM_T4) {
					texture_send(tex->Clut, 8,  2, tex->VramClut, tex->ClutPSM, 1, GS_CLUT_PALLETE);
				}
			}

			int tw, th;
			athena_set_tw_th(tex, &tw, &th);

			owl_packet *packet = owl_open_packet(CHANNEL_GIF, 3);

			owl_add_cnt_tag_fill(packet, 2);

			owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));

			owl_add_tag(packet, 
				GS_TEX0_1, 
				GS_SETREG_TEX0(tex->Vram/256, 
							  tex->TBW, 
							  tex->PSM,
							  tw, th, 
							  true, //gsGlobal->PrimAlphaEnable, 
							  COLOR_MODULATE,
							  tex->VramClut/256, 
							  tex->ClutPSM, 
							  0, 0, 
							  tex->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
			);

			owl_flush_packet();
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

void texture_send(u32 *mem, int width, int height, u32 tbp, u32 psm, u32 tbw, u8 clut)
{
	u32* p_mem;
	int packet_size;
	int packets;
	int remain;
	int qwc;
	int dmasize;

	qwc = gsKit_texture_size_ee(width, height, psm) / 16;
	if( gsKit_texture_size_ee(width, height, psm) % 16 )
	{
		qwc++;
	}

	packets = qwc / GS_GIF_BLOCKSIZE;
	remain  = qwc % GS_GIF_BLOCKSIZE;
	p_mem   = (u32*)mem;

	if(clut == GS_CLUT_TEXTURE)
		packet_size = (7+(packets * 3)+remain);
	else
		packet_size = (9+(packets * 3)+remain);

	if(remain > 0)
		packet_size += 2;

	owl_packet *packet = owl_open_packet(CHANNEL_GIF, packet_size);

	owl_add_cnt_tag_fill(packet, 5);

	owl_add_tag(packet, GIF_AD, GIFTAG(4, 0, 0, 0, 0, 1));

	owl_add_tag(packet, GS_BITBLTBUF, GS_SETREG_BITBLTBUF(0, 0, 0, tbp/256, tbw, psm));
	owl_add_tag(packet, GS_TRXPOS, GS_SETREG_TRXPOS(0, 0, 0, 0, 0));
	owl_add_tag(packet, GS_TRXREG, GS_SETREG_TRXREG(width, height));
	owl_add_tag(packet, GS_TRXDIR, GS_SETREG_TRXDIR(0));

	while (packets-- > 0)
	{
		owl_add_tag(packet, 0, DMA_TAG( 1, 0, DMA_CNT, 0, 0, 0));
		owl_add_tag(packet, 0, GIFTAG( GS_GIF_BLOCKSIZE, 0, 0, 0, GSKIT_GIF_FLG_IMAGE, 0 ));
		owl_add_tag(packet, 0, DMA_TAG( GS_GIF_BLOCKSIZE, 0, DMA_REF, 0, (u32)p_mem, 0 ));

		p_mem+= (GS_GIF_BLOCKSIZE * 4);
	}

	if (remain > 0) {
		owl_add_tag(packet, 0, DMA_TAG( 1, 0, DMA_CNT, 0, 0, 0));
		owl_add_tag(packet, 0, GIFTAG( remain, 0, 0, 0, GSKIT_GIF_FLG_IMAGE, 0 ));
		owl_add_tag(packet, 0, DMA_TAG( remain, 0, DMA_REF, 0, (u32)p_mem, 0 ));
	}

	if(clut != GS_CLUT_TEXTURE)
	{
		owl_open_packet(CHANNEL_GIF, 3);

		owl_add_cnt_tag_fill(packet, 2);
		owl_add_tag(packet, GIF_AD, GIFTAG(1, 0, 0, 0, 0, 1));
		owl_add_tag(packet, GS_TEXFLUSH, 0);
	}
}

void texture_upload(GSGLOBAL *gsGlobal, GSTEXTURE *Texture)
{
	gsKit_setup_tbw(Texture);

	if (Texture->PSM == GS_PSM_T8)
	{
		texture_send(Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_TEXTURE);
		texture_send(Texture->Clut, 16, 16, Texture->VramClut, Texture->ClutPSM, 1, GS_CLUT_PALLETE);

	}
	else if (Texture->PSM == GS_PSM_T4)
	{
		texture_send(Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_TEXTURE);
		texture_send(Texture->Clut, 8,  2, Texture->VramClut, Texture->ClutPSM, 1, GS_CLUT_PALLETE);
	}
	else
	{
		texture_send(Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_NONE);
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
_blockAlloc(unsigned int size)
{
	struct SVramBlock * block = NULL;
	unsigned int weight = 0;

	// Locate free block
	for (block = __head; block != NULL; block = block->pNext) {
		if ((block->tex == NULL) && (block->iSize >= size)) {
			// Free block found (first fit)
			break;
		}
	}

	while (block == NULL) {
		// Free blocks starting with the least used textures
		for (block = __head; block != NULL; block = block->pNext) {
			if ((block->tex != NULL) && (_blockGetWeight(block) <= weight)) {
				// Free block
				block->tex = NULL;
				block = _blockMergeFree(block);
				if (block->iSize >= size) {
					// Free block found (created)
					break;
				}
			}
		}
		weight++;
	}

	// Split the block into the right size
	if (block != NULL) {
		block = _blockSplitFree(block, size);
	}

	return block;
}

//---------------------------------------------------------------------------
// Public functions

//---------------------------------------------------------------------------
void texture_manager_init(GSGLOBAL *gsGlobal)
{
	struct SVramBlock * block = __head;

	// Delete all blocks (if present)
	while(block != NULL)
		block = _blockRemove(block);

	// Allocate the initial free block
	__head = _blockCreate(gsGlobal->CurrentPointer, (4*1024*1024) - gsGlobal->CurrentPointer);

	if (texture_upload_callback_id == -1) {
		add_texture_upload_handler(upload_texture_handler);
	}
}

//---------------------------------------------------------------------------

int texture_manager_push(GSTEXTURE *tex) {
	int id = texture_upload_queue_top;
	texture_upload_queue[id] = tex;

	texture_upload_queue_top = ( texture_upload_queue_top + 1 ) & (TEXTURE_UPLOAD_QUEUE_SIZE-1);

	return id;
}


unsigned int texture_manager_bind(GSGLOBAL *gsGlobal, GSTEXTURE *tex, bool async)
{

	struct SVramBlock * block;
	unsigned int ttransfer = 0;
	unsigned int ctransfer = 0;
	unsigned int tsize;
	unsigned int csize = 0;
	unsigned int cwidth = 16;
	unsigned int cheight = 16;

	// Locate texture
	for (block = __head; block != NULL; block = block->pNext) {
		if(block->tex == tex)
			break;
	}

	tsize = gsKit_texture_size(tex->Width, tex->Height, tex->PSM);
	if (tex->Clut != NULL) {
		cwidth  = (tex->PSM == GS_PSM_T8) ? 16 : 8;
		cheight = (tex->PSM == GS_PSM_T8) ? 16 : 2;
		csize   = gsKit_texture_size(cwidth, cheight, tex->ClutPSM);
	}

	// Allocate new block if not already loaded
	if (block == NULL) {
		block = _blockAlloc(tsize + csize);
		block->tex = tex;
		block->iUseCount = 0;
		block->iUseCountPrev = 1;

		tex->Vram = 0;
		tex->VramClut = 0;
	}

	// (Re-)transfer texture if invalidated
	if (tex->Vram == 0)
		ttransfer = 1;

	// (Re-)transfer clut if invalidated
	if ((tex->Clut) && (tex->VramClut == 0))
		ctransfer = 1;

	if (ttransfer) {
		tex->Vram = block->iStart;
		gsKit_setup_tbw(tex);
		SyncDCache(tex->Mem, (u8 *)(tex->Mem) + tsize);

		if (async) {
			tex->Vram |= TRANSFER_REQUEST_MASK;
		} else {
			texture_send(tex->Mem, tex->Width, tex->Height, tex->Vram, tex->PSM, tex->TBW, tex->Clut ? GS_CLUT_TEXTURE : GS_CLUT_NONE);
		}

	}

	if (ctransfer) {
		tex->VramClut = block->iStart + tsize;
		SyncDCache(tex->Clut, (u8 *)(tex->Clut) + csize);

		if (async) {
			tex->VramClut |= TRANSFER_REQUEST_MASK;
		} else {
			texture_send(tex->Clut, cwidth, cheight, tex->VramClut, tex->ClutPSM, 1, GS_CLUT_PALLETE);
		}
		

	}

	block->iUseCount++;

	if (async)
		return texture_manager_push(tex);

	return (ttransfer|ctransfer);
}

//---------------------------------------------------------------------------

void texture_manager_invalidate(GSGLOBAL *gsGlobal, GSTEXTURE *tex)
{
	tex->Vram = 0;
	tex->VramClut = 0;
}

//---------------------------------------------------------------------------
void texture_manager_free(GSGLOBAL * gsGlobal, GSTEXTURE * tex)
{
	struct SVramBlock * block;

	// Locate texture
	for (block = __head; block != NULL; block = block->pNext) {
		if(block->tex == tex) {
			// Free block
			block->tex = NULL;
			_blockMergeFree(block);
			tex->Vram = 0;
			tex->VramClut = 0;
			break;
		}
	}
}

//---------------------------------------------------------------------------
void texture_manager_nextFrame(GSGLOBAL * gsGlobal)
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
