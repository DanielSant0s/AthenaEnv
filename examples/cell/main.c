/*
 * Cell.io, in C: the port of main.js. Eat the smaller cells, avoid the bigger
 * ones. Menus use the standard chrome; gameplay owns the screen.
 */

#include <math.h>
#include <stdio.h>

#include <athena/athena.h>
#include "athenaos/app.h"

#define RGB(r, g, b) athena_color_rgba((r), (g), (b), 128)

static const char *const MENU[] = { "Play", "Options", "Quit" };
static const AthenaOSHint MENU_HINTS[] = { { "D-pad", "Select" }, { "cross", "Confirm" } };
static const AthenaOSHint OVER_HINTS[] = { { "cross", "Menu" } };

enum { MAIN_MENU, RUN_GAME, PAUSE_MENU, GAME_OVER };

typedef struct {
    float x, y, r;
    Color color;
} Cell;

#define MAX_ENEMIES 16

static bool circle_circle_coll(const Cell *c1, const Cell *c2) {
    const float dx = c1->x - c2->x, dy = c1->y - c2->y;
    return sqrtf(dx * dx + dy * dy) <= c1->r + c2->r;
}

static void draw_cell(float x, float y, float r, Color color, bool hollow, Color white) {
    athena_draw_circle(x, y, r, color, true);
    if (hollow)
        athena_draw_circle(x, y, r, white, false);
}

static int cell_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Cell.io", "games", NULL, 0);

    athena_screen_set_frame_counter(true);
    athena_screen_set_vsync(false);

    const Color transparent = athena_color_rgba(255, 255, 255, 40);
    const Color purple = RGB(64, 0, 128);
    const Color white = RGB(255, 255, 255);

    AthenaFont *font = athena_font_load("fonts/LEMONMILK-Regular.otf");
    font->scale = 2;

    int game_state = MAIN_MENU;
    Cell player = { 0, 0, 25, RGB(128, 100, 255) };
    float camera_x = -320, camera_y = -224;
    Cell enemies[MAX_ENEMIES];
    int enemy_count = 0;
    int main_menu_ptr = 0;

    AthenaPad *pad = athena_pad_open(0);

    for (;;) {
        athena_screen_clear(purple);
        athena_pad_update(pad);

        if (athenaos_home_requested(pad))
            break;

        if (game_state == MAIN_MENU) {
            if (athena_pad_just_pressed(pad, PAD_UP) && main_menu_ptr > 0)
                main_menu_ptr--;
            if (athena_pad_just_pressed(pad, PAD_DOWN) && main_menu_ptr < 2)
                main_menu_ptr++;

            if (athena_pad_just_pressed(pad, PAD_CROSS)) {
                if (main_menu_ptr == 0) {
                    game_state = RUN_GAME;
                    player.x = 0;
                    player.y = 0;
                    player.r = 25;
                    camera_x = -320;
                    camera_y = -224;

                    const int enemies_qt = athena_randomi(8, 15);
                    while (enemies_qt > enemy_count) {
                        Cell enemy = {
                            athena_randomi(0, 640), athena_randomi(0, 448), athena_randomi(5, 75),
                            RGB(athena_randomi(0, 255), athena_randomi(0, 255), athena_randomi(0, 255)),
                        };
                        bool found_collision = false;
                        for (int j = 0; j < enemy_count; j++)
                            if (circle_circle_coll(&enemy, &enemies[j]))
                                found_collision = true;
                        if (!found_collision && !circle_circle_coll(&enemy, &player))
                            enemies[enemy_count++] = enemy;
                    }
                    font->scale = 1;
                } else if (main_menu_ptr == 2) {
                    break;
                }
            }

            if (game_state == MAIN_MENU) {
                athenaos_background(&ui);
                const float x = ui.content.x, y = ui.content.y;

                font->scale = 1.6f;
                font->color = player.color;
                athena_font_print(font, x, y + 10, "Cell.io");

                for (int i = 0; i < 3; i++) {
                    const float cy = y + 130 + i * 44;
                    if (i == main_menu_ptr) {
                        athena_draw_rect(x, cy - 18, 260, 36, athenaos_rgba(255, 255, 255, 0.10f));
                        athena_draw_rect(x, cy - 18, 3, 36, athenaos_accent(1.0f));
                    }
                    athenaos_text_v(&ui, x + 18, cy, MENU[i], 0.7f,
                                    i == main_menu_ptr ? athenaos_rgba(255, 255, 255, 1.0f) : athenaos_rgba(255, 255, 255, 0.62f));
                }

                athenaos_chrome_hints(&ui, MENU_HINTS, 2);
            }
        } else if (game_state == RUN_GAME) {
            for (int i = -1500; i < 1501; i += 50) {
                float sx = i - camera_x, sy = -1500 - camera_y;
                if (sx > 0 && sx < 640)
                    athena_draw_line(sx, sy, i - camera_x, 1500 - camera_y, transparent);
                sx = -1500 - camera_x;
                sy = i - camera_y;
                if (sy > 0 && sy < 448)
                    athena_draw_line(sx, sy, 1500 - camera_x, i - camera_y, transparent);
            }

            if (athena_pad_pressed(pad, PAD_LEFT) && player.x > -1500) { player.x -= 4; camera_x -= 4; }
            if (athena_pad_pressed(pad, PAD_RIGHT) && player.x < 1500) { player.x += 4; camera_x += 4; }
            if (athena_pad_pressed(pad, PAD_UP) && player.y > -1500) { player.y -= 4; camera_y -= 4; }
            if (athena_pad_pressed(pad, PAD_DOWN) && player.y < 1500) { player.y += 4; camera_y += 4; }

            for (int i = 0; i < enemy_count; i++) {
                const float ex = enemies[i].x - camera_x, ey = enemies[i].y - camera_y, r = enemies[i].r;
                if (ex + r > 0 && ex - r < 640 && ey + r > 0 && ey - r < 448) {
                    draw_cell(ex, ey, r, enemies[i].color, false, white);
                    if (circle_circle_coll(&enemies[i], &player)) {
                        if (enemies[i].r < player.r) {
                            player.r += enemies[i].r / 2;
                            for (int j = i; j < enemy_count - 1; j++)
                                enemies[j] = enemies[j + 1];
                            enemy_count--;
                            if (!enemy_count) {
                                game_state = GAME_OVER;
                                font->scale = 2;
                            }
                        } else if (enemies[i].r > player.r) {
                            game_state = GAME_OVER;
                            font->scale = 2;
                        }
                    }
                }
            }

            draw_cell(player.x - camera_x, player.y - camera_y, player.r, player.color, true, white);

            char l0[32], l1[48];
            snprintf(l0, sizeof(l0), "%.1f FPS", athena_screen_get_fps(360));
            snprintf(l1, sizeof(l1), "Size %d  |  %d cells left", (int)roundf(player.r), enemy_count);
            const char *lines[] = { l0, l1 };
            athenaos_hud(&ui, lines, 2, 0);
        } else if (game_state == GAME_OVER) {
            font->scale = 2;
            font->color = RGB(128, 128, 128);
            font->align = ALIGN_HCENTER;
            athena_font_print(font, ui.W / 2, 190, !enemy_count ? "VICTORY!" : "GAME OVER");
            font->align = ALIGN_NONE;
            athenaos_chrome_hints(&ui, OVER_HINTS, 1);

            if (athena_pad_just_pressed(pad, PAD_CROSS)) {
                game_state = MAIN_MENU;
                enemy_count = 0;
            }
        }

        athena_screen_flip();
    }
    return 0;
}

ATHENAOS_APP(cell, cell_main)
