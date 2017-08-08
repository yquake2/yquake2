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
 * This file implements some generic funktions
 *
 * =======================================================================
 */

#include <stdio.h>
#include <string.h>

#if defined(__linux) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <unistd.h> // readlink(), amongst others
#include "../../common/header/shared.h"

#endif

#ifdef __FreeBSD__
#include <sys/sysctl.h> // for sysctl() to get path to executable
#endif

#ifdef _WIN32
#include <windows.h> // GetModuleFileNameA()
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h> // _NSGetExecutablePath
#endif

#ifndef PATH_MAX
// this is mostly for windows. windows has a MAX_PATH = 260 #define, but allows
// longer paths anyway.. this might not be the maximum allowed length, but is
// hopefully good enough for realistic usecases
#define PATH_MAX 4096
#define _DG__DEFINED_PATH_MAX
#endif

static void SetExecutablePath(char* exePath)
{
	// !!! this assumes that exePath can hold PATH_MAX chars !!!

#ifdef _WIN32

	DWORD len = GetModuleFileNameA(NULL, exePath, PATH_MAX);
	if(len <= 0 || len == PATH_MAX)
	{
		// an error occured, clear exe path
		exePath[0] = '\0';
	}

#elif defined(__linux) || defined(__NetBSD__)

	// all the platforms that have /proc/$pid/exe or similar that symlink the
	// real executable - basiscally Linux and the BSDs except for FreeBSD which
	// doesn't enable proc by default and has a sysctl() for this. OpenBSD once
	// had /proc but removed it for security reasons.
	char buf[PATH_MAX] = {0};
#ifdef __linux
	snprintf(buf, sizeof(buf), "/proc/%d/exe", getpid());
#else // the BSDs
	snprintf(buf, sizeof(buf), "/proc/%d/file", getpid());
#endif
	// readlink() doesn't null-terminate!
	int len = readlink(buf, exePath, PATH_MAX-1);
	if (len <= 0)
	{
		// an error occured, clear exe path
		exePath[0] = '\0';
	}
	else
	{
		exePath[len] = '\0';
	}

#elif defined(__FreeBSD__)

	// the sysctl should also work when /proc/ is not mounted (which seems to
	// be common on FreeBSD), so use it..
	int name[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
	size_t len = PATH_MAX-1;
	int ret = sysctl(name, sizeof(name)/sizeof(name[0]), exePath, &len, NULL, 0);
	if(ret != 0)
	{
		// an error occured, clear exe path
		exePath[0] = '\0';
	}

#elif defined(__APPLE__)

	uint32_t bufSize = PATH_MAX;
	if(_NSGetExecutablePath(exePath, &bufSize) != 0)
	{
		// WTF, PATH_MAX is not enough to hold the path?
		// an error occured, clear exe path
		exePath[0] = '\0';
	}

	// TODO: realpath() ?
	// TODO: no idea what this is if the executable is in an app bundle

#else

	// Several platforms (for example OpenBSD) donn't provide a
	// reliable way to determine the executable path. Just return
	// an empty string.
	exePath[0] = '\0';

// feel free to add implementation for your platform and send a pull request.
#warning "SetExecutablePath() is unimplemented on this platform"

#endif
}

const char *Sys_GetBinaryDir(void)
{
	static char exeDir[PATH_MAX] = {0};

	if(exeDir[0] != '\0') {
		return exeDir;
	}

	SetExecutablePath(exeDir);

	if (exeDir[0] == '\0') {
		Com_Printf("Couldn't determine executable path. Using ./ instead.\n");
		Q_strlcpy(exeDir, "./", sizeof(exeDir));
	} else {
		// cut off executable name
		char *lastSlash = strrchr(exeDir, '/');
#ifdef _WIN32
		char* lastBackSlash = strrchr(exeDir, '\\');
		if(lastSlash == NULL || lastBackSlash > lastSlash) lastSlash = lastBackSlash;
#endif // _WIN32

		if (lastSlash != NULL) lastSlash[1] = '\0'; // cut off after last (back)slash
	}

	return exeDir;
}
