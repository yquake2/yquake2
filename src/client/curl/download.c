/*
 * Copyright (C) 1997-2001 Id Software, Inc.
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
 * Asset downloads over HTTP with CURL.
 *
 * =======================================================================
 */

#include <ctype.h>
#include "../header/client.h"
#include "header/qcurl.h"

#ifdef USE_CURL

cvar_t *cl_http_downloads;
cvar_t *cl_http_filelists;
cvar_t *cl_http_proxy;
cvar_t *cl_http_max_connections;

typedef enum
{
	HTTPDL_ABORT_NONE,
	HTTPDL_ABORT_SOFT,
	HTTPDL_ABORT_HARD
} http_abort_t;

static CURLM  *multi = NULL;
static int handleCount = 0;
static int pendingCount = 0;
static int abortDownloads = HTTPDL_ABORT_NONE;
static qboolean	httpDown = false;

// --------

// CURL callback functions
// -----------------------

/*
 * In memory buffer for receiving files. Used
 * as special CURL callback for filelists.
 */
static size_t CL_HTTP_Recv(void *ptr, size_t size, size_t nmemb, void *stream)
{
	dlhandle_t *dl = (dlhandle_t *)stream;
	size_t bytes = size * nmemb;

	if (!dl->fileSize)
	{
		double length = 0;

		qcurl_easy_getinfo(dl->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &length);

		// Mkay, the remote file should be at least one byte long.
		// Since this is used for paclists only we assume that the
		// file cannot be longer then 256k. Everythings else is
		// considered malicious or a broken server.
		if (length < 1 || length > 262144)
		{
			length = 262144.0f;
		}

		dl->fileSize = ceil(length) + 1;
		dl->tempBuffer = malloc(dl->fileSize);
	}
	else if (dl->position + bytes >= dl->fileSize)
	{
		dl->fileSize *= 2;
		realloc(dl->tempBuffer, dl->fileSize);
	}

	memcpy (dl->tempBuffer + dl->position, ptr, bytes);
	dl->position += bytes;
	dl->tempBuffer[dl->position] = 0;

	return bytes;
}

static size_t CL_HTTP_CurlWriteCB(char* data, size_t size, size_t nmemb, void* userdata)
{
	dlhandle_t *dl = (dlhandle_t *)userdata;
	return fwrite(data, size, nmemb, dl->file);
}

// --------

// Helper functions
// ----------------

/*
 * Escapes a path with HTTP encoding. We can't use the
 * function provided by CURL because it encodes to many
 * characters and filter some characters needed by Quake
 * out. Oh Yeah.
 */
static void CL_EscapeHTTPPath(const char *filePath, char *escaped)
{
	char *p = escaped;
	size_t len = strlen (filePath);

	for (int i = 0; i < len; i++)
	{
		if (!isalnum(filePath[i]) && filePath[i] != ';' && filePath[i] != '/' &&
			filePath[i] != '?' && filePath[i] != ':' && filePath[i] != '@' && filePath[i] != '&' &&
			filePath[i] != '=' && filePath[i] != '+' && filePath[i] != '$' && filePath[i] != ',' &&
			filePath[i] != '[' && filePath[i] != ']' && filePath[i] != '-' && filePath[i] != '_' &&
			filePath[i] != '.' && filePath[i] != '!' && filePath[i] != '~' && filePath[i] != '*' &&
			filePath[i] != '\'' && filePath[i] != '(' && filePath[i] != ')')
		{
			sprintf(p, "%%%02x", filePath[i]);
			p += 3;
		}
		else
		{
			*p = filePath[i];
			p++;
		}
	}

	p[0] = 0;

	// Using ./ in a url is legal, but all browsers condense
	// the path and some IDS / request filtering systems act
	// a bit funky if http requests come in with uncondensed
	// paths.
	len = strlen(escaped);
	p = escaped;

	while ((p = strstr (p, "./")))
	{
		memmove (p, p+2, len - (p - escaped) - 1);
		len -= 2;
	}
}

/*
 * Removes an entry from the download queue.
 */
