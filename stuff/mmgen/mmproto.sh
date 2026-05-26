#!/bin/bash
if [[ "$1" == "" ]]; then
	GAMESRCDIR=.
else
	GAMESRCDIR=$1
fi

grep -rzhoP 'mmove_t\s+\K[a-zA-Z0-9_]+(?=\s*=\s*{)' "$GAMESRCDIR"|\
	awk '!seen[$0]++ { print gensub(/([_a-zA-Z0-9]+)/, "extern mmove_t \\1;\n", "g") }'
