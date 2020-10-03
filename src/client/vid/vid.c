/*
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
 * API between the client and renderers.
 *
 * =======================================================================
 */

#include <assert.h>
#include <errno.h>

#include "../../client/header/client.h"
#include "../../client/header/keyboard.h"

// --------

// Screenshots
// -----------

#include "../../common/unzip/miniz.h"

static unsigned char*
compress_for_stbiw(unsigned char *data, int data_len, int *out_len, int quality)
{
	uLongf bufSize = compressBound(data_len);
	unsigned char* buf = malloc(bufSize);

	if (buf == NULL)
	{
		return NULL;
	}

	if (compress2(buf, &bufSize, data, data_len, quality) != Z_OK)
	{
		free(buf);
		return NULL;
	}

	*out_len = bufSize;

	return buf;
}

#define STBIW_ZLIB_COMPRESS compress_for_stbiw
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "header/stb_image_write.h"

/*
 * Writes a screenshot. This function is called with raw image data of
 * width*height pixels, each pixel has comp bytes. Must be 3 or 4, for
 * RGB or RGBA. The pixels must be given row-wise, stating at the top
 * left.
 */
void VID_WriteScreenshot(int width, int height, int comp, const void* data)
{
	char picname[80];
	char checkname[MAX_OSPATH];
	int i, success = 0;
	static const char* supportedFormats[] = { "tga", "bmp", "png", "jpg" };
	static const int numFormats = sizeof(supportedFormats)/sizeof(supportedFormats[0]);
	int format = 0; // 0=tga, 1=bmp, 2=png, 3=jpg
	int quality = 85;
	int argc = Cmd_Argc();
	const char* gameDir = FS_Gamedir();

	// FS_InitFilesystem() made sure the screenshots dir exists./

	if (argc > 1)
	{
		const char* maybeFormat = Cmd_Argv(1);

		for (i = 0; i < numFormats; ++i)
		{
			if (Q_stricmp(maybeFormat, supportedFormats[i]) == 0)
			{
				format = i;
				break;
			}
		}

		if (i == numFormats)
		{
			Com_Printf("the (optional) second argument to 'screenshot' is the format, one of \"tga\", \"bmp\", \"png\", \"jpg\"\n");
			return;
		}

		if (argc > 2)
		{
			const char* q = Cmd_Argv(2);
			int qualityStrLen = strlen(q);

			for (i = 0; i < qualityStrLen; ++i)
			{
				if (q[i] < '0' || q[i] > '9')
				{
					Com_Printf("The (optional!) third argument to 'screenshot' is jpg quality, a number between 1 and 100\n");
					Com_Printf("  or png compression level, between 0 and 9!\n");

					return;
				}
			}

			quality = atoi(q);

			if (format == 2) // png
			{
				if (quality < 0)
				{
					quality = 0;
				}
				else if (quality > 9)
				{
					quality = 9;
				}
			}
			else if(format == 3) // jpg
			{
				if (quality < 1)
				{
					quality = 1;
				}
				else if (quality > 100)
				{
					quality = 100;
				}
			}
		}
	}

	/* find a file name to save it to */
	for (i = 0; i <= 9999; i++)
	{
		FILE *f;
		Com_sprintf(checkname, sizeof(checkname), "%s/scrnshot/q2_%04d.%s", gameDir, i, supportedFormats[format]);
		f = Q_fopen(checkname, "rb");

		if (!f)
		{
			Com_sprintf(picname, sizeof(picname), "q2_%04d.%s", i, supportedFormats[format]);
			break; /* file doesn't exist */
		}

		fclose(f);
	}

	if (i == 10000)
	{
		Com_Printf("SCR_ScreenShot_f: Couldn't create a file\n");
		return;
	}

	switch (format) // 0=tga, 1=bmp, 2=png, 3=jpg
	{
		case 0:
			success = stbi_write_tga(checkname, width, height, comp, data);
			break;
		case 1:
			success = stbi_write_bmp(checkname, width, height, comp, data);
			break;
		case 2:
			stbi_write_png_compression_level = (quality < 10) ? quality : 7;
			success = stbi_write_png(checkname, width, height, comp, data, 0);
			break;
		case 3:
			success = stbi_write_jpg(checkname, width, height, comp, data, quality);
			break;
	}

	if(success)
	{
		Com_Printf("Wrote %s\n", picname);
	}
	else
	{
		Com_Printf("SCR_ScreenShot_f: Couldn't write %s\n", picname);
	}
}

