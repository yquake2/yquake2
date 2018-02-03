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
 * This file is the starting point of the program and implements
 * several support functions and the main loop.
 *
 * =======================================================================
 */

#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include <shlobj.h>
#include <windows.h>

#include "SDL2/SDL.h"
#include "SDL2/SDL_Main.h"

#include "../../common/header/common.h"
#include "../generic/header/input.h"
#include "header/resource.h"

int starttime;

static HANDLE hinput, houtput;

static HINSTANCE game_library;

unsigned int sys_frame_time;

static char console_text[256];
static int console_textlen;

char findbase[MAX_OSPATH];
char findpath[MAX_OSPATH];
int findhandle;

qboolean is_portable;

/* ================================================================ */

void
Sys_Error(char *error, ...)
{
	va_list argptr;
	char text[1024];

#ifndef DEDICATED_ONLY
	CL_Shutdown();
#endif

	Qcommon_Shutdown();

	va_start(argptr, error);
	vsprintf(text, error, argptr);
	va_end(argptr);
	fprintf(stderr, "Error: %s\n", text);

	MessageBox(NULL, text, "Error", 0 /* MB_OK */);

	/* Close stdout and stderr */
#ifndef DEDICATED_ONLY
	fclose(stdout);
	fclose(stderr);
#endif

	exit(1);
}

void
Sys_Quit(void)
{
	timeEndPeriod(1);

#ifndef DEDICATED_ONLY
	CL_Shutdown();
#endif

	Qcommon_Shutdown();

	if (dedicated && dedicated->value)
	{
		FreeConsole();
	}

	/* Close stdout and stderr */
#ifndef DEDICATED_ONLY
	fclose(stdout);
	fclose(stderr);
#endif

	exit(0);
}

/* ================================================================ */

void
Sys_Init(void)
{
	OSVERSIONINFO vinfo;

	timeBeginPeriod(1);
	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx(&vinfo))
	{
		Sys_Error("Couldn't get OS info");
	}

	/* While Quake II should run on older versions,
	   limit Yamagi Quake II to Windows XP and
	   above. Testing older version would be a
	   PITA. */
	if (!((vinfo.dwMajorVersion > 5) ||
		  ((vinfo.dwMajorVersion == 5) &&
		   (vinfo.dwMinorVersion >= 1))))
	{
		Sys_Error("Yamagi Quake II needs Windows XP or higher!\n");
	}


	if (dedicated->value)
	{
		AllocConsole();

		hinput = GetStdHandle(STD_INPUT_HANDLE);
		houtput = GetStdHandle(STD_OUTPUT_HANDLE);
	}
}

char *
Sys_ConsoleInput(void)
{
	INPUT_RECORD recs[1024];
	int ch;
   	DWORD dummy, numread, numevents;

	if (!dedicated || !dedicated->value)
	{
		return NULL;
	}

	for ( ; ; )
	{
		if (!GetNumberOfConsoleInputEvents(hinput, &numevents))
		{
			Sys_Error("Error getting # of console events");
		}

		if (numevents <= 0)
		{
			break;
		}

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
		{
			Sys_Error("Error reading console input");
		}

		if (numread != 1)
		{
			Sys_Error("Couldn't read console input");
		}

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);

						if (console_textlen)
						{
							console_text[console_textlen] = 0;
							console_textlen = 0;
							return console_text;
						}

						break;

					case '\b':

						if (console_textlen)
						{
							console_textlen--;
							WriteFile(houtput, "\b \b", 3, &dummy, NULL);
						}

						break;

					default:

						if (ch >= ' ')
						{
							if (console_textlen < sizeof(console_text) - 2)
							{
								WriteFile(houtput, &ch, 1, &dummy, NULL);
								console_text[console_textlen] = ch;
								console_textlen++;
							}
						}

						break;
				}
			}
		}
	}

	return NULL;
}

void
Sys_ConsoleOutput(char *string)
{
	char text[256];
	DWORD dummy;

	if (!dedicated || !dedicated->value)
	{
		fputs(string, stdout);
	}
	else
	{
		if (console_textlen)
		{
			text[0] = '\r';
			memset(&text[1], ' ', console_textlen);
			text[console_textlen + 1] = '\r';
			text[console_textlen + 2] = 0;
			WriteFile(houtput, text, console_textlen + 2, &dummy, NULL);
		}

		WriteFile(houtput, string, strlen(string), &dummy, NULL);

		if (console_textlen)
		{
			WriteFile(houtput, console_text, console_textlen, &dummy, NULL);
		}
	}
}

