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
 * This file is the starting point of the program and implements
 * the main loop
 *
 * =======================================================================
 */

#include <fcntl.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../common/header/common.h"
#include "header/unix.h"

cvar_t *nostdout;
uid_t saved_euid;

int
main ( int argc, char **argv )
{
	int time, oldtime, newtime;

    /* register signal handler */
	registerHandler();

	/* go back to real user for config loads */
	saved_euid = geteuid();
	seteuid( getuid() );

	/* enforce C locale */
	setenv("LC_ALL", "C", 1);

	printf( "\nYamagi Quake II v%4.2f\n", VERSION);
	printf( "=====================\n\n");

	printf("Client build options:\n");
#ifdef CDA
	printf(" + CD audio\n");
#else
	printf(" - CD audio\n");
#endif
#ifdef OGG
	printf(" + OGG/Vorbis\n");
#else
	printf(" - OGG/Vorbis\n");
#endif
#ifdef ZIP
	printf(" + Zip file support\n");
#else
	printf(" - Zip file support\n");
#endif

    printf("Platform: %s\n", BUILDSTRING);
	printf("Architecture: %s\n", CPUSTRING);

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
