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
// d_scan.c
//
// Portable C scan-level rasterization code, all pixel depths.

#include "header/local.h"


#define SPANSTEP_SHIFT	4
#define SPANSTEP	(1 << SPANSTEP_SHIFT)

byte	**warp_rowptr;
int	*warp_column;

/*
=============
D_WarpScreen

this performs a slight compression of the screen at the same time as
the sine warp, to keep the edges from wrapping
=============
*/
void
D_WarpScreen (void)
{
	int	w, h;
	int	u,v;
	pixel_t	*dest;
	int	*turb;
	byte	**row;

	static int	cached_width, cached_height;

	//
	// these are constant over resolutions, and can be saved
	//
	w = r_newrefdef.width;
	h = r_newrefdef.height;
	if (w != cached_width || h != cached_height)
	{
		cached_width = w;
		cached_height = h;
		for (v=0 ; v<h+AMP2*2 ; v++)
		{
			int v2;

			v2 = (int)((float)v/(h + AMP2 * 2) * r_refdef.vrect.height);
			warp_rowptr[v] = r_warpbuffer + (vid.width * v2);
		}

		for (u=0 ; u<w+AMP2*2 ; u++)
		{
			int u2;

			u2 = (int)((float)u/(w + AMP2 * 2) * r_refdef.vrect.width);
			warp_column[u] = u2;
		}
	}

	turb = intsintable + ((int)(r_newrefdef.time*SPEED)&(CYCLE-1));
	dest = vid_buffer + r_newrefdef.y * vid.width + r_newrefdef.x;

	for (v=0 ; v<h ; v++, dest += vid.width)
	{
		int *col;

		col = warp_column + turb[v];
		row = warp_rowptr + v;
		for (u=0 ; u<w ; u++)
		{
			dest[u] = row[turb[u]][col[u]];
		}
	}
}


/*
=============
D_DrawSpanGetStep

Return safe span step for u/z and v/z
=============
*/
static int
D_DrawSpanGetStep(float d_zistepu, float d_zistepv, int cachewidth)
{
	int	spanzshift = SPANSTEP_SHIFT;
	int	spanzshift_value = (1 << spanzshift);
	float	d_zistepu_shifted, d_zistepv_shifted;

	d_zistepu_shifted = d_zistepu * SHIFT16XYZ_MULT;
	d_zistepv_shifted = d_zistepv * SHIFT16XYZ_MULT;

	// check that we can draw parallel surfaces to screen surface
	// (both ends have same z value)
	if ((int)(d_zistepu_shifted * spanzshift_value) == 0 &&
	    (int)(d_zistepv_shifted * spanzshift_value) == 0)
	{
		// search next safe value
		while (spanzshift_value < vid.width &&
		       (int)(d_zistepu_shifted * spanzshift_value) == 0 &&
		       (int)(d_zistepv_shifted * spanzshift_value) == 0)
		{
			spanzshift ++;
			spanzshift_value <<= 1;
		}

		// step back to last safe value
		if ((int)(d_zistepu_shifted * spanzshift_value) != 0 ||
		    (int)(d_zistepv_shifted * spanzshift_value) != 0)
		{
			spanzshift --;
		}
	}
	return spanzshift;
}

/*
=============
D_DrawTurbulentSpan
=============
*/
static pixel_t *
D_DrawTurbulentSpan (pixel_t *pdest, const pixel_t *pbase,
			 int s, int t,
			 int sstep, int tstep,
			 int spancount,
			 int *turb)
{
	do
	{
		int sturb, tturb;
		sturb = ((s + turb[(t>>SHIFT16XYZ)&(CYCLE-1)])>>16)&63;
		tturb = ((t + turb[(s>>SHIFT16XYZ)&(CYCLE-1)])>>16)&63;
		*pdest++ = *(pbase + (tturb<<6) + sturb);
		s += sstep;
		t += tstep;
	} while (--spancount > 0);
	return pdest;
}

