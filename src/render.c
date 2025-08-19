#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>
#include <matrix.h>
#include <render.h>
#include <dbgprintf.h>

#include <owl_packet.h>

#include <mpg_manager.h>

#include <texture_manager.h>z

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

void render_init() {
	initCamera(&world_screen, &world_view, &view_screen);
	
	vu1_set_double_buffer_settings(270, 339); // Skinned layout
	owl_flush_packet();

	vu1_colors   = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DCS),   embed_vu_code_size(VU1Draw3DCS),   VECTOR_UNIT_1, false); 
	vu1_lights   = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCS),  embed_vu_code_size(VU1Draw3DLCS),  VECTOR_UNIT_1, false);
	vu1_specular = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCSS), embed_vu_code_size(VU1Draw3DLCSS), VECTOR_UNIT_1, false);

	vu1_colors_skinned = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DCS_Skin), embed_vu_code_size(VU1Draw3DCS_Skin), VECTOR_UNIT_1, false);
	vu1_lights_skinned = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCS_Skin), embed_vu_code_size(VU1Draw3DLCS_Skin), VECTOR_UNIT_1, false);
	vu1_specular_skinned = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCSS_Skin), embed_vu_code_size(VU1Draw3DLCSS_Skin), VECTOR_UNIT_1, false);

	vu1_lights_reflection = vu_mpg_load_buffer(embed_vu_code_ptr(VU1Draw3DLCS_Ref), embed_vu_code_size(VU1Draw3DLCS_Ref), VECTOR_UNIT_1, false);
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

static int active_aaa_lights = 0;
static int active_bbb_lights = 0;
static int active_pnt_lights = 0;
static int active_dir_lights = 0;

static LightData dir_lights;

int NewLight() {
	if (active_dir_lights < 4) {
		dir_lights.ambient[0].w = active_dir_lights+1;
		return active_dir_lights++;
	}
		
	return -1;
}
 
