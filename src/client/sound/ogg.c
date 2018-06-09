/*
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
 * This file implements an interface to libvorbis for decoding
 * OGG/Vorbis files. Strongly spoken this file isn't part of the sound
 * system but part of the main client. It justs converts Vorbis streams
 * into normal, raw Wave stream which are injected into the backends as 
 * if they were normal "raw" samples. At this moment only background
 * music playback and in theory .cin movie file playback is supported.
 *
 * =======================================================================
 */

#ifdef OGG

#define OV_EXCLUDE_STATIC_CALLBACKS

#ifndef _WIN32
#include <sys/time.h>
#endif
#include <errno.h>
#include <vorbis/vorbisfile.h>

#include "../header/client.h"
#include "header/local.h"
#include "header/vorbis.h"

qboolean ogg_first_init = true; /* First initialization flag. */
qboolean ogg_started = false;   /* Initialization flag. */
int ogg_bigendian = 0;
byte *ogg_buffer;				/* File buffer. */
char **ogg_filelist;			/* List of Ogg Vorbis files. */
char ovBuf[4096];               /* Buffer for sound. */
int ogg_curfile;				/* Index of currently played file. */
int ogg_numfiles;				/* Number of Ogg Vorbis files. */
int ovSection;					/* Position in Ogg Vorbis file. */
ogg_status_t ogg_status;		/* Status indicator. */
cvar_t *ogg_check;				/* Check Ogg files or not. */
cvar_t *ogg_sequence;			/* Sequence play indicator. */
cvar_t *ogg_volume;				/* Music volume. */
cvar_t *ogg_ignoretrack0;		/* Toggle track 0 playing */
OggVorbis_File ovFile;			/* Ogg Vorbis file. */
vorbis_info *ogg_info;			/* Ogg Vorbis file information */
int ogg_numbufs;				/* Number of buffers for OpenAL */

/*
 * Initialize the Ogg Vorbis subsystem.
 */
void
OGG_Init(void)
{
	cvar_t *cv; /* Cvar pointer. */

	if (ogg_started)
	{
		return;
	}

	Com_Printf("Starting Ogg Vorbis.\n");

	/* Skip initialization if disabled. */
	cv = Cvar_Get("ogg_enable", "1", CVAR_ARCHIVE);

	if (cv->value != 1)
	{
		Com_Printf("Ogg Vorbis not initializing.\n");
		return;
	}

	if (bigendien == true)
	{
		ogg_bigendian = 1;
	}

	/* Cvars. */
	ogg_check = Cvar_Get("ogg_check", "0", CVAR_ARCHIVE);
	ogg_sequence = Cvar_Get("ogg_sequence", "loop", CVAR_ARCHIVE);
	ogg_volume = Cvar_Get("ogg_volume", "0.7", CVAR_ARCHIVE);
	ogg_ignoretrack0 = Cvar_Get("ogg_ignoretrack0", "0", CVAR_ARCHIVE);

	/* Console commands. */
	Cmd_AddCommand("ogg_list", OGG_ListCmd);
	Cmd_AddCommand("ogg_pause", OGG_PauseCmd);
	Cmd_AddCommand("ogg_play", OGG_PlayCmd);
	Cmd_AddCommand("ogg_resume", OGG_ResumeCmd);
	Cmd_AddCommand("ogg_status", OGG_StatusCmd);
	Cmd_AddCommand("ogg_stop", OGG_Stop);

	/* Build list of files. */
	ogg_numfiles = 0;

	OGG_LoadFileList(); // FIXME: call whenever $game changes

	/* Check if we have Ogg Vorbis files. */
	if (ogg_numfiles <= 0)
	{
		Com_Printf("No Ogg Vorbis files found.\n");
		ogg_started = true; /* For OGG_Shutdown(). */
		OGG_Shutdown();
		return;
	}

	/* Initialize variables. */
	if (ogg_first_init)
	{
		ogg_buffer = NULL;
		ogg_curfile = -1;
		ogg_info = NULL;
		ogg_status = STOP;
		ogg_first_init = false;
	}

	ogg_started = true;

	Com_Printf("%d Ogg Vorbis files found.\n", ogg_numfiles);
}

/*
 * Shutdown the Ogg Vorbis subsystem.
 */
void
OGG_Shutdown(void)
{
	if (!ogg_started)
	{
		return;
	}

	Com_Printf("Shutting down Ogg Vorbis.\n");

	OGG_Stop();

	/* Free the list of files. */
	FS_FreeList(ogg_filelist, ogg_numfiles + 1);

	/* Remove console commands. */
	Cmd_RemoveCommand("ogg_list");
	Cmd_RemoveCommand("ogg_pause");
	Cmd_RemoveCommand("ogg_play");
	Cmd_RemoveCommand("ogg_resume");
	Cmd_RemoveCommand("ogg_status");
	Cmd_RemoveCommand("ogg_stop");

	ogg_started = false;
}

/*
 * Check if the file is a valid Ogg Vorbis file.
 */
