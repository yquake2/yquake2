/*
 * Copyright (C) 2018 Yamagi
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
 * CURL backend for Quake II.
 *
 * =======================================================================
 */

#ifdef USE_CURL

// --------

#include <curl/curl.h>

#include "header/qcurl.h"
#include "../../common/header/common.h"

// --------

// True if cURL is initialized.
qboolean qcurlInitialized;

// Pointer to the dynamic library.
static void *curlhandle;

// CVars
static cvar_t *cl_libcurl;

// Function pointers to cURL.
void (*qcurl_easy_cleanup)(CURL *curl);
CURL *(*qcurl_easy_init)(void);
CURLcode (*qcurl_easy_getinfo)(CURL *curl, CURLINFO info, ...);
CURLcode (*qcurl_easy_setopt)(CURL *curl, CURLoption option, ...);
const char *(*qcurl_easy_strerror)(CURLcode);

void (*qcurl_global_cleanup)(void);
CURLcode (*qcurl_global_init)(long flags);

CURLMcode (*qcurl_multi_add_handle)(CURLM *multi_handle, CURL *curl_handle);
CURLMcode (*qcurl_multi_cleanup)(CURLM *multi_handle);
CURLMsg *(*qcurl_multi_info_read)(CURLM *multi_handle, int *msgs_in_queue);
CURLM *(*qcurl_multi_init)(void);
CURLMcode (*qcurl_multi_perform)(CURLM *multi_handle, int *running_handles);
CURLMcode (*qcurl_multi_remove_handle)(CURLM *multi_handle, CURL *curl_handle);

// --------

/*
 * Load libcurl, connect the function pointer
 * and call cURLs global init function.
 */
qboolean qcurlInit(void)
{
	Com_Printf("------- curl initialization -------\n");

	assert(!qcurlInitialized && "cURL already initialized?!");

	// Most systems have only one distinct name for the
	// libcurl, loading that one will be successfull in
	// at least 99% of all cases. But Linux, the mother
	// of chaos, is more complicated. Debian / Ubuntu
	// alone has 6 possible names... So:
	//  - Let's try whatever the user has written into
	//    the cl_libcurl cvar first.
	//  - If that fails try all possible names until we
	//    find a working one.
	//  - If we found one put it into cl_libcurl, so we
	//    don't need to iterate trought the whole list
	//    the next time the game is started.
	//  - If we found none error out.
#ifdef __APPLE__
	const char *libcurl[] = { "libcurl.dylib", NULL };
#elif __linux__
	const char *libcurl[] = { "libcurl.so.3", "libcurl.so.4", "libcurl-gnutls.so.3",
		"libcurl-gnutls.so.4", "libcurl-nss.so.3", "libcurl-nss.so.4", "libcurl.so",
		NULL };
#elif _WIN32
	const char *libcurl[] = { "curl.dll", "libcurl.dll", NULL };
#else
	const char *libcurl[] = { "libcurl.so", NULL };
#endif

	// Mkay, let's try to find a working libcurl.
	cl_libcurl = Cvar_Get("cl_libcurl", (char *)libcurl[0], CVAR_ARCHIVE);

	Com_Printf("LoadLibrary(%s)\n", cl_libcurl->string);
	Sys_LoadLibrary(cl_libcurl->string, NULL, &curlhandle);

	if (!curlhandle)
	{
		Com_Printf("Loading %s failed!\n", cl_libcurl->string);

		for (int i = 0; libcurl[i] != NULL; i++)
		{
			// Tried that one already.
			if (!strcmp(cl_libcurl->string, libcurl[i]))
			{
				continue;
			}

			Com_Printf("LoadLibrary(%s)\n", libcurl[i]);
			Sys_LoadLibrary(libcurl[i], NULL, &curlhandle);

			if (!curlhandle)
			{
				// No luck with this one.
				Com_Printf("Loading %s failed!\n", libcurl[i]);
				continue;
			}
			else
			{
				// Got one!
				Cvar_Set("cl_libcurl", (char *)libcurl[i]);
				break;
			}
		}

		if (!curlhandle)
		{
			goto error;
		}
	}

	// libcurl loaded sucessfully, connect the pointers.
	#define CONCURL(var, sym) do { \
		var = Sys_GetProcAddress(curlhandle, sym); \
		if (!var) goto error; \
	} while(0)
	
	CONCURL(qcurl_easy_cleanup, "curl_easy_cleanup");
	CONCURL(qcurl_easy_init, "curl_easy_init");
	CONCURL(qcurl_easy_getinfo, "curl_easy_getinfo");
	CONCURL(qcurl_easy_setopt, "curl_easy_setopt");
	CONCURL(qcurl_easy_strerror, "curl_easy_strerror");

	CONCURL(qcurl_global_cleanup, "curl_global_cleanup");
	CONCURL(qcurl_global_init, "curl_global_init");

	CONCURL(qcurl_multi_add_handle, "curl_multi_add_handle");
	CONCURL(qcurl_multi_cleanup, "curl_multi_cleanup");
	CONCURL(qcurl_multi_info_read, "curl_multi_info_read");
	CONCURL(qcurl_multi_init, "curl_multi_init");
	CONCURL(qcurl_multi_perform, "curl_multi_perform");
	CONCURL(qcurl_multi_remove_handle, "curl_multi_remove_handle");

	#undef CONCURL

	// And finally the global cURL initialization.
	qcurl_global_init(CURL_GLOBAL_NOTHING);
	qcurlInitialized = true;

	Com_Printf("------------------------------------\n\n");

	return true;

error:
	qcurlShutdown();

	Com_Printf("------------------------------------\n\n");

	return false;
}

/*
 * Calls cURLs global shutdown funtion,
 * unloads libcurl and set the function
 * pointers to NULL.
 */
void qcurlShutdown(void)
{
	if (qcurlInitialized)
	{
		Com_Printf("Shutting down curl.\n");

		qcurl_global_cleanup();
		qcurlInitialized = false;
	}

	qcurl_easy_cleanup = NULL;
	qcurl_easy_init = NULL;
	qcurl_easy_getinfo = NULL;
	qcurl_easy_setopt = NULL;
	qcurl_easy_strerror = NULL;

	qcurl_global_cleanup = NULL;
	qcurl_global_init = NULL;

	qcurl_multi_add_handle = NULL;
	qcurl_multi_cleanup = NULL;
	qcurl_multi_info_read = NULL;
	qcurl_multi_init = NULL;
	qcurl_multi_perform = NULL;
	qcurl_multi_remove_handle = NULL;

	if (curlhandle)
	{
		Sys_FreeLibrary(curlhandle);
		curlhandle = NULL;
	}
}

// --------

#endif // USE_CURL
