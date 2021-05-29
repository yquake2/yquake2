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
 * Main header file for the client
 *
 * =======================================================================
 */

#ifndef SV_SERVER_H
#define SV_SERVER_H

#include "../../common/header/common.h"
#include "../../game/header/game.h"

#define MAX_MASTERS 8
#define LATENCY_COUNTS 16
#define RATE_MESSAGES 10

/* MAX_CHALLENGES is made large to prevent a denial
   of service attack that could cycle all of them
   out before legitimate users connected */
#define MAX_CHALLENGES 1024

/* MAX_TOKEN_CHARS was 128. YQ2 bumped it to 1024, since we
 * need to support some very long cvars like gl_nolerp_list.
 * Keep structs used in savegames at 128, otherwise older
 * savegames would be broken. */
#define MAX_SAVE_TOKEN_CHARS 128


#define SV_OUTPUTBUF_LENGTH (MAX_MSGLEN - 16)
#define EDICT_NUM(n) ((edict_t *)((byte *)ge->edicts + ge->edict_size * (n)))
#define NUM_FOR_EDICT(e) (((byte *)(e) - (byte *)ge->edicts) / ge->edict_size)

typedef enum
{
	ss_dead,            /* no map loaded */
	ss_loading,         /* spawning level edicts */
	ss_game,            /* actively running */
	ss_cinematic,
	ss_demo,
	ss_pic
} server_state_t;

typedef struct
{
	server_state_t state;           /* precache commands are only valid during load */

	qboolean attractloop;           /* running cinematics and demos for the local system only */
	qboolean loadgame;              /* client begins should reuse existing entity */

	unsigned time;                  /* always sv.framenum * 100 msec */
	int framenum;

	char name[MAX_QPATH];           /* map name, or cinematic name */
	struct cmodel_s *models[MAX_MODELS];

	char configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];
	entity_state_t baselines[MAX_EDICTS];

	/* the multicast buffer is used to send a message to a set of clients
	   it is only used to marshall data until SV_Multicast is called */
	sizebuf_t multicast;
	byte multicast_buf[MAX_MSGLEN];

	/* demo server information */
	fileHandle_t demofile;
	qboolean timedemo; /* don't time sync */
} server_t;

typedef enum
{
	cs_free,        /* can be reused for a new connection */
	cs_zombie,      /* client has been disconnected, but don't reuse 
					   connection for a couple seconds */
	cs_connected,   /* has been assigned to a client_t, but not in game yet */
	cs_spawned      /* client is fully in game */
} client_state_t;

typedef struct
{
	int areabytes;
	byte areabits[MAX_MAP_AREAS / 8];       /* portalarea visibility bits */
	player_state_t ps;
	int num_entities;
	int first_entity;                       /* into the circular sv_packet_entities[] */
	int senttime;                           /* for ping calculations */
} client_frame_t;

typedef struct client_s
{
	client_state_t state;

	char userinfo[MAX_INFO_STRING];     /* name, etc */

	int lastframe;                      /* for delta compression */
	usercmd_t lastcmd;                  /* for filling in big drops */

	int commandMsec;                    /* every seconds this is reset, if user */
										/* commands exhaust it, assume time cheating */

	int frame_latency[LATENCY_COUNTS];
	int ping;

	int message_size[RATE_MESSAGES];    /* used to rate drop packets */
	int rate;
	int surpressCount;                  /* number of messages rate supressed */

	edict_t *edict;                     /* EDICT_NUM(clientnum+1) */
	char name[32];                      /* extracted from userinfo, high bits masked */
	int messagelevel;                   /* for filtering printed messages */

	/* The datagram is written to by sound calls, prints, 
	   temp ents, etc. It can be harmlessly overflowed. */
	sizebuf_t datagram;
	byte datagram_buf[MAX_MSGLEN];

	client_frame_t frames[UPDATE_BACKUP];     /* updates can be delta'd from here */

	byte *download;                     /* file being downloaded */
	int downloadsize;                   /* total bytes (can't use EOF because of paks) */
	int downloadcount;                  /* bytes sent */

	int lastmessage;                    /* sv.framenum when packet was last received */
	int lastconnect;

	int challenge;                      /* challenge of this user, randomly generated */

	netchan_t netchan;
} client_t;

typedef struct
{
	netadr_t adr;
	int challenge;
	int time;
} challenge_t;

