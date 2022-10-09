#-------------------- Embedded IOP Modules ------------------------#
$(EE_ASM_DIR)iomanx.s: $(PS2SDK)/iop/irx/iomanX.irx | $(EE_ASM_DIR)
	echo "Embedding iomanX Driver..."
	$(BIN2S) $< $@ iomanX_irx

$(EE_ASM_DIR)filexio.s: $(PS2SDK)/iop/irx/fileXio.irx | $(EE_ASM_DIR)
	echo "Embedding fileXio Driver..."
	$(BIN2S) $< $@ fileXio_irx

$(EE_ASM_DIR)sio2man.s: $(PS2SDK)/iop/irx/sio2man.irx | $(EE_ASM_DIR)
	echo "Embedding SIO2MAN Driver..."
	$(BIN2S) $< $@ sio2man_irx
	
$(EE_ASM_DIR)mcman.s: $(PS2SDK)/iop/irx/mcman.irx | $(EE_ASM_DIR)
	echo "Embedding MCMAN Driver..."
	$(BIN2S) $< $@ mcman_irx

$(EE_ASM_DIR)mcserv.s: $(PS2SDK)/iop/irx/mcserv.irx | $(EE_ASM_DIR)
	echo "Embedding MCSERV Driver..."
	$(BIN2S) $< $@ mcserv_irx

$(EE_ASM_DIR)padman.s: $(PS2SDK)/iop/irx/padman.irx | $(EE_ASM_DIR)
	echo "Embedding PADMAN Driver..."
	$(BIN2S) $< $@ padman_irx
	
$(EE_ASM_DIR)libsd.s: $(PS2SDK)/iop/irx/libsd.irx | $(EE_ASM_DIR)
	echo "Embedding LIBSD Driver..."
	$(BIN2S) $< $@ libsd_irx

$(EE_ASM_DIR)usbd.s: $(PS2SDK)/iop/irx/usbd.irx | $(EE_ASM_DIR)
	echo "Embedding USB Driver..."
	$(BIN2S) $< $@ usbd_irx

$(EE_ASM_DIR)audsrv.s: $(PS2SDK)/iop/irx/audsrv.irx | $(EE_ASM_DIR)
	echo "Embedding AUDSRV Driver..."
	$(BIN2S) $< $@ audsrv_irx

$(EE_ASM_DIR)bdm.s: $(PS2SDK)/iop/irx/bdm.irx | $(EE_ASM_DIR)
	echo "Embedding Block Device Manager(BDM)..."
	$(BIN2S) $< $@ bdm_irx

$(EE_ASM_DIR)bdmfs_vfat.s: $(PS2SDK)/iop/irx/bdmfs_vfat.irx | $(EE_ASM_DIR)
	echo "Embedding BDM VFAT Driver..."
	$(BIN2S) $< $@ bdmfs_vfat_irx

$(EE_ASM_DIR)usbmass_bd.s: $(PS2SDK)/iop/irx/usbmass_bd.irx | $(EE_ASM_DIR)
	echo "Embedding BD USB Mass Driver..."
	$(BIN2S) $< $@ usbmass_bd_irx

$(EE_ASM_DIR)usbhdfsd.s: $(PS2SDK)/iop/irx/usbhdfsd.irx | $(EE_ASM_DIR)
	echo "Embedding USBHDFSD Driver..."
	$(BIN2S) $< $@ usbhdfsd_irx

$(EE_ASM_DIR)cdfs.s: $(PS2SDK)/iop/irx/cdfs.irx | $(EE_ASM_DIR)
	echo "Embedding CDFS Driver..."
	$(BIN2S) $< $@ cdfs_irx

modules/ds34bt/ee/libds34bt.a: modules/ds34bt/ee
	echo "Building DS3/4 Bluetooth Library..."
	$(MAKE) -C $<

modules/ds34bt/iop/ds34bt.irx: modules/ds34bt/iop
	echo "Building DS3/4 Bluetooth Driver..."
	$(MAKE) -C $<

$(EE_ASM_DIR)ds34bt.s: modules/ds34bt/iop/ds34bt.irx | $(EE_ASM_DIR)
	echo "Embedding DS3/4 Bluetooth Driver..."
	$(BIN2S) $< $@ ds34bt_irx

modules/ds34usb/ee/libds34usb.a: modules/ds34usb/ee
	echo "Building DS3/4 USB Library..."
	$(MAKE) -C $<

modules/ds34usb/iop/ds34usb.irx: modules/ds34usb/iop
	echo "Building DS3/4 USB Driver..."
	$(MAKE) -C $<

$(EE_ASM_DIR)ds34usb.s: modules/ds34usb/iop/ds34usb.irx | $(EE_ASM_DIR)
	echo "Embedding DS3/4 USB Driver..."
	$(BIN2S) $< $@ ds34usb_irx
	
#-------------------------- App Content ---------------------------#