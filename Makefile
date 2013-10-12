# ------------------------------------------------------ #
# Makefile for the "Yamagi Quake 2 Client"               #
#                                                        #
# Just type "make" to compile the                        #
#  - SDL Client (quake2)                                 #
#  - Server (q2ded)                                      #
#  - Quake II Game (baseq2)                              #
#                                                        #
# Base dependencies:                                     #
#  - SDL 1.2 or SDL 2.0                                  #
#  - libGL                                               #
#                                                        #
# Platforms:                                             #
#  - FreeBSD                                             #
#  - Linux                                               #
#  - OpenBSD                                             #
#  - Windows (MinGW)                                     #
# ------------------------------------------------------ #

# User configurable options
# -------------------------

# Enables CD audio playback. CD audio playback is used
# for the background music and doesn't add any further
# dependencies. It should work on all platforms where
# CD playback is supported by SDL.
# was yes
WITH_CDA:=yes

# Enables OGG/Vorbis support. OGG/Vorbis files can be
# used as a substitute of CD audio playback. Adds
# dependencies to libogg, libvorbis and libvorbisfile.
WITH_OGG:=yes

# Enables the optional OpenAL sound system.
# To use it your system needs libopenal.so.1
# or openal32.dll (we recommend openal-soft)
# installed
WITH_OPENAL:=yes

# Enables retexturing support. Adds
# a dependency to libjpeg
WITH_RETEXTURING:=yes

# Use SDL2 instead of SDL1.2. Disables CD audio support,
# because SDL2 has none. Use OGG/Vorbis music instead :-)
# On Windows sdl-config isn't used, so make sure that
# you've got the SDL2 headers and libs installed.
WITH_SDL2:=yes

# Set the gamma via X11 and not via SDL. This works
# around problems in some SDL version. Adds dependencies
# to pkg-config, libX11 and libXxf86vm. Unsupported on
# Windows and OS X.
WITH_X11GAMMA:=no

# Enables opening of ZIP files (also known as .pk3 paks).
# Adds a dependency to libz
WITH_ZIP:=yes

# Enable systemwide installation of game assets
WITH_SYSTEMWIDE:=no

# This will set the default SYSTEMDIR, a non-empty string
# would actually be used. On Windows normals slashes (/)
# instead of backslashed (\) should be used!
WITH_SYSTEMDIR:=

# This will set the architectures of the OSX-binaries.
# You have to make sure your libs/frameworks supports
# these architectures to build an universal ppc-compatible
# one would add -arch ppc for example, but I did not tested
# it!
OSX_ARCH := -arch i386 -arch x86_64

# This will set the build options to create an MacOS .app-bundle.
# The app-bundle itself will not be created, but the runtime paths
# will be set to expect the linked Frameworks in *.app/Contents/
# Frameworks and the game-data will be expected in # *.app/
# Contents/Resources
OSX_APP := yes

# ====================================================== #
#     !!! DO NOT ALTER ANYTHING BELOW THIS LINE !!!      #
# ====================================================== #

# Detect the OS
ifdef SystemRoot
OSTYPE := Windows
else
OSTYPE := $(shell uname -s)
endif

# Detect the architecture
ifeq ($(OSTYPE), Windows)
# At this time only i386 is supported on Windows
# (amd64 works, but building an 64 bit executable
# is not that easy. Especially SDL and OpenAL are
# somewhat problematic)
ARCH := i386
else
# Some platforms call it "amd64" and some "x86_64"
ARCH := $(shell uname -m | sed -e s/i.86/i386/ -e s/amd64/x86_64/)
endif

# Refuse all other platforms as a firewall against PEBKAC
# (You'll need some #ifdef for your unsupported  plattform!)
ifeq ($(findstring $(ARCH), i386 x86_64 sparc64 ia64),)
$(error arch $(ARCH) is currently not supported)
endif

# Disable CDA for SDL2
ifeq ($(WITH_SDL2),yes)
ifeq ($(WITH_CDA),yes)
WITH_CDA:=no

