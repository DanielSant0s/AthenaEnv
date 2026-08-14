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

ATHENA_JS ?= 1

DEBUG ?= 0
EE_SIO ?= 0

PADEMU ?= 1
GRAPHICS ?= 1
ODE_PHYSICS_COLLISION ?= 1
BOX2D_PHYSICS ?= 1
AUDIO ?= 1

# Module linking control
STATIC_KEYBOARD ?= 1
STATIC_MOUSE ?= 1
STATIC_NETWORK ?= 1
STATIC_CAMERA ?= 0
STATIC_REMOTE ?= 1

DYNAMIC_KEYBOARD ?= 0
DYNAMIC_MOUSE ?= 0
DYNAMIC_NETWORK ?= 0
DYNAMIC_CAMERA ?= 0

EE_LIBS = -L$(PS2SDK)/ports/lib -lmc -lpad -lmtap -lpatches -lz -llzma -lzip -lfileXio -lelf-loader-nocolour -lerl -ldebug

EE_INCS += -I$(PS2SDK)/ports/include -I$(PS2SDK)/ports/include/zlib -Isrc/readini/include -Isrc/include -Isrc/include/athena -Isrc/js_api

EE_CFLAGS +=  -Wall -fpermissive -DCONFIG_VERSION=\"$(shell cat VERSION)\" -D__TM_GMTOFF=tm_gmtoff -DPATH_MAX=256 -DPS2

ifeq ($(DEBUG),1)
  EE_CFLAGS += -DDEBUG
endif

JS_CORE = quickjs/cutils.o quickjs/libregexp.o quickjs/libunicode.o \
				 quickjs/realpath.o quickjs/quickjs.o quickjs/quickjs-libc.o

VU1_MPGS = draw_3D_colors.o \
           draw_3D_lights.o \
           draw_3D_spec.o \
           draw_3D_colors_skin.o \
           draw_3D_lights_skin.o \
           draw_3D_spec_skin.o \
           draw_3D_lights_ref.o \
           draw_2D_tile_list.o

# VU0_MPGS = matrix_multiply.o

APP_CORE = main.o athena_core.o bootlogo.o texture_manager.o owl_packet.o vif.o athena_math.o memory.o ee_tools.o module_system.o iop_manager.o taskman.o lockman.o pad.o system.o strUtils.o mpg_manager.o matrix.o vector.o excepHandler.o exceptions.o athena/system_facade.o athena/iop_facade.o athena/archive.o athena/pad.o athena/timer.o athena/mutex.o athena/task.o

INI_READER = readini/src/readini.o

ATHENA_MODULES = ath_env.o ath_module_registry.o ath_vector.o ath_vector4.o ath_matrix.o ath_pads.o ath_system.o ath_iop.o ath_archive.o ath_timer.o ath_task.o ath_mutex.o ath_box2d.o ath_box2d_body.o ath_box2d_shape.o ath_box2d_joint.o ath_box2d_cast.o ath_box2d_chain.o ath_box2d_common.o ath_box2d_conversion.o ath_box2d_events.o ath_box2d_query.o ath_box2d_userdata.o ath_box2d_world.o

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
  APP_CORE += athena/ode_facade.o

  EXT_LIBS += ee_modules/ode/lib/libice.a ee_modules/ode/lib/libopcode.a ee_modules/ode/lib/libode.a
endif

ifeq ($(BOX2D_PHYSICS),1)
  EE_LIBS += -Lee_modules/box2d/lib/ -lbox2d
  EE_INCS += -Isrc/Box2d/include
  EE_CFLAGS += -DATHENA_BOX2D -DBOX2D_DISABLE_SIMD

  EXT_LIBS += ee_modules/box2d/lib/libbox2d.a
endif

