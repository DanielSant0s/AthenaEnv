#include "include/def_mods.h"
#include "include/dbgprintf.h"
#include <string.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <fileio.h>
#include <hdd-ioctl.h>

#define started_from(device) (strstr(path, device) == path)

static const char hddarg[] = "-o" "\0" "4" "\0" "-n" "\0" "20";
static const char pfsarg[] = "-m" "\0" "4" "\0" "-o" "\0" "10" "\0" "-n" "\0" "40";

int get_boot_device(const char* path) {
	int device = -1;
	if(started_from("host")) {
		device = -1;
	} else if(started_from("mass")) {
		device = USB_MASS_MODULE;
	} else if(started_from("mc")) {
		device = MC_MODULE;
	} else if(started_from("cdfs") || started_from("cdrom")) {
		device = CDFS_MODULE;
	} else if(started_from("hdd")) {
		device = HDD_MODULE;
	}

	return device;
}

#define REPORT(MODNAME) dbgprintf(" [%s]: ret=%d, ID=%d\n", MODNAME, ret, ID)

/// @brief small macro to check if module loaded sucessfully
/// @note this checks if ret is not 1 instead of checking if it is 0. this way we implicitly support value 2 (`MODULE_REMOVABLE_END`)
#define LOAD_SUCCESS() (ID > 0 && ret != 1)