static qboolean CL_RemoveFromQueue(dlqueue_t *entry)
{
	dlqueue_t *last = &cls.downloadQueue;
	dlqueue_t *cur = last->next;

	while (cur)
	{
		if (last->next == entry)
		{
			last->next = cur->next;
			Z_Free(cur);

			return true;
		}

		last = cur;
		cur = cur->next;
	}


	return false;
}

/*
 * Starts a download. Generates an URL, brings the
 * download handle into defined state and adds it
 * to the CURL multihandle.
 */
static void CL_StartHTTPDownload (dlqueue_t *entry, dlhandle_t *dl)
{
	char tempFile[MAX_OSPATH];
	char escapedFilePath[MAX_QPATH*4];

	size_t len = strlen(entry->quakePath);

	if (len > 9 && !strncmp(entry->quakePath + len - 9, ".filelist", len))
	{
		// Special case for filelists. The code identifies
		// filelist by the special handle NULL...
		dl->file = NULL;
		CL_EscapeHTTPPath(entry->quakePath, escapedFilePath);
	}
	else
	{
		// Full path to the local file.
		Com_sprintf (dl->filePath, sizeof(dl->filePath), "%s/%s", FS_Gamedir(), entry->quakePath);

		// Full path to the remote file.
		Com_sprintf (tempFile, sizeof(tempFile), "%s/%s", cl.gamedir, entry->quakePath);
		CL_EscapeHTTPPath (tempFile, escapedFilePath);

		// Create a temporary file where the downloaded data is stored...
		Q_strlcat(dl->filePath, ".tmp", sizeof(dl->filePath));
		FS_CreatePath (dl->filePath);

		// ...and put it into the download handle.
		dl->file = Q_fopen(dl->filePath, "wb");

		if (!dl->file)
		{
			Com_Printf("HTTP download: Couldn't open %s for writing\n", dl->filePath);
			CL_RemoveFromQueue(entry);
			pendingCount--;

			return;
		}
	}

	// Make sure that the download handle is in empty state.
	dl->tempBuffer = NULL;
	dl->fileSize = 0;
	dl->position = 0;
	dl->queueEntry = entry;

	// Setup and configure the CURL part of our download handle.
	if (!dl->curl)
	{
		dl->curl = qcurl_easy_init();
	}

	Com_sprintf(dl->URL, sizeof(dl->URL), "%s%s", cls.downloadServer, escapedFilePath);

	qcurl_easy_setopt(dl->curl, CURLOPT_ENCODING, "");

	if (dl->file)
	{
		qcurl_easy_setopt(dl->curl, CURLOPT_WRITEDATA, dl);
		qcurl_easy_setopt(dl->curl, CURLOPT_WRITEFUNCTION, CL_HTTP_CurlWriteCB);
	}
	else
	{
		qcurl_easy_setopt(dl->curl, CURLOPT_WRITEDATA, dl);
		qcurl_easy_setopt(dl->curl, CURLOPT_WRITEFUNCTION, CL_HTTP_Recv);
	}

	qcurl_easy_setopt(dl->curl, CURLOPT_PROXY, cl_http_proxy->string);
	qcurl_easy_setopt(dl->curl, CURLOPT_FOLLOWLOCATION, 1);
	qcurl_easy_setopt(dl->curl, CURLOPT_MAXREDIRS, 5);
	qcurl_easy_setopt(dl->curl, CURLOPT_PROGRESSDATA, dl);
	qcurl_easy_setopt(dl->curl, CURLOPT_USERAGENT, Cvar_VariableString ("version"));
	qcurl_easy_setopt(dl->curl, CURLOPT_REFERER, cls.downloadReferer);
	qcurl_easy_setopt(dl->curl, CURLOPT_URL, dl->URL);

	size_t ret;

	if ((ret = qcurl_multi_add_handle(multi, dl->curl)) != CURLM_OK)
	{
		Com_Printf("HTTP download: cURL error - %s\n", qcurl_easy_strerror(ret));
		CL_RemoveFromQueue(entry);

		return;
	}

	handleCount++;

	Com_DPrintf("CL_StartHTTPDownload: Fetching %s...\n", dl->URL);
	dl->queueEntry->state = DLQ_STATE_RUNNING;
}

