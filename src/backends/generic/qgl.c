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
void ( APIENTRY *qglMultiTexCoord2fARB ) ( GLenum, GLfloat, GLfloat );
void ( APIENTRY *qglActiveTextureARB ) ( GLenum );
void ( APIENTRY *qglClientActiveTextureARB ) ( GLenum );

/* ------------------------- GL_ARB_shader_objects ------------------------- */

PFNGLATTACHOBJECTARBPROC qglAttachObjectARB;
PFNGLCOMPILESHADERARBPROC qglCompileShaderARB;
PFNGLCREATEPROGRAMOBJECTARBPROC qglCreateProgramObjectARB;
PFNGLCREATESHADEROBJECTARBPROC qglCreateShaderObjectARB;
PFNGLDELETEOBJECTARBPROC qglDeleteObjectARB;
PFNGLDETACHOBJECTARBPROC qglDetachObjectARB;
PFNGLGETACTIVEUNIFORMARBPROC qglGetActiveUniformARB;
PFNGLGETATTACHEDOBJECTSARBPROC qglGetAttachedObjectsARB;
PFNGLGETHANDLEARBPROC qglGetHandleARB;
PFNGLGETINFOLOGARBPROC qglGetInfoLogARB;
PFNGLGETOBJECTPARAMETERFVARBPROC qglGetObjectParameterfvARB;
PFNGLGETOBJECTPARAMETERIVARBPROC qglGetObjectParameterivARB;
PFNGLGETSHADERSOURCEARBPROC qglGetShaderSourceARB;
PFNGLGETUNIFORMLOCATIONARBPROC qglGetUniformLocationARB;
PFNGLGETUNIFORMFVARBPROC qglGetUniformfvARB;
PFNGLGETUNIFORMIVARBPROC qglGetUniformivARB;
PFNGLLINKPROGRAMARBPROC qglLinkProgramARB;
PFNGLSHADERSOURCEARBPROC qglShaderSourceARB;
PFNGLUNIFORM1FARBPROC qglUniform1fARB;
PFNGLUNIFORM1FVARBPROC qglUniform1fvARB;
PFNGLUNIFORM1IARBPROC qglUniform1iARB;
PFNGLUNIFORM1IVARBPROC qglUniform1ivARB;
PFNGLUNIFORM2FARBPROC qglUniform2fARB;
PFNGLUNIFORM2FVARBPROC qglUniform2fvARB;
PFNGLUNIFORM2IARBPROC qglUniform2iARB;
PFNGLUNIFORM2IVARBPROC qglUniform2ivARB;
PFNGLUNIFORM3FARBPROC qglUniform3fARB;
PFNGLUNIFORM3FVARBPROC qglUniform3fvARB;
PFNGLUNIFORM3IARBPROC qglUniform3iARB;
PFNGLUNIFORM3IVARBPROC qglUniform3ivARB;
PFNGLUNIFORM4FARBPROC qglUniform4fARB;
PFNGLUNIFORM4FVARBPROC qglUniform4fvARB;
PFNGLUNIFORM4IARBPROC qglUniform4iARB;
PFNGLUNIFORM4IVARBPROC qglUniform4ivARB;
PFNGLUNIFORMMATRIX2FVARBPROC qglUniformMatrix2fvARB;
PFNGLUNIFORMMATRIX3FVARBPROC qglUniformMatrix3fvARB;
PFNGLUNIFORMMATRIX4FVARBPROC qglUniformMatrix4fvARB;
PFNGLUSEPROGRAMOBJECTARBPROC qglUseProgramObjectARB;
PFNGLVALIDATEPROGRAMARBPROC qglValidateProgramARB;

/* ========================================================================= */

void QGL_EXT_Reset ( void )
{
	qglLockArraysEXT          = NULL;
	qglUnlockArraysEXT 	      = NULL;
	qglPointParameterfEXT     = NULL;
	qglPointParameterfvEXT    = NULL;
	qglColorTableEXT          = NULL;
	qgl3DfxSetPaletteEXT      = NULL;
	qglMultiTexCoord2fARB     = NULL;
	qglActiveTextureARB       = NULL;
	qglClientActiveTextureARB = NULL;
   
	/* ------------------------- GL_ARB_shader_objects ------------------------- */

	qglAttachObjectARB = NULL;
	qglCompileShaderARB = NULL;
	qglCreateProgramObjectARB = NULL;
	qglCreateShaderObjectARB = NULL;
	qglDeleteObjectARB = NULL;
	qglDetachObjectARB = NULL;
	qglGetActiveUniformARB = NULL;
	qglGetAttachedObjectsARB = NULL;
	qglGetHandleARB = NULL;
	qglGetInfoLogARB = NULL;
	qglGetObjectParameterfvARB = NULL;
	qglGetObjectParameterivARB = NULL;
	qglGetShaderSourceARB = NULL;
	qglGetUniformLocationARB = NULL;
	qglGetUniformfvARB = NULL;
	qglGetUniformivARB = NULL;
	qglLinkProgramARB = NULL;
	qglShaderSourceARB = NULL;
	qglUniform1fARB = NULL;
	qglUniform1fvARB = NULL;
	qglUniform1iARB = NULL;
	qglUniform1ivARB = NULL;
	qglUniform2fARB = NULL;
	qglUniform2fvARB = NULL;
	qglUniform2iARB = NULL;
	qglUniform2ivARB = NULL;
	qglUniform3fARB = NULL;
	qglUniform3fvARB = NULL;
	qglUniform3iARB = NULL;
	qglUniform3ivARB = NULL;
	qglUniform4fARB = NULL;
	qglUniform4fvARB = NULL;
	qglUniform4iARB = NULL;
	qglUniform4ivARB = NULL;
	qglUniformMatrix2fvARB = NULL;
	qglUniformMatrix3fvARB = NULL;
	qglUniformMatrix4fvARB = NULL;
	qglUseProgramObjectARB = NULL;
	qglValidateProgramARB = NULL;	
}

/* ========================================================================= */

void
QGL_Shutdown ( void )
{
	// Reset GL extension pointers
	QGL_EXT_Reset();
}

/* ========================================================================= */

qboolean
QGL_Init (void)
{
	// Reset GL extension pointers
	QGL_EXT_Reset();
	return true;
}

