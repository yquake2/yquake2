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
 * Platform independent initialization, main loop and frame handling.
 *
 * =======================================================================
 */

#include "header/common.h"
#include "header/zone.h"
#include <setjmp.h>

cvar_t *developer;
cvar_t *modder;
cvar_t *timescale;
cvar_t *fixedtime;
cvar_t *cl_maxfps;
cvar_t *dedicated;

extern cvar_t *logfile_active;
extern jmp_buf abortframe; /* an ERR_DROP occured, exit the entire frame */
extern zhead_t z_chain;

#ifndef DEDICATED_ONLY
FILE *log_stats_file;
cvar_t *busywait;
cvar_t *cl_async;
cvar_t *cl_timedemo;
cvar_t *vid_maxfps;
cvar_t *host_speeds;
cvar_t *log_stats;
cvar_t *showtrace;
#endif

// Forward declarations
#ifndef DEDICATED_ONLY
int GLimp_GetRefreshRate(void);
qboolean R_IsVSyncActive(void);
#endif

/* host_speeds times */
int time_before_game;
int time_after_game;
int time_before_ref;
int time_after_ref;

// Used in the network- and input pathes.
int curtime;

#ifndef DEDICATED_ONLY
void Key_Init(void);
void SCR_EndLoadingPlaque(void);
#endif

// Is the game portable?
qboolean is_portable;

// Game given by user
char userGivenGame[MAX_QPATH];

// Game should quit next frame.
// Hack for the signal handlers.
qboolean quitnextframe;

// ----

static void
Qcommon_Buildstring(void)
{
	int i;
	int verLen;
	const char* versionString;


	versionString = va("Yamagi Quake II v%s", YQ2VERSION);
	verLen = strlen(versionString);

	printf("\n%s\n", versionString);

	for( i = 0; i < verLen; ++i)
	{
		printf("=");
	}

	printf("\n");


#ifndef DEDICATED_ONLY
	printf("Client build options:\n");

#ifdef USE_CURL
	printf(" + cURL HTTP downloads\n");
#else
	printf(" - cURL HTTP downloads\n");
#endif

#ifdef USE_OPENAL
	printf(" + OpenAL audio\n");
#else
	printf(" - OpenAL audio\n");
#endif

#ifdef SYSTEMWIDE
	printf(" + Systemwide installation\n");
#else
	printf(" - Systemwide installation\n");
#endif
#endif

	printf("Platform: %s\n", YQ2OSTYPE);
	printf("Architecture: %s\n", YQ2ARCH);
}

void
Qcommon_Mainloop(void)
{
	long long newtime;
	long long oldtime = Sys_Microseconds();

	/* The mainloop. The legend. */
	while (1)
	{
#ifndef DEDICATED_ONLY
		// Throttle the game a little bit.
		if (busywait->value)
		{
			long long spintime = Sys_Microseconds();

			while (1)
			{
				/* Give the CPU a hint that this is a very tight
				   spinloop. One PAUSE instruction each loop is
				   enough to reduce power consumption and head
				   dispersion a lot, it's 95°C against 67°C on
				   a Kaby Lake laptop. */
#if defined (__GNUC__) && (__i386 || __x86_64__)
				asm("pause");
#elif defined(__aarch64__) || (defined(__ARM_ARCH) && __ARM_ARCH >= 7) || defined(__ARM_ARCH_6K__)
				asm("yield");
#endif

				if (Sys_Microseconds() - spintime >= 5)
				{
					break;
				}
			}
		}
		else
		{
			Sys_Nanosleep(5000);
		}
#else
		Sys_Nanosleep(850000);
#endif

		newtime = Sys_Microseconds();
		Qcommon_Frame(newtime - oldtime);
		oldtime = newtime;
	}
}

