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
	// init the VAO and VBO for the standard vertexdata: 10 floats and 1 uint
	// (X, Y, Z), (S, T), (LMS, LMT), (normX, normY, normZ) ; lightFlags - last two groups for lightmap/dynlights

	glGenVertexArrays(1, &gl3state.vao3D);
	GL3_BindVAO(gl3state.vao3D);

	glGenBuffers(1, &gl3state.vbo3D);
	GL3_BindVBO(gl3state.vbo3D);

	if(gl3config.useBigVBO)
	{
		gl3state.vbo3Dsize = 5*1024*1024; // a 5MB buffer seems to work well?
		gl3state.vbo3DcurOffset = 0;
		glBufferData(GL_ARRAY_BUFFER, gl3state.vbo3Dsize, NULL, GL_STREAM_DRAW); // allocate/reserve that data
	}

	glEnableVertexAttribArray(GL3_ATTRIB_POSITION);
	qglVertexAttribPointer(GL3_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(gl3_3D_vtx_t), 0);

	glEnableVertexAttribArray(GL3_ATTRIB_TEXCOORD);
	qglVertexAttribPointer(GL3_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(gl3_3D_vtx_t), offsetof(gl3_3D_vtx_t, texCoord));

	glEnableVertexAttribArray(GL3_ATTRIB_LMTEXCOORD);
	qglVertexAttribPointer(GL3_ATTRIB_LMTEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(gl3_3D_vtx_t), offsetof(gl3_3D_vtx_t, lmTexCoord));

	glEnableVertexAttribArray(GL3_ATTRIB_NORMAL);
	qglVertexAttribPointer(GL3_ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(gl3_3D_vtx_t), offsetof(gl3_3D_vtx_t, normal));

	glEnableVertexAttribArray(GL3_ATTRIB_LIGHTFLAGS);
	qglVertexAttribIPointer(GL3_ATTRIB_LIGHTFLAGS, 1, GL_UNSIGNED_INT, sizeof(gl3_3D_vtx_t), offsetof(gl3_3D_vtx_t, lightFlags));



	// init VAO and VBO for model vertexdata: 9 floats
	// (X,Y,Z), (S,T), (R,G,B,A)

	glGenVertexArrays(1, &gl3state.vaoAlias);
	GL3_BindVAO(gl3state.vaoAlias);

	glGenBuffers(1, &gl3state.vboAlias);
	GL3_BindVBO(gl3state.vboAlias);

	glEnableVertexAttribArray(GL3_ATTRIB_POSITION);
	qglVertexAttribPointer(GL3_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 0);

	glEnableVertexAttribArray(GL3_ATTRIB_TEXCOORD);
	qglVertexAttribPointer(GL3_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 3*sizeof(GLfloat));

	glEnableVertexAttribArray(GL3_ATTRIB_COLOR);
	qglVertexAttribPointer(GL3_ATTRIB_COLOR, 4, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 5*sizeof(GLfloat));

	glGenBuffers(1, &gl3state.eboAlias);

	// init VAO and VBO for particle vertexdata: 9 floats
	// (X,Y,Z), (point_size,distace_to_camera), (R,G,B,A)

	glGenVertexArrays(1, &gl3state.vaoParticle);
	GL3_BindVAO(gl3state.vaoParticle);

	glGenBuffers(1, &gl3state.vboParticle);
	GL3_BindVBO(gl3state.vboParticle);

	glEnableVertexAttribArray(GL3_ATTRIB_POSITION);
	qglVertexAttribPointer(GL3_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 0);

	// TODO: maybe move point size and camera origin to UBO and calculate distance in vertex shader
	glEnableVertexAttribArray(GL3_ATTRIB_TEXCOORD); // it's abused for (point_size, distance) here..
	qglVertexAttribPointer(GL3_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 3*sizeof(GLfloat));

	glEnableVertexAttribArray(GL3_ATTRIB_COLOR);
	qglVertexAttribPointer(GL3_ATTRIB_COLOR, 4, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 5*sizeof(GLfloat));
}

