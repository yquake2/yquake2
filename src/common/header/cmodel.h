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
 * The header file for the common model stuff.
 *
 * =======================================================================
 */

#ifndef CO_CMODEL_H
#define CO_CMODEL_H

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
	unsigned short	firstleafbrush;
	unsigned short	numleafbrushes;
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

int			checkcount;

char		map_name[MAX_QPATH];

int			numbrushsides;
cbrushside_t map_brushsides[MAX_MAP_BRUSHSIDES];

int			numtexinfo;
mapsurface_t	map_surfaces[MAX_MAP_TEXINFO];

int			numplanes;
cplane_t	map_planes[MAX_MAP_PLANES+6]; /* extra for box hull */

int			numnodes;
cnode_t		map_nodes[MAX_MAP_NODES+6]; /* extra for box hull */

cleaf_t		map_leafs[MAX_MAP_LEAFS];
int			emptyleaf, solidleaf;

int			numleafbrushes;
unsigned short	map_leafbrushes[MAX_MAP_LEAFBRUSHES];

int			numcmodels;
cmodel_t	map_cmodels[MAX_MAP_MODELS];

int			numbrushes;
cbrush_t	map_brushes[MAX_MAP_BRUSHES];

int			numvisibility;
byte		map_visibility[MAX_MAP_VISIBILITY];

int			numentitychars;
char		map_entitystring[MAX_MAP_ENTSTRING];

carea_t		map_areas[MAX_MAP_AREAS];

int			numareaportals;
dareaportal_t map_areaportals[MAX_MAP_AREAPORTALS];

mapsurface_t	nullsurface;

int			floodvalid;

qboolean	portalopen[MAX_MAP_AREAPORTALS];

cvar_t		*map_noareas;

void	CM_InitBoxHull (void);
void	FloodAreaConnections (void);

#ifndef DEDICATED_ONLY
int		c_pointcontents;
int		c_traces, c_brush_traces;
#endif

int CM_BoxLeafnums_headnode (vec3_t mins, vec3_t maxs, int *list, int listsize, int headnode, int *topnode);

#endif
