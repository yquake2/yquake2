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

#define GL_TEXTURE0_ARB 0x84C0
#define GL_TEXTURE1_ARB 0x84C1
#define GL_TEXTURE2_ARB 0x84C2
#define GL_TEXTURE3_ARB 0x84C3
#define GL_TEXTURE4_ARB 0x84C4
#define GL_TEXTURE5_ARB 0x84C5
#define GL_TEXTURE6_ARB 0x84C6
#define GL_TEXTURE7_ARB 0x84C7
#define GL_TEXTURE8_ARB 0x84C8
#define GL_TEXTURE9_ARB 0x84C9
#define GL_TEXTURE10_ARB 0x84CA

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
extern void ( APIENTRY *qglMultiTexCoord3fARB) ( GLenum, GLfloat, GLfloat, GLfloat );
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

/* -------------------------- GL_ARB_vertex_shader ------------------------- */

#ifndef GL_ARB_vertex_shader
#define GL_ARB_vertex_shader 1

#define GL_VERTEX_SHADER_ARB 0x8B31
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB 0x8B4A
#define GL_MAX_VARYING_FLOATS_ARB 0x8B4B
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB 0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB 0x8B4D
#define GL_OBJECT_ACTIVE_ATTRIBUTES_ARB 0x8B89
#define GL_OBJECT_ACTIVE_ATTRIBUTE_MAX_LENGTH_ARB 0x8B8A

typedef void ( APIENTRY * PFNGLBINDATTRIBLOCATIONARBPROC) (GLhandleARB programObj, GLuint index, const GLcharARB* name);
typedef void ( APIENTRY * PFNGLGETACTIVEATTRIBARBPROC) (GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei* length, GLint *size, GLenum *type, GLcharARB *name);
typedef GLint ( APIENTRY * PFNGLGETATTRIBLOCATIONARBPROC) (GLhandleARB programObj, const GLcharARB* name);

extern PFNGLBINDATTRIBLOCATIONARBPROC qglBindAttribLocationARB;
extern PFNGLGETACTIVEATTRIBARBPROC qglGetActiveAttribARB;
extern PFNGLGETATTRIBLOCATIONARBPROC qglGetAttribLocationARB;

#endif /* GL_ARB_vertex_shader */

/* ------------------------- GL_ARB_fragment_shader ------------------------ */

#ifndef GL_ARB_fragment_shader
#define GL_ARB_fragment_shader 1

#define GL_FRAGMENT_SHADER_ARB 0x8B30
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB 0x8B49
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB 0x8B8B

#endif /* GL_ARB_fragment_shader */

/* -------------------------- GL_ARB_texture_float ------------------------- */

#ifndef GL_ARB_texture_float
#define GL_ARB_texture_float 1

#define GL_RGBA32F_ARB 0x8814
#define GL_RGB32F_ARB 0x8815
#define GL_ALPHA32F_ARB 0x8816
#define GL_INTENSITY32F_ARB 0x8817
#define GL_LUMINANCE32F_ARB 0x8818
#define GL_LUMINANCE_ALPHA32F_ARB 0x8819
#define GL_RGBA16F_ARB 0x881A
#define GL_RGB16F_ARB 0x881B
#define GL_ALPHA16F_ARB 0x881C
#define GL_INTENSITY16F_ARB 0x881D
#define GL_LUMINANCE16F_ARB 0x881E
#define GL_LUMINANCE_ALPHA16F_ARB 0x881F
#define GL_TEXTURE_RED_TYPE_ARB 0x8C10
#define GL_TEXTURE_GREEN_TYPE_ARB 0x8C11
#define GL_TEXTURE_BLUE_TYPE_ARB 0x8C12
#define GL_TEXTURE_ALPHA_TYPE_ARB 0x8C13
#define GL_TEXTURE_LUMINANCE_TYPE_ARB 0x8C14
#define GL_TEXTURE_INTENSITY_TYPE_ARB 0x8C15
#define GL_TEXTURE_DEPTH_TYPE_ARB 0x8C16
#define GL_UNSIGNED_NORMALIZED_ARB 0x8C17

