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

/* SDL includes */
#ifdef _WIN32
#include <SDL/SDL.h>
#elif defined(__APPLE__)
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

/* Local includes */
#include "../../client/header/client.h"
#include "../../client/sound/header/local.h"

/* Defines */
#define SDL_PAINTBUFFER_SIZE 2048
#define SDL_FULLVOLUME 80
#define SDL_LOOPATTENUATE 0.003

/* Globals */
cvar_t *s_sdldriver;
int *snd_p;
static dma_t *dmabackend;
static portable_samplepair_t paintbuffer[SDL_PAINTBUFFER_SIZE];
static int dmapos = 0;
static int dmasize = 0;
static int snd_inited = 0;
static int snd_scaletable[32][256];
static int snd_vol;

/* ------------------------------------------------------------------ */

/*
 * Transfers a mixed "paint buffer" to 
 * the SDL output buffer and places it
 * at the appropriate position.
 */
void
SDL_TransferPaintBuffer(int endtime)
{
	int i;
	int lpos;
	int ls_paintedtime;
	int out_idx;
	int count;
	int out_mask;
	int *p;
	int snd_linear_count;
	int step;
	int val;
	short *snd_out;
	unsigned char *pbuf;

	pbuf = dma.buffer;

	if (s_testsound->value)
	{
		int i;
		int count;

		/* write a fixed sine wave */
		count = (endtime - paintedtime);

		for (i = 0; i < count; i++)
		{
			paintbuffer[i].left = paintbuffer[i].right =
				(int)((float)sin((paintedtime + i) * 0.1f) * 20000 * 256);
		}
	}

	if ((dma.samplebits == 16) && (dma.channels == 2))
	{
		snd_p = (int *)paintbuffer;
		ls_paintedtime = paintedtime;

		while (ls_paintedtime < endtime)
		{
			lpos = ls_paintedtime & ((dma.samples >> 1) - 1);

			snd_out = (short *)pbuf + (lpos << 1);

			snd_linear_count = (dma.samples >> 1) - lpos;

			if (ls_paintedtime + snd_linear_count > endtime)
			{
				snd_linear_count = endtime - ls_paintedtime;
			}

			snd_linear_count <<= 1;

			for (i = 0; i < snd_linear_count; i += 2)
			{
				val = snd_p[i] >> 8;

				if (val > 0x7fff)
				{
					snd_out[i] = 0x7fff;
				}
				else if (val < -32768)
				{
					snd_out[i] = -32768;
				}
				else
				{
					snd_out[i] = val;
				}

				val = snd_p[i + 1] >> 8;

				if (val > 0x7fff)
				{
					snd_out[i + 1] = 0x7fff;
				}
				else if (val < -32768)
				{
					snd_out[i + 1] = -32768;
				}
				else
				{
					snd_out[i + 1] = val;
				}
			}

			snd_p += snd_linear_count;
			ls_paintedtime += (snd_linear_count >> 1);
		}
	}
	else
	{
		p = (int *)paintbuffer;
		count = (endtime - paintedtime) * dma.channels;
		out_mask = dma.samples - 1;
		out_idx = paintedtime * dma.channels & out_mask;
		step = 3 - dma.channels;

		if (dma.samplebits == 16)
		{
			short *out = (short *)pbuf;

			while (count--)
			{
				val = *p >> 8;
				p += step;

				if (val > 0x7fff)
				{
					val = 0x7fff;
				}

				else if (val < -32768)
				{
					val = -32768;
				}

				out[out_idx] = val;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
		else if (dma.samplebits == 8)
		{
			unsigned char *out = (unsigned char *)pbuf;

			while (count--)
			{
				val = *p >> 8;
				p += step;

				if (val > 0x7fff)
				{
					val = 0x7fff;
				}

				else if (val < -32768)
				{
					val = -32768;
				}

				out[out_idx] = (val >> 8) + 128;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
	}
}

/*
 * Mixes an 8 bit sample into a channel.
 */
void
SDL_PaintChannelFrom8(channel_t *ch, sfxcache_t *sc, int count, int offset)
{
	int data;
	int *lscale, *rscale;
	unsigned char *sfx;
	int i;
	portable_samplepair_t *samp;

	if (ch->leftvol > 255)
	{
		ch->leftvol = 255;
	}

	if (ch->rightvol > 255)
	{
		ch->rightvol = 255;
	}

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	sfx = sc->data + ch->pos;

	samp = &paintbuffer[offset];

	for (i = 0; i < count; i++, samp++)
	{
		data = sfx[i];
		samp->left += lscale[data];
		samp->right += rscale[data];
	}

	ch->pos += count;
}

/*
 * Mixes an 16 bit sample into a channel
 */
void
SDL_PaintChannelFrom16(channel_t *ch, sfxcache_t *sc, int count, int offset)
{
	int data;
	int left, right;
	int leftvol, rightvol;
	signed short *sfx;
	int i;
	portable_samplepair_t *samp;

	leftvol = ch->leftvol * snd_vol;
	rightvol = ch->rightvol * snd_vol;
	sfx = (signed short *)sc->data + ch->pos;

	samp = &paintbuffer[offset];

	for (i = 0; i < count; i++, samp++)
	{
		data = sfx[i];
		left = (data * leftvol) >> 8;
		right = (data * rightvol) >> 8;
		samp->left += left;
		samp->right += right;
	}

	ch->pos += count;
}

/*
 * Mixes all pending sounds into
 * the available output channels.
 */
void
SDL_PaintChannels(int endtime)
{
	int i;
	int end;
	channel_t *ch;
	sfxcache_t *sc;
	int ltime, count;
	playsound_t *ps;

	snd_vol = (int)(s_volume->value * 256);

	while (paintedtime < endtime)
	{
		/* if paintbuffer is smaller than DMA buffer */
		end = endtime;

		if (endtime - paintedtime > SDL_PAINTBUFFER_SIZE)
		{
			end = paintedtime + SDL_PAINTBUFFER_SIZE;
		}

		/* start any playsounds */
		for ( ; ; )
		{
			ps = s_pendingplays.next;

			if (ps == NULL)
			{
				break;
			}

			if (ps == &s_pendingplays)
			{
				break; /* no more pending sounds */
			}

			if (ps->begin <= paintedtime)
			{
				S_IssuePlaysound(ps);
				continue;
			}

			if (ps->begin < end)
			{
				end = ps->begin; /* stop here */
			}

			break;
		}

		/* clear the paint buffer */
		if (s_rawend < paintedtime)
		{
			memset(paintbuffer, 0, (end - paintedtime) 
					* sizeof(portable_samplepair_t));
		}
		else
		{
			/* copy from the streaming sound source */
			int s;
			int stop;

			stop = (end < s_rawend) ? end : s_rawend;

			for (i = paintedtime; i < stop; i++)
			{
				s = i & (MAX_RAW_SAMPLES - 1);
				paintbuffer[i - paintedtime] = s_rawsamples[s];
			}

			for ( ; i < end; i++)
			{
				memset(&paintbuffer[i - paintedtime], 0,
				   sizeof(paintbuffer[i - paintedtime]));
			}
		}

		/* paint in the channels. */
		ch = channels;

		for (i = 0; i < s_numchannels; i++, ch++)
		{
			ltime = paintedtime;

			while (ltime < end)
			{
				if (!ch->sfx || (!ch->leftvol && !ch->rightvol))
				{
					break;
				}

				/* max painting is to the end of the buffer */
				count = end - ltime;

				/* might be stopped by running out of data */
				if (ch->end - ltime < count)
				{
					count = ch->end - ltime;
				}

				sc = S_LoadSound(ch->sfx);

				if (!sc)
				{
					break;
				}

				if ((count > 0) && ch->sfx)
				{
					if (sc->width == 1)
					{
						SDL_PaintChannelFrom8(ch, sc, count, ltime - paintedtime);
					}

					else
					{
						SDL_PaintChannelFrom16(ch, sc, count, ltime - paintedtime);
					}

					ltime += count;
				}

				/* if at end of loop, restart */
				if (ltime >= ch->end)
				{
					if (ch->autosound)
					{
						/* autolooping sounds always go back to start */
						ch->pos = 0;
						ch->end = ltime + sc->length;
					}
					else if (sc->loopstart >= 0)
					{
						ch->pos = sc->loopstart;
						ch->end = ltime + sc->length - ch->pos;
					}
					else
					{
						/* channel just stopped */
						ch->sfx = NULL;
					}
				}
			}
		}

		/* transfer out according to DMA format */
		SDL_TransferPaintBuffer(end);
		paintedtime = end;
	}
}

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

/*
 * Updates the volume scale table
 * based on current volume setting.
 */
void
SDL_UpdateScaletable(void)
{
	int i, j;
	int scale;

	if (s_volume->value > 2.0f)
	{
		Cvar_Set("s_volume", "2");
	}
	else if (s_volume->value < 0)
	{
		Cvar_Set("s_volume", "0");
	}

	s_volume->modified = false;

	for (i = 0; i < 32; i++)
	{
		scale = (int)(i * 8 * 256 * s_volume->value);

		for (j = 0; j < 256; j++)
		{
			snd_scaletable[i][j] = ((j < 128) ? j : j - 0xff) * scale;
		}
	}
}

/*
 * Saves a sound sample into cache. If
 * necessary endianess convertions are
 * performed.
 */
qboolean
SDL_Cache(sfx_t *sfx, wavinfo_t *info, byte *data)
{
	float stepscale;
	int i;
	int len;
	int sample;
	int srcsample;
	sfxcache_t *sc;
	unsigned int samplefrac = 0;

	stepscale = (float)info->rate / dma.speed;
    len = (int)(info->samples / stepscale);

	if ((info->samples == 0) || (len == 0))
	{
		Com_Printf("WARNING: Zero length sound encountered: %s\n", sfx->name);
		return false;
	}

	len = len * info->width * info->channels;
	sc = sfx->cache = Z_Malloc(len + sizeof(sfxcache_t));

	if (!sc)
	{
		return false;
	}

	sc->loopstart = info->loopstart;
    sc->stereo = 0;
	sc->length = (int)(info->samples / stepscale);
	sc->speed = dma.speed;

	if ((int)(info->samples / stepscale) == 0)
	{
		Com_Printf("ResampleSfx: Invalid sound file '%s' (zero length)\n", sfx->name);
		Z_Free(sfx->cache);
		sfx->cache = NULL;
		return false;
	}

	if (sc->loopstart != -1)
	{
		sc->loopstart = (int)(sc->loopstart / stepscale);
	}

	if (s_loadas8bit->value)
	{
		sc->width = 1;
	}
	else
	{
		sc->width = info->width;
	}

	/* resample / decimate to the current source rate */
	for (i = 0; i < (int)(info->samples / stepscale); i++)
	{
		srcsample = samplefrac >> 8;
		samplefrac += (int)(stepscale * 256);

		if (info->width == 2)
		{
			sample = LittleShort(((short *)data)[srcsample]);
		}

		else
		{
			sample = (int)((unsigned char)(data[srcsample]) - 128) << 8;
		}

		if (sc->width == 2)
		{
			((short *)sc->data)[i] = sample;
		}

		else
		{
			((signed char *)sc->data)[i] = sample >> 8;
		}
	}

	return true;
}

/*
 * Playback of "raw samples", e.g. samples
 * without an origin entity. Used for music
 * and cinematic playback.
 */
void
SDL_RawSamples(int samples, int rate, int width, 
		int channels, byte *data, float volume)
{
	float scale;
	int dst;
	int i;
	int src;
	int intVolume;

	scale = (float)rate / dma.speed;
	intVolume = (int)(256 * volume);

	if ((channels == 2) && (width == 2))
	{
		for (i = 0; ; i++)
		{
			src = (int)(i * scale);

			if (src >= samples)
			{
				break;
			}

			dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left = ((short *)data)[src * 2] * intVolume;
			s_rawsamples[dst].right = ((short *)data)[src * 2 + 1] * intVolume;
		}
	}
	else if ((channels == 1) && (width == 2))
	{
		for (i = 0; ; i++)
		{
			src = (int)(i * scale);

			if (src >= samples)
			{
				break;
			}

			dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left = ((short *)data)[src] * intVolume;
			s_rawsamples[dst].right = ((short *)data)[src] * intVolume;
		}
	}
	else if ((channels == 2) && (width == 1))
	{
		intVolume *= 256;

		for (i = 0; ; i++)
		{
			src = (int)(i * scale);

			if (src >= samples)
			{
				break;
			}

			dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left =
				(((byte *)data)[src * 2] - 128) * intVolume;
			s_rawsamples[dst].right =
				(((byte *)data)[src * 2 + 1] - 128) * intVolume;
		}
	}
	else if ((channels == 1) && (width == 1))
	{
		intVolume *= 256;

		for (i = 0; ; i++)
		{
			src = (int)(i * scale);

			if (src >= samples)
			{
				break;
			}

			dst = s_rawend & (MAX_RAW_SAMPLES - 1);
			s_rawend++;
			s_rawsamples[dst].left = (((byte *)data)[src] - 128) * intVolume;
			s_rawsamples[dst].right = (((byte *)data)[src] - 128) * intVolume;
		}
	}
}

/*
 * Runs every frame, handles all necessary 
 * sound calculations and fills the play-
 * back buffer.
 */
void
SDL_Update(void)
{
	channel_t *ch;
	int i;
	int samps;
	int total;
	unsigned int endtime;

	/* if the loading plaque is up, clear everything
	   out to make sure we aren't looping a dirty
	   SDL buffer while loading */
	if (cls.disable_screen)
	{
		SDL_ClearBuffer();
		return;
	}

	/* rebuild scale tables if
	   volume is modified */
	if (s_volume->modified)
	{
		SDL_UpdateScaletable();
	}

	/* update spatialization 
	   for dynamic sounds */
	ch = channels;

	for (i = 0; i < s_numchannels; i++, ch++)
	{
		if (!ch->sfx)
		{
			continue;
		}

		if (ch->autosound)
		{
			/* autosounds are regenerated
			   fresh each frame */
			memset(ch, 0, sizeof(*ch));
			continue;
		}

		/* respatialize channel */
		SDL_Spatialize(ch);

		if (!ch->leftvol && !ch->rightvol)
		{
			memset(ch, 0, sizeof(*ch));
			continue;
		}
	}

	/* add loopsounds */
	SDL_AddLoopSounds();

	/* debugging output */
	if (s_show->value)
	{
		total = 0;
		ch = channels;

		for (i = 0; i < s_numchannels; i++, ch++)
		{
			if (ch->sfx && (ch->leftvol || ch->rightvol))
			{
				Com_Printf("%3i %3i %s\n", ch->leftvol,
						ch->rightvol, ch->sfx->name);
				total++;
			}
		}

		Com_Printf("----(%i)---- painted: %i\n", total, paintedtime);
	}

#ifdef OGG
	/* stream music */
	OGG_Stream();
#endif
  
	if (!dma.buffer)
	{
		return;
	}
  
    /* Mix the samples */
	SDL_LockAudio();

	/* Updates SDL time */
	SDL_UpdateSoundtime();

	if (!soundtime)
	{
		return;
	}

	/* check to make sure that we haven't overshot */
	if (paintedtime < soundtime)
	{
		Com_DPrintf("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

	/* mix ahead of current position */
	endtime = (int)(soundtime + s_mixahead->value * dma.speed);

	/* mix to an even submission block size */
	endtime = (endtime + dma.submission_chunk - 1) & ~(dma.submission_chunk - 1);
	samps = dma.samples >> (dma.channels - 1);

	if (endtime - soundtime > samps)
	{
		endtime = soundtime + samps;
	}

	SDL_PaintChannels(endtime);
	SDL_UnlockAudio();
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

	SDL_UpdateScaletable();
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
 
