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
 * This file implements the .cin video codec and the corresponding .pcx
 * bitmap decoder. .cin files are just a bunch of .pcx images.
 *
 * =======================================================================
 */

#include "header/client.h"

cvar_t *cin_force43;

typedef struct
{
	byte *data;
	int count;
} cblock_t;

typedef struct
{
	qboolean restart_sound;
	int s_rate;
	int s_width;
	int s_channels;

	int width;
	int height;
	byte *pic;
	byte *pic_pending;

	/* order 1 huffman stuff */
	int *hnodes1;

	/* [256][256][2]; */
	int numhnodes1[256];

	int h_used[512];
	int h_count[512];
} cinematics_t;

cinematics_t cin;

void
SCR_LoadPCX(char *filename, byte **pic, byte **palette, int *width, int *height)
{
	byte *raw;
	pcx_t *pcx;
	int x, y;
	int len;
	int dataByte, runLength;
	byte *out, *pix;

	*pic = NULL;

	/* load the file */
	len = FS_LoadFile(filename, (void **)&raw);

	if (!raw)
	{
		return;
	}

	/* parse the PCX file */
	pcx = (pcx_t *)raw;
	raw = &pcx->data;

	if ((pcx->manufacturer != 0x0a) ||
		(pcx->version != 5) ||
		(pcx->encoding != 1) ||
		(pcx->bits_per_pixel != 8) ||
		(pcx->xmax >= 640) ||
		(pcx->ymax >= 480))
	{
		Com_Printf("Bad pcx file %s\n", filename);
		return;
	}

	out = Z_Malloc((pcx->ymax + 1) * (pcx->xmax + 1));

	*pic = out;

	pix = out;

	if (palette)
	{
		*palette = Z_Malloc(768);
		memcpy(*palette, (byte *)pcx + len - 768, 768);
	}

	if (width)
	{
		*width = pcx->xmax + 1;
	}

	if (height)
	{
		*height = pcx->ymax + 1;
	}

	for (y = 0; y <= pcx->ymax; y++, pix += pcx->xmax + 1)
	{
		for (x = 0; x <= pcx->xmax; )
		{
			dataByte = *raw++;

			if ((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
			{
				runLength = 1;
			}

			while (runLength-- > 0)
			{
				pix[x++] = dataByte;
			}
		}
	}

	if (raw - (byte *)pcx > len)
	{
		Com_Printf("PCX file %s was malformed", filename);
		Z_Free(*pic);
		*pic = NULL;
	}

	FS_FreeFile(pcx);
}

void
SCR_StopCinematic(void)
{
	cl.cinematictime = 0; /* done */

	if (cin.pic)
	{
		Z_Free(cin.pic);
		cin.pic = NULL;
	}

	if (cin.pic_pending)
	{
		Z_Free(cin.pic_pending);
		cin.pic_pending = NULL;
	}

	if (cl.cinematicpalette_active)
	{
		R_SetPalette(NULL);
		cl.cinematicpalette_active = false;
	}

	if (cl.cinematic_file)
	{
		FS_FCloseFile(cl.cinematic_file);
		cl.cinematic_file = 0;
	}

	if (cin.hnodes1)
	{
		Z_Free(cin.hnodes1);
		cin.hnodes1 = NULL;
	}

	/* switch back down to 11 khz sound if necessary */
	if (cin.restart_sound)
	{
		cin.restart_sound = false;
		CL_Snd_Restart_f();
	}
}

void
SCR_FinishCinematic(void)
{
	/* tell the server to advance to the next map / cinematic */
	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
	SZ_Print(&cls.netchan.message, va("nextserver %i\n", cl.servercount));
}

int
SmallestNode1(int numhnodes)
{
	int i;
	int best, bestnode;

	best = 99999999;
	bestnode = -1;

	for (i = 0; i < numhnodes; i++)
	{
		if (cin.h_used[i])
		{
			continue;
		}

		if (!cin.h_count[i])
		{
			continue;
		}

		if (cin.h_count[i] < best)
		{
			best = cin.h_count[i];
			bestnode = i;
		}
	}

	if (bestnode == -1)
	{
		return -1;
	}

	cin.h_used[bestnode] = true;
	return bestnode;
}

/*
 * Reads the 64k counts table and initializes the node trees
 */
void
Huff1TableInit(void)
{
	int prev;
	int j;
	int *node, *nodebase;
	byte counts[256];
	int numhnodes;

	cin.hnodes1 = Z_Malloc(256 * 256 * 2 * 4);
	memset(cin.hnodes1, 0, 256 * 256 * 2 * 4);

	for (prev = 0; prev < 256; prev++)
	{
		memset(cin.h_count, 0, sizeof(cin.h_count));
		memset(cin.h_used, 0, sizeof(cin.h_used));

		/* read a row of counts */
		FS_Read(counts, sizeof(counts), cl.cinematic_file);

		for (j = 0; j < 256; j++)
		{
			cin.h_count[j] = counts[j];
		}

		/* build the nodes */
		numhnodes = 256;
		nodebase = cin.hnodes1 + prev * 256 * 2;

		while (numhnodes != 511)
		{
			node = nodebase + (numhnodes - 256) * 2;

			/* pick two lowest counts */
			node[0] = SmallestNode1(numhnodes);

			if (node[0] == -1)
			{
				break;  /* no more counts */
			}

			node[1] = SmallestNode1(numhnodes);

			if (node[1] == -1)
			{
				break;
			}

			cin.h_count[numhnodes] = cin.h_count[node[0]] +
									 cin.h_count[node[1]];
			numhnodes++;
		}

		cin.numhnodes1[prev] = numhnodes - 1;
	}
}

cblock_t
Huff1Decompress(cblock_t in)
{
	byte *input;
	byte *out_p;
	int nodenum;
	int count;
	cblock_t out;
	int inbyte;
	int *hnodes, *hnodesbase;

	/* get decompressed count */
	count = in.data[0] +
			(in.data[1] << 8) + (in.data[2] << 16) + (in.data[3] << 24);
	input = in.data + 4;
	out_p = out.data = Z_Malloc(count);

	/* read bits */
	hnodesbase = cin.hnodes1 - 256 * 2; /* nodes 0-255 aren't stored */

	hnodes = hnodesbase;
	nodenum = cin.numhnodes1[0];

	while (count)
	{
		inbyte = *input++;

		int i = 0;

		for (i = 0; i < 8; i++)
		{
			if (nodenum < 256)
			{
				hnodes = hnodesbase + (nodenum << 9);
				*out_p++ = nodenum;

				if (!--count)
				{
					break;
				}

				nodenum = cin.numhnodes1[nodenum];
			}

			nodenum = hnodes[nodenum * 2 + (inbyte & 1)];
			inbyte >>= 1;
		}
	}

	if ((input - in.data != in.count) && (input - in.data != in.count + 1))
	{
		Com_Printf("Decompression overread by %i", (input - in.data) - in.count);
	}

	out.count = out_p - out.data;

	return out;
}

byte *
SCR_ReadNextFrame(void)
{
	int r;
	int command;
	byte samples[22050 / 14 * 4];
	byte compressed[0x20000];
	int size;
	byte *pic;
	cblock_t in, huf1;
	int start, end, count;

	/* read the next frame */
	r = FS_FRead(&command, 4, 1, cl.cinematic_file);

	if (r == 0)
	{
		/* we'll give it one more chance */
		r = FS_FRead(&command, 4, 1, cl.cinematic_file);
	}

	if (r != 4)
	{
		return NULL;
	}

	command = LittleLong(command);

	if (command == 2)
	{
		return NULL;  /* last frame marker */
	}

	if (command == 1)
	{
		/* read palette */
		FS_Read(cl.cinematicpalette, sizeof(cl.cinematicpalette),
				cl.cinematic_file);
		cl.cinematicpalette_active = 0;
	}

	/* decompress the next frame */
	FS_Read(&size, 4, cl.cinematic_file);
	size = LittleLong(size);

	if (((unsigned long)size > sizeof(compressed)) || (size < 1))
	{
		Com_Error(ERR_DROP, "Bad compressed frame size");
	}

	FS_Read(compressed, size, cl.cinematic_file);

	/* read sound */
	start = cl.cinematicframe * cin.s_rate / 14;
	end = (cl.cinematicframe + 1) * cin.s_rate / 14;
	count = end - start;

	FS_Read(samples, count * cin.s_width * cin.s_channels,
			cl.cinematic_file);

	if (cin.s_width == 2)
	{
		for (r = 0; r < count * cin.s_channels; r++)
		{
			((short *)samples)[r] = LittleShort(((short *)samples)[r]);
		}
	}

	S_RawSamples(count, cin.s_rate, cin.s_width, cin.s_channels,
			samples, Cvar_VariableValue("s_volume"));

	in.data = compressed;
	in.count = size;

	huf1 = Huff1Decompress(in);

	pic = huf1.data;

	cl.cinematicframe++;

	return pic;
}

void
SCR_RunCinematic(void)
{
	int frame;

	if (cl.cinematictime <= 0)
	{
		SCR_StopCinematic();
		return;
	}

	if (cl.cinematicframe == -1)
	{
		return; /* static image */
	}

	if (cls.key_dest != key_game)
	{
		/* pause if menu or console is up */
		cl.cinematictime = cls.realtime - cl.cinematicframe * 1000 / 14;
		return;
	}

	frame = (cls.realtime - cl.cinematictime) * 14.0 / 1000;

	if (frame <= cl.cinematicframe)
	{
		return;
	}

	if (frame > cl.cinematicframe + 1)
	{
		Com_Printf("Dropped frame: %i > %i\n", frame, cl.cinematicframe + 1);
		cl.cinematictime = cls.realtime - cl.cinematicframe * 1000 / 14;
	}

	if (cin.pic)
	{
		Z_Free(cin.pic);
	}

	cin.pic = cin.pic_pending;
	cin.pic_pending = NULL;
	cin.pic_pending = SCR_ReadNextFrame();

	if (!cin.pic_pending)
	{
		SCR_StopCinematic();
		SCR_FinishCinematic();
		cl.cinematictime = 1; /* the black screen behind loading */
		SCR_BeginLoadingPlaque();
		cl.cinematictime = 0;
		return;
	}
}

/*
 * Returns true if a cinematic is active, meaning the
 * view rendering should be skipped
 */
qboolean
SCR_DrawCinematic(void)
{
	int x, y, w, h;

	if (cl.cinematictime <= 0)
	{
		return false;
	}

	/* blank screen and pause if menu is up */
	if (cls.key_dest == key_menu)
	{
		R_SetPalette(NULL);
		cl.cinematicpalette_active = false;
		return true;
	}

	if (!cl.cinematicpalette_active)
	{
		R_SetPalette(cl.cinematicpalette);
		cl.cinematicpalette_active = true;
	}

	if (!cin.pic)
	{
		return true;
	}

	if (cin_force43->value)
	{
		w = viddef.height * 4 / 3;
		if (w > viddef.width)
		{
			w = viddef.width;
		}
		w &= ~3;
		h = w * 3 / 4;
		x = (viddef.width - w) / 2;
		y = (viddef.height - h) / 2;
	}
	else
	{
		x = y = 0;
		w = viddef.width;
		h = viddef.height;
	}

	if (x > 0)
	{
		Draw_Fill(0, 0, x, viddef.height, 0);
	}
	if (x + w < viddef.width)
	{
		Draw_Fill(x + w, 0, viddef.width - (x + w), viddef.height, 0);
	}
	if (y > 0)
	{
		Draw_Fill(x, 0, w, y, 0);
	}
	if (y + h < viddef.height)
	{
		Draw_Fill(x, y + h, w, viddef.height - (y + h), 0);
	}

	Draw_StretchRaw(x, y, w, h, cin.width, cin.height, cin.pic);

	return true;
}

void
SCR_PlayCinematic(char *arg)
{
	int width, height;
	byte *palette;
	char name[MAX_OSPATH], *dot;

	/* make sure background music is not playing */
#ifdef CDA
	CDAudio_Stop();
#endif
#ifdef OGG
	OGG_Stop();
#endif

	cl.cinematicframe = 0;
	dot = strstr(arg, ".");

	/* static pcx image */
	if (dot && !strcmp(dot, ".pcx"))
	{
		Com_sprintf(name, sizeof(name), "pics/%s", arg);
		SCR_LoadPCX(name, &cin.pic, &palette, &cin.width, &cin.height);
		cl.cinematicframe = -1;
		cl.cinematictime = 1;
		SCR_EndLoadingPlaque();
		cls.state = ca_active;

		if (!cin.pic)
		{
			Com_Printf("%s not found.\n", name);
			cl.cinematictime = 0;
		}
		else
		{
			memcpy(cl.cinematicpalette, palette, sizeof(cl.cinematicpalette));
			Z_Free(palette);
		}

		return;
	}

	Com_sprintf(name, sizeof(name), "video/%s", arg);
	FS_FOpenFile(name, &cl.cinematic_file, false);

	if (!cl.cinematic_file)
	{
		SCR_FinishCinematic();
		cl.cinematictime = 0; /* done */
		return;
	}

	SCR_EndLoadingPlaque();

	cls.state = ca_active;

	FS_Read(&width, 4, cl.cinematic_file);
	FS_Read(&height, 4, cl.cinematic_file);
	cin.width = LittleLong(width);
	cin.height = LittleLong(height);

	FS_Read(&cin.s_rate, 4, cl.cinematic_file);
	cin.s_rate = LittleLong(cin.s_rate);
	FS_Read(&cin.s_width, 4, cl.cinematic_file);
	cin.s_width = LittleLong(cin.s_width);
	FS_Read(&cin.s_channels, 4, cl.cinematic_file);
	cin.s_channels = LittleLong(cin.s_channels);

	Huff1TableInit();

	cl.cinematicframe = 0;
	cin.pic = SCR_ReadNextFrame();
	cl.cinematictime = Sys_Milliseconds();
}