void SetLightAttribute(int id, float x, float y, float z, int attr) {
	if (id < 0)
		return;

	switch (attr) {
		case ATHENA_LIGHT_DIRECTION:
			dir_lights.direction[id][0] = x;
			dir_lights.direction[id][1] = y;
			dir_lights.direction[id][2] = z;
			break;
		case ATHENA_LIGHT_AMBIENT:
			dir_lights.ambient[id].x = x;
			dir_lights.ambient[id].y = y;
			dir_lights.ambient[id].z = z;
			break;
		case ATHENA_LIGHT_DIFFUSE:
			dir_lights.diffuse[id][0] = x;
			dir_lights.diffuse[id][1] = y;
			dir_lights.diffuse[id][2] = z;
			break;
		case ATHENA_LIGHT_SPECULAR:
			dir_lights.specular[id][0] = x;
			dir_lights.specular[id][1] = y;
			dir_lights.specular[id][2] = z;
			break;
	}
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

void bake_giftags(owl_packet *packet, athena_render_data *data, bool texture_mapping, int mat_id) {
	prim_reg_t prim_data = {
		.PRIM = GS_PRIM_PRIM_TRIFAN,
		.IIP = data->attributes.shade_model,
		.TME = 1,
		.FGE = gsGlobal->PrimFogEnable,
		.ABE = gsGlobal->PrimAlphaEnable,
		.AA1 = gsGlobal->PrimAAEnable,
		.FST = 0,
		.CTXT = gsGlobal->PrimContext,
		.FIX = 0
	};

	prim_reg_t notm_prim_data = prim_data;
	notm_prim_data.TME = 0;

	giftag_t clip_tag = {
		.NLOOP = 0,
		.EOP = 1,
		.PRE = 1,
		.PRIM = prim_data.data,
		.FLG = 0,
		.NREG = 3
	};

	giftag_t notm_clip_tag = clip_tag;
	notm_clip_tag.PRIM = notm_prim_data.data;

	prim_data.PRIM = (data->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE);
	notm_prim_data.PRIM = prim_data.PRIM;

	giftag_t prim_tag = {
		.NLOOP = 0,
		.EOP = 1,
		.PRE = 1,
		.PRIM = prim_data.data,
		.FLG = 0,
		.NREG = 3
	};

	giftag_t notm_prim_tag = prim_tag;
	notm_prim_tag.PRIM = notm_prim_data.data;

	data->materials[data->material_indices[mat_id].index].prim_tag.dword[1] = DRAW_STQ2_REGLIST;
	data->materials[data->material_indices[mat_id].index].prim_tag.dword[0] = prim_tag.data;

	data->materials[data->material_indices[mat_id].index].notm_prim_tag.dword[1] = DRAW_STQ2_REGLIST;
	data->materials[data->material_indices[mat_id].index].notm_prim_tag.dword[0] = notm_prim_tag.data;

	data->materials[data->material_indices[mat_id].index].clip_tag.f[3] = data->attributes.face_culling;
	data->materials[data->material_indices[mat_id].index].clip_tag.sword[2] = data->tristrip;
	data->materials[data->material_indices[mat_id].index].clip_tag.sword[1] = data->attributes.accurate_clipping? (clip_tag.data >> 32) : 0;

	data->materials[data->material_indices[mat_id].index].notm_clip_tag.f[3] = data->attributes.face_culling;
	data->materials[data->material_indices[mat_id].index].notm_clip_tag.sword[2] = data->tristrip;
	data->materials[data->material_indices[mat_id].index].notm_clip_tag.sword[1] = data->attributes.accurate_clipping? (notm_clip_tag.data >> 32) : 0;

	if (texture_mapping) {
		owl_add_unpack_data(packet, 26, (void*)&data->materials[data->material_indices[mat_id].index].clip_tag, 1, 0);
		owl_add_unpack_data(packet, 0, (void*)&data->materials[data->material_indices[mat_id].index].prim_tag, 1, 1);
	} else {
		owl_add_unpack_data(packet, 26, (void*)&data->materials[data->material_indices[mat_id].index].notm_clip_tag, 1, 0);
		owl_add_unpack_data(packet, 0, (void*)&data->materials[data->material_indices[mat_id].index].notm_prim_tag, 1, 1);
	}
}

void draw_vu1_with_colors(athena_object_data *obj, int pass_state) {
	athena_render_data *data = obj->data;

	int batch_size = BATCH_SIZE, mpg_addr = 0;

	if (data->skeleton) {
		batch_size = BATCH_SIZE_SKINNED;

		process_animation(obj);

		update_bone_transforms(obj);

		mpg_addr = vu_mpg_preload(vu1_colors_skinned, true);
	} else {
		mpg_addr = vu_mpg_preload(vu1_colors, true);
	}
	
	

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 8);

	if (obj->bone_matrices) {
		owl_add_unpack_data(packet, 141, (void*)obj->bone_matrices, data->skeleton->bone_count*4, 0);
	}

	owl_add_unpack_data(packet, 0, (void*)&screen_scale, 1, 0);
	owl_add_unpack_data(packet, 1, (void*)world_screen, 4, 0);
	owl_add_unpack_data(packet, 5, (void*)obj->transform, 4, 0);

	owl_add_unpack_data(packet, 269, (void*)obj->bump_offset_buffer, 1, 0);

	//owl_add_end_tag(packet);

	int last_index = -1;
	GSSURFACE* tex = NULL;
	int texture_id;
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

		VECTOR* positions = &data->positions[last_index+1];
		VECTOR* colours = &data->colours[last_index+1];
		VECTOR* texcoords = texture_mapping? &data->texcoords[last_index+1] : NULL;
		vertex_skin_data* skin_data = data->skin_data? &data->skin_data[last_index+1] : NULL;

		int idxs_to_draw = (data->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, texture_mapping? 18 : 8);    

			int count = batch_size;
			if (idxs_to_draw < batch_size)
			{
				count = idxs_to_draw;
			}

			if (texture_mapping) {
				append_texture_tags(packet, tex, texture_id, COLOR_MODULATE);
			}

			bake_giftags(packet, data, texture_mapping, i);

			owl_add_unpack_data(packet, 1, (void*)&data->materials[data->material_indices[i].index].diffuse, 1, 1);

			if (data->skin_data) 
				owl_add_unpack_data(packet, 2, &skin_data[idxs_drawn], count*2, 1);

			owl_add_unpack_data(packet, 2+batch_size*(data->skin_data? 2 : 0), &positions[idxs_drawn], count, 1);
			//owl_add_unpack_data(packet, 2+batch_size*(data->skin_data? 3 : 1), &normals[idxs_drawn], count, 1);
			owl_add_unpack_data(packet, 2+batch_size*(data->skin_data? 4 : 2), &colours[idxs_drawn], count, 1);

			if (texcoords) 
				owl_add_unpack_data(packet, 2+batch_size*(data->skin_data? 5 : 3), &texcoords[idxs_drawn], count, 1);

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

void draw_vu1_with_lights(athena_object_data *obj, int pass_state) {
	athena_render_data *data = obj->data;

	int batch_size = BATCH_SIZE, mpg_addr = 0;

	if (data->skeleton) {
		batch_size = BATCH_SIZE_SKINNED;

		process_animation(obj);

		update_bone_transforms(obj);

		mpg_addr = vu_mpg_preload(vu1_lights_skinned, true);
	} else {
		mpg_addr = vu_mpg_preload(vu1_lights, true);
	}
		
	

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 10); // 5 for unpack static data + 2 for flush with end

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSHE, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

	owl_add_unpack_data(packet, 0, (void*)&screen_scale, 1, 0);
	owl_add_unpack_data(packet, 1, (void*)world_screen, 4, 0);
	owl_add_unpack_data(packet, 5, (void*)obj->transform, 4, 0);
	owl_add_unpack_data(packet, 9, (void*)getCameraPosition(), 1, 0);
	owl_add_unpack_data(packet, 10, (void*)&dir_lights, 16, 0);

	if (obj->bone_matrices) {
		owl_add_unpack_data(packet, 141, (void*)obj->bone_matrices, data->skeleton->bone_count*4, 0);
	}

	//owl_add_end_tag(packet);

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

		VECTOR* positions = &data->positions[last_index+1];
		VECTOR* texcoords = texture_mapping? &data->texcoords[last_index+1] : NULL;
		VECTOR* normals = &data->normals[last_index+1];
		VECTOR* colours = &data->colours[last_index+1];
		vertex_skin_data* skin_data = data->skin_data? &data->skin_data[last_index+1] : NULL;

		int idxs_to_draw = (data->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, texture_mapping? 20 : 10);

			int count = batch_size;
			if (idxs_to_draw < batch_size)
			{
				count = idxs_to_draw;
			}

			if (texture_mapping) {
				append_texture_tags(packet, tex, texture_id, COLOR_MODULATE);
			}

			bake_giftags(packet, data, texture_mapping, i);
			
			owl_add_unpack_data(packet, 1, (void*)&data->materials[data->material_indices[i].index].diffuse, 1, 1);

			if (data->skin_data) 
				owl_add_unpack_data(packet, 2, &skin_data[idxs_drawn], count*2, 1);

			owl_add_unpack_data(packet, 2+batch_size*(data->skin_data? 2 : 0), &positions[idxs_drawn], count, 1);
			owl_add_unpack_data(packet, 2+batch_size*(data->skin_data? 3 : 1), &normals[idxs_drawn], count, 1);
			owl_add_unpack_data(packet, 2+batch_size*(data->skin_data? 4 : 2), &colours[idxs_drawn], count, 1);

			if (texcoords) 
				owl_add_unpack_data(packet, 2+batch_size*(data->skin_data? 5 : 3), &texcoords[idxs_drawn], count, 1);
			
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

	int batch_size = BATCH_SIZE, mpg_addr = 0;

	if (data->skeleton) {
		batch_size = BATCH_SIZE_SKINNED;

		process_animation(obj);

		update_bone_transforms(obj);

		mpg_addr = vu_mpg_preload(vu1_specular_skinned, true);
	} else {
		mpg_addr = vu_mpg_preload(vu1_specular, true);
	}

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 10);

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSHE, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

	if (obj->bone_matrices) {
		owl_add_unpack_data(packet, 141, (void*)obj->bone_matrices, data->skeleton->bone_count*4, 0);
	}

	owl_add_unpack_data(packet, 0, (void*)&screen_scale, 1, 0);
	owl_add_unpack_data(packet, 1, (void*)world_screen, 4, 0);
	owl_add_unpack_data(packet, 5, (void*)obj->transform, 4, 0);
	owl_add_unpack_data(packet, 9, (void*)getCameraPosition(), 1, 0);
	owl_add_unpack_data(packet, 10, (void*)&dir_lights, 16, 0);

	//owl_add_end_tag(packet);

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

		VECTOR* positions = &data->positions[last_index+1];
		VECTOR* texcoords = texture_mapping? &data->texcoords[last_index+1] : NULL;
		VECTOR* normals = &data->normals[last_index+1];
		VECTOR* colours = &data->colours[last_index+1];
		vertex_skin_data* skin_data = data->skin_data? &data->skin_data[last_index+1] : NULL;

		int idxs_to_draw = (data->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, texture_mapping? 19 : 9);

			int count = batch_size;
			if (idxs_to_draw < batch_size)
			{
				count = idxs_to_draw;
			}
			if (texture_mapping) {
				append_texture_tags(packet, tex, texture_id, COLOR_MODULATE);
			}

			bake_giftags(packet, data, texture_mapping, i);
			
			owl_add_unpack_data(packet, 1, (void*)&data->materials[data->material_indices[i].index].diffuse, 1, 1);

			if (data->skin_data) 
				owl_add_unpack_data(packet, 2, &skin_data[idxs_drawn], count*2, 1);

			owl_add_unpack_data(packet, 2+batch_size*(data->skin_data? 2 : 0), &positions[idxs_drawn], count, 1);
			owl_add_unpack_data(packet, 2+batch_size*(data->skin_data? 3 : 1), &normals[idxs_drawn], count, 1);
			owl_add_unpack_data(packet, 2+batch_size*(data->skin_data? 4 : 2), &colours[idxs_drawn], count, 1);

			if (texcoords) 
				owl_add_unpack_data(packet, 2+batch_size*(data->skin_data? 5 : 3), &texcoords[idxs_drawn], count, 1);

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

	int batch_size = BATCH_SIZE, mpg_addr = 0;

	if (data->skeleton) {
		batch_size = BATCH_SIZE_SKINNED;

		process_animation(obj);

		update_bone_transforms(obj);

		mpg_addr = vu_mpg_preload(vu1_lights_reflection, true);
	} else {
		mpg_addr = vu_mpg_preload(vu1_lights_reflection, true);
	}
		
	

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 10); // 5 for unpack static data + 2 for flush with end

	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSHE, 0), VIF_CODE(0, 0, VIF_NOP, 0)));

	owl_add_unpack_data(packet, 0, (void*)&screen_scale, 1, 0);
	owl_add_unpack_data(packet, 1, (void*)&world_screen, 4, 0);
	owl_add_unpack_data(packet, 5, (void*)obj->transform, 4, 0);
	owl_add_unpack_data(packet, 9, (void*)getCameraPosition(), 1, 0);
	owl_add_unpack_data(packet, 10, (void*)&dir_lights, 16, 0);

	if (obj->bone_matrices) {
		owl_add_unpack_data(packet, 141, (void*)obj->bone_matrices, data->skeleton->bone_count*4, 0);
	}

	//owl_add_end_tag(packet);

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

		VECTOR* positions = &data->positions[last_index+1];
		VECTOR* normals = &data->normals[last_index+1];
		VECTOR* colours = &data->colours[last_index+1];
		vertex_skin_data* skin_data = data->skin_data? &data->skin_data[last_index+1] : NULL;

		int idxs_to_draw = (data->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, texture_mapping? 19 : 9);

			int count = batch_size;
			if (idxs_to_draw < batch_size)
			{
				count = idxs_to_draw;
			}

			if (texture_mapping) {
				append_texture_tags(packet, tex, texture_id, COLOR_MODULATE);
			}

			bake_giftags(packet, data, texture_mapping, i);
			
			owl_add_unpack_data(packet, 1, (void*)&data->materials[data->material_indices[i].index].diffuse, 1, 1);

			if (data->skin_data) {
				// unpack_list_append(packet, &skin_data[idxs_drawn], count*2);
			}

			owl_add_unpack_data(packet, 2,                &positions[idxs_drawn], count, 1);
			owl_add_unpack_data(packet, 2+batch_size,     &normals[idxs_drawn], count, 1);
			owl_add_unpack_data(packet, 2+(batch_size*2), &colours[idxs_drawn], count, 1);

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
