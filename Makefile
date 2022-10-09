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

EE_SRC_DIR = src/
EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/

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

APP_CORE = main.o taskman.o pad.o graphics.o atlas.o fntsys.o sound.o \
		   system.o render.o calc_3d.o gsKit3d_sup.o

ATHENA_MODULES = duktape/duktape.o duktape/duk_console.o duktape/duk_module_node.o \
				 ath_env.o ath_screen.o ath_graphics.o ath_pads.o ath_sound.o \
				 ath_system.o ath_timer.o ath_render.o ath_task.o

IOP_MODULES = iomanx.o filexio.o sio2man.o mcman.o mcserv.o padman.o libsd.o  \
			  usbd.o audsrv.o bdm.o bdmfs_vfat.o usbmass_bd.o cdfs.o ds34bt.o \
			  ds34usb.o usbhdfsd.o

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
	rm -fr $(EE_OBJS_DIR) $(EE_ASM_DIR)

	echo "Cleaning DS3/4 Drivers..."
	rm -f src/ds34bt.s
	rm -f src/ds34usb.s
	$(MAKE) -C modules/ds34usb clean
	$(MAKE) -C modules/ds34bt clean

rebuild: clean all

$(EE_ASM_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR):
	@mkdir -p $@
	@mkdir -p $@duktape

EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	@echo -$<
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o: $(EE_ASM_DIR)%.s | $(EE_OBJS_DIR)
	@echo -$<
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
include embeds.make
