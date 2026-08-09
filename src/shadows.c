#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <shadows.h>
#include <vector.h>

// Helper function to create transform matrix specifically for shadows
// This avoids modifying shared functions that could break other systems
void shadow_create_transform_matrix(MATRIX result, const VECTOR position, 
                                  const VECTOR rotation, const VECTOR scale) {
    MATRIX scale_matrix;
    matrix_functions->identity(scale_matrix);
    scale_matrix[0] =  scale[0] * 1.0f;
    scale_matrix[5] =  scale[1] * 1.0f;
    scale_matrix[10] = scale[2] * 1.0f;

    MATRIX rotation_matrix;
    // Convert quaternion to matrix
    float x = rotation[0];
    float y = rotation[1];
    float z = rotation[2];
    float w = rotation[3];
    
    float x2 = x * 2.0f;
    float y2 = y * 2.0f;
    float z2 = z * 2.0f;
    float xx = x * x2;
    float xy = x * y2;
    float xz = x * z2;
    float yy = y * y2;
    float yz = y * z2;
    float zz = z * z2;
    float wx = w * x2;
    float wy = w * y2;
    float wz = w * z2;
    
    matrix_functions->identity(rotation_matrix);
    
    rotation_matrix[0] = 1.0f - (yy + zz);
    rotation_matrix[1] = xy - wz;
    rotation_matrix[2] = xz + wy;
    
    rotation_matrix[4] = xy + wz;
    rotation_matrix[5] = 1.0f - (xx + zz);
    rotation_matrix[6] = yz - wx;
    
    rotation_matrix[8] = xz - wy;
    rotation_matrix[9] = yz + wx;
    rotation_matrix[10] = 1.0f - (xx + yy);

    MATRIX translation_matrix;
    matrix_functions->identity(translation_matrix);
    translation_matrix[12] = position[0];
    translation_matrix[13] = position[1];
    translation_matrix[14] = position[2];

    MATRIX temp;
    matrix_functions->multiply(temp, rotation_matrix, scale_matrix);
    matrix_functions->multiply(result, translation_matrix, temp);
}

// forward declaration from render.c
void draw_vu1_with_colors(athena_object_data *obj, int pass_state);

static inline uint64_t rgba_u32(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	return (uint64_t)r | ((uint64_t)g << 8) | ((uint64_t)b << 16) | ((uint64_t)a << 24);
}

void shadow_projector_init(ath_shadow_projector *p, GSSURFACE *tex) {
	memset(p, 0, sizeof(*p));
	p->texture = tex;
	p->width = 1.0f;
	p->height = 1.0f;
	p->gridX = 2;
	p->gridZ = 2;
	p->enableRaycast = 0;
	p->rayLength = 2.0f;
	p->bias = 0.01f;
	p->lightOffset = 0.0f;
	p->maxSlopeCos = -1.0f; // disabled
	p->lightDir[0] = 0.0f;
	p->lightDir[1] = -1.0f;
	p->lightDir[2] = 0.0f;
	p->lightDir[3] = 0.0f;
    // default decal/shadow color in 0..1 range and blend mode
    p->color[0] = 0.0f; p->color[1] = 0.0f; p->color[2] = 0.0f; p->color[3] = 0.75f;
	p->blendMode = SHADOW_BLEND_DARKEN;
	p->u0 = 0.0f; p->v0 = 0.0f; p->u1 = 1.0f; p->v1 = 1.0f;
	matrix_functions->identity(p->transform);

#ifdef ATHENA_ODE
	p->raySpace = NULL;
	p->rayGeom = NULL;
#endif

    p->nodes = NULL;
    p->nodeNormals = NULL;
    p->triNodeIdx = NULL;
    p->vtxCount = 0;

    p->slots = NULL;
    p->slotCount = 0;
    p->slotCursor = 0;

    memset(&p->obj, 0, sizeof(p->obj));
    
    // Initialize position to origin
    p->obj.position[0] = 0.0f;
    p->obj.position[1] = 0.0f;
    p->obj.position[2] = 0.0f;
    p->obj.position[3] = 1.0f;
    
    // Initialize rotation to identity quaternion
    p->obj.rotation[0] = 0.0f;
    p->obj.rotation[1] = 0.0f;
    p->obj.rotation[2] = 0.0f;
    p->obj.rotation[3] = 1.0f;
    
    // Initialize scale to identity
    p->obj.scale[0] = 1.0f;
    p->obj.scale[1] = 1.0f;
    p->obj.scale[2] = 1.0f;
    p->obj.scale[3] = 1.0f;

    // Build initial geometry for default grid
    shadow_projector_rebuild_geometry(p);
}

