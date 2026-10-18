#ifndef ATHENA_LZ4_PS2_H
#define ATHENA_LZ4_PS2_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gsSurface;
struct athena_render_data;

/* -------------------------------------------------------------------------
 * R5900-Accelerated LZ4 Block Codec (Scratchpad 0x70003000 Hash Table)
 * ------------------------------------------------------------------------- */

#define ATHENA_LZ4_MAX_INPUT_SIZE 0x01FFFFFF /* 32 MB limit on PS2 EE */
#define ATHENA_LZ4_COMPRESS_BOUND(isize) \
    ((size_t)(isize) > (size_t)ATHENA_LZ4_MAX_INPUT_SIZE ? 0 : ((size_t)(isize) + ((size_t)(isize) / 255) + 16))

size_t athena_lz4_compress_bound(size_t input_size);

/**
 * Compress `src_size` bytes from `src` into `dst` (up to `dst_capacity` bytes).
 * Uses the upper 4 KB of PS2 EE Scratchpad RAM (0x70003000..0x70003FFF) for
 * zero-wait-state hash lookups when called from the main thread.
 *
 * @return Compressed byte count (> 0) on success, or 0 if incompressible / buffer too small.
 */
int athena_lz4_compress(const void *src, void *dst, int src_size, int dst_capacity);

/**
 * Decompress `compressed_size` bytes from `src` into `dst` (up to `dst_capacity` bytes).
 * Uses R5900 64-bit unaligned load/store (`ldl`/`ldr`, `sdl`/`sdr`) wild copies.
 *
 * @return Decompressed byte count (>= 0) on success, or < 0 on malformed stream / overflow.
 */
int athena_lz4_decompress(const void *src, void *dst, int compressed_size, int dst_capacity);

/* -------------------------------------------------------------------------
 * Smart Memory Compactor (Automatic Texture + 3D Mesh + OOM Rescue Daemon)
 * ------------------------------------------------------------------------- */

/* Texture compression flags (stored in GSSURFACE::CompressFlags) */
#define ATHENA_TEX_COMPRESSIBLE  (1u << 0)
#define ATHENA_TEX_COMPRESSED    (1u << 1)
#define ATHENA_TEX_PINNED        (1u << 2)

/* Mesh compression flags (stored in athena_render_data::compress_flags) */
#define ATHENA_MESH_COMPRESSIBLE (1u << 0)
#define ATHENA_MESH_COMPRESSED   (1u << 1)
#define ATHENA_MESH_PINNED       (1u << 2)

typedef struct AthenaMemCompressConfig {
    bool     enabled;              /* Default: true */
    uint32_t frame_budget_us;      /* Max CPU microseconds spent per frame in VBlank tick (default: 350 us) */
    uint32_t texture_vram_frames;  /* Grace frames after VRAM upload before compressing EE copy (default: 15) */
    uint32_t texture_idle_frames;  /* Idle frames before compressing non-VRAM texture in EE RAM (default: 60) */
    uint32_t mesh_idle_frames;     /* Idle frames after VU1 cook before compressing float arrays (default: 30) */
} AthenaMemCompressConfig;

typedef struct AthenaMemCompressStats {
    bool     enabled;
    size_t   compressed_bytes;     /* Current RAM used by LZ4 payloads */
    size_t   uncompressed_bytes;   /* Original size of currently compressed assets */
    size_t   saved_bytes;          /* Net RAM currently saved (uncompressed_bytes - compressed_bytes) */
    uint32_t compressed_textures;  /* Number of textures currently compressed in EE RAM */
    uint32_t tracked_textures;     /* Total registered compressible textures */
    uint32_t compressed_meshes;    /* Number of 3D meshes with compressed float vertex attributes */
    uint32_t tracked_meshes;       /* Total registered compressible 3D meshes */
    uint32_t compress_count;       /* Total asset compressions performed since boot */
    uint32_t decompress_count;     /* Total on-demand asset decompressions performed since boot */
    uint32_t oom_rescues;          /* Times an out-of-memory allocation was rescued by compaction */
    uint32_t frame_budget_us;      /* Active per-frame compaction time budget in microseconds */
} AthenaMemCompressStats;

void athena_mem_compress_init(void);
void athena_mem_compress_get_config(AthenaMemCompressConfig *out_cfg);
void athena_mem_compress_set_config(const AthenaMemCompressConfig *cfg);
void athena_mem_compress_get_stats(AthenaMemCompressStats *out_stats);

/**
 * Called once per frame (in texture_manager_nextFrame) to incrementally
 * compress cold textures and cooked 3D mesh buffers within `frame_budget_us`.
 */
void athena_mem_compress_tick(void);

/**
 * Synchronously compacts cold textures and 3D meshes immediately, ignoring the
 * per-frame time budget. Called automatically by malloc/realloc/calloc/memalign
 * on allocation failure (OOM rescue) or manually via System.compactMemory().
 *
 * @param target_bytes_to_free Stop early once at least this many bytes are freed (0 = compact all eligible).
 * @return Total net bytes freed in EE RAM during this call.
 */
size_t athena_mem_compact_now(size_t target_bytes_to_free);

/** Current frame counter maintained by the compactor. */
uint32_t athena_mem_compress_frame(void);

/* --- Texture hooks --- */
void athena_mem_register_texture(struct gsSurface *tex);
void athena_mem_unregister_texture(struct gsSurface *tex);
bool athena_texture_ensure_uncompressed(struct gsSurface *tex);
void athena_texture_pin(struct gsSurface *tex);
void athena_texture_unpin(struct gsSurface *tex);
void athena_texture_mark_bound(struct gsSurface *tex);
void athena_texture_mark_uploaded(struct gsSurface *tex);

/* --- 3D Mesh hooks --- */
void athena_mem_register_mesh(struct athena_render_data *mesh);
void athena_mem_unregister_mesh(struct athena_render_data *mesh);
bool athena_mesh_ensure_uncompressed(struct athena_render_data *mesh);
void athena_mesh_pin(struct athena_render_data *mesh);
void athena_mesh_unpin(struct athena_render_data *mesh);
void athena_mesh_mark_cpu_access(struct athena_render_data *mesh);

#ifdef __cplusplus
}
#endif

#endif /* ATHENA_LZ4_PS2_H */
