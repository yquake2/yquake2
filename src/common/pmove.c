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
 * Player movement code. This is the core of Quake IIs legendary physics
 * engine
 *
 * =======================================================================
 */

#include "header/common.h"
#include "../client/sound/header/local.h"
#include "../client/header/client.h"

#define STEPSIZE 18

/* all of the locals will be zeroed before each
 * pmove, just to make damn sure we don't have
 * any differences when running on client or server */

typedef struct
{
	vec3_t origin; /* full float precision */
	vec3_t velocity; /* full float precision */

	vec3_t forward, right, up;
	float frametime;

	csurface_t *groundsurface;
	cplane_t groundplane;
	int groundcontents;

	vec3_t previous_origin;
	qboolean ladder;
} pml_t;

pmove_t *pm;
pml_t pml;

/* movement parameters */
float pm_stopspeed = 100;
float pm_maxspeed = 300;
float pm_duckspeed = 100;
float pm_accelerate = 10;
float pm_airaccelerate = 0;
float pm_wateraccelerate = 10;
float pm_friction = 6;
float pm_waterfriction = 1;
float pm_waterspeed = 400;

#define STOP_EPSILON 0.1 /* Slide off of the impacting object returns the blocked flags (1 = floor, 2 = step / wall) */
#define MIN_STEP_NORMAL 0.7 /* can't step up onto very steep slopes */
#define MAX_CLIP_PLANES 5

static void
PM_ClipVelocity(vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	float backoff;
	float change;
	int i;

	backoff = DotProduct(in, normal) * overbounce;

	for (i = 0; i < 3; i++)
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;

		if ((out[i] > -STOP_EPSILON) && (out[i] < STOP_EPSILON))
		{
			out[i] = 0;
		}
	}
}

/*
 * Each intersection will try to step over the obstruction instead of
 * sliding along it.
 *
 * Returns a new origin, velocity, and contact entity
 * Does not modify any world state?
 */
static void
PM_StepSlideMove_(void)
{
	int bumpcount, numbumps;
	vec3_t dir;
	float d;
	int numplanes;
	vec3_t planes[MAX_CLIP_PLANES];
	vec3_t primal_velocity;
	int i, j;
	trace_t trace;
	vec3_t end;
	float time_left;

	numbumps = 4;

	VectorCopy(pml.velocity, primal_velocity);
	numplanes = 0;

	time_left = pml.frametime;

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		for (i = 0; i < 3; i++)
		{
			end[i] = pml.origin[i] + time_left * pml.velocity[i];
		}

		trace = pm->trace(pml.origin, pm->mins, pm->maxs, end);

		if (trace.allsolid)
		{
			/* entity is trapped in another solid */
			pml.velocity[2] = 0; /* don't build up falling damage */
			return;
		}

		if (trace.fraction > 0)
		{
			/* actually covered some distance */
			VectorCopy(trace.endpos, pml.origin);
			numplanes = 0;
		}

		if (trace.fraction == 1)
		{
			break; /* moved the entire distance */
		}

		/* save entity for contact */
		if ((pm->numtouch < MAXTOUCH) && trace.ent)
		{
			pm->touchents[pm->numtouch] = trace.ent;
			pm->numtouch++;
		}

		time_left -= time_left * trace.fraction;

		/* slide along this plane */
		if (numplanes >= MAX_CLIP_PLANES)
		{
			/* this shouldn't really happen */
			VectorCopy(vec3_origin, pml.velocity);
			break;
		}

		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		/* modify original_velocity so it parallels all of the clip planes */
		for (i = 0; i < numplanes; i++)
		{
			PM_ClipVelocity(pml.velocity, planes[i], pml.velocity, 1.01f);

			for (j = 0; j < numplanes; j++)
			{
				if (j != i)
				{
					if (DotProduct(pml.velocity, planes[j]) < 0)
					{
						break; /* not ok */
					}
				}
			}

			if (j == numplanes)
			{
				break;
			}
		}

		if (i != numplanes)
		{
			/* go along this plane */
		}
		else
		{
			/* go along the crease */
			if (numplanes != 2)
			{
				VectorCopy(vec3_origin, pml.velocity);
				break;
			}

			CrossProduct(planes[0], planes[1], dir);
			d = DotProduct(dir, pml.velocity);
			VectorScale(dir, d, pml.velocity);
		}

		/* if velocity is against the original velocity, stop dead
		   to avoid tiny occilations in sloping corners */
		if (DotProduct(pml.velocity, primal_velocity) <= 0)
		{
			VectorCopy(vec3_origin, pml.velocity);
			break;
		}
	}

	if (pm->s.pm_time)
	{
		VectorCopy(primal_velocity, pml.velocity);
	}
}

