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
 * Surface logic
 *
 * =======================================================================
 */

#include "../ref_shared.h"

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
struct image_s *
R_TextureAnimation(const entity_t *currententity, const mtexinfo_t *tex)
{
	int c;

	if (!tex->next)
		return tex->image;

	if (!currententity)
		return tex->image;

	c = currententity->frame % tex->numframes;
	while (c && tex)
	{
		tex = tex->next;
		c--;
	}

	return tex->image;
}

qboolean
R_AreaVisible(const byte *areabits, mleaf_t *pleaf)
{
	int area;

	// check for door connected areas
	if (!areabits)
		return true;

	area = pleaf->area;

	if ((areabits[area >> 3] & (1 << (area & 7))))
		return true;

	return false; // not visible
}

/*
=============
R_MarkLights

bit: 1 << i for light number i, will be or'ed into msurface_t::dlightbits
if surface is affected by this light
=============
*/
void
R_MarkLights(dlight_t *light, int bit, mnode_t *node, int r_dlightframecount,
	marksurfacelights_t mark_surface_lights)
{
	cplane_t	*splitplane;
	float		dist;
	int			intensity;

	if (node->contents != CONTENTS_NODE)
		return;

	splitplane = node->plane;
	dist = DotProduct(light->origin, splitplane->normal) - splitplane->dist;

	intensity = light->intensity;

	if (dist > intensity - DLIGHT_CUTOFF)	// (dist > light->intensity)
	{
		R_MarkLights (light, bit, node->children[0], r_dlightframecount,
			mark_surface_lights);
		return;
	}

	if (dist < -intensity + DLIGHT_CUTOFF)	// (dist < -light->intensity)
	{
		R_MarkLights(light, bit, node->children[1], r_dlightframecount,
			mark_surface_lights);
		return;
	}

	mark_surface_lights(light, bit, node, r_dlightframecount);

	R_MarkLights(light, bit, node->children[0], r_dlightframecount,
		mark_surface_lights);
	R_MarkLights(light, bit, node->children[1], r_dlightframecount,
		mark_surface_lights);
}
