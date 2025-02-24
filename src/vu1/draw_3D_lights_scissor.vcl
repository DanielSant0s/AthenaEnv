; 2024 - Daniel Santos
; AthenaEnv Renderer
;
; 
;
; 
;---------------------------------------------------------------
; draw_3D_lights.vcl                                           |
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
.name VU1Draw3DLCS
.vu
.init_vf_all
.init_vi_all


STACK_OFFSET        .assign  1023        ; (This was 1024 before, which was conflicting with the dummy XGKick...)

SCREEN_SCALE        .assign  0
RENDER_FLAGS        .assign  0

SCREEN_MATRIX       .assign  1
LIGHT_MATRIX        .assign  5

CAMERA_POSITION     .assign  9
NUM_DIR_LIGHTS      .assign  9

LIGHT_DIRECTION_PTR .assign 10
LIGHT_AMBIENT_PTR   .assign 14
LIGHT_DIFFUSE_PTR   .assign 18
LIGHT_SPECULAR_PTR  .assign 22

CLIPFAN_OFFSET      .assign 26

CLIP_WORK_BUF_0     .assign 61
CLIP_WORK_BUF_1     .assign 101

INBUF_SIZE          .assign 204         ; Max NbrVerts (51 * 4)
DUMMY_XGKICK_BUF    .assign 1023

.include "vcl_sml.i"

--enter
--endenter

    iaddiu               StackPtr, vi00, STACK_OFFSET

    ;//////////// --- Load data 1 --- /////////////
    ; Updated once per mesh
    MatrixLoad	ObjectToScreen, SCREEN_MATRIX, vi00 ; load view-projection matrix

    lq    scale, SCREEN_SCALE(vi00)

    loi            2048.0
    addi.xy        offset, vf00, i
    add.zw          offset, vf00, vf00

    add.xyz offset, scale, offset

	fcset   0x000000	; VCL won't let us use CLIP without first zeroing
				; the clip flags


    lq                   ClipTag, CLIPFAN_OFFSET(vi00)      ; load Triangle Fan Tag
 
    ;=====================================================================================
    ; If the result of ANDing 3 with the giftag's NLOOP (number of verts) is 3, the model
    ; is assumed to be a collection of triangles. Otherwise it's assumed it's a strip (-1)
    ;=====================================================================================
    mtir                 PrimitiveType, ClipTag[x]
    iaddi                Mask, vi00, 3
    iand                 PrimitiveType, PrimitiveType, Mask
 
    iaddi                ClipTrigger, vi00, -1
 
    ibne                 Mask, PrimitiveType, done
 
triangle:
    iaddiu               ClipTrigger, vi00, 3

done:

    ;/////////////////////////////////////////////


    ;//////////// --- Load data 2 --- /////////////
    ; Updated dynamically
