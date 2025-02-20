
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

#include <readini.h>

char boot_path[255];
char default_script[128] = "main.js";
char default_cfg[128] = "athena.ini";
bool dark_mode, boot_logo;

static void init_drivers() {
    load_default_module(MC_MODULE);
    load_default_module(MMCEMAN_MODULE);
    load_default_module(CDFS_MODULE);
    load_default_module(HDD_MODULE);
    load_default_module(USB_MASS_MODULE);
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

void set_default_script(const char* path) {
    strcpy(default_script, path);
    default_script[strlen(path)] = '\0';
}

int main(int argc, char **argv) {
    IniReader ini;
    boot_logo = true;
    dark_mode = true;

    bool ignore_ini = false;
    bool reset_iop = true;
#if defined(RESET_IOP)
    reset_iop = RESET_IOP;
#endif

    char MountPoint[32+6+1]; // max partition name + 'hdd0:/' + '\0'
    char newCWD[255];

    init_memory_manager();
    init_taskman();

    getcwd(boot_path, sizeof(boot_path));

    dbginit(); // if we are using serial port. initialize it here before the fun starts

    if (argc > 1) {
        char* arg = NULL;
        char* tmp_arg = NULL;
        for (int i = 1, arg = argv[1]; i < argc; i++, arg = argv[i]) {
            if ((tmp_arg = strpre("--script=", arg)) || (tmp_arg = strpre("-s=", arg))) {
                set_default_script(tmp_arg);
                tmp_arg = NULL;
            } else if (!strcmp("--nologo", arg) || !strcmp("-l", arg)) {
                boot_logo = false;
            } else if (!strcmp("--ignorecfg", arg) || !strcmp("-i", arg)) {
                ignore_ini = true;
            } else if (!strcmp("--lightmode", arg) || !strcmp("-w", arg)) {
                dark_mode = false;
            } else if (!strcmp("--noiopreset", arg) || !strcmp("-n", arg)) {
                reset_iop = false;
            } else if ((tmp_arg = strpre("--cfg=", arg)) || (tmp_arg = strpre("-c=", arg))) {
                strcpy(default_cfg, tmp_arg);
                default_cfg[strlen(tmp_arg)] = '\0';
                tmp_arg = NULL;
            }
        }
    }

    #ifdef ATHENA_GRAPHICS
	init_graphics();
    #endif

    if (!ignore_ini) {
        if (readini_open(&ini, default_cfg)) {
            while(readini_getline(&ini)) {
                if (readini_bool(&ini, "boot_logo", &boot_logo)) {
                    dbgprintf("reading boot_logo at athena.ini\n");

                } else if (readini_bool(&ini, "dark_mode", &dark_mode)) {
                    dbgprintf("reading dark_mode at athena.ini\n");

                } else if (readini_string(&ini, "default_script", default_script)) {
                    dbgprintf("reading default_script at athena.ini\n");

                }
            }

            readini_close(&ini);
        }
    }

    if (boot_logo) {
        init_bootlogo();
    }

    if (reset_iop) {
        prepare_IOP();

        init_drivers();

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
    }

    #ifdef ATHENA_GRAPHICS
    fntInit();
    #endif

	const char* err_msg = NULL;

    int jump_ret = setjmp(*get_reset_buf());

    do
    {
        err_msg = run_script(default_script, false);

        athena_error_screen(err_msg, dark_mode);

    } while (err_msg != NULL);

	// End program.
	return 0;

}


