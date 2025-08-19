.SILENT:
include Makefile.const

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

EE_BIN_PREF ?= athena
EE_BIN_PKD = $(EE_BIN_PREF)_pkd

UDPBD ?= 0
ILINK ?= 0
MX4SIO ?= 0

DEBUG ?= 0
EE_SIO ?= 0

PADEMU ?= 1
GRAPHICS ?= 1
ODE_PHYSICS_COLLISION ?= 1
AUDIO ?= 1

# Module linking control
STATIC_KEYBOARD ?= 1
STATIC_MOUSE ?= 1
STATIC_NETWORK ?= 1
STATIC_CAMERA ?= 0

DYNAMIC_KEYBOARD ?= 0
DYNAMIC_MOUSE ?= 0
DYNAMIC_NETWORK ?= 0
DYNAMIC_CAMERA ?= 0

EE_LIBS = -L$(PS2SDK)/ports/lib -lmc -lpad -lmtap -lpatches -lz -llzma -lzip -lfileXio -lelf-loader-nocolour -lerl -ldebug

EE_INCS += -I$(PS2SDK)/ports/include -I$(PS2SDK)/ports/include/zlib -Isrc/readini/include -Isrc/include

EE_CFLAGS +=  -Wall -fpermissive -DCONFIG_BIGNUM -DCONFIG_VERSION=\"$(shell cat VERSION)\" -D__TM_GMTOFF=tm_gmtoff -DPATH_MAX=256 -DPS2

ifeq ($(DEBUG),1)
  EE_CFLAGS += -DDEBUG
endif

JS_CORE = quickjs/cutils.o quickjs/libbf.o quickjs/libregexp.o quickjs/libunicode.o \
				 quickjs/realpath.o quickjs/quickjs.o quickjs/quickjs-libc.o

VU1_MPGS = draw_3D_colors.o \
           draw_3D_lights.o \
           draw_3D_spec.o \
           draw_3D_colors_skin.o \
           draw_3D_lights_skin.o \
           draw_3D_spec_skin.o \
           draw_3D_lights_ref.o 

# VU0_MPGS = matrix_multiply.o

APP_CORE = main.o bootlogo.o texture_manager.o owl_packet.o vif.o athena_math.o memory.o ee_tools.o module_system.o iop_manager.o taskman.o pad.o system.o strUtils.o mpg_manager.o matrix.o vector.o excepHandler.o exceptions.o 

INI_READER = readini/src/readini.o

ATHENA_MODULES = ath_env.o ath_vector.o ath_vector4.o ath_matrix.o ath_pads.o ath_system.o ath_iop.o ath_archive.o ath_timer.o ath_task.o

IOP_MODULES = iomanx.o filexio.o sio2man.o mcman.o mcserv.o padman.o  \
			  usbd.o bdm.o bdmfs_fatfs.o usbmass_bd.o cdfs.o \
			  freeram.o ps2dev9.o mtapman.o poweroff.o ps2atad.o \
			  ps2hdd.o ps2fs.o ata_bd.o mmceman.o 

EMBEDDED_ASSETS = quicksand_regular.o owl_indices.o owl_palette.o

EMBEDDED_ELFS = loader_elf.o

ifeq ($(UDPBD),1)
  EE_CFLAGS += -DATHENA_UDPBD
  IOP_MODULES += smap_udpbd.o
endif

ifeq ($(ILINK),1)
  EE_CFLAGS += -DATHENA_ILINK
  IOP_MODULES += iLinkman.o IEEE1394_bd.o
endif

ifeq ($(MX4SIO),1)
  EE_CFLAGS += -DATHENA_MX4SIO
  IOP_MODULES += mx4sio_bd.o
endif

ifeq ($(ODE_PHYSICS_COLLISION),1)
  EE_LIBS += -Lee_modules/ode/lib/ -lopcode -lice -lode
  EE_INCS += -Iee_modules/ode/include
  EE_CFLAGS += -DATHENA_ODE

  ATHENA_MODULES += ath_ode.o

  EXT_LIBS += ee_modules/ode/lib/libice.a ee_modules/ode/lib/libopcode.a ee_modules/ode/lib/libode.a
endif

