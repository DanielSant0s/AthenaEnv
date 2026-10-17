; 2024 - Daniel Santos
; AthenaEnv Renderer

.syntax new
.name VU1Draw3DLCS_Ref
.vu
.init_vf_all
.init_vi_all

.include "vu1/include/mem_layout.i"
.include "vu1/include/athena_consts.i"
.include "vu1/include/athena_macros.i"
.include "vu1/include/vcl_sml.i"

; Clip queue hooks (vu1/proc/clip_queue_pass.i).
.macro ClipQueueReadEntry
    ; Entry = input address of the triangle's last vertex (UV slot w lane); its
    ; output address follows from the vertex index: kick + 1 + 3 * index.
    ilw.w                qVert, TEXCOORD_OFFSET(qPtr)
    xtop                 qBase
    isub                 qOut,  qVert, qBase
    iadd                 qTmp,  qOut,  qOut
    iadd                 qOut,  qTmp,  qOut
    iadd                 qOut,  qOut,  qBase
    iaddiu               qOut,  qOut,  INBUF_SIZE
    iaddiu               qOut,  qOut,  4
.endm
.macro ClipQueueLoadTriangle
    lq                   CSVertex1, POSITION_OFFSET-2(qVert)
    lq                   CSVertex2, POSITION_OFFSET-1(qVert)
    lq                   CSVertex3, POSITION_OFFSET(qVert)
    DecompressPositionW  CSVertex1
    DecompressPositionW  CSVertex2
    DecompressPositionW  CSVertex3
    MatrixMultiplyVertex CSVertex1, ObjectToScreen, CSVertex1
    MatrixMultiplyVertex CSVertex2, ObjectToScreen, CSVertex2
    MatrixMultiplyVertex CSVertex3, ObjectToScreen, CSVertex3
.endm
.macro ClipQueueLoadSTQ
    ; Reflection coordinates, stored unprojected by the vertex loop.
    VectorLoad           TempSTQ1, TEXCOORD_OFFSET-2, qVert
    VectorLoad           TempSTQ2, TEXCOORD_OFFSET-1, qVert
    VectorLoad           TempSTQ3, TEXCOORD_OFFSET-0, qVert
.endm
  

--enter
--endenter
    ;//////////// --- Load data 1 --- /////////////
    ; Updated once per mesh
    MatrixLoad	ScreenMatrix, SCREEN_MATRIX, vi00 ; load view-projection matrix
    MatrixLoad	ObjectMatrix, OBJECT_MATRIX, vi00 ; load object matrix

    MatrixMultiply   ObjectToScreen, ObjectMatrix, ScreenMatrix

    ; w = culling direction (+1 back, -1 front, 0 off), x = winding flip:
    ; -1 for a tristrip, whose winding alternates every vertex, else +1.
    lq.xw           bfc_multiplier, CLIPFAN_OFFSET(vi00)

    ftoi0.w        bfc_sign_mask, bfc_multiplier
    mtir           z_sign_mask, bfc_sign_mask[w]

    ibeq           z_sign_mask, vi00, ignore_face_culling 

    iaddiu          z_sign_mask, vi00, 0x20
ignore_face_culling:

    lq scale, SCREEN_SCALE(vi00) 

    AddScreenOffset scale

    move vector, vf00
    move oldvector, vf00
    move vertex2, vf00
    move vertex3, vf00

    ;/////////////////////////////////////////////

	fcset   0x000000	; VCL won't let us use CLIP without first zeroing
				; the clip flags

    ; Mask for the texture flag bake_giftags stuffs into the prim giftag NLOOP
    ; field. NLOOP occupies bits 0..14 and EOP sits at bit 15, so ilw.x on the
    ; giftag always comes back with bit 15 set and has to be masked before it
    ; can be tested. Set before the branch below, because BOTH paths need it.
    iaddiu  texMask, vi00, 1


cull_init:
    LoadCullScale clip_scale, 0.5

    ;//////////// --- Load data 1 --- /////////////
    ; Updated once per mesh

    ;//////////// --- Load data 2 --- /////////////
    ; Updated dynamically
