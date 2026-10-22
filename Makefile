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

# Scripting engine: js (QuickJS), lua (Lua 5.4) or none (pure C app, see capp).
SCRIPT_ENGINE ?= js

ifeq ($(SCRIPT_ENGINE),lua)
  ATHENA_JS := 0
  ATHENA_LUA := 1
  EE_BIN_PREF := athena_lua
else ifeq ($(SCRIPT_ENGINE),none)
  ATHENA_JS := 0
  ATHENA_LUA := 0
else
  ATHENA_JS ?= 1
  ATHENA_LUA := 0
endif

DEBUG ?= 0
EE_SIO ?= 0

PADEMU ?= 1
GRAPHICS ?= 1
PHYSICS ?= 1
AUDIO ?= 1

# Module linking control
STATIC_KEYBOARD ?= 1
STATIC_MOUSE ?= 1
STATIC_NETWORK ?= 0
STATIC_CAMERA ?= 0
STATIC_REMOTE ?= 1
STATIC_NATIVE_COMPILER ?= 0
STATIC_ARCHIVE ?= 0
STATIC_MPEG_VIDEO ?= 0

DYNAMIC_KEYBOARD ?= 0
DYNAMIC_MOUSE ?= 0
DYNAMIC_NETWORK ?= 1
DYNAMIC_CAMERA ?= 0
DYNAMIC_NATIVE_COMPILER ?= 1
DYNAMIC_ARCHIVE ?= 1
DYNAMIC_MPEG_VIDEO ?= 1

EE_LIBS = -L$(PS2SDK)/ports/lib -lmc -lpad -lmtap -lpatches -lz -lfileXio -lelf-loader-nocolour -ldebug

ifneq (,$(wildcard $(EE_SRC_DIR)liberl/include/erl.h))
  EE_INCS += -I$(EE_SRC_DIR)liberl/include
  EE_LIBS += ee_modules/erl/lib/liberl.a
  EXT_LIBS += ee_modules/erl/lib/liberl.a
else
  EE_LIBS += -lerl
endif

EE_INCS += -I$(PS2SDK)/ports/include -I$(PS2SDK)/ports/include/zlib -Isrc/readini/include -Isrc/include -Isrc/include/athena -Isrc/js_api

EE_CFLAGS +=  -Wall -fpermissive -DCONFIG_VERSION=\"$(shell cat VERSION)\" -D__TM_GMTOFF=tm_gmtoff -DPATH_MAX=256 -DPS2

ifeq ($(DEBUG),1)
  EE_CFLAGS += -DDEBUG
endif

JS_CORE = quickjs/f32str.o quickjs/cutils.o quickjs/libregexp.o quickjs/libunicode.o \
				 quickjs/realpath.o quickjs/quickjs.o quickjs/quickjs-libc.o

VU1_MPGS = draw_3D_colors.o \
           draw_3D_lights.o \
           draw_3D_spec.o \
           draw_3D_colors_skin.o \
           draw_3D_lights_skin.o \
           draw_3D_spec_skin.o \
           draw_3D_lights_ref.o \
           draw_3D_lights_ref_skin.o \
           draw_2D_tile_list.o

# VU0_MPGS = matrix_multiply.o

APP_CORE = main.o athena_core.o malloc_lock.o athena/sampler.o bootlogo.o texture_manager.o owl_packet.o profiler.o vif.o athena_math.o memory.o lz4_ps2.o ee_tools.o module_system.o iop_manager.o taskman.o lockman.o pad.o system.o strUtils.o mpg_manager.o matrix.o vector.o excepHandler.o exceptions.o athena/system_facade.o athena/iop_facade.o athena/pad.o athena/timer.o athena/mutex.o athena/task.o

INI_READER = readini/src/readini.o

LUA_CORE = lua/lapi.o lua/lcode.o lua/lctype.o lua/ldebug.o lua/ldo.o lua/ldump.o lua/lfunc.o \
           lua/lgc.o lua/llex.o lua/lmem.o lua/lobject.o lua/lopcodes.o lua/lparser.o lua/lstate.o \
           lua/lstring.o lua/ltable.o lua/ltm.o lua/lundump.o lua/lvm.o lua/lzio.o lua/lauxlib.o \
           lua/lbaselib.o lua/lcorolib.o lua/ldblib.o lua/liolib.o lua/lmathlib.o lua/loadlib.o \
           lua/loslib.o lua/lstrlib.o lua/ltablib.o lua/lutf8lib.o lua/linit.o

LUA_MODULES = ath_lua_core.o ath_lua_env.o ath_lua_registry.o ath_lua_system.o ath_lua_pads.o ath_lua_basic.o \
              ath_lua_math.o ath_lua_sys.o ath_lua_input.o

