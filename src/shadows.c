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
    p->triNodeIdx = NULL;
    p->vtxCount = 0;

    memset(&p->data, 0, sizeof(p->data));
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
}

void shadow_projector_set_grid(ath_shadow_projector *p, int gx, int gz) {
	p->gridX = gx < 2? 2 : gx;
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

void shadow_projector_render(ath_shadow_projector *p) {
    const int gx = p->gridX;
    const int gz = p->gridZ;
    athena_render_data *data = &p->data;

    // Update texture pointer (in case it changed)
    if (data->textures && data->texture_count > 0)
        data->textures[0] = p->texture;

    // Build grid in local projector space (X,Z plane), then transform to world node positions
	const float halfW = p->width * 0.5f;
	const float halfH = p->height * 0.5f;

    const int nodeCount = gx * gz;
    VECTOR *nodeNormals = NULL;
    if (p->enableRaycast) nodeNormals = alloc_vectors(nodeCount);

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

    // Update object transform - keep position as set by JavaScript, update transform matrix
    //for (int r = 0; r < 16; r++) p->obj.transform[r] = p->transform[r];

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

	set_screen_param(ALPHA_BLEND_EQUATION, old_alpha);

    if (nodeNormals) free_vectors(nodeNormals);
}

void shadow_projector_rebuild_geometry(ath_shadow_projector *p) {
    const int gx = p->gridX;
    const int gz = p->gridZ;
    const int quadCount = (gx - 1) * (gz - 1);
    const int triCount = quadCount * 2;
    const int vtxCount = triCount * 3;
    const int nodeCount = gx * gz;

    // Free previous allocations
    if (p->nodes) { free_vectors(p->nodes); p->nodes = NULL; }
    if (p->triNodeIdx) { free(p->triNodeIdx); p->triNodeIdx = NULL; }

    athena_render_data *data = &p->data;
    if (data->positions) { free_vectors(data->positions); data->positions = NULL; }
    if (data->texcoords) { free_vectors(data->texcoords); data->texcoords = NULL; }
    if (data->colours) { free_vectors(data->colours); data->colours = NULL; }
    if (data->materials) { free(data->materials); data->materials = NULL; }
    if (data->material_indices) { free(data->material_indices); data->material_indices = NULL; }
    if (data->textures) { free(data->textures); data->textures = NULL; }

    // Allocate nodes and mapping
    p->nodes = alloc_vectors(nodeCount);
    p->triNodeIdx = (uint32_t*)malloc(sizeof(uint32_t) * vtxCount);
    p->vtxCount = vtxCount;

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
    data->material_indices = (material_index*)malloc(sizeof(material_index));
    data->material_index_count = 1;
    data->material_indices[0].index = 0;
    data->material_indices[0].end = vtxCount - 1;
    data->attributes.accurate_clipping = 0;
    data->attributes.face_culling = CULL_FACE_NONE;
    data->attributes.texture_mapping = 1;
    data->attributes.shade_model = SHADE_GOURAUD;
    data->attributes.has_refmap = 0;
    data->attributes.has_bumpmap = 0;
    data->attributes.has_decal = 0;
    data->tristrip = 0;

    // Precompute topology and UVs + per-vertex node mapping
    int w = gx;
    int v = 0;
    for (int j = 0; j < gz - 1; j++) {
        for (int i = 0; i < gx - 1; i++) {
            int i0 = j * w + i;
            int i1 = i0 + 1;
            int i2 = i0 + w;
            int i3 = i2 + 1;

            // tri 1: i0, i2, i1
            p->triNodeIdx[v+0] = (uint32_t)i0;
            p->triNodeIdx[v+1] = (uint32_t)i2;
            p->triNodeIdx[v+2] = (uint32_t)i1;

            float u0 = (float)i / (float)(gx - 1);
            float v0 = (float)j / (float)(gz - 1);
            float u1 = (float)(i+1) / (float)(gx - 1);
            float v1 = (float)(j+1) / (float)(gz - 1);

            float U0 = p->u0 + (p->u1 - p->u0) * u0;
            float V0 = p->v0 + (p->v1 - p->v0) * v0;
            float U1 = p->u0 + (p->u1 - p->u0) * u1;
            float V1 = p->v0 + (p->v1 - p->v0) * v1;
            data->texcoords[v+0][0] = U0; data->texcoords[v+0][1] = V0; data->texcoords[v+0][2] = 1.0f; data->texcoords[v+0][3] = 1.0f;
            data->texcoords[v+1][0] = U0; data->texcoords[v+1][1] = V1; data->texcoords[v+1][2] = 1.0f; data->texcoords[v+1][3] = 1.0f;
            data->texcoords[v+2][0] = U1; data->texcoords[v+2][1] = V0; data->texcoords[v+2][2] = 1.0f; data->texcoords[v+2][3] = 1.0f;

            for (int k = 0; k < 3; k++) {
                data->colours[v+k][0] = p->color[0];
                data->colours[v+k][1] = p->color[1];
                data->colours[v+k][2] = p->color[2];
                data->colours[v+k][3] = p->color[3];
            }
            v += 3;

            // tri 2: i1, i2, i3
            p->triNodeIdx[v+0] = (uint32_t)i1;
            p->triNodeIdx[v+1] = (uint32_t)i2;
            p->triNodeIdx[v+2] = (uint32_t)i3;

            data->texcoords[v+0][0] = U1; data->texcoords[v+0][1] = V0; data->texcoords[v+0][2] = 1.0f; data->texcoords[v+0][3] = 1.0f;
            data->texcoords[v+1][0] = U0; data->texcoords[v+1][1] = V1; data->texcoords[v+1][2] = 1.0f; data->texcoords[v+1][3] = 1.0f;
            data->texcoords[v+2][0] = U1; data->texcoords[v+2][1] = V1; data->texcoords[v+2][2] = 1.0f; data->texcoords[v+2][3] = 1.0f;

            for (int k = 0; k < 3; k++) {
                data->colours[v+k][0] = p->color[0];
                data->colours[v+k][1] = p->color[1];
                data->colours[v+k][2] = p->color[2];
                data->colours[v+k][3] = p->color[3];
            }

            v += 3;
        }
    }

    // Init object and bind render data once
    new_render_object(&p->obj, data);
    // Position will be set by JavaScript, just update transform matrix
    for (int r = 0; r < 16; r++) p->obj.transform[r] = p->transform[r];
}

void shadow_projector_free(ath_shadow_projector *p) {
    if (!p) return;
    if (p->nodes) { free_vectors(p->nodes); p->nodes = NULL; }
    if (p->triNodeIdx) { free(p->triNodeIdx); p->triNodeIdx = NULL; }
    athena_render_data *data = &p->data;
    if (data->positions) { free_vectors(data->positions); data->positions = NULL; }
    if (data->texcoords) { free_vectors(data->texcoords); data->texcoords = NULL; }
    if (data->colours) { free_vectors(data->colours); data->colours = NULL; }
    if (data->materials) { free(data->materials); data->materials = NULL; }
    if (data->material_indices) { free(data->material_indices); data->material_indices = NULL; }
    if (data->textures) { free(data->textures); data->textures = NULL; }
#ifdef ATHENA_ODE
    if (p->rayGeom) { dGeomDestroy(p->rayGeom); p->rayGeom = NULL; }
#endif
}


