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
 * Surface generation and drawing
 *
 * =======================================================================
 */

#include <assert.h>

#include "header/local.h"

int c_visible_lightmaps;
int c_visible_textures;
static vec3_t modelorg; /* relative to viewpoint */
static msurface_t *gl3_alpha_surfaces;

gl3lightmapstate_t gl3_lms;

#define BACKFACE_EPSILON 0.01

extern gl3image_t gl3textures[MAX_GL3TEXTURES];
extern int numgl3textures;

void GL3_SurfInit(void)
{
	// init the VAO and VBO for the standard vertexdata: 7 floats
	// (X, Y, Z), (S, T), (LMS, LMT) - last two for lightmap

	glGenVertexArrays(1, &gl3state.vao3D);
	GL3_BindVAO(gl3state.vao3D);

	glGenBuffers(1, &gl3state.vbo3D);
	GL3_BindVBO(gl3state.vbo3D);

	glEnableVertexAttribArray(GL3_ATTRIB_POSITION);
	qglVertexAttribPointer(GL3_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, VERTEXSIZE*sizeof(GLfloat), 0);

	glEnableVertexAttribArray(GL3_ATTRIB_TEXCOORD);
	qglVertexAttribPointer(GL3_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE*sizeof(GLfloat), 3*sizeof(GLfloat));

	glEnableVertexAttribArray(GL3_ATTRIB_LMTEXCOORD);
	qglVertexAttribPointer(GL3_ATTRIB_LMTEXCOORD, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE*sizeof(GLfloat), 5*sizeof(GLfloat));


	glGenVertexArrays(1, &gl3state.vaoAlias);
	GL3_BindVAO(gl3state.vaoAlias);

	glGenBuffers(1, &gl3state.vboAlias);
	GL3_BindVBO(gl3state.vboAlias);

	glEnableVertexAttribArray(GL3_ATTRIB_POSITION);
	qglVertexAttribPointer(GL3_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 0);

	glEnableVertexAttribArray(GL3_ATTRIB_TEXCOORD);
	qglVertexAttribPointer(GL3_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 3*sizeof(GLfloat));

	glEnableVertexAttribArray(GL3_ATTRIB_COLOR);
	qglVertexAttribPointer(GL3_ATTRIB_COLOR, 2, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 5*sizeof(GLfloat));
}

void GL3_SurfShutdown(void)
{
	glDeleteBuffers(1, &gl3state.vbo3D);
	gl3state.vbo3D = 0;
	glDeleteVertexArrays(1, &gl3state.vao3D);
	gl3state.vao3D = 0;

	glDeleteBuffers(1, &gl3state.vboAlias);
	gl3state.vboAlias = 0;
	glDeleteVertexArrays(1, &gl3state.vaoAlias);
	gl3state.vaoAlias = 0;
}

/*
 * Returns true if the box is completely outside the frustom
 */
static qboolean
CullBox(vec3_t mins, vec3_t maxs)
{
	int i;

	if (!gl_cull->value)
	{
		return false;
	}

	for (i = 0; i < 4; i++)
	{
		if (BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2)
		{
			return true;
		}
	}

	return false;
}

/*
 * Returns the proper texture for a given time and base texture
 */
static gl3image_t *
TextureAnimation(mtexinfo_t *tex)
{
	int c;

	if (!tex->next)
	{
		return tex->image;
	}

	c = currententity->frame % tex->numframes;

	while (c)
	{
		tex = tex->next;
		c--;
	}

	return tex->image;
}

void
GL3_DrawGLPoly(glpoly_t *p)
{
	float* v = p->verts[0];

	GL3_UseProgram(gl3state.si3D.shaderProgram); // TODO: needed each time?! maybe call this once in DrawTextureChains()?

	GL3_BindVAO(gl3state.vao3D);
	GL3_BindVBO(gl3state.vbo3D);
	glBufferData(GL_ARRAY_BUFFER, VERTEXSIZE*sizeof(GLfloat)*p->numverts, v, GL_STREAM_DRAW);

	glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
}

