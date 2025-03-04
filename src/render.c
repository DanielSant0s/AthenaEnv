#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>

#include "fast_obj/fast_obj.h"

#include <render.h>
#include <dbgprintf.h>

#include <owl_packet.h>

#define DEG2RAD(deg) ((deg) * (M_PI / 180.0f))

register_vu_program(VU1Draw3DColors);
register_vu_program(VU1Draw3DCS);

register_vu_program(VU1Draw3DLightsColors);
register_vu_program(VU1Draw3DLCS);

register_vu_program(VU1Draw3DSpec);
register_vu_program(VU1Draw3DLCSS);

MATRIX view_screen;
MATRIX world_view;

FIVECTOR screen_scale;

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
	screen_scale.w = 0; // model attributes

}

static int active_aaa_lights = 0;
static int active_bbb_lights = 0;
static int active_pnt_lights = 0;
static int active_dir_lights = 0;

static LightData dir_lights;

int NewLight() {
	if (active_dir_lights < 4)
		return active_dir_lights++;

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
			dir_lights.direction[id][3] = 1.00f;
			break;
		case ATHENA_LIGHT_AMBIENT:
			dir_lights.ambient[id][0] = x;
			dir_lights.ambient[id][1] = y;
			dir_lights.ambient[id][2] = z;
			dir_lights.ambient[id][3] = 1.00f;
			break;
		case ATHENA_LIGHT_DIFFUSE:
			dir_lights.diffuse[id][0] = x;
			dir_lights.diffuse[id][1] = y;
			dir_lights.diffuse[id][2] = z;
			dir_lights.diffuse[id][3] = 1.00f;
			break;
		case ATHENA_LIGHT_SPECULAR:
			dir_lights.specular[id][0] = x;
			dir_lights.specular[id][1] = y;
			dir_lights.specular[id][2] = z;
			dir_lights.specular[id][3] = 1.00f;
			break;
	}
}

