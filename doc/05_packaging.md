# Notes for Package Maintainers

This guide shows how Yamagi Quake II should be packaged and what
assumptions the games makes regarding binary and game data locations.

This guide uses the notation for unixoid platforms. Executables have no
file extensions and libraries are given with the *.so* extension. MacOS
and Windows behave exactly the same way, but with the platform specific
file extensions.


## The Executables

Yamagi Quake II expects all binaries (executables and libraries) to be
in the same directory or, in the case of *game.so*, in the mod-specific
subdirectory.

So the binary directory should look somehow like this:

* /path/to/yamagi-quake2/
  * quake2
  * q2ded
  * ref_gl1.so
  * ref_gl3.so
  * baseq2/
    * game.so
  * xatrix/
    * game.so
  * ... (the same for other addons)

Yamagi Quake2 will get the directory the `quake2` executable is in from
the system and then look in that directory (and nowhere else!) for the
`ref_*.so` renderer libraries. It will look for `game.so` there first,
but if it's not found in the binary directory, it will look for it in
all directories that are also searched for game data.  This is for
better compatibility with mods that might ship their own game.so.

You can **just symlink the executables to a directory into the $PATH**,
like */usr/bin/*. There's one exception to this rule: OpenBSD does not
provide a way to get the executable path, so a wrapper script is needed.

We want all binaries to be in the same directory to ensure that people
don't accidentally update only parts of their Yamagi Quake II
installation, so they'd end up with a new quake2 executable and old
renderer libraries (`ref_*.so`) and report weird bugs.


## The SYSTEMWIDE and SYSTEMDIR options

The Makefile allows to enable the *SYSTEMWIDE* feature to force Yamagi
Quake II to search the game data in an system specific directory. That
directory can be given in the Makefile. If no directory is given the
game defaults to */usr/share/games/quake2/* which should be correct for
most Linux distributions.

The `SYSTEMDIR` is meant to contain only the game data and *not* the
binaries. It allows several Quake2 source ports to share the same game
data.


## Alternative startup config

Yamagi Quake II has support for an alternative startup config. That
config overrides some global values with sane defaults. It may be a good
idea to install it. Copy yq2.cfg to the baseq2/ subdirectory in the
gamedata directory.
