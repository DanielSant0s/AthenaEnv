#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>
#include "fast_obj/fast_obj.h"

#include "include/render.h"

#include "mesh_data.c"

#include "vif.h"

extern u32 VU1Draw3D_CodeStart __attribute__((section(".vudata")));
extern u32 VU1Draw3D_CodeEnd __attribute__((section(".vudata")));

MATRIX view_screen;

VECTOR camera_position = { 0.00f, 0.00f, 0.00f, 1.00f };
VECTOR camera_rotation = { 0.00f, 0.00f, 0.00f, 1.00f };

int light_count;
VECTOR* light_direction;
VECTOR* light_colour;
int* light_type;

void init3D(float aspect)
{
	create_view_screen(view_screen, aspect,  -0.20f, 0.20f, -0.20f, 0.20f, 1.00f, 2000.00f);

}

void setCameraPosition(float x, float y, float z){
	camera_position[0] = x;
	camera_position[1] = y;
	camera_position[2] = z;
	camera_position[3] = 1.00f;
}

void setCameraRotation(float x, float y, float z){
	camera_rotation[0] = x;
	camera_rotation[1] = y;
	camera_rotation[2] = z;
	camera_rotation[3] = 1.00f;
}

void setLightQuantity(int quantity){
	light_count = quantity;
	light_direction = (VECTOR*)memalign(128, sizeof(VECTOR) * light_count);
	light_colour = (VECTOR*)memalign(128, sizeof(VECTOR) * light_count);
	light_type = (int*)memalign(128, sizeof(int) * light_count);
}

void createLight(int lightid, float dir_x, float dir_y, float dir_z, int type, float r, float g, float b){
	light_direction[lightid-1][0] = dir_x;
	light_direction[lightid-1][1] = dir_y;
	light_direction[lightid-1][2] = dir_z;
	light_direction[lightid-1][3] = 1.00f;

	light_colour[lightid-1][0] = r;
	light_colour[lightid-1][1] = g;
	light_colour[lightid-1][2] = b;
	light_colour[lightid-1][3] = 1.00f;

	light_type[lightid-1] = type;
}


int athena_process_xyz_rgbaq(GSPRIMPOINT *output, GSGLOBAL* gsGlobal, int count, color_f_t *colours, vertex_f_t *vertices)
{

	int z;
	unsigned int max_z;
	float q = 1.00f;

	switch(gsGlobal->PSMZ){
		case GS_PSMZ_32:
			z = 32;
			break;

		case GS_PSMZ_24:
			z = 24;
			break;

		case GS_PSMZ_16:
		case GS_PSMZ_16S:
			z = 16;
			break;
		
		default:
			return -1;
	}

	int center_x = gsGlobal->Width/2;
	int center_y = gsGlobal->Height/2;
	max_z = 1 << (z - 1);

	for (int i = 0; i < count; i++)
	{
		// Calculate the Q value.
		if (vertices[i].w != 0)
		{
			q = 1 / vertices[i].w;
		}

		output[i].rgbaq.color.r = (int)(colours[i].r * 128.0f);
		output[i].rgbaq.color.g = (int)(colours[i].g * 128.0f);
		output[i].rgbaq.color.b = (int)(colours[i].b * 128.0f);
		output[i].rgbaq.color.a = 0x80;
		output[i].rgbaq.color.q = q;
		output[i].rgbaq.tag = GS_RGBAQ;

		output[i].xyz2.xyz.x = gsKit_float_to_int_x(gsGlobal, (vertices[i].x + 1.0f) * center_x);
		output[i].xyz2.xyz.y = gsKit_float_to_int_y(gsGlobal, (vertices[i].y + 1.0f) * center_y);
		output[i].xyz2.xyz.z = (unsigned int)((vertices[i].z + 1.0f) * max_z);
		output[i].xyz2.tag = GS_XYZ2;

	}

	// End function.
	return 0;

}


