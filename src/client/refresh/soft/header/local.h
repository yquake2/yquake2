/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#ifndef __R_LOCAL__
#define __R_LOCAL__

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "../../ref_shared.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define REF_VERSION	"SOFT 0.01"

// up / down
#define PITCH	0

// left / right
#define YAW	1

// fall over
#define ROLL	2


/*

  skins will be outline flood filled and mip mapped
  pics and sprites with alpha will be outline flood filled
  pic won't be mip mapped

  model skin
  sprite frame
  wall texture
  pic

*/

#define NUM_MIPS	4

typedef struct image_s
{
	char		name[MAX_QPATH];	// game path, including extension
	imagetype_t	type;
	int		width, height;
	qboolean	transparent;		// true if any 255 pixels in image
	int		registration_sequence;  // 0 = free
	byte		*pixels[NUM_MIPS];	// mip levels
} image_t;


//===================================================================

typedef unsigned char pixel_t;
typedef int	shift20_t;
typedef int	zvalue_t;
typedef unsigned int	light_t;

// xyz-prescale to 16.16 fixed-point
#define SHIFT16XYZ 16
#define SHIFT16XYZ_MULT (1 << SHIFT16XYZ)

typedef enum
{
	rserr_ok,

	rserr_invalid_fullscreen,
	rserr_invalid_mode,

	rserr_unknown
} rserr_t;

extern viddef_t	vid;
extern pixel_t	*vid_buffer;	// invisible buffer
extern pixel_t	*vid_colormap;	// 256 * VID_GRADES size
extern pixel_t	*vid_alphamap;	// 256 * 256 translucency map
extern char	shift_size;	// shift size in fixed-point

typedef struct
{
	vrect_t		vrect;	// subwindow in video for refresh
				// FIXME: not need vrect next field here?
	vrect_t		aliasvrect; // scaled Alias version
	shift20_t	vrectright, vrectbottom; // right & bottom screen coords
	shift20_t	aliasvrectright, aliasvrectbottom; // scaled Alias versions
	float		vrectrightedge;	// rightmost right edge we care about,
					//  for use in edge list
	float		fvrectx, fvrecty; // for floating-point compares
	float		fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
	shift20_t	vrect_x_adj_shift20; // (vrect.x + 0.5 - epsilon) << 20
	shift20_t	vrectright_adj_shift20; // (vrectright + 0.5 - epsilon) << 20
	float		fvrectright_adj, fvrectbottom_adj; // right and bottom edges, for clamping
	float		fvrectright; // rightmost edge, for Alias clamping
	float		fvrectbottom; // bottommost edge, for Alias clamping
	float		horizontalFieldOfView;	// at Z = 1.0, this many X is visible
						// 2.0 = 90 degrees
	float		xOrigin; // should probably always be 0.5
	float		yOrigin; // between be around 0.3 to 0.5

	vec3_t		vieworg;
	vec3_t		viewangles;

	int		ambientlight;
} oldrefdef_t;

extern oldrefdef_t	r_refdef;

#include "model.h"

/*
====================================================

  CONSTANTS

====================================================
*/

#define VID_CBITS	6
#define VID_GRADES	(1 << VID_CBITS)


// sw_local.h: general refresh-related stuff shared between the refresh and the
// driver


#define MAXVERTS	64              // max points in a surface polygon
#define MAXWORKINGVERTS	(MAXVERTS+4)    // max points in an intermediate
					//  polygon (while processing)

#define PARTICLE_Z_CLIP 8.0

#define TRANSPARENT_COLOR	0xFF

#define CYCLE			128 // turbulent cycle size

// flags in finalvert_t.flags
#define ALIAS_LEFT_CLIP		0x0001
#define ALIAS_TOP_CLIP		0x0002
#define ALIAS_RIGHT_CLIP	0x0004
#define ALIAS_BOTTOM_CLIP	0x0008
#define ALIAS_Z_CLIP		0x0010
#define ALIAS_XY_CLIP_MASK	0x000F

#define SURFCACHE_SIZE_AT_320X240	1024*768

#define BMODEL_FULLY_CLIPPED	0x10 // value returned by R_BmodelCheckBBox ()
				     //  if bbox is trivially rejected

#define XCENTERING	(1.0 / 2.0)
#define YCENTERING	(1.0 / 2.0)

#define BACKFACE_EPSILON	0.01

