#!/bin/bash
if [[ "$2" == "" ]]; then
	echo -e "fnproto.sh <gamesrcdir> <pointer> <return type> <arg list>\nList name uses pointer name if not specified"
	exit 0
fi

if [[ "$1" == "" ]]; then
	GAMESRCDIR=.
else
	GAMESRCDIR=$1
fi

echo "// $2"
grep -rhoP '>'$2'\s*=\s*\K[a-zA-Z0-9_]+(?=\;)' "$GAMESRCDIR"|\
	awk 'NF && $0 != "NULL" && !seen[$0]++ { print gensub(/([_a-zA-Z0-9]+)/, "extern '"$3"' \\1 '"$4"';", "g") }'
