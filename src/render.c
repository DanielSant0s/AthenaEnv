#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <matrix.h>
#include <render.h>
#include <dbgprintf.h>

#include <owl_packet.h>

#include <mpg_manager.h>

#include <texture_manager.h>

#include <vector.h>

#define DEG2RAD(deg) ((deg) * (M_PI / 180.0f))

register_vu_program(VU1Draw3DCS);
register_vu_program(VU1Draw3DLCS);
register_vu_program(VU1Draw3DLCSS);

register_vu_program(VU1Draw3DCS_Skin);
register_vu_program(VU1Draw3DLCS_Skin);
register_vu_program(VU1Draw3DLCSS_Skin);

register_vu_program(VU1Draw3DLCS_Ref);

vu_mpg *vu1_colors = NULL;
vu_mpg *vu1_lights = NULL;
vu_mpg *vu1_specular = NULL;

vu_mpg *vu1_colors_skinned = NULL;
vu_mpg *vu1_lights_skinned = NULL;
vu_mpg *vu1_specular_skinned = NULL;

vu_mpg *vu1_lights_reflection = NULL;

MATRIX view_screen;
MATRIX world_view;
MATRIX world_screen;

FIVECTOR screen_scale;

static int active_aaa_lights = 0;
static int active_bbb_lights = 0;
static int active_pnt_lights = 0;
static int active_dir_lights = 0;

static LightData dir_lights = { };

// Ambients pre-summed for the VU, which used to add one per light inside its
// per-vertex loop. w = 1.0 so a single lq initialises the light accumulator
// including the alpha lane. The light count cannot share that w -- see
// mem_layout.i -- so it lives in its own quadword.
static VECTOR   light_ambient_sum qw_aligned = { 0.0f, 0.0f, 0.0f, 1.0f };
static FIVECTOR light_count qw_aligned = { 0.0f, 0.0f, 0.0f, 0 };

static void render_sum_ambient(void) {
	light_ambient_sum[0] = light_ambient_sum[1] = light_ambient_sum[2] = 0.0f;

	for (int i = 0; i < active_dir_lights; i++) {
		light_ambient_sum[0] += dir_lights.ambient[i].x;
		light_ambient_sum[1] += dir_lights.ambient[i].y;
		light_ambient_sum[2] += dir_lights.ambient[i].z;
	}
}
static render_stats_t g_render_stats = { 0 };

// View block: screen_scale (0), world_screen (1..4), camera position (9). Lives
// outside the double-buffered window (use_top=0), so VU1 keeps it across draws
// and frames -- upload only when it changes, not once per object per pass.
// Invalidated by render_set_view, cameraUpdate, tile_render (overwrites 0..1),
// and render_begin (once a frame, in case something else touches VU1 static).
static uint32_t g_view_version  = 1;
static uint32_t g_view_resident = 0;

// Quadwords render_upload_view() is about to emit -- callers need this before
// they can size their owl_query_packet() reservation.
static inline int render_view_qwc(void) {
	return (g_view_resident != g_view_version) ? 9 : 0;
}

// Emits the view block if VU1 no longer holds the current one. Address 9
// (camera position) goes out even for pipelines whose microprogram never reads
// it: uploading all three together keeps ONE version counter honest. Tracking
// them separately would let a colors draw mark the block resident and make the
// next lights draw skip a camera position it never actually sent.
static void render_upload_view(owl_packet *packet) {
	if (g_view_resident == g_view_version)
		return;

	owl_add_unpack_data_cnt(packet, 0, 1, 0);
	owl_add_uquad_ptr(packet, &screen_scale);

	owl_add_unpack_data_cnt(packet, 1, 4, 0);
	owl_add_uquad_ptr(packet, &(world_screen[0]));
	owl_add_uquad_ptr(packet, &(world_screen[4]));
	owl_add_uquad_ptr(packet, &(world_screen[8]));
	owl_add_uquad_ptr(packet, &(world_screen[12]));

	owl_add_unpack_data_cnt(packet, 9, 1, 0);
	owl_add_uquad_ptr(packet, getCameraPosition());

	g_view_resident = g_view_version;
}

void render_invalidate_vu_view(void) {
	g_view_version++;
}

static inline uint32_t render_calc_triangles(const athena_render_data *data) {
	if (!data)
		return 0;

	if (data->tristrip) {
		if (data->index_count < 2)
			return 0;
		return data->index_count - 2;
	}

	return data->index_count / 3;
}

void render_init() {
	initCamera(&world_screen, &world_view, &view_screen);
	
	// One window for both layouts, starting where the bone matrices used to be
	// (they moved to 880). 369 fits the larger of the two footprints: skinned
	// 2 + 40*6 + 3 + 1 + 40*3 = 366. Global VIF1 state, shared by all pipelines.
	vu1_set_double_buffer_settings(141, 369);

	vu1_colors   = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DCS),   embed_vu_code_size(VU1Draw3DCS),   VECTOR_UNIT_1, false); 
	vu1_lights   = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCS),  embed_vu_code_size(VU1Draw3DLCS),  VECTOR_UNIT_1, false);
	vu1_specular = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCSS), embed_vu_code_size(VU1Draw3DLCSS), VECTOR_UNIT_1, false);

	vu1_colors_skinned = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DCS_Skin), embed_vu_code_size(VU1Draw3DCS_Skin), VECTOR_UNIT_1, false);
	vu1_lights_skinned = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCS_Skin), embed_vu_code_size(VU1Draw3DLCS_Skin), VECTOR_UNIT_1, false);
	vu1_specular_skinned = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCSS_Skin), embed_vu_code_size(VU1Draw3DLCSS_Skin), VECTOR_UNIT_1, false);

	vu1_lights_reflection = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCS_Ref), embed_vu_code_size(VU1Draw3DLCS_Ref), VECTOR_UNIT_1, false);
}

void render_begin() {
	// One window for both layouts, starting where the bone matrices used to be
	// (they moved to 880). 369 fits the larger of the two footprints: skinned
	// 2 + 40*6 + 3 + 1 + 40*3 = 366. Global VIF1 state, shared by all pipelines.
	vu1_set_double_buffer_settings(141, 369);

	render_reset_stats();

	render_sum_ambient();

	// Cheap insurance, not a correctness requirement: VU1 static memory does
	// survive a frame boundary, so in principle the view block resident from
	// last frame is still good. Re-uploading once a frame costs 9 quadwords and
	// bounds the damage from any future code path that writes VU1 static memory
	// without calling render_invalidate_vu_view() -- a class of bug that would
	// otherwise show up as subtly wrong geometry with no obvious cause.
	render_invalidate_vu_view();

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 17);

	owl_add_unpack_data_cnt(packet, 10, 16, 0);
	owl_add_uquad_ptr(packet, (dir_lights.direction[0]));
	owl_add_uquad_ptr(packet, (dir_lights.direction[1]));
	owl_add_uquad_ptr(packet, (dir_lights.direction[2]));
	owl_add_uquad_ptr(packet, (dir_lights.direction[3]));
	owl_add_uquad_ptr(packet, &light_ambient_sum);
	owl_add_uquad_ptr(packet, &light_count);
	owl_add_uquad_ptr(packet, &light_count);   // 16..17 unused, see mem_layout.i
	owl_add_uquad_ptr(packet, &light_count);
	owl_add_uquad_ptr(packet, (dir_lights.diffuse[0]));
	owl_add_uquad_ptr(packet, (dir_lights.diffuse[1]));
	owl_add_uquad_ptr(packet, (dir_lights.diffuse[2]));
	owl_add_uquad_ptr(packet, (dir_lights.diffuse[3]));
	owl_add_uquad_ptr(packet, (dir_lights.specular[0]));
	owl_add_uquad_ptr(packet, (dir_lights.specular[1]));
	owl_add_uquad_ptr(packet, (dir_lights.specular[2]));
	owl_add_uquad_ptr(packet, (dir_lights.specular[3]));
}

void render_set_view(float fov, float near, float far, float width, float height) {
	if (width == 0.0f && height == 0.0f) {
		width = gsGlobal->Width;
		height = gsGlobal->Height * ((gsGlobal->Interlace == GS_INTERLACED) && (gsGlobal->Field == GS_FRAME)? 2 : 1);
	}

	create_view(view_screen, DEG2RAD(fov), near, far, width, height);
	matrix_functions->multiply(world_screen, world_view, view_screen);

	screen_scale.x = width/2;
	screen_scale.y = height/2;
	screen_scale.z = ((float)get_max_z(gsGlobal)) / 2.0f;
	screen_scale.w = 0;

	// Both halves of the view block just changed.
	render_invalidate_vu_view();
}

