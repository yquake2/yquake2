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
 * Joystick reading and deadzone handling is based on:
 * http://joshsutphin.com/2013/04/12/doing-thumbstick-dead-zones-right.html
 * ...and implementation is partially based on code from:
 * - http://quakespasm.sourceforge.net
 * - https://github.com/Minimuino/thumbstick-deadzones
 *
 * Flick Stick handling is based on:
 * http://gyrowiki.jibbsmart.com/blog:good-gyro-controls-part-2:the-flick-stick
 *
 * =======================================================================
 *
 * This is the Quake II input system backend, implemented with SDL.
 *
 * =======================================================================
 */

#include <SDL3/SDL.h>
#include <limits.h>

#include "SDL3/SDL_gamepad.h"
#include "SDL3/SDL_properties.h"
#include "SDL3/SDL_stdinc.h"
#include "header/input.h"
#include "header/gyro.h"
#include "../header/keyboard.h"
#include "../header/client.h"

// ----

// Maximal mouse move per frame
#define MOUSE_MAX 3000

// Minimal mouse move per frame
#define MOUSE_MIN 40

// ----

typedef enum
{
	LAYOUT_NONE = -1,
	LAYOUT_DEFAULT,
	LAYOUT_SOUTHPAW,
	LAYOUT_LEGACY,
	LAYOUT_LEGACY_SOUTHPAW,
	LAYOUT_FLICK_STICK,
	LAYOUT_FLICK_STICK_SOUTHPAW
} sticklayout_t;

typedef struct
{
	float x;
	float y;
} thumbstick_t;

typedef struct
{
	thumbstick_t left;
	thumbstick_t right;
	float *forwardmove;
	float *sidemove;
	float *yaw;
	float *pitch;
	float extra_scale;
	qboolean extra_active;
} joystate_t;

typedef enum
{
	REASON_NONE,
	REASON_CONTROLLERINIT,
	REASON_GYROCALIBRATION
} updates_countdown_reasons;

// ----

// These are used to communicate the events collected by
// IN_Update() called at the beginning of a frame to the
// actual movement functions called at a later time.
static float mouse_x, mouse_y;
static unsigned char joy_escbutton = SDL_GAMEPAD_BUTTON_START;
static int joystick_left_x, joystick_left_y, joystick_right_x, joystick_right_y;
static qboolean mlooking;

// The last time input events were processed.
// Used throughout the client.
int sys_frame_time;

// the joystick altselector that turns K_BTN_X into K_BTN_X_ALT
// is pressed
qboolean joy_altselector_pressed = false;

// Gamepad labels' style (Xbox, Playstation, etc.) in use, normally set after detection
gamepad_labels_t joy_current_lbls = LBL_SDL;

// Using japanese style for confirm & cancel buttons on gamepad
qboolean japanese_confirm = false;

// Console Variables
cvar_t *freelook;
cvar_t *lookstrafe;
cvar_t *m_forward;
cvar_t *m_pitch;
cvar_t *m_side;
cvar_t *m_up;
cvar_t *m_yaw;
static cvar_t *sensitivity;

static cvar_t *exponential_speedup;
static cvar_t *in_grab;
static cvar_t *m_filter;
static cvar_t *windowed_pauseonfocuslost;
static cvar_t *windowed_mouse;
static cvar_t *haptic_feedback_filter;

// ----

typedef struct haptic_effects_cache {
	int effect_volume;
	int effect_duration;
	int effect_delay;
	int effect_attack;
	int effect_fade;
	int effect_id;
	int effect_x;
	int effect_y;
	int effect_z;
} haptic_effects_cache_t;

qboolean show_gamepad = false, show_haptic = false, show_gyro = false;

static SDL_Haptic *joystick_haptic = NULL;
static SDL_Gamepad *controller = NULL;

#define HAPTIC_EFFECT_LIST_SIZE 16

static int last_haptic_volume = 0;
static int last_haptic_effect_size = HAPTIC_EFFECT_LIST_SIZE;
static int last_haptic_effect_pos = 0;
static haptic_effects_cache_t last_haptic_effect[HAPTIC_EFFECT_LIST_SIZE];

// Gamepad labels' style (Xbox, Playstation, etc.) requested by user
static cvar_t *joy_labels;

// Gamepad style for confirm and cancel buttons (traditional or japanese)
static cvar_t *joy_confirm;

// Joystick sensitivity
static cvar_t *joy_sensitivity;
static cvar_t *joy_advanced;
static cvar_t *joy_yawspeed;
static cvar_t *joy_pitchspeed;
static cvar_t *joy_extra_yawspeed;
static cvar_t *joy_extra_pitchspeed;
static cvar_t *joy_ramp_time;
static cvar_t *joy_outer_threshold;
static cvar_t *joy_forwardsensitivity;
static cvar_t *joy_sidesensitivity;
void IN_ApplyJoyPreset(void);

// Joystick's analog sticks configuration
static sticklayout_t joy_active_layout;
cvar_t *joy_layout;
static cvar_t *joy_left_expo;
static cvar_t *joy_left_snapaxis;
static cvar_t *joy_left_deadzone;
static cvar_t *joy_right_expo;
static cvar_t *joy_right_snapaxis;
static cvar_t *joy_right_deadzone;
static cvar_t *joy_flick_threshold;
static cvar_t *joy_flick_smoothed;

// Joystick's trigger threshold
static cvar_t *joy_trigger;

// Joystick haptic
static cvar_t *joy_haptic_magnitude;
static cvar_t *joy_haptic_distance;

// Gyro mode (0=off, 3=on, 1-2=uses button to enable/disable)
cvar_t *gyro_mode;

// Gyro space
static cvar_t *gyro_space;
static cvar_t *gyro_local_roll;

// Gyro sensitivity
static cvar_t *gyro_yawsensitivity;
static cvar_t *gyro_pitchsensitivity;

// Gyro tightening
static cvar_t *gyro_tightening;

// Gyro smoothing
static cvar_t *gyro_smoothing;
static cvar_t *gyro_smoothing_window;

// Gyro acceleration
static cvar_t *gyro_acceleration;
static cvar_t *gyro_accel_multiplier;
static cvar_t *gyro_accel_lower_thresh;
static cvar_t *gyro_accel_upper_thresh;

// Gyro is ready to take player inputs
static qboolean gyro_active = false;

// Controller is connected and supports gyro, gyro is not disabled, game is not
// paused, game is receiving inputs, controller is initialized and is not
// calibrating gyro.
static qboolean gyro_enabled;

// Gyro calibration
static cvar_t *gyro_calibration_a;
static cvar_t *gyro_calibration_x;
static cvar_t *gyro_calibration_y;
static cvar_t *gyro_calibration_z;

// If "SDL gyro" is not explicitly disabled, use SDL_EVENT_GAMEPAD_SENSOR_UPDATE to read gyro

#ifdef NO_SDL_GYRO // gyro can be read as a "secondary joystick" exposed by dkms-hid-nintendo
static float accel_accum[3];
static float gyro_accum[3];
static unsigned int num_accel[3];
static unsigned int num_gyro[3];
static SDL_Joystick *imu_joystick = NULL;	// gyro "joystick"
// Accelerometer values reported as 4096 digits per g.
#define IMU_JOY_ACCEL_SCALE (1.0f / 4096.0f)
#define IMU_JOY_AXIS_ACCEL_Y 1
#define IMU_JOY_AXIS_ACCEL_X 2
#define IMU_JOY_AXIS_ACCEL_Z 0
// Gyro values reported as 14247 digits per 1000 degrees/second.
#define IMU_JOY_GYRO_SCALE (float)(M_PI / (180.0 * 14.247))
#define IMU_JOY_AXIS_GYRO_PITCH 4
#define IMU_JOY_AXIS_GYRO_YAW 5
#define IMU_JOY_AXIS_GYRO_ROLL 3
#endif

// To ignore SDL_EVENT_JOYSTICK_ADDED at game init. Allows for hot plugging of gamepad afterwards.
static qboolean first_init = true;

// Countdown of calls to IN_Update(), needed for controller init and gyro calibration
static unsigned short int updates_countdown = 30;

// Reason for the countdown
static updates_countdown_reasons countdown_reason = REASON_CONTROLLERINIT;

// Flick Stick
#define FLICK_TIME 100		// time it takes for a flick to execute, in ms
static float target_angle;	// angle to end up facing at the end of a flick
static float flick_progress = 1.0f;	// from 0.0 to 1.0
static int started_flick;	// time of flick start

// Flick Stick's rotation input samples to smooth out
#define MAX_SMOOTH_SAMPLES 8
static float flick_samples[MAX_SMOOTH_SAMPLES];
static unsigned short int front_sample = 0;

// Threshold at which a trigger press is registered, in SDL units
static int trig_thresh;

extern void CalibrationFinishedCallback(void);

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
#ifdef __APPLE__
		case SDLK_RGUI:
		case SDLK_LGUI:
			key = K_COMMAND;
			break;
#else
		case SDLK_RGUI:
		case SDLK_LGUI:
			key = K_SUPER;
			break;
#endif
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

static void IN_Controller_Init(qboolean notify_user);
static void IN_Controller_Shutdown(qboolean notify_user);

/*
 * Sets the gamepad buttons' style of labels (SDL, Xbox, PS, Switch).
 * They are only visible in the gamepad binding options.
 * Traditional binding uses SDL style, no matter the gamepad.
 */
static void
IN_GamepadLabels_Changed(void)
{
	const int requested = (int)joy_labels->value;
	joy_labels->modified = false;
	joy_current_lbls = LBL_SDL;

	if (requested < 0 && controller) // try to autodetect...
	{
		switch (SDL_GetGamepadType(controller))
		{
			case SDL_GAMEPAD_TYPE_XBOX360:
			case SDL_GAMEPAD_TYPE_XBOXONE:
				joy_current_lbls = LBL_XBOX;
				return;
			default:
				break;
		}

		// Getting type based on label detected by SDL3
		switch (SDL_GetGamepadButtonLabel(controller, SDL_GAMEPAD_BUTTON_SOUTH))
		{
			case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
				joy_current_lbls = LBL_PLAYSTATION;
				return;
			case SDL_GAMEPAD_BUTTON_LABEL_B:
				joy_current_lbls = LBL_SWITCH;
			default:
				return;
		}
	}
	else if (requested >= LBL_SDL && requested < LBL_MAX_COUNT)
	{
		joy_current_lbls = (gamepad_labels_t)requested;
	}
}

