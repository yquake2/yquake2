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
 * The Quake II file system, implements generic file system operations
 * as well as the .pak file format and support for .pk3 files.
 *
 * =======================================================================
 */

#include "header/common.h"
#include "../common/header/glob.h"

#ifdef ZIP
 #include "unzip/unzip.h"
#endif

#define MAX_HANDLES 512
#define MAX_PAKS 100

#ifdef SYSTEMWIDE
 #ifndef SYSTEMDIR
  #define SYSTEMDIR "/usr/share/games/quake2"
 #endif
#endif

typedef struct
{
	char name[MAX_QPATH];
	fsMode_t mode;
	FILE *file;           /* Only one will be used. */
#ifdef ZIP
	unzFile *zip;        /* (file or zip) */
#endif
} fsHandle_t;

typedef struct fsLink_s
{
	char *from;
	char *to;
	int length;
	struct fsLink_s *next;
} fsLink_t;

typedef struct
{
	char name[MAX_QPATH];
	int size;
	int offset;     /* Ignored in PK3 files. */
} fsPackFile_t;

typedef struct
{
	char name[MAX_OSPATH];
	int numFiles;
	FILE *pak;
#ifdef ZIP
	unzFile *pk3;
#endif
	fsPackFile_t *files;
} fsPack_t;

typedef struct fsSearchPath_s
{
	char path[MAX_OSPATH]; /* Only one used. */
	fsPack_t *pack; /* (path or pack) */
	struct fsSearchPath_s *next;
} fsSearchPath_t;

typedef enum
{
	PAK,
#ifdef ZIP
	PK3
#endif
} fsPackFormat_t;

typedef struct
{
	char *suffix;
	fsPackFormat_t format;
} fsPackTypes_t;

fsHandle_t fs_handles[MAX_HANDLES];
fsLink_t *fs_links;
fsSearchPath_t *fs_searchPaths;
fsSearchPath_t *fs_baseSearchPaths;

/* Pack formats / suffixes. */
fsPackTypes_t fs_packtypes[] = {
	{"pak", PAK},
#ifdef ZIP
	{"pk2", PK3},
	{"pk3", PK3},
	{"zip", PK3}
#endif
};

char fs_gamedir[MAX_OSPATH];
static char fs_currentGame[MAX_QPATH];

static char fs_fileInPath[MAX_OSPATH];
static qboolean fs_fileInPack;

/* Set by FS_FOpenFile. */
int file_from_pak = 0;
#ifdef ZIP
int file_from_pk3 = 0;
char file_from_pk3_name[MAX_QPATH];
#endif

cvar_t *fs_homepath;
cvar_t *fs_basedir;
cvar_t *fs_cddir;
cvar_t *fs_gamedirvar;
cvar_t *fs_debug;

fsHandle_t *FS_GetFileByHandle(fileHandle_t f);
char *Sys_GetCurrentDirectory(void);

/*
 * All of Quake's data access is through a hierchal file system, but the
 * contents of the file system can be transparently merged from several
 * sources.
 *
 * The "base directory" is the path to the directory holding the quake.exe and
 * all game directories.  The sys_* files pass this to host_init in
 * quakeparms_t->basedir.  This can be overridden with the "-basedir" command
 * line parm to allow code debugging in a different directory.  The base
 * directory is only used during filesystem initialization.
 *
 * The "game directory" is the first tree on the search path and directory that
 * all generated files (savegames, screenshots, demos, config files) will be
 * saved to.  This can be overridden with the "-game" command line parameter.
 * The game directory can never be changed while quake is executing.  This is
 * a precacution against having a malicious server instruct clients to write
 * files over areas they shouldn't.
 *
 */

/*
 * Returns the path up to, but not including the last '/'.
 */
void
Com_FilePath(const char *path, char *dst, int dstSize)
{
	char *pos; /* Position of the last '/'. */

	if ((pos = strrchr(path, '/')) != NULL)
	{
		pos--;

		if ((pos - path) < dstSize)
		{
			memcpy(dst, path, pos - path);
			dst[pos - path] = '\0';
		}
		else
		{
			Com_Printf("Com_FilePath: not enough space.\n");
			return;
		}
	}
	else
	{
		Q_strlcpy(dst, path, dstSize);
	}
}

int
FS_FileLength(FILE *f)
{
	int pos; /* Current position. */
	int end; /* End of file. */

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell(f);
	fseek(f, pos, SEEK_SET);

	return end;
}

/*
 * Creates any directories needed to store the given filename.
 */
void
FS_CreatePath(char *path)
{
	char *cur; /* Current '/'. */
	char *old; /* Old '/'. */

	FS_DPrintf("FS_CreatePath(%s)\n", path);

	if (strstr(path, "..") != NULL)
	{
		Com_Printf("WARNING: refusing to create relative path '%s'.\n", path);
		return;
	}

	cur = old = path;

	while (cur != NULL)
	{
		if ((cur - old) > 1)
		{
			*cur = '\0';
			Sys_Mkdir(path);
			*cur = '/';
		}

		old = cur;
		cur = strchr(old + 1, '/');
	}
}

