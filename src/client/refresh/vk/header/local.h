/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2018-2019 Krzysztof Kondrak

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef __VK_LOCAL_H__
#define __VK_LOCAL_H__

#include <math.h>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>


#include "../../ref_shared.h"
#include "../volk/volk.h"
#include "qvk.h"

// verify if VkResult is VK_SUCCESS
#define VK_VERIFY(x) { \
	VkResult res = (x); \
	if(res != VK_SUCCESS) { \
		R_Printf(PRINT_ALL, "%s:%d: VkResult verification failed: %s\n", \
			 __func__, __LINE__, QVk_GetError(res)); \
	} \
}

// up / down
#define	PITCH	0

// left / right
#define	YAW	1

// fall over
#define	ROLL	2

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

extern	viddef_t	vid;

typedef struct image_s
{
	char	name[MAX_QPATH];			// game path, including extension
	imagetype_t	type;
	int		width, height;				// source image
	int		upload_width, upload_height;	// after power of two and picmip
	int		registration_sequence;		// 0 = free
	struct msurface_s	*texturechain;	// for sort-by-texture world drawing
	qvktexture_t vk_texture;			// Vulkan texture handle
} image_t;

#define		MAX_VKTEXTURES	1024

//===================================================================

typedef enum
{
	rserr_ok,

	rserr_invalid_fullscreen,
	rserr_invalid_mode,

	rserr_unknown
} rserr_t;

#include "model.h"

#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01


//====================================================

extern	image_t		vktextures[MAX_VKTEXTURES];
extern	int			numvktextures;

extern	image_t		*r_notexture;
extern	image_t		*r_particletexture;
extern	image_t		*r_squaretexture;
extern	int			r_visframecount;
extern	int			r_framecount;
extern	cplane_t	frustum[4];
extern	int			c_brush_polys, c_alias_polys;

//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_newrefdef;
extern	int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern	cvar_t	*r_norefresh;
extern	cvar_t	*r_lefthand;
extern	cvar_t	*r_drawentities;
extern	cvar_t	*r_drawworld;
extern	cvar_t	*r_speeds;
extern	cvar_t	*r_fullbright;
extern	cvar_t	*r_novis;
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_lerpmodels;
extern	cvar_t	*r_mode;
extern	cvar_t	*r_lockpvs;
extern	cvar_t	*r_modulate;
extern	cvar_t	*r_vsync;
extern	cvar_t	*r_clear;
extern	cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level
extern	cvar_t	*r_gunfov;
extern	cvar_t	*r_farsee;

extern	cvar_t	*vk_overbrightbits;
extern	cvar_t	*vk_validation;
extern	cvar_t	*vk_bitdepth;
extern	cvar_t	*vk_picmip;
extern	cvar_t	*vk_skymip;
extern	cvar_t	*vk_flashblend;
extern	cvar_t	*vk_finish;
extern	cvar_t	*vk_polyblend;
extern	cvar_t	*vk_shadows;
extern	cvar_t	*vk_particle_size;
extern	cvar_t	*vk_particle_att_a;
extern	cvar_t	*vk_particle_att_b;
extern	cvar_t	*vk_particle_att_c;
extern	cvar_t	*vk_particle_min_size;
extern	cvar_t	*vk_particle_max_size;
extern	cvar_t	*vk_custom_particles;
extern	cvar_t	*vk_dynamic;
extern	cvar_t	*vk_msaa;
extern	cvar_t	*vk_showtris;
extern	cvar_t	*vk_lightmap;
extern	cvar_t	*vk_texturemode;
extern	cvar_t	*vk_lmaptexturemode;
extern	cvar_t	*vk_aniso;
extern	cvar_t	*vk_sampleshading;
extern	cvar_t	*vk_device_idx;
extern	cvar_t	*vk_retexturing;
extern	cvar_t	*vk_nolerp_list;

extern	cvar_t	*vid_fullscreen;
extern	cvar_t	*vid_gamma;

extern	int		c_visible_lightmaps;
extern	int		c_visible_textures;

extern	float	r_viewproj_matrix[16];

void R_LightPoint (vec3_t p, vec3_t color, entity_t *currententity);
void R_PushDlights (void);

//====================================================================

extern	model_t	*r_worldmodel;

extern	unsigned	d_8to24table[256];

extern	int		registration_sequence;
extern	qvksampler_t vk_current_sampler;
extern	qvksampler_t vk_current_lmap_sampler;

void	 RE_Shutdown( void );

