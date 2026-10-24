// AthenaOS app SDK for C -- see app.h. Mirrors athenaos/app.js, gfx.js and ui.js.

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app.h"

#define PILL_SCALE 0.40f
#define LABEL_SCALE 0.46f
#define PI 3.14159265f

static const struct { const char *id; int rgb[3]; } CATEGORIES[] = {
    { "games",   { 255,  95, 120 } },
    { "3d",      {  40, 160, 255 } },
    { "2d",      {  40, 210, 160 } },
    { "media",   { 255, 170,  40 } },
    { "network", {  90, 200, 255 } },
    { "input",   { 170,  90, 255 } },
    { "system",  { 150, 160, 180 } },
};

static const int ACCENTS[][3] = {
    { 104, 112, 255 }, { 40, 160, 255 }, { 40, 210, 160 },
    { 255, 170, 40 }, { 255, 80, 130 }, { 170, 90, 255 },
};

// athenaos/settings.json, the fields the chrome uses.
static struct {
    bool loaded;
    int accent;
    bool clock24;
    int utc_offset;
} settings = { false, 0, true, 0 };

static AthenaFont *shared_font;

static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }

// The value after "key": in a flat JSON object, or NULL.
static const char *json_value(const char *json, const char *key) {
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p)
        return NULL;
    p = strchr(p + strlen(pattern), ':');
    if (!p)
        return NULL;
    p++;
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

static void load_settings(void) {
    if (settings.loaded)
        return;
    settings.loaded = true;
    FILE *f = fopen("athenaos/settings.json", "r");
    if (!f)
        return;
    char buf[512];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    const char *v;
    if ((v = json_value(buf, "accent")))
        settings.accent = atoi(v) % (int)(sizeof(ACCENTS) / sizeof(ACCENTS[0]));
    if ((v = json_value(buf, "clock24")))
        settings.clock24 = strncmp(v, "false", 5) != 0;
    if ((v = json_value(buf, "utcOffset")))
        settings.utc_offset = atoi(v);
}

Color athenaos_rgba(int r, int g, int b, float a) {
    return athena_color_rgba(r, g, b, (int)(a * 128.0f + 0.5f));
}

static int text_top(float cy, float scale) { return (int)lroundf(cy - 15.5f * scale + 2.0f); }

void athenaos_app_init(AthenaOSApp *ui, const char *title, const char *category,
                       const AthenaOSHint *hints, int hint_count) {
    AthenaVideoMode mode;
    memset(ui, 0, sizeof(*ui));
    athena_screen_get_mode(&mode);
    ui->W = mode.width > 0 ? mode.width : 640;
    ui->H = mode.height > 0 ? mode.height : 448;
    ui->M = (int)lroundf(ui->W * 0.075f);
    ui->title = title;
    ui->hints = hints;
    ui->hint_count = hint_count;
    ui->home.glyph = "triangle";
    ui->home.label = "Home";
    const int last = sizeof(CATEGORIES) / sizeof(CATEGORIES[0]) - 1;
    int c = last;
    for (int i = 0; i <= last; i++)
        if (category && !strcmp(category, CATEGORIES[i].id))
            c = i;
    memcpy(ui->category_rgb, CATEGORIES[c].rgb, sizeof(ui->category_rgb));
    ui->content.x = ui->M;
    ui->content.y = ATHENAOS_BAR_H + 14;
    ui->content.w = ui->W - 2 * ui->M;
    ui->content.h = ui->H - ATHENAOS_BAR_H - ATHENAOS_FOOTER_H - 28;

    load_settings();
    if (!shared_font)
        shared_font = athena_font_load("fonts/Segoe UI.ttf");
    ui->font = shared_font;
}

bool athenaos_home_requested(const AthenaPad *pad) {
    return pad && athena_pad_just_pressed(pad, PAD_TRIANGLE);
}

float athenaos_measure(AthenaOSApp *ui, const char *str, float scale) {
    if (!ui->font)
        return 0.0f;
    athena_font_set_scale(ui->font, scale);
    return athena_font_get_text_size(ui->font, str).width;
}

void athenaos_text_v(AthenaOSApp *ui, float x, float cy, const char *str, float scale, Color color) {
    if (!ui->font)
        return;
    athena_font_set_scale(ui->font, scale);
    athena_font_set_color(ui->font, color);
    athena_font_print(ui->font, roundf(x), (float)text_top(cy, scale), str);
}

void athenaos_text_v_align(AthenaOSApp *ui, float x, float cy, const char *str, float scale, Color color, int align) {
    if (!ui->font)
        return;
    athena_font_set_align(ui->font, align);
    athenaos_text_v(ui, x, cy, str, scale, color);
    athena_font_set_align(ui->font, ALIGN_NONE);
}

