/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016-2017 Daniel Gibson
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
 * SDL-specific refresher things, for OpenGL3
 * Also all glad (or whatever OpenGL loader I end up using) specific things
 * (I'd like to keep the OpenGL loader easily exchangable if possible)
 *
 * =======================================================================
 */


#include "header/local.h"

#include <SDL2/SDL.h>

static SDL_Window* window = NULL;
static SDL_GLContext context = NULL;
static qboolean vsyncActive = false;

qboolean have_stencil = false;

// called by GLimp_InitGraphics() before creating window,
// returns flags for SDL window creation, -1 on error
int GL3_PrepareForWindow(void)
{
	int msaa_samples = 0;

	if(SDL_GL_LoadLibrary(NULL) < 0) // Default OpenGL is fine.
	{
		// TODO: is there a better way?
		ri.Sys_Error(ERR_FATAL, "Couldn't load libGL: %s!", SDL_GetError());

		return -1;
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	int contextFlags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;

	if(gl3_debugcontext && gl3_debugcontext->value)
	{
		contextFlags |= SDL_GL_CONTEXT_DEBUG_FLAG;
	}
	if(contextFlags != 0)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
	}

	gl3config.compat_profile = false;

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

	return SDL_WINDOW_OPENGL;
}

enum {
	// for some reason my driver calls the DebugCallback with the following severity
	// even though I think it shouldn't for the extension I'm using?
	// anyway, my gl headers don't know GL_DEBUG_SEVERITY_NOTIFICATION_* so I define it here
	QGL_DEBUG_SEVERITY_NOTIFICATION = 0x826B
};

static void APIENTRY
DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
              const GLchar *message, const void *userParam)
{
	const char* sourceStr = "Source: Unknown";
	const char* typeStr = "Type: Unknown";
	const char* severityStr = "Severity: Unknown";

	switch(severity)
	{
		case QGL_DEBUG_SEVERITY_NOTIFICATION:
			// severityStr = "Severity: Note";    break;
			return; // ignore these

		case GL_DEBUG_SEVERITY_HIGH_ARB:   severityStr = "Severity: High";   break;
		case GL_DEBUG_SEVERITY_MEDIUM_ARB: severityStr = "Severity: Medium"; break;
		case GL_DEBUG_SEVERITY_LOW_ARB:    severityStr = "Severity: Low";    break;
	}

	switch(source)
	{
#define SRCCASE(X)  case GL_DEBUG_SOURCE_ ## X ## _ARB: sourceStr = "Source: " #X; break;

		SRCCASE(API);
		SRCCASE(WINDOW_SYSTEM);
		SRCCASE(SHADER_COMPILER);
		SRCCASE(THIRD_PARTY);
		SRCCASE(APPLICATION);
		SRCCASE(OTHER);

#undef SRCCASE
	}

	switch(type)
	{
#define TYPECASE(X)  case GL_DEBUG_TYPE_ ## X ## _ARB: typeStr = "Type: " #X; break;

		TYPECASE(ERROR);
		TYPECASE(DEPRECATED_BEHAVIOR);
		TYPECASE(UNDEFINED_BEHAVIOR);
		TYPECASE(PORTABILITY);
		TYPECASE(PERFORMANCE);
		TYPECASE(OTHER);

#undef TYPECASE
	}

	// use PRINT_ALL - this is only called with gl3_debugcontext != 0 anyway.
	R_Printf(PRINT_ALL, "GLDBG %s %s %s: %s\n", sourceStr, typeStr, severityStr, message);
}

int GL3_InitContext(void* win)
{
	int msaa_samples = 0, stencil_bits = 0;
	char title[40] = {0};

	if(win == NULL)
	{
		ri.Sys_Error(ERR_FATAL, "R_InitContext() must not be called with NULL argument!");

		return false;
	}

	window = (SDL_Window*)win;
	context = SDL_GL_CreateContext(window);

	if(context == NULL)
	{
		R_Printf(PRINT_ALL, "GL3_InitContext(): Creating OpenGL Context failed: %s\n", SDL_GetError());

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

	/* For SDL2, this must be done after creating the window */
	GL3_SetSwapInterval();

	/* Initialize the stencil buffer */
	if (SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits) == 0)
	{
		R_Printf(PRINT_ALL, "Got %d bits of stencil.\n", stencil_bits);

		if (stencil_bits >= 1)
		{
			have_stencil = true;
		}
	}

	if(!gladLoadGLLoader(SDL_GL_GetProcAddress))
	{
		R_Printf(PRINT_ALL, "GL3_InitContext(): ERROR: loading OpenGL function pointers failed!\n");

		return false;
	}
	else if (GLVersion.major < 3 || (GLVersion.major == 3 && GLVersion.minor < 2))
	{
		R_Printf(PRINT_ALL, "GL3_InitContext(): ERROR: glad only got GL version %d.%d!\n", GLVersion.major, GLVersion.minor);

		return false;
	}
	else
	{
		R_Printf(PRINT_ALL, "Successfully loaded OpenGL function pointers using glad, got version %d.%d!\n", GLVersion.major, GLVersion.minor);
	}

	gl3config.debug_output = GLAD_GL_ARB_debug_output != 0;
	gl3config.anisotropic = GLAD_GL_EXT_texture_filter_anisotropic != 0;

	gl3config.major_version = GLVersion.major;
	gl3config.minor_version = GLVersion.minor;

	if(gl3_debugcontext && gl3_debugcontext->value && gl3config.debug_output)
	{
		glDebugMessageCallbackARB(DebugCallback, NULL);
		// call GL3_DebugCallback() synchronously, i.e. directly when and where the error happens
		// (so we can get the cause in a backtrace)
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB); // TODO: only do this if gl3_debugcontext->value >= 2 ?

		// TODO: the following line could control verboseness (in that case we'd get all the low prio messages)
		// glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW_ARB, 0, NULL, true);
	}

	/* Window title - set here so we can display renderer name in it */
	snprintf(title, sizeof(title), "Yamagi Quake II %s - OpenGL 3.2", YQ2VERSION);
	SDL_SetWindowTitle(window, title);

	return true;
}

void GL3_SetSwapInterval(void)
{
	/* Set vsync - TODO: -1 could be set for "late swap tearing" */
	SDL_GL_SetSwapInterval(r_vsync->value ? 1 : 0);
	vsyncActive = SDL_GL_GetSwapInterval() != 0;
}

qboolean GL3_IsVsyncActive(void)
{
	return vsyncActive;
}

/*
 * Swaps the buffers to show the new frame
 */
void GL3_EndFrame(void)
{
	SDL_GL_SwapWindow(window);
}

/*
 * Shuts the SDL render backend down
 */
void GL3_ShutdownWindow(qboolean contextOnly)
{
	if (window)
	{
		if(context)
		{
			SDL_GL_DeleteContext(context);
			context = NULL;
		}

		if (!contextOnly)
		{
			ri.Vid_ShutdownWindow();

			window = NULL;
		}
	}
}
