/*
 * Copyright (C) 2010 Yamagi Burmeister
 * Copyright (C) 1997-2005 Id Software, Inc.
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
 * Joystick threshold code is partially based on http://ioquake3.org code.
 *
 * =======================================================================
 *
 * This is the Quake II input system backend, implemented with SDL.
 *
 * =======================================================================
 */

#include <SDL2/SDL.h>

#include "header/input.h"
#include "../../client/header/keyboard.h"
#include "../../client/header/client.h"

// ----

// Maximal mouse move per frame
#define MOUSE_MAX 3000

// Minimal mouse move per frame
#define MOUSE_MIN 40

// ----

// These are used to communicate the events collected by
// IN_Update() called at the beginning of a frame to the
// actual movement functions called at a later time.
static float mouse_x, mouse_y;
static int sdl_back_button = SDL_CONTROLLER_BUTTON_BACK;
static float joystick_yaw, joystick_pitch;
static float joystick_forwardmove, joystick_sidemove;
static float joystick_up;
static qboolean mlooking;

// The last time input events were processed.
// Used throughout the client.
int sys_frame_time;

// the joystick altselector that turns K_BTN_X into K_BTN_X_ALT
// is pressed
qboolean joy_altselector_pressed = false;

// Console Variables
cvar_t *freelook;
cvar_t *lookstrafe;
cvar_t *m_forward;
cvar_t *m_pitch;
cvar_t *m_side;
cvar_t *m_up;
cvar_t *m_yaw;
cvar_t *sensitivity;

static cvar_t *exponential_speedup;
static cvar_t *in_grab;
static cvar_t *m_filter;
static cvar_t *windowed_mouse;

// ----

struct hapric_effects_cache {
	int effect_volume;
	int effect_duration;
	int effect_begin;
	int effect_end;
	int effect_attack;
	int effect_fade;
	int effect_id;
	int effect_x;
	int effect_y;
	int effect_z;
};

qboolean show_haptic;

static SDL_Haptic *joystick_haptic = NULL;
static SDL_GameController *controller = NULL;

#define HAPTIC_EFFECT_LIST_SIZE 16

static int last_haptic_volume = 0;
static int last_haptic_efffect_size = HAPTIC_EFFECT_LIST_SIZE;
static int last_haptic_efffect_pos = 0;
static struct hapric_effects_cache last_haptic_efffect[HAPTIC_EFFECT_LIST_SIZE];

// Joystick sensitivity
cvar_t *joy_yawsensitivity;
cvar_t *joy_pitchsensitivity;
cvar_t *joy_forwardsensitivity;
cvar_t *joy_sidesensitivity;
cvar_t *joy_upsensitivity;
cvar_t *joy_expo;

// Joystick direction settings
static cvar_t *joy_axis_leftx;
static cvar_t *joy_axis_lefty;
static cvar_t *joy_axis_rightx;
static cvar_t *joy_axis_righty;
static cvar_t *joy_axis_triggerleft;
static cvar_t *joy_axis_triggerright;

// Joystick threshold settings
static cvar_t *joy_axis_leftx_threshold;
static cvar_t *joy_axis_lefty_threshold;
static cvar_t *joy_axis_rightx_threshold;
static cvar_t *joy_axis_righty_threshold;
static cvar_t *joy_axis_triggerleft_threshold;
static cvar_t *joy_axis_triggerright_threshold;

// Joystick haptic
static cvar_t *joy_haptic_magnitude;

