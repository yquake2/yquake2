/*
 * Copyright (C) 2013 Alejandro Ricoveri
 * Copyright (C) 1999-2005 Id Software, Inc.
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
 * Quake GL prototypes based on ioquake3 source code
 *
 * =======================================================================
 */

#ifndef REF_QGL_H
#define REF_QGL_H

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

#define GL_SHARED_TEXTURE_PALETTE_EXT 0x81FB

#define GL_TEXTURE0_SGIS 0x835E
#define GL_TEXTURE1_SGIS 0x835F

#define GL_POINT_SIZE_MIN_EXT 0x8126
#define GL_POINT_SIZE_MAX_EXT 0x8127
#define GL_DISTANCE_ATTENUATION_EXT 0x8129

#ifndef GL_EXT_texture_env_combine
#define GL_COMBINE_EXT 0x8570
#define GL_COMBINE_RGB_EXT 0x8571
#define GL_COMBINE_ALPHA_EXT 0x8572
#define GL_RGB_SCALE_EXT 0x8573
#define GL_ADD_SIGNED_EXT 0x8574
#define GL_INTERPOLATE_EXT 0x8575
#define GL_CONSTANT_EXT 0x8576
#define GL_PRIMARY_COLOR_EXT 0x8577
#define GL_PREVIOUS_EXT 0x8578
#define GL_SOURCE0_RGB_EXT 0x8580
#define GL_SOURCE1_RGB_EXT 0x8581
#define GL_SOURCE2_RGB_EXT 0x8582
#define GL_SOURCE3_RGB_EXT 0x8583
#define GL_SOURCE4_RGB_EXT 0x8584
#define GL_SOURCE5_RGB_EXT 0x8585
#define GL_SOURCE6_RGB_EXT 0x8586
#define GL_SOURCE7_RGB_EXT 0x8587
#define GL_SOURCE0_ALPHA_EXT 0x8588
#define GL_SOURCE1_ALPHA_EXT 0x8589
#define GL_SOURCE2_ALPHA_EXT 0x858A
#define GL_SOURCE3_ALPHA_EXT 0x858B
#define GL_SOURCE4_ALPHA_EXT 0x858C
#define GL_SOURCE5_ALPHA_EXT 0x858D
#define GL_SOURCE6_ALPHA_EXT 0x858E
#define GL_SOURCE7_ALPHA_EXT 0x858F
#define GL_OPERAND0_RGB_EXT 0x8590
#define GL_OPERAND1_RGB_EXT 0x8591
#define GL_OPERAND2_RGB_EXT 0x8592
#define GL_OPERAND3_RGB_EXT 0x8593
#define GL_OPERAND4_RGB_EXT 0x8594
#define GL_OPERAND5_RGB_EXT 0x8595
#define GL_OPERAND6_RGB_EXT 0x8596
#define GL_OPERAND7_RGB_EXT 0x8597
#define GL_OPERAND0_ALPHA_EXT 0x8598
#define GL_OPERAND1_ALPHA_EXT 0x8599
#define GL_OPERAND2_ALPHA_EXT 0x859A
#define GL_OPERAND3_ALPHA_EXT 0x859B
#define GL_OPERAND4_ALPHA_EXT 0x859C
#define GL_OPERAND5_ALPHA_EXT 0x859D
#define GL_OPERAND6_ALPHA_EXT 0x859E
#define GL_OPERAND7_ALPHA_EXT 0x859F
#endif

/* QGL main functions */

/*
 * This is responsible for setting up our QGL extension pointers
 */
qboolean QGL_Init ( void );

/*
 * Unloads the specified DLL then nulls out all the proc pointers.
 */
void QGL_Shutdown ( void );

/* GL extensions */
extern void ( APIENTRY *qglPointParameterfEXT ) ( GLenum param, GLfloat value );
extern void ( APIENTRY *qglPointParameterfvEXT ) ( GLenum param,
		const GLfloat *value );
