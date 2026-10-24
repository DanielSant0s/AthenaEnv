/*
 * File manager, in C: the port of main.js. Browse memory cards, USB, HDD and
 * the host; copy, move and delete; open scripts and ELFs; extract archives.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <athena/athena.h>
#include "athenaos/app.h"

static const AthenaOSHint HINTS[] = { { "cross", "Open" }, { "circle", "Up" }, { "R1", "Actions" } };
static const AthenaOSHint MENU_HINTS[] = { { "cross", "Run" }, { "circle", "Close" } };

static const char *const ACTIONS[] = { "Copy", "Move", "Paste", "Rename", "Delete" };
#define ACTION_COUNT 5

static AthenaDirEntry ROOT[] = {
    { "cdfs:/", 0, true }, { "host:/", 0, true }, { "mass:/", 0, true }, { "mc0:/", 0, true }, { "mc1:/", 0, true },
};

#define ROW_H 24

static char path[256];              // "" (device list) or always ends with "/"
static AthenaDirListing *listing;   // NULL at the device list
static AthenaDirEntry *file;
static int file_count;
static int sel, top;

static bool ends_with(const char *s, const char *suffix) {
    const size_t n = strlen(s), m = strlen(suffix);
    return n >= m && !strcmp(s + n - m, suffix);
}

static void list(const char *new_path) {
    char copy[256];
    snprintf(copy, sizeof(copy), "%s", new_path);
    snprintf(path, sizeof(path), "%s", copy);
    if (listing) {
        athena_system_dir_listing_free(listing);
        listing = NULL;
    }
    if (path[0] == '\0') {
        file = ROOT;
        file_count = sizeof(ROOT) / sizeof(ROOT[0]);
    } else {
        listing = athena_system_list_dir(path);
        file = listing ? listing->entries : NULL;
        file_count = listing ? listing->count : 0;
    }
    const int max_sel = file_count > 0 ? file_count - 1 : 0;
    if (sel > max_sel)
        sel = max_sel;
}

static void go_up(void) {
    char *slash = strrchr(path, '/');
    if (slash && slash > path && slash[-1] != ':') {
        *slash = '\0';
        slash = strrchr(path, '/');
        char parent[256];
        snprintf(parent, sizeof(parent), "%.*s", slash ? (int)(slash - path + 1) : 0, path);
        list(parent);
    } else {
        list("");
    }
    sel = 0;
}

static void open_entry(const AthenaDirEntry *entry) {
    char full[512];
    snprintf(full, sizeof(full), "%s%s", path, entry->name);
    if (entry->is_dir) {
        sel = 0;
        if (path[0] == '\0')
            list(entry->name);
        else {
            char next[300];
            snprintf(next, sizeof(next), "%s%s/", path, entry->name);
            list(next);
        }
    } else if (ends_with(entry->name, ".js")) {
        char elf[300], script[530];
        const size_t n = strlen(boot_path);
        snprintf(elf, sizeof(elf), "%.*s/athena.elf", (int)(n && boot_path[n - 1] == '/' ? n - 1 : n), boot_path);
        snprintf(script, sizeof(script), "--script=%s", full);
        char *args[] = { script };
        athena_system_load_elf(elf, 1, args, false);
    } else if (ends_with(entry->name, ".elf") || ends_with(entry->name, ".ELF")) {
        athena_system_load_elf(full, 0, NULL, false);
    } else if (ends_with(entry->name, ".zip")) {
        AthenaArchive *arc = athena_archive_open(full);
        if (arc) {
            athena_archive_extract_all(arc, NULL, NULL);
            athena_archive_close(arc);
        }
        list(path);
    } else if (ends_with(entry->name, ".tar.gz")) {
        athena_archive_untar(full);
        list(path);
    }
}

static void format_size(uint32_t bytes, char *out, size_t size) {
    if (bytes < 1024) snprintf(out, size, "%u B", (unsigned)bytes);
    else if (bytes < 1048576) snprintf(out, size, "%.1f KB", bytes / 1024.0);
    else snprintf(out, size, "%.1f MB", bytes / 1048576.0);
}

static int app_filemgr_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSApp ui;
    athenaos_app_init(&ui, "File manager", "system", HINTS, 3);
    athena_screen_set_param(DEPTH_TEST_ENABLE, false);

    bool menu = false;
    int action = 0;
    // Clipboard for copy/move -> paste
    char src[512] = "", srcfile[256] = "";
    int fileop = 0; // 0 = copy, 1 = move

    path[0] = '\0';
    listing = NULL;
    sel = top = 0;
    list("");

    const Color text = athenaos_rgba(255, 255, 255, 1.0f), dim = athenaos_rgba(255, 255, 255, 0.62f);
    const Color faint = athenaos_rgba(255, 255, 255, 0.38f);
    AthenaPad *pad = athena_pad_open(0);
    char buf[256], left[32];

    for (;;) {
        athena_pad_update(pad);
        if (athenaos_home_requested(pad))
            break;

        if (athena_pad_just_pressed(pad, PAD_R1)) {
            menu = !menu;
            action = 0;
        }

        if (menu) {
            if (athena_pad_just_pressed(pad, PAD_DOWN)) action = (action + 1) % ACTION_COUNT;
            if (athena_pad_just_pressed(pad, PAD_UP)) action = (action + ACTION_COUNT - 1) % ACTION_COUNT;
            if (athena_pad_just_pressed(pad, PAD_CROSS)) {
                const AthenaDirEntry *entry = file_count ? &file[sel] : NULL;
                bool done = true;
                switch (action) {
                    case 0:
                    case 1:
                        if (!entry || path[0] == '\0') { done = false; break; }
                        fileop = action;
                        snprintf(src, sizeof(src), "%s%s", path, entry->name);
                        snprintf(srcfile, sizeof(srcfile), "%s", entry->name);
                        break;
                    case 2: {
                        if (!srcfile[0] || path[0] == '\0') { done = false; break; }
                        char dst[512];
                        snprintf(dst, sizeof(dst), "%s%s", path, srcfile);
                        if (fileop == 0) athena_system_copy_file(src, dst);
                        else athena_system_move_file(src, dst);
                        break;
                    }
                    case 3:
                        break;
                    case 4: {
                        if (!entry || path[0] == '\0') { done = false; break; }
                        char target[512];
                        snprintf(target, sizeof(target), "%s%s", path, entry->name);
                        if (entry->is_dir) athena_system_remove_dir(target);
                        else athena_system_remove_file(target);
                        break;
                    }
                }
                if (done) {
                    menu = false;
                    list(path);
                }
            }
            if (athena_pad_just_pressed(pad, PAD_CIRCLE)) menu = false;
        } else {
            if (athena_pad_just_pressed(pad, PAD_DOWN) && file_count) sel = (sel + 1) % file_count;
            if (athena_pad_just_pressed(pad, PAD_UP) && file_count) sel = (sel + file_count - 1) % file_count;
            if (athena_pad_just_pressed(pad, PAD_CROSS) && file_count) open_entry(&file[sel]);
            if (athena_pad_just_pressed(pad, PAD_CIRCLE)) go_up();
        }

        athenaos_background(&ui);
        const float x = ui.content.x, y = ui.content.y, w = ui.content.w, h = ui.content.h;

        // Location bar
        athenaos_heading(&ui, x, y + 6, "Location");
        athenaos_text_v(&ui, x + 72, y + 6, athenaos_fit(&ui, path[0] ? path : "Devices", 0.5f, w - 72, buf, sizeof(buf)), 0.5f, text);

        // File list
        const float ly = y + 20, lh = h - 20;
        const int rows = (int)((lh - 12) / ROW_H);
        if (sel < top) top = sel;
        if (sel >= top + rows) top = sel - rows + 1;
        athenaos_panel(&ui, x, ly, w, lh);

        if (!file_count)
            athenaos_text_v_align(&ui, x + w / 2, ly + lh / 2, "Empty folder", 0.5f, dim, ALIGN_HCENTER);

        for (int i = top; i < file_count && i < top + rows; i++) {
            const AthenaDirEntry *entry = &file[i];
            const float cy = ly + 6 + (i - top) * ROW_H + ROW_H / 2;
            if (i == sel) {
                athena_draw_rect(x + 6, cy - ROW_H / 2, w - 12, ROW_H, athenaos_rgba(255, 255, 255, menu ? 0.06f : 0.10f));
                athena_draw_rect(x + 6, cy - ROW_H / 2, 3, ROW_H, athenaos_accent(1.0f));
            }
            // Folder / file marker
            if (entry->is_dir) athena_draw_rect(x + 18, cy - 5, 14, 10, athenaos_accent(0.85f));
            else athena_draw_rect(x + 20, cy - 6, 10, 12, athenaos_rgba(255, 255, 255, 0.45f));
            athenaos_text_v(&ui, x + 42, cy, athenaos_fit(&ui, entry->name, 0.46f, w - 160, buf, sizeof(buf)), 0.46f,
                            i == sel ? text : dim);
            char size_text[32];
            if (entry->is_dir) snprintf(size_text, sizeof(size_text), "Folder");
            else format_size(entry->size, size_text, sizeof(size_text));
            athenaos_text_v_align(&ui, x + w - 16, cy, size_text, 0.42f, faint, ALIGN_RIGHT);
        }

        // Actions menu
        if (menu) {
            const float mw = 150, mh = ACTION_COUNT * 30 + 16, mx = x + w - mw - 12, my = ly + 12;
            athenaos_panel(&ui, mx, my, mw, mh);
            for (int i = 0; i < ACTION_COUNT; i++) {
                const float cy = my + 8 + i * 30 + 15;
                const bool disabled = (i == 2 && !srcfile[0]) || i == 3;
                if (i == action) athena_draw_rect(mx + 6, cy - 13, mw - 12, 26, athenaos_accent(0.85f));
                athenaos_text_v(&ui, mx + 18, cy, ACTIONS[i], 0.48f, disabled ? faint : text);
            }
        }

        if (srcfile[0])
            snprintf(ui.status, sizeof(ui.status), "%s: %s", fileop == 0 ? "Copy" : "Move",
                     athenaos_fit(&ui, srcfile, 0.44f, 140, buf, sizeof(buf)));
        else
            ui.status[0] = '\0';
        snprintf(left, sizeof(left), "%d items", file_count);
        ui.footer_left = left;
        if (menu)
            athenaos_chrome_hints(&ui, MENU_HINTS, 2);
        else
            athenaos_chrome(&ui);
        athena_screen_flip();
    }

    if (listing)
        athena_system_dir_listing_free(listing);
    return 0;
}

ATHENAOS_APP(app_filemgr, app_filemgr_main)