void shadow_projector_set_transform(ath_shadow_projector *p, const MATRIX transform) {
	memcpy(p->transform, transform, sizeof(MATRIX));
}

void shadow_projector_set_size(ath_shadow_projector *p, float width, float height) {
	p->width = width;
	p->height = height;
	// Changes the local grid itself, which the non-raycast path only writes when
	// this says so.
	p->localGridDirty = 1;
}

void shadow_projector_set_grid(ath_shadow_projector *p, int gx, int gz) {
	p->gridX = gx < 2? 2 : (gx > SHADOW_MAX_GRID_X? SHADOW_MAX_GRID_X : gx);
	p->gridZ = gz < 2? 2 : gz;
    shadow_projector_rebuild_geometry(p);
}

void shadow_projector_set_light_dir(ath_shadow_projector *p, float x, float y, float z) {
	p->lightDir[0] = x;
	p->lightDir[1] = y;
	p->lightDir[2] = z;
	vector_functions->normalize(p->lightDir, p->lightDir);
}

void shadow_projector_set_bias(ath_shadow_projector *p, float bias) {
	p->bias = bias;
}

void shadow_projector_set_light_offset(ath_shadow_projector *p, float dist) {
    p->lightOffset = dist;
}

void shadow_projector_set_slope_limit(ath_shadow_projector *p, float maxSlopeCos) {
	p->maxSlopeCos = maxSlopeCos;
}

void shadow_projector_set_color(ath_shadow_projector *p, float r, float g, float b, float a) {
    p->color[0] = r; p->color[1] = g; p->color[2] = b; p->color[3] = a;
}

void shadow_projector_set_blend(ath_shadow_projector *p, int mode) {
    p->blendMode = mode;
}

void shadow_projector_set_uv_rect(ath_shadow_projector *p, float u0, float v0, float u1, float v1) {
    p->u0 = u0; p->v0 = v0; p->u1 = u1; p->v1 = v1;
    // UVs are baked in geometry; rebuild to apply new rect
    shadow_projector_rebuild_geometry(p);
}

#ifdef ATHENA_ODE
void shadow_projector_enable_raycast(ath_shadow_projector *p, dSpaceID space, float rayLength, int enable) {
	p->raySpace = space;
	p->rayLength = rayLength;
	p->enableRaycast = enable;
	if (enable) {
		if (!p->rayGeom) {
			p->rayGeom = dCreateRay(space, rayLength);
			dGeomRaySetClosestHit(p->rayGeom, 1);
		}
		else {
			dGeomRaySetLength(p->rayGeom, rayLength);
		}
	}
}

typedef struct ClosestHitData {
	int hasHit;
	float bestDepth;
	VECTOR position;
	VECTOR normal;
} ClosestHitData;

static void shadow_ode_ray_cb(void *data, dGeomID o1, dGeomID o2) {
	ClosestHitData *ch = (ClosestHitData*)data;
	const int maxC = 8;
	static dContact contact[8];
	int n = dCollide(o1, o2, maxC, &contact[0].geom, sizeof(dContact));
	for (int i = 0; i < n; i++) {
		float depth = contact[i].geom.depth;
		if (!ch->hasHit || depth < ch->bestDepth) {
			ch->hasHit = 1;
			ch->bestDepth = depth;
			ch->position[0] = contact[i].geom.pos[0];
			ch->position[1] = contact[i].geom.pos[1];
			ch->position[2] = contact[i].geom.pos[2];
			ch->position[3] = 1.0f;
			ch->normal[0] = contact[i].geom.normal[0];
			ch->normal[1] = contact[i].geom.normal[1];
			ch->normal[2] = contact[i].geom.normal[2];
			ch->normal[3] = 0.0f;
		}
	}
}
#endif

