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
// sw_edge.c
#include <limits.h>
#include <stdint.h>
#include "header/local.h"
/*
the complex cases add new polys on most lines, so dont optimize for keeping them the same
have multiple free span lists to try to get better coherence?
low depth complexity -- 1 to 3 or so

have a sentinal at both ends?
*/

// surfaces are generated in back to front order by the bsp, so if a surf
// pointer is greater than another one, it should be drawn in front
// surfaces[1] is the background, and is used as the active surface stack

edge_t	**newedges;
edge_t	**removeedges;
espan_t	*edge_basespans;

static espan_t	*span_p, *max_span_p;

int	r_currentkey;

static shift20_t	current_iv;
static shift20_t	edge_head_u_shift20, edge_tail_u_shift20;

static void (*pdrawfunc)(void);

static edge_t	edge_head;
static edge_t	edge_tail;
static edge_t	edge_aftertail;
static edge_t	edge_sentinel;
static float	fv;
static int	miplevel;

float	scale_for_mip;

static void R_GenerateSpans (void);
static void R_GenerateSpansBackward (void);

static void R_TrailingEdge (surf_t *surf, edge_t *edge);

/*
===============================================================================

EDGE SCANNING

===============================================================================
*/

/*
==============
R_BeginEdgeFrame
==============
*/
void
R_BeginEdgeFrame (void)
{
	surfaces[1].spans = NULL;	// no background spans yet
	surfaces[1].flags = SURF_DRAWBACKGROUND;

	// put the background behind everything in the world
	if (sw_draworder->value)
	{
		pdrawfunc = R_GenerateSpansBackward;
		surfaces[1].key = 0;
		r_currentkey = 1;
	}
	else
	{
		pdrawfunc = R_GenerateSpans;
		surfaces[1].key = 0x7FFfFFFF;
		r_currentkey = 0;
	}

	if ((r_refdef.vrectbottom - r_refdef.vrect.y) > 0)
	{
		memset(newedges + r_refdef.vrect.y, 0,
			(r_refdef.vrectbottom - r_refdef.vrect.y) * sizeof(edge_t*));
		memset(removeedges + r_refdef.vrect.y, 0,
			(r_refdef.vrectbottom - r_refdef.vrect.y) * sizeof(edge_t*));
	}
}

/*
==============
R_InsertNewEdges

Adds the edges in the linked list edgestoadd, adding them to the edges in the
linked list edgelist.  edgestoadd is assumed to be sorted on u, and non-empty (this is actually newedges[v]).  edgelist is assumed to be sorted on u, with a
sentinel at the end (actually, this is the active edge table starting at
edge_head.next).
==============
*/
static void
R_InsertNewEdges (edge_t *edgestoadd, edge_t *edgelist)
{
	edge_t	*next_edge;

	do
	{
		next_edge = edgestoadd->next;

		while (edgelist->u < edgestoadd->u)
		{
			edgelist = edgelist->next;
		}

		// insert edgestoadd before edgelist
		edgestoadd->next = edgelist;
		edgestoadd->prev = edgelist->prev;
		edgelist->prev->next = edgestoadd;
		edgelist->prev = edgestoadd;
	} while ((edgestoadd = next_edge) != NULL);
}

/*
==============
R_RemoveEdges
==============
*/
static void
R_RemoveEdges (edge_t *pedge)
{

	do
	{
		pedge->next->prev = pedge->prev;
		pedge->prev->next = pedge->next;
	} while ((pedge = pedge->nextremove) != NULL);
}

