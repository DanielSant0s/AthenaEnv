
#include <kernel.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libmc.h>
#include <libcdvd.h>
#include <iopheap.h>
#include <iopcontrol.h>
#include <smod.h>
#include <audsrv.h>
#include <sys/stat.h>
#include <stdbool.h>

#include <sbv_patches.h>
#include <smem.h>

#include <ps2_all_drivers.h>
#include <libpwroff.h>

#include "ath_env.h"
#include "include/graphics.h"
#include "include/pad.h"

#include <libds34bt.h>
#include <libds34usb.h>

#include <netman.h>
#include <ps2ip.h>

extern unsigned char ds34usb_irx[] __attribute__((aligned(16)));
extern unsigned int size_ds34usb_irx;

extern unsigned char ds34bt_irx[] __attribute__((aligned(16)));
extern unsigned int size_ds34bt_irx;

extern unsigned char SMAP_irx[];
extern unsigned int size_SMAP_irx;

extern unsigned char NETMAN_irx[];
extern unsigned int size_NETMAN_irx;

char boot_path[255];

static void prepare_IOP() {
    printf("AthenaEnv: Starting IOP Reset...\n");
    SifInitRpc(0);
    #if defined(RESET_IOP)  
    while (!SifIopReset("", 0)){};
    #endif
    while (!SifIopSync()){};
    SifInitRpc(0);
    printf("AthenaEnv: IOP reset done.\n");
    
    // install sbv patch fix
    printf("AthenaEnv: Installing SBV Patches...\n");
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check(); 
}

static void deinit_drivers(bool deinit_powerOff) {
    deinit_audio_driver();
    deinit_joystick_driver(false);
    deinit_hdd_driver(false);
    deinit_cdfs_driver();
    deinit_usb_driver();
    deinit_memcard_driver(true);
    deinit_fileXio_driver();
    
    if (deinit_powerOff)
        deinit_poweroff_driver();
}

static void prepare_for_exit(bool deinit_powerOff) {
    umount_current_hdd_partition();

    deinit_drivers(deinit_powerOff);
}

static void poweroffHandler(void *arg)
{
   prepare_for_exit(false);
   poweroffShutdown();
}

static void init_drivers() {
    init_poweroff_driver();
    init_fileXio_driver();
    init_memcard_driver(true);
    init_usb_driver();
    init_cdfs_driver();
    init_hdd_driver(false, false);
    init_joystick_driver(true);
    init_audio_driver();

    poweroffSetCallback(&poweroffHandler, NULL);
    mount_current_hdd_partition();

    int ds3pads = 1;
    SifExecModuleBuffer(&ds34usb_irx, size_ds34usb_irx, 4, (char *)&ds3pads, NULL);
    SifExecModuleBuffer(&ds34bt_irx, size_ds34bt_irx, 4, (char *)&ds3pads, NULL);
    ds34usb_init();
    ds34bt_init();

    SifExecModuleBuffer(NETMAN_irx, size_NETMAN_irx, 0, NULL, NULL);
    SifExecModuleBuffer(SMAP_irx, size_SMAP_irx, 0, NULL, NULL);

    pad_init();
}

static void waitUntilDeviceIsReady(char *path)
{
    struct stat buffer;
    int ret = -1;
    int retries = 50;

    while(ret != 0 && retries > 0) {
        ret = stat(path, &buffer);
        /* Wait untill the device is ready */
        nopdelay();

        retries--;
    }
}

int main(int argc, char **argv) {
    prepare_IOP();
    init_drivers();
    
    getcwd(boot_path, sizeof(boot_path));
    waitUntilDeviceIsReady(boot_path);
    
    init_taskman();
	init_graphics();
    loadFontM();

	const char* errMsg;

    while(true)
    {
        errMsg = runScript("main.js", false);

        gsKit_clear_screens();

        if (errMsg != NULL)
        {
            printf("AthenaEnv ERROR!\n%s", errMsg);
        	while (!isButtonPressed(PAD_START)) {
				clearScreen(GS_SETREG_RGBAQ(0x20,0x60,0xB0,0x80,0x00));
				printFontMText("AthenaEnv ERROR!", 15.0f, 15.0f, 0.9f, 0x80808080);
				printFontMText(errMsg, 15.0f, 80.0f, 0.6f, 0x80808080);
		   		printFontMText("\nPress [start] to restart\n", 15.0f, 400.0f, 0.6f, 0x80808080);
				flipScreen();
			}
        }

    }

    prepare_for_exit(true);

	// End program.
	return 0;

}
