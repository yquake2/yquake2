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
 * Texture handling
 *
 * =======================================================================
 */

#include "header/local.h"

image_t gltextures[MAX_GLTEXTURES];
int numgltextures;
static int image_max = 0;
extern qboolean scrap_dirty;
extern byte scrap_texels[MAX_SCRAPS][SCRAP_WIDTH * SCRAP_HEIGHT];

static byte intensitytable[256];
unsigned char gammatable[256];

static cvar_t *intensity;

unsigned d_8to24table[256];

extern cvar_t *gl1_minlight;
extern unsigned char minlight[256];

qboolean R_Upload8(byte *data, int width, int height,
		qboolean mipmap, qboolean is_sky);
qboolean R_Upload32(unsigned *data, int width, int height, qboolean mipmap);

#define Q2_GL_SOLID_FORMAT GL_RGB
#define Q2_GL_ALPHA_FORMAT GL_RGBA

#ifdef YQ2_GL1_GLES
#define DEFAULT_SOLID_FORMAT GL_RGBA
#else
#define DEFAULT_SOLID_FORMAT GL_RGB
#endif

int gl_tex_solid_format = DEFAULT_SOLID_FORMAT;
int gl_tex_alpha_format = GL_RGBA;

#undef DEFAULT_SOLID_FORMAT

int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int gl_filter_max = GL_LINEAR;

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

#define NUM_GL_MODES ARRLEN(modes)

typedef struct
{
	char *name;
	int mode;
} gltmode_t;

#ifdef YQ2_GL1_GLES

gltmode_t gl_alpha_modes[] = {
	{"default", GL_RGBA},
	{"GL_RGBA", GL_RGBA},
};

gltmode_t gl_solid_modes[] = {
	{"default", GL_RGBA},
	{"GL_RGBA", GL_RGBA},
};

#else

gltmode_t gl_alpha_modes[] = {
	{"default", GL_RGBA},
	{"GL_RGBA", GL_RGBA},
	{"GL_RGBA8", GL_RGBA8},
	{"GL_RGB5_A1", GL_RGB5_A1},
	{"GL_RGBA4", GL_RGBA4},
	{"GL_RGBA2", GL_RGBA2},
};

gltmode_t gl_solid_modes[] = {
	{"default", GL_RGB},
	{"GL_RGB", GL_RGB},
	{"GL_RGB8", GL_RGB8},
	{"GL_RGB5", GL_RGB5},
	{"GL_RGB4", GL_RGB4},
	{"GL_R3_G3_B2", GL_R3_G3_B2},
};

#endif

#define NUM_GL_ALPHA_MODES ARRLEN(gl_alpha_modes)
#define NUM_GL_SOLID_MODES ARRLEN(gl_solid_modes)

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

static int upload_width, upload_height;
static qboolean uploaded_paletted;

void
R_SetTexturePalette(const unsigned palette[256])
{
	if (gl_config.palettedtexture)
	{
		unsigned char temptable[768];
		int i;

		for (i = 0; i < 256; i++)
		{
			temptable[i * 3 + 0] = (palette[i] >> 0) & 0xff;
			temptable[i * 3 + 1] = (palette[i] >> 8) & 0xff;
			temptable[i * 3 + 2] = (palette[i] >> 16) & 0xff;
		}

		qglColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB,
				256, GL_RGB, GL_UNSIGNED_BYTE, temptable);
	}
}

void
R_SelectTexture(GLenum texture)
{
	if (!gl_config.multitexture || gl_state.currenttarget == texture)
	{
		return;
	}

	gl_state.currenttmu = texture - GL_TEXTURE0;
	gl_state.currenttarget = texture;

	qglActiveTexture(texture);
	qglClientActiveTexture(texture);
}

void
R_TexEnv(GLenum mode)
{
	static int lastmodes[2] = {-1, -1};

	if (mode != lastmodes[gl_state.currenttmu])
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
		lastmodes[gl_state.currenttmu] = mode;
	}
}