#endif /* GL_ARB_texture_float */

/* ---------------------- GL_ARB_texture_buffer_object --------------------- */

#ifndef GL_ARB_texture_buffer_object
#define GL_ARB_texture_buffer_object 1

#define GL_TEXTURE_BUFFER_ARB 0x8C2A
#define GL_MAX_TEXTURE_BUFFER_SIZE_ARB 0x8C2B
#define GL_TEXTURE_BINDING_BUFFER_ARB 0x8C2C
#define GL_TEXTURE_BUFFER_DATA_STORE_BINDING_ARB 0x8C2D
#define GL_TEXTURE_BUFFER_FORMAT_ARB 0x8C2E

typedef void ( APIENTRY * PFNGLTEXBUFFERARBPROC) (GLenum target, GLenum internalformat, GLuint buffer);

extern PFNGLTEXBUFFERARBPROC qglTexBufferARB;

#endif /* GL_ARB_texture_buffer_object */

/* ---------------------- GL_EXT_texture_buffer_object --------------------- */

#ifndef GL_EXT_texture_buffer_object
#define GL_EXT_texture_buffer_object 1

#define GL_TEXTURE_BUFFER_EXT 0x8C2A
#define GL_MAX_TEXTURE_BUFFER_SIZE_EXT 0x8C2B
#define GL_TEXTURE_BINDING_BUFFER_EXT 0x8C2C
#define GL_TEXTURE_BUFFER_DATA_STORE_BINDING_EXT 0x8C2D
#define GL_TEXTURE_BUFFER_FORMAT_EXT 0x8C2E

typedef void ( APIENTRY * PFNGLTEXBUFFEREXTPROC) (GLenum target, GLenum internalformat, GLuint buffer);

extern PFNGLTEXBUFFEREXTPROC qglTexBufferEXT;

#endif /* GL_EXT_texture_buffer_object */

/* ----------------------------- GL_VERSION_3_0 ---------------------------- */

#ifndef GL_VERSION_3_0
#define GL_VERSION_3_0 1

