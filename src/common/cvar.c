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
 * The Quake II CVAR subsystem. Implements dynamic variable handling.
 *
 * =======================================================================
 */

#include "header/common.h"

cvar_t *cvar_vars;


typedef struct
{
	char *old;
	char *new;
} replacement_t;

/* An ugly hack to rewrite CVARs loaded from config.cfg */
replacement_t replacements[] = {
	{"cd_shuffle", "ogg_shuffle"},
	{"cl_anglekicks", "cl_kickangles"},
	{"cl_drawfps", "cl_showfps"},
	{"gl_drawentities", "r_drawentities"},
	{"gl_drawworld", "r_drawworld"},
	{"gl_fullbright", "r_fullbright"},
	{"gl_lerpmodels", "r_lerpmodels"},
	{"gl_lightlevel", "r_lightlevel"},
	{"gl_norefresh", "r_norefresh"},
	{"gl_novis", "r_novis"},
	{"gl_speeds", "r_speeds"},
	{"gl_clear", "r_clear"},
	{"gl_consolescale", "r_consolescale"},
	{"gl_hudscale", "r_hudscale"},
	{"gl_menuscale", "r_scale"},
	{"gl_customheight", "r_customheight"},
	{"gl_customwidth", "r_customheight"},
	{"gl_dynamic", "gl1_dynamic"},
	{"gl_farsee", "r_farsee"},
	{"gl_flashblend", "gl1_flashblend"},
	{"gl_lockpvs", "r_lockpvs"},
	{"gl_maxfps", "vid_maxfps"},
	{"gl_mode", "r_mode"},
	{"gl_modulate", "r_modulate"},
	{"gl_overbrightbits", "gl1_overbrightbits"},
	{"gl_palettedtextures", "gl1_palettedtextures"},
	{"gl_particle_min_size", "gl1_particle_min_size"},
	{"gl_particle_max_size", "gl1_particle_max_size"},
	{"gl_particle_size", "gl1_particle_size"},
	{"gl_particle_att_a", "gl1_particle_att_a"},
	{"gl_particle_att_b", "gl1_particle_att_b"},
	{"gl_particle_att_c", "gl1_particle_att_c"},
	{"gl_picmip", "gl1_picmip"},
	{"gl_pointparameters", "gl1_pointparameters"},
	{"gl_polyblend", "gl1_polyblend"},
	{"gl_round_down", "gl1_round_down"},
	{"gl_saturatelightning", "gl1_saturatelightning"},
	{"gl_stencilshadows", "gl1_stencilshadows"},
	{"gl_stereo", "gl1_stereo"},
	{"gl_stereo_separation", "gl1_stereo_separation"},
	{"gl_stereo_anaglyph_colors", "gl1_stereo_anaglyph_colors"},
	{"gl_stereo_convergence", "gl1_stereo_convergence"},
	{"gl_swapinterval", "r_vsync"},
	{"gl_texturealphamode", "gl1_texturealphamode"},
	{"gl_texturesolidmode", "gl1_texturesolidmode"},
	{"gl_ztrick", "gl1_ztrick"},
	{"gl_msaa_samples", "r_msaa_samples"},
	{"gl_nolerp_list", "r_nolerp_list"},
	{"gl_retexturing", "r_retexturing"},
	{"gl_shadows", "r_shadows"},
	{"gl_anisotropic", "r_anisotropic"},
	{"intensity", "gl1_intensity"}
};


static qboolean
Cvar_InfoValidate(char *s)
{
	if (strstr(s, "\\"))
	{
		return false;
	}

	if (strstr(s, "\""))
	{
		return false;
	}

	if (strstr(s, ";"))
	{
		return false;
	}

	return true;
}

