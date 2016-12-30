/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016 Daniel Gibson
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

// TODO: put this in local.h ?
#define REF_VERSION "Yamagi Quake II OpenGL3 Refresher"

refimport_t ri;

gl3config_t gl3config;
gl3state_t gl3state;

viddef_t vid;

gl3image_t *gl3_notexture; /* use for bad textures */
gl3image_t *gl3_particletexture; /* little dot for particles */

cvar_t *gl_msaa_samples;
cvar_t *gl_swapinterval;
cvar_t *gl_retexturing;
cvar_t *vid_fullscreen;
cvar_t *gl_mode;
cvar_t *gl_customwidth;
cvar_t *gl_customheight;
cvar_t *vid_gamma;
cvar_t *gl_anisotropic;

cvar_t *gl_nolerp_list;

cvar_t *gl3_debugcontext;

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
	gl_swapinterval = ri.Cvar_Get("gl_swapinterval", "1", CVAR_ARCHIVE);
	gl_msaa_samples = ri.Cvar_Get ( "gl_msaa_samples", "0", CVAR_ARCHIVE );
	gl_retexturing = ri.Cvar_Get("gl_retexturing", "1", CVAR_ARCHIVE);
	gl3_debugcontext = ri.Cvar_Get("gl3_debugcontext", "0", 0);
	gl_mode = ri.Cvar_Get("gl_mode", "4", CVAR_ARCHIVE);
	gl_customwidth = ri.Cvar_Get("gl_customwidth", "1024", CVAR_ARCHIVE);
	gl_customheight = ri.Cvar_Get("gl_customheight", "768", CVAR_ARCHIVE);

	/* don't bilerp characters and crosshairs */
	gl_nolerp_list = ri.Cvar_Get("gl_nolerp_list", "pics/conchars.pcx pics/ch1.pcx pics/ch2.pcx pics/ch3.pcx", 0);

	gl_anisotropic = ri.Cvar_Get("gl_anisotropic", "0", CVAR_ARCHIVE);

	vid_fullscreen = ri.Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = ri.Cvar_Get("vid_gamma", "1.0", CVAR_ARCHIVE);


