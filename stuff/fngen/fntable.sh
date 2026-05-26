#!/bin/bash
if [[ "$2" == "" ]]; then
	echo -e "fntable.sh <gamesrcdir> <pointer name> [<list name>]\nList name uses pointer name if not specified"
	exit 0
fi

if [[ "$1" == "" ]]; then
	GAMESRCDIR=.
else
	GAMESRCDIR=$1
fi

if [[ "$3" == "" ]]; then
	TBLNAME=$2
else
	TBLNAME=$3
fi

echo -e "static const fnlist_entry_t fnentries_$TBLNAME[] =\n{"

grep -rhoP '>'$2'\s*=\s*\K[a-zA-Z0-9_]+(?=\;)' "$GAMESRCDIR"|\
	awk 'NF && $0 != "NULL" && !seen[$0]++ { print gensub(/([_a-zA-Z0-9]+)/, "\t{\"\\1\", (byte *)\\1},", "g") }'

echo -e "};\n\
static const functionList_t fnlist_$TBLNAME =\n\
{\n\
\t\"$TBLNAME\",\n\
\tfnentries_$TBLNAME, ARREND(fnentries_$TBLNAME),\n\
};\n"
