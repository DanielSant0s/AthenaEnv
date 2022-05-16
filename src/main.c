
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

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <fileio.h>

#include <libds34bt.h>
#include <libds34usb.h>

extern unsigned char iomanX_irx[] __attribute__((aligned(16)));
extern unsigned int size_iomanX_irx;

extern unsigned char fileXio_irx[] __attribute__((aligned(16)));
extern unsigned int size_fileXio_irx;

extern unsigned char sio2man_irx[] __attribute__((aligned(16)));
extern unsigned int size_sio2man_irx;

extern unsigned char mcman_irx[] __attribute__((aligned(16)));
extern unsigned int size_mcman_irx;

extern unsigned char mcserv_irx[] __attribute__((aligned(16)));
extern unsigned int size_mcserv_irx;

extern unsigned char padman_irx[] __attribute__((aligned(16)));
extern unsigned int size_padman_irx;

extern unsigned char libsd_irx[] __attribute__((aligned(16)));
extern unsigned int size_libsd_irx;

extern unsigned char cdfs_irx[] __attribute__((aligned(16)));
extern unsigned int size_cdfs_irx;

extern unsigned char usbd_irx[] __attribute__((aligned(16)));
extern unsigned int size_usbd_irx;

extern unsigned char usbhdfsd_irx[] __attribute__((aligned(16)));
extern unsigned int size_usbhdfsd_irx;

extern unsigned char bdm_irx[] __attribute__((aligned(16)));
extern unsigned int size_bdm_irx;

extern unsigned char bdmfs_vfat_irx[] __attribute__((aligned(16)));
extern unsigned int size_bdmfs_vfat_irx;

extern unsigned char usbmass_bd_irx[] __attribute__((aligned(16)));
extern unsigned int size_usbmass_bd_irx;

extern unsigned char audsrv_irx[] __attribute__((aligned(16)));
extern unsigned int size_audsrv_irx;

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

void initMC(void)
{
   int ret;
   // mc variables
   int mc_Type, mc_Free, mc_Format;

   
   printf("initMC: Initializing Memory Card\n");

   ret = mcInit(MC_TYPE_XMC);
   
   if( ret < 0 ) {
	printf("initMC: failed to initialize memcard server.\n");
   } else {
       printf("initMC: memcard server started successfully.\n");
   }
   
   // Since this is the first call, -1 should be returned.
   // makes me sure that next ones will work !
   mcGetInfo(0, 0, &mc_Type, &mc_Free, &mc_Format); 
   mcSync(MC_WAIT, NULL, &ret);
}

int main(int argc, char **argv) {
  
    #ifdef RESET_IOP  
    printf("AthenaEnv: Starting IOP Reset...\n");
    SifInitRpc(0);
    while (!SifIopReset("", 0)){};
    while (!SifIopSync()){};
    SifInitRpc(0);
    printf("AthenaEnv: IOP reset done.\n");
    #endif
    
    // install sbv patch fix
    printf("AthenaEnv: Installing SBV Patches...\n");
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check(); 
    sbv_patch_fileio(); 

    SifExecModuleBuffer(&iomanX_irx, size_iomanX_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&fileXio_irx, size_fileXio_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&sio2man_irx, size_sio2man_irx, 0, NULL, NULL);
    fileXioInitSkipOverride();

    // load pad & mc modules 
    printf("AthenaEnv: Installing Pad & MC modules...\n");

    SifExecModuleBuffer(&mcman_irx, size_mcman_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&mcserv_irx, size_mcserv_irx, 0, NULL, NULL);
    initMC();

    // load USB modules    
    SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, NULL);
    /*SifExecModuleBuffer(&bdm_irx, size_bdm_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&bdmfs_vfat_irx, size_bdmfs_vfat_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL, NULL);*/
    SifExecModuleBuffer(&cdfs_irx, size_cdfs_irx, 0, NULL, NULL);

    SifExecModuleBuffer(&padman_irx, size_padman_irx, 0, NULL, NULL);

    int ds3pads = 1;
    SifExecModuleBuffer(&ds34usb_irx, size_ds34usb_irx, 4, (char *)&ds3pads, NULL);
    SifExecModuleBuffer(&ds34bt_irx, size_ds34bt_irx, 4, (char *)&ds3pads, NULL);
    ds34usb_init();
    ds34bt_init();

    /*
    SifExecModuleBuffer(&libsd_irx, size_libsd_irx, 0, NULL, NULL);
    SifExecModuleBuffer(&audsrv_irx, size_audsrv_irx, 0, NULL, NULL);

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

    */

    init_taskman();
	initGraphics();
	pad_init();
    

	const char* errMsg;
    

    while(true)
    {
        
        errMsg = runScript("main.js", false);

        loadFontM();

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

		unloadFontM();

    }

	// End program.
	return 0;

}