init:
    xtop    iBase

    lq      primTag,        0(iBase) ; GIF tag - tell GS how many data we will send
    lq      matDiffuse,     1(iBase) ; material diffuse color

    iaddiu   Mask, vi00, 0x7fff
    mtir     vertCount, primTag[x]
    iand     vertCount, vertCount, Mask              ; Get the number of verts (bit 0-14) from the PRIM giftag

    iaddiu  vertexData,     iBase,      2           ; pointer to vertex data
    iadd    normalData,     vertexData, vertCount   ; pointer to stq
    iadd    colorData,     normalData, vertCount   ; pointer to stq
    iadd    stqData,     colorData,    vertCount   ; pointer to colors

    iaddiu     kickAddress,    vertexData, INBUF_SIZE
    ;////////////////////////////////////////////

    ;/////////// --- Store tags --- /////////////
    sq primTag,    0(kickAddress) ; prim + tell gs how many data will be
    ;////////////////////////////////////////////

    iaddiu    outputAddress,    kickAddress,  1       ; helper pointer for data inserting

    ;=====================================================================================
    ; These are the clip flag results for the last 3 verts.
    ; Initialize them all to be -2.
    ;=====================================================================================
    ;iaddi                ClipFlag1, vi00, -2     ;;;;;;; No need to do this...
    iaddi                ClipFlag2, vi00, -2
    iaddi                ClipFlag3, vi00, -2

    ;=====================================================================================
    ; These are the clipping space version of the last 3 verts.
    ; Initialize them at (0,0,0,1), which is sure to be within range (not clipped)
    ;=====================================================================================
    ;move                 CSVertex1, vf00     ;;;;;;; No need to do this...
    move                 CSVertex2, vf00
    move                 CSVertex3, vf00

    loop:

        ;////////// --- Load loop data --- //////////
        lq inVert,  0(vertexData)   
        lq stq,     0(stqData)       
        lq inNorm,  0(normalData) 
        lq inColor, 0(colorData) 
        ;////////////////////////////////////////////    


        ;////////////// --- Vertex --- //////////////
        MatrixMultiplyVertex	vertex, ObjectToScreen, inVert ; transform each vertex by the matrix
        VertexPersCorrST vertex, modStq, vertex, stq

        mul.xyz    vertex, vertex,     scale
        add.xyz    vertex, vertex,     offset

        VertexFpToGsXYZ2  vertex,vertex
        ;////////////////////////////////////////////

        ;//////////////// - NORMALS - /////////////////
        MatrixLoad	LocalLight,     LIGHT_MATRIX, vi00     ; load local light matrix

        MatrixMultiplyVertex	normal,    LocalLight, inNorm ; transform each normal by the matrix
        div         q,      vf00[w],    normal[w]   ; perspective divide (1/vert[w]):
        mul.xyz     normal, normal,     q
        
        add light, vf00, vf00
        add intensity, vf00, vf00

        iadd  currDirLight, vi00, vi00
        directionaLightsLoop:
            ilw.w       dirLightQnt,    NUM_DIR_LIGHTS(vi00) ; load active directional lights
            iaddiu      lightDirs,      vi00,    LIGHT_DIRECTION_PTR      
            iaddiu      lightAmbs,      vi00,    LIGHT_AMBIENT_PTR  
            iaddiu      lightDiffs,     vi00,    LIGHT_DIFFUSE_PTR      

            iadd  currLightPtr, lightAmbs, currDirLight
            lq LightAmbient, 0(currLightPtr)

            ; Ambient lighting
            add light, light, LightAmbient

            iadd  currLightPtr, lightDirs, currDirLight
            lq LightDirection, 0(currLightPtr)
            
            ; Diffuse lighting
            VectorDotProduct intensity, normal, LightDirection

            maxx.xyzw  intensity, intensity, vf00

            iadd  currLightPtr, lightDiffs, currDirLight
            lq LightDiffuse, 0(currLightPtr)

            mul diffuse, LightDiffuse, intensity[x]
            add light, light, diffuse

            iaddiu   currDirLight,  currDirLight,  1; increment the loop counter 
            ibne    dirLightQnt,  currDirLight,  directionaLightsLoop	; and repeat if needed

        add.xyzw   color, matDiffuse, inColor
        mul    color, color,      light       ; color = color * light
        VectorClamp color, color 0.0 1.0
        loi 128.0
        mul color, color, i                        ; normalize RGBA
        ColorFPtoGsRGBAQ intColor, color           ; convert to int
        ;///////////////////////////////////////////


        ;//////////// --- Store data --- ////////////
        sq modStq,      0(outputAddress)      ; STQ
        sq intColor,    1(outputAddress)      ; RGBA ; q is grabbed from stq
        sq vertex,  2(outputAddress)      ; XYZ2
        ;////////////////////////////////////////////

        ;=====================================================================================
        ; Push the clipping-space vertex queue down 1
        ;=====================================================================================
        move                 CSVertex1, CSVertex2
        move                 CSVertex2, CSVertex3        

        ;=====================================================================================
        ; Apply the world to clip matrix to the vertex (push onto above stack)
        ;=====================================================================================
        MatrixMultiplyVertex CSVertex3, ObjectToScreen, inVert

        ;=====================================================================================
        ; Perform clipping judgement on the the previous 3 vertices.  sets the clipping flags
        ;=====================================================================================
        clipw.xyz            CSVertex1, CSVertex1
        clipw.xyz            CSVertex2, CSVertex2
        clipw.xyz            CSVertex3, CSVertex3

        ;=====================================================================================
        ; Pushing the clipping flag result down 1
        ;=====================================================================================
        iadd                 ClipFlag1, vi00, ClipFlag2
        iadd                 ClipFlag2, vi00, ClipFlag3

        ;=====================================================================================
        ; AND clipping flags with 0x3f (current judgement) and store result in vi01.
        ; vi01 will be 0 if the vertex is within the clip volume
        ;=====================================================================================
        fcand                vi01, 0x3f

        ;=====================================================================================
        ; store result in the clip result stack
        ;=====================================================================================
        iadd                 ClipFlag3, vi00, vi01

    clip_strip:
        ;=====================================================================================
        ; ClipTrigger is 1, 2 or 3 if we're dealing with triangles or -1 for strips.  There's
        ; no need to run the code immediately following if we got a strip
        ;=====================================================================================
        ibltz                ClipTrigger, check

    clip_triangle:
        ;=====================================================================================
        ; If it's a triangle rather than a strip, you only need to perform the clipping check
        ; every 3 verts (i.e. when ClipTrigger == 0)
        ;=====================================================================================
        iaddi                ClipTrigger, ClipTrigger, -1
        ibgtz                ClipTrigger, after_scissoring

        ;=====================================================================================
        ; Reset the ClipTrigger
        ;=====================================================================================
        iaddiu               ClipTrigger, vi00, 3

    check:
        ;=====================================================================================
        ; If all 3 verts are inside (i.e., All clip results = 0), we don't need to perform
        ; scissoring.
        ;=====================================================================================
        iadd                 ClipFlag1, ClipFlag1, ClipFlag2
        iadd                 ClipFlag1, ClipFlag1, ClipFlag3
        iblez                ClipFlag1, after_scissoring


    cull_vertex:
        ;=====================================================================================
        ; Find out if we got one of those triangles that don't need to be displayed at all
        ;=====================================================================================
 
        ;=====================================================================================
        ; 0xfdf7df = 111111 011111 011111 011111
        ; fcor returns 1 if all fields in result are 1, or 0 if they are not all 1
        ; vi01 will return 1 if all 3 verts are clipped by the -Z plane, in which case we
        ; don't have to display it at all
        ;=====================================================================================
        fcor                 vi01, 0xfdf7df
        ibne                 vi01, vi00, triangle_outside
 
        ;=====================================================================================
        ; Ditto but this time fefbef = 111111 101111 101111 101111 (+Z)
        ;=====================================================================================
        fcor                 vi01, 0xfefbef
        ibne                 vi01, vi00, triangle_outside
 
        ;=====================================================================================
        ; Ditto but this time ff7df7 = 111111 110111 110111 110111 (-Y)
        ;=====================================================================================
        fcor                 vi01, 0xff7df7
        ibne                 vi01, vi00, triangle_outside
 
        ;=====================================================================================
        ; Ditto but this time ffbefb = 111111 111011 111011 111011 (+Y)
        ;=====================================================================================
        fcor                 vi01, 0xffbefb
        ibne                 vi01, vi00, triangle_outside
 
        ;=====================================================================================
        ; Ditto but this time ffdf7d = 111111 111101 111101 111101 (-X)
        ;=====================================================================================
        fcor                 vi01, 0xffdf7d
        ibne                 vi01, vi00, triangle_outside
 
        ;=====================================================================================
        ; Ditto but this time ffefbe = 111111 111110 111110 111110 (+X)
        ;=====================================================================================
        fcor                 vi01, 0xffefbe
        ibne                 vi01, vi00, triangle_outside
 
        ;=====================================================================================
        ; If we get here, it means we have to scissor
        ;=====================================================================================

        ;=====================================================================================
        ; Save context
        ;=====================================================================================
        PushVertex           StackPtr, CSVertex1
        PushVertex           StackPtr, CSVertex2
        PushVertex           StackPtr, CSVertex3
        PushInteger4         StackPtr, kickAddress, vertexData,  ClipFlag1, ClipFlag2
        PushInteger4         StackPtr, ClipFlag3, outputAddress, vertCount,  ClipTrigger

        ;-----------------------------------------------------------------------------------------------------------------------------------
        ;=====================================================================================
        ; Setup pointers to clipping work buffers
        ;=====================================================================================
        iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_0
        iaddiu               ClipWorkBuf1, vi00, CLIP_WORK_BUF_1

        ;=====================================================================================
        ; Calculate the average vertex of the 3 clipping-space verts
        ;=====================================================================================
        add                  AverageVertex, CSVertex1,     CSVertex2
        add                  AverageVertex, AverageVertex, CSVertex3
        loi                  0.3333333
        mul                  AverageVertex, AverageVertex, I

        ;=====================================================================================
        ; To get rid of the cracks generated by our piss-poor scissoring algorithm, scale the
        ; triangle up 1% in relation to the average vertex.
        ;=====================================================================================
        sub                  ScaledVertex1, CSVertex1, AverageVertex
        sub                  ScaledVertex2, CSVertex2, AverageVertex
        sub                  ScaledVertex3, CSVertex3, AverageVertex

        loi                  \(1.01)

        mul                  acc, AverageVertex, vf00[w]
        madd                 ScaledVertex1, ScaledVertex1, I

        mul                  acc, AverageVertex, vf00[w]
        madd                 ScaledVertex2, ScaledVertex2, I

        mul                  acc, AverageVertex, vf00[w]
        madd                 ScaledVertex3, ScaledVertex3, I

        ;=====================================================================================
        ; Store the 3 verts in the clipping work buffer.  The first vertex is added at the end
        ; to avoid having to wrap around the vertex index when we'll scissor lines.
        ;=====================================================================================
        VertexSave           ScaledVertex1, 2,  ClipWorkBuf0
        VertexSave           ScaledVertex2, 5,  ClipWorkBuf0
        VertexSave           ScaledVertex3, 8,  ClipWorkBuf0
        VertexSave           ScaledVertex1, 11, ClipWorkBuf0

        ;=====================================================================================
        ; Load the STQs from the original data and store them in the clipping work buffer.
        ;=====================================================================================
        VectorLoad           TempSTQ1, -2, stqData
        VectorLoad           TempSTQ2, -1, stqData
        VectorLoad           TempSTQ3,  0, stqData

        VectorSave           TempSTQ1,  0, ClipWorkBuf0
        VectorSave           TempSTQ2,  3, ClipWorkBuf0
        VectorSave           TempSTQ3,  6, ClipWorkBuf0
        VectorSave           TempSTQ1,  9, ClipWorkBuf0

        ;=====================================================================================
        ; Only execute the following code snippet if we're processing triangles
        ; (ClipTrigger >= 0).
        ;=====================================================================================
        ibltz                ClipTrigger, STRIP_COLOR
 
        ;=====================================================================================
        ; Triangle only: Load 3 colors, convert them to float, and store them in the clipping
        ; work buffer.
        ;=====================================================================================
        VectorLoad           TempColor1, -5, outputAddress
        VectorLoad           TempColor2, -2, outputAddress
        VectorLoad           TempColor3,  1, outputAddress
 
        ColorGsRGBAQtoFP     TempColor1, TempColor1
        ColorGsRGBAQtoFP     TempColor2, TempColor2
        ColorGsRGBAQtoFP     TempColor3, TempColor3
 
        VectorSave           TempColor1,  1, ClipWorkBuf0
        VectorSave           TempColor2,  4, ClipWorkBuf0
        VectorSave           TempColor3,  7, ClipWorkBuf0
        VectorSave           TempColor1, 10, ClipWorkBuf0
 
        b                    PLANE_START

    STRIP_COLOR:
        ;=====================================================================================
        ; Strip only: Load 1 color, convert it to fixed, and store it to the clipping work
        ; buffer.
        ;=====================================================================================
        VectorLoad           TempColor1, 1, outputAddress
     
        ColorGsRGBAQtoFP     TempColor1, TempColor1
     
        VectorSave           TempColor1,  1, ClipWorkBuf0
        VectorSave           TempColor1,  4, ClipWorkBuf0
        VectorSave           TempColor1,  7, ClipWorkBuf0
        VectorSave           TempColor1, 10, ClipWorkBuf0

    PLANE_START:
        ;=====================================================================================
        ; Useful for clipping: A zero vector (0.f, 0.f, 0.f, 0.f)
        ;=====================================================================================
        sub                  Vector0000,       vf00, vf00
        
        ;=====================================================================================
        ; Scissor against the Z- plane
        ;=====================================================================================
        iaddiu               PreviousClipFlag, vi00, 0x800
        iaddiu               CurrentClipFlag,  vi00, 0x020
        iaddiu               NbrRotates,       vi00, 2
        iaddiu               ClipWorkBuf0,     vi00, CLIP_WORK_BUF_0
        iaddiu               ClipWorkBuf1,     vi00, CLIP_WORK_BUF_1
        iaddiu               vertCount,        vi00, 3
        iadd                 newVertCount,      vi00, vi00
        sub.x                PlaneSign,        vf00, vf00[w]     ; Negative
        
        ; Load a vertex from the buffer
        lqi                  NextSTQ,    (ClipWorkBuf0++)
        lqi                  NextColor,  (ClipWorkBuf0++)
        lqi                  NextVertex, (ClipWorkBuf0++)

    LOOP_Z_MINUS:
        ; Interpolate the vertex values at the plane
        bal                  RetAddr2, scissor_interpolation
     
        ; Repeat until we have no more vertices
        isubiu               vertCount, vertCount, 1
        ibne                 vertCount, vi00, LOOP_Z_MINUS
     
        ; Copy the triangle fan's first vertex to the end (in clipping work buffer 1)
        iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_1
        bal                  RetAddr2, SAVE_LAST_LOOP
     
        ; If all verts got clipped out, stop
        ibeq                 newVertCount, vi00, SCISSOR_END
     
        ;=====================================================================================
        ; Scissor against the Z+ plane
        ;=====================================================================================
        iaddiu               PreviousClipFlag, vi00, 0x400
        iaddiu               CurrentClipFlag,  vi00, 0x010
        iaddiu               NbrRotates,       vi00, 2
        iaddiu               ClipWorkBuf0,     vi00, CLIP_WORK_BUF_1
        iaddiu               ClipWorkBuf1,     vi00, CLIP_WORK_BUF_0
        iadd                 vertCount,         vi00, newVertCount
        iadd               newVertCount,      vi00, vi00
        add.x                PlaneSign,        vf00, vf00[w]     ; Positive

        ; Load a vertex from the buffer
        lqi                  NextSTQ,    (ClipWorkBuf0++)
        lqi                  NextColor,  (ClipWorkBuf0++)
        lqi                  NextVertex, (ClipWorkBuf0++)

    LOOP_Z_PLUS:
        ; Interpolate the vertex values at the plane
        bal                  RetAddr2, scissor_interpolation
     
        ; Repeat until we have no more vertices
        isubiu               vertCount, vertCount, 1
        ibne                 vertCount, vi00, LOOP_Z_PLUS
     
        ; Copy the triangle fan's first vertex to the end (in clipping work buffer 0)
        iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_0
        bal                  RetAddr2, SAVE_LAST_LOOP
     
        ; If all verts got clipped out, stop
        ibeq                 newVertCount, vi00, SCISSOR_END
     
        ;=====================================================================================
        ; Scissor against the X- plane
        ;=====================================================================================
        iaddiu               PreviousClipFlag, vi00, 0x080
        iaddiu               CurrentClipFlag,  vi00, 0x002
        iaddiu               NbrRotates,       vi00, 0
        iaddiu               ClipWorkBuf0,     vi00, CLIP_WORK_BUF_0
        iaddiu               ClipWorkBuf1,     vi00, CLIP_WORK_BUF_1
        iadd                 vertCount,         vi00, newVertCount
        iadd               newVertCount,      vi00, vi00
        sub.x                PlaneSign,        vf00, vf00[w]     ; Negative

        ; Load a vertex from the buffer
        lqi                  NextSTQ,    (ClipWorkBuf0++)
        lqi                  NextColor,  (ClipWorkBuf0++)
        lqi                  NextVertex, (ClipWorkBuf0++)

    LOOP_X_MINUS:
       ; Interpolate the vertex values at the plane
       bal                  RetAddr2, scissor_interpolation

       ; Repeat until we have no more vertices
       isubiu               vertCount, vertCount, 1
       ibne                 vertCount, vi00, LOOP_X_MINUS

       ; Copy the triangle fan's first vertex to the end (in clipping work buffer 1)
       iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_1
       bal                  RetAddr2, SAVE_LAST_LOOP

       ; If all verts got clipped out, stop
       ibeq                 newVertCount, vi00, SCISSOR_END

       ;=====================================================================================
       ; Scissor against the X+ plane
       ;=====================================================================================
       iaddiu               PreviousClipFlag, vi00, 0x040
       iaddiu               CurrentClipFlag,  vi00, 0x001
       iaddiu               NbrRotates,       vi00, 0
       iaddiu               ClipWorkBuf0,     vi00, CLIP_WORK_BUF_1
       iaddiu               ClipWorkBuf1,     vi00, CLIP_WORK_BUF_0
       iadd                 vertCount,         vi00, newVertCount
       iadd               newVertCount,      vi00, vi00
       add.x                PlaneSign,        vf00, vf00[w]     ; Positive

       ; Load a vertex from the buffer
       lqi                  NextSTQ,    (ClipWorkBuf0++)
       lqi                  NextColor,  (ClipWorkBuf0++)
       lqi                  NextVertex, (ClipWorkBuf0++)

    LOOP_X_PLUS:
       ; Interpolate the vertex values at the plane
       bal                  RetAddr2, scissor_interpolation

       ; Repeat until we have no more vertices
       isubiu               vertCount, vertCount, 1
       ibne                 vertCount, vi00, LOOP_X_PLUS

       ; Copy the triangle fan's first vertex to the end (in clipping work buffer 0)
       iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_0
       bal                  RetAddr2, SAVE_LAST_LOOP

       ; If all verts got clipped out, stop
       ibeq                 newVertCount, vi00, SCISSOR_END

       ;=====================================================================================
       ; Scissor against the Y- plane
       ;=====================================================================================
       iaddiu               PreviousClipFlag, vi00, 0x200
       iaddiu               CurrentClipFlag,  vi00, 0x008
       iaddiu               NbrRotates,       vi00, 1
       iaddiu               ClipWorkBuf0,     vi00, CLIP_WORK_BUF_0
       iaddiu               ClipWorkBuf1,     vi00, CLIP_WORK_BUF_1
       iadd                 vertCount,         vi00, newVertCount
       iadd               newVertCount,      vi00, vi00
       sub.x                PlaneSign,        vf00, vf00[w]     ; Negative

       ; Load a vertex from the buffer
       lqi                  NextSTQ,    (ClipWorkBuf0++)
       lqi                  NextColor,  (ClipWorkBuf0++)
       lqi                  NextVertex, (ClipWorkBuf0++)

    LOOP_Y_MINUS:
       ; Interpolate the vertex values at the plane
       bal                  RetAddr2, scissor_interpolation

       ; Repeat until we have no more vertices
       isubiu               vertCount, vertCount, 1
       ibne                 vertCount, vi00, LOOP_Y_MINUS

       ; Copy the triangle fan's first vertex to the end (in clipping work buffer 1)
       iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_1
       bal                  RetAddr2, SAVE_LAST_LOOP

       ; If all verts got clipped out, stop
       ibeq                 newVertCount, vi00, SCISSOR_END

       ;=====================================================================================
       ; Scissor against the Y+ plane
       ;=====================================================================================
       iaddiu               PreviousClipFlag, vi00, 0x100
       iaddiu               CurrentClipFlag,  vi00, 0x004
       iaddiu               NbrRotates,       vi00, 1
       iaddiu               ClipWorkBuf0,     vi00, CLIP_WORK_BUF_1
       iaddiu               ClipWorkBuf1,     vi00, CLIP_WORK_BUF_0
       iadd                 vertCount,         vi00, newVertCount
       iadd               newVertCount,      vi00, vi00
       add.x                PlaneSign,        vf00, vf00[w]     ; Positive

       ; Load a vertex from the buffer
       lqi                  NextSTQ,    (ClipWorkBuf0++)
       lqi                  NextColor,  (ClipWorkBuf0++)
       lqi                  NextVertex, (ClipWorkBuf0++)

    LOOP_Y_PLUS:
       ; Interpolate the vertex values at the plane
       bal                  RetAddr2, scissor_interpolation

       ; Repeat until we have no more vertices
       isubiu               vertCount, vertCount, 1
       ibne                 vertCount, vi00, LOOP_Y_PLUS

       ; Copy the triangle fan's first vertex to the end (in clipping work buffer 0)
       iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_0
       bal                  RetAddr2, SAVE_LAST_LOOP

       ; If all verts got clipped out, stop
       ibeq                 newVertCount, vi00, SCISSOR_END

        ;=====================================================================================
        ; If we get here, we gotta draw the triangle fan
        ;=====================================================================================
        iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_0
        iaddiu               ClipWorkBuf1, vi00, CLIP_WORK_BUF_1
        iadd                 vertCount,    vi00, newVertCount

        iaddiu               DummyXGKickPtr, vi00, DUMMY_XGKICK_BUF
        iaddiu               kickAddress, vi00, CLIPFAN_OFFSET

        lq                   ClipTag,    0(kickAddress)

        iaddiu               kickAddress, kickAddress, 1

        sq                   ClipTag,    0(kickAddress) ; prim + tell gs how many data will be

        iadd                 vertexData, vi00, ClipWorkBuf0
        iaddiu               outputAddress, kickAddress, 1

        ; Clear the GifTag's NLOOP to 0 (for a dummy XGKick stall)
        iaddiu               Mask, vi00, 0x7fff
        iaddiu               Mask, Mask, 0x01
        isw.x                Mask, 0(DummyXGKickPtr)

        xgkick               DummyXGKickPtr

        ; Set the GifTag EOP bit to 1 and NLOOP to the number of vertices
        iaddiu               Mask, vertCount, 0x7fff
        iaddiu               Mask, Mask, 0x01
        isw.x                Mask, 0(kickAddress)

    LOOP2:
        ;=====================================================================================
        ; Load a clip-space vertex, apply clip-to-screen matrix, perspective-correct it (and
        ; its STQ), convert it to GS format and then save it
        ;=====================================================================================
        VertexLoad           inVert, 2, vertexData
        VectorLoad           Color,  1, vertexData
        VectorLoad           stq,    0, vertexData
     
        ;MatrixMultiplyVertex vertex, ObjectToScreen, inVert
        move                 vertex, inVert

        VertexPersCorrST     vertex, STQTransform, vertex, stq

        mul.xyz    vertex, vertex,     scale
        add.xyz    vertex, vertex,     offset

        VertexFpToGsXYZ2    vertex, vertex
     
        VertexSave           vertex,  2, outputAddress
        VectorSave           STQTransform,     0, outputAddress
     
        ;=====================================================================================
        ; Convert the color to GS format and output it
        ;=====================================================================================
        ColorFPtoGSRGBAQ     Color, Color
        VectorSave           Color, 1, outputAddress
     
        ;=====================================================================================
        ; Next vertex...
        ;=====================================================================================
        iaddiu               vertexData, vertexData, 3
        iaddiu               outputAddress, outputAddress, 3
     
        iaddi                vertCount, vertCount, -1
        ibne                 vertCount, vi00, LOOP2
     
        ; --- send result to GIF and stop ---
        xgkick               kickAddress

    SCISSOR_END:
    ;-----------------------------------------------------------------------------------------------------------------------------------

        ;=====================================================================================
        ; Restore context
        ;=====================================================================================
        PopInteger4          StackPtr, ClipFlag3, outputAddress, vertCount, ClipTrigger
        PopInteger4          StackPtr, kickAddress, vertexData, ClipFlag1, ClipFlag2
        PopVertex            StackPtr, CSVertex3
        PopVertex            StackPtr, CSVertex2
        PopVertex            StackPtr, CSVertex1

    triangle_outside:
        ;=====================================================================================
        ; We don't need to draw this triangle after all...  Set the ADC bit (0x8000)
        ;=====================================================================================
        iaddiu               Mask, vi00, 0x7fff
        iaddi                Mask, Mask, 1
        isw.w                Mask, 2(outputAddress)

    after_scissoring:
        iaddiu          vertexData,     vertexData,     1                         
        iaddiu          stqData,        stqData,        1   
        iaddiu          normalData,     normalData,     1
        iaddiu          colorData,      colorData,      1

        iaddiu          outputAddress,  outputAddress,  3

        iaddi   vertCount,  vertCount,  -1	; decrement the loop counter 
        ibne    vertCount,  vi00,   loop	; and repeat if needed

    ;//////////////////////////////////////////// 

    xgkick kickAddress ; dispatch to the GS rasterizer.