int NewLight() {
	if (active_dir_lights < 4) {
		light_count.w = active_dir_lights+1;
		owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 2);
		owl_add_unpack_data_cnt(packet, 15, 1, 0);
		owl_add_uquad_ptr(packet, &light_count);
		return active_dir_lights++;
	}
		
	return -1;
}
 
void SetLightAttribute(int id, float x, float y, float z, int attr) {
	if (id < 0)
		return;

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 2);

	switch (attr) {
		case ATHENA_LIGHT_DIRECTION:
			dir_lights.direction[id][0] = x;
			dir_lights.direction[id][1] = y;
			dir_lights.direction[id][2] = z;
			owl_add_unpack_data_cnt(packet, 10+id, 1, 0);
			owl_add_uquad_ptr(packet, (dir_lights.direction[id]));
			break;
		case ATHENA_LIGHT_AMBIENT:
			dir_lights.ambient[id].x = x;
			dir_lights.ambient[id].y = y;
			dir_lights.ambient[id].z = z;
			render_sum_ambient();
			owl_add_unpack_data_cnt(packet, 14, 1, 0);
			owl_add_uquad_ptr(packet, &light_ambient_sum);
			break;
		case ATHENA_LIGHT_DIFFUSE:
			dir_lights.diffuse[id][0] = x;
			dir_lights.diffuse[id][1] = y;
			dir_lights.diffuse[id][2] = z;
			owl_add_unpack_data_cnt(packet, 18+id, 1, 0);
			owl_add_uquad_ptr(packet, (dir_lights.diffuse[id]));
			break;
		case ATHENA_LIGHT_SPECULAR:
			dir_lights.specular[id][0] = x;
			dir_lights.specular[id][1] = y;
			dir_lights.specular[id][2] = z;
			owl_add_unpack_data_cnt(packet, 22+id, 1, 0);
			owl_add_uquad_ptr(packet, (dir_lights.specular[id]));
			break;
	}
}

const render_stats_t *render_get_stats(void) {
	return &g_render_stats;
}

void render_reset_stats(void) {
	memset(&g_render_stats, 0, sizeof(g_render_stats));
}


VECTOR zero_bump_offset = { 0.0f, 0.0f, 0.0f, 0.0f };

// Defined below, but render_object() (above them) drives the skeleton now.
void process_animation(athena_object_data *obj);

void draw_vu1_with_colors(athena_object_data *obj, int pass_state);
void draw_vu1_with_lights(athena_object_data *obj, int pass_state);
void draw_vu1_with_spec_lights(athena_object_data *obj, int pass_state);
void draw_vu1_with_lights_ref(athena_object_data *obj, int pass_state);

void (*render_funcs[])(athena_object_data *obj, int pass_state) = {
	draw_vu1_with_colors,
	draw_vu1_with_lights,
	draw_vu1_with_spec_lights
};

// A skinned mesh draws sum(w_i * BoneMatrix_i * v), so the bind-pose
// bounding_box is not what reaches the screen -- up close, a slightly wrong box
// reads as "entirely past the near plane" and the object vanishes.
//
// Rebuild it from the live bone palette: transform the bind box by every bone
// matrix, take the AABB of the union. Conservative for any animation, since the
// skinned vertex is a convex combination of points each inside one M_i(box).
// Corner-by-corner, not centre+extent: the latter only holds for rigid matrices
// and would under-cover a skeleton carrying scale.
static void render_update_skinned_bounds(athena_object_data *obj) {
	athena_render_data *data = obj->data;
	uint32_t bone_count = data->skeleton->bone_count;

	if (!obj->skinned_bounds || bone_count == 0)
		return;

	VECTOR lo, hi;
	int seeded = 0;

	for (uint32_t b = 0; b < bone_count; b++) {
		for (int c = 0; c < 8; c++) {
			VECTOR p;
			matrix_functions->apply(p, obj->bone_matrices[b], data->bounding_box[c]);

			if (!seeded) {
				lo[0] = hi[0] = p[0];
				lo[1] = hi[1] = p[1];
				lo[2] = hi[2] = p[2];
				seeded = 1;
				continue;
			}

			for (int k = 0; k < 3; k++) {
				if (p[k] < lo[k]) lo[k] = p[k];
				if (p[k] > hi[k]) hi[k] = p[k];
			}
		}
	}

	if (!seeded)
		return;

	// Same corner ordering calculate_bbox() uses; clip_bounding_box() only
	// cares that all 8 are present, but keeping them consistent means the two
	// boxes stay interchangeable for anything added later.
	const float xs[8] = { lo[0], lo[0], lo[0], lo[0], hi[0], hi[0], hi[0], hi[0] };
	const float ys[8] = { lo[1], lo[1], hi[1], hi[1], lo[1], lo[1], hi[1], hi[1] };
	const float zs[8] = { lo[2], hi[2], lo[2], hi[2], lo[2], hi[2], lo[2], hi[2] };

	for (int c = 0; c < 8; c++) {
		obj->skinned_bounds[c][0] = xs[c];
		obj->skinned_bounds[c][1] = ys[c];
		obj->skinned_bounds[c][2] = zs[c];
		obj->skinned_bounds[c][3] = 1.0f;
	}
}

// Transforms the bounding box's 8 corners by (object * world_screen) on VU0 and
// ANDs their CLIP flags: non-zero means all 8 violate the SAME plane, so the
// whole (convex) box is off-screen. One-sided -- a box outside two planes but
// neither alone is reported visible. The exact test costs more than it saves.
int render_object_in_frustum(athena_object_data *obj) {
	if (!obj || !obj->data || !obj->frustum_cull)
		return 1;

	// skinned_bounds is non-NULL only for skeletal meshes, where it holds the
	// animated box render_update_skinned_bounds() rebuilt from this frame's
	// bone palette. Everything else has no bone matrices between its positions
	// and the screen, so its bind-time box is already the right thing to test.
	VECTOR *bounds = obj->skinned_bounds ? obj->skinned_bounds : obj->data->bounding_box;

	// A never-computed bounding box is all zeroes, which survives the transform
	// as (0,0,0,0) and trips no CLIP flag at all -- the AND collapses to 0 and
	// the object is reported visible. That fail-open is the intended behaviour
	// for geometry whose bounds nobody filled in (see calculate_bbox's
	// callers); culling must never be the reason something silently stops
	// rendering.
	MATRIX local_screen;
	matrix_functions->multiply(local_screen, obj->transform, world_screen);

	return !clip_bounding_box(local_screen, bounds);
}

void render_object(athena_object_data *obj) {
	if (!obj || !obj->data)
		return;

	// Physics still has to advance for culled objects: it is game state, not
	// drawing. Skipping it off-screen would make bodies freeze the moment the
	// camera looks away.
	if (obj->update_physics)
		obj->update_physics(obj);

	// Advance the skeleton BEFORE the cull test, because the test's bounds are
	// derived from the bone palette (render_update_skinned_bounds). This used
	// to live inside every draw_vu1_* function, which meant an object drawn in
	// several passes -- base, then decal, then reflection, then bump twice --
	// re-evaluated its animation and rebuilt every bone matrix once per pass.
	// Now it happens once per frame per object, and a culled object skips it
	// entirely (process_animation() is driven by wall clock, so it picks up
	// again correctly whenever the object comes back into view).
	if (obj->data->skeleton && obj->bone_matrices) {
		process_animation(obj);
		update_bone_transforms(obj);
		render_update_skinned_bounds(obj);
	}

	if (!render_object_in_frustum(obj)) {
		g_render_stats.objects_culled++;
		return;
	}

	g_render_stats.draw_calls++;
	g_render_stats.triangles += render_calc_triangles(obj->data);

	uint64_t old_alpha = get_screen_param(ALPHA_BLEND_EQUATION);
	uint64_t old_colclamp = get_screen_param(COLOR_CLAMP_MODE);

	render_funcs[obj->data->pipeline](obj, 0);

	if (obj->data->attributes.has_decal) {
		render_funcs[obj->data->pipeline](obj, 2);
	}
	
	if (obj->data->attributes.has_refmap) {
		set_screen_param(ALPHA_BLEND_EQUATION, ALPHA_EQUATION(SRC_RGB, ZERO_RGB, ALPHA_FIX, DST_RGB, 0x40));

		draw_vu1_with_lights_ref(obj, 0);
	}

	if (obj->data->attributes.has_bumpmap) {
    	VECTOR light_dir;

		matrix_functions->apply(light_dir, obj->transform, dir_lights.direction[0]);

		vector_functions->normalize(light_dir, light_dir);
		
    	obj->bump_offset[0] = light_dir[0] * 0.008f;
    	obj->bump_offset[1] = light_dir[1] * 0.008f;

		set_screen_param(COLOR_CLAMP_MODE, 1);

		obj->bump_offset_buffer = &obj->bump_offset;
		set_screen_param(ALPHA_BLEND_EQUATION, ALPHA_EQUATION(SRC_RGB, ZERO_RGB, ALPHA_FIX, DST_RGB, 0x34));
		draw_vu1_with_colors(obj, 1);

		obj->bump_offset_buffer = &zero_bump_offset;
		set_screen_param(ALPHA_BLEND_EQUATION, ALPHA_EQUATION(ZERO_RGB, SRC_RGB, ALPHA_FIX, DST_RGB, 0x34));
		draw_vu1_with_colors(obj, 1);
	}	

	set_screen_param(COLOR_CLAMP_MODE, old_colclamp);
	set_screen_param(ALPHA_BLEND_EQUATION, old_alpha);
}

