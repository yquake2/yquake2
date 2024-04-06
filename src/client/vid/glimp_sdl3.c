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

/* TODO SDL3:
 *  * Bump copyright.
 *  * Do we need to request High DPI modes when vid_highdpiaware > 0?
 *  * `fullscreen` should be an enum to make the code more readable.
 *  * Debug fullscreen handling, maybe refactor it further.
 *  * Check if window size handling is correct.
 *  * Check pointers returned by SDL functions for memory leaks.
 */

#include "../../common/header/common.h"
#include "header/ref.h"

#include <SDL3/SDL.h>

float glimp_refreshRate = -1.0f;

static cvar_t *vid_displayrefreshrate;
static cvar_t *vid_displayindex;
static cvar_t *vid_highdpiaware;
static cvar_t *vid_rate;

static int last_flags = 0;
static int last_display = 0;
static int last_position_x = SDL_WINDOWPOS_UNDEFINED;
static int last_position_y = SDL_WINDOWPOS_UNDEFINED;
static SDL_Window* window = NULL;
static qboolean initSuccessful = false;
static char **displayindices = NULL;
static int num_displays = 0;

/* Fullscreen modes */
enum
{
	FULLSCREEN_OFF = 0,
	FULLSCREEN_EXCLUSIVE = 1,
	FULLSCREEN_DESKTOP = 2
};

/*
 * Resets the display index Cvar if out of bounds
 */
static void
ClampDisplayIndexCvar(void)
{
	if (!vid_displayindex)
	{
		// uninitialized render?
		return;
	}

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
CreateSDLWindow(int flags, int fullscreen, int w, int h)
{
	if (SDL_WINDOWPOS_ISUNDEFINED(last_position_x) || SDL_WINDOWPOS_ISUNDEFINED(last_position_y) || last_position_x < 0 ||last_position_y < 24)
	{
		last_position_x = last_position_y = SDL_WINDOWPOS_UNDEFINED_DISPLAY((int)vid_displayindex->value);
	}

	/* Force the window to minimize when focus is lost. This was the
	 * default behavior until SDL 2.0.12 and changed with 2.0.14.
	 * The windows staying maximized has some odd implications for
	 * window ordering under Windows and some X11 window managers
	 * like kwin. See:
	 *  * https://github.com/libsdl-org/SDL/issues/4039
	 *  * https://github.com/libsdl-org/SDL/issues/3656 */
	SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "1");

	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Yamagi Quake II");
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, last_position_x);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, last_position_y);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, w);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, h);
	SDL_SetNumberProperty(props, "flags", flags);

	window = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);

	if (window)
	{
		/* save current display as default */
		if ((last_display = SDL_GetDisplayForWindow(window)) == 0)
		{
			/* There are some obscure setups were SDL is
			   unable to get the current display,one X11
			   server with several screen is one of these,
			   so add a fallback to the first display. */
			last_display = 1;
		}

		/* Set requested fullscreen mode. */
		if (flags & SDL_WINDOW_FULLSCREEN)
		{
            /* SDLs behavior changed between SDL 2 and SDL 3: In SDL 2
			   the fullscreen window could be set with whatever mode
			   was requested. In SDL 3 the fullscreen window is always
			   created at desktop resolution. If a fullscreen window
			   is requested, we can't do anything else and are done here. */
			if (fullscreen == FULLSCREEN_DESKTOP)
			{
				return true;
			}

			/* Otherwise try to find a mode near the requested one and
			   switch to it in exclusive fullscreen mode. */
			/* TODO SDL3: Leak? */
		    const SDL_DisplayMode *closestMode = SDL_GetClosestFullscreenDisplayMode(last_display, w, h, vid_rate->value, false);

			if (closestMode == NULL)
			{
				Com_Printf("SDL was unable to find a mode close to %ix%i@%f\n", w, h, vid_rate->value);

				if (vid_rate->value != 0)
				{
					Com_Printf("Retrying with desktop refresh rate\n");
					closestMode = SDL_GetClosestFullscreenDisplayMode(last_display, w, h, 0, false);

					if (closestMode != NULL)
					{
						Cvar_SetValue("vid_rate", 0);
					}
					else
					{
						Com_Printf("SDL was unable to find a mode close to %ix%i@0\n", w, h);
						return false;
					}
				}
			}

			Com_Printf("User requested %ix%i@%f, setting closest mode %ix%i@%f\n",
					w, h, vid_rate->value, closestMode->w, closestMode->h , closestMode->refresh_rate);


			/* TODO SDL3: Same code is in InitGraphics(), refactor into
			 * a function? */
			if (SDL_SetWindowFullscreenMode(window, closestMode) < 0)
			{
				Com_Printf("Couldn't set closest mode: %s\n", SDL_GetError());
				return false;
			}

			if (SDL_SetWindowFullscreen(window, true) < 0)
			{
				Com_Printf("Couldn't switch to exclusive fullscreen: %s\n", SDL_GetError());
				return false;
			}

			int ret = SDL_SyncWindow(window);

			if (ret > 0)
			{
				Com_Printf("Synchronizing window state timed out\n");
				return false;
			}
			else if (ret < 0)
			{
				Com_Printf("Couldn't synchronize window state: %s\n", SDL_GetError());
				return false;
			}
		}
	}
	else
	{
		Com_Printf("Creating window failed: %s\n", SDL_GetError());
		return false;
	}

	return true;
}

