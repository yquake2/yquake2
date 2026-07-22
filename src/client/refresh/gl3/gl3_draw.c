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
 * Drawing of all images that are not textures
 *
 * =======================================================================
 */

#include "header/local.h"

#include "header/DG_dynarr.h"

unsigned d_8to24table[256];

gl3image_t *draw_chars;

static GLuint vbo2D = 0, ebo2D = 0, vao2D = 0, vao2Dcolor = 0; // vao2D is for textured rendering, vao2Dcolor for color-only

int gl3_num3Ddraws = 0, gl3_num2Ddraws = 0, gl3_numBufferVtxData = 0, gl3_numBufferUniforms = 0;

typedef struct gl3_drawVert2D_s {
	float x, y, s, t;
} gl3_drawVert2D;

DA_TYPEDEF(gl3_drawVert2D, Vtx2DArray_t);
DA_TYPEDEF(GLushort, UShortArray_t);
// dynamic arrays to batch all consecutive 2D draws with same texture to reduce drawcalls
static Vtx2DArray_t vtxBuf = {0};
static UShortArray_t idxBuf = {0};
static GLuint lastBatchTexture = 0;

void
GL3_Draw_InitLocal(void)
{
	/* load console characters */
	draw_chars = R_FindPic("conchars", (findimage_t)GL3_FindImage);
	if (!draw_chars)
	{
		Com_Error(ERR_FATAL, "%s: Couldn't load pics/conchars.pcx",
			__func__);
	}

	// set up attribute layout for 2D textured rendering
	glGenVertexArrays(1, &vao2D);
	glBindVertexArray(vao2D);

	glGenBuffers(1, &vbo2D);
	GL3_BindVBO(vbo2D);
	glGenBuffers(1, &ebo2D);

	GL3_UseProgram(gl3state.si2D.shaderProgram);

	glEnableVertexAttribArray(GL3_ATTRIB_POSITION);
	// Note: the glVertexAttribPointer() configuration is stored in the VAO, not the shader or sth
	//       (that's why I use one VAO per 2D shader)
	qglVertexAttribPointer(GL3_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);

	glEnableVertexAttribArray(GL3_ATTRIB_TEXCOORD);
	qglVertexAttribPointer(GL3_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 2*sizeof(float));

	// set up attribute layout for 2D flat color rendering

	glGenVertexArrays(1, &vao2Dcolor);
	glBindVertexArray(vao2Dcolor);

	GL3_BindVBO(vbo2D); // yes, both VAOs share the same VBO

	GL3_UseProgram(gl3state.si2Dcolor.shaderProgram);

	glEnableVertexAttribArray(GL3_ATTRIB_POSITION);
	qglVertexAttribPointer(GL3_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), 0);

	GL3_BindVAO(0);
}

void
GL3_Draw_ShutdownLocal(void)
{
	glDeleteBuffers(1, &ebo2D);
	ebo2D = 0;
	glDeleteBuffers(1, &vbo2D);
	vbo2D = 0;
	glDeleteVertexArrays(1, &vao2D);
	vao2D = 0;
	glDeleteVertexArrays(1, &vao2Dcolor);
	vao2Dcolor = 0;

	da_free(vtxBuf);
	da_free(idxBuf);
}

void
GL3_DrawCurrent2Dbatch()
{
	int numVtx = da_count(vtxBuf);
	if(numVtx == 0)
		return;

	if(gl3_scrap_dirty)
		GL3_Scrap_Upload();

	GL3_UseProgram(gl3state.si2D.shaderProgram);
	GL3_Bind(lastBatchTexture);

	GL3_BindVAO(vao2D);

	// Note: while vao2D "remembers" its vbo for drawing, binding the vao does *not*
	//       implicitly bind the vbo, so I need to explicitly bind it before glBufferData()
	GL3_BindVBO(vbo2D);
	glBufferData(GL_ARRAY_BUFFER, sizeof(gl3_drawVert2D)*numVtx, vtxBuf.p, GL_STREAM_DRAW);

	GL3_BindEBO(ebo2D);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, da_count(idxBuf)*sizeof(GLushort), idxBuf.p, GL_STREAM_DRAW);
	glDrawElements(GL_TRIANGLES, da_count(idxBuf), GL_UNSIGNED_SHORT, NULL);

	++gl3_numBufferVtxData;
	++gl3_num2Ddraws;

	lastBatchTexture = 0;
	da_clear(vtxBuf);
	da_clear(idxBuf);
}

