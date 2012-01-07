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
 * This file implements some misc stuff like the main loop
 *
 * =======================================================================
 */

#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../common/header/common.h"
#include "header/unix.h"

cvar_t *nostdout;
uid_t saved_euid;

char *
strlwr ( char *s )
{
	char *p = s;

	while ( *s )
	{
		*s = tolower( *s );
		s++;
	}

	return ( p );
}

int
main ( int argc, char **argv )
{
	int time, oldtime, newtime;

    /* register signal handler */
	registerHandler();

	/* go back to real user for config loads */
	saved_euid = geteuid();
	seteuid( getuid() );

	printf( "\nYamagi Quake II v%4.2f\n", VERSION);
	printf( "=====================\n\n");

	Qcommon_Init( argc, argv );

	fcntl( 0, F_SETFL, fcntl( 0, F_GETFL, 0 ) | FNDELAY );

	nostdout = Cvar_Get( "nostdout", "0", 0 );

	if ( !nostdout->value )
	{
		fcntl( 0, F_SETFL, fcntl( 0, F_GETFL, 0 ) | FNDELAY );
	}

	oldtime = Sys_Milliseconds();

	/* The legendary Quake II mainloop */
	while ( 1 )
	{
		/* find time spent rendering last frame */
		do
		{
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
		}
		while ( time < 1 );

		Qcommon_Frame( time );
		oldtime = newtime;
	}

	return 0;
}  

