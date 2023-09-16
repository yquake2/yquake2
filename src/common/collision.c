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
 * The collision model. Slaps "boxes" through the world and checks if
 * they collide with the world model, entities or other boxes.
 *
 * =======================================================================
 */

#include <stdint.h>

#include "header/common.h"

typedef struct
{
	cplane_t	*plane;
	int			children[2]; /* negative numbers are leafs */
} cnode_t;

typedef struct
{
	cplane_t	*plane;
	mapsurface_t	*surface;
} cbrushside_t;

typedef struct
{
	int			contents;
	int			cluster;
	int			area;
	unsigned int	firstleafbrush;
	unsigned int	numleafbrushes;
} cleaf_t;

typedef struct
{
	int			contents;
	int			numsides;
	int			firstbrushside;
	int			checkcount;	/* to avoid repeated testings */
} cbrush_t;

typedef struct
{
	int		numareaportals;
	int		firstareaportal;
	int		floodnum; /* if two areas have equal floodnums, they are connected */
	int		floodvalid;
} carea_t;

static byte *cmod_base;
static byte map_visibility[MAX_MAP_VISIBILITY];
// DG: is casted to int32_t* in SV_FatPVS() so align accordingly
static YQ2_ALIGNAS_TYPE(int32_t) byte pvsrow[MAX_MAP_LEAFS / 8];
static byte phsrow[MAX_MAP_LEAFS / 8];
static carea_t	map_areas[MAX_MAP_AREAS];
static cbrush_t map_brushes[MAX_MAP_BRUSHES];
static cbrushside_t map_brushsides[MAX_MAP_BRUSHSIDES];
static char map_name[MAX_QPATH];
static char map_entitystring[MAX_MAP_ENTSTRING];
static cbrush_t *box_brush;
static cleaf_t	*box_leaf;
static cleaf_t	map_leafs[MAX_MAP_LEAFS];
static cmodel_t map_cmodels[MAX_MAP_MODELS];
static cnode_t	map_nodes[MAX_MAP_NODES+6]; /* extra for box hull */
static cplane_t *box_planes;
static cplane_t map_planes[MAX_MAP_PLANES+6]; /* extra for box hull */
static cvar_t *map_noareas;
static dareaportal_t map_areaportals[MAX_MAP_AREAPORTALS];
static dvis_t *map_vis = (dvis_t *)map_visibility;
static int box_headnode;
static int	checkcount;
static int	emptyleaf, solidleaf;
static int	floodvalid;
static float *leaf_mins, *leaf_maxs;
static int leaf_count, leaf_maxcount;
static int *leaf_list;
static int leaf_topnode;
static int	numareaportals;
static int numareas = 1;
static int	numbrushes;
static int	numbrushsides;
static int numclusters = 1;
static int	numcmodels;
static int	numentitychars;
static int	numleafbrushes;
static int numleafs = 1; /* allow leaf funcs to be called without a map */
static int	numnodes;
static int	numplanes;
int	numtexinfo;
static int	numvisibility;
static int trace_contents;
mapsurface_t map_surfaces[MAX_MAP_TEXINFO];
static mapsurface_t nullsurface;
static qboolean portalopen[MAX_MAP_AREAPORTALS];
static qboolean trace_ispoint; /* optimized case */
static trace_t trace_trace;
static unsigned int	map_leafbrushes[MAX_MAP_LEAFBRUSHES];
static vec3_t trace_start, trace_end;
static vec3_t trace_mins, trace_maxs;
static vec3_t trace_extents;

#ifndef DEDICATED_ONLY
int		c_pointcontents;
int		c_traces, c_brush_traces;
#endif

/* 1/32 epsilon to keep floating point happy */
#define DIST_EPSILON (0.03125f)

static void
FloodArea_r(carea_t *area, int floodnum)
{
	int i;
	dareaportal_t *p;

	if (area->floodvalid == floodvalid)
	{
		if (area->floodnum == floodnum)
		{
			return;
		}

		Com_Error(ERR_DROP, "%s: reflooded", __func__);
	}

	area->floodnum = floodnum;
	area->floodvalid = floodvalid;
	p = &map_areaportals[area->firstareaportal];

	for (i = 0; i < area->numareaportals; i++, p++)
	{
		if (portalopen[LittleLong(p->portalnum)])
		{
			FloodArea_r(&map_areas[LittleLong(p->otherarea)], floodnum);
		}
	}
}

static void
FloodAreaConnections(void)
{
	int i;
	carea_t *area;
	int floodnum;

	/* all current floods are now invalid */
	floodvalid++;
	floodnum = 0;

	/* area 0 is not used */
	for (i = 1; i < numareas; i++)
	{
		area = &map_areas[i];

		if (area->floodvalid == floodvalid)
		{
			continue; /* already flooded into */
		}

		floodnum++;
		FloodArea_r(area, floodnum);
	}
}

void
CM_SetAreaPortalState(int portalnum, qboolean open)
{
	if (portalnum > numareaportals)
	{
		Com_Error(ERR_DROP, "%s: areaportal > numareaportals", __func__);
	}

	portalopen[portalnum] = open;
	FloodAreaConnections();
}

qboolean
CM_AreasConnected(int area1, int area2)
{
	if (map_noareas->value)
	{
		return true;
	}

	if ((area1 > numareas) || (area2 > numareas))
	{
		Com_Error(ERR_DROP, "%s: area > numareas", __func__);
	}

	if (map_areas[area1].floodnum == map_areas[area2].floodnum)
	{
		return true;
	}

	return false;
}

/*
 * Writes a length byte followed by a bit vector of all the areas
 * that area in the same flood as the area parameter
 *
 * This is used by the client refreshes to cull visibility
 */
