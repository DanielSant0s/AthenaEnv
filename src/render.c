#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>

#include "include/render.h"

MATRIX view_screen;

VECTOR camera_position = { 0.00f, 0.00f, 0.00f, 1.00f };
VECTOR camera_rotation = { 0.00f, 0.00f, 0.00f, 1.00f };

int light_count;
VECTOR* light_direction;
VECTOR* light_colour;
int* light_type;


typedef struct {
	float x;
	float y;
	float z;
	float t1;
	float t2;
	float n1;
	float n2;
	float n3;
} vertex;

typedef struct {
	vertex v1;
	vertex v2;
	vertex v3;
	struct vertexList* next;
} vertexList;

typedef struct {
	vertex* vert;
	struct rawVertexList* next;
} rawVertexList;


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

model* loadOBJ(const char* path, GSTEXTURE* text){
    // Opening model file and loading it on RAM
	int file = open(path, O_RDONLY, 0777);
	uint32_t size = lseek(file, 0, SEEK_END);
	lseek(file, 0, SEEK_SET);
	char* content = (char*)malloc(size+1);
	read(file, content, size);
	content[size] = 0;
	
	// Closing file
	close(file);
	
	// Creating temp vertexList
	rawVertexList* vl = (rawVertexList*)malloc(sizeof(rawVertexList));
	rawVertexList* init = vl;
	
	// Init variables
	char* str = content;
	char* ptr = strstr(str,"v ");
	int idx;
	char float_val[16];
	char* init_val;
	char magics[3][3] = {"v ","vt","vn"};
	int magics_idx = 0;
	vertex* res;
	int v_idx = 0;
	bool skip = false;
	char* end_vert;
	char* end_val;
	float* vert_args;
	rawVertexList* old_vl;
	
	// Vertices extraction
	for(;;){
		
		// Check if a magic change is needed
		while (ptr == NULL){
			if (magics_idx < 2){
				res = init->vert;
				vl = init;
				magics_idx++;
				ptr = strstr(str,magics[magics_idx]);
			}else{
				skip = true;
				break;
			}
		}
		if (skip) break;
		
		// Extract vertex
		if (magics_idx == 0) idx = 0;
		else if (magics_idx == 1) idx = 3;
		else idx = 5;
		if (magics_idx == 0) init_val = ptr + 2;
		else init_val = ptr + 3;
		while (init_val[0] == ' ') init_val++;
		end_vert = strstr(init_val,"\n");
		if (magics_idx == 0) res = (vertex*)malloc(sizeof(vertex));
		end_val = strstr(init_val," ");
		vert_args = (float*)res; // Hacky way to iterate in vertex struct		
		while (init_val < end_vert){
			if (end_val > end_vert) end_val = end_vert;
			strncpy(float_val, init_val, end_val - init_val);
			float_val[end_val - init_val] = 0;
			vert_args[idx] = atof(float_val);
			idx++;
			init_val = end_val + 1;
			while (init_val[0] == ' ') init_val++;
			end_val = strstr(init_val," ");
		}
		
		// Update rawVertexList struct
		if (magics_idx == 0){
			vl->vert = res;
			vl->next = (rawVertexList*)malloc(sizeof(rawVertexList));
		}
		old_vl = vl;
		vl = vl->next;
		if (magics_idx == 0){
			vl->vert = NULL;
			vl->next = NULL;
		}else{
			if (vl == NULL){
				old_vl->next = (rawVertexList*)malloc(sizeof(rawVertexList));
				vl = old_vl->next;
				vl->vert = (vertex*)malloc(sizeof(vertex));
				vl->next = NULL;
			}else if(vl->vert == NULL) vl->vert = (vertex*)malloc(sizeof(vertex));
			res = vl->vert;
		}
		
		// Searching for next vertex
		str = ptr + 1;
		ptr = strstr(str,magics[magics_idx]);
		
	}

	// Creating real RAW vertexList
	ptr = strstr(str, "f ");
	rawVertexList* faces = (rawVertexList*)malloc(sizeof(rawVertexList));
	rawVertexList* initFaces = faces;
	faces->vert = NULL;
	faces->next = NULL;
	int len = 0;
	char val[8];
	int f_idx;
	char* ptr2;
	int t_idx;
	rawVertexList* tmp;
	
	// Faces extraction
	while (ptr != NULL){
		
		// Skipping padding
		ptr+=2;		
		
		// Extracting face info
		f_idx = 0;
		while (f_idx < 3){
		
			// Allocating new vertex
			faces->vert = (vertex*)malloc(sizeof(vertex));
		
			// Extracting x,y,z
			ptr2 = strstr(ptr,"/");
			strncpy(val,ptr,ptr2-ptr);
			val[ptr2-ptr] = 0;
			v_idx = atoi(val);
			t_idx = 1;
			tmp = init;
			while (t_idx < v_idx){
				tmp = tmp->next;
				t_idx++;
			}
			faces->vert->x = tmp->vert->x;
			faces->vert->y = tmp->vert->y;
			faces->vert->z = tmp->vert->z;
			
			// Extracting texture info
			ptr = ptr2+1;
			ptr2 = strstr(ptr,"/");
			if (ptr2 != ptr){
				strncpy(val,ptr,ptr2-ptr);
				val[ptr2-ptr] = 0;
				v_idx = atoi(val);
				t_idx = 1;
				tmp = init;
				while (t_idx < v_idx){
					tmp = tmp->next;
					t_idx++;
				}
				faces->vert->t1 = tmp->vert->t1;
				faces->vert->t2 = 1.0f - tmp->vert->t2;
			}else{
				faces->vert->t1 = 0.0f;
				faces->vert->t2 = 0.0f;
			}
			
			// Extracting normals info
			ptr = ptr2+1;
			if (f_idx < 2) ptr2 = strstr(ptr," ");
			else{
				ptr2 = strstr(ptr,"\n");
				if (ptr2 == NULL) ptr2 = content + size;
			}
			strncpy(val,ptr,ptr2-ptr);
			val[ptr2-ptr] = 0;
			v_idx = atoi(val);
			t_idx = 1;
			tmp = init;
			while (t_idx < v_idx){
				tmp = tmp->next;
				t_idx++;
			}
			faces->vert->n1 = tmp->vert->n1;
			faces->vert->n2 = tmp->vert->n2;
			faces->vert->n3 = tmp->vert->n3;

			// Setting values for next vertex
			ptr = ptr2;
			faces->next = (rawVertexList*)malloc(sizeof(rawVertexList));
			faces = faces->next;
			faces->next = NULL;
			faces->vert = NULL;
			len++;
			f_idx++;
		}
		
		ptr = strstr(ptr,"f ");
		
	}
	
	// Freeing temp vertexList and allocated file
	free(content);
	rawVertexList* tmp_init;
	while (init != NULL){
		tmp_init = init;
		free(init->vert);
		init = init->next;
		free(tmp_init);
	}
	
	// Create the model struct and populating vertex list
	model* res_m = (model*)malloc(sizeof(model));
	vertexList* vlist = (vertexList*)malloc(sizeof(vertexList));
	vertexList* vlist_start = vlist;
	vlist->next = NULL;
	bool first = true;
	for(int i = 0; i < len; i+=3) {
		if (first) first = false;
		else{
			vlist->next = (vertexList*)malloc(sizeof(vertexList));
			vlist = vlist->next;
			vlist->next = NULL;
		}
		tmp_init = initFaces;
		memcpy(&vlist->v1,initFaces->vert,sizeof(vertex));
		initFaces = initFaces->next;
		free(tmp_init->vert);
		free(tmp_init);
		tmp_init = initFaces;
		memcpy(&vlist->v2,initFaces->vert,sizeof(vertex));
		initFaces = initFaces->next;
		free(tmp_init->vert);
		free(tmp_init);
		tmp_init = initFaces;
		memcpy(&vlist->v3,initFaces->vert,sizeof(vertex));
		initFaces = initFaces->next;
		free(tmp_init->vert);
		free(tmp_init);
	}
	res_m->facesCount = len / 3;
  
	
	// Setting texture
	res_m->texture = text;

	vlist = vlist_start;
	res_m->positions = (VECTOR*)memalign(128, res_m->facesCount * 3 * sizeof(VECTOR));
    res_m->texcoords = (VECTOR*)memalign(128, res_m->facesCount * 3 * sizeof(VECTOR));
    res_m->normals =  (VECTOR*)memalign(128, res_m->facesCount * 3 * sizeof(VECTOR));
	res_m->colours =  (VECTOR*)memalign(128, res_m->facesCount * 3 * sizeof(VECTOR));
	res_m->idxList = (uint16_t*)memalign(128, res_m->facesCount * 3 * sizeof(uint16_t));
	vertexList* object = vlist;
	int n = 0;
	while (object != NULL){

        //v1
        res_m->positions[n][0] = object->v1.x;
		res_m->positions[n][1] = object->v1.y;
		res_m->positions[n][2] = object->v1.z;
		res_m->positions[n][3] = 1.000f;

        res_m->normals[n][0] = object->v1.n1;
		res_m->normals[n][1] = object->v1.n2;
		res_m->normals[n][2] = object->v1.n3;
		res_m->normals[n][3] = 1.000f;

        res_m->texcoords[n][0] = object->v1.t1;
		res_m->texcoords[n][1] = object->v1.t2;
		res_m->texcoords[n][2] = 0.000f;
		res_m->texcoords[n][3] = 0.000f;

		res_m->colours[n][0] = 1.000f;
		res_m->colours[n][1] = 1.000f;
		res_m->colours[n][2] = 1.000f;
		res_m->colours[n][3] = 1.000f;

        //v2
        res_m->positions[n+1][0] = object->v2.x;
		res_m->positions[n+1][1] = object->v2.y;
		res_m->positions[n+1][2] = object->v2.z;
		res_m->positions[n+1][3] = 1.000f;

        res_m->normals[n+1][0] = object->v2.n1;
		res_m->normals[n+1][1] = object->v2.n2;
		res_m->normals[n+1][2] = object->v2.n3;
		res_m->normals[n+1][3] = 1.000f;

        res_m->texcoords[n+1][0] = object->v2.t1;
		res_m->texcoords[n+1][1] = object->v2.t2;
		res_m->texcoords[n+1][2] = 0.000f;
		res_m->texcoords[n+1][3] = 0.000f;
	
		res_m->colours[n+1][0] = 1.000f;
		res_m->colours[n+1][1] = 1.000f;
		res_m->colours[n+1][2] = 1.000f;
		res_m->colours[n+1][3] = 1.000f;

        //v3
        res_m->positions[n+2][0] = object->v3.x;
		res_m->positions[n+2][1] = object->v3.y;
		res_m->positions[n+2][2] = object->v3.z;
		res_m->positions[n+2][3] = 1.000f;

        res_m->normals[n+2][0] = object->v3.n1;
		res_m->normals[n+2][1] = object->v3.n2;
		res_m->normals[n+2][2] = object->v3.n3;
		res_m->normals[n+2][3] = 1.000f;

        res_m->texcoords[n+2][0] = object->v3.t1;
		res_m->texcoords[n+2][1] = object->v3.t2;
		res_m->texcoords[n+2][2] = 0.000f;
		res_m->texcoords[n+2][3] = 0.000f;

		res_m->colours[n+2][0] = 1.000f;
		res_m->colours[n+2][1] = 1.000f;
		res_m->colours[n+2][2] = 1.000f;
		res_m->colours[n+2][3] = 1.000f;

		res_m->idxList[n] = n;
		res_m->idxList[n+1] = n+1;
		res_m->idxList[n+2] = n+2;
		object = object->next;
		n += 3;
	}

	while (vlist != NULL){
		vertexList* old = vlist;
		vlist = vlist->next;
		free(old);
	}

	VECTOR* c_verts = (VECTOR*)memalign(128, res_m->facesCount * 3 * sizeof(VECTOR));
    VECTOR* c_texcoords = (VECTOR*)memalign(128, res_m->facesCount * 3 * sizeof(VECTOR));
    VECTOR* c_normals =  (VECTOR*)memalign(128, res_m->facesCount * 3 * sizeof(VECTOR));

	for (int i = 0; i < res_m->facesCount*3; i++)
	{
		c_verts[i][0] = res_m->positions[res_m->idxList[i]][0];
		c_verts[i][1] = res_m->positions[res_m->idxList[i]][1];
		c_verts[i][2] = res_m->positions[res_m->idxList[i]][2];
		c_verts[i][3] = 1.0f;

		c_normals[i][0] = res_m->normals[res_m->idxList[i]][0];
		c_normals[i][1] = res_m->normals[res_m->idxList[i]][1];
		c_normals[i][2] = res_m->normals[res_m->idxList[i]][2];
		c_normals[i][3] = 1.0f;

		c_texcoords[i][0] = res_m->texcoords[res_m->idxList[i]][0];
		c_texcoords[i][1] = res_m->texcoords[res_m->idxList[i]][1];
		c_texcoords[i][2] = 0.0f;
		c_texcoords[i][3] = 0.0f;
	}
	
	//calculate bounding box
	float lowX, lowY, lowZ, hiX, hiY, hiZ;
    lowX = hiX = res_m->positions[res_m->idxList[0]][0];
    lowY = hiY = res_m->positions[res_m->idxList[0]][1];
    lowZ = hiZ = res_m->positions[res_m->idxList[0]][2];
    for (int i = 0; i < res_m->facesCount; i++)
    {
        if (lowX > res_m->positions[res_m->idxList[i]][0]) lowX = res_m->positions[res_m->idxList[i]][0];
        if (hiX < res_m->positions[res_m->idxList[i]][0]) hiX = res_m->positions[res_m->idxList[i]][0];
        if (lowY > res_m->positions[res_m->idxList[i]][1]) lowY = res_m->positions[res_m->idxList[i]][1];
        if (hiY < res_m->positions[res_m->idxList[i]][1]) hiY = res_m->positions[res_m->idxList[i]][1];
        if (lowZ > res_m->positions[res_m->idxList[i]][2]) lowZ = res_m->positions[res_m->idxList[i]][2];
        if (hiZ < res_m->positions[res_m->idxList[i]][2]) hiZ = res_m->positions[res_m->idxList[i]][2];
    }

	res_m->bounding_box = (VECTOR*)malloc(sizeof(VECTOR)*8);

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

	free(res_m->positions);
	free(res_m->normals);
	free(res_m->texcoords);

	res_m->positions = c_verts;
	res_m->normals = c_normals;
	res_m->texcoords = c_texcoords;

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
	calculate_vertices_no_clip((VECTOR *)t_xyz,  8, m->bounding_box, local_screen);

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


int athena_process_xyz_rgbaq_st(ATHTEXTRI *output, GSGLOBAL* gsGlobal, int count, color_f_t *colours, vertex_f_t *vertices, texel_f_t *coords)
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

		output[i].st.stq.s = coords[i].s * q;
		output[i].st.stq.t = coords[i].t * q;
		output[i].st.tag = GS_ST;

		output[i].xyz2.xyz.x = gsKit_float_to_int_x(gsGlobal, (vertices[i].x + 1.0f) * center_x);
		output[i].xyz2.xyz.y = gsKit_float_to_int_y(gsGlobal, (vertices[i].y + 1.0f) * center_y);
		output[i].xyz2.xyz.z = (unsigned int)((vertices[i].z + 1.0f) * max_z);
		output[i].xyz2.tag = GS_XYZ2;

	}

	// End function.
	return 0;

}

