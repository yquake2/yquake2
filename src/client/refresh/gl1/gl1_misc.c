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
 * Misc refresher functions
 *
 * =======================================================================
 */

#include "header/local.h"

static byte dottexture[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 1, 2, 3, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 3, 3, 3, 3, 3, 3, 1, 0, 0, 0, 0, 0, 0, 0},
	{0, 2, 3, 3, 3, 3, 3, 3, 2, 0, 0, 0, 0, 0, 0, 0},
	{0, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0},
	{0, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0},
	{0, 2, 3, 3, 3, 3, 3, 3, 2, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 3, 3, 3, 3, 3, 3, 1, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 1, 2, 3, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

static byte squaretexture[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0},
	{0, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0},
	{0, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0},
	{0, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0},
	{0, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0},
	{0, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0},
	{0, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0},
	{0, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

static byte notex[4][4] = {
	{0, 0, 0, 0},
	{0, 0, 1, 1},
	{0, 1, 1, 1},
	{0, 1, 1, 1}
};

typedef struct _TargaHeader
{
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;

void
R_InitParticleTexture(void)
{
	int x, y;
	byte partData[16][16][4];
	byte notexData[8][8][4];

	/* particle texture */
	for (x = 0; x < 16; x++)
	{
		for (y = 0; y < 16; y++)
		{
			partData[y][x][0] = 255;
			partData[y][x][1] = 255;
			partData[y][x][2] = 255;

			if (!gl1_particle_square->value)
			{
				partData[y][x][3] = dottexture[x][y] * 85;
			}
			else
			{
				partData[y][x][3] = squaretexture[x][y] * 85;
			}
		}
	}

	r_particletexture = R_LoadPic("***particle***", (byte *)partData,
	                              16, 0, 16, 0, it_sprite, 32);

	/* also use this for bad textures, but without alpha */
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			notexData[y][x][0] = notex[x & 3][y & 3] * 255;
			notexData[y][x][1] = 0;
			notexData[y][x][2] = 0;
			notexData[y][x][3] = 255;
		}
	}

	r_notexture = R_LoadPic("***r_notexture***", (byte *)notexData,
	                        8, 0, 8, 0, it_wall, 32);
}

void
R_ScreenShot(void)
{
	int w=vid.width, h=vid.height;
	byte *buffer = malloc(w*h*3);

	if (!buffer)
	{
		R_Printf(PRINT_ALL, "R_ScreenShot: Couldn't malloc %d bytes\n", w*h*3);
		return;
	}

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, buffer);

	// the pixels are now row-wise left to right, bottom to top,
	// but we need them row-wise left to right, top to bottom.
	// so swap bottom rows with top rows
	{
		size_t bytesPerRow = 3*w;
		byte rowBuffer[bytesPerRow];
		byte *curRowL = buffer; // first byte of first row
		byte *curRowH = buffer + bytesPerRow*(h-1); // first byte of last row
		while(curRowL < curRowH)
		{
			memcpy(rowBuffer, curRowL, bytesPerRow);
			memcpy(curRowL, curRowH, bytesPerRow);
			memcpy(curRowH, rowBuffer, bytesPerRow);

			curRowL += bytesPerRow;
			curRowH -= bytesPerRow;
		}
	}

	ri.Vid_WriteScreenshot(w, h, 3, buffer);

	free(buffer);
}

void
R_Strings(void)
{
	R_Printf(PRINT_ALL, "GL_VENDOR: %s\n", gl_config.vendor_string);
	R_Printf(PRINT_ALL, "GL_RENDERER: %s\n", gl_config.renderer_string);
	R_Printf(PRINT_ALL, "GL_VERSION: %s\n", gl_config.version_string);
	R_Printf(PRINT_ALL, "GL_EXTENSIONS: %s\n", gl_config.extensions_string);
}

void
R_SetDefaultState(void)
{
	glClearColor(1, 0, 0.5, 0.5);
	glDisable(GL_MULTISAMPLE);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	glColor4f(1, 1, 1, 1);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_FLAT);

	R_TextureMode(gl_texturemode->string);
	R_TextureAlphaMode(gl1_texturealphamode->string);
	R_TextureSolidMode(gl1_texturesolidmode->string);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_TexEnv(GL_REPLACE);

	if (gl_config.pointparameters)
	{
		float attenuations[3];

		attenuations[0] = gl1_particle_att_a->value;
		attenuations[1] = gl1_particle_att_b->value;
		attenuations[2] = gl1_particle_att_c->value;

		qglPointParameterfARB(GL_POINT_SIZE_MIN_EXT, gl1_particle_min_size->value);
		qglPointParameterfARB(GL_POINT_SIZE_MAX_EXT, gl1_particle_max_size->value);
		qglPointParameterfvARB(GL_DISTANCE_ATTENUATION_EXT, attenuations);
	}

	if (gl_config.palettedtexture)
	{
		glEnable(GL_SHARED_TEXTURE_PALETTE_EXT);
		R_SetTexturePalette(d_8to24table);
	}

	if (gl_msaa_samples->value)
	{
		glEnable(GL_MULTISAMPLE);
		glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_NICEST);
	}
}
