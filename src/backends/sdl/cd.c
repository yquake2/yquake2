/*
 * Copyright (C) 2001 Robert Bäuml
 * Copyright (C) 2002 W. P. va Paassen
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
 * Audio-CD backend in SDL
 *
 * =======================================================================
 */

#ifdef CDA

#include <stdio.h>

#ifdef _WIN32
 #include "SDL/SDL.h"
#elif defined(__APPLE__)
 #include <SDL/SDL.h>
#else
 #include "SDL.h"
#endif

#include "../../client/header/client.h"

static qboolean cdValid = false;
static qboolean initialized = false;
static qboolean enabled = true;
static qboolean playLooping = false;
static SDL_CD *cd_id;
static float cdvolume = 1.0;
static int lastTrack = 0;

cvar_t *cd_volume;
cvar_t *cd_nocd;
cvar_t *cd_dev;

static void CD_f();

static void
CDAudio_Eject()
{
	if (!cd_id || !enabled)
	{
		return;
	}

	if (SDL_CDEject(cd_id))
	{
		Com_DPrintf("Unable to eject CD-ROM tray.\n");
	}
}

void
CDAudio_Play(int track, qboolean looping)
{
	CDstatus cd_stat;

	lastTrack = track + 1;

	if (!cd_id || !enabled)
	{
		return;
	}

	cd_stat = SDL_CDStatus(cd_id);

	if (!cdValid)
	{
		if (!CD_INDRIVE(cd_stat) || (!cd_id->numtracks))
		{
			return;
		}

		cdValid = true;
	}

	if ((track < 1) || (track >= cd_id->numtracks))
	{
		Com_DPrintf("CDAudio: Bad track number: %d\n", track);
		return;
	}

	track--;

	if (cd_stat == CD_PLAYING)
	{
		if (cd_id->cur_track == track)
		{
			return;
		}

		CDAudio_Stop();
	}

	if (SDL_CDPlay(cd_id, cd_id->track[track].offset,
				cd_id->track[track].length))
	{
		Com_DPrintf("CDAudio_Play: Unable to play track: %d (%s)\n",
				track + 1, SDL_GetError());
		return;
	}

	playLooping = looping;
}

void
CDAudio_RandomPlay(void)
{
	int track, i = 0, free_tracks = 0;
	float f;
	CDstatus cd_stat;
	byte *track_bools;

	if (!cd_id || !enabled)
	{
		return;
	}

	track_bools = (byte *)malloc(cd_id->numtracks * sizeof(byte));

	if (track_bools == 0)
	{
		return;
	}

	/* create an array of available audio tracknumbers */
	for ( ; i < cd_id->numtracks; i++)
	{
		track_bools[i] = cd_id->track[i].type == SDL_AUDIO_TRACK;
		free_tracks += track_bools[i];
	}

	if (!free_tracks)
	{
		Com_DPrintf("CDAudio_RandomPlay: Unable to find and play a random audio track, insert an audio cd please");
		goto free_end;
	}

	/*choose random audio track */
	do
	{
		do
		{
			f = ((float)randk()) / ((float)RAND_MAX + 1.0);
			track = (int)(cd_id->numtracks * f);
		}
		while (!track_bools[track]);

		lastTrack = track + 1;

		cd_stat = SDL_CDStatus(cd_id);

		if (!cdValid)
		{
			if (!CD_INDRIVE(cd_stat) || (!cd_id->numtracks))
			{
				goto free_end;
			}

			cdValid = true;
		}

		if (cd_stat == CD_PLAYING)
		{
			if (cd_id->cur_track == track + 1)
			{
				goto free_end;
			}

			CDAudio_Stop();
		}

		if (SDL_CDPlay(cd_id, cd_id->track[track].offset,
					cd_id->track[track].length))
		{
			track_bools[track] = 0;
			free_tracks--;
		}
		else
		{
			playLooping = true;
			break;
		}
	}
	while (free_tracks > 0);

free_end:
	free((void *)track_bools);
}

void
CDAudio_Stop()
{
	int cdstate;

	if (!cd_id || !enabled)
	{
		return;
	}

	cdstate = SDL_CDStatus(cd_id);

	if ((cdstate != CD_PLAYING) && (cdstate != CD_PAUSED))
	{
		return;
	}

	if (SDL_CDStop(cd_id))
	{
		Com_DPrintf("CDAudio_Stop: Failed to stop track.\n");
	}

	playLooping = 0;
}

void
CDAudio_Pause()
{
	if (!cd_id || !enabled)
	{
		return;
	}

	if (SDL_CDStatus(cd_id) != CD_PLAYING)
	{
		return;
	}

	if (SDL_CDPause(cd_id))
	{
		Com_DPrintf("CDAudio_Pause: Failed to pause track.\n");
	}
}

void
CDAudio_Resume()
{
	if (!cd_id || !enabled)
	{
		return;
	}

	if (SDL_CDStatus(cd_id) != CD_PAUSED)
	{
		return;
	}

	if (SDL_CDResume(cd_id))
	{
		Com_DPrintf("CDAudio_Resume: Failed to resume track.\n");
	}
}

