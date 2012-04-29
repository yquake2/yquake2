/*
 * Copyright (C) 2012 Yamagi Burmeister
 * Copyright (C) 2010 skuller.net
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
 * Low level, platform depended "qal" API implementation. This files
 * provides functions to load, initialize, shutdown und unload the
 * OpenAL library and connects the "qal" funtion pointers to the
 * OpenAL functions. This source file was taken from Q2Pro and
 * modified by the YQ2 authors.
 *
 * =======================================================================
 */

#ifdef USE_OPENAL

#include <dlfcn.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include "../common/header/common.h"
#include "header/qal.h"

static ALCcontext *context;
static ALCdevice *device;
static cvar_t *al_device;
static cvar_t *al_driver;
static void *handle;

/* Function pointers for OpenAL management */
static LPALCCREATECONTEXT qalcCreateContext;
static LPALCMAKECONTEXTCURRENT qalcMakeContextCurrent;
static LPALCPROCESSCONTEXT qalcProcessContext;
static LPALCSUSPENDCONTEXT qalcSuspendContext;
static LPALCDESTROYCONTEXT qalcDestroyContext;
static LPALCGETCURRENTCONTEXT qalcGetCurrentContext;
static LPALCGETCONTEXTSDEVICE qalcGetContextsDevice;
static LPALCOPENDEVICE qalcOpenDevice;
static LPALCCLOSEDEVICE qalcCloseDevice;
static LPALCGETERROR qalcGetError;
static LPALCISEXTENSIONPRESENT qalcIsExtensionPresent;
static LPALCGETPROCADDRESS qalcGetProcAddress;
static LPALCGETENUMVALUE qalcGetEnumValue;
static LPALCGETSTRING qalcGetString;
static LPALCGETINTEGERV qalcGetIntegerv;
static LPALCCAPTUREOPENDEVICE qalcCaptureOpenDevice;
static LPALCCAPTURECLOSEDEVICE qalcCaptureCloseDevice;
static LPALCCAPTURESTART qalcCaptureStart;
static LPALCCAPTURESTOP qalcCaptureStop;
static LPALCCAPTURESAMPLES qalcCaptureSamples ;

/* Declaration of function pointers used
   to connect OpenAL to our internal API */
