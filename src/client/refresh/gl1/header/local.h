/*
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
 * Local header for the refresher.
 *
 * =======================================================================
 */

#ifndef REF_LOCAL_H
#define REF_LOCAL_H

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "../../ref_shared.h"
#include "qgl.h"

#ifdef YQ2_GL1_GLES
#define REF_VERSION "Yamagi Quake II OpenGL ES1 Refresher"
#define GL_COLOR_INDEX	GL_RGBA
#define GL_COLOR_INDEX8_EXT	GL_RGBA
#else
#define REF_VERSION "Yamagi Quake II OpenGL Refresher"
#ifndef GL_COLOR_INDEX8_EXT
#define GL_COLOR_INDEX8_EXT	GL_COLOR_INDEX
#endif
#endif

#define MAX_LIGHTMAPS 128
#define MAX_LIGHTMAP_COPIES 3	// Meant for tile / deferred rendering platforms
#define MAX_SCRAPS 1
#define TEXNUM_LIGHTMAPS 1024
#define TEXNUM_SCRAPS (TEXNUM_LIGHTMAPS + MAX_LIGHTMAPS * MAX_LIGHTMAP_COPIES)
#define TEXNUM_IMAGES (TEXNUM_SCRAPS + MAX_SCRAPS)
#define MAX_GLTEXTURES 1024
#define BLOCK_WIDTH 128
#define BLOCK_HEIGHT 128
#define SCRAP_WIDTH (BLOCK_WIDTH * 2)
#define SCRAP_HEIGHT (BLOCK_HEIGHT * 2)
#define BACKFACE_EPSILON 0.01
#define LIGHTMAP_BYTES 4
#define MAX_TEXTURE_UNITS 2
#define GL_LIGHTMAP_FORMAT GL_RGBA

// GL buffer definitions
#define MAX_VERTICES	16384
#define MAX_INDICES 	(MAX_VERTICES * 4)

/* up / down */
#define PITCH 0

/* left / right */
#define YAW 1

/* fall over */
#define ROLL 2

#if defined(USE_SDL3) || defined(YQ2_GL1_GLES)
// Use internal lookup table instead of SDL2 hw gamma funcs for GL1/GLES1
#define GL1_GAMMATABLE
#endif

enum stereo_modes {
	STEREO_MODE_NONE,
	STEREO_MODE_OPENGL,
	STEREO_MODE_ANAGLYPH,
	STEREO_MODE_ROW_INTERLEAVED,
	STEREO_MODE_COLUMN_INTERLEAVED,
	STEREO_MODE_PIXEL_INTERLEAVED,
	STEREO_SPLIT_HORIZONTAL,
	STEREO_SPLIT_VERTICAL,
};

enum opengl_special_buffer_modes {
	OPENGL_SPECIAL_BUFFER_MODE_NONE,
	OPENGL_SPECIAL_BUFFER_MODE_STEREO,
	OPENGL_SPECIAL_BUFFER_MODE_STENCIL,
};

typedef struct image_s
{
	char name[MAX_QPATH];               /* game path, including extension */
	imagetype_t type;
	int width, height;                  /* source image */
	int upload_width, upload_height;    /* after power of two and picmip */
	int registration_sequence;          /* 0 = free */
	struct msurface_s *texturechain;    /* for sort-by-texture world drawing */
	int texnum;                         /* gl texture binding */
	float sl, tl, sh, th;               /* 0,0 - 1,1 unless part of the scrap */
	qboolean scrap;
	qboolean has_alpha;

	qboolean paletted;
} image_t;

typedef enum
{
	rserr_ok,

	rserr_invalid_mode,

	rserr_unknown
} rserr_t;

typedef enum
{
	buf_2d,
	buf_singletex,
	buf_mtex,
	buf_alpha,
	buf_alias,
	buf_flash,
	buf_shadow
} buffered_draw_t;

typedef struct	//	832k aprox.
{
	buffered_draw_t	type;

	GLfloat
		vtx[MAX_VERTICES * 3],	// vertexes
		tex[MAX_TEXTURE_UNITS][MAX_VERTICES * 2];	// texture coords

	GLubyte	clr[MAX_VERTICES * 4];	// color components

	GLushort idx[MAX_INDICES];	// indices for the draw call

	GLuint vt, tx, cl;	// indices for GLfloat arrays above

	int	texture[MAX_TEXTURE_UNITS];
	int	flags;	// entity flags
	float	alpha;
} glbuffer_t;

