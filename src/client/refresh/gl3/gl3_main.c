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
 * Refresher setup and main part of the frame generation, for OpenGL3
 *
 * =======================================================================
 */


#include "../../header/ref.h"
#include "header/local.h"

#define HANDMADE_MATH_IMPLEMENTATION
#include "header/HandmadeMath.h"

// TODO: put this in local.h ?
#define REF_VERSION "Yamagi Quake II OpenGL3 Refresher"

refimport_t ri;

gl3config_t gl3config;
gl3state_t gl3state;

unsigned gl3_rawpalette[256];

/* screen size info */
refdef_t gl3_newrefdef;

viddef_t vid;
gl3model_t *gl3_worldmodel;
gl3model_t *currentmodel;
entity_t *currententity;

float gl3depthmin=0.0f, gl3depthmax=1.0f;

cplane_t frustum[4];

/* view origin */
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t gl3_origin;

hmm_mat4 gl3_projectionMatrix; // eye cord -> clip coord
hmm_mat4 gl3_world_matrix; // the view matrix: world coord -> eye coord
// TODO: I don't know yet if we'll also need a model matrix (model coord -> world coord) here.


int gl3_visframecount; /* bumped when going to a new PVS */
int gl3_framecount; /* used for dlight push checking */

int c_brush_polys, c_alias_polys;

static float v_blend[4]; /* final blending color */

int gl3_viewcluster, gl3_viewcluster2, gl3_oldviewcluster, gl3_oldviewcluster2;

cvar_t *gl_msaa_samples;
cvar_t *gl_swapinterval;
cvar_t *gl_retexturing;
cvar_t *vid_fullscreen;
cvar_t *gl_mode;
cvar_t *gl_customwidth;
cvar_t *gl_customheight;
cvar_t *vid_gamma;
cvar_t *gl_anisotropic;
cvar_t *gl_texturemode;
cvar_t *gl_drawbuffer;
cvar_t *gl_clear;

cvar_t *gl_lefthand;
cvar_t *gl_farsee;

cvar_t *intensity;
cvar_t *gl_lightlevel;
cvar_t *gl_overbrightbits;

cvar_t *gl_norefresh;
cvar_t *gl_nolerp_list;
cvar_t *gl_nobind;
cvar_t *gl_lockpvs;
cvar_t *gl_novis;
cvar_t *gl_speeds;
cvar_t *gl_finish;

cvar_t *gl_cull;
cvar_t *gl_zfix;
cvar_t *gl_fullbright;
cvar_t *gl_flashblend;
cvar_t *gl_modulate;
cvar_t *gl_lightmap;
cvar_t *gl_shadows; // TODO: do we really need 2 cvars for shadows here?
cvar_t *gl_stencilshadow;

cvar_t *gl_dynamic;

cvar_t *gl3_debugcontext;

// Yaw-Pitch-Roll
// equivalent to R_z * R_y * R_x where R_x is the trans matrix for rotating around X axis for aroundXdeg
static hmm_mat4 rotAroundAxisZYX(float aroundZdeg, float aroundYdeg, float aroundXdeg)
{
#if 0
	// TODO: make sure this code is equivalent to the naive multiplications below

	// Naming of variables is consistent with http://planning.cs.uiuc.edu/node102.html
	// and https://de.wikipedia.org/wiki/Roll-Nick-Gier-Winkel#.E2.80.9EZY.E2.80.B2X.E2.80.B3-Konvention.E2.80.9C
	float alpha = HMM_ToRadians(aroundZdeg);
	float beta = HMM_ToRadians(aroundYdeg);
	float gamma = HMM_ToRadians(aroundXdeg);

	float sinA = HMM_SinF(alpha);
	float cosA = HMM_CosF(alpha);
	// TODO: or sincosf(alpha, &sinA, &cosA); ?? (not standard conform)
	float sinB = HMM_SinF(beta);
	float cosB = HMM_CosF(beta);
	float sinG = HMM_SinF(gamma);
	float cosG = HMM_CosF(gamma);

	hmm_mat4 ret = {{
		{ cosA*cosB,                  sinA*cosB,                   -sinB,    0 }, // first *column*
		{ cosA*sinB*sinG - sinA*cosG, sinA*sinB*sinG + cosA*cosG, cosB*sinG, 0 }, // cosA*sinB und cosA*sinG 2x genutzt
		{ cosA*sinB*cosG + sinA*sinG, sinA*sinB*cosG - cosA*sinG, cosB*cosG, 0 }, // sinA*sinB und sinA*cosG auch
		{  0,                          0,                          0,        1 }  // sinB*sinG und sinB*cosG auch
	}};

	return ret;
#else
	hmm_mat4 ret = HMM_Rotate(aroundZdeg, HMM_Vec3(0, 0, 1));
	ret = HMM_MultiplyMat4(ret, HMM_Rotate(aroundYdeg, HMM_Vec3(0, 1, 0)));
	ret = HMM_MultiplyMat4(ret, HMM_Rotate(aroundXdeg, HMM_Vec3(1, 0, 0)));

	return ret;
#endif
}

void
GL3_RotateForEntity(entity_t *e)
{
	STUB_ONCE("TODO: Implement for OpenGL3!");

	// angles: pitch (around y), yaw (around z), roll (around x)
	// rot matrices to be multiplied in order Z, Y, X (yaw, pitch, roll)
	hmm_mat4 rotMat = rotAroundAxisZYX(e->angles[1], -e->angles[0], -e->angles[2]);

#if 1
	hmm_mat4 transMat = HMM_Translate( HMM_Vec3(e->origin[0], e->origin[1], e->origin[2]) );
	hmm_mat4 modelViewMat = HMM_MultiplyMat4(gl3_world_matrix, transMat);
	modelViewMat = HMM_MultiplyMat4(modelViewMat, rotMat);
#else
	// TODO: I /think/ I could just set the translation in the end instead of multiplying above
	hmm_mat4 modelViewMat = HMM_MultiplyMat4(gl3_world_matrix, rotMat);
	for(int i=0; i<3; ++i)  modelViewMat.Elements[3][i] = e->origin[i];
#endif


	// TODO: set in shaders

#if 0
	glTranslatef(e->origin[0], e->origin[1], e->origin[2]);

	// angles: pitch (around y), yaw (around z), roll (around x)

	glRotatef(e->angles[1], 0, 0, 1); // yaw
	glRotatef(-e->angles[0], 0, 1, 0); // pitch
	glRotatef(-e->angles[2], 1, 0, 0); // roll
#endif // 0
}


