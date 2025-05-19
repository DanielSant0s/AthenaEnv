#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>

#include <render.h>
#include <dbgprintf.h>

#include <owl_packet.h>

#define DEG2RAD(deg) ((deg) * (M_PI / 180.0f))

register_vu_program(VU1Draw3DCS);
register_vu_program(VU1Draw3DLCS);
register_vu_program(VU1Draw3DLCSS);

MATRIX view_screen;
MATRIX world_view;

FIVECTOR screen_scale;

void draw_vu1_with_colors(athena_object_data *obj);
void draw_vu1_with_lights(athena_object_data *obj);
void draw_vu1_with_spec_lights(athena_object_data *obj);

void (*render_funcs[3])(athena_object_data *obj) = {
	draw_vu1_with_colors,
	draw_vu1_with_lights,
	draw_vu1_with_spec_lights
};

void render_object(athena_object_data *obj) {
	render_funcs[obj->data->pipeline](obj);
}

void init3D(float fov, float near, float far)
{
	GSGLOBAL* gsGlobal = getGSGLOBAL();

	initCamera(&world_view);
	create_view(view_screen, DEG2RAD(fov), near, far, gsGlobal->Width, gsGlobal->Height);
	vu1_set_double_buffer_settings(141, 400);
	owl_flush_packet();

	screen_scale.x = gsGlobal->Width/2;
	screen_scale.y = gsGlobal->Height/2;
	screen_scale.z = ((float)get_max_z(gsGlobal));
	screen_scale.w = 0; // athena_render_data attributes

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

static uint32_t* last_mpg = NULL;

#define update_vu_program(name) \
	do { \
		if (last_mpg != &name##_CodeStart) { \
			vu1_upload_micro_program(&name##_CodeStart, &name##_CodeEnd); \
			last_mpg = &name##_CodeStart; \
		} \
	} while (0)

void append_texture_tags(owl_packet* packet, GSTEXTURE *texture, int texture_id, eColorFunctions func) {
	if (texture_id != -1) {
		owl_add_cnt_tag(packet, 8, 0); // 4 quadwords for vif
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

	} else {
		owl_add_cnt_tag(packet, 4, 0);
	}

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(3, 0, VIF_DIRECT, 0)); 
	
	owl_add_tag(packet, GIF_AD, GIFTAG(2, 1, 0, 0, 0, 1));

	int tw, th;
	athena_set_tw_th(texture, &tw, &th);

	owl_add_tag(packet, 
		GS_TEX0_1, 
		GS_SETREG_TEX0((texture->Vram & ~TRANSFER_REQUEST_MASK)/256, 
					  texture->TBW, 
					  texture->PSM,
					  tw, th, 
					  gsGlobal->PrimAlphaEnable, 
					  COLOR_MODULATE,
					  (texture->VramClut & ~TRANSFER_REQUEST_MASK)/256, 
					  texture->ClutPSM, 
					  0, 0, 
					  texture->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
	);
	
	owl_add_tag(packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, texture->Filter, texture->Filter, 0, 0, 0));
}

void draw_vu1_with_colors(athena_object_data *obj) {
	athena_render_data *data = obj->data;
	MATRIX local_world;

	int batch_size = BATCH_SIZE;

	update_vu_program(VU1Draw3DCS);
	
	gsGlobal->PrimAAEnable = GS_SETTING_ON;

	create_local_world(local_world, obj->position, obj->rotation);
	create_local_screen(obj->local_screen, local_world, world_view, view_screen);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 4);

	unpack_list_open(packet, 0, false);
	{
		screen_scale.w = data->attributes.accurate_clipping;
		unpack_list_append(packet, &screen_scale,       1);

		unpack_list_append(packet, obj->local_screen,       4);
	}
	unpack_list_close(packet);

	//owl_add_end_tag(packet);

	int last_index = -1;
	GSTEXTURE* tex = NULL;
	int texture_id;
	for(int i = 0; i < data->material_index_count; i++) {
		bool texture_mapping = ((data->materials[data->material_indices[i].index].texture_id != -1) && data->attributes.texture_mapping);

		if (texture_mapping) {
			GSTEXTURE *cur_tex = data->textures[data->materials[data->material_indices[i].index].texture_id];
			if (cur_tex != tex) {
				texture_id = texture_manager_bind(gsGlobal, cur_tex, true);
				tex = cur_tex;
			}
		}

		VECTOR* positions = &data->positions[last_index+1];
		VECTOR* colours = &data->colours[last_index+1];
		VECTOR* texcoords = texture_mapping? &data->texcoords[last_index+1] : NULL;

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

			giftag_t clip_tag = {
				.data = {
					.NLOOP = 0,
					.EOP = 1,
					.PRE = 1,
					.PRIM = (prim_reg_t) {
						.data = { 
							.PRIM = GS_PRIM_PRIM_TRIFAN,
							.IIP = data->attributes.shade_model,
							.TME = texture_mapping,
							.FGE = gsGlobal->PrimFogEnable,
							.ABE = gsGlobal->PrimAlphaEnable,
							.AA1 = gsGlobal->PrimAAEnable,
							.FST = 0,
							.CTXT = 0,
							.FIX = 0
						}
					}.raw,
					.FLG = 0,
					.NREG = 3

				}
			};

			giftag_t prim_tag = {
				.data = {
					.NLOOP = 0,
					.EOP = 1,
					.PRE = 1,
					.PRIM = (prim_reg_t) {
						.data = { 
							.PRIM = (data->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE),
							.IIP = data->attributes.shade_model,
							.TME = texture_mapping,
							.FGE = gsGlobal->PrimFogEnable,
							.ABE = gsGlobal->PrimAlphaEnable,
							.AA1 = gsGlobal->PrimAAEnable,
							.FST = 0,
							.CTXT = 0,
							.FIX = 0
						}
					}.raw,
					.FLG = 0,
					.NREG = 3

				}
			};

			data->materials[data->material_indices[i].index].prim_tag.dword[1] = DRAW_STQ2_REGLIST;
			data->materials[data->material_indices[i].index].prim_tag.dword[0] = prim_tag.raw;

			data->materials[data->material_indices[i].index].clip_prim_tag.sword[2] = data->tristrip;
			data->materials[data->material_indices[i].index].clip_prim_tag.sword[1] = data->attributes.accurate_clipping? (clip_tag.raw >> 32) : 0;

			owl_add_unpack_data(packet, 26, (void*)&data->materials[data->material_indices[i].index].clip_prim_tag, 1, 0);

			unpack_list_open(packet, 0, true);
			{
				unpack_list_append(packet, (void*)&data->materials[data->material_indices[i].index].prim_tag, 1);
				unpack_list_append(packet, (void*)&data->materials[data->material_indices[i].index].diffuse, 1);
				unpack_list_append(packet, &positions[idxs_drawn], count);
				unpack_list_append(packet, &colours[idxs_drawn], count);
				if (texcoords) 
					unpack_list_append(packet, &texcoords[idxs_drawn], count);
			}
			unpack_list_close(packet);

			owl_add_cnt_tag(packet, 1, owl_vif_code_double(VIF_CODE(0, 0, VIF_NOP, 0), VIF_CODE(0, 0, VIF_NOP, 0)));
			
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
			owl_add_uint(packet, VIF_CODE(count, 0, VIF_ITOP, 0));
			owl_add_uint(packet, VIF_CODE(0, 0, (last_index == -1? VIF_MSCALF : VIF_MSCNT), 0)); 

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		last_index = data->material_indices[i].end;
	}

	owl_add_vif_codes(packet,
		VIF_CODE(0, 0, VIF_FLUSH, 0),
		VIF_CODE(0, 0, VIF_FLUSH, 0),
		VIF_CODE(0, 0, VIF_NOP, 0),
		VIF_CODE(0, 0, VIF_NOP, 0)
	);
}