static void
PM_StepSlideMove(void)
{
	vec3_t start_o, start_v;
	vec3_t down_o, down_v;
	trace_t trace;
	float down_dist, up_dist;
	vec3_t up, down;

	VectorCopy(pml.origin, start_o);
	VectorCopy(pml.velocity, start_v);

	PM_StepSlideMove_();

	VectorCopy(pml.origin, down_o);
	VectorCopy(pml.velocity, down_v);

	VectorCopy(start_o, up);
	up[2] += STEPSIZE;

	trace = pm->trace(up, pm->mins, pm->maxs, up);

	if (trace.allsolid)
	{
		return; /* can't step up */
	}

	/* try sliding above */
	VectorCopy(up, pml.origin);
	VectorCopy(start_v, pml.velocity);

	PM_StepSlideMove_();

	/* push down the final amount */
	VectorCopy(pml.origin, down);
	down[2] -= STEPSIZE;
	trace = pm->trace(pml.origin, pm->mins, pm->maxs, down);

	if (!trace.allsolid)
	{
		VectorCopy(trace.endpos, pml.origin);
	}

	VectorCopy(pml.origin, up);

	/* decide which one went farther */
	down_dist = (down_o[0] - start_o[0]) * (down_o[0] - start_o[0])
				+ (down_o[1] - start_o[1]) * (down_o[1] - start_o[1]);
	up_dist = (up[0] - start_o[0]) * (up[0] - start_o[0])
			  + (up[1] - start_o[1]) * (up[1] - start_o[1]);

	if ((down_dist > up_dist) || (trace.plane.normal[2] < MIN_STEP_NORMAL))
	{
		VectorCopy(down_o, pml.origin);
		VectorCopy(down_v, pml.velocity);
		return;
	}

	pml.velocity[2] = down_v[2];
}

/*
 * Handles both ground friction and water friction
 */
static void
PM_Friction(void)
{
	float *vel;
	float speed, newspeed, control;
	float friction;
	float drop;

	vel = pml.velocity;

	speed = VectorLength(vel);

	if (speed < 1)
	{
		vel[0] = 0;
		vel[1] = 0;
		return;
	}

	drop = 0;

	/* apply ground friction */
	if ((pm->groundentity && pml.groundsurface &&
		 !(pml.groundsurface->flags & SURF_SLICK)) || (pml.ladder))
	{
		friction = pm_friction;
		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control * friction * pml.frametime;
	}

	/* apply water friction */
	if (pm->waterlevel && !pml.ladder)
	{
		drop += speed * pm_waterfriction * pm->waterlevel * pml.frametime;
	}

	/* scale the velocity */
	newspeed = speed - drop;

	if (newspeed < 0)
	{
		newspeed = 0;
	}

	newspeed /= speed;

	vel[0] = vel[0] * newspeed;
	vel[1] = vel[1] * newspeed;
	vel[2] = vel[2] * newspeed;
}

/* Used for speedoomter display. */
void GetPlayerSpeed(float *speed, float *speedxy)
{
	*speedxy = sqrt(pml.velocity[0] * pml.velocity[0] + pml.velocity[1] * pml.velocity[1]);
	*speed = VectorLength(pml.velocity);
}

/*
 * Handles user intended acceleration
 */
static void
PM_Accelerate(vec3_t wishdir, float wishspeed, float accel)
{
	int i;
	float addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct(pml.velocity, wishdir);
	addspeed = wishspeed - currentspeed;

	if (addspeed <= 0)
	{
		return;
	}

	accelspeed = accel * pml.frametime * wishspeed;

	if (accelspeed > addspeed)
	{
		accelspeed = addspeed;
	}

	for (i = 0; i < 3; i++)
	{
		pml.velocity[i] += accelspeed * wishdir[i];
	}
}

