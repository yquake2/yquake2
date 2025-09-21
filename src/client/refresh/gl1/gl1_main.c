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
 * Refresher setup and main part of the frame generation
 *
 * =======================================================================
 */

#include "header/local.h"

#define NUM_BEAM_SEGS 6

model_t *r_worldmodel;

float gldepthmin, gldepthmax;

glconfig_t gl_config;
glstate_t gl_state;

image_t *r_notexture; /* use for bad textures */
image_t *r_particletexture; /* little dot for particles */

cplane_t frustum[4];

int r_visframecount; /* bumped when going to a new PVS */
int r_framecount; /* used for dlight push checking */

int c_brush_polys, c_alias_polys;

float v_blend[4]; /* final blending color */

void R_Strings(void);

/* view origin */
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

float r_world_matrix[16];
float r_base_world_matrix[16];

/* screen size info */
int r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;
unsigned r_rawpalette[256];

cvar_t *r_norefresh;
cvar_t *r_drawentities;
cvar_t *r_drawworld;
cvar_t *r_speeds;
cvar_t *r_fullbright;
cvar_t *r_novis;
cvar_t *r_lerpmodels;
cvar_t *gl_lefthand;
cvar_t *r_gunfov;
cvar_t *r_farsee;
cvar_t *r_validation;

cvar_t *r_lightlevel;
cvar_t *gl1_overbrightbits;

cvar_t *gl1_particle_min_size;
cvar_t *gl1_particle_max_size;
cvar_t *gl1_particle_size;
cvar_t *gl1_particle_att_a;
cvar_t *gl1_particle_att_b;
cvar_t *gl1_particle_att_c;
cvar_t *gl1_particle_square;

cvar_t *gl1_palettedtexture;
cvar_t *gl1_pointparameters;
cvar_t *gl1_multitexture;
cvar_t *gl1_lightmapcopies;
cvar_t *gl1_discardfb;

cvar_t *gl_drawbuffer;
cvar_t *gl_lightmap;
cvar_t *gl_shadows;
cvar_t *gl1_stencilshadow;
cvar_t *r_mode;
cvar_t *r_fixsurfsky;
cvar_t *gl1_minlight;

cvar_t *r_customwidth;
cvar_t *r_customheight;

cvar_t *r_retexturing;
cvar_t *r_scale8bittextures;

cvar_t *gl_nolerp_list;
cvar_t *r_lerp_list;
cvar_t *r_2D_unfiltered;
cvar_t *r_videos_unfiltered;

cvar_t *gl1_dynamic;
cvar_t *r_modulate;
cvar_t *gl_nobind;
cvar_t *gl1_round_down;
cvar_t *gl1_picmip;
cvar_t *gl_showtris;
cvar_t *gl_showbbox;
cvar_t *gl1_ztrick;
cvar_t *gl_zfix;
cvar_t *gl_finish;
cvar_t *r_clear;
cvar_t *r_cull;
cvar_t *gl_polyblend;
cvar_t *gl1_flashblend;
cvar_t *gl1_saturatelighting;
cvar_t *r_vsync;
cvar_t *gl_texturemode;
cvar_t *gl1_texturealphamode;
cvar_t *gl1_texturesolidmode;
cvar_t *gl_anisotropic;
cvar_t *r_lockpvs;
cvar_t *gl_msaa_samples;

cvar_t *vid_fullscreen;
cvar_t *vid_gamma;

cvar_t *gl1_stereo;
cvar_t *gl1_stereo_separation;
cvar_t *gl1_stereo_anaglyph_colors;
cvar_t *gl1_stereo_convergence;

static cvar_t *gl_znear;
static cvar_t *gl1_waterwarp;

refimport_t ri;

void LM_FreeLightmapBuffers(void);
void Scrap_Init(void);

extern void R_SetDefaultState(void);
extern void R_ResetGLBuffer(void);

void
R_RotateForEntity(entity_t *e)
{
	glTranslatef(e->origin[0], e->origin[1], e->origin[2]);

	glRotatef(e->angles[1], 0, 0, 1);
	glRotatef(-e->angles[0], 0, 1, 0);
	glRotatef(-e->angles[2], 1, 0, 0);
}

void
R_DrawSpriteModel(entity_t *currententity, const model_t *currentmodel)
{
	float alpha = 1.0F;
	vec3_t point[4];
	dsprframe_t *frame;
	float *up, *right;
	dsprite_t *psprite;
	image_t *skin;

	R_EnableMultitexture(false);
	/* don't even bother culling, because it's just
	   a single polygon without a surface cache */
	psprite = (dsprite_t *)currentmodel->extradata;

	currententity->frame %= psprite->numframes;
	frame = &psprite->frames[currententity->frame];

	/* normal sprite */
	up = vup;
	right = vright;

	if (currententity->flags & RF_TRANSLUCENT)
	{
		alpha = currententity->alpha;
	}

	if (alpha != 1.0F)
	{
		glEnable(GL_BLEND);
	}

	glColor4f(1, 1, 1, alpha);

	skin = currentmodel->skins[currententity->frame];
	if (!skin)
	{
		skin = r_notexture; /* fallback... */
	}

	R_Bind(skin->texnum);

	R_TexEnv(GL_MODULATE);

	if (alpha == 1.0)
	{
		glEnable(GL_ALPHA_TEST);
	}
	else
	{
		glDisable(GL_ALPHA_TEST);
	}

	GLfloat tex[] = {
		0, 1,
		0, 0,
		1, 0,
		1, 1
	};

	VectorMA( currententity->origin, -frame->origin_y, up, point[0] );
	VectorMA( point[0], -frame->origin_x, right, point[0] );

	VectorMA( currententity->origin, frame->height - frame->origin_y, up, point[1] );
	VectorMA( point[1], -frame->origin_x, right, point[1] );

	VectorMA( currententity->origin, frame->height - frame->origin_y, up, point[2] );
	VectorMA( point[2], frame->width - frame->origin_x, right, point[2] );

	VectorMA( currententity->origin, -frame->origin_y, up, point[3] );
	VectorMA( point[3], frame->width - frame->origin_x, right, point[3] );

	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );

	glVertexPointer( 3, GL_FLOAT, 0, point );
	glTexCoordPointer( 2, GL_FLOAT, 0, tex );
	glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );

	glDisable(GL_ALPHA_TEST);
	R_TexEnv(GL_REPLACE);

	if (alpha != 1.0F)
	{
		glDisable(GL_BLEND);
	}

	glColor4f(1, 1, 1, 1);
}

void
R_DrawNullModel(entity_t *currententity)
{
	vec3_t shadelight;

	if (currententity->flags & RF_FULLBRIGHT)
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0F;
	}
	else
	{
		R_LightPoint(currententity, currententity->origin, shadelight);
	}

	R_EnableMultitexture(false);
	glPushMatrix();
	R_RotateForEntity(currententity);

	glDisable(GL_TEXTURE_2D);
	glColor4f( shadelight[0], shadelight[1], shadelight[2], 1 );

    GLfloat vtxA[] = {
        0, 0, -16,
        16 * cos( 0 * M_PI / 2 ), 16 * sin( 0 * M_PI / 2 ), 0,
        16 * cos( 1 * M_PI / 2 ), 16 * sin( 1 * M_PI / 2 ), 0,
        16 * cos( 2 * M_PI / 2 ), 16 * sin( 2 * M_PI / 2 ), 0,
        16 * cos( 3 * M_PI / 2 ), 16 * sin( 3 * M_PI / 2 ), 0,
        16 * cos( 4 * M_PI / 2 ), 16 * sin( 4 * M_PI / 2 ), 0
    };

    glEnableClientState( GL_VERTEX_ARRAY );

    glVertexPointer( 3, GL_FLOAT, 0, vtxA );
    glDrawArrays( GL_TRIANGLE_FAN, 0, 6 );

    glDisableClientState( GL_VERTEX_ARRAY );

	GLfloat vtxB[] = {
		0, 0, 16,
		16 * cos( 4 * M_PI / 2 ), 16 * sin( 4 * M_PI / 2 ), 0,
		16 * cos( 3 * M_PI / 2 ), 16 * sin( 3 * M_PI / 2 ), 0,
		16 * cos( 2 * M_PI / 2 ), 16 * sin( 2 * M_PI / 2 ), 0,
		16 * cos( 1 * M_PI / 2 ), 16 * sin( 1 * M_PI / 2 ), 0,
		16 * cos( 0 * M_PI / 2 ), 16 * sin( 0 * M_PI / 2 ), 0
	};

	glEnableClientState( GL_VERTEX_ARRAY );

	glVertexPointer( 3, GL_FLOAT, 0, vtxB );
	glDrawArrays( GL_TRIANGLE_FAN, 0, 6 );

	glDisableClientState( GL_VERTEX_ARRAY );

	glColor4f(1, 1, 1, 1);
	glPopMatrix();
	glEnable(GL_TEXTURE_2D);
}

