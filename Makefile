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

RESET_IOP ?= 1
BDM ?= 0

EE_LIBS = -L$(PS2SDK)/ports/lib -L$(PS2DEV)/gsKit/lib/ -Lmodules/ds34bt/ee/ -Lmodules/ds34usb/ee/ -lpatches -lfileXio -lcdvd -lpad -ldebug -lmath3d -ljpeg -lfreetype -lgskit_toolkit -lgskit -ldmakit -lpng -lz -lmc -laudsrv -lelf-loader -lds34bt -lds34usb

EE_INCS += -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include -I$(PS2SDK)/ports/include/freetype2 -I$(PS2SDK)/ports/include/zlib

EE_INCS += -Imodules/ds34bt/ee -Imodules/ds34usb/ee

EE_CFLAGS += -Wno-sign-compare -fno-strict-aliasing -fno-exceptions -D_R5900

ifeq ($(RESET_IOP),1)
EE_CFLAGS += -DRESET_IOP
endif

ifeq ($(BDM),1)
EE_CFLAGS += -DBDM
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


all: $(EXT_LIBS) $(EE_BIN)
	@echo "$$HEADER"

	echo "Building $(EE_BIN)..."
	$(EE_STRIP) $(EE_BIN)

	echo "Compressing $(EE_BIN_PKD)...\n"
	ps2-packer $(EE_BIN) $(EE_BIN_PKD) > /dev/null
	
	mv $(EE_BIN) bin/
	mv $(EE_BIN_PKD) bin/

debug: $(EXT_LIBS) $(EE_BIN)
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
	
	echo "Cleaning AUDSRV Driver..."
	rm -f src/audsrv.s
	
	echo "Cleaning BDM VFAT Driver..."
	rm -f src/bdmfs_vfat.s
	
	echo "Cleaning BD USB Mass Driver..."
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
include embeds.make
