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
 * This is the Quake II input system backend, written in SDL.
 *
 * =======================================================================
 */

#include "../../client/refresh/header/local.h"
#include "../../client/header/keyboard.h"
#include "../generic/header/input.h"

/* There's no sdl-config on OS X and Windows */
#if defined(_WIN32) || defined(__APPLE__)
#ifdef SDL2
#include <SDL2/SDL.h>
#else /* SDL1.2 */
#include <SDL/SDL.h>
#endif /*SDL2 */
#else /* not _WIN32 || APPLE */
#include <SDL.h>
#endif /* _WIN32 || APPLE */

/* SDL 1.2 <-> 2.0 compatiblity cruft */
#if SDL_VERSION_ATLEAST(2, 0, 0)
	#define SDLK_KP0 SDLK_KP_0
	#define SDLK_KP1 SDLK_KP_1
	#define SDLK_KP2 SDLK_KP_2
	#define SDLK_KP3 SDLK_KP_3
	#define SDLK_KP4 SDLK_KP_4
	#define SDLK_KP5 SDLK_KP_5
	#define SDLK_KP6 SDLK_KP_6
	#define SDLK_KP7 SDLK_KP_7
	#define SDLK_KP8 SDLK_KP_8
	#define SDLK_KP9 SDLK_KP_9

	#define SDLK_RMETA SDLK_RGUI
	#define SDLK_LMETA SDLK_LGUI

	#define SDLK_COMPOSE SDLK_APPLICATION

	#define SDLK_PRINT SDLK_PRINTSCREEN
	#define SDLK_SCROLLOCK SDLK_SCROLLLOCK
	#define SDLK_NUMLOCK SDLK_NUMLOCKCLEAR
#endif

#define MOUSE_MAX 3000
#define MOUSE_MIN 40
 
/* Globals */
Key_Event_fp_t Key_Event_fp;
static in_state_t *in_state;
static int mouse_x, mouse_y;
static int old_mouse_x, old_mouse_y;
static qboolean have_grab;
static qboolean mlooking;

/* CVars */
cvar_t *vid_fullscreen;
static cvar_t *in_grab;
static cvar_t *in_mouse;
static cvar_t *exponential_speedup;
static cvar_t *freelook;
static cvar_t *lookstrafe;
static cvar_t *m_forward;
static cvar_t *m_filter;
static cvar_t *m_pitch;
static cvar_t *m_side;
static cvar_t *m_yaw;
static cvar_t *sensitivity;
static cvar_t *windowed_mouse;

/* Key queue */
struct
{
	int key;
	int down;
} keyq[128];

int keyq_head = 0;
int keyq_tail = 0; 

/*
 * This creepy function translates the SDL 
 * keycodes to the internal key representation
 * of the id Tech 2 engine.
 */
