/*
** RW_SDL.C
**
** This file contains ALL Linux specific stuff having to do with the
** software refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** SWimp_EndFrame
** SWimp_Init
** SWimp_InitGraphics
** SWimp_SetPalette
** SWimp_Shutdown
** SWimp_SwitchFullscreen
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "SDL.h"

#ifdef OPENGL
#include <GL/gl.h>
#endif

#ifdef OPENGL
#include "../ref_gl/gl_local.h"
#include "../linux/glw_linux.h"
#else
#include "../ref_soft/r_local.h"
#endif

#include "../client/keys.h"
#include "rw_linux.h"

#ifdef Joystick
#include "joystick.h"
#endif

/*****************************************************************************/

static qboolean                 X11_active = false;

qboolean have_stencil = false;

static SDL_Surface *surface;

#ifndef OPENGL
static unsigned int sdl_palettemode;
#endif

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

#ifdef OPENGL
glwstate_t glw_state;
static cvar_t *use_stencil;
#endif
						      
// Console variables that we need to access from this module

/*****************************************************************************/
/* MOUSE                                                                     */
/*****************************************************************************/

#define MOUSE_MAX 3000
#define MOUSE_MIN 40
qboolean mouse_active;
int mx, my, mouse_buttonstate;

// this is inside the renderer shared lib, so these are called from vid_so

static float old_windowed_mouse;

static cvar_t	*_windowed_mouse;

/************************
 * Joystick
 ************************/
#ifdef Joystick
static SDL_Joystick *joy;
static int joy_oldbuttonstate;
static int joy_numbuttons;
#endif

void RW_IN_PlatformInit() {
  _windowed_mouse = ri.Cvar_Get ("_windowed_mouse", "0", CVAR_ARCHIVE);
}

#ifdef Joystick
qboolean CloseJoystick(void) {
  if (joy) {
    SDL_JoystickClose(joy);
    joy = NULL;
  }
  return true;
}

void PlatformJoyCommands(int *axis_vals, int *axis_map) {
  int i;
  int key_index;
  in_state_t *in_state = getState();
  if (joy) {
    for (i=0 ; i < joy_numbuttons ; i++) {
      if ( SDL_JoystickGetButton(joy, i) && joy_oldbuttonstate!=i ) {
	key_index = (i < 4) ? K_JOY1 : K_AUX1;
	in_state->Key_Event_fp (key_index + i, true);
	joy_oldbuttonstate = i;
      }
      
      if ( !SDL_JoystickGetButton(joy, i) && joy_oldbuttonstate!=i ) {
	key_index = (i < 4) ? K_JOY1 : K_AUX1;
	in_state->Key_Event_fp (key_index + i, false);
	joy_oldbuttonstate = i;
      }
    }
    for (i=0;i<6;i++) {
      axis_vals[axis_map[i]] = (int)SDL_JoystickGetAxis(joy, i);
    }
  }
}
#endif


#if 0
static void IN_DeactivateMouse( void ) 
{ 
  if (!mouse_avail)
    return;
  
  if (mouse_active) {
    /* uninstall_grabs(); */
    mouse_active = false;
  }
}

static void IN_ActivateMouse( void ) 
{
  if (!mouse_avail)
    return;
  
  if (!mouse_active) {
    mx = my = 0; // don't spazz
    /* install_grabs(); */
    mouse_active = true;
  }
}
#endif

void RW_IN_Activate(qboolean active)
{
  mx = my = 0;
#if 0
  if (active || vidmode_active)
    IN_ActivateMouse();
  else
    IN_DeactivateMouse();
#endif		
}

/*****************************************************************************/

#if 0 /* SDL parachute should catch everything... */
// ========================================================================
// Tragic death handler
// ========================================================================

