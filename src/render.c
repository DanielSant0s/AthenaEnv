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
register_vu_program(VU1Draw3DColors);
register_vu_program(VU1Draw3DColorsNoTex);
register_vu_program(VU1Draw3DLightsColors);
register_vu_program(VU1Draw3DLightsColorsNoTex);
register_vu_program(VU1Draw3DSpec);
register_vu_program(VU1Draw3DSpecNoTex);

MATRIX view_screen;
MATRIX world_view;

VECTOR camera_position = { 0.00f, 0.00f, 0.00f, 0.00f };
VECTOR camera_target =   { 0.00f, 0.00f, 0.00f, 0.00f };
VECTOR camera_yd =       { 0.00f, 1.00f, 0.00f, 0.00f };
VECTOR camera_zd =       { 0.00f, 0.00f, 1.00f, 0.00f };
VECTOR camera_up =       { 0.00f, 1.00f, 0.00f, 0.00f };
VECTOR camera_rotation = { 0.00f, 0.00f, 0.00f, 0.00f };
VECTOR local_up =        { 0.00f, 1.00f, 0.00f, 1.00f };

eCameraTypes camera_type = CAMERA_DEFAULT;

void init3D(float aspect, float fov, float near, float far)
{
	create_view_screen(view_screen, aspect, -fov, fov, -fov, fov, near, far);
	vu1_set_double_buffer_settings(26, 496);

}

void setCameraType(eCameraTypes type) {
	camera_type = type;
}

void cameraUpdate() {
	switch (camera_type) {
		case CAMERA_DEFAULT:
			RotCameraMatrix(world_view, camera_position, camera_zd, camera_yd, camera_rotation);
			break;
		case CAMERA_LOOKAT:
			LookAtCameraMatrix(world_view, camera_position, camera_target, camera_up);
			break;
	}
	dbgprintf("Camera matrix:\n");
	dbgprintf("%f %f %f %f\n", world_view[0], world_view[1], world_view[2], world_view[3]);
	dbgprintf("%f %f %f %f\n", world_view[4], world_view[5], world_view[6], world_view[7]);
	dbgprintf("%f %f %f %f\n", world_view[8], world_view[9], world_view[10], world_view[11]);
	dbgprintf("%f %f %f %f\n", world_view[12], world_view[13], world_view[14], world_view[15]);
}

void setCameraPosition(float x, float y, float z){
	camera_position[0] = x;
	camera_position[1] = y;
	camera_position[2] = z;
	camera_position[3] = 0.00f;
}

void setCameraTarget(float x, float y, float z){
	VECTOR tmp_target = { x, y, z, 0.0f };

	SubVector(camera_target, camera_target, tmp_target);
	SubVector(camera_position, camera_position, camera_target);

	camera_target[0] = x;
	camera_target[1] = y;
	camera_target[2] = z;
	camera_target[3] = 0.00f;
}

void setCameraRotation(float x, float y, float z){
	camera_rotation[0] = x;
	camera_rotation[1] = y;
	camera_rotation[2] = z;
	camera_rotation[3] = 1.00f;
}

typedef struct {
    float w, x, y, z;
} Quat;

static Quat Quat_rotation(float rotation, VECTOR axis) {
    Quat result;

	Normalize(axis, axis);

    float halfAngle = rotation * 0.5f;
    float sinHalfAngle = sinf(halfAngle);

    result.w = cosf(halfAngle);
    result.x = axis[0] * sinHalfAngle;
    result.y = axis[1] * sinHalfAngle;
    result.z = axis[2] * sinHalfAngle;

    return result;
}

static void rotate(VECTOR output, VECTOR input, Quat r) {
    Quat v = {0.0f, input[0], input[1], input[2]};

    Quat conjR = {r.w, -r.x, -r.y, -r.z};

    Quat rotatedV = {
        v.w * conjR.w - v.x * conjR.x - v.y * conjR.y - v.z * conjR.z,
        v.w * conjR.x + v.x * conjR.w + v.y * conjR.z - v.z * conjR.y,
        v.w * conjR.y - v.x * conjR.z + v.y * conjR.w + v.z * conjR.x,
        v.w * conjR.z + v.x * conjR.y - v.y * conjR.x + v.z * conjR.w
    };

    output[0] = rotatedV.x;
	output[1] = rotatedV.y;
	output[2] = rotatedV.z;
}

void turnCamera(float yaw, float pitch)
{
	VECTOR dir;
	SubVector(dir, camera_target, camera_position);

	VECTOR dy = {0.0f, 1.0f, 0.0f, 1.0f};

	Quat r = Quat_rotation(yaw, dy);
	rotate(dir, dir, r);
	rotate(local_up, local_up, r);

	VECTOR right;
	OuterProduct(right, dir, local_up);
	Normalize(right, right);

	r = Quat_rotation(pitch, right);
	rotate(dir, dir, r);

	OuterProduct(local_up, right, dir);
	Normalize(local_up, local_up);

	if(local_up[1] >= 0.0f) {
		camera_up[1] = 1.0f; 
	} else {
		camera_up[1] = -1.0f;
	}

	AddVector(camera_target, camera_position, dir);
}

