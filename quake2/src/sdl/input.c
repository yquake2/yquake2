/*
 * Copyright (C) 2010 Yamagi Burmeister
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * This is the Quake II input system, written in SDL.
 *
 * =======================================================================
 */

#include <SDL.h>
#include "../refresh/header/local.h"
#include "../client/input/header/keyboard.h"
#include "../unix/header/unix.h"

#define MOUSE_MAX 3000
#define MOUSE_MIN 40

static int		old_windowed_mouse;
static cvar_t	*windowed_mouse;
static cvar_t   *in_grab;
static int		mouse_x, mouse_y;
static int		old_mouse_x, old_mouse_y;
static int		mouse_buttonstate;
static int		mouse_oldbuttonstate;    

struct
{
  int key;
  int down;
} keyq[64];

int keyq_head=0;
int keyq_tail=0;
int mx;
int my; 

Key_Event_fp_t Key_Event_fp;

extern SDL_Surface	 *surface; 
static in_state_t    *in_state;
static unsigned char KeyStates[SDLK_LAST];
static qboolean		 mlooking;

static cvar_t *sensitivity;
static cvar_t *exponential_speedup;
static cvar_t *lookstrafe;
static cvar_t *m_side;
static cvar_t *m_yaw;
static cvar_t *m_pitch;
static cvar_t *m_forward;
static cvar_t *freelook;
static cvar_t *m_filter;
static cvar_t *in_mouse;

/*
 * This function translates the SDL keycodes
 * to the internal key representation of the
 * id Tech 2 engine.
 */
int
IN_TranslateSDLtoQ2Key(unsigned int keysym)
{
    int key = 0;

    if( keysym >= SDLK_SPACE && keysym < SDLK_DELETE )
    {
        /* These happen to match the ASCII chars */
        key = (int)keysym;
    }
    else
    {
        switch( keysym )
        {
            case SDLK_PAGEUP:       key = K_PGUP;          break;
            case SDLK_KP9:          key = K_KP_PGUP;       break;
            case SDLK_PAGEDOWN:     key = K_PGDN;          break;
            case SDLK_KP3:          key = K_KP_PGDN;       break;
            case SDLK_KP7:          key = K_KP_HOME;       break;
            case SDLK_HOME:         key = K_HOME;          break;
            case SDLK_KP1:          key = K_KP_END;        break;
            case SDLK_END:          key = K_END;           break;
            case SDLK_KP4:          key = K_KP_LEFTARROW;  break;
            case SDLK_LEFT:         key = K_LEFTARROW;     break;
            case SDLK_KP6:          key = K_KP_RIGHTARROW; break;
            case SDLK_RIGHT:        key = K_RIGHTARROW;    break;
            case SDLK_KP2:          key = K_KP_DOWNARROW;  break;
            case SDLK_DOWN:         key = K_DOWNARROW;     break;
            case SDLK_KP8:          key = K_KP_UPARROW;    break;
            case SDLK_UP:           key = K_UPARROW;       break;
            case SDLK_ESCAPE:       key = K_ESCAPE;        break;
            case SDLK_KP_ENTER:     key = K_KP_ENTER;      break;
            case SDLK_RETURN:       key = K_ENTER;         break;
            case SDLK_TAB:          key = K_TAB;           break;
            case SDLK_F1:           key = K_F1;            break;
            case SDLK_F2:           key = K_F2;            break;
            case SDLK_F3:           key = K_F3;            break;
            case SDLK_F4:           key = K_F4;            break;
            case SDLK_F5:           key = K_F5;            break;
            case SDLK_F6:           key = K_F6;            break;
            case SDLK_F7:           key = K_F7;            break;
            case SDLK_F8:           key = K_F8;            break;
            case SDLK_F9:           key = K_F9;            break;
            case SDLK_F10:          key = K_F10;           break;
            case SDLK_F11:          key = K_F11;           break;
            case SDLK_F12:          key = K_F12;           break;
            case SDLK_F13:          key = K_F13;           break;
            case SDLK_F14:          key = K_F14;           break;
            case SDLK_F15:          key = K_F15;           break;

            case SDLK_BACKSPACE:    key = K_BACKSPACE;     break;
            case SDLK_KP_PERIOD:    key = K_KP_DEL;        break;
            case SDLK_DELETE:       key = K_DEL;           break;
            case SDLK_PAUSE:        key = K_PAUSE;         break;

            case SDLK_LSHIFT:
            case SDLK_RSHIFT:       key = K_SHIFT;         break;

            case SDLK_LCTRL:
            case SDLK_RCTRL:        key = K_CTRL;          break;

            case SDLK_RMETA:
            case SDLK_LMETA:        key = K_COMMAND;       break;

            case SDLK_RALT:
            case SDLK_LALT:         key = K_ALT;           break;

            case SDLK_LSUPER:
            case SDLK_RSUPER:       key = K_SUPER;         break;

            case SDLK_KP5:          key = K_KP_5;          break;
            case SDLK_INSERT:       key = K_INS;           break;
            case SDLK_KP0:          key = K_KP_INS;        break;
            case SDLK_KP_MULTIPLY:  key = K_KP_STAR;       break;
            case SDLK_KP_PLUS:      key = K_KP_PLUS;       break;
            case SDLK_KP_MINUS:     key = K_KP_MINUS;      break;
            case SDLK_KP_DIVIDE:    key = K_KP_SLASH;      break;

            case SDLK_MODE:         key = K_MODE;          break;
            case SDLK_COMPOSE:      key = K_COMPOSE;       break;
            case SDLK_HELP:         key = K_HELP;          break;
            case SDLK_PRINT:        key = K_PRINT;         break;
            case SDLK_SYSREQ:       key = K_SYSREQ;        break;
            case SDLK_BREAK:        key = K_BREAK;         break;
            case SDLK_MENU:         key = K_MENU;          break;
            case SDLK_POWER:        key = K_POWER;         break;
            case SDLK_EURO:         key = K_EURO;          break;
            case SDLK_UNDO:         key = K_UNDO;          break;
            case SDLK_SCROLLOCK:    key = K_SCROLLOCK;     break;
            case SDLK_NUMLOCK:      key = K_KP_NUMLOCK;    break;
            case SDLK_CAPSLOCK:     key = K_CAPSLOCK;      break;

            default:
                if( keysym >= SDLK_WORLD_0 && keysym <= SDLK_WORLD_95 )
                    key = ( keysym - SDLK_WORLD_0 ) + K_WORLD_0;
                break;
        }
    }

    return key;
}

