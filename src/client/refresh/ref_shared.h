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
 * Header shared between different refreshers (but not with client)
 *
 * =======================================================================
 */

#ifndef SRC_CLIENT_REFRESH_REF_SHARED_H_
#define SRC_CLIENT_REFRESH_REF_SHARED_H_

#include "../vid/header/ref.h"

#ifdef _MSC_VER

  #include <malloc.h>

  #define YQ2_VLA(TYPE, VARNAME, NUMELEMS) \
	TYPE * VARNAME = (TYPE *) _malloca(sizeof(TYPE) * NUMELEMS)
  #define YQ2_VLAFREE(VARNAME) \
	_freea(VARNAME); VARNAME=NULL;

#else // other compilers hopefully support C99 VLAs (gcc/mingw and clang do)

  #define YQ2_VLA(TYPE, VARNAME, NUMELEMS) \
	TYPE VARNAME[NUMELEMS]
  #define YQ2_VLAFREE(VARNAME)

#endif

/*
 * skins will be outline flood filled and mip mapped
 * pics and sprites with alpha will be outline flood filled
 * pic won't be mip mapped
 *
 * model skin
 * sprite frame
 * wall texture
 * pic
 */
typedef enum
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky
} imagetype_t;

typedef enum
{
	mod_bad,
	mod_brush,
	mod_sprite,
	mod_alias
} modtype_t;

#define MAX_LBM_HEIGHT 480

extern void R_Printf(int level, const char* msg, ...) PRINTF_ATTR(2, 3);

/* Shared images load */
typedef struct image_s* (*loadimage_t)(const char *name, byte *pic, int width, int realwidth,
	int height, int realheight, size_t data_size, imagetype_t type, int bits);
extern struct image_s* LoadWal(const char *origname, imagetype_t type, loadimage_t load_image);
extern struct image_s* LoadM8(const char *origname, imagetype_t type, loadimage_t load_image);
extern struct image_s* LoadM32(const char *origname, imagetype_t type, loadimage_t load_image);
extern void FixFileExt(const char *origname, const char *ext, char *filename, size_t size);
extern void GetPCXPalette(byte **colormap, unsigned *d_8to24table);
extern void LoadPCX(const char *origname, byte **pic, byte **palette, int *width, int *height);
extern void GetPCXInfo(const char *origname, int *width, int *height);
extern void GetWalInfo(const char *name, int *width, int *height);
extern void GetM8Info(const char *name, int *width, int *height);
extern void GetM32Info(const char *name, int *width, int *height);

extern qboolean ResizeSTB(const byte *input_pixels, int input_width, int input_height,
			  byte *output_pixels, int output_width, int output_height);
extern void SmoothColorImage(unsigned *dst, size_t size, size_t rstep);
extern void scale2x(const byte *src, byte *dst, int width, int height);
extern void scale3x(const byte *src, byte *dst, int width, int height);

extern float Mod_RadiusFromBounds(const vec3_t mins, const vec3_t maxs);
extern const byte* Mod_DecompressVis(const byte *in, int row);

/* Shared models struct */

enum {
	SIDE_FRONT = 0,
	SIDE_BACK = 1,
	SIDE_ON = 2
};

// FIXME: differentiate from texinfo SURF_ flags
enum {
	SURF_PLANEBACK = 0x02,
	SURF_DRAWSKY = 0x04, // sky brush face
	SURF_DRAWTURB = 0x10,
	SURF_DRAWBACKGROUND = 0x40,
	SURF_UNDERWATER = 0x80
};

typedef struct mvertex_s
{
	vec3_t		position;
} mvertex_t;