// Builds one draw slot's render data: topology, UVs and colours, which are the
// same for every slot -- only the positions differ, and those are rewritten per
// draw. Also (re)writes the shared triNodeIdx, which is identical work on the
// second and later slot but keeps this to one loop.
static void shadow_slot_build(ath_shadow_projector *p, shadow_draw_slot *slot);

// Frees everything a slot owns, including what the render pipeline allocated
// behind it (compact VU wire cache and baked DMA_CALL chains).
static void shadow_slot_free(shadow_draw_slot *slot) {
    athena_render_data *data = &slot->data;

    // Everything below is freed, and the DMAC reaches most of it by reference:
    // a setGrid/setUVRect between two draws would pull it out from under a draw
    // call still queued this frame.
    if (slot->hasQueued)
        owl_wait_generation(slot->queuedGen);

    if (data->positions) { free_vectors(data->positions); data->positions = NULL; }
    if (data->texcoords) { free_vectors(data->texcoords); data->texcoords = NULL; }
    if (data->colours) { free_vectors(data->colours); data->colours = NULL; }
    if (data->materials) { free(data->materials); data->materials = NULL; }
    if (data->material_indices) { free(data->material_indices); data->material_indices = NULL; }
    if (data->textures) { free(data->textures); data->textures = NULL; }

    // Compact VIF-unpack cache (render_cook_compact_vertices)
    free(data->compact_positions); data->compact_positions = NULL;
    free(data->compact_normals); data->compact_normals = NULL;
    free(data->compact_colors); data->compact_colors = NULL;
    free(data->compact_uvs); data->compact_uvs = NULL;
    free(data->compact_group_base); data->compact_group_base = NULL;
    data->compact_capacity = 0;
    data->compact_group_count = 0;

    // Baked DMA_CALL chains: 3 pass_state slots per pipeline, plus reflection.
    athena_chain_cache *chains[10] = {
        &data->chain[0][0], &data->chain[0][1], &data->chain[0][2],
        &data->chain[1][0], &data->chain[1][1], &data->chain[1][2],
        &data->chain[2][0], &data->chain[2][1], &data->chain[2][2],
        &data->ref_chain
    };
    for (int ci = 0; ci < 10; ci++) {
        free(chains[ci]->buffer); chains[ci]->buffer = NULL;
        free(chains[ci]->chunk_offset); chains[ci]->chunk_offset = NULL;
        free(chains[ci]->tex_giftag); chains[ci]->tex_giftag = NULL;
        chains[ci]->qwc_alloc = 0;
        chains[ci]->chunk_count = 0;
    }

    memset(slot, 0, sizeof(*slot));
}

static void shadow_slots_release(ath_shadow_projector *p) {
    for (int i = 0; i < p->slotCount; i++)
        shadow_slot_free(&p->slots[i]);

    free(p->slots);
    p->slots = NULL;
    p->slotCount = 0;
    p->slotCursor = 0;
}

// Picks a slot whose last draw the DMAC is already done with, so this render can
// overwrite its grid. Grows the pool when they are all still in flight, which is
// what keeps a projector shared by several casters from stalling the EE.
static shadow_draw_slot *shadow_pick_slot(ath_shadow_projector *p) {
    for (int i = 0; i < p->slotCount; i++) {
        int idx = (p->slotCursor + i) % p->slotCount;
        shadow_draw_slot *slot = &p->slots[idx];

        if (!slot->hasQueued || owl_generation_read(slot->queuedGen)) {
            p->slotCursor = (idx + 1) % p->slotCount;
            return slot;
        }
    }

    if (p->slotCount < SHADOW_MAX_DRAW_SLOTS) {
        shadow_draw_slot *grown = (shadow_draw_slot*)realloc(p->slots, (p->slotCount + 1) * sizeof(shadow_draw_slot));
        if (grown) {
            p->slots = grown;

            shadow_draw_slot *slot = &grown[p->slotCount];
            memset(slot, 0, sizeof(*slot));
            shadow_slot_build(p, slot);

            p->slotCount++;
            p->slotCursor = 0;
            return slot;
        }
    }

    // Out of slots (or out of memory): the oldest draw is the one most likely to
    // be nearly done, so wait that one out and reuse it.
    shadow_draw_slot *slot = &p->slots[p->slotCursor];
    owl_wait_generation(slot->queuedGen);
    p->slotCursor = (p->slotCursor + 1) % p->slotCount;
    return slot;
}