void orbitCamera(float yaw, float pitch)
{
	VECTOR dir;
	SubVector(dir, camera_target, camera_position);

	VECTOR dy = {0.0f, 1.0f, 0.0f, 1.0f};

	Quat r = Quat_rotation(yaw, dy);
	rotate(dir, dir, r);
	rotate(local_up, local_up, r);

	VECTOR right;
	OuterProduct(right, dir, local_up);
	Normalize(right, right);

	r = Quat_rotation(-pitch, right);
	rotate(dir, dir, r);

	OuterProduct(local_up, right, dir);
	Normalize(local_up, local_up);

	if(local_up[1] >= 0.0f) {
		camera_up[1] = 1.0f; 
	} else {
		camera_up[1] = -1.0f;
	}

	SubVector(camera_position, camera_target, dir);
}

void dollyCamera(float dist)
{
	VECTOR dir;
	SubVector(dir, camera_target, camera_position);
	SetLenVector(dir, dist);

	AddVector(camera_position, camera_position, dir);
	AddVector(camera_target, camera_target, dir);
}

void zoomCamera(float dist)
{
	VECTOR dir;
	SubVector(dir, camera_target, camera_position);
	float curdist = LenVector(dir);

	if(dist >= curdist)
		dist = curdist - 0.01f;

	SetLenVector(dir, dist);

	AddVector(camera_position, camera_position, dir);
}