static void
GL3_Strings(void)
{
	GLint i, numExtensions;
	R_Printf(PRINT_ALL, "GL_VENDOR: %s\n", gl3config.vendor_string);
	R_Printf(PRINT_ALL, "GL_RENDERER: %s\n", gl3config.renderer_string);
	R_Printf(PRINT_ALL, "GL_VERSION: %s\n", gl3config.version_string);
	R_Printf(PRINT_ALL, "GL_SHADING_LANGUAGE_VERSION: %s\n", gl3config.glsl_version_string);

	glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);

	R_Printf(PRINT_ALL, "GL_EXTENSIONS:");
	for(i = 0; i < numExtensions; i++)
	{
		R_Printf(PRINT_ALL, " %s", (const char*)glGetStringi(GL_EXTENSIONS, i));
	}
	R_Printf(PRINT_ALL, "\n");
}

static void
GL3_Register(void)
{
	gl_lefthand = ri.Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	gl_farsee = ri.Cvar_Get("gl_farsee", "0", CVAR_LATCH | CVAR_ARCHIVE);

	gl_drawbuffer = ri.Cvar_Get("gl_drawbuffer", "GL_BACK", 0);
	gl_swapinterval = ri.Cvar_Get("gl_swapinterval", "1", CVAR_ARCHIVE);
	gl_msaa_samples = ri.Cvar_Get ( "gl_msaa_samples", "0", CVAR_ARCHIVE );
	gl_retexturing = ri.Cvar_Get("gl_retexturing", "1", CVAR_ARCHIVE);
	gl3_debugcontext = ri.Cvar_Get("gl3_debugcontext", "0", 0);
	gl_mode = ri.Cvar_Get("gl_mode", "4", CVAR_ARCHIVE);
	gl_customwidth = ri.Cvar_Get("gl_customwidth", "1024", CVAR_ARCHIVE);
	gl_customheight = ri.Cvar_Get("gl_customheight", "768", CVAR_ARCHIVE);

	gl_norefresh = ri.Cvar_Get("gl_norefresh", "0", 0);
	gl_fullbright = ri.Cvar_Get("gl_fullbright", "0", 0);

	/* don't bilerp characters and crosshairs */
	gl_nolerp_list = ri.Cvar_Get("gl_nolerp_list", "pics/conchars.pcx pics/ch1.pcx pics/ch2.pcx pics/ch3.pcx", 0);
	gl_nobind = ri.Cvar_Get("gl_nobind", "0", 0);

	gl_texturemode = ri.Cvar_Get("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE);
	gl_anisotropic = ri.Cvar_Get("gl_anisotropic", "0", CVAR_ARCHIVE);

	vid_fullscreen = ri.Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = ri.Cvar_Get("vid_gamma", "1.0", CVAR_ARCHIVE);
	intensity = ri.Cvar_Get("intensity", "1.0", CVAR_ARCHIVE);

	gl_lightlevel = ri.Cvar_Get("gl_lightlevel", "0", 0);
	gl_overbrightbits = ri.Cvar_Get("gl_overbrightbits", "0", CVAR_ARCHIVE);

	gl_lightmap = ri.Cvar_Get("gl_lightmap", "0", 0);
	gl_shadows = ri.Cvar_Get("gl_shadows", "0", CVAR_ARCHIVE);
	gl_stencilshadow = ri.Cvar_Get("gl_stencilshadow", "0", CVAR_ARCHIVE);

	gl_flashblend = ri.Cvar_Get("gl_flashblend", "0", 0);
	gl_modulate = ri.Cvar_Get("gl_modulate", "1", CVAR_ARCHIVE);
	gl_zfix = ri.Cvar_Get("gl_zfix", "0", 0);
	gl_clear = ri.Cvar_Get("gl_clear", "0", 0);
	gl_cull = ri.Cvar_Get("gl_cull", "1", 0);
	gl_lockpvs = ri.Cvar_Get("gl_lockpvs", "0", 0);
	gl_novis = ri.Cvar_Get("gl_novis", "0", 0);
	gl_speeds = ri.Cvar_Get("gl_speeds", "0", 0);
	gl_finish = ri.Cvar_Get("gl_finish", "0", CVAR_ARCHIVE);

	gl_dynamic = ri.Cvar_Get("gl_dynamic", "1", 0);


#if 0 // TODO!
	//gl_lefthand = ri.Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	//gl_farsee = ri.Cvar_Get("gl_farsee", "0", CVAR_LATCH | CVAR_ARCHIVE);
	//gl_norefresh = ri.Cvar_Get("gl_norefresh", "0", 0);
	//gl_fullbright = ri.Cvar_Get("gl_fullbright", "0", 0);
	gl_drawentities = ri.Cvar_Get("gl_drawentities", "1", 0);
	gl_drawworld = ri.Cvar_Get("gl_drawworld", "1", 0);
	//gl_novis = ri.Cvar_Get("gl_novis", "0", 0);
	//gl_lerpmodels = ri.Cvar_Get("gl_lerpmodels", "1", 0); NOTE: screw this, it looks horrible without
	//gl_speeds = ri.Cvar_Get("gl_speeds", "0", 0);

	gl_lightlevel = ri.Cvar_Get("gl_lightlevel", "0", 0);
	gl_overbrightbits = ri.Cvar_Get("gl_overbrightbits", "0", CVAR_ARCHIVE);

	gl_particle_min_size = ri.Cvar_Get("gl_particle_min_size", "2", CVAR_ARCHIVE);
	gl_particle_max_size = ri.Cvar_Get("gl_particle_max_size", "40", CVAR_ARCHIVE);
	gl_particle_size = ri.Cvar_Get("gl_particle_size", "40", CVAR_ARCHIVE);
	gl_particle_att_a = ri.Cvar_Get("gl_particle_att_a", "0.01", CVAR_ARCHIVE);
	gl_particle_att_b = ri.Cvar_Get("gl_particle_att_b", "0.0", CVAR_ARCHIVE);
	gl_particle_att_c = ri.Cvar_Get("gl_particle_att_c", "0.01", CVAR_ARCHIVE);

	//gl_modulate = ri.Cvar_Get("gl_modulate", "1", CVAR_ARCHIVE);
	//gl_mode = ri.Cvar_Get("gl_mode", "4", CVAR_ARCHIVE);
	//gl_lightmap = ri.Cvar_Get("gl_lightmap", "0", 0);
	//gl_shadows = ri.Cvar_Get("gl_shadows", "0", CVAR_ARCHIVE);
	//gl_stencilshadow = ri.Cvar_Get("gl_stencilshadow", "0", CVAR_ARCHIVE);
	//gl_dynamic = ri.Cvar_Get("gl_dynamic", "1", 0);
	//gl_nobind = ri.Cvar_Get("gl_nobind", "0", 0);
	gl_round_down = ri.Cvar_Get("gl_round_down", "1", 0);
	gl_picmip = ri.Cvar_Get("gl_picmip", "0", 0);
	gl_showtris = ri.Cvar_Get("gl_showtris", "0", 0);
	//gl_ztrick = ri.Cvar_Get("gl_ztrick", "0", 0); NOTE: dump this.
	//gl_zfix = ri.Cvar_Get("gl_zfix", "0", 0);
	//gl_finish = ri.Cvar_Get("gl_finish", "0", CVAR_ARCHIVE);
	gl_clear = ri.Cvar_Get("gl_clear", "0", 0);
//	gl_cull = ri.Cvar_Get("gl_cull", "1", 0);
	gl_polyblend = ri.Cvar_Get("gl_polyblend", "1", 0);
	//gl_flashblend = ri.Cvar_Get("gl_flashblend", "0", 0);

	//gl_texturemode = ri.Cvar_Get("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE);
	gl_texturealphamode = ri.Cvar_Get("gl_texturealphamode", "default", CVAR_ARCHIVE);
	gl_texturesolidmode = ri.Cvar_Get("gl_texturesolidmode", "default", CVAR_ARCHIVE);
	//gl_anisotropic = ri.Cvar_Get("gl_anisotropic", "0", CVAR_ARCHIVE);
	//gl_lockpvs = ri.Cvar_Get("gl_lockpvs", "0", 0);

	//gl_palettedtexture = ri.Cvar_Get("gl_palettedtexture", "0", CVAR_ARCHIVE); NOPE.
	gl_pointparameters = ri.Cvar_Get("gl_pointparameters", "1", CVAR_ARCHIVE);

	//gl_drawbuffer = ri.Cvar_Get("gl_drawbuffer", "GL_BACK", 0);
	//gl_swapinterval = ri.Cvar_Get("gl_swapinterval", "1", CVAR_ARCHIVE);

	gl_saturatelighting = ri.Cvar_Get("gl_saturatelighting", "0", 0);

	//vid_fullscreen = ri.Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
	//vid_gamma = ri.Cvar_Get("vid_gamma", "1.0", CVAR_ARCHIVE);

	//gl_customwidth = ri.Cvar_Get("gl_customwidth", "1024", CVAR_ARCHIVE);
	//gl_customheight = ri.Cvar_Get("gl_customheight", "768", CVAR_ARCHIVE);
	//gl_msaa_samples = ri.Cvar_Get ( "gl_msaa_samples", "0", CVAR_ARCHIVE );

	//gl_retexturing = ri.Cvar_Get("gl_retexturing", "1", CVAR_ARCHIVE);


	gl_stereo = ri.Cvar_Get( "gl_stereo", "0", CVAR_ARCHIVE );
	gl_stereo_separation = ri.Cvar_Get( "gl_stereo_separation", "-0.4", CVAR_ARCHIVE );
	gl_stereo_anaglyph_colors = ri.Cvar_Get( "gl_stereo_anaglyph_colors", "rc", CVAR_ARCHIVE );
	gl_stereo_convergence = ri.Cvar_Get( "gl_stereo_convergence", "1", CVAR_ARCHIVE );
#endif // 0

	ri.Cmd_AddCommand("imagelist", GL3_ImageList_f);
	ri.Cmd_AddCommand("screenshot", GL3_ScreenShot);
	ri.Cmd_AddCommand("modellist", GL3_Mod_Modellist_f);
	ri.Cmd_AddCommand("gl_strings", GL3_Strings);
}

/*
 * Changes the video mode
 */

// the following is only used in the next to functions,
// no need to put it in a header
enum
{
	rserr_ok,

	rserr_invalid_fullscreen,
	rserr_invalid_mode,

	rserr_unknown
};

static int
SetMode_impl(int *pwidth, int *pheight, int mode, qboolean fullscreen)
{
	R_Printf(PRINT_ALL, "setting mode %d:", mode);

	/* mode -1 is not in the vid mode table - so we keep the values in pwidth
	   and pheight and don't even try to look up the mode info */
	if ((mode != -1) && !ri.Vid_GetModeInfo(pwidth, pheight, mode))
	{
		R_Printf(PRINT_ALL, " invalid mode\n");
		return rserr_invalid_mode;
	}

	R_Printf(PRINT_ALL, " %d %d\n", *pwidth, *pheight);

	if (!ri.GLimp_InitGraphics(fullscreen, pwidth, pheight))
	{
		return rserr_invalid_mode;
	}

	return rserr_ok;
}

static qboolean
GL3_SetMode(void)
{
	int err;
	qboolean fullscreen;

	fullscreen = vid_fullscreen->value;

	vid_fullscreen->modified = false;
	gl_mode->modified = false;

	/* a bit hackish approach to enable custom resolutions:
	   Glimp_SetMode needs these values set for mode -1 */
	vid.width = gl_customwidth->value;
	vid.height = gl_customheight->value;

	if ((err = SetMode_impl(&vid.width, &vid.height, gl_mode->value,
					 fullscreen)) == rserr_ok)
	{
		if (gl_mode->value == -1)
		{
			gl3state.prev_mode = 4; /* safe default for custom mode */
		}
		else
		{
			gl3state.prev_mode = gl_mode->value;
		}
	}
	else
	{
		if (err == rserr_invalid_fullscreen)
		{
			ri.Cvar_SetValue("vid_fullscreen", 0);
			vid_fullscreen->modified = false;
			R_Printf(PRINT_ALL, "ref_gl::R_SetMode() - fullscreen unavailable in this mode\n");

			if ((err = SetMode_impl(&vid.width, &vid.height, gl_mode->value, false)) == rserr_ok)
			{
				return true;
			}
		}
		else if (err == rserr_invalid_mode)
		{
			ri.Cvar_SetValue("gl_mode", gl3state.prev_mode);
			gl_mode->modified = false;
			R_Printf(PRINT_ALL, "ref_gl::R_SetMode() - invalid mode\n");
		}

		/* try setting it back to something safe */
		if ((err = SetMode_impl(&vid.width, &vid.height, gl3state.prev_mode, false)) != rserr_ok)
		{
			R_Printf(PRINT_ALL, "ref_gl::R_SetMode() - could not revert to safe mode\n");
			return false;
		}
	}

	return true;
}

static qboolean
GL3_Init(void)
{
	int j;
	extern float gl3_turbsin[256];
	for (j = 0; j < 256; j++)
	{
		gl3_turbsin[j] *= 0.5;
	}

	Swap_Init(); // FIXME: for fucks sake, this doesn't have to be done at runtime!

	/* Options */
	R_Printf(PRINT_ALL, "Refresher build options:\n");

	R_Printf(PRINT_ALL, " + Retexturing support\n");

	R_Printf(PRINT_ALL, "Refresh: " REF_VERSION "\n");

	GL3_Draw_GetPalette();

	GL3_Register();

	/* initialize our QGL dynamic bindings */
	//QGL_Init();

	/* initialize OS-specific parts of OpenGL */
	if (!ri.GLimp_Init())
	{
		//QGL_Shutdown();
		return false;
	}

	/* set our "safe" mode */
	gl3state.prev_mode = 4;
	//gl_state.stereo_mode = gl_stereo->value;

	/* create the window and set up the context */
	if (!GL3_SetMode())
	{
		//QGL_Shutdown();
		R_Printf(PRINT_ALL, "ref_gl3::R_Init() - could not R_SetMode()\n");
		return false;
	}

	ri.Vid_MenuInit();

	/* get our various GL strings */
	gl3config.vendor_string = (const char*)glGetString(GL_VENDOR);
	gl3config.renderer_string = (const char*)glGetString(GL_RENDERER);
	gl3config.version_string = (const char*)glGetString(GL_VERSION);
	gl3config.glsl_version_string = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);

	R_Printf(PRINT_ALL, "\nOpenGL setting:\n");
	GL3_Strings();

	/*
	if (gl_config.major_version < 3)
	{
		// if (gl_config.major_version == 3 && gl_config.minor_version < 2)
		{
			QGL_Shutdown();
			R_Printf(PRINT_ALL, "Support for OpenGL 3.2 is not available\n");

			return false;
		}
	}
	*/

	R_Printf(PRINT_ALL, "\n\nProbing for OpenGL extensions:\n");


	/* Anisotropic */
	R_Printf(PRINT_ALL, " - Anisotropic Filtering: ");

	if(gl3config.anisotropic)
	{
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl3config.max_anisotropy);

		R_Printf(PRINT_ALL, "Max level: %ux\n", (int)gl3config.max_anisotropy);
	}
	else
	{
		gl3config.max_anisotropy = 0.0;

		R_Printf(PRINT_ALL, "Not supported\n");
	}

	if(gl3config.debug_output)
	{
		R_Printf(PRINT_ALL, " - OpenGL Debug Output: Supported ");
		if(gl3_debugcontext->value == 0.0f)
		{
			R_Printf(PRINT_ALL, "(but disabled with gl3_debugcontext = 0)\n");
		}
		else
		{
			R_Printf(PRINT_ALL, "and enabled with gl3_debugcontext = %i\n", (int)gl3_debugcontext->value);
		}
	}
	else
	{
		R_Printf(PRINT_ALL, " - OpenGL Debug Output: Not Supported\n");
	}


	GL3_SetDefaultState();

	if(GL3_InitShaders())
	{
		R_Printf(PRINT_ALL, "Loading shaders succeeded!\n");
	}
	else
	{
		R_Printf(PRINT_ALL, "Loading shaders failed!\n");
		return false;
	}

	registration_sequence = 1; // from R_InitImages() (everything else from there shouldn't be needed anymore)

	GL3_Mod_Init();

	GL3_InitParticleTexture();

	GL3_Draw_InitLocal();

	GL3_SurfInit();

	R_Printf(PRINT_ALL, "\n");
	return true;
}