int
CM_WriteAreaBits(byte *buffer, int area)
{
	int i;
	int floodnum;
	int bytes;

	bytes = (numareas + 7) >> 3;

	if (map_noareas->value)
	{
		/* for debugging, send everything */
		memset(buffer, 255, bytes);
	}

	else
	{
		memset(buffer, 0, bytes);

		floodnum = map_areas[area].floodnum;

		for (i = 0; i < numareas; i++)
		{
			if ((map_areas[i].floodnum == floodnum) || !area)
			{
				buffer[i >> 3] |= 1 << (i & 7);
			}
		}
	}

	return bytes;
}

/*
 * Writes the portal state to a savegame file
 */
void
CM_WritePortalState(FILE *f)
{
	fwrite(portalopen, sizeof(portalopen), 1, f);
}

/*
 * Reads the portal state from a savegame file
 * and recalculates the area connections
 */
void
CM_ReadPortalState(fileHandle_t f)
{
	FS_Read(portalopen, sizeof(portalopen), f);
	FloodAreaConnections();
}

/*
 * Returns true if any leaf under headnode has a cluster that
 * is potentially visible
 */
qboolean
CM_HeadnodeVisible(int nodenum, byte *visbits)
{
	int leafnum1;
	int cluster;
	cnode_t *node;

	if (nodenum < 0)
	{
		leafnum1 = -1 - nodenum;
		cluster = map_leafs[leafnum1].cluster;

		if (cluster == -1)
		{
			return false;
		}

		if (visbits[cluster >> 3] & (1 << (cluster & 7)))
		{
			return true;
		}

		return false;
	}

	node = &map_nodes[nodenum];

	if (CM_HeadnodeVisible(node->children[0], visbits))
	{
		return true;
	}

	return CM_HeadnodeVisible(node->children[1], visbits);
}

/*
 * Set up the planes and nodes so that the six floats of a bounding box
 * can just be stored out and get a proper clipping hull structure.
 */
static void
CM_InitBoxHull(void)
{
	int i;
	int side;
	cnode_t *c;
	cplane_t *p;
	cbrushside_t *s;

	box_headnode = numnodes;
	box_planes = &map_planes[numplanes];

	if ((numnodes + 6 > MAX_MAP_NODES) ||
		(numbrushes + 1 > MAX_MAP_BRUSHES) ||
		(numleafbrushes + 1 > MAX_MAP_LEAFBRUSHES) ||
		(numbrushsides + 6 > MAX_MAP_BRUSHSIDES) ||
		(numplanes + 12 > MAX_MAP_PLANES))
	{
		Com_Error(ERR_DROP, "%s: Not enough room for box tree", __func__);
	}

	box_brush = &map_brushes[numbrushes];
	box_brush->numsides = 6;
	box_brush->firstbrushside = numbrushsides;
	box_brush->contents = CONTENTS_MONSTER;

	box_leaf = &map_leafs[numleafs];
	box_leaf->contents = CONTENTS_MONSTER;
	box_leaf->firstleafbrush = numleafbrushes;
	box_leaf->numleafbrushes = 1;

	map_leafbrushes[numleafbrushes] = numbrushes;

	for (i = 0; i < 6; i++)
	{
		side = i & 1;

		/* brush sides */
		s = &map_brushsides[numbrushsides + i];
		s->plane = map_planes + (numplanes + i * 2 + side);
		s->surface = &nullsurface;

		/* nodes */
		c = &map_nodes[box_headnode + i];
		c->plane = map_planes + (numplanes + i * 2);
		c->children[side] = -1 - emptyleaf;

		if (i != 5)
		{
			c->children[side ^ 1] = box_headnode + i + 1;
		}

		else
		{
			c->children[side ^ 1] = -1 - numleafs;
		}

		/* planes */
		p = &box_planes[i * 2];
		p->type = i >> 1;
		p->signbits = 0;
		VectorClear(p->normal);
		p->normal[i >> 1] = 1;

		p = &box_planes[i * 2 + 1];
		p->type = 3 + (i >> 1);
		p->signbits = 0;
		VectorClear(p->normal);
		p->normal[i >> 1] = -1;
	}
}

/*
 * To keep everything totally uniform, bounding boxes are turned into
 * small BSP trees instead of being compared directly.
 */
int
CM_HeadnodeForBox(vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -maxs[0];
	box_planes[2].dist = mins[0];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = maxs[1];
	box_planes[5].dist = -maxs[1];
	box_planes[6].dist = mins[1];
	box_planes[7].dist = -mins[1];
	box_planes[8].dist = maxs[2];
	box_planes[9].dist = -maxs[2];
	box_planes[10].dist = mins[2];
	box_planes[11].dist = -mins[2];

	return box_headnode;
}

static int
CM_PointLeafnum_r(vec3_t p, int num)
{
	float d;
	cnode_t *node;
	cplane_t *plane;

	while (num >= 0)
	{
		node = map_nodes + num;
		plane = node->plane;

		if (plane->type < 3)
		{
			d = p[plane->type] - plane->dist;
		}

		else
		{
			d = DotProduct(plane->normal, p) - plane->dist;
		}

		if (d < 0)
		{
			num = node->children[1];
		}

		else
		{
			num = node->children[0];
		}
	}

#ifndef DEDICATED_ONLY
	c_pointcontents++; /* optimize counter */
#endif

	return -1 - num;
}

int
CM_PointLeafnum(vec3_t p)
{
	if (!numplanes)
	{
		return 0; /* sound may call this without map loaded */
	}

	return CM_PointLeafnum_r(p, 0);
}

/*
 * Fills in a list of all the leafs touched
 */
