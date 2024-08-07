; _____     ___ ____     ___ ____
;  ____|   |    ____|   |        | |____|
; |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
;-----------------------------------------------------------------------
; (c) 2020 h4570 Sandro Sobczyński <sandro.sobczynski@gmail.com>
; Licenced under Academic Free License version 2.0
; Review ps2sdk README & LICENSE files for further details.
;
;
;---------------------------------------------------------------
; draw_3D_lights_notex.vcl                                                   |
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
.name VU1Draw3DLightsColorsNoTex
.vu
.init_vf_all
.init_vi_all

.include "vcl_sml.i"

--enter
--endenter

    ;//////////// --- Load data 1 --- /////////////
    ; Updated once per mesh
    MatrixLoad	ObjectToScreen, 0, vi00 ; load view-projection matrix
    MatrixLoad	LocalLight,     4, vi00     ; load local light matrix
    ;/////////////////////////////////////////////

	fcset   0x000000	; VCL won't let us use CLIP without first zeroing
				; the clip flags

    ;//////////// --- Load data 2 --- /////////////
    ; Updated dynamically
    xtop    iBase

    lq.xyz  scale,          0(iBase) ; load program params
                                     ; float : X, Y, Z - scale vector that we will use to scale the verts after projecting them.
                                     ; float : W - vert count.
    lq      gifSetTag,      1(iBase) ; GIF tag - set
    lq      primTag,        2(iBase) ; GIF tag - tell GS how many data we will send
    lq      rgba,           3(iBase) ; RGBA
                                     ; u32 : R, G, B, A (0-128)
    iaddiu  vertexData,     iBase,      4           ; pointer to vertex data
    ilw.w   vertCount,      0(iBase)                ; load vert count from scale vector
    iadd    colorData,      vertexData, vertCount   ; pointer to colors
    iadd    normalData,      colorData, vertCount   ; pointer to colors
    iadd    lightsData,     normalData,  vertCount
    MatrixLoad	LightDirection,   0,   lightsData   ; load light directions
    MatrixLoad	LightAmbient,     4,   lightsData   ; load light ambients
    MatrixLoad	LightDiffuse,     8,   lightsData   ; load light diffuses
    iaddiu    kickAddress,    lightsData,  12       ; pointer for XGKICK
    iaddiu    destAddress,    lightsData,  12       ; helper pointer for data inserting
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
        lq.xyzw color,  0(colorData) ; load color
        lq.xyzw normal,  0(normalData) ; load normal                    
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
        mula.xyz    acc,    scale,      vf00[w]     ; scale to GS screen space
        madd.xyz    vertex, vertex,     scale       ; multiply and add the scales -> vert = vert * scale + scale
        ftoi4.xyz   vertex, vertex                  ; convert vertex to 12:4 fixed point format
        ;////////////////////////////////////////////

        ;//////////////// - NORMALS - /////////////////
        MatrixMultiplyVertex	normal, LocalLight, normal ; transform each normal by the matrix
        div         q,      vf00[w],    normal[w]   ; perspective divide (1/vert[w]):
        mul.xyz     normal, normal,     q
        
        add light, vf00, vf00
        add light, light, LightAmbient[0]
        add light, light, LightAmbient[1]
        add light, light, LightAmbient[2]
        add light, light, LightAmbient[3]

        add intensity, vf00, vf00

        loi  -1.0              
        addi minusOne, vf00, i

        VectorDotProduct intensity, normal, LightDirection[0]
        
        mul intensity, intensity, minusOne
        maxx.xyzw  intensity, intensity, vf00

        mul diffuse, LightDiffuse[0], intensity[x]
        add light, light, diffuse

        VectorDotProduct intensity, normal, LightDirection[1]
        
        mul intensity, intensity, minusOne
        maxx.xyzw  intensity, intensity, vf00

        mul diffuse, LightDiffuse[1], intensity[x]
        add light, light, diffuse

        VectorDotProduct intensity, normal, LightDirection[2]
        
        mul intensity, intensity, minusOne
        maxx.xyzw  intensity, intensity, vf00

        mul diffuse, LightDiffuse[2], intensity[x]
        add light, light, diffuse

        VectorDotProduct intensity, normal, LightDirection[3]
        
        mul intensity, intensity, minusOne
        maxx.xyzw  intensity, intensity, vf00

        mul diffuse, LightDiffuse[3], intensity[x]
        add light, light, diffuse

        mul.xyz    color, color,  light            ; color = color * light
        VectorClamp color, color 0.0 1.99
        mul color, color, rgba                     ; normalize RGBA
        ColorFPtoGsRGBAQ intColor, color           ; convert to int
        ;///////////////////////////////////////////


        ;//////////// --- Store data --- ////////////
        sq intColor,    0(destAddress)      ; RGBA ; q is grabbed from stq
        sq.xyz vertex,  1(destAddress)      ; XYZ2
        ;////////////////////////////////////////////

        iaddiu          vertexData,     vertexData,     1                         
        iaddiu          colorData,      colorData,      1  
        iaddiu          normalData,     normalData,     1
        iaddiu          destAddress,    destAddress,    2

        iaddi   vertexCounter,  vertexCounter,  -1	; decrement the loop counter 
        ibne    vertexCounter,  iBase,   vertexLoop	; and repeat if needed

    ;//////////////////////////////////////////// 

    --barrier

    xgkick kickAddress ; dispatch to the GS rasterizer.

--exit
--endexit
