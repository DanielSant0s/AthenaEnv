
;//--------------------------------------------------------------------------------
;// Athena Macros Library
;//
;// Daniel Santos - 2025
;//
;//--------------------------------------------------------------------------------

;//--------------------------------------------------------------------
;// AddScreenOffset
;//--------------------------------------------------------------------
   .macro   AddScreenOffset  scale
    loi            2048.0
    addi.xy        offset, vf00, i
    add.zw          offset, vf00, vf00

    add.xyz offset, \scale, offset
   .endm

;//--------------------------------------------------------------------
;// LoadCullScale
;//--------------------------------------------------------------------
   .macro   LoadCullScale ret_scale, scale
   loi \scale
   add.xy     \ret_scale, vf00, i
   loi 1.0
   add.z      \ret_scale,  vf00, i
   mul.w      \ret_scale, vf00, vf00
   .endm

;//--------------------------------------------------------------------
;// VectorNormalizeClamp - Clamp vector to a [0.0, 1.0]
;//
;// Note:
;//--------------------------------------------------------------------

   .macro   VectorNormalizeClamp output, input
   max            Vector1111,  vf00,        vf00[w]
   sub            Vector0000,  vf00, vf00
   maxx.xyzw      \output, \input,  Vector0000              ;//
   minix.xyzw     \output, \output, Vector1111              ;//
   .endm

;//--------------------------------------------------------------------
;// BackfaceCull
;//--------------------------------------------------------------------

   .macro BackfaceCull v1, v2, v3
   ; this screen triangle's normal
   sub.xyz        delta_1\@, \v1, \v2
   sub.xyz        delta_2\@, \v3, \v2

   ; bfc_multiplier is 1 to cull back-facing polys, -1 for front
   mulw.xyz       delta_1\@, delta_1\@, bfc_multiplier
   opmula.xyz     acc, delta_1\@, delta_2\@
   opmsub.xyz     bfc_normal\@, delta_2\@, delta_1\@

   ; get sign of normal
   fmand          z_sign, z_sign_mask
   .endm
