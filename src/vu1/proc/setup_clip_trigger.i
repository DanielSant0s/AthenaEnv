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