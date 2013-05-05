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
int base_textureid; /* gltextures[i] = base_textureid+i */
extern qboolean scrap_dirty;
extern byte scrap_texels[MAX_SCRAPS][BLOCK_WIDTH * BLOCK_HEIGHT];

static byte intensitytable[256];
static unsigned char gammatable[256];

cvar_t *intensity;

unsigned d_8to24table[256];

qboolean R_Upload8(byte *data, int width, int height,
		qboolean mipmap, qboolean is_sky);
qboolean R_Upload32(unsigned *data, int width, int height, qboolean mipmap);

int gl_solid_format = 3;
int gl_alpha_format = 4;

int gl_tex_solid_format = 3;
int gl_tex_alpha_format = 4;

int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int gl_filter_max = GL_LINEAR;

int Draw_GetPalette(void);

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

#define NUM_GL_MODES (sizeof(modes) / sizeof(glmode_t))

typedef struct
{
	char *name;
	int mode;
} gltmode_t;

gltmode_t gl_alpha_modes[] = {
	{"default", 4},
	{"GL_RGBA", GL_RGBA},
	{"GL_RGBA8", GL_RGBA8},
	{"GL_RGB5_A1", GL_RGB5_A1},
	{"GL_RGBA4", GL_RGBA4},
	{"GL_RGBA2", GL_RGBA2},
};

#define NUM_GL_ALPHA_MODES (sizeof(gl_alpha_modes) / sizeof(gltmode_t))

gltmode_t gl_solid_modes[] = {
	{"default", 3},
	{"GL_RGB", GL_RGB},
	{"GL_RGB8", GL_RGB8},
	{"GL_RGB5", GL_RGB5},
	{"GL_RGB4", GL_RGB4},
	{"GL_R3_G3_B2", GL_R3_G3_B2},
#ifdef GL_RGB2_EXT
	{"GL_RGB2", GL_RGB2_EXT},
#endif
};

#define NUM_GL_SOLID_MODES (sizeof(gl_solid_modes) / sizeof(gltmode_t))

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

int upload_width, upload_height;
qboolean uploaded_paletted;

void
R_SetTexturePalette(unsigned palette[256])
{
	int i;
	unsigned char temptable[768];

	if (qglColorTableEXT && gl_ext_palettedtexture->value)
	{
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
R_EnableMultitexture(qboolean enable)
{
	if (!qglSelectTextureSGIS && !qglActiveTextureARB)
	{
		return;
	}

	if (enable)
	{
		R_SelectTexture(QGL_TEXTURE1);
		qglEnable(GL_TEXTURE_2D);
		R_TexEnv(GL_REPLACE);
	}
	else
	{
		R_SelectTexture(QGL_TEXTURE1);
		qglDisable(GL_TEXTURE_2D);
		R_TexEnv(GL_REPLACE);
	}

	R_SelectTexture(QGL_TEXTURE0);
	R_TexEnv(GL_REPLACE);
}

void
R_SelectTexture(GLenum texture)
{
	int tmu;

	if (!qglSelectTextureSGIS && !qglActiveTextureARB)
	{
		return;
	}

	if (texture == QGL_TEXTURE0)
	{
		tmu = 0;
	}
	else
	{
		tmu = 1;
	}

	if (tmu == gl_state.currenttmu)
	{
		return;
	}

	gl_state.currenttmu = tmu;

	if (qglSelectTextureSGIS)
	{
		qglSelectTextureSGIS(texture);
	}
	else if (qglActiveTextureARB)
	{
		qglActiveTextureARB(texture);
		qglClientActiveTextureARB(texture);
	}
}

void
R_TexEnv(GLenum mode)
{
	static int lastmodes[2] = {-1, -1};

	if (mode != lastmodes[gl_state.currenttmu])
	{
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
		lastmodes[gl_state.currenttmu] = mode;
	}
}

void
R_Bind(int texnum)
{
	extern image_t *draw_chars;

	if (gl_nobind->value && draw_chars) /* performance evaluation option */
	{
		texnum = draw_chars->texnum;
	}

	if (gl_state.currenttextures[gl_state.currenttmu] == texnum)
	{
		return;
	}

	gl_state.currenttextures[gl_state.currenttmu] = texnum;
	qglBindTexture(GL_TEXTURE_2D, texnum);
}

void
R_MBind(GLenum target, int texnum)
{
	R_SelectTexture(target);

	if (target == QGL_TEXTURE0)
	{
		if (gl_state.currenttextures[0] == texnum)
		{
			return;
		}
	}
	else
	{
		if (gl_state.currenttextures[1] == texnum)
		{
			return;
		}
	}

	R_Bind(texnum);
}

void
R_TextureMode(char *string)
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
		VID_Printf(PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	/* clamp selected anisotropy */
	if (gl_config.anisotropic)
	{
		if (gl_anisotropic->value > gl_config.max_anisotropy)
		{
			Cvar_SetValue("gl_anisotropic", gl_config.max_anisotropy);
		}
		else if (gl_anisotropic->value < 1.0)
		{
			Cvar_SetValue("gl_anisotropic", 1.0);
		}
	}

	/* change all the existing mipmap texture objects */
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if ((glt->type != it_pic) && (glt->type != it_sky))
		{
			R_Bind(glt->texnum);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
					gl_filter_min);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
					gl_filter_max);

			/* Set anisotropic filter if supported and enabled */
			if (gl_config.anisotropic && gl_anisotropic->value)
			{
				qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
						gl_anisotropic->value);
			}
		}
	}
}

