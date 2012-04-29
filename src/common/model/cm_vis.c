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
 * This file implements loading an parsing of the vis of a map or model
 *
 * =======================================================================
 */

#include "../header/common.h"
#include "../header/cmodel.h"

extern int numclusters;

dvis_t *map_vis = (dvis_t *)map_visibility;

void CM_DecompressVis (byte *in, byte *out)
{
	int		c;
	byte	*out_p;
	int		row;

	row = (numclusters+7)>>3;
	out_p = out;

	if (!in || !numvisibility)
	{
		/* no vis info, so make all visible */
		while (row)
		{
			*out_p++ = 0xff;
			row--;
		}

		return;
	}

	do
	{
		if (*in)
		{
			*out_p++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;

		if ((out_p - out) + c > row)
		{
			c = row - (out_p - out);
			Com_DPrintf ("warning: Vis decompression overrun\n");
		}

		while (c)
		{
			*out_p++ = 0;
			c--;
		}
	}
	while (out_p - out < row);
}

byte pvsrow[MAX_MAP_LEAFS/8];
byte phsrow[MAX_MAP_LEAFS/8];

byte *CM_ClusterPVS (int cluster)
{
	if (cluster == -1)
		memset (pvsrow, 0, (numclusters+7)>>3);

	else
		CM_DecompressVis (map_visibility + LittleLong(map_vis->bitofs[cluster][DVIS_PVS]), pvsrow);

	return pvsrow;
}

byte	*CM_ClusterPHS (int cluster)
{
	if (cluster == -1)
		memset (phsrow, 0, (numclusters+7)>>3);

	else
		CM_DecompressVis (map_visibility + LittleLong(map_vis->bitofs[cluster][DVIS_PHS]), phsrow);

	return phsrow;
}
