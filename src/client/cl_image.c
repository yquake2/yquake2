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
 * This file implements .pcx bitmap decoder.
 *
 * =======================================================================
 */

#include "header/client.h"

#define PCX_IDENT ((0x05 << 8) + 0x0a)

// don't need HDR stuff
#define STBI_NO_LINEAR
#define STBI_NO_HDR
// make sure STB_image uses standard malloc(), as we'll use standard free() to deallocate
#define STBI_MALLOC(sz)    malloc(sz)
#define STBI_REALLOC(p,sz) realloc(p,sz)
#define STBI_FREE(p)       free(p)
// Switch of the thread local stuff. Breaks mingw under Windows.
#define STBI_NO_THREAD_LOCALS
// include implementation part of stb_image into this file
#define STB_IMAGE_IMPLEMENTATION
#include "refresh/files/stb_image.h"

static const byte *
PCX_RLE_Decode(byte *pix, byte *pix_max, const byte *raw, const byte *raw_max,
	int bytes_per_line, qboolean *image_issues)
{
	int x;

	for (x = 0; x < bytes_per_line; )
	{
		int runLength;
		byte dataByte;

		if (raw >= raw_max)
		{
			// no place for read
			*image_issues = true;
			return raw;
		}
		dataByte = *raw++;

		if ((dataByte & 0xC0) == 0xC0)
		{
			runLength = dataByte & 0x3F;
			if (raw >= raw_max)
			{
				// no place for read
				*image_issues = true;
				return raw;
			}
			dataByte = *raw++;
		}
		else
		{
			runLength = 1;
		}

		while (runLength-- > 0)
		{
			if (pix_max <= (pix + x))
			{
				// no place for write
				*image_issues = true;
				return raw;
			}
			else
			{
				pix[x++] = dataByte;
			}
		}
	}
	return raw;
}

