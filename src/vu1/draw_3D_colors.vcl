; 2024 - Daniel Santos
; AthenaEnv Renderer

.syntax new
.name VU1Draw3DCS
.vu
.init_vf_all
.init_vi_all

.include "vu1/include/mem_layout.i"
.include "vu1/include/athena_consts.i"
.include "vu1/include/athena_macros.i"
.include "vu1/include/vcl_sml.i"


--enter
--endenter
    ;//////////// --- Load data 1 --- /////////////
    ; Updated once per mesh
    MatrixLoad	ScreenMatrix, SCREEN_MATRIX, vi00 ; load view-projection matrix
    MatrixLoad	ObjectMatrix, OBJECT_MATRIX, vi00 ; load object matrix

    MatrixMultiply   ObjectToScreen, ObjectMatrix, ScreenMatrix

    lq             st_offset, BUMP_OFFSET(vi00)

    lq.w           bfc_multiplier, CLIPFAN_OFFSET(vi00)

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

    ilw.y       accurateClipping,    CLIPFAN_OFFSET(vi00)
    ibne vi00,  accurateClipping, scissor_init

cull_init:
    LoadCullScale clip_scale, 0.5

    ;//////////// --- Load data 2 --- /////////////
    ; Updated dynamically
culled_init:
    xtop    iBase
    xitop   vertCount

    lq      primTag,        0(iBase) ; GIF tag - tell GS how many data we will send
    lq      matDiffuse,     1(iBase) ; RGBA
                                     ; u32 : R, G, B, A (0-128)

    ; texGiftagAddr: 3 QW written directly by the EE (an AD giftag header +
    ; TEX0 + TEX1, see render_build_chain tex_giftag unpack -- EOP=1,
    ; a complete standalone packet), landing in the same double-buffered
    ; window this chunk other input data does. Sent on its own, right now,
    ; BEFORE the vertex loop -- not chained onto kickAddress below. That
    ; ordering is required, not just tidy: a scissored triangle further down
    ; can fire its own mid-loop XGKICK (see process_scissor_clip_offset.i)
    ; that carries no texture state of its own, only whatever TEX0/TEX1 the
    ; GS already has latched -- so this chunk texture state must reach the
    ; GS before that can happen. Same PATH1, so XGKICK order is GS order.
    iaddiu    texGiftagAddr,    iBase,  INBUF_SIZE
    xgkick    texGiftagAddr

    ; kickAddress = where WE store our own PRIM giftag + vertex stream, same
    ; as always -- untouched by the texGiftagAddr split above, so
    ; process_scissor_clip_offset.i keeps working unmodified.
    iaddiu    kickAddress,    texGiftagAddr,  3
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
        lq vertex,  POSITION_OFFSET(iBase)
        DecompressPositionW vertex

        lq inColorPacked, COLOR_OFFSET(iBase)
        DecompressColor8 inColor, inColorPacked

        lq stqPacked, TEXCOORD_OFFSET(iBase)
        DecompressUV16 stq, stqPacked
        ;////////////////////////////////////////////


        ;////////////// --- Vertex --- //////////////
        MatrixMultiplyVertex	vertex, ObjectToScreen, vertex ; transform each vertex by the matrix
       
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

        mulw.xyz       vector, vector, bfc_multiplier
	    opmula.xyz	acc, vector, oldvector
	    opmsub.xyz	crossproduct, oldvector, vector

	    fmand		z_sign, z_sign_mask
        iaddiu		z_sign, z_sign, 0xFFE0
        ior        iADC, iADC, z_sign
        
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
                 
        iaddiu          iBase,        iBase,        1   

        iaddiu          destAddress,    destAddress,    3

        iaddi   vertexCounter,  vertexCounter,  -1	; decrement the loop counter 
        ibne    vertexCounter,  vi00,   vertexLoop	; and repeat if needed

    ;////////////////////////////////////////////

    xgkick kickAddress ; dispatch our primTag+vertices. Texture state already
                        ; went out earlier, standalone, via texGiftagAddr.

--barrier
--cont

    b culled_init

scissor_init:
    iaddiu               StackPtr, vi00, STACK_OFFSET

    .include "vu1/proc/setup_clip_trigger.i"

    ;//////////// --- Load data 2 --- /////////////
    ; Updated dynamically
