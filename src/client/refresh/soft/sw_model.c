/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// models.c -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include <limits.h>
#include "header/local.h"

static void Mod_LoadBrushModel(model_t *mod, void *buffer, int modfilelen);

static YQ2_ALIGNAS_TYPE(int) byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
static model_t	mod_known[MAX_MOD_KNOWN];
static int	mod_numknown;
static int	mod_max = 0;

int	registration_sequence;

//===============================================================================

static qboolean
Mod_HasFreeSpace(void)
{
	int		i, used;
	model_t	*mod;

	used = 0;

	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
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
	return (mod_numknown + mod_max) < MAX_MOD_KNOWN;
}

/*
================
Mod_Modellist_f
================
*/
void
Mod_Modellist_f (void)
{
	int		i, total, used;
	model_t	*mod;
	qboolean	freeup;

	total = 0;
	used = 0;

	Com_Printf("Loaded models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		char *in_use = "";

		if (mod->registration_sequence == registration_sequence)
		{
			in_use = "*";
			used ++;
		}

		if (!mod->name[0])
			continue;
		Com_Printf("%8i : %s %s\n",
			 mod->extradatasize, mod->name, in_use);
		total += mod->extradatasize;
	}
	Com_Printf("Total resident: %i\n", total);
	// update statistics
	freeup = Mod_HasFreeSpace();
	Com_Printf("Used %d of %d models%s.\n", used, mod_max, freeup ? ", has free space" : "");
}

/*
===============
Mod_Init
===============
*/
void
Mod_Init (void)
{
	mod_max = 0;
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
static model_t *
Mod_ForName(const char *name, model_t *parent_model, qboolean crash)
{
	model_t	*mod;
	void	*buf;
	int	i, modfilelen;

	if (!name[0])
	{
		Com_Error(ERR_DROP, "%s: NULL name", __func__);
	}

	//
	// inline models are grabbed only from worldmodel
	//
	if (name[0] == '*' && parent_model)
	{
		i = atoi(name+1);
		if (i < 1 || i >= parent_model->numsubmodels)
		{
			Com_Error(ERR_DROP, "%s: bad inline model number",
					__func__);
		}

		return &parent_model->submodels[i];
	}

	//
	// search the currently loaded models
	//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (!strcmp (mod->name, name) )
			return mod;

	//
	// find a free model slot spot
	//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			break;	// free spot
	}
	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Com_Error(ERR_DROP, "%s: mod_numknown == MAX_MOD_KNOWN", __func__);
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
			Com_Error(ERR_DROP, "%s: %s not found",
					__func__, mod->name);
		}

		memset (mod->name, 0, sizeof(mod->name));
		return NULL;
	}

	//
	// fill it in
	//

	// call the apropriate loader

	switch (LittleLong(*(unsigned *)buf))
	{
	case IDALIASHEADER:
		{
			mod->extradata = Mod_LoadMD2(mod->name, buf, modfilelen,
				mod->mins, mod->maxs,
				(struct image_s **)mod->skins, (findimage_t)R_FindImage,
				&(mod->type));
			if (!mod->extradata)
			{
				Com_Error(ERR_DROP, "%s: Failed to load %s",
					__func__, mod->name);
			}
		};
		break;

	case IDSPRITEHEADER:
		{
			mod->extradata = Mod_LoadSP2(mod->name, buf, modfilelen,
				(struct image_s **)mod->skins, (findimage_t)R_FindImage,
				&(mod->type));
			if (!mod->extradata)
			{
				Com_Error(ERR_DROP, "%s: Failed to load %s",
					__func__, mod->name);
			}
		}
		break;

	case IDBSPHEADER:
		Mod_LoadBrushModel(mod, buf, modfilelen);
		break;

	default:
		Com_Error(ERR_DROP, "%s: unknown fileid for %s",
				__func__, mod->name);
		break;
	}

	mod->extradatasize = Hunk_End();

	ri.FS_FreeFile(buf);

	return mod;
}