static void
CM_BoxLeafnums_r(int nodenum)
{
	cplane_t *plane;
	cnode_t *node;
	int s;

	while (1)
	{
		if (nodenum < 0)
		{
			if (leaf_count >= leaf_maxcount)
			{
				return;
			}

			leaf_list[leaf_count++] = -1 - nodenum;
			return;
		}

		node = &map_nodes[nodenum];
		plane = node->plane;
		s = BOX_ON_PLANE_SIDE(leaf_mins, leaf_maxs, plane);

		if (s == 1)
		{
			nodenum = node->children[0];
		}

		else if (s == 2)
		{
			nodenum = node->children[1];
		}

		else
		{
			/* go down both */
			if (leaf_topnode == -1)
			{
				leaf_topnode = nodenum;
			}

			CM_BoxLeafnums_r(node->children[0]);
			nodenum = node->children[1];
		}
	}
}

static int
CM_BoxLeafnums_headnode(vec3_t mins, vec3_t maxs, int *list,
		int listsize, int headnode, int *topnode)
{
	leaf_list = list;
	leaf_count = 0;
	leaf_maxcount = listsize;
	leaf_mins = mins;
	leaf_maxs = maxs;

	leaf_topnode = -1;

	CM_BoxLeafnums_r(headnode);

	if (topnode)
	{
		*topnode = leaf_topnode;
	}

	return leaf_count;
}

int
CM_BoxLeafnums(vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode)
{
	return CM_BoxLeafnums_headnode(mins, maxs, list,
			listsize, map_cmodels[0].headnode, topnode);
}

int
CM_PointContents(vec3_t p, int headnode)
{
	int l;

	if (!numnodes) /* map not loaded */
	{
		return 0;
	}

	l = CM_PointLeafnum_r(p, headnode);

	return map_leafs[l].contents;
}

/*
 * Handles offseting and rotation of the end points for moving and
 * rotating entities
 */
int
CM_TransformedPointContents(vec3_t p, int headnode,
		vec3_t origin, vec3_t angles)
{
	vec3_t p_l;
	vec3_t temp;
	vec3_t forward, right, up;
	int l;

	/* subtract origin offset */
	VectorSubtract(p, origin, p_l);

	/* rotate start and end into the models frame of reference */
	if ((headnode != box_headnode) &&
		(angles[0] || angles[1] || angles[2]))
	{
		AngleVectors(angles, forward, right, up);

		VectorCopy(p_l, temp);
		p_l[0] = DotProduct(temp, forward);
		p_l[1] = -DotProduct(temp, right);
		p_l[2] = DotProduct(temp, up);
	}

	l = CM_PointLeafnum_r(p_l, headnode);

	return map_leafs[l].contents;
}

static void
CM_ClipBoxToBrush(vec3_t mins, vec3_t maxs, vec3_t p1,
		vec3_t p2, trace_t *trace, cbrush_t *brush)
{
	int i, j;
	cplane_t *plane, *clipplane;
	float dist;
	float enterfrac, leavefrac;
	vec3_t ofs;
	float d1, d2;
	qboolean getout, startout;
	float f;
	cbrushside_t *side, *leadside;

	enterfrac = -1;
	leavefrac = 1;
	clipplane = NULL;

	if (!brush->numsides)
	{
		return;
	}

#ifndef DEDICATED_ONLY
	c_brush_traces++;
#endif

	getout = false;
	startout = false;
	leadside = NULL;

	for (i = 0; i < brush->numsides; i++)
	{
		side = &map_brushsides[brush->firstbrushside + i];
		plane = side->plane;

		if (!trace_ispoint)
		{
			/* general box case
			   push the plane out
			   apropriately for mins/maxs */
			for (j = 0; j < 3; j++)
			{
				if (plane->normal[j] < 0)
				{
					ofs[j] = maxs[j];
				}

				else
				{
					ofs[j] = mins[j];
				}
			}

			dist = DotProduct(ofs, plane->normal);
			dist = plane->dist - dist;
		}

		else
		{
			/* special point case */
			dist = plane->dist;
		}

		d1 = DotProduct(p1, plane->normal) - dist;
		d2 = DotProduct(p2, plane->normal) - dist;

		if (d2 > 0)
		{
			getout = true; /* endpoint is not in solid */
		}

		if (d1 > 0)
		{
			startout = true;
		}

		/* if completely in front of face, no intersection */
		if ((d1 > 0) && (d2 >= d1))
		{
			return;
		}

		if ((d1 <= 0) && (d2 <= 0))
		{
			continue;
		}

		/* crosses face */
		if (d1 > d2)
		{
			/* enter */
			f = (d1 - DIST_EPSILON) / (d1 - d2);

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
			f = (d1 + DIST_EPSILON) / (d1 - d2);

			if (f < leavefrac)
			{
				leavefrac = f;
			}
		}
	}

	if (!startout)
	{
		/* original point was inside brush */
		trace->startsolid = true;

		if (!getout)
		{
			trace->allsolid = true;
		}

		return;
	}

	if (enterfrac < leavefrac)
	{
		if ((enterfrac > -1) && (enterfrac < trace->fraction))
		{
			if (enterfrac < 0)
			{
				enterfrac = 0;
			}

			if (clipplane == NULL)
			{
				Com_Error(ERR_FATAL, "%s: clipplane was NULL!\n", __func__);
			}

			trace->fraction = enterfrac;
			trace->plane = *clipplane;
			trace->surface = &(leadside->surface->c);
			trace->contents = brush->contents;
		}
	}
}

static void
CM_TestBoxInBrush(vec3_t mins, vec3_t maxs, vec3_t p1,
		trace_t *trace, cbrush_t *brush)
{
	int i, j;
	cplane_t *plane;
	float dist;
	vec3_t ofs;
	float d1;
	cbrushside_t *side;

	if (!brush->numsides)
	{
		return;
	}

	for (i = 0; i < brush->numsides; i++)
	{
		side = &map_brushsides[brush->firstbrushside + i];
		plane = side->plane;

		/* general box case
		   push the plane out
		   apropriately for mins/maxs */
		for (j = 0; j < 3; j++)
		{
			if (plane->normal[j] < 0)
			{
				ofs[j] = maxs[j];
			}

			else
			{
				ofs[j] = mins[j];
			}
		}

		dist = DotProduct(ofs, plane->normal);
		dist = plane->dist - dist;

		d1 = DotProduct(p1, plane->normal) - dist;

		/* if completely in front of face, no intersection */
		if (d1 > 0)
		{
			return;
		}
	}

	/* inside this brush */
	trace->startsolid = trace->allsolid = true;
	trace->fraction = 0;
	trace->contents = brush->contents;
}

