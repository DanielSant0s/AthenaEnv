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

EE_SRC_DIR = src/
EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/

RESET_IOP ?= 1
DEBUG ?= 0
EE_SIO ?= 0

CLI ?= 0
GRAPHICS ?= 1
AUDIO ?= 1
NETWORK ?= 1
KEYBOARD ?= 1
MOUSE ?= 1

EE_LIBS = -L$(PS2SDK)/ports/lib -L$(PS2DEV)/gsKit/lib/ -Lmodules/ds34bt/ee/ -Lmodules/ds34usb/ee/ -lmc -lpad -laudsrv -lpatches -ldebug -lmath3d -ljpeg -lfreetype -lgskit_toolkit -lgskit -ldmakit -lpng -lz -lelf-loader -lds34bt -lds34usb -lnetman -lps2ip -lcurl -lwolfssl -lkbd -lmouse -lvorbisfile -lvorbis -logg -llzma -lzip -lfileXio

EE_INCS += -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include -I$(PS2SDK)/ports/include/freetype2 -I$(PS2SDK)/ports/include/zlib

EE_INCS += -Imodules/ds34bt/ee -Imodules/ds34usb/ee

EE_CFLAGS += -Wno-sign-compare -fno-strict-aliasing -fno-exceptions -DCONFIG_VERSION=\"$(shell cat VERSION)\" -D__TM_GMTOFF=tm_gmtoff -DPATH_MAX=256 -DEMSCRIPTEN

ifeq ($(RESET_IOP),1)
  EE_CFLAGS += -DRESET_IOP
endif

ifeq ($(DEBUG),1)
  EE_CFLAGS += -DDEBUG
endif

BIN2S = $(PS2SDK)/bin/bin2s

EXT_LIBS = modules/ds34usb/ee/libds34usb.a modules/ds34bt/ee/libds34bt.a

JS_CORE = quickjs/cutils.o quickjs/libbf.o quickjs/libregexp.o quickjs/libunicode.o \
				 quickjs/realpath.o quickjs/quickjs.o quickjs/quickjs-libc.o 

APP_CORE = main.o module_system.o taskman.o pad.o system.o strUtils.o

ATHENA_MODULES = ath_env.o ath_pads.o ath_system.o ath_archive.o ath_timer.o ath_task.o 

IOP_MODULES = iomanx.o filexio.o sio2man.o mcman.o mcserv.o padman.o  \
			  usbd.o bdm.o bdmfs_fatfs.o usbmass_bd.o cdfs.o ds34bt.o \
			  ds34usb.o freeram.o ps2dev9.o mtapman.o poweroff.o ps2atad.o \
			  ps2hdd.o ps2fs.o

ifeq ($(CLI),1)
  EE_CFLAGS += -DATHENA_CLI
  ATHENA_MODULES += ath_cli.o
  GRAPHICS = 0
endif

ifeq ($(GRAPHICS),1)
  EE_CFLAGS += -DATHENA_GRAPHICS
  APP_CORE += graphics.o atlas.o fntsys.o render.o calc_3d.o 
  ATHENA_MODULES += ath_color.o ath_font.o ath_render.o ath_screen.o ath_image.o ath_imagelist.o ath_shape.o 
endif

ifeq ($(AUDIO),1)
  EE_CFLAGS += -DATHENA_AUDIO
  APP_CORE += sound.o audsrv.o 
  ATHENA_MODULES += ath_sound.o 
  IOP_MODULES += libsd.o 
endif

ifeq ($(NETWORK),1)
  EE_CFLAGS += -DATHENA_NETWORK
  APP_CORE += network.o
  ATHENA_MODULES += ath_network.o ath_socket.o
  IOP_MODULES += NETMAN.o SMAP.o ps2ips.o
endif

ifeq ($(KEYBOARD),1)
  EE_CFLAGS += -DATHENA_KEYBOARD
  ATHENA_MODULES += ath_keyboard.o 
  IOP_MODULES += ps2kbd.o  
endif

ifeq ($(MOUSE),1)
  EE_CFLAGS += -DATHENA_MOUSE
  ATHENA_MODULES += ath_mouse.o
  IOP_MODULES += ps2mouse.o 
endif

ifneq ($(EE_SIO), 0)
  EE_CFLAGS += -D__EESIO_PRINTF
  EE_LIBS += -lsiocookie
endif


EE_OBJS = $(APP_CORE) $(JS_CORE) $(ATHENA_MODULES) $(IOP_MODULES) #group them all
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%) #prepend the object folder


#-------------------------- App Content ---------------------------#

all: $(EXT_LIBS) $(EE_BIN) $(EE_ASM_DIR) $(EE_OBJS_DIR)
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
	echo Cleaning executables...
	rm -f bin/$(EE_BIN) bin/$(EE_BIN_PKD)
	rm -rf $(EE_OBJS_DIR)
	rm -rf $(EE_ASM_DIR)
	$(MAKE) -C modules/ds34usb clean
	$(MAKE) -C modules/ds34bt clean

rebuild: clean all

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
include embed.make

$(EE_ASM_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	@echo CC - $<
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o: $(EE_ASM_DIR)%.s | $(EE_OBJS_DIR)
	@echo AS - $<
	$(DIR_GUARD)
	$(EE_AS) $(EE_ASFLAGS) $< -o $@
