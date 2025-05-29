#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>

#define CGLTF_IMPLEMENTATION
#include "fast_obj/fast_obj.h"
#include "cgltf/cgltf.h"

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

void load_gltf_material(ath_mat* mat, const cgltf_material* gltf_mat, athena_render_data* res_m) {
    init_vector(mat->ambient);
    init_vector(mat->diffuse);
    init_vector(mat->specular);
    init_vector(mat->emission);
    init_vector(mat->transmittance);
    init_vector(mat->transmission_filter);
    
    mat->shininess = 1.0f;
    mat->refraction = 1.0f;
    mat->disolve = 1.0f;
    mat->texture_id = -1;
    
    if (!gltf_mat) return;
    
    if (gltf_mat->has_pbr_metallic_roughness) {
        const cgltf_pbr_metallic_roughness* pbr = &gltf_mat->pbr_metallic_roughness;

        mat->diffuse[0] = pbr->base_color_factor[0];
        mat->diffuse[1] = pbr->base_color_factor[1];
        mat->diffuse[2] = pbr->base_color_factor[2];
        mat->diffuse[3] = pbr->base_color_factor[3];

        float metallic = pbr->metallic_factor;
        mat->specular[0] = metallic;
        mat->specular[1] = metallic;
        mat->specular[2] = metallic;
        mat->specular[3] = 1.0f;
        
        // Roughness affects shininess (inverse relationship)
        float roughness = pbr->roughness_factor;
        mat->shininess = (1.0f - roughness) * 128.0f;

        if (pbr->base_color_texture.texture) {
            const cgltf_texture* tex = pbr->base_color_texture.texture;
            if (tex->image && tex->image->uri) {
                mat->texture_id = res_m->texture_count;
                GSTEXTURE* texture = malloc(sizeof(GSTEXTURE));
                load_image(texture, tex->image->uri, true);
                append_texture(res_m, texture);
            }
        }
    }
    
    if (gltf_mat->has_emissive_strength) {
        mat->emission[0] = gltf_mat->emissive_factor[0];
        mat->emission[1] = gltf_mat->emissive_factor[1];
        mat->emission[2] = gltf_mat->emissive_factor[2];
        mat->emission[3] = 1.0f;
    }

    if (gltf_mat->alpha_mode == cgltf_alpha_mode_blend) {
        mat->disolve = gltf_mat->alpha_cutoff;
    }
}

void gltfReadFloat(const float* _accessorData, cgltf_size _accessorNumComponents, cgltf_size _index, float* _out, cgltf_size _outElementSize)
{
    const float* input = &_accessorData[_accessorNumComponents * _index];

    for (cgltf_size ii = 0; ii < _outElementSize; ++ii)
    {
        _out[ii] = (ii < _accessorNumComponents) ? input[ii] : 0.0f;
    }
}

void gltf_transfer_vertex(athena_render_data* m, uint32_t dst_idx, float* positions, float* texcoords, float* normals, float* colors, uint32_t src_idx)
{
    if (positions) {
        m->positions[dst_idx][0] = positions[src_idx * 3 + 0];
        m->positions[dst_idx][1] = positions[src_idx * 3 + 1];
        m->positions[dst_idx][2] = positions[src_idx * 3 + 2];
        m->positions[dst_idx][3] = 1.0f;
    }

    if (texcoords) {
        m->texcoords[dst_idx][0] = texcoords[src_idx * 2 + 0];
        m->texcoords[dst_idx][1] = 1.0f - texcoords[src_idx * 2 + 1]; 
        m->texcoords[dst_idx][2] = 1.0f;
        m->texcoords[dst_idx][3] = 1.0f;
    } else {
        m->texcoords[dst_idx][0] = 0.0f;
        m->texcoords[dst_idx][1] = 0.0f;
        m->texcoords[dst_idx][2] = 1.0f;
        m->texcoords[dst_idx][3] = 1.0f;
    }

    if (normals) {
        m->normals[dst_idx][0] = normals[src_idx * 3 + 0];
        m->normals[dst_idx][1] = normals[src_idx * 3 + 1];
        m->normals[dst_idx][2] = normals[src_idx * 3 + 2];
        m->normals[dst_idx][3] = 1.0f;
    } else {
        m->normals[dst_idx][0] = 0.0f;
        m->normals[dst_idx][1] = 1.0f;
        m->normals[dst_idx][2] = 0.0f;
        m->normals[dst_idx][3] = 1.0f;
    }

    if (colors) {
        m->colours[dst_idx][0] = colors[src_idx * 3 + 0];
        m->colours[dst_idx][1] = colors[src_idx * 3 + 1];
        m->colours[dst_idx][2] = colors[src_idx * 3 + 2];
        m->colours[dst_idx][3] = 1.0f;
    } else {
        m->colours[dst_idx][0] = 1.0f;
        m->colours[dst_idx][1] = 1.0f;
        m->colours[dst_idx][2] = 1.0f;
        m->colours[dst_idx][3] = 1.0f;
    }
}

