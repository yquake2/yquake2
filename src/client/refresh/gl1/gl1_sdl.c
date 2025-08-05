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
 * SDL backend for the GL1 renderer.
 *
 * =======================================================================
 */

#include "header/local.h"

#ifdef USE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL2/SDL.h>
#endif

static SDL_Window* window = NULL;
static SDL_GLContext context = NULL;
qboolean IsHighDPIaware = false;
static qboolean vsyncActive = false;

extern cvar_t *gl1_discardfb;

// ----

/*
 * Swaps the buffers and shows the next frame.
 */
void
RI_EndFrame(void)
{
	R_ApplyGLBuffer();	// to draw buffered 2D text

#ifdef YQ2_GL1_GLES
	static const GLenum attachments[3] = {GL_COLOR_EXT, GL_DEPTH_EXT, GL_STENCIL_EXT};

	if (qglDiscardFramebufferEXT)
	{
		switch ((int)gl1_discardfb->value)
		{
			case 1:
				qglDiscardFramebufferEXT(GL_FRAMEBUFFER_OES, 3, &attachments[0]);
				break;
			case 2:
				qglDiscardFramebufferEXT(GL_FRAMEBUFFER_OES, 2, &attachments[1]);
				break;
			default:
				break;
		}
	}
#endif

	SDL_GL_SwapWindow(window);
}

/*
 * Returns the adress of a GL function
 */
void *
RI_GetProcAddress(const char* proc)
{
	return SDL_GL_GetProcAddress(proc);
}

/*
 * Returns whether the vsync is enabled.
 */
qboolean RI_IsVSyncActive(void)
{
	return vsyncActive;
}

/*
 * This function returns the flags used at the SDL window
 * creation by GLimp_InitGraphics(). In case of error -1
 * is returned.
 */
int RI_PrepareForWindow(void)
{
	// Set GL context attributs bound to the window.
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

#ifdef USE_SDL3
	if (SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8))
#else
	if (SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8) == 0)
#endif
	{
		gl_state.stencil = true;
	}
	else
	{
		gl_state.stencil = false;
	}

#ifdef YQ2_GL1_GLES
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif

	// Let's see if the driver supports MSAA.
	int msaa_samples = 0;

	if (gl_msaa_samples->value)
	{
		msaa_samples = gl_msaa_samples->value;

#ifdef USE_SDL3
		if (!SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1))
#else
		if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1) < 0)
#endif
		{
			Com_Printf("MSAA is unsupported: %s\n", SDL_GetError());

			ri.Cvar_SetValue ("r_msaa_samples", 0);

			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
		}
#ifdef USE_SDL3
		else if (!SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa_samples))
#else
		else if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa_samples) < 0)
#endif
		{
			Com_Printf("MSAA %ix is unsupported: %s\n", msaa_samples, SDL_GetError());

			ri.Cvar_SetValue("r_msaa_samples", 0);

			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
		}
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	}

	return SDL_WINDOW_OPENGL;
}

/*
 * Enables or disables the vsync.
 */
void RI_SetVsync(void)
{
	// Make sure that the user given
	// value is SDL compatible...
	int vsync = 0;

	if (r_vsync->value == 1)
	{
		vsync = 1;
	}
	else if (r_vsync->value == 2)
	{
		vsync = -1;
	}

#ifdef USE_SDL3
	if (!SDL_GL_SetSwapInterval(vsync))
#else
	if (SDL_GL_SetSwapInterval(vsync) == -1)
#endif
	{
		if (vsync == -1)
		{
			// Not every system supports adaptive
			// vsync, fallback to normal vsync.
			Com_Printf("Failed to set adaptive vsync, reverting to normal vsync.\n");
			SDL_GL_SetSwapInterval(1);
		}
	}

#ifdef USE_SDL3
	int vsyncState;
	if (!SDL_GL_GetSwapInterval(&vsyncState))
	{
		Com_Printf("Failed to get vsync state, assuming vsync inactive.\n");
		vsyncActive = false;
	}
	else
	{
		vsyncActive = vsyncState ? true : false;
	}
#else
	vsyncActive = SDL_GL_GetSwapInterval() != 0;
#endif
}

/*
 * Updates the gamma ramp. Only used with SDL2.
 */
void
RI_UpdateGamma(void)
{
// Hardware gamma / gamma ramps are no longer supported with SDL3.
// There's no replacement and sdl2-compat won't support it either.
// See https://github.com/libsdl-org/SDL/pull/6617 for the rational.
// Gamma works with a lookup table when using SDL3 (or GLES1).
#ifndef GL1_GAMMATABLE
	float gamma = (vid_gamma->value);

	Uint16 ramp[256];
	SDL_CalculateGammaRamp(gamma, ramp);

	if (SDL_SetWindowGammaRamp(window, ramp, ramp, ramp) != 0)
	{
		Com_Printf("Setting gamma failed: %s\n", SDL_GetError());
	}
#endif
}

