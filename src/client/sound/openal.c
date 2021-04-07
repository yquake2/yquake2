/*
 * Copyright (C) 1997-2005 Id Software, Inc.
 *           (C) 2010 skuller.net
 *           (C) 2005 Stuart Dalton (badcdev@gmail.com)
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
 * This is the OpenAL backend for the Quake II Soundsystem. Most of these
 * functions were optained from the Q2Pro project, and some are from zeq2.
 * We adapted them to work with Yamagi Quake II. The OpenAL backend is
 * split into two layers. This is the upper layer, doing the actual work.
 * The lower layer implements the interface between Q2 and the OpenAL
 * implementation. This backend is overmuch complicated due to the
 * restrictions of the frontend.
 *
 * =======================================================================
 */

#ifdef USE_OPENAL

#include "../header/client.h"
#include "header/local.h"
#include "header/qal.h"
#include "header/vorbis.h"

/* translates from AL coordinate system to quake */
#define AL_UnpackVector(v) - v[1], v[2], -v[0]
#define AL_CopyVector(a, b) ((b)[0] = -(a)[1], (b)[1] = (a)[2], (b)[2] = -(a)[0])

// avg US male height / q2PlayerHeight = 1.764f / 56.0f = 0.0315f
// see: https://en.wikipedia.org/wiki/List_of_average_human_height_worldwide
// see: PutClientInServer(edict_t *ent) bbox
#define AL_METER_OF_Q2_UNIT 0.0315f

/* The OpenAL implementation should support
   at least this number of sources */
#define MIN_CHANNELS 16

/* Globals */
int active_buffers;
static qboolean streamPlaying;
static ALuint s_srcnums[MAX_CHANNELS - 1];
static ALuint streamSource;
static int s_framecount;
static ALuint underwaterFilter;

/* ----------------------------------------------------------------- */

/*
 * Silence / stop all OpenAL streams
 */
static void
AL_StreamDie(void)
{
	int numBuffers;

	/* openal might not be initialised yet */
	if (!qalSourceStop)
            return;

	streamPlaying = false;
	qalSourceStop(streamSource);

	/* Un-queue any buffers, and delete them */
	qalGetSourcei(streamSource, AL_BUFFERS_QUEUED, &numBuffers);

	while (numBuffers--)
	{
		ALuint buffer;
		qalSourceUnqueueBuffers(streamSource, 1, &buffer);
		qalDeleteBuffers(1, &buffer);
		active_buffers--;
	}
}

/*
 * Updates stream sources by removing all played
 * buffers and restarting playback if necessary.
 */
static void
AL_StreamUpdate(void)
{
	int numBuffers;
	ALint state;

	qalGetSourcei(streamSource, AL_SOURCE_STATE, &state);

	if (state == AL_STOPPED)
	{
		streamPlaying = false;
	}
	else
	{
		/* Un-queue any already pleyed buffers and delete them */
		qalGetSourcei(streamSource, AL_BUFFERS_PROCESSED, &numBuffers);

		while (numBuffers--)
		{
			ALuint buffer;
			qalSourceUnqueueBuffers(streamSource, 1, &buffer);
			qalDeleteBuffers(1, &buffer);
			active_buffers--;
		}
	}

	/* Start the streamSource playing if necessary */
	qalGetSourcei(streamSource, AL_BUFFERS_QUEUED, &numBuffers);

	if (!streamPlaying && numBuffers)
	{
		qalSourcePlay(streamSource);
		streamPlaying = true;
	}
}

/* ----------------------------------------------------------------- */

static ALenum
AL_GetFormat(int width, int channels)
{
	if (width == 1)
	{
		if (channels == 1)
		{
			return AL_FORMAT_MONO8;
		}
		else if (channels == 2)
		{
			return AL_FORMAT_STEREO8;
		}
	}
	else if (width == 2)
	{
		if (channels == 1)
		{
			return AL_FORMAT_MONO16;
		}
		else if (channels == 2)
		{
			return AL_FORMAT_STEREO16;
		}
	}

	return AL_FORMAT_MONO8;
}

/*
 * Uploads a sample to OpenAL and places
 * a dummy entry in the frontends cache.
 * This is not nice but necessary for the
 * frontend to work.
 */
