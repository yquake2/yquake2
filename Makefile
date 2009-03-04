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
CFLAGS_BASE = -O2 -ffast-math -funroll-loops -falign-loops=2 \
		-falign-jumps=2 -falign-functions=2 -fno-strict-aliasing \
		-Wall -pipe
endif

ifeq ($(ARCH),x86_64)
CFLAGS_BASE = -O2 -ffast-math -funroll-loops -fomit-frame-pointer \
	   	 -fexpensive-optimizations -fno-strict-aliasing \
		 -Wall -pipe
endif

# SDL
SDLCFLAGS = $(shell sdl-config --cflags)

# Client
CFLAGS_CLIENT = $(CFLAGS_BASE)

# Dedicated Server
CFLAGS_DEDICATED_SERVER = $(CFLAGS_BASE)
CFLAGS_DEDICATED_SERVER += -DDEDICATED_ONLY

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
		build/gameabi \
		build/posix \
		build/posix/glob \
		build/posix/sdl \
		build/server
	$(MAKE) build/client/quake2	

dedicated_server:
	@-mkdir -p build \
		build/dedicated_server \
		build/dedicated_server_common \
		build/dedicated_server_posix 
	$(MAKE) build/dedicated_server/q2ded

clean:
	@-rm -Rf build

# ----------

# Client object
CLIENT_OBJS = \
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
	build/client/sound/snd_mix.o

# ---------

# Common objects
COMMON_OBJS = \
	build/common/cmd.o \
	build/common/cmodel.o \
	build/common/common.o \
	build/common/crc.o \
	build/common/cvar.o \
	build/common/files.o \
	build/common/md4.o \
	build/common/net_chan.o \
	build/common/pmove.o 
 
# ----------

# Game ABI objets
GAME_ABI_OBJS = \
	build/gameabi/m_flash.o \
	build/gameabi/q_shared.o

# ----------

# Server objects
SERVER_OBJS = \
	build/server/sv_ccmds.o \
	build/server/sv_ents.o \
	build/server/sv_game.o \
	build/server/sv_init.o \
	build/server/sv_main.o \
	build/server/sv_send.o \
	build/server/sv_user.o \
	build/server/sv_world.o

# ---------

# POSIX platform objects
POSIX_OBJS = \
 	build/posix/net_udp.o \
	build/posix/posix.o \
	build/posix/sys_linux.o \
	build/posix/vid_menu.o \
	build/posix/vid_so.o \
    build/posix/glob/glob.o \
	build/posix/sdl/cd.o \
	build/posix/sdl/sound.o

# ----------

# Dedicated server object
DEDICATED_SERVER_OBJS = \
	build/dedicated_server/sv_ccmds.o \
	build/dedicated_server/sv_ents.o \
	build/dedicated_server/sv_game.o \
	build/dedicated_server/sv_init.o \
	build/dedicated_server/sv_main.o \
	build/dedicated_server/sv_send.o \
	build/dedicated_server/sv_user.o \
	build/dedicated_server/sv_world.o

# ----------

# Dedicated server common objects
DEDICATED_SERVER_COMMON_OBJS = \
	build/dedicated_server_common/cmd.o \
	build/dedicated_server_common/cmodel.o \
	build/dedicated_server_common/common.o \
	build/dedicated_server_common/crc.o \
	build/dedicated_server_common/cvar.o \
	build/dedicated_server_common/files.o \
	build/dedicated_server_common/md4.o \
	build/dedicated_server_common/net_chan.o \
	build/dedicated_server_common/pmove.o 

# ----------

# Dedicated server POSIX platform objects
DEDICATED_SERVER_POSIX_OBJS = \
	build/dedicated_server_posix/glob.o \
	build/dedicated_server_posix/net_udp.o \
	build/dedicated_server_posix/posix.o \
	build/dedicated_server_posix/sys_linux.o
 
# ----------

# Client build
build/client/cl_cin.o :     		src/client/cl_cin.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_ents.o :    		src/client/cl_ents.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_fx.o :      		src/client/cl_fx.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_input.o :   		src/client/cl_input.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_inv.o :     		src/client/cl_inv.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_main.o :    		src/client/cl_main.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_parse.o :   		src/client/cl_parse.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_pred.o :    		src/client/cl_pred.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_tent.o :    		src/client/cl_tent.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_scrn.o :    		src/client/cl_scrn.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_view.o :    		src/client/cl_view.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_newfx.o :   		src/client/cl_newfx.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<  

build/client/console/console.o :	src/client/console/console.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<

