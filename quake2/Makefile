# ------------------------------------------------------ #
# Makefile for the "Yamagi Quake 2 Client"               #
#                                                        #
# Just type "make" to compile the                        #
#  - SDL Client (quake2)                                 #
#  - Server (q2ded)                                      #
#  - SDL OpenGL-Refresher (ref_gl.so)                    #
#  - games:                                              #
#    - Quake II (quake2)                                 #
#    - Quake II - Threewave Capture The Flag (ctf)       #
#                                                        #
# Dependencies:                                          #
#  - SDL 1.2                                             #
#  - libX11                                              #
#  - libGL                                               #
#  - libvorbis                                           #
#  - libogg                                              #
#  - zlib                                                #
#                                                        #
# Platforms:                                             #
#  - Linux                                               #
#  - FreeBSD                                             #
#  - Maybe any other POSIX compliant system              #
#    supported by SDL 1.2                                #
# ------------------------------------------------------ #

# Check the OS type
OSTYPE := $(shell uname -s)

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
CFLAGS_BASE = -O2  -fno-strict-aliasing -fomit-frame-pointer \
		 -fstack-protector-all -Wall -pipe -g
endif

ifeq ($(ARCH),x86_64)
CFLAGS_BASE = -O2 -fomit-frame-pointer -fno-strict-aliasing \
		 -fstack-protector-all -Wall -pipe -g
endif

# OGG/Vorbis
OGGCFLAGS = -I/usr/include -I/usr/local/include

# SDL
SDLCFLAGS := $(shell sdl-config --cflags)

# Client
CFLAGS_CLIENT = $(CFLAGS_BASE)

# Dedicated Server
CFLAGS_DEDICATED_SERVER = $(CFLAGS_BASE)
CFLAGS_DEDICATED_SERVER += -DDEDICATED_ONLY

# OpenGL refresher
CFLAGS_OPENGL = $(CFLAGS_BASE)
CFLAGS_OPENGL += -I/usr/include -I/usr/local/include -I/usr/X11R6/include
CFLAGS_OPENGL += -fPIC

# Game
CFLAGS_GAME = $(CFLAGS_BASE)
CFLAGS_GAME += -fPIC

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

# OGG/Vorbis
OGGLDFLAGS = -lvorbis -lvorbisfile -logg

# SDL
SDLLDFLAGS := $(shell sdl-config --libs)

# ZLib
ZLIBLDFLAGS = -lz

# OpenGL
OPENGLLDFLAGS = -shared

# Game
GAMELDFLAGS = -shared

# ----------

# Targets

all: client dedicated_server ref_gl baseq2 ctf

client:
	@-mkdir -p build \
		build/client \
		build/client/console \
		build/client/input \
		build/client/menu \
		build/client/sound \
		build/common \
		build/common/command \
		build/common/common \
		build/common/message \
		build/common/model \
		build/common/unzip \
		build/gameabi \
		build/unix \
		build/unix/vid \
		build/sdl \
		build/server \
		build/unzip \
		release
	$(MAKE) release/quake2	

dedicated_server:
	@-mkdir -p build \
		build/dedicated_server \
		build/dedicated_server_common \
		build/dedicated_server_common/command \
		build/dedicated_server_common/common \
		build/dedicated_server_common/message \
		build/dedicated_server_common/model \
		build/dedicated_server_common/unzip \
		build/dedicated_server_unix \
		release
	$(MAKE) release/q2ded

ref_gl:
	@-mkdir -p build \
		build/ref_gl \
		build/ref_gl_game \
		build/ref_gl_unix \
		build/ref_gl/files \
		release
	$(MAKE) release/ref_gl.so

baseq2:
	@-mkdir -p build \
		build/baseq2 \
		release \
		release/baseq2
	$(MAKE) release/baseq2/game.so

ctf:
	@-mkdir -p build \
		build/ctf \
		release \
		release/ctf
	$(MAKE) release/ctf/game.so

clean:
	@-rm -Rf build release

# ----------

