/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2018-2019 Krzysztof Kondrak

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// vk_model.c -- model loading and caching

#include "header/local.h"

YQ2_ALIGNAS_TYPE(int) static byte mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
static model_t	*models_known;
static int		mod_numknown = 0;
static int		mod_loaded = 0;
static int		mod_max = 0;
static int 	models_known_max = 0;

int		registration_sequence;

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;

	if (!model || !model->nodes)
		ri.Sys_Error (ERR_DROP, "%s: bad model", __func__);

	node = model->nodes;
	while (1)
	{
		cplane_t	*plane;
		float	d;

		if (node->contents != -1)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return NULL;	// never reached
}


/*
==============
Mod_ClusterPVS
==============
*/
byte *Mod_ClusterPVS (int cluster, model_t *model)
{
	if (cluster == -1 || !model->vis)
		return mod_novis;

	return Mod_DecompressVis ( (byte *)model->vis +
		model->vis->bitofs[cluster][DVIS_PVS],
		(model->vis->numclusters+7)>>3);
}


//===============================================================================

/*
===============
Mod_Reallocate
===============
*/
void Mod_Reallocate (void)
{
	if ((models_known_max >= (mod_max * 4)) && (models_known != NULL))
	{
		return;
	}

	if (models_known)
	{
		// Always up if already allocated
		models_known_max *= 2;
		// free up
		Mod_FreeAll();
		Mod_FreeModelsKnown();
	}

	if (models_known_max < (mod_max * 4))
	{
		// double is not enough?
		models_known_max = ROUNDUP(mod_max * 4, 16);
	}

	R_Printf(PRINT_ALL, "Reallocate space for %d models.\n", models_known_max);

	models_known = malloc(models_known_max * sizeof(model_t));
	memset(models_known, 0, models_known_max * sizeof(model_t));
}

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
	mod_numknown = 0;
	mod_loaded = 0;
	mod_max = 0;
	models_known_max = MAX_MOD_KNOWN;
	models_known = NULL;

	Mod_Reallocate ();
}

/*
================
Mod_Free
================
*/
static void Mod_Free (model_t *mod)
{
	if (!mod->extradata)
	{
		// looks as empty model
		memset (mod, 0, sizeof(*mod));
		return;
	}

	if (vk_validation->value)
	{
		R_Printf(PRINT_ALL, "%s: Unload %s[%d]\n", __func__, mod->name, mod_loaded);
	}

	Hunk_Free (mod->extradata);
	memset (mod, 0, sizeof(*mod));
	mod_loaded --;
	if (mod_loaded < 0)
	{
		ri.Sys_Error (ERR_DROP, "%s: Broken unload", __func__);
	}
}

/*
================
Mod_FreeAll
================
*/
void Mod_FreeAll (void)
{
	int		i;

	for (i=0 ; i<mod_numknown ; i++)
	{
		if (models_known[i].extradatasize)
			Mod_Free (&models_known[i]);
	}
}

/*
================
Mod_FreeModelsKnown
================
*/
void Mod_FreeModelsKnown (void)
{
	free(models_known);
	models_known = NULL;
}

/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

/*
=================
Mod_LoadLighting
=================
*/
static void Mod_LoadLighting (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}
	loadmodel->lightdata = Hunk_Alloc ( l->filelen);
	memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVisibility
=================
*/
static void Mod_LoadVisibility (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	int		i;

	if (!l->filelen)
	{
		loadmodel->vis = NULL;
		return;
	}
	loadmodel->vis = Hunk_Alloc ( l->filelen);
	memcpy (loadmodel->vis, mod_base + l->fileofs, l->filelen);

	loadmodel->vis->numclusters = LittleLong (loadmodel->vis->numclusters);
	for (i=0 ; i<loadmodel->vis->numclusters ; i++)
	{
		loadmodel->vis->bitofs[i][0] = LittleLong (loadmodel->vis->bitofs[i][0]);
		loadmodel->vis->bitofs[i][1] = LittleLong (loadmodel->vis->bitofs[i][1]);
	}
}


