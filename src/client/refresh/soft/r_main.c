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

// r_main.c
#include <stdint.h>

#ifdef SDL2
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

#else // SDL1.2
#include <SDL/SDL.h>
#endif //SDL2

#include "header/local.h"

viddef_t	vid;
pixel_t		*vid_buffer = NULL;
espan_t		*vid_polygon_spans = NULL;
pixel_t		*vid_colormap = NULL;
pixel_t		*vid_alphamap = NULL;

refimport_t	ri;

unsigned	d_8to24table[256];

entity_t	r_worldentity;

char		skyname[MAX_QPATH];
float		skyrotate;
vec3_t		skyaxis;
image_t		*sky_images[6];

refdef_t	r_newrefdef;
model_t		*currentmodel;

model_t		*r_worldmodel;

pixel_t		*r_warpbuffer;

swstate_t sw_state;

void		*colormap;
vec3_t		viewlightvec;
alight_t	r_viewlighting = {128, 192, viewlightvec};
float		r_time1;
int			r_numallocatededges;
float		r_aliasuvscale = 1.0;
int			r_outofsurfaces;
int			r_outofedges;

qboolean	r_dowarp;

mvertex_t	*r_pcurrentvertbase;

int			c_surf;
int			r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
qboolean	r_surfsonstack;
int			r_clipflags;

//
// view origin
//
vec3_t	vup, base_vup;
vec3_t	vpn, base_vpn;
vec3_t	vright, base_vright;
vec3_t	r_origin;

//
// screen size info
//
oldrefdef_t	r_refdef;
float		xcenter, ycenter;
float		xscale, yscale;
float		xscaleinv, yscaleinv;
float		xscaleshrink, yscaleshrink;
float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int		r_screenwidth;

float	verticalFieldOfView;
float	xOrigin, yOrigin;

mplane_t	screenedge[4];

//
// refresh flags
//
int		r_framecount = 1;	// so frame counts initialized to 0 don't match
int		r_visframecount;
int		d_spanpixcount;
int		r_polycount;
int		r_drawnpolycount;
int		r_wholepolycount;

int			*pfrustum_indexes[4];
int			r_frustum_indexes[4*6];

mleaf_t		*r_viewleaf;
int			r_viewcluster, r_oldviewcluster;

image_t  	*r_notexture_mip;

float	da_time1, da_time2, dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
float	se_time1, se_time2, de_time1, de_time2;

void R_MarkLeaves (void);

cvar_t	*r_lefthand;
cvar_t	*sw_aliasstats;
cvar_t	*sw_allow_modex;
cvar_t	*sw_clearcolor;
cvar_t	*sw_drawflat;
cvar_t	*sw_draworder;
cvar_t	*sw_maxedges;
cvar_t	*sw_maxsurfs;
cvar_t  *r_mode;
cvar_t	*sw_reportedgeout;
cvar_t	*sw_reportsurfout;
cvar_t  *sw_stipplealpha;
cvar_t	*sw_surfcacheoverride;
cvar_t	*sw_waterwarp;
cvar_t	*sw_overbrightbits;

cvar_t	*r_drawworld;
cvar_t	*r_drawentities;
cvar_t	*r_dspeeds;
cvar_t	*r_fullbright;
cvar_t  *r_lerpmodels;
cvar_t  *r_novis;

cvar_t	*r_speeds;
cvar_t	*r_lightlevel;	//FIXME HACK

cvar_t	*vid_fullscreen;
cvar_t	*vid_gamma;

//PGM
cvar_t	*r_lockpvs;
//PGM

#define	STRINGER(x) "x"

// r_vars.c

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver


// d_vars.c

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver

float	d_sdivzstepu, d_tdivzstepu, d_zistepu;
float	d_sdivzstepv, d_tdivzstepv, d_zistepv;
float	d_sdivzorigin, d_tdivzorigin, d_ziorigin;

int	sadjust, tadjust, bbextents, bbextentt;

pixel_t		*cacheblock;
int		cachewidth;
pixel_t		*d_viewbuffer;
zvalue_t	*d_pzbuffer;
unsigned int	d_zrowbytes;
unsigned int	d_zwidth;

struct texture_buffer {
	image_t	image;
	byte	buffer[1024];
} r_notexture_buffer;

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
	int		x,y, m;

	// create a simple checkerboard texture for the default
	r_notexture_mip = &r_notexture_buffer.image;

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->pixels[0] = r_notexture_buffer.buffer;
	r_notexture_mip->pixels[1] = r_notexture_mip->pixels[0] + 16*16;
	r_notexture_mip->pixels[2] = r_notexture_mip->pixels[1] + 8*8;
	r_notexture_mip->pixels[3] = r_notexture_mip->pixels[2] + 4*4;

	for (m=0 ; m<4 ; m++)
	{
		byte	*dest;

		dest = r_notexture_mip->pixels[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )

					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}
}


/*
================
R_InitTurb
================
*/
void R_InitTurb (void)
{
	int		i;

	memset(blanktable, 0, (vid.width+CYCLE) * sizeof(int));
	for (i = 0; i < (vid.width+CYCLE); i++)
	{
		sintable[i] = AMP + sin(i*3.14159*2/CYCLE)*AMP;
		intsintable[i] = AMP2 + sin(i*3.14159*2/CYCLE)*AMP2; // AMP2, not 20
	}
}

void R_ImageList_f( void );