void
R_DrawEntitiesOnList(void)
{
	int i;

	if (!r_drawentities->value)
	{
		return;
	}

	/* draw non-transparent first */
	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		entity_t *currententity = &r_newrefdef.entities[i];

		if (currententity->flags & RF_TRANSLUCENT)
		{
			continue; /* solid */
		}

		if (currententity->flags & RF_BEAM)
		{
			R_DrawBeam(currententity);
		}
		else
		{
			const model_t *currentmodel = currententity->model;

			if (!currentmodel)
			{
				R_DrawNullModel(currententity);
				continue;
			}

			switch (currentmodel->type)
			{
				case mod_alias:
					R_DrawAliasModel(currententity, currentmodel);
					break;
				case mod_brush:
					R_DrawBrushModel(currententity, currentmodel);
					break;
				case mod_sprite:
					R_DrawSpriteModel(currententity, currentmodel);
					break;
				default:
					Com_Error(ERR_DROP, "Bad modeltype");
					break;
			}
		}
	}

	/* draw transparent entities
	   we could sort these if it ever
	   becomes a problem... */
	glDepthMask(GL_FALSE);

	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		entity_t *currententity = &r_newrefdef.entities[i];

		if (!(currententity->flags & RF_TRANSLUCENT))
		{
			continue; /* solid */
		}

		if (currententity->flags & RF_BEAM)
		{
			R_DrawBeam(currententity);
		}
		else
		{
			const model_t *currentmodel = currententity->model;

			if (!currentmodel)
			{
				R_DrawNullModel(currententity);
				continue;
			}

			switch (currentmodel->type)
			{
				case mod_alias:
					R_DrawAliasModel(currententity, currentmodel);
					break;
				case mod_brush:
					R_DrawBrushModel(currententity, currentmodel);
					break;
				case mod_sprite:
					R_DrawSpriteModel(currententity, currentmodel);
					break;
				default:
					Com_Error(ERR_DROP, "Bad modeltype");
					break;
			}
		}
	}

	glDepthMask(GL_TRUE); /* back to writing */
	R_EnableMultitexture(false);
}

void
R_DrawParticles2(int num_particles, const particle_t particles[],
		const unsigned *colortable)
{
	const particle_t *p;
	int i;
	vec3_t up, right;
	float scale;
	YQ2_ALIGNAS_TYPE(unsigned) byte color[4];

	YQ2_VLA(GLfloat, vtx, 3 * num_particles * 3);
	YQ2_VLA(GLfloat, tex, 2 * num_particles * 3);
	YQ2_VLA(GLubyte, clr, 4 * num_particles * 3);

	unsigned int index_vtx = 0;
	unsigned int index_tex = 0;
	unsigned int index_clr = 0;
	unsigned int j;

	R_Bind(r_particletexture->texnum);
	glDepthMask(GL_FALSE); /* no z buffering */
	glEnable(GL_BLEND);
	R_TexEnv(GL_MODULATE);

	VectorScale( vup, 1.5, up );
	VectorScale( vright, 1.5, right );

	for ( p = particles, i = 0; i < num_particles; i++, p++ )
	{
		/* hack a scale up to keep particles from disapearing */
		scale = ( p->origin [ 0 ] - r_origin [ 0 ] ) * vpn [ 0 ] +
			( p->origin [ 1 ] - r_origin [ 1 ] ) * vpn [ 1 ] +
			( p->origin [ 2 ] - r_origin [ 2 ] ) * vpn [ 2 ];

		if ( scale < 20 )
		{
			scale = 1;
		}
		else
		{
			scale = 1 + scale * 0.004;
		}

		*(unsigned *) color = colortable [ p->color ];

		for (j=0; j<3; j++) // Copy the color for each point
		{
			clr[index_clr++] = gammatable[color[0]];
			clr[index_clr++] = gammatable[color[1]];
			clr[index_clr++] = gammatable[color[2]];
			clr[index_clr++] = p->alpha * 255;
		}

		// point 0
		tex[index_tex++] = 0.0625f;
		tex[index_tex++] = 0.0625f;

		vtx[index_vtx++] = p->origin[0];
		vtx[index_vtx++] = p->origin[1];
		vtx[index_vtx++] = p->origin[2];

		// point 1
		tex[index_tex++] = 1.0625f;
		tex[index_tex++] = 0.0625f;

		vtx[index_vtx++] = p->origin [ 0 ] + up [ 0 ] * scale;
		vtx[index_vtx++] = p->origin [ 1 ] + up [ 1 ] * scale;
		vtx[index_vtx++] = p->origin [ 2 ] + up [ 2 ] * scale;

		// point 2
		tex[index_tex++] = 0.0625f;
		tex[index_tex++] = 1.0625f;

		vtx[index_vtx++] = p->origin [ 0 ] + right [ 0 ] * scale;
		vtx[index_vtx++] = p->origin [ 1 ] + right [ 1 ] * scale;
		vtx[index_vtx++] = p->origin [ 2 ] + right [ 2 ] * scale;
	}

	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glEnableClientState( GL_COLOR_ARRAY );

	glVertexPointer( 3, GL_FLOAT, 0, vtx );
	glTexCoordPointer( 2, GL_FLOAT, 0, tex );
	glColorPointer( 4, GL_UNSIGNED_BYTE, 0, clr );
	glDrawArrays( GL_TRIANGLES, 0, num_particles*3 );

	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	glDisableClientState( GL_COLOR_ARRAY );

	glDisable(GL_BLEND);
	glColor4f(1, 1, 1, 1);
	glDepthMask(GL_TRUE); /* back to normal Z buffering */
	R_TexEnv(GL_REPLACE);

	YQ2_VLAFREE(vtx);
	YQ2_VLAFREE(tex);
	YQ2_VLAFREE(clr);
}

void
R_DrawParticles(void)
{
	qboolean stereo_split_tb = ((gl_state.stereo_mode == STEREO_SPLIT_VERTICAL) && gl_state.camera_separation);
	qboolean stereo_split_lr = ((gl_state.stereo_mode == STEREO_SPLIT_HORIZONTAL) && gl_state.camera_separation);

	if (r_newrefdef.num_particles <= 0) /* avoiding VLA with no size and vertexes built on it */
	{
		return;
	}

	if (gl_config.pointparameters && !(stereo_split_tb || stereo_split_lr))
	{
		int i;
		YQ2_ALIGNAS_TYPE(unsigned) byte color[4];
		const particle_t *p;

		YQ2_VLA(GLfloat, vtx, 3 * r_newrefdef.num_particles);
		YQ2_VLA(GLubyte, clr, 4 * r_newrefdef.num_particles);

		unsigned int index_vtx = 0;
		unsigned int index_clr = 0;

		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);

		// assume the particle size looks good with window height 480px and scale according to real resolution
		glPointSize(gl1_particle_size->value * (float)r_newrefdef.height/480.0f);

		for ( i = 0, p = r_newrefdef.particles; i < r_newrefdef.num_particles; i++, p++ )
		{
			*(int *) color = d_8to24table [ p->color & 0xFF ];
			clr[index_clr++] = gammatable[color[0]];
			clr[index_clr++] = gammatable[color[1]];
			clr[index_clr++] = gammatable[color[2]];
			clr[index_clr++] = p->alpha * 255;

			vtx[index_vtx++] = p->origin[0];
			vtx[index_vtx++] = p->origin[1];
			vtx[index_vtx++] = p->origin[2];
		}

		glEnableClientState( GL_VERTEX_ARRAY );
		glEnableClientState( GL_COLOR_ARRAY );

		glVertexPointer( 3, GL_FLOAT, 0, vtx );
		glColorPointer( 4, GL_UNSIGNED_BYTE, 0, clr );
		glDrawArrays( GL_POINTS, 0, r_newrefdef.num_particles );

		glDisableClientState( GL_VERTEX_ARRAY );
		glDisableClientState( GL_COLOR_ARRAY );

		glDisable(GL_BLEND);
		glColor4f( 1, 1, 1, 1 );
		glDepthMask(GL_TRUE);
		glEnable(GL_TEXTURE_2D);

		YQ2_VLAFREE(vtx);
		YQ2_VLAFREE(clr);
	}
	else
	{
		R_DrawParticles2(r_newrefdef.num_particles,
				r_newrefdef.particles, d_8to24table);
	}
}

