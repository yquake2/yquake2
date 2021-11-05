/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2015 Daniel Gibson
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
 * File formats supported by stb_image, for now only tga, png, jpg
 * See also https://github.com/nothings/stb
 *
 * =======================================================================
 */

#include <stdlib.h>

#include "../ref_shared.h"

// don't need HDR stuff
#define STBI_NO_LINEAR
#define STBI_NO_HDR
// make sure STB_image uses standard malloc(), as we'll use standard free() to deallocate
#define STBI_MALLOC(sz)    malloc(sz)
#define STBI_REALLOC(p,sz) realloc(p,sz)
#define STBI_FREE(p)       free(p)
// include implementation part of stb_image into this file
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// include resize implementation
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

/*
 * origname: the filename to be opened, might be without extension
 * type: extension of the type we wanna open ("jpg", "png" or "tga")
 * pic: pointer RGBA pixel data will be assigned to
 */
qboolean
LoadSTB(const char *origname, const char* type, byte **pic, int *width, int *height)
{
	char filename[256];

	Q_strlcpy(filename, origname, sizeof(filename));

	/* Add the extension */
	if (strcmp(COM_FileExtension(filename), type) != 0)
	{
		Q_strlcat(filename, ".", sizeof(filename));
		Q_strlcat(filename, type, sizeof(filename));
	}

	*pic = NULL;

	byte* rawdata = NULL;
	int rawsize = ri.FS_LoadFile(filename, (void **)&rawdata);
	if (rawdata == NULL)
	{
		return false;
	}

	int w, h, bytesPerPixel;
	byte* data = NULL;
	data = stbi_load_from_memory(rawdata, rawsize, &w, &h, &bytesPerPixel, STBI_rgb_alpha);
	if (data == NULL)
	{
		R_Printf(PRINT_ALL, "%s couldn't load data from %s: %s!\n", __func__, filename, stbi_failure_reason());
		ri.FS_FreeFile(rawdata);
		return false;
	}

	ri.FS_FreeFile(rawdata);

	R_Printf(PRINT_DEVELOPER, "%s() loaded: %s\n", __func__, filename);

	*pic = data;
	*width = w;
	*height = h;
	return true;
}

qboolean
ResizeSTB(byte *input_pixels, int input_width, int input_height,
			  byte *output_pixels, int output_width, int output_height)
{
	if (stbir_resize_uint8(input_pixels, input_width, input_height, 0,
			       output_pixels, output_width, output_height, 0, 4))
		return true;
	return false;
}

// We have 16 color palette, 256 / 16 should be enough
#define COLOR_DISTANCE 16

void
SmoothColorImage(unsigned *dst, size_t size, size_t rstep)
{
	unsigned *full_size;
	unsigned last_color;
	unsigned *last_diff;

	// maximum step for apply
	if (rstep < 2)
	{
		return;
	}

	// step one pixel back as with check one pixel more
	full_size = dst + size - rstep - 1;
	last_diff = dst;
	last_color = *dst;

	// skip current point
	dst ++;

	while (dst < full_size)
	{
		if (last_color != *dst)
		{
			int step = dst - last_diff;
			if (step > 1)
			{
				int a_beg, b_beg, c_beg, d_beg;
				int a_end, b_end, c_end, d_end;
				int a_step, b_step, c_step, d_step;
				int k;

				// minimize effect size to rstep
				if (step > rstep)
				{
					// change place for start effect
					last_diff += (step - rstep);
					step = rstep;
				}

				// compare next pixels
				for(k = 1; k <= step; k ++)
				{
					if (dst[k] != dst[0])
					{
						break;
					}
				}

				// step back as pixel different after previous step
				k --;

				// mirror steps
				if (k < step)
				{
					// R_Printf(PRINT_ALL, "%s %d -> %d\n", __func__, k, step);
					// change place for start effect
					last_diff += (step - k);
					step = k;
				}

				// update step to correct value
				step += k;
				dst += k;

				// get colors
				a_beg = (last_color >> 0 ) & 0xff;
				b_beg = (last_color >> 8 ) & 0xff;
				c_beg = (last_color >> 16) & 0xff;
				d_beg = (last_color >> 24) & 0xff;

				a_end = (*dst >> 0 ) & 0xff;
				b_end = (*dst >> 8 ) & 0xff;
				c_end = (*dst >> 16) & 0xff;
				d_end = (*dst >> 24) & 0xff;

				a_step = a_end - a_beg;
				b_step = b_end - b_beg;
				c_step = c_end - c_beg;
				d_step = d_end - d_beg;

				if ((abs(a_step) <= COLOR_DISTANCE) &&
				    (abs(b_step) <= COLOR_DISTANCE) &&
				    (abs(c_step) <= COLOR_DISTANCE) &&
				    (abs(d_step) <= COLOR_DISTANCE) &&
				    step > 0)
				{
					// generate color change steps
					a_step = (a_step << 16) / step;
					b_step = (b_step << 16) / step;
					c_step = (c_step << 16) / step;
					d_step = (d_step << 16) / step;

					// apply color changes
					for (k=0; k < step; k++)
					{
						*last_diff = (((a_beg + ((a_step * k) >> 16)) << 0) & 0x000000ff) |
									 (((b_beg + ((b_step * k) >> 16)) << 8) & 0x0000ff00) |
									 (((c_beg + ((c_step * k) >> 16)) << 16) & 0x00ff0000) |
									 (((d_beg + ((d_step * k) >> 16)) << 24) & 0xff000000);
						last_diff++;
					}
				}
			}
			last_color = *dst;
			last_diff = dst;
		}
		dst ++;
	}
}

