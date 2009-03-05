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

#ifdef OPENGL
#include "../../../refresh/opengl/header/local.h"
#else
#include "../ref_soft/r_local.h"
#endif

#include "../../../client/input/keys.h"
#include "../posix.h"

static qboolean	mlooking;

// state struct passed in Init
static in_state_t	*in_state;

static cvar_t *sensitivity;
static cvar_t *exponential_speedup;
static cvar_t *lookstrafe;
static cvar_t *m_side;
static cvar_t *m_yaw;
static cvar_t *m_pitch;
static cvar_t *m_forward;
static cvar_t *freelook;

static cvar_t	*m_filter;
static cvar_t	*in_mouse;
static int mouse_x, mouse_y;
static int old_mouse_x, old_mouse_y;
static qboolean	mouse_avail;
static int mouse_buttonstate;
static int mouse_oldbuttonstate;

in_state_t *getState() {
  return in_state;
}

static void Force_CenterView_f (void)
{
	in_state->viewangles[PITCH] = 0;
}

static void RW_IN_MLookDown (void) 
{ 
	mlooking = true; 
}

static void RW_IN_MLookUp (void) 
{
	mlooking = false;
	in_state->IN_CenterView_fp ();
}

void RW_IN_Init(in_state_t *in_state_p)
{
  in_state = in_state_p;
  // mouse variables
  m_filter = ri.Cvar_Get ("m_filter", "0", 0);
  in_mouse = ri.Cvar_Get ("in_mouse", "0", CVAR_ARCHIVE);
    
  freelook = ri.Cvar_Get( "freelook", "0", 0 );
  lookstrafe = ri.Cvar_Get ("lookstrafe", "0", 0);
  sensitivity = ri.Cvar_Get ("sensitivity", "3", 0);
  exponential_speedup = ri.Cvar_Get("exponential_speedup", "0", 
				    CVAR_ARCHIVE);
  
  m_pitch = ri.Cvar_Get ("m_pitch", "0.022", 0);
  m_yaw = ri.Cvar_Get ("m_yaw", "0.022", 0);
  m_forward = ri.Cvar_Get ("m_forward", "1", 0);
  m_side = ri.Cvar_Get ("m_side", "0.8", 0);
  
  ri.Cmd_AddCommand ("+mlook", RW_IN_MLookDown);
  ri.Cmd_AddCommand ("-mlook", RW_IN_MLookUp);
  
  ri.Cmd_AddCommand ("force_centerview", Force_CenterView_f);
  
  mouse_x = mouse_y = 0.0;  
  mouse_avail = true;	
  
  RW_IN_PlatformInit();
}


void RW_IN_Shutdown(void)
{
  if (mouse_avail) {
    RW_IN_Activate (false);
    
    mouse_avail = false;
    
    ri.Cmd_RemoveCommand ("+mlook");
    ri.Cmd_RemoveCommand ("-mlook");
    ri.Cmd_RemoveCommand ("force_centerview");
  }
}

/*
===========
IN_Commands
===========
*/
void doneMouse();

void RW_IN_Commands (void)
{
  int i;
  
  if (mouse_avail) { 
    getMouse(&mouse_x, &mouse_y, &mouse_buttonstate);
    for (i=0 ; i<3 ; i++) {
      if ( (mouse_buttonstate & (1<<i)) && !(mouse_oldbuttonstate & (1<<i)) )
	in_state->Key_Event_fp (K_MOUSE1 + i, true);
      
      if ( !(mouse_buttonstate & (1<<i)) && (mouse_oldbuttonstate & (1<<i)) )
	in_state->Key_Event_fp (K_MOUSE1 + i, false);
    }
    if ( (mouse_buttonstate & (1<<3)) && !(mouse_oldbuttonstate & (1<<3)) )
      in_state->Key_Event_fp (K_MOUSE4, true);
    if ( !(mouse_buttonstate & (1<<3)) && (mouse_oldbuttonstate & (1<<3)) )
      in_state->Key_Event_fp (K_MOUSE4, false);
    
    if ( (mouse_buttonstate & (1<<4)) && !(mouse_oldbuttonstate & (1<<4)) )
      in_state->Key_Event_fp (K_MOUSE5, true);
    if ( !(mouse_buttonstate & (1<<4)) && (mouse_oldbuttonstate & (1<<4)) )
      in_state->Key_Event_fp (K_MOUSE5, false);
    
    mouse_oldbuttonstate = mouse_buttonstate;
  }
}

/*
===========
IN_Move
===========
*/
void RW_IN_Move (usercmd_t *cmd)
{
  if (mouse_avail) {
    getMouse(&mouse_x, &mouse_y, &mouse_buttonstate);
    if (m_filter->value)
      {
	mouse_x = (mouse_x + old_mouse_x) * 0.5;
	mouse_y = (mouse_y + old_mouse_y) * 0.5;
      }
    
    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;
    
    if (mouse_x || mouse_y) {
      if (!exponential_speedup->value) {
	mouse_x *= sensitivity->value;
	mouse_y *= sensitivity->value;
      }
      else {
	if (mouse_x > MOUSE_MIN || mouse_y > MOUSE_MIN || 
	    mouse_x < -MOUSE_MIN || mouse_y < -MOUSE_MIN) {
	  mouse_x = (mouse_x*mouse_x*mouse_x)/4;
	  mouse_y = (mouse_y*mouse_y*mouse_y)/4;
	  if (mouse_x > MOUSE_MAX)
	    mouse_x = MOUSE_MAX;
	  else if (mouse_x < -MOUSE_MAX)
	    mouse_x = -MOUSE_MAX;
	  if (mouse_y > MOUSE_MAX)
	    mouse_y = MOUSE_MAX;
	  else if (mouse_y < -MOUSE_MAX)
	    mouse_y = -MOUSE_MAX;
	}
      }
      // add mouse X/Y movement to cmd
      if ( (*in_state->in_strafe_state & 1) || 
	   (lookstrafe->value && mlooking ))
	cmd->sidemove += m_side->value * mouse_x;
      else
	in_state->viewangles[YAW] -= m_yaw->value * mouse_x;
      
      if ( (mlooking || freelook->value) && 
	   !(*in_state->in_strafe_state & 1))
	{
	  in_state->viewangles[PITCH] += m_pitch->value * mouse_y;
	}
      else
	{
	  cmd->forwardmove -= m_forward->value * mouse_y;
	}
      doneMouse();
    }
  }
}

void RW_IN_Frame (void)
{
}