const char *athenaos_fit(AthenaOSApp *ui, const char *str, float scale, float max_w, char *out, size_t size) {
    snprintf(out, size, "%s", str);
    if (athenaos_measure(ui, out, scale) <= max_w)
        return out;
    char probe[160];
    size_t len = strlen(out);
    while (len > 1) {
        out[--len] = '\0';
        snprintf(probe, sizeof(probe), "%s..", out);
        if (athenaos_measure(ui, probe, scale) <= max_w)
            break;
    }
    while (len > 0 && out[len - 1] == ' ')
        out[--len] = '\0';
    snprintf(probe, sizeof(probe), "%s..", out);
    snprintf(out, size, "%s", probe);
    return out;
}

void athenaos_auto_chrome(AthenaOSApp *ui, const AthenaPad *pad) {
    static int idle;
    const bool active = pad->btns != 0 || pad->lx > 64 || pad->lx < -64 || pad->ly > 64 || pad->ly < -64;
    idle = active ? 0 : idle + 1;
    if (idle < 240)
        athenaos_chrome(ui);
}

// A quarter disc as a fan, centered on (cx, cy), towards (sx, sy).
static void corner(float cx, float cy, float r, float sx, float sy, Color color) {
    const int steps = r > 6.0f ? 6 : 3;
    for (int i = 0; i < steps; i++) {
        float a0 = (PI * 0.5f) * i / steps, a1 = (PI * 0.5f) * (i + 1) / steps;
        athena_draw_triangle(cx, cy,
            cx + sx * cosf(a0) * r, cy + sy * sinf(a0) * r,
            cx + sx * cosf(a1) * r, cy + sy * sinf(a1) * r, color);
    }
}

// Non-overlapping pieces, so translucent fills blend evenly.
void athenaos_round_rect(float x, float y, float w, float h, float r, Color color) {
    x = roundf(x); y = roundf(y); w = roundf(w); h = roundf(h);
    r = roundf(minf(r, minf(w * 0.5f, h * 0.5f)));
    if (r < 1.0f) { athena_draw_rect(x, y, w, h, color); return; }
    athena_draw_rect(x + r, y, w - 2 * r, h, color);
    athena_draw_rect(x, y + r, r, h - 2 * r, color);
    athena_draw_rect(x + w - r, y + r, r, h - 2 * r, color);
    corner(x + r,     y + r,     r, -1, -1, color);
    corner(x + w - r, y + r,     r,  1, -1, color);
    corner(x + r,     y + h - r, r, -1,  1, color);
    corner(x + w - r, y + h - r, r,  1,  1, color);
}

void athenaos_panel(AthenaOSApp *ui, float x, float y, float w, float h) {
    (void)ui;
    athenaos_round_rect(x, y, w, h, 14, athenaos_rgba(40, 44, 64, 0.70f));
}

Color athenaos_accent(float a) {
    load_settings();
    const int *c = ACCENTS[settings.accent];
    return athenaos_rgba(c[0], c[1], c[2], a);
}

void athenaos_heading(AthenaOSApp *ui, float x, float cy, const char *str) {
    char upper[96];
    size_t i = 0;
    for (; str[i] && i < sizeof(upper) - 1; i++)
        upper[i] = (str[i] >= 'a' && str[i] <= 'z') ? str[i] - 'a' + 'A' : str[i];
    upper[i] = '\0';
    athenaos_text_v(ui, x, cy, upper, 0.38f, athenaos_accent(1.0f));
}

// Same backdrop as the shell's "Midnight" wallpaper, tinted by the accent.
void athenaos_background(AthenaOSApp *ui) {
    const int *a = ACCENTS[settings.accent];
    const int base[3] = { 18, 20, 30 };
    int top[3];
    for (int i = 0; i < 3; i++)
        top[i] = (int)lroundf(base[i] + (a[i] - base[i]) * 0.12f);
    athena_screen_clear(athenaos_rgba(0, 0, 0, 1.0f));
    Color t = athenaos_rgba(top[0], top[1], top[2], 1.0f), b = athenaos_rgba(4, 4, 8, 1.0f);
    athena_draw_quad_gouraud(0, 0, ui->W, 0, 0, ui->H, ui->W, ui->H, t, t, b, b);
}