LPALENABLE qalEnable;
LPALDISABLE qalDisable;
LPALISENABLED qalIsEnabled;
LPALGETSTRING qalGetString;
LPALGETBOOLEANV qalGetBooleanv;
LPALGETINTEGERV qalGetIntegerv;
LPALGETFLOATV qalGetFloatv;
LPALGETDOUBLEV qalGetDoublev;
LPALGETBOOLEAN qalGetBoolean;
LPALGETINTEGER qalGetInteger;
LPALGETFLOAT qalGetFloat;
LPALGETDOUBLE qalGetDouble;
LPALGETERROR qalGetError;
LPALISEXTENSIONPRESENT qalIsExtensionPresent;
LPALGETPROCADDRESS qalGetProcAddress;
LPALGETENUMVALUE qalGetEnumValue;
LPALLISTENERF qalListenerf;
LPALLISTENER3F qalListener3f;
LPALLISTENERFV qalListenerfv;
LPALLISTENERI qalListeneri;
LPALLISTENER3I qalListener3i;
LPALLISTENERIV qalListeneriv;
LPALGETLISTENERF qalGetListenerf;
LPALGETLISTENER3F qalGetListener3f;
LPALGETLISTENERFV qalGetListenerfv;
LPALGETLISTENERI qalGetListeneri;
LPALGETLISTENER3I qalGetListener3i;
LPALGETLISTENERIV qalGetListeneriv;
LPALGENSOURCES qalGenSources;
LPALDELETESOURCES qalDeleteSources;
LPALISSOURCE qalIsSource;
LPALSOURCEF qalSourcef;
LPALSOURCE3F qalSource3f;
LPALSOURCEFV qalSourcefv;
LPALSOURCEI qalSourcei;
LPALSOURCE3I qalSource3i;
LPALSOURCEIV qalSourceiv;
LPALGETSOURCEF qalGetSourcef;
LPALGETSOURCE3F qalGetSource3f;
LPALGETSOURCEFV qalGetSourcefv;
LPALGETSOURCEI qalGetSourcei;
LPALGETSOURCE3I qalGetSource3i;
LPALGETSOURCEIV qalGetSourceiv;
LPALSOURCEPLAYV qalSourcePlayv;
LPALSOURCESTOPV qalSourceStopv;
LPALSOURCEREWINDV qalSourceRewindv;
LPALSOURCEPAUSEV qalSourcePausev;
LPALSOURCEPLAY qalSourcePlay;
LPALSOURCESTOP qalSourceStop;
LPALSOURCEREWIND qalSourceRewind;
LPALSOURCEPAUSE qalSourcePause;
LPALSOURCEQUEUEBUFFERS qalSourceQueueBuffers;
LPALSOURCEUNQUEUEBUFFERS qalSourceUnqueueBuffers;
LPALGENBUFFERS qalGenBuffers;
LPALDELETEBUFFERS qalDeleteBuffers;
LPALISBUFFER qalIsBuffer;
LPALBUFFERDATA qalBufferData;
LPALBUFFERF qalBufferf;
LPALBUFFER3F qalBuffer3f;
LPALBUFFERFV qalBufferfv;
LPALBUFFERI qalBufferi;
LPALBUFFER3I qalBuffer3i;
LPALBUFFERIV qalBufferiv;
LPALGETBUFFERF qalGetBufferf;
LPALGETBUFFER3F qalGetBuffer3f;
LPALGETBUFFERFV qalGetBufferfv;
LPALGETBUFFERI qalGetBufferi;
LPALGETBUFFER3I qalGetBuffer3i;
LPALGETBUFFERIV qalGetBufferiv;
LPALDOPPLERFACTOR qalDopplerFactor;
LPALDOPPLERVELOCITY qalDopplerVelocity;
LPALSPEEDOFSOUND qalSpeedOfSound;
LPALDISTANCEMODEL qalDistanceModel;
LPALGENFILTERS qalGenFilters;
LPALFILTERI qalFilteri;
LPALFILTERF qalFilterf;
LPALDELETEFILTERS qalDeleteFilters;

/*
 * Gives information over the OpenAL
 * implementation and it's state
 */
