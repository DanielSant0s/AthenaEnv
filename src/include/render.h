#ifndef ATHENA_RENDER_H
#define ATHENA_RENDER_H

#include <graphics.h>
#include <owl_packet.h>

//3D math

typedef struct {
	float    x;
	float    y;
	float    z;
	uint32_t w;
} FIVECTOR;

typedef struct {
	float    x;
	float    y;
	float    z;
	uint32_t w;
} VUVECTOR;

typedef struct {
	VECTOR direction[4];
	FIVECTOR ambient[4];
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
	PL_NO_LIGHTS,
	PL_DEFAULT,
	PL_SPECULAR,
} eRenderPipelines;

typedef enum {
	SHADE_FLAT,
	SHADE_GOURAUD,
	SHADE_BLINN_PHONG,
} eRenderShadeModels;

typedef struct {
	int accurate_clipping; 
	int backface_culling; 
	int texture_mapping;
	int shade_model; // 0 = flat, 1 = gouraud
} RenderAttributes;


typedef struct
{
	owl_qword prim_tag;
	owl_qword clip_prim_tag;

    VECTOR ambient; 
    VECTOR diffuse;
    VECTOR specular;  
    VECTOR emission;  
    VECTOR transmittance; 
    float  shininess;  
    float  refraction; 
    VECTOR transmission_filter; 
    float  disolve;  

	int texture_id;
} ath_mat;

typedef struct {
	uint32_t index;
	uint32_t end;
} material_index;

typedef struct athena_render_data {
    uint32_t index_count;

    VECTOR* positions;
	VECTOR* texcoords;
	VECTOR* normals;
    VECTOR* colours;

    VECTOR bounding_box[8];

    eRenderPipelines pipeline;

    GSTEXTURE** textures;
	int texture_count;

	ath_mat *materials;
	uint32_t material_count;

	material_index *material_indices;
	int material_index_count;

	RenderAttributes attributes;

	bool tristrip;
} athena_render_data;

typedef struct athena_object_data {
	athena_render_data *data;

	VECTOR position;
	VECTOR rotation;

	MATRIX local_light;
	MATRIX local_screen;
} athena_object_data;

typedef enum {
	CAMERA_DEFAULT,
	CAMERA_LOOKAT,
} eCameraTypes;

void initCamera(MATRIX* wv);

void setCameraType(eCameraTypes type);

void cameraUpdate();

#define BATCH_SIZE 51
#define BATCH_SIZE_NO_CLIPPING 60

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

void init3D(float fov, float near, float far);

VECTOR *getCameraPosition();
void    setCameraPosition(float x, float y, float z);
void    setCameraRotation(float x, float y, float z);

void setCameraTarget(float x, float y, float z);
void turnCamera(float yaw, float pitch);
void orbitCamera(float yaw, float pitch);
void dollyCamera(float dist);
void zoomCamera(float dist);
void panCamera(float x, float y);

int NewLight();
void SetLightAttribute(int id, float x, float y, float z, int attr);

void loadOBJ(athena_render_data* res_m, const char* path, GSTEXTURE* text);
void draw_bbox(athena_object_data *obj, Color color);

void render_object(athena_object_data *obj);

void SubVector(VECTOR v0, VECTOR v1, VECTOR v2);
void AddVector(VECTOR res, VECTOR v1, VECTOR v2);
float LenVector(VECTOR v);
void SetLenVector(VECTOR v, float newLength);
void Normalize(VECTOR v0, VECTOR v1);
void OuterProduct(VECTOR v0, VECTOR v1, VECTOR v2);
void ScaleVector(VECTOR res, VECTOR v, float size);

void create_view(MATRIX view_screen, float fov, float near, float far, float w, float h);

#define alloc_vectors(cnt) (VECTOR*)malloc(cnt * sizeof(VECTOR))
#define copy_vectors(dst, src, cnt) memcpy(dst, src, cnt*sizeof(VECTOR))
#define copy_vector(dst, src) memcpy(dst, src, sizeof(VECTOR))
#define free_vectors(vec) free(vec)

#define copy_init_w_vector(dst, src) \
do { \
	dst[0] = src[0]; \
	dst[1] = src[1]; \
	dst[2] = src[2]; \
	dst[3] = 1.0f;  \
} while (0)

#define init_vector(vec) do { vec[0] = 1.0f; vec[1] = 1.0f; vec[2] = 1.0f; vec[3] = 1.0f; } while (0)

#define append_texture(m, tex) \
do { \
	m->texture_count++; \
	m->textures = realloc(m->textures, sizeof(GSTEXTURE*)*m->texture_count); \
	m->textures[m->texture_count-1] = tex; \
} while (0)

#endif