#define GL_CLIP_DISTANCE0 GL_CLIP_PLANE0
#define GL_CLIP_DISTANCE1 GL_CLIP_PLANE1
#define GL_CLIP_DISTANCE2 GL_CLIP_PLANE2
#define GL_CLIP_DISTANCE3 GL_CLIP_PLANE3
#define GL_CLIP_DISTANCE4 GL_CLIP_PLANE4
#define GL_CLIP_DISTANCE5 GL_CLIP_PLANE5
#define GL_COMPARE_REF_TO_TEXTURE GL_COMPARE_R_TO_TEXTURE_ARB
#define GL_MAX_CLIP_DISTANCES GL_MAX_CLIP_PLANES
#define GL_MAX_VARYING_COMPONENTS GL_MAX_VARYING_FLOATS
#define GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT 0x0001
#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C
#define GL_NUM_EXTENSIONS 0x821D
#define GL_CONTEXT_FLAGS 0x821E
#define GL_DEPTH_BUFFER 0x8223
#define GL_STENCIL_BUFFER 0x8224
#define GL_RGBA32F 0x8814
#define GL_RGB32F 0x8815
#define GL_RGBA16F 0x881A
#define GL_RGB16F 0x881B
#define GL_VERTEX_ATTRIB_ARRAY_INTEGER 0x88FD
#define GL_MAX_ARRAY_TEXTURE_LAYERS 0x88FF
#define GL_MIN_PROGRAM_TEXEL_OFFSET 0x8904
#define GL_MAX_PROGRAM_TEXEL_OFFSET 0x8905
#define GL_CLAMP_VERTEX_COLOR 0x891A
#define GL_CLAMP_FRAGMENT_COLOR 0x891B
#define GL_CLAMP_READ_COLOR 0x891C
#define GL_FIXED_ONLY 0x891D
#define GL_TEXTURE_RED_TYPE 0x8C10
#define GL_TEXTURE_GREEN_TYPE 0x8C11
#define GL_TEXTURE_BLUE_TYPE 0x8C12
#define GL_TEXTURE_ALPHA_TYPE 0x8C13
#define GL_TEXTURE_LUMINANCE_TYPE 0x8C14
#define GL_TEXTURE_INTENSITY_TYPE 0x8C15
#define GL_TEXTURE_DEPTH_TYPE 0x8C16
#define GL_TEXTURE_1D_ARRAY 0x8C18
#define GL_PROXY_TEXTURE_1D_ARRAY 0x8C19
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#define GL_PROXY_TEXTURE_2D_ARRAY 0x8C1B
#define GL_TEXTURE_BINDING_1D_ARRAY 0x8C1C
#define GL_TEXTURE_BINDING_2D_ARRAY 0x8C1D
#define GL_R11F_G11F_B10F 0x8C3A
#define GL_UNSIGNED_INT_10F_11F_11F_REV 0x8C3B
#define GL_RGB9_E5 0x8C3D
#define GL_UNSIGNED_INT_5_9_9_9_REV 0x8C3E
#define GL_TEXTURE_SHARED_SIZE 0x8C3F
#define GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH 0x8C76
#define GL_TRANSFORM_FEEDBACK_BUFFER_MODE 0x8C7F
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS 0x8C80
#define GL_TRANSFORM_FEEDBACK_VARYINGS 0x8C83
#define GL_TRANSFORM_FEEDBACK_BUFFER_START 0x8C84
#define GL_TRANSFORM_FEEDBACK_BUFFER_SIZE 0x8C85
#define GL_PRIMITIVES_GENERATED 0x8C87
#define GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN 0x8C88
#define GL_RASTERIZER_DISCARD 0x8C89
#define GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS 0x8C8A
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS 0x8C8B
#define GL_INTERLEAVED_ATTRIBS 0x8C8C
#define GL_SEPARATE_ATTRIBS 0x8C8D
#define GL_TRANSFORM_FEEDBACK_BUFFER 0x8C8E
#define GL_TRANSFORM_FEEDBACK_BUFFER_BINDING 0x8C8F
#define GL_RGBA32UI 0x8D70
#define GL_RGB32UI 0x8D71
#define GL_RGBA16UI 0x8D76
#define GL_RGB16UI 0x8D77
#define GL_RGBA8UI 0x8D7C
#define GL_RGB8UI 0x8D7D
#define GL_RGBA32I 0x8D82
#define GL_RGB32I 0x8D83
#define GL_RGBA16I 0x8D88
#define GL_RGB16I 0x8D89
#define GL_RGBA8I 0x8D8E
#define GL_RGB8I 0x8D8F
#define GL_RED_INTEGER 0x8D94
#define GL_GREEN_INTEGER 0x8D95
#define GL_BLUE_INTEGER 0x8D96
#define GL_ALPHA_INTEGER 0x8D97
#define GL_RGB_INTEGER 0x8D98
#define GL_RGBA_INTEGER 0x8D99
#define GL_BGR_INTEGER 0x8D9A
#define GL_BGRA_INTEGER 0x8D9B
#define GL_SAMPLER_1D_ARRAY 0x8DC0
#define GL_SAMPLER_2D_ARRAY 0x8DC1
#define GL_SAMPLER_1D_ARRAY_SHADOW 0x8DC3
#define GL_SAMPLER_2D_ARRAY_SHADOW 0x8DC4
#define GL_SAMPLER_CUBE_SHADOW 0x8DC5
#define GL_UNSIGNED_INT_VEC2 0x8DC6
#define GL_UNSIGNED_INT_VEC3 0x8DC7
#define GL_UNSIGNED_INT_VEC4 0x8DC8
#define GL_INT_SAMPLER_1D 0x8DC9
#define GL_INT_SAMPLER_2D 0x8DCA
#define GL_INT_SAMPLER_3D 0x8DCB
#define GL_INT_SAMPLER_CUBE 0x8DCC
#define GL_INT_SAMPLER_1D_ARRAY 0x8DCE
#define GL_INT_SAMPLER_2D_ARRAY 0x8DCF
#define GL_UNSIGNED_INT_SAMPLER_1D 0x8DD1
#define GL_UNSIGNED_INT_SAMPLER_2D 0x8DD2
#define GL_UNSIGNED_INT_SAMPLER_3D 0x8DD3
#define GL_UNSIGNED_INT_SAMPLER_CUBE 0x8DD4
#define GL_UNSIGNED_INT_SAMPLER_1D_ARRAY 0x8DD6
#define GL_UNSIGNED_INT_SAMPLER_2D_ARRAY 0x8DD7
#define GL_QUERY_WAIT 0x8E13
#define GL_QUERY_NO_WAIT 0x8E14
#define GL_QUERY_BY_REGION_WAIT 0x8E15
#define GL_QUERY_BY_REGION_NO_WAIT 0x8E16

