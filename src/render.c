#include <stdio.h>
#include <profiler.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <float.h>
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
register_vu_program(VU1Draw3DLCS_Ref_Skin);

vu_mpg *vu1_colors = NULL;
vu_mpg *vu1_lights = NULL;
vu_mpg *vu1_specular = NULL;

vu_mpg *vu1_colors_skinned = NULL;
vu_mpg *vu1_lights_skinned = NULL;
vu_mpg *vu1_specular_skinned = NULL;

vu_mpg *vu1_lights_reflection = NULL;
vu_mpg *vu1_lights_reflection_skinned = NULL;

MATRIX view_screen;
MATRIX world_view;
MATRIX world_screen;

FIVECTOR screen_scale;

static int active_lights = 0;

static LightData scene_lights = { };

// Ambients pre-summed for the VU, which used to add one per light inside its
// per-vertex loop. w = 1.0 so a single lq initialises the light accumulator
// including the alpha lane.
static VECTOR   light_ambient_sum qw_aligned = { 0.0f, 0.0f, 0.0f, 1.0f };
static FIVECTOR dir_light_count   qw_aligned = { 0.0f, 0.0f, 0.0f, 0 };
static FIVECTOR pnt_light_count   qw_aligned = { 0.0f, 0.0f, 0.0f, 0 };
static VECTOR   dir_lights_packed_dir[4]   qw_aligned = { 0 };
static VECTOR   point_lights_packed_pos[4] qw_aligned = { 0 };
static VECTOR   point_lights_packed_dif[4] qw_aligned = { 0 };

static void render_sum_ambient(void) {
	light_ambient_sum[0] = light_ambient_sum[1] = light_ambient_sum[2] = 0.0f;

	for (int i = 0; i < active_lights; i++) {
		light_ambient_sum[0] += scene_lights.ambient[i].x;
		light_ambient_sum[1] += scene_lights.ambient[i].y;
		light_ambient_sum[2] += scene_lights.ambient[i].z;
	}
}
static render_stats_t g_render_stats = { 0 };
static uint32_t g_render_frame = 1;

// View block: screen_scale (0), world_screen (1..4). Lives
// outside the double-buffered window (use_top=0), so VU1 keeps it across draws
// and frames -- upload only when it changes, not once per object per pass.
// Invalidated by render_set_view, cameraUpdate, tile_render (overwrites 0..1),
// and render_begin (once a frame, in case something else touches VU1 static).
static uint32_t g_view_version  = 1;
static uint32_t g_view_resident = 0;

// Quadwords render_upload_view() is about to emit -- callers need this before
// they can size their owl_query_packet() reservation.
static inline int render_view_qwc(void) {
	return (g_view_resident != g_view_version) ? 7 : 0;
}

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

	g_view_resident = g_view_version;
}

void render_invalidate_vu_view(void) {
	g_view_version++;
}

static inline uint32_t render_calc_triangles(const athena_render_data *data) {
	if (!data)
		return 0;

	if (data->tristrip) {
		// One strip per material group, not one for the whole mesh: a chunk
		// carries its own GIFtag, so the primitive restarts at every group
		// boundary (which is exactly why shadow_slot_build gives each grid row
		// a group of its own). Each strip loses its first two vertices.
		uint32_t groups = data->material_index_count > 0
			? (uint32_t)data->material_index_count : 1;
		uint32_t warmup = groups * 2;

		if (data->index_count <= warmup)
			return 0;

		return data->index_count - warmup;
	}

	return data->index_count / 3;
}

void render_init() {
	initCamera(&world_screen, &world_view, &view_screen);
	
	// One window for both layouts: 342 QW fits unskinned (194 + 3 + 1 + 48*3 = 342)
	// and skinned (218 + 3 + 1 + 36*3 = 330), allowing BONE_MATRICES to start at
	// 825 (184 QW = 46 bones).
	vu1_set_double_buffer_settings(141, 342);

	vu1_colors   = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DCS),   embed_vu_code_size(VU1Draw3DCS),   VECTOR_UNIT_1, false); 
	vu1_lights   = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCS),  embed_vu_code_size(VU1Draw3DLCS),  VECTOR_UNIT_1, false);
	vu1_specular = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCSS), embed_vu_code_size(VU1Draw3DLCSS), VECTOR_UNIT_1, false);

	vu1_colors_skinned = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DCS_Skin), embed_vu_code_size(VU1Draw3DCS_Skin), VECTOR_UNIT_1, false);
	vu1_lights_skinned = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCS_Skin), embed_vu_code_size(VU1Draw3DLCS_Skin), VECTOR_UNIT_1, false);
	vu1_specular_skinned = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCSS_Skin), embed_vu_code_size(VU1Draw3DLCSS_Skin), VECTOR_UNIT_1, false);

	vu1_lights_reflection = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCS_Ref), embed_vu_code_size(VU1Draw3DLCS_Ref), VECTOR_UNIT_1, false);
	vu1_lights_reflection_skinned = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCS_Ref_Skin), embed_vu_code_size(VU1Draw3DLCS_Ref_Skin), VECTOR_UNIT_1, false);
}

static void render_sync_static_lights(void) {
	render_sum_ambient();

	int d_cnt = 0;
	int p_cnt = 0;
	VECTOR d_diff[4] qw_aligned = { 0 };
	VECTOR d_spec[4] qw_aligned = { 0 };
	VECTOR p_spec[4] qw_aligned = { 0 };

	memset(dir_lights_packed_dir, 0, sizeof(dir_lights_packed_dir));
	memset(point_lights_packed_pos, 0, sizeof(point_lights_packed_pos));
	memset(point_lights_packed_dif, 0, sizeof(point_lights_packed_dif));

	for (int i = 0; i < active_lights; i++) {
		if (scene_lights.is_point[i]) {
			copy_vector(point_lights_packed_pos[p_cnt], scene_lights.direction[i]);
			point_lights_packed_dif[p_cnt][0] = scene_lights.diffuse[i][0];
			point_lights_packed_dif[p_cnt][1] = scene_lights.diffuse[i][1];
			point_lights_packed_dif[p_cnt][2] = scene_lights.diffuse[i][2];
			point_lights_packed_dif[p_cnt][3] = scene_lights.direction[i][3];
			copy_vector(p_spec[p_cnt], scene_lights.specular[i]);
			p_cnt++;
		} else {
			copy_vector(dir_lights_packed_dir[d_cnt], scene_lights.direction[i]);
			copy_vector(d_diff[d_cnt], scene_lights.diffuse[i]);
			copy_vector(d_spec[d_cnt], scene_lights.specular[i]);
			d_cnt++;
		}
	}

	dir_light_count.w = (uint32_t)d_cnt;
	pnt_light_count.w = (uint32_t)p_cnt;

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 18);

	owl_add_unpack_data_cnt(packet, 14, 4, 0);
	owl_add_uquad_ptr(packet, &light_ambient_sum);
	owl_add_uquad_ptr(packet, &dir_light_count);
	owl_add_uquad_ptr(packet, &pnt_light_count); // 16 = BUMP_OFFSET (overwritten when needed)
	owl_add_uquad_ptr(packet, &pnt_light_count); // 17 = NUM_POINT_LIGHTS

	owl_add_unpack_data_cnt(packet, 18, 8, 0);
	for (int i = 0; i < 4; i++)
		owl_add_uquad_ptr(packet, d_diff[i]);
	for (int i = 0; i < 4; i++)
		owl_add_uquad_ptr(packet, d_spec[i]);

	owl_add_unpack_data_cnt(packet, 132, 4, 0);
	for (int i = 0; i < 4; i++)
		owl_add_uquad_ptr(packet, p_spec[i]);
}

void render_begin() {
	if (++g_render_frame == 0) g_render_frame = 1;
	vu1_set_double_buffer_settings(141, 342);

	render_reset_stats();
	render_invalidate_vu_view();
	render_sync_static_lights();
}

void render_restore_vu_lighting(void) {
	render_sync_static_lights();
}

static float g_render_near = 1.0f;
static float g_render_far  = 4000.0f;

float render_get_near(void) { return g_render_near; }
float render_get_far(void)  { return g_render_far; }

