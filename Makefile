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
# Further dependencies:                                  #
#  - libogg                                              #
#  - libvorbis                                           #
#  - OpenAL                                              #
#  - zlib                                                #
#                                                        #
# Platforms:                                             #
#  - FreeBSD                                             #
#  - Linux                                               #
#  - OpenBSD                                             #
#  - OS X                                                #
#  - Windows (MinGW)                                     #
# ------------------------------------------------------ #

# User configurable options
# -------------------------

# Enables CD audio playback. CD audio playback is used
# for the background music and doesn't add any further
# dependencies. It should work on all platforms where
# CD playback is supported by SDL.
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

# Enables optional runtime loading of OpenAL (dlopen or
# similar). If set to "no", the library is linked in at
# compile time in the normal way. On Windows this option
# is ignored, OpenAL is always loaded at runtime.
DLOPEN_OPENAL:=no

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
# instead of backslashed (\) should be used! The string
# MUST NOT be surrounded by quotation marks!
WITH_SYSTEMDIR:=""

# This will set the architectures of the OSX-binaries.
# You have to make sure your libs/frameworks supports
# these architectures! To build an universal ppc-compatible
# one would add -arch ppc for example.
OSX_ARCH:=-arch $(shell uname -m | sed -e s/i.86/i386/)

# This will set the build options to create an MacOS .app-bundle.
# The app-bundle itself will not be created, but the runtime paths
# will be set to expect the game-data in *.app/
# Contents/Resources
OSX_APP:=yes

# This is an optional configuration file, it'll be used in
# case of presence.
CONFIG_FILE := config.mk

# ----------

# In case a of a configuration file being present, we'll just use it
ifeq ($(wildcard $(CONFIG_FILE)), $(CONFIG_FILE))
include $(CONFIG_FILE)
endif

# Detect the OS
ifdef SystemRoot
OSTYPE := Windows
else
OSTYPE ?= $(shell uname -s)
endif

# Special case for MinGW
ifneq (,$(findstring MINGW,$(OSTYPE)))
OSTYPE := Windows
endif

# Detect the architecture
ifeq ($(OSTYPE), Windows)
ifdef PROCESSOR_ARCHITEW6432
# 64 bit Windows
ARCH ?= $(PROCESSOR_ARCHITEW6432)
else
# 32 bit Windows
ARCH ?= $(PROCESSOR_ARCHITECTURE)
endif
else
# Normalize some abiguous ARCH strings
ARCH ?= $(shell uname -m | sed -e 's/i.86/i386/' -e 's/amd64/x86_64/' -e 's/^arm.*/arm/')
endif

# Disable CDA for SDL2
ifeq ($(WITH_SDL2),yes)
ifeq ($(WITH_CDA),yes)
WITH_CDA:=no

# Evil hack to tell the "all" target
# that CDA was disabled because SDL2
# is enabled.
CDA_DISABLED:=yes
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
		  -Wall -pipe -g -fwrapv
CFLAGS += $(OSX_ARCH)
else
CFLAGS := -O2 -fno-strict-aliasing -fomit-frame-pointer \
		  -Wall -pipe -g -ggdb -MMD -fwrapv
endif

# ----------

# Defines the operating system and architecture
CFLAGS += -DYQ2OSTYPE=\"$(OSTYPE)\" -DYQ2ARCH=\"$(ARCH)\"

# ----------

# https://reproducible-builds.org/specs/source-date-epoch/
ifdef SOURCE_DATE_EPOCH
CFLAGS += -DBUILD_DATE=\"$(shell date --utc --date="@${SOURCE_DATE_EPOCH}" +"%b %_d %Y" | sed -e 's/ /\\ /g')\"
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

# On Windows / MinGW $(CC) is
# undefined by default.
ifeq ($(OSTYPE),Windows)
CC := gcc
endif

# ----------

