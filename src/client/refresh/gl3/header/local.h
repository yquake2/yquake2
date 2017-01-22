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
 * Local header for the OpenGL3 refresher.
 *
 * =======================================================================
 */


#ifndef SRC_CLIENT_REFRESH_GL3_HEADER_LOCAL_H_
#define SRC_CLIENT_REFRESH_GL3_HEADER_LOCAL_H_

#ifdef IN_IDE_PARSER
  // this is just a hack to get proper auto-completion in IDEs:
  // using system headers for their parsers/indexers but glad for real build
  // (in glad glFoo is just a #define to glad_glFoo or sth, which screws up autocompletion)
  // (you may have to configure your IDE to #define IN_IDE_PARSER, but not for building!)
  #define GL_GLEXT_PROTOTYPES 1
  #include <GL/gl.h>
  #include <GL/glext.h>
#else
  #include <glad/glad.h>
#endif

#include "../../ref_shared.h"

#define STUB(msg) \
	R_Printf(PRINT_ALL, "STUB: %s() %s\n", __FUNCTION__, msg)

#define STUB_ONCE(msg) do { \
		static int show=1; \
		if(show) { \
			show = 0; \
			R_Printf(PRINT_ALL, "STUB: %s() %s\n", __FUNCTION__, msg); \
		} \
	} while(0);

// a wrapper around glVertexAttribPointer() to stay sane
// (caller doesn't have to cast to GLintptr and then void*)
static inline void
qglVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset)
{
	glVertexAttribPointer(index, size, type, normalized, stride, (const void*)offset);
}

// TODO: do we need the following configurable?
static const int gl3_solid_format = GL_RGB;
static const int gl3_alpha_format = GL_RGBA;
static const int gl3_tex_solid_format = GL_RGB;
static const int gl3_tex_alpha_format = GL_RGBA;

extern unsigned gl3_rawpalette[256];
extern unsigned d_8to24table[256];

typedef struct
{
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *glsl_version_string;
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
	GLuint shaderProgram;

	GLint attribPosition;
	GLint attribTexCoord;
	GLint attribColor;

	GLint uniTransMatrix; // TODO: could use 2 or 3 matrices?

	// TODO: probably more uniforms, at least gamma and intensity

} gl3ShaderInfo_t;

typedef struct
{
	// TODO: what of this do we need?
	float inverse_intensity;
	qboolean fullscreen;

	int prev_mode;

	unsigned char *d_16to8table;

	int lightmap_textures;

	//int currenttextures[2];
	GLuint currenttexture;
	//int currenttmu;
	//GLenum currenttarget;

	//float camera_separation;
	//enum stereo_modes stereo_mode;

	//qboolean hwgamma;

	gl3ShaderInfo_t si2D; // shader for rendering 2D with textures
	gl3ShaderInfo_t si2Dcolor; // shader for rendering 2D with flat colors
	GLuint vbo2D; // this vbo is reused for all 2D drawing (HUD, movies, menu, console, ..)
	GLuint vao2D; // same for this vao

} gl3state_t;

extern gl3config_t gl3config;
extern gl3state_t gl3state;

extern viddef_t vid;

extern int gl3_viewcluster, gl3_viewcluster2, gl3_oldviewcluster, gl3_oldviewcluster2;

/* NOTE: struct image_s* is what re.RegisterSkin() etc return so no gl3image_s!
 *       (I think the client only passes the pointer around and doesn't know the
 *        definition of this struct, so this being different from struct image_s
 *        in ref_gl should be ok)
 */
typedef struct image_s
{
	// TODO: what of this is actually needed?
	char name[MAX_QPATH];               /* game path, including extension */
	imagetype_t type;
	int width, height;                  /* source image */
	int upload_width, upload_height;    /* after power of two and picmip */
	int registration_sequence;          /* 0 = free */
	struct msurface_s *texturechain;    /* for sort-by-texture world drawing */
	GLuint texnum;                      /* gl texture binding */
	float sl, tl, sh, th;               /* 0,0 - 1,1 unless part of the scrap */
	qboolean scrap;
	qboolean has_alpha;

	//qboolean paletted;
} gl3image_t;

enum {MAX_GL3TEXTURES = 1024};

// include this down here so it can use gl3image_t
#include "model.h"

extern gl3image_t *gl3_notexture; /* use for bad textures */
extern gl3image_t *gl3_particletexture; /* little dot for particles */

extern int gl_filter_min;
extern int gl_filter_max;

// gl3_sdl.c
extern int GL3_PrepareForWindow(void);
extern int GL3_InitContext(void* win);
extern void GL3_EndFrame(void);
extern void GL3_ShutdownWindow(qboolean contextOnly);

// gl3_misc.c
extern void GL3_InitParticleTexture(void);
extern void GL3_ScreenShot(void);
extern void GL3_SetDefaultState(void);

// gl3_model.c
extern int registration_sequence;
extern void GL3_Mod_Init(void);
extern void GL3_Mod_FreeAll(void);
extern void GL3_BeginRegistration(char *model);
extern struct model_s * GL3_RegisterModel(char *name);
extern void GL3_EndRegistration(void);
extern void GL3_Mod_Modellist_f(void);

// gl3_draw.c
extern void GL3_Draw_InitLocal(void);
extern void GL3_Draw_ShutdownLocal(void);
extern gl3image_t * GL3_Draw_FindPic(char *name);
extern void GL3_Draw_GetPicSize(int *w, int *h, char *pic);
extern int GL3_Draw_GetPalette(void);

extern void GL3_Draw_PicScaled(int x, int y, char *pic, float factor);
extern void GL3_Draw_StretchPic(int x, int y, int w, int h, char *pic);
extern void GL3_Draw_CharScaled(int x, int y, int num, float scale);
extern void GL3_Draw_TileClear(int x, int y, int w, int h, char *pic);
extern void GL3_Draw_Fill(int x, int y, int w, int h, int c);
extern void GL3_Draw_FadeScreen(void);
extern void GL3_Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data);

// gl3_image.c
extern void GL3_TextureMode(char *string);
extern void GL3_Bind(int texnum);
extern gl3image_t *GL3_LoadPic(char *name, byte *pic, int width, int realwidth,
                               int height, int realheight, imagetype_t type, int bits);
extern gl3image_t *GL3_FindImage(char *name, imagetype_t type);
extern gl3image_t *GL3_RegisterSkin(char *name);
extern void GL3_ShutdownImages(void);
extern void GL3_FreeUnusedImages(void);
extern void GL3_ImageList_f(void);

// gl3_warp.c
extern void GL3_SetSky(char *name, float rotate, vec3_t axis);


// gl3_shaders.c
extern qboolean GL3_InitShaders(void);
extern void GL3_ShutdownShaders(void);

// ############ Cvars ###########

extern cvar_t *gl_msaa_samples;
extern cvar_t *gl_swapinterval;
extern cvar_t *gl_retexturing;
extern cvar_t *vid_fullscreen;
extern cvar_t *gl_mode;
extern cvar_t *gl_customwidth;
extern cvar_t *gl_customheight;

extern cvar_t *gl_nolerp_list;
extern cvar_t *gl_nobind;

extern cvar_t *vid_gamma;
extern cvar_t *gl_anisotropic;

extern cvar_t *gl3_debugcontext;

#endif /* SRC_CLIENT_REFRESH_GL3_HEADER_LOCAL_H_ */