qboolean
R_Bind(int texnum)
{
	extern image_t *draw_chars;

	if (gl_nobind->value && draw_chars) /* performance evaluation option */
	{
		texnum = draw_chars->texnum;
	}

	if (gl_state.currenttextures[gl_state.currenttmu] == texnum)
	{
		return false;
	}

	gl_state.currenttextures[gl_state.currenttmu] = texnum;
	glBindTexture(GL_TEXTURE_2D, texnum);
	return true;
}

void
R_MBind(GLenum target, int texnum)
{
	const int tmu = target - GL_TEXTURE0;

	if (target != gl_state.currenttarget)
	{
		R_SelectTexture(target);
	}

	if (gl_state.currenttextures[tmu] == texnum)
	{
		return;
	}

	R_Bind(texnum);
}

void
R_EnableMultitexture(qboolean enable)
{
	static qboolean active;

	if (!gl_config.multitexture || enable == active)
	{
		return;	// current state is the right one
	}

	active = enable;
	R_SelectTexture(GL_TEXTURE1);

	if (active && !r_fullbright->value)
	{
		glEnable(GL_TEXTURE_2D);

		if (gl_lightmap->value)
		{
			R_TexEnv(GL_REPLACE);
		}
		else
		{
			R_TexEnv(GL_MODULATE);
		}
	}
	else	// disable multitexturing
	{
		glDisable(GL_TEXTURE_2D);
		R_TexEnv(GL_REPLACE);
	}

	R_SelectTexture(GL_TEXTURE0);
	R_TexEnv(GL_REPLACE);
}

void
R_TextureMode(const char *string)
{
	int i;
	image_t *glt;

	for (i = 0; i < NUM_GL_MODES; i++)
	{
		if (!Q_stricmp(modes[i].name, string))
		{
			break;
		}
	}

	if (i == NUM_GL_MODES)
	{
		Com_Printf("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	/* clamp selected anisotropy */
	if (gl_config.anisotropic)
	{
		if (gl_anisotropic->value > gl_config.max_anisotropy)
		{
			ri.Cvar_SetValue("r_anisotropic", gl_config.max_anisotropy);
		}
	}
	else
	{
		ri.Cvar_SetValue("r_anisotropic", 0.0);
	}

	const char* nolerplist = gl_nolerp_list->string;
	const char* lerplist = r_lerp_list->string;
	qboolean unfiltered2D = r_2D_unfiltered->value != 0;

	/* change all the existing mipmap texture objects */
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		qboolean nolerp = false;
		/* r_2D_unfiltered and r_nolerp_list allow rendering stuff unfiltered even if gl_filter_* is filtered */
		if (unfiltered2D && glt->type == it_pic)
		{
			// exception to that exception: stuff on the r_lerp_list
			nolerp = (lerplist== NULL) || (strstr(lerplist, glt->name) == NULL);
		}
		else if(nolerplist != NULL && strstr(nolerplist, glt->name) != NULL)
		{
			nolerp = true;
		}

		if ( !R_Bind(glt->texnum) )
		{
			continue;	// don't bother changing anything if texture was already set
		}

		if ((glt->type != it_pic) && (glt->type != it_sky)) /* mipmapped texture */
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

			/* Set anisotropic filter if supported and enabled */
			if (gl_config.anisotropic && gl_anisotropic->value)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
						Q_max(gl_anisotropic->value, 1.f));
			}
		}
		else /* texture has no mipmaps */
		{
			if (nolerp)
			{
				// this texture shouldn't be filtered at all (no gl_nolerp_list or r_2D_unfiltered case)
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}
			else
			{
				// we can't use gl_filter_min which might be GL_*_MIPMAP_*
				// also, there's no anisotropic filtering for textures w/o mipmaps
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
			}
		}
	}
}