/*
 * Checks if a line given in a filelist is a
 * valid file and puts it into the download
 * queue.
 */
static void CL_CheckAndQueueDownload(char *path)
{
	// NOTE: The original r1q2 download code in r1q2 allowed
	// only // pathes made of plain ASCII chars. YQ2 is more
	// or less UTF-8 clean, so we're allowing all characters.

	// Malicious filelists may have very long lines.
	size_t length = strlen(path);

	if (length >= MAX_QPATH)
	{
		return;
	}

	// The good, old problem with Windows being case sensitive
	// and Unix platforms are not. We're asuming that the
	// the server admin did the right things(tm). That should
	// be more or less save since r1q2, q2pro and q2dos are
	// doing the same. But for the sake of readbility and the
	// general state of my brain we're doing the security
	// checks on an lower case copy of the path.
	char lowerPath[MAX_QPATH];
	Q_strlcpy(lowerPath, path, sizeof(lowerPath));
	Q_strlwr(lowerPath);

	// All files must have an extension, extensionsless
	// files just make no sense for Quake II.
	char *ext = strrchr(lowerPath, '.');

	if (!ext)
	{
		return;
	}

	ext++;

	if (!ext[0])
	{
		return;
	}

	// These file extensions must be in sync with
	// fs_packtypes in filesystem.c!
	qboolean pak = false;

	if (!strcmp (ext, "pak") || !strcmp (ext, "pk2") ||
			!strcmp(ext, "pk3") || !strcmp(ext, "zip"))
	{
		pak = true;
	}

	// Filter out all filetypes not understood by Quake II.
	if (!pak && strcmp(ext, "pcx") && strcmp(ext, "wal") && strcmp(ext, "wav") && strcmp(ext, "md2") &&
		strcmp(ext, "sp2") && strcmp(ext, "tga") && strcmp(ext, "png") && strcmp(ext, "jpg") &&
		strcmp(ext, "bsp") && strcmp(ext, "ent") && strcmp(ext, "txt") && strcmp(ext, "dm2") &&
		strcmp(ext, "loc"))
	{
		Com_Printf ("HTTP download: Illegal file type '%s' in filelist\n", ext);

		return;
	}

	// Files starting with @ are game local. For example the code
	// wouldn't download conchars.pcx because it already exists in
	// the global scope. If it's prefixed with @ it's downloaded
	// anyways if it doesn't exists in the current game dir.
	qboolean gameLocal = false;

	if (path[0] == '@')
	{
		if (pak)
		{
			Com_Printf("HTTP download: @ prefix used on a pak file '%s' in filelist\n", path);

			return;
		}

		gameLocal = true;
		path++;
		length--;
	}

	// Make sure that there're no evil pathes in the filelist. Most
	// of these should be pretty okay with YQ2 since we've got a much
	// better filesystem as other clients but let's stay consistent.
	//
	// .. -> Don't change to upper dirs.
	// // -> Should be okay
	// \\ -> May fuck some string functions and CURL up. They
	//		 aren't valid URL characters anyways.
	//
	// !pak && !strchr (path, '/') -> Plain files are always loaded
	//								  from and into subdirs.
	//	(pak && strchr(path, '/') -> Paks are always loaded from and
	//								 into the toplevel dir.
	if (strstr (path, "..") || strstr(path, "//") || strchr (path, '\\') ||
			(!pak && !strchr(path, '/')) || (pak && strchr(path, '/')))
	{
		Com_Printf("HTTP download: Illegal path '%s' in filelist\n", path);

		return;
	}

	// Let's see if we've already got that file.
	qboolean exists = false;

	if (gameLocal || pak)
	{
		if (pak)
		{
			// We need to check paks ourself.
			char gamePath[MAX_OSPATH];

			Com_sprintf(gamePath, sizeof(gamePath),"%s/%s",FS_Gamedir(), path);

			FILE *f = Q_fopen(gamePath, "rb");

			if (f)
			{
				exists = true;
				fclose(f);
			}
		}
		else
		{
			// All other files are checked by the filesystem.
			exists = FS_FileInGamedir(path);
		}

		if (!exists)
		{
			// Queue the file for download.
			CL_QueueHTTPDownload(path);
		}
	}
	else
	{
		// Check if the file is already there and download it if not.
		CL_CheckOrDownloadFile(path);
	}
}

