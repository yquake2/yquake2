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
 * This file implements the entity and network protocol parsing
 *
 * =======================================================================
 */

#include "header/client.h"
#include "input/header/input.h"

void CL_DownloadFileName(char *dest, int destlen, char *fn);
void CL_ParseDownload(void);

int bitcounts[32]; /* just for protocol profiling */

char *svc_strings[256] = {
	"svc_bad",

	"svc_muzzleflash",
	"svc_muzzlflash2",
	"svc_temp_entity",
	"svc_layout",
	"svc_inventory",

	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_sound",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",
	"svc_centerprint",
	"svc_download",
	"svc_playerinfo",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_frame"
};

void
CL_RegisterSounds(void)
{
	int i;

	S_BeginRegistration();
	CL_RegisterTEntSounds();

	for (i = 1; i < MAX_SOUNDS; i++)
	{
		if (!cl.configstrings[CS_SOUNDS + i][0])
		{
			break;
		}

		cl.sound_precache[i] = S_RegisterSound(cl.configstrings[CS_SOUNDS + i]);
		IN_Update();
	}

	S_EndRegistration();
}

/*
 * Returns the entity number and the header bits
 */
int
CL_ParseEntityBits(unsigned *bits)
{
	unsigned b, total;
	int i;
	int number;

	total = MSG_ReadByte(&net_message);

	if (total & U_MOREBITS1)
	{
		b = MSG_ReadByte(&net_message);
		total |= b << 8;
	}

	if (total & U_MOREBITS2)
	{
		b = MSG_ReadByte(&net_message);
		total |= b << 16;
	}

	if (total & U_MOREBITS3)
	{
		b = MSG_ReadByte(&net_message);
		total |= b << 24;
	}

	/* count the bits for net profiling */
	for (i = 0; i < 32; i++)
	{
		if (total & (1 << i))
		{
			bitcounts[i]++;
		}
	}

	if (total & U_NUMBER16)
	{
		number = MSG_ReadShort(&net_message);
	}

	else
	{
		number = MSG_ReadByte(&net_message);
	}

	*bits = total;

	return number;
}

/*
 * Can go from either a baseline or a previous packet_entity
 */
void
CL_ParseDelta(entity_state_t *from, entity_state_t *to, int number, int bits)
{
	/* set everything to the state we are delta'ing from */
	*to = *from;

	VectorCopy(from->origin, to->old_origin);
	to->number = number;

	if (bits & U_MODEL)
	{
		to->modelindex = MSG_ReadByte(&net_message);
	}

	if (bits & U_MODEL2)
	{
		to->modelindex2 = MSG_ReadByte(&net_message);
	}

	if (bits & U_MODEL3)
	{
		to->modelindex3 = MSG_ReadByte(&net_message);
	}

	if (bits & U_MODEL4)
	{
		to->modelindex4 = MSG_ReadByte(&net_message);
	}

	if (bits & U_FRAME8)
	{
		to->frame = MSG_ReadByte(&net_message);
	}

	if (bits & U_FRAME16)
	{
		to->frame = MSG_ReadShort(&net_message);
	}

	/* used for laser colors */
	if ((bits & U_SKIN8) && (bits & U_SKIN16))
	{
		to->skinnum = MSG_ReadLong(&net_message);
	}
	else if (bits & U_SKIN8)
	{
		to->skinnum = MSG_ReadByte(&net_message);
	}
	else if (bits & U_SKIN16)
	{
		to->skinnum = MSG_ReadShort(&net_message);
	}

	if ((bits & (U_EFFECTS8 | U_EFFECTS16)) == (U_EFFECTS8 | U_EFFECTS16))
	{
		to->effects = MSG_ReadLong(&net_message);
	}
	else if (bits & U_EFFECTS8)
	{
		to->effects = MSG_ReadByte(&net_message);
	}
	else if (bits & U_EFFECTS16)
	{
		to->effects = MSG_ReadShort(&net_message);
	}

	if ((bits & (U_RENDERFX8 | U_RENDERFX16)) == (U_RENDERFX8 | U_RENDERFX16))
	{
		to->renderfx = MSG_ReadLong(&net_message);
	}
	else if (bits & U_RENDERFX8)
	{
		to->renderfx = MSG_ReadByte(&net_message);
	}
	else if (bits & U_RENDERFX16)
	{
		to->renderfx = MSG_ReadShort(&net_message);
	}

	if (bits & U_ORIGIN1)
	{
		to->origin[0] = MSG_ReadCoord(&net_message);
	}

	if (bits & U_ORIGIN2)
	{
		to->origin[1] = MSG_ReadCoord(&net_message);
	}

	if (bits & U_ORIGIN3)
	{
		to->origin[2] = MSG_ReadCoord(&net_message);
	}

	if (bits & U_ANGLE1)
	{
		to->angles[0] = MSG_ReadAngle(&net_message);
	}

	if (bits & U_ANGLE2)
	{
		to->angles[1] = MSG_ReadAngle(&net_message);
	}

	if (bits & U_ANGLE3)
	{
		to->angles[2] = MSG_ReadAngle(&net_message);
	}

	if (bits & U_OLDORIGIN)
	{
		MSG_ReadPos(&net_message, to->old_origin);
	}

	if (bits & U_SOUND)
	{
		to->sound = MSG_ReadByte(&net_message);
	}

	if (bits & U_EVENT)
	{
		to->event = MSG_ReadByte(&net_message);
	}
	else
	{
		to->event = 0;
	}

	if (bits & U_SOLID)
	{
		to->solid = MSG_ReadShort(&net_message);
	}
}