void
Sys_SendKeyEvents(void)
{
#ifndef DEDICATED_ONLY
	IN_Update();
#endif

	/* grab frame time */
	sys_frame_time = timeGetTime();
}

/* ================================================================ */

void
Sys_UnloadGame(void)
{
	if (!FreeLibrary(game_library))
	{
		Com_Error(ERR_FATAL, "FreeLibrary failed for game library");
	}

	game_library = NULL;
}

void *
Sys_GetGameAPI(void *parms)
{
	void *(*GetGameAPI)(void *);
	char name[MAX_OSPATH];
	char *path = NULL;

	if (game_library)
	{
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");
	}

	/* now run through the search paths */
	path = NULL;

	while (1)
	{
		path = FS_NextPath(path);

		if (!path)
		{
			return NULL; /* couldn't find one anywhere */
		}

		/* Try game.dll */
		Com_sprintf(name, sizeof(name), "%s/%s", path, "game.dll");
		game_library = LoadLibrary(name);

		if (game_library)
		{
			Com_DPrintf("LoadLibrary (%s)\n", name);
			break;
		}

		/* Try gamex86.dll as fallback */
 		Com_sprintf(name, sizeof(name), "%s/%s", path, "gamex86.dll");
		game_library = LoadLibrary(name);

		if (game_library)
		{
			Com_DPrintf("LoadLibrary (%s)\n", name);
			break;
		}
	}

	GetGameAPI = (void *)GetProcAddress(game_library, "GetGameAPI");

	if (!GetGameAPI)
	{
		Sys_UnloadGame();
		return NULL;
	}

	return GetGameAPI(parms);
}

/* ======================================================================= */

long long
Sys_Microseconds(void)
{
	long long microseconds;
	static long long uSecbase;

	FILETIME ft;
	unsigned long long tmpres = 0;

	GetSystemTimeAsFileTime(&ft);

	tmpres |= ft.dwHighDateTime;
	tmpres <<= 32;
	tmpres |= ft.dwLowDateTime;

	tmpres /= 10; // Convert to microseconds.
	tmpres -= 11644473600000000ULL; // ...and to unix epoch.

	microseconds = tmpres;

	if (!uSecbase)
	{
		uSecbase = microseconds - 1001ll;
	}

	return microseconds - uSecbase;
}

int
Sys_Milliseconds(void)
{
	return (int)(Sys_Microseconds()/1000ll);
}

void
Sys_Sleep(int msec)
{
	Sleep(msec);
}

void Sys_Nanosleep(int nanosec)
{
	HANDLE timer;
	LARGE_INTEGER li;

	timer = CreateWaitableTimer(NULL, TRUE, NULL);

	// Windows has a max. resolution of 100ns.
	li.QuadPart = -nanosec / 100;

	SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE);
	WaitForSingleObject(timer, INFINITE);

	CloseHandle(timer);
}

/* ======================================================================= */

static qboolean
CompareAttributes(unsigned found, unsigned musthave, unsigned canthave)
{
	if ((found & _A_RDONLY) && (canthave & SFF_RDONLY))
	{
		return false;
	}

	if ((found & _A_HIDDEN) && (canthave & SFF_HIDDEN))
	{
		return false;
	}

	if ((found & _A_SYSTEM) && (canthave & SFF_SYSTEM))
	{
		return false;
	}

	if ((found & _A_SUBDIR) && (canthave & SFF_SUBDIR))
	{
		return false;
	}

	if ((found & _A_ARCH) && (canthave & SFF_ARCH))
	{
		return false;
	}

	if ((musthave & SFF_RDONLY) && !(found & _A_RDONLY))
	{
		return false;
	}

	if ((musthave & SFF_HIDDEN) && !(found & _A_HIDDEN))
	{
		return false;
	}

	if ((musthave & SFF_SYSTEM) && !(found & _A_SYSTEM))
	{
		return false;
	}

	if ((musthave & SFF_SUBDIR) && !(found & _A_SUBDIR))
	{
		return false;
	}

	if ((musthave & SFF_ARCH) && !(found & _A_ARCH))
	{
		return false;
	}

	return true;
}

