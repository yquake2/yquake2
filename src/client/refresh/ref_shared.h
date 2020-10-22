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
 * Header shared between different refreshers (but not with client)
 *
 * =======================================================================
 */

#ifndef SRC_CLIENT_REFRESH_REF_SHARED_H_
#define SRC_CLIENT_REFRESH_REF_SHARED_H_

#include "../vid/header/ref.h"

/*
 * skins will be outline flood filled and mip mapped
 * pics and sprites with alpha will be outline flood filled
 * pic won't be mip mapped
 *
 * model skin
 * sprite frame
 * wall texture
 * pic
 */
typedef enum
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky
} imagetype_t;

typedef enum
{
	mod_bad,
	mod_brush,
	mod_sprite,
	mod_alias
} modtype_t;

#define MAX_LBM_HEIGHT 480

extern void R_Printf(int level, const char* msg, ...) __attribute__ ((format (printf, 2, 3)));

extern void LoadPCX(char *origname, byte **pic, byte **palette, int *width, int *height);
extern void GetPCXInfo(char *filename, int *width, int *height);

extern qboolean LoadSTB(const char *origname, const char* type, byte **pic, int *width, int *height);
extern qboolean ResizeSTB(byte *input_pixels, int input_width, int input_height,
			  byte *output_pixels, int output_width, int output_height);
extern void SmoothColorImage(unsigned *dst, size_t size, size_t rstep);
extern void scale2x(byte *src, byte *dst, int width, int height);
extern void scale3x(byte *src, byte *dst, int width, int height);

extern void GetWalInfo(char *name, int *width, int *height);

extern float Mod_RadiusFromBounds(const vec3_t mins, const vec3_t maxs);
extern byte* Mod_DecompressVis(byte *in, int row);
#endif /* SRC_CLIENT_REFRESH_REF_SHARED_H_ */