void
R_PolyBlend(void)
{
	if (!gl_polyblend->value)
	{
		return;
	}

	if (!v_blend[3])
	{
		return;
	}

	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);

	glLoadIdentity();

	glRotatef(-90, 1, 0, 0); /* put Z going up */
	glRotatef(90, 0, 0, 1); /* put Z going up */

	glColor4f( v_blend[0], v_blend[1], v_blend[2], v_blend[3] );

	GLfloat vtx[] = {
		10, 100, 100,
		10, -100, 100,
		10, -100, -100,
		10, 100, -100
	};

	glEnableClientState( GL_VERTEX_ARRAY );

	glVertexPointer( 3, GL_FLOAT, 0, vtx );
	glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

	glDisableClientState( GL_VERTEX_ARRAY );

	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);

	glColor4f(1, 1, 1, 1);
}

static void
R_ResetClearColor(void)
{
	if (gl1_discardfb->value == 1 && !r_clear->value)
	{
		glClearColor(0, 0, 0, 0.5);
	}
	else
	{
		glClearColor(1, 0, 0.5, 0.5);
	}
}

void
R_SetupFrame(void)
{
	int i;
	mleaf_t *leaf;

	r_framecount++;

	/* build the transformation matrix for the given view angles */
	VectorCopy(r_newrefdef.vieworg, r_origin);

	AngleVectors(r_newrefdef.viewangles, vpn, vright, vup);

	/* current viewcluster */
	if (!(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		if (!r_worldmodel)
		{
			Com_Error(ERR_DROP, "%s: bad world model", __func__);
			return;
		}

		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf(r_origin, r_worldmodel->nodes);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		/* check above and below so crossing solid water doesn't draw wrong */
		if (!leaf->contents)
		{
			/* look down a bit */
			vec3_t temp;

			VectorCopy(r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel->nodes);

			if (!(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2))
			{
				r_viewcluster2 = leaf->cluster;
			}
		}
		else
		{
			/* look up a bit */
			vec3_t temp;

			VectorCopy(r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel->nodes);

			if (!(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2))
			{
				r_viewcluster2 = leaf->cluster;
			}
		}
	}

	for (i = 0; i < 3; i++)
	{
		v_blend[i] = r_newrefdef.blend[i] * gl_state.sw_gamma;
	}
	v_blend[3] = r_newrefdef.blend[3];

	c_brush_polys = 0;
	c_alias_polys = 0;

	/* clear out the portion of the screen that the NOWORLDMODEL defines */
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		glEnable(GL_SCISSOR_TEST);
		glClearColor(0.3, 0.3, 0.3, 1);
		glScissor(r_newrefdef.x,
				vid.height - r_newrefdef.height - r_newrefdef.y,
				r_newrefdef.width, r_newrefdef.height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		R_ResetClearColor();
		glDisable(GL_SCISSOR_TEST);
	}
}

void
R_SetPerspective(GLdouble fovy)
{
	// gluPerspective style parameters
	const GLdouble zNear = Q_max(gl_znear->value, 0.1f);
	const GLdouble zFar = (r_farsee->value) ? 8192.0f : 4096.0f;
	const GLdouble aspectratio = (GLdouble)r_newrefdef.width / r_newrefdef.height;

	GLdouble xmin, xmax, ymin, ymax;

	// traditional gluPerspective calculations - https://youtu.be/YqSNGcF5nvM?t=644
	ymax = zNear * tan(fovy * M_PI / 360.0);
	xmax = ymax * aspectratio;

	if ((r_newrefdef.rdflags & RDF_UNDERWATER) && gl1_waterwarp->value)
	{
		const GLdouble warp = sin(r_newrefdef.time * 1.5) * 0.03 * gl1_waterwarp->value;
		ymax *= 1.0 - warp;
		xmax *= 1.0 + warp;
	}

	ymin = -ymax;
	xmin = -xmax;

	if (gl_state.camera_separation)
	{
		const GLdouble separation = - gl1_stereo_convergence->value * (2 * gl_state.camera_separation) / zNear;
		xmin += separation;
		xmax += separation;
	}

	glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

void
R_SetupGL(void)
{
	int x, x2, y2, y, w, h;

	/* set up viewport */
	x = floor(r_newrefdef.x * vid.width / (float)vid.width);
	x2 = ceil((r_newrefdef.x + r_newrefdef.width) * vid.width / (float)vid.width);
	y = floor(vid.height - r_newrefdef.y * vid.height / (float)vid.height);
	y2 = ceil(vid.height -
			  (r_newrefdef.y + r_newrefdef.height) * vid.height / (float)vid.height);

	w = x2 - x;
	h = y - y2;

	qboolean drawing_left_eye = gl_state.camera_separation < 0;
	qboolean stereo_split_tb = ((gl_state.stereo_mode == STEREO_SPLIT_VERTICAL) && gl_state.camera_separation);
	qboolean stereo_split_lr = ((gl_state.stereo_mode == STEREO_SPLIT_HORIZONTAL) && gl_state.camera_separation);

	if(stereo_split_lr) {
		w = w / 2;
		x = drawing_left_eye ? (x / 2) : (x + vid.width) / 2;
	}

	if(stereo_split_tb) {
		h = h / 2;
		y2 = drawing_left_eye ? (y2 + vid.height) / 2 : (y2 / 2);
	}

	glViewport(x, y2, w, h);

	/* set up projection matrix */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	R_SetPerspective(r_newrefdef.fov_y);

	glCullFace(GL_FRONT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glRotatef(-90, 1, 0, 0); /* put Z going up */
	glRotatef(90, 0, 0, 1); /* put Z going up */
	glRotatef(-r_newrefdef.viewangles[2], 1, 0, 0);
	glRotatef(-r_newrefdef.viewangles[0], 0, 1, 0);
	glRotatef(-r_newrefdef.viewangles[1], 0, 0, 1);
	glTranslatef(-r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1],
			-r_newrefdef.vieworg[2]);

	glGetFloatv(GL_MODELVIEW_MATRIX, r_world_matrix);

	/* set drawing parms */
	if (r_cull->value)
	{
		glEnable(GL_CULL_FACE);
	}
	else
	{
		glDisable(GL_CULL_FACE);
	}

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}

void
R_Clear(void)
{
	// Define which buffers need clearing
	GLbitfield clearFlags = 0;
	GLenum depthFunc = GL_LEQUAL;

	// This breaks stereo modes, but we'll leave that responsibility to the user
	if (r_clear->value)
	{
		clearFlags |= GL_COLOR_BUFFER_BIT;
	}

	// No stencil shadows allowed when using certain stereo modes, otherwise "wallhack" happens
	if (gl_state.stereo_mode >= STEREO_MODE_ROW_INTERLEAVED && gl_state.stereo_mode <= STEREO_MODE_PIXEL_INTERLEAVED)
	{
		glClearStencil(0);
		clearFlags |= GL_STENCIL_BUFFER_BIT;
	}
	else if (gl_shadows->value && gl_state.stencil && gl1_stencilshadow->value)
	{
		glClearStencil(1);
		clearFlags |= GL_STENCIL_BUFFER_BIT;
	}

	if (gl1_ztrick->value)
	{
		static int trickframe;

		trickframe++;

		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			depthFunc = GL_GEQUAL;
		}
	}
	else
	{
		clearFlags |= GL_DEPTH_BUFFER_BIT;

		gldepthmin = 0;
		gldepthmax = 1;
	}

	switch ((int)gl1_discardfb->value)
	{
		case 1:
			if (gl_state.stereo_mode == STEREO_MODE_NONE)
			{
				clearFlags |= GL_COLOR_BUFFER_BIT;
			}
		case 2:
			clearFlags |= GL_STENCIL_BUFFER_BIT;
		default:
			break;
	}

	if (clearFlags)
	{
		glClear(clearFlags);
	}
	glDepthFunc(depthFunc);
	glDepthRange(gldepthmin, gldepthmax);

	if (gl_zfix->value)
	{
		if (gldepthmax > gldepthmin)
		{
			glPolygonOffset(0.05, 1);
		}
		else
		{
			glPolygonOffset(-0.05, -1);
		}
	}
}

void
R_Flash(void)
{
	R_PolyBlend();
}

void
R_SetGL2D(void)
{
	int x, w, y, h;
	/* set 2D virtual screen size */
	qboolean drawing_left_eye = gl_state.camera_separation < 0;
	qboolean stereo_split_tb = ((gl_state.stereo_mode == STEREO_SPLIT_VERTICAL) && gl_state.camera_separation);
	qboolean stereo_split_lr = ((gl_state.stereo_mode == STEREO_SPLIT_HORIZONTAL) && gl_state.camera_separation);

	x = 0;
	w = vid.width;
	y = 0;
	h = vid.height;

	if(stereo_split_lr) {
		w =  w / 2;
		x = drawing_left_eye ? 0 : w;
	}

	if(stereo_split_tb) {
		h =  h / 2;
		y = drawing_left_eye ? h : 0;
	}

	glViewport(x, y, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, vid.width, vid.height, 0, -99999, 99999);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glColor4f(1, 1, 1, 1);
}

/*
 * r_newrefdef must be set before the first call
 */
static void
R_RenderView(refdef_t *fd)
{
	if ((gl_state.stereo_mode != STEREO_MODE_NONE) && gl_state.camera_separation) {

		qboolean drawing_left_eye = gl_state.camera_separation < 0;
		switch (gl_state.stereo_mode) {
			case STEREO_MODE_ANAGLYPH:
				{

					// Work out the colour for each eye.
					int anaglyph_colours[] = { 0x4, 0x3 }; // Left = red, right = cyan.

					if (strlen(gl1_stereo_anaglyph_colors->string) == 2) {
						int eye, colour, missing_bits;
						// Decode the colour name from its character.
						for (eye = 0; eye < 2; ++eye) {
							colour = 0;
							switch (toupper((unsigned char)gl1_stereo_anaglyph_colors->string[eye])) {
								case 'B': ++colour; // 001 Blue
								case 'G': ++colour; // 010 Green
								case 'C': ++colour; // 011 Cyan
								case 'R': ++colour; // 100 Red
								case 'M': ++colour; // 101 Magenta
								case 'Y': ++colour; // 110 Yellow
									anaglyph_colours[eye] = colour;
									break;
							}
						}
						// Fill in any missing bits.
						missing_bits = ~(anaglyph_colours[0] | anaglyph_colours[1]) & 0x3;
						for (eye = 0; eye < 2; ++eye) {
							anaglyph_colours[eye] |= missing_bits;
						}
					}

					// Set the current colour.
					glColorMask(
						!!(anaglyph_colours[drawing_left_eye] & 0x4),
						!!(anaglyph_colours[drawing_left_eye] & 0x2),
						!!(anaglyph_colours[drawing_left_eye] & 0x1),
						GL_TRUE
					);
				}
				break;
			case STEREO_MODE_ROW_INTERLEAVED:
			case STEREO_MODE_COLUMN_INTERLEAVED:
			case STEREO_MODE_PIXEL_INTERLEAVED:
				{
					qboolean flip_eyes = true;
					int client_x, client_y;

					GLshort screen[] = {
						0, 0,
						(GLshort)vid.width, 0,
						(GLshort)vid.width, (GLshort)vid.height,
						0, (GLshort)vid.height
					};

					//GLimp_GetClientAreaOffset(&client_x, &client_y);
					client_x = 0;
					client_y = 0;

					R_SetGL2D();

					glEnable(GL_STENCIL_TEST);
					glStencilMask(1);
					glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

					glStencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);
					glStencilFunc(GL_NEVER, 0, 1);

					glEnableClientState(GL_VERTEX_ARRAY);
					glVertexPointer(2, GL_SHORT, 0, screen);
					glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
					glDisableClientState(GL_VERTEX_ARRAY);

					glStencilOp(GL_INVERT, GL_KEEP, GL_KEEP);
					glStencilFunc(GL_NEVER, 1, 1);

					if (gl_state.stereo_mode == STEREO_MODE_ROW_INTERLEAVED || gl_state.stereo_mode == STEREO_MODE_PIXEL_INTERLEAVED)
					{
						for (int y = 0; y <= vid.height; y += 2)
						{
							gl_buf.vtx[gl_buf.vt    ] = 0;
							gl_buf.vtx[gl_buf.vt + 1] = y - 0.5f;
							gl_buf.vtx[gl_buf.vt + 2] = vid.width;
							gl_buf.vtx[gl_buf.vt + 3] = y - 0.5f;
							gl_buf.vt += 4;
						}
						flip_eyes ^= (client_y & 1);
					}

					if (gl_state.stereo_mode == STEREO_MODE_COLUMN_INTERLEAVED || gl_state.stereo_mode == STEREO_MODE_PIXEL_INTERLEAVED)
					{
						for (int x = 0; x <= vid.width; x += 2)
						{
							gl_buf.vtx[gl_buf.vt    ] = x - 0.5f;
							gl_buf.vtx[gl_buf.vt + 1] = 0;
							gl_buf.vtx[gl_buf.vt + 2] = x - 0.5f;
							gl_buf.vtx[gl_buf.vt + 3] = vid.height;
							gl_buf.vt += 4;
						}
						flip_eyes ^= (client_x & 1);
					}

					glEnableClientState(GL_VERTEX_ARRAY);
					glVertexPointer(2, GL_FLOAT, 0, gl_buf.vtx);
					glDrawArrays(GL_LINES, 0, gl_buf.vt / 2);
					glDisableClientState(GL_VERTEX_ARRAY);
					gl_buf.vt = 0;

					glStencilMask(0);
					glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

					glStencilFunc(GL_EQUAL, drawing_left_eye ^ flip_eyes, 1);
					glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
				}
				break;
			default:
				break;
		}
	}

	if (r_norefresh->value)
	{
		return;
	}

	r_newrefdef = *fd;

	if (!r_worldmodel && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		Com_Error(ERR_DROP, "%s: NULL worldmodel", __func__);
	}

	if (r_speeds->value)
	{
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	R_PushDlights();

	if (gl_finish->value)
	{
		glFinish();
	}

	R_SetupFrame();

	R_SetFrustum(vup, vpn, vright, r_origin, r_newrefdef.fov_x, r_newrefdef.fov_y,
		frustum);

	R_SetupGL();

	R_MarkLeaves(); /* done here so we know if we're in water */

	R_DrawWorld();

	R_DrawEntitiesOnList();

	R_RenderDlights();

	R_DrawParticles();

	R_DrawAlphaSurfaces();

	R_Flash();

	if (r_speeds->value)
	{
		Com_Printf("%4i wpoly %4i epoly %i tex %i lmaps\n",
				c_brush_polys, c_alias_polys, c_visible_textures,
				c_visible_lightmaps);
	}

	switch (gl_state.stereo_mode) {
		case STEREO_MODE_NONE:
			break;
		case STEREO_MODE_ANAGLYPH:
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			break;
		case STEREO_MODE_ROW_INTERLEAVED:
		case STEREO_MODE_COLUMN_INTERLEAVED:
		case STEREO_MODE_PIXEL_INTERLEAVED:
			glDisable(GL_STENCIL_TEST);
			break;
		default:
			break;
	}
}

enum opengl_special_buffer_modes
GL_GetSpecialBufferModeForStereoMode(enum stereo_modes stereo_mode) {
	switch (stereo_mode) {
		case STEREO_MODE_NONE:
		case STEREO_SPLIT_HORIZONTAL:
		case STEREO_SPLIT_VERTICAL:
		case STEREO_MODE_ANAGLYPH:
			return OPENGL_SPECIAL_BUFFER_MODE_NONE;
		case STEREO_MODE_OPENGL:
			return OPENGL_SPECIAL_BUFFER_MODE_STEREO;
		case STEREO_MODE_ROW_INTERLEAVED:
		case STEREO_MODE_COLUMN_INTERLEAVED:
		case STEREO_MODE_PIXEL_INTERLEAVED:
			return OPENGL_SPECIAL_BUFFER_MODE_STENCIL;
	}
	return OPENGL_SPECIAL_BUFFER_MODE_NONE;
}

static void
R_SetLightLevel(entity_t *currententity)
{
	vec3_t shadelight;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		return;
	}

	/* save off light value for server to look at */
	R_LightPoint(currententity, r_newrefdef.vieworg, shadelight);

	/* pick the greatest component, which should be the
	 * same as the mono value returned by software */
	if (shadelight[0] > shadelight[1])
	{
		if (shadelight[0] > shadelight[2])
		{
			r_lightlevel->value = 150 * shadelight[0];
		}
		else
		{
			r_lightlevel->value = 150 * shadelight[2];
		}
	}
	else
	{
		if (shadelight[1] > shadelight[2])
		{
			r_lightlevel->value = 150 * shadelight[1];
		}
		else
		{
			r_lightlevel->value = 150 * shadelight[2];
		}
	}
}