# Evil hack to tell the "all" target
# that CDA was disabled because SDL2
# is enabled.
CDA_ENABLED:=yes
endif
endif

# ----------

# Base CFLAGS.
#
# -O2 are enough optimizations.
#
# -fno-strict-aliasing since the source doesn't comply
#  with strict aliasing rules and it's next to impossible
#  to get it there...
#
# -fomit-frame-pointer since the framepointer is mostly
#  useless for debugging Quake II and slows things down.
#
# -g to build always with debug symbols. Please DO NOT
#  CHANGE THIS, since it's our only chance to debug this
#  crap when random crashes happen!
#
# -MMD to generate header dependencies. (They cannot be
#  generated if building universal binaries on OSX)
ifeq ($(OSTYPE), Darwin)
CFLAGS := -O2 -fno-strict-aliasing -fomit-frame-pointer \
		  -Wall -pipe -g
CFLAGS += $(OSX_ARCH)
else
CFLAGS := -O2 -fno-strict-aliasing -fomit-frame-pointer \
		  -Wall -pipe -g -MMD
endif

# ----------

# Systemwide installation
ifeq ($(WITH_SYSTEMWIDE),yes)
CFLAGS += -DSYSTEMWIDE
ifneq ($(WITH_SYSTEMDIR),"")
CFLAGS += -DSYSTEMDIR=\"$(WITH_SYSTEMDIR)\"
endif
endif

# ----------

# Extra CFLAGS for SDL
ifneq ($(OSTYPE), Windows)
ifeq ($(OSTYPE), Darwin)
SDLCFLAGS :=
else # not darwin
ifeq ($(WITH_SDL2),yes)
SDLCFLAGS := $(shell sdl2-config --cflags)
else # not SDL2
SDLCFLAGS := $(shell sdl-config --cflags)
endif # SDL2

endif # darwin's else
endif # not windows

# ----------

# Extra CFLAGS for X11
ifneq ($(OSTYPE), Windows)
ifneq ($(OSTYPE), Darwin)
ifeq ($(WITH_X11GAMMA),yes)
X11CFLAGS := $(shell pkg-config x11 --cflags)
X11CFLAGS += $(shell pkg-config xxf86vm --cflags)
endif
endif
endif

# ----------

# Base include path.
ifeq ($(OSTYPE),Linux)
INCLUDE := -I/usr/include
else ifeq ($(OSTYPE),FreeBSD)
INCLUDE := -I/usr/local/include
else ifeq ($(OSTYPE),OpenBSD)
INCLUDE := -I/usr/local/include
else ifeq ($(OSTYPE),Darwin)
INCLUDE :=
endif

# ----------

# Base LDFLAGS.
ifeq ($(OSTYPE),Linux)
LDFLAGS := -L/usr/lib -lm -ldl -rdynamic
else ifeq ($(OSTYPE),FreeBSD)
LDFLAGS := -L/usr/local/lib -lm
else ifeq ($(OSTYPE),OpenBSD)
LDFLAGS := -L/usr/local/lib -lm
else ifeq ($(OSTYPE),Windows)
LDFLAGS := -lws2_32 -lwinmm
else ifeq ($(OSTYPE), Darwin)
LDFLAGS := $(OSX_ARCH) -lm
endif

# ----------

# Extra LDFLAGS for SDL
ifeq ($(OSTYPE), Windows)
ifeq ($(WITH_SDL2),yes)
SDLLDFLAGS := -lSDL2main -lSDL2  -mwindows  -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lversion -luuid
else # not SDL2
SDLLDFLAGS := -lSDL
endif # SDL2
else ifeq ($(OSTYPE), Darwin)
ifeq ($(WITH_SDL2),yes)
SDLLDFLAGS := -framework SDL2 -framework OpenGL -framework Cocoa
else # not SDL2
SDLLDFLAGS := -framework SDL -framework OpenGL -framework Cocoa
endif # SDL2
else # not Darwin/Win
ifeq ($(WITH_SDL2),yes)
SDLLDFLAGS := $(shell sdl2-config --libs)
else # not SDL2
SDLLDFLAGS := $(shell sdl-config --libs)
endif # SDL2
endif # Darwin/Win

