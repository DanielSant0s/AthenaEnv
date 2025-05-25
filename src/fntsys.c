/*
 Copyright 2010, Volca
 Licenced under Academic Free License version 3.0
 Review OpenUsbLd README & LICENSE files for further details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <fntsys.h>
#include <utf8.h>
#include <atlas.h>
#include <graphics.h>
#include <owl_packet.h>
#include <dbgprintf.h>

#include <sys/types.h>
#include <ft2build.h>

#include <strUtils.h>

#include <macros.h>

#include FT_FREETYPE_H

extern unsigned char quicksand_regular[] __attribute__((aligned(16)));
extern int size_quicksand_regular;

// freetype vars
static FT_Library font_library;

static s32 gFontSemaId;
static ee_sema_t gFontSema;

static GSCLUT fontClut;

static const float fDPI = 72.0f;

/** Single entry in the glyph cache */
typedef struct
{
    int isValid;
    // size in pixels of the glyph
    int width, height;
    // offsetting of the glyph
    int ox, oy;
    // advancements in pixels after rendering this glyph
    int shx, shy;

    // atlas for which the allocation was done
    atlas_t *atlas;

    // atlas allocation position
    struct atlas_allocation_t *allocation;

} fnt_glyph_cache_entry_t;

/** A whole font definition */
typedef struct
{
    /** GLYPH CACHE. Every glyph in the ASCII range is cached when first used
     * this means no additional memory aside from the one needed to render the
     * character set is used.
     */
    fnt_glyph_cache_entry_t **glyphCache;

    /// Maximal font cache page index
    int cacheMaxPageID;

    /// Font face
    FT_Face face;

    /// Nonzero if font is used
    int isValid;

    /// Texture atlases (default to NULL)
    atlas_t *atlases[ATLAS_MAX];

    /// Pointer to data, if allocation takeover was selected (will be freed)
    void *dataPtr;
} font_t;

#define FNT_MAX_COUNT (16)

/// Array of font definitions
static font_t fonts[FNT_MAX_COUNT];

static uint32_t codepoint, state;
static fnt_glyph_cache_entry_t *glyph;
static FT_Bool use_kerning;
static FT_UInt glyph_index, previous;
static FT_Vector delta;

#define GLYPH_CACHE_PAGE_SIZE 256

#define GLYPH_PAGE_OK(font, page) ((pageid <= font->cacheMaxPageID) && (font->glyphCache[page]))

static fnt_glyph_cache_entry_t *fntCacheGlyph(font_t *font, uint32_t gid);

void *readFile(const char* path, int align, int *size)
{
    void *buffer = NULL;

    int fd = open(path, O_RDONLY, 0666);
    if (fd >= 0) {
        int realSize = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);

        if ((*size > 0) && (*size != realSize)) {
            dbgprintf("UTIL Invalid filesize, expected: %d, got: %d\n", *size, realSize);
            close(fd);
            return NULL;
        }

        if (align > 0)
            buffer = memalign(64, realSize); // The allocation is aligned to aid the DMA transfers
        else
            buffer = malloc(realSize);

        if (!buffer) {
            dbgprintf("UTIL ReadFile: Failed allocation of %d bytes", realSize);
            *size = 0;
        } else {
            read(fd, buffer, realSize);
            close(fd);
            *size = realSize;
        }
    }
    return buffer;
}


static void fntCacheFlushPage(fnt_glyph_cache_entry_t *page)
{
    int i;

    for (i = 0; i < GLYPH_CACHE_PAGE_SIZE; ++i, ++page) {
        page->isValid = 0;
        // we're not doing any atlasFree or such - atlas has to be rebuild
        page->allocation = NULL;
        page->atlas = NULL;
    }
}

static void fntCacheFlush(font_t *font)
{
    // Release all the glyphs from the cache
    int i;
    for (i = 0; i <= font->cacheMaxPageID; ++i) {
        if (font->glyphCache[i]) {
            fntCacheFlushPage(font->glyphCache[i]);
            free(font->glyphCache[i]);
            font->glyphCache[i] = NULL;
        }
    }

    free(font->glyphCache);
    font->glyphCache = NULL;
    font->cacheMaxPageID = -1;

    // free all atlasses too, they're invalid now anyway
    int aid;
    for (aid = 0; aid < ATLAS_MAX; ++aid) {
        atlasFree(font->atlases[aid]);
        font->atlases[aid] = NULL;
    }

    for (char c = 32; c <= 126; ++c) {
        utf8Decode(&state, &codepoint, c);
        fntCacheGlyph(font, codepoint); // Preload all visible glyphs
    }
}

