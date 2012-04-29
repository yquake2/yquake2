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
 * A header file for some of the plattform dependend stuff. This stuff
 * is implemented in unix/ and sdl/
 *
 * =======================================================================
 */

#ifndef UNIX_UNIX_H
#define UNIX_UNIX_H

typedef void ( *Key_Event_fp_t )( int key, qboolean down );
extern void ( *IN_Update_fp )( void );

typedef struct in_state
{
	/* Pointers to functions back in client, set by vid_so */
	void ( *IN_CenterView_fp )( void );
	Key_Event_fp_t Key_Event_fp;
	vec_t *viewangles;
	int *in_strafe_state;
	int *in_speed_state;
} in_state_t;

void registerHandler(void);

#endif
