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

iopman_define_module(iomanX);
iopman_define_module(fileXio);
iopman_define_module(sio2man);
iopman_define_module(mcman);
iopman_define_module(mcserv);
iopman_define_module(padman);
iopman_define_module(mtapman);
iopman_define_module(mmceman);
iopman_define_module(cdfs);
iopman_define_module(usbd);
iopman_define_module(bdm);
iopman_define_module(bdmfs_fatfs);
iopman_define_module(usbmass_bd);

iopman_define_module(ps2dev9);
iopman_define_module(ps2atad);
iopman_define_module(ps2hdd);
iopman_define_module(ps2fs);

iopman_define_module(ata_bd);

#ifdef ATHENA_MX4SIO
iopman_define_module(mx4sio_bd);
#endif

#ifdef ATHENA_ILINK
iopman_define_module(IEEE1394_bd);
#endif

#ifdef ATHENA_UDPBD
iopman_define_module(smap_udpbd);
#endif

#ifdef ATHENA_NETWORK
iopman_define_module(SMAP);
iopman_define_module(NETMAN);
#endif

#ifdef ATHENA_AUDIO
iopman_define_module(libsd);
iopman_define_module(audsrv);
#endif

#ifdef ATHENA_KEYBOARD
iopman_define_module(ps2kbd);
#endif

#ifdef ATHENA_MOUSE
iopman_define_module(ps2mouse);
#endif

#ifdef ATHENA_CAMERA
iopman_define_module(ps2cam);
#endif

#ifdef ATHENA_PADEMU
iopman_define_module(ds34bt);
iopman_define_module(ds34usb);
#endif

iopman_define_module(poweroff);
iopman_define_module(freeram);

void register_iop_modules();

char *get_boot_device(const char* path);

char *get_block_device(const char* path);

int load_default_module(int id);

bool wait_device(char *path);

void prepare_IOP();