/*
 * Parses deltas from the given base and adds the resulting entity to
 * the current frame
 */
void
CL_DeltaEntity(frame_t *frame, int newnum, entity_state_t *old, int bits)
{
	centity_t *ent;
	entity_state_t *state;

	ent = &cl_entities[newnum];

	state = &cl_parse_entities[cl.parse_entities & (MAX_PARSE_ENTITIES - 1)];
	cl.parse_entities++;
	frame->num_entities++;

	CL_ParseDelta(old, state, newnum, bits);

	/* some data changes will force no lerping */
	if ((state->modelindex != ent->current.modelindex) ||
		(state->modelindex2 != ent->current.modelindex2) ||
		(state->modelindex3 != ent->current.modelindex3) ||
		(state->modelindex4 != ent->current.modelindex4) ||
		(state->event == EV_PLAYER_TELEPORT) ||
		(state->event == EV_OTHER_TELEPORT) ||
		(abs((int)(state->origin[0] - ent->current.origin[0])) > 512) ||
		(abs((int)(state->origin[1] - ent->current.origin[1])) > 512) ||
		(abs((int)(state->origin[2] - ent->current.origin[2])) > 512)
		)
	{
		ent->serverframe = -99;
	}

	/* wasn't in last update, so initialize some things */
	if (ent->serverframe != cl.frame.serverframe - 1)
	{
		ent->trailcount = 1024; /* for diminishing rocket / grenade trails */

		/* duplicate the current state so
		   lerping doesn't hurt anything */
		ent->prev = *state;

		if (state->event == EV_OTHER_TELEPORT)
		{
			VectorCopy(state->origin, ent->prev.origin);
			VectorCopy(state->origin, ent->lerp_origin);
		}
		else
		{
			VectorCopy(state->old_origin, ent->prev.origin);
			VectorCopy(state->old_origin, ent->lerp_origin);
		}
	}
	else
	{
		/* shuffle the last state to previous */
		ent->prev = ent->current;
	}

	ent->serverframe = cl.frame.serverframe;
	ent->current = *state;
}

/*
 * An svc_packetentities has just been
 * parsed, deal with the rest of the
 * data stream.
 */