# Client object
CLIENT_OBJS = \
	build/client/cl_cin.o \
	build/client/cl_download.o \
	build/client/cl_effects.o \
	build/client/cl_entities.o \
	build/client/cl_input.o \
	build/client/cl_inventory.o \
	build/client/cl_lights.o \
	build/client/cl_main.o \
	build/client/cl_network.o \
	build/client/cl_parse.o \
	build/client/cl_particles.o \
	build/client/cl_prediction.o \
	build/client/cl_tempentities.o \
	build/client/cl_screen.o \
	build/client/cl_view.o \
	build/client/console/console.o \
	build/client/input/keyboard.o \
	build/client/menu/menu.o \
	build/client/menu/qmenu.o \
	build/client/menu/videomenu.o \
	build/client/sound/snd_dma.o \
	build/client/sound/snd_mem.o \
	build/client/sound/snd_mix.o \
	build/client/sound/snd_vorbis.o \
	build/client/sound/snd_wav.o

# ---------

# Common objects
COMMON_OBJS = \
	build/common/crc.o \
	build/common/cvar.o \
	build/common/filesystem.o \
	build/common/md4.o \
	build/common/misc.o \
	build/common/netchan.o \
	build/common/pmove.o \
	build/common/szone.o \
	build/common/zone.o \
	build/common/command/cmd_execution.o \
	build/common/command/cmd_parser.o \
	build/common/command/cmd_script.o \
	build/common/common/com_arg.o \
	build/common/common/com_clientserver.o \
	build/common/message/msg_io.o \
	build/common/message/msg_read.o \
 	build/common/model/cm_areaportals.o \
	build/common/model/cm_box.o \
	build/common/model/cm_boxtracing.o \
	build/common/model/cm_bsp.o \
	build/common/model/cm_vis.o \
	build/common/unzip/ioapi.o \
	build/common/unzip/unzip.o 

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
    build/unix/glob.o \
 	build/unix/network.o \
	build/unix/hunk.o \
	build/unix/misc.o \
	build/unix/system.o \
	build/unix/vid.o
 
# ----------

# SDL Objects
SDL_OBJS= \
	build/sdl/cd.o \
	build/sdl/sound.o

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
	build/dedicated_server_common/crc.o \
	build/dedicated_server_common/cvar.o \
	build/dedicated_server_common/filesystem.o \
	build/dedicated_server_common/md4.o \
	build/dedicated_server_common/misc.o \
	build/dedicated_server_common/netchan.o \
	build/dedicated_server_common/pmove.o \
	build/dedicated_server_common/szone.o \
	build/dedicated_server_common/zone.o \
	build/dedicated_server_common/command/cmd_execution.o \
	build/dedicated_server_common/command/cmd_parser.o \
	build/dedicated_server_common/command/cmd_script.o \
	build/dedicated_server_common/common/com_arg.o \
	build/dedicated_server_common/common/com_clientserver.o \
	build/dedicated_server_common/message/msg_io.o \
	build/dedicated_server_common/message/msg_read.o \
	build/dedicated_server_common/model/cm_areaportals.o \
	build/dedicated_server_common/model/cm_box.o \
	build/dedicated_server_common/model/cm_boxtracing.o \
	build/dedicated_server_common/model/cm_bsp.o \
	build/dedicated_server_common/model/cm_vis.o \
	build/dedicated_server_common/unzip/ioapi.o \
	build/dedicated_server_common/unzip/unzip.o 

# ----------

# Dedicated server POSIX platform objects
DEDICATED_SERVER_POSIX_OBJS = \
	build/dedicated_server_unix/glob.o \
	build/dedicated_server_unix/misc.o \
	build/dedicated_server_unix/network.o \
	build/dedicated_server_unix/hunk.o \
	build/dedicated_server_unix/system.o
 
# ----------