void render_set_view(float fov, float near, float far, float width, float height) {
	if (width == 0.0f && height == 0.0f) {
		width = gsGlobal->Width;
		height = gsGlobal->Height * ((gsGlobal->Interlace == GS_INTERLACED) && (gsGlobal->Field == GS_FRAME)? 2 : 1);
	}

	g_render_near = near;
	g_render_far  = far;

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
	if (active_lights < 4) {
		int id = active_lights++;
		scene_lights.is_point[id] = 0;
		scene_lights.direction[id][0] = 0.0f;
		scene_lights.direction[id][1] = 1.0f;
		scene_lights.direction[id][2] = 0.0f;
		scene_lights.direction[id][3] = 0.02f;
		render_sync_static_lights();
		return id;
	}
		
	return -1;
}
 
void SetLightAttribute(int id, float x, float y, float z, int attr) {
	if (id < 0 || id >= 4)
		return;

	switch (attr) {
		case ATHENA_LIGHT_DIRECTION:
			scene_lights.direction[id][0] = x;
			scene_lights.direction[id][1] = y;
			scene_lights.direction[id][2] = z;
			scene_lights.is_point[id] = 0;
			render_sync_static_lights();
			break;
		case ATHENA_LIGHT_POSITION:
			scene_lights.direction[id][0] = x;
			scene_lights.direction[id][1] = y;
			scene_lights.direction[id][2] = z;
			if (scene_lights.direction[id][3] <= 0.0f)
				scene_lights.direction[id][3] = 0.02f;
			scene_lights.is_point[id] = 1;
			render_sync_static_lights();
			break;
		case ATHENA_LIGHT_ATTENUATION: {
			float k = (z > 0.0f) ? z : ((y > 0.0f) ? y : x);
			if (k < 0.0f) k = 0.0f;
			scene_lights.direction[id][3] = k;
			scene_lights.is_point[id] = 1;
			render_sync_static_lights();
			break;
		}
		case ATHENA_LIGHT_AMBIENT:
			scene_lights.ambient[id].x = x;
			scene_lights.ambient[id].y = y;
			scene_lights.ambient[id].z = z;
			render_sync_static_lights();
			break;
		case ATHENA_LIGHT_DIFFUSE:
			scene_lights.diffuse[id][0] = x;
			scene_lights.diffuse[id][1] = y;
			scene_lights.diffuse[id][2] = z;
			render_sync_static_lights();
			break;
		case ATHENA_LIGHT_SPECULAR:
			scene_lights.specular[id][0] = x;
			scene_lights.specular[id][1] = y;
			scene_lights.specular[id][2] = z;
			render_sync_static_lights();
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

	// Sentinel seeds rather than "first point wins": bone_count is non-zero
	// here, so at least 8 corners always land on both.
	VECTOR lo = {  FLT_MAX,  FLT_MAX,  FLT_MAX, 0.0f };
	VECTOR hi = { -FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f };

	vu0_bounds_from_palette(lo, hi, obj->bone_matrices, bone_count, data->bounding_box);

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

static void render_object_impl(athena_object_data *obj);

void render_object(athena_object_data *obj) {
	prof_push(PROF_RENDER);
	render_object_impl(obj);
	prof_pop();
}

static void render_object_impl(athena_object_data *obj) {
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
	// Multiple cameras in one frame reuse the same palette. Rewriting it for
	// the shadow pass would force a DMA wait on the main pass's bone REF.
	if (obj->data->skeleton && obj->bone_matrices && obj->bones_updated_frame != g_render_frame) {
		prof_push(PROF_ANIM);
		process_animation(obj);
		if (obj->bones_queued && !owl_generation_read(obj->bones_queued_gen))
			owl_wait_generation(obj->bones_queued_gen);
		update_bone_transforms(obj);
		render_update_skinned_bounds(obj);
		obj->bones_updated_frame = g_render_frame;
		prof_pop();
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

	if (obj->data->attributes.texture_mapping && obj->data->attributes.has_decal) {
		render_funcs[obj->data->pipeline](obj, 2);
	}
	
	if (obj->data->attributes.texture_mapping && obj->data->attributes.has_refmap) {
		set_screen_param(ALPHA_BLEND_EQUATION, ALPHA_EQUATION(SRC_RGB, ZERO_RGB, ALPHA_FIX, DST_RGB, 0x40));

		draw_vu1_with_lights_ref(obj, 0);
	}

	if (obj->data->attributes.texture_mapping && obj->data->attributes.has_bumpmap) {
    	VECTOR light_dir;

		matrix_functions->apply(light_dir, obj->transform, scene_lights.direction[0]);

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

	owl_packet *flush_pkt = owl_query_packet(CHANNEL_VIF1, 1);
	owl_add_cnt_tag(flush_pkt, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSH, 0), VIF_CODE(0, 0, VIF_NOP, 0)));
}

// Defined further down, next to the tex_giftag refresh that shares it.
static void render_chain_wait_if_queued(athena_chain_cache *chain);

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
		free(data->compact_skin_indices);
		free(data->compact_skin_weights);
		data->compact_positions = NULL;
		data->compact_normals = NULL;
		data->compact_colors = NULL;
		data->compact_uvs = NULL;
		data->compact_skin_indices = NULL;
		data->compact_skin_weights = NULL;
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
		// These are freed, not just rewritten, and the DMAC may still be reading
		// them from a draw call queued earlier this frame (rebinding a
		// RenderData, or shadows.c rebuilding its projector geometry).
		render_chain_wait_if_queued(slots[i]);

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
	obj->bones_updated_frame = 0;

	if (data->skeleton) {
		memset(&obj->anim_controller, 0, sizeof(obj->anim_controller));

		obj->bones = (athena_bone_transform*)memalign(16, data->skeleton->bone_count * sizeof(athena_bone_transform));
		obj->bone_matrices = (MATRIX*)memalign(16, data->skeleton->bone_count * sizeof(MATRIX));

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
		render_update_skinned_bounds(obj);
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
	if (!obj || !obj->data)
		return;

	VECTOR *bounds = (obj->data->skeleton && obj->skinned_bounds)
		? obj->skinned_bounds
		: obj->data->bounding_box;

	MATRIX local_screen;
	matrix_functions->multiply(local_screen, obj->transform, world_screen);

	if (clip_bounding_box(local_screen, bounds))
		return;

	VECTOR clip_corners[8] qw_aligned;
	for (int i = 0; i < 8; i++) {
		matrix_functions->apply(clip_corners[i], local_screen, bounds[i]);
	}

	static const uint8_t edges[12][2] = {
		{0, 1}, {1, 3}, {3, 2}, {2, 0},
		{4, 5}, {5, 7}, {7, 6}, {6, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7}
	};

	const float near_w = 0.05f;
	float sx = screen_scale.x;
	float sy = screen_scale.y;
	float sz = screen_scale.z;

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 50);

	prim_reg_t line_prim = {
		.PRIM = GS_PRIM_PRIM_LINE,
		.IIP = 0,
		.TME = 0,
		.FGE = 0,
		.ABE = gsGlobal->PrimAlphaEnable,
		.AA1 = gsGlobal->PrimAAEnable,
		.FST = 0,
		.CTXT = gsGlobal->PrimContext,
		.FIX = 0
	};

	uint64_t rgbaq = (uint64_t)(color & 0xFFFFFFFFULL) | (0x3F800000ULL << 32);

	for (int e = 0; e < 12; e++) {
		VECTOR a, b;
		copy_vector(a, clip_corners[edges[e][0]]);
		copy_vector(b, clip_corners[edges[e][1]]);

		if (a[3] < near_w && b[3] < near_w)
			continue;

		if (a[3] < near_w) {
			float t = (near_w - a[3]) / (b[3] - a[3]);
			a[0] = a[0] + (b[0] - a[0]) * t;
			a[1] = a[1] + (b[1] - a[1]) * t;
			a[2] = a[2] + (b[2] - a[2]) * t;
			a[3] = near_w;
		} else if (b[3] < near_w) {
			float t = (near_w - b[3]) / (a[3] - b[3]);
			b[0] = b[0] + (a[0] - b[0]) * t;
			b[1] = b[1] + (a[1] - b[1]) * t;
			b[2] = b[2] + (a[2] - b[2]) * t;
			b[3] = near_w;
		}

		float inv_wa = 1.0f / a[3];
		float inv_wb = 1.0f / b[3];

		float ax = a[0] * inv_wa * sx + 2048.0f;
		float ay = a[1] * inv_wa * sy + 2048.0f;
		float az = a[2] * inv_wa * sz + sz;

		float bx = b[0] * inv_wb * sx + 2048.0f;
		float by = b[1] * inv_wb * sy + 2048.0f;
		float bz = b[2] * inv_wb * sz + sz;

		if (ax < 0.0f) ax = 0.0f; else if (ax > 4095.0f) ax = 4095.0f;
		if (ay < 0.0f) ay = 0.0f; else if (ay > 4095.0f) ay = 4095.0f;
		if (bx < 0.0f) bx = 0.0f; else if (bx > 4095.0f) bx = 4095.0f;
		if (by < 0.0f) by = 0.0f; else if (by > 4095.0f) by = 4095.0f;
		if (az < 0.0f) az = 0.0f;
		if (bz < 0.0f) bz = 0.0f;

		int ix1 = (int)(ax * 16.0f);
		int iy1 = (int)(ay * 16.0f);
		unsigned int iz1 = (unsigned int)az;

		int ix2 = (int)(bx * 16.0f);
		int iy2 = (int)(by * 16.0f);
		unsigned int iz2 = (unsigned int)bz;

		owl_add_cnt_tag(packet, 4, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSH, 0), VIF_CODE(4, 0, VIF_DIRECT, 0)));
		owl_add_tag(packet, GIF_AD, GIFTAG(3, 1, 1, line_prim.data, 0, 1));
		owl_add_tag(packet, GS_RGBAQ, rgbaq);
		owl_add_tag(packet, GS_XYZ2, GS_SETREG_XYZ2(ix1, iy1, iz1));
		owl_add_tag(packet, GS_XYZ2, GS_SETREG_XYZ2(ix2, iy2, iz2));
	}
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
	athena_animation_controller *ctrl = &obj->anim_controller;
	if (ctrl->current) {
		clock_t now_clk = clock();
		float dt = 0.0f;
		if (ctrl->last_clock != 0) {
			dt = (float)(now_clk - ctrl->last_clock) / (float)CLOCKS_PER_SEC;
			if (dt < 0.0f || dt > 0.25f)
				dt = 0.0f;
		}
		ctrl->last_clock = now_clk;

		if (!ctrl->is_playing) {
			ctrl->current_time = 0.0f;
			ctrl->is_playing = true;
		} else {
			ctrl->current_time += dt;
		}

		if (ctrl->prev && ctrl->blend_duration > 0.0f) {
			ctrl->blend_elapsed += dt;
			ctrl->prev_time += dt;
			if (ctrl->blend_elapsed >= ctrl->blend_duration) {
				ctrl->prev = NULL;
				ctrl->blend_duration = 0.0f;
				ctrl->blend_elapsed = 0.0f;
			}
		}

		apply_animation(obj, ctrl->current_time);

		if (ctrl->current_time > ctrl->current->duration) {
			if (ctrl->loop) {
				if (ctrl->current->duration > 0.0f)
					ctrl->current_time = fmodf(ctrl->current_time, ctrl->current->duration);
				else
					ctrl->current_time = 0.0f;
			} else {
				ctrl->is_playing = false;
				ctrl->current = NULL;
			}
		}
	}
}