# Extra CFLAGS for SDL
ifeq ($(WITH_SDL2),yes)
ifeq ($(OSTYPE),Windows)
SDLCFLAGS := $(shell /custom/bin/sdl2-config --cflags)
else
SDLCFLAGS := $(shell sdl2-config --cflags)
endif
else # not SDL2
ifeq ($(OSTYPE),Windows)
SDLCFLAGS :=
else
SDLCFLAGS := $(shell sdl-config --cflags)
endif
endif # SDL2

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
else ifeq ($(OSTYPE),Windows)
INCLUDE := -I/custom/include
else ifeq ($(OSTYPE), Darwin)
INCLUDE := -I/usr/local/opt/openal-soft/include
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
LDFLAGS := -L/custom/lib -lws2_32 -lwinmm
else ifeq ($(OSTYPE), Darwin)
LDFLAGS := $(OSX_ARCH) -lm -L/usr/local/opt/openal-soft/lib
endif

# ----------

# Extra LDFLAGS for SDL
ifeq ($(OSTYPE), Windows)
ifeq ($(WITH_SDL2),yes)
SDLLDFLAGS := $(shell /custom/bin/sdl2-config --libs)
else # not SDL2
SDLLDFLAGS := -lSDL
endif # SDL2
else ifeq ($(OSTYPE), Darwin)
ifeq ($(WITH_SDL2),yes)
SDLLDFLAGS := -lSDL2 -framework OpenGL -framework Cocoa
else # not SDL2
SDLLDFLAGS := -lSDL -framework OpenGL -framework Cocoa
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
	@echo "WITH_SDL2 = $(WITH_SDL2)"
	@echo "WITH_X11GAMMA = $(WITH_X11GAMMA)"
	@echo "WITH_ZIP = $(WITH_ZIP)"
	@echo "WITH_SYSTEMWIDE = $(WITH_SYSTEMWIDE)"
	@echo "WITH_SYSTEMDIR = $(WITH_SYSTEMDIR)"
	@echo "============================"
	@echo ""
ifeq ($(WITH_SDL2),yes)
ifeq ($(CDA_DISABLED),yes)
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
	${Q}mkdir -p build/icon
	${Q}windres src/backends/windows/icon.rc -O COFF -o build/icon/icon.res
endif

# ----------

# Cleanup
clean:
	@echo "===> CLEAN"
	${Q}rm -Rf build release

# ----------

# The client
ifeq ($(OSTYPE), Windows)
client:
	@echo "===> Building quake2.exe"
	${Q}mkdir -p release
	$(MAKE) release/quake2.exe

build/client/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(INCLUDE) -o $@ $<

ifeq ($(WITH_CDA),yes)
release/quake2.exe : CFLAGS += -DCDA
endif

ifeq ($(WITH_OGG),yes)
release/quake2.exe : CFLAGS += -DOGG
release/quake2.exe : LDFLAGS += -lvorbisfile -lvorbis -logg
endif

ifeq ($(WITH_OPENAL),yes)
release/quake2.exe : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"openal32.dll"' -DDLOPEN_OPENAL
endif

ifeq ($(WITH_ZIP),yes)
release/quake2.exe : CFLAGS += -DZIP -DNOUNCRYPT
release/quake2.exe : LDFLAGS += -lz
endif

ifeq ($(WITH_SDL2),yes)
release/quake2.exe : CFLAGS += -DSDL2
endif

release/quake2.exe : LDFLAGS += -mwindows -lopengl32
else # not Windows
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
release/quake2 : LDFLAGS += -lvorbis -lvorbisfile -logg
endif

ifeq ($(WITH_OPENAL),yes)
ifeq ($(DLOPEN_OPENAL),yes)
ifeq ($(OSTYPE), OpenBSD)
release/quake2 : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"libopenal.so"' -DDLOPEN_OPENAL
else ifeq ($(OSTYPE), Darwin)
release/quake2 : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"libopenal.dylib"' -I/usr/local/opt/openal-soft/include -DDLOPEN_OPENAL
release/quake2 : LDFLAGS += -L/usr/local/opt/openal-soft/lib
else
release/quake2 : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"libopenal.so.1"' -DDLOPEN_OPENAL
endif
else # !DLOPEN_OPENAL
release/quake2 : CFLAGS += -DUSE_OPENAL -I/usr/local/opt/openal-soft/include
release/quake2 : LDFLAGS += -lopenal -L/usr/local/opt/openal-soft/lib
endif # !DLOPEN_OPENAL
endif # WITH_OPENAL