/*
==============
R_StepActiveU
==============
*/
static void
R_StepActiveU (edge_t *pedge)
{
	edge_t	*pnext_edge, *pwedge;

	while (1)
	{
		pedge->u += pedge->u_step;
		if (pedge->u < pedge->prev->u)
		{
			if (pedge == &edge_aftertail)
				return;

			if (pedge->prev == &edge_head)
			{
				R_Printf(PRINT_ALL,"Already in head.\n");
			}

			// push it back to keep it sorted
			pnext_edge = pedge->next;

			// pull the edge out of the edge list
			pedge->next->prev = pedge->prev;
			pedge->prev->next = pedge->next;

			// find out where the edge goes in the edge list
			pwedge = pedge->prev->prev;

			while (pwedge->u > pedge->u)
			{
				pwedge = pwedge->prev;
			}

			// put the edge back into the edge list
			pedge->next = pwedge->next;
			pedge->prev = pwedge;
			pedge->next->prev = pedge;
			pwedge->next = pedge;

			pedge = pnext_edge;
			if (pedge == &edge_tail)
				return;
		}
		else
		{
			pedge = pedge->next;
		}
	}
}

/*
==============
R_CleanupSpan
==============
*/
static void
R_CleanupSpan (void)
{
	surf_t		*surf;
	shift20_t	iu;
	espan_t		*span;

	// now that we've reached the right edge of the screen, we're done with any
	// unfinished surfaces, so emit a span for whatever's on top
	surf = surfaces[1].next;
	iu = edge_tail_u_shift20;
	if (iu > surf->last_u)
	{
		span = span_p++;
		span->u = surf->last_u;
		span->count = iu - span->u;
		span->v = current_iv;
		span->pnext = surf->spans;
		surf->spans = span;
	}

	// reset spanstate for all surfaces in the surface stack
	do
	{
		surf->spanstate = 0;
		surf = surf->next;
	} while (surf != &surfaces[1]);
}


/*
==============
D_SurfSearchBackwards
==============
*/
static surf_t*
D_SurfSearchBackwards(const surf_t *surf, surf_t *surf2)
{
	do
	{
		do
		{
			surf2 = surf2->next;
		} while (surf->key < surf2->key);

		// if it's two surfaces on the same plane, the one that's already
		// active is in front, so keep going unless it's a bmodel
		// must be two bmodels in the same leaf; don't care which is really
		// in front, because they'll never be farthest anyway
	} while (surf->key == surf2->key && !surf->insubmodel);

	return surf2;
}


/*
==============
R_LeadingEdgeBackwards
==============
*/
static void
R_LeadingEdgeBackwards (const edge_t *edge)
{
	surf_t		*surf, *surf2;

	// it's adding a new surface in, so find the correct place
	surf = &surfaces[edge->surfs[1]];

	// don't start a span if this is an inverted span, with the end
	// edge preceding the start edge (that is, we've already seen the
	// end edge)
	if (++surf->spanstate == 1)
	{
		surf2 = surfaces[1].next;

		// if it's two surfaces on the same plane, the one that's already
		// active is in front, so keep going unless it's a bmodel
		// must be two bmodels in the same leaf; don't care, because they'll
		// never be farthest anyway
		if (surf->key > surf2->key || (surf->insubmodel && (surf->key == surf2->key))) {
			shift20_t	iu;

			// emit a span (obscures current top)
			iu = edge->u >> shift_size;

			if (iu > surf2->last_u)
			{
				espan_t	*span;

				span = span_p++;
				span->u = surf2->last_u;
				span->count = iu - span->u;
				span->v = current_iv;
				span->pnext = surf2->spans;
				surf2->spans = span;
			}

			// set last_u on the new span
			surf->last_u = iu;
		}
		else
		{
			surf2 = D_SurfSearchBackwards(surf, surf2);
		}

		// insert before surf2
		surf->next = surf2;
		surf->prev = surf2->prev;
		surf2->prev->next = surf;
		surf2->prev = surf;
	}
}


/*
==============
R_TrailingEdge
==============
*/
static void
R_TrailingEdge (surf_t *surf, edge_t *edge)
{
	espan_t *span;

	// don't generate a span if this is an inverted span, with the end
	// edge preceding the start edge (that is, we haven't seen the
	// start edge yet)
	if (--surf->spanstate == 0)
	{
		if (surf == surfaces[1].next)
		{
			shift20_t iu;

			// emit a span (current top going away)
			iu = edge->u >> shift_size;
			if (iu > surf->last_u)
			{
				span = span_p++;
				span->u = surf->last_u;
				span->count = iu - span->u;
				span->v = current_iv;
				span->pnext = surf->spans;
				surf->spans = span;
			}

			// set last_u on the surface below
			surf->next->last_u = iu;
		}

		surf->prev->next = surf->next;
		surf->next->prev = surf->prev;
	}
}

