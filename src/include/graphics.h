#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <kernel.h>

#include <gsKit.h>
#include <dmaKit.h>

#include <gsInline.h>

#include <math3d.h>

#include <packet2.h>
#include <packet2_utils.h>

#include <texture_manager.h>

// The GS's alpha blending formula is fixed but it contains four variables that can be reconfigured:
// Output = (((A - B) * C) >> 7) + D
// A, B, and D are colors and C is an alpha value. Their specific values come from the ALPHA register:
//       A                B                C                   D
//   0   Source RGB       Source RGB       Source alpha        Source RGB
//   1   Framebuffer RGB  Framebuffer RGB  Framebuffer alpha   Framebuffer RGB
//   2   0                0                FIX                 0
//   3   Reserved         Reserved         Reserved            Reserved

#define SRC_RGB 0
#define DST_RGB 1
#define ZERO_RGB 2

#define SRC_ALPHA 0
#define DST_ALPHA 1
#define ALPHA_FIX 2

#define ALPHA_EQUATION(A,B,C,D,FIX) ( (((uint64_t)(A))&3) | ((((uint64_t)(B))&3)<<2) | ((((uint64_t)(C))&3)<<4) | ((((uint64_t)(D))&3)<<6) | ((((uint64_t)(FIX)))<<32UL) )//(A - B)*C >> 7 + D

#define GS_ALPHA_BLEND_NORMAL         (ALPHA_EQUATION(SRC_RGB, DST_RGB, SRC_ALPHA, DST_ALPHA, 0x00))
#define GS_ALPHA_BLEND_ADD_NOALPHA    (ALPHA_EQUATION(SRC_RGB, ZERO_RGB, ALPHA_FIX, DST_ALPHA, 0x80))
#define GS_ALPHA_BLEND_ADD            (ALPHA_EQUATION(SRC_RGB, ZERO_RGB, SRC_ALPHA, DST_ALPHA, 0x00))

extern GSGLOBAL *gsGlobal;

//remove fontm specific things here
typedef enum {
    FONT_TYPE_FNT,
    FONT_TYPE_BMP_DAT,
    FONT_TYPE_PNG_DAT
} eTextureFontTypes;

/// gsKit Font Structure
/// This structure holds all relevant data for any
/// given font object, regardless of original format or type.
struct gsFont
{
	char *Path;		///< Path (string) to the Font File
	char *Path_DAT;		///< Path (string) to the Glyph DAT File
	u8 Type;		///< Font Type
	u8 *RawData;		///< Raw File Data
	int RawSize;		///< Raw File Datasize
	GSTEXTURE *Texture;	///< Font Texture Object
	u32 CharWidth;		///< Character Width
	u32 CharHeight;		///< Character Height
	u32 HChars;		///< Character Rows
	u32 VChars;		///< Character Columns
	s16 *Additional;		///< Additional Font Data
    int pgcount;    /// Number of pages used in one call to gsKit_font_print_scaled
};
typedef struct gsFont GSFONT;

/// GSKit CLUT base struct. This should've been in gsKit from the start :)
typedef struct
{
    u8 PSM;       ///< Pixel Storage Method (Color Format)
    u8 ClutPSM;   ///< CLUT Pixel Storage Method (Color Format)
    u32 *Clut;    ///< EE CLUT Memory Pointer
    u32 VramClut; ///< GS VRAM CLUT Memory Pointer
} GSCLUT;

typedef struct
{
    float x, y;
    float u, v;
} rm_tx_coord_t;

typedef struct
{
    rm_tx_coord_t ul;
    rm_tx_coord_t br;
    u64 color;
    GSTEXTURE *txt;
} rm_quad_t;

typedef enum {
    COLOR_MODULATE,
    COLOR_DECAL,
    COLOR_HIGHLIGHT,
    COLOR_HIGHLIGHT2
} eColorFunctions;

typedef u64 Color;
#define A(color) ((u8)(color >> 24 & 0xFF))
#define B(color) ((u8)(color >> 16 & 0xFF))
#define G(color) ((u8)(color >> 8 & 0xFF))
#define R(color) ((u8)(color & 0xFF))

