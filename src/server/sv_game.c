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
 * Interface between the server and the game module.
 *
 * =======================================================================
 */

#include "header/server.h"

#ifndef DEDICATED_ONLY
void SCR_DebugGraph(float value, int color);
#endif

game_export_t *ge;

/*
 * Sends the contents of the mutlicast buffer to a single client
 */
static void
PF_Unicast(edict_t *ent, qboolean reliable)
{
	int p;
	client_t *client;

	if (!ent)
	{
		return;
	}

	p = NUM_FOR_EDICT(ent);

	if ((p < 1) || (p > maxclients->value))
	{
		return;
	}

	client = svs.clients + (p - 1);

	if (reliable)
	{
		SZ_Write(&client->netchan.message, sv.multicast.data,
				sv.multicast.cursize);
	}
	else
	{
		SZ_Write(&client->datagram, sv.multicast.data, sv.multicast.cursize);
	}

	SZ_Clear(&sv.multicast);
}

/*
 * Debug print to server console
 */
static void
PF_dprintf(const char *fmt, ...)
{
	char msg[1024];
	va_list argptr;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_Printf("%s", msg);
}

/*
 * Print to a single client
 */
static void
PF_cprintf(edict_t *ent, int level, const char *fmt, ...)
{
	char msg[1024];
	va_list argptr;
	int n;

	n = 0;

	if (ent)
	{
		n = NUM_FOR_EDICT(ent);

		if ((n < 1) || (n > maxclients->value))
		{
			Com_Error(ERR_DROP, "cprintf to a non-client");
		}
	}

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	if (ent)
	{
		SV_ClientPrintf(svs.clients + (n - 1), level, "%s", msg);
	}
	else
	{
		Com_Printf("%s", msg);
	}
}

/*
 * centerprint to a single client
 */
static void
PF_centerprintf(edict_t *ent, const char *fmt, ...)
{
	char msg[1024];
	va_list argptr;
	int n;

	n = NUM_FOR_EDICT(ent);

	if ((n < 1) || (n > maxclients->value))
	{
		return;
	}

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	MSG_WriteByte(&sv.multicast, svc_centerprint);
	MSG_WriteString(&sv.multicast, msg);
	PF_Unicast(ent, true);
}

/*
 * Abort the server with a game error
 */
YQ2_ATTR_NORETURN_FUNCPTR static void
PF_error(const char *fmt, ...)
{
	char msg[1024];
	va_list argptr;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_Error(ERR_DROP, "Game Error: %s", msg);
}

/*
 * Also sets mins and maxs for inline bmodels
 */
static void
PF_setmodel(edict_t *ent, char *name)
{
	int i;

	if (!name)
	{
		Com_Error(ERR_DROP, "PF_setmodel: NULL");
	}

	i = SV_ModelIndex(name);

	ent->s.modelindex = i;

	/* if it is an inline model, get
	   the size information for it */
	if (name[0] == '*')
	{
		cmodel_t *mod;

		mod = CM_InlineModel(name);
		VectorCopy(mod->mins, ent->mins);
		VectorCopy(mod->maxs, ent->maxs);
		SV_LinkEdict(ent);
	}
}

static void
PF_Configstring(int index, char *val)
{
	if ((index < 0) || (index >= MAX_CONFIGSTRINGS))
	{
		Com_Error(ERR_DROP, "configstring: bad index %i\n", index);
	}

	if (!val)
	{
		val = "";
	}

	/* change the string in sv */
	strcpy(sv.configstrings[index], val);

	if (sv.state != ss_loading)
	{
		/* send the update to everyone */
		SZ_Clear(&sv.multicast);
		MSG_WriteChar(&sv.multicast, svc_configstring);
		MSG_WriteConfigString(&sv.multicast, index, val);

		SV_Multicast(vec3_origin, MULTICAST_ALL_R);
	}
}

static void
PF_WriteChar(int c)
{
	MSG_WriteChar(&sv.multicast, c);
}

static void
PF_WriteByte(int c)
{
	MSG_WriteByte(&sv.multicast, c);
}

static void
PF_WriteShort(int c)
{
	MSG_WriteShort(&sv.multicast, c);
}

static void
PF_WriteLong(int c)
{
	MSG_WriteLong(&sv.multicast, c);
}

static void
PF_WriteFloat(float f)
{
	MSG_WriteFloat(&sv.multicast, f);
}

static void
PF_WriteString(char *s)
{
	MSG_WriteString(&sv.multicast, s);
}

static void
PF_WritePos(vec3_t pos)
{
	MSG_WritePos(&sv.multicast, pos);
}

static void
PF_WriteDir(vec3_t dir)
{
	MSG_WriteDir(&sv.multicast, dir);
}

static void
PF_WriteAngle(float f)
{
	MSG_WriteAngle(&sv.multicast, f);
}

/*
 * Also checks portalareas so that doors block sight
 */
