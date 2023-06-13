#-------------------- Embedded IOP Modules ------------------------#
$(EE_ASM_DIR)iomanx.s: $(PS2SDK)/iop/irx/iomanX.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ iomanX_irx

$(EE_ASM_DIR)filexio.s: $(PS2SDK)/iop/irx/fileXio.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ fileXio_irx

$(EE_ASM_DIR)sio2man.s: $(PS2SDK)/iop/irx/sio2man.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ sio2man_irx

$(EE_ASM_DIR)mcman.s: $(PS2SDK)/iop/irx/mcman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mcman_irx

$(EE_ASM_DIR)mcserv.s: $(PS2SDK)/iop/irx/mcserv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mcserv_irx

$(EE_ASM_DIR)padman.s: $(PS2SDK)/iop/irx/padman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ padman_irx

$(EE_ASM_DIR)mtapman.s: $(PS2SDK)/iop/irx/mtapman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mtapman_irx

$(EE_ASM_DIR)libsd.s: $(PS2SDK)/iop/irx/libsd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ libsd_irx

$(EE_ASM_DIR)usbd.s: $(PS2SDK)/iop/irx/usbd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbd_irx

$(EE_ASM_DIR)audsrv.s: $(PS2SDK)/iop/irx/audsrv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ audsrv_irx

$(EE_ASM_DIR)bdm.s: $(PS2SDK)/iop/irx/bdm.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ bdm_irx

$(EE_ASM_DIR)bdmfs_fatfs.s: $(PS2SDK)/iop/irx/bdmfs_fatfs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ bdmfs_fatfs_irx

$(EE_ASM_DIR)usbmass_bd.s: $(PS2SDK)/iop/irx/usbmass_bd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbmass_bd_irx
	
$(EE_ASM_DIR)ps2dev9.s: $(PS2SDK)/iop/irx/ps2dev9.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2dev9_irx

$(EE_ASM_DIR)ps2atad.s: $(PS2SDK)/iop/irx/ps2atad.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2atad_irx

$(EE_ASM_DIR)ps2hdd.s: $(PS2SDK)/iop/irx/ps2hdd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2hdd_irx

$(EE_ASM_DIR)ps2fs.s: $(PS2SDK)/iop/irx/ps2fs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2fs_irx

$(EE_ASM_DIR)cdfs.s: $(PS2SDK)/iop/irx/cdfs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ cdfs_irx

$(EE_ASM_DIR)poweroff.s: $(PS2SDK)/iop/irx/poweroff.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ poweroff_irx

modules/ds34bt/ee/libds34bt.a: modules/ds34bt/ee
	$(MAKE) -C $<

modules/ds34bt/iop/ds34bt.irx: modules/ds34bt/iop
	$(MAKE) -C $<

$(EE_ASM_DIR)ds34bt.s: modules/ds34bt/iop/ds34bt.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ds34bt_irx

modules/ds34usb/ee/libds34usb.a: modules/ds34usb/ee
	$(MAKE) -C $<

modules/ds34usb/iop/ds34usb.irx: modules/ds34usb/iop
	$(MAKE) -C $<

$(EE_ASM_DIR)ds34usb.s: modules/ds34usb/iop/ds34usb.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ds34usb_irx

modules/freeram/freeram.irx: modules/freeram
	$(MAKE) -C $<

$(EE_ASM_DIR)freeram.s: modules/freeram/freeram.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ freeram_irx

$(EE_ASM_DIR)NETMAN.s: $(PS2SDK)/iop/irx/netman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ NETMAN_irx

$(EE_ASM_DIR)SMAP.s: $(PS2SDK)/iop/irx/smap.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ SMAP_irx

$(EE_ASM_DIR)ps2ips.s: $(PS2SDK)/iop/irx/ps2ips.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2ips_irx

$(EE_ASM_DIR)ps2kbd.s: $(PS2SDK)/iop/irx/ps2kbd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2kbd_irx

$(EE_ASM_DIR)ps2mouse.s: $(PS2SDK)/iop/irx/ps2mouse.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2mouse_irx

$(EE_ASM_DIR)ps2cam.s: $(PS2SDK)/iop/irx/ps2cam.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2cam_irx


#--------------------- Embedded text fonts ------------------------#

$(EE_ASM_DIR)quicksand_regular.s: assets/fonts/Quicksand-Regular.ttf | $(EE_ASM_DIR)
	$(BIN2S) $< $@ quicksand_regular