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
 * The upper layer of the Quake II sound system. This is merely more
 * than an interface between the client and a backend. Currently only
 * two backends are supported:
 * - OpenAL, renders sound with OpenAL.
 * - SDL, has the same features than the original sound system.
 *
 * =======================================================================
 */

#include "../header/client.h"
#include "header/local.h"
#include "header/qal.h"
#include "header/vorbis.h"

/* During registration it is possible to have more sounds
   than could actually be referenced during gameplay,
   because we don't want to free anything until we are
   sure we won't need it. */
#define MAX_SFX (MAX_SOUNDS * 2)
#define MAX_PLAYSOUNDS 128

/* Maximum length (seconds) of audio data to test for silence. */
#define S_MAX_LEN_TO_TEST_FOR_SILENCE_S (2)

/* Minimum length (milliseconds) of audio data to treat as silence. */
#define S_MIN_LEN_TO_TREAT_AS_SILENCE_MS (3)

/* Minimum amplitude absolute value of unsigned 8-bit audio data to treat as silence. */
#define S_MIN_ABS_AMP_8_TO_TREAT_AS_SILENCE (1)

/* Minimum amplitude absolute value of signed 16-bit audio data to treat as silence. */
#define S_MIN_ABS_AMP_16_TO_TREAT_AS_SILENCE (2)


vec3_t listener_origin;
vec3_t listener_forward;
vec3_t listener_right;
vec3_t listener_up;

static playsound_t s_playsounds[MAX_PLAYSOUNDS];
static playsound_t s_freeplays;
playsound_t s_pendingplays;

cvar_t *s_volume;
cvar_t *s_testsound;
cvar_t *s_loadas8bit;
cvar_t *s_khz;
cvar_t *s_mixahead;
cvar_t *s_show;
cvar_t *s_ambient;
cvar_t* s_underwater;
cvar_t* s_underwater_gain_hf;
cvar_t* s_doppler;
cvar_t* s_ps_sorting;

channel_t channels[MAX_CHANNELS];
static int num_sfx;
int paintedtime;
int s_numchannels;
int s_rawend;
static int s_registration_sequence = 0;
portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];
static sfx_t known_sfx[MAX_SFX];
sndstarted_t sound_started = SS_NOT;
sound_t sound;
static qboolean s_registering;

qboolean snd_is_underwater;
qboolean snd_is_underwater_enabled;
/* ----------------------------------------------------------------- */

static qboolean
S_IsShortSilence(const wavinfo_t* info, const void* raw_data)
{
	/* Test only mono-audio data. */
	if (info->channels != 1)
	{
		return false;
	}

	/* Treat it as silence if sample count too small. */
	if (((info->samples * 1000) / info->rate) <= S_MIN_LEN_TO_TREAT_AS_SILENCE_MS)
	{
		return true;
	}

	/* Test only several second's of audio data. */
	if (info->samples > (info->rate * S_MAX_LEN_TO_TEST_FOR_SILENCE_S))
	{
		return false;
	}

	if (info->width == 1)
	{
		/* Unsigned 8-bit audio data. */

		const unsigned char* samples = (const unsigned char*)raw_data;
		const int sample_count = info->samples * info->width;

		int i;

		for (i = 0; i < sample_count; ++i)
		{
			const int sample = samples[i] - 0x80;

			if (abs(sample) > S_MIN_ABS_AMP_8_TO_TREAT_AS_SILENCE)
			{
				return false;
			}
		}
	}
	else if (info->width == 2)
	{
		/* Signed 16-bit audio data. */

		const short* samples = (const short*)raw_data;
		const int sample_count = info->samples * info->width;

		int i;

		for (i = 0; i < sample_count; ++i)
		{
			const int sample = LittleShort(samples[i]);

			if (abs(sample) > S_MIN_ABS_AMP_16_TO_TREAT_AS_SILENCE)
			{
				return false;
			}
		}
	}
	else
	{
		/* Unsupported bit depth. */
		return false;
	}

	return true;
}