# OpenGL refresher objects
OPENGL_OBJS = \
	build/ref_gl/r_draw.o \
	build/ref_gl/r_image.o \
	build/ref_gl/r_light.o \
	build/ref_gl/r_lightmap.o \
	build/ref_gl/r_mesh.o \
	build/ref_gl/r_model.o \
	build/ref_gl/r_main.o \
	build/ref_gl/r_misc.o \
	build/ref_gl/r_surf.o \
	build/ref_gl/r_scrap.o \
	build/ref_gl/gl_warp.o \
	build/ref_gl/files/md2.o \
	build/ref_gl/files/pcx.o \
	build/ref_gl/files/sp2.o \
	build/ref_gl/files/tga.o \
	build/ref_gl/files/wal.o \

# ----------

# OpenGL Game ABI
OPENGL_GAME_OBJS = \
	build/ref_gl_game/q_shared.o
 
# ----------

# OpenGL refresher POSIX platform object
OPENGL_POSIX_OBJS = \
	build/ref_gl_unix/input.o \
	build/ref_gl_unix/glob.o \
	build/ref_gl_unix/hunk.o \
	build/ref_gl_unix/misc.o \
	build/ref_gl_unix/refresh.o \
	build/ref_gl_unix/qgl.o

# ----------

# Quake II object
BASEQ2_OBJS = \
    build/baseq2/g_ai.o \
	build/baseq2/g_chase.o \
	build/baseq2/g_cmds.o \
	build/baseq2/g_combat.o \
	build/baseq2/g_func.o \
	build/baseq2/g_items.o \
	build/baseq2/g_main.o \
	build/baseq2/g_misc.o \
	build/baseq2/g_monster.o \
	build/baseq2/g_phys.o \
	build/baseq2/g_save.o \
	build/baseq2/g_spawn.o \
	build/baseq2/g_svcmds.o \
	build/baseq2/g_target.o \
	build/baseq2/g_trigger.o \
	build/baseq2/g_turret.o \
	build/baseq2/g_utils.o \
	build/baseq2/g_weapon.o \
	build/baseq2/m_actor.o \
	build/baseq2/m_berserk.o \
	build/baseq2/m_boss2.o \
	build/baseq2/m_boss3.o \
	build/baseq2/m_boss31.o \
	build/baseq2/m_boss32.o \
	build/baseq2/m_brain.o \
	build/baseq2/m_chick.o \
	build/baseq2/m_flash.o \
	build/baseq2/m_flipper.o \
	build/baseq2/m_float.o \
	build/baseq2/m_flyer.o \
	build/baseq2/m_gladiator.o \
	build/baseq2/m_gunner.o \
	build/baseq2/m_hover.o \
	build/baseq2/m_infantry.o \
	build/baseq2/m_insane.o \
	build/baseq2/m_medic.o \
	build/baseq2/m_move.o \
	build/baseq2/m_mutant.o \
	build/baseq2/m_parasite.o \
	build/baseq2/m_soldier.o \
	build/baseq2/m_supertank.o \
	build/baseq2/m_tank.o \
	build/baseq2/p_client.o \
	build/baseq2/p_hud.o \
	build/baseq2/p_trail.o \
	build/baseq2/p_view.o \
	build/baseq2/p_weapon.o \
	build/baseq2/q_shared.o 

# ----------

# CTF object
CTF_OBJS = \
	build/ctf/g_ai.o \
	build/ctf/g_chase.o \
	build/ctf/g_cmds.o \
	build/ctf/g_combat.o \
	build/ctf/g_ctf.o \
	build/ctf/g_func.o \
	build/ctf/g_items.o \
	build/ctf/g_main.o \
	build/ctf/g_misc.o \
	build/ctf/g_monster.o \
	build/ctf/g_phys.o \
	build/ctf/g_save.o \
	build/ctf/g_spawn.o \
	build/ctf/g_svcmds.o \
	build/ctf/g_target.o \
	build/ctf/g_trigger.o \
	build/ctf/g_utils.o \
	build/ctf/g_weapon.o \
	build/ctf/m_move.o \
	build/ctf/p_client.o \
	build/ctf/p_hud.o \
	build/ctf/p_menu.o \
	build/ctf/p_trail.o \
	build/ctf/p_view.o \
	build/ctf/p_weapon.o \
	build/ctf/q_shared.o

# ----------