static int
GetFullscreenType()
{
	if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)
	{
		/* TODO SDL3: Leak? */
		const SDL_DisplayMode *fsmode = SDL_GetWindowFullscreenMode(window);

		if (fsmode != NULL)
		{
			return FULLSCREEN_EXCLUSIVE;
		}
		else
		{
			return FULLSCREEN_DESKTOP;
		}
	}

	return FULLSCREEN_OFF;
}

static qboolean
GetWindowSize(int* w, int* h)
{
	if (window == NULL || w == NULL || h == NULL)
	{
		return false;
	}

	if (SDL_GetWindowSize(window, w, h) < 0)
	{
		Com_Printf("Couldn't get window size: %s\n", SDL_GetError());
		return false;
	}

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
		YQ2_COM_CHECK_OOM(displayindices[i], "malloc()", 11 * sizeof( char ))

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
	int curdisplay;

	if (window == NULL)
	{
		/* Called without a windows, list modes
		   from the first display. This is the
		   primary display and likely the one the
		   game will run on. */
		curdisplay = SDL_GetPrimaryDisplay();
	}
	else
	{
		/* Otherwise use the window were the window
		   is displayed. There are some obscure
		   setups were this can fail - one X11 server
		   with several screen is one of these - so
		   add a fallback to the first display. */
		if ((curdisplay = SDL_GetDisplayForWindow(window)) == 0)
		{
			curdisplay = SDL_GetPrimaryDisplay();
		}
	}

	int nummodes = 0;
	const SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(curdisplay, &nummodes);

	if (modes)
	{
        for (int i = 0; i < nummodes; ++i)
		{
            const SDL_DisplayMode *mode = modes[i];
			Com_Printf(" - Mode %2i: %ix%i@%.2f\n", i, mode->w, mode->h, mode->refresh_rate);
		}

		SDL_free(modes);
	}
	else
	{
		Com_Printf("Couldn't get display modes: %s\n", SDL_GetError());
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

	SDL_Surface* icon = SDL_CreateSurfaceFrom((void *)q2icon64.pixel_data, q2icon64.width, q2icon64.height, q2icon64.bytes_per_pixel * q2icon64.width, SDL_GetPixelFormatEnumForMasks(q2icon64.bytes_per_pixel * 8, rmask, gmask, bmask, amask));
	SDL_SetWindowIcon(window, icon);
	SDL_DestroySurface(icon);
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
		last_display = SDL_GetDisplayForWindow(window);

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
	vid_highdpiaware = Cvar_Get("vid_highdpiaware", "1", CVAR_ARCHIVE);
	vid_rate = Cvar_Get("vid_rate", "-1", CVAR_ARCHIVE);

	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			Com_Printf("Couldn't init SDL video: %s.\n", SDL_GetError());

			return false;
		}

		SDL_Version version;

		SDL_GetVersion(&version);
		Com_Printf("-------- vid initialization --------\n");
		Com_Printf("SDL version is: %i.%i.%i\n", (int)version.major, (int)version.minor, (int)version.patch);
		Com_Printf("SDL video driver is \"%s\".\n", SDL_GetCurrentVideoDriver());

		SDL_DisplayID *displays;
		if ((displays = SDL_GetDisplays(&num_displays)) == NULL)
		{
			Com_Printf("Couldn't get number of displays: %s\n", SDL_GetError());
		}
		else
		{
			SDL_free(displays);
		}

		InitDisplayIndices();
		ClampDisplayIndexCvar();
		Com_Printf("SDL display modes:\n");

		PrintDisplayModes();
		Com_Printf("------------------------------------\n\n");
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
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	ClearDisplayIndices();
}

/*
 * Determine if we want to be high dpi aware. If
 * we are we must scale ourself. If we are not the
 * compositor might scale us.
 */
