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
 * This file implements an OpenGL context via SDL
 *
 * =======================================================================
 */

#include "../../client/refresh/header/local.h"
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#ifdef _WIN32
#include <SDL/SDL.h>
#elif defined(__APPLE__)
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

/* The window icon */
#include "icon/q2icon.xbm"

/* X.org stuff */
#ifdef X11GAMMA
 #include <X11/Xos.h>
 #include <X11/Xlib.h>
 #include <X11/Xutil.h>
 #include <X11/extensions/xf86vmode.h>
 #include <X11/extensions/Xrandr.h>

 #include <SDL_syswm.h>
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
SDL_Window* window = NULL;
SDL_GLContext context = NULL;
#else
SDL_Surface* window = NULL;
#endif

qboolean have_stencil = false;

char *displayname = NULL;
int screen = -1;

#ifdef X11GAMMA
Display *dpy;
XF86VidModeGamma x11_oldgamma;
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
// some compatibility defines
#define SDL_SRCCOLORKEY SDL_TRUE
#define SDL_FULLSCREEN SDL_WINDOW_FULLSCREEN
#define SDL_OPENGL SDL_WINDOW_OPENGL

#endif

/*
 * Initialzes the SDL OpenGL context
 */
int
GLimp_Init(void)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{

		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			VID_Printf(PRINT_ALL, "Couldn't init SDL video: %s.\n",
					SDL_GetError());
			return false;
		}
#if SDL_VERSION_ATLEAST(2, 0, 0)
		const char* driverName = SDL_GetCurrentVideoDriver();
#else
		char driverName[64];
		SDL_VideoDriverName(driverName, sizeof(driverName) - 1);
#endif
		VID_Printf(PRINT_ALL, "SDL video driver is \"%s\".\n", driverName);
	}

	return true;
}

/*
 * Returns the adress of a GL function
 */
void *
GLimp_GetProcAddress (const char* proc)
{
	return SDL_GL_GetProcAddress ( proc );
}

/*
 * Sets the window icon
 */
static void
SetSDLIcon()
{
	SDL_Surface *icon;
	SDL_Color transColor, solidColor;
	Uint8 *ptr;
	int i;
	int mask;

	icon = SDL_CreateRGBSurface(SDL_SWSURFACE,
			q2icon_width, q2icon_height, 8,
			0, 0, 0, 0);

	if (icon == NULL)
	{
		return;
	}

	SDL_SetColorKey(icon, SDL_SRCCOLORKEY, 0);

	transColor.r = 255;
	transColor.g = 255;
	transColor.b = 255;

	solidColor.r = 0;
	solidColor.g = 16;
	solidColor.b = 0;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	// only SDL2 has alphas there and they must be set apparently
	transColor.a = 0;
	solidColor.a = 255;

	SDL_Palette* palette = SDL_AllocPalette(256);
	SDL_SetPaletteColors(palette, &transColor, 0, 1);
	SDL_SetPaletteColors(palette, &solidColor, 1, 1);

	SDL_SetSurfacePalette(icon, palette);
#else
	SDL_SetColors(icon, &transColor, 0, 1);
	SDL_SetColors(icon, &solidColor, 1, 1);
#endif

	ptr = (Uint8 *)icon->pixels;

	for (i = 0; i < sizeof(q2icon_bits); i++)
	{
		for (mask = 1; mask != 0x100; mask <<= 1)
		{
			*ptr = (q2icon_bits[i] & mask) ? 1 : 0;
			ptr++;
		}
	}
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_SetWindowIcon(window, icon);
	SDL_FreePalette(palette);
#else
	SDL_WM_SetIcon(icon, NULL);
#endif

	SDL_FreeSurface(icon);
}

/*
 * Sets the hardware gamma
 */
#ifdef X11GAMMA
void
UpdateHardwareGamma(void)
{
#if 1
	float gamma = (vid_gamma->value);
	// RANDR implementation
	int i;
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	SDL_GetWindowWMInfo(window, &info); // TODO: check return val

	XRRScreenResources* res = XRRGetScreenResources(info.info.x11.display, info.info.x11.window);

	Uint16 ramp[256];
	SDL_CalculateGammaRamp(gamma, ramp);
	size_t rampSize = 256*sizeof(Uint16);

	for(i=0; i < res->ncrtc; ++i)
	{
		XRRCrtcGamma* gamma = XRRAllocGamma(256);

		memcpy(gamma->red, ramp, rampSize);
		memcpy(gamma->green, ramp, rampSize);
		memcpy(gamma->blue, ramp, rampSize);
		XRRSetCrtcGamma(info.info.x11.display, res->crtcs[i], gamma);
		XRRFreeGamma(gamma);
	}

	XRRFreeScreenResources(res);

#else
	float gamma;
	XF86VidModeGamma x11_gamma;

	gamma = vid_gamma->value;

	x11_gamma.red = gamma;
	x11_gamma.green = gamma;
	x11_gamma.blue = gamma;

	XF86VidModeSetGamma(dpy, screen, &x11_gamma);

	/* This forces X11 to update the gamma tables */
	XF86VidModeGetGamma(dpy, screen, &x11_gamma);
#endif
}

