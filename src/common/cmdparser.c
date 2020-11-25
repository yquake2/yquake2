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
 * This file implements the Quake II command processor. Every command
 * which is send via the command line at startup, via the console and
 * via rcon is processed here and send to the apropriate subsystem.
 *
 * =======================================================================
 */

#include <ctype.h>

#include "header/common.h"

#define MAX_ALIAS_NAME 32
#define ALIAS_LOOP_COUNT 16

typedef struct cmd_function_s
{
	struct cmd_function_s *next;
	char *name;
	xcommand_t function;
} cmd_function_t;

static cmd_function_t *cmd_functions; /* possible commands to execute */

typedef struct cmdalias_s
{
	struct cmdalias_s *next;
	char name[MAX_ALIAS_NAME];
	char *value;
} cmdalias_t;

char retval[256];
int alias_count; /* for detecting runaway loops */
cmdalias_t *cmd_alias;
int cmd_wait;
static int cmd_argc;
static int cmd_argc;
static char *cmd_argv[MAX_STRING_TOKENS];
static char *cmd_null_string = "";
static char cmd_args[MAX_STRING_CHARS];
sizebuf_t cmd_text;
byte cmd_text_buf[32768];
char defer_text_buf[32768];

/*
 * Causes execution of the remainder of the command buffer to be delayed
 * until next frame.  This allows commands like: bind g "impulse 5 ;
 * +attack ; wait ; -attack ; impulse 2"
 */
void
Cmd_Wait_f(void)
{
	cmd_wait = Sys_Milliseconds();
}

void
Cbuf_Init(void)
{
	SZ_Init(&cmd_text, cmd_text_buf, sizeof(cmd_text_buf));
}

/*
 * Adds command text at the end of the buffer
 */
void
Cbuf_AddText(char *text)
{
	int l;

	l = strlen(text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Com_Printf("Cbuf_AddText: overflow\n");
		return;
	}

	SZ_Write(&cmd_text, text, strlen(text));
}

/*
 * Adds command text immediately after the current command
 * Adds a \n to the text
 */
void
Cbuf_InsertText(char *text)
{
	char *temp;
	int templen;

	/* copy off any commands still remaining in the exec buffer */
	templen = cmd_text.cursize;

	if (templen)
	{
		temp = Z_Malloc(templen);
		memcpy(temp, cmd_text.data, templen);
		SZ_Clear(&cmd_text);
	}
	else
	{
		temp = NULL;
	}

	/* add the entire text of the file */
	Cbuf_AddText(text);

	/* add the copied off data */
	if (templen)
	{
		SZ_Write(&cmd_text, temp, templen);
		Z_Free(temp);
	}
}

void
Cbuf_CopyToDefer(void)
{
	memcpy(defer_text_buf, cmd_text_buf, cmd_text.cursize);
	defer_text_buf[cmd_text.cursize] = 0;
	cmd_text.cursize = 0;
}

void
Cbuf_InsertFromDefer(void)
{
	Cbuf_InsertText(defer_text_buf);
	defer_text_buf[0] = 0;
}

void
Cbuf_ExecuteText(int exec_when, char *text)
{
	switch (exec_when)
	{
		case EXEC_NOW:
			Cmd_ExecuteString(text);
			break;
		case EXEC_INSERT:
			Cbuf_InsertText(text);
			break;
		case EXEC_APPEND:
			Cbuf_AddText(text);
			break;
		default:
			Com_Error(ERR_FATAL, "Cbuf_ExecuteText: bad exec_when");
	}
}