static void
CM_TraceToLeaf(int leafnum)
{
	int k;
	int brushnum;
	cleaf_t *leaf;
	cbrush_t *b;

	leaf = &map_leafs[leafnum];

	if (!(leaf->contents & trace_contents))
	{
		return;
	}

	/* trace line against all brushes in the leaf */
	for (k = 0; k < leaf->numleafbrushes; k++)
	{
		brushnum = map_leafbrushes[leaf->firstleafbrush + k];
		b = &map_brushes[brushnum];

		if (b->checkcount == checkcount)
		{
			continue; /* already checked this brush in another leaf */
		}

		b->checkcount = checkcount;

		if (!(b->contents & trace_contents))
		{
			continue;
		}

		CM_ClipBoxToBrush(trace_mins, trace_maxs, trace_start,
				trace_end, &trace_trace, b);

		if (!trace_trace.fraction)
		{
			return;
		}
	}
}

static void
CM_TestInLeaf(int leafnum)
{
	int k;
	int brushnum;
	cleaf_t *leaf;
	cbrush_t *b;

	leaf = &map_leafs[leafnum];

	if (!(leaf->contents & trace_contents))
	{
		return;
	}

	/* trace line against all brushes in the leaf */
	for (k = 0; k < leaf->numleafbrushes; k++)
	{
		brushnum = map_leafbrushes[leaf->firstleafbrush + k];
		b = &map_brushes[brushnum];

		if (b->checkcount == checkcount)
		{
			continue; /* already checked this brush in another leaf */
		}

		b->checkcount = checkcount;

		if (!(b->contents & trace_contents))
		{
			continue;
		}

		CM_TestBoxInBrush(trace_mins, trace_maxs, trace_start, &trace_trace, b);

		if (!trace_trace.fraction)
		{
			return;
		}
	}
}

static void
CM_RecursiveHullCheck(int num, float p1f, float p2f, vec3_t p1, vec3_t p2)
{
	cnode_t *node;
	cplane_t *plane;
	float t1, t2, offset;
	float frac, frac2;
	float idist;
	int i;
	vec3_t mid;
	int side;
	float midf;

	if (trace_trace.fraction <= p1f)
	{
		return; /* already hit something nearer */
	}

	/* if < 0, we are in a leaf node */
	if (num < 0)
	{
		CM_TraceToLeaf(-1 - num);
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
		t1 = DotProduct(plane->normal, p1) - plane->dist;
		t2 = DotProduct(plane->normal, p2) - plane->dist;

		if (trace_ispoint)
		{
			offset = 0;
		}

		else
		{
			offset = (float)fabs(trace_extents[0] * plane->normal[0]) +
					 (float)fabs(trace_extents[1] * plane->normal[1]) +
					 (float)fabs(trace_extents[2] * plane->normal[2]);
		}
	}

	/* see which sides we need to consider */
	if ((t1 >= offset) && (t2 >= offset))
	{
		CM_RecursiveHullCheck(node->children[0], p1f, p2f, p1, p2);
		return;
	}

	if ((t1 < -offset) && (t2 < -offset))
	{
		CM_RecursiveHullCheck(node->children[1], p1f, p2f, p1, p2);
		return;
	}

	/* put the crosspoint DIST_EPSILON pixels on the near side */
	if (t1 < t2)
	{
		idist = 1.0f / (t1 - t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON) * idist;
		frac = (t1 - offset + DIST_EPSILON) * idist;
	}

	else if (t1 > t2)
	{
		idist = 1.0 / (t1 - t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON) * idist;
		frac = (t1 + offset + DIST_EPSILON) * idist;
	}

	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	/* move up to the node */
	if (frac < 0)
	{
		frac = 0;
	}

	if (frac > 1)
	{
		frac = 1;
	}

	midf = p1f + (p2f - p1f) * frac;

	for (i = 0; i < 3; i++)
	{
		mid[i] = p1[i] + frac * (p2[i] - p1[i]);
	}

	CM_RecursiveHullCheck(node->children[side], p1f, midf, p1, mid);

	/* go past the node */
	if (frac2 < 0)
	{
		frac2 = 0;
	}

	if (frac2 > 1)
	{
		frac2 = 1;
	}

	midf = p1f + (p2f - p1f) * frac2;

	for (i = 0; i < 3; i++)
	{
		mid[i] = p1[i] + frac2 * (p2[i] - p1[i]);
	}

	CM_RecursiveHullCheck(node->children[side ^ 1], midf, p2f, mid, p2);
}