/*
==============
D_SurfSearchForward
==============
*/
static surf_t*
D_SurfSearchForward(const surf_t *surf, surf_t *surf2)
{
	do
	{
		do
		{
			surf2 = surf2->next;
		} while (surf->key > surf2->key);
		// if it's two surfaces on the same plane, the one that's already
		// active is in front, so keep going unless it's a bmodel
	} while (surf->key == surf2->key && !surf->insubmodel);

	return surf2;
}

/*
==============
R_LeadingEdgeSearch
==============
*/
static surf_t*
R_LeadingEdgeSearch (const edge_t *edge, const surf_t *surf, surf_t *surf2)
{
	float	testzi, newzitop;

	do
	{
		float	fu, newzi, newzibottom;

		surf2 = D_SurfSearchForward(surf, surf2);

		if (surf->key != surf2->key)
			return surf2;

		// must be two bmodels in the same leaf; sort on 1/z
		fu = (float)(edge->u - (1<<shift_size) + 1) * (1.0 / (1<<shift_size));
		newzi = surf->d_ziorigin + fv*surf->d_zistepv +
				fu*surf->d_zistepu;
		newzibottom = newzi * 0.99;

		testzi = surf2->d_ziorigin + fv*surf2->d_zistepv +
				fu*surf2->d_zistepu;

		if (newzibottom >= testzi)
		{
			return surf2;
		}

		newzitop = newzi * 1.01;
	}
	while(newzitop < testzi || surf->d_zistepu < surf2->d_zistepu);

	return surf2;
}

/*
==============
R_InsertBeforeSurf
==============
*/
static void
R_InsertBeforeSurf(surf_t *surf, surf_t *surf2)
{
	surf->next = surf2;
	surf->prev = surf2->prev;
	surf2->prev->next = surf;
	surf2->prev = surf;
}

/*
==============
R_EmitSpanBeforeTop
==============
*/
static void
R_EmitSpanBeforeTop(const edge_t *edge, surf_t *surf, surf_t *surf2)
{
	shift20_t	iu;

	iu = edge->u >> shift_size;

	if (iu > surf2->last_u)
	{
		espan_t	*span;

		span = span_p++;
		span->u = surf2->last_u;
		span->count = iu - span->u;
		span->v = current_iv;
		span->pnext = surf2->spans;
		surf2->spans = span;
	}

	// set last_u on the new span
	surf->last_u = iu;

	// insert before surf2
	R_InsertBeforeSurf(surf, surf2);
}

/*
==============
R_LeadingEdge
==============
*/
static void
R_LeadingEdge (const edge_t *edge)
{
	if (edge->surfs[1])
	{
		surf_t		*surf;

		// it's adding a new surface in, so find the correct place
		surf = &surfaces[edge->surfs[1]];

		// don't start a span if this is an inverted span, with the end
		// edge preceding the start edge (that is, we've already seen the
		// end edge)
		if (++surf->spanstate == 1)
		{
			surf_t		*surf2;

			surf2 = surfaces[1].next;

			if (surf->key < surf2->key)
			{
				// emit a span (obscures current top)
				R_EmitSpanBeforeTop(edge, surf, surf2);
				return;
			}

			// if it's two surfaces on the same plane, the one that's already
			// active is in front, so keep going unless it's a bmodel
			if (surf->insubmodel && (surf->key == surf2->key))
			{
				float	fu, newzi, testzi, newzitop, newzibottom;

				// must be two bmodels in the same leaf; sort on 1/z
				fu = (float)(edge->u - (1<<shift_size) + 1) * (1.0 / (1<<shift_size));
				newzi = surf->d_ziorigin + fv*surf->d_zistepv +
						fu*surf->d_zistepu;
				newzibottom = newzi * 0.99;

				testzi = surf2->d_ziorigin + fv*surf2->d_zistepv +
						fu*surf2->d_zistepu;

				if (newzibottom >= testzi)
				{
					// emit a span (obscures current top)
					R_EmitSpanBeforeTop(edge, surf, surf2);
					return;
				}

				newzitop = newzi * 1.01;
				if (newzitop >= testzi)
				{
					if (surf->d_zistepu >= surf2->d_zistepu)
					{
						// emit a span (obscures current top)
						R_EmitSpanBeforeTop(edge, surf, surf2);
						return;
					}
				}
			}

			surf2 = R_LeadingEdgeSearch (edge, surf, surf2);
			// insert before surf2
			R_InsertBeforeSurf(surf, surf2);
		}
	}
}

