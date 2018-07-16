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
 * SDL-specific OpenGL shit formerly in refresh.c
 *
 * =======================================================================
 */

#include "header/local.h"

#ifdef SDL2
#include <SDL2/SDL.h>
#else // SDL1.2
#include <SDL/SDL.h>
#endif //SDL2

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
 static SDL_Window* window = NULL;
 static SDL_GLContext context = NULL;
#else
static SDL_Surface* window = NULL;
#endif

qboolean have_stencil = false;
static qboolean vsyncActive = false;

/*
 * Returns the adress of a GL function
 */
void *
GLimp_GetProcAddress (const char* proc)
{
	return SDL_GL_GetProcAddress ( proc );
}

/*
 *  from SDL2 SDL_CalculateGammaRamp, adjusted for arbitrary ramp sizes
 *  because xrandr seems to support ramp sizes != 256 (in theory at least)
 */
void CalculateGammaRamp(float gamma, Uint16* ramp, int len)
{
    int i;

    /* Input validation */
    if (gamma < 0.0f ) {
      return;
    }
    if (ramp == NULL) {
      return;
    }

    /* 0.0 gamma is all black */
    if (gamma == 0.0f) {
        for (i = 0; i < len; ++i) {
            ramp[i] = 0;
        }
        return;
    } else if (gamma == 1.0f) {
        /* 1.0 gamma is identity */
        for (i = 0; i < len; ++i) {
            ramp[i] = (i << 8) | i;
        }
        return;
    } else {
        /* Calculate a real gamma ramp */
        int value;
        gamma = 1.0f / gamma;
        for (i = 0; i < len; ++i) {
            value = (int) (pow((double) i / (double) len, gamma) * 65535.0 + 0.5);
            if (value > 65535) {
                value = 65535;
            }
            ramp[i] = (Uint16) value;
        }
    }
}

void
UpdateHardwareGamma(void)
{
	float gamma = (vid_gamma->value);

	Uint16 ramp[256];
	CalculateGammaRamp(gamma, ramp, 256);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if(SDL_SetWindowGammaRamp(window, ramp, ramp, ramp) != 0) {
#else
	if(SDL_SetGammaRamp(ramp, ramp, ramp) < 0) {
#endif
		R_Printf(PRINT_ALL, "Setting gamma failed: %s\n", SDL_GetError());
	}
}

static void InitGamma()
{
	R_Printf(PRINT_ALL, "Using hardware gamma via SDL.\n");

	gl_state.hwgamma = true;
	vid_gamma->modified = true;
}

// called by GLimp_InitGraphics() before creating window,
// returns flags for SDL window creation
int RI_PrepareForWindow(void)
{
	unsigned int flags = 0;
	int msaa_samples = 0;

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

#if !SDL_VERSION_ATLEAST(2, 0, 0)
	/* Set vsync - For SDL1.2, this must be done before creating the window */
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, r_vsync->value ? 1 : 0);
#endif

	if (gl_msaa_samples->value)
	{
		msaa_samples = gl_msaa_samples->value;

		if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1) < 0)
		{
			R_Printf(PRINT_ALL, "MSAA is unsupported: %s\n", SDL_GetError());
			ri.Cvar_SetValue ("gl_msaa_samples", 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
		}
		else if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa_samples) < 0)
		{
			R_Printf(PRINT_ALL, "MSAA %ix is unsupported: %s\n", msaa_samples, SDL_GetError());
			ri.Cvar_SetValue("gl_msaa_samples", 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
		}
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	}

	/* Initiate the flags */
#if SDL_VERSION_ATLEAST(2, 0, 0)
	flags = SDL_WINDOW_OPENGL;
#else // SDL 1.2
	flags = SDL_OPENGL;
#endif

	return flags;
}

void RI_SetSwapInterval(void)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	/* Set vsync - TODO: -1 could be set for "late swap tearing" */
	SDL_GL_SetSwapInterval(r_vsync->value ? 1 : 0);
	vsyncActive = SDL_GL_GetSwapInterval() != 0;
#else
	R_Printf(PRINT_ALL, "SDL1.2 requires a vid_restart to apply changes to r_vsync (vsync)!\n");
#endif
}

int RI_InitContext(void* win)
{
	int msaa_samples = 0, stencil_bits = 0;
	char title[40] = {0};

	if(win == NULL)
	{
		ri.Sys_Error(ERR_FATAL, "R_InitContext() must not be called with NULL argument!");
		return false;
	}

#if SDL_VERSION_ATLEAST(2, 0, 0)
	window = (SDL_Window*)win;

	context = SDL_GL_CreateContext(window);
	if(context == NULL)
	{
		R_Printf(PRINT_ALL, "R_InitContext(): Creating OpenGL Context failed: %s\n", SDL_GetError());
		window = NULL;
		return false;
	}
#else // SDL 1.2

	window = (SDL_Surface*)win;
	// context is created implicitly with window, nothing to do here

#endif

	const char* glver = (char *)glGetString(GL_VERSION);
	sscanf(glver, "%d.%d", &gl_config.major_version, &gl_config.minor_version);
	if (gl_config.major_version < 1 || (gl_config.major_version == 1 && gl_config.minor_version < 4))
	{
		R_Printf(PRINT_ALL, "R_InitContext(): Got an OpenGL version %d.%d context - need (at least) 1.4!\n", gl_config.major_version, gl_config.minor_version);
		return false;
	}

	if (gl_msaa_samples->value)
	{
		if (SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &msaa_samples) == 0)
		{
			ri.Cvar_SetValue("gl_msaa_samples", msaa_samples);
		}
	}

#if SDL_VERSION_ATLEAST(2, 0, 0)
	/* For SDL2, this must be done after creating the window */
	RI_SetSwapInterval();
#else // SDL1.2 - set vsyncActive to whatever is configured, hoping it was actually set
	vsyncActive = r_vsync->value ? 1 : 0;
#endif

	/* Initialize the stencil buffer */
	if (!SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits))
	{
		R_Printf(PRINT_ALL, "Got %d bits of stencil.\n", stencil_bits);

		if (stencil_bits >= 1)
		{
			have_stencil = true;
		}
	}

	/* Initialize hardware gamma */
	InitGamma();

	/* Window title - set here so we can display renderer name in it */
	snprintf(title, sizeof(title), "Yamagi Quake II %s - OpenGL 1.x", YQ2VERSION);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_SetWindowTitle(window, title);
#else
	SDL_WM_SetCaption(title, title);
#endif

	return true;
}

qboolean RI_IsVSyncActive(void)
{
	return vsyncActive;
}

/*
 * Swaps the buffers to show the new frame
 */
void
RI_EndFrame(void)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_GL_SwapWindow(window);
#else
	SDL_GL_SwapBuffers();
#endif
}

/*
 * Shuts the SDL render backend down
 */
void
RI_ShutdownWindow(qboolean contextOnly)
{
	/* Clear the backbuffer and make it
	   current. This may help some broken
	   video drivers like the AMD Catalyst
	   to avoid artifacts in unused screen
	   areas.
	   Only do this if we have a context, though. */
	if (window)
	{
#if SDL_VERSION_ATLEAST(2, 0, 0)
		if(context)
		{
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			RI_EndFrame();

			SDL_GL_DeleteContext(context);
			context = NULL;
		}
#else // SDL 1.2
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		RI_EndFrame();
#endif
	}

	window = NULL;

	gl_state.hwgamma = false;

	if(!contextOnly)
	{
		ri.Vid_ShutdownWindow();
	}
}