void
CDAudio_Update()
{
	static int cnt = 0;

	if (!cd_id || !enabled)
	{
		return;
	}

	if (cd_volume && (cd_volume->value != cdvolume))
	{
		if (cdvolume)
		{
			Cvar_SetValue("cd_volume", 0.0);
			CDAudio_Pause();
		}
		else
		{
			Cvar_SetValue("cd_volume", 1.0);
			CDAudio_Resume();
		}

		cdvolume = cd_volume->value;
		return;
	}

	/* this causes too much overhead to be executed every frame */
	if (++cnt == 16)
	{
		cnt = 0;

		if (cd_nocd->value)
		{
			CDAudio_Stop();
			return;
		}

		if (playLooping &&
			(SDL_CDStatus(cd_id) != CD_PLAYING) &&
			(SDL_CDStatus(cd_id) != CD_PAUSED))
		{
			CDAudio_Play(lastTrack, true);
		}
	}
}

int
CDAudio_Init()
{
	cvar_t *cv;

	if (initialized)
	{
		return 0;
	}

	cv = Cvar_Get("nocdaudio", "0", CVAR_NOSET);

	if (cv->value)
	{
		return -1;
	}

#ifdef OGG
	cd_nocd = Cvar_Get("cd_nocd", "1", CVAR_ARCHIVE);
#else
	cd_nocd = Cvar_Get("cd_nocd", "0", CVAR_ARCHIVE);
#endif

	if (cd_nocd->value)
	{
		return -1;
	}

	cd_volume = Cvar_Get("cd_volume", "1", CVAR_ARCHIVE);

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0)
	{
		if (SDL_Init(SDL_INIT_CDROM) < 0)
		{
			Com_Printf("Couldn't init SDL cdrom: %s\n", SDL_GetError());
			return -1;
		}
	}
	else if (SDL_WasInit(SDL_INIT_CDROM) == 0)
	{
		if (SDL_InitSubSystem(SDL_INIT_CDROM) < 0)
		{
			Com_Printf("Couldn't init SDL cdrom: %s\n", SDL_GetError());
			return -1;
		}
	}

	cd_id = SDL_CDOpen(0);

	if (!cd_id)
	{
		Com_Printf("CDAudio_Init: Unable to open default CD-ROM drive: %s\n",
				SDL_GetError());
		return -1;
	}

	initialized = true;
	enabled = true;
	cdValid = true;

	if (!CD_INDRIVE(SDL_CDStatus(cd_id)))
	{
		Com_Printf("CDAudio_Init: No CD in drive.\n");
		cdValid = false;
	}

	if (!cd_id->numtracks)
	{
		Com_Printf("CDAudio_Init: CD contains no audio tracks.\n");
		cdValid = false;
	}

	Cmd_AddCommand("cd", CD_f);
	Com_Printf("CD Audio Initialized.\n");
	return 0;
}

void
CDAudio_Shutdown()
{
	if (!cd_id)
	{
		return;
	}

	CDAudio_Stop();
	SDL_CDClose(cd_id);
	cd_id = NULL;

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_CDROM)
	{
		SDL_Quit();
	}
	else
	{
		SDL_QuitSubSystem(SDL_INIT_CDROM);
	}

	initialized = false;
}

static void
CD_f()
{
	char *command;
	int cdstate;

	if (Cmd_Argc() < 2)
	{
		return;
	}

	command = Cmd_Argv(1);

	if (!Q_strcasecmp(command, "on"))
	{
		enabled = true;
	}

	if (!Q_strcasecmp(command, "off"))
	{
		if (!cd_id)
		{
			return;
		}

		cdstate = SDL_CDStatus(cd_id);

		if ((cdstate == CD_PLAYING) || (cdstate == CD_PAUSED))
		{
			CDAudio_Stop();
		}

		enabled = false;
		return;
	}

	if (!Q_strcasecmp(command, "play"))
	{
		CDAudio_Play((byte)(int)strtol(Cmd_Argv(2), (char **)NULL, 10), false);
		return;
	}

	if (!Q_strcasecmp(command, "loop"))
	{
		CDAudio_Play((byte)(int)strtol(Cmd_Argv(2), (char **)NULL, 10), true);
		return;
	}

	if (!Q_strcasecmp(command, "stop"))
	{
		CDAudio_Stop();
		return;
	}

	if (!Q_strcasecmp(command, "pause"))
	{
		CDAudio_Pause();
		return;
	}

	if (!Q_strcasecmp(command, "resume"))
	{
		CDAudio_Resume();
		return;
	}

	if (!Q_strcasecmp(command, "eject"))
	{
		CDAudio_Eject();
		return;
	}

	if (!Q_strcasecmp(command, "info"))
	{
		if (!cd_id)
		{
			return;
		}

		cdstate = SDL_CDStatus(cd_id);
		Com_Printf("%d tracks\n", cd_id->numtracks);

		if (cdstate == CD_PLAYING)
		{
			Com_Printf("Currently %s track %d\n",
					playLooping ? "looping" : "playing",
					cd_id->cur_track + 1);
		}
		else
		if (cdstate == CD_PAUSED)
		{
			Com_Printf("Paused %s track %d\n",
					playLooping ? "looping" : "playing",
					cd_id->cur_track + 1);
		}

		return;
	}
}

void
CDAudio_Activate(qboolean active)
{
	if (active)
	{
		CDAudio_Resume();
	}
	else
	{
		CDAudio_Pause();
	}
}

#endif /* CDA */
