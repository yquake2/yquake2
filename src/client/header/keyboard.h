/*
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * =======================================================================
 *
 * The header file for the upper level key event processing
 *
 * =======================================================================
 */

#ifndef CL_HEADER_KEYBOARD_H
#define CL_HEADER_KEYBOARD_H

#include "../../common/header/shared.h" /* for qboolean etc */

/* Max length of a console command line. 1024
 * chars allow for a vertical resolution of
 * 8192 pixel which should be enough for the
 * years to come. */
#define MAXCMDLINE 1024

/* number of console command lines saved in history,
 * must be a power of two, because we use & (NUM_KEY_LINES-1)
 * instead of % so -1 wraps to NUM_KEY_LINES-1 */
#define NUM_KEY_LINES 32

/*
 * the joystick altselector key is pressed
 * => K_BTN_x turns into K_BTN_x_ALT
 */
extern qboolean joy_altselector_pressed;

/* these are the key numbers that should be passed to Key_Event
   they must be matched by the low level key event processing! */
enum QKEYS {
	K_TAB = 9,
	K_ENTER = 13,
	K_ESCAPE = 27,
	// Note: ASCII keys are generally valid but don't get constants here,
	// just use 'a' (yes, lowercase) or '2' or whatever, however there are
	// some special cases when writing/parsing configs (space or quotes or
	// also ; and $ have a special meaning there so we use e.g. "SPACE" instead),
	// see keynames[] in cl_keyboard.c
	K_SPACE = 32,

	K_BACKSPACE = 127,

	K_COMMAND = 128, // "Windows Key"
	K_CAPSLOCK,
	K_POWER,
	K_PAUSE,

	K_UPARROW,
	K_DOWNARROW,
	K_LEFTARROW,
	K_RIGHTARROW,

	K_ALT,
	K_CTRL,
	K_SHIFT,
	K_INS,
	K_DEL,
	K_PGDN,
	K_PGUP,
	K_HOME,
	K_END,

	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
	K_F13,
	K_F14,
	K_F15,

	K_KP_HOME,
	K_KP_UPARROW,
	K_KP_PGUP,
	K_KP_LEFTARROW,
	K_KP_5,
	K_KP_RIGHTARROW,
	K_KP_END,
	K_KP_DOWNARROW,
	K_KP_PGDN,
	K_KP_ENTER,
	K_KP_INS,
	K_KP_DEL,
	K_KP_SLASH,
	K_KP_MINUS,
	K_KP_PLUS,
	K_KP_NUMLOCK,
	K_KP_STAR,
	K_KP_EQUALS,

	K_MOUSE1,
	K_MOUSE2,
	K_MOUSE3,
	K_MOUSE4,
	K_MOUSE5,

	K_MWHEELDOWN,
	K_MWHEELUP,

	K_SUPER, // TODO: what is this? SDL doesn't seem to know it..
	K_COMPOSE,
	K_MODE,
	K_HELP,
	K_PRINT,
	K_SYSREQ,
	K_SCROLLOCK,
	K_MENU,
	K_UNDO,

	// The following are mapped from SDL_Scancodes, used as a *fallback* for keys
	// whose SDL_KeyCode we don't have a K_ constant for, like German Umlaut keys.
	// The scancode name corresponds to the key at that position on US-QWERTY keyboards
	// *not* the one in the local layout (e.g. German 'Ö' key is K_SC_SEMICOLON)
	// !!! NOTE: if you add a scancode here, make sure to also add it to:
	// 1. keynames[] in cl_keyboard.c
	// 2. IN_TranslateScancodeToQ2Key() in input/sdl.c
	K_SC_A,
	K_SC_B,
	K_SC_C,
	K_SC_D,
	K_SC_E,
	K_SC_F,
	K_SC_G,
	K_SC_H,
	K_SC_I,
	K_SC_J,
	K_SC_K,
	K_SC_L,
	K_SC_M,
	K_SC_N,
	K_SC_O,
	K_SC_P,
	K_SC_Q,
	K_SC_R,
	K_SC_S,
	K_SC_T,
	K_SC_U,
	K_SC_V,
	K_SC_W,
	K_SC_X,
	K_SC_Y,
	K_SC_Z,
	// leaving out SDL_SCANCODE_1 ... _0, we handle them separately already
	// also return, escape, backspace, tab, space, already handled as keycodes
	K_SC_MINUS,
	K_SC_EQUALS,
	K_SC_LEFTBRACKET,
	K_SC_RIGHTBRACKET,
	K_SC_BACKSLASH,
	K_SC_NONUSHASH,
	K_SC_SEMICOLON,
	K_SC_APOSTROPHE,
	K_SC_GRAVE,
	K_SC_COMMA,
	K_SC_PERIOD,
	K_SC_SLASH,
	// leaving out lots of key incl. from keypad, we already handle them as normal keys
	K_SC_NONUSBACKSLASH,
	K_SC_INTERNATIONAL1, /**< used on Asian keyboards, see footnotes in USB doc */
	K_SC_INTERNATIONAL2,
	K_SC_INTERNATIONAL3, /**< Yen */
	K_SC_INTERNATIONAL4,
	K_SC_INTERNATIONAL5,
	K_SC_INTERNATIONAL6,
	K_SC_INTERNATIONAL7,
	K_SC_INTERNATIONAL8,
	K_SC_INTERNATIONAL9,
	K_SC_THOUSANDSSEPARATOR,
	K_SC_DECIMALSEPARATOR,
	K_SC_CURRENCYUNIT,
	K_SC_CURRENCYSUBUNIT,

