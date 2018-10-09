/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// sw_alias.c: routines for setting up to draw alias models

/*
** use a real variable to control lerping
*/
#include <stdint.h>
#include "header/local.h"

#define LIGHT_MIN	5	// lowest light value we'll allow, to avoid the
				//  need for inner-loop light clamping

int				r_amodels_drawn;

affinetridesc_t	r_affinetridesc;

static vec3_t	r_plightvec;
static vec3_t	r_lerp_frontv, r_lerp_backv, r_lerp_move;

static int	r_ambientlight;
int		r_aliasblendcolor;
static float		r_shadelight;


static daliasframe_t	*r_thisframe, *r_lastframe;
static dmdl_t	*s_pmdl;

static float	aliastransform[3][4];
static float	aliasworldtransform[3][4];
static float	aliasoldworldtransform[3][4];

static float	s_ziscale;
static vec3_t	s_alias_forward, s_alias_right, s_alias_up;


#define NUMVERTEXNORMALS	162

static const float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "../constants/anorms.h"
};


static void R_AliasTransformVector(const vec3_t in, vec3_t out, const float m[3][4]);
static void R_AliasTransformFinalVerts(const entity_t *currententity, int numpoints, finalvert_t *fv, dtrivertx_t *oldv, dtrivertx_t *newv );

void R_AliasProjectAndClipTestFinalVert(finalvert_t *fv);


/*
================
R_AliasCheckBBox
================
*/

#define BBOX_TRIVIAL_ACCEPT 0
#define BBOX_MUST_CLIP_XY   1
#define BBOX_MUST_CLIP_Z    2
#define BBOX_TRIVIAL_REJECT 8

/*
** R_AliasCheckFrameBBox
**
** Checks a specific alias frame bounding box
*/
static unsigned long
R_AliasCheckFrameBBox( daliasframe_t *frame, float worldxf[3][4] )
{
	unsigned long aggregate_and_clipcode = ~0U,
		          aggregate_or_clipcode = 0;
	int           i;
	vec3_t        mins, maxs;
	vec3_t        transformed_min, transformed_max;
	qboolean      zclipped = false, zfullyclipped = true;

	/*
	** get the exact frame bounding box
	*/
	for (i=0 ; i<3 ; i++)
	{
		mins[i] = frame->translate[i];
		maxs[i] = mins[i] + frame->scale[i]*255;
	}

	/*
	** transform the min and max values into view space
	*/
	R_AliasTransformVector( mins, transformed_min, aliastransform );
	R_AliasTransformVector( maxs, transformed_max, aliastransform );

	if ( transformed_min[2] >= ALIAS_Z_CLIP_PLANE )
		zfullyclipped = false;
	if ( transformed_max[2] >= ALIAS_Z_CLIP_PLANE )
		zfullyclipped = false;

	if ( zfullyclipped )
	{
		return BBOX_TRIVIAL_REJECT;
	}
	if ( zclipped )
	{
		return ( BBOX_MUST_CLIP_XY | BBOX_MUST_CLIP_Z );
	}

	/*
	** build a transformed bounding box from the given min and max
	*/
	for ( i = 0; i < 8; i++ )
	{
		int      j;
		vec3_t   tmp, transformed;
		unsigned long clipcode = 0;

		if ( i & 1 )
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if ( i & 2 )
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if ( i & 4 )
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];

		R_AliasTransformVector( tmp, transformed, worldxf );

		for ( j = 0; j < 4; j++ )
		{
			float dp = DotProduct( transformed, view_clipplanes[j].normal );

			if ( ( dp - view_clipplanes[j].dist ) < 0.0F )
				clipcode |= 1 << j;
		}

		aggregate_and_clipcode &= clipcode;
		aggregate_or_clipcode  |= clipcode;
	}

	if ( aggregate_and_clipcode )
	{
		return BBOX_TRIVIAL_REJECT;
	}
	if ( !aggregate_or_clipcode )
	{
		return BBOX_TRIVIAL_ACCEPT;
	}

	return BBOX_MUST_CLIP_XY;
}