static void
RI_RenderFrame(refdef_t *fd)
{
	R_ApplyGLBuffer();	// menu rendering when needed
	R_RenderView(fd);
	R_SetLightLevel (NULL);
	R_SetGL2D();
}

#ifdef YQ2_GL1_GLES
#define GLES1_ENABLED_ONLY	"1"
#else
#define GLES1_ENABLED_ONLY	"0"
#endif

void
R_Register(void)
{
	gl_lefthand = ri.Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	r_gunfov = ri.Cvar_Get("r_gunfov", "80", CVAR_ARCHIVE);
	r_farsee = ri.Cvar_Get("r_farsee", "0", CVAR_LATCH | CVAR_ARCHIVE);
	r_norefresh = ri.Cvar_Get("r_norefresh", "0", 0);
	r_fullbright = ri.Cvar_Get("r_fullbright", "0", 0);
	r_drawentities = ri.Cvar_Get("r_drawentities", "1", 0);
	r_drawworld = ri.Cvar_Get("r_drawworld", "1", 0);
	r_novis = ri.Cvar_Get("r_novis", "0", 0);
	r_lerpmodels = ri.Cvar_Get("r_lerpmodels", "1", 0);
	r_speeds = ri.Cvar_Get("r_speeds", "0", 0);

	r_lightlevel = ri.Cvar_Get("r_lightlevel", "0", 0);
	gl1_overbrightbits = ri.Cvar_Get("gl1_overbrightbits", "0", CVAR_ARCHIVE);

	gl1_particle_min_size = ri.Cvar_Get("gl1_particle_min_size", "2", CVAR_ARCHIVE);
	gl1_particle_max_size = ri.Cvar_Get("gl1_particle_max_size", "40", CVAR_ARCHIVE);
	gl1_particle_size = ri.Cvar_Get("gl1_particle_size", "40", CVAR_ARCHIVE);
	gl1_particle_att_a = ri.Cvar_Get("gl1_particle_att_a", "0.01", CVAR_ARCHIVE);
	gl1_particle_att_b = ri.Cvar_Get("gl1_particle_att_b", "0.0", CVAR_ARCHIVE);
	gl1_particle_att_c = ri.Cvar_Get("gl1_particle_att_c", "0.01", CVAR_ARCHIVE);
	gl1_particle_square = ri.Cvar_Get("gl1_particle_square", "0", CVAR_ARCHIVE);

	r_modulate = ri.Cvar_Get("r_modulate", "1", CVAR_ARCHIVE);
	r_mode = ri.Cvar_Get("r_mode", "4", CVAR_ARCHIVE);
	gl_lightmap = ri.Cvar_Get("r_lightmap", "0", 0);
	gl_shadows = ri.Cvar_Get("r_shadows", "0", CVAR_ARCHIVE);
	gl1_stencilshadow = ri.Cvar_Get("gl1_stencilshadow", "0", CVAR_ARCHIVE);
	gl1_dynamic = ri.Cvar_Get("gl1_dynamic", "1", 0);
	gl_nobind = ri.Cvar_Get("gl_nobind", "0", 0);
	gl1_round_down = ri.Cvar_Get("gl1_round_down", "1", 0);
	gl1_picmip = ri.Cvar_Get("gl1_picmip", "0", 0);
	gl_showtris = ri.Cvar_Get("gl_showtris", "0", 0);
	gl_showbbox = ri.Cvar_Get("gl_showbbox", "0", 0);
	gl1_ztrick = ri.Cvar_Get("gl1_ztrick", "0", 0);
	gl_zfix = ri.Cvar_Get("gl_zfix", "0", 0);
	gl_finish = ri.Cvar_Get("gl_finish", "0", CVAR_ARCHIVE);
	r_clear = ri.Cvar_Get("r_clear", "0", 0);
	r_cull = ri.Cvar_Get("r_cull", "1", 0);
	gl_polyblend = ri.Cvar_Get("gl_polyblend", "1", 0);
	gl1_flashblend = ri.Cvar_Get("gl1_flashblend", "0", 0);
	r_fixsurfsky = ri.Cvar_Get("r_fixsurfsky", "0", CVAR_ARCHIVE);
	gl_znear = ri.Cvar_Get("gl_znear", "4", CVAR_ARCHIVE);
	gl1_minlight = ri.Cvar_Get("gl1_minlight", "0", CVAR_ARCHIVE);

	gl_texturemode = ri.Cvar_Get("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE);
	gl1_texturealphamode = ri.Cvar_Get("gl1_texturealphamode", "default", CVAR_ARCHIVE);
	gl1_texturesolidmode = ri.Cvar_Get("gl1_texturesolidmode", "default", CVAR_ARCHIVE);
	gl_anisotropic = ri.Cvar_Get("r_anisotropic", "0", CVAR_ARCHIVE);
	r_lockpvs = ri.Cvar_Get("r_lockpvs", "0", 0);

	gl1_palettedtexture = ri.Cvar_Get("r_palettedtextures", "0", CVAR_ARCHIVE);
	gl1_pointparameters = ri.Cvar_Get("gl1_pointparameters", "1", CVAR_ARCHIVE);
	gl1_multitexture = ri.Cvar_Get("gl1_multitexture", "1", CVAR_ARCHIVE);
	gl1_lightmapcopies = ri.Cvar_Get("gl1_lightmapcopies", GLES1_ENABLED_ONLY, CVAR_ARCHIVE);
	gl1_discardfb = ri.Cvar_Get("gl1_discardfb", GLES1_ENABLED_ONLY, CVAR_ARCHIVE);

	gl_drawbuffer = ri.Cvar_Get("gl_drawbuffer", "GL_BACK", 0);
	r_vsync = ri.Cvar_Get("r_vsync", "1", CVAR_ARCHIVE);

	gl1_saturatelighting = ri.Cvar_Get("gl1_saturatelighting", "0", 0);

	vid_fullscreen = ri.Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = ri.Cvar_Get("vid_gamma", "1.2", CVAR_ARCHIVE);

	r_customwidth = ri.Cvar_Get("r_customwidth", "1024", CVAR_ARCHIVE);
	r_customheight = ri.Cvar_Get("r_customheight", "768", CVAR_ARCHIVE);
	gl_msaa_samples = ri.Cvar_Get ( "r_msaa_samples", "0", CVAR_ARCHIVE );

	r_retexturing = ri.Cvar_Get("r_retexturing", "1", CVAR_ARCHIVE);
	r_validation = ri.Cvar_Get("r_validation", "0", CVAR_ARCHIVE);
	r_scale8bittextures = ri.Cvar_Get("r_scale8bittextures", "0", CVAR_ARCHIVE);

	/* don't bilerp characters and crosshairs */
	gl_nolerp_list = ri.Cvar_Get("r_nolerp_list", "pics/conchars.pcx pics/ch1.pcx pics/ch2.pcx pics/ch3.pcx", CVAR_ARCHIVE);
	/* textures that should always be filtered, even if r_2D_unfiltered or an unfiltered gl mode is used */
	r_lerp_list = ri.Cvar_Get("r_lerp_list", "", CVAR_ARCHIVE);
	/* don't bilerp any 2D elements */
	r_2D_unfiltered = ri.Cvar_Get("r_2D_unfiltered", "0", CVAR_ARCHIVE);
	/* don't bilerp videos */
	r_videos_unfiltered = ri.Cvar_Get("r_videos_unfiltered", "0", CVAR_ARCHIVE);

	gl1_stereo = ri.Cvar_Get( "gl1_stereo", "0", CVAR_ARCHIVE );
	gl1_stereo_separation = ri.Cvar_Get( "gl1_stereo_separation", "-0.4", CVAR_ARCHIVE );
	gl1_stereo_anaglyph_colors = ri.Cvar_Get( "gl1_stereo_anaglyph_colors", "rc", CVAR_ARCHIVE );
	gl1_stereo_convergence = ri.Cvar_Get( "gl1_stereo_convergence", "1", CVAR_ARCHIVE );

	gl1_waterwarp = ri.Cvar_Get( "gl1_waterwarp", "1.0", CVAR_ARCHIVE );

	ri.Cmd_AddCommand("imagelist", R_ImageList_f);
	ri.Cmd_AddCommand("screenshot", R_ScreenShot);
	ri.Cmd_AddCommand("modellist", Mod_Modellist_f);
	ri.Cmd_AddCommand("gl_strings", R_Strings);
}

#undef GLES1_ENABLED_ONLY

/*
 * Changes the video mode
 */
static int
SetMode_impl(int *pwidth, int *pheight, int mode, int fullscreen)
{
	Com_Printf("Setting mode %d:", mode);

	/* mode -1 is not in the vid mode table - so we keep the values in pwidth
	   and pheight and don't even try to look up the mode info */
	if ((mode >= 0) && !ri.Vid_GetModeInfo(pwidth, pheight, mode))
	{
		Com_Printf(" invalid mode\n");
		return rserr_invalid_mode;
	}

	/* We trying to get resolution from desktop */
	if (mode == -2)
	{
		if(!ri.GLimp_GetDesktopMode(pwidth, pheight))
		{
			Com_Printf(" can't detect mode\n" );
			return rserr_invalid_mode;
		}
	}

	Com_Printf(" %dx%d (vid_fullscreen %i)\n", *pwidth, *pheight, fullscreen);

	if (!ri.GLimp_InitGraphics(fullscreen, pwidth, pheight))
	{
		return rserr_invalid_mode;
	}

	/* This is totaly obscure: For some strange reasons the renderer
	   maintains two(!) repesentations of the resolution. One comes
	   from the client and is saved in r_newrefdef. The other one
	   is determined here and saved in vid. Several calculations take
	   both representations into account.

	   The values will always be the same. The GLimp_InitGraphics()
	   call above communicates the requested resolution to the client
	   where it ends up in the vid subsystem and the vid system writes
	   it into r_newrefdef.

	   We can't avoid the client roundtrip, because we can get the
	   real size of the drawable (which can differ from the resolution
	   due to high dpi awareness) only after the render context was
	   created by GLimp_InitGraphics() and need to communicate it
	   somehow to the client. So we just overwrite the values saved
	   in vid with a call to RI_GetDrawableSize(), just like the
	   client does. This makes sure that both values are the same
	   and everything is okay.

	   We also need to take the special case fullscreen window into
	   account. With the fullscreen windows we cannot use the
	   drawable size, it would scale all cases to the size of the
	   window. Instead use the drawable size when the user wants
	   native resolution (the fullscreen window fills the screen)
	   and use the requested resolution in all other cases. */
	if (IsHighDPIaware)
	{
		if (vid_fullscreen->value != 2)
		{
			RI_GetDrawableSize(pwidth, pheight);
		}
		else
		{
			if (r_mode->value == -2)
			{
				/* User requested native resolution. */
				RI_GetDrawableSize(pwidth, pheight);
			}
		}
	}

	return rserr_ok;
}

qboolean
R_SetMode(void)
{
	rserr_t err;
	int fullscreen;

	fullscreen = (int)vid_fullscreen->value;

	/* a bit hackish approach to enable custom resolutions:
	   Glimp_SetMode needs these values set for mode -1 */
	vid.width = r_customwidth->value;
	vid.height = r_customheight->value;

	if ((err = SetMode_impl(&vid.width, &vid.height, r_mode->value,
					 fullscreen)) == rserr_ok)
	{
		if (r_mode->value == -1)
		{
			gl_state.prev_mode = 4; /* safe default for custom mode */
		}
		else
		{
			gl_state.prev_mode = r_mode->value;
		}
	}
	else
	{
		if (err == rserr_invalid_mode)
		{
			Com_Printf("ref_gl::R_SetMode() - invalid mode\n");
			if (gl_msaa_samples->value != 0.0f)
			{
				Com_Printf("gl_msaa_samples was %d - will try again with gl_msaa_samples = 0\n", (int)gl_msaa_samples->value);
				ri.Cvar_SetValue("r_msaa_samples", 0.0f);
				gl_msaa_samples->modified = false;

				if ((err = SetMode_impl(&vid.width, &vid.height, r_mode->value, 0)) == rserr_ok)
				{
					return true;
				}
			}
			if(r_mode->value == gl_state.prev_mode)
			{
				// trying again would result in a crash anyway, give up already
				// (this would happen if your initing fails at all and your resolution already was 640x480)
				return false;
			}
			ri.Cvar_SetValue("r_mode", gl_state.prev_mode);
			r_mode->modified = false;
		}

		/* try setting it back to something safe */
		if ((err = SetMode_impl(&vid.width, &vid.height, gl_state.prev_mode, 0)) != rserr_ok)
		{
			Com_Printf("ref_gl::R_SetMode() - could not revert to safe mode\n");
			return false;
		}
	}

	return true;
}

// just to avoid too many preprocessor directives in RI_Init()
typedef enum
{
	rf_opengl14,
	rf_opengles10
} refresher_t;

qboolean
RI_Init(void)
{
	int j;
	byte *colormap;
	extern float r_turbsin[256];

#ifdef YQ2_GL1_GLES
#define GLEXTENSION_NPOT	"GL_OES_texture_npot"
	static const refresher_t refresher = rf_opengles10;
#else
#define GLEXTENSION_NPOT	"GL_ARB_texture_non_power_of_two"
	static const refresher_t refresher = rf_opengl14;
#endif

	Swap_Init();

	for (j = 0; j < 256; j++)
	{
		r_turbsin[j] *= 0.5;
	}

	Com_Printf("Refresh: " REF_VERSION "\n");
	Com_Printf("Client: " YQ2VERSION "\n\n");

#ifdef DEBUG
	Com_Printf("ref_gl1::%s - DEBUG mode enabled\n", __func__);
#endif

	GetPCXPalette (&colormap, d_8to24table);
	free(colormap);

	R_Register();

	/* initialize our QGL dynamic bindings */
	QGL_Init();

	/* set our "safe" mode */
	gl_state.prev_mode = 4;
	gl_state.stereo_mode = gl1_stereo->value;

	/* create the window and set up the context */
	if (!R_SetMode())
	{
		QGL_Shutdown();
		Com_Printf("ref_gl1::%s - could not R_SetMode()\n", __func__);
		return false;
	}

	ri.Vid_MenuInit();

	// --------

	/* get our various GL strings */
	Com_Printf("\nOpenGL setting:\n");

	gl_config.vendor_string = (char *)glGetString(GL_VENDOR);
	Com_Printf("GL_VENDOR: %s\n", gl_config.vendor_string);

	gl_config.renderer_string = (char *)glGetString(GL_RENDERER);
	Com_Printf("GL_RENDERER: %s\n", gl_config.renderer_string);

	gl_config.version_string = (char *)glGetString(GL_VERSION);
	Com_Printf("GL_VERSION: %s\n", gl_config.version_string);

	gl_config.extensions_string = (char *)glGetString(GL_EXTENSIONS);
	Com_Printf("GL_EXTENSIONS: %s\n", gl_config.extensions_string);

	sscanf(gl_config.version_string, "%d.%d", &gl_config.major_version, &gl_config.minor_version);

	if (refresher == rf_opengl14 && gl_config.major_version == 1)
	{
		if (gl_config.minor_version < 4)
		{
			QGL_Shutdown();
			Com_Printf("Support for OpenGL 1.4 is not available\n");

			return false;
		}
	}

	Com_Printf("\n\nProbing for OpenGL extensions:\n");

	// ----

	/* Point parameters */
	Com_Printf(" - Point parameters: ");

	if ( refresher == rf_opengles10 ||
		strstr(gl_config.extensions_string, "GL_ARB_point_parameters") ||
		strstr(gl_config.extensions_string, "GL_EXT_point_parameters") )	// should exist for all OGL 1.4 hw...
	{
		qglPointParameterf = (void (APIENTRY *)(GLenum, GLfloat))RI_GetProcAddress ( "glPointParameterf" );
		qglPointParameterfv = (void (APIENTRY *)(GLenum, const GLfloat *))RI_GetProcAddress ( "glPointParameterfv" );

		if (!qglPointParameterf || !qglPointParameterfv)
		{
			qglPointParameterf = (void (APIENTRY *)(GLenum, GLfloat))RI_GetProcAddress ( "glPointParameterfARB" );
			qglPointParameterfv = (void (APIENTRY *)(GLenum, const GLfloat *))RI_GetProcAddress ( "glPointParameterfvARB" );
		}
		if (!qglPointParameterf || !qglPointParameterfv)
		{
			qglPointParameterf = (void (APIENTRY *)(GLenum, GLfloat))RI_GetProcAddress ( "glPointParameterfEXT" );
			qglPointParameterfv = (void (APIENTRY *)(GLenum, const GLfloat *))RI_GetProcAddress ( "glPointParameterfvEXT" );
		}
	}

	gl_config.pointparameters = false;

	if (gl1_pointparameters->value)
	{
		if (qglPointParameterf && qglPointParameterfv)
		{
			gl_config.pointparameters = true;
			Com_Printf("Okay\n");
		}
		else
		{
			Com_Printf("Failed\n");
		}
	}
	else
	{
		Com_Printf("Disabled\n");
	}

	// ----

	/* Paletted texture */
	Com_Printf(" - Paletted texture: ");

	if (strstr(gl_config.extensions_string, "GL_EXT_paletted_texture") &&
		strstr(gl_config.extensions_string, "GL_EXT_shared_texture_palette"))
	{
			qglColorTableEXT = (void (APIENTRY *)(GLenum, GLenum, GLsizei, GLenum, GLenum, const GLvoid * ))
					RI_GetProcAddress ("glColorTableEXT");
	}

	gl_config.palettedtexture = false;

	if (gl1_palettedtexture->value)
	{
		if (qglColorTableEXT)
		{
			gl_config.palettedtexture = true;
			Com_Printf("Okay\n");
		}
		else
		{
			Com_Printf("Failed\n");
		}
	}
	else
	{
		Com_Printf("Disabled\n");
	}

	// --------

	/* Anisotropic */
	Com_Printf(" - Anisotropic: ");

	if (strstr(gl_config.extensions_string, "GL_EXT_texture_filter_anisotropic"))
	{
		gl_config.anisotropic = true;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_config.max_anisotropy);

		Com_Printf("%ux\n", (int)gl_config.max_anisotropy);
	}
	else
	{
		gl_config.anisotropic = false;
		gl_config.max_anisotropy = 0.0;

		Com_Printf("Failed\n");
	}

	// ----

	/* Non power of two textures */
	Com_Printf(" - Non power of two textures: ");

	if (strstr(gl_config.extensions_string, GLEXTENSION_NPOT))
	{
		gl_config.npottextures = true;
		Com_Printf("Okay\n");
	}
	else
	{
		gl_config.npottextures = false;
		Com_Printf("Failed\n");
	}

#undef GLEXTENSION_NPOT

	// ----

	/* Multitexturing */
	gl_config.multitexture = false;

	Com_Printf(" - Multitexturing: ");

	if ( refresher == rf_opengles10 || strstr(gl_config.extensions_string, "GL_ARB_multitexture") )
	{
		qglActiveTexture = (void (APIENTRY *)(GLenum))RI_GetProcAddress ("glActiveTexture");
		qglClientActiveTexture = (void (APIENTRY *)(GLenum))RI_GetProcAddress ("glClientActiveTexture");

		if (!qglActiveTexture || !qglClientActiveTexture)
		{
			qglActiveTexture = (void (APIENTRY *)(GLenum))RI_GetProcAddress ("glActiveTextureARB");
			qglClientActiveTexture = (void (APIENTRY *)(GLenum))RI_GetProcAddress ("glClientActiveTextureARB");
		}
	}

	if (gl1_multitexture->value)
	{
		if (qglActiveTexture && qglClientActiveTexture)
		{
			gl_config.multitexture = true;
			Com_Printf("Okay\n");
		}
		else
		{
			Com_Printf("Failed\n");
		}
	}
	else
	{
		Com_Printf("Disabled\n");
	}

	// ----

	/* Lightmap copies: keep multiple copies of "the same" lightmap on video memory.
	 * All of them are actually different, because they are affected by different dynamic lighting,
	 * in different frames. This is not meant for Immediate-Mode Rendering systems (desktop),
	 * but for Tile-Based / Deferred Rendering ones (embedded / mobile), since active manipulation
	 * of textures already being used in the last few frames can cause slowdown on these systems.
	 * Needless to say, GPU memory usage is highly increased, so watch out in low memory situations.
	 */

	Com_Printf(" - Lightmap copies: ");
	gl_config.lightmapcopies = false;
	if (gl_config.multitexture && gl1_lightmapcopies->value)
	{
		gl_config.lightmapcopies = true;
		Com_Printf("Okay\n");
	}
	else
	{
		Com_Printf("Disabled\n");
	}

	// ----

	/* Discard framebuffer: Enables the use of a "performance hint" to the graphic
	 * driver in GLES1, to get rid of the contents of the different framebuffers.
	 * Useful for some GPUs that may attempt to keep them and/or write them back to
	 * external/uniform memory, actions that are useless for Quake 2 rendering path.
	 * https://registry.khronos.org/OpenGL/extensions/EXT/EXT_discard_framebuffer.txt
	 * This extension is used by 'gl1_discardfb', and regardless of its existence,
	 * that cvar will enable glClear at the start of each frame, helping mobile GPUs.
	 */

#ifdef YQ2_GL1_GLES
	Com_Printf(" - Discard framebuffer: ");

	if (strstr(gl_config.extensions_string, "GL_EXT_discard_framebuffer"))
	{
		qglDiscardFramebufferEXT = (void (APIENTRY *)(GLenum, GLsizei, const GLenum *))
				RI_GetProcAddress ("glDiscardFramebufferEXT");
	}

	if (gl1_discardfb->value)
	{
		if (qglDiscardFramebufferEXT)	// enough to verify availability
		{
			Com_Printf("Okay\n");
		}
		else
		{
			Com_Printf("Failed\n");
		}
	}
	else
	{
		Com_Printf("Disabled\n");
	}
#endif

	// ----

	R_ResetClearColor();
	R_SetDefaultState();

	Scrap_Init();
	R_InitImages();
	Mod_Init();
	R_InitParticleTexture();
	Draw_InitLocal();
	R_ResetGLBuffer();

	return true;
}