ifeq ($(GRAPHICS),1)
  EE_LIBS += -L$(PS2DEV)/gsKit/lib/ -ljpeg -lfreetype -ldmakit -lpng
  EE_INCS += -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include/freetype2
  EE_CFLAGS += -DATHENA_GRAPHICS
  APP_CORE += tile_render.o graphics.o image_font.o owl_draw.o image_loaders.o mesh_loaders.o atlas.o fntsys.o render.o camera.o skin_math.o calc_3d.o fast_obj/fast_obj.o athena/color.o athena/draw.o athena/vec2.o athena/vec3.o athena/vec4.o athena/matrix_facade.o athena/screen.o athena/sprite.o athena/image.o athena/image_list.o athena/font.o \
              athena/camera3d.o athena/lights.o athena/anim3d.o athena/shadows_facade.o athena/render_facade.o

  ATHENA_MODULES += ath_color.o ath_font.o ath_render.o ath_anim_3d.o ath_lights.o ath_3dcamera.o ath_screen.o ath_image.o ath_imagelist.o ath_shape.o ath_shadows.o ath_sprite.o
  APP_CORE += shadows.o render_batch.o render_scene.o render_async_loader.o
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
  APP_CORE += sound_sfx.o sound_stream.o athena/sound_facade.o
  ATHENA_MODULES += ath_sound.o
  IOP_MODULES += libsd.o audsrv.o

  EE_LIBS += -laudsrv -lvorbisfile -lvorbis -logg
endif

# MPEG Video support (requires PS2SDK libmpeg)
MPEG_VIDEO ?= 1

ifeq ($(MPEG_VIDEO),1)
  EE_CFLAGS += -DATHENA_MPEG_VIDEO
  APP_CORE += mpeg_player.o athena/video.o
  ATHENA_MODULES += ath_mpeg.o
  # Prefer the vendored libmpeg sources (src/libmpeg, from our ps2sdk fork's
  # fix-libmpeg branch -- fixes real ABI/$at register bugs in libmpeg_core.s
  # that the toolchain's prebuilt libmpeg.a doesn't have) over the toolchain
  # build; fall back to it if the vendored sources aren't present.
  ifneq (,$(wildcard $(EE_SRC_DIR)libmpeg/include/libmpeg.h))
    EE_INCS += -I$(EE_SRC_DIR)libmpeg/include
    EE_LIBS += -Lee_modules/mpeg/lib -lmpeg
    EXT_LIBS += ee_modules/mpeg/lib/libmpeg.a
  else
    EE_LIBS += -lmpeg
  endif
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
  APP_CORE += network.o athena/net_facade.o athena/request.o athena/socket_facade.o athena/websocket.o
  ATHENA_MODULES += ath_network.o ath_socket.o ath_request.o ath_websocket.o
  IOP_MODULES += NETMAN.o SMAP.o ps2ips.o
  # Native networking backend (lwIP + BearSSL)
  EE_LIBS += -lnetman -lps2ip
  APP_CORE += net/ath_http.o net/ath_tls.o net/ath_ws.o
  # Optional TLS (BearSSL)
  EE_CFLAGS += -DATHENA_HAS_BEARSSL=1
  # Prefer vendored BearSSL sources if present; else link against libbearssl
  ifneq (,$(wildcard $(EE_SRC_DIR)BearSSL/inc/bearssl.h))
    EE_INCS += -I$(EE_SRC_DIR)BearSSL/inc
    EE_LIBS += -Lee_modules/bearssl/lib -lbearssl
    EXT_LIBS += ee_modules/bearssl/lib/libbearssl.a
  else
    EE_LIBS += -lbearssl
  endif

  DYNAMIC_NETWORK = 0
endif

ifeq ($(STATIC_KEYBOARD),1)
  EE_CFLAGS += -DATHENA_KEYBOARD
  APP_CORE += athena/keyboard.o
  ATHENA_MODULES += ath_keyboard.o
  IOP_MODULES += ps2kbd.o
  EE_LIBS += -lkbd

  DYNAMIC_KEYBOARD = 0
endif

ifeq ($(STATIC_MOUSE),1)
  EE_CFLAGS += -DATHENA_MOUSE
  APP_CORE += athena/mouse.o
  ATHENA_MODULES += ath_mouse.o
  IOP_MODULES += ps2mouse.o
  EE_LIBS += -lmouse

  DYNAMIC_MOUSE = 0
