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

gl3image_t gl3textures[MAX_GL3TEXTURES];
int numgl3textures;

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
		R_Printf(PRINT_ALL, "bad filter name '%s' (probably from gl_texturemode)\n", string);
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

	gl3image_t *glt;

	const char* nolerplist = gl_nolerp_list->string;

	/* change all the existing texture objects */
	for (i = 0, glt = gl3textures; i < numgl3textures; i++, glt++)
	{
		if (nolerplist != NULL && strstr(nolerplist, glt->name) != NULL)
		{
			continue; /* those (by default: font and crosshairs) always only use GL_NEAREST */
		}

		GL3_SelectTMU(GL_TEXTURE0);
		GL3_Bind(glt->texnum);
		if ((glt->type != it_pic) && (glt->type != it_sky)) /* mipmapped texture */
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

			/* Set anisotropic filter if supported and enabled */
			if (gl3config.anisotropic && gl_anisotropic->value)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropic->value);
			}
		}
		else /* texture has no mipmaps */
		{
			// we can't use gl_filter_min which might be GL_*_MIPMAP_*
			// also, there's no anisotropic filtering for textures w/o mipmaps
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

void
GL3_Bind(GLuint texnum)
{
	extern gl3image_t *draw_chars;

	if (gl_nobind->value && draw_chars) /* performance evaluation option */
	{
		texnum = draw_chars->texnum;
	}

	if (gl3state.currenttexture == texnum)
	{
		return;
	}

	gl3state.currenttexture = texnum;
	GL3_SelectTMU(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texnum);
}

void
GL3_BindLightmap(int lightmapnum)
{
	int i=0;
	if(lightmapnum < 0 || lightmapnum >= MAX_LIGHTMAPS)
	{
		R_Printf(PRINT_ALL, "WARNING: Invalid lightmapnum %i used!\n", lightmapnum);
		return;
	}

	if (gl3state.currentlightmap == lightmapnum)
	{
		return;
	}

	gl3state.currentlightmap = lightmapnum;
	for(i=0; i<MAX_LIGHTMAPS_PER_SURFACE; ++i)
	{
		// this assumes that GL_TEXTURE<i+1> = GL_TEXTURE<i> + 1
		// at least for GL_TEXTURE0 .. GL_TEXTURE31 that's true
		GL3_SelectTMU(GL_TEXTURE1+i);
		glBindTexture(GL_TEXTURE_2D, gl3state.lightmap_textureIDs[lightmapnum][i]);
	}
}

/*
 * Returns has_alpha
 */
qboolean
GL3_Upload32(unsigned *data, int width, int height, qboolean mipmap)
{
	qboolean res;

	int samples;
	int i, c;
	byte *scan;
	int comp;

	c = width * height;
	scan = ((byte *)data) + 3;
	samples = gl3_solid_format;
	comp = gl3_tex_solid_format;

	for (i = 0; i < c; i++, scan += 4)
	{
		if (*scan != 255)
		{
			samples = gl3_alpha_format;
			comp = gl3_tex_alpha_format;
			break;
		}
	}

	glTexImage2D(GL_TEXTURE_2D, 0, comp, width, height,
	             0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	res = (samples == gl3_alpha_format);

	if (mipmap)
	{
		// TODO: some hardware may require mipmapping disabled for NPOT textures!
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else // if the texture has no mipmaps, we can't use gl_filter_min which might be GL_*_MIPMAP_*
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

	if (mipmap && gl3config.anisotropic && gl_anisotropic->value)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropic->value);
	}

	return res;
}


/*
 * Returns has_alpha
 */
qboolean
GL3_Upload8(byte *data, int width, int height, qboolean mipmap, qboolean is_sky)
{
	unsigned trans[512 * 256];
	int i, s;
	int p;

	s = width * height;

	if (s > sizeof(trans) / 4)
	{
		ri.Sys_Error(ERR_DROP, "GL3_Upload8: too large");
	}

	for (i = 0; i < s; i++)
	{
		p = data[i];
		trans[i] = d_8to24table[p];

		/* transparent, so scan around for
		   another color to avoid alpha fringes */
		if (p == 255)
		{
			if ((i > width) && (data[i - width] != 255))
			{
				p = data[i - width];
			}
			else if ((i < s - width) && (data[i + width] != 255))
			{
				p = data[i + width];
			}
			else if ((i > 0) && (data[i - 1] != 255))
			{
				p = data[i - 1];
			}
			else if ((i < s - 1) && (data[i + 1] != 255))
			{
				p = data[i + 1];
			}
			else
			{
				p = 0;
			}

			/* copy rgb components */
			((byte *)&trans[i])[0] = ((byte *)&d_8to24table[p])[0];
			((byte *)&trans[i])[1] = ((byte *)&d_8to24table[p])[1];
			((byte *)&trans[i])[2] = ((byte *)&d_8to24table[p])[2];
		}
	}

	return GL3_Upload32(trans, width, height, mipmap);
}

typedef struct
{
	short x, y;
} floodfill_t;

/* must be a power of 2 */
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP(off, dx, dy)	\
	{ \
		if (pos[off] == fillcolor) \
		{ \
			pos[off] = 255;	\
			fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
			inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
		} \
		else if (pos[off] != 255) \
		{ \
			fdc = pos[off];	\
		} \
	}

/*
 * Fill background pixels so mipmapping doesn't have haloes
 */
static void
FloodFillSkin(byte *skin, int skinwidth, int skinheight)
{
	byte fillcolor = *skin; /* assume this is the pixel to fill */
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int inpt = 0, outpt = 0;
	int filledcolor = 0;
	int i;

	/* attempt to find opaque black */
	for (i = 0; i < 256; ++i)
	{
		if (LittleLong(d_8to24table[i]) == (255 << 0)) /* alpha 1.0 */
		{
			filledcolor = i;
			break;
		}
	}

	/* can't fill to filled color or to transparent color (used as visited marker) */
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int x = fifo[outpt].x, y = fifo[outpt].y;
		int fdc = filledcolor;
		byte *pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)
		{
			FLOODFILL_STEP(-1, -1, 0);
		}

		if (x < skinwidth - 1)
		{
			FLOODFILL_STEP(1, 1, 0);
		}

		if (y > 0)
		{
			FLOODFILL_STEP(-skinwidth, 0, -1);
		}

		if (y < skinheight - 1)
		{
			FLOODFILL_STEP(skinwidth, 0, 1);
		}

		skin[x + skinwidth * y] = fdc;
	}
}