void panCamera(float x, float y)
{
	VECTOR dir;
	SubVector(dir, camera_target, camera_position);
	Normalize(dir, dir);

	VECTOR right;
	OuterProduct(right, dir, camera_up);
	Normalize(right, right);
	
	OuterProduct(local_up, right, dir);
	Normalize(local_up, local_up);

	VECTOR work0, work1;
	ScaleVector(work0, right, x);
	ScaleVector(work1, local_up, y);

	AddVector(dir, work0, work1);

	AddVector(camera_position, camera_position, dir);
	AddVector(camera_target, camera_target, dir);
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

void draw_vu1_notex(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);
void draw_vu1(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

void draw_vu1_with_colors_notex(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);
void draw_vu1_with_colors(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

void draw_vu1_with_lights_notex(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);
void draw_vu1_with_lights(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

void draw_vu1_with_spec_lights_notex(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);
void draw_vu1_with_spec_lights(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

int athena_render_set_pipeline(model* m, int pl_id) {
	switch (pl_id) {
		case PL_NO_LIGHTS_COLORS:
			if (m->tex_count) {
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
		case PL_NO_LIGHTS:
			if (m->tex_count) {
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
			if (m->tex_count) {
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
			if (m->tex_count) {
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
	
    for (int i = 0; i < res_m->indexCount; i++) {
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

#define alloc_vectors(cnt) (VECTOR*)malloc(cnt * sizeof(VECTOR))
#define copy_vectors(dst, src, cnt) memcpy(dst, src, cnt*sizeof(VECTOR))
#define copy_vector(dst, src) memcpy(dst, src, sizeof(VECTOR))
#define free_vectors(vec) free(vec)

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

    res_m->facesCount = m->face_count;

    int face_mat_index;
	char* old_tex = NULL;
	char* cur_tex = NULL;

	res_m->tex_count = 0;
	res_m->textures = NULL;
	res_m->tex_ranges = NULL;

	res_m->tristrip = m->strip_count > 0;

	if (m->strip_count > 0) {
		res_m->indexCount = m->index_count + (2 * m->strip_count) + (((m->index_count + (2 * m->strip_count)) / (BATCH_SIZE - 2)) * 2) + 1;

		res_m->positions = alloc_vectors(res_m->indexCount);
    	res_m->texcoords = alloc_vectors(res_m->indexCount);
    	res_m->normals =   alloc_vectors(res_m->indexCount);
    	res_m->colours =   alloc_vectors(res_m->indexCount);

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

        	if (m->material_count > 0) {
        	    face_mat_index = m->face_materials[unified_strip_indices[i] / 3];
        	    res_m->colours[index][0] = m->materials[face_mat_index].Kd[0];
        	    res_m->colours[index][1] = m->materials[face_mat_index].Kd[1];
        	    res_m->colours[index][2] = m->materials[face_mat_index].Kd[2];
        	    res_m->colours[index][3] = 1.0f;
        	} else {
        	    res_m->colours[index][0] = 1.0f;
        	    res_m->colours[index][1] = 1.0f;
        	    res_m->colours[index][2] = 1.0f;
        	    res_m->colours[index][3] = 1.0f;
        	}

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

		res_m->indexCount = index;
	} else {
		res_m->indexCount = m->index_count;

    	res_m->positions = alloc_vectors(res_m->indexCount);
    	res_m->texcoords = alloc_vectors(res_m->indexCount);
    	res_m->normals =   alloc_vectors(res_m->indexCount);
    	res_m->colours =   alloc_vectors(res_m->indexCount);

		for (int i = 0; i < res_m->indexCount; i++) {
			obj_transfer_vertex(res_m, i, m, i);

			if(m->material_count > 0) {
				face_mat_index = m->face_materials[i / 3];

				if(old_tex != m->materials[face_mat_index].map_Kd.name) {

					cur_tex = m->materials[face_mat_index].map_Kd.name;
					old_tex = cur_tex;

					res_m->textures =   realloc(res_m->textures,   sizeof(GSTEXTURE*)*(res_m->tex_count+1));
					res_m->tex_ranges = realloc(res_m->tex_ranges, sizeof(int)*(res_m->tex_count+1));

					res_m->textures[res_m->tex_count] = malloc(sizeof(GSTEXTURE));

					load_image(res_m->textures[res_m->tex_count], cur_tex, true);

					res_m->tex_ranges[res_m->tex_count] = i;
					res_m->tex_count++;

				} else if (m->materials[face_mat_index].map_Kd.name) {
					res_m->tex_ranges[res_m->tex_count-1] = i;
				}

    	    	res_m->colours[i][0] = m->materials[face_mat_index].Kd[0];
    	    	res_m->colours[i][1] = m->materials[face_mat_index].Kd[1];
    	    	res_m->colours[i][2] = m->materials[face_mat_index].Kd[2];
    	    	res_m->colours[i][3] = 1.0f;
			} else {
				res_m->colours[i][0] = 1.0f;
    	    	res_m->colours[i][1] = 1.0f;
    	    	res_m->colours[i][2] = 1.0f;
    	    	res_m->colours[i][3] = 1.0f;
			}

    	}
	}

    calculate_bbox(res_m);

	if(text) {
		res_m->textures = malloc(sizeof(GSTEXTURE*));
		res_m->tex_ranges = malloc(sizeof(int));
		res_m->tex_count = 1;

		res_m->textures[0] = text;
		res_m->tex_ranges[0] = res_m->indexCount;
	}

	athena_render_set_pipeline(res_m, PL_DEFAULT);

	fast_obj_destroy(m);
}

void draw_bbox(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z, Color color)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

    GSGLOBAL *gsGlobal = getGSGLOBAL();

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

u8 context = 0;

static u32* last_mpg = NULL;

void draw_vu1(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_screen;

	GSGLOBAL *gsGlobal = getGSGLOBAL();

	if (last_mpg != &VU1Draw3D_CodeStart) {
		dmaKit_wait(DMA_CHANNEL_VIF1, 0);
		vu1_upload_micro_program(&VU1Draw3D_CodeStart, &VU1Draw3D_CodeEnd);
		last_mpg = &VU1Draw3D_CodeStart;
	}

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	curr_vif_packet = vif_packets[context];

	//memset(curr_vif_packet, 0, 16*22);

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
	*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32));

	// Add matrix at the beggining of VU mem (skip TOP)
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 0, &local_screen, 4, 0);

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

	

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

	int lastIdx = -1;
	for(int i = 0; i < model_test->tex_count; i++) {
		gsKit_TexManager_bind(gsGlobal, model_test->textures[i]);

		VECTOR* positions = &model_test->positions[lastIdx+1];
		VECTOR* texcoords = &model_test->texcoords[lastIdx+1];
		GSTEXTURE* tex = model_test->textures[i];

		int idxs_to_draw = (model_test->tex_ranges[i]-lastIdx);
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
		
			u64* p_data = cube_packet;
		
			*p_data++ = (*(u32*)(&fX) | (u64)*(u32*)(&fY) << 32);
			*p_data++ = (*(u32*)(&fZ) | (u64)count << 32);
		
			*p_data++ = GIF_TAG(1, 0, 0, 0, 0, 1);
			*p_data++ = GIF_AD;
		
			*p_data++ = GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0);
			*p_data++ = GS_TEX1_1;
		
			int tw, th;
			athena_set_tw_th(tex, &tw, &th);
		
			if(tex->VramClut == 0)
			{
				*p_data++ = GS_SETREG_TEX0(tex->Vram/256, tex->TBW, tex->PSM,
					tw, th, gsGlobal->PrimAlphaEnable, 0,
					0, 0, 0, 0, GS_CLUT_STOREMODE_NOLOAD);
			}
			else
			{
				*p_data++ = GS_SETREG_TEX0(tex->Vram/256, tex->TBW, tex->PSM,
					tw, th, gsGlobal->PrimAlphaEnable, 0,
					tex->VramClut/256, tex->ClutPSM, 0, 0, GS_CLUT_STOREMODE_LOAD);
			}
			
			*p_data++ = GS_TEX0_1;
		
			*p_data++ = VU_GS_GIFTAG(count, 1, 1,
    			VU_GS_PRIM(model_test->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 1, 1, gsGlobal->PrimFogEnable, 
				0, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    0, 3);
		
			*p_data++ = DRAW_STQ2_REGLIST;
		
			*p_data++ = (128 | (u64)128 << 32);
			*p_data++ = (128 | (u64)128 << 32);	

			curr_vif_packet = vif_packets[context];

			//memset(curr_vif_packet, 0, 16*22);

			//*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
			//

			u32 vif_added_bytes = 0; // zero because now we will use TOP register (double buffer)
									 // we don't wan't to unpack at 8 + beggining of buffer, but at
									 // the beggining of the buffer

			// Merge packets
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, cube_packet, 6, 1);
			vif_added_bytes += 6;

			// Add vertices
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &positions[idxs_drawn], count, 1);
			vif_added_bytes += count; // one VECTOR is size of qword

			// Add sts
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &texcoords[idxs_drawn], count, 1);
			vif_added_bytes += count;

			*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
			*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, (lastIdx == -1? VIF_MSCALF : VIF_MSCNT), 0) << 32));

			*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
			*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

			asm volatile("nop":::"memory");

			vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		lastIdx = model_test->tex_ranges[i];
	}


	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}

void draw_vu1_notex(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_screen;

	GSGLOBAL *gsGlobal = getGSGLOBAL();

	if (last_mpg != &VU1Draw3DNoTex_CodeStart) {
		dmaKit_wait(DMA_CHANNEL_VIF1, 0);
		vu1_upload_micro_program(&VU1Draw3DNoTex_CodeStart, &VU1Draw3DNoTex_CodeEnd);
		last_mpg = &VU1Draw3DNoTex_CodeStart;
	}

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	int idxs_to_draw = model_test->indexCount;
	int idxs_drawn = 0;

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	curr_vif_packet = vif_packets[context];

	//memset(curr_vif_packet, 0, 16*22);

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
	*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32));

	// Add matrix at the beggining of VU mem (skip TOP)
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 0, &local_screen, 4, 0);

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

	

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

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

		u64* p_data = cube_packet;

		*p_data++ = (*(u32*)(&fX) | (u64)*(u32*)(&fY) << 32);
		*p_data++ = (*(u32*)(&fZ) | (u64)(count) << 32);

		*p_data++ = VU_GS_GIFTAG(count, 1, 1,
    		VU_GS_PRIM(model_test->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 1, 0, gsGlobal->PrimFogEnable, 
			0, gsGlobal->PrimAAEnable, 0, 0, 0),
    	    0, 2);

		*p_data++ = DRAW_NOTEX_REGLIST;

		*p_data++ = (128 | (u64)128 << 32);
		*p_data++ = (128 | (u64)128 << 32);	

		curr_vif_packet = vif_packets[context];
	
		//memset(curr_vif_packet, 0, 16*22);

		//*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
		//
	
		u32 vif_added_bytes = 0; // zero because now we will use TOP register (double buffer)
								 // we don't wan't to unpack at 8 + beggining of buffer, but at
								 // the beggining of the buffer
	
		// Merge packets
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, cube_packet, 3, 1);
		vif_added_bytes += 3;
	
		// Add vertices
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &model_test->positions[idxs_drawn], count, 1);
		vif_added_bytes += count; // one VECTOR is size of qword

		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
		*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, ((!idxs_drawn)? VIF_MSCALF : VIF_MSCNT), 0) << 32));
	
		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
		*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);
		
		asm volatile("nop":::"memory");

		vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

		idxs_to_draw -= count;
		idxs_drawn += count;
	}


	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}