void athenaos_hud(AthenaOSApp *ui, const char *const *lines, int count, int flags) {
    if (!ui->font || count <= 0)
        return;
    const float scale = 0.40f, lh = 15.0f;
    float w = 0.0f;
    for (int i = 0; i < count; i++)
        w = maxf(w, athenaos_measure(ui, lines[i], scale));
    w = roundf(w) + 20.0f;
    const float h = count * lh + 10.0f;
    float x = ui->M, y = ATHENAOS_BAR_H + 10;
    if (flags & ATHENAOS_HUD_RIGHT)
        x = ui->W - ui->M - w;
    if (flags & ATHENAOS_HUD_BOTTOM)
        y = ui->H - ATHENAOS_FOOTER_H - 10 - h;
    athenaos_round_rect(x, y, w, h, 8, athenaos_rgba(0, 0, 0, 0.45f));
    for (int i = 0; i < count; i++)
        athenaos_text_v(ui, x + 10, y + 5 + lh / 2 + i * lh, lines[i], scale,
                        i == 0 ? athenaos_rgba(255, 255, 255, 1.0f) : athenaos_rgba(255, 255, 255, 0.62f));
}

static bool is_button(const char *kind) {
    return !strcmp(kind, "cross") || !strcmp(kind, "circle") || !strcmp(kind, "square") || !strcmp(kind, "triangle");
}

static float glyph_width(AthenaOSApp *ui, const char *kind) {
    return is_button(kind) ? 16.0f : athenaos_measure(ui, kind, PILL_SCALE) + 12.0f;
}

// A PlayStation button glyph, or a text pill, vertically centered on cy.
static float glyph(AthenaOSApp *ui, const char *kind, float x, float cy) {
    x = roundf(x); cy = roundf(cy);
    if (!is_button(kind)) {
        float w = glyph_width(ui, kind);
        athenaos_round_rect(x, cy - 8, w, 16, 8, athenaos_rgba(255, 255, 255, 0.16f));
        athena_font_set_scale(ui->font, PILL_SCALE);
        athena_font_set_color(ui->font, athenaos_rgba(255, 255, 255, 1.0f));
        athena_font_print(ui->font, x + 6, (float)text_top(cy, PILL_SCALE), kind);
        return w;
    }
    athena_draw_circle(x + 8, cy, 8, athenaos_rgba(255, 255, 255, 0.16f), true);
    if (!strcmp(kind, "cross")) {
        Color c = athenaos_rgba(124, 178, 232, 1.0f);
        athena_draw_line(x + 5, cy - 3, x + 11, cy + 3, c);
        athena_draw_line(x + 11, cy - 3, x + 5, cy + 3, c);
    } else if (!strcmp(kind, "circle")) {
        athena_draw_circle(x + 8, cy, 4, athenaos_rgba(255, 102, 102, 1.0f), false);
    } else if (!strcmp(kind, "square")) {
        Color c = athenaos_rgba(255, 105, 248, 1.0f);
        athena_draw_line(x + 5, cy - 3, x + 11, cy - 3, c);
        athena_draw_line(x + 11, cy - 3, x + 11, cy + 3, c);
        athena_draw_line(x + 11, cy + 3, x + 5, cy + 3, c);
        athena_draw_line(x + 5, cy + 3, x + 5, cy - 3, c);
    } else {
        Color c = athenaos_rgba(64, 226, 160, 1.0f);
        athena_draw_line(x + 8, cy - 4, x + 12, cy + 3, c);
        athena_draw_line(x + 12, cy + 3, x + 4, cy + 3, c);
        athena_draw_line(x + 4, cy + 3, x + 8, cy - 4, c);
    }
    return 16.0f;
}

static void clock_text(char *out, size_t size) {
    time_t now = time(NULL) + settings.utc_offset * 3600;
    struct tm *tm = gmtime(&now);
    const int h = tm ? tm->tm_hour : 0, m = tm ? tm->tm_min : 0;
    if (settings.clock24)
        snprintf(out, size, "%02d:%02d", h, m);
    else
        snprintf(out, size, "%d:%02d %s", (h % 12) ? (h % 12) : 12, m, h < 12 ? "AM" : "PM");
}

// Hints right-aligned at `right`; gaps, then label size, shrink until they fit.
static void hints_row(AthenaOSApp *ui, float right, float cy, float max_w) {
    const AthenaOSHint *list[17];
    int count = 0;
    for (int i = 0; i < ui->hint_count && count < 16; i++)
        list[count++] = &ui->hints[i];
    if (ui->home.glyph)
        list[count++] = &ui->home;

    float gap = 18.0f, label_scale = LABEL_SCALE, widths[17], total;
    for (;;) {
        total = -gap;
        for (int i = 0; i < count; i++) {
            widths[i] = glyph_width(ui, list[i]->glyph) + 6 + athenaos_measure(ui, list[i]->label, label_scale);
            total += widths[i] + gap;
        }
        if (total <= max_w) break;
        if (gap > 10.0f) gap = 10.0f;
        else if (label_scale > 0.34f) label_scale -= 0.02f;
        else break;
    }
    float x = right - total;
    for (int i = 0; i < count; i++) {
        float gw = glyph(ui, list[i]->glyph, x, cy);
        athena_font_set_scale(ui->font, label_scale);
        athena_font_set_color(ui->font, athenaos_rgba(255, 255, 255, 0.62f));
        athena_font_print(ui->font, roundf(x + gw + 6), (float)text_top(cy, label_scale), list[i]->label);
        x += widths[i] + gap;
    }
}

