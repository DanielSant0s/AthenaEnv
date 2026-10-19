#include <stdlib.h>
#include <string.h>

#include "ath_lua.h"
#include "ath_lua_graphics.h"

#include <graphics.h>
#include <texture_manager.h>
#include <lz4_ps2.h>
#include <athena/image.h>
#include <athena/image_list.h>
#include <athena/font.h>
#include <athena/screen.h>

/* ---------------------------------------------------------------- Image -- */

typedef struct {
    AthenaImage *image;
    bool owned;
} LuaImage;

AthenaImage *athena_lua_check_image(lua_State *L, int idx)
{
    LuaImage *img = athena_lua_check_object(L, idx, ATHENA_LUA_IMAGE);
    if (!img->image)
        luaL_error(L, "image was freed");
    return img->image;
}

AthenaImage *athena_lua_test_image(lua_State *L, int idx)
{
    LuaImage *img = athena_lua_test_object(L, idx, ATHENA_LUA_IMAGE);
    return img ? img->image : NULL;
}

void athena_lua_push_image(lua_State *L, AthenaImage *image, bool owned)
{
    LuaImage *img = athena_lua_new_object(L, ATHENA_LUA_IMAGE, sizeof(LuaImage));
    img->image = image;
    img->owned = owned;
}

static int l_image_free(lua_State *L)
{
    LuaImage *img = athena_lua_check_object(L, 1, ATHENA_LUA_IMAGE);
    if (img->image && img->owned)
        athena_image_destroy(img->image);
    img->image = NULL;
    return 0;
}

/* Image(path [, imageList]) or Image() for an empty image. */
static int l_image_new(lua_State *L)
{
    lua_settop(L, 2); /* optional args must not alias the new object */
    const char *path = luaL_optstring(L, 1, NULL);
    AthenaImage *image = path ? athena_image_create(path, true) : athena_image_create_empty(true);
    if (!image)
        return luaL_error(L, "could not load image '%s'", path ? path : "(empty)");

    athena_lua_push_image(L, image, true);

    if (!lua_isnoneornil(L, 2)) {
        AthenaImageList *list = athena_lua_check_image_list(L, 2);
        athena_image_list_append(list, image);
        /* The loader thread owns a pointer to the image until it is loaded. */
        athena_lua_retain(L, 2, -1, NULL);
    }

    return 1;
}

static int l_image_draw(lua_State *L)
{
    athena_image_draw(athena_lua_check_image(L, 1), athena_lua_checkfloat(L, 2), athena_lua_checkfloat(L, 3));
    return 0;
}

static int l_image_ready(lua_State *L)
{
    lua_pushboolean(L, athena_image_is_loaded(athena_lua_check_image(L, 1)));
    return 1;
}

static int l_image_optimize(lua_State *L)
{
    lua_pushboolean(L, athena_image_optimize(athena_lua_check_image(L, 1)));
    return 1;
}

static int l_image_lock(lua_State *L)
{
    lua_pushboolean(L, athena_image_lock(athena_lua_check_image(L, 1)));
    return 1;
}

static int l_image_unlock(lua_State *L)
{
    lua_pushboolean(L, athena_image_unlock(athena_lua_check_image(L, 1)));
    return 1;
}

static int l_image_locked(lua_State *L)
{
    lua_pushboolean(L, athena_image_locked(athena_lua_check_image(L, 1)));
    return 1;
}

#define IMAGE_FLOAT_PROP(field) \
    static int get_img_##field(lua_State *L) { lua_pushnumber(L, athena_lua_check_image(L, 1)->field); return 1; } \
    static int set_img_##field(lua_State *L) { athena_lua_check_image(L, 1)->field = athena_lua_checkfloat(L, 2); return 0; }

IMAGE_FLOAT_PROP(width)
IMAGE_FLOAT_PROP(height)
IMAGE_FLOAT_PROP(startx)
IMAGE_FLOAT_PROP(starty)
IMAGE_FLOAT_PROP(endx)
IMAGE_FLOAT_PROP(endy)
IMAGE_FLOAT_PROP(angle)

static int get_img_color(lua_State *L)
{
    athena_lua_pushu32(L, athena_lua_check_image(L, 1)->color);
    return 1;
}

static int set_img_color(lua_State *L)
{
    athena_lua_check_image(L, 1)->color = athena_lua_checku32(L, 2);
    return 0;
}

static int get_img_filter(lua_State *L)
{
    lua_pushinteger(L, athena_lua_check_image(L, 1)->tex->Filter);
    return 1;
}

static int set_img_filter(lua_State *L)
{
    athena_lua_check_image(L, 1)->tex->Filter = athena_lua_checku32(L, 2);
    return 0;
}

static int get_img_size(lua_State *L)
{
    GSSURFACE *tex = athena_lua_check_image(L, 1)->tex;
    lua_pushinteger(L, athena_surface_size(tex->Width, tex->Height, tex->PSM));
    return 1;
}

static int get_img_bpp(lua_State *L)
{
    int bpp = 0;
    switch (athena_lua_check_image(L, 1)->tex->PSM) {
        case GS_PSM_T4: bpp = 4; break;
        case GS_PSM_T8: bpp = 8; break;
        case GS_PSM_CT16: case GS_PSM_CT16S: case GS_PSMZ_16S: bpp = 16; break;
        case GS_PSM_CT24: case GS_PSMZ_24: bpp = 24; break;
        case GS_PSM_CT32: case GS_PSMZ_32: bpp = 32; break;
    }
    lua_pushinteger(L, bpp);
    return 1;
}