void
FS_DPrintf(const char *format, ...)
{
	char msg[1024];
	va_list argPtr;

	if (fs_debug->value != 1)
	{
		return;
	}

	va_start(argPtr, format);
	vsnprintf(msg, sizeof(msg), format, argPtr);
	va_end(argPtr);

	Com_Printf("%s", msg);
}

char *
FS_Gamedir(void)
{
	return fs_gamedir;
}

/*
 * Finds a free fileHandle_t.
 */
fsHandle_t *
FS_HandleForFile(const char *path, fileHandle_t *f)
{
	int i;
	fsHandle_t *handle;

	handle = fs_handles;

	for (i = 0; i < MAX_HANDLES; i++, handle++)
	{
		if ((handle->file == NULL)
#ifdef ZIP
			 && (handle->zip == NULL)
#endif
			)
		{
			Q_strlcpy(handle->name, path, sizeof(handle->name));
			*f = i + 1;
			return handle;
		}
	}

	/* Failed. */
	Com_Error(ERR_DROP, "FS_HandleForFile: none free");

	return NULL;
}

/*
 * Returns a fsHandle_t * for the given fileHandle_t.
 */
fsHandle_t *
FS_GetFileByHandle(fileHandle_t f)
{
	if ((f < 0) || (f > MAX_HANDLES))
	{
		Com_Error(ERR_DROP, "FS_GetFileByHandle: out of range");
	}

	if (f == 0)
	{
		f++;
	}

	return &fs_handles[f - 1];
}

/*
 * Returns file size or -1 on error.
 */
int
FS_FOpenFileAppend(fsHandle_t *handle)
{
	char path[MAX_OSPATH];

	FS_CreatePath(handle->name);

	Com_sprintf(path, sizeof(path), "%s/%s", fs_gamedir, handle->name);

	handle->file = fopen(path, "ab");

	if (handle->file)
	{
		if (fs_debug->value)
		{
			Com_Printf("FS_FOpenFileAppend: '%s'.\n", path);
		}

		return FS_FileLength(handle->file);
	}

	if (fs_debug->value)
	{
		Com_Printf("FS_FOpenFileAppend: couldn't open '%s'.\n", path);
	}

	return -1;
}

/*
 * Always returns 0 or -1 on error.
 */
int
FS_FOpenFileWrite(fsHandle_t *handle)
{
	char path[MAX_OSPATH];

	FS_CreatePath(handle->name);

	Com_sprintf(path, sizeof(path), "%s/%s", fs_gamedir, handle->name);

	if ((handle->file = fopen(path, "wb")) != NULL)
	{
		if (fs_debug->value)
		{
			Com_Printf("FS_FOpenFileWrite: '%s'.\n", path);
		}

		return 0;
	}

	if (fs_debug->value)
	{
		Com_Printf("FS_FOpenFileWrite: couldn't open '%s'.\n", path);
	}

	return -1;
}

/*
 * Other dll's can't just call fclose() on files returned by FS_FOpenFile.
 */
void
FS_FCloseFile(fileHandle_t f)
{
	fsHandle_t *handle;

	handle = FS_GetFileByHandle(f);

	if (handle->file)
	{
		fclose(handle->file);
	}
#ifdef ZIP
	else if (handle->zip)
	{
		unzCloseCurrentFile(handle->zip);
		unzClose(handle->zip);
	}
#endif

	memset(handle, 0, sizeof(*handle));
}

int
Developer_searchpath(int who)
{
	fsSearchPath_t *search;

	for (search = fs_searchPaths; search; search = search->next)
	{
		if (strstr(search->path, "xatrix"))
		{
			return 1;
		}

		if (strstr(search->path, "rogue"))
		{
			return 2;
		}
	}

	return 0;
}

/*
 * Finds the file in the search path. Returns filesize and an open FILE *. Used
 * for streaming data out of either a pak file or a seperate file.
 */