trace_t
CM_BoxTrace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs,
		int headnode, int brushmask)
{
	int i;

	checkcount++; /* for multi-check avoidance */

#ifndef DEDICATED_ONLY
	c_traces++; /* for statistics, may be zeroed */
#endif

	/* fill in a default trace */
	memset(&trace_trace, 0, sizeof(trace_trace));
	trace_trace.fraction = 1;
	trace_trace.surface = &(nullsurface.c);

	if (!numnodes)  /* map not loaded */
	{
		return trace_trace;
	}

	trace_contents = brushmask;
	VectorCopy(start, trace_start);
	VectorCopy(end, trace_end);
	VectorCopy(mins, trace_mins);
	VectorCopy(maxs, trace_maxs);

	/* check for position test special case */
	if ((start[0] == end[0]) && (start[1] == end[1]) && (start[2] == end[2]))
	{
		int leafs[1024];
		int i, numleafs;
		vec3_t c1, c2;
		int topnode;

		VectorAdd(start, mins, c1);
		VectorAdd(start, maxs, c2);

		for (i = 0; i < 3; i++)
		{
			c1[i] -= 1;
			c2[i] += 1;
		}

		numleafs = CM_BoxLeafnums_headnode(c1, c2, leafs, 1024,
				headnode, &topnode);

		for (i = 0; i < numleafs; i++)
		{
			CM_TestInLeaf(leafs[i]);

			if (trace_trace.allsolid)
			{
				break;
			}
		}

		VectorCopy(start, trace_trace.endpos);
		return trace_trace;
	}

	/* check for point special case */
	if ((mins[0] == 0) && (mins[1] == 0) && (mins[2] == 0) &&
		(maxs[0] == 0) && (maxs[1] == 0) && (maxs[2] == 0))
	{
		trace_ispoint = true;
		VectorClear(trace_extents);
	}

	else
	{
		trace_ispoint = false;
		trace_extents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		trace_extents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		trace_extents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	/* general sweeping through world */
	CM_RecursiveHullCheck(headnode, 0, 1, start, end);

	if (trace_trace.fraction == 1)
	{
		VectorCopy(end, trace_trace.endpos);
	}

	else
	{
		for (i = 0; i < 3; i++)
		{
			trace_trace.endpos[i] = start[i] + trace_trace.fraction *
									(end[i] - start[i]);
		}
	}

	return trace_trace;
}

/*
 * Handles offseting and rotation of the end points for moving and
 * rotating entities
 */
trace_t
CM_TransformedBoxTrace(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs,
		int headnode, int brushmask, vec3_t origin, vec3_t angles)
{
	trace_t trace;
	vec3_t start_l, end_l;
	vec3_t a;
	vec3_t forward, right, up;
	vec3_t temp;
	qboolean rotated;

	/* subtract origin offset */
	VectorSubtract(start, origin, start_l);
	VectorSubtract(end, origin, end_l);

	/* rotate start and end into the models frame of reference */
	if ((headnode != box_headnode) &&
		(angles[0] || angles[1] || angles[2]))
	{
		rotated = true;
	}

	else
	{
		rotated = false;
	}

	if (rotated)
	{
		AngleVectors(angles, forward, right, up);

		VectorCopy(start_l, temp);
		start_l[0] = DotProduct(temp, forward);
		start_l[1] = -DotProduct(temp, right);
		start_l[2] = DotProduct(temp, up);

		VectorCopy(end_l, temp);
		end_l[0] = DotProduct(temp, forward);
		end_l[1] = -DotProduct(temp, right);
		end_l[2] = DotProduct(temp, up);
	}

	/* sweep the box through the model */
	trace = CM_BoxTrace(start_l, end_l, mins, maxs, headnode, brushmask);

	if (rotated && (trace.fraction != 1.0))
	{
		VectorNegate(angles, a);
		AngleVectors(a, forward, right, up);

		VectorCopy(trace.plane.normal, temp);
		trace.plane.normal[0] = DotProduct(temp, forward);
		trace.plane.normal[1] = -DotProduct(temp, right);
		trace.plane.normal[2] = DotProduct(temp, up);
	}

	trace.endpos[0] = start[0] + trace.fraction * (end[0] - start[0]);
	trace.endpos[1] = start[1] + trace.fraction * (end[1] - start[1]);
	trace.endpos[2] = start[2] + trace.fraction * (end[2] - start[2]);

	return trace;
}

static void
CMod_LoadSubmodels(lump_t *l)
{
	dmodel_t *in;
	cmodel_t *out;
	int i, j, count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size", __func__);
	}

	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Com_Error(ERR_DROP, "%s: Map with no models", __func__);
	}

	if (count > MAX_MAP_MODELS)
	{
		Com_Error(ERR_DROP, "%s: Map has too many models", __func__);
	}

	numcmodels = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out = &map_cmodels[i];

		for (j = 0; j < 3; j++)
		{
			/* spread the mins / maxs by a pixel */
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
			out->origin[j] = LittleFloat(in->origin[j]);
		}

		out->headnode = LittleLong(in->headnode);
	}
}

static void
CMod_LoadSurfaces(lump_t *l)
{
	texinfo_t *in;
	mapsurface_t *out;
	int i, count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size", __func__);
	}

	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Com_Error(ERR_DROP, "%s: Map with no surfaces", __func__);
	}

	if (count > MAX_MAP_TEXINFO)
	{
		Com_Error(ERR_DROP, "%s: Map has too many surfaces", __func__);
	}

	numtexinfo = count;
	out = map_surfaces;

	for (i = 0; i < count; i++, in++, out++)
	{
		Q_strlcpy(out->c.name, in->texture, sizeof(out->c.name));
		Q_strlcpy(out->rname, in->texture, sizeof(out->rname));
		out->c.flags = LittleLong(in->flags);
		out->c.value = LittleLong(in->value);
	}
}

static void
CMod_LoadNodes(lump_t *l)
{
	dnode_t *in;
	int child;
	cnode_t *out;
	int i, j, count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size %ld", __func__, sizeof(*in));
	}

	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Com_Error(ERR_DROP, "%s: Map has no nodes", __func__);
	}

	if (count > MAX_MAP_NODES)
	{
		Com_Error(ERR_DROP, "%s: Map has too many nodes", __func__);
	}

	out = map_nodes;

	numnodes = count;

	for (i = 0; i < count; i++, out++, in++)
	{
		out->plane = map_planes + LittleLong(in->planenum);

		for (j = 0; j < 2; j++)
		{
			child = LittleLong(in->children[j]);
			out->children[j] = child;
		}
	}
}

