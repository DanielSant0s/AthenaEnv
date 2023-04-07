#include <sifrpc.h>
#include <loadfile.h>
#include <libmc.h>
#include <audsrv.h>
#include <iopheap.h>
#include <iopcontrol.h>
#include <smod.h>

#include <sbv_patches.h>
#include <smem.h>

#include <libpwroff.h>

#include <libds34bt.h>
#include <libds34usb.h>

#include "pad.h"

#include <netman.h>
#include <ps2ip.h>

#include <stdbool.h>

#define irx_define(mod)               \
    extern unsigned char mod##_irx[] __attribute__((aligned(16))); \
    extern unsigned int size_##mod##_irx

extern bool kbd_started;
extern bool mouse_started;
extern bool freeram_started;
extern bool ds34bt_started;
extern bool ds34usb_started;
extern bool network_started;
extern bool sio2man_started;
extern bool usbd_started;
extern bool usb_mass_started;
extern bool pads_started;
extern bool audio_started;
extern bool cdfs_started;
extern bool dev9_started;
extern bool mc_started;
extern bool hdd_started;
extern bool filexio_started;

#define KEYBOARD_MODULE 0
#define MOUSE_MODULE 1
#define FREERAM_MODULE 2
#define DS34BT_MODULE 3
#define DS34USB_MODULE 4
#define NETWORK_MODULE 5
#define USB_MASS_MODULE 6
#define PADS_MODULE 7
#define AUDIO_MODULE 8
#define CDFS_MODULE 9
#define MC_MODULE 10
#define HDD_MODULE 11
#define FILEXIO_MODULE 12

#define BOOT_MODULE 99

irx_define(iomanX);
irx_define(fileXio);
irx_define(sio2man);
irx_define(mcman);
irx_define(mcserv);
irx_define(padman);
irx_define(mtapman);
irx_define(libsd);
irx_define(cdfs);
irx_define(usbd);
irx_define(bdm);
irx_define(bdmfs_fatfs);
irx_define(usbmass_bd);
irx_define(ps2dev9);
irx_define(ps2atad);
irx_define(ps2hdd);
irx_define(ps2fs);
irx_define(SMAP);
irx_define(NETMAN);
irx_define(audsrv);
irx_define(ps2kbd);
irx_define(ps2mouse);
irx_define(freeram);
irx_define(ds34bt);
irx_define(ds34usb);
irx_define(poweroff);

int get_boot_device(const char* path);

int load_default_module(int id);

