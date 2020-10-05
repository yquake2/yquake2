# ------------------------------------------------------ #
# Makefile for the "Yamagi Quake 2 Client"               #
#                                                        #
# Just type "make" to compile the                        #
#  - Client (quake2)                                     #
#  - Server (q2ded)                                      #
#  - Quake II Game (baseq2)                              #
#  - Renderer libraries (gl1, gl3, soft)                 #
#                                                        #
# Base dependencies:                                     #
#  - SDL 2.0                                             #
#  - libGL                                               #
#                                                        #
# Optional dependencies:                                 #
#  - CURL                                                #
#  - OpenAL                                              #
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

# Enables HTTP support through cURL. Used for
# HTTP download.
WITH_CURL:=yes

# Enables the optional OpenAL sound system.
# To use it your system needs libopenal.so.1
# or openal32.dll (we recommend openal-soft)
# installed
WITH_OPENAL:=yes

# Sets an RPATH to $ORIGIN/lib. It can be used to
# inject custom libraries, e.g. a patches libSDL.so
# or libopenal.so. Not supported on Windows.
WITH_RPATH:=yes

# Enable systemwide installation of game assets.
WITH_SYSTEMWIDE:=no

# This will set the default SYSTEMDIR, a non-empty string
# would actually be used. On Windows normals slashes (/)
# instead of backslashed (\) should be used! The string
# MUST NOT be surrounded by quotation marks!
WITH_SYSTEMDIR:=""

# This will set the build options to create an MacOS .app-bundle.
# The app-bundle itself will not be created, but the runtime paths
# will be set to expect the game-data in *.app/
# Contents/Resources
OSX_APP:=yes

# Build vulkan render
WITH_REFVK:=yes

# This is an optional configuration file, it'll be used in
# case of presence.
CONFIG_FILE:=config.mk

# ----------

# In case a of a configuration file being present, we'll just use it
ifeq ($(wildcard $(CONFIG_FILE)), $(CONFIG_FILE))
include $(CONFIG_FILE)
endif

# Detect the OS
ifdef SystemRoot
YQ2_OSTYPE ?= Windows
else
YQ2_OSTYPE ?= $(shell uname -s)
endif

# Special case for MinGW
ifneq (,$(findstring MINGW,$(YQ2_OSTYPE)))
YQ2_OSTYPE := Windows
endif

# Detect the architecture
ifeq ($(YQ2_OSTYPE), Windows)
ifdef PROCESSOR_ARCHITEW6432
# 64 bit Windows
YQ2_ARCH ?= $(PROCESSOR_ARCHITEW6432)
else
# 32 bit Windows
YQ2_ARCH ?= $(PROCESSOR_ARCHITECTURE)
endif
else
# Normalize some abiguous YQ2_ARCH strings
YQ2_ARCH ?= $(shell uname -m | sed -e 's/i.86/i386/' -e 's/amd64/x86_64/' -e 's/^arm.*/arm/')
endif

# On Windows / MinGW $(CC) is undefined by default.
ifeq ($(YQ2_OSTYPE),Windows)
CC ?= gcc
endif

