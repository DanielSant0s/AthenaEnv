#include <lz4_ps2.h>
#include <graphics.h>
#include <render.h>
#include <memory.h>
#include <kernel.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Part 1: R5900-Accelerated LZ4 Block Codec
 * =========================================================================
 *
 * Hardware layout on the PlayStation 2 Emotion Engine (R5900):
 * - Scratchpad RAM: 0x70000000 .. 0x70003FFF (16 KB, 0 wait-state SRAM).
 *   libmpeg uses 0x70000000 .. 0x70002A00 during MPEG macroblock decode.
 *   The upper 4 KB (0x70003000 .. 0x70003FFF) is completely unused across
 *   the engine, making it an ideal 1024-entry (4 KB) LZ4 hash table with
 *   zero D-Cache eviction and zero heap overhead.
 * - Unaligned loads/stores: GCC for R5900 lowers packed 32/64-bit struct
 *   accesses directly into `lwl`/`lwr` + `swl`/`swr` and `ldl`/`ldr` +
 *   `sdl`/`sdr` pairs (2 instructions each, no alignment exceptions).
 */

#define LZ4_HASHLOG          10
#define LZ4_HASHTABLESIZE    (1u << LZ4_HASHLOG) /* 1024 entries = 4096 bytes */
#define LZ4_MINMATCH         4
#define LZ4_COPYLENGTH       8
#define LZ4_LASTLITERALS     5
#define LZ4_MFLIMIT          (LZ4_COPYLENGTH + LZ4_MINMATCH)
#define LZ4_MIN_LENGTH       (LZ4_MFLIMIT + 1)
#define LZ4_MAX_DISTANCE     65535
#define LZ4_SKIP_TRIGGER     6

#define PS2_SPAD_HASH_ADDR   0x70003000u

typedef struct __attribute__((packed)) { uint16_t v; } lz4_u16_p;
typedef struct __attribute__((packed)) { uint32_t v; } lz4_u32_p;
typedef struct __attribute__((packed)) { uint64_t v; } lz4_u64_p;

static inline uint16_t lz4_read16(const void *p) { return ((const lz4_u16_p *)p)->v; }
static inline uint32_t lz4_read32(const void *p) { return ((const lz4_u32_p *)p)->v; }
static inline uint64_t lz4_read64(const void *p) { return ((const lz4_u64_p *)p)->v; }

static inline void lz4_write16(void *p, uint16_t v) { ((lz4_u16_p *)p)->v = v; }
static inline void lz4_write32(void *p, uint32_t v) { ((lz4_u32_p *)p)->v = v; }
static inline void lz4_write64(void *p, uint64_t v) { ((lz4_u64_p *)p)->v = v; }

static inline uint32_t lz4_hash4(uint32_t sequence) {
    return (sequence * 2654435761u) >> (32 - LZ4_HASHLOG);
}

static inline void lz4_wild_copy16(uint8_t *dst, const uint8_t *src, const uint8_t *dst_end) {
    do {
        lz4_write64(dst,     lz4_read64(src));
        lz4_write64(dst + 8, lz4_read64(src + 8));
        dst += 16;
        src += 16;
    } while (dst < dst_end);
}

static inline void lz4_wild_copy8(uint8_t *dst, const uint8_t *src, const uint8_t *dst_end) {
    do {
        lz4_write64(dst, lz4_read64(src));
        dst += 8;
        src += 8;
    } while (dst < dst_end);
}

/* Count matching bytes after the initial 4-byte match using 64-bit XOR + CTZ */
static inline unsigned int lz4_count_match(const uint8_t *pIn, const uint8_t *pMatch, const uint8_t *pInLimit) {
    const uint8_t *const pStart = pIn;

    while (pIn < pInLimit - 7) {
        uint64_t diff = lz4_read64(pIn) ^ lz4_read64(pMatch);
        if (!diff) {
            pIn += 8;
            pMatch += 8;
            continue;
        }
        return (unsigned int)(pIn - pStart) + ((unsigned int)__builtin_ctzll(diff) >> 3);
    }

    if ((pIn < (pInLimit - 3)) && (lz4_read32(pMatch) == lz4_read32(pIn))) {
        pIn += 4;
        pMatch += 4;
    }
    if ((pIn < (pInLimit - 1)) && (lz4_read16(pMatch) == lz4_read16(pIn))) {
        pIn += 2;
        pMatch += 2;
    }
    if ((pIn < pInLimit) && (*pMatch == *pIn)) {
        pIn++;
    }
    return (unsigned int)(pIn - pStart);
}