static int set_img_bpp(lua_State *L)
{
    GSSURFACE *tex = athena_lua_check_image(L, 1)->tex;
    switch (luaL_checkinteger(L, 2)) {
        case 4: tex->PSM = GS_PSM_T4; break;
        case 8: tex->PSM = GS_PSM_T8; break;
        case 16: tex->PSM = GS_PSM_CT16S; break;
        case 24: tex->PSM = GS_PSM_CT24; break;
        case 32: tex->PSM = GS_PSM_CT32; break;
        case 18: tex->PSM = GS_PSMZ_16S; break;
        case 26: tex->PSM = GS_PSMZ_24; break;
        case 34: tex->PSM = GS_PSMZ_32; break;
    }
    return 0;
}

/* Pixels/palette are exposed as Buffers that view the texture memory; the
 * Buffer keeps the image alive. Assigning an owned Buffer transfers its
 * memory to the texture. */
static int get_img_pixels(lua_State *L)
{
    GSSURFACE *tex = athena_lua_check_image(L, 1)->tex;
    athena_texture_pin(tex);
    if (!tex->Mem)
        return 0;
    athena_lua_push_buffer(L, tex->Mem, athena_surface_size(tex->Width, tex->Height, tex->PSM), false);
    athena_lua_retain(L, -1, 1, "__image");
    return 1;
}

static int set_img_pixels(lua_State *L)
{
    GSSURFACE *tex = athena_lua_check_image(L, 1)->tex;
    AthenaLuaBuffer *buf = athena_lua_check_object(L, 2, ATHENA_LUA_BUFFER);
    if (!tex->Delayed || buf->data == (uint8_t *)tex->Mem)
        return 0;
    if (!buf->owned)
        return luaL_error(L, "pixels must be an owned Buffer (Buffer.new)");

    athena_mem_unregister_texture(tex);
    if (tex->Mem)
        free(tex->Mem);
    tex->Mem = (u32 *)buf->data;
    buf->owned = false;
    athena_lua_retain(L, 2, 1, "__image");
    return 0;
}

static int get_img_palette(lua_State *L)
{
    GSSURFACE *tex = athena_lua_check_image(L, 1)->tex;
    uint32_t size;
    if (tex->PSM == GS_PSM_T4)
        size = athena_surface_size(8, 2, GS_PSM_CT32);
    else if (tex->PSM == GS_PSM_T8)
        size = athena_surface_size(16, 16, GS_PSM_CT32);
    else
        return 0;

    athena_lua_push_buffer(L, tex->Clut, size, false);
    athena_lua_retain(L, -1, 1, "__image");
    return 1;
}

static int set_img_palette(lua_State *L)
{
    GSSURFACE *tex = athena_lua_check_image(L, 1)->tex;
    AthenaLuaBuffer *buf = athena_lua_check_object(L, 2, ATHENA_LUA_BUFFER);
    if (!tex->Clut || !tex->Delayed || buf->data == (uint8_t *)tex->Clut)
        return 0;
    if (!buf->owned)
        return luaL_error(L, "palette must be an owned Buffer (Buffer.new)");

    free(tex->Clut);
    tex->Clut = (u32 *)buf->data;
    buf->owned = false;
    athena_lua_retain(L, 2, 1, "__image");
    return 0;
}

#define IMAGE_TEX_PROP(name, field) \
    static int get_img_##name(lua_State *L) { lua_pushinteger(L, athena_lua_check_image(L, 1)->tex->field); return 1; } \
    static int set_img_##name(lua_State *L) { athena_lua_check_image(L, 1)->tex->field = athena_lua_checku32(L, 2); return 0; }

IMAGE_TEX_PROP(texWidth, Width)
IMAGE_TEX_PROP(texHeight, Height)
IMAGE_TEX_PROP(renderable, PageAligned)

static int get_img_delayed(lua_State *L)
{
    lua_pushboolean(L, athena_lua_check_image(L, 1)->delayed);
    return 1;
}

#define IMG_PROP(name) { #name, get_img_##name, set_img_##name }

static const athena_lua_prop image_props[] = {
    IMG_PROP(width), IMG_PROP(height), IMG_PROP(startx), IMG_PROP(starty),
    IMG_PROP(endx), IMG_PROP(endy), IMG_PROP(angle), IMG_PROP(color), IMG_PROP(filter),
    { "size", get_img_size, NULL },
    IMG_PROP(bpp), IMG_PROP(pixels), IMG_PROP(palette),
    IMG_PROP(texWidth), IMG_PROP(texHeight), IMG_PROP(renderable),
    { "delayed", get_img_delayed, NULL },
    { NULL, NULL, NULL }
};

static const luaL_Reg image_methods[] = {
    { "draw", l_image_draw },
    { "ready", l_image_ready },
    { "optimize", l_image_optimize },
    { "free", l_image_free },
    { "lock", l_image_lock },
    { "unlock", l_image_unlock },
    { "locked", l_image_locked },
    { NULL, NULL }
};

static int l_image_copy_block(lua_State *L)
{
    athena_image_copy_vram_block(athena_lua_check_image(L, 1), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3),
                                 athena_lua_check_image(L, 4), (int)luaL_checkinteger(L, 5), (int)luaL_checkinteger(L, 6));
    return 0;
}

static const luaL_Reg image_funcs[] = {
    { "copyVRAMBlock", l_image_copy_block },
    { NULL, NULL }
};

static const athena_lua_const image_consts[] = {
    { "NEAREST", GS_FILTER_NEAREST },
    { "LINEAR", GS_FILTER_LINEAR },
    { NULL, 0 }
};

static const athena_lua_class image_class = {
    .name = ATHENA_LUA_IMAGE,
    .methods = image_methods,
    .props = image_props,
    .gc = l_image_free,
};

/* ------------------------------------------------------------ ImageList -- */

#define IMAGELIST_CLASS "ImageList"

typedef struct {
    AthenaImageList *list;
} LuaImageList;

AthenaImageList *athena_lua_check_image_list(lua_State *L, int idx)
{
    LuaImageList *l = athena_lua_check_object(L, idx, IMAGELIST_CLASS);
    if (!l->list)
        luaL_error(L, "image list was destroyed");
    return l->list;
}