void TragicDeath(int signal_num)
{
  /* SDL_Quit(); */
  Sys_Error("This death brought to you by the number %d\n", signal_num);
}
#endif

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
    
    /* suggestions on how to handle this better would be appreciated */
  case SDLK_WORLD_7:		key = '`'; break;
    
  default: /* assuming that the other sdl keys are mapped to ascii */
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
#ifdef Joystick
	case SDL_JOYBUTTONDOWN:
	  keyq[keyq_head].key = 
	    ((((SDL_JoyButtonEvent*)event)->button < 4)?K_JOY1:K_AUX1)+
	    ((SDL_JoyButtonEvent*)event)->button;
	  keyq[keyq_head].down = true;
	  keyq_head = (keyq_head+1)&63;
	  break;
	case SDL_JOYBUTTONUP:
	  keyq[keyq_head].key = 
	    ((((SDL_JoyButtonEvent*)event)->button < 4)?K_JOY1:K_AUX1)+
	    ((SDL_JoyButtonEvent*)event)->button;
	  keyq[keyq_head].down = false;
	  keyq_head = (keyq_head+1)&63;
	  break;
#endif
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
	    fullscreen->modified = false; /* we just changed it with SDL. */
	    
	    break; /* ignore this key */
	  }
	  
	  if ( (KeyStates[SDLK_LCTRL] || KeyStates[SDLK_RCTRL]) &&
	       (event->key.keysym.sym == SDLK_g) ) {
	    SDL_GrabMode gm = SDL_WM_GrabInput(SDL_GRAB_QUERY);
	    /*	
	      SDL_WM_GrabInput((gm == SDL_GRAB_ON) ? SDL_GRAB_OFF : SDL_GRAB_ON);
	      gm = SDL_WM_GrabInput(SDL_GRAB_QUERY);
	    */	
	    ri.Cvar_SetValue( "_windowed_mouse", (gm == SDL_GRAB_ON) ? /*1*/ 0 : /*0*/ 1 );
	    
	    break; /* ignore this key */
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

#ifdef Joystick
qboolean OpenJoystick(cvar_t *joy_dev) {
  int num_joysticks, i;
  joy = NULL;

  if (!(SDL_INIT_JOYSTICK&SDL_WasInit(SDL_INIT_JOYSTICK))) {
    ri.Con_Printf(PRINT_ALL, "SDL Joystick not initialized, trying to init...\n");
    SDL_Init(SDL_INIT_JOYSTICK);
  }
  ri.Con_Printf(PRINT_ALL, "Trying to start-up joystick...\n");
  if ((num_joysticks=SDL_NumJoysticks())) {
    for(i=0;i<num_joysticks;i++) {
      ri.Con_Printf(PRINT_ALL, "Trying joystick [%s]\n", 
		    SDL_JoystickName(i));
      if (!SDL_JoystickOpened(i)) {
	joy = SDL_JoystickOpen(i);
	if (joy) {
	  ri.Con_Printf(PRINT_ALL, "Joytick activated.\n");
	  joy_numbuttons = SDL_JoystickNumButtons(joy);
	  break;
	}
      }
    }
    if (!joy) {
      ri.Con_Printf(PRINT_ALL, "Failed to open any joysticks\n");
      return false;
    }
  }
  else {
    ri.Con_Printf(PRINT_ALL, "No joysticks available\n");
    return false;
  }
  return true;
}
#endif


/*****************************************************************************/

/*
** SWimp_Init
**
** This routine is responsible for initializing the implementation
** specific stuff in a software rendering subsystem.
*/
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
  
  //	SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
  
  // catch signals so i can turn on auto-repeat
#if 0
  {
    struct sigaction sa;
    sigaction(SIGINT, 0, &sa);
    sa.sa_handler = TragicDeath;
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);
  }
#endif
  return true;
}



#ifdef OPENGL
void *GLimp_GetProcAddress(const char *func)
{
	return SDL_GL_GetProcAddress(func);
}

int GLimp_Init( void *hInstance, void *wndProc )
{
  return SWimp_Init(hInstance, wndProc);
}
#endif

