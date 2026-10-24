#ifndef ATHENA_SHADOW_VOLUMES_H
#define ATHENA_SHADOW_VOLUMES_H

#include <render.h>

// Stencil shadow volumes without a stencil buffer (shadow_volumes.c).
//
// The red channel of the frame buffer is the stencil: between
// shadow_volumes_begin() and shadow_volumes_end() each volume adds 1 where its
// front faces pass the depth test and subtracts 1 where its back faces do
// (modulo 256), so red ends non-zero exactly where the scene lies inside a
// volume. The GS only tests GEQUAL/GREATER, which rules out z-fail: this is
// depth-pass. The camera is kept out of every volume instead: extruded vertices
// stop in front of the near plane, and caps close the volume there.

typedef struct ath_shadow_volume {
	athena_object_data *caster;   // drawn object the volume follows (transform, light side)

	// The caster welded by position into a closed mesh.
	VECTOR   *pos;
	uint32_t  pos_count;
	uint32_t (*tri)[3];
	VECTOR   *tri_normal;
	uint32_t  tri_count;
	struct { uint32_t a, b; int32_t t0, t1; } *edges; // a->b as wound in t0
	uint32_t  edge_count;
	uint8_t  *lit;                 // per triangle, this frame

	// One quad per edge (extruded on silhouette edges), then two triangles per
	// triangle (the caps, from the triangles facing away from the light), all
	// degenerate where unused: the vertex count -- and with it the cached DMA
	// chain -- never changes.
	// Two buffers: the DMAC may still be reading last frame's.
	athena_render_data slot[2];
	athena_object_data obj[2];
	int cur;

	float extrusion;               // distance the silhouette is pushed away from the light
} ath_shadow_volume;

// NULL when the caster's mesh has no float source (frozen) or no triangles.
ath_shadow_volume *shadow_volume_create(athena_object_data *caster);
void shadow_volume_free(ath_shadow_volume *v);

// Draws v into the stencil. Only between begin and end.
void shadow_volume_render(ath_shadow_volume *v);

// rect = { x0, y0, x1, y1 } the volumes can reach on screen, NULL for all of it.
// Returns false (and draws nothing until end) without a CT32 frame buffer and
// a Z24 depth buffer: the top byte of Z24 keeps the scene's red meanwhile.
bool shadow_volumes_begin(const int *rect);
// Darkens what ended up inside a volume to 1 - opacity.
void shadow_volumes_end(float opacity);
// What a light's shadow really leaves: what ended up inside a volume is drawn
// again with the ambient light only -- objs, through the destination alpha
// test, so their shadowed pixels alone. Faces turned from the light, already
// ambient only, come out the same; nothing is darkened twice.
void shadow_volumes_end_relight(athena_object_data *const *objs, int count);

#endif
