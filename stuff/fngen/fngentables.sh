#!/bin/bash

HERE=$(dirname "$0")

$HERE/fntable.sh "$1" die
$HERE/fntable.sh "$1" pain
$HERE/fntable.sh "$1" prethink
$HERE/fntable.sh "$1" think
$HERE/fntable.sh "$1" touch
$HERE/fntable.sh "$1" use
$HERE/fntable.sh "$1" blocked

$HERE/fntable.sh "$1" monsterinfo.stand mi_stand
$HERE/fntable.sh "$1" monsterinfo.walk mi_walk
$HERE/fntable.sh "$1" monsterinfo.run mi_run
$HERE/fntable.sh "$1" monsterinfo.melee mi_melee
$HERE/fntable.sh "$1" monsterinfo.attack mi_attack
$HERE/fntable.sh "$1" monsterinfo.checkattack mi_checkattack
$HERE/fntable.sh "$1" monsterinfo.dodge mi_dodge
$HERE/fntable.sh "$1" monsterinfo.idle mi_idle
$HERE/fntable.sh "$1" monsterinfo.search mi_search
$HERE/fntable.sh "$1" monsterinfo.sight mi_sight

$HERE/fntable.sh "$1" moveinfo.endfunc mv_end
