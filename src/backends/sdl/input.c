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
static int joystick_yaw, joystick_pitch;
static int joystick_forwardmove, joystick_sidemove;
static int joystick_up;
static int old_mouse_x, old_mouse_y;
static char last_hat = SDL_HAT_CENTERED;
static qboolean mlooking;

static SDL_HapticEffect haptic_click_effect;
static int haptic_click_effect_id = -1;
static SDL_HapticEffect haptic_menu_effect;
static int haptic_menu_effect_id = -1;
static SDL_Haptic *joystick_haptic = NULL;

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
cvar_t *m_up;
cvar_t *m_yaw;
cvar_t *sensitivity;
static cvar_t *windowed_mouse;
/* Joystick sensitivity */
cvar_t *joy_sensitivity_yaw;
cvar_t *joy_sensitivity_pitch;
cvar_t *joy_sensitivity_forwardmove;
cvar_t *joy_sensitivity_sidemove;
cvar_t *joy_sensitivity_up;
/* Joystick direction settings */
cvar_t *joy_axis_leftx;
cvar_t *joy_axis_lefty;
cvar_t *joy_axis_rightx;
cvar_t *joy_axis_righty;
cvar_t *joy_axis_triggerleft;
cvar_t *joy_axis_triggerright;
cvar_t *joy_harpic_level;

extern void GLimp_GrabInput(qboolean grab);

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

