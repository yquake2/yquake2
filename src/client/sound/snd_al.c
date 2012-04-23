/*
This Source File - except for the last few functions - see the other copyright
notice further down the - was taken from the Q2Pro Port.

Copyright (C) 2010 skuller.net
              2012 Some changes by the Yamagi Quake II developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifdef USE_OPENAL

#include "../header/client.h"
#include "header/local.h"
#include "header/qal_api.h"
#include "header/vorbis.h"

// translates from AL coordinate system to quake
#define AL_UnpackVector(v)  -v[1],v[2],-v[0]
#define AL_CopyVector(a,b)  ((b)[0]=-(a)[1],(b)[1]=(a)[2],(b)[2]=-(a)[0])

// OpenAL implementation should support at least this number of sources
#define MIN_CHANNELS 16

qboolean streamPlaying;
int active_buffers;

static ALuint streamSource;

static ALuint s_srcnums[MAX_CHANNELS-1];
static int s_framecount;

// Forward Declarations
static void S_AL_StreamUpdate( void );
static void S_AL_StreamDie( void );
// /Forward Declarations

void AL_SoundInfo( void ) {
    Com_Printf( "AL_VENDOR: %s\n", qalGetString( AL_VENDOR ) );
    Com_Printf( "AL_RENDERER: %s\n", qalGetString( AL_RENDERER ) );
    Com_Printf( "AL_VERSION: %s\n", qalGetString( AL_VERSION ) );
    Com_Printf( "AL_EXTENSIONS: %s\n", qalGetString( AL_EXTENSIONS ) );
    Com_Printf( "Number of sources: %d\n", s_numchannels );
}

static void AL_InitStreamSource() {
	qalSourcei (streamSource, AL_BUFFER,          0            );
	qalSourcei (streamSource, AL_LOOPING,         AL_FALSE     );
	qalSource3f(streamSource, AL_POSITION,        0.0, 0.0, 0.0);
	qalSource3f(streamSource, AL_VELOCITY,        0.0, 0.0, 0.0);
	qalSource3f(streamSource, AL_DIRECTION,       0.0, 0.0, 0.0);
	qalSourcef (streamSource, AL_ROLLOFF_FACTOR,  0.0          );
	qalSourcei (streamSource, AL_SOURCE_RELATIVE, AL_TRUE      );

	// srcList[cursrc].scaleGain = 0.0f; FIXME - something like that?
}

qboolean AL_Init( void ) {
    int i;

    if( !QAL_Init() ) {
        Com_Printf( "ERROR: OpenAL failed to initialize.\n" );
        return false;
    }

    // check for linear distance extension
    if( !qalIsExtensionPresent( "AL_EXT_LINEAR_DISTANCE" ) ) {
        Com_Printf( "ERROR: Required AL_EXT_LINEAR_DISTANCE extension is missing.\n" );
        goto fail;
    }

    // generate source names
    qalGetError();
    qalGenSources( 1, &streamSource );
    if( qalGetError() != AL_NO_ERROR )
    {
    	Com_Printf( "ERROR: Couldn't get a single Source.\n" );
		goto fail;

    } else {
    	// -1 because we already got one channel for streaming
    	for( i = 0; i < MAX_CHANNELS - 1; i++ ) {
			qalGenSources( 1, &s_srcnums[i] );
			if( qalGetError() != AL_NO_ERROR ) {
				break;
			}
		}

		if( i < MIN_CHANNELS - 1 ) {
			Com_Printf( "ERROR: Required at least %d sources, but got %d.\n", MIN_CHANNELS, i+1 );
			goto fail;
		}
    }

    s_numchannels = i;
    AL_InitStreamSource();

	Com_Printf("\nOpenAL setting:\n");
	AL_SoundInfo();
    return true;

fail:
    QAL_Shutdown();
    return false;
}

void AL_Shutdown( void ) {
    Com_Printf( "Shutting down OpenAL.\n" );

	qalDeleteSources(1, &streamSource);

    if( s_numchannels ) {
        // delete source names
        qalDeleteSources( s_numchannels, s_srcnums );
        memset( s_srcnums, 0, sizeof( s_srcnums ) );
        s_numchannels = 0;
    }
    S_AL_StreamDie();
    QAL_Shutdown();
}

sfxcache_t *AL_UploadSfx( sfx_t *s, wavinfo_t *s_info, byte *data ) {
    sfxcache_t *sc;
    ALsizei size = s_info->samples * s_info->width;
    ALenum format = s_info->width == 2 ? AL_FORMAT_MONO16 : AL_FORMAT_MONO8;
    ALuint name;

    if( !size ) {
        // s->error = Q_ERR_TOO_FEW; FIXME: do I want this information?
        return NULL;
    }

    qalGetError();
    qalGenBuffers( 1, &name );
    qalBufferData( name, format, data, size, s_info->rate );
	active_buffers++;
    if( qalGetError() != AL_NO_ERROR ) {
        // s->error = Q_ERR_LIBRARY_ERROR; FIXME: do I want this info?
        return NULL;
    }

    // allocate placeholder sfxcache
    sc = s->cache = Z_TagMalloc(sizeof(*sc), 0); // FIXME: TAG_SOUND instead of 0 - this possibly leaks!
    sc->length = s_info->samples * 1000 / s_info->rate; // in msec
    sc->loopstart = s_info->loopstart;
    sc->width = s_info->width;
    sc->size = size;
    sc->bufnum = name;

    return sc;
}

void AL_DeleteSfx( sfx_t *s ) {
    sfxcache_t *sc;
    ALuint name;

    sc = s->cache;
    if( !sc ) {
        return;
    }

    name = sc->bufnum;
    qalDeleteBuffers( 1, &name );
	active_buffers--;
}

void AL_StopChannel( channel_t *ch ) {
#ifdef _DEBUG
    if (s_show->integer > 1)
        Com_Printf("%s: %s\n", __func__, ch->sfx->name );
#endif

    // stop it
    qalSourceStop( ch->srcnum );
    qalSourcei( ch->srcnum, AL_BUFFER, AL_NONE );
    memset (ch, 0, sizeof(*ch));
}

static void AL_Spatialize( channel_t *ch ) {
    vec3_t      origin;

    // anything coming from the view entity will always be full volume
    // no attenuation = no spatialization
    if( ch->entnum == -1 || ch->entnum == cl.playernum + 1 || !ch->dist_mult ) {
        VectorCopy( listener_origin, origin );
    } else if( ch->fixed_origin ) {
        VectorCopy( ch->origin, origin );
    } else {
        CL_GetEntitySoundOrigin( ch->entnum, origin );
    }

    qalSource3f( ch->srcnum, AL_POSITION, AL_UnpackVector( origin ) );
}

void AL_PlayChannel( channel_t *ch ) {
    sfxcache_t *sc = ch->sfx->cache;

#ifdef _DEBUG
    if (s_show->integer > 1)
        Com_Printf("%s: %s\n", __func__, ch->sfx->name );
#endif

    ch->srcnum = s_srcnums[ch - channels];
    qalGetError();
    qalSourcei( ch->srcnum, AL_BUFFER, sc->bufnum );
    //qalSourcei( ch->srcnum, AL_LOOPING, sc->loopstart == -1 ? AL_FALSE : AL_TRUE );
    qalSourcei( ch->srcnum, AL_LOOPING, ch->autosound ? AL_TRUE : AL_FALSE );
    qalSourcef( ch->srcnum, AL_GAIN, ch->master_vol );
    qalSourcef( ch->srcnum, AL_REFERENCE_DISTANCE, SOUND_FULLVOLUME );
    qalSourcef( ch->srcnum, AL_MAX_DISTANCE, 8192 );
    qalSourcef( ch->srcnum, AL_ROLLOFF_FACTOR, ch->dist_mult * ( 8192 - SOUND_FULLVOLUME ) );

    AL_Spatialize( ch );

    // play it
    qalSourcePlay( ch->srcnum );
    if( qalGetError() != AL_NO_ERROR ) {
        AL_StopChannel( ch );
    }
}

void AL_StopAllChannels( void ) {
    int         i;
    channel_t   *ch;

    ch = channels;
    for( i = 0; i < s_numchannels; i++, ch++ ) {
        if (!ch->sfx)
            continue;
        AL_StopChannel( ch );
    }
    s_rawend = 0;
    S_AL_StreamDie();
}

static channel_t *AL_FindLoopingSound( int entnum, sfx_t *sfx ) {
    int         i;
    channel_t   *ch;

    ch = channels;
    for( i = 0; i < s_numchannels; i++, ch++ ) {
        if( !ch->sfx )
            continue;
        if( !ch->autosound )
            continue;
        if( ch->entnum != entnum )
            continue;
        if( ch->sfx != sfx )
            continue;
        return ch;
    }

    return NULL;
}

static void AL_AddLoopSounds( void ) {
    int         i;
    int         sounds[64]; // 64 is MAX_PACKET_ENTITIES in YQ2 (there's no define for it, though :/)
    channel_t   *ch;
    sfx_t       *sfx;
    sfxcache_t  *sc;
    int         num;
    entity_state_t  *ent;

    if( cls.state != ca_active || !s_ambient->value ) { // FIXME: sv_paused->value ||
        return;
    }

    S_BuildSoundList( sounds );

    for( i = 0; i < cl.frame.num_entities; i++ ) {
        if (!sounds[i])
            continue;

        sfx = cl.sound_precache[sounds[i]];
        if (!sfx)
            continue;       // bad sound effect
        sc = sfx->cache;
        if (!sc)
            continue;

        num = ( cl.frame.parse_entities + i ) & ( MAX_PARSE_ENTITIES - 1 );
		ent = &cl_parse_entities [ num ];

        ch = AL_FindLoopingSound( ent->number, sfx );
        if( ch ) {
            ch->autoframe = s_framecount;
            ch->end = paintedtime + sc->length;
            continue;
        }

        // allocate a channel
        ch = S_PickChannel(0, 0);
        if (!ch)
            continue;

        ch->autosound = true;  // remove next frame
        ch->autoframe = s_framecount;
        ch->sfx = sfx;
        ch->entnum = ent->number;
        ch->master_vol = 1;
        ch->dist_mult = SOUND_LOOPATTENUATE;
        ch->end = paintedtime + sc->length;

        AL_PlayChannel( ch );
    }
}

static void AL_IssuePlaysounds( void ) {
    playsound_t *ps;

    // start any playsounds
    while (1) {
        ps = s_pendingplays.next;
        if (ps == &s_pendingplays)
            break;  // no more pending sounds
        if (ps->begin > paintedtime)
            break;
        S_IssuePlaysound (ps);
    }
}

void AL_Update( void ) {
    int         i;
    channel_t   *ch;
    vec_t       orientation[6];

    /* FIXME
    if( !s_active ) {
        return;
    }
    */

    paintedtime = cl.time;

    // set listener parameters
    qalListener3f( AL_POSITION, AL_UnpackVector( listener_origin ) );
    AL_CopyVector( listener_forward, orientation );
    AL_CopyVector( listener_up, orientation + 3 );
    qalListenerfv( AL_ORIENTATION, orientation );
    qalListenerf( AL_GAIN, s_volume->value );
    qalDistanceModel( AL_LINEAR_DISTANCE_CLAMPED );

    // update spatialization for dynamic sounds
    ch = channels;
    for( i = 0; i < s_numchannels; i++, ch++ ) {
        if( !ch->sfx )
            continue;

        if( ch->autosound ) {
            // autosounds are regenerated fresh each frame
            if( ch->autoframe != s_framecount ) {
                AL_StopChannel( ch );
                continue;
            }
        } else {
            ALenum state;

            qalGetError();
            qalGetSourcei( ch->srcnum, AL_SOURCE_STATE, &state );
            if( qalGetError() != AL_NO_ERROR || state == AL_STOPPED ) {
                AL_StopChannel( ch );
                continue;
            }
        }

#ifdef _DEBUG
        if (s_show->integer) {
            Com_Printf ("%.1f %s\n", ch->master_vol, ch->sfx->name);
        //    total++;
        }
#endif

        AL_Spatialize(ch);         // respatialize channel
    }

    s_framecount++;

    // add loopsounds
    AL_AddLoopSounds ();

    // add music
    OGG_Stream();
    S_AL_StreamUpdate();

    AL_IssuePlaysounds();
}

