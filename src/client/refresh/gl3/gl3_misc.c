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
	//glEnable(GL_TEXTURE_2D);

	// TODO: this must be done in shader instead,
	// see https://www.khronos.org/opengl/wiki/Transparency_Sorting#Alpha_test
	//glEnable(GL_ALPHA_TEST);
	//glAlphaFunc(GL_GREATER, 0.666);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	//glColor4f(1, 1, 1, 1);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	//glShadeModel(GL_FLAT);

	//GL3_TextureMode(gl_texturemode->string);
	STUB("call GL3_TextureMode(gl_texturemode->string);");
	STUB("Use gl_texturealphamode and somehow or only support one mode? => R_TextureAlphaMode(), R_TextureSolidMode()");
	//R_TextureAlphaMode(gl_texturealphamode->string);
	//R_TextureSolidMode(gl_texturesolidmode->string);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	STUB("need replacement for R_TexEnv(GL_REPLACE);");

	STUB("Apparently the GL_POINT stuff works differently in OpenGL3 core");
#if 0
	{
		// TODO: are we gonna use glPointParameter?
		float attenuations[3] = {0.01f, 0.0f, 0.01f};
		/*
		attenuations[0] = gl_particle_att_a->value;
		attenuations[1] = gl_particle_att_b->value;
		attenuations[2] = gl_particle_att_c->value;
		*/
		STUB("gl_particle_att_[abc], gl_particle_min/max_size ??");

		/* GL_POINT_SMOOTH is not implemented by some OpenGL
		   drivers, especially the crappy Mesa3D backends like
		   i915.so. That the points are squares and not circles
		   is not a problem by Quake II! */
		glEnable(GL_POINT_SMOOTH);
		glPointParameterf(GL_POINT_SIZE_MIN_EXT, 2.0f); // TODO was gl_particle_min_size->value);
		glPointParameterf(GL_POINT_SIZE_MAX_EXT, 40.0f); // TODO was gl_particle_max_size->value);
		glPointParameterf(GL_DISTANCE_ATTENUATION_EXT, attenuations);
	}
#endif // 0

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
	byte *buffer;
	char picname[80];
	char checkname[MAX_OSPATH];
	int i, c;
	FILE *f;

	/* FS_InitFilesystem() made sure the screenshots dir exists */

	/* find a file name to save it to */
	strcpy(picname, "quake00.tga");

	for (i = 0; i <= 99; i++)
	{
		picname[5] = i / 10 + '0';
		picname[6] = i % 10 + '0';
		Com_sprintf(checkname, sizeof(checkname), "%s/scrnshot/%s",
			   	ri.FS_Gamedir(), picname);
		f = fopen(checkname, "rb");

		if (!f)
		{
			break; /* file doesn't exist */
		}

		fclose(f);
	}

	if (i == 100)
	{
		R_Printf(PRINT_ALL, "SCR_ScreenShot_f: Couldn't create a file\n");
		return;
	}

	static const int headerLength = 18+4;

	c = headerLength + vid.width * vid.height * 3;

	buffer = malloc(c);
	if (!buffer)
	{
		R_Printf(PRINT_ALL, "SCR_ScreenShot_f: Couldn't malloc %d bytes\n", c);
		return;
	}

	memset(buffer, 0, headerLength);
	buffer[0] = 4; // image ID: "yq2\0"
	buffer[2] = 2; /* uncompressed type */
	buffer[12] = vid.width & 255;
	buffer[13] = vid.width >> 8;
	buffer[14] = vid.height & 255;
	buffer[15] = vid.height >> 8;
	buffer[16] = 24; /* pixel size */
	buffer[17] = 0; // image descriptor
	buffer[18] = 'y'; // following: the 4 image ID fields
	buffer[19] = 'q';
	buffer[20] = '2';
	buffer[21] = '\0';

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, vid.width, vid.height, GL_BGR,
			GL_UNSIGNED_BYTE, buffer + headerLength);

	f = fopen(checkname, "wb");
	if (f)
	{
		fwrite(buffer, 1, c, f);
		fclose(f);
		R_Printf(PRINT_ALL, "Wrote %s\n", picname);
	}
	else
	{
		R_Printf(PRINT_ALL, "SCR_ScreenShot_f: Couldn't write %s\n", picname);
	}

	free(buffer);
}
