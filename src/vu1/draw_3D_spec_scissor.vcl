; 2024 - Daniel Santos
; AthenaEnv Renderer

.syntax new
.name VU1Draw3DLCSS
.vu
.init_vf_all
.init_vi_all

.include "vu1/include/mem_layout.i"
.include "vu1/include/athena_consts.i"
.include "vu1/include/athena_macros.i"
.include "vu1/include/vcl_sml.i"

.macro SpecularPowerScale 
    add specAngle, vf00, vf00
    VectorDotProduct specAngle, normal, halfDir
    maxx		specAngle, specAngle, vf00			; Clamp to > 0
    mul  		specAngle, specAngle, specAngle	; Square it
    mul  		specAngle, specAngle, specAngle	; 4th power
    mul  		specAngle, specAngle, specAngle	; 8th power
    mul  		specAngle, specAngle, specAngle	; 16th power
    ;mul 		specAngle, specAngle, specAngle	; 32nd power
    ;mul 		specAngle, specAngle, specAngle	; 64nd power
    mul         specAngle, LightSpecular, specAngle[x]
    add.xyz         light, light, specAngle 
.endm

--enter
--endenter
    ;//////////// --- Load data 1 --- /////////////
    ; Updated once per mesh
    MatrixLoad	ObjectToScreen, SCREEN_MATRIX, vi00 ; load view-projection matrix

    lq scale, SCREEN_SCALE(vi00)

    AddScreenOffset scale

    ;/////////////////////////////////////////////

	fcset   0x000000	; VCL won't let us use CLIP without first zeroing
				; the clip flags

    ilw.y       accurateClipping,    CLIPFAN_OFFSET(vi00)
    ibne vi00, accurateClipping, scissor_init

cull_init:
    LoadCullScale clip_scale, 0.5

    ;//////////// --- Load data 1 --- /////////////
    ; Updated once per mesh
    MatrixLoad	LocalLight,     LIGHT_MATRIX,  vi00   ; load local light matrix
    ilw.w       dirLightQnt,    NUM_DIR_LIGHTS(vi00)  ; load active directional lights
    lq.xyz      CamPos,         CAMERA_POSITION(vi00) ; load program params
    iaddiu      lightDirs,      vi00,    LIGHT_DIRECTION_PTR       
    iaddiu      lightAmbs,      vi00,    LIGHT_AMBIENT_PTR
    iaddiu      lightDiffs,     vi00,    LIGHT_DIFFUSE_PTR    
    iaddiu      lightSpecs,     vi00,    LIGHT_SPECULAR_PTR 

    ;//////////// --- Load data 2 --- /////////////
    ; Updated dynamically