static cvar_t *
Cvar_FindVar(const char *var_name)
{
	cvar_t *var;
	int i;

	/* An ugly hack to rewrite changed CVARs */
	for (i = 0; i < sizeof(replacements) / sizeof(replacement_t); i++)
	{
		if (!strcmp(var_name, replacements[i].old))
		{
			Com_Printf("cvar %s ist deprecated, use %s instead\n", replacements[i].old, replacements[i].new);

			var_name = replacements[i].new;
		}
	}

	for (var = cvar_vars; var; var = var->next)
	{
		if (!strcmp(var_name, var->name))
		{
			return var;
		}
	}

	return NULL;
}

static qboolean
Cvar_IsFloat(const char *s)
{
	int dot = '.';

	if (*s == '-')
	{
		s++;
	}

	if (!*s)
	{
		return false;
	}

	do {
		int c;

		c = *s++;

		if (c == dot)
		{
			dot = 0;
		}
		else if (c < '0' || c > '9')
		{
			return false;
		}
	} while (*s);

	return true;
}

float
Cvar_VariableValue(char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);

	if (!var)
	{
		return 0;
	}

	return strtod(var->string, (char **)NULL);
}

const char *
Cvar_VariableString(const char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);

	if (!var)
	{
		return "";
	}

	return var->string;
}

/*
 * If the variable already exists, the value will not be set
 * The flags will be or'ed in if the variable exists.
 */
cvar_t *
Cvar_Get(char *var_name, char *var_value, int flags)
{
	cvar_t *var;
	cvar_t **pos;

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate(var_name))
		{
			Com_Printf("invalid info cvar name\n");
			return NULL;
		}
	}

	var = Cvar_FindVar(var_name);

	if (var)
	{
		var->flags |= flags;

		if (!var_value)
		{
			var->default_string = CopyString("");
		}
		else
		{
			var->default_string = CopyString(var_value);
		}

		return var;
	}

	if (!var_value)
	{
		return NULL;
	}

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate(var_value))
		{
			Com_Printf("invalid info cvar value\n");
			return NULL;
		}
	}

	// if $game is the default one ("baseq2"), then use "" instead because
	// other code assumes this behavior (e.g. FS_BuildGameSpecificSearchPath())
	if(strcmp(var_name, "game") == 0 && strcmp(var_value, BASEDIRNAME) == 0)
	{
		var_value = "";
	}

	var = Z_Malloc(sizeof(*var));
	var->name = CopyString(var_name);
	var->string = CopyString(var_value);
	var->default_string = CopyString(var_value);
	var->modified = true;
	var->value = strtod(var->string, (char **)NULL);

	/* link the variable in */
	pos = &cvar_vars;
	while (*pos && strcmp((*pos)->name, var->name) < 0)
	{
		pos = &(*pos)->next;
	}
	var->next = *pos;
	*pos = var;

	var->flags = flags;

	return var;
}

cvar_t *
Cvar_Set2(char *var_name, char *value, qboolean force)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);

	if (!var)
	{
		return Cvar_Get(var_name, value, 0);
	}

	if (var->flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate(value))
		{
			Com_Printf("invalid info cvar value\n");
			return var;
		}
	}

	// if $game is the default one ("baseq2"), then use "" instead because
	// other code assumes this behavior (e.g. FS_BuildGameSpecificSearchPath())
	if(strcmp(var_name, "game") == 0 && strcmp(value, BASEDIRNAME) == 0)
	{
		value = "";
	}

	if (!force)
	{
		if (var->flags & CVAR_NOSET)
		{
			Com_Printf("%s is write protected.\n", var_name);
			return var;
		}

		if (var->flags & CVAR_LATCH)
		{
			if (var->latched_string)
			{
				if (strcmp(value, var->latched_string) == 0)
				{
					return var;
				}

				Z_Free(var->latched_string);
				var->latched_string = NULL;
			}
			else
			{
				if (strcmp(value, var->string) == 0)
				{
					return var;
				}
			}

			if (Com_ServerState())
			{
				Com_Printf("%s will be changed for next game.\n", var_name);
				var->latched_string = CopyString(value);
			}
			else
			{
				var->string = CopyString(value);
				var->value = (float)strtod(var->string, (char **)NULL);

				if (!strcmp(var->name, "game"))
				{
					FS_BuildGameSpecificSearchPath(var->string);
				}
			}

			return var;
		}
	}
	else
	{
		if (var->latched_string)
		{
			Z_Free(var->latched_string);
			var->latched_string = NULL;
		}
	}

	if (!strcmp(value, var->string))
	{
		return var;
	}

	var->modified = true;

	if (var->flags & CVAR_USERINFO)
	{
		userinfo_modified = true;
	}

	Z_Free(var->string);

	var->string = CopyString(value);
	var->value = strtod(var->string, (char **)NULL);

	return var;
}

