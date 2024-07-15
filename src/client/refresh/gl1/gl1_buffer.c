/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2024      Jaime Moreira
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
 * Drawing buffer: sort of a "Q3A shader" handler, allows to join multiple
 * draw calls into one, by grouping those which share the same
 * characteristics (mostly the same texture).
 *
 * =======================================================================
 */

#include "header/local.h"

#define MAX_VERTICES	16384
#define MAX_INDICES 	(MAX_VERTICES * 4)

typedef struct	//	832k aprox.
{
	buffered_draw_t	type;

	GLfloat
		vtx[MAX_VERTICES * 3],	// vertexes
		tex[MAX_TEXTURE_UNITS][MAX_VERTICES * 2],	// texture coords
		clr[MAX_VERTICES * 4];	// color components

	GLushort
		idx[MAX_INDICES],	// indices
		vtx_ptr, idx_ptr;	// pointers for array positions

	int	texture[MAX_TEXTURE_UNITS];
	int	flags;	// entity flags
	float	alpha;
} glbuffer_t;

glbuffer_t gl_buf;

GLuint vt, tx, cl;	// indices for arrays in gl_buf

extern void R_MYgluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);

void
R_ApplyGLBuffer(void)
{
	// Properties of batched draws here
	GLint vtx_size;
	qboolean texture;

	if (gl_buf.vtx_ptr == 0 || gl_buf.idx_ptr == 0)
	{
		return;
	}

	// defaults for drawing
	vtx_size = 3;
	texture = true;

	// choosing features by type
	switch (gl_buf.type)
	{
		case buf_2d:
			vtx_size = 2;
			break;
		default:
			break;
	}

	glEnableClientState( GL_VERTEX_ARRAY );
	glVertexPointer (vtx_size, GL_FLOAT, 0, gl_buf.vtx);

	if (texture)
	{
		R_Bind(gl_buf.texture[0]);

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, gl_buf.tex[0]);
	}

	// All set, we can finally draw
	glDrawElements(GL_TRIANGLES, gl_buf.idx_ptr, GL_UNSIGNED_SHORT, gl_buf.idx);
	// ... and now, turn back everything as it was

	if (texture)
	{
		glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	glDisableClientState( GL_VERTEX_ARRAY );

	gl_buf.vtx_ptr = gl_buf.idx_ptr = 0;
}

void
R_UpdateGLBuffer(buffered_draw_t type, int colortex, int lighttex, int flags, float alpha)
{
	if ( gl_buf.type != type || gl_buf.texture[0] != colortex )
	{
		R_ApplyGLBuffer();

		gl_buf.type = type;
		gl_buf.texture[0] = colortex;
		gl_buf.texture[1] = lighttex;
		gl_buf.flags = flags;
		gl_buf.alpha = alpha;
	}
}

void
R_Buffer2DQuad(GLfloat ul_vx, GLfloat ul_vy, GLfloat dr_vx, GLfloat dr_vy,
	GLfloat ul_tx, GLfloat ul_ty, GLfloat dr_tx, GLfloat dr_ty)
{
	static const GLushort idx_max = MAX_INDICES - 7;
	static const GLushort vtx_max = MAX_VERTICES - 5;
	unsigned int i;

	if (gl_buf.idx_ptr > idx_max || gl_buf.vtx_ptr > vtx_max)
	{
		R_ApplyGLBuffer();
	}

	i = gl_buf.vtx_ptr * 2;      // vertex index

	// "Quad" = 2-triangle GL_TRIANGLE_FAN
	gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr;
	gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr+1;
	gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr+2;
	gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr;
	gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr+2;
	gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr+3;

	// up left corner coords
	gl_buf.vtx[i]   = ul_vx;
	gl_buf.vtx[i+1] = ul_vy;
	// up right
	gl_buf.vtx[i+2] = dr_vx;
	gl_buf.vtx[i+3] = ul_vy;
	// down right
	gl_buf.vtx[i+4] = dr_vx;
	gl_buf.vtx[i+5] = dr_vy;
	// and finally, down left
	gl_buf.vtx[i+6] = ul_vx;
	gl_buf.vtx[i+7] = dr_vy;

	gl_buf.tex[0][i]   = ul_tx;
	gl_buf.tex[0][i+1] = ul_ty;
	gl_buf.tex[0][i+2] = dr_tx;
	gl_buf.tex[0][i+3] = ul_ty;
	gl_buf.tex[0][i+4] = dr_tx;
	gl_buf.tex[0][i+5] = dr_ty;
	gl_buf.tex[0][i+6] = ul_tx;
	gl_buf.tex[0][i+7] = dr_ty;

	gl_buf.vtx_ptr += 4;
}
