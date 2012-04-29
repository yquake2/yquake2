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
 * This file implements loading and parsing of BSP files and trees.
 *
 * =======================================================================
 */

#include "../header/common.h"
#include "../header/cmodel.h"

extern dvis_t *map_vis;

int	numleafs = 1; /* allow leaf funcs to be called without a map */
int	numclusters = 1;
int	numareas = 1;

byte	*cmod_base;

void CMod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	cmodel_t	*out;
	int			i, j, count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");

	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no models");

	if (count > MAX_MAP_MODELS)
		Com_Error (ERR_DROP, "Map has too many models");

	numcmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out = &map_cmodels[i];

		for (j=0 ; j<3 ; j++)
		{
			/* spread the mins / maxs by a pixel */
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}

		out->headnode = LittleLong (in->headnode);
	}
}

void CMod_LoadSurfaces (lump_t *l)
{
	texinfo_t	*in;
	mapsurface_t	*out;
	int			i, count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");

	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no surfaces");

	if (count > MAX_MAP_TEXINFO)
		Com_Error (ERR_DROP, "Map has too many surfaces");

	numtexinfo = count;
	out = map_surfaces;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		strncpy (out->c.name, in->texture, sizeof(out->c.name)-1);
		strncpy (out->rname, in->texture, sizeof(out->rname)-1);
		out->c.flags = LittleLong (in->flags);
		out->c.value = LittleLong (in->value);
	}
}

void CMod_LoadNodes (lump_t *l)
{
	dnode_t		*in;
	int			child;
	cnode_t		*out;
	int			i, j, count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");

	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map has no nodes");

	if (count > MAX_MAP_NODES)
		Com_Error (ERR_DROP, "Map has too many nodes");

	out = map_nodes;

	numnodes = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->plane = map_planes + LittleLong(in->planenum);

		for (j=0 ; j<2 ; j++)
		{
			child = LittleLong (in->children[j]);
			out->children[j] = child;
		}
	}

}

void CMod_LoadBrushes (lump_t *l)
{
	dbrush_t	*in;
	cbrush_t	*out;
	int			i, count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");

	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_BRUSHES)
		Com_Error (ERR_DROP, "Map has too many brushes");

	out = map_brushes;

	numbrushes = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->firstbrushside = LittleLong(in->firstside);
		out->numsides = LittleLong(in->numsides);
		out->contents = LittleLong(in->contents);
	}

}

void CMod_LoadLeafs (lump_t *l)
{
	int			i;
	cleaf_t		*out;
	dleaf_t 	*in;
	int			count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");

	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no leafs");

	/* need to save space for box planes */
	if (count > MAX_MAP_PLANES)
		Com_Error (ERR_DROP, "Map has too many planes");

	out = map_leafs;
	numleafs = count;
	numclusters = 0;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->contents = LittleLong (in->contents);
		out->cluster = LittleShort (in->cluster);
		out->area = LittleShort (in->area);
		out->firstleafbrush = LittleShort (in->firstleafbrush);
		out->numleafbrushes = LittleShort (in->numleafbrushes);

		if (out->cluster >= numclusters)
			numclusters = out->cluster + 1;
	}

	if (map_leafs[0].contents != CONTENTS_SOLID)
		Com_Error (ERR_DROP, "Map leaf 0 is not CONTENTS_SOLID");

	solidleaf = 0;
	emptyleaf = -1;

	for (i=1 ; i<numleafs ; i++)
	{
		if (!map_leafs[i].contents)
		{
			emptyleaf = i;
			break;
		}
	}

	if (emptyleaf == -1)
		Com_Error (ERR_DROP, "Map does not have an empty leaf");
}

void CMod_LoadPlanes (lump_t *l)
{
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");

	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no planes");

	/* need to save space for box planes */
	if (count > MAX_MAP_PLANES)
		Com_Error (ERR_DROP, "Map has too many planes");

	out = map_planes;
	numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;

		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);

			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

void CMod_LoadLeafBrushes (lump_t *l)
{
	int				i;
	unsigned short	*out;
	unsigned short 	*in;
	int				count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");

	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no planes");

	/* need to save space for box planes */
	if (count > MAX_MAP_LEAFBRUSHES)
		Com_Error (ERR_DROP, "Map has too many leafbrushes");

	out = map_leafbrushes;
	numleafbrushes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
		*out = LittleShort (*in);
}

void CMod_LoadBrushSides (lump_t *l)
{
	int				i, j;
	cbrushside_t	*out;
	dbrushside_t 	*in;
	int				count;
	int				num;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");

	count = l->filelen / sizeof(*in);

	/* need to save space for box planes */
	if (count > MAX_MAP_BRUSHSIDES)
		Com_Error (ERR_DROP, "Map has too many planes");

	out = map_brushsides;
	numbrushsides = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		num = LittleShort (in->planenum);
		out->plane = &map_planes[num];
		j = LittleShort (in->texinfo);

		if (j >= numtexinfo)
			Com_Error (ERR_DROP, "Bad brushside texinfo");

		out->surface = &map_surfaces[j];
	}
}

