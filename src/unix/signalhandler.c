/*
 * Copyright (C) 2011 Yamagi Burmeister
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
 * This is a signal handler for printing some hints to debug problem in
 * the case of a crash. On Linux a backtrace is printed.
 *
 * =======================================================================
 */

#include <signal.h>

#ifdef __linux__
#include <execinfo.h>
#endif

#include "../common/header/common.h"

#ifdef __linux__

void
printBacktrace(int sig)
{
	void *array[15];
	size_t size;
	char **strings;
	int i;

	size = backtrace(array, 15);
	strings = backtrace_symbols(array, size);

	printf("Product:      Yamagi Quake II\n");
	printf("Version:      %4.2f\n", VERSION);
	printf("Plattform:    %s\n", BUILDSTRING);
	printf("Architecture: %s\n", CPUSTRING);
	printf("Signal:       %i\n", sig);
	printf("\nBacktrace:\n");

	for (i = 0; i < size; i++)
	{
		printf("  %s\n", strings[i]);
	}

	printf("\n");
}

#else

void
printBacktrace(int sig)
{
	printf("Product:      Yamagi Quake II\n");
	printf("Version:      %4.2f\n", VERSION);
	printf("Plattform:    %s\n", BUILDSTRING);
	printf("Architecture: %s\n", CPUSTRING);
	printf("Signal:       %i\n", sig);
	printf("\nBacktrace:\n");
	printf("  Not available on this plattform.\n\n");

}

#endif

void
signalhandler(int sig)
{
	printf("\nYamagi Quake II crashed! This should not happen...\n");
	printf("\nMake sure that you're using the last version. It can\n");
	printf("be found at http://www.yamagi.org/quake2. If you do send\n");
	printf("a bug report to quake2@yamagi.org and include this output,\n");
	printf("detailed condition, reproduction of the issue (if possible)\n");
	printf("and additional data you think might be useful. Thanks.\n");
	printf("\n=======================================================\n\n");

	printBacktrace(sig);

    /* reset signalhandler */
	signal(SIGSEGV, SIG_DFL);
	signal(SIGILL, SIG_DFL);
	signal(SIGFPE, SIG_DFL);
    signal(SIGABRT, SIG_DFL); 

	/* pass signal to the os */
    raise(sig);
}

void
registerHandler(void)
{
	signal(SIGSEGV, signalhandler);
	signal(SIGILL, signalhandler);
	signal(SIGFPE, signalhandler);
    signal(SIGABRT, signalhandler);
}