/*
 * Input event processing
 */
void 
IN_GetEvent(SDL_Event *event)
{
	unsigned int key;
	
	switch(event->type) 
	{
		/* The mouse wheel */
		case SDL_MOUSEBUTTONDOWN:
			if (event->button.button == 4) 
			{
				keyq[keyq_head].key = K_MWHEELUP;
				keyq[keyq_head].down = true;
				keyq_head = (keyq_head + 1) & 63;
				keyq[keyq_head].key = K_MWHEELUP;
				keyq[keyq_head].down = false;
				keyq_head = (keyq_head + 1) & 63;
			} 
			else if (event->button.button == 5) 
			{
				keyq[keyq_head].key = K_MWHEELDOWN;
				keyq[keyq_head].down = true;
				keyq_head = (keyq_head + 1) & 63;
				keyq[keyq_head].key = K_MWHEELDOWN;
				keyq[keyq_head].down = false;
				keyq_head = (keyq_head + 1) & 63;
			} 
			break;

		/* The user pressed a button */
		case SDL_KEYDOWN:
			/* Fullscreen switch via Alt-Return */
			if ( (KeyStates[SDLK_LALT] || KeyStates[SDLK_RALT]) && (event->key.keysym.sym == SDLK_RETURN) ) 
			{
				cvar_t *fullscreen;

				SDL_WM_ToggleFullScreen(surface);

				if (surface->flags & SDL_FULLSCREEN) 
				{
					ri.Cvar_SetValue( "vid_fullscreen", 1 );
				} 
				else 
				{
					ri.Cvar_SetValue( "vid_fullscreen", 0 );
				}

				fullscreen = ri.Cvar_Get( "vid_fullscreen", "0", 0 );
				fullscreen->modified = false; 

				break;
			}
	  
			KeyStates[event->key.keysym.sym] = 1;
	  
			/* Get the pressed key and add it to the key list */
			key = IN_TranslateSDLtoQ2Key(event->key.keysym.sym);
			if (key) 
			{
				keyq[keyq_head].key = key;
				keyq[keyq_head].down = true;
				keyq_head = (keyq_head + 1) & 63;
			}
			break;

		/* The user released a key */
		case SDL_KEYUP:
			if (KeyStates[event->key.keysym.sym]) 
			{
				KeyStates[event->key.keysym.sym] = 0;

				/* Get the pressed key and remove it from the key list */
				key = IN_TranslateSDLtoQ2Key(event->key.keysym.sym);
				if (key) 
				{
					keyq[keyq_head].key = key;
					keyq[keyq_head].down = false;
					keyq_head = (keyq_head + 1) & 63;
				}
			}
			break;
	}
}

/*
 * Updates the state of the input queue
 */