int load_default_module(int id) {
	int ds3pads = 1;
	int ID, ret;
	int HDDSTAT; // IOCTL...
	switch (id) {
		case USBD_MODULE:
		if (!usbd_started) {
				ID = SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, &ret);
				REPORT("USBD");
				usbd_started = LOAD_SUCCESS();
			}
			break;
		case KEYBOARD_MODULE:
			if (!usbd_started)
				load_default_module(USBD_MODULE);
			if (!kbd_started) {
				ID = SifExecModuleBuffer((void*)ps2kbd_irx, size_ps2kbd_irx, 0, NULL, &ret);
				REPORT("PS2KBD");
				kbd_started = LOAD_SUCCESS();
			}
			break;
		case MOUSE_MODULE:
			if (!usbd_started)
				load_default_module(USBD_MODULE);
			if (!mouse_started) {
				ID = SifExecModuleBuffer((void*)ps2mouse_irx, size_ps2mouse_irx, 0, NULL, &ret);
				REPORT("PS2MOUSE");
				mouse_started = LOAD_SUCCESS();
			}
			break;
		case FREERAM_MODULE:
			if (!freeram_started) {
				ID = SifExecModuleBuffer((void*)freeram_irx, size_freeram_irx, 0, NULL, &ret);
				// freeram_started = true; // senseless... FreeRam always returns MODULE_NO_RESIDENT_END. so MODLOAD removes it alwyays...
			}
			break;
		case DS34BT_MODULE:
			if (!usbd_started)
				load_default_module(USBD_MODULE);
			if (!ds34bt_started) {
				ID = SifExecModuleBuffer((void*)ds34bt_irx, size_ds34bt_irx, 4, (char*)&ds3pads, &ret);
				REPORT("DS34BT");
				ds34bt_init();
				ds34bt_started = LOAD_SUCCESS();
			}
			break;
		case DS34USB_MODULE:
			if (!usbd_started)
				load_default_module(USBD_MODULE);
			if (!ds34usb_started) {
				ID = SifExecModuleBuffer((void*)ds34usb_irx, size_ds34usb_irx, 4, (char*)&ds3pads, &ret);
				REPORT("DS34USB");
				ds34usb_init();
				ds34usb_started = LOAD_SUCCESS();
			}
			break;
		case NETWORK_MODULE:
			if (!dev9_started)
				load_default_module(DEV9_MODULE);
			if (!network_started) {
				ID = SifExecModuleBuffer((void*)NETMAN_irx, size_NETMAN_irx, 0, NULL, &ret);
				REPORT("NETMAN");
				ID = SifExecModuleBuffer((void*)SMAP_irx, size_SMAP_irx, 0, NULL, &ret);
				REPORT("SMAP");
				network_started = LOAD_SUCCESS();
			}
			break;
		case SIO2MAN_MODULE:
			if (!sio2man_started) {
				ID = SifExecModuleBuffer(&sio2man_irx, size_sio2man_irx, 0, NULL, &ret);
				REPORT("SIO2MAN");
				sio2man_started = LOAD_SUCCESS();
			}
		break;
		case PADS_MODULE:
			if (!sio2man_started)
				load_default_module(SIO2MAN_MODULE);

			if (!pads_started) {
				ID = SifExecModuleBuffer(&padman_irx, size_padman_irx, 0, NULL, &ret);
				REPORT("PADMAN");
				pad_init();
				pads_started = LOAD_SUCCESS();
			}
			break;
		case MC_MODULE:
			if (!sio2man_started)
				load_default_module(SIO2MAN_MODULE);
			if (!mc_started) {
				ID = SifExecModuleBuffer(&mcman_irx, size_mcman_irx, 0, NULL, &ret);
				REPORT("MCMAN");
    			ID = SifExecModuleBuffer(&mcserv_irx, size_mcserv_irx, 0, NULL, &ret);
				REPORT("MCSERV");
				mc_started = LOAD_SUCCESS();
			}
			break;
		case AUDIO_MODULE:
			if (!audio_started) {
				ID = SifExecModuleBuffer(&libsd_irx, size_libsd_irx, 0, NULL, &ret);
				REPORT("LIBSD");
    			ID = SifExecModuleBuffer(&audsrv_irx, size_audsrv_irx, 0, NULL, &ret);
				REPORT("AUDSVR");
    			audsrv_init();
				audio_started = LOAD_SUCCESS();
			}
			break;
		case USB_MASS_MODULE:
			if (!usbd_started)
				load_default_module(USBD_MODULE);
			if (!usb_mass_started) {
    			ID = SifExecModuleBuffer(&bdm_irx, size_bdm_irx, 0, NULL, &ret);
				REPORT("BDM");
    			ID = SifExecModuleBuffer(&bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL, &ret);
				REPORT("BDMFS_FATFS");
    			ID = SifExecModuleBuffer(&usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL, &ret);
				REPORT("USMASS_BD");

				usb_mass_started = LOAD_SUCCESS();
			}
			break;
		case CDFS_MODULE:
			if (!cdfs_started) {
				ID = SifExecModuleBuffer(&cdfs_irx, size_cdfs_irx, 0, NULL, &ret);
				REPORT("CDFS");
				cdfs_started = LOAD_SUCCESS();
			}

			break;
		case DEV9_MODULE:
			if (!dev9_started) {
				ID = SifExecModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &ret);
				REPORT("DEV9");
				dev9_started = LOAD_SUCCESS();
			}
		break;
		case HDD_MODULE:
			if (!filexio_started)
				load_default_module(FILEXIO_MODULE);
			if (!dev9_started)
				load_default_module(DEV9_MODULE);
			if ((!hdd_started) && filexio_started) {

    			ID = SifExecModuleBuffer(&ps2atad_irx, size_ps2atad_irx, 0, NULL, &ret);
				REPORT("ATAD");

    			ID = SifExecModuleBuffer(&ps2hdd_irx, size_ps2hdd_irx, sizeof(hddarg), hddarg, &ret);
				REPORT("PS2HDD");

    			HDDSTAT = fileXioDevctl("hdd0:", HDIOC_STATUS, NULL, 0, NULL, 0); /* 0 = HDD connected and formatted, 1 = not formatted, 2 = HDD not usable, 3 = HDD not connected. */
				dbgprintf("%s: HDD status is %d\n", __func__, HDDSTAT); 
    			HDD_USABLE = (HDDSTAT == 0 || HDDSTAT == 1); // ONLY if HDD is usable. as we will offer HDD Formatting operation

    			if (HDD_USABLE)
    			{
    			    ID = SifExecModuleBuffer(&ps2fs_irx, size_ps2fs_irx, sizeof(pfsarg), pfsarg,  &ret);
    			    REPORT("PS2FS");
					hdd_started = LOAD_SUCCESS();
    			} else {
					hdd_started = false;
				}
				
			}
			break;
		case FILEXIO_MODULE:
			if (!filexio_started) {
				ID = SifExecModuleBuffer(&iomanX_irx, size_iomanX_irx, 0, NULL, &ret);
				REPORT("IOMANX");
    			ID = SifExecModuleBuffer(&fileXio_irx, size_fileXio_irx, 0, NULL, &ret);
				REPORT("FILEXIO");
				filexio_started = LOAD_SUCCESS();
			}
		default:
			break;
	}
	return 0;
}
