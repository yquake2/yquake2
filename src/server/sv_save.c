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
 * Serverside savegame code.
 *
 * =======================================================================
 */

#include "header/server.h"

void CM_ReadPortalState(fileHandle_t f);

/*
 * Delete save/<XXX>/
 */
void
SV_WipeSavegame(char *savename)
{
	char name[MAX_OSPATH];
	char *s;

	Com_DPrintf("SV_WipeSaveGame(%s)\n", savename);

	Com_sprintf(name, sizeof(name), "%s/save/%s/server.ssv",
				FS_Gamedir(), savename);

	Sys_Remove(name);

	Com_sprintf(name, sizeof(name), "%s/save/%s/game.ssv",
				FS_Gamedir(), savename);

	Sys_Remove(name);

	Com_sprintf(name, sizeof(name), "%s/save/%s/*.sav", FS_Gamedir(), savename);
	s = Sys_FindFirst(name, 0, 0);

	while (s)
	{
		Sys_Remove(s);
		s = Sys_FindNext(0, 0);
	}

	Sys_FindClose();
	Com_sprintf(name, sizeof(name), "%s/save/%s/*.sv2", FS_Gamedir(), savename);
	s = Sys_FindFirst(name, 0, 0);

	while (s)
	{
		Sys_Remove(s);
		s = Sys_FindNext(0, 0);
	}

	Sys_FindClose();
}

void
CopyFile(char *src, char *dst)
{
	FILE *f1, *f2;
	size_t l;
	byte buffer[65536];

	Com_DPrintf("CopyFile (%s, %s)\n", src, dst);

	f1 = Q_fopen(src, "rb");

	if (!f1)
	{
		return;
	}

	f2 = Q_fopen(dst, "wb");

	if (!f2)
	{
		fclose(f1);
		return;
	}

	while (1)
	{
		l = fread(buffer, 1, sizeof(buffer), f1);

		if (!l)
		{
			break;
		}

		fwrite(buffer, 1, l, f2);
	}

	fclose(f1);
	fclose(f2);
}

void
SV_CopySaveGame(char *src, char *dst)
{
	char name[MAX_OSPATH], name2[MAX_OSPATH];
	size_t l, len;
	char *found;

	Com_DPrintf("SV_CopySaveGame(%s, %s)\n", src, dst);

	SV_WipeSavegame(dst);

	/* copy the savegame over */
	Com_sprintf(name, sizeof(name), "%s/save/%s/server.ssv", FS_Gamedir(), src);
	Com_sprintf(name2, sizeof(name2), "%s/save/%s/server.ssv", FS_Gamedir(), dst);
	FS_CreatePath(name2);
	CopyFile(name, name2);

	Com_sprintf(name, sizeof(name), "%s/save/%s/game.ssv", FS_Gamedir(), src);
	Com_sprintf(name2, sizeof(name2), "%s/save/%s/game.ssv", FS_Gamedir(), dst);
	CopyFile(name, name2);

	Com_sprintf(name, sizeof(name), "%s/save/%s/", FS_Gamedir(), src);
	len = strlen(name);
	Com_sprintf(name, sizeof(name), "%s/save/%s/*.sav", FS_Gamedir(), src);
	found = Sys_FindFirst(name, 0, 0);

	while (found)
	{
		strcpy(name + len, found + len);

		Com_sprintf(name2, sizeof(name2), "%s/save/%s/%s",
					FS_Gamedir(), dst, found + len);
		CopyFile(name, name2);

		/* change sav to sv2 */
		l = strlen(name);
		strcpy(name + l - 3, "sv2");
		l = strlen(name2);
		strcpy(name2 + l - 3, "sv2");
		CopyFile(name, name2);

		found = Sys_FindNext(0, 0);
	}

	Sys_FindClose();
}

