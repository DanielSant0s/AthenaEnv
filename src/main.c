
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

#include "ath_env.h"
#include "include/graphics.h"
#include "include/pad.h"

#include <libds34bt.h>
#include <libds34usb.h>

extern unsigned char cdfs_irx[] __attribute__((aligned(16)));
extern unsigned int size_cdfs_irx;

extern unsigned char ds34usb_irx[] __attribute__((aligned(16)));
extern unsigned int size_ds34usb_irx;

extern unsigned char ds34bt_irx[] __attribute__((aligned(16)));
extern unsigned int size_ds34bt_irx;

char boot_path[255];

void setBootPath(int argc, char ** argv, int idx)
{
    if (argc>=(idx+1))
    {

	char *p;
	if ((p = strrchr(argv[idx], '/'))!=NULL) {
	    snprintf(boot_path, sizeof(boot_path), "%s", argv[idx]);
	    p = strrchr(boot_path, '/');
	if (p!=NULL)
	    p[1]='\0';
	} else if ((p = strrchr(argv[idx], '\\'))!=NULL) {
	   snprintf(boot_path, sizeof(boot_path), "%s", argv[idx]);
	   p = strrchr(boot_path, '\\');
	   if (p!=NULL)
	     p[1]='\0';
	} else if ((p = strchr(argv[idx], ':'))!=NULL) {
	   snprintf(boot_path, sizeof(boot_path), "%s", argv[idx]);
	   p = strchr(boot_path, ':');
	   if (p!=NULL)
	   p[1]='\0';
	}

    }
    
    // check if path needs patching
    if( !strncmp( boot_path, "mass:/", 6) && (strlen (boot_path)>6))
    {
        strcpy((char *)&boot_path[5],(const char *)&boot_path[6]);
    }
         
}

int main(int argc, char **argv) {
  
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
    sbv_patch_fileio(); 

	init_fileXio_driver();
	init_memcard_driver(true);
	init_usb_driver();
	init_joystick_driver(true);
	init_audio_driver();

    SifExecModuleBuffer(&cdfs_irx, size_cdfs_irx, 0, NULL, NULL);
    int ds3pads = 1;
    SifExecModuleBuffer(&ds34usb_irx, size_ds34usb_irx, 4, (char *)&ds3pads, NULL);
    SifExecModuleBuffer(&ds34bt_irx, size_ds34bt_irx, 4, (char *)&ds3pads, NULL);
    ds34usb_init();
    ds34bt_init();

    //waitUntilDeviceIsReady by fjtrujy
    
    struct stat buffer;
    int ret = -1;
    int retries = 50;

    while(ret != 0 && retries > 0)
    {
        ret = stat("mass:/", &buffer);
        // Wait until the device is ready
        nopdelay();

        retries--;
    }
    
	setBootPath(argc, argv, 0); 
    init_taskman();
	init_graphics();
    loadFontM();

    chdir(boot_path); 

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
