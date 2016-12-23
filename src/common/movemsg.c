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
 * Movement message (forward, backward, left, right, etc) handling.
 *
 * =======================================================================
 */

#include "header/common.h"

vec3_t bytedirs[NUMVERTEXNORMALS] = {
	{-0.525731, 0.000000, 0.850651},
	{-0.442863, 0.238856, 0.864188},
	{-0.295242, 0.000000, 0.955423},
	{-0.309017, 0.500000, 0.809017},
	{-0.162460, 0.262866, 0.951056},
	{0.000000, 0.000000, 1.000000},
	{0.000000, 0.850651, 0.525731},
	{-0.147621, 0.716567, 0.681718},
	{0.147621, 0.716567, 0.681718},
	{0.000000, 0.525731, 0.850651},
	{0.309017, 0.500000, 0.809017},
	{0.525731, 0.000000, 0.850651},
	{0.295242, 0.000000, 0.955423},
	{0.442863, 0.238856, 0.864188},
	{0.162460, 0.262866, 0.951056},
	{-0.681718, 0.147621, 0.716567},
	{-0.809017, 0.309017, 0.500000},
	{-0.587785, 0.425325, 0.688191},
	{-0.850651, 0.525731, 0.000000},
	{-0.864188, 0.442863, 0.238856},
	{-0.716567, 0.681718, 0.147621},
	{-0.688191, 0.587785, 0.425325},
	{-0.500000, 0.809017, 0.309017},
	{-0.238856, 0.864188, 0.442863},
	{-0.425325, 0.688191, 0.587785},
	{-0.716567, 0.681718, -0.147621},
	{-0.500000, 0.809017, -0.309017},
	{-0.525731, 0.850651, 0.000000},
	{0.000000, 0.850651, -0.525731},
	{-0.238856, 0.864188, -0.442863},
	{0.000000, 0.955423, -0.295242},
	{-0.262866, 0.951056, -0.162460},
	{0.000000, 1.000000, 0.000000},
	{0.000000, 0.955423, 0.295242},
	{-0.262866, 0.951056, 0.162460},
	{0.238856, 0.864188, 0.442863},
	{0.262866, 0.951056, 0.162460},
	{0.500000, 0.809017, 0.309017},
	{0.238856, 0.864188, -0.442863},
	{0.262866, 0.951056, -0.162460},
	{0.500000, 0.809017, -0.309017},
	{0.850651, 0.525731, 0.000000},
	{0.716567, 0.681718, 0.147621},
	{0.716567, 0.681718, -0.147621},
	{0.525731, 0.850651, 0.000000},
	{0.425325, 0.688191, 0.587785},
	{0.864188, 0.442863, 0.238856},
	{0.688191, 0.587785, 0.425325},
	{0.809017, 0.309017, 0.500000},
	{0.681718, 0.147621, 0.716567},
	{0.587785, 0.425325, 0.688191},
	{0.955423, 0.295242, 0.000000},
	{1.000000, 0.000000, 0.000000},
	{0.951056, 0.162460, 0.262866},
	{0.850651, -0.525731, 0.000000},
	{0.955423, -0.295242, 0.000000},
	{0.864188, -0.442863, 0.238856},
	{0.951056, -0.162460, 0.262866},
	{0.809017, -0.309017, 0.500000},
	{0.681718, -0.147621, 0.716567},
	{0.850651, 0.000000, 0.525731},
	{0.864188, 0.442863, -0.238856},
	{0.809017, 0.309017, -0.500000},
	{0.951056, 0.162460, -0.262866},
	{0.525731, 0.000000, -0.850651},
	{0.681718, 0.147621, -0.716567},
	{0.681718, -0.147621, -0.716567},
	{0.850651, 0.000000, -0.525731},
	{0.809017, -0.309017, -0.500000},
	{0.864188, -0.442863, -0.238856},
	{0.951056, -0.162460, -0.262866},
	{0.147621, 0.716567, -0.681718},
	{0.309017, 0.500000, -0.809017},
	{0.425325, 0.688191, -0.587785},
	{0.442863, 0.238856, -0.864188},
	{0.587785, 0.425325, -0.688191},
	{0.688191, 0.587785, -0.425325},
	{-0.147621, 0.716567, -0.681718},
	{-0.309017, 0.500000, -0.809017},
	{0.000000, 0.525731, -0.850651},
	{-0.525731, 0.000000, -0.850651},
	{-0.442863, 0.238856, -0.864188},
	{-0.295242, 0.000000, -0.955423},
	{-0.162460, 0.262866, -0.951056},
	{0.000000, 0.000000, -1.000000},
	{0.295242, 0.000000, -0.955423},
	{0.162460, 0.262866, -0.951056},
	{-0.442863, -0.238856, -0.864188},
	{-0.309017, -0.500000, -0.809017},
	{-0.162460, -0.262866, -0.951056},
	{0.000000, -0.850651, -0.525731},
	{-0.147621, -0.716567, -0.681718},
	{0.147621, -0.716567, -0.681718},
	{0.000000, -0.525731, -0.850651},
	{0.309017, -0.500000, -0.809017},
	{0.442863, -0.238856, -0.864188},
	{0.162460, -0.262866, -0.951056},
	{0.238856, -0.864188, -0.442863},
	{0.500000, -0.809017, -0.309017},
	{0.425325, -0.688191, -0.587785},
	{0.716567, -0.681718, -0.147621},
	{0.688191, -0.587785, -0.425325},
	{0.587785, -0.425325, -0.688191},
	{0.000000, -0.955423, -0.295242},
	{0.000000, -1.000000, 0.000000},
	{0.262866, -0.951056, -0.162460},
	{0.000000, -0.850651, 0.525731},
	{0.000000, -0.955423, 0.295242},
	{0.238856, -0.864188, 0.442863},
	{0.262866, -0.951056, 0.162460},
	{0.500000, -0.809017, 0.309017},
	{0.716567, -0.681718, 0.147621},
	{0.525731, -0.850651, 0.000000},
	{-0.238856, -0.864188, -0.442863},
	{-0.500000, -0.809017, -0.309017},
	{-0.262866, -0.951056, -0.162460},
	{-0.850651, -0.525731, 0.000000},
	{-0.716567, -0.681718, -0.147621},
	{-0.716567, -0.681718, 0.147621},
	{-0.525731, -0.850651, 0.000000},
	{-0.500000, -0.809017, 0.309017},
	{-0.238856, -0.864188, 0.442863},
	{-0.262866, -0.951056, 0.162460},
	{-0.864188, -0.442863, 0.238856},
	{-0.809017, -0.309017, 0.500000},
	{-0.688191, -0.587785, 0.425325},
	{-0.681718, -0.147621, 0.716567},
	{-0.442863, -0.238856, 0.864188},
	{-0.587785, -0.425325, 0.688191},
	{-0.309017, -0.500000, 0.809017},
	{-0.147621, -0.716567, 0.681718},
	{-0.425325, -0.688191, 0.587785},
	{-0.162460, -0.262866, 0.951056},
	{0.442863, -0.238856, 0.864188},
	{0.162460, -0.262866, 0.951056},
	{0.309017, -0.500000, 0.809017},
	{0.147621, -0.716567, 0.681718},
	{0.000000, -0.525731, 0.850651},
	{0.425325, -0.688191, 0.587785},
	{0.587785, -0.425325, 0.688191},
	{0.688191, -0.587785, 0.425325},
	{-0.955423, 0.295242, 0.000000},
	{-0.951056, 0.162460, 0.262866},
	{-1.000000, 0.000000, 0.000000},
	{-0.850651, 0.000000, 0.525731},
	{-0.955423, -0.295242, 0.000000},
	{-0.951056, -0.162460, 0.262866},
	{-0.864188, 0.442863, -0.238856},
	{-0.951056, 0.162460, -0.262866},
	{-0.809017, 0.309017, -0.500000},
	{-0.864188, -0.442863, -0.238856},
	{-0.951056, -0.162460, -0.262866},
	{-0.809017, -0.309017, -0.500000},
	{-0.681718, 0.147621, -0.716567},
	{-0.681718, -0.147621, -0.716567},
	{-0.850651, 0.000000, -0.525731},
	{-0.688191, 0.587785, -0.425325},
	{-0.587785, 0.425325, -0.688191},
	{-0.425325, 0.688191, -0.587785},
	{-0.425325, -0.688191, -0.587785},
	{-0.587785, -0.425325, -0.688191},
	{-0.688191, -0.587785, -0.425325}
};