static qboolean
S_IsSilencedMuzzleFlash(const wavinfo_t* info, const void* raw_data, const char* name)
{
	/* Skip the prefix. */
	static const size_t base_sound_string_length = strlen("sound/");
	const char* base_name = name + base_sound_string_length;

	/* Match to well-known muzzle flash sound names. */
	const qboolean is_name_matched =
		strcmp(base_name, "weapons/bfg__f1y.wav") == 0 ||
		strcmp(base_name, "weapons/blastf1a.wav") == 0 ||
		strcmp(base_name, "weapons/disint2.wav") == 0 ||
		strcmp(base_name, "weapons/grenlf1a.wav") == 0 ||
		strcmp(base_name, "weapons/grenlr1b.wav") == 0 ||
		strcmp(base_name, "weapons/hyprbf1a.wav") == 0 ||
		strcmp(base_name, "weapons/machgf1b.wav") == 0 ||
		strcmp(base_name, "weapons/machgf2b.wav") == 0 ||
		strcmp(base_name, "weapons/machgf3b.wav") == 0 ||
		strcmp(base_name, "weapons/machgf4b.wav") == 0 ||
		strcmp(base_name, "weapons/machgf5b.wav") == 0 ||
		strcmp(base_name, "weapons/nail1.wav") == 0 ||
		strcmp(base_name, "weapons/plasshot.wav") == 0 ||
		strcmp(base_name, "weapons/railgf1a.wav") == 0 ||
		strcmp(base_name, "weapons/rippfire.wav") == 0 ||
		strcmp(base_name, "weapons/rocklf1a.wav") == 0 ||
		strcmp(base_name, "weapons/rocklr1b.wav") == 0 ||
		strcmp(base_name, "weapons/shotg2.wav") == 0 ||
		strcmp(base_name, "weapons/shotgf1b.wav") == 0 ||
		strcmp(base_name, "weapons/shotgr1b.wav") == 0 ||
		strcmp(base_name, "weapons/sshotf1b.wav") == 0 ||
		false
	;

	if (!is_name_matched)
	{
		return false;
	}

	/* Now check for silence. */
	if (!S_IsShortSilence(info, raw_data))
	{
		return false;
	}

	return true;
}

/*
 * Loads one sample into memory
 */
