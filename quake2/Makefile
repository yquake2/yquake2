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
	build/client/console.o \
	build/client/keys.o \
	build/client/menu.o \
	build/client/qmenu.o \
	build/client/snd_dma.o \
	build/client/snd_mem.o \
	build/client/snd_mix.o \
	build/client/cmd.o \
	build/client/cmodel.o \
	build/client/common.o \
	build/client/crc.o \
	build/client/cvar.o \
	build/client/files.o \
	build/client/md4.o \
	build/client/net_chan.o \
	build/client/pmove.o \
	build/client/m_flash.o \
	build/client/q_shared.o \
	build/client/sv_ccmds.o \
	build/client/sv_ents.o \
	build/client/sv_game.o \
	build/client/sv_init.o \
	build/client/sv_main.o \
	build/client/sv_send.o \
	build/client/sv_user.o \
	build/client/sv_world.o \
	build/client/cd_sdl.o \
	build/client/glob.o \
	build/client/net_udp.o \
	build/client/q_shlinux.o \
	build/client/snd_sdl.o \
	build/client/sys_linux.o \
	build/client/vid_menu.o \
	build/client/vid_so.o 

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

build/client/console.o :    src/client/console.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/keys.o :       src/client/keys.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/menu.o :       src/client/menu.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/qmenu.o :      src/client/qmenu.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/snd_dma.o :    src/client/snd_dma.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/snd_mem.o :    src/client/snd_mem.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/snd_mix.o :    src/client/snd_mix.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cmd.o :        src/common/cmd.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cmodel.o :     src/common/cmodel.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/common.o :     src/common/common.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/crc.o :        src/common/crc.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/cvar.o :       src/common/cvar.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/files.o :      src/common/files.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/md4.o :        src/common/md4.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/net_chan.o :   src/common/net_chan.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/pmove.o :      src/common/pmove.c
	$(CC) $(CFLAGS) -o $@ -c $< 
 
build/client/m_flash.o :    src/game/quake2/m_flash.c
	$(CC) $(CFLAGS) -o $@ -c $< 
 
build/client/q_shared.o :   src/game/quake2/q_shared.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/sv_ccmds.o :   src/server/sv_ccmds.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/sv_ents.o :    src/server/sv_ents.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/sv_game.o :    src/server/sv_game.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/sv_init.o :    src/server/sv_init.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/sv_main.o :    src/server/sv_main.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/sv_send.o :    src/server/sv_send.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/sv_user.o :    src/server/sv_user.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/sv_world.o :   src/server/sv_world.c
	$(CC) $(CFLAGS) -o $@ -c $< 
 
build/client/cd_sdl.o :     src/platforms/posix/cd_sdl.c
	$(CC) $(CFLAGS) -o $@ -c $< $(SDLCFLAGS)
 
build/client/glob.o :       src/platforms/posix/glob.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/net_udp.o :   src/platforms/posix/net_udp.c
	$(CC) $(CFLAGS) -o $@ -c $< 
 
build/client/q_shlinux.o :  src/platforms/posix/q_shlinux.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/snd_sdl.o :    src/platforms/posix/snd_sdl.c
	$(CC) $(CFLAGS) -o $@ -c $< $(SDLCFLAGS)
 
build/client/sys_linux.o :  src/platforms/posix/sys_linux.c
	$(CC) $(CFLAGS) -o $@ -c $< 
 
build/client/vid_menu.o :   src/platforms/posix/vid_menu.c
	$(CC) $(CFLAGS) -o $@ -c $< 

build/client/vid_so.o :     src/platforms/posix/vid_so.c
	$(CC) $(CFLAGS) -o $@ -c $< 

