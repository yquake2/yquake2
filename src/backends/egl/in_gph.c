/**
 *
 *  Copyright (C) 2011 Scott R. Smith
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 *
 */

#include "in_gph.h"

int8_t      Volume = 20;
uint8_t     VolumeDirection = 0;
int32_t     HandleStick = 0;
int32_t     HandleGpio = 0;
uint32_t    ButtonsCurrent = 0;
uint32_t    ButtonsPrevious = 0;
uint32_t    Ticksup = 0;
uint32_t    Ticksdown = 0;

SDLKey gph_map[GPH_MAX_BUTTONS] = {
    SDLK_UP,		// GPH_UP
    0,			    // GPH_UP_LEFT
    SDLK_LEFT,		// GPH_LEFT
    0,			    // GPH_DOWN_LEFT
    SDLK_DOWN,		// GPH_DOWN
    0,			    // GPH_DOWN_RIGHT
    SDLK_RIGHT,		// GPH_RIGHT
    0,			    // GPH_UP_RIGHT
    SDLK_RETURN,	// GPH_START
    SDLK_ESCAPE,	// GPH_SELECT
    SDLK_l,		    // GPH_L
    SDLK_r,		    // GPH_R
    SDLK_a,		    // GPH_A
    SDLK_b,		    // GPH_B`
    SDLK_x,		    // GPH_Y
    SDLK_y,		    // GPH_X
    0,			    // GPH_VOL_UP
    0,			    // GPH_VOL_DOWN
    SDLK_p,		    // GPH_PUSH
    SDLK_q,		    // GPH_HELP1
    SDLK_1,         // AB
    SDLK_F1,        // AX
    SDLK_2,         // AY
    SDLK_3,         // BX
    SDLK_4,         // BY
    SDLK_5,         // XY
    0,
    0,
    0,
    0,
    0,
    0
};

void GPH_Close( void )
{
    close(HandleGpio);
#if defined(CAANOO)
    close(HandleStick);
#endif
}

int8_t GPH_Open( void )
{
#if defined(CAANOO)
    HandleStick = open( "/dev/input/event1", O_RDONLY|O_NONBLOCK );
    if (HandleStick < 0)
    {
        printf( "GPH Input Analog stick OPEN FAIL\n");
        return 1;
    }
    else
    {
        printf("GPH Input Caanoo stick loaded\n");
    }
#endif
    HandleGpio = open( "/dev/GPIO", O_RDWR|O_NDELAY );
    if (HandleGpio < 0)
    {
        printf( "GPH Input GPIO OPEN FAIL\n");
        return 1;
    }
    else
    {
        printf("GPH Input Wiz/Caanoo buttons loaded\n");
    }

    return 0;
}

void GPH_Input( void )
{
    ButtonsPrevious = ButtonsCurrent;

    GPH_ReadButtons();
#if defined(CAANOO)
    GPH_ReadAnalog();
#endif

    return ButtonsCurrent;
}