static int l_imagelist_new(lua_State *L)
{
    AthenaImageList *list = athena_image_list_create();
    if (!list)
        return luaL_error(L, "could not create image list");
    LuaImageList *l = athena_lua_new_object(L, IMAGELIST_CLASS, sizeof(LuaImageList));
    l->list = list;
    return 1;
}

static int l_imagelist_gc(lua_State *L)
{
    LuaImageList *l = athena_lua_check_object(L, 1, IMAGELIST_CLASS);
    if (l->list)
        athena_image_list_destroy(l->list);
    l->list = NULL;
    return 0;
}

static int l_imagelist_process(lua_State *L)
{
    athena_image_list_process(athena_lua_check_image_list(L, 1));
    return 0;
}

static const luaL_Reg imagelist_methods[] = {
    { "process", l_imagelist_process },
    { NULL, NULL }
};

static const athena_lua_class imagelist_class = {
    .name = IMAGELIST_CLASS,
    .methods = imagelist_methods,
    .gc = l_imagelist_gc,
};

/* ----------------------------------------------------------------- Font -- */

#define FONT_CLASS "Font"
#define FONTRENDER_CLASS "FontRender"

typedef struct {
    AthenaFont *font;
} LuaFont;

typedef struct {
    AthenaFontRender *render;
} LuaFontRender;

static AthenaFont *check_font(lua_State *L, int idx)
{
    LuaFont *f = athena_lua_check_object(L, idx, FONT_CLASS);
    if (!f->font)
        luaL_error(L, "font was destroyed");
    return f->font;
}

static int l_font_new(lua_State *L)
{
    const char *path = luaL_optstring(L, 1, NULL);
    AthenaFont *font = athena_font_load(path);
    if (!font)
        return luaL_error(L, "could not load font '%s'", path ? path : "default");
    LuaFont *f = athena_lua_new_object(L, FONT_CLASS, sizeof(LuaFont));
    f->font = font;
    return 1;
}

static int l_font_gc(lua_State *L)
{
    LuaFont *f = athena_lua_check_object(L, 1, FONT_CLASS);
    if (f->font)
        athena_font_destroy(f->font);
    f->font = NULL;
    return 0;
}

static int l_font_print(lua_State *L)
{
    athena_font_print(check_font(L, 1), athena_lua_checkfloat(L, 2), athena_lua_checkfloat(L, 3),
                      luaL_tolstring(L, 4, NULL));
    return 0;
}

static int l_font_gettextsize(lua_State *L)
{
    Coords size = athena_font_get_text_size(check_font(L, 1), luaL_tolstring(L, 2, NULL));
    lua_createtable(L, 0, 2);
    athena_lua_set_num(L, "width", size.width);
    athena_lua_set_num(L, "height", size.height);
    return 1;
}

static int l_font_render(lua_State *L)
{
    AthenaFontRender *render = athena_font_render_create(check_font(L, 1), luaL_tolstring(L, 2, NULL));
    if (!render)
        return luaL_error(L, "could not render text");
    LuaFontRender *r = athena_lua_new_object(L, FONTRENDER_CLASS, sizeof(LuaFontRender));
    r->render = render;
    athena_lua_retain(L, -1, 1, "__font");
    return 1;
}

#define FONT_FLOAT_PROP(field) \
    static int get_font_##field(lua_State *L) { lua_pushnumber(L, check_font(L, 1)->field); return 1; } \
    static int set_font_##field(lua_State *L) { check_font(L, 1)->field = athena_lua_checkfloat(L, 2); return 0; }
#define FONT_U32_PROP(field) \
    static int get_font_##field(lua_State *L) { athena_lua_pushu32(L, check_font(L, 1)->field); return 1; } \
    static int set_font_##field(lua_State *L) { check_font(L, 1)->field = athena_lua_checku32(L, 2); return 0; }

FONT_FLOAT_PROP(scale)
FONT_FLOAT_PROP(outline)
FONT_FLOAT_PROP(dropshadow)
FONT_U32_PROP(color)
FONT_U32_PROP(outline_color)
FONT_U32_PROP(dropshadow_color)
FONT_U32_PROP(align)

#define FONT_PROP(name) { #name, get_font_##name, set_font_##name }

static const athena_lua_prop font_props[] = {
    FONT_PROP(scale), FONT_PROP(outline), FONT_PROP(dropshadow), FONT_PROP(color),
    FONT_PROP(outline_color), FONT_PROP(dropshadow_color), FONT_PROP(align),
    { NULL, NULL, NULL }
};

static const luaL_Reg font_methods[] = {
    { "print", l_font_print },
    { "render", l_font_render },
    { "getTextSize", l_font_gettextsize },
    { NULL, NULL }
};

static const athena_lua_const font_consts[] = {
    { "ALIGN_TOP", ALIGN_TOP }, { "ALIGN_BOTTOM", ALIGN_BOTTOM }, { "ALIGN_VCENTER", ALIGN_VCENTER },
    { "ALIGN_LEFT", ALIGN_LEFT }, { "ALIGN_RIGHT", ALIGN_RIGHT }, { "ALIGN_HCENTER", ALIGN_HCENTER },
    { "ALIGN_NONE", ALIGN_NONE }, { "ALIGN_CENTER", ALIGN_CENTER },
    { NULL, 0 }
};

static const athena_lua_class font_class = {
    .name = FONT_CLASS,
    .methods = font_methods,
    .props = font_props,
    .gc = l_font_gc,
};

static int l_fontrender_print(lua_State *L)
{
    LuaFontRender *r = athena_lua_check_object(L, 1, FONTRENDER_CLASS);
    athena_font_render_print(r->render, athena_lua_checkfloat(L, 2), athena_lua_checkfloat(L, 3));
    return 0;
}

