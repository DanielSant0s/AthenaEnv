EE_BASE := $(PS2SDK)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

SRC_ROOT := ../../src/liberl
INC_DIR  := $(SRC_ROOT)/include

OBJDIR := obj
LIBDIR := lib

C_SRCS := $(wildcard $(SRC_ROOT)/*.c)
OBJS   := $(patsubst $(SRC_ROOT)/%.c,$(OBJDIR)/%.o,$(C_SRCS))

TARGET := $(LIBDIR)/liberl.a

.PHONY: all clean dirs

all: dirs $(TARGET)

dirs:
	@mkdir -p $(LIBDIR)
	@mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SRC_ROOT)/%.c
	@echo CC - $<
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -I$(INC_DIR) -I$(SRC_ROOT) -c $< -o $@

$(TARGET): $(OBJS)
	@echo AR - $@
	@mkdir -p $(LIBDIR)
	$(EE_AR) rcs $@ $(OBJS)

clean:
	rm -rf $(OBJDIR) $(LIBDIR)
