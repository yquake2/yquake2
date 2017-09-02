# Yamagi Quake II

This is the Yamagi Quake II Client, an enhanced version of id Software's Quake
II with focus on offline and coop gameplay. Both the gameplay and the graphics
are unchanged, but many bugs if the last official release were fixed and some
nice to have features like widescreen support and a modern OpenGL 3.2 renderer
were added. Unlike most other Quake II source ports Yamagi Quake II is fully 64
bit clean. It works perfectly on modern processors and operating systems. Yamagi
Quake II runs on nearly all common platforms; including FreeBSD, Linux, OpenBSD,
Windows and OS X.

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
- etc.

Yamagi Quake II supports all of these version, even the rather limited demo.
Full versions require about 1.6 GiB hard drive space, the demo version about 150
Mib.


#### Full versions

The easiest way to install a full version of Quake II is to start with the
patch. Please note that the patch is **required** for all full versions of the
game, even the newer ones like Steam. Without it Yamagi Quake II will not work!

1. Download the patch:
   http://deponie.yamagi.org/quake2/idstuff/q2-3.20-x86-full-ctf.exe
2. Extract the patch into an empty directory. The patch is just an ordinary
   self-extracting ZIP file. On Windows it can be extracted by double clicking
   on it, on other systems an archiver or even the *unzip* command can be used.

Now it's time to remove the following files from the extracted patch. They're
the original executables, documentation and so on. They aren't needed anymore:

- 3.20_Changes.txt
- quake2.exe
- ref_gl.dll
- ref_soft.dll
- baseq2/gamex86.dll
- ctf/ctf2.ico
- ctf/gamex86.dll
- ctf/readme.txt
- ctf/server.cfg
- xatrix/gamex86.dll
- rogue/gamex86.dll

Copy the pak0.pak file and the video/ subdirectory from your Quake II
distribution (CD, Steam download, etc) into the baseq2/ subdirectory of the
extracted patch.

If you own the optional addons you'll need to copy their gamedata too:

- For The Reckoning copy the pak0.pak and the video/ subdirectory from your
  addon distribution into the xatrix/ subdirectory.
- For Ground Zero copy the pak0.pak and the video/ subdirectory from your
  addon distribution into the rogue/ subdirectory.


#### The demo version

1. Get the demo from http://deponie.yamagi.org/quake2/idstuff/q2-314-demo-x86.exe
   and extract it. It's just an ordinary, self-extract ZIP file. On Windows it
   can be extracted by double clicking on it, on other system an archiver or
   even the *unzip* command can be used.
3. Create a new directory and a subdirectory baseq2/ in it.
3. Copy the pak0.pak and the players/ subdirectory from Install/Data/baseq2/
   into the newly created baseq2/ subdirectory.

The demo **must not** be patched! Patching the demo will break it!


### Music extraction

The retail CD version of Quake II and both addons contain up to 11 Audio CD
tracks, forming the soundtrack. Since modern computers lack the ability for
classic Audio CD playback, it's often emulated by a transparent combination 
of CD ripping and playback. The tracks can be ripped into OGG/Vorbis files.
Yamagi Quake II will use these files instead of the CD tracks.

Later Quake II version, for example the one included with Quake IV and the one
available through Steam, lack the soundtrack. Nevertheless Yamagi Quake II can
play it if you copy the files into the directories mentioned above. 


#### Using a generic CD extractor / CD ripper

1. Install a CD extractor and set it to OGG/Vorbis files.
2. Put the Quake II CD into your CD drive and extract the files.
3. The files must be named after the CD track. Track 02 becomes 02.ogg, track
   03 becomes 03.ogg and so on. On both the Quake II and the Addon CDs track 1
   is the data track and can't be ripped.
4. Put these files into the corresponding subdirectory:
    - baseq2/music for Quake II.
    - xatrix/music for The Reckoning.
    - rogue/music for Ground Zero.


#### Extracting music on BSD, Linux and OS X 

An easy way to extract the music on unixoid platforms is to use
*stuff/cdripper.sh*, a simple shellscript we provide. It needs *cdparanoia* and
oggenc from the *vorbis-tools* package installed. Use your package manager of
choice (apt-get, dnf, homebrew, ...) to install them. Just execute the script
and copy the resulting music/ directory to:
	- baseq2/ for Quake II.
    - xatrix/ for The Reckoning.
    - rogue/ for Ground Zero.
 

### Download and extract the executables

How the Yamagi Quake II executables are installed depends on the platform:

- For Windows a package with all Yamagi Quake II executables is provided.
- Some Linux distributions and BSD systems provide Yamagi Quake II packages.
- On OS X you need to compile from source.
- Of course Yamagi Quake II can be compiled from source on all platforms.


#### Microsoft Windows

1. Get the latest release from http://www.yamagi.org/quake2
2. Extract it into the gamedata directory created above. quake2.exe must be
   placed next to the baseq2/ subdirectory.

On Windows Yamagi Quake II is fully portable, you can move the installation
directory wherever and whenever you like. To update Yamagi Quake II just
overwrite the old files with the new ones.


#### Binary Package from your Linux distribution or BSD system

Many Linux distributions and BSD systems provide Yamagi Quake II packages.
Please refer to the documentation of your distribution or system. The gamedata
is searched at:

- A global directory specified by the package.
- The same directory as the quake2 executable.
- In $HOME/.yq2
- The directory given with the *+set basedir /path/to/quake2_installation/*
  commandline argument.

If you're a package maintainer, please look at our documentation at
[stuff/packaging.md](stuff/packaging.md).


### Compiling from source

To compile Yamagi Quake II from source, you need the following dependencies,
including development headers:

- A GCC-compatible compiler like GCC, MinGW (see below) or clang.
- GNU make or cmake.
- A libGL implementation with OpenGL system headers.
- SDL 2.0 or SDL 1.2 (2.0 recommended, edit the Makefile to use 1.2 instead).
- libogg, libvorbis and libvorbisfile.
- A OpenAL implementation, openal-soft is highly recommended.
- zlib.

Yamagi Quake II has 2 build systems:

- A classic GNU Makefile.
- A CMake file.

Both build system provide the same options and features, it's up to the user to
decide which one to use.


#### On Linux distribution or BSD systems

On debian based distributions (including Ubuntu and Mint) the dependencies can
be installed with: `apt-get install build-essential libgl1-mesa-dev libsdl2-dev
libogg-dev libvorbis-dev libopenal-dev zlib1g-dev`

On OS X we recommend using [homebrew](http://brew.sh) to install the
dependencies: `brew install sdl2 libvorbis libogg openal-soft`

On FreeBSD you'll need something like: `pkg install gmake libGL sdl2 libogg
libvorbis openal-soft`


#### On Windows

On Windows a MinGW environment is needed. A preinstalled environment with all
dependencies can be found at http://deponie.yamagi.org/quake2/windows/build/
Just extract it into C:\MSYS2\ and start either the 32 bit or 64 bit version
through *C:\MSYS2\msys32.exe* or *C:\MSYS2\msys64.exe*. With the preinstalled
MinGW environment GNU Make is highly recommended, CMake requires further
configuration. At this time Yamagi Quake II can't be compiled with Microsoft
Visual Studio.


#### Compiling

Download the latest release from http://www.yamagi.org/quake2 or clone the
source from https://github.com/yquake2/yquake2.git, change into the yquake2/
source directory and type *make*. After that copy everything from the release/
directory to your Yamagi Quake II installation directory.

For the addons download or clone their source, change into the source directory
and type *make*. After the compilation finishes the release/game.so is copied to
the corresponding directory in your Quake II installation.
 
