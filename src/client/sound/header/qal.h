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
 * Header file to the low level "qal" API implementation. This source file
 * was taken from Q2Pro and modified by the YQ2 authors.
 *
 * =======================================================================
 */

#ifdef USE_OPENAL

#ifndef _QAL_API_H_
 #define _QAL_API_H_

 #include <AL/al.h>
 #include <AL/efx.h>

/* Function pointers used to tie
*  the qal API to the OpenAL API */
extern LPALENABLE qalEnable;
extern LPALDISABLE qalDisable;
extern LPALISENABLED qalIsEnabled;
extern LPALGETSTRING qalGetString;
extern LPALGETBOOLEANV qalGetBooleanv;
extern LPALGETINTEGERV qalGetIntegerv;
extern LPALGETFLOATV qalGetFloatv;
extern LPALGETDOUBLEV qalGetDoublev;
extern LPALGETBOOLEAN qalGetBoolean;
extern LPALGETINTEGER qalGetInteger;
extern LPALGETFLOAT qalGetFloat;
extern LPALGETDOUBLE qalGetDouble;
extern LPALGETERROR qalGetError;
extern LPALISEXTENSIONPRESENT qalIsExtensionPresent;
extern LPALGETPROCADDRESS qalGetProcAddress;
extern LPALGETENUMVALUE qalGetEnumValue;
extern LPALLISTENERF qalListenerf;
extern LPALLISTENER3F qalListener3f;
extern LPALLISTENERFV qalListenerfv;
extern LPALLISTENERI qalListeneri;
extern LPALLISTENER3I qalListener3i;
extern LPALLISTENERIV qalListeneriv;
extern LPALGETLISTENERF qalGetListenerf;
extern LPALGETLISTENER3F qalGetListener3f;
extern LPALGETLISTENERFV qalGetListenerfv;
extern LPALGETLISTENERI qalGetListeneri;
extern LPALGETLISTENER3I qalGetListener3i;
extern LPALGETLISTENERIV qalGetListeneriv;
extern LPALGENSOURCES qalGenSources;
extern LPALDELETESOURCES qalDeleteSources;
extern LPALISSOURCE qalIsSource;
extern LPALSOURCEF qalSourcef;
extern LPALSOURCE3F qalSource3f;
extern LPALSOURCEFV qalSourcefv;
extern LPALSOURCEI qalSourcei;
extern LPALSOURCE3I qalSource3i;
extern LPALSOURCEIV qalSourceiv;
extern LPALGETSOURCEF qalGetSourcef;
extern LPALGETSOURCE3F qalGetSource3f;
extern LPALGETSOURCEFV qalGetSourcefv;
extern LPALGETSOURCEI qalGetSourcei;
extern LPALGETSOURCE3I qalGetSource3i;
extern LPALGETSOURCEIV qalGetSourceiv;
extern LPALSOURCEPLAYV qalSourcePlayv;
extern LPALSOURCESTOPV qalSourceStopv;
extern LPALSOURCEREWINDV qalSourceRewindv;
extern LPALSOURCEPAUSEV qalSourcePausev;
extern LPALSOURCEPLAY qalSourcePlay;
extern LPALSOURCESTOP qalSourceStop;
extern LPALSOURCEREWIND qalSourceRewind;
extern LPALSOURCEPAUSE qalSourcePause;
extern LPALSOURCEQUEUEBUFFERS qalSourceQueueBuffers;
extern LPALSOURCEUNQUEUEBUFFERS qalSourceUnqueueBuffers;
extern LPALGENBUFFERS qalGenBuffers;
extern LPALDELETEBUFFERS qalDeleteBuffers;
extern LPALISBUFFER qalIsBuffer;
extern LPALBUFFERDATA qalBufferData;
extern LPALBUFFERF qalBufferf;
extern LPALBUFFER3F qalBuffer3f;
extern LPALBUFFERFV qalBufferfv;
extern LPALBUFFERI qalBufferi;
extern LPALBUFFER3I qalBuffer3i;
extern LPALBUFFERIV qalBufferiv;
extern LPALGETBUFFERF qalGetBufferf;
extern LPALGETBUFFER3F qalGetBuffer3f;
extern LPALGETBUFFERFV qalGetBufferfv;
extern LPALGETBUFFERI qalGetBufferi;
extern LPALGETBUFFER3I qalGetBuffer3i;
extern LPALGETBUFFERIV qalGetBufferiv;
extern LPALDOPPLERFACTOR qalDopplerFactor;
extern LPALDOPPLERVELOCITY qalDopplerVelocity;
extern LPALSPEEDOFSOUND qalSpeedOfSound;
extern LPALDISTANCEMODEL qalDistanceModel;
extern LPALGENFILTERS qalGenFilters;
extern LPALFILTERI qalFilteri;
extern LPALFILTERF qalFilterf;
extern LPALDELETEFILTERS qalDeleteFilters;

/*
 * Gives information over the OpenAL
 * implementation and it's state
 */
void QAL_SoundInfo(void);

/*
 * Checks if the output device is still connected. Returns true
 * if it is, false otherwise. Should be called every frame, if
 * disconnected a 'snd_restart' is injected after waiting for 50
 * frame.
 *
 * This is mostly a work around for broken Sound driver. For
 * example the _good_ Intel display driver for Windows 10
 * destroys the DisplayPort sound device when the display
 * resolution changes and recreates it after an unspecified
 * amount of time...
 */
qboolean QAL_RecoverLostDevice();

/*
 * Loads the OpenAL shared lib, creates
 * a context and device handle.
 */
qboolean QAL_Init(void);

/*
 * Shuts OpenAL down, frees all context and
 * device handles and unloads the shared lib.
 */
void QAL_Shutdown(void);

#endif /* _QAL_API_H_ */
#endif /* USE_OPENAL */
