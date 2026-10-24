/*
 * DejaVu Font Demo, in C: the port of main.js. A bitmap font with outlines,
 * drop shadows, alignment and text measuring.
 */

#include <stdio.h>

#include <athena/athena.h>
#include "athenaos/app.h"

#define RGBA(r, g, b, a) athena_color_rgba((r), (g), (b), (a))

static int image_font_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaPad *pad = athena_pad_open(0);
    AthenaFont *font = athena_font_load("dejavu.bmp");

    AthenaOSApp ui;
    athenaos_app_init(&ui, "DejaVu Font Demo", "2d", NULL, 0);

    athena_screen_set_param(DEPTH_TEST_ENABLE, false);

    const float line_height = 27;

    for (;;) {
        athena_pad_update(pad);
        if (athenaos_home_requested(pad))
            break;

        athenaos_background(&ui);
        const float left = ui.content.x, right = ui.content.x + ui.content.w, center = ui.W / 2;
        float y = ui.content.y;

        font->scale = 1.0f;
        font->color = RGBA(255, 255, 255, 128);
        font->outline = 0;
        font->dropshadow = 0;
        athena_font_print(font, left, y, "DejaVu Bitmap Font Demo");
        y += line_height;

        font->scale = 1.0f;
        font->color = RGBA(255, 255, 0, 128);
        font->outline = 2.0f;
        font->outline_color = RGBA(0, 0, 0, 128);
        athena_font_print(font, left, y, "Text with black outline");
        y += line_height;

        font->color = RGBA(255, 255, 255, 128);
        font->outline = 1.5f;
        font->outline_color = RGBA(255, 0, 0, 128);
        athena_font_print(font, left, y, "White text with red outline");
        y += line_height;

        font->outline = 0;

        font->scale = 1.0f;
        font->color = RGBA(0, 255, 255, 128);
        font->dropshadow = 3.0f;
        font->dropshadow_color = RGBA(0, 0, 0, 80);
        athena_font_print(font, left, y, "Cyan text with drop shadow");
        y += line_height;

        font->color = RGBA(255, 128, 0, 128);
        font->dropshadow = 2.0f;
        font->dropshadow_color = RGBA(64, 32, 0, 100);
        athena_font_print(font, left, y, "Orange with brown shadow");
        y += line_height;

        font->dropshadow = 0;

        font->scale = 0.8f;
        font->color = RGBA(200, 200, 200, 128);

        font->align = ALIGN_LEFT;
        athena_font_print(font, left, y, "Left aligned (starts at x)");
        y += line_height;

        font->align = ALIGN_HCENTER;
        athena_font_print(font, center, y, "Center aligned (centered at x)");
        y += line_height;

        font->align = ALIGN_RIGHT;
        athena_font_print(font, right, y, "Right aligned (ends at x)");
        y += line_height;

        font->align = ALIGN_LEFT;

        font->scale = 1.2f;
        font->color = RGBA(255, 255, 255, 128);
        font->outline = 2.0f;
        font->outline_color = RGBA(128, 0, 255, 128);
        font->align = ALIGN_HCENTER;
        athena_font_print(font, center, y, "Combined: Outline + Center");
        y += line_height * 1.4f;

        font->outline = 0;
        font->dropshadow = 2.5f;
        font->dropshadow_color = RGBA(0, 0, 0, 100);
        font->color = RGBA(0, 255, 128, 128);
        athena_font_print(font, center, y, "Shadow + Center Align");
        y += line_height * 1.4f;

        font->dropshadow = 0;
        font->align = ALIGN_LEFT;

        const char *test_text = "Text Size Test";
        const Coords size = athena_font_get_text_size(font, test_text);
        font->scale = 1.0f;
        font->color = RGBA(128, 255, 128, 128);
        athena_font_print(font, left, y, test_text);

        char size_text[64];
        snprintf(size_text, sizeof(size_text), "Size: %dx%d pixels", (int)size.width, (int)size.height);
        font->scale = 0.7f;
        font->color = RGBA(128, 128, 128, 128);
        athena_font_print(font, left + 220, y + 4, size_text);

        athenaos_chrome(&ui);
        athena_screen_flip();
    }
    return 0;
}

ATHENAOS_APP(image_font, image_font_main)
