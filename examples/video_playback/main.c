/*
 * Video Playback, in C: the port of main.js. Fullscreen MPEG-1/2 playback of
 * video.m2v (an MPEG-2 elementary stream), e.g. from ffmpeg:
 *   ffmpeg -i input.mp4 -vf scale=320:240 -c:v mpeg2video -b:v 2000k video.m2v
 */

#include <stdio.h>

#include <athena/athena.h>
#include "athenaos/app.h"

static const AthenaOSHint HINTS[] = { { "square", "Loop" } };

static int video_playback_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Video Playback", "media", HINTS, 1);

    AthenaVideo *video = athena_video_create("video.m2v");
    if (!video)
        return 0;
    athena_video_set_loop(video, false);
    athena_video_play(video);

    printf("Video loaded:\n");
    printf("  Width: %d\n", athena_video_get_width(video));
    printf("  Height: %d\n", athena_video_get_height(video));
    printf("  FPS: %g\n", athena_video_get_fps(video));

    bool should_finish = false;
    AthenaPad *pad = athena_pad_open(0);

    while (!athena_video_is_ended(video) && !should_finish) {
        athena_pad_update(pad);

        if (athenaos_home_requested(pad))
            should_finish = true;
        else if (athena_pad_just_pressed(pad, PAD_SQUARE))
            athena_video_set_loop(video, !athena_video_get_loop(video));

        athena_screen_clear(athena_color_rgba(0, 0, 0, 128));

        // Handles frame timing and decoding.
        athena_video_update(video);
        athena_video_draw(video, 0, 0, athena_video_get_width(video), athena_video_get_height(video));

        // The chrome hides itself after a few idle seconds; any button brings it back.
        snprintf(ui.status, sizeof(ui.status), "%s", athena_video_get_loop(video) ? "Loop ON" : "");
        athenaos_auto_chrome(&ui, pad);

        athena_screen_flip();
    }

    athena_video_destroy(video);
    printf("Video playback finished!\n");
    return 0;
}

ATHENAOS_APP(video_playback, video_playback_main)
