/*
 * App store, in C: the port of main.js. Downloads the Brew Store catalog, then
 * downloads and extracts the package picked, all with async requests so the UI
 * keeps drawing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <athena/athena.h>
#include "athenaos/app.h"

static const AthenaOSHint CATALOG_HINTS[] = { { "D-pad", "Browse" }, { "cross", "Details" } };
static const AthenaOSHint DETAILS_HINTS[] = { { "cross", "Download" }, { "circle", "Back" } };

#define CATALOG_URL "https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json"
#define MAX_PACKAGES 64
#define LOG_LINES 16
#define ROW_H 48

typedef struct {
    char name[64], category[32], fname[96], link[256], icon[64];
    AthenaImage *image;
} Package;

static Package packages[MAX_PACKAGES];
static int package_count;

typedef struct {
    char lines[LOG_LINES][128];
    int count;
} Log;

static void log_push(Log *log, const char *line) {
    if (log->count == LOG_LINES) {
        memmove(log->lines[0], log->lines[1], sizeof(log->lines[0]) * (LOG_LINES - 1));
        log->count--;
    }
    snprintf(log->lines[log->count++], sizeof(log->lines[0]), "%s", line);
}

// --- brew_data.json: [{"name": "...", "category": "...", ...}, ...] ---

// The JSON string at *p into out; *p moves past it.
static void json_string(const char **p, char *out, size_t size) {
    size_t n = 0;
    const char *s = *p + 1;
    while (*s && *s != '"') {
        char c = *s++;
        if (c == '\\' && *s) {
            c = *s++;
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
        }
        if (n + 1 < size) out[n++] = c;
    }
    out[n] = '\0';
    *p = *s ? s + 1 : s;
}

// value into a field, cut to its size.
static void set_field(char *dst, size_t size, const char *value) {
    const size_t n = strlen(value) < size - 1 ? strlen(value) : size - 1;
    memcpy(dst, value, n);
    dst[n] = '\0';
}

static void load_app_db(const char *fname) {
    package_count = 0;
    FILE *f = fopen(fname, "r");
    if (!f)
        return;
    fseek(f, 0, SEEK_END);
    const long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *json = malloc(len + 1);
    json[fread(json, 1, len, f)] = '\0';
    fclose(f);

    const char *p = json;
    Package *pkg = NULL;
    while (*p) {
        if (*p == '{') {
            pkg = package_count < MAX_PACKAGES ? &packages[package_count++] : NULL;
            if (pkg) memset(pkg, 0, sizeof(*pkg));
            p++;
        } else if (*p == '"' && pkg) {
            char key[32], value[256];
            json_string(&p, key, sizeof(key));
            while (*p == ' ' || *p == ':') p++;
            if (*p != '"')
                continue;
            json_string(&p, value, sizeof(value));
            if (!strcmp(key, "name")) set_field(pkg->name, sizeof(pkg->name), value);
            else if (!strcmp(key, "category")) set_field(pkg->category, sizeof(pkg->category), value);
            else if (!strcmp(key, "fname")) set_field(pkg->fname, sizeof(pkg->fname), value);
            else if (!strcmp(key, "link")) set_field(pkg->link, sizeof(pkg->link), value);
            else if (!strcmp(key, "icon")) set_field(pkg->icon, sizeof(pkg->icon), value);
        } else {
            p++;
        }
    }
    free(json);
}

static void load_network_driver(void) {
    athena_iop_reset();
    athena_iop_load_module(athena_iop_get_module_by_name("usbmass_bd"), 0, NULL);
    module_entry *ps2fs = athena_iop_get_module_by_name("ps2fs");
    if (ps2fs)
        athena_iop_load_module(ps2fs, 0, NULL);
    athena_iop_load_module(athena_iop_get_module_by_name("cdfs"), 0, NULL);
    athena_iop_load_module(athena_iop_get_module_by_name("mcserv"), 0, NULL);
    athena_iop_load_module(athena_iop_get_module_by_name("padman"), 0, NULL);
    athena_iop_load_module(athena_iop_get_module_by_name("SMAP"), 0, NULL);
    athena_net_init_dhcp();
}

// req.asyncDownload(url, path) of the script binding.
static bool async_download(AthenaRequest *req, const char *url, const char *path) {
    req->url = url;
    req->method = ATHENA_REQUEST_GET;
    req->save = true;
    req->chunk.timer = 0;
    req->chunk.transferring = false;
    req->chunk.size = 0;
    req->chunk.fp = fopen(path, "wb");
    if (!req->chunk.fp)
        return false;
    if (athena_request_run_async(req, "Requests: Download") != 0) {
        fclose(req->chunk.fp);
        req->chunk.fp = NULL;
        return false;
    }
    return true;
}

static bool ends_with(const char *s, const char *suffix) {
    const size_t n = strlen(s), m = strlen(suffix);
    return n >= m && !strcmp(s + n - m, suffix);
}

enum { LOADING, MAIN_MENU, DLING_MENU = 3 };
enum { LD_INITIAL, LD_NETWORK, LD_PKGLIST, LD_PKGQNTD };
enum { DETAILS, TODOWNLOAD, DOWNLOADING, DOWNLOADED, EXTRACTING, EXTRACTED };
enum { NOT_UPDATED, UPDATING, UPDATED };

static AthenaOSApp ui;
static int sel, top, app_state;
static Log dling_log;

static void draw_icon(int i, float x, float cy, float size) {
    AthenaImage *icn = packages[i].image;
    if (!icn)
        return;
    icn->width = size;
    icn->height = size;
    athena_image_draw(icn, x, cy - size / 2);
}

static void draw_log(const Log *log, float x, float y, float w, float h) {
    char buf[160];
    athenaos_panel(&ui, x, y, w, h);
    for (int i = 0; i < log->count; i++)
        athenaos_text_v(&ui, x + 16, y + 20 + i * 20, athenaos_fit(&ui, log->lines[i], 0.46f, w - 32, buf, sizeof(buf)), 0.46f,
                        i == log->count - 1 ? athenaos_rgba(255, 255, 255, 1.0f) : athenaos_rgba(255, 255, 255, 0.62f));
}

static void draw_details(float x, float y, float w, float h) {
    const Package *pkg = &packages[sel];
    const Color text = athenaos_rgba(255, 255, 255, 1.0f), dim = athenaos_rgba(255, 255, 255, 0.62f);
    char buf[160];
    athenaos_panel(&ui, x, y, w, h);
    draw_icon(sel, x + 20, y + 52, 64);
    athenaos_text_v(&ui, x + 100, y + 38, athenaos_fit(&ui, pkg->name, 0.8f, w - 120, buf, sizeof(buf)), 0.8f, text);
    athenaos_text_v(&ui, x + 100, y + 66, pkg->category, 0.46f, dim);

    athenaos_heading(&ui, x + 20, y + 112, "Package");
    athenaos_text_v(&ui, x + 20, y + 132, athenaos_fit(&ui, pkg->fname, 0.44f, w - 40, buf, sizeof(buf)), 0.44f, text);
    athenaos_heading(&ui, x + 20, y + 162, "Source");
    // The link has no spaces: one wrapped line, fitted.
    athenaos_text_v(&ui, x + 20, y + 182, athenaos_fit(&ui, pkg->link, 0.40f, w - 40, buf, sizeof(buf)), 0.40f, dim);

    if (app_state == DLING_MENU) {
        const int first = dling_log.count > 4 ? dling_log.count - 4 : 0;
        for (int i = first; i < dling_log.count; i++)
            athenaos_text_v(&ui, x + 20, y + 250 + (i - first) * 20,
                            athenaos_fit(&ui, dling_log.lines[i], 0.46f, w - 40, buf, sizeof(buf)), 0.46f, text);
    }
}

static void draw_catalog(void) {
    const float x = ui.content.x, y = ui.content.y, w = ui.content.w, h = ui.content.h;
    const float list_w = 260;
    const int rows = (int)((h - 12) / ROW_H);
    if (sel < top) top = sel;
    if (sel >= top + rows) top = sel - rows + 1;
    char buf[96];

    athenaos_panel(&ui, x, y, list_w, h);
    for (int i = top; i < package_count && i < top + rows; i++) {
        const float cy = y + 6 + (i - top) * ROW_H + ROW_H / 2;
        if (i == sel) {
            athena_draw_rect(x + 6, cy - ROW_H / 2 + 2, list_w - 12, ROW_H - 4, athenaos_rgba(255, 255, 255, 0.10f));
            athena_draw_rect(x + 6, cy - ROW_H / 2 + 2, 3, ROW_H - 4, athenaos_accent(1.0f));
        }
        draw_icon(i, x + 18, cy, 36);
        athenaos_text_v(&ui, x + 64, cy - 7, athenaos_fit(&ui, packages[i].name, 0.5f, list_w - 80, buf, sizeof(buf)), 0.5f,
                        i == sel ? athenaos_rgba(255, 255, 255, 1.0f) : athenaos_rgba(255, 255, 255, 0.62f));
        athenaos_text_v(&ui, x + 64, cy + 10, packages[i].category, 0.38f, athenaos_rgba(255, 255, 255, 0.38f));
    }

    draw_details(x + list_w + 16, y, w - list_w - 16, h);
}

static int app_store_main(int argc, char **argv) {
    (void)argc; (void)argv;

    athenaos_app_init(&ui, "App store", "network", NULL, 0);
    athena_screen_set_param(DEPTH_TEST_ENABLE, false);

    AthenaRequest *req = athena_request_create();
    athena_request_set_keepalive(req, true);

    app_state = LOADING;
    int loading_state = LD_INITIAL, dl_state = DETAILS, update_state = NOT_UPDATED;
    Log loading_log = { .count = 0 };
    log_push(&loading_log, "Athena package manager");
    memset(&dling_log, 0, sizeof(dling_log));
    sel = top = 0;
    bool terminate = false, transfering = false;
    char pkg_path[400];

    AthenaPad *pad = athena_pad_open(0);

    for (;;) {
        athena_pad_update(pad);

        const AthenaOSHint *hints = NULL;
        int hint_count = 0;

        switch (app_state) {
            case LOADING:
                switch (loading_state) {
                    case LD_INITIAL:
                        log_push(&loading_log, "Loading network driver...");
                        loading_state++;
                        break;
                    case LD_NETWORK:
                        load_network_driver();
                        log_push(&loading_log, "Loading package list, it's gonna take a while...");
                        loading_state++;
                        break;
                    case LD_PKGLIST:
                        if (update_state == NOT_UPDATED) {
                            async_download(req, CATALOG_URL, "brew_data.json");
                            update_state = UPDATING;
                            transfering = true;
                        } else if (update_state == UPDATING) {
                            if (athena_request_poll(req, 5000, 3000)) {
                                transfering = false;
                                update_state = UPDATED;
                            }
                        } else {
                            load_app_db("brew_data.json");
                            char line[64];
                            snprintf(line, sizeof(line), "%d packages found.", package_count);
                            log_push(&loading_log, line);
                            loading_state++;
                        }
                        break;
                    case LD_PKGQNTD:
                        for (int i = 0; i < package_count; i++) {
                            char path[128];
                            snprintf(path, sizeof(path), "store/icons/%s", packages[i].icon);
                            packages[i].image = athena_image_create(path, true);
                            if (packages[i].image)
                                athena_image_set_filter(packages[i].image, GS_FILTER_LINEAR);
                        }
                        app_state = MAIN_MENU;
                        break;
                }
                athenaos_background(&ui);
                draw_log(&loading_log, ui.content.x, ui.content.y, ui.content.w, 40 + loading_log.count * 20);
                break;
            case MAIN_MENU:
                if (package_count) {
                    if (athena_pad_just_pressed(pad, PAD_DOWN)) sel = (sel + 1) % package_count;
                    if (athena_pad_just_pressed(pad, PAD_UP)) sel = (sel + package_count - 1) % package_count;
                    if (athena_pad_just_pressed(pad, PAD_CROSS)) app_state = DLING_MENU;
                }
                if (athenaos_home_requested(pad))
                    terminate = true;
                athenaos_background(&ui);
                if (package_count)
                    draw_catalog();
                hints = CATALOG_HINTS;
                hint_count = 2;
                break;
            case DLING_MENU:
                switch (dl_state) {
                    case DETAILS:
                        if (athena_pad_just_pressed(pad, PAD_CROSS)) {
                            log_push(&dling_log, "Downloading package...");
                            log_push(&dling_log, "This is an alpha version, it will take a long time");
                            dl_state++;
                        }
                        if (athena_pad_just_pressed(pad, PAD_CIRCLE))
                            app_state = MAIN_MENU;
                        if (athenaos_home_requested(pad))
                            terminate = true;
                        hints = DETAILS_HINTS;
                        hint_count = 2;
                        break;
                    case TODOWNLOAD:
                        snprintf(pkg_path, sizeof(pkg_path), "%s/%s", boot_path, packages[sel].fname);
                        async_download(req, packages[sel].link, pkg_path);
                        transfering = true;
                        dl_state++;
                        break;
                    case DOWNLOADING: {
                        const bool ready = athena_request_poll(req, 30, 3000);
                        if (req->error) {
                            log_push(&dling_log, req->error);
                            log_push(&dling_log, "Restarting application...");
                            for (int i = 0; i < 300; i++) {
                                athenaos_background(&ui);
                                draw_catalog();
                                athenaos_chrome_hints(&ui, NULL, 0);
                                athena_screen_flip();
                            }
                            athena_net_deinit();
                            // The script reloads itself; this ELF reloads this app.
                            char self[300];
                            snprintf(self, sizeof(self), "%s/athena_demos.elf", boot_path);
                            char *args[] = { "--app=app_store" };
                            athena_system_load_elf(self, 1, args, false);
                        } else if (ready) {
                            transfering = false;
                            dl_state++;
                        }
                        break;
                    }
                    case DOWNLOADED:
                        if (!ends_with(packages[sel].fname, ".elf") && !ends_with(packages[sel].fname, ".ELF")) {
                            dl_state++;
                            log_push(&dling_log, "Extracting files...");
                        } else {
                            dl_state = EXTRACTED;
                        }
                        break;
                    case EXTRACTING:
                        snprintf(pkg_path, sizeof(pkg_path), "%s/%s", boot_path, packages[sel].fname);
                        if (ends_with(packages[sel].fname, ".tar.gz")) {
                            athena_archive_untar(pkg_path);
                        } else if (ends_with(packages[sel].fname, ".zip")) {
                            AthenaArchive *arc = athena_archive_open(pkg_path);
                            if (arc) {
                                athena_archive_extract_all(arc, NULL, NULL);
                                athena_archive_close(arc);
                            }
                        }
                        dl_state++;
                        break;
                    case EXTRACTED:
                        app_state = MAIN_MENU;
                        dl_state = DETAILS;
                        dling_log.count = 0;
                        break;
                }
                athenaos_background(&ui);
                draw_catalog();
                break;
        }

        if (terminate)
            break;

        if (transfering)
            snprintf(ui.status, sizeof(ui.status), "%u bytes", (unsigned)req->chunk.size);
        else
            ui.status[0] = '\0';
        // Home is only offered where leaving is safe (not mid-download).
        ui.home.glyph = (app_state == MAIN_MENU || (app_state == DLING_MENU && dl_state == DETAILS)) ? "triangle" : NULL;
        athenaos_chrome_hints(&ui, hints, hint_count);
        athena_screen_flip();
    }

    athena_net_deinit();
    athena_request_destroy(req);
    return 0;
}

ATHENAOS_APP(app_store, app_store_main)