void R_Register (void)
{
	sw_aliasstats = ri.Cvar_Get ("sw_polymodelstats", "0", 0);
	sw_allow_modex = ri.Cvar_Get( "sw_allow_modex", "1", CVAR_ARCHIVE );
	sw_clearcolor = ri.Cvar_Get ("sw_clearcolor", "2", 0);
	sw_drawflat = ri.Cvar_Get ("sw_drawflat", "0", 0);
	sw_draworder = ri.Cvar_Get ("sw_draworder", "0", 0);
	sw_maxedges = ri.Cvar_Get ("sw_maxedges", STRINGER(MAXSTACKSURFACES), 0);
	sw_maxsurfs = ri.Cvar_Get ("sw_maxsurfs", "0", 0);
	sw_mipcap = ri.Cvar_Get ("sw_mipcap", "0", 0);
	sw_mipscale = ri.Cvar_Get ("sw_mipscale", "1", 0);
	sw_reportedgeout = ri.Cvar_Get ("sw_reportedgeout", "0", 0);
	sw_reportsurfout = ri.Cvar_Get ("sw_reportsurfout", "0", 0);
	sw_stipplealpha = ri.Cvar_Get( "sw_stipplealpha", "0", CVAR_ARCHIVE );
	sw_surfcacheoverride = ri.Cvar_Get ("sw_surfcacheoverride", "0", 0);
	sw_waterwarp = ri.Cvar_Get ("sw_waterwarp", "1", 0);
	sw_overbrightbits = ri.Cvar_Get("sw_overbrightbits", "1.0", CVAR_ARCHIVE);
	r_mode = ri.Cvar_Get( "r_mode", "0", CVAR_ARCHIVE );

	r_lefthand = ri.Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	r_speeds = ri.Cvar_Get ("r_speeds", "0", 0);
	r_fullbright = ri.Cvar_Get ("r_fullbright", "0", 0);
	r_drawentities = ri.Cvar_Get ("r_drawentities", "1", 0);
	r_drawworld = ri.Cvar_Get ("r_drawworld", "1", 0);
	r_dspeeds = ri.Cvar_Get ("r_dspeeds", "0", 0);
	r_lightlevel = ri.Cvar_Get ("r_lightlevel", "0", 0);
	r_lerpmodels = ri.Cvar_Get( "r_lerpmodels", "1", 0 );
	r_novis = ri.Cvar_Get( "r_novis", "0", 0 );

	vid_fullscreen = ri.Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE );
	vid_gamma = ri.Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );

	ri.Cmd_AddCommand("modellist", Mod_Modellist_f);
	ri.Cmd_AddCommand("screenshot", R_ScreenShot_f);
	ri.Cmd_AddCommand("imagelist", R_ImageList_f);

	r_mode->modified = true; // force us to do mode specific stuff later
	vid_gamma->modified = true; // force us to rebuild the gamma table later
	sw_overbrightbits->modified = true; // force us to rebuild pallete later

	//PGM
	r_lockpvs = ri.Cvar_Get ("r_lockpvs", "0", 0);
	//PGM
}

void R_UnRegister (void)
{
	ri.Cmd_RemoveCommand( "screenshot" );
	ri.Cmd_RemoveCommand ("modellist");
	ri.Cmd_RemoveCommand( "imagelist" );
}

/*
===============
R_Init
===============
*/
qboolean RE_Init(void)
{
	R_InitImages ();
	Mod_Init ();
	Draw_InitLocal ();
	R_InitTextures ();

	view_clipplanes[0].leftedge = true;
	view_clipplanes[1].rightedge = true;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge =
			view_clipplanes[3].leftedge = false;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge =
			view_clipplanes[3].rightedge = false;

	r_refdef.xOrigin = XCENTERING;
	r_refdef.yOrigin = YCENTERING;

	r_aliasuvscale = 1.0;

	R_Register ();
	Draw_GetPalette ();
	if (SWimp_Init() == false)
		return false;

	// create the window
	RE_BeginFrame( 0 );

	R_Printf(PRINT_ALL, "ref_soft version: "REF_VERSION"\n");

	return true;
}

/*
===============
RE_Shutdown
===============
*/
void RE_Shutdown (void)
{
	// free z buffer
	if (d_pzbuffer)
	{
		free (d_pzbuffer);
		d_pzbuffer = NULL;
	}
	// free surface cache
	if (sc_base)
	{
		D_FlushCaches ();
		free (sc_base);
		sc_base = NULL;
	}

	// free colormap
	if (vid_colormap)
	{
		free (vid_colormap);
		vid_colormap = NULL;
	}
	R_UnRegister ();
	Mod_FreeAll ();
	R_ShutdownImages ();

	SWimp_Shutdown();
}

/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	r_viewcluster = -1;

	r_cnumsurfs = sw_maxsurfs->value;

	if (r_cnumsurfs <= MINSURFACES)
		r_cnumsurfs = MINSURFACES;

	if (r_cnumsurfs > NUMSTACKSURFACES)
	{
		surfaces = malloc (r_cnumsurfs * sizeof(surf_t));
		if (!surfaces)
		{
		    R_Printf(PRINT_ALL, "R_NewMap: Couldn't malloc %ld bytes\n", r_cnumsurfs * sizeof(surf_t));
		    return;
		}

		surface_p = surfaces;
		surf_max = &surfaces[r_cnumsurfs];
		r_surfsonstack = false;
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;
	}
	else
	{
		r_surfsonstack = true;
	}

	r_maxedgesseen = 0;
	r_maxsurfsseen = 0;

	r_numallocatededges = sw_maxedges->value;

	if (r_numallocatededges < MINEDGES)
		r_numallocatededges = MINEDGES;

	if (r_numallocatededges <= NUMSTACKEDGES)
	{
		auxedges = NULL;
	}
	else
	{
		auxedges = malloc (r_numallocatededges * sizeof(edge_t));
	}
}