void
GL3_Shutdown(void)
{
	ri.Cmd_RemoveCommand("modellist");
	ri.Cmd_RemoveCommand("screenshot");
	ri.Cmd_RemoveCommand("imagelist");
	ri.Cmd_RemoveCommand("gl_strings");

	GL3_Mod_FreeAll();
	GL3_ShutdownImages();
	GL3_SurfShutdown();
	GL3_Draw_ShutdownLocal();
	GL3_ShutdownShaders();

	/* shutdown OS specific OpenGL stuff like contexts, etc.  */
	GL3_ShutdownWindow(false);
}

static void
GL3_DrawEntitiesOnList(void)
{
	STUB_ONCE("TODO: Implement!");
#if 0
	int i;
/*
	if (!gl_drawentities->value)
	{
		return;
	}
*/

	/* draw non-transparent first */
	for (i = 0; i < gl3_newrefdef.num_entities; i++)
	{
		currententity = &gl3_newrefdef.entities[i];

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
			currentmodel = currententity->model;

			if (!currentmodel)
			{
				R_DrawNullModel();
				continue;
			}

			switch (currentmodel->type)
			{
				case mod_alias:
					GL3_DrawAliasModel(currententity);
					break;
				case mod_brush:
					R_DrawBrushModel(currententity);
					break;
				case mod_sprite:
					R_DrawSpriteModel(currententity);
					break;
				default:
					ri.Sys_Error(ERR_DROP, "Bad modeltype");
					break;
			}
		}
	}

	/* draw transparent entities
	   we could sort these if it ever
	   becomes a problem... */
	glDepthMask(0);

	for (i = 0; i < gl3_newrefdef.num_entities; i++)
	{
		currententity = &gl3_newrefdef.entities[i];

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
			currentmodel = currententity->model;

			if (!currentmodel)
			{
				R_DrawNullModel();
				continue;
			}

			switch (currentmodel->type)
			{
				case mod_alias:
					R_DrawAliasModel(currententity);
					break;
				case mod_brush:
					R_DrawBrushModel(currententity);
					break;
				case mod_sprite:
					R_DrawSpriteModel(currententity);
					break;
				default:
					ri.Sys_Error(ERR_DROP, "Bad modeltype");
					break;
			}
		}
	}

	glDepthMask(1); /* back to writing */
