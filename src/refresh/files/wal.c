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
 * The Wal image format
 *
 * =======================================================================
 */

#include "../header/local.h"

image_t *
LoadWal(char *origname)
{
	miptex_t *mt;
	int width, height, ofs;
	image_t *image;
	int len;
	char name[256];

	/* Add the extension */
	len = strlen(origname);

	if (strcmp(origname + len - 4, ".wal"))
	{
		strncpy(name, origname, 256);
		strncat(name, ".wal", 255);
	}
	else
	{
		strncpy(name, origname, 256);
	}

	ri.FS_LoadFile(name, (void **)&mt);

	if (!mt)
	{
		ri.Con_Printf(PRINT_ALL, "LoadWall: can't load %s\n", name);
		return r_notexture;
	}

	width = LittleLong(mt->width);
	height = LittleLong(mt->height);
	ofs = LittleLong(mt->offsets[0]);

	image = R_LoadPic(name, (byte *)mt + ofs, width, 0, height, 0, it_wall, 8);

	ri.FS_FreeFile((void *)mt);

	return image;
}

void
GetWalInfo(char *name, int *width, int *height)
{
	miptex_t *mt;

	ri.FS_LoadFile(name, (void **)&mt);

	if (!mt)
	{
		return;
	}

	*width = LittleLong(mt->width);
	*height = LittleLong(mt->height);

	ri.FS_FreeFile((void *)mt);

	return;
}
