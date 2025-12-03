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
#  - SDL 2 or SDL 3                                      #
#  - libGL                                               #
#  - Vulkan headers                                      #
#                                                        #
# Optional dependencies:                                 #
#  - CURL                                                #
#  - OpenAL                                              #
#                                                        #
# Platforms:                                             #
#  - FreeBSD                                             #
#  - Linux                                               #
#  - NetBSD                                              #
#  - OpenBSD                                             #
#  - OS X                                                #
#  - Windows (MinGW)                                     #
# ------------------------------------------------------ #

# Variables
# ---------
# ASAN
#   Builds with address sanitizer, includes DEBUG.
# DEBUG
#   Builds a debug build, forces -O0 and adds debug symbols.
# MINGW_CHOST
#   If you use mingw this can specify architecture.
#   Available values:
#   x86_64-w64-mingw32 -> indicates x86_64
#   i686-w64-mingw32   -> indicates i386
# QUIET
#   If defined, "===> CC ..." lines are silenced.
# SOURCE_DATE_EPOCH
#   For reproduceable builds, look here for details:
#   https://reproducible-builds.org/specs/source-date-epoch/
#   If set, adds a BUILD_DATE define to CFLAGS.
# UBSAN
#   Builds with undefined behavior sanitizer.includes DEBUG.
# VERBOSE
#   Prints full compile, linker and misc commands.
# WERR
#   Treat compiler warnings as errors.
#   If defined, -Werror is added to compiler flags.
# ----------

# User configurable options
# -------------------------

# WITH_CURL
# Enables HTTP support through cURL. Used for
# HTTP download.
WITH_CURL:=yes

# WITH_EXECINFO
# Enables backtraces through execinfo on supported platforms.
# Usefull to get some basic debug information when the game
# crashes.
WITH_EXECINFO:=yes

# WITH_OPENAL
# Enables the optional OpenAL sound system.
# To use it your system needs libopenal.so.1
# or openal32.dll (we recommend openal-soft)
# installed
WITH_OPENAL:=yes

# WITH_RPATH
# Sets an RPATH to $ORIGIN/lib. It can be used to
# inject custom libraries, e.g. a patches libSDL.so
# or libopenal.so. Not supported on Windows.
WITH_RPATH:=yes

# WITH_SDL3
# When disabled SDL 2 is used instead of SDL 3.
WITH_SDL3:=yes

# WITH_SYSTEMWIDE
# Enable systemwide installation of game assets.
WITH_SYSTEMWIDE:=no

# WITH_SYSTEMDIR
# This will set the default SYSTEMDIR, a non-empty string
# would actually be used. On Windows normals slashes (/)
# instead of backslashed (\) should be used! The string
# MUST NOT be surrounded by quotation marks!
WITH_SYSTEMDIR:=""

# OSX_APP
# This will set the build options to create an MacOS .app-bundle.
# The app-bundle itself will not be created, but the runtime paths
# will be set to expect the game-data in *.app/
# Contents/Resources
OSX_APP:=yes

# CONFIG_FILE
# This is an optional configuration file, it'll be used in
# case of presence.
CONFIG_FILE:=config.mk

# PKG_CONFIG
# Specify program that configures SDL.
# Needs to be overridable for cross-compilation.
PKG_CONFIG ?= pkgconf

# ----------

# In case of a configuration file being present, we'll just use it
ifeq ($(wildcard $(CONFIG_FILE)), $(CONFIG_FILE))
include $(CONFIG_FILE)
endif

# Normalize QUIET value to either "x" or ""
ifdef QUIET
	override QUIET := "x"
else
	override QUIET := ""
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
ifdef MINGW_CHOST
ifeq ($(MINGW_CHOST), x86_64-w64-mingw32)
YQ2_ARCH ?= x86_64
else # i686-w64-mingw32
YQ2_ARCH ?= i386
endif
else # windows, but MINGW_CHOST not defined
ifdef PROCESSOR_ARCHITEW6432
# 64 bit Windows
YQ2_ARCH ?= $(PROCESSOR_ARCHITEW6432)
else
# 32 bit Windows
YQ2_ARCH ?= $(PROCESSOR_ARCHITECTURE)
endif
endif # windows but MINGW_CHOST not defined
else
ifneq ($(YQ2_OSTYPE), Darwin)
# Normalize some abiguous YQ2_ARCH strings
YQ2_ARCH ?= $(shell uname -m | sed -e 's/i.86/i386/' -e 's/amd64/x86_64/' -e 's/arm64/aarch64/' -e 's/^arm.*/arm/')
else
YQ2_ARCH ?= $(shell uname -m)
endif
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

# UBSAN includes DEBUG
ifdef UBSAN
DEBUG=1
endif

# ----------

# Set up build and bin output directories