void draw_vu1_with_colors(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_light;
	MATRIX local_screen;

	GSGLOBAL *gsGlobal = getGSGLOBAL();

	if (last_mpg != &VU1Draw3DColors_CodeStart) {
		dmaKit_wait(DMA_CHANNEL_VIF1, 0);
		vu1_upload_micro_program(&VU1Draw3DColors_CodeStart, &VU1Draw3DColors_CodeEnd);
		last_mpg = &VU1Draw3DColors_CodeStart;
	}

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	curr_vif_packet = vif_packets[context];

	//memset(curr_vif_packet, 0, 16*22);

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
	*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32));

	// Add matrix at the beggining of VU mem (skip TOP)
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 0, &local_screen, 4, 0);

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

	

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

	int lastIdx = -1;
	for(int i = 0; i < model_test->tex_count; i++) {
		gsKit_TexManager_bind(gsGlobal, model_test->textures[i]);

		VECTOR* positions = &model_test->positions[lastIdx+1];
		VECTOR* texcoords = &model_test->texcoords[lastIdx+1];
		VECTOR* colours = &model_test->colours[lastIdx+1];
		GSTEXTURE* tex = model_test->textures[i];

		int idxs_to_draw = (model_test->tex_ranges[i]-lastIdx);
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

			float texCol = 128.0f;

			u64* p_data = cube_packet;

			*p_data++ = (*(u32*)(&fX) | (u64)*(u32*)(&fY) << 32);
			*p_data++ = (*(u32*)(&fZ) | (u64)(count) << 32);

			*p_data++ = GIF_TAG(1, 0, 0, 0, 0, 1);
			*p_data++ = GIF_AD;

			*p_data++ = GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0);
			*p_data++ = GS_TEX1_1;

			int tw, th;
			athena_set_tw_th(tex, &tw, &th);

			if(tex->VramClut == 0)
			{
				*p_data++ = GS_SETREG_TEX0(tex->Vram/256, tex->TBW, tex->PSM,
					tw, th, gsGlobal->PrimAlphaEnable, 0,
					0, 0, 0, 0, GS_CLUT_STOREMODE_NOLOAD);
			}
			else
			{
				*p_data++ = GS_SETREG_TEX0(tex->Vram/256, tex->TBW, tex->PSM,
					tw, th, gsGlobal->PrimAlphaEnable, 0,
					tex->VramClut/256, tex->ClutPSM, 0, 0, GS_CLUT_STOREMODE_LOAD);
			}
	
			*p_data++ = GS_TEX0_1;

			*p_data++ = VU_GS_GIFTAG(count, 1, 1,
    			VU_GS_PRIM(model_test->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 1, 1, gsGlobal->PrimFogEnable, 
				0, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    0, 3);

			*p_data++ = DRAW_STQ2_REGLIST;

			*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);
			*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);	

			curr_vif_packet = vif_packets[context];

			//memset(curr_vif_packet, 0, 16*22);

			//*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
			//

			u32 vif_added_bytes = 0; // zero because now we will use TOP register (double buffer)
									 // we don't wan't to unpack at 8 + beggining of buffer, but at
									 // the beggining of the buffer

			// Merge packets
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, cube_packet, 6, 1);
			vif_added_bytes += 6;

			// Add vertices
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &positions[idxs_drawn], count, 1);
			vif_added_bytes += count; // one VECTOR is size of qword

			// Add sts
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &texcoords[idxs_drawn], count, 1);
			vif_added_bytes += count;

			// Add colors
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &colours[idxs_drawn], count, 1);
			vif_added_bytes += count;

			*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
			*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, (lastIdx == -1? VIF_MSCALF : VIF_MSCNT), 0) << 32));

			*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
			*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

			asm volatile("nop":::"memory");

			vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		lastIdx = model_test->tex_ranges[i];
	}

	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}