void
Cbuf_Execute(void)
{
	int i;
	char *text;
	char line[1024];
	int quotes;

	if(cmd_wait > 0)
	{
		// make sure that "wait" in scripts waits for ~16.66ms (1 frame at 60fps)
		// regardless of framerate
		if (Sys_Milliseconds() - cmd_wait <= 16)
		{
			return;
		}
		cmd_wait = 0;
	}

	alias_count = 0; /* don't allow infinite alias loops */

	while (cmd_text.cursize)
	{
		/* find a \n or ; line break */
		text = (char *)cmd_text.data;

		quotes = 0;

		for (i = 0; i < cmd_text.cursize; i++)
		{
			if (text[i] == '"')
			{
				quotes++;
			}

			if (!(quotes & 1) && (text[i] == ';'))
			{
				break; /* don't break if inside a quoted string */
			}

			if (text[i] == '\n')
			{
				break;
			}
		}

		if (i > sizeof(line) - 1)
		{
			memcpy(line, text, sizeof(line) - 1);
			line[sizeof(line) - 1] = 0;
		}
		else
		{
			memcpy(line, text, i);
			line[i] = 0;
		}

		/* delete the text from the command buffer and move remaining
		   commands down this is necessary because commands (exec,
		   alias) can insert data at the beginning of the text buffer */
		if (i == cmd_text.cursize)
		{
			cmd_text.cursize = 0;
		}
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove(text, text + i, cmd_text.cursize);
		}

		/* execute the command line */
		Cmd_ExecuteString(line);

		if (cmd_wait > 0)
		{
			/* skip out while text still remains in buffer,
			   leaving it for after we're done waiting */
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
void
Cbuf_AddEarlyCommands(qboolean clear)
{
	int i;
	char *s;

	for (i = 0; i < COM_Argc(); i++)
	{
		s = COM_Argv(i);

		if (strcmp(s, "+set") != 0)
		{
			continue;
		}

		Cbuf_AddText(va("set %s %s\n", COM_Argv(i + 1), COM_Argv(i + 2)));

		if (clear)
		{
			COM_ClearArgv(i);
			COM_ClearArgv(i + 1);
			COM_ClearArgv(i + 2);
		}

		i += 2;
	}
}

/*
 * Adds command line parameters as script statements
 * Commands lead with a + and continue until another + or -
 * quake +developer 1 +map amlev1
 *
 * Returns true if any late commands were added, which
 * will keep the demoloop from immediately starting
 */
qboolean
Cbuf_AddLateCommands(void)
{
	int i, j;
	int s;
	char *text, *build, c;
	int argc;
	qboolean ret;

	/* build the combined string to parse from */
	s = 0;
	argc = COM_Argc();

	for (i = 1; i < argc; i++)
	{
		s += strlen(COM_Argv(i)) + 1;
	}

	if (!s)
	{
		return false;
	}

	text = Z_Malloc(s + 1);
	text[0] = 0;

	for (i = 1; i < argc; i++)
	{
		strcat(text, COM_Argv(i));

		if (i != argc - 1)
		{
			strcat(text, " ");
		}
	}

	/* pull out the commands */
	build = Z_Malloc(s + 1);
	build[0] = 0;

	for (i = 0; i < s - 1; i++)
	{
		if (text[i] == '+')
		{
			i++;

			for (j = i; (text[j] != '+') && (text[j] != '-') && (text[j] != 0); j++)
			{
			}

			c = text[j];
			text[j] = 0;

			strcat(build, text + i);
			strcat(build, "\n");
			text[j] = c;
			i = j - 1;
		}
	}

	ret = (build[0] != 0);

	if (ret)
	{
		Cbuf_AddText(build);
	}

	Z_Free(text);
	Z_Free(build);

	return ret;
}

/*
 * Execute a script file
 */
void
Cmd_Exec_f(void)
{
	char *f, *f2;
	int len;

	if (Cmd_Argc() != 2)
	{
		Com_Printf("exec <filename> : execute a script file\n");
		return;
	}

	len = FS_LoadFile(Cmd_Argv(1), (void **)&f);

	if (!f)
	{
		Com_Printf("couldn't exec %s\n", Cmd_Argv(1));
		return;
	}

	Com_Printf("execing %s.\n", Cmd_Argv(1));

	/* the file doesn't have a trailing 0, so we need to copy it off */
	/* we also add a newline */
	f2 = Z_Malloc(len + 2);
	memcpy(f2, f, len);
	f2[len] = '\n'; // make sure last line has a newline
	f2[len+1] = '\0';

	Cbuf_InsertText(f2);

	Z_Free(f2);
	FS_FreeFile(f);
}

/*
 * Inserts the current value of a variable as command text
 */
void Cmd_Vstr_f( void ) {
	const char	*v;

	if (Cmd_Argc() != 2) {
		Com_Printf("vstr <variablename> : execute a variable command\n");
		return;
	}

	v = Cvar_VariableString(Cmd_Argv(1));
	Cbuf_InsertText(va("%s\n", v));
}

/*
 * Just prints the rest of the line to the console
 */
void
Cmd_Echo_f(void)
{
	int i;

	for (i = 1; i < Cmd_Argc(); i++)
	{
		Com_Printf("%s ", Cmd_Argv(i));
	}

	Com_Printf("\n");
}

/*
 * Creates a new command that executes
 * a command string (possibly ; seperated)
 */
void
Cmd_Alias_f(void)
{
	cmdalias_t *a;
	char cmd[1024];
	int i, c;
	char *s;

	if (Cmd_Argc() == 1)
	{
		Com_Printf("Current alias commands:\n");

		for (a = cmd_alias; a; a = a->next)
		{
			Com_Printf("%s : %s\n", a->name, a->value);
		}

		return;
	}

	s = Cmd_Argv(1);

	if (strlen(s) >= MAX_ALIAS_NAME)
	{
		Com_Printf("Alias name is too long\n");
		return;
	}

	/* if the alias already exists, reuse it */
	for (a = cmd_alias; a; a = a->next)
	{
		if (!strcmp(s, a->name))
		{
			Z_Free(a->value);
			break;
		}
	}

	if (!a)
	{
		a = Z_Malloc(sizeof(cmdalias_t));
		a->next = cmd_alias;
		cmd_alias = a;
	}

	strcpy(a->name, s);

	/* copy the rest of the command line */
	cmd[0] = 0; /* start out with a null string */
	c = Cmd_Argc();

	for (i = 2; i < c; i++)
	{
		strcat(cmd, Cmd_Argv(i));

		if (i != (c - 1))
		{
			strcat(cmd, " ");
		}
	}

	strcat(cmd, "\n");

	a->value = CopyString(cmd);
}

int
Cmd_Argc(void)
{
	return cmd_argc;
}

char *
Cmd_Argv(int arg)
{
	if ((unsigned)arg >= cmd_argc)
	{
		return cmd_null_string;
	}

	return cmd_argv[arg];
}

/*
 * Returns a single string containing argv(1) to argv(argc()-1)
 */
char *
Cmd_Args(void)
{
	return cmd_args;
}

char *
Cmd_MacroExpandString(char *text)
{
	int i, j, count, len;
	qboolean inquote;
	char *scan;
	static char expanded[MAX_STRING_CHARS];
	char temporary[MAX_STRING_CHARS];
	char *token, *start;

	inquote = false;
	scan = text;

	len = strlen(scan);

	if (len >= MAX_STRING_CHARS)
	{
		Com_Printf("Line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
		return NULL;
	}

	count = 0;

	for (i = 0; i < len; i++)
	{
		if (scan[i] == '"')
		{
			inquote ^= 1;
		}

		if (inquote)
		{
			continue; /* don't expand inside quotes */
		}

		if (scan[i] != '$')
		{
			continue;
		}

		/* scan out the complete macro */
		start = scan + i + 1;
		token = COM_Parse(&start);

		if (!start)
		{
			continue;
		}

		token = (char *)Cvar_VariableString(token);

		j = strlen(token);
		len += j;

		if (len >= MAX_STRING_CHARS)
		{
			Com_Printf("Expanded line exceeded %i chars, discarded.\n",
					MAX_STRING_CHARS);
			return NULL;
		}

		memcpy(temporary, scan, i);
		memcpy(temporary + i, token, j);
		strcpy(temporary + i + j, start);

		strcpy(expanded, temporary);
		scan = expanded;
		i--;

		if (++count == 100)
		{
			Com_Printf("Macro expansion loop, discarded.\n");
			return NULL;
		}
	}

	if (inquote)
	{
		Com_Printf("Line has unmatched quote, discarded.\n");
		return NULL;
	}

	return scan;
}

/*
 * Parses the given string into command line tokens.
 * $Cvars will be expanded unless they are in a quoted token
 */
void
Cmd_TokenizeString(char *text, qboolean macroExpand)
{
	int i;
	const char *com_token;

	/* clear the args from the last string */
	for (i = 0; i < cmd_argc; i++)
	{
		Z_Free(cmd_argv[i]);
	}

	cmd_argc = 0;
	cmd_args[0] = 0;

	/* macro expand the text */
	if (macroExpand)
	{
		text = Cmd_MacroExpandString(text);
	}

	if (!text)
	{
		return;
	}

	while (1)
	{
		/* skip whitespace up to a /n */
		while (*text && *text <= ' ' && *text != '\n')
		{
			text++;
		}

		if (*text == '\n')
		{
			/* a newline seperates commands in the buffer */
			text++;
			break;
		}

		if (!*text)
		{
			return;
		}

		/* set cmd_args to everything after the first arg */
		if (cmd_argc == 1)
		{
			int l;

			strcpy(cmd_args, text);

			/* strip off any trailing whitespace */
			l = strlen(cmd_args) - 1;

			for ( ; l >= 0; l--)
			{
				if (cmd_args[l] <= ' ')
				{
					cmd_args[l] = 0;
				}

				else
				{
					break;
				}
			}
		}

		com_token = COM_Parse(&text);

		if (!text)
		{
			return;
		}

		if (cmd_argc < MAX_STRING_TOKENS)
		{
			cmd_argv[cmd_argc] = Z_Malloc(strlen(com_token) + 1);
			strcpy(cmd_argv[cmd_argc], com_token);
			cmd_argc++;
		}
	}
}

void
Cmd_AddCommand(char *cmd_name, xcommand_t function)
{
	cmd_function_t *cmd;
	cmd_function_t **pos;

	/* fail if the command is a variable name */
	if (Cvar_VariableString(cmd_name)[0])
	{
		Cmd_RemoveCommand(cmd_name);
	}

	/* fail if the command already exists */
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!strcmp(cmd_name, cmd->name))
		{
			Com_Printf("Cmd_AddCommand: %s already defined\n", cmd_name);
			return;
		}
	}

	cmd = Z_Malloc(sizeof(cmd_function_t));
	cmd->name = cmd_name;
	cmd->function = function;

	/* link the command in */
	pos = &cmd_functions;
	while (*pos && strcmp((*pos)->name, cmd->name) < 0)
	{
		pos = &(*pos)->next;
	}
	cmd->next = *pos;
	*pos = cmd;
}

void
Cmd_RemoveCommand(char *cmd_name)
{
	cmd_function_t *cmd, **back;

	back = &cmd_functions;

	while (1)
	{
		cmd = *back;

		if (!cmd)
		{
			Com_Printf("Cmd_RemoveCommand: %s not added\n", cmd_name);
			return;
		}

		if (!strcmp(cmd_name, cmd->name))
		{
			*back = cmd->next;
			Z_Free(cmd);
			return;
		}

		back = &cmd->next;
	}
}

qboolean
Cmd_Exists(char *cmd_name)
{
	cmd_function_t *cmd;

	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!strcmp(cmd_name, cmd->name))
		{
			return true;
		}
	}

	return false;
}