#include "model.h"

extern glbuffer_t gl_buf;
extern float gldepthmin, gldepthmax;

extern image_t gltextures[MAX_GLTEXTURES];
extern int numgltextures;

extern image_t *r_notexture;
extern image_t *r_particletexture;
extern int r_visframecount;
extern int r_framecount;
extern cplane_t frustum[4];
extern int c_brush_polys, c_alias_polys;
extern int gl_filter_min, gl_filter_max;

/* view origin */
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t r_origin;

/* screen size info */
extern int r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern qboolean IsHighDPIaware;

extern cvar_t *r_norefresh;
extern cvar_t *gl_lefthand;
extern cvar_t *r_gunfov;
extern cvar_t *r_farsee;
extern cvar_t *r_drawentities;
extern cvar_t *r_drawworld;
extern cvar_t *r_speeds;
extern cvar_t *r_fullbright;
extern cvar_t *r_novis;
extern cvar_t *r_lerpmodels;
extern cvar_t *r_fixsurfsky;

extern cvar_t *r_lightlevel;
extern cvar_t *gl1_overbrightbits;

extern cvar_t *gl1_palettedtexture;
extern cvar_t *gl1_pointparameters;
extern cvar_t *gl1_multitexture;

extern cvar_t *gl1_particle_min_size;
extern cvar_t *gl1_particle_max_size;
extern cvar_t *gl1_particle_size;
extern cvar_t *gl1_particle_att_a;
extern cvar_t *gl1_particle_att_b;
extern cvar_t *gl1_particle_att_c;
extern cvar_t *gl1_particle_square;

extern cvar_t *r_mode;
extern cvar_t *r_customwidth;
extern cvar_t *r_customheight;

extern cvar_t *r_retexturing;
extern cvar_t *r_scale8bittextures;
extern cvar_t *r_validation;

extern cvar_t *gl_nolerp_list;
extern cvar_t *r_lerp_list;
extern cvar_t *r_2D_unfiltered;
extern cvar_t *r_videos_unfiltered;

extern cvar_t *gl_lightmap;
extern cvar_t *gl_shadows;
extern cvar_t *gl1_stencilshadow;
extern cvar_t *gl1_dynamic;
extern cvar_t *gl_nobind;
extern cvar_t *gl1_round_down;
extern cvar_t *gl1_picmip;
extern cvar_t *gl_showtris;
extern cvar_t *gl_showbbox;
extern cvar_t *gl_finish;
extern cvar_t *gl1_ztrick;
extern cvar_t *gl_zfix;
extern cvar_t *r_clear;
extern cvar_t *r_cull;
extern cvar_t *gl1_polyblend;
extern cvar_t *gl1_flashblend;
extern cvar_t *r_modulate;
extern cvar_t *gl_drawbuffer;
extern cvar_t *r_vsync;
extern cvar_t *gl_anisotropic;
extern cvar_t *gl_texturemode;
extern cvar_t *gl1_texturealphamode;
extern cvar_t *gl1_texturesolidmode;
extern cvar_t *gl1_saturatelighting;
extern cvar_t *r_lockpvs;
extern cvar_t *gl_msaa_samples;

extern cvar_t *vid_fullscreen;
extern cvar_t *vid_gamma;

extern int gl_tex_solid_format;
extern int gl_tex_alpha_format;

extern int c_visible_lightmaps;
extern int c_visible_textures;

extern float r_world_matrix[16];

extern unsigned char gammatable[256];

qboolean R_Bind(int texnum);

void R_TexEnv(GLenum value);
void R_SelectTexture(GLenum);
void R_MBind(GLenum target, int texnum);
void R_EnableMultitexture(qboolean enable);

void R_LightPoint(entity_t *currententity, vec3_t p, vec3_t color);
void R_PushDlights(void);

extern model_t *r_worldmodel;
extern unsigned d_8to24table[256];
extern int registration_sequence;