static void
PM_AirAccelerate(vec3_t wishdir, float wishspeed, float accel)
{
	int i;
	float addspeed, accelspeed, currentspeed, wishspd = wishspeed;

	if (wishspd > 30)
	{
		wishspd = 30;
	}

	currentspeed = DotProduct(pml.velocity, wishdir);
	addspeed = wishspd - currentspeed;

	if (addspeed <= 0)
	{
		return;
	}

	accelspeed = accel * wishspeed * pml.frametime;

	if (accelspeed > addspeed)
	{
		accelspeed = addspeed;
	}

	for (i = 0; i < 3; i++)
	{
		pml.velocity[i] += accelspeed * wishdir[i];
	}
}

static void
PM_AddCurrents(vec3_t wishvel)
{
	/* account for ladders */
	if (pml.ladder && (fabs(pml.velocity[2]) <= 200))
	{
		if ((pm->viewangles[PITCH] <= -15) && (pm->cmd.forwardmove > 0))
		{
			wishvel[2] = 200;
		}
		else if ((pm->viewangles[PITCH] >= 15) && (pm->cmd.forwardmove > 0))
		{
			wishvel[2] = -200;
		}
		else if (pm->cmd.upmove > 0)
		{
			wishvel[2] = 200;
		}
		else if (pm->cmd.upmove < 0)
		{
			wishvel[2] = -200;
		}
		else
		{
			wishvel[2] = 0;
		}

		/* limit horizontal speed when on a ladder */
		if (wishvel[0] < -25)
		{
			wishvel[0] = -25;
		}
		else if (wishvel[0] > 25)
		{
			wishvel[0] = 25;
		}

		if (wishvel[1] < -25)
		{
			wishvel[1] = -25;
		}
		else if (wishvel[1] > 25)
		{
			wishvel[1] = 25;
		}
	}

	/* add water currents */
	if (pm->watertype & MASK_CURRENT)
	{
		vec3_t v;
		float s;

		VectorClear(v);

		if (pm->watertype & CONTENTS_CURRENT_0)
		{
			v[0] += 1;
		}

		if (pm->watertype & CONTENTS_CURRENT_90)
		{
			v[1] += 1;
		}

		if (pm->watertype & CONTENTS_CURRENT_180)
		{
			v[0] -= 1;
		}

		if (pm->watertype & CONTENTS_CURRENT_270)
		{
			v[1] -= 1;
		}

		if (pm->watertype & CONTENTS_CURRENT_UP)
		{
			v[2] += 1;
		}

		if (pm->watertype & CONTENTS_CURRENT_DOWN)
		{
			v[2] -= 1;
		}

		s = pm_waterspeed;

		if ((pm->waterlevel == 1) && (pm->groundentity))
		{
			s /= 2;
		}

		VectorMA(wishvel, s, v, wishvel);
	}

	/* add conveyor belt velocities */
	if (pm->groundentity)
	{
		vec3_t v;

		VectorClear(v);

		if (pml.groundcontents & CONTENTS_CURRENT_0)
		{
			v[0] += 1;
		}

		if (pml.groundcontents & CONTENTS_CURRENT_90)
		{
			v[1] += 1;
		}

		if (pml.groundcontents & CONTENTS_CURRENT_180)
		{
			v[0] -= 1;
		}

		if (pml.groundcontents & CONTENTS_CURRENT_270)
		{
			v[1] -= 1;
		}

		if (pml.groundcontents & CONTENTS_CURRENT_UP)
		{
			v[2] += 1;
		}

		if (pml.groundcontents & CONTENTS_CURRENT_DOWN)
		{
			v[2] -= 1;
		}

		VectorMA(wishvel, 100, v, wishvel);
	}
}

static void
PM_WaterMove(void)
{
	int i;
	vec3_t wishvel;
	float wishspeed;
	vec3_t wishdir;

	/* user intentions */
	for (i = 0; i < 3; i++)
	{
		wishvel[i] = pml.forward[i] * pm->cmd.forwardmove +
					 pml.right[i] * pm->cmd.sidemove;
	}

	if (!pm->cmd.forwardmove && !pm->cmd.sidemove && !pm->cmd.upmove)
	{
		wishvel[2] -= 60; /* drift towards bottom */
	}
	else
	{
		wishvel[2] += pm->cmd.upmove;
	}

	PM_AddCurrents(wishvel);

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	if (wishspeed > pm_maxspeed)
	{
		VectorScale(wishvel, pm_maxspeed / wishspeed, wishvel);
		wishspeed = pm_maxspeed;
	}

	wishspeed *= 0.5;

	PM_Accelerate(wishdir, wishspeed, pm_wateraccelerate);

	PM_StepSlideMove();
}

