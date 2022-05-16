.SILENT:

define HEADER

:=.                                                      .=:
 :#+.                                                  .+%- 
  :#%+                                                =%#:  
   .#%#-                                            -#%#.   
     *%%#:                                        :#%%*     
      #%%%*.                                    .+%%%#      
      -%%%%%+.                                 +%%%%%-      
      :%%%%%%%+.                            .+%%%%%%%:      
      -%%%%%%%%%*=.                      .=*%%%%%%%%%=      
      #%%#*++*#%%%%#=.                .=#%%%%#*++*#%%#      
     -%*:       .-+%%%*:            :*%%%*-.       .*%=     
    :%+        ..   :+#%+.         +%#+:   ..        +%-    
    ##          .--    :+#:      :#*:    --.          #%    
   -%+            .+-    .=-    -=.    -+.            +%-   
   -%=      .:---==-**:              .**-==---:.      =%-   
   .%=   .:-%=   %%%%%#-            -#%%%%%   =%-:.   =%:   
    *#      =*   :*##+. ::        .: .+##*:   *=      #*    
    .#-      =*.       :=          =:       .*=      -#.    
      +-       -++=====.            .=====++-       -+      
       :-.                                        .-:            

                    AthenaEnv project                                                               
                                                                                
endef
export HEADER

EE_BIN = athena.elf
EE_BIN_PKD = athena_pkd.elf

RESET_IOP = 1

EE_LIBS = -L$(PS2SDK)/ports/lib -L$(PS2DEV)/gsKit/lib/ -Lmodules/ds34bt/ee/ -Lmodules/ds34usb/ee/ -lpatches -lfileXio -lcdvd -lpad -ldebug -lmath3d -ljpeg -lfreetype -lgskit_toolkit -lgskit -ldmakit -lpng -lz -lmc -laudsrv -lelf-loader -lds34bt -lds34usb

EE_INCS += -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include -I$(PS2SDK)/ports/include/freetype2 -I$(PS2SDK)/ports/include/zlib

EE_INCS += -Imodules/ds34bt/ee -Imodules/ds34usb/ee

EE_CFLAGS += -Wno-sign-compare -fno-strict-aliasing -fno-exceptions -D_R5900

ifeq ($(RESET_IOP),1)
EE_CFLAGS += -DRESET_IOP
endif

ifeq ($(DEBUG),1)
EE_CFLAGS += -DDEBUG
endif

BIN2S = $(PS2SDK)/bin/bin2s

EXT_LIBS = modules/ds34usb/ee/libds34usb.a modules/ds34bt/ee/libds34bt.a

APP_CORE = src/main.o src/taskman.o src/pad.o src/graphics.o src/atlas.o src/fntsys.o src/sound.o \
		   src/system.o src/render.o src/calc_3d.o src/gsKit3d_sup.o

ATHENA_MODULES = src/duktape/duktape.o src/duktape/duk_console.o src/duktape/duk_module_node.o \
				 src/ath_env.o src/ath_screen.o src/ath_graphics.o src/ath_pads.o src/ath_sound.o \
				 src/ath_system.o src/ath_timer.o src/ath_render.o src/ath_task.o

IOP_MODULES = src/iomanx.o src/filexio.o src/sio2man.o src/mcman.o src/mcserv.o src/padman.o src/libsd.o  \
			  src/usbd.o src/audsrv.o src/bdm.o src/bdmfs_vfat.o src/usbmass_bd.o src/cdfs.o src/ds34bt.o \
			  src/ds34usb.o src/usbhdfsd.o

EE_OBJS = $(IOP_MODULES) $(APP_CORE) $(ATHENA_MODULES)

#-------------------- Embedded IOP Modules ------------------------#
src/iomanx.s: $(PS2SDK)/iop/irx/iomanX.irx
	echo "Embedding iomanX Driver..."
	$(BIN2S) $< $@ iomanX_irx

src/filexio.s: $(PS2SDK)/iop/irx/fileXio.irx
	echo "Embedding fileXio Driver..."
	$(BIN2S) $< $@ fileXio_irx

src/sio2man.s: $(PS2SDK)/iop/irx/sio2man.irx
	echo "Embedding SIO2MAN Driver..."
	$(BIN2S) $< $@ sio2man_irx
	
src/mcman.s: $(PS2SDK)/iop/irx/mcman.irx
	echo "Embedding MCMAN Driver..."
	$(BIN2S) $< $@ mcman_irx

src/mcserv.s: $(PS2SDK)/iop/irx/mcserv.irx
	echo "Embedding MCSERV Driver..."
	$(BIN2S) $< $@ mcserv_irx

src/padman.s: $(PS2SDK)/iop/irx/padman.irx
	echo "Embedding PADMAN Driver..."
	$(BIN2S) $< $@ padman_irx
	
src/libsd.s: $(PS2SDK)/iop/irx/libsd.irx
	echo "Embedding LIBSD Driver..."
	$(BIN2S) $< $@ libsd_irx