static void
PCX_Decode(const char *name, const byte *raw, int len, byte **pic, byte **palette,
	int *width, int *height, int *bitsPerPixel)
{
	const pcx_t *pcx;
	int full_size;
	int pcx_width, pcx_height, bytes_per_line;
	qboolean image_issues = false;
	byte *out, *pix;
	const byte *data;

	*pic = NULL;
	*bitsPerPixel = 8;

	if (palette)
	{
		*palette = NULL;
	}

	if (len < sizeof(pcx_t))
	{
		return;
	}

	/* parse the PCX file */
	pcx = (const pcx_t *)raw;

	data = &pcx->data;

	bytes_per_line = LittleShort(pcx->bytes_per_line);
	pcx_width = LittleShort(pcx->xmax) - LittleShort(pcx->xmin);
	pcx_height = LittleShort(pcx->ymax) - LittleShort(pcx->ymin);

	if ((pcx->manufacturer != 0x0a) ||
		(pcx->version != 5) ||
		(pcx->encoding != 1) ||
		(pcx_width <= 0) ||
		(pcx_height <= 0) ||
		(bytes_per_line <= 0) ||
		(pcx->color_planes <= 0) ||
		(pcx->bits_per_pixel <= 0))
	{
		Com_Printf("%s: Bad pcx file %s: version: %d:%d, encoding: %d\n",
			__func__, name, pcx->manufacturer, pcx->version, pcx->encoding);
		return;
	}

	full_size = (pcx_height + 1) * (pcx_width + 1);
	if ((pcx->color_planes == 3 || pcx->color_planes == 4)
		&& pcx->bits_per_pixel == 8)
	{
		full_size *= 4;
		*bitsPerPixel = 32;
	}

	out = malloc(full_size);
	if (!out)
	{
		Com_Printf("%s: Can't allocate for %s\n", __func__, name);
		return;
	}

	*pic = out;

	pix = out;

	if (width)
	{
		*width = pcx_width + 1;
	}

	if (height)
	{
		*height = pcx_height + 1;
	}

	if ((pcx->color_planes == 3 || pcx->color_planes == 4)
		&& pcx->bits_per_pixel == 8)
	{
		int x, y, linesize;
		byte *line;

		if (bytes_per_line <= pcx_width)
		{
			image_issues = true;
		}

		/* clean image alpha */
		memset(pix, 255, full_size);

		linesize = Q_max(bytes_per_line, pcx_width + 1) * pcx->color_planes;
		line = malloc(linesize);

		for (y = 0; y <= pcx_height; y++, pix += (pcx_width + 1) * 4)
		{
			data = PCX_RLE_Decode(line, line + linesize,
				data, (byte *)pcx + len,
				bytes_per_line * pcx->color_planes, &image_issues);

			for (x = 0; x <= pcx_width; x++) {
				int j;

				for (j = 0; j < pcx->color_planes; j++)
				{
					pix[4 * x + j] = line[x + bytes_per_line * j];
				}
			}
		}

		free(line);
	}
	else if (pcx->bits_per_pixel == 1)
	{
		byte *line;
		int y;

		if (palette)
		{
			*palette = malloc(768);

			if (!(*palette))
			{
				Com_Printf("%s: Can't allocate for %s\n", __func__, name);
				free(out);
				return;
			}

			memcpy(*palette, pcx->palette, sizeof(pcx->palette));
		}

		line = malloc(bytes_per_line * pcx->color_planes);

		for (y = 0; y <= pcx_height; y++, pix += pcx_width + 1)
		{
			int x;

			data = PCX_RLE_Decode(line, line + bytes_per_line * pcx->color_planes,
				data, (byte *)pcx + len,
				bytes_per_line * pcx->color_planes, &image_issues);

			for (x = 0; x <= pcx_width; x++)
			{
				int m, i, v;

				m = 0x80 >> (x & 7);
				v = 0;

				for (i = pcx->color_planes - 1; i >= 0; i--) {
					v <<= 1;
					v += (line[i * bytes_per_line + (x >> 3)] & m) ? 1 : 0;
				}
				pix[x] = v;
			}
		}
		free(line);
	}
	else if (pcx->color_planes == 1 && pcx->bits_per_pixel == 8)
	{
		int y, linesize;
		byte *line;

		if (palette)
		{
			*palette = malloc(768);

			if (!(*palette))
			{
				Com_Printf("%s: Can't allocate for %s\n", __func__, name);
				free(out);
				return;
			}

			if (len > 768)
			{
				memcpy(*palette, (byte *)pcx + len - 768, 768);

				if ((((byte *)pcx)[len - 769] != 0x0C))
				{
					Com_DPrintf("%s: %s has no palette marker\n",
						__func__, name);
				}
			}
			else
			{
				image_issues = true;
			}
		}

		if (bytes_per_line <= pcx_width)
		{
			image_issues = true;
		}

		linesize = Q_max(bytes_per_line, pcx_width + 1);
		line = malloc(linesize);
		for (y = 0; y <= pcx_height; y++, pix += pcx_width + 1)
		{
			data = PCX_RLE_Decode(line, line + linesize,
				data, (byte *)pcx + len,
				bytes_per_line, &image_issues);
			/* copy only visible part */
			memcpy(pix, line, pcx_width + 1);
		}
		free(line);
	}
	else if (pcx->color_planes == 1 &&
		(pcx->bits_per_pixel == 2 || pcx->bits_per_pixel == 4))
	{
		int y;

		byte *line;

		if (palette)
		{
			*palette = malloc(768);

			if (!(*palette))
			{
				Com_Printf("%s: Can't allocate for %s\n", __func__, name);
				free(out);
				return;
			}

			memcpy(*palette, pcx->palette, sizeof(pcx->palette));
		}

		line = malloc(bytes_per_line);

		for (y = 0; y <= pcx_height; y++, pix += pcx_width + 1)
		{
			int x, mask, div;

			data = PCX_RLE_Decode(line, line + bytes_per_line,
				data, (byte *)pcx + len,
				bytes_per_line, &image_issues);

			mask = (1 << pcx->bits_per_pixel) - 1;
			div = 8 / pcx->bits_per_pixel;

			for (x = 0; x <= pcx_width; x++)
			{
				unsigned v, shift;

				v = line[x / div] & 0xFF;
				/* for 2 bits:
				 * 0 -> 6
				 * 1 -> 4
				 * 3 -> 2
				 * 4 -> 0
				 */
				shift = pcx->bits_per_pixel * ((div - 1) - x % div);
				pix[x] = (v >> shift) & mask;
			}
		}

		free(line);
	}
	else
	{
		Com_Printf("%s: Bad pcx file %s: planes: %d, bits: %d\n",
			__func__, name, pcx->color_planes, pcx->bits_per_pixel);
		free(*pic);
		*pic = NULL;
	}

	if (pcx->color_planes != 1 || pcx->bits_per_pixel != 8)
	{
		Com_DPrintf("%s: %s has uncommon flags, "
			"could be unsupported by other engines\n",
			__func__, name);
	}

	if (data - (byte *)pcx > len)
	{
		Com_DPrintf("%s: %s file was malformed\n", __func__, name);
		free(*pic);
		*pic = NULL;
	}

	if (image_issues)
	{
		Com_Printf("%s: %s file has possible size issues.\n", __func__, name);
	}
}

void
SCR_LoadImageWithPalette(char *filename, byte **pic, byte **palette,
	int *width, int *height, int *bitsPerPixel)
{
	const char* ext;
	int len, ident;
	byte *raw;

	ext = COM_FileExtension(filename);
	*pic = NULL;

	/* load the file */
	len = FS_LoadFile(filename, (void **)&raw);

	if (!raw || len <= 0)
	{
		return;
	}

	if (len <= sizeof(int))
	{
		FS_FreeFile(raw);
		return;
	}

	ident = LittleShort(*((short*)raw));
	if (!strcmp(ext, "pcx") && (ident == PCX_IDENT))
	{
		PCX_Decode(filename, raw, len, pic, palette, width, height, bitsPerPixel);
	}
	else
	{
		int sourcebitsPerPixel = 0;

		/* other formats does not have palette directly */
		if (palette)
		{
			*palette = NULL;
		}

		*pic = stbi_load_from_memory(raw, len, width, height,
			&sourcebitsPerPixel, STBI_rgb_alpha);

		if (*pic == NULL)
		{
			Com_DPrintf("%s couldn't load data from %s: %s!\n",
				__func__, filename, stbi_failure_reason());
		}

		*bitsPerPixel = 32;
	}

	FS_FreeFile(raw);
}