int
FS_FOpenFile(const char *name, fileHandle_t *f, qboolean gamedir_only)
{
	char path[MAX_OSPATH];
	fsHandle_t *handle;
	fsPack_t *pack;
	fsSearchPath_t *search;
	int i;

	file_from_pak = 0;
#ifdef ZIP
	file_from_pk3 = 0;
#endif

	handle = FS_HandleForFile(name, f);
	Q_strlcpy(handle->name, name, sizeof(handle->name));
	handle->mode = FS_READ;

	/* Search through the path, one element at a time. */
	for (search = fs_searchPaths; search; search = search->next)
	{
		if (gamedir_only)
		{
			if (strstr(search->path, FS_Gamedir()) == NULL)
			{
				break;
			}
		}

		/* Search inside a pack file. */
		if (search->pack)
		{
			pack = search->pack;

			for (i = 0; i < pack->numFiles; i++)
			{
				if (Q_stricmp(pack->files[i].name, handle->name) == 0)
				{
					/* Found it! */
					Com_FilePath(pack->name, fs_fileInPath, sizeof(fs_fileInPath));
					fs_fileInPack = true;

					if (fs_debug->value)
					{
						Com_Printf("FS_FOpenFile: '%s' (found in '%s').\n",
								   handle->name, pack->name);
					}

					if (pack->pak)
					{
						/* PAK */
						file_from_pak = 1;
						handle->file = fopen(pack->name, "rb");

						if (handle->file)
						{
							fseek(handle->file, pack->files[i].offset, SEEK_SET);
							return pack->files[i].size;
						}
					}
#ifdef ZIP
					else if (pack->pk3)
					{
						/* PK3 */
						file_from_pk3 = 1;
						Q_strlcpy(file_from_pk3_name, strrchr(pack->name, '/') + 1, sizeof(file_from_pk3_name));
						handle->zip = unzOpen(pack->name);

						if (handle->zip)
						{
							if (unzLocateFile(handle->zip, handle->name, 2) == UNZ_OK)
							{
								if (unzOpenCurrentFile(handle->zip) == UNZ_OK)
								{
									return pack->files[i].size;
								}
							}

							unzClose(handle->zip);
						}
					}
#endif

					Com_Error(ERR_FATAL, "Couldn't reopen '%s'", pack->name);
				}
			}
		}
		else
		{
			/* Search in a directory tree. */
			Com_sprintf(path, sizeof(path), "%s/%s", search->path, handle->name);

			handle->file = fopen(path, "rb");

			if (!handle->file)
			{
				Q_strlwr(path);
				handle->file = fopen(path, "rb");
			}

			if (!handle->file)
			{
				continue;
			}

			if (handle->file)
			{
				/* Found it! */
				Q_strlcpy(fs_fileInPath, search->path, sizeof(fs_fileInPath));
				fs_fileInPack = false;

				if (fs_debug->value)
				{
					Com_Printf("FS_FOpenFile: '%s' (found in '%s').\n",
							   handle->name, search->path);
				}

				return FS_FileLength(handle->file);
			}
		}
	}

	/* Not found! */
	fs_fileInPath[0] = 0;
	fs_fileInPack = false;

	if (fs_debug->value)
	{
		Com_Printf("FS_FOpenFile: couldn't find '%s'.\n", handle->name);
	}

	/* Couldn't open, so free the handle. */
	memset(handle, 0, sizeof(*handle));
	*f = 0;
	return -1;
}

/*
 * Properly handles partial reads.
 */
int
FS_Read(void *buffer, int size, fileHandle_t f)
{
	qboolean tried = false;    /* Tried to read from a CD. */
	byte *buf;        /* Buffer. */
	int r;         /* Number of bytes read. */
	int remaining;        /* Remaining bytes. */
	fsHandle_t *handle;  /* File handle. */

	handle = FS_GetFileByHandle(f);

	/* Read. */
	remaining = size;
	buf = (byte *)buffer;

	while (remaining)
	{
		if (handle->file)
		{
			r = fread(buf, 1, remaining, handle->file);
		}
#ifdef ZIP
		else if (handle->zip)
		{
			r = unzReadCurrentFile(handle->zip, buf, remaining);
		}
#endif
		else
		{
			return 0;
		}

		if (r == 0)
		{
			if (!tried)
			{
				/* Might tried to read from a CD. */
				tried = true;
			}
			else
			{
				/* Already tried once. */
				Com_Error(ERR_FATAL,
						va("FS_Read: 0 bytes read from '%s'", handle->name));
				return size - remaining;
			}
		}
		else if (r == -1)
		{
			Com_Error(ERR_FATAL,
					"FS_Read: -1 bytes read from '%s'",
					handle->name);
		}

		remaining -= r;
		buf += r;
	}

	return size;
}

/*
 * Properly handles partial reads of size up to count times. No error if it
 * can't read.
 */
int
FS_FRead(void *buffer, int size, int count, fileHandle_t f)
{
	qboolean tried = false;    /* Tried to read from a CD. */
	byte *buf;        /* Buffer. */
	int loops;         /* Loop indicator. */
	int r;         /* Number of bytes read. */
	int remaining;         /* Remaining bytes. */
	fsHandle_t *handle;  /* File handle. */

	handle = FS_GetFileByHandle(f);

	/* Read. */
	loops = count;
	buf = (byte *)buffer;

	while (loops)
	{
		/* Read in chunks. */
		remaining = size;

		while (remaining)
		{
			if (handle->file)
			{
				r = fread(buf, 1, remaining, handle->file);
			}
#ifdef ZIP
			else if (handle->zip)
			{
				r = unzReadCurrentFile(handle->zip, buf, remaining);
			}
#endif
			else
			{
				return 0;
			}

			if (r == 0)
			{
				if (!tried)
				{
					/* Might tried to read from a CD. */
					tried = true;
				}
				else
				{
					/* Already tried once. */
					return size - remaining;
				}
			}
			else if (r == -1)
			{
				Com_Error(ERR_FATAL,
						"FS_FRead: -1 bytes read from '%s'",
						handle->name);
			}

			remaining -= r;
			buf += r;
		}

		loops--;
	}

	return size;
}