void
GL3_DrawGLFlowingPoly(msurface_t *fa)
{
	int i;
	float *v;
	glpoly_t *p;
	float scroll;

	p = fa->polys;

	scroll = -64.0f * ((gl3_newrefdef.time / 40.0f) - (int)(gl3_newrefdef.time / 40.0f));

	if (scroll == 0.0f)
	{
		scroll = -64.0f;
	}

	if(gl3state.uni3DData.scroll != scroll)
	{
		gl3state.uni3DData.scroll = scroll;
		GL3_UpdateUBO3D();
	}

	GL3_UseProgram(gl3state.si3Dflow.shaderProgram);

	GL3_BindVAO(gl3state.vao3D);
	GL3_BindVBO(gl3state.vbo3D);

	glBufferData(GL_ARRAY_BUFFER, VERTEXSIZE*sizeof(GLfloat)*p->numverts, p->verts[0], GL_STREAM_DRAW);
	glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
}

static void
DrawTriangleOutlines(void)
{
	int i, j;
	glpoly_t *p;

	STUB_ONCE("TODO: Implement for gl_showtris support!");

#if 0
	if (!gl_showtris->value)
	{
		return;
	}

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glColor4f(1, 1, 1, 1);

	for (i = 0; i < MAX_LIGHTMAPS; i++)
	{
		msurface_t *surf;

		for (surf = gl3_lms.lightmap_surfaces[i];
				surf != 0;
				surf = surf->lightmapchain)
		{
			p = surf->polys;

			for ( ; p; p = p->chain)
			{
				for (j = 2; j < p->numverts; j++)
				{
					GLfloat vtx[12];
					unsigned int k;

					for (k=0; k<3; k++)
					{
						vtx[0+k] = p->verts [ 0 ][ k ];
						vtx[3+k] = p->verts [ j - 1 ][ k ];
						vtx[6+k] = p->verts [ j ][ k ];
						vtx[9+k] = p->verts [ 0 ][ k ];
					}

					glEnableClientState( GL_VERTEX_ARRAY );

					glVertexPointer( 3, GL_FLOAT, 0, vtx );
					glDrawArrays( GL_LINE_STRIP, 0, 4 );

					glDisableClientState( GL_VERTEX_ARRAY );
				}
			}
		}
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
#endif // 0
}

static void
DrawGLPolyChain(glpoly_t *p, float soffset, float toffset)
{
	STUB_ONCE("TODO: Implement!");
#if 0
	if ((soffset == 0) && (toffset == 0))
	{
		for ( ; p != 0; p = p->chain)
		{
			float *v;

			v = p->verts[0];

			if (v == NULL)
			{
				fprintf(stderr, "BUGFIX: R_DrawGLPolyChain: v==NULL\n");
				return;
			}

			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );

			glVertexPointer( 3, GL_FLOAT, VERTEXSIZE*sizeof(GLfloat), v );
			glTexCoordPointer( 2, GL_FLOAT, VERTEXSIZE*sizeof(GLfloat), v+5 );
			glDrawArrays( GL_TRIANGLE_FAN, 0, p->numverts );

			glDisableClientState( GL_VERTEX_ARRAY );
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
		}
	}
	else
	{
		for ( ; p != 0; p = p->chain)
		{
			float *v;
			int j;

			v = p->verts[0];

			GLfloat tex[2*p->numverts];
			unsigned int index_tex = 0;

			for ( j = 0; j < p->numverts; j++, v += VERTEXSIZE )
			{
				tex[index_tex++] = v [ 5 ] - soffset;
				tex[index_tex++] = v [ 6 ] - toffset;
			}

			v = p->verts [ 0 ];

			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );

			glVertexPointer( 3, GL_FLOAT, VERTEXSIZE*sizeof(GLfloat), v );
			glTexCoordPointer( 2, GL_FLOAT, 0, tex );
			glDrawArrays( GL_TRIANGLE_FAN, 0, p->numverts );

			glDisableClientState( GL_VERTEX_ARRAY );
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
		}
	}
#endif // 0
}

/*
 * This routine takes all the given light mapped surfaces
 * in the world and blends them into the framebuffer.
 */