void GPH_ReadButtons( void )
{
    uint8_t button;
    uint16_t index;

    read( HandleGpio, &ButtonsCurrent, 4 );

#if defined(WIZ)
    // Use diagonals
    if (  (ButtonsCurrent&GPH_BUTTON(GPH_UP_LEFT)) &&
        !((ButtonsCurrent&GPH_BUTTON(GPH_UP)) || (ButtonsCurrent&GPH_BUTTON(GPH_LEFT))))
    {
        ButtonsCurrent |= GPH_BUTTON(GPH_UP);
        ButtonsCurrent |= GPH_BUTTON(GPH_LEFT);
    }
    if (  (ButtonsCurrent&GPH_BUTTON(GPH_UP_RIGHT)) &&
        !((ButtonsCurrent&GPH_BUTTON(GPH_UP)) || (ButtonsCurrent&GPH_BUTTON(GPH_RIGHT))))
    {
        ButtonsCurrent |= GPH_BUTTON(GPH_UP);
        ButtonsCurrent |= GPH_BUTTON(GPH_RIGHT);
    }
    if (  (ButtonsCurrent&GPH_BUTTON(GPH_DOWN_LEFT)) &&
        !((ButtonsCurrent&GPH_BUTTON(GPH_DOWN)) || (ButtonsCurrent&GPH_BUTTON(GPH_LEFT))))
    {
        ButtonsCurrent |= GPH_BUTTON(GPH_DOWN);
        ButtonsCurrent |= GPH_BUTTON(GPH_LEFT);
    }
    if (  (ButtonsCurrent&GPH_BUTTON(GPH_DOWN_RIGHT)) &&
        !((ButtonsCurrent&GPH_BUTTON(GPH_DOWN)) || (ButtonsCurrent&GPH_BUTTON(GPH_RIGHT))))
    {
        ButtonsCurrent |= GPH_BUTTON(GPH_DOWN);
        ButtonsCurrent |= GPH_BUTTON(GPH_RIGHT);
    }
#endif

    // Map combos
    if ( (ButtonsCurrent&GPH_BUTTON(GPH_A)) && (ButtonsCurrent&GPH_BUTTON(GPH_B)) )
    {
        ButtonsCurrent ^= GPH_BUTTON(GPH_A);
        ButtonsCurrent ^= GPH_BUTTON(GPH_B);
        ButtonsCurrent |= GPH_BUTTON(GPH_AB);
    }
    if ( (ButtonsCurrent&GPH_BUTTON(GPH_A)) && (ButtonsCurrent&GPH_BUTTON(GPH_X)) )
    {
        ButtonsCurrent ^= GPH_BUTTON(GPH_A);
        ButtonsCurrent ^= GPH_BUTTON(GPH_X);
        ButtonsCurrent |= GPH_BUTTON(GPH_AX);
    }
    if ( (ButtonsCurrent&GPH_BUTTON(GPH_A)) && (ButtonsCurrent&GPH_BUTTON(GPH_Y)) )
    {
        ButtonsCurrent ^= GPH_BUTTON(GPH_A);
        ButtonsCurrent ^= GPH_BUTTON(GPH_Y);
        ButtonsCurrent |= GPH_BUTTON(GPH_AY);
    }
    if ( (ButtonsCurrent&GPH_BUTTON(GPH_B)) && (ButtonsCurrent&GPH_BUTTON(GPH_X)) )
    {
        ButtonsCurrent ^= GPH_BUTTON(GPH_B);
        ButtonsCurrent ^= GPH_BUTTON(GPH_X);
        ButtonsCurrent |= GPH_BUTTON(GPH_BX);
    }
    if ( (ButtonsCurrent&GPH_BUTTON(GPH_B)) && (ButtonsCurrent&GPH_BUTTON(GPH_Y)) )
    {
        ButtonsCurrent ^= GPH_BUTTON(GPH_B);
        ButtonsCurrent ^= GPH_BUTTON(GPH_Y);
        ButtonsCurrent |= GPH_BUTTON(GPH_BY);
    }
    if ( (ButtonsCurrent&GPH_BUTTON(GPH_X)) && (ButtonsCurrent&GPH_BUTTON(GPH_Y)) )
    {
        ButtonsCurrent ^= GPH_BUTTON(GPH_X);
        ButtonsCurrent ^= GPH_BUTTON(GPH_Y);
        ButtonsCurrent |= GPH_BUTTON(GPH_XY);
    }

#if defined(WIZ)
    // Volume Up
    if ((((ButtonsPrevious & GPH_BUTTON(GPH_VOL_UP)) == 0) && ((ButtonsCurrent & GPH_BUTTON(GPH_VOL_UP))> 0)) ||
        (((ButtonsPrevious & GPH_BUTTON(GPH_VOL_UP))  > 0) && ((ButtonsCurrent & GPH_BUTTON(GPH_VOL_UP))> 0) && ((SDL_GetTicks() - Ticksup) > VOLUME_TIME_RATE )) )
    {
        Ticksup = SDL_GetTicks();
        GPH_AdjustVolume(VOLUME_UP);
    }
    // Volume Down
    if ((((ButtonsPrevious & GPH_BUTTON(GPH_VOL_DOWN)) == 0) && ((ButtonsCurrent & GPH_BUTTON(GPH_VOL_DOWN))> 0)) ||
        (((ButtonsPrevious & GPH_BUTTON(GPH_VOL_DOWN))  > 0) && ((ButtonsCurrent & GPH_BUTTON(GPH_VOL_DOWN))> 0) && ((SDL_GetTicks() - Ticksdown) > VOLUME_TIME_RATE )) )
    {
        Ticksdown = SDL_GetTicks();
        GPH_AdjustVolume(VOLUME_DOWN);
    }
#endif
}

#if defined(CAANOO)
#define ZERO_POINT 128
#define OPT_DEADZONE 64

