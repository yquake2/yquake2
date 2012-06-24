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

#include "../common/header/common.h"
#include "header/resource.h"
#include "header/winquake.h"

#define MAX_NUM_ARGVS 128

int curtime;
int starttime;
qboolean ActiveApp;
qboolean Minimized;

static HANDLE hinput, houtput;
static HANDLE qwclsemaphore;

HINSTANCE global_hInstance;
static HINSTANCE game_library;

unsigned int sys_msg_time;
unsigned int sys_frame_time;

static char console_text[256];
static int console_textlen;  

char findbase[MAX_OSPATH];
char findpath[MAX_OSPATH];
int findhandle;

int argc;
char *argv[MAX_NUM_ARGVS];

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

#ifndef DEDICATED_ONLY
	fprintf(stderr, "Error: %s\n", text);
#endif

	MessageBox(NULL, text, "Error", 0 /* MB_OK */);

	if (qwclsemaphore)
	{
		CloseHandle(qwclsemaphore);
	}

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
	CloseHandle(qwclsemaphore);

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

void
WinError(void)
{
	LPVOID lpMsgBuf;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf, 0, NULL);

	/* Display the string. */
	MessageBox(NULL, lpMsgBuf, "GetLastError", MB_OK | MB_ICONINFORMATION);

	/* Free the buffer. */
	LocalFree(lpMsgBuf);
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

	if ( IN_Update_fp )
	{
		IN_Update_fp();
	}

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
	const char *gamename = "game.dll";
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

		Com_sprintf(name, sizeof(name), "%s/%s", path, gamename);
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

void
ParseCommandLine(LPSTR lpCmdLine)
{
	argc = 1;
	argv[0] = "exe";

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
		{
			lpCmdLine++;
		}

		if (*lpCmdLine)
		{
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
			{
				lpCmdLine++;
			}

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}
}

/* ======================================================================= */

int
Sys_Milliseconds(void)
{
	static int base;
	static qboolean initialized = false;

	if (!initialized)
	{   /* let base retain 16 bits of effectively random data */
		base = timeGetTime() & 0xffff0000;
		initialized = true;
	}

	curtime = timeGetTime() - base;

	return curtime;
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
	char *home;
	char *old;
	char path_stdout[MAX_OSPATH];
	char path_stderr[MAX_OSPATH];

	if (dedicated && dedicated->value)
	{
		return;
	}

	home = Sys_GetHomeDir();

	if (home == NULL)
	{
		return;
	}

	cur = old = home;

	while (cur != NULL)
	{
		if ((cur - old) > 1)
		{
			*cur = '\0';
			Sys_Mkdir(home);
			*cur = '/';
		}

		old = cur;
		cur = strchr(old + 1, '/');
	}

	snprintf(path_stdout, sizeof(path_stdout), "%s/%s", home, "stdout.txt");
	snprintf(path_stderr, sizeof(path_stderr), "%s/%s", home, "stderr.txt");

	freopen(path_stdout, "w", stdout);
	freopen(path_stderr, "w", stderr);
}

/* ======================================================================= */

/*
 * Windows main function. Containts the 
 * initialization code and the main loop
 */
int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
		LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
	int time, oldtime, newtime;

	/* Previous instances do not exist in Win32 */
	if (hPrevInstance)
	{
		return 0;
	}

	/* Make the current instance global */
	global_hInstance = hInstance;

	/* Redirect stdout and stderr into a file */
#ifndef DEDICATED_ONLY
	Sys_RedirectStdout();
#endif

	printf("Yamagi Quake II v%4.2f\n", VERSION);
	printf("=====================\n\n");

#ifndef DEDICATED_ONLY
	printf("Client build options:\n");
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

	printf("Platform: %s\n", BUILDSTRING);
	printf("Architecture: %s\n", CPUSTRING);

	/* Seed PRNG */
	randk_seed();

	/* Parse the command line arguments */
	ParseCommandLine(lpCmdLine);

	/* Call the initialization code */
	Qcommon_Init(argc, argv);

	/* Save our time */
	oldtime = Sys_Milliseconds();

	/* The legendary main loop */
	while (1)
	{
		/* If at a full screen console, don't update unless needed */
		if (Minimized || (dedicated && dedicated->value))
		{
			Sleep(1);
		}

		while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage(&msg, NULL, 0, 0))
			{
				Com_Quit();
			}

			sys_msg_time = msg.time;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		do
		{
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
		}
		while (time < 1);

		_controlfp(_PC_24, _MCW_PC);
		Qcommon_Frame(time);

		oldtime = newtime;
	}

	/* never gets here */
	return TRUE;
}