qboolean
OGG_Check(char *name)
{
	qboolean res;        /* Return value. */
	byte *buffer;        /* File buffer. */
	int size;            /* File size. */
	OggVorbis_File ovf;  /* Ogg Vorbis file. */

	if (ogg_check->value == 0)
	{
		return true;
	}

	res = false;

	if ((size = FS_LoadFile(name, (void **)&buffer)) > 0)
	{
		if (ov_test(NULL, &ovf, (char *)buffer, size) == 0)
		{
			res = true;
			ov_clear(&ovf);
		}

		FS_FreeFile(buffer);
	}

	return res;
}

/*
 * Load list of Ogg Vorbis files in "music".
 */
void
OGG_LoadFileList(void) // FIXME: call whenever $game changes, rename: OGG_InitTrackList() or sth
{
	char **list; /* List of .ogg files. */
	int i;		 /* Loop counter. */
	int j;		 /* Real position in list. */

	/* Get file list. */
	list = FS_ListFiles2(va("%s/*.ogg", OGG_DIR),
		   	&ogg_numfiles, 0, 0);
	ogg_numfiles--;

	/* Check if there are posible Ogg files. */
	if (list == NULL)
	{
		return;
	}

	/* Allocate list of files. */
	ogg_filelist = malloc(sizeof(char *) * ogg_numfiles);

	/* Add valid Ogg Vorbis file to the list. */
	for (i = 0, j = 0; i < ogg_numfiles; i++)
	{
		if (!OGG_Check(list[i]))
		{
			free(list[i]);
			continue;
		}

		ogg_filelist[j++] = list[i];
	}

	/* Free the file list. */
	free(list);

	/* Adjust the list size (remove 
	   space for invalid music files). */
	ogg_numfiles = j;
	ogg_filelist = realloc(ogg_filelist, sizeof(char *) * ogg_numfiles);
}

/*
 * Play Ogg Vorbis file (with absolute or relative index).
 */
qboolean
OGG_Open(ogg_seek_t type, int offset)
{
	int size;     /* File size. */
	int pos = -1; /* Absolute position. */
	int res;      /* Error indicator. */

	switch (type)
	{
		case ABS:

			/* Absolute index. */
			if ((offset < 0) || (offset >= ogg_numfiles))
			{
				Com_Printf("OGG_Open: %d out of range.\n", offset + 1);
				return false;
			}
			else
			{
				pos = offset;
			}

			break;
		case REL:

			/* Simulate a loopback. */
			if ((ogg_curfile == -1) && (offset < 0))
			{
				offset++;
			}

			while (ogg_curfile + offset < 0)
			{
				offset += ogg_numfiles;
			}

			while (ogg_curfile + offset >= ogg_numfiles)
			{
				offset -= ogg_numfiles;
			}

			pos = ogg_curfile + offset;
			break;
	}

	/* Check running music. */
	if (ogg_status == PLAY)
	{
		if (ogg_curfile == pos)
		{
			return true;
		}

		else
		{
			OGG_Stop();
		}
	}

	/* Find file. */
	if ((size = FS_LoadFile(ogg_filelist[pos], (void **)&ogg_buffer)) == -1)
	{
		Com_Printf("OGG_Open: could not open %d (%s): %s.\n",
				pos, ogg_filelist[pos], strerror(errno));
		return false;
	}

	/* Open ogg vorbis file. */
	if ((res = ov_open(NULL, &ovFile, (char *)ogg_buffer, size)) < 0)
	{
		Com_Printf("OGG_Open: '%s' is not a valid Ogg Vorbis file (error %i).\n",
				ogg_filelist[pos], res); FS_FreeFile(ogg_buffer);
		ogg_buffer = NULL;
		return false;
	}

	ogg_info = ov_info(&ovFile, 0);

	if (!ogg_info)
	{
		Com_Printf("OGG_Open: Unable to get stream information for %s.\n",
				ogg_filelist[pos]);
		ov_clear(&ovFile);
		FS_FreeFile(ogg_buffer);
		ogg_buffer = NULL;
		return false;
	}

	/* Play file. */
	ovSection = 0;
	ogg_curfile = pos;
	ogg_status = PLAY;

	return true;
}

/*
 * Play Ogg Vorbis file (with name only).
 */
qboolean
OGG_OpenName(char *filename)
{
	char *name; /* File name. */
	int i;		/* Loop counter. */

	/* If the track name is '00', and ogg_ignoretrack0 is set to 0, stop playback */
	if ((!strncmp(filename, "00", sizeof(char) * 3)) && ogg_ignoretrack0->value == 0)
	{
		OGG_PauseCmd();
		return false;
	}

	name = va("%s/%s.ogg", OGG_DIR, filename);

	for (i = 0; i < ogg_numfiles; i++)
	{
		if (strcmp(name, ogg_filelist[i]) == 0)
		{
			break;
		}
	}

	if (i < ogg_numfiles)
	{
		return OGG_Open(ABS, i);
	}

	else
	{
		Com_Printf("OGG_OpenName: '%s' not in the list.\n", filename);
		return false;
	}
}

/*
 * Play a portion of the currently opened file.
 */