// Support for hot plugging of game controller
static qboolean first_init = true;
static int init_delay = 30;

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
		case SDLK_TAB:
			key = K_TAB;
			break;
		case SDLK_RETURN:
			key = K_ENTER;
			break;
		case SDLK_ESCAPE:
			key = K_ESCAPE;
			break;
		case SDLK_BACKSPACE:
			key = K_BACKSPACE;
			break;
		case SDLK_LGUI:
		case SDLK_RGUI:
			key = K_COMMAND; // Win key
			break;
		case SDLK_CAPSLOCK:
			key = K_CAPSLOCK;
			break;
		case SDLK_POWER:
			key = K_POWER;
			break;
		case SDLK_PAUSE:
			key = K_PAUSE;
			break;

		case SDLK_UP:
			key = K_UPARROW;
			break;
		case SDLK_DOWN:
			key = K_DOWNARROW;
			break;
		case SDLK_LEFT:
			key = K_LEFTARROW;
			break;
		case SDLK_RIGHT:
			key = K_RIGHTARROW;
			break;


		case SDLK_RALT:
		case SDLK_LALT:
			key = K_ALT;
			break;
		case SDLK_LCTRL:
		case SDLK_RCTRL:
			key = K_CTRL;
			break;
		case SDLK_LSHIFT:
		case SDLK_RSHIFT:
			key = K_SHIFT;
			break;
		case SDLK_INSERT:
			key = K_INS;
			break;
		case SDLK_DELETE:
			key = K_DEL;
			break;
		case SDLK_PAGEDOWN:
			key = K_PGDN;
			break;
		case SDLK_PAGEUP:
			key = K_PGUP;
			break;
		case SDLK_HOME:
			key = K_HOME;
			break;
		case SDLK_END:
			key = K_END;
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


		case SDLK_KP_7:
			key = K_KP_HOME;
			break;
		case SDLK_KP_8:
			key = K_KP_UPARROW;
			break;
		case SDLK_KP_9:
			key = K_KP_PGUP;
			break;
		case SDLK_KP_4:
			key = K_KP_LEFTARROW;
			break;
		case SDLK_KP_5:
			key = K_KP_5;
			break;
		case SDLK_KP_6:
			key = K_KP_RIGHTARROW;
			break;
		case SDLK_KP_1:
			key = K_KP_END;
			break;
		case SDLK_KP_2:
			key = K_KP_DOWNARROW;
			break;
		case SDLK_KP_3:
			key = K_KP_PGDN;
			break;
		case SDLK_KP_ENTER:
			key = K_KP_ENTER;
			break;
		case SDLK_KP_0:
			key = K_KP_INS;
			break;
		case SDLK_KP_PERIOD:
			key = K_KP_DEL;
			break;
		case SDLK_KP_DIVIDE:
			key = K_KP_SLASH;
			break;
		case SDLK_KP_MINUS:
			key = K_KP_MINUS;
			break;
		case SDLK_KP_PLUS:
			key = K_KP_PLUS;
			break;
		case SDLK_NUMLOCKCLEAR:
			key = K_KP_NUMLOCK;
			break;
		case SDLK_KP_MULTIPLY:
			key = K_KP_STAR;
			break;
		case SDLK_KP_EQUALS:
			key = K_KP_EQUALS;
			break;

		// TODO: K_SUPER ? Win Key is already K_COMMAND

		case SDLK_APPLICATION:
			key = K_COMPOSE;
			break;
		case SDLK_MODE:
			key = K_MODE;
			break;
		case SDLK_HELP:
			key = K_HELP;
			break;
		case SDLK_PRINTSCREEN:
			key = K_PRINT;
			break;
		case SDLK_SYSREQ:
			key = K_SYSREQ;
			break;
		case SDLK_SCROLLLOCK:
			key = K_SCROLLOCK;
			break;
		case SDLK_MENU:
			key = K_MENU;
			break;
		case SDLK_UNDO:
			key = K_UNDO;
			break;

		default:
			break;
	}

	return key;
}

static int
IN_TranslateScancodeToQ2Key(SDL_Scancode sc)
{

#define MY_SC_CASE(X) case SDL_SCANCODE_ ## X : return K_SC_ ## X;

	switch( (int)sc ) // cast to int to shut -Wswitch up
	{
		// case SDL_SCANCODE_A : return K_SC_A;
		MY_SC_CASE(A)
		MY_SC_CASE(B)
		MY_SC_CASE(C)
		MY_SC_CASE(D)
		MY_SC_CASE(E)
		MY_SC_CASE(F)
		MY_SC_CASE(G)
		MY_SC_CASE(H)
		MY_SC_CASE(I)
		MY_SC_CASE(J)
		MY_SC_CASE(K)
		MY_SC_CASE(L)
		MY_SC_CASE(M)
		MY_SC_CASE(N)
		MY_SC_CASE(O)
		MY_SC_CASE(P)
		MY_SC_CASE(Q)
		MY_SC_CASE(R)
		MY_SC_CASE(S)
		MY_SC_CASE(T)
		MY_SC_CASE(U)
		MY_SC_CASE(V)
		MY_SC_CASE(W)
		MY_SC_CASE(X)
		MY_SC_CASE(Y)
		MY_SC_CASE(Z)
		MY_SC_CASE(MINUS)
		MY_SC_CASE(EQUALS)
		MY_SC_CASE(LEFTBRACKET)
		MY_SC_CASE(RIGHTBRACKET)
		MY_SC_CASE(BACKSLASH)
		MY_SC_CASE(NONUSHASH)
		MY_SC_CASE(SEMICOLON)
		MY_SC_CASE(APOSTROPHE)
		MY_SC_CASE(GRAVE)
		MY_SC_CASE(COMMA)
		MY_SC_CASE(PERIOD)
		MY_SC_CASE(SLASH)
		MY_SC_CASE(NONUSBACKSLASH)
		MY_SC_CASE(INTERNATIONAL1)
		MY_SC_CASE(INTERNATIONAL2)
		MY_SC_CASE(INTERNATIONAL3)
		MY_SC_CASE(INTERNATIONAL4)
		MY_SC_CASE(INTERNATIONAL5)
		MY_SC_CASE(INTERNATIONAL6)
		MY_SC_CASE(INTERNATIONAL7)
		MY_SC_CASE(INTERNATIONAL8)
		MY_SC_CASE(INTERNATIONAL9)
		MY_SC_CASE(THOUSANDSSEPARATOR)
		MY_SC_CASE(DECIMALSEPARATOR)
		MY_SC_CASE(CURRENCYUNIT)
		MY_SC_CASE(CURRENCYSUBUNIT)
	}

#undef MY_SC_CASE

	return 0;
}

