#ifndef ATHENA_SHADOWS_H
#define ATHENA_SHADOWS_H

#include <graphics.h>
#include <matrix.h>
#include <render.h>

#ifdef ATHENA_ODE
#include <ode/ode.h>
#endif

#define SHADOW_BLEND_DARKEN 0
#define SHADOW_BLEND_ALPHA  1
#define SHADOW_BLEND_ADD    2

// A projector's chunks reach its vertex buffer through a DMA_REF, so the DMAC
// reads that buffer whenever the chain it was queued in finally runs -- long
// after shadow_projector_render() returned. One buffer per draw still in flight
// is therefore the price of drawing the same projector twice in a frame (two
// casters sharing it): rewriting a single buffer would hand the earlier draw
// the later draw's grid, and waiting the earlier draw out would stall the EE on
// the GS. Slots are allocated on demand, so a projector drawn once per frame
// keeps exactly one.
typedef struct shadow_draw_slot {
    athena_render_data data;
    uint32_t queuedGen;          // owl_flush_generation() of this slot's last draw
    int      hasQueued;
} shadow_draw_slot;

// Ceiling on that growth, so a script drawing dozens of shadows off one
// projector cannot quietly eat RAM -- past it, draws wait instead.
#define SHADOW_MAX_DRAW_SLOTS 8

// The grid is drawn as one tristrip per row, and each row is its own material
// group so that the draw loop never splits a strip across two chunks (a chunk
// boundary restarts the primitive). That only holds while a row's 2*gridX
// vertices fit in one chunk, which is what this clamps gridX to.
#define SHADOW_MAX_GRID_X ((BATCH_SIZE - (BATCH_SIZE % 12)) / 2)

typedef struct ath_shadow_projector {
	MATRIX       transform;      // World transform: position/orientation of projector
	GSSURFACE   *texture;        // Shadow texture
	float        width;          // Extents in local X
	float        height;         // Extents in local Z
	int          gridX;          // Grid tesselation X
	int          gridZ;          // Grid tesselation Z
	int          enableRaycast;  // Deform to world via ODE raycasts
	float        rayLength;      // Ray length along -lightDir
	float        bias;           // Small offset along -N/light to avoid z-fighting
    float        lightOffset;    // Offset along -lightDir to shift decal center
	float        maxSlopeCos;    // Reject hits steeper than this (optional)
	VECTOR       lightDir;       // Directional light (normalized)
    VECTOR       color;          // RGBA as floats (0..255 expected by pipeline)
    int          blendMode;      // SHADOW_BLEND_*

    // Texture sub-rectangle (normalized 0..1)
    float        u0, v0, u1, v1;

#ifdef ATHENA_ODE
	dSpaceID     raySpace;       // Space to collide rays against
	dGeomID      rayGeom;        // Reusable ODE ray geom
#endif

    // Prebuilt render data, one per draw that can be in flight (see
    // shadow_draw_slot). Rebuilt from scratch when the grid changes.
    shadow_draw_slot *slots;
    int slotCount;
    int slotCursor;              // round-robin start, so the oldest draw is tried first
    athena_object_data obj;

    // Grid nodes and mapping (allocated with grid)
    VECTOR *nodes;               // gx*gz world-space nodes updated per frame
    VECTOR *nodeNormals;         // gx*gz raycast hit normals, only touched when enableRaycast; same lifecycle as nodes (was alloc/free'd every shadow_projector_render() call before, which is wasteful)
    uint32_t *triNodeIdx;        // vtxCount entries mapping vertex -> node index
    int vtxCount;                // cached vertex count
    int localGridDirty;          // slot 0 no longer holds the untouched local grid
} ath_shadow_projector;

void shadow_projector_init(ath_shadow_projector *p, GSSURFACE *tex);
void shadow_projector_set_transform(ath_shadow_projector *p, const MATRIX transform);
void shadow_projector_set_size(ath_shadow_projector *p, float width, float height);
void shadow_projector_set_grid(ath_shadow_projector *p, int gx, int gz);
void shadow_projector_set_light_dir(ath_shadow_projector *p, float x, float y, float z);
void shadow_projector_set_bias(ath_shadow_projector *p, float bias);
void shadow_projector_set_light_offset(ath_shadow_projector *p, float dist);
void shadow_projector_set_slope_limit(ath_shadow_projector *p, float maxSlopeCos);
void shadow_projector_set_color(ath_shadow_projector *p, float r, float g, float b, float a);
void shadow_projector_set_blend(ath_shadow_projector *p, int mode);
void shadow_projector_set_uv_rect(ath_shadow_projector *p, float u0, float v0, float u1, float v1);

#ifdef ATHENA_ODE
void shadow_projector_enable_raycast(ath_shadow_projector *p, dSpaceID space, float rayLength, int enable);
#else
static inline void shadow_projector_enable_raycast(ath_shadow_projector *p, void *space, float rayLength, int enable) { (void)p; (void)space; (void)rayLength; (void)enable; }
#endif

// Draw using the VU1 colors pipeline, with darkening blend equation
void shadow_projector_render(ath_shadow_projector *p);

// Free all projector-allocated resources
void shadow_projector_free(ath_shadow_projector *p);

// Rebuild internal geometry when grid changes
void shadow_projector_rebuild_geometry(ath_shadow_projector *p);

// Shadow-specific transform matrix creation (avoids modifying shared functions)
void shadow_create_transform_matrix(MATRIX result, const VECTOR position, 
                                  const VECTOR rotation, const VECTOR scale);

#endif


