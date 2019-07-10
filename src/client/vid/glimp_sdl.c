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

int glimp_refreshRate = -1;

static cvar_t *vid_displayrefreshrate;
static cvar_t *vid_displayindex;
static cvar_t *vid_rate;

static int last_flags = 0;
static int last_display = 0;
static int last_position_x = SDL_WINDOWPOS_UNDEFINED;
static int last_position_y = SDL_WINDOWPOS_UNDEFINED;
static SDL_Window* window = NULL;
static qboolean initSuccessful = false;
static char **displayindices = NULL;
static int num_displays = 0;

/*
 * Resets the display index Cvar if out of bounds
 */
static void
ClampDisplayIndexCvar(void)
{
	if (vid_displayindex->value < 0 || vid_displayindex->value >= num_displays)
	{
		Cvar_SetValue("vid_displayindex", 0);
	}
}

static void
ClearDisplayIndices(void)
{
	if ( displayindices )
	{
		for ( int i = 0; i < num_displays; i++ )
		{
			free( displayindices[ i ] );
		}

		free( displayindices );
		displayindices = NULL;
	}
}

static qboolean
CreateSDLWindow(int flags, int w, int h)
{
	if (SDL_WINDOWPOS_ISUNDEFINED(last_position_x) || SDL_WINDOWPOS_ISUNDEFINED(last_position_y))
	{
		last_position_x = last_position_y = SDL_WINDOWPOS_UNDEFINED_DISPLAY((int)vid_displayindex->value);
	}

	window = SDL_CreateWindow("Yamagi Quake II", last_position_x, last_position_y, w, h, flags);

	if (window)
	{

		/* save current display as default */
		last_display = SDL_GetWindowDisplayIndex(window);
		SDL_GetWindowPosition(window, &last_position_x, &last_position_y);

		/* Check if we're really in the requested diplay mode. There is
		   (or was) an SDL bug were SDL switched into the wrong mode
		   without giving an error code. See the bug report for details:
		   https://bugzilla.libsdl.org/show_bug.cgi?id=4700 */
		SDL_DisplayMode real_mode;

		if (SDL_GetWindowDisplayMode(window, &real_mode) != 0)
		{
			SDL_DestroyWindow(window);
			window = NULL;

			Com_Printf("Can't get display mode: %s\n", SDL_GetError());

			return false;
		}

		if ((flags & SDL_WINDOW_FULLSCREEN) && ((real_mode.w != w) || (real_mode.h != h)))
		{

			Com_Printf("Current display mode isn't requested display mode\n");
			Com_Printf("Likely SDL bug #4700, trying to work around it\n");

			/* Mkay, try to hack around that. */
			SDL_DisplayMode wanted_mode = {};

			wanted_mode.w = w;
			wanted_mode.h = h;

			if (SDL_SetWindowDisplayMode(window, &wanted_mode) != 0)
			{
				SDL_DestroyWindow(window);
				window = NULL;

				Com_Printf("Can't force resolution to %ix%i: %s\n", w, h, SDL_GetError());

				return false;
			}

			/* The SDL doku says, that SDL_SetWindowSize() shouldn't be
			   used on fullscreen windows. But at least in my test with
			   SDL 2.0.9 the subsequent SDL_GetWindowDisplayMode() fails
			   if I don't call it. */
			SDL_SetWindowSize(window, wanted_mode.w, wanted_mode.h);

			if (SDL_GetWindowDisplayMode(window, &real_mode) != 0)
			{
				SDL_DestroyWindow(window);
				window = NULL;

				Com_Printf("Can't get display mode: %s\n", SDL_GetError());

				return false;
			}

			if ((real_mode.w != w) || (real_mode.h != h))
			{
				SDL_DestroyWindow(window);
				window = NULL;

				Com_Printf("Can't get display mode: %s\n", SDL_GetError());

				return false;
			}
		}

        /* Normally SDL stays at desktop refresh rate or chooses
		   something sane. Some player may want to override that. */
		if (flags & SDL_WINDOW_FULLSCREEN)
		{
			if (vid_rate->value > 0)
			{
				SDL_DisplayMode closest_mode;
				SDL_DisplayMode requested_mode = real_mode;

				requested_mode.refresh_rate = (int)vid_rate->value;

				if (SDL_GetClosestDisplayMode(last_display, &requested_mode, &closest_mode) == NULL)
				{
					Com_Printf("SDL was unable to find a mode close to %ix%i@%i\n", w, h, requested_mode.refresh_rate);
					Cvar_SetValue("vid_rate", -1);
				}
				else
				{
					Com_Printf("User requested %ix%i@%i, setting closest mode %ix%i@%i\n", 
							w, h, requested_mode.refresh_rate, w, h, closest_mode.refresh_rate);

					if (SDL_SetWindowDisplayMode(window, &closest_mode) != 0)
					{
						Com_Printf("Couldn't switch to mode %ix%i@%i, staying at current mode\n",
								w, h, closest_mode.refresh_rate);
						Cvar_SetValue("vid_rate", -1);
					}
					else
					{
						Cvar_SetValue("vid_rate", closest_mode.refresh_rate);
					}
				}

			}
		}
	}
	else
	{
		return false;
	}

	return true;
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

static void
InitDisplayIndices()
{
	displayindices = malloc((num_displays + 1) * sizeof(char *));

	for ( int i = 0; i < num_displays; i++ )
	{
		/* There are a maximum of 10 digits in 32 bit int + 1 for the NULL terminator. */
		displayindices[ i ] = malloc(11 * sizeof( char ));
		snprintf( displayindices[ i ], 11, "%d", i );
	}

	/* The last entry is NULL to indicate the list of strings ends. */
	displayindices[ num_displays ] = 0;
}

/*
 * Lists all available display modes.
 */
static void
PrintDisplayModes(void)
{
	int curdisplay = SDL_GetWindowDisplayIndex(window);

	// On X11 (at least for me)
	// curdisplay is always -1.
	if (curdisplay < 0) {
		curdisplay = 0;
	}

	int nummodes = SDL_GetNumDisplayModes(curdisplay);

	if (nummodes < 1)
	{
		Com_Printf("Can't get display modes: %s\n", SDL_GetError());
		return;
	}

	for (int i = 0; i < nummodes; i++)
	{
		SDL_DisplayMode mode;

		if (SDL_GetDisplayMode(curdisplay, i, &mode) != 0)
		{
			Com_Printf("Can't get display mode: %s\n", SDL_GetError());
			return;
		}

		Com_Printf(" - Mode %2i: %ix%i@%i\n", i, mode.w, mode.h, mode.refresh_rate);
	}
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
	ClampDisplayIndexCvar();

	if (window)
	{
		/* save current display as default */
		last_display = SDL_GetWindowDisplayIndex(window);

		/* or if current display isn't the desired default */
		if (last_display != vid_displayindex->value) {
			last_position_x = last_position_y = SDL_WINDOWPOS_UNDEFINED;
			last_display = vid_displayindex->value;
		}
		else {
			SDL_GetWindowPosition(window,
				      &last_position_x, &last_position_y);
		}

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
	vid_displayindex = Cvar_Get("vid_displayindex", "0", CVAR_ARCHIVE);
	vid_rate = Cvar_Get("vid_rate", "-1", CVAR_ARCHIVE);

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

		num_displays = SDL_GetNumVideoDisplays();
		InitDisplayIndices();
		ClampDisplayIndexCvar();
		Com_Printf("SDL didplay modes:\n");

		PrintDisplayModes();
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

	ClearDisplayIndices();
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
				Cvar_SetValue("vid_rate", -1);

				fullscreen = 0;
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

	/* Now that we've got a working window print it's mode. */
	int curdisplay = SDL_GetWindowDisplayIndex(window);

    if (curdisplay < 0) {
		curdisplay = 0;
	}

	SDL_DisplayMode mode;

	if (SDL_GetCurrentDisplayMode(curdisplay, &mode) != 0)
	{
		Com_Printf("Can't get current display mode: %s\n", SDL_GetError());
	}
	else
	{
		Com_Printf("Real display mode: %ix%i@%i (vid_fullscreen: %i)\n", mode.w, mode.h,
				mode.refresh_rate, fullscreen);
	}


    /* Initialize rendering context. */
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
 * * SDL returns only full integers. In most cases they're rounded
 *   up, but in some cases - likely depending on the GPU driver -
 *   they're rounded down. If the value is rounded up, we'll see
 *   some small and nard to notice timing problems. If the value
 *   is rounded down frames will be missed. Both is only relevant
 *   if the vsync is enabled.
 */
int
GLimp_GetRefreshRate(void)
{

	if (vid_displayrefreshrate->value > -1 ||
			vid_displayrefreshrate->modified)
	{
		glimp_refreshRate = ceil(vid_displayrefreshrate->value);
		vid_displayrefreshrate->modified = false;
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

/*
 * Detect current desktop mode
 */
qboolean
GLimp_GetDesktopMode(int *pwidth, int *pheight)
{
	// Declare display mode structure to be filled in.
	SDL_DisplayMode mode;

	if (window)
	{
		/* save current display as default */
		last_display = SDL_GetWindowDisplayIndex(window);
		SDL_GetWindowPosition(window, &last_position_x, &last_position_y);
	}

	if (last_display < 0)
	{
		// In case of error...
		Com_Printf("Can't detect current desktop.\n");
		last_display = 0;
	}

	// We can't get desktop where we start, so use first desktop
	if(SDL_GetDesktopDisplayMode(last_display, &mode) != 0)
	{
		// In case of error...
		Com_Printf("Can't detect default desktop mode: %s\n",
				SDL_GetError());
		return false;
	}
	*pwidth = mode.w;
	*pheight = mode.h;
	return true;
}

const char**
GLimp_GetDisplayIndices(void)
{
	return (const char**)displayindices;
}

int
GLimp_GetNumVideoDisplays(void)
{
	return num_displays;
}

int
GLimp_GetWindowDisplayIndex(void)
{
	return last_display;
}