# Root dir names
override BINROOT :=
override BUILDROOT := build

ifdef DEBUG
	override BINDIR := $(BINROOT)debug
	override BUILDDIR := $(BUILDROOT)/debug
else
	override BINDIR := $(BINROOT)release
	override BUILDDIR := $(BUILDROOT)/release
endif

# ----------

# Base CFLAGS. These may be overridden by the environment.
# Highest supported optimizations are -O2, higher levels
# will likely break this crappy code.
ifdef DEBUG
CFLAGS ?= -O0 -g -Wall -pipe -DDEBUG
ifdef ASAN
override CFLAGS += -fsanitize=address -DUSE_SANITIZER
endif
ifdef UBSAN
override CFLAGS += -fsanitize=undefined -DUSE_SANITIZER
endif
else
CFLAGS ?= -O2 -Wall -pipe -fomit-frame-pointer
endif

# Treat warnings as errors
ifdef WERR
override CFLAGS += -Werror
endif

# Always needed are:
#  -fno-strict-aliasing since the source doesn't comply
#   with strict aliasing rules and it's next to impossible
#   to get it there...
#  -fwrapv for defined integer wrapping. MSVC6 did this
#   and the game code requires it.
#  -fvisibility=hidden to keep symbols hidden. This is
#   mostly best practice and not really necessary.
override CFLAGS += -fno-strict-aliasing -fwrapv -fvisibility=hidden

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
	# -fno-common avoids linker warnings due
	#  uninitialized global variables treated as "common".
	#  On most platforms clang sets no-commom by default,
	#  MacOS is a prominent exception.
	override CFLAGS += -Wno-missing-braces -fno-common
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

ifdef SOURCE_DATE_EPOCH
override CFLAGS += -DBUILD_DATE=\"$(shell date --utc --date="@${SOURCE_DATE_EPOCH}" +"%b %_d %Y" | sed -e 's/ /\\ /g')\"
endif

# ----------

# Using the default x87 float math on 32bit x86 causes rounding trouble
# -ffloat-store could work around that, but the better solution is to
# just enforce SSE - every x86 CPU since Pentium3 supports that
# and this should even improve the performance on old CPUs
ifeq ($(YQ2_ARCH), i386)
override CFLAGS += -msse -mfpmath=sse
endif

# Force SSE math on x86_64. All sane compilers should do this
# anyway, just to protect us from broken Linux distros.
ifeq ($(YQ2_ARCH), x86_64)
override CFLAGS += -mfpmath=sse
endif

# Disable floating-point expression contraction. While this shouldn't be
# a problem for C (only for C++) better be safe than sorry. See
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=100839 for details.
ifeq ($(COMPILER), gcc)
override CFLAGS += -ffp-contract=off
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
ifeq ($(WITH_SDL3),yes)
SDLCFLAGS := $(shell $(PKG_CONFIG) --cflags sdl3)
SDLCFLAGS += -DUSE_SDL3
else
SDLCFLAGS := $(shell sdl2-config --cflags)
endif

ifdef NO_SDL_GYRO
SDLCFLAGS += -DNO_SDL_GYRO
endif

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
else ifeq ($(YQ2_OSTYPE),Darwin)
INCLUDE ?= -I/usr/local/include -I/opt/homebrew/include
endif

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
else ifeq ($(YQ2_OSTYPE),Darwin)
LDFLAGS ?= -L/usr/local/lib -L/opt/homebrew/lib
endif

# Link address sanitizer if requested.
ifdef ASAN
override LDFLAGS += -fsanitize=address
endif

# Link undefined behavior sanitizer if requested.
ifdef UBSAN
override LDFLAGS += -fsanitize=undefined
endif

# Required libraries.
ifeq ($(YQ2_OSTYPE),Linux)
LDLIBS ?= -lm -ldl -rdynamic
else ifeq ($(YQ2_OSTYPE),FreeBSD)
LDLIBS ?= -lm
else ifeq ($(YQ2_OSTYPE),NetBSD)
LDLIBS ?= -lm
else ifeq ($(YQ2_OSTYPE),OpenBSD)
LDLIBS ?= -lm
else ifeq ($(YQ2_OSTYPE),Windows)
LDLIBS ?= -lws2_32 -lwinmm -static-libgcc
else ifeq ($(YQ2_OSTYPE), Darwin)
LDLIBS ?= -arch $(YQ2_ARCH)
else ifeq ($(YQ2_OSTYPE), Haiku)
LDLIBS ?= -lm -lnetwork
else ifeq ($(YQ2_OSTYPE), SunOS)
LDLIBS ?= -lm -lsocket -lnsl
endif

