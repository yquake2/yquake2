// SDL-specific OpenGL shit formerly in refresh.c

#include "../header/local.h"

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

/* X.org stuff - put this here as more modern renderers wouldn't need it
 * e.g. an OpenGL3 renderer could just apply gamma through a shader */
#ifdef X11GAMMA
 #include <X11/Xos.h>
 #include <X11/Xlib.h>
 #include <X11/Xutil.h>
 #include <X11/extensions/xf86vmode.h>
 #include <X11/extensions/Xrandr.h>

 #include <SDL_syswm.h>

 XRRCrtcGamma** gammaRamps = NULL;
 int noGammaRamps = 0;
#endif


#if SDL_VERSION_ATLEAST(2, 0, 0)
 static SDL_Window* window = NULL;
 static SDL_GLContext context = NULL;
#else
static SDL_Surface* window = NULL;
#endif

qboolean have_stencil = false;

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
		ri.Con_Printf(PRINT_ALL, "Couldn't get Window info from SDL\n");
		return;
	}

	dpy = info.info.x11.display;

	XRRScreenResources* res = XRRGetScreenResources(dpy, info.info.x11.window);
	if(res == NULL)
	{
		ri.Con_Printf(PRINT_ALL, "Unable to get xrandr screen resources.\n");
		return;
	}

	for(i=0; i < res->ncrtc; ++i)
	{
		int len = XRRGetCrtcGammaSize(dpy, res->crtcs[i]);
		size_t rampSize = len*sizeof(Uint16);
		Uint16* ramp = malloc(rampSize); // TODO: check for NULL
		if(ramp == NULL)
		{
			ri.Con_Printf(PRINT_ALL, "Couldn't allocate &zd byte of memory for gamma ramp - OOM?!\n", rampSize);
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
		ri.Con_Printf(PRINT_ALL, "Setting gamma failed: %s\n", SDL_GetError());
	}
}
#endif // X11GAMMA

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
		ri.Con_Printf(PRINT_ALL, "Couldn't get Window info from SDL\n");
		return;
	}

	dpy = info.info.x11.display;

	XRRScreenResources* res = XRRGetScreenResources(dpy, info.info.x11.window);
	if(res == NULL)
	{
		ri.Con_Printf(PRINT_ALL, "Unable to get xrandr screen resources.\n");
		return;
	}

	noGammaRamps = res->ncrtc;
	gammaRamps = calloc(noGammaRamps, sizeof(XRRCrtcGamma*));
	if(gammaRamps == NULL) {
		ri.Con_Printf(PRINT_ALL, "Couldn't allocate memory for %d gamma ramps - OOM?!\n", noGammaRamps);
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

	ri.Con_Printf(PRINT_ALL, "Using hardware gamma via X11/xRandR.\n");
#elif __APPLE__
	gl_state.hwgamma = false;
	ri.Con_Printf(PRINT_ALL, "Using software gamma (needs vid_restart after changes)\n");
	return;
#else
	ri.Con_Printf(PRINT_ALL, "Using hardware gamma via SDL.\n");
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
		ri.Con_Printf(PRINT_ALL, "Couldn't get Window info from SDL\n");
		return;
	}

	dpy = info.info.x11.display;

	XRRScreenResources* res = XRRGetScreenResources(dpy, info.info.x11.window);
	if(res == NULL)
	{
		ri.Con_Printf(PRINT_ALL, "Unable to get xrandr screen resources.\n");
		return;
	}

	for(i=0; i < noGammaRamps; ++i)
	{
		// in case a display was unplugged or something, noGammaRamps may be > res->ncrtc
		if(i < res->ncrtc)
		{
			int len = XRRGetCrtcGammaSize(dpy, res->crtcs[i]);
			if(len != gammaRamps[i]->size) {
				ri.Con_Printf(PRINT_ALL, "WTF, gamma ramp size for display %d has changed from %d to %d!\n",
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

	ri.Con_Printf(PRINT_ALL, "Restored original Gamma\n");
}
#endif // X11GAMMA


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
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, gl_swapinterval->value ? 1 : 0);
#endif

	if (gl_msaa_samples->value)
	{
		msaa_samples = gl_msaa_samples->value;

		if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1) < 0)
		{
			ri.Con_Printf(PRINT_ALL, "MSAA is unsupported: %s\n", SDL_GetError());
			ri.Cvar_SetValue ("gl_msaa_samples", 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
		}
		else if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa_samples) < 0)
		{
			ri.Con_Printf(PRINT_ALL, "MSAA %ix is unsupported: %s\n", msaa_samples, SDL_GetError());
			ri.Cvar_SetValue("gl_msaa_samples", 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
		}
	}

	/* Initiate the flags */
#if SDL_VERSION_ATLEAST(2, 0, 0)
	flags = SDL_WINDOW_OPENGL;
#else // SDL 1.2
	flags = SDL_OPENGL;
#endif

	return flags;
}

int RI_InitContext(void* win)
{
	int msaa_samples = 0, stencil_bits = 0;

	if(win == NULL)
	{
		ri.Sys_Error(ERR_FATAL, "R_InitContext() must not be called with NULL argument!");
		return false;
	}

	window = (SDL_Window*)win;
	context = SDL_GL_CreateContext(window);
	if(context == NULL)
	{
		ri.Con_Printf(PRINT_ALL, "R_InitContext(): Creating OpenGL Context failed: %s\n", SDL_GetError());
		window = NULL;
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
	/* Set vsync - TODO: -1 could be set for "late swap tearing" */
	SDL_GL_SetSwapInterval(gl_swapinterval->value ? 1 : 0);
#endif

	/* Initialize the stencil buffer */
	if (!SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits))
	{
		ri.Con_Printf(PRINT_ALL, "Got %d bits of stencil.\n", stencil_bits);

		if (stencil_bits >= 1)
		{
			have_stencil = true;
		}
	}

	/* Initialize hardware gamma */
	InitGamma();

	return true;
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
#ifdef X11GAMMA
	RestoreGamma();
#endif

	/* Clear the backbuffer and make it
	   current. This may help some broken
	   video drivers like the AMD Catalyst
	   to avoid artifacts in unused screen
	   areas. */
	if (window)
	{
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		RI_EndFrame();

#if SDL_VERSION_ATLEAST(2, 0, 0)
		if(context)
		{
			SDL_GL_DeleteContext(context);
			context = NULL;
		}
#endif
	}

	window = NULL;

	gl_state.hwgamma = false;

	if(!contextOnly)
	{
		ri.Vid_ShutdownWindow();
	}
}
