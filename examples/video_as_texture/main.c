/*
 * Texture video, in C: the port of main.js. The current video frame as an
 * image: drawn rotating and scaling in the middle, and as four thumbnails.
 */

#include <stdio.h>

#include <athena/athena.h>
#include "athenaos/app.h"

static int video_as_texture_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Texture video", "media", NULL, 0);

    AthenaVideo *video = athena_video_create("video.m2v");
    if (!video)
        return 0;
    athena_video_set_loop(video, true);
    athena_video_play(video);

    const int vw = athena_video_get_width(video), vh = athena_video_get_height(video);
    printf("Video as Texture Demo\n");
    printf("  Resolution: %dx%d\n", vw, vh);

    float angle = 0, scale = 1.0f, scale_dir = 0.01f;

    // The frame as an image referencing the video's texture: no copy.
    AthenaImage *frame = NULL;
    GSSURFACE *texture = athena_video_get_texture(video);
    if (texture) {
        frame = athena_image_wrap(texture, false);
        frame->width = frame->endx = vw;
        frame->height = frame->endy = vh;
    }

    athena_screen_set_param(DEPTH_TEST_ENABLE, false);
    AthenaPad *pad = athena_pad_open(0);

    for (;;) {
        athena_pad_update(pad);
        if (athenaos_home_requested(pad))
            break;

        athenaos_background(&ui);

        athena_video_update(video);

        if (frame) {
            // Main video in the center
            const float center_x = 320 - (vw * scale) / 2, center_y = 224 - (vh * scale) / 2;
            frame->width = vw * scale;
            frame->height = vh * scale;
            frame->angle = angle;
            athena_image_draw(frame, center_x, center_y);

            // Smaller thumbnails in the corners
            frame->width = 80;
            frame->height = 60;
            frame->angle = 0;
            athena_image_draw(frame, 10, 10);
            athena_image_draw(frame, 550, 10);
            athena_image_draw(frame, 10, 380);
            athena_image_draw(frame, 550, 380);
        }

        // Animate rotation and scale
        angle += 0.01f;
        if (angle >= 360) angle = 0;
        scale += scale_dir;
        if (scale > 1.5f || scale < 0.5f)
            scale_dir = -scale_dir;

        athenaos_auto_chrome(&ui, pad);
        athena_screen_flip();
    }

    athena_video_destroy(video);
    printf("Demo finished!\n");
    return 0;
}

ATHENAOS_APP(video_as_texture, video_as_texture_main)
