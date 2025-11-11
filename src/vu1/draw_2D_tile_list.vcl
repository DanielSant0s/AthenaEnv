; 2025 - Daniel Santos
; AthenaEnv Renderer

; Context Input: ScreenOffset(x, y), Camera(x, y)
; Static Input: Position(x, y)
; Dynamic Input: Array({x, y, w, h, u1, v1, u2, v2, r, g, b, a, zindex})

; Context Data

SCREEN_OFFSET        .assign  0 ; xy
GLOBAL_CAMERA        .assign  0 ; zw

; Static Data

TILEMAP_POSITION     .assign  1 ; xy

; Dynamic Data
; Currently this basically a vertex array:
;
; Input buffer layout
; x, y, w, h
; u1, v1, u2, v2
; r, g, b, a
; _, _, zindex, _
;
; Output buffer layout
;   r,   g,      b, a
;   x,   y, zindex, w
;  u1,  v1,      0, 0
; x+w, y+h, zindex, w
;  u2,  v2,      0, 0

INBUF_SIZE          .assign 201         ; Max NbrVerts (50 * 4) + prim tag
OUTBUF_SIZE         .assign 251         ; Max NbrVerts (50 * 5) + prim tag

GIFTAG_OFFSET      .assign 0
INBUF_OFFSET       .assign 1
SIZEOF_INBUF       .assign 4

RGBA .assign 0
POS  .assign 1
UV1  .assign 2
SIZE .assign 3
UV2  .assign 4

POS_SIZE_OFFSET .assign 0
UVS_OFFSET      .assign 1
COLOR_OFFSET    .assign 2
ZINDEX_OFFSET   .assign 3

.syntax new
.name VU1Draw2D_TileList
.vu
.init_vf_all
.init_vi_all

.include "vu1/include/vcl_sml.i"

--enter
--endenter
    lq  screenOffset,    SCREEN_OFFSET(vi00)

    mr32 globalCamera, screenOffset ; yzwx
    mr32 globalCamera, globalCamera ; zwxy

    lq  tmapPosition, TILEMAP_POSITION(vi00)

    sub finalOffset, vf00, vf00
    add.xy finalOffset, finalOffset, screenOffset
    add.xy finalOffset, finalOffset, globalCamera
    add.xy finalOffset, finalOffset, tmapPosition

    fcset   0x000000

init:
    xtop    iBase
    xitop   vertCount

    lq      primTag,        GIFTAG_OFFSET(iBase) ; GIF tag - tell GS how many data we will send
    iaddiu  iBase,          iBase,          1   

    iaddiu    kickAddress,    iBase,  INBUF_SIZE       ; pointer for XGKICK
    iaddiu    destAddress,    kickAddress,  1       ; helper pointer for data inserting

    sq primTag,    0(kickAddress) ; prim + tell gs how many data will be

    ; Set the GifTag EOP bit to 1 and NLOOP to the number of vertices
    iaddiu               Mask, vertCount, 0x7fff
    iaddiu               Mask, Mask, 0x01
    isw.x                Mask, 0(kickAddress)

    loi            4095.75
    addi           maxvec,  vf00,       i    

    iadd vertexCounter, vi00, vertCount ; loop vertCount times
    vertexLoop:
        lq inPosSize,  POS_SIZE_OFFSET(iBase)  
        lq inUVs,      UVS_OFFSET(iBase)  
        lq inColor,    COLOR_OFFSET(iBase)    
        lq zindex,     ZINDEX_OFFSET(iBase)

        sub outUV2, vf00, vf00
        mr32 inUV2, inUVs ; yzwx
        mr32 inUV2, inUV2 ; zwxy
        add.xy outUV2, outUV2, inUV2

        sub outUV1, vf00, vf00
        add.xy outUV1, outUV1, inUVs

        sub inPos, vf00, vf00
        add.xy inPos, inPosSize, finalOffset
        add.z  inPos, inPos, zindex

        maxx.xyzw      inPos, inPos,    vf00             
        minix.xyzw     inPos, inPos, maxvec  

        move midSize, inPos
        mr32 inSize, inPosSize ; yzwx
        mr32 inSize, inSize ; zwxy

        add.xy midSize, inSize, midSize
                                     
        maxx.xy      midSize, midSize,  vf00             
        minix.xy     midSize, midSize, maxvec           

        sq inColor,      RGBA(destAddress)      
        sq inPos,       POS(destAddress)  
        sq outUV1,       UV1(destAddress)  
        sq midSize,      SIZE(destAddress)     
        sq outUV2,       UV2(destAddress)     

        iaddiu          iBase,        iBase,          4   
        iaddiu          destAddress,  destAddress,    5

        iaddi   vertexCounter,  vertexCounter,  -1	; decrement the loop counter 
        ibne    vertexCounter,  vi00,   vertexLoop	; and repeat if needed

    xgkick kickAddress ; dispatch to the GS rasterizer.
--barrier
--cont

    b init

--exit
--endexit
