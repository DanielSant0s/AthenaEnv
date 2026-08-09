#ifndef ATHENA_RENDER_H
#define ATHENA_RENDER_H

#include <graphics.h>
#include <owl_packet.h>

//3D math

typedef struct {
	VECTOR position;
	VECTOR target;
	VECTOR up;
	VECTOR local_up;
} athena_camera_state;

typedef struct {
	float    x;
	float    y;
	float    z;
	uint32_t w;
} FIVECTOR;

typedef union {
    struct {
	    float    x;
	    float    y;
	    float    z;
	    float    w;
    };

    struct {
	    uint32_t    ix;
	    uint32_t    iy;
	    uint32_t    iz;
	    uint32_t    iw;
    };

    float    f[4];
    uint32_t u[4];
} VUVECTOR;

typedef struct {
	VECTOR direction[4];
	FIVECTOR ambient[4];
	VECTOR diffuse[4];
	VECTOR specular[4];
} LightData;

#define CULL_FACE_NONE 0.0f
#define CULL_FACE_FRONT -1.0f
#define CULL_FACE_BACK 1.0f

typedef enum {
	ATHENA_LIGHT_DIRECTION,
	ATHENA_LIGHT_AMBIENT,
	ATHENA_LIGHT_DIFFUSE,
	ATHENA_LIGHT_SPECULAR,
} eLightAttributes;

typedef enum {
	PL_NO_LIGHTS,
	PL_DEFAULT,
	PL_SPECULAR,
} eRenderPipelines;

typedef enum {
	SHADE_FLAT,
	SHADE_GOURAUD,
	SHADE_BLINN_PHONG,
} eRenderShadeModels;

typedef struct { 
	int accurate_clipping; 
	float face_culling; 
	int texture_mapping;
	int shade_model; // 0 = flat, 1 = gouraud
    int has_refmap; 
    int has_bumpmap; 
    int has_decal;
} RenderAttributes;


typedef struct athena_keyframe {
    float time;
    VECTOR position;
    VECTOR rotation;
    VECTOR scale;
} athena_keyframe;

typedef struct athena_bone_animation {
    uint32_t bone_id;
    athena_keyframe* position_keys;
    athena_keyframe* rotation_keys;
    athena_keyframe* scale_keys;
    uint32_t position_key_count;
    uint32_t rotation_key_count;
    uint32_t scale_key_count;
} athena_bone_animation;

typedef struct athena_animation {
    char name[64];
    float duration;
    float ticks_per_second;
    athena_bone_animation* bone_animations;
    uint32_t bone_animation_count;
} athena_animation;

typedef struct athena_animation_collection {
    athena_animation* animations;
    uint32_t count;
} athena_animation_collection;

typedef struct athena_animation_controller {
    athena_animation* current;

	float initial_time;
    float current_time;
    bool is_playing;
    bool loop;
} athena_animation_controller;

typedef struct athena_bone_data {
    char name[64];
    int32_t parent_id;

    MATRIX inverse_bind;   
    
    VECTOR position;
    VECTOR rotation;
    VECTOR scale;
} athena_bone_data;

typedef struct athena_bone_transform {
    MATRIX transform;
    
    VECTOR position;
    VECTOR rotation;
    VECTOR scale;
} athena_bone_transform;

typedef struct athena_skeleton {
    athena_bone_data* bones;
    uint32_t bone_count;
} athena_skeleton;

typedef struct vertex_skin_data {
    uint32_t bone_indices[4];  
    float bone_weights[4];    
} vertex_skin_data;

typedef struct
{
    VECTOR ambient; 
    VECTOR diffuse;
    VECTOR specular;  
    VECTOR emission;  
    VECTOR transmittance; 
    float  shininess;  
    float  refraction; 
    VECTOR transmission_filter; 
    float  disolve;  

    float bump_scale; 

	int texture_id;
    int bump_texture_id;
    int decal_texture_id;
    int ref_texture_id;
} ath_mat;

typedef struct {
	uint32_t index;
	uint32_t end;
} material_index;

// Compact wire formats fed to the VU1 via VIF unpack (V3_32/V4_8/V2_16).
// These are derived caches cooked from positions/normals/colours/texcoords
// every render, never a source of truth -- see render_cook_compact_vertices().
typedef struct {
	float x, y, z;
} athena_compact_position; // V3_32, 12 bytes

typedef struct {
	int8_t x, y, z, w;
} athena_compact_normal; // V4_8 signed, 4 bytes

typedef struct {
	uint8_t r, g, b, a;
} athena_compact_color; // V4_8 unsigned, 4 bytes

