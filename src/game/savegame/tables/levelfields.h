/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2011 Knightmare
 * Copyright (C) 2011 Yamagi Burmeister
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * Fields inside a level to be saved.
 *
 * =======================================================================
 */

{"changemap", LLOFS(changemap), F_LSTRING},
{"sight_client", LLOFS(sight_client), F_EDICT},
{"sight_entity", LLOFS(sight_entity), F_EDICT},
{"sound_entity", LLOFS(sound_entity), F_EDICT},
{"sound2_entity", LLOFS(sound2_entity), F_EDICT},
{NULL, 0, F_INT}