cvar_t *
Cvar_ForceSet(char *var_name, char *value)
{
	return Cvar_Set2(var_name, value, true);
}

cvar_t *
Cvar_Set(char *var_name, char *value)
{
	return Cvar_Set2(var_name, value, false);
}

cvar_t *
Cvar_FullSet(char *var_name, char *value, int flags)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);

	if (!var)
	{
		return Cvar_Get(var_name, value, flags);
	}

	var->modified = true;

	if (var->flags & CVAR_USERINFO)
	{
		userinfo_modified = true;
	}

	// if $game is the default one ("baseq2"), then use "" instead because
	// other code assumes this behavior (e.g. FS_BuildGameSpecificSearchPath())
	if(strcmp(var_name, "game") == 0 && strcmp(value, BASEDIRNAME) == 0)
	{
		value = "";
	}

	Z_Free(var->string);

	var->string = CopyString(value);
	var->value = (float)strtod(var->string, (char **)NULL);

	var->flags = flags;

	return var;
}

void
Cvar_SetValue(char *var_name, float value)
{
	char val[32];

	if (value == (int)value)
	{
		Com_sprintf(val, sizeof(val), "%i", (int)value);
	}

	else
	{
		Com_sprintf(val, sizeof(val), "%f", value);
	}

	Cvar_Set(var_name, val);
}

/*
 * Any variables with latched values will now be updated
 */
void
Cvar_GetLatchedVars(void)
{
	cvar_t *var;

	for (var = cvar_vars; var; var = var->next)
	{
		if (!var->latched_string)
		{
			continue;
		}

		Z_Free(var->string);
		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = strtod(var->string, (char **)NULL);

		if (!strcmp(var->name, "game"))
		{
			FS_BuildGameSpecificSearchPath(var->string);
		}
	}
}

/*
 * Handles variable inspection and changing from the console
 */
qboolean
Cvar_Command(void)
{
	cvar_t *v;

	/* check variables */
	v = Cvar_FindVar(Cmd_Argv(0));

	if (!v)
	{
		return false;
	}

	/* perform a variable print or set */
	if (Cmd_Argc() == 1)
	{
		Com_Printf("\"%s\" is \"%s\"\n", v->name, v->string);
		return true;
	}

	/* Another evil hack: The user has just changed 'game' trough
	   the console. We reset userGivenGame to that value, otherwise
	   we would revert to the initialy given game at disconnect. */
	if (strcmp(v->name, "game") == 0)
	{
		Q_strlcpy(userGivenGame, Cmd_Argv(1), sizeof(userGivenGame));
	}

	Cvar_Set(v->name, Cmd_Argv(1));
	return true;
}

/*
 * Allows setting and defining of arbitrary cvars from console
 */