void
R_TextureAlphaMode(const char *string)
{
	int i;

	for (i = 0; i < NUM_GL_ALPHA_MODES; i++)
	{
		if (!Q_stricmp(gl_alpha_modes[i].name, string))
		{
			break;
		}
	}

	if (i == NUM_GL_ALPHA_MODES)
	{
		Com_Printf("bad alpha texture mode name\n");
		return;
	}

	gl_tex_alpha_format = gl_alpha_modes[i].mode;
}

void
R_TextureSolidMode(const char *string)
{
	int i;

	for (i = 0; i < NUM_GL_SOLID_MODES; i++)
	{
		if (!Q_stricmp(gl_solid_modes[i].name, string))
		{
			break;
		}
	}

	if (i == NUM_GL_SOLID_MODES)
	{
		Com_Printf("bad solid texture mode name\n");
		return;
	}

	gl_tex_solid_format = gl_solid_modes[i].mode;
}

void
R_ImageList_f(void)
{
	int i, used, texels;
	image_t *image;
	qboolean	freeup;
	const char *palstrings[2] = {
		"RGB",
		"PAL"
	};

	Com_Printf("------------------\n");
	texels = 0;
	used = 0;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		char *in_use = "";

		if (image->texnum <= 0)
		{
			continue;
		}

		if (image->registration_sequence == registration_sequence)
		{
			in_use = "*";
			used++;
		}

		texels += image->upload_width * image->upload_height;

		switch (image->type)
		{
			case it_skin:
				Com_Printf("M");
				break;
			case it_sprite:
				Com_Printf("S");
				break;
			case it_wall:
				Com_Printf("W");
				break;
			case it_pic:
				Com_Printf("P");
				break;
			default:
				Com_Printf(" ");
				break;
		}

		Com_Printf(" %3i %3i %s: %s (%dx%d) %s\n",
				image->upload_width, image->upload_height,
				palstrings[image->paletted], image->name,
				image->width, image->height, in_use);
	}

	Com_Printf("Total texel count (not counting mipmaps): %i\n",
			texels);
	freeup = R_ImageHasFreeSpace();
	Com_Printf("Used %d of %d images%s.\n", used, image_max, freeup ? ", has free space" : "");
}

/*
 * Fill background pixels so mipmapping doesn't have haloes
 */
