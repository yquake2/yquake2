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
 * Memory handling functions.
 *
 * =======================================================================
 */

#include <windows.h>

#include "../../../common/header/common.h"

byte *membase;
int hunkcount;
int hunkmaxsize;
int cursize;

void *
Hunk_Begin(int maxsize)
{
	/* reserve a huge chunk of memory,
	   but don't commit any yet */
	cursize = 0;
	hunkmaxsize = maxsize;
	membase = VirtualAlloc(NULL, maxsize, MEM_RESERVE, PAGE_NOACCESS);

	if (!membase)
	{
		Sys_Error("VirtualAlloc reserve failed");
	}

	return (void *)membase;
}

void *
Hunk_Alloc(int size)
{
	void *buf;

	/* round to cacheline */
	size = (size + 31) & ~31;

	/* commit pages as needed */
	buf = VirtualAlloc(membase, cursize + size, MEM_COMMIT, PAGE_READWRITE);

	if (!buf)
	{
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPTSTR)&buf, 0, NULL);
		Sys_Error("VirtualAlloc commit failed.\n%s", buf);
	}

	cursize += size;

	if (cursize > hunkmaxsize)
	{
		Sys_Error("Hunk_Alloc overflow");
	}

	return (void *)(membase + cursize - size);
}

int
Hunk_End(void)
{
	hunkcount++;

	return cursize;
}

void
Hunk_Free(void *base)
{
	if (base)
	{
		VirtualFree(base, 0, MEM_RELEASE);
	}

	hunkcount--;
}

