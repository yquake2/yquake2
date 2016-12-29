/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016 Daniel Gibson
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
 * Texture handling for OpenGL3
 *
 * =======================================================================
 */

#include "header/local.h"

typedef struct
{
	char *name;
	int minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int gl_filter_max = GL_LINEAR;

void
GL3_TextureMode(char *string)
{
	const int num_modes = sizeof(modes)/sizeof(modes[0]);
	int i;

	for (i = 0; i < num_modes; i++)
	{
		if (!Q_stricmp(modes[i].name, string))
		{
			break;
		}
	}

	if (i == num_modes)
	{
		R_Printf(PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	/* clamp selected anisotropy */
	if (gl3config.anisotropic)
	{
		if (gl_anisotropic->value > gl3config.max_anisotropy)
		{
			ri.Cvar_SetValue("gl_anisotropic", gl3config.max_anisotropy);
		}
		else if (gl_anisotropic->value < 1.0)
		{
			ri.Cvar_SetValue("gl_anisotropic", 1.0);
		}
	}
	else
	{
		ri.Cvar_SetValue("gl_anisotropic", 0.0);
	}

#if 0 // TODO!
	image_t *glt;

	/* change all the existing mipmap texture objects */
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if ((glt->type != it_pic) && (glt->type != it_sky))
		{
			R_Bind(glt->texnum);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
					gl_filter_min);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
					gl_filter_max);

			/* Set anisotropic filter if supported and enabled */
			if (gl_config.anisotropic && gl_anisotropic->value)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
						gl_anisotropic->value);
			}
		}
	}
#endif // 0
}

/*
static gl3image_t *
LoadWal(char *origname)
{
	miptex_t *mt;
	int width, height, ofs;
	gl3image_t *image;
	char name[256];

	Q_strlcpy(name, origname, sizeof(name));

	/* Add the extension */
	if (strcmp(COM_FileExtension(name), "wal"))
	{
		Q_strlcat(name, ".wal", sizeof(name));
	}

	ri.FS_LoadFile(name, (void **)&mt);

	if (!mt)
	{
		R_Printf(PRINT_ALL, "LoadWal: can't load %s\n", name);
		return gl3_notexture;
	}

	width = LittleLong(mt->width);
	height = LittleLong(mt->height);
	ofs = LittleLong(mt->offsets[0]);

	image = GL3_LoadPic(name, (byte *)mt + ofs, width, 0, height, 0, it_wall, 8);

	ri.FS_FreeFile((void *)mt);

	return image;
}

/*
 * Finds or loads the given image
 */