static int l_fontrender_gc(lua_State *L)
{
    LuaFontRender *r = athena_lua_check_object(L, 1, FONTRENDER_CLASS);
    if (r->render)
        athena_font_render_destroy(r->render);
    r->render = NULL;
    return 0;
}

static int get_fontrender_width(lua_State *L)
{
    LuaFontRender *r = athena_lua_check_object(L, 1, FONTRENDER_CLASS);
    lua_pushnumber(L, athena_font_render_get_size(r->render).width);
    return 1;
}

static int get_fontrender_height(lua_State *L)
{
    LuaFontRender *r = athena_lua_check_object(L, 1, FONTRENDER_CLASS);
    lua_pushnumber(L, athena_font_render_get_size(r->render).height);
    return 1;
}

static const athena_lua_prop fontrender_props[] = {
    { "width", get_fontrender_width, NULL },
    { "height", get_fontrender_height, NULL },
    { NULL, NULL, NULL }
};

static const luaL_Reg fontrender_methods[] = {
    { "print", l_fontrender_print },
    { NULL, NULL }
};

static const athena_lua_class fontrender_class = {
    .name = FONTRENDER_CLASS,
    .methods = fontrender_methods,
    .props = fontrender_props,
    .gc = l_fontrender_gc,
};

/* --------------------------------------------------------------- Screen -- */

/* GS ALPHA register fields; 64-bit values don't fit Lua's int32 integers so
 * blend equations travel as {a, b, c, d, fix} tables. */
static void push_alpha_table(lua_State *L, uint64_t v)
{
    lua_createtable(L, 0, 5);
    athena_lua_set_int(L, "a", (int)(v & 3));
    athena_lua_set_int(L, "b", (int)((v >> 2) & 3));
    athena_lua_set_int(L, "c", (int)((v >> 4) & 3));
    athena_lua_set_int(L, "d", (int)((v >> 6) & 3));
    athena_lua_set_int(L, "fix", (int)((v >> 32) & 0xFF));
}

uint64_t athena_lua_check_alpha_equation(lua_State *L, int idx)
{
    if (lua_type(L, idx) == LUA_TNUMBER)
        return (uint64_t)athena_lua_checku32(L, idx);

    luaL_checktype(L, idx, LUA_TTABLE);
    int a = 0, b = 1, c = 0, d = 1, fix = 0;
    athena_lua_field_int(L, idx, "a", &a);
    athena_lua_field_int(L, idx, "b", &b);
    athena_lua_field_int(L, idx, "c", &c);
    athena_lua_field_int(L, idx, "d", &d);
    athena_lua_field_int(L, idx, "fix", &fix);
    return athena_screen_alpha_equation(a, b, c, d, fix);
}

static int l_screen_display(lua_State *L)
{
    if (!lua_isnil(L, 1))
        luaL_checktype(L, 1, LUA_TFUNCTION);
    athena_lua_set_render_loop(L, 1);
    return 0;
}

static int l_screen_flip(lua_State *L)
{
    athena_screen_flip();
    athena_lua_pump(L);
    return 0;
}

static int l_screen_clear(lua_State *L)
{
    athena_screen_clear(athena_lua_optu32(L, 1, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00)));
    return 0;
}

static int l_screen_clear_color(lua_State *L)
{
    athena_screen_set_clear_color(athena_lua_checku32(L, 1));
    return 0;
}

static int l_screen_vblank(lua_State *L)
{
    athena_screen_wait_vblank();
    return 0;
}

static int l_screen_vsync(lua_State *L)
{
    athena_screen_set_vsync(athena_lua_tobool(L, 1));
    return 0;
}

static int l_screen_fcount(lua_State *L)
{
    athena_screen_set_frame_counter(athena_lua_tobool(L, 1));
    return 0;
}

static int l_screen_set_frame_overlap(lua_State *L)
{
    athena_screen_set_frame_overlap(athena_lua_tobool(L, 1));
    return 0;
}

static int l_screen_get_frame_overlap(lua_State *L)
{
    lua_pushboolean(L, athena_screen_get_frame_overlap());
    return 1;
}

static int l_screen_get_profile(lua_State *L)
{
    const prof_report *r = athena_screen_get_profile();
    lua_createtable(L, 0, 7);
    athena_lua_set_num(L, "frame", r->frame_ms);
    athena_lua_set_num(L, "script", r->section_ms[PROF_SCRIPT]);
    athena_lua_set_num(L, "render", r->section_ms[PROF_RENDER]);
    athena_lua_set_num(L, "anim", r->section_ms[PROF_ANIM]);
    athena_lua_set_num(L, "physics", r->section_ms[PROF_PHYSICS]);
    athena_lua_set_num(L, "gpuWait", r->section_ms[PROF_GPU_WAIT]);
    athena_lua_set_num(L, "vsync", r->section_ms[PROF_VSYNC]);
    return 1;
}

static int l_screen_get_free_vram(lua_State *L)
{
    lua_pushinteger(L, athena_screen_get_free_vram((int)luaL_optinteger(L, 1, VRAM_USED_TOTAL)));
    return 1;
}

static int l_screen_get_fps(lua_State *L)
{
    /* the interval is also the sampling period; 0 would stop the counter */
    lua_pushnumber(L, athena_screen_get_fps((int)luaL_optinteger(L, 1, 60)));
    return 1;
}

static int l_screen_get_mode(lua_State *L)
{
    AthenaVideoMode mode;
    athena_screen_get_mode(&mode);
    lua_createtable(L, 0, 9);
    athena_lua_set_int(L, "mode", mode.mode);
    athena_lua_set_int(L, "width", mode.width);
    athena_lua_set_int(L, "height", mode.height);
    athena_lua_set_int(L, "psm", mode.psm);
    athena_lua_set_int(L, "interlace", mode.interlace);
    athena_lua_set_int(L, "field", mode.field);
    athena_lua_set_int(L, "psmz", mode.psmz);
    athena_lua_set_bool(L, "zbuffering", mode.zbuffering);
    athena_lua_set_bool(L, "double_buffering", mode.double_buffering);
    return 1;
}

