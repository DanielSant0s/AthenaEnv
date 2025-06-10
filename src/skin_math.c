#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>

#include <render.h>
#include <dbgprintf.h>

#include <owl_packet.h>

#include <matrix.h>

void lerp_vector(VECTOR result, VECTOR a, VECTOR b, float t) {
    result[0] = a[0] + (b[0] - a[0]) * t;
    result[1] = a[1] + (b[1] - a[1]) * t;
    result[2] = a[2] + (b[2] - a[2]) * t;
    result[3] = a[3] + (b[3] - a[3]) * t;
}

void slerp_quaternion(VECTOR result, VECTOR q1, VECTOR q2, float t) {
    float dot = q1[0] * q2[0] + q1[1] * q2[1] + q1[2] * q2[2] + q1[3] * q2[3];

    if (dot < 0.0f) {
        q2[0] = -q2[0];
        q2[1] = -q2[1];
        q2[2] = -q2[2];
        q2[3] = -q2[3];
        dot = -dot;
    }

    if (dot > 0.9995f) {
        result[0] = q1[0] + (q2[0] - q1[0]) * t;
        result[1] = q1[1] + (q2[1] - q1[1]) * t;
        result[2] = q1[2] + (q2[2] - q1[2]) * t;
        result[3] = q1[3] + (q2[3] - q1[3]) * t;
    } else {
        float theta_0 = acosf(dot);
        float theta = theta_0 * t;
        float sin_theta = sinf(theta);
        float sin_theta_0 = sinf(theta_0);
        
        float s0 = cosf(theta) - dot * sin_theta / sin_theta_0;
        float s1 = sin_theta / sin_theta_0;
        
        result[0] = s0 * q1[0] + s1 * q2[0];
        result[1] = s0 * q1[1] + s1 * q2[1];
        result[2] = s0 * q1[2] + s1 * q2[2];
        result[3] = s0 * q1[3] + s1 * q2[3];
    }

    float length = sqrtf(result[0] * result[0] + result[1] * result[1] + 
                        result[2] * result[2] + result[3] * result[3]);
    if (length > 0.0f) {
        result[0] /= length;
        result[1] /= length;
        result[2] /= length;
        result[3] /= length;
    }
}

void find_keyframe_indices(athena_keyframe* keys, uint32_t key_count, float time, 
                          uint32_t* prev_idx, uint32_t* next_idx, float* t) {
    if (key_count == 0) {
        *prev_idx = *next_idx = 0;
        *t = 0.0f;
        return;
    }
    
    if (time <= keys[0].time) {
        *prev_idx = *next_idx = 0;
        *t = 0.0f;
        return;
    }
    
    if (time >= keys[key_count - 1].time) {
        *prev_idx = *next_idx = key_count - 1;
        *t = 0.0f;
        return;
    }

    for (uint32_t i = 0; i < key_count - 1; i++) {
        if (time >= keys[i].time && time <= keys[i + 1].time) {
            *prev_idx = i;
            *next_idx = i + 1;
            
            float duration = keys[i + 1].time - keys[i].time;
            *t = (duration > 0.0f) ? (time - keys[i].time) / duration : 0.0f;
            return;
        }
    }

    *prev_idx = *next_idx = key_count - 1;
    *t = 0.0f;
}

