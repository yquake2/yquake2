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
 * Client / Server interactions
 *
 * =======================================================================
 */

#include "header/common.h"
#include <stdlib.h>
#include <setjmp.h>

#define MAXPRINTMSG 4096

FILE *logfile;
cvar_t *logfile_active;  /* 1 = buffer log, 2 = flush after each print */
jmp_buf abortframe; /* an ERR_DROP occured, exit the entire frame */
int server_state;

static int rd_target;
static char *rd_buffer;
static int rd_buffersize;
static void (*rd_flush)(int target, char *buffer);

void
Com_BeginRedirect(int target, char *buffer, int buffersize, void (*flush)(int, char *))
{
	if (!target || !buffer || !buffersize || !flush)
	{
		return;
	}

	rd_target = target;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;

	*rd_buffer = 0;
}

void
Com_EndRedirect(void)
{
	rd_flush(rd_target, rd_buffer);

	rd_target = 0;
	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
}

/*
 * Both client and server can use this, and it will output
 * to the apropriate place.
 */
void
Com_VPrintf(int print_level, const char *fmt, va_list argptr)
{
	if((print_level == PRINT_DEVELOPER) && (!developer || !developer->value))
	{
		return; /* don't confuse non-developers with techie stuff... */
	}
	else
	{
		int i;
		char msg[MAXPRINTMSG];

		int msgLen = vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
		if (msgLen >= MAXPRINTMSG || msgLen < 0) {
			msgLen = MAXPRINTMSG-1;
			msg[msgLen] = '\0';
		}

		if (rd_target)
		{
			if ((msgLen + strlen(rd_buffer)) > (rd_buffersize - 1))
			{
				rd_flush(rd_target, rd_buffer);
				*rd_buffer = 0;
			}

			strcat(rd_buffer, msg);
			return;
		}

	#ifndef DEDICATED_ONLY
		Con_Print(msg);
	#endif

		// remove unprintable characters
		for(i=0; i<msgLen; ++i)
		{
			char c = msg[i];
			if(c < ' ' && (c < '\t' || c > '\r'))
			{
				switch(c)
				{
					// no idea if the following two are ever sent here, but in conchars.pcx they look like this
					// so do the replacements.. won't hurt I guess..
					case 0x10:
						msg[i] = '[';
						break;
					case 0x11:
						msg[i] = ']';
						break;
					// horizontal line chars
					case 0x1D:
					case 0x1F:
						msg[i] = '-';
						break;
					case 0x1E:
						msg[i] = '=';
						break;
					default: // just replace all other unprintable chars with space, should be good enough
						msg[i] = ' ';
				}
			}
		}

		/* also echo to debugging console */
		Sys_ConsoleOutput(msg);

		/* logfile */
		if (logfile_active && logfile_active->value)
		{
			char name[MAX_OSPATH];

			if (!logfile)
			{
				Com_sprintf(name, sizeof(name), "%s/qconsole.log", FS_Gamedir());

				if (logfile_active->value > 2)
				{
					logfile = Q_fopen(name, "a");
				}

				else
				{
					logfile = Q_fopen(name, "w");
				}
			}

			if (logfile)
			{
				fprintf(logfile, "%s", msg);
			}

			if (logfile_active->value > 1)
			{
				fflush(logfile);  /* force it to save every time */
			}
		}
	}
}

/*
 * Both client and server can use this, and it will output
 * to the apropriate place.
 */
void
Com_Printf(char *fmt, ...)
{
	va_list argptr;
	va_start(argptr, fmt);
	Com_VPrintf(PRINT_ALL, fmt, argptr);
	va_end(argptr);
}

/*
 * A Com_Printf that only shows up if the "developer" cvar is set
 */
void
Com_DPrintf(char *fmt, ...)
{
	va_list argptr;
	va_start(argptr, fmt);
	Com_VPrintf(PRINT_DEVELOPER, fmt, argptr);
	va_end(argptr);
}

/*
 * A Com_Printf that only shows up when either the "modder" or "developer"
 * cvars is set
 */
void
Com_MDPrintf(char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	if ((!modder || !modder->value) && (!developer || !developer->value))
	{
		return;
	}

	va_start(argptr, fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);

	Com_Printf("%s", msg);
}

/*
 * Both client and server can use this, and it will
 * do the apropriate things.
 */
void
Com_Error(int code, char *fmt, ...)
{
	va_list argptr;
	static char msg[MAXPRINTMSG];
	static qboolean recursive;

	if (recursive)
	{
		Sys_Error("recursive error after: %s", msg);
	}

	recursive = true;

	va_start(argptr, fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);

	if (code == ERR_DISCONNECT)
	{
#ifndef DEDICATED_ONLY
		CL_Drop();
#endif
		recursive = false;
		longjmp(abortframe, -1);
	}
	else if (code == ERR_DROP)
	{
		Com_Printf("********************\nERROR: %s\n********************\n",
				msg);
		SV_Shutdown(va("Server crashed: %s\n", msg), false);
#ifndef DEDICATED_ONLY
		CL_Drop();
#endif
		recursive = false;
		longjmp(abortframe, -1);
	}
	else
	{
		SV_Shutdown(va("Server fatal crashed: %s\n", msg), false);
#ifndef DEDICATED_ONLY
		CL_Shutdown();
#endif
	}

	if (logfile)
	{
		fclose(logfile);
		logfile = NULL;
	}

	Sys_Error("%s", msg);
	recursive = false;
}

/*
 * Both client and server can use this, and it will
 * do the apropriate things.
 */
void
Com_Quit(void)
{
	Com_Printf("\n----------- shutting down ----------\n");
	SV_Shutdown("Server quit\n", false);
	Sys_Quit();
}

int
Com_ServerState(void)
{
	return server_state;
}

void
Com_SetServerState(int state)
{
	server_state = state;
}