void
Cvar_Set_f(void)
{
	char *firstarg;
	int c, i;

	c = Cmd_Argc();

	if ((c != 3) && (c != 4))
	{
		Com_Printf("usage: set <variable> <value> [u / s]\n");
		return;
	}

	firstarg = Cmd_Argv(1);

	/* An ugly hack to rewrite changed CVARs */
	for (i = 0; i < sizeof(replacements) / sizeof(replacement_t); i++)
	{
		if (!strcmp(firstarg, replacements[i].old))
		{
			firstarg = replacements[i].new;
		}
	}

	if (c == 4)
	{
		int flags;

		if (!strcmp(Cmd_Argv(3), "u"))
		{
			flags = CVAR_USERINFO;
		}

		else if (!strcmp(Cmd_Argv(3), "s"))
		{
			flags = CVAR_SERVERINFO;
		}

		else
		{
			Com_Printf("flags can only be 'u' or 's'\n");
			return;
		}

		Cvar_FullSet(firstarg, Cmd_Argv(2), flags);
	}
	else
	{
		Cvar_Set(firstarg, Cmd_Argv(2));
	}
}

/*
 * Appends lines containing "set variable value" for all variables
 * with the archive flag set to true.
 */
void
Cvar_WriteVariables(char *path)
{
	cvar_t *var;
	char buffer[1024];
	FILE *f;

	f = Q_fopen(path, "a");

	for (var = cvar_vars; var; var = var->next)
	{
		if (var->flags & CVAR_ARCHIVE)
		{
			Com_sprintf(buffer, sizeof(buffer), "set %s \"%s\"\n",
					var->name, var->string);
			fprintf(f, "%s", buffer);
		}
	}

	fflush(f);
	fclose(f);
}

void
Cvar_List_f(void)
{
	cvar_t *var;
	int i;

	i = 0;

	for (var = cvar_vars; var; var = var->next, i++)
	{
		if (var->flags & CVAR_ARCHIVE)
		{
			Com_Printf("*");
		}

		else
		{
			Com_Printf(" ");
		}

		if (var->flags & CVAR_USERINFO)
		{
			Com_Printf("U");
		}

		else
		{
			Com_Printf(" ");
		}

		if (var->flags & CVAR_SERVERINFO)
		{
			Com_Printf("S");
		}

		else
		{
			Com_Printf(" ");
		}

		if (var->flags & CVAR_NOSET)
		{
			Com_Printf("-");
		}

		else if (var->flags & CVAR_LATCH)
		{
			Com_Printf("L");
		}

		else
		{
			Com_Printf(" ");
		}

		Com_Printf(" %s \"%s\"\n", var->name, var->string);
	}

	Com_Printf("%i cvars\n", i);
}

qboolean userinfo_modified;

char *
Cvar_BitInfo(int bit)
{
	static char info[MAX_INFO_STRING];
	cvar_t *var;

	info[0] = 0;

	for (var = cvar_vars; var; var = var->next)
	{
		if (var->flags & bit)
		{
			Info_SetValueForKey(info, var->name, var->string);
		}
	}

	return info;
}

/*
 * returns an info string containing
 * all the CVAR_USERINFO cvars
 */
char *
Cvar_Userinfo(void)
{
	return Cvar_BitInfo(CVAR_USERINFO);
}

/*
 * returns an info string containing
 * all the CVAR_SERVERINFO cvars
 */
char *
Cvar_Serverinfo(void)
{
	return Cvar_BitInfo(CVAR_SERVERINFO);
}

/*
 * Increments the given cvar by 1 or adds the
 * optional given float value to it.
 */
void Cvar_Inc_f(void)
{
	char string[MAX_QPATH];
    cvar_t *var;
    float value;

    if (Cmd_Argc() < 2)
	{
        Com_Printf("Usage: %s <cvar> [value]\n", Cmd_Argv(0));
        return;
    }

    var = Cvar_FindVar(Cmd_Argv(1));

    if (!var)
	{
        Com_Printf("%s is not a cvar\n", Cmd_Argv(1));
        return;
    }

    if (!Cvar_IsFloat(var->string))
	{
        Com_Printf("\"%s\" is \"%s\", can't %s\n", var->name, var->string, Cmd_Argv(0));
        return;
    }

    value = 1;

    if (Cmd_Argc() > 2) {
        value = atof(Cmd_Argv(2));
    }

    if (!strcmp(Cmd_Argv(0), "dec")) {
        value = -value;
    }

	Com_sprintf(string, sizeof(string), "%f", var->value + value);
    Cvar_Set(var->name, string);
}