/*
 * Filename are reletive to the quake search path. A null buffer will just
 * return the file length without loading.
 */
int
FS_LoadFile(char *path, void **buffer)
{
	byte *buf; /* Buffer. */
	int size; /* File size. */
	fileHandle_t f; /* File handle. */

	buf = NULL;
	size = FS_FOpenFile(path, &f, false);

	if (size <= 0)
	{
		if (buffer)
		{
			*buffer = NULL;
		}

		return size;
	}

	if (buffer == NULL)
	{
		FS_FCloseFile(f);
		return size;
	}

	buf = Z_Malloc(size);
	*buffer = buf;

	FS_Read(buf, size, f);
	FS_FCloseFile(f);

	return size;
}

void
FS_FreeFile(void *buffer)
{
	if (buffer == NULL)
	{
		FS_DPrintf("FS_FreeFile: NULL buffer.\n");
		return;
	}

	Z_Free(buffer);
}

/*
 * Takes an explicit (not game tree related) path to a pak file.
 *
 * Loads the header and directory, adding the files at the beginning of the
 * list so they override previous pack files.
 */
fsPack_t *
FS_LoadPAK(const char *packPath)
{
	int i; /* Loop counter. */
	int numFiles; /* Number of files in PAK. */
	FILE *handle; /* File handle. */
	fsPackFile_t *files; /* List of files in PAK. */
	fsPack_t *pack; /* PAK file. */
	dpackheader_t header; /* PAK file header. */
	dpackfile_t info[MAX_FILES_IN_PACK]; /* PAK info. */

	handle = fopen(packPath, "rb");

	if (handle == NULL)
	{
		return NULL;
	}

	fread(&header, 1, sizeof(dpackheader_t), handle);

	if (LittleLong(header.ident) != IDPAKHEADER)
	{
		fclose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPAK: '%s' is not a pack file", packPath);
	}

	header.dirofs = LittleLong(header.dirofs);
	header.dirlen = LittleLong(header.dirlen);

	numFiles = header.dirlen / sizeof(dpackfile_t);

	if ((numFiles > MAX_FILES_IN_PACK) || (numFiles == 0))
	{
		fclose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPAK: '%s' has %i files",
				packPath, numFiles);
	}

	files = Z_Malloc(numFiles * sizeof(fsPackFile_t));

	fseek(handle, header.dirofs, SEEK_SET);
	fread(info, 1, header.dirlen, handle);

	/* Parse the directory. */
	for (i = 0; i < numFiles; i++)
	{
		Q_strlcpy(files[i].name, info[i].name, sizeof(files[i].name));
		files[i].offset = LittleLong(info[i].filepos);
		files[i].size = LittleLong(info[i].filelen);
	}

	pack = Z_Malloc(sizeof(fsPack_t));
	Q_strlcpy(pack->name, packPath, sizeof(pack->name));
	pack->pak = handle;
#ifdef ZIP
	pack->pk3 = NULL;
#endif
	pack->numFiles = numFiles;
	pack->files = files;

	Com_Printf("Added packfile '%s' (%i files).\n", pack, numFiles);

	return pack;
}

#ifdef ZIP
/*
 * Takes an explicit (not game tree related) path to a pack file.
 *
 * Loads the header and directory, adding the files at the beginning of the list
 * so they override previous pack files.
 */
fsPack_t *
FS_LoadPK3(const char *packPath)
{
	char fileName[MAX_QPATH]; /* File name. */
	int i = 0; /* Loop counter. */
	int numFiles; /* Number of files in PK3. */
	int status; /* Error indicator. */
	fsPackFile_t *files; /* List of files in PK3. */
	fsPack_t *pack; /* PK3 file. */
	unzFile *handle; /* Zip file handle. */
	unz_file_info info; /* Zip file info. */
	unz_global_info global; /* Zip file global info. */

	handle = unzOpen(packPath);

	if (handle == NULL)
	{
		return NULL;
	}

	if (unzGetGlobalInfo(handle, &global) != UNZ_OK)
	{
		unzClose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPK3: '%s' is not a pack file", packPath);
	}

	numFiles = global.number_entry;

	if ((numFiles > MAX_FILES_IN_PACK) || (numFiles == 0))
	{
		unzClose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPK3: '%s' has %i files",
				packPath, numFiles);
	}

	files = Z_Malloc(numFiles * sizeof(fsPackFile_t));

	/* Parse the directory. */
	status = unzGoToFirstFile(handle);

	while (status == UNZ_OK)
	{
		fileName[0] = '\0';
		unzGetCurrentFileInfo(handle, &info, fileName, MAX_QPATH,
				NULL, 0, NULL, 0);
		Q_strlcpy(files[i].name, fileName, sizeof(files[i].name));
		files[i].offset = -1; /* Not used in ZIP files */
		files[i].size = info.uncompressed_size;
		i++;
		status = unzGoToNextFile(handle);
	}

	pack = Z_Malloc(sizeof(fsPack_t));
	Q_strlcpy(pack->name, packPath, sizeof(pack->name));
	pack->pak = NULL;
	pack->pk3 = handle;
	pack->numFiles = numFiles;
	pack->files = files;

	Com_Printf("Added packfile '%s' (%i files).\n", pack, numFiles);

	return pack;
}
#endif