/*
==============
R_GenerateSpans
==============
*/
static void
R_GenerateSpans (void)
{
	edge_t			*edge;
	surf_t			*surf;

	// clear active surfaces to just the background surface
	surfaces[1].next = surfaces[1].prev = &surfaces[1];
	surfaces[1].last_u = edge_head_u_shift20;

	// generate spans
	for (edge=edge_head.next ; edge != &edge_tail; edge=edge->next)
	{
		if (edge->surfs[0])
		{
			// it has a left surface, so a surface is going away for this span
			surf = &surfaces[edge->surfs[0]];

			R_TrailingEdge (surf, edge);

			if (!edge->surfs[1])
				continue;
		}

		R_LeadingEdge (edge);
	}

	R_CleanupSpan ();
}

/*
==============
R_GenerateSpansBackward
==============
*/
static void
R_GenerateSpansBackward (void)
{
	edge_t			*edge;

	// clear active surfaces to just the background surface
	surfaces[1].next = surfaces[1].prev = &surfaces[1];
	surfaces[1].last_u = edge_head_u_shift20;

	// generate spans
	for (edge=edge_head.next ; edge != &edge_tail; edge=edge->next)
	{
		if (edge->surfs[0])
			R_TrailingEdge (&surfaces[edge->surfs[0]], edge);

		if (edge->surfs[1])
			R_LeadingEdgeBackwards (edge);
	}

	R_CleanupSpan ();
}

static void D_DrawSurfaces (surf_t *surface);