/*
 * Resets a cvar to its default value.
 */
void Cvar_Reset_f(void)
{
    cvar_t *var;

    if (Cmd_Argc() < 2)
	{
        Com_Printf("Usage: %s <cvar>\n", Cmd_Argv(0));
        return;
    }

    var = Cvar_FindVar(Cmd_Argv(1));

    if (!var)
	{
        Com_Printf("%s is not a cvar\n", Cmd_Argv(1));
        return;
    }

	Com_Printf("%s: %s\n", var->name, var->default_string);
    Cvar_Set(var->name, var->default_string);
}

/*
 * Resets all known cvar (with the exception of `game') to
 * their default values.
 */
void Cvar_ResetAll_f(void)
{
    cvar_t *var;

    for (var = cvar_vars; var; var = var->next)
	{
        if ((var->flags & CVAR_NOSET))
		{
            continue;
		}
		else if (strcmp(var->name, "game") == 0)
		{
            continue;
		}

        Cvar_Set(var->name, var->default_string);
    }
}

/*
 * Toggles a cvar between 0 and 1 or the given values.
 */
void Cvar_Toggle_f(void)
{
    cvar_t *var;
    int i, argc = Cmd_Argc();

    if (argc < 2)
	{
        Com_Printf("Usage: %s <cvar> [values]\n", Cmd_Argv(0));
        return;
    }

    var = Cvar_FindVar(Cmd_Argv(1));

    if (!var)
	{
        Com_Printf("%s is not a cvar\n", Cmd_Argv(1));
        return;
    }

    if (argc < 3)
	{
        if (!strcmp(var->string, "0"))
		{
            Cvar_Set(var->name, "1");
        }
		else if (!strcmp(var->string, "1"))
		{
            Cvar_Set(var->name, "0");
        }
		else
		{
            Com_Printf("\"%s\" is \"%s\", can't toggle\n", var->name, var->string);
        }

        return;
    }

    for (i = 0; i < argc - 2; i++)
	{
        if (!Q_stricmp(var->string, Cmd_Argv(2 + i)))
		{
            i = (i + 1) % (argc - 2);
            Cvar_Set(var->name, Cmd_Argv(2 + i));

            return;
        }
    }

    Com_Printf("\"%s\" is \"%s\", can't cycle\n", var->name, var->string);
}

/*
 * Reads in all archived cvars
 */
void
Cvar_Init(void)
{
	Cmd_AddCommand("cvarlist", Cvar_List_f);
	Cmd_AddCommand("dec", Cvar_Inc_f);
	Cmd_AddCommand("inc", Cvar_Inc_f);
	Cmd_AddCommand("reset", Cvar_Reset_f);
	Cmd_AddCommand("resetall", Cvar_ResetAll_f);
	Cmd_AddCommand("set", Cvar_Set_f);
	Cmd_AddCommand("toggle", Cvar_Toggle_f);
}

/*
 * Free list of cvars
 */
void
Cvar_Fini(void)
{
	cvar_t *var;

	for (var = cvar_vars; var;)
	{
		cvar_t *c = var->next;
		Z_Free(var->string);
		Z_Free(var->name);
		Z_Free(var);
        var = c;
	}

	Cmd_RemoveCommand("cvarlist");
	Cmd_RemoveCommand("dec");
	Cmd_RemoveCommand("inc");
	Cmd_RemoveCommand("reset");
	Cmd_RemoveCommand("resetall");
	Cmd_RemoveCommand("set");
	Cmd_RemoveCommand("toggle");
}