static int
R_AliasCheckBBox (const entity_t *currententity)
{
	unsigned long ccodes[2] = { 0, 0 };

	ccodes[0] = R_AliasCheckFrameBBox( r_thisframe, aliasworldtransform );

	/*
	** non-lerping model
	*/
	if ( currententity->backlerp == 0 )
	{
		if ( ccodes[0] == BBOX_TRIVIAL_ACCEPT )
			return BBOX_TRIVIAL_ACCEPT;
		else if ( ccodes[0] & BBOX_TRIVIAL_REJECT )
			return BBOX_TRIVIAL_REJECT;
		else
			return ( ccodes[0] & ~BBOX_TRIVIAL_REJECT );
	}

	ccodes[1] = R_AliasCheckFrameBBox( r_lastframe, aliasoldworldtransform );

	if ( ( ccodes[0] | ccodes[1] ) == BBOX_TRIVIAL_ACCEPT )
		return BBOX_TRIVIAL_ACCEPT;
	else if ( ( ccodes[0] & ccodes[1] ) & BBOX_TRIVIAL_REJECT )
		return BBOX_TRIVIAL_REJECT;
	else
		return ( ccodes[0] | ccodes[1] ) & ~BBOX_TRIVIAL_REJECT;
}


/*
================
R_AliasTransformVector
================
*/
static void
R_AliasTransformVector(const vec3_t in, vec3_t out, const float xf[3][4] )
{
	out[0] = DotProduct(in, xf[0]) + xf[0][3];
	out[1] = DotProduct(in, xf[1]) + xf[1][3];
	out[2] = DotProduct(in, xf[2]) + xf[2][3];
}


/*
================
R_AliasPreparePoints

General clipped case
================
*/

static void
R_AliasPreparePoints (const entity_t *currententity, finalvert_t *verts, const finalvert_t *verts_max)
{
	int		i;
	dstvert_t	*pstverts;
	dtriangle_t	*ptri;
	finalvert_t	*pfv[3];

	if ((verts + s_pmdl->num_xyz) >= verts_max)
	{
		r_outofverts += s_pmdl->num_xyz - (verts_max - verts);
		return;
	}

	R_AliasTransformFinalVerts(currententity,
				   s_pmdl->num_xyz,
				   verts,	// destination for transformed verts
				   r_lastframe->verts,	// verts from the last frame
				   r_thisframe->verts	// verts from this frame
				);

	// clip and draw all triangles
	//
	pstverts = (dstvert_t *)((byte *)s_pmdl + s_pmdl->ofs_st);
	ptri = (dtriangle_t *)((byte *)s_pmdl + s_pmdl->ofs_tris);

	if ( ( currententity->flags & RF_WEAPONMODEL ) && ( r_lefthand->value == 1.0F ) )
	{
		for (i=0 ; i<s_pmdl->num_tris ; i++, ptri++)
		{
			pfv[0] = &verts[ptri->index_xyz[0]];
			pfv[1] = &verts[ptri->index_xyz[1]];
			pfv[2] = &verts[ptri->index_xyz[2]];

			if ( pfv[0]->flags & pfv[1]->flags & pfv[2]->flags )
				continue;	// completely clipped

			// insert s/t coordinates
			pfv[0]->s = pstverts[ptri->index_st[0]].s << SHIFT16XYZ;
			pfv[0]->t = pstverts[ptri->index_st[0]].t << SHIFT16XYZ;

			pfv[1]->s = pstverts[ptri->index_st[1]].s << SHIFT16XYZ;
			pfv[1]->t = pstverts[ptri->index_st[1]].t << SHIFT16XYZ;

			pfv[2]->s = pstverts[ptri->index_st[2]].s << SHIFT16XYZ;
			pfv[2]->t = pstverts[ptri->index_st[2]].t << SHIFT16XYZ;

			if ( ! (pfv[0]->flags | pfv[1]->flags | pfv[2]->flags) )
			{
				// totally unclipped
				R_DrawTriangle(currententity, pfv[2], pfv[1], pfv[0]);
			}
			else
			{
				R_AliasClipTriangle(currententity, pfv[2], pfv[1], pfv[0]);
			}
		}
	}
	else
	{
		for (i=0 ; i<s_pmdl->num_tris ; i++, ptri++)
		{
			pfv[0] = &verts[ptri->index_xyz[0]];
			pfv[1] = &verts[ptri->index_xyz[1]];
			pfv[2] = &verts[ptri->index_xyz[2]];

			if ( pfv[0]->flags & pfv[1]->flags & pfv[2]->flags )
				continue;	// completely clipped

			// insert s/t coordinates
			pfv[0]->s = pstverts[ptri->index_st[0]].s << SHIFT16XYZ;
			pfv[0]->t = pstverts[ptri->index_st[0]].t << SHIFT16XYZ;

			pfv[1]->s = pstverts[ptri->index_st[1]].s << SHIFT16XYZ;
			pfv[1]->t = pstverts[ptri->index_st[1]].t << SHIFT16XYZ;

			pfv[2]->s = pstverts[ptri->index_st[2]].s << SHIFT16XYZ;
			pfv[2]->t = pstverts[ptri->index_st[2]].t << SHIFT16XYZ;

			if ( ! (pfv[0]->flags | pfv[1]->flags | pfv[2]->flags) )
			{
				// totally unclipped
				R_DrawTriangle(currententity, pfv[0], pfv[1], pfv[2]);
			}
			else
			{	// partially clipped
				R_AliasClipTriangle(currententity, pfv[0], pfv[1], pfv[2]);
			}
		}
	}
}