void
CL_ParsePacketEntities(frame_t *oldframe, frame_t *newframe)
{
	unsigned int newnum;
	unsigned bits;
	entity_state_t
	*oldstate = NULL;
	int oldindex, oldnum;

	newframe->parse_entities = cl.parse_entities;
	newframe->num_entities = 0;

	/* delta from the entities present in oldframe */
	oldindex = 0;

	if (!oldframe)
	{
		oldnum = 99999;
	}

	else
	{
		if (oldindex >= oldframe->num_entities)
		{
			oldnum = 99999;
		}

		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities +
									oldindex) & (MAX_PARSE_ENTITIES - 1)];
			oldnum = oldstate->number;
		}
	}

	while (1)
	{
		newnum = CL_ParseEntityBits(&bits);

		if (newnum >= MAX_EDICTS)
		{
			Com_Error(ERR_DROP, "CL_ParsePacketEntities: bad number:%i", newnum);
		}

		if (net_message.readcount > net_message.cursize)
		{
			Com_Error(ERR_DROP, "CL_ParsePacketEntities: end of message");
		}

		if (!newnum)
		{
			break;
		}

		while (oldnum < newnum)
		{
			/* one or more entities from the old packet are unchanged */
			if (cl_shownet->value == 3)
			{
				Com_Printf("   unchanged: %i\n", oldnum);
			}

			CL_DeltaEntity(newframe, oldnum, oldstate, 0);

			oldindex++;

			if (oldindex >= oldframe->num_entities)
			{
				oldnum = 99999;
			}

			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities +
										oldindex) & (MAX_PARSE_ENTITIES - 1)];
				oldnum = oldstate->number;
			}
		}

		if (bits & U_REMOVE)
		{
			/* the entity present in oldframe is not in the current frame */
			if (cl_shownet->value == 3)
			{
				Com_Printf("   remove: %i\n", newnum);
			}

			if (oldnum != newnum)
			{
				Com_Printf("U_REMOVE: oldnum != newnum\n");
			}

			oldindex++;

			if (oldindex >= oldframe->num_entities)
			{
				oldnum = 99999;
			}

			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities +
										oldindex) & (MAX_PARSE_ENTITIES - 1)];
				oldnum = oldstate->number;
			}

			continue;
		}

		if (oldnum == newnum)
		{
			/* delta from previous state */
			if (cl_shownet->value == 3)
			{
				Com_Printf("   delta: %i\n", newnum);
			}

			CL_DeltaEntity(newframe, newnum, oldstate, bits);

			oldindex++;

			if (oldindex >= oldframe->num_entities)
			{
				oldnum = 99999;
			}

			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities +
										oldindex) & (MAX_PARSE_ENTITIES - 1)];
				oldnum = oldstate->number;
			}

			continue;
		}

		if (oldnum > newnum)
		{
			/* delta from baseline */
			if (cl_shownet->value == 3)
			{
				Com_Printf("   baseline: %i\n", newnum);
			}

			CL_DeltaEntity(newframe, newnum,
					&cl_entities[newnum].baseline,
					bits);
			continue;
		}
	}

	/* any remaining entities in the old frame are copied over */
	while (oldnum != 99999)
	{
		/* one or more entities from the old packet are unchanged */
		if (cl_shownet->value == 3)
		{
			Com_Printf("   unchanged: %i\n", oldnum);
		}

		CL_DeltaEntity(newframe, oldnum, oldstate, 0);

		oldindex++;

		if (oldindex >= oldframe->num_entities)
		{
			oldnum = 99999;
		}

		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities +
									oldindex) & (MAX_PARSE_ENTITIES - 1)];
			oldnum = oldstate->number;
		}
	}
}