sfxcache_t *
S_LoadSound(sfx_t *s)
{
	char namebuffer[MAX_QPATH];
	byte *data;
	wavinfo_t info;
	sfxcache_t *sc;
	int size;
	char *name;

	if (s->name[0] == '*')
	{
		return NULL;
	}

	/* see if still in memory */
	sc = s->cache;

	if (sc)
	{
		return sc;
	}

	/* load it */
	if (s->truename)
	{
		name = s->truename;
	}

	else
	{
		name = s->name;
	}

	if (name[0] == '#')
	{
		strcpy(namebuffer, &name[1]);
	}
	else
	{
		Com_sprintf(namebuffer, sizeof(namebuffer), "sound/%s", name);
	}

	size = FS_LoadFile(namebuffer, (void **)&data);

	if (!data)
	{
		s->cache = NULL;
		Com_DPrintf("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	info = GetWavinfo(s->name, data, size);

	if (info.channels < 1 || info.channels > 2)
	{
		Com_Printf("%s has an invalid number of channels\n", s->name);
		FS_FreeFile(data);
		return NULL;
	}

	if (S_IsSilencedMuzzleFlash(&info, data, namebuffer))
	{
		s->is_silenced_muzzle_flash = true;
	}

#if USE_OPENAL
	if (sound_started == SS_OAL)
	{
		sc = AL_UploadSfx(s, &info, data + info.dataofs);
	}
	else
#endif
	{
		if (sound_started == SS_SDL)
		{
			if (!SDL_Cache(s, &info, data + info.dataofs))
			{
				Com_Printf("Pansen!\n");
				FS_FreeFile(data);
				return NULL;
			}
		}
	}

	FS_FreeFile(data);
	return sc;
}

/*
 * Returns the name of a sound
 */
static sfx_t *
S_FindName(char *name, qboolean create)
{
	int i;
	sfx_t *sfx;

	if (!name)
	{
		Com_Error(ERR_FATAL, "%s: NULL\n", __func__);
	}

	if (!name[0])
	{
		Com_Error(ERR_FATAL, "%s: empty name\n", __func__);
	}

	if (strlen(name) >= MAX_QPATH)
	{
		Com_Error(ERR_FATAL, "%s :Sound name too long: %s",
			__func__, name);
	}

	/* see if already loaded */
	for (i = 0; i < num_sfx; i++)
	{
		if (!strcmp(known_sfx[i].name, name))
		{
			return &known_sfx[i];
		}
	}

	if (!create)
	{
		return NULL;
	}

	/* find a free sfx */
	for (i = 0; i < num_sfx; i++)
	{
		if (!known_sfx[i].name[0])
		{
			break;
		}
	}

	if (i == num_sfx)
	{
		if (num_sfx == MAX_SFX)
		{
			Com_Error(ERR_FATAL, "%s: out of sfx_t", __func__);
		}

		num_sfx++;
	}

	sfx = &known_sfx[i];
	sfx->truename = NULL;
	strcpy(sfx->name, name);
	sfx->registration_sequence = s_registration_sequence;
	sfx->is_silenced_muzzle_flash = false;

	return sfx;
}

/*
 * Registers an alias name
 * for a sound
 */
static sfx_t *
S_AliasName(char *aliasname, char *truename)
{
	sfx_t *sfx;
	char *s;
	int i;

	s = Z_Malloc(MAX_QPATH);
	strcpy(s, truename);

	/* find a free sfx */
	for (i = 0; i < num_sfx; i++)
	{
		if (!known_sfx[i].name[0])
		{
			break;
		}
	}

	if (i == num_sfx)
	{
		if (num_sfx == MAX_SFX)
		{
			Com_Error(ERR_FATAL, "%s: out of sfx_t", __func__);
		}

		num_sfx++;
	}

	sfx = &known_sfx[i];
	sfx->cache = NULL;
	strcpy(sfx->name, aliasname);
	sfx->registration_sequence = s_registration_sequence;
	sfx->truename = s;

	return sfx;
}

/*
 * Called before registering
 * of sound starts
 */
void
S_BeginRegistration(void)
{
	s_registration_sequence++;
	s_registering = true;
}

/*
 * Registers a sound
 */
sfx_t *
S_RegisterSound(char *name)
{
	sfx_t *sfx;

	if (sound_started == SS_NOT)
	{
		return NULL;
	}

	sfx = S_FindName(name, true);
	sfx->registration_sequence = s_registration_sequence;

	if (!s_registering)
	{
		S_LoadSound(sfx);
	}

	return sfx;
}

static struct sfx_s *
S_RegisterSexedSound(entity_state_t *ent, char *base)
{
	int n;
	struct sfx_s *sfx;
	char model[MAX_QPATH];
	char sexedFilename[MAX_QPATH];

	/* determine what model the client is using */
	model[0] = 0;
	n = CS_PLAYERSKINS + ent->number - 1;

	if (cl.configstrings[n][0])
	{
		char *p;
		p = strchr(cl.configstrings[n], '\\');

		if (p)
		{
			p += 1;
			strcpy(model, p);
			p = strchr(model, '/');

			if (p)
			{
				p[0] = 0;
			}
		}
	}

	/* if we can't figure it out, they're male */
	if (!model[0])
	{
		strcpy(model, "male");
	}

	/* see if we already know of the model specific sound */
	Com_sprintf(sexedFilename, sizeof(sexedFilename),
			"#players/%s/%s", model, base + 1);
	sfx = S_FindName(sexedFilename, false);

	if (!sfx)
	{
		int len;

		/* no, so see if it exists */
		len = FS_LoadFile(&sexedFilename[1], NULL);

		if (len != -1)
		{
			/* yes, close the file and register it */
			sfx = S_RegisterSound(sexedFilename);
		}
		else
		{
			char maleFilename[MAX_QPATH];

			/* no, revert to the male sound in the pak0.pak */
			Com_sprintf(maleFilename, sizeof(maleFilename),
					"player/male/%s", base + 1);
			sfx = S_AliasName(sexedFilename, maleFilename);
		}
	}

	return sfx;
}

/*
 * Called after registering of
 * sound has ended
 */
void
S_EndRegistration(void)
{
	int i;
	sfx_t *sfx;

	/* free any sounds not from this registration sequence */
	for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++)
	{
		if (!sfx->name[0])
		{
			continue;
		}

		if (sfx->registration_sequence != s_registration_sequence)
		{
			/* it is possible to have a leftover */
			if (sfx->cache)
			{
				Z_Free(sfx->cache); /* from a server that didn't finish loading */
			}

			if (sfx->truename)
			{
				Z_Free(sfx->truename);
			}

			sfx->cache = NULL;
			sfx->name[0] = 0;
		}
	}

	/* load everything in */
	for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++)
	{
		if (!sfx->name[0])
		{
			continue;
		}

		S_LoadSound(sfx);
	}

	s_registering = false;
}

