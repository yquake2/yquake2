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
 * Model loading and caching. Includes the .bsp file format
 *
 * =======================================================================
 */

#include "header/local.h"

#define MAX_MOD_KNOWN 512

model_t *loadmodel;
int modfilelen;
byte mod_novis[MAX_MAP_LEAFS / 8];
model_t mod_known[MAX_MOD_KNOWN];
int mod_numknown;
int registration_sequence;
byte *mod_base;

void LoadSP2(model_t *mod, void *buffer);
void Mod_LoadBrushModel(model_t *mod, void *buffer);
void LoadMD2(model_t *mod, void *buffer);
model_t *Mod_LoadModel(model_t *mod, qboolean crash);
void LM_BuildPolygonFromSurface(msurface_t *fa);
void LM_CreateSurfaceLightmap(msurface_t *surf);
void LM_EndBuildingLightmaps(void);
void LM_BeginBuildingLightmaps(model_t *m);

/* the inline * models from the current map are kept seperate */
model_t mod_inline[MAX_MOD_KNOWN];

mleaf_t *
Mod_PointInLeaf(vec3_t p, model_t *model)
{
	mnode_t *node;
	float d;
	cplane_t *plane;

	if (!model || !model->nodes)
	{
		VID_Error(ERR_DROP, "Mod_PointInLeaf: bad model");
	}

	node = model->nodes;

	while (1)
	{
		if (node->contents != -1)
		{
			return (mleaf_t *)node;
		}

		plane = node->plane;
		d = DotProduct(p, plane->normal) - plane->dist;

		if (d > 0)
		{
			node = node->children[0];
		}
		else
		{
			node = node->children[1];
		}
	}

	return NULL; /* never reached */
}

byte *
Mod_DecompressVis(byte *in, model_t *model)
{
	static byte decompressed[MAX_MAP_LEAFS / 8];
	int c;
	byte *out;
	int row;

	row = (model->vis->numclusters + 7) >> 3;
	out = decompressed;

	if (!in)
	{
		/* no vis info, so make all visible */
		while (row)
		{
			*out++ = 0xff;
			row--;
		}

		return decompressed;
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;

		while (c)
		{
			*out++ = 0;
			c--;
		}
	}
	while (out - decompressed < row);

	return decompressed;
}

byte *
Mod_ClusterPVS(int cluster, model_t *model)
{
	if ((cluster == -1) || !model->vis)
	{
		return mod_novis;
	}

	return Mod_DecompressVis((byte *)model->vis +
			model->vis->bitofs[cluster][DVIS_PVS],
			model);
}

void
Mod_Modellist_f(void)
{
	int i;
	model_t *mod;
	int total;

	total = 0;
	VID_Printf(PRINT_ALL, "Loaded models:\n");

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!mod->name[0])
		{
			continue;
		}

		VID_Printf(PRINT_ALL, "%8i : %s\n", mod->extradatasize, mod->name);
		total += mod->extradatasize;
	}

	VID_Printf(PRINT_ALL, "Total resident: %i\n", total);
}

void
Mod_Init(void)
{
	memset(mod_novis, 0xff, sizeof(mod_novis));
}

/*
 * Loads in a model for the given name
 */
model_t *
Mod_ForName(char *name, qboolean crash)
{
	model_t *mod;
	unsigned *buf;
	int i;

	if (!name[0])
	{
		VID_Error(ERR_DROP, "Mod_ForName: NULL name");
	}

	/* inline models are grabbed only from worldmodel */
	if (name[0] == '*')
	{
		i = (int)strtol(name + 1, (char **)NULL, 10);

		if ((i < 1) || !r_worldmodel || (i >= r_worldmodel->numsubmodels))
		{
			VID_Error(ERR_DROP, "bad inline model number");
		}

		return &mod_inline[i];
	}

	/* search the currently loaded models */
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!mod->name[0])
		{
			continue;
		}

		if (!strcmp(mod->name, name))
		{
			return mod;
		}
	}

	/* find a free model slot spot */
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!mod->name[0])
		{
			break; /* free spot */
		}
	}

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
		{
			VID_Error(ERR_DROP, "mod_numknown == MAX_MOD_KNOWN");
		}

		mod_numknown++;
	}

	strcpy(mod->name, name);

	/* load the file */
	modfilelen = FS_LoadFile(mod->name, (void **)&buf);

	if (!buf)
	{
		if (crash)
		{
			VID_Error(ERR_DROP, "Mod_NumForName: %s not found", mod->name);
		}

		memset(mod->name, 0, sizeof(mod->name));
		return NULL;
	}

	loadmodel = mod;

	/* call the apropriate loader */
	switch (LittleLong(*(unsigned *)buf))
	{
		case IDALIASHEADER:
			loadmodel->extradata = Hunk_Begin(0x200000);
			LoadMD2(mod, buf);
			break;

		case IDSPRITEHEADER:
			loadmodel->extradata = Hunk_Begin(0x10000);
			LoadSP2(mod, buf);
			break;

		case IDBSPHEADER:
			loadmodel->extradata = Hunk_Begin(0x1000000);
			Mod_LoadBrushModel(mod, buf);
			break;

		default:
			VID_Error(ERR_DROP,
				"Mod_NumForName: unknown fileid for %s",
				mod->name);
			break;
	}

	loadmodel->extradatasize = Hunk_End();

	FS_FreeFile(buf);

	return mod;
}

