# ------------------------------------------------------ #
# Makefile for the "Yamagi Quake 2 Client"               #
#                                                        #
# Just type "make" to compile the                        #
#  - SDL Client (quake2)                                 #
#  - Server (q2ded)                                      #
#  - SDL OpenGL-Refresher (ref_gl.so)                    #
#  - games:                                              #
#    - Quake II (baseq2)                                 #
#    - Quake II - Threewave Capture The Flag (ctf)       #
#                                                        #
# Dependencies:                                          #
#  - SDL 1.2                                             #
#  - libX11                                              #
#  - libGL                                               #
#  - libvorbis                                           #
#  - libogg                                              #
#  - X11 (libX11, Xxf86vm)								 #
#  - zlib                                                #
#                                                        #
# Platforms:                                             #
#  - Linux                                               #
#  - FreeBSD                                             #
#  - Maybe any other UNIX compliant system              #
#    supported by SDL 1.2                                #
# ------------------------------------------------------ # 

# Check the OS type
OSTYPE := $(shell uname -s)

# Some plattforms call it "amd64" and some "x86_64"
ARCH := $(shell uname -m | sed -e s/i.86/i386/ -e s/amd64/x86_64/)

# Refuse all other plattforms as a firewall against PEBKAC
ifneq ($(ARCH),i386)
ifneq ($(ARCH),x86_64)
$(error arch $(ARCH) is currently not supported)
endif
endif

# ----------

# The compiler
CC := gcc

# ----------

# Base CFLAGS. These are extended later
# for each independet target
CFLAGS := -O2  -fno-strict-aliasing -fomit-frame-pointer\
		  -fstack-protector-all -Wall -pipe -g -MMD

# ----------

# SDL Flags
CFSDL := $(shell sdl-config --cflags)
LDSDL := $(shell sdl-config --libs) 

# ----------

# Base include path.
ifeq ($(OSTYPE),Linux)
INCLUDE := -I/usr/include
else ifeq ($(OSTYPE),FreeBSD)
INCLUDE := -I/usr/local/include
endif 

# ----------

# Base LDFLAGS. These are extended later
# for each independet target 
ifeq ($(OSTYPE),Linux)
LDFLAGS := -L/usr/lib -lm -ldl
else ifeq ($(OSTYPE),FreeBSD)
LDFLAGS := -L/usr/local/lib -lm
endif 

# ----------

# Builds everything
all: client server refresher baseq2 ctf

# Cleanup
clean:
	@echo "===> CLEAN"
	@rm -Rf build release
 
# ----------

# The client 
client:
	@echo '===> Building quake2'
	@mkdir -p release
	$(MAKE) release/quake2     

build/client/%.o: %.c
	@echo '===> CC $<'
	@mkdir -p $(@D)
	@$(CC) -c $(CFLAGS) $(CFSDL) $(INCLUDE) -o $@ $<

release/quake2 : LDFLAGS += -lvorbis -lvorbisfile -logg -lz \
							-lXxf86vm -lX11
# ----------

# The server
server:
	@echo '===> Building q2ded'
	@mkdir -p release
	$(MAKE) release/q2ded

build/server/%.o: %.c
	@echo '===> CC $<'
	@mkdir -p $(@D)
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

release/q2ded : CFLAGS += -DDEDICATED_ONLY
release/q2ded : LDFLAGS += -lz

# ----------

# The refresher
refresher:
	@echo '===> Building ref_gl.so'
	@mkdir -p release
	$(MAKE) release/ref_gl.so

build/refresher/%.o: %.c
	@echo '===> CC $<'
	@mkdir -p $(@D)
	@$(CC) -c $(CFLAGS) $(CFSDL) $(INCLUDE) -o $@ $<

release/ref_gl.so : INCLUDE += $(shell pkg-config x11 --cflags)
release/ref_gl.so : CFLAGS += -fPIC
release/ref_gl.so : LDFLAGS += -shared
	
# ----------

# The baseq2 game
baseq2:
	@echo '===> Building baseq2'
	@mkdir -p release/baseq2
	$(MAKE) release/baseq2/game.so

build/baseq2/%.o: %.c
	@echo '===> CC $<'
	@mkdir -p $(@D)
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

release/baseq2/game.so : CFLAGS += -fPIC
release/baseq2/game.so : LDFLAGS += -shared

# ----------

# The ctf game
ctf:
	@echo '===> Building ctf'
	@mkdir -p release/ctf
	$(MAKE) release/ctf/game.so

build/ctf/%.o: %.c
	@echo '===> CC $<'
	@mkdir -p $(@D)
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

release/ctf/game.so : CFLAGS += -fPIC
release/ctf/game.so : LDFLAGS += -shared
                                        
