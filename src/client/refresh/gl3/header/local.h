/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016 Daniel Gibson
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
 * Local header for the OpenGL3 refresher.
 *
 * =======================================================================
 */


#ifndef SRC_CLIENT_REFRESH_GL3_HEADER_LOCAL_H_
#define SRC_CLIENT_REFRESH_GL3_HEADER_LOCAL_H_

#include "../../../header/ref.h"

typedef struct
{
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	//const char *extensions_string; deprecated in GL3

	int major_version;
	int minor_version;

	// ----

	qboolean anisotropic; // is GL_EXT_texture_filter_anisotropic supported?
	qboolean debug_output; // is GL_ARB_debug_output supported?

	// ----

	float max_anisotropy;
} gl3config_t;

typedef struct
{
	// TODO: what of this do we need?
	float inverse_intensity;
	qboolean fullscreen;

	int prev_mode;

	unsigned char *d_16to8table;

	int lightmap_textures;

	int currenttextures[2];
	int currenttmu;
	//GLenum currenttarget;

	//float camera_separation;
	//enum stereo_modes stereo_mode;

	//qboolean hwgamma;
} gl3state_t;

extern gl3config_t gl3config;
extern gl3state_t gl3state;

extern int GL3_PrepareForWindow(void);
extern int GL3_InitContext(void* win);
extern void GL3_EndFrame(void);
extern void GL3_ShutdownWindow(qboolean contextOnly);

extern int GL3_Draw_GetPalette(void);

extern void R_Printf(int level, const char* msg, ...) __attribute__((format (printf, 2, 3)));

extern void LoadPCX(char *filename, byte **pic, byte **palette, int *width, int *height);

extern cvar_t *gl_msaa_samples;
extern cvar_t *gl_swapinterval;
extern cvar_t *gl_retexturing;
extern cvar_t *vid_fullscreen;
extern cvar_t *gl_mode;
extern cvar_t *gl_customwidth;
extern cvar_t *gl_customheight;
extern cvar_t *vid_gamma;

extern cvar_t *gl3_debugcontext;

#endif /* SRC_CLIENT_REFRESH_GL3_HEADER_LOCAL_H_ */