void
Mod_LoadLighting(lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}

	loadmodel->lightdata = Hunk_Alloc(l->filelen);
	memcpy(loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}

void
Mod_LoadVisibility(lump_t *l)
{
	int i;

	if (!l->filelen)
	{
		loadmodel->vis = NULL;
		return;
	}

	loadmodel->vis = Hunk_Alloc(l->filelen);
	memcpy(loadmodel->vis, mod_base + l->fileofs, l->filelen);

	loadmodel->vis->numclusters = LittleLong(loadmodel->vis->numclusters);

	for (i = 0; i < loadmodel->vis->numclusters; i++)
	{
		loadmodel->vis->bitofs[i][0] = LittleLong(loadmodel->vis->bitofs[i][0]);
		loadmodel->vis->bitofs[i][1] = LittleLong(loadmodel->vis->bitofs[i][1]);
	}
}

void
Mod_LoadVertexes(lump_t *l)
{
	dvertex_t *in;
	mvertex_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		VID_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",
				loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

float
Mod_RadiusFromBounds(vec3_t mins, vec3_t maxs)
{
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return VectorLength(corner);
}

void
Mod_LoadSubmodels(lump_t *l)
{
	dmodel_t *in;
	mmodel_t *out;
	int i, j, count;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		VID_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",
				loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			/* spread the mins / maxs by a pixel */
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
			out->origin[j] = LittleFloat(in->origin[j]);
		}

		out->radius = Mod_RadiusFromBounds(out->mins, out->maxs);
		out->headnode = LittleLong(in->headnode);
		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}

void
Mod_LoadEdges(lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		VID_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",
				loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc((count + 1) * sizeof(*out));

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

void
Mod_LoadTexinfo(lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out, *step;
	int i, j, count;
	char name[MAX_QPATH];
	int next;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		VID_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",
				loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 4; j++)
		{
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);
			out->vecs[1][j] = LittleFloat(in->vecs[1][j]);
		}

		out->flags = LittleLong(in->flags);
		next = LittleLong(in->nexttexinfo);

		if (next > 0)
		{
			out->next = loadmodel->texinfo + next;
		}
		else
		{
			out->next = NULL;
		}

		Com_sprintf(name, sizeof(name), "textures/%s.wal", in->texture);

		out->image = R_FindImage(name, it_wall);

		if (!out->image)
		{
			VID_Printf(PRINT_ALL, "Couldn't load %s\n", name);
			out->image = r_notexture;
		}
	}

	/* count animation frames */
	for (i = 0; i < count; i++)
	{
		out = &loadmodel->texinfo[i];
		out->numframes = 1;

		for (step = out->next; step && step != out; step = step->next)
		{
			out->numframes++;
		}
	}
}

/*
 * Fills in s->texturemins[] and s->extents[]
 */
void
Mod_CalcSurfaceExtents(msurface_t *s)
{
	float mins[2], maxs[2], val;
	int i, j, e;
	mvertex_t *v;
	mtexinfo_t *tex;
	int bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++)
	{
		e = loadmodel->surfedges[s->firstedge + i];

		if (e >= 0)
		{
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		}
		else
		{
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		}

		for (j = 0; j < 2; j++)
		{
			val = v->position[0] * tex->vecs[j][0] +
				  v->position[1] * tex->vecs[j][1] +
				  v->position[2] * tex->vecs[j][2] +
				  tex->vecs[j][3];

			if (val < mins[j])
			{
				mins[j] = val;
			}

			if (val > maxs[j])
			{
				maxs[j] = val;
			}
		}
	}

	for (i = 0; i < 2; i++)
	{
		bmins[i] = floor(mins[i] / 16);
		bmaxs[i] = ceil(maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
	}
}