/*
 * Adds the directory to the head of the path, then loads and adds pak1.pak
 * pak2.pak ...
 *
 * Extended all functionality to include Quake III .pk3
 */
void
FS_AddGameDirectory(const char *dir)
{
	char **list; /* File list. */
	char path[MAX_OSPATH]; /* Path to PAK / PK3. */
	int i, j; /* Loop counters. */
	int nfiles; /* Number of files in list. */
	fsSearchPath_t *search; /* Search path. */
	fsPack_t *pack; /* PAK / PK3 file. */

	pack = NULL;

	/* Set game directory. */
	Q_strlcpy(fs_gamedir, dir, sizeof(fs_gamedir));

	/* Create directory if it does not exist. */
	FS_CreatePath(fs_gamedir);

	/* Add the directory to the search path. */
	search = Z_Malloc(sizeof(fsSearchPath_t));
	Q_strlcpy(search->path, dir, sizeof(search->path));
	search->next = fs_searchPaths;
	fs_searchPaths = search;

	/* Add numbered pack files in sequence. */
	for (i = 0; i < sizeof(fs_packtypes) / sizeof(fs_packtypes[0]); i++)
	{
		for (j = 0; j < MAX_PAKS; j++)
		{
			Com_sprintf(path, sizeof(path), "%s/pak%d.%s",
					dir, j, fs_packtypes[i].suffix);

			switch (fs_packtypes[i].format)
			{
				case PAK:
					pack = FS_LoadPAK(path);
					break;
#ifdef ZIP
				case PK3:
					pack = FS_LoadPK3(path);
					break;
#endif
			}

			if (pack == NULL)
			{
				continue;
			}

			search = Z_Malloc(sizeof(fsSearchPath_t));
			search->pack = pack;
			search->next = fs_searchPaths;
			fs_searchPaths = search;
		}
	}

	/* Add not numbered pack files. */
	for (i = 0; i < sizeof(fs_packtypes) / sizeof(fs_packtypes[0]); i++)
	{
		Com_sprintf(path, sizeof(path), "%s/*.%s", dir, fs_packtypes[i].suffix);

		if ((list = FS_ListFiles(path, &nfiles, 0, SFF_SUBDIR)) == NULL)
		{
			continue;
		}

		Com_sprintf(path, sizeof(path), "%s/pak*.%s",
				dir, fs_packtypes[i].suffix);

		for (j = 0; j < nfiles - 1; j++)
		{
			/* Skip all packs starting with "pak" */
			if (glob_match(path, list[j]))
			{
				continue;
			}

			switch (fs_packtypes[i].format)
			{
				case PAK:
					pack = FS_LoadPAK(list[j]);
					break;
#ifdef ZIP
				case PK3:
					pack = FS_LoadPK3(list[j]);
					break;
#endif
			}

			if (pack == NULL)
			{
				continue;
			}

			search = Z_Malloc(sizeof(fsSearchPath_t));
			search->pack = pack;
			search->next = fs_searchPaths;
			fs_searchPaths = search;
		}

		FS_FreeList(list, nfiles);
	}
}

/*
 * Use ~/.quake2/dir as fs_gamedir.
 */
void
FS_AddHomeAsGameDirectory(char *dir)
{
	char *home;
	char gdir[MAX_OSPATH];
	size_t len;

	home = Sys_GetHomeDir();

	if (home == NULL)
	{
		return;
	}

	len = snprintf(gdir, sizeof(gdir), "%s%s/", home, dir);
	FS_CreatePath(gdir);

	if ((len > 0) && (len < sizeof(gdir)) && (gdir[len - 1] == '/'))
	{
		gdir[len - 1] = 0;
	}

	Q_strlcpy(fs_gamedir, gdir, sizeof(fs_gamedir));

	FS_AddGameDirectory(gdir);
}

void FS_AddBinaryDirAsGameDirectory(const char* dir)
{
	char gdir[MAX_OSPATH];
	const char *datadir = Sys_GetBinaryDir();
	if(datadir[0] == '\0')
	{
		return;
	}
	int len = snprintf(gdir, sizeof(gdir), "%s%s/", datadir, dir);

	printf("Using binary dir %s to fetch paks\n", gdir);

	if ((len > 0) && (len < sizeof(gdir)) && (gdir[len - 1] == '/'))
	{
		gdir[len - 1] = 0;
	}

	FS_AddGameDirectory(gdir);
}

/*
 * Allows enumerating all of the directories in the search path.
 */
char *
FS_NextPath(char *prevPath)
{
	char *prev;
	fsSearchPath_t *search;

	if (prevPath == NULL)
	{
		return fs_gamedir;
	}

	prev = fs_gamedir;

	for (search = fs_searchPaths; search; search = search->next)
	{
		if (search->pack != NULL)
		{
			continue;
		}

		if (prevPath == prev)
		{
			return search->path;
		}

		prev = search->path;
	}

	return NULL;
}

