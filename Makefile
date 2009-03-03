# ------------------------------------------------------ #
# Makefile for the "Yamagi Quake 2 Client"               #
#                                                        #
# Just type "make" to compile the                        #
#  - SDL Client (quake2)                                 #
#  - Server (quake2ded)                                  #
#  - SDL Software-Renderer (ref_soft.so)                 #
#  - SDL OpenGL-Renderer (ref_gl.so)                     #
#  - games:                                              #
#    - Quake II (quake2)                                 #
#    - Quake II - The Reckoning (xatrix)                 #
#    - Quake II - Ground Zero (rogue)                    #
#    - Quake II - Threewave Capture The Flag (ctf)       #
#                                                        #
# Dependencies:                                          #
#  - SDL 1.2                                             #
#  - libX11                                              #
#  - libGL                                               #
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
$(error $(OSTYPE) is currently not supported by this Makefile.)
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
CFLAGS= -O2 -ffast-math -funroll-loops -falign-loops=2 \
		-falign-jumps=2 -falign-functions=2 -fno-strict-aliasing \
		-Wall -pipe
endif

ifeq ($(ARCH),x86_64)
CFLAGS = -O2 -ffast-math -funroll-loops -fomit-frame-pointer \
	   	 -fexpensive-optimizations -fno-strict-aliasing \
		 -Wall -pipe
endif

SDLCFLAGS = $(shell sdl-config --cflags)

# ----------

# The linker and linkerflags

# Linux
ifeq ($(OSTYPE),Linux)
LDFLAGS=-lm -ldl
endif

# FreeBSD
ifeq ($(OSTYPE),FreeBSD)
LDFLAGS=-lm
endif 

# SDL
SDLLDFLAGS=$(shell sdl-config --libs)

# ----------

# Targets

all: client

client:
	@-mkdir -p build \
		build/client \
		build/client/console \
		build/client/input \
		build/client/menu \
		build/client/sound \
		build/common \
		build/game \
		build/posix \
		build/server
	$(MAKE) build/client/quake2	

clean:
	@-rm -Rf build

# ----------

# The Client
QUAKE2_OBJS = \
	build/client/cl_cin.o \
	build/client/cl_ents.o \
	build/client/cl_fx.o \
	build/client/cl_input.o \
	build/client/cl_inv.o \
	build/client/cl_main.o \
	build/client/cl_parse.o \
	build/client/cl_pred.o \
	build/client/cl_tent.o \
	build/client/cl_scrn.o \
	build/client/cl_view.o \
	build/client/cl_newfx.o \
	build/client/console/console.o \
	build/client/input/keys.o \
	build/client/menu/menu.o \
	build/client/menu/qmenu.o \
	build/client/sound/snd_dma.o \
	build/client/sound/snd_mem.o \
	build/client/sound/snd_mix.o \
	build/common/cmd.o \
	build/common/cmodel.o \
	build/common/common.o \
	build/common/crc.o \
	build/common/cvar.o \
	build/common/files.o \
	build/common/md4.o \
	build/common/net_chan.o \
	build/common/pmove.o \
	build/game/m_flash.o \
	build/game/q_shared.o \
	build/server/sv_ccmds.o \
	build/server/sv_ents.o \
	build/server/sv_game.o \
	build/server/sv_init.o \
	build/server/sv_main.o \
	build/server/sv_send.o \
	build/server/sv_user.o \
	build/server/sv_world.o \
	build/posix/cd_sdl.o \
	build/posix/glob.o \
	build/posix/net_udp.o \
	build/posix/q_shlinux.o \
	build/posix/snd_sdl.o \
	build/posix/sys_linux.o \
	build/posix/vid_menu.o \
	build/posix/vid_so.o 

build/client/quake2 : $(QUAKE2_OBJS)
	$(CC) $(CFLAGS) -o $@ $(QUAKE2_OBJS) $(LDFLAGS) $(SDLLDFLAGS)

build/client/cl_cin.o :     src/client/cl_cin.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cl_ents.o :    src/client/cl_ents.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cl_fx.o :      src/client/cl_fx.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cl_input.o :   src/client/cl_input.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cl_inv.o :     src/client/cl_inv.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cl_main.o :    src/client/cl_main.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cl_parse.o :   src/client/cl_parse.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cl_pred.o :    src/client/cl_pred.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cl_tent.o :    src/client/cl_tent.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cl_scrn.o :    src/client/cl_scrn.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cl_view.o :    src/client/cl_view.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cl_newfx.o :   src/client/cl_newfx.c
	$(CC) $(CFLAGS) -o $@ -c $<  

build/client/console/console.o :    src/client/console/console.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/input/keys.o :       src/client/input/keys.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/menu/menu.o :       src/client/menu/menu.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/menu/qmenu.o :      src/client/menu/qmenu.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/sound/snd_dma.o :    src/client/sound/snd_dma.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/sound/snd_mem.o :    src/client/sound/snd_mem.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/sound/snd_mix.o :    src/client/sound/snd_mix.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/common/cmd.o :        src/common/cmd.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/common/cmodel.o :     src/common/cmodel.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/common/common.o :     src/common/common.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/common/crc.o :        src/common/crc.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/common/cvar.o :       src/common/cvar.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/common/files.o :      src/common/files.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/common/md4.o :        src/common/md4.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/common/net_chan.o :   src/common/net_chan.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/common/pmove.o :      src/common/pmove.c
	$(CC) $(CFLAGS) -o $@ -c $< 
 
build/game/m_flash.o :    src/game/quake2/m_flash.c
	$(CC) $(CFLAGS) -o $@ -c $< 
 
build/game/q_shared.o :   src/game/quake2/q_shared.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/server/sv_ccmds.o :   src/server/sv_ccmds.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/server/sv_ents.o :    src/server/sv_ents.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/server/sv_game.o :    src/server/sv_game.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/server/sv_init.o :    src/server/sv_init.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/server/sv_main.o :    src/server/sv_main.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/server/sv_send.o :    src/server/sv_send.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/server/sv_user.o :    src/server/sv_user.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/server/sv_world.o :   src/server/sv_world.c
	$(CC) $(CFLAGS) -o $@ -c $< 
 
build/posix/cd_sdl.o :     src/platforms/posix/cd_sdl.c
	$(CC) $(CFLAGS) -o $@ -c $< $(SDLCFLAGS)
 
build/posix/glob.o :       src/platforms/posix/glob.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/posix/net_udp.o :   src/platforms/posix/net_udp.c
	$(CC) $(CFLAGS) -o $@ -c $< 
 
build/posix/q_shlinux.o :  src/platforms/posix/q_shlinux.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/posix/snd_sdl.o :    src/platforms/posix/snd_sdl.c
	$(CC) $(CFLAGS) -o $@ -c $< $(SDLCFLAGS)
 
build/posix/sys_linux.o :  src/platforms/posix/sys_linux.c
	$(CC) $(CFLAGS) -o $@ -c $< 
 
build/posix/vid_menu.o :   src/platforms/posix/vid_menu.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/posix/vid_so.o :     src/platforms/posix/vid_so.c
	$(CC) $(CFLAGS) -o $@ -c $< 

