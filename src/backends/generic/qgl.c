/*
 * Copyright (C) 2013 Alejandro Ricoveri
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
 * This file implements the operating system binding of GL to QGL function
 * pointers.  When doing a port of Quake2 you must implement the following
 * two functions:
 *
 * QGL_Init() - loads libraries, assigns function pointers, etc.
 * QGL_Shutdown() - unloads libraries, NULLs function pointers
 *
 * This implementation should work for Windows and unixoid platforms,
 * other platforms may need an own implementation.
 *
 * =======================================================================
 */

#include "../../client/refresh/header/local.h"

/*
 * GL extensions
 */
void (APIENTRY *qglLockArraysEXT)(int, int);
void (APIENTRY *qglUnlockArraysEXT)(void);
void (APIENTRY *qglPointParameterfEXT)(GLenum param, GLfloat value);
void (APIENTRY *qglPointParameterfvEXT)(GLenum param, const GLfloat *value);
void (APIENTRY *qglColorTableEXT)(GLenum, GLenum, GLsizei, GLenum, GLenum,
		const GLvoid *);

void ( APIENTRY *qgl3DfxSetPaletteEXT ) ( GLuint * );
void ( APIENTRY *qglSelectTextureSGIS ) ( GLenum );
void ( APIENTRY *qglMTexCoord2fSGIS ) ( GLenum, GLfloat, GLfloat );
void ( APIENTRY *qglActiveTextureARB ) ( GLenum );
void ( APIENTRY *qglClientActiveTextureARB ) ( GLenum );

/* ========================================================================= */

void
QGL_Shutdown ( void )
{
	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;
	qglPointParameterfEXT = NULL;
	qglPointParameterfvEXT = NULL;
	qglColorTableEXT = NULL;
	qgl3DfxSetPaletteEXT = NULL;
	qglSelectTextureSGIS = NULL;
	qglMTexCoord2fSGIS = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
}

/* ========================================================================= */

void *
QGL_GetProcAddress ( char *proc )
{
	return GLimp_GetProcAddress (proc );
}

/* ========================================================================= */

qboolean
QGL_Init (void)
{
	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;
	qglPointParameterfEXT = NULL;
	qglPointParameterfvEXT = NULL;
	qglColorTableEXT = NULL;
	qgl3DfxSetPaletteEXT = NULL;
	qglSelectTextureSGIS = NULL;
	qglMTexCoord2fSGIS = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	return true;
}

