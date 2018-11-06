/*
 * Copyright (C) 2010 Yamagi Burmeister
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
 * This is the client side of the render backend, implemented trough SDL.
 * The SDL window and related functrion (mouse grap, fullscreen switch)
 * are implemented here, everything else is in the renderers.
 *
 * =======================================================================
 */

#include "../../common/header/common.h"
#include "header/ref.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

cvar_t *vid_displayrefreshrate;
int glimp_refreshRate = -1;

static int last_flags = 0;
static SDL_Window* window = NULL;
static qboolean initSuccessful = false;

// --------

static qboolean
CreateSDLWindow(int flags, int w, int h)
{
	int windowPos = SDL_WINDOWPOS_UNDEFINED;

	window = SDL_CreateWindow("Yamagi Quake II", windowPos, windowPos, w, h, flags);

	return window != NULL;
}

static int
GetFullscreenType()
{
	if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP)
	{
		return 1;
	}
	else if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)
	{
		return 2;
	}
	else
	{
		return 0;
	}
}

static qboolean
GetWindowSize(int* w, int* h)
{
	if (window == NULL || w == NULL || h == NULL)
	{
		return false;
	}

	SDL_DisplayMode m;

	if (SDL_GetWindowDisplayMode(window, &m) != 0)
	{
		Com_Printf("Can't get Displaymode: %s\n", SDL_GetError());

		return false;
	}

	*w = m.w;
	*h = m.h;

	return true;
}

/*
 * Sets the window icon
 */
static void
SetSDLIcon()
{
	#include "icon/q2icon64.h" // 64x64 32 Bit

	/* these masks are needed to tell SDL_CreateRGBSurface(From)
	   to assume the data it gets is byte-wise RGB(A) data */
	Uint32 rmask, gmask, bmask, amask;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	int shift = (q2icon64.bytes_per_pixel == 3) ? 8 : 0;
	rmask = 0xff000000 >> shift;
	gmask = 0x00ff0000 >> shift;
	bmask = 0x0000ff00 >> shift;
	amask = 0x000000ff >> shift;
#else /* little endian, like x86 */
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = (q2icon64.bytes_per_pixel == 3) ? 0 : 0xff000000;
#endif

	SDL_Surface* icon = SDL_CreateRGBSurfaceFrom((void*)q2icon64.pixel_data, q2icon64.width,
		q2icon64.height, q2icon64.bytes_per_pixel*8, q2icon64.bytes_per_pixel*q2icon64.width,
		rmask, gmask, bmask, amask);
	SDL_SetWindowIcon(window, icon);
	SDL_FreeSurface(icon);
}

// FIXME: We need a header for this.
// Maybe we could put it in vid.h.
void GLimp_GrabInput(qboolean grab);

/*
 * Shuts the SDL render backend down
 */
static void
ShutdownGraphics(void)
{
	if (window)
	{
		/* cleanly ungrab input (needs window) */
		GLimp_GrabInput(false);
		SDL_DestroyWindow(window);

		window = NULL;
	}

	// make sure that after vid_restart the refreshrate will be queried from SDL2 again.
	glimp_refreshRate = -1;

	initSuccessful = false; // not initialized anymore
}
// --------

/*
 * Initializes the SDL video subsystem. Must
 * be called before anything else.
 */
qboolean
GLimp_Init(void)
{
	vid_displayrefreshrate = Cvar_Get("vid_displayrefreshrate", "-1", CVAR_ARCHIVE);

	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			Com_Printf("Couldn't init SDL video: %s.\n", SDL_GetError());

			return false;
		}

		SDL_version version;

		SDL_GetVersion(&version);
		Com_Printf("SDL version is: %i.%i.%i\n", (int)version.major, (int)version.minor, (int)version.patch);
		Com_Printf("SDL video driver is \"%s\".\n", SDL_GetCurrentVideoDriver());
	}

	return true;
}

/*
 * Shuts the SDL video subsystem down. Must
 * be called after evrything's finished and
 * clean up.
 */
void
GLimp_Shutdown(void)
{
	ShutdownGraphics();

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
	{
		SDL_Quit();
	}
	else
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}

/*
 * (Re)initializes the actual window.
 */
