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

void calculate_bbox(athena_render_data* res_m) {
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

void obj_transfer_vertex(athena_render_data* m, uint32_t dst_idx, fastObjMesh* obj, uint32_t src_idx) {
	copy_vector(&m->positions[dst_idx], obj->positions + (3 * obj->indices[src_idx].p));
	m->positions[dst_idx][3] = 1.0f;

	m->texcoords[dst_idx][0] = obj->texcoords[2 * obj->indices[src_idx].t];
    m->texcoords[dst_idx][1] = 1.0f - obj->texcoords[2 * obj->indices[src_idx].t + 1];
    m->texcoords[dst_idx][2] = 1.0f;
    m->texcoords[dst_idx][3] = 1.0f;

	copy_vector(&m->normals[dst_idx],  obj->normals + (3 * obj->indices[src_idx].n));
	m->normals[dst_idx][3] = 1.0f;
}

void loadOBJ(athena_render_data* res_m, const char* path, GSTEXTURE* text) {
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

	res_m->pipeline = PL_DEFAULT;

	fast_obj_destroy(m);
}