/*
 * This is also used as an entry point for the generated r_notexture
 */
gl3image_t *
GL3_LoadPic(char *name, byte *pic, int width, int realwidth,
            int height, int realheight, imagetype_t type, int bits)
{
	gl3image_t *image = NULL;
	GLuint texNum=0;
	int i;

	qboolean nolerp = false;

	if (gl_nolerp_list != NULL && gl_nolerp_list->string != NULL)
	{
		nolerp = strstr(gl_nolerp_list->string, name) != NULL;
	}
	/* find a free gl3image_t */
	for (i = 0, image = gl3textures; i < numgl3textures; i++, image++)
	{
		if (image->texnum == 0)
		{
			break;
		}
	}

	if (i == numgl3textures)
	{
		if (numgl3textures == MAX_GL3TEXTURES)
		{
			ri.Sys_Error(ERR_DROP, "MAX_GLTEXTURES");
		}

		numgl3textures++;
	}

	image = &gl3textures[i];

	if (strlen(name) >= sizeof(image->name))
	{
		ri.Sys_Error(ERR_DROP, "GL3_LoadPic: \"%s\" is too long", name);
	}

	strcpy(image->name, name);
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->type = type;

	if ((type == it_skin) && (bits == 8))
	{
		FloodFillSkin(pic, width, height);

	}

	// image->scrap = false; // TODO: reintroduce scrap? would allow optimizations in 2D rendering..

	glGenTextures(1, &texNum);

	image->texnum = texNum;

	GL3_SelectTMU(GL_TEXTURE0);
	GL3_Bind(texNum);

	if (bits == 8)
	{
		image->has_alpha = GL3_Upload8(pic, width, height,
					(image->type != it_pic && image->type != it_sky),
					image->type == it_sky);
	}
	else
	{
		image->has_alpha = GL3_Upload32((unsigned *)pic, width, height,
					(image->type != it_pic && image->type != it_sky));
	}

	if (realwidth && realheight)
	{
		if ((realwidth <= image->width) && (realheight <= image->height))
		{
			image->width = realwidth;
			image->height = realheight;
		}
		else
		{
			R_Printf(PRINT_DEVELOPER,
					"Warning, image '%s' has hi-res replacement smaller than the original! (%d x %d) < (%d x %d)\n",
					name, image->width, image->height, realwidth, realheight);
		}
	}

	image->sl = 0;
	image->sh = 1;
	image->tl = 0;
	image->th = 1;

	if (nolerp)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
#if 0 // TODO: the scrap could allow batch rendering 2D stuff? not sure it's worth the hassle..
	/* load little pics into the scrap */
	if (!nolerp && (image->type == it_pic) && (bits == 8) &&
		(image->width < 64) && (image->height < 64))
	{
		int x, y;
		int i, j, k;
		int texnum;

		texnum = Scrap_AllocBlock(image->width, image->height, &x, &y);

		if (texnum == -1)
		{
			goto nonscrap;
		}

		scrap_dirty = true;

		/* copy the texels into the scrap block */
		k = 0;

		for (i = 0; i < image->height; i++)
		{
			for (j = 0; j < image->width; j++, k++)
			{
				scrap_texels[texnum][(y + i) * BLOCK_WIDTH + x + j] = pic[k];
			}
		}

		image->texnum = TEXNUM_SCRAPS + texnum;
		image->scrap = true;
		image->has_alpha = true;
		image->sl = (x + 0.01) / (float)BLOCK_WIDTH;
		image->sh = (x + image->width - 0.01) / (float)BLOCK_WIDTH;
		image->tl = (y + 0.01) / (float)BLOCK_WIDTH;
		image->th = (y + image->height - 0.01) / (float)BLOCK_WIDTH;
	}
	else
	{
	nonscrap:
		image->scrap = false;
		image->texnum = TEXNUM_IMAGES + (image - gltextures);
		R_Bind(image->texnum);

		if (bits == 8)
		{
			image->has_alpha = R_Upload8(pic, width, height,
						(image->type != it_pic && image->type != it_sky),
						image->type == it_sky);
		}
		else
		{
			image->has_alpha = R_Upload32((unsigned *)pic, width, height,
						(image->type != it_pic && image->type != it_sky));
		}

		image->upload_width = upload_width; /* after power of 2 and scales */
		image->upload_height = upload_height;
		image->paletted = uploaded_paletted;

		if (realwidth && realheight)
		{
			if ((realwidth <= image->width) && (realheight <= image->height))
			{
				image->width = realwidth;
				image->height = realheight;
			}
			else
			{
				R_Printf(PRINT_DEVELOPER,
						"Warning, image '%s' has hi-res replacement smaller than the original! (%d x %d) < (%d x %d)\n",
						name, image->width, image->height, realwidth, realheight);
			}
		}

		image->sl = 0;
		image->sh = 1;
		image->tl = 0;
		image->th = 1;

		if (nolerp)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
	}
#endif // 0
	return image;
}