void new_render_object(athena_object_data *obj, athena_render_data *data) {
	obj->data = data;

	// Compact vertex cache (render_cook_compact_vertices): free whatever a
	// previous life of this render_data cooked (e.g. shadows.c rebuilding
	// its projector geometry with a new vertex count) and reset so the next
	// draw call reallocates at the current index_count.
	//
	// Skip all of this if already frozen: a frozen object's positions/
	// normals/texcoords/colours are NULL (freed by render_freeze_compact_
	// vertices) and its compact cache is the only valid copy of the
	// geometry left. Resetting compact_dirty/frozen here regardless of
	// that would make the next draw call's cook loop dereference the
	// (still-NULL) float source -- e.g. wrapping an already-frozen
	// RenderData in a second RenderObject, or freezing before the first
	// `new RenderObject(...)` call finishes constructing it.
	if (!data->frozen) {
		free(data->compact_positions);
		free(data->compact_normals);
		free(data->compact_colors);
		free(data->compact_uvs);
		data->compact_positions = NULL;
		data->compact_normals = NULL;
		data->compact_colors = NULL;
		data->compact_uvs = NULL;
		data->compact_capacity = 0;
		data->compact_dirty = RENDER_DIRTY_ALL;

		// The padded group layout describes the buffers just freed, and is
		// rebuilt by the next cook from the current material table.
		free(data->compact_group_base);
		data->compact_group_base = NULL;
		data->compact_group_count = 0;
	}

	// Baked DMA_CALL chains: independent of the frozen float source, so reset
	// unconditionally -- covers a previous life of this render_data (e.g.
	// shadows.c rebuilding geometry with a different chunk count).
	athena_chain_cache *slots[10] = {
		&data->chain[0][0], &data->chain[0][1], &data->chain[0][2],
		&data->chain[1][0], &data->chain[1][1], &data->chain[1][2],
		&data->chain[2][0], &data->chain[2][1], &data->chain[2][2],
		&data->ref_chain
	};
	for (int i = 0; i < 10; i++) {
		free(slots[i]->buffer);
		free(slots[i]->chunk_offset);
		free(slots[i]->tex_giftag);
		slots[i]->buffer = NULL;
		slots[i]->chunk_offset = NULL;
		slots[i]->tex_giftag = NULL;
		slots[i]->qwc_alloc = 0;
		slots[i]->chunk_count = 0;
		slots[i]->mpg_addr = -1;
		slots[i]->built_version = 0;
	}
	data->chain_version = 1;

	obj->bump_offset_buffer = &zero_bump_offset;

	// On by default: it is a pure win for the static meshes that make up most
	// of a scene, and harmless (fail-open) for render_data whose bounding_box
	// was never filled in. Callers whose geometry outgrows its box at runtime
	// clear this -- see the field's doc comment in render.h.
	obj->frustum_cull = true;

	// A previous life of this athena_object_data may have been skinned
	// (shadows.c rebinds its projector object, JS rebinds RenderData): drop
	// any animated box before deciding whether this data needs one. Every
	// creator zeroes the struct first -- calloc in athena_render_object_create,
	// memset in shadow_projector_init -- so this is free(NULL) on a fresh one.
	free(obj->skinned_bounds);
	obj->skinned_bounds = NULL;

	if (data->skin_data) {
		obj->anim_controller.current = NULL;

		obj->bones = (athena_bone_transform*)malloc(data->skeleton->bone_count * sizeof(athena_bone_transform));
		obj->bone_matrices = (MATRIX*)malloc(data->skeleton->bone_count * sizeof(MATRIX));

		// Per-object, not per-render_data: two RenderObjects sharing one
		// skinned RenderData play different animations, so they cannot share
		// an animated box. memalign because clip_bounding_box() reads it with
		// lqc2.
		obj->skinned_bounds = (VECTOR*)memalign(16, 8 * sizeof(VECTOR));

		for (int i = 0; i < data->skeleton->bone_count; i++) {
			copy_vector(obj->bones[i].position, data->skeleton->bones[i].position);
			copy_vector(obj->bones[i].rotation, data->skeleton->bones[i].rotation);
			copy_vector(obj->bones[i].scale, data->skeleton->bones[i].scale);
		}

		update_bone_transforms(obj);
	}

	obj->position[0] = 0.0f;
	obj->position[1] = 0.0f;
	obj->position[2] = 0.0f;
	obj->position[3] = 1.0f;

	obj->rotation[0] = 0.0f;
	obj->rotation[1] = 0.0f;
	obj->rotation[2] = 0.0f;
	obj->rotation[3] = 1.0f;

	obj->scale[0] = 1.0f;
	obj->scale[1] = 1.0f;
	obj->scale[2] = 1.0f;
	obj->scale[3] = 1.0f;

	obj->collision = NULL;
	obj->update_collision = NULL;

	obj->physics = NULL;
	obj->update_physics = NULL;

	update_object_space(obj);
}

void draw_bbox(athena_object_data* obj, Color color) {
	

	/*VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	// Matrices to setup the 3D environment and camera
	MATRIX local_world;
	MATRIX local_screen;

	create_local_world(local_world, obj->position ,obj->rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);
	if(clip_bounding_box(local_screen, m->bounding_box)) return;

	vertex_f_t *xyz = (vertex_f_t *)memalign(128, sizeof(vertex_f_t)*8);
	calculate_vertices_clipped((VECTOR *)xyz,  8, m->bounding_box, local_screen);

	athena_line_goraud_3d(gsGlobal, xyz[0].x, xyz[0].y, xyz[0].z, xyz[1].x, xyz[1].y, xyz[1].z, color, color);
	athena_line_goraud_3d(gsGlobal, xyz[1].x, xyz[1].y, xyz[1].z, xyz[3].x, xyz[3].y, xyz[3].z, color, color);
	athena_line_goraud_3d(gsGlobal, xyz[2].x, xyz[2].y, xyz[2].z, xyz[3].x, xyz[3].y, xyz[3].z, color, color);
	athena_line_goraud_3d(gsGlobal, xyz[0].x, xyz[0].y, xyz[0].z, xyz[2].x, xyz[2].y, xyz[2].z, color, color);
	athena_line_goraud_3d(gsGlobal, xyz[4].x, xyz[4].y, xyz[4].z, xyz[5].x, xyz[5].y, xyz[5].z, color, color);
	athena_line_goraud_3d(gsGlobal, xyz[5].x, xyz[5].y, xyz[5].z, xyz[7].x, xyz[7].y, xyz[7].z, color, color);
	athena_line_goraud_3d(gsGlobal, xyz[6].x, xyz[6].y, xyz[6].z, xyz[7].x, xyz[7].y, xyz[7].z, color, color);
	athena_line_goraud_3d(gsGlobal, xyz[4].x, xyz[4].y, xyz[4].z, xyz[6].x, xyz[6].y, xyz[6].z, color, color);
	athena_line_goraud_3d(gsGlobal, xyz[0].x, xyz[0].y, xyz[0].z, xyz[4].x, xyz[4].y, xyz[4].z, color, color);
	athena_line_goraud_3d(gsGlobal, xyz[1].x, xyz[1].y, xyz[1].z, xyz[5].x, xyz[5].y, xyz[5].z, color, color);
	athena_line_goraud_3d(gsGlobal, xyz[2].x, xyz[2].y, xyz[2].z, xyz[6].x, xyz[6].y, xyz[6].z, color, color);
	athena_line_goraud_3d(gsGlobal, xyz[3].x, xyz[3].y, xyz[3].z, xyz[7].x, xyz[7].y, xyz[7].z, color, color);

	free(xyz);*/
}