// --------

// Video mode array
// ----------------

typedef struct vidmode_s
{
	const char *description;
	int width, height;
	int mode;
} vidmode_t;

// This must be the same as VID_MenuInit()->resolutions[] in videomenu.c!
vidmode_t vid_modes[] = {
	{"Mode  0:  320x240", 320, 240, 0},
	{"Mode  1:  400x300", 400, 300, 1},
	{"Mode  2:  512x384", 512, 384, 2},
	{"Mode  3:  640x400", 640, 400, 3},
	{"Mode  4:  640x480", 640, 480, 4},
	{"Mode  5:  800x500", 800, 500, 5},
	{"Mode  6:  800x600", 800, 600, 6},
	{"Mode  7:  960x720", 960, 720, 7},
	{"Mode  8: 1024x480", 1024, 480, 8},
	{"Mode  9: 1024x640", 1024, 640, 9},
	{"Mode 10: 1024x768", 1024, 768, 10},
	{"Mode 11: 1152x768", 1152, 768, 11},
	{"Mode 12: 1152x864", 1152, 864, 12},
	{"Mode 13: 1280x800", 1280, 800, 13},
	{"Mode 14: 1280x720", 1280, 720, 14},
	{"Mode 15: 1280x960", 1280, 960, 15},
	{"Mode 16: 1280x1024", 1280, 1024, 16},
	{"Mode 17: 1366x768", 1366, 768, 17},
	{"Mode 18: 1440x900", 1440, 900, 18},
	{"Mode 19: 1600x1200", 1600, 1200, 19},
	{"Mode 20: 1680x1050", 1680, 1050, 20},
	{"Mode 21: 1920x1080", 1920, 1080, 21},
	{"Mode 22: 1920x1200", 1920, 1200, 22},
	{"Mode 23: 2048x1536", 2048, 1536, 23},
	{"Mode 24: 2560x1080", 2560, 1080, 24},
	{"Mode 25: 2560x1440", 2560, 1440, 25},
	{"Mode 26: 2560x1600", 2560, 1600, 26},
	{"Mode 27: 3440x1440", 3440, 1440, 27},
	{"Mode 28: 3840x1600", 3840, 1600, 28},
	{"Mode 29: 3840x2160", 3840, 2160, 29},
	{"Mode 30: 4096x2160", 4096, 2160, 30},
	{"Mode 31: 5120x2880", 5120, 2880, 31},
};

#define VID_NUM_MODES (sizeof(vid_modes) / sizeof(vid_modes[0]))

/*
 * Callback function for the 'vid_listmodes' cmd.
 */
void
VID_ListModes_f(void)
{
	int i;

	Com_Printf("Supported video modes (r_mode):\n");

	for (i = 0; i < VID_NUM_MODES; ++i)
	{
		Com_Printf("  %s\n", vid_modes[i].description);
	}
	Com_Printf("  Mode -1: r_customwidth x r_customheight\n");
}

/*
 * Returns informations about the given mode.
 */
qboolean
VID_GetModeInfo(int *width, int *height, int mode)
{
	if ((mode < 0) || (mode >= VID_NUM_MODES))
	{
		return false;
	}

	*width = vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return true;
}

// --------

// Renderer load, restart and shutdown
// -----------------------------------

// Global console variables.
cvar_t *vid_gamma;
cvar_t *vid_fullscreen;
cvar_t *vid_renderer;

// Global video state, used throughout the client.
viddef_t viddef;

// Struct with the pointers exported by the renderer.
refexport_t	re;

// Handle / pointer the the loaded renderer DLL.
void *reflib_handle = NULL;

// Is a renderer loaded and active?
qboolean ref_active = false;

/*
 * Restarts the renderer.
 */
void
VID_Restart_f(void)
{
	vid_fullscreen->modified = true;
}

/*
 * Shuts the renderer down and unloads it.
 */
void
VID_ShutdownRenderer(void)
{
	if (ref_active)
	{
		/* Shut down the renderer */
		re.Shutdown();
		GLimp_ShutdownGraphics();
		Sys_FreeLibrary(reflib_handle);
		reflib_handle = NULL;
		memset(&re, 0, sizeof(re));
	}

	// Declare the refresher as inactive
	ref_active = false;
}

/*
 * Loads and initializes a renderer.
 */