static void
BlendLightmaps(void)
{
	int i;
	msurface_t *surf, *newdrawsurf = 0;

	/* don't bother if we're set to fullbright */
	if (gl_fullbright->value)
	{
		return;
	}

	if (!gl3_worldmodel->lightdata)
	{
		return;
	}

	STUB_ONCE("TODO: Implement!");
#if 0
	/* don't bother writing Z */
	glDepthMask(0);

	/* set the appropriate blending mode unless
	   we're only looking at the lightmaps. */
	if (!gl_lightmap->value)
	{
		glEnable(GL_BLEND);

		if (gl_saturatelighting->value)
		{
			glBlendFunc(GL_ONE, GL_ONE);
		}
		else
		{
			glBlendFunc(GL_ZERO, GL_SRC_COLOR);
		}
	}

	if (currentmodel == gl3_worldmodel)
	{
		c_visible_lightmaps = 0;
	}

	/* render static lightmaps first */
	for (i = 1; i < MAX_LIGHTMAPS; i++)
	{
		if (gl3_lms.lightmap_surfaces[i])
		{
			if (currentmodel == gl3_worldmodel)
			{
				c_visible_lightmaps++;
			}

			GL3_Bind(gl_state.lightmap_textures + i);

			for (surf = gl3_lms.lightmap_surfaces[i];
				 surf != 0;
				 surf = surf->lightmapchain)
			{
				if (surf->polys)
				{
					// Apply overbright bits to the static lightmaps
					if (gl_overbrightbits->value)
					{
						R_TexEnv(GL_COMBINE_EXT);
						glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, gl_overbrightbits->value);
					}

					DrawGLPolyChain(surf->polys, 0, 0);
				}
			}
		}
	}

	/* render dynamic lightmaps */
	if (gl_dynamic->value)
	{
		LM_InitBlock();

		GL3_Bind(gl_state.lightmap_textures + 0);

		if (currentmodel == gl3_worldmodel)
		{
			c_visible_lightmaps++;
		}

		newdrawsurf = gl3_lms.lightmap_surfaces[0];

		for (surf = gl3_lms.lightmap_surfaces[0];
			 surf != 0;
			 surf = surf->lightmapchain)
		{
			int smax, tmax;
			byte *base;

			smax = (surf->extents[0] >> 4) + 1;
			tmax = (surf->extents[1] >> 4) + 1;

			if (LM_AllocBlock(smax, tmax, &surf->dlight_s, &surf->dlight_t))
			{
				base = gl3_lms.lightmap_buffer;
				base += (surf->dlight_t * BLOCK_WIDTH +
						surf->dlight_s) * LIGHTMAP_BYTES;

				R_BuildLightMap(surf, base, BLOCK_WIDTH * LIGHTMAP_BYTES);
			}
			else
			{
				msurface_t *drawsurf;

				/* upload what we have so far */
				LM_UploadBlock(true);

				/* draw all surfaces that use this lightmap */
				for (drawsurf = newdrawsurf;
					 drawsurf != surf;
					 drawsurf = drawsurf->lightmapchain)
				{
					if (drawsurf->polys)
					{
						// Apply overbright bits to the dynamic lightmaps
						if (gl_overbrightbits->value)
						{
							R_TexEnv(GL_COMBINE_EXT);
							glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, gl_overbrightbits->value);
						}

						DrawGLPolyChain(drawsurf->polys,
								(drawsurf->light_s - drawsurf->dlight_s) * (1.0 / 128.0),
								(drawsurf->light_t - drawsurf->dlight_t) * (1.0 / 128.0));
					}
				}

				newdrawsurf = drawsurf;

				/* clear the block */
				LM_InitBlock();

				/* try uploading the block now */
				if (!LM_AllocBlock(smax, tmax, &surf->dlight_s, &surf->dlight_t))
				{
					ri.Sys_Error(ERR_FATAL,
							"Consecutive calls to LM_AllocBlock(%d,%d) failed (dynamic)\n",
							smax, tmax);
				}

				base = gl3_lms.lightmap_buffer;
				base += (surf->dlight_t * BLOCK_WIDTH +
						surf->dlight_s) * LIGHTMAP_BYTES;

				R_BuildLightMap(surf, base, BLOCK_WIDTH * LIGHTMAP_BYTES);
			}
		}

		/* draw remainder of dynamic lightmaps that haven't been uploaded yet */
		if (newdrawsurf)
		{
			LM_UploadBlock(true);
		}

		for (surf = newdrawsurf; surf != 0; surf = surf->lightmapchain)
		{
			if (surf->polys)
			{
				// Apply overbright bits to the remainder lightmaps
				if (gl_overbrightbits->value)
				{
					R_TexEnv(GL_COMBINE_EXT);
					glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, gl_overbrightbits->value);
				}

				DrawGLPolyChain(surf->polys,
						(surf->light_s - surf->dlight_s) * (1.0 / 128.0),
						(surf->light_t - surf->dlight_t) * (1.0 / 128.0));
			}
		}
	}

	/* restore state */
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(1);
#endif // 0
}