static int
IN_TranslateGamepadBtnToQ2Key(int btn)
{

#define MY_BTN_CASE(X,Y) case SDL_CONTROLLER_BUTTON_ ## X : return K_ ## Y;

	switch( btn )
	{
		// case SDL_CONTROLLER_BUTTON_A : return K_BTN_A;
		MY_BTN_CASE(A,BTN_A)
		MY_BTN_CASE(B,BTN_B)
		MY_BTN_CASE(X,BTN_X)
		MY_BTN_CASE(Y,BTN_Y)
		MY_BTN_CASE(LEFTSHOULDER,SHOULDER_LEFT)
		MY_BTN_CASE(RIGHTSHOULDER,SHOULDER_RIGHT)
		MY_BTN_CASE(LEFTSTICK,STICK_LEFT)
		MY_BTN_CASE(RIGHTSTICK,STICK_RIGHT)
		MY_BTN_CASE(DPAD_UP,DPAD_UP)
		MY_BTN_CASE(DPAD_DOWN,DPAD_DOWN)
		MY_BTN_CASE(DPAD_LEFT,DPAD_LEFT)
		MY_BTN_CASE(DPAD_RIGHT,DPAD_RIGHT)
#if SDL_VERSION_ATLEAST(2, 0, 14)	// support for newer buttons
		MY_BTN_CASE(PADDLE1,PADDLE_1)
		MY_BTN_CASE(PADDLE2,PADDLE_2)
		MY_BTN_CASE(PADDLE3,PADDLE_3)
		MY_BTN_CASE(PADDLE4,PADDLE_4)
		MY_BTN_CASE(MISC1,BTN_MISC1)
		MY_BTN_CASE(TOUCHPAD,TOUCHPAD)
#endif
		MY_BTN_CASE(BACK,BTN_BACK)
		MY_BTN_CASE(GUIDE,BTN_GUIDE)
		MY_BTN_CASE(START,BTN_START)
	}

#undef MY_BTN_CASE

	return 0;
}