int athena_process_xyz_rgbaq_st(GSPRIMSTQPOINT *output, GSGLOBAL* gsGlobal, int count, color_f_t *colours, vertex_f_t *vertices, texel_f_t *coords)
{

	int z;
	unsigned int max_z;
	float q = 1.00f;

	switch(gsGlobal->PSMZ){
		case GS_PSMZ_32:
			z = 32;
			break;

		case GS_PSMZ_24:
			z = 24;
			break;

		case GS_PSMZ_16:
		case GS_PSMZ_16S:
			z = 16;
			break;
		
		default:
			return -1;
	}

	int center_x = gsGlobal->Width/2;
	int center_y = gsGlobal->Height/2;
	max_z = 1 << (z - 1);

	for (int i = 0; i < count; i++)
	{
		// Calculate the Q value.
		if (vertices[i].w != 0)
		{
			q = 1 / vertices[i].w;
		}

		output[i].rgbaq.color.r = (int)(colours[i].r * 128.0f);
		output[i].rgbaq.color.g = (int)(colours[i].g * 128.0f);
		output[i].rgbaq.color.b = (int)(colours[i].b * 128.0f);
		output[i].rgbaq.color.a = 0x80;
		output[i].rgbaq.color.q = q;
		output[i].rgbaq.tag = GS_RGBAQ;

		output[i].stq.st.s = coords[i].s * q;
		output[i].stq.st.t = coords[i].t * q;
		output[i].stq.tag = GS_ST;

		output[i].xyz2.xyz.x = gsKit_float_to_int_x(gsGlobal, (vertices[i].x + 1.0f) * center_x);
		output[i].xyz2.xyz.y = gsKit_float_to_int_y(gsGlobal, (vertices[i].y + 1.0f) * center_y);
		output[i].xyz2.xyz.z = (unsigned int)((vertices[i].z + 1.0f) * max_z);
		output[i].xyz2.tag = GS_XYZ2;

	}

	// End function.
	return 0;

}

void render_notex(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

    GSGLOBAL *gsGlobal = getGSGLOBAL();

	// Matrices to setup the 3D environment and camera
	MATRIX local_world;
	MATRIX local_light;
	MATRIX world_view;
	MATRIX local_screen;

	//gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
	//gsKit_set_test(gsGlobal, GS_ATEST_OFF);
	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	// Create the local_world matrix.
	create_local_world(local_world, object_position, object_rotation);
	create_local_light(local_light, object_rotation);
	create_world_view(world_view, camera_position, camera_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);
	if(clip_bounding_box(local_screen, m->bounding_box)) return;

	// Calculate the normal values.
	calculate_normals(m->tmp_normals, m->indexCount, m->normals, local_light);
	calculate_lights(m->tmp_lights, m->indexCount, m->tmp_normals, light_direction, light_colour, light_type, light_count);
	calculate_colours((VECTOR *)m->tmp_colours, m->indexCount, m->colours, m->tmp_lights);
	calculate_vertices_clipped((VECTOR *)m->tmp_xyz, m->indexCount, m->positions, local_screen);

	athena_process_xyz_rgbaq(m->vertices, gsGlobal, m->indexCount, m->tmp_colours, m->tmp_xyz);
	gsKit_prim_list_triangle_gouraud_3d(gsGlobal, m->indexCount, m->vertices);
}

void render_singletex(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

    GSGLOBAL *gsGlobal = getGSGLOBAL();

	// Matrices to setup the 3D environment and camera
	MATRIX local_world;
	MATRIX local_light;
	MATRIX world_view;
	MATRIX local_screen;

	//gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
	//gsKit_set_test(gsGlobal, GS_ATEST_OFF);
	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	// Create the local_world matrix.
	create_local_world(local_world, object_position, object_rotation);
	create_local_light(local_light, object_rotation);
	create_world_view(world_view, camera_position, camera_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);
	if(clip_bounding_box(local_screen, m->bounding_box)) return;

	// Calculate the normal values.
	calculate_normals(m->tmp_normals, m->indexCount, m->normals, local_light);
	calculate_lights(m->tmp_lights, m->indexCount, m->tmp_normals, light_direction, light_colour, light_type, light_count);
	calculate_colours((VECTOR *)m->tmp_colours, m->indexCount, m->colours, m->tmp_lights);
	calculate_vertices_clipped((VECTOR *)m->tmp_xyz, m->indexCount, m->positions, local_screen);

	athena_process_xyz_rgbaq_st(m->vertices, gsGlobal, m->indexCount, m->tmp_colours, m->tmp_xyz, (texel_f_t *)m->texcoords);
	gsKit_TexManager_bind(gsGlobal, m->textures[0]);
	gsKit_prim_list_triangle_goraud_texture_stq_3d(gsGlobal, m->textures[0], m->indexCount, m->vertices);
}

