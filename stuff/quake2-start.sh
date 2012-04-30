#!/bin/sh
set -eu

# PID of "gnome-screensaver-command -i" (if used)
GSC_PID=""

# if gnome-screensaver is running in the background..
if ps auxww | grep -q 'gnome-screensaver'; then
    echo "inhibiting gnome screensaver"
    gnome-screensaver-command -i &
    # save the PID of the last command
    GSC_PID="$!"
fi

# Stop unclutter
if ps auxww | grep -q 'unclutter'; then
	echo 'inhibiting unclutter'
	killall -STOP unclutter
fi

# enable core dumps
ulimit -c unlimited

# run quake 2
./quake2 "$@"

# Continue unclutter
if ps auxww | grep -q 'unclutter'; then
	echo 'reactivating unclutter'
	killall -CONT unclutter
fi

# if gnome-screensaver was running..
if [ -n "$GSC_PID" ]; then
    echo "reactivating gnome screensaver"
    kill "$GSC_PID"
fi