void R_ScreenShot(void);
void R_DrawAliasModel(entity_t *currententity, const model_t *currentmodel);
void R_DrawBrushModel(entity_t *currententity, const model_t *currentmodel);
void R_DrawSpriteModel(entity_t *currententity, const model_t *currentmodel);
void R_DrawBeam(entity_t *e);
void R_DrawWorld(void);
void R_RenderDlights(void);
void R_DrawAlphaSurfaces(void);
void R_InitParticleTexture(void);
void Draw_InitLocal(void);
void R_SubdivideSurface(model_t *loadmodel, msurface_t *fa);
void R_RotateForEntity(entity_t *e);
void R_MarkLeaves(void);

extern int r_dlightframecount;
void R_EmitWaterPolys(msurface_t *fa);
void R_AddSkySurface(msurface_t *fa);
void R_ClearSkyBox(void);
void R_DrawSkyBox(void);
void R_MarkSurfaceLights(dlight_t *light, int bit, mnode_t *node,
	int lightframecount);

image_t *R_LoadPic(const char *name, byte *pic, int width, int realwidth,
		int height, int realheight, size_t data_size, imagetype_t type, int bits);
image_t *R_FindImage(const char *name, imagetype_t type);
void R_TextureMode(const char *string);
void R_ImageList_f(void);

void R_SetTexturePalette(const unsigned palette[256]);

void R_InitImages(void);
void R_ShutdownImages(void);

void R_FreeUnusedImages(void);
qboolean R_ImageHasFreeSpace(void);

void R_TextureAlphaMode(const char *string);
void R_TextureSolidMode(const char *string);
int Scrap_AllocBlock(int w, int h, int *x, int *y);

// GL buffer operations

#define GLBUFFER_VERTEX(X, Y, Z) \
	gl_buf.vtx[gl_buf.vt] = X; gl_buf.vtx[gl_buf.vt+1] = Y; \
	gl_buf.vtx[gl_buf.vt+2] = Z; gl_buf.vt += 3;

#define GLBUFFER_SINGLETEX(S, T) \
	gl_buf.tex[0][gl_buf.tx] = S; gl_buf.tex[0][gl_buf.tx+1] = T; gl_buf.tx += 2;

#define GLBUFFER_MULTITEX(CS, CT, LS, LT) \
	gl_buf.tex[0][gl_buf.tx] = CS; gl_buf.tex[0][gl_buf.tx+1] = CT; \
	gl_buf.tex[1][gl_buf.tx] = LS; gl_buf.tex[1][gl_buf.tx+1] = LT; gl_buf.tx += 2;

#define GLBUFFER_COLOR(R, G, B, A) \
	gl_buf.clr[gl_buf.cl] = gammatable[R]; gl_buf.clr[gl_buf.cl+1] = gammatable[G]; \
	gl_buf.clr[gl_buf.cl+2] = gammatable[B]; gl_buf.clr[gl_buf.cl+3] = A; gl_buf.cl += 4;

void R_ApplyGLBuffer(void);
void R_UpdateGLBuffer(buffered_draw_t type, int colortex, int lighttex, int flags, float alpha);
void R_Buffer2DQuad(GLfloat ul_vx, GLfloat ul_vy, GLfloat dr_vx, GLfloat dr_vy,
	GLfloat ul_tx, GLfloat ul_ty, GLfloat dr_tx, GLfloat dr_ty);
void R_SetBufferIndices(GLenum primitive, GLuint vertices_num);

