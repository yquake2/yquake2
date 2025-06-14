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

static int scrap_allocated[MAX_SCRAPS][SCRAP_WIDTH];
byte scrap_texels[MAX_SCRAPS][SCRAP_WIDTH * SCRAP_HEIGHT];
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
		best = SCRAP_HEIGHT;

		for (i = 0; i < SCRAP_WIDTH - w; i++)
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

		if (best + h > SCRAP_HEIGHT)
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
	R_Upload8(scrap_texels[0], SCRAP_WIDTH, SCRAP_HEIGHT, false, false);
	scrap_dirty = false;
}

void
Scrap_Init(void)
{
	memset (scrap_allocated, 0, sizeof(scrap_allocated));	// empty
	memset (scrap_texels, 255, sizeof(scrap_texels));	// transparent
}

