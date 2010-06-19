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
#include "../posix/refresh/glwindow.h"

#include "../client/input/header/keyboard.h"
#include "../posix/posix.h"

/*****************************************************************************/

static qboolean                 X11_active = false;

qboolean have_stencil = false;

static SDL_Surface *surface;

struct
{
  int key;
  int down;
} keyq[64];
int keyq_head=0;
int keyq_tail=0;

int config_notify=0;
int config_notify_width;
int config_notify_height;

glwstate_t glw_state;
static cvar_t *use_stencil;
						      

/*****************************************************************************/
/* MOUSE                                                                     */
/*****************************************************************************/

#define MOUSE_MAX 3000
#define MOUSE_MIN 40
qboolean mouse_active;
int mx, my, mouse_buttonstate;

static float old_windowed_mouse;
static cvar_t	*windowed_mouse;
static cvar_t   *windowed_mouse_always;

void RW_IN_PlatformInit() {
  windowed_mouse = ri.Cvar_Get ("windowed_mouse", "1", CVAR_ARCHIVE);
  windowed_mouse_always = ri.Cvar_Get ("windowed_mouse_always", "2", CVAR_ARCHIVE);
}

void RW_IN_Activate(qboolean active)
{
  mx = my = 0;
}

/*****************************************************************************/

int XLateKey(unsigned int keysym)
{
  int key;
  
  key = 0;
  switch(keysym) {
  case SDLK_KP9:			key = K_KP_PGUP; break;
  case SDLK_PAGEUP:		key = K_PGUP; break;
    
  case SDLK_KP3:			key = K_KP_PGDN; break;
  case SDLK_PAGEDOWN:		key = K_PGDN; break;
    
  case SDLK_KP7:			key = K_KP_HOME; break;
  case SDLK_HOME:			key = K_HOME; break;
    
  case SDLK_KP1:			key = K_KP_END; break;
  case SDLK_END:			key = K_END; break;
    
  case SDLK_KP4:			key = K_KP_LEFTARROW; break;
  case SDLK_LEFT:			key = K_LEFTARROW; break;
    
  case SDLK_KP6:			key = K_KP_RIGHTARROW; break;
  case SDLK_RIGHT:		key = K_RIGHTARROW; break;
    
  case SDLK_KP2:			key = K_KP_DOWNARROW; break;
  case SDLK_DOWN:			key = K_DOWNARROW; break;
    
  case SDLK_KP8:			key = K_KP_UPARROW; break;
  case SDLK_UP:			key = K_UPARROW; break;
    
  case SDLK_ESCAPE:		key = K_ESCAPE; break;
    
  case SDLK_KP_ENTER:		key = K_KP_ENTER; break;
  case SDLK_RETURN:		key = K_ENTER; break;
    
  case SDLK_TAB:			key = K_TAB; break;
    
  case SDLK_F1:			key = K_F1; break;
  case SDLK_F2:			key = K_F2; break;
  case SDLK_F3:			key = K_F3; break;
  case SDLK_F4:			key = K_F4; break;
  case SDLK_F5:			key = K_F5; break;
  case SDLK_F6:			key = K_F6; break;
  case SDLK_F7:			key = K_F7; break;
  case SDLK_F8:			key = K_F8; break;
  case SDLK_F9:			key = K_F9; break;
  case SDLK_F10:			key = K_F10; break;
  case SDLK_F11:			key = K_F11; break;
  case SDLK_F12:			key = K_F12; break;
    
  case SDLK_BACKSPACE:		key = K_BACKSPACE; break;
    
  case SDLK_KP_PERIOD:		key = K_KP_DEL; break;
  case SDLK_DELETE:		key = K_DEL; break;
    
  case SDLK_PAUSE:		key = K_PAUSE; break;
    
  case SDLK_LSHIFT:
  case SDLK_RSHIFT:		key = K_SHIFT; break;
    
  case SDLK_LCTRL:
  case SDLK_RCTRL:		key = K_CTRL; break;
    
  case SDLK_LMETA:
  case SDLK_RMETA:
  case SDLK_LALT:
  case SDLK_RALT:			key = K_ALT; break;
    
  case SDLK_KP5:			key = K_KP_5; break;
    
  case SDLK_INSERT:		key = K_INS; break;
  case SDLK_KP0:			key = K_KP_INS; break;
    
  case SDLK_KP_MULTIPLY:		key = '*'; break;
  case SDLK_KP_PLUS:		key = K_KP_PLUS; break;
  case SDLK_KP_MINUS:		key = K_KP_MINUS; break;
  case SDLK_KP_DIVIDE:		key = K_KP_SLASH; break;
    
  case SDLK_WORLD_7:		key = '`'; break;
    
  default: 
    if (keysym < 128)
      key = keysym;
    break;
  }
  
  return key;		
}

static unsigned char KeyStates[SDLK_LAST];

void getMouse(int *x, int *y, int *state) {
  *x = mx;
  *y = my;
  *state = mouse_buttonstate;
}

void doneMouse() {
  mx = my = 0;
}

