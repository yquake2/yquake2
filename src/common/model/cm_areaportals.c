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
 * This file implements the the area portals.
 *
 * =======================================================================
 */

#include "../header/common.h"
#include "../header/cmodel.h"

extern int numareas;

void FloodArea_r (carea_t *area, int floodnum)
{
	int		i;
	dareaportal_t	*p;

	if (area->floodvalid == floodvalid)
	{
		if (area->floodnum == floodnum)
			return;

		Com_Error (ERR_DROP, "FloodArea_r: reflooded");
	}

	area->floodnum = floodnum;
	area->floodvalid = floodvalid;
	p = &map_areaportals[area->firstareaportal];

	for (i=0 ; i<area->numareaportals ; i++, p++)
	{
		if (portalopen[LittleLong(p->portalnum)])
			FloodArea_r (&map_areas[LittleLong(p->otherarea)], floodnum);
	}
}

void FloodAreaConnections (void)
{
	int		i;
	carea_t	*area;
	int		floodnum;

	/* all current floods are now invalid */
	floodvalid++;
	floodnum = 0;

	/* area 0 is not used */
	for (i=1 ; i<numareas ; i++)
	{
		area = &map_areas[i];

		if (area->floodvalid == floodvalid)
			continue; /* already flooded into */

		floodnum++;
		FloodArea_r (area, floodnum);
	}

}

void CM_SetAreaPortalState (int portalnum, qboolean open)
{
	if (portalnum > numareaportals)
		Com_Error (ERR_DROP, "areaportal > numareaportals");

	portalopen[portalnum] = open;
	FloodAreaConnections ();
}

qboolean CM_AreasConnected (int area1, int area2)
{
	if (map_noareas->value)
		return true;

	if (area1 > numareas || area2 > numareas)
		Com_Error (ERR_DROP, "area > numareas");

	if (map_areas[area1].floodnum == map_areas[area2].floodnum)
		return true;

	return false;
}

/*
 * Writes a length byte followed by a bit vector of all the areas
 * that area in the same flood as the area parameter
 *
 * This is used by the client refreshes to cull visibility
 */
int CM_WriteAreaBits (byte *buffer, int area)
{
	int		i;
	int		floodnum;
	int		bytes;

	bytes = (numareas+7)>>3;

	if (map_noareas->value)
	{
		/* for debugging, send everything */
		memset (buffer, 255, bytes);
	}

	else
	{
		memset (buffer, 0, bytes);

		floodnum = map_areas[area].floodnum;

		for (i=0 ; i<numareas ; i++)
		{
			if (map_areas[i].floodnum == floodnum || !area)
				buffer[i>>3] |= 1<<(i&7);
		}
	}

	return bytes;
}

/*
 * Writes the portal state to a savegame file
 */
void CM_WritePortalState (FILE *f)
{
	fwrite (portalopen, sizeof(portalopen), 1, f);
}

/*
 * Reads the portal state from a savegame file
 * and recalculates the area connections
 */
void CM_ReadPortalState (fileHandle_t f)
{
	FS_Read (portalopen, sizeof(portalopen), f);
	FloodAreaConnections ();
}

/*
 * Returns true if any leaf under headnode has a cluster that
 * is potentially visible
 */
qboolean CM_HeadnodeVisible (int nodenum, byte *visbits)
{
	int		leafnum1;
	int		cluster;
	cnode_t	*node;

	if (nodenum < 0)
	{
		leafnum1 = -1-nodenum;
		cluster = map_leafs[leafnum1].cluster;

		if (cluster == -1)
			return false;

		if (visbits[cluster>>3] & (1<<(cluster&7)))
			return true;

		return false;
	}

	node = &map_nodes[nodenum];

	if (CM_HeadnodeVisible(node->children[0], visbits))
		return true;

	return CM_HeadnodeVisible(node->children[1], visbits);
}