void apply_animation(athena_render_data* render_data, uint32_t animation_index, float time) {
    athena_animation* anim = &render_data->anim_controller.animations[animation_index];
    athena_skeleton* skeleton = render_data->skeleton;

    float normalized_time = fmodf(time, anim->duration);

    for (uint32_t bone_anim_idx = 0; bone_anim_idx < anim->bone_animation_count; bone_anim_idx++) {
        athena_bone_animation* bone_anim = &anim->bone_animations[bone_anim_idx];
        
        if (bone_anim->bone_id >= skeleton->bone_count) {
            continue; 
        }
        
        athena_bone* bone = &skeleton->bones[bone_anim->bone_id];

        if (bone_anim->position_keys && bone_anim->position_key_count > 0) {
            uint32_t prev_idx, next_idx;
            float t;
            find_keyframe_indices(bone_anim->position_keys, bone_anim->position_key_count, 
                                normalized_time, &prev_idx, &next_idx, &t);
            
            if (prev_idx == next_idx) {
                bone->position[0] = bone_anim->position_keys[prev_idx].position[0];
                bone->position[1] = bone_anim->position_keys[prev_idx].position[1];
                bone->position[2] = bone_anim->position_keys[prev_idx].position[2];
            } else {
                VECTOR pos;
                lerp_vector(pos, bone_anim->position_keys[prev_idx].position,
                                       bone_anim->position_keys[next_idx].position, t);
                bone->position[0] = pos[0];
                bone->position[1] = pos[1];
                bone->position[2] = pos[2];
            }
        }

        if (bone_anim->rotation_keys && bone_anim->rotation_key_count > 0) {
            uint32_t prev_idx, next_idx;
            float t;
            find_keyframe_indices(bone_anim->rotation_keys, bone_anim->rotation_key_count, 
                                normalized_time, &prev_idx, &next_idx, &t);
            
            if (prev_idx == next_idx) {
                bone->rotation[0] = bone_anim->rotation_keys[prev_idx].rotation[0];
                bone->rotation[1] = bone_anim->rotation_keys[prev_idx].rotation[1];
                bone->rotation[2] = bone_anim->rotation_keys[prev_idx].rotation[2];
                bone->rotation[3] = bone_anim->rotation_keys[prev_idx].rotation[3];
            } else {
                VECTOR rot;
                slerp_quaternion(rot, bone_anim->rotation_keys[prev_idx].rotation,
                                            bone_anim->rotation_keys[next_idx].rotation, t);

                
                bone->rotation[0] = rot[0];
                bone->rotation[1] = rot[1];
                bone->rotation[2] = rot[2];
                bone->rotation[3] = rot[3];
            }
        }

        if (bone_anim->scale_keys && bone_anim->scale_key_count > 0) {
            uint32_t prev_idx, next_idx;
            float t;
            find_keyframe_indices(bone_anim->scale_keys, bone_anim->scale_key_count, 
                                normalized_time, &prev_idx, &next_idx, &t);
            
            if (prev_idx == next_idx) {
                bone->scale[0] = bone_anim->scale_keys[prev_idx].scale[0];
                bone->scale[1] = bone_anim->scale_keys[prev_idx].scale[1];
                bone->scale[2] = bone_anim->scale_keys[prev_idx].scale[2];
            } else {
                VECTOR scale;
                lerp_vector(scale, bone_anim->scale_keys[prev_idx].scale,
                                         bone_anim->scale_keys[next_idx].scale, t);
                bone->scale[0] = scale[0];
                bone->scale[1] = scale[1];
                bone->scale[2] = scale[2];
            }
        }
    }

    update_bone_transforms(skeleton);
}

void update_bone_transforms(athena_skeleton* skeleton) {
    if (!skeleton || !skeleton->bones) {
        return;
    }

    for (uint32_t i = 0; i < skeleton->bone_count; i++) {
        athena_bone* bone = &skeleton->bones[i];

        MATRIX local_transform;
        create_transform_matrix(local_transform, 
                              bone->position, 
                              bone->rotation, 
                              bone->scale);

        memcpy(bone->current_transform, local_transform, sizeof(MATRIX));

        if (bone->parent_id == -1 || bone->parent_id >= (int32_t)skeleton->bone_count) {
            
        } else {
            athena_bone* parent = &skeleton->bones[bone->parent_id];
            athena_bone* old_parent = NULL;
            
            do {
                old_parent = parent;
                matrix_functions->multiply(bone->current_transform, parent->current_transform, bone->current_transform);
                parent = &skeleton->bones[parent->parent_id];
            } while (old_parent->parent_id != -1 && old_parent->parent_id >= (int32_t)skeleton->bone_count);
            
        }

        MATRIX trans_inv;
        matrix_functions->transpose(trans_inv, bone->inverse_bind);
        matrix_functions->multiply(skeleton->bone_matrices[i], bone->current_transform, trans_inv);
        
        matrix_functions->transpose(skeleton->bone_matrices[i], skeleton->bone_matrices[i]);
    }
}

void decompose_transform_matrix(const MATRIX matrix, VECTOR position, 
                              VECTOR rotation, VECTOR scale) {
    position[0] = matrix[3];
    position[1] = matrix[7];
    position[2] = matrix[11];

    VECTOR col1 = {matrix[0], matrix[4], matrix[8], 0.0f};
    VECTOR col2 = {matrix[1], matrix[5], matrix[9], 0.0f};
    VECTOR col3 = {matrix[2], matrix[6], matrix[10], 0.0f};

    scale[0] = sqrtf(col1[0] * col1[0] + col1[1] * col1[1] + col1[2] * col1[2]);
    scale[1] = sqrtf(col2[0] * col2[0] + col2[1] * col2[1] + col2[2] * col2[2]);
    scale[2] = sqrtf(col3[0] * col3[0] + col3[1] * col3[1] + col3[2] * col3[2]);

    float det = matrix[0] * (matrix[5] * matrix[10] - matrix[9] * matrix[6]) -
                matrix[1] * (matrix[4] * matrix[10] - matrix[8] * matrix[6]) +
                matrix[2] * (matrix[4] * matrix[9] - matrix[8] * matrix[5]);
    
    if (det < 0.0f) {
        scale[0] = -scale[0];
    }

    MATRIX rotation_matrix;
    matrix_unit(rotation_matrix);
    
    if (scale[0] != 0.0f) {
        rotation_matrix[0] = col1[0] / scale[0];
        rotation_matrix[4] = col1[1] / scale[0];
        rotation_matrix[8] = col1[2] / scale[0];
    }
    
    if (scale[1] != 0.0f) {
        rotation_matrix[1] = col2[0] / scale[1];
        rotation_matrix[5] = col2[1] / scale[1];
        rotation_matrix[9] = col2[2] / scale[1];
    }
    
    if (scale[2] != 0.0f) {
        rotation_matrix[2] = col3[0] / scale[2];
        rotation_matrix[6] = col3[1] / scale[2];
        rotation_matrix[10] = col3[2] / scale[2];
    }

    matrix_to_quaternion(rotation, rotation_matrix);
}