# ----------

# Used by the baseq2 game
BASEQ2_OBJS_ = \
    src/game/baseq2/g_ai.o \
	src/game/baseq2/g_chase.o \
	src/game/baseq2/g_cmds.o \
	src/game/baseq2/g_combat.o \
	src/game/baseq2/g_func.o \
	src/game/baseq2/g_items.o \
	src/game/baseq2/g_main.o \
	src/game/baseq2/g_misc.o \
	src/game/baseq2/g_monster.o \
	src/game/baseq2/g_phys.o \
	src/game/baseq2/g_spawn.o \
	src/game/baseq2/g_svcmds.o \
	src/game/baseq2/g_target.o \
	src/game/baseq2/g_trigger.o \
	src/game/baseq2/g_turret.o \
	src/game/baseq2/g_utils.o \
	src/game/baseq2/g_weapon.o \
	src/game/baseq2/m_actor.o \
	src/game/baseq2/m_berserk.o \
	src/game/baseq2/m_boss2.o \
	src/game/baseq2/m_boss3.o \
	src/game/baseq2/m_boss31.o \
	src/game/baseq2/m_boss32.o \
	src/game/baseq2/m_brain.o \
	src/game/baseq2/m_chick.o \
	src/game/baseq2/m_flash.o \
	src/game/baseq2/m_flipper.o \
	src/game/baseq2/m_float.o \
	src/game/baseq2/m_flyer.o \
	src/game/baseq2/m_gladiator.o \
	src/game/baseq2/m_gunner.o \
	src/game/baseq2/m_hover.o \
	src/game/baseq2/m_infantry.o \
	src/game/baseq2/m_insane.o \
	src/game/baseq2/m_medic.o \
	src/game/baseq2/m_move.o \
	src/game/baseq2/m_mutant.o \
	src/game/baseq2/m_parasite.o \
	src/game/baseq2/m_soldier.o \
	src/game/baseq2/m_supertank.o \
	src/game/baseq2/m_tank.o \
	src/game/baseq2/p_client.o \
	src/game/baseq2/p_hud.o \
	src/game/baseq2/p_trail.o \
	src/game/baseq2/p_view.o \
	src/game/baseq2/p_weapon.o \
	src/game/baseq2/q_shared.o \
    src/game/baseq2/savegame/savegame.o	

# Used by the client
CLIENT_OBJS_ := \
	src/client/cl_cin.o \
	src/client/cl_console.o \
	src/client/cl_download.o \
	src/client/cl_effects.o \
	src/client/cl_entities.o \
	src/client/cl_input.o \
	src/client/cl_inventory.o \
	src/client/cl_keyboard.o \
	src/client/cl_lights.o \
	src/client/cl_main.o \
	src/client/cl_network.o \
	src/client/cl_parse.o \
	src/client/cl_particles.o \
	src/client/cl_prediction.o \
	src/client/cl_screen.o \
	src/client/cl_tempentities.o \
	src/client/cl_view.o \
	src/client/menu/menu.o \
	src/client/menu/qmenu.o \
	src/client/menu/videomenu.o \
	src/client/sound/snd_dma.o \
	src/client/sound/snd_mem.o \
	src/client/sound/snd_mix.o \
	src/client/sound/snd_vorbis.o \
	src/client/sound/snd_wav.o

# Used by the client and the server
COMMON_OBJS_ := \
	src/common/crc.o \
	src/common/cvar.o \
	src/common/filesystem.o \
	src/common/md4.o \
	src/common/misc.o \
	src/common/netchan.o \
	src/common/pmove.o \
	src/common/szone.o \
	src/common/zone.o \
	src/common/command/cmd_execution.o \
	src/common/command/cmd_parser.o \
	src/common/command/cmd_script.o \
	src/common/common/com_arg.o \
	src/common/common/com_clientserver.o \
	src/common/message/msg_io.o \
	src/common/message/msg_read.o \
 	src/common/model/cm_areaportals.o \
	src/common/model/cm_box.o \
	src/common/model/cm_boxtracing.o \
	src/common/model/cm_bsp.o \
	src/common/model/cm_vis.o \
	src/common/unzip/ioapi.o \
	src/common/unzip/unzip.o