void
Mod_LoadFaces(lump_t *l)
{
	dface_t *in;
	msurface_t *out;
	int i, count, surfnum;
	int planenum, side;
	int ti;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		VID_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",
				loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	currentmodel = loadmodel;

	LM_BeginBuildingLightmaps(loadmodel);

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		out->flags = 0;
		out->polys = NULL;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);

		if (side)
		{
			out->flags |= SURF_PLANEBACK;
		}

		out->plane = loadmodel->planes + planenum;

		ti = LittleShort(in->texinfo);

		if ((ti < 0) || (ti >= loadmodel->numtexinfo))
		{
			VID_Error(ERR_DROP, "MOD_LoadBmodel: bad texinfo number");
		}

		out->texinfo = loadmodel->texinfo + ti;

		Mod_CalcSurfaceExtents(out);

		/* lighting info */
		for (i = 0; i < MAXLIGHTMAPS; i++)
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

		/* set the drawing flags */
		if (out->texinfo->flags & SURF_WARP)
		{
			out->flags |= SURF_DRAWTURB;

			for (i = 0; i < 2; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}

			R_SubdivideSurface(out); /* cut up polygon for warps */
		}

		/* create lightmaps and polygons */
		if (!(out->texinfo->flags &
			  (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)))
		{
			LM_CreateSurfaceLightmap(out);
		}

		if (!(out->texinfo->flags & SURF_WARP))
		{
			LM_BuildPolygonFromSurface(out);
		}
	}

	LM_EndBuildingLightmaps();
}

void
Mod_SetParent(mnode_t *node, mnode_t *parent)
{
	node->parent = parent;

	if (node->contents != -1)
	{
		return;
	}

	Mod_SetParent(node->children[0], node);
	Mod_SetParent(node->children[1], node);
}

void
Mod_LoadNodes(lump_t *l)
{
	int i, j, count, p;
	dnode_t *in;
	mnode_t *out;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		VID_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",
				loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);
		out->contents = -1; /* differentiate from leafs */

		for (j = 0; j < 2; j++)
		{
			p = LittleLong(in->children[j]);

			if (p >= 0)
			{
				out->children[j] = loadmodel->nodes + p;
			}
			else
			{
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
			}
		}
	}

	Mod_SetParent(loadmodel->nodes, NULL); /* sets nodes and leafs */
}

void
Mod_LoadLeafs(lump_t *l)
{
	dleaf_t *in;
	mleaf_t *out;
	int i, j, count, p;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		VID_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",
				loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		out->firstmarksurface = loadmodel->marksurfaces +
								LittleShort(in->firstleafface);
		out->nummarksurfaces = LittleShort(in->numleaffaces);
	}
}

void
Mod_LoadMarksurfaces(lump_t *l)
{
	int i, j, count;
	short *in;
	msurface_t **out;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		VID_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",
				loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++)
	{
		j = LittleShort(in[i]);

		if ((j < 0) || (j >= loadmodel->numsurfaces))
		{
			VID_Error(ERR_DROP, "Mod_ParseMarksurfaces: bad surface number");
		}

		out[i] = loadmodel->surfaces + j;
	}
}

void
Mod_LoadSurfedges(lump_t *l)
{
	int i, count;
	int *in, *out;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		VID_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",
				loadmodel->name);
	}

	count = l->filelen / sizeof(*in);

	if ((count < 1) || (count >= MAX_MAP_SURFEDGES))
	{
		VID_Error(ERR_DROP, "MOD_LoadBmodel: bad surfedges count in %s: %i",
				loadmodel->name, count);
	}

	out = Hunk_Alloc(count * sizeof(*out));

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i = 0; i < count; i++)
	{
		out[i] = LittleLong(in[i]);
	}
}