void
RI_Shutdown(void)
{
	ri.Cmd_RemoveCommand("modellist");
	ri.Cmd_RemoveCommand("screenshot");
	ri.Cmd_RemoveCommand("imagelist");
	ri.Cmd_RemoveCommand("gl_strings");

	LM_FreeLightmapBuffers();
	Mod_FreeAll();

	R_ShutdownImages();

	/* shutdown OS specific OpenGL stuff like contexts, etc.  */
	RI_ShutdownContext();

	/* shutdown our QGL subsystem */
	QGL_Shutdown();
}

void
RI_BeginFrame(float camera_separation)
{
	gl_state.camera_separation = camera_separation;

	// force a vid_restart if gl1_stereo has been modified.
	if ( gl_state.stereo_mode != gl1_stereo->value )
	{
		// If we've gone from one mode to another with the same special buffer requirements there's no need to restart.
		if ( GL_GetSpecialBufferModeForStereoMode( gl_state.stereo_mode ) == GL_GetSpecialBufferModeForStereoMode( gl1_stereo->value ) )
		{
			gl_state.stereo_mode = gl1_stereo->value;
		}
		else
		{
			Com_Printf("stereo supermode changed, restarting video!\n");
			ri.Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
		}
	}

	if (vid_gamma->modified)
	{
		vid_gamma->modified = false;
		RI_UpdateGamma();
	}

	// Clamp overbrightbits
	if (gl1_overbrightbits->modified)
	{
		int obb_val = (int)gl1_overbrightbits->value;

		obb_val = Q_clamp(obb_val, 0, 4);
		if (obb_val == 3)	// allowed values: 0,1,2,4
		{
			obb_val = 2;
		}

		ri.Cvar_SetValue("gl1_overbrightbits", obb_val);
		gl1_overbrightbits->modified = false;
	}

	/* go into 2D mode */

	// FIXME: just call R_SetGL2D();

	int x, w, y, h;
	qboolean drawing_left_eye = gl_state.camera_separation < 0;
	qboolean stereo_split_tb = ((gl_state.stereo_mode == STEREO_SPLIT_VERTICAL) && gl_state.camera_separation);
	qboolean stereo_split_lr = ((gl_state.stereo_mode == STEREO_SPLIT_HORIZONTAL) && gl_state.camera_separation);

	x = 0;
	w = vid.width;
	y = 0;
	h = vid.height;

	if(stereo_split_lr) {
		w =  w / 2;
		x = drawing_left_eye ? 0 : w;
	}

	if(stereo_split_tb) {
		h =  h / 2;
		y = drawing_left_eye ? h : 0;
	}

	glViewport(x, y, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, vid.width, vid.height, 0, -99999, 99999);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glColor4f(1, 1, 1, 1);

	if (gl1_particle_square->modified)
	{
		if (gl_config.pointparameters)
		{
			/* GL_POINT_SMOOTH is not implemented by some OpenGL
			   drivers, especially the crappy Mesa3D backends like
			   i915.so. That the points are squares and not circles
			   is not a problem by Quake II! */
			if (gl1_particle_square->value)
			{
				glDisable(GL_POINT_SMOOTH);
			}
			else
			{
				glEnable(GL_POINT_SMOOTH);
			}
		}
		else
		{
			// particles aren't drawn as GL_POINTS, but as textured triangles
			// => update particle texture to look square - or circle-ish
			R_InitParticleTexture();
		}

		gl1_particle_square->modified = false;
	}

	/* draw buffer stuff */
	if (gl_drawbuffer->modified)
	{
		gl_drawbuffer->modified = false;

#ifndef YQ2_GL1_GLES
		if ((gl_state.camera_separation == 0) || gl_state.stereo_mode != STEREO_MODE_OPENGL)
		{
			if (Q_stricmp(gl_drawbuffer->string, "GL_FRONT") == 0)
			{
				glDrawBuffer(GL_FRONT);
			}
			else
			{
				glDrawBuffer(GL_BACK);
			}
		}
#endif
	}

	/* texturemode stuff */
	if (gl_texturemode->modified || (gl_config.anisotropic && gl_anisotropic->modified)
	    || gl_nolerp_list->modified || r_lerp_list->modified
		|| r_2D_unfiltered->modified || r_videos_unfiltered->modified)
	{
		R_TextureMode(gl_texturemode->string);
		gl_texturemode->modified = false;
		gl_anisotropic->modified = false;
		gl_nolerp_list->modified = false;
		r_lerp_list->modified = false;
		r_2D_unfiltered->modified = false;
		r_videos_unfiltered->modified = false;
	}

	if (gl1_texturealphamode->modified)
	{
		R_TextureAlphaMode(gl1_texturealphamode->string);
		gl1_texturealphamode->modified = false;
	}

	if (gl1_texturesolidmode->modified)
	{
		R_TextureSolidMode(gl1_texturesolidmode->string);
		gl1_texturesolidmode->modified = false;
	}

	if (r_vsync->modified)
	{
		r_vsync->modified = false;
		RI_SetVsync();
	}

	/* clear screen if desired */
	R_Clear();
}