void append_texture_tags(owl_packet* packet, GSSURFACE *texture, int texture_id, eColorFunctions func) {
	if (texture_id != -1) {
		owl_add_cnt_tag(packet, 4, 0); // 4 quadwords for vif
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSH, 0));
		owl_add_uint(packet, VIF_CODE(2, 0, VIF_DIRECT, 0));

		owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));
		owl_add_tag(packet, GIF_NOP, 0);

		owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
		owl_add_uint(packet, VIF_CODE(texture_id, 0, VIF_MARK, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 1));

	} 
}

void process_animation(athena_object_data *obj) {
	if (obj->anim_controller.current) {
		if (!obj->anim_controller.is_playing) {
			obj->anim_controller.initial_time = (clock()  / (float)CLOCKS_PER_SEC);

			obj->anim_controller.is_playing = true;
		}

		obj->anim_controller.current_time = (clock() / (float)CLOCKS_PER_SEC) - obj->anim_controller.initial_time;

		apply_animation(obj, obj->anim_controller.current_time); 

		if (obj->anim_controller.current_time > obj->anim_controller.current->duration) {
			if (obj->anim_controller.loop) {
				obj->anim_controller.initial_time = (clock()  / (float)CLOCKS_PER_SEC);
			} else {
				obj->anim_controller.is_playing = false;
				obj->anim_controller.current = NULL;
			}
		}
	}
}

void update_object_space(athena_object_data *obj) {
  	matrix_functions->identity(obj->transform);

  	matrix_functions->rotate(obj->transform, obj->transform, obj->rotation);
	matrix_functions->scale(obj->transform, obj->transform, obj->scale);
  	matrix_functions->translate(obj->transform, obj->transform, obj->position);

	if (obj->update_collision)
		obj->update_collision(obj);
}

static void bake_giftags(owl_packet *packet, athena_render_data *data, bool texture_mapping, int mat_id) {
	prim_reg_t prim_data = {
		.PRIM = GS_PRIM_PRIM_TRIFAN,
		.IIP = data->attributes.shade_model,
		.TME = texture_mapping,
		.FGE = gsGlobal->PrimFogEnable,
		.ABE = gsGlobal->PrimAlphaEnable,
		.AA1 = gsGlobal->PrimAAEnable,
		.FST = 0,
		.CTXT = gsGlobal->PrimContext,
		.FIX = 0
	};

	giftag_t clip_tag = {
		.NLOOP = 0,
		.EOP = 1,
		.PRE = 1,
		.PRIM = prim_data.data,
		.FLG = 0,
		.NREG = 3
	};

	prim_data.PRIM = (data->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE);

	// NLOOP carries the "kick texture state" flag to the VU, which reads it via
	// ilw.x and skips its texture XGKICK when 0. Free: every microprogram
	// overwrites word x of its output copy with the real vertex count.
	// Must ride here, not in CLIPFAN's spare lane: this unpack is double-buffered,
	// CLIPFAN is static and the VIF writes chunk N+1 while the VU runs chunk N.
	giftag_t prim_tag = {
		.NLOOP = texture_mapping ? 1 : 0,
		.EOP = 1,
		.PRE = 1,
		.PRIM = prim_data.data,
		.FLG = 0,
		.NREG = 3
	};

	owl_add_unpack_data_cnt(packet, 26, 1, 0);
	owl_add_uint(packet, 0);
	owl_add_uint(packet, data->attributes.accurate_clipping? (clip_tag.data >> 32) : 0);
	owl_add_uint(packet, data->tristrip);
	// float, not uint: the VU multiplies the triangle edge by this lane
	// (bfc_multiplier, +1 cull back / -1 cull front) and only then converts it
	// with ftoi0 to decide whether culling is on at all. Sent as an integer,
	// CULL_FACE_BACK's 1.0f arrived as the bit pattern 0x00000001 -- a denormal
	// the VU reads as ~0, which both zeroed the edge vector and made the enable
	// test see "no culling". Face culling has never actually run.
	owl_add_float(packet, data->attributes.face_culling);
	owl_add_unpack_data_cnt(packet, 0, 1, 1);
	owl_add_ulong(packet, prim_tag.data);
	owl_add_ulong(packet, DRAW_STQ2_REGLIST);
}