void IN_Update(void)
{
  SDL_Event event;
  static int IN_Update_Flag;
  int bstate;

  /* Protection against multiple calls */
  if (IN_Update_Flag == 1)
  {
    return;
  }
  
  IN_Update_Flag = 1;

  while (SDL_PollEvent(&event))
  {
	  IN_GetEvent(&event);
  }

  /* Mouse button processing. Button 4 
     and 5 are the mousewheel and thus
     not processed here. */

  if (!mx && !my)
  {
	  SDL_GetRelativeMouseState(&mx, &my);
  }

  mouse_buttonstate = 0;
  bstate = SDL_GetMouseState(NULL, NULL);

  if (SDL_BUTTON(1) & bstate)
  {
	  mouse_buttonstate |= (1 << 0);
  }
  else if (SDL_BUTTON(3) & bstate) 
  {
	  mouse_buttonstate |= (1 << 1);
  }
  else if (SDL_BUTTON(2) & bstate) 
  {
	  mouse_buttonstate |= (1 << 2);
  }
  else if (SDL_BUTTON(6) & bstate)
  {
	  mouse_buttonstate |= (1 << 3);
  }
  else if (SDL_BUTTON(7) & bstate)
  {
	  mouse_buttonstate |= (1 << 4);
  }

  /* Grab and ungrab the mouse if the
     console is opened */
  if (in_grab->value == 2)
  {
	  if (old_windowed_mouse != windowed_mouse->value) 
	  {
		  old_windowed_mouse = windowed_mouse->value;

		  if (!windowed_mouse->value) 
		  {
			  SDL_WM_GrabInput(SDL_GRAB_OFF);
		  } 
		  else 
		  {
			  SDL_WM_GrabInput(SDL_GRAB_ON);
		  }
	  }
  }
  else if (in_grab->value == 1)
  {
	  SDL_WM_GrabInput(SDL_GRAB_ON);
  }
  else
  {
	  SDL_WM_GrabInput(SDL_GRAB_OFF);
  }

  /* Process the key events */
  while (keyq_head != keyq_tail)
  {
	  in_state->Key_Event_fp(keyq[keyq_tail].key, keyq[keyq_tail].down);
	  keyq_tail = (keyq_tail + 1) & 63;
  }

  IN_Update_Flag = 0;
}

/* 
 * Closes all inputs and clears
 * the input queue.
 */
void IN_Close(void)
{
	keyq_head = 0;
	keyq_tail = 0;
	
	memset(keyq, 0, sizeof(keyq));
}

/*
 * Gets the mouse state
 */
void IN_GetMouseState(int *x, int *y, int *state) {
  *x = mx;
  *y = my;
  *state = mouse_buttonstate;
} 

/*
 * Cleares the mouse state
 */
void IN_ClearMouseState() 
{
  mx = my = 0;
}
 
/*
 * Centers the view
 */
static void
IN_ForceCenterView ( void )
{
	in_state->viewangles [ PITCH ] = 0;
}  

/*
 * Look up
 */
static void
IN_MLookDown ( void )
{
	mlooking = true;
}

/*
 * Look down
 */
static void
IN_MLookUp ( void )
{
	mlooking = false;
	in_state->IN_CenterView_fp();
}

/*
 * Keyboard initialisation. Called
 * by the client via function pointer
 */
void
IN_KeyboardInit(Key_Event_fp_t fp)
{
	Key_Event_fp = fp;
}

/*
 * Initializes the backend
 */
void
IN_BackendInit ( in_state_t *in_state_p )
{
	in_state = in_state_p;
	m_filter = ri.Cvar_Get( "m_filter", "0", CVAR_ARCHIVE );
	in_mouse = ri.Cvar_Get( "in_mouse", "0", CVAR_ARCHIVE );

	freelook = ri.Cvar_Get( "freelook", "1", 0 );
	lookstrafe = ri.Cvar_Get( "lookstrafe", "0", 0 );
	sensitivity = ri.Cvar_Get( "sensitivity", "3", 0 );
	exponential_speedup = ri.Cvar_Get( "exponential_speedup", "0", CVAR_ARCHIVE );

	m_pitch = ri.Cvar_Get( "m_pitch", "0.022", 0 );
	m_yaw = ri.Cvar_Get( "m_yaw", "0.022", 0 );
	m_forward = ri.Cvar_Get( "m_forward", "1", 0 );
	m_side = ri.Cvar_Get( "m_side", "0.8", 0 );

	ri.Cmd_AddCommand( "+mlook", IN_MLookDown );
	ri.Cmd_AddCommand( "-mlook", IN_MLookUp );
	ri.Cmd_AddCommand( "force_centerview", IN_ForceCenterView );

	mouse_x = mouse_y = 0.0;

	/* SDL stuff */
	SDL_EnableUNICODE( 0 );
    SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );

	windowed_mouse = ri.Cvar_Get ("windowed_mouse", "1", CVAR_ARCHIVE);
	in_grab = ri.Cvar_Get ("in_grab", "2", CVAR_ARCHIVE);

	Com_Printf( "Input initialized.\n" );
}