static int
IN_TranslateSDLtoQ2Key(unsigned int keysym)
{
	int key = 0;

	if ((keysym >= SDLK_SPACE) && (keysym < SDLK_DELETE))
	{
		/* These happen to match
		   the ASCII chars */
		key = (int)keysym;
	}
	else
	{
		switch (keysym)
		{
			case SDLK_PAGEUP:
				key = K_PGUP;
				break;
			case SDLK_KP9:
				key = K_KP_PGUP;
				break;
			case SDLK_PAGEDOWN:
				key = K_PGDN;
				break;
			case SDLK_KP3:
				key = K_KP_PGDN;
				break;
			case SDLK_KP7:
				key = K_KP_HOME;
				break;
			case SDLK_HOME:
				key = K_HOME;
				break;
			case SDLK_KP1:
				key = K_KP_END;
				break;
			case SDLK_END:
				key = K_END;
				break;
			case SDLK_KP4:
				key = K_KP_LEFTARROW;
				break;
			case SDLK_LEFT:
				key = K_LEFTARROW;
				break;
			case SDLK_KP6:
				key = K_KP_RIGHTARROW;
				break;
			case SDLK_RIGHT:
				key = K_RIGHTARROW;
				break;
			case SDLK_KP2:
				key = K_KP_DOWNARROW;
				break;
			case SDLK_DOWN:
				key = K_DOWNARROW;
				break;
			case SDLK_KP8:
				key = K_KP_UPARROW;
				break;
			case SDLK_UP:
				key = K_UPARROW;
				break;
			case SDLK_ESCAPE:
				key = K_ESCAPE;
				break;
			case SDLK_KP_ENTER:
				key = K_KP_ENTER;
				break;
			case SDLK_RETURN:
				key = K_ENTER;
				break;
			case SDLK_TAB:
				key = K_TAB;
				break;
			case SDLK_F1:
				key = K_F1;
				break;
			case SDLK_F2:
				key = K_F2;
				break;
			case SDLK_F3:
				key = K_F3;
				break;
			case SDLK_F4:
				key = K_F4;
				break;
			case SDLK_F5:
				key = K_F5;
				break;
			case SDLK_F6:
				key = K_F6;
				break;
			case SDLK_F7:
				key = K_F7;
				break;
			case SDLK_F8:
				key = K_F8;
				break;
			case SDLK_F9:
				key = K_F9;
				break;
			case SDLK_F10:
				key = K_F10;
				break;
			case SDLK_F11:
				key = K_F11;
				break;
			case SDLK_F12:
				key = K_F12;
				break;
			case SDLK_F13:
				key = K_F13;
				break;
			case SDLK_F14:
				key = K_F14;
				break;
			case SDLK_F15:
				key = K_F15;
				break;
			case SDLK_BACKSPACE:
				key = K_BACKSPACE;
				break;
			case SDLK_KP_PERIOD:
				key = K_KP_DEL;
				break;
			case SDLK_DELETE:
				key = K_DEL;
				break;
			case SDLK_PAUSE:
				key = K_PAUSE;
				break;
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
				key = K_SHIFT;
				break;
			case SDLK_LCTRL:
			case SDLK_RCTRL:
				key = K_CTRL;
				break;
			case SDLK_RMETA:
			case SDLK_LMETA:
				key = K_COMMAND;
				break;
			case SDLK_RALT:
			case SDLK_LALT:
				key = K_ALT;
				break;
			case SDLK_KP5:
				key = K_KP_5;
				break;
			case SDLK_INSERT:
				key = K_INS;
				break;
			case SDLK_KP0:
				key = K_KP_INS;
				break;
			case SDLK_KP_MULTIPLY:
				key = K_KP_STAR;
				break;
			case SDLK_KP_PLUS:
				key = K_KP_PLUS;
				break;
			case SDLK_KP_MINUS:
				key = K_KP_MINUS;
				break;
			case SDLK_KP_DIVIDE:
				key = K_KP_SLASH;
				break;
			case SDLK_MODE:
				key = K_MODE;
				break;
			case SDLK_COMPOSE:
				key = K_COMPOSE;
				break;
			case SDLK_HELP:
				key = K_HELP;
				break;
			case SDLK_PRINT:
				key = K_PRINT;
				break;
			case SDLK_SYSREQ:
				key = K_SYSREQ;
				break;
			case SDLK_MENU:
				key = K_MENU;
				break;
			case SDLK_POWER:
				key = K_POWER;
				break;
			case SDLK_UNDO:
				key = K_UNDO;
				break;
			case SDLK_SCROLLOCK:
				key = K_SCROLLOCK;
				break;
			case SDLK_NUMLOCK:
				key = K_KP_NUMLOCK;
				break;
			case SDLK_CAPSLOCK:
				key = K_CAPSLOCK;
				break;

			default:
				break;
		}
	}

	return key;
}

/*
 * Synthesize up and down events for the
 * mousewheel. Quake II is unable to use
 * buttons wich do not generate events.
 */
static void
IN_AddMouseWheelEvents(int key)
{
	assert(key == K_MWHEELUP || key == K_MWHEELDOWN);

	/* Key down */
	keyq[keyq_head].key = key;
	keyq[keyq_head].down = true;
	keyq_head = (keyq_head + 1) & 127;

	/* Key up */
	keyq[keyq_head].key = key;
	keyq[keyq_head].down = false;
	keyq_head = (keyq_head + 1) & 127;
}

/*
 * Input event processing
 */
