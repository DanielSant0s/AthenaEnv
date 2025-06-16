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

typedef union {
    struct {
	    float    x;
	    float    y;
	    float    z;
	    float    w;
    };

    struct {
	    uint32_t    ix;
	    uint32_t    iy;
	    uint32_t    iz;
	    uint32_t    iw;
    };

    float    f[4];
    uint32_t u[4];
} VUVECTOR;

typedef struct {
	VECTOR direction[4];
	FIVECTOR ambient[4];
	VECTOR diffuse[4];
	VECTOR specular[4];
} LightData;

#define CULL_FACE_NONE 0.0f
#define CULL_FACE_FRONT -1.0f
#define CULL_FACE_BACK 1.0f

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
	float face_culling; 
	int texture_mapping;
	int shade_model; // 0 = flat, 1 = gouraud
    int has_refmap; 
    int has_bumpmap; 
} RenderAttributes;


typedef struct athena_keyframe {
    float time;
    VECTOR position;
    VECTOR rotation;
    VECTOR scale;
} athena_keyframe;

typedef struct athena_bone_animation {
    uint32_t bone_id;
    athena_keyframe* position_keys;
    athena_keyframe* rotation_keys;
    athena_keyframe* scale_keys;
    uint32_t position_key_count;
    uint32_t rotation_key_count;
    uint32_t scale_key_count;
} athena_bone_animation;

typedef struct athena_animation {
    char name[64];
    float duration;
    float ticks_per_second;
    athena_bone_animation* bone_animations;
    uint32_t bone_animation_count;
} athena_animation;

typedef struct athena_animation_collection {
    athena_animation* animations;
    uint32_t count;
} athena_animation_collection;

typedef struct athena_animation_controller {
    athena_animation* current;

	float initial_time;
    float current_time;
    bool is_playing;
    bool loop;
} athena_animation_controller;

typedef struct athena_bone_data {
    char name[64];
    int32_t parent_id;

    MATRIX inverse_bind;   
    
    VECTOR position;
    VECTOR rotation;
    VECTOR scale;
} athena_bone_data;

typedef struct athena_bone_transform {
    MATRIX transform;
    
    VECTOR position;
    VECTOR rotation;
    VECTOR scale;
} athena_bone_transform;

typedef struct athena_skeleton {
    athena_bone_data* bones;
    uint32_t bone_count;
} athena_skeleton;

typedef struct vertex_skin_data {
    uint32_t bone_indices[4];  
    float bone_weights[4];    
} vertex_skin_data;

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

    float bump_scale; 

	int texture_id;
    int bump_texture_id;
    int ref_texture_id;
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

    vertex_skin_data* skin_data;    
	athena_skeleton* skeleton;        

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
	MATRIX transform;

    VECTOR position;
	VECTOR rotation;
    VECTOR scale;

    athena_animation_controller anim_controller; 
    athena_bone_transform *bones;
    MATRIX *bone_matrices;

    VECTOR *bump_offset_buffer;
    VECTOR bump_offset;

    athena_render_data *data;

} athena_object_data qw_aligned;

void initCamera(MATRIX *ws, MATRIX *wv, MATRIX *vs);

void cameraUpdate();

#define BATCH_SIZE 51
#define BATCH_SIZE_SKINNED 33

int clip_bounding_box(MATRIX local_clip, VECTOR *bounding_box);
void calculate_vertices_clipped(VECTOR *output,  int count, VECTOR *vertices, MATRIX local_screen);
int draw_convert_xyz(xyz_t *output, float x, float y, int z, int count, vertex_f_t *vertices);

unsigned int get_max_z(GSGLOBAL* gsGlobal);

void athena_set_tw_th(const GSTEXTURE *Texture, int *tw, int *th);

void athena_line_goraud_3d(GSGLOBAL *gsGlobal, float x1, float y1, int iz1, float x2, float y2, int iz2, u64 color1, u64 color2);

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

void loadModel(athena_render_data* res_m, const char* path, GSTEXTURE* text);
void draw_bbox(athena_object_data *obj, Color color);

void render_object(athena_object_data *obj);

void new_render_object(athena_object_data *obj, athena_render_data *data);

void update_object_space(athena_object_data *obj);

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

void slerp_quaternion(VECTOR result, VECTOR q1, VECTOR q2, float t);
void find_keyframe_indices(athena_keyframe* keys, uint32_t key_count, float time, 
                          uint32_t* prev_idx, uint32_t* next_idx, float* t);

void apply_animation(athena_object_data* obj, float anim_time);					  

void update_bone_transforms(athena_object_data* obj);

void create_transform_matrix(MATRIX result, const VECTOR position, 
                           const VECTOR rotation, const VECTOR scale);

void decompose_transform_matrix(const MATRIX matrix, VECTOR position, 
                              VECTOR rotation, VECTOR scale);

#endif