static void
PM_AirMove(void)
{
	int i;
	vec3_t wishvel;
	float fmove, smove;
	vec3_t wishdir;
	float wishspeed;
	float maxspeed;

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	for (i = 0; i < 2; i++)
	{
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	}

	wishvel[2] = 0;

	PM_AddCurrents(wishvel);

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	/* clamp to server defined max speed */
	maxspeed = (pm->s.pm_flags & PMF_DUCKED) ? pm_duckspeed : pm_maxspeed;

	if (wishspeed > maxspeed)
	{
		VectorScale(wishvel, maxspeed / wishspeed, wishvel);
		wishspeed = maxspeed;
	}

	if (pml.ladder)
	{
		PM_Accelerate(wishdir, wishspeed, pm_accelerate);

		if (!wishvel[2])
		{
			if (pml.velocity[2] > 0)
			{
				pml.velocity[2] -= pm->s.gravity * pml.frametime;

				if (pml.velocity[2] < 0)
				{
					pml.velocity[2] = 0;
				}
			}
			else
			{
				pml.velocity[2] += pm->s.gravity * pml.frametime;

				if (pml.velocity[2] > 0)
				{
					pml.velocity[2] = 0;
				}
			}
		}

		PM_StepSlideMove();
	}
	else if (pm->groundentity)
	{
		/* walking on ground */
		pml.velocity[2] = 0;
		PM_Accelerate(wishdir, wishspeed, pm_accelerate);

		if (pm->s.gravity > 0)
		{
			pml.velocity[2] = 0;
		}
		else
		{
			pml.velocity[2] -= pm->s.gravity * pml.frametime;
		}

		if (!pml.velocity[0] && !pml.velocity[1])
		{
			return;
		}

		PM_StepSlideMove();
	}
	else
	{
		/* not on ground, so little effect on velocity */
		if (pm_airaccelerate)
		{
			PM_AirAccelerate(wishdir, wishspeed, pm_accelerate);
		}
		else
		{
			PM_Accelerate(wishdir, wishspeed, 1);
		}

		/* add gravity */
		pml.velocity[2] -= pm->s.gravity * pml.frametime;
		PM_StepSlideMove();
	}
}

static void
PM_CatagorizePosition(void)
{
	vec3_t point;
	int cont;
	trace_t trace;
	float sample1;
	float sample2;

	/* if the player hull point one unit down
	   is solid, the player is on ground */

	/* see if standing on something solid */
	point[0] = pml.origin[0];
	point[1] = pml.origin[1];
	point[2] = pml.origin[2] - 0.25f;

	if (pml.velocity[2] > 180)
	{
		pm->s.pm_flags &= ~PMF_ON_GROUND;
		pm->groundentity = NULL;
	}
	else
	{
		trace = pm->trace(pml.origin, pm->mins, pm->maxs, point);
		pml.groundplane = trace.plane;
		pml.groundsurface = trace.surface;
		pml.groundcontents = trace.contents;

		if (!trace.ent || ((trace.plane.normal[2] < 0.7) && !trace.startsolid))
		{
			pm->groundentity = NULL;
			pm->s.pm_flags &= ~PMF_ON_GROUND;
		}
		else
		{
			pm->groundentity = trace.ent;

			/* hitting solid ground will end a waterjump */
			if (pm->s.pm_flags & PMF_TIME_WATERJUMP)
			{
				pm->s.pm_flags &=
					~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
				pm->s.pm_time = 0;
			}

			if (!(pm->s.pm_flags & PMF_ON_GROUND))
			{
				/* just hit the ground */
				pm->s.pm_flags |= PMF_ON_GROUND;

				/* don't do landing time if we were just going down a slope */
				if (pml.velocity[2] < -200)
				{
					pm->s.pm_flags |= PMF_TIME_LAND;

					/* don't allow another jump for a little while */
					if (pml.velocity[2] < -400)
					{
						pm->s.pm_time = 25;
					}
					else
					{
						pm->s.pm_time = 18;
					}
				}
			}
		}

		if ((pm->numtouch < MAXTOUCH) && trace.ent)
		{
			pm->touchents[pm->numtouch] = trace.ent;
			pm->numtouch++;
		}
	}

	/* get waterlevel, accounting for ducking */
	pm->waterlevel = 0;
	pm->watertype = 0;

	sample2 = pm->viewheight - pm->mins[2];
	sample1 = sample2 / 2;

	point[2] = pml.origin[2] + pm->mins[2] + 1;
	cont = pm->pointcontents(point);

	if (cont & MASK_WATER)
	{
		pm->watertype = cont;
		pm->waterlevel = 1;
		point[2] = pml.origin[2] + pm->mins[2] + sample1;
		cont = pm->pointcontents(point);

		if (cont & MASK_WATER)
		{
			pm->waterlevel = 2;
			point[2] = pml.origin[2] + pm->mins[2] + sample2;
			cont = pm->pointcontents(point);

			if (cont & MASK_WATER)
			{
				pm->waterlevel = 3;
			}
		}
	}
}