# ----------

# Extra LDFLAGS for X11
ifneq ($(OSTYPE), Windows)
ifneq ($(OSTYPE), Darwin)
ifeq ($(WITH_X11GAMMA),yes)
X11LDFLAGS := $(shell pkg-config x11 --libs)
X11LDFLAGS += $(shell pkg-config xxf86vm --libs)
X11LDFLAGS += $(shell pkg-config xrandr --libs)
endif
endif
endif

# ----------

# When make is invoked by "make VERBOSE=1" print
# the compiler and linker commands.

ifdef VERBOSE
Q :=
else
Q := @
endif

# ----------

# Phony targets
#.PHONY : all client game icon refresher server
.PHONY : all client game icon server

# ----------

# Builds everything
all: config client server game

# Print config values
config:
	@echo "Build configuration"
	@echo "============================"
	@echo "WITH_CDA = $(WITH_CDA)"
	@echo "WITH_OPENAL = $(WITH_OPENAL)"
	@echo "WITH_RETEXTURING = $(WITH_RETEXTURING)"
	@echo "WITH_SDL2 = $(WITH_SDL2)"
	@echo "WITH_X11GAMMA = $(WITH_X11GAMMA)"
	@echo "WITH_ZIP = $(WITH_ZIP)"
	@echo "WITH_SYSTEMWIDE = $(WITH_SYSTEMWIDE)"
	@echo "WITH_SYSTEMDIR = $(WITH_SYSTEMDIR)"
	@echo "============================"
	@echo ""
ifeq ($(WITH_SDL2),yes)
ifeq ($(CDA_ENABLED),yes)
	@echo "WARNING: CDA disabled because SDL2 doesn't support it!"
	@echo ""
endif
endif
	
# ----------

# Special target to compile
# the icon on Windows
ifeq ($(OSTYPE), Windows)
icon:
	@echo "===> WR build/icon/icon.res"
	${Q}stuff/misc/mkdir -p build/icon
	${Q}windres stuff\icon\icon.rc -O COFF -o build\icon\icon.res
endif

# ----------

# Cleanup
ifeq ($(OSTYPE), Windows)
clean:
	@echo "===> CLEAN"
	@-rmdir /S /Q release build
else
clean:
	@echo "===> CLEAN"
	${Q}rm -Rf build release
endif

# ----------

# The client
ifeq ($(OSTYPE), Windows)
client:
	@echo "===> Building quake2.exe"
	${Q}stuff/misc/mkdir.exe -p release
	$(MAKE) release/quake2.exe

build/client/%.o: %.c
	@echo "===> CC $<"
	${Q}stuff/misc/mkdir.exe -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(INCLUDE) -o $@ $<

ifeq ($(WITH_CDA),yes)
release/quake2.exe : CFLAGS += -DCDA
endif

ifeq ($(WITH_OGG),yes)
release/quake2.exe : CFLAGS += -DOGG
release/quake2.exe : LDFLAGS += -lvorbisfile -lvorbis -logg
endif

ifeq ($(WITH_OPENAL),yes)
release/quake2.exe : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"openal32.dll"'
endif

ifeq ($(WITH_ZIP),yes)
release/quake2.exe : CFLAGS += -DZIP -DNOUNCRYPT
release/quake2.exe : LDFLAGS += -lz
endif
 
ifeq ($(WITH_RETEXTURING),yes)
release/quake2.exe : CFLAGS += -DRETEXTURE
release/quake2.exe : LDFLAGS += -ljpeg
endif 

ifeq ($(WITH_SDL2),yes)
release/quake2.exe : CFLAGS += -DSDL2
endif