static void
RenderBrushPoly(msurface_t *fa)
{
	int maps;
	gl3image_t *image;
	qboolean is_dynamic = false;

	c_brush_polys++;

	image = TextureAnimation(fa->texinfo);



	if (fa->flags & SURF_DRAWTURB)
	{
		GL3_Bind(image->texnum);

		STUB_ONCE("TODO: do something about inverse intensity on water surfaces b/c they have no lightmap!");
#if 0 // TODO
		/* This is a hack ontop of a hack. Warping surfaces like those generated
		   by R_EmitWaterPolys() don't have a lightmap. Original Quake II therefore
		   negated the global intensity on those surfaces, because otherwise they
		   would show up much too bright. When we implemented overbright bits this
		   hack modified the global GL state in an incompatible way. So implement
		   a new hack, based on overbright bits... Depending on the value set to
		   gl_overbrightbits the result is different:

		    0: Old behaviour.
		    1: No overbright bits on the global scene but correct lightning on
		       warping surfaces.
		    2: Overbright bits on the global scene but not on warping surfaces.
		        They oversaturate otherwise. */
		if (gl_overbrightbits->value)
		{
			R_TexEnv(GL_COMBINE_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1);
		}
		else
		{
			R_TexEnv(GL_MODULATE);
			glColor4f(gl_state.inverse_intensity, gl_state.inverse_intensity,
					  gl_state.inverse_intensity, 1.0f);
		}
#endif // 0

		GL3_EmitWaterPolys(fa);

		//R_TexEnv(GL_REPLACE); TODO

		return;
	}
	else
	{
		GL3_Bind(image->texnum);

		// R_TexEnv(GL_REPLACE); TODO!
	}

	if (fa->texinfo->flags & SURF_FLOWING)
	{
		GL3_DrawGLFlowingPoly(fa);
	}
	else
	{
		GL3_DrawGLPoly(fa->polys);
	}

	/* check for lightmap modification */
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++)
	{
		if (gl3_newrefdef.lightstyles[fa->styles[maps]].white !=
			fa->cached_light[maps])
		{
			goto dynamic;
		}
	}

	/* dynamic this frame or dynamic previously */
	if (fa->dlightframe == gl3_framecount)
	{
	dynamic:

		if (gl_dynamic->value)
		{
			if (!(fa->texinfo->flags &
				  (SURF_SKY | SURF_TRANS33 |
				   SURF_TRANS66 | SURF_WARP)))
			{
				is_dynamic = true;
			}
		}
	}

	STUB_ONCE("TODO: lightmap support (=> esp. LM textures)")
#if 0
	// TODO: 2D texture array für lightmaps?
	if (is_dynamic)
	{
		if (((fa->styles[maps] >= 32) ||
			 (fa->styles[maps] == 0)) &&
			  (fa->dlightframe != gl3_framecount))
		{
			unsigned temp[34 * 34];
			int smax, tmax;

			smax = (fa->extents[0] >> 4) + 1;
			tmax = (fa->extents[1] >> 4) + 1;

			GL3_BuildLightMap(fa, (void *)temp, smax * 4);
			GL3_SetCacheState(fa);

			GL3_Bind(gl3state.lightmap_textures + fa->lightmaptexturenum);

			glTexSubImage2D(GL_TEXTURE_2D, 0, fa->light_s, fa->light_t,
					smax, tmax, GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE, temp);

			fa->lightmapchain = gl3_lms.lightmap_surfaces[fa->lightmaptexturenum];
			gl3_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa;
		}
		else
		{
			fa->lightmapchain = gl3_lms.lightmap_surfaces[0];
			gl3_lms.lightmap_surfaces[0] = fa;
		}
	}
	else
#endif // 0
	{
		fa->lightmapchain = gl3_lms.lightmap_surfaces[fa->lightmaptexturenum];
		gl3_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa;
	}

}

/*
 * Draw water surfaces and windows.
 * The BSP tree is waled front to back, so unwinding the chain
 * of alpha_surfaces will draw back to front, giving proper ordering.
 */
