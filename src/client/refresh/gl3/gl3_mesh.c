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
 * Mesh handling
 *
 * =======================================================================
 */

#include "header/local.h"

#include "header/DG_dynarr.h"

#define NUMVERTEXNORMALS 162
#define SHADEDOT_QUANT 16

static float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "../constants/anorms.h"
};

/* precalculated dot products for quantized angles */
static float r_avertexnormal_dots[SHADEDOT_QUANT][256] = {
#include "../constants/anormtab.h"
};

typedef float vec4_t[4];
static vec4_t s_lerped[MAX_VERTS];
vec3_t shadevector;
float shadelight[3];
float *shadedots = r_avertexnormal_dots[0];
extern vec3_t lightspot;
extern qboolean have_stencil;


DA_TYPEDEF(gl3_alias_vtx_t, AliasVtxArray_t);
DA_TYPEDEF(GLushort, UShortArray_t);
// dynamic arrays to batch all the data of a model, so we can render a model in one draw call
static AliasVtxArray_t vtxBuf = {0};
static UShortArray_t idxBuf = {0};

void
GL3_ShutdownMeshes(void)
{
	da_free(vtxBuf);
	da_free(idxBuf);
}

static void
LerpVerts(int nverts, dtrivertx_t *v, dtrivertx_t *ov,
		dtrivertx_t *verts, float *lerp, float move[3],
		float frontv[3], float backv[3])
{
	int i;

	if (currententity->flags &
		(RF_SHELL_RED | RF_SHELL_GREEN |
		 RF_SHELL_BLUE | RF_SHELL_DOUBLE |
		 RF_SHELL_HALF_DAM))
	{
		for (i = 0; i < nverts; i++, v++, ov++, lerp += 4)
		{
			float *normal = r_avertexnormals[verts[i].lightnormalindex];

			lerp[0] = move[0] + ov->v[0] * backv[0] + v->v[0] * frontv[0] +
					  normal[0] * POWERSUIT_SCALE;
			lerp[1] = move[1] + ov->v[1] * backv[1] + v->v[1] * frontv[1] +
					  normal[1] * POWERSUIT_SCALE;
			lerp[2] = move[2] + ov->v[2] * backv[2] + v->v[2] * frontv[2] +
					  normal[2] * POWERSUIT_SCALE;
		}
	}
	else
	{
		for (i = 0; i < nverts; i++, v++, ov++, lerp += 4)
		{
			lerp[0] = move[0] + ov->v[0] * backv[0] + v->v[0] * frontv[0];
			lerp[1] = move[1] + ov->v[1] * backv[1] + v->v[1] * frontv[1];
			lerp[2] = move[2] + ov->v[2] * backv[2] + v->v[2] * frontv[2];
		}
	}
}

/*
 * Interpolates between two frames and origins
 */
