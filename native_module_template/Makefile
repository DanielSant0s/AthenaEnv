EE_ERL = sample_module.erl
EE_OBJS = main.o

EE_INCS += -I$(ATHENAENV_SRC)/include -I$(ATHENAENV_SRC)/quickjs

all: $(EE_OBJS)
	$(DIR_GUARD)
	$(EE_CC) -nostartfiles -nostdlib $(EE_OPTFLAGS) -o $(EE_ERL) $(EE_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -Wl,-r -Wl,-d
	$(EE_STRIP) --strip-unneeded -R .mdebug.eabi64 -R .reginfo -R .comment $(EE_ERL)

clean:
	rm -f $(EE_ERL) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
