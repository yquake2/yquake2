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
 */
#ifndef SAVEGAME_LOCAL_H
#define SAVEGAME_LOCAL_H

/*
 * Connects a human readable
 * function signature with
 * the corresponding pointer
 */
typedef struct
{
	char *funcStr;
	byte *funcPtr;
} functionList_t;

/*
 * Connects a human readable
 * mmove_t string with the
 * corresponding pointer
 * */
typedef struct
{
	char	*mmoveStr;
	mmove_t *mmovePtr;
} mmoveList_t;

typedef struct
{
    char ver[32];
    char game[32];
    char os[32];
    char arch[32];
} savegameHeader_t;

#endif /* SAVEGAME_LOCAL_H */