# Detect the compiler
ifeq ($(shell $(CC) -v 2>&1 | grep -c "clang version"), 1)
COMPILER := clang
COMPILERVER := $(shell $(CC)  -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$$/&00/')
else ifeq ($(shell $(CC) -v 2>&1 | grep -c -E "(gcc version|gcc-Version)"), 1)
COMPILER := gcc
COMPILERVER := $(shell $(CC)  -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$$/&00/')
else
COMPILER := unknown
endif

# ASAN includes DEBUG
ifdef ASAN
DEBUG=1
endif

# ----------

# Base CFLAGS. These may be overridden by the environment.
# Highest supported optimizations are -O2, higher levels
# will likely break this crappy code.
ifdef DEBUG
CFLAGS ?= -O0 -g -Wall -pipe
ifdef ASAN
CFLAGS += -fsanitize=address
endif
else
CFLAGS ?= -O2 -Wall -pipe -fomit-frame-pointer
endif

# Always needed are:
#  -fno-strict-aliasing since the source doesn't comply
#   with strict aliasing rules and it's next to impossible
#   to get it there...
#  -fwrapv for defined integer wrapping. MSVC6 did this
#   and the game code requires it.
#  -fvisibility=hidden to keep symbols hidden. This is
#   mostly best practice and not really necessary.
override CFLAGS += -std=gnu99 -fno-strict-aliasing -fwrapv -fvisibility=hidden

# -MMD to generate header dependencies. Unsupported by
#  the Clang shipped with OS X.
ifneq ($(YQ2_OSTYPE), Darwin)
override CFLAGS += -MMD
endif

# OS X architecture.
ifeq ($(YQ2_OSTYPE), Darwin)
override CFLAGS += -arch $(YQ2_ARCH)
endif

# ----------

# ARM needs a sane minimum architecture. We need the `yield`
# opcode, arm6k is the first iteration that supports it. arm6k
# is also the first Raspberry PI generation and older hardware
# is likely too slow to run the game. We're not enforcing the
# minimum architecture, but if you're build for something older
# like arm5 the `yield` opcode isn't compiled in and the game
# (especially q2ded) will consume more CPU time than necessary.
ifeq ($(YQ2_ARCH), arm)
CFLAGS += -march=armv6k
endif

# ----------

# Switch of some annoying warnings.
ifeq ($(COMPILER), clang)
	# -Wno-missing-braces because otherwise clang complains
	#  about totally valid 'vec3_t bla = {0}' constructs.
	override CFLAGS += -Wno-missing-braces
else ifeq ($(COMPILER), gcc)
	# GCC 8.0 or higher.
	ifeq ($(shell test $(COMPILERVER) -ge 80000; echo $$?),0)
	    # -Wno-format-truncation and -Wno-format-overflow
		# because GCC spams about 50 false positives.
		override CFLAGS += -Wno-format-truncation -Wno-format-overflow
	endif
endif

# ----------

# Defines the operating system and architecture
override CFLAGS += -DYQ2OSTYPE=\"$(YQ2_OSTYPE)\" -DYQ2ARCH=\"$(YQ2_ARCH)\"

# ----------

# For reproduceable builds, look here for details:
# https://reproducible-builds.org/specs/source-date-epoch/
ifdef SOURCE_DATE_EPOCH
override CFLAGS += -DBUILD_DATE=\"$(shell date --utc --date="@${SOURCE_DATE_EPOCH}" +"%b %_d %Y" | sed -e 's/ /\\ /g')\"
endif

# ----------

# If we're building with gcc for i386 let's define -ffloat-store.
# This helps the old and crappy x87 FPU to produce correct values.
# Would be nice if Clang had something comparable.
ifeq ($(YQ2_ARCH), i386)
ifeq ($(COMPILER), gcc)
override CFLAGS += -ffloat-store
endif
endif

# Force SSE math on x86_64. All sane compilers should do this
# anyway, just to protect us from broken Linux distros.
ifeq ($(YQ2_ARCH), x86_64)
override CFLAGS += -mfpmath=sse
endif

# ----------

# Systemwide installation.
ifeq ($(WITH_SYSTEMWIDE),yes)
override CFLAGS += -DSYSTEMWIDE
ifneq ($(WITH_SYSTEMDIR),"")
override CFLAGS += -DSYSTEMDIR=\"$(WITH_SYSTEMDIR)\"
endif
endif

# ----------

# We don't support encrypted ZIP files.
ZIPCFLAGS := -DNOUNCRYPT

# Just set IOAPI_NO_64 on everything that's not Linux or Windows,
# otherwise minizip will use fopen64(), fseek64() and friends that
# may be unavailable. This is - of course - not really correct, in
# a better world we would set -DIOAPI_NO_64 to force everything to
# fopen(), fseek() and so on and -D_FILE_OFFSET_BITS=64 to let the
# libc headers do their work. Currently we can't do that because
# Quake II uses nearly everywere int instead of off_t...
#
# This may have the side effect that ZIP files larger than 2GB are
# unsupported. But I doubt that anyone has such large files, they
# would likely hit other internal limits.
ifneq ($(YQ2_OSTYPE),Windows)
ifneq ($(YQ2_OSTYPE),Linux)
ZIPCFLAGS += -DIOAPI_NO_64
endif
endif

# ----------

# Extra CFLAGS for SDL.
SDLCFLAGS := $(shell sdl2-config --cflags)

# ----------

# Base include path.
ifeq ($(YQ2_OSTYPE),Linux)
INCLUDE ?= -I/usr/include
else ifeq ($(YQ2_OSTYPE),FreeBSD)
INCLUDE ?= -I/usr/local/include
else ifeq ($(YQ2_OSTYPE),NetBSD)
INCLUDE ?= -I/usr/X11R7/include -I/usr/pkg/include
else ifeq ($(YQ2_OSTYPE),OpenBSD)
INCLUDE ?= -I/usr/local/include
else ifeq ($(YQ2_OSTYPE),Windows)
INCLUDE ?= -I/usr/include
endif

# ----------

# Local includes for GLAD.
GLAD_INCLUDE = -Isrc/client/refresh/gl3/glad/include

# ----------

# Base LDFLAGS. This is just the library path.
ifeq ($(YQ2_OSTYPE),Linux)
LDFLAGS ?= -L/usr/lib
else ifeq ($(YQ2_OSTYPE),FreeBSD)
LDFLAGS ?= -L/usr/local/lib
else ifeq ($(YQ2_OSTYPE),NetBSD)
LDFLAGS ?= -L/usr/X11R7/lib -Wl,-R/usr/X11R7/lib -L/usr/pkg/lib -Wl,-R/usr/pkg/lib
else ifeq ($(YQ2_OSTYPE),OpenBSD)
LDFLAGS ?= -L/usr/local/lib
else ifeq ($(YQ2_OSTYPE),Windows)
LDFLAGS ?= -L/usr/lib
endif

# Link address sanitizer if requested.
ifdef ASAN
LDFLAGS += -fsanitize=address
endif

# Required libraries.
ifeq ($(YQ2_OSTYPE),Linux)
override LDFLAGS += -lm -ldl -rdynamic
else ifeq ($(YQ2_OSTYPE),FreeBSD)
override LDFLAGS += -lm
else ifeq ($(YQ2_OSTYPE),NetBSD)
override LDFLAGS += -lm
else ifeq ($(YQ2_OSTYPE),OpenBSD)
override LDFLAGS += -lm
else ifeq ($(YQ2_OSTYPE),Windows)
override LDFLAGS += -lws2_32 -lwinmm -static-libgcc
else ifeq ($(YQ2_OSTYPE), Darwin)
override LDFLAGS += -arch $(YQ2_ARCH)
else ifeq ($(YQ2_OSTYPE), Haiku)
override LDFLAGS += -lm -lnetwork
endif

ifneq ($(YQ2_OSTYPE), Darwin)
ifneq ($(YQ2_OSTYPE), OpenBSD)
# For some reason the OSX & OpenBSD
# linker doesn't support this...
override LDFLAGS += -Wl,--no-undefined
endif
endif

# ----------

# Extra LDFLAGS for SDL
ifeq ($(YQ2_OSTYPE), Darwin)
SDLLDFLAGS := -lSDL2
else # not Darwin
SDLLDFLAGS := $(shell sdl2-config --libs)
endif # Darwin

# The renderer libs don't need libSDL2main, libmingw32 or -mwindows.
ifeq ($(YQ2_OSTYPE), Windows)
DLL_SDLLDFLAGS = $(subst -mwindows,,$(subst -lmingw32,,$(subst -lSDL2main,,$(SDLLDFLAGS))))
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
.PHONY : all client game icon server ref_gl1 ref_gl3 ref_soft ref_vk

# ----------

# Builds everything
all: config client server game ref_gl1 ref_gl3 ref_soft ref_vk

# ----------

# Print config values
config:
	@echo "Build configuration"
	@echo "============================"
	@echo "WITH_CURL = $(WITH_CURL)"
	@echo "WITH_OPENAL = $(WITH_OPENAL)"
	@echo "WITH_REFVK = $(WITH_REFVK)"
	@echo "WITH_RPATH = $(WITH_RPATH)"
	@echo "WITH_SYSTEMWIDE = $(WITH_SYSTEMWIDE)"
	@echo "WITH_SYSTEMDIR = $(WITH_SYSTEMDIR)"
	@echo "============================"
	@echo ""

# ----------

# Special target to compile the icon on Windows
ifeq ($(YQ2_OSTYPE), Windows)
icon:
	@echo "===> WR build/icon/icon.res"
	${Q}mkdir -p build/icon
	${Q}windres src/backends/windows/icon.rc -O COFF -o build/icon/icon.res
endif

# ----------

# Cleanup
clean:
	@echo "===> CLEAN"
	${Q}rm -Rf build release/*

cleanall:
	@echo "===> CLEAN"
	${Q}rm -Rf build release

# ----------

# The client
ifeq ($(YQ2_OSTYPE), Windows)
client:
	@echo "===> Building yquake2.exe"
	${Q}mkdir -p release
	$(MAKE) release/yquake2.exe
	@echo "===> Building quake2.exe Wrapper"
	$(MAKE) release/quake2.exe

build/client/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(ZIPCFLAGS) $(INCLUDE) -o $@ $<

release/yquake2.exe : LDFLAGS += -mwindows

ifeq ($(WITH_CURL),yes)
release/yquake2.exe : CFLAGS += -DUSE_CURL
endif

ifeq ($(WITH_REFVK),yes)
release/yquake2.exe : CFLAGS += -DUSE_REFVK
endif

ifeq ($(WITH_OPENAL),yes)
release/yquake2.exe : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"openal32.dll"'
endif

else # not Windows

client:
	@echo "===> Building quake2"
	${Q}mkdir -p release
	$(MAKE) release/quake2

ifeq ($(YQ2_OSTYPE), Darwin)
build/client/%.o : %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -arch $(YQ2_ARCH) -x objective-c -c $(CFLAGS) $(SDLCFLAGS) $(ZIPCFLAGS) $(INCLUDE)  $< -o $@
else
build/client/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(ZIPCFLAGS) $(INCLUDE) -o $@ $<
endif

release/quake2 : CFLAGS += -Wno-unused-result

ifeq ($(WITH_CURL),yes)
release/quake2 : CFLAGS += -DUSE_CURL
endif

ifeq ($(WITH_REFVK),yes)
release/quake2 : CFLAGS += -DUSE_REFVK
endif

ifeq ($(WITH_OPENAL),yes)
ifeq ($(YQ2_OSTYPE), OpenBSD)
release/quake2 : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"libopenal.so"'
else ifeq ($(YQ2_OSTYPE), Darwin)
release/quake2 : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"libopenal.dylib"' -I/usr/local/opt/openal-soft/include
release/quake2 : LDFLAGS += -L/usr/local/opt/openal-soft/lib -rpath /usr/local/opt/openal-soft/lib
else
release/quake2 : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"libopenal.so.1"'
endif
endif

ifeq ($(YQ2_OSTYPE), Linux)
release/quake2 : CFLAGS += -DHAVE_EXECINFO
endif

ifeq ($(YQ2_OSTYPE), Darwin)
release/quake2 : CFLAGS += -DHAVE_EXECINFO
endif

ifeq ($(YQ2_OSTYPE), SunOS)
release/quake2 : CFLAGS += -DHAVE_EXECINFO
endif

ifeq ($(YQ2_OSTYPE), FreeBSD)
release/quake2 : CFLAGS += -DHAVE_EXECINFO
release/quake2 : LDFLAGS += -lexecinfo
endif

ifeq ($(YQ2_OSTYPE), NetBSD)
release/quake2 : CFLAGS += -DHAVE_EXECINFO
release/quake2 : LDFLAGS += -lexecinfo
endif

ifeq ($(YQ2_OSTYPE), OpenBSD)
release/quake2 : CFLAGS += -DHAVE_EXECINFO
release/quake2 : LDFLAGS += -lexecinfo
endif

ifeq ($(YQ2_OSTYPE), Haiku)
release/quake2 : CFLAGS += -DHAVE_EXECINFO
release/quake2 : LDFLAGS += -lexecinfo
endif

ifeq ($(WITH_RPATH),yes)
ifeq ($(YQ2_OSTYPE), Darwin)
release/quake2 : LDFLAGS += -Wl,-rpath,'@executable_path/lib'
else
release/quake2 : LDFLAGS += -Wl,-z,origin,-rpath='$$ORIGIN/lib'
endif
endif
endif

# ----------

# The server
ifeq ($(YQ2_OSTYPE), Windows)
server:
	@echo "===> Building q2ded"
	${Q}mkdir -p release
	$(MAKE) release/q2ded.exe

build/server/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(ZIPCFLAGS) $(INCLUDE) -o $@ $<

release/q2ded.exe : CFLAGS += -DDEDICATED_ONLY

else # not Windows

server:
	@echo "===> Building q2ded"
	${Q}mkdir -p release
	$(MAKE) release/q2ded

build/server/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(ZIPCFLAGS) $(INCLUDE) -o $@ $<

release/q2ded : CFLAGS += -DDEDICATED_ONLY -Wno-unused-result

ifeq ($(YQ2_OSTYPE), FreeBSD)
release/q2ded : LDFLAGS += -lexecinfo
endif
endif

# ----------

# The OpenGL 1.x renderer lib

ifeq ($(YQ2_OSTYPE), Windows)

ref_gl1:
	@echo "===> Building ref_gl1.dll"
	$(MAKE) release/ref_gl1.dll

release/ref_gl1.dll : LDFLAGS += -lopengl32 -shared

else ifeq ($(YQ2_OSTYPE), Darwin)

ref_gl1:
	@echo "===> Building ref_gl1.dylib"
	$(MAKE) release/ref_gl1.dylib


release/ref_gl1.dylib : LDFLAGS += -shared -framework OpenGL

else # not Windows or Darwin

ref_gl1:
	@echo "===> Building ref_gl1.so"
	$(MAKE) release/ref_gl1.so


release/ref_gl1.so : CFLAGS += -fPIC
release/ref_gl1.so : LDFLAGS += -shared -lGL

endif # OS specific ref_gl1 stuff

build/ref_gl1/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(INCLUDE) -o $@ $<

# ----------

# The OpenGL 3.x renderer lib

ifeq ($(YQ2_OSTYPE), Windows)

ref_gl3:
	@echo "===> Building ref_gl3.dll"
	$(MAKE) release/ref_gl3.dll

release/ref_gl3.dll : LDFLAGS += -shared

else ifeq ($(YQ2_OSTYPE), Darwin)

ref_gl3:
	@echo "===> Building ref_gl3.dylib"
	$(MAKE) release/ref_gl3.dylib


release/ref_gl3.dylib : LDFLAGS += -shared

else # not Windows or Darwin

ref_gl3:
	@echo "===> Building ref_gl3.so"
	$(MAKE) release/ref_gl3.so


release/ref_gl3.so : CFLAGS += -fPIC
release/ref_gl3.so : LDFLAGS += -shared

endif # OS specific ref_gl3 stuff

build/ref_gl3/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(INCLUDE) $(GLAD_INCLUDE) -o $@ $<

# ----------

# The soft renderer lib

ifeq ($(YQ2_OSTYPE), Windows)

ref_soft:
	@echo "===> Building ref_soft.dll"
	$(MAKE) release/ref_soft.dll

release/ref_soft.dll : LDFLAGS += -shared

else ifeq ($(YQ2_OSTYPE), Darwin)

ref_soft:
	@echo "===> Building ref_soft.dylib"
	$(MAKE) release/ref_soft.dylib

release/ref_soft.dylib : LDFLAGS += -shared

else # not Windows or Darwin

ref_soft:
	@echo "===> Building ref_soft.so"
	$(MAKE) release/ref_soft.so

release/ref_soft.so : CFLAGS += -fPIC
release/ref_soft.so : LDFLAGS += -shared

endif # OS specific ref_soft stuff

build/ref_soft/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(INCLUDE) $(GLAD_INCLUDE) -o $@ $<

# ----------

# The vk renderer lib
ifeq ($(YQ2_OSTYPE), Windows)
ref_vk:
	@echo "===> Building ref_vk.dll"
	$(MAKE) release/ref_vk.dll

release/ref_vk.dll : LDFLAGS += -shared
else
ref_vk:
	@echo "===> Building ref_vk.so"
	$(MAKE) release/ref_vk.so

release/ref_vk.so : CFLAGS += -fPIC
release/ref_vk.so : LDFLAGS += -shared
endif

build/ref_vk/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(INCLUDE) -o $@ $<

# ----------

# The baseq2 game
ifeq ($(YQ2_OSTYPE), Windows)
game:
	@echo "===> Building baseq2/game.dll"
	${Q}mkdir -p release/baseq2
	$(MAKE) release/baseq2/game.dll

build/baseq2/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

release/baseq2/game.dll : LDFLAGS += -shared

else ifeq ($(YQ2_OSTYPE), Darwin)

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

release/baseq2/game.so : CFLAGS += -fPIC -Wno-unused-result
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
	src/client/curl/download.o \
	src/client/curl/qcurl.o \
	src/client/input/sdl.o \
	src/client/menu/menu.o \
	src/client/menu/qmenu.o \
	src/client/menu/videomenu.o \
	src/client/sound/sdl.o \
	src/client/sound/ogg.o \
	src/client/sound/openal.o \
	src/client/sound/qal.o \
	src/client/sound/sound.o \
	src/client/sound/wave.o \
	src/client/vid/glimp_sdl.o \
	src/client/vid/vid.o \
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
	src/common/frame.o \
	src/common/netchan.o \
	src/common/pmove.o \
	src/common/szone.o \
	src/common/zone.o \
	src/common/shared/flash.o \
	src/common/shared/rand.o \
	src/common/shared/shared.o \
	src/common/unzip/ioapi.o \
	src/common/unzip/miniz.o \
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

ifeq ($(YQ2_OSTYPE), Windows)
CLIENT_OBJS_ += \
	src/backends/windows/main.o \
	src/backends/windows/network.o \
	src/backends/windows/system.o \
	src/backends/windows/shared/hunk.o
else
CLIENT_OBJS_ += \
	src/backends/unix/main.o \
	src/backends/unix/network.o \
	src/backends/unix/signalhandler.o \
	src/backends/unix/system.o \
	src/backends/unix/shared/hunk.o
endif

# ----------

REFGL1_OBJS_ := \
	src/client/refresh/gl1/qgl.o \
	src/client/refresh/gl1/gl1_draw.o \
	src/client/refresh/gl1/gl1_image.o \
	src/client/refresh/gl1/gl1_light.o \
	src/client/refresh/gl1/gl1_lightmap.o \
	src/client/refresh/gl1/gl1_main.o \
	src/client/refresh/gl1/gl1_mesh.o \
	src/client/refresh/gl1/gl1_misc.o \
	src/client/refresh/gl1/gl1_model.o \
	src/client/refresh/gl1/gl1_scrap.o \
	src/client/refresh/gl1/gl1_surf.o \
	src/client/refresh/gl1/gl1_warp.o \
	src/client/refresh/gl1/gl1_sdl.o \
	src/client/refresh/gl1/gl1_md2.o \
	src/client/refresh/gl1/gl1_sp2.o \
	src/client/refresh/files/pcx.o \
	src/client/refresh/files/stb.o \
	src/client/refresh/files/wal.o \
	src/client/refresh/files/pvs.o \
	src/common/shared/shared.o \
	src/common/md4.o

ifeq ($(YQ2_OSTYPE), Windows)
REFGL1_OBJS_ += \
	src/backends/windows/shared/hunk.o
else # not Windows
REFGL1_OBJS_ += \
	src/backends/unix/shared/hunk.o
endif

# ----------

REFGL3_OBJS_ := \
	src/client/refresh/gl3/gl3_draw.o \
	src/client/refresh/gl3/gl3_image.o \
	src/client/refresh/gl3/gl3_light.o \
	src/client/refresh/gl3/gl3_lightmap.o \
	src/client/refresh/gl3/gl3_main.o \
	src/client/refresh/gl3/gl3_mesh.o \
	src/client/refresh/gl3/gl3_misc.o \
	src/client/refresh/gl3/gl3_model.o \
	src/client/refresh/gl3/gl3_sdl.o \
	src/client/refresh/gl3/gl3_surf.o \
	src/client/refresh/gl3/gl3_warp.o \
	src/client/refresh/gl3/gl3_shaders.o \
	src/client/refresh/gl3/gl3_md2.o \
	src/client/refresh/gl3/gl3_sp2.o \
	src/client/refresh/gl3/glad/src/glad.o \
	src/client/refresh/files/pcx.o \
	src/client/refresh/files/stb.o \
	src/client/refresh/files/wal.o \
	src/client/refresh/files/pvs.o \
	src/common/shared/shared.o \
	src/common/md4.o

ifeq ($(YQ2_OSTYPE), Windows)
REFGL3_OBJS_ += \
	src/backends/windows/shared/hunk.o
else # not Windows
REFGL3_OBJS_ += \
	src/backends/unix/shared/hunk.o
endif

# ----------

REFSOFT_OBJS_ := \
	src/client/refresh/soft/sw_aclip.o \
	src/client/refresh/soft/sw_alias.o \
	src/client/refresh/soft/sw_bsp.o \
	src/client/refresh/soft/sw_draw.o \
	src/client/refresh/soft/sw_edge.o \
	src/client/refresh/soft/sw_image.o \
	src/client/refresh/soft/sw_light.o \
	src/client/refresh/soft/sw_main.o \
	src/client/refresh/soft/sw_misc.o \
	src/client/refresh/soft/sw_model.o \
	src/client/refresh/soft/sw_part.o \
	src/client/refresh/soft/sw_poly.o \
	src/client/refresh/soft/sw_polyset.o \
	src/client/refresh/soft/sw_rast.o \
	src/client/refresh/soft/sw_scan.o \
	src/client/refresh/soft/sw_sprite.o \
	src/client/refresh/soft/sw_surf.o \
	src/client/refresh/files/pcx.o \
	src/client/refresh/files/stb.o \
	src/client/refresh/files/wal.o \
	src/client/refresh/files/pvs.o \
	src/common/shared/shared.o \
	src/common/md4.o

ifeq ($(YQ2_OSTYPE), Windows)
REFSOFT_OBJS_ += \
	src/backends/windows/shared/hunk.o
else # not Windows
REFSOFT_OBJS_ += \
	src/backends/unix/shared/hunk.o
endif

# ----------

REFVK_OBJS_ := \
	src/client/refresh/vk/vk_buffer.o \
	src/client/refresh/vk/vk_cmd.o \
	src/client/refresh/vk/vk_common.o \
	src/client/refresh/vk/vk_device.o \
	src/client/refresh/vk/vk_draw.o \
	src/client/refresh/vk/vk_image.o \
	src/client/refresh/vk/vk_light.o \
	src/client/refresh/vk/vk_mesh.o \
	src/client/refresh/vk/vk_model.o \
	src/client/refresh/vk/vk_pipeline.o \
	src/client/refresh/vk/vk_rmain.o \
	src/client/refresh/vk/vk_rmisc.o \
	src/client/refresh/vk/vk_rsurf.o \
	src/client/refresh/vk/vk_shaders.o \
	src/client/refresh/vk/vk_swapchain.o \
	src/client/refresh/vk/vk_validation.o \
	src/client/refresh/vk/vk_warp.o \
	src/client/refresh/vk/vk_util.o \
	src/client/refresh/vk/volk/volk.o \
	src/client/refresh/files/pcx.o \
	src/client/refresh/files/stb.o \
	src/client/refresh/files/wal.o \
	src/client/refresh/files/pvs.o \
	src/common/shared/shared.o \
	src/common/md4.o

ifeq ($(YQ2_OSTYPE), Windows)
REFVK_OBJS_ += \
	src/backends/windows/shared/hunk.o
else # not Windows
REFVK_OBJS_ += \
	src/backends/unix/shared/hunk.o
endif

# ----------

# Used by the server
SERVER_OBJS_ := \
	src/backends/generic/misc.o \
	src/common/argproc.o \
	src/common/clientserver.o \
	src/common/collision.o \
	src/common/crc.o \
	src/common/cmdparser.o \
	src/common/cvar.o \
	src/common/filesystem.o \
	src/common/glob.o \
	src/common/md4.o \
	src/common/frame.o \
	src/common/movemsg.o \
	src/common/netchan.o \
	src/common/pmove.o \
	src/common/szone.o \
	src/common/zone.o \
	src/common/shared/rand.o \
	src/common/shared/shared.o \
	src/common/unzip/ioapi.o \
	src/common/unzip/miniz.o \
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

ifeq ($(YQ2_OSTYPE), Windows)
SERVER_OBJS_ += \
	src/backends/windows/main.o \
	src/backends/windows/network.o \
	src/backends/windows/system.o \
	src/backends/windows/shared/hunk.o
else # not Windows
SERVER_OBJS_ += \
	src/backends/unix/main.o \
	src/backends/unix/network.o \
	src/backends/unix/signalhandler.o \
	src/backends/unix/system.o \
	src/backends/unix/shared/hunk.o
endif

# ----------

# Rewrite pathes to our object directory.
CLIENT_OBJS = $(patsubst %,build/client/%,$(CLIENT_OBJS_))
REFGL1_OBJS = $(patsubst %,build/ref_gl1/%,$(REFGL1_OBJS_))
REFGL3_OBJS = $(patsubst %,build/ref_gl3/%,$(REFGL3_OBJS_))
REFSOFT_OBJS = $(patsubst %,build/ref_soft/%,$(REFSOFT_OBJS_))
REFVK_OBJS = $(patsubst %,build/ref_vk/%,$(REFVK_OBJS_))
SERVER_OBJS = $(patsubst %,build/server/%,$(SERVER_OBJS_))
GAME_OBJS = $(patsubst %,build/baseq2/%,$(GAME_OBJS_))

# ----------

# Generate header dependencies.
CLIENT_DEPS= $(CLIENT_OBJS:.o=.d)
GAME_DEPS= $(GAME_OBJS:.o=.d)
REFGL1_DEPS= $(REFGL1_OBJS:.o=.d)
REFGL3_DEPS= $(REFGL3_OBJS:.o=.d)
REFSOFT_DEPS= $(REFSOFT_OBJS:.o=.d)
REFVK_DEPS= $(REFVK_OBJS:.o=.d)
SERVER_DEPS= $(SERVER_OBJS:.o=.d)

# Suck header dependencies in.
-include $(CLIENT_DEPS)
-include $(GAME_DEPS)
-include $(REFGL1_DEPS)
-include $(REFGL3_DEPS)
-include $(REFVK_DEPS)
-include $(SERVER_DEPS)

# ----------

# release/quake2
ifeq ($(YQ2_OSTYPE), Windows)
release/yquake2.exe : $(CLIENT_OBJS) icon
	@echo "===> LD $@"
	${Q}$(CC) build/icon/icon.res $(CLIENT_OBJS) $(LDFLAGS) $(SDLLDFLAGS) -o $@
	$(Q)strip $@
release/quake2.exe : src/win-wrapper/wrapper.c icon
	$(Q)$(CC) -Wall -mwindows build/icon/icon.res src/win-wrapper/wrapper.c -o $@
	$(Q)strip $@
else
release/quake2 : $(CLIENT_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(CLIENT_OBJS) $(LDFLAGS) $(SDLLDFLAGS) -o $@
endif

# release/q2ded
ifeq ($(YQ2_OSTYPE), Windows)
release/q2ded.exe : $(SERVER_OBJS) icon
	@echo "===> LD $@"
	${Q}$(CC) build/icon/icon.res $(SERVER_OBJS) $(LDFLAGS) $(SDLLDFLAGS) -o $@
	$(Q)strip $@
else
release/q2ded : $(SERVER_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(SERVER_OBJS) $(LDFLAGS) -o $@
endif

# release/ref_gl1.so
ifeq ($(YQ2_OSTYPE), Windows)
release/ref_gl1.dll : $(REFGL1_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(REFGL1_OBJS) $(LDFLAGS) $(DLL_SDLLDFLAGS) -o $@
	$(Q)strip $@
else ifeq ($(YQ2_OSTYPE), Darwin)
release/ref_gl1.dylib : $(REFGL1_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(REFGL1_OBJS) $(LDFLAGS) $(SDLLDFLAGS) -o $@
else
release/ref_gl1.so : $(REFGL1_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(REFGL1_OBJS) $(LDFLAGS) $(SDLLDFLAGS) -o $@
endif

# release/ref_gl3.so
ifeq ($(YQ2_OSTYPE), Windows)
release/ref_gl3.dll : $(REFGL3_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(REFGL3_OBJS) $(LDFLAGS) $(DLL_SDLLDFLAGS) -o $@
	$(Q)strip $@
else ifeq ($(YQ2_OSTYPE), Darwin)
release/ref_gl3.dylib : $(REFGL3_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(REFGL3_OBJS) $(LDFLAGS) $(SDLLDFLAGS) -o $@
else
release/ref_gl3.so : $(REFGL3_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(REFGL3_OBJS) $(LDFLAGS) $(SDLLDFLAGS) -o $@
endif

# release/ref_soft.so
ifeq ($(YQ2_OSTYPE), Windows)
release/ref_soft.dll : $(REFSOFT_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(REFSOFT_OBJS) $(LDFLAGS) $(DLL_SDLLDFLAGS) -o $@
	$(Q)strip $@
else ifeq ($(YQ2_OSTYPE), Darwin)
release/ref_soft.dylib : $(REFSOFT_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(REFSOFT_OBJS) $(LDFLAGS) $(SDLLDFLAGS) -o $@
else
release/ref_soft.so : $(REFSOFT_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(REFSOFT_OBJS) $(LDFLAGS) $(SDLLDFLAGS) -o $@
endif

# release/ref_vk.so
ifeq ($(YQ2_OSTYPE), Windows)
release/ref_vk.dll : $(REFVK_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(REFVK_OBJS) $(LDFLAGS) $(SDLLDFLAGS) -o $@
else
release/ref_vk.so : $(REFVK_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(REFVK_OBJS) $(LDFLAGS) $(SDLLDFLAGS) -o $@
endif

# release/baseq2/game.so
ifeq ($(YQ2_OSTYPE), Windows)
release/baseq2/game.dll : $(GAME_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(GAME_OBJS) $(LDFLAGS) -o $@
	$(Q)strip $@
else ifeq ($(YQ2_OSTYPE), Darwin)
release/baseq2/game.dylib : $(GAME_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(GAME_OBJS) $(LDFLAGS) -o $@
else
release/baseq2/game.so : $(GAME_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(GAME_OBJS) $(LDFLAGS) -o $@
endif

# ----------
