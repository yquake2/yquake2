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
static unsigned scrap_texels[MAX_SCRAPS][SCRAP_WIDTH * SCRAP_HEIGHT];
static qboolean scrap_dirty[MAX_SCRAPS];

static qboolean
CommonAllocBlock(int *allocated, size_t alloc_width, size_t alloc_height,
	unsigned w, unsigned h, int *x, int *y)
{
	size_t i, best;

	best = alloc_height;

	for (i = 0; i < alloc_width - w; i++)
	{
		size_t best2, j;

		best2 = 0;

		for (j = 0; j < w; j++)
		{
			if (allocated[i + j] >= best)
			{
				break;
			}

			if (allocated[i + j] > best2)
			{
				best2 = allocated[i + j];
			}
		}

		if (j == w)
		{   /* this is a valid spot */
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > alloc_height)
	{
		return false;
	}

	for (i = 0; i < w; i++)
	{
		allocated[*x + i] = best + h;
	}

	return true;
}

/* returns a texture number and the position inside it */
int
Scrap_AllocBlock(unsigned w, unsigned h, int *x, int *y, const unsigned *pic,
	int scrap_offset)
{
	int texnum;
	w += 2;	// add an empty border to all sides
	h += 2;

	for (texnum = scrap_offset; texnum < MAX_SCRAPS; texnum++)
	{
		if (CommonAllocBlock(scrap_allocated[texnum], SCRAP_WIDTH, SCRAP_HEIGHT,
			w, h, x, y))
		{
			size_t k, i;

			(*x)++;	// jump the border
			(*y)++;

			scrap_dirty[texnum] = true;

			/* copy the texels into the scrap block */
			k = 0;

			for (i = 0; i < h - 2; i++)
			{
				memcpy(&scrap_texels[texnum][(*y + i) * SCRAP_WIDTH + *x],
					pic + k, (w - 2) * sizeof(unsigned));
				k += w - 2;
			}

			return texnum;
		}
	}

	return -1;
}

unsigned *
Scrap_Upload(int texnum)
{
	if (texnum < 0 || texnum >= MAX_SCRAPS || !scrap_dirty[texnum])
	{
		return NULL;
	}

	scrap_dirty[texnum] = false;
	return scrap_texels[texnum];
}

void
Scrap_Init(void)
{
	memset(scrap_allocated, 0, sizeof(scrap_allocated));
	memset(scrap_dirty, 0, sizeof(scrap_dirty));

	/* transparent */
	memset(scrap_texels, 0, sizeof(scrap_texels));
}

