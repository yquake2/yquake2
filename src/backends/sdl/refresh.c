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
 * ----------------------------------------------------------------------
 *
 * CalculateGammaRamp() is derived from SDL2's SDL_CalculateGammaRamp()
 * (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>
 * Published under zlib License: http://www.libsdl.org/license.php
 *
 * =======================================================================
 *
 * This file implements an OpenGL context and window handling through
 * SDL. The code is complicated by supporting the fairly different SDL
 * 1.2 and SDL 2 APIs, each with hardware gamma or software gamma by
 * RANDR.
 *
 * =======================================================================
 */

#include "../../client/refresh/header/local.h"
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#if defined(_WIN32) || defined(__APPLE__)
#ifdef SDL2
#include <SDL2/SDL.h>
#else // SDL1.2
#include <SDL/SDL.h>
#endif //SDL2
#else // not _WIN32 || APPLE
#include <SDL.h>
#endif // _WIN32 || APPLE

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
extern qboolean in_relativemode;
SDL_Window* window = NULL;
SDL_GLContext context = NULL;
#else
SDL_Surface* window = NULL;
#endif

qboolean have_stencil = false;

#ifdef X11GAMMA
XRRCrtcGamma** gammaRamps = NULL;
int noGammaRamps = 0;
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
		SDL_VideoDriverName(driverName, sizeof(driverName));
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

/*
 * Sets the hardware gamma
 */
#ifdef X11GAMMA
void
UpdateHardwareGamma(void)
{
	float gamma = (vid_gamma->value);
	int i;

	Display* dpy = NULL;
	SDL_SysWMinfo info;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_VERSION(&info.version);
	if(!SDL_GetWindowWMInfo(window, &info))
#else
	if(SDL_GetWMInfo(&info) != 1)
#endif
	{
		VID_Printf(PRINT_ALL, "Couldn't get Window info from SDL\n");
		return;
	}

	dpy = info.info.x11.display;

	XRRScreenResources* res = XRRGetScreenResources(dpy, info.info.x11.window);
	if(res == NULL)
	{
		VID_Printf(PRINT_ALL, "Unable to get xrandr screen resources.\n");
		return;
	}

	for(i=0; i < res->ncrtc; ++i)
	{
		int len = XRRGetCrtcGammaSize(dpy, res->crtcs[i]);
		size_t rampSize = len*sizeof(Uint16);
		Uint16* ramp = malloc(rampSize); // TODO: check for NULL
		if(ramp == NULL)
		{
			VID_Printf(PRINT_ALL, "Couldn't allocate &zd byte of memory for gamma ramp - OOM?!\n", rampSize);
			return;
		}

		CalculateGammaRamp(gamma, ramp, len);

		XRRCrtcGamma* gamma = XRRAllocGamma(len);

		memcpy(gamma->red, ramp, rampSize);
		memcpy(gamma->green, ramp, rampSize);
		memcpy(gamma->blue, ramp, rampSize);

		free(ramp);

		XRRSetCrtcGamma(dpy, res->crtcs[i], gamma);

		XRRFreeGamma(gamma);
	}

	XRRFreeScreenResources(res);
}
#else // no X11GAMMA
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
		VID_Printf(PRINT_ALL, "Setting gamma failed: %s\n", SDL_GetError());
	}
}
#endif // X11GAMMA

static qboolean IsFullscreen()
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	return !!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN);
#else
	return !!(window->flags & SDL_FULLSCREEN);
#endif
}

static qboolean CreateSDLWindow(int flags)
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

static void InitGamma()
{
#ifdef X11GAMMA
	int i=0;
	SDL_SysWMinfo info;
	Display* dpy = NULL;

	if(gammaRamps != NULL) // already saved gamma
		return;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_VERSION(&info.version);
	if(!SDL_GetWindowWMInfo(window, &info))
#else
	if(SDL_GetWMInfo(&info) != 1)
#endif
	{
		VID_Printf(PRINT_ALL, "Couldn't get Window info from SDL\n");
		return;
	}

	dpy = info.info.x11.display;

	XRRScreenResources* res = XRRGetScreenResources(dpy, info.info.x11.window);
	if(res == NULL)
	{
		VID_Printf(PRINT_ALL, "Unable to get xrandr screen resources.\n");
		return;
	}

	noGammaRamps = res->ncrtc;
	gammaRamps = calloc(noGammaRamps, sizeof(XRRCrtcGamma*));
	if(gammaRamps == NULL) {
		VID_Printf(PRINT_ALL, "Couldn't allocate memory for %d gamma ramps - OOM?!\n", noGammaRamps);
		return;
	}

	for(i=0; i < noGammaRamps; ++i)
	{
		int len = XRRGetCrtcGammaSize(dpy, res->crtcs[i]);
		size_t rampSize = len*sizeof(Uint16);

		XRRCrtcGamma* origGamma = XRRGetCrtcGamma(dpy, res->crtcs[i]);

		XRRCrtcGamma* gammaCopy = XRRAllocGamma(len);

		memcpy(gammaCopy->red, origGamma->red, rampSize);
		memcpy(gammaCopy->green, origGamma->green, rampSize);
		memcpy(gammaCopy->blue, origGamma->blue, rampSize);

		gammaRamps[i] = gammaCopy;
	}

	XRRFreeScreenResources(res);

	VID_Printf(PRINT_ALL, "Using hardware gamma via X11/xRandR.\n");

#else
	VID_Printf(PRINT_ALL, "Using hardware gamma via SDL.\n");
#endif
	gl_state.hwgamma = true;
	vid_gamma->modified = true;
}