/* Fast 128-bit quadword zeroing of the 4 KB Scratchpad hash table */
static inline void lz4_clear_table_128(uint32_t *table) {
#ifdef _EE
    typedef uint32_t qword_u32 __attribute__((mode(TI), aligned(16)));
    qword_u32 zero = 0;
    qword_u32 *ptr = (qword_u32 *)table;
    for (int i = 0; i < (int)(LZ4_HASHTABLESIZE * sizeof(uint32_t)) / 64; i++) {
        ptr[0] = zero;
        ptr[1] = zero;
        ptr[2] = zero;
        ptr[3] = zero;
        ptr += 4;
    }
#else
    memset(table, 0, LZ4_HASHTABLESIZE * sizeof(uint32_t));
#endif
}

static volatile int s_spad_lock = 0;
static uint32_t s_fallback_hash_table[LZ4_HASHTABLESIZE] __attribute__((aligned(16)));

size_t athena_lz4_compress_bound(size_t input_size) {
    return ATHENA_LZ4_COMPRESS_BOUND(input_size);
}

int athena_lz4_compress(const void *src, void *dst, int src_size, int dst_capacity) {
    if (!src || !dst || src_size <= 0 || dst_capacity <= 0)
        return 0;
    if ((size_t)src_size > ATHENA_LZ4_MAX_INPUT_SIZE)
        return 0;

    const uint8_t *const ibase = (const uint8_t *)src;
    const uint8_t *ip = ibase;
    const uint8_t *anchor = ibase;
    const uint8_t *const iend = ibase + src_size;
    const uint8_t *const mflimit = iend - LZ4_MFLIMIT;
    const uint8_t *const matchlimit = iend - LZ4_LASTLITERALS;

    uint8_t *const obase = (uint8_t *)dst;
    uint8_t *op = obase;
    uint8_t *const olimit = obase + dst_capacity;

    /* Acquire Scratchpad 0x70003000 hash table if free; fallback if concurrent */
    uint32_t *hash_table;
    bool using_spad = false;
    int old_irq = DIntr();
    if (!s_spad_lock) {
        s_spad_lock = 1;
        using_spad = true;
    }
    if (old_irq)
        EIntr();

    if (using_spad) {
        hash_table = (uint32_t *)(uintptr_t)PS2_SPAD_HASH_ADDR;
    } else {
        hash_table = s_fallback_hash_table;
    }

    int result = 0;

    if (src_size < LZ4_MIN_LENGTH)
        goto _last_literals;

    lz4_clear_table_128(hash_table);

    /* First byte */
    hash_table[lz4_hash4(lz4_read32(ip))] = (uint32_t)(ip - ibase);
    ip++;
    uint32_t forwardH = lz4_hash4(lz4_read32(ip));

    for (;;) {
        const uint8_t *match;
        uint8_t *token;

        /* Find a 4-byte match */
        {
            const uint8_t *forwardIp = ip;
            unsigned int step = 1;
            unsigned int searchMatchNb = 1u << LZ4_SKIP_TRIGGER;

            do {
                uint32_t h = forwardH;
                ip = forwardIp;
                forwardIp += step;
                step = (searchMatchNb++ >> LZ4_SKIP_TRIGGER);

                if (__builtin_expect(forwardIp > mflimit, 0))
                    goto _last_literals;

                match = ibase + hash_table[h];
                forwardH = lz4_hash4(lz4_read32(forwardIp));
                hash_table[h] = (uint32_t)(ip - ibase);
            } while (((unsigned int)(ip - match) > LZ4_MAX_DISTANCE) ||
                     (lz4_read32(match) != lz4_read32(ip)));
        }

        /* Catch up backward if match extends earlier */
        while ((ip > anchor) && (match > ibase) && (__builtin_expect(ip[-1] == match[-1], 0))) {
            ip--;
            match--;
        }

        /* Encode literal length */
        {
            unsigned int litLength = (unsigned int)(ip - anchor);
            token = op++;
            if (__builtin_expect(op + litLength + (2 + 1 + LZ4_LASTLITERALS) + (litLength / 255) > olimit, 0))
                goto _cleanup;

            if (litLength >= 15) {
                unsigned int len = litLength - 15;
                *token = (15u << 4);
                while (len >= 255) {
                    *op++ = 255;
                    len -= 255;
                }
                *op++ = (uint8_t)len;
            } else {
                *token = (uint8_t)(litLength << 4);
            }

            /* Copy literals */
            if (litLength > 0) {
                lz4_wild_copy8(op, anchor, op + litLength);
                op += litLength;
            }
        }

_next_match:
        /* Encode 16-bit offset */
        lz4_write16(op, (uint16_t)(ip - match));
        op += 2;

        /* Encode match length */
        {
            unsigned int matchLength = lz4_count_match(ip + LZ4_MINMATCH, match + LZ4_MINMATCH, matchlimit);
            ip += LZ4_MINMATCH + matchLength;

            if (__builtin_expect(op + (1 + LZ4_LASTLITERALS) + (matchLength >> 8) > olimit, 0))
                goto _cleanup;

            if (matchLength >= 15) {
                *token += 15;
                matchLength -= 15;
                while (matchLength >= 510) {
                    *op++ = 255;
                    *op++ = 255;
                    matchLength -= 510;
                }
                if (matchLength >= 255) {
                    matchLength -= 255;
                    *op++ = 255;
                }
                *op++ = (uint8_t)matchLength;
            } else {
                *token += (uint8_t)matchLength;
            }
        }

        anchor = ip;

        /* Test end of chunk */
        if (ip > mflimit)
            break;

        /* Fill table with boundary position */
        hash_table[lz4_hash4(lz4_read32(ip - 2))] = (uint32_t)(ip - 2 - ibase);

        /* Test immediate next position */
        {
            uint32_t h = lz4_hash4(lz4_read32(ip));
            match = ibase + hash_table[h];
            hash_table[h] = (uint32_t)(ip - ibase);
            if (((unsigned int)(ip - match) <= LZ4_MAX_DISTANCE) &&
                (lz4_read32(match) == lz4_read32(ip))) {
                token = op++;
                *token = 0;
                goto _next_match;
            }
        }

        forwardH = lz4_hash4(lz4_read32(++ip));
    }

_last_literals:
    {
        size_t lastRun = (size_t)(iend - anchor);
        if (op + 1 + lastRun + ((lastRun + 255 - 15) / 255) > olimit)
            goto _cleanup;

        if (lastRun >= 15) {
            size_t accumulator = lastRun - 15;
            *op++ = (15u << 4);
            while (accumulator >= 255) {
                *op++ = 255;
                accumulator -= 255;
            }
            *op++ = (uint8_t)accumulator;
        } else {
            *op++ = (uint8_t)(lastRun << 4);
        }
        memcpy(op, anchor, lastRun);
        op += lastRun;
    }

    result = (int)(op - obase);

_cleanup:
    if (using_spad)
        s_spad_lock = 0;
    return result;
}