/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
void R_MarkLeaves (void)
{
	byte	*vis;
	mnode_t	*node;
	int		i;
	mleaf_t	*leaf;

	if (r_oldviewcluster == r_viewcluster && !r_novis->value && r_viewcluster != -1)
		return;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if (r_lockpvs->value)
		return;

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;

	if (r_novis->value || r_viewcluster == -1 || !r_worldmodel->vis)
	{
		// mark everything
		for (i=0 ; i<r_worldmodel->numleafs ; i++)
			r_worldmodel->leafs[i].visframe = r_visframecount;
		for (i=0 ; i<r_worldmodel->numnodes ; i++)
			r_worldmodel->nodes[i].visframe = r_visframecount;
		return;
	}

	vis = Mod_ClusterPVS (r_viewcluster, r_worldmodel);

	for (i=0,leaf=r_worldmodel->leafs ; i<r_worldmodel->numleafs ; i++, leaf++)
	{
		int cluster;

		cluster = leaf->cluster;
		if (cluster == -1)
			continue;
		if (vis[cluster>>3] & (1<<(cluster&7)))
		{
			node = (mnode_t *)leaf;
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

/*
** R_DrawNullModel
**
** IMPLEMENT THIS!
*/
void R_DrawNullModel( void )
{
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int			i;
	qboolean	translucent_entities = false;

	if (!r_drawentities->value)
		return;

	// all bmodels have already been drawn by the edge list
	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];

		if ( currententity->flags & RF_TRANSLUCENT )
		{
			translucent_entities = true;
			continue;
		}

		if ( currententity->flags & RF_BEAM )
		{
			modelorg[0] = -r_origin[0];
			modelorg[1] = -r_origin[1];
			modelorg[2] = -r_origin[2];
			VectorCopy( vec3_origin, r_entorigin );
			R_DrawBeam( currententity );
		}
		else
		{
			currentmodel = currententity->model;
			if (!currentmodel)
			{
				R_DrawNullModel();
				continue;
			}
			VectorCopy (currententity->origin, r_entorigin);
			VectorSubtract (r_origin, r_entorigin, modelorg);

			switch (currentmodel->type)
			{
			case mod_sprite:
				R_DrawSprite ();
				break;

			case mod_alias:
				R_AliasDrawModel ();
				break;

			default:
				break;
			}
		}
	}

	if ( !translucent_entities )
		return;

	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];

		if ( !( currententity->flags & RF_TRANSLUCENT ) )
			continue;

		if ( currententity->flags & RF_BEAM )
		{
			modelorg[0] = -r_origin[0];
			modelorg[1] = -r_origin[1];
			modelorg[2] = -r_origin[2];
			VectorCopy( vec3_origin, r_entorigin );
			R_DrawBeam( currententity );
		}
		else
		{
			currentmodel = currententity->model;
			if (!currentmodel)
			{
				R_DrawNullModel();
				continue;
			}
			VectorCopy (currententity->origin, r_entorigin);
			VectorSubtract (r_origin, r_entorigin, modelorg);

			switch (currentmodel->type)
			{
			case mod_sprite:
				R_DrawSprite ();
				break;

			case mod_alias:
				R_AliasDrawModel ();
				break;

			default:
				break;
			}
		}
	}
}


/*
=============
R_BmodelCheckBBox
=============
*/
int R_BmodelCheckBBox (float *minmaxs)
{
	int i, clipflags;

	clipflags = 0;

	for (i=0 ; i<4 ; i++)
	{
		vec3_t acceptpt, rejectpt;
		int *pindex;
		float d;

		// generate accept and reject points
		// FIXME: do with fast look-ups or integer tests based on the sign bit
		// of the floating point values
		pindex = pfrustum_indexes[i];

		rejectpt[0] = minmaxs[pindex[0]];
		rejectpt[1] = minmaxs[pindex[1]];
		rejectpt[2] = minmaxs[pindex[2]];

		d = DotProduct (rejectpt, view_clipplanes[i].normal);
		d -= view_clipplanes[i].dist;

		if (d <= 0)
			return BMODEL_FULLY_CLIPPED;

		acceptpt[0] = minmaxs[pindex[3+0]];
		acceptpt[1] = minmaxs[pindex[3+1]];
		acceptpt[2] = minmaxs[pindex[3+2]];

		d = DotProduct (acceptpt, view_clipplanes[i].normal);
		d -= view_clipplanes[i].dist;

		if (d <= 0)
			clipflags |= (1<<i);
	}

	return clipflags;
}


/*
===================
R_FindTopnode

Find the first node that splits the given box
===================
*/
mnode_t *R_FindTopnode (vec3_t mins, vec3_t maxs)
{
	mnode_t *node;

	node = r_worldmodel->nodes;

	while (1)
	{
		mplane_t *splitplane;
		int sides;

		if (node->visframe != r_visframecount)
			return NULL;		// not visible at all

		if (node->contents != CONTENTS_NODE)
		{
			if (node->contents != CONTENTS_SOLID)
				return	node;	// we've reached a non-solid leaf, so it's
						//  visible and not BSP clipped
			return NULL;	// in solid, so not visible
		}

		splitplane = node->plane;
		sides = BOX_ON_PLANE_SIDE(mins, maxs, (cplane_t *)splitplane);

		if (sides == 3)
			return node;	// this is the splitter

		// not split yet; recurse down the contacted side
		if (sides & 1)
			node = node->children[0];
		else
			node = node->children[1];
	}
}


/*
=============
RotatedBBox

Returns an axially aligned box that contains the input box at the given rotation
=============
*/
void RotatedBBox (vec3_t mins, vec3_t maxs, vec3_t angles, vec3_t tmins, vec3_t tmaxs)
{
	vec3_t	tmp, v;
	int		i, j;
	vec3_t	forward, right, up;

	if (!angles[0] && !angles[1] && !angles[2])
	{
		VectorCopy (mins, tmins);
		VectorCopy (maxs, tmaxs);
		return;
	}

	for (i=0 ; i<3 ; i++)
	{
		tmins[i] = 99999;
		tmaxs[i] = -99999;
	}

	AngleVectors (angles, forward, right, up);

	for ( i = 0; i < 8; i++ )
	{
		if ( i & 1 )
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if ( i & 2 )
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if ( i & 4 )
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];


		VectorScale (forward, tmp[0], v);
		VectorMA (v, -tmp[1], right, v);
		VectorMA (v, tmp[2], up, v);

		for (j=0 ; j<3 ; j++)
		{
			if (v[j] < tmins[j])
				tmins[j] = v[j];
			if (v[j] > tmaxs[j])
				tmaxs[j] = v[j];
		}
	}
}