void
SV_WriteLevelFile(void)
{
	char name[MAX_OSPATH];
	char workdir[MAX_OSPATH];
	FILE *f;

	Com_DPrintf("SV_WriteLevelFile()\n");

	Com_sprintf(name, sizeof(name), "%s/save/current/%s.sv2",
				FS_Gamedir(), sv.name);
	f = Q_fopen(name, "wb");

	if (!f)
	{
		Com_Printf("Failed to open %s\n", name);
		return;
	}

	fwrite(sv.configstrings, sizeof(sv.configstrings), 1, f);
	CM_WritePortalState(f);
	fclose(f);

	Com_sprintf(name, sizeof(name), "%s/save/current", FS_Gamedir());
	Sys_GetWorkDir(workdir, sizeof(workdir));
	Sys_Mkdir(name);

	if (!Sys_SetWorkDir(name))
	{
		Com_Printf("Couldn't change to %s\n", name);
		Sys_SetWorkDir(workdir);
		return;
	}

	Com_sprintf(name, sizeof(name), "%s.sav", sv.name);
	ge->WriteLevel(name);

	Sys_SetWorkDir(workdir);
}

void
SV_ReadLevelFile(void)
{
	char name[MAX_OSPATH];
	char workdir[MAX_OSPATH];
	fileHandle_t f;

	Com_DPrintf("SV_ReadLevelFile()\n");

	Com_sprintf(name, sizeof(name), "save/current/%s.sv2", sv.name);
	FS_FOpenFile(name, &f, true);

	if (!f)
	{
		Com_Printf("Failed to open %s\n", name);
		return;
	}

	FS_Read(sv.configstrings, sizeof(sv.configstrings), f);
	CM_ReadPortalState(f);
	FS_FCloseFile(f);

	Com_sprintf(name, sizeof(name), "%s/save/current", FS_Gamedir());
	Sys_GetWorkDir(workdir, sizeof(workdir));

	if (!Sys_SetWorkDir(name))
	{
		Com_Printf("Couldn't change to %s\n", name);
		Sys_SetWorkDir(workdir);
		return;
	}

	Com_sprintf(name, sizeof(name), "%s.sav", sv.name);
	ge->ReadLevel(name);

	Sys_SetWorkDir(workdir);
}

void
SV_WriteServerFile(qboolean autosave)
{
	FILE *f;
	cvar_t *var;
	char name[MAX_OSPATH], string[128];
	char workdir[MAX_OSPATH];
	char comment[32];
	time_t aclock;
	struct tm *newtime;

	Com_DPrintf("SV_WriteServerFile(%s)\n", autosave ? "true" : "false");

	Com_sprintf(name, sizeof(name), "%s/save/current/server.ssv", FS_Gamedir());
	f = Q_fopen(name, "wb");

	if (!f)
	{
		Com_Printf("Couldn't write %s\n", name);
		return;
	}

	/* write the comment field */
	memset(comment, 0, sizeof(comment));

	if (!autosave)
	{
		time(&aclock);
		newtime = localtime(&aclock);
		Com_sprintf(comment, sizeof(comment), "%2i:%i%i %2i/%2i  ",
				newtime->tm_hour, newtime->tm_min / 10,
				newtime->tm_min % 10, newtime->tm_mon + 1,
				newtime->tm_mday);
		Q_strlcat(comment, sv.configstrings[CS_NAME], sizeof(comment));
	}
	else
	{
		/* autosaved */
		Com_sprintf(comment, sizeof(comment), "ENTERING %s",
				sv.configstrings[CS_NAME]);
	}

	fwrite(comment, 1, sizeof(comment), f);

	/* write the mapcmd */
	fwrite(svs.mapcmd, 1, sizeof(svs.mapcmd), f);

	/* write all CVAR_LATCH cvars
	   these will be things like coop,
	   skill, deathmatch, etc */
	for (var = cvar_vars; var; var = var->next)
	{
		char cvarname[LATCH_CVAR_SAVELENGTH] = {0};
		if (!(var->flags & CVAR_LATCH))
		{
			continue;
		}

		if ((strlen(var->name) >= sizeof(cvarname) - 1) ||
			(strlen(var->string) >= sizeof(string) - 1))
		{
			Com_Printf("Cvar too long: %s = %s\n", var->name, var->string);
			continue;
		}

		memset(string, 0, sizeof(string));
		strcpy(cvarname, var->name);
		strcpy(string, var->string);
		fwrite(cvarname, 1, sizeof(cvarname), f);
		fwrite(string, 1, sizeof(string), f);
	}

	fclose(f);

	/* write game state */
	Com_sprintf(name, sizeof(name), "%s/save/current", FS_Gamedir());
	Sys_GetWorkDir(workdir, sizeof(workdir));
	Sys_Mkdir(name);

	if (!Sys_SetWorkDir(name))
	{
		Com_Printf("Couldn't change to %s\n", name);
		Sys_SetWorkDir(workdir);
		return;
	}

	ge->WriteGame("game.ssv", autosave);

	Sys_SetWorkDir(workdir);
}

