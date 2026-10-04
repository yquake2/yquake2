# Windows Build Scripts

These scripts are used to build Yamagi Quake II and its addons under
Windows. They rely on mingw-w64, at least Yamagi Quake II can also be
build with Visual Studio. At this time the addons don't support Visual
Studio.


## makebuildenv.sh

This script constructs the official build environment, published at
https://deponie.yamagi.org/quake2/windows/buildenv/ and used by the
Github CI workers for the Windows flows. Requires an internet connection
to download the components and about 1,5 GB of free space. Can be run
under Windows within any POSIX shell or Bash environment. git-bash and
the build environment itself are tested. Also many unixoid platforms
like FreeBSD or Linux are supported.

The exact versions of the components used are hardcoded at the top of
the script.


## makewinrelease.sh

This script is used to create the official Windows release package.
Checkouts of yquake2, ctf, rogue and xatrix are required. A curl.dll
must also be given,. The script iterates through the given checkouts,
does a `make clean`, followed by a `make` and constructs an output
directory containing the compiled binaries and documentation.


## curl.dll

The curl.dll for the official Windows release packages is compiled with
Visual Studio.

1. Get the sources from https://curl.se and extract them. Be carefull
   that the version should be the same as in in the build environment.
   Missmatches between headers and the library can cause problems.
2. Open a "Native Tools Command Prompt" and navigate to the sources.
3. Generate the project files with the Visual Studio cmake derivate:
   `cmake -DHTTP_ONLY=ON -DCURL_USE_SCHANNEL=ON -DCURL_USE_LIBPSL=OFF
    -DCURL_STATIC_CRT=ON -DCMAKE_BUILD_TYPE=Release -B build\ -G 
    "Visual Studio 17 2022" -A Win32` Use you Visual Studio version and
    select the correct architecture.
4. Compile with `cmake --build build/ --config Release`
5. The results are written to build/lib/Release.

The library will work on all Windows versions with UCRT support. On
older Windows versions the library might not load and HTTP downloads
won't be available.