src/usbd.s: $(PS2SDK)/iop/irx/usbd.irx
	echo "Embedding USB Driver..."
	$(BIN2S) $< $@ usbd_irx

src/audsrv.s: $(PS2SDK)/iop/irx/audsrv.irx
	echo "Embedding AUDSRV Driver..."
	$(BIN2S) $< $@ audsrv_irx

src/bdm.s: $(PS2SDK)/iop/irx/bdm.irx
	echo "Embedding Block Device Manager(BDM)..."
	$(BIN2S) $< $@ bdm_irx

src/bdmfs_vfat.s: $(PS2SDK)/iop/irx/bdmfs_vfat.irx
	echo "Embedding BDM VFAT Driver..."
	$(BIN2S) $< $@ bdmfs_vfat_irx

src/usbmass_bd.s: $(PS2SDK)/iop/irx/usbmass_bd.irx
	echo "Embedding BD USB Mass Driver..."
	$(BIN2S) $< $@ usbmass_bd_irx

src/usbhdfsd.s: $(PS2SDK)/iop/irx/usbhdfsd.irx
	echo "Embedding USBHDFSD Driver..."
	$(BIN2S) $< $@ usbhdfsd_irx

src/cdfs.s: $(PS2SDK)/iop/irx/cdfs.irx
	echo "Embedding CDFS Driver..."
	$(BIN2S) $< $@ cdfs_irx

modules/ds34bt/ee/libds34bt.a: modules/ds34bt/ee
	echo "Building DS3/4 Bluetooth Library..."
	$(MAKE) -C $<

modules/ds34bt/iop/ds34bt.irx: modules/ds34bt/iop
	echo "Building DS3/4 Bluetooth Driver..."
	$(MAKE) -C $<

src/ds34bt.s: modules/ds34bt/iop/ds34bt.irx
	echo "Embedding DS3/4 Bluetooth Driver..."
	$(BIN2S) $< $@ ds34bt_irx

modules/ds34usb/ee/libds34usb.a: modules/ds34usb/ee
	echo "Building DS3/4 USB Library..."
	$(MAKE) -C $<

modules/ds34usb/iop/ds34usb.irx: modules/ds34usb/iop
	echo "Building DS3/4 USB Driver..."
	$(MAKE) -C $<

src/ds34usb.s: modules/ds34usb/iop/ds34usb.irx
	echo "Embedding DS3/4 USB Driver..."
	$(BIN2S) $< $@ ds34usb_irx
	
#-------------------------- App Content ---------------------------#

all: $(EXT_LIBS) $(EE_BIN)
	@echo "$$HEADER"

	echo "Building $(EE_BIN)..."
	$(EE_STRIP) $(EE_BIN)

	echo "Compressing $(EE_BIN_PKD)...\n"
	ps2-packer $(EE_BIN) $(EE_BIN_PKD) > /dev/null
	
	mv $(EE_BIN) bin/
	mv $(EE_BIN_PKD) bin/

debug: $(EE_BIN)
	echo "Building $(EE_BIN) with debug symbols..."

clean:
	echo "\nCleaning $(EE_BIN)..."
	rm -f bin/$(EE_BIN)

	echo "\nCleaning $(EE_BIN_PKD)..."
	rm -f bin/$(EE_BIN_PKD)

	echo "\nCleaning objects..."
	rm -f $(EE_OBJS)

	echo "Cleaning iomanX Driver..."
	rm -f src/iomanx.s

	echo "Cleaning fileXio Driver..."
	rm -f src/filexio.s

	echo "Cleaning USBHDFSD Driver..."
	rm -f src/usbhdfsd.s
		
	echo "Cleaning SIO2MAN Driver..."
	rm -f src/sio2man.s

	echo "Cleaning MCMAN Driver..."
	rm -f src/mcman.s

	echo "Cleaning MCSERV Driver..."
	rm -f src/mcserv.s

	echo "Cleaning PADMAN Driver..."
	rm -f src/padman.s

	echo "Cleaning LIBSD Driver..."
	rm -f src/libsd.s

	echo "Cleaning Block Device Manager(BDM)..."
	rm -f src/bdm.s
	
	echo "Cleaning USB Driver..."
	rm -f src/usbd.s
	
	echo "Embedding AUDSRV Driver..."
	rm -f src/audsrv.s
	
	echo "Cleaning BDM VFAT Driver..."
	rm -f src/bdmfs_vfat.s
	
	echo "Embedding BD USB Mass Driver..."
	rm -f src/usbmass_bd.s
	
	echo "Cleaning CDFS Driver..."
	rm -f src/cdfs.s

	echo "Cleaning DS3/4 Drivers..."
	rm -f src/ds34bt.s
	rm -f src/ds34usb.s
	$(MAKE) -C modules/ds34usb clean
	$(MAKE) -C modules/ds34bt clean

rebuild: clean all

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