static gl3image_t *
LoadWal(char *origname, imagetype_t type)
{
	miptex_t *mt;
	int width, height, ofs, size;
	gl3image_t *image;
	char name[256];

	Q_strlcpy(name, origname, sizeof(name));

	/* Add the extension */
	if (strcmp(COM_FileExtension(name), "wal"))
	{
		Q_strlcat(name, ".wal", sizeof(name));
	}

	size = ri.FS_LoadFile(name, (void **)&mt);

	if (!mt)
	{
		R_Printf(PRINT_ALL, "LoadWal: can't load %s\n", name);
		return gl3_notexture;
	}

	if (size < sizeof(miptex_t))
	{
		R_Printf(PRINT_ALL, "LoadWal: can't load %s, small header\n", name);
		ri.FS_FreeFile((void *)mt);
		return gl3_notexture;
	}

	width = LittleLong(mt->width);
	height = LittleLong(mt->height);
	ofs = LittleLong(mt->offsets[0]);

	if ((ofs <= 0) || (width <= 0) || (height <= 0) ||
	    (((size - ofs) / height) < width))
	{
		R_Printf(PRINT_ALL, "LoadWal: can't load %s, small body\n", name);
		ri.FS_FreeFile((void *)mt);
		return gl3_notexture;
	}

	image = GL3_LoadPic(name, (byte *)mt + ofs, width, 0, height, 0, type, 8);

	ri.FS_FreeFile((void *)mt);

	return image;
}

/*
 * Finds or loads the given image
 */
