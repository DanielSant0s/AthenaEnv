; 2024 - Daniel Santos
; AthenaEnv Renderer

.syntax new
.name VU1Draw3DLCS_Skin
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

    ilw.y       accurateClipping,    CLIPFAN_OFFSET(vi00)
    ibne vi00,  accurateClipping, scissor_init

cull_init:
    LoadCullScale clip_scale, 0.5

    ;//////////// --- Load data 1 --- /////////////
    ; Updated once per mesh
    ilw.w       dirLightQnt,    NUM_DIR_LIGHTS(vi00) ; load active directional lights

    ;//////////// --- Load data 2 --- /////////////
    ; Updated dynamically
culled_init:
    xtop    iBase
    xitop   vertCount
    lq.w           bfc_multiplier, CLIPFAN_OFFSET(vi00) ; restart strip parity

    lq      primTag,        0(iBase) ; GIF tag - tell GS how many data we will send 
    lq      matDiffuse,     1(iBase) ; RGBA 
                                     ; u32 : R, G, B, A (0-128)
    iaddiu  skinData,        iBase,      0           ; pointer to vertex data
    
    ; texGiftagAddr: 3 QW of AD giftag (header + TEX0 + TEX1) written by the EE
    ; via render_emit_tex_giftag, EOP=1, a complete standalone packet. Sent on
    ; its own BEFORE the vertex loop, never chained onto the vertex XGKICK: the
    ; accurate-clipping path can fire mid-loop XGKICKs that carry no texture
    ; state of their own and draw with whatever the GS has latched.
    ;
    ; Skipped entirely for an untextured chunk -- the EE does not emit the block
    ; at all in that case, so kicking it would send stale VU memory to the GS.
    ; bake_giftags flags this in the prim giftag NLOOP field.
    iaddiu    texGiftagAddr,  iBase,  SKINNED_INBUF_SIZE
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
        lq inVert,  SKINNED_POSITION_OFFSET(iBase)
        DecompressPositionW inVert

        lq inNormPacked, SKINNED_NORMAL_OFFSET(iBase)
        DecompressNormal8 inNorm, inNormPacked

        lq inColorPacked, SKINNED_COLOR_OFFSET(iBase)
        DecompressColor8 inColor, inColorPacked

        lq stqPacked, SKINNED_TEXCOORD_OFFSET(iBase)
        DecompressUV16 stq, stqPacked
        ;////////////////////////////////////////////

        lq boneIndices,    SKINNED_SKELETON_OFFSET(skinData)
        lq boneWeights,    SKINNED_SKELETON_OFFSET+1(skinData)

        ; Loop counter comes from the weights themselves. The EE sorts them
        ; descending at load time, so the first zero ends the influences: 1 or 2
        ; bones is the common case and the other 2 iterations were pure waste.
        ; ftoi12 makes the test an integer compare (weight < 1/4096 is nothing);
        ; sub.x clears the slot just consumed so mr32 shifts a zero into w and
        ; the loop cannot run past the 4th bone.
        ftoi12 weightTest, boneWeights

        move final_vertex, vf00
        move final_normal, vf00

        skinWeightLoop_cull:
            mtir           boneIndex, boneIndices[x]

            MatrixLoad	BoneMatrix, BONE_MATRICES, boneIndex
  
            MatrixMultiplyVertex ts_vertex, BoneMatrix, inVert
            MatrixMultiplyVector ts_normal, BoneMatrix, inNorm

            mul ts_vertex, ts_vertex, boneWeights[x]
            mul ts_normal, ts_normal, boneWeights[x]

            add final_vertex, final_vertex, ts_vertex
            add final_normal, final_normal, ts_normal

            mr32 boneIndices, boneIndices
            mr32 boneWeights, boneWeights

            sub.x weightTest, weightTest, weightTest
            mr32  weightTest, weightTest

            mtir  weightBits, weightTest[x]
            ibne  weightBits, vi00,  skinWeightLoop_cull

        ;////////////// --- Vertex --- //////////////
        MatrixMultiplyVertex	vertex, ObjectToScreen, final_vertex ; transform each vertex by the matrix

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

        ; Next vertex of a tristrip is wound the other way round; x is -1 there
        ; and +1 for a triangle list, where this is a no-op.
        mulx.w        bfc_multiplier, bfc_multiplier, bfc_multiplier
        
        mfir.w		vertex, iADC
        ftoi4.xy    vertex, vertex
        ftoi0.z     vertex, vertex
        ;////////////////////////////////////////////

        ;//////////////// --- ST --- ////////////////
        mulq modStq, stq, q
        ;////////////////////////////////////////////

        ;//////////////// - NORMALS - /////////////////
        MatrixMultiplyVector	normal,    ObjectMatrix, final_normal ; transform each normal by the matrix
        ; Renormalise: ObjectMatrix carries the object scale, so without this the
        ; diffuse term scales with it -- an object at scale 2 renders twice as bright.
        VectorNormalize normal, normal
        
        lq light, LIGHT_AMBIENT_SUM(vi00)  ; xyz = summed ambient, w = 1.0
        move intensity, vf00

        iadd  currDirLight, vi00, vi00
        ibeq  dirLightQnt, vi00, skip_culled_directionaLightsLoop   ; do-while: 0 lights would never terminate
        culled_directionaLightsLoop:

            lq LightDirection, LIGHT_DIRECTION_PTR(currDirLight)
            
            ; Diffuse lighting
            VectorDotProduct intensity, normal, LightDirection

            maxx.xyzw  intensity, intensity, vf00

            lq LightDiffuse, LIGHT_DIFFUSE_PTR(currDirLight)

            mul diffuse, LightDiffuse, intensity[x]
            add.xyz light, light, diffuse

            iaddiu   currDirLight,  currDirLight,  1; increment the loop counter 
            ibne    dirLightQnt,  currDirLight,  culled_directionaLightsLoop	; and repeat if needed
        skip_culled_directionaLightsLoop:

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
        iaddiu         iBase,     iBase,     1                         
        iaddiu         skinData,  skinData,  2   

        iaddi   vertexCounter,  vertexCounter,  -1	; decrement the loop counter 
        ibgtz    vertexCounter,  vertexLoop	; and repeat if needed

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
    lq.w           bfc_multiplier, CLIPFAN_OFFSET(vi00) ; restart strip parity

    lq      primTag,        0(iBase) ; GIF tag - tell GS how many data we will send
    lq      matDiffuse,     1(iBase) ; material diffuse color
 
    iaddiu  skinData,        iBase,      0           ; pointer to vertex data

    ; See the comment in culled_init. Matters more here: this is the
    ; accurate-clipping path, whose scissored triangles fire their own mid-loop
    ; XGKICKs with no texture state attached.
    iaddiu     texGiftagAddr,  iBase, SKINNED_INBUF_SIZE
    ilw.x      texEnabled,     GIFTAG_OFFSET(iBase)
    iand       texEnabled,     texEnabled, texMask
    ibeq       texEnabled,     vi00,   no_tex_kick_clip
    xgkick     texGiftagAddr