release/quake2.exe : LDFLAGS += -mwindows -lopengl32
else
client:
	@echo "===> Building quake2"
	${Q}mkdir -p release
	$(MAKE) release/quake2

build/client/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(X11CFLAGS) $(INCLUDE) -o $@ $<

ifeq ($(OSTYPE), Darwin)
build/client/%.o : %.m
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) $(OSX_ARCH) -x objective-c -c $< -o $@
endif

ifeq ($(WITH_CDA),yes)
release/quake2 : CFLAGS += -DCDA
endif

ifeq ($(WITH_OGG),yes)
release/quake2 : CFLAGS += -DOGG
ifeq ($(OSTYPE), Darwin)
release/quake2 : LDFLAGS += -framework Vorbis -framework Ogg
else
release/quake2 : LDFLAGS += -lvorbis -lvorbisfile -logg
endif
endif

ifeq ($(WITH_OPENAL),yes)
ifeq ($(OSTYPE), OpenBSD)
release/quake2 : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"libopenal.so"'
else ifeq ($(OSTYPE), Darwin)
release/quake2 : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"/System/Library/Frameworks/OpenAL.framework/OpenAL"'
release/quake2 : LDFLAGS += -framework OpenAL
else
release/quake2 : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"libopenal.so.1"'
endif
endif

ifeq ($(WITH_ZIP),yes)
release/quake2 : CFLAGS += -DZIP -DNOUNCRYPT
release/quake2 : LDFLAGS += -lz
endif

ifeq ($(WITH_X11GAMMA),yes)
release/quake2 : CFLAGS += -DX11GAMMA
endif

ifeq ($(WITH_RETEXTURING),yes)
release/quake2 : CFLAGS += -DRETEXTURE
ifeq ($(OSTYPE), Darwin)
release/quake2 : LDFLAGS += -framework libjpeg
else
release/quake2 : LDFLAGS += -ljpeg
endif
endif

ifeq ($(WITH_SDL2),yes)
release/quake2 : CFLAGS += -DSDL2
endif
 
ifeq ($(OSTYPE), Darwin)
ifeq ($(OSX_APP), yes)
release/quake2 : LDFLAGS += -Xlinker -rpath -Xlinker @loader_path/../Frameworks
endif
endif
 
ifneq ($(OSTYPE), Darwin)
release/quake2 : LDFLAGS += -lGL
endif
endif

# ----------

# The server
ifeq ($(OSTYPE), Windows)
server:
	@echo "===> Building q2ded"
	${Q}stuff/misc/mkdir.exe -p release
	$(MAKE) release/q2ded.exe

build/server/%.o: %.c
	@echo "===> CC $<"
	${Q}stuff/misc/mkdir.exe -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

release/q2ded.exe : CFLAGS += -DDEDICATED_ONLY
release/q2ded.exe : LDFLAGS += -lz

ifeq ($(WITH_ZIP),yes)
release/q2ded.exe : CFLAGS += -DZIP -DNOUNCRYPT
release/q2ded.exe : LDFLAGS += -lz
endif
else
server:
	@echo "===> Building q2ded"
	${Q}mkdir -p release
	$(MAKE) release/q2ded

build/server/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

release/q2ded : CFLAGS += -DDEDICATED_ONLY

ifeq ($(WITH_ZIP),yes)
release/q2ded : CFLAGS += -DZIP -DNOUNCRYPT
release/q2ded : LDFLAGS += -lz
endif
endif

# ----------

# The baseq2 game
ifeq ($(OSTYPE), Windows)
game:
	@echo "===> Building baseq2/game.dll"
	${Q}stuff/misc/mkdir.exe -p release/baseq2
	$(MAKE) release/baseq2/game.dll

build/baseq2/%.o: %.c
	@echo "===> CC $<"
	${Q}stuff/misc/mkdir.exe -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

release/baseq2/game.dll : LDFLAGS += -shared
else
game:
	@echo "===> Building baseq2/game.so"
	${Q}mkdir -p release/baseq2
	$(MAKE) release/baseq2/game.so

