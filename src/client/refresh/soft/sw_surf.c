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
// sw_surf.c: surface-related refresh code

#include "header/local.h"

static int		lightleft, sourcetstep;
static int		lightright, lightleftstep, lightrightstep, blockdivshift;
static void		*prowdestbase;
static unsigned char	*pbasesource;
static int		r_stepback;
static int		r_lightwidth;
static int		r_numhblocks, r_numvblocks;
static unsigned char	*r_source, *r_sourcemax;
static unsigned		*r_lightptr;

void R_BuildLightMap (drawsurf_t *drawsurf);
extern	unsigned	blocklights[1024];	// allow some very large lightmaps

static	float	surfscale;

static int	sc_size;
static surfcache_t	*sc_rover;
surfcache_t	*sc_base;

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
static image_t *
R_TextureAnimation (const entity_t *currententity, mtexinfo_t *tex)
{
	int c;

	if (!tex->next)
		return tex->image;

	c = currententity->frame % tex->numframes;
	while (c)
	{
		tex = tex->next;
		c--;
	}

	return tex->image;
}


/*
================
R_DrawSurfaceBlock8_anymip
================
*/
static void
R_DrawSurfaceBlock8_anymip (int level, int surfrowbytes)
{
	int		v, i, b, lightstep, lighttemp, light, size;
	unsigned char	pix, *psource, *prowdest;

	size = 1 << level;
	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
		// FIXME: make these locals?
		// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> level;
		lightrightstep = (r_lightptr[1] - lightright) >> level;

		for (i=0 ; i<size ; i++)
		{
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> level;

			light = lightright;

			for (b=(size-1); b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)vid_colormap)
						[(light & 0xFF00) + pix];
				light += lightstep;
			}

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}


/*
===============
R_DrawSurface
===============
*/
static void
R_DrawSurface (drawsurf_t *drawsurf)
{
	unsigned char	*basetptr;
	int		smax, tmax, twidth;
	int		u;
	int		soffset, basetoffset, texwidth;
	int		blocksize;
	unsigned char	*pcolumndest;
	image_t		*mt;

	mt = drawsurf->image;

	r_source = mt->pixels[drawsurf->surfmip];

	// the fractional light values should range from 0 to (VID_GRADES - 1) << 16
	// from a source range of 0 - 255

	texwidth = mt->width >> drawsurf->surfmip;

	blocksize = 16 >> drawsurf->surfmip;
	blockdivshift = NUM_MIPS - drawsurf->surfmip;

	r_lightwidth = (drawsurf->surf->extents[0]>>4)+1;

	r_numhblocks = drawsurf->surfwidth >> blockdivshift;
	r_numvblocks = drawsurf->surfheight >> blockdivshift;

	//==============================

	smax = mt->width >> drawsurf->surfmip;
	twidth = texwidth;
	tmax = mt->height >> drawsurf->surfmip;
	sourcetstep = texwidth;
	r_stepback = tmax * twidth;

	r_sourcemax = r_source + (tmax * smax);

	soffset = drawsurf->surf->texturemins[0];
	basetoffset = drawsurf->surf->texturemins[1];

	// << 16 components are to guarantee positive values for %
	soffset = ((soffset >> drawsurf->surfmip) + (smax << SHIFT16XYZ)) % smax;
	basetptr = &r_source[((((basetoffset >> drawsurf->surfmip)
		+ (tmax << SHIFT16XYZ)) % tmax) * twidth)];

	pcolumndest = drawsurf->surfdat;

	for (u=0 ; u<r_numhblocks; u++)
	{
		r_lightptr = blocklights + u;

		prowdestbase = pcolumndest;

		pbasesource = basetptr + soffset;

		R_DrawSurfaceBlock8_anymip(NUM_MIPS - drawsurf->surfmip, drawsurf->rowbytes);

		soffset = soffset + blocksize;
		if (soffset >= smax)
			soffset = 0;

		pcolumndest += blocksize;
	}
}


//=============================================================================

/*
================
R_InitCaches

================
*/
void
R_InitCaches (void)
{
	int		size;

	// calculate size to allocate
	if (sw_surfcacheoverride->value)
	{
		size = sw_surfcacheoverride->value;
	}
	else
	{
		int pix;
		size = SURFCACHE_SIZE_AT_320X240;

		pix = vid.width*vid.height;
		if (pix > 64000)
			size += (pix-64000)*3;
	}

	// round up to page size
	size = (size + 8191) & ~8191;

	R_Printf(PRINT_ALL,"%ik surface cache\n", size/1024);

	sc_size = size;
	sc_base = (surfcache_t *)malloc(size);
	sc_rover = sc_base;

	sc_base->next = NULL;
	sc_base->owner = NULL;
	sc_base->size = sc_size;
}


/*
==================
D_FlushCaches
==================
*/
void
D_FlushCaches (void)
{
	surfcache_t     *c;

	if (!sc_base)
		return;

	for (c = sc_base ; c ; c = c->next)
	{
		if (c->owner)
			*c->owner = NULL;
	}

	sc_rover = sc_base;
	sc_base->next = NULL;
	sc_base->owner = NULL;
	sc_base->size = sc_size;
}