int athena_lz4_decompress(const void *src, void *dst, int compressed_size, int dst_capacity) {
    if (!src || !dst || compressed_size <= 0 || dst_capacity < 0)
        return -1;

    const uint8_t *ip = (const uint8_t *)src;
    const uint8_t *const iend = ip + compressed_size;

    uint8_t *op = (uint8_t *)dst;
    uint8_t *const oend = op + dst_capacity;
    uint8_t *cpy;

    static const int dec32table[8] = { 0, 1, 2, 1, 0, 4, 4, 4 };
    static const int dec64table[8] = { 0, 0, 0, -1, 0, 1, 2, 3 };

    for (;;) {
        if (__builtin_expect(ip >= iend, 0))
            return -1;

        unsigned int token = *ip++;
        size_t length = token >> 4;

        /* Decode literal length */
        if (length == 15) {
            unsigned int s;
            do {
                if (__builtin_expect(ip >= iend, 0))
                    return -1;
                s = *ip++;
                length += s;
            } while (s == 255);
        }

        /* Copy literals */
        cpy = op + length;
        if ((cpy > oend - LZ4_MFLIMIT) || (ip + length > iend - (2 + 1 + LZ4_LASTLITERALS))) {
            if (cpy > oend)
                return -1;
            if (ip + length != iend)
                return -1;
            memcpy(op, ip, length);
            ip += length;
            op += length;
            break;
        }

        lz4_wild_copy16(op, ip, cpy);
        ip += length;
        op = cpy;

        /* Get 16-bit match offset */
        unsigned int offset = lz4_read16(ip);
        ip += 2;
        const uint8_t *match = op - offset;
        if (__builtin_expect(offset == 0 || match < (const uint8_t *)dst, 0))
            return -1;

        /* Get match length */
        length = token & 15;
        if (length == 15) {
            unsigned int s;
            do {
                if (__builtin_expect(ip > iend - LZ4_LASTLITERALS, 0))
                    return -1;
                s = *ip++;
                length += s;
            } while (s == 255);
        }
        length += LZ4_MINMATCH;

        cpy = op + length;
        if (__builtin_expect(cpy > oend, 0))
            return -1;

        /* Overlapping short-offset match (< 8 bytes distance) */
        if (__builtin_expect(offset < 8, 0)) {
            const int dec64 = dec64table[offset];
            op[0] = match[0];
            op[1] = match[1];
            op[2] = match[2];
            op[3] = match[3];
            match += dec32table[offset];
            lz4_write32(op + 4, lz4_read32(match));
            match -= dec64;
        } else {
            lz4_write64(op, lz4_read64(match));
            match += 8;
        }
        op += 8;

        if (cpy > oend - 12) {
            uint8_t *const oCopyLimit = oend - 8;
            if (op < oCopyLimit) {
                lz4_wild_copy8(op, match, oCopyLimit);
                match += oCopyLimit - op;
                op = oCopyLimit;
            }
            while (op < cpy)
                *op++ = *match++;
        } else {
            lz4_write64(op, lz4_read64(match));
            if (length > 16)
                lz4_wild_copy16(op + 8, match + 8, cpy);
        }
        op = cpy;
    }

    return (int)(op - (uint8_t *)dst);
}