static void IN_Controller_Init(qboolean notify_user);
static void IN_Controller_Shutdown(qboolean notify_user);

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

	static qboolean left_trigger = false;
	static qboolean right_trigger = false;

	static int consoleKeyCode = 0;

	/* Get and process an event */
	while (SDL_PollEvent(&event))
	{

		switch (event.type)
		{
			case SDL_MOUSEWHEEL:
				Key_Event((event.wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), true, true);
				Key_Event((event.wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), false, true);
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				switch (event.button.button)
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
				if (cls.key_dest == key_game && (int) cl_paused->value == 0)
				{
					mouse_x += event.motion.xrel;
					mouse_y += event.motion.yrel;
				}
				break;

			case SDL_TEXTINPUT:
			{
				int c = event.text.text[0];
				// also make sure we don't get the char that corresponds to the
				// "console key" (like "^" or "`") as text input
				if ((c >= ' ') && (c <= '~') && c != consoleKeyCode)
				{
					Char_Event(c);
				}
			}

				break;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
			{
				qboolean down = (event.type == SDL_KEYDOWN);

				/* workaround for AZERTY-keyboards, which don't have 1, 2, ..., 9, 0 in first row:
				 * always map those physical keys (scancodes) to those keycodes anyway
				 * see also https://bugzilla.libsdl.org/show_bug.cgi?id=3188 */
				SDL_Scancode sc = event.key.keysym.scancode;

				if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0)
				{
					/* Note that the SDL_SCANCODEs are SDL_SCANCODE_1, _2, ..., _9, SDL_SCANCODE_0
					 * while in ASCII it's '0', '1', ..., '9' => handle 0 and 1-9 separately
					 * (quake2 uses the ASCII values for those keys) */
					int key = '0'; /* implicitly handles SDL_SCANCODE_0 */

					if (sc <= SDL_SCANCODE_9)
					{
						key = '1' + (sc - SDL_SCANCODE_1);
					}

					Key_Event(key, down, false);
				}
				else
				{
					SDL_Keycode kc = event.key.keysym.sym;
					if(sc == SDL_SCANCODE_GRAVE && kc != '\'' && kc != '"')
					{
						// special case/hack: open the console with the "console key"
						// (beneath Esc, left of 1, above Tab)
						// but not if the keycode for this is a quote (like on Brazilian
						// keyboards) - otherwise you couldn't type them in the console
						if((event.key.keysym.mod & (KMOD_CAPS|KMOD_SHIFT|KMOD_ALT|KMOD_CTRL|KMOD_GUI)) == 0)
						{
							// also, only do this if no modifiers like shift or AltGr or whatever are pressed
							// so kc will most likely be the ascii char generated by this and can be ignored
							// in case SDL_TEXTINPUT above (so we don't get ^ or whatever as text in console)
							// (can't just check for mod == 0 because numlock is a KMOD too)
							Key_Event(K_CONSOLE, down, true);
							consoleKeyCode = kc;
						}
					}
					else if ((kc >= SDLK_SPACE) && (kc < SDLK_DELETE))
					{
						Key_Event(kc, down, false);
					}
					else
					{
						int key = IN_TranslateSDLtoQ2Key(kc);
						if(key == 0)
						{
							// fallback to scancodes if we don't know the keycode
							key = IN_TranslateScancodeToQ2Key(sc);
						}
						if(key != 0)
						{
							Key_Event(key, down, true);
						}
						else
						{
							Com_DPrintf("Pressed unknown key with SDL_Keycode %d, SDL_Scancode %d.\n", kc, (int)sc);
						}
					}
				}

				break;
			}

			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
					event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
				{
					Key_MarkAllUp();
				}
				else if (event.window.event == SDL_WINDOWEVENT_MOVED)
				{
					// make sure GLimp_GetRefreshRate() will query from SDL again - the window might
					// be on another display now!
					glimp_refreshRate = -1;
				}
				break;

			case SDL_CONTROLLERBUTTONUP:
			case SDL_CONTROLLERBUTTONDOWN:
			{
				qboolean down = (event.type == SDL_CONTROLLERBUTTONDOWN);

				// Handle Back Button first, to override its original key
				if (event.cbutton.button == sdl_back_button)
				{
					Key_Event(K_JOY_BACK, down, true);
					break;
				}

				key = IN_TranslateGamepadBtnToQ2Key(event.cbutton.button);
				if(key != 0)
				{
					Key_Event(key, down, true);
				}

				break;
			}

			case SDL_CONTROLLERAXISMOTION:  /* Handle Controller Motion */
			{
				char *direction_type;
				float threshold = 0;
				float fix_value = 0;
				int axis_value = event.caxis.value;

				switch (event.caxis.axis)
				{
					/* left/right */
					case SDL_CONTROLLER_AXIS_LEFTX:
						direction_type = joy_axis_leftx->string;
						threshold = joy_axis_leftx_threshold->value;
						break;

					/* top/bottom */
					case SDL_CONTROLLER_AXIS_LEFTY:
						direction_type = joy_axis_lefty->string;
						threshold = joy_axis_lefty_threshold->value;
						break;

					/* second left/right */
					case SDL_CONTROLLER_AXIS_RIGHTX:
						direction_type = joy_axis_rightx->string;
						threshold = joy_axis_rightx_threshold->value;
						break;

					/* second top/bottom */
					case SDL_CONTROLLER_AXIS_RIGHTY:
						direction_type = joy_axis_righty->string;
						threshold = joy_axis_righty_threshold->value;
						break;

					case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
						direction_type = joy_axis_triggerleft->string;
						threshold = joy_axis_triggerleft_threshold->value;
						break;

					case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
						direction_type = joy_axis_triggerright->string;
						threshold = joy_axis_triggerright_threshold->value;
						break;

					default:
						direction_type = "none";
				}

				if (threshold > 0.9)
				{
					threshold = 0.9;
				}

				if (axis_value < 0 && (axis_value > (32768 * threshold)))
				{
					axis_value = 0;
				}
				else if (axis_value > 0 && (axis_value < (32768 * threshold)))
				{
					axis_value = 0;
				}

				// Smoothly ramp from dead zone to maximum value (from ioquake)
				// https://github.com/ioquake/ioq3/blob/master/code/sdl/sdl_input.c
				fix_value = ((float) abs(axis_value) / 32767.0f - threshold) / (1.0f - threshold);

				if (fix_value < 0.0f)
				{
					fix_value = 0.0f;
				}

				// Apply expo
				fix_value = pow(fix_value, joy_expo->value);

				axis_value = (int) (32767 * ((axis_value < 0) ? -fix_value : fix_value));

				if (cls.key_dest == key_game && (int) cl_paused->value == 0)
				{
					if (strcmp(direction_type, "sidemove") == 0)
					{
						joystick_sidemove = axis_value * joy_sidesensitivity->value;

						// We need to be twice faster because with joystic we run...
						joystick_sidemove *= cl_sidespeed->value * 2.0f;
					}
					else if (strcmp(direction_type, "forwardmove") == 0)
					{
						joystick_forwardmove = axis_value * joy_forwardsensitivity->value;

						// We need to be twice faster because with joystic we run...
						joystick_forwardmove *= cl_forwardspeed->value * 2.0f;
					}
					else if (strcmp(direction_type, "yaw") == 0)
					{
						joystick_yaw = axis_value * joy_yawsensitivity->value;
						joystick_yaw *= cl_yawspeed->value;
					}
					else if (strcmp(direction_type, "pitch") == 0)
					{
						joystick_pitch = axis_value * joy_pitchsensitivity->value;
						joystick_pitch *= cl_pitchspeed->value;
					}
					else if (strcmp(direction_type, "updown") == 0)
					{
						joystick_up = axis_value * joy_upsensitivity->value;
						joystick_up *= cl_upspeed->value;
					}
				}

				if (strcmp(direction_type, "triggerleft") == 0)
				{
					qboolean new_left_trigger = abs(axis_value) > (32767 / 4);

					if (new_left_trigger != left_trigger)
					{
						left_trigger = new_left_trigger;
						Key_Event(K_TRIG_LEFT, left_trigger, true);
					}
				}
				else if (strcmp(direction_type, "triggerright") == 0)
				{
					qboolean new_right_trigger = abs(axis_value) > (32767 / 4);

					if (new_right_trigger != right_trigger)
					{
						right_trigger = new_right_trigger;
						Key_Event(K_TRIG_RIGHT, right_trigger, true);
					}
				}

				break;
			}

			case SDL_CONTROLLERDEVICEREMOVED:
				if (!controller)
				{
					break;
				}
				if (event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
					IN_Controller_Shutdown(true);
					IN_Controller_Init(false);
				}
				break;

			case SDL_JOYDEVICEADDED:
				if (!controller)
				{
					// This should be lower, but some controllers just don't want to get detected by the OS
					init_delay = 100;
				}
				break;

			case SDL_QUIT:
				Com_Quit();
				break;
		}
	}

	/* Grab and ungrab the mouse if the console or the menu is opened */
	if (in_grab->value == 3)
	{
		want_grab = windowed_mouse->value;
	}
	else
	{
		want_grab = (vid_fullscreen->value || in_grab->value == 1 ||
			(in_grab->value == 2 && windowed_mouse->value));
	}

	// calling GLimp_GrabInput() each frame is a bit ugly but simple and should work.
	// The called SDL functions return after a cheap check, if there's nothing to do.
	GLimp_GrabInput(want_grab);

	// We need to save the frame time so other subsystems
	// know the exact time of the last input events.
	sys_frame_time = Sys_Milliseconds();

	// Hot plugging delay handling, to not be "overwhelmed" because some controllers
	// present themselves as two different devices, triggering SDL_JOYDEVICEADDED
	// too many times. They could trigger it even at game initialization.
	if (init_delay)
	{
		init_delay--;
		if (!init_delay)
		{
			if (!first_init)
			{
				IN_Controller_Shutdown(false);
				IN_Controller_Init(true);
			}
			else
			{
				first_init = false;
			}
		}
	}
}