#endif // 0
}

static int
SignbitsForPlane(cplane_t *out)
{
	int bits, j;

	/* for fast box on planeside test */
	bits = 0;

	for (j = 0; j < 3; j++)
	{
		if (out->normal[j] < 0)
		{
			bits |= 1 << j;
		}
	}

	return bits;
}

static void
SetFrustum(void)
{
	int i;

	/* rotate VPN right by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[0].normal, vup, vpn,
			-(90 - gl3_newrefdef.fov_x / 2));
	/* rotate VPN left by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[1].normal,
			vup, vpn, 90 - gl3_newrefdef.fov_x / 2);
	/* rotate VPN up by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[2].normal,
			vright, vpn, 90 - gl3_newrefdef.fov_y / 2);
	/* rotate VPN down by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[3].normal, vright, vpn,
			-(90 - gl3_newrefdef.fov_y / 2));

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct(gl3_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane(&frustum[i]);
	}
}

static void
SetupFrame(void)
{
	int i;
	mleaf_t *leaf;

	gl3_framecount++;

	/* build the transformation matrix for the given view angles */
	VectorCopy(gl3_newrefdef.vieworg, gl3_origin);

	AngleVectors(gl3_newrefdef.viewangles, vpn, vright, vup);

	/* current viewcluster */
	if (!(gl3_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		gl3_oldviewcluster = gl3_viewcluster;
		gl3_oldviewcluster2 = gl3_viewcluster2;
		leaf = GL3_Mod_PointInLeaf(gl3_origin, gl3_worldmodel);
		gl3_viewcluster = gl3_viewcluster2 = leaf->cluster;

		/* check above and below so crossing solid water doesn't draw wrong */
		if (!leaf->contents)
		{
			/* look down a bit */
			vec3_t temp;

			VectorCopy(gl3_origin, temp);
			temp[2] -= 16;
			leaf = GL3_Mod_PointInLeaf(temp, gl3_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != gl3_viewcluster2))
			{
				gl3_viewcluster2 = leaf->cluster;
			}
		}
		else
		{
			/* look up a bit */
			vec3_t temp;

			VectorCopy(gl3_origin, temp);
			temp[2] += 16;
			leaf = GL3_Mod_PointInLeaf(temp, gl3_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != gl3_viewcluster2))
			{
				gl3_viewcluster2 = leaf->cluster;
			}
		}
	}

	for (i = 0; i < 4; i++)
	{
		v_blend[i] = gl3_newrefdef.blend[i];
	}

	c_brush_polys = 0;
	c_alias_polys = 0;

	/* clear out the portion of the screen that the NOWORLDMODEL defines */
	if (gl3_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		glEnable(GL_SCISSOR_TEST);
		glClearColor(0.3, 0.3, 0.3, 1);
		glScissor(gl3_newrefdef.x,
				vid.height - gl3_newrefdef.height - gl3_newrefdef.y,
				gl3_newrefdef.width, gl3_newrefdef.height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(1, 0, 0.5, 0.5);
		glDisable(GL_SCISSOR_TEST);
	}
}

