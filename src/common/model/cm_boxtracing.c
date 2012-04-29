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
 * This file implements tracing of collision boxes through the world
 * model
 *
 * =======================================================================
 */

#include "../header/common.h"
#include "../header/cmodel.h"

extern int	box_headnode;

/* 1/32 epsilon to keep floating point happy */
#define	DIST_EPSILON	(0.03125f)

vec3_t	trace_start, trace_end;
vec3_t	trace_mins, trace_maxs;
vec3_t	trace_extents;

trace_t	trace_trace;
int		trace_contents;
qboolean	trace_ispoint; /* optimized case */

void CM_ClipBoxToBrush (vec3_t mins, vec3_t maxs, vec3_t p1, vec3_t p2,
                        trace_t *trace, cbrush_t *brush)
{
	int			i, j;
	cplane_t	*plane, *clipplane;
	float		dist;
	float		enterfrac, leavefrac;
	vec3_t		ofs;
	float		d1, d2;
	qboolean	getout, startout;
	float		f;
	cbrushside_t	*side, *leadside;

	enterfrac = -1;
	leavefrac = 1;
	clipplane = NULL;

	if (!brush->numsides)
		return;

#ifndef DEDICATED_ONLY
	c_brush_traces++;
#endif

	getout = false;
	startout = false;
	leadside = NULL;

	for (i=0 ; i<brush->numsides ; i++)
	{
		side = &map_brushsides[brush->firstbrushside+i];
		plane = side->plane;

		if (!trace_ispoint)
		{
			/* general box case
			   push the plane out
			   apropriately for mins/maxs */
			for (j=0 ; j<3 ; j++)
			{
				if (plane->normal[j] < 0)
					ofs[j] = maxs[j];

				else
					ofs[j] = mins[j];
			}

			dist = DotProduct (ofs, plane->normal);
			dist = plane->dist - dist;
		}

		else
		{
			/* special point case */
			dist = plane->dist;
		}

		d1 = DotProduct (p1, plane->normal) - dist;
		d2 = DotProduct (p2, plane->normal) - dist;

		if (d2 > 0)
			getout = true; /* endpoint is not in solid */

		if (d1 > 0)
			startout = true;

		/* if completely in front of face, no intersection */
		if (d1 > 0 && d2 >= d1)
			return;

		if (d1 <= 0 && d2 <= 0)
			continue;

		/* crosses face */
		if (d1 > d2)
		{
			/* enter */
			f = (d1-DIST_EPSILON) / (d1-d2);

			if (f > enterfrac)
			{
				enterfrac = f;
				clipplane = plane;
				leadside = side;
			}
		}

		else
		{
			/* leave */
			f = (d1+DIST_EPSILON) / (d1-d2);

			if (f < leavefrac)
				leavefrac = f;
		}
	}

	if (!startout)
	{
		/* original point was inside brush */
		trace->startsolid = true;

		if (!getout)
			trace->allsolid = true;

		return;
	}

	if (enterfrac < leavefrac)
	{
		if (enterfrac > -1 && enterfrac < trace->fraction)
		{
			if (enterfrac < 0)
				enterfrac = 0;

			trace->fraction = enterfrac;
			trace->plane = *clipplane;
			trace->surface = &(leadside->surface->c);
			trace->contents = brush->contents;
		}
	}
}

void CM_TestBoxInBrush (vec3_t mins, vec3_t maxs, vec3_t p1,
                        trace_t *trace, cbrush_t *brush)
{
	int			i, j;
	cplane_t	*plane;
	float		dist;
	vec3_t		ofs;
	float		d1;
	cbrushside_t	*side;

	if (!brush->numsides)
		return;

	for (i=0 ; i<brush->numsides ; i++)
	{
		side = &map_brushsides[brush->firstbrushside+i];
		plane = side->plane;

		/* general box case
		   push the plane out
		   apropriately for mins/maxs */
		for (j=0 ; j<3 ; j++)
		{
			if (plane->normal[j] < 0)
				ofs[j] = maxs[j];

			else
				ofs[j] = mins[j];
		}

		dist = DotProduct (ofs, plane->normal);
		dist = plane->dist - dist;

		d1 = DotProduct (p1, plane->normal) - dist;

		/* if completely in front of face, no intersection */
		if (d1 > 0)
			return;

	}

	/* inside this brush */
	trace->startsolid = trace->allsolid = true;
	trace->fraction = 0;
	trace->contents = brush->contents;
}

