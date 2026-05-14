# Mmove list generator

## About

These scripts scan the game code for `mmove_t`
objects and generate table entries and prototypes
for the `mmove_t` list used by savegames.

## Usage

```
# cd to stuff/mmgen
./mmproto.sh ../../src/game > prototypes.txt
./mmtable.sh ../../src/game > table.txt
```

Then copy the content of the output files to the
respective header files in `src/game/savegame/tables`.

## Dependencies

These scripts use the following programs / commands:
* dirname
* awk
* grep

Windows users may need to download and install
one or more of these. Tested with Git Bash.

## Implementation details

`grep` looks for `mmove_t (some_name) = {`
patterns. Then the list of names is sent
through `awk` to remove any duplicate names.

`mmproto.sh` generates a list of prototypes:
`extern mmove_t some_name;`

`mmtable.sh` generates a list of table entries:
`{"some_name", &some_name},`