/* =========================================================================
 * Part 2: Smart Memory Compactor Daemon (Textures + 3D Meshes + OOM Rescue)
 * ========================================================================= */

#define ATHENA_COMPRESS_SKIP     (1u << 7)  /* Asset proved incompressible; skip future attempts */
#define TRANSFER_REQ_BIT         0x80000000u
#define VRAM_NULL_BLOCK          0xFFFFFFFFu

#define MAX_TRACKED_TEXTURES     512
#define MAX_TRACKED_MESHES       256
#define SCRATCH_BUFFER_SIZE      (64 * 1024) /* 64 KB static BSS staging buffer (zero heap frag) */

static GSSURFACE          *s_textures[MAX_TRACKED_TEXTURES];
static uint32_t            s_texture_count = 0;
static uint32_t            s_tex_cursor = 0;

static athena_render_data *s_meshes[MAX_TRACKED_MESHES];
static uint32_t            s_mesh_count = 0;
static uint32_t            s_mesh_cursor = 0;

static uint8_t             s_compress_scratch[SCRATCH_BUFFER_SIZE] __attribute__((aligned(16)));
static volatile int        s_in_compactor = 0;
static uint32_t            s_frame_counter = 1;
static bool                s_initialized = false;

static AthenaMemCompressConfig s_config = {
    .enabled             = true,
    .frame_budget_us     = 350, /* 0.35 ms max per frame (~2% of a 16.6 ms 60fps frame) */
    .texture_vram_frames = 15,  /* 15 frames after VRAM upload, EE RAM copy can be compressed */
    .texture_idle_frames = 60,  /* 60 frames (~1 sec) idle for non-VRAM textures */
    .mesh_idle_frames    = 30   /* 30 frames (~0.5 sec) after VU1 compact cook */
};

static AthenaMemCompressStats s_stats = { 0 };

static inline uint32_t rd_ee_cycles(void) {
#ifdef _EE
    uint32_t c;
    __asm__ __volatile__("mfc0 %0, $9" : "=r"(c));
    return c;
#else
    return 0;
#endif
}

void athena_mem_compress_init(void) {
    if (s_initialized)
        return;
    memset(s_textures, 0, sizeof(s_textures));
    memset(s_meshes, 0, sizeof(s_meshes));
    s_texture_count = 0;
    s_mesh_count = 0;
    s_tex_cursor = 0;
    s_mesh_cursor = 0;
    s_frame_counter = 1;
    s_stats.enabled = s_config.enabled;
    s_stats.frame_budget_us = s_config.frame_budget_us;
    s_initialized = true;
}

uint32_t athena_mem_compress_frame(void) {
    return s_frame_counter;
}

void athena_mem_compress_get_config(AthenaMemCompressConfig *out_cfg) {
    if (!out_cfg)
        return;
    *out_cfg = s_config;
}

void athena_mem_compress_set_config(const AthenaMemCompressConfig *cfg) {
    if (!cfg)
        return;
    s_config = *cfg;
    s_stats.enabled = s_config.enabled;
    s_stats.frame_budget_us = s_config.frame_budget_us;
}

void athena_mem_compress_get_stats(AthenaMemCompressStats *out_stats) {
    if (!out_stats)
        return;
    s_stats.enabled = s_config.enabled;
    s_stats.tracked_textures = s_texture_count;
    s_stats.tracked_meshes = s_mesh_count;
    s_stats.frame_budget_us = s_config.frame_budget_us;
    s_stats.saved_bytes = (s_stats.uncompressed_bytes > s_stats.compressed_bytes)
                        ? (s_stats.uncompressed_bytes - s_stats.compressed_bytes)
                        : 0;
    *out_stats = s_stats;
}

/* -------------------------------------------------------------------------
 * Core Buffer Compress Helper (Zero-Fragmentation Staging + Fallback)
 * ------------------------------------------------------------------------- */