/*
==============
R_ScanEdges

Input:
newedges[] array
	this has links to edges, which have links to surfaces

Output:
Each surface has a linked list of its visible spans

==============
*/
void
R_ScanEdges (surf_t *surface)
{
	shift20_t	iv, bottom;
	espan_t		*basespan_p;
	surf_t		*s;

	basespan_p = edge_basespans;
	max_span_p = edge_basespans + vid.width * 2 - r_refdef.vrect.width;
	if ((vid.width * 2 - r_refdef.vrect.width) < 0)
	{
		R_Printf(PRINT_ALL,"No space in edge_basespans\n");
		return;
	}

	span_p = basespan_p;

	// clear active edges to just the background edges around the whole screen
	// FIXME: most of this only needs to be set up once
	edge_head.u = r_refdef.vrect.x << shift_size;
	edge_head_u_shift20 = edge_head.u >> shift_size;
	edge_head.u_step = 0;
	edge_head.prev = NULL;
	edge_head.next = &edge_tail;
	edge_head.surfs[0] = 0;
	edge_head.surfs[1] = 1;

	edge_tail.u = (r_refdef.vrectright << shift_size) + (1 << shift_size) - 1;
	edge_tail_u_shift20 = edge_tail.u >> shift_size;
	edge_tail.u_step = 0;
	edge_tail.prev = &edge_head;
	edge_tail.next = &edge_aftertail;
	edge_tail.surfs[0] = 1;
	edge_tail.surfs[1] = 0;

	edge_aftertail.u = -1;	// force a move
	edge_aftertail.u_step = 0;
	edge_aftertail.next = &edge_sentinel;
	edge_aftertail.prev = &edge_tail;

	// FIXME: do we need this now that we clamp x in r_draw.c?
	edge_sentinel.u = INT_MAX; // make sure nothing sorts past this
	edge_sentinel.prev = &edge_aftertail;

	//
	// process all scan lines
	//
	bottom = r_refdef.vrectbottom - 1;

	for (iv=r_refdef.vrect.y ; iv<bottom ; iv++)
	{
		current_iv = iv;
		fv = (float)iv;

		// mark that the head (background start) span is pre-included
		surfaces[1].spanstate = 1;

		if (newedges[iv])
		{
			// Update AET with GET event
			R_InsertNewEdges (newedges[iv], edge_head.next);
		}

		// Generate spans
		(*pdrawfunc) ();

		// flush the span list if we can't be sure we have enough spans left for
		// the next scan
		if (span_p >= max_span_p)
		{
			// Draw stuff on screen
			D_DrawSurfaces (surface);

			// clear the surface span pointers
			for (s = &surfaces[1] ; s<surface ; s++)
				s->spans = NULL;

			span_p = basespan_p;
		}

		if (removeedges[iv])
			R_RemoveEdges (removeedges[iv]);

		if (edge_head.next != &edge_tail)
			R_StepActiveU (edge_head.next);
	}

	// do the last scan (no need to step or sort or remove on the last scan)
	current_iv = iv;
	fv = (float)iv;

	// mark that the head (background start) span is pre-included
	surfaces[1].spanstate = 1;

	// Flush span buffer
	if (newedges[iv])
		// Update AET with GET event
		R_InsertNewEdges (newedges[iv], edge_head.next);

	// Update AET with GET event
	(*pdrawfunc) ();

	// draw whatever's left in the span list
	D_DrawSurfaces (surface);
}


/*
=========================================================================

SURFACE FILLING

=========================================================================
*/

static msurface_t		*pface;
static surfcache_t		*pcurrentcache;
static vec3_t			transformed_modelorg;
static vec3_t			world_transformed_modelorg;

/*
=============
D_MipLevelForScale
=============
*/
static int
D_MipLevelForScale (float scale)
{
	int	lmiplevel = NUM_MIPS-1, i;

	for (i=0; i < NUM_MIPS-1; i ++)
	{
		if (scale >= d_scalemip[i])
		{
			lmiplevel = i;
			break;
		}
	}

	if (lmiplevel < d_minmip)
		lmiplevel = d_minmip;

	return lmiplevel;
}


/*
==============
D_FlatFillSurface

Simple single color fill with no texture mapping
==============
*/
static void
D_FlatFillSurface (surf_t *surf, pixel_t color)
{
	espan_t	*span;

	for (span=surf->spans ; span ; span=span->pnext)
	{
		pixel_t   *pdest;

		pdest = d_viewbuffer + vid.width*span->v + span->u;
		memset(pdest,  color&0xFF, span->count * sizeof(pixel_t));
	}
}


