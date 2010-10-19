/*
	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA
*/ 

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "SDL.h"
#include <GL/gl.h>
#include "../refresh/header/local.h"
#include "../unix/header/glwindow.h"

#include "../client/input/header/keyboard.h"
#include "../unix/header/unix.h"

/*****************************************************************************/

static qboolean                 X11_active = false;

qboolean have_stencil = false;

SDL_Surface *surface;

int config_notify=0;
int config_notify_width;
int config_notify_height;

glwstate_t glw_state;
static cvar_t *use_stencil;
						      

/*****************************************************************************/
/* MOUSE                                                                     */
/*****************************************************************************/

/*****************************************************************************/



/*****************************************************************************/

int SWimp_Init( void *hInstance, void *wndProc )
{
  if (SDL_WasInit(SDL_INIT_AUDIO|SDL_INIT_CDROM|SDL_INIT_VIDEO) == 0) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      Sys_Error("SDL Init failed: %s\n", SDL_GetError());
      return false;
    }
  } else if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
      Sys_Error("SDL Init failed: %s\n", SDL_GetError());
      return false;
    }
  }

  return true;
}

void *GLimp_GetProcAddress(const char *func)
{
	return SDL_GL_GetProcAddress(func);
}

int GLimp_Init( void *hInstance, void *wndProc )
{
  return SWimp_Init(hInstance, wndProc);
}

static void SetSDLIcon()
{
#include "icon/q2icon.xbm"
	SDL_Surface *icon;
	SDL_Color color;
	Uint8 *ptr;
	int i, mask;

	icon = SDL_CreateRGBSurface(SDL_SWSURFACE, q2icon_width, q2icon_height, 8, 0, 0, 0, 0);
	if (icon == NULL)
		return; 
	SDL_SetColorKey(icon, SDL_SRCCOLORKEY, 0);

	color.r = 255;
	color.g = 255;
	color.b = 255;
	SDL_SetColors(icon, &color, 0, 1);
	color.r = 0;
	color.g = 16;
	color.b = 0;
	SDL_SetColors(icon, &color, 1, 1);

	ptr = (Uint8 *)icon->pixels;
	for (i = 0; i < sizeof(q2icon_bits); i++) {
		for (mask = 1; mask != 0x100; mask <<= 1) {
			*ptr = (q2icon_bits[i] & mask) ? 1 : 0;
			ptr++;
		}		
	}

	SDL_WM_SetIcon(icon, NULL);
	SDL_FreeSurface(icon);
}

void
UpdateHardwareGamma(void)
{
	float g = (1.3 - vid_gamma->value + 1);
	g = (g > 1 ? g : 1);
	
	SDL_SetGamma(g, g, g);
}

void 
SetSDLGamma(void)
{
	gl_state.hwgamma = true;
	vid_gamma->modified = true;
	ri.Con_Printf(PRINT_ALL, "Using hardware gamma\n");
}  


static qboolean GLimp_InitGraphics( qboolean fullscreen )
{
	int flags;
	
	if (surface && (surface->w == vid.width) && (surface->h == vid.height)) {
		int isfullscreen = (surface->flags & SDL_FULLSCREEN) ? 1 : 0;
		if (fullscreen != isfullscreen)
			SDL_WM_ToggleFullScreen(surface);

		isfullscreen = (surface->flags & SDL_FULLSCREEN) ? 1 : 0;
		if (fullscreen == isfullscreen)
			return true;
	}
	
	srandom(getpid());

	if (surface)
		SDL_FreeSurface(surface);

	ri.Vid_NewWindow (vid.width, vid.height);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	
	if (use_stencil) 
	  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);
	
	flags = SDL_OPENGL;
	if (fullscreen)
		flags |= SDL_FULLSCREEN;
	
	SetSDLIcon();
	
	if ((surface = SDL_SetVideoMode(vid.width, vid.height, 0, flags)) == NULL) {
		Sys_Error("(SDLGL) SDL SetVideoMode failed: %s\n", SDL_GetError());
		return false;
	}

 	if (use_stencil) {
	  int stencil_bits;
	  
	  have_stencil = false;
	  
	  if (!SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits)) {
	    ri.Con_Printf(PRINT_ALL, "I: got %d bits of stencil\n", 
			  stencil_bits);
	    if (stencil_bits >= 1) {
	      have_stencil = true;
	    }
	  }
	}

	SDL_WM_SetCaption("Quake II", "Quake II");

	SDL_ShowCursor(0);

	X11_active = true;
    SetSDLGamma();

	return true;
}

void GLimp_BeginFrame( float camera_seperation )
{
}

void GLimp_EndFrame (void)
{
	SDL_GL_SwapBuffers();
}


int GLimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	ri.Con_Printf (PRINT_ALL, "setting mode %d:", mode );
	
	// mode -1 is not in the vid mode table - so we keep the values in pwidth 
	// and pheight and don't even try to look up the mode info
	if ( mode != -1 && !ri.Vid_GetModeInfo( pwidth, pheight, mode ) )
	{
		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	ri.Con_Printf( PRINT_ALL, " %d %d\n", *pwidth, *pheight);

	if ( !GLimp_InitGraphics( fullscreen ) ) {
		return rserr_invalid_mode;
	}

	return rserr_ok;
}


void GLimp_Shutdown( void )
{
	if (surface)
		SDL_FreeSurface(surface);
	surface = NULL;
	
	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	
	gl_state.hwgamma = false;
	X11_active = false;
}

void GLimp_AppActivate( qboolean active )
{
}

//===============================================================================

/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{

	int r;
	unsigned long addr;
	int psize = getpagesize();

	addr = (startaddr & ~(psize-1)) - psize;

	r = mprotect((char*)addr, length + startaddr - addr + psize, 7);

	if (r < 0)
    		Sys_Error("Protection change failed\n");

}

/*****************************************************************************/
/* KEYBOARD                                                                  */
/*****************************************************************************/


void Fake_glColorTableEXT( GLenum target, GLenum internalformat,
                             GLsizei width, GLenum format, GLenum type,
                             const GLvoid *table )
{
	byte temptable[256][4];
	byte *intbl;
	int i;

	for (intbl = (byte *)table, i = 0; i < 256; i++) {
		temptable[i][2] = *intbl++;
		temptable[i][1] = *intbl++;
		temptable[i][0] = *intbl++;
		temptable[i][3] = 255;
	}
}