static void
DrawAliasFrameLerp(dmdl_t *paliashdr, float backlerp)
{
	GLenum type;
	float l;
	daliasframe_t *frame, *oldframe;
	dtrivertx_t *v, *ov, *verts;
	int *order;
	int count;
	float frontlerp;
	float alpha;
	vec3_t move, delta, vectors[3];
	vec3_t frontv, backv;
	int i;
	int index_xyz;
	float *lerp;
	// draw without texture? used for quad damage effect etc, I think
	qboolean colorOnly = 0 != (currententity->flags &
			(RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE |
			 RF_SHELL_HALF_DAM));

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames
							  + currententity->frame * paliashdr->framesize);
	verts = v = frame->verts;

	oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames
				+ currententity->oldframe * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

	if (currententity->flags & RF_TRANSLUCENT)
	{
		alpha = currententity->alpha;
	}
	else
	{
		alpha = 1.0;
	}

	if (colorOnly)
	{
		GL3_UseProgram(gl3state.si3DaliasColor.shaderProgram);
	}
	else
	{
		GL3_UseProgram(gl3state.si3Dalias.shaderProgram);
	}

	frontlerp = 1.0 - backlerp;

	/* move should be the delta back to the previous frame * backlerp */
	VectorSubtract(currententity->oldorigin, currententity->origin, delta);
	AngleVectors(currententity->angles, vectors[0], vectors[1], vectors[2]);

	move[0] = DotProduct(delta, vectors[0]); /* forward */
	move[1] = -DotProduct(delta, vectors[1]); /* left */
	move[2] = DotProduct(delta, vectors[2]); /* up */

	VectorAdd(move, oldframe->translate, move);

	for (i = 0; i < 3; i++)
	{
		move[i] = backlerp * move[i] + frontlerp * frame->translate[i];

		frontv[i] = frontlerp * frame->scale[i];
		backv[i] = backlerp * oldframe->scale[i];
	}

	lerp = s_lerped[0];

	LerpVerts(paliashdr->num_xyz, v, ov, verts, lerp, move, frontv, backv);

	assert(sizeof(gl3_alias_vtx_t) == 9*sizeof(GLfloat));

	// all the triangle fans and triangle strips of this model will be converted to
	// just triangles: the vertices stay the same and are batched in vtxBuf,
	// but idxBuf will contain indices to draw them all as GL_TRIANGLE
	// this way there's only one draw call (and two glBufferData() calls)
	// instead of (at least) dozens. *greatly* improves performance.

	// so first clear out the data from last call to this function
	// (the buffers are static global so we don't have malloc()/free() for each rendered model)
	da_clear(vtxBuf);
	da_clear(idxBuf);

	while (1)
	{
		GLushort nextVtxIdx = da_count(vtxBuf);

		/* get the vertex count and primitive type */
		count = *order++;

		if (!count)
		{
			break; /* done */
		}

		if (count < 0)
		{
			count = -count;

			type = GL_TRIANGLE_FAN;
		}
		else
		{
			type = GL_TRIANGLE_STRIP;
		}

		gl3_alias_vtx_t* buf = da_addn_uninit(vtxBuf, count);

		if (colorOnly)
		{
			int i;
			for(i=0; i<count; ++i)
			{
				int j=0;
				gl3_alias_vtx_t* cur = &buf[i];
				index_xyz = order[2];
				order += 3;

				for(j=0; j<3; ++j)
				{
					cur->pos[j] = s_lerped[index_xyz][j];
					cur->color[j] = shadelight[j];
				}
				cur->color[3] = alpha;
			}
		}
		else
		{
			int i;
			for(i=0; i<count; ++i)
			{
				int j=0;
				gl3_alias_vtx_t* cur = &buf[i];
				/* texture coordinates come from the draw list */
				cur->texCoord[0] = ((float *) order)[0];
				cur->texCoord[1] = ((float *) order)[1];

				index_xyz = order[2];

				order += 3;

				/* normals and vertexes come from the frame list */
				// shadedots is set in GL3DrawAliasModel() according to rotation (around Z axis I think)
				// to one of 16 (SHADEDOT_QUANT) presets in r_avertexnormal_dots
				l = shadedots[verts[index_xyz].lightnormalindex];

				for(j=0; j<3; ++j)
				{
					cur->pos[j] = s_lerped[index_xyz][j];
					cur->color[j] = l * shadelight[j];
				}
				cur->color[3] = alpha;
			}
		}

		// translate triangle fan/strip to just triangle indices
		if(type == GL_TRIANGLE_FAN)
		{
			GLushort i;
			for(i=1; i < count-1; ++i)
			{
				GLushort* add = da_addn_uninit(idxBuf, 3);

				add[0] = nextVtxIdx;
				add[1] = nextVtxIdx+i;
				add[2] = nextVtxIdx+i+1;
			}
		}
		else // triangle strip
		{
			GLushort i;
			for(i=1; i < count-2; i+=2)
			{
				// add two triangles at once, because the vertex order is different
				// for odd vs even triangles
				GLushort* add = da_addn_uninit(idxBuf, 6);

				add[0] = nextVtxIdx + i-1;
				add[1] = nextVtxIdx + i;
				add[2] = nextVtxIdx + i+1;

				add[3] = nextVtxIdx + i;
				add[4] = nextVtxIdx + i+2;
				add[5] = nextVtxIdx + i+1;
			}
			// add remaining triangle, if any
			if(i < count-1)
			{
				GLushort* add = da_addn_uninit(idxBuf, 3);

				add[0] = nextVtxIdx + i-1;
				add[1] = nextVtxIdx + i;
				add[2] = nextVtxIdx + i+1;
			}
		}
	}

	GL3_BindVAO(gl3state.vaoAlias);
	GL3_BindVBO(gl3state.vboAlias);

	glBufferData(GL_ARRAY_BUFFER, da_count(vtxBuf)*sizeof(gl3_alias_vtx_t), vtxBuf.p, GL_STREAM_DRAW);
	GL3_BindEBO(gl3state.eboAlias);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, da_count(idxBuf)*sizeof(GLushort), idxBuf.p, GL_STREAM_DRAW);
	glDrawElements(GL_TRIANGLES, da_count(idxBuf), GL_UNSIGNED_SHORT, NULL);
}
#if 0 // TODO: implemenet some time..
static void
DrawAliasShadow(dmdl_t *paliashdr, int posenum)
{

	unsigned short total;
	GLenum type;
	int *order;
	vec3_t point;
	float height = 0, lheight;
	int count;

	lheight = currententity->origin[2] - lightspot[2];
	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);
	height = -lheight + 0.1f;

	/* stencilbuffer shadows */