static void *compress_buffer_replace(void *orig_ptr, uint32_t orig_size, uint32_t *out_comp_size) {
    if (!orig_ptr || orig_size < 2048)
        return NULL;

    /* Require at least 12.5% reduction to be worthwhile */
    int max_acceptable = (int)(orig_size - (orig_size >> 3));
    if (max_acceptable <= 64)
        return NULL;

    /* Fast path: if max_acceptable fits in our 64 KB static scratch buffer,
     * compress into static BSS first, free the large original buffer FIRST,
     * and then allocate the exact small compressed buffer (OOM-proof!). */
    if (max_acceptable <= SCRATCH_BUFFER_SIZE) {
        int csize = athena_lz4_compress(orig_ptr, s_compress_scratch, (int)orig_size, max_acceptable);
        if (csize <= 0 || (uint32_t)csize >= (uint32_t)max_acceptable)
            return NULL;

        /* Free original buffer BEFORE allocating compressed block so heap is never tighter */
        free(orig_ptr);

        void *cblock = malloc((size_t)csize);
        if (!cblock)
            return NULL;
        memcpy(cblock, s_compress_scratch, (size_t)csize);
        *out_comp_size = (uint32_t)csize;
        return cblock;
    }

    /* Large buffer (> 64 KB compressed target): try compressing into 64 KB scratch first!
     * Even a 256 KB - 512 KB texture or mesh often compresses to < 64 KB with LZ4! */
    int csize = athena_lz4_compress(orig_ptr, s_compress_scratch, (int)orig_size, SCRATCH_BUFFER_SIZE);
    if (csize > 0) {
        free(orig_ptr);
        void *cblock = malloc((size_t)csize);
        if (!cblock)
            return NULL;
        memcpy(cblock, s_compress_scratch, (size_t)csize);
        *out_comp_size = (uint32_t)csize;
        return cblock;
    }

    /* Fallback for huge payloads whose compressed output exceeds 64 KB */
    void *temp = malloc((size_t)max_acceptable);
    if (!temp)
        return NULL;

    csize = athena_lz4_compress(orig_ptr, temp, (int)orig_size, max_acceptable);
    if (csize <= 0 || csize >= max_acceptable) {
        free(temp);
        return NULL;
    }

    /* Release original huge buffer first, then shrink temp */
    free(orig_ptr);
    void *shrunk = realloc(temp, (size_t)csize);
    *out_comp_size = (uint32_t)csize;
    return shrunk ? shrunk : temp;
}

/* -------------------------------------------------------------------------
 * Texture Compaction Lifecycle
 * ------------------------------------------------------------------------- */

void athena_mem_register_texture(GSSURFACE *tex) {
    if (!tex || !tex->Mem)
        return;
    if (tex->PageAligned || tex->Macroblock)
        return;

    if (!s_initialized)
        athena_mem_compress_init();

    uint32_t byte_size = athena_surface_size(tex->Width, tex->Height, tex->PSM);
    if (byte_size < 2048)
        return;

    tex->CompressedMem = NULL;
    tex->CompressedSize = 0;
    tex->UncompressedSize = byte_size;
    tex->LastBoundFrame = s_frame_counter;
    tex->LastUploadFrame = s_frame_counter;
    tex->CompressFlags = ATHENA_TEX_COMPRESSIBLE;

    for (uint32_t i = 0; i < s_texture_count; i++) {
        if (s_textures[i] == tex)
            return;
    }
    if (s_texture_count < MAX_TRACKED_TEXTURES) {
        s_textures[s_texture_count++] = tex;
    }
}

void athena_mem_unregister_texture(GSSURFACE *tex) {
    if (!tex)
        return;

    int32_t found_idx = -1;
    for (uint32_t i = 0; i < s_texture_count; i++) {
        if (s_textures[i] == tex) {
            found_idx = (int32_t)i;
            break;
        }
    }
    if (found_idx < 0)
        return;

    if (tex->CompressFlags & ATHENA_TEX_COMPRESSED) {
        if (s_stats.compressed_bytes >= tex->CompressedSize)
            s_stats.compressed_bytes -= tex->CompressedSize;
        if (s_stats.uncompressed_bytes >= tex->UncompressedSize)
            s_stats.uncompressed_bytes -= tex->UncompressedSize;
        if (s_stats.compressed_textures > 0)
            s_stats.compressed_textures--;
    }

    if (tex->CompressedMem) {
        free(tex->CompressedMem);
        tex->CompressedMem = NULL;
    }
    tex->CompressedSize = 0;
    tex->CompressFlags = 0;

    s_textures[found_idx] = s_textures[--s_texture_count];
    s_textures[s_texture_count] = NULL;
}

void athena_texture_mark_bound(GSSURFACE *tex) {
    if (tex)
        tex->LastBoundFrame = s_frame_counter;
}

void athena_texture_mark_uploaded(GSSURFACE *tex) {
    if (tex) {
        tex->LastBoundFrame = s_frame_counter;
        tex->LastUploadFrame = s_frame_counter;
    }
}