/*
=================
Mod_LoadVertexes
=================
*/
static void Mod_LoadVertexes (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

/*
=================
RadiusFromBounds
=================
*/
static float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return VectorLength (corner);
}


/*
=================
Mod_LoadSubmodels
=================
*/
static void Mod_LoadSubmodels (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	dmodel_t	*in;
	model_t	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		if (i == 0)
		{
			// copy parent as template for first model
			memcpy(out, loadmodel, sizeof(*out));
		}
		else
		{
			// copy first as template for model
			memcpy(out, loadmodel->submodels, sizeof(*out));
		}

		Com_sprintf (out->name, sizeof(out->name), "*%d", i);

		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}

		out->radius = RadiusFromBounds (out->mins, out->maxs);
		out->firstnode = LittleLong (in->headnode);
		out->firstmodelsurface = LittleLong (in->firstface);
		out->nummodelsurfaces = LittleLong (in->numfaces);
		// visleafs
		out->numleafs = 0;
		//  check limits
		if (out->firstnode >= loadmodel->numnodes)
		{
			ri.Sys_Error(ERR_DROP, "%s: Inline model %i has bad firstnode",
					__func__, i);
		}
	}
}

/*
=================
Mod_LoadEdges
=================
*/
static void Mod_LoadEdges (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( (count + 1) * sizeof(*out));

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
static void Mod_LoadTexinfo (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out, *step;
	int 	i, j, count;
	char	name[MAX_QPATH];

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		int	next;

		for (j=0 ; j<4 ; j++)
		{
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);
			out->vecs[1][j] = LittleFloat (in->vecs[1][j]);
		}

		out->flags = LittleLong (in->flags);
		next = LittleLong (in->nexttexinfo);
		if (next > 0)
			out->next = loadmodel->texinfo + next;
		else
		    out->next = NULL;
		Com_sprintf (name, sizeof(name), "textures/%s.wal", in->texture);

		out->image = Vk_FindImage (name, it_wall);
		if (!out->image || out->image == r_notexture)
		{
			Com_sprintf (name, sizeof(name), "textures/%s.m8", in->texture);
			out->image = Vk_FindImage (name, it_wall);
		}

		if (!out->image)
		{
			R_Printf(PRINT_ALL, "Couldn't load %s\n", name);
			out->image = r_notexture;
		}
	}

	// count animation frames
	for (i=0 ; i<count ; i++)
	{
		out = &loadmodel->texinfo[i];
		out->numframes = 1;
		for (step = out->next ; step && step != out ; step=step->next)
			out->numframes++;
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
static void CalcSurfaceExtents (model_t *loadmodel, msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i=0 ; i<s->numedges ; i++)
	{
		int e, j;
		mvertex_t *v;

		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
	}
}