void athenaos_chrome(AthenaOSApp *ui) {
    if (!ui->font)
        return;
    const uint64_t depth = athena_screen_get_param(DEPTH_TEST_ENABLE);
    if (depth)
        athena_screen_set_param(DEPTH_TEST_ENABLE, false);

    const float cy = ATHENAOS_BAR_H / 2;
    athena_draw_rect(0, 0, ui->W, ATHENAOS_BAR_H, athenaos_rgba(0, 0, 0, 0.45f));
    athena_draw_circle(ui->M + 4, cy, 4, athenaos_rgba(ui->category_rgb[0], ui->category_rgb[1], ui->category_rgb[2], 1.0f), true);
    athenaos_text_v(ui, ui->M + 14, cy, ui->title, 0.48f, athenaos_rgba(255, 255, 255, 1.0f));

    char clock[16];
    clock_text(clock, sizeof(clock));
    float x = ui->W - ui->M - athenaos_measure(ui, clock, 0.48f);
    athenaos_text_v(ui, x, cy, clock, 0.48f, athenaos_rgba(255, 255, 255, 1.0f));
    if (ui->status[0]) {
        x -= 16 + athenaos_measure(ui, ui->status, 0.44f);
        athenaos_text_v(ui, x, cy, ui->status, 0.44f, athenaos_rgba(255, 255, 255, 0.62f));
    }

    const float fy = ui->H - ATHENAOS_FOOTER_H, fcy = fy + ATHENAOS_FOOTER_H / 2;
    athena_draw_rect(0, fy, ui->W, ATHENAOS_FOOTER_H, athenaos_rgba(0, 0, 0, 0.45f));
    const float left_w = ui->footer_left ? roundf(athenaos_measure(ui, ui->footer_left, 0.44f)) + 20 : 0;
    hints_row(ui, ui->W - ui->M, fcy, ui->W - 2 * ui->M - left_w);
    if (ui->footer_left)
        athenaos_text_v(ui, ui->M, fcy, ui->footer_left, 0.44f, athenaos_rgba(255, 255, 255, 0.62f));

    if (depth)
        athena_screen_set_param(DEPTH_TEST_ENABLE, depth);
}

void athenaos_chrome_hints(AthenaOSApp *ui, const AthenaOSHint *hints, int hint_count) {
    const AthenaOSHint *saved = ui->hints;
    const int saved_count = ui->hint_count;
    ui->hints = hints;
    ui->hint_count = hint_count;
    athenaos_chrome(ui);
    ui->hints = saved;
    ui->hint_count = saved_count;
}

void athenaos_loading(AthenaOSApp *ui, const char *message, const char *detail) {
    const uint64_t depth = athena_screen_get_param(DEPTH_TEST_ENABLE);
    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
    athenaos_background(ui);
    const float cy = ui->H / 2;
    athenaos_text_v(ui, (ui->W - athenaos_measure(ui, message, 0.62f)) / 2, cy - 10, message, 0.62f,
                    athenaos_rgba(255, 255, 255, 1.0f));
    if (detail && detail[0])
        athenaos_text_v(ui, (ui->W - athenaos_measure(ui, detail, 0.44f)) / 2, cy + 16, detail, 0.44f,
                        athenaos_rgba(255, 255, 255, 0.62f));
    const AthenaOSHint *hints = ui->hints;
    const int hint_count = ui->hint_count;
    ui->hint_count = 0;
    athenaos_chrome(ui);
    ui->hints = hints;
    ui->hint_count = hint_count;
    athena_screen_flip();
    athena_screen_set_param(DEPTH_TEST_ENABLE, depth);
}

// --- Terminal ---

#define TERM_SCALE 0.42f

void athenaos_terminal_init(AthenaOSTerminal *t, const char *title, const char *category, const char *subtitle,
                            const AthenaOSHint *hints, int hint_count) {
    memset(t, 0, sizeof(*t));
    athenaos_app_init(&t->ui, title, category, hints, hint_count);
    t->subtitle = subtitle;
    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
}