# Client build
build/client/cl_cin.o :     		src/client/cl_cin.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_download.o :   		src/client/cl_download.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<

build/client/cl_effects.o :      		src/client/cl_effects.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
 
build/client/cl_entities.o :   		src/client/cl_entities.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_input.o :   		src/client/cl_input.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_inventory.o : 		src/client/cl_inventory.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_lights.o :     		src/client/cl_lights.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
 
build/client/cl_main.o :    		src/client/cl_main.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_network.o :    		src/client/cl_network.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
   
build/client/cl_parse.o :   		src/client/cl_parse.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
 
build/client/cl_particles.o :  		src/client/cl_particles.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
 
build/client/cl_prediction.o :		src/client/cl_prediction.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_tempentities.o :	src/client/cl_tempentities.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_screen.o :    		src/client/cl_screen.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/cl_view.o :    		src/client/cl_view.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/console/console.o :	src/client/console/console.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<

build/client/input/keyboard.o :		src/client/input/keyboard.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/menu/menu.o :			src/client/menu/menu.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/menu/qmenu.o :			src/client/menu/qmenu.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/menu/videomenu.o :		src/client/menu/videomenu.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
  
build/client/sound/snd_dma.o :		src/client/sound/snd_dma.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/sound/snd_mem.o :		src/client/sound/snd_mem.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/sound/snd_mix.o :		src/client/sound/snd_mix.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/client/sound/snd_vorbis.o :  	src/client/sound/snd_vorbis.c
	$(CC) $(CFLAGS_CLIENT) $(OGGCFLAGS) -o $@ -c $< 

build/client/sound/snd_wav.o :		src/client/sound/snd_wav.c
	$(CC) $(CFLAGS_CLIENT) $(OGGCFLAGS) -o $@ -c $< 
 
# ---------

# Common build 
build/common/crc.o :        		src/common/crc.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<

build/common/cvar.o :       		src/common/cvar.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/common/filesystem.o :    		src/common/filesystem.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/common/md4.o :        		src/common/md4.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
 
build/common/misc.o :        		src/common/misc.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/common/netchan.o :   			src/common/netchan.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/common/pmove.o :      		src/common/pmove.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
 
build/common/szone.o :      		src/common/szone.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
  
build/common/zone.o :	      		src/common/zone.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/common/command/cmd_execution.o :    	src/common/command/cmd_execution.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<

build/common/command/cmd_parser.o :    		src/common/command/cmd_parser.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<
  
build/common/command/cmd_script.o :    		src/common/command/cmd_script.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<

build/common/common/com_arg.o :				src/common/common/com_arg.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<
 
build/common/common/com_clientserver.o : 	src/common/common/com_clientserver.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<
  
build/common/message/msg_io.o :     src/common/message/msg_io.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
  
build/common/message/msg_read.o :   src/common/message/msg_read.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
       
build/common/model/cm_areaportals.o : 	src/common/model/cm_areaportals.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<
	
build/common/model/cm_box.o :     		src/common/model/cm_box.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<
  
build/common/model/cm_boxtracing.o : 	src/common/model/cm_boxtracing.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<

build/common/model/cm_bsp.o :        	src/common/model/cm_bsp.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<

build/common/model/cm_vis.o :        	src/common/model/cm_vis.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<    
 
build/common/unzip/ioapi.o :   	   		src/common/unzip/ioapi.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/common/unzip/unzip.o :	      	src/common/unzip/unzip.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $<  

# ----------

# Game ABI build
build/gameabi/m_flash.o :  			src/game/baseq2/m_flash.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
 
build/gameabi/q_shared.o : 			src/game/baseq2/q_shared.c
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

# Unix build
build/unix/network.o : 			src/unix/network.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
 
build/unix/hunk.o :	  			src/unix/hunk.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/unix/misc.o :	  			src/unix/misc.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
 
build/unix/system.o :  			src/unix/system.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
 
build/unix/glob.o :  			src/unix/glob.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/unix/vid.o :		     	src/unix/vid.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

# ----------