void draw_vu1_with_colors_notex(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_light;
	MATRIX local_screen;

	GSGLOBAL *gsGlobal = getGSGLOBAL();

	if (last_mpg != &VU1Draw3DColorsNoTex_CodeStart) {
		dmaKit_wait(DMA_CHANNEL_VIF1, 0);
		vu1_upload_micro_program(&VU1Draw3DColorsNoTex_CodeStart, &VU1Draw3DColorsNoTex_CodeEnd);
		last_mpg = &VU1Draw3DColorsNoTex_CodeStart;
	}

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	int idxs_to_draw = model_test->indexCount;
	int idxs_drawn = 0;

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	curr_vif_packet = vif_packets[context];

	//memset(curr_vif_packet, 0, 16*22);

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
	*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32));

	// Add matrix at the beggining of VU mem (skip TOP)
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 0, &local_screen, 4, 0);

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

	

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

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

		u64* p_data = cube_packet;

		*p_data++ = (*(u32*)(&fX) | (u64)*(u32*)(&fY) << 32);
		*p_data++ = (*(u32*)(&fZ) | (u64)(count) << 32);

		*p_data++ = VU_GS_GIFTAG(count, 1, 1,
    		VU_GS_PRIM(model_test->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 1, 0, gsGlobal->PrimFogEnable, 
			0, gsGlobal->PrimAAEnable, 0, 0, 0),
    	    0, 2);

		*p_data++ = DRAW_NOTEX_REGLIST;

		*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);
		*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);	

		curr_vif_packet = vif_packets[context];
	
		//memset(curr_vif_packet, 0, 16*22);

		//*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
		//
	
		u32 vif_added_bytes = 0; // zero because now we will use TOP register (double buffer)
								 // we don't wan't to unpack at 8 + beggining of buffer, but at
								 // the beggining of the buffer
	
		// Merge packets
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, cube_packet, 3, 1);
		vif_added_bytes += 3;
	
		// Add vertices
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &model_test->positions[idxs_drawn], count, 1);
		vif_added_bytes += count; // one VECTOR is size of qword
	
		// Add colors
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &model_test->colours[idxs_drawn], count, 1);
		vif_added_bytes += count;

		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
		*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, ((!idxs_drawn)? VIF_MSCALF : VIF_MSCNT), 0) << 32));
	
		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
		*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);
		
		asm volatile("nop":::"memory");

		vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

		idxs_to_draw -= count;
		idxs_drawn += count;
	}

	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}

