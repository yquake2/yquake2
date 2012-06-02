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
 * This is the "heart" of the id Tech 2 refresh engine. This file
 * implements the main window in which Quake II is running. The window
 * itself is created by the SDL backend, but here the refresh module is
 * loaded, initialized and it's interaction with the operating system
 * implemented. This code is also the interconnect between the input
 * system (the mouse) and the keyboard system, both are here tied
 * together with the refresher. The direct interaction between the
 * refresher and those subsystems are the main cause for the very
 * acurate and precise input controls of the id Tech 2.
 *
 * =======================================================================
 */

#include <assert.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "../client/header/client.h"
#include "header/unix.h"

/* Structure containing functions exported from refresh DLL */
refexport_t re;

/* Console variables that we need to access from this module */
cvar_t      *vid_gamma;
cvar_t      *vid_ref;           /* Name of Refresh DLL loaded */
cvar_t      *vid_xpos;          /* X coordinate of window position */
cvar_t      *vid_ypos;          /* Y coordinate of window position */
cvar_t      *vid_fullscreen;

/* Global variables used internally by this module */
viddef_t viddef;                /* global video state; used by other modules */
void        *reflib_library;    /* Handle to refresh DLL */
qboolean reflib_active = 0;

#define VID_NUM_MODES ( sizeof ( vid_modes ) / sizeof ( vid_modes [ 0 ] ) )

/* INPUT */
void Do_Key_Event ( int key, qboolean down );
void ( *IN_Update_fp )( void );
void ( *IN_KeyboardInit_fp )( Key_Event_fp_t fp );
void ( *IN_Close_fp )( void );

in_state_t in_state;

void ( *IN_BackendInit_fp )( in_state_t *in_state_p );
void ( *IN_BackendShutdown_fp )( void );
void ( *IN_BackendMouseButtons_fp )( void );
void ( *IN_BackendMove_fp )( usercmd_t *cmd );

extern void VID_MenuShutdown ( void );

/* DLL GLUE */

#define MAXPRINTMSG 4096

void
VID_Printf ( int print_level, char *fmt, ... )
{
	va_list argptr;
	char msg [ MAXPRINTMSG ];

	va_start( argptr, fmt );
	vsnprintf( msg, MAXPRINTMSG, fmt, argptr );
	va_end( argptr );

	if ( print_level == PRINT_ALL )
	{
		Com_Printf( "%s", msg );
	}
	else
	{
		Com_DPrintf( "%s", msg );
	}
}

void
VID_Error ( int err_level, char *fmt, ... )
{
	va_list argptr;
	char msg [ MAXPRINTMSG ];

	va_start( argptr, fmt );
	vsnprintf( msg, MAXPRINTMSG, fmt, argptr );
	va_end( argptr );

	Com_Error( err_level, "%s", msg );
}

/*
 * Console command to re-start the video mode and refresh DLL. We do this
 * simply by setting the modified flag for the vid_ref variable, which will
 * cause the entire video mode and refresh DLL to be reset on the next frame.
 */
void
VID_Restart_f ( void )
{
	vid_ref->modified = true;
}

typedef struct vidmode_s
{
	const char *description;
	int width, height;
	int mode;
} vidmode_t;

/* This must be the same as in menu.c! */
vidmode_t vid_modes[] = {
	{ "Mode 0: 320x240",   320, 240,   0 },
	{ "Mode 1: 400x300",   400, 300,   1 },
	{ "Mode 2: 512x384",   512, 384,   2 },
	{ "Mode 3: 640x400",   640, 400,   3 },
	{ "Mode 4: 640x480",   640, 480,   4 },
	{ "Mode 5: 800x500",   800, 500,   5 },
	{ "Mode 6: 800x600",   800, 600,   6 },
	{ "Mode 7: 960x720",   960, 720,   7 },
	{ "Mode 8: 1024x480",  1024,  480, 8 },
	{ "Mode 9: 1024x640",  1024,  640, 9 },
	{ "Mode 10: 1024x768", 1024, 768,  10 },
	{ "Mode 11: 1152x768", 1152,  768, 11 },
	{ "Mode 12: 1152x864", 1152, 864,  12 },
	{ "Mode 13: 1280x800", 1280,  800, 13 },
	{ "Mode 14: 1280x854", 1280,  854, 14 },
	{ "Mode 15: 1280x960", 1280,  860, 15 },
	{ "Mode 16: 1280x1024", 1280, 1024, 16 },
	{ "Mode 17: 1440x900",	1440, 900,	17 },
	{ "Mode 18: 1600x1200", 1600, 1200, 18 },
	{ "Mode 19: 1680x1050", 1680, 1050, 19 },
	{ "Mode 20: 1920x1080",	1920, 1080, 20 },
	{ "Mode 21: 1920x1200", 1920, 1200, 21 },
	{ "Mode 22: 2048x1536", 2048, 1536, 22 },
};

