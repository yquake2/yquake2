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
 * Header file for Windows specific stuff.
 *
 * =======================================================================
 */ 

#ifndef WIN_WINQUAKE_H
#define WIN_WINQUAKE_H

#include <windows.h>

#define WINDOW_STYLE (WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_VISIBLE)

#ifndef _PC_24
 #define _PC_24 0x00020000
#endif

#ifndef _MCW_PC
 #define _MCW_PC 0x00030000
#endif

/* This is a hack to work around a missing MinGW prototype */
#ifndef _controlfp
unsigned int _controlfp(unsigned int new, unsigned int mask);
#endif

extern HINSTANCE global_hInstance;
extern HWND cl_hwnd;

extern qboolean ActiveApp, Minimized;

extern int window_center_x, window_center_y;
extern RECT window_rect;

typedef void (*Key_Event_fp_t)(int key, qboolean down);
extern void (*IN_Update_fp)(void);

typedef struct in_state
{
	/* Pointers to functions back in client, set by vid_so */
	void ( *IN_CenterView_fp )( void );
	Key_Event_fp_t Key_Event_fp;
	vec_t *viewangles;
	int *in_strafe_state;
	int *in_speed_state;
} in_state_t;

#endif