/*
 * Parses a filelist referenced in the memory
 * buffer in the given dlhandle_t.
 */
static void CL_ParseFileList(dlhandle_t *dl)
{
	if (!cl_http_filelists->value)
	{
		return;
	}

	char *list = dl->tempBuffer;

	for (;;)
	{
		char *p = strchr(list, '\n');

		if (p)
		{
			p[0] = 0;

			if (list[0])
			{
				CL_CheckAndQueueDownload(list);
			}

			list = p + 1;
		}
		else
		{
			if (list[0])
			{
				CL_CheckAndQueueDownload(list);
			}

			break;
		}
	}

	free(dl->tempBuffer);
	dl->tempBuffer = NULL;
}

/*
 * A pak file was just downloaded. Go through the
 * download queue and check if we can cancel some
 * downloads.
 */
static void CL_ReVerifyHTTPQueue (void)
{
	dlqueue_t *q = &cls.downloadQueue;

	pendingCount = 0;

	while (q->next)
	{
		q = q->next;

		if (q->state == DLQ_STATE_NOT_STARTED)
		{
			if (FS_LoadFile (q->quakePath, NULL) != -1)
			{
				CL_RemoveFromQueue(q);
			}
			else
			{
				pendingCount++;
			}
		}
	}
}

/*
 * Processesall finished downloads. React on
 * errors, if there're none process the file.
 */
