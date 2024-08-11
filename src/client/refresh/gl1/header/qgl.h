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

#ifdef YQ2_GL1_GLES
#include "../glad-gles1/include/glad/glad.h"
#else
#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

// Extracted from <glext.h>
#ifndef GL_VERSION_1_4
#define GL_POINT_SIZE_MIN                 0x8126
#define GL_POINT_SIZE_MAX                 0x8127
#define GL_POINT_DISTANCE_ATTENUATION     0x8129
#define GL_GENERATE_MIPMAP                0x8191
#endif

#ifndef GL_VERSION_1_3
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1
#define GL_MULTISAMPLE                    0x809D
#define GL_COMBINE                        0x8570
#define GL_RGB_SCALE                      0x8573
#endif

#ifndef GL_EXT_shared_texture_palette
#define GL_SHARED_TEXTURE_PALETTE_EXT     0x81FB
#endif

#ifndef GL_EXT_texture_filter_anisotropic
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#ifndef GL_NV_multisample_filter_hint
#define GL_MULTISAMPLE_FILTER_HINT_NV     0x8534
#endif

// =======================================================================

/*
 * This is responsible for setting up our QGL extension pointers
 */
void QGL_Init ( void );

/*
 * Unloads the specified DLL then nulls out all the proc pointers.
 */
void QGL_Shutdown ( void );

/* GL extensions */
extern void ( APIENTRY *qglPointParameterf ) ( GLenum param, GLfloat value );
extern void ( APIENTRY *qglPointParameterfv ) ( GLenum param,
		const GLfloat *value );
extern void ( APIENTRY *qglColorTableEXT ) ( GLenum, GLenum, GLsizei, GLenum,
		GLenum, const GLvoid * );
extern void ( APIENTRY *qglActiveTexture ) ( GLenum texture );
extern void ( APIENTRY *qglClientActiveTexture ) ( GLenum texture );
extern void ( APIENTRY *qglDiscardFramebufferEXT ) ( GLenum target,
		GLsizei numAttachments, const GLenum *attachments );

#endif
