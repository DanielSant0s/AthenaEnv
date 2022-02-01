
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

#include "ath_env.h"
#include "include/graphics.h"
#include "include/pad.h"

#include <libds34bt.h>
#include <libds34usb.h>

extern unsigned char sio2man_irx;
extern unsigned int size_sio2man_irx;

extern unsigned char mcman_irx;
extern unsigned int size_mcman_irx;

extern unsigned char mcserv_irx;
extern unsigned int size_mcserv_irx;

extern unsigned char padman_irx;
extern unsigned int size_padman_irx;

extern unsigned char libsd_irx;
extern unsigned int size_libsd_irx;

extern unsigned char cdfs_irx;
extern unsigned int size_cdfs_irx;

extern unsigned char usbd_irx;
extern unsigned int size_usbd_irx;

extern unsigned char bdm_irx;
extern unsigned int size_bdm_irx;

extern unsigned char bdmfs_vfat_irx;
extern unsigned int size_bdmfs_vfat_irx;

extern unsigned char usbmass_bd_irx;
extern unsigned int size_usbmass_bd_irx;

extern unsigned char audsrv_irx;
extern unsigned int size_audsrv_irx;

extern unsigned char ds34usb_irx;
extern unsigned int size_ds34usb_irx;

extern unsigned char ds34bt_irx;
extern unsigned int size_ds34bt_irx;

char boot_path[255];

void initMC(void)
{
   int ret;
   // mc variables
   int mc_Type, mc_Free, mc_Format;

   
   printf("Initializing Memory Card\n");

   ret = mcInit(MC_TYPE_MC);
   
   if( ret < 0 ) {
	printf("MC_Init : failed to initialize memcard server.\n");
   }
   
   // Since this is the first call, -1 should be returned.
   // makes me sure that next ones will work !
   mcGetInfo(0, 0, &mc_Type, &mc_Free, &mc_Format); 
   mcSync(MC_WAIT, NULL, &ret);
}

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
  
    #ifdef RESET_IOP  
    SifInitRpc(0);
    while (!SifIopReset("", 0)){};
    while (!SifIopSync()){};
    SifInitRpc(0);
    #endif
    
    // install sbv patch fix
    printf("Installing SBV Patches...\n");
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check(); 
    sbv_patch_fileio(); 

    SifExecModuleBuffer(&sio2man_irx, size_sio2man_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&mcman_irx, size_mcman_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&mcserv_irx, size_mcserv_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&padman_irx, size_padman_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&libsd_irx, size_libsd_irx, 0, NULL, NULL);

    // load pad & mc modules 
    printf("Installing Pad & MC modules...\n");

    // load USB modules    
    SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, NULL);

    int ds3pads = 1;
    SifExecModuleBuffer(&ds34usb_irx, size_ds34usb_irx, 4, (char *)&ds3pads, NULL);
    SifExecModuleBuffer(&ds34bt_irx, size_ds34bt_irx, 4, (char *)&ds3pads, NULL);
    ds34usb_init();
    ds34bt_init();

    SifExecModuleBuffer(&bdm_irx, size_bdm_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&bdmfs_vfat_irx, size_bdmfs_vfat_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&cdfs_irx, size_cdfs_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&audsrv_irx, size_audsrv_irx, 0, NULL, NULL);

    initMC();

    //waitUntilDeviceIsReady by fjtrujy

    struct stat buffer;
    int ret = -1;
    int retries = 50;

    while(ret != 0 && retries > 0)
    {
        ret = stat("mass:/", &buffer);
        /* Wait until the device is ready */
        nopdelay();

        retries--;
    }

	setBootPath(argc, argv, 0);  

	initGraphics();

	pad_init();

	const char* errMsg;

    while(true)
    {
    
        errMsg = runScript("main.js", false);

        gsKit_clear_screens();

		loadFontM();

        if (errMsg != NULL)
        {
        	while (!isButtonPressed(PAD_START)) {
				clearScreen(GS_SETREG_RGBAQ(0x20,0x60,0xB0,0x80,0x00));
				printFontMText("AthenaEnv ERROR!", 15.0f, 15.0f, 0.9f, 0x80808080);
				printFontMText(errMsg, 15.0f, 80.0f, 0.6f, 0x80808080);
		   		printFontMText("\nPress [start] to restart\n", 15.0f, 400.0f, 0.6f, 0x80808080);
				flipScreen();
			}
        }

		unloadFontM();

    }

	// End program.
	return 0;

}