void load_gltf_skinning_data(athena_render_data* res_m, float* joints, float* weights, uint32_t dst_idx, uint32_t src_idx) {
    if (!(joints && weights)) return;

    vertex_skin_data* skin = &res_m->skin_data[dst_idx];
    for (int j = 0; j < 4; j++) {
        skin->bone_indices[j] = (uint32_t)joints[src_idx * 4 + j];
        skin->bone_weights[j] = weights[src_idx * 4 + j];
    }

    float total_weight = skin->bone_weights[0] + skin->bone_weights[1] + 
                       skin->bone_weights[2] + skin->bone_weights[3];
    if (total_weight > 0.0f) {
        for (int j = 0; j < 4; j++) {
            skin->bone_weights[j] /= total_weight;
        }
    }
}

athena_skeleton* load_gltf_skeleton(cgltf_data* data, cgltf_skin* skin) {
    if (!skin || !skin->joints || skin->joints_count == 0) {
        return NULL;
    }
    
    athena_skeleton* skeleton = (athena_skeleton*)malloc(sizeof(athena_skeleton));
    skeleton->bone_count = skin->joints_count;
    skeleton->bones = (athena_bone*)malloc(skeleton->bone_count * sizeof(athena_bone));
    skeleton->bone_matrices = (MATRIX*)malloc(skeleton->bone_count * sizeof(MATRIX));

    float* inverse_bind_matrices = NULL;
    if (skin->inverse_bind_matrices) {
        cgltf_size float_count = cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, NULL, 0);
        inverse_bind_matrices = (float*)malloc(float_count * sizeof(float));
        cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, inverse_bind_matrices, float_count);
    }

    for (cgltf_size i = 0; i < skin->joints_count; i++) {
        cgltf_node* joint_node = skin->joints[i];
        athena_bone* bone = &skeleton->bones[i];
        
        bone->id = (uint32_t)i;

        if (joint_node->name) {
            strncpy(bone->name, joint_node->name, sizeof(bone->name) - 1);
            bone->name[sizeof(bone->name) - 1] = '\0';
        } else {
            snprintf(bone->name, sizeof(bone->name), "Bone_%u", (uint32_t)i);
        }

        bone->parent_id = -1;
        for (cgltf_size node_idx = 0; node_idx < data->nodes_count; node_idx++) {
            cgltf_node* node = &data->nodes[node_idx];
            for (cgltf_size child_idx = 0; child_idx < node->children_count; child_idx++) {
                if (node->children[child_idx] == joint_node) {
                    for (cgltf_size parent_joint_idx = 0; parent_joint_idx < skin->joints_count; parent_joint_idx++) {
                        if (skin->joints[parent_joint_idx] == node) {
                            bone->parent_id = (int32_t)parent_joint_idx;
                            break;
                        }
                    }
                    break;
                }
            }
            if (bone->parent_id != -1) break;
        }

        if (joint_node->has_translation) {
            bone->position[0] = joint_node->translation[0];
            bone->position[1] = joint_node->translation[1];
            bone->position[2] = joint_node->translation[2];
            bone->position[3] = 1.0f;
        } else {
            bone->position[0] = bone->position[1] = bone->position[2] = 0.0f;
            bone->position[3] = 1.0f;
        }
        
        if (joint_node->has_rotation) {
            bone->rotation[0] = joint_node->rotation[0];
            bone->rotation[1] = joint_node->rotation[1];
            bone->rotation[2] = joint_node->rotation[2];
            bone->rotation[3] = joint_node->rotation[3];
        } else {
            bone->rotation[0] = bone->rotation[1] = bone->rotation[2] = 0.0f;
            bone->rotation[3] = 1.0f;
        }
        
        if (joint_node->has_scale) {
            bone->scale[0] = joint_node->scale[0];
            bone->scale[1] = joint_node->scale[1];
            bone->scale[2] = joint_node->scale[2];
            bone->scale[3] = 1.0f;
        } else {
            bone->scale[0] = bone->scale[1] = bone->scale[2] = 1.0f;
            bone->scale[3] = 1.0f;
        }

        if (inverse_bind_matrices) {
            memcpy(&bone->inverse_bind, &inverse_bind_matrices[i * 16], sizeof(MATRIX));
        } else {
            matrix_unit(bone->inverse_bind);
        }

        matrix_unit(bone->bind_pose);
        matrix_unit(bone->current_transform);
        matrix_unit(skeleton->bone_matrices[i]);
    }
    
    if (inverse_bind_matrices) {
        free(inverse_bind_matrices);
    }
    
    return skeleton;
}