extern void ( APIENTRY *qglColorTableEXT ) ( GLenum, GLenum, GLsizei, GLenum,
		GLenum, const GLvoid * );
extern void ( APIENTRY *qglLockArraysEXT ) ( int, int );
extern void ( APIENTRY *qglUnlockArraysEXT ) ( void );
extern void ( APIENTRY *qglMultiTexCoord2fARB) ( GLenum, GLfloat, GLfloat );
extern void ( APIENTRY *qglActiveTextureARB ) ( GLenum );
extern void ( APIENTRY *qglClientActiveTextureARB ) ( GLenum );


/* ------------------------- GL_ARB_shader_objects ------------------------- */

#ifndef GL_ARB_shader_objects
#define GL_ARB_shader_objects 1

#define GL_PROGRAM_OBJECT_ARB 0x8B40
#define GL_SHADER_OBJECT_ARB 0x8B48
#define GL_OBJECT_TYPE_ARB 0x8B4E
#define GL_OBJECT_SUBTYPE_ARB 0x8B4F
#define GL_FLOAT_VEC2_ARB 0x8B50
#define GL_FLOAT_VEC3_ARB 0x8B51
#define GL_FLOAT_VEC4_ARB 0x8B52
#define GL_INT_VEC2_ARB 0x8B53
#define GL_INT_VEC3_ARB 0x8B54
#define GL_INT_VEC4_ARB 0x8B55
#define GL_BOOL_ARB 0x8B56
#define GL_BOOL_VEC2_ARB 0x8B57
#define GL_BOOL_VEC3_ARB 0x8B58
#define GL_BOOL_VEC4_ARB 0x8B59
#define GL_FLOAT_MAT2_ARB 0x8B5A
#define GL_FLOAT_MAT3_ARB 0x8B5B
#define GL_FLOAT_MAT4_ARB 0x8B5C
#define GL_SAMPLER_1D_ARB 0x8B5D
#define GL_SAMPLER_2D_ARB 0x8B5E
#define GL_SAMPLER_3D_ARB 0x8B5F
#define GL_SAMPLER_CUBE_ARB 0x8B60
#define GL_SAMPLER_1D_SHADOW_ARB 0x8B61
#define GL_SAMPLER_2D_SHADOW_ARB 0x8B62
#define GL_SAMPLER_2D_RECT_ARB 0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW_ARB 0x8B64
#define GL_OBJECT_DELETE_STATUS_ARB 0x8B80
#define GL_OBJECT_COMPILE_STATUS_ARB 0x8B81
#define GL_OBJECT_LINK_STATUS_ARB 0x8B82
#define GL_OBJECT_VALIDATE_STATUS_ARB 0x8B83
#define GL_OBJECT_INFO_LOG_LENGTH_ARB 0x8B84
#define GL_OBJECT_ATTACHED_OBJECTS_ARB 0x8B85
#define GL_OBJECT_ACTIVE_UNIFORMS_ARB 0x8B86
#define GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB 0x8B87
#define GL_OBJECT_SHADER_SOURCE_LENGTH_ARB 0x8B88

typedef char GLcharARB;
typedef unsigned int GLhandleARB;

