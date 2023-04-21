
#include <kernel.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "include/def_mods.h"
#include "include/strUtils.h"

#include "ath_env.h"

#ifdef ATHENA_GRAPHICS
#include "include/graphics.h"
#endif

#ifdef ATHENA_CLI
#include <debug.h>

void athena_error_screen(const char* errMsg, bool dark_mode) {
    uint32_t color = 0xFF000000;
    uint32_t color2 = 0xFFFFFFFF;

    if (errMsg != NULL)
    {
        dbgprintf("AthenaEnv ERROR!\n%s", errMsg);

        /*if (strstr(errMsg, "EvalError") != NULL) {
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
            color = 0xFF000000;
        } else {
            color2 = 0xFFFFFFFF;
        }*/

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
bool camera_started = false;
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
    dbgprintf("waiting for '%s'\n", path);
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

static bool sema_started = false;

static s32 malloc_semaid;
static ee_sema_t malloc_sema;

static int malloc_locked = 0;
static int malloc_lock_count = 0;

#include <reent.h>

extern void __malloc_lock(struct _reent *r) {
    if(!sema_started) {
        malloc_sema.init_count = 1;
        malloc_sema.max_count = 1;
        malloc_sema.option = 0;
        malloc_semaid = CreateSema(&malloc_sema);
        sema_started = true;
    }
    if (!malloc_locked) {
        WaitSema(malloc_semaid);
        malloc_locked = 1;
    }
    malloc_lock_count++;
}

extern void __malloc_unlock(struct _reent *r) {
    malloc_lock_count--;
    if (malloc_lock_count == 0) {
        malloc_locked = 0;
        SignalSema(malloc_semaid);
    }
}


int main(int argc, char **argv) {
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
    loadFontM();
    #else
    #ifdef ATHENA_CLI
    init_scr();
    #endif
    #endif

    init_taskman();

	const char* errMsg;

    dark_mode = true;

    while(true)
    {
        if (argc < 2) {
            errMsg = runScript("main.js", false);
        } else {
            errMsg = runScript(argv[1], false);
        }

        athena_error_screen(errMsg, dark_mode);

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