static inline float render_clampf(float v, float lo, float hi) {
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

void render_invalidate_compact_cache(athena_render_data *data) {
	if (data)
		data->compact_dirty |= RENDER_DIRTY_ALL;
}

void render_invalidate_compact_positions(athena_render_data *data) {
	if (data)
		data->compact_dirty |= RENDER_DIRTY_POSITIONS;
}

void render_invalidate_chain_cache(athena_render_data *data) {
	if (data)
		data->chain_version++;
}

// Cooks the compact VIF wire formats from positions/normals/colours/texcoords,
// only for the attributes marked dirty. positions/colours are assumed non-NULL
// (file-wide invariant); normals/texcoords are optional.
static void render_cook_compact_vertices(athena_render_data *data) {
	if (!data || data->index_count == 0 || data->frozen)
		return;

	// (Re)build the padded group layout whenever the material table changed
	// shape. Every group base is rounded up to a multiple of 4 elements so the
	// DMA_REF each chunk issues lands on a quadword boundary -- see
	// athena_render_data.compact_group_base for why that is mandatory rather
	// than tidy.
	if (data->material_index_count > 0 &&
	    data->compact_group_count != (uint32_t)data->material_index_count) {
		free(data->compact_group_base);
		data->compact_group_base = (uint32_t*)malloc(data->material_index_count * sizeof(uint32_t));
		data->compact_group_count = data->material_index_count;

		// Force a re-cook and a chain rebuild: every group just moved inside
		// the compact buffers, so both the cooked contents and the DMA_REF
		// addresses baked into the cached chains are stale.
		data->compact_dirty = RENDER_DIRTY_ALL;
		data->compact_capacity = 0;
		data->chain_version++;
	}

	// Total padded footprint. Computed every call (it is a handful of adds over
	// the material table, not per vertex) so the capacity check below always
	// compares against the layout actually in use.
	uint32_t padded_total = 0;
	if (data->compact_group_base) {
		int prev_end = -1;
		for (int i = 0; i < data->material_index_count; i++) {
			data->compact_group_base[i] = padded_total;
			uint32_t group_count = (uint32_t)(data->material_indices[i].end - prev_end);
			padded_total += (group_count + 3u) & ~3u;
			prev_end = data->material_indices[i].end;
		}
	} else {
		padded_total = data->index_count;
	}

	if (data->compact_capacity < padded_total) {
		uint32_t new_capacity = padded_total + 4;

		free(data->compact_positions);
		free(data->compact_normals);
		free(data->compact_colors);
		free(data->compact_uvs);

		data->compact_positions = (athena_compact_position*)memalign(16, new_capacity * sizeof(athena_compact_position));
		data->compact_normals   = (athena_compact_normal*)memalign(16, new_capacity * sizeof(athena_compact_normal));
		data->compact_colors    = (athena_compact_color*)memalign(16, new_capacity * sizeof(athena_compact_color));
		data->compact_uvs       = (athena_compact_uv*)memalign(16, new_capacity * sizeof(athena_compact_uv));

		// The cook loop below only ever writes [0, index_count); the spare
		// tail exists solely to absorb owl_add_unpack_data_ref_packed's
		// element-count rounding (up to +3). Zero it so that harmless
		// over-read never decodes as a NaN/denormal on the VU.
		memset(data->compact_positions, 0, new_capacity * sizeof(athena_compact_position));
		memset(data->compact_normals,   0, new_capacity * sizeof(athena_compact_normal));
		memset(data->compact_colors,    0, new_capacity * sizeof(athena_compact_color));
		memset(data->compact_uvs,       0, new_capacity * sizeof(athena_compact_uv));

		data->compact_capacity = new_capacity;
		data->compact_dirty |= RENDER_DIRTY_ALL;

		// The pre-baked DMA_CALL chains (draw_vu1_with_colors) embed these
		// buffers' addresses in DMA_REF tags -- a realloc here just moved
		// them, so every cached chain now points at freed memory.
		data->chain_version++;
	}

	if (!data->compact_dirty)
		return;

	bool cook_positions = data->compact_dirty & RENDER_DIRTY_POSITIONS;
	bool cook_colors     = data->compact_dirty & RENDER_DIRTY_COLORS;
	bool cook_normals    = (data->compact_dirty & RENDER_DIRTY_NORMALS) && data->normals;
	bool cook_uvs        = (data->compact_dirty & RENDER_DIRTY_UVS) && data->texcoords;

	data->compact_dirty = 0;

	if (!cook_positions && !cook_colors && !cook_normals && !cook_uvs)
		return;

	// src walks the source arrays straight through; dst follows the padded
	// layout, jumping to each group's aligned base. With no material table
	// (compact_group_base NULL) the two stay equal and this degenerates to the
	// old 1:1 copy.
	for (int g = 0; g < (data->compact_group_base ? data->material_index_count : 1); g++) {
		uint32_t src = data->compact_group_base ? (g == 0 ? 0 : (uint32_t)(data->material_indices[g-1].end + 1)) : 0;
		uint32_t dst = data->compact_group_base ? data->compact_group_base[g] : 0;
		uint32_t count = data->compact_group_base
			? (uint32_t)(data->material_indices[g].end + 1) - src
			: data->index_count;

		// A material table that runs past the vertex array (hand-edited from
		// JS, or a loader that mis-sized it) would otherwise read out of
		// bounds here.
		if (src >= data->index_count)
			continue;
		if (src + count > data->index_count)
			count = data->index_count - src;

		for (uint32_t j = 0; j < count; j++, src++, dst++) {
			if (cook_positions) {
				data->compact_positions[dst].x = data->positions[src][0];
				data->compact_positions[dst].y = data->positions[src][1];
				data->compact_positions[dst].z = data->positions[src][2];
			}

			if (cook_colors) {
				data->compact_colors[dst].r = (uint8_t)(render_clampf(data->colours[src][0], 0.0f, 1.0f) * 255.0f);
				data->compact_colors[dst].g = (uint8_t)(render_clampf(data->colours[src][1], 0.0f, 1.0f) * 255.0f);
				data->compact_colors[dst].b = (uint8_t)(render_clampf(data->colours[src][2], 0.0f, 1.0f) * 255.0f);
				data->compact_colors[dst].a = (uint8_t)(render_clampf(data->colours[src][3], 0.0f, 1.0f) * 255.0f);
			}

			if (cook_normals) {
				data->compact_normals[dst].x = (int8_t)(render_clampf(data->normals[src][0], -1.0f, 1.0f) * 127.0f);
				data->compact_normals[dst].y = (int8_t)(render_clampf(data->normals[src][1], -1.0f, 1.0f) * 127.0f);
				data->compact_normals[dst].z = (int8_t)(render_clampf(data->normals[src][2], -1.0f, 1.0f) * 127.0f);
				data->compact_normals[dst].w = 0;
			}

			if (cook_uvs) {
				data->compact_uvs[dst].u = (int16_t)(render_clampf(data->texcoords[src][0], -127.99f, 127.99f) * 256.0f);
				data->compact_uvs[dst].v = (int16_t)(render_clampf(data->texcoords[src][1], -127.99f, 127.99f) * 256.0f);
			}
		}
	}

	// These are read by the DMA controller (owl_add_unpack_data_ref_packed's
	// DMA_REF inside the baked chains), never by the EE core, so the cook above
	// has to be written back explicitly -- same reason render_build_chain syncs
	// its own buffer. Only bites geometry re-cooked at runtime (the shadow
	// projector rewrites its whole grid every draw); a mesh cooked once at load
	// is evicted by ordinary cache pressure long before it is drawn, which is
	// why this was never missed.
	if (padded_total > 0) {
		if (cook_positions) SyncDCache(data->compact_positions, &data->compact_positions[padded_total - 1]);
		if (cook_normals)   SyncDCache(data->compact_normals,   &data->compact_normals[padded_total - 1]);
		if (cook_colors)    SyncDCache(data->compact_colors,    &data->compact_colors[padded_total - 1]);
		if (cook_uvs)       SyncDCache(data->compact_uvs,       &data->compact_uvs[padded_total - 1]);
	}
}

// Origin of material_indices[i]'s slice inside the compact_* buffers.
//
// Deliberately NOT last_index+1, which is where that group lives in the SOURCE
// arrays: the compact buffers use a padded layout that keeps every group on a
// quadword boundary, because the DMA_REF a chunk hands the DMAC cannot start
// anywhere else. See athena_render_data.compact_group_base for the full
// reasoning. Falls back to the unpadded index when no layout has been built
// (no material table), which keeps the degenerate single-group case identical
// to before.
static inline uint32_t render_compact_base(const athena_render_data *data, int i, int last_index) {
	if (data->compact_group_base && i >= 0 && i < (int)data->compact_group_count)
		return data->compact_group_base[i];

	return (uint32_t)(last_index + 1);
}

void render_freeze_compact_vertices(athena_render_data *data) {
	if (!data || data->frozen)
		return;

	// Make sure the compact cache reflects the latest data before the float
	// source it was cooked from goes away for good.
	render_cook_compact_vertices(data);

	free(data->positions);
	free(data->normals);
	free(data->texcoords);
	free(data->colours);

	data->positions = NULL;
	data->normals = NULL;
	data->texcoords = NULL;
	data->colours = NULL;

	data->frozen = true;
}

// One sub-chain per material chunk: the unpacks (all DMA_REF) plus the
// FLUSHA/NOP/ITOP/MSCALF-or-MSCNT trailer. Mirrors draw_vu1_with_colors'
// chunk loop, including the last_index==-1 MSCALF-vs-MSCNT condition.

// Bakes a tex_giftag slot: AD header + two inert AD pairs. The pairs default to
// GIF_NOP because an untextured material's slot is never refreshed, and an AD
// address of 0 is GS_PRIM -- zeroed memory would write PRIM=0 to the GS.
static inline void render_bake_tex_giftag_header(owl_qword *slot) {
	owl_qword header;
	header.dword[1] = GIF_AD;
	header.dword[0] = GIFTAG(2, 1, 0, 0, 0, 1);
	slot[0] = header;

	owl_qword nop_pair;
	nop_pair.dword[1] = GIF_NOP;
	nop_pair.dword[0] = 0;
	slot[1] = nop_pair;
	slot[2] = nop_pair;
}

// Which attribute set a chain slot was baked for. colors/lights/spec share the
// same VU-mem slot numbering (colors just leaves the normals slot unwritten);
// the reflection program uses a flat layout with no skin and no UVs.
typedef enum {
	CHAIN_COLORS,   // positions, colours, uvs
	CHAIN_LIT,      // + normals  (lights and spec are identical here)
	CHAIN_REF,      // positions, normals, colours; flat offsets, skips
	                // materials without a ref texture
} eChainKind;

// Bakes one DMA_CALL sub-chain per material chunk: the unpacks (all DMA_REF)
// plus the FLUSHA/NOP/ITOP/MSCALF-or-MSCNT trailer. Mirrors the caller's chunk
// loop, including the last_index==-1 MSCALF-vs-MSCNT condition.
static void render_build_chain(athena_object_data *obj, athena_chain_cache *chain,
                               int pass_state, int batch_size, int mpg_addr,
                               eChainKind kind) {
	athena_render_data *data = obj->data;

	bool is_ref  = (kind == CHAIN_REF);
	bool skinned = data->skin_data && !is_ref;
	bool want_normals = (kind != CHAIN_COLORS);

	// Lands at texGiftagAddr: the chunk's OUTPUT window, XGKICKed standalone
	// ahead of kickAddress = texGiftagAddr+3. See RENDER_INBUF_SIZE.
	int tex_giftag_dest = data->skeleton ? RENDER_SKINNED_INBUF_SIZE : RENDER_INBUF_SIZE;

	// Slot numbering: skinned meshes spend slots 0..1 on the bone data.
	int slot0 = skinned ? 2 : 0;
	int dst_positions = is_ref ? 2 : 2 + batch_size * slot0;
	int dst_normals   = is_ref ? 2 + batch_size     : 2 + batch_size * (slot0 + 1);
	int dst_colours   = is_ref ? 2 + batch_size * 2 : 2 + batch_size * (slot0 + 2);
	int dst_uvs       = 2 + batch_size * (slot0 + 3);

	int chunk_cap = batch_size - (batch_size % 12);

	// A material the caller will skip contributes no chunks, but still advances
	// last_index -- keep both passes below in lockstep with the draw loop.
	#define CHAIN_SKIPS(i) (is_ref && data->materials[data->material_indices[i].index].ref_texture_id == -1)

	uint32_t chunk_count = 0;
	int last_index = -1;
	for (int i = 0; i < data->material_index_count; i++) {
		if (!CHAIN_SKIPS(i)) {
			int idxs_to_draw = (data->material_indices[i].end - last_index);
			while (idxs_to_draw > 0) {
				int count = idxs_to_draw < chunk_cap ? idxs_to_draw : chunk_cap;
				idxs_to_draw -= count;
				chunk_count++;
			}
		}
		last_index = data->material_indices[i].end;
	}

	chain->mpg_addr = mpg_addr;
	chain->built_version = data->chain_version;


	if (chunk_count == 0) {
		free(chain->buffer);
		free(chain->chunk_offset);
		free(chain->tex_giftag);
		chain->buffer = NULL;
		chain->chunk_offset = NULL;
		chain->tex_giftag = NULL;
		chain->qwc_alloc = 0;
		chain->chunk_count = 0;
		return;
	}

	// Worst case per chunk: tex_giftag + diffuse + skin + positions + normals +
	// colours + uvs + trailer (2) + DMA_RET = 10. Built once, so slack is free.
	uint32_t qwc_budget = chunk_count * 10;

	if (chain->qwc_alloc < qwc_budget) {
		free(chain->buffer);
		chain->buffer = (owl_qword*)memalign(16, qwc_budget * sizeof(owl_qword));
		chain->qwc_alloc = qwc_budget;
	}

	free(chain->chunk_offset);
	chain->chunk_offset = (uint32_t*)malloc(chunk_count * sizeof(uint32_t));
	chain->chunk_count = chunk_count;

	// One 3-QW slot per material, shared by all its chunks. Baked with inert
	// defaults so an untextured material's slot is well-formed even though
	// render_update_tex_giftag never touches it.
	free(chain->tex_giftag);
	chain->tex_giftag = (owl_qword*)memalign(16, data->material_index_count * 3 * sizeof(owl_qword));

	for (int i = 0; i < data->material_index_count; i++)
		render_bake_tex_giftag_header(&chain->tex_giftag[i * 3]);

	owl_packet chain_pkt = { 0 };
	chain_pkt.ptr = chain->buffer;

	uint32_t chunk_idx = 0;
	last_index = -1;

	for (int i = 0; i < data->material_index_count; i++) {
		if (CHAIN_SKIPS(i)) {
			last_index = data->material_indices[i].end;
			continue;
		}

		const ath_mat *mat = &data->materials[data->material_indices[i].index];
		bool texture_mapping = is_ref
			? true
			: (((mat->texture_id != -1) && data->attributes.texture_mapping) || pass_state);

		uint32_t base = render_compact_base(data, i, last_index);
		athena_compact_position* positions = &data->compact_positions[base];
		athena_compact_normal*   normals   = &data->compact_normals[base];
		athena_compact_color*    colours   = &data->compact_colors[base];
		athena_compact_uv*       texcoords = (texture_mapping && !is_ref) ? &data->compact_uvs[base] : NULL;
		vertex_skin_data*        skin_data = skinned ? &data->skin_data[last_index+1] : NULL;

		int idxs_to_draw = (data->material_indices[i].end - last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			int count = idxs_to_draw < chunk_cap ? idxs_to_draw : chunk_cap;

			chain->chunk_offset[chunk_idx] = (uint32_t)(chain_pkt.ptr - chain->buffer);

			owl_add_unpack_data_ref(&chain_pkt, tex_giftag_dest, &chain->tex_giftag[i * 3], 3, 1);
			owl_add_unpack_data_ref(&chain_pkt, 1, (void*)&mat->diffuse, 1, 1);

			if (skinned)
				owl_add_unpack_data_ref(&chain_pkt, 2, &skin_data[idxs_drawn], count*2, 1);

			owl_add_unpack_data_ref_packed(&chain_pkt, dst_positions, &positions[idxs_drawn], count, UNPACK_V3_32, sizeof(athena_compact_position), 1, 1);

			if (want_normals)
				owl_add_unpack_data_ref_packed(&chain_pkt, dst_normals, &normals[idxs_drawn], count, UNPACK_V4_8, sizeof(athena_compact_normal), 1, 0);

			owl_add_unpack_data_ref_packed(&chain_pkt, dst_colours, &colours[idxs_drawn], count, UNPACK_V4_8, sizeof(athena_compact_color), 1, 1);

			if (texcoords)
				owl_add_unpack_data_ref_packed(&chain_pkt, dst_uvs, &texcoords[idxs_drawn], count, UNPACK_V2_16, sizeof(athena_compact_uv), 1, 0);

			// FLUSHA waits for PATH1/2/3 to go idle. PATH3 is the load-bearing
			// one: render_trigger_texture_upload fires a VIF interrupt whose
			// handler queues the texture transfer on PATH3, running in parallel
			// with this VIF1 unpack. Without the wait, the XGKICK below can
			// reach the GS while that upload is still streaming.
			owl_add_cnt_tag(&chain_pkt, 1, owl_vif_code_double(VIF_CODE(0, 0, VIF_NOP, 0), VIF_CODE(0, 0, VIF_NOP, 0)));
			owl_add_uint(&chain_pkt, VIF_CODE(0, 0, VIF_FLUSHA, 0));
			owl_add_uint(&chain_pkt, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(&chain_pkt, VIF_CODE(count, 0, VIF_ITOP, 0));
			owl_add_uint(&chain_pkt, VIF_CODE(mpg_addr, 0, (last_index == -1? VIF_MSCALF : VIF_MSCNT), 0));

			owl_add_dma_ret(&chain_pkt);

			idxs_to_draw -= count;
			idxs_drawn += count;
			chunk_idx++;
		}

		last_index = data->material_indices[i].end;
	}

	#undef CHAIN_SKIPS

	SyncDCache(chain->buffer, &chain->buffer[qwc_budget - 1]);
	SyncDCache(chain->tex_giftag, &chain->tex_giftag[data->material_index_count * 3 - 1]);
}

// Refreshes the TEX0/TEX1 quadwords of a tex_giftag slot (the header, slot[0],
// is constant -- see render_bake_tex_giftag_header). Called from
// draw_vu1_with_colors's material loop whenever texture_mapping is true, NOT
// gated on whether the texture actually changed: gsGlobal->PrimContext flips
// every frame regardless, and this is a plain memory write (no packet/VIF
// cost), so there is nothing to gain by trying to skip it.
static void render_update_tex_giftag(owl_qword *slot, GSSURFACE *tex) {
	int tw, th;
	athena_set_tw_th(tex, &tw, &th);

	slot[1].dword[1] = GS_TEX0_1 + gsGlobal->PrimContext;
	slot[1].dword[0] = GS_SETREG_TEX0((tex->Vram & ~TRANSFER_REQUEST_MASK) / 256,
	                                   tex->TBW,
	                                   tex->PSM,
	                                   tw, th,
	                                   gsGlobal->PrimAlphaEnable,
	                                   COLOR_MODULATE,
	                                   (tex->VramClut & ~TRANSFER_REQUEST_MASK) / 256,
	                                   tex->ClutPSM,
	                                   0, 0,
	                                   tex->VramClut ? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD);

	slot[2].dword[1] = GS_TEX1_1 + gsGlobal->PrimContext;
	slot[2].dword[0] = GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0);

	// Read by the DMA controller (owl_add_unpack_data_ref's DMA_REF), not by
	// the EE core, so it needs an explicit writeback -- the CPU write above
	// can still be sitting in D-cache otherwise.
	SyncDCache(&slot[1], &slot[2]);
}

static inline void render_trigger_texture_upload(owl_packet *packet, int texture_id) {
	if (texture_id == -1)
		return;

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(
		VIF_CODE(0, 0, VIF_NOP, 1),
		VIF_CODE(texture_id, 0, VIF_MARK, 0)));
}