sfxcache_t *
AL_UploadSfx(sfx_t *s, wavinfo_t *s_info, byte *data)
{
	sfxcache_t *sc;
	ALsizei size;
	ALenum format;
	ALuint name;

	size = s_info->samples * s_info->width;

	/* Work out format */
	format = AL_GetFormat(s_info->width, s_info->channels);

	if (!size)
	{
		return NULL;
	}

	qalGetError();
	qalGenBuffers(1, &name);
	qalBufferData(name, format, data, size, s_info->rate);
	active_buffers++;

	if (qalGetError() != AL_NO_ERROR)
	{
		return NULL;
	}

	/* allocate placeholder sfxcache */
	sc = s->cache = Z_TagMalloc(sizeof(*sc), 0);
	sc->length = s_info->samples * 1000 / s_info->rate;
	sc->loopstart = s_info->loopstart;
	sc->width = s_info->width;
	sc->size = size;
	sc->bufnum = name;
	sc->stereo = s_info->channels - 1;

	return sc;
}

/*
 * Deletes a sample from OpenALs internal
 * cache. The dummy entry in the frontends
 * cache is deleted by the frontend.
 */
void
AL_DeleteSfx(sfx_t *s)
{
	sfxcache_t *sc;
	ALuint name;

	sc = s->cache;

	if (!sc)
	{
		return;
	}

	name = sc->bufnum;
	qalDeleteBuffers(1, &name);
	active_buffers--;
}

/* ----------------------------------------------------------------- */

/*
 * Performance stereo spatialization
 * of a channel in the frontends
 * sense.
 */
static void
AL_Spatialize(channel_t *ch)
{
	vec3_t origin;
	vec3_t velocity;

	if ((ch->entnum == -1) || (ch->entnum == cl.playernum + 1) || !ch->dist_mult)
	{
		/* from view entity (player) => nothing to do,
		 * position is still (0,0,0) and relative,
		 * as set in AL_PlayChannel() */

		return;
	}
	else if (ch->fixed_origin)
	{
		VectorCopy(ch->origin, origin);
		qalSource3f(ch->srcnum, AL_POSITION, AL_UnpackVector(origin));
		return;
	}
	else
	{
		CL_GetEntitySoundOrigin(ch->entnum, origin);
		qalSource3f(ch->srcnum, AL_POSITION, AL_UnpackVector(origin));

		if (s_doppler->value) {
			CL_GetEntitySoundVelocity(ch->entnum, velocity);
			VectorScale(velocity, AL_METER_OF_Q2_UNIT, velocity);
			qalSource3f(ch->srcnum, AL_VELOCITY, AL_UnpackVector(velocity));
		}

		return;
	}
}

/*
 * Plays a channel (in the frontends
 * sense) with OpenAL.
 */
void
AL_PlayChannel(channel_t *ch)
{
	sfxcache_t *sc;
	float vol;

	/* Debug */
	if (s_show->value > 1)
	{
		Com_Printf("%s: %s\n", __func__, ch->sfx->name);
	}

	/* Clamp volume */
	vol = ch->oal_vol;

	if (vol > 1.0f)
	{
		vol = 1.0f;
	}

	sc = ch->sfx->cache;
	ch->srcnum = s_srcnums[ch - channels];

	qalGetError();
	qalSourcef(ch->srcnum, AL_REFERENCE_DISTANCE, SOUND_FULLVOLUME);
	qalSourcef(ch->srcnum, AL_MAX_DISTANCE, 8192);
	qalSourcef(ch->srcnum, AL_ROLLOFF_FACTOR, ch->dist_mult * (8192 - SOUND_FULLVOLUME));
	qalSourcef(ch->srcnum, AL_GAIN, vol);
 	qalSourcei(ch->srcnum, AL_BUFFER, sc->bufnum);
	qalSourcei(ch->srcnum, AL_LOOPING, ch->autosound ? AL_TRUE : AL_FALSE);

	if ((ch->entnum == -1) || (ch->entnum == cl.playernum + 1) || !ch->dist_mult)
	{
		/* anything coming from the view entity will always
		 * be full volume and at the listeners position */
		qalSource3f(ch->srcnum, AL_POSITION, 0.0f, 0.0f, 0.0f);
		qalSourcei(ch->srcnum, AL_SOURCE_RELATIVE, AL_TRUE);
	}
	else
	{
		/* all other sources are *not* relative */
		qalSourcei(ch->srcnum, AL_SOURCE_RELATIVE, AL_FALSE);
	}


	/* Spatialize it */
	AL_Spatialize(ch);

	/* Play it */
	qalSourcePlay(ch->srcnum);

	/* Do not play broken channels */
	if (qalGetError() != AL_NO_ERROR)
	{
		AL_StopChannel(ch);
	}
}