#endif /* GL_VERSION_3_0 */

/* ----------------------------- GL_VERSION_3_1 ---------------------------- */

#ifndef GL_VERSION_3_1
#define GL_VERSION_3_1 1

#define GL_TEXTURE_RECTANGLE 0x84F5
#define GL_TEXTURE_BINDING_RECTANGLE 0x84F6
#define GL_PROXY_TEXTURE_RECTANGLE 0x84F7
#define GL_MAX_RECTANGLE_TEXTURE_SIZE 0x84F8
#define GL_SAMPLER_2D_RECT 0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW 0x8B64
#define GL_TEXTURE_BUFFER 0x8C2A
#define GL_MAX_TEXTURE_BUFFER_SIZE 0x8C2B
#define GL_TEXTURE_BINDING_BUFFER 0x8C2C
#define GL_TEXTURE_BUFFER_DATA_STORE_BINDING 0x8C2D
#define GL_TEXTURE_BUFFER_FORMAT 0x8C2E
#define GL_SAMPLER_BUFFER 0x8DC2
#define GL_INT_SAMPLER_2D_RECT 0x8DCD
#define GL_INT_SAMPLER_BUFFER 0x8DD0
#define GL_UNSIGNED_INT_SAMPLER_2D_RECT 0x8DD5
#define GL_UNSIGNED_INT_SAMPLER_BUFFER 0x8DD8
#define GL_RED_SNORM 0x8F90
#define GL_RG_SNORM 0x8F91
#define GL_RGB_SNORM 0x8F92
#define GL_RGBA_SNORM 0x8F93
#define GL_R8_SNORM 0x8F94
#define GL_RG8_SNORM 0x8F95
#define GL_RGB8_SNORM 0x8F96
#define GL_RGBA8_SNORM 0x8F97
#define GL_R16_SNORM 0x8F98
#define GL_RG16_SNORM 0x8F99
#define GL_RGB16_SNORM 0x8F9A
#define GL_RGBA16_SNORM 0x8F9B
#define GL_SIGNED_NORMALIZED 0x8F9C
#define GL_PRIMITIVE_RESTART 0x8F9D
#define GL_PRIMITIVE_RESTART_INDEX 0x8F9E
#define GL_BUFFER_ACCESS_FLAGS 0x911F
#define GL_BUFFER_MAP_LENGTH 0x9120
#define GL_BUFFER_MAP_OFFSET 0x9121

typedef void ( APIENTRY * PFNGLDRAWARRAYSINSTANCEDPROC) (GLenum mode, GLint first, GLsizei count, GLsizei primcount);
typedef void ( APIENTRY * PFNGLDRAWELEMENTSINSTANCEDPROC) (GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei primcount);
typedef void ( APIENTRY * PFNGLPRIMITIVERESTARTINDEXPROC) (GLuint buffer);
typedef void ( APIENTRY * PFNGLTEXBUFFERPROC) (GLenum target, GLenum internalFormat, GLuint buffer);

extern PFNGLDRAWARRAYSINSTANCEDPROC qglDrawArraysInstanced;
extern PFNGLDRAWELEMENTSINSTANCEDPROC qglDrawElementsInstanced;
extern PFNGLPRIMITIVERESTARTINDEXPROC qglPrimitiveRestartIndex;
extern PFNGLTEXBUFFERPROC qglTexBuffer;