void
SV_ReadServerFile(void)
{
	fileHandle_t f;
	char name[MAX_OSPATH], string[128];
	char workdir[MAX_OSPATH];
	char comment[32];
	char mapcmd[MAX_TOKEN_CHARS];

	Com_DPrintf("SV_ReadServerFile()\n");

	Com_sprintf(name, sizeof(name), "save/current/server.ssv");
	FS_FOpenFile(name, &f, true);

	if (!f)
	{
		Com_Printf("Couldn't read %s\n", name);
		return;
	}

	/* read the comment field */
	FS_Read(comment, sizeof(comment), f);

	/* read the mapcmd */
	FS_Read(mapcmd, sizeof(mapcmd), f);

	/* read all CVAR_LATCH cvars
	   these will be things like
	   coop, skill, deathmatch, etc */
	while (1)
	{
		char cvarname[LATCH_CVAR_SAVELENGTH] = {0};
		if (!FS_FRead(cvarname, 1, sizeof(cvarname), f))
		{
			break;
		}

		FS_Read(string, sizeof(string), f);
		Com_DPrintf("Set %s = %s\n", cvarname, string);
		Cvar_ForceSet(cvarname, string);
	}

	FS_FCloseFile(f);

	/* start a new game fresh with new cvars */
	SV_InitGame();

	strcpy(svs.mapcmd, mapcmd);

	/* read game state */
	Com_sprintf(name, sizeof(name), "%s/save/current", FS_Gamedir());
	Sys_GetWorkDir(workdir, sizeof(workdir));

	if (!Sys_SetWorkDir(name))
	{
		Com_Printf("Couldn't change to %s\n", name);
		Sys_SetWorkDir(workdir);
		return;
	}

	ge->ReadGame("game.ssv");

	Sys_SetWorkDir(workdir);
}

void
SV_Loadgame_f(void)
{
	char name[MAX_OSPATH];
	FILE *f;
	char *dir;

	if (Cmd_Argc() != 2)
	{
		Com_Printf("USAGE: loadgame <directory>\n");
		return;
	}

	Com_Printf("Loading game...\n");

	dir = Cmd_Argv(1);

	if (strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\"))
	{
		Com_Printf("Bad savedir.\n");
	}

	/* make sure the server.ssv file exists */
	Com_sprintf(name, sizeof(name), "%s/save/%s/server.ssv",
				FS_Gamedir(), Cmd_Argv(1));
	f = Q_fopen(name, "rb");

	if (!f)
	{
		Com_Printf("No such savegame: %s\n", name);
		return;
	}

	fclose(f);

	SV_CopySaveGame(Cmd_Argv(1), "current");

	SV_ReadServerFile();

	/* go to the map */
	sv.state = ss_dead; /* don't save current level when changing */
	SV_Map(false, svs.mapcmd, true);
}

void
SV_Savegame_f(void)
{
	char *dir;

	if (sv.state != ss_game)
	{
		Com_Printf("You must be in a game to save.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Com_Printf("USAGE: savegame <directory>\n");
		return;
	}

	if (Cvar_VariableValue("deathmatch"))
	{
		Com_Printf("Can't savegame in a deathmatch\n");
		return;
	}

	if (!strcmp(Cmd_Argv(1), "current"))
	{
		Com_Printf("Can't save to 'current'\n");
		return;
	}

	if ((maxclients->value == 1) &&
		(svs.clients[0].edict->client->ps.stats[STAT_HEALTH] <= 0))
	{
		Com_Printf("\nCan't savegame while dead!\n");
		return;
	}

	dir = Cmd_Argv(1);

	if (strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\"))
	{
		Com_Printf("Bad savedir.\n");
	}

	Com_Printf("Saving game...\n");

	/* archive current level, including all client edicts.
	   when the level is reloaded, they will be shells awaiting
	   a connecting client */
	SV_WriteLevelFile();

	/* save server state */
	SV_WriteServerFile(false);

	/* copy it off */
	SV_CopySaveGame("current", dir);

	Com_Printf("Done.\n");
}