static void
drawTexturedRectangle(GLuint texNum, float x, float y, float w, float h,
                      float sl, float tl, float sh, float th)
{
	/*
	 *  x,y+h      x+w,y+h
	 * sl,th--------sh,th
	 *  |             |
	 *  |             |
	 *  |             |
	 * sl,tl--------sh,tl
	 *  x,y        x+w,y
	 */

	if((lastBatchTexture != 0 && texNum != lastBatchTexture) || da_count(vtxBuf)+4 > UINT16_MAX)
		GL3_DrawCurrent2Dbatch();

	lastBatchTexture = texNum;

	GLushort firstIdx = da_count(vtxBuf);

	gl3_drawVert2D* addVtx = da_addn_uninit(vtxBuf, 4);
	//                            X,   Y,   S,  T
	addVtx[0] = (gl3_drawVert2D){ x,   y+h, sl, th };
	addVtx[1] = (gl3_drawVert2D){ x,   y,   sl, tl };
	addVtx[2] = (gl3_drawVert2D){ x+w, y+h, sh, th };
	addVtx[3] = (gl3_drawVert2D){ x+w, y,   sh, tl };

	GLushort* addIdx = da_addn_uninit(idxBuf, 6);
	addIdx[0] = firstIdx;  // first triangle of rectangle
	addIdx[1] = firstIdx+1;
	addIdx[2] = firstIdx+2;
	addIdx[3] = firstIdx+1; // second triangle
	addIdx[4] = firstIdx+3;
	addIdx[5] = firstIdx+2;
}

// bind the texture before calling this
static void
drawTexturedRectangleNow(float x, float y, float w, float h,
                      float sl, float tl, float sh, float th)
{
	/*
	 *  x,y+h      x+w,y+h
	 * sl,th--------sh,th
	 *  |             |
	 *  |             |
	 *  |             |
	 * sl,tl--------sh,tl
	 *  x,y        x+w,y
	 */

	// in case some batched 2D draws are outstanding, draw them now
	// to preserve draw order
	GL3_DrawCurrent2Dbatch();

	if (gl3_scrap_dirty)
		GL3_Scrap_Upload();

	gl3_drawVert2D vBuf[4] = {
	//    X,   Y,   S,  T
		{ x,   y+h, sl, th },
		{ x,   y,   sl, tl },
		{ x+w, y+h, sh, th },
		{ x+w, y,   sh, tl }
	};

	GL3_BindVAO(vao2D);

	// Note: while vao2D "remembers" its vbo for drawing, binding the vao does *not*
	//       implicitly bind the vbo, so I need to explicitly bind it before glBufferData()
	GL3_BindVBO(vbo2D);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vBuf), vBuf, GL_STREAM_DRAW);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	++gl3_numBufferVtxData;
	++gl3_num2Ddraws;

	//glMultiDrawArrays(mode, first, count, drawcount) ??
}

/*
 * Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be
 * smoothly scrolled off.
 */
void
GL3_Draw_CharScaled(int x, int y, int num, float scale)
{
	int row, col;
	float frow, fcol, size, scaledSize;
	num &= 255;

	if ((num & 127) == 32)
	{
		return; /* space */
	}

	if (y <= -8)
	{
		return; /* totally off screen */
	}

	row = num >> 4;
	col = num & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;

	scaledSize = 8*scale;

	drawTexturedRectangle( draw_chars->texnum, x, y, scaledSize, scaledSize,
	                       draw_chars->sl + fcol * (draw_chars->sh - draw_chars->sl),
	                       draw_chars->tl + frow * (draw_chars->th - draw_chars->tl),
	                       draw_chars->sl + (fcol + size) * (draw_chars->sh - draw_chars->sl),
	                       draw_chars->tl + (frow + size) * (draw_chars->th - draw_chars->tl) );
}

// DG: copy of DrawStringScaled(), so we can draw some stats right here in the render DLL
void
GL3_DrawStringScaled(int x, int y, const char *s, float factor)
{
	while (*s)
	{
		GL3_Draw_CharScaled(x, y, *s ^ 0x80, factor);
		x += 8*factor;
		s++;
	}
}

gl3image_t *
GL3_Draw_FindPic(const char *name)
{
	return R_FindPic(name, (findimage_t)GL3_FindImage);
}

void
GL3_Draw_GetPicSize(int *w, int *h, const char *pic)
{
	gl3image_t *gl;

	gl = R_FindPic(pic, (findimage_t)GL3_FindImage);

	if (!gl)
	{
		*w = *h = -1;
		return;
	}

	*w = gl->width;
	*h = gl->height;
}

void
GL3_Draw_StretchPic(int x, int y, int w, int h, const char *pic)
{
	gl3image_t *gl = R_FindPic(pic, (findimage_t)GL3_FindImage);

	if (!gl)
	{
		Com_Printf("Can't find pic: %s\n", pic);
		return;
	}

	drawTexturedRectangle(gl->texnum, x, y, w, h, gl->sl, gl->tl, gl->sh, gl->th);
}

