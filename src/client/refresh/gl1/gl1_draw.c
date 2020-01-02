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
	draw_chars = R_FindImage("pics/conchars.pcx", it_pic);
	if (!draw_chars)
	{
		ri.Sys_Error(ERR_FATAL, "Couldn't load pics/conchars.pcx");
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

	scaledSize = 8*scale;

	R_Bind(draw_chars->texnum);

	GLfloat vtx[] = {
		x, y,
		x + scaledSize, y,
		x + scaledSize, y + scaledSize,
		x, y + scaledSize
	};

	GLfloat tex[] = {
		fcol, frow,
		fcol + size, frow,
		fcol + size, frow + size,
		fcol, frow + size
	};

	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );

	glVertexPointer( 2, GL_FLOAT, 0, vtx );
	glTexCoordPointer( 2, GL_FLOAT, 0, tex );
	glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
}

image_t *
RDraw_FindPic(char *name)
{
	image_t *gl;
	char fullname[MAX_QPATH];

	if ((name[0] != '/') && (name[0] != '\\'))
	{
		Com_sprintf(fullname, sizeof(fullname), "pics/%s.pcx", name);
		gl = R_FindImage(fullname, it_pic);
	}
	else
	{
		gl = R_FindImage(name + 1, it_pic);
	}

	return gl;
}

void
RDraw_GetPicSize(int *w, int *h, char *pic)
{
	image_t *gl;

	gl = RDraw_FindPic(pic);

	if (!gl)
	{
		*w = *h = -1;
		return;
	}

	*w = gl->width;
	*h = gl->height;
}

void
RDraw_StretchPic(int x, int y, int w, int h, char *pic)
{
	image_t *gl;

	gl = RDraw_FindPic(pic);

	if (!gl)
	{
		R_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
	{
		Scrap_Upload();
	}

	R_Bind(gl->texnum);

	GLfloat vtx[] = {
		x, y,
		x + w, y,
		x + w, y + h,
		x, y + h
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

void
RDraw_PicScaled(int x, int y, char *pic, float factor)
{
	image_t *gl;

	gl = RDraw_FindPic(pic);

	if (!gl)
	{
		R_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
	{
		Scrap_Upload();
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
RDraw_TileClear(int x, int y, int w, int h, char *pic)
{
	image_t *image;

	image = RDraw_FindPic(pic);

	if (!image)
	{
		R_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	R_Bind(image->texnum);

	GLfloat vtx[] = {
		x, y,
		x + w, y,
		x + w, y + h,
		x, y + h
	};

	GLfloat tex[] = {
		x / 64.0, y / 64.0,
		( x + w ) / 64.0, y / 64.0,
		( x + w ) / 64.0, ( y + h ) / 64.0,
		x / 64.0, ( y + h ) / 64.0
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
		ri.Sys_Error(ERR_FATAL, "Draw_Fill: bad color");
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
RDraw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data)
{
	GLfloat tex[8];
	byte *source;
	float hscale = 1.0f;
	int frac, fracstep;
	int i, j, trows;
	int row;

	R_Bind(0);

	if(gl_config.npottextures || rows <= 256)
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

	if (!gl_config.palettedtexture)
	{
		unsigned image32[320*240]; /* was 256 * 256, but we want a bit more space */

		/* .. because now if non-power-of-2 textures are supported, we just load
		 * the data into a texture in the original format, without skipping any
		 * pixels to fit into a 256x256 texture.
		 * This causes text in videos (which are 320x240) to not look broken anymore.
		 */
		if(gl_config.npottextures || rows <= 256)
		{
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
			unsigned *dest;

			for (i = 0; i < trows; i++)
			{
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
		unsigned char *dest;

		for (i = 0; i < trows; i++)
		{
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );

	glVertexPointer( 2, GL_FLOAT, 0, vtx );
	glTexCoordPointer( 2, GL_FLOAT, 0, tex );
	glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
}

int
Draw_GetPalette(void)
{
	int i;
	int r, g, b;
	unsigned v;
	byte *pic, *pal;
	int width, height;

	/* get the palette */
	LoadPCX("pics/colormap.pcx", &pic, &pal, &width, &height);

	if (!pal)
	{
		ri.Sys_Error(ERR_FATAL, "Couldn't load pics/colormap.pcx");
	}

	for (i = 0; i < 256; i++)
	{
		r = pal[i * 3 + 0];
		g = pal[i * 3 + 1];
		b = pal[i * 3 + 2];

		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		d_8to24table[i] = LittleLong(v);
	}

	d_8to24table[255] &= LittleLong(0xffffff); /* 255 is transparent */

	free(pic);
	free(pal);

	return 0;
}

