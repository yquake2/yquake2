/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// draw.c

#include "header/local.h"


static image_t	*draw_chars;	// 8*8 graphic characters

//=============================================================================

/*
================
RE_Draw_FindPic
================
*/
image_t *
RE_Draw_FindPic (char *name)
{
	if (name[0] != '/' && name[0] != '\\')
	{
		char fullname[MAX_QPATH];

		Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);
		return R_FindImage (fullname, it_pic);
	}
	else
		return R_FindImage (name+1, it_pic);
}



/*
===============
Draw_InitLocal
===============
*/
void
Draw_InitLocal (void)
{
	draw_chars = RE_Draw_FindPic ("conchars");
	if (!draw_chars)
	{
		ri.Sys_Error(ERR_FATAL, "%s: Couldn't load pics/conchars.pcx", __func__);
	}
}



/*
================
Draw_Char

Draws one 8*8 graphics character
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void
RE_Draw_CharScaled(int x, int y, int c, float scale)
{
	pixel_t	*dest;
	byte	*source, *pic_pixels;
	int	drawline;
	int	row, col, v, iscale, sscale, width, height;

	iscale = (int) scale;

	if (iscale < 1)
		return;

	c &= 255;

	if ((c&127) == 32)
		return;

	if (y <= -8)
		return;	// totally off screen

	if ( ( y + 8 ) > vid_buffer_height )	// status text was missing in sw...
		return;

	row = c>>4;
	col = c&15;

	width = draw_chars->asset_width * iscale;
	height = draw_chars->asset_height * iscale;
	pic_pixels = Get_BestImageSize(draw_chars, &width, &height);

	sscale = width / draw_chars->asset_width;
	pic_pixels += ((row<<10) * sscale + (col<<3)) * sscale;

	if (y < 0)
	{	// clipped
		drawline = 8 + y;
		pic_pixels -= width * y * sscale;
		y = 0;
	}
	else
		drawline = 8;

	dest = vid_buffer + y * vid_buffer_width + x;

	// clipped last lines
	if ((y + iscale * (drawline + 1)) > vid_buffer_height)
	{
		drawline = (vid_buffer_height - y) / scale;
	}

	VID_DamageBuffer(x, y);
	VID_DamageBuffer(x + (scale * 8), y + (scale * drawline));

	drawline = drawline * scale;

	for (v=0 ; v < drawline; v++, dest += vid_buffer_width)
	{
		int f, fstep, u;
		int sv = v * height / (iscale * draw_chars->asset_height);
		source = pic_pixels + sv * width;
		f = 0;
		fstep = (width << SHIFT16XYZ) / (scale * draw_chars->asset_width);
		for (u=0 ; u < (8 * iscale) ; u++)
		{
			if (source[f>>16] != TRANSPARENT_COLOR)
			{
				dest[u] = source[f>>16];
			}
			f += fstep;
		}
	}
}

/*
=============
RE_Draw_GetPicSize
=============
*/
void
RE_Draw_GetPicSize (int *w, int *h, char *name)
{
	image_t *gl;

	gl = RE_Draw_FindPic (name);
	if (!gl)
	{
		*w = *h = -1;
		return;
	}
	*w = gl->asset_width;
	*h = gl->asset_height;
}

/*
=============
RE_Draw_StretchPicImplementation
=============
*/
static void
RE_Draw_StretchPicImplementation (int x, int y, int w, int h, const image_t *pic)
{
	pixel_t	*dest;
	byte	*source;
	int		height;
	int		skip;
	int		pic_height, pic_width;
	byte	*pic_pixels;

	if ((x < 0) ||
		(x + w > vid_buffer_width) ||
		(y + h > vid_buffer_height))
	{
		R_Printf(PRINT_ALL, "%s: bad coordinates %dx%d[%dx%d]",
			__func__, x, y, w, h);
		return;
	}

	VID_DamageBuffer(x, y);
	VID_DamageBuffer(x + w, y + h);

	height = h;
	if (y < 0)
	{
		skip = -y;
		height += y;
		y = 0;
	}
	else
		skip = 0;

	dest = vid_buffer + y * vid_buffer_width + x;

	pic_height = h;
	pic_width = w;
	pic_pixels = Get_BestImageSize(pic, &pic_width, &pic_height);

	if (!pic->transparent)
	{
		if (w == pic_width)
		{
			int v;

			for (v=0 ; v<height ; v++, dest += vid_buffer_width)
			{
				int sv = (skip + v) * pic_height / h;
				memcpy (dest, pic_pixels + sv*pic_width, w);
			}
		}
		else
		{
			int v;

			// size of screen tile to pic pixel
			int picupscale = h / pic_height;

			for (v=0 ; v<height ; v++, dest += vid_buffer_width)
			{
				int f, fstep, u;
				int sv = (skip + v) * pic_height/h;
				source = pic_pixels + sv*pic_width;
				f = 0;
				fstep = (pic_width << SHIFT16XYZ) / w;
				for (u=0 ; u<w ; u++)
				{
					dest[u] = source[f>>16];
					f += fstep;
				}
				if (picupscale > 1)
				{
					int i;
					pixel_t	*dest_orig = dest;

					// copy first line to fill whole sector
					for (i=1; i < picupscale; i++)
					{
						// go to next line
						dest += vid_buffer_width;
						memcpy (dest, dest_orig, w);
					}
					// skip updated lines
					v += (picupscale - 1);
				}
			}
		}
	}
	else
	{
		source = pic_pixels;

		if (h == pic_height && w == pic_width)
		{
			int v;

			for (v = 0; v < pic_height; v++)
			{
				int u;

				for (u = 0; u < pic_width; u++)
				{
					if (source[u] != TRANSPARENT_COLOR)
						dest[u] = source[u];
				}
				dest += vid_buffer_width;
				source += pic_width;
			}
		}
		else
		{
			int v;

			for (v=0 ; v<height ; v++, dest += vid_buffer_width)
			{
				int f, fstep, u;
				int sv = (skip + v) * pic_height/h;
				source = pic_pixels + sv*pic_width;
				f = 0;
				fstep = (pic_width << SHIFT16XYZ) / w;
				for (u=0 ; u<w ; u++)
				{
					if (source[f>>16] != TRANSPARENT_COLOR)
					{
						dest[u] = source[f>>16];
					}
					f += fstep;
				}
			}
		}
	}
}