static int l_screen_set_mode(lua_State *L)
{
    AthenaVideoMode mode = { 0 };
    luaL_checktype(L, 1, LUA_TTABLE);
    athena_lua_field_int(L, 1, "mode", &mode.mode);
    athena_lua_field_int(L, 1, "width", &mode.width);
    athena_lua_field_int(L, 1, "height", &mode.height);
    athena_lua_field_int(L, 1, "psm", &mode.psm);
    athena_lua_field_int(L, 1, "interlace", &mode.interlace);
    athena_lua_field_int(L, 1, "field", &mode.field);
    athena_lua_field_int(L, 1, "psmz", &mode.psmz);
    athena_lua_field_bool(L, 1, "zbuffering", &mode.zbuffering);
    athena_lua_field_bool(L, 1, "double_buffering", &mode.double_buffering);
    athena_lua_field_u32(L, 1, "pass_count", &mode.pass_count);
    athena_screen_set_mode(&mode);
    return 0;
}

static int l_screen_alpha_equation(lua_State *L)
{
    push_alpha_table(L, athena_screen_alpha_equation((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2),
                                                     (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4),
                                                     (int)luaL_optinteger(L, 5, 0)));
    return 1;
}

static int l_screen_set_param(lua_State *L)
{
    int param = (int)luaL_checkinteger(L, 1);
    uint64_t value;

    switch (param) {
        case ALPHA_TEST_ENABLE:
        case DST_ALPHA_TEST_ENABLE:
        case DEPTH_TEST_ENABLE:
        case PIXEL_ALPHA_BLEND_ENABLE:
            value = athena_lua_tobool(L, 2);
            break;
        case ALPHA_TEST_METHOD:
        case ALPHA_TEST_REF:
        case ALPHA_TEST_FAIL:
        case DST_ALPHA_TEST_METHOD:
        case DEPTH_TEST_METHOD:
        case COLOR_CLAMP_MODE:
            value = athena_lua_checku32(L, 2);
            break;
        case ALPHA_BLEND_EQUATION:
            value = athena_lua_check_alpha_equation(L, 2);
            break;
        case SCISSOR_BOUNDS: {
            int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
            luaL_checktype(L, 2, LUA_TTABLE);
            athena_lua_field_int(L, 2, "x1", &x1);
            athena_lua_field_int(L, 2, "y1", &y1);
            athena_lua_field_int(L, 2, "x2", &x2);
            athena_lua_field_int(L, 2, "y2", &y2);
            value = GS_SETREG_SCISSOR_1(x1, x2, y1, y2);
            break;
        }
        default:
            return 0;
    }

    athena_screen_set_param(param, value);
    return 0;
}

static int l_screen_get_param(lua_State *L)
{
    int param = (int)luaL_checkinteger(L, 1);
    uint64_t value = athena_screen_get_param(param);

    switch (param) {
        case ALPHA_TEST_ENABLE:
        case ALPHA_TEST_METHOD:
        case ALPHA_TEST_REF:
        case ALPHA_TEST_FAIL:
        case DST_ALPHA_TEST_ENABLE:
        case DST_ALPHA_TEST_METHOD:
        case DEPTH_TEST_ENABLE:
        case DEPTH_TEST_METHOD:
        case PIXEL_ALPHA_BLEND_ENABLE:
        case COLOR_CLAMP_MODE:
            lua_pushinteger(L, (lua_Integer)value);
            return 1;
        case ALPHA_BLEND_EQUATION: {
            AthenaAlphaBlend blend;
            if (!athena_screen_get_alpha_blend(param, &blend))
                return 0;
            lua_createtable(L, 0, 5);
            athena_lua_set_int(L, "a", blend.a);
            athena_lua_set_int(L, "b", blend.b);
            athena_lua_set_int(L, "c", blend.c);
            athena_lua_set_int(L, "d", blend.d);
            athena_lua_set_int(L, "fix", blend.fix);
            return 1;
        }
        case SCISSOR_BOUNDS: {
            AthenaScissorBounds bounds;
            if (!athena_screen_get_scissor_bounds(param, &bounds))
                return 0;
            lua_createtable(L, 0, 4);
            athena_lua_set_int(L, "x1", bounds.x1);
            athena_lua_set_int(L, "y1", bounds.y1);
            athena_lua_set_int(L, "x2", bounds.x2);
            athena_lua_set_int(L, "y2", bounds.y2);
            return 1;
        }
    }
    return 0;
}

/* Screen buffer Image objects live in a registry table so getBuffer returns
 * the same object each time: [1..3] = originals, [4..6] = current. */
static const char screen_buffers_key = 0;

static int check_buffer_id(lua_State *L, int idx)
{
    int id = (int)luaL_checkinteger(L, idx);
    if (id < 0 || id > 2)
        luaL_argerror(L, idx, "invalid buffer id");
    return id;
}

static void push_screen_buffers(lua_State *L)
{
    lua_rawgetp(L, LUA_REGISTRYINDEX, &screen_buffers_key);
}

static int l_screen_init_buffers(lua_State *L)
{
    push_screen_buffers(L);
    bool have = lua_istable(L, -1);
    lua_pop(L, 1);
    if (athena_screen_buffers_ready() && have)
        return 0;

    if (athena_screen_init_buffers() < 0)
        return luaL_error(L, "could not init screen buffers");

    lua_createtable(L, 6, 0);
    for (int i = 0; i < 3; i++) {
        athena_lua_push_image(L, athena_screen_get_buffer(i), false);
        lua_pushvalue(L, -1);
        lua_rawseti(L, -3, i + 1);
        lua_rawseti(L, -2, i + 4);
    }
    lua_rawsetp(L, LUA_REGISTRYINDEX, &screen_buffers_key);
    return 0;
}

