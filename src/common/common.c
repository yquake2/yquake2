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
 * Misc. common stuff
 *
 * =======================================================================
 */

#include "qcommon.h"
#include "header/zone.h"
#include <setjmp.h>

FILE	*log_stats_file;
cvar_t	*host_speeds;
cvar_t	*log_stats;
cvar_t	*developer;
cvar_t	*modder;
cvar_t	*timescale;
cvar_t	*fixedtime;
#ifndef DEDICATED_ONLY
cvar_t	*showtrace;
#endif
cvar_t	*dedicated;

extern cvar_t	*logfile_active;
extern jmp_buf  abortframe; /* an ERR_DROP occured, exit the entire frame */
extern zhead_t	z_chain;

/* host_speeds times */
int		time_before_game;
int		time_after_game;
int		time_before_ref;
int		time_after_ref;

/*
 * For proxy protecting
 */
byte	COM_BlockSequenceCRCByte (byte *base, int length, int sequence)
{
	int				n;
	int				x;
	byte			*p;
	byte			chkb[60 + 4];
	unsigned short	crc;
	byte			r;

	if (sequence < 0)
		Sys_Error("sequence < 0, this shouldn't happen\n");

	p = chktbl + (sequence % (sizeof(chktbl) - 4));

	if (length > 60)
		length = 60;

	memcpy (chkb, base, length);

	chkb[length] = p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = p[2];
	chkb[length+3] = p[3];

	length += 4;

	crc = CRC_Block(chkb, length);

	for (x=0, n=0; n<length; n++)
		x += chkb[n];

	r = (crc ^ x) & 0xff;

	return r;
}

float frand(void)
{
	return (rand()&32767)* (1.0/32767);
}

float crand(void)
{
	return (rand()&32767)* (2.0/32767) - 1;
}

#ifndef DEDICATED_ONLY
void Key_Init (void);
void SCR_EndLoadingPlaque (void);
#endif

/*
 * Just throw a fatal error to
 * test error shutdown procedures
 */
void Com_Error_f (void)
{
	Com_Error (ERR_FATAL, "%s", Cmd_Argv(1));
}

void Qcommon_Init (int argc, char **argv)
{
	char	*s;

	if (setjmp (abortframe) )
		Sys_Error ("Error during initialization");

	z_chain.next = z_chain.prev = &z_chain;

	/* prepare enough of the subsystems to handle
	   cvar and command buffer management */
	COM_InitArgv (argc, argv);

	Swap_Init ();
	Cbuf_Init ();

	Cmd_Init ();
	Cvar_Init ();

#ifndef DEDICATED_ONLY
	Key_Init ();
#endif

	/* we need to add the early commands twice, because
	   a basedir or cddir needs to be set before execing
	   config files, but we want other parms to override
	   the settings of the config files */
	Cbuf_AddEarlyCommands (false);
	Cbuf_Execute ();

	FS_InitFilesystem ();

	Cbuf_AddText ("exec default.cfg\n");
	Cbuf_AddText ("exec config.cfg\n");

	Cbuf_AddEarlyCommands (true);
	Cbuf_Execute ();

	/* init commands and vars */
	Cmd_AddCommand ("z_stats", Z_Stats_f);
	Cmd_AddCommand ("error", Com_Error_f);

	host_speeds = Cvar_Get ("host_speeds", "0", 0);
	log_stats = Cvar_Get ("log_stats", "0", 0);
	developer = Cvar_Get ("developer", "0", 0);
	modder = Cvar_Get ("modder", "0", 0);
	timescale = Cvar_Get ("timescale", "1", 0);
	fixedtime = Cvar_Get ("fixedtime", "0", 0);
	logfile_active = Cvar_Get ("logfile", "0", 0);
#ifndef DEDICATED_ONLY
	showtrace = Cvar_Get ("showtrace", "0", 0);
#endif
#ifdef DEDICATED_ONLY
	dedicated = Cvar_Get ("dedicated", "1", CVAR_NOSET);
#else
	dedicated = Cvar_Get ("dedicated", "0", CVAR_NOSET);
#endif

	s = va("%4.2f %s %s %s", VERSION, CPUSTRING, __DATE__, BUILDSTRING);
	Cvar_Get ("version", s, CVAR_SERVERINFO|CVAR_NOSET);

	if (dedicated->value)
		Cmd_AddCommand ("quit", Com_Quit);

	Sys_Init ();

	NET_Init ();
	Netchan_Init ();

	SV_Init ();
#ifndef DEDICATED_ONLY
	CL_Init ();
#endif

	/* add + commands from command line */
	if (!Cbuf_AddLateCommands ())
	{
		/* if the user didn't give any commands, run default action */
		if (!dedicated->value)
			Cbuf_AddText ("d1\n");

		else
			Cbuf_AddText ("dedicated_start\n");

		Cbuf_Execute ();
	}

#ifndef DEDICATED_ONLY

	else
	{
		/* the user asked for something explicit
		   so drop the loading plaque */
		SCR_EndLoadingPlaque ();
	}

#endif

	Com_Printf ("====== Quake2 Initialized ======\n\n");
}

void Qcommon_Frame (int msec)
{
	char	*s;

#ifndef DEDICATED_ONLY
	int		time_before = 0;
	int		time_between = 0;
	int		time_after;
#endif

	if (setjmp (abortframe) )
		return; /* an ERR_DROP was thrown */

	if ( log_stats->modified )
	{
		log_stats->modified = false;

		if ( log_stats->value )
		{
			if ( log_stats_file )
			{
				fclose( log_stats_file );
				log_stats_file = 0;
			}

			log_stats_file = fopen( "stats.log", "w" );

			if ( log_stats_file )
				fprintf( log_stats_file, "entities,dlights,parts,frame time\n" );
		}

		else
		{
			if ( log_stats_file )
			{
				fclose( log_stats_file );
				log_stats_file = 0;
			}
		}
	}

	if (fixedtime->value)
		msec = fixedtime->value;

	else if (timescale->value)
	{
		msec *= timescale->value;

		if (msec < 1)
			msec = 1;
	}

#ifndef DEDICATED_ONLY

	if (showtrace->value)
	{
		extern	int c_traces, c_brush_traces;
		extern	int	c_pointcontents;

		Com_Printf ("%4i traces  %4i points\n", c_traces, c_pointcontents);
		c_traces = 0;
		c_brush_traces = 0;
		c_pointcontents = 0;
	}

#endif

	do
	{
		s = Sys_ConsoleInput ();

		if (s)
			Cbuf_AddText (va("%s\n",s));
	}
	while (s);

	Cbuf_Execute ();

#ifndef DEDICATED_ONLY

	if (host_speeds->value)
		time_before = Sys_Milliseconds ();

#endif

	SV_Frame (msec);

#ifndef DEDICATED_ONLY

	if (host_speeds->value)
		time_between = Sys_Milliseconds ();

	CL_Frame (msec);

	if (host_speeds->value)
		time_after = Sys_Milliseconds ();


	if (host_speeds->value)
	{
		int			all, sv, gm, cl, rf;

		all = time_after - time_before;
		sv = time_between - time_before;
		cl = time_after - time_between;
		gm = time_after_game - time_before_game;
		rf = time_after_ref - time_before_ref;
		sv -= gm;
		cl -= rf;
		Com_Printf ("all:%3i sv:%3i gm:%3i cl:%3i rf:%3i\n",
		            all, sv, gm, cl, rf);
	}

#endif
}

void Qcommon_Shutdown (void)
{
}

