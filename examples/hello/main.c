/*
 * Hello world, in C: the port of main.js. The default font in four styles
 * (plain, color, outline, drop shadow) at two scales.
 */

#include <athena/athena.h>
#include "athenaos/app.h"

#define RGB(r, g, b) athena_color_rgba((r), (g), (b), 128)

// Prints the four variants (plain, color, outline, shadow) at one scale.
static void variants(AthenaFont *font, float x, float y, float scale, float step) {
    font->scale = scale;

    font->color = RGB(128, 128, 128);
    athena_font_print(font, x, y, "Hello World!");

    font->color = RGB(128, 0, 0);
    athena_font_print(font, x, y + step, "Hello World(with custom color)!");

    font->color = RGB(128, 128, 128);
    font->outline = 1.0f;
    font->outline_color = RGB(128, 0, 0);
    athena_font_print(font, x, y + step * 2, "Hello World(with outline)!");

    font->outline = 0.0f;
    font->outline_color = RGB(0, 0, 0);

    font->color = RGB(128, 128, 128);
    font->dropshadow = 1.0f;
    font->dropshadow_color = RGB(128, 0, 0);
    athena_font_print(font, x, y + step * 3, "Hello World(with drop shadow)!");

    font->dropshadow = 0.0f;
    font->dropshadow_color = RGB(0, 0, 0);
}

static int hello_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaPad *pad = athena_pad_open(0);
    AthenaFont *font = athena_font_load("default");

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Hello world", "2d", NULL, 0);

    athena_screen_set_param(DEPTH_TEST_ENABLE, false);

    for (;;) {
        athena_pad_update(pad);
        if (athenaos_home_requested(pad))
            break;

        athenaos_background(&ui);
        const float x = ui.content.x, y = ui.content.y, w = ui.content.w;

        athenaos_panel(&ui, x, y, w, 168);
        athenaos_heading(&ui, x + 20, y + 20, "Scale 1.0");
        variants(font, x + 20, y + 32, 1.0f, 32);

        athenaos_panel(&ui, x, y + 184, w, 120);
        athenaos_heading(&ui, x + 20, y + 204, "Scale 0.6");
        variants(font, x + 20, y + 216, 0.6f, 20);

        athenaos_chrome(&ui);
        athena_screen_flip();
    }
    return 0;
}

ATHENAOS_APP(hello, hello_main)
