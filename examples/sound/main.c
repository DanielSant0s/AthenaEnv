/*
 * Audio player, in C: the port of main.js. Streams the OGG and WAV tracks in
 * sounds/, with a playlist, seek and play/pause.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <athena/athena.h>
#include "athenaos/app.h"

static const AthenaOSHint HINTS[] = {
    { "START", "Play/Pause" }, { "L1", "Prev" }, { "R1", "Next" }, { "D-pad", "Seek" }, { "square", "Rewind" },
};

#define MAX_TRACKS 32

typedef struct {
    char name[64];
    char file[96];
} Track;

static Track tracks[MAX_TRACKS];
static int track_count, current;
static AthenaSound *track;
static int duration;

static bool ends_with(const char *s, const char *suffix) {
    const size_t n = strlen(s), m = strlen(suffix);
    return n >= m && !strcmp(s + n - m, suffix);
}

static void ms_to_second_min(int ms, char *out, size_t size) {
    const int minutes = ms / 60000;
    int seconds = (int)lroundf(ms / 1000.0f) - minutes * 60;
    snprintf(out, size, seconds < 10 ? "%d:0%d" : "%d:%d", minutes, seconds);
}

static AthenaSound *open_track(int index) {
    char path[128];
    snprintf(path, sizeof(path), "sounds/%s", tracks[index].file);
    return athena_sound_load(path);
}

static void switch_track(int index) {
    athena_sound_pause();
    current = (index + track_count) % track_count;
    AthenaSound *new_track = open_track(current);
    duration = athena_sound_get_duration(new_track);
    athena_sound_play(new_track);
    AthenaSound *old_track = track;
    track = new_track;
    athena_sound_free(old_track);
}

static void draw_icon(AthenaImage *img, float cx, float cy, Color color) {
    img->color = color;
    athena_image_draw(img, cx - img->width / 2, cy - img->height / 2);
}

static AthenaImage *load_icon(const char *path) {
    AthenaImage *img = athena_image_create(path, true);
    img->width /= 4;
    img->height /= 4;
    athena_image_set_filter(img, GS_FILTER_LINEAR);
    return img;
}

static int sound_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Audio player", "media", HINTS, 5);

    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
    athena_sound_set_volume(100);

    AthenaDirListing *listing = athena_system_list_dir("/sounds");
    track_count = 0;
    for (int i = 0; listing && i < listing->count && track_count < MAX_TRACKS; i++) {
        const char *file = listing->entries[i].name;
        if (!ends_with(file, ".ogg") && !ends_with(file, ".wav"))
            continue;
        Track *t = &tracks[track_count++];
        snprintf(t->file, sizeof(t->file), "%s", file);
        snprintf(t->name, sizeof(t->name), "%.*s", (int)(strlen(file) - 4), file);
    }
    athena_system_dir_listing_free(listing);
    if (track_count == 0)
        return 0;

    current = 0;
    track = open_track(current);
    duration = athena_sound_get_duration(track);
    athena_sound_play(track);

    AthenaPad *pad = athena_pad_open(0);

    AthenaImage *icon_play = load_icon("amp/play.png"), *icon_pause = load_icon("amp/pause.png");
    AthenaImage *icon_next = load_icon("amp/next.png"), *icon_back = load_icon("amp/back.png");

    const Color text = athenaos_rgba(255, 255, 255, 1.0f), text_dim = athenaos_rgba(255, 255, 255, 0.62f);

    for (;;) {
        athena_pad_update(pad);
        if (athenaos_home_requested(pad))
            break;

        if (athena_pad_just_pressed(pad, PAD_START)) {
            if (athena_sound_is_playing(track))
                athena_sound_pause();
            else
                athena_sound_play(track);
        }

        if (athena_pad_just_pressed(pad, PAD_L1)) switch_track(current - 1);
        if (athena_pad_just_pressed(pad, PAD_R1)) switch_track(current + 1);

        if (athena_pad_just_pressed(pad, PAD_RIGHT) && athena_sound_is_playing(track))
            athena_sound_set_position(track, athena_sound_get_position(track) + 5000);
        else if (athena_pad_just_pressed(pad, PAD_LEFT) && athena_sound_is_playing(track))
            athena_sound_set_position(track, athena_sound_get_position(track) - 5000);

        if (athena_pad_just_pressed(pad, PAD_SQUARE))
            athena_sound_rewind(track);

        athenaos_background(&ui);
        const float x = ui.content.x, y = ui.content.y, w = ui.content.w, h = ui.content.h;
        char buf[96];

        // Playlist
        const float list_w = 200;
        athenaos_panel(&ui, x, y, list_w, h);
        snprintf(buf, sizeof(buf), "Playlist - %d tracks", track_count);
        athenaos_heading(&ui, x + 16, y + 20, buf);
        for (int i = 0; i < track_count; i++) {
            const float ry = y + 40 + i * 30;
            if (i == current) {
                athena_draw_rect(x + 8, ry, list_w - 16, 26, athenaos_rgba(255, 255, 255, 0.10f));
                athena_draw_rect(x + 8, ry, 3, 26, athenaos_accent(1.0f));
            }
            athenaos_text_v(&ui, x + 20, ry + 13, athenaos_fit(&ui, tracks[i].name, 0.46f, list_w - 40, buf, sizeof(buf)),
                            0.46f, i == current ? text : text_dim);
        }

        // Now playing
        const bool playing = athena_sound_is_playing(track);
        const float px = x + list_w + 16, pw = w - list_w - 16, cx = roundf(px + pw / 2);
        athenaos_panel(&ui, px, y, pw, h);
        athenaos_heading(&ui, px + 20, y + 20, playing ? "Now playing" : "Paused");
        athenaos_text_v_align(&ui, cx, y + 110, athenaos_fit(&ui, tracks[current].name, 0.8f, pw - 40, buf, sizeof(buf)),
                              0.8f, text, ALIGN_HCENTER);

        const int position = athena_sound_get_position(track);
        const float t = duration ? fminf((float)position / duration, 1) : 0;
        const float bx = px + 30, bw = pw - 60, by = y + 170;
        athena_draw_rect(bx, by, bw, 6, athenaos_rgba(255, 255, 255, 0.15f));
        athena_draw_rect(bx, by, bw * t, 6, athenaos_accent(1.0f));
        athena_draw_circle(bx + bw * t, by + 3, 7, text, true);
        ms_to_second_min(position, buf, sizeof(buf));
        athenaos_text_v(&ui, bx, by + 24, buf, 0.44f, text_dim);
        ms_to_second_min(duration, buf, sizeof(buf));
        athenaos_text_v_align(&ui, bx + bw, by + 24, buf, 0.44f, text_dim, ALIGN_RIGHT);

        const float iy = y + 260;
        draw_icon(icon_back, cx - 90, iy, athenaos_rgba(255, 255, 255, 0.8f));
        athena_draw_circle(cx, iy, 30, athenaos_accent(1.0f), true);
        draw_icon(playing ? icon_pause : icon_play, cx, iy, text);
        draw_icon(icon_next, cx + 90, iy, athenaos_rgba(255, 255, 255, 0.8f));

        athenaos_chrome(&ui);
        athena_screen_flip();
    }

    athena_sound_pause();
    athena_sound_free(track);
    return 0;
}

ATHENAOS_APP(sound, sound_main)