static void
R_FloodFillSkin(byte *skin, int skinwidth, int skinheight)
{
	byte fillcolor = *skin; /* assume this is the pixel to fill */
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int inpt = 0, outpt = 0;
	int filledcolor = 0;
	int i;

	// NOTE: there was a if(filledcolor == -1) which didn't make sense b/c filledcolor used to be initialized to -1
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
 * Scale up the pixel values in a
 * texture to increase the
 * lighting range
 */
static void
R_LightScaleTexture(unsigned *in, int inwidth,
		int inheight, qboolean only_gamma)
{
	if (only_gamma)
	{
		int i, c;
		byte *p;

		p = (byte *)in;

		c = inwidth * inheight;

		for (i = 0; i < c; i++, p += 4)
		{
			p[0] = gammatable[p[0]];
			p[1] = gammatable[p[1]];
			p[2] = gammatable[p[2]];
		}
	}
	else
	{
		int i, c;
		byte *p;

		p = (byte *)in;

		c = inwidth * inheight;

		for (i = 0; i < c; i++, p += 4)
		{
			p[0] = gammatable[intensitytable[p[0]]];
			p[1] = gammatable[intensitytable[p[1]]];
			p[2] = gammatable[intensitytable[p[2]]];
		}
	}
}

/*
 * Operates in place, quartering the size of the texture
 */
static void
R_MipMap(byte *in, int width, int height)
{
	int i, j;
	byte *out;

	width <<= 2;
	height >>= 1;
	out = in;

	for (i = 0; i < height; i++, in += width)
	{
		for (j = 0; j < width; j += 8, out += 4, in += 8)
		{
			out[0] = (in[0] + in[4] + in[width + 0] + in[width + 4]) >> 2;
			out[1] = (in[1] + in[5] + in[width + 1] + in[width + 5]) >> 2;
			out[2] = (in[2] + in[6] + in[width + 2] + in[width + 6]) >> 2;
			out[3] = (in[3] + in[7] + in[width + 3] + in[width + 7]) >> 2;
		}
	}
}

/*
 * Returns has_alpha
 */
static void
R_BuildPalettedTexture(unsigned char *paletted_texture, unsigned char *scaled,
		int scaled_width, int scaled_height)
{
	int i;

	for (i = 0; i < scaled_width * scaled_height; i++)
	{
		unsigned int r, g, b, c;

		r = (scaled[0] >> 3) & 31;
		g = (scaled[1] >> 2) & 63;
		b = (scaled[2] >> 3) & 31;

		c = r | (g << 5) | (b << 11);

		paletted_texture[i] = gl_state.d_16to8table[c];

		scaled += 4;
	}
}

static qboolean
R_Upload32Native(unsigned *data, int width, int height, qboolean mipmap)
{
	// This is for GL 2.x so no palettes, no scaling, no messing around with the data here. :)
	int samples;
	int i, c;
	byte *scan;
	int comp;

	c = width * height;
	scan = ((byte *)data) + 3;
	samples = Q2_GL_SOLID_FORMAT;
	comp = gl_tex_solid_format;
	upload_width = width;
	upload_height = height;

	R_LightScaleTexture(data, upload_width, upload_height, !mipmap);

	for (i = 0; i < c; i++, scan += 4)
	{
		if (*scan != 255)
		{
			samples = Q2_GL_ALPHA_FORMAT;
			comp = gl_tex_alpha_format;
			break;
		}
	}
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, mipmap);
	glTexImage2D(GL_TEXTURE_2D, 0, comp, width,
			height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
			data);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, false);
	return samples == Q2_GL_ALPHA_FORMAT;
}