void update_object_space(athena_object_data *obj) {
  	matrix_functions->trs_euler(obj->transform, obj->position, obj->rotation, obj->scale);

	if (obj->update_collision)
		obj->update_collision(obj);
}

// Also selects the VU1 path: lane y non-zero = accurate (scissoring) path, zero
// = fast path. Every microprogram reads it once on entry, and each material
// group enters with MSCALF, so this can change from one material to the next.
static void emit_clipfan_giftag(owl_packet *packet, athena_render_data *data, bool texture_mapping, bool accurate) {
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

	owl_add_unpack_data_cnt(packet, 26, 1, 0);
	owl_add_float(packet, data->tristrip ? -1.0f : 1.0f);
	owl_add_uint(packet, accurate ? (clip_tag.data >> 32) : 0);
	owl_add_uint(packet, data->tristrip);
	owl_add_float(packet, data->attributes.face_culling);
}

// Cook constants. Clamping AFTER the scale is equivalent to clamping before it
// (the bounds are just scaled too) and lets VU0 do the whole thing branchless:
// vmax/vmini for the clamp, vftoi0 to truncate exactly like the C cast did.
// The upper UV bound is written as the product so it is bit-identical to the
// old clampf(v, -127.99f, 127.99f) * 256.0f at the boundary.
static const VECTOR COOK_ZERO  = { 0.0f, 0.0f, 0.0f, 0.0f };
static const VECTOR COOK_C255  = { 255.0f, 255.0f, 255.0f, 255.0f };
static const VECTOR COOK_C127  = { 127.0f, 127.0f, 127.0f, 0.0f };     // .w = 0 forces normal.w = 0
static const VECTOR COOK_CN127 = { -127.0f, -127.0f, -127.0f, 0.0f };
// Strip meshes keep normal.w: it carries ATHENA_STRIP_NO_KICK (-2 saturates to
// -128), which VU1 ORs into the ADC bit. Everything else still gets w = 0, so a
// stray w in script-built normals can never suppress triangles.
static const VECTOR COOK_C127W  = { 127.0f, 127.0f, 127.0f, 127.0f };
static const VECTOR COOK_CN127W = { -127.0f, -127.0f, -127.0f, -128.0f };
static const VECTOR COOK_C256  = { 256.0f, 256.0f, 256.0f, 256.0f };
static const VECTOR COOK_UVHI  = {  127.99f*256.0f,  127.99f*256.0f,  127.99f*256.0f,  127.99f*256.0f };
static const VECTOR COOK_UVLO  = { -127.99f*256.0f, -127.99f*256.0f, -127.99f*256.0f, -127.99f*256.0f };

// 16B VECTOR -> 12B struct, no conversion. Left scalar on purpose: an MMI
// repack of 4 vertices needs ~12 shuffles to land 12 words that straddle every
// doubleword boundary, for 19 instructions against 24. Not worth it -- the win
// here was hoisting the per-vertex cook_* branches out of the loop.
static void cook_positions_range(athena_compact_position *dst, const VECTOR *src, uint32_t count) {
	for (uint32_t i = 0; i < count; i++) {
		dst[i].x = src[i][0];
		dst[i].y = src[i][1];
		dst[i].z = src[i][2];
	}
}

// ppach folds the 4 words to 4 halfwords, ppacb folds those to 4 bytes, so one
// sw writes the whole packed vertex.
static void cook_colors_range(athena_compact_color *dst, const VECTOR *src, uint32_t count) {
	if (count == 0)
		return;

	__asm__ __volatile__(
	".set noreorder             \n"
	"lqc2   $vf4, 0x0(%3)       \n"
	"lqc2   $vf5, 0x0(%4)       \n"
    "1:                         \n"
	"lqc2   $vf1, 0x0(%1)       \n"
	"vmul.xyzw   $vf1, $vf1, $vf4\n"
	"vmax.xyzw   $vf1, $vf1, $vf5\n"
	"vmini.xyzw  $vf1, $vf1, $vf4\n"
	"vftoi0.xyzw $vf1, $vf1     \n"
	"qmfc2  $8, $vf1            \n"
	"ppach  $8, $0, $8          \n"
	"ppacb  $8, $0, $8          \n"
	"sw     $8, 0x0(%0)         \n"
	"addiu  %1, %1, 0x10        \n"
	"addiu  %2, %2, -1          \n"
	"bne    $0, %2, 1b          \n"
	"addiu  %0, %0, 0x4         \n"
	".set reorder               \n"
	: "+r" (dst), "+r" (src), "+r" (count)
	: "r" (COOK_C255), "r" (COOK_ZERO)
	: "$8", "memory");
}

static void cook_normals_range(athena_compact_normal *dst, const VECTOR *src, uint32_t count, bool keep_w) {
	if (count == 0)
		return;

	__asm__ __volatile__(
	".set noreorder             \n"
	"lqc2   $vf4, 0x0(%3)       \n"
	"lqc2   $vf5, 0x0(%4)       \n"
    "1:                         \n"
	"lqc2   $vf1, 0x0(%1)       \n"
	"vmul.xyzw   $vf1, $vf1, $vf4\n"
	"vmax.xyzw   $vf1, $vf1, $vf5\n"
	"vmini.xyzw  $vf1, $vf1, $vf4\n"
	"vftoi0.xyzw $vf1, $vf1     \n"
	"qmfc2  $8, $vf1            \n"
	"ppach  $8, $0, $8          \n"
	"ppacb  $8, $0, $8          \n"
	"sw     $8, 0x0(%0)         \n"
	"addiu  %1, %1, 0x10        \n"
	"addiu  %2, %2, -1          \n"
	"bne    $0, %2, 1b          \n"
	"addiu  %0, %0, 0x4         \n"
	".set reorder               \n"
	: "+r" (dst), "+r" (src), "+r" (count)
	: "r" (keep_w ? COOK_C127W : COOK_C127), "r" (keep_w ? COOK_CN127W : COOK_CN127)
	: "$8", "memory");
}

