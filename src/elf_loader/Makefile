# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# (c) 2020 Francisco Javier Trujillo Mata <fjtrujy@gmail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

EE_OBJS = elf.o loader.o

BIN2C = $(PS2SDK)/bin/bin2c

all:: prepareLoader $(EE_LIB)

clean:: cleanLoader

prepareLoader:
	$(BIN2C) src/loader/loader.elf src/loader.c loader_elf

cleanLoader:
	$(MAKE) -C src/loader clean