/*
 * Stops playback of a "channel"
 * in the frontends sense.
 */
void
AL_StopChannel(channel_t *ch)
{
	/* Debug output */
	if (s_show->value > 1)
	{
		Com_Printf("%s: %s\n", __func__, ch->sfx->name);
	}

	/* stop it */
	qalSourceStop(ch->srcnum);
	qalSourcei(ch->srcnum, AL_BUFFER, AL_NONE);
	memset(ch, 0, sizeof(*ch));
}

/*
 * Stops playback of all channels.
 */
void
AL_StopAllChannels(void)
{
	int i;
	channel_t *ch;

	ch = channels;

	/* It doesn't matter if a channel
	   is active or not. */
	for (i = 0; i < s_numchannels; i++, ch++)
	{
		if (!ch->sfx)
		{
			continue;
		}

		AL_StopChannel(ch);
	}

	s_rawend = 0;

	/* Remove all pending samples */
	AL_StreamDie();
}

/* ----------------------------------------------------------------- */

/*
 * Returns the channel which contains
 * the looping sound for the entity
 * "entnum".
 */
static channel_t *
AL_FindLoopingSound(int entnum, sfx_t *sfx)
{
	int i;
	channel_t *ch;

	ch = channels;

	for (i = 0; i < s_numchannels; i++, ch++)
	{
		if (!ch->sfx)
		{
			continue;
		}

		if (!ch->autosound)
		{
			continue;
		}

		if (ch->entnum != entnum)
		{
			continue;
		}

		if (ch->sfx != sfx)
		{
			continue;
		}

		return ch;
	}

	return NULL;
}

/*
 * Plays an looping sound with OpenAL
 */
static void
AL_AddLoopSounds(void)
{
	int i;
	int sounds[MAX_EDICTS];
	channel_t *ch;
	sfx_t *sfx;
	sfxcache_t *sc;
	int num;
	entity_state_t *ent;

	if ((cls.state != ca_active) || cl_paused->value || !s_ambient->value)
	{
		return;
	}

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

		ch = AL_FindLoopingSound(ent->number, sfx);

		if (ch)
		{
			ch->autoframe = s_framecount;
			ch->end = paintedtime + sc->length;
			continue;
		}

		/* allocate a channel */
		ch = S_PickChannel(0, 0);

		if (!ch)
		{
			continue;
		}

		ch->autosound = true; /* remove next frame */
		ch->autoframe = s_framecount;
		ch->sfx = sfx;
		ch->entnum = ent->number;
		ch->master_vol = 1;
		ch->dist_mult = SOUND_LOOPATTENUATE;
		ch->end = paintedtime + sc->length;

		/* it seems like looped sounds are always played at full volume
		 * see SDL_AddLoopSounds() which calls SDL_SpatializeOrigin() with volume 255.0f
		 * so set it to full volume (1.0 * s_volume). */
		ch->oal_vol = s_volume->value;

		AL_PlayChannel(ch);
	}
}

/*
 * Starts all pending playsounds.
 */
static void
AL_IssuePlaysounds(void)
{
	playsound_t *ps;

	/* start any playsounds */
	while (1)
	{
		ps = s_pendingplays.next;

		if (ps == &s_pendingplays)
		{
			break; /* no more pending sounds */
		}

		if (ps->begin > paintedtime)
		{
			break;
		}

		S_IssuePlaysound(ps);
	}
}

/*
 * Queues raw samples for playback. Used
 * by the background music an cinematics.
 */
void
AL_RawSamples(int samples, int rate, int width, int channels,
		byte *data, float volume)
{
	ALuint buffer;
	ALuint format = 0;

	/* Work out format */
	format = AL_GetFormat(width, channels);

	/* Create a buffer, and stuff the data into it */
	qalGenBuffers(1, &buffer);
	qalBufferData(buffer, format, (ALvoid *)data,
			(samples * width * channels), rate);
	active_buffers++;

	/* set volume */
	if (volume > 1.0f)
	{
		volume = 1.0f;
	}

	qalSourcef(streamSource, AL_GAIN, volume);

	/* Shove the data onto the streamSource */
	qalSourceQueueBuffers(streamSource, 1, &buffer);

	/* emulate behavior of S_RawSamples for s_rawend */
	s_rawend += samples;
}