void Qcommon_ExecConfigs(qboolean gameStartUp)
{
	Cbuf_AddText("exec default.cfg\n");
	Cbuf_AddText("exec yq2.cfg\n");
	Cbuf_AddText("exec config.cfg\n");
	Cbuf_AddText("exec autoexec.cfg\n");

	if (gameStartUp)
	{
		/* Process cmd arguments only startup. */
		Cbuf_AddEarlyCommands(true);
	}

	Cbuf_Execute();
}

static qboolean checkForHelp(int argc, char **argv)
{
	const char* helpArgs[] = { "--help", "-h", "-help", "-?", "/?" };
	const int numHelpArgs = sizeof(helpArgs)/sizeof(helpArgs[0]);

	for (int i=1; i<argc; ++i)
	{
		const char* arg = argv[i];

		for (int h=0; h<numHelpArgs; ++h)
		{
			if (Q_stricmp(arg, helpArgs[h]) == 0)
			{
				printf("Yamagi Quake II v%s\n", YQ2VERSION);
				printf("Most interesting commandline arguments:\n");
				printf("-h or --help: Show this help\n");
				printf("-cfgdir <path>\n");
				printf("  set the name of your config directory\n");
				printf("-datadir <path>\n");
				printf("  set path to your Quake2 game data (the directory baseq2/ is in)\n");
				printf("-portable\n");
				printf("  Write (savegames, configs, ...) in the binary directory\n");
				printf("+exec <config>\n");
				printf("  execute the given config (mainly relevant for dedicated servers)\n");
				printf("+set <cvarname> <value>\n");
				printf("  Set the given cvar to the given value, e.g. +set vid_fullscreen 0\n");

				printf("\nSome interesting cvars:\n");
				printf("+set game <gamename>\n");
				printf("  start the given addon/mod, e.g. +set game xatrix\n");
#ifndef DEDICATED_ONLY
				printf("+set vid_fullscreen <0 or 1>\n");
				printf("  start game in windowed (0) or desktop fullscreen (1)\n");
				printf("  or classic fullscreen (2) mode\n");
				printf("+set r_mode <modenumber>\n");
				printf("  start game in resolution belonging to <modenumber>,\n");
				printf("  use -1 for custom resolutions:\n");
				printf("+set r_customwidth <size in pixels>\n");
				printf("+set r_customheight <size in pixels>\n");
				printf("  if r_mode is set to -1, these cvars allow you to specify the\n");
				printf("  width/height of your custom resolution\n");
				printf("+set vid_renderer <renderer>\n");
				printf("  Selects the render backend. Currently available:\n");
				printf("    'gl1'  (the OpenGL 1.x renderer),\n");
				printf("    'gl3'  (the OpenGL 3.2 renderer),\n");
				printf("    'soft' (the software renderer)\n");
#ifdef USE_REFVK
				printf("    'vulkan' (the experimental Vulkan renderer)\n");
#endif
#endif // DEDICATED_ONLY
				printf("\nSee https://github.com/yquake2/yquake2/blob/master/doc/04_cvarlist.md\nfor some more cvars\n");

				return true;
			}
		}
	}

	return false;
}

