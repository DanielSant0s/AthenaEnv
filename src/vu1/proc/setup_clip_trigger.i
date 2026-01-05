    lq                   ClipTag, CLIPFAN_OFFSET(vi00)      ; load Triangle Fan Tag
 
    ;=====================================================================================
    ; If the result of ANDing 3 with the giftag's NLOOP (number of verts) is 3, the model
    ; is assumed to be a collection of triangles. Otherwise it's assumed it's a strip (-1)
    ;=====================================================================================
    mtir                 PrimitiveType, ClipTag[z]
    
    iaddi                ClipTrigger, vi00, -1
 
    ibne                 vi00, PrimitiveType, done
 
triangle:
    iaddiu               ClipTrigger, vi00, 3

done: