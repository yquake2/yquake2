/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2010 Yamagi Burmeister
 * Copyright (C) 2005 Ryan C. Gordon
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
 * The lower layer of the sound system. It utilizes SDL for writing the
 * sound data to the device. This file was rewritten by Yamagi to solve
 * a lot problems arrising from the old Icculus Quake 2 SDL backend like
 * stuttering and cracking. This implementation is based on ioQuake3s
 * snd_sdl.c.
 *
 * =======================================================================
 */

#ifdef _WIN32
#include <SDL/SDL.h>
#elif defined(__APPLE__)
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

#include "../../client/header/client.h"
#include "../../client/sound/header/local.h"

#define SDL_FULLVOLUME 80
#define SDL_LOOPATTENUATE 0.003

/* Global stuff */
int    snd_inited = 0;
static int dmapos = 0;
static int dmasize = 0;
static dma_t *dmabackend;
cvar_t *s_sdldriver;

/* ------------------------------------------------------------------ */

/*
 * Calculates when a sound
 * must be started.
 */
int
SDL_DriftBeginofs(float timeofs)
{
	int start = (int)(cl.frame.servertime * 0.001f * dma.speed + s_beginofs);

	if (start < paintedtime)
	{
		start = paintedtime;
		s_beginofs = (int)(start - (cl.frame.servertime * 0.001f * dma.speed));
	}
	else if (start > paintedtime + 0.3f * dma.speed)
	{
		start = (int)(paintedtime + 0.1f * dma.speed);
		s_beginofs = (int)(start - (cl.frame.servertime * 0.001f * dma.speed));
	}
	else
	{
		s_beginofs -= 10;
	}

	return timeofs ? start + timeofs * dma.speed : paintedtime;
}

/*
 * Spatialize a sound effect based on it's origin.
 */
void
SDL_SpatializeOrigin(vec3_t origin, float master_vol, float dist_mult, 
		int *left_vol, int *right_vol)
{
	vec_t dot;
	vec_t dist;
	vec_t lscale, rscale, scale;
	vec3_t source_vec;

	if (cls.state != ca_active)
	{
		*left_vol = *right_vol = 255;
		return;
	}

	/* Calculate stereo seperation and distance attenuation */
	VectorSubtract(origin, listener_origin, source_vec);

	dist = VectorNormalize(source_vec);
	dist -= SDL_FULLVOLUME;

	if (dist < 0)
	{
		dist = 0; /* Close enough to be at full volume */
	}

	dist *= dist_mult;
	dot = DotProduct(listener_right, source_vec);

	if ((dma.channels == 1) || !dist_mult)
	{
		rscale = 1.0f;
		lscale = 1.0f;
	}
	else
	{
		rscale = 0.5f * (1.0f + dot);
		lscale = 0.5f * (1.0f - dot);
	}

	/* Add in distance effect */
	scale = (1.0f - dist) * rscale;
	*right_vol = (int)(master_vol * scale);

	if (*right_vol < 0)
	{
		*right_vol = 0;
	}

	scale = (1.0 - dist) * lscale;
	*left_vol = (int)(master_vol * scale);

	if (*left_vol < 0)
	{
		*left_vol = 0;
	}
}

/*
 * Spatializes a channel.
 */
void
SDL_Spatialize(channel_t *ch)
{
	vec3_t origin;

	/* Anything coming from the view entity
	   will always be full volume */
	if (ch->entnum == cl.playernum + 1)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	if (ch->fixed_origin)
	{
		VectorCopy(ch->origin, origin);
	}
	else
	{
		CL_GetEntitySoundOrigin(ch->entnum, origin);
	}

	SDL_SpatializeOrigin(origin, (float)ch->master_vol, ch->dist_mult,
			&ch->leftvol, &ch->rightvol);
}

/*
 * Entities with a "sound" field will generated looped sounds
 * that are automatically started, stopped, and merged together
 * as the entities are sent to the client
 */
void
SDL_AddLoopSounds(void)
{
	int i, j;
	int sounds[MAX_EDICTS];
	int left, right, left_total, right_total;
	channel_t *ch;
	sfx_t *sfx;
	sfxcache_t *sc;
	int num;
	entity_state_t *ent;
	vec3_t origin;

	if (cl_paused->value)
	{
		return;
	}

	if (cls.state != ca_active)
	{
		return;
	}

	if (!cl.sound_prepped || !s_ambient->value)
	{
		return;
	}

	memset(&sounds, 0, sizeof(int) * MAX_EDICTS);
	S_BuildSoundList(sounds);

	for (i = 0; i < cl.frame.num_entities; i++)
	{
		if (!sounds[i])
		{
			continue;
		}

		sfx = cl.sound_precache[sounds[i]];

		if (!sfx)
		{
			continue; /* bad sound effect */
		}

		sc = sfx->cache;

		if (!sc)
		{
			continue;
		}

		num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		ent = &cl_parse_entities[num];

		CL_GetEntitySoundOrigin(ent->number, origin);

		/* find the total contribution of all sounds of this type */
		SDL_SpatializeOrigin(ent->origin, 255.0f, SDL_LOOPATTENUATE,
				&left_total, &right_total);

		for (j = i + 1; j < cl.frame.num_entities; j++)
		{
			if (sounds[j] != sounds[i])
			{
				continue;
			}

			sounds[j] = 0; /* don't check this again later */
			num = (cl.frame.parse_entities + j) & (MAX_PARSE_ENTITIES - 1);
			ent = &cl_parse_entities[num];

			SDL_SpatializeOrigin(ent->origin, 255.0f, SDL_LOOPATTENUATE, &left, &right);

			left_total += left;
			right_total += right;
		}

		if ((left_total == 0) && (right_total == 0))
		{
			continue; /* not audible */
		}

		/* allocate a channel */
		ch = S_PickChannel(0, 0);

		if (!ch)
		{
			return;
		}

		if (left_total > 255)
		{
			left_total = 255;
		}

		if (right_total > 255)
		{
			right_total = 255;
		}

		ch->leftvol = left_total;
		ch->rightvol = right_total;
		ch->autosound = true; /* remove next frame */
		ch->sfx = sfx;

		/* Sometimes, the sc->length argument can become 0,
		   and in that case we get a SIGFPE in the next
		   modulo operation. The workaround checks for this
		   situation and in that case, sets the pos and end
		   parameters to 0. */
		if (sc->length == 0)
		{
			ch->pos = 0;
			ch->end = 0;
		}
		else
		{
			ch->pos = paintedtime % sc->length;
			ch->end = paintedtime + sc->length - ch->pos;
		}
	}
}

/*
 * Clears the playback buffer so
 * that all playback stops.
 */
void
SDL_ClearBuffer(void)
{
	int clear;
	int i;
	unsigned char *ptr = dma.buffer;

	if (!sound_started)
	{
		return;
	}

	s_rawend = 0;

	if (dma.samplebits == 8)
	{
		clear = 0x80;
	}
	else
	{
		clear = 0;
	}

	SDL_LockAudio();

	if (dma.buffer)
	{
		i = dma.samples * dma.samplebits / 8;

		while (i--)
		{
			*ptr = clear;
			ptr++;
		}
	}

	SDL_UnlockAudio();
}

/*
 * Calculates the absolute timecode
 * of current playback.
 */
void
SDL_UpdateSoundtime(void)
{
	static int buffers;
	static int oldsamplepos;
	int fullsamples;

	fullsamples = dma.samples / dma.channels;

	/* it is possible to miscount buffers if it has wrapped twice between
	   calls to S_Update. Oh well. This a hack around that. */
	if (dmapos < oldsamplepos)
	{
		buffers++; /* buffer wrapped */

		if (paintedtime > 0x40000000)
		{
			/* time to chop things off to avoid 32 bit limits */
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds();
		}
	}

	oldsamplepos = dmapos;
	soundtime = buffers * fullsamples + dmapos / dma.channels;
}

/* ------------------------------------------------------------------ */

/*
 * Gives information over user
 * defineable variables
 */
void
SDL_SoundInfo(void)
{
	Com_Printf("%5d stereo\n", dma.channels - 1);
	Com_Printf("%5d samples\n", dma.samples);
	Com_Printf("%5d samplepos\n", dma.samplepos);
	Com_Printf("%5d samplebits\n", dma.samplebits);
	Com_Printf("%5d submission_chunk\n", dma.submission_chunk);
	Com_Printf("%5d speed\n", dma.speed);
	Com_Printf("%p dma buffer\n", dma.buffer);
}
 
/*
 * Callback funktion for SDL. Writes 
 * sound data to SDL when requested.
 */
static void
SDL_Callback(void *data, Uint8 *stream, int length)
{
	int length1;
	int length2;
	int pos = (dmapos * (dmabackend->samplebits / 8));

	if (pos >= dmasize)
	{
		dmapos = pos = 0;
	}

	/* This can't happen! */
	if (!snd_inited)
	{
		memset(stream, '\0', length);
		return;
	}

	int tobufferend = dmasize - pos;

	if (length > tobufferend)
	{
		length1 = tobufferend;
		length2 = length - length1;
	}
	else
	{
		length1= length;
		length2 = 0;
	}

	memcpy(stream, dmabackend->buffer + pos, length1);

	/* Set new position */
	if (length2 <= 0)
	{
		dmapos += (length1 / (dmabackend->samplebits / 8));
	}
	else
	{
		memcpy(stream + length1, dmabackend->buffer, length2);
		dmapos = (length2 / (dmabackend->samplebits / 8));
	}

    if (dmapos >= dmasize)
	{
		dmapos = 0;
	}
}

/*
 * Initializes the SDL sound
 * backend and sets up SDL.
 */
qboolean
SDL_BackendInit(void)
{
	char drivername[128];
	char reqdriver[128];
	SDL_AudioSpec desired;
	SDL_AudioSpec obtained;
	int tmp, val;

	/* This should never happen,
	   but this is Quake 2 ... */
	if (snd_inited)
	{
		return 1;
	}

	int sndbits = (Cvar_Get("sndbits", "16", CVAR_ARCHIVE))->value;
	int sndfreq = (Cvar_Get("s_khz", "44", CVAR_ARCHIVE))->value;
	int sndchans = (Cvar_Get("sndchannels", "2", CVAR_ARCHIVE))->value;

#ifdef _WIN32
	s_sdldriver = (Cvar_Get("s_sdldriver", "dsound", CVAR_ARCHIVE));
#elif __linux__
	s_sdldriver = (Cvar_Get("s_sdldriver", "alsa", CVAR_ARCHIVE));
#elif __APPLE__
	s_sdldriver = (Cvar_Get("s_sdldriver", "CoreAudio", CVAR_ARCHIVE));
#else
	s_sdldriver = (Cvar_Get("s_sdldriver", "dsp", CVAR_ARCHIVE));
#endif

	snprintf(reqdriver, sizeof(drivername), "%s=%s", "SDL_AUDIODRIVER", s_sdldriver->string);
	putenv(reqdriver);

	Com_Printf("Starting SDL audio callback.\n");

	if (!SDL_WasInit(SDL_INIT_AUDIO))
	{
		if (SDL_Init(SDL_INIT_AUDIO) == -1)
		{
			Com_Printf ("Couldn't init SDL audio: %s.\n", SDL_GetError ());
			return 0;
		}
	}

	if (SDL_AudioDriverName(drivername, sizeof(drivername)) == NULL)
	{
		strcpy(drivername, "(UNKNOW)");
	}

	Com_Printf("SDL audio driver is \"%s\".\n", drivername);

	memset(&desired, '\0', sizeof(desired));
	memset(&obtained, '\0', sizeof(obtained));

	/* Users are stupid */
	if ((sndbits != 16) && (sndbits != 8))
	{
		sndbits = 16;
	}

	if (sndfreq == 48)
	{
		desired.freq = 48000;
	}
	else if (sndfreq == 44)
	{
		desired.freq = 44100;
	}
	else if (sndfreq == 22)
	{
		desired.freq = 22050;
	}
	else if (sndfreq == 11)
	{
		desired.freq = 11025;
	}

	desired.format = ((sndbits == 16) ? AUDIO_S16SYS : AUDIO_U8);

	if (desired.freq <= 11025)
	{
		desired.samples = 256;
	}
	else if (desired.freq <= 22050)
	{
		desired.samples = 512;
	}
	else if (desired.freq <= 44100)
	{
		desired.samples = 1024;
	}
	else
	{
		desired.samples = 2048;
	}

	desired.channels = sndchans;
	desired.callback = SDL_Callback;

	/* Okay, let's try our luck */
	if (SDL_OpenAudio(&desired, &obtained) == -1)
	{
		Com_Printf("SDL_OpenAudio() failed: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return 0;
	}

	/* This points to the frontend */
	dmabackend = &dma;

	dmapos = 0;
	dmabackend->samplebits = obtained.format & 0xFF;
	dmabackend->channels = obtained.channels;

	tmp = (obtained.samples * obtained.channels) * 10;
	if (tmp & (tmp - 1))
	{	/* make it a power of two */
		val = 1;
		while (val < tmp)
			val <<= 1;

		tmp = val;
	}
	dmabackend->samples = tmp;

	dmabackend->submission_chunk = 1;
	dmabackend->speed = obtained.freq;
	dmasize = (dmabackend->samples * (dmabackend->samplebits / 8));
	dmabackend->buffer = calloc(1, dmasize);

	s_numchannels = MAX_CHANNELS;
	S_InitScaletable();

	SDL_PauseAudio(0);

	Com_Printf("SDL audio initialized.\n");
	snd_inited = 1;
	return 1;
}

/*
 * Shuts the SDL backend down.
 */
void
SDL_BackendShutdown(void)
{
	Com_Printf("Closing SDL audio device...\n");
    SDL_PauseAudio(1);
    SDL_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    free(dmabackend->buffer);
    dmabackend->buffer = NULL;
    dmapos = dmasize = 0;
    snd_inited = 0;
    Com_Printf("SDL audio device shut down.\n");
}
 
/*
 * This sends the sound to the device.
 * In the SDL backend it's useless and
 * only implemented for compatiblity.
 */
void
SDL_Submit(void)
{
	SDL_UnlockAudio();
}

void
SDL_BeginPainting(void)
{
	SDL_LockAudio();
}

