#!/bin/sh 
# simple script that rips CDs to a format useable by Yamagis Quake2 client 
# Needs cdparanoia and oggenc, useable with Quake II and both addons. 

# Create directory
mkdir music 
cd music 

# rip all tracks beginning with second one (the first track is data) 
cdparanoia -B "2-" 

# could certainly be more elegant.. 
for i in "02" "03" "04" "05" "06" "07" "08" "09" "10" "11" 
do 
    oggenc -q 6 -o $i.ogg track$i.cdda.wav 
done 

# remove .wav files 
rm *.wav 

echo -e "\n Ripping done, move music/ directory to /your/path/to/quake2/game/music/"