# Used by the ctf game
CTF_OBJS_ = \
	src/game/ctf/g_ai.o \
	src/game/ctf/g_chase.o \
	src/game/ctf/g_cmds.o \
	src/game/ctf/g_combat.o \
	src/game/ctf/g_ctf.o \
	src/game/ctf/g_func.o \
	src/game/ctf/g_items.o \
	src/game/ctf/g_main.o \
	src/game/ctf/g_misc.o \
	src/game/ctf/g_monster.o \
	src/game/ctf/g_phys.o \
	src/game/ctf/g_save.o \
	src/game/ctf/g_spawn.o \
	src/game/ctf/g_svcmds.o \
	src/game/ctf/g_target.o \
	src/game/ctf/g_trigger.o \
	src/game/ctf/g_utils.o \
	src/game/ctf/g_weapon.o \
	src/game/ctf/m_move.o \
	src/game/ctf/p_client.o \
	src/game/ctf/p_hud.o \
	src/game/ctf/p_menu.o \
	src/game/ctf/p_trail.o \
	src/game/ctf/p_view.o \
	src/game/ctf/p_weapon.o \
	src/game/ctf/q_shared.o

# Used by the client and the server
GAME_ABI_OBJS_ := \
	src/game/baseq2/m_flash.o \
	src/game/baseq2/q_shared.o

# Used by the OpenGL refresher
OPENGL_OBJS_ = \
	src/refresh/r_draw.o \
	src/refresh/r_image.o \
	src/refresh/r_light.o \
	src/refresh/r_lightmap.o \
	src/refresh/r_main.o \
	src/refresh/r_mesh.o \
	src/refresh/r_misc.o \
	src/refresh/r_model.o \
	src/refresh/r_scrap.o \
	src/refresh/r_surf.o \
	src/refresh/r_warp.o \
	src/refresh/files/md2.o \
	src/refresh/files/pcx.o \
	src/refresh/files/sp2.o \
	src/refresh/files/tga.o \
	src/refresh/files/wal.o 

# Used by the OpenGL refresher
OPENGL_GAME_ABI_OBJS_ = \
	src/game/baseq2/q_shared.o

# Used by the client
UNIX_CLIENT_OBJS_ := \
    src/unix/glob.o \
	src/unix/hunk.o \
	src/unix/misc.o \
 	src/unix/network.o \
	src/unix/system.o \
 	src/unix/vid.o

# Used by the OpenGL refresher
UNIX_OPENGL_OBJS_ = \
	src/unix/glob.o \
	src/unix/hunk.o \
	src/unix/misc.o \
	src/unix/qgl.o

# Used by the server
UNIX_SERVER_OBJS_ := \
    src/unix/glob.o \
	src/unix/hunk.o \
	src/unix/misc.o \
 	src/unix/network.o \
	src/unix/system.o

# Used by the client
SDL_OBJS_ := \
	src/sdl/cd.o \
	src/sdl/sound.o        

# Used by the OpenGL refresher
SDL_OPENGL_OBJS_ := \
	src/sdl/input.o \
	src/sdl/refresh.o

# Used by the server
SERVER_OBJS_ := \
	src/server/sv_cmd.o \
	src/server/sv_conless.o \
	src/server/sv_entities.o \
	src/server/sv_game.o \
	src/server/sv_init.o \
	src/server/sv_main.o \
	src/server/sv_save.o \
	src/server/sv_send.o \
	src/server/sv_user.o \
	src/server/sv_world.o

# ----------

# Rewrite pathes to our object directory
CLIENT_COMMON_OBJS = $(patsubst %,build/client/%,$(COMMON_OBJS_))
CLIENT_GAME_ABI_OBJS = $(patsubst %,build/client/%,$(GAME_ABI_OBJS_))
CLIENT_OBJS = $(patsubst %,build/client/%,$(CLIENT_OBJS_))
CLIENT_SERVER_OBJS = $(patsubst %,build/client/%,$(SERVER_OBJS_))
UNIX_CLIENT_OBJS = $(patsubst %,build/client/%,$(UNIX_CLIENT_OBJS_))
SDL_OBJS = $(patsubst %,build/client/%,$(SDL_OBJS_))

SERVER_OBJS = $(patsubst %,build/server/%,$(SERVER_OBJS_))
SERVER_COMMON_OBJS = $(patsubst %,build/server/%,$(COMMON_OBJS_))
SERVER_GAME_ABI_OBJS = $(patsubst %,build/server/%,$(GAME_ABI_OBJS_))
UNIX_SERVER_OBJS= $(patsubst %,build/server/%,$(UNIX_SERVER_OBJS_))

OPENGL_OBJS = $(patsubst %,build/refresher/%,$(OPENGL_OBJS_))
OPENGL_GAME_ABI_OBJS = $(patsubst %,build/refresher/%,$(OPENGL_GAME_ABI_OBJS_))
UNIX_OPENGL_OBJS = $(patsubst %,build/refresher/%,$(UNIX_OPENGL_OBJS_))
SDL_OPENGL_OBJS = $(patsubst %,build/refresher/%,$(SDL_OPENGL_OBJS_))

BASEQ2_OBJS = $(patsubst %,build/baseq2/%,$(BASEQ2_OBJS_))