void CMod_LoadAreas (lump_t *l)
{
	int			i;
	carea_t		*out;
	darea_t 	*in;
	int			count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");

	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_AREAS)
		Com_Error (ERR_DROP, "Map has too many areas");

	out = map_areas;
	numareas = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->numareaportals = LittleLong (in->numareaportals);
		out->firstareaportal = LittleLong (in->firstareaportal);
		out->floodvalid = 0;
		out->floodnum = 0;
	}
}

void CMod_LoadAreaPortals (lump_t *l)
{
	dareaportal_t	*out;
	dareaportal_t 	*in;
	int				count;

	in = (void *)(cmod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");

	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_AREAS)
		Com_Error (ERR_DROP, "Map has too many areas");

	out = map_areaportals;
	numareaportals = count;

	memcpy (out, in, sizeof(dareaportal_t)*count);
}

void CMod_LoadVisibility (lump_t *l)
{
	numvisibility = l->filelen;

	if (l->filelen > MAX_MAP_VISIBILITY)
		Com_Error (ERR_DROP, "Map has too large visibility lump");

	memcpy (map_visibility, cmod_base + l->fileofs, l->filelen);

	map_vis->numclusters = LittleLong (map_vis->numclusters);
}

void CMod_LoadEntityString (lump_t *l)
{
	numentitychars = l->filelen;

	if (l->filelen > MAX_MAP_ENTSTRING)
		Com_Error (ERR_DROP, "Map has too large entity lump");

	memcpy (map_entitystring, cmod_base + l->fileofs, l->filelen);
}

/*
 * Loads in the map and all submodels
 */
cmodel_t *CM_LoadMap (char *name, qboolean clientload, unsigned *checksum)
{
	unsigned		*buf;
	int				i;
	dheader_t		header;
	int				length;
	static unsigned	last_checksum;

	map_noareas = Cvar_Get ("map_noareas", "0", 0);

	if (  !strcmp (map_name, name) && (clientload || !Cvar_VariableValue ("flushmap")) )
	{
		*checksum = last_checksum;

		if (!clientload)
		{
			memset (portalopen, 0, sizeof(portalopen));
			FloodAreaConnections ();
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

	if (!name || !name[0])
	{
		numleafs = 1;
		numclusters = 1;
		numareas = 1;
		*checksum = 0;
		return &map_cmodels[0]; /* cinematic servers won't have anything at all */
	}

	length = FS_LoadFile (name, (void **)&buf);

	if (!buf)
		Com_Error (ERR_DROP, "Couldn't load %s", name);

	last_checksum = LittleLong (Com_BlockChecksum (buf, length));
	*checksum = last_checksum;

	header = *(dheader_t *)buf;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)&header)[i] = LittleLong ( ((int *)&header)[i]);

	if (header.version != BSPVERSION)
		Com_Error (ERR_DROP, "CMod_LoadBrushModel: %s has wrong version number (%i should be %i)"
		           , name, header.version, BSPVERSION);

	cmod_base = (byte *)buf;

	/* load into heap */
	CMod_LoadSurfaces (&header.lumps[LUMP_TEXINFO]);
	CMod_LoadLeafs (&header.lumps[LUMP_LEAFS]);
	CMod_LoadLeafBrushes (&header.lumps[LUMP_LEAFBRUSHES]);
	CMod_LoadPlanes (&header.lumps[LUMP_PLANES]);
	CMod_LoadBrushes (&header.lumps[LUMP_BRUSHES]);
	CMod_LoadBrushSides (&header.lumps[LUMP_BRUSHSIDES]);
	CMod_LoadSubmodels (&header.lumps[LUMP_MODELS]);
	CMod_LoadNodes (&header.lumps[LUMP_NODES]);
	CMod_LoadAreas (&header.lumps[LUMP_AREAS]);
	CMod_LoadAreaPortals (&header.lumps[LUMP_AREAPORTALS]);
	CMod_LoadVisibility (&header.lumps[LUMP_VISIBILITY]);
	CMod_LoadEntityString (&header.lumps[LUMP_ENTITIES]);

	FS_FreeFile (buf);

	CM_InitBoxHull ();

	memset (portalopen, 0, sizeof(portalopen));
	FloodAreaConnections ();

	strcpy (map_name, name);

	return &map_cmodels[0];
}

cmodel_t *CM_InlineModel (char *name)
{
	int		num;

	if (!name || name[0] != '*')
		Com_Error (ERR_DROP, "CM_InlineModel: bad name");

	num = atoi (name+1);

	if (num < 1 || num >= numcmodels)
		Com_Error (ERR_DROP, "CM_InlineModel: bad number");

	return &map_cmodels[num];
}

int CM_NumClusters (void)
{
	return numclusters;
}

int CM_NumInlineModels (void)
{
	return numcmodels;
}

char *CM_EntityString (void)
{
	return map_entitystring;
}

int CM_LeafContents (int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error (ERR_DROP, "CM_LeafContents: bad number");

	return map_leafs[leafnum].contents;
}

int CM_LeafCluster (int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error (ERR_DROP, "CM_LeafCluster: bad number");

	return map_leafs[leafnum].cluster;
}

int CM_LeafArea (int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error (ERR_DROP, "CM_LeafArea: bad number");

	return map_leafs[leafnum].area;
}