#define NEAR_CLIP	0.01

#define ALIAS_Z_CLIP_PLANE	4

// turbulence stuff
#define AMP	8*0x10000
#define AMP2	3
#define SPEED	20


/*
====================================================

TYPES

====================================================
*/

typedef struct
{
	float   u, v;
	float   s, t;
	float   zi;
} emitpoint_t;

/*
** if you change this structure be sure to change the #defines
** listed after it!
*/
typedef struct finalvert_s {
	int		u, v, s, t;
	int		l;
	zvalue_t	zi;
	int		flags;
	float		xyz[3];         // eye space
} finalvert_t;


typedef struct
{
	void	*pskin;
	int	skinwidth;
	int	skinheight;
} affinetridesc_t;

typedef struct
{
	byte		*surfdat; // destination for generated surface
	int		rowbytes; // destination logical width in bytes
	msurface_t	*surf; // description for surface to generate
	fixed8_t	lightadj[MAXLIGHTMAPS]; // adjust for lightmap levels for dynamic lighting
	image_t		*image;
	int		surfmip; // mipmapped ratio of surface texels / world pixels
	int		surfwidth; // in mipmapped texels
	int		surfheight; // in mipmapped texels
} drawsurf_t;

typedef struct {
	int	ambientlight;
	int	shadelight;
	float	*plightvec;
} alight_t;

// clipped bmodel edges
typedef struct bedge_s
{
	mvertex_t	*v[2];
	struct bedge_s	*pnext;
} bedge_t;

typedef struct clipplane_s
{
	vec3_t	normal;
	float	dist;
	struct clipplane_s *next;
	byte	leftedge;
	byte	rightedge;
	byte	reserved[2];
} clipplane_t;

typedef struct surfcache_s
{
	struct surfcache_s	*next;
	struct surfcache_s	**owner; // NULL is an empty chunk of memory
	int			lightadj[MAXLIGHTMAPS]; // checked for strobe flush
	int			dlight;
	int			size; // including header
	unsigned		width;
	unsigned		height; // DEBUG only needed for debug
	float			mipscale;
	image_t			*image;
	byte			data[4]; // width*height elements
} surfcache_t;

typedef struct espan_s
{
	int		u, v, count;
	struct espan_s	*pnext;
} espan_t;
extern espan_t	*vid_polygon_spans; // space for spans in r_poly

// used by the polygon drawer (sw_poly.c) and sprite setup code (sw_sprite.c)
typedef struct
{
	int	nump;
	emitpoint_t *pverts;
	byte	*pixels; // image
	int	pixel_width; // image width
	int	pixel_height; // image height
	vec3_t	vup, vright, vpn; // in worldspace, for plane eq
	float	dist;
	float	s_offset, t_offset;
	float	viewer_position[3];
	void	(*drawspanlet)(const int *r_turb_turb);
	int	stipple_parity;
} polydesc_t;

// FIXME: compress, make a union if that will help
// insubmodel is only 1, flags is fewer than 32, spanstate could be a byte
typedef struct surf_s
{
	struct surf_s	*next; // active surface stack in r_edge.c
	struct surf_s	*prev; // used in r_edge.c for active surf stack
	struct espan_s	*spans; // pointer to linked list of spans to draw
	int		key;    // sorting key (BSP order)
	shift20_t	last_u; // set during tracing
	int		spanstate; // 0 = not in span
				   // 1 = in span
				   // -1 = in inverted span (end before
				   //  start)
	int		flags;	   // currentface flags
	msurface_t	*msurf;
	entity_t	*entity;
	float		nearzi; // nearest 1/z on surface, for mipmapping
	qboolean	insubmodel;
	float		d_ziorigin, d_zistepu, d_zistepv;
} surf_t;

typedef unsigned short	surfindex_t;

// surface index size
#define SURFINDEX_MAX	(1 << (sizeof(surfindex_t) * 8))

typedef struct edge_s
{
	shift20_t	u;
	shift20_t	u_step;
	struct edge_s	*prev, *next;
	surfindex_t	surfs[2];
	struct edge_s	*nextremove;
	float		nearzi;
	medge_t		*owner;
} edge_t;


/*
====================================================

VARS

====================================================
*/
extern int	r_framecount; // sequence # of current frame since Quake
			      //  started
extern float	r_aliasuvscale; // scale-up factor for screen u and v
				//  on Alias vertices passed to driver
