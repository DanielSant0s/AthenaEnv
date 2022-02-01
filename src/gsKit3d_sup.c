#include "include/render.h"

#define GIF_TAG_TRIANGLE_GORAUD_TEXTURED_ST_REGS(ctx) \
    ((u64)(GS_TEX0_1 + ctx) << 0 ) | \
    ((u64)(GS_PRIM)         << 4 ) | \
    ((u64)(GS_RGBAQ)        << 8 ) | \
    ((u64)(GS_ST)           << 12) | \
    ((u64)(GS_XYZ2)         << 16) | \
    ((u64)(GS_RGBAQ)        << 20) | \
    ((u64)(GS_ST)           << 24) | \
    ((u64)(GS_XYZ2)         << 28) | \
    ((u64)(GS_RGBAQ)        << 32) | \
    ((u64)(GS_ST)           << 36) | \
    ((u64)(GS_XYZ2)         << 40) | \
    ((u64)(GIF_NOP)         << 44)

#define GIF_TAG_TRIANGLE_GORAUD_TEXTURED_ST_FOG_REGS(ctx) \
    ((u64)(GS_TEX0_1 + ctx) << 0 ) | \
    ((u64)(GS_PRIM)         << 4 ) | \
    ((u64)(GS_RGBAQ)        << 8 ) | \
    ((u64)(GS_ST)           << 12) | \
    ((u64)(GS_XYZF2)        << 16) | \
    ((u64)(GS_RGBAQ)        << 20) | \
    ((u64)(GS_ST)           << 24) | \
    ((u64)(GS_XYZF2)        << 28) | \
    ((u64)(GS_RGBAQ)        << 32) | \
    ((u64)(GS_ST)           << 36) | \
    ((u64)(GS_XYZF2)        << 40) | \
    ((u64)(GIF_NOP)         << 44)

// this is the same as the TRIANGLE_GORAUD primitive, but with XYZF

#define GIF_TAG_TRIANGLE_GOURAUD_FOG_REGS \
    ((u64)(GS_PRIM)  << 0)  | \
    ((u64)(GS_RGBAQ) << 4)  | \
    ((u64)(GS_XYZF2) << 8)  | \
    ((u64)(GS_RGBAQ) << 12) | \
    ((u64)(GS_XYZF2) << 16) | \
    ((u64)(GS_RGBAQ) << 20) | \
    ((u64)(GS_XYZF2) << 24) | \
    ((u64)(GIF_NOP)  << 28)

static inline u32 lzw(u32 val) {
    u32 res;
    __asm__ __volatile__ ("   plzcw   %0, %1    " : "=r" (res) : "r" (val));
    return(res);
}

static inline void gsKit_set_tw_th(const GSTEXTURE *Texture, int *tw, int *th) {
    *tw = 31 - (lzw(Texture->Width) + 1);
    if(Texture->Width > (1<<*tw))
        (*tw)++;

    *th = 31 - (lzw(Texture->Height) + 1);
    if(Texture->Height > (1<<*th))
        (*th)++;
}

void gsKit_prim_triangle_goraud_texture_3d_st(
    GSGLOBAL *gsGlobal, GSTEXTURE *Texture,
    float x1, float y1, int iz1, float u1, float v1,
    float x2, float y2, int iz2, float u2, float v2,
    float x3, float y3, int iz3, float u3, float v3,
    u64 color1, u64 color2, u64 color3
) {
    gsKit_set_texfilter(gsGlobal, Texture->Filter);
    u64* p_store;
    u64* p_data;
    const int qsize = 6;
    const int bsize = 96;

    int tw, th;
    gsKit_set_tw_th(Texture, &tw, &th);

    int ix1 = gsKit_float_to_int_x(gsGlobal, x1);
    int ix2 = gsKit_float_to_int_x(gsGlobal, x2);
    int ix3 = gsKit_float_to_int_x(gsGlobal, x3);
    int iy1 = gsKit_float_to_int_y(gsGlobal, y1);
    int iy2 = gsKit_float_to_int_y(gsGlobal, y2);
    int iy3 = gsKit_float_to_int_y(gsGlobal, y3);
 
    TexCoord st1 = (TexCoord) { { u1, v1 } };
    TexCoord st2 = (TexCoord) { { u2, v2 } };
    TexCoord st3 = (TexCoord) { { u3, v3 } };

    p_store = p_data = (u64*)gsKit_heap_alloc(gsGlobal, qsize, bsize, GSKIT_GIF_PRIM_TRIANGLE_TEXTURED);

    *p_data++ = GIF_TAG_TRIANGLE_GORAUD_TEXTURED(0);
    *p_data++ = GIF_TAG_TRIANGLE_GORAUD_TEXTURED_ST_REGS(gsGlobal->PrimContext);

    const int replace = 0; // cur_shader->tex_mode == TEXMODE_REPLACE;
    const int alpha = gsGlobal->PrimAlphaEnable;

    if (Texture->VramClut == 0) {
        *p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
            tw, th, alpha, replace,
            0, 0, 0, 0, GS_CLUT_STOREMODE_NOLOAD);
    } else {
        *p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
            tw, th, alpha, replace,
            Texture->VramClut/256, Texture->ClutPSM, 0, 0, GS_CLUT_STOREMODE_LOAD);
    }

    *p_data++ = GS_SETREG_PRIM( GS_PRIM_PRIM_TRIANGLE, 1, 1, gsGlobal->PrimFogEnable,
                gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
                0, gsGlobal->PrimContext, 0);


    *p_data++ = color1;
    *p_data++ = st1.word;
    *p_data++ = GS_SETREG_XYZ2( ix1, iy1, iz1 );

    *p_data++ = color2;
    *p_data++ = st2.word;
    *p_data++ = GS_SETREG_XYZ2( ix2, iy2, iz2 );

    *p_data++ = color3;
    *p_data++ = st3.word;
    *p_data++ = GS_SETREG_XYZ2( ix3, iy3, iz3 );
}