// Does the grid have to be recomputed per draw? Only raycasting makes it depend
// on where the projector is: it conforms every node to whatever is under it.
static inline int shadow_is_dynamic(const ath_shadow_projector *p) {
#ifdef ATHENA_ODE
    return p->enableRaycast && p->raySpace && p->rayGeom;
#else
    (void)p;
    return 0;
#endif
}

// Writes the flat grid, in projector-local space, into a slot. Constant for a
// given size and tesselation, so this runs on a grid rebuild and when coming
// back from raycasting -- never per draw.
static void shadow_fill_local_grid(ath_shadow_projector *p, shadow_draw_slot *slot) {
    const int gx = p->gridX;
    const int gz = p->gridZ;
    const float halfW = p->width * 0.5f;
    const float halfH = p->height * 0.5f;

    // The slot may still be queued from an earlier draw, and this overwrites the
    // buffer it points a DMA_REF at.
    if (slot->hasQueued)
        owl_wait_generation(slot->queuedGen);

    int ni = 0;
    for (int j = 0; j < gz; j++) {
        float tz = (float)j / (float)(gz - 1);
        for (int i = 0; i < gx; i++) {
            float tx = (float)i / (float)(gx - 1);
            p->nodes[ni][0] = -halfW + tx * p->width;
            p->nodes[ni][1] = 0.0f;
            p->nodes[ni][2] = -halfH + tz * p->height;
            p->nodes[ni][3] = 1.0f;
            ni++;
        }
    }

    for (int v = 0; v < p->vtxCount; v++)
        copy_vector(slot->data.positions[v], p->nodes[p->triNodeIdx[v]]);

    render_invalidate_compact_positions(&slot->data);
    p->localGridDirty = 0;
}

// Object matrix for a local-space grid: the projector transform, with the light
// offset folded into its translation. That offset is the same world-space shift
// for every node, which is exactly what a translation is -- no reason to pay it
// per vertex.
static void shadow_place_object(ath_shadow_projector *p) {
    memcpy(p->obj.transform, p->transform, sizeof(MATRIX));

    p->obj.transform[12] -= p->lightDir[0] * p->lightOffset;
    p->obj.transform[13] -= p->lightDir[1];
    p->obj.transform[14] -= p->lightDir[2] * p->lightOffset;
}