static int fntPrepareGlyphCachePage(font_t *font, int pageid)
{
    if (pageid > font->cacheMaxPageID) {
        fnt_glyph_cache_entry_t **np = (fnt_glyph_cache_entry_t**)realloc(font->glyphCache, (pageid + 1) * sizeof(fnt_glyph_cache_entry_t *));

        if (!np)
            return 0;

        font->glyphCache = np;

        int page;
        for (page = font->cacheMaxPageID + 1; page <= pageid; ++page)
            font->glyphCache[page] = NULL;

        font->cacheMaxPageID = pageid;
    }

    // if it already was allocated, skip this
    if (font->glyphCache[pageid])
        return 1;

    // allocate the page
    font->glyphCache[pageid] = (fnt_glyph_cache_entry_t*)malloc(sizeof(fnt_glyph_cache_entry_t) * GLYPH_CACHE_PAGE_SIZE);

    int i;
    for (i = 0; i < GLYPH_CACHE_PAGE_SIZE; ++i) {
        font->glyphCache[pageid][i].isValid = 0;
        font->glyphCache[pageid][i].atlas = NULL;
        font->glyphCache[pageid][i].allocation = NULL;
    }

    return 1;
}

static void fntPrepareCLUT()
{
    fontClut.PSM = GS_PSM_T8;
    fontClut.ClutPSM = GS_PSM_CT32;
    fontClut.Clut = (u32*)memalign(128, 256 * 4);
    fontClut.VramClut = 0;

    // generate the clut table
    size_t i;
    u32 *clut = fontClut.Clut;
    for (i = 0; i < 256; ++i) {
        u8 alpha = (i * 128) / 255;

        *clut = GS_SETREG_RGBA(0xFF, 0xFF, 0xFF, alpha);
        clut++;
    }
}

static void fntDestroyCLUT()
{
    free(fontClut.Clut);
    fontClut.Clut = NULL;
}

static void fntInitSlot(font_t *font)
{
    font->face = NULL;
    font->glyphCache = NULL;
    font->cacheMaxPageID = -1;
    font->dataPtr = NULL;
    font->isValid = 0;

    int aid = 0;
    for (; aid < ATLAS_MAX; ++aid)
        font->atlases[aid] = NULL;
}

static void fntDeleteSlot(font_t *font)
{
    // free the glyph cache, atlases, unload the font
    fntCacheFlush(font);

    FT_Done_Face(font->face);
    font->face = NULL;

    if (font->dataPtr) {
        free(font->dataPtr);
        font->dataPtr = NULL;
    }

    font->isValid = 0;
}

void fntRelease(int id)
{
    if (id > FNT_DEFAULT && id < FNT_MAX_COUNT)
        fntDeleteSlot(&fonts[id]);
}

static int fntLoadSlot(font_t *font, const char* path)
{
    void *buffer = NULL;
    int bufferSize = -1;

    fntInitSlot(font);

    if (path) {
        buffer = readFile(path, -1, &bufferSize);
        if (!buffer) {
            dbgprintf("FNTSYS Font file loading failed: %s\n", path);
            return FNT_ERROR;
        }
        font->dataPtr = buffer;
    } else {
        buffer = &quicksand_regular;
        bufferSize = size_quicksand_regular;
    }

    // load the font via memory handle
    int error = FT_New_Memory_Face(font_library, (FT_Byte *)buffer, bufferSize, 0, &font->face);
    if (error) {
        dbgprintf("FNTSYS Freetype font loading failed with %x!\n", error);
        fntDeleteSlot(font);
        return FNT_ERROR;
    }

    font->isValid = 1;
    fntUpdateAspectRatio();

    return 0;
}