void draw_vu1_with_lights(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	GSGLOBAL *gsGlobal = getGSGLOBAL();

	if (last_mpg != &VU1Draw3DLightsColors_CodeStart) {
		dmaKit_wait(DMA_CHANNEL_VIF1, 0);
		vu1_upload_micro_program(&VU1Draw3DLightsColors_CodeStart, &VU1Draw3DLightsColors_CodeEnd);
		last_mpg = &VU1Draw3DLightsColors_CodeStart;
	}

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

	curr_vif_packet = vif_packets[context];

	// Add matrix at the beggining of VU mem (skip TOP)
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 0, &local_screen,      4, 0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 4, &local_light,       4, 0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 8, &active_dir_lights, 1, 0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 9, &dir_lights,       12, 0);

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

	int lastIdx = -1;
	for(int i = 0; i < model_test->tex_count; i++) {
		gsKit_TexManager_bind(gsGlobal, model_test->textures[i]);

		VECTOR* positions = &model_test->positions[lastIdx+1];
		VECTOR* texcoords = &model_test->texcoords[lastIdx+1];
		VECTOR* normals = &model_test->normals[lastIdx+1];
		VECTOR* colours = &model_test->colours[lastIdx+1];
		GSTEXTURE* tex = model_test->textures[i];

		int idxs_to_draw = (model_test->tex_ranges[i]-lastIdx);
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

			float texCol = 128.0f;

			u64* p_data = cube_packet;

			*p_data++ = (*(u32*)(&fX) | (u64)*(u32*)(&fY) << 32);
			*p_data++ = (*(u32*)(&fZ) | (u64)(count) << 32);

			*p_data++ = GIF_TAG(1, 0, 0, 0, 0, 1);
			*p_data++ = GIF_AD;

			*p_data++ = GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0);
			*p_data++ = GS_TEX1_1;

			int tw, th;
			athena_set_tw_th(tex, &tw, &th);

			if(tex->VramClut == 0)
			{
				*p_data++ = GS_SETREG_TEX0(tex->Vram/256, tex->TBW, tex->PSM,
					tw, th, gsGlobal->PrimAlphaEnable, COLOR_MODULATE,
					0, 0, 0, 0, GS_CLUT_STOREMODE_NOLOAD);
			}
			else
			{
				*p_data++ = GS_SETREG_TEX0(tex->Vram/256, tex->TBW, tex->PSM,
					tw, th, gsGlobal->PrimAlphaEnable, 0,
					tex->VramClut/256, tex->ClutPSM, 0, 0, GS_CLUT_STOREMODE_LOAD);
			}
	
			*p_data++ = GS_TEX0_1;

			*p_data++ = VU_GS_GIFTAG(count, 1, 1,
    			VU_GS_PRIM(model_test->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 1, 1, gsGlobal->PrimFogEnable, 
				gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    0, 3);

			*p_data++ = DRAW_STQ2_REGLIST;

			*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);
			*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);	

			curr_vif_packet = vif_packets[context];

			//memset(curr_vif_packet, 0, 16*22);

			//*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
			//

			u32 vif_added_bytes = 0; // zero because now we will use TOP register (double buffer)
									 // we don't wan't to unpack at 8 + beggining of buffer, but at
									 // the beggining of the buffer

			// Merge packets
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, cube_packet, 6, 1);
			vif_added_bytes += 6;

			// Add vertices
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &positions[idxs_drawn], count, 1);
			vif_added_bytes += count; // one VECTOR is size of qword

			// Add sts
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &texcoords[idxs_drawn], count, 1);
			vif_added_bytes += count;

			// Add colors
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &colours[idxs_drawn], count, 1);
			vif_added_bytes += count;

			// Add normals
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &normals[idxs_drawn], count, 1);
			vif_added_bytes += count;

			*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
			*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, (lastIdx == -1? VIF_MSCALF : VIF_MSCNT), 0) << 32));

			
			

			*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
			*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

			asm volatile("nop":::"memory");

			vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		lastIdx = model_test->tex_ranges[i];
	}

	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}