void CM_TraceToLeaf (int leafnum)
{
	int			k;
	int			brushnum;
	cleaf_t		*leaf;
	cbrush_t	*b;

	leaf = &map_leafs[leafnum];

	if ( !(leaf->contents & trace_contents))
		return;

	/* trace line against all brushes in the leaf */
	for (k=0 ; k<leaf->numleafbrushes ; k++)
	{
		brushnum = map_leafbrushes[leaf->firstleafbrush+k];
		b = &map_brushes[brushnum];

		if (b->checkcount == checkcount)
			continue; /* already checked this brush in another leaf */

		b->checkcount = checkcount;

		if ( !(b->contents & trace_contents))
			continue;

		CM_ClipBoxToBrush (trace_mins, trace_maxs, trace_start, trace_end, &trace_trace, b);

		if (!trace_trace.fraction)
			return;
	}

}

void CM_TestInLeaf (int leafnum)
{
	int			k;
	int			brushnum;
	cleaf_t		*leaf;
	cbrush_t	*b;

	leaf = &map_leafs[leafnum];

	if ( !(leaf->contents & trace_contents))
		return;

	/* trace line against all brushes in the leaf */
	for (k=0 ; k<leaf->numleafbrushes ; k++)
	{
		brushnum = map_leafbrushes[leaf->firstleafbrush+k];
		b = &map_brushes[brushnum];

		if (b->checkcount == checkcount)
			continue; /* already checked this brush in another leaf */

		b->checkcount = checkcount;

		if ( !(b->contents & trace_contents))
			continue;

		CM_TestBoxInBrush (trace_mins, trace_maxs, trace_start, &trace_trace, b);

		if (!trace_trace.fraction)
			return;
	}

}

void CM_RecursiveHullCheck (int num, float p1f, float p2f, vec3_t p1, vec3_t p2)
{
	cnode_t		*node;
	cplane_t	*plane;
	float		t1, t2, offset;
	float		frac, frac2;
	float		idist;
	int			i;
	vec3_t		mid;
	int			side;
	float		midf;

	if (trace_trace.fraction <= p1f)
		return; /* already hit something nearer */

	/* if < 0, we are in a leaf node */
	if (num < 0)
	{
		CM_TraceToLeaf (-1-num);
		return;
	}

	/* find the point distances to the seperating plane
	   and the offset for the size of the box */
	node = map_nodes + num;
	plane = node->plane;

	if (plane->type < 3)
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = trace_extents[plane->type];
	}

	else
	{
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;

		if (trace_ispoint)
			offset = 0;

		else
			offset = (float)fabs(trace_extents[0]*plane->normal[0]) +
			         (float)fabs(trace_extents[1]*plane->normal[1]) +
			         (float)fabs(trace_extents[2]*plane->normal[2]);
	}

	/* see which sides we need to consider */
	if (t1 >= offset && t2 >= offset)
	{
		CM_RecursiveHullCheck (node->children[0], p1f, p2f, p1, p2);
		return;
	}

	if (t1 < -offset && t2 < -offset)
	{
		CM_RecursiveHullCheck (node->children[1], p1f, p2f, p1, p2);
		return;
	}

	/* put the crosspoint DIST_EPSILON pixels on the near side */
	if (t1 < t2)
	{
		idist = 1.0f/(t1-t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON)*idist;
		frac = (t1 - offset + DIST_EPSILON)*idist;
	}

	else if (t1 > t2)
	{
		idist = 1.0/(t1-t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON)*idist;
		frac = (t1 + offset + DIST_EPSILON)*idist;
	}

	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	/* move up to the node */
	if (frac < 0)
		frac = 0;

	if (frac > 1)
		frac = 1;

	midf = p1f + (p2f - p1f)*frac;

	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac*(p2[i] - p1[i]);

	CM_RecursiveHullCheck (node->children[side], p1f, midf, p1, mid);


	/* go past the node */
	if (frac2 < 0)
		frac2 = 0;

	if (frac2 > 1)
		frac2 = 1;

	midf = p1f + (p2f - p1f)*frac2;

	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac2*(p2[i] - p1[i]);

	CM_RecursiveHullCheck (node->children[side^1], midf, p2f, mid, p2);
}