static void
IN_GetEvent(SDL_Event *event)
{
	unsigned int key;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Keymod modstate = SDL_GetModState();
#else
	SDLMod modstate = SDL_GetModState();
#endif

	switch (event->type)
	{
		/* The mouse wheel */
#if SDL_VERSION_ATLEAST(2, 0, 0)
		case SDL_MOUSEWHEEL:
			IN_AddMouseWheelEvents(event->wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN);
			break;
#endif
		case SDL_MOUSEBUTTONDOWN:
#if ! SDL_VERSION_ATLEAST(2, 0, 0) // SDL1.2 mousewheel stuff
			if (event->button.button == 4)
			{
				IN_AddMouseWheelEvents(K_MWHEELUP);
				break;
			}
			else if (event->button.button == 5)
			{
				IN_AddMouseWheelEvents(K_MWHEELDOWN);
				break;
			}
#endif
			// fall-through
		case SDL_MOUSEBUTTONUP:
			// DG: luckily, we don't need that IN_MouseEvent() magic with SDL,
			//     as it really sends one event per pressed/released button
			switch( event->button.button )
			{
				case SDL_BUTTON_LEFT:
					key = K_MOUSE1;
					break;
				case SDL_BUTTON_MIDDLE:
					key = K_MOUSE3;
					break;
				case SDL_BUTTON_RIGHT:
					key = K_MOUSE2;
					break;
				case SDL_BUTTON_X1:
					key = K_MOUSE4;
					break;
				case SDL_BUTTON_X2:
					key = K_MOUSE5;
					break;
				default: // WTF, unknown mousebutton
					// TODO: print warning?
					return;
			}

			keyq[keyq_head].key = key;
			keyq[keyq_head].down = event->type == SDL_MOUSEBUTTONDOWN;
			keyq_head = (keyq_head + 1) & 127;

			break;

		/* The user pressed a button */
		case SDL_KEYDOWN:

			/* Fullscreen switch via Alt-Return */
			if ((modstate & KMOD_ALT) && (event->key.keysym.sym == SDLK_RETURN))
			{
				GLimp_ToggleFullscreen();
				break;
			}

			/* Make Shift+Escape toggle the console. This
			   really belongs in Key_Event(), but since
			   Key_ClearStates() can mess up the internal
			   K_SHIFT state let's do it here instead. */
			if ((modstate & KMOD_SHIFT) && (event->key.keysym.sym == SDLK_ESCAPE))
			{
				Cbuf_ExecuteText(EXEC_NOW, "toggleconsole");
				break;
			}

			/* Get the pressed key and add it to the key list */
			key = IN_TranslateSDLtoQ2Key(event->key.keysym.sym);

			if (key)
			{
				keyq[keyq_head].key = key;
				keyq[keyq_head].down = true;
				keyq_head = (keyq_head + 1) & 127;
			}

			break;

		/* The user released a key */
		case SDL_KEYUP:

			/* Get the pressed key and remove it from the key list */
			key = IN_TranslateSDLtoQ2Key(event->key.keysym.sym);

			if (key)
			{
				keyq[keyq_head].key = key;
				keyq[keyq_head].down = false;
				keyq_head = (keyq_head + 1) & 127;
			}

			break;
	}
}

/*
 * Updates the input queue state
 */
void
IN_Update(void)
{
	qboolean want_grab;
	SDL_Event event;
	static int protection;

	/* Protection against multiple calls.
	   In theory this should neber trigger. */ 
	if (protection == 1)
	{
		return;
	}
	else
	{
		protection = 1;
	}

	/* Get and process an event */
	while (SDL_PollEvent(&event))
	{
		IN_GetEvent(&event);
	}

	/* Get new mouse coordinates */
	if (!mouse_x && !mouse_y)
	{
		SDL_GetRelativeMouseState(&mouse_x, &mouse_y);
	}

	/* Grab and ungrab the mouse if the
	 * console or the menu is opened */
	want_grab = (vid_fullscreen->value || in_grab->value == 1 ||
			(in_grab->value == 2 && windowed_mouse->value));

	if (have_grab != want_grab)
	{
		GLimp_GrabInput(want_grab);
		have_grab = want_grab;
	}

	/* Process the key events */
	while (keyq_head != keyq_tail)
	{
		in_state->Key_Event_fp(keyq[keyq_tail].key, keyq[keyq_tail].down);
		keyq_tail = (keyq_tail + 1) & 127;
	}

	protection = 0;
}

/*
 * Closes all inputs and 
 * clears the input queue.
 */
void
IN_Close(void)
{
	keyq_head = 0;
	keyq_tail = 0;

	memset(keyq, 0, sizeof(keyq));
}

/*
 * Centers the view
 */
static void
IN_ForceCenterView(void)
{
	in_state->viewangles[PITCH] = 0;
}

/*
 * Look down
 */
static void
IN_MLookDown(void)
{
	mlooking = true;
}

/*
 * Look up
 */
static void
IN_MLookUp(void)
{
	mlooking = false;
	in_state->IN_CenterView_fp();
}

/*
 * Keyboard initialisation. Called by the client.
 */