typedef struct {
	int16_t u, v;
} athena_compact_uv; // V2_16 signed, 4 bytes

// Per-attribute bits for athena_render_data.compact_dirty -- see its doc
// comment and render_invalidate_compact_cache()/_positions().
#define RENDER_DIRTY_POSITIONS (1 << 0)
#define RENDER_DIRTY_NORMALS   (1 << 1)
#define RENDER_DIRTY_COLORS    (1 << 2)
#define RENDER_DIRTY_UVS       (1 << 3)
#define RENDER_DIRTY_ALL       (RENDER_DIRTY_POSITIONS | RENDER_DIRTY_NORMALS | RENDER_DIRTY_COLORS | RENDER_DIRTY_UVS)

// Pre-baked DMA_CALL sub-chain for draw_vu1_with_colors: the per-chunk parts
// that are static across frames. All DMA_REF, so live JS mutation still works.
// bake_giftags stays dynamic -- it embeds PrimContext, which flips per frame.
typedef struct {
    owl_qword *buffer;       // NULL until first built
    uint32_t   qwc_alloc;    // buffer size, in quadwords
    uint32_t  *chunk_offset; // per-chunk entry point, in quadwords into buffer
    uint32_t   chunk_count;
    int        mpg_addr;     // mpg_addr baked into this chain's MSCALF/MSCNT trailers
    uint32_t   built_version; // athena_render_data.chain_version at last build; see below


    // GS TEX0/TEX1 on PATH1 instead of a PATH2 DIRECT. 3 QW per
    // material_indices entry: AD giftag header + TEX0 + TEX1, EOP=1. The VU
    // XGKICKs it standalone BEFORE the vertex data -- chaining it onto the
    // vertex kick would let the accurate-clipping path's mid-loop XGKICKs
    // (which carry no texture state) draw with the previous chunk's texture.
    // Header baked once; TEX0/TEX1 refreshed per draw (PrimContext flips).
    owl_qword *tex_giftag;

    // owl_flush_generation() of the last draw call that queued this chain. Both
    // buffers above reach the DMAC by reference, so they belong to that draw
    // until it has been read -- rewriting them earlier (a second draw of the
    // same object with a different texture, or a rebuild triggered mid-frame)
    // would retroactively change what the first draw renders with.
    uint32_t queued_gen;
    int      has_queued;
} athena_chain_cache;

typedef struct athena_render_data {
    uint32_t index_count;

    uint32_t *indices;

    VECTOR* positions;
	VECTOR* texcoords;
	VECTOR* normals;
    VECTOR* colours;

    // Lazily (re)allocated by render_cook_compact_vertices(); freed/reset by
    // new_render_object(). Sized padded_total+4 to keep DMA_REF QWC rounding
    // (owl_add_unpack_data_ref_packed) from ever reading past the allocation.
    // Indexed via compact_group_base, NOT by source vertex number.
    athena_compact_position* compact_positions;
    athena_compact_normal*   compact_normals;
    athena_compact_color*    compact_colors;
    athena_compact_uv*       compact_uvs;
    uint32_t compact_capacity;

    // Start of each material_indices[] group inside compact_*, in elements,
    // rounded up to a multiple of 4. The DMAC only fetches from quadword-aligned
    // addresses, and with the compact formats (12B position, 4B normal/colour/uv)
    // that means index % 4 == 0; a group starting elsewhere is read up to 8 bytes
    // early and its whole slice slides one lane. Was free when a 16B VECTOR made
    // every index aligned. skin_data is exempt: 32B, always aligned.
    uint32_t* compact_group_base;
    uint32_t  compact_group_count;
    // Cooked on first draw and after render_invalidate_compact_*(), not every
    // draw: skinned meshes animate via bone matrices, not this data. Bits are
    // per-attribute (RENDER_DIRTY_*) because callers that do touch something
    // every frame usually touch only one (shadows.c rewrites positions only).
    uint8_t compact_dirty;
    // render_freeze_compact_vertices() freed the float source (~64 B/vertex),
    // leaving only the compact buffers. Reassigning positions un-freezes.
    // A frozen mesh cannot be used with ODE trimesh or shadows.c's grid.
    bool frozen;

    // [pipeline][pass_state]. A dedicated slot per combination, never shared:
    // render_object() issues base/bump/decal back-to-back into the same
    // unflushed ring, and a script can flip data->pipeline between passes of the
    // same frame (shadows.js does). Rebuilding a slot in place while a queued
    // DMA_CALL still points at it desyncs the VIF stream.
    // Rebuilt when built_version != chain_version or mpg_addr goes stale.
    athena_chain_cache chain[3][3];

    // The reflection pass runs IN ADDITION to the pipeline above, so it cannot
    // share those slots. Always pass_state 0.
    athena_chain_cache ref_chain;

    uint32_t chain_version;

    vertex_skin_data* skin_data;
	athena_skeleton* skeleton;

    VECTOR bounding_box[8];

    eRenderPipelines pipeline;

    GSSURFACE** textures;
	int texture_count;

	ath_mat *materials;
	uint32_t material_count;

	material_index *material_indices;
	int material_index_count;

	RenderAttributes attributes;

	bool tristrip;
} athena_render_data;