void
R_TextureAlphaMode(char *string)
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
		VID_Printf(PRINT_ALL, "bad alpha texture mode name\n");
		return;
	}

	gl_tex_alpha_format = gl_alpha_modes[i].mode;
}

void
R_TextureSolidMode(char *string)
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
		VID_Printf(PRINT_ALL, "bad solid texture mode name\n");
		return;
	}

	gl_tex_solid_format = gl_solid_modes[i].mode;
}

void
R_ImageList_f(void)
{
	int i;
	image_t *image;
	int texels;
	const char *palstrings[2] = {
		"RGB",
		"PAL"
	};

	VID_Printf(PRINT_ALL, "------------------\n");
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
				VID_Printf(PRINT_ALL, "M");
				break;
			case it_sprite:
				VID_Printf(PRINT_ALL, "S");
				break;
			case it_wall:
				VID_Printf(PRINT_ALL, "W");
				break;
			case it_pic:
				VID_Printf(PRINT_ALL, "P");
				break;
			default:
				VID_Printf(PRINT_ALL, " ");
				break;
		}

		VID_Printf(PRINT_ALL, " %3i %3i %s: %s\n",
				image->upload_width, image->upload_height,
				palstrings[image->paletted], image->name);
	}

	VID_Printf(PRINT_ALL,
			"Total texel count (not counting mipmaps): %i\n",
			texels);
}

/*
 * Fill background pixels so mipmapping doesn't have haloes
 */
