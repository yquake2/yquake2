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
// sw_polyset.c: routines for drawing sets of polygons sharing the same
// texture (used for Alias models)

#include "header/local.h"
#include <limits.h>

typedef struct {
	int	numleftedges;
	compactvert_t	*pleftedgevert0;
	compactvert_t	*pleftedgevert1;
	compactvert_t	*pleftedgevert2;
	int	numrightedges;
	compactvert_t	*prightedgevert0;
	compactvert_t	*prightedgevert1;
	compactvert_t	*prightedgevert2;
} edgetable;

static int	ubasestep, errorterm, erroradjustup, erroradjustdown;

static compactvert_t r_p0, r_p1, r_p2;

static int	d_xdenom;

static edgetable *pedgetable;

static edgetable edgetables[12] = {
	{1, &r_p0, &r_p2, &r_p2, 2, &r_p0, &r_p1, &r_p2},
	{2, &r_p1, &r_p0, &r_p2, 1, &r_p1, &r_p2, &r_p2},
	{1, &r_p0, &r_p2, &r_p2, 1, &r_p1, &r_p2, &r_p2},	// flat top
	{1, &r_p1, &r_p0, &r_p0, 2, &r_p1, &r_p2, &r_p0},
	{2, &r_p0, &r_p2, &r_p1, 1, &r_p0, &r_p1, &r_p1},
	{1, &r_p2, &r_p1, &r_p1, 1, &r_p2, &r_p0, &r_p0},
	{1, &r_p2, &r_p1, &r_p1, 2, &r_p2, &r_p0, &r_p1},
	{2, &r_p2, &r_p1, &r_p0, 1, &r_p2, &r_p0, &r_p0},
	{1, &r_p1, &r_p0, &r_p0, 1, &r_p1, &r_p2, &r_p2},
	{1, &r_p2, &r_p1, &r_p1, 1, &r_p0, &r_p1, &r_p1},	// flat top
	{1, &r_p1, &r_p0, &r_p0, 1, &r_p2, &r_p0, &r_p0},	// flat top
	{1, &r_p0, &r_p2, &r_p2, 1, &r_p0, &r_p1, &r_p1},
};

// FIXME: some of these can become statics
static int	a_sstepxfrac, a_tstepxfrac, a_ststepxwhole;
static int	r_sstepx, r_tstepx, r_sstepy, r_tstepy;
static light3_t	r_lstepx, r_lstepy;
static zvalue_t	r_zistepx, r_zistepy;
static int	d_aspancount;

static spanpackage_t	*d_pedgespanpackage;

spanpackage_t	*triangle_spans, *triangles_max;

static int	d_sfrac, d_tfrac;
static light3_t d_light;
static zvalue_t	d_zi;
static int	d_ptexextrastep, d_sfracextrastep;
static int	d_tfracextrastep, d_ptexbasestep;
static light3_t	d_lightbasestep, d_lightextrastep;
static int	d_sfracbasestep, d_tfracbasestep;
static zvalue_t	d_ziextrastep, d_zibasestep;

static byte	*skintable[MAX_LBM_HEIGHT];
int		skinwidth;
static pixel_t	*skinstart;

void	(*d_pdrawspans)(const entity_t *currententity, spanpackage_t *pspanpackage);

static void R_PolysetSetEdgeTable(void);
static void R_RasterizeAliasPolySmooth(const entity_t *currententity);