build/baseq2/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

release/baseq2/game.so : CFLAGS += -fPIC
release/baseq2/game.so : LDFLAGS += -shared
endif

# ----------

# Used by the game
GAME_OBJS_ = \
	src/common/shared/flash.o \
	src/common/shared/rand.o \
	src/common/shared/shared.o \
	src/game/g_ai.o \
	src/game/g_chase.o \
	src/game/g_cmds.o \
	src/game/g_combat.o \
	src/game/g_func.o \
	src/game/g_items.o \
	src/game/g_main.o \
	src/game/g_misc.o \
	src/game/g_monster.o \
	src/game/g_phys.o \
	src/game/g_spawn.o \
	src/game/g_svcmds.o \
	src/game/g_target.o \
	src/game/g_trigger.o \
	src/game/g_turret.o \
	src/game/g_utils.o \
	src/game/g_weapon.o \
	src/game/monster/berserker/berserker.o \
	src/game/monster/boss2/boss2.o \
	src/game/monster/boss3/boss3.o \
	src/game/monster/boss3/boss31.o \
	src/game/monster/boss3/boss32.o \
	src/game/monster/brain/brain.o \
	src/game/monster/chick/chick.o \
	src/game/monster/flipper/flipper.o \
	src/game/monster/float/float.o \
	src/game/monster/flyer/flyer.o \
	src/game/monster/gladiator/gladiator.o \
	src/game/monster/gunner/gunner.o \
	src/game/monster/hover/hover.o \
	src/game/monster/infantry/infantry.o \
	src/game/monster/insane/insane.o \
	src/game/monster/medic/medic.o \
	src/game/monster/misc/move.o \
	src/game/monster/mutant/mutant.o \
	src/game/monster/parasite/parasite.o \
	src/game/monster/soldier/soldier.o \
	src/game/monster/supertank/supertank.o \
	src/game/monster/tank/tank.o \
	src/game/player/client.o \
	src/game/player/hud.o \
	src/game/player/trail.o \
	src/game/player/view.o \
	src/game/player/weapon.o \
	src/game/savegame/savegame.o

# ----------

# Used by the client
CLIENT_OBJS_ := \
	src/backends/generic/qal.o \
	src/backends/generic/vid.o \
 	src/backends/generic/qgl.o \
	src/backends/sdl/cd.o \
	src/backends/sdl/input.o \
	src/backends/sdl/refresh.o \
	src/backends/sdl/sound.o \
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
	src/client/refresh/r_draw.o \
	src/client/refresh/r_image.o \
	src/client/refresh/r_light.o \
	src/client/refresh/r_lightmap.o \
	src/client/refresh/r_main.o \
	src/client/refresh/r_mesh.o \
	src/client/refresh/r_misc.o \
	src/client/refresh/r_model.o \
	src/client/refresh/r_scrap.o \
	src/client/refresh/r_surf.o \
	src/client/refresh/r_warp.o \
	src/client/refresh/files/md2.o \
	src/client/refresh/files/pcx.o \
	src/client/refresh/files/sp2.o \
	src/client/refresh/files/tga.o \
	src/client/refresh/files/jpeg.o \
	src/client/refresh/files/wal.o \
	src/client/menu/menu.o \
	src/client/menu/qmenu.o \
	src/client/menu/videomenu.o \
	src/client/sound/ogg.o \
	src/client/sound/openal.o \
	src/client/sound/sound.o \
	src/client/sound/wave.o \
	src/common/argproc.o \
	src/common/clientserver.o \
	src/common/collision.o \
	src/common/crc.o \
	src/common/cmdparser.o \
	src/common/cvar.o \
	src/common/filesystem.o \
	src/common/glob.o \
	src/common/md4.o \
	src/common/movemsg.o \
	src/common/misc.o \
	src/common/netchan.o \
	src/common/pmove.o \
	src/common/szone.o \
	src/common/zone.o \
	src/common/shared/flash.o \
	src/common/shared/rand.o \
	src/common/shared/shared.o \
	src/common/unzip/ioapi.o \
	src/common/unzip/unzip.o \
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

