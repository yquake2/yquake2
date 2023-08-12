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

#include "../ref_shared.h"

// Fix Jennell Jaquays' name in the Quitscreen
// this is 98x11 pixels, each value an index
// into the standard baseq2/pak0/pics/quit.pcx colormap
static unsigned char quitscreenfix[] = {
	191,191,191,47,28,39,4,4,39,1,47,28,47,28,29,1,
	28,28,47,31,31,1,29,31,1,28,47,47,47,47,29,28,
	47,31,30,28,40,40,4,28,28,40,39,40,29,102,102,245,
	28,39,4,4,39,103,40,40,1,1,102,94,47,47,1,94,
	94,94,94,47,102,245,103,103,103,47,1,102,1,102,29,29,
	29,29,47,28,245,31,31,31,47,1,28,1,28,47,1,102, 102,102,
	191,191,142,47,4,8,8,8,8,4,47,28,1,28,29,28,
	29,29,31,1,47,245,47,47,28,28,31,47,28,1,31,1,
	1,245,47,39,8,8,8,40,39,8,8,8,39,1,1,47,
	4,8,8,8,8,4,47,29,28,31,28,28,29,28,28,28,
	29,28,31,28,47,29,1,28,31,47,1,28,1,1,29,29,
	29,47,28,1,28,28,245,28,28,28,28,47,29,28,47,102,102,103,
	191,191,142,31,29,36,8,8,36,31,40,39,40,4,1,1,
	39,40,39,40,40,31,28,40,40,4,39,40,28,47,31,40,
	39,40,4,1,36,8,8,4,47,36,8,8,39,1,1,1,
	29,36,8,8,36,4,4,39,40,4,47,1,47,40,40,39,
	39,40,28,40,40,47,45,39,40,28,4,39,40,4,39,1,
	28,4,40,28,28,4,39,28,47,40,40,39,40,39,28,28,1,103,
	1,142,29,142,28,39,8,8,36,36,8,8,8,8,36,1,
	8,8,8,8,8,36,39,8,8,8,8,8,36,40,36,8,
	8,8,8,36,40,8,8,40,1,4,8,8,40,1,1,31,
	28,39,8,8,36,8,8,8,8,8,36,31,36,8,8,8,
	8,8,36,8,8,4,40,8,8,36,8,8,8,8,8,36,
	40,8,8,40,39,8,8,40,36,8,8,8,8,8,39,29,28,29,
	103,191,142,47,28,40,8,8,40,8,8,33,33,8,8,36,
	8,8,36,36,8,8,36,8,8,36,36,8,8,36,8,8,
	33,33,8,8,36,8,8,4,47,40,8,8,39,47,28,245,
	28,40,8,8,40,40,36,36,33,8,8,36,8,8,36,36,
	8,8,36,8,8,40,40,8,8,40,4,36,36,33,8,8,
	36,8,8,39,39,8,8,36,8,8,33,36,36,39,28,1,47,28,
	103,246,1,47,1,39,8,8,40,8,8,8,8,8,8,36,
	8,8,4,40,8,8,36,8,8,40,4,8,8,36,8,8,
	8,8,8,8,36,8,8,40,29,39,8,8,39,1,1,47,
	1,39,8,8,40,36,8,8,8,8,8,36,8,8,4,40,
	8,8,36,8,8,40,39,8,8,40,36,8,8,8,8,8,
	36,8,8,39,40,8,8,40,36,8,8,8,8,36,28,1,1,29,
	103,47,40,40,4,36,8,8,36,8,8,33,36,36,36,4,
	8,8,39,4,8,8,36,8,8,4,40,8,8,36,8,8,
	33,36,36,36,36,8,8,40,31,40,8,8,40,47,40,40,
	4,36,8,8,36,8,8,33,33,8,8,36,8,8,36,36,
	8,8,36,8,8,36,36,8,8,36,8,8,33,33,8,8,
	36,8,8,36,36,8,8,4,39,36,36,33,8,8,4,40,4,31,
	191,40,8,8,8,8,8,36,29,36,8,8,8,8,8,40,
	8,8,40,4,8,8,36,8,8,40,39,8,8,39,36,8,
	8,8,8,8,39,8,8,39,45,4,8,8,40,40,8,8,
	8,8,8,36,29,36,8,8,8,8,8,40,36,8,8,8,
	8,8,40,36,8,8,8,8,8,40,36,8,8,8,8,8,
	40,36,8,8,8,8,8,36,8,8,8,8,8,36,4,8,8,4,
	47,45,40,39,40,39,39,245,246,1,40,40,40,39,4,47,
	40,4,28,29,39,40,30,39,39,1,28,40,4,28,1,40,
	40,40,39,4,29,40,39,1,1,1,4,4,47,45,40,39,
	40,39,39,245,246,29,39,40,40,40,4,47,28,39,39,36,
	8,8,4,1,39,40,4,40,40,1,29,4,39,4,40,39,
	1,39,36,36,33,8,8,4,39,4,39,4,40,47,36,8,8,40,
	1,28,47,28,28,29,1,28,47,28,31,28,28,27,47,28,
	45,246,30,28,245,29,47,47,29,30,28,47,27,1,246,47,
	47,47,1,28,47,28,47,1,47,47,1,29,29,47,47,28,
	28,29,1,47,1,47,47,28,31,47,47,31,47,47,47,4,
	8,8,39,245,1,47,28,245,28,47,31,28,47,28,28,28,
	40,8,8,8,8,8,36,47,28,1,246,47,1,40,8,8,36,1,
	47,1,102,1,102,102,47,94,94,102,47,47,102,102,102,102,
	94,1,94,47,102,1,102,47,30,30,102,27,47,102,94,1,
	102,47,1,94,102,103,1,102,103,103,47,47,47,29,1,29,
	28,28,29,28,1,47,28,31,29,1,47,29,28,1,1,47,
	4,39,1,47,47,1,28,28,28,47,1,28,45,28,47,47,
	1,40,4,4,40,4,29,28,31,45,47,28,47,47,4,40,28,28
};

