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
LoadTGA ( char *name, byte **pic, int *width, int *height )
{
	int columns, rows, numPixels;
	byte    *pixbuf;
	int row, column;
	byte    *buf_p;
	byte    *buffer;
	int length;
	TargaHeader targa_header;
	byte            *targa_rgba;
	byte tmp [ 2 ];

	*pic = NULL;

	/* load the file */
	length = ri.FS_LoadFile( name, (void **) &buffer );

	if ( !buffer )
	{
		ri.Con_Printf( PRINT_DEVELOPER, "Bad tga file %s\n", name );
		return;
	}

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	tmp [ 0 ] = buf_p [ 0 ];
	tmp [ 1 ] = buf_p [ 1 ];
	targa_header.colormap_index = LittleShort( *( (short *) tmp ) );
	buf_p += 2;
	tmp [ 0 ] = buf_p [ 0 ];
	tmp [ 1 ] = buf_p [ 1 ];
	targa_header.colormap_length = LittleShort( *( (short *) tmp ) );
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort( *( (short *) buf_p ) );
	buf_p += 2;
	targa_header.y_origin = LittleShort( *( (short *) buf_p ) );
	buf_p += 2;
	targa_header.width = LittleShort( *( (short *) buf_p ) );
	buf_p += 2;
	targa_header.height = LittleShort( *( (short *) buf_p ) );
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if ( ( targa_header.image_type != 2 ) &&
		 ( targa_header.image_type != 10 ) )
	{
		ri.Sys_Error( ERR_DROP, "LoadTGA: Only type 2 and 10 targa RGB images supported\n" );
	}

	if ( ( targa_header.colormap_type != 0 ) ||
		 ( ( targa_header.pixel_size != 32 ) && ( targa_header.pixel_size != 24 ) ) )
	{
		ri.Sys_Error( ERR_DROP, "LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n" );
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if ( width )
	{
		*width = columns;
	}

	if ( height )
	{
		*height = rows;
	}

	targa_rgba = malloc( numPixels * 4 );
	*pic = targa_rgba;

	if ( targa_header.id_length != 0 )
	{
		buf_p += targa_header.id_length; /* skip TARGA image comment */
	}

	if ( targa_header.image_type == 2 ) /* Uncompressed, RGB images */
	{
		for ( row = rows - 1; row >= 0; row-- )
		{
			pixbuf = targa_rgba + row * columns * 4;

			for ( column = 0; column < columns; column++ )
			{
				unsigned char red, green, blue, alphabyte;

				switch ( targa_header.pixel_size )
				{
					case 24:

						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = 255;
						break;
					case 32:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = *buf_p++;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						break;
				}
			}
		}
	}
	else if ( targa_header.image_type == 10 ) /* Runlength encoded RGB images */
	{
		unsigned char red, green, blue, alphabyte, packetHeader, packetSize, j;

		for ( row = rows - 1; row >= 0; row-- )
		{
			pixbuf = targa_rgba + row * columns * 4;

			for ( column = 0; column < columns; )
			{
				packetHeader = *buf_p++;
				packetSize = 1 + ( packetHeader & 0x7f );

				if ( packetHeader & 0x80 ) /* run-length packet */
				{
					switch ( targa_header.pixel_size )
					{
						case 24:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = 255;
							break;
						case 32:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = *buf_p++;
							break;
						default:
							blue = 0;
							green = 0;
							red = 0;
							alphabyte = 0;
							break;
					}

					for ( j = 0; j < packetSize; j++ )
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;

						/* run spans across rows */
						if ( column == columns ) 
						{
							column = 0;

							if ( row > 0 )
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
				else /* non run-length packet */
				{
					for ( j = 0; j < packetSize; j++ )
					{
						switch ( targa_header.pixel_size )
						{
							case 24:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = 255;
								break;
							case 32:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = *buf_p++;
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = alphabyte;
								break;
						}

						column++;

						if ( column == columns ) /* pixel packet run spans across rows */
						{
							column = 0;

							if ( row > 0 )
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
			}

		breakOut:;
		}
	}

	ri.FS_FreeFile( buffer );
}  

