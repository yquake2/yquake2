#!/bin/sh 

# PID of "gnome-screensaver-command -i" (if used) 
GSC_PID=0 

# if gnome-screensaver is running in the background.. 
if [ $(ps auxww | grep gnome-screensaver | wc -l) -gt 0 ]; then 
    echo "inhibiting gnome screensaver" 
    gnome-screensaver-command -i & 
    # save the PID of the last command 
    GSC_PID=$! 
fi 

# run quake 2 
# ./quake2 $* 

# if gnome-screensaver was running.. 
if [ $GSC_PID -gt 0 ]; then 
    echo "reactivating gnome screensaver"     
    kill $GSC_PID 
fi