/*
 * Move handling
 */
void
IN_Move(usercmd_t *cmd)
{
	static float old_mouse_x;
	static float old_mouse_y;

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

		// add mouse X/Y movement to cmd
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

	// To make the the viewangles changes independent of framerate we need to scale
	// with frametime (assuming the configured values are for 60hz)
	//
	// 1/32768 is to normalize the input values from SDL (they're between -32768 and
	// 32768 and we want -1 to 1) for movement this is not needed, as those are
	// absolute values independent of framerate
	float joyViewFactor = (1.0f/32768.0f) * (cls.rframetime/0.01666f);

	if (joystick_yaw)
	{
		cl.viewangles[YAW] -= (m_yaw->value * joystick_yaw) * joyViewFactor;
	}

	if(joystick_pitch)
	{
		cl.viewangles[PITCH] += (m_pitch->value * joystick_pitch) * joyViewFactor;
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

static void
IN_JoyAltSelectorDown(void)
{
	joy_altselector_pressed = true;
}

static void
IN_JoyAltSelectorUp(void)
{
	joy_altselector_pressed = false;
}

/*
 * Removes all pending events from SDLs queue.
 */
void
In_FlushQueue(void)
{
	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
	Key_MarkAllUp();
	IN_JoyAltSelectorUp();
}

/* ------------------------------------------------------------------ */

static void IN_Haptic_Shutdown(void);

/*
 * Init haptic effects
 */
static int
IN_Haptic_Effect_Init(int effect_x, int effect_y, int effect_z,
				 int period, int magnitude,
				 int delay, int attack, int fade)
{
	static SDL_HapticEffect haptic_effect;

	/* limit magnitude */
	if (magnitude > SHRT_MAX)
	{
		magnitude = SHRT_MAX;
	}
	else if (magnitude < 0)
	{
		magnitude = 0;
	}

	SDL_memset(&haptic_effect, 0, sizeof(SDL_HapticEffect)); // 0 is safe default

	haptic_effect.type = SDL_HAPTIC_SINE;
	haptic_effect.periodic.direction.type = SDL_HAPTIC_CARTESIAN; // Cartesian/3d coordinates
	haptic_effect.periodic.direction.dir[0] = effect_x;
	haptic_effect.periodic.direction.dir[1] = effect_y;
	haptic_effect.periodic.direction.dir[2] = effect_z;
	haptic_effect.periodic.period = period;
	haptic_effect.periodic.magnitude = magnitude;
	haptic_effect.periodic.length = period;
	haptic_effect.periodic.delay = delay;
	haptic_effect.periodic.attack_length = attack;
	haptic_effect.periodic.fade_length = fade;

	int effect_id = SDL_HapticNewEffect(joystick_haptic, &haptic_effect);

	if (effect_id < 0)
	{
		Com_Printf ("SDL_HapticNewEffect failed: %s\n", SDL_GetError());
		Com_Printf ("Please try to rerun game. Effects will be disabled for now.\n");

		IN_Haptic_Shutdown();
	}

	return effect_id;
}

static void
IN_Haptic_Effects_Info(void)
{
	show_haptic = true;

	Com_Printf ("Joystick/Mouse haptic:\n");
	Com_Printf (" * %d effects\n", SDL_HapticNumEffects(joystick_haptic));
	Com_Printf (" * %d effects in same time\n", SDL_HapticNumEffectsPlaying(joystick_haptic));
	Com_Printf (" * %d haptic axis\n", SDL_HapticNumAxes(joystick_haptic));
}

static void
IN_Haptic_Effects_Init(void)
{
	last_haptic_efffect_size = SDL_HapticNumEffectsPlaying(joystick_haptic);

	if (last_haptic_efffect_size > HAPTIC_EFFECT_LIST_SIZE)
	{
		last_haptic_efffect_size = HAPTIC_EFFECT_LIST_SIZE;
	}

	for (int i=0; i<HAPTIC_EFFECT_LIST_SIZE; i++)
	{
		last_haptic_efffect[i].effect_id = -1;
		last_haptic_efffect[i].effect_volume = 0;
		last_haptic_efffect[i].effect_duration = 0;
		last_haptic_efffect[i].effect_begin = 0;
		last_haptic_efffect[i].effect_end = 0;
		last_haptic_efffect[i].effect_attack = 0;
		last_haptic_efffect[i].effect_fade = 0;
		last_haptic_efffect[i].effect_x = 0;
		last_haptic_efffect[i].effect_y = 0;
		last_haptic_efffect[i].effect_z = 0;
	}
}

/*
 * Shuts the backend down
 */
static void
IN_Haptic_Effect_Shutdown(int * effect_id)
{
	if (!effect_id)
	{
		return;
	}

	if (*effect_id >= 0)
	{
		SDL_HapticDestroyEffect(joystick_haptic, *effect_id);
	}

	*effect_id = -1;
}

static void
IN_Haptic_Effects_Shutdown(void)
{
	for (int i=0; i<HAPTIC_EFFECT_LIST_SIZE; i++)
	{
		last_haptic_efffect[i].effect_volume = 0;
		last_haptic_efffect[i].effect_duration = 0;
		last_haptic_efffect[i].effect_begin = 0;
		last_haptic_efffect[i].effect_end = 0;
		last_haptic_efffect[i].effect_attack = 0;
		last_haptic_efffect[i].effect_fade = 0;
		last_haptic_efffect[i].effect_x = 0;
		last_haptic_efffect[i].effect_y = 0;
		last_haptic_efffect[i].effect_z = 0;

		IN_Haptic_Effect_Shutdown(&last_haptic_efffect[i].effect_id);
	}
}

/*
 * Haptic Feedback:
 *    effect_volume=0..SHRT_MAX
 *    effect{x,y,z} - effect direction
 *    name - sound file name
 */
void
Haptic_Feedback(char *name, int effect_volume, int effect_duration,
			   int effect_begin, int effect_end,
			   int effect_attack, int effect_fade,
			   int effect_x, int effect_y, int effect_z)
{
	if (!joystick_haptic)
	{
		return;
	}

	if (joy_haptic_magnitude->value <= 0)
	{
		return;
	}

	if (effect_volume <= 0)
	{
		return;
	}

	if (effect_duration <= 0)
	{
		return;
	}

	if (last_haptic_volume != (int)(joy_haptic_magnitude->value * 255))
	{
		IN_Haptic_Effects_Shutdown();
		IN_Haptic_Effects_Init();
	}

	last_haptic_volume = joy_haptic_magnitude->value * 255;

	if (
		strstr(name, "misc/menu") ||
		strstr(name, "weapons/") ||
		/* detect pain for any player model */
		((
			strstr(name, "player/") ||
			strstr(name, "players/")
		) && (
			strstr(name, "/pain")
		)) ||
		strstr(name, "player/step") ||
		strstr(name, "player/land")
	)
	{
		// check last effect for reuse
		if (
		    last_haptic_efffect[last_haptic_efffect_pos].effect_volume != effect_volume ||
		    last_haptic_efffect[last_haptic_efffect_pos].effect_duration != effect_duration ||
		    last_haptic_efffect[last_haptic_efffect_pos].effect_begin != effect_begin ||
		    last_haptic_efffect[last_haptic_efffect_pos].effect_end != effect_end ||
		    last_haptic_efffect[last_haptic_efffect_pos].effect_attack != effect_attack ||
		    last_haptic_efffect[last_haptic_efffect_pos].effect_fade != effect_fade ||
		    last_haptic_efffect[last_haptic_efffect_pos].effect_x != effect_x ||
		    last_haptic_efffect[last_haptic_efffect_pos].effect_y != effect_y ||
		    last_haptic_efffect[last_haptic_efffect_pos].effect_z != effect_z)
		{
			if ((SDL_HapticQuery(joystick_haptic) & SDL_HAPTIC_SINE)==0)
			{
				return;
			}

			int hapric_volume = joy_haptic_magnitude->value * effect_volume; // 32767 max strength;

			if (effect_duration <= 0)
			{
				return;
			}

			/*
			Com_Printf("%s: volume %d: %d ms %d:%d:%d ms speed: %.2f\n",
				name,  effect_volume, effect_duration - effect_end,
				effect_begin, effect_attack, effect_fade,
				(float)effect_volume / effect_fade);
			*/

			// FIFO for effects
			last_haptic_efffect_pos = (last_haptic_efffect_pos+1) % last_haptic_efffect_size;
			IN_Haptic_Effect_Shutdown(&last_haptic_efffect[last_haptic_efffect_pos].effect_id);
			last_haptic_efffect[last_haptic_efffect_pos].effect_volume = effect_volume;
			last_haptic_efffect[last_haptic_efffect_pos].effect_duration = effect_duration;
			last_haptic_efffect[last_haptic_efffect_pos].effect_attack = effect_attack;
			last_haptic_efffect[last_haptic_efffect_pos].effect_fade = effect_fade;
			last_haptic_efffect[last_haptic_efffect_pos].effect_x = effect_x;
			last_haptic_efffect[last_haptic_efffect_pos].effect_y = effect_y;
			last_haptic_efffect[last_haptic_efffect_pos].effect_z = effect_z;
			last_haptic_efffect[last_haptic_efffect_pos].effect_id = IN_Haptic_Effect_Init(
				effect_x, effect_y, effect_z,
				effect_duration - effect_end, hapric_volume,
				effect_begin, effect_attack, effect_fade);
		}

		SDL_HapticRunEffect(joystick_haptic, last_haptic_efffect[last_haptic_efffect_pos].effect_id, 1);
	}
}

/*
 * Game Controller
 */
static void
IN_Controller_Init(qboolean notify_user)
{
	cvar_t *in_sdlbackbutton;
	int nummappings;
	char controllerdb[MAX_OSPATH] = {0};
	SDL_Joystick *joystick = NULL;
	SDL_bool is_controller = SDL_FALSE;

	in_sdlbackbutton = Cvar_Get("in_sdlbackbutton", "0", CVAR_ARCHIVE);
	if (in_sdlbackbutton)
	{
		switch ((int)in_sdlbackbutton->value)
		{
			case 1:
				sdl_back_button = SDL_CONTROLLER_BUTTON_START;
				break;
			case 2:
				sdl_back_button = SDL_CONTROLLER_BUTTON_GUIDE;
				break;
			default:
				sdl_back_button = SDL_CONTROLLER_BUTTON_BACK;
		}
	}

	if (notify_user)
	{
		Com_Printf("- Game Controller init attempt -\n");
	}

	if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC))
	{
		if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) == -1)
		{
			Com_Printf ("Couldn't init SDL joystick: %s.\n", SDL_GetError ());
			return;
		}
	}

	Com_Printf ("%i joysticks were found.\n", SDL_NumJoysticks());

	if (!SDL_NumJoysticks())
	{
		joystick_haptic = SDL_HapticOpenFromMouse();

		if (joystick_haptic == NULL)
		{
			Com_Printf("Most likely mouse isn't haptic.\n");
		}
		else
		{
			IN_Haptic_Effects_Info();
		}

		return;
	}

	for (const char* rawPath = FS_GetNextRawPath(NULL); rawPath != NULL; rawPath = FS_GetNextRawPath(rawPath))
	{
		snprintf(controllerdb, MAX_OSPATH, "%s/gamecontrollerdb.txt", rawPath);
		nummappings = SDL_GameControllerAddMappingsFromFile(controllerdb);
		if (nummappings > 0)
			Com_Printf ("%d mappings loaded from gamecontrollerdb.txt\n", nummappings);
	}

	for (int i = 0; i < SDL_NumJoysticks(); i++)
	{
		joystick = SDL_JoystickOpen(i);
		const char* joystick_name = SDL_JoystickName(joystick);
		const int name_len = strlen(joystick_name);

		Com_Printf ("The name of the joystick is '%s'\n", joystick_name);

		// Ugly hack to detect IMU-only devices - works for Switch Pro Controller at least
		if (name_len > 4 && !strncmp(joystick_name + name_len - 4, " IMU", 4))
		{
			Com_Printf ("Skipping IMU device.\n");
			SDL_JoystickClose(joystick);
			joystick = NULL;
			continue;
		}

		Com_Printf ("Number of Axes: %d\n", SDL_JoystickNumAxes(joystick));
		Com_Printf ("Number of Buttons: %d\n", SDL_JoystickNumButtons(joystick));
		Com_Printf ("Number of Balls: %d\n", SDL_JoystickNumBalls(joystick));
		Com_Printf ("Number of Hats: %d\n", SDL_JoystickNumHats(joystick));

		is_controller = SDL_IsGameController(i);
		if (!is_controller)
		{
			char joystick_guid[256] = {0};

			SDL_JoystickGUID guid;
			guid = SDL_JoystickGetDeviceGUID(i);

			SDL_JoystickGetGUIDString(guid, joystick_guid, 255);

			Com_Printf ("To use joystick as game controller please set SDL_GAMECONTROLLERCONFIG:\n");
			Com_Printf ("e.g.: SDL_GAMECONTROLLERCONFIG='%s,%s,leftx:a0,lefty:a1,rightx:a2,righty:a3,back:b1,...\n", joystick_guid, joystick_name);
			Com_Printf ("Or you can put 'gamecontrollerdb.txt' in your game directory.\n");
		}

		SDL_JoystickClose(joystick);
		joystick = NULL;

		if (is_controller)
		{
			controller = SDL_GameControllerOpen(i);

			Com_Printf ("Controller settings: %s\n", SDL_GameControllerMapping(controller));
			Com_Printf ("Controller axis: \n");
			Com_Printf (" * leftx = %s\n", joy_axis_leftx->string);
			Com_Printf (" * lefty = %s\n", joy_axis_lefty->string);
			Com_Printf (" * rightx = %s\n", joy_axis_rightx->string);
			Com_Printf (" * righty = %s\n", joy_axis_righty->string);
			Com_Printf (" * triggerleft = %s\n", joy_axis_triggerleft->string);
			Com_Printf (" * triggerright = %s\n", joy_axis_triggerright->string);

			Com_Printf ("Controller thresholds: \n");
			Com_Printf (" * leftx = %f\n", joy_axis_leftx_threshold->value);
			Com_Printf (" * lefty = %f\n", joy_axis_lefty_threshold->value);
			Com_Printf (" * rightx = %f\n", joy_axis_rightx_threshold->value);
			Com_Printf (" * righty = %f\n", joy_axis_righty_threshold->value);
			Com_Printf (" * triggerleft = %f\n", joy_axis_triggerleft_threshold->value);
			Com_Printf (" * triggerright = %f\n", joy_axis_triggerright_threshold->value);

			joystick_haptic = SDL_HapticOpenFromJoystick(SDL_GameControllerGetJoystick(controller));

			if (joystick_haptic == NULL)
			{
				Com_Printf("Most likely controller isn't haptic.\n");
			}
			else
			{
				IN_Haptic_Effects_Info();
			}

			break;
		}
	}
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
	freelook = Cvar_Get("freelook", "1", CVAR_ARCHIVE);
	in_grab = Cvar_Get("in_grab", "2", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get("lookstrafe", "0", CVAR_ARCHIVE);
	m_filter = Cvar_Get("m_filter", "0", CVAR_ARCHIVE);
	m_up = Cvar_Get("m_up", "1", CVAR_ARCHIVE);
	m_forward = Cvar_Get("m_forward", "1", CVAR_ARCHIVE);
	m_pitch = Cvar_Get("m_pitch", "0.022", CVAR_ARCHIVE);
	m_side = Cvar_Get("m_side", "0.8", CVAR_ARCHIVE);
	m_yaw = Cvar_Get("m_yaw", "0.022", CVAR_ARCHIVE);
	sensitivity = Cvar_Get("sensitivity", "3", CVAR_ARCHIVE);

	joy_haptic_magnitude = Cvar_Get("joy_haptic_magnitude", "0.0", CVAR_ARCHIVE);

	joy_yawsensitivity = Cvar_Get("joy_yawsensitivity", "1.0", CVAR_ARCHIVE);
	joy_pitchsensitivity = Cvar_Get("joy_pitchsensitivity", "1.0", CVAR_ARCHIVE);
	joy_forwardsensitivity = Cvar_Get("joy_forwardsensitivity", "1.0", CVAR_ARCHIVE);
	joy_sidesensitivity = Cvar_Get("joy_sidesensitivity", "1.0", CVAR_ARCHIVE);
	joy_upsensitivity = Cvar_Get("joy_upsensitivity", "1.0", CVAR_ARCHIVE);
	joy_expo = Cvar_Get("joy_expo", "2.0", CVAR_ARCHIVE);

	joy_axis_leftx = Cvar_Get("joy_axis_leftx", "sidemove", CVAR_ARCHIVE);
	joy_axis_lefty = Cvar_Get("joy_axis_lefty", "forwardmove", CVAR_ARCHIVE);
	joy_axis_rightx = Cvar_Get("joy_axis_rightx", "yaw", CVAR_ARCHIVE);
	joy_axis_righty = Cvar_Get("joy_axis_righty", "pitch", CVAR_ARCHIVE);
	joy_axis_triggerleft = Cvar_Get("joy_axis_triggerleft", "triggerleft", CVAR_ARCHIVE);
	joy_axis_triggerright = Cvar_Get("joy_axis_triggerright", "triggerright", CVAR_ARCHIVE);

	joy_axis_leftx_threshold = Cvar_Get("joy_axis_leftx_threshold", "0.15", CVAR_ARCHIVE);
	joy_axis_lefty_threshold = Cvar_Get("joy_axis_lefty_threshold", "0.15", CVAR_ARCHIVE);
	joy_axis_rightx_threshold = Cvar_Get("joy_axis_rightx_threshold", "0.15", CVAR_ARCHIVE);
	joy_axis_righty_threshold = Cvar_Get("joy_axis_righty_threshold", "0.15", CVAR_ARCHIVE);
	joy_axis_triggerleft_threshold = Cvar_Get("joy_axis_triggerleft_threshold", "0.15", CVAR_ARCHIVE);
	joy_axis_triggerright_threshold = Cvar_Get("joy_axis_triggerright_threshold", "0.15", CVAR_ARCHIVE);

	windowed_mouse = Cvar_Get("windowed_mouse", "1", CVAR_USERINFO | CVAR_ARCHIVE);

	Cmd_AddCommand("+mlook", IN_MLookDown);
	Cmd_AddCommand("-mlook", IN_MLookUp);

	Cmd_AddCommand("+joyaltselector", IN_JoyAltSelectorDown);
	Cmd_AddCommand("-joyaltselector", IN_JoyAltSelectorUp);

	SDL_StartTextInput();

	IN_Controller_Init(false);

	Com_Printf("------------------------------------\n\n");
}

/*
 * Shuts the backend down
 */
static void
IN_Haptic_Shutdown(void)
{
	if (joystick_haptic)
	{
		IN_Haptic_Effects_Shutdown();

		SDL_HapticClose(joystick_haptic);
		joystick_haptic = NULL;
	}
}

static void
IN_Controller_Shutdown(qboolean notify_user)
{
	if (notify_user)
	{
		Com_Printf("- Game Controller disconnected -\n");
	}

	IN_Haptic_Shutdown();

	if (controller)
	{
		SDL_GameControllerClose(controller);
		controller  = NULL;
	}
}

void
IN_Shutdown(void)
{
	Cmd_RemoveCommand("force_centerview");
	Cmd_RemoveCommand("+mlook");
	Cmd_RemoveCommand("-mlook");

	Com_Printf("Shutting down input.\n");

	IN_Controller_Shutdown(false);

	const Uint32 subsystems = SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC;
	if (SDL_WasInit(subsystems) == subsystems)
		SDL_QuitSubSystem(subsystems);
}

/* ------------------------------------------------------------------ */
