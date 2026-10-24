/*
 * AthenaOS app SDK for C -- the C side of athenaos/app.js and app.lua, so a C
 * app gets the same chrome as the shell and the script apps:
 *
 *   static const AthenaOSHint hints[] = { { "cross", "Jump" }, { "L1", "Dance" } };
 *
 *   static int my_main(int argc, char **argv) {
 *       AthenaOSApp ui;
 *       athenaos_app_init(&ui, "My demo", "3d", hints, 2);
 *       while (!athenaos_home_requested(pad)) {
 *           ...draw...
 *           athenaos_hud(&ui, lines, count, 0);
 *           athenaos_chrome(&ui);
 *           athena_screen_flip();
 *       }
 *       return 0;   // back to the shell (or the C launcher)
 *   }
 *   ATHENAOS_APP(my_demo, my_main)
 *
 * ATHENAOS_APP builds the app either alone (make capp CAPP_NAME=my_demo, the
 * app is the ELF's entry point) or into athena_demos.elf next to every other C
 * app (make demos), which runs the one named by --app=<id>.
 */
#ifndef ATHENAOS_APP_H
#define ATHENAOS_APP_H

#include <stdbool.h>

#include <athena/athena.h>

#define ATHENAOS_BAR_H 26     // top app/status bar
#define ATHENAOS_FOOTER_H 34  // bottom hint bar

typedef struct {
    const char *glyph;  // "cross", "circle", "square", "triangle", or the text of a pill ("L1", "START"...)
    const char *label;
} AthenaOSHint;

typedef struct {
    AthenaFont *font;
    int W, H, M;          // screen size and the shared left/right margin
    const char *title;
    int category_rgb[3];
    const AthenaOSHint *hints;
    int hint_count;
    AthenaOSHint home;    // appended to the hints; { "triangle", "Home" } unless the app changes it
    const char *footer_left;  // optional text on the left of the footer
    char status[48];      // right side of the top bar, e.g. "60 FPS"
    struct { int x, y, w, h; } content;  // the area between the bars
} AthenaOSApp;

// Before chdir() into an asset folder: the font and the settings are read
// from the boot folder. category: games, 3d, 2d, media, network, input, system.
void athenaos_app_init(AthenaOSApp *ui, const char *title, const char *category,
                       const AthenaOSHint *hints, int hint_count);

// Triangle, same in every app.
bool athenaos_home_requested(const AthenaPad *pad);

// rgba(r, g, b, a) of gfx.js: a in 0..1.
Color athenaos_rgba(int r, int g, int b, float a);

void athenaos_background(AthenaOSApp *ui);  // skip in full-screen 3D/video apps
void athenaos_text_v(AthenaOSApp *ui, float x, float cy, const char *str, float scale, Color color);
float athenaos_measure(AthenaOSApp *ui, const char *str, float scale);
// textV with a font alignment (ALIGN_HCENTER, ALIGN_RIGHT...).
void athenaos_text_v_align(AthenaOSApp *ui, float x, float cy, const char *str, float scale, Color color, int align);
// str cut to fit max_w, ending in ".." (into out).
const char *athenaos_fit(AthenaOSApp *ui, const char *str, float scale, float max_w, char *out, size_t size);
// Chrome for full-screen media: shown while the pad is in use, hidden after
// ~4 s idle so it does not cover the picture. Call every frame.
void athenaos_auto_chrome(AthenaOSApp *ui, const AthenaPad *pad);
void athenaos_round_rect(float x, float y, float w, float h, float r, Color color);
void athenaos_panel(AthenaOSApp *ui, float x, float y, float w, float h);
// The settings' accent color, at alpha a (0..1).
Color athenaos_accent(float a);
// Section heading inside the content area: upper case, in the accent color.
void athenaos_heading(AthenaOSApp *ui, float x, float cy, const char *str);

#define ATHENAOS_HUD_RIGHT  1
#define ATHENAOS_HUD_BOTTOM 2
// Translucent stats card; top-left under the bar by default.
void athenaos_hud(AthenaOSApp *ui, const char *const *lines, int count, int flags);

// Top bar + footer hints (Home appended). Disables the depth test while it draws.
void athenaos_chrome(AthenaOSApp *ui);
// The same with other hints than the app's, e.g. per game state.
void athenaos_chrome_hints(AthenaOSApp *ui, const AthenaOSHint *hints, int hint_count);

// One-shot frame for long blocking work (asset loading): message, detail (or
// NULL), the chrome without hints, and a flip.
void athenaos_loading(AthenaOSApp *ui, const char *message, const char *detail);

// --- Terminal: scrolling text console with the standard chrome, for demos that mostly log ---

#define ATHENAOS_TERMINAL_LINES 20

typedef struct {
    AthenaOSApp ui;
    const char *subtitle;
    char lines[ATHENAOS_TERMINAL_LINES][128];
    int count;
} AthenaOSTerminal;

void athenaos_terminal_init(AthenaOSTerminal *t, const char *title, const char *category, const char *subtitle,
                            const AthenaOSHint *hints, int hint_count);
// printf-style; also printed to the console. Wraps to the panel's width.
void athenaos_terminal_log(AthenaOSTerminal *t, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void athenaos_terminal_clear(AthenaOSTerminal *t);
// Draws without flipping, so callers can add their own overlay.
void athenaos_terminal_render(AthenaOSTerminal *t);
// render + flip.
void athenaos_terminal_draw(AthenaOSTerminal *t);

// --- App registry (athena_demos.elf) ---

typedef int (*AthenaOSMain)(int argc, char **argv);

typedef struct {
    const char *id;     // the folder under examples/ and apps/
    AthenaOSMain main;
} AthenaOSEntry;

void athenaos_register(const char *id, AthenaOSMain main);
int athenaos_app_count(void);
const AthenaOSEntry *athenaos_app_at(int index);
const AthenaOSEntry *athenaos_app_find(const char *id);

#ifdef ATHENAOS_DEMOS
#define ATHENAOS_APP(id, fn)                                                   \
    static void athenaos_register_##id(void) __attribute__((constructor));    \
    static void athenaos_register_##id(void) { athenaos_register(#id, fn); }
#else
#define ATHENAOS_APP(id, fn) ATHENA_APP_ENTRY(fn)
#endif

#endif /* ATHENAOS_APP_H */
