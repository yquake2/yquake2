/*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
* 
* See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/*
==========================================================

  OGG Vorbis decoding

==========================================================
*/

#include <sys/time.h>
#include <errno.h>
#include <vorbis/vorbisfile.h>

#include "../header/client.h"
#include "snd_loc.h"
#include "snd_ogg.h"

extern int	sound_started;		/* Sound initialization flag. */
extern cvar_t	*fs_basedir;		/* Path to "music". */

qboolean	ogg_first_init = true;	/* First initialization flag. */
qboolean	ogg_started = false;	/* Initialization flag. */
byte		*ogg_buffer;		/* File buffer. */
char		**ogg_filelist;		/* List of Ogg Vorbis files. */
char		ovBuf[4096];		/* Buffer for sound. */
int		ogg_curfile;		/* Index of currently played file. */
int		ogg_numfiles;		/* Number of Ogg Vorbis files. */
int		ovSection;		/* Position in Ogg Vorbis file. */
ogg_status_t	ogg_status;		/* Status indicator. */
cvar_t		*ogg_autoplay;		/* Play this song when started. */
cvar_t		*ogg_check;		/* Check Ogg files or not. */
cvar_t		*ogg_playlist;		/* Playlist. */
cvar_t		*ogg_sequence;		/* Sequence play indicator. */
cvar_t		*ogg_volume;		/* Music volume. */
OggVorbis_File	ovFile;			/* Ogg Vorbis file. */

/*
==========
OGG_Init

Initialize the Ogg Vorbis subsystem.
==========
*/
void OGG_Init(void)
{
	cvar_t	*cv;	/* Cvar pointer. */

	if (ogg_started)
		return;

	Com_Printf("Starting Ogg Vorbis.\n");

	/* Skip initialization if disabled. */
	cv = Cvar_Get ("ogg_enable", "0", CVAR_ARCHIVE);
	if (cv->value != 1) {
		Com_Printf ("Ogg Vorbis not initializing.\n");
		return;
	}

	/* Cvars. */
	ogg_autoplay = Cvar_Get("ogg_autoplay", "?", CVAR_ARCHIVE);
	ogg_check = Cvar_Get("ogg_check", "0", CVAR_ARCHIVE);
	ogg_playlist = Cvar_Get("ogg_playlist", "playlist", CVAR_ARCHIVE);
	ogg_sequence = Cvar_Get("ogg_sequence", "loop", CVAR_ARCHIVE);
	ogg_volume = Cvar_Get("ogg_volume", "0.7", CVAR_ARCHIVE);

	/* Console commands. */
	Cmd_AddCommand("ogg_list", OGG_ListCmd);
	Cmd_AddCommand("ogg_pause", OGG_PauseCmd);
	Cmd_AddCommand("ogg_play", OGG_PlayCmd);
	Cmd_AddCommand("ogg_reinit", OGG_Reinit);
	Cmd_AddCommand("ogg_resume", OGG_ResumeCmd);
	Cmd_AddCommand("ogg_seek", OGG_SeekCmd);
	Cmd_AddCommand("ogg_status", OGG_StatusCmd);
	Cmd_AddCommand("ogg_stop", OGG_Stop);

	/* Build list of files. */
	ogg_numfiles = 0;
	if (ogg_playlist->string[0] != '\0')
		OGG_LoadPlaylist(ogg_playlist->string);
	if (ogg_numfiles == 0)
		OGG_LoadFileList();

	/* Check if we have Ogg Vorbis files. */
	if (ogg_numfiles <= 0) {
		Com_Printf("No Ogg Vorbis files found.\n");
		ogg_started = true;	/* For OGG_Shutdown(). */
		OGG_Shutdown();
		return;
	}

	/* Initialize variables. */
	if (ogg_first_init) {
		srand(time(NULL));
		ogg_buffer = NULL;
		ogg_curfile = -1;
		ogg_status = STOP;
		ogg_first_init = false;
	}

	ogg_started = true;

	Com_Printf("%d Ogg Vorbis files found.\n", ogg_numfiles);

	/* Autoplay support. */
	if (ogg_autoplay->string[0] != '\0')
		OGG_ParseCmd(ogg_autoplay->string);
}