static void require_buffers(lua_State *L)
{
    if (!athena_screen_buffers_ready())
        luaL_error(L, "call Screen.initBuffers() before using this function");
}

static int l_screen_get_buffer(lua_State *L)
{
    require_buffers(L);
    int id = check_buffer_id(L, 1);
    push_screen_buffers(L);
    lua_rawgeti(L, -1, id + 4);
    return 1;
}

static int l_screen_set_buffer(lua_State *L)
{
    require_buffers(L);
    int id = check_buffer_id(L, 1);
    AthenaImage *image = athena_lua_check_image(L, 2);
    athena_screen_set_buffer(id, image, athena_lua_optu32(L, 3, 0));

    if (!getGSGLOBAL()->PrimContext) {
        push_screen_buffers(L);
        lua_pushvalue(L, 2);
        lua_rawseti(L, -2, id + 4);
    }
    return 0;
}

static int l_screen_reset_buffers(lua_State *L)
{
    require_buffers(L);
    athena_screen_reset_buffers();
    push_screen_buffers(L);
    for (int i = 1; i <= 3; i++) {
        lua_rawgeti(L, -1, i);
        lua_rawseti(L, -2, i + 3);
    }
    return 0;
}

static int l_screen_switch_context(lua_State *L)
{
    lua_pushinteger(L, athena_screen_switch_context());
    return 1;
}

static int l_screen_flush(lua_State *L)
{
    athena_screen_flush();
    return 0;
}

/* nil -> identity ramp, function(i) -> color, Buffer (>= 1KB) or 256-entry table (1-based). */
static void parse_lut_entries(lua_State *L, int idx, uint32_t out[256])
{
    if (lua_isnoneornil(L, idx)) {
        for (uint32_t i = 0; i < 256; i++)
            out[i] = i | (i << 8) | (i << 16) | (0x80u << 24);
        return;
    }

    if (lua_isfunction(L, idx)) {
        for (uint32_t i = 0; i < 256; i++) {
            lua_pushvalue(L, idx);
            lua_pushinteger(L, i);
            lua_call(L, 1, 1);
            out[i] = athena_lua_optu32(L, -1, 0);
            lua_pop(L, 1);
        }
        return;
    }

    uint32_t size;
    const uint8_t *bytes = athena_lua_test_object(L, idx, ATHENA_LUA_BUFFER) ? athena_lua_to_bytes(L, idx, &size) : NULL;
    if (bytes) {
        if (size < 256 * sizeof(uint32_t))
            luaL_argerror(L, idx, "LUT buffer needs 1024 bytes");
        memcpy(out, bytes, 256 * sizeof(uint32_t));
        return;
    }

    luaL_checktype(L, idx, LUA_TTABLE);
    for (uint32_t i = 0; i < 256; i++) {
        lua_geti(L, idx, i + 1);
        out[i] = athena_lua_optu32(L, -1, 0);
        lua_pop(L, 1);
    }
}

static int l_screen_create_shuffle_lut(lua_State *L)
{
    uint32_t entries[256];
    const uint32_t *ptr = NULL;
    if (!lua_isnoneornil(L, 1)) {
        parse_lut_entries(L, 1, entries);
        ptr = entries;
    }

    AthenaImage *lut = athena_screen_create_shuffle_lut(ptr);
    if (!lut)
        return luaL_error(L, "out of memory");
    athena_lua_push_image(L, lut, true);
    return 1;
}

static int l_screen_update_shuffle_lut(lua_State *L)
{
    AthenaImage *lut = athena_lua_check_image(L, 1);
    uint32_t entries[256];
    parse_lut_entries(L, 2, entries);
    athena_screen_update_shuffle_lut(lut, entries);
    return 0;
}

static void parse_fog_options(lua_State *L, int idx, gs_depth_fog_config *cfg)
{
    if (!lua_istable(L, idx))
        return;
    int curve;
    athena_lua_field_float(L, idx, "near", &cfg->near_z);
    athena_lua_field_float(L, idx, "far", &cfg->far_z);
    athena_lua_field_float(L, idx, "density", &cfg->density);
    athena_lua_field_float(L, idx, "skyFog", &cfg->sky_fog);
    if (athena_lua_field_int(L, idx, "curve", &curve))
        cfg->curve = (eFogCurve)curve;
    athena_lua_field_int(L, idx, "shift", &cfg->shift);
}

static void init_fog_config(lua_State *L, int base, gs_depth_fog_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->color = athena_lua_checku32(L, base);
    cfg->start = athena_lua_checkfloat(L, base + 1);
    cfg->end = athena_lua_checkfloat(L, base + 2);
    cfg->density = 1.5f;
    cfg->sky_fog = 0.0f;
    cfg->curve = FOG_CURVE_LINEAR;
    parse_fog_options(L, base + 3, cfg);
}

static int l_screen_build_depth_fog_lut(lua_State *L)
{
    AthenaImage *lut = athena_lua_check_image(L, 1);
    gs_depth_fog_config cfg;
    init_fog_config(L, 2, &cfg);
    athena_screen_build_depth_fog_lut(lut, &cfg);
    return 0;
}

static int l_screen_depth_fog(lua_State *L)
{
    gs_depth_fog_config cfg;
    init_fog_config(L, 1, &cfg);
    athena_screen_depth_fog(&cfg);
    return 0;
}

static GSSURFACE *resolve_shuffle_surface(lua_State *L, int idx, GSSURFACE *def)
{
    if (lua_isnoneornil(L, idx))
        return def;
    if (lua_type(L, idx) == LUA_TNUMBER) {
        int id = (int)lua_tointeger(L, idx);
        return (id >= 0 && id <= 2) ? main_screen_buffer[id] : def;
    }
    AthenaImage *img = athena_lua_test_image(L, idx);
    return (img && img->tex) ? img->tex : def;
}