ifeq ($(GRAPHICS),1)
  EE_LIBS += -L$(PS2DEV)/gsKit/lib/ -ljpeg -lfreetype -ldmakit -lpng
  EE_INCS += -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include/freetype2
  EE_CFLAGS += -DATHENA_GRAPHICS
  APP_CORE += graphics.o image_font.o owl_draw.o image_loaders.o mesh_loaders.o atlas.o fntsys.o render.o camera.o skin_math.o calc_3d.o fast_obj/fast_obj.o

  ATHENA_MODULES += ath_color.o ath_font.o ath_render.o ath_anim_3d.o ath_lights.o ath_3dcamera.o ath_screen.o ath_image.o ath_imagelist.o ath_shape.o
  EE_OBJS += $(VU1_MPGS) $(VU0_MPGS)
endif

ifeq ($(PADEMU),1)
  EE_CFLAGS += -DATHENA_PADEMU
  EE_INCS += -Iiop_modules/ds34bt/ee -Iiop_modules/ds34usb/ee
  EE_LIBS += -Liop_modules/ds34bt/ee/ -Liop_modules/ds34usb/ee/ -lds34bt -lds34usb
  IOP_MODULES += ds34usb.o ds34bt.o
	EXT_LIBS += iop_modules/ds34usb/ee/libds34usb.a iop_modules/ds34bt/ee/libds34bt.a
endif

ifeq ($(AUDIO),1)
  EE_CFLAGS += -DATHENA_AUDIO
  APP_CORE += sound_sfx.o sound_stream.o
  ATHENA_MODULES += ath_sound.o
  IOP_MODULES += libsd.o audsrv.o

  EE_LIBS += -laudsrv -lvorbisfile -lvorbis -logg
endif

ifneq ($(EE_SIO), 0)
  EE_BIN_PREF := $(EE_BIN_PREF)_eesio
  EE_BIN_PKD := $(EE_BIN_PKD)_eesio
  EE_CFLAGS += -D__EESIO_PRINTF
  EE_LIBS += -lsiocookie
endif

# Static module linking
ifeq ($(STATIC_NETWORK),1)
  EE_CFLAGS += -DATHENA_NETWORK
  APP_CORE += network.o request.o
  ATHENA_MODULES += ath_network.o ath_socket.o ath_request.o ath_websocket.o
  IOP_MODULES += NETMAN.o SMAP.o ps2ips.o
  EE_LIBS += -lnetman -lps2ip -lcurl -lwolfssl

  DYNAMIC_NETWORK = 0
endif

ifeq ($(STATIC_KEYBOARD),1)
  EE_CFLAGS += -DATHENA_KEYBOARD
  ATHENA_MODULES += ath_keyboard.o
  IOP_MODULES += ps2kbd.o
  EE_LIBS += -lkbd

  DYNAMIC_KEYBOARD = 0
endif

ifeq ($(STATIC_MOUSE),1)
  EE_CFLAGS += -DATHENA_MOUSE
  ATHENA_MODULES += ath_mouse.o
  IOP_MODULES += ps2mouse.o
  EE_LIBS += -lmouse

  DYNAMIC_MOUSE = 0
endif

ifeq ($(STATIC_CAMERA),1)
  EE_BIN_PREF := $(EE_BIN_PREF)_cam
  EE_BIN_PKD := $(EE_BIN_PKD)_cam
  EE_CFLAGS += -DATHENA_CAMERA
  ATHENA_MODULES += ath_camera.o
  IOP_MODULES += ps2cam.o
  EE_LIBS += -lps2cam

  DYNAMIC_CAMERA = 0
endif

ATHENA_MODULES := $(ATHENA_MODULES:%=$(JS_API_DIR)%) #prepend the modules folder
VU1_MPGS := $(VU1_MPGS:%=$(VU1_MPGS_DIR)%) #prepend the microprograms folder
VU0_MPGS := $(VU0_MPGS:%=$(VU0_MPGS_DIR)%) #prepend the microprograms folder

EE_OBJS = $(APP_CORE) $(INI_READER) $(JS_CORE) $(ATHENA_MODULES) $(VU1_MPGS) $(VU0_MPGS) $(IOP_MODULES) $(EMBEDDED_ELFS) $(EMBEDDED_ASSETS) # group them all
EE_OBJS := $(EE_OBJS:%=$(EE_OBJ_DIR)%) #prepend the object folder