void
Qcommon_Init(int argc, char **argv)
{
	// Jump point used in emergency situations.
	if (setjmp(abortframe))
	{
		Sys_Error("Error during initialization");
	}

	if (checkForHelp(argc, argv))
	{
		// ok, --help or similar commandline option was given
		// and info was printed, exit the game now
		exit(1);
	}

	// Print the build and version string
	Qcommon_Buildstring();

	// Seed PRNG
	randk_seed();

	// Initialize zone malloc().
	z_chain.next = z_chain.prev = &z_chain;

	// Start early subsystems.
	COM_InitArgv(argc, argv);
	Swap_Init();
	Cbuf_Init();
	Cmd_Init();
	Cvar_Init();

#ifndef DEDICATED_ONLY
	Key_Init();
#endif

	/* we need to add the early commands twice, because
	   a basedir or cddir needs to be set before execing
	   config files, but we want other parms to override
	   the settings of the config files */
	Cbuf_AddEarlyCommands(false);
	Cbuf_Execute();

	// remember the initial game name that might have been set on commandline
	{
		cvar_t* gameCvar = Cvar_Get("game", "", CVAR_LATCH | CVAR_SERVERINFO);
		const char* game = "";

		if(gameCvar->string && gameCvar->string[0])
		{
			game = gameCvar->string;
		}

		Q_strlcpy(userGivenGame, game, sizeof(userGivenGame));
	}

	// The filesystems needs to be initialized after the cvars.
	FS_InitFilesystem();

	// Add and execute configuration files.
	Qcommon_ExecConfigs(true);

	// Zone malloc statistics.
	Cmd_AddCommand("z_stats", Z_Stats_f);

	// cvars

	cl_maxfps = Cvar_Get("cl_maxfps", "60", CVAR_ARCHIVE);

	developer = Cvar_Get("developer", "0", 0);
	fixedtime = Cvar_Get("fixedtime", "0", 0);

	logfile_active = Cvar_Get("logfile", "1", CVAR_ARCHIVE);
	modder = Cvar_Get("modder", "0", 0);
	timescale = Cvar_Get("timescale", "1", 0);

	char *s;
	s = va("%s %s %s %s", YQ2VERSION, YQ2ARCH, BUILD_DATE, YQ2OSTYPE);
	Cvar_Get("version", s, CVAR_SERVERINFO | CVAR_NOSET);

#ifndef DEDICATED_ONLY
	busywait = Cvar_Get("busywait", "1", CVAR_ARCHIVE);
	cl_async = Cvar_Get("cl_async", "1", CVAR_ARCHIVE);
	cl_timedemo = Cvar_Get("timedemo", "0", 0);
	dedicated = Cvar_Get("dedicated", "0", CVAR_NOSET);
	vid_maxfps = Cvar_Get("vid_maxfps", "300", CVAR_ARCHIVE);
	host_speeds = Cvar_Get("host_speeds", "0", 0);
	log_stats = Cvar_Get("log_stats", "0", 0);
	showtrace = Cvar_Get("showtrace", "0", 0);
#else
	dedicated = Cvar_Get("dedicated", "1", CVAR_NOSET);
#endif

	// We can't use the clients "quit" command when running dedicated.
	if (dedicated->value)
	{
		Cmd_AddCommand("quit", Com_Quit);
	}

	// Start late subsystem.
	Sys_Init();
	NET_Init();
	Netchan_Init();
	SV_Init();
#ifndef DEDICATED_ONLY
	CL_Init();
#endif

	// Everythings up, let's add + cmds from command line.
	if (!Cbuf_AddLateCommands())
	{
		if (!dedicated->value)
		{
			// Start demo loop...
			Cbuf_AddText("d1\n");
		}
		else
		{
			// ...or dedicated server.
			Cbuf_AddText("dedicated_start\n");
		}

		Cbuf_Execute();
	}
#ifndef DEDICATED_ONLY
	else
	{
		/* the user asked for something explicit
		   so drop the loading plaque */
		SCR_EndLoadingPlaque();
	}
#endif

	Com_Printf("==== Yamagi Quake II Initialized ====\n\n");
	Com_Printf("*************************************\n\n");

	// Call the main loop
	Qcommon_Mainloop();
}