/*
=============
R_DrawBEntitiesOnList
=============
*/
void R_DrawBEntitiesOnList (void)
{
	int			i, clipflags;
	vec3_t		oldorigin;
	vec3_t		mins, maxs;
	float		minmaxs[6];
	mnode_t		*topnode;

	if (!r_drawentities->value)
		return;

	VectorCopy (modelorg, oldorigin);
	insubmodel = true;
	r_dlightframecount = r_framecount;

	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];
		currentmodel = currententity->model;
		if (!currentmodel)
			continue;
		if (currentmodel->nummodelsurfaces == 0)
			continue;	// clip brush only
		if ( currententity->flags & RF_BEAM )
			continue;
		if (currentmodel->type != mod_brush)
			continue;
		// see if the bounding box lets us trivially reject, also sets
		// trivial accept status
		RotatedBBox (currentmodel->mins, currentmodel->maxs,
			currententity->angles, mins, maxs);
		VectorAdd (mins, currententity->origin, minmaxs);
		VectorAdd (maxs, currententity->origin, (minmaxs+3));

		clipflags = R_BmodelCheckBBox (minmaxs);
		if (clipflags == BMODEL_FULLY_CLIPPED)
			continue;	// off the edge of the screen

		topnode = R_FindTopnode (minmaxs, minmaxs+3);
		if (!topnode)
			continue;	// no part in a visible leaf

		VectorCopy (currententity->origin, r_entorigin);
		VectorSubtract (r_origin, r_entorigin, modelorg);

		r_pcurrentvertbase = currentmodel->vertexes;

		// FIXME: stop transforming twice
		R_RotateBmodel ();

		// calculate dynamic lighting for bmodel
		R_PushDlights (currentmodel);

		if (topnode->contents == CONTENTS_NODE)
		{
			// not a leaf; has to be clipped to the world BSP
			r_clipflags = clipflags;
			R_DrawSolidClippedSubmodelPolygons (currentmodel, topnode);
		}
		else
		{
			// falls entirely in one leaf, so we just put all the
			// edges in the edge list and let 1/z sorting handle
			// drawing order
			R_DrawSubmodelPolygons (currentmodel, clipflags, topnode);
		}

		// put back world rotation and frustum clipping
		// FIXME: R_RotateBmodel should just work off base_vxx
		VectorCopy (base_vpn, vpn);
		VectorCopy (base_vup, vup);
		VectorCopy (base_vright, vright);
		VectorCopy (oldorigin, modelorg);
		R_TransformFrustum ();
	}

	insubmodel = false;
}

edge_t	*ledges;
surf_t	*lsurfs;

/*
================
R_EdgeDrawing
================
*/
void R_EdgeDrawing (void)
{
	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL )
		return;

	if (auxedges)
	{
		r_edges = auxedges;
	}
	else
	{
		r_edges = ledges;
	}

	if (r_surfsonstack)
	{
		surfaces = lsurfs;
		surf_max = &surfaces[r_cnumsurfs];
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;
	}

	R_BeginEdgeFrame ();

	if (r_dspeeds->value)
	{
		rw_time1 = SDL_GetTicks();
	}

	R_RenderWorld ();

	if (r_dspeeds->value)
	{
		rw_time2 = SDL_GetTicks();
		db_time1 = rw_time2;
	}

	R_DrawBEntitiesOnList ();

	if (r_dspeeds->value)
	{
		db_time2 = SDL_GetTicks();
		se_time1 = db_time2;
	}

	R_ScanEdges ();
}

//=======================================================================


/*
=============
R_CalcPalette

=============
*/
void R_CalcPalette (void)
{
	static qboolean modified;
	byte	palette[256][4], *in, *out;
	int		i, j;
	float	alpha, one_minus_alpha;
	vec3_t	premult;
	int		v;

	alpha = r_newrefdef.blend[3];
	if (alpha <= 0)
	{
		if (modified)
		{	// set back to default
			modified = false;
			R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_8to24table );
			return;
		}
		return;
	}

	modified = true;
	if (alpha > 1)
		alpha = 1;

	premult[0] = r_newrefdef.blend[0]*alpha*255;
	premult[1] = r_newrefdef.blend[1]*alpha*255;
	premult[2] = r_newrefdef.blend[2]*alpha*255;

	one_minus_alpha = (1.0 - alpha);

	in = (byte *)d_8to24table;
	out = palette[0];
	for (i=0 ; i<256 ; i++, in+=4, out+=4)
	{
		for (j=0 ; j<3 ; j++)
		{
			v = premult[j] + one_minus_alpha * in[j];
			if (v > 255)
				v = 255;
			out[j] = v;
		}
		out[3] = 255;
	}

	R_GammaCorrectAndSetPalette( ( const unsigned char * ) palette[0] );
}

//=======================================================================

void R_SetLightLevel (void)
{
	vec3_t		light;

	if ((r_newrefdef.rdflags & RDF_NOWORLDMODEL) || (!r_drawentities->value) || (!currententity))
	{
		r_lightlevel->value = 150.0;
		return;
	}

	// save off light value for server to look at (BIG HACK!)
	R_LightPoint (r_newrefdef.vieworg, light);
	r_lightlevel->value = 150.0 * light[0];
}