int
OGG_Read(void)
{
	int res; /* Number of bytes read. */

	/* Read and resample. */
	res = ov_read(&ovFile, ovBuf, sizeof(ovBuf),
			ogg_bigendian, OGG_SAMPLEWIDTH, 1,
			&ovSection);
	S_RawSamples(res / (OGG_SAMPLEWIDTH * ogg_info->channels),
			ogg_info->rate, OGG_SAMPLEWIDTH, ogg_info->channels,
			(byte *)ovBuf, ogg_volume->value);

	/* Check for end of file. */
	if (res == 0)
	{
		OGG_Stop();
		//OGG_Sequence();
		OGG_Open(REL, 0);
	}

	return res;
}

/*
 * Stop playing the current file.
 */
void
OGG_Stop(void)
{
	if (ogg_status == STOP)
	{
		return;
	}

#ifdef USE_OPENAL
	if (sound_started == SS_OAL)
	{
		AL_UnqueueRawSamples();
	}
#endif

	ov_clear(&ovFile);
	ogg_status = STOP;
	ogg_info = NULL;
	ogg_numbufs = 0;

	if (ogg_buffer != NULL)
	{
		FS_FreeFile(ogg_buffer);
		ogg_buffer = NULL;
	}
}

/*
 * Stream music.
 */
void
OGG_Stream(void)
{
	if (!ogg_started)
	{
		return;
	}

	if (ogg_status == PLAY)
	{
#ifdef USE_OPENAL
		if (sound_started == SS_OAL)
		{
			/* Calculate the number of buffers used
			   for storing decoded OGG/Vorbis data.
			   We take the number of active buffers
			   and add 256. 256 are about 12 seconds
			   worth of sound, more than enough to
			   be resilent against underruns. */
			if (ogg_numbufs == 0 || active_buffers < ogg_numbufs - 256)
			{
				ogg_numbufs = active_buffers + 256;
			}

			/* active_buffers are all active OpenAL buffers,
			   buffering normal sfx _and_ ogg/vorbis samples. */
			while (active_buffers <= ogg_numbufs)
			{
				OGG_Read();
			}
		}
		else /* using SDL */
#endif
		{
			if (sound_started == SS_SDL)
			{
				/* Read that number samples into the buffer, that
				   were played since the last call to this function.
				   This keeps the buffer at all times at an "optimal"
				   fill level. */
				while (paintedtime + MAX_RAW_SAMPLES - 2048 > s_rawend)
				{
					OGG_Read();
				}
			}
		} /* using SDL */
	} /* ogg_status == PLAY */
}

/*
 * List Ogg Vorbis files.
 */
void
OGG_ListCmd(void)
{
	int i;

	for (i = 0; i < ogg_numfiles; i++)
	{
		Com_Printf("%d %s\n", i + 1, ogg_filelist[i]);
	}

	Com_Printf("%d Ogg Vorbis files.\n", ogg_numfiles);
}

// TODO: document
void
OGG_PlayTrack(int track)
{
	if(track == 0)
	{
		if(ogg_ignoretrack0->value == 0)
		{
			OGG_PauseCmd();
		}
	}
	else if(track == -1) // randon track
	{
		OGG_Open(ABS, randk() % ogg_numfiles);
	}
	else
	{
		// TODO: support other naming schemes etc
		char name[3] = { '0' + track / 10, '0' + track % 10, '\0' };
		OGG_OpenName(name);
	}
}

/*
 * Pause current song.
 */
void
OGG_PauseCmd(void)
{
	if (ogg_status == PLAY)
	{
		ogg_status = PAUSE;
		ogg_numbufs = 0;
	}

#ifdef USE_OPENAL
	if (sound_started == SS_OAL)
	{
		AL_UnqueueRawSamples();
	}
#endif
}

/*
 * Play control.
 */
void
OGG_PlayCmd(void)
{
	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: ogg_play {filename | #n | ? | >n | <n}\n");
		return;
	}

	//OGG_ParseCmd(Cmd_Argv(1));
	// TODO: selber parsen, nur noch track nummer oder garnix anbieten
}

/*
 * Resume current song.
 */
void
OGG_ResumeCmd(void) // TODO: OGG_TogglePlay?
{
	if (ogg_status == PAUSE)
	{
		ogg_status = PLAY;
	}
}

/*
 * Display status.
 */
void
OGG_StatusCmd(void)
{
	switch (ogg_status)
	{
		case PLAY:
			Com_Printf("Playing file %d (%s) at %0.2f seconds.\n",
				ogg_curfile + 1, ogg_filelist[ogg_curfile],
				ov_time_tell(&ovFile));
			break;

		case PAUSE:
			Com_Printf("Paused file %d (%s) at %0.2f seconds.\n",
				ogg_curfile + 1, ogg_filelist[ogg_curfile],
				ov_time_tell(&ovFile));
			break;

		case STOP:
			if (ogg_curfile == -1)
			{
				Com_Printf("Stopped.\n");
			}
			else
			{
				Com_Printf("Stopped file %d (%s).\n",
						ogg_curfile + 1, ogg_filelist[ogg_curfile]);
			}

			break;
	}
}

#endif  /* OGG */