void render_multitex(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

    GSGLOBAL *gsGlobal = getGSGLOBAL();

	// Matrices to setup the 3D environment and camera
	MATRIX local_world;
	MATRIX local_light;
	MATRIX world_view;
	MATRIX local_screen;

	//gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
	//gsKit_set_test(gsGlobal, GS_ATEST_OFF);
	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	// Create the local_world matrix.
	create_local_world(local_world, object_position, object_rotation);
	create_local_light(local_light, object_rotation);
	create_world_view(world_view, camera_position, camera_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);
	if(clip_bounding_box(local_screen, m->bounding_box)) return;

	// Calculate the normal values.
	calculate_normals(m->tmp_normals, m->indexCount, m->normals, local_light);
	calculate_lights(m->tmp_lights, m->indexCount, m->tmp_normals, light_direction, light_colour, light_type, light_count);
	calculate_colours((VECTOR *)m->tmp_colours, m->indexCount, m->colours, m->tmp_lights);
	calculate_vertices_clipped((VECTOR *)m->tmp_xyz, m->indexCount, m->positions, local_screen);

	athena_process_xyz_rgbaq_st(m->vertices, gsGlobal, m->indexCount, m->tmp_colours, m->tmp_xyz, (texel_f_t *)m->texcoords);

	GSPRIMSTQPOINT* vertices = m->vertices;
	
	int lastIdx = -1;
	for(int i = 0; i < m->tex_count; i++) {
		gsKit_TexManager_bind(gsGlobal, m->textures[i]);
		gsKit_prim_list_triangle_goraud_texture_stq_3d(gsGlobal, m->textures[i], (m->tex_ranges[i]-lastIdx), &vertices[lastIdx+1]);
		lastIdx = m->tex_ranges[i];
	}
}

