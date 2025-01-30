#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>
#include "fast_obj/fast_obj.h"

#include "include/render.h"

#include "include/dbgprintf.h"

#include "vif.h"

register_vu_program(VU1Draw3D);
register_vu_program(VU1Draw3DNoTex);
register_vu_program(VU1Draw3DPVC);
register_vu_program(VU1Draw3DPVCNoTex);
register_vu_program(VU1Draw3DColors);
register_vu_program(VU1Draw3DColorsNoTex);
register_vu_program(VU1Draw3DLightsColors);
register_vu_program(VU1Draw3DLightsColorsNoTex);
register_vu_program(VU1Draw3DSpec);
register_vu_program(VU1Draw3DSpecNoTex);

MATRIX view_screen;
MATRIX world_view;

void init3D(float aspect, float fov, float near, float far)
{
	initCamera(&world_view);
	create_view_screen(view_screen, aspect, -fov, fov, -fov, fov, near, far);
	vu1_set_double_buffer_settings(26, 496);

}

static int active_dir_lights = 0;
static int active_pnt_lights = 0;
static int active_aaa_lights = 0;
static int active_bbb_lights = 0;
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

void draw_vu1_notex(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);
void draw_vu1(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

void draw_vu1_pvc_notex(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);
void draw_vu1_pvc(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

void draw_vu1_with_colors_notex(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);
void draw_vu1_with_colors(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

void draw_vu1_with_lights_notex(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);
void draw_vu1_with_lights(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

void draw_vu1_with_spec_lights_notex(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);
void draw_vu1_with_spec_lights(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

int athena_render_set_pipeline(model* m, int pl_id) {
	switch (pl_id) {
		case PL_NO_LIGHTS_COLORS:
			if (m->texture_count) {
				m->render = draw_vu1;
				m->pipeline = PL_NO_LIGHTS_COLORS;
			} else {
				m->render = draw_vu1_notex;
				m->pipeline = PL_NO_LIGHTS_COLORS_TEX;
			}
			break;
		case PL_NO_LIGHTS_COLORS_TEX:
			m->render = draw_vu1_notex;
			m->pipeline = PL_NO_LIGHTS_COLORS_TEX;
			break;
		case PL_PVC:
			if (!m->colours)
				break;
			if (m->texture_count) {
				m->render = draw_vu1_pvc;
				m->pipeline = PL_PVC;
			} else {
				m->render = draw_vu1_pvc_notex;
				m->pipeline = PL_PVC_NO_TEX;
			}
			break;
		case PL_PVC_NO_TEX:
			if (!m->colours)
				break;
			m->render = draw_vu1_pvc_notex;
			m->pipeline = PL_PVC_NO_TEX;
			break;
		case PL_NO_LIGHTS:
			if (m->texture_count) {
				m->render = draw_vu1_with_colors;
				m->pipeline = PL_NO_LIGHTS;
			} else {
				m->render = draw_vu1_with_colors_notex;
				m->pipeline = PL_NO_LIGHTS_TEX;
			}
			break;
		case PL_NO_LIGHTS_TEX:
			m->render = draw_vu1_with_colors_notex;
			m->pipeline = PL_NO_LIGHTS_TEX;
			break;
		case PL_DEFAULT:
			if (m->texture_count) {
				m->render = draw_vu1_with_lights;
				m->pipeline = PL_DEFAULT;
			} else {
				m->render = draw_vu1_with_lights_notex;
				m->pipeline = PL_DEFAULT_NO_TEX;
			}
			break;
		case PL_DEFAULT_NO_TEX:
			m->render = draw_vu1_with_lights_notex;
			m->pipeline = PL_DEFAULT_NO_TEX;
			break;
		case PL_SPECULAR:
			if (m->texture_count) {
				m->render = draw_vu1_with_spec_lights;
				m->pipeline = PL_SPECULAR;
			} else {
				m->render = draw_vu1_with_spec_lights_notex;
				m->pipeline = PL_DEFAULT_NO_TEX;
			}
			break;
		case PL_SPECULAR_NO_TEX:
			m->render = draw_vu1_with_spec_lights_notex;
			m->pipeline = PL_SPECULAR_NO_TEX;
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
	GSGLOBAL *gsGlobal = getGSGLOBAL();

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


u64 vif_packets[2][44] __attribute__((aligned(64)));
u64* curr_vif_packet;

/** Cube data */
u64 cube_packet[20] __attribute__((aligned(64)));

dma_packet draw_packet;
dma_packet attr_packet;

u8 context = 0;

static u32* last_mpg = NULL;

#define update_vu_program(name) \
	do { \
		if (last_mpg != &name##_CodeStart) { \
			dmaKit_wait(DMA_CHANNEL_VIF1, 0); \
			vu1_upload_micro_program(&name##_CodeStart, &name##_CodeEnd); \
			last_mpg = &name##_CodeStart; \
		} \
	} while (0)

void draw_vu1(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z) {
	GSGLOBAL *gsGlobal = getGSGLOBAL();

	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_screen;

	update_vu_program(VU1Draw3D);

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	curr_vif_packet = vu_add_unpack_data(vif_packets[context], 0, &local_screen, 4, 0);

	dma_add_end_tag(curr_vif_packet);

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

	dma_packet_create(&attr_packet, cube_packet, 0);

	int last_index = -1;
	GSTEXTURE* tex = NULL;
	for(int i = 0; i < m->material_index_count; i++) {
		GSTEXTURE *cur_tex = m->textures[m->materials[m->material_indices[i].index].texture_id];
		if (cur_tex != tex) {
			gsKit_TexManager_bind(gsGlobal, cur_tex);
			tex = cur_tex;
		}
	
		VECTOR* positions = &m->positions[last_index+1];
		VECTOR* texcoords = &m->texcoords[last_index+1];

		int idxs_to_draw = (m->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			dmaKit_wait(DMA_CHANNEL_VIF1, 0);

			int count = BATCH_SIZE;
			if (idxs_to_draw < BATCH_SIZE)
			{
				count = idxs_to_draw;
			}

			float fX = 2048.0f+gsGlobal->Width/2;
			float fY = 2048.0f+gsGlobal->Height/2;
			float fZ = ((float)get_max_z(gsGlobal));

			dma_packet_reset(&attr_packet);

			dma_packet_add_float(&attr_packet, fX);
			dma_packet_add_float(&attr_packet, fY);
			dma_packet_add_float(&attr_packet, fZ);

			dma_packet_add_uint(&attr_packet, count);
		
			dma_packet_add_tag(&attr_packet, GIF_AD, GIFTAG(1, 0, 0, 0, 0, 1));
		
			dma_packet_add_tag(&attr_packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0));
		
			int tw, th;
			athena_set_tw_th(tex, &tw, &th);

			dma_packet_add_tag(&attr_packet, 
							   GS_TEX0_1, 
							   GS_SETREG_TEX0(tex->Vram/256, 
											  tex->TBW, 
											  tex->PSM,
											  tw, th, 
											  gsGlobal->PrimAlphaEnable, 
											  COLOR_MODULATE,
											  tex->VramClut/256, 
											  tex->ClutPSM, 
											  0, 0, 
											  tex->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
								);

			dma_packet_add_tag(&attr_packet, 
			                   DRAW_STQ2_REGLIST, 
							   VU_GS_GIFTAG(count, 
							                1, 1, 
											VU_GS_PRIM(m->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 
													   1, 1, 
													   gsGlobal->PrimFogEnable, 
													   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    							0, 3)
								);

			dma_packet_add_uint(&attr_packet, 128);
			dma_packet_add_uint(&attr_packet, 128);
			dma_packet_add_uint(&attr_packet, 128);
			dma_packet_add_uint(&attr_packet, 128);

			dma_packet_create(&draw_packet, vif_packets[context], 0);

			unpack_list_open(&draw_packet, 0, true);
			{
				unpack_list_append(&draw_packet, attr_packet.base, 6);
				unpack_list_append(&draw_packet, &positions[idxs_drawn], count);
				unpack_list_append(&draw_packet, &texcoords[idxs_drawn], count);
			}
			unpack_list_close(&draw_packet);

			dma_packet_start_program(&draw_packet, last_index == -1);
			dma_packet_add_end_tag(&draw_packet);

			dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);

			dma_packet_destroy(&draw_packet);

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		last_index = m->material_indices[i].end;
	}

	context = !context;
}

void draw_vu1_notex(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z) {
	GSGLOBAL *gsGlobal = getGSGLOBAL();

	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_screen;

	update_vu_program(VU1Draw3DNoTex);

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	curr_vif_packet = vu_add_unpack_data(vif_packets[context], 0, &local_screen, 4, 0);

	dma_add_end_tag(curr_vif_packet);

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

	int idxs_to_draw = m->index_count;
	int idxs_drawn = 0;

	dma_packet_create(&attr_packet, cube_packet, 0);

	while (idxs_to_draw > 0) {
		dmaKit_wait(DMA_CHANNEL_VIF1, 0);

		int count = BATCH_SIZE;
		if (idxs_to_draw < BATCH_SIZE)
		{
			count = idxs_to_draw;
		}

		float fX = 2048.0f+gsGlobal->Width/2;
		float fY = 2048.0f+gsGlobal->Height/2;
		float fZ = ((float)get_max_z(gsGlobal));

		float texCol = 128.0f;

		dma_packet_reset(&attr_packet);

		dma_packet_add_float(&attr_packet, fX);
		dma_packet_add_float(&attr_packet, fY);
		dma_packet_add_float(&attr_packet, fZ);

		dma_packet_add_uint(&attr_packet, count);

		dma_packet_add_tag(&attr_packet, 
		                   DRAW_NOTEX_REGLIST, 
						   VU_GS_GIFTAG(count, 
						                1, 1, 
										VU_GS_PRIM(m->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 
												   1, 0, 
												   gsGlobal->PrimFogEnable, 
												   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    	    							0, 2)
							);

		dma_packet_add_uint(&attr_packet, 128);
		dma_packet_add_uint(&attr_packet, 128);
		dma_packet_add_uint(&attr_packet, 128);
		dma_packet_add_uint(&attr_packet, 128);

		dma_packet_create(&draw_packet, vif_packets[context], 0);

		unpack_list_open(&draw_packet, 0, true);
		{
			unpack_list_append(&draw_packet, attr_packet.base, 3);
			unpack_list_append(&draw_packet, &m->positions[idxs_drawn], count);
		}
		unpack_list_close(&draw_packet);

		dma_packet_start_program(&draw_packet, (!idxs_drawn));
		dma_packet_add_end_tag(&draw_packet);

		dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);
		dma_packet_destroy(&draw_packet);

		idxs_to_draw -= count;
		idxs_drawn += count;
	}

	context = !context;
}

void draw_vu1_pvc(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z) {
	GSGLOBAL *gsGlobal = getGSGLOBAL();

	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_light;
	MATRIX local_screen;

	update_vu_program(VU1Draw3DPVC);

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	curr_vif_packet = vu_add_unpack_data(vif_packets[context], 0, &local_screen, 4, 0);

	dma_add_end_tag(curr_vif_packet);

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

	dma_packet_create(&attr_packet, cube_packet, 0);

	int last_index = -1;
	GSTEXTURE* tex = NULL;
	for(int i = 0; i < m->material_index_count; i++) {
		GSTEXTURE *cur_tex = m->textures[m->materials[m->material_indices[i].index].texture_id];
		if (cur_tex != tex) {
			gsKit_TexManager_bind(gsGlobal, cur_tex);
			tex = cur_tex;
		}

		VECTOR* positions = &m->positions[last_index+1];
		VECTOR* texcoords = &m->texcoords[last_index+1];
		VECTOR* colours = &m->colours[last_index+1];

		int idxs_to_draw = (m->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			dmaKit_wait(DMA_CHANNEL_VIF1, 0);

			int count = BATCH_SIZE;
			if (idxs_to_draw < BATCH_SIZE)
			{
				count = idxs_to_draw;
			}

			float fX = 2048.0f+gsGlobal->Width/2;
			float fY = 2048.0f+gsGlobal->Height/2;
			float fZ = ((float)get_max_z(gsGlobal));

			dma_packet_reset(&attr_packet);

			dma_packet_add_float(&attr_packet, fX);
			dma_packet_add_float(&attr_packet, fY);
			dma_packet_add_float(&attr_packet, fZ);

			dma_packet_add_uint(&attr_packet, count);
		
			dma_packet_add_tag(&attr_packet, GIF_AD, GIFTAG(1, 0, 0, 0, 0, 1));
		
			dma_packet_add_tag(&attr_packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0));
		
			int tw, th;
			athena_set_tw_th(tex, &tw, &th);

			dma_packet_add_tag(&attr_packet, 
							   GS_TEX0_1, 
							   GS_SETREG_TEX0(tex->Vram/256, 
											  tex->TBW, 
											  tex->PSM,
											  tw, th, 
											  gsGlobal->PrimAlphaEnable, 
											  COLOR_MODULATE,
											  tex->VramClut/256, 
											  tex->ClutPSM, 
											  0, 0, 
											  tex->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
								);

			dma_packet_add_tag(&attr_packet, 
			                   DRAW_STQ2_REGLIST, 
							   VU_GS_GIFTAG(count, 
							                1, 1, 
											VU_GS_PRIM(m->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 
													   1, 1, 
													   gsGlobal->PrimFogEnable, 
													   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    							0, 3)
								);

			dma_packet_add_float(&attr_packet, 128.0f);
			dma_packet_add_float(&attr_packet, 128.0f);
			dma_packet_add_float(&attr_packet, 128.0f);
			dma_packet_add_float(&attr_packet, 128.0f);

			dma_packet_create(&draw_packet, vif_packets[context], 0);

			unpack_list_open(&draw_packet, 0, true);
			{
				unpack_list_append(&draw_packet, attr_packet.base, 6);
				unpack_list_append(&draw_packet, &positions[idxs_drawn], count);
				unpack_list_append(&draw_packet, &texcoords[idxs_drawn], count);
				unpack_list_append(&draw_packet,   &colours[idxs_drawn], count);
			}
			unpack_list_close(&draw_packet);

			dma_packet_start_program(&draw_packet, last_index == -1);
			dma_packet_add_end_tag(&draw_packet);

			dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);

			dma_packet_destroy(&draw_packet);

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		last_index = m->material_indices[i].end;
	}

	context = !context;
}

void draw_vu1_pvc_notex(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z) {
	GSGLOBAL *gsGlobal = getGSGLOBAL();

	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_light;
	MATRIX local_screen;

	update_vu_program(VU1Draw3DPVCNoTex);

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	int idxs_to_draw = m->index_count;
	int idxs_drawn = 0;

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	curr_vif_packet = vu_add_unpack_data(vif_packets[context], 0, &local_screen, 4, 0);

	dma_add_end_tag(curr_vif_packet);

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

	dma_packet_create(&attr_packet, cube_packet, 0);

	while (idxs_to_draw > 0) {
		dmaKit_wait(DMA_CHANNEL_VIF1, 0);

		int count = BATCH_SIZE;
		if (idxs_to_draw < BATCH_SIZE)
		{
			count = idxs_to_draw;
		}

		float fX = 2048.0f+gsGlobal->Width/2;
		float fY = 2048.0f+gsGlobal->Height/2;
		float fZ = ((float)get_max_z(gsGlobal));

		dma_packet_reset(&attr_packet);

		dma_packet_add_float(&attr_packet, fX);
		dma_packet_add_float(&attr_packet, fY);
		dma_packet_add_float(&attr_packet, fZ);

		dma_packet_add_uint(&attr_packet, count);

		dma_packet_add_tag(&attr_packet, 
		                   DRAW_NOTEX_REGLIST, 
						   VU_GS_GIFTAG(count, 
						                1, 1, 
										VU_GS_PRIM(m->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 
												   1, 0, 
												   gsGlobal->PrimFogEnable, 
												   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    	    							0, 2)
							);

		dma_packet_add_float(&attr_packet, 128.0f);
		dma_packet_add_float(&attr_packet, 128.0f);
		dma_packet_add_float(&attr_packet, 128.0f);
		dma_packet_add_float(&attr_packet, 128.0f);

		dma_packet_create(&draw_packet, vif_packets[context], 0);

		unpack_list_open(&draw_packet, 0, true);
		{
			unpack_list_append(&draw_packet, attr_packet.base, 3);
			unpack_list_append(&draw_packet, &m->positions[idxs_drawn], count);
			unpack_list_append(&draw_packet,   &m->colours[idxs_drawn], count);
		}
		unpack_list_close(&draw_packet);

		dma_packet_start_program(&draw_packet, (!idxs_drawn));
		dma_packet_add_end_tag(&draw_packet);

		dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);

		dma_packet_destroy(&draw_packet);

		idxs_to_draw -= count;
		idxs_drawn += count;
	}

	context = !context;
}

void draw_vu1_with_colors(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z) {
	GSGLOBAL *gsGlobal = getGSGLOBAL();
	
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_light;
	MATRIX local_screen;

	update_vu_program(VU1Draw3DColors);

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	curr_vif_packet = vu_add_unpack_data(vif_packets[context], 0, &local_screen, 4, 0);

	dma_add_end_tag(curr_vif_packet);

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

	dma_packet_create(&attr_packet, cube_packet, 0);

	int last_index = -1;
	GSTEXTURE* tex = NULL;
	for(int i = 0; i < m->material_index_count; i++) {
		GSTEXTURE *cur_tex = m->textures[m->materials[m->material_indices[i].index].texture_id];
		if (cur_tex != tex) {
			gsKit_TexManager_bind(gsGlobal, cur_tex);
			tex = cur_tex;
		}

		VECTOR* positions = &m->positions[last_index+1];
		VECTOR* texcoords = &m->texcoords[last_index+1];

		int idxs_to_draw = (m->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			dmaKit_wait(DMA_CHANNEL_VIF1, 0);

			int count = BATCH_SIZE;
			if (idxs_to_draw < BATCH_SIZE)
			{
				count = idxs_to_draw;
			}

			float fX = 2048.0f+gsGlobal->Width/2;
			float fY = 2048.0f+gsGlobal->Height/2;
			float fZ = ((float)get_max_z(gsGlobal));

			dma_packet_reset(&attr_packet);

			dma_packet_add_float(&attr_packet, fX);
			dma_packet_add_float(&attr_packet, fY);
			dma_packet_add_float(&attr_packet, fZ);

			dma_packet_add_uint(&attr_packet, count);
		
			dma_packet_add_tag(&attr_packet, GIF_AD, GIFTAG(1, 0, 0, 0, 0, 1));
		
			dma_packet_add_tag(&attr_packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0));
		
			int tw, th;
			athena_set_tw_th(tex, &tw, &th);

			dma_packet_add_tag(&attr_packet, 
							   GS_TEX0_1, 
							   GS_SETREG_TEX0(tex->Vram/256, 
											  tex->TBW, 
											  tex->PSM,
											  tw, th, 
											  gsGlobal->PrimAlphaEnable, 
											  COLOR_MODULATE,
											  tex->VramClut/256, 
											  tex->ClutPSM, 
											  0, 0, 
											  tex->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
								);

			dma_packet_add_tag(&attr_packet, 
			                   DRAW_STQ2_REGLIST, 
							   VU_GS_GIFTAG(count, 
							                1, 1, 
											VU_GS_PRIM(m->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 
													   1, 1, 
													   gsGlobal->PrimFogEnable, 
													   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    							0, 3)
								);

			union {
				VECTOR v;
				__uint128_t q;
			} diffuse;

			__asm volatile ( 	
				"lq    $7,0x0(%1)\n"
				"sq    $7,0x0(%0)\n"
				 : : "r" (&diffuse), "r" (m->materials[m->material_indices[i].index].diffuse):"$7","memory");

			dma_packet_add_uquad(&attr_packet, diffuse.q);

			dma_packet_create(&draw_packet, vif_packets[context], 0);

			unpack_list_open(&draw_packet, 0, true);
			{
				unpack_list_append(&draw_packet, attr_packet.base, 6);
				unpack_list_append(&draw_packet, &positions[idxs_drawn], count);
				unpack_list_append(&draw_packet, &texcoords[idxs_drawn], count);
			}
			unpack_list_close(&draw_packet);

			dma_packet_start_program(&draw_packet, last_index == -1);
			dma_packet_add_end_tag(&draw_packet);

			dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);

			dma_packet_destroy(&draw_packet);

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		last_index = m->material_indices[i].end;
	}

	context = !context;
}

void draw_vu1_with_colors_notex(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z) {
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_light;
	MATRIX local_screen;

	GSGLOBAL *gsGlobal = getGSGLOBAL();

	update_vu_program(VU1Draw3DColorsNoTex);

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	curr_vif_packet = vu_add_unpack_data(vif_packets[context], 0, &local_screen, 4, 0);

	dma_add_end_tag(curr_vif_packet);

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

	dma_packet_create(&attr_packet, cube_packet, 0);

	int last_index = -1;
	for(int i = 0; i < m->material_index_count; i++) {
		VECTOR* positions = &m->positions[last_index+1];

		int idxs_to_draw = (m->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			dmaKit_wait(DMA_CHANNEL_VIF1, 0);

			int count = BATCH_SIZE;
			if (idxs_to_draw < BATCH_SIZE)
			{
				count = idxs_to_draw;
			}

			float fX = 2048.0f+gsGlobal->Width/2;
			float fY = 2048.0f+gsGlobal->Height/2;
			float fZ = ((float)get_max_z(gsGlobal));

			dma_packet_reset(&attr_packet);

			dma_packet_add_float(&attr_packet, fX);
			dma_packet_add_float(&attr_packet, fY);
			dma_packet_add_float(&attr_packet, fZ);
 
			dma_packet_add_uint(&attr_packet, count);

			dma_packet_add_tag(&attr_packet, 
			                   DRAW_NOTEX_REGLIST, 
							   VU_GS_GIFTAG(count,  
							                1, 1, 
											VU_GS_PRIM(m->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 
													   1, 0, 
													   gsGlobal->PrimFogEnable, 
													   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    							0, 2)
								);

			union {
				VECTOR v; 
				__uint128_t q;
			} diffuse;

			__asm volatile ( 	
				"lq    $7,0x0(%1)\n" 
				"sq    $7,0x0(%0)\n"
				 : : "r" (&diffuse), "r" (m->materials[m->material_indices[i].index].diffuse):"$7","memory");

			dma_packet_add_uquad(&attr_packet, diffuse.q);

			dma_packet_create(&draw_packet, vif_packets[context], 0);

			unpack_list_open(&draw_packet, 0, true);
			{
				unpack_list_append(&draw_packet, attr_packet.base, 3);
				unpack_list_append(&draw_packet, &positions[idxs_drawn], count);
			}
			unpack_list_close(&draw_packet);

			dma_packet_start_program(&draw_packet, last_index == -1);
			dma_packet_add_end_tag(&draw_packet);

			dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);

			dma_packet_destroy(&draw_packet);

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		last_index = m->material_indices[i].end;
	}

	context = !context;
}

void draw_vu1_with_lights(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z) {
	GSGLOBAL *gsGlobal = getGSGLOBAL();

	update_vu_program(VU1Draw3DLightsColors);

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_light;
	MATRIX local_screen;

  	// Create the local_world matrix.
  	matrix_unit(local_world);
  	matrix_rotate(local_world, local_world, object_rotation);
  	matrix_translate(local_world, local_world, object_position);

  	// Create the local_light matrix.
  	matrix_unit(local_light);
  	matrix_rotate(local_light, local_light, object_rotation);

  	// Create the local_screen matrix.
  	matrix_unit(local_screen);

  	matrix_multiply(local_screen, local_screen, local_world);
  	matrix_multiply(local_screen, local_screen, world_view);
  	matrix_multiply(local_screen, local_screen, view_screen);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dma_packet_create(&draw_packet, vif_packets[context], 0);

	unpack_list_open(&draw_packet, 0, false);
	{
		unpack_list_append(&draw_packet, &local_screen,       4);
		unpack_list_append(&draw_packet, &local_light,        4);
		unpack_list_append(&draw_packet, &active_dir_lights,  1);
		unpack_list_append(&draw_packet, &dir_lights,         12);
	}
	unpack_list_close(&draw_packet);

	dma_packet_add_end_tag(&draw_packet);

	dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);

	dma_packet_destroy(&draw_packet);

	dma_packet_create(&attr_packet, cube_packet, 0);

	int last_index = -1;
	GSTEXTURE* tex = NULL;
	for(int i = 0; i < m->material_index_count; i++) {
		GSTEXTURE *cur_tex = m->textures[m->materials[m->material_indices[i].index].texture_id];
		if (cur_tex != tex) {
			gsKit_TexManager_bind(gsGlobal, cur_tex);
			tex = cur_tex;
		}

		VECTOR* positions = &m->positions[last_index+1];
		VECTOR* texcoords = &m->texcoords[last_index+1];
		VECTOR* normals = &m->normals[last_index+1];

		int idxs_to_draw = (m->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			dmaKit_wait(DMA_CHANNEL_VIF1, 0);

			int count = BATCH_SIZE;
			if (idxs_to_draw < BATCH_SIZE)
			{
				count = idxs_to_draw;
			}

			float fX = 2048.0f+gsGlobal->Width/2;
			float fY = 2048.0f+gsGlobal->Height/2;
			float fZ = ((float)get_max_z(gsGlobal));
			dma_packet_reset(&attr_packet);

			dma_packet_add_float(&attr_packet, fX);
			dma_packet_add_float(&attr_packet, fY);
			dma_packet_add_float(&attr_packet, fZ);

			dma_packet_add_uint(&attr_packet, count);
		
			dma_packet_add_tag(&attr_packet, GIF_AD, GIFTAG(1, 0, 0, 0, 0, 1));
		
			dma_packet_add_tag(&attr_packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0));
		
			int tw, th;
			athena_set_tw_th(tex, &tw, &th);

			dma_packet_add_tag(&attr_packet, 
							   GS_TEX0_1, 
							   GS_SETREG_TEX0(tex->Vram/256, 
											  tex->TBW, 
											  tex->PSM,
											  tw, th, 
											  gsGlobal->PrimAlphaEnable, 
											  COLOR_MODULATE,
											  tex->VramClut/256, 
											  tex->ClutPSM, 
											  0, 0, 
											  tex->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
								);

			dma_packet_add_tag(&attr_packet, 
			                   DRAW_STQ2_REGLIST, 
							   VU_GS_GIFTAG(count, 
							                1, 1, 
											VU_GS_PRIM(m->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 
													   1, 1, 
													   gsGlobal->PrimFogEnable, 
													   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    							0, 3)
								);

			union {
				VECTOR v;
				__uint128_t q;
			} diffuse;

			__asm volatile ( 	
				"lq    $7,0x0(%1)\n"
				"sq    $7,0x0(%0)\n"
				 : : "r" (&diffuse), "r" (m->materials[m->material_indices[i].index].diffuse):"$7","memory");

			dma_packet_add_uquad(&attr_packet, diffuse.q);

			dma_packet_create(&draw_packet, vif_packets[context], 0);

			unpack_list_open(&draw_packet, 0, true);
			{
				unpack_list_append(&draw_packet, attr_packet.base, 6);
				unpack_list_append(&draw_packet, &positions[idxs_drawn], count);
				unpack_list_append(&draw_packet, &texcoords[idxs_drawn], count);
				unpack_list_append(&draw_packet, &normals[idxs_drawn], count);
			}
			unpack_list_close(&draw_packet);

			dma_packet_start_program(&draw_packet, last_index == -1);
			dma_packet_add_end_tag(&draw_packet);

			dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);

			dma_packet_destroy(&draw_packet);

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		last_index = m->material_indices[i].end;
	}

	context = !context;
}

void draw_vu1_with_lights_notex(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z) {
	GSGLOBAL *gsGlobal = getGSGLOBAL();

	update_vu_program(VU1Draw3DLightsColorsNoTex);

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_light;
	MATRIX local_screen;

  	// Create the local_world matrix.
  	matrix_unit(local_world);
  	matrix_rotate(local_world, local_world, object_rotation);
  	matrix_translate(local_world, local_world, object_position);

  	// Create the local_light matrix.
  	matrix_unit(local_light);
  	matrix_rotate(local_light, local_light, object_rotation);

  	// Create the local_screen matrix.
  	matrix_unit(local_screen);

  	matrix_multiply(local_screen, local_screen, local_world);
  	matrix_multiply(local_screen, local_screen, world_view);
  	matrix_multiply(local_screen, local_screen, view_screen);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dma_packet_create(&draw_packet, vif_packets[context], 0);

	unpack_list_open(&draw_packet, 0, false);
	{
		unpack_list_append(&draw_packet, &local_screen,       4);
		unpack_list_append(&draw_packet, &local_light,        4);
		unpack_list_append(&draw_packet, &active_dir_lights,  1);
		unpack_list_append(&draw_packet, &dir_lights,         12);
	}
	unpack_list_close(&draw_packet);

	dma_packet_add_end_tag(&draw_packet);

	dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);

	dma_packet_destroy(&draw_packet);

	dma_packet_create(&attr_packet, cube_packet, 0);

	int last_index = -1;
	for(int i = 0; i < m->material_index_count; i++) {
		VECTOR* positions = &m->positions[last_index+1];
		VECTOR* normals = &m->normals[last_index+1];

		int idxs_to_draw = (m->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			dmaKit_wait(DMA_CHANNEL_VIF1, 0);

			int count = BATCH_SIZE;
			if (idxs_to_draw < BATCH_SIZE)
			{
				count = idxs_to_draw;
			}

			float fX = 2048.0f+gsGlobal->Width/2;
			float fY = 2048.0f+gsGlobal->Height/2;
			float fZ = ((float)get_max_z(gsGlobal));

			dma_packet_reset(&attr_packet);

			dma_packet_add_float(&attr_packet, fX);
			dma_packet_add_float(&attr_packet, fY);
			dma_packet_add_float(&attr_packet, fZ);

			dma_packet_add_uint(&attr_packet, count);

			dma_packet_add_tag(&attr_packet, 
			                   DRAW_NOTEX_REGLIST, 
							   VU_GS_GIFTAG(count, 
							                1, 1, 
											VU_GS_PRIM(m->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 
													   1, 0, 
													   gsGlobal->PrimFogEnable, 
													   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    							0, 2)
								);

			union {
				VECTOR v;
				__uint128_t q;
			} diffuse;

			__asm volatile ( 	
				"lq    $7,0x0(%1)\n"
				"sq    $7,0x0(%0)\n"
				 : : "r" (&diffuse), "r" (m->materials[m->material_indices[i].index].diffuse):"$7","memory");

			dma_packet_add_uquad(&attr_packet, diffuse.q);

			dma_packet_create(&draw_packet, vif_packets[context], 0);

			unpack_list_open(&draw_packet, 0, true);
			{
				unpack_list_append(&draw_packet, attr_packet.base, 3);
				unpack_list_append(&draw_packet, &positions[idxs_drawn], count);
				unpack_list_append(&draw_packet, &normals[idxs_drawn], count);
			}
			unpack_list_close(&draw_packet);

			dma_packet_start_program(&draw_packet, last_index == -1);
			dma_packet_add_end_tag(&draw_packet);

			dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);

			dma_packet_destroy(&draw_packet);

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		last_index = m->material_indices[i].end;
	}

	context = !context;
}

void draw_vu1_with_spec_lights(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z) {
	GSGLOBAL *gsGlobal = getGSGLOBAL();

	update_vu_program(VU1Draw3DSpec);

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_light;
	MATRIX local_screen;

  	// Create the local_world matrix.
  	matrix_unit(local_world);
  	matrix_rotate(local_world, local_world, object_rotation);
  	matrix_translate(local_world, local_world, object_position);

  	// Create the local_light matrix.
  	matrix_unit(local_light);
  	matrix_rotate(local_light, local_light, object_rotation);

  	// Create the local_screen matrix.
  	matrix_unit(local_screen);

  	matrix_multiply(local_screen, local_screen, local_world);
  	matrix_multiply(local_screen, local_screen, world_view);
  	matrix_multiply(local_screen, local_screen, view_screen);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dma_packet_create(&draw_packet, vif_packets[context], 0);

	unpack_list_open(&draw_packet, 0, false);
	{
		unpack_list_append(&draw_packet, &local_screen,       4);
		unpack_list_append(&draw_packet, &local_light,        4);
		unpack_list_append(&draw_packet, &active_dir_lights,  1);
		unpack_list_append(&draw_packet, getCameraPosition(), 1);
		unpack_list_append(&draw_packet, &dir_lights,         16);
	}
	unpack_list_close(&draw_packet);

	dma_packet_add_end_tag(&draw_packet);

	dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);

	dma_packet_destroy(&draw_packet);

	dma_packet_create(&attr_packet, cube_packet, 0);

	int last_index = -1;
	GSTEXTURE* tex = NULL;
	for(int i = 0; i < m->material_index_count; i++) {
		GSTEXTURE *cur_tex = m->textures[m->materials[m->material_indices[i].index].texture_id];
		if (cur_tex != tex) {
			gsKit_TexManager_bind(gsGlobal, cur_tex);
			tex = cur_tex;
		}

		VECTOR* positions = &m->positions[last_index+1];
		VECTOR* texcoords = &m->texcoords[last_index+1];
		VECTOR* normals = &m->normals[last_index+1];

		int idxs_to_draw = (m->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			dmaKit_wait(DMA_CHANNEL_VIF1, 0);

			int count = BATCH_SIZE;
			if (idxs_to_draw < BATCH_SIZE)
			{
				count = idxs_to_draw;
			}

			float fX = 2048.0f+gsGlobal->Width/2;
			float fY = 2048.0f+gsGlobal->Height/2;
			float fZ = ((float)get_max_z(gsGlobal));

			dma_packet_reset(&attr_packet);

			dma_packet_add_float(&attr_packet, fX);
			dma_packet_add_float(&attr_packet, fY);
			dma_packet_add_float(&attr_packet, fZ);

			dma_packet_add_uint(&attr_packet, count);
		
			dma_packet_add_tag(&attr_packet, GIF_AD, GIFTAG(1, 0, 0, 0, 0, 1));
		
			dma_packet_add_tag(&attr_packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0));
		
			int tw, th;
			athena_set_tw_th(tex, &tw, &th);

			dma_packet_add_tag(&attr_packet, 
							   GS_TEX0_1, 
							   GS_SETREG_TEX0(tex->Vram/256, 
											  tex->TBW, 
											  tex->PSM,
											  tw, th, 
											  gsGlobal->PrimAlphaEnable, 
											  COLOR_MODULATE,
											  tex->VramClut/256, 
											  tex->ClutPSM, 
											  0, 0, 
											  tex->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
								);

			dma_packet_add_tag(&attr_packet, 
			                   DRAW_STQ2_REGLIST, 
							   VU_GS_GIFTAG(count, 
							                1, 1, 
											VU_GS_PRIM(m->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 
													   1, 1, 
													   gsGlobal->PrimFogEnable, 
													   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    							0, 3)
								);

			union {
				VECTOR v;
				__uint128_t q;
			} diffuse;

			__asm volatile ( 	
				"lq    $7,0x0(%1)\n"
				"sq    $7,0x0(%0)\n"
				 : : "r" (&diffuse), "r" (m->materials[m->material_indices[i].index].diffuse):"$7","memory");

			dma_packet_add_uquad(&attr_packet, diffuse.q);
			dma_packet_create(&draw_packet, vif_packets[context], 0);

			unpack_list_open(&draw_packet, 0, true);
			{
				unpack_list_append(&draw_packet, attr_packet.base, 6);
				unpack_list_append(&draw_packet, &positions[idxs_drawn], count);
				unpack_list_append(&draw_packet, &texcoords[idxs_drawn], count);
				unpack_list_append(&draw_packet, &normals[idxs_drawn], count);
			}
			unpack_list_close(&draw_packet);

			dma_packet_start_program(&draw_packet, last_index == -1);
			dma_packet_add_end_tag(&draw_packet);

			dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);

			dma_packet_destroy(&draw_packet);

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		last_index = m->material_indices[i].end;
	}

	context = !context;
}

void draw_vu1_with_spec_lights_notex(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z) {
	GSGLOBAL *gsGlobal = getGSGLOBAL();

	update_vu_program(VU1Draw3DSpecNoTex);

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_light;
	MATRIX local_screen;

  	// Create the local_world matrix.
  	matrix_unit(local_world);
  	matrix_rotate(local_world, local_world, object_rotation);
  	matrix_translate(local_world, local_world, object_position);

  	// Create the local_light matrix.
  	matrix_unit(local_light);
  	matrix_rotate(local_light, local_light, object_rotation);

  	// Create the local_screen matrix.
  	matrix_unit(local_screen);

  	matrix_multiply(local_screen, local_screen, local_world);
  	matrix_multiply(local_screen, local_screen, world_view);
  	matrix_multiply(local_screen, local_screen, view_screen);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	dma_packet_create(&draw_packet, vif_packets[context], 0);

	unpack_list_open(&draw_packet, 0, false);
	{
		unpack_list_append(&draw_packet, &local_screen,       4);
		unpack_list_append(&draw_packet, &local_light,        4);
		unpack_list_append(&draw_packet, &active_dir_lights,  1);
		unpack_list_append(&draw_packet, getCameraPosition(), 1);
		unpack_list_append(&draw_packet, &dir_lights,         16);
	}
	unpack_list_close(&draw_packet);

	dma_packet_add_end_tag(&draw_packet);

	dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);

	dma_packet_destroy(&draw_packet);

	dma_packet_create(&attr_packet, cube_packet, 0);

	int last_index = -1;
	for(int i = 0; i < m->material_index_count; i++) {
		VECTOR* positions = &m->positions[last_index+1];
		VECTOR* normals = &m->normals[last_index+1];

		int idxs_to_draw = (m->material_indices[i].end-last_index);
		int idxs_drawn = 0;

		while (idxs_to_draw > 0) {
			dmaKit_wait(DMA_CHANNEL_VIF1, 0);

			int count = BATCH_SIZE;
			if (idxs_to_draw < BATCH_SIZE)
			{
				count = idxs_to_draw;
			}

			float fX = 2048.0f+gsGlobal->Width/2;
			float fY = 2048.0f+gsGlobal->Height/2;
			float fZ = ((float)get_max_z(gsGlobal));

			dma_packet_reset(&attr_packet);

			dma_packet_add_float(&attr_packet, fX);
			dma_packet_add_float(&attr_packet, fY);
			dma_packet_add_float(&attr_packet, fZ);

			dma_packet_add_uint(&attr_packet, count);

			dma_packet_add_tag(&attr_packet, 
			                   DRAW_NOTEX_REGLIST, 
							   VU_GS_GIFTAG(count, 
							                1, 1, 
											VU_GS_PRIM(m->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 
													   1, 0, 
													   gsGlobal->PrimFogEnable, 
													   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    							0, 2)
								);

			union {
				VECTOR v;
				__uint128_t q;
			} diffuse;

			__asm volatile ( 	
				"lq    $7,0x0(%1)\n"
				"sq    $7,0x0(%0)\n"
				 : : "r" (&diffuse), "r" (m->materials[m->material_indices[i].index].diffuse):"$7","memory");

			dma_packet_add_uquad(&attr_packet, diffuse.q);

			dma_packet_create(&draw_packet, vif_packets[context], 0);

			unpack_list_open(&draw_packet, 0, true);
			{
				unpack_list_append(&draw_packet, attr_packet.base, 3);
				unpack_list_append(&draw_packet, &positions[idxs_drawn], count);
				unpack_list_append(&draw_packet, &normals[idxs_drawn], count);
			}
			unpack_list_close(&draw_packet);

			dma_packet_start_program(&draw_packet, last_index == -1);
			dma_packet_add_end_tag(&draw_packet);

			dma_packet_send(&draw_packet, DMA_CHANNEL_VIF1);

			dma_packet_destroy(&draw_packet);

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		last_index = m->material_indices[i].end;
	}

	context = !context;
}
