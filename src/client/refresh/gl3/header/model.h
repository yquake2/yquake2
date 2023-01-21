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
 * Header for the model stuff.
 *
 * =======================================================================
 */

#ifndef SRC_CLIENT_REFRESH_GL3_HEADER_MODEL_H_
#define SRC_CLIENT_REFRESH_GL3_HEADER_MODEL_H_

// used for vertex array elements when drawing brushes, sprites, sky and more
// (ok, it has the layout used for rendering brushes, but is not used there)
typedef struct gl3_3D_vtx_s {
	vec3_t pos;
	float texCoord[2];
	float lmTexCoord[2]; // lightmap texture coordinate (sometimes unused)
	vec3_t normal;
	GLuint lightFlags; // bit i set means: dynlight i affects surface
} gl3_3D_vtx_t;

// used for vertex array elements when drawing models
typedef struct gl3_alias_vtx_s {
	GLfloat pos[3];
	GLfloat texCoord[2];
	GLfloat color[4];
} gl3_alias_vtx_t;

/* in memory representation */

typedef struct glpoly_s
{
	struct  glpoly_s *next;
	struct  glpoly_s *chain;
	int numverts;
	int flags; /* for SURF_UNDERWATER (not needed anymore?) */
	gl3_3D_vtx_t vertices[4]; /* variable sized */
} glpoly_t;

typedef struct msurface_s
{
	int visframe; /* should be drawn when node is crossed */

	cplane_t *plane;
	int flags;

	int firstedge;          /* look up in model->surfedges[], negative numbers */
	int numedges;           /* are backwards edges */

	short texturemins[2];
	short extents[2];

	int light_s, light_t;           /* gl lightmap coordinates */
	int dlight_s, dlight_t;         /* gl lightmap coordinates for dynamic lightmaps */

	glpoly_t *polys;                /* multiple if warped */
	struct  msurface_s *texturechain;
	// struct  msurface_s *lightmapchain; not used/needed anymore

	mtexinfo_t *texinfo;

	/* lighting info */
	int dlightframe;
	int dlightbits;

	int lightmaptexturenum;
	byte styles[MAXLIGHTMAPS]; // MAXLIGHTMAPS = MAX_LIGHTMAPS_PER_SURFACE (defined in local.h)
	// I think cached_light is not used/needed anymore
	//float cached_light[MAXLIGHTMAPS];       /* values currently used in lightmap */
	byte *samples;                          /* [numstyles*surfsize] */
} msurface_t;

/* Whole model */

// this, must be struct model_s, not gl3model_s,
// because struct model_s* is returned by re.RegisterModel()
typedef struct model_s
{
	char name[MAX_QPATH];

	int registration_sequence;

	modtype_t type;
	int numframes;

	int flags;

	/* volume occupied by the model graphics */
	vec3_t mins, maxs;
	float radius;

	/* solid volume for clipping */
	qboolean clipbox;
	vec3_t clipmins, clipmaxs;

	/* brush model */
	int firstmodelsurface, nummodelsurfaces;
	int lightmap; /* only for submodels */

	int numsubmodels;
	struct model_s *submodels;

	int numplanes;
	cplane_t *planes;

	int numleafs; /* number of visible leafs, not counting 0 */
	mleaf_t *leafs;

	int numvertexes;
	mvertex_t *vertexes;

	int numedges;
	medge_t *edges;

	int numnodes;
	int firstnode;
	mnode_t *nodes;

	int numtexinfo;
	mtexinfo_t *texinfo;

	int numsurfaces;
	msurface_t *surfaces;

	int numsurfedges;
	int *surfedges;

	int nummarksurfaces;
	msurface_t **marksurfaces;

	dvis_t *vis;

	byte *lightdata;

	/* for alias models and skins */
	gl3image_t *skins[MAX_MD2SKINS];

	int extradatasize;
	void *extradata;

	// submodules
	vec3_t		origin;	// for sounds or lights
} gl3model_t;

#endif /* SRC_CLIENT_REFRESH_GL3_HEADER_MODEL_H_ */