void
MSG_WriteChar(sizebuf_t *sb, int c)
{
	byte *buf;

	buf = SZ_GetSpace(sb, 1);
	buf[0] = c;
}

void
MSG_WriteByte(sizebuf_t *sb, int c)
{
	byte *buf;

	buf = SZ_GetSpace(sb, 1);
	buf[0] = c;
}

void
MSG_WriteShort(sizebuf_t *sb, int c)
{
	byte *buf;

	buf = SZ_GetSpace(sb, 2);
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

void
MSG_WriteLong(sizebuf_t *sb, int c)
{
	byte *buf;

	buf = SZ_GetSpace(sb, 4);
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

void
MSG_WriteFloat(sizebuf_t *sb, float f)
{
	union
	{
		float f;
		int l;
	} dat;

	dat.f = f;
	dat.l = LittleLong(dat.l);

	SZ_Write(sb, &dat.l, 4);
}

void
MSG_WriteString(sizebuf_t *sb, char *s)
{
	if (!s)
	{
		SZ_Write(sb, "", 1);
	}

	else
	{
		SZ_Write(sb, s, (int)strlen(s) + 1);
	}
}

void
MSG_WriteCoord(sizebuf_t *sb, float f)
{
	MSG_WriteShort(sb, (int)(f * 8));
}

void
MSG_WritePos(sizebuf_t *sb, vec3_t pos)
{
	MSG_WriteShort(sb, (int)(pos[0] * 8));
	MSG_WriteShort(sb, (int)(pos[1] * 8));
	MSG_WriteShort(sb, (int)(pos[2] * 8));
}

void
MSG_WriteAngle(sizebuf_t *sb, float f)
{
	MSG_WriteByte(sb, (int)(f * 256 / 360) & 255);
}

void
MSG_WriteAngle16(sizebuf_t *sb, float f)
{
	MSG_WriteShort(sb, ANGLE2SHORT(f));
}

void
MSG_WriteDeltaUsercmd(sizebuf_t *buf, usercmd_t *from, usercmd_t *cmd)
{
	int bits;

	/* Movement messages */
	bits = 0;

	if (cmd->angles[0] != from->angles[0])
	{
		bits |= CM_ANGLE1;
	}

	if (cmd->angles[1] != from->angles[1])
	{
		bits |= CM_ANGLE2;
	}

	if (cmd->angles[2] != from->angles[2])
	{
		bits |= CM_ANGLE3;
	}

	if (cmd->forwardmove != from->forwardmove)
	{
		bits |= CM_FORWARD;
	}

	if (cmd->sidemove != from->sidemove)
	{
		bits |= CM_SIDE;
	}

	if (cmd->upmove != from->upmove)
	{
		bits |= CM_UP;
	}

	if (cmd->buttons != from->buttons)
	{
		bits |= CM_BUTTONS;
	}

	if (cmd->impulse != from->impulse)
	{
		bits |= CM_IMPULSE;
	}

	MSG_WriteByte(buf, bits);

	if (bits & CM_ANGLE1)
	{
		MSG_WriteShort(buf, cmd->angles[0]);
	}

	if (bits & CM_ANGLE2)
	{
		MSG_WriteShort(buf, cmd->angles[1]);
	}

	if (bits & CM_ANGLE3)
	{
		MSG_WriteShort(buf, cmd->angles[2]);
	}

	if (bits & CM_FORWARD)
	{
		MSG_WriteShort(buf, cmd->forwardmove);
	}

	if (bits & CM_SIDE)
	{
		MSG_WriteShort(buf, cmd->sidemove);
	}

	if (bits & CM_UP)
	{
		MSG_WriteShort(buf, cmd->upmove);
	}

	if (bits & CM_BUTTONS)
	{
		MSG_WriteByte(buf, cmd->buttons);
	}

	if (bits & CM_IMPULSE)
	{
		MSG_WriteByte(buf, cmd->impulse);
	}

	MSG_WriteByte(buf, cmd->msec);
	MSG_WriteByte(buf, cmd->lightlevel);
}

void
MSG_WriteDir(sizebuf_t *sb, vec3_t dir)
{
	int i, best;
	float d, bestd;

	if (!dir)
	{
		MSG_WriteByte(sb, 0);
		return;
	}

	bestd = 0;
	best = 0;

	for (i = 0; i < NUMVERTEXNORMALS; i++)
	{
		d = DotProduct(dir, bytedirs[i]);

		if (d > bestd)
		{
			bestd = d;
			best = i;
		}
	}

	MSG_WriteByte(sb, best);
}

void
MSG_ReadDir(sizebuf_t *sb, vec3_t dir)
{
	int b;

	b = MSG_ReadByte(sb);

	if (b >= NUMVERTEXNORMALS)
	{
		Com_Error(ERR_DROP, "MSF_ReadDir: out of range");
	}

	VectorCopy(bytedirs[b], dir);
}

/*
 * Writes part of a packetentities message.
 * Can delta from either a baseline or a previous packet_entity
 */
void
MSG_WriteDeltaEntity(entity_state_t *from,
		entity_state_t *to,
		sizebuf_t *msg,
		qboolean force,
		qboolean newentity)
{
	int bits;

	if (!to->number)
	{
		Com_Error(ERR_FATAL, "Unset entity number");
	}

	if (to->number >= MAX_EDICTS)
	{
		Com_Error(ERR_FATAL, "Entity number >= MAX_EDICTS");
	}

	/* send an update */
	bits = 0;

	if (to->number >= 256)
	{
		bits |= U_NUMBER16; /* number8 is implicit otherwise */
	}

	if (to->origin[0] != from->origin[0])
	{
		bits |= U_ORIGIN1;
	}

	if (to->origin[1] != from->origin[1])
	{
		bits |= U_ORIGIN2;
	}

	if (to->origin[2] != from->origin[2])
	{
		bits |= U_ORIGIN3;
	}

	if (to->angles[0] != from->angles[0])
	{
		bits |= U_ANGLE1;
	}

	if (to->angles[1] != from->angles[1])
	{
		bits |= U_ANGLE2;
	}

	if (to->angles[2] != from->angles[2])
	{
		bits |= U_ANGLE3;
	}

	if (to->skinnum != from->skinnum)
	{
		if ((unsigned)to->skinnum < 256)
		{
			bits |= U_SKIN8;
		}

		else if ((unsigned)to->skinnum < 0x10000)
		{
			bits |= U_SKIN16;
		}

		else
		{
			bits |= (U_SKIN8 | U_SKIN16);
		}
	}

	if (to->frame != from->frame)
	{
		if (to->frame < 256)
		{
			bits |= U_FRAME8;
		}

		else
		{
			bits |= U_FRAME16;
		}
	}

	if (to->effects != from->effects)
	{
		if (to->effects < 256)
		{
			bits |= U_EFFECTS8;
		}

		else if (to->effects < 0x8000)
		{
			bits |= U_EFFECTS16;
		}

		else
		{
			bits |= U_EFFECTS8 | U_EFFECTS16;
		}
	}

	if (to->renderfx != from->renderfx)
	{
		if (to->renderfx < 256)
		{
			bits |= U_RENDERFX8;
		}

		else if (to->renderfx < 0x8000)
		{
			bits |= U_RENDERFX16;
		}

		else
		{
			bits |= U_RENDERFX8 | U_RENDERFX16;
		}
	}

	if (to->solid != from->solid)
	{
		bits |= U_SOLID;
	}

	/* event is not delta compressed, just 0 compressed */
	if (to->event)
	{
		bits |= U_EVENT;
	}

	if (to->modelindex != from->modelindex)
	{
		bits |= U_MODEL;
	}

	if (to->modelindex2 != from->modelindex2)
	{
		bits |= U_MODEL2;
	}

	if (to->modelindex3 != from->modelindex3)
	{
		bits |= U_MODEL3;
	}

	if (to->modelindex4 != from->modelindex4)
	{
		bits |= U_MODEL4;
	}

	if (to->sound != from->sound)
	{
		bits |= U_SOUND;
	}

	if (newentity || (to->renderfx & RF_BEAM))
	{
		bits |= U_OLDORIGIN;
	}

	/* write the message */
	if (!bits && !force)
	{
		return; /* nothing to send! */
	}

	if (bits & 0xff000000)
	{
		bits |= U_MOREBITS3 | U_MOREBITS2 | U_MOREBITS1;
	}

	else if (bits & 0x00ff0000)
	{
		bits |= U_MOREBITS2 | U_MOREBITS1;
	}

	else if (bits & 0x0000ff00)
	{
		bits |= U_MOREBITS1;
	}

	MSG_WriteByte(msg, bits & 255);

	if (bits & 0xff000000)
	{
		MSG_WriteByte(msg, (bits >> 8) & 255);
		MSG_WriteByte(msg, (bits >> 16) & 255);
		MSG_WriteByte(msg, (bits >> 24) & 255);
	}

	else if (bits & 0x00ff0000)
	{
		MSG_WriteByte(msg, (bits >> 8) & 255);
		MSG_WriteByte(msg, (bits >> 16) & 255);
	}

	else if (bits & 0x0000ff00)
	{
		MSG_WriteByte(msg, (bits >> 8) & 255);
	}

	if (bits & U_NUMBER16)
	{
		MSG_WriteShort(msg, to->number);
	}

	else
	{
		MSG_WriteByte(msg, to->number);
	}

	if (bits & U_MODEL)
	{
		MSG_WriteByte(msg, to->modelindex);
	}

	if (bits & U_MODEL2)
	{
		MSG_WriteByte(msg, to->modelindex2);
	}

	if (bits & U_MODEL3)
	{
		MSG_WriteByte(msg, to->modelindex3);
	}

	if (bits & U_MODEL4)
	{
		MSG_WriteByte(msg, to->modelindex4);
	}

	if (bits & U_FRAME8)
	{
		MSG_WriteByte(msg, to->frame);
	}

	if (bits & U_FRAME16)
	{
		MSG_WriteShort(msg, to->frame);
	}

	if ((bits & U_SKIN8) && (bits & U_SKIN16)) /*used for laser colors */
	{
		MSG_WriteLong(msg, to->skinnum);
	}

	else if (bits & U_SKIN8)
	{
		MSG_WriteByte(msg, to->skinnum);
	}

	else if (bits & U_SKIN16)
	{
		MSG_WriteShort(msg, to->skinnum);
	}

	if ((bits & (U_EFFECTS8 | U_EFFECTS16)) == (U_EFFECTS8 | U_EFFECTS16))
	{
		MSG_WriteLong(msg, to->effects);
	}

	else if (bits & U_EFFECTS8)
	{
		MSG_WriteByte(msg, to->effects);
	}

	else if (bits & U_EFFECTS16)
	{
		MSG_WriteShort(msg, to->effects);
	}

	if ((bits & (U_RENDERFX8 | U_RENDERFX16)) == (U_RENDERFX8 | U_RENDERFX16))
	{
		MSG_WriteLong(msg, to->renderfx);
	}

	else if (bits & U_RENDERFX8)
	{
		MSG_WriteByte(msg, to->renderfx);
	}

	else if (bits & U_RENDERFX16)
	{
		MSG_WriteShort(msg, to->renderfx);
	}

	if (bits & U_ORIGIN1)
	{
		MSG_WriteCoord(msg, to->origin[0]);
	}

	if (bits & U_ORIGIN2)
	{
		MSG_WriteCoord(msg, to->origin[1]);
	}

	if (bits & U_ORIGIN3)
	{
		MSG_WriteCoord(msg, to->origin[2]);
	}

	if (bits & U_ANGLE1)
	{
		MSG_WriteAngle(msg, to->angles[0]);
	}

	if (bits & U_ANGLE2)
	{
		MSG_WriteAngle(msg, to->angles[1]);
	}

	if (bits & U_ANGLE3)
	{
		MSG_WriteAngle(msg, to->angles[2]);
	}

	if (bits & U_OLDORIGIN)
	{
		MSG_WriteCoord(msg, to->old_origin[0]);
		MSG_WriteCoord(msg, to->old_origin[1]);
		MSG_WriteCoord(msg, to->old_origin[2]);
	}

	if (bits & U_SOUND)
	{
		MSG_WriteByte(msg, to->sound);
	}

	if (bits & U_EVENT)
	{
		MSG_WriteByte(msg, to->event);
	}

	if (bits & U_SOLID)
	{
		MSG_WriteShort(msg, to->solid);
	}
}

void
MSG_BeginReading(sizebuf_t *msg)
{
	msg->readcount = 0;
}

int
MSG_ReadChar(sizebuf_t *msg_read)
{
	int c;

	if (msg_read->readcount + 1 > msg_read->cursize)
	{
		c = -1;
	}

	else
	{
		c = (signed char)msg_read->data[msg_read->readcount];
	}

	msg_read->readcount++;

	return c;
}

int
MSG_ReadByte(sizebuf_t *msg_read)
{
	int c;

	if (msg_read->readcount + 1 > msg_read->cursize)
	{
		c = -1;
	}

	else
	{
		c = (unsigned char)msg_read->data[msg_read->readcount];
	}

	msg_read->readcount++;

	return c;
}

int
MSG_ReadShort(sizebuf_t *msg_read)
{
	int c;

	if (msg_read->readcount + 2 > msg_read->cursize)
	{
		c = -1;
	}

	else
	{
		c = (short)(msg_read->data[msg_read->readcount]
					+ (msg_read->data[msg_read->readcount + 1] << 8));
	}

	msg_read->readcount += 2;

	return c;
}

int
MSG_ReadLong(sizebuf_t *msg_read)
{
	int c;

	if (msg_read->readcount + 4 > msg_read->cursize)
	{
		c = -1;
	}

	else
	{
		c = msg_read->data[msg_read->readcount]
			+ (msg_read->data[msg_read->readcount + 1] << 8)
			+ (msg_read->data[msg_read->readcount + 2] << 16)
			+ (msg_read->data[msg_read->readcount + 3] << 24);
	}

	msg_read->readcount += 4;

	return c;
}

float
MSG_ReadFloat(sizebuf_t *msg_read)
{
	union
	{
		byte b[4];
		float f;
		int l;
	} dat;

	if (msg_read->readcount + 4 > msg_read->cursize)
	{
		dat.f = -1;
	}

	else
	{
		dat.b[0] = msg_read->data[msg_read->readcount];
		dat.b[1] = msg_read->data[msg_read->readcount + 1];
		dat.b[2] = msg_read->data[msg_read->readcount + 2];
		dat.b[3] = msg_read->data[msg_read->readcount + 3];
	}

	msg_read->readcount += 4;

	dat.l = LittleLong(dat.l);

	return dat.f;
}

char *
MSG_ReadString(sizebuf_t *msg_read)
{
	static char string[2048];
	int l, c;

	l = 0;

	do
	{
		c = MSG_ReadByte(msg_read);

		if ((c == -1) || (c == 0))
		{
			break;
		}

		string[l] = c;
		l++;
	}
	while (l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

char *
MSG_ReadStringLine(sizebuf_t *msg_read)
{
	static char string[2048];
	int l, c;

	l = 0;

	do
	{
		c = MSG_ReadByte(msg_read);

		if ((c == -1) || (c == 0) || (c == '\n'))
		{
			break;
		}

		string[l] = c;
		l++;
	}
	while (l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

float
MSG_ReadCoord(sizebuf_t *msg_read)
{
	return MSG_ReadShort(msg_read) * (0.125f);
}

void
MSG_ReadPos(sizebuf_t *msg_read, vec3_t pos)
{
	pos[0] = MSG_ReadShort(msg_read) * (0.125f);
	pos[1] = MSG_ReadShort(msg_read) * (0.125f);
	pos[2] = MSG_ReadShort(msg_read) * (0.125f);
}

float
MSG_ReadAngle(sizebuf_t *msg_read)
{
	return MSG_ReadChar(msg_read) * 1.40625f;
}

float
MSG_ReadAngle16(sizebuf_t *msg_read)
{
	return SHORT2ANGLE(MSG_ReadShort(msg_read));
}

void
MSG_ReadDeltaUsercmd(sizebuf_t *msg_read, usercmd_t *from, usercmd_t *move)
{
	int bits;

	memcpy(move, from, sizeof(*move));

	bits = MSG_ReadByte(msg_read);

	/* read current angles */
	if (bits & CM_ANGLE1)
	{
		move->angles[0] = MSG_ReadShort(msg_read);
	}

	if (bits & CM_ANGLE2)
	{
		move->angles[1] = MSG_ReadShort(msg_read);
	}

	if (bits & CM_ANGLE3)
	{
		move->angles[2] = MSG_ReadShort(msg_read);
	}

	/* read movement */
	if (bits & CM_FORWARD)
	{
		move->forwardmove = MSG_ReadShort(msg_read);
	}

	if (bits & CM_SIDE)
	{
		move->sidemove = MSG_ReadShort(msg_read);
	}

	if (bits & CM_UP)
	{
		move->upmove = MSG_ReadShort(msg_read);
	}

	/* read buttons */
	if (bits & CM_BUTTONS)
	{
		move->buttons = MSG_ReadByte(msg_read);
	}

	if (bits & CM_IMPULSE)
	{
		move->impulse = MSG_ReadByte(msg_read);
	}

	/* read time to run command */
	move->msec = MSG_ReadByte(msg_read);

	/* read the light level */
	move->lightlevel = MSG_ReadByte(msg_read);
}

void
MSG_ReadData(sizebuf_t *msg_read, void *data, int len)
{
	int i;

	for (i = 0; i < len; i++)
	{
		((byte *)data)[i] = MSG_ReadByte(msg_read);
	}
}

