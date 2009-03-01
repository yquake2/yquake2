/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cl.input.c  -- builds an intended movement command to send to the server

#include "client.h"

cvar_t	*cl_nodelta;

extern	unsigned	sys_frame_time;
unsigned	frame_msec;
unsigned	old_sys_frame_time;

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition


Key_Event (int key, qboolean down, unsigned time);

  +mlook src time

===============================================================================
*/


kbutton_t	in_klook;
kbutton_t	in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed, in_use, in_attack;
kbutton_t	in_up, in_down;

int			in_impulse;


void KeyDown (kbutton_t *b)
{
	int		k;
	char	*c;
	
	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1;		// typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return;		// repeating key
	
	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else
	{
		Com_Printf ("Three keys down for a button!\n");
		return;
	}
	
	if (b->state & 1)
		return;		// still down

	// save timestamp
	c = Cmd_Argv(2);
	b->downtime = atoi(c);
	if (!b->downtime)
		b->downtime = sys_frame_time - 100;

	b->state |= 1 + 2;	// down + impulse down
}

void KeyUp (kbutton_t *b)
{
	int		k;
	char	*c;
	unsigned	uptime;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
	{ // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;	// impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)
	if (b->down[0] || b->down[1])
		return;		// some other key is still holding it down

	if (!(b->state & 1))
		return;		// still up (this should not happen)

	// save timestamp
	c = Cmd_Argv(2);
	uptime = atoi(c);
	if (uptime)
		b->msec += uptime - b->downtime;
	else
		b->msec += 10;

	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
}

void IN_KLookDown (void) {KeyDown(&in_klook);}
void IN_KLookUp (void) {KeyUp(&in_klook);}
void IN_UpDown(void) {KeyDown(&in_up);}
void IN_UpUp(void) {KeyUp(&in_up);}
void IN_DownDown(void) {KeyDown(&in_down);}
void IN_DownUp(void) {KeyUp(&in_down);}
void IN_LeftDown(void) {KeyDown(&in_left);}
void IN_LeftUp(void) {KeyUp(&in_left);}
void IN_RightDown(void) {KeyDown(&in_right);}
void IN_RightUp(void) {KeyUp(&in_right);}
void IN_ForwardDown(void) {KeyDown(&in_forward);}
void IN_ForwardUp(void) {KeyUp(&in_forward);}
void IN_BackDown(void) {KeyDown(&in_back);}
void IN_BackUp(void) {KeyUp(&in_back);}
void IN_LookupDown(void) {KeyDown(&in_lookup);}
void IN_LookupUp(void) {KeyUp(&in_lookup);}
void IN_LookdownDown(void) {KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {KeyUp(&in_lookdown);}
void IN_MoveleftDown(void) {KeyDown(&in_moveleft);}
void IN_MoveleftUp(void) {KeyUp(&in_moveleft);}
void IN_MoverightDown(void) {KeyDown(&in_moveright);}
void IN_MoverightUp(void) {KeyUp(&in_moveright);}

void IN_SpeedDown(void) {KeyDown(&in_speed);}
void IN_SpeedUp(void) {KeyUp(&in_speed);}
void IN_StrafeDown(void) {KeyDown(&in_strafe);}
void IN_StrafeUp(void) {KeyUp(&in_strafe);}

void IN_AttackDown(void) {KeyDown(&in_attack);}
void IN_AttackUp(void) {KeyUp(&in_attack);}

void IN_UseDown (void) {KeyDown(&in_use);}
void IN_UseUp (void) {KeyUp(&in_use);}

void IN_Impulse (void) {in_impulse=atoi(Cmd_Argv(1));}

/*
===============
CL_KeyState

Returns the fraction of the frame that the key was down
===============
*/
float CL_KeyState (kbutton_t *key)
{
	float		val;
	int			msec;

	key->state &= 1;		// clear impulses

	msec = key->msec;
	key->msec = 0;

	if (key->state)
	{	// still down
		msec += sys_frame_time - key->downtime;
		key->downtime = sys_frame_time;
	}

	val = (float)msec / frame_msec;
	if (val < 0)
		val = 0;
	if (val > 1)
		val = 1;

	return val;
}




//==========================================================================

cvar_t	*cl_upspeed;
cvar_t	*cl_forwardspeed;
cvar_t	*cl_sidespeed;

cvar_t	*cl_yawspeed;
cvar_t	*cl_pitchspeed;

cvar_t	*cl_run;

cvar_t	*cl_anglespeedkey;


/*
================
CL_BaseMove

Applies keyboard input to usercmd
================
*/
void CL_BaseMove (usercmd_t *cmd)
{	
	int i;
	float	tspeed;
	float	mspeed;
	
	//adjust for turning speed
	if (in_speed.state & 1)
		tspeed = cls.frametime * cl_anglespeedkey->value;
	else
		tspeed = cls.frametime;
	
	//adjust for running speed
	if ( (in_speed.state & 1) ^ (int)(cl_run->value) )
		mspeed = 2;
	else
		mspeed = 1;
	
	//handle left/right on keyboard
	i = cls.netchan.outgoing_sequence & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	cl.cmd_time[i] = cls.realtime;	// for netgraph ping calculation
	
	if (in_strafe.state & 1)
	{	// keyboard strafe
		cmd->sidemove += mspeed * cl_sidespeed->value * CL_KeyState (&in_right);
		cmd->sidemove -= mspeed * cl_sidespeed->value * CL_KeyState (&in_left);
	}
	else
	{	// keyboard turn
		cl.viewangles[YAW] -= tspeed*cl_yawspeed->value*CL_KeyState (&in_right);
		cl.viewangles[YAW] += tspeed*cl_yawspeed->value*CL_KeyState (&in_left);
	}	

	//handle foward/back on keyboard
	if (in_klook.state & 1)
	{	// keyboard look
		cl.viewangles[PITCH] -= tspeed*cl_pitchspeed->value * CL_KeyState (&in_forward);
		cl.viewangles[PITCH] += tspeed*cl_pitchspeed->value * CL_KeyState (&in_back);
	}	
	else
	{	// keyboard move front/back
		cmd->forwardmove += mspeed * cl_forwardspeed->value * CL_KeyState (&in_forward);
		cmd->forwardmove -= mspeed * cl_forwardspeed->value * CL_KeyState (&in_back);
	}	
	
	// keyboard look up/down
	cl.viewangles[PITCH] -= tspeed*cl_pitchspeed->value * CL_KeyState(&in_lookup);
	cl.viewangles[PITCH] += tspeed*cl_pitchspeed->value * CL_KeyState(&in_lookdown);
	
	// keyboard strafe left/right
	cmd->sidemove += mspeed * cl_sidespeed->value * CL_KeyState (&in_moveright);
	cmd->sidemove -= mspeed * cl_sidespeed->value * CL_KeyState (&in_moveleft);

	// keyboard jump/crouch
	cmd->upmove += mspeed * cl_upspeed->value * CL_KeyState (&in_up);
	cmd->upmove -= mspeed * cl_upspeed->value * CL_KeyState (&in_down);
}

void CL_ClampPitch (void)
{
	float	pitch;

	pitch = SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[PITCH]);
	if (pitch > 180)
		pitch -= 360;

	if (cl.viewangles[PITCH] + pitch < -360)
		cl.viewangles[PITCH] += 360; // wrapped
	if (cl.viewangles[PITCH] + pitch > 360)
		cl.viewangles[PITCH] -= 360; // wrapped

	if (cl.viewangles[PITCH] + pitch > 89)
		cl.viewangles[PITCH] = 89 - pitch;
	if (cl.viewangles[PITCH] + pitch < -89)
		cl.viewangles[PITCH] = -89 - pitch;
}

// CL_InitCmd
inline void CL_InitCmd (void)
{
	usercmd_t *cmd = &cl.cmds[ cls.netchan.outgoing_sequence & (CMD_BACKUP-1) ];

	// init the current cmd buffer
	memset(cmd, 0, sizeof(struct usercmd_s) );
}
	
// CL_RefreshCmd
void CL_RefreshCmd (void)
{	
	int i, ms;
	usercmd_t *cmd = &cl.cmds[ cls.netchan.outgoing_sequence & (CMD_BACKUP-1) ];

	//get delta for this sample.
	frame_msec = sys_frame_time - old_sys_frame_time;	

	// bounds checking
	if (frame_msec < 1)
		return;
	if (frame_msec > 1000)
		frame_msec = 500;

	// get basic movement from keyboard
	CL_BaseMove (cmd);

	// allow mice or other external controllers to add to the move
	IN_Move (cmd);

	// update cmd viewangles for CL_PredictMove
	CL_ClampPitch ();
	for (i=0 ; i<3 ; i++)
		cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);

	// update cmd->msec for CL_PredictMove
	ms = cls.frametime * 1000;
	if (ms > 250)
		ms = 100;
	cmd->msec = ms;

	//update counter
	old_sys_frame_time = sys_frame_time;
}

