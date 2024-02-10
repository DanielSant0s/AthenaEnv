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

extern u32 VU1Draw3DLights_CodeStart __attribute__((section(".vudata")));
extern u32 VU1Draw3DLights_CodeEnd __attribute__((section(".vudata")));

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

	cube_packet =    vifCreatePacket(6);
	vif_packets[0] = vifCreatePacket(9);
	vif_packets[1] = vifCreatePacket(9);

	return model_test;
}

static u32* last_mpg = NULL;

void draw_cube(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX world_view;
	MATRIX local_screen;

	GSGLOBAL *gsGlobal = getGSGLOBAL();

	if (last_mpg != &VU1Draw3D_CodeStart) {
		vu1_upload_micro_program(&VU1Draw3D_CodeStart, &VU1Draw3D_CodeEnd);
		last_mpg = &VU1Draw3D_CodeStart;
	}

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_world_view(world_view, camera_position, camera_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	gsKit_TexManager_bind(gsGlobal, model_test->textures[0]);

	// res_m->idx_ranges[res_m->idx_range_count] = i;
	int last_idx = -1;
	for (int i = 0; i < model_test->idx_range_count; i++) {
		dmaKit_wait(DMA_CHANNEL_VIF1, 0);

		calculate_cube(gsGlobal, model_test->textures[0], model_test->idx_ranges[i]-last_idx);

		curr_vif_packet = vif_packets[context];
	
		memset(curr_vif_packet, 0, 16*6);

		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
		*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32));
		
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
		*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, VIF_MSCALF, 0) << 32));
	
		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
		*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);
		
		asm volatile("nop":::"memory");

		vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);
		last_idx = model_test->idx_ranges[i];


	}

	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}
/*
typedef struct {
	VECTOR direction[3];
	int intensity[3];
	int ambient;

} LightData;

void SetRow(MATRIX output, int row, VECTOR vec) {
    for (int i = 0; i < 4; i++) {
        output[row * 4 + i] = vec[i];
    }
}

void SetColumn(MATRIX output, int col, VECTOR vec) {
    for (int i = 0; i < 4; i++) {
        output[i * 4 + col] = vec[i];
    }
}

void InitLightMatrix(MATRIX output, MATRIX local_light, LightData lights, MATRIX objecttoworld)
{
	matrix_unit(local_light);
 	SetRow(local_light, 0, lights.direction[0]);
 	SetRow(local_light, 1, lights.direction[1]);
 	SetRow(local_light, 2, lights.direction[2]);

	matrix_multiply(output, local_light, objecttoworld);

	//SetColumn(output, 3, Vec4(lights.intensity[0], lights.intensity[1], lights.intensity[2], lights.ambient));
}*/

float vu0_innerproduct(VECTOR v0, VECTOR v1)
{
    float ret;

    __asm__ __volatile__(
		"lqc2     $vf4, 0(%1)\n"
		"lqc2     $vf5, 0(%2)\n"
		"vmul.xyz $vf5, $vf4,  $vf5\n"
		"vaddy.x  $vf5, $vf5,  $vf5\n"
		"vaddz.x  $vf5, $vf5,  $vf5\n"
		"qmfc2    $2,   $vf5\n"
		"mtc1     $2,   %0\n"
	: "=f" (ret) :"r" (v0) ,"r" (v1) :"$2", "memory" );

    return ret;
}


void vu0_calculate_lights(VECTOR *output, int count, VECTOR *normals, VECTOR *light_direction, VECTOR *light_colour, const int *light_type, int light_count) {	
	int loop0, loop1; float intensity;
	memset(output, 0, sizeof(VECTOR) * count);
	for (loop0=0;loop0<count;loop0++) {
		for (loop1=0;loop1<light_count;loop1++) {
   			if (light_type[loop1] == LIGHT_AMBIENT)  {
    			intensity = 1.00f;
   			} else if (light_type[loop1] == LIGHT_DIRECTIONAL)  {
    			intensity = -vu0_innerproduct(normals[loop0], light_direction[loop1]);
    			// Clamp the minimum intensity.
    			if (intensity < 0.00f) { intensity = 0.00f; }
   				// Else, this is an invalid light type.
   			} else { intensity = 0.00f; }
   				// If the light has intensity...
   			if (intensity > 0.00f) {
    			// Add the light value.
    			output[loop0][0] += (light_colour[loop1][0] * intensity);
    			output[loop0][1] += (light_colour[loop1][1] * intensity);
    			output[loop0][2] += (light_colour[loop1][2] * intensity);
    			output[loop0][3] = 1.00f;
   			}
  		}
 	}
}