build/client/input/keys.o :			src/client/input/keys.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/menu/menu.o :			src/client/menu/menu.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/menu/qmenu.o :			src/client/menu/qmenu.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/sound/snd_dma.o :		src/client/sound/snd_dma.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/sound/snd_mem.o :		src/client/sound/snd_mem.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/sound/snd_mix.o :		src/client/sound/snd_mix.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

# ---------

# Common build
build/common/cmd.o :		        src/common/cmd.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<

build/common/cmodel.o :     		src/common/cmodel.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<

build/common/common.o :     		src/common/common.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<

build/common/crc.o :        		src/common/crc.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<

build/common/cvar.o :       		src/common/cvar.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/common/files.o :      		src/common/files.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/common/md4.o :        		src/common/md4.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/common/net_chan.o :   		src/common/net_chan.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/common/pmove.o :      		src/common/pmove.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

# ----------

# Game ABI build
build/gameabi/m_flash.o :  			src/game/quake2/m_flash.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
 
build/gameabi/q_shared.o : 			src/game/quake2/q_shared.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

# ---------

# Server build
build/server/sv_ccmds.o :			src/server/sv_ccmds.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/server/sv_ents.o :			src/server/sv_ents.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/server/sv_game.o :    		src/server/sv_game.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/server/sv_init.o :    		src/server/sv_init.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/server/sv_main.o :    		src/server/sv_main.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/server/sv_send.o :    		src/server/sv_send.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/server/sv_user.o :    		src/server/sv_user.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/server/sv_world.o :   		src/server/sv_world.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

# ----------

# POSIX build
build/posix/net_udp.o :   			src/platforms/posix/net_udp.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
 
build/posix/posix.o :	  			src/platforms/posix/posix.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/posix/sys_linux.o :  			src/platforms/posix/sys_linux.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
 
build/posix/vid_menu.o :   			src/platforms/posix/vid_menu.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/posix/vid_so.o :     			src/platforms/posix/vid_so.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/posix/glob/glob.o :  			src/platforms/posix/glob/glob.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/posix/sdl/cd.o :     			src/platforms/posix/sdl/cd.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< $(SDLCFLAGS)

build/posix/sdl/sound.o :  			src/platforms/posix/sdl/sound.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< $(SDLCFLAGS)
 
# ----------
 
# Dedicated server build
build/dedicated_server/sv_ccmds.o :			src/server/sv_ccmds.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_ents.o :			src/server/sv_ents.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_game.o :    		src/server/sv_game.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_init.o :    		src/server/sv_init.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_main.o :    		src/server/sv_main.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_send.o :    		src/server/sv_send.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_user.o :    		src/server/sv_user.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_world.o :   		src/server/sv_world.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

# ---------
 
# Dedicated server common build
build/dedicated_server_common/cmd.o :        src/common/cmd.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/cmodel.o :     src/common/cmodel.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/common.o :     src/common/common.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/crc.o :        src/common/crc.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/cvar.o :       src/common/cvar.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/files.o :      src/common/files.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/md4.o :        src/common/md4.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/net_chan.o :  src/common/net_chan.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/pmove.o :     src/common/pmove.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

# ----------

# Dedicated server POSIX build
build/dedicated_server_posix/glob.o :       src/platforms/posix/glob.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_posix/net_udp.o :   	src/platforms/posix/net_udp.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 
 
build/dedicated_server_posix/q_shlinux.o :  src/platforms/posix/q_shlinux.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_posix/sys_linux.o :  src/platforms/posix/sys_linux.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

# ----------
  
#  The client
build/client/quake2 : $(CLIENT_OBJS) $(COMMON_OBJS) $(GAME_ABI_OBJS) \
    $(SERVER_OBJS) $(POSIX_OBJS)
	$(CC) $(CFLAGS_CLIENT) -o $@ $(CLIENT_OBJS) $(COMMON_OBJS) $(GAME_ABI_OBJS) \
		$(SERVER_OBJS) $(POSIX_OBJS) $(LDFLAGS) $(SDLLDFLAGS)

# Dedicated Server
build/dedicated_server/q2ded : $(DEDICATED_SERVER_OBJS) $(DEDICATED_SERVER_COMMON_OBJS) \
	$(GAME_ABI_OBJS) $(DEDICATED_SERVER_POSIX_OBJS)
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ $(DEDICATED_SERVER_OBJS) \
		$(DEDICATED_SERVER_COMMON_OBJS) $(GAME_ABI_OBJS) \
		$(DEDICATED_SERVER_POSIX_OBJS) $(LDFLAGS)