	// hardcoded pseudo-key to open the console, emitted when pressing the "console key"
	// (SDL_SCANCODE_GRAVE, the one between Esc, 1 and Tab) on layouts that don't
	// have a relevant char there (unlike Brazilian which has quotes there which you
	// want to be able to type in the console) - the user can't bind this key.
	K_CONSOLE,

	// Keyboard keys / codes end here. Any new ones should go before this.
	// From here on, only gamepad controls must be allowed.
	// Otherwise, separate bindings (keyboard / controller) menu options will not work.

	K_BTN_A,
	K_JOY_FIRST_REGULAR = K_BTN_A,
	K_BTN_B,
	K_BTN_X,
	K_BTN_Y,
	K_BTN_BACK,
	K_BTN_GUIDE,
	K_BTN_START,
	K_STICK_LEFT,
	K_STICK_RIGHT,
	K_SHOULDER_LEFT,
	K_SHOULDER_RIGHT,
	K_DPAD_UP,
	K_DPAD_DOWN,
	K_DPAD_LEFT,
	K_DPAD_RIGHT,
	K_BTN_MISC1,
	K_PADDLE_1,
	K_PADDLE_2,
	K_PADDLE_3,
	K_PADDLE_4,
	K_TOUCHPAD,	// SDL_CONTROLLER_BUTTON_MAX - 1
	K_TRIG_LEFT,	// buttons for triggers (axes)
	K_TRIG_RIGHT,

	// add other joystick/controller keys before this one
	// and adjust it accordingly, also remember to add corresponding _ALT key below!
	K_JOY_LAST_REGULAR = K_TRIG_RIGHT,

	/* Can't be mapped to any action (=> not regular) */
	K_JOY_BACK,

	K_BTN_A_ALT,
	K_JOY_FIRST_REGULAR_ALT = K_BTN_A_ALT,
	K_BTN_B_ALT,
	K_BTN_X_ALT,
	K_BTN_Y_ALT,
	K_BTN_BACK_ALT,
	K_BTN_GUIDE_ALT,
	K_BTN_START_ALT,
	K_STICK_LEFT_ALT,
	K_STICK_RIGHT_ALT,
	K_SHOULDER_LEFT_ALT,
	K_SHOULDER_RIGHT_ALT,
	K_DPAD_UP_ALT,
	K_DPAD_DOWN_ALT,
	K_DPAD_LEFT_ALT,
	K_DPAD_RIGHT_ALT,
	K_BTN_MISC1_ALT,
	K_PADDLE_1_ALT,
	K_PADDLE_2_ALT,
	K_PADDLE_3_ALT,
	K_PADDLE_4_ALT,
	K_TOUCHPAD_ALT,
	K_TRIG_LEFT_ALT,
	K_TRIG_RIGHT_ALT,

	K_LAST
};

extern char		*keybindings[K_LAST];
extern int		key_repeats[K_LAST];
extern int		anykeydown;
extern char		chat_buffer[];
extern int		chat_bufferlen;
extern int		chat_cursorpos;
extern qboolean	chat_team;

qboolean IN_NumpadIsOn();
void Char_Event(int key);
void Key_Event(int key, qboolean down, qboolean special);
void Key_Init(void);
void Key_Shutdown(void);
void Key_WriteBindings(FILE *f);
void Key_ReadConsoleHistory();
void Key_WriteConsoleHistory();
void Key_SetBinding(int keynum, char *binding);
void Key_MarkAllUp(void);
void Controller_Rumble(const char *name, vec3_t source, qboolean from_player,
				unsigned int duration, unsigned short int volume);
void Haptic_Feedback(const char *name, int effect_volume, int effect_duration,
				int effect_delay, int effect_attack, int effect_fade,
				int effect_x, int effect_y, int effect_z, float effect_distance);
int Key_GetMenuKey(int key);

#endif
