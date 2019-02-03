/*
 * Copyright (C) 1997-2001 Id Software, Inc.
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
 * Fields of the client to be saved.
 *
 * =======================================================================
 */

{"pers.weapon", CLOFS(pers.weapon), F_ITEM},
{"pers.lastweapon", CLOFS(pers.lastweapon), F_ITEM},
{"newweapon", CLOFS(newweapon), F_ITEM},
{"resp.coop_respawn.weapon", CLOFS(resp.coop_respawn.weapon), F_ITEM, 0, 3},
{"resp.coop_respawn.lastweapon", CLOFS(resp.coop_respawn.lastweapon), F_ITEM, 0, 3},
{NULL, 0, F_INT, 0}