typedef void ( APIENTRY * PFNGLATTACHOBJECTARBPROC) (GLhandleARB containerObj, GLhandleARB obj);
typedef void ( APIENTRY * PFNGLCOMPILESHADERARBPROC) (GLhandleARB shaderObj);
typedef GLhandleARB ( APIENTRY * PFNGLCREATEPROGRAMOBJECTARBPROC) (void);
typedef GLhandleARB ( APIENTRY * PFNGLCREATESHADEROBJECTARBPROC) (GLenum shaderType);
typedef void ( APIENTRY * PFNGLDELETEOBJECTARBPROC) (GLhandleARB obj);
typedef void ( APIENTRY * PFNGLDETACHOBJECTARBPROC) (GLhandleARB containerObj, GLhandleARB attachedObj);
typedef void ( APIENTRY * PFNGLGETACTIVEUNIFORMARBPROC) (GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei* length, GLint *size, GLenum *type, GLcharARB *name);
typedef void ( APIENTRY * PFNGLGETATTACHEDOBJECTSARBPROC) (GLhandleARB containerObj, GLsizei maxCount, GLsizei* count, GLhandleARB *obj);
typedef GLhandleARB ( APIENTRY * PFNGLGETHANDLEARBPROC) (GLenum pname);
typedef void ( APIENTRY * PFNGLGETINFOLOGARBPROC) (GLhandleARB obj, GLsizei maxLength, GLsizei* length, GLcharARB *infoLog);
typedef void ( APIENTRY * PFNGLGETOBJECTPARAMETERFVARBPROC) (GLhandleARB obj, GLenum pname, GLfloat* params);
typedef void ( APIENTRY * PFNGLGETOBJECTPARAMETERIVARBPROC) (GLhandleARB obj, GLenum pname, GLint* params);
typedef void ( APIENTRY * PFNGLGETSHADERSOURCEARBPROC) (GLhandleARB obj, GLsizei maxLength, GLsizei* length, GLcharARB *source);
typedef GLint ( APIENTRY * PFNGLGETUNIFORMLOCATIONARBPROC) (GLhandleARB programObj, const GLcharARB* name);
typedef void ( APIENTRY * PFNGLGETUNIFORMFVARBPROC) (GLhandleARB programObj, GLint location, GLfloat* params);
typedef void ( APIENTRY * PFNGLGETUNIFORMIVARBPROC) (GLhandleARB programObj, GLint location, GLint* params);
typedef void ( APIENTRY * PFNGLLINKPROGRAMARBPROC) (GLhandleARB programObj);
typedef void ( APIENTRY * PFNGLSHADERSOURCEARBPROC) (GLhandleARB shaderObj, GLsizei count, const GLcharARB ** string, const GLint *length);
typedef void ( APIENTRY * PFNGLUNIFORM1FARBPROC) (GLint location, GLfloat v0);
typedef void ( APIENTRY * PFNGLUNIFORM1FVARBPROC) (GLint location, GLsizei count, const GLfloat* value);
typedef void ( APIENTRY * PFNGLUNIFORM1IARBPROC) (GLint location, GLint v0);
typedef void ( APIENTRY * PFNGLUNIFORM1IVARBPROC) (GLint location, GLsizei count, const GLint* value);
typedef void ( APIENTRY * PFNGLUNIFORM2FARBPROC) (GLint location, GLfloat v0, GLfloat v1);
typedef void ( APIENTRY * PFNGLUNIFORM2FVARBPROC) (GLint location, GLsizei count, const GLfloat* value);
typedef void ( APIENTRY * PFNGLUNIFORM2IARBPROC) (GLint location, GLint v0, GLint v1);
typedef void ( APIENTRY * PFNGLUNIFORM2IVARBPROC) (GLint location, GLsizei count, const GLint* value);
typedef void ( APIENTRY * PFNGLUNIFORM3FARBPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void ( APIENTRY * PFNGLUNIFORM3FVARBPROC) (GLint location, GLsizei count, const GLfloat* value);
typedef void ( APIENTRY * PFNGLUNIFORM3IARBPROC) (GLint location, GLint v0, GLint v1, GLint v2);
typedef void ( APIENTRY * PFNGLUNIFORM3IVARBPROC) (GLint location, GLsizei count, const GLint* value);
typedef void ( APIENTRY * PFNGLUNIFORM4FARBPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void ( APIENTRY * PFNGLUNIFORM4FVARBPROC) (GLint location, GLsizei count, const GLfloat* value);
typedef void ( APIENTRY * PFNGLUNIFORM4IARBPROC) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void ( APIENTRY * PFNGLUNIFORM4IVARBPROC) (GLint location, GLsizei count, const GLint* value);
typedef void ( APIENTRY * PFNGLUNIFORMMATRIX2FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void ( APIENTRY * PFNGLUNIFORMMATRIX3FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void ( APIENTRY * PFNGLUNIFORMMATRIX4FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void ( APIENTRY * PFNGLUSEPROGRAMOBJECTARBPROC) (GLhandleARB programObj);
typedef void ( APIENTRY * PFNGLVALIDATEPROGRAMARBPROC) (GLhandleARB programObj);

extern PFNGLATTACHOBJECTARBPROC qglAttachObjectARB;
extern PFNGLCOMPILESHADERARBPROC qglCompileShaderARB;
extern PFNGLCREATEPROGRAMOBJECTARBPROC qglCreateProgramObjectARB;
extern PFNGLCREATESHADEROBJECTARBPROC qglCreateShaderObjectARB;
extern PFNGLDELETEOBJECTARBPROC qglDeleteObjectARB;
extern PFNGLDETACHOBJECTARBPROC qglDetachObjectARB;
extern PFNGLGETACTIVEUNIFORMARBPROC qglGetActiveUniformARB;
extern PFNGLGETATTACHEDOBJECTSARBPROC qglGetAttachedObjectsARB;
extern PFNGLGETHANDLEARBPROC qglGetHandleARB;
extern PFNGLGETINFOLOGARBPROC qglGetInfoLogARB;
extern PFNGLGETOBJECTPARAMETERFVARBPROC qglGetObjectParameterfvARB;
extern PFNGLGETOBJECTPARAMETERIVARBPROC qglGetObjectParameterivARB;
extern PFNGLGETSHADERSOURCEARBPROC qglGetShaderSourceARB;
extern PFNGLGETUNIFORMLOCATIONARBPROC qglGetUniformLocationARB;
extern PFNGLGETUNIFORMFVARBPROC qglGetUniformfvARB;
extern PFNGLGETUNIFORMIVARBPROC qglGetUniformivARB;
extern PFNGLLINKPROGRAMARBPROC qglLinkProgramARB;
extern PFNGLSHADERSOURCEARBPROC qglShaderSourceARB;
extern PFNGLUNIFORM1FARBPROC qglUniform1fARB;
extern PFNGLUNIFORM1FVARBPROC qglUniform1fvARB;
extern PFNGLUNIFORM1IARBPROC qglUniform1iARB;
extern PFNGLUNIFORM1IVARBPROC qglUniform1ivARB;
extern PFNGLUNIFORM2FARBPROC qglUniform2fARB;
extern PFNGLUNIFORM2FVARBPROC qglUniform2fvARB;
extern PFNGLUNIFORM2IARBPROC qglUniform2iARB;
extern PFNGLUNIFORM2IVARBPROC qglUniform2ivARB;
extern PFNGLUNIFORM3FARBPROC qglUniform3fARB;
extern PFNGLUNIFORM3FVARBPROC qglUniform3fvARB;
extern PFNGLUNIFORM3IARBPROC qglUniform3iARB;
extern PFNGLUNIFORM3IVARBPROC qglUniform3ivARB;
extern PFNGLUNIFORM4FARBPROC qglUniform4fARB;
extern PFNGLUNIFORM4FVARBPROC qglUniform4fvARB;
extern PFNGLUNIFORM4IARBPROC qglUniform4iARB;
extern PFNGLUNIFORM4IVARBPROC qglUniform4ivARB;
extern PFNGLUNIFORMMATRIX2FVARBPROC qglUniformMatrix2fvARB;
extern PFNGLUNIFORMMATRIX3FVARBPROC qglUniformMatrix3fvARB;
extern PFNGLUNIFORMMATRIX4FVARBPROC qglUniformMatrix4fvARB;
extern PFNGLUSEPROGRAMOBJECTARBPROC qglUseProgramObjectARB;
extern PFNGLVALIDATEPROGRAMARBPROC qglValidateProgramARB;

#endif /* GL_ARB_shader_objects */

#endif