void
GL3_DrawAlphaSurfaces(void)
{
	STUB_ONCE("TODO: implement!");

	msurface_t *s;
	float intens;

	/* go back to the world matrix */
	gl3state.uni3DData.transModelViewMat4 = gl3_world_matrix;
	GL3_UpdateUBO3D();

	glEnable(GL_BLEND);
	//R_TexEnv(GL_MODULATE);

	/* the textures are prescaled up for a better
	   lighting range, so scale it back down */
	//intens = gl_state.inverse_intensity;
	STUB_ONCE("Something about inverse intensity??");

	for (s = gl3_alpha_surfaces; s != NULL; s = s->texturechain)
	{
		GL3_Bind(s->texinfo->image->texnum);
		c_brush_polys++;
		float alpha = 1.0f;
		if (s->texinfo->flags & SURF_TRANS33)
		{
			alpha = 0.333f;
		}
		else if (s->texinfo->flags & SURF_TRANS66)
		{
			alpha = 0.666f;
		}
		if(alpha != gl3state.uni3DData.alpha)
		{
			gl3state.uni3DData.alpha = alpha;
			GL3_UpdateUBO3D();
		}

		if (s->flags & SURF_DRAWTURB)
		{
			GL3_EmitWaterPolys(s);
		}
		else if (s->texinfo->flags & SURF_FLOWING)
		{
			GL3_DrawGLFlowingPoly(s);
		}
		else
		{
			GL3_DrawGLPoly(s->polys);
		}
	}

	gl3state.uni3DData.alpha = 1.0f;
	GL3_UpdateUBO3D();

	//R_TexEnv(GL_REPLACE);
	//glColor4f(1, 1, 1, 1);
	glDisable(GL_BLEND);

	gl3_alpha_surfaces = NULL;
}

static void
DrawTextureChains(void)
{
	int i;
	msurface_t *s;
	gl3image_t *image;

	c_visible_textures = 0;

	for (i = 0, image = gl3textures; i < numgl3textures; i++, image++)
	{
		if (!image->registration_sequence)
		{
			continue;
		}

		s = image->texturechain;

		if (!s)
		{
			continue;
		}

		c_visible_textures++;

		for ( ; s; s = s->texturechain)
		{
			RenderBrushPoly(s);
		}

		image->texturechain = NULL;
	}

	STUB_ONCE("TODO: do something about R_TexEnv()!");
	// R_TexEnv(GL_REPLACE);
}

static void
DrawInlineBModel(void)
{
	int i, k;
	cplane_t *pplane;
	float dot;
	msurface_t *psurf;
	dlight_t *lt;

	/* calculate dynamic lighting for bmodel */
	if (!gl_flashblend->value)
	{
		lt = gl3_newrefdef.dlights;

		for (k = 0; k < gl3_newrefdef.num_dlights; k++, lt++)
		{
			GL3_MarkLights(lt, 1 << k, currentmodel->nodes + currentmodel->firstnode);
		}
	}

	psurf = &currentmodel->surfaces[currentmodel->firstmodelsurface];

	if (currententity->flags & RF_TRANSLUCENT)
	{
		STUB_ONCE("TODO: implement for OpenGL3 (esp. the alpha 0.25 part in color?)");
		glEnable(GL_BLEND);
		/* TODO: if you change this, also do at the end of the function
		glEnable(GL_BLEND);
		glColor4f(1, 1, 1, 0.25);
		R_TexEnv(GL_MODULATE);
		*/
	}

	/* draw texture */
	for (i = 0; i < currentmodel->nummodelsurfaces; i++, psurf++)
	{
		/* find which side of the node we are on */
		pplane = psurf->plane;

		dot = DotProduct(modelorg, pplane->normal) - pplane->dist;

		/* draw the polygon */
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if (psurf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
			{
				/* add to the translucent chain */
				psurf->texturechain = gl3_alpha_surfaces;
				gl3_alpha_surfaces = psurf;
			}
			else
			{
				RenderBrushPoly(psurf);
			}
		}
	}

	if (!(currententity->flags & RF_TRANSLUCENT))
	{
		BlendLightmaps();
	}
	else
	{
		//STUB_ONCE("TODO: implement for OpenGL3");

		glDisable(GL_BLEND);
		/*
		glColor4f(1, 1, 1, 1);
		R_TexEnv(GL_REPLACE);
		*/
	}
}