void QAL_SoundInfo()
{
	Com_Printf("OpenAL settings:\n");
    Com_Printf("AL_VENDOR: %s\n", qalGetString(AL_VENDOR));
    Com_Printf("AL_RENDERER: %s\n", qalGetString(AL_RENDERER));
    Com_Printf("AL_VERSION: %s\n", qalGetString(AL_VERSION));
    Com_Printf("AL_EXTENSIONS: %s\n", qalGetString(AL_EXTENSIONS));

	if (qalcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT"))
	{
		const char *devs = qalcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);

		Com_Printf("\nAvailable OpenAL devices:\n");

		if (devs == NULL)
		{
			Com_Printf("- No devices found. Depending on your\n");
			Com_Printf("  platform this may be expected and\n");
			Com_Printf("  doesn't indicate a problem!\n");
		}
		else
		{
			while (devs && *devs)
			{
				Com_Printf("- %s\n", devs);
				devs += strlen(devs) + 1;
			}
		}
	}

   	if (qalcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT"))
	{
		const char *devs = qalcGetString(device, ALC_DEVICE_SPECIFIER);

		Com_Printf("\nCurrent OpenAL device:\n");

		if (devs == NULL)
		{
			Com_Printf("- No OpenAL device in use\n");
		}
		else
		{
			Com_Printf("- %s\n", devs);
		}
	}
}

/*
 * Shuts OpenAL down, frees all context and
 * device handles and unloads the shared lib.
 */
void
QAL_Shutdown()
{
    if (context)
   	{
        qalcMakeContextCurrent( NULL );
        qalcDestroyContext( context );
        context = NULL;
    }

	if (device)
	{
        qalcCloseDevice( device );
        device = NULL;
    }

	/* Disconnect function pointers used
	   for OpenAL management calls */
	qalcCreateContext = NULL;
	qalcMakeContextCurrent = NULL;
	qalcProcessContext = NULL;
	qalcSuspendContext = NULL;
	qalcDestroyContext = NULL;
	qalcGetCurrentContext = NULL;
	qalcGetContextsDevice = NULL;
	qalcOpenDevice = NULL;
	qalcCloseDevice = NULL;
	qalcGetError = NULL;
	qalcIsExtensionPresent = NULL;
	qalcGetProcAddress = NULL;
	qalcGetEnumValue = NULL;
	qalcGetString = NULL;
	qalcGetIntegerv = NULL;
	qalcCaptureOpenDevice = NULL;
	qalcCaptureCloseDevice = NULL;
	qalcCaptureStart = NULL;
	qalcCaptureStop = NULL;
	qalcCaptureSamples = NULL;

	/* Disconnect OpenAL
	 * function pointers */
	qalEnable = NULL;
	qalDisable = NULL;
	qalIsEnabled = NULL;
	qalGetString = NULL;
	qalGetBooleanv = NULL;
	qalGetIntegerv = NULL;
	qalGetFloatv = NULL;
	qalGetDoublev = NULL;
	qalGetBoolean = NULL;
	qalGetInteger = NULL;
	qalGetFloat = NULL;
	qalGetDouble = NULL;
	qalGetError = NULL;
	qalIsExtensionPresent = NULL;
	qalGetProcAddress = NULL;
	qalGetEnumValue = NULL;
	qalListenerf = NULL;
	qalListener3f = NULL;
	qalListenerfv = NULL;
	qalListeneri = NULL;
	qalListener3i = NULL;
	qalListeneriv = NULL;
	qalGetListenerf = NULL;
	qalGetListener3f = NULL;
	qalGetListenerfv = NULL;
	qalGetListeneri = NULL;
	qalGetListener3i = NULL;
	qalGetListeneriv = NULL;
	qalGenSources = NULL;
	qalDeleteSources = NULL;
	qalIsSource = NULL;
	qalSourcef = NULL;
	qalSource3f = NULL;
	qalSourcefv = NULL;
	qalSourcei = NULL;
	qalSource3i = NULL;
	qalSourceiv = NULL;
	qalGetSourcef = NULL;
	qalGetSource3f = NULL;
	qalGetSourcefv = NULL;
	qalGetSourcei = NULL;
	qalGetSource3i = NULL;
	qalGetSourceiv = NULL;
	qalSourcePlayv = NULL;
	qalSourceStopv = NULL;
	qalSourceRewindv = NULL;
	qalSourcePausev = NULL;
	qalSourcePlay = NULL;
	qalSourceStop = NULL;
	qalSourceRewind = NULL;
	qalSourcePause = NULL;
	qalSourceQueueBuffers = NULL;
	qalSourceUnqueueBuffers = NULL;
	qalGenBuffers = NULL;
	qalDeleteBuffers = NULL;
	qalIsBuffer = NULL;
	qalBufferData = NULL;
	qalBufferf = NULL;
	qalBuffer3f = NULL;
	qalBufferfv = NULL;
	qalBufferi = NULL;
	qalBuffer3i = NULL;
	qalBufferiv = NULL;
	qalGetBufferf = NULL;
	qalGetBuffer3f = NULL;
	qalGetBufferfv = NULL;
	qalGetBufferi = NULL;
	qalGetBuffer3i = NULL;
	qalGetBufferiv = NULL;
	qalDopplerFactor = NULL;
	qalDopplerVelocity = NULL;
	qalSpeedOfSound = NULL;
	qalDistanceModel = NULL;
	qalGenFilters = NULL;
	qalFilteri = NULL;
	qalFilterf = NULL;
	qalDeleteFilters = NULL;

	/* Unload the shared lib */
	dlclose(handle);
    handle = NULL;
}

/*
 * Loads the OpenAL shared lib, creates
 * a context and device handle.
 */
qboolean
QAL_Init()
{
	/* DEFAULT_OPENAL_DRIVER is defined at compile time via the compiler */
	al_driver = Cvar_Get( "al_driver", DEFAULT_OPENAL_DRIVER, CVAR_ARCHIVE );
	al_device = Cvar_Get( "al_device", "", CVAR_ARCHIVE );

	Com_Printf("LoadLibrary(%s)\n", al_driver->string);

	/* Load the library */
	handle = dlopen( al_driver->string, RTLD_LAZY );

	if (!handle)
	{
		Com_Printf("Loading %s failed! Disabling OpenAL.\n", al_driver->string);
		return false;
	}

	/* Connect function pointers to management functions */
	qalcCreateContext = dlsym(handle, "alcCreateContext");
	qalcMakeContextCurrent = dlsym(handle, "alcMakeContextCurrent");
	qalcProcessContext = dlsym(handle, "alcProcessContext");
	qalcSuspendContext = dlsym(handle, "alcSuspendContext");
	qalcDestroyContext = dlsym(handle, "alcDestroyContext");
	qalcGetCurrentContext = dlsym(handle, "alcGetCurrentContext");
	qalcGetContextsDevice = dlsym(handle, "alcGetContextsDevice");
	qalcOpenDevice = dlsym(handle, "alcOpenDevice");
	qalcCloseDevice = dlsym(handle, "alcCloseDevice");
	qalcGetError = dlsym(handle, "alcGetError");
	qalcIsExtensionPresent = dlsym(handle, "alcIsExtensionPresent");
	qalcGetProcAddress = dlsym(handle, "alcGetProcAddress");
	qalcGetEnumValue = dlsym(handle, "alcGetEnumValue");
	qalcGetString = dlsym(handle, "alcGetString");
	qalcGetIntegerv = dlsym(handle, "alcGetIntegerv");
	qalcCaptureOpenDevice = dlsym(handle, "alcCaptureOpenDevice");
	qalcCaptureCloseDevice = dlsym(handle, "alcCaptureCloseDevice");
	qalcCaptureStart = dlsym(handle, "alcCaptureStart");
	qalcCaptureStop = dlsym(handle, "alcCaptureStop");
	qalcCaptureSamples = dlsym(handle, "alcCaptureSamples");

	/* Connect function pointers to
	   to OpenAL API functions */
	qalEnable = dlsym(handle, "alEnable");
	qalDisable = dlsym(handle, "alDisable");
	qalIsEnabled = dlsym(handle, "alIsEnabled");
	qalGetString = dlsym(handle, "alGetString");
	qalGetBooleanv = dlsym(handle, "alGetBooleanv");
	qalGetIntegerv = dlsym(handle, "alGetIntegerv");
	qalGetFloatv = dlsym(handle, "alGetFloatv");
	qalGetDoublev = dlsym(handle, "alGetDoublev");
	qalGetBoolean = dlsym(handle, "alGetBoolean");
	qalGetInteger = dlsym(handle, "alGetInteger");
	qalGetFloat = dlsym(handle, "alGetFloat");
	qalGetDouble = dlsym(handle, "alGetDouble");
	qalGetError = dlsym(handle, "alGetError");
	qalIsExtensionPresent = dlsym(handle, "alIsExtensionPresent");
	qalGetProcAddress = dlsym(handle, "alGetProcAddress");
	qalGetEnumValue = dlsym(handle, "alGetEnumValue");
	qalListenerf = dlsym(handle, "alListenerf");
	qalListener3f = dlsym(handle, "alListener3f");
	qalListenerfv = dlsym(handle, "alListenerfv");
	qalListeneri = dlsym(handle, "alListeneri");
	qalListener3i = dlsym(handle, "alListener3i");
	qalListeneriv = dlsym(handle, "alListeneriv");
	qalGetListenerf = dlsym(handle, "alGetListenerf");
	qalGetListener3f = dlsym(handle, "alGetListener3f");
	qalGetListenerfv = dlsym(handle, "alGetListenerfv");
	qalGetListeneri = dlsym(handle, "alGetListeneri");
	qalGetListener3i = dlsym(handle, "alGetListener3i");
	qalGetListeneriv = dlsym(handle, "alGetListeneriv");
	qalGenSources = dlsym(handle, "alGenSources");
	qalDeleteSources = dlsym(handle, "alDeleteSources");
	qalIsSource = dlsym(handle, "alIsSource");
	qalSourcef = dlsym(handle, "alSourcef");
	qalSource3f = dlsym(handle, "alSource3f");
	qalSourcefv = dlsym(handle, "alSourcefv");
	qalSourcei = dlsym(handle, "alSourcei");
	qalSource3i = dlsym(handle, "alSource3i");
	qalSourceiv = dlsym(handle, "alSourceiv");
	qalGetSourcef = dlsym(handle, "alGetSourcef");
	qalGetSource3f = dlsym(handle, "alGetSource3f");
	qalGetSourcefv = dlsym(handle, "alGetSourcefv");
	qalGetSourcei = dlsym(handle, "alGetSourcei");
	qalGetSource3i = dlsym(handle, "alGetSource3i");
	qalGetSourceiv = dlsym(handle, "alGetSourceiv");
	qalSourcePlayv = dlsym(handle, "alSourcePlayv");
	qalSourceStopv = dlsym(handle, "alSourceStopv");
	qalSourceRewindv = dlsym(handle, "alSourceRewindv");
	qalSourcePausev = dlsym(handle, "alSourcePausev");
	qalSourcePlay = dlsym(handle, "alSourcePlay");
	qalSourceStop = dlsym(handle, "alSourceStop");
	qalSourceRewind = dlsym(handle, "alSourceRewind");
	qalSourcePause = dlsym(handle, "alSourcePause");
	qalSourceQueueBuffers = dlsym(handle, "alSourceQueueBuffers");
	qalSourceUnqueueBuffers = dlsym(handle, "alSourceUnqueueBuffers");
	qalGenBuffers = dlsym(handle, "alGenBuffers");
	qalDeleteBuffers = dlsym(handle, "alDeleteBuffers");
	qalIsBuffer = dlsym(handle, "alIsBuffer");
	qalBufferData = dlsym(handle, "alBufferData");
	qalBufferf = dlsym(handle, "alBufferf");
	qalBuffer3f = dlsym(handle, "alBuffer3f");
	qalBufferfv = dlsym(handle, "alBufferfv");
	qalBufferi = dlsym(handle, "alBufferi");
	qalBuffer3i = dlsym(handle, "alBuffer3i");
	qalBufferiv = dlsym(handle, "alBufferiv");
	qalGetBufferf = dlsym(handle, "alGetBufferf");
	qalGetBuffer3f = dlsym(handle, "alGetBuffer3f");
	qalGetBufferfv = dlsym(handle, "alGetBufferfv");
	qalGetBufferi = dlsym(handle, "alGetBufferi");
	qalGetBuffer3i = dlsym(handle, "alGetBuffer3i");
	qalGetBufferiv = dlsym(handle, "alGetBufferiv");
	qalDopplerFactor = dlsym(handle, "alDopplerFactor");
	qalDopplerVelocity = dlsym(handle, "alDopplerVelocity");
	qalSpeedOfSound = dlsym(handle, "alSpeedOfSound");
	qalDistanceModel = dlsym(handle, "alDistanceModel");
	qalGenFilters = dlsym(handle, "alGenFilters");
	qalFilteri = dlsym(handle, "alFilteri");
	qalFilterf = dlsym(handle, "alFilterf");
	qalDeleteFilters = dlsym(handle, "alDeleteFilters");

	/* Open the OpenAL device */
    Com_Printf("...opening OpenAL device:");

	device = qalcOpenDevice(al_device->string[0] ? al_device->string : NULL);

	if(!device)
	{
		Com_DPrintf("failed\n");
		QAL_Shutdown();
		return false;
	}

	Com_Printf("ok\n");

	/* Create the OpenAL context */
	Com_Printf("...creating OpenAL context: ");

	context = qalcCreateContext(device, NULL);

	if(!context)
	{
		Com_DPrintf("failed\n");
		QAL_Shutdown();
		return false;
	}

	Com_Printf("ok\n");

	/* Set the created context as current context */
	Com_Printf("...making context current: ");

	if (!qalcMakeContextCurrent(context))
	{
		Com_DPrintf("failed\n");
		QAL_Shutdown();
		return false;
	}

	Com_Printf("ok\n");

	/* Print OpenAL informations */
	Com_Printf("\n");
	QAL_SoundInfo();
	Com_Printf("\n");

    return true;
}

#endif /* USE_OPENAL */
