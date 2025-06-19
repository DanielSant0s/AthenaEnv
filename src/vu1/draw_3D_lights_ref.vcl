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

--enter
--endenter
    ;//////////// --- Load data 1 --- /////////////
    ; Updated once per mesh
    MatrixLoad	ScreenMatrix, SCREEN_MATRIX, vi00 ; load view-projection matrix
    MatrixLoad	ObjectMatrix, OBJECT_MATRIX, vi00 ; load object matrix

    MatrixMultiply   ObjectToScreen, ObjectMatrix, ScreenMatrix

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

cull_init:
    LoadCullScale clip_scale, 0.5

    ;//////////// --- Load data 1 --- /////////////
    ; Updated once per mesh
    MatrixLoad	LocalLight,     LIGHT_MATRIX, vi00     ; load local light matrix
    ilw.w       dirLightQnt,    NUM_DIR_LIGHTS(vi00) ; load active directional lights

    ;//////////// --- Load data 2 --- /////////////
    ; Updated dynamically
culled_init:
    xtop    iBase
    xitop   vertCount

    lq      primTag,        0(iBase) ; GIF tag - tell GS how many data we will send
    lq      matDiffuse,     1(iBase) ; RGBA 
                                     ; u32 : R, G, B, A (0-128)
    iaddiu  vertexData,     iBase,      2           ; pointer to vertex data
    
    iadd      normalData,     vertexData, vertCount   ; pointer to stq
    iadd      colorData,      normalData, vertCount   ; pointer to stq
    iadd      dataPointers,   colorData,  vertCount
    
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
    iadd vertexCounter, vi00, vertCount ; loop vertCount times
    vertexLoop:

        ;////////// --- Load loop data --- //////////
        lq inVert, 0(vertexData) 
        lq.xyzw inNorm,  0(normalData)          
        lq inColor, 0(colorData)           
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
        MatrixMultiplyVector	stq, ObjectMatrix, inNorm

        sub.xyzw tmp_stq, vf00, vf00
        subx.x tmp_stq, stq, vf00[x]
        suby.y tmp_stq, tmp_stq, stq[y]
        addw.xyz tmp_stq, tmp_stq, vf00[w]

        loi 0.5
        muli.xy tmp_stq, tmp_stq, I

        mulq modStq, tmp_stq, q
        ;////////////////////////////////////////////

        ;//////////////// - NORMALS - /////////////////
        MatrixMultiplyVector	normal,    ObjectMatrix, inNorm ; transform each normal by the matrix
        
        move light, vf00
        move intensity, vf00

        iadd  currDirLight, vi00, vi00
        culled_directionaLightsLoop:
            lq LightAmbient, LIGHT_AMBIENT_PTR(currDirLight)

            ; Ambient lighting
            add.xyz light, light, LightAmbient

            lq LightDirection, LIGHT_DIRECTION_PTR(currDirLight)
            
            ; Diffuse lighting
            VectorDotProduct intensity, normal, LightDirection

            maxx.xyzw  intensity, intensity, vf00

            lq LightDiffuse, LIGHT_DIFFUSE_PTR(currDirLight)

            mul diffuse, LightDiffuse, intensity[x]
            add.xyz light, light, diffuse

            iaddiu   currDirLight,  currDirLight,  1; increment the loop counter 
            ibne    dirLightQnt,  currDirLight,  culled_directionaLightsLoop	; and repeat if needed

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
        iaddiu          vertexData,     vertexData,     1                            
        iaddiu          normalData,     normalData,     1
        iaddiu          colorData,      colorData,      1

        iaddi   vertexCounter,  vertexCounter,  -1	; decrement the loop counter 
        ibgtz    vertexCounter,  vertexLoop	; and repeat if needed

    ;//////////////////////////////////////////// 

    xgkick kickAddress ; dispatch to the GS rasterizer.

--barrier
--cont

    b culled_init

--exit
--endexit
