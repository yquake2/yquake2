/*
 * Copyright (C) 1997-2001 Id Software, Inc.
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
 * The header for the command processor system
 *
 * =======================================================================
 */

#ifndef CO_CMD_H
#define CO_CMD_H

#define	MAX_ALIAS_NAME	32
#define	ALIAS_LOOP_COUNT 16

typedef struct cmdalias_s {
	struct cmdalias_s	*next;
	char	name[MAX_ALIAS_NAME];
	char	*value;
} cmdalias_t;

cmdalias_t	*cmd_alias;
qboolean	cmd_wait;

int		alias_count; /* for detecting runaway loops */

void Cmd_Exec_f (void);
void Cmd_Echo_f (void);
void Cmd_Alias_f (void);
void Cmd_Wait_f (void);

#endif
