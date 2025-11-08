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
 * Header file to the zone malloc
 *
 * =======================================================================
 */

#ifndef CO_ZONE_H
#define CO_ZONE_H

void Z_Init(void);

void Z_Free(void *ptr);
void Z_FreeTags(unsigned short tag);

void *Z_Malloc(size_t size);           /* returns 0 filled memory */
void *Z_TagMalloc(size_t size, unsigned short tag);
void *Z_Realloc(void *ptr, size_t size);
void *Z_TagRealloc(void *ptr, size_t size, unsigned short tag);

size_t Z_BlockSize(const void *ptr);

void Z_Stats_f (void);

#endif
