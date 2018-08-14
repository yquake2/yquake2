# Yamagi Quake II

This is the Yamagi Quake II Client, an enhanced version of id Software's Quake
II with focus on offline and coop gameplay. Both the gameplay and the graphics
are unchanged, but many bugs if the last official release were fixed and some
nice to have features like widescreen support and a modern OpenGL 3.2 renderer
were added. Unlike most other Quake II source ports Yamagi Quake II is fully 64
bit clean. It works perfectly on modern processors and operating systems. Yamagi
Quake II runs on nearly all common platforms; including FreeBSD, Linux, OpenBSD,
Windows and OS X (experimental).

This code is build upon Icculus Quake II, which itself is based on Quake II
3.21. Yamagi Quake II is released under the terms of the GPL version 2. See the
LICENSE file for further information.


## Installation

Yamagi Quake II is installed in 3 steps:

- Game data setup.
- Music extraction.
- Download and extract the executables.


### Game data Setup

Over the years Quake II was distributed in a myriad of ways:

- As a demo version.
- As a retail release on CD.
- As part of Quake IV.
- Through steam.
- On GOG.com
- etc.

Yamagi Quake II supports all of these version, even the rather limited demo.
Full versions require about 1.6 GiB hard drive space, the demo version about 150
Mib.


#### Full versions

The easiest way to install a full version of Quake II is to start with the
patch. Please note that the patch is **required** for all full versions of the
game, even the newer ones like Steam. Without it Yamagi Quake II will not work!

1. Download the patch:
   https://deponie.yamagi.org/quake2/idstuff/q2-3.20-x86-full-ctf.exe
2. Extract the patch into an empty directory. The patch is just an ordinary
   self-extracting ZIP file. On Windows it can be extracted by double clicking
   on it, on other systems an archiver or even the *unzip* command can be used.

Now it's time to remove the following files from the extracted patch. They're
the original executables, documentation and so on. They aren't needed anymore:

- *3.20_Changes.txt*
- *quake2.exe*
- *ref_gl.dll*
- *ref_soft.dll*
- *baseq2/gamex86.dll*
- *ctf/ctf2.ico*
- *ctf/gamex86.dll*
- *ctf/readme.txt*
- *ctf/server.cfg*
- *xatrix/gamex86.dll*
- *rogue/gamex86.dll*