model* loadOBJ(const char* path, GSTEXTURE* text) {
    model* res_m = (model*)malloc(sizeof(model));
    fastObjMesh* m = fast_obj_read(path);

    int positionCount = m->position_count;
    int texcoordCount = m->texcoord_count;
    int normalCount = m->normal_count;
    int indexCount = m->index_count;

    VECTOR* c_verts = (VECTOR*)malloc(positionCount * sizeof(VECTOR));
    VECTOR* c_texcoords = (VECTOR*)malloc(texcoordCount * sizeof(VECTOR));
    VECTOR* c_normals = (VECTOR*)malloc(normalCount * sizeof(VECTOR));
    VECTOR* c_colours = (VECTOR*)malloc(indexCount * sizeof(VECTOR));

	char* tex_names[10];
	res_m->tex_count = 0;

    for (int i = 0; i < positionCount; i++) {
        memcpy(c_verts[i], m->positions + (3 * i), sizeof(VECTOR));
        c_verts[i][3] = 1.0f;
    }

    for (int i = 0; i < normalCount; i++) {
        memcpy(c_normals[i], m->normals + (3 * i), sizeof(VECTOR));
        c_normals[i][3] = 1.0f;
    }

    float oneMinusTexcoord;
    for (int i = 0; i < texcoordCount; i++) {
        c_texcoords[i][0] = m->texcoords[(2 * i) + 0];
        oneMinusTexcoord = 1.0f - m->texcoords[(2 * i) + 1];
        c_texcoords[i][1] = oneMinusTexcoord;
        c_texcoords[i][2] = 1.0f;
        c_texcoords[i][3] = 1.0f;
    }

    res_m->facesCount = m->face_count;
	res_m->indexCount = m->index_count;
    res_m->positions = (VECTOR*)malloc(indexCount * sizeof(VECTOR));
    res_m->texcoords = (VECTOR*)malloc(indexCount * sizeof(VECTOR));
    res_m->normals = (VECTOR*)malloc(indexCount * sizeof(VECTOR));
    res_m->colours = (VECTOR*)malloc(indexCount * sizeof(VECTOR));

    int faceMaterialIndex;
	char* oldTex = NULL;
    for (int i = 0, j = 0; i < indexCount; i++, j += 3) {
        int vertIndex = m->indices[i].p;
        int texcoordIndex = m->indices[i].t;
        int normalIndex = m->indices[i].n;

        memcpy(&res_m->positions[i], &c_verts[vertIndex], sizeof(VECTOR));
        memcpy(&res_m->texcoords[i], &c_texcoords[texcoordIndex], sizeof(VECTOR));
        memcpy(&res_m->normals[i], &c_normals[normalIndex], sizeof(VECTOR));

		if(m->material_count > 0) {
			faceMaterialIndex = m->face_materials[i / 3];

			if(oldTex != m->materials[faceMaterialIndex].map_Kd.name) {
				tex_names[res_m->tex_count] = m->materials[faceMaterialIndex].map_Kd.name;
				oldTex = tex_names[res_m->tex_count];
				res_m->textures[res_m->tex_count] = malloc(sizeof(GSTEXTURE));
				load_image(res_m->textures[res_m->tex_count], tex_names[res_m->tex_count], true);
				res_m->tex_ranges[res_m->tex_count] = i;
				res_m->tex_count++;
			} else if (m->materials[faceMaterialIndex].map_Kd.name) {
				res_m->tex_ranges[res_m->tex_count-1] = i;
			}

        	c_colours[i][0] = m->materials[faceMaterialIndex].Kd[0];
        	c_colours[i][1] = m->materials[faceMaterialIndex].Kd[1];
        	c_colours[i][2] = m->materials[faceMaterialIndex].Kd[2];
        	c_colours[i][3] = 1.0f;
		} else {
			c_colours[i][0] = 1.0f;
        	c_colours[i][1] = 1.0f;
        	c_colours[i][2] = 1.0f;
        	c_colours[i][3] = 1.0f;
		}

    }

    memcpy(res_m->colours, c_colours, indexCount * sizeof(VECTOR));

	res_m->idx_range_count = 0;
	for (int i = 0; i < res_m->indexCount; i++) {
		if ((i+1) % 108 == 0) {
			res_m->idx_ranges[res_m->idx_range_count] = i;
			res_m->idx_range_count++;
			
		} else if (i == (res_m->indexCount-1)) {
			res_m->idx_ranges[res_m->idx_range_count] = i;
			res_m->idx_range_count++;
		}
	}

    free(c_verts);
    free(c_colours);
    free(c_normals);
    free(c_texcoords);

    float lowX, lowY, lowZ, hiX, hiY, hiZ;
    lowX = hiX = res_m->positions[0][0];
    lowY = hiY = res_m->positions[0][1];
    lowZ = hiZ = res_m->positions[0][2];
	
    for (int i = 0; i < res_m->facesCount; i++) {
        if (lowX > res_m->positions[i][0]) lowX = res_m->positions[i][0];
        if (hiX < res_m->positions[i][0]) hiX = res_m->positions[i][0];
        if (lowY > res_m->positions[i][1]) lowY = res_m->positions[i][1];
        if (hiY < res_m->positions[i][1]) hiY = res_m->positions[i][1];
        if (lowZ > res_m->positions[i][2]) lowZ = res_m->positions[i][2];
        if (hiZ < res_m->positions[i][2]) hiZ = res_m->positions[i][2];
    }

    res_m->bounding_box[0][0] = lowX;
    res_m->bounding_box[0][1] = lowY;
    res_m->bounding_box[0][2] = lowZ;
    res_m->bounding_box[0][3] = 1.00f;

    res_m->bounding_box[1][0] = lowX;
    res_m->bounding_box[1][1] = lowY;
    res_m->bounding_box[1][2] = hiZ;
    res_m->bounding_box[1][3] = 1.00f;

    res_m->bounding_box[2][0] = lowX;
    res_m->bounding_box[2][1] = hiY;
    res_m->bounding_box[2][2] = lowZ;
    res_m->bounding_box[2][3] = 1.00f;

    res_m->bounding_box[3][0] = lowX;
    res_m->bounding_box[3][1] = hiY;
    res_m->bounding_box[3][2] = hiZ;
    res_m->bounding_box[3][3] = 1.00f;

    res_m->bounding_box[4][0] = hiX;
    res_m->bounding_box[4][1] = lowY;
    res_m->bounding_box[4][2] = lowZ;
    res_m->bounding_box[4][3] = 1.00f;

    res_m->bounding_box[5][0] = hiX;
    res_m->bounding_box[5][1] = lowY;
    res_m->bounding_box[5][2] = hiZ;
    res_m->bounding_box[5][3] = 1.00f;

    res_m->bounding_box[6][0] = hiX;
    res_m->bounding_box[6][1] = hiY;
    res_m->bounding_box[6][2] = lowZ;
    res_m->bounding_box[6][3] = 1.00f;

    res_m->bounding_box[7][0] = hiX;
    res_m->bounding_box[7][1] = hiY;
    res_m->bounding_box[7][2] = hiZ;
    res_m->bounding_box[7][3] = 1.00f;

	if(text) {
		res_m->textures[0] = text;
		res_m->render = render_singletex;
		res_m->vertices = memalign(128, sizeof(GSPRIMSTQPOINT)*m->index_count);
	} else if (res_m->tex_count > 0) {
		res_m->render = render_multitex;
		res_m->vertices = memalign(128, sizeof(GSPRIMSTQPOINT)*m->index_count);
	} else {
		res_m->render = render_notex;
		res_m->vertices = memalign(128, sizeof(GSPRIMPOINT)*m->index_count);
	}

	res_m->tmp_normals  = (VECTOR*)memalign(128, sizeof(VECTOR)*(res_m->indexCount));
	res_m->tmp_lights  = (VECTOR*)memalign(128, sizeof(VECTOR)*(res_m->indexCount));
	res_m->tmp_colours  = (color_f_t*)memalign(128, sizeof(color_f_t)*(res_m->indexCount));
	res_m->tmp_xyz  = (vertex_f_t*)memalign(128, sizeof(vertex_f_t)*(res_m->indexCount));

    return res_m;
}