/*
==========
OGG_Shutdown

Shutdown the Ogg Vorbis subsystem.
==========
*/
void OGG_Shutdown(void)
{

	if (!ogg_started)
		return;

	Com_Printf("Shutting down Ogg Vorbis.\n");

	OGG_Stop();

	/* Free the list of files. */
	FS_FreeList(ogg_filelist, ogg_numfiles + 1);

	/* Remove console commands. */
	Cmd_RemoveCommand("ogg_list");
	Cmd_RemoveCommand("ogg_pause");
	Cmd_RemoveCommand("ogg_play");
	Cmd_RemoveCommand("ogg_reinit");
	Cmd_RemoveCommand("ogg_resume");
	Cmd_RemoveCommand("ogg_seek");
	Cmd_RemoveCommand("ogg_status");
	Cmd_RemoveCommand("ogg_stop");

	ogg_started = false;
}

/*
==========
OGG_Reinit

Reinitialize the Ogg Vorbis subsystem.
==========
*/
void OGG_Reinit(void)
{

	OGG_Shutdown();
	OGG_Init();
}

/*
==========
OGG_Check

Check if the file is a valid Ogg Vorbis file.
==========
*/
qboolean OGG_Check(char *name)
{
	qboolean	 res;		/* Return value. */
	byte		*buffer;	/* File buffer. */
	int		 size;		/* File size. */
	OggVorbis_File	 ovf;		/* Ogg Vorbis file. */

	if (ogg_check->value == 0)
		return (true);

	res = false;
	if ((size = FS_LoadFile(name, (void**)&buffer)) > 0) {
		if (ov_test(NULL, &ovf, (char *)buffer, size) == 0) {
			res = true;
			ov_clear(&ovf);
		}
		FS_FreeFile(buffer);
	}

	return (res);
}

/*
==========
OGG_Seek

Change position in the file.
==========
*/
void OGG_Seek(ogg_seek_t type, double offset)
{
	double pos;	/* Position in file (in seconds). */
	double total;	/* Length of file (in seconds). */

	/* Check if the file is seekable. */
	if (ov_seekable(&ovFile) == 0) {
		Com_Printf("OGG_Seek: file is not seekable.\n");
		return;
	}

	/* Get file information. */
	pos = ov_time_tell(&ovFile);
	total = ov_time_total(&ovFile, -1);

	switch (type) {
	case ABS:
		if (offset >= 0 && offset <= total) {
			if (ov_time_seek(&ovFile, offset) != 0)
				Com_Printf("OGG_Seek: could not seek.\n");
			else
				Com_Printf("%0.2f -> %0.2f of %0.2f.\n", pos, offset, total);
		} else
			Com_Printf("OGG_Seek: invalid offset.\n");
		break;
	case REL:
		if (pos + offset >= 0 && pos + offset <= total) {
			if (ov_time_seek(&ovFile, pos + offset) != 0)
				Com_Printf("OGG_Seek: could not seek.\n");
			else
				Com_Printf("%0.2f -> %0.2f of %0.2f.\n", pos, pos + offset, total);
		} else
			Com_Printf("OGG_Seek: invalid offset.\n");
		break;
	}
}

/*
==========
OGG_LoadFileList

Load list of Ogg Vorbis files in "music".
==========
*/
void OGG_LoadFileList(void)
{
	char	**list;			/* List of .ogg files. */
	int	  i;			/* Loop counter. */
	int	  j;			/* Real position in list. */

	/* Get file list. */
	list = FS_ListFiles2(va("%s/*.ogg", OGG_DIR), &ogg_numfiles, 0,
	    SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);
	ogg_numfiles--;

	/* Check if there are posible Ogg files. */
	if (list == NULL)
		return;

	/* Allocate list of files. */
	ogg_filelist = malloc(sizeof(char *) * ogg_numfiles);

	/* Add valid Ogg Vorbis file to the list. */
	for (i = 0, j = 0; i < ogg_numfiles; i++) {
		if (!OGG_Check(list[i])) {
			free(list[i]);
			continue;
		}
		ogg_filelist[j++] = list[i];
	}

	/* Free the file list. */
	free(list);

	/* Adjust the list size (remove space for invalid music files). */
	ogg_numfiles = j;
	ogg_filelist = realloc(ogg_filelist, sizeof(char *) * ogg_numfiles);
}