static void
PM_CheckJump(void)
{
	if (pm->s.pm_flags & PMF_TIME_LAND)
	{
		/* hasn't been long enough since landing to jump again */
		return;
	}

	if (pm->cmd.upmove < 10)
	{
		/* not holding jump */
		pm->s.pm_flags &= ~PMF_JUMP_HELD;
		return;
	}

	/* must wait for jump to be released */
	if (pm->s.pm_flags & PMF_JUMP_HELD)
	{
		return;
	}

	if (pm->s.pm_type == PM_DEAD)
	{
		return;
	}

	if (pm->waterlevel >= 2)
	{
		/* swimming, not jumping */
		pm->groundentity = NULL;

		if (pml.velocity[2] <= -300)
		{
			return;
		}

		if (pm->watertype == CONTENTS_WATER)
		{
			pml.velocity[2] = 100;
		}
		else if (pm->watertype == CONTENTS_SLIME)
		{
			pml.velocity[2] = 80;
		}
		else
		{
			pml.velocity[2] = 50;
		}

		return;
	}

	if (pm->groundentity == NULL)
	{
		return; /* in air, so no effect */
	}

	pm->s.pm_flags |= PMF_JUMP_HELD;

	pm->groundentity = NULL;
	pml.velocity[2] += 270;

	if (pml.velocity[2] < 270)
	{
		pml.velocity[2] = 270;
	}
}

static void
PM_CheckSpecialMovement(void)
{
	vec3_t spot;
	int cont;
	vec3_t flatforward;
	trace_t trace;

	if (pm->s.pm_time)
	{
		return;
	}

	pml.ladder = false;

	/* check for ladder */
	flatforward[0] = pml.forward[0];
	flatforward[1] = pml.forward[1];
	flatforward[2] = 0;
	VectorNormalize(flatforward);

	VectorMA(pml.origin, 1, flatforward, spot);
	trace = pm->trace(pml.origin, pm->mins, pm->maxs, spot);

	if ((trace.fraction < 1) && (trace.contents & CONTENTS_LADDER))
	{
		pml.ladder = true;
	}

	/* check for water jump */
	if (pm->waterlevel != 2)
	{
		return;
	}

	VectorMA(pml.origin, 30, flatforward, spot);
	spot[2] += 4;
	cont = pm->pointcontents(spot);

	if (!(cont & CONTENTS_SOLID))
	{
		return;
	}

	spot[2] += 16;
	cont = pm->pointcontents(spot);

	if (cont)
	{
		return;
	}

	/* jump out of water */
	VectorScale(flatforward, 50, pml.velocity);
	pml.velocity[2] = 350;

	pm->s.pm_flags |= PMF_TIME_WATERJUMP;
	pm->s.pm_time = 255;
}

