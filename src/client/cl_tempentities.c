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
 * This file implements all temporary (dynamic created) entities
 *
 * =======================================================================
 */

#include "header/client.h"
#include "sound/header/local.h"

typedef enum
{
	ex_free, ex_explosion, ex_misc, ex_flash, ex_mflash, ex_poly, ex_poly2
} exptype_t;

typedef struct
{
	exptype_t type;
	entity_t ent;

	int frames;
	float light;
	vec3_t lightcolor;
	float start;
	int baseframe;
} explosion_t;

#define MAX_EXPLOSIONS 64
#define MAX_BEAMS 64
#define MAX_LASERS 64

static explosion_t cl_explosions[MAX_EXPLOSIONS];

typedef struct
{
	int entity;
	int dest_entity;
	struct model_s *model;
	int endtime;
	vec3_t offset;
	vec3_t start, end;
} beam_t;

static beam_t cl_beams[MAX_BEAMS];
static beam_t cl_heatbeams[MAX_BEAMS];

typedef struct
{
	entity_t ent;
	int endtime;
} laser_t;

static laser_t cl_lasers[MAX_LASERS];

static cl_sustain_t cl_sustains[MAX_SUSTAINS];

extern void CL_TeleportParticles(vec3_t org);
void CL_BlasterParticles(vec3_t org, vec3_t dir);
void CL_BFGExplosionParticles(vec3_t org);
void CL_BlueBlasterParticles(vec3_t org, vec3_t dir);

void CL_ExplosionParticles(vec3_t org);
void CL_Explosion_Particle(vec3_t org, float size,
		qboolean large, qboolean rocket);

extern cvar_t *hand;

#define EXPLOSION_PARTICLES(x) CL_ExplosionParticles((x));
#define NUM_FOOTSTEP_SFX 4

/* sounds */
static struct sfx_s *cl_sfx_ric1;
static struct sfx_s *cl_sfx_ric2;
static struct sfx_s *cl_sfx_ric3;
static struct sfx_s *cl_sfx_lashit;
static struct sfx_s *cl_sfx_spark5;
static struct sfx_s *cl_sfx_spark6;
static struct sfx_s *cl_sfx_spark7;
static struct sfx_s *cl_sfx_railg;
static struct sfx_s *cl_sfx_rockexp;
static struct sfx_s *cl_sfx_grenexp;
static struct sfx_s *cl_sfx_watrexp;
static struct sfx_s *cl_sfx_footsteps[NUM_FOOTSTEP_SFX];

static struct sfx_s *cl_sfx_lightning;
static struct sfx_s *cl_sfx_disrexp;

/* models */
static struct model_s *cl_mod_explode;
static struct model_s *cl_mod_smoke;
static struct model_s *cl_mod_flash;
static struct model_s *cl_mod_parasite_segment;
static struct model_s *cl_mod_grapple_cable;
static struct model_s *cl_mod_parasite_tip;
static struct model_s *cl_mod_explo4;
static struct model_s *cl_mod_bfg_explo;
static struct model_s *cl_mod_powerscreen;
static struct model_s *cl_mod_plasmaexplo;

static struct model_s *cl_mod_lightning;
static struct model_s *cl_mod_heatbeam;
static struct model_s *cl_mod_explo4_big;

/*
 * Utility functions
 */
static beam_t *
CL_Beams_NextFree(const beam_t *list)
{
	const beam_t *b;

	for (b = list; b < &list[MAX_BEAMS]; b++)
	{
		if (!b->model || (b->endtime < cl.time))
			return (beam_t *)b;
	}

	return NULL;
}

static beam_t *
CL_Beams_SameEnt(const beam_t *list, int src, int dest)
{
	const beam_t *b;

	for (b = list; b < &list[MAX_BEAMS]; b++)
	{
		if ((src < 0 || b->entity == src) &&
			(dest < 0 || b->dest_entity == dest))
		{
			return (beam_t *)b;
		}
	}

	return NULL;
}

static void
CL_Beams_Set(beam_t *b,
	int src, int dest, struct model_s *model,
	const vec3_t start, const vec3_t end, const vec3_t ofs,
	int tm)
{
	b->entity = src;
	b->dest_entity = dest;
	b->model = model;

	VectorCopy(start, b->start);
	VectorCopy(end, b->end);

	if (ofs)
	{
		VectorCopy(ofs, b->offset);
	}
	else
	{
		VectorClear(b->offset);
	}

	b->endtime = cl.time + tm;
}

static cl_sustain_t *
CL_NextFreeSustain(void)
{
	cl_sustain_t *s;

	for (s = cl_sustains; s < &cl_sustains[MAX_SUSTAINS]; s++)
	{
		if (s->id == 0)
		{
			return s;
		}
	}

	return NULL;
}

static explosion_t *
CL_AllocExplosion(void)
{
	int i;
	float time;
	int index;

	for (i = 0; i < MAX_EXPLOSIONS; i++)
	{
		if (cl_explosions[i].type == ex_free)
		{
			memset(&cl_explosions[i], 0, sizeof(cl_explosions[i]));
			return &cl_explosions[i];
		}
	}

	/* find the oldest explosion */
	time = (float)cl.time;
	index = 0;

	for (i = 0; i < MAX_EXPLOSIONS; i++)
	{
		if (cl_explosions[i].start < time)
		{
			time = cl_explosions[i].start;
			index = i;
		}
	}

	memset(&cl_explosions[index], 0, sizeof(cl_explosions[index]));
	return &cl_explosions[index];
}

/*
 * Resource registration
 */
