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
 * The Targa image format
 *
 * =======================================================================
 */

#include "../header/local.h"

typedef struct _TargaHeader
{
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;

void
LoadTGA(char *origname, byte **pic, int *width, int *height)
{
	unsigned rows, numPixels;
	byte *pixbuf;
	int row, column, columns;
	byte *buf_p;
	byte *buffer;
	TargaHeader targa_header;
	byte *targa_rgba;
	int length;
	int pixel_size;
	char name[256];
	int len;

	/* Add the extension */
	len = strlen(origname);

	if (strcmp(origname + len - 4, ".tga"))
	{
		strncpy(name, origname, 256);
		strncat(name, ".tga", 255);
	}
	else
	{
		strncpy(name, origname, 256);
	}

	*pic = NULL;

	/* load the file */
	length = ri.FS_LoadFile(name, (void **)&buffer);

	if (!buffer)
	{
		return;
	}

	if (length < 18)
	{
		ri.Sys_Error(ERR_DROP, "LoadTGA: %s has an invalid file size", name);
	}

	buf_p = buffer;

	targa_header.id_length = buf_p[0];
	targa_header.colormap_type = buf_p[1];
	targa_header.image_type = buf_p[2];

	memcpy(&targa_header.colormap_index, &buf_p[3], 2);
	memcpy(&targa_header.colormap_length, &buf_p[5], 2);
	targa_header.colormap_size = buf_p[7];
	memcpy(&targa_header.x_origin, &buf_p[8], 2);
	memcpy(&targa_header.y_origin, &buf_p[10], 2);
	memcpy(&targa_header.width, &buf_p[12], 2);
	memcpy(&targa_header.height, &buf_p[14], 2);
	targa_header.pixel_size = buf_p[16];
	targa_header.attributes = buf_p[17];

	targa_header.colormap_index = LittleShort(targa_header.colormap_index);
	targa_header.colormap_length = LittleShort(targa_header.colormap_length);
	targa_header.x_origin = LittleShort(targa_header.x_origin);
	targa_header.y_origin = LittleShort(targa_header.y_origin);
	targa_header.width = LittleShort(targa_header.width);
	targa_header.height = LittleShort(targa_header.height);

	buf_p += 18;

	if ((targa_header.image_type != 2) &&
		(targa_header.image_type != 10) &&
		(targa_header.image_type != 3))
	{
		ri.Sys_Error( ERR_DROP, "LoadTGA (%s): Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported", name);
	}

	if (targa_header.colormap_type != 0)
	{
		ri.Sys_Error(ERR_DROP, "LoadTGA (%s): colormaps not supported", name);
	}

	if (((targa_header.pixel_size != 32) && (targa_header.pixel_size != 24)) && (targa_header.image_type != 3))
	{
		ri.Sys_Error( ERR_DROP, "LoadTGA (%s): Only 32 or 24 bit images supported (no colormaps)", name);
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows * 4;

	if (width)
	{
		*width = columns;
	}

	if (height)
	{
		*height = rows;
	}

	if (!columns || !rows || (numPixels > 0x7FFFFFFF) || (numPixels / columns / 4 != rows))
	{
		ri.Sys_Error(ERR_DROP, "LoadTGA (%s): Invalid image size", name);
	}

	targa_rgba = malloc(numPixels);
	*pic = targa_rgba;

	if (targa_header.id_length != 0)
	{
		buf_p += targa_header.id_length; /* skip TARGA image comment */
	}

	pixel_size = targa_header.pixel_size;

	if ((targa_header.image_type == 2) || (targa_header.image_type == 3))
	{
		/* Uncompressed RGB or gray scale image */
		switch (pixel_size)
		{
			case 24:

				if (buf_p - buffer + (3 * columns * rows) > length)
				{
					ri.Sys_Error( ERR_DROP, "LoadTGA: (%s): Pointer passed end of file - corrupt TGA file", name);
				}

				for (row = rows - 1; row >= 0; row--)
				{
					pixbuf = targa_rgba + row * columns * 4;

					for (column = 0; column < columns; column++)
					{
						unsigned char red, green, blue;

						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = 255;
					}
				}

				break;

			case 32:

				if (buf_p - buffer + (4 * columns * rows) > length)
				{
					ri.Sys_Error( ERR_DROP, "LoadTGA: (%s): Pointer passed end of file - corrupt TGA file", name);
				}

				for (row = rows - 1; row >= 0; row--)
				{
					pixbuf = targa_rgba + row * columns * 4;

					for (column = 0; column < columns; column++)
					{
						unsigned char red, green, blue, alphabyte;

						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = *buf_p++;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
					}
				}

				break;

			case 8:

				if (buf_p - buffer + (1 * columns * rows) > length)
				{
					ri.Sys_Error( ERR_DROP, "LoadTGA: (%s): Pointer passed end of file - corrupt TGA file", name);
				}

				for (row = rows - 1; row >= 0; row--)
				{
					pixbuf = targa_rgba + row * columns * 4;

					for (column = 0; column < columns; column++)
					{
						unsigned char red, green, blue;

						blue = *buf_p++;
						green = blue;
						red = blue;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = 255;
					}
				}

				break;
		}
	}
	else if (targa_header.image_type == 10)
	{
		/* Runlength encoded RGB images */
		byte red, green, blue, alphabyte, packetHeader;
		unsigned packetSize, j;

		red = 0;
		green = 0;
		blue = 0;
		alphabyte = 0xff;

		for (row = rows - 1; row >= 0; row--)
		{
			pixbuf = targa_rgba + row * columns * 4;

			for (column = 0; column < columns; )
			{
				packetHeader = *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);

				if (packetHeader & 0x80)
				{
					/* run-length packet */
					switch (pixel_size)
					{
						case 24:

							if (buf_p - buffer + (3) > length)
							{
								ri.Sys_Error( ERR_DROP, "LoadTGA: (%s): Pointer passed end of file - corrupt TGA file", name);
							}

							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = 255;
							break;
						case 32:

							if (buf_p - buffer + (4) > length)
							{
								ri.Sys_Error( ERR_DROP, "LoadTGA: (%s): Pointer passed end of file - corrupt TGA file", name);
							}

							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = *buf_p++;
							break;
						default:
							break;
					}

					for (j = 0; j < packetSize; j++)
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;

						if (column == columns)
						{
							/* run spans across rows */
							column = 0;

							if (row > 0)
							{
								row--;
							}
							else
							{
								goto breakOut;
							}

							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
				else
				{
					/* non run-length packet */
					switch (pixel_size)
					{
						case 24:

							if (buf_p - buffer + (3 * packetSize) > length)
							{
								ri.Sys_Error( ERR_DROP, "LoadTGA: (%s): Pointer passed end of file - corrupt TGA file", name);
							}

							for (j = 0; j < packetSize; j++)
							{
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = 255;

								column++;

								if (column == columns)
								{
									/* pixel packet run spans across rows */
									column = 0;

									if (row > 0)
									{
										row--;
									}
									else
									{
										goto breakOut;
									}

									pixbuf = targa_rgba + row * columns * 4;
								}
							}

							break;

						case 32:

							if (buf_p - buffer + (4 * packetSize) > length)
							{
								ri.Sys_Error( ERR_DROP, "LoadTGA: (%s): Pointer passed end of file - corrupt TGA file", name);
							}

							for (j = 0; j < packetSize; j++)
							{
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = *buf_p++;
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = alphabyte;

								column++;

								if (column == columns)
								{
									/* pixel packet run spans across rows */
									column = 0;

									if (row > 0)
									{
										row--;
									}
									else
									{
										goto breakOut;
									}

									pixbuf = targa_rgba + row * columns * 4;
								}
							}

							break;

						default:
							break;
					}
				}
			}

		breakOut:;
		}
	}

	if (targa_header.attributes & 0x20)
	{
		byte *temp;
		temp = malloc(numPixels);

		if (!temp)
		{
			ri.Sys_Error(ERR_FATAL, "LoadTGA: not enough memory");
		}

		ri.Con_Printf(PRINT_DEVELOPER, "LoadTGA: Bottom-to-top TGA file (slow): %s\n", name);
		memcpy(temp, targa_rgba, numPixels);

		for (row = 0; row < rows; row++)
		{
			memcpy(targa_rgba + (row * columns * 4), temp + (rows - row - 1) * columns * 4, columns * 4);
		}

		free(temp);
	}

	ri.FS_FreeFile(buffer);
}
