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
 * This file implements the command processor for command scripting.
 *
 * =======================================================================
 */

#include "../header/common.h"
#include "../header/cmd.h"

void Cmd_Exec_f (void) {
	char	*f, *f2;
	int		len;

	if (Cmd_Argc () != 2) {
		Com_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	len = FS_LoadFile (Cmd_Argv(1), (void **)&f);

	if (!f) {
		Com_Printf ("couldn't exec %s\n",Cmd_Argv(1));
		return;
	}

	Com_Printf ("execing %s\n",Cmd_Argv(1));

	/* the file doesn't have a trailing 0, so we need to copy it off */
	f2 = Z_Malloc(len+1);
	memcpy (f2, f, len);
	f2[len] = 0;

	Cbuf_InsertText (f2);

	Z_Free (f2);
	FS_FreeFile (f);
}

/*
 * Just prints the rest of the line to the console
 */
void Cmd_Echo_f (void) {
	int		i;

	for (i=1 ; i<Cmd_Argc() ; i++)
		Com_Printf ("%s ",Cmd_Argv(i));

	Com_Printf ("\n");
}

/*
 * Creates a new command that executes
 * a command string (possibly ; seperated)
 */
void Cmd_Alias_f (void) {
	cmdalias_t	*a;
	char		cmd[1024];
	int			i, c;
	char		*s;

	if (Cmd_Argc() == 1) {
		Com_Printf ("Current alias commands:\n");

		for (a = cmd_alias ; a ; a=a->next)
			Com_Printf ("%s : %s\n", a->name, a->value);

		return;
	}

	s = Cmd_Argv(1);

	if (strlen(s) >= MAX_ALIAS_NAME) {
		Com_Printf ("Alias name is too long\n");
		return;
	}

	/* if the alias already exists, reuse it */
	for (a = cmd_alias ; a ; a=a->next) {
		if (!strcmp(s, a->name)) {
			Z_Free (a->value);
			break;
		}
	}

	if (!a) {
		a = Z_Malloc (sizeof(cmdalias_t));
		a->next = cmd_alias;
		cmd_alias = a;
	}

	strcpy (a->name, s);

	/* copy the rest of the command line */
	cmd[0] = 0; /* start out with a null string */
	c = Cmd_Argc();

	for (i=2 ; i< c ; i++) {
		strcat (cmd, Cmd_Argv(i));

		if (i != (c - 1))
			strcat (cmd, " ");
	}

	strcat (cmd, "\n");

	a->value = CopyString (cmd);
}
