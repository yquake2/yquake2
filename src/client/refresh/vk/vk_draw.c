/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2018-2019 Krzysztof Kondrak

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

// vk_draw.c

#include "header/local.h"

static image_t	*draw_chars;

/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	// load console characters (don't bilerp characters)
	qvksampler_t samplerType = S_NEAREST;
	draw_chars = Vk_FindImage("pics/conchars.pcx", it_pic, &samplerType);
	if (!draw_chars)
	{
		ri.Sys_Error(ERR_FATAL, "%s: Couldn't load pics/conchars.pcx", __func__);
	}
}



/*
================
RE_Draw_CharScaled

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void RE_Draw_CharScaled (int x, int y, int num, float scale)
{
	int	row, col;
	float	frow, fcol, size;

	num &= 255;

	if ((num & 127) == 32)
		return;		// space

	if (y <= -8)
		return;			// totally off screen

	row = num >> 4;
	col = num & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;

	float imgTransform[] = { (float)x / vid.width, (float)y / vid.height,
							 8.f * scale / vid.width, 8.f * scale / vid.height,
							 fcol, frow, size, size };
	QVk_DrawTexRect(imgTransform, sizeof(imgTransform), &draw_chars->vk_texture);
}

/*
=============
RE_Draw_FindPic
=============
*/
image_t	*RE_Draw_FindPic (char *name)
{
	image_t *vk;

	if (name[0] != '/' && name[0] != '\\')
	{
		char	fullname[MAX_QPATH];

		Com_sprintf(fullname, sizeof(fullname), "pics/%s.pcx", name);
		vk = Vk_FindImage(fullname, it_pic, NULL);
	}
	else
		vk = Vk_FindImage(name + 1, it_pic, NULL);

	return vk;
}

/*
=============
RE_Draw_GetPicSize
=============
*/
void RE_Draw_GetPicSize (int *w, int *h, char *name)
{
	image_t *vk;

	vk = RE_Draw_FindPic(name);
	if (!vk)
	{
		*w = *h = -1;
		return;
	}

	*w = vk->width;
	*h = vk->height;
}

/*
=============
RE_Draw_StretchPic
=============
*/
void RE_Draw_StretchPic (int x, int y, int w, int h, char *name)
{
	image_t *vk;

	vk = RE_Draw_FindPic(name);
	if (!vk)
	{
		R_Printf(PRINT_ALL, "%s(): Can't find pic: %s\n", __func__, name);
		return;
	}

	float imgTransform[] = { (float)x / vid.width, (float)y / vid.height,
							 (float)w / vid.width, (float)h / vid.height,
							  vk->sl,				vk->tl,
							  vk->sh - vk->sl,		vk->th - vk->tl };
	QVk_DrawTexRect(imgTransform, sizeof(imgTransform), &vk->vk_texture);
}


/*
=============
RE_Draw_PicScaled
=============
*/
void RE_Draw_PicScaled (int x, int y, char *name, float scale)
{
	image_t *vk;

	vk = RE_Draw_FindPic(name);
	if (!vk)
	{
		R_Printf(PRINT_ALL, "%s(): Can't find pic: %s\n", __func__, name);
		return;
	}

	RE_Draw_StretchPic(x, y, vk->width*scale, vk->height*scale, name);
}

/*
=============
RE_Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void RE_Draw_TileClear (int x, int y, int w, int h, char *name)
{
	image_t	*image;

	image = RE_Draw_FindPic(name);
	if (!image)
	{
		R_Printf(PRINT_ALL, "%s(): Can't find pic: %s\n", __func__, name);
		return;
	}

	float imgTransform[] = { (float)x / vid.width,  (float)y / vid.height,
							 (float)w / vid.width,  (float)h / vid.height,
							 (float)x / 64.0,		(float)y / 64.0,
							 (float)w / 64.0,		(float)h / 64.0 };
	QVk_DrawTexRect(imgTransform, sizeof(imgTransform), &image->vk_texture);
}


/*
=============
RE_Draw_Fill

Fills a box of pixels with a single color
=============
*/
void RE_Draw_Fill (int x, int y, int w, int h, int c)
{
	union
	{
		unsigned	c;
		byte		v[4];
	} color;

	if ((unsigned)c > 255)
		ri.Sys_Error(ERR_FATAL, "%s: bad color", __func__);

	color.c = d_8to24table[c];

	float imgTransform[] = { (float)x / vid.width, (float)y / vid.height,
							 (float)w / vid.width, (float)h / vid.height,
							 color.v[0] / 255.f, color.v[1] / 255.f, color.v[2] / 255.f, 1.f };
	QVk_DrawColorRect(imgTransform, sizeof(imgTransform), RP_UI);
}

//=============================================================================

/*
================
RE_Draw_FadeScreen

================
*/
void RE_Draw_FadeScreen (void)
{
	float imgTransform[] = { 0.f, 0.f, vid.width, vid.height, 0.f, 0.f, 0.f, .8f };

	QVk_DrawColorRect(imgTransform, sizeof(imgTransform), RP_UI);
}


//====================================================================


/*
=============
RE_Draw_StretchRaw
=============
*/
extern unsigned	r_rawpalette[256];
extern qvktexture_t vk_rawTexture;

static unsigned	raw_image32[512 * 512];

void RE_Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{

	int	i, j;
	int	hscale, vscale;

	hscale = cols * 0x10000 / 512;
	vscale = rows * 0x10000 / 512;

	unsigned *dest = raw_image32;
	byte *source = data;
	for (i = 0; i < 512; i++)
	{
		for (j = 0; j < 512; j++)
		{
			*dest = r_rawpalette[*(source + ((j * hscale) >> 16))];
			dest ++;
		}
		source = data + (((i * vscale) >> 16) * cols);
	}

	if (vk_rawTexture.resource.image != VK_NULL_HANDLE)
	{
		QVk_UpdateTextureData(&vk_rawTexture, (unsigned char*)&raw_image32, 0, 0, 512, 512);
	}
	else
	{
		QVVKTEXTURE_CLEAR(vk_rawTexture);
		QVk_CreateTexture(&vk_rawTexture, (unsigned char*)&raw_image32, 512, 512,
			vk_current_sampler);
		QVk_DebugSetObjectName((uint64_t)vk_rawTexture.resource.image,
			VK_OBJECT_TYPE_IMAGE, "Image: raw texture");
		QVk_DebugSetObjectName((uint64_t)vk_rawTexture.imageView,
			VK_OBJECT_TYPE_IMAGE_VIEW, "Image View: raw texture");
		QVk_DebugSetObjectName((uint64_t)vk_rawTexture.descriptorSet,
			VK_OBJECT_TYPE_DESCRIPTOR_SET, "Descriptor Set: raw texture");
		QVk_DebugSetObjectName((uint64_t)vk_rawTexture.resource.memory,
			VK_OBJECT_TYPE_DEVICE_MEMORY, "Memory: raw texture");
	}

	float imgTransform[] = { (float)x / vid.width, (float)y / vid.height,
							 (float)w / vid.width, (float)h / vid.height,
							 0.f, 0.f, 1.f, 1.0f };
	QVk_DrawTexRect(imgTransform, sizeof(imgTransform), &vk_rawTexture);
}