static void CL_FinishHTTPDownload(void)
{
	CURL *curl;
	char tempName[MAX_OSPATH];
	dlhandle_t *dl = NULL;
	int	msgs_in_queue;
	qboolean isFile;
	size_t i;

	do
	{
		// Get a message from CURL.
		CURLMsg *msg = qcurl_multi_info_read(multi, &msgs_in_queue);

		if (!msg)
		{
			return;
		}

		if (msg->msg != CURLMSG_DONE)
		{
			continue;
		}

		// Find the download handle for the message.
		curl = msg->easy_handle;

		for (i = 0; i < MAX_HTTP_HANDLES; i++)
		{
			if (cls.HTTPHandles[i].curl == curl)
			{
				dl = &cls.HTTPHandles[i];
				break;
			}
		}

		if (i == MAX_HTTP_HANDLES)
		{
			Com_Error(ERR_DROP, "CL_FinishHTTPDownload: Handle not found");
		}

		// Some files aren't saved but read
		// into memory buffers. This is used
		// for filelists only.
		if (dl->file)
		{
			isFile = true;

			// Mkay, it's a file. Let's
			// close it's handle.
			fclose(dl->file);
			dl->file = NULL;
		}
		else
		{
			isFile = false;
		}

		// All downloads might have been aborted.
		// This is the case if the backend (or the
		// whole client) shuts down.
		if (pendingCount)
		{
			pendingCount--;
		}

		// The file finished downloading, it's
		// handle it's now empty and ready for
		// reuse.
		handleCount--;

		// Get the download result (success, some
		// error, etc.) from CURL and process it.
		CURLcode result = msg->data.result;
		long responseCode = 0;

		switch (result)
		{
			case CURLE_HTTP_RETURNED_ERROR:
			case CURLE_OK:
				qcurl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

				if (responseCode == 404)
				{
					Com_Printf("HTTP download: %s - File Not Found\n", dl->queueEntry->quakePath);

					// We got a 404, remove the target file.
					if (isFile)
					{
						Sys_Remove(dl->filePath);
						isFile = false;
					}

					// ...and remove it from the CURL multihandle.
					qcurl_multi_remove_handle(multi, dl->curl);
					CL_RemoveFromQueue(dl->queueEntry);
					dl->queueEntry = NULL;


					break;

				}
				else if (responseCode == 200)
				{
					Com_Printf("HTTP download: %s - OK\n", dl->queueEntry->quakePath);

					// This wasn't a file, so it must be a filelist.
					if (!isFile && !abortDownloads)
					{
						CL_ParseFileList(dl);
						CL_RemoveFromQueue(dl->queueEntry);
						dl->queueEntry = NULL;
					}

					break;
				}


			// Everything that's not 200 and 404 is fatal, fall through.
			case CURLE_COULDNT_RESOLVE_HOST:
			case CURLE_COULDNT_CONNECT:
			case CURLE_COULDNT_RESOLVE_PROXY:
				Com_Printf("HTTP download: %s - Server broken, aborting\n", dl->queueEntry->quakePath);

				// The download failed. Remove the temporary file...
				if (isFile)
				{
					Sys_Remove(dl->filePath);
					isFile = false;
				}

				// ...and the handle from CURLs mutihandle.
				qcurl_multi_remove_handle(multi, dl->curl);

				// Special case: We're already aborting HTTP downloading,
				// so we can't just kill everything. Otherwise we'll get
				// stuck.
				if (abortDownloads)
				{
					CL_RemoveFromQueue(dl->queueEntry);
					dl->queueEntry = NULL;
				}

				// Abort all HTTP downloads.
				CL_CancelHTTPDownloads (true);
				CL_RemoveFromQueue(dl->queueEntry);
				dl->queueEntry = NULL;

				break;

			default:
				Com_Printf ("HTTP download: cURL error - %s\n", qcurl_easy_strerror(result));

				// The download failed. Remove the temporary file...
				if (isFile)
				{
					Sys_Remove(dl->filePath);
					isFile = false;
				}

				// ...and the handle from CURLs mutihandle.
				qcurl_multi_remove_handle (multi, dl->curl);
				CL_RemoveFromQueue(dl->queueEntry);
				dl->queueEntry = NULL;

				break;
		}

		if (isFile)
		{
			// Rename the temporary file to it's final location
			Com_sprintf(tempName, sizeof(tempName), "%s/%s", FS_Gamedir(), dl->queueEntry->quakePath);
			Sys_Rename(dl->filePath, tempName);

			// Pak files are special because they contain
			// other files that we may be downloading...
			i = strlen(tempName);

			// The list of file types must be consistent with fs_packtypes in filesystem.c.
			if ( !strcmp (tempName + i - 4, ".pak") || !strcmp (tempName + i - 4, ".pk2") ||
					!strcmp (tempName + i - 4, ".pk3") || !strcmp (tempName + i - 4, ".zip") )
			{
				FS_AddPAKFromGamedir(dl->queueEntry->quakePath);
				CL_ReVerifyHTTPQueue ();
			}

			CL_RemoveFromQueue(dl->queueEntry);
			dl->queueEntry = NULL;
		}

		// Remove the file fo CURLs multihandle.
		qcurl_multi_remove_handle (multi, dl->curl);
	} while (msgs_in_queue > 0);


	// No more downloads are in in flight, so...
	if (handleCount == 0)
	{
		if (abortDownloads == HTTPDL_ABORT_SOFT)
		{
			// ...if we're soft aborting we're done.
			abortDownloads = HTTPDL_ABORT_NONE;
		}
		else if (abortDownloads == HTTPDL_ABORT_HARD)
		{
			// ...if we're hard aborting we need to prevent future downloads.
			Q_strlcpy(cls.downloadServerRetry, cls.downloadServer, sizeof(cls.downloadServerRetry));
			cls.downloadServer[0] = 0;
		}
	}

	// All downloads done. Let's check if we've got more files to
	// request. This can be the case if we've processed a filelist
	// or downloaded a BSP that references other assets.
	if (cls.state == ca_connected && !CL_PendingHTTPDownloads())
	{
		CL_RequestNextDownload();
	}
}

/*
 * Returns a free download handle.
 */
static dlhandle_t *CL_GetFreeDLHandle(void)
{
	for (int i = 0; i < MAX_HTTP_HANDLES; i++)
	{
		dlhandle_t *dl = &cls.HTTPHandles[i];

		if (!dl->queueEntry)
		{
			return dl;
		}
	}

	return NULL;
}

/*
 * Starts the next download.
 */