void draw_vu1_with_colors(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

void draw_vu1_with_lights(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

void draw_vu1_with_spec_lights(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

int athena_render_set_pipeline(model* m, int pl_id) {
	switch (pl_id) {
		case PL_NO_LIGHTS:
			m->render = draw_vu1_with_colors;
			m->pipeline = PL_NO_LIGHTS;
			break;
		case PL_DEFAULT:
			m->render = draw_vu1_with_lights;
			m->pipeline = PL_DEFAULT;
			break;
		case PL_SPECULAR:
			m->render = draw_vu1_with_spec_lights;
			m->pipeline = PL_SPECULAR;
			break;
	}
	return m->pipeline;
}

void calculate_bbox(model* res_m) {
	float lowX, lowY, lowZ, hiX, hiY, hiZ;

    lowX = hiX = res_m->positions[0][0];
    lowY = hiY = res_m->positions[0][1];
    lowZ = hiZ = res_m->positions[0][2];
	
    for (int i = 0; i < res_m->index_count; i++) {
        float* pos = res_m->positions[i];
        lowX = fmin(lowX, pos[0]);
        hiX =  fmax(hiX,  pos[0]);
        lowY = fmin(lowY, pos[1]);
        hiY =  fmax(hiY,  pos[1]);
        lowZ = fmin(lowZ, pos[2]);
        hiZ =  fmax(hiZ,  pos[2]);
    }

    VECTOR bbox[8] = {
        {lowX, lowY, lowZ, 1.0f}, {lowX, lowY, hiZ, 1.0f}, {lowX, hiY, lowZ, 1.0f}, {lowX, hiY, hiZ, 1.0f},
        {hiX,  lowY, lowZ, 1.0f}, {hiX,  lowY, hiZ, 1.0f}, {hiX,  hiY, lowZ, 1.0f}, {hiX,  hiY, hiZ, 1.0f}
    };

    memcpy(res_m->bounding_box, bbox, sizeof(bbox));
}

void obj_transfer_vertex(model* m, uint32_t dst_idx, fastObjMesh* obj, uint32_t src_idx) {
	copy_vector(&m->positions[dst_idx], obj->positions + (3 * obj->indices[src_idx].p));
	m->positions[dst_idx][3] = 1.0f;

	m->texcoords[dst_idx][0] = obj->texcoords[2 * obj->indices[src_idx].t];
    m->texcoords[dst_idx][1] = 1.0f - obj->texcoords[2 * obj->indices[src_idx].t + 1];
    m->texcoords[dst_idx][2] = 1.0f;
    m->texcoords[dst_idx][3] = 1.0f;

	copy_vector(&m->normals[dst_idx],  obj->normals + (3 * obj->indices[src_idx].n));
	m->normals[dst_idx][3] = 1.0f;
}

void loadOBJ(model* res_m, const char* path, GSTEXTURE* text) {
    fastObjMesh* m = fast_obj_read(path);

	res_m->texture_count = 0;
	res_m->textures = NULL;

	if (m->material_count && !m->strip_count) {
		res_m->materials = (ath_mat *)malloc(m->material_count*sizeof(ath_mat));
		res_m->material_count = m->material_count;

		// Allocate a initial size so we can grow this buffer using realloc if necessary
		res_m->material_indices = (material_index *)malloc(m->material_count*sizeof(material_index)); 
		res_m->material_index_count = m->material_count;
		
		for (int i = 0; i < res_m->material_count; i++) {
			copy_init_w_vector(res_m->materials[i].ambient,             m->materials[i].Ka);  
			copy_init_w_vector(res_m->materials[i].diffuse,             m->materials[i].Kd);
			copy_init_w_vector(res_m->materials[i].specular,            m->materials[i].Ks);
			copy_init_w_vector(res_m->materials[i].emission,            m->materials[i].Ke);
			copy_init_w_vector(res_m->materials[i].transmittance,       m->materials[i].Kt);
			copy_init_w_vector(res_m->materials[i].transmission_filter, m->materials[i].Tf);

			res_m->materials[i].shininess =  m->materials[i].Ns;
			res_m->materials[i].refraction =  m->materials[i].Ni;
			res_m->materials[i].disolve =   m->materials[i].d;

			res_m->materials[i].texture_id = -1;

			if (m->materials[i].map_Kd.name) {
				bool prev_loaded = false;

				if (!prev_loaded) {
					res_m->materials[i].texture_id = res_m->texture_count;
					GSTEXTURE* tex = malloc(sizeof(GSTEXTURE));
					load_image(tex, m->materials[i].map_Kd.name, true);

					append_texture(res_m, tex);
				}
			}


		}
	} else {
		res_m->materials = (ath_mat *)malloc(sizeof(ath_mat));
		res_m->material_count = 1;

		res_m->material_indices = (material_index *)malloc(sizeof(material_index)); 
		res_m->material_index_count = 1;

		init_vector(res_m->materials[0].ambient);
		init_vector(res_m->materials[0].diffuse);
		init_vector(res_m->materials[0].specular);
		init_vector(res_m->materials[0].emission);
		init_vector(res_m->materials[0].transmittance);
		init_vector(res_m->materials[0].transmission_filter);

		res_m->materials[0].shininess = 1.0f;
		res_m->materials[0].refraction = 1.0f;
		res_m->materials[0].disolve = 1.0f;

		res_m->materials[0].texture_id = -1;

		if((text)) {
			res_m->materials[0].texture_id = 0;
			append_texture(res_m, text);
		}
	}

    int cur_mat_index = (m->material_count? m->face_materials[0] : 0);
	char* old_tex = NULL;
	char* cur_tex = NULL;

	res_m->tristrip = m->strip_count > 0;

	if (res_m->tristrip) {
		res_m->index_count = m->index_count + (2 * m->strip_count) + (((m->index_count + (2 * m->strip_count)) / (BATCH_SIZE - 2)) * 2) + 1;

		res_m->positions = alloc_vectors(res_m->index_count);
    	res_m->texcoords = alloc_vectors(res_m->index_count);
    	res_m->normals =   alloc_vectors(res_m->index_count);
		res_m->colours =   alloc_vectors(res_m->index_count); 

		int unified_strip_count = 0;  
		int* unified_strip_indices = malloc(sizeof(int) * (m->index_count + 2 * m->strip_count));

		for (int i = 0; i < m->strip_count; i++) {
		    int strip_start = m->strips[i];
		    int strip_end = (i + 1 < m->strip_count) ? m->strips[i + 1] : m->index_count;

		    for (int j = strip_start; j < strip_end; j++) {
		        unified_strip_indices[unified_strip_count++] = j;

		        if (j == strip_end - 1 && i + 1 < m->strip_count) {
		            unified_strip_indices[unified_strip_count++] = j;
		            unified_strip_indices[unified_strip_count++] = m->strips[i + 1];
		        }
		    }
		}

		int batch_index = 0;
		int index = 0;

		for (int i = 0; i < unified_strip_count; i++) {
		    obj_transfer_vertex(res_m, index, m, unified_strip_indices[i]);

        	res_m->colours[index][0] = 1.0f;
        	res_m->colours[index][1] = 1.0f;
        	res_m->colours[index][2] = 1.0f;
        	res_m->colours[index][3] = 1.0f;

		    index++;
		    batch_index++;

		    if (batch_index == BATCH_SIZE) {
				copy_vector(&res_m->positions[index], &res_m->positions[index - 2]);
				copy_vector(&res_m->texcoords[index], &res_m->texcoords[index - 2]);
				copy_vector(&res_m->normals[index],   &res_m->normals[index - 2]);
				copy_vector(&res_m->colours[index],   &res_m->colours[index - 2]);
        	    index++;

				copy_vector(&res_m->positions[index], &res_m->positions[index - 2]);
				copy_vector(&res_m->texcoords[index], &res_m->texcoords[index - 2]);
				copy_vector(&res_m->normals[index],   &res_m->normals[index - 2]);
				copy_vector(&res_m->colours[index],   &res_m->colours[index - 2]);
        	    index++;

        	    batch_index = 2;
		    }
		}

		free(unified_strip_indices);

		res_m->index_count = index-1;
	} else {
		res_m->index_count = m->index_count;

    	res_m->positions = alloc_vectors(res_m->index_count);
    	res_m->texcoords = alloc_vectors(res_m->index_count);
    	res_m->normals =   alloc_vectors(res_m->index_count);
		res_m->colours =   alloc_vectors(res_m->index_count);


		uint32_t added_material_indices = 0;
		for (int i = 0; i < res_m->index_count; i++) {
			obj_transfer_vertex(res_m, i, m, i);

        	res_m->colours[i][0] = 0.0f; 
        	res_m->colours[i][1] = 0.0f;
        	res_m->colours[i][2] = 0.0f;
        	res_m->colours[i][3] = 0.0f;

			if (m->material_count > 0) {
				if (m->face_materials[i / 3] != cur_mat_index) {
					added_material_indices++;

					if (added_material_indices > res_m->material_index_count) {
						res_m->material_indices = (material_index*)realloc(res_m->material_indices, added_material_indices);
						res_m->material_index_count++;
					}

					res_m->material_indices[added_material_indices-1].index = cur_mat_index;
					res_m->material_indices[added_material_indices-1].end = i-1;
					cur_mat_index = m->face_materials[i / 3];
				}
			}
    	}

		added_material_indices++;

		if (added_material_indices > res_m->material_index_count) {
			res_m->material_indices = (material_index*)realloc(res_m->material_indices, added_material_indices);
			res_m->material_index_count++;
		}

		res_m->material_indices[added_material_indices-1].index = cur_mat_index;
		res_m->material_indices[added_material_indices-1].end = res_m->index_count;
	}

    calculate_bbox(res_m);

	athena_render_set_pipeline(res_m, PL_DEFAULT);

	fast_obj_destroy(m);
}

void draw_bbox(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z, Color color) {
	

	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	// Matrices to setup the 3D environment and camera
	MATRIX local_world;
	MATRIX local_screen;

	create_local_world(local_world, object_position, object_rotation);
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

	free(xyz);
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
	owl_add_cnt_tag(packet, 7, 0); // 4 quadwords for vif
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

	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	owl_add_uint(packet, VIF_CODE(2, 0, VIF_DIRECT, 0)); 
	
	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));
	
	owl_add_tag(packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, texture->Filter, texture->Filter, 0, 0, 0));
}

