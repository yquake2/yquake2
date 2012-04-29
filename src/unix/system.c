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
 * This file implements all system dependend generic funktions
 *
 * =======================================================================
 */

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include <dlfcn.h>
#include <dirent.h>

#include "header/glob.h"
#include "../common/header/common.h"
#include "header/unix.h"

unsigned sys_frame_time;
int curtime;
static void *game_library;

static char findbase [ MAX_OSPATH ];
static char findpath [ MAX_OSPATH ];
static char findpattern [ MAX_OSPATH ];
static DIR  *fdir;

qboolean stdin_active = true;
extern cvar_t *nostdout;
extern FILE	*logfile;

static qboolean
CompareAttributes ( char *path, char *name, unsigned musthave, unsigned canthave )
{
	struct stat st;
	char fn [ MAX_OSPATH ];

	/* . and .. never match */
	if ( ( strcmp( name, "." ) == 0 ) || ( strcmp( name, ".." ) == 0 ) )
	{
		return ( false );
	}

	return ( true );

	if ( stat( fn, &st ) == -1 )
	{
		return ( false ); /* shouldn't happen */
	}

	if ( ( st.st_mode & S_IFDIR ) && ( canthave & SFF_SUBDIR ) )
	{
		return ( false );
	}

	if ( ( musthave & SFF_SUBDIR ) && !( st.st_mode & S_IFDIR ) )
	{
		return ( false );
	}

	return ( true );
}

int
Sys_Milliseconds ( void )
{
	struct timeval tp;
	struct timezone tzp;
	static int secbase;

	gettimeofday( &tp, &tzp );

	if ( !secbase )
	{
		secbase = tp.tv_sec;
		return ( tp.tv_usec / 1000 );
	}

	curtime = ( tp.tv_sec - secbase ) * 1000 + tp.tv_usec / 1000;

	return ( curtime );
}

void
Sys_Mkdir ( char *path )
{
	mkdir( path, 0755 );
}

void
Sys_Rmdir ( char *path )
{
	rmdir( path );
}

char *
Sys_GetCurrentDirectory ( void )
{
	static char dir [ MAX_OSPATH ];

	if ( !getcwd( dir, sizeof ( dir ) ) )
	{
		Sys_Error( "Couldn't get current working directory" );
	}

	return ( dir );
}

char *
Sys_FindFirst ( char *path, unsigned musthave, unsigned canhave )
{
	struct dirent *d;
	char *p;

	if ( fdir )
	{
		Sys_Error( "Sys_BeginFind without close" );
	}

	strcpy( findbase, path );

	if ( ( p = strrchr( findbase, '/' ) ) != NULL )
	{
		*p = 0;
		strcpy( findpattern, p + 1 );
	}
	else
	{
		strcpy( findpattern, "*" );
	}

	if ( strcmp( findpattern, "*.*" ) == 0 )
	{
		strcpy( findpattern, "*" );
	}

	if ( ( fdir = opendir( findbase ) ) == NULL )
	{
		return ( NULL );
	}

	while ( ( d = readdir( fdir ) ) != NULL )
	{
		if ( !*findpattern || glob_match( findpattern, d->d_name ) )
		{
			if ( CompareAttributes( findbase, d->d_name, musthave, canhave ) )
			{
				sprintf( findpath, "%s/%s", findbase, d->d_name );
				return ( findpath );
			}
		}
	}

	return ( NULL );
}

char *
Sys_FindNext ( unsigned musthave, unsigned canhave )
{
	struct dirent *d;

	if ( fdir == NULL )
	{
		return ( NULL );
	}

	while ( ( d = readdir( fdir ) ) != NULL )
	{
		if ( !*findpattern || glob_match( findpattern, d->d_name ) )
		{
			if ( CompareAttributes( findbase, d->d_name, musthave, canhave ) )
			{
				sprintf( findpath, "%s/%s", findbase, d->d_name );
				return ( findpath );
			}
		}
	}

	return ( NULL );
}

void
Sys_FindClose ( void )
{
	if ( fdir != NULL )
	{
		closedir( fdir );
	}

	fdir = NULL;
}

void
Sys_ConsoleOutput ( char *string )
{
	if ( nostdout && nostdout->value )
	{
		return;
	}

	fputs( string, stdout );
}

void
Sys_Printf ( char *fmt, ... )
{
	va_list argptr;
	char text [ 1024 ];
	unsigned char   *p;

	va_start( argptr, fmt );
	vsnprintf( text, 1024, fmt, argptr );
	va_end( argptr );

	if ( nostdout && nostdout->value )
	{
		return;
	}

	for ( p = (unsigned char *) text; *p; p++ )
	{
		*p &= 0x7f;

		if ( ( ( *p > 128 ) || ( *p < 32 ) ) && ( *p != 10 ) && ( *p != 13 ) && ( *p != 9 ) )
		{
			printf( "[%02x]", *p );
		}
		else
		{
			putc( *p, stdout );
		}
	}
}