void Vk_ScreenShot_f (void);
void R_DrawAliasModel (entity_t *currententity, model_t *currentmodel);
void R_DrawBrushModel (entity_t *currententity, model_t *currentmodel);
void R_DrawSpriteModel (entity_t *currententity, model_t *currentmodel);
void R_DrawBeam (entity_t *currententity);
void R_DrawWorld (void);
void R_RenderDlights (void);
void R_DrawAlphaSurfaces (void);
void RE_InitParticleTexture (void);
void Draw_InitLocal (void);
void Vk_SubdivideSurface (msurface_t *fa);
qboolean R_CullBox (vec3_t mins, vec3_t maxs);
void R_RotateForEntity (entity_t *e, float *mvMatrix);
void R_MarkLeaves (void);

void EmitWaterPolys (msurface_t *fa, image_t *texture, float *modelMatrix, float *color);
void R_AddSkySurface (msurface_t *fa);
void R_ClearSkyBox (void);
void R_DrawSkyBox (void);
void R_MarkLights (dlight_t *light, int bit, mnode_t *node);

struct image_s	*RE_Draw_FindPic (char *name);

void	RE_Draw_GetPicSize (int *w, int *h, char *name);
void	RE_Draw_PicScaled (int x, int y, char *name, float scale);
void	RE_Draw_StretchPic (int x, int y, int w, int h, char *name);
void	RE_Draw_CharScaled (int x, int y, int num, float scale);
void	RE_Draw_TileClear (int x, int y, int w, int h, char *name);
void	RE_Draw_Fill (int x, int y, int w, int h, int c);
void	RE_Draw_FadeScreen (void);
void	RE_Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data);

qboolean	RE_EndWorldRenderpass( void );

struct image_s *RE_RegisterSkin (char *name);

void LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height);
image_t *Vk_LoadPic(char *name, byte *pic, int width, int realwidth,
		    int height, int realheight, imagetype_t type,
		    int bits);
image_t	*Vk_FindImage (char *name, imagetype_t type);
void	Vk_TextureMode( char *string );
void	Vk_LmapTextureMode( char *string );
void	Vk_ImageList_f (void);

void	Vk_InitImages (void);
void	Vk_ShutdownImages (void);
void	Vk_FreeUnusedImages (void);

void	RE_BeginRegistration (char *model);
struct model_s	*RE_RegisterModel (char *name);
struct image_s	*RE_RegisterSkin (char *name);
void	RE_SetSky (char *name, float rotate, vec3_t axis);
void	RE_EndRegistration (void);

void Mat_Identity(float *matrix);
void Mat_Mul(float *m1, float *m2, float *res);
void Mat_Translate(float *matrix, float x, float y, float z);
void Mat_Rotate(float *matrix, float deg, float x, float y, float z);
void Mat_Scale(float *matrix, float x, float y, float z);
void Mat_Perspective(float *matrix, float *correction_matrix, float fovy, float aspect, float zNear, float zFar);
void Mat_Ortho(float *matrix, float left, float right, float bottom, float top, float zNear, float zFar);

typedef struct
{
	uint32_t    vk_version;
	const char *vendor_name;
	const char *device_type;
	const char *present_mode;
	const char *supported_present_modes[256];
	const char *extensions[256];
	const char *layers[256];
	uint32_t    vertex_buffer_usage;
	uint32_t    vertex_buffer_max_usage;
	uint32_t    vertex_buffer_size;
	uint32_t    index_buffer_usage;
	uint32_t    index_buffer_max_usage;
	uint32_t    index_buffer_size;
	uint32_t    uniform_buffer_usage;
	uint32_t    uniform_buffer_max_usage;
	uint32_t    uniform_buffer_size;
	uint32_t    triangle_fan_index_usage;
	uint32_t    triangle_fan_index_max_usage;
	uint32_t    triangle_fan_index_count;
} vkconfig_t;

#define MAX_LIGHTMAPS 128
#define DYNLIGHTMAP_OFFSET MAX_LIGHTMAPS

typedef struct
{
	float inverse_intensity;
	qboolean fullscreen;

	int     prev_mode;

	unsigned char *d_16to8table;

	qvktexture_t lightmap_textures[MAX_LIGHTMAPS*2];

	int	currenttextures[2];
	int currenttmu;

	float camera_separation;
	qboolean stereo_enabled;

	VkPipeline current_pipeline;
} vkstate_t;

extern vkconfig_t  vk_config;
extern vkstate_t   vk_state;

/*
====================================================================

IMPORTED FUNCTIONS

====================================================================
*/

extern	refimport_t	ri;


/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

qboolean Vkimp_CreateSurface(SDL_Window *window);

#endif