// ======================
// 64 65 66 67 68 69 70 71   72 73 74 75 76 77 78 79
static const byte irtable[256] = {
	79, 78, 77, 76, 75, 74, 73, 72,		// black/white
	71, 70, 69, 68, 67, 66, 65, 64,
	64, 65, 66, 67, 68, 69, 70, 71,		// dark taupe
	72, 73, 74, 75, 76, 77, 78, 79,

	64, 65, 66, 67, 68, 69, 70, 71,		// slate grey
	72, 73, 74, 75, 76, 77, 78, 79,
	208, 208, 208, 208, 208, 208, 208, 208,	// unused?'
	64, 66, 68, 70, 72, 74, 76, 78,		// dark yellow

	64, 65, 66, 67, 68, 69, 70, 71,		// dark red
	72, 73, 74, 75, 76, 77, 78, 79,
	64, 65, 66, 67, 68, 69, 70, 71,		// grey/tan
	72, 73, 74, 75, 76, 77, 78, 79,

	64, 66, 68, 70, 72, 74, 76, 78,		// chocolate
	68, 67, 66, 65, 64, 65, 66, 67,		// mauve / teal
	68, 69, 70, 71, 72, 73, 74, 75,
	76, 76, 77, 77, 78, 78, 79, 79,

	64, 65, 66, 67, 68, 69, 70, 71,		// more mauve
	72, 73, 74, 75, 76, 77, 78, 79,
	64, 65, 66, 67, 68, 69, 70, 71,		// olive
	72, 73, 74, 75, 76, 77, 78, 79,

	64, 65, 66, 67, 68, 69, 70, 71,		// maroon
	72, 73, 74, 75, 76, 77, 78, 79,
	64, 65, 66, 67, 68, 69, 70, 71,		// sky blue
	72, 73, 74, 75, 76, 77, 78, 79,

	64, 65, 66, 67, 68, 69, 70, 71,		// olive again
	72, 73, 74, 75, 76, 77, 78, 79,
	64, 65, 66, 67, 68, 69, 70, 71,		// nuclear green
	64, 65, 66, 67, 68, 69, 70, 71,		// bright yellow

	64, 65, 66, 67, 68, 69, 70, 71,		// fire colors
	72, 73, 74, 75, 76, 77, 78, 79,
	208, 208, 64, 64, 70, 71, 72, 64,		// mishmash1
	66, 68, 70, 64, 65, 66, 67, 68};		// mishmash2

// ======================

/*
================
R_PolysetUpdateTables
================
*/
void
R_PolysetUpdateTables (void)
{
	byte	*s;

	if (r_affinetridesc.skinwidth != skinwidth ||
		r_affinetridesc.pskin != skinstart)
	{
		int i;

		skinwidth = r_affinetridesc.skinwidth;
		skinstart = r_affinetridesc.pskin;
		s = skinstart;
		for (i=0 ; i<MAX_LBM_HEIGHT ; i++, s+=skinwidth)
			skintable[i] = s;
	}
}

/*
================
R_DrawTriangle
================
*/
void
R_DrawTriangle(const entity_t *currententity, const finalvert_t *a, const finalvert_t *b, const finalvert_t *c)
{
	int dv1_ab, dv0_ac;
	int dv0_ab, dv1_ac;

	/*
	d_xdenom = ( a->v[1] - b->v[1] ) * ( a->v[0] - c->v[0] ) -
			   ( a->v[0] - b->v[0] ) * ( a->v[1] - c->v[1] );
	*/

	dv0_ab = a->cv.u - b->cv.u;
	dv1_ab = a->cv.v - b->cv.v;

	if ( !( dv0_ab | dv1_ab ) )
		return;

	dv0_ac = a->cv.u - c->cv.u;
	dv1_ac = a->cv.v - c->cv.v;

	if ( !( dv0_ac | dv1_ac ) )
		return;

	d_xdenom = ( dv0_ac * dv1_ab ) - ( dv0_ab * dv1_ac );

	if ( d_xdenom < 0 )
	{
		memcpy(&r_p0, &a->cv, sizeof(compactvert_t));
		memcpy(&r_p1, &b->cv, sizeof(compactvert_t));
		memcpy(&r_p2, &c->cv, sizeof(compactvert_t));

		R_PolysetSetEdgeTable ();
		R_RasterizeAliasPolySmooth(currententity);
	}
}