trace_t		CM_BoxTrace (vec3_t start, vec3_t end,
                         vec3_t mins, vec3_t maxs,
                         int headnode, int brushmask)
{
	int		i;

	checkcount++; /* for multi-check avoidance */

#ifndef DEDICATED_ONLY
	c_traces++; /* for statistics, may be zeroed */
#endif

	/* fill in a default trace */
	memset (&trace_trace, 0, sizeof(trace_trace));
	trace_trace.fraction = 1;
	trace_trace.surface = &(nullsurface.c);

	if (!numnodes)	/* map not loaded */
		return trace_trace;

	trace_contents = brushmask;
	VectorCopy (start, trace_start);
	VectorCopy (end, trace_end);
	VectorCopy (mins, trace_mins);
	VectorCopy (maxs, trace_maxs);

	/* check for position test special case */
	if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2])
	{
		int		leafs[1024];
		int		i, numleafs;
		vec3_t	c1, c2;
		int		topnode;

		VectorAdd (start, mins, c1);
		VectorAdd (start, maxs, c2);

		for (i=0 ; i<3 ; i++)
		{
			c1[i] -= 1;
			c2[i] += 1;
		}

		numleafs = CM_BoxLeafnums_headnode (c1, c2, leafs, 1024, headnode, &topnode);

		for (i=0 ; i<numleafs ; i++)
		{
			CM_TestInLeaf (leafs[i]);

			if (trace_trace.allsolid)
				break;
		}

		VectorCopy (start, trace_trace.endpos);
		return trace_trace;
	}

	/* check for point special case */
	if (mins[0] == 0 && mins[1] == 0 && mins[2] == 0
	        && maxs[0] == 0 && maxs[1] == 0 && maxs[2] == 0)
	{
		trace_ispoint = true;
		VectorClear (trace_extents);
	}

	else
	{
		trace_ispoint = false;
		trace_extents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		trace_extents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		trace_extents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	/* general sweeping through world */
	CM_RecursiveHullCheck (headnode, 0, 1, start, end);

	if (trace_trace.fraction == 1)
	{
		VectorCopy (end, trace_trace.endpos);
	}

	else
	{
		for (i=0 ; i<3 ; i++)
			trace_trace.endpos[i] = start[i] + trace_trace.fraction * (end[i] - start[i]);
	}

	return trace_trace;
}

/*
 * Handles offseting and rotation of the end points for moving and
 * rotating entities
 */
trace_t CM_TransformedBoxTrace (vec3_t start, vec3_t end,
                                    vec3_t mins, vec3_t maxs,
                                    int headnode, int brushmask,
                                    vec3_t origin, vec3_t angles)
{
	trace_t		trace;
	vec3_t		start_l, end_l;
	vec3_t		a;
	vec3_t		forward, right, up;
	vec3_t		temp;
	qboolean	rotated;

	/* subtract origin offset */
	VectorSubtract (start, origin, start_l);
	VectorSubtract (end, origin, end_l);

	/* rotate start and end into the models frame of reference */
	if (headnode != box_headnode &&
	        (angles[0] || angles[1] || angles[2]) )
		rotated = true;

	else
		rotated = false;

	if (rotated)
	{
		AngleVectors (angles, forward, right, up);

		VectorCopy (start_l, temp);
		start_l[0] = DotProduct (temp, forward);
		start_l[1] = -DotProduct (temp, right);
		start_l[2] = DotProduct (temp, up);

		VectorCopy (end_l, temp);
		end_l[0] = DotProduct (temp, forward);
		end_l[1] = -DotProduct (temp, right);
		end_l[2] = DotProduct (temp, up);
	}

	/* sweep the box through the model */
	trace = CM_BoxTrace (start_l, end_l, mins, maxs, headnode, brushmask);

	if (rotated && trace.fraction != 1.0)
	{
		VectorNegate (angles, a);
		AngleVectors (a, forward, right, up);

		VectorCopy (trace.plane.normal, temp);
		trace.plane.normal[0] = DotProduct (temp, forward);
		trace.plane.normal[1] = -DotProduct (temp, right);
		trace.plane.normal[2] = DotProduct (temp, up);
	}

	trace.endpos[0] = start[0] + trace.fraction * (end[0] - start[0]);
	trace.endpos[1] = start[1] + trace.fraction * (end[1] - start[1]);
	trace.endpos[2] = start[2] + trace.fraction * (end[2] - start[2]);

	return trace;
}