ifeq ($(OSTYPE), Windows)
CLIENT_OBJS_ += \
	src/backends/windows/network.o \
	src/backends/windows/system.o \
	src/backends/windows/shared/mem.o
else
CLIENT_OBJS_ += \
	src/backends/unix/main.o \
	src/backends/unix/network.o \
	src/backends/unix/signalhandler.o \
	src/backends/unix/system.o \
	src/backends/unix/shared/hunk.o
endif

ifeq ($(OSTYPE), Darwin)
CLIENT_OBJS_ += src/backends/sdl_osx/SDLMain.o
endif

# ----------

# Used by the server
SERVER_OBJS_ := \
	src/common/argproc.o \
	src/common/clientserver.o \
	src/common/collision.o \
	src/common/crc.o \
	src/common/cmdparser.o \
	src/common/cvar.o \
	src/common/filesystem.o \
	src/common/glob.o \
	src/common/md4.o \
	src/common/misc.o \
	src/common/movemsg.o \
	src/common/netchan.o \
	src/common/pmove.o \
	src/common/szone.o \
	src/common/zone.o \
	src/common/shared/rand.o \
	src/common/shared/shared.o \
	src/common/unzip/ioapi.o \
	src/common/unzip/unzip.o \
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

ifeq ($(OSTYPE), Windows)
SERVER_OBJS_ += \
	src/backends/windows/network.o \
	src/backends/windows/system.o \
	src/backends/windows/shared/mem.o
else
SERVER_OBJS_ += \
	src/backends/unix/main.o \
	src/backends/unix/network.o \
	src/backends/unix/signalhandler.o \
	src/backends/unix/system.o \
	src/backends/unix/shared/hunk.o
endif

# ----------

# Rewrite pathes to our object directory
CLIENT_OBJS = $(patsubst %,build/client/%,$(CLIENT_OBJS_))
SERVER_OBJS = $(patsubst %,build/server/%,$(SERVER_OBJS_))
GAME_OBJS = $(patsubst %,build/baseq2/%,$(GAME_OBJS_))

# ----------

# Generate header dependencies
CLIENT_DEPS= $(CLIENT_OBJS:.o=.d)
SERVER_DEPS= $(SERVER_OBJS:.o=.d)
GAME_DEPS= $(GAME_OBJS:.o=.d)

# ----------

# Suck header dependencies in
-include $(CLIENT_DEPS)
-include $(SERVER_DEPS)
-include $(GAME_DEPS)

# ----------

# release/quake2
ifeq ($(OSTYPE), Windows)
release/quake2.exe : $(CLIENT_OBJS) icon
	@echo "===> LD $@"
	${Q}$(CC) build/icon/icon.res $(CLIENT_OBJS) $(LDFLAGS) $(SDLLDFLAGS) -o $@
else
release/quake2 : $(CLIENT_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(CLIENT_OBJS) $(LDFLAGS) $(SDLLDFLAGS) $(X11LDFLAGS) -o $@
endif

# release/q2ded
ifeq ($(OSTYPE), Windows)
release/q2ded.exe : $(SERVER_OBJS) icon
	@echo "===> LD $@.exe"
	${Q}$(CC) build/icon/icon.res $(SERVER_OBJS) $(LDFLAGS) -o $@
else
release/q2ded : $(SERVER_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(SERVER_OBJS) $(LDFLAGS) -o $@
endif

# release/baseq2/game.so
ifeq ($(OSTYPE), Windows)
release/baseq2/game.dll : $(GAME_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(GAME_OBJS) $(LDFLAGS) -o $@
else
release/baseq2/game.so : $(GAME_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(GAME_OBJS) $(LDFLAGS) -o $@
endif

# ----------
