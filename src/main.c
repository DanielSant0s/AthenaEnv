
#include <kernel.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "include/def_mods.h"

#include "ath_env.h"
#include "include/graphics.h"

char boot_path[255];

void prepare_IOP() {
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

static void init_drivers() {
    int ds3pads = 1;

    SifExecModuleBuffer(&poweroff_irx, size_poweroff_irx, 0, NULL, NULL);

    SifExecModuleBuffer(&iomanX_irx, size_iomanX_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&fileXio_irx, size_fileXio_irx, 0, NULL, NULL);

    SifExecModuleBuffer(&sio2man_irx, size_sio2man_irx, 0, NULL, NULL);

    SifExecModuleBuffer(&mcman_irx, size_mcman_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&mcserv_irx, size_mcserv_irx, 0, NULL, NULL);

    SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&bdm_irx, size_bdm_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL, NULL);

    SifExecModuleBuffer(&cdfs_irx, size_cdfs_irx, 0, NULL, NULL);
    // SifExecModuleBuffer(&mtapman_irx, size_mtapman_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&padman_irx, size_padman_irx, 0, NULL, NULL);
    // mtapInit();

    SifExecModuleBuffer(&libsd_irx, size_libsd_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&audsrv_irx, size_audsrv_irx, 0, NULL, NULL);
    audsrv_init();

    pad_init();
    //ds34usb_init();
    //ds34bt_init();
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

	// End program.
	return 0;

}
