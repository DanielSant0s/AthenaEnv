# Building an AthenaEnv binary

AthenaEnv is normally distributed on Releases tab or GitHub artifacts for dev binaries, however, there's some relevance to mantain a build instruction manual for customization and preservation purposes.

## What do you need

### Essential components
* [Personal Computer](https://en.wikipedia.org/wiki/Personal_computer)
* [ps2dev](https://github.com/ps2dev/ps2dev)
* [ps2-packer](https://github.com/ps2dev/ps2-packer)

### Optional components
* [Vector Unit Command Line](https://ps2linux.no-ip.info/playstation2-linux.com/projects/vcl.html) - P.S.: VCL is a 32bit binary and depends on [GASP](https://github.com/matrach/gasp). It is used to compile AthenaEnv VU1 microprograms. It can compile without VCL, but you can't edit AthenaEnv VU1 microprograms without it.

_P.S.: Install and usage instructions are inside their pages._

## Compiling
Once you have PS2DEV environment working on your computer, you can compile AthenaEnv.  
  
AthenaEnv is easily compilable with a single command ```make```. Two AthenaEnv binary variants will be generated at bin folder.  
* **athena.elf** - Athena _default_ binary
* **athena_pkd.elf** - Athena binary _compressed_ variant. Proper to be used inside Memory Card or other low storage devices.

`make` also builds the optional subsystems that stay out of the ELF by default, as relocatable modules in `bin/`:

* **athena_network.erl** — `Network`, `Request`, `Socket`, `WebSocket`
* **athena_native.erl** — `Native`
* **athena_archive.erl** — `Archive`
* **athena_mpeg.erl** — `Video`

A script loads one with `import` (see [Module System](MODULE_SYSTEM.md)). Setting the matching `STATIC_*` flag to `1` links that subsystem into the ELF instead and turns its `DYNAMIC_*` flag off.
  
In addition, you can just type ```make clean``` to remove compilation cache resources.

## The bin folder and the examples
`bin/` is generated: it is what goes on the memory card or USB drive. `make` builds the ELFs and modules into it, and ```make dist``` copies the shell, the apps and their assets there from `examples/`:

| Source | Ends up in | What |
|--------|------------|------|
| `examples/shell/` | `bin/` | AthenaOS (`main.js`, `main.lua`, `athenaos/`) and `athena.ini` |
| `examples/shared/` | `bin/` | Assets several apps use (`render/`, `fonts/`, ...) |
| `examples/<app>/` | `bin/apps/<app>/` | `app.json`, `main.js`, `main.lua`, `main.c` (marks a C version, built into `athena_demos.elf` by `make demos`) |
| `examples/<app>/assets/` | `bin/` | Assets only that app uses |

Apps run from the boot folder, like the shell, so an app's asset paths are relative to `bin/` whether the file came from `shared/` or from the app's own `assets/`. `app.json` holds what the shell lists (name, author, version, category, blurb, icon, exit button); a `"lua"` or `"js"` object overrides fields for that language. `examples/<app>/main.c` is the app's C version (see [C API](C_API.md)).

Only newer files are copied, and saved data in `bin/` (`athena.ini`, settings, saves) is never overwritten. ```make dist DIST_FLAGS=--clean``` empties `bin/` first, keeping the ELFs, the modules and `athena.ini`.

## Release packages
`scripts/package.sh js lua demos` zips a built `bin/` into `dist/` (each zip unpacks into an `AthenaEnv/` folder). The CI builds and publishes the same three:

| Package | Contents |
|---------|----------|
| `AthenaEnv-js.zip` | `athena.elf`, `athena_pkd.elf`, the JavaScript `.erl` modules and a hello world `main.js` with its `athena.ini` (from `examples/starter/js/`) |
| `AthenaEnv-lua.zip` | `athena_lua.elf`, `athena_lua_pkd.elf`, the Lua `.erl` modules and a hello world `main.lua` with its `athena.ini` (from `examples/starter/lua/`) |
| `AthenaEnv-demos.zip` | `bin/` after `make dist`: AthenaOS, every example in JavaScript and Lua, and the C versions in `athena_demos.elf` |

Build `bin/` the way the CI does before packaging: `make`, `make SCRIPT_ENGINE=lua`, `make demos` for the C apps, then `make dist`.

`make capp` and `make demos` compile into `obj/c/`, not `obj/`: a C build links the network, archive and MPEG modules in, which the script builds load as `.erl` modules, so the two can't share objects.

## Customizing compilation
AthenaEnv has some flags and variables that can be change when compiling as command line arguments, which let you to customize athena final binaries.
* EE_BIN_PREF - Binary name prefix. Default: athena
* RESET_IOP - Enable IOP reset inside the binary. Default: 1
* DEBUG - Enable debug level logging. Default: 0
* EE_SIO - Redirect all print calls to EE Serial. Default: 0
* PADEMU - Enable support for DualShock 3 and 4 and include its resources. Default: 1
* GRAPHICS - Enable graphics and include its resources. Default: 1
* PHYSICS - Enable VU0/MMI hardware-accelerated physics and collision engine. Default: 1
* AUDIO - Enable audio and include its resources. Default: 1
* STATIC_KEYBOARD - Link keyboard into the ELF. Also disables DYNAMIC_KEYBOARD. Default: 1
* STATIC_MOUSE - Link mouse into the ELF. Also disables DYNAMIC_MOUSE. Default: 1
* STATIC_NETWORK - Link network into the ELF. Also disables DYNAMIC_NETWORK. Default: 0
* STATIC_ARCHIVE - Link archive (zip/gz/tar) into the ELF. Also disables DYNAMIC_ARCHIVE. Default: 0
* STATIC_NATIVE_COMPILER - Link the AOT compiler into the ELF. Also disables DYNAMIC_NATIVE_COMPILER. Default: 0
* STATIC_MPEG_VIDEO - Link MPEG video into the ELF. Also disables DYNAMIC_MPEG_VIDEO. Default: 0
* STATIC_CAMERA - Enable EyeToy and include its resources. Default: 0
* DYNAMIC_KEYBOARD - Build `athena_keyboard.erl` instead of linking keyboard into the ELF. Default: 0
* DYNAMIC_MOUSE - Build `athena_mouse.erl` instead of linking mouse into the ELF. Default: 0
* DYNAMIC_NETWORK - Build `athena_network.erl`. Default: 1
* DYNAMIC_ARCHIVE - Build `athena_archive.erl`. Default: 1
* DYNAMIC_NATIVE_COMPILER - Build `athena_native.erl`. Default: 1
* DYNAMIC_MPEG_VIDEO - Build `athena_mpeg.erl`. Default: 1

`liberl` and `libmpeg` are vendored under `src/liberl` and `src/libmpeg` and built into `ee_modules/` when those trees are present. The host ELF exports the symbols those `.erl` files import. A new libc or zlib call inside an `.erl` has to be a symbol the host already exports, or the reloc stays `0` and the EE jumps to a null address.

### Usage

Debug on the EE serial port, leave network out of the image, and drop DualShock 3/4 support:
```shell
make DYNAMIC_NETWORK=0 EE_SIO=1 PADEMU=0
```
Link network into the ELF instead of shipping `athena_network.erl`:
```shell
make STATIC_NETWORK=1
```