static void
GL3_SetGL2D(void)
{
	int x = 0;
	int w = vid.width;
	int y = 0;
	int h = vid.height;

#if 0 // TODO: stereo
	/* set 2D virtual screen size */
	qboolean drawing_left_eye = gl_state.camera_separation < 0;
	qboolean stereo_split_tb = ((gl_state.stereo_mode == STEREO_SPLIT_VERTICAL) && gl_state.camera_separation);
	qboolean stereo_split_lr = ((gl_state.stereo_mode == STEREO_SPLIT_HORIZONTAL) && gl_state.camera_separation);

	if(stereo_split_lr) {
		w =  w / 2;
		x = drawing_left_eye ? 0 : w;
	}

	if(stereo_split_tb) {
		h =  h / 2;
		y = drawing_left_eye ? h : 0;
	}
#endif // 0

	glViewport(x, y, w, h);

	hmm_mat4 transMatr = HMM_Orthographic(0, vid.width, vid.height, 0, -99999, 99999);

	/*
	glUseProgram(gl3state.si2Dcolor.shaderProgram);
	glUniformMatrix4fv(gl3state.si2Dcolor.uniProjMatrix , 1, GL_FALSE, transMatr.Elements[0]);
	glUseProgram(gl3state.si2D.shaderProgram);
	glUniformMatrix4fv(gl3state.si2D.uniProjMatrix , 1, GL_FALSE, transMatr.Elements[0]);
	*/

	//memcpy(gl3state.uni2DData.transMat4, transMatr.Elements, 4*4*sizeof(GLfloat));
	for(int i=0; i<4; ++i)
		for(int j=0; j<4; ++j)
			gl3state.uni2DData.transMat4[i][j] = transMatr.Elements[i][j];

	GL3_UpdateUBO2D();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	// glColor4f(1, 1, 1, 1); // FIXME: change to GL3 code!
}