void matrix_to_quaternion(VECTOR quaternion, const MATRIX rotation_matrix) {
    float trace = rotation_matrix[0] + rotation_matrix[5] + rotation_matrix[10];
    
    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f) * 2.0f; // s = 4 * qw
        quaternion[3] = 0.25f * s;
        quaternion[0] = (rotation_matrix[9] - rotation_matrix[6]) / s;
        quaternion[1] = (rotation_matrix[2] - rotation_matrix[8]) / s;
        quaternion[2] = (rotation_matrix[4] - rotation_matrix[1]) / s;
    } else if ((rotation_matrix[0] > rotation_matrix[5]) && (rotation_matrix[0] > rotation_matrix[10])) {
        float s = sqrtf(1.0f + rotation_matrix[0] - rotation_matrix[5] - rotation_matrix[10]) * 2.0f; // s = 4 * qx
        quaternion[3] = (rotation_matrix[9] - rotation_matrix[6]) / s;
        quaternion[0] = 0.25f * s;
        quaternion[1] = (rotation_matrix[1] + rotation_matrix[4]) / s;
        quaternion[2] = (rotation_matrix[2] + rotation_matrix[8]) / s;
    } else if (rotation_matrix[5] > rotation_matrix[10]) {
        float s = sqrtf(1.0f + rotation_matrix[5] - rotation_matrix[0] - rotation_matrix[10]) * 2.0f; // s = 4 * qy
        quaternion[3] = (rotation_matrix[2] - rotation_matrix[8]) / s;
        quaternion[0] = (rotation_matrix[1] + rotation_matrix[4]) / s;
        quaternion[1] = 0.25f * s;
        quaternion[2] = (rotation_matrix[6] + rotation_matrix[9]) / s;
    } else {
        float s = sqrtf(1.0f + rotation_matrix[10] - rotation_matrix[0] - rotation_matrix[5]) * 2.0f; // s = 4 * qz
        quaternion[3] = (rotation_matrix[4] - rotation_matrix[1]) / s;
        quaternion[0] = (rotation_matrix[2] + rotation_matrix[8]) / s;
        quaternion[1] = (rotation_matrix[6] + rotation_matrix[9]) / s;
        quaternion[2] = 0.25f * s;
    }
}

void create_transform_matrix(MATRIX result, const VECTOR position, 
                           const VECTOR rotation, const VECTOR scale) {

    MATRIX scale_matrix;
    matrix_unit(scale_matrix);
    scale_matrix[0] =  scale[0] * 1.0f;
    scale_matrix[5] =  scale[1] * 1.0f;
    scale_matrix[10] = scale[2] * 1.0f;

    MATRIX rotation_matrix;
    quaternion_to_matrix(rotation_matrix, rotation);

    MATRIX translation_matrix;
    matrix_unit(translation_matrix);
    translation_matrix[3] = position[0];
    translation_matrix[7] = position[1];
    translation_matrix[11] = position[2];

    MATRIX temp;
    matrix_functions->multiply(temp, rotation_matrix, scale_matrix);
    matrix_functions->multiply(result, translation_matrix, temp);
}

void quaternion_to_matrix(MATRIX result, const VECTOR quaternion) {
    float x = quaternion[0];
    float y = quaternion[1];
    float z = quaternion[2];
    float w = quaternion[3];
    
    float x2 = x * 2.0f;
    float y2 = y * 2.0f;
    float z2 = z * 2.0f;
    float xx = x * x2;
    float xy = x * y2;
    float xz = x * z2;
    float yy = y * y2;
    float yz = y * z2;
    float zz = z * z2;
    float wx = w * x2;
    float wy = w * y2;
    float wz = w * z2;
    
    matrix_unit(result);
    
    result[0] = 1.0f - (yy + zz);
    result[1] = xy - wz;
    result[2] = xz + wy;
    
    result[4] = xy + wz;
    result[5] = 1.0f - (xx + zz);
    result[6] = yz - wx;
    
    result[8] = xz - wy;
    result[9] = yz + wx;
    result[10] = 1.0f - (xx + yy);
}