bool athena_texture_ensure_uncompressed(GSSURFACE *tex) {
    if (!tex)
        return false;
    tex->LastBoundFrame = s_frame_counter;

    if (!(tex->CompressFlags & ATHENA_TEX_COMPRESSED))
        return (tex->Mem != NULL);

    if (!tex->CompressedMem || tex->UncompressedSize == 0)
        return false;

    s_in_compactor++;
    uint32_t *raw = (uint32_t *)memalign(128, tex->UncompressedSize);
    if (!raw) {
        /* If heap is full when trying to decompress a texture for VRAM upload,
         * compact other cold assets to make room! */
        s_in_compactor--;
        athena_mem_compact_now(tex->UncompressedSize + 4096);
        s_in_compactor++;
        raw = (uint32_t *)memalign(128, tex->UncompressedSize);
    }

    if (!raw) {
        s_in_compactor--;
        return false;
    }

    int dec = athena_lz4_decompress(tex->CompressedMem, raw,
                                    (int)tex->CompressedSize, (int)tex->UncompressedSize);
    if (dec != (int)tex->UncompressedSize) {
        free(raw);
        s_in_compactor--;
        return false;
    }

    SyncDCache(raw, (uint8_t *)raw + tex->UncompressedSize - 1);

    free(tex->CompressedMem);
    tex->CompressedMem = NULL;
    tex->Mem = raw;
    tex->CompressFlags &= (uint8_t)~ATHENA_TEX_COMPRESSED;
    tex->LastUploadFrame = s_frame_counter;

    if (s_stats.compressed_bytes >= tex->CompressedSize)
        s_stats.compressed_bytes -= tex->CompressedSize;
    if (s_stats.uncompressed_bytes >= tex->UncompressedSize)
        s_stats.uncompressed_bytes -= tex->UncompressedSize;
    if (s_stats.compressed_textures > 0)
        s_stats.compressed_textures--;
    tex->CompressedSize = 0;
    s_stats.decompress_count++;

    s_in_compactor--;
    return true;
}

void athena_texture_pin(GSSURFACE *tex) {
    if (!tex)
        return;
    athena_texture_ensure_uncompressed(tex);
    tex->CompressFlags |= ATHENA_TEX_PINNED;
}

void athena_texture_unpin(GSSURFACE *tex) {
    if (!tex)
        return;
    tex->CompressFlags &= (uint8_t)~ATHENA_TEX_PINNED;
}

static size_t try_compress_texture(GSSURFACE *tex, bool emergency) {
    if (!tex || !tex->Mem)
        return 0;
    if (!(tex->CompressFlags & ATHENA_TEX_COMPRESSIBLE))
        return 0;
    if (tex->CompressFlags & (ATHENA_TEX_COMPRESSED | ATHENA_TEX_PINNED | ATHENA_COMPRESS_SKIP))
        return 0;
    if (tex->PageAligned || tex->Macroblock)
        return 0;
    /* Never touch a texture with a pending VIF1/GIF async upload */
    if (tex->Vram & TRANSFER_REQ_BIT)
        return 0;

    uint32_t since_upload = s_frame_counter - tex->LastUploadFrame;
    uint32_t since_bound  = s_frame_counter - tex->LastBoundFrame;

    /* Always wait at least 3 frames after any upload so overlapped GIF DMA is done */
    if (since_upload < (emergency ? 2u : 4u))
        return 0;

    if (!emergency) {
        bool in_vram = (tex->Vram != 0 && tex->Vram != VRAM_NULL_BLOCK);
        if (in_vram) {
            if (since_upload < s_config.texture_vram_frames)
                return 0;
        } else {
            if (since_bound < s_config.texture_idle_frames)
                return 0;
        }
    }

    uint32_t orig_size = athena_surface_size(tex->Width, tex->Height, tex->PSM);
    if (orig_size < 2048) {
        tex->CompressFlags |= ATHENA_COMPRESS_SKIP;
        return 0;
    }
    tex->UncompressedSize = orig_size;

    uint32_t comp_size = 0;
    void *cblock = compress_buffer_replace(tex->Mem, orig_size, &comp_size);
    if (!cblock) {
        tex->CompressFlags |= ATHENA_COMPRESS_SKIP;
        return 0;
    }

    tex->Mem = NULL;
    tex->CompressedMem = cblock;
    tex->CompressedSize = comp_size;
    tex->CompressFlags |= ATHENA_TEX_COMPRESSED;

    s_stats.compressed_bytes += comp_size;
    s_stats.uncompressed_bytes += orig_size;
    s_stats.compressed_textures++;
    s_stats.compress_count++;

    return (size_t)(orig_size - comp_size);
}

/* -------------------------------------------------------------------------
 * 3D Mesh Compaction Lifecycle (athena_render_data)
 * ------------------------------------------------------------------------- */