void fntInit()
{
    dbgprintf("FNTSYS Init\n");
    int error = FT_Init_FreeType(&font_library);
    if (error) {
        // just report over the ps2link
        dbgprintf("FNTSYS Freetype init failed with %x!\n", error);
        return;
    }

    fntPrepareCLUT();

    gFontSema.init_count = 1;
    gFontSema.max_count = 1;
    gFontSema.option = 0;
    gFontSemaId = CreateSema(&gFontSema);

    int i = 0;
    for (; i < FNT_MAX_COUNT; ++i)
        fntInitSlot(&fonts[i]);

    //fntLoadDefault(NULL);

    fntUpdateAspectRatio();
}

int fntLoadFile(const char* path)
{
    font_t *font;
    int i = 1;
    for (; i < FNT_MAX_COUNT; i++) {
        font = &fonts[i];
        if (!font->isValid) {
            if (fntLoadSlot(font, path) != FNT_ERROR)
                return i;
            break;
        }
    }

    return FNT_ERROR;
}

int fntLoadDefault(const char* path)
{
    font_t newFont, oldFont;

    if (fntLoadSlot(&newFont, path) != FNT_ERROR) {
        // copy over the new font definition
        // we have to lock this phase, as the old font may still be used
        // Note: No check for concurrency is done here, which is kinda funky!
        WaitSema(gFontSemaId);
        memcpy(&oldFont, &fonts[FNT_DEFAULT], sizeof(font_t));
        memcpy(&fonts[FNT_DEFAULT], &newFont, sizeof(font_t));
        SignalSema(gFontSemaId);

        // delete the old font
        fntDeleteSlot(&oldFont);

        return 0;
    }

    return -1;
}

void fntEnd()
{
    dbgprintf("FNTSYS End\n");
    // release all the fonts
    int id;
    for (id = 0; id < FNT_MAX_COUNT; ++id)
        fntDeleteSlot(&fonts[id]);

    // deinit freetype system
    FT_Done_FreeType(font_library);

    DeleteSema(gFontSemaId);

    fntDestroyCLUT();
}

static atlas_t *fntNewAtlas()
{
    atlas_t *atl = atlasNew(ATLAS_WIDTH, ATLAS_HEIGHT, GS_PSM_T8);

    atl->surface.ClutPSM = GS_PSM_CT32;
    atl->surface.Clut = fontClut.Clut;

    return atl;
}

static int fntGlyphAtlasPlace(font_t *font, fnt_glyph_cache_entry_t *glyph)
{
    FT_GlyphSlot slot = font->face->glyph;

    // dbgprintf("FNTSYS GlyphAtlasPlace: Placing the glyph... %d x %d\n", slot->bitmap.width, slot->bitmap.rows);

    if (slot->bitmap.width == 0 || slot->bitmap.rows == 0) {
        // no bitmap glyph, just skip
        return 1;
    }

    int aid = 0;
    for (; aid < ATLAS_MAX; aid++) {
        // dbgprintf("FNTSYS Placing aid %d...\n", aid);
        atlas_t **atl = &font->atlases[aid];
        if (!*atl) { // atlas slot not yet used
            // dbgprintf("FNTSYS aid %d is new...\n", aid);
            *atl = fntNewAtlas();
        }

        glyph->allocation = atlasPlace(*atl, slot->bitmap.width, slot->bitmap.rows, slot->bitmap.buffer);
        if (glyph->allocation) {
            // dbgprintf("FNTSYS Found placement\n", aid);
            glyph->atlas = *atl;

            return 1;
        }
    }

    dbgprintf("FNTSYS No atlas free\n");
    return 0;
}