int8_t up, down, left, right = 0;

void GPH_ReadAnalog( void )
{
    struct input_event event[16];
    int32_t rbytes;
    uint32_t i;

    rbytes = read( HandleStick, &event, sizeof(event) );
    if (rbytes > 0)
    {
      for (i=0; i<rbytes/sizeof(struct input_event); i++)
      {
	  switch (event[i].type)
	  {
	      case EV_ABS:
		  switch (event[i].code)
		  {
		      case ABS_X:
			  if (event[i].value>ZERO_POINT+OPT_DEADZONE) {
			      right = 1;
			  } else {
			      right = 0;
			  }

			  if (event[i].value<ZERO_POINT-OPT_DEADZONE) {
			      left = 1;
			  } else {
			      left = 0;
			  }

			  //printf("x = 0x%x\n", event[i].value);
			  break;
		      case ABS_Y:
			  if (event[i].value<ZERO_POINT-OPT_DEADZONE) {
			      up = 1;
			  } else {
			      up = 0;
			  }

			  if (event[i].value>ZERO_POINT+OPT_DEADZONE) {
			      down = 1;
			  } else {
			      down = 0;
			  }

			  //printf("y = 0x%x\n", event[i].value);
			  break;
		      default:
			  //printf("type = 0x%x, code = 0x%x\n", event[i].type, event[i].code);
			  break;
		  }
		  break;
	      default:
		  if (event[i].type) {
		      //printf("type = 0x%x, code = 0x%x\n", event[i].type, event[i].code);
		  }
		  break;
	  }
      }
    }

    if (right)
	ButtonsCurrent |= GPH_BUTTON(GPH_RIGHT);

    if (left)
	ButtonsCurrent |= GPH_BUTTON(GPH_LEFT);

    if (up)
	ButtonsCurrent |= GPH_BUTTON(GPH_UP);

    if (down)
	ButtonsCurrent |= GPH_BUTTON(GPH_DOWN);
}
#endif /* if defined(CAANOO) */

#if defined(WIZ)
void GPH_AdjustVolume( uint8_t direction )
{
	if (direction != VOLUME_NOCHG)
	{
		if (Volume <= 10)
		{
			if (direction == VOLUME_UP)   Volume += VOLUME_CHANGE_RATE/2;
			if (direction == VOLUME_DOWN) Volume -= VOLUME_CHANGE_RATE/2;
		}
		else
		{
			if (direction == VOLUME_UP)   Volume += VOLUME_CHANGE_RATE;
			if (direction == VOLUME_DOWN) Volume -= VOLUME_CHANGE_RATE;
		}

		if (Volume < VOLUME_MIN) Volume = VOLUME_MIN;
		if (Volume > VOLUME_MAX) Volume = VOLUME_MAX;

		printf( "Volume Change: %i\n", Volume );

		unsigned long soundDev = open("/dev/mixer", O_RDWR);
		if (soundDev)
		{
			int vol = ((Volume << 8) | Volume);
			ioctl(soundDev, SOUND_MIXER_WRITE_PCM, &vol);
			close(soundDev);
		}
	}
}
#endif /* if defined(WIZ) */

int8_t GPH_GetButttonState( uint32_t button )
{
    uint32_t current_state;
    uint32_t previous_state;

    current_state   = button & ButtonsCurrent;
    previous_state  = button & ButtonsPrevious;

            // Was released and now pressed
    if (previous_state == 0 && current_state == button)
    {
        //printf ("GPH_RELEASED_TO_PRESSED %X\n", button );
        return GPH_RELEASED_TO_PRESSED;
    }
    else    // Was pressed and still being pressed
    if (previous_state == button && current_state == button)
    {
        //printf ("GPH_PRESSED %X\n", button );
        return GPH_PRESSED;
    }
    else    // Was pressed and now released
    if (previous_state == button && current_state == 0)
    {
        //printf ("GPH_PRESSED_TO_RELEASED %X\n", button );
        return GPH_PRESSED_TO_RELEASED;
    }
    else    // Was released and still released
    if (previous_state == 0 && current_state == 0)
    {
        //printf ("GPH_RELEASED %X\n", button );
        return GPH_RELEASED;
    }
    else    // Shouldnt ever have this state
    {
        printf( "GPH Input Wiz/Caanoo detected unexpected button state: previous %X current %X\n", ButtonsPrevious, ButtonsCurrent );
        return GPH_NONE;
    }
}