/*
 * Kills all raw samples still in flight.
 * This is used to stop music playback
 * when silence is triggered.
 */
void
AL_UnqueueRawSamples()
{
	AL_StreamDie();
}

static void
AL_UpdateUnderwater()
{
	int i;
	float gain_hf;
	qboolean update = false;
	ALuint filter;

	if (underwaterFilter == 0) {
		return;
	}

	if (s_underwater->modified) {
		update = true;
		s_underwater->modified = false;
		snd_is_underwater_enabled = ((int)s_underwater->value != 0);
	}

	if (s_underwater_gain_hf->modified) {
		update = true;
		s_underwater_gain_hf->modified = false;
	}

	if (!update) {
		return;
	}

	gain_hf = s_underwater_gain_hf->value;

	if (gain_hf < AL_LOWPASS_MIN_GAINHF) {
		gain_hf = AL_LOWPASS_MIN_GAINHF;
	}

	if (gain_hf > AL_LOWPASS_MAX_GAINHF) {
		gain_hf = AL_LOWPASS_MAX_GAINHF;
	}

	qalFilterf(underwaterFilter, AL_LOWPASS_GAINHF, gain_hf);

	if (snd_is_underwater_enabled && snd_is_underwater) {
		filter = underwaterFilter;
	} else {
		filter = 0;
	}

	for (i = 0; i < s_numchannels; ++i) {
		qalSourcei(s_srcnums[i], AL_DIRECT_FILTER, filter);
	}
}

/*
 * Main update function. Called every frame,
 * performes all necessary calculations.
 */
void
AL_Update(void)
{
	int i;
	channel_t *ch;
	vec_t orientation[6];
	vec3_t listener_velocity;

	paintedtime = cls.realtime;

	/* Do nothing if we aren't connected to a output device.
	   This is called after increasing paintedtime, because
	   the sound for the current frame needs to be skipped.
	   Otherwise sound and game will become asynchronous and
	   the paint buffer may overflow. */
	if (!QAL_RecoverLostDevice()) {
		return;
	}

	/* set listener (player) parameters */
	AL_CopyVector(listener_forward, orientation);
	AL_CopyVector(listener_up, orientation + 3);
	qalDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
	qalListener3f(AL_POSITION, AL_UnpackVector(listener_origin));
	qalListenerfv(AL_ORIENTATION, orientation);

	if (s_doppler->value) {
		CL_GetViewVelocity(listener_velocity);
		VectorScale(listener_velocity, AL_METER_OF_Q2_UNIT, listener_velocity);
		qalListener3f(AL_VELOCITY, AL_UnpackVector(listener_velocity));
	}

	/* update spatialization for dynamic sounds */
	ch = channels;

	for (i = 0; i < s_numchannels; i++, ch++)
	{
		if (!ch->sfx)
		{
			continue;
		}

		if (ch->autosound)
		{
			/* autosounds are regenerated fresh each frame */
			if (ch->autoframe != s_framecount)
			{
				AL_StopChannel(ch);
				continue;
			}
		}
		else
		{
			ALenum state;

			qalGetError();
			qalGetSourcei(ch->srcnum, AL_SOURCE_STATE, &state);

			if ((qalGetError() != AL_NO_ERROR) || (state == AL_STOPPED))
			{
				AL_StopChannel(ch);
				continue;
			}
		}

		if (s_show->value)
		{
			Com_Printf("%3i %s\n", ch->master_vol, ch->sfx->name);
		}

		/* respatialize channel */
		AL_Spatialize(ch);
	}

	s_framecount++;

	/* add loopsounds */
	AL_AddLoopSounds();

	/* add music */
	OGG_Stream();

	AL_StreamUpdate();
	AL_IssuePlaysounds();

	AL_UpdateUnderwater();
}

/* ----------------------------------------------------------------- */

/*
 * Enables underwater effect
 */
void
AL_Underwater()
{
	int i;

	if (sound_started != SS_OAL)
	{
		return;
	}

	if (underwaterFilter == 0)
		return;

	/* Apply to all sources */
	for (i = 0; i < s_numchannels; i++)
	{
		qalSourcei(s_srcnums[i], AL_DIRECT_FILTER, underwaterFilter);
	}
}