/** Internal method. Makes sure the bitmap data for particular character are pre-rendered to the glyph cache */
static fnt_glyph_cache_entry_t *fntCacheGlyph(font_t *font, uint32_t gid)
{
    // calc page id and in-page index from glyph id
    int pageid = gid / GLYPH_CACHE_PAGE_SIZE;
    int idx = gid % GLYPH_CACHE_PAGE_SIZE;

    // do not call on every char of every font rendering call
    if (!GLYPH_PAGE_OK(font, pageid))
        if (!fntPrepareGlyphCachePage(font, pageid)) // failed to prepare the page...
            return NULL;

    fnt_glyph_cache_entry_t *page = font->glyphCache[pageid];
    /* Should never happen.
    if (!page) // safeguard
        return NULL;
    */

    fnt_glyph_cache_entry_t *glyph = &page[idx];
    if (glyph->isValid)
        return glyph;

    // not cached but valid. Cache
    if (!font->face) {
        dbgprintf("FNTSYS Face is NULL!\n");
    }

    int error = FT_Load_Char(font->face, gid, FT_LOAD_RENDER);
    if (error) {
        dbgprintf("FNTSYS Error loading glyph - %d\n", error);
        return NULL;
    }

    // find atlas placement for the glyph
    if (!fntGlyphAtlasPlace(font, glyph))
        return NULL;

    FT_GlyphSlot slot = font->face->glyph;
    glyph->width = slot->bitmap.width;
    glyph->height = slot->bitmap.rows;
    glyph->shx = slot->advance.x;
    glyph->shy = slot->advance.y;
    glyph->ox = slot->bitmap_left;
    glyph->oy = -slot->bitmap_top;

    glyph->isValid = 1;

    return glyph;
}

void fntUpdateAspectRatio()
{
    int i;
    int h, hn;
    float hs;


    h = 480;
    hn = 448;
    // Scale height from virtual resolution (640x480) to the native display resolution
    hs = (float)hn / (float)h;
    // Scale width according to the PAR (Pixel Aspect Ratio)
    //ws = hs * rmGetPAR();

    // Supersample height*2 when using interlaced frame mode
    if (GetInterlacedFrameMode() == 1)
        hs *= 2;

    // flush cache - it will be invalid after the setting
    for (i = 0; i < FNT_MAX_COUNT; i++) {
        if (fonts[i].isValid) {
            fntCacheFlush(&fonts[i]);
            // TODO: this seems correct, but the rest of the OPL UI (i.e. spacers) doesn't seem to be correctly scaled.
            FT_Set_Char_Size(fonts[i].face, FNTSYS_CHAR_SIZE * 64, FNTSYS_CHAR_SIZE * 64, fDPI, fDPI * hs);
        }
    }
}

void fntSetPixelSize(int fontid, int width, int height)
{
    fntCacheFlush(&fonts[fontid]);
    // TODO: this seems correct, but the rest of the OPL UI (i.e. spacers) doesn't seem to be correctly scaled.
    FT_Set_Pixel_Sizes(fonts[fontid].face, width, height);
}

void fntSetCharSize(int fontid, int width, int height)
{
    fntCacheFlush(&fonts[fontid]);
    // TODO: this seems correct, but the rest of the OPL UI (i.e. spacers) doesn't seem to be correctly scaled.
    FT_Set_Char_Size(fonts[fontid].face, width, height, fDPI, fDPI);
}

const float XYUV_MAX_FLOAT[4] qw_aligned = { 4095.75f, 4095.75f, 1024.0f, 1024.0f };

void fntRenderGlyph(fnt_glyph_cache_entry_t *glyph, owl_packet *packet, int pen_x, int pen_y, float scale)
{
    float x1, y1, x2, y2;
    float u1, v1, u2, v2;

    x1 = (float)pen_x + ((float)glyph->ox * scale)-0.5f;
    
    if (GetInterlacedFrameMode()) {
        y1 = ((float)pen_y + ((float)glyph->oy / 2.0f) * scale)-0.5f;
        y2 = (y1 + ((float)glyph->height / 2.0f) * scale)-0.5f;
    } else {
        y1 = (float)pen_y + ((float)glyph->oy * scale)-0.5f;
        y2 = y1 + ((float)glyph->height * scale)-0.5f;
    }
    
    x2 = x1 + ((float)glyph->width * scale)-0.5f;

    u1 = glyph->allocation->x; 
    v1 = glyph->allocation->y;
    u2 = glyph->allocation->x + glyph->width + 0.5f;
    v2 = glyph->allocation->y + glyph->height + 0.5f;

    float float_pos[8] = { x1, y1, u1, v1, x2, y2, u2, v2 };
    int fixed_pos[8];
    vu0_ftoi4_clamp_8x(float_pos, fixed_pos, XYUV_MAX_FLOAT);

    owl_add_xy_uv_2x_font(packet, fixed_pos[0], fixed_pos[1], fixed_pos[2], fixed_pos[3], fixed_pos[4], fixed_pos[5], fixed_pos[6], fixed_pos[7]);
}