endif

ifeq ($(STATIC_REMOTE),1)
  EE_CFLAGS += -DATHENA_REMOTE
  APP_CORE += remote.o athena/remote.o
  ATHENA_MODULES += ath_remote.o
  IOP_MODULES += rmman.o
  EE_LIBS += -lrm
endif

ifeq ($(STATIC_CAMERA),1)
  EE_BIN_PREF := $(EE_BIN_PREF)_cam
  EE_BIN_PKD := $(EE_BIN_PKD)_cam
  EE_CFLAGS += -DATHENA_CAMERA
  APP_CORE += athena/camera.o
  ATHENA_MODULES += ath_camera.o
  IOP_MODULES += ps2cam.o
  EE_LIBS += -lps2cam

  DYNAMIC_CAMERA = 0
endif

# Native compiler (AOT JS to MIPS R5900) — requires QuickJS
NATIVE_COMPILER ?= 1
NATIVE_COMPILER_OBJS :=

ifeq ($(NATIVE_COMPILER),1)
ifeq ($(ATHENA_JS),1)
  EE_CFLAGS += -DATHENA_NATIVE_COMPILER
  EE_INCS += -Isrc/native_compiler -Isrc
  ATHENA_MODULES += ath_native.o
  NATIVE_COMPILER_OBJS = native_compiler/native_compiler.o native_compiler/mips_emitter.o native_compiler/type_inference.o native_compiler/native_struct.o native_compiler/int64_runtime.o native_compiler/native_string.o native_compiler/native_array.o athena/native_facade.o
endif
endif

ATHENA_MODULES := $(ATHENA_MODULES:%=$(JS_API_DIR)%) #prepend the modules folder
VU1_MPGS := $(VU1_MPGS:%=$(VU1_MPGS_DIR)%) #prepend the microprograms folder
VU0_MPGS := $(VU0_MPGS:%=$(VU0_MPGS_DIR)%) #prepend the microprograms folder

EE_OBJS = $(APP_CORE) $(INI_READER) $(JS_CORE) $(ATHENA_MODULES) $(NATIVE_COMPILER_OBJS) $(VU1_MPGS) $(VU0_MPGS) $(IOP_MODULES) $(EMBEDDED_ELFS) $(EMBEDDED_ASSETS)
ifeq ($(ATHENA_JS),0)
EE_OBJS := $(filter-out $(JS_CORE) $(ATHENA_MODULES) $(NATIVE_COMPILER_OBJS),$(EE_OBJS))
EE_OBJS += capp_main.o
endif
EE_OBJS := $(EE_OBJS:%=$(EE_OBJ_DIR)%)

ifeq ($(ATHENA_JS),1)
  EE_CFLAGS += -DATHENA_JS
endif

# main.c is compiled differently with/without ATHENA_JS; invalidate only that
# object when switching modes instead of wiping the whole tree on every capp.
ifeq ($(filter capp,$(MAKECMDGOALS)),capp)
MAIN_BUILD_STAMP := $(EE_OBJ_DIR).main_build_capp
else ifeq ($(ATHENA_JS),0)
MAIN_BUILD_STAMP := $(EE_OBJ_DIR).main_build_capp
else
MAIN_BUILD_STAMP := $(EE_OBJ_DIR).main_build_js
endif

$(MAIN_BUILD_STAMP): | $(EE_OBJ_DIR)
	@for s in $(EE_OBJ_DIR).main_build_js $(EE_OBJ_DIR).main_build_capp; do \
		if [ -f "$$s" ] && [ "$$s" != "$(MAIN_BUILD_STAMP)" ]; then \
			rm -f "$$s" "$(EE_OBJ_DIR)main.o"; \
		fi; \
	done
	@touch $@

include Makefile.lib

EE_BIN := $(EE_BIN_DIR)$(EE_BIN_PREF)$(EE_EXT)
EE_BIN_PKD := $(EE_BIN_DIR)$(EE_BIN_PKD)$(EE_EXT)