/*
================
RE_RenderFrame

================
*/
void RE_RenderFrame (refdef_t *fd)
{
	r_newrefdef = *fd;

	if (!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
		ri.Sys_Error (ERR_FATAL,"R_RenderView: NULL worldmodel");

	VectorCopy (fd->vieworg, r_refdef.vieworg);
	VectorCopy (fd->viewangles, r_refdef.viewangles);

	if (r_speeds->value || r_dspeeds->value)
		r_time1 = SDL_GetTicks();

	R_SetupFrame ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_PushDlights (r_worldmodel);

	R_EdgeDrawing ();

	if (r_dspeeds->value)
	{
		se_time2 = SDL_GetTicks();
		de_time1 = se_time2;
	}

	R_DrawEntitiesOnList ();

	if (r_dspeeds->value)
	{
		de_time2 = SDL_GetTicks();
		dp_time1 = SDL_GetTicks();
	}

	R_DrawParticles ();

	if (r_dspeeds->value)
		dp_time2 = SDL_GetTicks();

	R_DrawAlphaSurfaces();

	R_SetLightLevel ();

	if (r_dowarp)
		D_WarpScreen ();

	if (r_dspeeds->value)
		da_time1 = SDL_GetTicks();

	if (r_dspeeds->value)
		da_time2 = SDL_GetTicks();

	R_CalcPalette ();

	if (sw_aliasstats->value)
		R_PrintAliasStats ();

	if (r_speeds->value)
		R_PrintTimes ();

	if (r_dspeeds->value)
		R_PrintDSpeeds ();

	if (sw_reportsurfout->value && r_outofsurfaces)
		R_Printf(PRINT_ALL,"Short %d surfaces\n", r_outofsurfaces);

	if (sw_reportedgeout->value && r_outofedges)
		R_Printf(PRINT_ALL,"Short roughly %d edges\n", r_outofedges * 2 / 3);
}

/*
** R_InitGraphics
*/
void R_InitGraphics( int width, int height )
{
	vid.width  = width;
	vid.height = height;

	// free z buffer
	if ( d_pzbuffer )
	{
		free(d_pzbuffer);
		d_pzbuffer = NULL;
	}

	// free surface cache
	if ( sc_base )
	{
		D_FlushCaches();
		free(sc_base);
		sc_base = NULL;
	}

	d_pzbuffer = malloc(vid.width * vid.height * sizeof(zvalue_t));

	R_InitCaches();

	R_GammaCorrectAndSetPalette((const unsigned char *)d_8to24table);
}

/*
** RE_BeginFrame
*/
void RE_BeginFrame( float camera_separation )
{
	extern void Draw_BuildGammaTable( void );

	/*
	** rebuild the gamma correction palette if necessary
	*/
	if ( vid_gamma->modified || sw_overbrightbits->modified )
	{
		Draw_BuildGammaTable();
		R_GammaCorrectAndSetPalette((const unsigned char * )d_8to24table);

		vid_gamma->modified = false;
		sw_overbrightbits->modified = false;
	}

	while (r_mode->modified || vid_fullscreen->modified)
	{
		rserr_t err;

		/*
		** if this returns rserr_invalid_fullscreen then it set the mode but not as a
		** fullscreen mode, e.g. 320x200 on a system that doesn't support that res
		*/
		if ((err = SWimp_SetMode( &vid.width, &vid.height, r_mode->value, vid_fullscreen->value)) == rserr_ok )
		{
			R_InitGraphics( vid.width, vid.height );

			sw_state.prev_mode = r_mode->value;
			vid_fullscreen->modified = false;
			r_mode->modified = false;
		}
		else
		{
			if ( err == rserr_invalid_mode )
			{
				ri.Cvar_SetValue( "sw_mode", sw_state.prev_mode );
				R_Printf( PRINT_ALL, "ref_soft::RE_BeginFrame() - could not set mode\n" );
			}
			else if ( err == rserr_invalid_fullscreen )
			{
				R_InitGraphics( vid.width, vid.height );

				ri.Cvar_SetValue( "vid_fullscreen", 0);
				R_Printf( PRINT_ALL, "ref_soft::RE_BeginFrame() - fullscreen unavailable in this mode\n" );
				sw_state.prev_mode = r_mode->value;
			}
			else
			{
				ri.Sys_Error( ERR_FATAL, "ref_soft::RE_BeginFrame() - catastrophic mode change failure\n" );
			}
		}
	}
}

/*
** R_GammaCorrectAndSetPalette
*/
void R_GammaCorrectAndSetPalette( const unsigned char *palette )
{
	int i;

	for ( i = 0; i < 256; i++ )
	{
		sw_state.currentpalette[i*4+0] = sw_state.gammatable[palette[i*4+0]];
		sw_state.currentpalette[i*4+1] = sw_state.gammatable[palette[i*4+1]];
		sw_state.currentpalette[i*4+2] = sw_state.gammatable[palette[i*4+2]];
	}
}

/*
** RE_SetPalette
*/
void RE_SetPalette(const unsigned char *palette)
{
	byte palette32[1024];
	int i;

	// clear screen to black to avoid any palette flash
	memset(vid_buffer, 0, vid.height * vid.width * sizeof(pixel_t));

	// flush it to the screen
	RE_EndFrame ();

	if (palette)
	{
		for ( i = 0; i < 256; i++ )
		{
			palette32[i*4+0] = palette[i*3+0];
			palette32[i*4+1] = palette[i*3+1];
			palette32[i*4+2] = palette[i*3+2];
			palette32[i*4+3] = 0xFF;
		}

		R_GammaCorrectAndSetPalette( palette32 );
	}
	else
	{
		R_GammaCorrectAndSetPalette((const unsigned char *)d_8to24table);
	}
}

/*
================
Draw_BuildGammaTable
================
*/
void Draw_BuildGammaTable (void)
{
	int i;
	float	g;
	float	overbright;

	overbright = sw_overbrightbits->value;

	if(overbright < 0.5)
		overbright = 0.5;

	if(overbright > 4.0)
		overbright = 4.0;

	g = (2.1 - vid_gamma->value);


	if (g == 1.0)
	{
		for (i=0 ; i<256 ; i++) {
			int inf;

			inf = i * overbright;

			if (inf < 0)
				inf = 0;
			if (inf > 255)
				inf = 255;

			sw_state.gammatable[i] = inf;
		}
	}
	else
		for (i=0 ; i<256 ; i++)
		{
			int inf;

			inf = (255 * pow ( (i+0.5)/255.5 , g ) + 0.5) * overbright;
			if (inf < 0)
				inf = 0;
			if (inf > 255)
				inf = 255;
			sw_state.gammatable[i] = inf;
		}
}

/*
** R_DrawBeam
*/
void R_DrawBeam( entity_t *e )
{
#define NUM_BEAM_SEGS 6

	int	i;

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );
	VectorScale( perpvec, e->frame / 2, perpvec );

	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		R_IMFlatShadedQuad( start_points[i],
		                    end_points[i],
							end_points[(i+1)%NUM_BEAM_SEGS],
							start_points[(i+1)%NUM_BEAM_SEGS],
							e->skinnum & 0xFF,
							e->alpha );
	}
}


//===================================================================

/*
============
RE_SetSky
============
*/
// 3dstudio environment map names
char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
int	r_skysideimage[6] = {5, 2, 4, 1, 0, 3};
extern	mtexinfo_t		r_skytexinfo[6];
void RE_SetSky (char *name, float rotate, vec3_t axis)
{
	int		i;
	char	pathname[MAX_QPATH];

	strncpy (skyname, name, sizeof(skyname)-1);
	skyrotate = rotate;
	VectorCopy (axis, skyaxis);

	for (i=0 ; i<6 ; i++)
	{
		Com_sprintf (pathname, sizeof(pathname), "env/%s%s.pcx", skyname, suf[r_skysideimage[i]]);
		r_skytexinfo[i].image = R_FindImage (pathname, it_sky);
	}
}