void
CL_ParsePlayerstate(frame_t *oldframe, frame_t *newframe)
{
	int flags;
	player_state_t *state;
	int i;
	int statbits;

	state = &newframe->playerstate;

	/* clear to old value before delta parsing */
	if (oldframe)
	{
		*state = oldframe->playerstate;
	}

	else
	{
		memset(state, 0, sizeof(*state));
	}

	flags = MSG_ReadShort(&net_message);

	/* parse the pmove_state_t */
	if (flags & PS_M_TYPE)
	{
		state->pmove.pm_type = MSG_ReadByte(&net_message);
	}

	if (flags & PS_M_ORIGIN)
	{
		state->pmove.origin[0] = MSG_ReadShort(&net_message);
		state->pmove.origin[1] = MSG_ReadShort(&net_message);
		state->pmove.origin[2] = MSG_ReadShort(&net_message);
	}

	if (flags & PS_M_VELOCITY)
	{
		state->pmove.velocity[0] = MSG_ReadShort(&net_message);
		state->pmove.velocity[1] = MSG_ReadShort(&net_message);
		state->pmove.velocity[2] = MSG_ReadShort(&net_message);
	}

	if (flags & PS_M_TIME)
	{
		state->pmove.pm_time = MSG_ReadByte(&net_message);
	}

	if (flags & PS_M_FLAGS)
	{
		state->pmove.pm_flags = MSG_ReadByte(&net_message);
	}

	if (flags & PS_M_GRAVITY)
	{
		state->pmove.gravity = MSG_ReadShort(&net_message);
	}

	if (flags & PS_M_DELTA_ANGLES)
	{
		state->pmove.delta_angles[0] = MSG_ReadShort(&net_message);
		state->pmove.delta_angles[1] = MSG_ReadShort(&net_message);
		state->pmove.delta_angles[2] = MSG_ReadShort(&net_message);
	}

	if (cl.attractloop)
	{
		state->pmove.pm_type = PM_FREEZE; /* demo playback */
	}

	/* parse the rest of the player_state_t */
	if (flags & PS_VIEWOFFSET)
	{
		state->viewoffset[0] = MSG_ReadChar(&net_message) * 0.25f;
		state->viewoffset[1] = MSG_ReadChar(&net_message) * 0.25f;
		state->viewoffset[2] = MSG_ReadChar(&net_message) * 0.25f;
	}

	if (flags & PS_VIEWANGLES)
	{
		state->viewangles[0] = MSG_ReadAngle16(&net_message);
		state->viewangles[1] = MSG_ReadAngle16(&net_message);
		state->viewangles[2] = MSG_ReadAngle16(&net_message);
	}

	if (flags & PS_KICKANGLES)
	{
		state->kick_angles[0] = MSG_ReadChar(&net_message) * 0.25f;
		state->kick_angles[1] = MSG_ReadChar(&net_message) * 0.25f;
		state->kick_angles[2] = MSG_ReadChar(&net_message) * 0.25f;
	}

	if (flags & PS_WEAPONINDEX)
	{
		state->gunindex = MSG_ReadByte(&net_message);
	}

	if (flags & PS_WEAPONFRAME)
	{
		state->gunframe = MSG_ReadByte(&net_message);
		state->gunoffset[0] = MSG_ReadChar(&net_message) * 0.25f;
		state->gunoffset[1] = MSG_ReadChar(&net_message) * 0.25f;
		state->gunoffset[2] = MSG_ReadChar(&net_message) * 0.25f;
		state->gunangles[0] = MSG_ReadChar(&net_message) * 0.25f;
		state->gunangles[1] = MSG_ReadChar(&net_message) * 0.25f;
		state->gunangles[2] = MSG_ReadChar(&net_message) * 0.25f;
	}

	if (flags & PS_BLEND)
	{
		state->blend[0] = MSG_ReadByte(&net_message) / 255.0f;
		state->blend[1] = MSG_ReadByte(&net_message) / 255.0f;
		state->blend[2] = MSG_ReadByte(&net_message) / 255.0f;
		state->blend[3] = MSG_ReadByte(&net_message) / 255.0f;
	}

	if (flags & PS_FOV)
	{
		state->fov = (float)MSG_ReadByte(&net_message);
	}

	if (flags & PS_RDFLAGS)
	{
		state->rdflags = MSG_ReadByte(&net_message);
	}

	/* parse stats */
	statbits = MSG_ReadLong(&net_message);

	for (i = 0; i < MAX_STATS; i++)
	{
		if (statbits & (1 << i))
		{
			state->stats[i] = MSG_ReadShort(&net_message);
		}
	}
}

void
CL_FireEntityEvents(frame_t *frame)
{
	entity_state_t *s1;
	int pnum, num;

	for (pnum = 0; pnum < frame->num_entities; pnum++)
	{
		num = (frame->parse_entities + pnum) & (MAX_PARSE_ENTITIES - 1);
		s1 = &cl_parse_entities[num];

		if (s1->event)
		{
			CL_EntityEvent(s1);
		}

		if (s1->effects & EF_TELEPORTER)
		{
			CL_TeleporterParticles(s1);
		}
	}
}