void gsKit_prim_triangle_gouraud_3d_fog(
    GSGLOBAL *gsGlobal, float x1, float y1, int iz1,
    float x2, float y2, int iz2,
    float x3, float y3, int iz3,
    u64 color1, u64 color2, u64 color3,
    u8 fog1, u8 fog2, u8 fog3
) {
    u64* p_store;
    u64* p_data;
    const int qsize = 4;
    const int bsize = 64;

    int ix1 = gsKit_float_to_int_x(gsGlobal, x1);
    int iy1 = gsKit_float_to_int_y(gsGlobal, y1);

    int ix2 = gsKit_float_to_int_x(gsGlobal, x2);
    int iy2 = gsKit_float_to_int_y(gsGlobal, y2);

    int ix3 = gsKit_float_to_int_x(gsGlobal, x3);
    int iy3 = gsKit_float_to_int_y(gsGlobal, y3);

    p_store = p_data = (u64*)gsKit_heap_alloc(gsGlobal, qsize, bsize, GSKIT_GIF_PRIM_TRIANGLE_GOURAUD);

    if (p_store == gsGlobal->CurQueue->last_tag) {
        *p_data++ = GIF_TAG_TRIANGLE_GOURAUD(0);
        *p_data++ = GIF_TAG_TRIANGLE_GOURAUD_FOG_REGS;
    }

    *p_data++ = GS_SETREG_PRIM( GS_PRIM_PRIM_TRIANGLE, 1, 0, gsGlobal->PrimFogEnable,
                gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
                0, gsGlobal->PrimContext, 0) ;

    *p_data++ = color1;
    *p_data++ = GS_SETREG_XYZF2(ix1, iy1, iz1, fog1);

    *p_data++ = color2;
    *p_data++ = GS_SETREG_XYZF2(ix2, iy2, iz2, fog2);

    *p_data++ = color3;
    *p_data++ = GS_SETREG_XYZF2(ix3, iy3, iz3, fog3);
}

void gsKit_prim_triangle_goraud_texture_3d_st_fog(
    GSGLOBAL *gsGlobal, GSTEXTURE *Texture,
    float x1, float y1, int iz1, float u1, float v1,
    float x2, float y2, int iz2, float u2, float v2,
    float x3, float y3, int iz3, float u3, float v3,
    u64 color1, u64 color2, u64 color3,
    u8 fog1, u8 fog2, u8 fog3
) {
    gsKit_set_texfilter(gsGlobal, Texture->Filter);
    u64* p_store;
    u64* p_data;
    int qsize = 6;
    int bsize = 96;

    int tw, th;
    gsKit_set_tw_th(Texture, &tw, &th);

    int ix1 = gsKit_float_to_int_x(gsGlobal, x1);
    int ix2 = gsKit_float_to_int_x(gsGlobal, x2);
    int ix3 = gsKit_float_to_int_x(gsGlobal, x3);
    int iy1 = gsKit_float_to_int_y(gsGlobal, y1);
    int iy2 = gsKit_float_to_int_y(gsGlobal, y2);
    int iy3 = gsKit_float_to_int_y(gsGlobal, y3);

    TexCoord st1 = (TexCoord) { { u1, v1 } };
    TexCoord st2 = (TexCoord) { { u2, v2 } };
    TexCoord st3 = (TexCoord) { { u3, v3 } };

    p_store = p_data = (u64*)gsKit_heap_alloc(gsGlobal, qsize, bsize, GSKIT_GIF_PRIM_TRIANGLE_TEXTURED);

    *p_data++ = GIF_TAG_TRIANGLE_GORAUD_TEXTURED(0);
    *p_data++ = GIF_TAG_TRIANGLE_GORAUD_TEXTURED_ST_FOG_REGS(gsGlobal->PrimContext);

    const int replace = 0; // cur_shader->tex_mode == TEXMODE_REPLACE;
    const int alpha = gsGlobal->PrimAlphaEnable;

    if (Texture->VramClut == 0) {
        *p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
            tw, th, alpha, replace,
            0, 0, 0, 0, GS_CLUT_STOREMODE_NOLOAD);
    } else {
        *p_data++ = GS_SETREG_TEX0(Texture->Vram/256, Texture->TBW, Texture->PSM,
            tw, th, alpha, replace,
            Texture->VramClut/256, Texture->ClutPSM, 0, 0, GS_CLUT_STOREMODE_LOAD);
    }

    *p_data++ = GS_SETREG_PRIM( GS_PRIM_PRIM_TRIANGLE, 1, 1, gsGlobal->PrimFogEnable,
                gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
                0, gsGlobal->PrimContext, 0);


    *p_data++ = color1;
    *p_data++ = st1.word;
    *p_data++ = GS_SETREG_XYZF2( ix1, iy1, iz1, fog1 );

    *p_data++ = color2;
    *p_data++ = st2.word;
    *p_data++ = GS_SETREG_XYZF2( ix2, iy2, iz2, fog2 );

    *p_data++ = color3;
    *p_data++ = st3.word;
    *p_data++ = GS_SETREG_XYZF2( ix3, iy3, iz3, fog3 );
}

