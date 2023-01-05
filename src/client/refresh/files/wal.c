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

#include "../ref_shared.h"

struct image_s *
LoadWal(const char *origname, imagetype_t type, loadimage_t load_image)
{
	int	width, height, ofs, size;
	struct image_s	*image;
	char	name[256];
	miptex_t	*mt;

	FixFileExt(origname, "wal", name, sizeof(name));

	size = ri.FS_LoadFile(name, (void **)&mt);

	if (!mt)
	{
		return NULL;
	}

	if (size < sizeof(miptex_t))
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, small header\n", __func__, name);
		ri.FS_FreeFile((void *)mt);
		return NULL;
	}

	width = LittleLong(mt->width);
	height = LittleLong(mt->height);
	ofs = LittleLong(mt->offsets[0]);

	if ((ofs <= 0) || (width <= 0) || (height <= 0) ||
	    (((size - ofs) / height) < width))
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, small body\n", __func__, name);
		ri.FS_FreeFile((void *)mt);
		return NULL;
	}

	image = load_image(name, (byte *)mt + ofs,
		width, 0,
		height, 0,
		(size - ofs), type, 8);

	ri.FS_FreeFile((void *)mt);

	return image;
}

struct image_s *
LoadM8(const char *origname, imagetype_t type, loadimage_t load_image)
{
	m8tex_t *mt;
	int width, height, ofs, size, i;
	struct image_s *image;
	char name[256];
	unsigned char *image_buffer = NULL;

	FixFileExt(origname, "m8", name, sizeof(name));

	size = ri.FS_LoadFile(name, (void **)&mt);

	if (!mt)
	{
		return NULL;
	}

	if (size < sizeof(m8tex_t))
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, small header\n", __func__, name);
		ri.FS_FreeFile((void *)mt);
		return NULL;
	}

	if (LittleLong (mt->version) != M8_VERSION)
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, wrong magic value.\n", __func__, name);
		ri.FS_FreeFile ((void *)mt);
		return NULL;
	}

	width = LittleLong(mt->width[0]);
	height = LittleLong(mt->height[0]);
	ofs = LittleLong(mt->offsets[0]);

	if ((ofs <= 0) || (width <= 0) || (height <= 0) ||
	    (((size - ofs) / height) < width))
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, small body\n", __func__, name);
		ri.FS_FreeFile((void *)mt);
		return NULL;
	}

	image_buffer = malloc ((size - ofs) * 4);
	for(i=0; i<(size - ofs); i++)
	{
		unsigned char value = *((byte *)mt + ofs + i);
		image_buffer[i * 4 + 0] = mt->palette[value].r;
		image_buffer[i * 4 + 1] = mt->palette[value].g;
		image_buffer[i * 4 + 2] = mt->palette[value].b;
		image_buffer[i * 4 + 3] = value == 255 ? 0 : 255;
	}

	image = load_image(name, image_buffer,
		width, 0,
		height, 0,
		(size - ofs), type, 32);
	free(image_buffer);

	ri.FS_FreeFile((void *)mt);

	return image;
}

struct image_s *
LoadM32(const char *origname, imagetype_t type, loadimage_t load_image)
{
	m32tex_t	*mt;
	int		width, height, ofs, size;
	struct image_s	*image;
	char name[256];

	FixFileExt(origname, "m32", name, sizeof(name));

	size = ri.FS_LoadFile(name, (void **)&mt);

	if (!mt)
	{
		return NULL;
	}

	if (size < sizeof(m32tex_t))
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, small header\n", __func__, name);
		ri.FS_FreeFile((void *)mt);
		return NULL;
	}

	if (LittleLong (mt->version) != M32_VERSION)
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, wrong magic value.\n", __func__, name);
		ri.FS_FreeFile ((void *)mt);
		return NULL;
	}

	width = LittleLong (mt->width[0]);
	height = LittleLong (mt->height[0]);
	ofs = LittleLong (mt->offsets[0]);

	if ((ofs <= 0) || (width <= 0) || (height <= 0) ||
	    (((size - ofs) / height) < (width * 4)))
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, small body\n", __func__, name);
		ri.FS_FreeFile((void *)mt);
		return NULL;
	}

	image = load_image(name, (byte *)mt + ofs,
		width, 0,
		height, 0,
		(size - ofs) / 4, type, 32);
	ri.FS_FreeFile ((void *)mt);

	return image;
}

void
GetWalInfo(const char *origname, int *width, int *height)
{
	miptex_t *mt;
	int size;
	char filename[256];

	FixFileExt(origname, "wal", filename, sizeof(filename));

	size = ri.FS_LoadFile(filename, (void **)&mt);

	if (!mt)
	{
		return;
	}

	if (size < sizeof(miptex_t))
	{
		ri.FS_FreeFile((void *)mt);
		return;
	}

	*width = LittleLong(mt->width);
	*height = LittleLong(mt->height);

	ri.FS_FreeFile((void *)mt);

	return;
}

void
GetM8Info(const char *origname, int *width, int *height)
{
	m8tex_t *mt;
	int size;
	char filename[256];

	FixFileExt(origname, "m8", filename, sizeof(filename));

	size = ri.FS_LoadFile(filename, (void **)&mt);

	if (!mt)
	{
		return;
	}

	if (size < sizeof(m8tex_t) || LittleLong (mt->version) != M8_VERSION)
	{
		ri.FS_FreeFile((void *)mt);
		return;
	}

	*width = LittleLong(mt->width[0]);
	*height = LittleLong(mt->height[0]);

	ri.FS_FreeFile((void *)mt);

	return;
}

void
GetM32Info(const char *origname, int *width, int *height)
{
	m32tex_t *mt;
	int size;
	char filename[256];

	FixFileExt(origname, "m32", filename, sizeof(filename));

	size = ri.FS_LoadFile(filename, (void **)&mt);

	if (!mt)
	{
		return;
	}

	if (size < sizeof(m32tex_t) || LittleLong (mt->version) != M32_VERSION)
	{
		ri.FS_FreeFile((void *)mt);
		return;
	}

	*width = LittleLong(mt->width[0]);
	*height = LittleLong(mt->height[0]);

	ri.FS_FreeFile((void *)mt);

	return;
}