// Only .xy survive: ppach leaves u,v in the low two halfwords and sw writes
// exactly those four bytes.
static void cook_uvs_range(athena_compact_uv *dst, const VECTOR *src, uint32_t count) {
	if (count == 0)
		return;

	__asm__ __volatile__(
	".set noreorder             \n"
	"lqc2   $vf4, 0x0(%3)       \n"
	"lqc2   $vf5, 0x0(%4)       \n"
	"lqc2   $vf6, 0x0(%5)       \n"
    "1:                         \n"
	"lqc2   $vf1, 0x0(%1)       \n"
	"vmul.xyzw   $vf1, $vf1, $vf4\n"
	"vmax.xyzw   $vf1, $vf1, $vf5\n"
	"vmini.xyzw  $vf1, $vf1, $vf6\n"
	"vftoi0.xyzw $vf1, $vf1     \n"
	"qmfc2  $8, $vf1            \n"
	"ppach  $8, $0, $8          \n"
	"sw     $8, 0x0(%0)         \n"
	"addiu  %1, %1, 0x10        \n"
	"addiu  %2, %2, -1          \n"
	"bne    $0, %2, 1b          \n"
	"addiu  %0, %0, 0x4         \n"
	".set reorder               \n"
	: "+r" (dst), "+r" (src), "+r" (count)
	: "r" (COOK_C256), "r" (COOK_UVLO), "r" (COOK_UVHI)
	: "$8", "memory");
}

void render_invalidate_compact_cache(athena_render_data *data) {
	if (data)
		data->compact_dirty |= RENDER_DIRTY_ALL;
}

static void render_refit_cached_bounds(athena_render_data *data);
void render_invalidate_compact_positions(athena_render_data *data) {
    if (data) {
        data->compact_dirty |= RENDER_DIRTY_POSITIONS;
        calculate_bbox(data);
        render_refit_cached_bounds(data);
    }
}

void render_invalidate_chain_cache(athena_render_data *data) {
	if (data)
		data->chain_version++;
}

static void cook_skin_range(athena_compact_skin_indices * restrict dst_idx,
                            athena_compact_skin_weights * restrict dst_w,
                            const vertex_skin_data * restrict src,
                            uint32_t count) {
	const uint32_t max_bone_ofs = (MAX_VU1_BONES - 1) * 4;
	for (uint32_t v = 0; v < count; v++) {
		float w0 = src[v].bone_weights[0] > 0.0f ? src[v].bone_weights[0] : 0.0f;
		float w1 = src[v].bone_weights[1] > 0.0f ? src[v].bone_weights[1] : 0.0f;
		float w2 = src[v].bone_weights[2] > 0.0f ? src[v].bone_weights[2] : 0.0f;
		float w3 = src[v].bone_weights[3] > 0.0f ? src[v].bone_weights[3] : 0.0f;
		float wsum = w0 + w1 + w2 + w3;
		if (wsum > 0.0001f) {
			float inv = 4096.0f / wsum;
			int iw0 = (int)(w0 * inv + 0.5f);
			int iw1 = (int)(w1 * inv + 0.5f);
			int iw2 = (int)(w2 * inv + 0.5f);
			int iw3 = 4096 - iw0 - iw1 - iw2;
			if (iw3 < 0) iw3 = 0;
			if (iw0 > 65535) iw0 = 65535;
			if (iw1 > 65535) iw1 = 65535;
			if (iw2 > 65535) iw2 = 65535;
			if (iw3 > 65535) iw3 = 65535;
			dst_w[v].w[0] = (uint16_t)iw0;
			dst_w[v].w[1] = (uint16_t)iw1;
			dst_w[v].w[2] = (uint16_t)iw2;
			dst_w[v].w[3] = (uint16_t)iw3;
		} else {
			dst_w[v].w[0] = 4096;
			dst_w[v].w[1] = 0;
			dst_w[v].w[2] = 0;
			dst_w[v].w[3] = 0;
		}

		for (int j = 0; j < 4; j++) {
			uint32_t idx = src[v].bone_indices[j];
			if (idx > max_bone_ofs)
				idx = max_bone_ofs;
			dst_idx[v].idx[j] = (uint8_t)idx;
		}
	}
}

// Cooks the compact VIF wire formats from positions/normals/colours/texcoords,
// only for the attributes marked dirty. positions/colours are assumed non-NULL
// (file-wide invariant); normals/texcoords/skin_data are optional.
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
		free(data->compact_skin_indices);
		free(data->compact_skin_weights);

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

		if (data->skin_data) {
			data->compact_skin_indices = (athena_compact_skin_indices*)memalign(16, new_capacity * sizeof(athena_compact_skin_indices));
			data->compact_skin_weights = (athena_compact_skin_weights*)memalign(16, new_capacity * sizeof(athena_compact_skin_weights));
			memset(data->compact_skin_indices, 0, new_capacity * sizeof(athena_compact_skin_indices));
			memset(data->compact_skin_weights, 0, new_capacity * sizeof(athena_compact_skin_weights));
		} else {
			data->compact_skin_indices = NULL;
			data->compact_skin_weights = NULL;
		}

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
	bool cook_skin       = (data->compact_dirty & RENDER_DIRTY_SKIN) && data->skin_data && data->compact_skin_indices && data->compact_skin_weights;

	data->compact_dirty = 0;

	if (!cook_positions && !cook_colors && !cook_normals && !cook_uvs && !cook_skin)
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

		// One specialised pass per attribute instead of four per-vertex tests:
		// the cook_* flags are loop-invariant, but GCC kept the branches (and
		// their delay slots) inside the vertex loop.
		if (cook_positions)
			cook_positions_range(&data->compact_positions[dst], &data->positions[src], count);

		if (cook_colors)
			cook_colors_range(&data->compact_colors[dst], &data->colours[src], count);

		if (cook_normals)
			cook_normals_range(&data->compact_normals[dst], &data->normals[src], count, data->tristrip);

		if (cook_uvs)
			cook_uvs_range(&data->compact_uvs[dst], &data->texcoords[src], count);

		if (cook_skin)
			cook_skin_range(&data->compact_skin_indices[dst], &data->compact_skin_weights[dst], &data->skin_data[src], count);
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
		if (cook_skin) {
			SyncDCache(data->compact_skin_indices, &data->compact_skin_indices[padded_total - 1]);
			SyncDCache(data->compact_skin_weights, &data->compact_skin_weights[padded_total - 1]);
		}
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
	free(data->skin_data);

	data->positions = NULL;
	data->normals = NULL;
	data->texcoords = NULL;
	data->colours = NULL;
	data->skin_data = NULL;

	data->frozen = true;
}

// One sub-chain per material chunk: the unpacks (all DMA_REF) plus the
// FLUSHA/NOP/ITOP/MSCALF-or-MSCNT trailer. Mirrors draw_vu1_with_colors'
// chunk loop, including the last_index==-1 MSCALF-vs-MSCNT condition.

#define MAT_GIFTAG_STRIDE 13

// Only the CPU reads these cull boxes; DMA_REF consumes giftag slots 0..4.
// Moving a shadow grid changes its bounds, not its topology or DMA commands.
static void render_refit_cached_bounds(athena_render_data *data) {
	if (!data->positions) return;
	uint32_t first = 0;
	for (int g = 0; g < data->material_index_count; g++) {
		uint32_t end = data->material_indices[g].end;
		if (first >= data->index_count) break;
		if (end >= data->index_count) end = data->index_count - 1;
		if (end < first) continue;
		float lo[3], hi[3];
		for (int k = 0; k < 3; k++) lo[k] = hi[k] = data->positions[first][k];
		for (uint32_t v = first + 1; v <= end; v++) {
			for (int k = 0; k < 3; k++) {
				float p = data->positions[v][k];
				if (p < lo[k]) lo[k] = p;
				if (p > hi[k]) hi[k] = p;
			}
		}
		for (int c = 0; c < 10; c++) {
			athena_chain_cache *chain = c == 9 ? &data->ref_chain : &data->chain[c / 3][c % 3];
			if (!chain->tex_giftag || chain->built_version != data->chain_version) continue;
			VECTOR *bounds = (VECTOR *)&chain->tex_giftag[g * MAT_GIFTAG_STRIDE + 5];
			for (int corner = 0; corner < 8; corner++) {
				bounds[corner][0] = (corner & 1) ? hi[0] : lo[0];
				bounds[corner][1] = (corner & 2) ? hi[1] : lo[1];
				bounds[corner][2] = (corner & 4) ? hi[2] : lo[2];
				bounds[corner][3] = 1;
			}
		}
		first = end + 1;
	}
}