void
CL_RegisterTEntSounds(void)
{
	int i;
	char name[MAX_QPATH];

	cl_sfx_ric1 = S_RegisterSound("world/ric1.wav");
	cl_sfx_ric2 = S_RegisterSound("world/ric2.wav");
	cl_sfx_ric3 = S_RegisterSound("world/ric3.wav");
	cl_sfx_lashit = S_RegisterSound("weapons/lashit.wav");
	cl_sfx_spark5 = S_RegisterSound("world/spark5.wav");
	cl_sfx_spark6 = S_RegisterSound("world/spark6.wav");
	cl_sfx_spark7 = S_RegisterSound("world/spark7.wav");
	cl_sfx_railg = S_RegisterSound("weapons/railgf1a.wav");
	cl_sfx_rockexp = S_RegisterSound("weapons/rocklx1a.wav");
	cl_sfx_grenexp = S_RegisterSound("weapons/grenlx1a.wav");
	cl_sfx_watrexp = S_RegisterSound("weapons/xpld_wat.wav");
	S_RegisterSound("player/land1.wav");

	S_RegisterSound("player/fall2.wav");
	S_RegisterSound("player/fall1.wav");

	for (i = 0; i < NUM_FOOTSTEP_SFX; i++)
	{
		Com_sprintf(name, sizeof(name), "player/step%i.wav", i + 1);
		cl_sfx_footsteps[i] = S_RegisterSound(name);
	}

	cl_sfx_lightning = S_RegisterSound("weapons/tesla.wav");
	cl_sfx_disrexp = S_RegisterSound("weapons/disrupthit.wav");
}

void
CL_RegisterTEntModels(void)
{
	cl_mod_explode = R_RegisterModel("models/objects/explode/tris.md2");
	cl_mod_smoke = R_RegisterModel("models/objects/smoke/tris.md2");
	cl_mod_flash = R_RegisterModel("models/objects/flash/tris.md2");
	cl_mod_parasite_segment = R_RegisterModel("models/monsters/parasite/segment/tris.md2");
	cl_mod_grapple_cable = R_RegisterModel("models/ctf/segment/tris.md2");
	cl_mod_parasite_tip = R_RegisterModel("models/monsters/parasite/tip/tris.md2");
	cl_mod_explo4 = R_RegisterModel("models/objects/r_explode/tris.md2");
	cl_mod_bfg_explo = R_RegisterModel("sprites/s_bfg2.sp2");
	cl_mod_powerscreen = R_RegisterModel("models/items/armor/effect/tris.md2");

	R_RegisterModel("models/objects/laser/tris.md2");
	R_RegisterModel("models/objects/grenade2/tris.md2");
	R_RegisterModel("models/weapons/v_machn/tris.md2");
	R_RegisterModel("models/weapons/v_handgr/tris.md2");
	R_RegisterModel("models/weapons/v_shotg2/tris.md2");
	R_RegisterModel("models/objects/gibs/bone/tris.md2");
	R_RegisterModel("models/objects/gibs/sm_meat/tris.md2");
	R_RegisterModel("models/objects/gibs/bone2/tris.md2");

	Draw_FindPic("w_machinegun");
	Draw_FindPic("a_bullets");
	Draw_FindPic("i_health");
	Draw_FindPic("a_grenades");

	cl_mod_explo4_big = R_RegisterModel("models/objects/r_explode2/tris.md2");
	cl_mod_lightning = R_RegisterModel("models/proj/lightning/tris.md2");
	cl_mod_heatbeam = R_RegisterModel("models/proj/beam/tris.md2");
}

/*
 * Clear temp entity state
 */
static void
CL_ClearTEntSoundVars(void)
{
	int i;

	cl_sfx_ric1 = NULL;
	cl_sfx_ric2 = NULL;
	cl_sfx_ric3 = NULL;
	cl_sfx_lashit = NULL;
	cl_sfx_spark5 = NULL;
	cl_sfx_spark6 = NULL;
	cl_sfx_spark7 = NULL;
	cl_sfx_railg = NULL;
	cl_sfx_rockexp = NULL;
	cl_sfx_grenexp = NULL;
	cl_sfx_watrexp = NULL;

	for (i = 0; i < NUM_FOOTSTEP_SFX; i++)
	{
		cl_sfx_footsteps[i] = NULL;
	}

	cl_sfx_lightning = NULL;
	cl_sfx_disrexp = NULL;
}

static void
CL_ClearTEntModelVars(void)
{
	cl_mod_explode = NULL;
	cl_mod_smoke = NULL;
	cl_mod_flash = NULL;
	cl_mod_parasite_segment = NULL;
	cl_mod_grapple_cable = NULL;
	cl_mod_parasite_tip = NULL;
	cl_mod_explo4 = NULL;
	cl_mod_bfg_explo = NULL;
	cl_mod_powerscreen = NULL;
	cl_mod_plasmaexplo = NULL;

	cl_mod_lightning = NULL;
	cl_mod_heatbeam = NULL;
	cl_mod_explo4_big = NULL;
}

void
CL_ClearTEnts(void)
{
	memset(cl_beams, 0, sizeof(cl_beams));
	memset(cl_explosions, 0, sizeof(cl_explosions));
	memset(cl_lasers, 0, sizeof(cl_lasers));

	memset(cl_heatbeams, 0, sizeof(cl_heatbeams));
	memset(cl_sustains, 0, sizeof(cl_sustains));

	CL_ClearTEntModelVars();
	CL_ClearTEntSoundVars();
}

void
CL_ClearTEntModels(void)
{
	memset(cl_explosions, 0, sizeof(cl_explosions));

	memset(cl_beams, 0, sizeof(cl_beams));
	memset(cl_heatbeams, 0, sizeof(cl_heatbeams));

	CL_ClearTEntModelVars();
}

/*
 * Parse temp ent messages
 */
void
CL_SmokeAndFlash(vec3_t origin)
{
	explosion_t *ex;

	ex = CL_AllocExplosion();
	VectorCopy(origin, ex->ent.origin);
	ex->type = ex_misc;
	ex->frames = 4;
	ex->ent.flags = RF_TRANSLUCENT;
	ex->start = cl.frame.servertime - 100.0f;
	ex->ent.model = cl_mod_smoke;

	ex = CL_AllocExplosion();
	VectorCopy(origin, ex->ent.origin);
	ex->type = ex_flash;
	ex->ent.flags = RF_FULLBRIGHT;
	ex->frames = 2;
	ex->start = cl.frame.servertime - 100.0f;
	ex->ent.model = cl_mod_flash;
}