--barrier
--cont

    b init

;--------------------------------------------------------------
; add first element to end of the list for triangle fan
;--------------------------------------------------------------
SAVE_LAST_LOOP:
   VertexLoad           TempVertex, 2, ClipWorkBuf0
   VectorLoad           TempColor,  1, ClipWorkBuf0
   VectorLoad           TempSTQ,    0, ClipWorkBuf0

   VertexSave           TempVertex, 2, ClipWorkBuf1
   VectorSave           TempColor,  1, ClipWorkBuf1
   VectorSave           TempSTQ,    0, ClipWorkBuf1
   iaddiu               ClipWorkBuf1, ClipWorkBuf1, 3

   jr                   RetAddr2

;--------------------------------------------------------------------------------------------------
; Scissor a line going from "CurrentVertex" to "NextVertex" to a clipping plane.
;--------------------------------------------------------------------------------------------------
; Input:  "CurrentVertex" and "NextVertex" is the line
;         "ClipWorkBuf0" must point on the input vertex buffer
;         "ClipWorkBuf1" must point on the output vertex buffer
;         "newVertCount" is the current number of output vertices
; Output: "ClipWorkBuf0" will be incremented to the next input vertex
;         "ClipWorkBuf1" will contain new verts, and will be incremented to the next output vertex
;         "newVertCount" will be incremented according to 'ClipWorkBuf1'
;--------------------------------------------------------------------------------------------------
scissor_interpolation:
   ;----------------------------------------
   ; The next vertex becomes the current one
   ;----------------------------------------
   add                  CurrentVertex, Vector0000, NextVertex
   add                  CurrentColor,  Vector0000, NextColor
   add                  CurrentSTQ,    Vector0000, NextSTQ

   ;---------------------------------------
   ; Update the indices for the next vertex
   ;---------------------------------------
   lqi                  NextSTQ,    (ClipWorkBuf0++)
   lqi                  NextColor,  (ClipWorkBuf0++)
   lqi                  NextVertex, (ClipWorkBuf0++)
   

   ;----------------------------------------------------------
   ; Test if the 2 vertices are on different side of the plane
   ;----------------------------------------------------------
   clipw.xyz            CurrentVertex, CurrentVertex[w]
   clipw.xyz            NextVertex, NextVertex[w]
   fcget                vi01

   ;------------------------------------------------------
   ; If the result is 0, then the first vertex is "inside"
   ;------------------------------------------------------
   iand                 ClipFlag1, vi01, PreviousClipFlag
   ibeq                 ClipFlag1, vi00, CUR_IN