void
RI_SetPalette(const unsigned char *palette)
{
	int i;

	byte *rp = (byte *)r_rawpalette;

	if (palette)
	{
		for (i = 0; i < 256; i++)
		{
			rp[i * 4 + 0] = gammatable[palette[i * 3 + 0]];
			rp[i * 4 + 1] = gammatable[palette[i * 3 + 1]];
			rp[i * 4 + 2] = gammatable[palette[i * 3 + 2]];
			rp[i * 4 + 3] = 0xff;
		}
	}
	else
	{
		for (i = 0; i < 256; i++)
		{
			rp[i * 4 + 0] = LittleLong(d_8to24table[i]) & 0xff;
			rp[i * 4 + 1] = (LittleLong(d_8to24table[i]) >> 8) & 0xff;
			rp[i * 4 + 2] = (LittleLong(d_8to24table[i]) >> 16) & 0xff;
			rp[i * 4 + 3] = 0xff;
		}
	}

	R_SetTexturePalette(r_rawpalette);

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	R_ResetClearColor();
}

/* R_DrawBeam */
void
R_DrawBeam(entity_t *e)
{
	int i, clr[4];

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	GLfloat vtx[3*NUM_BEAM_SEGS*4];
	unsigned int index_vtx = 0;
	unsigned int pointb;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if (VectorNormalize(normalized_direction) == 0)
	{
		return;
	}

	PerpendicularVector(perpvec, normalized_direction);
	VectorScale(perpvec, e->frame / 2, perpvec);

	for (i = 0; i < 6; i++)
	{
		RotatePointAroundVector(start_points[i], normalized_direction, perpvec,
				(360.0 / NUM_BEAM_SEGS) * i);
		VectorAdd(start_points[i], origin, start_points[i]);
		VectorAdd(start_points[i], direction, end_points[i]);
	}

	R_EnableMultitexture(false);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glDepthMask(GL_FALSE);

	clr[0] = (LittleLong(d_8to24table[e->skinnum & 0xFF])) & 0xFF;
	clr[1] = (LittleLong(d_8to24table[e->skinnum & 0xFF]) >> 8) & 0xFF;
	clr[2] = (LittleLong(d_8to24table[e->skinnum & 0xFF]) >> 16) & 0xFF;
	clr[3] = e->alpha * 255;

	glColor4ub(gammatable[clr[0]], gammatable[clr[1]],
		gammatable[clr[2]], clr[3]);

	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		vtx[index_vtx++] = start_points [ i ][ 0 ];
		vtx[index_vtx++] = start_points [ i ][ 1 ];
		vtx[index_vtx++] = start_points [ i ][ 2 ];

		vtx[index_vtx++] = end_points [ i ][ 0 ];
		vtx[index_vtx++] = end_points [ i ][ 1 ];
		vtx[index_vtx++] = end_points [ i ][ 2 ];

		pointb = ( i + 1 ) % NUM_BEAM_SEGS;
		vtx[index_vtx++] = start_points [ pointb ][ 0 ];
		vtx[index_vtx++] = start_points [ pointb ][ 1 ];
		vtx[index_vtx++] = start_points [ pointb ][ 2 ];

		vtx[index_vtx++] = end_points [ pointb ][ 0 ];
		vtx[index_vtx++] = end_points [ pointb ][ 1 ];
		vtx[index_vtx++] = end_points [ pointb ][ 2 ];
	}

	glEnableClientState( GL_VERTEX_ARRAY );

	glVertexPointer( 3, GL_FLOAT, 0, vtx );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, NUM_BEAM_SEGS*4 );

	glDisableClientState( GL_VERTEX_ARRAY );

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
}

