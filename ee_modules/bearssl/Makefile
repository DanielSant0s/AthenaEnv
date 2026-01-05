EE_BASE := $(PS2SDK)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

SRC_ROOT := ../../src/BearSSL
SRC_DIR  := $(SRC_ROOT)/src
INC_DIR  := $(SRC_ROOT)/inc

OBJDIR := obj
LIBDIR := lib

DIRS := \
  $(SRC_DIR)/ssl \
  $(SRC_DIR)/x509 \
  $(SRC_DIR)/rsa \
  $(SRC_DIR)/ec \
  $(SRC_DIR)/hash \
  $(SRC_DIR)/mac \
  $(SRC_DIR)/hmac \
  $(SRC_DIR)/int \
  $(SRC_DIR)/pem \
  $(SRC_DIR)/rand \
  $(SRC_DIR)/prf \
  $(SRC_DIR)/aead \
  $(SRC_DIR)/block \
  $(SRC_DIR)/codec \
  $(SRC_DIR)/kdf \
  $(SRC_DIR)/symcipher

SRCS := $(foreach d,$(DIRS),$(wildcard $(d)/*.c))
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

TARGET := $(LIBDIR)/libbearssl.a

.PHONY: all clean dirs

all: dirs $(TARGET)

dirs:
	@mkdir -p $(LIBDIR)
	@mkdir -p $(OBJDIR)
	@for d in $(DIRS); do \
		reld=$${d#$(SRC_DIR)/}; \
		mkdir -p $(OBJDIR)/$$reld; \
	done

$(OBJDIR)/%.o: $(SRC_DIR)/%.c
	@echo CC - $<
	$(EE_CC) $(EE_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) -c $< -o $@

$(TARGET): $(OBJS)
	@echo AR - $@
	@mkdir -p $(LIBDIR)
	$(EE_AR) rcs $@ $(OBJS)

clean:
	rm -rf $(OBJDIR) $(LIBDIR)
