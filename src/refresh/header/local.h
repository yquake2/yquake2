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

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "../../client/header/ref.h"
#include "../../backends/generic/header/qgl.h"

#ifndef GL_COLOR_INDEX8_EXT
 #define GL_COLOR_INDEX8_EXT GL_COLOR_INDEX
#endif

#ifndef GL_EXT_texture_filter_anisotropic
 #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
 #define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#ifndef GL_ARB_multitexture
 #define GL_TEXTURE0_ARB 0x84C0
 #define GL_TEXTURE1_ARB 0x84C1
#endif

#ifndef GL_VERSION_1_3
 #define GL_TEXTURE1 0x84C1
#endif

#define TEXNUM_LIGHTMAPS 1024
#define TEXNUM_SCRAPS 1152
#define TEXNUM_IMAGES 1153
#define MAX_GLTEXTURES 1024
#define MAX_SCRAPS 1
#define BLOCK_WIDTH 128
#define BLOCK_HEIGHT 128
#define REF_VERSION "Yamagi Quake II OpenGL Refresher"
#define MAX_LBM_HEIGHT 480
#define BACKFACE_EPSILON 0.01
#define DYNAMIC_LIGHT_WIDTH 128
#define DYNAMIC_LIGHT_HEIGHT 128
#define LIGHTMAP_BYTES 4
#define MAX_LIGHTMAPS 128
#define GL_LIGHTMAP_FORMAT GL_RGBA

/* up / down */
#define PITCH 0

/* left / right */
#define YAW 1

/* fall over */
#define ROLL 2

char *strlwr(char *s);

extern viddef_t vid;

/*
 * skins will be outline flood filled and mip mapped
 * pics and sprites with alpha will be outline flood filled
 * pic won't be mip mapped
 *
 * model skin
 * sprite frame
 * wall texture
 * pic
 */
typedef enum
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky
} imagetype_t;

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

	rserr_invalid_fullscreen,
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
extern entity_t *currententity;
extern model_t *currentmodel;
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

extern cvar_t *gl_norefresh;
extern cvar_t *gl_lefthand;
extern cvar_t *gl_farsee;
extern cvar_t *gl_drawentities;
extern cvar_t *gl_drawworld;
extern cvar_t *gl_speeds;
extern cvar_t *gl_fullbright;
extern cvar_t *gl_novis;
extern cvar_t *gl_nocull;
extern cvar_t *gl_lerpmodels;

extern cvar_t *gl_lightlevel;
extern cvar_t *gl_overbrightbits;

extern cvar_t *gl_vertex_arrays;

extern cvar_t *gl_ext_swapinterval;
extern cvar_t *gl_ext_palettedtexture;
extern cvar_t *gl_ext_multitexture;
extern cvar_t *gl_ext_pointparameters;
extern cvar_t *gl_ext_compiled_vertex_array;
extern cvar_t *gl_ext_mtexcombine;

extern cvar_t *gl_particle_min_size;
extern cvar_t *gl_particle_max_size;
extern cvar_t *gl_particle_size;
extern cvar_t *gl_particle_att_a;
extern cvar_t *gl_particle_att_b;
extern cvar_t *gl_particle_att_c;

extern cvar_t *gl_nosubimage;
extern cvar_t *gl_bitdepth;
extern cvar_t *gl_mode;

extern cvar_t *gl_customwidth;
extern cvar_t *gl_customheight;

#ifdef RETEXTURE
extern cvar_t *gl_retexturing;
#endif

extern cvar_t *gl_log;
extern cvar_t *gl_lightmap;
extern cvar_t *gl_shadows;
extern cvar_t *gl_stencilshadow;
extern cvar_t *gl_dynamic;
extern cvar_t *gl_nobind;
extern cvar_t *gl_round_down;
extern cvar_t *gl_picmip;
extern cvar_t *gl_skymip;
extern cvar_t *gl_showtris;
extern cvar_t *gl_finish;
extern cvar_t *gl_ztrick;
extern cvar_t *gl_zfix;
extern cvar_t *gl_clear;
extern cvar_t *gl_cull;
extern cvar_t *gl_poly;
extern cvar_t *gl_texsort;
extern cvar_t *gl_polyblend;
extern cvar_t *gl_flashblend;
extern cvar_t *gl_lightmaptype;
extern cvar_t *gl_modulate;
extern cvar_t *gl_playermip;
extern cvar_t *gl_drawbuffer;
extern cvar_t *gl_3dlabs_broken;
extern cvar_t *gl_driver;
extern cvar_t *gl_swapinterval;
extern cvar_t *gl_anisotropic;
extern cvar_t *gl_anisotropic_avail;
extern cvar_t *gl_texturemode;
extern cvar_t *gl_texturealphamode;
extern cvar_t *gl_texturesolidmode;
extern cvar_t *gl_saturatelighting;
extern cvar_t *gl_lockpvs;