char *
Sys_FindFirst(char *path, unsigned musthave, unsigned canthave)
{
	struct _finddata_t findinfo;

	if (findhandle)
	{
		Sys_Error("Sys_BeginFind without close");
	}

	findhandle = 0;

	COM_FilePath(path, findbase);
	findhandle = _findfirst(path, &findinfo);

	if (findhandle == -1)
	{
		return NULL;
	}

	if (!CompareAttributes(findinfo.attrib, musthave, canthave))
	{
		return NULL;
	}

	Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
	return findpath;
}

char *
Sys_FindNext(unsigned musthave, unsigned canthave)
{
	struct _finddata_t findinfo;

	if (findhandle == -1)
	{
		return NULL;
	}

	if (_findnext(findhandle, &findinfo) == -1)
	{
		return NULL;
	}

	if (!CompareAttributes(findinfo.attrib, musthave, canthave))
	{
		return NULL;
	}

	Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
	return findpath;
}

void
Sys_FindClose(void)
{
	if (findhandle != -1)
	{
		_findclose(findhandle);
	}

	findhandle = 0;
}

void
Sys_Mkdir(char *path)
{
	_mkdir(path);
}

char *
Sys_GetCurrentDirectory(void)
{
	static char dir[MAX_OSPATH];

	if (!_getcwd(dir, sizeof(dir)))
	{
		Sys_Error("Couldn't get current working directory");
	}

	return dir;
}

char *
Sys_GetHomeDir(void)
{
	char *cur;
	char *old;
	char profile[MAX_PATH];
	int len;
	static char gdir[MAX_OSPATH];
	WCHAR sprofile[MAX_PATH];
	WCHAR uprofile[MAX_PATH];

	/* The following lines implement a horrible
	   hack to connect the UTF-16 WinAPI to the
	   ASCII Quake II. While this should work in
	   most cases, it'll fail if the "Windows to
	   DOS filename translation" is switched off.
	   In that case the function will return NULL
	   and no homedir is used. */

	/* Get the path to "My Documents" directory */
	SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, uprofile);

	/* Create a UTF-16 DOS path */
    len = GetShortPathNameW(uprofile, sprofile, sizeof(sprofile));

	if (len == 0)
	{
		return NULL;
	}

	/* Since the DOS path contains no UTF-16 characters, just convert it to ASCII */
	WideCharToMultiByte(CP_ACP, 0, sprofile, -1, profile, sizeof(profile), NULL, NULL);

	if (len == 0)
	{
		return NULL;
	}

	/* Check if path is too long */
    if ((len + strlen(CFGDIR) + 3) >= 256)
	{
		return NULL;
	}

	/* Replace backslashes by slashes */
	cur = old = profile;

	if (strstr(cur, "\\") != NULL)
	{
		while (cur != NULL)
		{
			if ((cur - old) > 1)
			{
				*cur = '/';

			}

			old = cur;
			cur = strchr(old + 1, '\\');
		}
	}

	snprintf(gdir, sizeof(gdir), "%s/%s/", profile, CFGDIR);

	return gdir;
}

void
Sys_RedirectStdout(void)
{
	char *cur;
	char *old;
	char dir[MAX_OSPATH];
	char path_stdout[MAX_OSPATH];
	char path_stderr[MAX_OSPATH];
	const char *tmp;

	if (is_portable) {
		tmp = Sys_GetBinaryDir();
		Q_strlcpy(dir, tmp, sizeof(dir));
	} else {
		tmp = Sys_GetHomeDir();
		Q_strlcpy(dir, tmp, sizeof(dir));
	}

	if (dir == NULL)
	{
		return;
	}

	cur = old = dir;

	while (cur != NULL)
	{
		if ((cur - old) > 1)
		{
			*cur = '\0';
			Sys_Mkdir(dir);
			*cur = '/';
		}

		old = cur;
		cur = strchr(old + 1, '/');
	}

	snprintf(path_stdout, sizeof(path_stdout), "%s/%s", dir, "stdout.txt");
	snprintf(path_stderr, sizeof(path_stderr), "%s/%s", dir, "stderr.txt");

	freopen(path_stdout, "w", stdout);
	freopen(path_stderr, "w", stderr);
}

/* ======================================================================= */

typedef enum YQ2_PROCESS_DPI_AWARENESS {
	YQ2_PROCESS_DPI_UNAWARE = 0,
	YQ2_PROCESS_SYSTEM_DPI_AWARE = 1,
	YQ2_PROCESS_PER_MONITOR_DPI_AWARE = 2
} YQ2_PROCESS_DPI_AWARENESS;