static void
CMod_LoadQNodes(lump_t *l)
{
	dqnode_t *in;
	int child;
	cnode_t *out;
	int i, j, count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size %ld", __func__, sizeof(*in));
	}

	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Com_Error(ERR_DROP, "%s: Map has no nodes", __func__);
	}

	if (count > MAX_MAP_NODES)
	{
		Com_Error(ERR_DROP, "%s: Map has too many nodes", __func__);
	}

	out = map_nodes;

	numnodes = count;

	for (i = 0; i < count; i++, out++, in++)
	{
		out->plane = map_planes + LittleLong(in->planenum);

		for (j = 0; j < 2; j++)
		{
			child = LittleLong(in->children[j]);
			out->children[j] = child;
		}
	}
}

static void
CMod_LoadBrushes(lump_t *l)
{
	dbrush_t *in;
	cbrush_t *out;
	int i, count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size", __func__);
	}

	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_BRUSHES)
	{
		Com_Error(ERR_DROP, "%s: Map has too many brushes", __func__);
	}

	out = map_brushes;

	numbrushes = count;

	for (i = 0; i < count; i++, out++, in++)
	{
		out->firstbrushside = LittleLong(in->firstside);
		out->numsides = LittleLong(in->numsides);
		out->contents = LittleLong(in->contents);
	}
}

static void
CMod_LoadLeafs(lump_t *l)
{
	int i;
	cleaf_t *out;
	dleaf_t *in;
	int count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size", __func__);
	}

	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Com_Error(ERR_DROP, "%s: Map with no leafs", __func__);
	}

	/* need to save space for box planes */
	if (count > MAX_MAP_PLANES)
	{
		Com_Error(ERR_DROP, "%s: Map has too many planes", __func__);
	}

	out = map_leafs;
	numleafs = count;
	numclusters = 0;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->contents = LittleLong(in->contents);
		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);
		out->firstleafbrush = LittleShort(in->firstleafbrush);
		out->numleafbrushes = LittleShort(in->numleafbrushes);

		if (out->cluster >= numclusters)
		{
			numclusters = out->cluster + 1;
		}
	}

	if (map_leafs[0].contents != CONTENTS_SOLID)
	{
		Com_Error(ERR_DROP, "%s: Map leaf 0 is not CONTENTS_SOLID", __func__);
	}

	solidleaf = 0;
	emptyleaf = -1;

	for (i = 1; i < numleafs; i++)
	{
		if (!map_leafs[i].contents)
		{
			emptyleaf = i;
			break;
		}
	}

	if (emptyleaf == -1)
	{
		Com_Error(ERR_DROP, "%s: Map does not have an empty leaf", __func__);
	}
}

static void
CMod_LoadQLeafs(lump_t *l)
{
	int i;
	cleaf_t *out;
	dqleaf_t *in;
	int count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size", __func__);
	}

	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Com_Error(ERR_DROP, "%s: Map with no leafs", __func__);
	}

	/* need to save space for box planes */
	if (count > MAX_MAP_PLANES)
	{
		Com_Error(ERR_DROP, "%s: Map has too many planes", __func__);
	}

	out = map_leafs;
	numleafs = count;
	numclusters = 0;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->contents = LittleLong(in->contents);
		out->cluster = LittleLong(in->cluster);
		out->area = LittleLong(in->area);
		out->firstleafbrush = LittleFloat(in->firstleafbrush);
		out->numleafbrushes = LittleFloat(in->numleafbrushes);

		if (out->cluster >= numclusters)
		{
			numclusters = out->cluster + 1;
		}
	}

	if (map_leafs[0].contents != CONTENTS_SOLID)
	{
		Com_Error(ERR_DROP, "%s: Map leaf 0 is not CONTENTS_SOLID", __func__);
	}

	solidleaf = 0;
	emptyleaf = -1;

	for (i = 1; i < numleafs; i++)
	{
		if (!map_leafs[i].contents)
		{
			emptyleaf = i;
			break;
		}
	}

	if (emptyleaf == -1)
	{
		Com_Error(ERR_DROP, "%s: Map does not have an empty leaf", __func__);
	}
}

static void
CMod_LoadPlanes(lump_t *l)
{
	int i, j;
	cplane_t *out;
	dplane_t *in;
	int count;
	int bits;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size", __func__);
	}

	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Com_Error(ERR_DROP, "%s: Map with no planes", __func__);
	}

	/* need to save space for box planes */
	if (count > MAX_MAP_PLANES)
	{
		Com_Error(ERR_DROP, "%s: Map has too many planes", __func__);
	}

	out = map_planes;
	numplanes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		bits = 0;

		for (j = 0; j < 3; j++)
		{
			out->normal[j] = LittleFloat(in->normal[j]);

			if (out->normal[j] < 0)
			{
				bits |= 1 << j;
			}
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->signbits = bits;
	}
}

static void
CMod_LoadLeafBrushes(lump_t *l)
{
	int i;
	unsigned int *out;
	unsigned short *in;
	int count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size", __func__);
	}

	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Com_Error(ERR_DROP, "%s: Map with no planes", __func__);
	}

	/* need to save space for box planes */
	if (count > MAX_MAP_LEAFBRUSHES)
	{
		Com_Error(ERR_DROP, "%s: Map has too many leafbrushes", __func__);
	}

	out = map_leafbrushes;
	numleafbrushes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		*out = LittleShort(*in);
	}
}

static void
CMod_LoadQLeafBrushes(lump_t *l)
{
	int i;
	unsigned int *out;
	unsigned int *in;
	int count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size", __func__);
	}

	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Com_Error(ERR_DROP, "%s: Map with no planes", __func__);
	}

	/* need to save space for box planes */
	if (count > MAX_MAP_LEAFBRUSHES)
	{
		Com_Error(ERR_DROP, "%s: Map has too many leafbrushes", __func__);
	}

	out = map_leafbrushes;
	numleafbrushes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		*out = LittleLong(*in);
	}
}

