# Notes for Package Maintainers

Our 7.00 release caused some trouble for package maintainers
(see https://github.com/yquake2/yquake2/issues/214), so we decided to finally
properly document how we think it should be done and what assumptions
Yamagi Quake II makes regarding binary locations etc.

## Where you should put the executables

Yamagi Quake II expects all binaries (executables and libs) to be in the same directory
(or, in the case of game.so/.dll/dylib, in the mod-specific subdirectory).

So the binary directory should look somehow like this *(on Unix-like systems;
on Windows and OSX it's very similar but with different extensions: .dll or 
.dylib instead of .so, and the executables have .exe file extension on Windows of course)*:

* /path/to/yamagi-quake2/
  - quake2
  - q2ded
  - ref_gl1.so
  - ref_gl3.so
  - baseq2/
    * game.so
  - xatrix/
    * game.so
  - ... *(the same for other addons/mods)*

Yamagi Quake2 will get the directory the `quake2` executable is in from the system
and then look in that directory (and nowhere else!) for `ref_*.so`.  
It will look for `game.so` there first, but if it's not found in the binary directory,
it will look for it in all directories that are also searched for game
data (SYSTEMDIR, basedir, $HOME/.yq2/). This is for better compatibility with mods that
might ship their own game.so.

You can **just symlink the executables to a directory in your $PATH**, like /usr/bin/.  
(*Except on OpenBSD, which does not provide a way to get the executable directory,
 there you'll need a shellscript that first does a `cd /path/to/yamagi-quake2/` and
 then executes `./quake2`*)

We want all binaries to be in the same directory to ensure that people don't accidentally
update only parts of their Yamagi Quake II installtion, so they'd end up with a new
quake2 executable and old render libraries (`ref_*.so`) and report weird bugs.

## The SYSTEMWIDE and SYSTEMDIR options

The Makefile allows you to enable the `SYSTEMWIDE` feature (`WITH_SYSTEMWIDE=yes`) and
lets you specify the directory that will be used (`SYSTEMDIR`, `WITH_SYSTEMDIR=/your/custom/path/`).
If you don't set SYSTEMDIR, it defaults to `/usr/share/games/quake2`, which is what debian uses.

The `SYSTEMDIR` was meant to contain just the game data, *not* the binaries, and allows
several Quake2 source ports to share the same game data.  
Unfortunately, we didn't document this assumption, so some packages used it for both binaries
and data, just binaries or - which causes most trouble - only for the game libs, but not the
executables. The latter case doesn't work anymore since we (re)introduced the render libs,
as they need to be located next to the executable, and if the executable is in /usr/bin/ you
don't want to put libs next to it.

Anyway: If you use `SYSTEMWIDE`/`SYSTEMDIR`, please use it for game data.  
You *can* also put the binaries in there, but in that case please put all of them
(including executables) in there, as explained above.

## Alternative startup config

Yamagi Quake II has support for an alternative startup config.  
It may be a good idea to install it, since it sets some global options to sane defaults.  
Copy yq2.cfg to the baseq2/ subdirectory in the gamedata (`SYSTEMDIR`) directory.