/*
===============
Draw_GetPalette
===============
*/
void Draw_GetPalette (void)
{
	byte	*pal, *out;
	int		i;

	// get the palette and colormap
	LoadPCX ("pics/colormap.pcx", &vid_colormap, &pal, NULL, NULL);
	if (!vid_colormap)
		ri.Sys_Error (ERR_FATAL, "Couldn't load pics/colormap.pcx");
	vid_alphamap = vid_colormap + 64*256;

	out = (byte *)d_8to24table;
	for (i=0 ; i<256 ; i++, out+=4)
	{
		int r, g, b;

		r = pal[i*3+0];
		g = pal[i*3+1];
		b = pal[i*3+2];

		out[0] = r;
		out[1] = g;
		out[2] = b;
	}

	free (pal);
}

struct image_s *RE_RegisterSkin (char *name);

void R_Printf(int level, const char* msg, ...)
{
	va_list argptr;
	va_start(argptr, msg);
	ri.Com_VPrintf(level, msg, argptr);
	va_end(argptr);
}

qboolean RE_IsVsyncActive(void)
{
	return true;
}

/*
===============
GetRefAPI
===============
*/
Q2_DLL_EXPORTED refexport_t
GetRefAPI(refimport_t imp)
{
	refexport_t	re;

	memset(&re, 0, sizeof(refexport_t));
	ri = imp;

	re.api_version = API_VERSION;

	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.DrawFindPic = RE_Draw_FindPic;
	re.SetSky = RE_SetSky;
	re.EndRegistration = RE_EndRegistration;

	re.RenderFrame = RE_RenderFrame;

	re.DrawGetPicSize = RE_Draw_GetPicSize;

	re.DrawPicScaled = RE_Draw_PicScaled;
	re.DrawStretchPic = RE_Draw_StretchPic;
	re.DrawCharScaled = RE_Draw_CharScaled;
	re.DrawTileClear = RE_Draw_TileClear;
	re.DrawFill = RE_Draw_Fill;
	re.DrawFadeScreen = RE_Draw_FadeScreen;

	re.DrawStretchRaw = RE_Draw_StretchRaw;

	re.Init = RE_Init;
	re.IsVSyncActive = RE_IsVsyncActive;
	re.Shutdown = RE_Shutdown;

	re.SetPalette = RE_SetPalette;
	re.BeginFrame = RE_BeginFrame;
	re.EndFrame = RE_EndFrame;

	Swap_Init ();

	return re;
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
static SDL_Window* window = NULL;
static SDL_Surface *surface = NULL;
static SDL_Texture *texture = NULL;
static SDL_Renderer *renderer = NULL;
#else
static SDL_Surface* window = NULL;
#endif
static qboolean X11_active = false;

/*
** SWimp_Init
**
** This routine is responsible for initializing the implementation
** specific stuff in a software rendering subsystem.
*/
int SWimp_Init(void)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{

		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			Com_Printf("Couldn't init SDL video: %s.\n", SDL_GetError());
			return false;
		}

		SDL_version version;

#if SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_GetVersion(&version);
		const char* driverName = SDL_GetCurrentVideoDriver();
#else
		char driverName[64];
		SDL_VideoDriverName(driverName, sizeof(driverName));
		version = *SDL_Linked_Version();
#endif
		Com_Printf("SDL version is: %i.%i.%i\n", (int)version.major, (int)version.minor, (int)version.patch);
		Com_Printf("SDL video driver is \"%s\".\n", driverName);
	}

	return true;
}

/*
 * Sets the window icon
 */
#if SDL_VERSION_ATLEAST(2, 0, 0)

/* The 64x64 32bit window icon */
#include "../../../backends/sdl/icon/q2icon64.h"

static void
SetSDLIcon()
{
	/* these masks are needed to tell SDL_CreateRGBSurface(From)
	   to assume the data it gets is byte-wise RGB(A) data */
	Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	int shift = (q2icon64.bytes_per_pixel == 3) ? 8 : 0;
	rmask = 0xff000000 >> shift;
	gmask = 0x00ff0000 >> shift;
	bmask = 0x0000ff00 >> shift;
	amask = 0x000000ff >> shift;
#else /* little endian, like x86 */
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = (q2icon64.bytes_per_pixel == 3) ? 0 : 0xff000000;
#endif

	SDL_Surface* icon = SDL_CreateRGBSurfaceFrom((void*)q2icon64.pixel_data, q2icon64.width,
		q2icon64.height, q2icon64.bytes_per_pixel*8, q2icon64.bytes_per_pixel*q2icon64.width,
		rmask, gmask, bmask, amask);

	SDL_SetWindowIcon(window, icon);

	SDL_FreeSurface(icon);
}

#else /* SDL 1.2 */

/* The window icon */
#include "../../../backends/sdl/icon/q2icon.xbm"

static void
SetSDLIcon()
{
	SDL_Surface *icon;
	SDL_Color transColor, solidColor;
	Uint8 *ptr;
	int i;
	int mask;

	icon = SDL_CreateRGBSurface(SDL_SWSURFACE,
			q2icon_width, q2icon_height, 8,
			0, 0, 0, 0);

	if (icon == NULL)
	{
		return;
	}

	SDL_SetColorKey(icon, SDL_SRCCOLORKEY, 0);

	transColor.r = 255;
	transColor.g = 255;
	transColor.b = 255;

	solidColor.r = 0;
	solidColor.g = 0;
	solidColor.b = 0;

	SDL_SetColors(icon, &transColor, 0, 1);
	SDL_SetColors(icon, &solidColor, 1, 1);

	ptr = (Uint8 *)icon->pixels;

	for (i = 0; i < sizeof(q2icon_bits); i++)
	{
		for (mask = 1; mask != 0x100; mask <<= 1)
		{
			*ptr = (q2icon_bits[i] & mask) ? 1 : 0;
			ptr++;
		}
	}

	SDL_WM_SetIcon(icon, NULL);

	SDL_FreeSurface(icon);
}
#endif /* SDL 1.2 */

static int IsFullscreen()
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
		return 1;
	} else if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
		return 2;
	} else {
		return 0;
	}
#else
	return !!(window->flags & SDL_FULLSCREEN);
#endif
}

