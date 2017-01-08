/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016-2017 Daniel Gibson
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
 * Model loading and caching for OpenGL3. Includes the .bsp file format
 *
 * =======================================================================
 */

#include "header/local.h"

enum { MAX_MOD_KNOWN = 512 };

int registration_sequence;

gl3model_t mod_known[MAX_MOD_KNOWN];
int mod_numknown;

void
GL3_Mod_Modellist_f(void)
{
#if 0 // TODO!
	int i;
	model_t *mod;
	int total;

	total = 0;
	R_Printf(PRINT_ALL, "Loaded models:\n");

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!mod->name[0])
		{
			continue;
		}

		R_Printf(PRINT_ALL, "%8i : %s\n", mod->extradatasize, mod->name);
		total += mod->extradatasize;
	}

	R_Printf(PRINT_ALL, "Total resident: %i\n", total);
#endif
}

void
GL3_Mod_Init(void)
{
	STUB("TODO: Implement!");
	// memset(mod_novis, 0xff, sizeof(mod_novis));
}


static void
Mod_Free(gl3model_t *mod)
{
	Hunk_Free(mod->extradata);
	memset(mod, 0, sizeof(*mod));
}

void
GL3_Mod_FreeAll(void)
{
	int i;

	for (i = 0; i < mod_numknown; i++)
	{
		if (mod_known[i].extradatasize)
		{
			Mod_Free(&mod_known[i]);
		}
	}
}


/*
 * Specifies the model that will be used as the world
 */
void
GL3_BeginRegistration(char *model)
{
	char fullname[MAX_QPATH];
	cvar_t *flushmap;

	registration_sequence++;
	gl3_oldviewcluster = -1; /* force markleafs */

	Com_sprintf(fullname, sizeof(fullname), "maps/%s.bsp", model);

	/* explicitly free the old map if different
	   this guarantees that mod_known[0] is the
	   world map */
	flushmap = ri.Cvar_Get("flushmap", "0", 0);

	if (strcmp(mod_known[0].name, fullname) || flushmap->value)
	{
		Mod_Free(&mod_known[0]);
	}

	STUB_ONCE("TODO: Implement Mod_ForName()!");
#if 0 // TODO!
	r_worldmodel = Mod_ForName(fullname, true);
#endif // 0
	gl3_viewcluster = -1;
}

struct model_s *
GL3_RegisterModel(char *name)
{
	STUB_ONCE("TODO: Implement!");
	return NULL;
#if 0
	model_t *mod;
	int i;
	dsprite_t *sprout;
	dmdl_t *pheader;

	mod = Mod_ForName(name, false);

	if (mod)
	{
		mod->registration_sequence = registration_sequence;

		/* register any images used by the models */
		if (mod->type == mod_sprite)
		{
			sprout = (dsprite_t *)mod->extradata;

			for (i = 0; i < sprout->numframes; i++)
			{
				mod->skins[i] = R_FindImage(sprout->frames[i].name, it_sprite);
			}
		}
		else if (mod->type == mod_alias)
		{
			pheader = (dmdl_t *)mod->extradata;

			for (i = 0; i < pheader->num_skins; i++)
			{
				mod->skins[i] = R_FindImage((char *)pheader + pheader->ofs_skins +
					   	i * MAX_SKINNAME, it_skin);
			}

			mod->numframes = pheader->num_frames;
		}
		else if (mod->type == mod_brush)
		{
			for (i = 0; i < mod->numtexinfo; i++)
			{
				mod->texinfo[i].image->registration_sequence =
					registration_sequence;
			}
		}
	}

	return mod;
#endif // 0
}

void
GL3_EndRegistration(void)
{
	int i;
	gl3model_t *mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!mod->name[0])
		{
			continue;
		}

		if (mod->registration_sequence != registration_sequence)
		{
			/* don't need this model */
			Mod_Free(mod);
		}
	}

	GL3_FreeUnusedImages();
}