void
R_FloodFillSkin(byte *skin, int skinwidth, int skinheight)
{
	byte fillcolor = *skin; /* assume this is the pixel to fill */
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int inpt = 0, outpt = 0;
	int filledcolor = -1;
	int i;

	if (filledcolor == -1)
	{
		filledcolor = 0;

		/* attempt to find opaque black */
		for (i = 0; i < 256; ++i)
		{
			if (LittleLong(d_8to24table[i]) == (255 << 0)) /* alpha 1.0 */
			{
				filledcolor = i;
				break;
			}
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

void
R_ResampleTexture(unsigned *in, int inwidth, int inheight,
		unsigned *out, int outwidth, int outheight)
{
	int i, j;
	unsigned *inrow, *inrow2;
	unsigned frac, fracstep;
	unsigned p1[1024], p2[1024];
	byte *pix1, *pix2, *pix3, *pix4;

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;

	for (i = 0; i < outwidth; i++)
	{
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	frac = 3 * (fracstep >> 2);

	for (i = 0; i < outwidth; i++)
	{
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	for (i = 0; i < outheight; i++, out += outwidth)
	{
		inrow = in + inwidth * (int)((i + 0.25) * inheight / outheight);
		inrow2 = in + inwidth * (int)((i + 0.75) * inheight / outheight);

		for (j = 0; j < outwidth; j++)
		{
			pix1 = (byte *)inrow + p1[j];
			pix2 = (byte *)inrow + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
			((byte *)(out + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte *)(out + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte *)(out + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte *)(out + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
}

/*
 * Scale up the pixel values in a
 * texture to increase the
 * lighting range
 */
void
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
void
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
void
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

qboolean
R_Upload32(unsigned *data, int width, int height, qboolean mipmap)
{
	int samples;
	unsigned scaled[256 * 256];
	unsigned char paletted_texture[256 * 256];
	int scaled_width, scaled_height;
	int i, c;
	byte *scan;
	int comp;

	uploaded_paletted = false;

	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1)
	{
	}

	if (gl_round_down->value && (scaled_width > width) && mipmap)
	{
		scaled_width >>= 1;
	}

	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1)
	{
	}

	if (gl_round_down->value && (scaled_height > height) && mipmap)
	{
		scaled_height >>= 1;
	}

	/* let people sample down the world textures for speed */
	if (mipmap)
	{
		scaled_width >>= (int)gl_picmip->value;
		scaled_height >>= (int)gl_picmip->value;
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
		VID_Error(ERR_DROP, "R_Upload32: too big");
	}

	/* scan the texture for any non-255 alpha */
	c = width * height;
	scan = ((byte *)data) + 3;
	samples = gl_solid_format;

	for (i = 0; i < c; i++, scan += 4)
	{
		if (*scan != 255)
		{
			samples = gl_alpha_format;
			break;
		}
	}

	if (samples == gl_solid_format)
	{
		comp = gl_tex_solid_format;
	}
	else if (samples == gl_alpha_format)
	{
		comp = gl_tex_alpha_format;
	}
	else
	{
		VID_Printf(PRINT_ALL, "Unknown number of texture components %i\n",
				samples);
		comp = samples;
	}

	if ((scaled_width == width) && (scaled_height == height))
	{
		if (!mipmap)
		{
			if (qglColorTableEXT && gl_ext_palettedtexture->value &&
				(samples == gl_solid_format))
			{
				uploaded_paletted = true;
				R_BuildPalettedTexture(paletted_texture, (unsigned char *)data,
						scaled_width, scaled_height);
				qglTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT,
						scaled_width, scaled_height, 0, GL_COLOR_INDEX,
						GL_UNSIGNED_BYTE, paletted_texture);
			}
			else
			{
				qglTexImage2D(GL_TEXTURE_2D, 0, comp, scaled_width,
						scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
						data);
			}

			goto done;
		}

		memcpy(scaled, data, width * height * 4);
	}
	else
	{
		R_ResampleTexture(data, width, height, scaled,
				scaled_width, scaled_height);
	}

	R_LightScaleTexture(scaled, scaled_width, scaled_height, !mipmap);

	if (qglColorTableEXT && gl_ext_palettedtexture->value &&
		(samples == gl_solid_format))
	{
		uploaded_paletted = true;
		R_BuildPalettedTexture(paletted_texture, (unsigned char *)scaled,
				scaled_width, scaled_height);
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT,
				scaled_width, scaled_height, 0, GL_COLOR_INDEX,
				GL_UNSIGNED_BYTE, paletted_texture);
	}
	else
	{
		qglTexImage2D(GL_TEXTURE_2D, 0, comp, scaled_width,
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

			if (qglColorTableEXT && gl_ext_palettedtexture->value &&
				(samples == gl_solid_format))
			{
				uploaded_paletted = true;
				R_BuildPalettedTexture(paletted_texture, (unsigned char *)scaled,
						scaled_width, scaled_height);
				qglTexImage2D(GL_TEXTURE_2D, miplevel, GL_COLOR_INDEX8_EXT,
						scaled_width, scaled_height, 0, GL_COLOR_INDEX,
						GL_UNSIGNED_BYTE, paletted_texture);
			}
			else
			{
				qglTexImage2D(GL_TEXTURE_2D, miplevel, comp, scaled_width,
						scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
			}
		}
	}

done:

	if (mipmap)
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

	if (mipmap && gl_config.anisotropic && gl_anisotropic->value)
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
				gl_anisotropic->value);
	}

	return samples == gl_alpha_format;
}

/*
 * Returns has_alpha
 */
qboolean
R_Upload8(byte *data, int width, int height, qboolean mipmap, qboolean is_sky)
{
	unsigned trans[512 * 256];
	int i, s;
	int p;

	s = width * height;

	if (s > sizeof(trans) / 4)
	{
		VID_Error(ERR_DROP, "R_Upload8: too large");
	}

	if (qglColorTableEXT && gl_ext_palettedtexture->value && is_sky)
	{
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT,
				width, height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
				data);

		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

		return false; /* SBF: FIXME - what is the correct return value? */
	}
	else
	{
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

		return R_Upload32(trans, width, height, mipmap);
	}
}

/*
 * This is also used as an entry point for the generated r_notexture
 */
image_t *
R_LoadPic(char *name, byte *pic, int width, int realwidth,
		int height, int realheight, imagetype_t type, int bits)
{
	image_t *image;
	int i;

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
			VID_Error(ERR_DROP, "MAX_GLTEXTURES");
		}

		numgltextures++;
	}

	image = &gltextures[i];

	if (strlen(name) >= sizeof(image->name))
	{
		VID_Error(ERR_DROP, "Draw_LoadPic: \"%s\" is too long", name);
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
	if ((image->type == it_pic) && (bits == 8) &&
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
				VID_Printf(PRINT_DEVELOPER,
						"Warning, image '%s' has hi-res replacement smaller than the original! (%d x %d) < (%d x %d)\n",
						name, image->width, image->height, realwidth, realheight);
			}
		}

		image->sl = 0;
		image->sh = 1;
		image->tl = 0;
		image->th = 1;
	}

	return image;
}

/*
 * Finds or loads the given image
 */
