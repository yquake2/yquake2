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
 * The models file format
 *
 * =======================================================================
 */

#include "../ref_shared.h"

/*
=================
Mod_LoadAliasModel/Mod_LoadMD2
=================
*/
void *
Mod_LoadMD2 (const char *mod_name, const void *buffer, int modfilelen,
	vec3_t mins, vec3_t maxs, struct image_s **skins, findimage_t find_image,
	modtype_t *type)
{
	dmdl_t		*pinmodel, *pheader;
	dtriangle_t	*pintri, *pouttri;
	dstvert_t	*pinst, *poutst;
	int		*pincmd, *poutcmd;
	void	*extradata;
	int		version;
	int		ofs_end;
	int		i, j;

	pinmodel = (dmdl_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
	{
		R_Printf(PRINT_ALL, "%s: %s has wrong version number (%i should be %i)",
				__func__, mod_name, version, ALIAS_VERSION);
		return NULL;
	}

	ofs_end = LittleLong(pinmodel->ofs_end);
	if (ofs_end < 0 || ofs_end > modfilelen)
	{
		R_Printf(PRINT_ALL, "%s: model %s file size(%d) too small, should be %d",
				__func__, mod_name, modfilelen, ofs_end);
		return NULL;
	}

	extradata = Hunk_Begin(modfilelen);
	pheader = Hunk_Alloc(ofs_end);

	// byte swap the header fields and sanity check
	for (i=0 ; i<sizeof(dmdl_t)/sizeof(int) ; i++)
		((int *)pheader)[i] = LittleLong (((int *)buffer)[i]);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
	{
		R_Printf(PRINT_ALL, "%s: model %s has a skin taller than %d",
				__func__, mod_name, MAX_LBM_HEIGHT);
		return NULL;
	}

	if (pheader->num_xyz <= 0)
	{
		R_Printf(PRINT_ALL, "%s: model %s has no vertices",
				__func__, mod_name);
		return NULL;
	}

	if (pheader->num_xyz > MAX_VERTS)
	{
		R_Printf(PRINT_ALL, "%s: model %s has too many vertices",
				__func__, mod_name);
		return NULL;
	}

	if (pheader->num_st <= 0)
	{
		R_Printf(PRINT_ALL, "%s: model %s has no st vertices",
				__func__, mod_name);
		return NULL;
	}

	if (pheader->num_tris <= 0)
	{
		R_Printf(PRINT_ALL, "%s: model %s has no triangles",
				__func__, mod_name);
		return NULL;
	}

	if (pheader->num_frames <= 0)
	{
		R_Printf(PRINT_ALL, "%s: model %s has no frames",
				__func__, mod_name);
		return NULL;
	}

	if (pheader->num_skins > MAX_MD2SKINS)
	{
		R_Printf(PRINT_ALL, "%s has too many skins (%i > %i), "
				"extra sprites will be ignored\n",
				mod_name, pheader->num_skins, MAX_MD2SKINS);
		pheader->num_skins = MAX_MD2SKINS;
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
			__func__, mod_name, poutcmd[pheader->num_glcmds-1]);
	}

	// register all skins
	memcpy ((char *)pheader + pheader->ofs_skins, (char *)pinmodel + pheader->ofs_skins,
		pheader->num_skins*MAX_SKINNAME);
	for (i=0 ; i<pheader->num_skins ; i++)
	{
		skins[i] = find_image((char *)pheader + pheader->ofs_skins + i*MAX_SKINNAME,
			it_skin);
	}

	*type = mod_alias;

	mins[0] = -32;
	mins[1] = -32;
	mins[2] = -32;
	maxs[0] = 32;
	maxs[1] = 32;
	maxs[2] = 32;

	return extradata;
}

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/

/*
=================
Mod_LoadSP2

support for .sp2 sprites
=================
*/
void *
Mod_LoadSP2 (const char *mod_name, const void *buffer, int modfilelen,
	struct image_s **skins, findimage_t find_image, modtype_t *type)
{
	dsprite_t *sprin, *sprout;
	void	*extradata;
	int i;

	sprin = (dsprite_t *)buffer;
	extradata = Hunk_Begin(modfilelen);
	sprout = Hunk_Alloc(modfilelen);

	sprout->ident = LittleLong(sprin->ident);
	sprout->version = LittleLong(sprin->version);
	sprout->numframes = LittleLong(sprin->numframes);

	if (sprout->version != SPRITE_VERSION)
	{
		R_Printf(PRINT_ALL, "%s has wrong version number (%i should be %i)",
				mod_name, sprout->version, SPRITE_VERSION);
		return NULL;
	}

	if (sprout->numframes > MAX_MD2SKINS)
	{
		R_Printf(PRINT_ALL, "%s has too many frames (%i > %i), "
				"extra frames will be ignored\n",
				mod_name, sprout->numframes, MAX_MD2SKINS);
		sprout->numframes = MAX_MD2SKINS;
	}