qboolean
GLimp_InitGraphics(int fullscreen, int *pwidth, int *pheight)
{
	int flags;
	int curWidth, curHeight;
	int width = *pwidth;
	int height = *pheight;
	unsigned int fs_flag = 0;

	if (fullscreen == 1)
	{
		fs_flag = SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	else if (fullscreen == 2)
	{
		fs_flag = SDL_WINDOW_FULLSCREEN;
	}

	/* Only do this if we already have a working window and a fully
	initialized rendering backend GLimp_InitGraphics() is also
	called when recovering if creating GL context fails or the
	one we got is unusable. */
	if (initSuccessful && GetWindowSize(&curWidth, &curHeight)
			&& (curWidth == width) && (curHeight == height))
	{
		/* If we want fullscreen, but aren't */
		if (GetFullscreenType())
		{
			SDL_SetWindowFullscreen(window, fs_flag);
			Cvar_SetValue("vid_fullscreen", fullscreen);
		}

		/* Are we now? */
		if (GetFullscreenType())
		{
			return true;
		}
	}

	/* Is the surface used? */
	if (window)
	{
		re.ShutdownContext();
		ShutdownGraphics();

		window = NULL;
	}

	/* We need the window size for the menu, the HUD, etc. */
	viddef.width = width;
	viddef.height = height;

	if(last_flags != -1 && (last_flags & SDL_WINDOW_OPENGL))
	{
		/* Reset SDL. */
		SDL_GL_ResetAttributes();
	}

	/* Let renderer prepare things (set OpenGL attributes).
	   FIXME: This is no longer necessary, the renderer
	   could and should pass the flags when calling this
	   function. */
	flags = re.PrepareForWindow();

	if (flags == -1)
	{
		/* It's PrepareForWindow() job to log an error */
		return false;
	}

	if (fs_flag)
	{
		flags |= fs_flag;
	}

	/* Mkay, now the hard work. Let's create the window. */
	cvar_t *gl_msaa_samples = Cvar_Get("gl_msaa_samples", "0", CVAR_ARCHIVE);

	while (1)
	{
		if (!CreateSDLWindow(flags, width, height))
		{
			if((flags & SDL_WINDOW_OPENGL) && gl_msaa_samples->value)
			{
				Com_Printf("SDL SetVideoMode failed: %s\n", SDL_GetError());
				Com_Printf("Reverting to %s r_mode %i (%ix%i) without MSAA.\n",
					        (flags & fs_flag) ? "fullscreen" : "windowed",
					        (int) Cvar_VariableValue("r_mode"), width, height);

				/* Try to recover */
				Cvar_SetValue("gl_msaa_samples", 0);

				SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
				SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
			}
			else if (width != 640 || height != 480 || (flags & fs_flag))
			{
				Com_Printf("SDL SetVideoMode failed: %s\n", SDL_GetError());
				Com_Printf("Reverting to windowed r_mode 4 (640x480).\n");

				/* Try to recover */
				Cvar_SetValue("r_mode", 4);
				Cvar_SetValue("vid_fullscreen", 0);

				*pwidth = width = 640;
				*pheight = height = 480;
				flags &= ~fs_flag;
			}
			else
			{
				Com_Error(ERR_FATAL, "Failed to revert to r_mode 4. Exiting...\n");
				return false;
			}
		}
		else
		{
			break;
		}
	}

	last_flags = flags;

	if (!re.InitContext(window))
	{
		/* InitContext() should have logged an error. */
		return false;
	}

	/* Set the window icon - For SDL2, this must be done after creating the window */
	SetSDLIcon();

	/* No cursor */
	SDL_ShowCursor(0);

	initSuccessful = true;

	return true;
}

/*
 * Shuts the window down.
 */
void
GLimp_ShutdownGraphics(void)
{
	SDL_GL_ResetAttributes();
	ShutdownGraphics();
}

/*
 * (Un)grab Input
 */
void
GLimp_GrabInput(qboolean grab)
{
	if(window != NULL)
	{
		SDL_SetWindowGrab(window, grab ? SDL_TRUE : SDL_FALSE);
	}

	if(SDL_SetRelativeMouseMode(grab ? SDL_TRUE : SDL_FALSE) < 0)
	{
		Com_Printf("WARNING: Setting Relative Mousemode failed, reason: %s\n", SDL_GetError());
		Com_Printf("         You should probably update to SDL 2.0.3 or newer!\n");
	}
}

/*
 * Returns the current display refresh rate. There're 2 limitations:
 *
 * * The timing code in frame.c only understands full integers, so
 *   values given by vid_displayrefreshrate are always round up. For
 *   example 59.95 become 60. Rounding up is the better choice for
 *   most users because assuming a too high display refresh rate
 *   avoids micro stuttering caused by missed frames if the vsync
 *   is enabled. The price are small and hard to notice timing
 *   problems.
 *
 * * SDL returns only full integer. In most cases they're rounded
 *   up, but in some cases - likely depending on the GPU driver -
 *   they're rounded down. If the value is rounded up, we'll see
 *   some small and nard to notice timing problems. If the value
 *   is rounded down frames will be missed. Both is only relevant
 *   if the vsync is enabled.
 */
int
GLimp_GetRefreshRate(void)
{

	if (vid_displayrefreshrate->value > -1)
	{
		glimp_refreshRate = ceil(vid_displayrefreshrate->value);
	}

	if (glimp_refreshRate == -1)
	{
		SDL_DisplayMode mode;

		int i = SDL_GetWindowDisplayIndex(window);

		if (i >= 0 && SDL_GetCurrentDisplayMode(i, &mode) == 0)
		{
			glimp_refreshRate = mode.refresh_rate;
		}

		// Something went wrong, use default.
		if (glimp_refreshRate <= 0)
		{
			glimp_refreshRate = 60;
		}
	}

	return glimp_refreshRate;
}
