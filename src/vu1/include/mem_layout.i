STACK_OFFSET        .assign  1023        ; (This was 1024 before, which was conflicting with the dummy XGKick...)

SCREEN_SCALE        .assign  0 ; xyz
RENDER_FLAGS        .assign  0 ; w

SCREEN_MATRIX       .assign  1
OBJECT_MATRIX       .assign  5

CAMERA_POSITION     .assign  9 ; xyz

LIGHT_DIRECTION_PTR .assign 10
LIGHT_AMBIENT_PTR   .assign 14 ; xyz[4]
NUM_DIR_LIGHTS      .assign 14 ; w[1]
LIGHT_DIFFUSE_PTR   .assign 18
LIGHT_SPECULAR_PTR  .assign 22

CLIPFAN_OFFSET      .assign 26

CLIP_WORK_BUF_0     .assign 61
CLIP_WORK_BUF_1     .assign 101

BONE_MATRICES       .assign 141

BUMP_OFFSET     .assign 269

DUMMY_XGKICK_BUF    .assign 1023

INBUF_SIZE          .assign 194         ; Max NbrVerts (48 * 4) + prim tag + diffuse color
SKINNED_INBUF_SIZE          .assign 182         ; Max NbrVerts (30 * 6) + prim tag + diffuse color

GIFTAG_OFFSET      .assign 0
DIFF_MAT_OFFSET    .assign 1

POSITION_OFFSET .assign 2
NORMAL_OFFSET  .assign 50
COLOR_OFFSET    .assign 98
TEXCOORD_OFFSET .assign 146

SKINNED_SKELETON_OFFSET  .assign 2
SKINNED_POSITION_OFFSET .assign 62
SKINNED_NORMAL_OFFSET  .assign 92
SKINNED_COLOR_OFFSET    .assign 122
SKINNED_TEXCOORD_OFFSET .assign 152