	/* byte swap everything */
	for (i = 0; i < sprout->numframes; i++)
	{
		sprout->frames[i].width = LittleLong(sprin->frames[i].width);
		sprout->frames[i].height = LittleLong(sprin->frames[i].height);
		sprout->frames[i].origin_x = LittleLong(sprin->frames[i].origin_x);
		sprout->frames[i].origin_y = LittleLong(sprin->frames[i].origin_y);
		memcpy(sprout->frames[i].name, sprin->frames[i].name, MAX_SKINNAME);

		skins[i] = find_image((char *)sprout->frames[i].name, it_sprite);
	}

	*type = mod_sprite;

	return extradata;
}

/*
=================
Mod_ReLoad

Reload images in SP2/MD2 (mark registration_sequence)
=================
*/
int
Mod_ReLoadSkins(struct image_s **skins, findimage_t find_image, void *extradata,
	modtype_t type)
{
	if (type == mod_sprite)
	{
		dsprite_t	*sprout;
		int	i;

		sprout = (dsprite_t *)extradata;
		for (i=0; i < sprout->numframes; i++)
		{
			skins[i] = find_image(sprout->frames[i].name, it_sprite);
		}
		return sprout->numframes;
	}
	else if (type == mod_alias)
	{
		dmdl_t *pheader;
		int	i;

		pheader = (dmdl_t *)extradata;
		for (i=0; i < pheader->num_skins; i++)
		{
			skins[i] = find_image ((char *)pheader + pheader->ofs_skins + i*MAX_SKINNAME, it_skin);
		}
		return  pheader->num_frames;
	}
	/* Unknow format, no images associated with it */
	return 0;
}

/*
=================
Mod_SetParent
=================
*/
static void
Mod_SetParent(mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents != CONTENTS_NODE)
	{
		return;
	}

	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_NumberLeafs
=================
*/
static void
Mod_NumberLeafs(mleaf_t *leafs, mnode_t *node, int *r_leaftovis, int *r_vistoleaf,
	int *numvisleafs)
{
	if (node->contents != CONTENTS_NODE)
	{
		mleaf_t *leaf;
		int leafnum;

		leaf = (mleaf_t *)node;
		leafnum = leaf - leafs;
		if (leaf->contents & CONTENTS_SOLID)
		{
			return;
		}

		r_leaftovis[leafnum] = *numvisleafs;
		r_vistoleaf[*numvisleafs] = leafnum;
		(*numvisleafs) ++;
		return;
	}

	Mod_NumberLeafs(leafs, node->children[0], r_leaftovis, r_vistoleaf,
		numvisleafs);
	Mod_NumberLeafs(leafs, node->children[1], r_leaftovis, r_vistoleaf,
		numvisleafs);
}

/*
=================
Mod_LoadNodes
=================
*/
void
Mod_LoadNodes(const char *name, cplane_t *planes, int numplanes, mleaf_t *leafs,
	int numleafs, mnode_t **nodes, int *numnodes, const byte *mod_base,
	const lump_t *l)
{
	int	r_leaftovis[MAX_MAP_LEAFS], r_vistoleaf[MAX_MAP_LEAFS];
	int	i, count, numvisleafs;
	dnode_t	*in;
	mnode_t	*out;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof(*out));

	*nodes = out;
	*numnodes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		int j, planenum;

		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		planenum = LittleLong(in->planenum);
		if (planenum  < 0 || planenum >= numplanes)
		{
			ri.Sys_Error(ERR_DROP, "%s: Incorrect %d < %d planenum.",
					__func__, planenum, numplanes);
		}
		out->plane = planes + planenum;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);
		out->contents = CONTENTS_NODE; /* differentiate from leafs */

		for (j = 0; j < 2; j++)
		{
			int leafnum;

			leafnum = LittleLong(in->children[j]);

			if (leafnum >= 0)
			{
				if (leafnum  < 0 || leafnum >= *numnodes)
				{
					ri.Sys_Error(ERR_DROP, "%s: Incorrect %d nodenum as leaf.",
							__func__, leafnum);
				}

				out->children[j] = *nodes + leafnum;
			}
			else
			{
				leafnum = -1 - leafnum;
				if (leafnum  < 0 || leafnum >= numleafs)
				{
					ri.Sys_Error(ERR_DROP, "%s: Incorrect %d leafnum.",
							__func__, leafnum);
				}

				out->children[j] = (mnode_t *)(leafs + leafnum);
			}
		}
	}

	Mod_SetParent(*nodes, NULL); /* sets nodes and leafs */

	numvisleafs = 0;
	Mod_NumberLeafs (leafs, *nodes, r_leaftovis, r_vistoleaf, &numvisleafs);
}

/*
=================
Mod_LoadVisibility
=================
*/
void
Mod_LoadVisibility (dvis_t **vis, const byte *mod_base, const lump_t *l)
{
	dvis_t	*out;
	int	i;

	if (!l->filelen)
	{
		*vis = NULL;
		return;
	}

	out = Hunk_Alloc(l->filelen);
	*vis = out;
	memcpy(out, mod_base + l->fileofs, l->filelen);

	out->numclusters = LittleLong(out->numclusters);

	for (i = 0; i < out->numclusters; i++)
	{
		out->bitofs[i][0] = LittleLong(out->bitofs[i][0]);
		out->bitofs[i][1] = LittleLong(out->bitofs[i][1]);
	}
}

