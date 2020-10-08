# Yamagi Quake II Commands

This list explains all commands added by Yamagi Quake II. All of the
original clients (Vanilla Quake II) commands are still in place.


* **cycleweap <weapons>**: Cycles through the given weapons. Can be used
  to bind several weapons on one key. The list is provided as a list of
  weapon classnames separated by whitespaces. A weapon in the list is
  skipped if it is not a valid weapon classname, you do not own it in
  your inventory or you do not have enough ammo to use it.

* **listentities <class>**: Lists the coordinates of all entities of a
  given class.  Possible classes are `ammo`, `items`, `keys`, `monsters`
  and `weapons`. Multiple classes can be given, they're separated by
  whitespaces. The special class `all` lists the coordinates of all
  entities.

* **teleport <x y z>**: Teleports the player to the given coordinates.

* **listmaps**: Lists available maps for the player to load. Maps from
  loaded pak files will be listed first followed by maps placed in 
  the current game's maps folder.

* **vstr**: Inserts the current value of a variable as command text.
