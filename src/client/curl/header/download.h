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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * =======================================================================
 *
 * Header for the HTTP / cURL download stuff.
 *
 * =======================================================================
 */

#ifdef USE_CURL

#ifndef DOWNLOAD_H
#define DOWNLOAD_H

// Number of max. parallel downloads.
#define MAX_HTTP_HANDLES 1

#include <curl/curl.h>
#include "../../../common/header/common.h"

typedef enum
{
	DLQ_STATE_NOT_STARTED,
	DLQ_STATE_RUNNING
} dlq_state;

typedef struct dlqueue_s
{
	struct dlqueue_s *next;
	char quakePath[MAX_QPATH];
	dlq_state state;
} dlqueue_t;

typedef struct dlhandle_s
{
	CURL *curl;
	char filePath[MAX_OSPATH];
	FILE *file;
	dlqueue_t *queueEntry;
	size_t fileSize;
	size_t position;
	char URL[576];
	char *tempBuffer;
} dlhandle_t;

extern cvar_t *cl_http_downloads;
extern cvar_t *cl_http_filelists;
extern cvar_t *cl_http_proxy;
extern cvar_t *cl_http_max_connections;

void CL_CancelHTTPDownloads(qboolean permKill);
void CL_InitHTTPDownloads(void);
qboolean CL_QueueHTTPDownload(const char *quakePath);
void CL_RunHTTPDownloads(void);
qboolean CL_PendingHTTPDownloads(void);
void CL_SetHTTPServer(const char *URL);
void CL_HTTP_Cleanup(qboolean fullShutdown);

#endif // DOWNLOAD_H
#endif // USE_CURL