# ASAN and UBSAN must not be linked
# with --no-undefined. OSX and OpenBSD
# don't support it at all.
ifndef ASAN
ifndef UBSAN
ifneq ($(YQ2_OSTYPE), Darwin)
ifneq ($(YQ2_OSTYPE), OpenBSD)
override LDFLAGS += -Wl,--no-undefined
endif
endif
endif
endif

# ----------

# Extra LDFLAGS for SDL
ifeq ($(WITH_SDL3),yes)
ifeq ($(YQ2_OSTYPE), Darwin)
SDLLDFLAGS := -lSDL3
else
SDLLDFLAGS := $(shell $(PKG_CONFIG) --libs sdl3)
endif
else
ifeq ($(YQ2_OSTYPE), Darwin)
SDLLDFLAGS := -lSDL2
else
SDLLDFLAGS := $(shell sdl2-config --libs)
endif
endif

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
.PHONY : all client game icon server ref_gl1 ref_gl3 ref_gles1 ref_gles3 ref_soft

# ----------

# Builds everything but the GLES1 renderer
all: config client server game ref_gl1 ref_gl3 ref_gles3 ref_soft

# ----------

# Builds everything, including the GLES1 renderer
with_gles1: all ref_gles1

# ----------

# Print config values
config:
	@echo "Build configuration"
	@echo "============================"
	@echo "YQ2_ARCH = $(YQ2_ARCH) COMPILER = $(COMPILER)"
	@echo "WITH_CURL = $(WITH_CURL)"
	@echo "WITH_OPENAL = $(WITH_OPENAL)"
	@echo "WITH_RPATH = $(WITH_RPATH)"
	@echo "WITH_SDL3 = $(WITH_SDL3)"
	@echo "WITH_SYSTEMWIDE = $(WITH_SYSTEMWIDE)"
	@echo "WITH_SYSTEMDIR = $(WITH_SYSTEMDIR)"
	@echo "============================"
	@echo ""

# ----------

# Special target to compile the icon on Windows
ifeq ($(YQ2_OSTYPE), Windows)
icon:
	@echo "===> WR $(BUILDROOT)/icon/icon.res"
	${Q}mkdir -p $(BUILDROOT)/icon
	${Q}windres src/backends/windows/icon.rc -O COFF -o $(BUILDROOT)/icon/icon.res
endif

# ----------