void draw_vu1_with_lights_notex(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_light;
	MATRIX local_screen;

	GSGLOBAL *gsGlobal = getGSGLOBAL();

	if (last_mpg != &VU1Draw3DLightsColorsNoTex_CodeStart) {
		dmaKit_wait(DMA_CHANNEL_VIF1, 0);
		vu1_upload_micro_program(&VU1Draw3DLightsColorsNoTex_CodeStart, &VU1Draw3DLightsColorsNoTex_CodeEnd);
		last_mpg = &VU1Draw3DLightsColorsNoTex_CodeStart;
	}

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_light(local_light, object_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	int idxs_to_draw = model_test->indexCount;
	int idxs_drawn = 0;

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	curr_vif_packet = vif_packets[context];

	// Add matrix at the beggining of VU mem (skip TOP)
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 0, &local_screen,      4,  0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 4, &local_light,       4,  0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 8, &active_dir_lights, 1,  0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 9, &dir_lights,        12, 0);

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

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

		u64* p_data = cube_packet;

		*p_data++ = (*(u32*)(&fX) | (u64)*(u32*)(&fY) << 32);
		*p_data++ = (*(u32*)(&fZ) | (u64)(count) << 32);

		*p_data++ = VU_GS_GIFTAG(count, 1, 1,
    		VU_GS_PRIM(model_test->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 1, 0, gsGlobal->PrimFogEnable, 
			0, gsGlobal->PrimAAEnable, 0, 0, gsGlobal->PrimAAEnable),
    	    0, 2);

		*p_data++ = DRAW_NOTEX_REGLIST;

		*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);
		*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);	

		curr_vif_packet = vif_packets[context];
	
		////memset(curr_vif_packet, 0, 16*22);

		//*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
		//
	
		u32 vif_added_bytes = 0; // zero because now we will use TOP register (double buffer)
								 // we don't wan't to unpack at 8 + beggining of buffer, but at
								 // the beggining of the buffer
	
		// Merge packets
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, cube_packet, 3, 1);
		vif_added_bytes += 3;
	
		// Add vertices
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &model_test->positions[idxs_drawn], count, 1);
		vif_added_bytes += count; // one VECTOR is size of qword
	
		// Add colors
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &model_test->colours[idxs_drawn], count, 1);
		vif_added_bytes += count;

		// Add normals
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &model_test->normals[idxs_drawn], count, 1);
		vif_added_bytes += count;

		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
		*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, ((!idxs_drawn)? VIF_MSCALF : VIF_MSCNT), 0) << 32));

		
		
	
		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
		*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);
		
		asm volatile("nop":::"memory");

		vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

		idxs_to_draw -= count;
		idxs_drawn += count;
	}

	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}

void draw_vu1_with_spec_lights(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	GSGLOBAL *gsGlobal = getGSGLOBAL();

	if (last_mpg != &VU1Draw3DSpec_CodeStart) {
		dmaKit_wait(DMA_CHANNEL_VIF1, 0);
		vu1_upload_micro_program(&VU1Draw3DSpec_CodeStart, &VU1Draw3DSpec_CodeEnd);
		last_mpg = &VU1Draw3DSpec_CodeStart;
	}
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

	curr_vif_packet = vif_packets[context];

	//memset(curr_vif_packet, 0, 16*22);

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	// Add matrix at the beggining of VU mem (skip TOP)
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 0,  &local_screen,      4,  0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 4,  &local_light,       4,  0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 8,  &active_dir_lights, 1,  0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 9,  &camera_position,   1,  0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 10, &dir_lights,        16, 0);

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

	int lastIdx = -1;
	for(int i = 0; i < model_test->tex_count; i++) {
		gsKit_TexManager_bind(gsGlobal, model_test->textures[i]);

		VECTOR* positions = &model_test->positions[lastIdx+1];
		VECTOR* texcoords = &model_test->texcoords[lastIdx+1];
		VECTOR* normals = &model_test->normals[lastIdx+1];
		VECTOR* colours = &model_test->colours[lastIdx+1];
		GSTEXTURE* tex = model_test->textures[i];

		int idxs_to_draw = (model_test->tex_ranges[i]-lastIdx);
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

			float texCol = 128.0f;

			u64* p_data = cube_packet;

			*p_data++ = (*(u32*)(&fX) | (u64)*(u32*)(&fY) << 32);
			*p_data++ = (*(u32*)(&fZ) | (u64)(count) << 32);

			*p_data++ = GIF_TAG(1, 0, 0, 0, 0, 1);
			*p_data++ = GIF_AD;

			*p_data++ = GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0);
			*p_data++ = GS_TEX1_1;

			int tw, th;
			athena_set_tw_th(tex, &tw, &th);

			if(tex->VramClut == 0)
			{
				*p_data++ = GS_SETREG_TEX0(tex->Vram/256, tex->TBW, tex->PSM,
					tw, th, gsGlobal->PrimAlphaEnable, COLOR_MODULATE,
					0, 0, 0, 0, GS_CLUT_STOREMODE_NOLOAD);
			}
			else
			{
				*p_data++ = GS_SETREG_TEX0(tex->Vram/256, tex->TBW, tex->PSM,
					tw, th, gsGlobal->PrimAlphaEnable, 0,
					tex->VramClut/256, tex->ClutPSM, 0, 0, GS_CLUT_STOREMODE_LOAD);
			}
	
			*p_data++ = GS_TEX0_1;

			*p_data++ = VU_GS_GIFTAG(count, 1, 1,
    			VU_GS_PRIM(model_test->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 1, 1, gsGlobal->PrimFogEnable, 
				gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 0, 0, 0),
    		    0, 3);

			*p_data++ = DRAW_STQ2_REGLIST;

			*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);
			*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);	

			curr_vif_packet = vif_packets[context];

			//memset(curr_vif_packet, 0, 16*22);

			//*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
			//

			u32 vif_added_bytes = 0; // zero because now we will use TOP register (double buffer)
									 // we don't wan't to unpack at 8 + beggining of buffer, but at
									 // the beggining of the buffer

			// Merge packets
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, cube_packet, 6, 1);
			vif_added_bytes += 6;

			// Add vertices
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &positions[idxs_drawn], count, 1);
			vif_added_bytes += count; // one VECTOR is size of qword

			// Add sts
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &texcoords[idxs_drawn], count, 1);
			vif_added_bytes += count;

			// Add colors
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &colours[idxs_drawn], count, 1);
			vif_added_bytes += count;

			// Add normals
			curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &normals[idxs_drawn], count, 1);
			vif_added_bytes += count;

			*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
			*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, (lastIdx == -1? VIF_MSCALF : VIF_MSCNT), 0) << 32));

			*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
			*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

			asm volatile("nop":::"memory");

			vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

			idxs_to_draw -= count;
			idxs_drawn += count;
		}

		lastIdx = model_test->tex_ranges[i];
	}

	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}