CUR_OUT:
   ;------------------------------------------------------
   ; The first vertex is "outside".  Check the second one.
   ;------------------------------------------------------
   iand                 ClipFlag1, vi01, CurrentClipFlag
   ibeq                 ClipFlag1, vi00, CO_NEXT_IN

CO_NEXT_OUT:
   ;------------------------------------------------------------------
   ; Both vertices are "outside".  Don't need to interpolate anything.
   ;------------------------------------------------------------------
;   b                    SCISSOR_INTERPOLATION_END
   jr                   RetAddr2

CO_NEXT_IN:
   ;---------------------------------------------------------------------
   ; The first vertex is "outside", and the second is "inside".  Find the
   ; point of intersection, and store it in the clipping work buffer.
   ;---------------------------------------------------------------------
   bal                  RetAddr1, INTERPOLATE

   sqi                  NewSTQ,    (ClipWorkBuf1++)
   sqi                  NewColor,  (ClipWorkBuf1++)
   sqi                  NewVertex, (ClipWorkBuf1++)

   iaddiu               newVertCount, newVertCount, 1

;   b                    SCISSOR_INTERPOLATION_END
   jr                   RetAddr2

CUR_IN:
   ;-----------------------------
   ; The first vertex is "inside"
   ;-----------------------------
   iand                 ClipFlag1, vi01, CurrentClipFlag
   ibeq                 ClipFlag1, vi00, CI_NEXT_IN

