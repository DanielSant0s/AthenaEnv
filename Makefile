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

EE_EXT = .elf

EE_BIN = athena
EE_BIN_PKD = athena_pkd

EE_SRC_DIR = src/
EE_OBJS_DIR = obj/
EE_EMBED_DIR = embed/

RESET_IOP ?= 1
DEBUG ?= 0
EE_SIO ?= 0

PADEMU ?= 1
GRAPHICS ?= 1
AUDIO ?= 1
KEYBOARD ?= 1
MOUSE ?= 1
NETWORK ?= 1
CAMERA ?= 0

EE_LIBS = -L$(PS2SDK)/ports/lib -lmc -lpad -lpatches -ldebug -lz -llzma -lzip -lfileXio -lelf-loader-nocolour -lerl

EE_INCS += -I$(PS2SDK)/ports/include -I$(PS2SDK)/ports/include/zlib -Isrc/readini/include

EE_CFLAGS += -Wno-sign-compare -fno-strict-aliasing -fno-exceptions -fpermissive -DCONFIG_VERSION=\"$(shell cat VERSION)\" -D__TM_GMTOFF=tm_gmtoff -DPATH_MAX=256 -DPS2
ifeq ($(RESET_IOP),1)
  EE_CFLAGS += -DRESET_IOP
endif

ifeq ($(DEBUG),1)
  EE_CFLAGS += -DDEBUG
endif

BIN2S = $(PS2SDK)/bin/bin2c
EE_DVP = dvp-as
EE_VCL = vcl
EE_VCLPP = vclpp

JS_CORE = quickjs/cutils.o quickjs/libbf.o quickjs/libregexp.o quickjs/libunicode.o \
				 quickjs/realpath.o quickjs/quickjs.o quickjs/quickjs-libc.o

VU1_MPGS = draw_3D_pvc.o \
           draw_3D_colors.o draw_3D_colors_scissor.o \
           draw_3D_lights.o draw_3D_lights_scissor.o \
           draw_3D_spec.o draw_3D_spec_scissor.o

APP_CORE = main.o bootlogo.o vif.o athena_math.o memory.o ee_tools.o module_system.o taskman.o pad.o system.o strUtils.o

INI_READER = readini/src/readini.o

ATHENA_MODULES = ath_env.o ath_physics.o ath_vector.o ath_pads.o ath_system.o ath_archive.o ath_timer.o ath_task.o

IOP_MODULES = iomanx.o filexio.o sio2man.o mcman.o mcserv.o padman.o  \
			  usbd.o bdm.o bdmfs_fatfs.o usbmass_bd.o cdfs.o \
			  freeram.o ps2dev9.o mtapman.o poweroff.o ps2atad.o \
			  ps2hdd.o ps2fs.o mmceman.o

EMBEDDED_ASSETS = quicksand_regular.o owl_indices.o owl_palette.o

EE_OBJS = $(APP_CORE) $(INI_READER) $(JS_CORE) $(ATHENA_MODULES) $(IOP_MODULES) $(EMBEDDED_ASSETS) # group them all

ifeq ($(GRAPHICS),1)
  EE_LIBS += -L$(PS2DEV)/gsKit/lib/ -lmath3d -ljpeg -lfreetype -lgskit_toolkit -lgskit -ldmakit -lpng
  EE_INCS += -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include/freetype2
  EE_CFLAGS += -DATHENA_GRAPHICS
  APP_CORE += graphics.o atlas.o fntsys.o render.o camera.o calc_3d.o fast_obj/fast_obj.o

  ATHENA_MODULES += ath_color.o ath_font.o ath_render.o ath_lights.o ath_3dcamera.o ath_screen.o ath_image.o ath_imagelist.o ath_shape.o
  EE_OBJS += $(VU1_MPGS)
endif

ifeq ($(PADEMU),1)
  EE_CFLAGS += -DATHENA_PADEMU
  EE_INCS += -Imodules/ds34bt/ee -Imodules/ds34usb/ee
  EE_LIBS += -Lmodules/ds34bt/ee/ -Lmodules/ds34usb/ee/ -lds34bt -lds34usb
  IOP_MODULES += ds34usb.o ds34bt.o
	EXT_LIBS = modules/ds34usb/ee/libds34usb.a modules/ds34bt/ee/libds34bt.a
endif