#if 0
	if (have_stencil && gl_stencilshadow->value)
	{
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_EQUAL, 1, 2);
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
	}
#endif // 0

	while (1)
	{
		/* get the vertex count and primitive type */
		count = *order++;

		if (!count)
		{
			break; /* done */
		}

		if (count < 0)
		{
			count = -count;

			type = GL_TRIANGLE_FAN;
		}
		else
		{
			type = GL_TRIANGLE_STRIP;
		}

		total = count;
		GLfloat vtx[3*total];
		unsigned int index_vtx = 0;

		do
		{
			/* normals and vertexes come from the frame list */
			memcpy(point, s_lerped[order[2]], sizeof(point));

			point[0] -= shadevector[0] * (point[2] + lheight);
			point[1] -= shadevector[1] * (point[2] + lheight);
			point[2] = height;

			vtx[index_vtx++] = point [ 0 ];
			vtx[index_vtx++] = point [ 1 ];
			vtx[index_vtx++] = point [ 2 ];

			order += 3;
		}
		while (--count);

#if 0
		glEnableClientState( GL_VERTEX_ARRAY );

		glVertexPointer( 3, GL_FLOAT, 0, vtx );
		glDrawArrays( type, 0, total );

		glDisableClientState( GL_VERTEX_ARRAY );
#endif // 0
	}

	/* stencilbuffer shadows */
	if (have_stencil && gl_stencilshadow->value)
	{
		//glDisable(GL_STENCIL_TEST);
	}
}
#endif // 0

static qboolean
CullAliasModel(vec3_t bbox[8], entity_t *e)
{
	int i;
	vec3_t mins, maxs;
	dmdl_t *paliashdr;
	vec3_t vectors[3];
	vec3_t thismins, oldmins, thismaxs, oldmaxs;
	daliasframe_t *pframe, *poldframe;
	vec3_t angles;

	paliashdr = (dmdl_t *)currentmodel->extradata;

	if ((e->frame >= paliashdr->num_frames) || (e->frame < 0))
	{
		R_Printf(PRINT_DEVELOPER, "R_CullAliasModel %s: no such frame %d\n",
				currentmodel->name, e->frame);
		e->frame = 0;
	}

	if ((e->oldframe >= paliashdr->num_frames) || (e->oldframe < 0))
	{
		R_Printf(PRINT_DEVELOPER, "R_CullAliasModel %s: no such oldframe %d\n",
				currentmodel->name, e->oldframe);
		e->oldframe = 0;
	}

	pframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames +
			e->frame * paliashdr->framesize);

	poldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames +
			e->oldframe * paliashdr->framesize);

	/* compute axially aligned mins and maxs */
	if (pframe == poldframe)
	{
		for (i = 0; i < 3; i++)
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i] * 255;
		}
	}
	else
	{
		for (i = 0; i < 3; i++)
		{
			thismins[i] = pframe->translate[i];
			thismaxs[i] = thismins[i] + pframe->scale[i] * 255;

			oldmins[i] = poldframe->translate[i];
			oldmaxs[i] = oldmins[i] + poldframe->scale[i] * 255;

			if (thismins[i] < oldmins[i])
			{
				mins[i] = thismins[i];
			}
			else
			{
				mins[i] = oldmins[i];
			}

			if (thismaxs[i] > oldmaxs[i])
			{
				maxs[i] = thismaxs[i];
			}
			else
			{
				maxs[i] = oldmaxs[i];
			}
		}
	}

	/* compute a full bounding box */
	for (i = 0; i < 8; i++)
	{
		vec3_t tmp;

		if (i & 1)
		{
			tmp[0] = mins[0];
		}
		else
		{
			tmp[0] = maxs[0];
		}

		if (i & 2)
		{
			tmp[1] = mins[1];
		}
		else
		{
			tmp[1] = maxs[1];
		}

		if (i & 4)
		{
			tmp[2] = mins[2];
		}
		else
		{
			tmp[2] = maxs[2];
		}

		VectorCopy(tmp, bbox[i]);
	}

	/* rotate the bounding box */
	VectorCopy(e->angles, angles);
	angles[YAW] = -angles[YAW];
	AngleVectors(angles, vectors[0], vectors[1], vectors[2]);

	for (i = 0; i < 8; i++)
	{
		vec3_t tmp;

		VectorCopy(bbox[i], tmp);

		bbox[i][0] = DotProduct(vectors[0], tmp);
		bbox[i][1] = -DotProduct(vectors[1], tmp);
		bbox[i][2] = DotProduct(vectors[2], tmp);

		VectorAdd(e->origin, bbox[i], bbox[i]);
	}

	int p, f, aggregatemask = ~0;

	for (p = 0; p < 8; p++)
	{
		int mask = 0;

		for (f = 0; f < 4; f++)
		{
			float dp = DotProduct(frustum[f].normal, bbox[p]);

			if ((dp - frustum[f].dist) < 0)
			{
				mask |= (1 << f);
			}
		}

		aggregatemask &= mask;
	}

	if (aggregatemask)
	{
		return true;
	}

	return false;
}