CI_NEXT_OUT:
   ;-----------------------------------------------------------
   ; The first vertex is "inside", and the second is "outside".
   ; Find the point of intersection, and store both the first
   ; and new vertices in the clipping work buffer.
   ;-----------------------------------------------------------
   bal                  RetAddr1, INTERPOLATE
   
   sqi                  CurrentSTQ,    (ClipWorkBuf1++)
   sqi                  CurrentColor,  (ClipWorkBuf1++)
   sqi                  CurrentVertex, (ClipWorkBuf1++)

   sqi                  NewSTQ,    (ClipWorkBuf1++)
   sqi                  NewColor,  (ClipWorkBuf1++)
   sqi                  NewVertex, (ClipWorkBuf1++)
   

   iaddiu               newVertCount, newVertCount, 2

;   b                    SCISSOR_INTERPOLATION_END
   jr                   RetAddr2

   ;---------------------------------------------------------------------
   ; Both vertices are "inside". Don't need to interpolate anything.  But
   ; still need to store the first vertex in the clipping work buffer.
   ;---------------------------------------------------------------------
CI_NEXT_IN:
    sqi                  CurrentSTQ,    (ClipWorkBuf1++)
    sqi                  CurrentColor,  (ClipWorkBuf1++)
    sqi                  CurrentVertex, (ClipWorkBuf1++)

   iaddiu               newVertCount, newVertCount, 1

   ;-------------------
   ; Scissoring is over
   ;-------------------
