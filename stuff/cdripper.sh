#!/bin/sh
set -eu

# simple script that rips CDs to a format useable by Yamagis Quake2 client
# Needs cdparanoia and oggenc, useable with Quake II and both addons.

# Create directory
mkdir -p music
cd music

# rip all tracks beginning with second one (the first track is data)
cdparanoia -B "2-"

for I in track*.cdda.wav; do
    NUM="${I#track}"
    NUM="${NUM%.cdda.wav}"
    oggenc -q 6 -o "$NUM.ogg" "$I"
done

# remove .wav files
rm *.wav

echo '
Ripping done, move music/ directory to /your/path/to/quake2/game/music/'