// Raycast path: every node is snapped onto whatever the ray hit, so the whole
// grid comes out in world space and gets rewritten per draw.
static void shadow_build_world_grid(ath_shadow_projector *p, athena_render_data *data) {
    const int gx = p->gridX;
    const int gz = p->gridZ;

	const float halfW = p->width * 0.5f;
	const float halfH = p->height * 0.5f;

    // Persistent buffer (allocated in shadow_projector_rebuild_geometry,
    // sized nodeCount) -- was alloc_vectors()/free_vectors()'d on every
    // single call before, which is wasteful given this function already
    // runs once per shadow-casting object per frame.
    VECTOR *nodeNormals = p->enableRaycast ? p->nodeNormals : NULL;

	int ni = 0;
	for (int j = 0; j < gz; j++) {
		float tz = (float)j / (float)(gz - 1);
		float z = -halfH + tz * (p->height);
		for (int i = 0; i < gx; i++) {
			float tx = (float)i / (float)(gx - 1);
			float x = -halfW + tx * (p->width);
			VECTOR local = { x, 0.0f, z, 1.0f };
			VECTOR world;
            matrix_functions->apply(world, p->transform, local);
            // shift along -lightDir to move decal relative to light direction
            world[0] -= p->lightDir[0] * p->lightOffset;
            world[1] -= p->lightDir[1];
            world[2] -= p->lightDir[2] * p->lightOffset;

#ifdef ATHENA_ODE
			if (p->enableRaycast && p->raySpace && p->rayGeom) {
				VECTOR start, dir;
				// Start above the surface along +lightDir, cast towards -lightDir
				start[0] = world[0] + p->lightDir[0] * (p->rayLength * 0.5f);
				start[1] = world[1] + p->lightDir[1] * (p->rayLength * 0.5f);
				start[2] = world[2] + p->lightDir[2] * (p->rayLength * 0.5f);
				start[3] = 1.0f;
				dir[0] = -p->lightDir[0];
				dir[1] = -p->lightDir[1];
				dir[2] = -p->lightDir[2];
				dir[3] = 0.0f;

				dGeomRaySetLength(p->rayGeom, p->rayLength);
				dGeomRaySet(p->rayGeom, start[0], start[1], start[2], dir[0], dir[1], dir[2]);
				dGeomRaySetClosestHit(p->rayGeom, 1);
				dGeomRaySetParams(p->rayGeom, 1, 0);

				ClosestHitData ch = { 0 };
				dSpaceCollide2((dGeomID)p->rayGeom, (dGeomID)p->raySpace, &ch, shadow_ode_ray_cb);
				if (ch.hasHit) {
					// Slope filter
                    if (p->maxSlopeCos > -0.5f) {
						VECTOR up = { 0.0f, 1.0f, 0.0f, 0.0f };
                    float dp = vector_functions->dot(up, ch.normal);
						if (dp < p->maxSlopeCos) {
							// reject: keep original
                            p->nodes[ni][0] = world[0];
                            p->nodes[ni][1] = world[1];
                            p->nodes[ni][2] = world[2];
                            p->nodes[ni][3] = 1.0f;
							if (nodeNormals) { nodeNormals[ni][0] = 0; nodeNormals[ni][1] = 1; nodeNormals[ni][2] = 0; nodeNormals[ni][3] = 0; }
							ni++;
							continue;
						}
					}
                    p->nodes[ni][0] = ch.position[0] - p->lightDir[0] * p->bias;
                    p->nodes[ni][1] = ch.position[1] - p->lightDir[1] * p->bias;
                    p->nodes[ni][2] = ch.position[2] - p->lightDir[2] * p->bias;
                    p->nodes[ni][3] = 1.0f;
					if (nodeNormals) {
						nodeNormals[ni][0] = ch.normal[0];
						nodeNormals[ni][1] = ch.normal[1];
						nodeNormals[ni][2] = ch.normal[2];
						nodeNormals[ni][3] = 0.0f;
					}
				} else {
                    p->nodes[ni][0] = world[0];
                    p->nodes[ni][1] = world[1];
                    p->nodes[ni][2] = world[2];
                    p->nodes[ni][3] = 1.0f;
					if (nodeNormals) { nodeNormals[ni][0] = 0; nodeNormals[ni][1] = 1; nodeNormals[ni][2] = 0; nodeNormals[ni][3] = 0; }
				}
			} else
#endif
			{
                p->nodes[ni][0] = world[0];
                p->nodes[ni][1] = world[1];
                p->nodes[ni][2] = world[2];
                p->nodes[ni][3] = 1.0f;
			}
			ni++;
		}
	}

    // Update positions in prebuilt vertex list using node mapping
    for (int v = 0; v < p->vtxCount; v++) {
        uint32_t n = p->triNodeIdx[v];
        copy_vector(data->positions[v], p->nodes[n]);
    }

    // data->positions was just rewritten directly (not through the JS
    // "vertices" setter, which is the only other place that invalidates
    // this automatically). Without this, the compact VU wire cache keeps
    // serving whatever was cooked on an earlier call, so the grid the GS
    // actually sees would never move.
    //
    // Positions only, not the full cache: colours/texcoords are set once in
    // shadow_slot_build() and never touched again, so re-cooking them every
    // render() call would just be discarding-and-redoing work for data that
    // never changed.
    render_invalidate_compact_positions(data);
}