#else
void
UpdateHardwareGamma(void)
{
	// FIXME: SDL expects a value between 0 and 1, X11 not?
	float gamma = (vid_gamma->value);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	// FIXME: does this work?
	Uint16 ramp[256];
	SDL_CalculateGammaRamp(gamma, ramp);
	if(SDL_SetWindowGammaRamp(window, ramp, ramp, ramp) != 0) {
		printf("## setting gamma failed: %s\n", SDL_GetError());
	}
#else
	SDL_SetGamma(gamma, gamma, gamma);
#endif
}
#endif

static qboolean IsFullscreen()
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	return !!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN);
#else
	return !!(window->flags & SDL_FULLSCREEN);
#endif
}

static qboolean CreateWindow(int flags)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	int windowPos = SDL_WINDOWPOS_UNDEFINED;
	// TODO: support fullscreen on different displays with SDL_WINDOWPOS_UNDEFINED_DISPLAY(displaynum)
	window = SDL_CreateWindow("Yamagi Quake II", windowPos, windowPos,
	                          vid.width, vid.height, flags);

	if(window == NULL)
	{
		return false;
	}

	context = SDL_GL_CreateContext(window);
	if(context == NULL)
	{
		SDL_DestroyWindow(window);
		window = NULL;
		return false;
	}

	// set vsync - TODO: -1 could be set for "late swap tearing",
	//  i.e. only vsync if framerate is high enough
	SDL_GL_SetSwapInterval(gl_swapinterval->value ? 1 : 0);

	return true;
#else
	window = SDL_SetVideoMode(vid.width, vid.height, 0, flags);
	return window != NULL;
#endif
}

static qboolean GetWindowSize(int* w, int* h)
{
	if(window == NULL || w == NULL || h == NULL)
		return false;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_DisplayMode m;
	if(SDL_GetWindowDisplayMode(window, &m) != 0)
	{
		VID_Printf(PRINT_ALL, "Can't get Displaymode: %s\n", SDL_GetError());
		return false;
	}
	*w = m.w;
	*h = m.h;
#else
	*w = window->w;
	*h = window->h;
#endif

	return true;
}

/*
 * Initializes the OpenGL window
 */
static qboolean
GLimp_InitGraphics(qboolean fullscreen)
{
	int counter = 0;
	int flags;
	int stencil_bits;
	char title[24];

	int width, height;

	if (GetWindowSize(&width, &height) && (width == vid.width) && (height == vid.height))
	{
		/* If we want fullscreen, but aren't */
		if (fullscreen != IsFullscreen())
		{
			GLimp_ToggleFullscreen();
		}

		/* Are we now? */
		if (fullscreen == IsFullscreen())
		{
			return true;
		}
	}

	/* Is the surface used? */
	if (window)
	{
#if SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_GL_DeleteContext(context);
		SDL_DestroyWindow(window);
#else
		SDL_FreeSurface(window);
#endif
	}

	/* Create the window */
	VID_NewWindow(vid.width, vid.height);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	/* Initiate the flags */
	flags = SDL_OPENGL;

	if (fullscreen)
	{
		flags |= SDL_FULLSCREEN;
	}

#if !SDL_VERSION_ATLEAST(2, 0, 0)
	/* Set the icon - for SDL1.2 this must be done before creating the window */
	SetSDLIcon();

	/* Enable vsync */
	if (gl_swapinterval->value)
	{
		SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
	}
#endif

	while (1)
	{
		if (!CreateWindow(flags))
		{
			if (counter == 1)
			{
				VID_Error(ERR_FATAL, "Failed to revert to gl_mode 4. Exiting...\n");
				return false;
			}

			VID_Printf(PRINT_ALL, "SDL SetVideoMode failed: %s\n",
					SDL_GetError());
			VID_Printf(PRINT_ALL, "Reverting to gl_mode 4 (640x480) and windowed mode.\n");

			/* Try to recover */
			Cvar_SetValue("gl_mode", 4);
			Cvar_SetValue("vid_fullscreen", 0);
			vid.width = 640;
			vid.height = 480;

			counter++;
			continue;
		}
		else
		{
			break;
		}
	}
#if SDL_VERSION_ATLEAST(2, 0, 0)
	/* Set the icon - for SDL2 this must be done after creating the window */
	SetSDLIcon();
#endif

	/* Initialize the stencil buffer */
	if (!SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits))
	{
		VID_Printf(PRINT_ALL, "Got %d bits of stencil.\n", stencil_bits);

		if (stencil_bits >= 1)
		{
			have_stencil = true;
		}
	}

	/* Initialize hardware gamma */