/*
 * Shuts the backend down
 */
void
IN_BackendShutdown ( void )
{
	Com_Printf("Input shut down.\n");
}

/*
 * Mouse button handling
 */
void
IN_BackendMouseButtons ( void )
{
	int i;

	IN_GetMouseState( &mouse_x, &mouse_y, &mouse_buttonstate );

	for ( i = 0; i < 3; i++ )
	{
		if ( ( mouse_buttonstate & ( 1 << i ) ) && !( mouse_oldbuttonstate & ( 1 << i ) ) )
		{
			in_state->Key_Event_fp( K_MOUSE1 + i, true );
		}

		if ( !( mouse_buttonstate & ( 1 << i ) ) && ( mouse_oldbuttonstate & ( 1 << i ) ) )
		{
			in_state->Key_Event_fp( K_MOUSE1 + i, false );
		}
	}

	if ( ( mouse_buttonstate & ( 1 << 3 ) ) && !( mouse_oldbuttonstate & ( 1 << 3 ) ) )
	{
		in_state->Key_Event_fp( K_MOUSE4, true );
	}

	if ( !( mouse_buttonstate & ( 1 << 3 ) ) && ( mouse_oldbuttonstate & ( 1 << 3 ) ) )
	{
		in_state->Key_Event_fp( K_MOUSE4, false );
	}

	if ( ( mouse_buttonstate & ( 1 << 4 ) ) && !( mouse_oldbuttonstate & ( 1 << 4 ) ) )
	{
		in_state->Key_Event_fp( K_MOUSE5, true );
	}

	if ( !( mouse_buttonstate & ( 1 << 4 ) ) && ( mouse_oldbuttonstate & ( 1 << 4 ) ) )
	{
		in_state->Key_Event_fp( K_MOUSE5, false );
	}

	mouse_oldbuttonstate = mouse_buttonstate;
}

/*
 * Move handling
 */
void
IN_BackendMove ( usercmd_t *cmd )
{
	IN_GetMouseState( &mouse_x, &mouse_y, &mouse_buttonstate );
	
	if ( m_filter->value )
	{
		if ( ( mouse_x > 1 ) || ( mouse_x < -1) )
		{
			mouse_x = ( mouse_x + old_mouse_x ) * 0.5;
		}
		if ( ( mouse_y > 1 ) || ( mouse_y < -1) )
		{
			mouse_y = ( mouse_y + old_mouse_y ) * 0.5;
		}
	}

	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

	if ( mouse_x || mouse_y )
	{
		if ( !exponential_speedup->value )
		{
			mouse_x *= sensitivity->value;
			mouse_y *= sensitivity->value;
		}
		else
		{
			if ( ( mouse_x > MOUSE_MIN ) || ( mouse_y > MOUSE_MIN ) ||
					( mouse_x < -MOUSE_MIN ) || ( mouse_y < -MOUSE_MIN ) )
			{
				mouse_x = ( mouse_x * mouse_x * mouse_x ) / 4;
				mouse_y = ( mouse_y * mouse_y * mouse_y ) / 4;

				if ( mouse_x > MOUSE_MAX )
				{
					mouse_x = MOUSE_MAX;
				}
				else if ( mouse_x < -MOUSE_MAX )
				{
					mouse_x = -MOUSE_MAX;
				}

				if ( mouse_y > MOUSE_MAX )
				{
					mouse_y = MOUSE_MAX;
				}
				else if ( mouse_y < -MOUSE_MAX )
				{
					mouse_y = -MOUSE_MAX;
				}
			}
		}

		/* add mouse X/Y movement to cmd */
		if ( ( *in_state->in_strafe_state & 1 ) ||
				( lookstrafe->value && mlooking ) )
		{
			cmd->sidemove += m_side->value * mouse_x;
		}
		else
		{
			in_state->viewangles [ YAW ] -= m_yaw->value * mouse_x;
		}

		if ( ( mlooking || freelook->value ) &&
				!( *in_state->in_strafe_state & 1 ) )
		{
			in_state->viewangles [ PITCH ] += m_pitch->value * mouse_y;
		}
		else
		{
			cmd->forwardmove -= m_forward->value * mouse_y;
		}

		IN_ClearMouseState();
	}
}