/*
==============
Mod_ClusterPVS
==============
*/
const byte *
Mod_ClusterPVS (int cluster, const model_t *model)
{
	if (cluster == -1 || !model->vis)
		return mod_novis;
	return Mod_DecompressVis ( (byte *)model->vis +
		model->vis->bitofs[cluster][DVIS_PVS],
		(model->vis->numclusters+7)>>3);
}

/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

/*
=================
Mod_LoadSubmodels
=================
*/
static void
Mod_LoadSubmodels (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	dmodel_t	*in;
	model_t	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count*sizeof(*out));

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		if (i == 0)
		{
			/* copy parent as template for first model */
			memcpy(out, loadmodel, sizeof(*out));
		}
		else
		{
			/* copy first as template for model */
			memmove(out, loadmodel->submodels, sizeof(*out));
		}

		Com_sprintf (out->name, sizeof(out->name), "*%d", i);

		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}

		out->radius = Mod_RadiusFromBounds (out->mins, out->maxs);
		out->firstnode = LittleLong (in->headnode);
		out->firstmodelsurface = LittleLong (in->firstface);
		out->nummodelsurfaces = LittleLong (in->numfaces);
		// visleafs
		out->numleafs = 0;
		//  check limits
		if (out->firstnode >= loadmodel->numnodes)
		{
			Com_Error(ERR_DROP, "%s: Inline model %i has bad firstnode",
					__func__, i);
		}
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
static void
CalcSurfaceExtents (model_t *loadmodel, msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = (float)INT_MAX; // Set maximum values for world range
	maxs[0] = maxs[1] = (float)INT_MIN; // Set minimal values for world range

	tex = s->texinfo;

	for (i=0 ; i<s->numedges ; i++)
	{
		int e, j;
		mvertex_t *v;

		e = loadmodel->surfedges[s->firstedge+i];

		if (e >= 0)
		{
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		}
		else
		{
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		}
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
		if (s->extents[i] < 16)
			s->extents[i] = 16;	// take at least one cache block
		if ( !(tex->flags & (SURF_WARP|SURF_SKY)) && s->extents[i] > 256)
		{
			Com_Error(ERR_DROP, "%s: Bad surface extents", __func__);
		}
	}
}


/*
=================
Mod_LoadFaces
=================
*/
static void
Mod_LoadFaces (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc((count+6)*sizeof(*out));	// extra for skybox

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		int planenum, side, ti;

		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		if (out->numedges < 3)
		{
			Com_Error(ERR_DROP, "%s: Surface with %d edges",
					__func__, out->numedges);
		}
		out->flags = 0;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		if (planenum < 0 || planenum >= loadmodel->numplanes)
		{
			Com_Error(ERR_DROP, "%s: Incorrect %d planenum.",
					__func__, planenum);
		}
		out->plane = loadmodel->planes + planenum;

		ti = LittleShort(in->texinfo);
		if (ti < 0 || ti >= loadmodel->numtexinfo)
		{
			Com_Error(ERR_DROP, "%s: bad texinfo number", __func__);
		}
		out->texinfo = loadmodel->texinfo + ti;

		CalcSurfaceExtents (loadmodel, out);

		// lighting is saved as its with 24 bit color
		for (i=0 ; i<MAXLIGHTMAPS ; i++)
		{
			out->styles[i] = in->styles[i];
		}

		i = LittleLong(in->lightofs);

		if (i == -1)
		{
			out->samples = NULL;
		}
		else
		{
			out->samples = loadmodel->lightdata + i;
		}

		// set the drawing flags flag

		if (!out->texinfo->image)
			continue;

		if (r_fixsurfsky->value)
		{
			if (out->texinfo->flags & SURF_SKY)
			{
				out->flags |= SURF_DRAWSKY;
				continue;
			}
		}

		if (out->texinfo->flags & SURF_WARP)
		{
			out->flags |= SURF_DRAWTURB;
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			continue;
		}

		//==============
		// this marks flowing surfaces as turbulent.
		if (out->texinfo->flags & SURF_FLOWING)
		{
			out->flags |= SURF_DRAWTURB;
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			continue;
		}
		//==============
	}
}