/*
 * Sets which gamepad button works as "confirm", and which
 * works as "cancel", in menus.
 */
static void
IN_GamepadConfirm_Changed(void)
{
	const int requested = (int)joy_confirm->value;
	joy_confirm->modified = false;
	japanese_confirm = false;

	if ( (requested < 0 && controller &&
		SDL_GetGamepadButtonLabel(controller, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_B)
		|| requested == 1 )
	{
		japanese_confirm = true;
	}
}

/*
 * Sets the threshold at which a trigger press is registered as a button
 */
static void
IN_GamepadTrigger_Changed(void)
{
	float thresh = joy_trigger->value;
	joy_trigger->modified = false;
	thresh = Q_clamp(thresh, 0.001f, 1.0f);
	trig_thresh = 32766.0f * thresh;	// max value = 32767
}

#ifdef NO_SDL_GYRO
static void
ImuJoy_AverageAccelSamples(vec3_t accel_avg)
{
	accel_avg[0] = accel_accum[0] / num_accel[0];
	accel_avg[1] = accel_accum[1] / num_accel[1];
	accel_avg[2] = accel_accum[2] / num_accel[2];
}

static void
ImuJoy_AverageGyroSamples(vec3_t gyro_avg)
{
	gyro_avg[0] = gyro_accum[0] / num_gyro[0];
	gyro_avg[1] = gyro_accum[1] / num_gyro[1];
	gyro_avg[2] = gyro_accum[2] / num_gyro[2];
}

static void
ImuJoy_ResetAccelSamples(void)
{
	memset(accel_accum, 0, sizeof(accel_accum));
	memset(num_accel, 0, sizeof(num_accel));
}

static void
ImuJoy_ResetGyroSamples(void)
{
	memset(gyro_accum, 0, sizeof(gyro_accum));
	memset(num_gyro, 0, sizeof(num_gyro));
}

static void
ImuJoy_ResetSamples(void)
{
	ImuJoy_ResetAccelSamples();
	ImuJoy_ResetGyroSamples();
}

static void
ImuJoy_PrepareSamples(void)
{
	if (num_accel[0] && num_accel[1] && num_accel[2])
	{
		vec3_t accel_data;
		ImuJoy_AverageAccelSamples(accel_data);
		ImuJoy_ResetAccelSamples();
		VectorScale(accel_data, IMU_JOY_ACCEL_SCALE, accel_data);

		// Now the data can be handled like SDL_EVENT_GAMEPAD_SENSOR_UPDATE.
		IN_GYRO_AccumulateAccelSamples(accel_data);
	}

	if (num_gyro[0] && num_gyro[1] && num_gyro[2])
	{
		vec3_t gyro_data;
		ImuJoy_AverageGyroSamples(gyro_data);
		ImuJoy_ResetGyroSamples();
		VectorScale(gyro_data, IMU_JOY_GYRO_SCALE, gyro_data);

		// Now the data can be handled like SDL_EVENT_GAMEPAD_SENSOR_UPDATE.
		IN_GYRO_AccumulateGyroSamples(gyro_data);
	}
}
#endif // NO_SDL_GYRO

static void
IN_CheckGyroModified(void)
{
	qboolean reset_needed = false;

	if (gyro_mode->modified)
	{
		gyro_active = (gyro_mode->value == 2 || gyro_mode->value == 3);
		gyro_mode->modified = false;
	}

	if (gyro_space->modified)
	{
		IN_GYRO_UpdateGyroSpace(gyro_space->value);
		gyro_space->modified = false;
		reset_needed = true;
	}

	if (gyro_local_roll->modified)
	{
		IN_GYRO_UpdateLocalRoll(gyro_local_roll->value);
		gyro_local_roll->modified = false;
	}

	if (gyro_tightening->modified)
	{
		IN_GYRO_UpdateTightening(gyro_tightening->value);
		gyro_tightening->modified = false;
	}

	if (gyro_smoothing->modified)
	{
		IN_GYRO_UpdateSmoothing(gyro_smoothing->value);
		gyro_smoothing->modified = false;
		reset_needed = true;
	}

	if (gyro_smoothing_window->modified)
	{
		IN_GYRO_UpdateSmoothingWindow(gyro_smoothing_window->value);
		gyro_smoothing_window->modified = false;
		reset_needed = true;
	}

	if (gyro_acceleration->modified)
	{
		IN_GYRO_UpdateAcceleration(gyro_acceleration->value);
		gyro_acceleration->modified = false;
	}

	if (gyro_accel_multiplier->modified)
	{
		IN_GYRO_UpdateAccelMult(gyro_accel_multiplier->value);
		gyro_accel_multiplier->modified = false;
	}

	if (gyro_accel_lower_thresh->modified || gyro_accel_upper_thresh->modified)
	{
		IN_GYRO_UpdateAccelThresh(gyro_accel_lower_thresh->value,
								  gyro_accel_upper_thresh->value);
		gyro_accel_lower_thresh->modified = false;
		gyro_accel_upper_thresh->modified = false;
	}

	if (gyro_pitchsensitivity->modified || gyro_yawsensitivity->modified)
	{
		IN_GYRO_UpdateSensitivity(gyro_pitchsensitivity->value,
								  gyro_yawsensitivity->value);
		gyro_pitchsensitivity->modified = false;
		gyro_yawsensitivity->modified = false;
	}

	if (reset_needed)
	{
		IN_GYRO_ResetState();
	}

	// Set if gyro can be used
	gyro_enabled = (show_gyro && gyro_active && !cl_paused->value
		&& cls.key_dest == key_game && countdown_reason == REASON_NONE);
}

static void
IN_VirtualKeyEvent(int keynum, qboolean *state_store, qboolean new_state)
{
	if (new_state != *state_store)
	{
		*state_store = new_state;
		Key_Event(keynum, *state_store, true);
	}
}

qboolean IN_NumpadIsOn()
{
	SDL_Keymod mod = SDL_GetModState();

	if ((mod & SDL_KMOD_NUM) == SDL_KMOD_NUM)
	{
		return true;
	}

	return false;
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

	static qboolean left_trigger = false;
	static qboolean right_trigger = false;
	static qboolean left_stick[4] = {false, false, false, false};   // left, right, up, down virtual keys

	static int consoleKeyCode = 0;

	/* Get and process an event */
	while (SDL_PollEvent(&event))
	{

		switch (event.type)
		{
			case SDL_EVENT_MOUSE_WHEEL :
				Key_Event((event.wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), true, true);
				Key_Event((event.wheel.y > 0 ? K_MWHEELUP : K_MWHEELDOWN), false, true);
				break;

			case SDL_EVENT_MOUSE_BUTTON_DOWN :
			case SDL_EVENT_MOUSE_BUTTON_UP :
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

				Key_Event(key,
					  (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN),
					  true);
				break;

			case SDL_EVENT_MOUSE_MOTION :
				if (cls.key_dest == key_game && (int) cl_paused->value == 0)
				{
					mouse_x += event.motion.xrel;
					mouse_y += event.motion.yrel;
				}
				break;

			case SDL_EVENT_TEXT_INPUT :
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

			case SDL_EVENT_KEY_DOWN :
			case SDL_EVENT_KEY_UP :
			{
				qboolean down = (event.type == SDL_EVENT_KEY_DOWN);

				/* workaround for AZERTY-keyboards, which don't have 1, 2, ..., 9, 0 in first row:
				 * always map those physical keys (scancodes) to those keycodes anyway
				 * see also https://bugzilla.libsdl.org/show_bug.cgi?id=3188 */
				SDL_Scancode sc = event.key.scancode;

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
					SDL_Keycode kc = event.key.key;
					if(sc == SDL_SCANCODE_GRAVE && kc != '\'' && kc != '"')
					{
						// special case/hack: open the console with the "console key"
						// (beneath Esc, left of 1, above Tab)
						// but not if the keycode for this is a quote (like on Brazilian
						// keyboards) - otherwise you couldn't type them in the console
						if((event.key.mod & (SDL_KMOD_CAPS|SDL_KMOD_SHIFT|SDL_KMOD_ALT|SDL_KMOD_CTRL|SDL_KMOD_GUI)) == 0)
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

			case SDL_EVENT_WINDOW_FOCUS_LOST:
			{
				Key_MarkAllUp();
				S_Activate(false);

				if (windowed_pauseonfocuslost->value != 1)
				{
					Cvar_SetValue("paused", 1);
				}

				/* pause music */
				if (Cvar_VariableValue("ogg_pausewithgame") == 1 &&
						OGG_Status() == PLAY && cl.attractloop == false)
				{
					Cbuf_AddText("ogg toggle\n");
				}
				break;
			}

			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			{
				S_Activate(true);

				if (windowed_pauseonfocuslost->value == 2)
				{
					Cvar_SetValue("paused", 0);
				}

				/* play music */
				if (Cvar_VariableValue("ogg_pausewithgame") == 1 &&
						OGG_Status() == PAUSE && cl.attractloop == false &&
						cl_paused->value == 0)
				{
					Cbuf_AddText("ogg toggle\n");
				}
				break;
			}

			case SDL_EVENT_WINDOW_MOVED:
			{
				// make sure GLimp_GetRefreshRate() will query from SDL again - the window might
				// be on another display now!
				glimp_refreshRate = -1.0;
				break;
			}

			case SDL_EVENT_WINDOW_SHOWN:
			{
				if (cl_unpaused_scvis->value > 0)
				{
					Cvar_SetValue("paused", 0);
				}

				/* play music */
				if (Cvar_VariableValue("ogg_pausewithgame") == 1 &&
					OGG_Status() == PAUSE && cl.attractloop == false &&
					cl_paused->value == 0)
				{
					Cbuf_AddText("ogg toggle\n");
				}
				break;
			}

			case SDL_EVENT_GAMEPAD_BUTTON_UP :
			case SDL_EVENT_GAMEPAD_BUTTON_DOWN :
			{
				qboolean down = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
				unsigned char btn = event.gbutton.button;

				// Handle Esc button first, to override its original key
				Key_Event( (btn == joy_escbutton)? K_ESCAPE : K_JOY_FIRST_BTN + btn,
					down, true );
				break;
			}

			case SDL_EVENT_GAMEPAD_AXIS_MOTION :  /* Handle Controller Motion */
			{
				int axis_value = event.gaxis.value;

				switch (event.gaxis.axis)
				{
					case SDL_GAMEPAD_AXIS_LEFT_TRIGGER :
						IN_VirtualKeyEvent(K_TRIG_LEFT, &left_trigger, axis_value > trig_thresh);
						break;

					case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER :
						IN_VirtualKeyEvent(K_TRIG_RIGHT, &right_trigger, axis_value > trig_thresh);
						break;
				}

				if (!cl_paused->value && cls.key_dest == key_game)
				{
					switch (event.gaxis.axis)
					{
						case SDL_GAMEPAD_AXIS_LEFTX :
							joystick_left_x = axis_value;
							break;
						case SDL_GAMEPAD_AXIS_LEFTY :
							joystick_left_y = axis_value;
							break;
						case SDL_GAMEPAD_AXIS_RIGHTX :
							joystick_right_x = axis_value;
							break;
						case SDL_GAMEPAD_AXIS_RIGHTY :
							joystick_right_y = axis_value;
							break;
					}
					break;
				}

				// Virtual keys to navigate menus with left stick
				if (cls.key_dest == key_menu)
				{
					switch (event.gaxis.axis)
					{
						case SDL_GAMEPAD_AXIS_LEFTX :
							IN_VirtualKeyEvent(K_LEFTARROW, &left_stick[0], axis_value < -16896);
							IN_VirtualKeyEvent(K_RIGHTARROW, &left_stick[1], axis_value > 16896);
							break;

						case SDL_GAMEPAD_AXIS_LEFTY :
							IN_VirtualKeyEvent(K_UPARROW, &left_stick[2], axis_value < -16896);
							IN_VirtualKeyEvent(K_DOWNARROW, &left_stick[3], axis_value > 16896);
							break;
					}
				}
				break;
			}

#ifndef NO_SDL_GYRO	// gamepad sensors' reading is supported (gyro, accelerometer)
			case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
				switch (event.gsensor.sensor)
				{
					case SDL_SENSOR_ACCEL:
					{
						// SDL accelerometer data is in SI units (m/s^2).
						// Convert the data to units of gravity (g).
						vec3_t accel_data;
						accel_data[0] = event.gsensor.data[0] / SDL_STANDARD_GRAVITY;
						accel_data[1] = event.gsensor.data[1] / SDL_STANDARD_GRAVITY;
						accel_data[2] = event.gsensor.data[2] / SDL_STANDARD_GRAVITY;
						IN_GYRO_AccumulateAccelSamples(accel_data);
						break;
					}

					case SDL_SENSOR_GYRO:
						// SDL gyro data is already in radians/second.
						IN_GYRO_AccumulateGyroSamples(event.gsensor.data);
						break;

					default:
						// All other sensors are unhandled.
						break;
				}
				break;
#else	// gyro read from a "secondary joystick" (usually with name ending in "IMU")
			case SDL_EVENT_JOYSTICK_AXIS_MOTION:
				if (imu_joystick && event.cdevice.which == SDL_GetJoystickID(imu_joystick))
				{
					const int axis_value = event.gaxis.value;
					switch (event.gaxis.axis)
					{
						case IMU_JOY_AXIS_ACCEL_Y:
							accel_accum[0] -= axis_value;
							num_accel[0]++;
							break;
						case IMU_JOY_AXIS_ACCEL_X:
							accel_accum[1] += axis_value;
							num_accel[1]++;
							break;
						case IMU_JOY_AXIS_ACCEL_Z:
							accel_accum[2] -= axis_value;
							num_accel[2]++;
							break;
						case IMU_JOY_AXIS_GYRO_PITCH:
							gyro_accum[0] -= axis_value;
							num_gyro[0]++;
							break;
						case IMU_JOY_AXIS_GYRO_YAW:
							gyro_accum[1] += axis_value;
							num_gyro[1]++;
							break;
						case IMU_JOY_AXIS_GYRO_ROLL:
							gyro_accum[2] -= axis_value;
							num_gyro[2]++;
							break;
					}
				}
				break;
#endif	// !NO_SDL_GYRO

			case SDL_EVENT_GAMEPAD_REMOVED :
				if (controller && event.gdevice.which == SDL_GetJoystickID(SDL_GetGamepadJoystick(controller))) {
					Cvar_SetValue("paused", 1);
					IN_Controller_Shutdown(true);
					IN_Controller_Init(false);
				}
				break;

			case SDL_EVENT_JOYSTICK_ADDED :
				if (!controller)
				{
					// This should be lower, but some controllers just don't want to get detected by the OS
					updates_countdown = 100;
					countdown_reason = REASON_CONTROLLERINIT;
				}
				break;

			case SDL_EVENT_JOYSTICK_BATTERY_UPDATED :
				if (!controller || event.jbattery.which != SDL_GetJoystickID(SDL_GetGamepadJoystick(controller)))
				{
					break;
				}
				if (event.jbattery.percent <= 20)
				{
					Com_Printf("WARNING: Gamepad battery Low, it is recommended to connect it by cable.\n");
				}
				else if (event.jbattery.percent <= 1)
				{
					SCR_CenterPrint("ALERT: Gamepad battery almost Empty.\n");
				}
				break;

			case SDL_EVENT_QUIT :
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
	// present themselves as two different devices, triggering SDL_EVENT_JOYSTICK_ADDED
	// too many times. They could trigger it even at game initialization.
	// Also used to keep time of the 'controller gyro calibration' pause.
	if (updates_countdown)
	{
		updates_countdown--;
		if (!updates_countdown)		// Countdown finished, apply needed action by reason
		{
			switch (countdown_reason)
			{
				case REASON_CONTROLLERINIT:
					if (!first_init)
					{
						IN_Controller_Shutdown(false);
						IN_Controller_Init(true);
					}
					else
					{
						first_init = false;
					}
					break;

				case REASON_GYROCALIBRATION:	// finish and save calibration
					{
						float accel_cal = 0.0f;
						vec3_t gyro_cal = {0.0f, 0.0f, 0.0f};
#ifndef NO_SDL_GYRO
						accel_cal = IN_GYRO_AverageAccelSamples() - GYRO_DEFAULT_ACCEL;
						IN_GYRO_AverageGyroSamples(gyro_cal);
#else
						if (num_accel[0] && num_accel[1] && num_accel[2])
						{
							vec3_t accel_avg;
							ImuJoy_AverageAccelSamples(accel_avg);
							VectorScale(accel_avg, IMU_JOY_ACCEL_SCALE, accel_avg);
							accel_cal = VectorLength(accel_avg) - GYRO_DEFAULT_ACCEL;
						}
						if (num_gyro[0] && num_gyro[1] && num_gyro[2])
						{
							ImuJoy_AverageGyroSamples(gyro_cal);
							VectorScale(gyro_cal, IMU_JOY_GYRO_SCALE, gyro_cal);
						}
						ImuJoy_ResetSamples();
#endif
						IN_GYRO_ResetSamples();
						IN_GYRO_UpdateCalibration(
							accel_cal + GYRO_DEFAULT_ACCEL, gyro_cal);
						Cvar_SetValue("gyro_calibration_a", accel_cal);
						Cvar_SetValue("gyro_calibration_x", gyro_cal[0]);
						Cvar_SetValue("gyro_calibration_y", gyro_cal[1]);
						Cvar_SetValue("gyro_calibration_z", gyro_cal[2]);
						Com_Printf("Calibration results:\n A=%f X=%f Y=%f Z=%f\n",
							gyro_calibration_a->value, gyro_calibration_x->value,
							gyro_calibration_y->value, gyro_calibration_z->value);
						CalibrationFinishedCallback();
						break;
					}

				default:
					break;	// avoiding compiler warning
			}
			countdown_reason = REASON_NONE;
		}
	}

	if (joy_sensitivity->modified)
	{
		IN_ApplyJoyPreset();
	}
	// Gamepad labels' type and "confirm & cancel style" change handling
	if (joy_labels->modified)
	{
		IN_GamepadLabels_Changed();
	}
	if (joy_confirm->modified)
	{
		IN_GamepadConfirm_Changed();
	}
	if (joy_trigger->modified)
	{
		IN_GamepadTrigger_Changed();
	}
	IN_CheckGyroModified();
}

/*
 * Joystick vector magnitude
 */
static float
IN_StickMagnitude(thumbstick_t stick)
{
	return sqrtf((stick.x * stick.x) + (stick.y * stick.y));
}

/*
 * Scales "v" from [deadzone, 1] range to [0, 1] range, then inherits sign
 */
static float
IN_MapRange(float v, float deadzone, float sign)
{
	return ((v - deadzone) / (1 - deadzone)) * sign;
}

/*
 * Radial deadzone based on github.com/jeremiah-sypult/Quakespasm-Rift
 */
static thumbstick_t
IN_RadialDeadzone(thumbstick_t stick, float deadzone, float threshold)
{
	thumbstick_t result = {0};
	float magnitude = Q_min(IN_StickMagnitude(stick), 1.0f);

	if (magnitude >= threshold)
	{
		result.x = stick.x / magnitude;
		result.y = stick.y / magnitude;
	}
	else if (magnitude > deadzone)
	{
		const float scale =
			((magnitude - deadzone) / (threshold - deadzone)) / magnitude;
		result.x = stick.x * scale;
		result.y = stick.y * scale;
	}

	return result;
}

/*
 * Sloped axial deadzone based on github.com/Minimuino/thumbstick-deadzones
 * Provides a "snap-to-axis" feeling, without losing precision near the center of the stick
 */
static thumbstick_t
IN_SlopedAxialDeadzone(thumbstick_t stick, float deadzone)
{
	thumbstick_t result = {0};
	float abs_x = fabsf(stick.x);
	float abs_y = fabsf(stick.y);
	float sign_x = copysignf(1.0f, stick.x);
	float sign_y = copysignf(1.0f, stick.y);
	float deadzone_x = deadzone * abs_y;	// deadzone of one axis depends...
	float deadzone_y = deadzone * abs_x;	// ...on the value of the other axis

	if (abs_x > deadzone_x)
	{
		result.x = IN_MapRange(abs_x, deadzone_x, sign_x);
	}
	if (abs_y > deadzone_y)
	{
		result.y = IN_MapRange(abs_y, deadzone_y, sign_y);
	}

	return result;
}

/*
 * Exponent applied on stick magnitude
 */
static thumbstick_t
IN_ApplyExpo(thumbstick_t stick, float exponent)
{
	thumbstick_t result = {0};
	float magnitude = IN_StickMagnitude(stick);
	if (magnitude == 0)
	{
		return result;
	}

	const float eased = powf(magnitude, exponent) / magnitude;
	result.x = stick.x * eased;
	result.y = stick.y * eased;
	return result;
}

/*
 * Delete flick stick's buffer of angle samples for smoothing
 */
static void
IN_ResetSmoothSamples()
{
	front_sample = 0;
	for (int i = 0; i < MAX_SMOOTH_SAMPLES; i++)
	{
		flick_samples[i] = 0.0f;
	}
}

/*
 * Soft tiered smoothing for angle rotations with Flick Stick
 * http://gyrowiki.jibbsmart.com/blog:tight-and-smooth:soft-tiered-smoothing
 */
static float
IN_SmoothedStickRotation(float value)
{
	float top_threshold = joy_flick_smoothed->value;
	float bottom_threshold = top_threshold / 2.0f;
	if (top_threshold == 0)
	{
		return value;
	}

	// sample in the circular smoothing buffer we want to write over
	front_sample = (front_sample + 1) % MAX_SMOOTH_SAMPLES;

	// if input > top threshold, it'll all be consumed immediately
	//				0 gets put into the smoothing buffer
	// if input < bottom threshold, it'll all be put in the smoothing buffer
	//				0 for immediate consumption
	float immediate_weight = (fabsf(value) - bottom_threshold)
					/ (top_threshold - bottom_threshold);
	immediate_weight = Q_min( Q_max(immediate_weight, 0.0f), 1.0f ); // clamp to [0, 1] range

	// now we can push the smooth sample
	float smooth_weight = 1.0f - immediate_weight;
	flick_samples[front_sample] = value * smooth_weight;

	// calculate smoothed result
	float average = 0;
	for (int i = 0; i < MAX_SMOOTH_SAMPLES; i++)
	{
		average += flick_samples[i];
	}
	average /= MAX_SMOOTH_SAMPLES;

	// finally, add immediate portion (original input)
	return average + value * immediate_weight;
}

/*
 * Flick Stick handling: detect if the player just started one, or return the
 * player rotation if stick was already flicked
 */
static float
IN_FlickStick(thumbstick_t stick, float axial_deadzone)
{
	static qboolean is_flicking;
	static float last_stick_angle;
	thumbstick_t processed = stick;
	float angle_change = 0;

	if (IN_StickMagnitude(stick) > Q_min(joy_flick_threshold->value, 1.0f))	// flick!
	{
		// Snap-to-axis only if player just started to flick. With x < 0.4,
		// f(x)=1-(1-x)^2 → < 0.64; this might mean a "tap" to the stick, so
		// treat it as a possible attempt to turn 90º / 180º.
		if (!is_flicking || flick_progress < 0.64f)
		{
			processed = IN_SlopedAxialDeadzone(stick, axial_deadzone);
		}

		const float stick_angle = (180 / M_PI) * atan2f(-processed.x, -processed.y);

		if (!is_flicking)
		{
			// Flicking begins now, with a new target
			is_flicking = true;
			flick_progress = 0.0f;
			started_flick = sys_frame_time;
			target_angle = stick_angle;
			IN_ResetSmoothSamples();
		}
		else
		{
			// Was already flicking, just turning now
			angle_change = stick_angle - last_stick_angle;

			// angle wrap: https://stackoverflow.com/a/11498248/1130520
			angle_change = fmod(angle_change + 180.0f, 360.0f);
			if (angle_change < 0)
			{
				angle_change += 360.0f;
			}
			angle_change -= 180.0f;
			angle_change = IN_SmoothedStickRotation(angle_change);
		}

		last_stick_angle = stick_angle;
	}
	else
	{
		is_flicking = false;
	}

	return angle_change;
}

static void
IN_AdvanceFlickStick(void)
{
	// Flick Stick: flick in progress, changing the yaw angle to the target
	// progressively
	if (flick_progress < 1.0f)
	{
		float cur_progress =
			(float)(sys_frame_time - started_flick) / FLICK_TIME;

		if (cur_progress > 1.0f)
		{
			cur_progress = 1.0f;
		}
		else
		{
			// "Ease out" warp processing: f(x)=1-(1-x)^2 , 0 <= x <= 1
			// http://gyrowiki.jibbsmart.com/blog:good-gyro-controls-part-2:the-flick-stick#toc0
			cur_progress = 1.0f - cur_progress;
			cur_progress *= cur_progress;
			cur_progress = 1.0f - cur_progress;
		}

		cl.viewangles[YAW] += (cur_progress - flick_progress) * target_angle;
		flick_progress = cur_progress;
	}
}

static void
IN_UpdateStickLayout(joystate_t *joy)
{
	switch (joy_active_layout)
	{
		case LAYOUT_SOUTHPAW:
			joy->forwardmove = &joy->right.y;
			joy->sidemove = &joy->right.x;
			joy->yaw = &joy->left.x;
			joy->pitch = &joy->left.y;
			break;

		case LAYOUT_LEGACY:
			joy->forwardmove = &joy->left.y;
			joy->sidemove = &joy->right.x;
			joy->yaw = &joy->left.x;
			joy->pitch = &joy->right.y;
			break;

		case LAYOUT_LEGACY_SOUTHPAW:
			joy->forwardmove = &joy->right.y;
			joy->sidemove = &joy->left.x;
			joy->yaw = &joy->right.x;
			joy->pitch = &joy->left.y;
			break;

		case LAYOUT_FLICK_STICK:
			joy->forwardmove = &joy->left.y;
			joy->sidemove = &joy->left.x;
			joy->yaw = &joy->right.x;			// Unused, must be zero
			joy->pitch = &joy->right.y;			// Unused, must be zero
			break;

		case LAYOUT_FLICK_STICK_SOUTHPAW:
			joy->forwardmove = &joy->right.y;
			joy->sidemove = &joy->right.x;
			joy->yaw = &joy->left.x;			// Unused, must be zero
			joy->pitch = &joy->left.y;			// Unused, must be zero
			break;

		default: // LAYOUT_DEFAULT or LAYOUT_NONE
			joy->forwardmove = &joy->left.y;
			joy->sidemove = &joy->left.x;
			joy->yaw = &joy->right.x;
			joy->pitch = &joy->right.y;
			break;
	}
}

static qboolean
IN_CrossedThreshold(joystate_t *joy, float outer_threshold)
{
	const float magnitude = Q_magnitude(*joy->yaw, *joy->pitch);
	return (magnitude >= outer_threshold);
}

static void
IN_UpdateExtraSpeed(float delta_time, float outer_threshold, joystate_t *joy)
{
	if ((joy_extra_yawspeed->value > 0.0f || joy_extra_pitchspeed->value > 0.0f)
		&& joy_active_layout < LAYOUT_FLICK_STICK
		&& IN_CrossedThreshold(joy, outer_threshold))
	{
		const float ramp_time = Q_clamp(joy_ramp_time->value, 0.0f, 1.0f);

		if (ramp_time > 1.0e-6f)
		{
			static float elapsed_time;

			if (joy->extra_active)
			{
				if (elapsed_time < ramp_time)
				{
					// Continue ramp
					elapsed_time += delta_time;
					joy->extra_scale = elapsed_time / ramp_time;
					joy->extra_scale = Q_min(joy->extra_scale, 1.0f);
				}
			}
			else
			{
				// Start ramp
				elapsed_time = 0.0f;
				joy->extra_scale = 0.0f;
				joy->extra_active = true;
			}
		}
		else
		{
			// Instant ramp
			joy->extra_scale = 1.0f;
			joy->extra_active = true;
		}
	}
	else
	{
		// Reset ramp
		joy->extra_scale = 0.0f;
		joy->extra_active = false;
	}
}

static void
IN_GamepadMove(usercmd_t *cmd)
{
	static joystate_t joy;

	// Factor used to transform from SDL joystick input ([-32768, 32767]) to
	// [-1, 1] range
	static const float normalize_sdl_axis = 1.0f / 32768.0f;

	// Left thumbstick settings
	const float left_expo = Q_clamp(joy_left_expo->value, 1.0f, 5.0f);
	const float left_snapaxis = Q_clamp(joy_left_snapaxis->value, 0.0f, 0.5f);
	const float left_deadzone = Q_clamp(joy_left_deadzone->value, 0.0f, 0.5f);

	// Right thumbstick settings
	const float right_expo = Q_clamp(joy_right_expo->value, 1.0f, 5.0f);
	const float right_snapaxis = Q_clamp(joy_right_snapaxis->value, 0.0f, 0.5f);
	const float right_deadzone = Q_clamp(joy_right_deadzone->value, 0.0f, 0.5f);

	// Shared settings
	const float outer_threshold =
		1.0f - Q_clamp(joy_outer_threshold->value, 0.0f, 0.3f);

	// Update stick layout
	const int layout = lroundf(Q_clamp(joy_layout->value, LAYOUT_DEFAULT,
									   LAYOUT_FLICK_STICK_SOUTHPAW));
	if (joy_active_layout != layout)
	{
		joy_active_layout = layout;
		memset(&joy, 0, sizeof(joy));
		IN_UpdateStickLayout(&joy);
	}

	// Joystick reading and processing
	joy.left.x = joystick_left_x * normalize_sdl_axis;
	joy.left.y = joystick_left_y * normalize_sdl_axis;
	joy.right.x = joystick_right_x * normalize_sdl_axis;
	joy.right.y = joystick_right_y * normalize_sdl_axis;

	IN_UpdateExtraSpeed(cls.rframetime, outer_threshold, &joy);

	if (joy.left.x || joy.left.y)
	{
		joy.left = IN_RadialDeadzone(joy.left, left_deadzone, outer_threshold);

		if (joy_active_layout == LAYOUT_FLICK_STICK_SOUTHPAW)
		{
			cl.viewangles[YAW] += IN_FlickStick(joy.left, left_snapaxis);
			IN_AdvanceFlickStick();
			joy.left.x = joy.left.y = 0.0f;
		}
		else
		{
			joy.left = IN_SlopedAxialDeadzone(joy.left, left_snapaxis);
			joy.left = IN_ApplyExpo(joy.left, left_expo);
		}
	}

	if (joy.right.x || joy.right.y)
	{
		joy.right =
			IN_RadialDeadzone(joy.right, right_deadzone, outer_threshold);

		if (joy_active_layout == LAYOUT_FLICK_STICK)
		{
			cl.viewangles[YAW] += IN_FlickStick(joy.right, right_snapaxis);
			IN_AdvanceFlickStick();
			joy.right.x = joy.right.y = 0.0f;
		}
		else
		{
			joy.right = IN_SlopedAxialDeadzone(joy.right, right_snapaxis);
			joy.right = IN_ApplyExpo(joy.right, right_expo);
		}
	}

	if (*joy.yaw)
	{
		const float speed =
			joy_yawspeed->value + joy.extra_scale * joy_extra_yawspeed->value;
		cl.viewangles[YAW] -= (*joy.yaw) * speed * cls.rframetime;
	}

	if (*joy.pitch)
	{
		const float speed = joy_pitchspeed->value
							+ joy.extra_scale * joy_extra_pitchspeed->value
								  * Q_signf(joy_pitchspeed->value);
		cl.viewangles[PITCH] += (*joy.pitch) * speed * cls.rframetime;
	}

	if (*joy.forwardmove)
	{
		// We need to be twice as fast because with joystick we run...
		cmd->forwardmove -= m_forward->value * joy_forwardsensitivity->value
							* cl_forwardspeed->value * 2.0f
							* (*joy.forwardmove);
	}

	if (*joy.sidemove)
	{
		// We need to be twice as fast because with joystick we run...
		cmd->sidemove += m_side->value * joy_sidesensitivity->value
						 * cl_sidespeed->value * 2.0f * (*joy.sidemove);
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

	if (controller)
	{
		IN_GamepadMove(cmd);
	}

	if (gyro_enabled)
	{
#ifdef NO_SDL_GYRO
		// IMU "joystick" data was accumulated in separate axes through
		// SDL_EVENT_JOYSTICK_AXIS_MOTION, so combine the data now.
		ImuJoy_PrepareSamples();
#endif
		// Always process gyro to keep the state current.
		thumbstick_t gyro_in = {0.0f, 0.0f};
		IN_GYRO_Process(cls.rframetime, &gyro_in.y, &gyro_in.x);

		if (gyro_in.x || gyro_in.y)
		{
			const float factor = cls.rframetime * (float)(180.0 / M_PI);

			if (gyro_in.x)
			{
				cl.viewangles[YAW] += gyro_in.x * factor;
			}

			if (gyro_in.y)
			{
				cl.viewangles[PITCH] -= gyro_in.y * factor;
			}
		}
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

static void
IN_GyroActionDown(void)
{
	switch ((int)gyro_mode->value)
	{
		case 1:
			gyro_active = true;
			return;
		case 2:
			gyro_active = false;
	}
}

static void
IN_GyroActionUp(void)
{
	switch ((int)gyro_mode->value)
	{
		case 1:
			gyro_active = false;
			return;
		case 2:
			gyro_active = true;
	}
}

/*
 * Removes all pending events from SDLs queue.
 */
void
In_FlushQueue(void)
{
	SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
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

	return SDL_CreateHapticEffect(joystick_haptic, &haptic_effect);
}

static void
IN_Haptic_Effects_Info(void)
{
	Com_Printf ("Joystick/Mouse haptic:\n");
	Com_Printf (" * %d effects\n",
		    SDL_GetMaxHapticEffects(joystick_haptic));
	Com_Printf (" * %d haptic effects at the same time\n",
		    SDL_GetMaxHapticEffectsPlaying(joystick_haptic));
	Com_Printf (" * %d haptic axis\n",
		    SDL_GetNumHapticAxes(joystick_haptic));
}

static void
IN_Haptic_Effects_Init(void)
{
	last_haptic_effect_size = SDL_GetMaxHapticEffectsPlaying(joystick_haptic);

	if (last_haptic_effect_size > HAPTIC_EFFECT_LIST_SIZE)
	{
		last_haptic_effect_size = HAPTIC_EFFECT_LIST_SIZE;
	}

	memset(&last_haptic_effect, 0, sizeof(last_haptic_effect));
	for (int i=0; i<HAPTIC_EFFECT_LIST_SIZE; i++)
	{
		last_haptic_effect[i].effect_id = -1;
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
		SDL_DestroyHapticEffect(joystick_haptic, *effect_id);
	}

	*effect_id = -1;
}

static void
IN_Haptic_Effects_Shutdown(void)
{
	for (int i=0; i<HAPTIC_EFFECT_LIST_SIZE; i++)
	{
		last_haptic_effect[i].effect_volume = 0;
		last_haptic_effect[i].effect_duration = 0;
		last_haptic_effect[i].effect_delay = 0;
		last_haptic_effect[i].effect_attack = 0;
		last_haptic_effect[i].effect_fade = 0;
		last_haptic_effect[i].effect_x = 0;
		last_haptic_effect[i].effect_y = 0;
		last_haptic_effect[i].effect_z = 0;

		IN_Haptic_Effect_Shutdown(&last_haptic_effect[i].effect_id);
	}
}

static int
IN_Haptic_GetEffectId(int effect_volume, int effect_duration,
				int effect_delay, int effect_attack, int effect_fade,
				int effect_x, int effect_y, int effect_z)
{
	int i, haptic_volume;

	// check effects for reuse
	for (i=0; i < last_haptic_effect_size; i++)
	{
		if (
			last_haptic_effect[i].effect_volume != effect_volume ||
			last_haptic_effect[i].effect_duration != effect_duration ||
			last_haptic_effect[i].effect_delay != effect_delay ||
			last_haptic_effect[i].effect_attack != effect_attack ||
			last_haptic_effect[i].effect_fade != effect_fade ||
			last_haptic_effect[i].effect_x != effect_x ||
			last_haptic_effect[i].effect_y != effect_y ||
			last_haptic_effect[i].effect_z != effect_z)
		{
			continue;
		}

		return last_haptic_effect[i].effect_id;
	}

	/* create new effect */
	haptic_volume = joy_haptic_magnitude->value * effect_volume; // 32767 max strength;

	/*
	Com_Printf("%d: volume %d: %d ms %d:%d:%d ms speed: %.2f\n",
		last_haptic_effect_pos,  effect_volume, effect_duration,
		effect_delay, effect_attack, effect_fade,
		(float)effect_volume / effect_fade);
	*/

	// FIFO for effects
	last_haptic_effect_pos = (last_haptic_effect_pos + 1) % last_haptic_effect_size;
	IN_Haptic_Effect_Shutdown(&last_haptic_effect[last_haptic_effect_pos].effect_id);
	last_haptic_effect[last_haptic_effect_pos].effect_volume = effect_volume;
	last_haptic_effect[last_haptic_effect_pos].effect_duration = effect_duration;
	last_haptic_effect[last_haptic_effect_pos].effect_delay = effect_delay;
	last_haptic_effect[last_haptic_effect_pos].effect_attack = effect_attack;
	last_haptic_effect[last_haptic_effect_pos].effect_fade = effect_fade;
	last_haptic_effect[last_haptic_effect_pos].effect_x = effect_x;
	last_haptic_effect[last_haptic_effect_pos].effect_y = effect_y;
	last_haptic_effect[last_haptic_effect_pos].effect_z = effect_z;
	last_haptic_effect[last_haptic_effect_pos].effect_id = IN_Haptic_Effect_Init(
		effect_x, effect_y, effect_z,
		effect_duration, haptic_volume,
		effect_delay, effect_attack, effect_fade);

	return last_haptic_effect[last_haptic_effect_pos].effect_id;
}

// Keep it same with rumble rules, look for descriptions to rumble
// filtering in Controller_Rumble
static char *default_haptic_filter = (
	// skipped files should be before wider rule
	"!weapons/*grenlb "     // bouncing grenades don't have feedback
	"!weapons/*hgrenb "     // bouncing grenades don't have feedback
	"!weapons/*open "       // rogue's items don't have feedback
	"!weapons/*warn "       // rogue's items don't have feedback
	// any weapons that are not in previous list
	"weapons/ "
	// player{,s} effects
	"player/*land "         // fall without injury
	"player/*burn "
	"player/*pain "
	"player/*fall "
	"player/*death "
	"players/*burn "
	"players/*pain "
	"players/*fall "
	"players/*death "
	// environment effects
	"doors/ "
	"plats/ "
	"world/*dish  "
	"world/*drill2a "
	"world/*dr_ "
	"world/*explod1 "
	"world/*rocks "
	"world/*rumble  "
	"world/*quake  "
	"world/*train2 "
);

/*
 * name: sound name
 * filter: sound name rule with '*'
 * return false for empty filter
 */
static qboolean
Haptic_Feedback_Filtered_Line(const char *name, const char *filter)
{
	const char *current_filter = filter;

	// skip empty filter
	if (!*current_filter)
	{
		return false;
	}

	while (*current_filter)
	{
		char part_filter[MAX_QPATH];
		const char *name_part;
		const char *str_end;

		str_end = strchr(current_filter, '*');
		if (!str_end)
		{
			if (!strstr(name, current_filter))
			{
				// no such part in string
				return false;
			}
			// have such part
			break;
		}
		// copy filter line
		if ((str_end - current_filter) >= MAX_QPATH)
		{
			return false;
		}
		memcpy(part_filter, current_filter, str_end - current_filter);
		part_filter[str_end - current_filter] = 0;
		// place part in name
		name_part = strstr(name, part_filter);
		if (!name_part)
		{
			// no such part in string
			return false;
		}
		// have such part
		name = name_part + strlen(part_filter);
		// move to next filter
		current_filter = str_end + 1;
	}

	return true;
}

/*
 * name: sound name
 * filter: sound names separated by space, and '!' for skip file
 */
static qboolean
Haptic_Feedback_Filtered(const char *name, const char *filter)
{
	const char *current_filter = filter;

	while (*current_filter)
	{
		char line_filter[MAX_QPATH];
		const char *str_end;

		str_end = strchr(current_filter, ' ');
		// its end of filter
		if (!str_end)
		{
			// check rules inside line
			if (Haptic_Feedback_Filtered_Line(name, current_filter))
			{
				return true;
			}
			return false;
		}
		// copy filter line
		if ((str_end - current_filter) >= MAX_QPATH)
		{
			return false;
		}
		memcpy(line_filter, current_filter, str_end - current_filter);
		line_filter[str_end - current_filter] = 0;
		// check rules inside line
		if (*line_filter == '!')
		{
			// has invert rule
			if (Haptic_Feedback_Filtered_Line(name, line_filter + 1))
			{
				return false;
			}
		}
		else
		{
			if (Haptic_Feedback_Filtered_Line(name, line_filter))
			{
				return true;
			}
		}
		// move to next filter
		current_filter = str_end + 1;
	}
	return false;
}

/*
 * Haptic Feedback:
 *    effect_volume=0..SHRT_MAX
 *    effect{x,y,z} - effect direction
 *    effect{delay,attack,fade} - effect durations
 *    effect_distance - distance to sound source
 *    name - sound file name
 */
void
Haptic_Feedback(const char *name, int effect_volume, int effect_duration,
				int effect_delay, int effect_attack, int effect_fade,
				int effect_x, int effect_y, int effect_z, float effect_distance)
{
	float max_distance = joy_haptic_distance->value;

	if (!joystick_haptic || joy_haptic_magnitude->value <= 0 ||
		max_distance <= 0 || /* skip haptic if distance is negative */
		effect_distance > max_distance ||
		effect_volume <= 0 || effect_duration <= 0 ||
		last_haptic_effect_size <= 0) /* haptic but without slots? */
	{
		return;
	}

	/* combine distance and volume */
	effect_volume *= (max_distance - effect_distance) / max_distance;

	if (last_haptic_volume != (int)(joy_haptic_magnitude->value * 16))
	{
		IN_Haptic_Effects_Shutdown();
		IN_Haptic_Effects_Init();
	}

	last_haptic_volume = joy_haptic_magnitude->value * 16;

	if (Haptic_Feedback_Filtered(name, haptic_feedback_filter->string))
	{
		int effect_id;

		effect_id = IN_Haptic_GetEffectId(effect_volume, effect_duration,
			effect_delay, effect_attack, effect_fade,
			effect_x, effect_y, effect_z);

		if (effect_id == -1)
		{
			/* have rumble used some slots in haptic effect list?,
			 * ok, use little bit less haptic effects at the same time*/
			IN_Haptic_Effects_Shutdown();
			last_haptic_effect_size --;
			Com_Printf("%d haptic effects at the same time\n", last_haptic_effect_size);
			return;
		}

		SDL_RunHapticEffect(joystick_haptic, effect_id, 1);
	}
}

/*
 * Controller_Rumble:
 *	name = sound file name
 *	effect_volume = 0..USHRT_MAX
 *	effect_duration is in ms
 *	source = origin of audio
 *	from_player = if source is the client (player)
 */
void
Controller_Rumble(const char *name, vec3_t source, qboolean from_player,
		unsigned int duration, unsigned short int volume)
{
	vec_t intens = 0.0f, low_freq = 1.0f, hi_freq = 1.0f, dist_prop;
	unsigned short int max_distance = 4;
	unsigned int effect_volume;

	if (!show_haptic || !controller || joy_haptic_magnitude->value <= 0
		|| volume == 0 || duration == 0)
	{
		return;
	}

	if (strstr(name, "weapons/"))
	{
		intens = 1.75f;

		if (strstr(name, "/blastf") || strstr(name, "/hyprbf") || strstr(name, "/nail"))
		{
			intens = 0.125f;	// dampen blasters and nailgun's fire
			low_freq = 0.7f;
			hi_freq = 1.2f;
		}
		else if (strstr(name, "/shotgf") || strstr(name, "/rocklf"))
		{
			low_freq = 1.1f;	// shotgun & RL shouldn't feel so weak
			duration *= 0.7;
		}
		else if (strstr(name, "/sshotf"))
		{
			duration *= 0.6;	// the opposite for super shotgun
		}
		else if (strstr(name, "/machgf") || strstr(name, "/disint"))
		{
			intens = 1.125f;	// machine gun & disruptor fire
		}
		else if (strstr(name, "/grenlb") || strstr(name, "/hgrenb")	// bouncing grenades
			|| strstr(name, "open") || strstr(name, "warn"))	// rogue's items
		{
			return;	// ... don't have feedback
		}
		else if (strstr(name, "/plasshot"))	// phalanx cannon
		{
			intens = 1.0f;
			hi_freq = 0.3f;
			duration *= 0.5;
		}
		else if (strstr(name, "x"))		// explosions...
		{
			low_freq = 1.1f;
			hi_freq = 0.9f;
			max_distance = 550;		// can be felt far away
		}
		else if (strstr(name, "r"))		// reloads & ion ripper fire
		{
			low_freq = 0.1f;
			hi_freq = 0.6f;
		}
	}
	else if (strstr(name, "player/land"))
	{
		intens = 2.2f;	// fall without injury
		low_freq = 1.1f;
	}
	else if (strstr(name, "player/") || strstr(name, "players/"))
	{
		low_freq = 1.2f;	// exaggerate player damage
		if (strstr(name, "/burn") || strstr(name, "/pain100") || strstr(name, "/pain75"))
		{
			intens = 2.4f;
		}
		else if (strstr(name, "/fall") || strstr(name, "/pain50") || strstr(name, "/pain25"))
		{
			intens = 2.7f;
		}
		else if (strstr(name, "/death"))
		{
			intens = 2.9f;
		}
	}
	else if (strstr(name, "doors/"))
	{
		intens = 0.125f;
		low_freq = 0.4f;
		max_distance = 280;
	}
	else if (strstr(name, "plats/"))
	{
		intens = 1.0f;			// platforms rumble...
		max_distance = 200;		// when player near them
	}
	else if (strstr(name, "world/"))
	{
		max_distance = 3500;	// ambient events
		if (strstr(name, "/dish") || strstr(name, "/drill2a") || strstr(name, "/dr_")
			|| strstr(name, "/explod1") || strstr(name, "/rocks")
			|| strstr(name, "/rumble"))
		{
			intens = 0.28f;
			low_freq = 0.7f;
		}
		else if (strstr(name, "/quake"))
		{
			intens = 0.67f;		// (earth)quakes are more evident
			low_freq = 1.2f;
		}
		else if (strstr(name, "/train2"))
		{
			intens = 0.28f;
			max_distance = 290;	// just machinery
		}
	}

	if (intens == 0.0f)
	{
		return;
	}

	if (from_player)
	{
		dist_prop = 1.0f;
	}
	else
	{
		dist_prop = VectorLength(source);
		if (dist_prop > max_distance)
		{
			return;
		}
		dist_prop = (max_distance - dist_prop) / max_distance;
	}

	if (gyro_enabled)
	{
		low_freq *= 0.3;
		hi_freq *= 0.7;
	}

	effect_volume = joy_haptic_magnitude->value * intens * dist_prop * volume;
	low_freq = Q_min(effect_volume * low_freq, USHRT_MAX);
	hi_freq = Q_min(effect_volume * hi_freq, USHRT_MAX);

	// Com_Printf("%-29s: vol %5u - %4u ms - dp %.3f l %5.0f h %5.0f\n",
	//	name, effect_volume, duration, dist_prop, low_freq, hi_freq);

	if (!SDL_RumbleGamepad(controller, low_freq, hi_freq, duration))
	{
		if (!joystick_haptic)
		{
			/* no haptic, some other reason of error */
			return;
		}

		/* All haptic/force feedback slots are busy, try to clean up little bit. */
		IN_Haptic_Effects_Shutdown();
	}
}

/*
 * Gyro calibration functions, called from menu
 */
void
StartCalibration(void)
{
#ifdef NO_SDL_GYRO
	ImuJoy_ResetSamples();
#endif
	IN_GYRO_ResetSamples();
	updates_countdown = 300;
	countdown_reason = REASON_GYROCALIBRATION;
}

qboolean
IsCalibrationZero(void)
{
	return (!gyro_calibration_x->value && !gyro_calibration_y->value && !gyro_calibration_z->value);
}

static void
IN_InitGyro(void)
{
#ifdef NO_SDL_GYRO
	ImuJoy_ResetSamples();
#endif
	IN_GYRO_ResetState();
	gyro_active = (gyro_mode->value == 2 || gyro_mode->value == 3);
	IN_GYRO_UpdateGyroSpace(gyro_space->value);
	IN_GYRO_UpdateLocalRoll(gyro_local_roll->value);
	IN_GYRO_UpdateTightening(gyro_tightening->value);
	IN_GYRO_UpdateSmoothing(gyro_smoothing->value);
	IN_GYRO_UpdateSmoothingWindow(gyro_smoothing_window->value);
	IN_GYRO_UpdateAcceleration(gyro_acceleration->value);
	IN_GYRO_UpdateAccelMult(gyro_accel_multiplier->value);
	IN_GYRO_UpdateAccelThresh(gyro_accel_lower_thresh->value,
							  gyro_accel_upper_thresh->value);
	IN_GYRO_UpdateSensitivity(gyro_pitchsensitivity->value,
							  gyro_yawsensitivity->value);
	float accel_magnitude = gyro_calibration_a->value + GYRO_DEFAULT_ACCEL;
	vec3_t gyro_offset = {gyro_calibration_x->value, gyro_calibration_y->value,
						  gyro_calibration_z->value};
	IN_GYRO_UpdateCalibration(accel_magnitude, gyro_offset);
}

/*
 * Game Controller
 */
static void
IN_Controller_Init(qboolean notify_user)
{
	cvar_t *cvar;
	int nummappings;
	char controllerdb[MAX_OSPATH] = {0};
	SDL_Joystick *joystick = NULL;
	bool is_controller = false;

	cvar = Cvar_Get("joy_escbutton", "0", CVAR_ARCHIVE);
	if (cvar)
	{
		switch ((int)cvar->value)
		{
			case 1:
				joy_escbutton = SDL_GAMEPAD_BUTTON_BACK;
				break;
			case 2:
				joy_escbutton = SDL_GAMEPAD_BUTTON_GUIDE;
				break;
			default:
				joy_escbutton = SDL_GAMEPAD_BUTTON_START;
		}
	}

	cvar = Cvar_Get("in_initjoy", "1", CVAR_NOSET);
	if (!cvar->value)
	{
		return;
	}

	if (notify_user)
	{
		Com_Printf("- Gamepad init attempt -\n");
	}

	if (!SDL_WasInit(SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC))
	{
		if (!SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC))
		{
			Com_Printf ("Couldn't init SDL Gamepad: %s.\n", SDL_GetError());
			return;
		}
	}

	int numjoysticks;
	const SDL_JoystickID *joysticks = SDL_GetJoysticks(&numjoysticks);

	if (joysticks != NULL)
	{
		Com_Printf ("%i joysticks were found.\n", numjoysticks);
	}


	if (numjoysticks == 0)
	{
		joystick_haptic = SDL_OpenHapticFromMouse();

		if (joystick_haptic &&
			(SDL_GetHapticFeatures(joystick_haptic) & SDL_HAPTIC_SINE) == 0)
		{
			/* Disable haptic for joysticks without SINE */
			SDL_CloseHaptic(joystick_haptic);
			joystick_haptic = NULL;
		}

		if (joystick_haptic)
		{
			IN_Haptic_Effects_Info();
			show_haptic = true;
		}

		SDL_free((void *)joysticks);

		return;
	}

	for (const char* rawPath = FS_GetNextRawPath(NULL); rawPath != NULL; rawPath = FS_GetNextRawPath(rawPath))
	{
		snprintf(controllerdb, MAX_OSPATH, "%s/gamecontrollerdb.txt", rawPath);
		nummappings = SDL_AddGamepadMappingsFromFile(controllerdb);
		if (nummappings > 0)
			Com_Printf ("%d mappings loaded from gamecontrollerdb.txt\n", nummappings);
	}

	for (int i = 0; i < numjoysticks; i++)
	{
		joystick = SDL_OpenJoystick(joysticks[i]);
		if (!joystick)
		{
			Com_Printf ("Couldn't open joystick %d: %s.\n", i+1, SDL_GetError());
			continue;	// try next joystick
		}

		const char* joystick_name = SDL_GetJoystickName(joystick);
		const int name_len = strlen(joystick_name);

		Com_Printf ("Trying joystick %d, '%s'\n", i+1, joystick_name);

		// Ugly hack to detect IMU-only devices - works for Switch controllers at least
		if ( name_len > 6 && strstr(joystick_name + name_len - 6, "IMU") )
		{
#ifndef NO_SDL_GYRO
			SDL_CloseJoystick(joystick);
			joystick = NULL;
			Com_Printf ("Skipping IMU device.\n");

#else	// if it's not a Left JoyCon, use it as Gyro
			qboolean using_imu = !imu_joystick && !( strstr(joystick_name, "Joy-Con") && strstr(joystick_name, "L") );
			Com_Printf ("IMU device found... ");
			SDL_CloseJoystick(joystick);
			joystick = NULL;

			if (using_imu)
			{
				imu_joystick = SDL_OpenJoystick(joysticks[i]);
				if (imu_joystick)
				{
					show_gyro = true;
					Com_Printf ("using it as Gyro sensor.\n");
				}
				else
				{
					Com_Printf ("\nCouldn't open IMU: %s.\n", SDL_GetError());
				}
			}
			else
			{
				Com_Printf ("skipping.\n");
			}
#endif
			continue;
		}

		Com_Printf ("Buttons = %d, Axes = %d, Hats = %d\n", SDL_GetNumJoystickButtons(joystick),
			    SDL_GetNumJoystickAxes(joystick), SDL_GetNumJoystickHats(joystick));
		is_controller = SDL_IsGamepad(joysticks[i]);

		if (!is_controller)
		{
			char joystick_guid[65] = {0};
			SDL_GUID guid = SDL_GetJoystickGUIDForID(joysticks[i]);

			SDL_GUIDToString(guid, joystick_guid, 64);

			Com_Printf ("To identify joystick as Gamepad, provide its config by either:\n"
				" * Putting 'gamecontrollerdb.txt' file in your game directory.\n"
				" * Or setting SDL_GAMECONTROLLERCONFIG environment variable. E.g.:\n");
			Com_Printf ("SDL_GAMECONTROLLERCONFIG='%s,%s,leftx:a0,lefty:a1,rightx:a2,righty:a3,back:b1,...'\n", joystick_guid, joystick_name);
		}

		SDL_CloseJoystick(joystick);
		joystick = NULL;

		if (is_controller && !controller)
		{
			controller = SDL_OpenGamepad(joysticks[i]);
			if (!controller)
			{
				Com_Printf("SDL Gamepad error: %s.\n", SDL_GetError());
				continue;	// try next joystick
			}

			show_gamepad = true;
			Com_Printf("Enabled as Gamepad, settings:\n%s\n",
				   SDL_GetGamepadMapping(controller));

#ifndef NO_SDL_GYRO

			const qboolean found_gyro =
				SDL_GamepadHasSensor(controller, SDL_SENSOR_GYRO)
				&& SDL_SetGamepadSensorEnabled(
					controller, SDL_SENSOR_GYRO, true);

			const qboolean found_accel =
				SDL_GamepadHasSensor(controller, SDL_SENSOR_ACCEL)
				&& SDL_SetGamepadSensorEnabled(
					controller, SDL_SENSOR_ACCEL, true);

			if (found_gyro && found_accel)
			{
				show_gyro = true;
				Com_Printf("Sensors enabled: Gyro at %.2f Hz, Accelerometer at %.2f Hz\n",
					SDL_GetGamepadSensorDataRate(controller, SDL_SENSOR_GYRO),
					SDL_GetGamepadSensorDataRate(controller, SDL_SENSOR_ACCEL));
			}
			else
			{
				if (!found_gyro)
				{
					Com_Printf("Gyro sensor not found.\n");
				}

				if (!found_accel)
				{
					Com_Printf("Accelerometer sensor not found.\n");
				}

				// Both were required for gyro support, so disable them completely.
				SDL_SetGamepadSensorEnabled(controller, SDL_SENSOR_GYRO, false);
				SDL_SetGamepadSensorEnabled(controller, SDL_SENSOR_ACCEL, false);
			}

			bool hasLED = SDL_GetBooleanProperty(SDL_GetGamepadProperties(controller), SDL_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN, false);
			if (hasLED)
			{
				SDL_SetGamepadLED(controller, 0, 80, 0);	// green light
			}

#endif	// !NO_SDL_GYRO

			joystick_haptic = SDL_OpenHapticFromJoystick(SDL_GetGamepadJoystick(controller));

			if (joystick_haptic &&
				(SDL_GetHapticFeatures(joystick_haptic) & SDL_HAPTIC_SINE) == 0)
			{
				/* Disable haptic for joysticks without SINE */
				SDL_CloseHaptic(joystick_haptic);
				joystick_haptic = NULL;
			}

			if (joystick_haptic)
			{
				IN_Haptic_Effects_Info();
				show_haptic = true;
			}

			bool hasRumble = SDL_GetBooleanProperty(SDL_GetGamepadProperties(controller), SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
			if (hasRumble)
			{
				show_haptic = true;
				Com_Printf("Rumble support available.\n");
			}
			else
			{
				Com_Printf("Gamepad doesn't support rumble.\n");
			}

#ifndef NO_SDL_GYRO	// "native SDL gyro" exits when finding a single working gamepad
			break;
#endif
		}
	}

	SDL_free((void *)joysticks);
	IN_GamepadLabels_Changed();
	IN_GamepadConfirm_Changed();
	IN_GamepadTrigger_Changed();
	IN_InitGyro();
}

typedef struct
{
	float yawspeed;
	float pitchspeed;
	float extra_yawspeed;
	float extra_pitchspeed;
	float ramp_time;
} joy_preset_t;

static const joy_preset_t joy_presets[] = {
	[0] = {   0.0f,   0.0f,   0.0f,   0.0f, 0.00f },
	[1] = {  50.0f,  50.0f,  60.0f,   0.0f, 0.50f },
	[2] = {  80.0f,  50.0f, 150.0f, 120.0f, 0.30f },
	[3] = { 160.0f, 120.0f, 220.0f,   0.0f, 0.35f }, // Default
	[4] = { 240.0f, 200.0f, 220.0f,   0.0f, 0.30f },
	[5] = { 380.0f, 240.0f,   0.0f,   0.0f, 0.00f },
	[6] = { 450.0f, 300.0f,   0.0f,   0.0f, 0.00f },
	[7] = { 500.0f, 500.0f,   0.0f,   0.0f, 0.00f },
	[8] = { 720.0f, 720.0f,   0.0f,   0.0f, 0.00f },
};

void
IN_ApplyJoyPreset(void)
{
	const int final_preset = ARRLEN(joy_presets) - 1;
	const int i = lroundf(Q_clamp(joy_sensitivity->value, 0, final_preset));

	Cvar_SetValue("joy_yawspeed", joy_presets[i].yawspeed);
	Cvar_SetValue("joy_pitchspeed",
				  joy_presets[i].pitchspeed * Q_signf(joy_pitchspeed->value));
	Cvar_SetValue("joy_extra_yawspeed", joy_presets[i].extra_yawspeed);
	Cvar_SetValue("joy_extra_pitchspeed", joy_presets[i].extra_pitchspeed);
	Cvar_SetValue("joy_ramp_time", joy_presets[i].ramp_time);

	joy_sensitivity->modified = false;
}

#define EQF(a, b) (fabsf((a) - (b)) < 1.0e-6f)
qboolean
IN_MatchJoyPreset(void)
{
	const int num_presets = ARRLEN(joy_presets);

	for (int i = 0; i < num_presets; i++)
	{
		if (EQF(joy_presets[i].yawspeed, joy_yawspeed->value)
			&& EQF(joy_presets[i].pitchspeed, fabsf(joy_pitchspeed->value))
			&& EQF(joy_presets[i].extra_yawspeed, joy_extra_yawspeed->value)
			&& EQF(joy_presets[i].extra_pitchspeed, joy_extra_pitchspeed->value)
			&& EQF(joy_presets[i].ramp_time, joy_ramp_time->value))
		{
			Cvar_SetValue("joy_sensitivity", i);
			joy_sensitivity->modified = false;
			return true;
		}
	}

	return false;
}
#undef EQF

/*
 * Initializes the backend
 */
void
IN_Init(void)
{
	Com_Printf("------- input initialization -------\n");

	mouse_x = mouse_y = 0;
	joystick_left_x = joystick_left_y = joystick_right_x = joystick_right_y = 0;
	joy_active_layout = LAYOUT_NONE;
	gyro_active = gyro_enabled = false;

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
	joy_haptic_distance = Cvar_Get("joy_haptic_distance", "100.0", CVAR_ARCHIVE);
	haptic_feedback_filter = Cvar_Get("joy_haptic_filter", default_haptic_filter, CVAR_ARCHIVE);

	joy_sensitivity = Cvar_Get("joy_sensitivity", "3", CVAR_ARCHIVE);
	joy_advanced = Cvar_Get("joy_advanced", "0", CVAR_ARCHIVE);
	joy_yawspeed = Cvar_Get("joy_yawspeed", "160", CVAR_ARCHIVE);
	joy_pitchspeed = Cvar_Get("joy_pitchspeed", "120", CVAR_ARCHIVE);
	joy_extra_yawspeed = Cvar_Get("joy_extra_yawspeed", "220", CVAR_ARCHIVE);
	joy_extra_pitchspeed = Cvar_Get("joy_extra_pitchspeed", "0", CVAR_ARCHIVE);
	joy_ramp_time = Cvar_Get("joy_ramp_time", "0.35", CVAR_ARCHIVE);
	joy_outer_threshold = Cvar_Get("joy_outer_threshold", "0.02", CVAR_ARCHIVE);
	joy_forwardsensitivity = Cvar_Get("joy_forwardsensitivity", "1.0", CVAR_ARCHIVE);
	joy_sidesensitivity = Cvar_Get("joy_sidesensitivity", "1.0", CVAR_ARCHIVE);

	joy_labels = Cvar_Get("joy_labels", "-1", CVAR_ARCHIVE);
	joy_confirm = Cvar_Get("joy_confirm", "-1", CVAR_ARCHIVE);
	joy_layout = Cvar_Get("joy_layout", "0", CVAR_ARCHIVE);
	joy_left_expo = Cvar_Get("joy_left_expo", "2.0", CVAR_ARCHIVE);
	joy_left_snapaxis = Cvar_Get("joy_left_snapaxis", "0.15", CVAR_ARCHIVE);
	joy_left_deadzone = Cvar_Get("joy_left_deadzone", "0.16", CVAR_ARCHIVE);
	joy_right_expo = Cvar_Get("joy_right_expo", "2.0", CVAR_ARCHIVE);
	joy_right_snapaxis = Cvar_Get("joy_right_snapaxis", "0.15", CVAR_ARCHIVE);
	joy_right_deadzone = Cvar_Get("joy_right_deadzone", "0.16", CVAR_ARCHIVE);
	joy_trigger = Cvar_Get("joy_trigger", "0.2", CVAR_ARCHIVE);
	joy_flick_threshold = Cvar_Get("joy_flick_threshold", "0.65", CVAR_ARCHIVE);
	joy_flick_smoothed = Cvar_Get("joy_flick_smoothed", "16", CVAR_ARCHIVE);

	gyro_mode = Cvar_Get("gyro_mode", "2", CVAR_ARCHIVE);
	gyro_space = Cvar_Get("gyro_space", "1", CVAR_ARCHIVE);
	gyro_local_roll = Cvar_Get("gyro_local_roll", "1", CVAR_ARCHIVE);
	gyro_yawsensitivity = Cvar_Get("gyro_yawsensitivity", "2.5", CVAR_ARCHIVE);
	gyro_pitchsensitivity = Cvar_Get("gyro_pitchsensitivity", "2.5", CVAR_ARCHIVE);
	gyro_tightening = Cvar_Get("gyro_tightening", "3.5", CVAR_ARCHIVE);
	gyro_smoothing = Cvar_Get("gyro_smoothing", "2.5", CVAR_ARCHIVE);
	gyro_smoothing_window = Cvar_Get("gyro_smoothing_window", "0.125", CVAR_ARCHIVE);
	gyro_acceleration = Cvar_Get("gyro_acceleration", "0", CVAR_ARCHIVE);
	gyro_accel_multiplier = Cvar_Get("gyro_accel_multiplier", "2.0", CVAR_ARCHIVE);
	gyro_accel_lower_thresh = Cvar_Get("gyro_accel_lower_thresh", "0.0", CVAR_ARCHIVE);
	gyro_accel_upper_thresh = Cvar_Get("gyro_accel_upper_thresh", "75.0", CVAR_ARCHIVE);
	gyro_calibration_a = Cvar_Get("gyro_calibration_a", "0.0", CVAR_ARCHIVE);
	gyro_calibration_x = Cvar_Get("gyro_calibration_x", "0.0", CVAR_ARCHIVE);
	gyro_calibration_y = Cvar_Get("gyro_calibration_y", "0.0", CVAR_ARCHIVE);
	gyro_calibration_z = Cvar_Get("gyro_calibration_z", "0.0", CVAR_ARCHIVE);

	windowed_pauseonfocuslost = Cvar_Get("vid_pauseonfocuslost", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	windowed_mouse = Cvar_Get("windowed_mouse", "1", CVAR_USERINFO | CVAR_ARCHIVE);

	Cmd_AddCommand("+mlook", IN_MLookDown);
	Cmd_AddCommand("-mlook", IN_MLookUp);

	Cmd_AddCommand("+joyaltselector", IN_JoyAltSelectorDown);
	Cmd_AddCommand("-joyaltselector", IN_JoyAltSelectorUp);
	Cmd_AddCommand("+gyroaction", IN_GyroActionDown);
	Cmd_AddCommand("-gyroaction", IN_GyroActionUp);

	if (!SDL_WasInit(SDL_INIT_EVENTS))
	{
		if ((SDL_InitSubSystem(SDL_INIT_EVENTS)) != 0)
		{
			Com_Error(ERR_FATAL, "Couldn't initialize SDL event subsystem:%s\n", SDL_GetError());
		}
	}

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

		SDL_CloseHaptic(joystick_haptic);
		joystick_haptic = NULL;
	}
}

static void
IN_Controller_Shutdown(qboolean notify_user)
{
	if (notify_user)
	{
		Com_Printf("- Gamepad disconnected -\n");
	}

	IN_Haptic_Shutdown();

	if (controller)
	{
		SDL_CloseGamepad(controller);
		controller = NULL;
	}
	show_gamepad = show_gyro = show_haptic = gyro_active = gyro_enabled = false;
	joystick_left_x = joystick_left_y = joystick_right_x = joystick_right_y = 0;
	joy_active_layout = LAYOUT_NONE;

#ifdef NO_SDL_GYRO
	if (imu_joystick)
	{
		SDL_CloseJoystick(imu_joystick);
		imu_joystick = NULL;
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

	Cmd_RemoveCommand("+joyaltselector");
	Cmd_RemoveCommand("-joyaltselector");
	Cmd_RemoveCommand("+gyroaction");
	Cmd_RemoveCommand("-gyroaction");

	Com_Printf("Shutting down input.\n");

	IN_Controller_Shutdown(false);

	const Uint32 subsystems = SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_EVENTS;
	if (SDL_WasInit(subsystems) == subsystems)
	{
		SDL_QuitSubSystem(subsystems);
	}
}

/* ------------------------------------------------------------------ */

void
IN_GetClipboardText(char *out, size_t n)
{
	char *s = SDL_GetClipboardText();

	if (!s || *s == '\0')
	{
		*out = '\0';
		return;
	}

	Q_strlcpy(out, s, n - 1);

	SDL_free(s);
}

/* Copy string s to the clipboard.
   Returns 0 on success, 1 otherwise.
*/
int
IN_SetClipboardText(const char *s)
{
	bool res = SDL_SetClipboardText(s);

	return !res;
}