/*
===========================================================================
* The remaining functions in this file are from zeq2 who seem to have got
* them from ioquake3. Adapted for Yamagi Quake II (e.g. we only need one
* non-spatialized stream)
*
* They came with the following copyright notice (modified to make clear
* that only the rest of this file is affected):

Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)
              2012 Some changes by the Yamagi Quake II developers

The rest of this file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
 */

static void S_AL_StreamDie( void )
{
	int		numBuffers;


	streamPlaying = false;
	qalSourceStop(streamSource);

	// Un-queue any buffers, and delete them
	qalGetSourcei( streamSource, AL_BUFFERS_PROCESSED, &numBuffers );
	while( numBuffers-- )
	{
		ALuint buffer;
		qalSourceUnqueueBuffers(streamSource, 1, &buffer);
		qalDeleteBuffers(1, &buffer);
		active_buffers--;
	}

	// S_AL_FreeStreamChannel(stream);
}

static void S_AL_StreamUpdate( void )
{
	int		numBuffers;
	ALint	state;

	// Un-queue any buffers, and delete them
	qalGetSourcei( streamSource, AL_BUFFERS_PROCESSED, &numBuffers );
	while( numBuffers-- )
	{
		ALuint buffer;
		qalSourceUnqueueBuffers(streamSource, 1, &buffer);
		qalDeleteBuffers(1, &buffer);
		active_buffers--;
	}

	// Start the streamSource playing if necessary
	qalGetSourcei( streamSource, AL_BUFFERS_QUEUED, &numBuffers );

	qalGetSourcei(streamSource, AL_SOURCE_STATE, &state);
	if(state == AL_STOPPED)
	{
		streamPlaying = false;

		// If there are no buffers queued up, release the streamSource
		//if( !numBuffers )
		//	S_AL_FreeStreamChannel( stream );
	}

	if( !streamPlaying && numBuffers )
	{
		qalSourcePlay( streamSource );
		streamPlaying = true;
	}
}