void drawOBJ(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
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

	// Allocate calculation space.
	VECTOR *t_normals  = (VECTOR*)memalign(128, sizeof(VECTOR)*(m->facesCount*3));
	VECTOR *t_lights   = (VECTOR*)memalign(128, sizeof(VECTOR)*(m->facesCount*3));
	color_f_t *t_colours  = (color_f_t*)memalign(128, sizeof(color_f_t)*(m->facesCount*3));
	vertex_f_t *t_xyz = (vertex_f_t*)memalign(128, sizeof(vertex_f_t)*(m->facesCount*3));

	// Calculate the normal values.
	calculate_normals(t_normals, m->facesCount*3, m->normals, local_light);
	calculate_lights(t_lights, m->facesCount*3, t_normals, light_direction, light_colour, light_type, light_count);
	calculate_colours((VECTOR *)t_colours, m->facesCount, m->colours, t_lights);
	calculate_vertices_clipped((VECTOR *)t_xyz, m->facesCount*3, m->positions, local_screen);

	if (m->texture != NULL) {
		ATHTEXTRI* gs_vertices = (ATHTEXTRI*)memalign(128, sizeof(ATHTEXTRI)*m->facesCount*3);

		athena_process_xyz_rgbaq_st(gs_vertices, gsGlobal, m->facesCount*3, (color_f_t*)t_lights, t_xyz, (texel_f_t *)m->texcoords);

		gsKit_TexManager_bind(gsGlobal, m->texture);
		gsKit_prim_list_triangle_goraud_texture_3d_st(gsGlobal, m->texture, m->facesCount*3, gs_vertices);

		free(gs_vertices);
	} else {
		GSPRIMPOINT* gs_vertices = (GSPRIMPOINT*)memalign(128, sizeof(GSPRIMPOINT)*m->facesCount*3);

		athena_process_xyz_rgbaq(gs_vertices, gsGlobal, m->facesCount*3, (color_f_t*)t_lights, t_xyz);

		gsKit_prim_list_triangle_gouraud_3d(gsGlobal, m->facesCount*3, gs_vertices);

		free(gs_vertices);
	}

	
	free(t_normals); free(t_lights); free(t_colours); free(t_xyz);

}