void draw_bbox(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z, Color color)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

    GSGLOBAL *gsGlobal = getGSGLOBAL();

	// Matrices to setup the 3D environment and camera
	MATRIX local_world;
	MATRIX world_view;
	MATRIX local_screen;

	create_local_world(local_world, object_position, object_rotation);
	create_world_view(world_view, camera_position, camera_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);
	if(clip_bounding_box(local_screen, m->bounding_box)) return;

	vertex_f_t *t_xyz = (vertex_f_t *)memalign(128, sizeof(vertex_f_t)*8);
	calculate_vertices_clipped((VECTOR *)t_xyz,  8, m->bounding_box, local_screen);

	xyz_t *xyz  = (xyz_t *)memalign(128, sizeof(xyz_t)*8);
	draw_convert_xyz(xyz, 2048, 2048, 16,  8, t_xyz);

	float fX=gsGlobal->Width/2;
	float fY=gsGlobal->Height/2;

	gsKit_prim_line_3d(gsGlobal, (t_xyz[0].x+1.0f)*fX, (t_xyz[0].y+1.0f)*fY, xyz[0].z, (t_xyz[1].x+1.0f)*fX, (t_xyz[1].y+1.0f)*fY, xyz[1].z, color);
	gsKit_prim_line_3d(gsGlobal, (t_xyz[1].x+1.0f)*fX, (t_xyz[1].y+1.0f)*fY, xyz[1].z, (t_xyz[3].x+1.0f)*fX, (t_xyz[3].y+1.0f)*fY, xyz[3].z, color);
	gsKit_prim_line_3d(gsGlobal, (t_xyz[2].x+1.0f)*fX, (t_xyz[2].y+1.0f)*fY, xyz[2].z, (t_xyz[3].x+1.0f)*fX, (t_xyz[3].y+1.0f)*fY, xyz[3].z, color);
	gsKit_prim_line_3d(gsGlobal, (t_xyz[0].x+1.0f)*fX, (t_xyz[0].y+1.0f)*fY, xyz[0].z, (t_xyz[2].x+1.0f)*fX, (t_xyz[2].y+1.0f)*fY, xyz[2].z, color);
	gsKit_prim_line_3d(gsGlobal, (t_xyz[4].x+1.0f)*fX, (t_xyz[4].y+1.0f)*fY, xyz[4].z, (t_xyz[5].x+1.0f)*fX, (t_xyz[5].y+1.0f)*fY, xyz[5].z, color);
	gsKit_prim_line_3d(gsGlobal, (t_xyz[5].x+1.0f)*fX, (t_xyz[5].y+1.0f)*fY, xyz[5].z, (t_xyz[7].x+1.0f)*fX, (t_xyz[7].y+1.0f)*fY, xyz[7].z, color);
	gsKit_prim_line_3d(gsGlobal, (t_xyz[6].x+1.0f)*fX, (t_xyz[6].y+1.0f)*fY, xyz[6].z, (t_xyz[7].x+1.0f)*fX, (t_xyz[7].y+1.0f)*fY, xyz[7].z, color);
	gsKit_prim_line_3d(gsGlobal, (t_xyz[4].x+1.0f)*fX, (t_xyz[4].y+1.0f)*fY, xyz[4].z, (t_xyz[6].x+1.0f)*fX, (t_xyz[6].y+1.0f)*fY, xyz[6].z, color);
	gsKit_prim_line_3d(gsGlobal, (t_xyz[0].x+1.0f)*fX, (t_xyz[0].y+1.0f)*fY, xyz[0].z, (t_xyz[4].x+1.0f)*fX, (t_xyz[4].y+1.0f)*fY, xyz[4].z, color);
	gsKit_prim_line_3d(gsGlobal, (t_xyz[1].x+1.0f)*fX, (t_xyz[1].y+1.0f)*fY, xyz[1].z, (t_xyz[5].x+1.0f)*fX, (t_xyz[5].y+1.0f)*fY, xyz[5].z, color);
	gsKit_prim_line_3d(gsGlobal, (t_xyz[2].x+1.0f)*fX, (t_xyz[2].y+1.0f)*fY, xyz[2].z, (t_xyz[6].x+1.0f)*fX, (t_xyz[6].y+1.0f)*fY, xyz[6].z, color);
	gsKit_prim_line_3d(gsGlobal, (t_xyz[3].x+1.0f)*fX, (t_xyz[3].y+1.0f)*fY, xyz[3].z, (t_xyz[7].x+1.0f)*fX, (t_xyz[7].y+1.0f)*fY, xyz[7].z, color);
	
	free(t_xyz);
	free(xyz);
}