/*
==========
OGG_LoadPlaylist

Load playlist.
==========
*/
void OGG_LoadPlaylist(char *playlist)
{
	byte	*buffer;	/* Buffer to read the file. */
	char	*ptr;		/* Pointer for parsing the file. */
	int	 i;		/* Loop counter. */
	int	 size;		/* Length of buffer and strings. */

	/* Open playlist. */
	if ((size = FS_LoadFile(va("%s/%s.lst", OGG_DIR, ogg_playlist->string),
	    (void **)&buffer)) < 0) {
		Com_Printf("OGG_LoadPlaylist: could not open playlist: %s.\n", strerror(errno));
		return;
	}

	/* Count the files in playlist. */
	for (ptr = strtok((char *)buffer, "\n");
	    ptr != NULL;
	    ptr = strtok(NULL, "\n")) {
		if ((byte *)ptr != buffer)
			ptr[-1] = '\n';
		if (OGG_Check(va("%s/%s", OGG_DIR, ptr)))
			ogg_numfiles++;
	}

	/* Allocate file list. */
	ogg_filelist = malloc(sizeof(char *) * ogg_numfiles);

	i = 0;
	for (ptr = strtok((char *)buffer, "\n");
	    ptr != NULL;
	    ptr = strtok(NULL, "\n"))
		if (OGG_Check(va("%s/%s", OGG_DIR, ptr)))
			ogg_filelist[i++] = strdup(va("%s/%s", OGG_DIR, ptr));

	/* Free file buffer. */
	FS_FreeFile(buffer);
}

/*
==========
OGG_Open

Play Ogg Vorbis file (with absolute or relative index).
==========
*/
qboolean OGG_Open(ogg_seek_t type, int offset)
{
	int		 size;		/* File size. */
	int		 pos;		/* Absolute position. */
	int		 res;		/* Error indicator. */

	pos = -1;

	switch (type) {
	case ABS:
		/* Absolute index. */
		if (offset < 0 || offset >= ogg_numfiles) {
			Com_Printf("OGG_Open: %d out of range.\n", offset+1);
			return (false);
		} else
			pos = offset;
		break;
	case REL:
		/* Simulate a loopback. */
		if (ogg_curfile == -1 && offset < 0)
			offset++;
		while (ogg_curfile + offset < 0)
			offset += ogg_numfiles;
		while (ogg_curfile + offset >= ogg_numfiles)
			offset -= ogg_numfiles;
		pos = ogg_curfile + offset;
		break;
	}

	/* Check running music. */
	if (ogg_status == PLAY) {
		if (ogg_curfile == pos)
			return (true);
		else
			OGG_Stop();
	}

	/* Find file. */
	if ((size = FS_LoadFile(ogg_filelist[pos], (void **)&ogg_buffer)) == -1) {
		Com_Printf("OGG_Open: could not open %d (%s): %s.\n", pos, ogg_filelist[pos], strerror(errno));
		return (false);
	}

	/* Open ogg vorbis file. */
	if ((res = ov_open(NULL, &ovFile, (char *)ogg_buffer, size)) < 0) {
		Com_Printf("OGG_Open: '%s' is not a valid Ogg Vorbis file (error %i).\n", ogg_filelist[pos], res);
		FS_FreeFile(ogg_buffer);
		return (false);
	}

	/* Play file. */
	ovSection = 0;
	ogg_curfile = pos;
	ogg_status = PLAY;

	return (true);
}

