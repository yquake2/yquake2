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
 * .sp2 sprites
 *
 * =======================================================================
 */

#include "../header/local.h"

extern int modfilelen;

void
LoadSP2(model_t *mod, void *buffer)
{
	dsprite_t *sprin, *sprout;
	int i;

	sprin = (dsprite_t *)buffer;
	sprout = Hunk_Alloc(modfilelen);

	sprout->ident = LittleLong(sprin->ident);
	sprout->version = LittleLong(sprin->version);
	sprout->numframes = LittleLong(sprin->numframes);

	if (sprout->version != SPRITE_VERSION)
	{
		VID_Error(ERR_DROP, "%s has wrong version number (%i should be %i)",
				mod->name, sprout->version, SPRITE_VERSION);
	}

	if (sprout->numframes > MAX_MD2SKINS)
	{
		VID_Error(ERR_DROP, "%s has too many frames (%i > %i)",
				mod->name, sprout->numframes, MAX_MD2SKINS);
	}

	/* byte swap everything */
	for (i = 0; i < sprout->numframes; i++)
	{
		sprout->frames[i].width = LittleLong(sprin->frames[i].width);
		sprout->frames[i].height = LittleLong(sprin->frames[i].height);
		sprout->frames[i].origin_x = LittleLong(sprin->frames[i].origin_x);
		sprout->frames[i].origin_y = LittleLong(sprin->frames[i].origin_y);
		memcpy(sprout->frames[i].name, sprin->frames[i].name, MAX_SKINNAME);
		mod->skins[i] = R_FindImage(sprout->frames[i].name,
				it_sprite);
	}

	mod->type = mod_sprite;
}