void
GL3_Draw_PicScaled(int x, int y, const char *pic, float factor)
{
	gl3image_t *gl = R_FindPic(pic, (findimage_t)GL3_FindImage);
	if (!gl)
	{
		Com_Printf("Can't find pic: %s\n", pic);
		return;
	}

	drawTexturedRectangle(gl->texnum, x, y, gl->width*factor, gl->height*factor, gl->sl, gl->tl, gl->sh, gl->th);
}

void
GL3_Draw_PicScaledCol(int x, int y, const char *pic, float factor, const float color[3])
{
	gl3image_t *gl = R_FindPic(pic, (findimage_t)GL3_FindImage);
	if (!gl)
	{
		Com_Printf("Can't find pic: %s\n", pic);
		return;
	}

	if (gl3_scrap_dirty)
		GL3_Scrap_Upload();

	gl3state.uniCommonData.color = HMM_Vec4(color[0], color[1], color[2], 1.0f);
	GL3_UpdateUBOCommon();

	GL3_UseProgram(gl3state.si2Dtinted.shaderProgram);
	GL3_Bind(gl->texnum);

	// NOTE: this function (and this shader) are only used for the crosshair
	//       so use the simple immediate (unbatched) draw function
	drawTexturedRectangleNow(x, y, gl->width*factor, gl->height*factor, gl->sl, gl->tl, gl->sh, gl->th);

	gl3state.uniCommonData.color = HMM_Vec4(1, 1, 1, 1);
	GL3_UpdateUBOCommon();
}

/*
 * This repeats a 64*64 tile graphic to fill
 * the screen around a sized down
 * refresh window.
 */
void
GL3_Draw_TileClear(int x, int y, int w, int h, const char *pic)
{
	if(w <= 0 || h <= 0)
		return;

	gl3image_t *image = R_FindPic(pic, (findimage_t)GL3_FindImage);
	if (!image)
	{
		Com_Printf("Can't find pic: %s\n", pic);
		return;
	}

	drawTexturedRectangle(image->texnum, x, y, w, h, x/64.0f, y/64.0f, (x+w)/64.0f, (y+h)/64.0f);
}

void
GL3_DrawFrameBufferObject(int x, int y, int w, int h, GLuint fboTexture, const float v_blend[4])
{
	qboolean underwater = (r_newrefdef.rdflags & RDF_UNDERWATER) != 0;
	gl3ShaderInfo_t* shader = underwater ? &gl3state.si2DpostProcessWater
	                                     : &gl3state.si2DpostProcess;
	GL3_UseProgram(shader->shaderProgram);
	GL3_Bind(fboTexture);

	if(underwater && shader->uniLmScalesOrTime != -1)
	{
		glUniform1f(shader->uniLmScalesOrTime, r_newrefdef.time);
	}
	if(shader->uniVblend != -1)
	{
		glUniform4fv(shader->uniVblend, 1, v_blend);
	}

	drawTexturedRectangleNow(x, y, w, h, 0, 1, 1, 0);
}

/*
 * Fills a box of pixels with a single color
 */
void
GL3_Draw_Fill(int x, int y, int w, int h, int c)
{
	union
	{
		unsigned c;
		byte v[4];
	} color;

	if ((unsigned)c > 255)
	{
		Com_Error(ERR_FATAL, "Draw_Fill: bad color");
	}

	color.c = d_8to24table[c];

	// in case some batched 2D draws are outstanding, draw them now
	// to preserve draw order
	GL3_DrawCurrent2Dbatch();

	GLfloat vBuf[8] = {
	//  X,   Y
		x,   y+h,
		x,   y,
		x+w, y+h,
		x+w, y
	};

	for(int i=0; i<3; ++i)
	{
		gl3state.uniCommonData.color.Elements[i] = color.v[i] * (1.0f/255.0f);
	}
	gl3state.uniCommonData.color.A = 1.0f;

	GL3_UpdateUBOCommon();

	GL3_UseProgram(gl3state.si2Dcolor.shaderProgram);
	GL3_BindVAO(vao2Dcolor);

	GL3_BindVBO(vbo2D);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vBuf), vBuf, GL_STREAM_DRAW);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	++gl3_numBufferVtxData;
	++gl3_num2Ddraws;
}