image_t *
R_FindImage(char *name, imagetype_t type)
{
	image_t *image;
	int i, len;
	byte *pic, *palette;
	int width, height;
	char *ptr;
	char namewe[256];

#ifdef RETEXTURE
	int realwidth = 0, realheight = 0;
#endif

	if (!name)
	{
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

	if (!strcmp(name + len - 4, ".pcx"))
	{
#ifdef RETEXTURE

		if (gl_retexturing->value)
		{
			GetPCXInfo(name, &realwidth, &realheight);

			/* Try to load a TGA */
			LoadTGA(namewe, &pic, &width, &height);

			if (!pic)
			{
				/* JPEG if no TGA available */
				LoadJPG(namewe, &pic, &width, &height);
			}
			else
			{
				/* Upload TGA */
				R_LoadPic(name, pic, width, realwidth, height,
						realheight, type, 32);
			}

			if (!pic)
			{
				/* PCX if no JPEG available (exists always) */
				LoadPCX(name, &pic, &palette, &width, &height);

				if (!pic)
				{
					/* No texture found */
					return NULL;
				}

				/* Upload the PCX */
				image = R_LoadPic(name, pic, width, 0, height, 0, type, 8);
			}
			else
			{
				/* Upload JPEG or TGA */
				image = R_LoadPic(name, pic, width, realwidth,
						height, realheight, type, 32);
			}
		}
		else
#endif
		{
			LoadPCX(name, &pic, &palette, &width, &height);

			if (!pic)
			{
				return NULL;
			}

			image = R_LoadPic(name, pic, width, 0, height, 0, type, 8);
		}
	}
	else if (!strcmp(name + len - 4, ".wal"))
	{
#ifdef RETEXTURE

		if (gl_retexturing->value)
		{
			/* Get size of the original texture */
			GetWalInfo(name, &realwidth, &realheight);

			/* Try to load a TGA */
			LoadTGA(namewe, &pic, &width, &height);

			if (!pic)
			{
				/* JPEG if no TGA available */
				LoadJPG(namewe, &pic, &width, &height);
			}
			else
			{
				/* Upload TGA */
				R_LoadPic(name, pic, width, realwidth, height,
						realheight, type, 32);
			}

			if (!pic)
			{
				/* WAL of no JPEG available (exists always) */
				image = LoadWal(namewe);
			}
			else
			{
				/* Upload JPEG or TGA */
				image = R_LoadPic(name, pic, width, realwidth,
						height, realheight, type, 32);
			}

			if (!image)
			{
				/* No texture found */
				return NULL;
			}
		}
		else
#endif
		{
			image = LoadWal(name);

			if (!image)
			{
				/* No texture found */
				return NULL;
			}
		}
	}

#ifdef RETEXTURE
	else if (!strcmp(name + len - 4, ".tga"))
	{
		LoadTGA(name, &pic, &width, &height);
		image = R_LoadPic(name, pic, width, realwidth,
				height, realwidth, type, 32);
	}
	else if (!strcmp(name + len - 4, ".jpg"))
	{
		LoadJPG(name, &pic, &width, &height);
		image = R_LoadPic(name, pic, width, realwidth,
				height, realheight, type, 32);
	}
#endif
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
}

struct image_s *
R_RegisterSkin(char *name)
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
		qglDeleteTextures(1, (GLuint *)&image->texnum);
		memset(image, 0, sizeof(*image));
	}
}

void
R_InitImages(void)
{
	int i, j;
	float g = vid_gamma->value;

	registration_sequence = 1;

	/* init intensity conversions */
	intensity = Cvar_Get("intensity", "2", CVAR_ARCHIVE);

	if (intensity->value <= 1)
	{
		Cvar_Set("intensity", "1");
	}

	gl_state.inverse_intensity = 1 / intensity->value;

	Draw_GetPalette();

	if (qglColorTableEXT)
	{
		FS_LoadFile("pics/16to8.dat", (void **)&gl_state.d_16to8table);

		if (!gl_state.d_16to8table)
		{
			VID_Error(ERR_FATAL, "Couldn't load pics/16to8.pcx");
		}
	}

	for (i = 0; i < 256; i++)
	{
		if ((g == 1) || gl_state.hwgamma)
		{
			gammatable[i] = i;
		}
		else
		{
			float inf;

			inf = 255 * pow((i + 0.5) / 255.5, g) + 0.5;

			if (inf < 0)
			{
				inf = 0;
			}

			if (inf > 255)
			{
				inf = 255;
			}

			gammatable[i] = inf;
		}
	}

	for (i = 0; i < 256; i++)
	{
		j = i * intensity->value;

		if (j > 255)
		{
			j = 255;
		}

		intensitytable[i] = j;
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
		qglDeleteTextures(1, (GLuint *)&image->texnum);
		memset(image, 0, sizeof(*image));
	}
}

