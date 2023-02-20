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

EE_LIBS = -L$(PS2SDK)/ports/lib -L$(PS2DEV)/gsKit/lib/ -Lmodules/ds34bt/ee/ -Lmodules/ds34usb/ee/ -lps2_drivers -lmc -lpatches -ldebug -lmath3d -ljpeg -lfreetype -lgskit_toolkit -lgskit -ldmakit -lpng -lz -lelf-loader -lds34bt -lds34usb -lnetman -lps2ip -lcurl -lwolfssl -lkbd -lmouse -lvorbisfile -lvorbis -logg -llzma -lzip

EE_INCS += -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include -I$(PS2SDK)/ports/include/freetype2 -I$(PS2SDK)/ports/include/zlib

EE_INCS += -Imodules/ds34bt/ee -Imodules/ds34usb/ee

EE_CFLAGS += -Wno-sign-compare -fno-strict-aliasing -fno-exceptions -DPS2IP_DNS -DCONFIG_VERSION=\"$(shell cat VERSION)\" -D__TM_GMTOFF=tm_gmtoff -DPATH_MAX=256 -DEMSCRIPTEN

ifeq ($(RESET_IOP),1)
EE_CFLAGS += -DRESET_IOP
endif

ifeq ($(DEBUG),1)
EE_CFLAGS += -DDEBUG
endif

BIN2S = $(PS2SDK)/bin/bin2s

EXT_LIBS = modules/ds34usb/ee/libds34usb.a modules/ds34bt/ee/libds34bt.a

APP_CORE = src/main.o src/taskman.o src/pad.o src/graphics.o src/atlas.o src/fntsys.o src/sound.o \
		   src/system.o src/render.o src/calc_3d.o

ATHENA_MODULES = src/quickjs/cutils.o src/quickjs/libbf.o src/quickjs/libregexp.o src/quickjs/libunicode.o \
				 src/quickjs/realpath.o src/quickjs/quickjs.o src/quickjs/quickjs-libc.o \
				 src/ath_env.o src/ath_screen.o src/ath_image.o src/ath_imagelist.o src/ath_shape.o src/ath_mouse.o\
				 src/ath_color.o src/ath_font.o src/ath_pads.o src/ath_keyboard.o src/ath_sound.o src/ath_system.o \
				 src/ath_archive.o src/ath_timer.o src/ath_render.o src/ath_task.o src/ath_network.o src/ath_socket.o

IOP_MODULES = src/ds34bt.o src/ds34usb.o src/NETMAN.o src/SMAP.o src/ps2kbd.o src/ps2mouse.o

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
	mv $(EE_BIN) bin/athena_debug.elf

clean:
	echo "\nCleaning $(EE_BIN)..."
	rm -f bin/$(EE_BIN)

	echo "\nCleaning $(EE_BIN_PKD)..."
	rm -f bin/$(EE_BIN_PKD)

	echo "\nCleaning objects..."
	rm -f $(EE_OBJS)

	echo "Cleaning DS3/4 Drivers..."
	rm -f src/ds34bt.s
	rm -f src/ds34usb.s
	$(MAKE) -C modules/ds34usb clean
	$(MAKE) -C modules/ds34bt clean

	echo "Cleaning Network Driver..."
	rm -f src/NETMAN.s
	rm -f src/SMAP.s
	rm -f src/ps2kbd.s
	rm -f src/ps2mouse.s

rebuild: clean all

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
include embeds.make