#ifndef __RTL  
int fntRenderString(int id, int x, int y, short aligned, size_t width, size_t height, const char *string, float scale, u64 colour)
{
    // wait for font lock to unlock
    WaitSema(gFontSemaId);
    font_t *font = &fonts[id];
    SignalSema(gFontSemaId);

    int text_width = fntCalcDimensions(id, scale, string);
    int text_height = FNTSYS_CHAR_SIZE*scale; 

    if (aligned & ALIGN_HCENTER)
        x -= text_width >> 1;
    else if (aligned & ALIGN_RIGHT)
        x -= text_width;

    if (aligned & ALIGN_VCENTER)
        y += (height - text_height) >> 1;
    else if (aligned & ALIGN_BOTTOM)
        y += height - text_height;
    else
        y += (text_height - 2);

    int pen_x = x;
    int xmax = x + width; 
    int ymax = y + height;

    use_kerning = FT_HAS_KERNING(font->face);
    state = UTF8_ACCEPT;
    previous = 0;

    // Note: We need to change this so that we'll accumulate whole word before doing a layout with it
    // for now this method breaks on any character - which is a bit ugly

    // I don't want to do anything complicated though so I'd say
    // we should instead have a dynamic layout routine that'll replace spaces with newlines as appropriate
    // because that'll make the code run only once per N frames, not every frame

    // cache glyphs and render as we go

    owl_packet *packet = NULL;

    GSTEXTURE *tex = NULL;

    const char *text_to_render = string;

    owl_qword *last_cnt, *last_direct, *last_prim, *before_first_draw, *after_draw;

    int text_size = 0, texture_id, last_texture_id;

    char *chars_to_count = width? " \n" : " ";

    for (; *text_to_render; ++text_to_render) {
        if (utf8Decode(&state, &codepoint, *text_to_render)) // accumulate the codepoint value 
            continue;
            

        glyph = fntCacheGlyph(font, codepoint);
        if (!glyph)
            continue;

        // kerning
        if (use_kerning && previous) {
            glyph_index = FT_Get_Char_Index(font->face, codepoint);
            if (glyph_index) {
                FT_Get_Kerning(font->face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
                pen_x += ((int)(delta.x*scale) >> 6);
            }
            previous = glyph_index;
        }

        if (width) {
            if (codepoint == '\n') {
                pen_x = x;
                y += 19; // hmax is too tight and unordered, generally
                continue;
            }

            //if (y > ymax) // stepped over the max
            //    break;

            if (pen_x + glyph->width > xmax) {
                //pen_x = xmax + 1; // to be sure no other cahr will be written (even not a smaller one just following)
                pen_x = x;
                y += 19; // hmax is too tight and unordered, generally
                //continue;
            }
        }

        if (glyph->allocation) {
            if (tex != &glyph->atlas->surface || !glyph->atlas->surface.Vram) {
                tex = &glyph->atlas->surface;
                
                if (text_to_render != string) {
                    int last_size = (((uint32_t)after_draw)-((uint32_t)before_first_draw))/16;

                    last_cnt->dword[0] = DMA_TAG((texture_id != -1? 12 : 8)+last_size, 0, DMA_CNT, 0, 0, 0);
            
                    last_direct->sword[3] = VIF_CODE(7+last_size, 0, VIF_DIRECT, 0);

                    last_prim->dword[0] = VU_GS_GIFTAG(last_size/2, 
				                            			1, NO_CUSTOM_DATA, 1, 
				                            			VU_GS_PRIM(GS_PRIM_PRIM_SPRITE, 
				                            					   0, 1, 
				                            					   gsGlobal->PrimFogEnable, 
				                            					   gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable, 1, gsGlobal->PrimContext, 0),
    			                            			1, 4);

                }

                text_size = strlen(text_to_render)-count_spaces(text_to_render, chars_to_count)-count_nonascii(text_to_render);
                int text_vert_size = (text_size*2);

                texture_id = texture_manager_bind(gsGlobal, tex, true);

	            packet = owl_query_packet(CHANNEL_VIF1, (texture_id != -1? 13 : 9)+text_vert_size);

                last_cnt = packet->ptr;
	            owl_add_cnt_tag(packet, (texture_id != -1? 12 : 8)+text_vert_size, 0); // 4 quadwords for vif

	            if (texture_id != -1) {
	            	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
	            	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
	            	owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSH, 0));
	            	owl_add_uint(packet, VIF_CODE(2, 0, VIF_DIRECT, 0));

	            	owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));
	            	owl_add_tag(packet, GIF_NOP, 0);

	            	owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
	            	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0)); 
	            	owl_add_uint(packet, VIF_CODE(texture_id, 0, VIF_MARK, 0));
	            	owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 1));
	            }

                last_direct = packet->ptr;
	            owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	            owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
	            owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
	            owl_add_uint(packet, VIF_CODE(7+(text_size*2), 0, VIF_DIRECT, 0)); // 3 giftags

	            owl_add_tag(packet, GIF_AD, GIFTAG(5, 1, 0, 0, 0, 1));

	            owl_add_tag(packet, GS_TEST_1, GS_SETREG_TEST_1(0, 0, 0, 0, 0, 0, 1, 1));

	            int tw, th;
	            athena_set_tw_th(tex, &tw, &th);

	            owl_add_tag(packet, 
	            	GS_TEX0_1, 
	            	GS_SETREG_TEX0((tex->Vram & ~TRANSFER_REQUEST_MASK)/256, 
	            				  tex->TBW, 
	            				  tex->PSM,
	            				  tw, th, 
	            				  gsGlobal->PrimAlphaEnable, 
	            				  COLOR_MODULATE,
	            				  (tex->VramClut & ~TRANSFER_REQUEST_MASK)/256, 
	            				  tex->ClutPSM, 
	            				  0, 0, 
	            				  tex->VramClut? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
	            );

	            owl_add_tag(packet, GS_TEX1_1, GS_SETREG_TEX1(1, 0, tex->Filter, tex->Filter, 0, 0, 0));

                owl_add_tag(packet, GS_PRIM, 
                    VU_GS_PRIM(
                        GS_PRIM_PRIM_SPRITE, 
                        0, 
                        1, 
                        gsGlobal->PrimFogEnable, 
                        gsGlobal->PrimAlphaEnable, 
                        gsGlobal->PrimAAEnable, 
                        1, 
                        gsGlobal->PrimContext, 
                        0
                    )
                );

                owl_add_tag(packet, GS_RGBAQ, colour);

                last_prim = packet->ptr; 
	            owl_add_tag(packet, 
					   ((uint64_t)(GS_UV) << 0 | (uint64_t)(GS_XYZ2) << 4), 
					   	VU_GS_GIFTAG(text_vert_size, 
							1, NO_CUSTOM_DATA, 0, 
							0,
    						1, 2)
						);

                before_first_draw = packet->ptr;
            }                 

            fntRenderGlyph(glyph, packet, pen_x, y, scale);
            
            after_draw = packet->ptr;
        }

        pen_x += ((int)(glyph->shx*scale) >> 6);
    }

    return pen_x;
}
 

