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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * This file implements the low level part of the Hunk_* memory system
 *
 * =======================================================================
 */


/* For mremap() - must be before sys/mman.h include! */
#if defined( __linux__ ) && ! defined( _GNU_SOURCE )
 #define _GNU_SOURCE
#endif

#include <sys/mman.h>
#include <errno.h>
#include <sys/time.h>

#include "../common/header/common.h"

#if defined( __FreeBSD__ )
 #include <machine/param.h>
 #define MAP_ANONYMOUS MAP_ANON
#endif


byte *membase;
int maxhunksize;
int curhunksize;

void *
Hunk_Begin ( int maxsize )
{
	/* reserve a huge chunk of memory, but don't commit any yet */
	maxhunksize = maxsize + sizeof ( int );
	curhunksize = 0;

	membase = mmap( 0, maxhunksize, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );

	if ( ( membase == NULL ) || ( membase == (byte *) -1 ) )
	{
		Sys_Error( "unable to virtual allocate %d bytes", maxsize );
	}

	*( (int *) membase ) = curhunksize;

	return ( membase + sizeof ( int ) );
}

void *
Hunk_Alloc ( int size )
{
	byte *buf;

	/* round to cacheline */
	size = ( size + 31 ) & ~31;

	if ( curhunksize + size > maxhunksize )
	{
		Sys_Error( "Hunk_Alloc overflow" );
	}

	buf = membase + sizeof ( int ) + curhunksize;
	curhunksize += size;
	return ( buf );
}

int
Hunk_End ( void )
{
	byte *n = NULL;

#if defined( __FreeBSD__ )
	size_t old_size = maxhunksize;
	size_t new_size = curhunksize + sizeof ( int );
	void *unmap_base;
	size_t unmap_len;

	new_size = round_page( new_size );
	old_size = round_page( old_size );

	if ( new_size > old_size )
	{
		n = 0; /* error */
	}
	else if ( new_size < old_size )
	{
		unmap_base = (caddr_t) ( membase + new_size );
		unmap_len = old_size - new_size;
		n = munmap( unmap_base, unmap_len ) + membase;
	}

#endif

#if defined( __linux__ )
	n = (byte *)mremap( membase, maxhunksize, curhunksize + sizeof ( int ), 0 );
#endif

	if ( n != membase )
	{
		Sys_Error( "Hunk_End: Could not remap virtual block (%d)", errno );
	}

	*( (int *) membase ) = curhunksize + sizeof ( int );

	return ( curhunksize );
}

void
Hunk_Free ( void *base )
{
	byte *m;

	if ( base )
	{
		m = ( (byte *) base ) - sizeof ( int );

		if ( munmap( m, *( (int *) m ) ) )
		{
			Sys_Error( "Hunk_Free: munmap failed (%d)", errno );
		}
	}
}