static qboolean
PF_inPVS(vec3_t p1, vec3_t p2)
{
	int leafnum;
	int cluster;
	int area1, area2;
	byte *mask;

	leafnum = CM_PointLeafnum(p1);
	cluster = CM_LeafCluster(leafnum);
	area1 = CM_LeafArea(leafnum);
	mask = CM_ClusterPVS(cluster);

	leafnum = CM_PointLeafnum(p2);
	cluster = CM_LeafCluster(leafnum);
	area2 = CM_LeafArea(leafnum);

	// cluster -1 means "not in a visible leaf" or something like that (void?)
	// so p1 and p2 probably don't "see" each other.
	// either way, we must avoid using a negative index into mask[]!
	if (cluster < 0 || (!(mask[cluster >> 3] & (1 << (cluster & 7)))))
	{
		return false;
	}

	if (!CM_AreasConnected(area1, area2))
	{
		return false; /* a door blocks sight */
	}

	return true;
}

/*
 * Also checks portalareas so that doors block sound
 */
static qboolean
PF_inPHS(vec3_t p1, vec3_t p2)
{
	int leafnum;
	int cluster;
	int area1, area2;
	byte *mask;

	leafnum = CM_PointLeafnum(p1);
	cluster = CM_LeafCluster(leafnum);
	area1 = CM_LeafArea(leafnum);
	mask = CM_ClusterPHS(cluster);

	leafnum = CM_PointLeafnum(p2);
	cluster = CM_LeafCluster(leafnum);
	area2 = CM_LeafArea(leafnum);

	// cluster -1 means "not in a visible leaf" or something like that (void?)
	// so p1 and p2 probably don't "hear" each other.
	// either way, we must avoid using a negative index into mask[]!
	if (cluster < 0 || (!(mask[cluster >> 3] & (1 << (cluster & 7)))))
	{
		return false; /* more than one bounce away */
	}

	if (!CM_AreasConnected(area1, area2))
	{
		return false; /* a door blocks hearing */
	}

	return true;
}

static void
PF_StartSound(edict_t *entity, int channel, int sound_num,
		float volume, float attenuation, float timeofs)
{
	if (!entity)
	{
		return;
	}

	SV_StartSound(NULL, entity, channel, sound_num,
			volume, attenuation, timeofs);
}

/*
 * Called when either the entire server is being killed, or
 * it is changing to a different game directory.
 */
void
SV_ShutdownGameProgs(void)
{
	if (!ge)
	{
		return;
	}

	ge->Shutdown();
	Sys_UnloadGame();
	ge = NULL;
}

/*
 * Init the game subsystem for a new map
 */
void
SV_InitGameProgs(void)
{
	game_import_t import;

	/* unload anything we have now */
	if (ge)
	{
		SV_ShutdownGameProgs();
	}

	Com_Printf("-------- game initialization -------\n");

	/* load a new game dll */
	import.multicast = SV_Multicast;
	import.unicast = PF_Unicast;
	import.bprintf = SV_BroadcastPrintf;
	import.dprintf = PF_dprintf;
	import.cprintf = PF_cprintf;
	import.centerprintf = PF_centerprintf;
	import.error = PF_error;

	import.linkentity = SV_LinkEdict;
	import.unlinkentity = SV_UnlinkEdict;
	import.BoxEdicts = SV_AreaEdicts;
	import.trace = SV_Trace;
	import.pointcontents = SV_PointContents;
	import.setmodel = PF_setmodel;
	import.inPVS = PF_inPVS;
	import.inPHS = PF_inPHS;
	import.Pmove = Pmove;

	import.modelindex = SV_ModelIndex;
	import.soundindex = SV_SoundIndex;
	import.imageindex = SV_ImageIndex;

	import.configstring = PF_Configstring;
	import.sound = PF_StartSound;
	import.positioned_sound = SV_StartSound;

	import.WriteChar = PF_WriteChar;
	import.WriteByte = PF_WriteByte;
	import.WriteShort = PF_WriteShort;
	import.WriteLong = PF_WriteLong;
	import.WriteFloat = PF_WriteFloat;
	import.WriteString = PF_WriteString;
	import.WritePosition = PF_WritePos;
	import.WriteDir = PF_WriteDir;
	import.WriteAngle = PF_WriteAngle;

	import.TagMalloc = Z_TagMalloc;
	import.TagFree = Z_Free;
	import.FreeTags = Z_FreeTags;

	import.cvar = Cvar_Get;
	import.cvar_set = Cvar_Set;
	import.cvar_forceset = Cvar_ForceSet;

	import.argc = Cmd_Argc;
	import.argv = Cmd_Argv;
	import.args = Cmd_Args;
	import.AddCommandString = Cbuf_AddText;

#ifndef DEDICATED_ONLY
	import.DebugGraph = SCR_DebugGraph;
#endif

	import.SetAreaPortalState = CM_SetAreaPortalState;
	import.AreasConnected = CM_AreasConnected;

	ge = (game_export_t *)Sys_GetGameAPI(&import);

	if (!ge)
	{
		Com_Error(ERR_DROP, "failed to load game DLL");
	}

	if (ge->apiversion != GAME_API_VERSION)
	{
		Com_Error(ERR_DROP, "game is version %i, not %i", ge->apiversion,
				GAME_API_VERSION);
	}

	ge->Init();

	Com_Printf("------------------------------------\n\n");
}

