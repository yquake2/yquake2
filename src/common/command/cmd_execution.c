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
 * This file implements the command execution, e.g. directs the commands
 * to the appropriate subsystems.
 *
 * =======================================================================
 */

#include "../header/common.h"
#include "../header/cmd.h"

typedef struct cmd_function_s
{
	struct cmd_function_s   *next;
	char                    *name;
	xcommand_t function;
} cmd_function_t;

static int cmd_argc;
static char     *cmd_argv [ MAX_STRING_TOKENS ];
static char     *cmd_null_string = "";
static char cmd_args [ MAX_STRING_CHARS ];
char retval [ 256 ];

static cmd_function_t  *cmd_functions;  /* possible commands to execute */

int
Cmd_Argc ( void )
{
	return ( cmd_argc );
}

char *
Cmd_Argv ( int arg )
{
	if ( (unsigned) arg >= cmd_argc )
	{
		return ( cmd_null_string );
	}

	return ( cmd_argv [ arg ] );
}

/*
 * Returns a single string containing argv(1) to argv(argc()-1)
 */
char *
Cmd_Args ( void )
{
	return ( cmd_args );
}

char *
Cmd_MacroExpandString ( char *text )
{
	int i, j, count, len;
	qboolean inquote;
	char    *scan;
	static char expanded [ MAX_STRING_CHARS ];
	char temporary [ MAX_STRING_CHARS ];
	char    *token, *start;

	inquote = false;
	scan = text;

	len = strlen( scan );

	if ( len >= MAX_STRING_CHARS )
	{
		Com_Printf( "Line exceeded %i chars, discarded.\n", MAX_STRING_CHARS );
		return ( NULL );
	}

	count = 0;

	for ( i = 0; i < len; i++ )
	{
		if ( scan [ i ] == '"' )
		{
			inquote ^= 1;
		}

		if ( inquote )
		{
			continue; /* don't expand inside quotes */
		}

		if ( scan [ i ] != '$' )
		{
			continue;
		}

		/* scan out the complete macro */
		start = scan + i + 1;
		token = COM_Parse( &start );

		if ( !start )
		{
			continue;
		}

		token = (char *) Cvar_VariableString( token );

		j = strlen( token );
		len += j;

		if ( len >= MAX_STRING_CHARS )
		{
			Com_Printf( "Expanded line exceeded %i chars, discarded.\n", MAX_STRING_CHARS );
			return ( NULL );
		}

		strncpy( temporary, scan, i );
		strcpy( temporary + i, token );
		strcpy( temporary + i + j, start );

		strcpy( expanded, temporary );
		scan = expanded;
		i--;

		if ( ++count == 100 )
		{
			Com_Printf( "Macro expansion loop, discarded.\n" );
			return ( NULL );
		}
	}

	if ( inquote )
	{
		Com_Printf( "Line has unmatched quote, discarded.\n" );
		return ( NULL );
	}

	return ( scan );
}

/*
 * Parses the given string into command line tokens.
 * $Cvars will be expanded unless they are in a quoted token
 */
void
Cmd_TokenizeString ( char *text, qboolean macroExpand )
{
	int i;
	const char  *com_token;

	/* clear the args from the last string */
	for ( i = 0; i < cmd_argc; i++ )
	{
		Z_Free( cmd_argv [ i ] );
	}

	cmd_argc = 0;
	cmd_args [ 0 ] = 0;

	/* macro expand the text */
	if ( macroExpand )
	{
		text = Cmd_MacroExpandString( text );
	}

	if ( !text )
	{
		return;
	}

	while ( 1 )
	{
		/* skip whitespace up to a /n */
		while ( *text && *text <= ' ' && *text != '\n' )
		{
			text++;
		}

		if ( *text == '\n' )
		{
			/* a newline seperates commands in the buffer */
			text++;
			break;
		}

		if ( !*text )
		{
			return;
		}

		/* set cmd_args to everything after the first arg */
		if ( cmd_argc == 1 )
		{
			int l;

			strcpy( cmd_args, text );

			/* strip off any trailing whitespace */
			l = strlen( cmd_args ) - 1;

			for ( ; l >= 0; l-- )
			{
				if ( cmd_args [ l ] <= ' ' )
				{
					cmd_args [ l ] = 0;
				}

				else
				{
					break;
				}
			}
		}

		com_token = COM_Parse( &text );

		if ( !text )
		{
			return;
		}

		if ( cmd_argc < MAX_STRING_TOKENS )
		{
			cmd_argv [ cmd_argc ] = Z_Malloc( strlen( com_token ) + 1 );
			strcpy( cmd_argv [ cmd_argc ], com_token );
			cmd_argc++;
		}
	}
}

