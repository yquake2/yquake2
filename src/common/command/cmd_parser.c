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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * =======================================================================
 *
 * This file implements the main part of the command processor. Every
 * command which is send vie the command line at startup, via the
 * console and via rcon is processed here and send to the apropriate
 * subsystem.
 *
 * =======================================================================
 */

#include "../header/common.h"
#include "../header/cmd.h"

/*
 * Causes execution of the remainder of the command buffer to be delayed
 * until next frame.  This allows commands like: bind g "impulse 5 ;
 * +attack ; wait ; -attack ; impulse 2"
 */
void Cmd_Wait_f (void) {
	cmd_wait = true;
}

/* COMMAND BUFFER */

sizebuf_t	cmd_text;
byte		cmd_text_buf[8192];

char		defer_text_buf[8192];

void Cbuf_Init (void) {
	SZ_Init (&cmd_text, cmd_text_buf, sizeof(cmd_text_buf));
}

/*
 * Adds command text at the end of the buffer
 */
void Cbuf_AddText (char *text) {
	int		l;

	l = strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize) {
		Com_Printf ("Cbuf_AddText: overflow\n");
		return;
	}

	SZ_Write (&cmd_text, text, strlen (text));
}

/*
 * Adds command text immediately after the current command
 * Adds a \n to the text
 */
void Cbuf_InsertText (char *text) {
	char	*temp;
	int		templen;

	/* copy off any commands still remaining in the exec buffer */
	templen = cmd_text.cursize;

	if (templen) {
		temp = Z_Malloc (templen);
		memcpy (temp, cmd_text.data, templen);
		SZ_Clear (&cmd_text);

	} else
		temp = NULL;

	/* add the entire text of the file */
	Cbuf_AddText (text);

	/* add the copied off data */
	if (templen) {
		SZ_Write (&cmd_text, temp, templen);
		Z_Free (temp);
	}
}

void Cbuf_CopyToDefer (void) {
	memcpy(defer_text_buf, cmd_text_buf, cmd_text.cursize);
	defer_text_buf[cmd_text.cursize] = 0;
	cmd_text.cursize = 0;
}

void Cbuf_InsertFromDefer (void) {
	Cbuf_InsertText (defer_text_buf);
	defer_text_buf[0] = 0;
}

void Cbuf_ExecuteText (int exec_when, char *text) {
	switch (exec_when) {
		case EXEC_NOW:
			Cmd_ExecuteString (text);
			break;
		case EXEC_INSERT:
			Cbuf_InsertText (text);
			break;
		case EXEC_APPEND:
			Cbuf_AddText (text);
			break;
		default:
			Com_Error (ERR_FATAL, "Cbuf_ExecuteText: bad exec_when");
	}
}

void Cbuf_Execute (void) {
	int		i;
	char	*text;
	char	line[1024];
	int		quotes;

	alias_count = 0; /* don't allow infinite alias loops */

	while (cmd_text.cursize) {
		/* find a \n or ; line break */
		text = (char *)cmd_text.data;

		quotes = 0;

		for (i=0 ; i< cmd_text.cursize ; i++) {
			if (text[i] == '"')
				quotes++;

			if ( !(quotes&1) &&  text[i] == ';')
				break; /* don't break if inside a quoted string */

			if (text[i] == '\n')
				break;
		}


		memcpy (line, text, i);
		line[i] = 0;

		/* delete the text from the command buffer and move remaining
		   commands down this is necessary because commands (exec,
		   alias) can insert data at the beginning of the text buffer */
		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;

		else {
			i++;
			cmd_text.cursize -= i;
			memmove (text, text+i, cmd_text.cursize);
		}

		/* execute the command line */
		Cmd_ExecuteString (line);

		if (cmd_wait) {
			/* skip out while text still remains in buffer,
			   leaving it for next frame */
			cmd_wait = false;
			break;
		}
	}
}

/*
 * Adds command line parameters as script statements Commands lead with
 * a +, and continue until another +
 *
 * Set commands are added early, so they are guaranteed to be set before
 * the client and server initialize for the first time.
 *
 * Other commands are added late, after all initialization is complete.
 */
void Cbuf_AddEarlyCommands (qboolean clear) {
	int		i;
	char	*s;

	for (i=0 ; i<COM_Argc() ; i++) {
		s = COM_Argv(i);

		if (strcmp (s, "+set"))
			continue;

		Cbuf_AddText (va("set %s %s\n", COM_Argv(i+1), COM_Argv(i+2)));

		if (clear) {
			COM_ClearArgv(i);
			COM_ClearArgv(i+1);
			COM_ClearArgv(i+2);
		}

		i+=2;
	}
}

/*
 * Adds command line parameters as script statements
 * Commands lead with a + and continue until another + or -
 * quake +vid_ref gl +map amlev1
 *
 * Returns true if any late commands were added, which
 * will keep the demoloop from immediately starting
 */
qboolean Cbuf_AddLateCommands (void) {
	int		i, j;
	int		s;
	char	*text, *build, c;
	int		argc;
	qboolean	ret;

	/* build the combined string to parse from */
	s = 0;
	argc = COM_Argc();

	for (i=1 ; i<argc ; i++) {
		s += strlen (COM_Argv(i)) + 1;
	}

	if (!s)
		return false;

	text = Z_Malloc (s+1);
	text[0] = 0;

	for (i=1 ; i<argc ; i++) {
		strcat (text,COM_Argv(i));

		if (i != argc-1)
			strcat (text, " ");
	}

	/* pull out the commands */
	build = Z_Malloc (s+1);
	build[0] = 0;

	for (i=0 ; i<s-1 ; i++) {
		if (text[i] == '+') {
			i++;

			for (j=i ; (text[j] != '+') && (text[j] != '-') && (text[j] != 0) ; j++)
				;

			c = text[j];
			text[j] = 0;

			strcat (build, text+i);
			strcat (build, "\n");
			text[j] = c;
			i = j-1;
		}
	}

	ret = (build[0] != 0);

	if (ret)
		Cbuf_AddText (build);

	Z_Free (text);
	Z_Free (build);

	return ret;
}