void GL3_SurfShutdown(void)
{
	glDeleteBuffers(1, &gl3state.vbo3D);
	gl3state.vbo3D = 0;
	glDeleteVertexArrays(1, &gl3state.vao3D);
	gl3state.vao3D = 0;

	glDeleteBuffers(1, &gl3state.eboAlias);
	gl3state.eboAlias = 0;
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


static void
SetLightFlags(msurface_t *surf)
{
	unsigned int lightFlags = 0;
	if (surf->dlightframe == gl3_framecount)
	{
		lightFlags = surf->dlightbits;
	}

	gl3_3D_vtx_t* verts = surf->polys->vertices;

	int numVerts = surf->polys->numverts;
	for(int i=0; i<numVerts; ++i)
	{
		verts[i].lightFlags = lightFlags;
	}
}

static void
SetAllLightFlags(msurface_t *surf)
{
	unsigned int lightFlags = 0xffffffff;

	gl3_3D_vtx_t* verts = surf->polys->vertices;

	int numVerts = surf->polys->numverts;
	for(int i=0; i<numVerts; ++i)
	{
		verts[i].lightFlags = lightFlags;
	}
}

void
GL3_DrawGLPoly(msurface_t *fa)
{
	glpoly_t *p = fa->polys;

	GL3_BindVAO(gl3state.vao3D);
	GL3_BindVBO(gl3state.vbo3D);

	GL3_BufferAndDraw3D(p->vertices, p->numverts, GL_TRIANGLE_FAN);
}

void
GL3_DrawGLFlowingPoly(msurface_t *fa)
{
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

	GL3_BindVAO(gl3state.vao3D);
	GL3_BindVBO(gl3state.vbo3D);

	GL3_BufferAndDraw3D(p->vertices, p->numverts, GL_TRIANGLE_FAN);
}

static void
DrawTriangleOutlines(void)
{
	STUB_ONCE("TODO: Implement for gl_showtris support!");
#if 0
	int i, j;
	glpoly_t *p;

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
						vtx[0+k] = p->vertices [ 0 ][ k ];
						vtx[3+k] = p->vertices [ j - 1 ][ k ];
						vtx[6+k] = p->vertices [ j ][ k ];
						vtx[9+k] = p->vertices [ 0 ][ k ];
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
UpdateLMscales(const hmm_vec4 lmScales[MAX_LIGHTMAPS_PER_SURFACE], gl3ShaderInfo_t* si)
{
	int i;
	qboolean hasChanged = false;

	for(i=0; i<MAX_LIGHTMAPS_PER_SURFACE; ++i)
	{
		if(hasChanged)
		{
			si->lmScales[i] = lmScales[i];
		}
		else if(   si->lmScales[i].R != lmScales[i].R
		        || si->lmScales[i].G != lmScales[i].G
		        || si->lmScales[i].B != lmScales[i].B
		        || si->lmScales[i].A != lmScales[i].A )
		{
			si->lmScales[i] = lmScales[i];
			hasChanged = true;
		}
	}

	if(hasChanged)
	{
		glUniform4fv(si->uniLmScales, MAX_LIGHTMAPS_PER_SURFACE, si->lmScales[0].Elements);
	}
}

static void
RenderBrushPoly(msurface_t *fa)
{
	int map;
	gl3image_t *image;

	c_brush_polys++;

	image = TextureAnimation(fa->texinfo);

	if (fa->flags & SURF_DRAWTURB)
	{
		GL3_Bind(image->texnum);

		GL3_EmitWaterPolys(fa);

		return;
	}
	else
	{
		GL3_Bind(image->texnum);
	}

	hmm_vec4 lmScales[MAX_LIGHTMAPS_PER_SURFACE] = {0};
	lmScales[0] = HMM_Vec4(1.0f, 1.0f, 1.0f, 1.0f);

	GL3_BindLightmap(fa->lightmaptexturenum);

	// Any dynamic lights on this surface?
	for (map = 0; map < MAX_LIGHTMAPS_PER_SURFACE && fa->styles[map] != 255; map++)
	{
		lmScales[map].R = gl3_newrefdef.lightstyles[fa->styles[map]].rgb[0];
		lmScales[map].G = gl3_newrefdef.lightstyles[fa->styles[map]].rgb[1];
		lmScales[map].B = gl3_newrefdef.lightstyles[fa->styles[map]].rgb[2];
		lmScales[map].A = 1.0f;
	}

	if (fa->texinfo->flags & SURF_FLOWING)
	{
		GL3_UseProgram(gl3state.si3DlmFlow.shaderProgram);
		UpdateLMscales(lmScales, &gl3state.si3DlmFlow);
		GL3_DrawGLFlowingPoly(fa);
	}
	else
	{
		GL3_UseProgram(gl3state.si3Dlm.shaderProgram);
		UpdateLMscales(lmScales, &gl3state.si3Dlm);
		GL3_DrawGLPoly(fa);
	}

	// Note: lightmap chains are gone, lightmaps are rendered together with normal texture in one pass
}

/*
 * Draw water surfaces and windows.
 * The BSP tree is waled front to back, so unwinding the chain
 * of alpha_surfaces will draw back to front, giving proper ordering.
 */
void
GL3_DrawAlphaSurfaces(void)
{
	msurface_t *s;

	/* go back to the world matrix */
	gl3state.uni3DData.transModelMat4 = gl3_identityMat4;
	GL3_UpdateUBO3D();

	glEnable(GL_BLEND);

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
			GL3_UseProgram(gl3state.si3DtransFlow.shaderProgram);
			GL3_DrawGLFlowingPoly(s);
		}
		else
		{
			GL3_UseProgram(gl3state.si3Dtrans.shaderProgram);
			GL3_DrawGLPoly(s);
		}
	}

	gl3state.uni3DData.alpha = 1.0f;
	GL3_UpdateUBO3D();

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
			SetLightFlags(s);
			RenderBrushPoly(s);
		}

		image->texturechain = NULL;
	}

	// TODO: maybe one loop for normal faces and one for SURF_DRAWTURB ???
}

