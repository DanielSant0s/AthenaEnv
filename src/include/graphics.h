#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

#include <kernel.h>

#include <gsKit.h>
#include <dmaKit.h>

#include <math3d.h>

#include <packet2.h>
#include <packet2_utils.h>

#undef GS_PSMZ_32 
#undef GS_PSMZ_24 
#undef GS_PSMZ_16 
#undef GS_PSMZ_16S

#define GS_PSMZ_32  0x30
#define GS_PSMZ_24  0x31
#define GS_PSMZ_16  0x32
#define GS_PSMZ_16S 0x3A

#define GS_ZBUF_32  0x00
#define GS_ZBUF_24  0x01
#define GS_ZBUF_16  0x02
#define GS_ZBUF_16S 0x0A

#define CHANNEL_SHUFFLE_RED 0x000000FF
#define CHANNEL_SHUFFLE_GREEN 0x0000FF00
#define CHANNEL_SHUFFLE_BLUE 0x00FF0000
#define CHANNEL_SHUFFLE_ALPHA 0xFF000000

struct gsContext
{
	int16_t Mode;            ///< Display Mode
	int16_t Interlace;       ///< Interlace (On/Off)
	int16_t Field;           ///< Field / Frame
	uint8_t Dithering;        ///< Dithering (On/Off)
	int8_t DitherMatrix[16]; ///< Dithering Matrix
	uint8_t DoubleBuffering;	 ///< Enable/Disable Double Buffering
	uint8_t ZBuffering;       ///< Enable/Disable Z Buffering
	uint32_t ScreenBuffer[2]; ///< Screenbuffer Pointer Array
	uint32_t ZBuffer;         ///< ZBuffer Pointer
	uint8_t EvenOrOdd;        ///< Is ((GSREG*)CSR)->FIELD (Used for Interlace Correction)
	uint8_t DrawOrder;        ///< Drawing Order (GS_PER_OS/GS_OS_PER) (GS_PER_OS = Persitant objects always drawn first)
	uint8_t FirstFrame;       ///< Have we executed a frame yet?
	uint8_t DrawField;        ///< Field to draw (GS_FIELD_NORMAL/GS_FIELD_ODD/GS_FIELD_EVEN)
	uint8_t ActiveBuffer;     ///< Active Framebuffer
	int Width;           ///< Framebuffer Width (the mode's DW is a multiple of this value)
	int Height;          ///< Framebuffer Height (the mode's DH is a multiple of this value)
	int Aspect;          ///< Framebuffer Aspect Ratio (GS_ASPECT_4_3/GS_ASPECT_16_9)
	int OffsetX;         ///< X Window Offset
	int OffsetY;         ///< Y Window Offset
	int StartX;          ///< X Starting Coordinate (Used for Placement Correction) Default value
	int StartY;          ///< Y Starting Coordinate (Used for Placement Correction) Default value
	int StartXOffset;    ///< X Starting Coordinate (Used for Placement Correction) Additional correction
	int StartYOffset;    ///< Y Starting Coordinate (Used for Placement Correction) Additional correction
	int MagH;            ///< X Magnification Value (MAGH = DW / Width - 1)
	int MagV;            ///< Y Magnification Value (MAGV = DH / Height - 1)
	int DW;              ///< Total Display Area Width (DW = Width * (MAGH + 1))
    int DH;              ///< Total Display Area Height (DH = Height * (MAGH + 1))
	GSBGCOLOR *BGColor;  ///< Background Color Structure Pointer
	void *dma_misc __attribute__ ((aligned (64)));	///< Misc 512 byte DMA Transfer Buffer (so we don't malloc at runtime)
	int PSM;             ///< Pixel Storage Method (Color Mode)
	int PSMZ;            ///< ZBuffer Pixel Storage Method
	int PrimContext;     ///< Primitive Context
	int PrimFogEnable;   ///< Primitive Fog Enable
	int PrimAAEnable;    ///< Primitive AntiAlaising Enable
	int PrimAlphaEnable; ///< Primitive Alpha Blending Enable
	uint64_t PrimAlpha;       ///< Primitive Alpha Value
};
typedef struct gsContext GSCONTEXT;