/*
 * Disables the underwater effect
 */
void
AL_Overwater()
{
	int i;

	if (sound_started != SS_OAL)
	{
		return;
	}

	if (underwaterFilter == 0)
		return;

	/* Apply to all sources */
	for (i = 0; i < s_numchannels; i++)
	{
		qalSourcei(s_srcnums[i], AL_DIRECT_FILTER, 0);
	}
}

/* ----------------------------------------------------------------- */

/*
 * Set up the stream sources
 */
static void
AL_InitStreamSource()
{
 	qalSource3f(streamSource, AL_POSITION, 0.0, 0.0, 0.0);
	qalSource3f(streamSource, AL_VELOCITY, 0.0, 0.0, 0.0);
	qalSource3f(streamSource, AL_DIRECTION, 0.0, 0.0, 0.0);
	qalSourcef(streamSource, AL_ROLLOFF_FACTOR, 0.0);
	qalSourcei(streamSource, AL_BUFFER, 0);
	qalSourcei(streamSource, AL_LOOPING, AL_FALSE);
	qalSourcei(streamSource, AL_SOURCE_RELATIVE, AL_TRUE);
}

/*
 * Set up the underwater filter
 */
static void
AL_InitUnderwaterFilter()
{
	underwaterFilter = 0;

	if (!(qalGenFilters && qalFilteri && qalFilterf && qalDeleteFilters))
		return;

	/* Generate a filter */
	qalGenFilters(1, &underwaterFilter);

	if (qalGetError() != AL_NO_ERROR)
	{
		Com_Printf("Couldn't generate an OpenAL filter!\n");
		return;
	}

	/* Low pass filter for underwater effect */
	qalFilteri(underwaterFilter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);

	if (qalGetError() != AL_NO_ERROR)
	{
		Com_Printf("Low pass filter is not supported!\n");
		return;
	}

	qalFilterf(underwaterFilter, AL_LOWPASS_GAIN, AL_LOWPASS_DEFAULT_GAIN);

	s_underwater->modified = true;
	s_underwater_gain_hf->modified = true;
}

/*
 * Initializes the OpenAL backend
 */
qboolean
AL_Init(void)
{
	int i;

	if (!QAL_Init())
	{
		Com_Printf("ERROR: OpenAL failed to initialize.\n");
		return false;
	}

	/* check for linear distance extension */
	if (!qalIsExtensionPresent("AL_EXT_LINEAR_DISTANCE"))
	{
		Com_Printf("ERROR: Required AL_EXT_LINEAR_DISTANCE extension is missing.\n");
		QAL_Shutdown();
		return false;
	}

	/* generate source names */
	qalGetError();
	qalGenSources(1, &streamSource);

	if (qalGetError() != AL_NO_ERROR)
	{
		Com_Printf("ERROR: Couldn't get a single Source.\n");
		QAL_Shutdown();
		return false;
	}
	else
	{
		/* -1 because we already got one channel for streaming */
		for (i = 0; i < MAX_CHANNELS - 1; i++)
		{
			qalGenSources(1, &s_srcnums[i]);

			if (qalGetError() != AL_NO_ERROR)
			{
				break;
			}
		}

		if (i < MIN_CHANNELS - 1)
		{
			Com_Printf("ERROR: Required at least %d sources, but got %d.\n",
					MIN_CHANNELS, i + 1);
			QAL_Shutdown();
			return false;
		}
	}

	s_numchannels = i;
	AL_InitStreamSource();
	AL_InitUnderwaterFilter();

	Com_Printf("Number of OpenAL sources: %d\n\n", s_numchannels);

	// exaggerate 2x because realistic is barely noticeable
	if (s_doppler->value) {
		qalDopplerFactor(2.0f);
	}

	return true;
}

/*
 * Shuts the OpenAL backend down
 */
void
AL_Shutdown(void)
{
	Com_Printf("Shutting down OpenAL.\n");

	AL_StopAllChannels();

	qalDeleteSources(1, &streamSource);
	qalDeleteFilters(1, &underwaterFilter);

	if (s_numchannels)
	{
		/* delete source names */
		qalDeleteSources(s_numchannels, s_srcnums);
		memset(s_srcnums, 0, sizeof(s_srcnums));
		s_numchannels = 0;
	}

	QAL_Shutdown();
}

#endif /* USE_OPENAL */

