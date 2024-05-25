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
 * Allocate all the little status bar objects into a single texture
 * to crutch up inefficient hardware / drivers.
 *
 * =======================================================================
 */

#include "header/local.h"

int *scrap_allocated[MAX_SCRAPS];
byte *scrap_texels[MAX_SCRAPS];
qboolean scrap_dirty;

qboolean R_Upload8(byte *data,
		int width,
		int height,
		qboolean mipmap,
		qboolean is_sky);

/* returns a texture number and the position inside it */
int
Scrap_AllocBlock(int w, int h, int *x, int *y)
{
	int i, j;
	int best, best2;
	int texnum;
	w += 2;	// add an empty border to all sides
	h += 2;

	for (texnum = 0; texnum < MAX_SCRAPS; texnum++)
	{
		best = gl_state.scrap_height;

		for (i = 0; i < gl_state.scrap_width - w; i++)
		{
			best2 = 0;

			for (j = 0; j < w; j++)
			{
				if (scrap_allocated[texnum][i + j] >= best)
				{
					break;
				}

				if (scrap_allocated[texnum][i + j] > best2)
				{
					best2 = scrap_allocated[texnum][i + j];
				}
			}

			if (j == w)
			{   /* this is a valid spot */
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > gl_state.scrap_height)
		{
			continue;
		}

		for (i = 0; i < w; i++)
		{
			scrap_allocated[texnum][*x + i] = best + h;
		}
		(*x)++;	// jump the border
		(*y)++;

		return texnum;
	}

	return -1;
}

void
Scrap_Upload(void)
{
	R_Bind(TEXNUM_SCRAPS);
	R_Upload8(scrap_texels[0], gl_state.scrap_width,
			gl_state.scrap_height, false, false);
	scrap_dirty = false;
}

void
Scrap_Free(void)
{
	for (int i = 0; i < MAX_SCRAPS; i++)
	{
		if (scrap_allocated[i])
		{
			free(scrap_allocated[i]);
		}
		scrap_allocated[i] = NULL;

		if (scrap_texels[i])
		{
			free(scrap_texels[i]);
		}
		scrap_texels[i] = NULL;
	}
}

void
Scrap_Init(void)
{
	const unsigned int allocd_size = gl_state.scrap_width * sizeof(int);
	const unsigned int texels_size = gl_state.scrap_width
			* gl_state.scrap_height * sizeof(byte);
	int i;

	Scrap_Free();

	for (i = 0; i < MAX_SCRAPS; i++)
	{
		if (!scrap_allocated[i])
		{
			scrap_allocated[i] = malloc (allocd_size) ;
		}
		if (!scrap_texels[i])
		{
			scrap_texels[i] = malloc (texels_size) ;
		}

		if (!scrap_allocated[i] || !scrap_texels[i])
		{
			ri.Sys_Error(ERR_FATAL, "Could not allocate scrap memory.\n");
		}
		memset (scrap_allocated[i], 0, allocd_size);	// empty
		memset (scrap_texels[i], 255, texels_size);	// transparent
	}
}

