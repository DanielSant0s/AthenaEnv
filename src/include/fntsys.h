#ifndef __FNTSYS_H
#define __FNTSYS_H

#include <gsToolkit.h>

/// Maximal count of atlases per font
#define ATLAS_MAX    4
/// Atlas width in pixels
#define ATLAS_WIDTH  256
/// Atlas height in pixels
#define ATLAS_HEIGHT 256

#define FNTSYS_CHAR_SIZE 26


/// default (built-in) font id
#define FNT_DEFAULT (0)
/// Value returned on errors
#define FNT_ERROR   (-1)

/** Initializes the font subsystem */
void fntInit();

/** Terminates the font subsystem */
void fntEnd();

/** Loads a font from a file path
 * @param path The path to the font file
 * @return font slot id (negative value means error happened) */
int fntLoadFile(const char *path);

/** Reloads the default font */
int fntLoadDefault(const char *path);

/** Releases a font slot */
void fntRelease(int id);

/** Updates to the native display resolution and aspect ratio
 * @note Invalidates the whole glyph cache for all fonts! */
void fntUpdateAspectRatio();

/** Renders a text with specified window dimensions */
int fntRenderString(int id, int x, int y, short aligned, size_t width, size_t height, const char *string, u64 colour);

/** replaces spaces with newlines so that the text fits into the specified width.
 * @note A destrutive operation - modifies the given string!
 */
void fntFitString(int id, char *string, size_t width);

/** Calculates the width of the given text string
 * We can't use the height for alignment, as the horizontal center would depends of the contained text itself */
int fntCalcDimensions(int id, const char *str);

void fntSetPixelSize(int fontid, int width, int height);

void fntSetCharSize(int fontid, int width, int height);

typedef struct {
    int width;
    int height;
} Coords;

Coords fntGetTextSize(int id, const char* text);

#endif