/*
=================
D_SCAlloc
=================
*/
static surfcache_t *
D_SCAlloc (int width, int size)
{
	surfcache_t	*new;

	if ((width < 0) || (width > 256))
		ri.Sys_Error (ERR_FATAL,"D_SCAlloc: bad cache width %d\n", width);

	if ((size <= 0) || (size > 0x10000))
		ri.Sys_Error (ERR_FATAL,"D_SCAlloc: bad cache size %d\n", size);

	// Add header size
	size += ((char*)sc_base->data - (char*)sc_base);
	size = (size + 3) & ~3;
	if (size > sc_size)
		ri.Sys_Error (ERR_FATAL,"D_SCAlloc: %i > cache size of %i",size, sc_size);

	// if there is not size bytes after the rover, reset to the start
	if ( !sc_rover || (byte *)sc_rover - (byte *)sc_base > sc_size - size)
	{
		sc_rover = sc_base;
	}

	// colect and free surfcache_t blocks until the rover block is large enough
	new = sc_rover;
	if (sc_rover->owner)
		*sc_rover->owner = NULL;

	while (new->size < size)
	{
		// free another
		sc_rover = sc_rover->next;
		if (!sc_rover)
			ri.Sys_Error (ERR_FATAL,"D_SCAlloc: hit the end of memory");
		if (sc_rover->owner)
			*sc_rover->owner = NULL;

		new->size += sc_rover->size;
		new->next = sc_rover->next;
	}

	// create a fragment out of any leftovers
	if (new->size - size > 256)
	{
		sc_rover = (surfcache_t *)( (byte *)new + size);
		sc_rover->size = new->size - size;
		sc_rover->next = new->next;
		sc_rover->width = 0;
		sc_rover->owner = NULL;
		new->next = sc_rover;
		new->size = size;
	}
	else
		sc_rover = new->next;

	new->width = width;
	// DEBUG
	if (width > 0)
		new->height = (size - sizeof(*new) + sizeof(new->data)) / width;

	new->owner = NULL; // should be set properly after return

	return new;
}

//=============================================================================

static drawsurf_t	r_drawsurf;

/*
================
D_CacheSurface
================
*/
surfcache_t *
D_CacheSurface (const entity_t *currententity, msurface_t *surface, int miplevel)
{
	surfcache_t	*cache;

	//
	// if the surface is animating or flashing, flush the cache
	//
	r_drawsurf.image = R_TextureAnimation (currententity, surface->texinfo);
	r_drawsurf.lightadj[0] = r_newrefdef.lightstyles[surface->styles[0]].white*128;
	r_drawsurf.lightadj[1] = r_newrefdef.lightstyles[surface->styles[1]].white*128;
	r_drawsurf.lightadj[2] = r_newrefdef.lightstyles[surface->styles[2]].white*128;
	r_drawsurf.lightadj[3] = r_newrefdef.lightstyles[surface->styles[3]].white*128;

	//
	// see if the cache holds apropriate data
	//
	cache = surface->cachespots[miplevel];

	if (cache && !cache->dlight && surface->dlightframe != r_framecount
			&& cache->image == r_drawsurf.image
			&& cache->lightadj[0] == r_drawsurf.lightadj[0]
			&& cache->lightadj[1] == r_drawsurf.lightadj[1]
			&& cache->lightadj[2] == r_drawsurf.lightadj[2]
			&& cache->lightadj[3] == r_drawsurf.lightadj[3] )
		return cache;

	//
	// determine shape of surface
	//
	surfscale = 1.0 / (1<<miplevel);
	r_drawsurf.surfmip = miplevel;
	r_drawsurf.surfwidth = surface->extents[0] >> miplevel;
	r_drawsurf.rowbytes = r_drawsurf.surfwidth;
	r_drawsurf.surfheight = surface->extents[1] >> miplevel;

	//
	// allocate memory if needed
	//
	if (!cache) // if a texture just animated, don't reallocate it
	{
		cache = D_SCAlloc (r_drawsurf.surfwidth,
						   r_drawsurf.surfwidth * r_drawsurf.surfheight);
		surface->cachespots[miplevel] = cache;
		cache->owner = &surface->cachespots[miplevel];
		cache->mipscale = surfscale;
	}

	if (surface->dlightframe == r_framecount)
		cache->dlight = 1;
	else
		cache->dlight = 0;

	r_drawsurf.surfdat = (pixel_t *)cache->data;

	cache->image = r_drawsurf.image;
	cache->lightadj[0] = r_drawsurf.lightadj[0];
	cache->lightadj[1] = r_drawsurf.lightadj[1];
	cache->lightadj[2] = r_drawsurf.lightadj[2];
	cache->lightadj[3] = r_drawsurf.lightadj[3];

	//
	// draw and light the surface texture
	//
	r_drawsurf.surf = surface;

	c_surf++;

	// calculate the lightings
	R_BuildLightMap (&r_drawsurf);

	// rasterize the surface into the cache
	R_DrawSurface (&r_drawsurf);

	return cache;
}