static int calcTexinfoAndFacesSize(const lump_t *fl, byte *mod_base, const lump_t *tl)
{
	dface_t* face_in = (void *)(mod_base + fl->fileofs);
	texinfo_t* texinfo_in = (void *)(mod_base + tl->fileofs);

	if (fl->filelen % sizeof(*face_in) || tl->filelen % sizeof(*texinfo_in))
	{
		// will error out when actually loading it
		return 0;
	}

	int ret = 0;

	int face_count = fl->filelen / sizeof(*face_in);
	int texinfo_count = tl->filelen / sizeof(*texinfo_in);

	{
		// out = Hunk_Alloc(count * sizeof(*out));
		int baseSize = face_count * sizeof(msurface_t);
		baseSize = (baseSize + 31) & ~31;
		ret += baseSize;

		int ti_size = texinfo_count * sizeof(mtexinfo_t);
		ti_size = (ti_size + 31) & ~31;
		ret += ti_size;
	}

	int numWarpFaces = 0;

	for (int surfnum = 0; surfnum < face_count; surfnum++, face_in++)
	{
		int numverts = LittleShort(face_in->numedges);
		int ti = LittleShort(face_in->texinfo);
		if ((ti < 0) || (ti >= texinfo_count))
		{
			return 0; // will error out
		}
		int texFlags = LittleLong(texinfo_in[ti].flags);

		/* set the drawing flags */
		if (texFlags & SURF_WARP)
		{
			if (numverts > 60)
				return 0; // will error out in R_SubdividePolygon()

			// Vk_SubdivideSurface(out, loadmodel); /* cut up polygon for warps */
			// for each (pot. recursive) call to R_SubdividePolygon():
			//   sizeof(vkpoly_t) + ((numverts - 4) + 2) * VERTEXSIZE*sizeof(float)

			// this is tricky, how much is allocated depends on the size of the surface
			// which we don't know (we'd need the vertices etc to know, but we can't load
			// those without allocating...)
			// so we just count warped faces and use a generous estimate below

			++numWarpFaces;
		}
		else
		{
			// Vk_BuildPolygonFromSurface(out);
			// => poly = Hunk_Alloc(sizeof(vkpoly_t) + (numverts - 4) * VERTEXSIZE*sizeof(float));
			int polySize = sizeof(vkpoly_t) + (numverts - 4) * VERTEXSIZE*sizeof(float);
			polySize = (polySize + 31) & ~31;
			ret += polySize;
		}
	}

	// yeah, this is a bit hacky, but it looks like for each warped face
	// 256-55000 bytes are allocated (usually on the lower end),
	// so just assume 48k per face to be safe
	ret += numWarpFaces * 49152;
	ret += 1000000; // and 1MB extra just in case

	return ret;
}

/*
=================
Mod_LoadFaces
=================
*/
static void Mod_LoadFaces (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	Vk_BeginBuildingLightmaps(loadmodel);

	for (surfnum = 0; surfnum<count; surfnum++, in++, out++)
	{
		int	side;
		int	ti;
		int	planenum;

		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		if (out->numedges < 3)
		{
			ri.Sys_Error(ERR_DROP, "%s: Surface with %d edges",
					__func__, out->numedges);
		}
		out->flags = 0;
		out->polys = NULL;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;

		ti = LittleShort(in->texinfo);
		if (ti < 0 || ti >= loadmodel->numtexinfo)
		{
			ri.Sys_Error(ERR_DROP, "%s: bad texinfo number", __func__);
		}
		out->texinfo = loadmodel->texinfo + ti;

		CalcSurfaceExtents(loadmodel, out);

		// lighting info

		for (i = 0; i<MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->lightdata + i;

		// set the drawing flags

		if (out->texinfo->flags & SURF_WARP)
		{
			out->flags |= SURF_DRAWTURB;
			for (i = 0; i<2; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			Vk_SubdivideSurface(out, loadmodel);	// cut up polygon for warps
		}

		if (r_fixsurfsky->value)
		{
			if (out->texinfo->flags & SURF_SKY)
			{
				out->flags |= SURF_DRAWSKY;
			}
		}

		// create lightmaps and polygons
		if (!(out->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)))
			Vk_CreateSurfaceLightmap(out);

		if (!(out->texinfo->flags & SURF_WARP))
			Vk_BuildPolygonFromSurface(out, loadmodel);

	}

	Vk_EndBuildingLightmaps();
}


/*
=================
Mod_SetParent
=================
*/
static void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents != -1)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
static void Mod_LoadNodes (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	int			i, j, count;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		int	p;

		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);
		out->contents = -1;	// differentiate from leafs

		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