// CL_FinalizeCmd
void CL_FinalizeCmd (void)
{
	usercmd_t *cmd = &cl.cmds[ cls.netchan.outgoing_sequence & (CMD_BACKUP-1) ];

	//set any button hits that occured since last frame
	if ( in_attack.state & 3 )
		cmd->buttons |= BUTTON_ATTACK;
	in_attack.state &= ~2;

	if (in_use.state & 3)
		cmd->buttons |= BUTTON_USE;
	in_use.state &= ~2;

	if (anykeydown && cls.key_dest == key_game)
		cmd->buttons |= BUTTON_ANY;

	cmd->impulse = in_impulse;
	in_impulse = 0;

	// set the ambient light level at the player's current position
	cmd->lightlevel = (byte)cl_lightlevel->value;
}


void IN_CenterView (void)
{
	cl.viewangles[PITCH] = -SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[PITCH]);
}

/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	Cmd_AddCommand ("centerview",IN_CenterView);

	Cmd_AddCommand ("+moveup",IN_UpDown);
	Cmd_AddCommand ("-moveup",IN_UpUp);
	Cmd_AddCommand ("+movedown",IN_DownDown);
	Cmd_AddCommand ("-movedown",IN_DownUp);
	Cmd_AddCommand ("+left",IN_LeftDown);
	Cmd_AddCommand ("-left",IN_LeftUp);
	Cmd_AddCommand ("+right",IN_RightDown);
	Cmd_AddCommand ("-right",IN_RightUp);
	Cmd_AddCommand ("+forward",IN_ForwardDown);
	Cmd_AddCommand ("-forward",IN_ForwardUp);
	Cmd_AddCommand ("+back",IN_BackDown);
	Cmd_AddCommand ("-back",IN_BackUp);
	Cmd_AddCommand ("+lookup", IN_LookupDown);
	Cmd_AddCommand ("-lookup", IN_LookupUp);
	Cmd_AddCommand ("+lookdown", IN_LookdownDown);
	Cmd_AddCommand ("-lookdown", IN_LookdownUp);
	Cmd_AddCommand ("+strafe", IN_StrafeDown);
	Cmd_AddCommand ("-strafe", IN_StrafeUp);
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand ("+moveright", IN_MoverightDown);
	Cmd_AddCommand ("-moveright", IN_MoverightUp);
	Cmd_AddCommand ("+speed", IN_SpeedDown);
	Cmd_AddCommand ("-speed", IN_SpeedUp);
	Cmd_AddCommand ("+attack", IN_AttackDown);
	Cmd_AddCommand ("-attack", IN_AttackUp);
	Cmd_AddCommand ("+use", IN_UseDown);
	Cmd_AddCommand ("-use", IN_UseUp);
	Cmd_AddCommand ("impulse", IN_Impulse);
	Cmd_AddCommand ("+klook", IN_KLookDown);
	Cmd_AddCommand ("-klook", IN_KLookUp);

	cl_nodelta = Cvar_Get ("cl_nodelta", "0", 0);
}

