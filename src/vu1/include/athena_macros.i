
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
;// DirLightsMatrix - all 4 directional lights in one go, no loop.
;//
;// LDirX/Y/Z are LIGHT_DIR_T (the light directions transposed), so
;// the multiply below leaves Li.N in lane i. Clamped at 0, each lane
;// weights the diffuse colour of its light (LIGHT_DIFFUSE_PTR rows LDiff0..3)
;// on top of the ambient already in \light. Lights that are not active
;// have zero direction and zero colour, so the cost is constant and,
;// unlike the per-light loop, there is no branch: a vertex loop built
;// on this can be software-pipelined by VCL.
;//
;// \light comes in holding the ambient sum (w = 1.0, left untouched).
;//--------------------------------------------------------------------
   .macro   DirLightsMatrix light, normal
   mul            acc,       LDirX,  \normal[x]
   madd           acc,       LDirY,  \normal[y]
   madd           dirDots,   LDirZ,  \normal[z]
   maxx.xyzw      dirDots,   dirDots, vf00
   mul.xyz        acc,       \light, vf00[w]
   madd.xyz       acc,       LDiff0, dirDots[x]
   madd.xyz       acc,       LDiff1, dirDots[y]
   madd.xyz       acc,       LDiff2, dirDots[z]
   madd.xyz       \light,    LDiff3, dirDots[w]
   .endm

;//--------------------------------------------------------------------
;// PointLightsSoA - all 4 point lights in one go, no loop.
;//
;// The light positions come transposed (POINT_LIGHT_SOA), so lane i of
;// dx/dy/dz is light i minus the vertex. Per light:
;//
;//   max(N.L, 0) / (1 + k d^2), L normalised
;//   = max(N.Lu, 0) * rsqrt(d^2 * (1 + k d^2)^2), Lu unnormalised
;//
;// so one rsqrt per light replaces the rsqrt + div of the loop version
;// (two on the FDIV unit, two on the EFU, which work in parallel).
;// Unused lights have zero colour. Branch-free, like DirLightsMatrix.
;// \vert and \normal are object space; \light accumulates (xyz).
;//--------------------------------------------------------------------
   .macro   PointLightsSoA light, vert, normal
   lq             PntX,      POINT_LIGHT_SOA+0(vi00)
   lq             PntY,      POINT_LIGHT_SOA+1(vi00)
   lq             PntZ,      POINT_LIGHT_SOA+2(vi00)
   lq             PntK,      POINT_LIGHT_SOA+3(vi00)
   sub            pdx,       PntX,   \vert[x]
   sub            pdy,       PntY,   \vert[y]
   sub            pdz,       PntZ,   \vert[z]
   mul            acc,       pdx,    \normal[x]
   madd           acc,       pdy,    \normal[y]
   madd           pndl,      pdz,    \normal[z]
   maxx.xyzw      pndl,      pndl,   vf00
   mul            acc,       pdx,    pdx
   madd           acc,       pdy,    pdy
   madd           pd2,       pdz,    pdz
   loi            0.0001
   maxi           pd2,       pd2,    i
   mul            patt,      PntK,   pd2
   add            patt,      patt,   vf00[w]
   mul            patt,      patt,   patt
   mul            patt,      patt,   pd2
   rsqrt          q,         vf00[w], patt[x]
   addq.x         pinv,      vf00,   q
   rsqrt          q,         vf00[w], patt[y]
   addq.y         pinv,      vf00,   q
   ; Lights 2 and 3 on the EFU instead: it runs alongside the FDIV unit, which
   ; already has the other two plus the perspective divide to get through.
   ersqrt         p,         patt[z]
   mfp.z          pinv,      p
   ersqrt         p,         patt[w]
   mfp.w          pinv,      p
   mul            pndl,      pndl,   pinv
   lq             PntDiff0,  POINT_LIGHT_DIFF_PTR+0(vi00)
   lq             PntDiff1,  POINT_LIGHT_DIFF_PTR+1(vi00)
   lq             PntDiff2,  POINT_LIGHT_DIFF_PTR+2(vi00)
   lq             PntDiff3,  POINT_LIGHT_DIFF_PTR+3(vi00)
   mul.xyz        acc,       \light, vf00[w]
   madd.xyz       acc,       PntDiff0, pndl[x]
   madd.xyz       acc,       PntDiff1, pndl[y]
   madd.xyz       acc,       PntDiff2, pndl[z]
   madd.xyz       \light,    PntDiff3, pndl[w]
   .endm

;// Loads the 7 constant rows DirLightsMatrix reads. Hoisted out of the
;// vertex loop where registers allow it.
   .macro   DirLightsLoad
   lq             LDirX,     LIGHT_DIR_T+0(vi00)
   lq             LDirY,     LIGHT_DIR_T+1(vi00)
   lq             LDirZ,     LIGHT_DIR_T+2(vi00)
   lq             LDiff0,    LIGHT_DIFFUSE_PTR+0(vi00)
   lq             LDiff1,    LIGHT_DIFFUSE_PTR+1(vi00)
   lq             LDiff2,    LIGHT_DIFFUSE_PTR+2(vi00)
   lq             LDiff3,    LIGHT_DIFFUSE_PTR+3(vi00)
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