void
CL_ParseParticles(void)
{
	int color, count;
	vec3_t pos, dir;

	MSG_ReadPos(&net_message, pos);
	MSG_ReadDir(&net_message, dir);

	color = MSG_ReadByte(&net_message);

	count = MSG_ReadByte(&net_message);

	CL_ParticleEffect(pos, dir, color, count);
}

static void
CL_ParseBeam(struct model_s *model, qboolean with_offset)
{
	int ent;
	vec3_t start, end, ofs;
	beam_t *b;

	ent = MSG_ReadShort(&net_message);

	MSG_ReadPos(&net_message, start);
	MSG_ReadPos(&net_message, end);

	if (with_offset)
	{
		MSG_ReadPos(&net_message, ofs);
	}
	else
	{
		VectorClear(ofs);
	}

	if (!model)
	{
		return;
	}

	/* override any beam with the same entity */
	b = CL_Beams_SameEnt(cl_beams, ent, -1);

	if (!b)
	{
		b = CL_Beams_NextFree(cl_beams);

		if (!b)
		{
			Com_Printf("beam list overflow!\n");
			return;
		}
	}

	CL_Beams_Set(b, ent, 0, model,
		start, end, ofs, 200);
}

/*
 * adds to the cl_playerbeam array instead of the cl_beams array
 */
static void
CL_ParseHeatBeam(qboolean is_monster)
{
	static const vec3_t plofs = {2, 7, -3};
	const vec_t *ofs;
	int ent;
	vec3_t start, end;
	beam_t *b;
	int tm;

	ent = MSG_ReadShort(&net_message);

	MSG_ReadPos(&net_message, start);
	MSG_ReadPos(&net_message, end);

	if (!cl_mod_heatbeam)
	{
		return;
	}

	ofs = (!is_monster) ?
		plofs : vec3_origin;

	tm = 200;

	/* Override any beam with the same entity
	   For player beams, we only want one per
	   player (entity) so... */
	b = CL_Beams_SameEnt(cl_heatbeams, ent, -1);

	if (!b)
	{
		b = CL_Beams_NextFree(cl_heatbeams);

		if (!b)
		{
			Com_Printf("beam list overflow!\n");
			return;
		}

		/* this needs to be 100 to
		   prevent multiple heatbeams
		*/
		tm = 100;
	}

	CL_Beams_Set(b, ent, 0, cl_mod_heatbeam,
		start, end, ofs, tm);
}

static unsigned
CL_ParseLightning(struct model_s *model)
{
	int srcEnt, destEnt;
	vec3_t start, end;
	beam_t *b;

	srcEnt = MSG_ReadShort(&net_message);
	if (srcEnt < 0)
	{
		Com_Error(ERR_DROP, "%s: unexpected message end", __func__);
		return 0;
	}

	destEnt = MSG_ReadShort(&net_message);
	if (destEnt < 0)
	{
		Com_Error(ERR_DROP, "%s: unexpected message end", __func__);
		return 0;
	}

	MSG_ReadPos(&net_message, start);
	MSG_ReadPos(&net_message, end);

	if (!model)
	{
		return srcEnt;
	}

	/* override any beam with the same
	   source AND destination entities */
	b = CL_Beams_SameEnt(cl_beams, srcEnt, destEnt);

	if (!b)
	{
		b = CL_Beams_NextFree(cl_beams);

		if (!b)
		{
			Com_Printf("beam list overflow!\n");
			return srcEnt;
		}
	}

	CL_Beams_Set(b, srcEnt, destEnt, model,
		start, end, NULL, 200);

	return srcEnt;
}

static void
CL_ParseLaser(int colors)
{
	vec3_t start;
	vec3_t end;
	laser_t *l;
	int i;

	MSG_ReadPos(&net_message, start);
	MSG_ReadPos(&net_message, end);

	for (i = 0, l = cl_lasers; i < MAX_LASERS; i++, l++)
	{
		if (l->endtime < cl.time)
		{
			float alpha = cl_laseralpha->value;
			if (alpha < 0.0f)
			{
				alpha = 0.0f;
			}
			else if (alpha > 1.0f)
			{
				alpha = 1.0f;
			}

			l->ent.flags = RF_TRANSLUCENT | RF_BEAM;
			VectorCopy(start, l->ent.origin);
			VectorCopy(end, l->ent.oldorigin);
			l->ent.alpha = alpha;
			l->ent.skinnum = (colors >> ((randk() % 4) * 8)) & 0xff;
			l->ent.model = NULL;
			l->ent.frame = 4;
			l->endtime = cl.time + 100;
			return;
		}
	}
}

static void
CL_ParseSteam(void)
{
	vec3_t pos, dir;
	int id;
	int r;
	int cnt;
	int color;
	int magnitude;
	cl_sustain_t *s, dummy;

	id = MSG_ReadShort(&net_message); /* an id of -1 is an instant effect */

	if (id == -1) /* instant */
	{
		/* instant */
		cnt = MSG_ReadByte(&net_message);
		MSG_ReadPos(&net_message, pos);
		MSG_ReadDir(&net_message, dir);
		r = MSG_ReadByte(&net_message);
		magnitude = MSG_ReadShort(&net_message);
		color = r & 0xff;

		CL_ParticleSteamEffect(pos, dir, color, cnt, magnitude);

		return;
	}

	s = CL_NextFreeSustain();

	if (!s)
	{
		s = &dummy;
	}

	s->id = id;
	s->count = MSG_ReadByte(&net_message);
	MSG_ReadPos(&net_message, s->org);
	MSG_ReadDir(&net_message, s->dir);
	r = MSG_ReadByte(&net_message);
	s->color = r & 0xff;
	s->magnitude = MSG_ReadShort(&net_message);
	s->endtime = cl.time + MSG_ReadLong(&net_message);
	s->think = CL_ParticleSteamEffect2;
	s->thinkinterval = 100;
	s->nextthink = cl.time;
}

