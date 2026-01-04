/**
 * @file mpeg_player.h
 * @brief MPEG1/2 video player using PS2SDK libmpeg and IPU
 * 
 * Provides video playback and video-as-texture functionality
 * integrated with the owl renderer for AthenaEnv.
 */

#ifndef MPEG_PLAYER_H
#define MPEG_PLAYER_H

#include <stdint.h>
#include <stdbool.h>
#include <graphics.h>
#include <libmpeg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Player states
typedef enum {
    MPEG_STATE_STOPPED = 0,
    MPEG_STATE_PLAYING,
    MPEG_STATE_PAUSED,
    MPEG_STATE_ENDED
} MPEGState;

// Forward declaration
typedef struct MPEGPlayer MPEGPlayer;

/**
 * @brief MPEG Player structure
 */
struct MPEGPlayer {
    // Video metadata
    MPEGSequenceInfo info;
    int width;
    int height;
    float fps;
    int frame_count;
    
    // Playback state
    MPEGState state;
    bool loop;
    int current_frame;
    
    // File handling
    FILE* file;
    uint8_t* file_buffer;
    size_t file_buffer_size;
    size_t file_buffer_pos;
    size_t file_size;
    
    // Decoded frame buffer (RGBA32, 16x16 macroblocks from libmpeg)
    uint8_t* frame_data;
    size_t frame_data_size;
    
    // GS texture for rendering
    GSSURFACE texture;
    bool texture_allocated;
    
    // Timing
    int64_t pts_current;
    uint32_t last_decode_time;
    uint32_t frame_time_ms;
    
    // Internal state for callbacks
    bool initialized;
    bool sequence_started;
};

/**
 * @brief Create a new MPEG player instance
 * @param path Path to the MPEG file
 * @return Pointer to player or NULL on failure
 */
MPEGPlayer* mpeg_player_create(const char* path);

/**
 * @brief Destroy a MPEG player and free resources
 * @param player Player instance
 */
void mpeg_player_destroy(MPEGPlayer* player);

/**
 * @brief Start or resume playback
 * @param player Player instance
 */
void mpeg_player_play(MPEGPlayer* player);

/**
 * @brief Pause playback
 * @param player Player instance
 */
void mpeg_player_pause(MPEGPlayer* player);

/**
 * @brief Stop playback and reset to beginning
 * @param player Player instance
 */
void mpeg_player_stop(MPEGPlayer* player);

/**
 * @brief Decode the next frame
 * @param player Player instance
 * @return true if frame decoded, false if end/error
 */
bool mpeg_player_decode_frame(MPEGPlayer* player);

/**
 * @brief Update player (call each frame when playing)
 * Handles timing and frame decoding automatically
 * @param player Player instance
 * @return true if a new frame was decoded
 */
bool mpeg_player_update(MPEGPlayer* player);

/**
 * @brief Upload current frame to VRAM texture
 * @param player Player instance
 */
void mpeg_player_upload_frame(MPEGPlayer* player);

/**
 * @brief Get the texture surface for the current frame
 * @param player Player instance
 * @return Pointer to internal GSSURFACE (do not free!)
 */
GSSURFACE* mpeg_player_get_texture(MPEGPlayer* player);

/**
 * @brief Draw current frame to screen
 * @param player Player instance
 * @param x X position
 * @param y Y position
 * @param w Width (0 = video width)
 * @param h Height (0 = video height)
 */
void mpeg_player_draw(MPEGPlayer* player, float x, float y, float w, float h);

/**
 * @brief Get video width
 */
static inline int mpeg_player_get_width(MPEGPlayer* player) {
    return player ? player->width : 0;
}

/**
 * @brief Get video height
 */
static inline int mpeg_player_get_height(MPEGPlayer* player) {
    return player ? player->height : 0;
}

/**
 * @brief Get video FPS
 */
static inline float mpeg_player_get_fps(MPEGPlayer* player) {
    return player ? player->fps : 0.0f;
}

/**
 * @brief Get player state
 */
static inline MPEGState mpeg_player_get_state(MPEGPlayer* player) {
    return player ? player->state : MPEG_STATE_STOPPED;
}

/**
 * @brief Check if playback has ended
 */
static inline bool mpeg_player_is_ended(MPEGPlayer* player) {
    return player ? player->state == MPEG_STATE_ENDED : true;
}

/**
 * @brief Check if player is ready (has decoded at least one frame)
 */
static inline bool mpeg_player_is_ready(MPEGPlayer* player) {
    return player ? player->sequence_started : false;
}

#ifdef __cplusplus
}
#endif

#endif /* MPEG_PLAYER_H */