void
Cmd_AddCommand ( char *cmd_name, xcommand_t function )
{
	cmd_function_t  *cmd;

	/* fail if the command is a variable name */
	if ( Cvar_VariableString( cmd_name ) [ 0 ] )
	{
		Cmd_RemoveCommand( cmd_name );
	}

	/* fail if the command already exists */
	for ( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if ( !strcmp( cmd_name, cmd->name ) )
		{
			Com_Printf( "Cmd_AddCommand: %s already defined\n", cmd_name );
			return;
		}
	}

	cmd = Z_Malloc( sizeof ( cmd_function_t ) );
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

void
Cmd_RemoveCommand ( char *cmd_name )
{
	cmd_function_t  *cmd, **back;

	back = &cmd_functions;

	while ( 1 )
	{
		cmd = *back;

		if ( !cmd )
		{
			Com_Printf( "Cmd_RemoveCommand: %s not added\n", cmd_name );
			return;
		}

		if ( !strcmp( cmd_name, cmd->name ) )
		{
			*back = cmd->next;
			Z_Free( cmd );
			return;
		}

		back = &cmd->next;
	}
}

qboolean
Cmd_Exists ( char *cmd_name )
{
	cmd_function_t  *cmd;

	for ( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if ( !strcmp( cmd_name, cmd->name ) )
		{
			return ( true );
		}
	}

	return ( false );
}

int qsort_strcomp ( const void *s1, const void *s2 )
{
	return (strcmp(*(char **)s1, *(char **)s2));
}

char *
Cmd_CompleteCommand ( char *partial )
{
	cmd_function_t *cmd;
	int len, i, o, p;
	cmdalias_t     *a;
	cvar_t         *cvar;
	char           *pmatch [ 1024 ];
	qboolean diff = false;

	len = strlen( partial );

	if ( !len )
	{
		return ( NULL );
	}

	/* check for exact match */
	for ( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if ( !strcmp( partial, cmd->name ) )
		{
			return ( cmd->name );
		}
	}

	for ( a = cmd_alias; a; a = a->next )
	{
		if ( !strcmp( partial, a->name ) )
		{
			return ( a->name );
		}
	}

	for ( cvar = cvar_vars; cvar; cvar = cvar->next )
	{
		if ( !strcmp( partial, cvar->name ) )
		{
			return ( cvar->name );
		}
	}

	for ( i = 0; i < 1024; i++ )
	{
		pmatch [ i ] = NULL;
	}

	i = 0;

	/* check for partial match */
	for ( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if ( !strncmp( partial, cmd->name, len ) )
		{
			pmatch [ i ] = cmd->name;
			i++;
		}
	}

	for ( a = cmd_alias; a; a = a->next )
	{
		if ( !strncmp( partial, a->name, len ) )
		{
			pmatch [ i ] = a->name;
			i++;
		}
	}

	for ( cvar = cvar_vars; cvar; cvar = cvar->next )
	{
		if ( !strncmp( partial, cvar->name, len ) )
		{
			pmatch [ i ] = cvar->name;
			i++;
		}
	}

	if ( i )
	{
		if ( i == 1 )
		{
			return ( pmatch [ 0 ] );
		}

		/* Sort it */
        qsort(pmatch, i, sizeof(pmatch[0]), qsort_strcomp);

		Com_Printf( "\n\n", partial );

		for ( o = 0; o < i; o++ )
		{
			Com_Printf( "  %s\n", pmatch [ o ] );
		}

		strcpy( retval, "" );
		p = 0;

		while ( !diff && p < 256 )
		{
			retval [ p ] = pmatch [ 0 ] [ p ];

			for ( o = 0; o < i; o++ )
			{
				if ( p > strlen( pmatch [ o ] ) )
				{
					continue;
				}

				if ( retval [ p ] != pmatch [ o ] [ p ] )
				{
					retval [ p ] = 0;
					diff = true;
				}
			}

			p++;
		}

		return ( retval );
	}

	return ( NULL );
}

qboolean
Cmd_IsComplete ( char *command )
{
	cmd_function_t *cmd;
	cmdalias_t     *a;
	cvar_t         *cvar;

	/* check for exact match */
	for ( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if ( !strcmp( command, cmd->name ) )
		{
			return ( true );
		}
	}

	for ( a = cmd_alias; a; a = a->next )
	{
		if ( !strcmp( command, a->name ) )
		{
			return ( true );
		}
	}

	for ( cvar = cvar_vars; cvar; cvar = cvar->next )
	{
		if ( !strcmp( command, cvar->name ) )
		{
			return ( true );
		}
	}

	return ( false );
}

/*
 * A complete command line has been parsed, so try to execute it
 */
void
Cmd_ExecuteString ( char *text )
{
	cmd_function_t  *cmd;
	cmdalias_t      *a;

	Cmd_TokenizeString( text, true );

	/* execute the command line */
	if ( !Cmd_Argc() )
	{
		return; /* no tokens */
	}

	/* check functions */
	for ( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if ( !Q_strcasecmp( cmd_argv [ 0 ], cmd->name ) )
		{
			if ( !cmd->function )
			{
				/* forward to server command */
				Cmd_ExecuteString( va( "cmd %s", text ) );
			}
			else
			{
				cmd->function();
			}

			return;
		}
	}

	/* check alias */
	for ( a = cmd_alias; a; a = a->next )
	{
		if ( !Q_strcasecmp( cmd_argv [ 0 ], a->name ) )
		{
			if ( ++alias_count == ALIAS_LOOP_COUNT )
			{
				Com_Printf( "ALIAS_LOOP_COUNT\n" );
				return;
			}

			Cbuf_InsertText( a->value );
			return;
		}
	}

	/* check cvars */
	if ( Cvar_Command() )
	{
		return;
	}

#ifndef DEDICATED_ONLY
	/* send it as a server command if we are connected */
	Cmd_ForwardToServer();
#endif
}

void
Cmd_List_f ( void )
{
	cmd_function_t *cmd;
	int i;

	i = 0;

	for ( cmd = cmd_functions; cmd; cmd = cmd->next, i++ )
	{
		Com_Printf( "%s\n", cmd->name );
	}

	Com_Printf( "%i commands\n", i );
}

void
Cmd_Init ( void )
{
	/* register our commands */
	Cmd_AddCommand( "cmdlist", Cmd_List_f );
	Cmd_AddCommand( "exec", Cmd_Exec_f );
	Cmd_AddCommand( "echo", Cmd_Echo_f );
	Cmd_AddCommand( "alias", Cmd_Alias_f );
	Cmd_AddCommand( "wait", Cmd_Wait_f );
}