char *
Cmd_CompleteCommand(char *partial)
{
	cmd_function_t *cmd;
	int len, i, o, p;
	cmdalias_t *a;
	cvar_t *cvar;
	char *pmatch[1024];
	qboolean diff = false;

	len = strlen(partial);

	if (!len)
	{
		return NULL;
	}

	/* check for exact match */
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!strcmp(partial, cmd->name))
		{
			return cmd->name;
		}
	}

	for (a = cmd_alias; a; a = a->next)
	{
		if (!strcmp(partial, a->name))
		{
			return a->name;
		}
	}

	for (cvar = cvar_vars; cvar; cvar = cvar->next)
	{
		if (!strcmp(partial, cvar->name))
		{
			return cvar->name;
		}
	}

	for (i = 0; i < 1024; i++)
	{
		pmatch[i] = NULL;
	}

	i = 0;

	/* check for partial match */
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!strncmp(partial, cmd->name, len))
		{
			pmatch[i] = cmd->name;
			i++;
		}
	}

	for (a = cmd_alias; a; a = a->next)
	{
		if (!strncmp(partial, a->name, len))
		{
			pmatch[i] = a->name;
			i++;
		}
	}

	for (cvar = cvar_vars; cvar; cvar = cvar->next)
	{
		if (!strncmp(partial, cvar->name, len))
		{
			pmatch[i] = cvar->name;
			i++;
		}
	}

	if (i)
	{
		if (i == 1)
		{
			return pmatch[0];
		}

		/* Sort it */
		qsort(pmatch, i, sizeof(pmatch[0]), Q_sort_strcomp);

		Com_Printf("\n\n");

		for (o = 0; o < i; o++)
		{
			Com_Printf("  %s\n", pmatch[o]);
		}

		strcpy(retval, "");
		p = 0;

		while (!diff && p < 256)
		{
			retval[p] = pmatch[0][p];

			for (o = 0; o < i; o++)
			{
				if (p > strlen(pmatch[o]))
				{
					continue;
				}

				if (retval[p] != pmatch[o][p])
				{
					retval[p] = 0;
					diff = true;
				}
			}

			p++;
		}

		return retval;
	}

	return NULL;
}