static qboolean
R_Upload32Soft(unsigned *data, int width, int height, qboolean mipmap)
{
	int samples;
	unsigned scaled[256 * 256];
	unsigned char paletted_texture[256 * 256];
	int scaled_width, scaled_height;
	int i, c;
	byte *scan;
	int comp;

	uploaded_paletted = false;

	scaled_width = NextPow2(width);

	if (gl1_round_down->value && (scaled_width > width) && mipmap)
	{
		scaled_width >>= 1;
	}

	scaled_height = NextPow2(height);

	if (gl1_round_down->value && (scaled_height > height) && mipmap)
	{
		scaled_height >>= 1;
	}

	/* let people sample down the world textures for speed */
	if (mipmap)
	{
		scaled_width >>= (int)gl1_picmip->value;
		scaled_height >>= (int)gl1_picmip->value;
	}

	/* don't ever bother with >256 textures */
	if (scaled_width > 256)
	{
		scaled_width = 256;
	}

	if (scaled_height > 256)
	{
		scaled_height = 256;
	}

	if (scaled_width < 1)
	{
		scaled_width = 1;
	}

	if (scaled_height < 1)
	{
		scaled_height = 1;
	}

	upload_width = scaled_width;
	upload_height = scaled_height;

	if (scaled_width * scaled_height > sizeof(scaled) / 4)
	{
		// this can't really happen (because they're clamped to 256 above), but whatever
		Com_Error(ERR_DROP, "%s: too big", __func__);
	}

	/* scan the texture for any non-255 alpha */
	c = width * height;
	scan = ((byte *)data) + 3;
	samples = Q2_GL_SOLID_FORMAT;
	comp = gl_tex_solid_format;

	for (i = 0; i < c; i++, scan += 4)
	{
		if (*scan != 255)
		{
			samples = Q2_GL_ALPHA_FORMAT;
			comp = gl_tex_alpha_format;
			break;
		}
	}

	if ((scaled_width == width) && (scaled_height == height))
	{
		if (!mipmap)
		{
			if (qglColorTableEXT && gl1_palettedtexture->value &&
				(samples == Q2_GL_SOLID_FORMAT))
			{
				uploaded_paletted = true;
				R_BuildPalettedTexture(paletted_texture, (unsigned char *)data,
						scaled_width, scaled_height);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT,
						scaled_width, scaled_height, 0, GL_COLOR_INDEX,
						GL_UNSIGNED_BYTE, paletted_texture);
			}
			else
			{
				glTexImage2D(GL_TEXTURE_2D, 0, comp, scaled_width,
						scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
						data);
			}

			goto done;
		}

		memcpy(scaled, data, width * height * 4);
	}
	else
	{
		ResizeSTB((byte *)data, width, height,
				  (byte *)scaled, scaled_width, scaled_height);
	}

	R_LightScaleTexture(scaled, scaled_width, scaled_height, !mipmap);

	if (qglColorTableEXT && gl1_palettedtexture->value &&
		(samples == Q2_GL_SOLID_FORMAT))
	{
		uploaded_paletted = true;
		R_BuildPalettedTexture(paletted_texture, (unsigned char *)scaled,
				scaled_width, scaled_height);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT,
				scaled_width, scaled_height, 0, GL_COLOR_INDEX,
				GL_UNSIGNED_BYTE, paletted_texture);
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0, comp, scaled_width,
				scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
				scaled);
	}

	if (mipmap)
	{
		int miplevel;

		miplevel = 0;

		while (scaled_width > 1 || scaled_height > 1)
		{
			R_MipMap((byte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;

			if (scaled_width < 1)
			{
				scaled_width = 1;
			}

			if (scaled_height < 1)
			{
				scaled_height = 1;
			}

			miplevel++;

			if (qglColorTableEXT && gl1_palettedtexture->value &&
				(samples == Q2_GL_SOLID_FORMAT))
			{
				uploaded_paletted = true;
				R_BuildPalettedTexture(paletted_texture, (unsigned char *)scaled,
						scaled_width, scaled_height);
				glTexImage2D(GL_TEXTURE_2D, miplevel, GL_COLOR_INDEX8_EXT,
						scaled_width, scaled_height, 0, GL_COLOR_INDEX,
						GL_UNSIGNED_BYTE, paletted_texture);
			}
			else
			{
				glTexImage2D(GL_TEXTURE_2D, miplevel, comp, scaled_width,
						scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
			}
		}
	}

done:

	return samples == Q2_GL_ALPHA_FORMAT;
}

qboolean
R_Upload32(unsigned *data, int width, int height, qboolean mipmap)
{
	qboolean res;

	if (gl_config.npottextures)
	{
		res = R_Upload32Native(data, width, height, mipmap);
	}
	else
	{
		res = R_Upload32Soft(data, width, height, mipmap);
	}

	if (mipmap)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

	if (mipmap && gl_config.anisotropic && gl_anisotropic->value)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
				Q_max(gl_anisotropic->value, 1.f));
	}
	return res;
}


/*
 * Returns has_alpha
 */
qboolean
R_Upload8(byte *data, int width, int height, qboolean mipmap, qboolean is_sky)
{
	int s = width * height;

	if (gl_config.palettedtexture && is_sky)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT,
				width, height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
				data);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

		return false; /* SBF: FIXME - what is the correct return value? */
	}
	else
	{
		unsigned *trans = malloc(s * sizeof(unsigned));

		for (int i = 0; i < s; i++)
		{
			int p = data[i];
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

		qboolean ret = R_Upload32(trans, width, height, mipmap);
		free(trans);
		return ret;
	}
}

/*
 * This is also used as an entry point for the generated r_notexture
 */