struct gsSurface
{
	uint32_t Width;		///< Width of the Texture
	uint32_t Height;		///< Height of the Texture
	uint8_t	PSM;		///< Pixel Storage Method (Color Format)
	uint8_t	ClutPSM;	///< CLUT Pixel Storage Method (Color Format)
	uint32_t	TBW;		///< Texture Base Width
	uint32_t *Mem;		///< EE Memory Pointer
	uint32_t *Clut;		///< EE CLUT Memory Pointer
	uint32_t Vram;		///< GS VRAM Memory Pointer
	uint32_t VramClut;	///< GS VRAM CLUT Memory Pointer
	uint32_t Filter;		///< NEAREST or LINEAR
	uint8_t ClutStorageMode;	///< CLUT Storage Mode
	uint8_t	Delayed;	///< Delay Texture Upload To VRAM
	uint8_t PageAligned;
};
typedef struct gsSurface GSSURFACE;

void athena_calculate_tbw(GSSURFACE *Texture);

void athena_texture_optimize(GSSURFACE *Texture);

uint32_t athena_surface_size(int width, int height, int psm);

uint32_t athena_vram_surface_size(int width, int height, int psm);

typedef enum {
	DRAW_BUFFER,
	DISPLAY_BUFFER,
	DEPTH_BUFFER
} eScreenBuffers;

typedef enum {
	ALPHA_TEST_ENABLE,
	ALPHA_TEST_METHOD,
	ALPHA_TEST_REF,
	ALPHA_TEST_FAIL,

	DST_ALPHA_TEST_ENABLE,
	DST_ALPHA_TEST_METHOD,

	DEPTH_TEST_ENABLE,
	DEPTH_TEST_METHOD,

	ALPHA_BLEND_EQUATION,
	SCISSOR_BOUNDS,
	PIXEL_ALPHA_BLEND_ENABLE,
	COLOR_CLAMP_MODE,
} eScreenParams;

typedef struct {
    uint64_t alpha_test_enabled     : 1;   
    uint64_t alpha_test_method      : 3;   
    uint64_t alpha_test_ref         : 8;   
    uint64_t alpha_fail_processing  : 2;   
    uint64_t dest_alpha_test_enabled: 1;   
    uint64_t dest_alpha_test_method : 1;   
    uint64_t depth_test_enabled     : 1;   
    uint64_t depth_test_method      : 2;   
    uint64_t                        : 45;  
} test_reg_fields;

typedef union {
	uint64_t data;
	test_reg_fields fields;
} test_reg;

typedef struct {
    uint64_t x0        : 11;
    uint64_t 		   : 5; 
    uint64_t x1        : 11;
    uint64_t 		   : 5; 
    uint64_t y0        : 11;
    uint64_t 	       : 5; 
    uint64_t y1        : 11;
    uint64_t 	       : 5; 
} scissor_reg_fields;

typedef union {
	uint64_t data;
	scissor_reg_fields fields;
} scissor_reg;

typedef struct {
    uint64_t a    : 2;  
    uint64_t b    : 2;  
    uint64_t c    : 2;  
    uint64_t d    : 2;  
    uint64_t fix  : 8;  
    uint64_t      : 48; 
} alpha_reg_fields;

typedef union {
	uint64_t data;
	alpha_reg_fields fields;
} alpha_reg;

typedef enum {
    ALPHA_NEVER,  
    ALPHA_ALWAYS, 
    ALPHA_LESS,   
    ALPHA_LEQUAL, 
    ALPHA_EQUAL,  
    ALPHA_GEQUAL, 
    ALPHA_GREATER,
    ALPHA_NEQUAL  
} alpha_test_methods;

typedef enum {
    ALPHA_FAIL_NO_UPDATE = 0,    
    ALPHA_FAIL_FB_ONLY = 1,      
    ALPHA_FAIL_ZB_ONLY = 2,      
    ALPHA_FAIL_RGB_ONLY = 3      
} alpha_fail_processing;

typedef enum {
    DEST_ALPHA_ZERO = 0,   
    DEST_ALPHA_ONE = 1     
} dest_alpha_test_method;

typedef enum {
    DEPTH_NEVER = 0,   
    DEPTH_ALWAYS = 1,  
    DEPTH_GEQUAL = 2,  
    DEPTH_GREATER = 3  
} depth_test_method;