char *
Cmd_CompleteMapCommand(char *partial)
{
	char **mapNames;
	int i, j, k, nbMatches, len, nMaps;
	char *mapName;
	char *pmatch[1024];
	qboolean partialFillContinue = true;

	if ((mapNames = FS_ListFiles2("maps/*.bsp", &nMaps, 0, 0)) != 0)
	{
		len = strlen(partial);
		nbMatches = 0;
		memset(retval, 0, strlen(retval));

		for (i = 0; i < nMaps - 1; i++)
		{
			if (strrchr(mapNames[i], '/'))
			{
				mapName = strrchr(mapNames[i], '/') + 1;
			}
			else
			{
				mapName = mapNames[i];
			}

			mapName = strtok(mapName, ".");

			/* check for exact match */
			if (!Q_strcasecmp(partial, mapName))
			{
				strcpy(retval, partial);
			}
			/* check for partial match */
			else if (!Q_strncasecmp(partial, mapName, len))
			{
				pmatch[nbMatches] = mapName;
				nbMatches++;
			}
		}

		if (nbMatches == 1)
		{
			strcpy(retval, pmatch[0]);
		}
		else if (nbMatches > 1)
		{
			Com_Printf("\n=================\n\n");

			for (j = 0; j < nbMatches; j++)
			{
				Com_Printf("%s\n", pmatch[j]);
			}

			//partial complete
			for (j = 0; j < strlen(pmatch[0]); j++)
			{
				for (k = 1; k < nbMatches; k++)
				{
					if (j >= strlen(pmatch[k]) || tolower((unsigned char)pmatch[0][j]) != tolower((unsigned char)pmatch[k][j]))
					{
						partialFillContinue = false;
						break;
					}
				}

				if (partialFillContinue)
				{
					retval[j] = pmatch[0][j];
				}
				else
				{
					break;
				}
			}
		}

		FS_FreeList(mapNames, nMaps);
	}

	return retval;
}