/*
=================
Mod_LoadVertexes

extra for skybox
=================
*/
void
Mod_LoadVertexes(const char *name, mvertex_t **vertexes, int *numvertexes,
	const byte *mod_base, const lump_t *l, int extra)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int	i, count;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc((count + extra)*sizeof(*out));

	/*
	 * FIXME: Recheck with soft render
	 * Fix for the problem where the games dumped core
	 * when changing levels.
	 */
	memset(out, 0, (count + extra) * sizeof(*out));

	*vertexes = out;
	*numvertexes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

/*
=================
Mod_LoadLighting
=================
*/
void
Mod_LoadLighting(byte **lightdata, const byte *mod_base, const lump_t *l)
{
	int	size;

	if (!l->filelen)
	{
		*lightdata = NULL;
		return;
	}

	size = l->filelen;
	*lightdata = Hunk_Alloc(size);
	memcpy(*lightdata, mod_base + l->fileofs, size);
}

/*
=================
Mod_LoadTexinfo

extra for skybox in soft render
=================
*/
void
Mod_LoadTexinfo(const char *name, mtexinfo_t **texinfo, int *numtexinfo,
	const byte *mod_base, const lump_t *l, findimage_t find_image,
	struct image_s *notexture, int extra)
{
	texinfo_t *in;
	mtexinfo_t *out, *step;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc((count + extra)*sizeof(*out));

	*texinfo = out;
	*numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		struct image_s *image;
		int j, next;

		for (j = 0; j < 4; j++)
		{
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);
			out->vecs[1][j] = LittleFloat(in->vecs[1][j]);
		}

		out->flags = LittleLong (in->flags);

		next = LittleLong (in->nexttexinfo);
		if (next > 0)
		{
			out->next = *texinfo + next;
		}
		else
		{
			/*
			 * Fix for the problem where the game
			 * domed core when loading a new level.
			 */
			out->next = NULL;
		}

		image = GetTexImage(in->texture, find_image);
		if (!image)
		{
			R_Printf(PRINT_ALL, "%s: Couldn't load %s\n",
				__func__, in->texture);
			image = notexture;
		}

		out->image = image;
	}

	// count animation frames
	for (i=0 ; i<count ; i++)
	{
		out = (*texinfo) + i;
		out->numframes = 1;
		for (step = out->next ; step && step != out ; step=step->next)
		{
			out->numframes++;
		}
	}
}

/*
=================
Mod_LoadEdges

extra is used for skybox, which adds 6 surfaces
=================
*/
void
Mod_LoadEdges(const char *name, medge_t **edges, int *numedges,
	const byte *mod_base, const lump_t *l, int extra)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc((count + extra) * sizeof(*out));

	*edges = out;
	*numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

/*
=================
Mod_LoadPlanes

extra is used for skybox, which adds 6 surfaces
=================
*/
void
Mod_LoadPlanes(const char *name, cplane_t **planes, int *numplanes,
	const byte *mod_base, const lump_t *l, int extra)
{
	int i;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, name);
	}

	count = l->filelen / sizeof(*in);
	// FIXME: why double of count
	out = Hunk_Alloc((count * 2 + extra) * sizeof(*out));

	*planes = out;
	*numplanes = count;

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

/*
=================
Mod_LoadSurfedges
=================
*/
void
Mod_LoadSurfedges(const char *name, int **surfedges, int *numsurfedges,
	const byte *mod_base, const lump_t *l, int extra)
{
	int		i, count;
	int		*in, *out;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
	{
		ri.Sys_Error(ERR_DROP, "%s: funny lump size in %s",
				__func__, name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc((count + extra)*sizeof(*out));	// extra for skybox

	*surfedges = out;
	*numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}

/*
=================
Mod_LoadSurfedges

calculate the size that Hunk_Alloc(), called by Mod_Load*() from Mod_LoadBrushModel(),
will use (=> includes its padding), so we'll know how big the hunk needs to be
extra is used for skybox, which adds 6 surfaces
=================
*/
int
Mod_CalcLumpHunkSize(const lump_t *l, int inSize, int outSize, int extra)
{
	if (l->filelen % inSize)
	{
		// Mod_Load*() will error out on this because of "funny size"
		// don't error out here because in Mod_Load*() it can print the functionname
		// (=> tells us what kind of lump) before shutting down the game
		return 0;
	}

	int count = l->filelen / inSize + extra;
	int size = count * outSize;

	// round to cacheline, like Hunk_Alloc() does
	size = (size + 31) & ~31;
	return size;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *
Mod_PointInLeaf(const vec3_t p, mnode_t *node)
{
	if (!node)
	{
		ri.Sys_Error(ERR_DROP, "%s: bad node.", __func__);
		return NULL;
	}

	while (1)
	{
		float d;
		cplane_t *plane;

		if (node->contents != CONTENTS_NODE)
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
