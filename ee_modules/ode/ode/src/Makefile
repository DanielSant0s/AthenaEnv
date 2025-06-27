EE_LIB_DIR=../../lib/
LIB_ODE=$(EE_LIB_DIR)libode.a
ERL_ODE=$(EE_LIB_DIR)libode.erl

EE_CFLAGS += -DDISABLE_TIMERS
EE_CFLAGS += -DNO_SIGNAL_H -DDEBUG_ERROR -DUSE_MATH_H
EE_CFLAGS += -DENABLE_PS2SDK -DdTRIMESH_ENABLED

ODE_OBJS  = fastdot.o fastldlt.o fastlsolve.o fastltsolve.o array.o collision_kernel.o \
	collision_quadtreespace.o collision_space.o collision_std.o collision_transform.o \
	collision_trimesh.o collision_trimesh_box.o collision_trimesh_ccylinder.o \
	collision_trimesh_distance.o collision_trimesh_ray.o collision_trimesh_sphere.o \
	collision_trimesh_trimesh.o collision_util.o error.o export-dif.o joint.o lcp.o \
	mass.o mat.o matrix.o memory.o misc.o obstack.o ode.o odemath.o quickstep.o \
	rotation.o step.o stepfast.o testing.o timer.o util.o 

EE_INCS  = -I../../include -I../../OPCODE

all: $(LIB_ODE) $(ERL_ODE)

$(LIB_ODE): $(ODE_OBJS)
	$(EE_AR) cru $(LIB_ODE) $(ODE_OBJS)

$(ERL_ODE): $(ODE_OBJS)
	$(DIR_GUARD)
	$(EE_CC) -nostartfiles -nostdlib $(EE_OPTFLAGS) -o $(ERL_ODE) $(ODE_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -Wl,-r -Wl,-d
	$(EE_STRIP) --strip-unneeded -R .mdebug.eabi64 -R .reginfo -R .comment $(ERL_ODE)

install: all
	mkdir -p $(DESTDIR)$(PS2SDK)/ports/include/ode
	mkdir -p $(DESTDIR)$(PS2SDK)/ports/lib
	cp -f $(LIB_ODE) $(DESTDIR)$(PS2SDK)/ports/lib
	cp -f ../../include/ode/*.h $(DESTDIR)$(PS2SDK)/ports/include/ode

clean:
	rm -f $(LIB_ODE) $(LIB_ODEMAIN) $(ODE_OBJS) $(ERL_ODE)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