void loadGLTF(athena_render_data* res_m, const char* path, GSTEXTURE* text) {
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, path, &data);

    if (result != cgltf_result_success) {
        printf("Error: Fail while parsing glTF\n");
        return;
    }

    result = cgltf_load_buffers(&options, data, path);
    if (result != cgltf_result_success) {
        printf("Error: Fail while loading glTF buffers\n");
        cgltf_free(data);
        return;
    }

    res_m->tristrip = false; 

    res_m->texture_count = 0;
    res_m->textures = NULL;

    if (data->materials_count > 0) {
        res_m->materials = (ath_mat*)malloc(data->materials_count * sizeof(ath_mat));
        res_m->material_count = data->materials_count;
        
        for (size_t i = 0; i < data->materials_count; i++) {
            load_gltf_material(&res_m->materials[i], &data->materials[i], res_m);
        }
    } else {
        res_m->materials = (ath_mat*)malloc(sizeof(ath_mat));
        res_m->material_count = 1;
        
        load_gltf_material(&res_m->materials[0], NULL, res_m);
        
        if (text) {
            res_m->materials[0].texture_id = 0;
            append_texture(res_m, text);
        }
    }

    uint32_t total_vertices = 0;
    for (cgltf_size mesh_idx = 0; mesh_idx < data->meshes_count; ++mesh_idx) {
        cgltf_mesh* mesh = &data->meshes[mesh_idx];
        for (cgltf_size prim_idx = 0; prim_idx < mesh->primitives_count; ++prim_idx) {
            cgltf_primitive* primitive = &mesh->primitives[prim_idx];
            if (primitive->indices) {
                total_vertices += primitive->indices->count;
            } else if (primitive->attributes_count > 0) {
                total_vertices += primitive->attributes[0].data->count;
            }
        }
    }

    res_m->index_count = total_vertices;

    res_m->positions = alloc_vectors(res_m->index_count);
    res_m->texcoords = alloc_vectors(res_m->index_count);
    res_m->normals = alloc_vectors(res_m->index_count);
    res_m->colours = alloc_vectors(res_m->index_count);

    if (data->skins_count > 0) {
        res_m->skin_data = (vertex_skin_data*)malloc(res_m->index_count * sizeof(vertex_skin_data));
    }

    res_m->material_indices = (material_index*)malloc(data->meshes_count * sizeof(material_index));
    res_m->material_index_count = 0;

    uint32_t current_vertex = 0;

    for (cgltf_size mesh_idx = 0; mesh_idx < data->meshes_count; ++mesh_idx) {
        cgltf_mesh* mesh = &data->meshes[mesh_idx];

        for (cgltf_size prim_idx = 0; prim_idx < mesh->primitives_count; ++prim_idx) {
            cgltf_primitive* primitive = &mesh->primitives[prim_idx];

            float* positions = NULL;
            float* texcoords = NULL;
            float* normals = NULL;
            float* colors = NULL;

            float* joints = NULL;
            float* weights = NULL;

            uint32_t vertex_count = 0;

            for (cgltf_size attr_idx = 0; attr_idx < primitive->attributes_count; ++attr_idx) {
                cgltf_attribute* attribute = &primitive->attributes[attr_idx];
                cgltf_accessor* accessor = attribute->data;

                cgltf_size float_count = cgltf_accessor_unpack_floats(accessor, NULL, 0);
                float* accessor_data = (float*)malloc(float_count * sizeof(float));
                cgltf_accessor_unpack_floats(accessor, accessor_data, float_count);

                if (attribute->type == cgltf_attribute_type_position && attribute->index == 0) {
                    positions = accessor_data;
                    vertex_count = accessor->count;
                }
                else if (attribute->type == cgltf_attribute_type_texcoord && attribute->index == 0) {
                    texcoords = accessor_data;
                }
                else if (attribute->type == cgltf_attribute_type_normal && attribute->index == 0) {
                    normals = accessor_data;
                }
                else if (attribute->type == cgltf_attribute_type_color && attribute->index == 0) {
                    colors = accessor_data;
                } 
                else if (attribute->type == cgltf_attribute_type_joints && attribute->index == 0) {
                    joints = accessor_data;
                }
                else if (attribute->type == cgltf_attribute_type_weights && attribute->index == 0) {
                    weights = accessor_data;
                }
                else {
                    free(accessor_data); 
                }
            }

            if (primitive->indices) {
                cgltf_accessor* indices_accessor = primitive->indices;

                for (cgltf_size i = 0; i < indices_accessor->count; ++i) {
                    uint32_t index = cgltf_accessor_read_index(indices_accessor, i);
                    load_gltf_skinning_data(res_m, joints, weights, current_vertex, index);
                    gltf_transfer_vertex(res_m, current_vertex, positions, texcoords, normals, colors, index);
                    current_vertex++;
                }
            }
            else {
                for (uint32_t i = 0; i < vertex_count; ++i) {
                    load_gltf_skinning_data(res_m, joints, weights, current_vertex, i);
                    gltf_transfer_vertex(res_m, current_vertex, positions, texcoords, normals, colors, i);
                    current_vertex++;
                }
            }

            if (res_m->material_index_count < data->meshes_count) {
                res_m->material_indices[res_m->material_index_count].index = 
                    primitive->material ? (primitive->material - data->materials) : 0;
                res_m->material_indices[res_m->material_index_count].end = current_vertex-1;
                res_m->material_index_count++;
            }

            if (positions) free(positions);
            if (texcoords) free(texcoords);
            if (normals) free(normals);
            if (colors) free(colors);

            if (joints) free(joints);
            if (weights) free(weights);
        }
    }

    calculate_bbox(res_m);

    res_m->pipeline = PL_DEFAULT;

    if (data->skins_count > 0) {
        res_m->skeleton = load_gltf_skeleton(data, &data->skins[0]);
        load_gltf_animations(res_m, data);
    }

    cgltf_free(data);
}