#endif /* GL_VERSION_3_1 */


/* ---------------------- GL_ARB_vertex_buffer_object ---------------------- */

#ifndef GL_ARB_vertex_buffer_object
#define GL_ARB_vertex_buffer_object 1

#define GL_BUFFER_SIZE_ARB 0x8764
#define GL_BUFFER_USAGE_ARB 0x8765
#define GL_ARRAY_BUFFER_ARB 0x8892
#define GL_ELEMENT_ARRAY_BUFFER_ARB 0x8893
#define GL_ARRAY_BUFFER_BINDING_ARB 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB 0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING_ARB 0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING_ARB 0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING_ARB 0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING_ARB 0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB 0x889A
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB 0x889B
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB 0x889C
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB 0x889D
#define GL_WEIGHT_ARRAY_BUFFER_BINDING_ARB 0x889E
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB 0x889F
#define GL_READ_ONLY_ARB 0x88B8
#define GL_WRITE_ONLY_ARB 0x88B9
#define GL_READ_WRITE_ARB 0x88BA
#define GL_BUFFER_ACCESS_ARB 0x88BB
#define GL_BUFFER_MAPPED_ARB 0x88BC
#define GL_BUFFER_MAP_POINTER_ARB 0x88BD
#define GL_STREAM_DRAW_ARB 0x88E0
#define GL_STREAM_READ_ARB 0x88E1
#define GL_STREAM_COPY_ARB 0x88E2
#define GL_STATIC_DRAW_ARB 0x88E4
#define GL_STATIC_READ_ARB 0x88E5
#define GL_STATIC_COPY_ARB 0x88E6
#define GL_DYNAMIC_DRAW_ARB 0x88E8
#define GL_DYNAMIC_READ_ARB 0x88E9
#define GL_DYNAMIC_COPY_ARB 0x88EA

typedef ptrdiff_t GLintptrARB;
typedef ptrdiff_t GLsizeiptrARB;

typedef void ( APIENTRY * PFNGLBINDBUFFERARBPROC) (GLenum target, GLuint buffer);
typedef void ( APIENTRY * PFNGLBUFFERDATAARBPROC) (GLenum target, GLsizeiptrARB size, const void *data, GLenum usage);
typedef void ( APIENTRY * PFNGLBUFFERSUBDATAARBPROC) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const void *data);
typedef void ( APIENTRY * PFNGLDELETEBUFFERSARBPROC) (GLsizei n, const GLuint* buffers);
typedef void ( APIENTRY * PFNGLGENBUFFERSARBPROC) (GLsizei n, GLuint* buffers);
typedef void ( APIENTRY * PFNGLGETBUFFERPARAMETERIVARBPROC) (GLenum target, GLenum pname, GLint* params);
typedef void ( APIENTRY * PFNGLGETBUFFERPOINTERVARBPROC) (GLenum target, GLenum pname, void** params);
typedef void ( APIENTRY * PFNGLGETBUFFERSUBDATAARBPROC) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, void *data);
typedef GLboolean ( APIENTRY * PFNGLISBUFFERARBPROC) (GLuint buffer);
typedef void * ( APIENTRY * PFNGLMAPBUFFERARBPROC) (GLenum target, GLenum access);
typedef GLboolean ( APIENTRY * PFNGLUNMAPBUFFERARBPROC) (GLenum target);

extern PFNGLBINDBUFFERARBPROC qglBindBufferARB;
extern PFNGLBUFFERDATAARBPROC qglBufferDataARB;
extern PFNGLBUFFERSUBDATAARBPROC qglBufferSubDataARB;
extern PFNGLDELETEBUFFERSARBPROC qglDeleteBuffersARB;
extern PFNGLGENBUFFERSARBPROC qglGenBuffersARB;
extern PFNGLGETBUFFERPARAMETERIVARBPROC qglGetBufferParameterivARB;
extern PFNGLGETBUFFERPOINTERVARBPROC qglGetBufferPointervARB;
extern PFNGLGETBUFFERSUBDATAARBPROC qglGetBufferSubDataARB;
extern PFNGLISBUFFERARBPROC qglIsBufferARB;
extern PFNGLMAPBUFFERARBPROC qglMapBufferARB;
extern PFNGLUNMAPBUFFERARBPROC qglUnmapBufferARB;

