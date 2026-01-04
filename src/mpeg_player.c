/**
 * @file mpeg_player.c
 * @brief MPEG1/2 video player implementation using PS2SDK libmpeg
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <kernel.h>
#include <dmaKit.h>
#include <time.h>

#include <mpeg_player.h>
#include <owl_packet.h>
#include <texture_manager.h>
#include <graphics.h>

// Buffer size for file reading (sent to IPU in chunks)
#define MPEG_FILE_BUFFER_SIZE (64 * 1024)  // 64KB buffer
#define MPEG_DMA_CHUNK_SIZE   2048         // 2KB per DMA transfer to IPU

// Current player (used by callbacks - libmpeg uses global callbacks)
static MPEGPlayer* g_current_player = NULL;
/**
 * @brief DMA callback - feeds data to IPU
 * Called by libmpeg when it needs more data
 */
static int mpeg_set_dma_callback(void* user_data) {
    MPEGPlayer* player = g_current_player;
    
    if (!player || !player->file) {
        return 0;  // End of data
    }
    
    // Check if we need to refill buffer from file
    if (player->file_buffer_pos >= player->file_buffer_size) {
        // Read more data from file
        size_t bytes_read = fread(player->file_buffer, 1, MPEG_FILE_BUFFER_SIZE, player->file);
        
        if (bytes_read == 0) {
            // Check for loop
            if (player->loop) {
                fseek(player->file, 0, SEEK_SET);
                bytes_read = fread(player->file_buffer, 1, MPEG_FILE_BUFFER_SIZE, player->file);
                if (bytes_read == 0) {
                    return 0;  // Still no data - real end
                }
                player->current_frame = 0;
            } else {
                return 0;  // End of file
            }
        }
        
        player->file_buffer_size = bytes_read;
        player->file_buffer_pos = 0;
    }
    
    // Calculate how much to send (up to chunk size)
    size_t remaining = player->file_buffer_size - player->file_buffer_pos;
    size_t to_send = (remaining > MPEG_DMA_CHUNK_SIZE) ? MPEG_DMA_CHUNK_SIZE : remaining;
    
    // Align to 16 bytes
    to_send = (to_send + 15) & ~15;
    if (to_send > remaining) {
        to_send = remaining & ~15;
        if (to_send == 0) {
            return 0;
        }
    }
    
    // Wait for previous transfer and send new data to IPU using dmaKit
    dmaKit_wait(DMA_CHANNEL_TOIPU, 0);
    dmaKit_send(DMA_CHANNEL_TOIPU, 
        player->file_buffer + player->file_buffer_pos, 
        to_send >> 4);  // QWC (quadwords)
    
    player->file_buffer_pos += to_send;
    
    return 1;  // Data sent
}

/**
 * @brief Initialization callback - called when sequence header is found
 * Allocates frame buffer and initializes DMA transfer chain
 */
static void* mpeg_init_callback(void* user_data, MPEGSequenceInfo* info) {
    MPEGPlayer* player = g_current_player;
    
    if (!player) {
        return NULL;
    }
    
    // Store video info
    memcpy(&player->info, info, sizeof(MPEGSequenceInfo));
    player->width = info->m_Width;
    player->height = info->m_Height;
    player->frame_count = info->m_FrameCnt;
    
    // Calculate FPS from ms per frame
    if (info->m_MSPerFrame > 0) {
        player->fps = 1000.0f / (float)info->m_MSPerFrame;
        player->frame_time_ms = info->m_MSPerFrame;
    } else {
        player->fps = 25.0f;  // Default to 25fps
        player->frame_time_ms = 40;
    }
    
    // Allocate frame data buffer (RGBA32)
    size_t frame_size = player->width * player->height * 4;
    
    if (player->frame_data) {
        free(player->frame_data);
    }
    
    player->frame_data = (uint8_t*)memalign(64, frame_size);
    if (!player->frame_data) {
        return NULL;
    }
    player->frame_data_size = frame_size;
    
    // Sync cache for DMA
    SyncDCache(player->frame_data, player->frame_data + frame_size);
    
    // Setup texture surface
    player->texture.Width = player->width;
    player->texture.Height = player->height;
    player->texture.PSM = GS_PSM_CT32;
    player->texture.Mem = (uint32_t*)player->frame_data;
    player->texture.Vram = 0;  // Will be allocated by texture manager
    player->texture.Filter = GS_FILTER_LINEAR;
    player->texture.Delayed = 0;
    player->texture.Macroblock = 1;  // Data is in macroblock format
    athena_calculate_tbw(&player->texture);
    
    player->sequence_started = true;
    player->initialized = true;
    
    return player->frame_data;
}
MPEGPlayer* mpeg_player_create(const char* path) {
    MPEGPlayer* player = (MPEGPlayer*)calloc(1, sizeof(MPEGPlayer));
    if (!player) {
        return NULL;
    }
    
    // Open file
    player->file = fopen(path, "rb");
    if (!player->file) {
        free(player);
        return NULL;
    }
    
    // Get file size
    fseek(player->file, 0, SEEK_END);
    player->file_size = ftell(player->file);
    fseek(player->file, 0, SEEK_SET);
    
    // Allocate file read buffer
    player->file_buffer = (uint8_t*)memalign(64, MPEG_FILE_BUFFER_SIZE);
    if (!player->file_buffer) {
        fclose(player->file);
        free(player);
        return NULL;
    }
    
    // Initial buffer read
    player->file_buffer_size = fread(player->file_buffer, 1, MPEG_FILE_BUFFER_SIZE, player->file);
    player->file_buffer_pos = 0;
    
    // Initialize state
    player->state = MPEG_STATE_STOPPED;
    player->loop = false;
    player->current_frame = 0;
    player->initialized = false;
    player->sequence_started = false;
    player->texture_allocated = false;
    
    // Initialize DMA channel for IPU using dmaKit
    // Note: dmaKit_init is already called in init_graphics()
    dmaKit_chan_init(DMA_CHANNEL_TOIPU);
    
    // Set as current player for callbacks
    g_current_player = player;
    
    // Initialize libmpeg
    MPEG_Initialize(mpeg_set_dma_callback, NULL, mpeg_init_callback, player, &player->pts_current);
    
    // Decode first frame to get video info
    if (!MPEG_Picture(player->frame_data, &player->pts_current)) {
        // Failed to decode first frame
        if (!player->sequence_started) {
            mpeg_player_destroy(player);
            return NULL;
        }
    }
    
    return player;
}

