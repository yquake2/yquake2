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
 * Header for the upper layer of the input system. This is for all
 * non-keyboard devices.
 *
 * =======================================================================
 */

#ifndef CL_HEADER_INPUT_H
#define CL_HEADER_INPUT_H

void IN_Shutdown (void);

/* oportunity for devices to stick commands on the script buffer */
void IN_Commands (void);

void IN_Frame (void);

/* add additional movement on top of the keyboard move cmd */
void IN_Move (usercmd_t *cmd);

#endif