void
Mod_LoadPlanes(lump_t *l)
{
	int i, j;
	cplane_t *out;
	dplane_t *in;
	int count;
	int bits;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		VID_Error(ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",
				loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * 2 * sizeof(*out));

	loadmodel->planes = out;
	loadmodel->numplanes = count;

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

void
Mod_LoadBrushModel(model_t *mod, void *buffer)
{
	int i;
	dheader_t *header;
	mmodel_t *bm;

	loadmodel->type = mod_brush;

	if (loadmodel != mod_known)
	{
		VID_Error(ERR_DROP, "Loaded a brush model after the world");
	}

	header = (dheader_t *)buffer;

	i = LittleLong(header->version);

	if (i != BSPVERSION)
	{
		VID_Error(ERR_DROP, "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)",
				mod->name, i, BSPVERSION);
	}

	/* swap all the lumps */
	mod_base = (byte *)header;

	for (i = 0; i < sizeof(dheader_t) / 4; i++)
	{
		((int *)header)[i] = LittleLong(((int *)header)[i]);
	}

	/* load into heap */
	Mod_LoadVertexes(&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges(&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadLighting(&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes(&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces(&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces(&header->lumps[LUMP_LEAFFACES]);
	Mod_LoadVisibility(&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs(&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes(&header->lumps[LUMP_NODES]);
	Mod_LoadSubmodels(&header->lumps[LUMP_MODELS]);
	mod->numframes = 2; /* regular and alternate animation */

	/* set up the submodels */
	for (i = 0; i < mod->numsubmodels; i++)
	{
		model_t *starmod;

		bm = &mod->submodels[i];
		starmod = &mod_inline[i];

		*starmod = *loadmodel;

		starmod->firstmodelsurface = bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;
		starmod->firstnode = bm->headnode;

		if (starmod->firstnode >= loadmodel->numnodes)
		{
			VID_Error(ERR_DROP, "Inline model %i has bad firstnode", i);
		}

		VectorCopy(bm->maxs, starmod->maxs);
		VectorCopy(bm->mins, starmod->mins);
		starmod->radius = bm->radius;

		if (i == 0)
		{
			*loadmodel = *starmod;
		}

		starmod->numleafs = bm->visleafs;
	}
}

void
Mod_Free(model_t *mod)
{
	Hunk_Free(mod->extradata);
	memset(mod, 0, sizeof(*mod));
}

void
Mod_FreeAll(void)
{
	int i;

	for (i = 0; i < mod_numknown; i++)
	{
		if (mod_known[i].extradatasize)
		{
			Mod_Free(&mod_known[i]);
		}
	}
}

/*
 * Specifies the model that will be used as the world
 */
void
R_BeginRegistration(char *model)
{
	char fullname[MAX_QPATH];
	cvar_t *flushmap;

	registration_sequence++;
	r_oldviewcluster = -1; /* force markleafs */

	Com_sprintf(fullname, sizeof(fullname), "maps/%s.bsp", model);

	/* explicitly free the old map if different
	   this guarantees that mod_known[0] is the
	   world map */
	flushmap = Cvar_Get("flushmap", "0", 0);

	if (strcmp(mod_known[0].name, fullname) || flushmap->value)
	{
		Mod_Free(&mod_known[0]);
	}

	r_worldmodel = Mod_ForName(fullname, true);

	r_viewcluster = -1;
}

struct model_s *
R_RegisterModel(char *name)
{
	model_t *mod;
	int i;
	dsprite_t *sprout;
	dmdl_t *pheader;

	mod = Mod_ForName(name, false);

	if (mod)
	{
		mod->registration_sequence = registration_sequence;

		/* register any images used by the models */
		if (mod->type == mod_sprite)
		{
			sprout = (dsprite_t *)mod->extradata;

			for (i = 0; i < sprout->numframes; i++)
			{
				mod->skins[i] = R_FindImage(sprout->frames[i].name, it_sprite);
			}
		}
		else if (mod->type == mod_alias)
		{
			pheader = (dmdl_t *)mod->extradata;

			for (i = 0; i < pheader->num_skins; i++)
			{
				mod->skins[i] = R_FindImage((char *)pheader + pheader->ofs_skins +
					   	i * MAX_SKINNAME, it_skin);
			}

			mod->numframes = pheader->num_frames;
		}
		else if (mod->type == mod_brush)
		{
			for (i = 0; i < mod->numtexinfo; i++)
			{
				mod->texinfo[i].image->registration_sequence =
					registration_sequence;
			}
		}
	}

	return mod;
}

void
R_EndRegistration(void)
{
	int i;
	model_t *mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!mod->name[0])
		{
			continue;
		}

		if (mod->registration_sequence != registration_sequence)
		{
			/* don't need this model */
			Mod_Free(mod);
		}
	}

	R_FreeUnusedImages();
}

