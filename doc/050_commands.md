# Yamagi Quake II Commands

This list explains all commands added by Yamagi Quake II. All of the
original clients (Vanilla Quake II) commands are still in place.


* **cycleweap <weapons>**: Cycles through the given weapons. Can be used
  to bind several weapons on one key. The list is provided as a list of
  weapon classnames separated by whitespaces. A weapon in the list is
  skipped if it is not a valid weapon classname, you do not own it in
  your inventory or you do not have enough ammo to use it.
  By quickly tapping the bound key, you can navigate the list faster.

* **prefweap <weapons>**: Similar to the previous command, this will
  select the first weapon available in the priority list given. Useful
  to set a "panic button". E.g. the following will select your best
  shotgun: `prefweap weapon_supershotgun weapon_shotgun`.

* **gamemode <mode>**: Provides a convenient way to switch the game mode
  between `coop`, `dm` and `sp` without having to set three cvars the
  correct way. `?` prints the current mode.

* **listentities <class>**: Lists the coordinates of all entities of a
  given class.  Possible classes are `ammo`, `items`, `keys`, `monsters`
  and `weapons`. Multiple classes can be given, they're separated by
  whitespaces. The special class `all` lists the coordinates of all
  entities.

* **viewpos**: Show player position.

* **teleport <x y z>**: Teleports the player to the given coordinates.

* **spawnentity classname x y z <angle_x angle_y angle_z> <flags>**:
  Spawn new entity of `classname` at `x y z` coordinates.

* **spawnonstart classname**: Spawn new entity of `classname` at start point.

* **listmaps**: Lists available maps for the player to load. Maps from
  loaded pak files will be listed first followed by maps placed in 
  the current game's maps folder.

* **vstr**: Inserts the current value of a variable as command text.

* **playermodels**: Lists available multiplayer models.
