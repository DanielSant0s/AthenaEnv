/*
 * 2D Draw, in C: the port of main.js. One card per drawing primitive.
 */

#include <math.h>

#include <athena/athena.h>
#include "athenaos/app.h"

#define RGB(r, g, b) athena_color_rgba((r), (g), (b), 128)

static Color pixel_colors[9];

static void sample_point(float cx, float cy) {
    for (int row = -4; row <= 4; row++)
        for (int i = 0; i < 9; i++)
            athena_draw_point(cx - 16 + i * 4, cy + row * 4, pixel_colors[i]);
}

static void sample_line(float cx, float cy) {
    for (int i = 0; i < 6; i++)
        athena_draw_line(cx - 30, cy + 25, cx - 30 + i * 12, cy - 25, pixel_colors[i]);
}

static void sample_rect(float cx, float cy) { athena_draw_rect(cx - 20, cy - 20, 40, 40, RGB(255, 0, 0)); }

static void sample_circle(float cx, float cy) {
    athena_draw_circle(cx, cy, 26, RGB(64, 64, 64), true);
    athena_draw_circle(cx, cy, 26, RGB(255, 0, 0), false);
}

static void flat_triangle(float cx, float cy) {
    athena_draw_triangle(cx - 22, cy + 20, cx - 22, cy - 20, cx + 22, cy + 20, RGB(0, 0, 255));
}

static void gouraud_triangle(float cx, float cy) {
    athena_draw_triangle_gouraud(cx - 22, cy + 20, cx - 22, cy - 20, cx + 22, cy + 20,
                                 RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255));
}

static void flat_quad(float cx, float cy) {
    athena_draw_quad(cx - 20, cy - 20, cx - 20, cy + 20, cx + 20, cy - 20, cx + 20, cy + 20, RGB(128, 0, 255));
}

static void gouraud_quad(float cx, float cy) {
    athena_draw_quad_gouraud(cx - 20, cy - 20, cx - 20, cy + 20, cx + 20, cy - 20, cx + 20, cy + 20,
                             RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 0));
}

// Each sample draws centered on (cx, cy) inside its card.
static const struct { const char *label; void (*draw)(float cx, float cy); } SAMPLES[] = {
    { "Draw.point", sample_point }, { "Draw.line", sample_line }, { "Draw.rect", sample_rect },
    { "Draw.circle", sample_circle }, { "Flat triangle", flat_triangle }, { "Gouraud triangle", gouraud_triangle },
    { "Flat quad", flat_quad }, { "Gouraud quad", gouraud_quad },
};

#define COLS 4
#define GAP 12

static int prim_draw_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaPad *pad = athena_pad_open(0);
    AthenaOSApp ui;
    athenaos_app_init(&ui, "2D Draw", "2d", NULL, 0);

    athena_screen_set_param(DEPTH_TEST_ENABLE, false);

    const int colors[9][3] = {
        { 255, 0, 0 }, { 255, 165, 0 }, { 255, 255, 0 }, { 0, 255, 0 }, { 0, 255, 165 },
        { 0, 255, 255 }, { 0, 0, 255 }, { 128, 0, 255 }, { 255, 0, 255 },
    };
    for (int i = 0; i < 9; i++)
        pixel_colors[i] = RGB(colors[i][0], colors[i][1], colors[i][2]);

    for (;;) {
        athena_pad_update(pad);
        if (athenaos_home_requested(pad))
            break;

        athenaos_background(&ui);

        const float x = ui.content.x, y = ui.content.y, w = ui.content.w, h = ui.content.h;
        const float cw = (w - GAP * (COLS - 1)) / COLS;
        const float ch = (h - GAP) / 2;

        for (int i = 0; i < (int)(sizeof(SAMPLES) / sizeof(SAMPLES[0])); i++) {
            const float cx = x + (i % COLS) * (cw + GAP), cy = y + (i / COLS) * (ch + GAP);
            athenaos_panel(&ui, cx, cy, cw, ch);
            athenaos_heading(&ui, cx + 14, cy + 18, SAMPLES[i].label);
            SAMPLES[i].draw(roundf(cx + cw / 2), roundf(cy + ch / 2 + 10));
        }

        athenaos_chrome(&ui);
        athena_screen_flip();
    }
    return 0;
}

ATHENAOS_APP(prim_draw, prim_draw_main)