static void SetSDLIcon()
{
#include "q2icon.xbm"
	SDL_Surface *icon;
	SDL_Color color;
	Uint8 *ptr;
	int i, mask;

	icon = SDL_CreateRGBSurface(SDL_SWSURFACE, q2icon_width, q2icon_height, 8, 0, 0, 0, 0);
	if (icon == NULL)
		return; /* oh well... */
	SDL_SetColorKey(icon, SDL_SRCCOLORKEY, 0);

	color.r = 255;
	color.g = 255;
	color.b = 255;
	SDL_SetColors(icon, &color, 0, 1); /* just in case */
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

/*
** SWimp_InitGraphics
**
** This initializes the software refresh's implementation specific
** graphics subsystem.  In the case of Windows it creates DIB or
** DDRAW surfaces.
**
** The necessary width and height parameters are grabbed from
** vid.width and vid.height.
*/
#ifndef OPENGL
static qboolean SWimp_InitGraphics( qboolean fullscreen )
{
	const SDL_VideoInfo *vinfo;
	int flags;

	/* Just toggle fullscreen if that's all that has been changed */	
	if (surface && (surface->w == vid.width) && (surface->h == vid.height)) {
		int isfullscreen = (surface->flags & SDL_FULLSCREEN) ? 1 : 0;
		if (fullscreen != isfullscreen)
			SDL_WM_ToggleFullScreen(surface);
	
		isfullscreen = (surface->flags & SDL_FULLSCREEN) ? 1 : 0;
		if (fullscreen == isfullscreen)
			return true;
	}
	
	srandom(getpid());

	// free resources in use
	if (surface)
		SDL_FreeSurface(surface);

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow (vid.width, vid.height);

/* 
	Okay, I am going to query SDL for the "best" pixel format.
	If the depth is not 8, use SetPalette with logical pal, 
	else use SetColors.
	
	Hopefully this works all the time.
*/
	vinfo = SDL_GetVideoInfo();
	sdl_palettemode = (vinfo->vfmt->BitsPerPixel == 8) ? (SDL_PHYSPAL|SDL_LOGPAL) : SDL_LOGPAL;
	flags = /*SDL_DOUBLEBUF|*/SDL_SWSURFACE|SDL_HWPALETTE;
	if (fullscreen)
		flags |= SDL_FULLSCREEN;
	
	SetSDLIcon(); /* currently uses q2icon.xbm data */
	
	if ((surface = SDL_SetVideoMode(vid.width, vid.height, 8, flags)) == NULL) {
		Sys_Error("(SOFTSDL) SDL SetVideoMode failed: %s\n", SDL_GetError());
		return false;
	}
	
	SDL_WM_SetCaption("Quake II", "Quake II");

	SDL_ShowCursor(0);

	vid.rowbytes = surface->pitch;
	vid.buffer = surface->pixels;

	X11_active = true;

	

	return true;
}
#else
static qboolean GLimp_InitGraphics( qboolean fullscreen )
{
	int flags;
	
	/* Just toggle fullscreen if that's all that has been changed */
	if (surface && (surface->w == vid.width) && (surface->h == vid.height)) {
		int isfullscreen = (surface->flags & SDL_FULLSCREEN) ? 1 : 0;
		if (fullscreen != isfullscreen)
			SDL_WM_ToggleFullScreen(surface);

		isfullscreen = (surface->flags & SDL_FULLSCREEN) ? 1 : 0;
		if (fullscreen == isfullscreen)
			return true;
	}
	
	srandom(getpid());

	// free resources in use
	if (surface)
		SDL_FreeSurface(surface);

	// let the sound and input subsystems know about the new window
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
	
	SetSDLIcon(); /* currently uses q2icon.xbm data */
	
	if ((surface = SDL_SetVideoMode(vid.width, vid.height, 0, flags)) == NULL) {
		Sys_Error("(SDLGL) SDL SetVideoMode failed: %s\n", SDL_GetError());
		return false;
	}

	// stencilbuffer shadows
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

	return true;
}
#endif

#ifdef OPENGL
void GLimp_BeginFrame( float camera_seperation )
{
}
#endif

/*
** SWimp_EndFrame
**
** This does an implementation specific copy from the backbuffer to the
** front buffer.  In the Win32 case it uses BitBlt or BltFast depending
** on whether we're using DIB sections/GDI or DDRAW.
*/

#ifndef OPENGL
void SWimp_EndFrame (void)
{
	/* SDL_Flip(surface); */
	SDL_UpdateRect(surface, 0, 0, 0, 0);
}
#else
void GLimp_EndFrame (void)
{
	SDL_GL_SwapBuffers();
}
#endif

/*
** SWimp_SetMode
*/
#ifndef OPENGL
rserr_t SWimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	rserr_t retval = rserr_ok;

	ri.Con_Printf (PRINT_ALL, "setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( pwidth, pheight, mode ) )
	{
		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	ri.Con_Printf( PRINT_ALL, " %d %d\n", *pwidth, *pheight);

	if ( !SWimp_InitGraphics( fullscreen ) ) {
		// failed to set a valid mode in windowed mode
		return rserr_invalid_mode;
	}

	R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_8to24table );

	return retval;
}
#else
int GLimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	ri.Con_Printf (PRINT_ALL, "setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( pwidth, pheight, mode ) )
	{
		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	ri.Con_Printf( PRINT_ALL, " %d %d\n", *pwidth, *pheight);

	if ( !GLimp_InitGraphics( fullscreen ) ) {
		// failed to set a valid mode in windowed mode
		return rserr_invalid_mode;
	}

	return rserr_ok;
}
#endif

/*
** SWimp_SetPalette
**
** System specific palette setting routine.  A NULL palette means
** to use the existing palette.  The palette is expected to be in
** a padded 4-byte xRGB format.
*/
#ifndef OPENGL
void SWimp_SetPalette( const unsigned char *palette )
{
	SDL_Color colors[256];
	
	int i;

	if (!X11_active)
		return;

	if ( !palette )
	        palette = ( const unsigned char * ) sw_state.currentpalette;
 
	for (i = 0; i < 256; i++) {
		colors[i].r = palette[i*4+0];
		colors[i].g = palette[i*4+1];
		colors[i].b = palette[i*4+2];
	}

	SDL_SetPalette(surface, sdl_palettemode, colors, 0, 256);
}
#endif

/*
** SWimp_Shutdown
**
** System specific graphics subsystem shutdown routine.  Destroys
** DIBs or DDRAW surfaces as appropriate.
*/

void SWimp_Shutdown( void )
{
	if (surface)
		SDL_FreeSurface(surface);
	surface = NULL;
	
	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		
	X11_active = false;
}

#ifdef OPENGL
void GLimp_Shutdown( void )
{
	SWimp_Shutdown();
}
#endif

/*
** SWimp_AppActivate
*/
#ifndef OPENGL
void SWimp_AppActivate( qboolean active )
{
}
#else
void GLimp_AppActivate( qboolean active )
{
}
#endif

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

//	fprintf(stderr, "writable code %lx(%lx)-%lx, length=%lx\n", startaddr,
//			addr, startaddr+length, length);

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
  
  // get events from x server
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
      if (SDL_BUTTON(3) & bstate) /* quake2 has the right button be mouse2 */
	mouse_buttonstate |= (1 << 1);
      if (SDL_BUTTON(2) & bstate) /* quake2 has the middle button be mouse3 */
	mouse_buttonstate |= (1 << 2);
      if (SDL_BUTTON(6) & bstate)
	mouse_buttonstate |= (1 << 3);
      if (SDL_BUTTON(7) & bstate)
	mouse_buttonstate |= (1 << 4);
      
      
      if (old_windowed_mouse != _windowed_mouse->value) {
	old_windowed_mouse = _windowed_mouse->value;
	
	if (!_windowed_mouse->value) {
	  /* ungrab the pointer */
	  SDL_WM_GrabInput(SDL_GRAB_OFF);
	} else {
	  /* grab the pointer */
	  SDL_WM_GrabInput(SDL_GRAB_ON);
	}
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


#ifdef OPENGL
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
	qgl3DfxSetPaletteEXT((GLuint *)temptable);
}
#endif
