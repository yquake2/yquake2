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
 * Common argument processing
 *
 * =======================================================================
 */

#include "header/common.h"

#define MAX_NUM_ARGVS 50

int com_argc;
char *com_argv[MAX_NUM_ARGVS + 1];

/*
 * Returns the position (1 to argc-1) in the program's argument list
 * where the given parameter apears, or 0 if not present
 */
int
COM_CheckParm(char *parm)
{
	int i;

	for (i = 1; i < com_argc; i++)
	{
		if (!strcmp(parm, com_argv[i]))
		{
			return i;
		}
	}

	return 0;
}

int
COM_Argc(void)
{
	return com_argc;
}

char *
COM_Argv(int arg)
{
	if ((arg < 0) || (arg >= com_argc) || !com_argv[arg])
	{
		return "";
	}

	return com_argv[arg];
}

void
COM_ClearArgv(int arg)
{
	if ((arg < 0) || (arg >= com_argc) || !com_argv[arg])
	{
		return;
	}

	com_argv[arg] = "";
}

void
COM_InitArgv(int argc, char **argv)
{
	int i;

	if (argc > MAX_NUM_ARGVS)
	{
		Com_Error(ERR_FATAL, "argc > MAX_NUM_ARGVS");
	}

	com_argc = argc;

	for (i = 0; i < argc; i++)
	{
		if (!argv[i] || (strlen(argv[i]) >= MAX_TOKEN_CHARS))
		{
			com_argv[i] = "";
		}

		else
		{
			com_argv[i] = argv[i];
		}
	}
}

/*
 * Adds the given string at the end of the current argument list
 */
void
COM_AddParm(char *parm)
{
	if (com_argc == MAX_NUM_ARGVS)
	{
		Com_Error(ERR_FATAL, "COM_AddParm: MAX_NUM)ARGS");
	}

	com_argv[com_argc++] = parm;
}

int
memsearch(byte *start, int count, int search)
{
	int i;

	for (i = 0; i < count; i++)
	{
		if (start[i] == search)
		{
			return i;
		}
	}

	return -1;
}

char *
CopyString(char *in)
{
	char *out;

	out = Z_Malloc((int)strlen(in) + 1);
	strcpy(out, in);
	return out;
}

void
Info_Print(char *s)
{
	char key[512];
	char value[512];
	char *o;
	int l;

	if (*s == '\\')
	{
		s++;
	}

	while (*s)
	{
		o = key;

		while (*s && *s != '\\')
		{
			*o++ = *s++;
		}

		l = o - key;

		if (l < 20)
		{
			memset(o, ' ', 20 - l);
			key[20] = 0;
		}

		else
		{
			*o = 0;
		}

		Com_Printf("%s", key);

		if (!*s)
		{
			Com_Printf("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;

		while (*s && *s != '\\')
		{
			*o++ = *s++;
		}

		*o = 0;

		if (*s)
		{
			s++;
		}

		Com_Printf("%s\n", value);
	}
}