qboolean
VID_LoadRenderer(void)
{
	refimport_t	ri;
	GetRefAPI_t	GetRefAPI;

#ifdef __APPLE__
	const char* lib_ext = "dylib";
#elif defined(_WIN32)
	const char* lib_ext = "dll";
#else
	const char* lib_ext = "so";
#endif

	char reflib_name[64] = {0};
	char reflib_path[MAX_OSPATH] = {0};

	// If the refresher is already active we need
	// to shut it down before loading a new one
	VID_ShutdownRenderer();

	// Log what we're doing.
	Com_Printf("----- refresher initialization -----\n");

	snprintf(reflib_name, sizeof(reflib_name), "ref_%s.%s", vid_renderer->string, lib_ext);
	snprintf(reflib_path, sizeof(reflib_path), "%s%s", Sys_GetBinaryDir(), reflib_name);
	Com_Printf("Loading library: %s\n", reflib_name);

	// Mkay, let's load the requested renderer.
	GetRefAPI = Sys_LoadLibrary(reflib_path, "GetRefAPI", &reflib_handle);

	// Okay, we couldn't load it. It's up to the
	// caller to recover from this.
	if (GetRefAPI == NULL)
	{
		Com_Printf("Loading %s as renderer lib failed!", reflib_path);

		return false;
	}

	// Fill in the struct exported to the renderer.
	// FIXME: Do we really need all these?
	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ExecuteText = Cbuf_ExecuteText;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Com_VPrintf = Com_VPrintf;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_SetValue = Cvar_SetValue;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_Gamedir = FS_Gamedir;
	ri.FS_LoadFile = FS_LoadFile;
	ri.GLimp_InitGraphics = GLimp_InitGraphics;
	ri.GLimp_GetDesktopMode = GLimp_GetDesktopMode;
	ri.Sys_Error = Com_Error;
	ri.Vid_GetModeInfo = VID_GetModeInfo;
	ri.Vid_MenuInit = VID_MenuInit;
	ri.Vid_WriteScreenshot = VID_WriteScreenshot;

	// Exchange our export struct with the renderers import struct.
	re = GetRefAPI(ri);

	// Declare the refresher as active.
	ref_active = true;

	// Let's check if we've got a compatible renderer.
	if (re.api_version != API_VERSION)
	{
		VID_ShutdownRenderer();

		Com_Printf("%s has incompatible api_version %d!\n", reflib_name, re.api_version);

		return false;
	}

	// Everything seems okay, initialize it.
	if (!re.Init())
	{
		VID_ShutdownRenderer();

		Com_Printf("ERROR: Loading %s as rendering backend failed.\n", reflib_name);
		Com_Printf("------------------------------------\n\n");

		return false;
	}

	/* Ensure that all key states are cleared */
	Key_MarkAllUp();

	Com_Printf("Successfully loaded %s as rendering backend.\n", reflib_name);
	Com_Printf("------------------------------------\n\n");

	return true;
}

/*
 * Checks if a renderer changes was requested and executes it.
 * Inclusive fallback through all renderers. :)
 */
void
VID_CheckChanges(void)
{
	// FIXME: Not with vid_fullscreen, should be a dedicated variable.
	// Sounds easy but this vid_fullscreen hack is really messy and
	// interacts with several critical places in both the client and
	// the renderers...
	if (vid_fullscreen->modified)
	{
		// Stop sound, because the clients blocks while
		// we're reloading the renderer. The sound system
		// would screw up it's internal timings.
		S_StopAllSounds();

		// Reset the client side of the renderer state.
		cl.refresh_prepped = false;
		cl.cinematicpalette_active = false;

		// More or less blocks the client.
		cls.disable_screen = true;

		// Mkay, let's try our luck.
		while (!VID_LoadRenderer())
		{
			// We try: vk -> gl3 -> gl1 -> soft.
			if (strcmp(vid_renderer->string, "vk") == 0)
			{
				Com_Printf("Retrying with gl3...\n");
				Cvar_Set("vid_renderer", "gl3");
			}
			else if (strcmp(vid_renderer->string, "gl3") == 0)
			{
				Com_Printf("Retrying with gl1...\n");
				Cvar_Set("vid_renderer", "gl1");
			}
			else if (strcmp(vid_renderer->string, "gl1") == 0)
			{
				Com_Printf("Retrying with soft...\n");
				Cvar_Set("vid_renderer", "soft");
			}
			else if (strcmp(vid_renderer->string, "soft") == 0)
			{
				// Sorry, no usable renderer found.
				Com_Error(ERR_FATAL, "No usable renderer found!\n");
			}
			else
			{
				// User forced something stupid.
				Com_Printf("Retrying with gl3...\n");
				Cvar_Set("vid_renderer", "gl3");
			}
		}

		// Unblock the client.
		cls.disable_screen = false;
	}
}