// Bakes a material's 13-QW slot:
//   slot[0]    : AD header (2 registers, EOP=1)
//   slot[1]    : GS_TEX0 (or GIF_NOP if untextured)
//   slot[2]    : GS_TEX1 (or GIF_NOP if untextured)
//   slot[3]    : prim_tag for chunk 0 of the material (NLOOP = texture_mapping ? 1 : 0)
//   slot[4]    : prim_tag for chunks 1..N-1 of the material (NLOOP = 0, skips redundant TEX0/TEX1 XGKICK)
//   slot[5..12]: 8-corner sub-mesh bounding box (VECTOR[8]) for per-submesh VU0 frustum culling
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
	slot[3] = nop_pair;
	slot[4] = nop_pair;
}

// Which attribute set a chain slot was baked for. colors/lights/spec/ref share
// the same VU-mem slot numbering (colors leaves the normals slot unwritten;
// ref leaves the uvs slot unwritten).
typedef enum {
	CHAIN_COLORS,   // positions, colours, uvs
	CHAIN_LIT,      // + normals  (lights and spec are identical here)
	CHAIN_REF,      // positions, normals, colours; skips materials without a ref texture
} eChainKind;

// Bakes one contiguous DMA_CALL sub-chain per material group: all chunks of a
// material run back-to-back inside a single DMA_CALL, with VIF_FLUSHA + texture
// XGKICK only on the first chunk of each material and VIF_FLUSH + VIF_MSCNT on
// subsequent chunks so VIF1/VU1/GS double-buffering streams at full hardware speed.
static void render_build_chain(athena_object_data *obj, athena_chain_cache *chain,
                               int pass_state, int batch_size, int mpg_addr,
                               eChainKind kind) {
	athena_render_data *data = obj->data;

	render_chain_wait_if_queued(chain);

	bool is_ref  = (kind == CHAIN_REF);
	bool skinned = (data->skeleton != NULL) && (data->compact_skin_indices != NULL);
	// Every microprogram reads the packed normal, the colors ones included:
	// its w carries the strip no-kick flag (ATHENA_STRIP_NO_KICK), so the slot
	// can never be left holding a previous draw's normals.
	bool want_normals = true;

	int tex_giftag_dest = data->skeleton ? RENDER_SKINNED_INBUF_SIZE : RENDER_INBUF_SIZE;

	int slot0 = skinned ? 2 : 0;
	int dst_positions = 2 + batch_size * slot0;
	int dst_normals   = 2 + batch_size * (slot0 + 1);
	int dst_colours   = 2 + batch_size * (slot0 + 2);
	int dst_uvs       = 2 + batch_size * (slot0 + 3);

	// A triangle list chunks on whole triangles. A strip chunk has to repeat
	// the last two vertices of the previous chunk instead (the GIFtag restarts
	// the strip at every chunk), and the step between chunks has to stay a
	// multiple of 4 so every DMA_REF below still lands on a quadword (12-byte
	// positions) and keeps the strip parity even (VU1 restarts it per chunk):
	// 48 -> 46-vertex chunks advancing 44, 36 -> 34 advancing 32.
	bool strip = data->tristrip;
	int chunk_cap = strip ? ((batch_size - 2) & ~3) + 2 : batch_size - (batch_size % 12);
	#define CHUNK_STEP(count, left) ((strip && (left) > (count)) ? (count) - 2 : (count))

	#define CHAIN_SKIPS(i) (is_ref && data->materials[data->material_indices[i].index].ref_texture_id == -1)

	uint32_t chunk_count = 0;
	int last_index = -1;
	for (int i = 0; i < data->material_index_count; i++) {
		if (!CHAIN_SKIPS(i)) {
			int idxs_to_draw = (data->material_indices[i].end - last_index);
			while (idxs_to_draw > 0) {
				int count = idxs_to_draw < chunk_cap ? idxs_to_draw : chunk_cap;
				idxs_to_draw -= CHUNK_STEP(count, idxs_to_draw);
				chunk_count++;
			}
		}
		last_index = data->material_indices[i].end;
	}

	chain->mpg_addr = mpg_addr;
	chain->built_version = data->chain_version;
	chain->texture_mapping = data->attributes.texture_mapping;

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

	// Worst case per chunk: prim_tag + tex_giftag + diffuse + skin_idx + skin_w +
	// positions + normals + colours + uvs + trailer (2) + DMA_RET = 12. Reserve 13.
	uint32_t qwc_budget = chunk_count * 13;

	if (chain->qwc_alloc < qwc_budget) {
		free(chain->buffer);
		chain->buffer = (owl_qword*)memalign(16, qwc_budget * sizeof(owl_qword));
		chain->qwc_alloc = qwc_budget;
	}

	free(chain->chunk_offset);
	chain->chunk_offset = (uint32_t*)malloc(data->material_index_count * sizeof(uint32_t));
	chain->chunk_count = chunk_count;

	free(chain->tex_giftag);
	chain->tex_giftag = (owl_qword*)memalign(16, data->material_index_count * MAT_GIFTAG_STRIDE * sizeof(owl_qword));

	for (int i = 0; i < data->material_index_count; i++)
		render_bake_tex_giftag_header(&chain->tex_giftag[i * MAT_GIFTAG_STRIDE]);

	owl_packet chain_pkt = { 0 };
	chain_pkt.ptr = chain->buffer;

	last_index = -1;

	for (int i = 0; i < data->material_index_count; i++) {
		chain->chunk_offset[i] = 0;
		if (CHAIN_SKIPS(i)) {
			last_index = data->material_indices[i].end;
			continue;
		}

		int idxs_to_draw = (data->material_indices[i].end - last_index);
		if (idxs_to_draw <= 0) {
			last_index = data->material_indices[i].end;
			continue;
		}

		const ath_mat *mat = &data->materials[data->material_indices[i].index];
		bool texture_mapping = !is_ref &&
			(((mat->texture_id != -1) && data->attributes.texture_mapping) || pass_state);
		uint32_t base = render_compact_base(data, i, last_index);
		athena_compact_position*     positions    = &data->compact_positions[base];
		athena_compact_normal*       normals      = &data->compact_normals[base];
		athena_compact_color*        colours      = &data->compact_colors[base];
		athena_compact_uv*           texcoords    = (texture_mapping && data->compact_uvs) ? &data->compact_uvs[base] : NULL;
		athena_compact_skin_indices* skin_indices = skinned ? &data->compact_skin_indices[base] : NULL;
		athena_compact_skin_weights* skin_weights = skinned ? &data->compact_skin_weights[base] : NULL;

		// Precompute the 8-corner AABB for this sub-mesh so draw_vu1_pass can
		// frustum-cull individual spatial sectors/materials on VU0 in ~20 cycles.
		{
			float min_x = positions[0].x, min_y = positions[0].y, min_z = positions[0].z;
			float max_x = min_x,          max_y = min_y,          max_z = min_z;
			for (int v = 1; v < idxs_to_draw; v++) {
				float px = positions[v].x, py = positions[v].y, pz = positions[v].z;
				if (px < min_x) min_x = px; else if (px > max_x) max_x = px;
				if (py < min_y) min_y = py; else if (py > max_y) max_y = py;
				if (pz < min_z) min_z = pz; else if (pz > max_z) max_z = pz;
			}
			VECTOR *sub_bbox = (VECTOR*)&chain->tex_giftag[i * MAT_GIFTAG_STRIDE + 5];
			for (int c = 0; c < 8; c++) {
				sub_bbox[c][0] = (c & 1) ? max_x : min_x;
				sub_bbox[c][1] = (c & 2) ? max_y : min_y;
				sub_bbox[c][2] = (c & 4) ? max_z : min_z;
				sub_bbox[c][3] = 1.0f;
			}
		}

		chain->chunk_offset[i] = (uint32_t)(chain_pkt.ptr - chain->buffer);

		int idxs_drawn = 0;
		int chunk_in_mat = 0;

		while (idxs_to_draw > 0) {
			int count = idxs_to_draw < chunk_cap ? idxs_to_draw : chunk_cap;
			owl_qword *mat_slot = &chain->tex_giftag[i * MAT_GIFTAG_STRIDE];

			owl_add_unpack_data_ref(&chain_pkt, 0, &mat_slot[chunk_in_mat == 0 ? 3 : 4], 1, 1);

			if (chunk_in_mat == 0 || (kind == CHAIN_COLORS && chunk_in_mat == 1))
				owl_add_unpack_data_ref(&chain_pkt, tex_giftag_dest, &mat_slot[0], 3, 1);

			owl_add_unpack_data_ref(&chain_pkt, 1, (void*)&mat->diffuse, 1, 1);

			if (skinned) {
				owl_add_unpack_data_ref_packed(&chain_pkt, 2, &skin_indices[idxs_drawn], count, UNPACK_V4_8, sizeof(athena_compact_skin_indices), 1, 1);
				owl_add_unpack_data_ref_packed(&chain_pkt, 2 + batch_size, &skin_weights[idxs_drawn], count, UNPACK_V4_16, sizeof(athena_compact_skin_weights), 1, 1);
			}

			owl_add_unpack_data_ref_packed(&chain_pkt, dst_positions, &positions[idxs_drawn], count, UNPACK_V3_32, sizeof(athena_compact_position), 1, 1);

			if (want_normals)
				owl_add_unpack_data_ref_packed(&chain_pkt, dst_normals, &normals[idxs_drawn], count, UNPACK_V4_8, sizeof(athena_compact_normal), 1, 0);

			owl_add_unpack_data_ref_packed(&chain_pkt, dst_colours, &colours[idxs_drawn], count, UNPACK_V4_8, sizeof(athena_compact_color), 1, 1);

			if (texcoords)
				owl_add_unpack_data_ref_packed(&chain_pkt, dst_uvs, &texcoords[idxs_drawn], count, UNPACK_V2_16, sizeof(athena_compact_uv), 1, 0);

			// Only the first chunk of a material needs FLUSHA + MSCALF (to wait for any
			// async PATH3 texture upload queued before this material and make each
			// sub-mesh self-contained for frustum culling). Subsequent chunks of the
			// same material use FLUSH + MSCNT so VU1/GIF double-buffering streams at peak speed.
			owl_add_cnt_tag(&chain_pkt, 1, owl_vif_code_double(VIF_CODE(0, 0, VIF_NOP, 0), VIF_CODE(0, 0, VIF_NOP, 0)));
			owl_add_uint(&chain_pkt, VIF_CODE(0, 0, (chunk_in_mat == 0 ? VIF_FLUSHA : VIF_FLUSH), 0));
			owl_add_uint(&chain_pkt, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(&chain_pkt, VIF_CODE(count, 0, VIF_ITOP, 0));
			owl_add_uint(&chain_pkt, VIF_CODE(mpg_addr, 0, (chunk_in_mat == 0 ? VIF_MSCALF : VIF_MSCNT), 0));

			chunk_in_mat++;
			int step = CHUNK_STEP(count, idxs_to_draw);
			idxs_to_draw -= step;
			idxs_drawn += step;
		}

		owl_add_dma_ret(&chain_pkt);

		last_index = data->material_indices[i].end;
	}

	#undef CHAIN_SKIPS
	#undef CHUNK_STEP

	SyncDCache(chain->buffer, &chain->buffer[qwc_budget - 1]);
	SyncDCache(chain->tex_giftag, &chain->tex_giftag[data->material_index_count * MAT_GIFTAG_STRIDE - 1]);
}

// Blocks until the DMAC is done with whatever this chain last queued, so its
// buffers can be rewritten. No-op unless a draw call is genuinely still in
// flight -- see athena_chain_cache.queued_gen.
static void render_chain_wait_if_queued(athena_chain_cache *chain) {
	if (!chain->has_queued || owl_generation_read(chain->queued_gen))
		return;

	owl_wait_generation(chain->queued_gen);
}

static void render_update_mat_giftags(athena_chain_cache *chain, athena_render_data *data,
                                      int mat_id, bool texture_mapping, GSSURFACE *tex) {
	owl_qword *slot = &chain->tex_giftag[mat_id * MAT_GIFTAG_STRIDE];

	prim_reg_t prim_data = {
		.PRIM = (data->tristrip ? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE),
		.IIP = data->attributes.shade_model,
		.TME = texture_mapping,
		.FGE = gsGlobal->PrimFogEnable,
		.ABE = gsGlobal->PrimAlphaEnable,
		.AA1 = gsGlobal->PrimAAEnable,
		.FST = 0,
		.CTXT = gsGlobal->PrimContext,
		.FIX = 0
	};

	giftag_t prim_first = {
		.NLOOP = texture_mapping ? 1 : 0,
		.EOP = 1,
		.PRE = 1,
		.PRIM = prim_data.data,
		.FLG = 0,
		.NREG = 3
	};

	giftag_t prim_next = prim_first;
	prim_next.NLOOP = 0;

	owl_qword p0, p1;
	p0.dword[0] = prim_first.data;
	p0.dword[1] = DRAW_STQ2_REGLIST;
	p1.dword[0] = prim_next.data;
	p1.dword[1] = DRAW_STQ2_REGLIST;

	owl_qword tex0 = slot[1];
	owl_qword tex1 = slot[2];

	if (texture_mapping && tex) {
		int tw, th;
		athena_set_tw_th(tex, &tw, &th);

		tex0.dword[1] = GS_TEX0_1 + gsGlobal->PrimContext;
		tex0.dword[0] = GS_SETREG_TEX0((tex->Vram & ~TRANSFER_REQUEST_MASK) / 256,
		                               tex->TBW,
		                               tex->PSM,
		                               tw, th,
		                               gsGlobal->PrimAlphaEnable,
		                               COLOR_MODULATE,
		                               (tex->VramClut & ~TRANSFER_REQUEST_MASK) / 256,
		                               tex->ClutPSM,
		                               0, 0,
		                               tex->VramClut ? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD);

		tex1.dword[1] = GS_TEX1_1 + gsGlobal->PrimContext;
		tex1.dword[0] = GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0);
	}

	if (slot[1].dword[0] == tex0.dword[0] && slot[1].dword[1] == tex0.dword[1] &&
	    slot[2].dword[0] == tex1.dword[0] && slot[2].dword[1] == tex1.dword[1] &&
	    slot[3].dword[0] == p0.dword[0]   && slot[3].dword[1] == p0.dword[1]   &&
	    slot[4].dword[0] == p1.dword[0]   && slot[4].dword[1] == p1.dword[1])
		return;

	render_chain_wait_if_queued(chain);

	slot[1] = tex0;
	slot[2] = tex1;
	slot[3] = p0;
	slot[4] = p1;

	SyncDCache(&slot[1], &slot[4]);
}

