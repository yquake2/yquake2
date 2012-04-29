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
 * The sound caching. This file contains support functions for loading
 * the sound samples into the memory.
 *
 * =======================================================================
 */

#include "../header/client.h"
#include "header/local.h"

void
ResampleSfx ( sfx_t *sfx, int inrate, int inwidth, byte *data )
{
	int outcount;
	int srcsample;
	float stepscale;
	int i;
	int sample;
	unsigned int samplefrac, fracstep;
	sfxcache_t  *sc;

	sc = sfx->cache;

	if ( !sc )
	{
		return;
	}

	stepscale = (float) inrate / dma.speed; /* this is usually 0.5, 1, or 2 */

	outcount = (int) ( sc->length / stepscale );

	if ( outcount == 0 )
	{
		Com_Printf( "ResampleSfx: Invalid sound file '%s' (zero length)\n", sfx->name );
		Z_Free( sfx->cache );
		sfx->cache = NULL;
		return;
	}

	sc->length = outcount;

	if ( sc->loopstart != -1 )
	{
		sc->loopstart = (int) ( sc->loopstart / stepscale );
	}

	sc->speed = dma.speed;

	if ( s_loadas8bit->value )
	{
		sc->width = 1;
	}

	else
	{
		sc->width = inwidth;
	}

	sc->stereo = 0;

	/* resample / decimate to the current source rate */
	samplefrac = 0;
	fracstep = (int) ( stepscale * 256 );

	for ( i = 0; i < outcount; i++ )
	{
		srcsample = samplefrac >> 8;
		samplefrac += fracstep;

		if ( inwidth == 2 )
		{
			sample = LittleShort( ( (short *) data ) [ srcsample ] );
		}

		else
		{
			sample = (int) ( (unsigned char) ( data [ srcsample ] ) - 128 ) << 8;
		}

		if ( sc->width == 2 )
		{
			( (short *) sc->data ) [ i ] = sample;
		}

		else
		{
			( (signed char *) sc->data ) [ i ] = sample >> 8;
		}
	}
}

sfxcache_t *
S_LoadSound ( sfx_t *s )
{
	char namebuffer [ MAX_QPATH ];
	byte    *data;
	wavinfo_t info;
	int len;
	float stepscale;
	sfxcache_t  *sc;
	int size;
	char    *name;

	if ( s->name [ 0 ] == '*' )
	{
		return ( NULL );
	}

	/* see if still in memory */
	sc = s->cache;

	if ( sc )
	{
		return ( sc );
	}

	/* load it in */
	if ( s->truename )
	{
		name = s->truename;
	}

	else
	{
		name = s->name;
	}

	if ( name [ 0 ] == '#' )
	{
		strcpy( namebuffer, &name [ 1 ] );
	} else {
		Com_sprintf( namebuffer, sizeof ( namebuffer ), "sound/%s", name );
	}

	size = FS_LoadFile( namebuffer, (void **) &data );

	if ( !data )
	{
		s->cache = NULL;
		Com_DPrintf( "Couldn't load %s\n", namebuffer );
		return ( NULL );
	}

	info = GetWavinfo( s->name, data, size );

	if ( info.channels != 1 )
	{
		Com_Printf( "%s is a stereo sample\n", s->name );
		FS_FreeFile( data );
		return ( NULL );
	}
	if (sound_started != SS_OAL) {
		stepscale = (float) info.rate / dma.speed;
		len = (int) ( info.samples / stepscale );

		if ( ( info.samples == 0 ) || ( len == 0 ) )
		{
			Com_Printf( "WARNING: Zero length sound encountered: %s\n", s->name );
			FS_FreeFile( data );
			return ( NULL );
		}

		len = len * info.width * info.channels;
		sc = s->cache = Z_Malloc( len + sizeof ( sfxcache_t ) );

		if ( !sc )
		{
			FS_FreeFile( data );
			return ( NULL );
		}

		sc->length = info.samples;
		sc->loopstart = info.loopstart;
		sc->speed = info.rate;
		sc->width = info.width;
		sc->stereo = info.channels;
	}

#if USE_OPENAL
	if (sound_started == SS_OAL)
		sc = AL_UploadSfx(s, &info, data + info.dataofs);
	else
#endif
		ResampleSfx( s, sc->speed, sc->width, data + info.dataofs );

	FS_FreeFile( data );

	return ( sc );
}
