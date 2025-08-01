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
 * Drawing of all images that are not textures
 *
 * =======================================================================
 */

#include "header/local.h"

image_t *draw_chars;

extern qboolean scrap_dirty;
void Scrap_Upload(void);

extern unsigned r_rawpalette[256];

void
Draw_InitLocal(void)
{
	/* load console characters */
	draw_chars = R_FindPic("conchars", (findimage_t)R_FindImage);
	if (!draw_chars)
	{
		Com_Error(ERR_FATAL, "%s: Couldn't load pics/conchars.pcx",
			__func__);
	}
}

/*
 * Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be
 * smoothly scrolled off.
 */
void
RDraw_CharScaled(int x, int y, int num, float scale)
{
	int row, col;
	float frow, fcol, size, scaledSize;

	num &= 255;

	if ((num & 127) == 32)
	{
		return; /* space */
	}

	if (y <= -8)
	{
		return; /* totally off screen */
	}

	row = num >> 4;
	col = num & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;

	scaledSize = 8 * scale;

	R_UpdateGLBuffer(buf_2d, draw_chars->texnum, 0, 0, 1);

	R_Buffer2DQuad(x, y, x + scaledSize, y + scaledSize,
		fcol, frow, fcol + size, frow + size);
}

image_t *
RDraw_FindPic(const char *name)
{
	return R_FindPic(name, (findimage_t)R_FindImage);
}

void
RDraw_GetPicSize(int *w, int *h, const char *pic)
{
	const image_t *gl;

	gl = R_FindPic(pic, (findimage_t)R_FindImage);

	if (!gl)
	{
		*w = *h = -1;
		return;
	}

	*w = gl->width;
	*h = gl->height;
}

