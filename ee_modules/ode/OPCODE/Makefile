EE_LIB_DIR=../lib/
LIB_OPCODE=$(EE_LIB_DIR)libopcode.a
ERL_OPCODE=$(EE_LIB_DIR)libopcode.erl

EE_CFLAGS += -DDISABLE_TIMERS
EE_CFLAGS += -DNO_SIGNAL_H -DDEBUG_ERROR -DDEBUG_VIDEO -DUSE_MATH_H
EE_CFLAGS += -DENABLE_PS2SDK

OPCODE_OBJS  = OPC_AABBCollider.o OPC_AABBTree.o OPC_BaseModel.o \
	OPC_BoxPruning.o OPC_Collider.o OPC_Common.o OPC_HybridModel.o \
	OPC_LSSCollider.o OPC_MeshInterface.o OPC_Model.o \
	OPC_OBBCollider.o OPC_OptimizedTree.o OPC_Picking.o \
	OPC_PlanesCollider.o OPC_RayCollider.o OPC_SphereCollider.o \
	OPC_SweepAndPrune.o OPC_TreeBuilders.o OPC_TreeCollider.o \
	OPC_VolumeCollider.o Opcode.o StdAfx.o

EE_INCS  = -I../include 

all: $(ERL_OPCODE) $(LIB_OPCODE) 

$(LIB_OPCODE): $(OPCODE_OBJS)
	$(EE_AR) cru $(LIB_OPCODE) $(OPCODE_OBJS)

$(ERL_OPCODE): $(OPCODE_OBJS)
	$(DIR_GUARD)
	$(EE_CC) -nostartfiles -nostdlib $(EE_OPTFLAGS) -o $(ERL_OPCODE) $(OPCODE_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -Wl,-r -Wl,-d
	$(EE_STRIP) --strip-unneeded -R .mdebug.eabi64 -R .reginfo -R .comment $(ERL_OPCODE)

install: all
	mkdir -p $(DESTDIR)$(PS2SDK)/ports/include/ode
	mkdir -p $(DESTDIR)$(PS2SDK)/ports/lib
	cp -f $(LIB_OPCODE) $(DESTDIR)$(PS2SDK)/ports/lib

clean:
	rm -f $(LIB_OPCODE) $(OPCODE_OBJS) $(ERL_OPCODE)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
