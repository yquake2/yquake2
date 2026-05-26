#!/bin/bash

HERE=$(dirname "$0")

$HERE/fnproto.sh "$1" die void "(edict_t *, edict_t *, edict_t *, int, const vec3_t)"
$HERE/fnproto.sh "$1"  pain void "(edict_t *, edict_t *, float, int)"
$HERE/fnproto.sh "$1" prethink void "(edict_t *)"
$HERE/fnproto.sh "$1" think void "(edict_t *)"
$HERE/fnproto.sh "$1" touch void "(edict_t *, edict_t *, const cplane_t *, const csurface_t *)"
$HERE/fnproto.sh "$1" use void "(edict_t *, edict_t *, edict_t *)"
$HERE/fnproto.sh "$1" blocked void "(edict_t *, edict_t *)"

$HERE/fnproto.sh "$1" monsterinfo.stand void "(edict_t *)"
$HERE/fnproto.sh "$1" monsterinfo.walk void "(edict_t *)"
$HERE/fnproto.sh "$1" monsterinfo.run void "(edict_t *)"
$HERE/fnproto.sh "$1" monsterinfo.melee void "(edict_t *)"
$HERE/fnproto.sh "$1" monsterinfo.attack void "(edict_t *)"
$HERE/fnproto.sh "$1" monsterinfo.checkattack qboolean "(edict_t *)"
$HERE/fnproto.sh "$1" monsterinfo.dodge void "(edict_t *, edict_t *, float)"
$HERE/fnproto.sh "$1" monsterinfo.idle void "(edict_t *)"
$HERE/fnproto.sh "$1" monsterinfo.search void "(edict_t *)"
$HERE/fnproto.sh "$1" monsterinfo.sight void "(edict_t *, edict_t *)"

$HERE/fnproto.sh "$1" moveinfo.endfunc void "(edict_t *)"