void draw_vu1_with_lights(athena_object_data *obj) {
	athena_render_data *data = obj->data;

	int batch_size = BATCH_SIZE;

	//if (data->attributes.accurate_clipping) {
		update_vu_program(VU1Draw3DLCS);
	//} else {
		//update_vu_program(VU1Draw3DLightsColors);
		//batch_size = BATCH_SIZE_NO_CLIPPING;
	//}
		
	gsGlobal->PrimAAEnable = GS_SETTING_ON;


	MATRIX local_world;

  	// Create the local_world matrix.
  	matrix_unit(local_world);
  	matrix_rotate(local_world, local_world, obj->rotation);

	matrix_copy(obj->local_light, local_world);

  	matrix_translate(local_world, local_world, obj->position);

  	// Create the local_screen matrix.
  	matrix_unit(obj->local_screen);

  	matrix_multiply(obj->local_screen, obj->local_screen, local_world);
  	matrix_multiply(obj->local_screen, obj->local_screen, world_view);
  	matrix_multiply(obj->local_screen, obj->local_screen, view_screen);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 7); // 5 for unpack static data + 2 for flush with end

	unpack_list_open(packet, 0, false);
	{
		screen_scale.w = data->attributes.accurate_clipping;
		unpack_list_append(packet, &screen_scale,       1); 

		unpack_list_append(packet, obj->local_screen,       4);
		unpack_list_append(packet, obj->local_light,        4);

		unpack_list_append(packet, getCameraPosition(), 1);
		unpack_list_append(packet, &dir_lights,        16);
	}
	unpack_list_close(packet);

	//owl_add_end_tag(packet);

	int last_index = -1;
	GSTEXTURE* tex = NULL;
	int texture_id;
	for(int i = 0; i < data->material_index_count; i++) {
		bool texture_mapping = ((data->materials[data->material_indices[i].index].texture_id != -1) && data->attributes.texture_mapping);

		if (texture_mapping) {
			GSTEXTURE *cur_tex = data->textures[data->materials[data->material_indices[i].index].texture_id];
			if (cur_tex != tex) {
				texture_id = texture_manager_bind(gsGlobal, cur_tex, true);
				tex = cur_tex;
			}
		}

		VECTOR* positions = &data->positions[last_index+1];
		VECTOR* texcoords = texture_mapping? &data->texcoords[last_index+1] : NULL;
		VECTOR* normals = &data->normals[last_index+1];
		VECTOR* colours = &data->colours[last_index+1];

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

			giftag_t clip_tag = {
				.data = {
					.NLOOP = 0,
					.EOP = 1,
					.PRE = 1,
					.PRIM = (prim_reg_t) {
						.data = { 
							.PRIM = GS_PRIM_PRIM_TRIFAN,
							.IIP = data->attributes.shade_model,
							.TME = texture_mapping,
							.FGE = gsGlobal->PrimFogEnable,
							.ABE = gsGlobal->PrimAlphaEnable,
							.AA1 = gsGlobal->PrimAAEnable,
							.FST = 0,
							.CTXT = 0,
							.FIX = 0
						}
					}.raw,
					.FLG = 0,
					.NREG = 3

				}
			};

			giftag_t prim_tag = {
				.data = {
					.NLOOP = 0,
					.EOP = 1,
					.PRE = 1,
					.PRIM = (prim_reg_t) {
						.data = { 
							.PRIM = (data->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE),
							.IIP = data->attributes.shade_model,
							.TME = texture_mapping,
							.FGE = gsGlobal->PrimFogEnable,
							.ABE = gsGlobal->PrimAlphaEnable,
							.AA1 = gsGlobal->PrimAAEnable,
							.FST = 0,
							.CTXT = 0,
							.FIX = 0
						}
					}.raw,
					.FLG = 0,
					.NREG = 3

				}
			};

			data->materials[data->material_indices[i].index].prim_tag.dword[1] = DRAW_STQ2_REGLIST;
			data->materials[data->material_indices[i].index].prim_tag.dword[0] = prim_tag.raw;

			data->materials[data->material_indices[i].index].clip_prim_tag.sword[2] = data->tristrip;
			data->materials[data->material_indices[i].index].clip_prim_tag.sword[1] = (data->attributes.accurate_clipping? (clip_tag.raw >> 32) : 0);

			owl_add_unpack_data(packet, 26, (void*)&data->materials[data->material_indices[i].index].clip_prim_tag, 1, 0);

			unpack_list_open(packet, 0, true);
			{
				unpack_list_append(packet, (void*)&data->materials[data->material_indices[i].index].prim_tag, 1);
				unpack_list_append(packet, (void*)&data->materials[data->material_indices[i].index].diffuse, 1);
				unpack_list_append(packet, &positions[idxs_drawn], count);
				unpack_list_append(packet, &normals[idxs_drawn], count);
				unpack_list_append(packet, &colours[idxs_drawn], count);
				if (texcoords) 
					unpack_list_append(packet, &texcoords[idxs_drawn], count);
			}
			unpack_list_close(packet);

			owl_add_cnt_tag(packet, 1, owl_vif_code_double(VIF_CODE(0, 0, VIF_NOP, 0), VIF_CODE(0, 0, VIF_NOP, 0)));
			
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(packet, VIF_CODE(count, 0, VIF_ITOP, 0));
			owl_add_uint(packet, VIF_CODE(0, 0, (last_index == -1? VIF_MSCALF : VIF_MSCNT), 0)); 

			idxs_to_draw -= count;
			idxs_drawn += count;
			
		}

		last_index = data->material_indices[i].end;
	}

	owl_add_vif_codes(packet,
		VIF_CODE(0, 0, VIF_FLUSH, 0),
		VIF_CODE(0, 0, VIF_FLUSH, 0),
		VIF_CODE(0, 0, VIF_NOP, 0),
		VIF_CODE(0, 0, VIF_NOP, 0)
	);

	
}