/*
==============
D_CalcGradients
==============
*/
static void
D_CalcGradients (msurface_t *pface)
{
	float		mipscale;
	vec3_t		p_temp1;
	vec3_t		p_saxis, p_taxis;
	float		t;

	mipscale = 1.0 / (float)(1 << miplevel);

	TransformVector (pface->texinfo->vecs[0], p_saxis);
	TransformVector (pface->texinfo->vecs[1], p_taxis);

	t = xscaleinv * mipscale;
	d_sdivzstepu = p_saxis[0] * t;
	d_tdivzstepu = p_taxis[0] * t;

	t = yscaleinv * mipscale;
	d_sdivzstepv = -p_saxis[1] * t;
	d_tdivzstepv = -p_taxis[1] * t;

	d_sdivzorigin = p_saxis[2] * mipscale - xcenter * d_sdivzstepu -
			ycenter * d_sdivzstepv;
	d_tdivzorigin = p_taxis[2] * mipscale - xcenter * d_tdivzstepu -
			ycenter * d_tdivzstepv;

	VectorScale (transformed_modelorg, mipscale, p_temp1);

	t = SHIFT16XYZ_MULT * mipscale;
	sadjust = ((int)(DotProduct (p_temp1, p_saxis) * SHIFT16XYZ_MULT + 0.5)) -
			((pface->texturemins[0] << SHIFT16XYZ) >> miplevel)
			+ pface->texinfo->vecs[0][3]*t;
	tadjust = ((int)(DotProduct (p_temp1, p_taxis) * SHIFT16XYZ_MULT + 0.5)) -
			((pface->texturemins[1] << SHIFT16XYZ) >> miplevel)
			+ pface->texinfo->vecs[1][3]*t;

	// PGM - changing flow speed for non-warping textures.
	if (pface->texinfo->flags & SURF_FLOWING)
	{
		if(pface->texinfo->flags & SURF_WARP)
			sadjust += SHIFT16XYZ_MULT * (-128 * ( (r_newrefdef.time * 0.25) - (int)(r_newrefdef.time * 0.25) ));
		else
			sadjust += SHIFT16XYZ_MULT * (-128 * ( (r_newrefdef.time * 0.77) - (int)(r_newrefdef.time * 0.77) ));
	}
	// PGM

	//
	// -1 (-epsilon) so we never wander off the edge of the texture
	//
	bbextents = ((pface->extents[0] << SHIFT16XYZ) >> miplevel) - 1;
	bbextentt = ((pface->extents[1] << SHIFT16XYZ) >> miplevel) - 1;
}


/*
==============
D_BackgroundSurf

The grey background filler seen when there is a hole in the map
==============
*/
static void
D_BackgroundSurf (surf_t *s)
{
	// set up a gradient for the background surface that places it
	// effectively at infinity distance from the viewpoint
	d_zistepu = 0;
	d_zistepv = 0;
	d_ziorigin = -0.9;

	D_FlatFillSurface (s, (int)sw_clearcolor->value & 0xFF);
	D_DrawZSpans (s->spans);
}

/*
=================
D_TurbulentSurf
=================
*/
static void
D_TurbulentSurf(surf_t *s)
{
	d_zistepu = s->d_zistepu;
	d_zistepv = s->d_zistepv;
	d_ziorigin = s->d_ziorigin;

	pface = s->msurf;
	miplevel = 0;
	cacheblock = pface->texinfo->image->pixels[0];
	cachewidth = 64;

	if (s->insubmodel)
	{
		entity_t *currententity;
		vec3_t local_modelorg;

		// FIXME: we don't want to do all this for every polygon!
		// TODO: store once at start of frame
		currententity = s->entity;
		VectorSubtract (r_origin, currententity->origin,
				local_modelorg);
		TransformVector (local_modelorg, transformed_modelorg);

		R_RotateBmodel(currententity);	// FIXME: don't mess with the frustum,
						// make entity passed in
	}

	D_CalcGradients (pface);

	//============
	//PGM
	// textures that aren't warping are just flowing. Use NonTurbulentPow2 instead
	if(!(pface->texinfo->flags & SURF_WARP))
		NonTurbulentPow2 (s->spans);
	else
		TurbulentPow2 (s->spans);
	//PGM
	//============

	D_DrawZSpans (s->spans);

	if (s->insubmodel)
	{
		//
		// restore the old drawing state
		// FIXME: we don't want to do this every time!
		// TODO: speed up
		//
		VectorCopy (world_transformed_modelorg,
					transformed_modelorg);
		VectorCopy (base_vpn, vpn);
		VectorCopy (base_vup, vup);
		VectorCopy (base_vright, vright);
		R_TransformFrustum ();
	}
}

/*
==============
D_SkySurf
==============
*/
static void
D_SkySurf (surf_t *s)
{
	pface = s->msurf;
	miplevel = 0;
	if (!pface->texinfo->image)
		return;
	cacheblock = pface->texinfo->image->pixels[0];
	cachewidth = 256;

	d_zistepu = s->d_zistepu;
	d_zistepv = s->d_zistepv;
	d_ziorigin = s->d_ziorigin;

	D_CalcGradients (pface);

	D_DrawSpansPow2 (s->spans);

	// set up a gradient for the background surface that places it
	// effectively at infinity distance from the viewpoint
	d_zistepu = 0;
	d_zistepv = 0;
	d_ziorigin = -0.9;

	D_DrawZSpans (s->spans);
}