/*
================
R_AliasSetUpTransform
================
*/
static void
R_AliasSetUpTransform(const entity_t *currententity)
{
	int				i;
	static float	viewmatrix[3][4];

	// TODO: should really be stored with the entity instead of being reconstructed
	// TODO: should use a look-up table
	// TODO: could cache lazily, stored in the entity
	//
	// AngleVectors never change angles, we can convert from const
	AngleVectors((float *)currententity->angles, s_alias_forward, s_alias_right, s_alias_up );

	// TODO: can do this with simple matrix rearrangement
	memset( aliasworldtransform, 0, sizeof( aliasworldtransform ) );
	memset( aliasoldworldtransform, 0, sizeof( aliasworldtransform ) );

	for (i=0 ; i<3 ; i++)
	{
		aliasoldworldtransform[i][0] = aliasworldtransform[i][0] =  s_alias_forward[i];
		aliasoldworldtransform[i][0] = aliasworldtransform[i][1] = -s_alias_right[i];
		aliasoldworldtransform[i][0] = aliasworldtransform[i][2] =  s_alias_up[i];
	}

	aliasworldtransform[0][3] = currententity->origin[0]-r_origin[0];
	aliasworldtransform[1][3] = currententity->origin[1]-r_origin[1];
	aliasworldtransform[2][3] = currententity->origin[2]-r_origin[2];

	aliasoldworldtransform[0][3] = currententity->oldorigin[0]-r_origin[0];
	aliasoldworldtransform[1][3] = currententity->oldorigin[1]-r_origin[1];
	aliasoldworldtransform[2][3] = currententity->oldorigin[2]-r_origin[2];

	// FIXME: can do more efficiently than full concatenation
	// memcpy( rotationmatrix, t2matrix, sizeof( rotationmatrix ) );

	// R_ConcatTransforms (t2matrix, tmatrix, rotationmatrix);

	// TODO: should be global, set when vright, etc., set
	VectorCopy (vright, viewmatrix[0]);
	VectorCopy (vup, viewmatrix[1]);
	VectorInverse (viewmatrix[1]);
	VectorCopy (vpn, viewmatrix[2]);

	viewmatrix[0][3] = 0;
	viewmatrix[1][3] = 0;
	viewmatrix[2][3] = 0;

	// memcpy( aliasworldtransform, rotationmatrix, sizeof( aliastransform ) );

	R_ConcatTransforms (viewmatrix, aliasworldtransform, aliastransform);

	aliasworldtransform[0][3] = currententity->origin[0];
	aliasworldtransform[1][3] = currententity->origin[1];
	aliasworldtransform[2][3] = currententity->origin[2];

	aliasoldworldtransform[0][3] = currententity->oldorigin[0];
	aliasoldworldtransform[1][3] = currententity->oldorigin[1];
	aliasoldworldtransform[2][3] = currententity->oldorigin[2];
}


