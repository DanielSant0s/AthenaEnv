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
* **athena.elf** - Athena _default_ binary, with all specified functions
* **athena_pkd.elf** - Athena binary _compressed_ variant. Proper to be used inside Memory Card or other low storage devices. 
  
In addition, you can just type ```make clean``` to remove compilation cache resources.

## Customizing compilation
AthenaEnv has some flags and variables that can be change when compiling as command line arguments, which let you to customize athena final binaries.
* EE_BIN_PREF - Binary name prefix. Default: athena
* RESET_IOP - Enable IOP reset inside the binary. Default: 1
* DEBUG - Enable debug level logging. Default: 0
* EE_SIO - Redirect all print calls to EE Serial. Default: 0
* PADEMU - Enable support for DualShock 3 and 4 and include its resources. Default: 1
* GRAPHICS - Enable graphics and include its resources. Default: 1
* AUDIO - Enable audio and include its resources. Default: 1
* STATIC_KEYBOARD - Enable keyboard and include its resources inside the binary file, also disables DYNAMIC_KEYBOARD. Default: 1
* STATIC_MOUSE - Enable mouse and include its resources inside the binary file, also disables DYNAMIC_MOUSE. Default: 1
* STATIC_NETWORK - Enable network and include its resources. Default: 1
* STATIC_CAMERA - Enable EyeToy and include its resources. Default: 0
* DYNAMIC_KEYBOARD - Enable keyboard and include its resources outside the binary file. Default: 0
* DYNAMIC_MOUSE - Enable mouse and include its resources outside the binary file. Default: 0

### Usage

Let's get some example. I want to debug on EE serial, disable network and pademu to decrease RAM usage:
```shell
make STATIC_NETWORK=0 EE_SIO=1 PADEMU=0
```
That's it!