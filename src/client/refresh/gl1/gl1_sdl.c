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

#include <SDL2/SDL.h>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

static SDL_Window* window = NULL;
static SDL_GLContext context = NULL;
static qboolean vsyncActive = false;

// ----

/*
 * Swaps the buffers and shows the next frame.
 */
void
RI_EndFrame(void)
{
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

	if (SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8) == 0)
	{
		gl_state.stencil = true;
	}
	else
	{
		gl_state.stencil = false;
	}

	// Let's see if the driver supports MSAA.
	int msaa_samples = 0;

	if (gl_msaa_samples->value)
	{
		msaa_samples = gl_msaa_samples->value;

		if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1) < 0)
		{
			R_Printf(PRINT_ALL, "MSAA is unsupported: %s\n", SDL_GetError());

			ri.Cvar_SetValue ("r_msaa_samples", 0);

			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
		}
		else if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa_samples) < 0)
		{
			R_Printf(PRINT_ALL, "MSAA %ix is unsupported: %s\n", msaa_samples, SDL_GetError());

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
 * Enables or disabes the vsync.
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

	if (SDL_GL_SetSwapInterval(vsync) == -1)
	{
		if (vsync == -1)
		{
			// Not every system supports adaptive
			// vsync, fallback to normal vsync.
			R_Printf(PRINT_ALL, "Failed to set adaptive vsync, reverting to normal vsync.\n");
			SDL_GL_SetSwapInterval(1);
		}
	}

	vsyncActive = SDL_GL_GetSwapInterval() != 0;
}

/*
 * Updates the gamma ramp.
 */
void
RI_UpdateGamma(void)
{
	float gamma = (vid_gamma->value);

	Uint16 ramp[256];
	SDL_CalculateGammaRamp(gamma, ramp);

	if (SDL_SetWindowGammaRamp(window, ramp, ramp, ramp) != 0)
	{
		R_Printf(PRINT_ALL, "Setting gamma failed: %s\n", SDL_GetError());
	}
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
		ri.Sys_Error(ERR_FATAL, "R_InitContext() must not be called with NULL argument!");

		return false;
	}

	window = (SDL_Window*)win;

	// Initialize GL context.
	context = SDL_GL_CreateContext(window);

	if (context == NULL)
	{
		R_Printf(PRINT_ALL, "R_InitContext(): Creating OpenGL Context failed: %s\n", SDL_GetError());

		window = NULL;

		return false;
	}

	// Check if it's really OpenGL 1.4.
	const char* glver = (char *)glGetString(GL_VERSION);
	sscanf(glver, "%d.%d", &gl_config.major_version, &gl_config.minor_version);

	if (gl_config.major_version < 1 || (gl_config.major_version == 1 && gl_config.minor_version < 4))
	{
		R_Printf(PRINT_ALL, "R_InitContext(): Got an OpenGL version %d.%d context - need (at least) 1.4!\n", gl_config.major_version, gl_config.minor_version);

		return false;
	}

	// Check if we've got the requested MSAA.
	int msaa_samples = 0;

	if (gl_msaa_samples->value)
	{
		if (SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &msaa_samples) == 0)
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
		if (SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits) < 0 || stencil_bits < 8)
		{
			gl_state.stencil = false;
		}
	}

	// Initialize gamma.
	vid_gamma->modified = true;

	// Window title - set here so we can display renderer name in it.
	char title[40] = {0};

	snprintf(title, sizeof(title), "Yamagi Quake II %s - OpenGL 1.4", YQ2VERSION);
	SDL_SetWindowTitle(window, title);

	return true;
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
			SDL_GL_DeleteContext(context);
			context = NULL;
		}
	}
}