static void
fixQuitScreen(byte* px)
{
	// overwrite 11 lines, 98 pixels each, from quitscreenfix[]
	// starting at line 140, column 188
	// quitscreen is 320x240 px
	int r, qsIdx = 0;

	px += 140*320; // go to line 140
	px += 188; // to colum 188
	for(r=0; r<11; ++r)
	{
		memcpy(px, quitscreenfix+qsIdx, 98);
		qsIdx += 98;
		px += 320;
	}
}

void
LoadPCX(const char *origname, byte **pic, byte **palette, int *width, int *height)
{
	byte *raw;
	pcx_t *pcx;
	int x, y;
	int len, full_size;
	int pcx_width, pcx_height;
	qboolean image_issues = false;
	int dataByte, runLength;
	byte *out, *pix;
	char filename[256];

	FixFileExt(origname, "pcx", filename, sizeof(filename));

	*pic = NULL;

	if (palette)
	{
		*palette = NULL;
	}

	/* load the file */
	len = ri.FS_LoadFile(filename, (void **)&raw);

	if (!raw || len < sizeof(pcx_t))
	{
		R_Printf(PRINT_DEVELOPER, "Bad pcx file %s\n", filename);
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

	pcx_width = pcx->xmax - pcx->xmin;
	pcx_height = pcx->ymax - pcx->ymin;

	if ((pcx->manufacturer != 0x0a) || (pcx->version != 5) ||
		(pcx->encoding != 1) || (pcx->bits_per_pixel != 8) ||
		(pcx_width >= 4096) || (pcx_height >= 4096))
	{
		R_Printf(PRINT_ALL, "Bad pcx file %s\n", filename);
		ri.FS_FreeFile(pcx);
		return;
	}

	full_size = (pcx_height + 1) * (pcx_width + 1);
	out = malloc(full_size);
	if (!out)
	{
		R_Printf(PRINT_ALL, "Can't allocate\n");
		ri.FS_FreeFile(pcx);
		return;
	}

	*pic = out;

	pix = out;

	if (palette)
	{
		*palette = malloc(768);
		if (!(*palette))
		{
			R_Printf(PRINT_ALL, "Can't allocate\n");
			free(out);
			ri.FS_FreeFile(pcx);
			return;
		}
		if (len > 768)
		{
			memcpy(*palette, (byte *)pcx + len - 768, 768);
		}
		else
		{
			image_issues = true;
		}
	}

	if (width)
	{
		*width = pcx_width + 1;
	}

	if (height)
	{
		*height = pcx_height + 1;
	}

	for (y = 0; y <= pcx_height; y++, pix += pcx_width + 1)
	{
		for (x = 0; x <= pcx_width; )
		{
			if (raw - (byte *)pcx > len)
			{
				// no place for read
				image_issues = true;
				x = pcx_width;
				break;
			}
			dataByte = *raw++;

			if ((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				if (raw - (byte *)pcx > len)
				{
					// no place for read
					image_issues = true;
					x = pcx_width;
					break;
				}
				dataByte = *raw++;
			}
			else
			{
				runLength = 1;
			}

			while (runLength-- > 0)
			{
				if ((*pic + full_size) <= (pix + x))
				{
					// no place for write
					image_issues = true;
					x += runLength;
					runLength = 0;
				}
				else
				{
					pix[x++] = dataByte;
				}
			}
		}
	}

	if (raw - (byte *)pcx > len)
	{
		R_Printf(PRINT_DEVELOPER, "PCX file %s was malformed", filename);
		free(*pic);
		*pic = NULL;
	}
	else if(pcx_width == 319 && pcx_height == 239
			&& Q_strcasecmp(filename, "pics/quit.pcx") == 0
			&& Com_BlockChecksum(pcx, len) == 3329419434u)
	{
		// it's the quit screen, and the baseq2 one (identified by checksum)
		// so fix it
		fixQuitScreen(*pic);
	}

	if (image_issues)
	{
		R_Printf(PRINT_ALL, "PCX file %s has possible size issues.\n", filename);
	}

	ri.FS_FreeFile(pcx);
}

void
GetPCXInfo(const char *origname, int *width, int *height)
{
	pcx_t *pcx;
	byte *raw;
	char filename[256];

	FixFileExt(origname, "pcx", filename, sizeof(filename));

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

static byte
Convert24to8(const byte *d_8to24table, const int rgb[3])
{
	int i, best, diff;

	best = 255;
	diff = 1 << 20;

	for (i = 0; i < 256; i ++)
	{
		int j, curr_diff;

		curr_diff = 0;

		for (j = 0; j < 3; j++)
		{
			int v;

			v = d_8to24table[i * 4 + j] - rgb[j];
			curr_diff += v * v;
		}

		if (curr_diff < diff)
		{
			diff = curr_diff;
			best = i;
		}
	}

	return best;
}

static void
GenerateColormap(const byte *palette, byte *out_colormap)
{
	// https://quakewiki.org/wiki/Quake_palette
	int num_fullbrights = 32; /* the last 32 colours will be full bright */
	int x;

	for (x = 0; x < 256; x++)
	{
		int y;

		for (y = 0; y < 64; y++)
		{
			if (x < 256 - num_fullbrights)
			{
				int rgb[3], i;

				for (i = 0; i < 3; i++)
				{
					/* divide by 32, rounding to nearest integer */
					rgb[i] = (palette[x * 4 + i] * (63 - y) + 16) >> 5;
					if (rgb[i] > 255)
					{
						rgb[i] = 255;
					}
				}

				out_colormap[y*256+x] = Convert24to8(palette, rgb);
			}
			else
			{
				/* this colour is a fullbright, just keep the original colour */
				out_colormap[y*256+x] = x;
			}
		}
	}
}

void
GetPCXPalette24to8(byte *d_8to24table, byte** d_16to8table)
{
	unsigned char * table16to8;
	char tablefile[] = "pics/16to8.dat";

	*d_16to8table = NULL;
	ri.FS_LoadFile(tablefile, (void **)&table16to8);

	if (!table16to8)
	{
		R_Printf(PRINT_ALL, "%s: Couldn't load %s\n", __func__, tablefile);
	}

	*d_16to8table = malloc(0x10000);
	if (!(*d_16to8table))
	{
		ri.Sys_Error(ERR_FATAL, "%s: Couldn't allocate memory for d_16to8table", __func__);
		// code never returns after ERR_FATAL
		return;
	}

	if (table16to8)
	{
		// Use predefined convert map
		memcpy(*d_16to8table, table16to8, 0x10000);
		ri.FS_FreeFile((void *)table16to8);
	}
	else
	{
		// create new one
		unsigned int r;

		R_Printf(PRINT_ALL, "%s: Generate 16 to 8 bit table\n", __func__);

		for (r = 0; r < 32; r++)
		{
			int g;

			for (g = 0; g < 64; g++)
			{
				int b;

				for (b = 0; b < 32; b++)
				{
					int c, rgb[3];

					rgb[0] = r << 3;
					rgb[1] = g << 2;
					rgb[2] = b << 3;

					c = r | ( g << 5 ) | ( b << 11 );

					// set color with minimal difference
					(*d_16to8table)[c & 0xFFFF] = Convert24to8(d_8to24table, rgb);
				}
			}
		}
	}
}

/*
===============
GetPCXPalette
===============
*/
void
GetPCXPalette(byte **colormap, unsigned *d_8to24table)
{
	char	filename[] = "pics/colormap.pcx";
	byte	*pal;
	int		i;

	/* get the palette and colormap */
	LoadPCX(filename, colormap, &pal, NULL, NULL);
	if (!*colormap || !pal)
	{
		R_Printf(PRINT_ALL, "%s: Couldn't load %s, use generated palette\n",
			__func__, filename);

		/* palette r:2bit, g:3bit, b:3bit */
		for (i=0 ; i < 256; i++)
		{
			unsigned v;

			v = (255U<<24) + (((i >> 0) & 0x3) << (6 + 0)) +
							 (((i >> 2) & 0x7) << (5 + 8)) +
							 (((i >> 5) & 0x7) << (5 + 16));
			d_8to24table[i] = LittleLong(v);
		}

		d_8to24table[255] &= LittleLong(0xffffff);	// 255 is transparent

		/* generate colormap */
		*colormap = malloc(256 * 320);
		if (!(*colormap))
		{
			ri.Sys_Error(ERR_FATAL, "%s: Couldn't allocate memory for colormap", __func__);
			// code never returns after ERR_FATAL
			return;
		}

		GenerateColormap((const byte *)d_8to24table, *colormap);
		return;
	}

	for (i=0 ; i<256 ; i++)
	{
		unsigned	v;
		int	r, g, b;

		r = pal[i*3+0];
		g = pal[i*3+1];
		b = pal[i*3+2];

		v = (255U<<24) + (r<<0) + (g<<8) + (b<<16);
		d_8to24table[i] = LittleLong(v);
	}

	d_8to24table[255] &= LittleLong(0xffffff);	// 255 is transparent

	free (pal);
}