#ifdef X11GAMMA
	if ((dpy = XOpenDisplay(displayname)) == NULL)
	{
		VID_Printf(PRINT_ALL, "Unable to open display.\n");
	}
	else
	{
		if (screen == -1)
		{
			screen = DefaultScreen(dpy);
		}

		gl_state.hwgamma = true;
		vid_gamma->modified = true;

		XF86VidModeGetGamma(dpy, screen, &x11_oldgamma);

		VID_Printf(PRINT_ALL, "Using hardware gamma via X11.\n");
	}
#else
	gl_state.hwgamma = true;
	vid_gamma->modified = true;
	VID_Printf(PRINT_ALL, "Using hardware gamma via SDL.\n");
#endif

	/* Window title */
	snprintf(title, sizeof(title), "Yamagi Quake II %s", VERSION);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_SetWindowTitle(window, title);
#else
	SDL_WM_SetCaption(title, title);
#endif

	/* No cursor */
	SDL_ShowCursor(0);

	return true;
}

/*
 * Swaps the buffers to show the new frame
 */
void
GLimp_EndFrame(void)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_GL_SwapWindow(window);
#else
	SDL_GL_SwapBuffers();
#endif
}

/*
 * Changes the video mode
 */
int
GLimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen)
{
	VID_Printf(PRINT_ALL, "setting mode %d:", mode);

	/* mode -1 is not in the vid mode table - so we keep the values in pwidth
	   and pheight and don't even try to look up the mode info */
	if ((mode != -1) && !VID_GetModeInfo(pwidth, pheight, mode))
	{
		VID_Printf(PRINT_ALL, " invalid mode\n");
		return rserr_invalid_mode;
	}

	VID_Printf(PRINT_ALL, " %d %d\n", *pwidth, *pheight);

	if (!GLimp_InitGraphics(fullscreen))
	{
		return rserr_invalid_mode;
	}

	return rserr_ok;
}

/*
 * Toggle fullscreen.
 */
void GLimp_ToggleFullscreen(void)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	int wantFullscreen = !IsFullscreen();

	SDL_SetWindowFullscreen(window, wantFullscreen ? SDL_WINDOW_FULLSCREEN : 0);
	Cvar_SetValue("vid_fullscreen", wantFullscreen);
#else
	SDL_WM_ToggleFullScreen(window);

	if (IsFullscreen())
	{
		Cvar_SetValue("vid_fullscreen", 1);
	}
	else
	{
		Cvar_SetValue("vid_fullscreen", 0);
	}
#endif
	vid_fullscreen->modified = false;
}

/*
 * (Un)grab Input
 */
void GLimp_GrabInput(qboolean grab)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_SetWindowGrab(window, grab ? SDL_TRUE : SDL_FALSE);
	if(grab)
		SDL_SetRelativeMouseMode(SDL_TRUE);

#else
	SDL_WM_GrabInput(grab ? SDL_GRAB_ON : SDL_GRAB_OFF);
#endif
}

/*
 * returns true if input is grabbed, else false
 */
qboolean GLimp_InputIsGrabbed()
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	return SDL_GetWindowGrab(window) ? true : false;
#else
	SDL_GrabMode m = SDL_WM_GrabInput(SDL_GRAB_QUERY);
	return m == SDL_GRAB_ON;
#endif
}


/*
 * Shuts the SDL render backend down
 */
void
GLimp_Shutdown(void)
{
	/* Clear the backbuffer and make it
	   current. This may help some broken
	   video drivers like the AMD Catalyst
	   to avoid artifacts in unused screen
	   areas. */
	if (SDL_WasInit(SDL_INIT_VIDEO))
	{
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		GLimp_EndFrame();
	}

	if (window)
	{
#if SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_DestroyWindow(window);

		if(context)
		{
			SDL_GL_DeleteContext(context);
			context = NULL;
		}
#else
		SDL_FreeSurface(window);
#endif
	}

	window = NULL;

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
	{
		SDL_Quit();
	}
	else
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

#ifdef X11GAMMA
	if (gl_state.hwgamma == true)
	{
		XF86VidModeSetGamma(dpy, screen, &x11_oldgamma);

		/* This forces X11 to update the gamma tables */
		XF86VidModeGetGamma(dpy, screen, &x11_oldgamma);
	}
#endif

	gl_state.hwgamma = false;
}

