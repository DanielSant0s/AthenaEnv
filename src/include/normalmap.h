#ifndef ATHENA_NORMALMAP_H
#define ATHENA_NORMALMAP_H

#include <render.h>

// Per-pixel DOT3 lighting from tangent-space normal maps, after Morten
// Mikkelsen's "DOT3 Normal Mapping on the PS2" (IO Interactive, 2004). The GS
// has no signed multiply and no way to add channels, so both are worked around:
// the signed N.L products land in R, G and B of the frame buffer through palette
// tricks and the blend unit, channel shuffles add them up, and an intensity
// palette turns the sum into light. Values of attributes.normal_mapping:
#define DOT3_OFF   0
#define DOT3_2PASS 1 // 2 geometry passes, light not renormalized per pixel
#define DOT3_4PASS 2 // 4 geometry passes, light renormalized through a sphere map

typedef enum {
	DOT3_PASS_POSITIVE,  // 2-pass: max(n, 0) * L
	DOT3_PASS_NEGATIVE,  // 2-pass: - max(-n, 0) * L, plus the correction term in alpha
	DOT3_PASS_SPHERE,    // 4-pass: the normalized light, from the sphere map
	DOT3_PASS_X,         // 4-pass: L.x * n.x into red
	DOT3_PASS_Y,         // 4-pass: into green
	DOT3_PASS_Z,         // 4-pass: into blue
	DOT3_PASS_COUNT
} eDot3Pass;

typedef struct athena_dot3_map athena_dot3_map;

// athena_render_data.dot3. Everything normal mapping keeps per mesh.
typedef struct athena_dot3_state {
	VECTOR *tangents;                        // per source vertex: unit tangent, w = handedness (+-1)
	athena_compact_normal *compact_tangents; // V4_8, padded like compact_normals
	uint32_t compact_capacity;
	uint32_t cooked_version;                 // chain_version the compact tangents were cooked for
	athena_chain_cache chains[DOT3_PASS_COUNT];
	athena_dot3_map **maps;                  // per data->textures index, built on first use
	int map_count;
	bool unsupported;                        // no UVs or no float source to build tangents from
} athena_dot3_state;

// One geometry pass of the DOT3 program (render.c).
typedef struct {
	eDot3Pass pass;
	int tfx;                  // texture function (COLOR_MODULATE / COLOR_HIGHLIGHT)
	VECTOR mode;              // DOT3_MODE weights, see draw_3D_dot3.vcl
	// The texture and palette a material draws this pass with.
	void (*bind)(void *ctx, athena_render_data *data, const ath_mat *mat,
	             GSSURFACE **tex, GSSURFACE **clut);
	void *ctx;
} render_dot3_pass;

// Draws obj with the DOT3 program (render.c). The caller sets up GS state.
void render_draw_dot3_pass(athena_object_data *obj, const render_dot3_pass *pass);

// The light normal mapping (and the shadow volumes) use: the scene's first
// light, world space. light w = 0: xyz = direction towards it; w = 1: xyz = its
// position, *atten = its quadratic attenuation k (1 / (1 + k d^2)), else 0.
// Also the scalar ambient and diffuse levels. False when there is no light.
bool render_get_dot3_light(VECTOR light, float *atten, float *ambient, float *diffuse);

// That light in the object space of transform m: out w as light w, a direction
// at unit length; out_atten = k for the object's (uniform) scale.
void render_dot3_light_to_object(const float *m, const VECTOR light, float atten, VECTOR out, float *out_atten);

// Draws obj's base pass with normal mapping when it is on and possible.
// Returns false to let render_object() draw it the usual way.
bool normalmap_render_object(athena_object_data *obj);

// Frees data->dot3, waiting for any draw still reading it. keep_maps leaves the
// prepared normal maps (texture-derived) for the next draw to reuse.
void normalmap_release(athena_render_data *data, bool keep_maps);

// Builds data->dot3->tangents from positions, normals and UVs. False when the
// mesh has no float source or no UVs.
bool normalmap_build_tangents(athena_render_data *data, athena_dot3_state *st);

#endif
