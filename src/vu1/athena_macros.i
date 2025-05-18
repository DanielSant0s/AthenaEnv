
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
   .macro   LoadCullScale scale
   loi \scale
   add.xy     clip_scale, vf00, i
   loi 1.0
   add.z      clip_scale,  vf00, i
   mul.w      clip_scale, vf00, vf00
   .endm

