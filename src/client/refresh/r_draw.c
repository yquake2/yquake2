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

static cvar_t *gl_nolerp_list;

void
Draw_InitLocal(void)
{
	/* don't bilerp characters and crosshairs */
	gl_nolerp_list = Cvar_Get("gl_nolerp_list", "pics/conchars.pcx pics/ch1.pcx pics/ch2.pcx pics/ch3.pcx", 0);

	/* load console characters */
	draw_chars = R_FindImage("pics/conchars.pcx", it_pic);
}

/*
 * Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be
 * smoothly scrolled off.
 */
void
Draw_Char(int x, int y, int num)
{
	Draw_CharScaled(x, y, num, 1.0f);
}

/*
 * Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be
 * smoothly scrolled off.
 */
void
Draw_CharScaled(int x, int y, int num, float scale)
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

	glBegin(GL_QUADS);
	glTexCoord2f(fcol, frow);
	glVertex2f(x, y);
	glTexCoord2f(fcol + size, frow);
	glVertex2f(x + scaledSize, y);
	glTexCoord2f(fcol + size, frow + size);
	glVertex2f(x + scaledSize, y + scaledSize);
	glTexCoord2f(fcol, frow + size);
	glVertex2f(x, y + scaledSize);
	glEnd();
}

image_t *
Draw_FindPic(char *name)
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
Draw_GetPicSize(int *w, int *h, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic(pic);

	if (!gl)
	{
		*w = *h = -1;
		return;
	}

	*w = gl->width;
	*h = gl->height;
}

void
Draw_StretchPic(int x, int y, int w, int h, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic(pic);

	if (!gl)
	{
		VID_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
	{
		Scrap_Upload();
	}

	R_Bind(gl->texnum);
	glBegin(GL_QUADS);
	glTexCoord2f(gl->sl, gl->tl);
	glVertex2f(x, y);
	glTexCoord2f(gl->sh, gl->tl);
	glVertex2f(x + w, y);
	glTexCoord2f(gl->sh, gl->th);
	glVertex2f(x + w, y + h);
	glTexCoord2f(gl->sl, gl->th);
	glVertex2f(x, y + h);
	glEnd();
}

void
Draw_Pic(int x, int y, char *pic)
{
	Draw_PicScaled(x, y, pic, 1.0f);
}

void
Draw_PicScaled(int x, int y, char *pic, float factor)
{
	image_t *gl;

	gl = Draw_FindPic(pic);

	if (!gl)
	{
		VID_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
	{
		Scrap_Upload();
	}

	GLfloat w = gl->width*factor;
	GLfloat h = gl->height*factor;

	R_Bind(gl->texnum);
	glBegin(GL_QUADS);
	glTexCoord2f(gl->sl, gl->tl);
	glVertex2f(x, y);
	glTexCoord2f(gl->sh, gl->tl);
	glVertex2f(x + w, y);
	glTexCoord2f(gl->sh, gl->th);
	glVertex2f(x + w, y + h);
	glTexCoord2f(gl->sl, gl->th);
	glVertex2f(x, y + h);
	glEnd();
}

/*
 * This repeats a 64*64 tile graphic to fill
 * the screen around a sized down
 * refresh window.
 */
void
Draw_TileClear(int x, int y, int w, int h, char *pic)
{
	image_t *image;

	image = Draw_FindPic(pic);

	if (!image)
	{
		VID_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	R_Bind(image->texnum);
	glBegin(GL_QUADS);
	glTexCoord2f(x / 64.0, y / 64.0);
	glVertex2f(x, y);
	glTexCoord2f((x + w) / 64.0, y / 64.0);
	glVertex2f(x + w, y);
	glTexCoord2f((x + w) / 64.0, (y + h) / 64.0);
	glVertex2f(x + w, y + h);
	glTexCoord2f(x / 64.0, (y + h) / 64.0);
	glVertex2f(x, y + h);
	glEnd();
}

/*
 * Fills a box of pixels with a single color
 */
void
Draw_Fill(int x, int y, int w, int h, int c)
{
	union
	{
		unsigned c;
		byte v[4];
	} color;

	if ((unsigned)c > 255)
	{
		VID_Error(ERR_FATAL, "Draw_Fill: bad color");
	}

	glDisable(GL_TEXTURE_2D);

	color.c = d_8to24table[c];
	glColor4f(color.v[0] / 255.0, color.v[1] / 255.0, color.v[2] / 255.0, 1);

	glBegin(GL_QUADS);

	glVertex2f(x, y);
	glVertex2f(x + w, y);
	glVertex2f(x + w, y + h);
	glVertex2f(x, y + h);

	glEnd();
	glColor4f(1, 1, 1, 1);
	glEnable(GL_TEXTURE_2D);
}

void
Draw_FadeScreen(void)
{
	glEnable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glColor4f(0, 0, 0, 0.8);
	glBegin(GL_QUADS);

	glVertex2f(0, 0);
	glVertex2f(vid.width, 0);
	glVertex2f(vid.width, vid.height);
	glVertex2f(0, vid.height);

	glEnd();
	glColor4f(1, 1, 1, 1);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

void
Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data)
{
	unsigned image32[256 * 256];
	unsigned char image8[256 * 256];
	int i, j, trows;
	byte *source;
	int frac, fracstep;
	float hscale;
	int row;
	float t;

	R_Bind(0);

	if (rows <= 256)
	{
		hscale = 1;
		trows = rows;
	}
	else
	{
		hscale = rows / 256.0;
		trows = 256;
	}

	t = rows * hscale / 256 - 1.0 / 512.0;

	if (!qglColorTableEXT)
	{
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
	else
	{
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

		glTexImage2D(GL_TEXTURE_2D,
				0,
				GL_COLOR_INDEX8_EXT,
				256, 256,
				0,
				GL_COLOR_INDEX,
				GL_UNSIGNED_BYTE,
				image8);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBegin(GL_QUADS);
	glTexCoord2f(1.0 / 512.0, 1.0 / 512.0);
	glVertex2f(x, y);
	glTexCoord2f(511.0 / 512.0, 1.0 / 512.0);
	glVertex2f(x + w, y);
	glTexCoord2f(511.0 / 512.0, t);
	glVertex2f(x + w, y + h);
	glTexCoord2f(1.0 / 512.0, t);
	glVertex2f(x, y + h);
	glEnd();
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
		VID_Error(ERR_FATAL, "Couldn't load pics/colormap.pcx");
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