void
GL3_DrawBrushModel(entity_t *e)
{
	vec3_t mins, maxs;
	int i;
	qboolean rotated;

	if (currentmodel->nummodelsurfaces == 0)
	{
		return;
	}

	currententity = e;
	//gl3state.currenttextures[0] = gl3state.currenttextures[1] = -1;
	gl3state.currenttexture = -1;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;

		for (i = 0; i < 3; i++)
		{
			mins[i] = e->origin[i] - currentmodel->radius;
			maxs[i] = e->origin[i] + currentmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd(e->origin, currentmodel->mins, mins);
		VectorAdd(e->origin, currentmodel->maxs, maxs);
	}

	if (CullBox(mins, maxs))
	{
		return;
	}

	if (gl_zfix->value)
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
	}

	STUB_ONCE("TODO: something about setting color to 1,1,1,1");
	//glColor4f(1, 1, 1, 1);
	memset(gl3_lms.lightmap_surfaces, 0, sizeof(gl3_lms.lightmap_surfaces));

	VectorSubtract(gl3_newrefdef.vieworg, e->origin, modelorg);

	if (rotated)
	{
		vec3_t temp;
		vec3_t forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}



	//glPushMatrix();
	hmm_mat4 oldMat = gl3state.uni3DData.transModelViewMat4;

	e->angles[0] = -e->angles[0];
	e->angles[2] = -e->angles[2];
	GL3_RotateForEntity(e);
	e->angles[0] = -e->angles[0];
	e->angles[2] = -e->angles[2];


	STUB_ONCE("TODO: something about R_TexEnv() and gl_lightmap");
#if 0
	R_TexEnv(GL_REPLACE);

	if (gl_lightmap->value)
	{
		R_TexEnv(GL_REPLACE);
	}
	else
	{
		R_TexEnv(GL_MODULATE);
	}
#endif // 0

	DrawInlineBModel();

	// glPopMatrix();
	gl3state.uni3DData.transModelViewMat4 = oldMat;
	GL3_UpdateUBO3D();

	if (gl_zfix->value)
	{
		glDisable(GL_POLYGON_OFFSET_FILL);
	}
}

static void
RecursiveWorldNode(mnode_t *node)
{
	int c, side, sidebit;
	cplane_t *plane;
	msurface_t *surf, **mark;
	mleaf_t *pleaf;
	float dot;
	gl3image_t *image;

	if (node->contents == CONTENTS_SOLID)
	{
		return; /* solid */
	}

	if (node->visframe != gl3_visframecount)
	{
		return;
	}

	if (CullBox(node->minmaxs, node->minmaxs + 3))
	{
		return;
	}

	/* if a leaf node, draw stuff */
	if (node->contents != -1)
	{
		pleaf = (mleaf_t *)node;

		/* check for door connected areas */
		if (gl3_newrefdef.areabits)
		{
			if (!(gl3_newrefdef.areabits[pleaf->area >> 3] & (1 << (pleaf->area & 7))))
			{
				return; /* not visible */
			}
		}

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = gl3_framecount;
				mark++;
			}
			while (--c);
		}

		return;
	}

	/* node is just a decision point, so go down the apropriate
	   sides find which side of the node we are on */
	plane = node->plane;

	switch (plane->type)
	{
		case PLANE_X:
			dot = modelorg[0] - plane->dist;
			break;
		case PLANE_Y:
			dot = modelorg[1] - plane->dist;
			break;
		case PLANE_Z:
			dot = modelorg[2] - plane->dist;
			break;
		default:
			dot = DotProduct(modelorg, plane->normal) - plane->dist;
			break;
	}

	if (dot >= 0)
	{
		side = 0;
		sidebit = 0;
	}
	else
	{
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

	/* recurse down the children, front side first */
	RecursiveWorldNode(node->children[side]);

	/* draw stuff */
	for (c = node->numsurfaces,
		 surf = gl3_worldmodel->surfaces + node->firstsurface;
		 c; c--, surf++)
	{
		if (surf->visframe != gl3_framecount)
		{
			continue;
		}

		if ((surf->flags & SURF_PLANEBACK) != sidebit)
		{
			continue; /* wrong side */
		}

		if (surf->texinfo->flags & SURF_SKY)
		{
			/* just adds to visible sky bounds */
			GL3_AddSkySurface(surf);
		}
		else if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
		{
			/* add to the translucent chain */
			surf->texturechain = gl3_alpha_surfaces;
			gl3_alpha_surfaces = surf;
			gl3_alpha_surfaces->texinfo->image = TextureAnimation(surf->texinfo);
		}
		else
		{
			/* the polygon is visible, so add it to the texture sorted chain */
			image = TextureAnimation(surf->texinfo);
			surf->texturechain = image->texturechain;
			image->texturechain = surf;
		}
	}

	/* recurse down the back side */
	RecursiveWorldNode(node->children[!side]);
}

