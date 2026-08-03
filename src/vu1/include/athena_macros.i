
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

;//--------------------------------------------------------------------
;// DecompressPositionW - positions arrive via a V3_32 VIF unpack (xyz
;// only). The VIF unpack hardware reuses the V4 datapath internally,
;// so the w lane it leaves behind is indeterminate (garbage), never
;// "whatever was there before". vf00 is hardwired to (0,0,0,1), so
;// copying its w lane deterministically restores w=1.0.
;//--------------------------------------------------------------------
   .macro   DecompressPositionW vertex
   move.w         \vertex, vf00
   .endm

;//--------------------------------------------------------------------
;// DecompressNormal8 - V4_8 signed unpack to float, matching the /127
;// scale render_cook_compact_vertices packs with (round(v*127), v
;// clamped to [-1,1] beforehand, so -128 is never actually produced).
;// All 4 lanes are real packed data (see athena_compact_normal), no
;// lane fixup needed. VU's ITOF only has fixed shift variants
;// (itof0/4/12/15), none of which give /127, so scale explicitly off
;// the raw int (itof0).
;//--------------------------------------------------------------------
   .macro   DecompressNormal8 output, packed
   itof0          \output, \packed
   loi            0.007874015748031496
   mul            \output, \output, i
   .endm

;//--------------------------------------------------------------------
;// DecompressColor8 - V4_8 unsigned unpack (0..255) to float
;// (0.0..1.0), the same range the existing add-with-matDiffuse +
;// VectorNormalizeClamp + ColorFPtoGsRGBAQ pipeline already expects.
;//--------------------------------------------------------------------
   .macro   DecompressColor8 output, packed
   itof0          \output, \packed
   loi            0.00392156862745098
   mul            \output, \output, i
   .endm

;//--------------------------------------------------------------------
;// DecompressUV16 - V2_16 signed unpack (only u,v are real stream
;// data) to float ST (/256, ~+-128.0 range; VU's ITOF has no /256
;// shift variant, so scale explicitly off the raw int via itof0, same
;// as DecompressNormal8/DecompressColor8). Like the V3 position case,
;// the unpack hardware's z/w lanes are indeterminate; z is the GS Q
;// baseline and every loader always set texcoords.z=1.0, so restore it
;// deterministically off vf00 (hardwired 0,0,0,1) the same way
;// LoadCullScale sets known lanes off a garbage destination. w is
;// never read downstream (only xyz gets stored to the GS ST/Q regs).
;//--------------------------------------------------------------------
   .macro   DecompressUV16 output, packed
   itof0          \output, \packed
   loi            0.00390625
   mul            \output, \output, i
   loi            1.0
   add.z          \output, vf00, i
   .endm