ATHENA_MODULES = ath_env.o ath_module_registry.o ath_vector.o ath_vector4.o ath_matrix.o ath_pads.o ath_system.o ath_iop.o ath_timer.o ath_task.o ath_mutex.o

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

ifeq ($(PHYSICS),1)
  EE_CFLAGS += -DATHENA_PHYSICS

  ATHENA_MODULES += ath_physics.o
  LUA_MODULES += ath_lua_physics.o
  APP_CORE += physics.o athena/physics_facade.o
endif

ifeq ($(GRAPHICS),1)
  EE_LIBS += -L$(PS2DEV)/gsKit/lib/ -ljpeg -lfreetype -ldmakit -lpng
  EE_INCS += -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include/freetype2
  EE_CFLAGS += -DATHENA_GRAPHICS
  APP_CORE += particles.o athena/particles.o tile_render.o graphics.o image_font.o owl_draw.o image_loaders.o mesh_loaders.o atlas.o fntsys.o render.o camera.o skin_math.o calc_3d.o fast_obj/fast_obj.o athena/color.o athena/draw.o athena/vec2.o athena/vec3.o athena/vec4.o athena/matrix_facade.o athena/screen.o athena/sprite.o athena/image.o athena/image_list.o athena/font.o \
              athena/camera3d.o athena/lights.o athena/anim3d.o athena/shadows_facade.o athena/render_facade.o

  LUA_MODULES += ath_lua_graphics.o ath_lua_render.o ath_lua_fx.o
  ATHENA_MODULES += ath_color.o ath_font.o ath_render.o ath_anim_3d.o ath_lights.o ath_3dcamera.o ath_screen.o ath_image.o ath_imagelist.o ath_shape.o ath_shadows.o ath_sprite.o ath_particles.o
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

# Archive support (ZIP/GZ/TAR via libzip + liblzma)
ifeq ($(STATIC_ARCHIVE),1)
  EE_CFLAGS += -DATHENA_ARCHIVE
  APP_CORE += athena/archive.o
  ATHENA_MODULES += ath_archive.o
  LUA_MODULES += ath_lua_archive.o
  EE_LIBS += -llzma -lzip
  DYNAMIC_ARCHIVE = 0
endif

# MPEG Video support (requires PS2SDK libmpeg)
ifeq ($(STATIC_MPEG_VIDEO),1)
  EE_CFLAGS += -DATHENA_MPEG_VIDEO
  APP_CORE += mpeg_player.o athena/video.o
  ATHENA_MODULES += ath_mpeg.o
  LUA_MODULES += ath_lua_video.o
  # Prefer the vendored libmpeg sources (src/libmpeg, from our ps2sdk fork's
  # fix-libmpeg branch -- fixes real ABI/$at register bugs in libmpeg_core.s
  # that the toolchain's prebuilt libmpeg.a doesn't have) over the toolchain
  # build; fall back to it if the vendored sources aren't present.
  ifneq (,$(wildcard $(EE_SRC_DIR)libmpeg/include/libmpeg.h))
    EE_INCS += -I$(EE_SRC_DIR)libmpeg/include
    EE_LIBS += ee_modules/mpeg/lib/libmpeg.a
    EXT_LIBS += ee_modules/mpeg/lib/libmpeg.a
  else
    EE_LIBS += -lmpeg
  endif
  DYNAMIC_MPEG_VIDEO = 0
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
  LUA_MODULES += ath_lua_network.o
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
NATIVE_COMPILER_OBJS :=

ifeq ($(STATIC_NATIVE_COMPILER),1)
ifeq ($(ATHENA_JS),1)
  EE_CFLAGS += -DATHENA_NATIVE_COMPILER
  EE_INCS += -Isrc/native_compiler -Isrc
  ATHENA_MODULES += ath_native.o
  NATIVE_COMPILER_OBJS = native_compiler/native_compiler.o native_compiler/mips_emitter.o native_compiler/type_inference.o native_compiler/native_struct.o native_compiler/int64_runtime.o native_compiler/native_string.o native_compiler/native_array.o athena/native_facade.o
  DYNAMIC_NATIVE_COMPILER = 0
endif
endif

ATHENA_MODULES := $(ATHENA_MODULES:%=$(JS_API_DIR)%) #prepend the modules folder
LUA_MODULES := $(LUA_MODULES:%=$(LUA_API_DIR)%)
VU1_MPGS := $(VU1_MPGS:%=$(VU1_MPGS_DIR)%) #prepend the microprograms folder
VU0_MPGS := $(VU0_MPGS:%=$(VU0_MPGS_DIR)%) #prepend the microprograms folder

