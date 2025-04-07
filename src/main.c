
#include <kernel.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>

#include <memory.h>

#include <def_mods.h>
#include <strUtils.h>

#include <taskman.h>

#include <ath_env.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <fileio.h>
#include <ps2sdkapi.h>

#ifdef ATHENA_GRAPHICS
#include <graphics.h>
#include <fntsys.h>
#endif

#include <readini.h>

#include <erl.h>

#include <iop_manager.h>

const char* module_ini_entries[] = {
    "keyboard",
    "mouse",
    "ds34bt",
    "ds34usb",
    "network",
    "usb_mass",
    "pads",
    "audio",
    "mmce",
    "cdfs",
    "mc",
    "hdd",
    "mx4sio",
    "ieee1394",
    "udpbd",
};

bool module_ini_values[] = {
    false, // kbd 
    false, // mouse
    false, // ds34bt
    false, // ds34usb
    false, // network
    true,  // usb_mass
    true , // pads
    true , // audio
    false, // mmceman
    false, // cdfs
    false, // mc
    false, // hdd
    false, // mx4sio
    false,  // ieee1394
    false  // udpbd
};

enum module_masks {
    index_kbd,
    index_mouse,
    index_ds34bt,
    index_ds34usb,
    index_network,
    index_usb_mass,
    index_pads,
    index_audio,
    index_mmceman,
    index_cdfs,
    index_mc,
    index_hdd,
    index_mx4sio,
    index_ieee1394,
    index_udpbd,
};

char path_workbuffer[255] = { 0 };

char boot_path[255] = { 0 };
char default_script[128] = "main.js";
char default_cfg[128] = "athena.ini";
bool dark_mode, boot_logo;

static __attribute__((used)) void *bypass_modulated_libs() {
    int func = NULL;
    func |= (int)_ps2sdk_ioctl;

    return func;
}

static void init_drivers() {
    int ds34pads = 1;

    register_iop_modules();

    if (module_ini_values[index_mc])
        iop_manager_load_module(iop_manager_search_module("mcserv"), 0, NULL);
    if (module_ini_values[index_mmceman])
        iop_manager_load_module(iop_manager_search_module("mmceman"), 0, NULL);
    if (module_ini_values[index_cdfs])
        iop_manager_load_module(iop_manager_search_module("cdfs"), 0, NULL);
    if (module_ini_values[index_hdd])
        iop_manager_load_module(iop_manager_search_module("ps2fs"), 0, NULL);
    if (module_ini_values[index_usb_mass])
        iop_manager_load_module(iop_manager_search_module("usbmass_bd"), 0, NULL);
    if (module_ini_values[index_pads])
        iop_manager_load_module(iop_manager_search_module("padman"), 0, NULL);
    if (module_ini_values[index_ds34bt])
        iop_manager_load_module(iop_manager_search_module("ds34bt"), 4, (char*)&ds34pads);
    if (module_ini_values[index_ds34usb])
        iop_manager_load_module(iop_manager_search_module("ds34usb"), 4, (char*)&ds34pads);
    if (module_ini_values[index_audio])
        iop_manager_load_module(iop_manager_search_module("audsrv"), 0, NULL);
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

void set_default_script(const char* path) {
    strcpy(default_script, path);
    default_script[strlen(path)] = '\0';
}


extern struct export_list_t {
    char * name;
    void * pointer;
} export_list[];

static char * prohibit_list[] = {
    "_edata", "_end_bss", "_fbss", "_fdata", "_fini",
    "_ftext", "_init", "main", //"_gp", "_end", 
    0
};

static void export_symbols() {
    struct export_list_t * p;
    int i, prohibit;
    
    for (p = export_list; p->name; p++) {
	prohibit = 0;
	for (i = 0; prohibit_list[i]; i++) {
	    if (!(strcmp(prohibit_list[i], p->name))) {
		prohibit = 1;
		break;
	    }
	}
	if (!prohibit)
	    erl_add_global_symbol(p->name, p->pointer);
    }
}

typedef void (*func_t)(void);

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

    if (!strncmp(argv[0], "cdrom0", 6))
        chdir("cdfs:/");

    getcwd(boot_path, sizeof(boot_path));

    dbginit(); // if we are using serial port. initialize it here before the fun starts

    dbgprintf("boot path: %s\n", boot_path);

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
        if (!strncmp(boot_path, "cdfs", 4) && !strpre("cdrom0", default_cfg)) {
            memset(default_cfg, 0, 128);
            strcpy(default_cfg, "cdrom0:ATHENA.INI;1"); // if using cdrom and custom arg is not valid, try to read from cdrom root.
        }
            
        if (readini_open(&ini, default_cfg)) {
            while(readini_getline(&ini)) {
                if (readini_emptyline(&ini)) {
                    continue;
                } if (readini_bool(&ini, "boot_logo", &boot_logo)) {
                    dbgprintf("reading boot_logo at athena.ini\n");

                } else if (readini_bool(&ini, "dark_mode", &dark_mode)) {
                    dbgprintf("reading dark_mode at athena.ini\n");

                } else if (readini_string(&ini, "default_script", default_script)) {
                    dbgprintf("reading default_script at athena.ini\n");

                } else {
                    for (int i = 0; i < sizeof(module_ini_entries)/sizeof(char*); i++) {
                        if (readini_bool(&ini, module_ini_entries[i], &module_ini_values[i])) {
                            printf("reading %s at athena.ini\n", module_ini_entries[i]);
                        }
                    }
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

    export_symbols();

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