static void
CMod_LoadBrushSides(lump_t *l)
{
	int i, j;
	cbrushside_t *out;
	dbrushside_t *in;
	int count;
	int num;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size", __func__);
	}

	count = l->filelen / sizeof(*in);

	/* need to save space for box planes */
	if (count > MAX_MAP_BRUSHSIDES)
	{
		Com_Error(ERR_DROP, "%s: Map has too many planes", __func__);
	}

	out = map_brushsides;
	numbrushsides = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		num = LittleShort(in->planenum);
		out->plane = &map_planes[num];
		j = LittleShort(in->texinfo);

		if (j >= numtexinfo)
		{
			Com_Error(ERR_DROP, "%s: Bad brushside texinfo", __func__);
		}

		out->surface = (j >= 0) ? &map_surfaces[j] : &nullsurface;
	}
}
static void
CMod_LoadQBrushSides(lump_t *l)
{
	int i, j;
	cbrushside_t *out;
	dqbrushside_t *in;
	int count;
	int num;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size", __func__);
	}

	count = l->filelen / sizeof(*in);

	/* need to save space for box planes */
	if (count > MAX_MAP_BRUSHSIDES)
	{
		Com_Error(ERR_DROP, "%s: Map has too many planes", __func__);
	}

	out = map_brushsides;
	numbrushsides = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		num = LittleLong(in->planenum);
		out->plane = &map_planes[num];
		j = LittleLong(in->texinfo);

		if (j >= numtexinfo)
		{
			Com_Error(ERR_DROP, "%s: Bad brushside texinfo", __func__);
		}

		out->surface = (j >= 0) ? &map_surfaces[j] : &nullsurface;
	}
}

static void
CMod_LoadAreas(lump_t *l)
{
	int i;
	carea_t *out;
	darea_t *in;
	int count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size", __func__);
	}

	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_AREAS)
	{
		Com_Error(ERR_DROP, "%s: Map has too many areas", __func__);
	}

	out = map_areas;
	numareas = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->numareaportals = LittleLong(in->numareaportals);
		out->firstareaportal = LittleLong(in->firstareaportal);
		out->floodvalid = 0;
		out->floodnum = 0;
	}
}

static void
CMod_LoadAreaPortals(lump_t *l)
{
	dareaportal_t *out;
	dareaportal_t *in;
	int count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size", __func__);
	}

	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_AREAS)
	{
		Com_Error(ERR_DROP, "%s: Map has too many areas", __func__);
	}

	out = map_areaportals;
	numareaportals = count;

	memcpy(out, in, sizeof(dareaportal_t) * count);
}

static void
CMod_LoadVisibility(lump_t *l)
{
	numvisibility = l->filelen;

	if (l->filelen > MAX_MAP_VISIBILITY)
	{
		Com_Error(ERR_DROP, "%s: Map has too large visibility lump", __func__);
	}

	memcpy(map_visibility, cmod_base + l->fileofs, l->filelen);

	map_vis->numclusters = LittleLong(map_vis->numclusters);
}

static void
CMod_LoadEntityString(lump_t *l, char *name)
{
	if (sv_entfile->value)
	{
		char s[MAX_QPATH];
		char *buffer = NULL;
		int nameLen, bufLen;

		nameLen = strlen(name);
		strcpy(s, name);
		s[nameLen-3] = 'e';	s[nameLen-2] = 'n';	s[nameLen-1] = 't';
		bufLen = FS_LoadFile(s, (void **)&buffer);

		if (buffer != NULL && bufLen > 1)
		{
			if (bufLen + 1 > sizeof(map_entitystring))
			{
				Com_Printf("CMod_LoadEntityString: .ent file %s too large: %i > %lu.\n", s, bufLen, (unsigned long)sizeof(map_entitystring));
				FS_FreeFile(buffer);
			}
			else
			{
				Com_Printf ("CMod_LoadEntityString: .ent file %s loaded.\n", s);
				numentitychars = bufLen;
				memcpy(map_entitystring, buffer, bufLen);
				map_entitystring[bufLen] = 0; /* jit entity bug - null terminate the entity string! */
				FS_FreeFile(buffer);
				return;
			}
		}
		else if (bufLen != -1)
		{
			/* If the .ent file is too small, don't load. */
			Com_Printf("CMod_LoadEntityString: .ent file %s too small.\n", s);
			FS_FreeFile(buffer);
		}
	}

	numentitychars = l->filelen;

	if (l->filelen + 1 > sizeof(map_entitystring))
	{
		Com_Error(ERR_DROP, "%s: Map has too large entity lump", __func__);
	}

	memcpy(map_entitystring, cmod_base + l->fileofs, l->filelen);
	map_entitystring[l->filelen] = 0;
}

/*
 * Loads in the map and all submodels
 */