void draw_vu1_with_colors(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z) {
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;

	if (m->attributes.accurate_clipping)
		update_vu_program(VU1Draw3DCS);
	else
		update_vu_program(VU1Draw3DColors);

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_screen(m->local_screen, local_world, world_view, view_screen);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 4);

	unpack_list_open(packet, 0, false);
	{
		unpack_list_append(packet, &screen_scale,       1);

		unpack_list_append(packet, m->local_screen,       4);
	}
	unpack_list_close(packet);

	//owl_add_end_tag(packet);

	int last_index = -1;
	GSTEXTURE* tex = NULL;
	int texture_id;
	for(int i = 0; i < m->material_index_count; i++) {
		bool texture_mapping = ((m->materials[m->material_indices[i].index].texture_id != -1) && m->attributes.texture_mapping);

		if (texture_mapping) {
			GSTEXTURE *cur_tex = m->textures[m->materials[m->material_indices[i].index].texture_id];
			if (cur_tex != tex) {
				texture_id = texture_manager_bind(gsGlobal, cur_tex, true);
				tex = cur_tex;
			}
		}

		VECTOR* positions = &m->positions[last_index+1];
		VECTOR* colours = &m->colours[last_index+1];
		VECTOR* texcoords = texture_mapping? &m->texcoords[last_index+1] : NULL;

		int idxs_to_draw = (m->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, texture_mapping? 17 : 8);

			int count = BATCH_SIZE;
			if (idxs_to_draw < BATCH_SIZE)
			{
				count = idxs_to_draw;
			}

			m->materials[m->material_indices[i].index].clip_prim_tag.dword[1] = DRAW_STQ2_REGLIST;
			m->materials[m->material_indices[i].index].clip_prim_tag.dword[0] = VU_GS_GIFTAG(m->tristrip? 5 : 11, 
							                							1, NO_CUSTOM_DATA, 1, 
																		VU_GS_PRIM(GS_PRIM_PRIM_TRIFAN, 
																				   m->attributes.shade_model, texture_mapping, 
																				   gsGlobal->PrimFogEnable, 
																				   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    														0, 3);

			owl_add_unpack_data(packet, 26, (void*)&m->materials[m->material_indices[i].index].clip_prim_tag, 1, 0);

			m->materials[m->material_indices[i].index].prim_tag.dword[1] = DRAW_STQ2_REGLIST;
			m->materials[m->material_indices[i].index].prim_tag.dword[0] = VU_GS_GIFTAG(0, 
							                							1, NO_CUSTOM_DATA, 1, 
																		VU_GS_PRIM(m->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 
																				   m->attributes.shade_model, texture_mapping, 
																				   gsGlobal->PrimFogEnable, 
																				   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    														0, 3);

			if (texture_mapping) {
				append_texture_tags(packet, tex, texture_id, COLOR_MODULATE);
			}

			unpack_list_open(packet, 0, true);
			{
				unpack_list_append(packet, (void*)&m->materials[m->material_indices[i].index].prim_tag, 1);
				unpack_list_append(packet, (void*)&m->materials[m->material_indices[i].index].diffuse, 1);
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

		last_index = m->material_indices[i].end;
	}

	owl_add_vif_codes(packet,
		VIF_CODE(0, 0, VIF_FLUSH, 0),
		VIF_CODE(0, 0, VIF_FLUSH, 0),
		VIF_CODE(0, 0, VIF_NOP, 0),
		VIF_CODE(0, 0, VIF_NOP, 0)
	);
}

void draw_vu1_with_lights(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z) {
	if (m->attributes.accurate_clipping)
		update_vu_program(VU1Draw3DLCS);
	else
		update_vu_program(VU1Draw3DLightsColors);
		
	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;

  	// Create the local_world matrix.
  	matrix_unit(local_world);
  	matrix_rotate(local_world, local_world, object_rotation);
  	matrix_translate(local_world, local_world, object_position);

  	// Create the local_light matrix.
  	matrix_unit(m->local_light);
  	matrix_rotate(m->local_light, m->local_light, object_rotation);

  	// Create the local_screen matrix.
  	matrix_unit(m->local_screen);

  	matrix_multiply(m->local_screen, m->local_screen, local_world);
  	matrix_multiply(m->local_screen, m->local_screen, world_view);
  	matrix_multiply(m->local_screen, m->local_screen, view_screen);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 7); // 5 for unpack static data + 2 for flush with end

	unpack_list_open(packet, 0, false);
	{
		unpack_list_append(packet, &screen_scale,       1); 

		unpack_list_append(packet, m->local_screen,       4);
		unpack_list_append(packet, m->local_light,        4);

		static FIVECTOR camera_pos_light_qt; // xyz for camera position and w for directional light quantity

		memcpy(&camera_pos_light_qt, getCameraPosition(), sizeof(FIVECTOR));
		camera_pos_light_qt.w = active_dir_lights;

		unpack_list_append(packet, &camera_pos_light_qt, 1);
		unpack_list_append(packet, &dir_lights,        16);
	}
	unpack_list_close(packet);

	//owl_add_end_tag(packet);

	int last_index = -1;
	GSTEXTURE* tex = NULL;
	int texture_id;
	for(int i = 0; i < m->material_index_count; i++) {
		bool texture_mapping = ((m->materials[m->material_indices[i].index].texture_id != -1) && m->attributes.texture_mapping);

		if (texture_mapping) {
			GSTEXTURE *cur_tex = m->textures[m->materials[m->material_indices[i].index].texture_id];
			if (cur_tex != tex) {
				texture_id = texture_manager_bind(gsGlobal, cur_tex, true);
				tex = cur_tex;
			}
		}

		VECTOR* positions = &m->positions[last_index+1];
		VECTOR* texcoords = texture_mapping? &m->texcoords[last_index+1] : NULL;
		VECTOR* normals = &m->normals[last_index+1];
		VECTOR* colours = &m->colours[last_index+1];

		int idxs_to_draw = (m->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, texture_mapping? 17 : 8);

			int count = BATCH_SIZE;
			if (idxs_to_draw < BATCH_SIZE)
			{
				count = idxs_to_draw;
			}

			if (texture_mapping) {
				append_texture_tags(packet, tex, texture_id, COLOR_MODULATE);
			}

			m->materials[m->material_indices[i].index].clip_prim_tag.dword[1] = DRAW_STQ2_REGLIST;
			m->materials[m->material_indices[i].index].clip_prim_tag.dword[0] = VU_GS_GIFTAG(m->tristrip? 5 : 11, 
							                							1, NO_CUSTOM_DATA, 1, 
																		VU_GS_PRIM(GS_PRIM_PRIM_TRIFAN, 
																				   m->attributes.shade_model, texture_mapping, 
																				   gsGlobal->PrimFogEnable, 
																				   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    														0, 3);

			owl_add_unpack_data(packet, 26, (void*)&m->materials[m->material_indices[i].index].clip_prim_tag, 1, 0);

			m->materials[m->material_indices[i].index].prim_tag.dword[1] = DRAW_STQ2_REGLIST;
			m->materials[m->material_indices[i].index].prim_tag.dword[0] = VU_GS_GIFTAG(0, 
							                							1, NO_CUSTOM_DATA, 1, 
																		VU_GS_PRIM(m->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 
																				   m->attributes.shade_model, texture_mapping, 
																				   gsGlobal->PrimFogEnable, 
																				   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    														0, 3);

			unpack_list_open(packet, 0, true);
			{
				unpack_list_append(packet, (void*)&m->materials[m->material_indices[i].index].prim_tag, 1);
				unpack_list_append(packet, (void*)&m->materials[m->material_indices[i].index].diffuse, 1);
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

		last_index = m->material_indices[i].end;
	}

	owl_add_vif_codes(packet,
		VIF_CODE(0, 0, VIF_FLUSH, 0),
		VIF_CODE(0, 0, VIF_FLUSH, 0),
		VIF_CODE(0, 0, VIF_NOP, 0),
		VIF_CODE(0, 0, VIF_NOP, 0)
	);

	
}

void draw_vu1_with_spec_lights(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z) {
	if (m->attributes.accurate_clipping)
		update_vu_program(VU1Draw3DLCSS);
	else
		update_vu_program(VU1Draw3DSpec);

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;

  	// Create the local_world matrix.
  	matrix_unit(local_world);
  	matrix_rotate(local_world, local_world, object_rotation);
  	matrix_translate(local_world, local_world, object_position);

  	// Create the local_light matrix.
  	matrix_unit(m->local_light);
  	matrix_rotate(m->local_light, m->local_light, object_rotation);

  	// Create the local_screen matrix.
  	matrix_unit(m->local_screen);

  	matrix_multiply(m->local_screen, m->local_screen, local_world);
  	matrix_multiply(m->local_screen, m->local_screen, world_view);
  	matrix_multiply(m->local_screen, m->local_screen, view_screen);

	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 7);

	screen_scale.w = *((uint32_t*)&m->attributes);

	unpack_list_open(packet, 0, false);
	{
		unpack_list_append(packet, &screen_scale,       1);

		unpack_list_append(packet, m->local_screen,       4);
		unpack_list_append(packet, m->local_light,        4);

		static FIVECTOR camera_pos_light_qt; // xyz for camera position and w for directional light quantity

		memcpy(&camera_pos_light_qt, getCameraPosition(), sizeof(FIVECTOR));
		camera_pos_light_qt.w = active_dir_lights;

		unpack_list_append(packet, &camera_pos_light_qt, 1);
		unpack_list_append(packet, &dir_lights,         16);
	}
	unpack_list_close(packet);

	//owl_add_end_tag(packet);

	int last_index = -1;
	GSTEXTURE* tex = NULL;
	int texture_id;
	for(int i = 0; i < m->material_index_count; i++) {
		bool texture_mapping = ((m->materials[m->material_indices[i].index].texture_id != -1) && m->attributes.texture_mapping);

		if (texture_mapping) {
			GSTEXTURE *cur_tex = m->textures[m->materials[m->material_indices[i].index].texture_id];
			if (cur_tex != tex) {
				texture_id = texture_manager_bind(gsGlobal, cur_tex, true);
				tex = cur_tex;
			}
		}

		VECTOR* positions = &m->positions[last_index+1];
		VECTOR* texcoords = texture_mapping? &m->texcoords[last_index+1] : NULL;
		VECTOR* normals = &m->normals[last_index+1];
		VECTOR* colours = &m->colours[last_index+1];

		int idxs_to_draw = (m->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			owl_query_packet(CHANNEL_VIF1, texture_mapping? 17 : 8);

			int count = BATCH_SIZE;
			if (idxs_to_draw < BATCH_SIZE)
			{
				count = idxs_to_draw;
			}
			if (texture_mapping) {
				append_texture_tags(packet, tex, texture_id, COLOR_MODULATE);
			}

			m->materials[m->material_indices[i].index].clip_prim_tag.dword[1] = DRAW_STQ2_REGLIST;
			m->materials[m->material_indices[i].index].clip_prim_tag.dword[0] = VU_GS_GIFTAG(m->tristrip? 5 : 11, 
							                							1, NO_CUSTOM_DATA, 1, 
																		VU_GS_PRIM(GS_PRIM_PRIM_TRIFAN, 
																				   m->attributes.shade_model, texture_mapping, 
																				   gsGlobal->PrimFogEnable, 
																				   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    														0, 3);

			owl_add_unpack_data(packet, 26, (void*)&m->materials[m->material_indices[i].index].clip_prim_tag, 1, 0);

			m->materials[m->material_indices[i].index].prim_tag.dword[1] = DRAW_STQ2_REGLIST;
			m->materials[m->material_indices[i].index].prim_tag.dword[0] = VU_GS_GIFTAG(0, 
							                							1, NO_CUSTOM_DATA, 1, 
																		VU_GS_PRIM(m->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 
																				   m->attributes.shade_model, texture_mapping, 
																				   gsGlobal->PrimFogEnable, 
																				   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    														0, 3);

			unpack_list_open(packet, 0, true);
			{
				unpack_list_append(packet, (void*)&m->materials[m->material_indices[i].index].prim_tag, 1);
				unpack_list_append(packet, (void*)&m->materials[m->material_indices[i].index].diffuse, 1);
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

		last_index = m->material_indices[i].end;
	}

	owl_add_vif_codes(packet,
		VIF_CODE(0, 0, VIF_FLUSH, 0),
		VIF_CODE(0, 0, VIF_FLUSH, 0),
		VIF_CODE(0, 0, VIF_NOP, 0),
		VIF_CODE(0, 0, VIF_NOP, 0)
	);
}
