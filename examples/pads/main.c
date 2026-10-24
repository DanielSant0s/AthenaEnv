/*
 * Gamepad tester, in C: the port of main.js. Every button, the sticks and the
 * pressure-sensitive buttons; L2+R2 toggles rumble. Every button is under test
 * here, so Home is START+R3+UP instead of Triangle.
 */

#include <stdio.h>

#include <athena/athena.h>
#include "athenaos/app.h"

static const AthenaOSHint HINTS[] = { { "L2+R2", "Rumble" } };

// Controller layout is shifted down to clear the info card under the top bar.
#define OY 24

static AthenaPad *pad;
static Color opaque, translucent;

// Pressure-sensitive buttons fade in with the pressure applied.
static void pressure(AthenaImage *img, int btn, float x, float y) {
    img->color = athena_color_rgba(128, 128, 128, (pad->btns & btn) ? athena_pad_pressure(0, btn) : 60);
    athena_image_draw(img, x, y + OY);
}

static void digital(AthenaImage *img, int btn, float x, float y) {
    img->color = (pad->btns & btn) ? opaque : translucent;
    athena_image_draw(img, x, y + OY);
}

static AthenaImage *load(const char *name) {
    char path[64];
    snprintf(path, sizeof(path), "pads/%s.png", name);
    return athena_image_create(path, true);
}

static int pads_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaVideoMode mode;
    athena_screen_get_mode(&mode);
    mode.psm = GS_PSM_CT24;
    mode.zbuffering = true;
    mode.double_buffering = true;
    athena_screen_set_mode(&mode);
    athena_screen_set_param(DEPTH_TEST_ENABLE, false);

    athena_screen_set_vsync(false);
    athena_screen_set_frame_counter(true);

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Gamepad tester", "input", HINTS, 1);
    ui.home.glyph = "START+R3+UP";
    ui.footer_left = "Buttons fade in with pressure";

    const Color dark_gray = athena_color_rgba(128, 128, 128, 32);
    opaque = athena_color_rgba(128, 128, 128, 128);
    translucent = athena_color_rgba(128, 128, 128, 60);

    AthenaImage *circle = load("circle"), *cross = load("cross"), *square = load("square"), *triangle = load("triangle");
    AthenaImage *up = load("up"), *down = load("down"), *left = load("left"), *right = load("right");
    AthenaImage *start = load("start"), *select = load("select");
    AthenaImage *r1 = load("R1"), *r2 = load("R2"), *l1 = load("L1"), *l2 = load("L2"), *l3 = load("L3"), *r3 = load("R3");

    bool rumble = false;
    pad = athena_pad_open(0);

    for (;;) {
        const int connected_count = athena_pad_connected_count();
        const bool active0 = athena_pad_is_active(0), active1 = athena_pad_is_active(1);

        athena_pad_update(pad);

        if ((pad->btns & PAD_START) && (pad->btns & PAD_R3) && (pad->btns & PAD_UP))
            break;

        athenaos_background(&ui);

        char l0[48], l1t[48], l2t[64], l3t[32];
        snprintf(l0, sizeof(l0), "Connected pads: %d", connected_count);
        snprintf(l1t, sizeof(l1t), "Active ports: [%s%s%s]", active0 ? "0" : "", active0 && active1 ? ", " : "", active1 ? "1" : "");
        snprintf(l2t, sizeof(l2t), "Port 0: %s  |  Port 1: %s", active0 ? "ACTIVE" : "INACTIVE", active1 ? "ACTIVE" : "INACTIVE");
        snprintf(l3t, sizeof(l3t), "Rumble: %s", rumble ? "ON" : "OFF");
        const char *lines[] = { l0, l1t, l2t, l3t };
        athenaos_hud(&ui, lines, 4, 0);

        digital(select, PAD_SELECT, 260, 190);
        digital(start, PAD_START, 380, 190);

        pressure(up, PAD_UP, 120, 155);
        pressure(down, PAD_DOWN, 120, 225);
        pressure(left, PAD_LEFT, 85, 190);
        pressure(right, PAD_RIGHT, 155, 190);

        pressure(triangle, PAD_TRIANGLE, 520, 155);
        pressure(cross, PAD_CROSS, 520, 225);
        pressure(square, PAD_SQUARE, 485, 190);
        pressure(circle, PAD_CIRCLE, 555, 190);

        pressure(l1, PAD_L1, 102, 100);
        pressure(r1, PAD_R1, 502, 100);
        pressure(l2, PAD_L2, 137, 100);
        pressure(r2, PAD_R2, 537, 100);

        athena_draw_rect(220, 280 + OY, 75, 75, dark_gray);
        athena_draw_rect(380, 280 + OY, 75, 75, dark_gray);

        l3->color = (pad->btns & PAD_L3) ? opaque : translucent;
        athena_image_draw(l3, pad->lx / 4.0f + 242.0f, pad->ly / 4.0f + 300.0f + OY);

        r3->color = (pad->btns & PAD_R3) ? opaque : translucent;
        athena_image_draw(r3, pad->rx / 4.0f + 402.0f, pad->ry / 4.0f + 300.0f + OY);

        if (((pad->btns & PAD_R2) && (pad->btns & PAD_L2)) && ((pad->old_btns & PAD_R2) && !(pad->old_btns & PAD_L2))) {
            if (rumble) athena_pad_rumble(0, 0, 0);
            else athena_pad_rumble(0, 1, (char)255);
            rumble = !rumble;
        }

        snprintf(ui.status, sizeof(ui.status), "%.1f FPS", athena_screen_get_fps(360));
        athenaos_chrome(&ui);
        athena_screen_flip();
    }

    athena_pad_rumble(0, 0, 0);
    return 0;
}

ATHENAOS_APP(pads, pads_main)