ifeq ($(WITH_ZIP),yes)
release/quake2 : CFLAGS += -DZIP -DNOUNCRYPT
release/quake2 : LDFLAGS += -lz
endif

ifeq ($(WITH_X11GAMMA),yes)
release/quake2 : CFLAGS += -DX11GAMMA
endif

ifeq ($(WITH_SDL2),yes)
release/quake2 : CFLAGS += -DSDL2
endif
 
ifneq ($(OSTYPE), Darwin)
release/quake2 : LDFLAGS += -lGL
endif

ifeq ($(OSTYPE), FreeBSD)
release/quake2 : LDFLAGS += -Wl,-z,origin,-rpath='$$ORIGIN/lib'
else ifeq ($(OSTYPE), Linux)
release/quake2 : LDFLAGS += -Wl,-z,origin,-rpath='$$ORIGIN/lib'
endif

ifeq ($(WITH_SYSTEMWIDE),yes)
ifneq ($(WITH_SYSTEMDIR),"")
ifeq ($(OSTYPE), FreeBSD)
release/quake2 : LDFLAGS += -Wl,-z,origin,-rpath='$(WITH_SYSTEMDIR)/lib'
else ifeq ($(OSTYPE), Linux)
release/quake2 : LDFLAGS += -Wl,-z,origin,-rpath='$(WITH_SYSTEMDIR)/lib'
endif
else
ifeq ($(OSTYPE), FreeBSD)
release/quake2 : LDFLAGS += -Wl,-z,origin,-rpath='/usr/share/games/quake2/lib'
else ifeq ($(OSTYPE), Linux)
release/quake2 : LDFLAGS += -Wl,-z,origin,-rpath='/usr/share/games/quake2/lib'
endif
endif
endif
endif

# ----------

# The server
ifeq ($(OSTYPE), Windows)
server:
	@echo "===> Building q2ded"
	${Q}mkdir -p release
	$(MAKE) release/q2ded.exe

build/server/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

release/q2ded.exe : CFLAGS += -DDEDICATED_ONLY
release/q2ded.exe : LDFLAGS += -lz

ifeq ($(WITH_ZIP),yes)
release/q2ded.exe : CFLAGS += -DZIP -DNOUNCRYPT
release/q2ded.exe : LDFLAGS += -lz
endif
else # not Windows
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
	${Q}mkdir -p release/baseq2
	$(MAKE) release/baseq2/game.dll

build/baseq2/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

release/baseq2/game.dll : LDFLAGS += -shared
else ifeq ($(OSTYPE), Darwin)
game:
	@echo "===> Building baseq2/game.dylib"
	${Q}mkdir -p release/baseq2
	$(MAKE) release/baseq2/game.dylib

build/baseq2/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

release/baseq2/game.dylib : CFLAGS += -fPIC
release/baseq2/game.dylib : LDFLAGS += -shared
else # not Windows or Darwin
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
	src/backends/generic/misc.o \
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
	src/client/refresh/files/stb.o \
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
	src/server/sv_world.o \
	src/backends/generic/misc.o

ifeq ($(OSTYPE), Windows)
SERVER_OBJS_ += \
	src/backends/windows/network.o \
	src/backends/windows/system.o \
	src/backends/windows/shared/mem.o
else # not Windows
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
	$(Q)strip $@
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
	$(Q)strip $@
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
	$(Q)strip $@
else ifeq ($(OSTYPE), Darwin)
release/baseq2/game.dylib : $(GAME_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(GAME_OBJS) $(LDFLAGS) -o $@
else
release/baseq2/game.so : $(GAME_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(GAME_OBJS) $(LDFLAGS) -o $@
endif

# ----------