init:
    xtop    iBase
    xitop   vertCount

    lq      primTag,        0(iBase) ; GIF tag - tell GS how many data we will send
    lq      matDiffuse,     1(iBase) ; material diffuse color

    ; See the comment in culled_init: texGiftagAddr holds the EE-written AD
    ; giftag (TEX0/TEX1) and gets XGKICKed standalone, right now, before this
    ; triangle vertex processing starts. That ordering matters MORE here
    ; than in culled_init: this is the accurate-clipping path, and a
    ; scissored triangle below can fire its own mid-loop XGKICK (see
    ; process_scissor_clip_offset.i "triangle fan" section) that carries no
    ; texture state of its own -- it draws with whatever TEX0/TEX1 the GS
    ; already has latched. Sending texGiftagAddr up front, before any of that
    ; can run, is what keeps a scissored triangle from drawing with the
    ; PREVIOUS chunk texture.
    iaddiu     texGiftagAddr,    iBase, INBUF_SIZE
    xgkick     texGiftagAddr

    ; kickAddress = where we store our own PRIM giftag chain, same as always.
    iaddiu     kickAddress,    texGiftagAddr,  3
    ;////////////////////////////////////////////

    ;/////////// --- Store tags --- /////////////
    sq primTag,    0(kickAddress) ; prim + tell gs how many data will be

    ; Set the GifTag EOP bit to 1 and NLOOP to the number of vertices
    iaddiu               Mask, vertCount, 0x7fff
    iaddiu               Mask, Mask, 0x01
    isw.x                Mask, 0(kickAddress)
    ;////////////////////////////////////////////

    iaddiu    outputAddress,    kickAddress,  1       ; helper pointer for data inserting

    .include "vu1/proc/setup_clip_flags.i"

    .include "vu1/proc/setup_vertex_queue.i"

    iadd vertexCounter, vi00, vertCount ; loop vertCount times

    loop:

        ;////////// --- Load loop data --- //////////
        lq inVert, POSITION_OFFSET(iBase)
        DecompressPositionW inVert

        lq inColorPacked, COLOR_OFFSET(iBase)
        DecompressColor8 inColor, inColorPacked

        lq stqPacked, TEXCOORD_OFFSET(iBase)
        DecompressUV16 stq, stqPacked
        ;////////////////////////////////////////////


        ;////////////// --- Vertex --- //////////////
        MatrixMultiplyVertex	vertex, ObjectToScreen, inVert ; transform each vertex by the matrix
        move formVertex, vertex

        add.xy stq, stq, st_offset
        VertexPersCorrST vertex, modStq, vertex, stq

        mul.xyz    vertex, vertex,     scale
        add.xyz    vertex, vertex,     offset

        move vertex1, vertex2
        move vertex2, vertex3       

        move vertex3, vertex
 
        VertexFpToGsXYZ2  vertex,vertex
        ;////////////////////////////////////////////

        ;//////////////// - COLORS - /////////////////
        add.xyzw    color, matDiffuse, inColor
        VectorNormalizeClamp color, color
        loi 128.0 
        mul color, color, i                        ; normalize RGBA
        ColorFPtoGsRGBAQ intColor, color           ; convert to int
        ;///////////////////////////////////////////


        ;//////////// --- Store data --- ////////////
        sq.xyz modStq,      STQ(outputAddress)      
        sq intColor,    RGBA(outputAddress)     ; q is grabbed from stq
        sq vertex,      XYZ2(outputAddress)     
        ;////////////////////////////////////////////

        .include "vu1/proc/process_scissor_clip_offset.i"
                  
        iaddiu          iBase,           iBase,        1   

        iaddiu          outputAddress,  outputAddress,  3

        iaddi   vertexCounter,  vertexCounter,  -1	; decrement the loop counter 
        ibne    vertexCounter,  vi00,   loop	; and repeat if needed

    ;//////////////////////////////////////////// 

    xgkick kickAddress ; dispatch to the GS rasterizer.

--barrier
--cont

    b init

.include "vu1/proc/save_last_loop.i"

.include "vu1/proc/scissor_interpolation.i"

--exit
--endexit