void GetEvent(SDL_Event *event)
{
	unsigned int key;
	
	switch(event->type) {
	case SDL_MOUSEBUTTONDOWN:
	  if (event->button.button == 4) {
	    keyq[keyq_head].key = K_MWHEELUP;
	    keyq[keyq_head].down = true;
	    keyq_head = (keyq_head + 1) & 63;
	    keyq[keyq_head].key = K_MWHEELUP;
	    keyq[keyq_head].down = false;
	    keyq_head = (keyq_head + 1) & 63;
	  } else if (event->button.button == 5) {
	    keyq[keyq_head].key = K_MWHEELDOWN;
	    keyq[keyq_head].down = true;
	    keyq_head = (keyq_head + 1) & 63;
	    keyq[keyq_head].key = K_MWHEELDOWN;
	    keyq[keyq_head].down = false;
	    keyq_head = (keyq_head + 1) & 63;
	  } 
	  break;
	case SDL_MOUSEBUTTONUP:
	  break;

	case SDL_KEYDOWN:
	  if ( (KeyStates[SDLK_LALT] || KeyStates[SDLK_RALT]) &&
	       (event->key.keysym.sym == SDLK_RETURN) ) {
	    cvar_t *fullscreen;
	    
	    SDL_WM_ToggleFullScreen(surface);
	    
	    if (surface->flags & SDL_FULLSCREEN) {
	      ri.Cvar_SetValue( "vid_fullscreen", 1 );
	    } else {
	      ri.Cvar_SetValue( "vid_fullscreen", 0 );
	    }
	    
	    fullscreen = ri.Cvar_Get( "vid_fullscreen", "0", 0 );
	    fullscreen->modified = false; 
	    
	    break;
	  }
	  
	  if ( (KeyStates[SDLK_LCTRL] || KeyStates[SDLK_RCTRL]) &&
	       (event->key.keysym.sym == SDLK_g) ) {
	    SDL_GrabMode gm = SDL_WM_GrabInput(SDL_GRAB_QUERY);
	    ri.Cvar_SetValue( "windowed_mouse", (gm == SDL_GRAB_ON) ? 1 : 0 );
	    
	    break; 
	  }
	  
	  KeyStates[event->key.keysym.sym] = 1;
	  
	  key = XLateKey(event->key.keysym.sym);
	  if (key) {
	    keyq[keyq_head].key = key;
	    keyq[keyq_head].down = true;
	    keyq_head = (keyq_head + 1) & 63;
	  }
	  break;
	case SDL_KEYUP:
	  if (KeyStates[event->key.keysym.sym]) {
	    KeyStates[event->key.keysym.sym] = 0;
	    
	    key = XLateKey(event->key.keysym.sym);
	    if (key) {
	      keyq[keyq_head].key = key;
	      keyq[keyq_head].down = false;
	      keyq_head = (keyq_head + 1) & 63;
	    }
	  }
	  break;
	case SDL_QUIT:
	  ri.Cmd_ExecuteText(EXEC_NOW, "quit");
	  break;
	}

}

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
#include "../posix/refresh/q2icon.xbm"
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

Key_Event_fp_t Key_Event_fp;

void KBD_Init(Key_Event_fp_t fp)
{
  Key_Event_fp = fp;
}

void KBD_Update(void)
{
  SDL_Event event;
  static int KBD_Update_Flag;
  
  in_state_t *in_state = getState();

  if (KBD_Update_Flag == 1)
    return;
  
  KBD_Update_Flag = 1;
  
  if (X11_active)
  {
	  int bstate;

	  while (SDL_PollEvent(&event))
		  GetEvent(&event);

	  if (!mx && !my)
		  SDL_GetRelativeMouseState(&mx, &my);
	  mouse_buttonstate = 0;
	  bstate = SDL_GetMouseState(NULL, NULL);
	  if (SDL_BUTTON(1) & bstate)
		  mouse_buttonstate |= (1 << 0);
	  if (SDL_BUTTON(3) & bstate) 
		  mouse_buttonstate |= (1 << 1);
	  if (SDL_BUTTON(2) & bstate) 
		  mouse_buttonstate |= (1 << 2);
	  if (SDL_BUTTON(6) & bstate)
		  mouse_buttonstate |= (1 << 3);
	  if (SDL_BUTTON(7) & bstate)
		  mouse_buttonstate |= (1 << 4);

	  if (windowed_mouse_always->value == 2)
	  {
		  if (old_windowed_mouse != windowed_mouse->value) {
			  old_windowed_mouse = windowed_mouse->value;

			  if (!windowed_mouse->value) {
				  SDL_WM_GrabInput(SDL_GRAB_OFF);
			  } else {
				  SDL_WM_GrabInput(SDL_GRAB_ON);
			  }
		  }
	  }
	  else if (windowed_mouse_always->value == 1)
	  {
		  SDL_WM_GrabInput(SDL_GRAB_ON);
	  }
	  else
	  {
		  SDL_WM_GrabInput(SDL_GRAB_OFF);
	  }

	  while (keyq_head != keyq_tail)
	  {
		  in_state->Key_Event_fp(keyq[keyq_tail].key, keyq[keyq_tail].down);
		  keyq_tail = (keyq_tail + 1) & 63;
	  }
  }

  KBD_Update_Flag = 0;
}

void KBD_Close(void)
{
	keyq_head = 0;
	keyq_tail = 0;
	
	memset(keyq, 0, sizeof(keyq));
}

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