void load_gltf_animations(athena_render_data* res_m, cgltf_data* data) {
    if (!data->animations_count || !res_m->skeleton) {
        res_m->anim_controller.animations = NULL;
        res_m->anim_controller.count = 0;
        return;
    }
    
    res_m->anim_controller.count = data->animations_count;
    res_m->anim_controller.animations = (athena_animation*)malloc(res_m->anim_controller.count * sizeof(athena_animation));
    
    for (cgltf_size anim_idx = 0; anim_idx < data->animations_count; anim_idx++) {
        cgltf_animation* gltf_anim = &data->animations[anim_idx];
        athena_animation* anim = &res_m->anim_controller.animations[anim_idx];

        if (gltf_anim->name) {
            strncpy(anim->name, gltf_anim->name, sizeof(anim->name) - 1);
            anim->name[sizeof(anim->name) - 1] = '\0';
        } else {
            snprintf(anim->name, sizeof(anim->name), "Animation_%u", (uint32_t)anim_idx);
        }

        anim->duration = 0.0f;
        anim->ticks_per_second = 1.0f;
        anim->bone_animation_count = gltf_anim->channels_count;
        anim->bone_animations = (athena_bone_animation*)malloc(anim->bone_animation_count * sizeof(athena_bone_animation));

        for (cgltf_size channel_idx = 0; channel_idx < gltf_anim->channels_count; channel_idx++) {
            cgltf_animation_channel* channel = &gltf_anim->channels[channel_idx];
            cgltf_animation_sampler* sampler = channel->sampler;
            athena_bone_animation* bone_anim = &anim->bone_animations[channel_idx];

            bone_anim->bone_id = UINT32_MAX;
            for (uint32_t bone_idx = 0; bone_idx < res_m->skeleton->bone_count; bone_idx++) {
                if (channel->target_node == data->skins[0].joints[bone_idx]) {
                    bone_anim->bone_id = bone_idx;
                    break;
                }
            }
            
            if (bone_anim->bone_id == UINT32_MAX) {
                bone_anim->position_keys = NULL;
                bone_anim->rotation_keys = NULL;
                bone_anim->scale_keys = NULL;
                bone_anim->position_key_count = 0;
                bone_anim->rotation_key_count = 0;
                bone_anim->scale_key_count = 0;
                continue;
            }

            cgltf_size input_count = cgltf_accessor_unpack_floats(sampler->input, NULL, 0);
            float* input_times = (float*)malloc(input_count * sizeof(float));
            cgltf_accessor_unpack_floats(sampler->input, input_times, input_count);
            
            cgltf_size output_count = cgltf_accessor_unpack_floats(sampler->output, NULL, 0);
            float* output_values = (float*)malloc(output_count * sizeof(float));
            cgltf_accessor_unpack_floats(sampler->output, output_values, output_count);
            
            for (cgltf_size i = 0; i < input_count; i++) {
                if (input_times[i] > anim->duration) {
                    anim->duration = input_times[i];
                }
            }
            
            if (channel->target_path == cgltf_animation_path_type_translation) {
                bone_anim->position_key_count = (uint32_t)input_count;
                bone_anim->position_keys = (athena_keyframe*)malloc(bone_anim->position_key_count * sizeof(athena_keyframe));
                
                for (uint32_t i = 0; i < bone_anim->position_key_count; i++) {
                    bone_anim->position_keys[i].time = input_times[i];
                    bone_anim->position_keys[i].position[0] = output_values[i * 3 + 0];
                    bone_anim->position_keys[i].position[1] = output_values[i * 3 + 1];
                    bone_anim->position_keys[i].position[2] = output_values[i * 3 + 2];
                    bone_anim->position_keys[i].position[3] = 1.0f;
                }
            }
            else if (channel->target_path == cgltf_animation_path_type_rotation) {
                bone_anim->rotation_key_count = (uint32_t)input_count;
                bone_anim->rotation_keys = (athena_keyframe*)malloc(bone_anim->rotation_key_count * sizeof(athena_keyframe));
                
                for (uint32_t i = 0; i < bone_anim->rotation_key_count; i++) {
                    bone_anim->rotation_keys[i].time = input_times[i];
                    bone_anim->rotation_keys[i].rotation[0] = output_values[i * 4 + 0]; // x
                    bone_anim->rotation_keys[i].rotation[1] = output_values[i * 4 + 1]; // y
                    bone_anim->rotation_keys[i].rotation[2] = output_values[i * 4 + 2]; // z
                    bone_anim->rotation_keys[i].rotation[3] = output_values[i * 4 + 3]; // w
                }
            }
            else if (channel->target_path == cgltf_animation_path_type_scale) {
                bone_anim->scale_key_count = (uint32_t)input_count;
                bone_anim->scale_keys = (athena_keyframe*)malloc(bone_anim->scale_key_count * sizeof(athena_keyframe));
                
                for (uint32_t i = 0; i < bone_anim->scale_key_count; i++) {
                    bone_anim->scale_keys[i].time = input_times[i];
                    bone_anim->scale_keys[i].scale[0] = output_values[i * 3 + 0];
                    bone_anim->scale_keys[i].scale[1] = output_values[i * 3 + 1];
                    bone_anim->scale_keys[i].scale[2] = output_values[i * 3 + 2];
                    bone_anim->scale_keys[i].scale[3] = 1.0f;
                }
            }
            
            if (channel->target_path != cgltf_animation_path_type_translation) {
                bone_anim->position_keys = NULL;
                bone_anim->position_key_count = 0;
            }
            if (channel->target_path != cgltf_animation_path_type_rotation) {
                bone_anim->rotation_keys = NULL;
                bone_anim->rotation_key_count = 0;
            }
            if (channel->target_path != cgltf_animation_path_type_scale) {
                bone_anim->scale_keys = NULL;
                bone_anim->scale_key_count = 0;
            }
            
            free(input_times);
            free(output_values);
        }
    }
}

void loadModel(athena_render_data* res_m, const char* path, GSTEXTURE* text) {
    const char* ext = strrchr(path, '.');
    if (!ext) {
        printf("Unknown file format: %s\n", path);
        return;
    }
    
    if (strcmp(ext, ".gltf") == 0 || strcmp(ext, ".glb") == 0) {
        loadGLTF(res_m, path, text);
    } else if (strcmp(ext, ".obj") == 0 || strcmp(ext, ".objf") == 0) {
        loadOBJ(res_m, path, text);
    } else {
        printf("Unsupported file format: %s\n", ext);
    }
}