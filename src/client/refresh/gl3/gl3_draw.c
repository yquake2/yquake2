/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016-2017 Daniel Gibson
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

unsigned d_8to24table[256];

gl3image_t *draw_chars;

void
GL3_Draw_InitLocal(void)
{
	/* load console characters */
	draw_chars = GL3_FindImage("pics/conchars.pcx", it_pic);

	glGenVertexArrays(1, &gl3state.vao2D);
	glBindVertexArray(gl3state.vao2D);

	glGenBuffers(1, &gl3state.vbo2D);
	glBindBuffer(GL_ARRAY_BUFFER, gl3state.vbo2D); // TODO ??

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void
GL3_Draw_ShutdownLocal(void)
{
	glDeleteBuffers(1, &gl3state.vbo2D);
	gl3state.vbo2D = 0;
	glDeleteVertexArrays(1, &gl3state.vao2D);
	gl3state.vao2D = 0;
}

/*
 * Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be
 * smoothly scrolled off.
 */
void
GL3_Draw_CharScaled(int x, int y, int num, float scale)
{
	STUB_ONCE("TODO: Implement!");
#if 0
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
#endif // 0
}

gl3image_t *
GL3_Draw_FindPic(char *name)
{
	gl3image_t *gl;
	char fullname[MAX_QPATH];

	if ((name[0] != '/') && (name[0] != '\\'))
	{
		Com_sprintf(fullname, sizeof(fullname), "pics/%s.pcx", name);
		gl = GL3_FindImage(fullname, it_pic);
	}
	else
	{
		gl = GL3_FindImage(name + 1, it_pic);
	}

	return gl;
}

void
GL3_Draw_GetPicSize(int *w, int *h, char *pic)
{
	gl3image_t *gl;

	gl = GL3_Draw_FindPic(pic);

	if (!gl)
	{
		*w = *h = -1;
		return;
	}

	*w = gl->width;
	*h = gl->height;
}

void
GL3_Draw_StretchPic(int x, int y, int w, int h, char *pic)
{
	STUB_ONCE("TODO: Implement!");
#if 0
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
#endif // 0
}

void
GL3_Draw_PicScaled(int x, int y, char *pic, float factor)
{
	STUB_ONCE("TODO: Implement!");
#if 0
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
#endif // 0
}

/*
 * This repeats a 64*64 tile graphic to fill
 * the screen around a sized down
 * refresh window.
 */
void
GL3_Draw_TileClear(int x, int y, int w, int h, char *pic)
{
	STUB_ONCE("TODO: Implement!");
#if 0
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
#endif // 0
}

/*
 * Fills a box of pixels with a single color
 */
void
GL3_Draw_Fill(int x, int y, int w, int h, int c)
{
	STUB_ONCE("TODO: Implement!");
#if 0
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
#endif // 0
}

void
GL3_Draw_FadeScreen(void)
{
	STUB_ONCE("TODO: Implement!");
#if 0
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
#endif // 0
}

void
GL3_Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data)
{
	byte *source;
	float hscale = 1.0f;
	int frac, fracstep;
	int i, j, trows;
	int row;

	GLfloat vBuf[] = {
	//  posX,  posY,  texS, texT
		x,     y + h, 0.0f, 1.0f,
		x,     y,     0.0f, 0.0f,
		x + w, y + h, 1.0f, 1.0f,
		x + w, y,     1.0f, 0.0f,
	};

	GL3_Bind(0);

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
			img[rowOffset+j] = gl3_rawpalette[palIdx];
		}
	}

	GLuint glTex;
	glGenTextures(1, &glTex);
	glBindTexture(GL_TEXTURE_2D, glTex);

	glTexImage2D(GL_TEXTURE_2D, 0, gl3_tex_solid_format,
	             cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);

	if(img != image32)
	{
		free(img);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


	glBindVertexArray(gl3state.vao2D);

	glBindBuffer(GL_ARRAY_BUFFER, gl3state.vbo2D);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vBuf), vBuf, GL_STREAM_DRAW);

	glEnableVertexAttribArray(gl3state.si2D.attribPosition);
	qglVertexAttribPointer(gl3state.si2D.attribPosition, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);

	glEnableVertexAttribArray(gl3state.si2D.attribTexCoord);
	qglVertexAttribPointer(gl3state.si2D.attribTexCoord, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), sizeof(float)*2);

	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );

	glDeleteTextures(1, &glTex);
	GL3_Bind(0);
}

int
GL3_Draw_GetPalette(void)
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
