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
