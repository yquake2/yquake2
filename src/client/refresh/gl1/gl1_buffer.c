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
	qboolean texture, mtex, alpha, color, alias, texenv_set;
	float fovy, dist;

	if (gl_buf.vtx_ptr == 0 || gl_buf.idx_ptr == 0)
	{
		return;
	}

	// defaults for drawing (mostly buf_singletex features)
	vtx_size = 3;
	texture = true;
	mtex = alpha = color = alias = texenv_set = false;

	// choosing features by type
	switch (gl_buf.type)
	{
		case buf_2d:
			vtx_size = 2;
			break;
		case buf_mtex:
			mtex = true;
			break;
		case buf_alpha:
			alpha = true;
			break;
		case buf_alias:
			alias = color = true;
			break;
		case buf_flash:
			color = true;
		case buf_shadow:
			texture = false;
			break;
		default:
			break;
	}

	R_EnableMultitexture(mtex);

	if (alias)
	{
		if (gl_buf.flags & RF_DEPTHHACK)
		{
			// hack the depth range to prevent view model from poking into walls
			glDepthRange(gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
		}

		if (gl_buf.flags & RF_WEAPONMODEL)
		{
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glLoadIdentity();

			if (gl_lefthand->value == 1.0f)
			{
				glScalef(-1, 1, 1);
			}

			fovy = (r_gunfov->value < 0) ? r_newrefdef.fov_y : r_gunfov->value;
			dist = (r_farsee->value == 0) ? 4096.0f : 8192.0f;
			R_MYgluPerspective(fovy, (float)r_newrefdef.width / r_newrefdef.height, 4, dist);

			glMatrixMode(GL_MODELVIEW);

			if (gl_lefthand->value == 1.0f)
			{
				glCullFace(GL_BACK);
			}
		}

		glShadeModel(GL_SMOOTH);
		R_TexEnv(GL_MODULATE);

		if (gl_buf.flags & RF_TRANSLUCENT)
		{
			glEnable(GL_BLEND);
		}

		if (gl_buf.flags & (RF_SHELL_RED | RF_SHELL_GREEN |
			RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
		{
			texture = false;
			glDisable(GL_TEXTURE_2D);
		}
	}

	if (alpha)
	{
		// the textures are prescaled up for a better
		// lighting range, so scale it back down
		glColor4f(gl_state.inverse_intensity, gl_state.inverse_intensity,
				  gl_state.inverse_intensity, gl_buf.alpha);

	}
	else if (gl_buf.flags & SURF_DRAWTURB)
	{
		texenv_set = true;

		// This is a hack ontop of a hack. Warping surfaces like those generated
		// by R_EmitWaterPolys() don't have a lightmap. Original Quake II therefore
		// negated the global intensity on those surfaces, because otherwise they
		// would show up much too bright. When we implemented overbright bits this
		// hack modified the global GL state in an incompatible way. So implement
		// a new hack, based on overbright bits... Depending on the value set to
		// gl1_overbrightbits the result is different:

		//  0: Old behaviour.
		//  1: No overbright bits on the global scene but correct lighting on
		//     warping surfaces.
		//  2,4: Overbright bits on the global scene but not on warping surfaces.
		//       They oversaturate otherwise.

		if (gl1_overbrightbits->value)
		{
			R_TexEnv(GL_COMBINE);
			glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
		}
		else
		{
			R_TexEnv(GL_MODULATE);
			glColor4f(gl_state.inverse_intensity, gl_state.inverse_intensity,
					  gl_state.inverse_intensity, 1.0f);
		}
	}

	glEnableClientState( GL_VERTEX_ARRAY );
	glVertexPointer (vtx_size, GL_FLOAT, 0, gl_buf.vtx);

	if (texture)
	{
		if (mtex)
		{
			// TMU 1: Lightmap texture
			R_MBind(GL_TEXTURE1, gl_state.lightmap_textures + gl_buf.texture[1]);

			if (gl1_overbrightbits->value)
			{
				R_TexEnv(GL_COMBINE);
				glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, gl1_overbrightbits->value);
			}

			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer(2, GL_FLOAT, 0, gl_buf.tex[1]);

			// TMU 0: Color texture
			R_MBind(GL_TEXTURE0, gl_buf.texture[0]);
		}
		else
		{
			R_Bind(gl_buf.texture[0]);
		}

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, gl_buf.tex[0]);
	}

	if (color)
	{
		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(4, GL_FLOAT, 0, gl_buf.clr);
	}

	// All set, we can finally draw
	glDrawElements(GL_TRIANGLES, gl_buf.idx_ptr, GL_UNSIGNED_SHORT, gl_buf.idx);
	// ... and now, turn back everything as it was

	if (color)
	{
		glDisableClientState(GL_COLOR_ARRAY);
	}

	if (texture)
	{
		glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	glDisableClientState( GL_VERTEX_ARRAY );

	if (texenv_set)
	{
		R_TexEnv(GL_REPLACE);
	}

	if (alias)
	{
		if (gl_buf.flags & (RF_SHELL_RED | RF_SHELL_GREEN |
			RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
		{
			glEnable(GL_TEXTURE_2D);
		}

		if (gl_buf.flags & RF_TRANSLUCENT)
		{
			glDisable(GL_BLEND);
		}

		R_TexEnv(GL_REPLACE);
		glShadeModel(GL_FLAT);

		if (gl_buf.flags & RF_WEAPONMODEL)
		{
			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
			if (gl_lefthand->value == 1.0F)
			{
				glCullFace(GL_FRONT);
			}
		}

		if (gl_buf.flags & RF_DEPTHHACK)
		{
			glDepthRange(gldepthmin, gldepthmax);
		}
	}

	gl_buf.vtx_ptr = gl_buf.idx_ptr = 0;
}

void
R_UpdateGLBuffer(buffered_draw_t type, int colortex, int lighttex, int flags, float alpha)
{
	if ( gl_buf.type != type || gl_buf.texture[0] != colortex ||
		(gl_config.multitexture && type == buf_mtex && gl_buf.texture[1] != lighttex) ||
		((type == buf_singletex || type == buf_alias) && gl_buf.flags != flags) ||
		(type == buf_alpha && gl_buf.alpha != alpha))
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

/*
 * Set up indices with the proper shape for the next buffered vertices
 */
void
R_SetBufferIndices(GLenum type, GLuint vertices_num)
{
	int i;

	if ( gl_buf.vtx_ptr + vertices_num >= MAX_VERTICES ||
		gl_buf.idx_ptr + ( (vertices_num - 2) * 3 ) >= MAX_INDICES )
	{
		R_ApplyGLBuffer();
	}

	switch (type)
	{
		case GL_TRIANGLE_FAN:
			for (i = 0; i < vertices_num-2; i++)
			{
				gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr;
				gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr+i+1;
				gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr+i+2;
			}
			break;
		case GL_TRIANGLE_STRIP:
			for (i = 0; i < vertices_num-2; i++)
			{
				if (i % 2 == 0)
				{
					gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr+i;
					gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr+i+1;
					gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr+i+2;
				}
				else	// backwards order
				{
					gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr+i+2;
					gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr+i+1;
					gl_buf.idx[gl_buf.idx_ptr++] = gl_buf.vtx_ptr+i;
				}
			}
			break;
		default:
			R_Printf(PRINT_DEVELOPER, "R_SetBufferIndices: no such type %d\n", type);
			return;
	}

	// These affect the functions that follow in this file
	vt = gl_buf.vtx_ptr * 3;	// vertex index
	tx = gl_buf.vtx_ptr * 2;	// texcoord index
	cl = gl_buf.vtx_ptr * 4;	// color index

	// R_BufferVertex() must be called as many times as vertices_num
	gl_buf.vtx_ptr += vertices_num;
}

/*
 * Adds a single vertex to buffer
 */
void
R_BufferVertex(GLfloat x, GLfloat y, GLfloat z)
{
	gl_buf.vtx[vt++] = x;
	gl_buf.vtx[vt++] = y;
	gl_buf.vtx[vt++] = z;
}

/*
 * Adds texture coordinates for color texture (no lightmap coords)
 */
void
R_BufferSingleTex(GLfloat s, GLfloat t)
{
	// tx should be set before this is called, by R_SetBufferIndices
	gl_buf.tex[0][tx++] = s;
	gl_buf.tex[0][tx++] = t;
}

/*
 * Adds texture coordinates for color and lightmap
 */
void
R_BufferMultiTex(GLfloat cs, GLfloat ct, GLfloat ls, GLfloat lt)
{
	gl_buf.tex[0][tx]   = cs;
	gl_buf.tex[0][tx+1] = ct;
	gl_buf.tex[1][tx]   = ls;
	gl_buf.tex[1][tx+1] = lt;
	tx += 2;
}

/*
 * Adds color components of vertex
 */
void
R_BufferColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	gl_buf.clr[cl++] = r;
	gl_buf.clr[cl++] = g;
	gl_buf.clr[cl++] = b;
	gl_buf.clr[cl++] = a;
}