void mpeg_player_destroy(MPEGPlayer* player) {
    if (!player) {
        return;
    }
    
    if (player == g_current_player) {
        MPEG_Destroy();
        g_current_player = NULL;
    }
    
    if (player->file) {
        fclose(player->file);
        player->file = NULL;
    }
    
    if (player->file_buffer) {
        free(player->file_buffer);
        player->file_buffer = NULL;
    }
    
    if (player->frame_data) {
        free(player->frame_data);
        player->frame_data = NULL;
    }
    
    // Free texture VRAM
    if (player->texture_allocated && player->texture.Vram) {
        texture_manager_free(&player->texture);
    }
    
    free(player);
}

void mpeg_player_play(MPEGPlayer* player) {
    if (!player || !player->initialized) {
        return;
    }
    
    if (player->state == MPEG_STATE_ENDED && player->loop) {
        // Restart from beginning
        fseek(player->file, 0, SEEK_SET);
        player->file_buffer_size = fread(player->file_buffer, 1, MPEG_FILE_BUFFER_SIZE, player->file);
        player->file_buffer_pos = 0;
        player->current_frame = 0;
    }
    
    player->state = MPEG_STATE_PLAYING;
    player->last_decode_time = clock() / (CLOCKS_PER_SEC / 1000);  // ms
}

void mpeg_player_pause(MPEGPlayer* player) {
    if (!player) {
        return;
    }
    
    if (player->state == MPEG_STATE_PLAYING) {
        player->state = MPEG_STATE_PAUSED;
    }
}

void mpeg_player_stop(MPEGPlayer* player) {
    if (!player) {
        return;
    }
    
    player->state = MPEG_STATE_STOPPED;
    
    // Reset to beginning
    if (player->file) {
        fseek(player->file, 0, SEEK_SET);
        player->file_buffer_size = fread(player->file_buffer, 1, MPEG_FILE_BUFFER_SIZE, player->file);
        player->file_buffer_pos = 0;
    }
    player->current_frame = 0;
}

bool mpeg_player_decode_frame(MPEGPlayer* player) {
    if (!player || !player->initialized || !player->frame_data) {
        return false;
    }
    
    // Set current player for callbacks
    g_current_player = player;
    
    // Try to decode next frame
    int64_t pts;
    if (!MPEG_Picture(player->frame_data, &pts)) {
        // Decode failed - check if end of file
        if (player->info.m_fEOF) {
            if (player->loop) {
                // Restart
                fseek(player->file, 0, SEEK_SET);
                player->file_buffer_size = fread(player->file_buffer, 1, MPEG_FILE_BUFFER_SIZE, player->file);
                player->file_buffer_pos = 0;
                player->current_frame = 0;
                
                // Re-initialize MPEG decoder
                MPEG_Destroy();
                MPEG_Initialize(mpeg_set_dma_callback, NULL, mpeg_init_callback, player, &player->pts_current);
                
                // Try decode again
                if (!MPEG_Picture(player->frame_data, &pts)) {
                    player->state = MPEG_STATE_ENDED;
                    return false;
                }
            } else {
                player->state = MPEG_STATE_ENDED;
                return false;
            }
        } else {
            return false;  // Decode error
        }
    }
    
    player->pts_current = pts;
    player->current_frame++;
    
    return true;
}

bool mpeg_player_update(MPEGPlayer* player) {
    if (!player || player->state != MPEG_STATE_PLAYING) {
        return false;
    }
    
    // Check timing
    uint32_t current_time = clock() / (CLOCKS_PER_SEC / 1000);
    uint32_t elapsed = current_time - player->last_decode_time;
    
    if (elapsed >= player->frame_time_ms) {
        player->last_decode_time = current_time;
        
        if (mpeg_player_decode_frame(player)) {
            mpeg_player_upload_frame(player);
            return true;
        }
    }
    
    return false;
}

void mpeg_player_upload_frame(MPEGPlayer* player) {
    if (!player || !player->frame_data || !player->sequence_started) {
        return;
    }
    
    // Sync cache for frame data
    SyncDCache(player->frame_data, player->frame_data + player->frame_data_size);
    
    // Invalidate texture so it gets re-uploaded with new frame data
    texture_manager_invalidate(&player->texture);
}

GSSURFACE* mpeg_player_get_texture(MPEGPlayer* player) {
    if (!player || !player->sequence_started) {
        return NULL;
    }
    return &player->texture;
}

void mpeg_player_draw(MPEGPlayer* player, float x, float y, float w, float h) {
    if (!player || !player->sequence_started) {
        return;
    }
    
    // Use video dimensions if not specified
    float draw_w = (w > 0) ? w : (float)player->width;
    float draw_h = (h > 0) ? h : (float)player->height;
    
    // Use existing draw_image - texture_manager handles VRAM binding
    draw_image(&player->texture, x, y, draw_w, draw_h, 
               0, 0, (float)player->width, (float)player->height, 
               0x80808080);
}
