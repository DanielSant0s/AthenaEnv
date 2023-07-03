#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>
#include "fast_obj/fast_obj.h"

#include "include/render.h"

MATRIX view_screen;

VECTOR camera_position = { 0.00f, 0.00f, 0.00f, 1.00f };
VECTOR camera_rotation = { 0.00f, 0.00f, 0.00f, 1.00f };

int light_count;
VECTOR* light_direction;
VECTOR* light_colour;
int* light_type;

void init3D(float aspect)
{
	create_view_screen(view_screen, aspect, -0.20f, 0.20f, -0.20f, 0.20f, 1.00f, 2000.00f);

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
	gsKit_TexManager_bind(gsGlobal, m->texture);
	gsKit_prim_list_triangle_goraud_texture_stq_3d(gsGlobal, m->texture, m->indexCount, m->vertices);
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
        c_texcoords[i][2] = 0.0f;
        c_texcoords[i][3] = 0.0f;
    }

    res_m->facesCount = m->face_count;
	res_m->indexCount = m->index_count;
    res_m->positions = (VECTOR*)malloc(indexCount * sizeof(VECTOR));
    res_m->texcoords = (VECTOR*)malloc(indexCount * sizeof(VECTOR));
    res_m->normals = (VECTOR*)malloc(indexCount * sizeof(VECTOR));
    res_m->colours = (VECTOR*)malloc(indexCount * sizeof(VECTOR));

    int faceMaterialIndex;
    for (int i = 0, j = 0; i < indexCount; i++, j += 3) {
        int vertIndex = m->indices[i].p;
        int texcoordIndex = m->indices[i].t;
        int normalIndex = m->indices[i].n;

        memcpy(&res_m->positions[i], &c_verts[vertIndex], sizeof(VECTOR));
        memcpy(&res_m->texcoords[i], &c_texcoords[texcoordIndex], sizeof(VECTOR));
        memcpy(&res_m->normals[i], &c_normals[normalIndex], sizeof(VECTOR));

		if(m->material_count > 0) {
			faceMaterialIndex = m->face_materials[i / 3];
			printf("%d\n", faceMaterialIndex);
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
		res_m->texture = text;
		res_m->render = render_singletex;
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