CTF_OBJS = $(patsubst %,build/baseq2/%,$(CTF_OBJS_))

# ----------

# Generate header dependencies
CLIENT_DEPS= $(CLIENT_OBJS:.o=.d) 
CLIENT_COMMON_DEPS = $(CLIENT_COMMON_OBJS:.o=.d) 
CLIENT_GAME_ABI_DEPS = $(CLIENT_GAME_ABI_OBJS:.o=.d) 
CLIENT_SERVER_DEPS = $(CLIENT_SERVER_OBJS:.o=.d) 
UNIX_CLIENT_DEPS = $(UNIX_CLIENT_OBJS:.o=.d) 
SDL_DEPS = $(SDL_OBJS:.o=.d) 
	
SERVER_DEPS= $(SERVER_OBJS:.o=.d) 
SERVER_COMMON_DEPS= $(SERVER_COMMON_OBJS:.o=.d) 
SERVER_GAME_ABI_DEPS = $(SERVER_GAME_ABI_OBJS:.o=.d) 
UNIX_SERVER_DEPS= $(UNIX_SERVER_OBJS:.o=.d) 

OPENGL_DEPS= $(OPENGL_OBJS:.o=.d) 
OPENGL_GAME_ABI_DEPS= $(OPENGL_GAME_ABI_OBJS:.o=.d) 
UNIX_OPENGL_DEPS= $(UNIX_OPENGL_OBJS:.o=.d) 
SDL_OPENGL_DEPS= $(SDL_OPENGL_OBJS:.o=.d) 

BASEQ2_DEPS= $(BASEQ2_OBJS:.o=.d) 

CTF_DEPS= $(CTF_OBJS:.o=.d) 

# ----------

# Suck header dependencies in
-include $(CLIENT_DEPS)  
-include $(CLIENT_COMMON_DEPS)  
-include $(CLIENT_GAME_ABI_DEPS)  
-include $(CLIENT_SERVER_DEPS)  
-include $(UNIX_CLIENT_DEPS)  
-include $(SDL_DEPS)  

-include $(SERVER_DEPS)  
-include $(SERVER_COMMON_DEPS)  
-include $(SERVER_GAME_ABI_DEPS)  
-include $(UNIX_SERVER_DEPS)  

-include $(OPENGL_DEPS)  
-include $(OPENGL_GAME_ABI_DEPS)  
-include $(UNIX_OPENGL_DEPS)  
-include $(SDL_OPENGL_DEPS)  

-include $(BASEQ2_DEPS)  

-include $(CTF_DEPS)  

# ----------

# release/quake2
release/quake2 : $(CLIENT_OBJS) $(CLIENT_COMMON_OBJS) $(CLIENT_GAME_ABI_OBJS) \
	$(UNIX_CLIENT_OBJS) $(SDL_OBJS) $(CLIENT_SERVER_OBJS) 
	@echo '===> LD $@'
	@$(CC) $(LDFLAGS) $(LDSDL) -o $@ $(CLIENT_OBJS) $(CLIENT_COMMON_OBJS) \
		$(CLIENT_GAME_ABI_OBJS) $(UNIX_CLIENT_OBJS) \
		$(SDL_OBJS) $(CLIENT_SERVER_OBJS)  

# release/q2ded
release/q2ded : $(SERVER_OBJS) $(SERVER_COMMON_OBJS) $(SERVER_GAME_ABI_OBJS) \
	$(UNIX_SERVER_OBJS)
	@echo '===> LD $@'
	@$(CC) $(LDFLAGS) -o $@ $(SERVER_OBJS) $(SERVER_COMMON_OBJS) \
		$(SERVER_GAME_ABI_OBJS) $(UNIX_SERVER_OBJS) 

# release/ref_gl.so
release/ref_gl.so : $(OPENGL_OBJS) $(OPENGL_GAME_ABI_OBJS) \
	$(UNIX_OPENGL_OBJS) $(SDL_OPENGL_OBJS) 
	@echo '===> LD $@'
	@$(CC) $(LDFLAGS) -o $@  $(OPENGL_OBJS) $(OPENGL_GAME_ABI_OBJS) \
		$(UNIX_OPENGL_OBJS) $(SDL_OPENGL_OBJS)  

# release/bsaeq2/game.so
release/baseq2/game.so : $(BASEQ2_OBJS)
	@echo '===> LD $@'
	@$(CC) $(LDFLAGS) -o $@ $(BASEQ2_OBJS) 

# release/ctf/game.so
release/ctf/game.so : $(CTF_OBJS)
	@echo '===> LD $@'
	@$(CC) $(LDFLAGS) -o $@ $(CTF_OBJS) 
	     
# ----------

