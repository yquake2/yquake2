/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "header/client.h"

extern	struct model_s	*cl_mod_powerscreen;
int	vidref_val;

int	bitcounts[32]; /* just for protocol profiling */
int CL_ParseEntityBits (unsigned *bits) {
	unsigned	b, total;
	int			i;
	int			number;

	total = MSG_ReadByte (&net_message);

	if (total & U_MOREBITS1) {
		b = MSG_ReadByte (&net_message);
		total |= b<<8;
	}

	if (total & U_MOREBITS2) {
		b = MSG_ReadByte (&net_message);
		total |= b<<16;
	}

	if (total & U_MOREBITS3) {
		b = MSG_ReadByte (&net_message);
		total |= b<<24;
	}

	/* count the bits for net profiling */
	for (i=0 ; i<32 ; i++)
		if (total&(1<<i))
			bitcounts[i]++;

	if (total & U_NUMBER16)
		number = MSG_ReadShort (&net_message);

	else
		number = MSG_ReadByte (&net_message);

	*bits = total;

	return number;
}

void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int number, int bits) {
	/* set everything to the state we are delta'ing from */
	*to = *from;

	VectorCopy (from->origin, to->old_origin);
	to->number = number;

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte (&net_message);

	if (bits & U_MODEL2)
		to->modelindex2 = MSG_ReadByte (&net_message);

	if (bits & U_MODEL3)
		to->modelindex3 = MSG_ReadByte (&net_message);

	if (bits & U_MODEL4)
		to->modelindex4 = MSG_ReadByte (&net_message);

	if (bits & U_FRAME8)
		to->frame = MSG_ReadByte (&net_message);

	if (bits & U_FRAME16)
		to->frame = MSG_ReadShort (&net_message);

	/* used for laser colors */
	if ((bits & U_SKIN8) && (bits & U_SKIN16))
		to->skinnum = MSG_ReadLong(&net_message);

	else if (bits & U_SKIN8)
		to->skinnum = MSG_ReadByte(&net_message);

	else if (bits & U_SKIN16)
		to->skinnum = MSG_ReadShort(&net_message);

	if ( (bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16) )
		to->effects = MSG_ReadLong(&net_message);

	else if (bits & U_EFFECTS8)
		to->effects = MSG_ReadByte(&net_message);

	else if (bits & U_EFFECTS16)
		to->effects = MSG_ReadShort(&net_message);

	if ( (bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16) )
		to->renderfx = MSG_ReadLong(&net_message);

	else if (bits & U_RENDERFX8)
		to->renderfx = MSG_ReadByte(&net_message);

	else if (bits & U_RENDERFX16)
		to->renderfx = MSG_ReadShort(&net_message);

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord (&net_message);

	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord (&net_message);

	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord (&net_message);

	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle(&net_message);

	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle(&net_message);

	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle(&net_message);

	if (bits & U_OLDORIGIN)
		MSG_ReadPos (&net_message, to->old_origin);

	if (bits & U_SOUND)
		to->sound = MSG_ReadByte (&net_message);

	if (bits & U_EVENT)
		to->event = MSG_ReadByte (&net_message);

	else
		to->event = 0;

	if (bits & U_SOLID)
		to->solid = MSG_ReadShort (&net_message);
}

/*
 * Parses deltas from the given base and adds the resulting entity to
 * the current frame
 */
void CL_DeltaEntity (frame_t *frame, int newnum, entity_state_t *old,
                     int bits) {
	centity_t		*ent;
	entity_state_t	*state;

	ent = &cl_entities[newnum];

	state = &cl_parse_entities[cl.parse_entities & (MAX_PARSE_ENTITIES-1)];
	cl.parse_entities++;
	frame->num_entities++;

	CL_ParseDelta (old, state, newnum, bits);

	/* some data changes will force no lerping */
	if (state->modelindex != ent->current.modelindex
	        || state->modelindex2 != ent->current.modelindex2
	        || state->modelindex3 != ent->current.modelindex3
	        || state->modelindex4 != ent->current.modelindex4
	        || state->event == EV_PLAYER_TELEPORT
	        || state->event == EV_OTHER_TELEPORT
	        || abs((int)(state->origin[0] - ent->current.origin[0])) > 512
	        || abs((int)(state->origin[1] - ent->current.origin[1])) > 512
	        || abs((int)(state->origin[2] - ent->current.origin[2])) > 512
	   ) {
		ent->serverframe = -99;
	}
	/* wasn't in last update, so initialize some things */
	if (ent->serverframe != cl.frame.serverframe - 1) {
		ent->trailcount = 1024; /* for diminishing rocket / grenade trails */
		
		/* duplicate the current state so 
		 * lerping doesn't hurt anything */
		ent->prev = *state;

		if (state->event == EV_OTHER_TELEPORT) {
			VectorCopy (state->origin, ent->prev.origin);
			VectorCopy (state->origin, ent->lerp_origin);

		} else {
			VectorCopy (state->old_origin, ent->prev.origin);
			VectorCopy (state->old_origin, ent->lerp_origin);
		}

	} else {
		/* shuffle the last state to previous */
		ent->prev = ent->current;
	}

	ent->serverframe = cl.frame.serverframe;
	ent->current = *state;
}

