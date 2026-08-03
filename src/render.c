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
static render_stats_t g_render_stats = { 0 };

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
	
	vu1_set_double_buffer_settings(270, 339); // Skinned layout

	vu1_colors   = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DCS),   embed_vu_code_size(VU1Draw3DCS),   VECTOR_UNIT_1, false); 
	vu1_lights   = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCS),  embed_vu_code_size(VU1Draw3DLCS),  VECTOR_UNIT_1, false);
	vu1_specular = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCSS), embed_vu_code_size(VU1Draw3DLCSS), VECTOR_UNIT_1, false);

	vu1_colors_skinned = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DCS_Skin), embed_vu_code_size(VU1Draw3DCS_Skin), VECTOR_UNIT_1, false);
	vu1_lights_skinned = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCS_Skin), embed_vu_code_size(VU1Draw3DLCS_Skin), VECTOR_UNIT_1, false);
	vu1_specular_skinned = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCSS_Skin), embed_vu_code_size(VU1Draw3DLCSS_Skin), VECTOR_UNIT_1, false);

	vu1_lights_reflection = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCS_Ref), embed_vu_code_size(VU1Draw3DLCS_Ref), VECTOR_UNIT_1, false);
}

void render_begin() {
	vu1_set_double_buffer_settings(270, 339); // Skinned layout

	render_reset_stats();

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 17);

	owl_add_unpack_data_cnt(packet, 10, 16, 0);
	owl_add_uquad_ptr(packet, (dir_lights.direction[0]));
	owl_add_uquad_ptr(packet, (dir_lights.direction[1]));
	owl_add_uquad_ptr(packet, (dir_lights.direction[2]));
	owl_add_uquad_ptr(packet, (dir_lights.direction[3]));
	owl_add_uquad_ptr(packet, &(dir_lights.ambient[0]));
	owl_add_uquad_ptr(packet, &(dir_lights.ambient[1]));
	owl_add_uquad_ptr(packet, &(dir_lights.ambient[2]));
	owl_add_uquad_ptr(packet, &(dir_lights.ambient[3]));
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
}