build/sdl/cd.o :     				src/sdl/cd.c
	$(CC) $(CFLAGS_CLIENT) $(SDLCFLAGS) -o $@ -c $<

build/sdl/sound.o : 	 			src/sdl/sound.c
	$(CC) $(CFLAGS_CLIENT) $(SDLCFLAGS) -o $@ -c $<  

# ----------
 
# Dedicated server build
build/dedicated_server/sv_ccmds.o :	src/server/sv_ccmds.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_ents.o : 	src/server/sv_ents.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_game.o : 	src/server/sv_game.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_init.o : 	src/server/sv_init.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_main.o : 	src/server/sv_main.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_send.o : 	src/server/sv_send.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_user.o : 	src/server/sv_user.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server/sv_world.o :	src/server/sv_world.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

# ---------
 
# Dedicated server common build     
build/dedicated_server_common/crc.o :       		src/common/crc.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/cvar.o :      		src/common/cvar.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/filesystem.o :   		src/common/filesystem.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/md4.o :       		src/common/md4.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 
 
build/dedicated_server_common/misc.o :       		src/common/misc.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/netchan.o :  			src/common/netchan.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/pmove.o :     		src/common/pmove.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 
 
build/dedicated_server_common/szone.o :	    		src/common/szone.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 
  
build/dedicated_server_common/zone.o :	    		src/common/zone.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/command/cmd_execution.o :		src/common/command/cmd_execution.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 
 
build/dedicated_server_common/command/cmd_parser.o :		src/common/command/cmd_parser.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/command/cmd_script.o :		src/common/command/cmd_script.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/common/com_arg.o :	src/common/common/com_arg.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 
 
build/dedicated_server_common/common/com_clientserver.o :	src/common/common/com_clientserver.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 
  
build/dedicated_server_common/message/msg_io.o :    src/common/message/msg_io.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 
  
build/dedicated_server_common/message/msg_read.o :  src/common/message/msg_read.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 
 
build/dedicated_server_common/model/cm_areaportals.o :  src/common/model/cm_areaportals.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/model/cm_box.o :    		src/common/model/cm_box.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/model/cm_boxtracing.o :	src/common/model/cm_boxtracing.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/model/cm_bsp.o :    		src/common/model/cm_bsp.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_common/model/cm_vis.o :    		src/common/model/cm_vis.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 
  
build/dedicated_server_common/unzip/ioapi.o :   	   	src/common/unzip/ioapi.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 

build/dedicated_server_common/unzip/unzip.o :	      	src/common/unzip/unzip.c
	$(CC) $(CFLAGS_CLIENT) -o $@ -c $< 
# ----------

# Dedicated server Unix build
build/dedicated_server_unix/glob.o :		src/unix/glob.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_unix/misc.o :		src/unix/misc.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 
    
build/dedicated_server_unix/network.o :   	src/unix/network.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 
 
build/dedicated_server_unix/hunk.o :	  	src/unix/hunk.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

build/dedicated_server_unix/system.o :  	src/unix/system.c
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ -c $< 

# ----------

# OpenGL refresher build
build/ref_gl/r_draw.o:						src/refresh/r_draw.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<

build/ref_gl/r_image.o:						src/refresh/r_image.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<
  
build/ref_gl/r_light.o:						src/refresh/r_light.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<
   
build/ref_gl/r_lightmap.o:					src/refresh/r_lightmap.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<
    
build/ref_gl/r_mesh.o:						src/refresh/r_mesh.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<
  
build/ref_gl/r_model.o:   					src/refresh/r_model.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<
  
build/ref_gl/r_main.o:   					src/refresh/r_main.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<
  
build/ref_gl/r_misc.o:   					src/refresh/r_misc.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<

build/ref_gl/r_surf.o:   					src/refresh/r_surf.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<
 
build/ref_gl/r_scrap.o:   					src/refresh/r_scrap.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<
 
build/ref_gl/gl_warp.o:						src/refresh/gl_warp.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<

build/ref_gl/files/md2.o:					src/refresh/files/md2.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<  
    
build/ref_gl/files/pcx.o:					src/refresh/files/pcx.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<  
 