extern cvar_t *vid_fullscreen;
extern cvar_t *vid_gamma;

extern cvar_t *intensity;

extern int gl_lightmap_format;
extern int gl_solid_format;
extern int gl_alpha_format;
extern int gl_tex_solid_format;
extern int gl_tex_alpha_format;

extern int c_visible_lightmaps;
extern int c_visible_textures;

extern float r_world_matrix[16];

void R_TranslatePlayerSkin(int playernum);
void R_Bind(int texnum);
void R_MBind(GLenum target, int texnum);
void R_TexEnv(GLenum value);
void R_EnableMultitexture(qboolean enable);
void R_SelectTexture(GLenum);

void R_LightPoint(vec3_t p, vec3_t color);
void R_PushDlights(void);

extern model_t *r_worldmodel;
extern unsigned d_8to24table[256];
extern int registration_sequence;

void V_AddBlend(float r, float g, float b, float a, float *v_blend);

void R_RenderView(refdef_t *fd);
void R_ScreenShot(void);
void R_DrawAliasModel(entity_t *e);
void R_DrawBrushModel(entity_t *e);
void R_DrawSpriteModel(entity_t *e);
void R_DrawBeam(entity_t *e);
void R_DrawWorld(void);
void R_RenderDlights(void);
void R_DrawAlphaSurfaces(void);
void R_RenderBrushPoly(msurface_t *fa);
void R_InitParticleTexture(void);
void Draw_InitLocal(void);
void R_SubdivideSurface(msurface_t *fa);
qboolean R_CullBox(vec3_t mins, vec3_t maxs);
void R_RotateForEntity(entity_t *e);
void R_MarkLeaves(void);

glpoly_t *WaterWarpPolyVerts(glpoly_t *p);
void R_EmitWaterPolys(msurface_t *fa);
void R_AddSkySurface(msurface_t *fa);
void R_ClearSkyBox(void);
void R_DrawSkyBox(void);
void R_MarkLights(dlight_t *light, int bit, mnode_t *node);

void COM_StripExtension(char *in, char *out);

void R_SwapBuffers(int);

int Draw_GetPalette(void);

void R_ResampleTexture(unsigned *in, int inwidth, int inheight,
		unsigned *out, int outwidth, int outheight);

void LoadPCX(char *filename, byte **pic, byte **palette,
		int *width, int *height);
image_t *LoadWal(char *name);
void LoadJPG(char *origname, byte **pic, int *width, int *height);
void LoadTGA(char *origname, byte **pic, int *width, int *height);
void GetWalInfo(char *name, int *width, int *height);
void GetPCXInfo(char *filename, int *width, int *height);
image_t *R_LoadPic(char *name, byte *pic, int width, int realwidth,
		int height, int realheight, imagetype_t type, int bits);
image_t *R_FindImage(char *name, imagetype_t type);
void R_TextureMode(char *string);
void R_ImageList_f(void);

void R_SetTexturePalette(unsigned palette[256]);

void R_InitImages(void);
void R_ShutdownImages(void);

void R_FreeUnusedImages(void);

void R_TextureAlphaMode(char *string);
void R_TextureSolidMode(char *string);
int Scrap_AllocBlock(int w, int h, int *x, int *y);

/* GL extension emulation functions */
void R_DrawParticles2(int n,
		const particle_t particles[],
		const unsigned colortable[768]);

/*
 * GL config stuff
 */

typedef struct
{
	int renderer;
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extensions_string;

	qboolean allow_cds;
	qboolean mtexcombine;

	qboolean anisotropic;
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

	float camera_separation;
	qboolean stereo_enabled;

	qboolean hwgamma;

	unsigned char originalRedGammaTable[256];
	unsigned char originalGreenGammaTable[256];
	unsigned char originalBlueGammaTable[256];
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

void GLimp_BeginFrame(float camera_separation);
int GLimp_Init(void);
void GLimp_Shutdown(void);
int GLimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen);
void GLimp_AppActivate(qboolean active);
void GLimp_EnableLogging(qboolean enable);
void GLimp_LogNewFrame(void);

#endif