gl3image_t *
GL3_FindImage(char *name, imagetype_t type)
{
	gl3image_t *image;
	int i, len;
	byte *pic;
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
	memcpy(namewe, name, len - (strlen(ext) + 1));

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
	for (i = 0, image = gl3textures; i < numgl3textures; i++, image++)
	{
		if (!strcmp(name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	/* load the pic from disk */
	pic = NULL;

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
				image = GL3_LoadPic(name, pic, width, realwidth, height,
						realheight, type, 32);
			}
			else
			{
				/* PCX if no TGA/PNG/JPEG available (exists always) */
				LoadPCX(name, &pic, NULL, &width, &height);

				if (!pic)
				{
					/* No texture found */
					return NULL;
				}

				/* Upload the PCX */
				image = GL3_LoadPic(name, pic, width, 0, height, 0, type, 8);
			}
		}
		else /* gl_retexture is not set */
		{
			LoadPCX(name, &pic, NULL, &width, &height);

			if (!pic)
			{
				return NULL;
			}

			image = GL3_LoadPic(name, pic, width, 0, height, 0, type, 8);
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
				image = GL3_LoadPic(name, pic, width, realwidth, height, realheight, type, 32);
			}
			else
			{
				/* WAL if no TGA/PNG/JPEG available (exists always) */
				image = LoadWal(namewe, type);
			}

			if (!image)
			{
				/* No texture found */
				return NULL;
			}
		}
		else /* gl_retexture is not set */
		{
			image = LoadWal(name, type);

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

		if(LoadSTB(name, ext, &pic, &width, &height))
		{
			image = GL3_LoadPic(name, pic, width, realwidth, height, realheight, type, 32);
		}
	}
	else
	{
		return NULL;
	}

	if (pic)
	{
		free(pic);
	}

	return image;
}

gl3image_t *
GL3_RegisterSkin(char *name)
{
	return GL3_FindImage(name, it_skin);
}

/*
 * Any image that was not touched on
 * this registration sequence
 * will be freed.
 */
void
GL3_FreeUnusedImages(void)
{
	int i;
	gl3image_t *image;

	/* never free r_notexture or particle texture */
	gl3_notexture->registration_sequence = registration_sequence;
	gl3_particletexture->registration_sequence = registration_sequence;

	for (i = 0, image = gl3textures; i < numgl3textures; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
		{
			continue; /* used this sequence */
		}

		if (!image->registration_sequence)
		{
			continue; /* free image_t slot */
		}

		if (image->type == it_pic)
		{
			continue; /* don't free pics */
		}

		/* free it */
		glDeleteTextures(1, &image->texnum);
		memset(image, 0, sizeof(*image));
	}
}

void
GL3_ShutdownImages(void)
{
	int i;
	gl3image_t *image;

	for (i = 0, image = gl3textures; i < numgl3textures; i++, image++)
	{
		if (!image->registration_sequence)
		{
			continue; /* free image_t slot */
		}

		/* free it */
		glDeleteTextures(1, &image->texnum);
		memset(image, 0, sizeof(*image));
	}
}

static qboolean IsNPOT(int v)
{
	unsigned int uv = v;
	// just try all the power of two values between 1 and 1 << 15 (32k)
	for(unsigned int i=0; i<16; ++i)
	{
		unsigned int pot = (1u << i);
		if(uv & pot)
		{
			return uv != pot;
		}
	}

	return true;
}

void
GL3_ImageList_f(void)
{
	int i, texels=0;
	gl3image_t *image;
	const char *formatstrings[2] = {
		"RGB ",
		"RGBA"
	};

	const char* potstrings[2] = {
		" POT", "NPOT"
	};

	R_Printf(PRINT_ALL, "------------------\n");

	for (i = 0, image = gl3textures; i < numgl3textures; i++, image++)
	{
		int w, h;
		qboolean isNPOT = false;
		if (image->texnum == 0)
		{
			continue;
		}
		w = image->width;
		h = image->height;

		isNPOT = IsNPOT(w) || IsNPOT(h);

		texels += w*h;

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
			case it_sky:
				R_Printf(PRINT_ALL, "Y");
				break;
			default:
				R_Printf(PRINT_ALL, "?");
				break;
		}

		R_Printf(PRINT_ALL, " %3i %3i %s %s: %s\n", w, h,
		         formatstrings[image->has_alpha], potstrings[isNPOT], image->name);
	}

	R_Printf(PRINT_ALL, "Total texel count (not counting mipmaps): %i\n", texels);
}