extern int glimp_refreshRate;

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
				if(event.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
						event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
				{
					Key_MarkAllUp();
				}
				else if(event.window.event == SDL_WINDOWEVENT_MOVED)
				{
					// make sure GLimp_GetRefreshRate() will query from SDL again - the window might
					// be on another display now!
					glimp_refreshRate = -1;
				}

#else /* SDL1.2 */
			case SDL_ACTIVEEVENT:
				if(event.active.gain == 0 && (event.active.state & SDL_APPINPUTFOCUS))
				{
					Key_MarkAllUp();
				}
#endif
				break;
#if SDL_VERSION_ATLEAST(2, 0, 0)
			case SDL_CONTROLLERAXISMOTION:  /* Handle Controller Motion */
				if (cls.key_dest == key_game && (int)cl_paused->value == 0) {
					char* direction_type;
					switch (event.caxis.axis)
					{
						/* left/right */
						case SDL_CONTROLLER_AXIS_LEFTX:
							direction_type = joy_axis_leftx->string;
							break;
						/* top/bottom */
						case SDL_CONTROLLER_AXIS_LEFTY:
							direction_type = joy_axis_lefty->string;
							break;
						/* second left/right */
						case SDL_CONTROLLER_AXIS_RIGHTX:
							direction_type = joy_axis_rightx->string;
							break;
						/* second top/bottom */
						case SDL_CONTROLLER_AXIS_RIGHTY:
							direction_type = joy_axis_righty->string;
							break;
						case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
							direction_type = joy_axis_triggerleft->string;
							break;
						case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
							direction_type = joy_axis_triggerright->string;
							break;
						default:
							direction_type = "none";
					}

					if (strcmp(direction_type, "sidemove") == 0)
					{
						joystick_sidemove = event.caxis.value * joy_sensitivity_sidemove->value;
						joystick_sidemove *= cl_sidespeed->value;
					}
					else if (strcmp(direction_type, "forwardmove") == 0)
					{
						joystick_forwardmove = event.caxis.value * joy_sensitivity_forwardmove->value;
						joystick_forwardmove *= cl_forwardspeed->value;
					}
					else if (strcmp(direction_type, "yaw") == 0)
					{
						joystick_yaw = event.caxis.value * joy_sensitivity_yaw->value;
						joystick_yaw *= cl_yawspeed->value;
					}
					else if (strcmp(direction_type, "pitch") == 0)
					{
						joystick_pitch = event.caxis.value * joy_sensitivity_pitch->value;
						joystick_pitch *= cl_pitchspeed->value;
					}
					else if (strcmp(direction_type, "updown") == 0)
					{
						joystick_up = event.caxis.value * joy_sensitivity_up->value;
						joystick_up *= cl_upspeed->value;
					}
				}
				break;
			/* Joystick can have more buttons than on general game controller
			 * so try to map not free buttons */
			case SDL_JOYBUTTONUP:
			case SDL_JOYBUTTONDOWN:
			{
				qboolean down = (event.type == SDL_JOYBUTTONDOWN);
				if(event.jbutton.button <= (K_JOY32 - K_JOY1)) {
					Key_Event(event.jbutton.button + K_JOY1, down, true);
				}
			}
				break;
			case SDL_JOYHATMOTION:
				if (last_hat != event.jhat.value)
				{
					char diff = last_hat ^ event.jhat.value;
					int i;
					for (i=0; i < 4; i++) {
						if (diff & (1 << i)) {
							/* check that we have button up for some bit */
							if (last_hat & (1 << i))
								Key_Event(i + K_HAT_UP, false, true);

							/* check that we have button down for some bit */
							if (event.jhat.value & (1 << i))
								Key_Event(i + K_HAT_UP, true, true);
						}
					}
					last_hat = event.jhat.value;
				}
				break;
#endif
			case SDL_QUIT:
				Com_Quit();

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
 * Removes all pending events from SDLs queue.
 */
void
In_FlushQueue(void)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
#else
	SDL_Event event;
	while(SDL_PollEvent(&event));
#endif

	Key_MarkAllUp();
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

	if (joystick_yaw)
	{
		cl.viewangles[YAW] -= (m_yaw->value * joystick_yaw) / 32768;
	}

	if(joystick_pitch)
	{
		cl.viewangles[PITCH] += (m_pitch->value * joystick_pitch) / 32768;
	}

	if (joystick_forwardmove)
	{
		cmd->forwardmove -= (m_forward->value * joystick_forwardmove) / 32768;
	}

	if (joystick_sidemove)
	{
		cmd->sidemove += (m_side->value * joystick_sidemove) / 32768;
	}

	if (joystick_up)
	{
		cmd->upmove -= (m_up->value * joystick_up) / 32768;
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
 * Init haptic effects
 */
static void
IN_Haptic_Effects_Init(void)
{
	if ((SDL_HapticQuery(joystick_haptic) & SDL_HAPTIC_SINE)==0)
		return;

	 // Create the effect
	SDL_memset(&haptic_click_effect, 0, sizeof(SDL_HapticEffect)); // 0 is safe default
	haptic_click_effect.type = SDL_HAPTIC_SINE;
	haptic_click_effect.periodic.direction.type = SDL_HAPTIC_POLAR; // Polar coordinates
	haptic_click_effect.periodic.direction.dir[0] = 27000; // Force comes from west
	haptic_click_effect.periodic.period = 1000; // 1000 ms
	haptic_click_effect.periodic.magnitude = 15000; // 15000/32767 strength
	haptic_click_effect.periodic.length = 200; // 0.2 seconds long
	haptic_click_effect.periodic.attack_length = 100; // Takes 0.1 second to get max strength
	haptic_click_effect.periodic.fade_length = 100; // Takes 0.1 second to fade away
	haptic_click_effect_id = SDL_HapticNewEffect(joystick_haptic, &haptic_click_effect);

	SDL_memset(&haptic_menu_effect, 0, sizeof(SDL_HapticEffect)); // 0 is safe default
	haptic_menu_effect.type = SDL_HAPTIC_SINE;
	haptic_menu_effect.periodic.direction.type = SDL_HAPTIC_POLAR; // Polar coordinates
	haptic_menu_effect.periodic.direction.dir[0] = 900; // Force comes from East
	haptic_menu_effect.periodic.period = 1000; // 1000 ms
	haptic_menu_effect.periodic.magnitude = 15000 * joy_harpic_level->value; // 15000/32767 strength
	haptic_menu_effect.periodic.length = 200; // 0.2 seconds long
	haptic_menu_effect.periodic.attack_length = 100; // Takes 0.1 second to get max strength
	haptic_menu_effect.periodic.fade_length = 100; // Takes 0.1 second to fade away
	haptic_menu_effect_id = SDL_HapticNewEffect(joystick_haptic, &haptic_menu_effect);
}

void
Haptic_Feedback(int type)
{
	int effect_id = -1;

	if (joy_harpic_level->value <= 0)
		return;

	switch(type)
	{
		case HARPIC_MENU_CLICK:
			effect_id = haptic_menu_effect_id;
			break;
		case HARPIC_CLICK:
			effect_id = haptic_click_effect_id;
			break;
	}

	if (effect_id >= 0 && joystick_haptic)
		SDL_HapticRunEffect(joystick_haptic, effect_id, 1);
}

/*
 * Initializes the backend
 */
void
IN_Init(void)
{
	Com_Printf("------- input initialization -------\n");

	mouse_x = mouse_y = 0;
	joystick_yaw = joystick_pitch = joystick_forwardmove = joystick_sidemove = 0;

	exponential_speedup = Cvar_Get("exponential_speedup", "0", CVAR_ARCHIVE);
	freelook = Cvar_Get("freelook", "1", 0);
	in_grab = Cvar_Get("in_grab", "2", CVAR_ARCHIVE);
	in_mouse = Cvar_Get("in_mouse", "0", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get("lookstrafe", "0", 0);
	m_filter = Cvar_Get("m_filter", "0", CVAR_ARCHIVE);
	m_up = Cvar_Get("m_up", "1", 0);
	m_forward = Cvar_Get("m_forward", "1", 0);
	m_pitch = Cvar_Get("m_pitch", "0.022", 0);
	m_side = Cvar_Get("m_side", "0.8", 0);
	m_yaw = Cvar_Get("m_yaw", "0.022", 0);
	sensitivity = Cvar_Get("sensitivity", "3", 0);

	joy_harpic_level = Cvar_Get("joy_harpic_level", "1.0", CVAR_ARCHIVE);

	joy_sensitivity_yaw = Cvar_Get("joy_sensitivity_yaw", "1.0", CVAR_ARCHIVE);
	joy_sensitivity_pitch = Cvar_Get("joy_sensitivity_pitch", "1.0", CVAR_ARCHIVE);
	joy_sensitivity_forwardmove = Cvar_Get("joy_sensitivity_forwardmove", "1.0", CVAR_ARCHIVE);
	joy_sensitivity_sidemove = Cvar_Get("joy_sensitivity_sidemove", "1.0", CVAR_ARCHIVE);
	joy_sensitivity_up = Cvar_Get("joy_sensitivity_up", "1.0", CVAR_ARCHIVE);

	joy_axis_leftx = Cvar_Get("joy_axis_leftx", "sidemove", CVAR_ARCHIVE);
	joy_axis_lefty = Cvar_Get("joy_axis_lefty", "forwardmove", CVAR_ARCHIVE);
	joy_axis_rightx = Cvar_Get("joy_axis_rightx", "yaw", CVAR_ARCHIVE);
	joy_axis_righty = Cvar_Get("joy_axis_righty", "pitch", CVAR_ARCHIVE);
	joy_axis_triggerleft = Cvar_Get("joy_axis_triggerleft", "updown", CVAR_ARCHIVE);
	joy_axis_triggerright = Cvar_Get("joy_axis_triggerright", "none", CVAR_ARCHIVE);

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

#if SDL_VERSION_ATLEAST(2, 0, 0)
	/* joystik init */
	if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC))
	{
		if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) == -1)
		{
			Com_Printf ("Couldn't init SDL joystick: %s.\n", SDL_GetError ());
		} else {
			Com_Printf ("%i joysticks were found.\n", SDL_NumJoysticks());
			if (SDL_NumJoysticks() > 0) {
				int i;
				for (i=0; i<SDL_NumJoysticks(); i ++) {
					SDL_Joystick *joystick = SDL_JoystickOpen(i);
					Com_Printf ("The name of the joystick is '%s'\n", SDL_JoystickName(joystick));
					Com_Printf ("Number of Axes: %d\n", SDL_JoystickNumAxes(joystick));
					Com_Printf ("Number of Buttons: %d\n", SDL_JoystickNumButtons(joystick));
					Com_Printf ("Number of Balls: %d\n", SDL_JoystickNumBalls(joystick));
					Com_Printf ("Number of Hats: %d\n", SDL_JoystickNumHats(joystick));

					joystick_haptic = SDL_HapticOpenFromJoystick(joystick);
					if (joystick_haptic == NULL)
						Com_Printf ("Most likely joystick isn't haptic\n");
					else
					{
						Com_Printf ("Supported %d effects\n", SDL_HapticNumEffects(joystick_haptic));
						Com_Printf ("Supported %d effects in same time\n", SDL_HapticNumEffectsPlaying(joystick_haptic));
						Com_Printf ("Supported by %d axis\n", SDL_HapticNumAxes(joystick_haptic));
						IN_Haptic_Effects_Init();
					}

					if(SDL_IsGameController(i))
					{
						SDL_GameController *controller;
						controller = SDL_GameControllerOpen(i);
						Com_Printf ("Controller settings: %s\n", SDL_GameControllerMapping(controller));
						Com_Printf ("Controller axis leftx = %s\n", joy_axis_leftx->string);
						Com_Printf ("Controller axis lefty = %s\n", joy_axis_lefty->string);
						Com_Printf ("Controller axis rightx = %s\n", joy_axis_rightx->string);
						Com_Printf ("Controller axis righty = %s\n", joy_axis_righty->string);
						Com_Printf ("Controller axis triggerleft = %s\n", joy_axis_triggerleft->string);
						Com_Printf ("Controller axis triggerright = %s\n", joy_axis_triggerright->string);

						break;
					} else {
						char joystick_guid[256] = {0};
						SDL_JoystickGUID guid;
						guid = SDL_JoystickGetDeviceGUID(i);
						SDL_JoystickGetGUIDString(guid, joystick_guid, 255);
						Com_Printf ("For use joystic as game contoller please set SDL_GAMECONTROLLERCONFIG:\n");
						Com_Printf ("e.g.: SDL_GAMECONTROLLERCONFIG='%s,%s,leftx:a0,lefty:a1,rightx:a2,righty:a3,...\n", joystick_guid, SDL_JoystickName(joystick));
					}
				}
			}
		}
	}
#endif
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

	if (joystick_haptic)
		SDL_HapticDestroyEffect(joystick_haptic, haptic_click_effect_id);
}

/* ------------------------------------------------------------------ */