/*
================
R_AliasTransformFinalVerts
================
*/
static void
R_AliasTransformFinalVerts(const entity_t *currententity, int numpoints, finalvert_t *fv, dtrivertx_t *oldv, dtrivertx_t *newv )
{
	int i;

	for ( i = 0; i < numpoints; i++, fv++, oldv++, newv++ )
	{
		int		temp;
		float	lightcos;
		const float	*plightnormal;
		vec3_t  lerped_vert;

		lerped_vert[0] = r_lerp_move[0] + oldv->v[0]*r_lerp_backv[0] + newv->v[0]*r_lerp_frontv[0];
		lerped_vert[1] = r_lerp_move[1] + oldv->v[1]*r_lerp_backv[1] + newv->v[1]*r_lerp_frontv[1];
		lerped_vert[2] = r_lerp_move[2] + oldv->v[2]*r_lerp_backv[2] + newv->v[2]*r_lerp_frontv[2];

		plightnormal = r_avertexnormals[newv->lightnormalindex];

		// PMM - added double damage shell
		if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
		{
			lerped_vert[0] += plightnormal[0] * POWERSUIT_SCALE;
			lerped_vert[1] += plightnormal[1] * POWERSUIT_SCALE;
			lerped_vert[2] += plightnormal[2] * POWERSUIT_SCALE;
		}

		fv->xyz[0] = DotProduct(lerped_vert, aliastransform[0]) + aliastransform[0][3];
		fv->xyz[1] = DotProduct(lerped_vert, aliastransform[1]) + aliastransform[1][3];
		fv->xyz[2] = DotProduct(lerped_vert, aliastransform[2]) + aliastransform[2][3];

		fv->flags = 0;

		// lighting
		lightcos = DotProduct (plightnormal, r_plightvec);
		temp = r_ambientlight;

		if (lightcos < 0)
		{
			temp += (int)(r_shadelight * lightcos);

			// clamp; because we limited the minimum ambient and shading light, we
			// don't have to clamp low light, just bright
			if (temp < 0)
				temp = 0;
		}

		fv->l = temp;

		if ( fv->xyz[2] < ALIAS_Z_CLIP_PLANE )
		{
			fv->flags |= ALIAS_Z_CLIP;
		}
		else
		{
			R_AliasProjectAndClipTestFinalVert( fv );
		}
	}
}

/*
================
R_AliasProjectAndClipTestFinalVert
================
*/
void
R_AliasProjectAndClipTestFinalVert( finalvert_t *fv )
{
	float	zi;
	float	x, y, z;

	// project points
	x = fv->xyz[0];
	y = fv->xyz[1];
	z = fv->xyz[2];
	zi = 1.0 / z;

	fv->zi = zi * s_ziscale;

	fv->u = (x * aliasxscale * zi) + aliasxcenter;
	fv->v = (y * aliasyscale * zi) + aliasycenter;

	if (fv->u < r_refdef.aliasvrect.x)
		fv->flags |= ALIAS_LEFT_CLIP;
	if (fv->v < r_refdef.aliasvrect.y)
		fv->flags |= ALIAS_TOP_CLIP;
	if (fv->u > r_refdef.aliasvrectright)
		fv->flags |= ALIAS_RIGHT_CLIP;
	if (fv->v > r_refdef.aliasvrectbottom)
		fv->flags |= ALIAS_BOTTOM_CLIP;
}