void
IN_KeyboardInit(Key_Event_fp_t fp)
{
	Key_Event_fp = fp;

	/* SDL stuff. Moved here from IN_BackendInit because
	   this must be done after video is initialized. */
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_SetRelativeMouseMode(SDL_TRUE);
	have_grab = GLimp_InputIsGrabbed();
#else
	SDL_EnableUNICODE(0);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	have_grab = (SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON);
#endif
}

/*
 * Initializes the backend
 */
void
IN_BackendInit(in_state_t *in_state_p)
{
	in_state = in_state_p;

	exponential_speedup = Cvar_Get("exponential_speedup", "0", CVAR_ARCHIVE);
	freelook = Cvar_Get("freelook", "1", 0);
	in_grab = Cvar_Get("in_grab", "2", CVAR_ARCHIVE);
	in_mouse = Cvar_Get("in_mouse", "0", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get("lookstrafe", "0", 0);
	m_filter = Cvar_Get("m_filter", "0", CVAR_ARCHIVE);
	m_forward = Cvar_Get("m_forward", "1", 0);
	m_pitch = Cvar_Get("m_pitch", "0.022", 0);
	m_side = Cvar_Get("m_side", "0.8", 0);
	m_yaw = Cvar_Get("m_yaw", "0.022", 0);
	sensitivity = Cvar_Get("sensitivity", "3", 0);
	vid_fullscreen = Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
	windowed_mouse = Cvar_Get("windowed_mouse", "1", CVAR_USERINFO | CVAR_ARCHIVE);

	Cmd_AddCommand("+mlook", IN_MLookDown);
	Cmd_AddCommand("-mlook", IN_MLookUp);
	Cmd_AddCommand("force_centerview", IN_ForceCenterView);

	mouse_x = mouse_y = 0;

	VID_Printf(PRINT_ALL, "Input initialized.\n");
}

/*
 * Shuts the backend down
 */
void
IN_BackendShutdown(void)
{
	Cmd_RemoveCommand("force_centerview");
	Cmd_RemoveCommand("+mlook");
	Cmd_RemoveCommand("-mlook");

	VID_Printf(PRINT_ALL, "Input shut down.\n");
}

/*
 * Mouse button handling
 */
void
IN_BackendMouseButtons(void)
{
	// nothing to do, we don't need this hack with SDL
	// the mouse events are generated directly in IN_GetEvent()
}

/*
 * Move handling
 */
void
IN_BackendMove(usercmd_t *cmd)
{
	if (m_filter->value)
	{
		if ((mouse_x > 1) || (mouse_x < -1))
		{
			mouse_x = (mouse_x + old_mouse_x) * 0.5;
		}

		if ((mouse_y > 1) || (mouse_y < -1))
		{
			mouse_y = (mouse_y + old_mouse_y) * 0.5;
		}
	}

	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;

	if (mouse_x || mouse_y)
	{
		if (!exponential_speedup->value)
		{
			mouse_x *= sensitivity->value;
			mouse_y *= sensitivity->value;
		}
		else
		{
			if ((mouse_x > MOUSE_MIN) || (mouse_y > MOUSE_MIN) ||
				(mouse_x < -MOUSE_MIN) || (mouse_y < -MOUSE_MIN))
			{
				mouse_x = (mouse_x * mouse_x * mouse_x) / 4;
				mouse_y = (mouse_y * mouse_y * mouse_y) / 4;

				if (mouse_x > MOUSE_MAX)
				{
					mouse_x = MOUSE_MAX;
				}
				else if (mouse_x < -MOUSE_MAX)
				{
					mouse_x = -MOUSE_MAX;
				}

				if (mouse_y > MOUSE_MAX)
				{
					mouse_y = MOUSE_MAX;
				}
				else if (mouse_y < -MOUSE_MAX)
				{
					mouse_y = -MOUSE_MAX;
				}
			}
		}

		/* add mouse X/Y movement to cmd */
		if ((*in_state->in_strafe_state & 1) ||
			(lookstrafe->value && mlooking))
		{
			cmd->sidemove += m_side->value * mouse_x;
		}
		else
		{
			in_state->viewangles[YAW] -= m_yaw->value * mouse_x;
		}

		if ((mlooking || freelook->value) &&
			!(*in_state->in_strafe_state & 1))
		{
			in_state->viewangles[PITCH] += m_pitch->value * mouse_y;
		}
		else
		{
			cmd->forwardmove -= m_forward->value * mouse_y;
		}

		mouse_x = mouse_y = 0;
	}
}

