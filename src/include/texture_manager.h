// 
// A Texture manager for Athena
// 
// ----------------------------------------------------------------------
// Original authors of the code took from gsKit:
// Copyright 2017 - Rick "Maximus32" Gaiser <rgaiser@gmail.com>
// Copyright 2004 - Chris "Neovanglist" Gilbert <Neovanglist@LainOS.org>
// Licenced under Academic Free License version 2.0
// 
// Changes to work with Athena multi-path DMA manager, cache lock system and memory tracking:
// 2025 - Daniel Santos
//


#ifndef __TEXTURE_MANAGER_H__
#define __TEXTURE_MANAGER_H__

#include <graphics.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRANSFER_REQUEST_MASK 0x80000000

void texture_upload(GSCONTEXT *gsGlobal, GSSURFACE *Texture);

/// Initialize the texture manager
void texture_manager_init(GSCONTEXT *gsGlobal);

/// Bind a texture to VRAM, will automatically transfer the texture.
int texture_manager_bind(GSCONTEXT *gsGlobal, GSSURFACE *tex, bool async);

/// Invalidate a texture, will automatically transfer the texture on next bind call.
void texture_manager_invalidate(GSSURFACE *tex);

/// Free the texture, this is mainly a performance optimization.
/// The texture will be automatically freed if not used.
void texture_manager_free(GSSURFACE *tex);

/// When starting a next frame (on vsync/swap), call this function.
/// It updates texture usage statistics.
void texture_manager_nextFrame(GSCONTEXT *gsGlobal);

int texture_manager_push(GSSURFACE *tex);

int texture_manager_lock(GSSURFACE *tex);

int texture_manager_unlock(GSSURFACE *tex);

int texture_manager_is_locked(GSSURFACE *tex);

int texture_manager_lock_and_bind(GSCONTEXT *gsGlobal, GSSURFACE *tex, bool async);

unsigned int texture_manager_used_memory();

unsigned int texture_manager_get_locked_memory();

unsigned int texture_manager_get_unlocked_memory();

#ifdef __cplusplus
};
#endif


#endif /* __TEXTURE_MANAGER_H__ */