Copy the *pak0.pak* file and the *video/* subdirectory from your Quake II
distribution (CD, Steam or GOG download, etc) into the *baseq2/* subdirectory of
the extracted patch.

If you have the optional addons you'll need to copy their gamedata too:

- For The Reckoning (also know as "xatrix") copy the *pak0.pak* and the *video/*
  subdirectory from your addon distribution into the *xatrix/* subdirectory.
- For Ground Zero (also known as "rogue") copy the *pak0.pak* and the *video/*
  subdirectory from your addon distribution into the *rogue/* subdirectory.


#### The demo version

1. Get the demo from https://deponie.yamagi.org/quake2/idstuff/q2-314-demo-x86.exe
   and extract it. It's just an ordinary, self-extract ZIP file. On Windows it
   can be extracted by double clicking on it, on other system an archiver or
   even the *unzip* command can be used.
3. Create a new directory and a subdirectory *baseq2/* in it.
3. Copy the *pak0.pak* and the *players/* subdirectory from *Install/Data/baseq2/*
   into the newly created *baseq2/* subdirectory.

The demo **must not** be patched! Patching the demo will break it!


### Music extraction

The retail CD version of Quake II and both addons contain up to 11 Audio CD
tracks, forming the soundtrack. Since modern computers lack the ability for
classic Audio CD playback, it's emulated by a transparent combination of CD
ripping and playback. The tracks can be ripped into OGG/Vorbis files. Yamagi
Quake II will use these files instead of the CD tracks.

Later Quake II version, for example the one included with Quake IV and the one
available through Steam, lack the soundtrack. Nevertheless Yamagi Quake II can
play it if you copy the files into the directories mentioned above. 

The GOG.com version is special, it includes the soundtrack as OGG/Vorbis files,
but in a non standard format. This is supported by Yamagi Quake II, see below for
details.


#### Using a generic CD extractor / CD ripper

1. Install a CD extractor (for example CDex) and set it to OGG/Vorbis files.
2. Put the Quake II CD into your CD drive and extract the files.
3. The files must be named after the CD track: Track 02 becomes *02.ogg*, track
   03 becomes *03.ogg* and so on. On both the Quake II and the Addon CDs track 01
   is the data track and thus can't be ripped.
4. Put these files into the corresponding subdirectory:
    - baseq2/music for Quake II.
    - xatrix/music for The Reckoning.
    - rogue/music for Ground Zero.


#### Extracting music on BSD, Linux and OS X 

An easy way to extract the music on unixoid platforms is to use
*stuff/cdripper.sh*, a simple shellscript we provide. It needs *cdparanoia* and
*oggenc* from the *vorbis-tools* package installed. Use your package manager of
choice (apt-get, dnf, homebrew, pkg, ...) to install them. Just execute the script
and copy the resulting *music/* directory to:
  - *baseq2/* for Quake II.
  - *xatrix/* for The Reckoning.
  - *rogue/* for Ground Zero.


#### The GOG.com version

The Quake II distributed by GOG.com contains the soundtrack, it just needs to
be copied into the game data directory. The target dir is just *music/*, next to
*baseq2/*. **Not** inside *baseq2/*.


### Download and extract the executables

How the Yamagi Quake II executables are installed depends on the platform:

- For Windows a package with all Yamagi Quake II executables is provided.
  There're two executables in the package. yquake.exe is the Yamagi Quake II
  client and should be preferred. quake2.exe is a simple wrapper, it's only
  provided to stay compatible with existing setups.
- Some Linux distributions and BSD systems provide Yamagi Quake II packages.
- On OS X you need to compile from source.
- Of course Yamagi Quake II can be compiled from source on all platforms.


#### Microsoft Windows

1. Get the latest release from https://www.yamagi.org/quake2
2. Extract it into the gamedata directory created above. *quake2.exe* must be
   placed next to the *baseq2/* subdirectory.

On Windows Yamagi Quake II is fully portable, you can move the installation
directory wherever and whenever you like. To update Yamagi Quake II just
overwrite the old files with the new ones.


#### Binary Package from your Linux distribution or BSD system

Many Linux distributions and BSD systems provide Yamagi Quake II packages.
Please refer to the documentation of your distribution or system. The gamedata
is searched at:

- A global directory specified by the package.
- The same directory as the quake2 executable.
- In *$HOME/.yq2*
- The directory given with the *-datadir /path/to/quake2_installation/*
  commandline argument.

If you're a package maintainer, please look at our documentation at
https://github.com/yquake2/yquake2/blob/master/stuff/packaging.md


### Compiling from source

To compile Yamagi Quake II from source, you need the following dependencies,
including development headers:

- A GCC-compatible compiler like GCC, MinGW (see below) or Clang.
- GNU make.
- A libGL implementation with OpenGL system headers.
- SDL 2.0.
- A OpenAL implementation, openal-soft is highly recommended.

While Yamagi Quake II ships with an optional CMakeFile using GNU make for
release builds is highly recommended. The GNU Makefile offers more options
and is well tested.
 

#### On Linux distribution or BSD systems

On debian based distributions (including Ubuntu and Mint) the dependencies can
be installed with: `apt-get install build-essential libgl1-mesa-dev libsdl2-dev
libopenal-dev`

On OS X we recommend using Homebrew - https://brew.sh - to install the
dependencies: `brew install sdl2 openal-soft`

On FreeBSD you'll need something like: `pkg install gmake libGL sdl2 openal-soft`


#### On Windows

On Windows a MinGW environment is needed. A preinstalled environment with all
dependencies can be found at https://deponie.yamagi.org/quake2/windows/build/
Just extract it into *C:\MSYS2\* and start either the 32 bit or 64 bit version
through *C:\MSYS2\msys32.exe* or *C:\MSYS2\msys64.exe*. With the preinstalled
MinGW environment GNU Make is highly recommended, CMake requires further
configuration. At this time Yamagi Quake II can't be compiled with Microsoft
Visual Studio.


#### Compiling

Download the latest release from https://www.yamagi.org/quake2 or clone the
source from https://github.com/yquake2/yquake2.git, change into the *yquake2/*
source directory and type *make*. After the build finished copy everything from
the *release/* directory to your Yamagi Quake II installation directory.

For the addons download or clone their source, change into the source directory
and type *make*. After the compilation finishes the *release/game.so* is copied to
the corresponding directory in your Quake II installation.