/*
==========
OGG_OpenName

Play Ogg Vorbis file (with name only).
==========
*/
qboolean OGG_OpenName(char *filename)
{
	char	*name;	/* File name. */
	int	 i;	/* Loop counter. */

	name = va("%s/%s.ogg", OGG_DIR, filename);

	for (i = 0; i < ogg_numfiles; i++)
		if (strcmp(name, ogg_filelist[i]) == 0)
			break;

	if (i < ogg_numfiles)
		return (OGG_Open(ABS, i));
	else {
		Com_Printf("OGG_OpenName: '%s' not in the list.\n", filename);
		return (false);
	}
}

/*
==========
OGG_Read

Play a portion of the currently opened file.
==========
*/
int OGG_Read(void)
{
	int res;	/* Number of bytes read. */

	/* Read and resample. */
	res = ov_read(&ovFile, ovBuf, sizeof(ovBuf), 0, 2, 1, &ovSection);
	S_RawSamplesVol(res>>2, 44100, 2, 2, (byte*)ovBuf, ogg_volume->value);

	/* Check for end of file. */
	if (res == 0) {
		OGG_Stop();
		OGG_Sequence();
	}

	return (res);
}

/*
==========
OGG_Sequence

Play files in sequence.
==========
*/
void OGG_Sequence(void)
{

	if (strcmp(ogg_sequence->string, "next") == 0)
		OGG_Open(REL, 1);
	else if (strcmp(ogg_sequence->string, "prev") == 0)
		OGG_Open(REL, -1);
	else if (strcmp(ogg_sequence->string, "random") == 0)
		OGG_Open(ABS, rand() % ogg_numfiles);
	else if (strcmp(ogg_sequence->string, "loop") == 0)
		OGG_Open(REL, 0);
	else if (strcmp(ogg_sequence->string, "none") != 0) {
		Com_Printf("Invalid value of ogg_sequence: %s\n", ogg_sequence->string);
		Cvar_Set("ogg_sequence", "none");
	}
}

/*
==========
OGG_Stop

Stop playing the current file.
==========
*/
void OGG_Stop(void)
{

	if (ogg_status == STOP)
		return;

	ov_clear(&ovFile);
	ogg_status = STOP;

	if (ogg_buffer != NULL) {
		FS_FreeFile(ogg_buffer);
		ogg_buffer = NULL;
	}
}

/*
==========
OGG_Stream

Stream music.
==========
*/
void OGG_Stream(void)
{

	if (!ogg_started)
		return;

	while (ogg_status == PLAY && paintedtime + MAX_RAW_SAMPLES - 2048 > s_rawend)
		OGG_Read();
}

/*
============
S_RawSamplesVol

Cinematic streaming and voice over network (with volume)
============
*/
void S_RawSamplesVol (int samples, int rate, int width, int channels, byte *data, float volume)
{
	int	i;
	int	src, dst;
	float	scale;

	if (!sound_started)
		return;

	if (s_rawend < paintedtime)
		s_rawend = paintedtime;

	scale = (float)rate / dma.speed;

//Com_Printf ("%i < %i < %i\n", soundtime, paintedtime, s_rawend);
	if (channels == 2 && width == 2)
	{
		if (scale == 1.0)
		{	// optimized case
			for (i=0 ; i<samples ; i++)
			{
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left =
				    (int)(volume * LittleShort(((short *)data)[i*2])) << 8;
				s_rawsamples[dst].right =
				    (int)(volume * LittleShort(((short *)data)[i*2+1])) << 8;
			}
		}
		else
		{
			for (i=0 ; ; i++)
			{
				src = i*scale;
				if (src >= samples)
					break;
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left =
				    (int)(volume * LittleShort(((short *)data)[src*2])) << 8;
				s_rawsamples[dst].right =
				    (int)(volume * LittleShort(((short *)data)[src*2+1])) << 8;
			}
		}
	}
	else if (channels == 1 && width == 2)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left =
			    (int)(volume * LittleShort(((short *)data)[src])) << 8;
			s_rawsamples[dst].right =
			    (int)(volume * LittleShort(((short *)data)[src])) << 8;
		}
	}
	else if (channels == 2 && width == 1)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left =
			    (int)(volume * ((char *)data)[src*2]) << 16;
			s_rawsamples[dst].right =
			    (int)(volume * ((char *)data)[src*2+1]) << 16;
		}
	}
	else if (channels == 1 && width == 1)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left =
			    (int)(volume * (((byte *)data)[src]-128)) << 16;
			s_rawsamples[dst].right = 
				(int)(volume * (((byte *)data)[src]-128)) << 16;
		}
	}
}