static void Mod_LoadLeafs (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		unsigned firstleafface;

		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		out->contents = LittleLong(in->contents);
		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		// make unsigned long from signed short
		firstleafface = LittleShort(in->firstleafface) & 0xFFFF;
		out->nummarksurfaces = LittleShort(in->numleaffaces) & 0xFFFF;

		out->firstmarksurface = loadmodel->marksurfaces + firstleafface;
		if ((firstleafface + out->nummarksurfaces) > loadmodel->nummarksurfaces)
		{
			ri.Sys_Error(ERR_DROP, "%s: wrong marksurfaces position in %s",
				__func__, loadmodel->name);
		}

		// gl underwater warp
#if 0
		if (out->contents & (CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA|CONTENTS_THINWATER) )
		{
			for (j=0 ; j<out->nummarksurfaces ; j++)
			{
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
				for (poly = out->firstmarksurface[j]->polys ; poly ; poly=poly->next)
					poly->flags |= SURF_UNDERWATER;
			}
		}
#endif
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
static void Mod_LoadMarksurfaces (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	int		i, count;
	short		*in;
	msurface_t **out;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		int	j;

		j = LittleShort(in[i]);
		if (j < 0 ||  j >= loadmodel->numsurfaces)
		{
			ri.Sys_Error(ERR_DROP, "%s: bad surface number", __func__);
		}
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
static void Mod_LoadSurfedges (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	int		i, count;
	int		*in, *out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		ri.Sys_Error (ERR_DROP, "%s: bad surfedges count in %s: %i",
		__func__, loadmodel->name, count);

	out = Hunk_Alloc ( count*sizeof(*out));

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
static void Mod_LoadPlanes (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	int			i;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*2*sizeof(*out));

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		int bits, j;

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


// calculate the size that Hunk_Alloc(), called by Mod_Load*() from Mod_LoadBrushModel(),
// will use (=> includes its padding), so we'll know how big the hunk needs to be
static int calcLumpHunkSize(const lump_t *l, int inSize, int outSize)
{
	if (l->filelen % inSize)
	{
		// Mod_Load*() will error out on this because of "funny size"
		// don't error out here because in Mod_Load*() it can print the functionname
		// (=> tells us what kind of lump) before shutting down the game
		return 0;
	}

	int count = l->filelen / inSize;
	int size = count * outSize;

	// round to cacheline, like Hunk_Alloc() does
	size = (size + 31) & ~31;
	return size;
}

/*
=================
Mod_LoadBrushModel
=================
*/
static void Mod_LoadBrushModel (model_t *loadmodel, void *buffer, int modfilelen)
{
	int			i;
	dheader_t	*header;
	byte		*mod_base;

	header = (dheader_t *)buffer;

	i = LittleLong(header->version);

	if (i != BSPVERSION)
	{
		ri.Sys_Error(ERR_DROP, "%s: %s has wrong version number (%i should be %i)",
				__func__, loadmodel->name, i, BSPVERSION);
	}

	// swap all the lumps
	mod_base = (byte *)header;

	for (i = 0; i < sizeof(dheader_t) / 4; i++)
	{
		((int *)header)[i] = LittleLong(((int *)header)[i]);
	}

	// calculate the needed hunksize from the lumps
	int hunkSize = 0;
	hunkSize += calcLumpHunkSize(&header->lumps[LUMP_VERTEXES], sizeof(dvertex_t), sizeof(mvertex_t));
	hunkSize += calcLumpHunkSize(&header->lumps[LUMP_EDGES], sizeof(dedge_t), sizeof(medge_t));
	hunkSize += sizeof(medge_t) + 31; // for count+1 in Mod_LoadEdges()
	int surfEdgeCount = (header->lumps[LUMP_SURFEDGES].filelen+sizeof(int)-1)/sizeof(int);
	if(surfEdgeCount < MAX_MAP_SURFEDGES) // else it errors out later anyway
		hunkSize += calcLumpHunkSize(&header->lumps[LUMP_SURFEDGES], sizeof(int), sizeof(int));
	hunkSize += calcLumpHunkSize(&header->lumps[LUMP_LIGHTING], 1, 1);
	hunkSize += calcLumpHunkSize(&header->lumps[LUMP_PLANES], sizeof(dplane_t), sizeof(cplane_t)*2);
	hunkSize += calcTexinfoAndFacesSize(&header->lumps[LUMP_FACES], mod_base, &header->lumps[LUMP_TEXINFO]);
	hunkSize += calcLumpHunkSize(&header->lumps[LUMP_LEAFFACES], sizeof(short), sizeof(msurface_t *)); // yes, out is indeeed a pointer!
	hunkSize += calcLumpHunkSize(&header->lumps[LUMP_VISIBILITY], 1, 1);
	hunkSize += calcLumpHunkSize(&header->lumps[LUMP_LEAFS], sizeof(dleaf_t), sizeof(mleaf_t));
	hunkSize += calcLumpHunkSize(&header->lumps[LUMP_NODES], sizeof(dnode_t), sizeof(mnode_t));
	hunkSize += calcLumpHunkSize(&header->lumps[LUMP_MODELS], sizeof(dmodel_t), sizeof(model_t));

	loadmodel->extradata = Hunk_Begin(hunkSize);
	loadmodel->type = mod_brush;
	loadmodel->numframes = 2;		// regular and alternate animation

	// load into heap
	Mod_LoadVertexes (loadmodel, mod_base, &header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (loadmodel, mod_base, &header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (loadmodel, mod_base, &header->lumps[LUMP_SURFEDGES]);
	Mod_LoadLighting (loadmodel, mod_base, &header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (loadmodel, mod_base, &header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (loadmodel, mod_base, &header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (loadmodel, mod_base, &header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (loadmodel, mod_base, &header->lumps[LUMP_LEAFFACES]);
	Mod_LoadVisibility (loadmodel, mod_base, &header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (loadmodel, mod_base, &header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (loadmodel, mod_base, &header->lumps[LUMP_NODES]);
	Mod_LoadSubmodels (loadmodel, mod_base, &header->lumps[LUMP_MODELS]);
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

/*
=================
Mod_LoadAliasModel
=================
*/
static void Mod_LoadAliasModel (model_t *mod, void *buffer, int modfilelen)
{
	int		i, j;
	dmdl_t		*pinmodel, *pheader;
	dstvert_t	*pinst, *poutst;
	dtriangle_t	*pintri, *pouttri;
	int		*pincmd, *poutcmd;
	int		version;
	int		ofs_end;

	pinmodel = (dmdl_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
	{
		ri.Sys_Error(ERR_DROP, "%s: %s has wrong version number (%i should be %i)",
				__func__, mod->name, version, ALIAS_VERSION);
	}

	ofs_end = LittleLong(pinmodel->ofs_end);
	if (ofs_end < 0 || ofs_end > modfilelen)
	{
		ri.Sys_Error(ERR_DROP, "%s: model %s file size(%d) too small, should be %d",
				__func__, mod->name, modfilelen, ofs_end);
	}

	mod->extradata = Hunk_Begin(modfilelen);
	pheader = Hunk_Alloc(ofs_end);

	// byte swap the header fields and sanity check
	for (i=0 ; i<sizeof(dmdl_t)/4 ; i++)
		((int *)pheader)[i] = LittleLong (((int *)buffer)[i]);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
	{
		ri.Sys_Error(ERR_DROP, "%s: model %s has a skin taller than %d",
				__func__, mod->name, MAX_LBM_HEIGHT);
	}

	if (pheader->num_xyz <= 0)
	{
		ri.Sys_Error(ERR_DROP, "%s: model %s has no vertices",
				__func__, mod->name);
	}

	if (pheader->num_xyz > MAX_VERTS)
	{
		ri.Sys_Error(ERR_DROP, "%s: model %s has too many vertices",
				__func__, mod->name);
	}

	if (pheader->num_st <= 0)
	{
		ri.Sys_Error(ERR_DROP, "%s: model %s has no st vertices",
				__func__, mod->name);
	}

	if (pheader->num_tris <= 0)
	{
		ri.Sys_Error(ERR_DROP, "%s: model %s has no triangles",
				__func__, mod->name);
	}

	if (pheader->num_frames <= 0)
	{
		ri.Sys_Error(ERR_DROP, "%s: model %s has no frames",
				__func__, mod->name);
	}

	//
	// load base s and t vertices (not used in gl version)
	//
	pinst = (dstvert_t *) ((byte *)pinmodel + pheader->ofs_st);
	poutst = (dstvert_t *) ((byte *)pheader + pheader->ofs_st);

	for (i=0 ; i<pheader->num_st ; i++)
	{
		poutst[i].s = LittleShort (pinst[i].s);
		poutst[i].t = LittleShort (pinst[i].t);
	}

	//
	// load triangle lists
	//
	pintri = (dtriangle_t *) ((byte *)pinmodel + pheader->ofs_tris);
	pouttri = (dtriangle_t *) ((byte *)pheader + pheader->ofs_tris);

	for (i=0 ; i<pheader->num_tris ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			pouttri[i].index_xyz[j] = LittleShort (pintri[i].index_xyz[j]);
			pouttri[i].index_st[j] = LittleShort (pintri[i].index_st[j]);
		}
	}

	//
	// load the frames
	//
	for (i=0 ; i<pheader->num_frames ; i++)
	{
		daliasframe_t		*pinframe, *poutframe;

		pinframe = (daliasframe_t *) ((byte *)pinmodel
			+ pheader->ofs_frames + i * pheader->framesize);
		poutframe = (daliasframe_t *) ((byte *)pheader
			+ pheader->ofs_frames + i * pheader->framesize);

		memcpy (poutframe->name, pinframe->name, sizeof(poutframe->name));
		for (j=0 ; j<3 ; j++)
		{
			poutframe->scale[j] = LittleFloat (pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat (pinframe->translate[j]);
		}
		// verts are all 8 bit, so no swapping needed
		memcpy (poutframe->verts, pinframe->verts,
			pheader->num_xyz*sizeof(dtrivertx_t));

	}

	mod->type = mod_alias;

	//
	// load the glcmds
	//
	pincmd = (int *) ((byte *)pinmodel + pheader->ofs_glcmds);
	poutcmd = (int *) ((byte *)pheader + pheader->ofs_glcmds);
	for (i=0; i < pheader->num_glcmds; i++)
	{
		poutcmd[i] = LittleLong (pincmd[i]);
	}

	if (poutcmd[pheader->num_glcmds-1] != 0)
	{
		R_Printf(PRINT_ALL, "%s: Entity %s has possible last element issues with %d verts.\n",
			__func__,
			mod->name,
			poutcmd[pheader->num_glcmds-1]);
	}

	// register all skins
	memcpy ((char *)pheader + pheader->ofs_skins, (char *)pinmodel + pheader->ofs_skins,
		pheader->num_skins*MAX_SKINNAME);
	for (i=0 ; i<pheader->num_skins ; i++)
	{
		mod->skins[i] = Vk_FindImage ((char *)pheader + pheader->ofs_skins + i*MAX_SKINNAME
			, it_skin);
	}

	mod->mins[0] = -32;
	mod->mins[1] = -32;
	mod->mins[2] = -32;
	mod->maxs[0] = 32;
	mod->maxs[1] = 32;
	mod->maxs[2] = 32;
}

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/

/*
=================
Mod_LoadSpriteModel
=================
*/
static void Mod_LoadSpriteModel (model_t *mod, void *buffer, int modfilelen)
{
	dsprite_t	*sprin, *sprout;
	int			i;

	sprin = (dsprite_t *)buffer;
	mod->extradata = Hunk_Begin(modfilelen);
	sprout = Hunk_Alloc(modfilelen);

	sprout->ident = LittleLong (sprin->ident);
	sprout->version = LittleLong (sprin->version);
	sprout->numframes = LittleLong (sprin->numframes);

	if (sprout->version != SPRITE_VERSION)
	{
		ri.Sys_Error(ERR_DROP, "%s: %s has wrong version number (%i should be %i)",
				__func__, mod->name, sprout->version, SPRITE_VERSION);
	}

	if (sprout->numframes > MAX_MD2SKINS)
	{
		ri.Sys_Error(ERR_DROP, "%s: %s has too many frames (%i > %i)",
				__func__, mod->name, sprout->numframes, MAX_MD2SKINS);
	}

	// byte swap everything
	for (i=0 ; i<sprout->numframes ; i++)
	{
		sprout->frames[i].width = LittleLong (sprin->frames[i].width);
		sprout->frames[i].height = LittleLong (sprin->frames[i].height);
		sprout->frames[i].origin_x = LittleLong (sprin->frames[i].origin_x);
		sprout->frames[i].origin_y = LittleLong (sprin->frames[i].origin_y);
		memcpy (sprout->frames[i].name, sprin->frames[i].name, MAX_SKINNAME);
		mod->skins[i] = Vk_FindImage (sprout->frames[i].name,
			it_sprite);
	}

	mod->type = mod_sprite;
}

//=============================================================================

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
static model_t *Mod_ForName (char *name, model_t *parent_model, qboolean crash)
{
	model_t	*mod;
	unsigned *buf;
	int		i, modfilelen;

	if (!name[0])
	{
		ri.Sys_Error(ERR_DROP, "%s: NULL name", __func__);
	}

	//
	// inline models are grabbed only from worldmodel
	//
	if (name[0] == '*' && parent_model)
	{
		i = atoi(name+1);
		if (i < 1 || i >= parent_model->numsubmodels)
			ri.Sys_Error (ERR_DROP, "bad inline model number");
		return &parent_model->submodels[i];
	}

	//
	// search the currently loaded models
	//
	for (i=0 , mod=models_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		if (!strcmp (mod->name, name) )
			return mod;
	}

	//
	// find a free model slot spot
	//
	for (i=0 , mod=models_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			break;	// free spot
	}
	if (i == mod_numknown)
	{
		if (mod_numknown == models_known_max)
			ri.Sys_Error(ERR_DROP, "%s: mod_numknown == models_known_max", __func__);
		mod_numknown++;
	}
	strcpy (mod->name, name);

	//
	// load the file
	//
	modfilelen = ri.FS_LoadFile (mod->name, (void **)&buf);
	if (!buf)
	{
		if (crash)
		{
			ri.Sys_Error(ERR_DROP, "%s: %s not found",
					__func__, mod->name);
		}

		if (vk_validation->value)
		{
			R_Printf(PRINT_ALL, "%s: Can't load %s\n", __func__, mod->name);
		}
		memset (mod->name, 0, sizeof(mod->name));
		return NULL;
	}

	// update count of loaded models
	mod_loaded ++;
	if (vk_validation->value)
	{
		R_Printf(PRINT_ALL, "%s: Load %s[%d]\n", __func__, mod->name, mod_loaded);
	}

	//
	// fill it in
	//

	// call the apropriate loader

	switch (LittleLong(*(unsigned *)buf))
	{
	case IDALIASHEADER:
		Mod_LoadAliasModel(mod, buf, modfilelen);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel(mod, buf, modfilelen);
		break;

	case IDBSPHEADER:
		Mod_LoadBrushModel(mod, buf, modfilelen);
		break;

	default:
		ri.Sys_Error(ERR_DROP, "%s: unknown fileid for %s",
				__func__, mod->name);
		break;
	}

	mod->extradatasize = Hunk_End ();

	ri.FS_FreeFile(buf);

	return mod;
}

/*
=====================
RE_BeginRegistration

Specifies the model that will be used as the world
=====================
*/
void
RE_BeginRegistration (char *model)
{
	char	fullname[MAX_QPATH];
	cvar_t	*flushmap;

	Mod_Reallocate();

	registration_sequence++;
	r_oldviewcluster = -1;		// force markleafs

	Com_sprintf (fullname, sizeof(fullname), "maps/%s.bsp", model);

	// explicitly free the old map if different
	// this guarantees that models_known[0] is the world map
	flushmap = ri.Cvar_Get ("flushmap", "0", 0);
	if ( strcmp(models_known[0].name, fullname) || flushmap->value)
		Mod_Free (&models_known[0]);
	r_worldmodel = Mod_ForName(fullname, NULL, true);
	if (r_worldmodel != models_known)
		ri.Sys_Error(ERR_DROP, "%s: Loaded a brush model after the world", __func__);

	r_viewcluster = -1;
}

/*
=====================
RE_RegisterModel

=====================
*/
struct model_s *RE_RegisterModel (char *name)
{
	model_t	*mod;

	mod = Mod_ForName (name, r_worldmodel, false);
	if (mod)
	{
		int	i;

		mod->registration_sequence = registration_sequence;

		// register any images used by the models
		if (mod->type == mod_sprite)
		{
			dsprite_t	*sprout;

			sprout = (dsprite_t *)mod->extradata;
			for (i=0 ; i<sprout->numframes ; i++)
				mod->skins[i] = Vk_FindImage (sprout->frames[i].name, it_sprite);
		}
		else if (mod->type == mod_alias)
		{
			dmdl_t *pheader;

			pheader = (dmdl_t *)mod->extradata;
			for (i=0 ; i<pheader->num_skins ; i++)
				mod->skins[i] = Vk_FindImage ((char *)pheader + pheader->ofs_skins + i*MAX_SKINNAME, it_skin);
//PGM
			mod->numframes = pheader->num_frames;
//PGM
		}
		else if (mod->type == mod_brush)
		{
			for (i=0 ; i<mod->numtexinfo ; i++)
				mod->texinfo[i].image->registration_sequence = registration_sequence;
		}
	}
	return mod;
}

static qboolean
Mod_HasFreeSpace(void)
{
	int		i, used;
	model_t	*mod;

	used = 0;

	for (i=0, mod=models_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		if (mod->registration_sequence == registration_sequence)
		{
			used ++;
		}
	}

	if (mod_max < used)
	{
		mod_max = used;
	}

	// should same size of free slots as currently used
	return (mod_loaded + mod_max) < models_known_max;
}

/*
================
Mod_Modellist_f
================
*/
void Mod_Modellist_f (void)
{
	int		i, total, used;
	model_t	*mod;
	qboolean	freeup;

	total = 0;
	used = 0;

	R_Printf(PRINT_ALL,"Loaded models:\n");
	for (i=0, mod=models_known ; i < mod_numknown ; i++, mod++)
	{
		char *in_use = "";

		if (mod->registration_sequence == registration_sequence)
		{
			in_use = "*";
			used ++;
		}

		if (!mod->name[0])
			continue;
		R_Printf(PRINT_ALL, "%8i : %s %s\n",
			 mod->extradatasize, mod->name, in_use);
		total += mod->extradatasize;
	}
	R_Printf(PRINT_ALL, "Total resident: %i in %d models\n", total, mod_loaded);
	// update statistics
	freeup = Mod_HasFreeSpace();
	R_Printf(PRINT_ALL, "Used %d of %d models%s.\n", used, mod_max, freeup ? ", has free space" : "");
}

/*
=====================
RE_EndRegistration

=====================
*/
void RE_EndRegistration (void)
{
	int		i;
	model_t	*mod;

	if (Mod_HasFreeSpace() && Vk_ImageHasFreeSpace())
	{
		// should be enough space for load next maps
		return;
	}

	for (i=0, mod=models_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		if (mod->registration_sequence != registration_sequence)
		{	// don't need this model
			Mod_Free (mod);
		}
	}

	Vk_FreeUnusedImages ();
}
