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

// Pre-baked DMA_CALL sub-chain cache for draw_vu1_with_colors -- see
// render_build_colors_chain(). Holds only the part of the per-chunk chain
// that's genuinely static across frames (diffuse/skin_data/positions/
// colours/uvs unpacks, all DMA_REF -- i.e. they only reference a pointer,
// never copy data, so live JS mutation keeps working with zero extra code
// -- plus the FLUSHA/NOP/ITOP/MSCALF-or-MSCNT trailer). Texture tags and
// bake_giftags() are NOT included: they embed the live GS VRAM address
// (subject to eviction) and gsGlobal->PrimContext (flips every frame), so
// they stay dynamic in the main packet stream, same as before.
typedef struct {
    owl_qword *buffer;       // NULL until first built
    uint32_t   qwc_alloc;    // buffer size, in quadwords
    uint32_t  *chunk_offset; // per-chunk entry point, in quadwords into buffer
    uint32_t   chunk_count;
    int        mpg_addr;     // mpg_addr baked into this chain's MSCALF/MSCNT trailers
    uint32_t   built_version; // athena_render_data.chain_version at last build; see below
} athena_chain_cache;

typedef struct athena_render_data {
    uint32_t index_count;

    uint32_t *indices;

    VECTOR* positions;
	VECTOR* texcoords;
	VECTOR* normals;
    VECTOR* colours;

    // Lazily (re)allocated by render_cook_compact_vertices(); freed/reset by
    // new_render_object(). Sized index_count+4 to keep DMA_REF QWC rounding
    // (owl_add_unpack_data_ref_packed) from ever reading past the allocation.
    athena_compact_position* compact_positions;
    athena_compact_normal*   compact_normals;
    athena_compact_color*    compact_colors;
    athena_compact_uv*       compact_uvs;
    uint32_t compact_capacity;
    // Cooked exactly once (on first draw, or after render_invalidate_compact_*())
    // rather than every draw call: positions/normals/colours/texcoords don't
    // change frame-to-frame for the overwhelming majority of meshes (skinned
    // meshes animate via bone matrices in the VU, not by touching this data),
    // so re-cooking unconditionally on every render_object() call was pure
    // waste -- doubly so for objects rendered more than once per frame (e.g.
    // a shadow pass + a main pass). render_invalidate_compact_cache() is
    // called automatically wherever positions/normals/colours/texcoords get
    // reassigned (e.g. the JS "vertices" setter) -- nothing JS-facing needs
    // to call it explicitly.
    //
    // Per-attribute bits (RENDER_DIRTY_*), not one whole-object flag: some
    // callers only ever touch one attribute every frame (e.g. shadows.c's
    // projector grid, which recomputes positions every render() call but
    // never touches colours/texcoords after the initial grid build) --
    // treating the other three as dirty too would re-cook 4x the data for
    // no reason, every single frame.
    uint8_t compact_dirty;
    // Set by render_freeze_compact_vertices(): once true, positions/normals/
    // texcoords/colours have been freed (set NULL) and only the compact
    // buffers above remain, reclaiming ~64 bytes/vertex for meshes that are
    // done being edited. Reassigning positions et al. (the JS "vertices"
    // setter) un-freezes automatically. A frozen mesh can no longer be used
    // with anything that needs the float source directly (e.g. ODE trimesh
    // collision, or shadows.c's procedural grid rewrite) -- that's the
    // caller's responsibility to avoid, same as any other bake/finalize step.
    bool frozen;

    // Pre-baked DMA_CALL chains for draw_vu1_with_colors -- see
    // athena_chain_cache's doc comment. One slot PER pass_state (0 = base,
    // 1 = bump, 2 = decal), not one shared slot: render_object() can invoke
    // draw_vu1_with_colors with different pass_state values back-to-back on
    // the SAME render_data, with no flush in between (base, then decal,
    // then bump twice, all appended to the same unflushed packet ring) --
    // and texture_mapping's "|| pass_state" term means the cached chain's
    // structure (which chunks include a UV unpack) differs between
    // pass_states. A single shared slot rebuilt in place for pass_state N+1
    // would overwrite the buffer a still-unflushed DMA_CALL from pass_state
    // N's draw was just pointed at -- by the time that queued DMA_CALL
    // actually reaches hardware, it jumps into a chain built for the wrong
    // pass_state's chunk structure, desyncing the VIF stream. Separate
    // per-pass_state slots mean rebuilding one never touches memory another
    // pass_state's in-flight DMA_CALL might still reference.
    //
    // Rebuilt whenever a slot's built_version doesn't match chain_version
    // below (bumped by render_invalidate_chain_cache() and by
    // render_cook_compact_vertices() growing the compact_* buffers, whose
    // addresses are baked into every slot's DMA_REF tags) or its cached
    // mpg_addr goes stale. Reset in new_render_object(), freed alongside
    // compact_*.
    athena_chain_cache colors_chain[3];
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

    VECTOR *bump_offset_buffer;
    VECTOR bump_offset;

    athena_render_data *data;

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
#define BATCH_SIZE_SKINNED 30

int clip_bounding_box(MATRIX local_clip, VECTOR *bounding_box);
void calculate_vertices_clipped(VECTOR *output,  int count, VECTOR *vertices, MATRIX local_screen);
int draw_convert_xyz(xyz_t *output, float x, float y, int z, int count, vertex_f_t *vertices);

unsigned int get_max_z(GSCONTEXT* gsGlobal);

void athena_set_tw_th(const GSSURFACE *Texture, int *tw, int *th);

void athena_line_goraud_3d(GSCONTEXT *gsGlobal, float x1, float y1, int iz1, float x2, float y2, int iz2, uint64_t color1, uint64_t color2);

void LookAtCameraMatrix(MATRIX m, VECTOR position, VECTOR target, VECTOR up);

void render_init();

void render_begin();

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

// Forces draw_vu1_with_colors to rebuild its pre-baked DMA_CALL chains (see
// athena_chain_cache) on their next use, instead of reusing the cached ones
// -- bumps chain_version, so every pass_state slot notices the mismatch and
// rebuilds independently on its own next draw. Needed after anything that
// changes materials/material_indices/texture_mapping/vertices post-creation,
// or that reallocates the compact_* buffers whose addresses are baked into
// the chains' DMA_REF tags.
void render_invalidate_chain_cache(athena_render_data *data);

void update_object_space(athena_object_data *obj);

void create_view(MATRIX view_screen, float fov, float near, float far, float w, float h);

#define alloc_vectors(cnt) (VECTOR*)malloc(cnt * sizeof(VECTOR))
#define copy_vectors(dst, src, cnt) memcpy(dst, src, cnt*sizeof(VECTOR))
#define copy_vector(dst, src) memcpy(dst, src, sizeof(VECTOR))
#define free_vectors(vec) free(vec)

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
} render_stats_t;

const render_stats_t *render_get_stats(void);
void render_reset_stats(void);

#endif