// in GL1 this is called R_Flash() (which just calls R_PolyBlend())
// now implemented in 2D mode and called after SetGL2D() because
// it's pretty similar to GL3_Draw_FadeScreen()
void
GL3_Draw_Flash(const float color[4], float x, float y, float w, float h)
{
	if (gl_polyblend->value == 0)
	{
		return;
	}

	// in case some batched 2D draws are outstanding, draw them now
	// to preserve draw order
	GL3_DrawCurrent2Dbatch();

	GLfloat vBuf[8] = {
	//  X,   Y
		x,   y+h,
		x,   y,
		x+w, y+h,
		x+w, y
	};

	glEnable(GL_BLEND);

	for(int i=0; i<4; ++i)
		gl3state.uniCommonData.color.Elements[i] = color[i];

	GL3_UpdateUBOCommon();

	GL3_UseProgram(gl3state.si2Dcolor.shaderProgram);

	GL3_BindVAO(vao2Dcolor);

	GL3_BindVBO(vbo2D);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vBuf), vBuf, GL_STREAM_DRAW);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	++gl3_numBufferVtxData;
	++gl3_num2Ddraws;

	glDisable(GL_BLEND);
}

void
GL3_Draw_FadeScreen(void)
{
	float color[4] = {0, 0, 0, 0.6f};
	GL3_Draw_Flash(color, 0, 0, vid.width, vid.height);
}

void
GL3_Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int bits)
{
	int i, j;

	GL3_Bind(0);

	unsigned image32[320*240]; /* was 256 * 256, but we want a bit more space */

	unsigned* img = image32;

	if (bits == 32)
	{
		img = (unsigned *)data;
	}
	else
	{
		if(cols*rows > 320*240)
		{
			/* in case there is a bigger video after all,
			 * malloc enough space to hold the frame */
			img = (unsigned*)malloc(cols*rows*4);
		}

		for(i=0; i<rows; ++i)
		{
			int rowOffset = i*cols;
			for(j=0; j<cols; ++j)
			{
				byte palIdx = data[rowOffset+j];
				img[rowOffset+j] = gl3_rawpalette[palIdx];
			}
		}
	}

	GL3_UseProgram(gl3state.si2D.shaderProgram);

	GLuint glTex;
	glGenTextures(1, &glTex);
	GL3_SelectTMU(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, glTex);

	glTexImage2D(GL_TEXTURE_2D, 0, gl3_tex_solid_format,
	             cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);

	if(img != image32 && img != (unsigned *)data)
	{
		free(img);
	}

	// Note: gl_filter_min could be GL_*_MIPMAP_* so we can't use it for min filter here (=> no mipmaps)
	//       but gl_filter_max (either GL_LINEAR or GL_NEAREST) should do the trick.
	GLint filter = (r_videos_unfiltered->value == 0) ? gl_filter_max : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	// NOTE: this is only used for videos and only called once per frame (or not at all)
	drawTexturedRectangleNow(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f);

	glDeleteTextures(1, &glTex);

	GL3_Bind(0);
}

/*
 * Called at the end of the frame, after 2D (UI) rendering is done.
 * Does some internal housekeeping, then swaps the buffers
 * and shows the next frame.
 */
void GL3_EndFrame(void)
{
	GL3_DrawCurrent2Dbatch();

	// by saving those values into a variable and setting them to 0 afterwards,
	// gl3_show_draw_stats can include its own drawcalls (from previous frame)
	int num3D = gl3_num3Ddraws;
	int num2D = gl3_num2Ddraws;
	int numBufVtx = gl3_numBufferVtxData;
	int numBufUni = gl3_numBufferUniforms;
	gl3_num3Ddraws = 0;
	gl3_num2Ddraws = 0;
	gl3_numBufferVtxData = 0;
	gl3_numBufferUniforms = 0;

	if(gl3_show_draw_stats->value)
	{
		float factor = 2.0f; // TODO: like SCR_GetConsoleScale()
		char stbuf[128] = {0};
		snprintf(stbuf, sizeof(stbuf), "3D drawcalls: %d - 2D drawcalls: %d - buffer vtx data: %d - buffer uniforms: %d",
		         num3D, num2D, numBufVtx, numBufUni);

		GL3_DrawStringScaled(10, 5, stbuf, factor);
		GL3_DrawCurrent2Dbatch();
	}

#ifdef YQ2_GL3_GLES
	if(gl3config.discardfb && gl_discardfb->value != 0.0f)
	{
		static const GLenum attachments[] = { GL_COLOR, GL_DEPTH, GL_STENCIL };
		// depth and stencil buffer can be discarded now (and that seems to be fastest)
		glDiscardFramebufferEXT(GL_FRAMEBUFFER, 3, &attachments[1]);

		GL3_SwapWindow();
		// ... but the color buffer must be discarded after swapping buffers
		// probably because it is what's getting rendered
		glDiscardFramebufferEXT(GL_FRAMEBUFFER, 1, attachments);
	}
	else
#endif
	{
		GL3_SwapWindow();
	}
}