EE_OBJS = $(APP_CORE) $(INI_READER) $(JS_CORE) $(ATHENA_MODULES) $(NATIVE_COMPILER_OBJS) $(VU1_MPGS) $(VU0_MPGS) $(IOP_MODULES) $(EMBEDDED_ELFS) $(EMBEDDED_ASSETS)
ifeq ($(ATHENA_JS),0)
EE_OBJS := $(filter-out $(JS_CORE) $(ATHENA_MODULES) $(NATIVE_COMPILER_OBJS),$(EE_OBJS))
ifeq ($(ATHENA_LUA),1)
EE_OBJS += $(LUA_CORE) $(LUA_MODULES) ath_lua_embedded.o
else
EE_OBJS += capp_main.o
endif
endif
EE_OBJS := $(EE_OBJS:%=$(EE_OBJ_DIR)%)

ifeq ($(ATHENA_JS),1)
  EE_CFLAGS += -DATHENA_JS
  # the JS heap pool (quickjs.c) takes back its blocks if they reach libc
  EE_LDFLAGS += -Wl,--wrap=free -Wl,--wrap=realloc
endif

ifeq ($(ATHENA_LUA),1)
  EE_CFLAGS += -DATHENA_LUA
  EE_INCS += -Isrc/lua -Isrc/lua_api
  # Network, Archive and MPEG ship as <name>_lua.erl; the rest is static
  # (or, for the native compiler, JS-only).
  DYNAMIC_KEYBOARD := 0
  DYNAMIC_MOUSE := 0
  DYNAMIC_CAMERA := 0
  DYNAMIC_NATIVE_COMPILER := 0
  DL_ENGINE := lua
else
  DL_ENGINE := js
endif

# main.c is compiled differently with/without ATHENA_JS; invalidate only that
# object when switching modes instead of wiping the whole tree on every capp.
ifeq ($(filter capp,$(MAKECMDGOALS)),capp)
MAIN_BUILD_STAMP := $(EE_OBJ_DIR).main_build_capp
else ifeq ($(ATHENA_LUA),1)
MAIN_BUILD_STAMP := $(EE_OBJ_DIR).main_build_lua
else ifeq ($(ATHENA_JS),0)
MAIN_BUILD_STAMP := $(EE_OBJ_DIR).main_build_capp
else
MAIN_BUILD_STAMP := $(EE_OBJ_DIR).main_build_js
endif

.DEFAULT_GOAL := all

