/*
 * Copyright (C) 2016 Edd Biddulph
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
void ( APIENTRY *qglMultiTexCoord3fARB ) ( GLenum, GLfloat, GLfloat, GLfloat );
void ( APIENTRY *qglMultiTexCoord4fARB ) ( GLenum, GLfloat, GLfloat, GLfloat, GLfloat );
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

/* -------------------------- GL_ARB_vertex_shader ------------------------- */

PFNGLBINDATTRIBLOCATIONARBPROC qglBindAttribLocationARB;
PFNGLGETACTIVEATTRIBARBPROC qglGetActiveAttribARB;
PFNGLGETATTRIBLOCATIONARBPROC qglGetAttribLocationARB;

/* ---------------------- GL_ARB_texture_buffer_object --------------------- */

PFNGLTEXBUFFERARBPROC qglTexBufferARB;

/* ---------------------- GL_EXT_texture_buffer_object --------------------- */

PFNGLTEXBUFFEREXTPROC qglTexBufferEXT;

/* ----------------------------- GL_VERSION_3_1 ---------------------------- */

PFNGLDRAWARRAYSINSTANCEDPROC qglDrawArraysInstanced;
PFNGLDRAWELEMENTSINSTANCEDPROC qglDrawElementsInstanced;
PFNGLPRIMITIVERESTARTINDEXPROC qglPrimitiveRestartIndex;
PFNGLTEXBUFFERPROC qglTexBuffer;


/* ---------------------- GL_ARB_vertex_buffer_object ---------------------- */

PFNGLBINDBUFFERARBPROC qglBindBufferARB;
PFNGLBUFFERDATAARBPROC qglBufferDataARB;
PFNGLBUFFERSUBDATAARBPROC qglBufferSubDataARB;
PFNGLDELETEBUFFERSARBPROC qglDeleteBuffersARB;
PFNGLGENBUFFERSARBPROC qglGenBuffersARB;
PFNGLGETBUFFERPARAMETERIVARBPROC qglGetBufferParameterivARB;
PFNGLGETBUFFERPOINTERVARBPROC qglGetBufferPointervARB;
PFNGLGETBUFFERSUBDATAARBPROC qglGetBufferSubDataARB;
PFNGLISBUFFERARBPROC qglIsBufferARB;
PFNGLMAPBUFFERARBPROC qglMapBufferARB;
PFNGLUNMAPBUFFERARBPROC qglUnmapBufferARB;

/* ----------------------------- GL_VERSION_1_5 ---------------------------- */

PFNGLBEGINQUERYPROC qglBeginQuery;
PFNGLBINDBUFFERPROC qglBindBuffer;
PFNGLBUFFERDATAPROC qglBufferData;
PFNGLBUFFERSUBDATAPROC qglBufferSubData;
PFNGLDELETEBUFFERSPROC qglDeleteBuffers;
PFNGLDELETEQUERIESPROC qglDeleteQueries;
PFNGLENDQUERYPROC qglEndQuery;
PFNGLGENBUFFERSPROC qglGenBuffers;
PFNGLGENQUERIESPROC qglGenQueries;
PFNGLGETBUFFERPARAMETERIVPROC qglGetBufferParameteriv;
PFNGLGETBUFFERPOINTERVPROC qglGetBufferPointerv;
PFNGLGETBUFFERSUBDATAPROC qglGetBufferSubData;
PFNGLGETQUERYOBJECTIVPROC qglGetQueryObjectiv;
PFNGLGETQUERYOBJECTUIVPROC qglGetQueryObjectuiv;
PFNGLGETQUERYIVPROC qglGetQueryiv;
PFNGLISBUFFERPROC qglIsBuffer;
PFNGLISQUERYPROC qglIsQuery;
PFNGLMAPBUFFERPROC qglMapBuffer;
PFNGLUNMAPBUFFERPROC qglUnmapBuffer;

/* ------------------------ GL_ARB_map_buffer_range ------------------------ */

PFNGLFLUSHMAPPEDBUFFERRANGEPROC qglFlushMappedBufferRange;
PFNGLMAPBUFFERRANGEPROC qglMapBufferRange;

/* ----------------------------- GL_VERSION_1_2 ---------------------------- */

PFNGLCOPYTEXSUBIMAGE3DPROC qglCopyTexSubImage3D;
PFNGLDRAWRANGEELEMENTSPROC qglDrawRangeElements;
PFNGLTEXIMAGE3DPROC qglTexImage3D;
PFNGLTEXSUBIMAGE3DPROC qglTexSubImage3D;

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
	qglMultiTexCoord3fARB     = NULL;
	qglMultiTexCoord4fARB     = NULL;
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

	/* -------------------------- GL_ARB_vertex_shader ------------------------- */
	
	qglBindAttribLocationARB = NULL;
	qglGetActiveAttribARB = NULL;
	qglGetAttribLocationARB = NULL;
	
	/* ---------------------- GL_ARB_texture_buffer_object --------------------- */

	qglTexBufferARB = NULL;

	/* ---------------------- GL_EXT_texture_buffer_object --------------------- */

	qglTexBufferEXT = NULL;
	
	/* ----------------------------- GL_VERSION_3_1 ---------------------------- */

	qglDrawArraysInstanced = NULL;
	qglDrawElementsInstanced = NULL;
	qglPrimitiveRestartIndex = NULL;
	qglTexBuffer = NULL;

	/* ---------------------- GL_ARB_vertex_buffer_object ---------------------- */

	qglBindBufferARB = NULL;
	qglBufferDataARB = NULL;
	qglBufferSubDataARB = NULL;
	qglDeleteBuffersARB = NULL;
	qglGenBuffersARB = NULL;
	qglGetBufferParameterivARB = NULL;
	qglGetBufferPointervARB = NULL;
	qglGetBufferSubDataARB = NULL;
	qglIsBufferARB = NULL;
	qglMapBufferARB = NULL;
	qglUnmapBufferARB = NULL;
	
	/* ----------------------------- GL_VERSION_1_5 ---------------------------- */

	qglBeginQuery = NULL;
	qglBindBuffer = NULL;
	qglBufferData = NULL;
	qglBufferSubData = NULL;
	qglDeleteBuffers = NULL;
	qglDeleteQueries = NULL;
	qglEndQuery = NULL;
	qglGenBuffers = NULL;
	qglGenQueries = NULL;
	qglGetBufferParameteriv = NULL;
	qglGetBufferPointerv = NULL;
	qglGetBufferSubData = NULL;
	qglGetQueryObjectiv = NULL;
	qglGetQueryObjectuiv = NULL;
	qglGetQueryiv = NULL;
	qglIsBuffer = NULL;
	qglIsQuery = NULL;
	qglMapBuffer = NULL;
	qglUnmapBuffer = NULL;
	
	/* ------------------------ GL_ARB_map_buffer_range ------------------------ */

	qglFlushMappedBufferRange = NULL;
	qglMapBufferRange = NULL;
	
	/* ----------------------------- GL_VERSION_1_2 ---------------------------- */

	qglCopyTexSubImage3D = NULL;
	qglDrawRangeElements = NULL;
	qglTexImage3D = NULL;
	qglTexSubImage3D = NULL;
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