static void CL_StartNextHTTPDownload(void)
{
	dlqueue_t *q = &cls.downloadQueue;

	while (q->next)
	{
		q = q->next;

		if (q->state == DLQ_STATE_NOT_STARTED)
		{
			dlhandle_t *dl = CL_GetFreeDLHandle();

			if (!dl)
			{
				return;
			}

			CL_StartHTTPDownload(q, dl);

			break;
		}
	}
}

// --------

// Startup and shutdown
// --------------------

/*
 * Initializes CURL.
 */
void CL_InitHTTPDownloads (void)
{
	// We're initializing the cURL backend here because
	// currently we're the only user. As soon as there
	// are other users this must be moved up into the
	// global client intialization.
	qcurlInit();
}

/*
 * Resets the internal state and - in case
 * of full shutdown - shuts CURL down.
 */
void CL_HTTP_Cleanup(qboolean fullShutdown)
{
	if (fullShutdown && httpDown)
	{
		return;
	}

	// Cleanup all internal handles.
	for (int i = 0; i < MAX_HTTP_HANDLES; i++)
	{
		dlhandle_t *dl = &cls.HTTPHandles[i];

		if (dl->file)
		{
			fclose (dl->file);
			Sys_Remove (dl->filePath);
			dl->file = NULL;
		}

		if (dl->tempBuffer)
		{
			Z_Free (dl->tempBuffer);
			dl->tempBuffer = NULL;
		}

		if (dl->curl)
		{
			if (multi)
			{
				qcurl_multi_remove_handle(multi, dl->curl);
			}

			qcurl_easy_cleanup(dl->curl);
			dl->curl = NULL;
		}

		dl->queueEntry = NULL;
	}

	// Cleanup download queue.
	dlqueue_t *q = &cls.downloadQueue;
	dlqueue_t *last = NULL;

	while (q->next)
	{
		q = q->next;

		if (last)
		{
			Z_Free (last);
		}

		last = q;
	}

	if (last)
	{
		Z_Free (last);
	}

	// Cleanup CURL multihandle.
	if (multi)
	{
		qcurl_multi_cleanup(multi);
		multi = NULL;
	}

	// Shutdown CURL.
	if (fullShutdown)
	{
		// This must be moved up into the generic
		// client shutdown as soon as there're
		// other users of the cURL backend.
		qcurlShutdown();
		httpDown = true;
	}
}
// --------

// Public functions
// ----------------

/*
 * The server hand us a new HTTP URL. Nuke
 * the internal state to start over.
 */
void CL_SetHTTPServer (const char *URL)
{
	dlqueue_t *last = NULL;
	dlqueue_t *q = &cls.downloadQueue;

	// This code abuses the download server setting to
	// determine if HTTP downloads are possible. So if
	// we don't set a download server, no downloads are
	// queued and run. :)
	if (!qcurlInitialized)
	{
		cls.downloadServer[0] = 0;
		return;
	}

	CL_HTTP_Cleanup(false);

	// Cleanup download queues.
	while (q->next)
	{
		q = q->next;

		if (last)
		{
			Z_Free (last);
		}

		last = q;
	}

	if (last)
	{
		Z_Free (last);
	}

	memset (&cls.downloadQueue, 0, sizeof(cls.downloadQueue));

	// Cleanup internal state.
	abortDownloads = HTTPDL_ABORT_NONE;
	handleCount = pendingCount = 0;
	cls.downloadServerRetry[0] = 0;
	Q_strlcpy(cls.downloadServer, URL, sizeof(cls.downloadServer) - 1);

	// Initializes a new multihandle.
	if (multi)
	{
		Com_Error(ERR_DROP, "HTTP download: Still have old handle?!");
	}

	multi = qcurl_multi_init();
}

/*
 * Cancels all downloads and clears the queue.
 */
void CL_CancelHTTPDownloads(qboolean permKill)
{
	if (permKill)
	{
		CL_ResetPrecacheCheck();
		abortDownloads = HTTPDL_ABORT_HARD;
	}
	else
	{
		abortDownloads = HTTPDL_ABORT_SOFT;
	}

	dlqueue_t *q = &cls.downloadQueue;

	while (q->next)
	{
		q = q->next;

		if (q->state == DLQ_STATE_NOT_STARTED)
		{
			CL_RemoveFromQueue(q);
		}
	}

	if (!pendingCount && !handleCount && abortDownloads == HTTPDL_ABORT_HARD)
	{
		cls.downloadServer[0] = 0;
	}

	pendingCount = 0;
}