void
CL_ParseFrame(void)
{
	int cmd;
	int len;
	frame_t *old;

	memset(&cl.frame, 0, sizeof(cl.frame));

	cl.frame.serverframe = MSG_ReadLong(&net_message);
	cl.frame.deltaframe = MSG_ReadLong(&net_message);
	cl.frame.servertime = cl.frame.serverframe * 100;

	/* BIG HACK to let old demos continue to work */
	if (cls.serverProtocol != 26)
	{
		cl.surpressCount = MSG_ReadByte(&net_message);
	}

	if (cl_shownet->value == 3)
	{
		Com_Printf("   frame:%i  delta:%i\n", cl.frame.serverframe,
				cl.frame.deltaframe);
	}

	/* If the frame is delta compressed from data that we
	   no longer have available, we must suck up the rest of
	   the frame, but not use it, then ask for a non-compressed
	   message */
	if (cl.frame.deltaframe <= 0)
	{
		cl.frame.valid = true; /* uncompressed frame */
		old = NULL;
		cls.demowaiting = false; /* we can start recording now */
	}
	else
	{
		old = &cl.frames[cl.frame.deltaframe & UPDATE_MASK];

		if (!old->valid)
		{
			/* should never happen */
			Com_Printf("Delta from invalid frame (not supposed to happen!).\n");
		}

		if (old->serverframe != cl.frame.deltaframe)
		{
			/* The frame that the server did the delta from
			   is too old, so we can't reconstruct it properly. */
			Com_Printf("Delta frame too old.\n");
		}
		else if (cl.parse_entities - old->parse_entities > MAX_PARSE_ENTITIES -
				 128)
		{
			Com_Printf("Delta parse_entities too old.\n");
		}
		else
		{
			cl.frame.valid = true; /* valid delta parse */
		}
	}

	/* clamp time */
	if (cl.time > cl.frame.servertime)
	{
		cl.time = cl.frame.servertime;
	}

	else if (cl.time < cl.frame.servertime - 100)
	{
		cl.time = cl.frame.servertime - 100;
	}

	/* read areabits */
	len = MSG_ReadByte(&net_message);
	MSG_ReadData(&net_message, &cl.frame.areabits, len);

	/* read playerinfo */
	cmd = MSG_ReadByte(&net_message);
	SHOWNET(svc_strings[cmd]);

	if (cmd != svc_playerinfo)
	{
		Com_Error(ERR_DROP, "CL_ParseFrame: 0x%X not playerinfo", cmd);
	}

	CL_ParsePlayerstate(old, &cl.frame);

	/* read packet entities */
	cmd = MSG_ReadByte(&net_message);
	SHOWNET(svc_strings[cmd]);

	if (cmd != svc_packetentities)
	{
		Com_Error(ERR_DROP, "CL_ParseFrame: 0x%X not packetentities", cmd);
	}

	CL_ParsePacketEntities(old, &cl.frame);

	/* save the frame off in the backup array for later delta comparisons */
	cl.frames[cl.frame.serverframe & UPDATE_MASK] = cl.frame;

	if (cl.frame.valid)
	{
		/* getting a valid frame message ends the connection process */
		if (cls.state != ca_active)
		{
			cls.state = ca_active;
			cl.force_refdef = true;
			cl.predicted_origin[0] = cl.frame.playerstate.pmove.origin[0] * 0.125f;
			cl.predicted_origin[1] = cl.frame.playerstate.pmove.origin[1] * 0.125f;
			cl.predicted_origin[2] = cl.frame.playerstate.pmove.origin[2] * 0.125f;
			VectorCopy(cl.frame.playerstate.viewangles, cl.predicted_angles);

			if ((cls.disable_servercount != cl.servercount) && cl.refresh_prepped)
			{
				SCR_EndLoadingPlaque();  /* get rid of loading plaque */
			}

			cl.sound_prepped = true;

			if (paused_at_load)
			{
				if (cl_loadpaused->value == 1)
				{
					Cvar_Set("paused", "0");
				}

				paused_at_load = false;
			}
		}

		/* fire entity events */
		CL_FireEntityEvents(&cl.frame);

		if (!(!cl_predict->value ||
			  (cl.frame.playerstate.pmove.pm_flags &
			   PMF_NO_PREDICTION)))
		{
			CL_CheckPredictionError();
		}
	}
}

void
CL_ParseServerData(void)
{
	extern cvar_t *fs_gamedirvar;
	char *str;
	int i;

	/* Clear all key states */
	In_FlushQueue();

	Com_DPrintf("Serverdata packet received.\n");

	/* wipe the client_state_t struct */
	CL_ClearState();
	cls.state = ca_connected;

	/* parse protocol version number */
	i = MSG_ReadLong(&net_message);
	cls.serverProtocol = i;

	/* another demo hack */
	if (Com_ServerState() && (PROTOCOL_VERSION == 34))
	{
	}
	else if (i != PROTOCOL_VERSION)
	{
		Com_Error(ERR_DROP, "Server returned version %i, not %i",
				i, PROTOCOL_VERSION);
	}

	cl.servercount = MSG_ReadLong(&net_message);
	cl.attractloop = MSG_ReadByte(&net_message);

	/* game directory */
	str = MSG_ReadString(&net_message);
	Q_strlcpy(cl.gamedir, str, sizeof(cl.gamedir));

	/* set gamedir */
	if ((*str && (!fs_gamedirvar->string || !*fs_gamedirvar->string ||
		  strcmp(fs_gamedirvar->string, str))) ||
		(!*str && (fs_gamedirvar->string && !*fs_gamedirvar->string)))
	{
		Cvar_Set("game", str);
	}

	/* parse player entity number */
	cl.playernum = MSG_ReadShort(&net_message);

	/* get the full level name */
	str = MSG_ReadString(&net_message);

	if (cl.playernum == -1)
	{
		/* playing a cinematic or showing a pic, not a level */
		SCR_PlayCinematic(str);
	}
	else
	{
		/* seperate the printfs so the server
		 * message can have a color */
		Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
		Com_Printf("%c%s\n", 2, str);

		/* need to prep refresh at next oportunity */
		cl.refresh_prepped = false;
	}
}

