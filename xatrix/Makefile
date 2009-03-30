# ------------------------------------------------------ #
# Makefile for the "The Reckoning" missionpack           #
#                                                        #
# Platforms:                                             #
#  - Linux                                               #
#  - FreeBSD                                             #
#  - Maybe any other POSIX compliant system              #
#    supported by SDL 1.2                                #
# ------------------------------------------------------ #

# Check the OS type
OSTYPE := $(shell uname -s)

ifneq ($(OSTYPE),Linux)
ifneq ($(OSTYPE),FreeBSD)
#$(error $(OSTYPE) is currently not supported by this Client.)
endif
endif

# ----------

ARCH := $(shell uname -m | sed -e s/i.86/i386/ -e s/amd64/x86_64/)

ifneq ($(ARCH),i386)
ifneq ($(ARCH),x86_64)
$(error arch $(ARCH) is currently not supported)
endif
endif

# ----------

# The compiler and compiler flags

CC = gcc

ifeq ($(ARCH),i386)
CFLAGS = -O2 -ffast-math -funroll-loops -falign-loops=2 \
		-falign-jumps=2 -falign-functions=2 -fno-strict-aliasing \
		-Wall -pipe -fPIC -g
endif

ifeq ($(ARCH),x86_64)
CFLAGS = -O2 -ffast-math -funroll-loops -fomit-frame-pointer \
	   	 -fexpensive-optimizations -fno-strict-aliasing \
		 -Wall -pipe -fPIC -g
endif

# Optimizations
#   ~25% - 30% perfomance gain, but may not
#   work on all CPUs. Adjust to your needs
# CFLAFS_BASE += -mmmx  -msse  -msse2 -msse3 -m3dnow 

# ----------

# The linker and linkerflags

# Linux
ifeq ($(OSTYPE),Linux)
LDFLAGS=-lm -ldl -shared
endif

# FreeBSD
ifeq ($(OSTYPE),FreeBSD)
LDFLAGS=-lm -shared
endif 

# ----------

# Targets

all: xatrix

xatrix:
	@-mkdir -p build \
		release
	$(MAKE) release/game.so

clean:
	@-rm -Rf build release

# ----------

# Quake II - The Reckoning Object
XATRIX_OBJS = \
    build/g_ai.o \
	build/g_chase.o \
	build/g_cmds.o \
	build/g_combat.o \
	build/g_func.o \
	build/g_items.o \
	build/g_main.o \
	build/g_misc.o \
	build/g_monster.o \
	build/g_phys.o \
	build/g_save.o \
	build/g_spawn.o \
	build/g_svcmds.o \
	build/g_target.o \
	build/g_trigger.o \
	build/g_turret.o \
	build/g_utils.o \
	build/g_weapon.o \
	build/m_actor.o \
	build/m_berserk.o \
	build/m_boss2.o \
	build/m_boss3.o \
	build/m_boss31.o \
	build/m_boss32.o \
	build/m_boss5.o \
	build/m_brain.o \
	build/m_chick.o \
	build/m_fixbot.o \
	build/m_flash.o \
	build/m_flipper.o \
	build/m_float.o \
	build/m_flyer.o \
	build/m_gekk.o \
	build/m_gladb.o \
	build/m_gladiator.o \
	build/m_gunner.o \
	build/m_hover.o \
	build/m_infantry.o \
	build/m_insane.o \
	build/m_medic.o \
	build/m_move.o \
	build/m_mutant.o \
	build/m_parasite.o \
	build/m_soldier.o \
	build/m_supertank.o \
	build/m_tank.o \
	build/p_client.o \
	build/p_hud.o \
	build/p_trail.o \
	build/p_view.o \
	build/p_weapon.o \
	build/q_shared.o 

# ----------

# Xatrix build
build/g_ai.o:					src/g_ai.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_chase.o:   				src/g_chase.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_cmds.o:					src/g_cmds.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_combat.o:  				src/g_combat.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_func.o:					src/g_func.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_items.o:   				src/g_items.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_main.o:					src/g_main.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_misc.o:					src/g_misc.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_monster.o: 				src/g_monster.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_phys.o:					src/g_phys.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_save.o:					src/g_save.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_spawn.o:	   			src/g_spawn.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_svcmds.o:      			src/g_svcmds.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_target.o:      			src/g_target.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_trigger.o:     			src/g_trigger.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_turret.o:      			src/g_turret.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_utils.o:	   			src/g_utils.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/g_weapon.o:  				src/g_weapon.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_actor.o:	   			src/m_actor.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_berserk.o:		   		src/m_berserk.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_boss2.o:	   			src/m_boss2.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_boss3.o:	   			src/m_boss3.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_boss31.o:		   		src/m_boss31.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_boss32.o:		   		src/m_boss32.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_boss5.o:	   	   		src/m_boss5.c
	$(CC) $(CFLAGS) -o $@ -c $<
 
build/m_brain.o:	   	 	  	src/m_brain.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/m_chick.o:	   			src/m_chick.c
	$(CC) $(CFLAGS) -o $@ -c $<
 
build/m_fixbot.o:		   		src/m_fixbot.c
	$(CC) $(CFLAGS) -o $@ -c $<
 
build/m_flash.o:	   			src/m_flash.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_flipper.o:		   		src/m_flipper.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_float.o:	   			src/m_float.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_flyer.o:	   			src/m_flyer.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_gekk.o:			   		src/m_gekk.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_gladb.o	:		   		src/m_gladb.c
	$(CC) $(CFLAGS) -o $@ -c $<
 
build/m_gladiator.o:   	   		src/m_gladiator.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_gunner.o:		   		src/m_gunner.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/m_hover.o:	   	   		src/m_hover.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_infantry.o:		   		src/m_infantry.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_insane.o:		   		src/m_insane.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_medic.o:	   			src/m_medic.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_move.o:					src/m_move.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_mutant.o:		   		src/m_mutant.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_parasite.o:		   		src/m_parasite.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/m_soldier.o:		   		src/m_soldier.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_supertank.o:   	   		src/m_supertank.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/m_tank.o:					src/m_tank.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/p_client.o:      			src/p_client.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/p_hud.o:					src/p_hud.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/p_trail.o:	   			src/p_trail.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/p_view.o:					src/p_view.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/p_weapon.o:      			src/p_weapon.c
	$(CC) $(CFLAGS) -o $@ -c $<

build/q_shared.o:      			src/q_shared.c
	$(CC) $(CFLAGS) -o $@ -c $<
 
# ----------

# Quake II
release/game.so : $(XATRIX_OBJS)
	$(CC) $(CFLAGS) -o $@ $(XATRIX_OBJS) $(LDFLAGS)