image_t *
R_LoadPic(const char *name, byte *pic, int width, int realwidth,
		int height, int realheight, size_t data_size, imagetype_t type, int bits)
{
	image_t *image;
	int i;

	qboolean nolerp = false;
	if (r_2D_unfiltered->value && type == it_pic)
	{
		// if r_2D_unfiltered is true(ish), nolerp should usually be true,
		// *unless* the texture is on the r_lerp_list
		nolerp = (r_lerp_list->string == NULL) || (strstr(r_lerp_list->string, name) == NULL);
	}
	else if (gl_nolerp_list != NULL && gl_nolerp_list->string != NULL)
	{
		nolerp = strstr(gl_nolerp_list->string, name) != NULL;
	}

	/* find a free image_t */
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->texnum)
		{
			break;
		}
	}

	if (i == numgltextures)
	{
		if (numgltextures == MAX_GLTEXTURES)
		{
			Com_Error(ERR_DROP, "%s: MAX_GLTEXTURES", __func__);
		}

		numgltextures++;
	}

	image = &gltextures[i];

	if (strlen(name) >= sizeof(image->name))
	{
		Com_Error(ERR_DROP, "%s: \"%s\" is too long", __func__, name);
	}

	strcpy(image->name, name);
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->type = type;

	if ((type == it_skin) && (bits == 8))
	{
		R_FloodFillSkin(pic, width, height);
	}

	/* load little pics into the scrap */
	if (!nolerp && (image->type == it_pic) && (bits == 8) &&
		(image->width < 64) && (image->height < 64))
	{
		int x, y;
		int i, k;
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
			int j;

			for (j = 0; j < image->width; j++, k++)
			{
				scrap_texels[texnum][(y + i) * SCRAP_WIDTH + x + j] = pic[k];
			}
		}

		image->texnum = TEXNUM_SCRAPS + texnum;
		image->scrap = true;
		image->has_alpha = true;
		image->sl = (float)x / SCRAP_WIDTH;
		image->sh = (float)(x + image->width) / SCRAP_WIDTH;
		image->tl = (float)y / SCRAP_HEIGHT;
		image->th = (float)(y + image->height) / SCRAP_HEIGHT;
	}
	else
	{
	nonscrap:
		image->scrap = false;
		image->texnum = TEXNUM_IMAGES + (image - gltextures);
		R_Bind(image->texnum);

		if (bits == 8)
		{
			// resize 8bit images only when we forced such logic
			if (r_scale8bittextures->value)
			{
				byte *image_converted;
				int scale = 2;

				// scale 3 times if lerp image
				if (!nolerp && (vid.height >= 240 * 3))
					scale = 3;

				image_converted = malloc(width * height * scale * scale);
				if (!image_converted)
					return NULL;

				if (scale == 3) {
					scale3x(pic, image_converted, width, height);
				} else {
					scale2x(pic, image_converted, width, height);
				}

				image->has_alpha = R_Upload8(image_converted, width * scale, height * scale,
							(image->type != it_pic && image->type != it_sky),
							image->type == it_sky);
				free(image_converted);
			}
			else
			{
				image->has_alpha = R_Upload8(pic, width, height,
							(image->type != it_pic && image->type != it_sky),
							image->type == it_sky);
			}
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
				Com_DPrintf(
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

	return image;
}

/*
 * Finds or loads the given image or null
 */
image_t *
R_FindImage(const char *originname, imagetype_t type)
{
	char namewe[256], name[256] = {0};
	const char* ext;
	image_t *image;
	size_t len;
	int i;

	if (!originname)
	{
		return NULL;
	}

	Q_strlcpy(name, originname, sizeof(name));

	/* fix backslashes */
	Q_replacebackslash(name);

	ext = COM_FileExtension(name);
	if (!ext[0])
	{
		/* file has no extension */
		return NULL;
	}

	/* Remove the extension */
	len = (ext - name) - 1;
	if ((len < 1) || (len > sizeof(namewe) - 1))
	{
		Com_DPrintf("%s: Bad filename %s\n", __func__, name);
		return NULL;
	}

	memcpy(namewe, name, len);
	namewe[len] = 0;

	/* look for it */
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!strcmp(name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	//
	// load the pic from disk
	//
	image = (image_t *)R_LoadImage(name, namewe, ext, type,
		r_retexturing->value, (loadimage_t)R_LoadPic);

	if (!image && r_validation->value)
	{
		Com_Printf("%s: can't load %s\n", __func__, name);
	}

	return image;
}

struct image_s *
RI_RegisterSkin(const char *name)
{
	return R_FindImage(name, it_skin);
}

/*
 * Any image that was not touched on
 * this registration sequence
 * will be freed.
 */
void
R_FreeUnusedImages(void)
{
	int i;
	image_t *image;

	/* never free r_notexture or particle texture */
	r_notexture->registration_sequence = registration_sequence;
	r_particletexture->registration_sequence = registration_sequence;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
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
		glDeleteTextures(1, (GLuint *)&image->texnum);
		memset(image, 0, sizeof(*image));
	}
}

qboolean
R_ImageHasFreeSpace(void)
{
	int		i, used;
	image_t	*image;

	used = 0;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->name[0])
			continue;
		if (image->registration_sequence == registration_sequence)
		{
			used ++;
		}
	}

	if (image_max < used)
	{
		image_max = used;
	}

	// should same size of free slots as currently used
	return (numgltextures + used) < MAX_GLTEXTURES;
}

