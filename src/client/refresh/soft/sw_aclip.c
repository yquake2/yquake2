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
// sw_aclip.c: clip routines for drawing Alias models directly to the screen

#include "header/local.h"

void R_AliasProjectAndClipTestFinalVert (finalvert_t *fv);

/*
================
R_Alias_clip_z

pfv0 is the unclipped vertex, pfv1 is the z-clipped vertex
================
*/
static void
R_Alias_clip_z (const finalvert_t *pfv0, const finalvert_t *pfv1, finalvert_t *out)
{
	float scale;
	int i;

	scale = (ALIAS_Z_CLIP_PLANE - pfv0->xyz[2]) /
			(pfv1->xyz[2] - pfv0->xyz[2]);

	out->xyz[0] = pfv0->xyz[0] + (pfv1->xyz[0] - pfv0->xyz[0]) * scale;
	out->xyz[1] = pfv0->xyz[1] + (pfv1->xyz[1] - pfv0->xyz[1]) * scale;
	out->xyz[2] = ALIAS_Z_CLIP_PLANE;

	out->cv.s =	pfv0->cv.s + (pfv1->cv.s - pfv0->cv.s) * scale;
	out->cv.t =	pfv0->cv.t + (pfv1->cv.t - pfv0->cv.t) * scale;

	for(i=0; i<3; i++)
		out->cv.l[i] =	pfv0->cv.l[i] + (pfv1->cv.l[i] - pfv0->cv.l[i]) * scale;

	R_AliasProjectAndClipTestFinalVert (out);
}

static void
R_Alias_clip_scale (const finalvert_t *pfv0, const finalvert_t *pfv1, float scale, finalvert_t *out)
{
	int i;

	out->cv.u  = pfv1->cv.u  + ( pfv0->cv.u  - pfv1->cv.u ) * scale + 0.5;
	out->cv.v  = pfv1->cv.v  + ( pfv0->cv.v  - pfv1->cv.v ) * scale + 0.5;
	out->cv.s  = pfv1->cv.s  + ( pfv0->cv.s  - pfv1->cv.s ) * scale + 0.5;
	out->cv.t  = pfv1->cv.t  + ( pfv0->cv.t  - pfv1->cv.t ) * scale + 0.5;

	for(i=0; i<3; i++)
		out->cv.l[i]  = pfv1->cv.l[i]  + ( pfv0->cv.l[i]  - pfv1->cv.l[i] ) * scale + 0.5;

	out->cv.zi = pfv1->cv.zi + ( pfv0->cv.zi - pfv1->cv.zi) * scale + 0.5;
}

static void
R_Alias_clip_left (const finalvert_t *pfv0, const finalvert_t *pfv1, finalvert_t *out)
{
	float		scale;

	if (pfv0->cv.v >= pfv1->cv.v )
	{
		scale = (float)(r_refdef.aliasvrect.x - pfv0->cv.u) /
				(pfv1->cv.u - pfv0->cv.u);

		R_Alias_clip_scale (pfv1, pfv0, scale, out);
	}
	else
	{
		scale = (float)(r_refdef.aliasvrect.x - pfv1->cv.u) /
				(pfv0->cv.u - pfv1->cv.u);

		R_Alias_clip_scale (pfv0, pfv1, scale, out);
	}
}

static void
R_Alias_clip_right (const finalvert_t *pfv0, const finalvert_t *pfv1, finalvert_t *out)
{
	float scale;

	if ( pfv0->cv.v >= pfv1->cv.v )
	{
		scale = (float)(r_refdef.aliasvrectright - pfv0->cv.u ) /
				(pfv1->cv.u - pfv0->cv.u );

		R_Alias_clip_scale (pfv1, pfv0, scale, out);
	}
	else
	{
		scale = (float)(r_refdef.aliasvrectright - pfv1->cv.u ) /
				(pfv0->cv.u - pfv1->cv.u );

		R_Alias_clip_scale (pfv0, pfv1, scale, out);
	}
}

static void
R_Alias_clip_top (const finalvert_t *pfv0, const finalvert_t *pfv1, finalvert_t *out)
{
	float		scale;

	if (pfv0->cv.v >= pfv1->cv.v)
	{
		scale = (float)(r_refdef.aliasvrect.y - pfv0->cv.v) /
				(pfv1->cv.v - pfv0->cv.v);

		R_Alias_clip_scale (pfv1, pfv0, scale, out);
	}
	else
	{
		scale = (float)(r_refdef.aliasvrect.y - pfv1->cv.v) /
				(pfv0->cv.v - pfv1->cv.v);

		R_Alias_clip_scale (pfv0, pfv1, scale, out);
	}
}


