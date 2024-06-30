#ifndef ATHENA_RENDER_H
#define ATHENA_RENDER_H

#include "graphics.h"

//3D math

typedef struct {
	VECTOR direction[4];
	VECTOR ambient[4];
	VECTOR diffuse[4];
	VECTOR specular[4];
} LightData;

typedef enum {
	ATHENA_LIGHT_DIRECTION,
	ATHENA_LIGHT_AMBIENT,
	ATHENA_LIGHT_DIFFUSE,
	ATHENA_LIGHT_SPECULAR,
} eLightAttributes;

typedef enum {
	PL_NO_LIGHTS_COLORS,
	PL_NO_LIGHTS_COLORS_TEX,
	PL_NO_LIGHTS,
	PL_NO_LIGHTS_TEX,
	PL_DEFAULT,
	PL_DEFAULT_NO_TEX,
} eRenderPipelines;

typedef struct ath_model {
	uint32_t facesCount;
    uint32_t indexCount;

    VECTOR* positions;
	VECTOR* texcoords;
	VECTOR* normals;
    VECTOR* colours;

    VECTOR bounding_box[8];

    void (*render)(struct ath_model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);
    eRenderPipelines pipeline;

    GSTEXTURE** textures;
	int *tex_ranges;
	int tex_count;
} model;

int athena_render_set_pipeline(model* m, int pl_id);

typedef enum {
	CAMERA_DEFAULT,
	CAMERA_LOOKAT,
} eCameraTypes;

void setCameraType(eCameraTypes type);

void cameraUpdate();

#define BATCH_SIZE 48

int clip_bounding_box(MATRIX local_clip, VECTOR *bounding_box);
void calculate_vertices_clipped(VECTOR *output,  int count, VECTOR *vertices, MATRIX local_screen);
int draw_convert_xyz(xyz_t *output, float x, float y, int z, int count, vertex_f_t *vertices);

unsigned int get_max_z(GSGLOBAL* gsGlobal);

void vu0_vector_clamp(VECTOR v0, VECTOR v1, float min, float max);

float vu0_innerproduct(VECTOR v0, VECTOR v1);

void athena_set_tw_th(const GSTEXTURE *Texture, int *tw, int *th);

void athena_line_goraud_3d(GSGLOBAL *gsGlobal, float x1, float y1, int iz1, float x2, float y2, int iz2, u64 color1, u64 color2);

void CameraMatrix(MATRIX m, VECTOR p, VECTOR zd, VECTOR yd);

void RotCameraMatrix(MATRIX m, VECTOR p, VECTOR zd, VECTOR yd, VECTOR rot);

void LookAtCameraMatrix(MATRIX m, VECTOR position, VECTOR target, VECTOR up);

void init3D(float aspect, float fov, float near, float far);

void setCameraPosition(float x, float y, float z);
void setCameraRotation(float x, float y, float z);

void setCameraTarget(float x, float y, float z);
void turnCamera(float yaw, float pitch);
void orbitCamera(float yaw, float pitch);
void dollyCamera(float dist);
void zoomCamera(float dist);
void panCamera(float x, float y);

void SetLightAttribute(int id, float x, float y, float z, int attr);

void loadOBJ(model* res_m, const char* path, GSTEXTURE* text);
void draw_bbox(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z, Color color);

void SubVector(VECTOR v0, VECTOR v1, VECTOR v2);
void AddVector(VECTOR res, VECTOR v1, VECTOR v2);
float LenVector(VECTOR v);
void SetLenVector(VECTOR v, float newLength);
void Normalize(VECTOR v0, VECTOR v1);
void OuterProduct(VECTOR v0, VECTOR v1, VECTOR v2);
void ScaleVector(VECTOR res, VECTOR v, float size);

#endif