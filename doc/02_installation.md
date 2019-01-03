# Installation

This guide shows how to install Yamagi Quake II from scratch. All
fully supported platforms and both the full version and the demo
version are covered.


## The Full Version

Over the years Quake II was distributed in a myriad of ways:

* As retail release on CD.
* As part of Quake IV.
* Through Steam.
* Through GOG.com.
* etc.

Yamagi Quake II is compatible with all of these versions. While some of
these versions come with all patches applied it's highly recommended to
follow this guide step by step and to reapply the patch by hand. Not all
distributors patched the game correctly, leading to serve problems like
missing assets or even crashes.


### Game Data Setup

The easiest way to install Yamagi Quake II is to start with the patch.
Please note that the patch is **required** for all full versions of the
game. Without the patch the game won't work correctly!

1. Download the patch from our mirror or somewhere else. Its MD5
   checksum is `490557d4a90ff346a175d865a2bade87`:
   https://deponie.yamagi.org/quake2/idstuff/q2-3.20-x86-full-ctf.exe
2. Extract the patch into an empty directory. The patch comes as an
   self-extracting ZIP file. On Windows it can be extracted by double
   clicking on it, on other systems an archiver or the *unzip* command
   can be used.

Now remove the following files from the extracted patch. They're the
original executables, documentation and so on. They aren't needed for
Yamagi Quake II and just waste space:

* *3.20_Changes.txt*
* *quake2.exe*
* *ref_gl.dll*
* *ref_soft.dll*
* *baseq2/gamex86.dll*
* *ctf/ctf2.ico*
* *ctf/gamex86.dll*
* *ctf/readme.txt*
* *ctf/server.cfg*
* *xatrix/gamex86.dll*
* *rogue/gamex86.dll*