/* https://en.wikipedia.org/wiki/Pixel-art_scaling_algorithms */

void
scale2x(byte *src, byte *dst, int width, int height)
{
	/*
		EPX/Scale2×/AdvMAME2×

		x A x
		C P B -> 1 2
		x D x    3 4

		1=P; 2=P; 3=P; 4=P;
		IF C==A AND C!=D AND A!=B => 1=A
		IF A==B AND A!=C AND B!=D => 2=B
		IF D==C AND D!=B AND C!=A => 3=C
		IF B==D AND B!=A AND D!=C => 4=D
	*/
	{
		byte *in_buff = src;
		byte *out_buff = dst;
		byte *out_buff_full = dst + ((width * height) << 2);
		while (out_buff < out_buff_full)
		{
			int x;
			for (x = 0; x < width; x ++)
			{
				// copy one source byte to two destinatuion bytes
				*out_buff = *in_buff;
				out_buff ++;
				*out_buff = *in_buff;
				out_buff ++;

				// next source pixel
				in_buff ++;
			}
			// copy last line one more time
			memcpy(out_buff, out_buff - (width << 1), width << 1);
			out_buff += width << 1;
		}
	}

	{
		int y, h, w;
		h = height - 1;
		w = width - 1;
		for (y = 0; y < height; y ++)
		{
			int x;
			for (x = 0; x < width; x ++)
			{
				byte a, b, c, d, p;

				p = src[(width * (y    )) + (x    )];
				a = (y > 0) ? src[(width * (y - 1)) + (x    )] : p;
				b = (x < w) ? src[(width * (y    )) + (x + 1)] : p;
				c = (x > 0) ? src[(width * (y    )) + (x - 1)] : p;
				d = (y < h) ? src[(width * (y + 1)) + (x    )] : p;

				if ((c == a) && (c != d) && (a != b))
				{
					dst[(2 * width * ((y * 2)    )) + ((x * 2)    )] = a;
				}

				if ((a == b) && (a != c) && (b != d))
				{
					dst[(2 * width * ((y * 2)    )) + ((x * 2) + 1)] = b;
				}

				if ((d == c) && (d != b) && (c != a))
				{
					dst[(2 * width * ((y * 2) + 1)) + ((x * 2)    )] = c;
				}

				if ((b == d) && (b != a) && (d != c))
				{
					dst[(2 * width * ((y * 2) + 1)) + ((x * 2) + 1)] = d;
				}
			}
		}
	}
}