# Lua-side runtime sources (JSON, prelude) embedded as C strings.
LUA_EMBED_SRCS := $(wildcard $(EE_SRC_DIR)lua_api/lua/*.lua)
$(EE_EMBED_DIR)ath_lua_embedded.c: $(LUA_EMBED_SRCS) scripts/lua2c.py | $(EE_EMBED_DIR)
	@echo LUA2C - $@
	python3 scripts/lua2c.py $@ $(LUA_EMBED_SRCS)

$(MAIN_BUILD_STAMP): | $(EE_OBJ_DIR)
	@for s in $(EE_OBJ_DIR).main_build_js $(EE_OBJ_DIR).main_build_capp $(EE_OBJ_DIR).main_build_lua; do \
		if [ -f "$$s" ] && [ "$$s" != "$(MAIN_BUILD_STAMP)" ]; then \
			rm -f "$$s" "$(EE_OBJ_DIR)main.o"; \
		fi; \
	done
	@touch $@

$(EE_OBJ_DIR)main.o: $(MAIN_BUILD_STAMP)

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

all: $(MAIN_BUILD_STAMP) $(DIR_GUARD) $(EXT_LIBS) $(EE_OBJS) libs
	$(MAKE) -f Makefile.dl ENGINE=$(DL_ENGINE) KEYBOARD=$(DYNAMIC_KEYBOARD)
	$(MAKE) -f Makefile.dl ENGINE=$(DL_ENGINE) MOUSE=$(DYNAMIC_MOUSE)
	$(MAKE) -f Makefile.dl ENGINE=$(DL_ENGINE) NETWORK=$(DYNAMIC_NETWORK)
	$(MAKE) -f Makefile.dl ENGINE=$(DL_ENGINE) NATIVE_COMPILER=$(DYNAMIC_NATIVE_COMPILER)
	$(MAKE) -f Makefile.dl ENGINE=$(DL_ENGINE) ARCHIVE=$(DYNAMIC_ARCHIVE)
	$(MAKE) -f Makefile.dl ENGINE=$(DL_ENGINE) MPEG_VIDEO=$(DYNAMIC_MPEG_VIDEO)

	$(EE_CXX) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $(EE_BIN_DIR)tmp.elf $(EE_OBJS) $(EE_SRC_DIR)dummy-exports.c $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -Wno-write-strings $(EE_LIBS)
	./build-exports.sh
	$(EE_CXX) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $(EE_BIN) $(EE_OBJS) $(EE_SRC_DIR)exports.c $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -fpermissive -Wno-write-strings $(EE_LIBS)
	rm $(EE_BIN_DIR)tmp.elf
	@echo "$$HEADER"
	
	echo "Building $(EE_BIN)..."
	$(EE_STRIP) $(EE_BIN) 
	
	$(PS2_PACKER) $(EE_BIN) $(EE_BIN_PKD) > /dev/null

 vu1_mpgs: src/vu1/draw_2D_tile_list.vsm src/vu1/draw_3D_colors.vsm src/vu1/draw_3D_lights.vsm src/vu1/draw_3D_spec.vsm src/vu1/draw_3D_colors_skin.vsm src/vu1/draw_3D_lights_skin.vsm src/vu1/draw_3D_spec_skin.vsm src/vu1/draw_3D_lights_ref.vsm src/vu1/draw_3D_lights_ref_skin.vsm 
 # vu0_mpgs: src/vu0/matrix_multiply.vsm

debug: $(DIR_GUARD) $(EXT_LIBS) $(EE_OBJS) 
	$(MAKE) -f Makefile.dl ENGINE=$(DL_ENGINE) KEYBOARD=$(DYNAMIC_KEYBOARD)
	$(MAKE) -f Makefile.dl ENGINE=$(DL_ENGINE) MOUSE=$(DYNAMIC_MOUSE)
	$(MAKE) -f Makefile.dl ENGINE=$(DL_ENGINE) NETWORK=$(DYNAMIC_NETWORK)
	$(MAKE) -f Makefile.dl ENGINE=$(DL_ENGINE) NATIVE_COMPILER=$(DYNAMIC_NATIVE_COMPILER)
	$(MAKE) -f Makefile.dl ENGINE=$(DL_ENGINE) ARCHIVE=$(DYNAMIC_ARCHIVE)
	$(MAKE) -f Makefile.dl ENGINE=$(DL_ENGINE) MPEG_VIDEO=$(DYNAMIC_MPEG_VIDEO)

	$(EE_CXX) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $(EE_BIN_DIR)tmp.elf $(EE_OBJS) $(EE_SRC_DIR)dummy-exports.c $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -Wno-write-strings $(EE_LIBS)
	./build-exports.sh
	$(EE_CXX) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o bin/athena_debug.elf $(EE_OBJS) $(EE_SRC_DIR)exports.c $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -fpermissive -Wno-write-strings $(EE_LIBS)
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
	$(MAKE) -C ee_modules/bearssl clean
	$(MAKE) -C ee_modules/mpeg clean
	$(MAKE) -C ee_modules/erl clean

	$(MAKE) -f Makefile.dl ENGINE=$(DL_ENGINE) KEYBOARD=1 clean
	$(MAKE) -f Makefile.dl MOUSE=1 clean
	$(MAKE) -f Makefile.dl NETWORK=1 clean
	$(MAKE) -f Makefile.dl NATIVE_COMPILER=1 clean
	$(MAKE) -f Makefile.dl ARCHIVE=1 clean
	$(MAKE) -f Makefile.dl MPEG_VIDEO=1 clean

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

# Build vendored liberl static library when present
ee_modules/erl/lib/liberl.a:
	$(MAKE) -C ee_modules/erl

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

# The .vsm files are committed: VCL (closed source, Linux x86 only) is not
# available everywhere, the CI included. Precious so make never deletes one as
# an intermediate of .vcl -> .vsm -> .o, and without VCL the committed .vsm is
# used as is -- a fresh checkout can easily leave a .vcl looking newer.
.PRECIOUS: $(EE_SRC_DIR)%.vsm

$(EE_SRC_DIR)%.vsm: $(EE_SRC_DIR)%.vcl | $(EE_SRC_DIR)
	$(DIR_GUARD)
	@if command -v $(EE_VCL) >/dev/null 2>&1; then 		echo VCL - $<; 		$(EE_VCL) -Isrc -g -t1 -o$@ $<; 	elif [ -f $@ ]; then 		echo "VCL - $< (no $(EE_VCL), using the committed $@)"; 		touch $@; 	else 		echo "error: $@ is missing and $(EE_VCL) is not installed to build it from $<"; 		exit 1; 	fi

$(EE_OBJ_DIR)%.o: $(EE_EMBED_DIR)%.c | $(EE_OBJ_DIR)
	@echo BIN2C - $<
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

# capp must override flags after Makefile.eeglobal expands EE_CFLAGS.
capp: ATHENA_JS=0
capp: EE_CFLAGS := $(filter-out -DATHENA_NATIVE_COMPILER -DATHENA_JS,$(EE_CFLAGS))