void shadow_projector_render(ath_shadow_projector *p) {
    shadow_draw_slot *slot;
    athena_render_data *data;

    if (shadow_is_dynamic(p)) {
        // World-space geometry, different every draw: it needs a slot the DMAC
        // is done with, and the object matrix has to stay out of the way.
        slot = shadow_pick_slot(p);
        shadow_build_world_grid(p, &slot->data);
        matrix_functions->identity(p->obj.transform);

        // This may well have been slot 0, so the local grid is no longer there.
        p->localGridDirty = 1;
    } else {
        // Nothing to rewrite: the grid is constant in local space and the whole
        // placement rides in the object matrix, which draw_vu1_with_colors
        // copies inline into the packet. One slot serves every draw, and both
        // the compact cook and the baked chain survive across frames -- a blob
        // shadow costs no EE geometry work at all.
        slot = &p->slots[0];
        if (p->localGridDirty)
            shadow_fill_local_grid(p, slot);

        shadow_place_object(p);
    }

    data = &slot->data;

    // The object is bound to whichever slot this draw got. Nothing else on it is
    // per-slot: the grid is the whole geometry, and it carries no skeleton.
    p->obj.data = data;

    // Update texture pointer (in case it changed)
    if (data->textures && data->texture_count > 0)
        data->textures[0] = p->texture;

    uint64_t old_alpha = get_screen_param(ALPHA_BLEND_EQUATION);

    switch (p->blendMode) {
        case SHADOW_BLEND_DARKEN:
            set_screen_param(ALPHA_BLEND_EQUATION, ALPHA_EQUATION(SRC_RGB, DST_RGB, SRC_ALPHA, DST_RGB, 0x00));
            break;
        case SHADOW_BLEND_ALPHA:
            set_screen_param(ALPHA_BLEND_EQUATION, GS_ALPHA_BLEND_NORMAL);
            break;
        case SHADOW_BLEND_ADD:
            set_screen_param(ALPHA_BLEND_EQUATION, GS_ALPHA_BLEND_ADD);
            break;
    }

    draw_vu1_with_colors(&p->obj, 0);

    // Taken AFTER the draw: it may have flushed the ring mid-chunk, so the last
    // chain referencing this grid is the current one, not the one we started in.
    slot->queuedGen = owl_flush_generation();
    slot->hasQueued = 1;

	set_screen_param(ALPHA_BLEND_EQUATION, old_alpha);
}

static void shadow_slot_build(ath_shadow_projector *p, shadow_draw_slot *slot) {
    const int gx = p->gridX;
    const int gz = p->gridZ;
    const int vtxCount = p->vtxCount;

    athena_render_data *data = &slot->data;

    // Allocate render data
    data->index_count = vtxCount;
    data->positions = alloc_vectors(vtxCount);
    data->texcoords = alloc_vectors(vtxCount);
    data->colours = alloc_vectors(vtxCount);
    data->normals = NULL;
    data->skin_data = NULL;
    data->skeleton = NULL;
    data->pipeline = PL_NO_LIGHTS;
    data->textures = (GSSURFACE**)malloc(sizeof(GSSURFACE*));
    data->texture_count = 1;
    data->textures[0] = p->texture;
    data->materials = (ath_mat*)malloc(sizeof(ath_mat));
    memset(data->materials, 0, sizeof(ath_mat));
    data->material_count = 1;
    data->materials[0].texture_id = 0;
    // Keep material neutral so texture+vertex color show as-is
    data->materials[0].diffuse[0] = 0.0f;
    data->materials[0].diffuse[1] = 0.0f;
    data->materials[0].diffuse[2] = 0.0f;
    data->materials[0].diffuse[3] = 0.0f;
    // One group per grid row -- see the topology loop below.
    data->material_indices = (material_index*)malloc((gz - 1) * sizeof(material_index));
    data->material_index_count = gz - 1;
    data->attributes.accurate_clipping = 0;
    data->attributes.face_culling = CULL_FACE_NONE;
    data->attributes.texture_mapping = 1;
    data->attributes.shade_model = SHADE_GOURAUD;
    data->attributes.has_refmap = 0;
    data->attributes.has_bumpmap = 0;
    data->attributes.has_decal = 0;
    data->tristrip = 1;

    // Topology, UVs and per-vertex node mapping.
    //
    // A grid row is a tristrip by construction -- zig-zag between row j and row
    // j+1 -- which is 2*gridX vertices per row against the 6 per quad a triangle
    // soup needs (264 vs 726 on a 12x12 grid). The catch is that every chunk
    // carries its own GIFtag and so restarts the primitive, which would tear a
    // strip split across two chunks. Making each row its OWN material group
    // solves that with the machinery already there: the draw loop never lets a
    // chunk span two materials, and SHADOW_MAX_GRID_X keeps a row inside one
    // chunk. Fewer chunks than the soup, too (one per row, 11 vs 16 here).
    //
    // Winding alternates along a strip; harmless because the projector draws
    // with CULL_FACE_NONE, which is also why the VU never has to know.
    int v = 0;
    for (int j = 0; j < gz - 1; j++) {
        float V0 = p->v0 + (p->v1 - p->v0) * ((float)j / (float)(gz - 1));
        float V1 = p->v0 + (p->v1 - p->v0) * ((float)(j + 1) / (float)(gz - 1));

        for (int i = 0; i < gx; i++) {
            float U = p->u0 + (p->u1 - p->u0) * ((float)i / (float)(gx - 1));

            p->triNodeIdx[v+0] = (uint32_t)(j * gx + i);
            p->triNodeIdx[v+1] = (uint32_t)((j + 1) * gx + i);

            data->texcoords[v+0][0] = U; data->texcoords[v+0][1] = V0; data->texcoords[v+0][2] = 1.0f; data->texcoords[v+0][3] = 1.0f;
            data->texcoords[v+1][0] = U; data->texcoords[v+1][1] = V1; data->texcoords[v+1][2] = 1.0f; data->texcoords[v+1][3] = 1.0f;

            for (int k = 0; k < 2; k++) {
                data->colours[v+k][0] = p->color[0];
                data->colours[v+k][1] = p->color[1];
                data->colours[v+k][2] = p->color[2];
                data->colours[v+k][3] = p->color[3];
            }

            v += 2;
        }

        data->material_indices[j].index = 0;
        data->material_indices[j].end = v - 1;   // inclusive, like every loader
    }
}