void
scale3x(byte *src, byte *dst, int width, int height)
{
	/*
		Scale3×/AdvMAME3× and ScaleFX

		A B C    1 2 3
		D E F -> 4 5 6
		G H I    7 8 9


		1=E; 2=E; 3=E; 4=E; 5=E; 6=E; 7=E; 8=E; 9=E;
		IF D==B AND D!=H AND B!=F => 1=D
		IF (D==B AND D!=H AND B!=F AND E!=C) OR (B==F AND B!=D AND F!=H AND E!=A) => 2=B
		IF B==F AND B!=D AND F!=H => 3=F
		IF (H==D AND H!=F AND D!=B AND E!=A) OR (D==B AND D!=H AND B!=F AND E!=G) => 4=D
		5=E
		IF (B==F AND B!=D AND F!=H AND E!=I) OR (F==H AND F!=B AND H!=D AND E!=C) => 6=F
		IF H==D AND H!=F AND D!=B => 7=D
		IF (F==H AND F!=B AND H!=D AND E!=G) OR (H==D AND H!=F AND D!=B AND E!=I) => 8=H
		IF F==H AND F!=B AND H!=D => 9=F
	*/
	{
		byte *in_buff = src;
		byte *out_buff = dst;
		byte *out_buff_full = dst + ((width * height) * 9);
		while (out_buff < out_buff_full)
		{
			int x;
			for (x = 0; x < width; x ++)
			{
				// copy one source byte to two destinatuion bytes
				*out_buff = *in_buff;
				out_buff ++;
				*out_buff = *in_buff;
				out_buff ++;
				*out_buff = *in_buff;
				out_buff ++;

				// next source pixel
				in_buff ++;
			}
			// copy last line one more time
			memcpy(out_buff, out_buff - (width * 3), width * 3);
			out_buff += width * 3;
			// copy last line one more time
			memcpy(out_buff, out_buff - (width * 3), width * 3);
			out_buff += width * 3;
		}
	}

	{
		int y, z, w;
		z = height - 1;
		w = width - 1;
		for (y = 0; y < height; y ++)
		{
			int x;
			for (x = 0; x < width; x ++)
			{
				byte a, b, c, d, e, f, g, h, i;

				e = src[(width * y) + x];

				a = ((y > 0) && (x > 0)) ? src[(width * (y - 1)) + (x - 1)] : e;
				b = ((y > 0) && (x    )) ? src[(width * (y - 1)) + (x    )] : e;
				c = ((y > 0) && (x < w)) ? src[(width * (y - 1)) + (x + 1)] : e;

				d = (           (x > 0)) ? src[(width * (y    )) + (x - 1)] : e;
				f = (           (x < w)) ? src[(width * (y    )) + (x + 1)] : e;

				g = ((y < z) && (x > 0)) ? src[(width * (y + 1)) + (x - 1)] : e;
				h = ((y < z) && (x    )) ? src[(width * (y + 1)) + (x    )] : e;
				i = ((y < z) && (x < w)) ? src[(width * (y + 1)) + (x + 1)] : e;

				if ((d == b) && (d != h) && (b != f))
				{
					dst[(3 * width * ((y * 3)    )) + ((x * 3)    )] = d;
				}

				if (((d == b) && (d != h) && (b != f) && (e != c)) ||
					((b == f) && (b != d) && (f != h) && (e != a)))
				{
					dst[(3 * width * ((y * 3)    )) + ((x * 3) + 1)] = b;
				}

				if ((b == f) && (b != d) && (f != h))
				{
					dst[(3 * width * ((y * 3)    )) + ((x * 3) + 2)] = f;
				}

				if (((h == d) && (h != f) && (d != b) && (e != a)) ||
					((d == b) && (d != h) && (b != f) && (e != g)))
				{
					dst[(3 * width * ((y * 3) + 1)) + ((x * 3)    )] = d;
				}

				if (((b == f) && (b != d) && (f != h) && (e != i)) ||
					((f == h) && (f != b) && (h != d) && (e != c)))
				{
					dst[(3 * width * ((y * 3) + 1)) + ((x * 3) + 2)] = f;
				}

				if ((h == d) && (h != f) && (d != b))
				{
					dst[(3 * width * ((y * 3) + 2)) + ((x * 3)    )] = d;
				}

				if (((f == h) && (f != b) && (h != d) && (e != g)) ||
					((h == d) && (h != f) && (d != b) && (e != i)))
				{
					dst[(3 * width * ((y * 3) + 2)) + ((x * 3) + 1)] = h;
				}

				if ((f == h) && (f != b) && (h != d))
				{
					dst[(3 * width * ((y * 3) + 2)) + ((x * 3) + 2)] = f;
				}
			}
		}
	}
}