u64* vif_packets[2] __attribute__((aligned(64)));
u64* curr_vif_packet;

/** Cube data */
u64 *cube_packet;

u8 context = 0;

VECTOR *c_verts __attribute__((aligned(128))), *c_sts __attribute__((aligned(128)));

static inline u32 lzw(u32 val)
{
	u32 res;
	__asm__ __volatile__ ("   plzcw   %0, %1    " : "=r" (res) : "r" (val));
	return(res);
}

static inline void gsKit_set_tw_th(const GSTEXTURE *Texture, int *tw, int *th)
{
	*tw = 31 - (lzw(Texture->Width) + 1);
	if(Texture->Width > (1<<*tw))
		(*tw)++;

	*th = 31 - (lzw(Texture->Height) + 1);
	if(Texture->Height > (1<<*th))
		(*th)++;
}

unsigned int get_max_z(GSGLOBAL* gsGlobal)
{

	int z;
	unsigned int max_z;

	switch(gsGlobal->PSMZ){
		case GS_PSMZ_32:
			z = 32;
			break;

		case GS_PSMZ_24:
			z = 24;
			break;

		case GS_PSMZ_16:
		case GS_PSMZ_16S:
			z = 16;
			break;
		
		default:
			return -1;
	}

	max_z = 1 << (z - 1);

	// End function.
	return max_z;

}

