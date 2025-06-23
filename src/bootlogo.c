#include <time.h>

#include <graphics.h>
#include <taskman.h>
#include <ath_env.h>

extern unsigned char owl_indices[];
extern unsigned int size_owl_indices;

extern unsigned char owl_palette[];
extern int size_owl_palette;

typedef enum {
    BOOT_FADE_IN,
    BOOT_WAIT,
    BOOT_FADE_OUT,
    BOOT_FINISH
} eBootState;

eBootState boot_state = BOOT_FADE_IN;

bool bootlogo_finished() {
    return boot_state == BOOT_FINISH;
}

void bootlogoThread(void* data) {
    clock_t start_time = 0;
    uint8_t logo_alpha = 0;
    GSSURFACE bootlogo = { };

   	bootlogo.Width = 128;
	bootlogo.Height = 86;
	bootlogo.PSM = GS_PSM_T8;
    bootlogo.ClutPSM = GS_PSM_CT32;

    bootlogo.Vram = NULL;
    bootlogo.VramClut = NULL;

	bootlogo.Mem = owl_indices;

    bootlogo.Clut = owl_palette;

    bootlogo.Filter = GS_FILTER_NEAREST;

    bootlogo.Delayed = true;

    athena_calculate_tbw(&bootlogo);

    while (boot_state != BOOT_FINISH) {
        clearScreen(GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00));

        switch (boot_state) {
            case BOOT_FADE_IN:
                if (logo_alpha <= 128) {
                    logo_alpha+=2;
                } else {
                    boot_state++;
                    start_time = clock();
                }

                break;

            case BOOT_WAIT:
                if (((float)(clock() - start_time) * 1000.0f / CLOCKS_PER_SEC) > 1500.0f) {
                    boot_state++;
                }
                break;

            case BOOT_FADE_OUT:
                if (logo_alpha > 0) {
                    logo_alpha-=2;
                } else {
                    boot_state++;
                }

                break;

        }

        draw_image(&bootlogo, 
                  (gsGlobal->Width/2)-(bootlogo.Width/2), 
                  (gsGlobal->Height/2)-(bootlogo.Height/2), 
                  bootlogo.Width, 
                  bootlogo.Height, 
                  0.0f, 
                  0.0f, 
                  bootlogo.Width, 
                  bootlogo.Height, 
                  GS_SETREG_RGBAQ(0x80, 0x80, 0x80, logo_alpha, 0x00));

        flipScreen();
    }

    texture_manager_free(&bootlogo);

    exitkill_task();
}

void init_bootlogo() {
    int tid = create_task("bootlogo", bootlogoThread, 4096, 0);
    init_task(tid, NULL);
    //bootlogoThread(NULL);
}