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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * =======================================================================
 *
 * ABI between client and refresher
 *
 * =======================================================================
 */

#ifndef CL_REF_H
#define CL_REF_H

#include "../../common/header/common.h"
#include "vid.h"

#define	MAX_DLIGHTS		32
#define	MAX_ENTITIES	128
#define	MAX_PARTICLES	4096
#define	MAX_LIGHTSTYLES	256

#define POWERSUIT_SCALE		4.0F

#define SHELL_RED_COLOR		0xF2
#define SHELL_GREEN_COLOR	0xD0
#define SHELL_BLUE_COLOR	0xF3

#define SHELL_RG_COLOR		0xDC
#define SHELL_RB_COLOR		0x68
#define SHELL_BG_COLOR		0x78

#define SHELL_DOUBLE_COLOR		0xDF
#define	SHELL_HALF_DAM_COLOR	0x90
#define SHELL_CYAN_COLOR		0x72

#define SHELL_WHITE_COLOR	0xD7

#define ENTITY_FLAGS	68
#define	API_VERSION		3

typedef struct entity_s {
	struct model_s		*model; /* opaque type outside refresh */
	float				angles[3];

	/* most recent data */
	float				origin[3]; /* also used as RF_BEAM's "from" */
	int					frame; /* also used as RF_BEAM's diameter */

	/* previous data for lerping */
	float				oldorigin[3]; /* also used as RF_BEAM's "to" */
	int					oldframe;

	/* misc */
	float	backlerp; /* 0.0 = current, 1.0 = old */
	int		skinnum; /* also used as RF_BEAM's palette index */

	int		lightstyle; /* for flashing entities */
	float	alpha; /* ignore if RF_TRANSLUCENT isn't set */

	struct image_s	*skin; /* NULL for inline skin */
	int		flags;
} entity_t;

typedef struct {
	vec3_t	origin;
	vec3_t	color;
	float	intensity;
} dlight_t;

typedef struct {
	vec3_t	origin;
	int		color;
	float	alpha;
} particle_t;

typedef struct {
	float		rgb[3]; /* 0.0 - 2.0 */
	float		white; /* highest of rgb */
} lightstyle_t;

typedef struct {
	int			x, y, width, height; /* in virtual screen coordinates */
	float		fov_x, fov_y;
	float		vieworg[3];
	float		viewangles[3];
	float		blend[4]; /* rgba 0-1 full screen blend */
	float		time; /* time is uesed to auto animate */
	int			rdflags; /* RDF_UNDERWATER, etc */

	byte		*areabits; /* if not NULL, only areas with set bits will be drawn */

	lightstyle_t	*lightstyles; /* [MAX_LIGHTSTYLES] */

	int			num_entities;
	entity_t	*entities;

	int			num_dlights;
	dlight_t	*dlights;

	int			num_particles;
	particle_t	*particles;
} refdef_t;

// Soon to be deleted
//void R_GetRefAPI(void);

/*
 * Refresh API
 */
void R_BeginRegistration(char *map);
void R_Clear(void);
struct model_s *R_RegisterModel(char *name);
struct image_s *R_RegisterSkin(char *name);
void R_SetSky(char *name, float rotate, vec3_t axis);
void R_EndRegistration(void);
struct image_s *Draw_FindPic(char *name);
void R_RenderFrame(refdef_t *fd);
void Draw_GetPicSize(int *w, int *h, char *name);
void Draw_Pic(int x, int y, char *name);
void Draw_StretchPic(int x, int y, int w, int h, char *name);
void Draw_Char(int x, int y, int c);
void Draw_TileClear(int x, int y, int w, int h, char *name);
void Draw_Fill(int x, int y, int w, int h, int c);
void Draw_FadeScreen(void);
void Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data);
int R_Init(void *hinstance, void *hWnd);
void R_Shutdown(void);
void R_SetPalette(const unsigned char *palette);
void R_BeginFrame(float camera_separation);
void GLimp_EndFrame(void);

#endif