/*
=============
TurbulentPow2
=============
*/
void
TurbulentPow2 (espan_t *pspan)
{
	float	spancountminus1;
	float	sdivzpow2stepu, tdivzpow2stepu, zipow2stepu;
	pixel_t	*r_turb_pbase;
	int	*r_turb_turb;

	r_turb_turb = sintable + ((int)(r_newrefdef.time*SPEED)&(CYCLE-1));

	r_turb_pbase = (unsigned char *)cacheblock;

	sdivzpow2stepu = d_sdivzstepu * SPANSTEP;
	tdivzpow2stepu = d_tdivzstepu * SPANSTEP;
	zipow2stepu = d_zistepu * SPANSTEP;

	do
	{
		int count, r_turb_s, r_turb_t;
		float sdivz, tdivz, zi, z, du, dv;
		pixel_t	*r_turb_pdest;

		r_turb_pdest = d_viewbuffer + (r_screenwidth * pspan->v) + pspan->u;

		count = pspan->count;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)SHIFT16XYZ_MULT / zi;	// prescale to 16.16 fixed-point

		r_turb_s = (int)(sdivz * z) + sadjust;
		if (r_turb_s > bbextents)
			r_turb_s = bbextents;
		else if (r_turb_s < 0)
			r_turb_s = 0;

		r_turb_t = (int)(tdivz * z) + tadjust;
		if (r_turb_t > bbextentt)
			r_turb_t = bbextentt;
		else if (r_turb_t < 0)
			r_turb_t = 0;

		do
		{
			int snext, tnext;
			int r_turb_sstep, r_turb_tstep;
			int r_turb_spancount;

			r_turb_sstep = 0;	// keep compiler happy
			r_turb_tstep = 0;	// ditto

			// calculate s and t at the far end of the span
			if (count >= SPANSTEP)
				r_turb_spancount = SPANSTEP;
			else
				r_turb_spancount = count;

			count -= r_turb_spancount;

			if (count)
			{
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivzpow2stepu;
				tdivz += tdivzpow2stepu;
				zi += zipow2stepu;
				z = (float)SHIFT16XYZ_MULT / zi;	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < SPANSTEP)
					// prevent round-off error on <0 steps from
					//  from causing overstepping & running off the
					//  edge of the texture
					snext = SPANSTEP;

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < SPANSTEP)
					// guard against round-off error on <0 steps
					tnext = SPANSTEP;

				r_turb_sstep = (snext - r_turb_s) >> SPANSTEP_SHIFT;
				r_turb_tstep = (tnext - r_turb_t) >> SPANSTEP_SHIFT;
			}
			else
			{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = (float)(r_turb_spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)SHIFT16XYZ_MULT / zi;	// prescale to 16.16 fixed-point
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < SPANSTEP)
					// prevent round-off error on <0 steps from
					//  from causing overstepping & running off the
					//  edge of the texture
					snext = SPANSTEP;

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < SPANSTEP)
					// guard against round-off error on <0 steps
					tnext = SPANSTEP;

				if (r_turb_spancount > 1)
				{
					r_turb_sstep = (snext - r_turb_s) / (r_turb_spancount - 1);
					r_turb_tstep = (tnext - r_turb_t) / (r_turb_spancount - 1);
				}
			}

			r_turb_s = r_turb_s & ((CYCLE<<SHIFT16XYZ)-1);
			r_turb_t = r_turb_t & ((CYCLE<<SHIFT16XYZ)-1);

			r_turb_pdest = D_DrawTurbulentSpan (r_turb_pdest, r_turb_pbase,
							    r_turb_s, r_turb_t,
							    r_turb_sstep, r_turb_tstep,
							    r_turb_spancount,
							    r_turb_turb);

			r_turb_s = snext;
			r_turb_t = tnext;

		} while (count > 0);

	} while ((pspan = pspan->pnext) != NULL);
}