static void
RenderLightmappedPoly(msurface_t *surf)
{
	int map;
	gl3image_t *image = TextureAnimation(surf->texinfo);

	hmm_vec4 lmScales[MAX_LIGHTMAPS_PER_SURFACE] = {0};
	lmScales[0] = HMM_Vec4(1.0f, 1.0f, 1.0f, 1.0f);

	assert((surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)) == 0
			&& "RenderLightMappedPoly mustn't be called with transparent, sky or warping surfaces!");

	// Any dynamic lights on this surface?
	for (map = 0; map < MAX_LIGHTMAPS_PER_SURFACE && surf->styles[map] != 255; map++)
	{
		lmScales[map].R = gl3_newrefdef.lightstyles[surf->styles[map]].rgb[0];
		lmScales[map].G = gl3_newrefdef.lightstyles[surf->styles[map]].rgb[1];
		lmScales[map].B = gl3_newrefdef.lightstyles[surf->styles[map]].rgb[2];
		lmScales[map].A = 1.0f;
	}

	c_brush_polys++;

	GL3_Bind(image->texnum);
	GL3_BindLightmap(surf->lightmaptexturenum);

	if (surf->texinfo->flags & SURF_FLOWING)
	{
		GL3_UseProgram(gl3state.si3DlmFlow.shaderProgram);
		UpdateLMscales(lmScales, &gl3state.si3DlmFlow);
		GL3_DrawGLFlowingPoly(surf);
	}
	else
	{
		GL3_UseProgram(gl3state.si3Dlm.shaderProgram);
		UpdateLMscales(lmScales, &gl3state.si3Dlm);
		GL3_DrawGLPoly(surf);
	}
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
	lt = gl3_newrefdef.dlights;

	for (k = 0; k < gl3_newrefdef.num_dlights; k++, lt++)
	{
		GL3_MarkLights(lt, 1 << k, currentmodel->nodes + currentmodel->firstnode);
	}

	psurf = &currentmodel->surfaces[currentmodel->firstmodelsurface];

	if (currententity->flags & RF_TRANSLUCENT)
	{
		glEnable(GL_BLEND);
		/* TODO: should I care about the 0.25 part? we'll just set alpha to 0.33 or 0.66 depending on surface flag..
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
			else if(!(psurf->flags & SURF_DRAWTURB))
			{
				SetAllLightFlags(psurf);
				RenderLightmappedPoly(psurf);
			}
			else
			{
				RenderBrushPoly(psurf);
			}
		}
	}

	if (currententity->flags & RF_TRANSLUCENT)
	{
		glDisable(GL_BLEND);
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
	hmm_mat4 oldMat = gl3state.uni3DData.transModelMat4;

	e->angles[0] = -e->angles[0];
	e->angles[2] = -e->angles[2];
	GL3_RotateForEntity(e);
	e->angles[0] = -e->angles[0];
	e->angles[2] = -e->angles[2];

	DrawInlineBModel();

	// glPopMatrix();
	gl3state.uni3DData.transModelMat4 = oldMat;
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
			// calling RenderLightmappedPoly() here probably isn't optimal, rendering everything
			// through texturechains should be faster, because far less glBindTexture() is needed
			// (and it might allow batching the drawcalls of surfaces with the same texture)
#if 0
			if(!(surf->flags & SURF_DRAWTURB))
			{
				RenderLightmappedPoly(surf);
			}
			else
#endif // 0
			{
				/* the polygon is visible, so add it to the texture sorted chain */
				image = TextureAnimation(surf->texinfo);
				surf->texturechain = image->texturechain;
				image->texturechain = surf;
			}
		}
	}

	/* recurse down the back side */
	RecursiveWorldNode(node->children[!side]);
}

void
GL3_DrawWorld(void)
{
	entity_t ent;

	if (!r_drawworld->value)
	{
		return;
	}

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

	gl3state.currenttexture = -1;

	GL3_ClearSkyBox();
	RecursiveWorldNode(gl3_worldmodel->nodes);
	DrawTextureChains();
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
		!r_novis->value &&
		(gl3_viewcluster != -1))
	{
		return;
	}

	/* development aid to let you run around
	   and see exactly where the pvs ends */
	if (r_lockpvs->value)
	{
		return;
	}

	gl3_visframecount++;
	gl3_oldviewcluster = gl3_viewcluster;
	gl3_oldviewcluster2 = gl3_viewcluster2;

	if (r_novis->value || (gl3_viewcluster == -1) || !gl3_worldmodel->vis)
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

