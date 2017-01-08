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
 * Warps. Used on water surfaces und for skybox rotation.
 *
 * =======================================================================
 */

#include "header/local.h"

static float skyrotate;
static vec3_t skyaxis;
static gl3image_t* sky_images[6];
static float sky_min, sky_max;

/* 3dstudio environment map names */
static const char* suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

void
GL3_SetSky(char *name, float rotate, vec3_t axis)
{
	int i;
	char pathname[MAX_QPATH];
	char skyname[MAX_QPATH];

	Q_strlcpy(skyname, name, sizeof(skyname));
	skyrotate = rotate;
	VectorCopy(axis, skyaxis);

	for (i = 0; i < 6; i++)
	{

		// NOTE: there might be a paletted .pcx version, which was only used
		//       if gl_config.palettedtexture
		Com_sprintf(pathname, sizeof(pathname), "env/%s%s.tga", skyname, suf[i]);

		sky_images[i] = GL3_FindImage(pathname, it_sky);

		if (!sky_images[i])
		{
			sky_images[i] = gl3_notexture;
		}

		sky_min = 1.0 / 512;
		sky_max = 511.0 / 512;
	}
}