culled_init:
    xtop    iBase
    xitop   vertCount

    ; --- clip queue setup (see vu1/proc/clip_queue_pass.i) ---
    ; listPtr: next free list entry. kick0..2: rotating "this vertex completes a
    ; triangle" pattern -- 0,0,1 repeating for a triangle list, 0,0,1,1,1... for a
    ; strip -- and all zero when clipping is off for this chunk, so nothing is queued.
    iadd      listPtr, iBase, vi00
    ilw.y     clipOn,  CLIPFAN_OFFSET(vi00)
    ilw.z     isStrip, CLIPFAN_OFFSET(vi00)
    iadd      kick2,   vi00, vi00
    ibeq      clipOn,  vi00, clipq_setup_off
    iaddiu    kick2,   vi00, 1
clipq_setup_off:
    iand      stripOr,  isStrip, kick2
    iaddiu    notStrip, vi00, 1
    isub      notStrip, notStrip, isStrip
    ; Kept in VU memory, not registers: this vertex loop has inner loops, and
    ; with the queue state live across them VCL runs out of registers.
    isw.x     vi00,     CLIPQ_STATE(vi00)    ; kick0
    isw.y     vi00,     CLIPQ_STATE(vi00)    ; kick1
    isw.z     kick2,    CLIPQ_STATE(vi00)
    isw.w     listPtr,  CLIPQ_STATE(vi00)
    isw.z     notStrip, CLIPQ_SAVE(vi00)
    isw.w     stripOr,  CLIPQ_SAVE(vi00)
    ilw.w       pntLightQnt,    NUM_POINT_LIGHTS(vi00) ; load active point lights
    lq.w           bfc_multiplier, CLIPFAN_OFFSET(vi00) ; restart strip parity

    lq      primTag,        GIFTAG_OFFSET(iBase) ; GIF tag - tell GS how many data we will send
    lq      matDiffuse,     DIFF_MAT_OFFSET(iBase) ; RGBA 
    
    ; texGiftagAddr: 3 QW of AD giftag (header + TEX0 + TEX1) written by the EE
    ; via render_emit_tex_giftag, EOP=1, a complete standalone packet. Sent on
    ; its own BEFORE the vertex loop, never chained onto the vertex XGKICK: the
    ; accurate-clipping path can fire mid-loop XGKICKs that carry no texture
    ; state of their own and draw with whatever the GS has latched.
    ;
    ; Skipped entirely for an untextured chunk -- the EE does not emit the block
    ; at all in that case, so kicking it would send stale VU memory to the GS.
    ; bake_giftags flags this in the prim giftag NLOOP field.
    iaddiu    texGiftagAddr,  iBase,  INBUF_SIZE
    ilw.x     texEnabled,     GIFTAG_OFFSET(iBase)
    iand      texEnabled,     texEnabled, texMask
    ibeq      texEnabled,     vi00,   no_tex_kick_cull
    xgkick    texGiftagAddr