void
FS_Path_f(void)
{
	int i;
	int totalFiles = 0;
	fsSearchPath_t *search;
	fsHandle_t *handle;
	fsLink_t *link;

	Com_Printf("Current search path:\n");

	for (search = fs_searchPaths; search; search = search->next)
	{
		if (search->pack != NULL)
		{
			Com_Printf("%s (%i files)\n", search->pack->name,
					search->pack->numFiles);
			totalFiles += search->pack->numFiles;
		}
		else
		{
			Com_Printf("%s\n", search->path);
		}
	}

	Com_Printf("\n");

	for (i = 0, handle = fs_handles; i < MAX_HANDLES; i++, handle++)
	{
		if ((handle->file != NULL)
#ifdef ZIP
			 || (handle->zip != NULL)
#endif
			)
		{
			Com_Printf("Handle %i: '%s'.\n", i + 1, handle->name);
		}
	}

	for (i = 0, link = fs_links; link; i++, link = link->next)
	{
		Com_Printf("Link %i: '%s' -> '%s'.\n", i, link->from, link->to);
	}

	Com_Printf("----------------------\n");
#ifdef ZIP
	Com_Printf("%i files in PAK/PK2/PK3/ZIP files.\n", totalFiles);
#else
	Com_Printf("%i files in PAK/PK2 files.\n", totalFiles);
#endif
}

void
FS_ExecAutoexec(void)
{
	char *dir;
	char name[MAX_QPATH];

	dir = (char *)Cvar_VariableString("gamedir");

	if (dir[0] != '\0')
	{
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg",
				fs_basedir->string, dir);
	}
	else
	{
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg",
				fs_basedir->string, BASEDIRNAME);
	}

	if (Sys_FindFirst(name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM) != NULL)
	{
		Cbuf_AddText("exec autoexec.cfg\n");
	}

	Sys_FindClose();
}

/*
 * Sets the gamedir and path to a different directory.
 */
void
FS_SetGamedir(char *dir)
{
	int i;
	fsSearchPath_t *next;

	if (!*dir || !strcmp(dir, ".") || strstr(dir, "..") || strstr(dir, "/"))
	{
		Com_Printf("Gamedir should be a single filename, not a path.\n");
		return;
	}

	/* Free up any current game dir info. */
	while (fs_searchPaths != fs_baseSearchPaths)
	{
		if (fs_searchPaths->pack)
		{
			if (fs_searchPaths->pack->pak)
			{
				fclose(fs_searchPaths->pack->pak);
			}

#ifdef ZIP
			if (fs_searchPaths->pack->pk3)
			{
				unzClose(fs_searchPaths->pack->pk3);
			}
#endif

			Z_Free(fs_searchPaths->pack->files);
			Z_Free(fs_searchPaths->pack);
		}

		next = fs_searchPaths->next;
		Z_Free(fs_searchPaths);
		fs_searchPaths = next;
	}

	/* Close open files for game dir. */
	for (i = 0; i < MAX_HANDLES; i++)
	{
		if (strstr(fs_handles[i].name, dir) &&
			((fs_handles[i].file != NULL)
#ifdef ZIP
			  || (fs_handles[i].zip != NULL)
#endif
			))
		{
			FS_FCloseFile(i);
		}
	}

	/* Flush all data, so it will be forced to reload. */
	if ((dedicated != NULL) && (dedicated->value != 1))
	{
		Cbuf_AddText("vid_restart\nsnd_restart\n");
	}

	Com_sprintf(fs_gamedir, sizeof(fs_gamedir), "%s/%s",
			fs_basedir->string, dir);

	if ((strcmp(dir, BASEDIRNAME) == 0) || (*dir == 0))
	{
		Cvar_FullSet("gamedir", "", CVAR_SERVERINFO | CVAR_NOSET);
		Cvar_FullSet("game", "", CVAR_LATCH | CVAR_SERVERINFO);
	}
	else
	{
		Cvar_FullSet("gamedir", dir, CVAR_SERVERINFO | CVAR_NOSET);

		if (fs_cddir->string[0] == '\0')
		{
			FS_AddGameDirectory(va("%s/%s", fs_cddir->string, dir));
		}

		FS_AddGameDirectory(va("%s/%s", fs_basedir->string, dir));
		FS_AddBinaryDirAsGameDirectory(dir);
		FS_AddHomeAsGameDirectory(dir);
	}
}

/*
 * Creates a filelink_t.
 */
void
FS_Link_f(void)
{
	fsLink_t *l, **prev;

	if (Cmd_Argc() != 3)
	{
		Com_Printf("USAGE: link <from> <to>\n");
		return;
	}

	/* See if the link already exists. */
	prev = &fs_links;

	for (l = fs_links; l != NULL; l = l->next)
	{
		if (strcmp(l->from, Cmd_Argv(1)) == 0)
		{
			Z_Free(l->to);

			if (strlen(Cmd_Argv(2)) == 0)
			{
				/* Delete it. */
				*prev = l->next;
				Z_Free(l->from);
				Z_Free(l);
				return;
			}

			l->to = CopyString(Cmd_Argv(2));
			return;
		}

		prev = &l->next;
	}

	/* Create a new link. */
	l = Z_Malloc(sizeof(*l));
	l->next = fs_links;
	fs_links = l;
	l->from = CopyString(Cmd_Argv(1));
	l->length = strlen(l->from);
	l->to = CopyString(Cmd_Argv(2));
}