void init_graphics();

void clearScreen(Color color);

extern void (*flipScreen)();

void graphicWaitVblankStart();

void setVSync(bool vsync_flag);

void toggleFrameCounter(bool enable);

GSGLOBAL *getGSGLOBAL();

int GetInterlacedFrameMode();

int getFreeVRAM();

float FPSCounter(int interval);

void setVideoMode(s16 mode, int width, int height, int psm, s16 interlace, s16 field, bool zbuffering, int psmz, bool double_buffering, uint8_t pass_count);

int load_image(GSTEXTURE* image, const char* path, bool delayed);

void draw_image(GSTEXTURE* source, float x, float y, float width, float height, float startx, float starty, float endx, float endy, Color color);
void draw_image_rotate(GSTEXTURE* source, float x, float y, float width, float height, float startx, float starty, float endx, float endy, float angle, Color color);

typedef struct {
	float x;
	float y;

	Color rgba;
} prim_point;

typedef struct {
	float x;
	float y;

	float x2;
	float y2;

	Color rgba;
} prim_line;

typedef struct {
	float x;
	float y;
    Color rgba;

	float x2;
	float y2;
    Color rgba2;
} prim_gouraud_line;

typedef struct {
	float x;
	float y;

	float x2;
	float y2;

	float x3;
	float y3;

	Color rgba;
} prim_triangle;

typedef struct {
	float x;
	float y;
    Color rgba;

	float x2;
	float y2;
    Color rgba2;

	float x3;
	float y3;
	Color rgba3;
} prim_gouraud_triangle;

typedef struct {
	float x;
	float y;
	float u;
	float v;

	float x2;
	float y2;
	float u2;
	float v2;

	float x3;
	float y3;
	float u3;
	float v3;

	Color rgba;
} prim_tex_triangle;

typedef struct {
	float x;
	float y;
	float u;
	float v;
    Color rgba;

	float x2;
	float y2;
	float u2;
	float v2;
    Color rgba2;

	float x3;
	float y3;
	float u3;
	float v3;
	Color rgba3;
} prim_tex_gouraud_triangle;

typedef struct {
	float x;
	float y;
	float u1;
	float v1;

	float w;
	float h;
	float u2;
	float v2;

	Color rgba;
} prim_tex_sprite;

void draw_point_list(float x, float y, prim_point *list, int list_size);

void draw_line_list(float x, float y, prim_line *list, int list_size);

void draw_line_gouraud_list(float x, float y, prim_gouraud_line *list, int list_size);

void draw_triangle_list(float x, float y, prim_triangle *list, int list_size);

void draw_triangle_gouraud_list(float x, float y, prim_gouraud_triangle *list, int list_size);

void draw_tex_triangle_list(GSTEXTURE* source, float x, float y, prim_tex_triangle *list, int list_size);

void draw_tex_triangle_gouraud_list(GSTEXTURE* source, float x, float y, prim_tex_gouraud_triangle *list, int list_size);

void draw_image_list(GSTEXTURE* source, float x, float y, prim_tex_sprite *list, int list_size);

void draw_point(float x, float y, Color color);
void draw_line(float x, float y, float x2, float y2, Color color);
void draw_sprite(float x, float y, int width, int height, Color color);
void draw_circle(float x, float y, float radius, u64 color, u8 filled);
void draw_triangle(float x, float y, float x2, float y2, float x3, float y3, Color color);
void draw_triangle_gouraud(float x, float y, float x2, float y2, float x3, float y3, Color color, Color color2, Color color3);
void draw_quad(float x, float y, float x2, float y2, float x3, float y3, float x4, float y4, Color color);
void draw_quad_gouraud(float x, float y, float x2, float y2, float x3, float y3, float x4, float y4, Color color, Color color2, Color color3, Color color4);

void set_alpha_blend_mode(uint64_t alpha_equation);

GSFONT* loadFont(const char* path);
void printFontText(GSFONT* font, const char* text, float x, float y, float scale, Color color);
void unloadFont(GSFONT* font);

void athena_error_screen(const char* errMsg, bool dark_mode);

#endif