/*
 * This function should be called from old UDP download code
 * during the precache phase to determine if HTTP downloads
 * for the requested files are possible. Queues the download
 * and returns true if yes, returns fales if not.
 */
qboolean CL_QueueHTTPDownload(const char *quakePath)
{
	// Not HTTP servers were send by the server, HTTP is disabled
	// or the client is shutting down and we're wrapping up.
	if (!cls.downloadServer[0] || abortDownloads || !cl_http_downloads->value)
	{
		return false;
	}

	// Mkay, now that the first download is queued we want
	// the generic(!) filelist.
	qboolean needList = false;

	if (!cls.downloadQueue.next && cl_http_filelists->value)
	{
		needList = true;
	}

	// Queue the download.
	dlqueue_t *q = &cls.downloadQueue;

	while (q->next)
	{
		q = q->next;

		// The generic code may request the same
		// file more than one time. *sigh*
		if (!strcmp(quakePath, q->quakePath))
		{
			return true;
		}
	}

	q->next = Z_TagMalloc(sizeof(*q), 0);
	q = q->next;
	q->next = NULL;
	q->state = DLQ_STATE_NOT_STARTED;
	Q_strlcpy(q->quakePath, quakePath, sizeof(q->quakePath) - 1);

	// Let's download the generic filelist if necessary.
	if (needList)
	{
		CL_QueueHTTPDownload("/.filelist");
	}

	// If we just queued a .bsp file ask for it's map
	// filelist. I don't think that this is good idea.
	// Cause a filelist can specifiy a .bsp that again
	// specifies a filelist... But r1q2 chose this way,
	// others followed and we have no choice but doing
	// the same.
	size_t len = strlen (quakePath);

	if (cl_http_filelists->value && len > 4 && !Q_stricmp((char *)(quakePath + len - 4), ".bsp"))
	{
		char listPath[MAX_OSPATH];
		char filePath[MAX_OSPATH];

		Com_sprintf (filePath, sizeof(filePath), "%s/%s", cl.gamedir, quakePath);
		COM_StripExtension (filePath, listPath);
		Q_strlcat(listPath, ".filelist", sizeof(listPath));

		CL_QueueHTTPDownload(listPath);
	}

	// If we're here CL_FinishHTTPDownload() is guaranteed to be called.
	pendingCount++;

	Com_Printf("HTTP download: %s - Queued\n", q->quakePath);

	return true;
}

/*
 * Returns true if still downloads pending and false
 * if not. Used by the old UDP code during precache
 * phase to determine if it's necessary to wait for
 * outstanding download.
 */
qboolean CL_PendingHTTPDownloads(void)
{
	if (!cls.downloadServer[0])
	{
		return false;
	}

	return pendingCount + handleCount;
}

/*
 * Calls CURL to perform the actual downloads.
 * Must be called every frame, otherwise CURL
 * will starve.
 */
void CL_RunHTTPDownloads(void)
{
	int	newHandleCount;
	CURLMcode ret;

	// No HTTP server given or not initialized.
	if (!cls.downloadServer[0])
	{
		return;
	}

	// Kick CURL into action.
	do
	{
		ret = qcurl_multi_perform(multi, &newHandleCount);

		if (newHandleCount < handleCount)
		{
			CL_FinishHTTPDownload();
			handleCount = newHandleCount;
		}
	}
	while (ret == CURLM_CALL_MULTI_PERFORM);

	// Somethings gone very wrong.
	if (ret != CURLM_OK)
	{
		Com_Printf("HTTP download: cURL error - %s\n", qcurl_easy_strerror(ret));
		CL_CancelHTTPDownloads(true);
	}

	// Not enough downloads running, start some more.
	if (pendingCount && abortDownloads == HTTPDL_ABORT_NONE &&
			handleCount < cl_http_max_connections->value)
	{
		CL_StartNextHTTPDownload();
	}
}

#endif /* USE_CURL */