//====================
//PGM
/*
=============
NonTurbulentPow2 - this is for drawing scrolling textures. they're warping water textures
	but the turbulence is automatically 0.
=============
*/
void
NonTurbulentPow2 (espan_t *pspan)
{
	float spancountminus1;
	float sdivzpow2stepu, tdivzpow2stepu, zipow2stepu;
	pixel_t	*r_turb_pbase;
	int	*r_turb_turb;

	r_turb_turb = blanktable;

	r_turb_pbase = (unsigned char *)cacheblock;

	sdivzpow2stepu = d_sdivzstepu * SPANSTEP;
	tdivzpow2stepu = d_tdivzstepu * SPANSTEP;
	zipow2stepu = d_zistepu * SPANSTEP;

	do
	{
		int count, r_turb_s, r_turb_t;
		float sdivz, tdivz, zi, z, dv, du;
		pixel_t	*r_turb_pdest;

		r_turb_pdest = d_viewbuffer + (r_screenwidth * pspan->v) + pspan->u;

		count = pspan->count;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)SHIFT16XYZ_MULT / zi;	// prescale to 16.16 fixed-point

		r_turb_s = (int)(sdivz * z) + sadjust;
		if (r_turb_s > bbextents)
			r_turb_s = bbextents;
		else if (r_turb_s < 0)
			r_turb_s = 0;

		r_turb_t = (int)(tdivz * z) + tadjust;
		if (r_turb_t > bbextentt)
			r_turb_t = bbextentt;
		else if (r_turb_t < 0)
			r_turb_t = 0;

		do
		{
			int snext, tnext;
			int r_turb_sstep, r_turb_tstep;
			int r_turb_spancount;

			r_turb_sstep = 0;	// keep compiler happy
			r_turb_tstep = 0;	// ditto

			// calculate s and t at the far end of the span
			if (count >= SPANSTEP)
				r_turb_spancount = SPANSTEP;
			else
				r_turb_spancount = count;

			count -= r_turb_spancount;

			if (count)
			{
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivzpow2stepu;
				tdivz += tdivzpow2stepu;
				zi += zipow2stepu;
				z = (float)SHIFT16XYZ_MULT / zi;	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < SPANSTEP)
					// prevent round-off error on <0 steps from
					//  from causing overstepping & running off the
					//  edge of the texture
					snext = SPANSTEP;

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < SPANSTEP)
					// guard against round-off error on <0 steps
					tnext = SPANSTEP;

				r_turb_sstep = (snext - r_turb_s) >> SPANSTEP_SHIFT;
				r_turb_tstep = (tnext - r_turb_t) >> SPANSTEP_SHIFT;
			}
			else
			{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = (float)(r_turb_spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)SHIFT16XYZ_MULT / zi;	// prescale to 16.16 fixed-point
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < SPANSTEP)
					// prevent round-off error on <0 steps from
					//  from causing overstepping & running off the
					//  edge of the texture
					snext = SPANSTEP;

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < SPANSTEP)
					// guard against round-off error on <0 steps
					tnext = SPANSTEP;

				if (r_turb_spancount > 1)
				{
					r_turb_sstep = (snext - r_turb_s) / (r_turb_spancount - 1);
					r_turb_tstep = (tnext - r_turb_t) / (r_turb_spancount - 1);
				}
			}

			r_turb_s = r_turb_s & ((CYCLE<<SHIFT16XYZ)-1);
			r_turb_t = r_turb_t & ((CYCLE<<SHIFT16XYZ)-1);

			r_turb_pdest = D_DrawTurbulentSpan (r_turb_pdest, r_turb_pbase,
							    r_turb_s, r_turb_t,
							    r_turb_sstep, r_turb_tstep,
							    r_turb_spancount,
							    r_turb_turb);

			r_turb_s = snext;
			r_turb_t = tnext;

		} while (count > 0);

	} while ((pspan = pspan->pnext) != NULL);
}
//PGM
//====================

// Enable custom filtering
extern cvar_t	*r_anisotropic;
static const int filtering_kernel[2][2][2] = {
	{
		{0x1 << (SHIFT16XYZ-2), 0x0},
		{0xC << (SHIFT16XYZ-4), 0x1 << (SHIFT16XYZ-1)}
	},
	{
		{0x1 << (SHIFT16XYZ-1), 0xC << (SHIFT16XYZ-4)},
		{0x0, 0x1 << (SHIFT16XYZ-2)}
	}
};

/*
=============
D_DrawSpan
=============
*/
static pixel_t *
D_DrawSpan(pixel_t *pdest, const pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount)
{
	const pixel_t *tdest_max = pdest + spancount;

	// horisontal span (span in same row)
	if (((t + tstep * spancount) >> SHIFT16XYZ) == (t >> SHIFT16XYZ))
	{
		// position in texture
		const pixel_t *tbase = pbase + (t >> SHIFT16XYZ) * cachewidth;

		do
		{
			*pdest++ = *(tbase + (s >> SHIFT16XYZ));
			s += sstep;
		} while (pdest < tdest_max);
	}
	// vertical span (span in same column)
	else if (((s + sstep * spancount) >> SHIFT16XYZ) == (s >> SHIFT16XYZ))
	{
		// position in texture
		const pixel_t *tbase = pbase + (s >> SHIFT16XYZ);

		do
		{
			*pdest++ = *(tbase + (t >> SHIFT16XYZ) * cachewidth);
			t += tstep;
		} while (pdest < tdest_max);
	}
	// diagonal span
	else
	{
		do
		{
			*pdest++ = *(pbase + (s >> SHIFT16XYZ) + (t >> SHIFT16XYZ) * cachewidth);
			s += sstep;
			t += tstep;
		} while (pdest < tdest_max);
	}

	return pdest;
}

/*
=============
D_DrawSpanFiltered
=============
*/
static pixel_t *
D_DrawSpanFiltered(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount,
	   const espan_t *pspan)
{
	do
	{
		int idiths = s;
		int iditht = t;

		int X = (pspan->u + spancount) & 1;
		int Y = (pspan->v)&1;

		//Using the kernel
		idiths += filtering_kernel[X][Y][0];
		iditht += filtering_kernel[X][Y][1];

		idiths = idiths >> SHIFT16XYZ;
		idiths = idiths ? idiths -1 : idiths;


		iditht = iditht >> SHIFT16XYZ;
		iditht = iditht ? iditht -1 : iditht;


		*pdest++ = *(pbase + idiths + iditht * cachewidth);
		s += sstep;
		t += tstep;
	} while (--spancount > 0);

	return pdest;
}