void draw_vu1_with_colors(athena_object_data *obj, int pass_state) {
	athena_render_data *data = obj->data;

	render_cook_compact_vertices(data);

	int batch_size = BATCH_SIZE, mpg_addr = 0;

	if (data->skeleton) {
		batch_size = BATCH_SIZE_SKINNED;

		// The skeleton was advanced once already, by render_object() -- see the
		// comment there. Objects drawn through several passes must not re-run it
		// per pass, and the cull test upstream depends on it already being done.

		mpg_addr = vu_mpg_preload(vu1_colors_skinned, true);
	} else {
		mpg_addr = vu_mpg_preload(vu1_colors, true);
	}

	// One slot per pass_state: render_object() issues base/decal/bump
	// back-to-back into the same unflushed ring. A stale mpg_addr (VU code
	// cache eviction) also forces a rebuild -- rare enough that patching just
	// the cached MSCALF/MSCNT immediates is not worth the bookkeeping.
	athena_chain_cache *chain = &data->chain[PL_NO_LIGHTS][pass_state];
	if (!chain->buffer || chain->built_version != data->chain_version
	    || chain->mpg_addr != mpg_addr) {
		render_build_chain(obj, chain, pass_state, batch_size, mpg_addr, CHAIN_COLORS);
	}

	// 8 unconditional quadwords (1 bone-matrix ref + 4+1 transform + 1+1
	// bump offset) plus the view block only when it is actually stale.
	// render_view_qwc() has to be read BEFORE render_upload_view() below,
	// which is what marks the block resident.
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 8 + render_view_qwc());

	if (obj->bone_matrices) {
		owl_add_unpack_data_ref(packet, 880, (void*)obj->bone_matrices, data->skeleton->bone_count*4, 0);
	}

	render_upload_view(packet);

	owl_add_unpack_data_cnt(packet, 5, 4, 0);
	owl_add_uquad_ptr(packet, &(obj->transform[0]));
	owl_add_uquad_ptr(packet, &(obj->transform[4]));
	owl_add_uquad_ptr(packet, &(obj->transform[8]));
	owl_add_uquad_ptr(packet, &(obj->transform[12]));

	owl_add_unpack_data_cnt(packet, 16, 1, 0);
	owl_add_uquad_ptr(packet, obj->bump_offset_buffer);

	//owl_add_end_tag(packet);

	// Never chunk against batch_size directly: it drives the VU-mem offset
	// formulas (2+batch_size*N) and must match mem_layout.i's strides.
	// chunk_cap floors to a multiple of 12 -- 4 for the unpack rounding, and
	// 3 so a chunk boundary never splits a triangle across two XGKICKs.
	int chunk_cap = batch_size - (batch_size % 12);

	int last_index = -1;
	GSSURFACE* tex = NULL;
	int texture_id;
	uint32_t chunk_idx = 0;
	for(int i = 0; i < data->material_index_count; i++) {
		bool texture_mapping = ((((data->materials[data->material_indices[i].index].texture_id != -1)) && data->attributes.texture_mapping) || pass_state);

		if (texture_mapping) {
			GSSURFACE *cur_tex = NULL;
			switch (pass_state) {
				case 1: // bump map
					cur_tex = data->textures[data->materials[data->material_indices[i].index].bump_texture_id];
					break;
				case 2: // decal
					cur_tex = data->textures[data->materials[data->material_indices[i].index].decal_texture_id];
					break;
				default:
					cur_tex = data->textures[data->materials[data->material_indices[i].index].texture_id];
			}

			if (cur_tex != tex) {
				texture_id = texture_manager_bind(gsGlobal, cur_tex, true);
				tex = cur_tex;

				owl_query_packet(CHANNEL_VIF1, 1);
				render_trigger_texture_upload(packet, texture_id);
			}

			// Refreshed every draw call that hits this material, not just on
			// rebind -- see render_update_tex_giftag's doc comment for why
			// (gsGlobal->PrimContext). Plain memory write, no packet cost.
			render_update_tex_giftag(&chain->tex_giftag[i * 3], tex);
		}

		int idxs_to_draw = (data->material_indices[i].end-last_index);

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, 5);

			int count = chunk_cap;
			if (idxs_to_draw < chunk_cap)
			{
				count = idxs_to_draw;
			}

			// bake_giftags stays dynamic: it embeds PrimContext. Everything else
			// for this chunk lives in the pre-baked chain.
			bake_giftags(packet, data, texture_mapping, i);

			owl_add_dma_call(packet, &chain->buffer[chain->chunk_offset[chunk_idx]]);

			idxs_to_draw -= count;
			chunk_idx++;
		}

		last_index = data->material_indices[i].end;
	}

	owl_query_packet(CHANNEL_VIF1, 1);

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSH, 0), VIF_CODE(0, 0, VIF_FLUSH, 0)));
}

