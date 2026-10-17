
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

;//--------------------------------------------------------------------
;// DirSpecularSoA - Blinn-Phong specular of all 4 directional lights at
;// once, no loop. Same result as the per-light HalfAngle/SpecularPowerScale
;// loop it replaces:
;//
;//   spec_i = max(N . normalize(L_i + V), 0)^16 * LightSpecular_i
;//
;// With LDirX/Y/Z (from DirLightsLoad) lane i of hx/hy/hz is L_i + V, so
;// N.H and |H|^2 come out for the 4 lights in 3 FMACs each. The 4 1/|H|
;// are split 2 on the FDIV unit and 2 on the EFU, which run in parallel,
;// and every light shares their latency instead of paying it in turn.
;// Unused lights have zero specular colour. \light accumulates (xyz).
;//--------------------------------------------------------------------
   .macro   DirSpecularSoA light, normal, view
   add            shx,       LDirX,  \view[x]
   add            shy,       LDirY,  \view[y]
   add            shz,       LDirZ,  \view[z]
   mul            acc,       shx,    \normal[x]
   madd           acc,       shy,    \normal[y]
   madd           sndh,      shz,    \normal[z]
   mul            acc,       shx,    shx
   madd           acc,       shy,    shy
   madd           shh,       shz,    shz
   loi            0.000001
   maxi           shh,       shh,    i              ; L = -V exactly: no 1/0
   rsqrt          q,         vf00[w], shh[x]
   addq.x         shinv,     vf00,   q
   rsqrt          q,         vf00[w], shh[y]
   addq.y         shinv,     vf00,   q
   ersqrt         p,         shh[z]
   mfp.z          shinv,     p
   ersqrt         p,         shh[w]
   mfp.w          shinv,     p
   mul            sndh,      sndh,   shinv
   maxx.xyzw      sndh,      sndh,   vf00
   mul            sndh,      sndh,   sndh           ; ^2
   mul            sndh,      sndh,   sndh           ; ^4
   mul            sndh,      sndh,   sndh           ; ^8
   mul            sndh,      sndh,   sndh           ; ^16
   lq.xyz         LSpec0,    LIGHT_SPECULAR_PTR+0(vi00)
   lq.xyz         LSpec1,    LIGHT_SPECULAR_PTR+1(vi00)
   lq.xyz         LSpec2,    LIGHT_SPECULAR_PTR+2(vi00)
   lq.xyz         LSpec3,    LIGHT_SPECULAR_PTR+3(vi00)
   mul.xyz        acc,       \light, vf00[w]
   madd.xyz       acc,       LSpec0, sndh[x]
   madd.xyz       acc,       LSpec1, sndh[y]
   madd.xyz       acc,       LSpec2, sndh[z]
   madd.xyz       \light,    LSpec3, sndh[w]
   .endm