/*
=============
RE_Draw_StretchPic
=============
*/
void
RE_Draw_StretchPic (int x, int y, int w, int h, char *name)
{
	image_t	*pic;

	pic = RE_Draw_FindPic (name);
	if (!pic)
	{
		R_Printf(PRINT_ALL, "Can't find pic: %s\n", name);
		return;
	}
	RE_Draw_StretchPicImplementation (x, y, w, h, pic);
}

/*
=============
RE_Draw_StretchRaw
=============
*/
void
RE_Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{
	image_t	pic;
	byte	*image_scaled;

	// we have only one image size
	pic.mip_levels = 1;

	if (sw_retexturing->value)
	{
		if (cols < (w / 3) || rows < (h / 3))
		{
			image_scaled = malloc(cols * rows * 9);

			scale3x(data, image_scaled, cols, rows);

			cols = cols * 3;
			rows = rows * 3;
		}
		else
		{
			image_scaled = malloc(cols * rows * 4);

			scale2x(data, image_scaled, cols, rows);

			cols = cols * 2;
			rows = rows * 2;
		}
	}
	else
	{
		image_scaled = data;
	}

	pic.pixels[0] = image_scaled;
	pic.width = cols;
	pic.height = rows;
	pic.asset_width = cols;
	pic.asset_height = rows;

	RE_Draw_StretchPicImplementation (x, y, w, h, &pic);

	if (sw_retexturing->value)
	{
		free(image_scaled);
	}
}

/*
=============
Draw_Pic
=============
*/
void
RE_Draw_PicScaled(int x, int y, char *name, float scale)
{
	image_t		*pic;

	pic = RE_Draw_FindPic (name);
	if (!pic)
	{
		R_Printf(PRINT_ALL, "Can't find pic: %s\n", name);
		return;
	}

	RE_Draw_StretchPicImplementation (
		x, y,
		scale * pic->asset_width, scale * pic->asset_height,
		pic);
}

/*
=============
RE_Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void
RE_Draw_TileClear (int x, int y, int w, int h, char *name)
{
	int			i, j;
	byte		*psrc;
	pixel_t		*pdest;
	image_t		*pic;
	int			x2;

	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}
	if (x + w > vid_buffer_width)
		w = vid_buffer_width - x;
	if (y + h > vid_buffer_height)
		h = vid_buffer_height - y;
	if (w <= 0 || h <= 0)
		return;

	VID_DamageBuffer(x, y);
	VID_DamageBuffer(x + w, y + h);

	pic = RE_Draw_FindPic (name);
	if (!pic)
	{
		R_Printf(PRINT_ALL, "Can't find pic: %s\n", name);
		return;
	}
	x2 = x + w;
	pdest = vid_buffer + y * vid_buffer_width;
	for (i=0 ; i<h ; i++, pdest += vid_buffer_width)
	{
		psrc = pic->pixels[0] + pic->width * ((i+y) % pic->height);
		for (j=x ; j<x2 ; j++)
			pdest[j] = psrc[j % pic->width];
	}
}


/*
=============
RE_Draw_Fill

Fills a box of pixels with a single color
=============
*/
void
RE_Draw_Fill (int x, int y, int w, int h, int c)
{
	pixel_t	*dest;
	int	v;

	if (x+w > vid_buffer_width)
		w = vid_buffer_width - x;
	if (y+h > vid_buffer_height)
		h = vid_buffer_height - y;

	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}

	if (w < 0 || h < 0)
		return;

	VID_DamageBuffer(x, y);
	VID_DamageBuffer(x + w, y + h);

	dest = vid_buffer + y * vid_buffer_width + x;
	for (v=0 ; v<h ; v++, dest += vid_buffer_width)
		memset(dest, c, w);
}
//=============================================================================

/*
================
RE_Draw_FadeScreen

================
*/
void
RE_Draw_FadeScreen (void)
{
	int x,y;

	VID_DamageBuffer(0, 0);
	VID_DamageBuffer(vid_buffer_width, vid_buffer_height);

	for (y=0 ; y<vid_buffer_height ; y++)
	{
		int t;
		pixel_t *pbuf;

		pbuf = vid_buffer + vid_buffer_width * y;
		t = (y & 1) << 1;

		for (x=0 ; x<vid_buffer_width ; x++)
		{
			if ((x & 3) != t)
				pbuf[x] = 0;
		}
	}
}