/* channelShuffle(source, lut [, inChannel] [, options]) */
static int l_screen_channel_shuffle(lua_State *L)
{
    GSSURFACE *src = resolve_shuffle_surface(L, 1, main_screen_buffer[DEPTH_BUFFER]);
    AthenaImage *lut = athena_lua_check_image(L, 2);

    gs_channel_shuffle_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.in_channel = CHANNEL_SHUFFLE_BLUE;
    cfg.out_mask = CHANNEL_SHUFFLE_RGB;
    cfg.alpha_blend = true;
    cfg.blend_equation = GS_ALPHA_BLEND_NORMAL;
    cfg.alpha_test = true;
    cfg.z_test_enable = false;
    cfg.z_test_method = DEPTH_ALWAYS;

    int opts = 0;
    if (lua_type(L, 3) == LUA_TNUMBER)
        cfg.in_channel = athena_lua_checku32(L, 3);
    if (lua_istable(L, 3))
        opts = 3;
    else if (lua_istable(L, 4))
        opts = 4;

    GSSURFACE *dst = main_screen_buffer[DRAW_BUFFER];
    GSSURFACE raw_src, raw_dst;

    if (opts) {
        bool raw_source = false, raw_destination = false, b;
        uint32_t zm;

        lua_getfield(L, opts, "dst");
        dst = resolve_shuffle_surface(L, -1, dst);
        lua_pop(L, 1);

        athena_lua_field_u32(L, opts, "inChannel", &cfg.in_channel);
        athena_lua_field_u32(L, opts, "outMask", &cfg.out_mask);
        if (athena_lua_field_bool(L, opts, "alphaBlend", &b)) cfg.alpha_blend = b;
        if (lua_getfield(L, opts, "blendEquation") != LUA_TNIL)
            cfg.blend_equation = athena_lua_check_alpha_equation(L, -1);
        lua_pop(L, 1);
        if (athena_lua_field_bool(L, opts, "alphaTest", &b)) cfg.alpha_test = b;
        if (athena_lua_field_bool(L, opts, "zTest", &b)) cfg.z_test_enable = b;
        if (athena_lua_field_u32(L, opts, "zTestMethod", &zm)) cfg.z_test_method = (uint8_t)zm;
        athena_lua_field_u32(L, opts, "zRef", &cfg.z_ref);
        athena_lua_field_u32(L, opts, "x", &cfg.x);
        athena_lua_field_u32(L, opts, "y", &cfg.y);
        athena_lua_field_u32(L, opts, "width", &cfg.width);
        athena_lua_field_u32(L, opts, "height", &cfg.height);
        athena_lua_field_bool(L, opts, "rawSource", &raw_source);
        athena_lua_field_bool(L, opts, "rawDestination", &raw_destination);

        /* Stack-only views that reinterpret the bytes as CT32 (see JS binding). */
        if (raw_source) {
            raw_src = *src;
            raw_src.PSM = GS_PSM_CT32;
            src = &raw_src;
        }
        if (raw_destination) {
            raw_dst = *dst;
            raw_dst.PSM = GS_PSM_CT32;
            dst = &raw_dst;
        }
    }

    gs_channel_shuffle_ex(dst, src, lut->tex, &cfg);
    return 0;
}

static int l_screen_depth_bitshift(lua_State *L)
{
    gs_depth_bitshift((int)luaL_optinteger(L, 1, 1));
    return 0;
}

static int l_screen_depth_invert_blue(lua_State *L)
{
    gs_depth_invert_blue();
    return 0;
}

static const luaL_Reg screen_funcs[] = {
    { "flip", l_screen_flip },
    { "clear", l_screen_clear },
    { "getMemoryStats", l_screen_get_free_vram },
    { "getFPS", l_screen_get_fps },
    { "waitVblankStart", l_screen_vblank },
    { "setVSync", l_screen_vsync },
    { "setFrameCounter", l_screen_fcount },
    { "setFrameOverlap", l_screen_set_frame_overlap },
    { "getFrameOverlap", l_screen_get_frame_overlap },
    { "getProfile", l_screen_get_profile },
    { "getMode", l_screen_get_mode },
    { "setMode", l_screen_set_mode },
    { "clearColor", l_screen_clear_color },
    { "display", l_screen_display },
    { "getParam", l_screen_get_param },
    { "setParam", l_screen_set_param },
    { "alphaEquation", l_screen_alpha_equation },
    { "initBuffers", l_screen_init_buffers },
    { "resetBuffers", l_screen_reset_buffers },
    { "getBuffer", l_screen_get_buffer },
    { "setBuffer", l_screen_set_buffer },
    { "createShuffleLUT", l_screen_create_shuffle_lut },
    { "updateShuffleLUT", l_screen_update_shuffle_lut },
    { "buildDepthFogLUT", l_screen_build_depth_fog_lut },
    { "channelShuffle", l_screen_channel_shuffle },
    { "depthFog", l_screen_depth_fog },
    { "depthBitShift", l_screen_depth_bitshift },
    { "depthInvertBlue", l_screen_depth_invert_blue },
    { "switchContext", l_screen_switch_context },
    { "flush", l_screen_flush },
    { NULL, NULL }
};

