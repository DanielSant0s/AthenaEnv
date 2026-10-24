/*
 * DVD Remote demo, in C: the port of main.js. Logs the DVD remote's buttons;
 * STOP also leaves.
 */

#include <athena/athena.h>
#include "athenaos/app.h"

static const struct { uint32_t button; const char *name; } BUTTON_NAMES[] = {
    { RM_DVD_PLAY, "PLAY" }, { RM_DVD_PAUSE, "PAUSE" }, { RM_DVD_STOP, "STOP" }, { RM_DVD_NEXT, "NEXT" },
    { RM_DVD_PREV, "PREV" }, { RM_DVD_SCAN_FORW, "SCAN_FORW" }, { RM_DVD_SCAN_BACK, "SCAN_BACK" },
};

static int remote_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSTerminal term;
    athenaos_terminal_init(&term, "DVD Remote demo", "input",
                           "Point the DVD remote at the console -- STOP also leaves", NULL, 0);
    AthenaPad *pad = athena_pad_open(0);

    athena_iop_load_module(athena_iop_get_module_by_name("rmman"), 0, NULL);
    athena_remote_open();

    // The remote control shows up on controller port 2 (index 1), whether it's
    // the external IR dongle on a fat PS2 or the built-in receiver on a slim.
    AthenaRemote *remote = athena_remote_get(1);

    for (;;) {
        athena_remote_update(remote);
        athena_pad_update(pad);

        if (athenaos_home_requested(pad))
            break;

        if (!athena_remote_released(remote) && remote->button != remote->old_button) {
            const char *name = NULL;
            for (size_t i = 0; i < sizeof(BUTTON_NAMES) / sizeof(BUTTON_NAMES[0]); i++)
                if (BUTTON_NAMES[i].button == remote->button)
                    name = BUTTON_NAMES[i].name;
            if (name)
                athenaos_terminal_log(&term, "remote button: %s", name);
            else
                athenaos_terminal_log(&term, "remote button: 0x%x", (unsigned)remote->button);
        }

        if (athena_remote_pressed(remote, RM_DVD_PLAY))
            athenaos_terminal_log(&term, "-> play requested");
        if (athena_remote_pressed(remote, RM_DVD_STOP))
            break;

        athenaos_terminal_draw(&term);
    }

    athena_remote_free(remote);
    return 0;
}

ATHENAOS_APP(remote, remote_main)
