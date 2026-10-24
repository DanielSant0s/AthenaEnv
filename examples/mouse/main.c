/*
 * Mouse demo, in C: the port of main.js. A USB mouse moves a cursor; every
 * report with movement, scroll or buttons is logged.
 */

#include <athena/athena.h>
#include "athenaos/app.h"

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int mouse_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSTerminal term;
    athenaos_terminal_init(&term, "Mouse demo", "input", "Plug a USB mouse -- move, click and scroll", NULL, 0);
    AthenaPad *pad = athena_pad_open(0);

    athena_iop_load_module(athena_iop_get_module_by_name("ps2mouse"), 0, NULL);
    athena_mouse_open();

    AthenaMouseData data;
    athena_mouse_read(&data);
    float cursor_x = term.ui.W / 2.0f, cursor_y = term.ui.H / 2.0f;

    for (;;) {
        athena_pad_update(pad);
        if (athenaos_home_requested(pad))
            break;

        athena_mouse_read(&data);
        if (data.x || data.y || data.wheel || data.buttons)
            athenaos_terminal_log(&term, "mouse output {\"x\":%d,\"y\":%d,\"wheel\":%d,\"buttons\":%u}",
                                  (int)data.x, (int)data.y, (int)data.wheel, (unsigned)data.buttons);

        // Default read mode reports relative movement.
        cursor_x = clampf(cursor_x + data.x, 0, term.ui.W);
        cursor_y = clampf(cursor_y + data.y, 0, term.ui.H);

        athenaos_terminal_render(&term);
        athena_draw_triangle(cursor_x, cursor_y, cursor_x, cursor_y + 16, cursor_x + 11, cursor_y + 11,
                             data.buttons ? athenaos_accent(1.0f) : athena_color_rgba(255, 255, 255, 128));
        athena_screen_flip();
    }
    return 0;
}

ATHENAOS_APP(mouse, mouse_main)