void athena_mem_register_mesh(athena_render_data *mesh) {
    if (!mesh || mesh->frozen)
        return;
    if (mesh->index_count * sizeof(VECTOR) < 2048)
        return;

    if (!s_initialized)
        athena_mem_compress_init();

    memset(mesh->compressed_attribs, 0, sizeof(mesh->compressed_attribs));
    memset(mesh->compressed_sizes, 0, sizeof(mesh->compressed_sizes));
    memset(mesh->uncompressed_sizes, 0, sizeof(mesh->uncompressed_sizes));
    mesh->last_cpu_access_frame = s_frame_counter;
    mesh->compress_flags = ATHENA_MESH_COMPRESSIBLE;

    for (uint32_t i = 0; i < s_mesh_count; i++) {
        if (s_meshes[i] == mesh)
            return;
    }
    if (s_mesh_count < MAX_TRACKED_MESHES) {
        s_meshes[s_mesh_count++] = mesh;
    }
}

void athena_mem_unregister_mesh(athena_render_data *mesh) {
    if (!mesh)
        return;

    int32_t found_idx = -1;
    for (uint32_t i = 0; i < s_mesh_count; i++) {
        if (s_meshes[i] == mesh) {
            found_idx = (int32_t)i;
            break;
        }
    }
    if (found_idx < 0)
        return;

    if (mesh->compress_flags & ATHENA_MESH_COMPRESSED) {
        for (int a = 0; a < 5; a++) {
            if (mesh->compressed_attribs[a]) {
                if (s_stats.compressed_bytes >= mesh->compressed_sizes[a])
                    s_stats.compressed_bytes -= mesh->compressed_sizes[a];
                if (s_stats.uncompressed_bytes >= mesh->uncompressed_sizes[a])
                    s_stats.uncompressed_bytes -= mesh->uncompressed_sizes[a];
            }
        }
        if (s_stats.compressed_meshes > 0)
            s_stats.compressed_meshes--;
    }

    for (int a = 0; a < 5; a++) {
        if (mesh->compressed_attribs[a]) {
            free(mesh->compressed_attribs[a]);
            mesh->compressed_attribs[a] = NULL;
        }
        mesh->compressed_sizes[a] = 0;
        mesh->uncompressed_sizes[a] = 0;
    }
    mesh->compress_flags = 0;

    s_meshes[found_idx] = s_meshes[--s_mesh_count];
    s_meshes[s_mesh_count] = NULL;
}

void athena_mesh_mark_cpu_access(athena_render_data *mesh) {
    if (mesh)
        mesh->last_cpu_access_frame = s_frame_counter;
}

bool athena_mesh_ensure_uncompressed(athena_render_data *mesh) {
    if (!mesh)
        return false;
    mesh->last_cpu_access_frame = s_frame_counter;

    if (!(mesh->compress_flags & ATHENA_MESH_COMPRESSED))
        return true;

    s_in_compactor++;
    void **targets[5] = {
        (void **)&mesh->positions,
        (void **)&mesh->normals,
        (void **)&mesh->texcoords,
        (void **)&mesh->colours,
        (void **)&mesh->skin_data
    };

    bool ok = true;
    for (int a = 0; a < 5; a++) {
        if (!mesh->compressed_attribs[a] || mesh->uncompressed_sizes[a] == 0)
            continue;

        uint32_t usize = mesh->uncompressed_sizes[a];
        uint32_t csize = mesh->compressed_sizes[a];

        void *raw = memalign(16, usize);
        if (!raw) {
            s_in_compactor--;
            athena_mem_compact_now(usize + 4096);
            s_in_compactor++;
            raw = memalign(16, usize);
        }
        if (!raw) {
            ok = false;
            break;
        }

        int dec = athena_lz4_decompress(mesh->compressed_attribs[a], raw, (int)csize, (int)usize);
        if (dec != (int)usize) {
            free(raw);
            ok = false;
            break;
        }

        free(mesh->compressed_attribs[a]);
        mesh->compressed_attribs[a] = NULL;
        mesh->compressed_sizes[a] = 0;
        mesh->uncompressed_sizes[a] = 0;
        *targets[a] = raw;

        if (s_stats.compressed_bytes >= csize)
            s_stats.compressed_bytes -= csize;
        if (s_stats.uncompressed_bytes >= usize)
            s_stats.uncompressed_bytes -= usize;
    }

    if (ok) {
        mesh->compress_flags &= (uint8_t)~ATHENA_MESH_COMPRESSED;
        if (s_stats.compressed_meshes > 0)
            s_stats.compressed_meshes--;
        s_stats.decompress_count++;
    }

    s_in_compactor--;
    return ok;
}

void athena_mesh_pin(athena_render_data *mesh) {
    if (!mesh)
        return;
    athena_mesh_ensure_uncompressed(mesh);
    mesh->compress_flags |= ATHENA_MESH_PINNED;
}

void athena_mesh_unpin(athena_render_data *mesh) {
    if (!mesh)
        return;
    mesh->compress_flags &= (uint8_t)~ATHENA_MESH_PINNED;
}