static inline void render_trigger_texture_upload(owl_packet *packet, int texture_id) {
	if (texture_id == -1)
		return;

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(
		VIF_CODE(0, 0, VIF_NOP, 1),
		VIF_CODE(texture_id, 0, VIF_MARK, 0)));
}

static inline int render_object_lighting_qwc(bool specular) {
	return ((pnt_light_count.w > 0) ? (1 + 5 + 1 + 3 + 1 + 8 + 1 + 4) : (1 + 5 + 1 + 3))
	     + (specular ? (1 + 4) : 0);
}

// Angular radius below which the specular pass treats the viewer as being at
// infinity for the whole object: sin(15 deg). At that size the direction to
// the camera varies by at most 15 deg across the object, so the half vector
// (halfway between light and view) by at most ~7.5 -- a highlight that slides
// a little less as the camera moves, not one that looks wrong.
#define SPEC_INFINITE_MAX_SIN 0.2588f

// Per-object specular setup for the PL_SPECULAR microprograms (SPEC_HALF_T /
// SPEC_MODE in mem_layout.i). Small objects for their distance get the viewer
// at infinity: one view vector, from the object's centre to the camera, so the
// half vector of each light is a constant worked out here and VU1 needs no
// per-vertex view vector, square root or divide for the specular. Anything
// large or close keeps the exact per-vertex Blinn-Phong.
static void render_upload_specular_mode(owl_packet *packet, athena_object_data *obj,
                                        const VECTOR obj_dir[4], const VECTOR obj_cam,
                                        float max_scale_sq) {
	VECTOR *bounds = obj->skinned_bounds ? obj->skinned_bounds : obj->data->bounding_box;
	const float *m = obj->transform;

	float cx = (bounds[0][0] + bounds[7][0]) * 0.5f;
	float cy = (bounds[0][1] + bounds[7][1]) * 0.5f;
	float cz = (bounds[0][2] + bounds[7][2]) * 0.5f;
	float ex = bounds[7][0] - bounds[0][0];
	float ey = bounds[7][1] - bounds[0][1];
	float ez = bounds[7][2] - bounds[0][2];
	float radius_sq = (ex*ex + ey*ey + ez*ez) * 0.25f * max_scale_sq;

	// World-space centre against the camera.
	float *cam = getCameraPosition();
	float wx = cx*m[0] + cy*m[4] + cz*m[8]  + m[12] - cam[0];
	float wy = cx*m[1] + cy*m[5] + cz*m[9]  + m[13] - cam[1];
	float wz = cx*m[2] + cy*m[6] + cz*m[10] + m[14] - cam[2];
	float dist_sq = wx*wx + wy*wy + wz*wz;

	// A never-computed (all zero) box has no extent: keep it exact.
	bool infinite = radius_sq > 0.0f &&
	                radius_sq < dist_sq * (SPEC_INFINITE_MAX_SIN * SPEC_INFINITE_MAX_SIN);

	VECTOR half_t[3] qw_aligned;
	memset(half_t, 0, sizeof(half_t));
	if (infinite) {
		// Object-space view direction, centre -> camera.
		float vx = obj_cam[0] - cx, vy = obj_cam[1] - cy, vz = obj_cam[2] - cz;
		float vlen = sqrtf(vx*vx + vy*vy + vz*vz);
		if (vlen > 1e-6f) {
			vx /= vlen; vy /= vlen; vz /= vlen;
		}
		for (int i = 0; i < 4; i++) {
			// Same L as the exact path (the light direction as given, not
			// renormalised) plus the unit view vector, then normalised.
			float hx = obj_dir[i][0] + vx, hy = obj_dir[i][1] + vy, hz = obj_dir[i][2] + vz;
			float hlen = sqrtf(hx*hx + hy*hy + hz*hz);
			float inv = hlen > 1e-6f ? 1.0f / hlen : 0.0f;
			half_t[0][i] = hx * inv;
			half_t[1][i] = hy * inv;
			half_t[2][i] = hz * inv;
		}
	}

	owl_qword mode qw_aligned;
	memset(&mode, 0, sizeof(mode));
	mode.sword[0] = infinite ? 1 : 0;

	owl_add_unpack_data_cnt(packet, 1013, 4, 0);
	owl_add_uquad_ptr(packet, half_t[0]);
	owl_add_uquad_ptr(packet, half_t[1]);
	owl_add_uquad_ptr(packet, half_t[2]);
	owl_add_uquad_ptr(packet, (__uint128_t *)&mode);
}