static void
PM_FlyMove(qboolean doclip)
{
	float speed, drop, friction, control, newspeed;
	float currentspeed, addspeed, accelspeed;
	int i;
	vec3_t wishvel;
	float fmove, smove;
	vec3_t wishdir;
	float wishspeed;
	vec3_t end;
	trace_t trace;

	pm->viewheight = 22;

	/* friction */
	speed = VectorLength(pml.velocity);

	if (speed < 1)
	{
		VectorCopy(vec3_origin, pml.velocity);
	}
	else
	{
		drop = 0;

		friction = pm_friction * 1.5f; /* extra friction */
		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control * friction * pml.frametime;

		/* scale the velocity */
		newspeed = speed - drop;

		if (newspeed < 0)
		{
			newspeed = 0;
		}

		newspeed /= speed;

		VectorScale(pml.velocity, newspeed, pml.velocity);
	}

	/* accelerate */
	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	VectorNormalize(pml.forward);
	VectorNormalize(pml.right);

	for (i = 0; i < 3; i++)
	{
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	}

	wishvel[2] += pm->cmd.upmove;

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	/* clamp to server defined max speed */
	if (wishspeed > pm_maxspeed)
	{
		VectorScale(wishvel, pm_maxspeed / wishspeed, wishvel);
		wishspeed = pm_maxspeed;
	}

	currentspeed = DotProduct(pml.velocity, wishdir);
	addspeed = wishspeed - currentspeed;

	if (addspeed <= 0)
	{
		return;
	}

	accelspeed = pm_accelerate * pml.frametime * wishspeed;

	if (accelspeed > addspeed)
	{
		accelspeed = addspeed;
	}

	for (i = 0; i < 3; i++)
	{
		pml.velocity[i] += accelspeed * wishdir[i];
	}

	if (doclip)
	{
		for (i = 0; i < 3; i++)
		{
			end[i] = pml.origin[i] + pml.frametime * pml.velocity[i];
		}

		trace = pm->trace(pml.origin, pm->mins, pm->maxs, end);

		VectorCopy(trace.endpos, pml.origin);
	}
	else
	{
		/* move */
		VectorMA(pml.origin, pml.frametime, pml.velocity, pml.origin);
	}
}

/*
 * Sets mins, maxs, and pm->viewheight
 */
static void
PM_CheckDuck(void)
{
	trace_t trace;

	pm->mins[0] = -16;
	pm->mins[1] = -16;

	pm->maxs[0] = 16;
	pm->maxs[1] = 16;

	if (pm->s.pm_type == PM_GIB)
	{
		pm->mins[2] = 0;
		pm->maxs[2] = 16;
		pm->viewheight = 8;
		return;
	}

	pm->mins[2] = -24;

	if (pm->s.pm_type == PM_DEAD)
	{
		pm->s.pm_flags |= PMF_DUCKED;
	}
	else if ((pm->cmd.upmove < 0) && (pm->s.pm_flags & PMF_ON_GROUND))
	{
		/* duck */
		pm->s.pm_flags |= PMF_DUCKED;
	}
	else
	{
		/* stand up if possible */
		if (pm->s.pm_flags & PMF_DUCKED)
		{
			/* try to stand up */
			pm->maxs[2] = 32;
			trace = pm->trace(pml.origin, pm->mins, pm->maxs, pml.origin);

			if (!trace.allsolid)
			{
				pm->s.pm_flags &= ~PMF_DUCKED;
			}
		}
	}

	if (pm->s.pm_flags & PMF_DUCKED)
	{
		pm->maxs[2] = 4;
		pm->viewheight = -2;
	}
	else
	{
		pm->maxs[2] = 32;
		pm->viewheight = 22;
	}
}

static void
PM_DeadMove(void)
{
	float forward;

	if (!pm->groundentity)
	{
		return;
	}

	/* extra friction */
	forward = VectorLength(pml.velocity);
	forward -= 20;

	if (forward <= 0)
	{
		VectorClear(pml.velocity);
	}
	else
	{
		VectorNormalize(pml.velocity);
		VectorScale(pml.velocity, forward, pml.velocity);
	}
}

static qboolean
PM_GoodPosition(void)
{
	trace_t trace;
	vec3_t origin, end;
	int i;

	if (pm->s.pm_type == PM_SPECTATOR)
	{
		return true;
	}

	for (i = 0; i < 3; i++)
	{
		origin[i] = end[i] = pm->s.origin[i] * 0.125f;
	}

	trace = pm->trace(origin, pm->mins, pm->maxs, end);

	return !trace.allsolid;
}

/*
 * On exit, the origin will have a value that is pre-quantized to the 0.125
 * precision of the network channel and in a valid position.
 */
