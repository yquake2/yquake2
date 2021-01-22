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
 * SDL backend for the GL3 renderer. Everything that needs to be on the
 * renderer side of thing. Also all glad (or whatever OpenGL loader I
 * end up using) specific things.
 *
 * =======================================================================
 */

#include "header/local.h"

#include <SDL2/SDL.h>

static SDL_Window* window = NULL;
static SDL_GLContext context = NULL;
static qboolean vsyncActive = false;

// --------

enum {
	// Not all GL.h header know about GL_DEBUG_SEVERITY_NOTIFICATION_*.
	QGL_DEBUG_SEVERITY_NOTIFICATION = 0x826B
};

/*
 * Callback function for debug output.
 */
static void APIENTRY
DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
              const GLchar *message, const void *userParam)
{
	const char* sourceStr = "Source: Unknown";
	const char* typeStr = "Type: Unknown";
	const char* severityStr = "Severity: Unknown";

	switch (severity)
	{
		case QGL_DEBUG_SEVERITY_NOTIFICATION:
			return;

		case GL_DEBUG_SEVERITY_HIGH_ARB:   severityStr = "Severity: High";   break;
		case GL_DEBUG_SEVERITY_MEDIUM_ARB: severityStr = "Severity: Medium"; break;
		case GL_DEBUG_SEVERITY_LOW_ARB:    severityStr = "Severity: Low";    break;
	}

	switch (source)
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

// ---------

/*
 * Swaps the buffers and shows the next frame.
 */
void GL3_EndFrame(void)
{
	if(gl3config.useBigVBO)
	{
		// I think this is a good point to orphan the VBO and get a fresh one
		GL3_BindVAO(gl3state.vao3D);
		GL3_BindVBO(gl3state.vbo3D);
		glBufferData(GL_ARRAY_BUFFER, gl3state.vbo3Dsize, NULL, GL_STREAM_DRAW);
		gl3state.vbo3DcurOffset = 0;
	}

	SDL_GL_SwapWindow(window);
}

/*
 * Returns whether the vsync is enabled.
 */
qboolean GL3_IsVsyncActive(void)
{
	return vsyncActive;
}

/*
 * Enables or disabes the vsync.
 */
void GL3_SetVsync(void)
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
 * This function returns the flags used at the SDL window
 * creation by GLimp_InitGraphics(). In case of error -1
 * is returned.
 */
int GL3_PrepareForWindow(void)
{
	// Mkay, let's try to load the libGL,
	const char *libgl;
	cvar_t *gl3_libgl = ri.Cvar_Get("gl3_libgl", "", CVAR_ARCHIVE);

	if (strlen(gl3_libgl->string) == 0)
	{
		libgl = NULL;
	}
	else
	{
		libgl = gl3_libgl->string;
	}

	while (1)
	{
		if (SDL_GL_LoadLibrary(libgl) < 0)
		{
			if (libgl == NULL)
			{
				ri.Sys_Error(ERR_FATAL, "Couldn't load libGL: %s!", SDL_GetError());

				return -1;
			}
			else
			{
				R_Printf(PRINT_ALL, "Couldn't load libGL: %s!\n", SDL_GetError());
				R_Printf(PRINT_ALL, "Retrying with default...\n");

				ri.Cvar_Set("gl3_libgl", "");
				libgl = NULL;
			}
		}
		else
		{
			break;
		}
	}

	// Set GL context attributs bound to the window.
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	if (SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8) == 0)
	{
		gl3config.stencil = true;
	}
	else
	{
		gl3config.stencil = false;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	// Set GL context flags.
	int contextFlags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;

	if (gl3_debugcontext && gl3_debugcontext->value)
	{
		contextFlags |= SDL_GL_CONTEXT_DEBUG_FLAG;
	}

	if (contextFlags != 0)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
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
 * Initializes the OpenGL context. Returns true at
 * success and false at failure.
 */
int GL3_InitContext(void* win)
{
	// Coders are stupid.
	if (win == NULL)
	{
		ri.Sys_Error(ERR_FATAL, "R_InitContext() must not be called with NULL argument!");

		return false;
	}

	window = (SDL_Window *)win;

	// Initialize GL context.
	context = SDL_GL_CreateContext(window);

	if(context == NULL)
	{
		R_Printf(PRINT_ALL, "GL3_InitContext(): Creating OpenGL Context failed: %s\n", SDL_GetError());

		window = NULL;

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

	// Check if we've got at least 8 stencil bits
	int stencil_bits = 0;

	if (gl3config.stencil)
	{
		if (SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits) < 0 || stencil_bits < 8)
		{
			gl3config.stencil = false;
		}
	}

	// Enable vsync if requested.
	GL3_SetVsync();

	// Load GL pointrs through GLAD and check context.
	if( !gladLoadGLLoader(SDL_GL_GetProcAddress))
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

	// Debug context setup.
	if (gl3_debugcontext && gl3_debugcontext->value && gl3config.debug_output)
	{
		glDebugMessageCallbackARB(DebugCallback, NULL);

		// Call GL3_DebugCallback() synchronously, i.e. directly when and
		// where the error happens (so we can get the cause in a backtrace)
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
	}

	// Window title - set here so we can display renderer name in it.
	char title[40] = {0};

	snprintf(title, sizeof(title), "Yamagi Quake II %s - OpenGL 3.2", YQ2VERSION);
	SDL_SetWindowTitle(window, title);

	return true;
}

/*
 * Shuts the GL context down.
 */
void GL3_ShutdownContext()
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