#endif /* GL_ARB_vertex_buffer_object */

/* ----------------------------- GL_VERSION_1_5 ---------------------------- */

#ifndef GL_VERSION_1_5
#define GL_VERSION_1_5 1

#define GL_CURRENT_FOG_COORD GL_CURRENT_FOG_COORDINATE
#define GL_FOG_COORD GL_FOG_COORDINATE
#define GL_FOG_COORD_ARRAY GL_FOG_COORDINATE_ARRAY
#define GL_FOG_COORD_ARRAY_BUFFER_BINDING GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING
#define GL_FOG_COORD_ARRAY_POINTER GL_FOG_COORDINATE_ARRAY_POINTER
#define GL_FOG_COORD_ARRAY_STRIDE GL_FOG_COORDINATE_ARRAY_STRIDE
#define GL_FOG_COORD_ARRAY_TYPE GL_FOG_COORDINATE_ARRAY_TYPE
#define GL_FOG_COORD_SRC GL_FOG_COORDINATE_SOURCE
#define GL_SRC0_ALPHA GL_SOURCE0_ALPHA
#define GL_SRC0_RGB GL_SOURCE0_RGB
#define GL_SRC1_ALPHA GL_SOURCE1_ALPHA
#define GL_SRC1_RGB GL_SOURCE1_RGB
#define GL_SRC2_ALPHA GL_SOURCE2_ALPHA
#define GL_SRC2_RGB GL_SOURCE2_RGB
#define GL_BUFFER_SIZE 0x8764
#define GL_BUFFER_USAGE 0x8765
#define GL_QUERY_COUNTER_BITS 0x8864
#define GL_CURRENT_QUERY 0x8865
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING 0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING 0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING 0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING 0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING 0x889A
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING 0x889B
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING 0x889C
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING 0x889D
#define GL_WEIGHT_ARRAY_BUFFER_BINDING 0x889E
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING 0x889F
#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#define GL_BUFFER_ACCESS 0x88BB
#define GL_BUFFER_MAPPED 0x88BC
#define GL_BUFFER_MAP_POINTER 0x88BD
#define GL_STREAM_DRAW 0x88E0
#define GL_STREAM_READ 0x88E1
#define GL_STREAM_COPY 0x88E2
#define GL_STATIC_DRAW 0x88E4
#define GL_STATIC_READ 0x88E5
#define GL_STATIC_COPY 0x88E6
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_DYNAMIC_READ 0x88E9
#define GL_DYNAMIC_COPY 0x88EA
#define GL_SAMPLES_PASSED 0x8914

typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;

