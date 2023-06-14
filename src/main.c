
#include <kernel.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "include/memory.h"

#include "include/def_mods.h"
#include "include/strUtils.h"

#include "include/taskman.h"

#include "ath_env.h"

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <fileio.h>

#ifdef ATHENA_GRAPHICS
#include "include/graphics.h"
#include "include/fntsys.h"
#endif

#ifdef ATHENA_CLI
#include <debug.h>

void athena_error_screen(const char* errMsg, bool dark_mode) {
    uint32_t color = 0x000000;
    uint32_t color2 = 0xFFFFFF;

    if (errMsg != NULL)
    {
        dbgprintf("AthenaEnv ERROR!\n%s", errMsg);

        if (strstr(errMsg, "EvalError") != NULL) {
            color = 0x7D7156;
        } else if (strstr(errMsg, "SyntaxError") != NULL) {
            color = 0xB06020;
        } else if (strstr(errMsg, "TypeError") != NULL) {
            color = 0x32813b;
        } else if (strstr(errMsg, "ReferenceError") != NULL) {
            color = 0x00DEE5; 
        } else if (strstr(errMsg, "RangeError") != NULL) {
            color = 0x3D31D0; 
        } else if (strstr(errMsg, "InternalError") != NULL) {
            color = 0xC2008A;
        } else if(strstr(errMsg, "URIError") != NULL) {
            color = 0x1F78FF;
        } else if(strstr(errMsg, "AggregateError") != NULL) {
            color = 0x9F61E2; 
        }

        if(dark_mode) {
            color2 = color;
            color = 0x000000;
        } else {
            color2 = 0xFFFFFF;
        }

        scr_clear();

        scr_setbgcolor(color);
        scr_setCursor(0);
        scr_setfontcolor(color2);

		scr_printf("AthenaEnv ERROR!\n");
		scr_printf("%s\n", errMsg);
	   	scr_printf("\n\nPress [start] to restart");

    	while (!isButtonPressed(PAD_START)) {
            nopdelay();
		} 
    }
}
#endif

char boot_path[255];
bool dark_mode;

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

int main(int argc, char **argv) {
    init_memory_manager();

    char MountPoint[32+6+1]; // max partition name + 'hdd0:/' = '\0' 
    char newCWD[255];
    dbginit(); // if we are using serial port. initialize it here before the fun starts
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
                chdir(newCWD);
#ifdef RESERVE_PFS0
                bootpath_is_on_HDD = 1;
#endif
            }

        }
    }
    waitUntilDeviceIsReady(boot_path);

    #ifdef ATHENA_GRAPHICS
	init_graphics();
    fntInit();
    loadFontM();
    #else
    #ifdef ATHENA_CLI
    init_scr();
    #endif
    #endif

    init_taskman();

	const char* errMsg = NULL;

    dark_mode = true;

    do
    {
        if (argc < 2) {
            errMsg = runScript("main.js", false);
        } else {
            errMsg = runScript(argv[1], false);
        }

        athena_error_screen(errMsg, dark_mode);

    } while (errMsg != NULL);

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
            dbgprintf("Second mount succeed!\n");
        }
    } else {
        dbgprintf("mount successfull on first attempt\n");
    }
    return 0;
}