#ifndef DEDICATED_ONLY
void
Qcommon_Frame(int usec)
{
	// Used for the dedicated server console.
	char *s;

	// Statistics.
	int time_before = 0;
	int time_between = 0;
	int time_after;

	// Target packetframerate.
	int pfps;

	//Target renderframerate.
	int rfps;

	// Time since last packetframe in microsec.
	static int packetdelta = 1000000;

	// Time since last renderframe in microsec.
	static int renderdelta = 1000000;

	// Accumulated time since last client run.
	static int clienttimedelta = 0;

	// Accumulated time since last server run.
	static int servertimedelta = 0;

	/* A packetframe runs the server and the client,
	   but not the renderer. The minimal interval of
	   packetframes is about 10.000 microsec. If run
	   more often the movement prediction in pmove.c
	   breaks. That's the Q2 variant if the famous
	   125hz bug. */
	qboolean packetframe = true;

	/* A rendererframe runs the renderer, but not the
	   client or the server. The minimal interval is
	   about 1000 microseconds. */
	qboolean renderframe = true;


	/* Tells the client to shutdown.
	   Used by the signal handlers. */
	if (quitnextframe)
	{
		Cbuf_AddText("quit");
	}


	/* In case of ERR_DROP we're jumping here. Don't know
	   if that's really save but it seems to work. So leave
	   it alone. */
	if (setjmp(abortframe))
	{
		return;
	}


	if (log_stats->modified)
	{
		log_stats->modified = false;

		if (log_stats->value)
		{
			if (log_stats_file)
			{
				fclose(log_stats_file);
				log_stats_file = 0;
			}

			log_stats_file = Q_fopen("stats.log", "w");

			if (log_stats_file)
			{
				fprintf(log_stats_file, "entities,dlights,parts,frame time\n");
			}
		}
		else
		{
			if (log_stats_file)
			{
				fclose(log_stats_file);
				log_stats_file = 0;
			}
		}
	}


	// Timing debug crap. Just for historical reasons.
	if (fixedtime->value)
	{
		usec = (int)fixedtime->value;
	}
	else if (timescale->value)
	{
		usec *= timescale->value;
	}


	if (showtrace->value)
	{
		extern int c_traces, c_brush_traces;
		extern int c_pointcontents;

		Com_Printf("%4i traces  %4i points\n", c_traces, c_pointcontents);
		c_traces = 0;
		c_brush_traces = 0;
		c_pointcontents = 0;
	}


	/* We can render 1000 frames at maximum, because the minimum
	   frametime of the client is 1 millisecond. And of course we
	   need to render something, the framerate can never be less
	   then 1. Cap vid_maxfps between 1 and 999. */
	if (vid_maxfps->value > 999 || vid_maxfps->value < 1)
	{
		Cvar_SetValue("vid_maxfps", 999);
	}

	if (cl_maxfps->value > 250)
	{
		Cvar_SetValue("cl_maxfps", 250);
	}
	else if (cl_maxfps->value < 1)
	{
		Cvar_SetValue("cl_maxfps", 60);
	}


	// Save global time for network- und input code.
	curtime = Sys_Milliseconds();


	// Calculate target and renderframerate.
	if (R_IsVSyncActive())
	{
		rfps = GLimp_GetRefreshRate();

		if (rfps > vid_maxfps->value)
		{
			rfps = (int)vid_maxfps->value;
		}
	}
	else
	{
		rfps = (int)vid_maxfps->value;
	}

	/* The target render frame rate may be too high. The current
	   scene may be more complex then the previous one and SDL
	   may give us a 1 or 2 frames too low display refresh rate.
	   Add a security magin of 5%, e.g. 60fps * 0.95 = 57fps. */
	pfps = (cl_maxfps->value > (rfps * 0.95)) ? floor(rfps * 0.95) : cl_maxfps->value;


	// Calculate timings.
	packetdelta += usec;
	renderdelta += usec;
	clienttimedelta += usec;
	servertimedelta += usec;

	if (!cl_timedemo->value)
	{
		if (cl_async->value)
		{
			if (R_IsVSyncActive())
			{
				// Netwwork frames.
				if (packetdelta < (0.8 * (1000000.0f / pfps)))
				{
					packetframe = false;
				}

				// Render frames.
				if (renderdelta < (0.8 * (1000000.0f / rfps)))
				{
					renderframe = false;
				}
			}
			else
			{
				// Network frames.
				if (packetdelta < (1000000.0f / pfps))
				{
					packetframe = false;
				}

				// Render frames.
				if (renderdelta < (1000000.0f ) / rfps)
				{
					renderframe = false;
				}
			}
		}
		else
		{
			// Cap frames at target framerate.
			if (renderdelta < (1000000.0f / rfps)) {
				renderframe = false;
				packetframe = false;
			}
		}
	}
	else if (clienttimedelta < 1000 || servertimedelta < 1000)
	{
		return;
	}


	// Dedicated server terminal console.
	do {
		s = Sys_ConsoleInput();

		if (s) {
			Cbuf_AddText(va("%s\n", s));
		}
	} while (s);

	Cbuf_Execute();


	if (host_speeds->value)
	{
		time_before = Sys_Milliseconds();
	}


	// Run the serverframe.
	if (packetframe) {
		SV_Frame(servertimedelta);
		servertimedelta = 0;
	}


	if (host_speeds->value)
	{
		time_between = Sys_Milliseconds();
	}


	// Run the client frame.
	if (packetframe || renderframe) {
		CL_Frame(packetdelta, renderdelta, clienttimedelta, packetframe, renderframe);
		clienttimedelta = 0;
	}


	if (host_speeds->value)
	{
		int all, sv, gm, cl, rf;

		time_after = Sys_Milliseconds();
		all = time_after - time_before;
		sv = time_between - time_before;
		cl = time_after - time_between;
		gm = time_after_game - time_before_game;
		rf = time_after_ref - time_before_ref;
		sv -= gm;
		cl -= rf;
		Com_Printf("all:%3i sv:%3i gm:%3i cl:%3i rf:%3i\n", all, sv, gm, cl, rf);
	}


	// Reset deltas and mark frame.
	if (packetframe) {
		packetdelta = 0;
	}

	if (renderframe) {
		renderdelta = 0;
	}
}
#else
void
Qcommon_Frame(int usec)
{
	// For the dedicated server terminal console.
	char *s;

	// Target packetframerate.
	int pfps;

	// Time since last packetframe in microsec.
	static int packetdelta = 1000000;

	// Accumulated time since last server run.
	static int servertimedelta = 0;

	/* A packetframe runs the server and the client,
	   but not the renderer. The minimal interval of
	   packetframes is about 10.000 microsec. If run
	   more often the movement prediction in pmove.c
	   breaks. That's the Q2 variant if the famous
	   125hz bug. */
	qboolean packetframe = true;


	/* Tells the client to shutdown.
	   Used by the signal handlers. */
	if (quitnextframe)
	{
		Cbuf_AddText("quit");
	}


	/* In case of ERR_DROP we're jumping here. Don't know
	   if that' really save but it seems to work. So leave
	   it alone. */
	if (setjmp(abortframe))
	{
		return;
	}


	// Timing debug crap. Just for historical reasons.
	if (fixedtime->value)
	{
		usec = (int)fixedtime->value;
	}
	else if (timescale->value)
	{
		usec *= timescale->value;
	}


	// Save global time for network- und input code.
	curtime = Sys_Milliseconds();


	// Target framerate.
	pfps = (int)cl_maxfps->value;


	// Calculate timings.
	packetdelta += usec;
	servertimedelta += usec;


	// Network frame time.
	if (packetdelta < (1000000.0f / pfps)) {
		packetframe = false;
	}


	// Dedicated server terminal console.
	do {
		s = Sys_ConsoleInput();

		if (s) {
			Cbuf_AddText(va("%s\n", s));
		}
	} while (s);

	Cbuf_Execute();


	// Run the serverframe.
	if (packetframe) {
		SV_Frame(servertimedelta);
		servertimedelta = 0;

		// Reset deltas if necessary.
		packetdelta = 0;
	}
}
#endif

void
Qcommon_Shutdown(void)
{
	Cvar_Fini();
}