void
CL_ParseBaseline(void)
{
	entity_state_t *es;
	unsigned bits;
	int newnum;
	entity_state_t nullstate;

	memset(&nullstate, 0, sizeof(nullstate));

	newnum = CL_ParseEntityBits(&bits);
	es = &cl_entities[newnum].baseline;
	CL_ParseDelta(&nullstate, es, newnum, bits);
}

void
CL_LoadClientinfo(clientinfo_t *ci, char *s)
{
	int i;
	char *t;
	char model_name[MAX_QPATH];
	char skin_name[MAX_QPATH];
	char model_filename[MAX_QPATH];
	char skin_filename[MAX_QPATH];
	char weapon_filename[MAX_QPATH];

	Q_strlcpy(ci->cinfo, s, sizeof(ci->cinfo));
	s = ci->cinfo;

	/* isolate the player's name */
	Q_strlcpy(ci->name, s, sizeof(ci->name));
	t = strstr(s, "\\");

	if (t)
	{
		ci->name[t - s] = 0;
		s = t + 1;
	}

	if (cl_noskins->value || (*s == 0))
	{
		strcpy(model_filename, "players/male/tris.md2");
		strcpy(weapon_filename, "players/male/weapon.md2");
		strcpy(skin_filename, "players/male/grunt.pcx");
		strcpy(ci->iconname, "/players/male/grunt_i.pcx");
		ci->model = R_RegisterModel(model_filename);
		memset(ci->weaponmodel, 0, sizeof(ci->weaponmodel));
		ci->weaponmodel[0] = R_RegisterModel(weapon_filename);
		ci->skin = R_RegisterSkin(skin_filename);
		ci->icon = Draw_FindPic(ci->iconname);
	}
	else
	{
		/* isolate the model name */
		strcpy(model_name, s);
		t = strstr(model_name, "/");

		if (!t)
		{
			t = strstr(model_name, "\\");
		}

		if (!t)
		{
			t = model_name;
		}

		*t = 0;

		/* isolate the skin name */
		strcpy(skin_name, s + strlen(model_name) + 1);

		/* model file */
		Com_sprintf(model_filename, sizeof(model_filename),
				"players/%s/tris.md2", model_name);
		ci->model = R_RegisterModel(model_filename);

		if (!ci->model)
		{
			strcpy(model_name, "male");
			Com_sprintf(model_filename, sizeof(model_filename),
					"players/male/tris.md2");
			ci->model = R_RegisterModel(model_filename);
		}

		/* skin file */
		Com_sprintf(skin_filename, sizeof(skin_filename),
				"players/%s/%s.pcx", model_name, skin_name);
		ci->skin = R_RegisterSkin(skin_filename);

		/* if we don't have the skin and the model wasn't male,
		 * see if the male has it (this is for CTF's skins) */
		if (!ci->skin && Q_stricmp(model_name, "male"))
		{
			/* change model to male */
			strcpy(model_name, "male");
			Com_sprintf(model_filename, sizeof(model_filename),
					"players/male/tris.md2");
			ci->model = R_RegisterModel(model_filename);

			/* see if the skin exists for the male model */
			Com_sprintf(skin_filename, sizeof(skin_filename),
					"players/%s/%s.pcx", model_name, skin_name);
			ci->skin = R_RegisterSkin(skin_filename);
		}

		/* if we still don't have a skin, it means that the male model didn't have
		 * it, so default to grunt */
		if (!ci->skin)
		{
			/* see if the skin exists for the male model */
			Com_sprintf(skin_filename, sizeof(skin_filename),
					"players/%s/grunt.pcx", model_name);
			ci->skin = R_RegisterSkin(skin_filename);
		}

		/* weapon file */
		for (i = 0; i < num_cl_weaponmodels; i++)
		{
			Com_sprintf(weapon_filename, sizeof(weapon_filename),
					"players/%s/%s", model_name, cl_weaponmodels[i]);
			ci->weaponmodel[i] = R_RegisterModel(weapon_filename);

			if (!ci->weaponmodel[i] && (strcmp(model_name, "cyborg") == 0))
			{
				/* try male */
				Com_sprintf(weapon_filename, sizeof(weapon_filename),
						"players/male/%s", cl_weaponmodels[i]);
				ci->weaponmodel[i] = R_RegisterModel(weapon_filename);
			}

			if (!cl_vwep->value)
			{
				break; /* only one when vwep is off */
			}
		}

		/* icon file */
		Com_sprintf(ci->iconname, sizeof(ci->iconname),
				"/players/%s/%s_i.pcx", model_name, skin_name);
		ci->icon = Draw_FindPic(ci->iconname);
	}

	/* must have loaded all data types to be valid */
	if (!ci->skin || !ci->icon || !ci->model || !ci->weaponmodel[0])
	{
		ci->skin = NULL;
		ci->icon = NULL;
		ci->model = NULL;
		ci->weaponmodel[0] = NULL;
		return;
	}
}