void athenaos_terminal_clear(AthenaOSTerminal *t) { t->count = 0; }

static void terminal_push(AthenaOSTerminal *t, const char *line, size_t len) {
    if (t->count == ATHENAOS_TERMINAL_LINES) {
        memmove(t->lines[0], t->lines[1], sizeof(t->lines[0]) * (ATHENAOS_TERMINAL_LINES - 1));
        t->count--;
    }
    if (len >= sizeof(t->lines[0]))
        len = sizeof(t->lines[0]) - 1;
    memcpy(t->lines[t->count], line, len);
    t->lines[t->count][len] = '\0';
    t->count++;
}

// Width of the first n characters of s.
static float measure_n(AthenaOSApp *ui, const char *s, size_t n) {
    char buf[256];
    if (n >= sizeof(buf))
        n = sizeof(buf) - 1;
    memcpy(buf, s, n);
    buf[n] = '\0';
    return athenaos_measure(ui, buf, TERM_SCALE);
}

// One source line: word-wrapped to max_w, a word longer than a line hard-split.
static void terminal_wrap(AthenaOSTerminal *t, const char *s, size_t len, float max_w) {
    if (len == 0) {
        terminal_push(t, "", 0);
        return;
    }
    size_t start = 0;
    while (start < len) {
        size_t end = start, last_space = 0;
        bool space_found = false;
        while (end < len && measure_n(&t->ui, s + start, end - start + 1) <= max_w) {
            if (s[end] == ' ') { last_space = end; space_found = true; }
            end++;
        }
        if (end < len && space_found && last_space > start)
            end = last_space;  // break at the last space that fits
        if (end == start)
            end = start + 1;   // not even one character fits: take it anyway
        terminal_push(t, s + start, end - start);
        start = end;
        while (start < len && s[start] == ' ')
            start++;
    }
}

void athenaos_terminal_log(AthenaOSTerminal *t, const char *fmt, ...) {
    // Sized to the message: demos log whole HTTP bodies.
    char small[512], *text = small;
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(small, sizeof(small), fmt, ap);
    va_end(ap);
    if (n >= (int)sizeof(small)) {
        text = malloc(n + 1);
        if (text) {
            va_start(ap, fmt);
            vsnprintf(text, n + 1, fmt, ap);
            va_end(ap);
        } else {
            text = small;
        }
    }
    printf("%s\n", text);
    for (char *c = text; *c; c++)
        if (*c == '\t') *c = ' ';
    const float max_w = t->ui.content.w - 24;
    const char *s = text;
    for (;;) {
        const char *nl = strchr(s, '\n');
        const size_t len = nl ? (size_t)(nl - s) : strlen(s);
        terminal_wrap(t, s, len, max_w);
        if (!nl)
            break;
        s = nl + 1;
    }
    if (text != small)
        free(text);
}

void athenaos_terminal_render(AthenaOSTerminal *t) {
    AthenaOSApp *ui = &t->ui;
    athenaos_background(ui);
    const float x = ui->content.x, y = ui->content.y, w = ui->content.w, h = ui->content.h;
    if (t->subtitle)
        athenaos_text_v(ui, x, y + 6, t->subtitle, 0.46f, athenaos_rgba(255, 255, 255, 0.62f));
    const float top = t->subtitle ? y + 20 : y;
    athenaos_round_rect(x, top, w, h - (top - y), 10, athenaos_rgba(0, 0, 0, 0.35f));
    for (int i = 0; i < t->count; i++)
        athenaos_text_v(ui, x + 12, top + 14 + i * 15, t->lines[i], TERM_SCALE, athenaos_rgba(255, 255, 255, 1.0f));
    athenaos_chrome(ui);
}

void athenaos_terminal_draw(AthenaOSTerminal *t) {
    athenaos_terminal_render(t);
    athena_screen_flip();
}

// --- Registry ---

#define MAX_APPS 64
static AthenaOSEntry registry[MAX_APPS];
static int registry_count;

void athenaos_register(const char *id, AthenaOSMain main) {
    if (registry_count < MAX_APPS) {
        registry[registry_count].id = id;
        registry[registry_count].main = main;
        registry_count++;
    }
}

int athenaos_app_count(void) { return registry_count; }

const AthenaOSEntry *athenaos_app_at(int index) {
    return (index >= 0 && index < registry_count) ? &registry[index] : NULL;
}

const AthenaOSEntry *athenaos_app_find(const char *id) {
    for (int i = 0; id && i < registry_count; i++)
        if (!strcmp(registry[i].id, id))
            return &registry[i];
    return NULL;
}