#ifdef YQ2_GL1_GLES
#define glPolygonMode(...)
#define glFrustum(...) glFrustumf(__VA_ARGS__)
#define glDepthRange(...) glDepthRangef(__VA_ARGS__)
#define glOrtho(...) glOrthof(__VA_ARGS__)
#else
#ifdef DEBUG
void glCheckError_(const char *file, const char *function, int line);
// Ideally, the following list should contain all OpenGL calls.
// Either way, errors are caught, since error flags are persisted until the next glGetError() call.
// So they show, even if the location of the error is inaccurate.
#define glDrawArrays(...) glDrawArrays(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glDrawElements(...) glDrawElements(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glTexImage2D(...) glTexImage2D(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glTexSubImage2D(...) glTexSubImage2D(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glTexEnvf(...) glTexEnvf(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glTexEnvi(...) glTexEnvi(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glVertexPointer(...) glVertexPointer(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glTexCoordPointer(...) glTexCoordPointer(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glColorPointer(...) glColorPointer(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glTexParameteri(...) glTexParameteri(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glBindTexture(...) glBindTexture(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glFrustum(...) glFrustum(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glTranslatef(...) glTranslatef(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glRotatef(...) glRotatef(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glScalef(...) glScalef(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glScissor(...) glScissor(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glBlendFunc(...) glBlendFunc(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glDepthFunc(...) glDepthFunc(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glDepthMask(...) glDepthMask(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glDepthRange(...) glDepthRange(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glEnable(...) glEnable(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glDisable(...) glDisable(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glEnableClientState(...) glEnableClientState(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glDisableClientState(...) glDisableClientState(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glPushMatrix(...) glPushMatrix(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glPopMatrix(...) glPopMatrix(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glMatrixMode(...) glMatrixMode(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glOrtho(...) glOrtho(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glColorMask(...) glColorMask(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glStencilOp(...) glStencilOp(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glStencilFunc(...) glStencilFunc(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glDrawBuffer(...) glDrawBuffer(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glReadPixels(...) glReadPixels(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glClear(...) glClear(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glClearColor(...) glClearColor(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glClearStencil(...) glClearStencil(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glDeleteTextures(...) glDeleteTextures(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glFinish() glFinish(); glCheckError_(__FILE__, __func__, __LINE__)
#define glAlphaFunc(...) glAlphaFunc(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glHint(...) glHint(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glLoadIdentity() glLoadIdentity(); glCheckError_(__FILE__, __func__, __LINE__)
#define glBegin(...) glBegin(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
#define glEnd() glEnd(); glCheckError_(__FILE__, __func__, __LINE__)
#endif
#endif

/* GL extension emulation functions */
void R_DrawParticles2(int n,
		const particle_t particles[],
		const unsigned *colortable);

/*
 * GL config stuff
 */

typedef struct
{
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extensions_string;

	int major_version;
	int minor_version;

	// ----

	qboolean anisotropic;
	qboolean npottextures;
	qboolean palettedtexture;
	qboolean pointparameters;
	qboolean multitexture;
	qboolean lightmapcopies;	// many copies of same lightmap, for embedded

	// ----

	float max_anisotropy;
} glconfig_t;

typedef struct
{
	float inverse_intensity;
	float sw_gamma;	// always 1 if using SDL2 hw gamma
	qboolean minlight_set;	// is gl1_minlight > 0 ?
	qboolean fullscreen;

	int prev_mode;

	unsigned char *d_16to8table;

	int lightmap_textures;

	int currenttextures[MAX_TEXTURE_UNITS];
	int currenttmu;
	GLenum currenttarget;

	float camera_separation;
	enum stereo_modes stereo_mode;

	qboolean stencil;
} glstate_t;

typedef struct
{
	int current_lightmap_texture;

	msurface_t *lightmap_surfaces[MAX_LIGHTMAPS];

	int allocated[BLOCK_WIDTH];

	/* the lightmap texture data needs to be kept in
	   main memory so texsubimage can update properly */
	byte *lightmap_buffer[MAX_LIGHTMAPS];
} gllightmapstate_t;

extern glconfig_t gl_config;
extern glstate_t gl_state;

/*
 * Updates the gamma ramp (SDL2 only).
 */
void RI_UpdateGamma(void);

/*
 * Enables or disables the vsync.
 */
void RI_SetVsync(void);

/*
 * Shuts the GL context down.
 */
void RI_ShutdownContext(void);

/*
 * Returns the address of the GL function proc,
 * or NULL if the function is not found.
 */
void *RI_GetProcAddress (const char* proc);

/*
 * Fills the actual size of the drawable into width and height.
 */
void RI_GetDrawableSize(int* width, int* height);

/*
 * Returns the SDL major version. Implemented
 * here to not polute gl1_main.c with the SDL
 * headers.
 */
int RI_GetSDLVersion();

/* g11_draw */
extern image_t * RDraw_FindPic(const char *name);
extern void RDraw_GetPicSize(int *w, int *h, const char *pic);
extern void RDraw_PicScaled(int x, int y, const char *pic, float factor);
extern void RDraw_StretchPic(int x, int y, int w, int h, const char *pic);
extern void RDraw_CharScaled(int x, int y, int num, float scale);
extern void RDraw_TileClear(int x, int y, int w, int h, const char *pic);
extern void RDraw_Fill(int x, int y, int w, int h, int c);
extern void RDraw_FadeScreen(void);
extern void RDraw_StretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int bits);

#endif