static void
PM_SnapPosition(void)
{
	int sign[3];
	int i, j, bits;
	short base[3];
	/* try all single bits first */
	static int jitterbits[8] = {0, 4, 1, 2, 3, 5, 6, 7};

	/* snap velocity to eigths */
	for (i = 0; i < 3; i++)
	{
		pm->s.velocity[i] = (int)(pml.velocity[i] * 8);
	}

	for (i = 0; i < 3; i++)
	{
		if (pml.origin[i] >= 0)
		{
			sign[i] = 1;
		}
		else
		{
			sign[i] = -1;
		}

		pm->s.origin[i] = (int)(pml.origin[i] * 8);

		if (pm->s.origin[i] * 0.125f == pml.origin[i])
		{
			sign[i] = 0;
		}
	}

	VectorCopy(pm->s.origin, base);

	/* try all combinations */
	for (j = 0; j < 8; j++)
	{
		bits = jitterbits[j];
		VectorCopy(base, pm->s.origin);

		for (i = 0; i < 3; i++)
		{
			if (bits & (1 << i))
			{
				pm->s.origin[i] += sign[i];
			}
		}

		if (PM_GoodPosition())
		{
			return;
		}
	}

	/* go back to the last position */
	VectorCopy(pml.previous_origin, pm->s.origin);
}

static void
PM_InitialSnapPosition(void)
{
	int x, y, z;
	short base[3];
	static int offset[3] = {0, -1, 1};

	VectorCopy(pm->s.origin, base);

	for (z = 0; z < 3; z++)
	{
		pm->s.origin[2] = base[2] + offset[z];

		for (y = 0; y < 3; y++)
		{
			pm->s.origin[1] = base[1] + offset[y];

			for (x = 0; x < 3; x++)
			{
				pm->s.origin[0] = base[0] + offset[x];

				if (PM_GoodPosition())
				{
					pml.origin[0] = pm->s.origin[0] * 0.125f;
					pml.origin[1] = pm->s.origin[1] * 0.125f;
					pml.origin[2] = pm->s.origin[2] * 0.125f;
					VectorCopy(pm->s.origin, pml.previous_origin);
					return;
				}
			}
		}
	}

	Com_DPrintf("Bad InitialSnapPosition\n");
}

static void
PM_ClampAngles(void)
{
	short temp;
	int i;

	if (pm->s.pm_flags & PMF_TIME_TELEPORT)
	{
		pm->viewangles[YAW] = SHORT2ANGLE(
				pm->cmd.angles[YAW] + pm->s.delta_angles[YAW]);
		pm->viewangles[PITCH] = 0;
		pm->viewangles[ROLL] = 0;
	}
	else
	{
		/* circularly clamp the angles with deltas */
		for (i = 0; i < 3; i++)
		{
			temp = pm->cmd.angles[i] + pm->s.delta_angles[i];
			pm->viewangles[i] = SHORT2ANGLE(temp);
		}

		/* don't let the player look up or down more than 90 degrees */
		if ((pm->viewangles[PITCH] > 89) && (pm->viewangles[PITCH] < 180))
		{
			pm->viewangles[PITCH] = 89;
		}
		else if ((pm->viewangles[PITCH] < 271) && (pm->viewangles[PITCH] >= 180))
		{
			pm->viewangles[PITCH] = 271;
		}
	}

	AngleVectors(pm->viewangles, pml.forward, pml.right, pml.up);
}

#if !defined(DEDICATED_ONLY)
static void
PM_CalculateViewHeightForDemo()
{
	if (pm->s.pm_type == PM_GIB)
		pm->viewheight = 8;
	else {
		if ((pm->s.pm_flags & PMF_DUCKED) != 0)
			pm->viewheight = -2;
		else
			pm->viewheight = 22;
	}
}

static void
PM_CalculateWaterLevelForDemo()
{
	vec3_t point;
	int cont;

	point[0] = pml.origin[0];
	point[1] = pml.origin[1];
	point[2] = pml.origin[2] + pm->viewheight;

	pm->waterlevel = 0;
	pm->watertype = 0;

	cont = pm->pointcontents(point);

	if ((cont & MASK_WATER) != 0) {
		pm->waterlevel = 3;
		pm->watertype = cont;
	}
}

