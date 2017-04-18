/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016-2017 Daniel Gibson
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
 * Lightmap handling
 *
 * =======================================================================
 */

#include "header/local.h"


#define TEXNUM_LIGHTMAPS 1024

extern gl3lightmapstate_t gl3_lms;

void
GL3_LM_InitBlock(void)
{
	memset(gl3_lms.allocated, 0, sizeof(gl3_lms.allocated));
}

void
GL3_LM_UploadBlock(void)
{
	int map;

	// NOTE: we don't use the dynamic lightmap anymore - all lightmaps are loaded at level load
	//       and not changed after that. they're blended dynamically depending on light styles
	//       though, and dynamic lights are (will be) applied in shader, hopefully per fragment.

	GL3_BindLightmap(gl3_lms.current_lightmap_texture);

	// upload all 4 lightmaps
	for(map=0; map < MAX_LIGHTMAPS_PER_SURFACE; ++map)
	{
		GL3_SelectTMU(GL_TEXTURE1+map); // this relies on GL_TEXTURE2 being GL_TEXTURE1+1 etc
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		gl3_lms.internal_format = GL_LIGHTMAP_FORMAT;
		glTexImage2D(GL_TEXTURE_2D, 0, gl3_lms.internal_format,
		             BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_LIGHTMAP_FORMAT,
		             GL_UNSIGNED_BYTE, gl3_lms.lightmap_buffers[map]);
	}

	if (++gl3_lms.current_lightmap_texture == MAX_LIGHTMAPS)
	{
		ri.Sys_Error(ERR_DROP, "LM_UploadBlock() - MAX_LIGHTMAPS exceeded\n");
	}
}

/*
 * returns a texture number and the position inside it
 */
qboolean
GL3_LM_AllocBlock(int w, int h, int *x, int *y)
{
	int i, j;
	int best, best2;

	best = BLOCK_HEIGHT;

	for (i = 0; i < BLOCK_WIDTH - w; i++)
	{
		best2 = 0;

		for (j = 0; j < w; j++)
		{
			if (gl3_lms.allocated[i + j] >= best)
			{
				break;
			}

			if (gl3_lms.allocated[i + j] > best2)
			{
				best2 = gl3_lms.allocated[i + j];
			}
		}

		if (j == w)
		{
			/* this is a valid spot */
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > BLOCK_HEIGHT)
	{
		return false;
	}

	for (i = 0; i < w; i++)
	{
		gl3_lms.allocated[*x + i] = best + h;
	}

	return true;
}

void
GL3_LM_BuildPolygonFromSurface(msurface_t *fa)
{
	int i, lindex, lnumverts;
	medge_t *pedges, *r_pedge;
	float *vec;
	float s, t;
	glpoly_t *poly;
	vec3_t total;
	vec3_t normal;

	/* reconstruct the polygon */
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;

	VectorClear(total);

	/* draw texture */
	poly = Hunk_Alloc(sizeof(glpoly_t) +
		   (lnumverts - 4) * sizeof(gl3_3D_vtx_t));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	VectorCopy(fa->plane->normal, normal);

	if(fa->flags & SURF_PLANEBACK)
	{
		// if for some reason the normal sticks to the back of the plane, invert it
		// so it's usable for the shader
		for (i=0; i<3; ++i)  normal[i] = -normal[i];
	}

	for (i = 0; i < lnumverts; i++)
	{
		gl3_3D_vtx_t* vert = &poly->vertices[i];

		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = currentmodel->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = currentmodel->vertexes[r_pedge->v[1]].position;
		}

		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->image->width;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->image->height;

		VectorAdd(total, vec, total);
		VectorCopy(vec, vert->pos);
		vert->texCoord[0] = s;
		vert->texCoord[1] = t;

		/* lightmap texture coordinates */
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH * 16; /* fa->texinfo->texture->width; */

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16; /* fa->texinfo->texture->height; */

		vert->lmTexCoord[0] = s;
		vert->lmTexCoord[1] = t;

		VectorCopy(normal, vert->normal);
		vert->lightFlags = 0;
	}

	poly->numverts = lnumverts;
}

void
GL3_LM_CreateSurfaceLightmap(msurface_t *surf)
{
	int smax, tmax;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
	{
		return;
	}

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	if (!GL3_LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
	{
		GL3_LM_UploadBlock();
		GL3_LM_InitBlock();

		if (!GL3_LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
		{
			ri.Sys_Error(ERR_FATAL, "Consecutive calls to LM_AllocBlock(%d,%d) failed\n",
					smax, tmax);
		}
	}

	surf->lightmaptexturenum = gl3_lms.current_lightmap_texture;

	GL3_BuildLightMap(surf, (surf->light_t * BLOCK_WIDTH + surf->light_s) * LIGHTMAP_BYTES, BLOCK_WIDTH * LIGHTMAP_BYTES);
}

void
GL3_LM_BeginBuildingLightmaps(gl3model_t *m)
{

	static lightstyle_t lightstyles[MAX_LIGHTSTYLES];
	int i;

	memset(gl3_lms.allocated, 0, sizeof(gl3_lms.allocated));

	gl3_framecount = 1; /* no dlightcache */

	/* setup the base lightstyles so the lightmaps
	   won't have to be regenerated the first time
	   they're seen */
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		lightstyles[i].rgb[0] = 1;
		lightstyles[i].rgb[1] = 1;
		lightstyles[i].rgb[2] = 1;
		lightstyles[i].white = 3;
	}

	gl3_newrefdef.lightstyles = lightstyles;

	gl3_lms.current_lightmap_texture = 0;
	gl3_lms.internal_format = GL_LIGHTMAP_FORMAT;

	// Note: the dynamic lightmap used to be initialized here, we don't use that anymore.
}

void
GL3_LM_EndBuildingLightmaps(void)
{
	GL3_LM_UploadBlock();
}