typedef struct athena_object_data {
	MATRIX transform;

    VECTOR position;
	VECTOR rotation;
    VECTOR scale;

    athena_animation_controller anim_controller;
    athena_bone_transform *bones;
    MATRIX *bone_matrices;

    // Frustum-cull bounds for skinned meshes, rebuilt from the live bone
    // palette every render_object() -- see render_update_skinned_bounds().
    // NULL (and unused) for everything else, which is culled straight against
    // data->bounding_box. Allocated alongside bones/bone_matrices.
    VECTOR *skinned_bounds;

    VECTOR *bump_offset_buffer;
    VECTOR bump_offset;

    athena_render_data *data;

    // Defaults true. Turn off when drawn geometry can escape the bounds --
    // i.e. a script mutating shareBuffers positions without reassigning
    // .bounds. Skeletal animation is already handled via skinned_bounds.
    bool frustum_cull;

    void *collision;
    void (*update_collision)(struct athena_object_data *obj);

    void *physics;
    void (*update_physics)(struct athena_object_data *obj);

} athena_object_data qw_aligned;

void updateGeomPosRot(athena_object_data *obj);

void updateBodyPosRot(athena_object_data *obj);

void initCamera(MATRIX *ws, MATRIX *wv, MATRIX *vs);

void cameraUpdate();

#define BATCH_SIZE 48
// Must match the VU mem stride baked into mem_layout.i's SKINNED_*_OFFSET
// constants (62, 92, 122, 152 -- each exactly 30 apart) -- render.c's
// destination-offset expressions (2+batch_size*N) depend on this equality.
// Do NOT change this to "fix" the rounding in owl_add_unpack_data_ref_packed;
// use render_compact_chunk_cap() instead, which floors it to a multiple of 4
// without touching what the offset formula sees.
#define BATCH_SIZE_SKINNED 40

// Mirrors mem_layout.i's INBUF_SIZE/SKINNED_INBUF_SIZE (VU1 asm has no
// C-visible constants). The tex_giftag unpack targets this offset, which is
// where the .vcl's texGiftagAddr lands. Keep in lockstep with mem_layout.i.
#define RENDER_INBUF_SIZE 194
#define RENDER_SKINNED_INBUF_SIZE 242

int clip_bounding_box(MATRIX local_clip, VECTOR *bounding_box);
void calculate_vertices_clipped(VECTOR *output,  int count, VECTOR *vertices, MATRIX local_screen);
int draw_convert_xyz(xyz_t *output, float x, float y, int z, int count, vertex_f_t *vertices);

unsigned int get_max_z(GSCONTEXT* gsGlobal);

void athena_set_tw_th(const GSSURFACE *Texture, int *tw, int *th);

void athena_line_goraud_3d(GSCONTEXT *gsGlobal, float x1, float y1, int iz1, float x2, float y2, int iz2, uint64_t color1, uint64_t color2);

void LookAtCameraMatrix(MATRIX m, VECTOR position, VECTOR target, VECTOR up);

void render_init();

void render_begin();

// Marks the VU1 static view block (screen_scale at 0, world_screen at 1..4,
// camera position at 9) stale, so the next draw re-uploads it. Call from any
// code that unpacks into VU1 static addresses 0..9 -- tile_render.c does.
void render_invalidate_vu_view(void);

void render_set_view(float fov, float near, float far, float width, float height);

VECTOR *getCameraPosition();
void    setCameraPosition(float x, float y, float z);
void    setCameraRotation(float x, float y, float z);

void setCameraTarget(float x, float y, float z);
void turnCamera(float yaw, float pitch);
void orbitCamera(float yaw, float pitch);
void dollyCamera(float dist);
void zoomCamera(float dist);
void panCamera(float x, float y);

int NewLight();
void SetLightAttribute(int id, float x, float y, float z, int attr);

void loadModel(athena_render_data* res_m, const char* path, GSSURFACE* text);
void draw_bbox(athena_object_data *obj, Color color);

void render_object(athena_object_data *obj);