void
R_InitImages(void)
{
	byte *colormap;
	int i;
	float m;

#ifdef GL1_GAMMATABLE
	float	g = vid_gamma->value;
#else
	float	g = 1;
#endif
	g = Q_max(g, 0.1f);

	registration_sequence = 1;
	image_max = 0;

	/* init intensity conversions */
	intensity = ri.Cvar_Get("gl1_intensity", "2", CVAR_ARCHIVE);

	if (intensity->value <= 1)
	{
		ri.Cvar_Set("gl1_intensity", "1");
	}

	gl_state.sw_gamma = g;
	gl_state.inverse_intensity = g / intensity->value;

	// FIXME: I think this is redundant - RI_Init() already calls that!
	GetPCXPalette (&colormap, d_8to24table);
	free(colormap);

	if (gl_config.palettedtexture)
	{
		ri.FS_LoadFile("pics/16to8.dat", (void **)&gl_state.d_16to8table);

		if (!gl_state.d_16to8table)
		{
			Com_Error(ERR_FATAL, "%s: Couldn't load pics/16to8.pcx",
				__func__);
		}
	}

	if (g == 1)
	{
		for (i = 0; i < 256; i++)
		{
			gammatable[i] = i;
		}
	}
	else
	{
		g = 1.0f / g;

		for (i = 0; i < 256; i++)
		{
			float inf;

			inf = pow ( (float)(i + 0.5f) / 255.5f , g ) * 255.0f + 0.5f;
			inf = Q_clamp(inf, 0, 255);

			gammatable[i] = inf;
		}
	}

	for (i = 0; i < 256; i++)
	{
		int j;

		j = i * intensity->value;
		j = Q_min(j, 255);

		intensitytable[i] = j;
	}

	// I know, minimum light level's calculation is much simpler than gamma.
	// But will still need a vid_restart to apply its values to currently loaded
	// lightmaps. Also, memory is cheaper than CPU.
	m = Q_clamp(gl1_minlight->value, 0, 255);
	gl_state.minlight_set = (m != 0);

	if (!gl_state.minlight_set)	// minlight == 0
	{
		for (i = 0; i < 256; i++)
		{
			minlight[i] = i;
		}
	}
	else
	{
		for (i = 0; i < 256; i++)
		{
			float inf = (255.0f - m) * (float)i / 255.0f + m;
			inf = Q_clamp(inf, 0, 255);
			minlight[i] = inf;
		}
	}
}

void
R_ShutdownImages(void)
{
	int i;
	image_t *image;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->registration_sequence)
		{
			continue; /* free image_t slot */
		}

		/* free it */
		glDeleteTextures(1, (GLuint *)&image->texnum);
		memset(image, 0, sizeof(*image));
	}
}

