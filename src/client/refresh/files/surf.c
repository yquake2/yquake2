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