void
GL3_DrawAliasModel(entity_t *e)
{
	int i;
	dmdl_t *paliashdr;
	float an;
	vec3_t bbox[8];
	gl3image_t *skin;
	hmm_mat4 origProjMat = {0}; // use for left-handed rendering
	// used to restore ModelView matrix after changing it for this entities position/rotation
	hmm_mat4 origModelMat = {0};

	if (!(e->flags & RF_WEAPONMODEL))
	{
		if (CullAliasModel(bbox, e))
		{
			return;
		}
	}

	if (e->flags & RF_WEAPONMODEL)
	{
		if (gl_lefthand->value == 2)
		{
			return;
		}
	}

	paliashdr = (dmdl_t *)currentmodel->extradata;

	/* get lighting information */
	if (currententity->flags &
		(RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED |
		 RF_SHELL_BLUE | RF_SHELL_DOUBLE))
	{
		VectorClear(shadelight);

		if (currententity->flags & RF_SHELL_HALF_DAM)
		{
			shadelight[0] = 0.56;
			shadelight[1] = 0.59;
			shadelight[2] = 0.45;
		}

		if (currententity->flags & RF_SHELL_DOUBLE)
		{
			shadelight[0] = 0.9;
			shadelight[1] = 0.7;
		}

		if (currententity->flags & RF_SHELL_RED)
		{
			shadelight[0] = 1.0;
		}

		if (currententity->flags & RF_SHELL_GREEN)
		{
			shadelight[1] = 1.0;
		}

		if (currententity->flags & RF_SHELL_BLUE)
		{
			shadelight[2] = 1.0;
		}
	}
	else if (currententity->flags & RF_FULLBRIGHT)
	{
		for (i = 0; i < 3; i++)
		{
			shadelight[i] = 1.0;
		}
	}
	else
	{
		GL3_LightPoint(currententity->origin, shadelight);

		/* player lighting hack for communication back to server */
		if (currententity->flags & RF_WEAPONMODEL)
		{
			/* pick the greatest component, which should be
			   the same as the mono value returned by software */
			if (shadelight[0] > shadelight[1])
			{
				if (shadelight[0] > shadelight[2])
				{
					gl_lightlevel->value = 150 * shadelight[0];
				}
				else
				{
					gl_lightlevel->value = 150 * shadelight[2];
				}
			}
			else
			{
				if (shadelight[1] > shadelight[2])
				{
					gl_lightlevel->value = 150 * shadelight[1];
				}
				else
				{
					gl_lightlevel->value = 150 * shadelight[2];
				}
			}
		}
	}

	if (currententity->flags & RF_MINLIGHT)
	{
		for (i = 0; i < 3; i++)
		{
			if (shadelight[i] > 0.1)
			{
				break;
			}
		}

		if (i == 3)
		{
			shadelight[0] = 0.1;
			shadelight[1] = 0.1;
			shadelight[2] = 0.1;
		}
	}

	if (currententity->flags & RF_GLOW)
	{
		/* bonus items will pulse with time */
		float scale;
		float min;

		scale = 0.1 * sin(gl3_newrefdef.time * 7);

		for (i = 0; i < 3; i++)
		{
			min = shadelight[i] * 0.8;
			shadelight[i] += scale;

			if (shadelight[i] < min)
			{
				shadelight[i] = min;
			}
		}
	}

	// Note: gl_overbrightbits are now applied in shader.

	/* ir goggles color override */
	if ((gl3_newrefdef.rdflags & RDF_IRGOGGLES) && (currententity->flags & RF_IR_VISIBLE))
	{
		shadelight[0] = 1.0;
		shadelight[1] = 0.0;
		shadelight[2] = 0.0;
	}

	// TODO: maybe we could somehow store the non-rotated normal and do the dot in shader?
	shadedots = r_avertexnormal_dots[((int)(currententity->angles[1] *
				(SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	an = currententity->angles[1] / 180 * M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize(shadevector);

	/* locate the proper data */
	c_alias_polys += paliashdr->num_tris;

	/* draw all the triangles */
	if (currententity->flags & RF_DEPTHHACK)
	{
		/* hack the depth range to prevent view model from poking into walls */
		glDepthRange(gl3depthmin, gl3depthmin + 0.3 * (gl3depthmax - gl3depthmin));
	}

	if ((currententity->flags & RF_WEAPONMODEL) && (gl_lefthand->value == 1.0F))
	{
		origProjMat = gl3state.uni3DData.transProjMat4;
		// to mirror gun so it's rendered left-handed, just invert X-axis column
		// of projection matrix
		for(int i=0; i<4; ++i)
		{
			gl3state.uni3DData.transProjMat4.Elements[0][i] = -gl3state.uni3DData.transProjMat4.Elements[0][i];
		}
		//GL3_UpdateUBO3D(); Note: GL3_RotateForEntity() will call this,no need to do it twice before drawing

		glCullFace(GL_BACK);
	}


	//glPushMatrix();
	origModelMat = gl3state.uni3DData.transModelMat4;

	e->angles[PITCH] = -e->angles[PITCH];
	GL3_RotateForEntity(e);
	e->angles[PITCH] = -e->angles[PITCH];


	/* select skin */
	if (currententity->skin)
	{
		skin = currententity->skin; /* custom player skin */
	}
	else
	{
		if (currententity->skinnum >= MAX_MD2SKINS)
		{
			skin = currentmodel->skins[0];
		}
		else
		{
			skin = currentmodel->skins[currententity->skinnum];

			if (!skin)
			{
				skin = currentmodel->skins[0];
			}
		}
	}

	if (!skin)
	{
		skin = gl3_notexture; /* fallback... */
	}

	GL3_Bind(skin->texnum);

	if (currententity->flags & RF_TRANSLUCENT)
	{
		glEnable(GL_BLEND);
	}


	if ((currententity->frame >= paliashdr->num_frames) ||
		(currententity->frame < 0))
	{
		R_Printf(PRINT_DEVELOPER, "R_DrawAliasModel %s: no such frame %d\n",
				currentmodel->name, currententity->frame);
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

	if ((currententity->oldframe >= paliashdr->num_frames) ||
		(currententity->oldframe < 0))
	{
		R_Printf(PRINT_DEVELOPER, "R_DrawAliasModel %s: no such oldframe %d\n",
				currentmodel->name, currententity->oldframe);
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

	DrawAliasFrameLerp(paliashdr, currententity->backlerp);

	//glPopMatrix();
	gl3state.uni3DData.transModelMat4 = origModelMat;
	GL3_UpdateUBO3D();

	if ((currententity->flags & RF_WEAPONMODEL) && (gl_lefthand->value == 1.0F))
	{
		gl3state.uni3DData.transProjMat4 = origProjMat;
		GL3_UpdateUBO3D();
		glCullFace(GL_FRONT);
	}

	if (currententity->flags & RF_TRANSLUCENT)
	{
		glDisable(GL_BLEND);
	}

	if (currententity->flags & RF_DEPTHHACK)
	{
		glDepthRange(gl3depthmin, gl3depthmax);
	}

	STUB_ONCE("TODO: *proper* stencil shadows!")
#if 0
	if (gl_shadows->value &&
		!(currententity->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL | RF_NOSHADOW)))
	{
		glPushMatrix();

		/* don't rotate shadows on ungodly axes */
		glTranslatef(e->origin[0], e->origin[1], e->origin[2]);
		glRotatef(e->angles[1], 0, 0, 1);

		glDisable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glColor4f(0, 0, 0, 0.5f);
		DrawAliasShadow(paliashdr, currententity->frame);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
		glPopMatrix();
	}

	glColor4f(1, 1, 1, 1);
#endif // 0
}