SCISSOR_INTERPOLATION_END:
   jr                   RetAddr2

;--------------------------------------------------------------------------------------------------
; Calculate the point of insersection between a clip plane and a line going from "CurrentVertex" to
; "NextVertex" (in clipping space). The intersection point is returned in "NewVertex".
;--------------------------------------------------------------------------------------------------
; Input:  "CurrentVertex" and "NextVertex" is the line
;         "PlaneSign" is the sign of the clipping plane
;         "NbrRotate" is 0 for X, 1 for Y, and 2 for Z plane
; Output: "NewVertex" is the intersection point
;--------------------------------------------------------------------------------------------------
INTERPOLATE:
   ;---------------------------------------------------------------------------------
   ; Find the distance of the current vertex to the clipping plane. In clip space the
   ; extents of the clipping regions are all vf17w for the clip test so the clipping
   ; plane in clip space is simply vf17w * the sign of the plane (vf30x)
   ;---------------------------------------------------------------------------------
   mul.w                Temp1, CurrentVertex, PlaneSign[x]        ; Temp1 = CurrentVertex - (sign * CurrentVertex[w])
   sub                  Temp1, CurrentVertex, Temp1[w]            ;

   ;-----------------------------------
   ; Same thing but for NextVertex
   ;-----------------------------------
   mul.w                Temp2, NextVertex, PlaneSign[x]           ; Temp1 = NextVertex - (sign * NextVertex[w])
   sub                  Temp2, NextVertex, Temp2[w]               ;

   ;-------------------------------------------
   ; Rotate the vertices around until the field
   ; we are checking against is in the x field
   ;-------------------------------------------
   iadd                 NbrRotations, vi00, NbrRotates

