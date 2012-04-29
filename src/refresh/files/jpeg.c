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
 * The JPEG image format
 *
 * =======================================================================
 */

#ifdef RETEXTURE

#include "../header/local.h"
#include <jpeglib.h>
#include <jerror.h>

void jpeg_memory_src(j_decompress_ptr cinfo, unsigned char *inbuffer, unsigned long insize);

void
jpg_null(j_decompress_ptr cinfo)
{
}

boolean
jpg_fill_input_buffer(j_decompress_ptr cinfo)
{
	ri.Con_Printf(PRINT_ALL, "Premature end of JPEG data\n");
	return 1;
}

void
jpg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	cinfo->src->next_input_byte += (size_t)num_bytes;
	cinfo->src->bytes_in_buffer -= (size_t)num_bytes;
}

void
jpeg_mem_src(j_decompress_ptr cinfo, unsigned char *mem, unsigned long len)
{
	cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(struct jpeg_source_mgr));
	cinfo->src->init_source = jpg_null;
	cinfo->src->fill_input_buffer = jpg_fill_input_buffer;
	cinfo->src->skip_input_data = jpg_skip_input_data;
	cinfo->src->resync_to_restart = jpeg_resync_to_restart;
	cinfo->src->term_source = jpg_null;
	cinfo->src->bytes_in_buffer = len;
	cinfo->src->next_input_byte = mem;
}

void
LoadJPG(char *origname, byte **pic, int *width, int *height)
{
	struct jpeg_decompress_struct cinfo;
	char filename[256];
	struct jpeg_error_mgr jerr;
	int len;
	byte *rawdata, *rgbadata, *scanline, *p, *q;
	unsigned int rawsize, i;

	/* Add the extension */
	len = strlen(origname);

	if (strcmp(origname + len - 4, ".jpg"))
	{
		strncpy(filename, origname, 256);
		strncat(filename, ".jpg", 255);
	}
	else
	{
		strncpy(filename, origname, 256);
	}

	*pic = NULL;

	/* Load JPEG file into memory */
	rawsize = ri.FS_LoadFile(filename, (void **)&rawdata);

	if (!rawdata)
	{
		return;
	}

	if ((rawsize < 10) || (rawdata[6] != 'J') || (rawdata[7] != 'F') || (rawdata[8] != 'I') || (rawdata[9] != 'F'))
	{
		ri.Con_Printf(PRINT_ALL, "Invalid JPEG header: %s\n", filename);
		ri.FS_FreeFile(rawdata);
		return;
	}

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_mem_src(&cinfo, (unsigned char *)rawdata, (unsigned long)rawsize);
	jpeg_read_header(&cinfo, true);
	jpeg_start_decompress(&cinfo);

	if ((cinfo.output_components != 3) && (cinfo.output_components != 4))
	{
		ri.Con_Printf(PRINT_ALL, "Invalid JPEG colour components\n");
		jpeg_destroy_decompress(&cinfo);
		ri.FS_FreeFile(rawdata);
		return;
	}

	/* Allocate Memory for decompressed image */
	rgbadata = malloc(cinfo.output_width * cinfo.output_height * 4);

	if (!rgbadata)
	{
		ri.Con_Printf(PRINT_ALL, "Insufficient memory for JPEG buffer\n");
		jpeg_destroy_decompress(&cinfo);
		ri.FS_FreeFile(rawdata);
		return;
	}

	/* Pass sizes to output */
	*width = cinfo.output_width;
	*height = cinfo.output_height;

	/* Allocate Scanline buffer */
	scanline = malloc(cinfo.output_width * 3);

	if (!scanline)
	{
		ri.Con_Printf(PRINT_ALL, "Insufficient memory for JPEG scanline buffer\n");
		free(rgbadata);
		jpeg_destroy_decompress(&cinfo);
		ri.FS_FreeFile(rawdata);
		return;
	}

	/* Read Scanlines, and expand from RGB to RGBA */
	q = rgbadata;

	while (cinfo.output_scanline < cinfo.output_height)
	{
		p = scanline;
		jpeg_read_scanlines(&cinfo, &scanline, 1);

		for (i = 0; i < cinfo.output_width; i++)
		{
			q[0] = p[0];
			q[1] = p[1];
			q[2] = p[2];
			q[3] = 255;
			p += 3;
			q += 4;
		}
	}

	free(scanline);
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	*pic = rgbadata;
}

#endif /* RETEXTURE */