void draw_vu1_with_spec_lights_notex(model* model_test, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z)
{
	VECTOR object_position = { pos_x, pos_y, pos_z, 1.00f };
	VECTOR object_rotation = { rot_x, rot_y, rot_z, 1.00f };

	MATRIX local_world;
	MATRIX local_light;
	MATRIX local_screen;

	GSGLOBAL *gsGlobal = getGSGLOBAL();

	if (last_mpg != &VU1Draw3DSpecNoTex_CodeStart) {
		dmaKit_wait(DMA_CHANNEL_VIF1, 0);
		vu1_upload_micro_program(&VU1Draw3DSpecNoTex_CodeStart, &VU1Draw3DSpecNoTex_CodeEnd);
		last_mpg = &VU1Draw3DSpecNoTex_CodeStart;
	}

	gsGlobal->PrimAAEnable = GS_SETTING_ON;
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	create_local_world(local_world, object_position, object_rotation);
	create_local_light(local_light, object_rotation);
	create_local_screen(local_screen, local_world, world_view, view_screen);

	int idxs_to_draw = model_test->indexCount;
	int idxs_drawn = 0;

	dmaKit_wait(DMA_CHANNEL_VIF1, 0);

	curr_vif_packet = vif_packets[context];

	// Add matrix at the beggining of VU mem (skip TOP)
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 0,  &local_screen,      4,  0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 4,  &local_light,       4,  0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 8,  &active_dir_lights, 1,  0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 9,  &camera_position,   1,  0);
	curr_vif_packet = vu_add_unpack_data(curr_vif_packet, 10, &dir_lights,        16, 0);

	*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
	*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);

	

	vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

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

		u64* p_data = cube_packet;

		*p_data++ = (*(u32*)(&fX) | (u64)*(u32*)(&fY) << 32);
		*p_data++ = (*(u32*)(&fZ) | (u64)(count) << 32);

		*p_data++ = VU_GS_GIFTAG(count, 1, 1,
    		VU_GS_PRIM(model_test->tristrip? GS_PRIM_PRIM_TRISTRIP : GS_PRIM_PRIM_TRIANGLE, 1, 0, gsGlobal->PrimFogEnable, 
			0, gsGlobal->PrimAAEnable, 0, 0, gsGlobal->PrimAAEnable),
    	    0, 2);

		*p_data++ = DRAW_NOTEX_REGLIST;

		*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);
		*p_data++ = (*(u32*)(&texCol) | (u64)*(u32*)(&texCol) << 32);	

		curr_vif_packet = vif_packets[context];
	
		////memset(curr_vif_packet, 0, 16*22);

		//*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
		//
	
		u32 vif_added_bytes = 0; // zero because now we will use TOP register (double buffer)
								 // we don't wan't to unpack at 8 + beggining of buffer, but at
								 // the beggining of the buffer
	
		// Merge packets
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, cube_packet, 3, 1);
		vif_added_bytes += 3;
	
		// Add vertices
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &model_test->positions[idxs_drawn], count, 1);
		vif_added_bytes += count; // one VECTOR is size of qword
	
		// Add colors
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &model_test->colours[idxs_drawn], count, 1);
		vif_added_bytes += count;

		// Add normals
		curr_vif_packet = vu_add_unpack_data(curr_vif_packet, vif_added_bytes, &model_test->normals[idxs_drawn], count, 1);
		vif_added_bytes += count;

		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_CNT, 0, 0, 0);
		*curr_vif_packet++ = ((VIF_CODE(0, 0, VIF_FLUSH, 0) | (u64)VIF_CODE(0, 0, ((!idxs_drawn)? VIF_MSCALF : VIF_MSCNT), 0) << 32));
		
	
		*curr_vif_packet++ = DMA_TAG(0, 0, DMA_END, 0, 0 , 0);
		*curr_vif_packet++ = (VIF_CODE(0, 0, VIF_NOP, 0) | (u64)VIF_CODE(0, 0, VIF_NOP, 0) << 32);
		
		asm volatile("nop":::"memory");

		vifSendPacket(vif_packets[context], DMA_CHANNEL_VIF1);

		idxs_to_draw -= count;
		idxs_drawn += count;
	}

	// Switch packet, so we can proceed during DMA transfer
	context = !context;
}
