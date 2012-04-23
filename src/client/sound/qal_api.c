/*
This Source File was taken from the Q2Pro Port.

Copyright (C) 2010 skuller.net

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

#include "../../common/header/common.h"
#include "header/qal_api.h"
#include <AL/alc.h>
#include <dlfcn.h>

#define QALC_IMP \
    QAL( LPALCCREATECONTEXT, alcCreateContext ); \
    QAL( LPALCMAKECONTEXTCURRENT, alcMakeContextCurrent ); \
    QAL( LPALCPROCESSCONTEXT, alcProcessContext ); \
    QAL( LPALCSUSPENDCONTEXT, alcSuspendContext ); \
    QAL( LPALCDESTROYCONTEXT, alcDestroyContext ); \
    QAL( LPALCGETCURRENTCONTEXT, alcGetCurrentContext ); \
    QAL( LPALCGETCONTEXTSDEVICE, alcGetContextsDevice ); \
    QAL( LPALCOPENDEVICE, alcOpenDevice ); \
    QAL( LPALCCLOSEDEVICE, alcCloseDevice ); \
    QAL( LPALCGETERROR, alcGetError ); \
    QAL( LPALCISEXTENSIONPRESENT, alcIsExtensionPresent ); \
    QAL( LPALCGETPROCADDRESS, alcGetProcAddress ); \
    QAL( LPALCGETENUMVALUE, alcGetEnumValue ); \
    QAL( LPALCGETSTRING, alcGetString ); \
    QAL( LPALCGETINTEGERV, alcGetIntegerv ); \
    QAL( LPALCCAPTUREOPENDEVICE, alcCaptureOpenDevice ); \
    QAL( LPALCCAPTURECLOSEDEVICE, alcCaptureCloseDevice ); \
    QAL( LPALCCAPTURESTART, alcCaptureStart ); \
    QAL( LPALCCAPTURESTOP, alcCaptureStop ); \
    QAL( LPALCCAPTURESAMPLES, alcCaptureSamples );

static cvar_t   *al_driver;
static cvar_t   *al_device;

static void *handle;
static ALCdevice *device;
static ALCcontext *context;

#define QAL(type,func)  static type q##func;
QALC_IMP
#undef QAL

#define QAL(type,func)  type q##func;
QAL_IMP
#undef QAL

void QAL_Shutdown( void ) {
    if( context ) {
        qalcMakeContextCurrent( NULL );
        qalcDestroyContext( context );
        context = NULL;
    }
    if( device ) {
        qalcCloseDevice( device );
        device = NULL;
    }

#define QAL(type,func)  q##func = NULL;
QALC_IMP
QAL_IMP
#undef QAL

    //Sys_FreeLibrary( handle );
	dlclose(handle);
    handle = NULL;

    al_driver->flags &= ~CVAR_ARCHIVE;
    al_device->flags &= ~CVAR_ARCHIVE;
}

qboolean QAL_Init( void ) {
	// DEFAULT_OPENAL_DRIVER is a define from the Makefile
    al_driver = Cvar_Get( "al_driver", DEFAULT_OPENAL_DRIVER, CVAR_ARCHIVE );
    al_device = Cvar_Get( "al_device", "", CVAR_ARCHIVE );

	Com_Printf("LoadLibrary(%s)\n", al_driver->string);
    handle = dlopen( al_driver->string, RTLD_LAZY );
    //Sys_LoadLibrary( al_driver->string, NULL, &handle );
    if( !handle ) {
        return false;
    }

#define QAL(type,func)  q##func = dlsym( handle, #func );
QALC_IMP
QAL_IMP
#undef QAL

    Com_Printf( "...opening OpenAL device: " );
    device = qalcOpenDevice( al_device->string[0] ? al_device->string : NULL );
    if( !device ) {
        goto fail;
    }
    Com_Printf( "ok\n" );

    Com_Printf( "...creating OpenAL context: " );
    context = qalcCreateContext( device, NULL );
    if( !context ) {
        goto fail;
    }
    Com_Printf( "ok\n" );

    Com_Printf( "...making context current: " );
    if( !qalcMakeContextCurrent( context ) ) {
        goto fail;
    }
    Com_Printf( "ok\n" );

    al_driver->flags |= CVAR_ARCHIVE;
    al_device->flags |= CVAR_ARCHIVE;

    return true;

fail:
    Com_DPrintf( "failed\n" );
    QAL_Shutdown();
    return false;
}