/*
 * Initializes the OpenGL context. Returns true at
 * success and false at failure.
 */
int RI_InitContext(void* win)
{
	// Coders are stupid.
	if (win == NULL)
	{
		Com_Error(ERR_FATAL, "%s must not be called with NULL argument!", __func__);

		return false;
	}

	window = (SDL_Window*)win;

	// Initialize GL context.
	context = SDL_GL_CreateContext(window);

	if (context == NULL)
	{
		Com_Printf("%s: Creating OpenGL Context failed: %s\n", __func__, SDL_GetError());

		window = NULL;

		return false;
	}

#ifdef YQ2_GL1_GLES

	// Load GL pointers through GLAD and check context.
	if( !gladLoadGLES1Loader( (void * (*)(const char *)) SDL_GL_GetProcAddress ) )
	{
		Com_Printf("%s ERROR: loading OpenGL ES function pointers failed!\n", __func__);
		return false;
	}

	gl_config.major_version = GLVersion.major;
	gl_config.minor_version = GLVersion.minor;
	Com_Printf("Initialized OpenGL ES version %d.%d context\n", gl_config.major_version, gl_config.minor_version);

#else

	// Check if it's really OpenGL 1.4.
	const char* glver = (char *)glGetString(GL_VERSION);
	sscanf(glver, "%d.%d", &gl_config.major_version, &gl_config.minor_version);

	if (gl_config.major_version < 1 || (gl_config.major_version == 1 && gl_config.minor_version < 4))
	{
		Com_Printf("%s: Got an OpenGL version %d.%d context - need (at least) 1.4!\n", __func__, gl_config.major_version, gl_config.minor_version);

		return false;
	}

#endif

	// Check if we've got the requested MSAA.
	int msaa_samples = 0;

	if (gl_msaa_samples->value)
	{
#ifdef USE_SDL3
		if (SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &msaa_samples))
#else
		if (SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &msaa_samples) == 0)
#endif
		{
			ri.Cvar_SetValue("r_msaa_samples", msaa_samples);
		}
	}

	// Enable vsync if requested.
	RI_SetVsync();

	// Check if we've got 8 stencil bits.
	int stencil_bits = 0;

	if (gl_state.stencil)
	{
#ifdef USE_SDL3
		if (!SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits) || stencil_bits < 8)
#else
		if (SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits) < 0 || stencil_bits < 8)
#endif
		{
			gl_state.stencil = false;
		}
	}

	// Initialize gamma.
	vid_gamma->modified = true;

	// Window title - set here so we can display renderer name in it.
	char title[40] = {0};

#ifdef YQ2_GL1_GLES
	snprintf(title, sizeof(title), "Yamagi Quake II %s - OpenGL ES 1.0", YQ2VERSION);
#else
	snprintf(title, sizeof(title), "Yamagi Quake II %s - OpenGL 1.4", YQ2VERSION);
#endif
	SDL_SetWindowTitle(window, title);

#if SDL_VERSION_ATLEAST(2, 26, 0)
	// Figure out if we are high dpi aware.
	int flags = SDL_GetWindowFlags(win);
#ifdef USE_SDL3
	IsHighDPIaware = (flags & SDL_WINDOW_HIGH_PIXEL_DENSITY) ? true : false;
#else
	IsHighDPIaware = (flags & SDL_WINDOW_ALLOW_HIGHDPI) ? true : false;
#endif
#endif

	return true;
}

/*
 * Fills the actual size of the drawable into width and height.
 */
void RI_GetDrawableSize(int* width, int* height)
{
#ifdef USE_SDL3
	SDL_GetWindowSizeInPixels(window, width, height);
#else
	SDL_GL_GetDrawableSize(window, width, height);
#endif
}

/*
 * Shuts the GL context down.
 */
void
RI_ShutdownContext(void)
{
	if (window)
	{
		if(context)
		{
#ifdef USE_SDL3
			SDL_GL_DestroyContext(context);
#else
			SDL_GL_DeleteContext(context);
#endif
			context = NULL;
		}
	}
}

/*
 * Returns the SDL major version. Implemented
 * here to not polute gl1_main.c with the SDL
 * headers.
 */
int RI_GetSDLVersion()
{
#ifdef USE_SDL3
	int version = SDL_GetVersion();
	return SDL_VERSIONNUM_MAJOR(version);
#else
	SDL_version ver;
	SDL_VERSION(&ver);
	return ver.major;
#endif
}