EE_BIN := $(EE_BIN_DIR)$(EE_BIN_PREF)$(EE_EXT)
EE_BIN_PKD := $(EE_BIN_DIR)$(EE_BIN_PKD)$(EE_EXT)


#-------------------------- App Content ---------------------------#

all: $(DIR_GUARD) $(EXT_LIBS) $(EE_OBJS)
	$(MAKE) -f Makefile.dl KEYBOARD=$(DYNAMIC_KEYBOARD)
	$(MAKE) -f Makefile.dl MOUSE=$(DYNAMIC_MOUSE)

	$(EE_CXX) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $(EE_BIN_DIR)tmp.elf $(EE_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -Wno-write-strings $(EE_LIBS) $(EE_SRC_DIR)dummy-exports.c
	./build-exports.sh
	$(EE_CXX) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $(EE_BIN) $(EE_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -fpermissive -Wno-write-strings $(EE_LIBS) $(EE_SRC_DIR)exports.c
	rm $(EE_BIN_DIR)tmp.elf
	@echo "$$HEADER"
	
	echo "Building $(EE_BIN)..."
	$(EE_STRIP) $(EE_BIN) 
	
	ps2-packer $(EE_BIN) $(EE_BIN_PKD) > /dev/null

 # vu1_mpgs: src/vu1/draw_3D_colors.vsm src/vu1/draw_3D_lights.vsm src/vu1/draw_3D_spec.vsm src/vu1/draw_3D_colors_skin.vsm src/vu1/draw_3D_lights_skin.vsm src/vu1/draw_3D_spec_skin.vsm src/vu1/draw_3D_lights_ref.vsm 
 # vu0_mpgs: src/vu0/matrix_multiply.vsm

debug: $(DIR_GUARD) $(EXT_LIBS) $(EE_OBJS) 
	$(MAKE) -f Makefile.dl KEYBOARD=$(DYNAMIC_KEYBOARD)
	$(MAKE) -f Makefile.dl MOUSE=$(DYNAMIC_MOUSE)

	$(EE_CXX) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $(EE_BIN_DIR)tmp.elf $(EE_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -Wno-write-strings $(EE_LIBS) $(EE_SRC_DIR)dummy-exports.c
	./build-exports.sh
	$(EE_CXX) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o bin/athena_debug.elf $(EE_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -fpermissive -Wno-write-strings $(EE_LIBS) $(EE_SRC_DIR)exports.c
	rm $(EE_BIN_DIR)tmp.elf

	echo "Building bin/athena_debug.elf with debug symbols..."

clean:
	echo Cleaning executables...
	rm -f bin/$(EE_BIN) bin/$(EE_BIN_PKD)
	rm -rf $(EE_OBJ_DIR)
	rm -rf $(EE_EMBED_DIR)
	$(MAKE) -C iop_modules/ds34usb clean
	$(MAKE) -C iop_modules/ds34bt clean
	$(MAKE) -C ee_modules/loader clean
	$(MAKE) -C ee_modules/ode clean

	$(MAKE) -f Makefile.dl KEYBOARD=$(DYNAMIC_KEYBOARD) clean
	$(MAKE) -f Makefile.dl MOUSE=$(DYNAMIC_MOUSE) clean

rebuild: clean all

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
include Makefile.embed

$(EE_EMBED_DIR):
	@mkdir -p $@

$(EE_OBJ_DIR):
	@mkdir -p $@

$(EE_OBJ_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJ_DIR)
	@echo CC - $<
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

  $(EE_OBJ_DIR)%.o: $(EE_SRC_DIR)%.s | $(EE_OBJ_DIR)
	@echo AS - $<
	$(DIR_GUARD)
	$(EE_AS) $(EE_ASFLAGS) $(EE_INCS) $< -o $@

  $(EE_OBJ_DIR)%.o: $(EE_SRC_DIR)%.S | $(EE_OBJ_DIR)
	@echo AS - $<
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJ_DIR)%.o: $(EE_SRC_DIR)%.vsm | $(EE_OBJ_DIR)
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

$(EE_OBJ_DIR)%.o: $(EE_EMBED_DIR)%.c | $(EE_OBJ_DIR)
	@echo BIN2C - $<
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
