#include "include/def_mods.h"
#include <string.h>

#define started_from(device) (strstr(path, device) == path)

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

int load_default_module(int id) {
	int ds3pads = 1;

	switch (id) {
		case KEYBOARD_MODULE:
			if (!usbd_started) {
				SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, NULL);
				usbd_started = true;
			}
			if (!kbd_started) {
				SifExecModuleBuffer((void*)ps2kbd_irx, size_ps2kbd_irx, 0, NULL, NULL);
				kbd_started = true;
			}
			break;
		case MOUSE_MODULE:
			if (!usbd_started) {
				SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, NULL);
				usbd_started = true;
			}
			if (!mouse_started) {
				SifExecModuleBuffer((void*)ps2mouse_irx, size_ps2mouse_irx, 0, NULL, NULL);
				mouse_started = true;
			}
			break;
		case FREERAM_MODULE:
			if (!freeram_started) {
				SifExecModuleBuffer((void*)freeram_irx, size_freeram_irx, 0, NULL, NULL);
				freeram_started = true;
			}
			break;
		case DS34BT_MODULE:
			if (!usbd_started) {
				SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, NULL);
				usbd_started = true;
			}
			if (!ds34bt_started) {
				SifExecModuleBuffer((void*)ds34bt_irx, size_ds34bt_irx, 4, (char*)&ds3pads, NULL);
				ds34bt_init();
				ds34bt_started = true;
			}
			break;
		case DS34USB_MODULE:
			if (!usbd_started) {
				SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, NULL);
				usbd_started = true;
			}
			if (!ds34usb_started) {
				SifExecModuleBuffer((void*)ds34usb_irx, size_ds34usb_irx, 4, (char*)&ds3pads, NULL);
				ds34usb_init();
				ds34usb_started = true;
			}
			break;
		case NETWORK_MODULE:
			if (!dev9_started) {
				SifExecModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, NULL);
				dev9_started = true;
			}
			if (!network_started) {
				SifExecModuleBuffer((void*)NETMAN_irx, size_NETMAN_irx, 0, NULL, NULL);
				SifExecModuleBuffer((void*)SMAP_irx, size_SMAP_irx, 0, NULL, NULL);
				network_started = true;
			}
			break;
		case PADS_MODULE:
			if (!sio2man_started) {
				SifExecModuleBuffer(&sio2man_irx, size_sio2man_irx, 0, NULL, NULL);
				sio2man_started = true;
			}
			if (!pads_started) {
				SifExecModuleBuffer(&padman_irx, size_padman_irx, 0, NULL, NULL);
				pad_init();
				pads_started = true;
			}
			break;
		case MC_MODULE:
			if (!sio2man_started) {
				SifExecModuleBuffer(&sio2man_irx, size_sio2man_irx, 0, NULL, NULL);
				sio2man_started = true;
			}
			if (!mc_started) {
				SifExecModuleBuffer(&mcman_irx, size_mcman_irx, 0, NULL, NULL);
    			SifExecModuleBuffer(&mcserv_irx, size_mcserv_irx, 0, NULL, NULL);
				mc_started = true;
			}
			break;
		case AUDIO_MODULE:
			if (!audio_started) {
				SifExecModuleBuffer(&libsd_irx, size_libsd_irx, 0, NULL, NULL);
    			SifExecModuleBuffer(&audsrv_irx, size_audsrv_irx, 0, NULL, NULL);
    			audsrv_init();
				audio_started = true;
			}
			break;
		case USB_MASS_MODULE:
			if (!usbd_started) {
				SifExecModuleBuffer(&usbd_irx, size_usbd_irx, 0, NULL, NULL);
				usbd_started = true;
			}
			if (!usb_mass_started) {
    			SifExecModuleBuffer(&bdm_irx, size_bdm_irx, 0, NULL, NULL);
    			SifExecModuleBuffer(&bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL, NULL);
    			SifExecModuleBuffer(&usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL, NULL);

				usb_mass_started = true;
			}
			break;
		case CDFS_MODULE:
			if (!cdfs_started) {
				SifExecModuleBuffer(&cdfs_irx, size_cdfs_irx, 0, NULL, NULL);
				cdfs_started = true;
			}

			break;
		case HDD_MODULE:
			if (!dev9_started) {
				SifExecModuleBuffer(&ps2dev9_irx, size_ps2dev9_irx, 0, NULL, NULL);
				dev9_started = true;
			}
			if (!hdd_started) {
				hdd_started = true;
			}
			break;
		case FILEXIO_MODULE:
			if (!filexio_started) {
				SifExecModuleBuffer(&iomanX_irx, size_iomanX_irx, 0, NULL, NULL);
    			SifExecModuleBuffer(&fileXio_irx, size_fileXio_irx, 0, NULL, NULL);
				filexio_started = true;
			}
		default:
			break;
	}
	return 0;
}
