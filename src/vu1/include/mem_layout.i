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
; Was 269, which now falls INSIDE the double buffer (141..824). Parked in the
; gap the pre-summed ambient freed up (16..17).
BUMP_OFFSET         .assign 16
NUM_POINT_LIGHTS    .assign 17 ; w = active point light count (0..4)

LIGHT_DIFFUSE_PTR   .assign 18
LIGHT_SPECULAR_PTR  .assign 22

CLIPFAN_OFFSET      .assign 26 ; 27..57 used by scissor triangle-fan output

CLIP_WORK_BUF_0     .assign 58 ; 58..90  (33 QW = up to 11 clipped fan verts)
CLIP_WORK_BUF_1     .assign 91 ; 91..123 (33 QW = up to 11 clipped fan verts)

; Hardware Point Lights (Object-Space position + Diffuse/Atten + Specular)
POINT_LIGHT_POS_PTR  .assign 124 ; 124..127 (xyz = obj-space pos, w = 1.0)
POINT_LIGHT_DIFF_PTR .assign 128 ; 128..131 (xyz = diffuse rgb, w = quad atten)
POINT_LIGHT_SPEC_PTR .assign 132 ; 132..135 (xyz = specular rgb, w = 0.0)

; Double buffer occupies 141..482 (342 QW) and 483..824 (342 QW).
; 825..1008 = 184 QW = 46 bones; stack lives above at 1009..1022.
BONE_MATRICES       .assign 825

DUMMY_XGKICK_BUF    .assign 1023

INBUF_SIZE          .assign 194         ; Max NbrVerts (48 * 4) + prim tag + diffuse color
SKINNED_INBUF_SIZE  .assign 218         ; Max NbrVerts (36 * 6) + prim tag + diffuse color

GIFTAG_OFFSET       .assign 0
DIFF_MAT_OFFSET     .assign 1

POSITION_OFFSET     .assign 2
NORMAL_OFFSET       .assign 50
COLOR_OFFSET        .assign 98
TEXCOORD_OFFSET     .assign 146

SKINNED_INDICES_OFFSET  .assign 2
SKINNED_WEIGHTS_OFFSET  .assign 38
SKINNED_POSITION_OFFSET .assign 74
SKINNED_NORMAL_OFFSET   .assign 110
SKINNED_COLOR_OFFSET    .assign 146
SKINNED_TEXCOORD_OFFSET .assign 182