void
RDraw_StretchPic(int x, int y, int w, int h, const char *pic)
{
	image_t *gl;

	gl = R_FindPic(pic, (findimage_t)R_FindImage);

	if (!gl)
	{
		Com_Printf("Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
	{
		Scrap_Upload();
	}

	R_UpdateGLBuffer(buf_2d, gl->texnum, 0, 0, 1);

	R_Buffer2DQuad(x, y, x + w, y + h,
		gl->sl, gl->tl, gl->sh, gl->th);
}

void
RDraw_PicScaled(int x, int y, const char *pic, float factor)
{
	image_t *gl;

	gl = R_FindPic(pic, (findimage_t)R_FindImage);

	if (!gl)
	{
		Com_Printf("Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
	{
		Scrap_Upload();
	}

	if (gl->texnum == TEXNUM_SCRAPS)
	{
		R_UpdateGLBuffer(buf_2d, TEXNUM_SCRAPS, 0, 0, 1);
		R_Buffer2DQuad(x, y, x + gl->width * factor, y + gl->height * factor,
			gl->sl, gl->tl, gl->sh, gl->th);
		return;
	}

	R_Bind(gl->texnum);

	GLfloat vtx[] = {
		x, y,
		x + gl->width * factor, y,
		x + gl->width * factor, y + gl->height * factor,
		x, y + gl->height * factor
	};

	GLfloat tex[] = {
		gl->sl, gl->tl,
		gl->sh, gl->tl,
		gl->sh, gl->th,
		gl->sl, gl->th
	};

	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );

	glVertexPointer( 2, GL_FLOAT, 0, vtx );
	glTexCoordPointer( 2, GL_FLOAT, 0, tex );
	glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
}

/*
 * This repeats a 64*64 tile graphic to fill
 * the screen around a sized down
 * refresh window.
 */
void
RDraw_TileClear(int x, int y, int w, int h, const char *pic)
{
	const image_t *image;

	image = R_FindPic(pic, (findimage_t)R_FindImage);

	if (!image)
	{
		Com_Printf("Can't find pic: %s\n", pic);
		return;
	}

	R_UpdateGLBuffer(buf_2d, image->texnum, 0, 0, 1);

	R_Buffer2DQuad(x, y, x + w, y + h, x / 64.0, y / 64.0,
		( x + w ) / 64.0, ( y + h ) / 64.0);
}

/*
 * Fills a box of pixels with a single color
 */
void
RDraw_Fill(int x, int y, int w, int h, int c)
{
	union
	{
		unsigned c;
		byte v[4];
	} color;

	if ((unsigned)c > 255)
	{
		Com_Error(ERR_FATAL, "%s: bad color", __func__);
	}

	glDisable(GL_TEXTURE_2D);

	color.c = d_8to24table[c];
	glColor4f(color.v [ 0 ] / 255.0, color.v [ 1 ] / 255.0,
			   color.v [ 2 ] / 255.0, 1);

	GLfloat vtx[] = {
		x, y,
		x + w, y,
		x + w, y + h,
		x, y + h
	};

	glEnableClientState( GL_VERTEX_ARRAY );

	glVertexPointer( 2, GL_FLOAT, 0, vtx );
	glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

	glDisableClientState( GL_VERTEX_ARRAY );

	glColor4f( 1, 1, 1, 1 );
	glEnable(GL_TEXTURE_2D);
}

void
RDraw_FadeScreen(void)
{
	R_ApplyGLBuffer();	// draw what needs to be hidden
	glEnable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glColor4f(0, 0, 0, 0.8);

	GLfloat vtx[] = {
		0, 0,
		vid.width, 0,
		vid.width, vid.height,
		0, vid.height
	};

	glEnableClientState( GL_VERTEX_ARRAY );

	glVertexPointer( 2, GL_FLOAT, 0, vtx );
	glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

	glDisableClientState( GL_VERTEX_ARRAY );

	glColor4f(1, 1, 1, 1);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

void
RDraw_StretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int bits)
{
	GLfloat tex[8];
	float hscale = 1.0f;
	int frac, fracstep;
	int i, j, trows;
	int row;

	R_Bind(0);

	if(gl_config.npottextures || rows <= 256 || bits == 32)
	{
		// X, X
		tex[0] = 0;
		tex[1] = 0;

		// X, Y
		tex[2] = 1;
		tex[3] = 0;

		// Y, X
		tex[4] = 1;
		tex[5] = 1;

		// Y, Y
		tex[6] = 0;
		tex[7] = 1;
	}
	else
	{
		// Scale params
		hscale = rows / 256.0;
		trows = 256;

		// X, X
		tex[0] = 1.0 / 512.0;
		tex[1] = 1.0 / 512.0;

		// X, Y
		tex[2] = 511.0 / 512.0;
		tex[3] = 1.0 / 512.0;

		// Y, X
		tex[4] = 511.0 / 512.0;
		tex[5] = rows * hscale / 256 - 1.0 / 512.0;

		// Y, Y
		tex[6] = 1.0 / 512.0;
		tex[7] = rows * hscale / 256 - 1.0 / 512.0;
	}

	GLfloat vtx[] = {
			x, y,
			x + w, y,
			x + w, y + h,
			x, y + h
	};

	if (!gl_config.palettedtexture || bits == 32)
	{
		/* .. because now if non-power-of-2 textures are supported, we just load
		 * the data into a texture in the original format, without skipping any
		 * pixels to fit into a 256x256 texture.
		 * This causes text in videos (which are 320x240) to not look broken anymore.
		 */
		if (bits == 32)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, gl_tex_solid_format,
					cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE,
					data);
		}
		else if(gl_config.npottextures || rows <= 256)
		{
			unsigned image32[320*240]; /* was 256 * 256, but we want a bit more space */
			unsigned* img = image32;

			if(cols*rows > 320*240)
			{
				/* in case there is a bigger video after all,
				 * malloc enough space to hold the frame */
				img = (unsigned*)malloc(cols*rows*4);
			}

			for(i=0; i<rows; ++i)
			{
				int rowOffset = i*cols;
				for(j=0; j<cols; ++j)
				{
					byte palIdx = data[rowOffset+j];
					img[rowOffset+j] = r_rawpalette[palIdx];
				}
			}

			glTexImage2D(GL_TEXTURE_2D, 0, gl_tex_solid_format,
								cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE,
								img);

			if(img != image32)
			{
				free(img);
			}
		}
		else
		{
			unsigned int image32[320*240];

			for (i = 0; i < trows; i++)
			{
				const byte *source;
				unsigned *dest;

				row = (int)(i * hscale);

				if (row > rows)
				{
					break;
				}

				source = data + cols * row;
				dest = &image32[i * 256];
				fracstep = cols * 0x10000 / 256;
				frac = fracstep >> 1;

				for (j = 0; j < 256; j++)
				{
					dest[j] = r_rawpalette[source[frac >> 16]];
					frac += fracstep;
				}
			}

			glTexImage2D(GL_TEXTURE_2D, 0, gl_tex_solid_format,
					256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE,
					image32);
		}
	}
	else
	{
		unsigned char image8[256 * 256];

		for (i = 0; i < trows; i++)
		{
			unsigned char *dest;
			const byte *source;

			row = (int)(i * hscale);

			if (row > rows)
			{
				break;
			}

			source = data + cols * row;
			dest = &image8[i * 256];
			fracstep = cols * 0x10000 / 256;
			frac = fracstep >> 1;

			for (j = 0; j < 256; j++)
			{
				dest[j] = source[frac >> 16];
				frac += fracstep;
			}
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, 256, 256,
				0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, image8);
	}

	// Note: gl_filter_min could be GL_*_MIPMAP_* so we can't use it for min filter here (=> no mipmaps)
	//       but gl_filter_max (either GL_LINEAR or GL_NEAREST) should do the trick.
	GLint filter = (r_videos_unfiltered->value == 0) ? gl_filter_max : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );

	glVertexPointer( 2, GL_FLOAT, 0, vtx );
	glTexCoordPointer( 2, GL_FLOAT, 0, tex );
	glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
}