build/ref_gl/files/sp2.o:					src/refresh/files/sp2.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<  
  
build/ref_gl/files/tga.o:					src/refresh/files/tga.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<  

build/ref_gl/files/wal.o:					src/refresh/files/wal.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<  
   
# ----------

build/ref_gl_game/q_shared.o:				src/game/baseq2/q_shared.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<
  
# ----------

# OpenGL refresher Unix build
build/ref_gl_unix/glob.o:					src/unix/glob.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<

build/ref_gl_unix/hunk.o:					src/unix/hunk.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<
 
build/ref_gl_unix/misc.o:					src/unix/misc.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<
      
build/ref_gl_unix/qgl.o:					src/unix/qgl.c
	$(CC) $(CFLAGS_OPENGL) -o $@ -c $<

build/ref_gl_unix/refresh.o:				src/sdl/refresh.c
	$(CC) $(CFLAGS_OPENGL) $(SDLCFLAGS) -o $@ -c $<

build/ref_gl_unix/input.o:					src/sdl/input.c
	$(CC) $(CFLAGS_OPENGL) $(SDLCFLAGS) -o $@ -c $<
 
# ----------

# Quake II build
build/baseq2/g_ai.o:					src/game/baseq2/g_ai.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_chase.o:					src/game/baseq2/g_chase.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_cmds.o:					src/game/baseq2/g_cmds.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_combat.o:   				src/game/baseq2/g_combat.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_func.o:					src/game/baseq2/g_func.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_items.o:					src/game/baseq2/g_items.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_main.o:					src/game/baseq2/g_main.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_misc.o:					src/game/baseq2/g_misc.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_monster.o:  				src/game/baseq2/g_monster.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_phys.o:					src/game/baseq2/g_phys.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_save.o:					src/game/baseq2/g_save.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_spawn.o:					src/game/baseq2/g_spawn.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_svcmds.o:   				src/game/baseq2/g_svcmds.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_target.o:   				src/game/baseq2/g_target.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_trigger.o:  				src/game/baseq2/g_trigger.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_turret.o:   				src/game/baseq2/g_turret.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_utils.o:					src/game/baseq2/g_utils.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/g_weapon.o:  				src/game/baseq2/g_weapon.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_actor.o:					src/game/baseq2/m_actor.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_berserk.o:		   		src/game/baseq2/m_berserk.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_boss2.o:					src/game/baseq2/m_boss2.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_boss3.o:					src/game/baseq2/m_boss3.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_boss31.o:		   		src/game/baseq2/m_boss31.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_boss32.o:		   		src/game/baseq2/m_boss32.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_brain.o:			 	  	src/game/baseq2/m_brain.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $< 

build/baseq2/m_chick.o:					src/game/baseq2/m_chick.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_flash.o:					src/game/baseq2/m_flash.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_flipper.o:		   		src/game/baseq2/m_flipper.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_float.o:					src/game/baseq2/m_float.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_flyer.o:					src/game/baseq2/m_flyer.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_gladiator.o:		   		src/game/baseq2/m_gladiator.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_gunner.o:		   		src/game/baseq2/m_gunner.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $< 

build/baseq2/m_hover.o:			   		src/game/baseq2/m_hover.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_infantry.o:		   		src/game/baseq2/m_infantry.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_insane.o:		   		src/game/baseq2/m_insane.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_medic.o:					src/game/baseq2/m_medic.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_move.o:					src/game/baseq2/m_move.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_mutant.o:		   		src/game/baseq2/m_mutant.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_parasite.o:		   		src/game/baseq2/m_parasite.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $< 

build/baseq2/m_soldier.o:		   		src/game/baseq2/m_soldier.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_supertank.o:		   		src/game/baseq2/m_supertank.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/m_tank.o:					src/game/baseq2/m_tank.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/p_client.o:   				src/game/baseq2/p_client.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/p_hud.o:					src/game/baseq2/p_hud.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/p_trail.o:					src/game/baseq2/p_trail.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/p_view.o:					src/game/baseq2/p_view.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $< 