no_tex_kick_cull:

    iaddiu    kickAddress,    texGiftagAddr,  3       ; pointer for XGKICK
    iaddiu    destAddress,    kickAddress,  1       ; helper pointer for data inserting
    ;////////////////////////////////////////////

    ;/////////// --- Store tags --- /////////////
    sq primTag,    0(kickAddress) ; prim + tell gs how many data will be

    ; Set the GifTag EOP bit to 1 and NLOOP to the number of vertices
    iaddiu               Mask, vertCount, 0x7fff
    iaddiu               Mask, Mask, 0x01
    isw.x                Mask, 0(kickAddress)
    ;////////////////////////////////////////////

    ;/////////////// --- Loop --- ///////////////
    iadd vertexCounter, vi00, vertCount ; loop vertCount times
    vertexLoop:

        ;////////// --- Load loop data --- //////////
        lq inVert,       POSITION_OFFSET(iBase)
        DecompressPositionW inVert

        lq inNormPacked, NORMAL_OFFSET(iBase)
        DecompressNormal8 inNorm, inNormPacked

        lq inColorPacked, COLOR_OFFSET(iBase)
        DecompressColor8 inColor, inColorPacked
        ;////////////////////////////////////////////

        ;////////////// --- Vertex --- //////////////
        MatrixMultiplyVertex	vertex, ObjectToScreen, inVert ; transform each vertex by the matrix

        mul clip_vertex, vertex, clip_scale

        clipw.xyz	clip_vertex, clip_vertex	
        fcand		VI01,   0x3FFFF  
        iaddiu		iADC,   VI01,       0x7FFF 
        
        div         q,      vf00[w],    vertex[w]   ; perspective divide (1/vert[w]):

        mul.xyz     vertex, vertex,     q

        mul.xyz    vertex, vertex,     scale
        add.xyz    vertex, vertex,     offset

        move vertex2, vertex3       
        move vertex3, vertex

        move.xyz	oldvector, vector

	    sub.xyz		vector, vertex3, vertex2

	    opmula.xyz	acc, vector, oldvector
	    opmsub.xyz	crossproduct, oldvector, vector

	    ; The direction factor scales the RESULT, not an edge: oldvector is last
	    ; iteration edge and would carry last iteration factor, so with a factor
	    ; that alternates (tristrip) the two would cancel out instead of flipping.
	    mulw.z	crossproduct, crossproduct, bfc_multiplier

	    fmand		z_sign, z_sign_mask
        ; z_sign = 0x20 when the cross product is negative (back-facing): +0x7FE0
        ; carries that into bit 15 of w, the GS ADC bit. Was 0xFFE0 -- inverted, and
        ; over 15 bits, so VCL skipped the line outright and culling never ran.
        iaddiu		z_sign, z_sign, 0x7FE0
        ior        iADC, iADC, z_sign

        ; Strip restart: the first two vertices after one carry a negative
        ; packed normal w (ATHENA_STRIP_NO_KICK), which mtir returns as 0xFFxx --
        ; bit 15, the ADC bit, so the triangle they complete is never drawn.
        ; Ordinary vertices have w = 127: only bits below 15, which the GS ignores.
        mtir       noKick, inNormPacked[w]
        ior        iADC, iADC, noKick

        ; Clip queue: a triangle the test above rejected (VI01 = some vertex outside
        ; the guard band or the near/far planes) that would otherwise be drawn --
        ; this vertex completes one (kick0) and is not a strip restart (bit 0 of the
        ; packed normal w, see ATHENA_STRIP_NO_KICK; lists have no restarts) -- is
        ; appended for clip_queue_pass.i. Always stored, advanced only when queued.
        ilw.x      kick0,    CLIPQ_STATE(vi00)
        ilw.w      listPtr,  CLIPQ_STATE(vi00)
        ilw.z      notStrip, CLIPQ_SAVE(vi00)
        ilw.w      stripOr,  CLIPQ_SAVE(vi00)
        ior        qOk,     noKick,  notStrip
        iand       qOk,     qOk,     kick0
        iand       qInc,    qOk,     VI01
        isw.w      iBase,       TEXCOORD_OFFSET(listPtr) ; UV w lane: never read
        iadd       listPtr, listPtr, qInc
        isw.w      listPtr,  CLIPQ_STATE(vi00)
        ior        kickNext, kick0,  stripOr
        ilw.y      kick1,    CLIPQ_STATE(vi00)
        ilw.z      kick2,    CLIPQ_STATE(vi00)
        isw.x      kick1,    CLIPQ_STATE(vi00)
        isw.y      kick2,    CLIPQ_STATE(vi00)
        isw.z      kickNext, CLIPQ_STATE(vi00)

        ; Next vertex of a tristrip is wound the other way round; x is -1 there
        ; and +1 for a triangle list, where this is a no-op.
        mulx.w        bfc_multiplier, bfc_multiplier, bfc_multiplier
        
        mfir.w		vertex, iADC
        ftoi4.xy    vertex, vertex
        ftoi0.z     vertex, vertex
        ;////////////////////////////////////////////

        ;//////////////// --- ST & NORMALS --- ////////////////
        MatrixMultiplyVector	normal, ObjectMatrix, inNorm
        VectorNormalize normal, normal

        sub.xyzw tmp_stq, vf00, vf00
        subx.x tmp_stq, normal, vf00[x]
        suby.y tmp_stq, tmp_stq, normal[y]
        addw.xyz tmp_stq, tmp_stq, vf00[w]

        loi 0.5
        muli.xy tmp_stq, tmp_stq, I

        sq.xyz     tmp_stq, TEXCOORD_OFFSET(iBase) ; for the clip queue
        mulq modStq, tmp_stq, q
        ;////////////////////////////////////////////
        
        lq light, LIGHT_AMBIENT_SUM(vi00)  ; xyz = summed ambient, w = 1.0
        move intensity, vf00

        ; All directional lights in one matrix multiply (see DirLightsMatrix).
        DirLightsLoad
        DirLightsMatrix light, inNorm

        iadd  currPntLight, vi00, vi00
        ibeq  pntLightQnt, vi00, skip_culled_pointLightsLoop
        culled_pointLightsLoop:
            lq PointPos, POINT_LIGHT_POS_PTR(currPntLight)
            sub.xyz toLight, PointPos, inVert
            mul.xyz distSq, toLight, toLight
            add.x distSq, distSq, distSq[y]
            add.x distSq, distSq, distSq[z]
            loi 0.0001
            max.x distSq, distSq, i
            rsqrt q, vf00[w], distSq[x]
            mul.xyz toLight, toLight, q

            VectorDotProduct intensity, inNorm, toLight
            maxx.xyzw intensity, intensity, vf00

            lq PointDiffuse, POINT_LIGHT_DIFF_PTR(currPntLight)
            mul.w attenDenom, PointDiffuse, distSq[x]
            add.w attenDenom, vf00, attenDenom
            div q, vf00[w], attenDenom[w]
            mul.x intensity, intensity, q

            mul diffuse, PointDiffuse, intensity[x]
            add.xyz light, light, diffuse

            iaddiu currPntLight, currPntLight, 1
            ibne   pntLightQnt, currPntLight, culled_pointLightsLoop
        skip_culled_pointLightsLoop:

        add.xyzw   color, matDiffuse, inColor
        mul    color, color,      light       ; color = color * light

        VectorNormalizeClamp color, color
        loi 128.0
        mul color, color, i                        ; normalize RGBA
        ColorFPtoGsRGBAQ intColor, color           ; convert to int
        ;///////////////////////////////////////////


        ;//////////// --- Store data --- ////////////
        sq.xyz modStq,      STQ(destAddress)     
        sq intColor,    RGBA(destAddress)     ; q is grabbed from stq
        sq vertex,  XYZ2(destAddress)    
        ;////////////////////////////////////////////

        iaddiu          destAddress,    destAddress,    3

    skip_rendering:
        iaddiu          iBase,     iBase,     1                            

        iaddi   vertexCounter,  vertexCounter,  -1	; decrement the loop counter 
        ibgtz    vertexCounter,  vertexLoop	; and repeat if needed

    ;//////////////////////////////////////////// 

culled_kick:
    ilw.w     listPtr,  CLIPQ_STATE(vi00)
    .include "vu1/proc/clip_queue_pass.i"

    ; kickAddress is rebuilt rather than kept alive through the clip queue pass,
    ; which needs every integer register it can get.
    xtop      kickBase
    iaddiu    kickAddress, kickBase,    INBUF_SIZE
    iaddiu    kickAddress, kickAddress, 3
    xgkick    kickAddress ; dispatch to the GS rasterizer.

--barrier
--cont

    b culled_init


.include "vu1/proc/save_last_loop.i"

.include "vu1/proc/scissor_interpolation.i"

--exit
--endexit