// True when obj still has to be drawn: either culling is off for it, or its
// bounding box is not provably outside the view frustum. See the definition in
// render.c for what "provably" buys and what it deliberately does not.
int render_object_in_frustum(athena_object_data *obj);

// Recomputes data->bounding_box from data->positions (axis-aligned, in object
// space). Implemented in mesh_loaders.c, where the model loaders already call
// it; declared here because anything that replaces positions after load has to
// call it too or frustum culling will test a stale box.
void calculate_bbox(athena_render_data *data);

void new_render_object(athena_object_data *obj, athena_render_data *data);

// Forces the next draw call to re-cook every compact VIF wire buffer from
// positions/normals/colours/texcoords instead of reusing the cached ones.
// Only needed after a script mutates those arrays in place post-load.
void render_invalidate_compact_cache(athena_render_data *data);

// Like render_invalidate_compact_cache(), but only forces positions to be
// re-cooked, leaving the (still valid) cached normals/colours/texcoords
// alone -- for callers that only ever touch positions per update, e.g.
// shadows.c's projector grid.
void render_invalidate_compact_positions(athena_render_data *data);

// Cooks the compact buffers if needed, then frees positions/normals/
// texcoords/colours and marks data as frozen -- see the `frozen` field doc
// in athena_render_data. A no-op if already frozen.
void render_freeze_compact_vertices(athena_render_data *data);

// Bumps chain_version, so every pass_state slot rebuilds on its next draw.
// Needed after changing materials/material_indices/texture_mapping/vertices,
// or reallocating the compact_* buffers the chains' DMA_REFs point at.
void render_invalidate_chain_cache(athena_render_data *data);

void update_object_space(athena_object_data *obj);

void create_view(MATRIX view_screen, float fov, float near, float far, float w, float h);

// memalign, not malloc: VECTOR is declared aligned(16) but malloc only promises
// 8, and the cook loops (render.c) read these arrays with lqc2, which faults on
// a misaligned address. free() releases memalign'd blocks fine.
#define alloc_vectors(cnt) (VECTOR*)memalign(16, cnt * sizeof(VECTOR))
#define copy_vectors(dst, src, cnt) memcpy(dst, src, cnt*sizeof(VECTOR))
#define copy_vector(dst, src) memcpy(dst, src, sizeof(VECTOR))
#define free_vectors(vec) free(vec)

// copy_vector() has to stay a memcpy: mesh_loaders.c feeds it a source with a
// 12-byte stride on purpose. Use this one only where both sides came from
// alloc_vectors -- 2 memory ops instead of the 8 an unaligned memcpy expands to.
static inline void copy_vector_qw(VECTOR dst, VECTOR src) {
	__asm__ __volatile__(
		"lq  $8, 0x0(%1)\n"
		"sq  $8, 0x0(%0)\n"
		: : "r" (dst), "r" (src) : "$8", "memory");
}

#define copy_init_w_vector(dst, src) \
do { \
	dst[0] = src[0]; \
	dst[1] = src[1]; \
	dst[2] = src[2]; \
	dst[3] = 1.0f;  \
} while (0)

#define init_vector(vec) do { vec[0] = 1.0f; vec[1] = 1.0f; vec[2] = 1.0f; vec[3] = 1.0f; } while (0)

#define append_texture(m, tex) \
do { \
	m->texture_count++; \
	m->textures = realloc(m->textures, sizeof(GSSURFACE*)*m->texture_count); \
	m->textures[m->texture_count-1] = tex; \
} while (0)

void slerp_quaternion(VECTOR result, VECTOR q1, VECTOR q2, float t);
void find_keyframe_indices(athena_keyframe* keys, uint32_t key_count, float time, 
                          uint32_t* prev_idx, uint32_t* next_idx, float* t);

void apply_animation(athena_object_data* obj, float anim_time);					  

void update_bone_transforms(athena_object_data* obj);

void create_transform_matrix(MATRIX result, const VECTOR position, 
                           const VECTOR rotation, const VECTOR scale);

void decompose_transform_matrix(const MATRIX matrix, VECTOR position, 
                              VECTOR rotation, VECTOR scale);

void append_texture_tags(owl_packet* packet, GSSURFACE *texture, int texture_id, eColorFunctions func);

typedef struct {
	uint32_t draw_calls;
	uint32_t triangles;
	// Objects render_object() rejected via render_object_in_frustum() this
	// frame. draw_calls/triangles count only what actually got submitted, so
	// these three together tell you how much the cull is really saving.
	uint32_t objects_culled;
} render_stats_t;

const render_stats_t *render_get_stats(void);
void render_reset_stats(void);

#endif