/*
===============
R_AliasSetupSkin
===============
*/
static qboolean
R_AliasSetupSkin(const entity_t *currententity, const model_t *currentmodel)
{
	image_t *pskindesc;

	if (currententity->skin)
		pskindesc = currententity->skin;
	else
	{
		int skinnum;

		skinnum = currententity->skinnum;
		if ((skinnum >= s_pmdl->num_skins) || (skinnum < 0))
		{
			R_Printf(PRINT_ALL, "R_AliasSetupSkin %s: no such skin # %d\n",
				currentmodel->name, skinnum);
			skinnum = 0;
		}

		pskindesc = currentmodel->skins[skinnum];
	}

	if ( !pskindesc )
		return false;

	r_affinetridesc.pskin = pskindesc->pixels[0];
	r_affinetridesc.skinwidth = pskindesc->width;
	r_affinetridesc.skinheight = pskindesc->height;

	R_PolysetUpdateTables (); // FIXME: precalc edge lookups

	return true;
}


/*
================
R_AliasSetupLighting

  FIXME: put lighting into tables
================
*/
static void
R_AliasSetupLighting(entity_t *currententity)
{
	alight_t lighting;
	float lightvec[3] = {-1, 0, 0};
	vec3_t light;
	int i, j;

	// all components of light should be identical in software
	if ( currententity->flags & RF_FULLBRIGHT )
	{
		for (i=0 ; i<3 ; i++)
			light[i] = 1.0;
	}
	else
	{
		R_LightPoint (currententity, currententity->origin, light);
	}

	// save off light value for server to look at (BIG HACK!)
	if ( currententity->flags & RF_WEAPONMODEL )
		r_lightlevel->value = 150.0 * light[0];


	if ( currententity->flags & RF_MINLIGHT )
	{
		for (i=0 ; i<3 ; i++)
			if (light[i] < 0.1)
				light[i] = 0.1;
	}

	if ( currententity->flags & RF_GLOW )
	{	// bonus items will pulse with time
		float	scale;

		scale = 0.1 * sin(r_newrefdef.time*7);
		for (i=0 ; i<3 ; i++)
		{
			float min;

			min = light[i] * 0.8;
			light[i] += scale;
			if (light[i] < min)
				light[i] = min;
		}
	}

	j = (light[0] + light[1] + light[2])*0.3333*255;

	lighting.ambientlight = j;
	lighting.shadelight = j;

	lighting.plightvec = lightvec;

	// clamp lighting so it doesn't overbright as much
	if (lighting.ambientlight > 128)
		lighting.ambientlight = 128;
	if (lighting.ambientlight + lighting.shadelight > 192)
		lighting.shadelight = 192 - lighting.ambientlight;

	// guarantee that no vertex will ever be lit below LIGHT_MIN, so we don't have
	// to clamp off the bottom
	r_ambientlight = lighting.ambientlight;

	if (r_ambientlight < LIGHT_MIN)
		r_ambientlight = LIGHT_MIN;

	r_ambientlight = (255 - r_ambientlight) << VID_CBITS;

	if (r_ambientlight < LIGHT_MIN)
		r_ambientlight = LIGHT_MIN;

	r_shadelight = lighting.shadelight;

	if (r_shadelight < 0)
		r_shadelight = 0;

	r_shadelight *= VID_GRADES;

	// rotate the lighting vector into the model's frame of reference
	r_plightvec[0] =  DotProduct( lighting.plightvec, s_alias_forward );
	r_plightvec[1] = -DotProduct( lighting.plightvec, s_alias_right );
	r_plightvec[2] =  DotProduct( lighting.plightvec, s_alias_up );
}


/*
=================
R_AliasSetupFrames

=================
*/
static void
R_AliasSetupFrames(const entity_t *currententity, const model_t *currentmodel, dmdl_t *pmdl)
{
	int thisframe = currententity->frame;
	int lastframe = currententity->oldframe;

	if ( ( thisframe >= pmdl->num_frames ) || ( thisframe < 0 ) )
	{
		R_Printf(PRINT_ALL, "R_AliasSetupFrames %s: no such thisframe %d\n",
			currentmodel->name, thisframe);
		thisframe = 0;
	}
	if ( ( lastframe >= pmdl->num_frames ) || ( lastframe < 0 ) )
	{
		R_Printf(PRINT_ALL, "R_AliasSetupFrames %s: no such lastframe %d\n",
			currentmodel->name, lastframe);
		lastframe = 0;
	}

	r_thisframe = (daliasframe_t *)((byte *)pmdl + pmdl->ofs_frames
		+ thisframe * pmdl->framesize);

	r_lastframe = (daliasframe_t *)((byte *)pmdl + pmdl->ofs_frames
		+ lastframe * pmdl->framesize);
}