;//--------------------------------------------------------------------
;// SpecPerspective / SpecAccumulate - DirSpecularSoA rewritten to use a
;// single FDIV op per vertex, shared with the perspective divide, so a
;// vertex loop built on it can be software-pipelined (VCL finds no loop
;// schedule with the 4 Q ops of DirSpecularSoA). Same Blinn-Phong result:
;//
;// * The specular cosine does not change when H is scaled, so
;//   H = L * |V| + V (|V| from esqrt, on the EFU) instead of L + V/|V|.
;// * spec = cos^16 = (cos^2)^8, cos^2 = (N.H)^2 / |H|^2. With lanes
;//   a,b,c,d = |H_i|^2, three lane rotations give the product of the
;//   OTHER three per lane (bcd, acd, abd, abc), and 1/a = bcd / abcd: one
;//   reciprocal of T = abcd serves all four lights.
;// * That reciprocal is merged with the perspective one: q = 1/(w T),
;//   then 1/w = q T and 1/T = q w.
;//
;// SpecPerspective leaves 1/w in sinvw.x (use it instead of q for the
;// perspective divide and the STQ) and cos^2 per light in sspec;
;// SpecPower16 raises that to cos^16 and SpecAccumulate adds the specular
;// colours into \light. Needs LDirX/Y/Z
;// (DirLightsLoad) and CamPos.
;//--------------------------------------------------------------------
   .macro   SpecPerspective vtx, vert, normal
   sub.xyz        sview,     CamPos, \vert
   mul.xyz        stmp,      sview,  sview
   add.x          stmp,      stmp,   stmp[y]
   add.x          stmp,      stmp,   stmp[z]
   esqrt          p,         stmp[x]
   mfp.x          svlen,     p
   mul            shx,       LDirX,  svlen[x]
   mul            shy,       LDirY,  svlen[x]
   mul            shz,       LDirZ,  svlen[x]
   add            shx,       shx,    sview[x]
   add            shy,       shy,    sview[y]
   add            shz,       shz,    sview[z]
   mul            acc,       shx,    \normal[x]
   madd           acc,       shy,    \normal[y]
   madd           sndh,      shz,    \normal[z]
   maxx.xyzw      sndh,      sndh,   vf00
   mul            sndh,      sndh,   sndh           ; (N.H)^2
   mul            acc,       shx,    shx
   madd           acc,       shy,    shy
   madd           shh,       shz,    shz            ; |H|^2
   loi            0.000001
   maxi           shh,       shh,    i              ; L = -V exactly: no 1/0
   mr32           sr1,       shh
   mr32           sr2,       sr1
   mr32           sr3,       sr2
   mul            sother,    sr1,    sr2
   mul            sother,    sother, sr3            ; product of the other 3 lanes
   mul            stot,      shh,    sother         ; T = abcd, in every lane
   mul.x          swt,       stot,   \vtx[w]
   div            q,         vf00[w], swt[x]        ; 1 / (w T)
   mulq.x         sinvw,     stot,   q              ; 1/w
   mulq.w         sinvt,     \vtx,   q              ; 1/T
   mul            sspec,     sother, sinvt[w]       ; 1/|H_i|^2
   mul            sspec,     sspec,  sndh           ; cos^2
   .endm

;//--------------------------------------------------------------------
;// SpecInfinite - specular with the viewer at infinity: one view vector for
;// the whole object, so each light's half vector is a per-object constant
;// the EE works out (SPEC_HALF_T, transposed, normalised) and N.H for the
;// 4 lights is one matrix multiply, like the diffuse term -- no esqrt, no
;// divide. Leaves cos^2 per light in \spec, like SpecPerspective, for
;// SpecPower16 / SpecAccumulate.
;//--------------------------------------------------------------------
   .macro   SpecInfinite spec, normal
   lq             SHalfX,    SPEC_HALF_T+0(vi00)
   lq             SHalfY,    SPEC_HALF_T+1(vi00)
   lq             SHalfZ,    SPEC_HALF_T+2(vi00)
   mul            acc,       SHalfX, \normal[x]
   madd           acc,       SHalfY, \normal[y]
   madd           \spec,     SHalfZ, \normal[z]
   maxx.xyzw      \spec,     \spec,  vf00
   mul            \spec,     \spec,  \spec          ; cos^2
   .endm

   ; cos^2 -> cos^16 (three squarings), in place.
   .macro   SpecPower16 spec
   mul            \spec,     \spec,  \spec          ; ^4
   mul            \spec,     \spec,  \spec          ; ^8
   mul            \spec,     \spec,  \spec          ; ^16
   .endm

   .macro   SpecAccumulate light, spec
   lq.xyz         LSpec0,    LIGHT_SPECULAR_PTR+0(vi00)
   lq.xyz         LSpec1,    LIGHT_SPECULAR_PTR+1(vi00)
   lq.xyz         LSpec2,    LIGHT_SPECULAR_PTR+2(vi00)
   lq.xyz         LSpec3,    LIGHT_SPECULAR_PTR+3(vi00)
   mul.xyz        acc,       \light, vf00[w]
   madd.xyz       acc,       LSpec0, \spec[x]
   madd.xyz       acc,       LSpec1, \spec[y]
   madd.xyz       acc,       LSpec2, \spec[z]
   madd.xyz       \light,    LSpec3, \spec[w]
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