typedef enum {
	GS_CACHE_TEX0,
	GS_CACHE_TEX0_2,
	GS_CACHE_CLAMP,
	GS_CACHE_CLAMP_2,
	GS_CACHE_TEX1,
	GS_CACHE_TEX1_2,
	GS_CACHE_TEX2,
	GS_CACHE_TEX2_2,
	GS_CACHE_XYOFFSET,
	GS_CACHE_XYOFFSET_2,
	GS_CACHE_PRMODECONT,
	GS_CACHE_PRMODE,
	GS_CACHE_TEXCLUT,
	GS_CACHE_MIPTBP1,
	GS_CACHE_MIPTBP1_2,
	GS_CACHE_MIPTBP2,
	GS_CACHE_MIPTBP2_2,
	GS_CACHE_TEXA,
	GS_CACHE_SCISSOR,
	GS_CACHE_SCISSOR_2,
	GS_CACHE_ALPHA,
	GS_CACHE_ALPHA_2,
	GS_CACHE_DIMX,
	GS_CACHE_DTHE,
	GS_CACHE_COLCLAMP,
	GS_CACHE_TEST,
	GS_CACHE_TEST_2,
	GS_CACHE_PABE,
	GS_CACHE_FBA,
	GS_CACHE_FBA_2,
	GS_CACHE_FRAME,
	GS_CACHE_FRAME_2,
	GS_CACHE_ZBUF,
	GS_CACHE_ZBUF_2,

	GS_CACHE_SIZE
} eGSRegCacheEntries;

uint64_t get_register(int reg_id);

void set_register(int reg_id, uint64_t data);

uint64_t get_screen_param(uint8_t param);

void set_screen_param(uint8_t param, uint64_t value);

void set_screen_buffer(eScreenBuffers id, GSSURFACE *buf, uint32_t mask);

int screen_switch_context();

void flush_gs_texcache();

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

extern GSCONTEXT *gsGlobal;
extern GSSURFACE *cur_screen_buffer[3];
extern GSSURFACE *main_screen_buffer[3];

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
	GSSURFACE *Texture;	///< Font Texture Object
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
    GSSURFACE *txt;
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

GSCONTEXT *getGSGLOBAL();

int GetInterlacedFrameMode();

typedef enum {
	VRAM_SIZE,
	VRAM_USED_TOTAL,
	VRAM_USED_STATIC,
	VRAM_USED_DYNAMIC
} eVRAMBlockType;

int getFreeVRAM(int mode);

float FPSCounter(int interval);

void setVideoMode(s16 mode, int width, int height, int psm, s16 interlace, s16 field, bool zbuffering, int psmz, bool double_buffering, uint8_t pass_count);

int load_image(GSSURFACE* image, const char* path, bool delayed);

void draw_image(GSSURFACE* source, float x, float y, float width, float height, float startx, float starty, float endx, float endy, Color color);
void draw_image_rotate(GSSURFACE* source, float x, float y, float width, float height, float startx, float starty, float endx, float endy, float angle, Color color);

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

void draw_tex_triangle_list(GSSURFACE* source, float x, float y, prim_tex_triangle *list, int list_size);

void draw_tex_triangle_gouraud_list(GSSURFACE* source, float x, float y, prim_tex_gouraud_triangle *list, int list_size);

void draw_image_list(GSSURFACE* source, float x, float y, prim_tex_sprite *list, int list_size);

void gs_copy_block(GSSURFACE *src, int src_x, int src_y, GSSURFACE *dst, int dst_x, int dst_y);

void draw_point(float x, float y, Color color);
void draw_line(float x, float y, float x2, float y2, Color color);
void draw_sprite(float x, float y, int width, int height, Color color);
void draw_circle(float x, float y, float radius, u64 color, u8 filled);
void draw_triangle(float x, float y, float x2, float y2, float x3, float y3, Color color);
void draw_triangle_gouraud(float x, float y, float x2, float y2, float x3, float y3, Color color, Color color2, Color color3);
void draw_quad(float x, float y, float x2, float y2, float x3, float y3, float x4, float y4, Color color);
void draw_quad_gouraud(float x, float y, float x2, float y2, float x3, float y3, float x4, float y4, Color color, Color color2, Color color3, Color color4);

GSFONT* loadFont(const char* path);
void printFontText(GSFONT* font, const char* text, float x, float y, float scale, Color color);
void unloadFont(GSFONT* font);

void athena_error_screen(const char* errMsg, bool dark_mode);

#endif