gl3image_t *
GL3_FindImage(char *name, imagetype_t type)
{
#if 0
	gl3image_t *image;
	int i, len;
	byte *pic, *palette;
	int width, height;
	char *ptr;
	char namewe[256];
	int realwidth = 0, realheight = 0;
	const char* ext;

	if (!name)
	{
		return NULL;
	}

	ext = COM_FileExtension(name);
	if(!ext[0])
	{
		/* file has no extension */
		return NULL;
	}

	len = strlen(name);

	/* Remove the extension */
	memset(namewe, 0, 256);
	memcpy(namewe, name, len - 4);

	if (len < 5)
	{
		return NULL;
	}

	/* fix backslashes */
	while ((ptr = strchr(name, '\\')))
	{
		*ptr = '/';
	}

	/* look for it */
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!strcmp(name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	/* load the pic from disk */
	pic = NULL;
	palette = NULL;

	if (strcmp(ext, "pcx") == 0)
	{
		if (gl_retexturing->value)
		{
			GetPCXInfo(name, &realwidth, &realheight);
			if(realwidth == 0)
			{
				/* No texture found */
				return NULL;
			}

			/* try to load a tga, png or jpg (in that order/priority) */
			if (  LoadSTB(namewe, "tga", &pic, &width, &height)
			   || LoadSTB(namewe, "png", &pic, &width, &height)
			   || LoadSTB(namewe, "jpg", &pic, &width, &height) )
			{
				/* upload tga or png or jpg */
				image = R_LoadPic(name, pic, width, realwidth, height,
						realheight, type, 32);
			}
			else
			{
				/* PCX if no TGA/PNG/JPEG available (exists always) */
				LoadPCX(name, &pic, &palette, &width, &height);

				if (!pic)
				{
					/* No texture found */
					return NULL;
				}

				/* Upload the PCX */
				image = R_LoadPic(name, pic, width, 0, height, 0, type, 8);
			}
		}
		else /* gl_retexture is not set */
		{
			LoadPCX(name, &pic, &palette, &width, &height);

			if (!pic)
			{
				return NULL;
			}

			image = R_LoadPic(name, pic, width, 0, height, 0, type, 8);
		}
	}
	else if (strcmp(ext, "wal") == 0)
	{
		if (gl_retexturing->value)
		{
			/* Get size of the original texture */
			GetWalInfo(name, &realwidth, &realheight);
			if(realwidth == 0)
			{
				/* No texture found */
				return NULL;
			}

			/* try to load a tga, png or jpg (in that order/priority) */
			if (  LoadSTB(namewe, "tga", &pic, &width, &height)
			   || LoadSTB(namewe, "png", &pic, &width, &height)
			   || LoadSTB(namewe, "jpg", &pic, &width, &height) )
			{
				/* upload tga or png or jpg */
				image = R_LoadPic(name, pic, width, realwidth, height,
						realheight, type, 32);
			}
			else
			{
				/* WAL if no TGA/PNG/JPEG available (exists always) */
				image = LoadWal(namewe);
			}

			if (!image)
			{
				/* No texture found */
				return NULL;
			}
		}
		else /* gl_retexture is not set */
		{
			image = LoadWal(name);

			if (!image)
			{
				/* No texture found */
				return NULL;
			}
		}
	}
	else if (strcmp(ext, "tga") == 0 || strcmp(ext, "png") == 0 || strcmp(ext, "jpg") == 0)
	{
		char tmp_name[256];

		realwidth = 0;
		realheight = 0;

		strcpy(tmp_name, namewe);
		strcat(tmp_name, ".wal");
		GetWalInfo(tmp_name, &realwidth, &realheight);

		if (realwidth == 0 || realheight == 0) {
			/* It's a sky or model skin. */
			strcpy(tmp_name, namewe);
			strcat(tmp_name, ".pcx");
			GetPCXInfo(tmp_name, &realwidth, &realheight);
		}

		/* TODO: not sure if not having realwidth/heigth is bad - a tga/png/jpg
		 * was requested, after all, so there might be no corresponding wal/pcx?
		 * if (realwidth == 0 || realheight == 0) return NULL;
		 */

		LoadSTB(name, ext, &pic, &width, &height);
		image = R_LoadPic(name, pic, width, realwidth,
				height, realheight, type, 32);
	}
	else
	{
		return NULL;
	}

	if (pic)
	{
		free(pic);
	}

	if (palette)
	{
		free(palette);
	}

	return image;
#endif // 0
	return NULL;
}

gl3image_t *
GL3_RegisterSkin(char *name)
{
	return GL3_FindImage(name, it_skin);
}



void
GL3_ImageList_f(void)
{
	R_Printf(PRINT_ALL, "TODO: Implement R_ImageList_f()\n");
#if 0 // TODO!
	int i;
	image_t *image;
	int texels;
	const char *palstrings[2] = {
		"RGB",
		"PAL"
	};

	R_Printf(PRINT_ALL, "------------------\n");
	texels = 0;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (image->texnum <= 0)
		{
			continue;
		}

		texels += image->upload_width * image->upload_height;

		switch (image->type)
		{
			case it_skin:
				R_Printf(PRINT_ALL, "M");
				break;
			case it_sprite:
				R_Printf(PRINT_ALL, "S");
				break;
			case it_wall:
				R_Printf(PRINT_ALL, "W");
				break;
			case it_pic:
				R_Printf(PRINT_ALL, "P");
				break;
			default:
				R_Printf(PRINT_ALL, " ");
				break;
		}

		R_Printf(PRINT_ALL, " %3i %3i %s: %s\n",
				image->upload_width, image->upload_height,
				palstrings[image->paletted], image->name);
	}

	R_Printf(PRINT_ALL, "Total texel count (not counting mipmaps): %i\n", texels);
#endif
}
