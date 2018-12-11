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

#ifndef QCURL_H
#define QCURL_H

// --------

#include <curl/curl.h>
#include "../../../common/header/common.h"

// --------

// True if cURL is initialized.
extern qboolean qcurlInitialized;

// Function pointers to cURL.
extern void (*qcurl_easy_cleanup)(CURL *curl);
extern CURL *(*qcurl_easy_init)(void);
extern CURLcode (*qcurl_easy_getinfo)(CURL *curl, CURLINFO info, ...);
extern CURLcode (*qcurl_easy_setopt)(CURL *curl, CURLoption option, ...);
extern const char *(*qcurl_easy_strerror)(CURLcode);

extern void (*qcurl_global_cleanup)(void);
extern CURLcode (*qcurl_global_init)(long flags);

extern CURLMcode (*qcurl_multi_add_handle)(CURLM *multi_handle, CURL *curl_handle);
extern CURLMcode (*qcurl_multi_cleanup)(CURLM *multi_handle);
extern CURLMsg *(*qcurl_multi_info_read)(CURLM *multi_handle, int *msgs_in_queue);
extern CURLM *(*qcurl_multi_init)(void);
extern CURLMcode (*qcurl_multi_perform)(CURLM *multi_handle, int *running_handles);
extern CURLMcode (*qcurl_multi_remove_handle)(CURLM *multi_handle, CURL *curl_handle);

// --------

// Loads and initialized cURL.
qboolean qcurlInit(void);

// Shuts cURL down and unloads it.
void qcurlShutdown(void);

// --------

#endif // QCURL_H
#endif // USE_CURL
