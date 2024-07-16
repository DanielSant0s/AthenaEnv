
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

char boot_path[255];
bool dark_mode;

static void init_drivers() {
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

void init_base_fs_drivers() {
    SifExecModuleBuffer(&poweroff_irx, size_poweroff_irx, 0, NULL, NULL);

    load_default_module(FILEXIO_MODULE);
    load_default_module(get_boot_device(boot_path));
}

int main(int argc, char **argv) {
    char MountPoint[32+6+1]; // max partition name + 'hdd0:/' = '\0' 
    char newCWD[255];

    init_memory_manager();
    init_taskman();

    getcwd(boot_path, sizeof(boot_path));

    dbginit(); // if we are using serial port. initialize it here before the fun starts

    #ifdef ATHENA_GRAPHICS
	init_graphics();
    #endif

    init_bootlogo();

    prepare_IOP();

    init_base_fs_drivers();

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

    wait_device(boot_path);

    init_drivers();

    #ifdef ATHENA_GRAPHICS
    fntInit();
    #endif

	const char* err_msg = NULL;

    dark_mode = true;

    do
    {
        if (argc < 2) {
            err_msg = run_script("main.js", false);
        } else {
            err_msg = run_script(argv[1], false);
        }

        athena_error_screen(err_msg, dark_mode);

    } while (err_msg != NULL);

	// End program.
	return 0;

}