/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd (void)
{
	sizebuf_t	buf;
	byte		data[128];
	int			i;
	usercmd_t	*cmd, *oldcmd;
	usercmd_t	nullcmd;
	int			checksumIndex;

	if (cls.state == ca_disconnected || cls.state == ca_connecting)
		return;

	if ( cls.state == ca_connected)
	{
		if (cls.netchan.message.cursize	|| curtime - cls.netchan.last_sent > 1000 ) {
	 		memset (&buf, 0, sizeof(buf));
			Com_DPrintf ("connected: flushing netchan (len=%d, %s)\n", cls.netchan.message.cursize, cls.netchan.message.data);
			Netchan_Transmit (&cls.netchan, 0, buf.data);	
		}
		return;
	}

	i = cls.netchan.outgoing_sequence & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	cl.cmd_time[i] = cls.realtime;	// for netgraph ping calculation

	//jec - prepare the pending user command for sending.
	CL_FinalizeCmd();

	cl.cmd = *cmd;

	// send a userinfo update if needed
	// send a userinfo update if needed
	if (userinfo_modified)
	{
		CL_FixUpGender();
		userinfo_modified = false;
		MSG_WriteByte (&cls.netchan.message, clc_userinfo);
		MSG_WriteString (&cls.netchan.message, Cvar_Userinfo() );
	}

	SZ_Init (&buf, data, sizeof(data));

	if (cmd->buttons && cl.cinematictime > 0 && !cl.attractloop 
		&& cls.realtime - cl.cinematictime > 1000)
	{	// skip the rest of the cinematic
		SCR_FinishCinematic ();
	}

	MSG_WriteByte (&buf, clc_move);

	// save the position for a checksum byte
	checksumIndex = buf.cursize;
	MSG_WriteByte (&buf, 0);

	// let the server know what the last frame we
	// got was, so the next message can be delta compressed

	//r1: after a vid_restart memory locations of models changes! all existing ents
	//need to be re-sent so the client updates its model to the new memory location.
	if (cl_nodelta->value || !cl.frame.valid || cls.demowaiting)
		MSG_WriteLong (&buf, -1);	// no compression
	else
		MSG_WriteLong (&buf, cl.frame.serverframe);

	// send this and the previous cmds in the message, so
	// if the last packet was dropped, it can be recovered
	i = (cls.netchan.outgoing_sequence-2) & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	memset (&nullcmd, 0, sizeof(nullcmd));
	MSG_WriteDeltaUsercmd (&buf, &nullcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence-1) & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence) & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);

	// calculate a checksum over the move commands
	buf.data[checksumIndex] = COM_BlockSequenceCRCByte(
		buf.data + checksumIndex + 1, buf.cursize - checksumIndex - 1,
		cls.netchan.outgoing_sequence);

	//
	// deliver the message
	//
	Netchan_Transmit (&cls.netchan, buf.cursize, buf.data);	
	CL_InitCmd(); //jec - init the next usercmd buffer.
}