int NewLight() {
	if (active_dir_lights < 4) {
		dir_lights.ambient[0].w = active_dir_lights+1;
		owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 2);
		owl_add_unpack_data_cnt(packet, 14, 1, 0);
		owl_add_uquad_ptr(packet, &(dir_lights.ambient[0]));
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
			owl_add_unpack_data_cnt(packet, 14+id, 1, 0);
			owl_add_uquad_ptr(packet, &(dir_lights.ambient[id]));
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

void draw_vu1_with_colors(athena_object_data *obj, int pass_state);
void draw_vu1_with_lights(athena_object_data *obj, int pass_state);
void draw_vu1_with_spec_lights(athena_object_data *obj, int pass_state);
void draw_vu1_with_lights_ref(athena_object_data *obj, int pass_state);

void (*render_funcs[])(athena_object_data *obj, int pass_state) = {
	draw_vu1_with_colors,
	draw_vu1_with_lights,
	draw_vu1_with_spec_lights
};

void render_object(athena_object_data *obj) {
	if (obj->update_physics)
		obj->update_physics(obj);

	if (obj && obj->data) {
		g_render_stats.draw_calls++;
		g_render_stats.triangles += render_calc_triangles(obj->data);
	}

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
	}

	// Pre-baked DMA_CALL chains (draw_vu1_with_colors), one per pass_state
	// slot: independent of the frozen float source (built from the
	// compact_* buffers and pointers that survive freezing), so reset
	// unconditionally -- covers the same "previous life of this
	// render_data" case as above (e.g. shadows.c rebuilding geometry with a
	// different chunk count).
	for (int i = 0; i < 3; i++) {
		free(data->colors_chain[i].buffer);
		free(data->colors_chain[i].chunk_offset);
		data->colors_chain[i].buffer = NULL;
		data->colors_chain[i].chunk_offset = NULL;
		data->colors_chain[i].qwc_alloc = 0;
		data->colors_chain[i].chunk_count = 0;
		data->colors_chain[i].mpg_addr = -1;
		data->colors_chain[i].built_version = 0;
	}
	data->chain_version = 1;

	obj->bump_offset_buffer = &zero_bump_offset;

	if (data->skin_data) {
		obj->anim_controller.current = NULL;

		obj->bones = (athena_bone_transform*)malloc(data->skeleton->bone_count * sizeof(athena_bone_transform));
		obj->bone_matrices = (MATRIX*)malloc(data->skeleton->bone_count * sizeof(MATRIX));

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

	giftag_t prim_tag = {
		.NLOOP = 0,
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
	owl_add_uint(packet, data->attributes.face_culling);
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

// Cooks the compact VIF-unpack wire formats (see render.h) from
// positions/normals/colours/texcoords, but only the attributes actually
// marked dirty (RENDER_DIRTY_* bits), and only when there's at least one:
// on the first draw, whenever index_count outgrows the current cache, or
// after an explicit render_invalidate_compact_cache()/_positions() call.
// These don't change frame-to-frame for almost all meshes (skinned meshes
// animate via bone matrices in the VU, not by touching this data), and
// callers that DO touch something every frame typically only touch one
// attribute (e.g. shadows.c's projector grid only ever rewrites positions),
// so treating this as a single all-or-nothing flag would still re-cook 4x
// the data for nothing on every one of those calls.
//
// positions/colours are assumed non-NULL, matching the existing invariant
// of this file (they're already dereferenced unconditionally below in the
// batch loops); normals/texcoords are optional, matching their existing
// NULL-checked usage.
static void render_cook_compact_vertices(athena_render_data *data) {
	if (!data || data->index_count == 0 || data->frozen)
		return;

	if (data->compact_capacity < data->index_count) {
		uint32_t new_capacity = data->index_count + 4;

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

	for (uint32_t i = 0; i < data->index_count; i++) {
		if (cook_positions) {
			data->compact_positions[i].x = data->positions[i][0];
			data->compact_positions[i].y = data->positions[i][1];
			data->compact_positions[i].z = data->positions[i][2];
		}

		if (cook_colors) {
			data->compact_colors[i].r = (uint8_t)(render_clampf(data->colours[i][0], 0.0f, 1.0f) * 255.0f);
			data->compact_colors[i].g = (uint8_t)(render_clampf(data->colours[i][1], 0.0f, 1.0f) * 255.0f);
			data->compact_colors[i].b = (uint8_t)(render_clampf(data->colours[i][2], 0.0f, 1.0f) * 255.0f);
			data->compact_colors[i].a = (uint8_t)(render_clampf(data->colours[i][3], 0.0f, 1.0f) * 255.0f);
		}

		if (cook_normals) {
			data->compact_normals[i].x = (int8_t)(render_clampf(data->normals[i][0], -1.0f, 1.0f) * 127.0f);
			data->compact_normals[i].y = (int8_t)(render_clampf(data->normals[i][1], -1.0f, 1.0f) * 127.0f);
			data->compact_normals[i].z = (int8_t)(render_clampf(data->normals[i][2], -1.0f, 1.0f) * 127.0f);
			data->compact_normals[i].w = 0;
		}

		if (cook_uvs) {
			data->compact_uvs[i].u = (int16_t)(render_clampf(data->texcoords[i][0], -127.99f, 127.99f) * 256.0f);
			data->compact_uvs[i].v = (int16_t)(render_clampf(data->texcoords[i][1], -127.99f, 127.99f) * 256.0f);
		}
	}
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

// Cooks the pre-baked DMA_CALL sub-chain for draw_vu1_with_colors: the
// diffuse/skin_data/positions/colours/uvs unpacks (all DMA_REF -- pointer
// only, so live JS mutation of the underlying buffers keeps working with no
// extra code here) plus the FLUSHA/NOP/ITOP/MSCALF-or-MSCNT trailer, one
// sub-chain per material chunk. Mirrors the material/chunk loop in
// draw_vu1_with_colors exactly (including the last_index==-1 MSCALF-vs-MSCNT
// condition), but does NOT touch texture tags or bake_giftags() -- those
// embed the live GS VRAM address (subject to eviction) and
// gsGlobal->PrimContext (flips every frame), so they must stay dynamic in
// the caller's packet stream. See athena_chain_cache's doc comment in
// render.h for the full rationale.
static void render_build_colors_chain(athena_object_data *obj, int pass_state, int batch_size, int mpg_addr) {
	athena_render_data *data = obj->data;
	athena_chain_cache *chain = &data->colors_chain[pass_state];

	int chunk_cap = batch_size - (batch_size % 12);

	// First pass: just count chunks, to size the buffer/offset-table
	// allocations up front.
	uint32_t chunk_count = 0;
	int last_index = -1;
	for (int i = 0; i < data->material_index_count; i++) {
		int idxs_to_draw = (data->material_indices[i].end - last_index);

		while (idxs_to_draw > 0) {
			int count = idxs_to_draw < chunk_cap ? idxs_to_draw : chunk_cap;
			idxs_to_draw -= count;
			chunk_count++;
		}

		last_index = data->material_indices[i].end;
	}

	if (chunk_count == 0) {
		free(chain->buffer);
		free(chain->chunk_offset);
		chain->buffer = NULL;
		chain->chunk_offset = NULL;
		chain->qwc_alloc = 0;
		chain->chunk_count = 0;
		chain->mpg_addr = mpg_addr;
		chain->built_version = data->chain_version;
		return;
	}

	// Worst case per chunk: diffuse + skin_data + positions + colours + uvs
	// (1 quadword each) + trailer (cnt-tag + FLUSHA/NOP/ITOP/MSCALF, 2
	// quadwords) + DMA_RET (1 quadword) = 8. A generous fixed upper bound
	// keeps this a single allocation pass; this buffer is built once (not
	// per frame), so wasted tail bytes are harmless.
	uint32_t qwc_budget = chunk_count * 8;

	if (chain->qwc_alloc < qwc_budget) {
		free(chain->buffer);
		chain->buffer = (owl_qword*)memalign(16, qwc_budget * sizeof(owl_qword));
		chain->qwc_alloc = qwc_budget;
	}

	free(chain->chunk_offset);
	chain->chunk_offset = (uint32_t*)malloc(chunk_count * sizeof(uint32_t));
	chain->chunk_count = chunk_count;
	chain->mpg_addr = mpg_addr;
	chain->built_version = data->chain_version;

	owl_packet chain_pkt = { 0 };
	chain_pkt.ptr = chain->buffer;

	uint32_t chunk_idx = 0;
	last_index = -1;

	for (int i = 0; i < data->material_index_count; i++) {
		bool texture_mapping = ((((data->materials[data->material_indices[i].index].texture_id != -1)) && data->attributes.texture_mapping) || pass_state);

		athena_compact_position* positions = &data->compact_positions[last_index+1];
		athena_compact_color* colours = &data->compact_colors[last_index+1];
		athena_compact_uv* texcoords = texture_mapping? &data->compact_uvs[last_index+1] : NULL;
		vertex_skin_data* skin_data = data->skin_data? &data->skin_data[last_index+1] : NULL;

		int idxs_to_draw = (data->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			int count = chunk_cap;
			if (idxs_to_draw < chunk_cap) {
				count = idxs_to_draw;
			}

			chain->chunk_offset[chunk_idx] = (uint32_t)(chain_pkt.ptr - chain->buffer);

			owl_add_unpack_data_ref(&chain_pkt, 1, (void*)&data->materials[data->material_indices[i].index].diffuse, 1, 1);

			if (data->skin_data)
				owl_add_unpack_data_ref(&chain_pkt, 2, &skin_data[idxs_drawn], count*2, 1);

			owl_add_unpack_data_ref_packed(&chain_pkt, 2+batch_size*(data->skin_data? 2 : 0), &positions[idxs_drawn], count, UNPACK_V3_32, sizeof(athena_compact_position), 1, 1);
			owl_add_unpack_data_ref_packed(&chain_pkt, 2+batch_size*(data->skin_data? 4 : 2), &colours[idxs_drawn], count, UNPACK_V4_8, sizeof(athena_compact_color), 1, 1);

			if (texcoords)
				owl_add_unpack_data_ref_packed(&chain_pkt, 2+batch_size*(data->skin_data? 5 : 3), &texcoords[idxs_drawn], count, UNPACK_V2_16, sizeof(athena_compact_uv), 1, 0);

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

	SyncDCache(chain->buffer, &chain->buffer[qwc_budget - 1]);
}

void draw_vu1_with_colors(athena_object_data *obj, int pass_state) {
	athena_render_data *data = obj->data;

	render_cook_compact_vertices(data);

	int batch_size = BATCH_SIZE, mpg_addr = 0;

	if (data->skeleton) {
		batch_size = BATCH_SIZE_SKINNED;

		process_animation(obj);

		update_bone_transforms(obj);

		mpg_addr = vu_mpg_preload(vu1_colors_skinned, true);
	} else {
		mpg_addr = vu_mpg_preload(vu1_colors, true);
	}

	// pass_state selects which of the 3 per-pass_state cache slots this
	// draw uses -- see athena_render_data.colors_chain's doc comment for
	// why a single shared slot isn't safe here (same-frame, unflushed
	// back-to-back draws with different pass_state on render_object()'s
	// base/decal/bump sequence). mpg_addr going stale (VU code cache
	// eviction, see mpg_manager.c) also forces a rebuild of this slot --
	// rare enough that patching just the cached MSCALF/MSCNT immediates
	// isn't worth the extra bookkeeping yet.
	athena_chain_cache *chain = &data->colors_chain[pass_state];
	if (!chain->buffer || chain->built_version != data->chain_version || chain->mpg_addr != mpg_addr) {
		render_build_colors_chain(obj, pass_state, batch_size, mpg_addr);
	}

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 14);

	if (obj->bone_matrices) {
		owl_add_unpack_data_ref(packet, 141, (void*)obj->bone_matrices, data->skeleton->bone_count*4, 0);
	}

	owl_add_unpack_data_cnt(packet, 0, 1, 0);
	owl_add_uquad_ptr(packet, &screen_scale);

	owl_add_unpack_data_cnt(packet, 1, 4, 0);
	owl_add_uquad_ptr(packet, &(world_screen[0]));
	owl_add_uquad_ptr(packet, &(world_screen[4]));
	owl_add_uquad_ptr(packet, &(world_screen[8]));
	owl_add_uquad_ptr(packet, &(world_screen[12]));

	owl_add_unpack_data_cnt(packet, 5, 4, 0);
	owl_add_uquad_ptr(packet, &(obj->transform[0]));
	owl_add_uquad_ptr(packet, &(obj->transform[4]));
	owl_add_uquad_ptr(packet, &(obj->transform[8]));
	owl_add_uquad_ptr(packet, &(obj->transform[12]));

	owl_add_unpack_data_cnt(packet, 269, 1, 0);
	owl_add_uquad_ptr(packet, obj->bump_offset_buffer);

	//owl_add_end_tag(packet);

	// batch_size drives the VU-mem destination-offset formulas baked into
	// the pre-cooked chain (2+batch_size*N, see render_build_colors_chain)
	// and must stay in lockstep with mem_layout.i's SKINNED_*_OFFSET
	// strides -- never chunk against it directly. chunk_cap floors it to a
	// multiple of 12 (not just 4): a multiple of 4 keeps
	// owl_add_unpack_data_ref_packed's element-count rounding (see
	// owl_packet.h) from reading/writing past the reserved stride, but for
	// non-tristrip (indexed triangle list) meshes a chunk boundary that
	// isn't ALSO a multiple of 3 splits a triangle's vertices across two
	// separate XGKICKs -- the GS regroups the orphaned 1-2 vertices with
	// whatever starts the next chunk, joining unrelated triangles across
	// the mesh and dropping/mangling the split one. BATCH_SIZE (48) and
	// BATCH_SIZE_SKINNED (30) were already both multiples of 3 by original
	// design; a naive floor-to-4 (e.g. 30->28) silently breaks that.
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
			}
		}

		int idxs_to_draw = (data->material_indices[i].end-last_index);

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, texture_mapping? 20 : 8);

			int count = chunk_cap;
			if (idxs_to_draw < chunk_cap)
			{
				count = idxs_to_draw;
			}

			// Texture upload trigger + GS TEX0/TEX1 stay dynamic (live VRAM
			// address, subject to eviction -- see athena_chain_cache's doc
			// comment) and bake_giftags() stays dynamic (embeds
			// gsGlobal->PrimContext, which flips every frame). Everything
			// else for this chunk (diffuse/skin_data/positions/colours/uvs
			// unpacks + the FLUSHA/NOP/ITOP/MSCALF-or-MSCNT trigger) lives
			// in the pre-baked chain reached via DMA_CALL below -- its
			// trailing FLUSHA there still runs after these GS register
			// writes complete, so draw ordering is unaffected by moving the
			// TEX0/TEX1 emission ahead of the vertex-unpack chain.
			if (texture_mapping) {
				append_texture_tags(packet, tex, texture_id, COLOR_MODULATE);
			}

			bake_giftags(packet, data, texture_mapping, i);

			if (texture_mapping) {
				owl_add_cnt_tag(packet, 4, owl_vif_code_double(VIF_CODE(0, 0, VIF_NOP, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

				owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
				owl_add_uint(packet, VIF_CODE(3, 0, VIF_DIRECT, 0));

				owl_add_tag(packet, GIF_AD, GIFTAG(2, 1, 0, 0, 0, 1));

				int tw, th;
				athena_set_tw_th(tex, &tw, &th);

				owl_add_tag(packet,
					GS_TEX0_1+gsGlobal->PrimContext,
					GS_SETREG_TEX0((tex->Vram & ~TRANSFER_REQUEST_MASK)/256,
								  tex->TBW,
								  tex->PSM,
								  tw, th,
								  gsGlobal->PrimAlphaEnable,
								  COLOR_MODULATE,
								  (tex->VramClut & ~TRANSFER_REQUEST_MASK)/256,
								  tex->ClutPSM,
								  0, 0,
								  tex->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
				);

				owl_add_tag(packet, GS_TEX1_1+gsGlobal->PrimContext, GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0));
			}

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

		process_animation(obj);

		update_bone_transforms(obj);

		mpg_addr = vu_mpg_preload(vu1_lights_skinned, true);
	} else {
		mpg_addr = vu_mpg_preload(vu1_lights, true);
	}

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 16); // 5 for unpack static data + 2 for flush with end

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSHE, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

	owl_add_unpack_data_cnt(packet, 0, 1, 0);
	owl_add_uquad_ptr(packet, &screen_scale);

	owl_add_unpack_data_cnt(packet, 1, 4, 0);
	owl_add_uquad_ptr(packet, &(world_screen[0]));
	owl_add_uquad_ptr(packet, &(world_screen[4]));
	owl_add_uquad_ptr(packet, &(world_screen[8]));
	owl_add_uquad_ptr(packet, &(world_screen[12]));

	owl_add_unpack_data_cnt(packet, 5, 4, 0);
	owl_add_uquad_ptr(packet, &(obj->transform[0]));
	owl_add_uquad_ptr(packet, &(obj->transform[4]));
	owl_add_uquad_ptr(packet, &(obj->transform[8]));
	owl_add_uquad_ptr(packet, &(obj->transform[12]));

	owl_add_unpack_data_cnt(packet, 9, 1, 0);
	owl_add_uquad_ptr(packet, getCameraPosition());

	if (obj->bone_matrices) {
		owl_add_unpack_data_ref(packet, 141, (void*)obj->bone_matrices, data->skeleton->bone_count*4, 0);
	}

	// batch_size drives the VU-mem destination-offset formulas below
	// (2+batch_size*N) and must stay in lockstep with mem_layout.i's
	// SKINNED_*_OFFSET strides -- never chunk against it directly.
	// chunk_cap floors it to a multiple of 12 (not just 4): a multiple of 4
	// keeps owl_add_unpack_data_ref_packed's element-count rounding (see
	// owl_packet.h) from reading/writing past the reserved stride, but for
	// non-tristrip (indexed triangle list) meshes a chunk boundary that
	// isn't ALSO a multiple of 3 splits a triangle's vertices across two
	// separate XGKICKs -- the GS regroups the orphaned 1-2 vertices with
	// whatever starts the next chunk, joining unrelated triangles across
	// the mesh and dropping/mangling the split one. BATCH_SIZE (48) and
	// BATCH_SIZE_SKINNED (30) were already both multiples of 3 by original
	// design; a naive floor-to-4 (e.g. 30->28) silently breaks that.
	int chunk_cap = batch_size - (batch_size % 12);

	int last_index = -1;
	GSSURFACE* tex = NULL;
	int texture_id;
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
			}
		}

		athena_compact_position* positions = &data->compact_positions[last_index+1];
		athena_compact_uv* texcoords = texture_mapping? &data->compact_uvs[last_index+1] : NULL;
		athena_compact_normal* normals = &data->compact_normals[last_index+1];
		athena_compact_color* colours = &data->compact_colors[last_index+1];
		vertex_skin_data* skin_data = data->skin_data? &data->skin_data[last_index+1] : NULL;

		int idxs_to_draw = (data->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, texture_mapping? 22 : 12);

			int count = chunk_cap;
			if (idxs_to_draw < chunk_cap)
			{
				count = idxs_to_draw;
			}

			if (texture_mapping) {
				append_texture_tags(packet, tex, texture_id, COLOR_MODULATE);
			}

			bake_giftags(packet, data, texture_mapping, i);

			owl_add_unpack_data_ref(packet, 1, (void*)&data->materials[data->material_indices[i].index].diffuse, 1, 1);

			if (data->skin_data)
				owl_add_unpack_data_ref(packet, 2, &skin_data[idxs_drawn], count*2, 1);

			owl_add_unpack_data_ref_packed(packet, 2+batch_size*(data->skin_data? 2 : 0), &positions[idxs_drawn], count, UNPACK_V3_32, sizeof(athena_compact_position), 1, 1);
			owl_add_unpack_data_ref_packed(packet, 2+batch_size*(data->skin_data? 3 : 1), &normals[idxs_drawn], count, UNPACK_V4_8, sizeof(athena_compact_normal), 1, 0);
			owl_add_unpack_data_ref_packed(packet, 2+batch_size*(data->skin_data? 4 : 2), &colours[idxs_drawn], count, UNPACK_V4_8, sizeof(athena_compact_color), 1, 1);

			if (texcoords)
				owl_add_unpack_data_ref_packed(packet, 2+batch_size*(data->skin_data? 5 : 3), &texcoords[idxs_drawn], count, UNPACK_V2_16, sizeof(athena_compact_uv), 1, 0);

			owl_add_cnt_tag(packet, texture_mapping? 5 : 1, owl_vif_code_double(VIF_CODE(0, 0, VIF_NOP, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

			if (texture_mapping) {
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
				owl_add_uint(packet, VIF_CODE(3, 0, VIF_DIRECT, 0));

				owl_add_tag(packet, GIF_AD, GIFTAG(2, 1, 0, 0, 0, 1));

				int tw, th;
				athena_set_tw_th(tex, &tw, &th);

				owl_add_tag(packet,
					GS_TEX0_1+gsGlobal->PrimContext,
					GS_SETREG_TEX0((tex->Vram & ~TRANSFER_REQUEST_MASK)/256,
								  tex->TBW,
								  tex->PSM,
								  tw, th,
								  gsGlobal->PrimAlphaEnable,
								  COLOR_MODULATE,
								  (tex->VramClut & ~TRANSFER_REQUEST_MASK)/256,
								  tex->ClutPSM,
								  0, 0,
								  tex->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
				);

				owl_add_tag(packet, GS_TEX1_1+gsGlobal->PrimContext, GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0));
			}

			owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(packet, VIF_CODE(count, 0, VIF_ITOP, 0));
			owl_add_uint(packet, VIF_CODE(mpg_addr, 0, (last_index == -1? VIF_MSCALF : VIF_MSCNT), 0));

			idxs_to_draw -= count;
			idxs_drawn += count;

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

		process_animation(obj);

		update_bone_transforms(obj);

		mpg_addr = vu_mpg_preload(vu1_specular_skinned, true);
	} else {
		mpg_addr = vu_mpg_preload(vu1_specular, true);
	}

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 16); // 5 for unpack static data + 2 for flush with end

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSHE, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

	owl_add_unpack_data_cnt(packet, 0, 1, 0);
	owl_add_uquad_ptr(packet, &screen_scale);

	owl_add_unpack_data_cnt(packet, 1, 4, 0);
	owl_add_uquad_ptr(packet, &(world_screen[0]));
	owl_add_uquad_ptr(packet, &(world_screen[4]));
	owl_add_uquad_ptr(packet, &(world_screen[8]));
	owl_add_uquad_ptr(packet, &(world_screen[12]));

	owl_add_unpack_data_cnt(packet, 5, 4, 0);
	owl_add_uquad_ptr(packet, &(obj->transform[0]));
	owl_add_uquad_ptr(packet, &(obj->transform[4]));
	owl_add_uquad_ptr(packet, &(obj->transform[8]));
	owl_add_uquad_ptr(packet, &(obj->transform[12]));

	owl_add_unpack_data_cnt(packet, 9, 1, 0);
	owl_add_uquad_ptr(packet, getCameraPosition());

	if (obj->bone_matrices) {
		owl_add_unpack_data_ref(packet, 141, (void*)obj->bone_matrices, data->skeleton->bone_count*4, 0);
	}

	//owl_add_end_tag(packet);

	// batch_size drives the VU-mem destination-offset formulas below
	// (2+batch_size*N) and must stay in lockstep with mem_layout.i's
	// SKINNED_*_OFFSET strides -- never chunk against it directly.
	// chunk_cap floors it to a multiple of 12 (not just 4): a multiple of 4
	// keeps owl_add_unpack_data_ref_packed's element-count rounding (see
	// owl_packet.h) from reading/writing past the reserved stride, but for
	// non-tristrip (indexed triangle list) meshes a chunk boundary that
	// isn't ALSO a multiple of 3 splits a triangle's vertices across two
	// separate XGKICKs -- the GS regroups the orphaned 1-2 vertices with
	// whatever starts the next chunk, joining unrelated triangles across
	// the mesh and dropping/mangling the split one. BATCH_SIZE (48) and
	// BATCH_SIZE_SKINNED (30) were already both multiples of 3 by original
	// design; a naive floor-to-4 (e.g. 30->28) silently breaks that.
	int chunk_cap = batch_size - (batch_size % 12);

	int last_index = -1;
	GSSURFACE* tex = NULL;
	int texture_id;
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
			}
		}

		athena_compact_position* positions = &data->compact_positions[last_index+1];
		athena_compact_uv* texcoords = texture_mapping? &data->compact_uvs[last_index+1] : NULL;
		athena_compact_normal* normals = &data->compact_normals[last_index+1];
		athena_compact_color* colours = &data->compact_colors[last_index+1];
		vertex_skin_data* skin_data = data->skin_data? &data->skin_data[last_index+1] : NULL;

		int idxs_to_draw = (data->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, texture_mapping? 21 : 11);

			int count = chunk_cap;
			if (idxs_to_draw < chunk_cap)
			{
				count = idxs_to_draw;
			}
			if (texture_mapping) {
				append_texture_tags(packet, tex, texture_id, COLOR_MODULATE);
			}

			bake_giftags(packet, data, texture_mapping, i);

			owl_add_unpack_data_ref(packet, 1, (void*)&data->materials[data->material_indices[i].index].diffuse, 1, 1);

			if (data->skin_data)
				owl_add_unpack_data_ref(packet, 2, &skin_data[idxs_drawn], count*2, 1);

			owl_add_unpack_data_ref_packed(packet, 2+batch_size*(data->skin_data? 2 : 0), &positions[idxs_drawn], count, UNPACK_V3_32, sizeof(athena_compact_position), 1, 1);
			owl_add_unpack_data_ref_packed(packet, 2+batch_size*(data->skin_data? 3 : 1), &normals[idxs_drawn], count, UNPACK_V4_8, sizeof(athena_compact_normal), 1, 0);
			owl_add_unpack_data_ref_packed(packet, 2+batch_size*(data->skin_data? 4 : 2), &colours[idxs_drawn], count, UNPACK_V4_8, sizeof(athena_compact_color), 1, 1);

			if (texcoords)
				owl_add_unpack_data_ref_packed(packet, 2+batch_size*(data->skin_data? 5 : 3), &texcoords[idxs_drawn], count, UNPACK_V2_16, sizeof(athena_compact_uv), 1, 0);

			owl_add_cnt_tag(packet, texture_mapping? 5 : 1, owl_vif_code_double(VIF_CODE(0, 0, VIF_NOP, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

			if (texture_mapping) {
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
				owl_add_uint(packet, VIF_CODE(3, 0, VIF_DIRECT, 0));

				owl_add_tag(packet, GIF_AD, GIFTAG(2, 1, 0, 0, 0, 1));

				int tw, th;
				athena_set_tw_th(tex, &tw, &th);

				owl_add_tag(packet,
					GS_TEX0_1+gsGlobal->PrimContext,
					GS_SETREG_TEX0((tex->Vram & ~TRANSFER_REQUEST_MASK)/256,
								  tex->TBW,
								  tex->PSM,
								  tw, th,
								  gsGlobal->PrimAlphaEnable,
								  COLOR_MODULATE,
								  (tex->VramClut & ~TRANSFER_REQUEST_MASK)/256,
								  tex->ClutPSM,
								  0, 0,
								  tex->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
				);

				owl_add_tag(packet, GS_TEX1_1+gsGlobal->PrimContext, GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0));
			}

			owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(packet, VIF_CODE(count, 0, VIF_ITOP, 0));
			owl_add_uint(packet, VIF_CODE(mpg_addr, 0, (last_index == -1? VIF_MSCALF : VIF_MSCNT), 0));

			idxs_to_draw -= count;
			idxs_drawn += count;
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

		process_animation(obj);

		update_bone_transforms(obj);

		mpg_addr = vu_mpg_preload(vu1_lights_reflection, true);
	} else {
		mpg_addr = vu_mpg_preload(vu1_lights_reflection, true);
	}
		
	

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 16); // 5 for unpack static data + 2 for flush with end

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSHE, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

	owl_add_unpack_data_cnt(packet, 0, 1, 0);
	owl_add_uquad_ptr(packet, &screen_scale);

	owl_add_unpack_data_cnt(packet, 1, 4, 0);
	owl_add_uquad_ptr(packet, &(world_screen[0]));
	owl_add_uquad_ptr(packet, &(world_screen[4]));
	owl_add_uquad_ptr(packet, &(world_screen[8]));
	owl_add_uquad_ptr(packet, &(world_screen[12]));

	owl_add_unpack_data_cnt(packet, 5, 4, 0);
	owl_add_uquad_ptr(packet, &(obj->transform[0]));
	owl_add_uquad_ptr(packet, &(obj->transform[4]));
	owl_add_uquad_ptr(packet, &(obj->transform[8]));
	owl_add_uquad_ptr(packet, &(obj->transform[12]));

	owl_add_unpack_data_cnt(packet, 9, 1, 0);
	owl_add_uquad_ptr(packet, getCameraPosition());

	if (obj->bone_matrices) {
		owl_add_unpack_data_ref(packet, 141, (void*)obj->bone_matrices, data->skeleton->bone_count*4, 0);
	}

	//owl_add_end_tag(packet);

	// batch_size drives the VU-mem destination-offset formulas below
	// (2+batch_size*N) and must stay in lockstep with mem_layout.i's
	// SKINNED_*_OFFSET strides -- never chunk against it directly.
	// chunk_cap floors it to a multiple of 12 (not just 4): a multiple of 4
	// keeps owl_add_unpack_data_ref_packed's element-count rounding (see
	// owl_packet.h) from reading/writing past the reserved stride, but for
	// non-tristrip (indexed triangle list) meshes a chunk boundary that
	// isn't ALSO a multiple of 3 splits a triangle's vertices across two
	// separate XGKICKs -- the GS regroups the orphaned 1-2 vertices with
	// whatever starts the next chunk, joining unrelated triangles across
	// the mesh and dropping/mangling the split one. BATCH_SIZE (48) and
	// BATCH_SIZE_SKINNED (30) were already both multiples of 3 by original
	// design; a naive floor-to-4 (e.g. 30->28) silently breaks that.
	int chunk_cap = batch_size - (batch_size % 12);

	int last_index = -1;
	GSSURFACE* tex = NULL;
	int texture_id;
	for(int i = 0; i < data->material_index_count; i++) {
		bool texture_mapping = ((data->materials[data->material_indices[i].index].ref_texture_id != -1));

		if (texture_mapping) {
			GSSURFACE *cur_tex = data->textures[data->materials[data->material_indices[i].index].ref_texture_id];
			if (cur_tex != tex) {
				texture_id = texture_manager_bind(gsGlobal, cur_tex, true);
				tex = cur_tex;
			}
		} else {
			continue;
		}

		athena_compact_position* positions = &data->compact_positions[last_index+1];
		athena_compact_normal* normals = &data->compact_normals[last_index+1];
		athena_compact_color* colours = &data->compact_colors[last_index+1];
		vertex_skin_data* skin_data = data->skin_data? &data->skin_data[last_index+1] : NULL;

		int idxs_to_draw = (data->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, texture_mapping? 21 : 11);

			int count = chunk_cap;
			if (idxs_to_draw < chunk_cap)
			{
				count = idxs_to_draw;
			}

			if (texture_mapping) {
				append_texture_tags(packet, tex, texture_id, COLOR_MODULATE);
			}

			bake_giftags(packet, data, texture_mapping, i);

			owl_add_unpack_data_ref(packet, 1, (void*)&data->materials[data->material_indices[i].index].diffuse, 1, 1);

			if (data->skin_data) {
				// unpack_list_append(packet, &skin_data[idxs_drawn], count*2);
			}

			owl_add_unpack_data_ref_packed(packet, 2,                &positions[idxs_drawn], count, UNPACK_V3_32, sizeof(athena_compact_position), 1, 1);
			owl_add_unpack_data_ref_packed(packet, 2+batch_size,     &normals[idxs_drawn], count, UNPACK_V4_8, sizeof(athena_compact_normal), 1, 0);
			owl_add_unpack_data_ref_packed(packet, 2+(batch_size*2), &colours[idxs_drawn], count, UNPACK_V4_8, sizeof(athena_compact_color), 1, 1);

			owl_add_cnt_tag(packet, texture_mapping? 5 : 1, owl_vif_code_double(VIF_CODE(0, 0, VIF_NOP, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

			if (texture_mapping) {
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
				owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
				owl_add_uint(packet, VIF_CODE(3, 0, VIF_DIRECT, 0)); 

				owl_add_tag(packet, GIF_AD, GIFTAG(2, 1, 0, 0, 0, 1));

				int tw, th;
				athena_set_tw_th(tex, &tw, &th);

				owl_add_tag(packet, 
					GS_TEX0_1+gsGlobal->PrimContext, 
					GS_SETREG_TEX0((tex->Vram & ~TRANSFER_REQUEST_MASK)/256, 
								  tex->TBW, 
								  tex->PSM,
								  tw, th, 
								  gsGlobal->PrimAlphaEnable, 
								  COLOR_MODULATE,
								  (tex->VramClut & ~TRANSFER_REQUEST_MASK)/256, 
								  tex->ClutPSM, 
								  0, 0, 
								  tex->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
				);

				owl_add_tag(packet, GS_TEX1_1+gsGlobal->PrimContext, GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0));
			}
			
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(packet, VIF_CODE(count, 0, VIF_ITOP, 0));
			owl_add_uint(packet, VIF_CODE(mpg_addr, 0, (last_index == -1? VIF_MSCALF : VIF_MSCNT), 0)); 

			idxs_to_draw -= count;
			idxs_drawn += count;
			
		}

		last_index = data->material_indices[i].end;
	}

	owl_query_packet(CHANNEL_VIF1, 1);

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSH, 0), VIF_CODE(0, 0, VIF_FLUSH, 0)));
}
