; 2024 - Daniel Santos
; AthenaEnv Renderer

.syntax new
.name VU1Draw3DCS_Skin
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
    ilw.w                qVert, SKINNED_TEXCOORD_OFFSET(qPtr)
    xtop                 qBase
    isub                 qOut,  qVert, qBase
    iadd                 qTmp,  qOut,  qOut
    iadd                 qOut,  qTmp,  qOut
    iadd                 qOut,  qOut,  qBase
    iaddiu               qOut,  qOut,  SKINNED_INBUF_SIZE
    iaddiu               qOut,  qOut,  4
.endm
.macro ClipQueueLoadTriangle
    ; Stored by the vertex loop (clip space, after skinning): recomputing would
    ; mean skinning the three vertices again.
    lq                   CSVertex1, SKINNED_POSITION_OFFSET-2(qVert)
    lq                   CSVertex2, SKINNED_POSITION_OFFSET-1(qVert)
    lq                   CSVertex3, SKINNED_POSITION_OFFSET(qVert)
.endm
.macro ClipQueueLoadSTQ
    VectorLoad           TempSTQ1Packed, SKINNED_TEXCOORD_OFFSET-2, qVert
    DecompressUV16       TempSTQ1, TempSTQ1Packed
    VectorLoad           TempSTQ2Packed, SKINNED_TEXCOORD_OFFSET-1, qVert
    DecompressUV16       TempSTQ2, TempSTQ2Packed
    VectorLoad           TempSTQ3Packed, SKINNED_TEXCOORD_OFFSET-0, qVert
    DecompressUV16       TempSTQ3, TempSTQ3Packed
    add.xy               TempSTQ1, TempSTQ1, st_offset
    add.xy               TempSTQ2, TempSTQ2, st_offset
    add.xy               TempSTQ3, TempSTQ3, st_offset
.endm



--enter
--endenter
    ;//////////// --- Load data 1 --- /////////////
    ; Updated once per mesh
    MatrixLoad	ScreenMatrix, SCREEN_MATRIX, vi00 ; load view-projection matrix
    MatrixLoad	ObjectMatrix, OBJECT_MATRIX, vi00 ; load object matrix

    MatrixMultiply   ObjectToScreen, ObjectMatrix, ScreenMatrix

    lq             st_offset, BUMP_OFFSET(vi00)

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


cull_init:
    LoadCullScale clip_scale, 0.5

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
    lq.w           bfc_multiplier, CLIPFAN_OFFSET(vi00) ; restart strip parity

    lq      primTag,        0(iBase) ; GIF tag - tell GS how many data we will send
    lq      matDiffuse,     1(iBase) ; RGBA
                                     ; u32 : R, G, B, A (0-128)
    ; texGiftagAddr: 3 QW written directly by the EE (an AD giftag header +
    ; TEX0 + TEX1, see render_build_chain tex_giftag unpack -- EOP=1,
    ; a complete standalone packet). Sent on its own, right now, BEFORE the
    ; vertex loop -- not chained onto kickAddress below. Required: the
    ; scissored fans of the clip queue pass (clip_queue_pass.i) go out on
    ; their own XGKICKs with no texture state of their own, only whatever
    ; TEX0/TEX1 the GS already has latched.
    iaddiu    texGiftagAddr,    iBase,  SKINNED_INBUF_SIZE
    xgkick    texGiftagAddr

    ; kickAddress = where WE store our own PRIM giftag + vertex stream, same
    ; as always.
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
        lq inVert,  SKINNED_POSITION_OFFSET(iBase)
        DecompressPositionW inVert

        lq inColorPacked, SKINNED_COLOR_OFFSET(iBase)
        lq inNormPacked, SKINNED_NORMAL_OFFSET(iBase) ; w = strip no-kick flag only
        DecompressColor8 inColor, inColorPacked

        lq stqPacked, SKINNED_TEXCOORD_OFFSET(iBase)
        DecompressUV16 stq, stqPacked
        ;////////////////////////////////////////////

        lq boneIndices,    SKINNED_INDICES_OFFSET(iBase)
        lq weightTest,     SKINNED_WEIGHTS_OFFSET(iBase)
        itof12 boneWeights, weightTest

        move final_vertex, vf00

        skinWeightLoop_cull:
            mtir           boneIndex, boneIndices[x]

            MatrixLoad	BoneMatrix, BONE_MATRICES, boneIndex
  
            MatrixMultiplyVertex ts_vertex, BoneMatrix, inVert

            mul ts_vertex, ts_vertex, boneWeights[x]

            add final_vertex, final_vertex, ts_vertex

            mr32 boneIndices, boneIndices
            mr32 boneWeights, boneWeights

            sub.x weightTest, weightTest, weightTest
            mr32  weightTest, weightTest

            mtir  weightBits, weightTest[x]
            ibne  weightBits, vi00,  skinWeightLoop_cull


        ;////////////// --- Vertex --- //////////////
        MatrixMultiplyVertex	vertex, ObjectToScreen, final_vertex ; transform each vertex by the matrix
        sq         vertex, SKINNED_POSITION_OFFSET(iBase) ; clip-space copy for the clip queue
       
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
        isw.w      iBase,       SKINNED_TEXCOORD_OFFSET(listPtr) ; UV w lane: never read
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


        ;//////////////// --- ST --- ////////////////
        add.xy stq, stq, st_offset
        mulq modStq, stq, q
        ;////////////////////////////////////////////

        ;//////////////// - COLORS - /////////////////
        add.xyzw    color, matDiffuse, inColor
        VectorNormalizeClamp color, color
        loi 128.0 
        mul color, color, i                   ; normalize RGBA
        ColorFPtoGsRGBAQ intColor, color           ; convert to int
        ;///////////////////////////////////////////


        ;//////////// --- Store data --- ////////////
        sq.xyz modStq,      STQ(destAddress)      
        sq intColor,    RGBA(destAddress)    ; q is grabbed from stq
        sq vertex,  XYZ2(destAddress)     
        ;////////////////////////////////////////////

        iaddiu      iBase,     iBase,     1    

        iaddiu          destAddress,    destAddress,    3

        iaddi   vertexCounter,  vertexCounter,  -1	; decrement the loop counter 
        ibne    vertexCounter,  vi00,   vertexLoop	; and repeat if needed

    ;////////////////////////////////////////////

culled_kick:
    ilw.w     listPtr,  CLIPQ_STATE(vi00)
    .include "vu1/proc/clip_queue_pass.i"

    ; kickAddress is rebuilt rather than kept alive through the clip queue pass,
    ; which needs every integer register it can get.
    xtop      kickBase
    iaddiu    kickAddress, kickBase,    SKINNED_INBUF_SIZE
    iaddiu    kickAddress, kickAddress, 3
    xgkick    kickAddress ; dispatch to the GS rasterizer.

--barrier
--cont

    b culled_init


.include "vu1/proc/save_last_loop.i"

.include "vu1/proc/scissor_interpolation.i"

--exit
--endexit