static const athena_lua_const screen_consts[] = {
    { "VRAM_SIZE", VRAM_SIZE }, { "VRAM_USED_TOTAL", VRAM_USED_TOTAL },
    { "VRAM_USED_STATIC", VRAM_USED_STATIC }, { "VRAM_USED_DYNAMIC", VRAM_USED_DYNAMIC },
    { "ALPHA_TEST_ENABLE", ALPHA_TEST_ENABLE }, { "ALPHA_TEST_METHOD", ALPHA_TEST_METHOD },
    { "ALPHA_TEST_REF", ALPHA_TEST_REF }, { "ALPHA_TEST_FAIL", ALPHA_TEST_FAIL },
    { "ALPHA_NEVER", ALPHA_NEVER }, { "ALPHA_ALWAYS", ALPHA_ALWAYS }, { "ALPHA_LESS", ALPHA_LESS },
    { "ALPHA_LEQUAL", ALPHA_LEQUAL }, { "ALPHA_EQUAL", ALPHA_EQUAL }, { "ALPHA_GEQUAL", ALPHA_GEQUAL },
    { "ALPHA_GREATER", ALPHA_GREATER }, { "ALPHA_NEQUAL", ALPHA_NEQUAL },
    { "ALPHA_FAIL_NO_UPDATE", ALPHA_FAIL_NO_UPDATE }, { "ALPHA_FAIL_FB_ONLY", ALPHA_FAIL_FB_ONLY },
    { "ALPHA_FAIL_ZB_ONLY", ALPHA_FAIL_ZB_ONLY }, { "ALPHA_FAIL_RGB_ONLY", ALPHA_FAIL_RGB_ONLY },
    { "DST_ALPHA_TEST_ENABLE", DST_ALPHA_TEST_ENABLE }, { "DST_ALPHA_TEST_METHOD", DST_ALPHA_TEST_METHOD },
    { "DST_ALPHA_ZERO", DEST_ALPHA_ZERO }, { "DST_ALPHA_ONE", DEST_ALPHA_ONE },
    { "DEPTH_TEST_ENABLE", DEPTH_TEST_ENABLE }, { "DEPTH_TEST_METHOD", DEPTH_TEST_METHOD },
    { "DEPTH_NEVER", DEPTH_NEVER }, { "DEPTH_ALWAYS", DEPTH_ALWAYS },
    { "DEPTH_GEQUAL", DEPTH_GEQUAL }, { "DEPTH_GREATER", DEPTH_GREATER },
    { "ALPHA_BLEND_EQUATION", ALPHA_BLEND_EQUATION },
    { "SRC_RGB", SRC_RGB }, { "DST_RGB", DST_RGB }, { "ZERO_RGB", ZERO_RGB },
    { "SRC_ALPHA", SRC_ALPHA }, { "DST_ALPHA", DST_ALPHA }, { "ALPHA_FIX", ALPHA_FIX },
    { "SCISSOR_BOUNDS", SCISSOR_BOUNDS }, { "PIXEL_ALPHA_BLEND_ENABLE", PIXEL_ALPHA_BLEND_ENABLE },
    { "COLOR_CLAMP_MODE", COLOR_CLAMP_MODE },
    { "NTSC", GS_MODE_NTSC }, { "DTV_480p", GS_MODE_DTV_480P }, { "PAL", GS_MODE_PAL },
    { "DTV_576p", GS_MODE_DTV_576P }, { "DTV_720p", GS_MODE_DTV_720P }, { "DTV_1080i", GS_MODE_DTV_1080I },
    { "INTERLACED", GS_INTERLACED }, { "PROGRESSIVE", GS_NONINTERLACED },
    { "FIELD", GS_FIELD }, { "FRAME", GS_FRAME },
    { "CT32", GS_PSM_CT32 }, { "CT24", GS_PSM_CT24 }, { "CT16", GS_PSM_CT16 }, { "CT16S", GS_PSM_CT16S },
    { "Z32", GS_ZBUF_32 }, { "Z24", GS_ZBUF_24 }, { "Z16", GS_ZBUF_16 }, { "Z16S", GS_ZBUF_16S },
    { "DRAW_BUFFER", DRAW_BUFFER }, { "DISPLAY_BUFFER", DISPLAY_BUFFER }, { "DEPTH_BUFFER", DEPTH_BUFFER },
    { "CHANNEL_RED", CHANNEL_SHUFFLE_RED }, { "CHANNEL_GREEN", CHANNEL_SHUFFLE_GREEN },
    { "CHANNEL_BLUE", CHANNEL_SHUFFLE_BLUE }, { "CHANNEL_ALPHA", CHANNEL_SHUFFLE_ALPHA },
    { "CHANNEL_RGB", CHANNEL_SHUFFLE_RGB }, { "CHANNEL_ALL", (int32_t)CHANNEL_SHUFFLE_ALL },
    { "FOG_LINEAR", FOG_CURVE_LINEAR }, { "FOG_EXP", FOG_CURVE_EXP }, { "FOG_EXP2", FOG_CURVE_EXP2 },
    { NULL, 0 }
};

void athena_lua_open_graphics(lua_State *L)
{
    athena_lua_register_class(L, &image_class);
    athena_lua_new_module(L, image_funcs, image_consts, l_image_new);
    athena_lua_set_global_module(L, "Image");

    athena_lua_register_class(L, &imagelist_class);
    athena_lua_new_module(L, NULL, NULL, l_imagelist_new);
    athena_lua_set_global_module(L, "ImageList");

    athena_lua_register_class(L, &fontrender_class);
    athena_lua_register_class(L, &font_class);
    athena_lua_new_module(L, NULL, font_consts, l_font_new);
    athena_lua_set_global_module(L, "Font");

    athena_lua_new_module(L, screen_funcs, screen_consts, NULL);
    push_alpha_table(L, GS_ALPHA_BLEND_NORMAL);
    lua_setfield(L, -2, "BLEND_DEFAULT");
    push_alpha_table(L, GS_ALPHA_BLEND_ADD_NOALPHA);
    lua_setfield(L, -2, "BLEND_ADD_NOALPHA");
    push_alpha_table(L, GS_ALPHA_BLEND_ADD);
    lua_setfield(L, -2, "BLEND_ADD");
    athena_lua_set_global_module(L, "Screen");

    /* JS exposes these as globals; keep them for parity. */
    lua_pushinteger(L, GS_FILTER_NEAREST);
    lua_setglobal(L, "NEAREST");
    lua_pushinteger(L, GS_FILTER_LINEAR);
    lua_setglobal(L, "LINEAR");
}
