/*
 * athena_demos.elf: every examples/<app>/main.c in one ELF (make demos).
 *
 *   --app=<id>             run that app
 *   --return=<elf>         when it returns, load this ELF (the shell that launched it) ...
 *   --return-script=<js>   ... running this script
 *
 * Without --app (or with an unknown one) it opens a launcher listing the apps;
 * Home there opens the AthenaOS shell.
 * An app picked there runs in a fresh copy of this ELF, which comes back to the
 * launcher: no app inherits another's video mode, buffers or GS state.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "athenaos/app.h"

#define SELF "athena_demos.elf"

static const char *arg_value(int argc, char **argv, const char *prefix) {
    const size_t n = strlen(prefix);
    for (int i = 1; i < argc; i++)
        if (!strncmp(argv[i], prefix, n))
            return argv[i] + n;
    return NULL;
}

// boot_path + name, whether or not boot_path ends with a slash.
static void boot_file(char *out, size_t size, const char *name) {
    const size_t n = strlen(boot_path);
    snprintf(out, size, "%s%s%s", boot_path, (n && boot_path[n - 1] != '/' && boot_path[n - 1] != ':') ? "/" : "", name);
}

// Loads elf (from the boot folder unless it has a device) with up to two args. Never returns.
static void load_elf(const char *elf, const char *arg1, const char *arg2) {
    char path[300];
    if (strchr(elf, ':'))
        snprintf(path, sizeof(path), "%s", elf);
    else
        boot_file(path, sizeof(path), elf);
    char *args[2];
    int n = 0;
    if (arg1) args[n++] = (char *)arg1;
    if (arg2) args[n++] = (char *)arg2;
    athena_screen_clear(athenaos_rgba(0, 0, 0, 1.0f));
    athena_screen_flip();
    athena_system_load_elf(path, n, args, false);
}

// --- Launcher ---

typedef struct {
    const AthenaOSEntry *entry;
    char name[64];
} LauncherItem;

// The "name" of apps/<id>/app.json, or the id.
static void app_name(const char *id, char *out, size_t size) {
    char path[96], buf[1024];
    snprintf(out, size, "%s", id);
    snprintf(path, sizeof(path), "apps/%s/app.json", id);
    FILE *f = fopen(path, "r");
    if (!f)
        return;
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    const char *p = strstr(buf, "\"name\"");
    if (!p || !(p = strchr(p + 6, ':')) || !(p = strchr(p, '"')))
        return;
    const char *end = strchr(++p, '"');
    if (end && (size_t)(end - p) < size) {
        memcpy(out, p, end - p);
        out[end - p] = '\0';
    }
}

static int item_order(const void *a, const void *b) {
    return strcasecmp(((const LauncherItem *)a)->name, ((const LauncherItem *)b)->name);
}

static const AthenaOSHint LAUNCHER_HINTS[] = { { "D-pad", "Select" }, { "cross", "Open" } };

static int launcher(void) {
    LauncherItem items[64];
    const int count = athenaos_app_count();
    for (int i = 0; i < count; i++) {
        items[i].entry = athenaos_app_at(i);
        app_name(items[i].entry->id, items[i].name, sizeof(items[i].name));
    }
    qsort(items, count, sizeof(LauncherItem), item_order);

    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
    AthenaOSApp ui;
    athenaos_app_init(&ui, "C apps", "system", LAUNCHER_HINTS, 2);
    AthenaPad *pad = athena_pad_open(0);
    int sel = 0;
    const float row = 30.0f;

    for (;;) {
        athena_pad_update(pad);
        // Home: the AthenaOS shell.
        if (athenaos_home_requested(pad))
            load_elf("athena.elf", "--script=main.js", NULL);
        if (count > 0 && athena_pad_just_pressed(pad, PAD_DOWN)) sel = (sel + 1) % count;
        if (count > 0 && athena_pad_just_pressed(pad, PAD_UP)) sel = (sel + count - 1) % count;
        if (count > 0 && athena_pad_just_pressed(pad, PAD_CROSS)) {
            char app_arg[80];
            snprintf(app_arg, sizeof(app_arg), "--app=%s", items[sel].entry->id);
            load_elf(SELF, app_arg, "--return=" SELF);
        }

        athenaos_background(&ui);
        const float x = ui.content.x, y = ui.content.y, w = ui.content.w;
        athenaos_panel(&ui, x, y, w, 20 + count * row);
        for (int i = 0; i < count; i++) {
            const float cy = y + 10 + row * i + row / 2;
            if (i == sel)
                athenaos_round_rect(x + 8, cy - row / 2 + 2, w - 16, row - 4, 8, athenaos_rgba(255, 255, 255, 0.12f));
            athenaos_text_v(&ui, x + 24, cy, items[i].name, 0.5f, athenaos_rgba(255, 255, 255, 1.0f));
            athenaos_text_v(&ui, x + w - 24 - athenaos_measure(&ui, items[i].entry->id, 0.42f), cy,
                            items[i].entry->id, 0.42f, athenaos_rgba(255, 255, 255, 0.62f));
        }
        if (count == 0)
            athenaos_text_v(&ui, x + 24, y + 40, "No C apps in this build.", 0.5f, athenaos_rgba(255, 255, 255, 0.62f));
        athenaos_chrome(&ui);
        athena_screen_flip();
    }
    return 0;
}

static int demos_main(int argc, char **argv) {
    while (!bootlogo_finished() && boot_logo)
        usleep(1000);

    const AthenaOSEntry *app = athenaos_app_find(arg_value(argc, argv, "--app="));
    if (!app)
        return launcher();

    int ret = app->main(argc, argv);

    const char *back = arg_value(argc, argv, "--return=");
    if (back) {
        const char *script = arg_value(argc, argv, "--return-script=");
        char script_arg[160];
        if (script)
            snprintf(script_arg, sizeof(script_arg), "--script=%s", script);
        load_elf(back, script ? script_arg : NULL, NULL);
    }
    return ret;
}

ATHENA_APP_ENTRY(demos_main);
