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
 * Item spawning.
 *
 * =======================================================================
 */

#include "header/local.h"
#include "savegame/tables/spawnfunc_decs.h"

typedef struct
{
	const char *name;
	void (*spawn)(edict_t *ent);
} spawn_t;

static spawn_t spawns[] = {
#include "savegame/tables/spawnfunc_list.h"
};

/*
 * Finds the spawn function for
 * the entity and calls it
 */
void
ED_CallSpawn(edict_t *ent)
{
	const spawn_t *s;
	gitem_t *item;
	int i;

	if (!ent)
	{
		return;
	}

	if (!ent->classname)
	{
		gi.dprintf("%s: NULL classname\n", __func__);
		G_FreeEdict(ent);
		return;
	}

	/* check item spawn functions */
	for (i = 0, item = itemlist; i < itemlist_len; i++, item++)
	{
		if (!item->classname)
		{
			continue;
		}

		if (!strcmp(item->classname, ent->classname))
		{
			/* found it */
			SpawnItem(ent, item);
			return;
		}
	}

	/* check normal spawn functions */
	for (s = spawns; s->name; s++)
	{
		if (!strcmp(s->name, ent->classname))
		{
			/* found it */
			s->spawn(ent);
			return;
		}
	}

	gi.dprintf("%s doesn't have a spawn function\n", ent->classname);
}

char *
ED_NewString(const char *string)
{
	char *newb, *new_p;
	size_t i, l;

	if (!string)
	{
		return NULL;
	}

	l = strlen(string) + 1;

	newb = gi.TagMalloc(l, TAG_LEVEL);

	new_p = newb;

	for (i = 0; i < l; i++)
	{
		if ((string[i] == '\\') && (i < l - 1))
		{
			i++;

			if (string[i] == 'n')
			{
				*new_p++ = '\n';
			}
			else
			{
				*new_p++ = '\\';
			}
		}
		else
		{
			*new_p++ = string[i];
		}
	}

	return newb;
}

/*
 * Takes a key/value pair and sets
 * the binary values in an edict
 */