static void
R_PushEdgesSpan(int u, int v, int count,
		pixel_t* d_ptex, int d_sfrac, int d_tfrac, light3_t d_light, zvalue_t d_zi)
{
	d_pedgespanpackage->u = u;
	d_pedgespanpackage->v = v;
	d_pedgespanpackage->count = count;
	d_pedgespanpackage->ptex = d_ptex;

	d_pedgespanpackage->sfrac = d_sfrac;
	d_pedgespanpackage->tfrac = d_tfrac;

	// FIXME: need to clamp l, s, t, at both ends?
	memcpy(d_pedgespanpackage->light, d_light, sizeof(light3_t));
	d_pedgespanpackage->zi = d_zi;

	d_pedgespanpackage++;
}

/*
===================
R_PolysetScanLeftEdge_C
====================
*/
static void
R_PolysetScanLeftEdge_C(int height, pixel_t *d_ptex, int u, int v)
{
	do
	{
		R_PushEdgesSpan(u, v, d_aspancount,
				d_ptex, d_sfrac, d_tfrac, d_light, d_zi);

		v ++;
		u += ubasestep;
		d_aspancount += ubasestep;

		errorterm += erroradjustup;
		if (errorterm >= 0)
		{
			int i;

			// addtional step for compensate error
			u ++;
			d_aspancount ++;

			d_ptex += d_ptexextrastep;
			d_sfrac += d_sfracextrastep;
			d_ptex += d_sfrac >> SHIFT16XYZ;

			d_sfrac &= 0xFFFF;
			d_tfrac += d_tfracextrastep;
			if (d_tfrac & 0x10000)
			{
				d_ptex += r_affinetridesc.skinwidth;
				d_tfrac &= 0xFFFF;
			}

			for(i=0; i<3; i++)
				d_light[i] += d_lightextrastep[i];

			d_zi += d_ziextrastep;
			errorterm -= erroradjustdown;
		}
		else
		{
			int i;

			d_ptex += d_ptexbasestep;
			d_sfrac += d_sfracbasestep;
			d_ptex += d_sfrac >> SHIFT16XYZ;
			d_sfrac &= 0xFFFF;
			d_tfrac += d_tfracbasestep;
			if (d_tfrac & 0x10000)
			{
				d_ptex += r_affinetridesc.skinwidth;
				d_tfrac &= 0xFFFF;
			}

			for(i=0; i<3; i++)
				d_light[i] += d_lightbasestep[i];

			d_zi += d_zibasestep;
		}
	} while (--height);
}

/*
===================
FloorDivMod

Returns mathematically correct (floor-based) quotient and remainder for
numer and denom, both of which should contain no fractional part. The
quotient must fit in 32 bits.
FIXME: GET RID OF THIS! (FloorDivMod)
====================
*/
static void
FloorDivMod(int numer, int denom, int *quo, int *rem)
{
	int q, r;

// just exclude ARM32 with FPU, e.g. Pi 1
#if defined(__i386__) || defined(__amd64__)
	q = numer / denom;
	r = numer - q * denom;
	if (-1/2 || 1/-2 || -1/-2) {
		// long live C89
		if (r < 0 && r < denom) {
			q += 1;
			r -= denom;
		}
	} else {
		// C99 always truncates to 0
		if ((numer ^ denom) < 0 && r != 0) {
			q -= 1;
			r += denom;
		}
	}
	if ((numer < 0) ^ (denom < 0))
		assert(q <= 0);
	else
		assert(q >= 0);
	if (denom < 0)
		assert(r > denom && r <= 0);
	else
		assert(r >= 0 && r < denom);
#else
	float num = numer, den = denom;
	float	x;

	if (numer >= 0)
	{

		x = floor(num / den);
		q = (int)x;
		r = (int)floor(num - (x * den));
	}
	else
	{
		//
		// perform operations with positive values, and fix mod to make floor-based
		//
		x = floor(-num / den);
		q = -(int)x;
		r = (int)floor(-num - (x * den));
		if (r != 0)
		{
			q--;
			r = denom - r;
		}
	}
#endif
	*quo = q;
	*rem = r;
}

