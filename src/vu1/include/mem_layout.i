STACK_OFFSET        .assign  1023        ; (This was 1024 before, which was conflicting with the dummy XGKick...)

SCREEN_SCALE        .assign  0 ; xyz
RENDER_FLAGS        .assign  0 ; w

SCREEN_MATRIX       .assign  1
OBJECT_MATRIX       .assign  5

CAMERA_POSITION     .assign  9 ; xyz

LIGHT_DIRECTION_PTR .assign 10
LIGHT_AMBIENT_SUM   .assign 14 ; xyz = ambients pre-summed on the EE, w = 1.0 so
                               ; one lq initialises the light accumulator whole
NUM_DIR_LIGHTS      .assign 15 ; w[1]. Cannot share QW 14: its w is the float 1.0
                               ; above, whose low 16 bits read back as 0.
                               ; 16..17 free (were the other 3 ambients).
LIGHT_DIFFUSE_PTR   .assign 18
LIGHT_SPECULAR_PTR  .assign 22

CLIPFAN_OFFSET      .assign 26

CLIP_WORK_BUF_0     .assign 61
CLIP_WORK_BUF_1     .assign 101

; Moved to the end of VU mem so the double buffer can start at 141 and be one
; window for both layouts. 128 QW = 32 bones; stack lives above at 1008..1023.
BONE_MATRICES       .assign 880

; Was 269, which now falls INSIDE the double buffer (141..879). Parked in the
; gap the pre-summed ambient freed up (16..17).
BUMP_OFFSET     .assign 16

DUMMY_XGKICK_BUF    .assign 1023

INBUF_SIZE          .assign 194         ; Max NbrVerts (48 * 4) + prim tag + diffuse color
SKINNED_INBUF_SIZE          .assign 242         ; Max NbrVerts (40 * 6) + prim tag + diffuse color

GIFTAG_OFFSET      .assign 0
DIFF_MAT_OFFSET    .assign 1

POSITION_OFFSET .assign 2
NORMAL_OFFSET  .assign 50
COLOR_OFFSET    .assign 98
TEXCOORD_OFFSET .assign 146

SKINNED_SKELETON_OFFSET  .assign 2
SKINNED_POSITION_OFFSET .assign 82
SKINNED_NORMAL_OFFSET  .assign 122
SKINNED_COLOR_OFFSET    .assign 162
SKINNED_TEXCOORD_OFFSET .assign 202