void draw_vu1_with_lights(athena_object_data *obj, int pass_state) {
	athena_render_data *data = obj->data;

	render_cook_compact_vertices(data);

	int batch_size = BATCH_SIZE, mpg_addr = 0;

	if (data->skeleton) {
		batch_size = BATCH_SIZE_SKINNED;

		// The skeleton was advanced once already, by render_object() -- see the
		// comment there. Objects drawn through several passes must not re-run it
		// per pass, and the cull test upstream depends on it already being done.

		mpg_addr = vu_mpg_preload(vu1_lights_skinned, true);
	} else {
		mpg_addr = vu_mpg_preload(vu1_lights, true);
	}

	athena_chain_cache *chain = &data->chain[PL_DEFAULT][pass_state];
	if (!chain->buffer || chain->built_version != data->chain_version
	    || chain->mpg_addr != mpg_addr) {
		render_build_chain(obj, chain, pass_state, batch_size, mpg_addr, CHAIN_LIT);
	}

	// 7 unconditional quadwords (FLUSHE tag + 4+1 transform + 1 bone-matrix
	// ref) plus the view block only when stale. render_view_qwc() must be
	// read before render_upload_view(), which marks the block resident.
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 7 + render_view_qwc());

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSHE, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

	render_upload_view(packet);

	owl_add_unpack_data_cnt(packet, 5, 4, 0);
	owl_add_uquad_ptr(packet, &(obj->transform[0]));
	owl_add_uquad_ptr(packet, &(obj->transform[4]));
	owl_add_uquad_ptr(packet, &(obj->transform[8]));
	owl_add_uquad_ptr(packet, &(obj->transform[12]));

	if (obj->bone_matrices) {
		owl_add_unpack_data_ref(packet, 880, (void*)obj->bone_matrices, data->skeleton->bone_count*4, 0);
	}

	// Never chunk against batch_size directly: it drives the VU-mem offset
	// formulas (2+batch_size*N) and must match mem_layout.i's strides.
	// chunk_cap floors to a multiple of 12 -- 4 for the unpack rounding, and
	// 3 so a chunk boundary never splits a triangle across two XGKICKs.
	int chunk_cap = batch_size - (batch_size % 12);

	int last_index = -1;
	GSSURFACE* tex = NULL;
	int texture_id;
	uint32_t chunk_idx = 0;
	for(int i = 0; i < data->material_index_count; i++) {
		bool texture_mapping = (((data->materials[data->material_indices[i].index].texture_id != -1) && data->attributes.texture_mapping) || pass_state);

		if (texture_mapping) {
			GSSURFACE *cur_tex = NULL;
			switch (pass_state) {
				case 1: // bump map 
					cur_tex = data->textures[data->materials[data->material_indices[i].index].bump_texture_id];
					break;
				case 2: // decal
					cur_tex = data->textures[data->materials[data->material_indices[i].index].decal_texture_id];
					break;
				default: 
					cur_tex = data->textures[data->materials[data->material_indices[i].index].texture_id];
			}

			if (cur_tex != tex) {
				texture_id = texture_manager_bind(gsGlobal, cur_tex, true);
				tex = cur_tex;

				// Hoisted out of the chunk loop: one MARK+IRQ per texture change
				// instead of one per chunk.
				owl_query_packet(CHANNEL_VIF1, 1);
				render_trigger_texture_upload(packet, texture_id);
			}

			// Refreshed per draw, not per rebind: PrimContext flips every frame.
			render_update_tex_giftag(&chain->tex_giftag[i * 3], tex);
		}

		int idxs_to_draw = (data->material_indices[i].end-last_index);

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, 5);

			int count = chunk_cap;
			if (idxs_to_draw < chunk_cap)
			{
				count = idxs_to_draw;
			}

			// bake_giftags stays dynamic: it embeds PrimContext. Everything else
			// for this chunk lives in the pre-baked chain.
			bake_giftags(packet, data, texture_mapping, i);

			owl_add_dma_call(packet, &chain->buffer[chain->chunk_offset[chunk_idx]]);

			idxs_to_draw -= count;
			chunk_idx++;
		}

		last_index = data->material_indices[i].end;
	}

	owl_query_packet(CHANNEL_VIF1, 1);

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSH, 0), VIF_CODE(0, 0, VIF_FLUSH, 0)));
}