/*
===================
R_PolysetSetUpForLineScan
====================
*/
static void
R_PolysetSetUpForLineScan(fixed8_t startvertu, fixed8_t startvertv,
		fixed8_t endvertu, fixed8_t endvertv)
{
	int tm, tn;

	tm = endvertu - startvertu;
	tn = endvertv - startvertv;

	errorterm = -1;
	erroradjustdown = tn;

	FloorDivMod (tm, tn, &ubasestep, &erroradjustup);
}

/*
================
R_PolysetCalcGradients
================
*/
static void
R_PolysetCalcGradients (int skinwidth)
{
	float	xstepdenominv, ystepdenominv, t0, t1;
	float	p01_minus_p21, p11_minus_p21, p00_minus_p20, p10_minus_p20;
	int i;

	p00_minus_p20 = r_p0.u - r_p2.u;
	p01_minus_p21 = r_p0.v - r_p2.v;
	p10_minus_p20 = r_p1.u - r_p2.u;
	p11_minus_p21 = r_p1.v - r_p2.v;

	xstepdenominv = 1.0 / (float)d_xdenom;

	ystepdenominv = -xstepdenominv;

	// ceil () for light so positive steps are exaggerated, negative steps
	// diminished,  pushing us away from underflow toward overflow. Underflow is
	// very visible, overflow is very unlikely, because of ambient lighting
	for (i=0; i<3; i++)
	{
		t0 = r_p0.l[i] - r_p2.l[i];
		t1 = r_p1.l[i] - r_p2.l[i];
		r_lstepx[i] = (int)
				ceil((t1 * p01_minus_p21 - t0 * p11_minus_p21) * xstepdenominv);
		r_lstepy[i] = (int)
				ceil((t1 * p00_minus_p20 - t0 * p10_minus_p20) * ystepdenominv);
	}

	t0 = r_p0.s - r_p2.s;
	t1 = r_p1.s - r_p2.s;
	r_sstepx = (int)((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
			xstepdenominv);
	r_sstepy = (int)((t1 * p00_minus_p20 - t0* p10_minus_p20) *
			ystepdenominv);

	t0 = r_p0.t - r_p2.t;
	t1 = r_p1.t - r_p2.t;
	r_tstepx = (int)((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
			xstepdenominv);
	r_tstepy = (int)((t1 * p00_minus_p20 - t0 * p10_minus_p20) *
			ystepdenominv);

	t0 = r_p0.zi - r_p2.zi;
	t1 = r_p1.zi - r_p2.zi;
	r_zistepx = (int)((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
			xstepdenominv);
	r_zistepy = (int)((t1 * p00_minus_p20 - t0 * p10_minus_p20) *
			ystepdenominv);

	{
		a_sstepxfrac = r_sstepx & 0xFFFF;
		a_tstepxfrac = r_tstepx & 0xFFFF;
	}

	a_ststepxwhole = skinwidth * (r_tstepx >> SHIFT16XYZ) + (r_sstepx >> SHIFT16XYZ);
}


/*
================
R_PolysetDrawSpans8
================
*/
void
R_PolysetDrawSpans8_33(const entity_t *currententity, spanpackage_t *pspanpackage)
{
	pixel_t		*lpdest;
	pixel_t		*lptex;
	int		lsfrac, ltfrac;
	light3_t	llight;
	zvalue_t	lzi;
	zvalue_t	*lpz;

	do
	{
		int lcount;

		lcount = d_aspancount - pspanpackage->count;

		errorterm += erroradjustup;
		d_aspancount += ubasestep;

		if (errorterm >= 0)
		{
			// addtional step for compensate error
			d_aspancount ++;
			errorterm -= erroradjustdown;
		}

		if (lcount > 0)
		{
			int	pos_shift = (pspanpackage->v * vid_buffer_width) + pspanpackage->u;

			lpdest = d_viewbuffer + pos_shift;
			lpz = d_pzbuffer + pos_shift;
			lptex = pspanpackage->ptex;
			lsfrac = pspanpackage->sfrac;
			ltfrac = pspanpackage->tfrac;
			memcpy(llight, pspanpackage->light, sizeof(light3_t));
			lzi = pspanpackage->zi;

			do
			{
				int i;

				if ((lzi >> SHIFT16XYZ) >= *lpz)
				{
					int temp = R_ApplyLight(*lptex, llight);

					*lpdest = vid_alphamap[temp + *lpdest*256];
				}
				lpdest++;
				lzi += r_zistepx;
				lpz++;
				for(i=0; i<3; i++)
					llight[i] += r_lstepx[i];
				lptex += a_ststepxwhole;
				lsfrac += a_sstepxfrac;
				lptex += lsfrac >> SHIFT16XYZ;
				lsfrac &= 0xFFFF;
				ltfrac += a_tstepxfrac;
				if (ltfrac & 0x10000)
				{
					lptex += r_affinetridesc.skinwidth;
					ltfrac &= 0xFFFF;
				}
			} while (--lcount);
		}

		pspanpackage++;
	} while (pspanpackage->count != INT_MIN);
}

void
R_PolysetDrawSpansConstant8_33(const entity_t *currententity, spanpackage_t *pspanpackage)
{
	pixel_t		*lpdest;
	int		lzi;
	zvalue_t	*lpz;

	do
	{
		int lcount;

		lcount = d_aspancount - pspanpackage->count;

		errorterm += erroradjustup;
		d_aspancount += ubasestep;

		if (errorterm >= 0)
		{
			// addtional step for compensate error
			d_aspancount ++;
			errorterm -= erroradjustdown;
		}

		if (lcount > 0)
		{
			int	pos_shift = (pspanpackage->v * vid_buffer_width) + pspanpackage->u;

			lpdest = d_viewbuffer + pos_shift;
			lpz = d_pzbuffer + pos_shift;
			lzi = pspanpackage->zi;

			do
			{
				if ((lzi >> SHIFT16XYZ) >= *lpz)
				{
					*lpdest = vid_alphamap[r_aliasblendcolor + *lpdest*256];
				}
				lpdest++;
				lzi += r_zistepx;
				lpz++;
			} while (--lcount);
		}

		pspanpackage++;
	} while (pspanpackage->count != INT_MIN);
}

void
R_PolysetDrawSpans8_66(const entity_t *currententity, spanpackage_t *pspanpackage)
{
	pixel_t		*lpdest;
	pixel_t		*lptex;
	int		lsfrac, ltfrac;
	light3_t	llight;
	zvalue_t	lzi;
	zvalue_t	*lpz;

	do
	{
		int lcount;

		lcount = d_aspancount - pspanpackage->count;

		errorterm += erroradjustup;
		d_aspancount += ubasestep;

		if (errorterm >= 0)
		{
			// addtional step for compensate error
			d_aspancount ++;
			errorterm -= erroradjustdown;
		}

		if (lcount > 0)
		{
			int	pos_shift = (pspanpackage->v * vid_buffer_width) + pspanpackage->u;
			qboolean	zdamaged = false;

			lpdest = d_viewbuffer + pos_shift;
			lpz = d_pzbuffer + pos_shift;
			lptex = pspanpackage->ptex;
			lsfrac = pspanpackage->sfrac;
			ltfrac = pspanpackage->tfrac;
			memcpy(llight, pspanpackage->light, sizeof(light3_t));
			lzi = pspanpackage->zi;

			do
			{
				int i;

				if ((lzi >> SHIFT16XYZ) >= *lpz)
				{
					int temp = R_ApplyLight(*lptex, llight);

					*lpdest = vid_alphamap[temp*256 + *lpdest];
					*lpz = lzi >> SHIFT16XYZ;
					zdamaged = true;
				}
				lpdest++;
				lzi += r_zistepx;
				lpz++;
				for(i=0; i<3; i++)
					llight[i] += r_lstepx[i];
				lptex += a_ststepxwhole;
				lsfrac += a_sstepxfrac;
				lptex += lsfrac >> SHIFT16XYZ;
				lsfrac &= 0xFFFF;
				ltfrac += a_tstepxfrac;
				if (ltfrac & 0x10000)
				{
					lptex += r_affinetridesc.skinwidth;
					ltfrac &= 0xFFFF;
				}
			} while (--lcount);

			if (zdamaged)
			{
				// damaged only current line
				VID_DamageZBuffer(pspanpackage->u, pspanpackage->v);
				VID_DamageZBuffer(
					pspanpackage->u + d_aspancount - pspanpackage->count,
					pspanpackage->v);
			}
		}

		pspanpackage++;
	} while (pspanpackage->count != INT_MIN);
}

void
R_PolysetDrawSpansConstant8_66(const entity_t *currententity, spanpackage_t *pspanpackage)
{
	pixel_t		*lpdest;
	zvalue_t	lzi;
	zvalue_t	*lpz;

	do
	{
		int lcount;

		lcount = d_aspancount - pspanpackage->count;

		errorterm += erroradjustup;
		d_aspancount += ubasestep;

		if (errorterm >= 0)
		{
			// addtional step for compensate error
			d_aspancount ++;
			errorterm -= erroradjustdown;
		}

		if (lcount > 0)
		{
			int	pos_shift = (pspanpackage->v * vid_buffer_width) + pspanpackage->u;
			qboolean	zdamaged = false;

			lpdest = d_viewbuffer + pos_shift;
			lpz = d_pzbuffer + pos_shift;
			lzi = pspanpackage->zi;

			do
			{
				if ((lzi >> SHIFT16XYZ) >= *lpz)
				{
					*lpdest = vid_alphamap[r_aliasblendcolor*256 + *lpdest];
					zdamaged = true;
				}
				lpdest++;
				lzi += r_zistepx;
				lpz++;
			} while (--lcount);

			if (zdamaged)
			{
				// damaged only current line
				VID_DamageZBuffer(pspanpackage->u, pspanpackage->v);
				VID_DamageZBuffer(
					pspanpackage->u + d_aspancount - pspanpackage->count,
					pspanpackage->v);
			}
		}

		pspanpackage++;
	} while (pspanpackage->count != INT_MIN);
}

void
R_PolysetDrawSpans8_Opaque (const entity_t *currententity, spanpackage_t *pspanpackage)
{
	do
	{
		int lcount;

		lcount = d_aspancount - pspanpackage->count;
		errorterm += erroradjustup;
		d_aspancount += ubasestep;

		if (errorterm >= 0)
		{
			// addtional step for compensate error
			d_aspancount ++;
			errorterm -= erroradjustdown;
		}

		if (lcount > 0)
		{
			int		lsfrac, ltfrac;
			pixel_t		*lpdest;
			pixel_t		*lptex;
			light3_t	llight;
			zvalue_t	lzi;
			zvalue_t	*lpz;
			int		pos_shift = (pspanpackage->v * vid_buffer_width) + pspanpackage->u;
			qboolean	zdamaged = false;

			lpdest = d_viewbuffer + pos_shift;
			lpz = d_pzbuffer + pos_shift;

			lptex = pspanpackage->ptex;
			lsfrac = pspanpackage->sfrac;
			ltfrac = pspanpackage->tfrac;
			memcpy(llight, pspanpackage->light, sizeof(light3_t));
			lzi = pspanpackage->zi;

			do
			{
				int i;

				if ((lzi >> SHIFT16XYZ) >= *lpz)
				{
					if(r_newrefdef.rdflags & RDF_IRGOGGLES && currententity->flags & RF_IR_VISIBLE)
						*lpdest = vid_colormap[irtable[*lptex]];
					else
						*lpdest = R_ApplyLight(*lptex, llight);

					*lpz = lzi >> SHIFT16XYZ;
					zdamaged = true;
				}
				lpdest++;
				lzi += r_zistepx;
				lpz++;
				for(i=0; i<3; i++)
					llight[i] += r_lstepx[i];
				lptex += a_ststepxwhole;
				lsfrac += a_sstepxfrac;
				lptex += lsfrac >> SHIFT16XYZ;
				lsfrac &= 0xFFFF;
				ltfrac += a_tstepxfrac;
				if (ltfrac & 0x10000)
				{
					lptex += r_affinetridesc.skinwidth;
					ltfrac &= 0xFFFF;
				}
			} while (--lcount);

			if (zdamaged)
			{
				// damaged only current line
				VID_DamageZBuffer(pspanpackage->u, pspanpackage->v);
				VID_DamageZBuffer(
					pspanpackage->u + d_aspancount - pspanpackage->count,
					pspanpackage->v);
			}
		}

		pspanpackage++;
	} while (pspanpackage->count != INT_MIN);
}

static void
R_ProcessLeftEdge(compactvert_t *plefttop, compactvert_t *prighttop,
		compactvert_t *pleftbottom)
{
	light3_t working_lstepx;
	pixel_t	*d_ptex;
	int u, v;
	int s, t;
	int height;
	int i;

	u = plefttop->u;
	v = plefttop->v;
	d_aspancount = plefttop->u - prighttop->u;

	s = plefttop->s;
	t = plefttop->t;
	i = (s >> SHIFT16XYZ) + (t >> SHIFT16XYZ) * r_affinetridesc.skinwidth;
	d_ptex = &r_affinetridesc.pskin[i];
	d_sfrac = s & 0xFFFF;
	d_tfrac = t & 0xFFFF;

	memcpy(d_light, plefttop->l, sizeof(light3_t));
	d_zi = plefttop->zi;

	height = pleftbottom->v - plefttop->v;
	if (height == 1)
	{
		// skip calculations needed for 2+ spans
		R_PushEdgesSpan(u, v, d_aspancount,
				d_ptex, d_sfrac, d_tfrac, d_light, d_zi);
		return;
	}
	R_PolysetSetUpForLineScan(plefttop->u, plefttop->v,
				  pleftbottom->u, pleftbottom->v);

	// for negative steps in x along left edge, bias toward overflow rather than
	// underflow (sort of turning the floor () we did in the gradient calcs into
	// ceil (), but plus a little bit)
	t = ubasestep < 0;
	for (i = 0; i < 3; i++)
		working_lstepx[i] = r_lstepx[i] - t;

	// base steps for drawers
	s = r_sstepy + r_sstepx * ubasestep;
	t = r_tstepy + r_tstepx * ubasestep;
	d_ptexbasestep = (s >> SHIFT16XYZ) +
			 (t >> SHIFT16XYZ) * r_affinetridesc.skinwidth;
	d_sfracbasestep = s & 0xFFFF;
	d_tfracbasestep = t & 0xFFFF;

	for (i = 0; i < 3; i++)
		d_lightbasestep[i] = r_lstepy[i] + working_lstepx[i] * ubasestep;

	d_zibasestep = r_zistepy + r_zistepx * ubasestep;

	// extra steps for drawers
	s += r_sstepx;
	t += r_tstepx;
	d_ptexextrastep = (s >> SHIFT16XYZ) +
			  (t >> SHIFT16XYZ) * r_affinetridesc.skinwidth;
	d_sfracextrastep = s & 0xFFFF;
	d_tfracextrastep = t & 0xFFFF;

	for (i = 0; i < 3; i++)
		d_lightextrastep[i] = d_lightbasestep[i] + working_lstepx[i];

	d_ziextrastep = d_zibasestep + r_zistepx;

	R_PolysetScanLeftEdge_C(height, d_ptex, u, v);
}

/*
================
R_RasterizeAliasPolySmooth
================
*/
static void
R_RasterizeAliasPolySmooth(const entity_t *currententity)
{
	compactvert_t *plefttop, *prighttop, *pleftbottom, *prightbottom;
	spanpackage_t *pstart;
	int originalcount;
	int leftheight, rightheight;

	// set the s, t, and light gradients, which are consistent across
	// the triangle because being a triangle, things are affine
	R_PolysetCalcGradients (r_affinetridesc.skinwidth);

	plefttop = pedgetable->pleftedgevert0;
	prighttop = pedgetable->prightedgevert0;

	pleftbottom = pedgetable->pleftedgevert1;
	prightbottom = pedgetable->prightedgevert1;

	// make sure we have enough spans for the full height
	// plus 1 more for the list terminator.
	// vert2 in the edgetable is always the lowest.
	// (this is silly anyway, just allocate vid.height+1)
	leftheight = pedgetable->pleftedgevert2->v - plefttop->v;
	rightheight = pedgetable->prightedgevert2->v - prighttop->v;
	if (leftheight < rightheight)
		leftheight = rightheight;
	if (&triangle_spans[leftheight] >= triangles_max)
	{
		r_outoftriangles = true;
		return;
	}
	// init span allocation ptr
	d_pedgespanpackage = triangle_spans;

	// 1. scan out the top (and possibly only) part of the left edge
	R_ProcessLeftEdge(plefttop, prighttop, pleftbottom);

	// 2. scan out the bottom part of the left edge, if it exists
	if (pedgetable->numleftedges == 2)
	{
		plefttop = pleftbottom;
		pleftbottom = pedgetable->pleftedgevert2;

		R_ProcessLeftEdge(plefttop, prighttop, pleftbottom);
	}
	assert(d_pedgespanpackage <= triangles_max);

	// 3. scan out the top (and possibly only) part of the right edge,
	//  drawing spans that terminate on it
	rightheight = prightbottom->v - prighttop->v;

	// reset u delta before drawing the top
	d_aspancount = 0;

	// save for step 4
	pstart = &triangle_spans[rightheight];
	originalcount = pstart->count;
	// mark end of the spanpackages
	pstart->count = INT_MIN;

	R_PolysetSetUpForLineScan(prighttop->u, prighttop->v,
				  prightbottom->u, prightbottom->v);
	(*d_pdrawspans) (currententity, triangle_spans);

	// 4. scan out the bottom part of the right edge, if it exists
	if (pedgetable->numrightedges == 2)
	{
		// 4.1 restore the starting span count
		pstart->count = originalcount;

		d_aspancount = prightbottom->u - prighttop->u;

		// 4.2 shift verts like in step 2
		prighttop = prightbottom;
		prightbottom = pedgetable->prightedgevert2;

		// 4.3 increase right height
		rightheight += prightbottom->v - prighttop->v;

		// mark end of the spanpackages
		triangle_spans[rightheight].count = INT_MIN;

		R_PolysetSetUpForLineScan(prighttop->u, prighttop->v,
					  prightbottom->u, prightbottom->v);
		(*d_pdrawspans) (currententity, pstart);
	}
}

/*
================
R_PolysetSetEdgeTable
================
*/
static void
R_PolysetSetEdgeTable (void)
{
	int			edgetableindex;

	edgetableindex = 0;	// assume the vertices are already in
				//  top to bottom order

	//
	// determine which edges are right & left, and the order in which
	// to rasterize them
	//
	if (r_p0.v >= r_p1.v)
	{
		if (r_p0.v == r_p1.v)
		{
			if (r_p0.v < r_p2.v)
				pedgetable = &edgetables[2];
			else
				pedgetable = &edgetables[5];

			return;
		}
		else
		{
			edgetableindex = 1;
		}
	}

	if (r_p0.v == r_p2.v)
	{
		if (edgetableindex)
			pedgetable = &edgetables[8];
		else
			pedgetable = &edgetables[9];

		return;
	}
	else if (r_p1.v == r_p2.v)
	{
		if (edgetableindex)
			pedgetable = &edgetables[10];
		else
			pedgetable = &edgetables[11];

		return;
	}

	if (r_p0.v > r_p2.v)
		edgetableindex += 2;

	if (r_p1.v > r_p2.v)
		edgetableindex += 4;

	pedgetable = &edgetables[edgetableindex];
}
