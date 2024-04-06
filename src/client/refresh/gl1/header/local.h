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


#ifndef GL_COLOR_INDEX8_EXT
 #define GL_COLOR_INDEX8_EXT GL_COLOR_INDEX
#endif

#ifndef GL_EXT_texture_filter_anisotropic
 #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
 #define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#ifndef GL_VERSION_1_3
 #define GL_TEXTURE0 0x84C0
 #define GL_TEXTURE1 0x84C1
#endif

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif

#ifndef GL_MULTISAMPLE_FILTER_HINT_NV
#define GL_MULTISAMPLE_FILTER_HINT_NV 0x8534
#endif

#define TEXNUM_LIGHTMAPS 1024
#define TEXNUM_SCRAPS 1152
#define TEXNUM_IMAGES 1153
#define MAX_GLTEXTURES 1024
#define MAX_SCRAPS 1
#define BLOCK_WIDTH 128
#define BLOCK_HEIGHT 128
#define REF_VERSION "Yamagi Quake II OpenGL Refresher"
#define BACKFACE_EPSILON 0.01
#define LIGHTMAP_BYTES 4
#define MAX_LIGHTMAPS 128
#define GL_LIGHTMAP_FORMAT GL_RGBA

/* up / down */
#define PITCH 0

/* left / right */
#define YAW 1

/* fall over */
#define ROLL 2

extern viddef_t vid;


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

#include "model.h"

void GL_BeginRendering(int *x, int *y, int *width, int *height);
void GL_EndRendering(void);

void R_SetDefaultState(void);

extern float gldepthmin, gldepthmax;

typedef struct
{
	float x, y, z;
	float s, t;
	float r, g, b;
} glvert_t;

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
extern refdef_t r_newrefdef;
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

extern cvar_t *intensity;

extern int gl_solid_format;
extern int gl_alpha_format;
extern int gl_tex_solid_format;
extern int gl_tex_alpha_format;

extern int c_visible_lightmaps;
extern int c_visible_textures;

extern float r_world_matrix[16];

void R_TranslatePlayerSkin(int playernum);
void R_Bind(int texnum);

void R_TexEnv(GLenum value);

void R_LightPoint(entity_t *currententity, vec3_t p, vec3_t color);
void R_PushDlights(void);

extern model_t *r_worldmodel;
extern unsigned d_8to24table[256];
extern int registration_sequence;

void V_AddBlend(float r, float g, float b, float a, float *v_blend);

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
glpoly_t *WaterWarpPolyVerts(glpoly_t *p);
void R_EmitWaterPolys(msurface_t *fa);
void R_AddSkySurface(msurface_t *fa);
void R_ClearSkyBox(void);
void R_DrawSkyBox(void);
void R_MarkSurfaceLights(dlight_t *light, int bit, mnode_t *node,
	int lightframecount);

void COM_StripExtension(char *in, char *out);

void R_SwapBuffers(int);

image_t *R_LoadPic(const char *name, byte *pic, int width, int realwidth,
		int height, int realheight, size_t data_size, imagetype_t type, int bits);
image_t *R_FindImage(const char *name, imagetype_t type);
void R_TextureMode(char *string);
void R_ImageList_f(void);

void R_SetTexturePalette(unsigned palette[256]);

void R_InitImages(void);
void R_ShutdownImages(void);

void R_FreeUnusedImages(void);
qboolean R_ImageHasFreeSpace(void);

void R_TextureAlphaMode(char *string);
void R_TextureSolidMode(char *string);
int Scrap_AllocBlock(int w, int h, int *x, int *y);

#ifdef DEBUG
void glCheckError_(const char *file, const char *function, int line);
// Ideally, the following list should contain all OpenGL calls.
// Either way, errors are caught, since error flags are persisted until the next glGetError() call.
// So they show, even if the location of the error is inaccurate.
#define glDrawArrays(...) glDrawArrays(__VA_ARGS__); glCheckError_(__FILE__, __func__, __LINE__)
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

	// ----

	float max_anisotropy;
} glconfig_t;

typedef struct
{
	float inverse_intensity;
	qboolean fullscreen;

	int prev_mode;

	unsigned char *d_16to8table;

	int lightmap_textures;

	int currenttextures[2];
	int currenttmu;
	GLenum currenttarget;

	float camera_separation;
	enum stereo_modes stereo_mode;

	qboolean stencil;
} glstate_t;

typedef struct
{
	int internal_format;
	int current_lightmap_texture;

	msurface_t *lightmap_surfaces[MAX_LIGHTMAPS];

	int allocated[BLOCK_WIDTH];

	/* the lightmap texture data needs to be kept in
	   main memory so texsubimage can update properly */
	byte lightmap_buffer[4 * BLOCK_WIDTH * BLOCK_HEIGHT];
} gllightmapstate_t;

extern glconfig_t gl_config;
extern glstate_t gl_state;

/*
 * Updates the gamma ramp.
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
extern image_t * RDraw_FindPic(char *name);
extern void RDraw_GetPicSize(int *w, int *h, char *pic);
extern void RDraw_PicScaled(int x, int y, char *pic, float factor);
extern void RDraw_StretchPic(int x, int y, int w, int h, char *pic);
extern void RDraw_CharScaled(int x, int y, int num, float scale);
extern void RDraw_TileClear(int x, int y, int w, int h, char *pic);
extern void RDraw_Fill(int x, int y, int w, int h, int c);
extern void RDraw_FadeScreen(void);
extern void RDraw_StretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int bits);

#endif