static void
R_Alias_clip_bottom (const finalvert_t *pfv0, const finalvert_t *pfv1, finalvert_t *out)
{
	float		scale;

	if (pfv0->cv.v >= pfv1->cv.v)
	{
		scale = (float)(r_refdef.aliasvrectbottom - pfv0->cv.v) /
				(pfv1->cv.v - pfv0->cv.v);

		R_Alias_clip_scale (pfv1, pfv0, scale, out);
	}
	else
	{
		scale = (float)(r_refdef.aliasvrectbottom - pfv1->cv.v) /
				(pfv0->cv.v - pfv1->cv.v);

		R_Alias_clip_scale (pfv0, pfv1, scale, out);
	}
}


int
R_AliasClip (const finalvert_t *in, finalvert_t *out, int flag, int count,
	void(*clip)(const finalvert_t *pfv0, const finalvert_t *pfv1, finalvert_t *out) )
{
	int			i,j,k;

	j = count-1;
	k = 0;
	for (i=0 ; i<count ; i++)
	{
		int flags, oldflags;

		oldflags = in[j].flags & flag;
		flags = in[i].flags & flag;

		if (flags && oldflags)
		{
			j = i;
			continue;
		}
		if (oldflags ^ flags)
		{
			clip (&in[j], &in[i], &out[k]);
			out[k].flags = 0;
			if (out[k].cv.u < r_refdef.aliasvrect.x)
				out[k].flags |= ALIAS_LEFT_CLIP;
			if (out[k].cv.v < r_refdef.aliasvrect.y)
				out[k].flags |= ALIAS_TOP_CLIP;
			if (out[k].cv.u > r_refdef.aliasvrectright)
				out[k].flags |= ALIAS_RIGHT_CLIP;
			if (out[k].cv.v > r_refdef.aliasvrectbottom)
				out[k].flags |= ALIAS_BOTTOM_CLIP;
			k++;
		}
		if (!flags)
		{
			out[k] = in[i];
			k++;
		}
		j = i;
	}

	return k;
}


/*
================
R_AliasClipTriangle
================
*/
void
R_AliasClipTriangle(const entity_t *currententity, const finalvert_t *index0, const finalvert_t *index1, const finalvert_t *index2)
{
	int				i, k, pingpong;
	unsigned		clipflags;
	finalvert_t		fv[2][8];

	// copy vertexes and fix seam texture coordinates
	fv[0][0] = *index0;
	fv[0][1] = *index1;
	fv[0][2] = *index2;

	// clip
	clipflags = fv[0][0].flags | fv[0][1].flags | fv[0][2].flags;

	if (clipflags & ALIAS_Z_CLIP)
	{
		k = R_AliasClip (fv[0], fv[1], ALIAS_Z_CLIP, 3, R_Alias_clip_z);
		if (k == 0)
			return;

		pingpong = 1;
		clipflags = fv[1][0].flags | fv[1][1].flags | fv[1][2].flags;
	}
	else
	{
		pingpong = 0;
		k = 3;
	}

	if (clipflags & ALIAS_LEFT_CLIP)
	{
		k = R_AliasClip (fv[pingpong], fv[pingpong ^ 1],
							ALIAS_LEFT_CLIP, k, R_Alias_clip_left);
		if (k == 0)
			return;

		pingpong ^= 1;
	}

	if (clipflags & ALIAS_RIGHT_CLIP)
	{
		k = R_AliasClip (fv[pingpong], fv[pingpong ^ 1],
							ALIAS_RIGHT_CLIP, k, R_Alias_clip_right);
		if (k == 0)
			return;

		pingpong ^= 1;
	}

	if (clipflags & ALIAS_BOTTOM_CLIP)
	{
		k = R_AliasClip (fv[pingpong], fv[pingpong ^ 1],
							ALIAS_BOTTOM_CLIP, k, R_Alias_clip_bottom);
		if (k == 0)
			return;

		pingpong ^= 1;
	}

	if (clipflags & ALIAS_TOP_CLIP)
	{
		k = R_AliasClip (fv[pingpong], fv[pingpong ^ 1],
							ALIAS_TOP_CLIP, k, R_Alias_clip_top);
		if (k == 0)
			return;

		pingpong ^= 1;
	}

	for (i=0 ; i<k ; i++)
	{
		if (fv[pingpong][i].cv.u < r_refdef.aliasvrect.x)
			fv[pingpong][i].cv.u = r_refdef.aliasvrect.x;
		else if (fv[pingpong][i].cv.u > r_refdef.aliasvrectright)
			fv[pingpong][i].cv.u = r_refdef.aliasvrectright;

		if (fv[pingpong][i].cv.v < r_refdef.aliasvrect.y)
			fv[pingpong][i].cv.v = r_refdef.aliasvrect.y;
		else if (fv[pingpong][i].cv.v > r_refdef.aliasvrectbottom)
			fv[pingpong][i].cv.v = r_refdef.aliasvrectbottom;

		fv[pingpong][i].flags = 0;
	}

	// draw triangles
	for (i=1 ; i<k-1 ; i++)
	{
		R_DrawTriangle(currententity, &fv[pingpong][0], &fv[pingpong][i], &fv[pingpong][i+1]);
	}
}