static void
PM_UpdateUnderwaterSfx()
{
	static int underwater;

	if ((pm->waterlevel == 3) && !underwater)
	{
		underwater = 1;
		snd_is_underwater = 1;

#ifdef USE_OPENAL
		AL_Underwater();
#endif
	}

	if ((pm->waterlevel < 3) && underwater)
	{
		underwater = 0;
		snd_is_underwater = 0;

#ifdef USE_OPENAL
		AL_Overwater();
#endif
	}
}
#endif

/*
 * Can be called by either the server or the client
 */
void
Pmove(pmove_t *pmove)
{
	pm = pmove;

	/* clear results */
	pm->numtouch = 0;
	VectorClear(pm->viewangles);
	pm->viewheight = 0;
	pm->groundentity = 0;
	pm->watertype = 0;
	pm->waterlevel = 0;

	/* clear all pmove local vars */
	memset(&pml, 0, sizeof(pml));

	/* convert origin and velocity to float values */
	pml.origin[0] = pm->s.origin[0] * 0.125f;
	pml.origin[1] = pm->s.origin[1] * 0.125f;
	pml.origin[2] = pm->s.origin[2] * 0.125f;

	pml.velocity[0] = pm->s.velocity[0] * 0.125f;
	pml.velocity[1] = pm->s.velocity[1] * 0.125f;
	pml.velocity[2] = pm->s.velocity[2] * 0.125f;

	/* save old org in case we get stuck */
	VectorCopy(pm->s.origin, pml.previous_origin);

	pml.frametime = pm->cmd.msec * 0.001f;

	PM_ClampAngles();

	if (pm->s.pm_type == PM_SPECTATOR)
	{
		PM_FlyMove(false);
		PM_SnapPosition();
		return;
	}

	if (pm->s.pm_type >= PM_DEAD)
	{
		pm->cmd.forwardmove = 0;
		pm->cmd.sidemove = 0;
		pm->cmd.upmove = 0;
	}

	if (pm->s.pm_type == PM_FREEZE)
	{
#if !defined(DEDICATED_ONLY)
		if (cl.attractloop) {
			PM_CalculateViewHeightForDemo();
			PM_CalculateWaterLevelForDemo();
			PM_UpdateUnderwaterSfx();
		}
#endif

		return; /* no movement at all */
	}

	/* set mins, maxs, and viewheight */
	PM_CheckDuck();

	if (pm->snapinitial)
	{
		PM_InitialSnapPosition();
	}

	/* set groundentity, watertype, and waterlevel */
	PM_CatagorizePosition();

	if (pm->s.pm_type == PM_DEAD)
	{
		PM_DeadMove();
	}

	PM_CheckSpecialMovement();

	/* drop timing counter */
	if (pm->s.pm_time)
	{
		int msec;

		msec = pm->cmd.msec >> 3;

		if (!msec)
		{
			msec = 1;
		}

		if (msec >= pm->s.pm_time)
		{
			pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
			pm->s.pm_time = 0;
		}
		else
		{
			pm->s.pm_time -= msec;
		}
	}

	if (pm->s.pm_flags & PMF_TIME_TELEPORT)
	{
		/* teleport pause stays exactly in place */
	}
	else if (pm->s.pm_flags & PMF_TIME_WATERJUMP)
	{
		/* waterjump has no control, but falls */
		pml.velocity[2] -= pm->s.gravity * pml.frametime;

		if (pml.velocity[2] < 0)
		{
			/* cancel as soon as we are falling down again */
			pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
			pm->s.pm_time = 0;
		}

		PM_StepSlideMove();
	}
	else
	{
		PM_CheckJump();

		PM_Friction();

		if (pm->waterlevel >= 2)
		{
			PM_WaterMove();
		}
		else
		{
			vec3_t angles;

			VectorCopy(pm->viewangles, angles);

			if (angles[PITCH] > 180)
			{
				angles[PITCH] = angles[PITCH] - 360;
			}

			angles[PITCH] /= 3;

			AngleVectors(angles, pml.forward, pml.right, pml.up);

			PM_AirMove();
		}
	}

	/* set groundentity, watertype, and waterlevel for final spot */
	PM_CatagorizePosition();

#if !defined(DEDICATED_ONLY)
	PM_UpdateUnderwaterSfx();
#endif

	PM_SnapPosition();
}