void
Sys_SetHighDPIMode(void)
{
	/* For Vista, Win7 and Win8 */
	BOOL(WINAPI *SetProcessDPIAware)(void) = NULL;

	/* Win8.1 and later */
	HRESULT(WINAPI *SetProcessDpiAwareness)(YQ2_PROCESS_DPI_AWARENESS dpiAwareness) = NULL;


	HINSTANCE userDLL = LoadLibrary("USER32.DLL");

	if (userDLL)
	{
		SetProcessDPIAware = (BOOL(WINAPI *)(void)) GetProcAddress(userDLL,
				"SetProcessDPIAware");
	}


	HINSTANCE shcoreDLL = LoadLibrary("SHCORE.DLL");

	if (shcoreDLL)
	{
		SetProcessDpiAwareness = (HRESULT(WINAPI *)(YQ2_PROCESS_DPI_AWARENESS))
			GetProcAddress(shcoreDLL, "SetProcessDpiAwareness");
	}


	if (SetProcessDpiAwareness) {
		SetProcessDpiAwareness(YQ2_PROCESS_PER_MONITOR_DPI_AWARE);
	}
	else if (SetProcessDPIAware) {
		SetProcessDPIAware();
	}
}

/* ======================================================================= */

/*
 * Windows main function. Containts the
 * initialization code and the main loop
 */
int
main(int argc, char *argv[])
{
	long long oldtime, newtime;

	/* Setup FPU if necessary */
	Sys_SetupFPU();

	/* Force DPI awareness */
	Sys_SetHighDPIMode();

	/* Are we portable? */
	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-portable") == 0) {
			is_portable = true;
		}
	}

	/* Need to redirect stdout before anything happens. */
#ifndef DEDICATED_ONLY
	Sys_RedirectStdout();
#endif

	printf("Yamagi Quake II v%s\n", YQ2VERSION);
	printf("=====================\n\n");

#ifndef DEDICATED_ONLY
	printf("Client build options:\n");
#ifdef SDL2
	printf(" + SDL2\n");
#else
	printf(" - SDL2 (using 1.2)\n");
#endif
#ifdef CDA
	printf(" + CD audio\n");
#else
	printf(" - CD audio\n");
#endif
#ifdef OGG
	printf(" + OGG/Vorbis\n");
#else
	printf(" - OGG/Vorbis\n");
#endif
#ifdef USE_OPENAL
	printf(" + OpenAL audio\n");
#else
	printf(" - OpenAL audio\n");
#endif
#ifdef ZIP
	printf(" + Zip file support\n");
#else
	printf(" - Zip file support\n");
#endif
#endif

	printf("Platform: %s\n", YQ2OSTYPE);
	printf("Architecture: %s\n", YQ2ARCH);


	/* Seed PRNG */
	randk_seed();

	/* Call the initialization code */
	Qcommon_Init(argc, argv);

	/* Save our time */
	oldtime = Sys_Microseconds();

	/* The legendary main loop */
	while (1)
	{
		// Throttle the game a little bit
		Sys_Nanosleep(5000);

		newtime = Sys_Microseconds();
		Qcommon_Frame(newtime - oldtime);
		oldtime = newtime;
	}

	/* never gets here */
	return TRUE;
}

void
Sys_FreeLibrary(void *handle)
{
	if (!handle)
	{
		return;
	}

	if (!FreeLibrary(handle))
	{
		Com_Error(ERR_FATAL, "FreeLibrary failed on %p", handle);
	}
}

void *
Sys_LoadLibrary(const char *path, const char *sym, void **handle)
{
	HMODULE module;
	void *entry;

	*handle = NULL;

	module = LoadLibraryA(path);

	if (!module)
	{
		Com_Printf("%s failed: LoadLibrary returned %lu on %s\n",
				__func__, GetLastError(), path);
		return NULL;
	}

	if (sym)
	{
		entry = GetProcAddress(module, sym);

		if (!entry)
		{
			Com_Printf("%s failed: GetProcAddress returned %lu on %s\n",
					__func__, GetLastError(), path);
			FreeLibrary(module);
			return NULL;
		}
	}
	else
	{
		entry = NULL;
	}

	*handle = module;

	Com_DPrintf("%s succeeded: %s\n", __func__, path);

	return entry;
}

void *
Sys_GetProcAddress(void *handle, const char *sym)
{
	return GetProcAddress(handle, sym);
}