extern int RI_PrepareForWindow(void);
extern int RI_InitContext(void* win);

extern void RI_BeginRegistration(const char *model);
extern struct model_s * RI_RegisterModel(const char *name);
extern struct image_s * RI_RegisterSkin(const char *name);

extern void RI_SetSky(const char *name, float rotate, vec3_t axis);
extern void RI_EndRegistration(void);

extern void RI_RenderFrame(refdef_t *fd);

extern void RI_SetPalette(const unsigned char *palette);
extern qboolean RI_IsVSyncActive(void);
extern void RI_EndFrame(void);

/*
=====================
RI_EndWorldRenderpass
=====================
*/
static qboolean
RI_EndWorldRenderpass( void )
{
	return true;
}

Q2_DLL_EXPORTED refexport_t
GetRefAPI(refimport_t imp)
{
	refexport_t re = {0};

	ri = imp;

	re.api_version = API_VERSION;
	re.framework_version = RI_GetSDLVersion();

	re.Init = RI_Init;
	re.Shutdown = RI_Shutdown;
	re.PrepareForWindow = RI_PrepareForWindow;
	re.InitContext = RI_InitContext;
	re.GetDrawableSize = RI_GetDrawableSize;
	re.ShutdownContext = RI_ShutdownContext;
	re.IsVSyncActive = RI_IsVSyncActive;
	re.BeginRegistration = RI_BeginRegistration;
	re.RegisterModel = RI_RegisterModel;
	re.RegisterSkin = RI_RegisterSkin;

	re.SetSky = RI_SetSky;
	re.EndRegistration = RI_EndRegistration;

	re.RenderFrame = RI_RenderFrame;

	re.DrawFindPic = RDraw_FindPic;

	re.DrawGetPicSize = RDraw_GetPicSize;
	//re.DrawPic = Draw_Pic;
	re.DrawPicScaled = RDraw_PicScaled;
	re.DrawStretchPic = RDraw_StretchPic;
	//re.DrawChar = Draw_Char;
	re.DrawCharScaled = RDraw_CharScaled;
	re.DrawTileClear = RDraw_TileClear;
	re.DrawFill = RDraw_Fill;
	re.DrawFadeScreen = RDraw_FadeScreen;

	re.DrawStretchRaw = RDraw_StretchRaw;

	re.SetPalette = RI_SetPalette;
	re.BeginFrame = RI_BeginFrame;
	re.EndWorldRenderpass = RI_EndWorldRenderpass;
	re.EndFrame = RI_EndFrame;

    // Tell the client that we're unsing the
	// new renderer restart API.
    ri.Vid_RequestRestart(RESTART_NO);

	return re;
}

#ifdef DEBUG
void
glCheckError_(const char *file, const char *function, int line)
{
	GLenum errorCode;
	const char * msg;

#define MY_ERROR_CASE(X) case X : msg = #X; break;

	while ((errorCode = glGetError()) != GL_NO_ERROR)
	{
		switch(errorCode)
		{
			MY_ERROR_CASE(GL_INVALID_ENUM);
			MY_ERROR_CASE(GL_INVALID_VALUE);
			MY_ERROR_CASE(GL_INVALID_OPERATION);
			MY_ERROR_CASE(GL_STACK_OVERFLOW);
			MY_ERROR_CASE(GL_STACK_UNDERFLOW);
			MY_ERROR_CASE(GL_OUT_OF_MEMORY);
			default: msg = "UNKNOWN";
		}
		Com_Printf("glError: %s in %s (%s, %d)\n", msg, function, file, line);
	}

#undef MY_ERROR_CASE

}
#endif