void shadow_projector_rebuild_geometry(ath_shadow_projector *p) {
    const int gx = p->gridX;
    const int gz = p->gridZ;
    // One tristrip per row: 2 vertices per column, gz-1 rows.
    const int vtxCount = (gz - 1) * gx * 2;
    const int nodeCount = gx * gz;

    // Free previous allocations. The pool drops back to a single slot: the grid
    // just changed size, and how many casters shared this projector before says
    // nothing about the new one.
    if (p->nodes) { free_vectors(p->nodes); p->nodes = NULL; }
    if (p->nodeNormals) { free_vectors(p->nodeNormals); p->nodeNormals = NULL; }
    if (p->triNodeIdx) { free(p->triNodeIdx); p->triNodeIdx = NULL; }

    shadow_slots_release(p);

    // Allocate nodes and mapping. nodeNormals is allocated unconditionally
    // (not just when enableRaycast is currently on) since raycasting can be
    // toggled on after this rebuild -- it's cheap (nodeCount VECTORs) and
    // this only runs when the grid itself changes, not per frame.
    p->nodes = alloc_vectors(nodeCount);
    p->nodeNormals = alloc_vectors(nodeCount);
    p->triNodeIdx = (uint32_t*)malloc(sizeof(uint32_t) * vtxCount);
    p->vtxCount = vtxCount;

    p->slots = (shadow_draw_slot*)malloc(sizeof(shadow_draw_slot));
    memset(p->slots, 0, sizeof(shadow_draw_slot));
    p->slotCount = 1;
    p->slotCursor = 0;
    shadow_slot_build(p, &p->slots[0]);

    // Positions are the one thing shadow_slot_build leaves empty: the first draw
    // fills them, in local or world space depending on raycasting.
    p->localGridDirty = 1;

    // Init object and bind render data once. shadow_projector_render rebinds it
    // to whichever slot each draw lands on, and sets obj.transform itself.
    new_render_object(&p->obj, &p->slots[0].data);
}

void shadow_projector_free(ath_shadow_projector *p) {
    if (!p) return;
    if (p->nodes) { free_vectors(p->nodes); p->nodes = NULL; }
    if (p->nodeNormals) { free_vectors(p->nodeNormals); p->nodeNormals = NULL; }
    if (p->triNodeIdx) { free(p->triNodeIdx); p->triNodeIdx = NULL; }

    shadow_slots_release(p);
#ifdef ATHENA_ODE
    if (p->rayGeom) { dGeomDestroy(p->rayGeom); p->rayGeom = NULL; }
#endif
}