qboolean
Cmd_IsComplete(char *command)
{
	cmd_function_t *cmd;
	cmdalias_t *a;
	cvar_t *cvar;

	/* check for exact match */
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!strcmp(command, cmd->name))
		{
			return true;
		}
	}

	for (a = cmd_alias; a; a = a->next)
	{
		if (!strcmp(command, a->name))
		{
			return true;
		}
	}

	for (cvar = cvar_vars; cvar; cvar = cvar->next)
	{
		if (!strcmp(command, cvar->name))
		{
			return true;
		}
	}

	return false;
}

/* ugly hack to suppress warnings from default.cfg in Key_Bind_f() */
qboolean doneWithDefaultCfg;

/*
 * A complete command line has been parsed, so try to execute it
 */
void
Cmd_ExecuteString(char *text)
{
	cmd_function_t *cmd;
	cmdalias_t *a;

	Cmd_TokenizeString(text, true);

	/* execute the command line */
	if (!Cmd_Argc())
	{
		return; /* no tokens */
	}

	if(Cmd_Argc() > 1 && Q_strcasecmp(cmd_argv[0], "exec") == 0 && Q_strcasecmp(cmd_argv[1], "yq2.cfg") == 0)
	{
		/* exec yq2.cfg is done directly after exec default.cfg, see Qcommon_Init() */
		doneWithDefaultCfg = true;
	}

	/* check functions */
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!Q_strcasecmp(cmd_argv[0], cmd->name))
		{
			if (!cmd->function)
			{
				/* forward to server command */
				Cmd_ExecuteString(va("cmd %s", text));
			}
			else
			{
				cmd->function();
			}

			return;
		}
	}

	/* check alias */
	for (a = cmd_alias; a; a = a->next)
	{
		if (!Q_strcasecmp(cmd_argv[0], a->name))
		{
			if (++alias_count == ALIAS_LOOP_COUNT)
			{
				Com_Printf("ALIAS_LOOP_COUNT\n");
				return;
			}

			Cbuf_InsertText(a->value);
			return;
		}
	}

	/* check cvars */
	if (Cvar_Command())
	{
		return;
	}

#ifndef DEDICATED_ONLY
	/* send it as a server command if we are connected */
	Cmd_ForwardToServer();
#endif
}

void
Cmd_List_f(void)
{
	cmd_function_t *cmd;
	int i;

	i = 0;

	for (cmd = cmd_functions; cmd; cmd = cmd->next, i++)
	{
		Com_Printf("%s\n", cmd->name);
	}

	Com_Printf("%i commands\n", i);
}

void
Cmd_Init(void)
{
	/* register our commands */
	Cmd_AddCommand("cmdlist", Cmd_List_f);
	Cmd_AddCommand("exec", Cmd_Exec_f);
	Cmd_AddCommand("vstr", Cmd_Vstr_f);
	Cmd_AddCommand("echo", Cmd_Echo_f);
	Cmd_AddCommand("alias", Cmd_Alias_f);
	Cmd_AddCommand("wait", Cmd_Wait_f);
}