static size_t try_compress_mesh(athena_render_data *mesh, bool emergency) {
    if (!mesh || mesh->frozen || mesh->index_count == 0)
        return 0;
    if (!(mesh->compress_flags & ATHENA_MESH_COMPRESSIBLE))
        return 0;
    if (mesh->compress_flags & (ATHENA_MESH_COMPRESSED | ATHENA_MESH_PINNED | ATHENA_COMPRESS_SKIP))
        return 0;

    uint32_t since_access = s_frame_counter - mesh->last_cpu_access_frame;
    if (!emergency) {
        /* In background VBlank mode, only compress meshes whose VU1 compact buffers
         * have already been cooked and are clean, so ongoing 60fps rendering never
         * needs to decompress them! */
        if (!mesh->compact_positions || mesh->compact_dirty != 0)
            return 0;
        if (since_access < s_config.mesh_idle_frames)
            return 0;
    } else {
        if (since_access < 2u)
            return 0;
    }

    void **targets[5] = {
        (void **)&mesh->positions,
        (void **)&mesh->normals,
        (void **)&mesh->texcoords,
        (void **)&mesh->colours,
        (void **)&mesh->skin_data
    };
    uint32_t elem_sizes[5] = {
        (uint32_t)sizeof(VECTOR),
        (uint32_t)sizeof(VECTOR),
        (uint32_t)sizeof(VECTOR),
        (uint32_t)sizeof(VECTOR),
        (uint32_t)sizeof(vertex_skin_data)
    };

    size_t total_saved = 0;
    bool any_compressed = false;

    for (int a = 0; a < 5; a++) {
        void *ptr = *targets[a];
        if (!ptr)
            continue;

        uint32_t usize = mesh->index_count * elem_sizes[a];
        if (usize < 2048)
            continue;

        uint32_t csize = 0;
        void *cblock = compress_buffer_replace(ptr, usize, &csize);
        if (cblock) {
            *targets[a] = NULL;
            mesh->compressed_attribs[a] = cblock;
            mesh->compressed_sizes[a] = csize;
            mesh->uncompressed_sizes[a] = usize;

            s_stats.compressed_bytes += csize;
            s_stats.uncompressed_bytes += usize;
            total_saved += (size_t)(usize - csize);
            any_compressed = true;
        }
    }

    if (any_compressed) {
        mesh->compress_flags |= ATHENA_MESH_COMPRESSED;
        s_stats.compressed_meshes++;
        s_stats.compress_count++;
    } else {
        mesh->compress_flags |= ATHENA_COMPRESS_SKIP;
    }

    return total_saved;
}

/* -------------------------------------------------------------------------
 * Per-Frame VBlank Daemon & Emergency OOM Compaction
 * ------------------------------------------------------------------------- */

void athena_mem_compress_tick(void) {
    s_frame_counter++;

    if (!s_config.enabled || s_in_compactor)
        return;

    if (s_texture_count == 0 && s_mesh_count == 0)
        return;

    s_in_compactor++;

    /* 294.912 MHz R5900 -> ~295 cycles per microsecond */
    const uint32_t budget_cycles = s_config.frame_budget_us * 295u;
    const uint32_t start_cycles = rd_ee_cycles();

    /* Scan up to 8 textures and 4 meshes per frame in round-robin order */
    uint32_t tex_checks = s_texture_count < 8 ? s_texture_count : 8;
    for (uint32_t i = 0; i < tex_checks; i++) {
        if (budget_cycles > 0 && (uint32_t)(rd_ee_cycles() - start_cycles) >= budget_cycles)
            break;
        if (s_tex_cursor >= s_texture_count)
            s_tex_cursor = 0;
        try_compress_texture(s_textures[s_tex_cursor++], false);
    }

    uint32_t mesh_checks = s_mesh_count < 4 ? s_mesh_count : 4;
    for (uint32_t i = 0; i < mesh_checks; i++) {
        if (budget_cycles > 0 && (uint32_t)(rd_ee_cycles() - start_cycles) >= budget_cycles)
            break;
        if (s_mesh_cursor >= s_mesh_count)
            s_mesh_cursor = 0;
        try_compress_mesh(s_meshes[s_mesh_cursor++], false);
    }

    s_in_compactor--;
}

size_t athena_mem_compact_now(size_t target_bytes_to_free) {
    if (s_in_compactor)
        return 0;

    s_in_compactor++;
    size_t freed = 0;

    for (uint32_t i = 0; i < s_texture_count; i++) {
        freed += try_compress_texture(s_textures[i], true);
        if (target_bytes_to_free > 0 && freed >= target_bytes_to_free)
            goto _done;
    }

    for (uint32_t i = 0; i < s_mesh_count; i++) {
        freed += try_compress_mesh(s_meshes[i], true);
        if (target_bytes_to_free > 0 && freed >= target_bytes_to_free)
            goto _done;
    }

_done:
    if (freed > 0 && target_bytes_to_free > 0)
        s_stats.oom_rescues++;

    s_in_compactor--;
    return freed;
}