static ALuint S_AL_Format(int width, int channels)
{
	ALuint format = AL_FORMAT_MONO16;

	// Work out format
	if(width == 1)
	{
		if(channels == 1)
			format = AL_FORMAT_MONO8;
		else if(channels == 2)
			format = AL_FORMAT_STEREO8;
	}
	else if(width == 2)
	{
		if(channels == 1)
			format = AL_FORMAT_MONO16;
		else if(channels == 2)
			format = AL_FORMAT_STEREO16;
	}

	return format;
}

void AL_RawSamples( int samples, int rate, int width, int channels, byte *data, float volume )
{
	ALuint buffer;
	ALuint format;

	format = S_AL_Format( width, channels );

	// Create a buffer, and stuff the data into it
	qalGenBuffers(1, &buffer);
	qalBufferData(buffer, format, (ALvoid *)data, (samples * width * channels), rate);
	active_buffers++;

	// set volume
	qalSourcef( streamSource, AL_GAIN, volume );

	// Shove the data onto the streamSource
	qalSourceQueueBuffers(streamSource, 1, &buffer);

	// emulate behavior of S_RawSamples for s_rawend
	s_rawend += samples;
	/*
	// get this buffers speed/frequency
	ALint speed = 0;
	qalGetBufferi(buffer, AL_FREQUENCY, &speed);
	// FIXME: the buffers are leaking I think
	float scale = (float) rate / speed;

	if( samples > 0 && scale >= 0.0f ) {
		s_rawend += samples/scale;
	}
	*/
}

#endif // USE_OPENAL