void
Sys_Quit ( void )
{
#ifndef DEDICATED_ONLY
	CL_Shutdown();
#endif

   	if (logfile)
	{
		fclose (logfile);
		logfile = NULL;
	}

	Qcommon_Shutdown();
	fcntl( 0, F_SETFL, fcntl( 0, F_GETFL, 0 ) & ~FNDELAY );

	printf("------------------------------------\n");

	exit( 0 );
}

void
Sys_Error ( char *error, ... )
{
	va_list argptr;
	char string [ 1024 ];

	/* change stdin to non blocking */
	fcntl( 0, F_SETFL, fcntl( 0, F_GETFL, 0 ) & ~FNDELAY );

#ifndef DEDICATED_ONLY
	CL_Shutdown();
#endif
	Qcommon_Shutdown();

	va_start( argptr, error );
	vsnprintf( string, 1024, error, argptr );
	va_end( argptr );
	fprintf( stderr, "Error: %s\n", string );

	exit( 1 );
}

void
Sys_Warn ( char *warning, ... )
{
	va_list argptr;
	char string [ 1024 ];

	va_start( argptr, warning );
	vsnprintf( string, 1024, warning, argptr );
	va_end( argptr );
	fprintf( stderr, "Warning: %s", string );
}

/*
 * returns -1 if not present
 */
int
Sys_FileTime ( char *path )
{
	struct  stat buf;

	if ( stat( path, &buf ) == -1 )
	{
		return ( -1 );
	}

	return ( buf.st_mtime );
}

void
floating_point_exception_handler ( int whatever )
{
	signal( SIGFPE, floating_point_exception_handler );
}

char *
Sys_ConsoleInput ( void )
{
	static char text [ 256 ];
	int len;
	fd_set fdset;
	struct timeval timeout;

	if ( !dedicated || !dedicated->value )
	{
		return ( NULL );
	}

	if ( !stdin_active )
	{
		return ( NULL );
	}

	FD_ZERO( &fdset );
	FD_SET( 0, &fdset ); /* stdin */
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	if ( ( select( 1, &fdset, NULL, NULL, &timeout ) == -1 ) || !FD_ISSET( 0, &fdset ) )
	{
		return ( NULL );
	}

	len = read( 0, text, sizeof ( text ) );

	if ( len == 0 ) /* eof! */
	{
		stdin_active = false;
		return ( NULL );
	}

	if ( len < 1 )
	{
		return ( NULL );
	}

	text [ len - 1 ] = 0; /* rip off the /n and terminate */

	return ( text );
}

void
Sys_UnloadGame ( void )
{
	if ( game_library )
	{
		dlclose( game_library );
	}

	game_library = NULL;
}

/*
 * Loads the game dll
 */
void *
Sys_GetGameAPI ( void *parms )
{
	void    *( *GetGameAPI )(void *);

	FILE    *fp;
	char name [ MAX_OSPATH ];
	char    *path;
	char    *str_p;
	const char *gamename = "game.so";

	setreuid( getuid(), getuid() );
	setegid( getgid() );

	if ( game_library )
	{
		Com_Error( ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame" );
	}

	Com_Printf( "LoadLibrary(\"%s\")\n", gamename );

	/* now run through the search paths */
	path = NULL;

	while ( 1 )
	{
		path = FS_NextPath( path );

		if ( !path )
		{
			return ( NULL ); /* couldn't find one anywhere */
		}

		snprintf( name, MAX_OSPATH, "%s/%s", path, gamename );

		/* skip it if it just doesn't exist */
		fp = fopen( name, "rb" );

		if ( fp == NULL )
		{
			continue;
		}

		fclose( fp );

		game_library = dlopen( name, RTLD_NOW );

		if ( game_library )
		{
			Com_MDPrintf( "LoadLibrary (%s)\n", name );
			break;
		}
		else
		{
			Com_Printf( "LoadLibrary (%s):", name );

			path = (char *) dlerror();
			str_p = strchr( path, ':' ); /* skip the path (already shown) */

			if ( str_p == NULL )
			{
				str_p = path;
			}
			else
			{
				str_p++;
			}

			Com_Printf( "%s\n", str_p );

			return ( NULL );
		}
	}

	GetGameAPI = (void *) dlsym( game_library, "GetGameAPI" );

	if ( !GetGameAPI )
	{
		Sys_UnloadGame();
		return ( NULL );
	}

	return ( GetGameAPI( parms ) );
}

void
Sys_SendKeyEvents ( void )
{
#ifndef DEDICATED_ONLY

	if ( IN_Update_fp )
	{
		IN_Update_fp();
	}

#endif

	/* grab frame time */
	sys_frame_time = Sys_Milliseconds();
}
