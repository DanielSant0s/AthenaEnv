;--------------------------------------------------------------
; add first element to end of the list for triangle fan
;--------------------------------------------------------------
save_last_loop:
   VertexLoad           TempVertex, XYZ2, ClipWorkBuf0
   VectorLoad           TempColor,  RGBA, ClipWorkBuf0
   VectorLoad           TempSTQ,    STQ,  ClipWorkBuf0

   VertexSave           TempVertex, XYZ2, ClipWorkBuf1
   VectorSave           TempColor,  RGBA, ClipWorkBuf1
   VectorSave           TempSTQ,    STQ,  ClipWorkBuf1
   iaddiu               ClipWorkBuf1, ClipWorkBuf1, 3

   jr                   RetAddr2