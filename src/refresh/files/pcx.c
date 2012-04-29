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
 * The PCX file format
 *
 * =======================================================================
 */

#include "../header/local.h"

void
LoadPCX(char *origname, byte **pic, byte **palette, int *width, int *height)
{
	byte *raw;
	pcx_t *pcx;
	int x, y;
	int len;
	int filelen;
	int dataByte, runLength;
	byte *out, *pix;
	char filename[256];

	/* Add the extension */
	filelen = strlen(origname);

	if (strcmp(origname + filelen - 4, ".pcx"))
	{
		strncpy(filename, origname, 256);
		strncat(filename, ".pcx", 255);
	}
	else
	{
		strncpy(filename, origname, 256);
	}

	*pic = NULL;
	*palette = NULL;

	/* load the file */
	len = ri.FS_LoadFile(filename, (void **)&raw);

	if (!raw)
	{
		ri.Con_Printf(PRINT_DEVELOPER, "Bad pcx file %s\n", filename);
		return;
	}

	/* parse the PCX file */
	pcx = (pcx_t *)raw;

	pcx->xmin = LittleShort(pcx->xmin);
	pcx->ymin = LittleShort(pcx->ymin);
	pcx->xmax = LittleShort(pcx->xmax);
	pcx->ymax = LittleShort(pcx->ymax);
	pcx->hres = LittleShort(pcx->hres);
	pcx->vres = LittleShort(pcx->vres);
	pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
	pcx->palette_type = LittleShort(pcx->palette_type);

	raw = &pcx->data;

	if ((pcx->manufacturer != 0x0a) || (pcx->version != 5) || (pcx->encoding != 1) || (pcx->bits_per_pixel != 8) || (pcx->xmax >= 640) || (pcx->ymax >= 480))
	{
		ri.Con_Printf(PRINT_ALL, "Bad pcx file %s\n", filename);
		return;
	}

	out = malloc((pcx->ymax + 1) * (pcx->xmax + 1));

	*pic = out;

	pix = out;

	if (palette)
	{
		*palette = malloc(768);
		memcpy(*palette, (byte *)pcx + len - 768, 768);
	}

	if (width)
	{
		*width = pcx->xmax + 1;
	}

	if (height)
	{
		*height = pcx->ymax + 1;
	}

	for (y = 0; y <= pcx->ymax; y++, pix += pcx->xmax + 1)
	{
		for (x = 0; x <= pcx->xmax; )
		{
			dataByte = *raw++;

			if ((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
			{
				runLength = 1;
			}

			while (runLength-- > 0)
			{
				pix[x++] = dataByte;
			}
		}
	}

	if (raw - (byte *)pcx > len)
	{
		ri.Con_Printf(PRINT_DEVELOPER, "PCX file %s was malformed", filename);
		free(*pic);
		*pic = NULL;
	}

	ri.FS_FreeFile(pcx);
}

void
GetPCXInfo(char *filename, int *width, int *height)
{
	pcx_t *pcx;
	byte *raw;

	ri.FS_LoadFile(filename, (void **)&raw);

	if (!raw)
	{
		return;
	}

	pcx = (pcx_t *)raw;

	*width = pcx->xmax + 1;
	*height = pcx->ymax + 1;

	ri.FS_FreeFile(raw);

	return;
}
