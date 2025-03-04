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


#ifndef __TEXTURE_MANAGER_H__
#define __TEXTURE_MANAGER_H__


#include <gsKit.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRANSFER_REQUEST_MASK 0x80000000

void texture_upload(GSGLOBAL *gsGlobal, GSTEXTURE *Texture);

/// Initialize the texture manager
void texture_manager_init(GSGLOBAL *gsGlobal);

/// Bind a texture to VRAM, will automatically transfer the texture.
int texture_manager_bind(GSGLOBAL *gsGlobal, GSTEXTURE *tex, bool async);

/// Invalidate a texture, will automatically transfer the texture on next bind call.
void texture_manager_invalidate(GSGLOBAL *gsGlobal, GSTEXTURE *tex);

/// Free the texture, this is mainly a performance optimization.
/// The texture will be automatically freed if not used.
void texture_manager_free(GSGLOBAL *gsGlobal, GSTEXTURE *tex);

/// When starting a next frame (on vsync/swap), call this function.
/// It updates texture usage statistics.
void texture_manager_nextFrame(GSGLOBAL *gsGlobal);

int texture_manager_push(GSTEXTURE *tex);

#ifdef __cplusplus
};
#endif


#endif /* __TEXTURE_MANAGER_H__ */