LOOP_ROT:
   ibeq                 NbrRotations, vi00, LOOP_ROT_END

   mr32                 Temp1, Temp1
   mr32                 Temp2, Temp2

   isubiu               NbrRotations, NbrRotations, 1
   b                    LOOP_ROT

LOOP_ROT_END:
   ;---------------------------------------------------------
   ; Calculate the difference between Current and Next vertex
   ;---------------------------------------------------------
   sub.x                Difference, Temp2, Temp1

   ;---------------------------------------------------------------
   ;ok so now divide current by the ratio between current and previous vectors
   ;and get the abs version
   ;q = (current.x - current.w * cplane)/(next.x-next.x * cplane)
   ;---------------------------------------------------------------
   div                  Q, Temp1[x], Difference[x]
   add.x                Answer, vf00, Q
   abs.x                Answer, Answer          ; ans = |(c-cw)/((n-nw)+(c-cw))|

   ;-------------------------------------------------------------------
   ; NewVertex = ((NextVertex - CurrentVertex) * Ratio) + CurrentVertex
   ;-------------------------------------------------------------------
   sub                  NewVertex, NextVertex, CurrentVertex
   sub                  NewColor,  NextColor,  CurrentColor
   sub                  NewSTQ,    NextSTQ,    CurrentSTQ

   mul                  NewVertex, NewVertex, Answer[x]
   mul                  NewColor,  NewColor,  Answer[x]
   mul                  NewSTQ,    NewSTQ,    Answer[x]

   add                  NewVertex, NewVertex, CurrentVertex
   add                  NewColor,  NewColor,  CurrentColor
   add                  NewSTQ,    NewSTQ,    CurrentSTQ

   ;-----------------------------------
   ; Done
   ;-----------------------------------
   jr                   RetAddr1

--exit
--endexit
