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
 * This file is the starting point of the program. It does some platform
 * specific initialization stuff and calls the common initialization code.
 *
 * =======================================================================
 */

#include <windows.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>

#include "../../common/header/common.h"

/*
 * Windows main function. Containts the
 * initialization code and the main loop
 */
int
main(int argc, char **argv)
{
	// Setup FPU if necessary.
	Sys_SetupFPU();

	// Force DPI awareness.
	Sys_SetHighDPIMode();

	// crappy argument parser can't parse.
	for (int i = 0; i < argc; i++)
	{
		// Are we portable?
		if (strcmp(argv[i], "-portable") == 0)
		{
			is_portable = true;
		}

		// Inject a custom data dir.
		if (strcmp(argv[i], "-datadir") == 0)
		{
			// Mkay, did the user give us an argument?
			if (i != (argc - 1))
			{
				DWORD attrib;
				WCHAR wpath[MAX_OSPATH];

				MultiByteToWideChar(CP_UTF8, 0, argv[i + 1], -1, wpath, MAX_OSPATH);
				attrib = GetFileAttributesW(wpath);

				if (attrib != INVALID_FILE_ATTRIBUTES)
				{
					if (!(attrib & FILE_ATTRIBUTE_DIRECTORY))
					{
						printf("-datadir %s is not a directory\n", argv[i + 1]);
						return 1;
					}

					Q_strlcpy(datadir, argv[i + 1], MAX_OSPATH);
				}
				else
				{
					printf("-datadir %s could not be found\n", argv[i + 1]);
					return 1;
				}
			}
			else
			{
				printf("-datadir needs an argument\n");
				return 1;
			}
		}
	}

	// Need to redirect stdout before anything happens.
#ifndef DEDICATED_ONLY
	Sys_RedirectStdout();
#endif

	// Call the initialization code.
	// Never returns.
	Qcommon_Init(argc, argv);

	return 0;
}