void draw_vu1_with_spec_lights(athena_object_data *obj, int pass_state) {
	athena_render_data *data = obj->data;

	render_cook_compact_vertices(data);

	int batch_size = BATCH_SIZE, mpg_addr = 0;

	if (data->skeleton) {
		batch_size = BATCH_SIZE_SKINNED;

		// The skeleton was advanced once already, by render_object() -- see the
		// comment there. Objects drawn through several passes must not re-run it
		// per pass, and the cull test upstream depends on it already being done.

		mpg_addr = vu_mpg_preload(vu1_specular_skinned, true);
	} else {
		mpg_addr = vu_mpg_preload(vu1_specular, true);
	}

	athena_chain_cache *chain = &data->chain[PL_SPECULAR][pass_state];
	if (!chain->buffer || chain->built_version != data->chain_version
	    || chain->mpg_addr != mpg_addr) {
		render_build_chain(obj, chain, pass_state, batch_size, mpg_addr, CHAIN_LIT);
	}

	// 7 unconditional quadwords (FLUSHE tag + 4+1 transform + 1 bone-matrix
	// ref) plus the view block only when stale. render_view_qwc() must be
	// read before render_upload_view(), which marks the block resident.
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 7 + render_view_qwc());

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSHE, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

	render_upload_view(packet);

	owl_add_unpack_data_cnt(packet, 5, 4, 0);
	owl_add_uquad_ptr(packet, &(obj->transform[0]));
	owl_add_uquad_ptr(packet, &(obj->transform[4]));
	owl_add_uquad_ptr(packet, &(obj->transform[8]));
	owl_add_uquad_ptr(packet, &(obj->transform[12]));

	if (obj->bone_matrices) {
		owl_add_unpack_data_ref(packet, 880, (void*)obj->bone_matrices, data->skeleton->bone_count*4, 0);
	}

	//owl_add_end_tag(packet);

	// Never chunk against batch_size directly: it drives the VU-mem offset
	// formulas (2+batch_size*N) and must match mem_layout.i's strides.
	// chunk_cap floors to a multiple of 12 -- 4 for the unpack rounding, and
	// 3 so a chunk boundary never splits a triangle across two XGKICKs.
	int chunk_cap = batch_size - (batch_size % 12);

	int last_index = -1;
	GSSURFACE* tex = NULL;
	int texture_id;
	uint32_t chunk_idx = 0;
	for(int i = 0; i < data->material_index_count; i++) {
		bool texture_mapping = (((data->materials[data->material_indices[i].index].texture_id != -1) && data->attributes.texture_mapping) || pass_state);

		if (texture_mapping) {
			GSSURFACE *cur_tex = NULL;
			switch (pass_state) {
				case 1: // bump map 
					cur_tex = data->textures[data->materials[data->material_indices[i].index].bump_texture_id];
					break;
				case 2: // decal
					cur_tex = data->textures[data->materials[data->material_indices[i].index].decal_texture_id];
					break;
				default: 
					cur_tex = data->textures[data->materials[data->material_indices[i].index].texture_id];
			}

			if (cur_tex != tex) {
				texture_id = texture_manager_bind(gsGlobal, cur_tex, true);
				tex = cur_tex;

				// Hoisted out of the chunk loop: one MARK+IRQ per texture change
				// instead of one per chunk.
				owl_query_packet(CHANNEL_VIF1, 1);
				render_trigger_texture_upload(packet, texture_id);
			}

			// Refreshed per draw, not per rebind: PrimContext flips every frame.
			render_update_tex_giftag(&chain->tex_giftag[i * 3], tex);
		}

		int idxs_to_draw = (data->material_indices[i].end-last_index);

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, 5);

			int count = chunk_cap;
			if (idxs_to_draw < chunk_cap)
			{
				count = idxs_to_draw;
			}

			// bake_giftags stays dynamic: it embeds PrimContext. Everything else
			// for this chunk lives in the pre-baked chain.
			bake_giftags(packet, data, texture_mapping, i);

			owl_add_dma_call(packet, &chain->buffer[chain->chunk_offset[chunk_idx]]);

			idxs_to_draw -= count;
			chunk_idx++;
		}

		last_index = data->material_indices[i].end;
	}

	owl_query_packet(CHANNEL_VIF1, 1);

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSH, 0), VIF_CODE(0, 0, VIF_FLUSH, 0)));
}

void draw_vu1_with_lights_ref(athena_object_data *obj, int pass_state) {
	athena_render_data *data = obj->data;

	render_cook_compact_vertices(data);

	int batch_size = BATCH_SIZE, mpg_addr = 0;

	if (data->skeleton) {
		batch_size = BATCH_SIZE_SKINNED;

		// The skeleton was advanced once already, by render_object() -- see the
		// comment there. Objects drawn through several passes must not re-run it
		// per pass, and the cull test upstream depends on it already being done.

		mpg_addr = vu_mpg_preload(vu1_lights_reflection, true);
	} else {
		mpg_addr = vu_mpg_preload(vu1_lights_reflection, true);
	}

	athena_chain_cache *chain = &data->ref_chain;
	if (!chain->buffer || chain->built_version != data->chain_version
	    || chain->mpg_addr != mpg_addr) {
		render_build_chain(obj, chain, pass_state, batch_size, mpg_addr, CHAIN_REF);
	}
		
	

	// 7 unconditional quadwords (FLUSHE tag + 4+1 transform + 1 bone-matrix
	// ref) plus the view block only when stale. render_view_qwc() must be
	// read before render_upload_view(), which marks the block resident.
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 7 + render_view_qwc());

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSHE, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

	render_upload_view(packet);

	owl_add_unpack_data_cnt(packet, 5, 4, 0);
	owl_add_uquad_ptr(packet, &(obj->transform[0]));
	owl_add_uquad_ptr(packet, &(obj->transform[4]));
	owl_add_uquad_ptr(packet, &(obj->transform[8]));
	owl_add_uquad_ptr(packet, &(obj->transform[12]));

	if (obj->bone_matrices) {
		owl_add_unpack_data_ref(packet, 880, (void*)obj->bone_matrices, data->skeleton->bone_count*4, 0);
	}

	//owl_add_end_tag(packet);

	// Never chunk against batch_size directly: it drives the VU-mem offset
	// formulas (2+batch_size*N) and must match mem_layout.i's strides.
	// chunk_cap floors to a multiple of 12 -- 4 for the unpack rounding, and
	// 3 so a chunk boundary never splits a triangle across two XGKICKs.
	int chunk_cap = batch_size - (batch_size % 12);

	int last_index = -1;
	GSSURFACE* tex = NULL;
	int texture_id;
	uint32_t chunk_idx = 0;
	for(int i = 0; i < data->material_index_count; i++) {
		bool texture_mapping = ((data->materials[data->material_indices[i].index].ref_texture_id != -1));

		if (texture_mapping) {
			GSSURFACE *cur_tex = data->textures[data->materials[data->material_indices[i].index].ref_texture_id];
			if (cur_tex != tex) {
				texture_id = texture_manager_bind(gsGlobal, cur_tex, true);
				tex = cur_tex;

				// Hoisted out of the chunk loop: one MARK+IRQ per texture change
				// instead of one per chunk.
				owl_query_packet(CHANNEL_VIF1, 1);
				render_trigger_texture_upload(packet, texture_id);
			}
		} else {
			// last_index must still advance past this material: it is the running
			// cursor into the vertex arrays, not a "last drawn" marker.
			last_index = data->material_indices[i].end;
			continue;
		}

		int idxs_to_draw = (data->material_indices[i].end-last_index);

		// Refreshed per draw, not per rebind: PrimContext flips every frame.
		render_update_tex_giftag(&chain->tex_giftag[i * 3], tex);

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, 5);

			int count = chunk_cap;
			if (idxs_to_draw < chunk_cap)
			{
				count = idxs_to_draw;
			}

			// bake_giftags stays dynamic: it embeds PrimContext. Everything else
			// for this chunk lives in the pre-baked chain.
			bake_giftags(packet, data, texture_mapping, i);

			owl_add_dma_call(packet, &chain->buffer[chain->chunk_offset[chunk_idx]]);

			idxs_to_draw -= count;
			chunk_idx++;
		}

		last_index = data->material_indices[i].end;
	}

	owl_query_packet(CHANNEL_VIF1, 1);

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSH, 0), VIF_CODE(0, 0, VIF_FLUSH, 0)));
}