/* ----------------------------------------------------------------- */

/*
 * Picks a free channel
 */
channel_t *
S_PickChannel(int entnum, int entchannel)
{
	int ch_idx;
	int first_to_die;
	int life_left;
	channel_t *ch;

	if (entchannel < 0)
	{
		Com_Error(ERR_DROP, "%s: entchannel<0", __func__);
	}

	/* Check for replacement sound, or find the best one to replace */
	first_to_die = -1;
	life_left = 0x7fffffff;

	for (ch_idx = 0; ch_idx < s_numchannels; ch_idx++)
	{
		/* channel 0 never overrides */
		if ((entchannel != 0) &&
			(channels[ch_idx].entnum == entnum) &&
			(channels[ch_idx].entchannel == entchannel))
		{
			/* always override sound from same entity */
			first_to_die = ch_idx;
			break;
		}

		/* don't let monster sounds override player sounds */
		if ((channels[ch_idx].entnum == cl.playernum + 1) &&
			(entnum != cl.playernum + 1) && channels[ch_idx].sfx)
		{
			continue;
		}

		if (channels[ch_idx].end - paintedtime < life_left)
		{
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
	}

	if (first_to_die == -1)
	{
		return NULL;
	}

	ch = &channels[first_to_die];

#if USE_OPENAL
	if ((sound_started == SS_OAL) && ch->sfx)
	{
		/* Make sure the channel is dead */
		AL_StopChannel(ch);
	}
#endif

	memset(ch, 0, sizeof(*ch));
	return ch;
}

/*
 * Picks a free playsound
 */
static playsound_t *
S_AllocPlaysound(void)
{
	playsound_t *ps;

	ps = s_freeplays.next;

	if (ps == &s_freeplays)
	{
		/* no free playsounds, this results
		   in stuttering an cracking */
		return NULL;
	}

	/* unlink from freelist */
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	return ps;
}

/*
 * Frees a playsound
 */
static void
S_FreePlaysound(playsound_t *ps)
{
	/* unlink from channel */
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	/* add to free list */
	ps->next = s_freeplays.next;
	s_freeplays.next->prev = ps;
	ps->prev = &s_freeplays;
	s_freeplays.next = ps;
}

/*
 * Take the next playsound and begin it on the channel
 * This is never called directly by S_Play*, but only
 * by the update loop.
 */
void
S_IssuePlaysound(playsound_t *ps)
{
	channel_t *ch;
	sfxcache_t *sc;

	if (!ps)
	{
		return;
	}

	if (s_show->value)
	{
		Com_Printf("Issue %i\n", ps->begin);
	}

	if (s_ps_sorting->value == 3 && ps->sfx->is_silenced_muzzle_flash)
	{
		S_FreePlaysound(ps);
		return;
	}

	/* pick a channel to play on */
	ch = S_PickChannel(ps->entnum, ps->entchannel);

	if (!ch)
	{
		S_FreePlaysound(ps);
		return;
	}

	sc = S_LoadSound(ps->sfx);

	if (!sc)
	{
		Com_Printf("S_IssuePlaysound: couldn't load %s\n", ps->sfx->name);
		S_FreePlaysound(ps);
		return;
	}

	/* spatialize */
	if (ps->attenuation == ATTN_STATIC)
	{
		ch->dist_mult = ps->attenuation * 0.001f;
	}

	else
	{
		ch->dist_mult = ps->attenuation * 0.0005f;
	}

	ch->entnum = ps->entnum;
	ch->entchannel = ps->entchannel;
	ch->sfx = ps->sfx;
	VectorCopy(ps->origin, ch->origin);
	ch->fixed_origin = ps->fixed_origin;

#if USE_OPENAL
	if (sound_started == SS_OAL)
	{
		/* This is clamped to 1.0 in AL_PlayChannel() */
		ch->oal_vol = ps->volume * (s_volume->value);
		AL_PlayChannel(ch);
	}
	else
#endif
	{
		if (sound_started == SS_SDL)
		{
			ch->master_vol = (int)ps->volume;
			SDL_Spatialize(ch);
		}
	}

	ch->pos = 0;
	ch->end = paintedtime + sc->length;

	/* free the playsound */
	S_FreePlaysound(ps);
}

/*
 * Validates the parms and queues the sound up.
 * If pos is NULL, the sound will be dynamically
 * sourced from the entity. Entchannel 0 will never
 * override a playing sound.
 */
void
S_StartSound(vec3_t origin, int entnum, int entchannel, sfx_t *sfx,
		float fvol, float attenuation, float timeofs)
{
	sfxcache_t *sc;
	playsound_t *ps, *sort;

	if (sound_started == SS_NOT)
	{
		return;
	}

	if (!sfx)
	{
		return;
	}

	if (sfx->name[0] == '*')
	{
		sfx = S_RegisterSexedSound(&cl_entities[entnum].current, sfx->name);

		if (!sfx)
		{
			return;
		}
	}

	/* make sure the sound is loaded */
	sc = S_LoadSound(sfx);

	if (!sc)
	{
		/* couldn't load the sound's data */
		return;
	}

	/* make the playsound_t */
	ps = S_AllocPlaysound();

	if (!ps)
	{
		return;
	}

	if (origin)
	{
		VectorCopy(origin, ps->origin);
		ps->fixed_origin = true;
	}
	else
	{
		ps->fixed_origin = false;
	}

	if (sfx->name[0])
	{
		vec3_t orientation, direction;
		vec_t distance_direction;
		int dir_x, dir_y, dir_z;

		VectorSubtract(listener_forward, listener_up, orientation);

		// with !fixed we have all sounds related directly to player,
		// e.g. players fire, pain, menu
		if (!ps->fixed_origin)
		{
			VectorCopy(orientation, direction);
			distance_direction = 0;
		}
		else
		{
			VectorSubtract(listener_origin, ps->origin, direction);
			distance_direction = VectorLength(direction);
		}

		VectorNormalize(direction);
		VectorNormalize(orientation);

		dir_x = 16 * orientation[0] * direction[0];
		dir_y = 16 * orientation[1] * direction[1];
		dir_z = 16 * orientation[2] * direction[2];

		Haptic_Feedback(sfx->name, 16 - distance_direction / 32, dir_x, dir_y, dir_z);
	}

	ps->entnum = entnum;
	ps->entchannel = entchannel;
	ps->attenuation = attenuation;
	ps->sfx = sfx;

#if USE_OPENAL
	if (sound_started == SS_OAL)
	{
		ps->begin = paintedtime + timeofs * 1000;
		ps->volume = fvol;
	}
	else
#endif
	{
		if (sound_started == SS_SDL)
		{
			ps->begin = SDL_DriftBeginofs(timeofs);
			ps->volume = fvol * 255;
		}
	}

	cvar_t *game = Cvar_Get("game",  "", CVAR_LATCH | CVAR_SERVERINFO);

	const qboolean is_mission_pack =
		(strcmp(game->string, "") == 0) ||
		(strcmp(game->string, "rogue") == 0) ||
		(strcmp(game->string, "xatrix") == 0);

	if ((s_ps_sorting->value == 1 && is_mission_pack) ||
			s_ps_sorting->value == 2 ||
			s_ps_sorting->value == 3)
	{
		for (sort = s_pendingplays.next;
				sort != &s_pendingplays && sort->begin <= ps->begin;
				sort = sort->next)
		{
		}
	}
	else if (!is_mission_pack && s_ps_sorting->value == 1)
	{
		for (sort = s_pendingplays.next;
				sort != &s_pendingplays && sort->begin < ps->begin;
				sort = sort->next)
		{
		}
	}
	else
	{
		/* No sorting. Just append at the end. */
		sort = s_pendingplays.prev;
	}

	ps->next = sort;
	ps->prev = sort->prev;

	ps->next->prev = ps;
	ps->prev->next = ps;
}

/*
 * Plays a sound when we're not
 * in a level. Used by the menu
 * system.
 */
void
S_StartLocalSound(char *sound)
{
	sfx_t *sfx;

	if (sound_started == SS_NOT)
	{
		return;
	}

	sfx = S_RegisterSound(sound);

	if (!sfx)
	{
		Com_Printf("%s: can't cache %s\n", __func__, sound);
		return;
	}

	S_StartSound(NULL, cl.playernum + 1, 0, sfx, 1, 1, 0);
}

/*
 * Stops all sounds
 */
void
S_StopAllSounds(void)
{
	int i;

	if (sound_started == SS_NOT)
	{
		return;
	}

	/* clear all the playsounds */
	memset(s_playsounds, 0, sizeof(s_playsounds));
	s_freeplays.next = s_freeplays.prev = &s_freeplays;
	s_pendingplays.next = s_pendingplays.prev = &s_pendingplays;

	for (i = 0; i < MAX_PLAYSOUNDS; i++)
	{
		s_playsounds[i].prev = &s_freeplays;
		s_playsounds[i].next = s_freeplays.next;
		s_playsounds[i].prev->next = &s_playsounds[i];
		s_playsounds[i].next->prev = &s_playsounds[i];
	}

#if USE_OPENAL
	if (sound_started == SS_OAL)
	{
		AL_StopAllChannels();
	}
	else
#endif
	{
		if (sound_started == SS_SDL)
		{
			SDL_ClearBuffer();
		}
	}

	/* clear all the channels */
	memset(channels, 0, sizeof(channels));
}

/*
 * Builds a list of all sounds
 */
void
S_BuildSoundList(int *sounds)
{
	int i;

	for (i = 0; i < cl.frame.num_entities; i++)
	{
		int num;
		entity_state_t *ent;

		if (i >= MAX_EDICTS)
		{
			break;
		}

		num = (cl.frame.parse_entities + i) & (MAX_PARSE_ENTITIES - 1);
		ent = &cl_parse_entities[num];

		if ((s_ambient->value == 2) && !ent->modelindex)
		{
			sounds[i] = 0;
		}
		else if ((s_ambient->value == 3) && (ent->number != cl.playernum + 1))
		{
			sounds[i] = 0;
		}
		else
		{
			sounds[i] = ent->sound;
		}
	}
}

/*
 * Cinematic streaming and voice over network.
 * This could be used for chat over network, but
 * that would be terrible slow.
 */
void
S_RawSamples(int samples, int rate, int width,
		int channels, byte *data, float volume)
{
	if (sound_started == SS_NOT)
	{
		return;
	}

	if (s_rawend < paintedtime)
	{
		s_rawend = paintedtime;
	}

#if USE_OPENAL
	if (sound_started == SS_OAL)
	{
		AL_RawSamples(samples, rate, width, channels, data, volume);
	}
	else
#endif
	{
		if (sound_started == SS_SDL)
		{
			SDL_RawSamples(samples, rate, width, channels, data, volume);
		}
	}
}

/*
 * Calls the update functions of the
 * backend. Those perform their
 * calculations and fill their buffers.
 */
void
S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{

	if (sound_started == SS_NOT)
	{
		return;
	}

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

#if USE_OPENAL
	if (sound_started == SS_OAL)
	{
		AL_Update();
	}
	else
#endif
	{
		if (sound_started == SS_SDL)
		{
			SDL_Update();
		}
	}
}

/*
 * Plays one sample. Called
 * by the "play" cmd.
 */
static void
S_Play(void)
{
	int i;

	i = 1;

	while (i < Cmd_Argc())
	{
		char name[256];
		sfx_t *sfx;

		if (!strrchr(Cmd_Argv(i), '.'))
		{
			Q_strlcpy(name, Cmd_Argv(i), sizeof(name) - 4);
			Q_strlcat(name, ".wav", sizeof(name));
		}
		else
		{
			Q_strlcpy(name, Cmd_Argv(i), sizeof(name));
		}

		if (strstr(name, "..") || (name[0] == '/') || (name[0] == '\\'))
		{
			Com_Printf("Bad filename %s\n", name);
			return;
		}

		sfx = S_RegisterSound(name);
		S_StartSound(NULL, cl.playernum + 1, 0, sfx, 1.0, 1.0, 0);
		i++;
	}
}

/*
 * List all loaded sounds
 */
static void
S_SoundList(void)
{
	int i;
	sfx_t *sfx;
	sfxcache_t *sc;
	int size, total;
	int numsounds;

	total = 0;
	numsounds = 0;

	for (sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++)
	{
		if (!sfx->name[0])
		{
			continue;
		}

		sc = sfx->cache;

		if (sc)
		{
			size = sc->length * sc->width * (sc->stereo + 1);
			total += size;
			Com_Printf("%s(%2db) %8i(%d ch) : %s\n",
					sc->loopstart != -1 ? "L" : " ",
					sc->width * 8, size,
					(sc->stereo + 1), sfx->name);
		}
		else
		{
			if (sfx->name[0] == '*')
			{
				Com_Printf("    placeholder : %s\n", sfx->name);
			}
			else
			{
				Com_Printf("    not loaded  : %s\n", sfx->name);
			}
		}

		numsounds++;
	}

	Com_Printf("Total resident: %i bytes (%.2f MB) in %d sounds\n", total,
			(float)total / 1024 / 1024, numsounds);
}

/* ----------------------------------------------------------------- */

/*
 * Prints information about the
 * active sound backend
 */
static void
S_SoundInfo_f(void)
{
	if (sound_started == SS_NOT)
	{
		Com_Printf("Sound system not started\n");
		return;
	}

#if USE_OPENAL
	if (sound_started == SS_OAL)
	{
		QAL_SoundInfo();
	}
	else
#endif
	{
		SDL_SoundInfo();
	}
}

/*
 * Initializes the sound system
 * and it's requested backend
 */
void
S_Init(void)
{
	cvar_t *cv;

	Com_Printf("\n------- sound initialization -------\n");

	cv = Cvar_Get("s_initsound", "1", 0);

	if (!cv->value)
	{
		Com_Printf("Not initializing.\n");
		Com_Printf("------------------------------------\n\n");
		return;
	}

	s_volume = Cvar_Get("s_volume", "0.7", CVAR_ARCHIVE);
	s_khz = Cvar_Get("s_khz", "44", CVAR_ARCHIVE);
	s_loadas8bit = Cvar_Get("s_loadas8bit", "0", CVAR_ARCHIVE);
	s_mixahead = Cvar_Get("s_mixahead", "0.14", CVAR_ARCHIVE);
	s_show = Cvar_Get("s_show", "0", 0);
	s_testsound = Cvar_Get("s_testsound", "0", 0);
	s_ambient = Cvar_Get("s_ambient", "1", 0);
	s_underwater = Cvar_Get("s_underwater", "1", CVAR_ARCHIVE);
	s_underwater_gain_hf = Cvar_Get("s_underwater_gain_hf", "0.25", CVAR_ARCHIVE);
	s_doppler = Cvar_Get("s_doppler", "0", CVAR_ARCHIVE);
	s_ps_sorting = Cvar_Get("s_ps_sorting", "1", CVAR_ARCHIVE);

	Cmd_AddCommand("play", S_Play);
	Cmd_AddCommand("stopsound", S_StopAllSounds);
	Cmd_AddCommand("soundlist", S_SoundList);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);

#if USE_OPENAL
	cv = Cvar_Get("s_openal", "1", CVAR_ARCHIVE);

	if (cv->value && AL_Init())
	{
		sound_started = SS_OAL;
	}
	else
#endif
	{
		if (SDL_BackendInit())
		{
			sound_started = SS_SDL;
		}
		else
		{
			sound_started = SS_NOT;
			return;
		}
	}

	num_sfx = 0;
	paintedtime = 0;

	OGG_Init();

	Com_Printf("Sound sampling rate: %i\n", sound.speed);
	S_StopAllSounds();

	Com_Printf("------------------------------------\n\n");
}

/*
 * Shutdown the sound system
 * and it's backend
 */
void
S_Shutdown(void)
{
	int i;
	sfx_t *sfx;

	if (sound_started == SS_NOT)
	{
		return;
	}

	S_StopAllSounds();
	OGG_Shutdown();

	/* free all sounds */
	for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++)
	{
		if (!sfx->name[0])
		{
			continue;
		}

#if USE_OPENAL
		if (sound_started == SS_OAL)
		{
			AL_DeleteSfx(sfx);
		}
#endif

		if (sfx->cache)
		{
			Z_Free(sfx->cache);
		}

		if (sfx->truename)
		{
			Z_Free(sfx->truename);
		}
	}

	memset(known_sfx, 0, sizeof(known_sfx));
	num_sfx = 0;

#if USE_OPENAL
	if (sound_started == SS_OAL)
	{
		AL_Shutdown();
	}
	else
#endif
	{
		if (sound_started == SS_SDL)
		{
			SDL_BackendShutdown();
		}
	}

	sound_started = SS_NOT;
	s_numchannels = 0;

	Cmd_RemoveCommand("soundlist");
	Cmd_RemoveCommand("soundinfo");
	Cmd_RemoveCommand("play");
	Cmd_RemoveCommand("stopsound");
}