/*
 * Create a list of files that match a criteria.
 */
char **
FS_ListFiles(char *findname, int *numfiles,
		unsigned musthave, unsigned canthave)
{
	char **list; /* List of files. */
	char *s; /* Next file in list. */
	int nfiles; /* Number of files in list. */

	/* Initialize variables. */
	list = NULL;
	nfiles = 0;

	/* Count the number of matches. */
	s = Sys_FindFirst(findname, musthave, canthave);

	while (s != NULL)
	{
		if (s[strlen(s) - 1] != '.')
		{
			nfiles++;
		}

		s = Sys_FindNext(musthave, canthave);
	}

	Sys_FindClose();

	/* Check if there are matches. */
	if (nfiles == 0)
	{
		return NULL;
	}

	nfiles++; /* Add space for a guard. */
	*numfiles = nfiles;

	/* Allocate the list. */
	list = calloc(nfiles, sizeof(char *));

	/* Fill the list. */
	s = Sys_FindFirst(findname, musthave, canthave);
	nfiles = 0;

	while (s)
	{
		if (s[strlen(s) - 1] != '.')
		{
			list[nfiles] = strdup(s);
			nfiles++;
		}

		s = Sys_FindNext(musthave, canthave);
	}

	Sys_FindClose();

	return list;
}

/*
 * Compare file attributes (musthave and canthave) in packed files. If
 * "output" is not NULL, "size" is greater than zero and the file matches the
 * attributes then a copy of the matching string will be placed there (with
 * SFF_SUBDIR it changes).
 */
qboolean
ComparePackFiles(const char *findname, const char *name, unsigned musthave,
		unsigned canthave, char *output, int size)
{
	qboolean retval;
	char *ptr;
	char buffer[MAX_OSPATH];

	Q_strlcpy(buffer, name, sizeof(buffer));

	if ((canthave & SFF_SUBDIR) && (name[strlen(name) - 1] == '/'))
	{
		return false;
	}

	if (musthave & SFF_SUBDIR)
	{
		if ((ptr = strrchr(buffer, '/')) != NULL)
		{
			*ptr = '\0';
		}
		else
		{
			return false;
		}
	}

	if ((musthave & SFF_HIDDEN) || (canthave & SFF_HIDDEN))
	{
		if ((ptr = strrchr(buffer, '/')) == NULL)
		{
			ptr = buffer;
		}

		if (((musthave & SFF_HIDDEN) && (ptr[1] != '.')) ||
			((canthave & SFF_HIDDEN) && (ptr[1] == '.')))
		{
			return false;
		}
	}

	if (canthave & SFF_RDONLY)
	{
		return false;
	}

	retval = glob_match((char *)findname, buffer);

	if (retval && (output != NULL))
	{
		Q_strlcpy(output, buffer, size);
	}

	return retval;
}

/*
 * Create a list of files that match a criteria.
 * Searchs are relative to the game directory and use all the search paths
 * including .pak and .pk3 files.
 */
char **
FS_ListFiles2(char *findname, int *numfiles,
		unsigned musthave, unsigned canthave)
{
	fsSearchPath_t *search; /* Search path. */
	int i, j; /* Loop counters. */
	int nfiles; /* Number of files found. */
	int tmpnfiles; /* Temp number of files. */
	char **tmplist; /* Temporary list of files. */
	char **list; /* List of files found. */
	char path[MAX_OSPATH]; /* Temporary path. */

	nfiles = 0;
	list = malloc(sizeof(char *));

	for (search = fs_searchPaths; search != NULL; search = search->next)
	{
		if (search->pack != NULL)
		{
			if (canthave & SFF_INPACK)
			{
				continue;
			}

			for (i = 0, j = 0; i < search->pack->numFiles; i++)
			{
				if (ComparePackFiles(findname, search->pack->files[i].name,
							musthave, canthave, NULL, 0))
				{
					j++;
				}
			}

			if (j == 0)
			{
				continue;
			}

			nfiles += j;
			list = realloc(list, nfiles * sizeof(char *));

			for (i = 0, j = nfiles - j; i < search->pack->numFiles; i++)
			{
				if (ComparePackFiles(findname, search->pack->files[i].name,
							musthave, canthave, path, sizeof(path)))
				{
					list[j++] = strdup(path);
				}
			}
		}

		if (musthave & SFF_INPACK)
		{
			continue;
		}

		Com_sprintf(path, sizeof(path), "%s/%s", search->path, findname);
		tmplist = FS_ListFiles(path, &tmpnfiles, musthave, canthave);

		if (tmplist != NULL)
		{
			tmpnfiles--;
			nfiles += tmpnfiles;
			list = realloc(list, nfiles * sizeof(char *));

			for (i = 0, j = nfiles - tmpnfiles; i < tmpnfiles; i++, j++)
			{
				list[j] = strdup(tmplist[i] + strlen(search->path) + 1);
			}

			FS_FreeList(tmplist, tmpnfiles + 1);
		}
	}

	/* Delete duplicates. */
	tmpnfiles = 0;

	for (i = 0; i < nfiles; i++)
	{
		if (list[i] == NULL)
		{
			continue;
		}

		for (j = i + 1; j < nfiles; j++)
		{
			if ((list[j] != NULL) &&
				(strcmp(list[i], list[j]) == 0))
			{
				free(list[j]);
				list[j] = NULL;
				tmpnfiles++;
			}
		}
	}

	if (tmpnfiles > 0)
	{
		nfiles -= tmpnfiles;
		tmplist = malloc(nfiles * sizeof(char *));

		for (i = 0, j = 0; i < nfiles + tmpnfiles; i++)
		{
			if (list[i] != NULL)
			{
				tmplist[j++] = list[i];
			}
		}

		free(list);
		list = tmplist;
	}

	/* Add a guard. */
	if (nfiles > 0)
	{
		nfiles++;
		list = realloc(list, nfiles * sizeof(char *));
		list[nfiles - 1] = NULL;
	}

	else
	{
		free(list);
		list = NULL;
	}

	*numfiles = nfiles;

	return list;
}