static void render_upload_object_lighting(owl_packet *packet, athena_object_data *obj, bool specular) {
	const float *m = obj->transform;

	float sx2 = m[0]*m[0] + m[1]*m[1] + m[2]*m[2];
	float sy2 = m[4]*m[4] + m[5]*m[5] + m[6]*m[6];
	float sz2 = m[8]*m[8] + m[9]*m[9] + m[10]*m[10];

	float inv_sx2 = (sx2 > 1e-8f) ? (1.0f / sx2) : 1.0f;
	float inv_sy2 = (sy2 > 1e-8f) ? (1.0f / sy2) : 1.0f;
	float inv_sz2 = (sz2 > 1e-8f) ? (1.0f / sz2) : 1.0f;
	float avg_scale_sq = (sx2 + sy2 + sz2) * (1.0f / 3.0f);

	VECTOR obj_dir[4] qw_aligned;
	for (int i = 0; i < 4; i++) {
		if (i < (int)dir_light_count.w) {
			float dx = dir_lights_packed_dir[i][0];
			float dy = dir_lights_packed_dir[i][1];
			float dz = dir_lights_packed_dir[i][2];
			float orig_len = sqrtf(dx*dx + dy*dy + dz*dz);

			float ox = (m[0]*dx + m[1]*dy + m[2]*dz) * inv_sx2;
			float oy = (m[4]*dx + m[5]*dy + m[6]*dz) * inv_sy2;
			float oz = (m[8]*dx + m[9]*dy + m[10]*dz) * inv_sz2;
			float len2 = ox*ox + oy*oy + oz*oz;
			if (len2 > 1e-8f) {
				float s = orig_len / sqrtf(len2);
				obj_dir[i][0] = ox * s;
				obj_dir[i][1] = oy * s;
				obj_dir[i][2] = oz * s;
			} else {
				obj_dir[i][0] = dx;
				obj_dir[i][1] = dy;
				obj_dir[i][2] = dz;
			}
			obj_dir[i][3] = 1.0f;
		} else {
			obj_dir[i][0] = 0.0f;
			obj_dir[i][1] = 0.0f;
			obj_dir[i][2] = 0.0f;
			obj_dir[i][3] = 1.0f;
		}
	}

	float *cam_w = getCameraPosition();
	float cx = cam_w[0] - m[12];
	float cy = cam_w[1] - m[13];
	float cz = cam_w[2] - m[14];
	VECTOR obj_cam qw_aligned = {
		(m[0]*cx + m[1]*cy + m[2]*cz) * inv_sx2,
		(m[4]*cx + m[5]*cy + m[6]*cz) * inv_sy2,
		(m[8]*cx + m[9]*cy + m[10]*cz) * inv_sz2,
		1.0f
	};

	owl_add_unpack_data_cnt(packet, 9, 5, 0);
	owl_add_uquad_ptr(packet, obj_cam);
	owl_add_uquad_ptr(packet, obj_dir[0]);
	owl_add_uquad_ptr(packet, obj_dir[1]);
	owl_add_uquad_ptr(packet, obj_dir[2]);
	owl_add_uquad_ptr(packet, obj_dir[3]);

	// Same directions transposed for the matrix form (LIGHT_DIR_T in
	// mem_layout.i): row k holds component k of all 4 lights.
	VECTOR dir_t[3] qw_aligned;
	for (int k = 0; k < 3; k++) {
		dir_t[k][0] = obj_dir[0][k];
		dir_t[k][1] = obj_dir[1][k];
		dir_t[k][2] = obj_dir[2][k];
		dir_t[k][3] = obj_dir[3][k];
	}
	owl_add_unpack_data_cnt(packet, 136, 3, 0);
	owl_add_uquad_ptr(packet, dir_t[0]);
	owl_add_uquad_ptr(packet, dir_t[1]);
	owl_add_uquad_ptr(packet, dir_t[2]);

	if (specular) {
		float max_scale_sq = sx2 > sy2 ? sx2 : sy2;
		if (sz2 > max_scale_sq) max_scale_sq = sz2;
		render_upload_specular_mode(packet, obj, obj_dir, obj_cam, max_scale_sq);
	}

	if (pnt_light_count.w > 0) {
		VECTOR obj_ppos[4] qw_aligned;
		VECTOR obj_pdiff[4] qw_aligned;
		for (int i = 0; i < 4; i++) {
			if (i < (int)pnt_light_count.w) {
				float px = point_lights_packed_pos[i][0] - m[12];
				float py = point_lights_packed_pos[i][1] - m[13];
				float pz = point_lights_packed_pos[i][2] - m[14];
				obj_ppos[i][0] = (m[0]*px + m[1]*py + m[2]*pz) * inv_sx2;
				obj_ppos[i][1] = (m[4]*px + m[5]*py + m[6]*pz) * inv_sy2;
				obj_ppos[i][2] = (m[8]*px + m[9]*py + m[10]*pz) * inv_sz2;
				obj_ppos[i][3] = 1.0f;

				obj_pdiff[i][0] = point_lights_packed_dif[i][0];
				obj_pdiff[i][1] = point_lights_packed_dif[i][1];
				obj_pdiff[i][2] = point_lights_packed_dif[i][2];
				obj_pdiff[i][3] = point_lights_packed_dif[i][3] * avg_scale_sq;
			} else {
				obj_ppos[i][0] = 0.0f;
				obj_ppos[i][1] = 0.0f;
				obj_ppos[i][2] = 0.0f;
				obj_ppos[i][3] = 1.0f;

				obj_pdiff[i][0] = 0.0f;
				obj_pdiff[i][1] = 0.0f;
				obj_pdiff[i][2] = 0.0f;
				obj_pdiff[i][3] = 0.0f;
			}
		}

		owl_add_unpack_data_cnt(packet, 124, 8, 0);
		owl_add_uquad_ptr(packet, obj_ppos[0]);
		owl_add_uquad_ptr(packet, obj_ppos[1]);
		owl_add_uquad_ptr(packet, obj_ppos[2]);
		owl_add_uquad_ptr(packet, obj_ppos[3]);
		owl_add_uquad_ptr(packet, obj_pdiff[0]);
		owl_add_uquad_ptr(packet, obj_pdiff[1]);
		owl_add_uquad_ptr(packet, obj_pdiff[2]);
		owl_add_uquad_ptr(packet, obj_pdiff[3]);

		// Transposed copy for PointLightsSoA (POINT_LIGHT_SOA in mem_layout.i):
		// x, y, z of the 4 positions, then the 4 attenuations.
		VECTOR p_soa[4] qw_aligned;
		for (int i = 0; i < 4; i++) {
			p_soa[0][i] = obj_ppos[i][0];
			p_soa[1][i] = obj_ppos[i][1];
			p_soa[2][i] = obj_ppos[i][2];
			p_soa[3][i] = obj_pdiff[i][3];
		}
		owl_add_unpack_data_cnt(packet, 1009, 4, 0);
		owl_add_uquad_ptr(packet, p_soa[0]);
		owl_add_uquad_ptr(packet, p_soa[1]);
		owl_add_uquad_ptr(packet, p_soa[2]);
		owl_add_uquad_ptr(packet, p_soa[3]);
	}
}

