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
 * This is the Quake II input system backend, implemented with SDL.
 *
 * =======================================================================
 */

#include "../../client/refresh/header/local.h"
#include "../../client/header/keyboard.h"
#include "../generic/header/input.h"
#include "../../client/header/client.h"

/* There's no sdl-config on OS X and Windows */
#ifdef SDL2
#include <SDL2/SDL.h>
#else /* SDL1.2 */
#include <SDL/SDL.h>
#endif /*SDL2 */

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
static int mouse_x, mouse_y;
static int old_mouse_x, old_mouse_y;
static qboolean mlooking;

/* CVars */
cvar_t *vid_fullscreen;
static cvar_t *in_grab;
static cvar_t *in_mouse;
static cvar_t *exponential_speedup;
cvar_t *freelook;
cvar_t *lookstrafe;
cvar_t *m_forward;
static cvar_t *m_filter;
cvar_t *m_pitch;
cvar_t *m_side;
cvar_t *m_yaw;
cvar_t *sensitivity;
static cvar_t *windowed_mouse;

/* ------------------------------------------------------------------ */

/*
 * This creepy function translates SDL keycodes into
 * the id Tech 2 engines interal representation.
 */
static int
IN_TranslateSDLtoQ2Key(unsigned int keysym)
{
	int key = 0;

	/* These must be translated */
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

	return key;
}

/* ------------------------------------------------------------------ */

/*
 * Updates the input queue state. Called every
 * frame by the client and does nearly all the
 * input magic.
 */
void
IN_Update(void)
{
	qboolean want_grab;
	SDL_Event event;
 	unsigned int key;

	/* Get and process an event */
	while (SDL_PollEvent(&event))
	{

		switch (event.type)
		{
#if SDL_VERSION_ATLEAST(2, 0, 0)
			case SDL_MOUSEWHEEL:
				Key_Event((event.wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), true, true);
				Key_Event((event.wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), false, true);
				break;
#endif
			case SDL_MOUSEBUTTONDOWN:
#if !SDL_VERSION_ATLEAST(2, 0, 0)
				if (event.button.button == 4)
				{
					Key_Event(K_MWHEELUP, true, true);
					Key_Event(K_MWHEELUP, false, true);
					break;
				}
				else if (event.button.button == 5)
				{
					Key_Event(K_MWHEELDOWN, true, true);
					Key_Event(K_MWHEELDOWN, false, true);
					break;
				}
#endif
				/* fall-through */
			case SDL_MOUSEBUTTONUP:
				switch( event.button.button )
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
					default:
						return;
				}

				Key_Event(key, (event.type == SDL_MOUSEBUTTONDOWN), true);
				break;

			case SDL_MOUSEMOTION:
                if (cls.key_dest == key_game && (int)cl_paused->value == 0) {
                    mouse_x += event.motion.xrel;
                    mouse_y += event.motion.yrel;
                }
				break;

#if SDL_VERSION_ATLEAST(2, 0, 0)
			case SDL_TEXTINPUT:
				if((event.text.text[0] >= ' ') &&
				   (event.text.text[0] <= '~'))
				{
					Char_Event(event.text.text[0]);
				}

				break;
#endif

			case SDL_KEYDOWN:
#if !SDL_VERSION_ATLEAST(2, 0, 0)
				if ((event.key.keysym.unicode >= SDLK_SPACE) &&
					 (event.key.keysym.unicode < SDLK_DELETE))
				{
					Char_Event(event.key.keysym.unicode);
				}
#endif
				/* fall-through */
			case SDL_KEYUP:
			{
				qboolean down = (event.type == SDL_KEYDOWN);

#if SDL_VERSION_ATLEAST(2, 0, 0)
				/* workaround for AZERTY-keyboards, which don't have 1, 2, ..., 9, 0 in first row:
				 * always map those physical keys (scancodes) to those keycodes anyway
				 * see also https://bugzilla.libsdl.org/show_bug.cgi?id=3188 */
				SDL_Scancode sc = event.key.keysym.scancode;
				if(sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0)
				{
					/* Note that the SDL_SCANCODEs are SDL_SCANCODE_1, _2, ..., _9, SDL_SCANCODE_0
					 * while in ASCII it's '0', '1', ..., '9' => handle 0 and 1-9 separately
					 * (quake2 uses the ASCII values for those keys) */
					int key = '0'; /* implicitly handles SDL_SCANCODE_0 */
					if(sc <= SDL_SCANCODE_9)
					{
						key = '1' + (sc - SDL_SCANCODE_1);
					}
					Key_Event(key, down, false);
				}
				else
#endif /* SDL2; (SDL1.2 doesn't have scancodes so nothing we can do there) */
				   if((event.key.keysym.sym >= SDLK_SPACE) &&
				      (event.key.keysym.sym < SDLK_DELETE))
				{
					Key_Event(event.key.keysym.sym, down, false);
				}
				else
				{
					Key_Event(IN_TranslateSDLtoQ2Key(event.key.keysym.sym), down, true);
				}
			}
				break;

#if SDL_VERSION_ATLEAST(2, 0, 0)
			case SDL_WINDOWEVENT:
				if(event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
				{
					Key_MarkAllUp();
				}

#else /* SDL1.2 */
			case SDL_ACTIVEEVENT:
				if(event.active.gain == 0 && (event.active.state & SDL_APPINPUTFOCUS))
				{
					Key_MarkAllUp();
				}
#endif
				break;
		}
	}

	/* Grab and ungrab the mouse if the* console or the menu is opened */
	want_grab = (vid_fullscreen->value || in_grab->value == 1 ||
			(in_grab->value == 2 && windowed_mouse->value));
	/* calling GLimp_GrabInput() each is a but ugly but simple and should work.
	 * + the called SDL functions return after a cheap check, if there's
	 * nothing to do, anyway
	 */
	GLimp_GrabInput(want_grab);
}
 
/*
 * Move handling
 */
void
IN_Move(usercmd_t *cmd)
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
		if ((in_strafe.state & 1) || (lookstrafe->value && mlooking))
		{
			cmd->sidemove += m_side->value * mouse_x;
		}
		else
		{
			cl.viewangles[YAW] -= m_yaw->value * mouse_x;
		}

		if ((mlooking || freelook->value) && !(in_strafe.state & 1))
		{
			cl.viewangles[PITCH] += m_pitch->value * mouse_y;
		}
		else
		{
			cmd->forwardmove -= m_forward->value * mouse_y;
		}

		mouse_x = mouse_y = 0;
	}
}
 
/* ------------------------------------------------------------------ */

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
	IN_CenterView();
}

/* ------------------------------------------------------------------ */

/*
 * Initializes the backend
 */
void
IN_Init(void)
{
	Com_Printf("------- input initialization -------\n");

	mouse_x = mouse_y = 0;

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

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_StartTextInput();
#else
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#endif

	Com_Printf("------------------------------------\n\n");
}

/*
 * Shuts the backend down
 */
void
IN_Shutdown(void)
{
	Cmd_RemoveCommand("force_centerview");
	Cmd_RemoveCommand("+mlook");
	Cmd_RemoveCommand("-mlook");

    Com_Printf("Shutting down input.\n");
}

/* ------------------------------------------------------------------ */