/* Console commands. */

/*
==========
OGG_ListCmd

List Ogg Vorbis files.
==========
*/
void OGG_ListCmd(void)
{
	int i;

	for (i = 0; i < ogg_numfiles; i++)
		Com_Printf("%d %s\n", i+1, ogg_filelist[i]);

	Com_Printf("%d Ogg Vorbis files.\n", ogg_numfiles);
}

/*
==========
OGG_ParseCmd

Parse play controls.
==========
*/
void OGG_ParseCmd(char *arg)
{
	int n; 
	cvar_t *ogg_enable;
	ogg_enable = Cvar_Get ("ogg_enable", "0", CVAR_ARCHIVE);  

	switch (arg[0]) {
	case '#':
		n = atoi(arg+1) - 1;
		OGG_Open(ABS, n);
		break;
	case '?':
		OGG_Open(ABS, rand() % ogg_numfiles);
		break;
	case '>':
		if (strlen(arg) > 1)
			OGG_Open(REL, atoi(arg+1));
		else
			OGG_Open(REL, 1);
		break;
	case '<':
		if (strlen(arg) > 1)
			OGG_Open(REL, -atoi(arg+1));
		else
			OGG_Open(REL, -1);
		break;
	default:
		if (ogg_enable->value != 0)
			OGG_OpenName(arg);
		break;
	}
}

/*
==========
OGG_PauseCmd

Pause current song.
==========
*/
void OGG_PauseCmd(void)
{

	if (ogg_status == PLAY)
		ogg_status = PAUSE;
}

/*
==========
OGG_PlayCmd

Play control.
==========
*/
void OGG_PlayCmd( void )
{

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: ogg_play {filename | #n | ? | >n | <n}\n");
		return;
	}

	OGG_ParseCmd(Cmd_Argv(1));
}

/*
==========
OGG_ResumeCmd

Resume current song.
==========
*/
void OGG_ResumeCmd(void)
{

	if (ogg_status == PAUSE)
		ogg_status = PLAY;
}

/*
==========
OGG_SeekCmd

Change position in the file being played.
==========
*/
void OGG_SeekCmd(void)
{

	if (ogg_status != STOP)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: ogg_seek {n | <n | >n}\n");
		return;
	}

	switch (Cmd_Argv(1)[0]) {
		case '>':
			OGG_Seek(REL, atof(Cmd_Argv(1)+1));
			break;
		case '<':
			OGG_Seek(REL, -atof(Cmd_Argv(1)+1));
			break;
		default:
			OGG_Seek(ABS, atof(Cmd_Argv(1)));
			break;
	}
}

/*
==========
OGG_StatusCmd

Display status.
==========
*/
void OGG_StatusCmd(void)
{

	switch (ogg_status) {
	case PLAY:
		Com_Printf("Playing file %d (%s) at %0.2f seconds.\n",
		    ogg_curfile+1, ogg_filelist[ogg_curfile], ov_time_tell(&ovFile));
		break;
	case PAUSE:
		Com_Printf("Paused file %d (%s) at %0.2f seconds.\n",
		    ogg_curfile+1, ogg_filelist[ogg_curfile], ov_time_tell(&ovFile));
		break;
	case STOP:
		if (ogg_curfile == -1)
			Com_Printf("Stopped.\n");
		else
			Com_Printf("Stopped file %d (%s).\n",
			    ogg_curfile+1, ogg_filelist[ogg_curfile]);
		break;
	}
}