/*
 * Free list of files created by FS_ListFiles().
 */
void
FS_FreeList(char **list, int nfiles)
{
	int i;

	for (i = 0; i < nfiles - 1; i++)
	{
		free(list[i]);
	}

	free(list);
}

/*
 * Directory listing.
 */
void
FS_Dir_f(void)
{
	char **dirnames; /* File list. */
	char findname[1024]; /* File search path and pattern. */
	char *path = NULL; /* Search path. */
	char wildcard[1024] = "*.*"; /* File pattern. */
	int i; /* Loop counter. */
	int ndirs; /* Number of files in list. */

	/* Check for pattern in arguments. */
	if (Cmd_Argc() != 1)
	{
		Q_strlcpy(wildcard, Cmd_Argv(1), sizeof(wildcard));
	}

	/* Scan search paths and list files. */
	while ((path = FS_NextPath(path)) != NULL)
	{
		Com_sprintf(findname, sizeof(findname), "%s/%s", path, wildcard);
		Com_Printf("Directory of '%s'.\n", findname);
		Com_Printf("----\n");

		if ((dirnames = FS_ListFiles(findname, &ndirs, 0, 0)) != 0)
		{
			for (i = 0; i < ndirs - 1; i++)
			{
				if (strrchr(dirnames[i], '/'))
				{
					Com_Printf("%s\n", strrchr(dirnames[i], '/') + 1);
				}
				else
				{
					Com_Printf("%s\n", dirnames[i]);
				}
			}

			FS_FreeList(dirnames, ndirs);
		}

		Com_Printf("\n");
	}
}

void
FS_InitFilesystem(void)
{
	/* Register FS commands. */
	Cmd_AddCommand("path", FS_Path_f);
	Cmd_AddCommand("link", FS_Link_f);
	Cmd_AddCommand("dir", FS_Dir_f);

	/* basedir <path> Allows the game to run from outside the data tree.  */
	fs_basedir = Cvar_Get("basedir",
#ifdef SYSTEMWIDE
		SYSTEMDIR,
#else
		".",
#endif
		CVAR_NOSET);

	/* cddir <path> Logically concatenates the cddir after the basedir to
	   allow the game to run from outside the data tree. */
	fs_cddir = Cvar_Get("cddir", "", CVAR_NOSET);

	if (fs_cddir->string[0] != '\0')
	{
		FS_AddGameDirectory(va("%s/" BASEDIRNAME, fs_cddir->string));
	}

	/* Debug flag. */
	fs_debug = Cvar_Get("fs_debug", "0", 0);

	/* Game directory. */
	fs_gamedirvar = Cvar_Get("game", "", CVAR_LATCH | CVAR_SERVERINFO);

	/* Current directory. */
	fs_homepath = Cvar_Get("homepath", Sys_GetCurrentDirectory(), CVAR_NOSET);

	/* Add baseq2 to search path. */
	FS_AddGameDirectory(va("%s/" BASEDIRNAME, fs_basedir->string));
	FS_AddBinaryDirAsGameDirectory(BASEDIRNAME);
	FS_AddHomeAsGameDirectory(BASEDIRNAME);

	/* Any set gamedirs will be freed up to here. */
	fs_baseSearchPaths = fs_searchPaths;
	Q_strlcpy(fs_currentGame, BASEDIRNAME, sizeof(fs_currentGame));

	/* Check for game override. */
	if (fs_gamedirvar->string[0] != '\0')
	{
		FS_SetGamedir(fs_gamedirvar->string);
	}

	/* Create directory if it does not exist. */
	FS_CreatePath(fs_gamedir);

	Com_Printf("Using '%s' for writing.\n", fs_gamedir);
}

