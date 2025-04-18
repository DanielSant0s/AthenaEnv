#-------------------- Embedded IOP Modules ------------------------#
$(EE_EMBED_DIR)iomanx.c: $(PS2SDK)/iop/irx/iomanX.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ iomanX_irx

$(EE_EMBED_DIR)filexio.c: $(PS2SDK)/iop/irx/fileXio.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ fileXio_irx

$(EE_EMBED_DIR)sio2man.c: $(PS2SDK)/iop/irx/sio2man.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ sio2man_irx

$(EE_EMBED_DIR)mcman.c: $(PS2SDK)/iop/irx/mcman.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ mcman_irx

$(EE_EMBED_DIR)mcserv.c: $(PS2SDK)/iop/irx/mcserv.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ mcserv_irx

$(EE_EMBED_DIR)padman.c: $(PS2SDK)/iop/irx/padman.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ padman_irx

$(EE_EMBED_DIR)mtapman.c: $(PS2SDK)/iop/irx/mtapman.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ mtapman_irx

$(EE_EMBED_DIR)libsd.c: $(PS2SDK)/iop/irx/libsd.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ libsd_irx

$(EE_EMBED_DIR)usbd.c: $(PS2SDK)/iop/irx/usbd.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ usbd_irx

$(EE_EMBED_DIR)audsrv.c: $(PS2SDK)/iop/irx/audsrv.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ audsrv_irx

$(EE_EMBED_DIR)bdm.c: $(PS2SDK)/iop/irx/bdm.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ bdm_irx

$(EE_EMBED_DIR)bdmfs_fatfs.c: $(PS2SDK)/iop/irx/bdmfs_fatfs.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ bdmfs_fatfs_irx

$(EE_EMBED_DIR)usbmass_bd.c: $(PS2SDK)/iop/irx/usbmass_bd.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ usbmass_bd_irx

$(EE_EMBED_DIR)ps2dev9.c: $(PS2SDK)/iop/irx/ps2dev9.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ ps2dev9_irx

$(EE_EMBED_DIR)ps2atad.c: $(PS2SDK)/iop/irx/ps2atad.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ ps2atad_irx

$(EE_EMBED_DIR)ps2hdd.c: $(PS2SDK)/iop/irx/ps2hdd.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ ps2hdd_irx

$(EE_EMBED_DIR)ps2fs.c: $(PS2SDK)/iop/irx/ps2fs.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ ps2fs_irx

$(EE_EMBED_DIR)cdfs.c: $(PS2SDK)/iop/irx/cdfs.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ cdfs_irx

$(EE_EMBED_DIR)poweroff.c: $(PS2SDK)/iop/irx/poweroff.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ poweroff_irx

$(EE_EMBED_DIR)mmceman.c: modules/mmceman/mmceman.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ mmceman_irx

modules/ds34bt/ee/libds34bt.a: modules/ds34bt/ee
	$(MAKE) -C $<

modules/ds34bt/iop/ds34bt.irx: modules/ds34bt/iop
	$(MAKE) -C $<

$(EE_EMBED_DIR)ds34bt.c: modules/ds34bt/iop/ds34bt.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ ds34bt_irx

modules/ds34usb/ee/libds34usb.a: modules/ds34usb/ee
	$(MAKE) -C $<

modules/ds34usb/iop/ds34usb.irx: modules/ds34usb/iop
	$(MAKE) -C $<

$(EE_EMBED_DIR)ds34usb.c: modules/ds34usb/iop/ds34usb.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ ds34usb_irx

modules/freeram/freeram.irx: modules/freeram
	$(MAKE) -C $<

$(EE_EMBED_DIR)freeram.c: modules/freeram/freeram.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ freeram_irx

$(EE_EMBED_DIR)NETMAN.c: $(PS2SDK)/iop/irx/netman.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ NETMAN_irx

$(EE_EMBED_DIR)SMAP.c: $(PS2SDK)/iop/irx/smap.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ SMAP_irx

$(EE_EMBED_DIR)ps2ips.c: $(PS2SDK)/iop/irx/ps2ips.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ ps2ips_irx

$(EE_EMBED_DIR)ps2kbd.c: $(PS2SDK)/iop/irx/ps2kbd.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ ps2kbd_irx

$(EE_EMBED_DIR)ps2mouse.c: $(PS2SDK)/iop/irx/ps2mouse.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ ps2mouse_irx

$(EE_EMBED_DIR)ps2cam.c: $(PS2SDK)/iop/irx/ps2cam.irx | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ ps2cam_irx


#--------------------- Embedded text fonts ------------------------#

$(EE_EMBED_DIR)quicksand_regular.c: assets/fonts/Quicksand-Regular.ttf | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ quicksand_regular

#-------------------------- Boot logo -----------------------------#

$(EE_EMBED_DIR)owl_indices.c: assets/bootlogo/owl_indices.raw | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ owl_indices

$(EE_EMBED_DIR)owl_palette.c: assets/bootlogo/owl_palette.raw | $(EE_EMBED_DIR)
	$(BIN2S) $< $@ owl_palette
