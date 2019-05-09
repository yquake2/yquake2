/*
 * Just a trivial stupid wrapper quake2.exe that starts yquake2.exe.
 * It calls it with the whole path (assuming it's in same directory as this wrapper)
 * and passes the commandline
 *
 * This should allow us to rename the real executable to yquake2.exe (to hopefully
 * avoid trouble with whatever stupid thing interferes with mouse input once
 * console has been opened if the games executable is called quake2.exe)
 * while avoiding confusion for people upgrading their yq2 installation who still have
 * shortcuts (possibly in Steam) to quake2.exe that would otherwise launch an old version.
 *
 * Can be built with just
 *   $ gcc -Wall -o quake2.exe wrapper.c
 * in our mingw build environment, will only depend on kernel32.dll and MSVCRT.DLL then,
 * which should be available on every Windows installation.
 * 
 * (C) 2017 Daniel Gibson
 * License:
 *  This software is dual-licensed to the public domain and under the following
 *  license: you are granted a perpetual, irrevocable license to copy, modify,
 *  publish, and distribute this file as you see fit.
 *  No warranty implied; use at your own risk.
 *
 * So you can do whatever you want with this code, including copying it
 * (or parts of it) into your own source.
 */

#include <windows.h>
#include <wchar.h>
#include <stdio.h>

static const WCHAR WRAPPED_EXE[] = L"yquake2.exe";

// the struct and OnGetWindowByProcess taken from https://stackoverflow.com/a/17166455
typedef struct {
	DWORD pid;
	HWND hwnd;
} WINDOWPROCESSINFO;

static BOOL CALLBACK OnGetWindowByProcess(HWND hwnd, LPARAM lParam)
{
	WINDOWPROCESSINFO *infoPtr = (WINDOWPROCESSINFO *)lParam;
	DWORD check = 0;
	BOOL br = TRUE;
	GetWindowThreadProcessId(hwnd, &check);
	if (check == infoPtr->pid)
	{
		infoPtr->hwnd = hwnd;
		br = FALSE;
	}
	return br;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WCHAR* cmdLine = GetCommandLineW();
	WCHAR exePath[2048];
	WCHAR* lastBackSlash = exePath;
	int maxLenSafe = sizeof(exePath)/sizeof(WCHAR) - wcslen(WRAPPED_EXE);

	// get full path to this executable..
	DWORD len = GetModuleFileNameW(NULL, exePath, maxLenSafe);
	if(len <= 0 || len == maxLenSafe) {
		// an error occured, clear exe path
		exePath[0] = 0;
	} else {
		// .. cut off executable name (after last backslash in path)
		lastBackSlash = wcsrchr(exePath, L'\\');
		if(lastBackSlash != NULL) {
			lastBackSlash[1] = 0;
		} else {
			// if there was no backslash, fall back to using only the wrapped exe name
			// (appended to empty exePath buffer)
			lastBackSlash = exePath;
			lastBackSlash[0] = 0;
		}
	}

	// .. append wrapped executable name to path ..
	// should be safe, because maxLenSafe subtracted WRAPPED_EXE's length
	// (and that's very conservative, we removed the original .exe name after all)
	wcscat(lastBackSlash, WRAPPED_EXE);

	// .. and start the wrapped executable
	{
		STARTUPINFOW si = {0};
		PROCESS_INFORMATION pi = {0};
		BOOL ret = FALSE;
		si.cb = sizeof(si);

		ret = CreateProcessW(exePath, cmdLine,
		                     NULL,  // process security attributes
		                     NULL,  // thread security attributes
		                     FALSE, // don't inherit handles
		                     0,     // no creation flag
		                     NULL,  // environment block - no changes, use ours
		                     NULL,  // don't change CWD
		                     &si, &pi);

		if(!ret)
		{
			fprintf(stderr, "Couldn't CreateProcess() (%ld).\n", GetLastError());
			return 1;
		}

		Sleep(1000); // wait until yq2 should have created the window

		// send window to foreground, as shown in https://stackoverflow.com/a/17166455

		// first we need to get its HWND
		WINDOWPROCESSINFO info;
		info.pid = GetProcessId(pi.hProcess);
		info.hwnd = 0;
		AllowSetForegroundWindow(info.pid);
		EnumWindows(OnGetWindowByProcess, (LPARAM)&info);
		if (info.hwnd != 0)
		{
			SetForegroundWindow(info.hwnd);
			SetActiveWindow(info.hwnd);
		}

		// wait for wrapped exe to exit
		WaitForSingleObject(pi.hProcess, INFINITE);

		// close the process and thread object handles
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	return 0;
}
