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
GL3_LM_UploadBlock(qboolean dynamic)
{
	STUB_ONCE("TODO: Implement!");
#if 0
	int texture;
	int height = 0;

	if (dynamic)
	{
		texture = 0;
	}
	else
	{
		texture = gl3_lms.current_lightmap_texture;
	}

	GL3_Bind(gl3state.lightmap_textures + texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (dynamic)
	{
		int i;

		for (i = 0; i < BLOCK_WIDTH; i++)
		{
			if (gl3_lms.allocated[i] > height)
			{
				height = gl3_lms.allocated[i];
			}
		}

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, BLOCK_WIDTH,
				height, GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE,
				gl3_lms.lightmap_buffer);
	}
	else
	{
		gl3_lms.internal_format = GL_LIGHTMAP_FORMAT;
		glTexImage2D(GL_TEXTURE_2D, 0, gl3_lms.internal_format,
				BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_LIGHTMAP_FORMAT,
				GL_UNSIGNED_BYTE, gl3_lms.lightmap_buffer);

		if (++gl3_lms.current_lightmap_texture == MAX_LIGHTMAPS)
		{
			ri.Sys_Error(ERR_DROP,
					"LM_UploadBlock() - MAX_LIGHTMAPS exceeded\n");
		}
	}
#endif // 0
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

	/* reconstruct the polygon */
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;

	VectorClear(total);

	/* draw texture */
	poly = Hunk_Alloc(sizeof(glpoly_t) +
		   (lnumverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++)
	{
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
		VectorCopy(vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

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

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	poly->numverts = lnumverts;
}

void
GL3_LM_CreateSurfaceLightmap(msurface_t *surf)
{
	int smax, tmax;
	byte *base;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
	{
		return;
	}

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	if (!GL3_LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
	{
		GL3_LM_UploadBlock(false);
		GL3_LM_InitBlock();

		if (!GL3_LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
		{
			ri.Sys_Error(ERR_FATAL, "Consecutive calls to LM_AllocBlock(%d,%d) failed\n",
					smax, tmax);
		}
	}

	surf->lightmaptexturenum = gl3_lms.current_lightmap_texture;

	base = gl3_lms.lightmap_buffer;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * LIGHTMAP_BYTES;

	GL3_SetCacheState(surf);
	GL3_BuildLightMap(surf, base, BLOCK_WIDTH * LIGHTMAP_BYTES);
}

void
GL3_LM_BeginBuildingLightmaps(gl3model_t *m)
{

	static lightstyle_t lightstyles[MAX_LIGHTSTYLES];
	int i;
	unsigned dummy[128 * 128];

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

	STUB_ONCE("TODO: IMPLEMENT!");
#if 0
	if (!gl_state.lightmap_textures)
	{
		gl3state.lightmap_textures = TEXNUM_LIGHTMAPS;
	}

	gl3_lms.current_lightmap_texture = 1;
	gl3_lms.internal_format = GL_LIGHTMAP_FORMAT;

	/* initialize the dynamic lightmap texture */
	GL3_Bind(gl_state.lightmap_textures + 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, gl3_lms.internal_format,
			BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_LIGHTMAP_FORMAT,
			GL_UNSIGNED_BYTE, dummy);
#endif // 0
}

void
GL3_LM_EndBuildingLightmaps(void)
{
	GL3_LM_UploadBlock(false);
}