/*
==============
D_SolidSurf

Normal surface cached, texture mapped surface
==============
*/
static void
D_SolidSurf (surf_t *s)
{
	entity_t *currententity;

	d_zistepu = s->d_zistepu;
	d_zistepv = s->d_zistepv;
	d_ziorigin = s->d_ziorigin;

	if (s->insubmodel)
	{
		vec3_t local_modelorg;

		// FIXME: we don't want to do all this for every polygon!
		// TODO: store once at start of frame
		currententity = s->entity;
		VectorSubtract(r_origin, currententity->origin, local_modelorg);
		TransformVector(local_modelorg, transformed_modelorg);

		R_RotateBmodel(currententity);	// FIXME: don't mess with the frustum,
					// make entity passed in
	}
	else
		currententity = &r_worldentity;

	pface = s->msurf;
	miplevel = D_MipLevelForScale(s->nearzi * scale_for_mip * pface->texinfo->mipadjust);

	// FIXME: make this passed in to D_CacheSurface
	pcurrentcache = D_CacheSurface (currententity, pface, miplevel);

	cacheblock = (pixel_t *)pcurrentcache->data;
	cachewidth = pcurrentcache->width;

	D_CalcGradients (pface);

	D_DrawSpansPow2 (s->spans);

	D_DrawZSpans (s->spans);

	if (s->insubmodel)
	{
		//
		// restore the old drawing state
		// FIXME: we don't want to do this every time!
		// TODO: speed up
		//
		VectorCopy (world_transformed_modelorg,
					transformed_modelorg);
		VectorCopy (base_vpn, vpn);
		VectorCopy (base_vup, vup);
		VectorCopy (base_vright, vright);
		R_TransformFrustum ();
	}
}

/*
=============
D_DrawflatSurfaces

To allow developers to see the polygon carving of the world
=============
*/
static void
D_DrawflatSurfaces (surf_t *surface)
{
	surf_t			*s;
	int color = 0;

	for (s = &surfaces[1] ; s<surface ; s++)
	{
		if (!s->spans)
			continue;

		d_zistepu = s->d_zistepu;
		d_zistepv = s->d_zistepv;
		d_ziorigin = s->d_ziorigin;

		// make a stable color for each surface by taking the low
		// bits of the msurface pointer
		D_FlatFillSurface (s, color & 0xFF);
		D_DrawZSpans (s->spans);

		color ++;
	}
}

/*
==============
D_DrawSurfaces

Rasterize all the span lists.  Guaranteed zero overdraw.
May be called more than once a frame if the surf list overflows (higher res)
==============
*/
static void
D_DrawSurfaces (surf_t *surface)
{
	VectorSubtract (r_origin, vec3_origin, modelorg);
	TransformVector (modelorg, transformed_modelorg);
	VectorCopy (transformed_modelorg, world_transformed_modelorg);

	if (!sw_drawflat->value)
	{
		surf_t *s;

		for (s = &surfaces[1] ; s<surface ; s++)
		{
			if (!s->spans)
				continue;

			r_drawnpolycount++;

			if (! (s->flags & (SURF_DRAWSKYBOX|SURF_DRAWBACKGROUND|SURF_DRAWTURB) ) )
				D_SolidSurf (s);
			else if (s->flags & SURF_DRAWSKYBOX)
				D_SkySurf (s);
			else if (s->flags & SURF_DRAWBACKGROUND)
				D_BackgroundSurf (s);
			else if (s->flags & SURF_DRAWTURB)
				D_TurbulentSurf (s);
		}
	}
	else
		D_DrawflatSurfaces (surface);

	VectorSubtract (r_origin, vec3_origin, modelorg);
	R_TransformFrustum ();
}