void
GL3_DrawWorld(void)
{
	entity_t ent;

	STUB_ONCE("TODO: gl_drawworld cvar");
	/*
	if (!gl_drawworld->value)
	{
		return;
	}*/

	if (gl3_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		return;
	}

	currentmodel = gl3_worldmodel;

	VectorCopy(gl3_newrefdef.vieworg, modelorg);

	/* auto cycle the world frame for texture animation */
	memset(&ent, 0, sizeof(ent));
	ent.frame = (int)(gl3_newrefdef.time * 2);
	currententity = &ent;

	gl3state.currenttexture = -1; //s[0] = gl3state.currenttextures[1] = -1;

	STUB_ONCE("somehow set color to 1,1,1,1 maybe");
	//glColor4f(1, 1, 1, 1);

	memset(gl3_lms.lightmap_surfaces, 0, sizeof(gl3_lms.lightmap_surfaces));

	GL3_ClearSkyBox();
	RecursiveWorldNode(gl3_worldmodel->nodes);
	DrawTextureChains();
	BlendLightmaps();
	GL3_DrawSkyBox();
	DrawTriangleOutlines();

	currententity = NULL;
}

/*
 * Mark the leaves and nodes that are
 * in the PVS for the current cluster
 */
void
GL3_MarkLeaves(void)
{
	byte *vis;
	byte fatvis[MAX_MAP_LEAFS / 8];
	mnode_t *node;
	int i, c;
	mleaf_t *leaf;
	int cluster;

	if ((gl3_oldviewcluster == gl3_viewcluster) &&
		(gl3_oldviewcluster2 == gl3_viewcluster2) &&
		!gl_novis->value &&
		(gl3_viewcluster != -1))
	{
		return;
	}

	/* development aid to let you run around
	   and see exactly where the pvs ends */
	if (gl_lockpvs->value)
	{
		return;
	}

	gl3_visframecount++;
	gl3_oldviewcluster = gl3_viewcluster;
	gl3_oldviewcluster2 = gl3_viewcluster2;

	if (gl_novis->value || (gl3_viewcluster == -1) || !gl3_worldmodel->vis)
	{
		/* mark everything */
		for (i = 0; i < gl3_worldmodel->numleafs; i++)
		{
			gl3_worldmodel->leafs[i].visframe = gl3_visframecount;
		}

		for (i = 0; i < gl3_worldmodel->numnodes; i++)
		{
			gl3_worldmodel->nodes[i].visframe = gl3_visframecount;
		}

		return;
	}

	vis = GL3_Mod_ClusterPVS(gl3_viewcluster, gl3_worldmodel);

	/* may have to combine two clusters because of solid water boundaries */
	if (gl3_viewcluster2 != gl3_viewcluster)
	{
		memcpy(fatvis, vis, (gl3_worldmodel->numleafs + 7) / 8);
		vis = GL3_Mod_ClusterPVS(gl3_viewcluster2, gl3_worldmodel);
		c = (gl3_worldmodel->numleafs + 31) / 32;

		for (i = 0; i < c; i++)
		{
			((int *)fatvis)[i] |= ((int *)vis)[i];
		}

		vis = fatvis;
	}

	for (i = 0, leaf = gl3_worldmodel->leafs;
		 i < gl3_worldmodel->numleafs;
		 i++, leaf++)
	{
		cluster = leaf->cluster;

		if (cluster == -1)
		{
			continue;
		}

		if (vis[cluster >> 3] & (1 << (cluster & 7)))
		{
			node = (mnode_t *)leaf;

			do
			{
				if (node->visframe == gl3_visframecount)
				{
					break;
				}

				node->visframe = gl3_visframecount;
				node = node->parent;
			}
			while (node);
		}
	}
}