int fntRenderStringPlus(int id, int x, int y, short aligned, size_t width, size_t height, const char *string, float scale, u64 colour, float outline, u64 outline_colour, float dropshadow, u64 dropshadow_colour) {
    if (outline > 0.0f) {
        float offsets[][2] = { {outline, outline}, {outline, -outline}, {-outline, outline}, {-outline, -outline} };

	    for(int i = 0; i < 4; i++){
            fntRenderString(id, x+offsets[i][0], y+offsets[i][1], aligned, width, height, string, scale, outline_colour);
	    }
    } else if (dropshadow > 0.0f) {
        fntRenderString(id, x+dropshadow, y+dropshadow, aligned, width, height, string, scale, dropshadow_colour);
    }

    fntRenderString(id, x, y, aligned, width, height, string, scale, colour);
}

Coords fntGetTextSize(int id, const char* text, float scale) {
    WaitSema(gFontSemaId);
    font_t *font = &fonts[id];
    SignalSema(gFontSemaId);

    int width = 0;

    for (; *text; ++text) {
        if (utf8Decode(&state, &codepoint, *text)) // accumulate the codepoint value
            continue;

        fnt_glyph_cache_entry_t *glyph = fntCacheGlyph(font, codepoint);
        if (!glyph)
            continue;

        // kerning
        if (use_kerning && previous) {
            glyph_index = FT_Get_Char_Index(font->face, codepoint);
            if (glyph_index) {
                FT_Get_Kerning(font->face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
                width += (int)(delta.x*scale) >> 6;
            }
            previous = glyph_index;
        }

        width += ((int)(glyph->shx*scale) >> 6); 
    }

    Coords size;
    size.width = width;
    size.height = FNTSYS_CHAR_SIZE*scale;
	
	return size;
}

#else
static int isRTL(u32 character)
{
    return (((character >= 0x00000590 && character <= 0x000008FF) || (character >= 0x0000FB50 && character <= 0x0000FDFF) || (character >= 0x0000FE70 && character <= 0x0000FEFF) || (character >= 0x00010800 && character <= 0x00010FFF) || (character >= 0x0001E800 && character <= 0x0001EFFF)) ? 1 : 0);
}

static int isWeak(u32 character)
{
    return (((character >= 0x0000 && character <= 0x0040) || (character >= 0x005B && character <= 0x0060) || (character >= 0x007B && character <= 0x00BF) || (character >= 0x00D7 && character <= 0x00F7) || (character >= 0x02B9 && character <= 0x02FF) || (character >= 0x2000 && character <= 0x2BFF)) ? 1 : 0);
}

static void fntRenderSubRTL(font_t *font, const char *startRTL, const char *string, fnt_glyph_cache_entry_t *glyph, int x, int y)
{
    if (glyph) {
        x -= glyph->shx >> 6;
        fntRenderGlyph(glyph, x, y);
    }

    for (; startRTL != string; ++startRTL) {
        if (utf8Decode(&state, &codepoint, *startRTL))
            continue;

        glyph = fntCacheGlyph(font, codepoint);
        if (!glyph)
            continue;

        if (use_kerning && previous) {
            glyph_index = FT_Get_Char_Index(font->face, codepoint);
            if (glyph_index) {
                FT_Get_Kerning(font->face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
                x -= delta.x >> 6;
            }
            previous = glyph_index;
        }

        x -= glyph->shx >> 6;
        fntRenderGlyph(glyph, x, y);
    }
}

int fntRenderString(int id, int x, int y, short aligned, size_t width, size_t height, const char *string, u64 colour)
{
    // wait for font lock to unlock
    WaitSema(gFontSemaId);
    font_t *font = &fonts[id];
    SignalSema(gFontSemaId);

    // Convert to native display resolution
    x = rmScaleX(x);
    y = rmScaleY(y);
    width = rmScaleX(width);
    height = rmScaleY(height);

    if (aligned & ALIGN_HCENTER) {
        if (width) {
            x -= min(fntCalcDimensions(id, string), width) >> 1;
        } else {
            x -= fntCalcDimensions(id, string) >> 1;
        }
    }

    if (aligned & ALIGN_VCENTER) {
        y += rmScaleY(FNTSYS_CHAR_SIZE - 4) >> 1;
    } else {
        y += rmScaleY(FNTSYS_CHAR_SIZE - 2);
    }

    quad.color = colour;

    int pen_x = x;
    int xmax = x + width;
    int ymax = y + height;

    use_kerning = FT_HAS_KERNING(font->face);
    state = UTF8_ACCEPT;
    previous = 0;

    short inRTL = 0;
    int delta_x, pen_xRTL = 0;
    fnt_glyph_cache_entry_t *glyphRTL = NULL;
    const char *startRTL = NULL;

    // cache glyphs and render as we go
    for (; *string; ++string) {
        if (utf8Decode(&state, &codepoint, *string)) // accumulate the codepoint value
            continue;

        glyph = fntCacheGlyph(font, codepoint);
        if (!glyph)
            continue;

        // kerning
        delta_x = 0;
        if (use_kerning && previous) {
            glyph_index = FT_Get_Char_Index(font->face, codepoint);
            if (glyph_index) {
                FT_Get_Kerning(font->face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
                delta_x = delta.x >> 6;
            }
            previous = glyph_index;
        }


        if (width) {
            if (codepoint == '\n') {
                pen_x = x;
                y += rmScaleY(MENU_ITEM_HEIGHT); // hmax is too tight and unordered, generally
                continue;
            }

            if ((pen_x + glyph->width > xmax) || (y > ymax)) // stepped over the max
                break;
        }

        if (isRTL(codepoint)) {
            if (!inRTL && !isWeak(codepoint)) {
                inRTL = 1;
                pen_xRTL = pen_x;
                glyphRTL = glyph;
                startRTL = string + 1;
            }
        } else {
            if (inRTL && !isWeak(codepoint)) { // A LTR character is encountered. Render RTL characters before continuing.
                inRTL = 0;
                pen_x = pen_xRTL;
                fntRenderSubRTL(font, startRTL, string, glyphRTL, pen_xRTL, y);
            }
        }

        if (inRTL) {
            pen_xRTL += delta_x + (glyph->shx >> 6);
        } else {
            pen_x += delta_x;
            fntRenderGlyph(glyph, pen_x, y);
            pen_x += glyph->shx >> 6;
        }
    }

    if (inRTL) {
        pen_x = pen_xRTL;
        fntRenderSubRTL(font, startRTL, string, glyphRTL, pen_xRTL, y);
    }

    return rmUnScaleX(pen_x);
}
#endif

#if 0
void fntFitString(int id, char *string, size_t width)
{
    size_t cw = 0;
    char *str = string;
    size_t spacewidth = fntCalcDimensions(id, " ");
    char *psp = NULL;

    while (*str) {
        // scan forward to the next whitespace
        char *sp = str;
        for (; *sp && *sp != ' ' && *sp != '\n'; ++sp)
            ;

        // store what was there before
        char osp = *sp;

        // newline resets the situation
        if (osp == '\n') {
            cw = 0;
            str = ++sp;
            psp = NULL;
            continue;
        }

        // terminate after the word
        *sp = '\0';

        // Calc the font's width...
        // NOTE: The word was terminated, so we're seeing a single word
        // on that position
        size_t ww = fntCalcDimensions(id, str);

        if (cw + ww > width) {
            if (psp) {
                // we have a prev space to utilise (wrap on it)
                *psp = '\n';
                *sp = osp;
                cw = ww;
                psp = sp;
            } else {
                // no prev. space to hijack, must break after the word
                // this will mean overflowed text...
                *sp = '\n';
                cw = 0;
            }
        } else {
            cw += ww;
            *sp = osp;
            psp = sp;
        }

        cw += spacewidth;
        str = ++sp;
    }
}
#endif

int fntCalcDimensions(int id, float scale, const char *str)
{
    int w = 0;

    WaitSema(gFontSemaId);
    font_t *font = &fonts[id];
    SignalSema(gFontSemaId);

    uint32_t codepoint;
    uint32_t state = UTF8_ACCEPT;
    FT_Bool use_kerning = FT_HAS_KERNING(font->face);
    FT_UInt glyph_index, previous = 0;
    FT_Vector delta;

    // cache glyphs and render as we go
    for (; *str; ++str) {
        if (utf8Decode(&state, &codepoint, *str)) // accumulate the codepoint value
            continue;

        // Could just as well only get the glyph dimensions
        // but it is probable the glyphs will be needed anyway
        fnt_glyph_cache_entry_t *glyph = fntCacheGlyph(font, codepoint);
        if (!glyph)
            continue;

        // kerning
        if (use_kerning && previous) {
            glyph_index = FT_Get_Char_Index(font->face, codepoint);
            if (glyph_index) {
                FT_Get_Kerning(font->face, previous, glyph_index, FT_KERNING_DEFAULT, &delta);
                w += (int)(delta.x*scale) >> 6;
            }
            previous = glyph_index;
        }

        w += (int)(glyph->shx*scale) >> 6;
    }

    return w;
}