static int
Glimp_DetermineHighDPISupport(int flags)
{
	/* Make sure that high dpi is never set when we don't want it. */
	flags &= ~SDL_WINDOW_HIGH_PIXEL_DENSITY;

	if (vid_highdpiaware->value == 0)
	{
		return flags;
	}

	/* Handle high dpi awareness based on the render backend.
	   SDL doesn't support high dpi awareness for all backends
	   and the quality and behavior differs between them. */
	if ((strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0))
	{
		flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
	}

	return flags;
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

	if (fullscreen == FULLSCREEN_EXCLUSIVE || fullscreen == FULLSCREEN_DESKTOP)
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
		/* TODO SDL3: Leak? */
		const SDL_DisplayMode *closestMode = NULL;

		/* If we want fullscreen, but aren't */
		if (GetFullscreenType())
		{
			if (fullscreen == FULLSCREEN_EXCLUSIVE)
			{
				closestMode = SDL_GetClosestFullscreenDisplayMode(last_display, width, height, vid_rate->value, false);

				if (closestMode == NULL)
				{
					Com_Printf("SDL was unable to find a mode close to %ix%i@%f\n", width, height, vid_rate->value);

					if (vid_rate->value != 0)
					{
						Com_Printf("Retrying with desktop refresh rate\n");
						closestMode = SDL_GetClosestFullscreenDisplayMode(last_display, width, height, 0, false);

						if (closestMode != NULL)
						{
							Cvar_SetValue("vid_rate", 0);
						}
						else
						{
							Com_Printf("SDL was unable to find a mode close to %ix%i@0\n", width, height);
							return false;
						}
					}
				}
			}
			else if (fullscreen == FULLSCREEN_DESKTOP)
			{
				/* Fullscreen window */
				closestMode = NULL;
			}

			if (SDL_SetWindowFullscreenMode(window, closestMode) < 0)
			{
				Com_Printf("Couldn't set fullscreen modmode: %s\n", SDL_GetError());
				Cvar_SetValue("vid_fullscreen", 0);
			}
			else
			{
				if (SDL_SetWindowFullscreen(window, true) < 0)
				{
					Com_Printf("Couldn't switch to exclusive fullscreen: %s\n", SDL_GetError());
					Cvar_SetValue("vid_fullscreen", 0);
				}
				else
				{
					int ret = SDL_SyncWindow(window);

					if (ret > 0)
					{
						Com_Printf("Synchronizing window state timed out\n");
						Cvar_SetValue("vid_fullscreen", 0);
					}
					else if (ret < 0)
					{
						Com_Printf("Couldn't synchronize window state: %s\n", SDL_GetError());
						Cvar_SetValue("vid_fullscreen", 0);
					}
				}
			}

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

	/* Check for high dpi support. */
	flags = Glimp_DetermineHighDPISupport(flags);

	/* Mkay, now the hard work. Let's create the window. */
	cvar_t *gl_msaa_samples = Cvar_Get("r_msaa_samples", "0", CVAR_ARCHIVE);

	while (1)
	{
		if (!CreateSDLWindow(flags, fullscreen, width, height))
		{
			if((flags & SDL_WINDOW_OPENGL) && gl_msaa_samples->value)
			{
				int msaa_samples = gl_msaa_samples->value;

				if (msaa_samples > 0)
				{
					msaa_samples /= 2;
				}

				Com_Printf("SDL SetVideoMode failed: %s\n", SDL_GetError());
				Com_Printf("Reverting to %s r_mode %i (%ix%i) with %dx MSAA.\n",
					        (flags & fs_flag) ? "fullscreen" : "windowed",
					        (int) Cvar_VariableValue("r_mode"), width, height,
					        msaa_samples);

				/* Try to recover */
				Cvar_SetValue("r_msaa_samples", msaa_samples);

				SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,
					msaa_samples > 0 ? 1 : 0);
				SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,
					msaa_samples);
			}
			else if (width != 640 || height != 480 || (flags & fs_flag))
			{
				Com_Printf("SDL SetVideoMode failed: %s\n", SDL_GetError());
				Com_Printf("Reverting to windowed r_mode 4 (640x480).\n");

				/* Try to recover */
				Cvar_SetValue("r_mode", 4);
				Cvar_SetValue("vid_fullscreen", 0);
				Cvar_SetValue("vid_rate", -1);

				fullscreen = FULLSCREEN_OFF;
				*pwidth = width = 640;
				*pheight = height = 480;
				flags &= ~fs_flag;
			}
			else
			{
				Com_Printf("Failed to revert to r_mode 4. Will try another render backend...\n");
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
	int curdisplay;
	if ((curdisplay = SDL_GetDisplayForWindow(window)) == 0)
	{
		/* There are some obscure setups were SDL is
		   unable to get the current display,one X11
		   server with several screen is one of these,
		   so add a fallback to the first display. */
		curdisplay = SDL_GetPrimaryDisplay();
	}

	const SDL_DisplayMode *mode;
	if ((mode = SDL_GetCurrentDisplayMode(curdisplay)) == NULL)
	{
		Com_Printf("Couldn't get current display mode: %s\n", SDL_GetError());
	}
	else
	{
		Com_Printf("Real display mode: %ix%i@%.2f\n", mode->w, mode->h, mode->refresh_rate);
	}


    /* Initialize rendering context. */
	if (!re.InitContext(window))
	{
		/* InitContext() should have logged an error. */
		return false;
	}

	/* We need the actual drawable size for things like the
	   console, the menus, etc. This might be different to
	   the resolution due to high dpi awareness.

	   The fullscreen window is special. We want it to fill
	   the screen when native resolution is requestes, all
	   other cases should look broken. */
	if (flags & SDL_WINDOW_HIGH_PIXEL_DENSITY)
	{
		if (fullscreen != FULLSCREEN_DESKTOP)
		{
			re.GetDrawableSize(&viddef.width, &viddef.height);
		}
		else
		{
			cvar_t *r_mode = Cvar_Get("r_mode", "4", 0);

			if (r_mode->value == -2 )
			{
				re.GetDrawableSize(&viddef.width, &viddef.height);
			}
			else
			{
				/* User likes it broken. */
				viddef.width = *pwidth;
				viddef.height = *pheight;
			}
		}
	}
	else
	{
		/* Another bug or design failure in SDL: When we are
		   not high dpi aware the drawable size returned by
		   SDL may be too small. It seems like the window
		   decoration are taken into account when they shouldn't.
		   It can be seen when creating a fullscreen window.

		   Work around that by always using the resolution and
		   not the drawable size when we are not high dpi aware. */
		viddef.width = *pwidth;
		viddef.height = *pheight;
	}

	Com_Printf("Drawable size: %ix%i\n", viddef.width, viddef.height);

	/* Set the window icon - For SDL2, this must be done after creating the window */
	SetSDLIcon();

	/* No cursor */
	SDL_ShowCursor();

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
		SDL_SetWindowMouseGrab(window, grab ? SDL_TRUE : SDL_FALSE);
	}

	if(SDL_SetRelativeMouseMode(grab ? SDL_TRUE : SDL_FALSE) < 0)
	{
		Com_Printf("WARNING: Setting Relative Mousemode failed, reason: %s\n", SDL_GetError());
		Com_Printf("         You should probably update to SDL 2.0.3 or newer!\n");
	}
}

/*
 * Returns the current display refresh rate.
 */
float
GLimp_GetRefreshRate(void)
{

	if (vid_displayrefreshrate->value > -1 ||
			vid_displayrefreshrate->modified)
	{
		glimp_refreshRate = vid_displayrefreshrate->value;
		vid_displayrefreshrate->modified = false;
	}
	else if (glimp_refreshRate == -1)
	{
		const SDL_DisplayMode *mode;
		int curdisplay;

		if (window == NULL)
		{
			/* This is paranoia. This function should only be
			   called if there is a working window. Otherwise
			   things will likely break somewhere else in the
			   client. */
			curdisplay = SDL_GetPrimaryDisplay();
		}
		else
		{
			if ((curdisplay = SDL_GetDisplayForWindow(window)) == 0)
			{
				/* There are some obscure setups were SDL is
				   unable to get the current display,one X11
				   server with several screen is one of these,
				   so add a fallback to the first display. */
				curdisplay = SDL_GetPrimaryDisplay();
			}

		}

		if ((mode = SDL_GetCurrentDisplayMode(curdisplay)) == NULL)
		{
			printf("Couldn't get display refresh rate: %s\n", SDL_GetError());
		}
		else
		{
			glimp_refreshRate = mode->refresh_rate;
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
	if (window == NULL)
	{
		/* Renderers call into this function before the
		   window is created. This could be refactored
		   by passing the mode and not the geometry from
		   the renderer to GLimp_InitGraphics(), however
		   that would break the renderer API. */
		last_display = SDL_GetPrimaryDisplay();
	}
	else
	{
		/* save current display as default */
		if ((last_display = SDL_GetDisplayForWindow(window)) == 0)
		{
			/* There are some obscure setups were SDL is
			   unable to get the current display,one X11
			   server with several screen is one of these,
			   so add a fallback to the first display. */
			last_display = SDL_GetPrimaryDisplay();
		}

		SDL_GetWindowPosition(window, &last_position_x, &last_position_y);
	}

	const SDL_DisplayMode *mode;

	if ((mode = SDL_GetCurrentDisplayMode(last_display)) == NULL)
	{
		Com_Printf("Couldn't detect default desktop mode: %s\n", SDL_GetError());
		return false;
	}

	*pwidth = mode->w;
	*pheight = mode->h;

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

int
GLimp_GetFrameworkVersion(void)
{
	SDL_Version ver;
	SDL_VERSION(&ver);

	return ver.major;
}