/*
** R_AliasSetUpLerpData
**
** Precomputes lerp coefficients used for the whole frame.
*/
static void
R_AliasSetUpLerpData(entity_t *currententity, dmdl_t *pmdl, float backlerp)
{
	float	frontlerp;
	vec3_t	translation, vectors[3];
	int		i;

	frontlerp = 1.0F - backlerp;

	/*
	** convert entity's angles into discrete vectors for R, U, and F
	*/
	AngleVectors (currententity->angles, vectors[0], vectors[1], vectors[2]);

	/*
	** translation is the vector from last position to this position
	*/
	VectorSubtract (currententity->oldorigin, currententity->origin, translation);

	/*
	** move should be the delta back to the previous frame * backlerp
	*/
	r_lerp_move[0] =  DotProduct(translation, vectors[0]);	// forward
	r_lerp_move[1] = -DotProduct(translation, vectors[1]);	// left
	r_lerp_move[2] =  DotProduct(translation, vectors[2]);	// up

	VectorAdd( r_lerp_move, r_lastframe->translate, r_lerp_move );

	for (i=0 ; i<3 ; i++)
	{
		r_lerp_move[i] = backlerp*r_lerp_move[i] + frontlerp * r_thisframe->translate[i];
	}

	for (i=0 ; i<3 ; i++)
	{
		r_lerp_frontv[i] = frontlerp * r_thisframe->scale[i];
		r_lerp_backv[i]  = backlerp  * r_lastframe->scale[i];
	}
}

finalvert_t *finalverts = NULL, *finalverts_max = NULL;