typedef void ( APIENTRY * PFNGLBEGINQUERYPROC) (GLenum target, GLuint id);
typedef void ( APIENTRY * PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
typedef void ( APIENTRY * PFNGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void ( APIENTRY * PFNGLBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
typedef void ( APIENTRY * PFNGLDELETEBUFFERSPROC) (GLsizei n, const GLuint* buffers);
typedef void ( APIENTRY * PFNGLDELETEQUERIESPROC) (GLsizei n, const GLuint* ids);
typedef void ( APIENTRY * PFNGLENDQUERYPROC) (GLenum target);
typedef void ( APIENTRY * PFNGLGENBUFFERSPROC) (GLsizei n, GLuint* buffers);
typedef void ( APIENTRY * PFNGLGENQUERIESPROC) (GLsizei n, GLuint* ids);
typedef void ( APIENTRY * PFNGLGETBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint* params);
typedef void ( APIENTRY * PFNGLGETBUFFERPOINTERVPROC) (GLenum target, GLenum pname, void** params);
typedef void ( APIENTRY * PFNGLGETBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, void* data);
typedef void ( APIENTRY * PFNGLGETQUERYOBJECTIVPROC) (GLuint id, GLenum pname, GLint* params);
typedef void ( APIENTRY * PFNGLGETQUERYOBJECTUIVPROC) (GLuint id, GLenum pname, GLuint* params);
typedef void ( APIENTRY * PFNGLGETQUERYIVPROC) (GLenum target, GLenum pname, GLint* params);
typedef GLboolean ( APIENTRY * PFNGLISBUFFERPROC) (GLuint buffer);
typedef GLboolean ( APIENTRY * PFNGLISQUERYPROC) (GLuint id);
typedef void* ( APIENTRY * PFNGLMAPBUFFERPROC) (GLenum target, GLenum access);
typedef GLboolean ( APIENTRY * PFNGLUNMAPBUFFERPROC) (GLenum target);

extern PFNGLBEGINQUERYPROC qglBeginQuery;
extern PFNGLBINDBUFFERPROC qglBindBuffer;
extern PFNGLBUFFERDATAPROC qglBufferData;
extern PFNGLBUFFERSUBDATAPROC qglBufferSubData;
extern PFNGLDELETEBUFFERSPROC qglDeleteBuffers;
extern PFNGLDELETEQUERIESPROC qglDeleteQueries;
extern PFNGLENDQUERYPROC qglEndQuery;
extern PFNGLGENBUFFERSPROC qglGenBuffers;
extern PFNGLGENQUERIESPROC qglGenQueries;
extern PFNGLGETBUFFERPARAMETERIVPROC qglGetBufferParameteriv;
extern PFNGLGETBUFFERPOINTERVPROC qglGetBufferPointerv;
extern PFNGLGETBUFFERSUBDATAPROC qglGetBufferSubData;
extern PFNGLGETQUERYOBJECTIVPROC qglGetQueryObjectiv;
extern PFNGLGETQUERYOBJECTUIVPROC qglGetQueryObjectuiv;
extern PFNGLGETQUERYIVPROC qglGetQueryiv;
extern PFNGLISBUFFERPROC qglIsBuffer;
extern PFNGLISQUERYPROC qglIsQuery;
extern PFNGLMAPBUFFERPROC qglMapBuffer;
extern PFNGLUNMAPBUFFERPROC qglUnmapBuffer;

#endif /* GL_VERSION_1_5 */

/* --------------------------- GL_ARB_texture_rg --------------------------- */

#ifndef GL_ARB_texture_rg
#define GL_ARB_texture_rg 1

#define GL_COMPRESSED_RED 0x8225
#define GL_COMPRESSED_RG 0x8226
#define GL_RG 0x8227
#define GL_RG_INTEGER 0x8228
#define GL_R8 0x8229
#define GL_R16 0x822A
#define GL_RG8 0x822B
#define GL_RG16 0x822C
#define GL_R16F 0x822D
#define GL_R32F 0x822E
#define GL_RG16F 0x822F
#define GL_RG32F 0x8230
#define GL_R8I 0x8231
#define GL_R8UI 0x8232
#define GL_R16I 0x8233
#define GL_R16UI 0x8234
#define GL_R32I 0x8235
#define GL_R32UI 0x8236
#define GL_RG8I 0x8237
#define GL_RG8UI 0x8238
#define GL_RG16I 0x8239
#define GL_RG16UI 0x823A
#define GL_RG32I 0x823B
#define GL_RG32UI 0x823C

#endif /* GL_ARB_texture_rg */

/* ------------------------ GL_ARB_map_buffer_range ------------------------ */

#ifndef GL_ARB_map_buffer_range
#define GL_ARB_map_buffer_range 1

#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT 0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT 0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT 0x0020

typedef void ( APIENTRY * PFNGLFLUSHMAPPEDBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length);
typedef void * ( APIENTRY * PFNGLMAPBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);

extern PFNGLFLUSHMAPPEDBUFFERRANGEPROC qglFlushMappedBufferRange;
extern PFNGLMAPBUFFERRANGEPROC qglMapBufferRange;

#endif /* GL_ARB_map_buffer_range */

#endif
