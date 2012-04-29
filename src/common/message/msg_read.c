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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * Message reading and preprocessing
 *
 * =======================================================================
 */

#include "../header/common.h"

void MSG_BeginReading (sizebuf_t *msg)
{
	msg->readcount = 0;
}

int MSG_ReadChar (sizebuf_t *msg_read)
{
	int	c;

	if (msg_read->readcount+1 > msg_read->cursize)
		c = -1;

	else
		c = (signed char)msg_read->data[msg_read->readcount];

	msg_read->readcount++;

	return c;
}

int MSG_ReadByte (sizebuf_t *msg_read)
{
	int	c;

	if (msg_read->readcount+1 > msg_read->cursize)
		c = -1;

	else
		c = (unsigned char)msg_read->data[msg_read->readcount];

	msg_read->readcount++;

	return c;
}

int MSG_ReadShort (sizebuf_t *msg_read)
{
	int	c;

	if (msg_read->readcount+2 > msg_read->cursize)
		c = -1;

	else
		c = (short)(msg_read->data[msg_read->readcount]
		            + (msg_read->data[msg_read->readcount+1]<<8));

	msg_read->readcount += 2;

	return c;
}

int MSG_ReadLong (sizebuf_t *msg_read)
{
	int	c;

	if (msg_read->readcount+4 > msg_read->cursize)
		c = -1;

	else
		c = msg_read->data[msg_read->readcount]
		    + (msg_read->data[msg_read->readcount+1]<<8)
		    + (msg_read->data[msg_read->readcount+2]<<16)
		    + (msg_read->data[msg_read->readcount+3]<<24);

	msg_read->readcount += 4;

	return c;
}

float MSG_ReadFloat (sizebuf_t *msg_read)
{
	union
	{
		byte	b[4];
		float	f;
		int	l;
	} dat;

	if (msg_read->readcount+4 > msg_read->cursize)
		dat.f = -1;

	else
	{
		dat.b[0] =	msg_read->data[msg_read->readcount];
		dat.b[1] =	msg_read->data[msg_read->readcount+1];
		dat.b[2] =	msg_read->data[msg_read->readcount+2];
		dat.b[3] =	msg_read->data[msg_read->readcount+3];
	}

	msg_read->readcount += 4;

	dat.l = LittleLong (dat.l);

	return dat.f;
}

char *MSG_ReadString (sizebuf_t *msg_read)
{
	static char	string[2048];
	int		l,c;

	l = 0;

	do
	{
		c = MSG_ReadChar (msg_read);

		if (c == -1 || c == 0)
			break;

		string[l] = c;
		l++;
	}
	while (l < sizeof(string)-1);

	string[l] = 0;

	return string;
}

char *MSG_ReadStringLine (sizebuf_t *msg_read)
{
	static char	string[2048];
	int		l,c;

	l = 0;

	do
	{
		c = MSG_ReadChar (msg_read);

		if (c == -1 || c == 0 || c == '\n')
			break;

		string[l] = c;
		l++;
	}
	while (l < sizeof(string)-1);

	string[l] = 0;

	return string;
}

float MSG_ReadCoord (sizebuf_t *msg_read)
{
	return MSG_ReadShort(msg_read) * (0.125f);
}

void MSG_ReadPos (sizebuf_t *msg_read, vec3_t pos)
{
	pos[0] = MSG_ReadShort(msg_read) * (0.125f);
	pos[1] = MSG_ReadShort(msg_read) * (0.125f);
	pos[2] = MSG_ReadShort(msg_read) * (0.125f);
}

float MSG_ReadAngle (sizebuf_t *msg_read)
{
	return MSG_ReadChar(msg_read) * 1.40625f;
}

float MSG_ReadAngle16 (sizebuf_t *msg_read)
{
	return SHORT2ANGLE(MSG_ReadShort(msg_read));
}

void MSG_ReadDeltaUsercmd (sizebuf_t *msg_read, usercmd_t *from, usercmd_t *move)
{
	int bits;

	memcpy (move, from, sizeof(*move));

	bits = MSG_ReadByte (msg_read);

	/* read current angles */
	if (bits & CM_ANGLE1)
		move->angles[0] = MSG_ReadShort (msg_read);

	if (bits & CM_ANGLE2)
		move->angles[1] = MSG_ReadShort (msg_read);

	if (bits & CM_ANGLE3)
		move->angles[2] = MSG_ReadShort (msg_read);

	/* read movement */
	if (bits & CM_FORWARD)
		move->forwardmove = MSG_ReadShort (msg_read);

	if (bits & CM_SIDE)
		move->sidemove = MSG_ReadShort (msg_read);

	if (bits & CM_UP)
		move->upmove = MSG_ReadShort (msg_read);

	/* read buttons */
	if (bits & CM_BUTTONS)
		move->buttons = MSG_ReadByte (msg_read);

	if (bits & CM_IMPULSE)
		move->impulse = MSG_ReadByte (msg_read);

	/* read time to run command */
	move->msec = MSG_ReadByte (msg_read);

	/* read the light level */
	move->lightlevel = MSG_ReadByte (msg_read);
}

void MSG_ReadData (sizebuf_t *msg_read, void *data, int len)
{
	int		i;

	for (i=0 ; i<len ; i++)
		((byte *)data)[i] = MSG_ReadByte (msg_read);
}
