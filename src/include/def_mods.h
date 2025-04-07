#include <sifrpc.h>
#include <loadfile.h>
#include <libmc.h>
#include <iopheap.h>
#include <iopcontrol.h>
#include <smod.h>

#include <sbv_patches.h>
#include <smem.h>

#include <libpwroff.h>
#ifdef ATHENA_PADEMU
#include <libds34bt.h>
#include <libds34usb.h>
#endif
#include <pad.h>
#include <libmtap.h>

#ifdef ATHENA_AUDIO
#include <audsrv.h>
#endif

#ifdef ATHENA_NETWORK
#include <netman.h>
#include <ps2ip.h>
#endif

#include <iop_manager.h>

extern bool HDD_USABLE;

iop_manager_define_module(iomanX);
iop_manager_define_module(fileXio);
iop_manager_define_module(sio2man);
iop_manager_define_module(mcman);
iop_manager_define_module(mcserv);
iop_manager_define_module(padman);
iop_manager_define_module(mtapman);
iop_manager_define_module(mmceman);
iop_manager_define_module(cdfs);
iop_manager_define_module(usbd);
iop_manager_define_module(bdm);
iop_manager_define_module(bdmfs_fatfs);
iop_manager_define_module(usbmass_bd);

iop_manager_define_module(ps2dev9);
iop_manager_define_module(ps2atad);
iop_manager_define_module(ps2hdd);
iop_manager_define_module(ps2fs);

#ifdef ATHENA_MX4SIO
iop_manager_define_module(mx4sio_bd);
#endif

#ifdef ATHENA_ILINK
iop_manager_define_module(IEEE1394_bd);
#endif

#ifdef ATHENA_UDPBD
iop_manager_define_module(smap_udpbd);
#endif

#ifdef ATHENA_NETWORK
iop_manager_define_module(SMAP);
iop_manager_define_module(NETMAN);
#endif

#ifdef ATHENA_AUDIO
iop_manager_define_module(libsd);
iop_manager_define_module(audsrv);
#endif

#ifdef ATHENA_KEYBOARD
iop_manager_define_module(ps2kbd);
#endif

#ifdef ATHENA_MOUSE
iop_manager_define_module(ps2mouse);
#endif

#ifdef ATHENA_CAMERA
iop_manager_define_module(ps2cam);
#endif

#ifdef ATHENA_PADEMU
iop_manager_define_module(ds34bt);
iop_manager_define_module(ds34usb);
#endif

iop_manager_define_module(poweroff);
iop_manager_define_module(freeram);

void register_iop_modules();

char *get_boot_device(const char* path);

int load_default_module(int id);

bool wait_device(char *path);

void prepare_IOP();