extern void (*d_pdrawspans)(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpans8_Opaque(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpans8_33(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpans8_66(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpansConstant8_33(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpansConstant8_66(const entity_t *currententity, spanpackage_t *pspanpackage);

/*
================
R_AliasDrawModel
================
*/
void
R_AliasDrawModel(entity_t *currententity, const model_t *currentmodel)
{
	s_pmdl = (dmdl_t *)currentmodel->extradata;

	if ( r_lerpmodels->value == 0 )
		currententity->backlerp = 0;

	float oldAliasxscale = aliasxscale;
	float oldAliasyscale = aliasyscale;

	if ( currententity->flags & RF_WEAPONMODEL )
	{
		if ( r_lefthand->value == 2.0F )
		{
			return;
		}

		float fov = 2.0*tan(r_gunfov->value*((4.0/3.0)*M_PI/360.0));
		aliasxscale = ((float)r_refdef.vrect.width / fov) * r_aliasuvscale;
		aliasyscale = aliasxscale;

		if ( r_lefthand->value == 1.0F )
			aliasxscale = -aliasxscale;
	}

	/*
	** we have to set our frame pointers and transformations before
	** doing any real work
	*/
	R_AliasSetupFrames(currententity, currentmodel, s_pmdl);
	R_AliasSetUpTransform(currententity);

	// see if the bounding box lets us trivially reject, also sets
	// trivial accept status
	if ( R_AliasCheckBBox(currententity) == BBOX_TRIVIAL_REJECT )
	{
		if ( currententity->flags & RF_WEAPONMODEL )
		{
			aliasxscale = oldAliasxscale;
			aliasyscale = oldAliasyscale;
		}
		return;
	}

	// set up the skin and verify it exists
	if ( !R_AliasSetupSkin(currententity, currentmodel) )
	{
		R_Printf( PRINT_ALL, "R_AliasDrawModel %s: NULL skin found\n",
			currentmodel->name);
		aliasxscale = oldAliasxscale;
		aliasyscale = oldAliasyscale;
		return;
	}

	r_amodels_drawn++;
	R_AliasSetupLighting(currententity);

	/*
	** select the proper span routine based on translucency
	*/
	// PMM - added double damage shell
	// PMM - reordered to handle blending
	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
	{
		int		color;

		// PMM - added double
		color = currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM);
		// PMM - reordered, new shells after old shells (so they get overriden)

		if ( color == RF_SHELL_RED )
			r_aliasblendcolor = SHELL_RED_COLOR;
		else if ( color == RF_SHELL_GREEN )
			r_aliasblendcolor = SHELL_GREEN_COLOR;
		else if ( color == RF_SHELL_BLUE )
			r_aliasblendcolor = SHELL_BLUE_COLOR;
		else if ( color == (RF_SHELL_RED | RF_SHELL_GREEN) )
			r_aliasblendcolor = SHELL_RG_COLOR;
		else if ( color == (RF_SHELL_RED | RF_SHELL_BLUE) )
			r_aliasblendcolor = SHELL_RB_COLOR;
		else if ( color == (RF_SHELL_BLUE | RF_SHELL_GREEN) )
			r_aliasblendcolor = SHELL_BG_COLOR;
		// PMM - added this .. it's yellowish
		else if ( color == (RF_SHELL_DOUBLE) )
			r_aliasblendcolor = SHELL_DOUBLE_COLOR;
		else if ( color == (RF_SHELL_HALF_DAM) )
			r_aliasblendcolor = SHELL_HALF_DAM_COLOR;
		// pmm
		else
			r_aliasblendcolor = SHELL_WHITE_COLOR;
		/*
		if ( color & RF_SHELL_RED )
		{
			if ( ( color & RF_SHELL_BLUE) && ( color & RF_SHELL_GREEN) )
				r_aliasblendcolor = SHELL_WHITE_COLOR;
			else if ( color & (RF_SHELL_BLUE | RF_SHELL_DOUBLE))
				r_aliasblendcolor = SHELL_RB_COLOR;
			else
				r_aliasblendcolor = SHELL_RED_COLOR;
		}
		else if ( color & RF_SHELL_BLUE)
		{
			if ( color & RF_SHELL_DOUBLE )
				r_aliasblendcolor = SHELL_CYAN_COLOR;
			else
				r_aliasblendcolor = SHELL_BLUE_COLOR;
		}
		else if ( color & (RF_SHELL_DOUBLE) )
			r_aliasblendcolor = SHELL_DOUBLE_COLOR;
		else if ( color & (RF_SHELL_HALF_DAM) )
			r_aliasblendcolor = SHELL_HALF_DAM_COLOR;
		else if ( color & RF_SHELL_GREEN )
			r_aliasblendcolor = SHELL_GREEN_COLOR;
		else
			r_aliasblendcolor = SHELL_WHITE_COLOR;
		*/

		if ( currententity->alpha > 0.33 )
			d_pdrawspans = R_PolysetDrawSpansConstant8_66;
		else
			d_pdrawspans = R_PolysetDrawSpansConstant8_33;
	}
	else if ( currententity->flags & RF_TRANSLUCENT )
	{
		if ( currententity->alpha > 0.66 )
			d_pdrawspans = R_PolysetDrawSpans8_Opaque;
		else if ( currententity->alpha > 0.33 )
			d_pdrawspans = R_PolysetDrawSpans8_66;
		else
			d_pdrawspans = R_PolysetDrawSpans8_33;
	}
	else
	{
		d_pdrawspans = R_PolysetDrawSpans8_Opaque;
	}

	/*
	** compute this_frame and old_frame addresses
	*/
	R_AliasSetUpLerpData(currententity, s_pmdl, currententity->backlerp);

	if (currententity->flags & RF_DEPTHHACK)
		s_ziscale = (float)0x8000 * (float)SHIFT16XYZ_MULT * 3.0;
	else
		s_ziscale = (float)0x8000 * (float)SHIFT16XYZ_MULT;

	R_AliasPreparePoints(currententity, finalverts, finalverts_max);

	if ( currententity->flags & RF_WEAPONMODEL )
	{
		aliasxscale = oldAliasxscale;
		aliasyscale = oldAliasyscale;
	}
}