no_tex_kick_clip:

    iaddiu     kickAddress,    texGiftagAddr, 3
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
        lq inVert,  SKINNED_POSITION_OFFSET(iBase)
        DecompressPositionW inVert

        lq inNormPacked, SKINNED_NORMAL_OFFSET(iBase)
        DecompressNormal8 inNorm, inNormPacked

        lq inColorPacked, SKINNED_COLOR_OFFSET(iBase)
        DecompressColor8 inColor, inColorPacked

        lq stqPacked, SKINNED_TEXCOORD_OFFSET(iBase)
        DecompressUV16 stq, stqPacked
        ;////////////////////////////////////////////

        lq boneIndices,    SKINNED_SKELETON_OFFSET(skinData)
        lq boneWeights,    SKINNED_SKELETON_OFFSET+1(skinData)

        ; Loop counter comes from the weights themselves. The EE sorts them
        ; descending at load time, so the first zero ends the influences: 1 or 2
        ; bones is the common case and the other 2 iterations were pure waste.
        ; ftoi12 makes the test an integer compare (weight < 1/4096 is nothing);
        ; sub.x clears the slot just consumed so mr32 shifts a zero into w and
        ; the loop cannot run past the 4th bone.
        ftoi12 weightTest, boneWeights

        move final_vertex, vf00
        move final_normal, vf00

        skinWeightLoop:
            mtir           boneIndex, boneIndices[x]

            MatrixLoad	BoneMatrix, BONE_MATRICES, boneIndex
  
            MatrixMultiplyVertex ts_vertex, BoneMatrix, inVert
            MatrixMultiplyVector ts_normal, BoneMatrix, inNorm

            mul ts_vertex, ts_vertex, boneWeights[x]
            mul ts_normal, ts_normal, boneWeights[x]

            add final_vertex, final_vertex, ts_vertex
            add final_normal, final_normal, ts_normal

            mr32 boneIndices, boneIndices
            mr32 boneWeights, boneWeights

            sub.x weightTest, weightTest, weightTest
            mr32  weightTest, weightTest

            mtir  weightBits, weightTest[x]
            ibne  weightBits, vi00,  skinWeightLoop

        ;////////////// --- Vertex --- //////////////
        MatrixMultiplyVertex	vertex, ObjectToScreen, final_vertex ; transform each vertex by the matrix
        move formVertex, vertex
        
        VertexPersCorrST vertex, modStq, vertex, stq 
 
        mul.xyz    vertex, vertex,     scale
        add.xyz    vertex, vertex,     offset

        move vertex1, vertex2
        move vertex2, vertex3       

        move vertex3, vertex

        VertexFpToGsXYZ2  vertex,vertex
        ;//////////////////////////////////////////// 

        ;//////////////// - NORMALS - /////////////////
        MatrixMultiplyVector	normal,    ObjectMatrix, final_normal ; transform each normal by the matrix
        ; Renormalise: ObjectMatrix carries the object scale, so without this the
        ; diffuse term scales with it -- an object at scale 2 renders twice as bright.
        VectorNormalize normal, normal
    
        lq light, LIGHT_AMBIENT_SUM(vi00)  ; xyz = summed ambient, w = 1.0
        move intensity, vf00

        iadd  currDirLight, vi00, vi00

        ilw.w       dirLightQnt,    NUM_DIR_LIGHTS(vi00) ; load active directional lights

        directionaLightsLoop: 

            lq LightDirection, LIGHT_DIRECTION_PTR(currDirLight)
            
            ; Diffuse lighting
            VectorDotProduct intensity, normal, LightDirection

            maxx.xyzw  intensity, intensity, vf00

            lq LightDiffuse, LIGHT_DIFFUSE_PTR(currDirLight)

            mul diffuse, LightDiffuse, intensity[x]
            add.xyz light, light, diffuse

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

        .include "vu1/proc/process_scissor_clip_skin.i"

        iaddiu         iBase,     iBase,     1                         
        iaddiu         skinData,  skinData,  2   

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