#if 0 // TODO!
	gl_lefthand = ri.Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	gl_farsee = ri.Cvar_Get("gl_farsee", "0", CVAR_LATCH | CVAR_ARCHIVE);
	gl_norefresh = ri.Cvar_Get("gl_norefresh", "0", 0);
	gl_fullbright = ri.Cvar_Get("gl_fullbright", "0", 0);
	gl_drawentities = ri.Cvar_Get("gl_drawentities", "1", 0);
	gl_drawworld = ri.Cvar_Get("gl_drawworld", "1", 0);
	gl_novis = ri.Cvar_Get("gl_novis", "0", 0);
	gl_lerpmodels = ri.Cvar_Get("gl_lerpmodels", "1", 0);
	gl_speeds = ri.Cvar_Get("gl_speeds", "0", 0);

	gl_lightlevel = ri.Cvar_Get("gl_lightlevel", "0", 0);
	gl_overbrightbits = ri.Cvar_Get("gl_overbrightbits", "0", CVAR_ARCHIVE);

	gl_particle_min_size = ri.Cvar_Get("gl_particle_min_size", "2", CVAR_ARCHIVE);
	gl_particle_max_size = ri.Cvar_Get("gl_particle_max_size", "40", CVAR_ARCHIVE);
	gl_particle_size = ri.Cvar_Get("gl_particle_size", "40", CVAR_ARCHIVE);
	gl_particle_att_a = ri.Cvar_Get("gl_particle_att_a", "0.01", CVAR_ARCHIVE);
	gl_particle_att_b = ri.Cvar_Get("gl_particle_att_b", "0.0", CVAR_ARCHIVE);
	gl_particle_att_c = ri.Cvar_Get("gl_particle_att_c", "0.01", CVAR_ARCHIVE);

	gl_modulate = ri.Cvar_Get("gl_modulate", "1", CVAR_ARCHIVE);
	//gl_mode = ri.Cvar_Get("gl_mode", "4", CVAR_ARCHIVE);
	gl_lightmap = ri.Cvar_Get("gl_lightmap", "0", 0);
	gl_shadows = ri.Cvar_Get("gl_shadows", "0", CVAR_ARCHIVE);
	gl_stencilshadow = ri.Cvar_Get("gl_stencilshadow", "0", CVAR_ARCHIVE);
	gl_dynamic = ri.Cvar_Get("gl_dynamic", "1", 0);
	gl_nobind = ri.Cvar_Get("gl_nobind", "0", 0);
	gl_round_down = ri.Cvar_Get("gl_round_down", "1", 0);
	gl_picmip = ri.Cvar_Get("gl_picmip", "0", 0);
	gl_showtris = ri.Cvar_Get("gl_showtris", "0", 0);
	gl_ztrick = ri.Cvar_Get("gl_ztrick", "0", 0);
	gl_zfix = ri.Cvar_Get("gl_zfix", "0", 0);
	gl_finish = ri.Cvar_Get("gl_finish", "0", CVAR_ARCHIVE);
	gl_clear = ri.Cvar_Get("gl_clear", "0", 0);
	gl_cull = ri.Cvar_Get("gl_cull", "1", 0);
	gl_polyblend = ri.Cvar_Get("gl_polyblend", "1", 0);
	gl_flashblend = ri.Cvar_Get("gl_flashblend", "0", 0);

	gl_texturemode = ri.Cvar_Get("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE);
	gl_texturealphamode = ri.Cvar_Get("gl_texturealphamode", "default", CVAR_ARCHIVE);
	gl_texturesolidmode = ri.Cvar_Get("gl_texturesolidmode", "default", CVAR_ARCHIVE);
	//gl_anisotropic = ri.Cvar_Get("gl_anisotropic", "0", CVAR_ARCHIVE);
	gl_lockpvs = ri.Cvar_Get("gl_lockpvs", "0", 0);

	gl_palettedtexture = ri.Cvar_Get("gl_palettedtexture", "0", CVAR_ARCHIVE);
	gl_pointparameters = ri.Cvar_Get("gl_pointparameters", "1", CVAR_ARCHIVE);

	gl_drawbuffer = ri.Cvar_Get("gl_drawbuffer", "GL_BACK", 0);
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
	/* TODO!
	int j;
	extern float r_turbsin[256];
	for (j = 0; j < 256; j++)
	{
		r_turbsin[j] *= 0.5;
	}*/

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

	STUB("TODO: Some intensity and gamma stuff that was in R_InitImages()");

#if 0
	//R_InitImages(); - most of the things in R_InitImages() shouldn't be needed anymore
	Mod_Init();
	R_InitParticleTexture();
#endif // 0

	GL3_Draw_InitLocal();

	R_Printf(PRINT_ALL, "\n");
	return true; // -1 is error, other values don't matter. FIXME: mabe make this return bool..

	return false;
}

void
GL3_Shutdown(void)
{

	ri.Cmd_RemoveCommand("modellist");
	ri.Cmd_RemoveCommand("screenshot");
	ri.Cmd_RemoveCommand("imagelist");
	ri.Cmd_RemoveCommand("gl_strings");
#if 0 // TODO!
	Mod_FreeAll();

	R_ShutdownImages();
#endif // 0

	/* shutdown OS specific OpenGL stuff like contexts, etc.  */
	GL3_ShutdownWindow(false);
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
#if 0 // TODO!
	re.BeginRegistration = RI_BeginRegistration;
	re.RegisterModel = RI_RegisterModel;
	re.RegisterSkin = RI_RegisterSkin;

	re.SetSky = RI_SetSky;
	re.EndRegistration = RI_EndRegistration;

	re.RenderFrame = RI_RenderFrame;

	re.DrawFindPic = RDraw_FindPic;

	re.DrawGetPicSize = RDraw_GetPicSize;

	re.DrawPicScaled = RDraw_PicScaled;
	re.DrawStretchPic = RDraw_StretchPic;

	re.DrawCharScaled = RDraw_CharScaled;
	re.DrawTileClear = RDraw_TileClear;
	re.DrawFill = RDraw_Fill;
	re.DrawFadeScreen = RDraw_FadeScreen;

	re.DrawStretchRaw = RDraw_StretchRaw;

	re.SetPalette = RI_SetPalette;
	re.BeginFrame = RI_BeginFrame;
#endif // 0
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