static qboolean GetWindowSize(int* w, int* h)
{
	if(window == NULL || w == NULL || h == NULL)
		return false;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_DisplayMode m;
	if(SDL_GetWindowDisplayMode(window, &m) != 0)
	{
		Com_Printf("Can't get Displaymode: %s\n", SDL_GetError());
		return false;
	}
	*w = m.w;
	*h = m.h;
#else
	*w = window->w;
	*h = window->h;
#endif

	return true;
}

int R_InitContext(void* win)
{
	char title[40] = {0};

	if(win == NULL)
	{
		ri.Sys_Error(ERR_FATAL, "R_InitContext() must not be called with NULL argument!");
		return false;
	}

#if SDL_VERSION_ATLEAST(2, 0, 0)
	window = (SDL_Window*)win;
#else // SDL 1.2
	window = (SDL_Surface*)win;
#endif

	/* Window title - set here so we can display renderer name in it */
	snprintf(title, sizeof(title), "Yamagi Quake II %s - Soft Render", YQ2VERSION);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_SetWindowTitle(window, title);
#else
	SDL_WM_SetCaption(title, title);
#endif

	return true;
}

static qboolean CreateSDLWindow(int flags, int w, int h)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	Uint32 Rmask, Gmask, Bmask, Amask;
	int bpp;
	int windowPos = SDL_WINDOWPOS_UNDEFINED;
	if (!SDL_PixelFormatEnumToMasks(SDL_PIXELFORMAT_ARGB8888, &bpp, &Rmask, &Gmask, &Bmask, &Amask))
		return 0;

	// TODO: support fullscreen on different displays with SDL_WINDOWPOS_UNDEFINED_DISPLAY(displaynum)
	window = SDL_CreateWindow("Yamagi Quake II", windowPos, windowPos, w, h, flags);

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	surface = SDL_CreateRGBSurface(0, w, h, bpp, Rmask, Gmask, Bmask, Amask);

	texture = SDL_CreateTexture(renderer,
								   SDL_PIXELFORMAT_ARGB8888,
								   SDL_TEXTUREACCESS_STREAMING,
								   w, h);
	return window != NULL;
#else
	window = SDL_SetVideoMode(w, h, 0, flags);
	SDL_EnableUNICODE(SDL_TRUE);
	return window != NULL;
#endif
}

static void SWimp_DestroyRender(void)
{
	if (vid_buffer)
	{
		free(vid_buffer);
	}
	vid_buffer = NULL;

	if (sintable)
	{
		free(sintable);
	}
	sintable = NULL;

	if (intsintable)
	{
		free(intsintable);
	}
	intsintable = NULL;

	if (blanktable)
	{
		free(blanktable);
	}
	blanktable = NULL;

	if (vid_polygon_spans)
	{
		free(vid_polygon_spans);
	}
	vid_polygon_spans = NULL;

	if (newedges)
	{
		free(newedges);
	}
	newedges = NULL;

	if (removeedges)
	{
		free(removeedges);
	}
	removeedges = NULL;

	if (triangle_spans)
	{
		free(triangle_spans);
	}
	triangle_spans = NULL;

	if (warp_rowptr)
	{
		free(warp_rowptr);
	}
	warp_rowptr = NULL;

	if (warp_column)
	{
		free(warp_column);
	}
	warp_column = NULL;

	if (edge_basespans)
	{
		free(edge_basespans);
	}
	edge_basespans = NULL;

	if (finalverts)
	{
		free(finalverts);
	}
	finalverts = NULL;

	if(ledges)
	{
		free(ledges);
	}
	ledges = NULL;

	if(lsurfs)
	{
		free(lsurfs);
	}
	lsurfs = NULL;

	if(r_warpbuffer)
	{
		free(r_warpbuffer);
	}
	r_warpbuffer = NULL;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (texture)
	{
		SDL_DestroyTexture(texture);
	}
	texture = NULL;

	if (surface)
	{
		SDL_FreeSurface(surface);
	}
	surface = NULL;

	if (renderer)
	{
		SDL_DestroyRenderer(renderer);
	}
	renderer = NULL;

	/* Is the surface used? */
	if (window)
		SDL_DestroyWindow(window);
#else
	/* Is the surface used? */
	if (window)
		SDL_FreeSurface(window);
#endif
	window = NULL;
}

/*
be careful if you ever want to change width: 12.20 fixed
point math used in R_ScanEdges() overflows at width 2048 !!
*/
char shift_size;