static void
CL_ParseWidow(void)
{
	int id;
	cl_sustain_t *s, dummy;

	id = MSG_ReadShort(&net_message);

	s = CL_NextFreeSustain();

	if (!s)
	{
		s = &dummy;
	}

	s->id = id;
	MSG_ReadPos(&net_message, s->org);
	s->endtime = cl.time + 2100;
	s->think = CL_Widowbeamout;
	s->thinkinterval = 1;
	s->nextthink = cl.time;
}

static void
CL_ParseNuke(void)
{
	cl_sustain_t *s, dummy;

	s = CL_NextFreeSustain();

	if (!s)
	{
		s = &dummy;
	}

	s->id = 21000;
	MSG_ReadPos(&net_message, s->org);
	s->endtime = cl.time + 1000;
	s->think = CL_Nukeblast;
	s->thinkinterval = 1;
	s->nextthink = cl.time;
}

static byte splash_color[] = {0x00, 0xe0, 0xb0, 0x50, 0xd0, 0xe0, 0xe8};

void
CL_ParseTEnt(void)
{
	int type;
	vec3_t pos, pos2, dir;
	explosion_t *ex;
	int cnt;
	int color;
	int r;
	int ent;
	int magnitude;

	type = MSG_ReadByte(&net_message);

	switch (type)
	{
		case TE_BLOOD: /* bullet hitting flesh */
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			CL_ParticleEffect(pos, dir, 0xe8, 60);
			break;

		case TE_GUNSHOT: /* bullet hitting wall */
		case TE_SPARKS:
		case TE_BULLET_SPARKS:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);

			if (type == TE_GUNSHOT)
			{
				CL_ParticleEffect(pos, dir, 0, 40);
			}
			else
			{
				CL_ParticleEffect(pos, dir, 0xe0, 6);
			}

			if (type != TE_SPARKS)
			{
				CL_SmokeAndFlash(pos);
				/* impact sound */
				cnt = randk() & 15;

				if (cnt == 1)
				{
					S_StartSound(pos, 0, 0, cl_sfx_ric1, 1, ATTN_NORM, 0);
				}
				else if (cnt == 2)
				{
					S_StartSound(pos, 0, 0, cl_sfx_ric2, 1, ATTN_NORM, 0);
				}
				else if (cnt == 3)
				{
					S_StartSound(pos, 0, 0, cl_sfx_ric3, 1, ATTN_NORM, 0);
				}
			}

			break;

		case TE_SCREEN_SPARKS:
		case TE_SHIELD_SPARKS:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);

			if (type == TE_SCREEN_SPARKS)
			{
				CL_ParticleEffect(pos, dir, 0xd0, 40);
			}

			else
			{
				CL_ParticleEffect(pos, dir, 0xb0, 40);
			}

			if (cl_limitsparksounds->value)
			{
				num_power_sounds++;

				/* If too many of these sounds are started in one frame
				 * (for example if the player shoots with the super
				 * shotgun into the power screen of a Brain) things get
				 * too loud and OpenAL is forced to scale the volume of
				 * several other sounds and the background music down.
				 * That leads to a noticable and annoying drop in the
				 * overall volume.
				 *
				 * Work around that by limiting the number of sounds
				 * started.
				 * 16 was choosen by empirical testing.
				 *
				 * This was fixed in openal-soft 0.19.0. We're keeping
				 * the work around hidden behind a cvar and no longer
				 * limited to OpenAL because a) some Linux distros may
				 * still ship older openal-soft versions and b) some
				 * player may like the changed behavior.
				 */
				if (num_power_sounds < 16)
				{
					S_StartSound(pos, 0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
				}
			}
			else
			{
				S_StartSound(pos, 0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
			}

			break;

		case TE_SHOTGUN: /* bullet hitting wall */
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			CL_ParticleEffect(pos, dir, 0, 20);
			CL_SmokeAndFlash(pos);
			break;

		case TE_SPLASH: /* bullet hitting water */
			cnt = MSG_ReadByte(&net_message);
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			r = MSG_ReadByte(&net_message);

			if (r > 6 || r < 0)
			{
				color = 0x00;
			}
			else
			{
				color = splash_color[r];
			}

			CL_ParticleEffect(pos, dir, color, cnt);

			if (r == SPLASH_SPARKS)
			{
				r = randk() & 3;

				if (r == 0)
				{
					S_StartSound(pos, 0, 0, cl_sfx_spark5, 1, ATTN_STATIC, 0);
				}
				else if (r == 1)
				{
					S_StartSound(pos, 0, 0, cl_sfx_spark6, 1, ATTN_STATIC, 0);
				}
				else
				{
					S_StartSound(pos, 0, 0, cl_sfx_spark7, 1, ATTN_STATIC, 0);
				}
			}

			break;

		case TE_LASER_SPARKS:
			cnt = MSG_ReadByte(&net_message);
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			color = MSG_ReadByte(&net_message);
			CL_ParticleEffect2(pos, dir, color, cnt);
			break;

		case TE_BLUEHYPERBLASTER:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadPos(&net_message, dir);
			CL_BlasterParticles(pos, dir);
			break;

		case TE_BLASTER: /* blaster hitting wall */
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			CL_BlasterParticles(pos, dir);

			ex = CL_AllocExplosion();
			VectorCopy(pos, ex->ent.origin);
			ex->ent.angles[0] = (float)acos(dir[2]) / M_PI * 180;

			if (dir[0])
			{
				ex->ent.angles[1] = (float)atan2(dir[1], dir[0]) / M_PI * 180;
			}

			else if (dir[1] > 0)
			{
				ex->ent.angles[1] = 90;
			}
			else if (dir[1] < 0)
			{
				ex->ent.angles[1] = 270;
			}
			else
			{
				ex->ent.angles[1] = 0;
			}

			ex->type = ex_misc;
			ex->ent.flags = 0;
			ex->start = cl.frame.servertime - 100.0f;
			ex->light = 150;
			ex->lightcolor[0] = 1;
			ex->lightcolor[1] = 1;
			ex->ent.model = cl_mod_explode;
			ex->frames = 4;
			S_StartSound(pos, 0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
			break;

		case TE_RAILTRAIL: /* railgun effect */
			MSG_ReadPos(&net_message, pos);
			MSG_ReadPos(&net_message, pos2);
			CL_RailTrail(pos, pos2);
			S_StartSound(pos2, 0, 0, cl_sfx_railg, 1, ATTN_NORM, 0);
			break;

		case TE_EXPLOSION2:
		case TE_GRENADE_EXPLOSION:
		case TE_GRENADE_EXPLOSION_WATER:
			MSG_ReadPos(&net_message, pos);
			ex = CL_AllocExplosion();
			VectorCopy(pos, ex->ent.origin);
			ex->type = ex_poly;
			ex->ent.flags = RF_FULLBRIGHT | RF_NOSHADOW;
			ex->start = cl.frame.servertime - 100.0f;
			ex->light = 350;
			ex->lightcolor[0] = 1.0;
			ex->lightcolor[1] = 0.5;
			ex->lightcolor[2] = 0.5;
			ex->ent.model = cl_mod_explo4;
			ex->frames = 19;
			ex->baseframe = 30;
			ex->ent.angles[1] = (float)(randk() % 360);
			EXPLOSION_PARTICLES(pos);

			if (type == TE_GRENADE_EXPLOSION_WATER)
			{
				S_StartSound(pos, 0, 0, cl_sfx_watrexp, 1, ATTN_NORM, 0);
			}
			else
			{
				S_StartSound(pos, 0, 0, cl_sfx_grenexp, 1, ATTN_NORM, 0);
			}

			break;

		case TE_PLASMA_EXPLOSION:
			MSG_ReadPos(&net_message, pos);
			ex = CL_AllocExplosion();
			VectorCopy(pos, ex->ent.origin);
			ex->type = ex_poly;
			ex->ent.flags = RF_FULLBRIGHT | RF_NOSHADOW;
			ex->start = cl.frame.servertime - 100.0f;
			ex->light = 350;
			ex->lightcolor[0] = 1.0;
			ex->lightcolor[1] = 0.5;
			ex->lightcolor[2] = 0.5;
			ex->ent.angles[1] = (float)(randk() % 360);
			ex->ent.model = cl_mod_explo4;

			if (frandk() < 0.5)
			{
				ex->baseframe = 15;
			}

			ex->frames = 15;
			EXPLOSION_PARTICLES(pos);
			S_StartSound(pos, 0, 0, cl_sfx_rockexp, 1, ATTN_NORM, 0);
			break;

		case TE_EXPLOSION1_BIG:
		case TE_EXPLOSION1_NP:
		case TE_EXPLOSION1:
		case TE_ROCKET_EXPLOSION:
		case TE_ROCKET_EXPLOSION_WATER:
			MSG_ReadPos(&net_message, pos);
			ex = CL_AllocExplosion();
			VectorCopy(pos, ex->ent.origin);
			ex->type = ex_poly;
			ex->ent.flags = RF_FULLBRIGHT | RF_NOSHADOW;
			ex->start = cl.frame.servertime - 100.0f;
			ex->light = 350;
			ex->lightcolor[0] = 1.0;
			ex->lightcolor[1] = 0.5;
			ex->lightcolor[2] = 0.5;
			ex->ent.angles[1] = (float)(randk() % 360);

			if (type != TE_EXPLOSION1_BIG)
			{
				ex->ent.model = cl_mod_explo4;
			}
			else
			{
				ex->ent.model = cl_mod_explo4_big;
			}

			if (frandk() < 0.5)
			{
				ex->baseframe = 15;
			}

			ex->frames = 15;

			if ((type != TE_EXPLOSION1_BIG) && (type != TE_EXPLOSION1_NP))
			{
				EXPLOSION_PARTICLES(pos);
			}

			if (type == TE_ROCKET_EXPLOSION_WATER)
			{
				S_StartSound(pos, 0, 0, cl_sfx_watrexp, 1, ATTN_NORM, 0);
			}
			else
			{
				S_StartSound(pos, 0, 0, cl_sfx_rockexp, 1, ATTN_NORM, 0);
			}

			break;

		case TE_BFG_EXPLOSION:
			MSG_ReadPos(&net_message, pos);
			ex = CL_AllocExplosion();
			VectorCopy(pos, ex->ent.origin);
			ex->type = ex_poly;
			ex->ent.flags = RF_FULLBRIGHT | RF_NOSHADOW;
			ex->start = cl.frame.servertime - 100.0f;
			ex->light = 350;
			ex->lightcolor[0] = 0.0;
			ex->lightcolor[1] = 1.0;
			ex->lightcolor[2] = 0.0;
			ex->ent.model = cl_mod_bfg_explo;
			ex->ent.flags |= RF_TRANSLUCENT;
			ex->ent.alpha = 0.30f;
			ex->frames = 4;
			break;

		case TE_BFG_BIGEXPLOSION:
			MSG_ReadPos(&net_message, pos);
			CL_BFGExplosionParticles(pos);
			break;

		case TE_BFG_LASER:
			CL_ParseLaser(0xd0d1d2d3);
			break;

		case TE_BUBBLETRAIL:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadPos(&net_message, pos2);
			CL_BubbleTrail(pos, pos2);
			break;

		case TE_PARASITE_ATTACK:
		case TE_MEDIC_CABLE_ATTACK:
			CL_ParseBeam(cl_mod_parasite_segment, false);
			break;

		case TE_BOSSTPORT: /* boss teleporting to station */
			MSG_ReadPos(&net_message, pos);
			CL_BigTeleportParticles(pos);
			S_StartSound(pos, 0, 0, S_RegisterSound(
						"misc/bigtele.wav"), 1, ATTN_NONE, 0);
			break;

		case TE_GRAPPLE_CABLE:
			CL_ParseBeam(cl_mod_grapple_cable, true);
			break;

		case TE_WELDING_SPARKS:
			cnt = MSG_ReadByte(&net_message);
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			color = MSG_ReadByte(&net_message);
			CL_ParticleEffect2(pos, dir, color, cnt);

			ex = CL_AllocExplosion();
			VectorCopy(pos, ex->ent.origin);
			ex->type = ex_flash;
			ex->ent.flags = RF_BEAM;
			ex->start = cl.frame.servertime - 0.1f;
			ex->light = 100 + (float)(randk() % 75);
			ex->lightcolor[0] = 1.0f;
			ex->lightcolor[1] = 1.0f;
			ex->lightcolor[2] = 0.3f;
			ex->ent.model = cl_mod_flash;
			ex->frames = 2;
			break;

		case TE_GREENBLOOD:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			CL_ParticleEffect2(pos, dir, 0xdf, 30);
			break;

		case TE_TUNNEL_SPARKS:
			cnt = MSG_ReadByte(&net_message);
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			color = MSG_ReadByte(&net_message);
			CL_ParticleEffect3(pos, dir, color, cnt);
			break;

		case TE_BLASTER2:
		case TE_FLECHETTE:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);

			if (type == TE_BLASTER2)
			{
				CL_BlasterParticles2(pos, dir, 0xd0);
			}
			else
			{
				CL_BlasterParticles2(pos, dir, 0x6f);
			}

			ex = CL_AllocExplosion();
			VectorCopy(pos, ex->ent.origin);
			ex->ent.angles[0] = (float)acos(dir[2]) / M_PI * 180;

			if (dir[0])
			{
				ex->ent.angles[1] = (float)atan2(dir[1], dir[0]) / M_PI * 180;
			}
			else if (dir[1] > 0)
			{
				ex->ent.angles[1] = 90;
			}

			else if (dir[1] < 0)
			{
				ex->ent.angles[1] = 270;
			}
			else
			{
				ex->ent.angles[1] = 0;
			}

			ex->type = ex_misc;
			ex->ent.flags = RF_FULLBRIGHT | RF_TRANSLUCENT;

			if (type == TE_BLASTER2)
			{
				ex->ent.skinnum = 1;
			}
			else /* flechette */
			{
				ex->ent.skinnum = 2;
			}

			ex->start = cl.frame.servertime - 100.0f;
			ex->light = 150;

			if (type == TE_BLASTER2)
			{
				ex->lightcolor[1] = 1;
			}
			else
			{
				/* flechette */
				ex->lightcolor[0] = 0.19f;
				ex->lightcolor[1] = 0.41f;
				ex->lightcolor[2] = 0.75f;
			}

			ex->ent.model = cl_mod_explode;
			ex->frames = 4;
			S_StartSound(pos, 0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
			break;

		case TE_LIGHTNING:
			ent = CL_ParseLightning(cl_mod_lightning);
			S_StartSound(NULL, ent, CHAN_WEAPON, cl_sfx_lightning,
				1, ATTN_NORM, 0);
			break;

		case TE_DEBUGTRAIL:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadPos(&net_message, pos2);
			CL_DebugTrail(pos, pos2);
			break;

		case TE_PLAIN_EXPLOSION:
			MSG_ReadPos(&net_message, pos);

			ex = CL_AllocExplosion();
			VectorCopy(pos, ex->ent.origin);
			ex->type = ex_poly;
			ex->ent.flags = RF_FULLBRIGHT | RF_NOSHADOW;
			ex->start = cl.frame.servertime - 100.0f;
			ex->light = 350;
			ex->lightcolor[0] = 1.0;
			ex->lightcolor[1] = 0.5;
			ex->lightcolor[2] = 0.5;
			ex->ent.angles[1] = randk() % 360;
			ex->ent.model = cl_mod_explo4;

			if (frandk() < 0.5)
			{
				ex->baseframe = 15;
			}

			ex->frames = 15;

			S_StartSound(pos, 0, 0, cl_sfx_rockexp, 1, ATTN_NORM, 0);

			break;

		case TE_FLASHLIGHT:
			MSG_ReadPos(&net_message, pos);
			ent = MSG_ReadShort(&net_message);
			CL_Flashlight(ent, pos);
			break;

		case TE_FORCEWALL:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadPos(&net_message, pos2);
			color = MSG_ReadByte(&net_message);
			CL_ForceWall(pos, pos2, color);
			break;

		case TE_HEATBEAM:
			CL_ParseHeatBeam(false);
			break;

		case TE_MONSTER_HEATBEAM:
			CL_ParseHeatBeam(true);
			break;

		case TE_HEATBEAM_SPARKS:
			cnt = 50;
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			r = 8;
			magnitude = 60;
			color = r & 0xff;
			CL_ParticleSteamEffect(pos, dir, color, cnt, magnitude);
			S_StartSound(pos, 0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
			break;

		case TE_HEATBEAM_STEAM:
			cnt = 20;
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			color = 0xe0;
			magnitude = 60;
			CL_ParticleSteamEffect(pos, dir, color, cnt, magnitude);
			S_StartSound(pos, 0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
			break;

		case TE_STEAM:
			CL_ParseSteam();
			break;

		case TE_BUBBLETRAIL2:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadPos(&net_message, pos2);
			CL_BubbleTrail2(pos, pos2, 8);
			S_StartSound(pos, 0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
			break;

		case TE_MOREBLOOD:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			CL_ParticleEffect(pos, dir, 0xe8, 250);
			break;

		case TE_CHAINFIST_SMOKE:
			dir[0] = 0;
			dir[1] = 0;
			dir[2] = 1;
			MSG_ReadPos(&net_message, pos);
			CL_ParticleSmokeEffect(pos, dir, 0, 20, 20);
			break;

		case TE_ELECTRIC_SPARKS:
			MSG_ReadPos(&net_message, pos);
			MSG_ReadDir(&net_message, dir);
			CL_ParticleEffect(pos, dir, 0x75, 40);
			S_StartSound(pos, 0, 0, cl_sfx_lashit, 1, ATTN_NORM, 0);
			break;

		case TE_TRACKER_EXPLOSION:
			MSG_ReadPos(&net_message, pos);
			CL_ColorFlash(pos, 0, 150, -1, -1, -1);
			CL_ColorExplosionParticles(pos, 0, 1);
			S_StartSound(pos, 0, 0, cl_sfx_disrexp, 1, ATTN_NORM, 0);
			break;

		case TE_TELEPORT_EFFECT:
		case TE_DBALL_GOAL:
			MSG_ReadPos(&net_message, pos);
			CL_TeleportParticles(pos);
			break;

		case TE_WIDOWBEAMOUT:
			CL_ParseWidow();
			break;

		case TE_NUKEBLAST:
			CL_ParseNuke();
			break;

		case TE_WIDOWSPLASH:
			MSG_ReadPos(&net_message, pos);
			CL_WidowSplash(pos);
			break;

		default:
			Com_Error(ERR_DROP, "CL_ParseTEnt: bad type");
	}
}

/*
 * Add temp entities to rendering list
 */
static void
CalculatePitchYaw(const vec3_t dir, float *pitch, float *yaw)
{
	float forward;
	float p, y;

	if ((dir[1] == 0) && (dir[0] == 0))
	{
		y = 0;

		if (dir[2] > 0)
		{
			p = 90;
		}
		else
		{
			p = 270;
		}
	}
	else
	{
		if (dir[0])
		{
			y = ((float)atan2(dir[1], dir[0]) * 180 / M_PI);
		}
		else if (dir[1] > 0)
		{
			y = 90;
		}
		else
		{
			y = 270;
		}

		if (y < 0)
		{
			y += 360;
		}

		forward = sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
		p = ((float)atan2(dir[2], forward) * -180.0 / M_PI);

		if (p < 0)
		{
			p += 360;
		}
	}

	*pitch = p;
	*yaw = y;
}

static void
CL_AddBeams(void)
{
	const beam_t *b;
	vec3_t dist, org;
	float d;
	entity_t ent;
	float yaw, pitch;
	float len, steps;
	float model_length;

	for (b = cl_beams; b < &cl_beams[MAX_BEAMS]; b++)
	{
		if (!b->model || (b->endtime < cl.time))
		{
			continue;
		}

		/* if coming from the player, update the start position */
		if (b->entity == cl.playernum + 1) /* entity 0 is the world */
		{
			VectorCopy(cl.refdef.vieworg, org);
			org[2] -= 22; /* adjust for view height */

			VectorAdd(org, b->offset, org);
		}
		else
		{
			VectorAdd(b->start, b->offset, org);
		}

		/* calculate pitch and yaw */
		VectorSubtract(b->end, org, dist);

		CalculatePitchYaw(dist, &pitch, &yaw);

		/* add new entities for the beams */
		d = VectorNormalize(dist);

		if (b->model == cl_mod_lightning)
		{
			model_length = 35.0;
			d -= 20.0; /* correction so it doesn't end in middle of tesla */
		}
		else
		{
			model_length = 30.0;
		}

		steps = (float)ceil(d / model_length);
		len = (d - model_length) / (steps - 1);

		memset(&ent, 0, sizeof(ent));

		/* special case for lightning model .. if the real length
		   is shorter than the model, flip it around & draw it
		   from the end to the start. This prevents the model from
		   going through the tesla mine (instead it goes through
		   the target) */
		if ((b->model == cl_mod_lightning) && (d <= model_length))
		{
			VectorCopy(b->end, ent.origin);
			ent.model = b->model;
			ent.flags = RF_FULLBRIGHT;

			ent.angles[0] = pitch;
			ent.angles[1] = yaw;
			ent.angles[2] = (float)(randk() % 360);

			V_AddEntity(&ent);

			continue;
		}

		while (d > 0)
		{
			VectorCopy(org, ent.origin);
			ent.model = b->model;

			if (b->model == cl_mod_lightning)
			{
				ent.angles[0] = -pitch;
				ent.angles[1] = yaw + 180.0f;
			}
			else
			{
				ent.angles[0] = pitch;
				ent.angles[1] = yaw;
			}

			ent.angles[2] = (float)(randk() % 360);

			V_AddEntity(&ent);

			VectorMA(org, len, dist, org);
			d -= model_length;
		}
	}
}

static float
HandMul(void)
{
	if (!hand)
	{
		return 1.0f;
	}

	switch ((int)hand->value)
	{
		case 1:
			return -1.0f;
		case 2:
			return 0.0f;
		default:
			return 1.0f;
	}
}

static void
ApplyBeamOffset(const vec3_t ofs, float hand_mul, vec3_t out)
{
	VectorMA(out, hand_mul * ofs[0], cl.v_right, out);
	VectorMA(out, ofs[1], cl.v_forward, out);
	VectorMA(out, ofs[2], cl.v_up, out);

	if (hand_mul == 0.0f)
	{
		VectorMA(out, -1, cl.v_up, out);
	}
}

static void
AdjustToWeapon(const vec3_t ofs, float hand_mul, vec3_t out)
{
	int i;
	frame_t *oldframe;
	player_state_t *ps, *ops;

	/* set up gun position */
	ps = &cl.frame.playerstate;
	i = (cl.frame.serverframe - 1) & UPDATE_MASK;
	oldframe = &cl.frames[i];

	if ((oldframe->serverframe != cl.frame.serverframe - 1) || !oldframe->valid)
	{
		oldframe = &cl.frame; /* previous frame was dropped or invalid */
	}

	ops = &oldframe->playerstate;

	for (i = 0; i < 3; i++)
	{
		out[i] = cl.refdef.vieworg[i] + ops->gunoffset[i]
				  + cl.lerpfrac * (ps->gunoffset[i] - ops->gunoffset[i]);
	}

	ApplyBeamOffset(ofs, hand_mul, out);
}

static void
CL_AddHeatBeams(void)
{
	const beam_t *b;
	vec3_t dist, org;
	float d;
	entity_t ent;
	float yaw, pitch;
	float len, steps;
	int framenum;
	float model_length;
	int by_us;
	float hand_mul;

	hand_mul = HandMul();

	for (b = cl_heatbeams; b < &cl_heatbeams[MAX_BEAMS]; b++)
	{
		if (!b->model || (b->endtime < cl.time))
		{
			continue;
		}

		by_us = (b->entity == cl.playernum + 1);

		if (by_us)
		{
			AdjustToWeapon(b->offset, hand_mul, org);
		}
		else
		{
			VectorCopy(b->start, org);
		}

		/* calculate pitch and yaw */
		VectorSubtract(b->end, org, dist);

		if (by_us)
		{
			len = VectorLength(dist);
			VectorScale(cl.v_forward, len, dist);

			ApplyBeamOffset(b->offset, hand_mul, dist);
		}

		CalculatePitchYaw(dist, &pitch, &yaw);

		if (by_us)
		{
			framenum = 1;

			/* add the rings */
			CL_Heatbeam(org, dist);
		}
		else
		{
			framenum = 2;

			CL_MonsterPlasma_Shell(org);
		}

		/* hack for left-handed weapon
		   RF_WEAPONMODEL will mirror the beam start pos
		   so put the start pos to the right
		   that way the renderer mirrors it to the left
		*/
		if (by_us && hand_mul == -1.0f)
		{
			AdjustToWeapon(b->offset, 1.0f, org);
			VectorSubtract(b->end, org, dist);

			len = VectorLength(dist);
			VectorScale(cl.v_forward, len, dist);
			ApplyBeamOffset(b->offset, 1.0f, dist);

			CalculatePitchYaw(dist, &pitch, &yaw);
		}

		/* add new entities for the beams */
		d = VectorNormalize(dist);

		model_length = 32.0;
		steps = ceil(d / model_length);
		len = (d - model_length) / (steps - 1);

		memset(&ent, 0, sizeof(ent));

		ent.model = b->model;
		ent.flags = RF_FULLBRIGHT;

		// DG: fix rogue heatbeam high FOV rendering
		if (by_us && hand_mul != 0.0f)
		{
			ent.flags |= RF_WEAPONMODEL;
		}

		ent.angles[0] = -pitch;
		ent.angles[1] = yaw + 180.0f;
		ent.angles[2] = (float)((cl.time) % 360);
		ent.frame = framenum;

		while (d > 0)
		{
			VectorCopy(org, ent.origin);

			V_AddEntity(&ent);

			VectorMA(org, len, dist, org);
			d -= model_length;
		}
	}
}

static void
CL_AddExplosions(void)
{
	entity_t *ent;
	int i;
	explosion_t *ex;
	float frac;
	int f;

	memset(&ent, 0, sizeof(ent));

	for (i = 0, ex = cl_explosions; i < MAX_EXPLOSIONS; i++, ex++)
	{
		if (ex->type == ex_free)
		{
			continue;
		}

		frac = (cl.time - ex->start) / 100.0;
		f = (int)floor(frac);

		ent = &ex->ent;

		switch (ex->type)
		{
			case ex_mflash:

				if (f >= ex->frames - 1)
				{
					ex->type = ex_free;
				}

				break;
			case ex_misc:

				if (f >= ex->frames - 1)
				{
					ex->type = ex_free;
					break;
				}

				ent->alpha = 1.0f - frac / (ex->frames - 1);
				break;
			case ex_flash:

				if (f >= 1)
				{
					ex->type = ex_free;
					break;
				}

				ent->alpha = 1.0;
				break;
			case ex_poly:

				if (f >= ex->frames - 1)
				{
					ex->type = ex_free;
					break;
				}

				ent->alpha = (16.0f - (float)f) / 16.0f;

				if (f < 10)
				{
					ent->skinnum = (f >> 1);

					if (ent->skinnum < 0)
					{
						ent->skinnum = 0;
					}
				}
				else
				{
					ent->flags |= RF_TRANSLUCENT;

					if (f < 13)
					{
						ent->skinnum = 5;
					}

					else
					{
						ent->skinnum = 6;
					}
				}

				break;
			case ex_poly2:

				if (f >= ex->frames - 1)
				{
					ex->type = ex_free;
					break;
				}

				ent->alpha = (5.0 - (float)f) / 5.0;
				ent->skinnum = 0;
				ent->flags |= RF_TRANSLUCENT;
				break;
			default:
				break;
		}

		if (ex->type == ex_free)
		{
			continue;
		}

		if (ex->light)
		{
			V_AddLight(ent->origin, ex->light * ent->alpha,
					ex->lightcolor[0], ex->lightcolor[1], ex->lightcolor[2]);
		}

		VectorCopy(ent->origin, ent->oldorigin);

		if (f < 0)
		{
			f = 0;
		}

		ent->frame = ex->baseframe + f + 1;
		ent->oldframe = ex->baseframe + f;
		ent->backlerp = 1.0f - cl.lerpfrac;

		V_AddEntity(ent);
	}
}

static void
CL_AddLasers(void)
{
	laser_t *l;
	int i;

	for (i = 0, l = cl_lasers; i < MAX_LASERS; i++, l++)
	{
		if (l->endtime >= cl.time)
		{
			V_AddEntity(&l->ent);
		}
	}
}

void
CL_ProcessSustain()
{
	cl_sustain_t *s;
	int i;

	for (i = 0, s = cl_sustains; i < MAX_SUSTAINS; i++, s++)
	{
		if (s->id)
		{
			if ((s->endtime >= cl.time) && (cl.time >= s->nextthink))
			{
				s->think(s);
			}
			else if (s->endtime < cl.time)
			{
				s->id = 0;
			}
		}
	}
}

void
CL_AddTEnts(void)
{
	CL_AddBeams();
	CL_AddHeatBeams();
	CL_AddExplosions();
	CL_AddLasers();
	CL_ProcessSustain();
}

struct sfx_s *
CL_RandomFootstepSfx(void)
{
	return cl_sfx_footsteps[randk() % NUM_FOOTSTEP_SFX];
}

struct model_s *
CL_PowerScreenModel(void)
{
	return cl_mod_powerscreen;
}