extern qboolean	r_dowarp;

extern affinetridesc_t	r_affinetridesc;

void D_WarpScreen(void);
void R_PolysetUpdateTables(void);

//=======================================================================//

// callbacks to Quake

extern int		c_surf;

extern pixel_t		*r_warpbuffer;

extern float		scale_for_mip;

extern float	d_sdivzstepu, d_tdivzstepu;
extern float	d_sdivzstepv, d_tdivzstepv;
extern float	d_sdivzorigin, d_tdivzorigin;

void D_DrawSpansPow2(espan_t *pspan, float d_ziorigin, float d_zistepu, float d_zistepv);
void D_DrawZSpans(espan_t *pspan, float d_ziorigin, float d_zistepu, float d_zistepv);
void TurbulentPow2(espan_t *pspan, float d_ziorigin, float d_zistepu, float d_zistepv);
void NonTurbulentPow2(espan_t *pspan, float d_ziorigin, float d_zistepu, float d_zistepv);

surfcache_t *D_CacheSurface(const entity_t *currententity, msurface_t *surface, int miplevel);

extern int	d_vrectx, d_vrecty, d_vrectright_particle, d_vrectbottom_particle;

extern int	d_pix_min, d_pix_max, d_pix_mul;

extern pixel_t	*d_viewbuffer;
extern zvalue_t	*d_pzbuffer;

extern int	d_minmip;
extern float	d_scalemip[NUM_MIPS-1];

//===================================================================

extern int	cachewidth;
extern pixel_t	*cacheblock;

extern int	r_drawnpolycount;

extern int	*sintable;
extern int	*intsintable;
extern int	*blanktable;

extern vec3_t	vup, base_vup;
extern vec3_t	vpn, base_vpn;
extern vec3_t	vright, base_vright;

extern surf_t	*surfaces, *surface_p, *surf_max;
// allow some very large lightmaps
extern light_t	*blocklights, *blocklight_max;

// surfaces are generated in back to front order by the bsp, so if a surf
// pointer is greater than another one, it should be drawn in front
// surfaces[1] is the background, and is used as the active surface stack.
// surfaces[0] is a dummy, because index 0 is used to indicate no surface
// attached to an edge_t

//===================================================================

extern float	xcenter, ycenter;
extern float	xscale, yscale;
extern float	xscaleinv, yscaleinv;
extern float	xscaleshrink, yscaleshrink;

extern void TransformVector(vec3_t in, vec3_t out);

//===========================================================================

extern cvar_t	*sw_clearcolor;
extern cvar_t	*sw_drawflat;
extern cvar_t	*sw_draworder;
extern cvar_t	*sw_mipcap;
extern cvar_t	*sw_mipscale;
extern cvar_t	*sw_stipplealpha;
extern cvar_t	*sw_surfcacheoverride;
extern cvar_t	*sw_waterwarp;
extern cvar_t	*sw_gunzposition;

extern cvar_t	*r_fullbright;
extern cvar_t	*r_lefthand;
extern cvar_t	*r_gunfov;
extern cvar_t	*r_drawworld;
extern cvar_t	*r_lerpmodels;
extern cvar_t	*r_lightlevel;
extern cvar_t	*r_modulate;


extern clipplane_t	view_clipplanes[4];
extern int		*pfrustum_indexes[4];


//=============================================================================

void R_RenderWorld(void);

//=============================================================================

extern mplane_t        screenedge[4];

extern vec3_t  r_origin;

extern entity_t	r_worldentity;
extern vec3_t	modelorg;
extern vec3_t	r_entorigin;

extern  int	r_visframecount;

extern msurface_t	*r_alpha_surfaces;

//=============================================================================

//
// current entity info
//
void R_DrawAlphaSurfaces(const entity_t *currententity);

void R_DrawSprite(entity_t *currententity, const model_t *currentmodel);

void R_RenderFace(entity_t *currententity, const model_t *currentmodel, msurface_t *fa, int clipflags, qboolean insubmodel);
void R_RenderBmodelFace(entity_t *currententity, bedge_t *pedges, msurface_t *psurf, int r_currentbkey);
void R_TransformFrustum(void);

void R_DrawSubmodelPolygons(entity_t *currententity, const model_t *currentmodel, int clipflags, mnode_t *topnode);
void R_DrawSolidClippedSubmodelPolygons(entity_t *currententity, const model_t *currentmodel, mnode_t *topnode);

