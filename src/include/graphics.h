#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <kernel.h>

#include <gsKit.h>
#include <dmaKit.h>

#include <gsToolkit.h>
#include <gsInline.h>

#include <math3d.h>

#include <packet2.h>
#include <packet2_utils.h>

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

typedef struct ath_model {
	uint32_t facesCount;
    uint32_t indexCount;

    VECTOR* positions;
	VECTOR* texcoords;
	VECTOR* normals;
    VECTOR* colours;

    VECTOR bounding_box[8];

    void (*render)(struct ath_model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z);

    VECTOR *tmp_normals;
    VECTOR *tmp_lights;
    color_f_t *tmp_colours;
    vertex_f_t *tmp_xyz;

    void* vertices;

    GSTEXTURE* textures[10];
	int tex_ranges[10];
	int tex_count;

	int idx_ranges[150];
	int idx_range_count;
} model;

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

void gsKit_clear_screens();

GSGLOBAL *getGSGLOBAL();

int GetInterlacedFrameMode();

int getFreeVRAM();

float FPSCounter(int interval);

void setVideoMode(s16 mode, int width, int height, int psm, s16 interlace, s16 field, bool zbuffering, int psmz, bool double_buffering, uint8_t pass_count);

int load_image(GSTEXTURE* image, const char* path, bool delayed);

void drawImage(GSTEXTURE* source, float x, float y, float width, float height, float startx, float starty, float endx, float endy, Color color);
void drawImageRotate(GSTEXTURE* source, float x, float y, float width, float height, float startx, float starty, float endx, float endy, float angle, Color color);

void drawPixel(float x, float y, Color color);
void drawLine(float x, float y, float x2, float y2, Color color);
void drawRect(float x, float y, int width, int height, Color color);
void drawCircle(float x, float y, float radius, u64 color, u8 filled);
void drawTriangle(float x, float y, float x2, float y2, float x3, float y3, Color color);
void drawTriangle_gouraud(float x, float y, float x2, float y2, float x3, float y3, Color color, Color color2, Color color3);
void drawQuad(float x, float y, float x2, float y2, float x3, float y3, float x4, float y4, Color color);
void drawQuad_gouraud(float x, float y, float x2, float y2, float x3, float y3, float x4, float y4, Color color, Color color2, Color color3, Color color4);

void InvalidateTexture(GSTEXTURE *txt);
void UnloadTexture(GSTEXTURE *txt);

void fntDrawQuad(rm_quad_t *q);

GSFONT* loadFont(const char* path);
void printFontText(GSFONT* font, const char* text, float x, float y, float scale, Color color);
void unloadFont(GSFONT* font);

void loadFontM();
void printFontMText(const char* text, float x, float y, float scale, Color color);
void unloadFontM();


void init3D(float aspect);

void setCameraPosition(float x, float y, float z);
void setCameraRotation(float x, float y, float z);

void setLightQuantity(int quantity);
void createLight(int lightid, float dir_x, float dir_y, float dir_z, int type, float r, float g, float b);

model* loadOBJ(const char* path, GSTEXTURE* text);
void draw_bbox(model* m, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z, Color color);


void athena_error_screen(const char* errMsg, bool dark_mode);

#endif
