        ;=====================================================================================
        ; Push the clipping-space vertex queue down 1
        ;=====================================================================================
        move                 CSVertex1, CSVertex2
        move                 CSVertex2, CSVertex3        

        ;=====================================================================================
        ; Apply the world to clip matrix to the vertex (push onto above stack)
        ;=====================================================================================
        move CSVertex3, formVertex
        
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
        BackfaceCull         vertex1, vertex2, vertex3
        ibne                 z_sign, vi00, triangle_outside
        
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
        PushInteger4         StackPtr, ClipFlag3, outputAddress, vertexCounter,  ClipTrigger

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
        VectorLoad           TempSTQ1, -2, normalData
        VectorLoad           TempSTQ2, -1, normalData
        VectorLoad           TempSTQ3,  0, normalData

        loi 0.5

        MatrixMultiplyVector	trans_stq, ObjectMatrix, TempSTQ1

        sub.xyzw tmp_stq, vf00, vf00
        subx.x tmp_stq, trans_stq, vf00[x]
        suby.y tmp_stq, tmp_stq, stq[y]
        addw.xyz tmp_stq, tmp_stq, vf00[w]

        muli.xy TempSTQ1, tmp_stq, I

        MatrixMultiplyVector	trans_stq, ObjectMatrix, TempSTQ2

        sub.xyzw tmp_stq, vf00, vf00
        subx.x tmp_stq, trans_stq, vf00[x]
        suby.y tmp_stq, tmp_stq, stq[y]
        addw.xyz tmp_stq, tmp_stq, vf00[w]

        muli.xy TempSTQ2, tmp_stq, I

        MatrixMultiplyVector	trans_stq, ObjectMatrix, TempSTQ3

        sub.xyzw tmp_stq, vf00, vf00
        subx.x tmp_stq, trans_stq, vf00[x]
        suby.y tmp_stq, tmp_stq, stq[y]
        addw.xyz tmp_stq, tmp_stq, vf00[w]

        muli.xy TempSTQ3, tmp_stq, I

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
        iaddiu               vertexCounter,        vi00, 3
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
        isubiu               vertexCounter, vertexCounter, 1
        ibne                 vertexCounter, vi00, LOOP_Z_MINUS
     
        ; Copy the triangle fan's first vertex to the end (in clipping work buffer 1)
        iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_1
        bal                  RetAddr2, save_last_loop
     
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
        iadd                 vertexCounter,         vi00, newVertCount
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
        isubiu               vertexCounter, vertexCounter, 1
        ibne                 vertexCounter, vi00, LOOP_Z_PLUS
     
        ; Copy the triangle fan's first vertex to the end (in clipping work buffer 0)
        iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_0
        bal                  RetAddr2, save_last_loop
     
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
        iadd                 vertexCounter,         vi00, newVertCount
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
       isubiu               vertexCounter, vertexCounter, 1
       ibne                 vertexCounter, vi00, LOOP_X_MINUS

       ; Copy the triangle fan's first vertex to the end (in clipping work buffer 1)
       iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_1
       bal                  RetAddr2, save_last_loop

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
       iadd                 vertexCounter,         vi00, newVertCount
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
       isubiu               vertexCounter, vertexCounter, 1
       ibne                 vertexCounter, vi00, LOOP_X_PLUS

       ; Copy the triangle fan's first vertex to the end (in clipping work buffer 0)
       iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_0
       bal                  RetAddr2, save_last_loop

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
       iadd                 vertexCounter,         vi00, newVertCount
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
       isubiu               vertexCounter, vertexCounter, 1
       ibne                 vertexCounter, vi00, LOOP_Y_MINUS

       ; Copy the triangle fan's first vertex to the end (in clipping work buffer 1)
       iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_1
       bal                  RetAddr2, save_last_loop

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
       iadd                 vertexCounter,         vi00, newVertCount
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
       isubiu               vertexCounter, vertexCounter, 1
       ibne                 vertexCounter, vi00, LOOP_Y_PLUS

       ; Copy the triangle fan's first vertex to the end (in clipping work buffer 0)
       iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_0
       bal                  RetAddr2, save_last_loop

       ; If all verts got clipped out, stop
       ibeq                 newVertCount, vi00, SCISSOR_END

        ;=====================================================================================
        ; If we get here, we gotta draw the triangle fan
        ;=====================================================================================
        iaddiu               ClipWorkBuf0, vi00, CLIP_WORK_BUF_0
        iaddiu               ClipWorkBuf1, vi00, CLIP_WORK_BUF_1
        iadd                 vertexCounter,    vi00, newVertCount

        iaddiu               DummyXGKickPtr, vi00, DUMMY_XGKICK_BUF
        iaddiu               kickAddress, vi00, CLIPFAN_OFFSET

        lq                   Uniqueflags,    0(kickAddress) 

        move                 ClipTag, primTag
        move.y               ClipTag, UniqueFlags

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
        iaddiu               Mask, vertexCounter, 0x7fff
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
     
        iaddi                vertexCounter, vertexCounter, -1
        ibne                 vertexCounter, vi00, LOOP2
     
        ; --- send result to GIF and stop ---
        xgkick               kickAddress

    SCISSOR_END:
    ;-----------------------------------------------------------------------------------------------------------------------------------

        ;=====================================================================================
        ; Restore context
        ;=====================================================================================
        PopInteger4          StackPtr, ClipFlag3, outputAddress, vertexCounter, ClipTrigger
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