// equivalent to R_x * R_y * R_z where R_x is the trans matrix for rotating around X axis for aroundXdeg
static hmm_mat4 rotAroundAxisXYZ(float aroundXdeg, float aroundYdeg, float aroundZdeg)
{
	float alpha = HMM_ToRadians(aroundXdeg);
	float beta = HMM_ToRadians(aroundYdeg);
	float gamma = HMM_ToRadians(aroundZdeg);

	float sinA = HMM_SinF(alpha);
	float cosA = HMM_CosF(alpha);
	float sinB = HMM_SinF(beta);
	float cosB = HMM_CosF(beta);
	float sinG = HMM_SinF(gamma);
	float cosG = HMM_CosF(gamma);

	hmm_mat4 ret = {{
		{  cosB*cosG,  sinA*sinB*cosG + cosA*sinG, -cosA*sinB*cosG + sinA*sinG, 0 }, // first *column*
		{ -cosB*sinG, -sinA*sinB*sinG + cosA*cosG,  cosA*sinB*sinG + sinA*cosG, 0 },
		{  sinB,      -sinA*cosB,                   cosA*cosB,                  0 },
		{  0,          0,                           0,                          1 }
	}};

	return ret;
}

static void
SetupGL(void)
{
	int x, x2, y2, y, w, h;

	/* set up viewport */
	x = floor(gl3_newrefdef.x * vid.width / vid.width);
	x2 = ceil((gl3_newrefdef.x + gl3_newrefdef.width) * vid.width / vid.width);
	y = floor(vid.height - gl3_newrefdef.y * vid.height / vid.height);
	y2 = ceil(vid.height - (gl3_newrefdef.y + gl3_newrefdef.height) * vid.height / vid.height);

	w = x2 - x;
	h = y - y2;

#if 0 // TODO: stereo stuff
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
#endif // 0

	glViewport(x, y2, w, h);

	/* set up projection matrix (eye coordinates -> clip coordinates) */
	{
		float screenaspect = (float)gl3_newrefdef.width / gl3_newrefdef.height;
		float dist = (gl_farsee->value == 0) ? 4096.0f : 8192.0f;
		gl3_projectionMatrix = HMM_Perspective(gl3_newrefdef.fov_y, screenaspect, 4, dist);
	}

#if 0
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	R_MYgluPerspective(gl3_newrefdef.fov_y, screenaspect, 4, dist);
#endif // 0

	glCullFace(GL_FRONT);

	/* set up view matrix (world coordinates -> eye coordinates) */
	{
		// first put Z axis going up
		hmm_mat4 viewMat = {{
			{  0, 0, -1, 0 }, // first *column* (the matrix is colum-major)
			{ -1, 0,  0, 0 },
			{  0, 1,  0, 0 },
			{  0, 0,  0, 1 }
		}};

		// now rotate by view angles
		hmm_mat4 rotMat = rotAroundAxisXYZ(-gl3_newrefdef.viewangles[2], -gl3_newrefdef.viewangles[0], -gl3_newrefdef.viewangles[1]);

		viewMat = HMM_MultiplyMat4( viewMat, rotMat );

		// .. and apply translation for current position
		hmm_vec3 trans = HMM_Vec3(-gl3_newrefdef.vieworg[0], -gl3_newrefdef.vieworg[1], -gl3_newrefdef.vieworg[2]);
		viewMat = HMM_MultiplyMat4( viewMat, HMM_Translate(trans) );

		gl3_world_matrix = viewMat;
	}

	memcpy(gl3state.uni3DData.transProjMat4, gl3_projectionMatrix.Elements, sizeof(gl3state.uni3DData.transProjMat4));
	memcpy(gl3state.uni3DData.transModelViewMat4, gl3_world_matrix.Elements, sizeof(gl3state.uni3DData.transProjMat4));

	gl3state.uni3DData.time = gl3_newrefdef.time;

	GL3_UpdateUBO3D();

#if 0
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glRotatef(-90, 1, 0, 0); /* put Z going up */
	glRotatef(90, 0, 0, 1); /* put Z going up */
	glRotatef(-gl3_newrefdef.viewangles[2], 1, 0, 0);
	glRotatef(-gl3_newrefdef.viewangles[0], 0, 1, 0);
	glRotatef(-gl3_newrefdef.viewangles[1], 0, 0, 1);
	glTranslatef(-gl3_newrefdef.vieworg[0], -gl3_newrefdef.vieworg[1],
			-gl3_newrefdef.vieworg[2]);

	glGetFloatv(GL_MODELVIEW_MATRIX, r_world_matrix);
#endif // 0

	/* set drawing parms */
	if (gl_cull->value)
	{
		glEnable(GL_CULL_FACE);
	}
	else
	{
		glDisable(GL_CULL_FACE);
	}

	STUB_ONCE("Should I do anything about disabling GL_BLEND and GL_ALPHA_TEST?");
	//glDisable(GL_BLEND);
	//glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}

/*
 * gl3_newrefdef must be set before the first call
 */
static void
GL3_RenderView(refdef_t *fd)
{
#if 0 // TODO: keep stereo stuff?
	if ((gl_state.stereo_mode != STEREO_MODE_NONE) && gl_state.camera_separation) {

		qboolean drawing_left_eye = gl_state.camera_separation < 0;
		switch (gl_state.stereo_mode) {
			case STEREO_MODE_ANAGLYPH:
				{

					// Work out the colour for each eye.
					int anaglyph_colours[] = { 0x4, 0x3 }; // Left = red, right = cyan.

					if (strlen(gl_stereo_anaglyph_colors->string) == 2) {
						int eye, colour, missing_bits;
						// Decode the colour name from its character.
						for (eye = 0; eye < 2; ++eye) {
							colour = 0;
							switch (toupper(gl_stereo_anaglyph_colors->string[eye])) {
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

					//GLimp_GetClientAreaOffset(&client_x, &client_y);
					client_x = 0;
					client_y = 0;

					GL3_SetGL2D();

					glEnable(GL_STENCIL_TEST);
					glStencilMask(GL_TRUE);
					glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

					glStencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);
					glStencilFunc(GL_NEVER, 0, 1);

					glBegin(GL_QUADS);
					{
						glVertex2i(0, 0);
						glVertex2i(vid.width, 0);
						glVertex2i(vid.width, vid.height);
						glVertex2i(0, vid.height);
					}
					glEnd();

					glStencilOp(GL_INVERT, GL_KEEP, GL_KEEP);
					glStencilFunc(GL_NEVER, 1, 1);

					glBegin(GL_LINES);
					{
						if (gl_state.stereo_mode == STEREO_MODE_ROW_INTERLEAVED || gl_state.stereo_mode == STEREO_MODE_PIXEL_INTERLEAVED) {
							int y;
							for (y = 0; y <= vid.height; y += 2) {
								glVertex2f(0, y - 0.5f);
								glVertex2f(vid.width, y - 0.5f);
							}
							flip_eyes ^= (client_y & 1);
						}

						if (gl_state.stereo_mode == STEREO_MODE_COLUMN_INTERLEAVED || gl_state.stereo_mode == STEREO_MODE_PIXEL_INTERLEAVED) {
							int x;
							for (x = 0; x <= vid.width; x += 2) {
								glVertex2f(x - 0.5f, 0);
								glVertex2f(x - 0.5f, vid.height);
							}
							flip_eyes ^= (client_x & 1);
						}
					}
					glEnd();

					glStencilMask(GL_FALSE);
					glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

					glStencilFunc(GL_EQUAL, drawing_left_eye ^ flip_eyes, 1);
					glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
				}
				break;
			default:
				break;
		}
	}
#endif // 0 (stereo stuff)

	if (gl_norefresh->value)
	{
		return;
	}

	gl3_newrefdef = *fd;

	STUB_ONCE("TODO: Implement!");


	if (!gl3_worldmodel && !(gl3_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		ri.Sys_Error(ERR_DROP, "R_RenderView: NULL worldmodel");
	}


	if (gl_speeds->value)
	{
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	GL3_PushDlights();

	if (gl_finish->value)
	{
		glFinish();
	}

	SetupFrame();

	SetFrustum();

	SetupGL();

	GL3_MarkLeaves(); /* done here so we know if we're in water */

	GL3_DrawWorld();
#if 0 // TODO !!
	R_DrawEntitiesOnList();

	GL3_RenderDlights();

	R_DrawParticles();
#endif // 0
	GL3_DrawAlphaSurfaces();
#if 0
	R_Flash();

	if (gl_speeds->value)
	{
		R_Printf(PRINT_ALL, "%4i wpoly %4i epoly %i tex %i lmaps\n",
				c_brush_polys, c_alias_polys, c_visible_textures,
				c_visible_lightmaps);
	}

#endif // 0

#if 0 // TODO: stereo stuff
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
#endif // 0
}

#if 0 // TODO: stereo
enum opengl_special_buffer_modes
GL3_GetSpecialBufferModeForStereoMode(enum stereo_modes stereo_mode) {
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
#endif // 0

static void
GL3_SetLightLevel(void)
{
	vec3_t shadelight = {0};

	if (gl3_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		return;
	}

	/* save off light value for server to look at */
	GL3_LightPoint(gl3_newrefdef.vieworg, shadelight);

	/* pick the greatest component, which should be the
	 * same as the mono value returned by software */
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


static void
GL3_RenderFrame(refdef_t *fd)
{
	GL3_RenderView(fd);
	GL3_SetLightLevel();
	GL3_SetGL2D();
}


static void
GL3_Clear(void)
{
	// Check whether the stencil buffer needs clearing, and do so if need be.
	GLbitfield stencilFlags = 0;
#if 0 // TODO: stereo stuff
	if (gl3state.stereo_mode >= STEREO_MODE_ROW_INTERLEAVED && gl_state.stereo_mode <= STEREO_MODE_PIXEL_INTERLEAVED) {
		glClearStencil(0);
		stencilFlags |= GL_STENCIL_BUFFER_BIT;
	}
#endif // 0


	if (gl_clear->value)
	{
		glClear(GL_COLOR_BUFFER_BIT | stencilFlags | GL_DEPTH_BUFFER_BIT);
	}
	else
	{
		glClear(GL_DEPTH_BUFFER_BIT | stencilFlags);
	}

	gl3depthmin = 0;
	gl3depthmax = 1;
	glDepthFunc(GL_LEQUAL);

	glDepthRange(gl3depthmin, gl3depthmax);

	if (gl_zfix->value)
	{
		if (gl3depthmax > gl3depthmin)
		{
			glPolygonOffset(0.05, 1);
		}
		else
		{
			glPolygonOffset(-0.05, -1);
		}
	}

	/* stencilbuffer shadows */
	if (gl_shadows->value && have_stencil && gl_stencilshadow->value)
	{
		glClearStencil(1);
		glClear(GL_STENCIL_BUFFER_BIT);
	}

	glClear(GL_COLOR_BUFFER_BIT); // TODO: I added this and the next line - keep?
	glClearColor(0.5, 0.5, 1, 0.5); // FIXME: this would prolly look better with black.
}

void
GL3_BeginFrame(float camera_separation)
{
	/* change modes if necessary */
	if (gl_mode->modified)
	{
		vid_fullscreen->modified = true;
	}

#if 0 // TODO: stereo stuff
	gl_state.camera_separation = camera_separation;
	// force a vid_restart if gl_stereo has been modified.
	if ( gl_state.stereo_mode != gl_stereo->value ) {
		// If we've gone from one mode to another with the same special buffer requirements there's no need to restart.
		if ( GL_GetSpecialBufferModeForStereoMode( gl_state.stereo_mode ) == GL_GetSpecialBufferModeForStereoMode( gl_stereo->value )  ) {
			gl_state.stereo_mode = gl_stereo->value;
		}
		else
		{
			R_Printf(PRINT_ALL, "stereo supermode changed, restarting video!\n");
			vid_fullscreen->modified = true;
		}
	}
#endif // 0

	if (vid_gamma->modified || intensity->modified)
	{
		vid_gamma->modified = false;
		intensity->modified = false;

		gl3state.uniCommonData.gamma = 1.0f/vid_gamma->value;
		gl3state.uniCommonData.intensity = intensity->value;
		GL3_UpdateUBOCommon();
	}

	// Clamp overbrightbits
	if (gl_overbrightbits->modified)
	{
		if (gl_overbrightbits->value > 2 && gl_overbrightbits->value < 4)
		{
			ri.Cvar_Set("gl_overbrightbits", "2");
		}
		else if (gl_overbrightbits->value > 4)
		{
			ri.Cvar_Set("gl_overbrightbits", "4");
		}

		gl_overbrightbits->modified = false;
	}

	/* go into 2D mode */

	GL3_SetGL2D();

	/* draw buffer stuff */
	if (gl_drawbuffer->modified)
	{
		gl_drawbuffer->modified = false;

		// TODO: stereo stuff
		//if ((gl3state.camera_separation == 0) || gl3state.stereo_mode != STEREO_MODE_OPENGL)
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
	}

	/* texturemode stuff */
	if (gl_texturemode->modified || (gl3config.anisotropic && gl_anisotropic->modified))
	{
		GL3_TextureMode(gl_texturemode->string);
		gl_texturemode->modified = false;
		gl_anisotropic->modified = false;
	}

	STUB_ONCE("TODO: texture-alpha/solid-mode stuff??")
#if 0
	if (gl_texturealphamode->modified)
	{
		R_TextureAlphaMode(gl_texturealphamode->string);
		gl_texturealphamode->modified = false;
	}

	if (gl_texturesolidmode->modified)
	{
		R_TextureSolidMode(gl_texturesolidmode->string);
		gl_texturesolidmode->modified = false;
	}
#endif // 0

	/* clear screen if desired */
	GL3_Clear();
}

static void
GL3_SetPalette(const unsigned char *palette)
{
	int i;
	byte *rp = (byte *)gl3_rawpalette;

	if (palette)
	{
		for (i = 0; i < 256; i++)
		{
			rp[i * 4 + 0] = palette[i * 3 + 0];
			rp[i * 4 + 1] = palette[i * 3 + 1];
			rp[i * 4 + 2] = palette[i * 3 + 2];
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

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(1, 0, 0.5, 0.5);
}

Q2_DLL_EXPORTED refexport_t
GetRefAPI(refimport_t imp)
{
	refexport_t re = {0};

	ri = imp;

	re.api_version = API_VERSION;

	re.Init = GL3_Init;
	re.Shutdown = GL3_Shutdown;
	re.PrepareForWindow = GL3_PrepareForWindow;
	re.InitContext = GL3_InitContext;
	re.ShutdownWindow = GL3_ShutdownWindow;

	re.BeginRegistration = GL3_BeginRegistration;
	re.RegisterModel = GL3_RegisterModel;
	re.RegisterSkin = GL3_RegisterSkin;

	re.SetSky = GL3_SetSky;
	re.EndRegistration = GL3_EndRegistration;

	re.RenderFrame = GL3_RenderFrame;

	re.DrawFindPic = GL3_Draw_FindPic;
	re.DrawGetPicSize = GL3_Draw_GetPicSize;

	re.DrawPicScaled = GL3_Draw_PicScaled;
	re.DrawStretchPic = GL3_Draw_StretchPic;

	re.DrawCharScaled = GL3_Draw_CharScaled;
	re.DrawTileClear = GL3_Draw_TileClear;
	re.DrawFill = GL3_Draw_Fill;
	re.DrawFadeScreen = GL3_Draw_FadeScreen;

	re.DrawStretchRaw = GL3_Draw_StretchRaw;
	re.SetPalette = GL3_SetPalette;

	re.BeginFrame = GL3_BeginFrame;
	re.EndFrame = GL3_EndFrame;

	return re;
}

void R_Printf(int level, const char* msg, ...)
{
	va_list argptr;
	va_start(argptr, msg);
	ri.Com_VPrintf(level, msg, argptr);
	va_end(argptr);
}

/*
 * this is only here so the functions in shared source files
 * (shared.c, rand.c, flash.c, mem.c/hunk.c) can link
 */
void
Sys_Error(char *error, ...)
{
	va_list argptr;
	char text[4096]; // MAXPRINTMSG == 4096

	va_start(argptr, error);
	vsprintf(text, error, argptr);
	va_end(argptr);

	ri.Sys_Error(ERR_FATAL, "%s", text);
}

void
Com_Printf(char *msg, ...)
{
	va_list argptr;
	va_start(argptr, msg);
	ri.Com_VPrintf(PRINT_ALL, msg, argptr);
	va_end(argptr);
}
