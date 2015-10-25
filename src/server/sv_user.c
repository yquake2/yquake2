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
 * Server side user (player entity) moving.
 *
 * =======================================================================
 */

#include "header/server.h"

#define MAX_STRINGCMDS 8

edict_t *sv_player;

void
SV_BeginDemoserver(void)
{
	char name[MAX_OSPATH];

	Com_sprintf(name, sizeof(name), "demos/%s", sv.name);
	FS_FOpenFile(name, &sv.demofile, false);

	if (!sv.demofile)
	{
		Com_Error(ERR_DROP, "Couldn't open %s\n", name);
	}
}

/*
 * Sends the first message from the server to a connected client.
 * This will be sent on the initial connection and upon each server load.
 */
void
SV_New_f(void)
{
	static char *gamedir;
	int playernum;
	edict_t *ent;

	Com_DPrintf("New() from %s\n", sv_client->name);

	if (sv_client->state != cs_connected)
	{
		Com_Printf("New not valid -- already spawned\n");
		return;
	}

	/* demo servers just dump the file message */
	if (sv.state == ss_demo)
	{
		SV_BeginDemoserver();
		return;
	}

	/* serverdata needs to go over for all types of servers
	   to make sure the protocol is right, and to set the gamedir */
	gamedir = (char *)Cvar_VariableString("gamedir");

	/* send the serverdata */
	MSG_WriteByte(&sv_client->netchan.message, svc_serverdata);
	MSG_WriteLong(&sv_client->netchan.message, PROTOCOL_VERSION);
	MSG_WriteLong(&sv_client->netchan.message, svs.spawncount);
	MSG_WriteByte(&sv_client->netchan.message, sv.attractloop);
	MSG_WriteString(&sv_client->netchan.message, gamedir);

	if ((sv.state == ss_cinematic) || (sv.state == ss_pic))
	{
		playernum = -1;
	}
	else
	{
		playernum = sv_client - svs.clients;
	}

	MSG_WriteShort(&sv_client->netchan.message, playernum);

	/* send full levelname */
	MSG_WriteString(&sv_client->netchan.message, sv.configstrings[CS_NAME]);

	/* game server */
	if (sv.state == ss_game)
	{
		/* set up the entity for the client */
		ent = EDICT_NUM(playernum + 1);
		ent->s.number = playernum + 1;
		sv_client->edict = ent;
		memset(&sv_client->lastcmd, 0, sizeof(sv_client->lastcmd));

		/* begin fetching configstrings */
		MSG_WriteByte(&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString(&sv_client->netchan.message,
				va("cmd configstrings %i 0\n", svs.spawncount));
	}
}

void
SV_Configstrings_f(void)
{
	int start;

	Com_DPrintf("Configstrings() from %s\n", sv_client->name);

	if (sv_client->state != cs_connected)
	{
		Com_Printf("configstrings not valid -- already spawned\n");
		return;
	}

	/* handle the case of a level changing while a client was connecting */
	if ((int)strtol(Cmd_Argv(1), (char **)NULL, 10) != svs.spawncount)
	{
		Com_Printf("SV_Configstrings_f from different level\n");
		SV_New_f();
		return;
	}

	start = (int)strtol(Cmd_Argv(2), (char **)NULL, 10);

	/* write a packet full of data */
	while (sv_client->netchan.message.cursize < MAX_MSGLEN / 2 &&
		   start < MAX_CONFIGSTRINGS)
	{
		if (sv.configstrings[start][0])
		{
			MSG_WriteByte(&sv_client->netchan.message, svc_configstring);
			MSG_WriteShort(&sv_client->netchan.message, start);
			MSG_WriteString(&sv_client->netchan.message,
					sv.configstrings[start]);
		}

		start++;
	}

	/* send next command */
	if (start == MAX_CONFIGSTRINGS)
	{
		MSG_WriteByte(&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString(&sv_client->netchan.message,
				va("cmd baselines %i 0\n", svs.spawncount));
	}
	else
	{
		MSG_WriteByte(&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString(&sv_client->netchan.message,
				va("cmd configstrings %i %i\n", svs.spawncount, start));
	}
}

void
SV_Baselines_f(void)
{
	int start;
	entity_state_t nullstate;
	entity_state_t *base;

	Com_DPrintf("Baselines() from %s\n", sv_client->name);

	if (sv_client->state != cs_connected)
	{
		Com_Printf("baselines not valid -- already spawned\n");
		return;
	}

	/* handle the case of a level changing while a client was connecting */
	if ((int)strtol(Cmd_Argv(1), (char **)NULL, 10) != svs.spawncount)
	{
		Com_Printf("SV_Baselines_f from different level\n");
		SV_New_f();
		return;
	}

	start = (int)strtol(Cmd_Argv(2), (char **)NULL, 10);
	memset(&nullstate, 0, sizeof(nullstate));

	/* write a packet full of data */
	while (sv_client->netchan.message.cursize < MAX_MSGLEN / 2 &&
		   start < MAX_EDICTS)
	{
		base = &sv.baselines[start];

		if (base->modelindex || base->sound || base->effects)
		{
			MSG_WriteByte(&sv_client->netchan.message, svc_spawnbaseline);
			MSG_WriteDeltaEntity(&nullstate, base,
					&sv_client->netchan.message,
					true, true);
		}

		start++;
	}

	/* send next command */
	if (start == MAX_EDICTS)
	{
		MSG_WriteByte(&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString(&sv_client->netchan.message,
				va("precache %i\n", svs.spawncount));
	}
	else
	{
		MSG_WriteByte(&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString(&sv_client->netchan.message,
				va("cmd baselines %i %i\n", svs.spawncount, start));
	}
}

void
SV_Begin_f(void)
{
	Com_DPrintf("Begin() from %s\n", sv_client->name);

	/* handle the case of a level changing while a client was connecting */
	if ((int)strtol(Cmd_Argv(1), (char **)NULL, 10) != svs.spawncount)
	{
		Com_Printf("SV_Begin_f from different level\n");
		SV_New_f();
		return;
	}

	sv_client->state = cs_spawned;

	/* call the game begin function */
	ge->ClientBegin(sv_player);

	Cbuf_InsertFromDefer();
}

void
SV_NextDownload_f(void)
{
	int r;
	int percent;
	int size;

	if (!sv_client->download)
	{
		return;
	}

	r = sv_client->downloadsize - sv_client->downloadcount;

	if (r > 1024)
	{
		r = 1024;
	}

	MSG_WriteByte(&sv_client->netchan.message, svc_download);
	MSG_WriteShort(&sv_client->netchan.message, r);

	sv_client->downloadcount += r;
	size = sv_client->downloadsize;

	if (!size)
	{
		size = 1;
	}

	percent = sv_client->downloadcount * 100 / size;
	MSG_WriteByte(&sv_client->netchan.message, percent);
	SZ_Write(&sv_client->netchan.message,
			sv_client->download + sv_client->downloadcount - r, r);

	if (sv_client->downloadcount != sv_client->downloadsize)
	{
		return;
	}

	FS_FreeFile(sv_client->download);
	sv_client->download = NULL;
}

void
SV_BeginDownload_f(void)
{
	char *name;
	extern cvar_t *allow_download;
	extern cvar_t *allow_download_players;
	extern cvar_t *allow_download_models;
	extern cvar_t *allow_download_sounds;
	extern cvar_t *allow_download_maps;
	extern int file_from_pak;
	int offset = 0;

	name = Cmd_Argv(1);

	if (Cmd_Argc() > 2)
	{
		offset = (int)strtol(Cmd_Argv(2), (char **)NULL, 10); /* downloaded offset */
	}

	/* hacked by zoid to allow more conrol over download
	   first off, no .. or global allow check */
	if (strstr(name, "..") || strstr(name, "\\") || !allow_download->value
		/* leading dot is no good */
		|| (*name == '.')
		/* leading slash bad as well, must be in subdir */
		|| (*name == '/')
		/* next up, skin check */
		|| ((strncmp(name, "players/", 6) == 0) && !allow_download_players->value)
		/* now models */
		|| ((strncmp(name, "models/", 6) == 0) && !allow_download_models->value)
		/* now sounds */
		|| ((strncmp(name, "sound/", 6) == 0) && !allow_download_sounds->value)
		/* now maps (note special case for maps, must not be in pak) */
		|| ((strncmp(name, "maps/", 6) == 0) && !allow_download_maps->value)
		/* MUST be in a subdirectory */
		|| !strstr(name, "/"))
	{
		MSG_WriteByte(&sv_client->netchan.message, svc_download);
		MSG_WriteShort(&sv_client->netchan.message, -1);
		MSG_WriteByte(&sv_client->netchan.message, 0);
		return;
	}

	if (sv_client->download)
	{
		FS_FreeFile(sv_client->download);
	}

	sv_client->downloadsize = FS_LoadFile(name, (void **)&sv_client->download);
	sv_client->downloadcount = offset;

	if (offset > sv_client->downloadsize)
	{
		sv_client->downloadcount = sv_client->downloadsize;
	}

	if (!sv_client->download || ((strncmp(name, "maps/", 5) == 0) && file_from_pak))
	{
		Com_DPrintf("Couldn't download %s to %s\n", name, sv_client->name);

		if (sv_client->download)
		{
			FS_FreeFile(sv_client->download);
			sv_client->download = NULL;
		}

		MSG_WriteByte(&sv_client->netchan.message, svc_download);
		MSG_WriteShort(&sv_client->netchan.message, -1);
		MSG_WriteByte(&sv_client->netchan.message, 0);
		return;
	}

	SV_NextDownload_f();
	Com_DPrintf("Downloading %s to %s\n", name, sv_client->name);
}

/*
 * The client is going to disconnect, so remove the connection immediately
 */
void
SV_Disconnect_f(void)
{
	SV_DropClient(sv_client);
}

/*
 * Dumps the serverinfo info string
 */
void
SV_ShowServerinfo_f(void)
{
	Info_Print(Cvar_Serverinfo());
}

void
SV_Nextserver(void)
{
	const char *v;

	if ((sv.state == ss_game) ||
		((sv.state == ss_pic) &&
		 !Cvar_VariableValue("coop")))
	{
		return; /* can't nextserver while playing a normal game */
	}

	svs.spawncount++; /* make sure another doesn't sneak in */
	v = Cvar_VariableString("nextserver");

	if (!v[0])
	{
		Cbuf_AddText("killserver\n");
	}
	else
	{
		Cbuf_AddText((char *)v);
		Cbuf_AddText("\n");
	}

	Cvar_Set("nextserver", "");
}

/*
 * A cinematic has completed or been aborted by a client, so move
 * to the next server,
 */
void
SV_Nextserver_f(void)
{
	if ((int)strtol(Cmd_Argv(1), (char **)NULL, 10) != svs.spawncount)
	{
		Com_DPrintf("Nextserver() from wrong level, from %s\n", sv_client->name);
		return; /* leftover from last server */
	}

	Com_DPrintf("Nextserver() from %s\n", sv_client->name);

	SV_Nextserver();
}

typedef struct
{
	char *name;
	void (*func)(void);
} ucmd_t;

ucmd_t ucmds[] = {
	/* auto issued */
	{"new", SV_New_f},
	{"configstrings", SV_Configstrings_f},
	{"baselines", SV_Baselines_f},
	{"begin", SV_Begin_f},
	{"nextserver", SV_Nextserver_f},
	{"disconnect", SV_Disconnect_f},

	/* issued by hand at client consoles */
	{"info", SV_ShowServerinfo_f},

	{"download", SV_BeginDownload_f},
	{"nextdl", SV_NextDownload_f},

	{NULL, NULL}
};

void
SV_ExecuteUserCommand(char *s)
{
	ucmd_t *u;

	/* Security Fix... This is being set to false so that client's can't
	   macro expand variables on the server.  It seems unlikely that a
	   client ever ought to need to be able to do this... */
	Cmd_TokenizeString(s, false);
	sv_player = sv_client->edict;

	for (u = ucmds; u->name; u++)
	{
		if (!strcmp(Cmd_Argv(0), u->name))
		{
			u->func();
			break;
		}
	}

	if (!u->name && (sv.state == ss_game))
	{
		ge->ClientCommand(sv_player);
	}
}

void
SV_ClientThink(client_t *cl, usercmd_t *cmd)

{
	cl->commandMsec -= cmd->msec;

	if ((cl->commandMsec < 0) && sv_enforcetime->value)
	{
		Com_DPrintf("commandMsec underflow from %s\n", cl->name);
		return;
	}

	ge->ClientThink(cl->edict, cmd);
}

/*
 * The current net_message is parsed for the given client
 */
void
SV_ExecuteClientMessage(client_t *cl)
{
	int c;
	char *s;

	usercmd_t nullcmd;
	usercmd_t oldest, oldcmd, newcmd;
	int net_drop;
	int stringCmdCount;
	int checksum, calculatedChecksum;
	int checksumIndex;
	qboolean move_issued;
	int lastframe;

	sv_client = cl;
	sv_player = sv_client->edict;

	/* only allow one move command */
	move_issued = false;
	stringCmdCount = 0;

	while (1)
	{
		if (net_message.readcount > net_message.cursize)
		{
			Com_Printf("SV_ReadClientMessage: badread\n");
			SV_DropClient(cl);
			return;
		}

		c = MSG_ReadByte(&net_message);

		if (c == -1)
		{
			break;
		}

		switch (c)
		{
			default:
				Com_Printf("SV_ReadClientMessage: unknown command char\n");
				SV_DropClient(cl);
				return;

			case clc_nop:
				break;

			case clc_userinfo:
				Q_strlcpy(cl->userinfo, MSG_ReadString(&net_message), sizeof(cl->userinfo));
				SV_UserinfoChanged(cl);
				break;

			case clc_move:

				if (move_issued)
				{
					return; /* someone is trying to cheat... */
				}

				move_issued = true;
				checksumIndex = net_message.readcount;
				checksum = MSG_ReadByte(&net_message);
				lastframe = MSG_ReadLong(&net_message);

				if (lastframe != cl->lastframe)
				{
					cl->lastframe = lastframe;

					if (cl->lastframe > 0)
					{
						cl->frame_latency[cl->lastframe & (LATENCY_COUNTS - 1)] =
							svs.realtime - cl->frames[cl->lastframe & UPDATE_MASK].senttime;
					}
				}

				memset(&nullcmd, 0, sizeof(nullcmd));
				MSG_ReadDeltaUsercmd(&net_message, &nullcmd, &oldest);
				MSG_ReadDeltaUsercmd(&net_message, &oldest, &oldcmd);
				MSG_ReadDeltaUsercmd(&net_message, &oldcmd, &newcmd);

				if (cl->state != cs_spawned)
				{
					cl->lastframe = -1;
					break;
				}

				/* if the checksum fails, ignore the rest of the packet */
				calculatedChecksum = COM_BlockSequenceCRCByte(
					net_message.data + checksumIndex + 1,
					net_message.readcount - checksumIndex - 1,
					cl->netchan.incoming_sequence);

				if (calculatedChecksum != checksum)
				{
					Com_DPrintf("Failed command checksum for %s (%d != %d)/%d\n",
							cl->name, calculatedChecksum, checksum,
							cl->netchan.incoming_sequence);
					return;
				}

				if (!sv_paused->value)
				{
					net_drop = cl->netchan.dropped;

					if (net_drop < 20)
					{
						while (net_drop > 2)
						{
							SV_ClientThink(cl, &cl->lastcmd);

							net_drop--;
						}

						if (net_drop > 1)
						{
							SV_ClientThink(cl, &oldest);
						}

						if (net_drop > 0)
						{
							SV_ClientThink(cl, &oldcmd);
						}
					}

					SV_ClientThink(cl, &newcmd);
				}

				cl->lastcmd = newcmd;
				break;

			case clc_stringcmd:
				s = MSG_ReadString(&net_message);

				/* malicious users may try using too many string commands */
				if (++stringCmdCount < MAX_STRINGCMDS)
				{
					SV_ExecuteUserCommand(s);
				}

				if (cl->state == cs_zombie)
				{
					return; /* disconnect command */
				}

				break;
		}
	}
}