#ifdef X11GAMMA
static void RestoreGamma()
{
	int i=0;
	SDL_SysWMinfo info;
	Display* dpy = NULL;

	if(gammaRamps == NULL)
			return;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_VERSION(&info.version);
	if(!SDL_GetWindowWMInfo(window, &info))
#else
	if(SDL_GetWMInfo(&info) != 1)
#endif
	{
		VID_Printf(PRINT_ALL, "Couldn't get Window info from SDL\n");
		return;
	}

	dpy = info.info.x11.display;

	XRRScreenResources* res = XRRGetScreenResources(dpy, info.info.x11.window);
	if(res == NULL)
	{
		VID_Printf(PRINT_ALL, "Unable to get xrandr screen resources.\n");
		return;
	}

	for(i=0; i < noGammaRamps; ++i)
	{
		// in case a display was unplugged or something, noGammaRamps may be > res->ncrtc
		if(i < res->ncrtc)
		{
			int len = XRRGetCrtcGammaSize(dpy, res->crtcs[i]);
			if(len != gammaRamps[i]->size) {
				VID_Printf(PRINT_ALL, "WTF, gamma ramp size for display %d has changed from %d to %d!\n",
							   i, gammaRamps[i]->size, len);

				continue;
			}

			XRRSetCrtcGamma(dpy, res->crtcs[i], gammaRamps[i]);
		}

		// the ramp needs to be free()d either way
		XRRFreeGamma(gammaRamps[i]);
		gammaRamps[i] = NULL;

	}
	XRRFreeScreenResources(res);
	free(gammaRamps);
	gammaRamps = NULL;

	VID_Printf(PRINT_ALL, "Restored original Gamma\n");
}
#endif // X11GAMMA

/*
 * Initializes the OpenGL window
 */
static qboolean
GLimp_InitGraphics(qboolean fullscreen)
{
	int flags;
	int msaa_samples;
	int stencil_bits;
	int width, height;
	char title[24];

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

	if (gl_msaa_samples->value)
	{
		msaa_samples = gl_msaa_samples->value;

		if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1) < 0)
		{
			Com_Printf("MSAA is unsupported: %s\n", SDL_GetError());
			Cvar_SetValue ("gl_msaa_samples", 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
		}
		else if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa_samples) < 0)
		{
			Com_Printf("MSAA %ix is unsupported: %s\n", msaa_samples, SDL_GetError());
			Cvar_SetValue("gl_msaa_samples", 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
		}
	}

	/* Initiate the flags */
	flags = SDL_OPENGL;

	if (fullscreen)
	{
		flags |= SDL_FULLSCREEN;
	}

#if !SDL_VERSION_ATLEAST(2, 0, 0)
	/* For SDL1.2, these things must be done before creating the window */

	/* Set the icon */
	SetSDLIcon();

	/* Set vsync */
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, gl_swapinterval->value ? 1 : 0);
#endif

	while (1)
	{
		if (!CreateSDLWindow(flags))
		{
			if (gl_msaa_samples->value)
			{
				VID_Printf(PRINT_ALL, "SDL SetVideoMode failed: %s\n",
						SDL_GetError());
				VID_Printf(PRINT_ALL, "Reverting to %s gl_mode %i (%ix%i) without MSAA.\n",
						(flags & SDL_FULLSCREEN) ? "fullscreen" : "windowed",
						(int)Cvar_VariableValue("gl_mode"), vid.width, vid.height);

				/* Try to recover */
				Cvar_SetValue("gl_msaa_samples", 0);
				SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
				SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
			}
			else if (vid.width != 640 || vid.height != 480 || (flags & SDL_FULLSCREEN))
			{
				VID_Printf(PRINT_ALL, "SDL SetVideoMode failed: %s\n",
						SDL_GetError());
				VID_Printf(PRINT_ALL, "Reverting to windowed gl_mode 4 (640x480).\n");

				/* Try to recover */
				Cvar_SetValue("gl_mode", 4);
				Cvar_SetValue("vid_fullscreen", 0);
				vid.width = 640;
				vid.height = 480;
				flags &= ~SDL_FULLSCREEN;
			}
			else
			{
				VID_Error(ERR_FATAL, "Failed to revert to gl_mode 4. Exiting...\n");
				return false;
			}
		}
		else
		{
			break;
		}
	}
	if (gl_msaa_samples->value)
	{
		if (SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &msaa_samples) == 0)
		{
			Cvar_SetValue("gl_msaa_samples", msaa_samples);
		}
	}

#if SDL_VERSION_ATLEAST(2, 0, 0)
	/* For SDL2, these things must be done after creating the window */

	/* Set the icon */
	SetSDLIcon();

	/* Set vsync - TODO: -1 could be set for "late swap tearing" */
	SDL_GL_SetSwapInterval(gl_swapinterval->value ? 1 : 0);
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
	InitGamma();

	/* Window title */
	snprintf(title, sizeof(title), "Yamagi Quake II %s", YQ2VERSION);
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
	SDL_SetRelativeMouseMode(grab ? SDL_TRUE : SDL_FALSE);
	in_relativemode = (SDL_GetRelativeMouseMode() == SDL_TRUE);
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
	return (SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON);
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

#ifdef X11GAMMA
	RestoreGamma();
#endif

	if (window)
	{
#if SDL_VERSION_ATLEAST(2, 0, 0)
		if(context)
		{
			SDL_GL_DeleteContext(context);
			context = NULL;
		}

		SDL_DestroyWindow(window);
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

	gl_state.hwgamma = false;
}

