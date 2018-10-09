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
#if defined(__linux__) && !defined(_GNU_SOURCE)
 #define _GNU_SOURCE
#endif

#include <sys/mman.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

#include "../../../common/header/common.h"

#if defined(__FreeBSD__) || defined(__OpenBSD__)
 #include <machine/param.h>
 #define MAP_ANONYMOUS MAP_ANON
#endif

#if defined(__APPLE__)
 #include <sys/types.h>
 #define MAP_ANONYMOUS MAP_ANON
#endif

byte *membase;
size_t maxhunksize;
size_t curhunksize;

void *
Hunk_Begin(int maxsize)
{

	/* reserve a huge chunk of memory, but don't commit any yet */
	/* plus 32 bytes for cacheline */
	maxhunksize = maxsize + sizeof(size_t) + 32;
	curhunksize = 0;

	membase = mmap(0, maxhunksize, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if ((membase == NULL) || (membase == (byte *)-1))
	{
		Sys_Error("unable to virtual allocate %d bytes", maxsize);
	}

	*((size_t *)membase) = curhunksize;

	return membase + sizeof(size_t);
}

void *
Hunk_Alloc(int size)
{
	byte *buf;

	/* round to cacheline */
	size = (size + 31) & ~31;

	if (curhunksize + size > maxhunksize)
	{
		Sys_Error("Hunk_Alloc overflow");
	}

	buf = membase + sizeof(size_t) + curhunksize;
	curhunksize += size;
	return buf;
}

int
Hunk_End(void)
{
	byte *n = NULL;

#if defined(__linux__)
	n = (byte *)mremap(membase, maxhunksize, curhunksize + sizeof(size_t), 0);
#else
 #ifndef round_page
 size_t page_size = sysconf(_SC_PAGESIZE);
 #define round_page(x) ((((size_t)(x)) + page_size-1) & ~(page_size-1))
 #endif

	size_t old_size = round_page(maxhunksize);
	size_t new_size = round_page(curhunksize + sizeof(size_t));

	if (new_size > old_size)
	{
		/* Can never happen. If it happens something's very wrong. */
		n = 0;
	}
	else if (new_size < old_size)
	{
		/* Hunk is to big, we need to shrink it. */
		n = munmap(membase + new_size, old_size - new_size) + membase;
	}
	else
	{
		/* No change necessary. */
		n = membase;
	}
#endif

	if (n != membase)
	{
		Sys_Error("Hunk_End: Could not remap virtual block (%d)", errno);
	}

	*((size_t *)membase) = curhunksize + sizeof(size_t);

	return curhunksize;
}

void
Hunk_Free(void *base)
{
	if (base)
	{
		byte *m;

		m = ((byte *)base) - sizeof(size_t);

		if (munmap(m, *((size_t *)m)))
		{
			Sys_Error("Hunk_Free: munmap failed (%d)", errno);
		}
	}
}