/*
 * Load the skin, icon, and model for a client
 */
void
CL_ParseClientinfo(int player)
{
	char *s;
	clientinfo_t *ci;

	s = cl.configstrings[player + CS_PLAYERSKINS];

	ci = &cl.clientinfo[player];

	CL_LoadClientinfo(ci, s);
}

void
CL_ParseConfigString(void)
{
	int i, length;
	char *s;
	char olds[MAX_QPATH];

	i = MSG_ReadShort(&net_message);

	if ((i < 0) || (i >= MAX_CONFIGSTRINGS))
	{
		Com_Error(ERR_DROP, "configstring > MAX_CONFIGSTRINGS");
	}

	s = MSG_ReadString(&net_message);

	Q_strlcpy(olds, cl.configstrings[i], sizeof(olds));

	length = strlen(s);
	if (length > sizeof(cl.configstrings) - sizeof(cl.configstrings[0])*i - 1)
	{
		Com_Error(ERR_DROP, "CL_ParseConfigString: oversize configstring");
	}

	strcpy(cl.configstrings[i], s);

	/* do something apropriate */
	if ((i >= CS_LIGHTS) && (i < CS_LIGHTS + MAX_LIGHTSTYLES))
	{
		CL_SetLightstyle(i - CS_LIGHTS);
	}
	else if (i == CS_CDTRACK)
	{
		if (cl.refresh_prepped)
		{
			OGG_PlayTrack((int)strtol(cl.configstrings[CS_CDTRACK], (char **)NULL, 10));
		}
	}
	else if ((i >= CS_MODELS) && (i < CS_MODELS + MAX_MODELS))
	{
		if (cl.refresh_prepped)
		{
			cl.model_draw[i - CS_MODELS] = R_RegisterModel(cl.configstrings[i]);

			if (cl.configstrings[i][0] == '*')
			{
				cl.model_clip[i - CS_MODELS] = CM_InlineModel(cl.configstrings[i]);
			}

			else
			{
				cl.model_clip[i - CS_MODELS] = NULL;
			}
		}
	}
	else if ((i >= CS_SOUNDS) && (i < CS_SOUNDS + MAX_MODELS))
	{
		if (cl.refresh_prepped)
		{
			cl.sound_precache[i - CS_SOUNDS] =
				S_RegisterSound(cl.configstrings[i]);
		}
	}
	else if ((i >= CS_IMAGES) && (i < CS_IMAGES + MAX_MODELS))
	{
		if (cl.refresh_prepped)
		{
			cl.image_precache[i - CS_IMAGES] = Draw_FindPic(cl.configstrings[i]);
		}
	}
	else if ((i >= CS_PLAYERSKINS) && (i < CS_PLAYERSKINS + MAX_CLIENTS))
	{
		if (cl.refresh_prepped && strcmp(olds, s))
		{
			CL_ParseClientinfo(i - CS_PLAYERSKINS);
		}
	}
}

