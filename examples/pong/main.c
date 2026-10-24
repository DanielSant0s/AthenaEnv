/*
 * Pong, in C: the port of main.js. The classic, with ADPCM sound effects.
 * Gameplay owns the screen; the standard chrome shows while paused.
 */

#include <stdio.h>
#include <stdlib.h>

#include <athena/athena.h>
#include "athenaos/app.h"

static const AthenaOSHint HINTS[] = { { "START", "Play/Pause" }, { "D-pad", "Move" } };

#define RGB(r, g, b) athena_color_rgba((r), (g), (b), 128)

#define DIRECTION_UP -1
#define DIRECTION_DOWN 1
#define DIRECTION_LEFT -1
#define DIRECTION_RIGHT 1

typedef struct {
    float x, y, w, h, speed;
    float bak_x, bak_y, bak_speed;
} Paddle;

typedef struct {
    float size, x, y, w, h, speed;
    float bak_x, bak_y, bak_speed;
    int v_direction, h_direction;
} Ball;

static int screen_w, screen_h;

static bool rect_rect(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2) {
    return x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && h1 + y1 > y2;
}

static void ball_random_direction(Ball *b) {
    b->v_direction = rand() % 2 ? -1 : 1;
    b->h_direction = rand() % 2 ? -1 : 1;
}

static void ball_reset(Ball *b) {
    b->x = b->bak_x;
    b->y = b->bak_y;
    b->speed = b->bak_speed;
    ball_random_direction(b);
}

static void ball_move(Ball *b) {
    if (b->x >= screen_w - b->size)
        b->h_direction = DIRECTION_LEFT;
    else if (b->x <= 0)
        b->h_direction = DIRECTION_RIGHT;
    if (b->y <= 0)
        b->v_direction = DIRECTION_DOWN;
    b->x += b->h_direction * b->speed;
    b->y += b->v_direction * b->speed;
}

static bool ball_collide(Ball *b, const Paddle *p) {
    const bool col = rect_rect(b->x, b->y, b->w, b->h, p->x, p->y, p->w, p->h);
    if (col)
        b->v_direction = DIRECTION_UP;
    return col;
}

static void paddle_reset(Paddle *p) {
    p->x = p->bak_x;
    p->y = p->bak_y;
    p->speed = p->bak_speed;
}

static int pong_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Pong", "games", HINTS, 2);

    athena_sound_set_volume(100);
    const int channel = athena_sound_find_channel();
    AthenaSfx *snd_unpause = athena_sfx_load("pong/unpause.adp");
    AthenaSfx *snd_score = athena_sfx_load("pong/score.adp");
    AthenaSfx *snd_over = athena_sfx_load("pong/over.adp");

    AthenaVideoMode mode;
    athena_screen_get_mode(&mode);
    screen_w = mode.width;
    screen_h = mode.height;

    const Color dark_gray = RGB(32, 32, 32), gray = RGB(64, 64, 64), white = RGB(255, 255, 255);

    athena_screen_set_param(DEPTH_TEST_ENABLE, false);

    AthenaFont *font = athena_font_load("fonts/minecraft.ttf");
    font->color = gray;
    AthenaFont *bigfont = athena_font_load("fonts/minecraft.ttf");
    bigfont->color = white;
    bigfont->scale = 2.0f;

    Paddle player = { .x = 285.0f, .y = 400.0f, .w = 70, .h = 10, .speed = 4 };
    player.bak_x = player.x; player.bak_y = player.y; player.bak_speed = player.speed;

    Ball ball = { .size = 16 };
    ball.x = 320 - ball.size / 2;
    ball.y = 224 - ball.size / 2;
    ball.w = ball.h = ball.size;
    ball.speed = 2;
    ball.bak_x = ball.x; ball.bak_y = ball.y; ball.bak_speed = ball.speed;
    ball_random_direction(&ball);

    AthenaPad *pad = athena_pad_open(0);

    bool paused = true, game_over = false;
    int score = 0, score_threshold = 5;

    const char *over_text = "GAME OVER", *pause_text = "PRESS START";
    const Coords over_size = athena_font_get_text_size(bigfont, over_text);
    const Coords pause_size = athena_font_get_text_size(bigfont, pause_text);
    const float over_w = over_size.width, over_h = over_size.height * bigfont->scale;
    const float pause_w = pause_size.width, pause_h = pause_size.height * bigfont->scale;

    for (;;) {
        athena_pad_update(pad);
        if (athenaos_home_requested(pad))
            break;

        athena_screen_clear(dark_gray);

        if (athena_pad_just_pressed(pad, PAD_START)) {
            paused = !paused;
            if (game_over) {
                ball_reset(&ball);
                paddle_reset(&player);
                score = 0;
                score_threshold = 5;
            }
            game_over = false;
            athena_sfx_play(snd_unpause, channel);
        }

        if (!paused) {
            if ((athena_pad_pressed(pad, PAD_LEFT) || pad->rx < -50) && player.x > 0)
                player.x -= player.speed;
            if ((athena_pad_pressed(pad, PAD_RIGHT) || pad->rx > 50) && player.x < screen_w - player.w)
                player.x += player.speed;

            if (ball_collide(&ball, &player)) {
                score++;
                athena_sfx_play(snd_score, channel);
                if (score > score_threshold) {
                    score_threshold *= 2;
                    ball.speed++;
                    player.speed++;
                }
            }

            if (ball.y >= screen_h) {
                game_over = true;
                paused = true;
                athena_sfx_play(snd_over, channel);
            }

            ball_move(&ball);
        }

        if (game_over)
            athena_font_print(bigfont, 320 - over_w / 2, 224 - over_h / 2, over_text);
        else if (paused)
            athena_font_print(bigfont, 320 - pause_w / 2, 224 - pause_h / 2, pause_text);

        char score_text[16];
        snprintf(score_text, sizeof(score_text), "%d", score);
        athena_font_print(font, 600 - athena_font_get_text_size(font, score_text).width, 10, score_text);
        athena_draw_rect(player.x, player.y, player.w, player.h, white);
        athena_draw_rect(ball.x, ball.y, ball.size, ball.size, white);

        if (paused)
            athenaos_chrome(&ui);

        athena_screen_flip();
    }
    return 0;
}

ATHENAOS_APP(pong, pong_main)