/*
=================
Mod_LoadLeafs
=================
*/
static void
Mod_LoadLeafs (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count*sizeof(*out));

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
			Com_Error(ERR_DROP, "%s: wrong marksurfaces position in %s",
				__func__, loadmodel->name);
		}
	}
}


/*
=================
Mod_LoadMarksurfaces
=================
*/
static void
Mod_LoadMarksurfaces (model_t *loadmodel, byte *mod_base, lump_t *l)
{
	int		i, count;
	short		*in;
	msurface_t **out;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		Com_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count*sizeof(*out));

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		int j;
		j = LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
		{
			Com_Error(ERR_DROP, "%s: bad surface number", __func__);
		}
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadBrushModel
=================
*/
static void
Mod_LoadBrushModel(model_t *mod, void *buffer, int modfilelen)
{
	int		i;
	dheader_t	*header;
	byte	*mod_base;

	if (mod != mod_known)
		Com_Error(ERR_DROP, "%s: Loaded a brush model after the world", __func__);

	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	if (i != BSPVERSION)
	{
		Com_Error(ERR_DROP, "%s: %s has wrong version number (%i should be %i)",
				__func__, mod->name, i, BSPVERSION);
	}

	// swap all the lumps
	mod_base = (byte *)header;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

	// calculate the needed hunksize from the lumps
	int hunkSize = 0;
	hunkSize += Mod_CalcLumpHunkSize(&header->lumps[LUMP_VERTEXES], sizeof(dvertex_t), sizeof(mvertex_t), 8);
	hunkSize += Mod_CalcLumpHunkSize(&header->lumps[LUMP_EDGES], sizeof(dedge_t), sizeof(medge_t), 13);
	float surfEdgeCount = (float)header->lumps[LUMP_SURFEDGES].filelen / sizeof(int);
	if(surfEdgeCount < MAX_MAP_SURFEDGES) // else it errors out later anyway
		hunkSize += Mod_CalcLumpHunkSize(&header->lumps[LUMP_SURFEDGES], sizeof(int), sizeof(int), 24);

	// lighting is a special case, because we keep only 1 byte out of 3
	// (=> no colored lighting in soft renderer by default)
	{
		int size = header->lumps[LUMP_LIGHTING].filelen;
		size = (size + 31) & ~31;
		/* save color data */
		hunkSize += size;
	}

	hunkSize += Mod_CalcLumpHunkSize(&header->lumps[LUMP_PLANES], sizeof(dplane_t), sizeof(cplane_t)*2, 6);
	hunkSize += Mod_CalcLumpHunkSize(&header->lumps[LUMP_TEXINFO], sizeof(texinfo_t), sizeof(mtexinfo_t), 6);
	hunkSize += Mod_CalcLumpHunkSize(&header->lumps[LUMP_FACES], sizeof(dface_t), sizeof(msurface_t), 6);
	hunkSize += Mod_CalcLumpHunkSize(&header->lumps[LUMP_LEAFFACES], sizeof(short), sizeof(msurface_t *), 0); // yes, out is indeed a pointer!
	hunkSize += Mod_CalcLumpHunkSize(&header->lumps[LUMP_VISIBILITY], 1, 1, 0);
	hunkSize += Mod_CalcLumpHunkSize(&header->lumps[LUMP_LEAFS], sizeof(dleaf_t), sizeof(mleaf_t), 0);
	hunkSize += Mod_CalcLumpHunkSize(&header->lumps[LUMP_NODES], sizeof(dnode_t), sizeof(mnode_t), 0);
	hunkSize += Mod_CalcLumpHunkSize(&header->lumps[LUMP_MODELS], sizeof(dmodel_t), sizeof(model_t), 0);

	hunkSize += 1048576; // 1MB extra just in case

	mod->extradata = Hunk_Begin(hunkSize);

	mod->type = mod_brush;

	// load into heap
	Mod_LoadVertexes (mod->name, &mod->vertexes, &mod->numvertexes, mod_base,
		&header->lumps[LUMP_VERTEXES], 8);
	Mod_LoadEdges (mod->name, &mod->edges, &mod->numedges,
		mod_base, &header->lumps[LUMP_EDGES], 13);
	Mod_LoadSurfedges (mod->name, &mod->surfedges, &mod->numsurfedges,
		mod_base, &header->lumps[LUMP_SURFEDGES], 24);
	Mod_LoadLighting (&mod->lightdata, mod_base, &header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (mod->name, &mod->planes, &mod->numplanes,
		mod_base, &header->lumps[LUMP_PLANES], 6);
	Mod_LoadTexinfo (mod->name, &mod->texinfo, &mod->numtexinfo,
		mod_base, &header->lumps[LUMP_TEXINFO], (findimage_t)R_FindImage,
		r_notexture_mip, 6);
	Mod_LoadFaces (mod, mod_base, &header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (mod, mod_base, &header->lumps[LUMP_LEAFFACES]);
	Mod_LoadVisibility (&mod->vis, mod_base, &header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (mod, mod_base, &header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (mod->name, mod->planes, mod->numplanes, mod->leafs,
		mod->numleafs, &mod->nodes, &mod->numnodes, mod_base,
		&header->lumps[LUMP_NODES]);
	Mod_LoadSubmodels (mod, mod_base, &header->lumps[LUMP_MODELS]);

	R_InitSkyBox (mod);
}

//=============================================================================

/*
=====================
RE_BeginRegistration

Specifies the model that will be used as the world
=====================
*/
void
RE_BeginRegistration(const char *model)
{
	char	fullname[MAX_QPATH];
	cvar_t	*flushmap;

	registration_sequence++;
	r_oldviewcluster = -1;		// force markleafs
	Com_sprintf (fullname, sizeof(fullname), "maps/%s.bsp", model);

	D_FlushCaches ();
	// explicitly free the old map if different
	// this guarantees that mod_known[0] is the world map
	flushmap = ri.Cvar_Get ("flushmap", "0", 0);
	if ( strcmp(mod_known[0].name, fullname) || flushmap->value)
		Mod_Free (&mod_known[0]);
	r_worldmodel = RE_RegisterModel (fullname);
	R_NewMap ();
}


/*
=====================
RE_RegisterModel

=====================
*/
struct model_s *
RE_RegisterModel (const char *name)
{
	model_t	*mod;

	mod = Mod_ForName(name, r_worldmodel, false);
	if (mod)
	{
		mod->registration_sequence = registration_sequence;

		// register any images used by the models
		if (mod->type == mod_brush)
		{
			int i;

			for (i=0 ; i<mod->numtexinfo ; i++)
				mod->texinfo[i].image->registration_sequence = registration_sequence;
		}
		else
		{
			/* numframes is unused for SP2 but lets set it also  */
			mod->numframes = Mod_ReLoadSkins((struct image_s **)mod->skins,
				(findimage_t)R_FindImage, mod->extradata, mod->type);
		}
	}
	return mod;
}

/*
=====================
RE_EndRegistration

=====================
*/
void
RE_EndRegistration (void)
{
	int	i;
	model_t	*mod;

	if (Mod_HasFreeSpace() && R_ImageHasFreeSpace())
	{
		// should be enough space for load next maps
		return;
	}

	for (i=0, mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		if (mod->registration_sequence != registration_sequence)
		{
			// don't need this model
			Mod_Free (mod);
		}
	}

	R_FreeUnusedImages ();
}


//=============================================================================

/*
================
Mod_Free
================
*/
void
Mod_Free (model_t *mod)
{
	Hunk_Free (mod->extradata);
	memset (mod, 0, sizeof(*mod));
}

/*
================
Mod_FreeAll
================
*/
void
Mod_FreeAll (void)
{
	int		i;

	for (i=0 ; i<mod_numknown ; i++)
	{
		if (mod_known[i].extradatasize)
			Mod_Free (&mod_known[i]);
	}
}
