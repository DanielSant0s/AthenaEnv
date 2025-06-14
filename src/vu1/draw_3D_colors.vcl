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

    lq             st_offset, TEXCOORD_OFFSET(vi00)

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
    iaddiu  vertexData,     iBase,      2           ; pointer to vertex data

    iadd    colorData,      vertexData, vertCount   ; pointer to stq
    iadd    stqData,        colorData, vertCount   ; pointer to stq
    iadd    kickAddress,    stqData,  vertCount       ; pointer for XGKICK
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
        lq vertex,  0(vertexData)  
        lq inColor, 0(colorData)  
        lq stq,     0(stqData)          
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

        iaddiu          vertexData,     vertexData,     1    
        iaddiu          colorData,      colorData,      1                        
        iaddiu          stqData,        stqData,        1   

        iaddiu          destAddress,    destAddress,    3

        iaddi   vertexCounter,  vertexCounter,  -1	; decrement the loop counter 
        ibne    vertexCounter,  vi00,   vertexLoop	; and repeat if needed

    ;//////////////////////////////////////////// 

    xgkick kickAddress ; dispatch to the GS rasterizer.

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

    iaddiu  vertexData,  iBase,      2           ; pointer to vertex data
    iadd    colorData,   vertexData,    vertCount   ; pointer to colors
    iadd    stqData,     colorData,     vertCount   ; pointer to colors

    iaddiu     kickAddress,    vertexData, INBUF_SIZE
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
        lq inVert, 0(vertexData)   
        lq inColor, 0(colorData)    
        lq stq,    0(stqData)       
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

        iaddiu          vertexData,     vertexData,        1     
        iaddiu          colorData,       colorData,        1                       
        iaddiu          stqData,           stqData,        1   

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