cmodel_t *
CM_LoadMap(char *name, qboolean clientload, unsigned *checksum)
{
	unsigned *buf;
	int i;
	dheader_t header;
	int length;
	static unsigned last_checksum;

	map_noareas = Cvar_Get("map_noareas", "0", 0);

	if (strcmp(map_name, name) == 0
		&& (clientload || !Cvar_VariableValue("flushmap")))
	{
		*checksum = last_checksum;

		if (!clientload)
		{
			memset(portalopen, 0, sizeof(portalopen));
			FloodAreaConnections();
		}

		return &map_cmodels[0]; /* still have the right version */
	}

	/* free old stuff */
	numplanes = 0;
	numnodes = 0;
	numleafs = 0;
	numcmodels = 0;
	numvisibility = 0;
	numentitychars = 0;
	map_entitystring[0] = 0;
	map_name[0] = 0;

	if (!name[0])
	{
		numleafs = 1;
		numclusters = 1;
		numareas = 1;
		*checksum = 0;
		return &map_cmodels[0]; /* cinematic servers won't have anything at all */
	}

	length = FS_LoadFile(name, (void **)&buf);

	if (!buf)
	{
		Com_Error(ERR_DROP, "%s: Couldn't load %s", name, __func__);
	}

	last_checksum = LittleLong(Com_BlockChecksum(buf, length));
	*checksum = last_checksum;

	header = *(dheader_t *)buf;

	for (i = 0; i < sizeof(dheader_t) / 4; i++)
	{
		((int *)&header)[i] = LittleLong(((int *)&header)[i]);
	}

	if (header.ident != IDBSPHEADER && header.ident != QDBSPHEADER)
	{
		Com_Error(ERR_DROP, "%s: %s has wrong ident (%i should be %i)",
				__func__, name, header.ident, IDBSPHEADER);
	}

	if (header.version != BSPVERSION)
	{
		Com_Error(ERR_DROP,
				"%s: %s has wrong version number (%i should be %i)",
				__func__, name, header.version, BSPVERSION);
	}

	cmod_base = (byte *)buf;

	/* load into heap */
	CMod_LoadSurfaces(&header.lumps[LUMP_TEXINFO]);
	if (header.ident == IDBSPHEADER)
	{
		CMod_LoadLeafs(&header.lumps[LUMP_LEAFS]);
		CMod_LoadLeafBrushes(&header.lumps[LUMP_LEAFBRUSHES]);
	}
	else
	{
		CMod_LoadQLeafs(&header.lumps[LUMP_LEAFS]);
		CMod_LoadQLeafBrushes(&header.lumps[LUMP_LEAFBRUSHES]);
	}
	CMod_LoadPlanes(&header.lumps[LUMP_PLANES]);
	CMod_LoadBrushes(&header.lumps[LUMP_BRUSHES]);
	if (header.ident == IDBSPHEADER)
	{
		CMod_LoadBrushSides(&header.lumps[LUMP_BRUSHSIDES]);
	}
	else
	{
		CMod_LoadQBrushSides(&header.lumps[LUMP_BRUSHSIDES]);
	}
	CMod_LoadSubmodels(&header.lumps[LUMP_MODELS]);
	if (header.ident == IDBSPHEADER)
	{
		CMod_LoadNodes(&header.lumps[LUMP_NODES]);
	}
	else
	{
		CMod_LoadQNodes(&header.lumps[LUMP_NODES]);
	}
	CMod_LoadAreas(&header.lumps[LUMP_AREAS]);
	CMod_LoadAreaPortals(&header.lumps[LUMP_AREAPORTALS]);
	CMod_LoadVisibility(&header.lumps[LUMP_VISIBILITY]);
	/* From kmquake2: adding an extra parameter for .ent support. */
	CMod_LoadEntityString(&header.lumps[LUMP_ENTITIES], name);

	FS_FreeFile(buf);

	CM_InitBoxHull();

	memset(portalopen, 0, sizeof(portalopen));
	FloodAreaConnections();

	strcpy(map_name, name);

	return &map_cmodels[0];
}

cmodel_t *
CM_InlineModel(char *name)
{
	int num;

	if (!name || (name[0] != '*'))
	{
		Com_Error(ERR_DROP, "%s: bad name", __func__);
	}

	num = (int)strtol(name + 1, (char **)NULL, 10);

	if ((num < 1) || (num >= numcmodels))
	{
		Com_Error(ERR_DROP, "%s: bad number", __func__);
	}

	return &map_cmodels[num];
}

int
CM_NumClusters(void)
{
	return numclusters;
}

int
CM_NumInlineModels(void)
{
	return numcmodels;
}

char *
CM_EntityString(void)
{
	return map_entitystring;
}

int
CM_LeafContents(int leafnum)
{
	if ((leafnum < 0) || (leafnum >= numleafs))
	{
		Com_Error(ERR_DROP, "%s: bad number", __func__);
	}

	return map_leafs[leafnum].contents;
}

int
CM_LeafCluster(int leafnum)
{
	if ((leafnum < 0) || (leafnum >= numleafs))
	{
		Com_Error(ERR_DROP, "%s: bad number", __func__);
	}

	return map_leafs[leafnum].cluster;
}

int
CM_LeafArea(int leafnum)
{
	if ((leafnum < 0) || (leafnum >= numleafs))
	{
		Com_Error(ERR_DROP, "%s: bad number", __func__);
	}

	return map_leafs[leafnum].area;
}

static void
CM_DecompressVis(byte *in, byte *out)
{
	int c;
	byte *out_p;
	int row;

	row = (numclusters + 7) >> 3;
	out_p = out;

	if (!in || !numvisibility)
	{
		/* no vis info, so make all visible */
		while (row)
		{
			*out_p++ = 0xff;
			row--;
		}

		return;
	}

	do
	{
		if (*in)
		{
			*out_p++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;

		if ((out_p - out) + c > row)
		{
			c = row - (out_p - out);
			Com_DPrintf("warning: Vis decompression overrun\n");
		}

		while (c)
		{
			*out_p++ = 0;
			c--;
		}
	}
	while (out_p - out < row);
}

byte *
CM_ClusterPVS(int cluster)
{
	if (cluster == -1)
	{
		memset(pvsrow, 0, (numclusters + 7) >> 3);
	}
	else
	{
		CM_DecompressVis(map_visibility +
				LittleLong(map_vis->bitofs[cluster][DVIS_PVS]), pvsrow);
	}

	return pvsrow;
}

byte *
CM_ClusterPHS(int cluster)
{
	if (cluster == -1)
	{
		memset(phsrow, 0, (numclusters + 7) >> 3);
	}

	else
	{
		CM_DecompressVis(map_visibility +
				LittleLong(map_vis->bitofs[cluster][DVIS_PHS]), phsrow);
	}

	return phsrow;
}