void
CL_ParseStartSoundPacket(void)
{
	vec3_t pos_v;
	float *pos;
	int channel, ent;
	int sound_num;
	float volume;
	float attenuation;
	int flags;
	float ofs;

	flags = MSG_ReadByte(&net_message);
	sound_num = MSG_ReadByte(&net_message);

	if (flags & SND_VOLUME)
	{
		volume = MSG_ReadByte(&net_message) / 255.0f;
	}

	else
	{
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	}

	if (flags & SND_ATTENUATION)
	{
		attenuation = MSG_ReadByte(&net_message) / 64.0f;
	}

	else
	{
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;
	}

	if (flags & SND_OFFSET)
	{
		ofs = MSG_ReadByte(&net_message) / 1000.0f;
	}

	else
	{
		ofs = 0;
	}

	if (flags & SND_ENT)
	{
		/* entity reletive */
		channel = MSG_ReadShort(&net_message);
		ent = channel >> 3;

		if (ent > MAX_EDICTS)
		{
			Com_Error(ERR_DROP, "CL_ParseStartSoundPacket: ent = %i", ent);
		}

		channel &= 7;
	}
	else
	{
		ent = 0;
		channel = 0;
	}

	if (flags & SND_POS)
	{
		/* positioned in space */
		MSG_ReadPos(&net_message, pos_v);

		pos = pos_v;
	}
	else
	{
		/* use entity number */
		pos = NULL;
	}

	if (!cl.sound_precache[sound_num])
	{
		return;
	}

	S_StartSound(pos, ent, channel, cl.sound_precache[sound_num],
			volume, attenuation, ofs);
}

void
SHOWNET(char *s)
{
	if (cl_shownet->value >= 2)
	{
		Com_Printf("%3i:%s\n", net_message.readcount - 1, s);
	}
}

void
CL_ParseServerMessage(void)
{
	int cmd;
	char *s;
	int i;

	/* if recording demos, copy the message out */
	if (cl_shownet->value == 1)
	{
		Com_Printf("%i ", net_message.cursize);
	}

	else if (cl_shownet->value >= 2)
	{
		Com_Printf("------------------\n");
	}

	/* parse the message */
	while (1)
	{
		if (net_message.readcount > net_message.cursize)
		{
			Com_Error(ERR_DROP, "CL_ParseServerMessage: Bad server message");
			break;
		}

		cmd = MSG_ReadByte(&net_message);

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			break;
		}

		if (cl_shownet->value >= 2)
		{
			if (!svc_strings[cmd])
			{
				Com_Printf("%3i:BAD CMD %i\n", net_message.readcount - 1, cmd);
			}

			else
			{
				SHOWNET(svc_strings[cmd]);
			}
		}

		/* other commands */
		switch (cmd)
		{
			default:
				Com_Error(ERR_DROP, "CL_ParseServerMessage: Illegible server message\n");
				break;

			case svc_nop:
				break;

			case svc_disconnect:
				Com_Error(ERR_DISCONNECT, "Server disconnected\n");
				break;

			case svc_reconnect:
				Com_Printf("Server disconnected, reconnecting\n");

				if (cls.download)
				{
					/* close download */
					fclose(cls.download);
					cls.download = NULL;
				}

				cls.state = ca_connecting;
				cls.connect_time = -99999; /* CL_CheckForResend() will fire immediately */
				break;

			case svc_print:
				i = MSG_ReadByte(&net_message);

				if (i == PRINT_CHAT)
				{
					S_StartLocalSound("misc/talk.wav");
					con.ormask = 128;
				}

				Com_Printf("%s", MSG_ReadString(&net_message));
				con.ormask = 0;
				break;

			case svc_centerprint:
				SCR_CenterPrint(MSG_ReadString(&net_message));
				break;

			case svc_stufftext:
				s = MSG_ReadString(&net_message);
				Com_DPrintf("stufftext: %s\n", s);
				Cbuf_AddText(s);
				break;

			case svc_serverdata:
				Cbuf_Execute();  /* make sure any stuffed commands are done */
				CL_ParseServerData();
				break;

			case svc_configstring:
				CL_ParseConfigString();
				break;

			case svc_sound:
				CL_ParseStartSoundPacket();
				break;

			case svc_spawnbaseline:
				CL_ParseBaseline();
				break;

			case svc_temp_entity:
				CL_ParseTEnt();
				break;

			case svc_muzzleflash:
				CL_AddMuzzleFlash();
				break;

			case svc_muzzleflash2:
				CL_AddMuzzleFlash2();
				break;

			case svc_download:
				CL_ParseDownload();
				break;

			case svc_frame:
				CL_ParseFrame();
				break;

			case svc_inventory:
				CL_ParseInventory();
				break;

			case svc_layout:
				s = MSG_ReadString(&net_message);
				Q_strlcpy(cl.layout, s, sizeof(cl.layout));
				break;

			case svc_playerinfo:
			case svc_packetentities:
			case svc_deltapacketentities:
				Com_Error(ERR_DROP, "Out of place frame data");
				break;
		}
	}

	CL_AddNetgraph();

	/* we don't know if it is ok to save a demo message
	   until after we have parsed the frame */
	if (cls.demorecording && !cls.demowaiting)
	{
		CL_WriteDemoMessage();
	}
}

