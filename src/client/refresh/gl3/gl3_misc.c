/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016-2017 Daniel Gibson
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
 * Misc OpenGL3 refresher functions
 *
 * =======================================================================
 */

#include "header/local.h"

gl3image_t *gl3_notexture; /* use for bad textures */
gl3image_t *gl3_particletexture; /* little dot for particles */

void
GL3_SetDefaultState(void)
{
	glClearColor(1, 0, 0.5, 0.5);
	glDisable(GL_MULTISAMPLE);
	glCullFace(GL_FRONT);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// TODO: gl_texturemode, gl1_texturealphamode?
	//GL3_TextureMode(gl_texturemode->string);
	//R_TextureAlphaMode(gl1_texturealphamode->string);
	//R_TextureSolidMode(gl1_texturesolidmode->string);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (gl_msaa_samples->value)
	{
		glEnable(GL_MULTISAMPLE);
		// glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_NICEST); TODO what is this for?
	}
}

static byte dottexture[8][8] = {
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 1, 1, 0, 0, 0, 0},
	{0, 1, 1, 1, 1, 0, 0, 0},
	{0, 1, 1, 1, 1, 0, 0, 0},
	{0, 0, 1, 1, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
};

void
GL3_InitParticleTexture(void)
{
	int x, y;
	byte data[8][8][4];

	/* particle texture */
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y] * 255;
		}
	}

	gl3_particletexture = GL3_LoadPic("***particle***", (byte *)data,
	                                  8, 0, 8, 0, it_sprite, 32);

	/* also use this for bad textures, but without alpha */
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = dottexture[x & 3][y & 3] * 255;
			data[y][x][1] = 0;
			data[y][x][2] = 0;
			data[y][x][3] = 255;
		}
	}

	gl3_notexture = GL3_LoadPic("***r_notexture***", (byte *)data,
	                            8, 0, 8, 0, it_wall, 32);
}

void
GL3_ScreenShot(void)
{
	int w=vid.width, h=vid.height;
	byte *buffer = malloc(w*h*3);

	if (!buffer)
	{
		R_Printf(PRINT_ALL, "GL3_ScreenShot: Couldn't malloc %d bytes\n", w*h*3);
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