/*
** SWimp_InitGraphics
**
** This initializes the software refresh's implementation specific
** graphics subsystem.  In the case of Windows it creates DIB or
** DDRAW surfaces.
**
** The necessary width and height parameters are grabbed from
** vid.width and vid.height.
*/
static qboolean SWimp_InitGraphics(qboolean fullscreen, int *pwidth, int *pheight)
{
	int flags;
	int curWidth, curHeight;
	int width = *pwidth;
	int height = *pheight;
	unsigned int fs_flag = 0;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (fullscreen == 1) {
		fs_flag = SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else if (fullscreen == 2) {
		fs_flag = SDL_WINDOW_FULLSCREEN;
	}
#else
	if (fullscreen) {
		fs_flag = SDL_FULLSCREEN;
	}
#endif

	if (GetWindowSize(&curWidth, &curHeight) && (curWidth == width) && (curHeight == height))
	{
		/* If we want fullscreen, but aren't */
		if (fullscreen != IsFullscreen())
		{
#if SDL_VERSION_ATLEAST(2, 0, 0)
			SDL_SetWindowFullscreen(window, fs_flag);
#else
			SDL_WM_ToggleFullScreen(window);
#endif

			ri.Cvar_SetValue("vid_fullscreen", fullscreen);
		}

		/* Are we now? */
		if (fullscreen == IsFullscreen())
		{
			return true;
		}
	}

	SWimp_DestroyRender();

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow (vid.width, vid.height);

#if !SDL_VERSION_ATLEAST(2, 0, 0)
	/* Set window icon - For SDL1.2, this must be done before creating the window */
	SetSDLIcon();
#endif

	flags = SDL_SWSURFACE;
	if (fs_flag)
	{
		flags |= fs_flag;
	}

	while (1)
	{
		if (!CreateSDLWindow(flags, width, height))
		{
			Sys_Error("(SOFTSDL) SDL SetVideoMode failed: %s\n", SDL_GetError());
			return false;
		}
		else
		{
			break;
		}
	}

	if(!R_InitContext(window))
	{
		// InitContext() should have logged an error
		return false;
	}

	/* Note: window title is now set in re.InitContext() to include renderer name */
#if SDL_VERSION_ATLEAST(2, 0, 0)
	/* Set the window icon - For SDL2, this must be done after creating the window */
	SetSDLIcon();
#endif

	/* No cursor */
	SDL_ShowCursor(0);

	vid_buffer = malloc(vid.height * vid.width * sizeof(pixel_t));

	sintable = malloc((vid.width+CYCLE) * sizeof(int));
	intsintable = malloc((vid.width+CYCLE) * sizeof(int));
	blanktable = malloc((vid.width+CYCLE) * sizeof(int));

	newedges = malloc(vid.width * sizeof(edge_t *));
	removeedges = malloc(vid.width * sizeof(edge_t *));

	// 1 extra for spanpackage that marks end
	triangle_spans = malloc((vid.width + 1) * sizeof(spanpackage_t));

	warp_rowptr = malloc((vid.width+AMP2*2) * sizeof(byte*));
	warp_column = malloc((vid.width+AMP2*2) * sizeof(int));

	edge_basespans = malloc((vid.width*2) * sizeof(espan_t));
	finalverts = malloc((MAXALIASVERTS + 3) * sizeof(finalvert_t));
	ledges = malloc((NUMSTACKEDGES + 1) * sizeof(edge_t));
	lsurfs = malloc((NUMSTACKSURFACES + 1) * sizeof(surf_t));
	r_warpbuffer = malloc(WARP_WIDTH * WARP_HEIGHT * sizeof(pixel_t));

	if ((vid.width >= 2048) && (sizeof(shift20_t) == 4)) // 2k+ resolution and 32 == shift20_t
	{
		shift_size = 18;
	}
	else
	{
		shift_size = 20;
	}

	R_InitTurb ();

	vid_polygon_spans = malloc(sizeof(espan_t) * (vid.height + 1));

	memset(sw_state.currentpalette, 0, sizeof(sw_state.currentpalette));

	X11_active = true;

	return true;
}

/*
** RE_EndFrame
**
** This does an implementation specific copy from the backbuffer to the
** front buffer.  In the Win32 case it uses BitBlt or BltFast depending
** on whether we're using DIB sections/GDI or DDRAW.
*/

void RE_EndFrame (void)
{
	int y,x, i;
	const unsigned char *pallete = sw_state.currentpalette;
	Uint32 pallete_colors[256];

	for(i=0; i < 256; i++)
	{
#if SDL_VERSION_ATLEAST(2, 0, 0)
		pallete_colors[i] = SDL_MapRGB(surface->format,
					       pallete[i * 4 + 0], // red
					       pallete[i * 4 + 1], // green
					       pallete[i * 4 + 2]  //blue
					);
#else
		pallete_colors[i] = SDL_MapRGB(window->format,
					       pallete[i * 4 + 0], // red
					       pallete[i * 4 + 1], // green
					       pallete[i * 4 + 2]  //blue
					);
#endif
	}

#if SDL_VERSION_ATLEAST(2, 0, 0)
	Uint32 * pixels = (Uint32 *)surface->pixels;
#else
	Uint32 * pixels = (Uint32 *)window->pixels;
#endif
	for (y=0; y < vid.height;  y++)
	{
		for (x=0; x < vid.width; x ++)
		{
			int buffer_pos = y * vid.width + x;
			Uint32 color = pallete_colors[vid_buffer[buffer_pos]];
#if SDL_VERSION_ATLEAST(2, 0, 0)
			pixels[y * surface->pitch / sizeof(Uint32) + x] = color;
#else
			pixels[y * window->pitch / sizeof(Uint32) + x] = color;
#endif
		}
	}

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
#else
	/* SDL_Flip(window); */
	SDL_UpdateRect(window, 0, 0, 0, 0);
#endif
}

/*
** SWimp_SetMode
*/
rserr_t SWimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	rserr_t retval = rserr_ok;

	R_Printf (PRINT_ALL, "setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( pwidth, pheight, mode ) )
	{
		R_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	R_Printf( PRINT_ALL, " %d %d\n", *pwidth, *pheight);

	if ( !SWimp_InitGraphics(fullscreen, pwidth, pheight) ) {
		// failed to set a valid mode in windowed mode
		return rserr_invalid_mode;
	}

	R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_8to24table );

	return retval;
}

/*
** SWimp_Shutdown
**
** System specific graphics subsystem shutdown routine.  Destroys
** DIBs or DDRAW surfaces as appropriate.
*/

void SWimp_Shutdown( void )
{
	SWimp_DestroyRender();

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);

	X11_active = false;
}

// this is only here so the functions in q_shared.c and q_shwin.c can link
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	ri.Sys_Error (ERR_FATAL, "%s", text);
}

void Com_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	R_Printf(PRINT_ALL, "%s", text);
}

/*
==============================================================================

SCREEN SHOTS

==============================================================================
*/

/*
==================
R_ScreenShot_f
==================
*/
void
R_ScreenShot_f(void)
{
	int x, y;
	byte *buffer = malloc(vid.width * vid.height * 3);
	const unsigned char *pallete = sw_state.currentpalette;

	if (!buffer)
	{
		R_Printf(PRINT_ALL, "R_ScreenShot: Couldn't malloc %d bytes\n", vid.width * vid.height * 3);
		return;
	}

	for (x=0; x < vid.width; x ++)
	{
		for (y=0; y < vid.height; y ++) {
			int buffer_pos = y * vid.width + x;
			buffer[buffer_pos * 3 + 0] = pallete[vid_buffer[buffer_pos] * 4 + 0]; // red
			buffer[buffer_pos * 3 + 1] = pallete[vid_buffer[buffer_pos] * 4 + 1]; // green
			buffer[buffer_pos * 3 + 2] = pallete[vid_buffer[buffer_pos] * 4 + 2]; // blue
		}
	}

	ri.Vid_WriteScreenshot(vid.width, vid.height, 3, buffer);

	free(buffer);
}