void R_AliasDrawModel(entity_t *currententity, const model_t *currentmodel);
void R_BeginEdgeFrame(void);
void R_ScanEdges(surf_t *surface);
void R_PushDlights(const model_t *model);
void R_RotateBmodel(const entity_t *currententity);

extern int	c_faceclip;
extern int	r_polycount;

extern int	sadjust, tadjust;
extern int	bbextents, bbextentt;

extern int	r_currentkey;

void R_DrawParticles (void);

extern int	r_amodels_drawn;
extern int	r_numallocatededges;
extern edge_t	*r_edges, *edge_p, *edge_max;

extern edge_t	**newedges;
extern edge_t	**removeedges;

typedef struct {
	int		u, v, count;
	pixel_t		*ptex;
	int		sfrac, tfrac, light;
	zvalue_t	zi;
} spanpackage_t;
extern spanpackage_t	*triangle_spans, *triangles_max;

void R_PolysetDrawSpans8_33(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpans8_66(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpans8_Opaque(const entity_t *currententity, spanpackage_t *pspanpackage);

extern byte	**warp_rowptr;
extern int	*warp_column;
extern espan_t	*edge_basespans;
extern espan_t	*max_span_p;
extern int	r_numallocatedverts;
extern int	r_numallocatededgebasespans;
extern finalvert_t	*finalverts, *finalverts_max;

extern	int	r_aliasblendcolor;

extern float	aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

extern qboolean	r_outofsurfaces;
extern qboolean	r_outofedges;
extern qboolean	r_outofverts;
extern qboolean	r_outoftriangles;
extern qboolean	r_outoflights;
extern qboolean	r_outedgebasespans;

extern mvertex_t	*r_pcurrentvertbase;

void R_DrawTriangle(const entity_t *currententity, const finalvert_t *a, const finalvert_t *b, const finalvert_t *c);
void R_AliasClipTriangle(const entity_t *currententity, const finalvert_t *index0, const finalvert_t *index1, finalvert_t *index2);


extern float	r_time1;
extern float	da_time1, da_time2;
extern float	dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
extern float	se_time1, se_time2, de_time1, de_time2;
extern int r_viewcluster, r_oldviewcluster;

extern int r_clipflags;

extern image_t		 *r_notexture_mip;
extern model_t		 *r_worldmodel;

void R_PrintAliasStats (void);
void R_PrintTimes (void);
void R_PrintDSpeeds (void);
void R_LightPoint (const entity_t *currententity, vec3_t p, vec3_t color);
void R_SetupFrame (void);

extern  refdef_t		r_newrefdef;

extern  surfcache_t	*sc_base;

extern  void			*colormap;

//====================================================================

void R_NewMap (void);
void Draw_InitLocal(void);
void R_InitCaches(void);
void D_FlushCaches(void);

void	RE_BeginRegistration (char *model);
struct model_s	*RE_RegisterModel (char *name);
void	RE_EndRegistration (void);

struct image_s	*RE_Draw_FindPic (char *name);

void	RE_Draw_GetPicSize (int *w, int *h, char *name);
void	RE_Draw_PicScaled (int x, int y, char *name, float scale);
void	RE_Draw_StretchPic (int x, int y, int w, int h, char *name);
void	RE_Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data);
void	RE_Draw_CharScaled (int x, int y, int c, float scale);
void	RE_Draw_TileClear (int x, int y, int w, int h, char *name);
void	RE_Draw_Fill (int x, int y, int w, int h, int c);
void	RE_Draw_FadeScreen (void);

void LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height);

void	R_InitImages(void);
void	R_ShutdownImages(void);
image_t	*R_FindImage(char *name, imagetype_t type);
void	R_FreeUnusedImages(void);

void R_InitSkyBox(void);
void R_IMFlatShadedQuad( vec3_t a, vec3_t b, vec3_t c, vec3_t d, int color, float alpha );

// VID Buffer damage
void VID_DamageBuffer(int u, int v);

// VID zBuffer damage
extern qboolean	fastmoving;
void VID_DamageZBuffer(int u, int v);
qboolean VID_CheckDamageZBuffer(int u, int v, int ucount, int vcount);

/*
====================================================================

IMPORTED FUNCTIONS

====================================================================
*/
extern refimport_t	ri;

#endif