/*
 * Initializes the video stuff.
 */
void
VID_Init(void)
{
	// Console variables
	vid_gamma = Cvar_Get("vid_gamma", "1.0", CVAR_ARCHIVE);
	vid_fullscreen = Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_renderer = Cvar_Get("vid_renderer", "gl1", CVAR_ARCHIVE);

	// Commands
	Cmd_AddCommand("vid_restart", VID_Restart_f);
	Cmd_AddCommand("vid_listmodes", VID_ListModes_f);

	// Initializes the video backend. This is NOT the renderer
	// itself, just the client side support stuff!
	if (!GLimp_Init())
	{
		Com_Error(ERR_FATAL, "Couldn't initialize the graphics subsystem!\n");
	}

	// Load the renderer and get things going.
	VID_CheckChanges();
}

/*
 * Shuts the video stuff down.
 */
void
VID_Shutdown(void)
{
	VID_ShutdownRenderer();
	GLimp_Shutdown();
}

// ----

// Wrappers for the functions provided by the renderer libs.
// =========================================================

void
R_BeginRegistration(char *map)
{
	if (ref_active)
	{
		re.BeginRegistration(map);
	}
}

struct model_s*
R_RegisterModel(char *name)
{
	if (ref_active)
	{
		return re.RegisterModel(name);
	}

	return NULL;
}

struct image_s*
R_RegisterSkin(char *name)
{
	if (ref_active)
	{
		return re.RegisterSkin(name);
	}

	return NULL;
}

void
R_SetSky(char *name, float rotate, vec3_t axis)
{
	if (ref_active)
	{
		re.SetSky(name, rotate, axis);
	}
}

void
R_EndRegistration(void)
{
	if (ref_active)
	{
		re.EndRegistration();
	}
}

void
R_RenderFrame(refdef_t *fd)
{
	if (ref_active)
	{
		re.RenderFrame(fd);
	}
}

struct image_s*
Draw_FindPic(char *name)
{
	if (ref_active)
	{
		return re.DrawFindPic(name);
	}

	return NULL;
}


void
Draw_GetPicSize(int *w, int *h, char *name)
{
	if (ref_active)
	{
		re.DrawGetPicSize(w, h, name);
	}
}

void
Draw_StretchPic(int x, int y, int w, int h, char *name)
{
	if (ref_active)
	{
		re.DrawStretchPic(x, y, w, h, name);
	}
}

void
Draw_PicScaled(int x, int y, char *pic, float factor)
{
	if (ref_active)
	{
		re.DrawPicScaled(x, y, pic, factor);
	}
}

void
Draw_CharScaled(int x, int y, int num, float scale)
{
	if (ref_active)
	{
		re.DrawCharScaled(x, y, num, scale);
	}
}

void
Draw_TileClear(int x, int y, int w, int h, char *name)
{
	if (ref_active)
	{
		re.DrawTileClear(x, y, w, h, name);
	}
}

void
Draw_Fill(int x, int y, int w, int h, int c)
{
	if (ref_active)
	{
		re.DrawFill(x, y, w, h, c);
	}
}

void
Draw_FadeScreen(void)
{
	if (ref_active)
	{
		re.DrawFadeScreen();
	}
}

void
Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data)
{
	if (ref_active)
	{
		re.DrawStretchRaw(x, y, w, h, cols, rows, data);
	}
}

void
R_SetPalette(const unsigned char *palette)
{
	if (ref_active)
	{
		re.SetPalette(palette);
	}
}

void
R_BeginFrame(float camera_separation)
{
	if (ref_active)
	{
		re.BeginFrame(camera_separation);
	}
}

qboolean
R_EndWorldRenderpass(void)
{
	if(ref_active)
	{
		return re.EndWorldRenderpass();
	}
	return false;
}

void
R_EndFrame(void)
{
	if(ref_active)
	{
		re.EndFrame();
	}
}

qboolean
R_IsVSyncActive(void)
{
	if (ref_active)
	{
		return re.IsVSyncActive();
	}

	return false;
}