void vu0_vector_clamp(VECTOR v0, VECTOR v1, float min, float max)
{
    __asm__ __volatile__(
        "mfc1         $8,    %2\n"
        "mfc1         $9,    %3\n"
		"lqc2         $vf6,  0(%1)\n"
        "qmtc2        $8,    $vf4\n"
        "qmtc2        $9,    $vf5\n"
		"vmaxx.xyzw   $vf6, $vf6, $vf4\n"
		"vminix.xyzw  $vf6, $vf6, $vf5\n"
		"sqc2         $vf6,  0(%0)\n"
	: : "r" (v0) , "r" (v1), "f" (min), "f" (max):"$8","$9","memory");
}

void vu0_calculate_colours(VECTOR *output, int count, VECTOR *colours, VECTOR *lights) {
  	int loop0;

  	// For each colour...
  	for (loop0=0;loop0<count;loop0++) {
   		// Apply the light value to the colour.
		__asm__ __volatile__(
			"lqc2     $vf4, 0(%1)\n"
			"lqc2     $vf5, 0(%2)\n"
			"vmul.xyz $vf6, $vf4,  $vf5\n"
			"sqc2     $vf6, 0(%0)\n"
		: :"r" (output[loop0]) , "r" (colours[loop0]) ,"r" (lights[loop0]) : "memory" );

   		// Clamp the colour value.
   		vu0_vector_clamp(output[loop0], output[loop0], 0.00f, 1.99f);
	}

}

void draw_vu1_with_lights(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_light;
	MATRIX world_view;
	MATRIX local_screen;

	GSGLOBAL *gsGlobal = getGSGLOBAL();

	if (last_mpg != &VU1Draw3DLights_CodeStart) {
		vu1_upload_micro_program(&VU1Draw3DLights_CodeStart, &VU1Draw3DLights_CodeEnd);
		last_mpg = &VU1Draw3DLights_CodeStart;
	}

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_light(local_light, object_rotation);
	create_world_view(world_view, camera_position, camera_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	// Calculate the normal values.
	calculate_normals(model_test->tmp_normals, model_test->indexCount, model_test->normals, local_light);
	vu0_calculate_lights(model_test->tmp_lights, model_test->indexCount, model_test->tmp_normals, light_direction, light_colour, light_type, light_count);
	vu0_calculate_colours((VECTOR *)model_test->tmp_colours, model_test->indexCount, model_test->colours, model_test->tmp_lights);

	gsKit_TexManager_bind(gsGlobal, model_test->textures[0]);

	// res_m->idx_ranges[res_m->idx_range_count] = i;
	int last_idx = -1;
	for (int i = 0; i < model_test->idx_range_count; i++) {
		dmaKit_wait(DMA_CHANNEL_VIF1, 0);

		float fX = 2048.0f+gsGlobal->Width/2;
		float fY = 2048.0f+gsGlobal->Height/2;
		float fZ = ((float)get_max_z(gsGlobal));

		float texCol = 128.0f;

		u64* p_data = cube_packet;

		*p_data++ = (*(u32*)(&fX) | (u64)*(u32*)(&fY) << 32);
		*p_data++ = (*(u32*)(&fZ) | (u64)(model_test->idx_ranges[i]-last_idx) << 32);

		*p_data++ = GIF_TAG(1, 0, 0, 0, 0, 1);
		*p_data++ = GIF_AD;

		*p_data++ = GS_SETREG_TEX1(1, 0, 0, 0, 0, 0, 0);
		*p_data++ = GS_TEX1_1;

		int tw, th;
		gsKit_set_tw_th(model_test->textures[0], &tw, &th);

		*p_data++ = GS_SETREG_TEX0(
    	        model_test->textures[0]->Vram/256, model_test->textures[0]->TBW, model_test->textures[0]->PSM,
    	        tw, th, gsGlobal->PrimAlphaEnable, 0,
    			0, 0, 0, 0, GS_CLUT_STOREMODE_NOLOAD);
		*p_data++ = GS_TEX0_1;

		*p_data++ = VU_GS_GIFTAG(model_test->idx_ranges[i]-last_idx, 1, 1,
    		VU_GS_PRIM(GS_PRIM_PRIM_TRIANGLE, 1, 1, gsGlobal->PrimFogEnable, 
			0, gsGlobal->PrimAAEnable, 0, 0, 0),
    	    0, 3);

		*p_data++ = DRAW_STQ2_REGLIST;

		*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);
		*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);	

		curr_vif_packet = vif_packets[context];
	
		memset(curr_vif_packet, 0, 16*6);

		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
		*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32));
		
		// Add matrix at the beggining of VU mem (skip TOP)
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 0, &local_screen, 4, 0);
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 4, &local_light, 4, 0);
	
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
	
		// Add normals
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &model_test->tmp_lights[last_idx+1], model_test->idx_ranges[i]-last_idx, 1);
		vif_added_bytes += model_test->idx_ranges[i]-last_idx;

		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
		*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, VIF_MSCALF, 0) << 32));
	
		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
		*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);
		
		asm volatile("nop":::"memory");

		vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);
		last_idx = model_test->idx_ranges[i];


	}

	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}