build/baseq2/p_weapon.o:   				src/game/baseq2/p_weapon.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/baseq2/q_shared.o:   				src/game/baseq2/q_shared.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
# ----------

build/ctf/g_ai.o:   				src/game/ctf/g_ai.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_chase.o:   				src/game/ctf/g_chase.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<

build/ctf/g_cmds.o:   				src/game/ctf/g_cmds.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_combat.o:  				src/game/ctf/g_combat.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_ctf.o:   				src/game/ctf/g_ctf.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_func.o:   				src/game/ctf/g_func.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_items.o:   				src/game/ctf/g_items.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_main.o:   				src/game/ctf/g_main.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_misc.o:   				src/game/ctf/g_misc.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_monster.o: 				src/game/ctf/g_monster.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_phys.o:  				src/game/ctf/g_phys.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_save.o:   				src/game/ctf/g_save.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_spawn.o:   				src/game/ctf/g_spawn.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_svcmds.o:  				src/game/ctf/g_svcmds.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_target.o:  				src/game/ctf/g_target.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_trigger.o: 				src/game/ctf/g_trigger.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_utils.o:   				src/game/ctf/g_utils.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/g_weapon.o:  				src/game/ctf/g_weapon.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/m_move.o:   				src/game/ctf/m_move.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/p_client.o:  				src/game/ctf/p_client.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/p_hud.o:   				src/game/ctf/p_hud.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/p_menu.o:   				src/game/ctf/p_menu.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/p_trail.o:   				src/game/ctf/p_trail.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/p_view.o:   				src/game/ctf/p_view.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/p_weapon.o:  				src/game/ctf/p_weapon.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
build/ctf/q_shared.o:  				src/game/ctf/q_shared.c
	$(CC) $(CFLAGS_GAME) -o $@ -c $<
 
# ----------
  
#  The client
release/quake2 : $(CLIENT_OBJS) $(COMMON_OBJS) $(GAME_ABI_OBJS) \
    $(SERVER_OBJS) $(POSIX_OBJS) $(SDL_OBJS)
	$(CC) $(CFLAGS_CLIENT) -o $@ $(CLIENT_OBJS) $(COMMON_OBJS) $(GAME_ABI_OBJS) \
		$(SERVER_OBJS) $(POSIX_OBJS) $(SDL_OBJS) $(LDFLAGS) \
		$(SDLLDFLAGS) $(OGGLDFLAGS) $(ZLIBLDFLAGS)

# Dedicated Server
release/q2ded : $(DEDICATED_SERVER_OBJS) $(DEDICATED_SERVER_COMMON_OBJS) \
	$(GAME_ABI_OBJS) $(DEDICATED_SERVER_POSIX_OBJS)
	$(CC) $(CFLAGS_DEDICATED_SERVER) -o $@ $(DEDICATED_SERVER_OBJS) \
		$(DEDICATED_SERVER_COMMON_OBJS) $(GAME_ABI_OBJS) \
		$(DEDICATED_SERVER_POSIX_OBJS) $(LDFLAGS) $(ZLIBLDFLAGS)

# OpenGL refresher
release/ref_gl.so : $(OPENGL_OBJS) $(OPENGL_POSIX_OBJS) $(OPENGL_GAME_OBJS)
	$(CC) $(CFLAGS_OPENGL) -o $@ $(OPENGL_OBJS) $(OPENGL_POSIX_OBJS) \
		$(OPENGL_GAME_OBJS) $(LDFLAGS) $(SDLLDFLAGS) $(OPENGLLDFLAGS)

# Quake II
release/baseq2/game.so : $(BASEQ2_OBJS)
	$(CC) $(CFLAGS_GAME) -o $@ $(BASEQ2_OBJS) $(LDFLAGS) $(GAMELDFLAGS)

# Quake II - Three Wave Capture The Flag
release/ctf/game.so : $(CTF_OBJS)
	$(CC) $(CFLAGS_GAME) -o $@ $(CTF_OBJS) $(LDFLAGS) $(GAMELDFLAGS)

