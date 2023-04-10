
#include <kernel.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "include/def_mods.h"
#include "include/strUtils.h"

#include "ath_env.h"
#include "include/graphics.h"

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <fileio.h>

char boot_path[255];
bool dark_mode;
bool kbd_started = false;
bool mouse_started = false;
bool freeram_started = false;
bool ds34bt_started = false;
bool ds34usb_started = false;
bool network_started = false;
bool sio2man_started = false;
bool usbd_started = false;
bool usb_mass_started = false;
bool pads_started = false;
bool audio_started = false;
bool cdfs_started = false;
bool dev9_started = false;
bool mc_started = false;
bool hdd_started = false;
bool filexio_started = false;
bool HDD_USABLE = false;

void prepare_IOP() {
    dbgprintf("AthenaEnv: Starting IOP Reset...\n");
    SifInitRpc(0);
    #if defined(RESET_IOP)  
    while (!SifIopReset("", 0)){};
    #endif
    while (!SifIopSync()){};
    SifInitRpc(0);
    dbgprintf("AthenaEnv: IOP reset done.\n");
    
    // install sbv patch fix
    dbgprintf("AthenaEnv: Installing SBV Patches...\n");
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check(); 
}

static void init_drivers() {
    SifExecModuleBuffer(&poweroff_irx, size_poweroff_irx, 0, NULL, NULL);

    load_default_module(FILEXIO_MODULE);
    load_default_module(MC_MODULE);
    load_default_module(USB_MASS_MODULE);
    load_default_module(CDFS_MODULE);
    load_default_module(HDD_MODULE);
    load_default_module(PADS_MODULE);
    load_default_module(DS34BT_MODULE);
    load_default_module(DS34USB_MODULE);
    load_default_module(AUDIO_MODULE);

    // SifExecModuleBuffer(&mtapman_irx, size_mtapman_irx, 0, NULL, NULL);
    // mtapInit();

}

bool waitUntilDeviceIsReady(char *path) {
    struct stat buffer;
    int ret = -1;
    int retries = 500;

    while(ret != 0 && retries > 0) {
        ret = stat(path, &buffer);
        /* Wait untill the device is ready */
        nopdelay();

        retries--;
    }

    return ret == 0;
}

int main(int argc, char **argv) {
    char MountPoint[32+6+1]; // max partition name + 'hdd0:/' = '\0' 
    char newCWD[255];
    prepare_IOP();
    init_drivers();
    
    getcwd(boot_path, sizeof(boot_path));
    if ((!strncmp(boot_path, "hdd0:", 5)) && (strstr(boot_path, ":pfs:") != NULL) && HDD_USABLE) // we booted from HDD and our modules are loaded and running...
    {
        if (getMountInfo(boot_path, NULL, MountPoint, newCWD)) // ...if we can parse the boot path...
        {
            if (mnt(MountPoint, 0, FIO_MT_RDWR)==0) // ...mount the partition...
            {
                strcpy(boot_path, newCWD); // ...replace boot path with mounted pfs path.
#ifdef RESERVE_PFS0
                bootpath_is_on_HDD = 1;
#endif
            }

        }
    }
    waitUntilDeviceIsReady(boot_path);
    
    init_taskman();
	init_graphics();
    loadFontM();

	const char* errMsg;

    dark_mode = true;
    uint64_t color = GS_SETREG_RGBAQ(0x20,0x20,0x20,0x80,0x00);
    uint64_t color2 = GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00);

    while(true)
    {
        errMsg = runScript("main.js", false);

        gsKit_clear_screens();

        if (errMsg != NULL)
        {
            dbgprintf("AthenaEnv ERROR!\n%s", errMsg);

            if (strstr(errMsg, "EvalError") != NULL) {
                color = GS_SETREG_RGBAQ(0x56,0x71,0x7D,0x80,0x00);
            } else if (strstr(errMsg, "SyntaxError") != NULL) {
                color = GS_SETREG_RGBAQ(0x20,0x60,0xB0,0x80,0x00);
            } else if (strstr(errMsg, "TypeError") != NULL) {
                color = GS_SETREG_RGBAQ(0x3b,0x81,0x32,0x80,0x00);
            } else if (strstr(errMsg, "ReferenceError") != NULL) {
                color = GS_SETREG_RGBAQ(0xE5,0xDE,0x00,0x80,0x00);
            } else if (strstr(errMsg, "RangeError") != NULL) {
                color = GS_SETREG_RGBAQ(0xD0,0x31,0x3D,0x80,0x00);
            } else if (strstr(errMsg, "InternalError") != NULL) {
                color = GS_SETREG_RGBAQ(0x8A,0x00,0xC2,0x80,0x00);
            } else if(strstr(errMsg, "URIError") != NULL) {
                color = GS_SETREG_RGBAQ(0xFF,0x78,0x1F,0x80,0x00);
            } else if(strstr(errMsg, "AggregateError") != NULL) {
                color = GS_SETREG_RGBAQ(0xE2,0x61,0x9F,0x80,0x00);
            }

            if(dark_mode) {
                color2 = color;
                color = GS_SETREG_RGBAQ(0x20,0x20,0x20,0x80,0x00);
            } else {
                color2 = GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00);
            }

        	while (!isButtonPressed(PAD_START)) {
				clearScreen(color);
				printFontMText("AthenaEnv ERROR!", 15.0f, 15.0f, 0.9f, color2);
				printFontMText(errMsg, 15.0f, 80.0f, 0.6f, color2);
		   		printFontMText("\nPress [start] to restart\n", 15.0f, 400.0f, 0.6f, color2);
				flipScreen();
			}
        }

    }

	// End program.
	return 0;

}


int mnt(const char* path, int index, int openmod)
{
    char PFS[5+1] = "pfs0:";
    if (index > 0)
        PFS[3] = '0' + index;

    dbgprintf("Mounting '%s' into pfs%d:\n", path, index);
    if (fileXioMount(PFS, path, openmod) < 0) // mount
    {
        dbgprintf("Mount failed. unmounting & trying again...\n");
        if (fileXioUmount(PFS) < 0) //try to unmount then mount again in case it got mounted by something else
        {
            dbgprintf("Unmount failed!!!\n");
        }
        if (fileXioMount(PFS, path, openmod) < 0)
        {
            dbgprintf("mount failed again!\n");
            return -1;
        } else {
            dbgprintf("Second mount succed!\n");
        }
    } else {
        dbgprintf("mount successfull on first attemp\n");
    }
    return 0;
}