# Cleanup
clean:
	@echo "===> CLEAN"
	${Q}rm -Rf build debug/* release/*

cleanall:
	@echo "===> CLEAN"
	${Q}rm -Rf build debug release

# ----------

# The client
ifeq ($(YQ2_OSTYPE), Windows)
client:
	@echo "===> Building yquake2.exe"
	${Q}mkdir -p $(BINDIR)
	$(MAKE) $(BINDIR)/yquake2.exe
	@echo "===> Building quake2.exe Wrapper"
	$(MAKE) $(BINDIR)/quake2.exe

$(BUILDDIR)/client/%.o: %.c
	@if [ -z $(QUIET) ]; then\
		echo "===> CC $<";\
	fi
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(ZIPCFLAGS) $(INCLUDE) -o $@ $<

$(BINDIR)/yquake2.exe : LDFLAGS += -mwindows

ifeq ($(WITH_CURL),yes)
$(BINDIR)/yquake2.exe : CFLAGS += -DUSE_CURL
endif

ifeq ($(WITH_OPENAL),yes)
$(BINDIR)/yquake2.exe : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"openal32.dll"'
endif

else # not Windows

client:
	@echo "===> Building quake2"
	${Q}mkdir -p $(BINDIR)
	$(MAKE) $(BINDIR)/quake2

ifeq ($(YQ2_OSTYPE), Darwin)
$(BUILDDIR)/client/%.o : %.c
	@if [ -z $(QUIET) ]; then\
		echo "===> CC $<";\
	fi
	${Q}mkdir -p $(@D)
	${Q}$(CC) -arch $(YQ2_ARCH) -x objective-c -c $(CFLAGS) $(SDLCFLAGS) $(ZIPCFLAGS) $(INCLUDE)  $< -o $@
else
$(BUILDDIR)/client/%.o: %.c
	@if [ -z $(QUIET) ]; then\
		echo "===> CC $<";\
	fi
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(ZIPCFLAGS) $(INCLUDE) -o $@ $<
endif

$(BINDIR)/quake2 : CFLAGS += -Wno-unused-result

ifeq ($(WITH_CURL),yes)
$(BINDIR)/quake2 : CFLAGS += -DUSE_CURL
endif

ifeq ($(WITH_OPENAL),yes)
ifeq ($(YQ2_OSTYPE), OpenBSD)
$(BINDIR)/quake2 : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"libopenal.so"'
else ifeq ($(YQ2_OSTYPE), Darwin)
OPENAL_PATH ?= $(shell brew --prefix openal-soft)
$(BINDIR)/quake2 : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"libopenal.dylib"' -I$(OPENAL_PATH)/include
$(BINDIR)/quake2 : LDFLAGS += -L$(OPENAL_PATH)/lib -rpath $(OPENAL_PATH)/lib
else
$(BINDIR)/quake2 : CFLAGS += -DUSE_OPENAL -DDEFAULT_OPENAL_DRIVER='"libopenal.so.1"'
endif
endif

ifeq ($(WITH_EXECINFO),yes)
ifeq ($(YQ2_OSTYPE), Linux)
$(BINDIR)/quake2 : CFLAGS += -DHAVE_EXECINFO
else ifeq ($(YQ2_OSTYPE), Darwin)
$(BINDIR)/quake2 : CFLAGS += -DHAVE_EXECINFO
else ifeq ($(YQ2_OSTYPE), SunOS)
$(BINDIR)/quake2 : CFLAGS += -DHAVE_EXECINFO
else ifeq ($(YQ2_OSTYPE), FreeBSD)
$(BINDIR)/quake2 : CFLAGS += -DHAVE_EXECINFO
$(BINDIR)/quake2 : LDLIBS += -lexecinfo
else ifeq ($(YQ2_OSTYPE), NetBSD)
$(BINDIR)/quake2 : CFLAGS += -DHAVE_EXECINFO
$(BINDIR)/quake2 : LDLIBS += -lexecinfo
else ifeq ($(YQ2_OSTYPE), OpenBSD)
$(BINDIR)/quake2 : CFLAGS += -DHAVE_EXECINFO
$(BINDIR)/quake2 : LDLIBS += -lexecinfo
else ifeq ($(YQ2_OSTYPE), Haiku)
$(BINDIR)/quake2 : CFLAGS += -DHAVE_EXECINFO
$(BINDIR)/quake2 : LDLIBS += -lexecinfo
endif
endif

ifeq ($(WITH_RPATH),yes)
ifeq ($(YQ2_OSTYPE), Darwin)
$(BINDIR)/quake2 : LDFLAGS += -Wl,-rpath,'@executable_path/lib'
else
$(BINDIR)/quake2 : LDFLAGS += -Wl,-z,origin,-rpath='$$ORIGIN/lib'
endif
endif
endif

# ----------

# The server
ifeq ($(YQ2_OSTYPE), Windows)
server:
	@echo "===> Building q2ded"
	${Q}mkdir -p $(BINDIR)
	$(MAKE) $(BINDIR)/q2ded.exe

$(BUILDDIR)/server/%.o: %.c
	@if [ -z $(QUIET) ]; then\
		echo "===> CC $<";\
	fi
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(ZIPCFLAGS) $(INCLUDE) -o $@ $<

$(BINDIR)/q2ded.exe : CFLAGS += -DDEDICATED_ONLY

else # not Windows

server:
	@echo "===> Building q2ded"
	${Q}mkdir -p $(BINDIR)
	$(MAKE) $(BINDIR)/q2ded

$(BUILDDIR)/server/%.o: %.c
	@if [ -z $(QUIET) ]; then\
		echo "===> CC $<";\
	fi
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(ZIPCFLAGS) $(INCLUDE) -o $@ $<

$(BINDIR)/q2ded : CFLAGS += -DDEDICATED_ONLY -Wno-unused-result

ifeq ($(YQ2_OSTYPE), FreeBSD)
$(BINDIR)/q2ded : LDLIBS += -lexecinfo
endif
endif

# ----------

# The OpenGL 1.x renderer lib

ifeq ($(YQ2_OSTYPE), Windows)

ref_gl1:
	@echo "===> Building ref_gl1.dll"
	$(MAKE) $(BINDIR)/ref_gl1.dll

$(BINDIR)/ref_gl1.dll : LDFLAGS += -shared
$(BINDIR)/ref_gl1.dll : LDLIBS += -lopengl32

else ifeq ($(YQ2_OSTYPE), Darwin)

ref_gl1:
	@echo "===> Building ref_gl1.dylib"
	$(MAKE) $(BINDIR)/ref_gl1.dylib


$(BINDIR)/ref_gl1.dylib : LDFLAGS += -shared -framework OpenGL

else # not Windows or Darwin

ref_gl1:
	@echo "===> Building ref_gl1.so"
	$(MAKE) $(BINDIR)/ref_gl1.so


$(BINDIR)/ref_gl1.so : CFLAGS += -fPIC
$(BINDIR)/ref_gl1.so : LDFLAGS += -shared
$(BINDIR)/ref_gl1.so : LDLIBS += -lGL

endif # OS specific ref_gl1 stuff

$(BUILDDIR)/ref_gl1/%.o: %.c
	@if [ -z $(QUIET) ]; then\
		echo "===> CC $<";\
	fi
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(INCLUDE) -o $@ $<

# ----------

# The OpenGL ES 1.0 renderer lib

ifeq ($(YQ2_OSTYPE), Windows)

ref_gles1:
	@echo "===> Building ref_gles1.dll"
	$(MAKE) $(BINDIR)/ref_gles1.dll

$(BINDIR)/ref_gles1.dll : GLAD_INCLUDE = -Isrc/client/refresh/gl1/glad-gles1/include
$(BINDIR)/ref_gles1.dll : CFLAGS += -DYQ2_GL1_GLES
$(BINDIR)/ref_gles1.dll : LDFLAGS += -shared

else ifeq ($(YQ2_OSTYPE), Darwin)

ref_gles1:
	@echo "===> Building ref_gles1.dylib"
	$(MAKE) $(BINDIR)/ref_gles1.dylib

$(BINDIR)/ref_gles1.dylib : GLAD_INCLUDE = -Isrc/client/refresh/gl1/glad-gles1/include
$(BINDIR)/ref_gles1.dylib : CFLAGS += -DYQ2_GL1_GLES
$(BINDIR)/ref_gles1.dylib : LDFLAGS += -shared

else # not Windows or Darwin

ref_gles1:
	@echo "===> Building ref_gles1.so"
	$(MAKE) $(BINDIR)/ref_gles1.so

$(BINDIR)/ref_gles1.so : GLAD_INCLUDE = -Isrc/client/refresh/gl1/glad-gles1/include
$(BINDIR)/ref_gles1.so : CFLAGS += -DYQ2_GL1_GLES -fPIC
$(BINDIR)/ref_gles1.so : LDFLAGS += -shared

endif # OS specific ref_gles1 stuff

$(BUILDDIR)/ref_gles1/%.o: %.c
	@if [ -z $(QUIET) ]; then\
		echo "===> CC $<";\
	fi
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(INCLUDE) $(GLAD_INCLUDE) -o $@ $<

# ----------

# The OpenGL 3.x renderer lib

ifeq ($(YQ2_OSTYPE), Windows)

ref_gl3:
	@echo "===> Building ref_gl3.dll"
	$(MAKE) $(BINDIR)/ref_gl3.dll

$(BINDIR)/ref_gl3.dll : GLAD_INCLUDE = -Isrc/client/refresh/gl3/glad/include
$(BINDIR)/ref_gl3.dll : LDFLAGS += -shared

else ifeq ($(YQ2_OSTYPE), Darwin)

ref_gl3:
	@echo "===> Building ref_gl3.dylib"
	$(MAKE) $(BINDIR)/ref_gl3.dylib

$(BINDIR)/ref_gl3.dylib : GLAD_INCLUDE = -Isrc/client/refresh/gl3/glad/include
$(BINDIR)/ref_gl3.dylib : LDFLAGS += -shared

else # not Windows or Darwin

ref_gl3:
	@echo "===> Building ref_gl3.so"
	$(MAKE) $(BINDIR)/ref_gl3.so

$(BINDIR)/ref_gl3.so : GLAD_INCLUDE = -Isrc/client/refresh/gl3/glad/include
$(BINDIR)/ref_gl3.so : CFLAGS += -fPIC
$(BINDIR)/ref_gl3.so : LDFLAGS += -shared

endif # OS specific ref_gl3 stuff

$(BUILDDIR)/ref_gl3/%.o: %.c
	@if [ -z $(QUIET) ]; then\
		echo "===> CC $<";\
	fi
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(INCLUDE) $(GLAD_INCLUDE) -o $@ $<

# ----------

# The OpenGL ES 3.0 renderer lib

ifeq ($(YQ2_OSTYPE), Windows)

ref_gles3:
	@echo "===> Building ref_gles3.dll"
	$(MAKE) $(BINDIR)/ref_gles3.dll

$(BINDIR)/ref_gles3.dll : GLAD_INCLUDE = -Isrc/client/refresh/gl3/glad-gles3/include

# YQ2_GL3_GLES3 is for GLES3, DYQ2_GL3_GLES is for things that are identical
# in both GLES3 and GLES2 (in case we ever support that)
$(BINDIR)/ref_gles3.dll : CFLAGS += -DYQ2_GL3_GLES3 -DYQ2_GL3_GLES

$(BINDIR)/ref_gles3.dll : LDFLAGS += -shared

else ifeq ($(YQ2_OSTYPE), Darwin)

ref_gles3:
	@echo "===> Building ref_gles3.dylib"
	$(MAKE) $(BINDIR)/ref_gles3.dylib

$(BINDIR)/ref_gles3.dylib : GLAD_INCLUDE = -Isrc/client/refresh/gl3/glad-gles3/include

# YQ2_GL3_GLES3 is for GLES3, DYQ2_GL3_GLES is for things that are identical
# in both GLES3 and GLES2 (in case we ever support that)
$(BINDIR)/ref_gles3.dylib : CFLAGS += -DYQ2_GL3_GLES3 -DYQ2_GL3_GLES

$(BINDIR)/ref_gles3.dylib : LDFLAGS += -shared

else # not Windows or Darwin

ref_gles3:
	@echo "===> Building ref_gles3.so"
	$(MAKE) $(BINDIR)/ref_gles3.so

$(BINDIR)/ref_gles3.so : GLAD_INCLUDE = -Isrc/client/refresh/gl3/glad-gles3/include

# YQ2_GL3_GLES3 is for GLES3, DYQ2_GL3_GLES is for things that are identical
# in both GLES3 and GLES2 (in case we ever support that)
$(BINDIR)/ref_gles3.so : CFLAGS += -DYQ2_GL3_GLES3 -DYQ2_GL3_GLES -fPIC

$(BINDIR)/ref_gles3.so : LDFLAGS += -shared

GLAD_INCLUDE = -Isrc/client/refresh/gl3/glad-gles3/include

endif # OS specific ref_gl3 stuff

$(BUILDDIR)/ref_gles3/%.o: %.c
	@if [ -z $(QUIET) ]; then\
		echo "===> CC $<";\
	fi
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(INCLUDE) $(GLAD_INCLUDE) -o $@ $<

# ----------

# The soft renderer lib

ifeq ($(YQ2_OSTYPE), Windows)

ref_soft:
	@echo "===> Building ref_soft.dll"
	$(MAKE) $(BINDIR)/ref_soft.dll

$(BINDIR)/ref_soft.dll : LDFLAGS += -shared

else ifeq ($(YQ2_OSTYPE), Darwin)

ref_soft:
	@echo "===> Building ref_soft.dylib"
	$(MAKE) $(BINDIR)/ref_soft.dylib

$(BINDIR)/ref_soft.dylib : LDFLAGS += -shared

else # not Windows or Darwin

ref_soft:
	@echo "===> Building ref_soft.so"
	$(MAKE) $(BINDIR)/ref_soft.so

$(BINDIR)/ref_soft.so : CFLAGS += -fPIC
$(BINDIR)/ref_soft.so : LDFLAGS += -shared

endif # OS specific ref_soft stuff

$(BUILDDIR)/ref_soft/%.o: %.c
	@if [ -z $(QUIET) ]; then\
		echo "===> CC $<";\
	fi
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(INCLUDE) -o $@ $<

# ----------

# The baseq2 game
ifeq ($(YQ2_OSTYPE), Windows)
game:
	@echo "===> Building baseq2/game.dll"
	${Q}mkdir -p $(BINDIR)/baseq2
	$(MAKE) $(BINDIR)/baseq2/game.dll

$(BUILDDIR)/baseq2/%.o: %.c
	@if [ -z $(QUIET) ]; then\
		echo "===> CC $<";\
	fi
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

$(BINDIR)/baseq2/game.dll : LDFLAGS += -shared

else ifeq ($(YQ2_OSTYPE), Darwin)

game:
	@echo "===> Building baseq2/game.dylib"
	${Q}mkdir -p $(BINDIR)/baseq2
	$(MAKE) $(BINDIR)/baseq2/game.dylib

$(BUILDDIR)/baseq2/%.o: %.c
	@if [ -z $(QUIET) ]; then\
		echo "===> CC $<";\
	fi
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

$(BINDIR)/baseq2/game.dylib : CFLAGS += -fPIC
$(BINDIR)/baseq2/game.dylib : LDFLAGS += -shared

else # not Windows or Darwin

game:
	@echo "===> Building baseq2/game.so"
	${Q}mkdir -p $(BINDIR)/baseq2
	$(MAKE) $(BINDIR)/baseq2/game.so

$(BUILDDIR)/baseq2/%.o: %.c
	@if [ -z $(QUIET) ]; then\
		echo "===> CC $<";\
	fi
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

$(BINDIR)/baseq2/game.so : CFLAGS += -fPIC -Wno-unused-result
$(BINDIR)/baseq2/game.so : LDFLAGS += -shared
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
	src/client/cl_image.o \
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
	src/client/input/gyro.o \
	src/client/menu/menu.o \
	src/client/menu/qmenu.o \
	src/client/menu/videomenu.o \
	src/client/sound/ogg.o \
	src/client/sound/openal.o \
	src/client/sound/qal.o \
	src/client/sound/sdl.o \
	src/client/sound/sound.o \
	src/client/sound/wave.o \
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
	src/common/unzip/unzip.o \
	src/common/unzip/miniz/miniz.o \
	src/common/unzip/miniz/miniz_tdef.o \
	src/common/unzip/miniz/miniz_tinfl.o \
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

ifeq ($(WITH_SDL3),yes)
CLIENT_OBJS_ += \
	src/client/input/sdl3.o \
	src/client/vid/glimp_sdl3.o
else
CLIENT_OBJS_ += \
	src/client/input/sdl2.o \
	src/client/vid/glimp_sdl2.o
endif

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
	src/client/refresh/gl1/gl1_buffer.o \
	src/client/refresh/files/common.o \
	src/client/refresh/files/surf.o \
	src/client/refresh/files/models.o \
	src/client/refresh/files/pcx.o \
	src/client/refresh/files/stb.o \
	src/client/refresh/files/wal.o \
	src/client/refresh/files/pvs.o \
	src/common/shared/shared.o \
	src/common/md4.o

REFGL1_OBJS_GLADEES_ := \
	src/client/refresh/gl1/glad-gles1/src/glad.o

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
	src/client/refresh/files/common.o \
	src/client/refresh/files/surf.o \
	src/client/refresh/files/models.o \
	src/client/refresh/files/pcx.o \
	src/client/refresh/files/stb.o \
	src/client/refresh/files/wal.o \
	src/client/refresh/files/pvs.o \
	src/common/shared/shared.o \
	src/common/md4.o

REFGL3_OBJS_GLADE_ := \
	src/client/refresh/gl3/glad/src/glad.o

REFGL3_OBJS_GLADEES_ := \
	src/client/refresh/gl3/glad-gles3/src/glad.o

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
	src/client/refresh/files/surf.o \
	src/client/refresh/files/common.o \
	src/client/refresh/files/models.o \
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
	src/common/unzip/unzip.o \
	src/common/unzip/miniz/miniz.o \
	src/common/unzip/miniz/miniz_tdef.o \
	src/common/unzip/miniz/miniz_tinfl.o \
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

# Rewrite paths to our object directory.
CLIENT_OBJS = $(patsubst %,$(BUILDDIR)/client/%,$(CLIENT_OBJS_))
REFGL1_OBJS = $(patsubst %,$(BUILDDIR)/ref_gl1/%,$(REFGL1_OBJS_))
REFGLES1_OBJS = $(patsubst %,$(BUILDDIR)/ref_gles1/%,$(REFGL1_OBJS_))
REFGLES1_OBJS += $(patsubst %,$(BUILDDIR)/ref_gles1/%,$(REFGL1_OBJS_GLADEES_))
REFGL3_OBJS = $(patsubst %,$(BUILDDIR)/ref_gl3/%,$(REFGL3_OBJS_))
REFGL3_OBJS += $(patsubst %,$(BUILDDIR)/ref_gl3/%,$(REFGL3_OBJS_GLADE_))
REFGLES3_OBJS = $(patsubst %,$(BUILDDIR)/ref_gles3/%,$(REFGL3_OBJS_))
REFGLES3_OBJS += $(patsubst %,$(BUILDDIR)/ref_gles3/%,$(REFGL3_OBJS_GLADEES_))
REFSOFT_OBJS = $(patsubst %,$(BUILDDIR)/ref_soft/%,$(REFSOFT_OBJS_))
SERVER_OBJS = $(patsubst %,$(BUILDDIR)/server/%,$(SERVER_OBJS_))
GAME_OBJS = $(patsubst %,$(BUILDDIR)/baseq2/%,$(GAME_OBJS_))

# ----------

# Generate header dependencies.
CLIENT_DEPS= $(CLIENT_OBJS:.o=.d)
GAME_DEPS= $(GAME_OBJS:.o=.d)
REFGL1_DEPS= $(REFGL1_OBJS:.o=.d)
REFGLES1_DEPS= $(REFGLES1_OBJS:.o=.d)
REFGL3_DEPS= $(REFGL3_OBJS:.o=.d)
REFGLES3_DEPS= $(REFGLES3_OBJS:.o=.d)
REFSOFT_DEPS= $(REFSOFT_OBJS:.o=.d)
SERVER_DEPS= $(SERVER_OBJS:.o=.d)

# Suck header dependencies in.
-include $(CLIENT_DEPS)
-include $(GAME_DEPS)
-include $(REFGL1_DEPS)
-include $(REFGLES1_DEPS)
-include $(REFGL3_DEPS)
-include $(REFGLES3_DEPS)
-include $(SERVER_DEPS)

# ----------

# quake2
ifeq ($(YQ2_OSTYPE), Windows)
$(BINDIR)/yquake2.exe : $(CLIENT_OBJS) icon
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(BUILDROOT)/icon/icon.res $(CLIENT_OBJS) $(LDLIBS) $(SDLLDFLAGS) -o $@
	$(Q)strip $@
$(BINDIR)/quake2.exe : src/win-wrapper/wrapper.c icon
	$(Q)$(CC) -Wall -mwindows $(BUILDROOT)/icon/icon.res src/win-wrapper/wrapper.c -o $@
	$(Q)strip $@
else
$(BINDIR)/quake2 : $(CLIENT_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(CLIENT_OBJS) $(LDLIBS) $(SDLLDFLAGS) -o $@
endif

# q2ded
ifeq ($(YQ2_OSTYPE), Windows)
$(BINDIR)/q2ded.exe : $(SERVER_OBJS) icon
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(BUILDROOT)/icon/icon.res $(SERVER_OBJS) $(LDLIBS) -o $@
	$(Q)strip $@
else
$(BINDIR)/q2ded : $(SERVER_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(SERVER_OBJS) $(LDLIBS) -o $@
endif

# ref_gl1.so
ifeq ($(YQ2_OSTYPE), Windows)
$(BINDIR)/ref_gl1.dll : $(REFGL1_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGL1_OBJS) $(LDLIBS) $(DLL_SDLLDFLAGS) -o $@
	$(Q)strip $@
else ifeq ($(YQ2_OSTYPE), Darwin)
$(BINDIR)/ref_gl1.dylib : $(REFGL1_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGL1_OBJS) $(LDLIBS) $(SDLLDFLAGS) -o $@
else
$(BINDIR)/ref_gl1.so : $(REFGL1_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGL1_OBJS) $(LDLIBS) $(SDLLDFLAGS) -o $@
endif

# ref_gles1.so
ifeq ($(YQ2_OSTYPE), Windows)
$(BINDIR)/ref_gles1.dll : $(REFGLES1_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGLES1_OBJS) $(LDLIBS) $(DLL_SDLLDFLAGS) -o $@
	$(Q)strip $@
else ifeq ($(YQ2_OSTYPE), Darwin)
$(BINDIR)/ref_gles1.dylib : $(REFGLES1_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGLES1_OBJS) $(LDLIBS) $(SDLLDFLAGS) -o $@
else
$(BINDIR)/ref_gles1.so : $(REFGLES1_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGLES1_OBJS) $(LDLIBS) $(SDLLDFLAGS) -o $@
endif

# ref_gl3.so
ifeq ($(YQ2_OSTYPE), Windows)
$(BINDIR)/ref_gl3.dll : $(REFGL3_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGL3_OBJS) $(LDLIBS) $(DLL_SDLLDFLAGS) -o $@
	$(Q)strip $@
else ifeq ($(YQ2_OSTYPE), Darwin)
$(BINDIR)/ref_gl3.dylib : $(REFGL3_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGL3_OBJS) $(LDLIBS) $(SDLLDFLAGS) -o $@
else
$(BINDIR)/ref_gl3.so : $(REFGL3_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGL3_OBJS) $(LDLIBS) $(SDLLDFLAGS) -o $@
endif

# ref_gles3.so
ifeq ($(YQ2_OSTYPE), Windows)
$(BINDIR)/ref_gles3.dll : $(REFGLES3_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGLES3_OBJS) $(LDLIBS) $(DLL_SDLLDFLAGS) -o $@
	$(Q)strip $@
else ifeq ($(YQ2_OSTYPE), Darwin)
$(BINDIR)/ref_gles3.dylib : $(REFGLES3_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGLES3_OBJS) $(LDLIBS) $(SDLLDFLAGS) -o $@
else
$(BINDIR)/ref_gles3.so : $(REFGLES3_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGLES3_OBJS) $(LDLIBS) $(SDLLDFLAGS) -o $@
endif

# ref_soft.so
ifeq ($(YQ2_OSTYPE), Windows)
$(BINDIR)/ref_soft.dll : $(REFSOFT_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFSOFT_OBJS) $(LDLIBS) $(DLL_SDLLDFLAGS) -o $@
	$(Q)strip $@
else ifeq ($(YQ2_OSTYPE), Darwin)
$(BINDIR)/ref_soft.dylib : $(REFSOFT_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFSOFT_OBJS) $(LDLIBS) $(SDLLDFLAGS) -o $@
else
$(BINDIR)/ref_soft.so : $(REFSOFT_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFSOFT_OBJS) $(LDLIBS) $(SDLLDFLAGS) -o $@
endif

# baseq2/game.so
ifeq ($(YQ2_OSTYPE), Windows)
$(BINDIR)/baseq2/game.dll : $(GAME_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(GAME_OBJS) $(LDLIBS) -o $@
	$(Q)strip $@
else ifeq ($(YQ2_OSTYPE), Darwin)
$(BINDIR)/baseq2/game.dylib : $(GAME_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(GAME_OBJS) $(LDLIBS) -o $@
else
$(BINDIR)/baseq2/game.so : $(GAME_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(GAME_OBJS) $(LDLIBS) -o $@
endif

# ----------