culled_init:
    xtop    iBase
    xitop   vertCount

    lq      primTag,        0(iBase) ; GIF tag - tell GS how many data we will send
    lq      matDiffuse,     1(iBase) ; RGBA
                                     ; u32 : R, G, B, A (0-128)

    iaddiu  vertexData,     iBase,      2           ; pointer to vertex data

    iadd      normalData,        vertexData, vertCount   ; pointer to stq
    iadd       colorData,     normalData, vertCount   ; pointer to stq
    iadd      stqData,      colorData,  vertCount   ; pointer to colors
    iadd      dataPointers,   stqData,  vertCount

    iaddiu    kickAddress,    dataPointers,  0       ; pointer for XGKICK
    iaddiu    destAddress,    dataPointers,  1       ; helper pointer for data inserting
    ;////////////////////////////////////////////

    ;/////////// --- Store tags --- /////////////
    sq primTag,    0(kickAddress) ; prim + tell gs how many data will be

    ; Set the GifTag EOP bit to 1 and NLOOP to the number of vertices
    iaddiu               Mask, vertCount, 0x7fff
    iaddiu               Mask, Mask, 0x01
    isw.x                Mask, 0(kickAddress)
    ;////////////////////////////////////////////

    ;/////////////// --- Loop --- ///////////////
    iadd vertexCounter, iBase, vertCount ; loop vertCount times
    vertexLoop:

        ;////////// --- Load loop data --- //////////
        lq inVert, 0(vertexData)  
        lq stq,    0(stqData)   
        lq.xyzw inNorm,  0(normalData)     
        lq inColor, 0(colorData)             
        ;////////////////////////////////////////////    


        ;////////////// --- Vertex --- //////////////
        MatrixMultiplyVertex	vertex, ObjectToScreen, inVert ; transform each vertex by the matrix
       
        mul clip_vertex, vertex, clip_scale

        clipw.xyz	clip_vertex, clip_vertex	
        fcand		VI01,   0x3FFFF  
        iaddiu		iADC,   VI01,       0x7FFF 

        isw.w		iADC,   XYZ2(destAddress)
        
        div         q,      vf00[w],    vertex[w]   ; perspective divide (1/vert[w]):
        mul.xyz     vertex, vertex,     q

        mul.xyz    vertex, vertex,     scale
        add.xyz    vertex, vertex,     offset

        VertexFpToGsXYZ2  vertex,vertex
        ;////////////////////////////////////////////


        ;//////////////// --- ST --- ////////////////
        mulq modStq, stq, q
        ;////////////////////////////////////////////

        ;//////////////// - NORMALS - /////////////////
        MatrixMultiplyVertex	normal,    LocalLight, inNorm ; transform each normal by the matrix
        MatrixMultiplyVertex	lightvert, LocalLight, inVert ; transform each normal by the matrix
        div         q,      vf00[w],    normal[w]   ; perspective divide (1/vert[w]):
        mul.xyz     normal, normal,     q
        
        move light, vf00
        move intensity, vf00

        iadd  currDirLight, vi00, vi00
        culled_directionaLightsLoop:
            iadd  currLightPtr, lightAmbs, currDirLight
            lq LightAmbient, 0(currLightPtr)

            ; Ambient lighting
            add.xyz light, light, LightAmbient

            iadd  currLightPtr, lightDirs, currDirLight
            lq LightDirection, 0(currLightPtr)
            
            ; Diffuse lighting
            VectorDotProduct intensity, normal, LightDirection

            maxx.xyzw  intensity, intensity, vf00

            iadd  currLightPtr, lightDiffs, currDirLight
            lq LightDiffuse, 0(currLightPtr)

            mul diffuse, LightDiffuse, intensity[x]
            add.xyz light, light, diffuse

            ; Blinn-Phong Lighting Calculation
            ;VectorNormalize CamPos, CamPos

            ;sub lightDir, lightvert, CamPos ; Compute light direction vector
            ;VectorNormalize lightDir, lightDir

            ; Compute halfway vector
            ;add halfDir, LightDirection, CamPos
            ;VectorNormalize halfDir, halfDir
            HalfAngle halfDir, LightDirection, CamPos

            iadd  currLightPtr, lightSpecs, currDirLight
            lq LightSpecular, 0(currLightPtr)

            SpecularPowerScale

            iaddiu   currDirLight,  currDirLight,  1; increment the loop counter 
            ibne    dirLightQnt,  currDirLight,  culled_directionaLightsLoop	; and repeat if needed

        add.xyzw   color, matDiffuse, inColor
        mul    color, color,      light       ; color = color * light

        VectorNormalizeClamp color, color
        loi 128.0
        mul color, color, i                     ; normalize RGBA
        ColorFPtoGsRGBAQ intColor, color           ; convert to int
        ;///////////////////////////////////////////


        ;//////////// --- Store data --- ////////////
        sq.xyz modStq,      STQ(destAddress)      
        sq intColor,    RGBA(destAddress)      ; q is grabbed from stq
        sq.xyz vertex,  XYZ2(destAddress)     
        ;////////////////////////////////////////////

        iaddiu          vertexData,     vertexData,     1                         
        iaddiu          stqData,        stqData,        1  
        iaddiu          normalData,     normalData,     1
        iaddiu          colorData,     colorData,     1
        iaddiu          destAddress,    destAddress,    3

        iaddi   vertexCounter,  vertexCounter,  -1	; decrement the loop counter 
        ibne    vertexCounter,  iBase,   vertexLoop	; and repeat if needed

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

    iaddiu  vertexData,     iBase,      2           ; pointer to vertex data
    iadd      normalData,        vertexData, vertCount   ; pointer to stq
    iadd       colorData,     normalData, vertCount   ; pointer to stq
    iadd      stqData,      colorData,  vertCount   ; pointer to colors

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

    loop:

        ;////////// --- Load loop data --- //////////
        lq inVert, 0(vertexData)   
        lq stq,    0(stqData)       
        lq inNorm, 0(normalData) 
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
        
        move light, vf00
        move intensity, vf00

        iadd  currDirLight, vi00, vi00
        ilw.w       dirLightQnt,    NUM_DIR_LIGHTS(vi00) ; load active directional lights 
        directionaLightsLoop:
            
            lq.xyz      CamPos,         CAMERA_POSITION(vi00) ; load program params
            iaddiu      lightDirs,      vi00,    LIGHT_DIRECTION_PTR      
            iaddiu      lightAmbs,      vi00,    LIGHT_AMBIENT_PTR  
            iaddiu      lightDiffs,     vi00,    LIGHT_DIFFUSE_PTR     
            iaddiu      lightSpecs,     vi00,    LIGHT_SPECULAR_PTR  

            iadd  currLightPtr, lightAmbs, currDirLight
            lq LightAmbient, 0(currLightPtr)

            ; Ambient lighting
            add.xyz light, light, LightAmbient

            iadd  currLightPtr, lightDirs, currDirLight
            lq LightDirection, 0(currLightPtr)
            
            ; Diffuse lighting
            VectorDotProduct intensity, normal, LightDirection

            maxx.xyzw  intensity, intensity, vf00

            iadd  currLightPtr, lightDiffs, currDirLight
            lq LightDiffuse, 0(currLightPtr)

            mul diffuse, LightDiffuse, intensity[x]
            add.xyz light, light, diffuse


            ; Blinn-Phong Lighting Calculation
            ;VectorNormalize CamPos, CamPos

            ;sub lightDir, lightvert, CamPos ; Compute light direction vector
            ;VectorNormalize lightDir, lightDir

            ; Compute halfway vector
            ;add halfDir, LightDirection, CamPos
            ;VectorNormalize halfDir, halfDir
            HalfAngle halfDir, LightDirection, CamPos

            iadd  currLightPtr, lightSpecs, currDirLight
            lq LightSpecular, 0(currLightPtr)

            SpecularPowerScale

            iaddiu   currDirLight,  currDirLight,  1; increment the loop counter 
            ibne    dirLightQnt,  currDirLight,  directionaLightsLoop	; and repeat if needed

        add.xyzw   color, matDiffuse, inColor
        mul    color, color,      light       ; color = color * light

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

        .include "vu1/proc/process_scissor_clip.i"

        iaddiu          vertexData,     vertexData,     1                         
        iaddiu          stqData,        stqData,        1   
        iaddiu          normalData,     normalData,     1
        iaddiu          colorData,     colorData,     1

        iaddiu          outputAddress,  outputAddress,  3

        iaddi   vertCount,  vertCount,  -1	; decrement the loop counter 
        ibne    vertCount,  vi00,   loop	; and repeat if needed

    ;//////////////////////////////////////////// 

    xgkick kickAddress ; dispatch to the GS rasterizer.

--barrier
--cont

    b init

.include "vu1/proc/save_last_loop.i"

.include "vu1/proc/scissor_interpolation.i"

--exit
--endexit