/*
 * An svc_packetentities has just been parsed, deal with the rest of the
 * data stream.
 */
void CL_ParsePacketEntities (frame_t *oldframe, frame_t *newframe) {
	unsigned int	newnum;
	unsigned		bits;
	entity_state_t
	*oldstate = NULL;
	int			oldindex, oldnum;

	newframe->parse_entities = cl.parse_entities;
	newframe->num_entities = 0;

	/* delta from the entities present in oldframe */
	oldindex = 0;

	if (!oldframe)
		oldnum = 99999;

	else {
		if (oldindex >= oldframe->num_entities)
			oldnum = 99999;

		else {
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

	while (1) {
		newnum = CL_ParseEntityBits (&bits);

		if (newnum >= MAX_EDICTS)
			Com_Error (ERR_DROP,"CL_ParsePacketEntities: bad number:%i", newnum);

		if (net_message.readcount > net_message.cursize)
			Com_Error (ERR_DROP,"CL_ParsePacketEntities: end of message");

		if (!newnum)
			break;

		while (oldnum < newnum) {
			/* one or more entities from the old packet are unchanged */
			if (cl_shownet->value == 3)
				Com_Printf ("   unchanged: %i\n", oldnum);

			CL_DeltaEntity (newframe, oldnum, oldstate, 0);

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;

			else {
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
		}

		if (bits & U_REMOVE) {
			/* the entity present in oldframe is not in the current frame */
			if (cl_shownet->value == 3)
				Com_Printf ("   remove: %i\n", newnum);

			if (oldnum != newnum)
				Com_Printf ("U_REMOVE: oldnum != newnum\n");

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;

			else {
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}

			continue;
		}

		if (oldnum == newnum) {
			/* delta from previous state */
			if (cl_shownet->value == 3)
				Com_Printf ("   delta: %i\n", newnum);

			CL_DeltaEntity (newframe, newnum, oldstate, bits);

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;

			else {
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}

			continue;
		}

		if (oldnum > newnum) {
			/* delta from baseline */
			if (cl_shownet->value == 3)
				Com_Printf ("   baseline: %i\n", newnum);

			CL_DeltaEntity (newframe, newnum, &cl_entities[newnum].baseline, bits);
			continue;
		}

	}

	/* any remaining entities in the old frame are copied over */
	while (oldnum != 99999) {
		/* one or more entities from the old packet are unchanged */
		if (cl_shownet->value == 3)
			Com_Printf ("   unchanged: %i\n", oldnum);

		CL_DeltaEntity (newframe, oldnum, oldstate, 0);

		oldindex++;

		if (oldindex >= oldframe->num_entities)
			oldnum = 99999;

		else {
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}
}

void CL_ParsePlayerstate (frame_t *oldframe, frame_t *newframe) {
	int			flags;
	player_state_t	*state;
	int			i;
	int			statbits;

	state = &newframe->playerstate;

	/* clear to old value before delta parsing */
	if (oldframe)
		*state = oldframe->playerstate;

	else
		memset (state, 0, sizeof(*state));

	flags = MSG_ReadShort (&net_message);

	/* parse the pmove_state_t */
	if (flags & PS_M_TYPE)
		state->pmove.pm_type = MSG_ReadByte (&net_message);

	if (flags & PS_M_ORIGIN) {
		state->pmove.origin[0] = MSG_ReadShort (&net_message);
		state->pmove.origin[1] = MSG_ReadShort (&net_message);
		state->pmove.origin[2] = MSG_ReadShort (&net_message);
	}

	if (flags & PS_M_VELOCITY) {
		state->pmove.velocity[0] = MSG_ReadShort (&net_message);
		state->pmove.velocity[1] = MSG_ReadShort (&net_message);
		state->pmove.velocity[2] = MSG_ReadShort (&net_message);
	}

	if (flags & PS_M_TIME)
		state->pmove.pm_time = MSG_ReadByte (&net_message);

	if (flags & PS_M_FLAGS)
		state->pmove.pm_flags = MSG_ReadByte (&net_message);

	if (flags & PS_M_GRAVITY)
		state->pmove.gravity = MSG_ReadShort (&net_message);

	if (flags & PS_M_DELTA_ANGLES) {
		state->pmove.delta_angles[0] = MSG_ReadShort (&net_message);
		state->pmove.delta_angles[1] = MSG_ReadShort (&net_message);
		state->pmove.delta_angles[2] = MSG_ReadShort (&net_message);
	}

	if (cl.attractloop)
		state->pmove.pm_type = PM_FREEZE; /* demo playback */

	/* parse the rest of the player_state_t */
	if (flags & PS_VIEWOFFSET) {
		state->viewoffset[0] = MSG_ReadChar (&net_message) * 0.25f;
		state->viewoffset[1] = MSG_ReadChar (&net_message) * 0.25f;
		state->viewoffset[2] = MSG_ReadChar (&net_message) * 0.25f;
	}

	if (flags & PS_VIEWANGLES) {
		state->viewangles[0] = MSG_ReadAngle16 (&net_message);
		state->viewangles[1] = MSG_ReadAngle16 (&net_message);
		state->viewangles[2] = MSG_ReadAngle16 (&net_message);
	}

	if (flags & PS_KICKANGLES) {
		state->kick_angles[0] = MSG_ReadChar (&net_message) * 0.25f;
		state->kick_angles[1] = MSG_ReadChar (&net_message) * 0.25f;
		state->kick_angles[2] = MSG_ReadChar (&net_message) * 0.25f;
	}

	if (flags & PS_WEAPONINDEX) {
		state->gunindex = MSG_ReadByte (&net_message);
	}

	if (flags & PS_WEAPONFRAME) {
		state->gunframe = MSG_ReadByte (&net_message);
		state->gunoffset[0] = MSG_ReadChar (&net_message)*0.25f;
		state->gunoffset[1] = MSG_ReadChar (&net_message)*0.25f;
		state->gunoffset[2] = MSG_ReadChar (&net_message)*0.25f;
		state->gunangles[0] = MSG_ReadChar (&net_message)*0.25f;
		state->gunangles[1] = MSG_ReadChar (&net_message)*0.25f;
		state->gunangles[2] = MSG_ReadChar (&net_message)*0.25f;
	}

	if (flags & PS_BLEND) {
		state->blend[0] = MSG_ReadByte (&net_message)/255.0f;
		state->blend[1] = MSG_ReadByte (&net_message)/255.0f;
		state->blend[2] = MSG_ReadByte (&net_message)/255.0f;
		state->blend[3] = MSG_ReadByte (&net_message)/255.0f;
	}

	if (flags & PS_FOV)
		state->fov = (float)MSG_ReadByte (&net_message);

	if (flags & PS_RDFLAGS)
		state->rdflags = MSG_ReadByte (&net_message);

	/* parse stats */
	statbits = MSG_ReadLong (&net_message);

	for (i=0 ; i<MAX_STATS ; i++)
		if (statbits & (1<<i) )
			state->stats[i] = MSG_ReadShort(&net_message);
}

void CL_FireEntityEvents (frame_t *frame) {
	entity_state_t		*s1;
	int					pnum, num;

	for (pnum = 0 ; pnum<frame->num_entities ; pnum++) {
		num = (frame->parse_entities + pnum)&(MAX_PARSE_ENTITIES-1);
		s1 = &cl_parse_entities[num];

		if (s1->event)
			CL_EntityEvent (s1);

		if (s1->effects & EF_TELEPORTER)
			CL_TeleporterParticles (s1);
	}
}

void CL_ParseFrame (void) {
	int			cmd;
	int			len;
	frame_t		*old;

	memset (&cl.frame, 0, sizeof(cl.frame));

	cl.frame.serverframe = MSG_ReadLong (&net_message);
	cl.frame.deltaframe = MSG_ReadLong (&net_message);
	cl.frame.servertime = cl.frame.serverframe*100;

	/* BIG HACK to let old demos continue to work */
	if (cls.serverProtocol != 26)
		cl.surpressCount = MSG_ReadByte (&net_message);

	if (cl_shownet->value == 3)
		Com_Printf ("   frame:%i  delta:%i\n", cl.frame.serverframe,
		            cl.frame.deltaframe);

	/*
	 * If the frame is delta compressed from data that we
	 * no longer have available, we must suck up the rest of
	 * the frame, but not use it, then ask for a non-compressed
	 * message
	 */
	if (cl.frame.deltaframe <= 0) {
		cl.frame.valid = true; /* uncompressed frame */
		old = NULL;
		cls.demowaiting = false; /* we can start recording now */

	} else {
		old = &cl.frames[cl.frame.deltaframe & UPDATE_MASK];

		if (!old->valid) {
			/* should never happen */
			Com_Printf ("Delta from invalid frame (not supposed to happen!).\n");
		}

		if (old->serverframe != cl.frame.deltaframe) {
			/* The frame that the server did the delta from
			 * is too old, so we can't reconstruct it properly. */
			Com_Printf ("Delta frame too old.\n");

		} else if (cl.parse_entities - old->parse_entities > MAX_PARSE_ENTITIES-128) {
			Com_Printf ("Delta parse_entities too old.\n");

		} else
			cl.frame.valid = true; /* valid delta parse */
	}

	/* clamp time */
	if (cl.time > cl.frame.servertime)
		cl.time = cl.frame.servertime;

	else if (cl.time < cl.frame.servertime - 100)
		cl.time = cl.frame.servertime - 100;

	/* read areabits */
	len = MSG_ReadByte (&net_message);
	MSG_ReadData (&net_message, &cl.frame.areabits, len);

	/* read playerinfo */
	cmd = MSG_ReadByte (&net_message);
	SHOWNET(svc_strings[cmd]);

	if (cmd != svc_playerinfo)
		Com_Error (ERR_DROP, "CL_ParseFrame: 0x%X not playerinfo", cmd);

	CL_ParsePlayerstate (old, &cl.frame);

	/* read packet entities */
	cmd = MSG_ReadByte (&net_message);
	SHOWNET(svc_strings[cmd]);

	if (cmd != svc_packetentities)
		Com_Error (ERR_DROP, "CL_ParseFrame: 0x%X not packetentities", cmd);

	CL_ParsePacketEntities (old, &cl.frame);

	/* save the frame off in the backup array for later delta comparisons */
	cl.frames[cl.frame.serverframe & UPDATE_MASK] = cl.frame;

	if (cl.frame.valid) {
		/* getting a valid frame message ends the connection process */
		if (cls.state != ca_active) {
			cls.state = ca_active;
			cl.force_refdef = true;
			cl.predicted_origin[0] = cl.frame.playerstate.pmove.origin[0]*0.125f;
			cl.predicted_origin[1] = cl.frame.playerstate.pmove.origin[1]*0.125f;
			cl.predicted_origin[2] = cl.frame.playerstate.pmove.origin[2]*0.125f;
			VectorCopy (cl.frame.playerstate.viewangles, cl.predicted_angles);

			if (cls.disable_servercount != cl.servercount
			        && cl.refresh_prepped)
				SCR_EndLoadingPlaque (); /* get rid of loading plaque */

			cl.sound_prepped = true;
		}

		/* fire entity events */
		CL_FireEntityEvents (&cl.frame);

		if (!(!cl_predict->value || (cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION)))
			CL_CheckPredictionError();
	}
}

struct model_s *S_RegisterSexedModel (entity_state_t *ent, char *base) {
	int				n;
	char			*p;
	struct model_s	*md2;
	char			model[MAX_QPATH];
	char			buffer[MAX_QPATH];

	/* determine what model the client is using */
	model[0] = 0;
	n = CS_PLAYERSKINS + ent->number - 1;

	if (cl.configstrings[n][0]) {
		p = strchr(cl.configstrings[n], '\\');

		if (p) {
			p += 1;
			strcpy(model, p);
			p = strchr(model, '/');

			if (p)
				*p = 0;
		}
	}

	/* if we can't figure it out, they're male */
	if (!model[0])
		strcpy(model, "male");

	Com_sprintf (buffer, sizeof(buffer), "players/%s/%s", model, base+1);
	md2 = re.RegisterModel(buffer);

	if (!md2) {
		/* not found, try default weapon model */
		Com_sprintf (buffer, sizeof(buffer), "players/%s/weapon.md2", model);
		md2 = re.RegisterModel(buffer);

		if (!md2) {
			/* no, revert to the male model */
			Com_sprintf (buffer, sizeof(buffer), "players/%s/%s", "male", base+1);
			md2 = re.RegisterModel(buffer);

			if (!md2) {
				/* last try, default male weapon.md2 */
				Com_sprintf (buffer, sizeof(buffer), "players/male/weapon.md2");
				md2 = re.RegisterModel(buffer);
			}
		}
	}

	return md2;
}

extern int Developer_searchpath (int who);

void CL_AddPacketEntities (frame_t *frame) {
	entity_t			ent = {0};
	entity_state_t		*s1;
	float				autorotate;
	int					i;
	int					pnum;
	centity_t			*cent;
	int					autoanim;
	clientinfo_t		*ci;
	unsigned int		effects, renderfx;

	/* bonus items rotate at a fixed rate */
	autorotate = anglemod(cl.time*0.1f);

	/* brush models can auto animate their frames */
	autoanim = 2*cl.time/1000;

	for (pnum = 0 ; pnum<frame->num_entities ; pnum++) {
		s1 = &cl_parse_entities[(frame->parse_entities+pnum)&(MAX_PARSE_ENTITIES-1)];

		cent = &cl_entities[s1->number];

		effects = s1->effects;
		renderfx = s1->renderfx;

		/* set frame */
		if (effects & EF_ANIM01)
			ent.frame = autoanim & 1;

		else if (effects & EF_ANIM23)
			ent.frame = 2 + (autoanim & 1);

		else if (effects & EF_ANIM_ALL)
			ent.frame = autoanim;

		else if (effects & EF_ANIM_ALLFAST)
			ent.frame = cl.time / 100;

		else
			ent.frame = s1->frame;

		/* quad and pent can do different things on client */
		if (effects & EF_PENT) {
			effects &= ~EF_PENT;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_RED;
		}

		if (effects & EF_QUAD) {
			effects &= ~EF_QUAD;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_BLUE;
		}

		if (effects & EF_DOUBLE) {
			effects &= ~EF_DOUBLE;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_DOUBLE;
		}

		if (effects & EF_HALF_DAMAGE) {
			effects &= ~EF_HALF_DAMAGE;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_HALF_DAM;
		}

		ent.oldframe = cent->prev.frame;
		ent.backlerp = 1.0f - cl.lerpfrac;

		if (renderfx & (RF_FRAMELERP|RF_BEAM)) {
			/* step origin discretely, because the frames
			 * do the animation properly */
			VectorCopy (cent->current.origin, ent.origin);
			VectorCopy (cent->current.old_origin, ent.oldorigin);

		} else {
			/* interpolate origin */
			for (i=0 ; i<3 ; i++) {
				ent.origin[i] = ent.oldorigin[i] = cent->prev.origin[i] + cl.lerpfrac *
				                                   (cent->current.origin[i] - cent->prev.origin[i]);
			}
		}

		/* tweak the color of beams */
		if ( renderfx & RF_BEAM ) {
			/* the four beam colors are encoded in 32 bits of skinnum (hack) */
			ent.alpha = 0.30f;
			ent.skinnum = (s1->skinnum >> ((rand() % 4)*8)) & 0xff;
			ent.model = NULL;

		} else {
			/* set skin */
			if (s1->modelindex == 255) {
				/* use custom player skin */
				ent.skinnum = 0;
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				ent.skin = ci->skin;
				ent.model = ci->model;

				if (!ent.skin || !ent.model) {
					ent.skin = cl.baseclientinfo.skin;
					ent.model = cl.baseclientinfo.model;
				}

				if (renderfx & RF_USE_DISGUISE) {
					if (ent.skin != NULL) {
						if(!strncmp((char *)ent.skin, "players/male", 12)) {
							ent.skin = re.RegisterSkin ("players/male/disguise.pcx");
							ent.model = re.RegisterModel ("players/male/tris.md2");

						} else if(!strncmp((char *)ent.skin, "players/female", 14)) {
							ent.skin = re.RegisterSkin ("players/female/disguise.pcx");
							ent.model = re.RegisterModel ("players/female/tris.md2");

						} else if(!strncmp((char *)ent.skin, "players/cyborg", 14)) {
							ent.skin = re.RegisterSkin ("players/cyborg/disguise.pcx");
							ent.model = re.RegisterModel ("players/cyborg/tris.md2");
						}
					}
				}

			} else {
				ent.skinnum = s1->skinnum;
				ent.skin = NULL;
				ent.model = cl.model_draw[s1->modelindex];
			}
		}

		/* only used for black hole model right now */
		if (renderfx & RF_TRANSLUCENT && !(renderfx & RF_BEAM))
			ent.alpha = 0.70f;

		/* render effects (fullbright, translucent, etc) */
		if ((effects & EF_COLOR_SHELL))
			ent.flags = 0;	/* renderfx go on color shell entity */

		else
			ent.flags = renderfx;

		/* calculate angles */
		if (effects & EF_ROTATE) {
			/* some bonus items auto-rotate */
			ent.angles[0] = 0;
			ent.angles[1] = autorotate;
			ent.angles[2] = 0;

		} else if (effects & EF_SPINNINGLIGHTS) {
			ent.angles[0] = 0;
			ent.angles[1] = anglemod(cl.time/2) + s1->angles[1];
			ent.angles[2] = 180;
			{
				vec3_t forward;
				vec3_t start;

				AngleVectors (ent.angles, forward, NULL, NULL);
				VectorMA (ent.origin, 64, forward, start);
				V_AddLight (start, 100, 1, 0, 0);
			}

		} else {
			/* interpolate angles */
			float	a1, a2;

			for (i=0 ; i<3 ; i++) {
				a1 = cent->current.angles[i];
				a2 = cent->prev.angles[i];
				ent.angles[i] = LerpAngle (a2, a1, cl.lerpfrac);
			}
		}

		if (s1->number == cl.playernum+1) {
			ent.flags |= RF_VIEWERMODEL;

			if (effects & EF_FLAG1)
				V_AddLight (ent.origin, 225, 1.0f, 0.1f, 0.1f);

			else if (effects & EF_FLAG2)
				V_AddLight (ent.origin, 225, 0.1f, 0.1f, 1.0f);

			else if (effects & EF_TAGTRAIL)
				V_AddLight (ent.origin, 225, 1.0f, 1.0f, 0.0f);

			else if (effects & EF_TRACKERTRAIL)
				V_AddLight (ent.origin, 225, -1.0f, -1.0f, -1.0f);

			continue;
		}

		/* if set to invisible, skip */
		if (!s1->modelindex)
			continue;

		if (effects & EF_BFG) {
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.30f;
		}

		if (effects & EF_PLASMA) {
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.6f;
		}

		if (effects & EF_SPHERETRANS) {
			ent.flags |= RF_TRANSLUCENT;

			if (effects & EF_TRACKERTRAIL)
				ent.alpha = 0.6f;

			else
				ent.alpha = 0.3f;
		}

		/* add to refresh list */
		V_AddEntity (&ent);


		/* color shells generate a seperate entity for the main model */
		if (effects & EF_COLOR_SHELL) {
			/* all of the solo colors are fine.  we need to catch any of
			 * the combinations that look bad (double & half) and turn
			 * them into the appropriate color, and make double/quad
			 * something special */
			if (renderfx & RF_SHELL_HALF_DAM) {
				if(Developer_searchpath(2) == 2) {
					/* ditch the half damage shell if any of red, blue, or double are on */
					if (renderfx & (RF_SHELL_RED|RF_SHELL_BLUE|RF_SHELL_DOUBLE))
						renderfx &= ~RF_SHELL_HALF_DAM;
				}
			}

			if (renderfx & RF_SHELL_DOUBLE) {
				if(Developer_searchpath(2) == 2) {
					/* lose the yellow shell if we have a red, blue, or green shell */
					if (renderfx & (RF_SHELL_RED|RF_SHELL_BLUE|RF_SHELL_GREEN))
						renderfx &= ~RF_SHELL_DOUBLE;

					/* if we have a red shell, turn it to purple by adding blue */
					if (renderfx & RF_SHELL_RED)
						renderfx |= RF_SHELL_BLUE;

					/* if we have a blue shell (and not a red shell), turn it to cyan by adding green */
					else if (renderfx & RF_SHELL_BLUE) {
						/* go to green if it's on already, otherwise do cyan (flash green) */
						if (renderfx & RF_SHELL_GREEN)
							renderfx &= ~RF_SHELL_BLUE;

						else
							renderfx |= RF_SHELL_GREEN;
					}
				}
			}

			ent.flags = renderfx | RF_TRANSLUCENT;
			ent.alpha = 0.30f;
			V_AddEntity (&ent);
		}

		ent.skin = NULL; /* never use a custom skin on others */
		ent.skinnum = 0;
		ent.flags = 0;
		ent.alpha = 0;

		/* duplicate for linked models */
		if (s1->modelindex2) {
			if (s1->modelindex2 == 255) {
				/* custom weapon */
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				i = (s1->skinnum >> 8); /* 0 is default weapon model */

				if (!cl_vwep->value || i > MAX_CLIENTWEAPONMODELS - 1)
					i = 0;

				ent.model = ci->weaponmodel[i];

				if (!ent.model) {
					if (i != 0)
						ent.model = ci->weaponmodel[0];

					if (!ent.model)
						ent.model = cl.baseclientinfo.weaponmodel[0];
				}

			} else
				ent.model = cl.model_draw[s1->modelindex2];

			/* check for the defender sphere shell and make it translucent */
			if (!Q_strcasecmp (cl.configstrings[CS_MODELS+(s1->modelindex2)], "models/items/shell/tris.md2")) {
				ent.alpha = 0.32f;
				ent.flags = RF_TRANSLUCENT;
			}

			V_AddEntity (&ent);

			ent.flags = 0;
			ent.alpha = 0;
		}

		if (s1->modelindex3) {
			ent.model = cl.model_draw[s1->modelindex3];
			V_AddEntity (&ent);
		}

		if (s1->modelindex4) {
			ent.model = cl.model_draw[s1->modelindex4];
			V_AddEntity (&ent);
		}

		if ( effects & EF_POWERSCREEN ) {
			ent.model = cl_mod_powerscreen;
			ent.oldframe = 0;
			ent.frame = 0;
			ent.flags |= (RF_TRANSLUCENT | RF_SHELL_GREEN);
			ent.alpha = 0.30f;
			V_AddEntity (&ent);
		}

		/* add automatic particle trails */
		if ( (effects&~EF_ROTATE) ) {
			if (effects & EF_ROCKET) {
				CL_RocketTrail (cent->lerp_origin, ent.origin, cent);
				V_AddLight (ent.origin, 200, 1, 0.25f, 0);
			}

			/* Do not reorder EF_BLASTER and EF_HYPERBLASTER.
			 * EF_BLASTER | EF_TRACKER is a special case for
			 * EF_BLASTER2 */
			else if (effects & EF_BLASTER) {
				if (effects & EF_TRACKER) {
					CL_BlasterTrail2 (cent->lerp_origin, ent.origin);
					V_AddLight (ent.origin, 200, 0, 1, 0);

				} else {
					CL_BlasterTrail (cent->lerp_origin, ent.origin);
					V_AddLight (ent.origin, 200, 1, 1, 0);
				}

			} else if (effects & EF_HYPERBLASTER) {
				if (effects & EF_TRACKER)
					V_AddLight (ent.origin, 200, 0, 1, 0);

				else
					V_AddLight (ent.origin, 200, 1, 1, 0);

			} else if (effects & EF_GIB) {
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);

			} else if (effects & EF_GRENADE) {
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);

			} else if (effects & EF_FLIES) {
				CL_FlyEffect (cent, ent.origin);

			} else if (effects & EF_BFG) {
				static int bfg_lightramp[6] = {300, 400, 600, 300, 150, 75};

				if (effects & EF_ANIM_ALLFAST) {
					CL_BfgParticles (&ent);
					i = 200;

				} else {
					i = bfg_lightramp[s1->frame];
				}

				V_AddLight (ent.origin, i, 0, 1, 0);

			} else if (effects & EF_TRAP) {
				ent.origin[2] += 32;
				CL_TrapParticles (&ent);
				i = (rand()%100) + 100;
				V_AddLight (ent.origin, i, 1, 0.8f, 0.1f);

			} else if (effects & EF_FLAG1) {
				CL_FlagTrail (cent->lerp_origin, ent.origin, 242);
				V_AddLight (ent.origin, 225, 1, 0.1f, 0.1f);

			} else if (effects & EF_FLAG2) {
				CL_FlagTrail (cent->lerp_origin, ent.origin, 115);
				V_AddLight (ent.origin, 225, 0.1f, 0.1f, 1);

			} else if (effects & EF_TAGTRAIL) {
				CL_TagTrail (cent->lerp_origin, ent.origin, 220);
				V_AddLight (ent.origin, 225, 1.0, 1.0, 0.0);

			} else if (effects & EF_TRACKERTRAIL) {
				if (effects & EF_TRACKER) {
					float intensity;

					intensity = 50 + (500 * ((float)sin(cl.time/500.0f) + 1.0f));

					if(vidref_val == VIDREF_GL)
						V_AddLight (ent.origin, intensity, -1.0, -1.0, -1.0);

					else
						V_AddLight (ent.origin, -1.0f * intensity, 1.0f, 1.0f, 1.0f);

				} else {
					CL_Tracker_Shell (cent->lerp_origin);
					V_AddLight (ent.origin, 155, -1.0, -1.0, -1.0);
				}

			} else if (effects & EF_TRACKER) {
				CL_TrackerTrail (cent->lerp_origin, ent.origin, 0);

				if(vidref_val == VIDREF_GL)
					V_AddLight (ent.origin, 200, -1, -1, -1);

				else
					V_AddLight (ent.origin, -200, 1, 1, 1);

			} else if (effects & EF_IONRIPPER) {
				CL_IonripperTrail (cent->lerp_origin, ent.origin);
				V_AddLight (ent.origin, 100, 1, 0.5, 0.5);

			} else if (effects & EF_BLUEHYPERBLASTER) {
				V_AddLight (ent.origin, 200, 0, 0, 1);

			} else if (effects & EF_PLASMA) {
				if (effects & EF_ANIM_ALLFAST) {
					CL_BlasterTrail (cent->lerp_origin, ent.origin);
				}

				V_AddLight (ent.origin, 130, 1, 0.5, 0.5);
			}
		}

		VectorCopy (ent.origin, cent->lerp_origin);
	}
}

void CL_AddViewWeapon (player_state_t *ps, player_state_t *ops) {
	entity_t	gun = {0}; /* view model */
	int			i;

	/* allow the gun to be completely removed */
	if (!cl_gun->value)
		return;

	/* don't draw gun if in wide angle view and drawing not forced */
	if (ps->fov > 90)
		if (cl_gun->value < 2)
			return;

	if (gun_model)
		gun.model = gun_model;

	else
		gun.model = cl.model_draw[ps->gunindex];

	if (!gun.model)
		return;

	/* set up gun position */
	for (i=0 ; i<3 ; i++) {
		gun.origin[i] = cl.refdef.vieworg[i] + ops->gunoffset[i]
		                + cl.lerpfrac * (ps->gunoffset[i] - ops->gunoffset[i]);
		gun.angles[i] = cl.refdef.viewangles[i] + LerpAngle (ops->gunangles[i],
		                ps->gunangles[i], cl.lerpfrac);
	}

	if (gun_frame) {
		gun.frame = gun_frame;
		gun.oldframe = gun_frame;

	} else {
		gun.frame = ps->gunframe;

		if (gun.frame == 0)
			gun.oldframe = 0; /* just changed weapons, don't lerp from old */

		else
			gun.oldframe = ops->gunframe;
	}

	gun.flags = RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL;
	gun.backlerp = 1.0f - cl.lerpfrac;
	VectorCopy (gun.origin, gun.oldorigin);	/* don't lerp at all */
	V_AddEntity (&gun);
}

/*
 * Sets cl.refdef view values
 */
void CL_CalcViewValues (void) {
	int			i;
	float		lerp, backlerp;
	centity_t	*ent;
	frame_t		*oldframe;
	player_state_t	*ps, *ops;

	/* find the previous frame to interpolate from */
	ps = &cl.frame.playerstate;
	i = (cl.frame.serverframe - 1) & UPDATE_MASK;
	oldframe = &cl.frames[i];

	if (oldframe->serverframe != cl.frame.serverframe-1 || !oldframe->valid)
		oldframe = &cl.frame; /* previous frame was dropped or invalid */

	ops = &oldframe->playerstate;

	/* see if the player entity was teleported this frame */
	if ( fabs(ops->pmove.origin[0] - ps->pmove.origin[0]) > 256*8
	        || abs(ops->pmove.origin[1] - ps->pmove.origin[1]) > 256*8
	        || abs(ops->pmove.origin[2] - ps->pmove.origin[2]) > 256*8)
		ops = ps;  /* don't interpolate */

	ent = &cl_entities[cl.playernum+1];
	lerp = cl.lerpfrac;

	/* calculate the origin */
	if ((cl_predict->value) && !(cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION)) {
		/* use predicted values */
		unsigned	delta;

		backlerp = 1.0f - lerp;

		for (i=0 ; i<3 ; i++) {
			cl.refdef.vieworg[i] = cl.predicted_origin[i] + ops->viewoffset[i]
			                       + cl.lerpfrac * (ps->viewoffset[i] - ops->viewoffset[i])
			                       - backlerp * cl.prediction_error[i];
		}

		/* smooth out stair climbing */
		delta = cls.realtime - cl.predicted_step_time;

		if (delta < 100)
			cl.refdef.vieworg[2] -= cl.predicted_step * (100 - delta) * 0.01;

	} else {
		/* just use interpolated values */
		for (i=0 ; i<3 ; i++)
			cl.refdef.vieworg[i] = ops->pmove.origin[i]*0.125 + ops->viewoffset[i]
			                       + lerp * (ps->pmove.origin[i]*0.125 + ps->viewoffset[i]
			                                 - (ops->pmove.origin[i]*0.125 + ops->viewoffset[i]) );
	}

	/* if not running a demo or on a locked frame, add the local angle movement */
	if ( cl.frame.playerstate.pmove.pm_type < PM_DEAD ) {
		/* use predicted values */
		for (i=0 ; i<3 ; i++)
			cl.refdef.viewangles[i] = cl.predicted_angles[i];

	} else {
		/* just use interpolated values */
		for (i=0 ; i<3 ; i++)
			cl.refdef.viewangles[i] = LerpAngle (ops->viewangles[i], ps->viewangles[i], lerp);
	}

	for (i=0 ; i<3 ; i++)
		cl.refdef.viewangles[i] += LerpAngle (ops->kick_angles[i], ps->kick_angles[i], lerp);

	AngleVectors (cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up);

	/* interpolate field of view */
	cl.refdef.fov_x = ops->fov + lerp * (ps->fov - ops->fov);

	/* don't interpolate blend color */
	for (i=0 ; i<4 ; i++)
		cl.refdef.blend[i] = ps->blend[i];

	/* add the weapon */
	CL_AddViewWeapon (ps, ops);
}

/*
 * Emits all entities, particles, and lights to the refresh
 */
void CL_AddEntities (void) {
	if (cls.state != ca_active)
		return;

	if (cl.time > cl.frame.servertime) {
		if (cl_showclamp->value)
			Com_Printf ("high clamp %i\n", cl.time - cl.frame.servertime);

		cl.time = cl.frame.servertime;
		cl.lerpfrac = 1.0;

	} else if (cl.time < cl.frame.servertime - 100) {
		if (cl_showclamp->value)
			Com_Printf ("low clamp %i\n", cl.frame.servertime-100 - cl.time);

		cl.time = cl.frame.servertime - 100;
		cl.lerpfrac = 0;

	} else
		cl.lerpfrac = 1.0 - (cl.frame.servertime - cl.time) * 0.01f;

	if (cl_timedemo->value)
		cl.lerpfrac = 1.0;

	CL_CalcViewValues ();
	CL_AddPacketEntities (&cl.frame);
	CL_AddTEnts ();
	CL_AddParticles ();
	CL_AddDLights ();
	CL_AddLightStyles ();
}

/*
 * Called to get the sound spatialization origin
 */
void CL_GetEntitySoundOrigin (int ent, vec3_t org) {
	centity_t	*old;

	if (ent < 0 || ent >= MAX_EDICTS)
		Com_Error (ERR_DROP, "CL_GetEntitySoundOrigin: bad ent");

	old = &cl_entities[ent];
	VectorCopy (old->lerp_origin, org);
}