Copy the *pak0.pak* file and the *video/* subdirectory from the Quake
II distribution (CD, Steam or GOG download, etc) into the *baseq2/*
subdirectory of the extracted patch.

If the optional addons are available their gamedata must be copied too:

* For The Reckoning (also know as "xatrix") copy the *pak0.pak* and the
  *video/* subdirectory from the addon distribution into the *xatrix/*
  subdirectory.
* For Ground Zero (also known as "rogue") copy the *pak0.pak* and the
  *video/* subdirectory from the addon distribution into the *rogue/*
  subdirectory.

The MD5 checksums of the pakfiles are:

* *baseq2/pak0.pak*: `1ec55a724dc3109fd50dde71ab581d70`
* *baseq2/pak1.pak*: `42663ea709b7cd3eb9b634b36cfecb1a`
* *baseq2/pak2.pak*: `c8217cc5557b672a87fc210c2347d98d`
* *ctf/pak0.pak*: `1f6bd3d4c08f7ed8c037b12fcffd2eb5`
* *rogue/pak0.pak*: `5e2ecbe9287152a1e6e0d77b3f47dcb2`
* *xatrix/pak0.pak*: `f5e7b04f7d6b9530c59c5e1daa873b51`


### Music Extraction

The retail releases of Quake II and both addons contain up to 11 Audio
CD tracks as soundtrack. Since modern computers lack the ability for
direct CD playback Yamagi Quake II reads the music from OGG/Vorbis
files.

Later Quake II releases, for example the one included with Quake IV and
the one available through Steam, lack the soundtrack. Nevertheless
Yamagi Quake II can play it if the files are copied into the directories
mentioned below.

Some digital distributed versions are special, they includes the
soundtrack as OGG/Vorbis files, but in a non standard format. Yamagi
Quake II can read this format for the GOG.com release. Other releases
may be supported in the future.


#### Using a Generic CD Extractor

1. Install a CD extractor (for example CDex) and set it to OGG/Vorbis
   files. Quality factor 6 (192 kbit/s) is usually more than enough.
2. Put the Quake II CD into the CD drive and extract the files.
3. The files must be named after the corresponding CD track: CD track 02
   becomes the file *02.ogg*, CD track 03 becomes the file *03.ogg* and
   so on. On both the Quake II and the Addon CDs track 01 is the data
   track and thus can't be ripped.
4. Put these files into the corresponding subdirectory:
	* baseq2/music for Quake II.
	* xatrix/music for The Reckoning.
	* rogue/music for Ground Zero.


#### Using a Shell Script

An easy way to extract the music on unixoid platforms (BSD, Linux and
MacOS) is to use *stuff/cdripper.sh*, a simple shellscript. It needs
*cdparanoia* and *oggenc* (from the *vorbis-tools* package) installed.
Use the package manager (apt, dnf, homebrew, pkg, ...) to install them.
Just execute the script and copy the resulting *music/* directory to:
  * *baseq2/* for Quake II.
  * *xatrix/* for The Reckoning.
  * *rogue/* for Ground Zero.


#### The GOG.com Release

The Quake II distributed by GOG.com contains the soundtrack, it just
needs to be copied into the game data directory. The target directory is
just *music/*, next to *baseq2/*. **Not** inside *baseq2/*.


### Alternate Startup Configuration

Yamagi Quake II ships with an alternative startup config that overrides
some gloibal settings to saner defaults. To use is copy *stuff/yq2.cfg*
into the *baseq2/* directory.


## The Demo Version

A free demo version of Quake II is available and supported by Yamagi
Quake II. This demo contains the first few levels, but no videos and
no soundtrack.


### Game Data Setup

1. Download the demo from our mirror or somewhere else. Its MD5
   checksum is `4d1cd4618e80a38db59304132ea0856c`:
   https://deponie.yamagi.org/quake2/idstuff/q2-314-demo-x86.exe
2. Extract the downloaded file. It's an ordinary, self-extract ZIP
   archive. On Windows it can be extracted by double clicking on it, on
   other system an archiver or the *unzip* command can be used.
3. Create a new directory and a subdirectory *baseq2/* in it.
4. Copy the *pak0.pak* and the *players/* subdirectory from the
   extracted archive into the newly created *baseq2/* subdirectory.

The demo **must not** be patched! Patching the demo will break it!

The MD5 checksums of the pakfiles are:

* *baseq2/pak0.pak*: `27d77240466ec4f3253256832b54db8a`


## Download and Extract the Executables

How the Yamagi Quake II executables are installed depends on the
platform:

- For Windows a prebuild package with all Yamagi Quake II executables
  and the required libraries is provided.
- Most Linux distributions and BSD systems provide Yamagi Quake II
  packages. Theses packages may be outdated, see below for compiling
  the executables.


### Windows

1. Get the latest release from https://www.yamagi.org/quake2
2. Extract it into the gamedata directory created above. *quake2.exe*
   must be placed next to the *baseq2/* subdirectory.

On Windows the Yamagi Quake II installation is fully portable, the
installation directory can be moved the installation directory wherever
and whenever it's necessary. To update Yamagi Quake II just overwrite
the old files with the new ones.

There're two executables:

* *yquake2.exe*: This is main executable and should be preferred.
* *quake.exe*: This is just a wrapper to stay compatible with existing
  setups. For technical reasons *quake.exe* may not start in foreground
  but in background!

If Windows Defender is activated, that's the default on Windows 8 and
Windows 10, it may complain that Yamagi Quake II is untrusted and should
not be started. That's because we're shipping unsigned binaries. You can
force Windows to start it anyways.


### Binary Package from Linux distributions or BSD systems

Most Linux distributions and BSD systems provide Yamagi Quake II
packages. Please refer to the documentation of the distribution or
system. The gamedata is searched at:

- A global directory specified by the package.
- The same directory as the quake2 executable.
- A directory given with the *-datadir /path/to/quake2_installation/*
  commandline argument.
- In *$HOME/.yq2*

If you're a package maintainer, please look at our documentation at
the [Packaging Guide](05_packaging.md)

## Compiling from source

To compile Yamagi Quake II from source the following dependencies
(including development headers) are needed:

* A GCC compatible compiler like *gcc*, *clang* or *mingw*.
* A LibGL implementation with system headers.
* An OpenAL implementation, *openal-soft* is highly recommended.
* SDL 2.0.
* libcurl

While Yamagi Quake II ships with an CMakeFile.txt using the GNU Makefile
for release builds is recommended. The GNU Makefile offers more options
and is well tested.


### Prerequisites on Windows

On Windows a MinGW environment is needed. A preconfigured environment
with all necessary dependencies and compatibles compilers can be found
at: https://deponie.yamagi.org/quake2/windows/build/

The environment must be extracted into *C:\MSYS2\* (other directory will
likely work but are unsupported). Either the 32 bit version can be
started through *C:\MSYS2\msys32.exe* or the 64 bit version through
*C:\MSYS2\msys64.exe*.

At this time Yamagi Quake II can't be compiled with Microsoft Visual
Studio.


### Prerequisites on Unixoid Platforms

The build dependencies can be installed with:

* On Debian based distributions: `apt install build-essential
  libgl1-mesa-dev libsdl2-dev libopenal-dev libcurl4-openssl-dev`
* On FreeBSD: `pkg install gmake libGL sdl2 openal-soft curl`
* On MacOS the dependencies can be installed with Homebrew (from
  https://brew.sh): `brew install sdl2 openal-soft`

Other distributions or platforms often have package names similar to the
Debian or FreeBSD packages.


### Compiling

Download the latest release from https://www.yamagi.org/quake2 or clone
the source from https://github.com/yquake2/yquake2.git, change into the
*yquake2/* source directory and type *make* (Linux, MacOS and Windows)
or *gmake* (FreeBSD). After the build finished copy everything from the
*release/* directory to the Yamagi Quake II installation directory.

For the addons download or clone their source, change into the source
directory and type *make* (Linux, MacOS and Windows) or *gmake*
(FreeBSD). After the compilation finishes the *release/game.so* is
copied to the corresponding directory in the Quake II installation.
