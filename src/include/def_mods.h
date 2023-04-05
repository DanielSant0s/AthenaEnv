#include <sifrpc.h>
#include <loadfile.h>
#include <libmc.h>
#include <audsrv.h>
#include <iopheap.h>
#include <iopcontrol.h>
#include <smod.h>

#include <sbv_patches.h>
#include <smem.h>

#include <ps2_all_drivers.h>
#include <libpwroff.h>

#include <libds34bt.h>
#include <libds34usb.h>

#include "pad.h"

#include <netman.h>
#include <ps2ip.h>

#define IRX_DEFINE(mod)               \
    extern unsigned char mod##_irx[] __attribute__((aligned(16))); \
    extern unsigned int size_##mod##_irx

IRX_DEFINE(iomanX);
IRX_DEFINE(fileXio);
IRX_DEFINE(sio2man);
IRX_DEFINE(mcman);
IRX_DEFINE(mcserv);
IRX_DEFINE(padman);
IRX_DEFINE(mtapman);
IRX_DEFINE(libsd);
IRX_DEFINE(cdfs);
IRX_DEFINE(usbd);
IRX_DEFINE(bdm);
IRX_DEFINE(bdmfs_fatfs);
IRX_DEFINE(usbmass_bd);
IRX_DEFINE(ps2dev9);
IRX_DEFINE(ps2atad);
IRX_DEFINE(ps2hdd);
IRX_DEFINE(ps2fs);
IRX_DEFINE(SMAP);
IRX_DEFINE(NETMAN);
IRX_DEFINE(audsrv);
IRX_DEFINE(ps2kbd);
IRX_DEFINE(ps2mouse);
IRX_DEFINE(freeram);
IRX_DEFINE(ds34bt);
IRX_DEFINE(ds34usb);
IRX_DEFINE(poweroff);