/** Calculate packet for cube data */
void calculate_cube(GSGLOBAL *gsGlobal, GSTEXTURE* tex, uint32_t idx_count)
{

	float fX = 2048.0f+gsGlobal->Width/2;
	float fY = 2048.0f+gsGlobal->Height/2;
	float fZ = ((float)get_max_z(gsGlobal));

	u64* p_data = cube_packet;

	*p_data++ = (*(u32*)(&fX) | (u64)*(u32*)(&fY) << 32);
	*p_data++ = (*(u32*)(&fZ) | (u64)idx_count << 32);

	*p_data++ = GIF_TAG(1, 0, 0, 0, 0, 1);
	*p_data++ = GIF_AD;

	*p_data++ = GS_SETREG_TEX1(1, 0, 0, 0, 0, 0, 0);
	*p_data++ = GS_TEX1_1;

	int tw, th;
	gsKit_set_tw_th(tex, &tw, &th);

	*p_data++ = GS_SETREG_TEX0(
            tex->Vram/256, tex->TBW, tex->PSM,
            tw, th, gsGlobal->PrimAlphaEnable, 0,
    		0, 0, 0, 0, GS_CLUT_STOREMODE_NOLOAD);
	*p_data++ = GS_TEX0_1;

	*p_data++ = VU_GS_GIFTAG(idx_count, 1, 1,
    	VU_GS_PRIM(GS_PRIM_PRIM_TRIANGLE, 1, 1, gsGlobal->PrimFogEnable, 
		0, gsGlobal->PrimAAEnable, 0, 0, 0),
        0, 3);

	*p_data++ = DRAW_STQ2_REGLIST;

	*p_data++ = (128 | (u64)128 << 32);
	*p_data++ = (128 | (u64)128 << 32);	
}

model* prepare_cube(const char* path, GSTEXTURE* Texture)
{
	model* model_test = loadOBJ(path, Texture);

	vu1_upload_micro_program(&VU1Draw3D_CodeStart, &VU1Draw3D_CodeEnd);
	vu1_set_double_buffer_settings();

	cube_packet =    vifCreatePacket(6);
	vif_packets[0] = vifCreatePacket(7);
	vif_packets[1] = vifCreatePacket(7);

	return model_test;
}

void draw_cube(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX world_view;
	MATRIX local_screen;

	GSGLOBAL *gsGlobal = getGSGLOBAL();

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_world_view(world_view, camera_position, camera_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	gsKit_TexManager_bind(gsGlobal, model_test->textures[0]);

	// res_m->idx_ranges[res_m->idx_range_count] = i;
	int last_idx = -1;
	for (int i = 0; i < model_test->idx_range_count; i++) {
		//asm(
  		//	"vu1_active:\n"
  		//	"bc2t vu1_active\n"
  		//	"nop\n"
  		//);

		printf("Hello world\n");

		calculate_cube(gsGlobal, model_test->textures[0], model_test->idx_ranges[i]-last_idx);

		curr_vif_packet = vif_packets[context];
	
		memset(curr_vif_packet, 0, 16*6);


		//if (i) {
		//	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
		//	*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32));
		//}
		
		// Add matrix at the beggining of VU mem (skip TOP)
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 0, &local_screen, 8, 0);
	
		u32 vif_added_bytes = 0; // zero because now we will use TOP register (double buffer)
								 // we don't wan't to unpack at 8 + beggining of buffer, but at
								 // the beggining of the buffer
	
		// Merge packets
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, cube_packet, 6, 1);
		vif_added_bytes += 6;
	
		// Add vertices
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &model_test->positions[last_idx+1], model_test->idx_ranges[i]-last_idx, 1);
		vif_added_bytes += model_test->idx_ranges[i]-last_idx; // one VECTOR is size of qword
	
		// Add sts
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &model_test->texcoords[last_idx+1], model_test->idx_ranges[i]-last_idx, 1);
		vif_added_bytes += model_test->idx_ranges[i]-last_idx;
	
		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
		*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, VIF_MSCAL, 0) << 32));
	
		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
		*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);
	
		vifSendPacket(vif_packets[context], 1);
		last_idx = model_test->idx_ranges[i];

	}

	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}
