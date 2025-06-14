STACK_OFFSET        .assign  1023        ; (This was 1024 before, which was conflicting with the dummy XGKick...)

SCREEN_SCALE        .assign  0 ; xyz
RENDER_FLAGS        .assign  0 ; w

SCREEN_MATRIX       .assign  1
LIGHT_MATRIX        .assign  5

CAMERA_POSITION     .assign  9 ; xyz

LIGHT_DIRECTION_PTR .assign 10
LIGHT_AMBIENT_PTR   .assign 14 ; xyz[4]
NUM_DIR_LIGHTS      .assign 14 ; w[1]
LIGHT_DIFFUSE_PTR   .assign 18
LIGHT_SPECULAR_PTR  .assign 22

CLIPFAN_OFFSET      .assign 26

CLIP_WORK_BUF_0     .assign 61
CLIP_WORK_BUF_1     .assign 101

OBJECT_MATRIX       .assign 141

BONE_MATRICES       .assign 145

TEXCOORD_OFFSET     .assign 273

INBUF_SIZE          .assign 204         ; Max NbrVerts (51 * 4)
DUMMY_XGKICK_BUF    .assign 1023
