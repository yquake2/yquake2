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
 * Zone malloc. It's just a normal malloc with tags.
 *
 * =======================================================================
 */

#include "header/common.h"
#include "header/zone.h"

#define Z_MAGIC 0x1d1d

zhead_t z_chain;
int z_count, z_bytes;

void
Z_Free(void *ptr)
{
	zhead_t *z;

	z = ((zhead_t *)ptr) - 1;

	if (z->magic != Z_MAGIC)
	{
		Com_Printf("ERROR: Z_free(%p) failed: bad magic\n", ptr);
		abort();
	}

	z->prev->next = z->next;
	z->next->prev = z->prev;

	z_count--;
	z_bytes -= z->size;
	free(z);
}

void
Z_Stats_f(void)
{
	Com_Printf("%i bytes in %i blocks\n", z_bytes, z_count);
}

void
Z_FreeTags(int tag)
{
	zhead_t *z, *next;

	for (z = z_chain.next; z != &z_chain; z = next)
	{
		next = z->next;

		if (z->tag == tag)
		{
			Z_Free((void *)(z + 1));
		}
	}
}

void *
Z_TagMalloc(int size, int tag)
{
	zhead_t *z;

	size = size + sizeof(zhead_t);
	z = malloc(size);

	if (!z)
	{
		Com_Error(ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes", size);
	}

	memset(z, 0, size);
	z_count++;
	z_bytes += size;
	z->magic = Z_MAGIC;
	z->tag = tag;
	z->size = size;

	z->next = z_chain.next;
	z->prev = &z_chain;
	z_chain.next->prev = z;
	z_chain.next = z;

	return (void *)(z + 1);
}

void *
Z_Malloc(int size)
{
	return Z_TagMalloc(size, 0);
}