qboolean
VID_GetModeInfo ( int *width, int *height, int mode )
{
	if ( ( mode < 0 ) || ( mode >= VID_NUM_MODES ) )
	{
		return ( false );
	}

	*width  = vid_modes [ mode ].width;
	*height = vid_modes [ mode ].height;

	return ( true );
}

void
VID_NewWindow ( int width, int height )
{
	viddef.width  = width;
	viddef.height = height;
}

void
VID_FreeReflib ( void )
{
	if ( reflib_library )
	{
		if ( IN_Close_fp )
		{
			IN_Close_fp();
		}

		if ( IN_BackendShutdown_fp )
		{
			IN_BackendShutdown_fp();
		}

		dlclose( reflib_library );
	}

	IN_KeyboardInit_fp = NULL;
	IN_Update_fp = NULL;
	IN_Close_fp = NULL;
	IN_BackendInit_fp = NULL;
	IN_BackendShutdown_fp = NULL;
	IN_BackendMouseButtons_fp = NULL;
	IN_BackendMove_fp = NULL;

	memset( &re, 0, sizeof ( re ) );
	reflib_library = NULL;
	reflib_active  = false;
}

qboolean
VID_LoadRefresh ( char *name )
{
	refimport_t ri;
	R_GetRefAPI_t R_GetRefAPI;
	char fn [ MAX_OSPATH ];
	char    *path;
	struct stat st;
	extern uid_t saved_euid;

	if ( reflib_active )
	{
		if ( IN_Close_fp )
		{
			IN_Close_fp();
		}

		if ( IN_BackendShutdown_fp )
		{
			IN_BackendShutdown_fp();
		}

		IN_Close_fp = NULL;
		IN_BackendShutdown_fp = NULL;
		re.Shutdown();
		VID_FreeReflib();
	}

	Com_Printf( "----- refresher initialization -----\n");

	/* regain root */
	seteuid( saved_euid );

	path = Cvar_Get( "basedir", ".", CVAR_NOSET )->string;

	snprintf( fn, MAX_OSPATH, "%s/%s", path, name );

	if ( stat( fn, &st ) == -1 )
	{
		Com_Printf( "LoadLibrary(\"%s\") failed: %s\n", name, strerror( errno ) );
		return ( false );
	}

	if ( ( reflib_library = dlopen( fn, RTLD_LAZY ) ) == 0 )
	{
		Com_Printf( "LoadLibrary(\"%s\") failed: %s\n", name, dlerror() );
		return ( false );
	}

	Com_Printf( "LoadLibrary(\"%s\")\n", fn );

	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ExecuteText = Cbuf_ExecuteText;
	ri.Con_Printf = VID_Printf;
	ri.Sys_Error = VID_Error;
	ri.Sys_Mkdir = Sys_Mkdir;
	ri.FS_LoadFile = FS_LoadFile;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_Gamedir = FS_Gamedir;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_SetValue = Cvar_SetValue;
	ri.Vid_GetModeInfo = VID_GetModeInfo;
	ri.Vid_MenuInit = VID_MenuInit;
	ri.Vid_NewWindow = VID_NewWindow;

	if ( ( R_GetRefAPI = (void *) dlsym( reflib_library, "R_GetRefAPI" ) ) == 0 )
	{
		Com_Error( ERR_FATAL, "dlsym failed on %s", name );
	}

	re = R_GetRefAPI( ri );

	if ( re.api_version != API_VERSION )
	{
		VID_FreeReflib();
		Com_Error( ERR_FATAL, "%s has incompatible api_version", name );
	}

	/* Init IN (Mouse) */
	in_state.IN_CenterView_fp = IN_CenterView;
	in_state.Key_Event_fp = Do_Key_Event;
	in_state.viewangles = cl.viewangles;
	in_state.in_strafe_state = &in_strafe.state;
	in_state.in_speed_state = &in_speed.state;

	if ( ( ( IN_BackendInit_fp = dlsym( reflib_library, "IN_BackendInit" ) ) == NULL ) ||
		 ( ( IN_BackendShutdown_fp = dlsym( reflib_library, "IN_BackendShutdown" ) ) == NULL ) ||
		 ( ( IN_BackendMouseButtons_fp = dlsym( reflib_library, "IN_BackendMouseButtons" ) ) == NULL ) ||
		 ( ( IN_BackendMove_fp = dlsym( reflib_library, "IN_BackendMove" ) ) == NULL ) )
	{
		Com_Error( ERR_FATAL, "No input backend init functions in REF.\n" );
	}

	if ( IN_BackendInit_fp )
	{
		IN_BackendInit_fp( &in_state );
	}

	if ( re.Init( 0, 0 ) == -1 )
	{
		re.Shutdown();
		VID_FreeReflib();
		return ( false );
	}

	/* Init IN */
	if ( ( ( IN_KeyboardInit_fp = dlsym( reflib_library, "IN_KeyboardInit" ) ) == NULL ) ||
		 ( ( IN_Update_fp = dlsym( reflib_library, "IN_Update" ) ) == NULL ) ||
		 ( ( IN_Close_fp = dlsym( reflib_library, "IN_Close" ) ) == NULL ) )
	{
		Com_Error( ERR_FATAL, "No keyboard input functions in REF.\n" );
	}

	IN_KeyboardInit_fp( Do_Key_Event );
	Key_ClearStates();

	/* give up root now */
	setreuid( getuid(), getuid() );
	setegid( getgid() );

	Com_Printf( "------------------------------------\n\n" );
	reflib_active = true;
	return ( true );
}