# ps2-packer locates its unpacking stubs relative to argv[0]'s directory. When it
# is invoked as a bare name resolved through PATH, argv[0] has no directory and the
# stub lookup fails ("Unable to open stub file ..."). Resolve the absolute path so
# argv[0] always contains a directory component.
PS2_PACKER := $(shell command -v ps2-packer 2>/dev/null)
ifeq ($(PS2_PACKER),)
PS2_PACKER := ps2-packer
endif


# Header dependency tracking
-include $(EE_OBJS:.o=.d)

#-------------------------- App Content ---------------------------#

all: $(DIR_GUARD) $(EXT_LIBS) $(EE_OBJS) libs
	$(MAKE) -f Makefile.dl KEYBOARD=$(DYNAMIC_KEYBOARD)
	$(MAKE) -f Makefile.dl MOUSE=$(DYNAMIC_MOUSE)

	$(EE_CXX) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $(EE_BIN_DIR)tmp.elf $(EE_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -Wno-write-strings $(EE_LIBS) $(EE_SRC_DIR)dummy-exports.c
	./build-exports.sh
	$(EE_CXX) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $(EE_BIN) $(EE_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -fpermissive -Wno-write-strings $(EE_LIBS) $(EE_SRC_DIR)exports.c
	rm $(EE_BIN_DIR)tmp.elf
	@echo "$$HEADER"
	
	echo "Building $(EE_BIN)..."
	$(EE_STRIP) $(EE_BIN) 
	
	$(PS2_PACKER) $(EE_BIN) $(EE_BIN_PKD) > /dev/null

 vu1_mpgs: src/vu1/draw_2D_tile_list.vsm src/vu1/draw_3D_colors.vsm src/vu1/draw_3D_lights.vsm src/vu1/draw_3D_spec.vsm src/vu1/draw_3D_colors_skin.vsm src/vu1/draw_3D_lights_skin.vsm src/vu1/draw_3D_spec_skin.vsm src/vu1/draw_3D_lights_ref.vsm 
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
	$(MAKE) -C ee_modules/bearssl clean
	$(MAKE) -C ee_modules/box2d clean
	$(MAKE) -C ee_modules/mpeg clean

	$(MAKE) -f Makefile.dl KEYBOARD=$(DYNAMIC_KEYBOARD) clean
	$(MAKE) -f Makefile.dl MOUSE=$(DYNAMIC_MOUSE) clean

rebuild: clean all

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
include Makefile.embed

# Build vendored BearSSL static library when present and TLS enabled
ee_modules/bearssl/lib/libbearssl.a:
	$(MAKE) -C ee_modules/bearssl

# Build vendored libmpeg static library when present
ee_modules/mpeg/lib/libmpeg.a:
	$(MAKE) -C ee_modules/mpeg

$(EE_EMBED_DIR):
	@mkdir -p $@

$(EE_OBJ_DIR):
	@mkdir -p $@

$(EE_OBJ_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJ_DIR)
	@echo CC - $<
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -MMD -MP -c $< -o $@

$(EE_OBJ_DIR)main.o: $(EE_SRC_DIR)main.c $(MAIN_BUILD_STAMP) | $(EE_OBJ_DIR)
	@echo CC - $<
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -MMD -MP -c $< -o $@

  $(EE_OBJ_DIR)%.o: $(EE_SRC_DIR)%.s | $(EE_OBJ_DIR)
	@echo AS - $<
	$(DIR_GUARD)
	$(EE_AS) $(EE_ASFLAGS) $(EE_INCS) $< -o $@

  $(EE_OBJ_DIR)%.o: $(EE_SRC_DIR)%.S | $(EE_OBJ_DIR)
	@echo AS - $<
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -MMD -MP -c $< -o $@

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

# capp must override flags after Makefile.eeglobal expands EE_CFLAGS.
capp: ATHENA_JS=0
capp: EE_CFLAGS := $(filter-out -DATHENA_NATIVE_COMPILER -DATHENA_JS,$(EE_CFLAGS))