/*
=============
D_DrawSpansPow2
=============
*/
void
D_DrawSpansPow2 (espan_t *pspan)
{
	int 	spancount;
	pixel_t	*pbase;
	int	snext, tnext;
	float	spancountminus1;
	float	sdivzpow2stepu, tdivzpow2stepu, zipow2stepu;
	int	texture_filtering;
	int	spanstep_shift, spanstep_value;

	spanstep_shift = D_DrawSpanGetStep(d_zistepu, d_zistepv, cachewidth);
	spanstep_value = (1 << spanstep_shift);

	pbase = (unsigned char *)cacheblock;

	texture_filtering = (int)r_anisotropic->value;
	sdivzpow2stepu = d_sdivzstepu * spanstep_value;
	tdivzpow2stepu = d_tdivzstepu * spanstep_value;
	zipow2stepu = d_zistepu * spanstep_value;

	do
	{
		pixel_t	*pdest;
		int	count, s, t;
		float	sdivz, tdivz, zi, z, du, dv;

		pdest = d_viewbuffer + (r_screenwidth * pspan->v) + pspan->u;

		count = pspan->count;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)SHIFT16XYZ_MULT / zi;	// prescale to 16.16 fixed-point

		s = (int)(sdivz * z) + sadjust;
		if (s > bbextents)
			s = bbextents;
		else if (s < 0)
			s = 0;

		t = (int)(tdivz * z) + tadjust;
		if (t > bbextentt)
			t = bbextentt;
		else if (t < 0)
			t = 0;

		do
		{
			int	sstep, tstep;

			sstep = 0;	// keep compiler happy
			tstep = 0;	// ditto

			// calculate s and t at the far end of the span
			if (count >= spanstep_value)
				spancount = spanstep_value;
			else
				spancount = count;

			count -= spancount;

			if (count)
			{
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivzpow2stepu;
				tdivz += tdivzpow2stepu;
				zi += zipow2stepu;
				z = (float)SHIFT16XYZ_MULT / zi;	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < spanstep_value)
					// prevent round-off error on <0 steps from
					//  from causing overstepping & running off the
					//  edge of the texture
					snext = spanstep_value;

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < spanstep_value)
					// guard against round-off error on <0 steps
					tnext = spanstep_value;

				sstep = (snext - s) >> spanstep_shift;
				tstep = (tnext - t) >> spanstep_shift;
			}
			else
			{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = (float)(spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)SHIFT16XYZ_MULT / zi;	// prescale to 16.16 fixed-point
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < spanstep_value)
					// prevent round-off error on <0 steps from
					//  from causing overstepping & running off the
					//  edge of the texture
					snext = spanstep_value;

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < spanstep_value)
					// guard against round-off error on <0 steps
					tnext = spanstep_value;

				if (spancount > 1)
				{
					sstep = (snext - s) / (spancount - 1);
					tstep = (tnext - t) / (spancount - 1);
				}
			}

			// Drawing phrase
			if (texture_filtering == 0)
			{
				pdest = D_DrawSpan(pdest, pbase, s, t, sstep, tstep,
						   spancount);
			}
			else
			{
				pdest = D_DrawSpanFiltered(pdest, pbase, s, t, sstep, tstep,
						   spancount, pspan);
			}
			s = snext;
			t = tnext;
		} while (count > 0);

	} while ((pspan = pspan->pnext) != NULL);
}

/*
=============
D_DrawZSpans
=============
*/
void
D_DrawZSpans (espan_t *pspan)
{
	zvalue_t	izistep;

	// FIXME: check for clamping/range problems
	// we count on FP exceptions being turned off to avoid range problems
	izistep = (int)(d_zistepu * 0x8000 * (float)SHIFT16XYZ_MULT);

	do
	{
		int		count;
		zvalue_t	izi;
		zvalue_t	*pdest;
		float		zi;
		float		du, dv;

		pdest = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;

		count = pspan->count;

		// calculate the initial 1/z
		du = (float)pspan->u;
		dv = (float)pspan->v;

		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		// we count on FP exceptions being turned off to avoid range problems
		izi = (int)(zi * 0x8000 * (float)SHIFT16XYZ_MULT);

		while (count > 0)
		{
			*pdest++ = izi >> SHIFT16XYZ;
			izi += izistep;
			count--;
		}
	} while ((pspan = pspan->pnext) != NULL);
}