static void
ED_ParseField(const char *key, const char *value, edict_t *ent)
{
	field_t *f;
	byte *b;
	float v;
	vec3_t vec;

	if (!ent || !value || !key)
	{
		return;
	}

	for (f = fields; f->name; f++)
	{
		if (!(f->flags & FFL_NOSPAWN) && !Q_strcasecmp(f->name, (char *)key))
		{
			/* found it */
			if (f->flags & FFL_SPAWNTEMP)
			{
				b = (byte *)&st;
			}
			else
			{
				b = (byte *)ent;
			}

			switch (f->type)
			{
				case F_LSTRING:
					*(char **)(b + f->ofs) = ED_NewString(value);
					break;
				case F_VECTOR:
					sscanf(value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
					((float *)(b + f->ofs))[0] = vec[0];
					((float *)(b + f->ofs))[1] = vec[1];
					((float *)(b + f->ofs))[2] = vec[2];
					break;
				case F_INT:
					*(int *)(b + f->ofs) = (int)strtol(value, (char **)NULL, 10);
					break;
				case F_FLOAT:
					*(float *)(b + f->ofs) = (float)strtod(value, (char **)NULL);
					break;
				case F_ANGLEHACK:
					v = (float)strtod(value, (char **)NULL);
					((float *)(b + f->ofs))[0] = 0;
					((float *)(b + f->ofs))[1] = v;
					((float *)(b + f->ofs))[2] = 0;
					break;
				case F_IGNORE:
					break;
				default:
					break;
			}

			return;
		}
	}

	gi.dprintf("'%s' is not a field. Value is '%s'\n", key, value);
}

/*
 * Parses an edict out of the given string,
 * returning the new position. ed should be
 * a properly initialized empty edict.
 */
static char *
ED_ParseEdict(char *data, edict_t *ent)
{
	qboolean init;

	if (!ent)
	{
		return NULL;
	}

	init = false;
	memset(&st, 0, sizeof(st));

	/* go through all the dictionary pairs */
	while (1)
	{
		const char *com_token;
		char keyname[256];

		/* parse key */
		com_token = COM_Parse(&data);

		if (com_token[0] == '}')
		{
			break;
		}

		if (!data)
		{
			gi.error("ED_ParseEntity: EOF without closing brace");
			break;
		}

		Q_strlcpy(keyname, com_token, sizeof(keyname));

		/* parse value */
		com_token = COM_Parse(&data);

		if (!data)
		{
			gi.error("%s: EOF without closing brace", __func__);
			break;
		}

		if (com_token[0] == '}')
		{
			gi.error("%s: closing brace without data", __func__);
			break;
		}

		init = true;

		/* keynames with a leading underscore are
		   used for utility comments, and are
		   immediately discarded by quake */
		if (keyname[0] == '_')
		{
			continue;
		}

		ED_ParseField(keyname, com_token, ent);
	}

	if (!init)
	{
		memset(ent, 0, sizeof(*ent));
	}

	return data;
}

/*
 * Chain together all entities with a matching team field.
 *
 * All but the first will have the FL_TEAMSLAVE flag set.
 * All but the last will have the teamchain field set to the next one
 */
static void
G_FindTeams(void)
{
	edict_t *e, *e2, *chain;
	int i, j;
	int c, c2;

	c = 0;
	c2 = 0;

	for (i = 1, e = g_edicts + i; i < globals.num_edicts; i++, e++)
	{
		if (!e->inuse)
		{
			continue;
		}

		if (!e->team)
		{
			continue;
		}

		if (e->flags & FL_TEAMSLAVE)
		{
			continue;
		}

		chain = e;
		e->teammaster = e;
		c++;
		c2++;

		for (j = i + 1, e2 = e + 1; j < globals.num_edicts; j++, e2++)
		{
			if (!e2->inuse)
			{
				continue;
			}

			if (!e2->team)
			{
				continue;
			}

			if (e2->flags & FL_TEAMSLAVE)
			{
				continue;
			}

			if (!strcmp(e->team, e2->team))
			{
				c2++;
				chain->teamchain = e2;
				e2->teammaster = e;
				chain = e2;
				e2->flags |= FL_TEAMSLAVE;
			}
		}
	}

	gi.dprintf("%i teams with %i entities.\n", c, c2);
}

/*
 * Creates a server's entity / program execution context by
 * parsing textual entity definitions out of an ent file.
 */
void
SpawnEntities(const char *mapname, char *entities, const char *spawnpoint)
{
	edict_t *ent;
	int inhibit;
	const char *com_token;
	int i;
	float skill_level;

	if (!mapname || !entities || !spawnpoint)
	{
		return;
	}

	skill_level = floor(skill->value);

	if (skill_level < 0)
	{
		skill_level = 0;
	}

	if (skill_level > 3)
	{
		skill_level = 3;
	}

	if (skill->value != skill_level)
	{
		gi.cvar_forceset("skill", va("%f", skill_level));
	}

	SaveClientData();

	gi.FreeTags(TAG_LEVEL);

	memset(&level, 0, sizeof(level));
	memset(g_edicts, 0, game.maxentities * sizeof(g_edicts[0]));

	Q_strlcpy(level.mapname, mapname, sizeof(level.mapname));
	Q_strlcpy(game.spawnpoint, spawnpoint, sizeof(game.spawnpoint));

	/* set client fields on player ents */
	for (i = 0; i < game.maxclients; i++)
	{
		g_edicts[i + 1].client = game.clients + i;
	}

	ent = NULL;
	inhibit = 0;

	/* parse ents */
	while (1)
	{
		/* parse the opening brace */
		com_token = COM_Parse(&entities);

		if (!entities)
		{
			break;
		}

		if (com_token[0] != '{')
		{
			gi.error("%s: found %s when expecting {", __func__, com_token);
			break;
		}

		if (!ent)
		{
			ent = g_edicts;
		}
		else
		{
			ent = G_Spawn();
		}

		entities = ED_ParseEdict(entities, ent);

		/* yet another map hack */
		if (!Q_stricmp(level.mapname, "command") &&
			!Q_stricmp(ent->classname, "trigger_once") &&
			!Q_stricmp(ent->model, "*27"))
		{
			ent->spawnflags &= ~SPAWNFLAG_NOT_HARD;
		}

		/* remove things (except the world) from
		   different skill levels or deathmatch */
		if (ent != g_edicts)
		{
			if (deathmatch->value)
			{
				if (ent->spawnflags & SPAWNFLAG_NOT_DEATHMATCH)
				{
					G_FreeEdict(ent);
					inhibit++;
					continue;
				}
			}
			else
			{
				if (((skill->value == SKILL_EASY) &&
					 (ent->spawnflags & SPAWNFLAG_NOT_EASY)) ||
					((skill->value == SKILL_MEDIUM) &&
					 (ent->spawnflags & SPAWNFLAG_NOT_MEDIUM)) ||
					(((skill->value == SKILL_HARD) ||
					  (skill->value == SKILL_HARDPLUS)) &&
					 (ent->spawnflags & SPAWNFLAG_NOT_HARD))
					)
				{
					G_FreeEdict(ent);
					inhibit++;
					continue;
				}
			}

			ent->spawnflags &=
				~(SPAWNFLAG_NOT_EASY | SPAWNFLAG_NOT_MEDIUM |
				  SPAWNFLAG_NOT_HARD |
				  SPAWNFLAG_NOT_COOP | SPAWNFLAG_NOT_DEATHMATCH);
		}

		ED_CallSpawn(ent);
	}

	/* in case the last entity in the entstring has spawntemp fields */
	memset(&st, 0, sizeof(st));

	gi.dprintf("%i entities inhibited.\n", inhibit);

	G_FindTeams();

	PlayerTrail_Init();
}

/* =================================================================== */

static char *single_statusbar =
	"yb	-24 "

/* health */
	"xv	0 "
	"hnum "
	"xv	50 "
	"pic 0 "

/* ammo */
	"if 2 "
	"	xv	100 "
	"	anum "
	"	xv	150 "
	"	pic 2 "
	"endif "

/* armor */
	"if 4 "
	"	xv	200 "
	"	rnum "
	"	xv	250 "
	"	pic 4 "
	"endif "

/* selected item */
	"if 6 "
	"	xv	296 "
	"	pic 6 "
	"endif "

	"yb	-50 "

/* picked up item */
	"if 7 "
	"	xv	0 "
	"	pic 7 "
	"	xv	26 "
	"	yb	-42 "
	"	stat_string 8 "
	"	yb	-50 "
	"endif "

/* timer */
	"if 9 "
	"	xv	262 "
	"	num	2	10 "
	"	xv	296 "
	"	pic	9 "
	"endif "

/*  help / weapon icon */
	"if 11 "
	"	xv	148 "
	"	pic	11 "
	"endif "
;

static char *dm_statusbar =
	"yb	-24 "

/* health */
	"xv	0 "
	"hnum "
	"xv	50 "
	"pic 0 "

/* ammo */
	"if 2 "
	"	xv	100 "
	"	anum "
	"	xv	150 "
	"	pic 2 "
	"endif "

/* armor */
	"if 4 "
	"	xv	200 "
	"	rnum "
	"	xv	250 "
	"	pic 4 "
	"endif "

/* selected item */
	"if 6 "
	"	xv	296 "
	"	pic 6 "
	"endif "

	"yb	-50 "

/* picked up item */
	"if 7 "
	"	xv	0 "
	"	pic 7 "
	"	xv	26 "
	"	yb	-42 "
	"	stat_string 8 "
	"	yb	-50 "
	"endif "

/* timer */
	"if 9 "
	"	xv	246 "
	"	num	2	10 "
	"	xv	296 "
	"	pic	9 "
	"endif "

/* help / weapon icon */
	"if 11 "
	"	xv	148 "
	"	pic	11 "
	"endif "

/* frags */
	"xr	-50 "
	"yt 2 "
	"num 3 14 "

/* spectator */
	"if 17 "
	"xv 0 "
	"yb -58 "
	"string2 \"SPECTATOR MODE\" "
	"endif "

/* chase camera */
	"if 16 "
	"xv 0 "
	"yb -68 "
	"string \"Chasing\" "
	"xv 64 "
	"stat_string 16 "
	"endif "
;

/*QUAKED worldspawn (0 0 0) ?
 *
 * Only used for the world.
 *  "sky"       environment map name
 *  "skyaxis"	vector axis for rotating sky
 *  "skyrotate"	speed of rotation in degrees/second
 *  "sounds"	music cd track number
 *  "gravity"	800 is default gravity
 *  "message"	text to print at user logon
 */
void
SP_worldspawn(edict_t *ent)
{
	if (!ent)
	{
		return;
	}

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	ent->inuse = true; /* since the world doesn't use G_Spawn() */
	ent->s.modelindex = 1; /* world model is always index 1 */

	/* --------------- */

	/* reserve some spots for dead
	   player bodies for coop / deathmatch */
	InitBodyQue();

	/* set configstrings for items */
	SetItemNames();

	if (st.nextmap)
	{
		Q_strlcpy(level.nextmap, st.nextmap, sizeof(level.nextmap));
	}

	/* make some data visible to the server */
	if (ent->message && ent->message[0])
	{
		gi.configstring(CS_NAME, ent->message);
		Q_strlcpy(level.level_name, ent->message, sizeof(level.level_name));
	}
	else
	{
		Q_strlcpy(level.level_name, level.mapname, sizeof(level.level_name));
	}

	if (st.sky && st.sky[0])
	{
		gi.configstring(CS_SKY, st.sky);
	}
	else
	{
		gi.configstring(CS_SKY, "unit1_");
	}

	gi.configstring(CS_SKYROTATE, va("%f", st.skyrotate));

	gi.configstring(CS_SKYAXIS, va("%f %f %f",
				st.skyaxis[0], st.skyaxis[1], st.skyaxis[2]));

	if (st.music && st.music[0])
	{
		gi.configstring(CS_CDTRACK, st.music);
	}
	else
	{
		gi.configstring(CS_CDTRACK, va("%i", ent->sounds));
	}

	gi.configstring(CS_MAXCLIENTS, va("%i", (int)(maxclients->value)));

	/* status bar program */
	if (deathmatch->value)
	{
		gi.configstring(CS_STATUSBAR, dm_statusbar);
	}
	else
	{
		gi.configstring(CS_STATUSBAR, single_statusbar);
	}

	/* --------------- */

	/* help icon for statusbar */
	gi.imageindex("i_help");
	level.pic_health = gi.imageindex("i_health");
	gi.imageindex("help");
	gi.imageindex("field_3");

	if (!st.gravity)
	{
		gi.cvar_set("sv_gravity", "800");
	}
	else
	{
		gi.cvar_set("sv_gravity", st.gravity);
	}

	snd_fry = gi.soundindex("player/fry.wav"); /* standing in lava / slime */

	PrecacheItem(FindItem("Blaster"));

	gi.soundindex("player/lava1.wav");
	gi.soundindex("player/lava2.wav");

	gi.soundindex("misc/pc_up.wav");
	gi.soundindex("misc/talk1.wav");

	gi.soundindex("misc/udeath.wav");

	/* gibs */
	gi.soundindex("items/respawn1.wav");

	/* sexed sounds */
	gi.soundindex("*death1.wav");
	gi.soundindex("*death2.wav");
	gi.soundindex("*death3.wav");
	gi.soundindex("*death4.wav");
	gi.soundindex("*fall1.wav");
	gi.soundindex("*fall2.wav");
	gi.soundindex("*gurp1.wav"); /* drowning damage */
	gi.soundindex("*gurp2.wav");
	gi.soundindex("*jump1.wav"); /* player jump */
	gi.soundindex("*pain25_1.wav");
	gi.soundindex("*pain25_2.wav");
	gi.soundindex("*pain50_1.wav");
	gi.soundindex("*pain50_2.wav");
	gi.soundindex("*pain75_1.wav");
	gi.soundindex("*pain75_2.wav");
	gi.soundindex("*pain100_1.wav");
	gi.soundindex("*pain100_2.wav");

	/* sexed models: THIS ORDER MUST MATCH THE DEFINES IN g_local.h
	   you can add more, max 19 (pete change)these models are only
	   loaded in coop or deathmatch. not singleplayer. */
	if (coop->value || deathmatch->value)
	{
		gi.modelindex("#w_blaster.md2");
		gi.modelindex("#w_shotgun.md2");
		gi.modelindex("#w_sshotgun.md2");
		gi.modelindex("#w_machinegun.md2");
		gi.modelindex("#w_chaingun.md2");
		gi.modelindex("#a_grenades.md2");
		gi.modelindex("#w_glauncher.md2");
		gi.modelindex("#w_rlauncher.md2");
		gi.modelindex("#w_hyperblaster.md2");
		gi.modelindex("#w_railgun.md2");
		gi.modelindex("#w_bfg.md2");
	}

	/* ------------------- */

	gi.soundindex("player/gasp1.wav"); /* gasping for air */
	gi.soundindex("player/gasp2.wav"); /* head breaking surface, not gasping */

	gi.soundindex("player/watr_in.wav"); /* feet hitting water */
	gi.soundindex("player/watr_out.wav"); /* feet leaving water */

	gi.soundindex("player/watr_un.wav"); /* head going underwater */

	gi.soundindex("player/u_breath1.wav");
	gi.soundindex("player/u_breath2.wav");

	gi.soundindex("items/pkup.wav"); /* bonus item pickup */
	gi.soundindex("world/land.wav"); /* landing thud */
	gi.soundindex("misc/h2ohit1.wav"); /* landing splash */

	gi.soundindex("items/damage.wav");
	gi.soundindex("items/protect.wav");
	gi.soundindex("items/protect4.wav");
	gi.soundindex("weapons/noammo.wav");

	gi.soundindex("infantry/inflies1.wav");

	sm_meat_index = gi.modelindex("models/objects/gibs/sm_meat/tris.md2");
	gi.modelindex("models/objects/gibs/arm/tris.md2");
	gi.modelindex("models/objects/gibs/bone/tris.md2");
	gi.modelindex("models/objects/gibs/bone2/tris.md2");
	gi.modelindex("models/objects/gibs/chest/tris.md2");
	gi.modelindex("models/objects/gibs/skull/tris.md2");
	gi.modelindex("models/objects/gibs/head2/tris.md2");

	/* Setup light animation tables. 'a'
	   is total darkness, 'z' is doublebright. */

	/* 0 normal */
	gi.configstring(CS_LIGHTS + 0, "m");

	/* 1 FLICKER (first variety) */
	gi.configstring(CS_LIGHTS + 1, "mmnmmommommnonmmonqnmmo");

	/* 2 SLOW STRONG PULSE */
	gi.configstring(CS_LIGHTS + 2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");

	/* 3 CANDLE (first variety) */
	gi.configstring(CS_LIGHTS + 3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg");

	/* 4 FAST STROBE */
	gi.configstring(CS_LIGHTS + 4, "mamamamamama");

	/* 5 GENTLE PULSE 1 */
	gi.configstring(CS_LIGHTS + 5, "jklmnopqrstuvwxyzyxwvutsrqponmlkj");

	/* 6 FLICKER (second variety) */
	gi.configstring(CS_LIGHTS + 6, "nmonqnmomnmomomno");

	/* 7 CANDLE (second variety) */
	gi.configstring(CS_LIGHTS + 7, "mmmaaaabcdefgmmmmaaaammmaamm");

	/* 8 CANDLE (third variety) */
	gi.configstring(CS_LIGHTS + 8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");

	/* 9 SLOW STROBE (fourth variety) */
	gi.configstring(CS_LIGHTS + 9, "aaaaaaaazzzzzzzz");

	/* 10 FLUORESCENT FLICKER */
	gi.configstring(CS_LIGHTS + 10, "mmamammmmammamamaaamammma");

	/* 11 SLOW PULSE NOT FADE TO BLACK */
	gi.configstring(CS_LIGHTS + 11, "abcdefghijklmnopqrrqponmlkjihgfedcba");

	/* styles 32-62 are assigned by the light program for switchable lights */

	/* 63 testing */
	gi.configstring(CS_LIGHTS + 63, "a");
}