typedef struct
{
	qboolean initialized;               /* sv_init has completed */
	int realtime;                       /* always increasing, no clamping, etc */

	char mapcmd[MAX_SAVE_TOKEN_CHARS];  /* ie: *intro.cin+base */

	int spawncount;                     /* incremented each server start */
										/* used to check late spawns */

	client_t *clients;                  /* [maxclients->value]; */
	int num_client_entities;            /* maxclients->value*UPDATE_BACKUP*MAX_PACKET_ENTITIES */
	int next_client_entities;           /* next client_entity to use */
	entity_state_t *client_entities;    /* [num_client_entities] */

	int last_heartbeat;

	challenge_t challenges[MAX_CHALLENGES];    /* to prevent invalid IPs from connecting */

	/* serverrecord values */
	FILE *demofile;
	sizebuf_t demo_multicast;
	byte demo_multicast_buf[MAX_MSGLEN];
} server_static_t;

extern netadr_t net_from;
extern sizebuf_t net_message;

extern netadr_t master_adr[MAX_MASTERS];    /* address of the master server */

extern server_static_t svs;                 /* persistant server info */
extern server_t sv;                         /* local server */

extern cvar_t *sv_paused;
extern cvar_t *maxclients;
extern cvar_t *sv_noreload;                 /* don't reload level state when reentering */
extern cvar_t *sv_airaccelerate;            /* don't reload level state when reentering */
											/* development tool */
extern cvar_t *sv_enforcetime;
extern cvar_t *sv_downloadserver;			/* Download server. */

extern client_t *sv_client;
extern edict_t *sv_player;

void SV_FinalMessage(char *message, qboolean reconnect);
void SV_DropClient(client_t *drop);

int SV_ModelIndex(char *name);
int SV_SoundIndex(char *name);
int SV_ImageIndex(char *name);

void SV_WriteClientdataToMessage(client_t *client, sizebuf_t *msg);

void SV_ExecuteUserCommand(char *s);
void SV_InitOperatorCommands(void);

void SV_SendServerinfo(client_t *client);
void SV_UserinfoChanged(client_t *cl);

void Master_Heartbeat(void);
void Master_Packet(void);

void SV_InitGame(void);
void SV_Map(qboolean attractloop, char *levelstring, qboolean loadgame, qboolean isautosave);

void SV_PrepWorldFrame(void);

typedef enum {RD_NONE, RD_CLIENT, RD_PACKET} redirect_t;

extern char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

void SV_FlushRedirect(int sv_redirected, char *outputbuf);

void SV_DemoCompleted(void);
void SV_SendClientMessages(void);

void SV_Multicast(vec3_t origin, multicast_t to);
void SV_StartSound(vec3_t origin, edict_t *entity, int channel,
		int soundindex, float volume, float attenuation,
		float timeofs);
void SV_ClientPrintf(client_t *cl, int level, char *fmt, ...);
void SV_BroadcastPrintf(int level, char *fmt, ...);
void SV_BroadcastCommand(char *fmt, ...);

void SV_Nextserver(void);
void SV_ExecuteClientMessage(client_t *cl);

void SV_ReadLevelFile(void);
void SV_Status_f(void);

void SV_WriteFrameToClient(client_t *client, sizebuf_t *msg);
void SV_RecordDemoMessage(void);
void SV_BuildClientFrame(client_t *client);

extern game_export_t *ge;

void SV_InitGameProgs(void);
void SV_ShutdownGameProgs(void);
void SV_InitEdict(edict_t *e);

/* server side savegame stuff */
void SV_WipeSavegame(char *savename);
void SV_CopySaveGame(char *src, char *dst);
void SV_WriteLevelFile(void);
void SV_WriteServerFile(qboolean autosave);
void SV_Loadgame_f(void);
void SV_Savegame_f(void);

/* high level object sorting to reduce interaction tests */
void SV_ClearWorld(void);

/* called after the world model has been loaded, before linking any entities */
void SV_UnlinkEdict(edict_t *ent);

/* call before removing an entity, and before trying to move one,
 * so it doesn't clip against itself */
void SV_LinkEdict(edict_t *ent);

/* Needs to be called any time an entity changes origin, mins, maxs,
   or solid. Automatically unlinks if needed. sets ent->v.absmin and
   ent->v.absmax sets ent->leafnums[] for pvs determination even if 
   the entity is not solid */
int SV_AreaEdicts(vec3_t mins, vec3_t maxs, edict_t **list,
		int maxcount, int areatype);

int SV_PointContents(vec3_t p);

trace_t SV_Trace(vec3_t start, vec3_t mins, vec3_t maxs,
		vec3_t end, edict_t *passedict, int contentmask);

#endif

