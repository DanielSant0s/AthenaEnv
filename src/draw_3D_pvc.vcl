; 2024 - Daniel Santos
; AthenaEnv Renderer
;
; 
;
; 
;---------------------------------------------------------------
; draw_3D_lights.vcl                                                   |
;---------------------------------------------------------------
; A VU1 microprogram to draw 3D object using XYZ2, RGBAQ and ST|
; This program uses double buffering (xtop)                    |
;                                                              |
; Many thanks to:                                              |
; - Dr Henry Fortuna                                           |
; - Jesper Svennevid, Daniel Collin                            |
; - Guilherme Lampert                                          |
;---------------------------------------------------------------

.syntax new
.name VU1Draw3DPVC
.vu
.init_vf_all
.init_vi_all


SCREEN_SCALE        .assign  0
RENDER_FLAGS        .assign  0

SCREEN_MATRIX       .assign  1
LIGHT_MATRIX        .assign  5

CAMERA_POSITION     .assign  9 ; x, y, z
NUM_DIR_LIGHTS      .assign  9 ; w

LIGHT_DIRECTION_PTR .assign 10
LIGHT_AMBIENT_PTR   .assign 14
LIGHT_DIFFUSE_PTR   .assign 18
LIGHT_SPECULAR_PTR  .assign 22

.include "vcl_sml.i"

--enter
--endenter

    ;//////////// --- Load data 1 --- /////////////
    ; Updated once per mesh
    MatrixLoad	ObjectToScreen, SCREEN_MATRIX, vi00 ; load view-projection matrix

    lq scale, SCREEN_SCALE(vi00)

    loi            2048.0
    addi.xy        offset, vf00, i
    add.zw          offset, vf00, vf00

    add.xyz offset, scale, offset
    
    ;/////////////////////////////////////////////

	fcset   0x000000	; VCL won't let us use CLIP without first zeroing
				; the clip flags

    ;//////////// --- Load data 2 --- /////////////
    ; Updated dynamically
init:
    xtop    iBase

    lq      primTag,        0(iBase) ; GIF tag - tell GS how many data we will send
    lq      rgba,           1(iBase) ; RGBA
                                     ; u32 : R, G, B, A (0-128)
    iaddiu  vertexData,     iBase,      2           ; pointer to vertex data

    iaddiu   Mask, vi00, 0x7fff
    mtir     vertCount, primTag[x]
    iand     vertCount, vertCount, Mask              ; Get the number of verts (bit 0-14) from the PRIM giftag

    iadd    colorData,      vertexData,   vertCount   ; pointer to stq
    iadd    stqData,        colorData,    vertCount   ; pointer to colors
    iadd    kickAddress,    stqData,  vertCount       ; pointer for XGKICK
    iadd    destAddress,    stqData,  vertCount       ; helper pointer for data inserting
    ;////////////////////////////////////////////

    ;/////////// --- Store tags --- /////////////
    sqi primTag,    (destAddress++) ; prim + tell gs how many data will be
    ;////////////////////////////////////////////

    ;/////////////// --- Loop --- ///////////////
    iadd vertexCounter, iBase, vertCount ; loop vertCount times
    vertexLoop:

        ;////////// --- Load loop data --- //////////
        lq vertex, 0(vertexData)    ; load xyz
                                    ; float : X, Y, Z
                                    ; any32 : _ = 0
        lq stq,    0(stqData)       ; load stq
                                    ; float : S, T
                                    ; any32 : Q = 1     ; 1, because we will mul this by 1/vert[w] and this
                                                        ; will be our q for texture perspective correction
                                    ; any32 : _ = 0 
        lq.xyzw color,  0(colorData) ; load color                   
        ;////////////////////////////////////////////    


        ;////////////// --- Vertex --- //////////////
        MatrixMultiplyVertex	vertex, ObjectToScreen, vertex ; transform each vertex by the matrix
       
        clipw.xyz	vertex, vertex			; Dr. Fortuna: This instruction checks if the vertex is outside
							; the viewing frustum. If it is, then the appropriate
							; clipping flags are set
        fcand		VI01,   0x3FFFF                 ; Bitwise AND the clipping flags with 0x3FFFF, this makes
							; sure that we get the clipping judgement for the last three
							; verts (i.e. that make up the triangle we are about to draw)
        iaddiu		iADC,   VI01,       0x7FFF      ; Add 0x7FFF. If any of the clipping flags were set this will
							; cause the triangle not to be drawn (any values above 0x8000
							; that are stored in the w component of XYZ2 will set the ADC
							; bit, which tells the GS not to perform a drawing kick on this
							; triangle.

        isw.w		iADC,   2(destAddress)
        
        div         q,      vf00[w],    vertex[w]   ; perspective divide (1/vert[w]):
        mul.xyz     vertex, vertex,     q

        mul.xyz    vertex, vertex,     scale
        add.xyz    vertex, vertex,     offset

        VertexFpToGsXYZ2  vertex,vertex
        ;////////////////////////////////////////////


        ;//////////////// --- ST --- ////////////////
        mulq modStq, stq, q
        ;////////////////////////////////////////////

        ;//////////////// - COLORS - /////////////////
        mul color, color, rgba                     ; normalize RGBA
        ColorFPtoGsRGBAQ intColor, color           ; convert to int
        ;///////////////////////////////////////////


        ;//////////// --- Store data --- ////////////
        sq modStq,      0(destAddress)      ; STQ
        sq intColor,    1(destAddress)      ; RGBA ; q is grabbed from stq
        sq.xyz vertex,  2(destAddress)      ; XYZ2
        ;////////////////////////////////////////////

        iaddiu          vertexData,     vertexData,     1                         
        iaddiu          stqData,        stqData,        1  
        iaddiu          colorData,      colorData,      1  
        iaddiu          destAddress,    destAddress,    3

        iaddi   vertexCounter,  vertexCounter,  -1	; decrement the loop counter 
        ibne    vertexCounter,  iBase,   vertexLoop	; and repeat if needed

    ;//////////////////////////////////////////// 

    xgkick kickAddress ; dispatch to the GS rasterizer.

--barrier
--cont

    b init

--exit
--endexit
