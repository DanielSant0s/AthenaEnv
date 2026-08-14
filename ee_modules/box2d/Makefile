EE_BASE := $(PS2SDK)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

SRC_ROOT := ../../src/Box2d
SRC_DIR  := $(SRC_ROOT)/src
INC_DIR  := $(SRC_ROOT)/include

OBJDIR := obj
LIBDIR := lib

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

TARGET := $(LIBDIR)/libbox2d.a

.PHONY: all clean dirs

all: dirs $(TARGET)

dirs:
	@mkdir -p $(LIBDIR)
	@mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SRC_DIR)/%.c
	@echo CC - $<
	$(EE_CC) $(EE_CFLAGS) -Wno-incompatible-pointer-types -Wno-error=incompatible-pointer-types -fpermissive -DBOX2D_DISABLE_SIMD -I$(INC_DIR) -I$(SRC_DIR) -c $< -o $@

$(TARGET): $(OBJS)
	@echo AR - $@
	@mkdir -p $(LIBDIR)
	$(EE_AR) rcs $@ $(OBJS)

clean:
	rm -rf $(OBJDIR) $(LIBDIR)