static void draw_vu1_pass(athena_object_data *obj, int pass_state,
                          eRenderPipelines pipeline, eChainKind kind,
                          vu_mpg *mpg_static, vu_mpg *mpg_skinned) {
	athena_render_data *data = obj->data;

	render_cook_compact_vertices(data);

	int batch_size = BATCH_SIZE, mpg_addr = 0;

	if (data->skeleton) {
		batch_size = BATCH_SIZE_SKINNED;
		mpg_addr = vu_mpg_preload(mpg_skinned, true);
	} else {
		mpg_addr = vu_mpg_preload(mpg_static, true);
	}

	bool is_ref = (kind == CHAIN_REF);
	athena_chain_cache *chain = is_ref ? &data->ref_chain : &data->chain[pipeline][pass_state];
	if (!chain->buffer || chain->built_version != data->chain_version
	    || chain->mpg_addr != mpg_addr
	    || (!is_ref && pass_state == 0
	        && chain->texture_mapping != data->attributes.texture_mapping)) {
		render_build_chain(obj, chain, pass_state, batch_size, mpg_addr, kind);
	}

	bool want_lights = (kind != CHAIN_COLORS);
	bool want_specular = (pipeline == PL_SPECULAR);
	int header_qwc = 9 + render_view_qwc() + (want_lights ? render_object_lighting_qwc(want_specular) : 0);
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, header_qwc);

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSHE, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

	render_upload_view(packet);

	owl_add_unpack_data_cnt(packet, 5, 4, 0);
	owl_add_uquad_ptr(packet, &(obj->transform[0]));
	owl_add_uquad_ptr(packet, &(obj->transform[4]));
	owl_add_uquad_ptr(packet, &(obj->transform[8]));
	owl_add_uquad_ptr(packet, &(obj->transform[12]));

	if (kind == CHAIN_COLORS) {
		owl_add_unpack_data_cnt(packet, 16, 1, 0);
		owl_add_uquad_ptr(packet, obj->bump_offset_buffer);
	}

	if (want_lights) {
		render_upload_object_lighting(packet, obj, want_specular);
	}

	if (obj->bone_matrices && data->skeleton) {
		uint32_t bone_cnt = data->skeleton->bone_count;
		if (bone_cnt > MAX_VU1_BONES)
			bone_cnt = MAX_VU1_BONES;
		owl_add_unpack_data_ref(packet, VU1_BONE_MATRICES_ADDR, (void*)obj->bone_matrices, bone_cnt * 4, 0);
		obj->bones_queued_gen = owl_flush_generation();
		obj->bones_queued = 1;
	}

	MATRIX local_clip;
	bool cull_submeshes = (!data->skeleton && data->material_index_count > 1);
	bool accurate_clipping = data->attributes.accurate_clipping;
	if (cull_submeshes || accurate_clipping) {
		matrix_functions->multiply(local_clip, obj->transform, world_screen);
	}

	// Accurate clipping only matters for geometry that can cross the guard
	// band or the near/far planes. Anything whose box sits entirely inside
	// takes the fast path instead -- same image, a fraction of the VU1 work.
	// Per material group when the mesh has several (the same boxes the cull
	// test above uses), else once for the whole object: the animated box for
	// a skinned mesh, the bind-time one otherwise.
	bool object_inside = false;
	if (accurate_clipping && !cull_submeshes) {
		VECTOR *bounds = obj->skinned_bounds ? obj->skinned_bounds
		                                     : (data->skeleton ? NULL : data->bounding_box);
		object_inside = bounds && bounding_box_inside_guard_band(local_clip, bounds);
	}

	int last_index = -1;
	int prev_clipfan = -1;
	GSSURFACE* tex = NULL;
	int texture_id;
	for (int i = 0; i < data->material_index_count; i++) {
		int idxs_to_draw = (data->material_indices[i].end - last_index);
		if (idxs_to_draw <= 0) {
			last_index = data->material_indices[i].end;
			continue;
		}

		if (cull_submeshes && clip_bounding_box(local_clip, (VECTOR*)&chain->tex_giftag[i * MAT_GIFTAG_STRIDE + 5])) {
			last_index = data->material_indices[i].end;
			continue;
		}

		const ath_mat *mat = &data->materials[data->material_indices[i].index];
		bool texture_mapping;

		if (is_ref) {
			texture_mapping = (mat->ref_texture_id != -1);
			if (!texture_mapping) {
				last_index = data->material_indices[i].end;
				continue;
			}
		} else {
			texture_mapping = (((mat->texture_id != -1) && data->attributes.texture_mapping) || pass_state);
		}

		GSSURFACE *cur_tex = NULL;
		if (texture_mapping) {
			if (is_ref) {
				cur_tex = data->textures[mat->ref_texture_id];
			} else {
				switch (pass_state) {
					case 1: // bump map
						cur_tex = data->textures[mat->bump_texture_id];
						break;
					case 2: // decal
						cur_tex = data->textures[mat->decal_texture_id];
						break;
					default:
						cur_tex = data->textures[mat->texture_id];
				}
			}

			if (cur_tex != tex) {
				texture_id = texture_manager_bind(gsGlobal, cur_tex, true);
				tex = cur_tex;

				owl_query_packet(CHANNEL_VIF1, 1);
				render_trigger_texture_upload(packet, texture_id);
			}
		}

		render_update_mat_giftags(chain, data, i, texture_mapping, cur_tex);

		bool accurate = accurate_clipping && !(cull_submeshes
			? bounding_box_inside_guard_band(local_clip, (VECTOR*)&chain->tex_giftag[i * MAT_GIFTAG_STRIDE + 5])
			: object_inside);
		if (!accurate && accurate_clipping)
			g_render_stats.fast_clip_groups++;

		int clipfan = (int)texture_mapping | ((int)accurate << 1);
		if (clipfan != prev_clipfan) {
			owl_query_packet(CHANNEL_VIF1, 2);
			emit_clipfan_giftag(packet, data, texture_mapping, accurate);
			prev_clipfan = clipfan;
		}

		owl_query_packet(CHANNEL_VIF1, 1);
		owl_add_dma_call(packet, &chain->buffer[chain->chunk_offset[i]]);

		last_index = data->material_indices[i].end;
	}

	chain->queued_gen = owl_flush_generation();
	chain->has_queued = 1;
}

void draw_vu1_with_colors(athena_object_data *obj, int pass_state) {
	draw_vu1_pass(obj, pass_state, PL_NO_LIGHTS, CHAIN_COLORS, vu1_colors, vu1_colors_skinned);
}

void draw_vu1_with_lights(athena_object_data *obj, int pass_state) {
	draw_vu1_pass(obj, pass_state, PL_DEFAULT, CHAIN_LIT, vu1_lights, vu1_lights_skinned);
}

void draw_vu1_with_spec_lights(athena_object_data *obj, int pass_state) {
	draw_vu1_pass(obj, pass_state, PL_SPECULAR, CHAIN_LIT, vu1_specular, vu1_specular_skinned);
}

void draw_vu1_with_lights_ref(athena_object_data *obj, int pass_state) {
	draw_vu1_pass(obj, pass_state, PL_DEFAULT, CHAIN_REF, vu1_lights_reflection, vu1_lights_reflection_skinned);
}