ifeq ($(AUDIO),1)
  EE_CFLAGS += -DATHENA_AUDIO
  APP_CORE += sound.o audsrv.o
  ATHENA_MODULES += ath_sound.o
  IOP_MODULES += libsd.o

  EE_LIBS += -laudsrv -lvorbisfile -lvorbis -logg
endif

ifeq ($(NETWORK),1)
  EE_CFLAGS += -DATHENA_NETWORK
  APP_CORE += network.o
  ATHENA_MODULES += ath_network.o ath_socket.o
  IOP_MODULES += NETMAN.o SMAP.o ps2ips.o
  EE_LIBS += -lnetman -lps2ip -lcurl -lwolfssl
endif

ifeq ($(KEYBOARD),1)
  EE_CFLAGS += -DATHENA_KEYBOARD
  ATHENA_MODULES += ath_keyboard.o
  IOP_MODULES += ps2kbd.o

  EE_LIBS += -lkbd
endif

ifeq ($(MOUSE),1)
  EE_CFLAGS += -DATHENA_MOUSE
  ATHENA_MODULES += ath_mouse.o
  IOP_MODULES += ps2mouse.o

  EE_LIBS += -lmouse
endif

ifeq ($(CAMERA),1)
  EE_BIN := $(EE_BIN)_cam
  EE_BIN_PKD := $(EE_BIN_PKD)_cam
  EE_CFLAGS += -DATHENA_CAMERA
  ATHENA_MODULES += ath_camera.o
  IOP_MODULES += ps2cam.o
  EE_LIBS += -lps2cam
endif

ifneq ($(EE_SIO), 0)
  EE_BIN := $(EE_BIN)_eesio
  EE_BIN_PKD := $(EE_BIN_PKD)_eesio
  EE_CFLAGS += -D__EESIO_PRINTF
  EE_LIBS += -lsiocookie
endif


EE_OBJS = $(APP_CORE) $(INI_READER) $(JS_CORE) $(ATHENA_MODULES) $(VU1_MPGS) $(IOP_MODULES) $(EMBEDDED_ASSETS) # group them all
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%) #prepend the object folder

EE_BIN := $(EE_BIN)$(EE_EXT)
EE_BIN_PKD := $(EE_BIN_PKD)$(EE_EXT)


#-------------------------- App Content ---------------------------#

all: $(EXT_LIBS) $(EE_BIN) $(EE_EMBED_DIR) $(EE_OBJS_DIR)
	@echo "$$HEADER"

	echo "Building $(EE_BIN)..."
	$(EE_STRIP) $(EE_BIN)

	ps2-packer $(EE_BIN) $(EE_BIN_PKD) > /dev/null

	mv $(EE_BIN) bin/
	mv $(EE_BIN_PKD) bin/

# mpgs: src/draw_3D_pvc.vsm src/draw_3D_colors.vsm src/draw_3D_colors_scissor.vsm src/draw_3D_lights.vsm src/draw_3D_lights_scissor.vsm src/draw_3D_spec.vsm src/draw_3D_spec_scissor.vsm

debug: $(EXT_LIBS) $(EE_BIN)
	echo "Building $(EE_BIN) with debug symbols..."
	mv $(EE_BIN) bin/athena_debug.elf

tests: all
	mv bin/$(EE_BIN) tests/test_suite.elf

clean:
	echo Cleaning executables...
	rm -f bin/$(EE_BIN) bin/$(EE_BIN_PKD)
	rm -rf $(EE_OBJS_DIR)
	rm -rf $(EE_EMBED_DIR)
	$(MAKE) -C modules/ds34usb clean
	$(MAKE) -C modules/ds34bt clean

rebuild: clean all

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
include embed.make

$(EE_EMBED_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	@echo CC - $<
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.vsm | $(EE_OBJS_DIR)
	@echo DVP - $<
	$(DIR_GUARD)
	$(EE_DVP) $< -o $@

$(EE_SRC_DIR)%.vcl: $(EE_SRC_DIR)%.vclpp | $(EE_SRC_DIR)
	@echo VCLPP - $<
	$(DIR_GUARD)
	$(EE_VCLPP) $< $@.vcl

$(EE_SRC_DIR)%.vsm: $(EE_SRC_DIR)%.vcl | $(EE_SRC_DIR)
	@echo VCL - $<
	$(DIR_GUARD)
	$(EE_VCL) -Isrc -g -o$@ $<

$(EE_OBJS_DIR)%.o: $(EE_EMBED_DIR)%.c | $(EE_OBJS_DIR)
	@echo BIN2C - $<
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