typedef struct medge_s
{
	unsigned short	v[2];
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct mtexinfo_s
{
	float	vecs[2][4];
	int	flags;
	int	numframes;
	struct mtexinfo_s	*next; /* animation chain */
	struct image_s	*image;
} mtexinfo_t;

#define	CONTENTS_NODE	-1
typedef struct mnode_s
{
	/* common with leaf */
	int		contents;	/* CONTENTS_NODE, to differentiate from leafs */
	int		visframe;	/* node needs to be traversed if current */

	float		minmaxs[6];	/* for bounding box culling */

	struct mnode_s	*parent;

	/* node specific */
	cplane_t	*plane;
	struct mnode_s	*children[2];

	unsigned short	firstsurface;
	unsigned short	numsurfaces;
} mnode_t;

typedef struct mleaf_s
{
	/* common with leaf */
	int		contents;	/* CONTENTS_NODE, to differentiate from leafs */
	int		visframe;	/* node needs to be traversed if current */

	float		minmaxs[6];	/* for bounding box culling */

	struct mnode_s	*parent;

	/* leaf specific */
	int		cluster;
	int		area;

	struct msurface_s	**firstmarksurface;
	int		nummarksurfaces;
	int		key;	/* BSP sequence number for leaf's contents */
} mleaf_t;

/* Shared models func */
typedef struct image_s* (*findimage_t)(const char *name, imagetype_t type);
extern void *Mod_LoadMD2 (const char *mod_name, const void *buffer, int modfilelen,
	vec3_t mins, vec3_t maxs, struct image_s **skins,
	findimage_t find_image, modtype_t *type);
extern void *Mod_LoadSP2 (const char *mod_name, const void *buffer, int modfilelen,
	struct image_s **skins, findimage_t find_image, modtype_t *type);
extern int Mod_ReLoadSkins(struct image_s **skins, findimage_t find_image,
	void *extradata, modtype_t type);
extern struct image_s *GetSkyImage(const char *skyname, const char* surfname,
	qboolean palettedtexture, findimage_t find_image);
extern struct image_s *GetTexImage(const char *name, findimage_t find_image);
extern struct image_s *R_FindPic(const char *name, findimage_t find_image);
extern struct image_s* R_LoadImage(const char *name, const char* namewe, const char *ext,
	imagetype_t type, qboolean r_retexturing, loadimage_t load_image);
extern void Mod_LoadNodes(const char *name, cplane_t *planes, int numplanes,
	mleaf_t *leafs, int numleafs, mnode_t **nodes, int *numnodes,
	const byte *mod_base, const lump_t *l);
extern void Mod_LoadVertexes(const char *name, mvertex_t **vertexes, int *numvertexes,
	const byte *mod_base, const lump_t *l, int extra);
extern void Mod_LoadVisibility(dvis_t **vis, const byte *mod_base, const lump_t *l);
extern void Mod_LoadLighting(byte **lightdata, const byte *mod_base, const lump_t *l);
extern void Mod_LoadTexinfo(const char *name, mtexinfo_t **texinfo, int *numtexinfo,
	const byte *mod_base, const lump_t *l, findimage_t find_image,
	struct image_s *notexture, int extra);
extern void Mod_LoadEdges(const char *name, medge_t **edges, int *numedges,
	const byte *mod_base, const lump_t *l, int extra);
extern void Mod_LoadPlanes (const char *name, cplane_t **planes, int *numplanes,
	const byte *mod_base, const lump_t *l, int extra);
extern void Mod_LoadSurfedges (const char *name, int **surfedges, int *numsurfedges,
	const byte *mod_base, const lump_t *l, int extra);
extern int Mod_CalcLumpHunkSize(const lump_t *l, int inSize, int outSize, int extra);
extern mleaf_t *Mod_PointInLeaf(const vec3_t p, mnode_t *node);

/* Surface logic */
#define DLIGHT_CUTOFF 64

typedef void (*marksurfacelights_t)(dlight_t *light, int bit, mnode_t *node,
	int lightframecount);
extern void R_MarkLights(dlight_t *light, int bit, mnode_t *node, int lightframecount,
	marksurfacelights_t mark_surface_lights);
extern struct image_s *R_TextureAnimation(const entity_t *currententity,
	const mtexinfo_t *tex);
extern qboolean R_AreaVisible(const byte *areabits, mleaf_t *pleaf);
extern qboolean R_CullBox(vec3_t mins, vec3_t maxs, cplane_t *frustum);
extern void R_SetFrustum(vec3_t vup, vec3_t vpn, vec3_t vright, vec3_t r_origin,
	float fov_x, float fov_y, cplane_t *frustum);

#endif /* SRC_CLIENT_REFRESH_REF_SHARED_H_ */
