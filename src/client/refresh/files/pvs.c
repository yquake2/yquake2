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
 * The PVS Decompress
 *
 * =======================================================================
 */

#include "../ref_shared.h"


/*
===================
Mod_DecompressVis
===================
*/
byte *
Mod_DecompressVis(byte *in, int row)
{
	static byte decompressed[MAX_MAP_LEAFS / 8];
	int c;
	byte *out;

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

float
Mod_RadiusFromBounds(const vec3_t mins, const vec3_t maxs)
{
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return VectorLength(corner);
}