/*
 * This function gets called once just before drawing each frame, and
 * it's sole purpose in life is to check to see if any of the video mode
 * parameters have changed, and if they have to update the rendering DLL
 * and/or video mode to match.
 */
void
VID_CheckChanges ( void )
{
	char name [ 100 ];

	if ( vid_ref->modified )
	{
		S_StopAllSounds();
	}

	while ( vid_ref->modified )
	{
		/* refresh has changed */
		vid_ref->modified = false;
		vid_fullscreen->modified = true;
		cl.refresh_prepped = false;
		cls.disable_screen = true;

		sprintf( name, "ref_%s.so", vid_ref->string );

		if ( !VID_LoadRefresh( name ) )
		{
			Cvar_Set( "vid_ref", "gl" );
		}

		cls.disable_screen = false;
	}
}


void
VID_Init ( void )
{
	/* Create the video variables so we know how to start the graphics drivers */
	vid_ref = Cvar_Get( "vid_ref", "gl", CVAR_ARCHIVE );

	vid_xpos = Cvar_Get( "vid_xpos", "3", CVAR_ARCHIVE );
	vid_ypos = Cvar_Get( "vid_ypos", "22", CVAR_ARCHIVE );
	vid_fullscreen = Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE );
	vid_gamma = Cvar_Get( "vid_gamma", "1", CVAR_ARCHIVE );

	/* Add some console commands that we want to handle */
	Cmd_AddCommand( "vid_restart", VID_Restart_f );

	/* Start the graphics mode and load refresh DLL */
	VID_CheckChanges();
}

void
VID_Shutdown ( void )
{
	if ( reflib_active )
	{
		if ( IN_Close_fp )
		{
			IN_Close_fp();
		}

		if ( IN_BackendShutdown_fp )
		{
			IN_BackendShutdown_fp();
		}

		IN_Close_fp = NULL;
		IN_BackendShutdown_fp = NULL;
		re.Shutdown();
		VID_FreeReflib();
	}

	VID_MenuShutdown();
}

/*
 * Checks to see if the given ref_NAME.so exists.
 * Placed here to avoid complicating other code if the library .so files
 * ever have their names changed.
 */
qboolean
VID_CheckRefExists ( const char *ref )
{
	char fn [ MAX_OSPATH ];
	char    *path;
	struct stat st;

	path = Cvar_Get( "basedir", ".", CVAR_NOSET )->string;
	snprintf( fn, MAX_OSPATH, "%s/ref_%s.so", path, ref );

	if ( stat( fn, &st ) == 0 )
	{
		return ( true );
	}
	else
	{
		return ( false );
	}
}

/* INPUT */
void
IN_Shutdown ( void )
{
	if ( IN_BackendShutdown_fp )
	{
		IN_BackendShutdown_fp();
	}
}

void
IN_Commands ( void )
{
	if ( IN_BackendMouseButtons_fp )
	{
		IN_BackendMouseButtons_fp();
	}
}

void
IN_Move ( usercmd_t *cmd )
{
	if ( IN_BackendMove_fp )
	{
		IN_BackendMove_fp( cmd );
	}
}

void
Do_Key_Event ( int key, qboolean down )
{
	Key_Event( key, down, Sys_Milliseconds() );
}