void draw_vu1_with_spec_lights(athena_object_data *obj) {
	athena_render_data *data = obj->data;

	int batch_size = BATCH_SIZE;

	update_vu_program(VU1Draw3DLCSS);


	gsGlobal->PrimAAEnable = GS_SETTING_ON;


	MATRIX local_world;

  	// Create the local_world matrix.
  	matrix_unit(local_world);
  	matrix_rotate(local_world, local_world, obj->rotation);

	matrix_copy(obj->local_light, local_world);

  	matrix_translate(local_world, local_world, obj->position);

  	// Create the local_screen matrix.
  	matrix_unit(obj->local_screen);

  	matrix_multiply(obj->local_screen, obj->local_screen, local_world);
  	matrix_multiply(obj->local_screen, obj->local_screen, world_view);
  	matrix_multiply(obj->local_screen, obj->local_screen, view_screen);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 7);

	screen_scale.w = *((uint32_t*)&data->attributes);

	unpack_list_open(packet, 0, false);
	{
		screen_scale.w = data->attributes.accurate_clipping;
		unpack_list_append(packet, &screen_scale,       1);

		unpack_list_append(packet, obj->local_screen,       4);
		unpack_list_append(packet, obj->local_light,        4);

		unpack_list_append(packet, getCameraPosition(), 1);
		unpack_list_append(packet, &dir_lights,         16);
	}
	unpack_list_close(packet);

	//owl_add_end_tag(packet);

	int last_index = -1;
	GSTEXTURE* tex = NULL;
	int texture_id;
	for(int i = 0; i < data->material_index_count; i++) {
		bool texture_mapping = ((data->materials[data->material_indices[i].index].texture_id != -1) && data->attributes.texture_mapping);

		if (texture_mapping) {
			GSTEXTURE *cur_tex = data->textures[data->materials[data->material_indices[i].index].texture_id];
			if (cur_tex != tex) {
				texture_id = texture_manager_bind(gsGlobal, cur_tex, true);
				tex = cur_tex;
			}
		}

		VECTOR* positions = &data->positions[last_index+1];
		VECTOR* texcoords = texture_mapping? &data->texcoords[last_index+1] : NULL;
		VECTOR* normals = &data->normals[last_index+1];
		VECTOR* colours = &data->colours[last_index+1];

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

			giftag_t clip_tag = {
				.data = {
					.NLOOP = 0,
					.EOP = 1,
					.PRE = 1,
					.PRIM = (prim_reg_t) {
						.data = { 
							.PRIM = GS_PRIM_PRIM_TRIFAN,
							.IIP = data->attributes.shade_model,
							.TME = texture_mapping,
							.FGE = gsGlobal->PrimFogEnable,
							.ABE = gsGlobal->PrimAlphaEnable,
							.AA1 = gsGlobal->PrimAAEnable,
							.FST = 0,
							.CTXT = 0,
							.FIX = 0
						}
					}.raw,
					.FLG = 0,
					.NREG = 3

				}
			};

			giftag_t prim_tag = {
				.data = {
					.NLOOP = 0,
					.EOP = 1,
					.PRE = 1,
					.PRIM = (prim_reg_t) {
						.data = { 
							.PRIM = (data->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE),
							.IIP = data->attributes.shade_model,
							.TME = texture_mapping,
							.FGE = gsGlobal->PrimFogEnable,
							.ABE = gsGlobal->PrimAlphaEnable,
							.AA1 = gsGlobal->PrimAAEnable,
							.FST = 0,
							.CTXT = 0,
							.FIX = 0
						}
					}.raw,
					.FLG = 0,
					.NREG = 3

				}
			};

			data->materials[data->material_indices[i].index].prim_tag.dword[1] = DRAW_STQ2_REGLIST;
			data->materials[data->material_indices[i].index].prim_tag.dword[0] = prim_tag.raw;

			data->materials[data->material_indices[i].index].clip_prim_tag.sword[2] = data->tristrip;
			data->materials[data->material_indices[i].index].clip_prim_tag.sword[1] = data->attributes.accurate_clipping? (clip_tag.raw >> 32) : 0;

			owl_add_unpack_data(packet, 26, (void*)&data->materials[data->material_indices[i].index].clip_prim_tag, 1, 0);

			unpack_list_open(packet, 0, true);
			{
				unpack_list_append(packet, (void*)&data->materials[data->material_indices[i].index].prim_tag, 1);
				unpack_list_append(packet, (void*)&data->materials[data->material_indices[i].index].diffuse, 1);
				unpack_list_append(packet, &positions[idxs_drawn], count);
				unpack_list_append(packet, &normals[idxs_drawn], count);
				unpack_list_append(packet, &colours[idxs_drawn], count);
				if (texcoords) 
					unpack_list_append(packet, &texcoords[idxs_drawn], count);
			}
			unpack_list_close(packet);

			owl_add_cnt_tag(packet, 1, owl_vif_code_double(VIF_CODE(0, 0, VIF_NOP, 0), VIF_CODE(0, 0, VIF_NOP, 0)));
			
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(packet, VIF_CODE(count, 0, VIF_ITOP, 0));
			owl_add_uint(packet, VIF_CODE(0, 0, (last_index == -1? VIF_MSCALF : VIF_MSCNT), 0)); 

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		last_index = data->material_indices[i].end;
	}

	owl_add_vif_codes(packet,
		VIF_CODE(0, 0, VIF_FLUSH, 0),
		VIF_CODE(0, 0, VIF_FLUSH, 0),
		VIF_CODE(0, 0, VIF_NOP, 0),
		VIF_CODE(0, 0, VIF_NOP, 0)
	);
}
