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
 * API between client and renderer.
 *
 * =======================================================================
 */

#ifndef CL_VID_H
#define CL_VID_H

#include "../../../common/header/common.h"

// FIXME: Remove it, it's unused.
typedef struct vrect_s {
	int				x,y,width,height;
} vrect_t;

// Hold the video state.
typedef struct {
	int height;
	int	width;
} viddef_t;

// Global video state.
extern viddef_t viddef;

// Generic stuff.
qboolean VID_HasRenderer(const char *renderer);
void	VID_Init(void);
void	VID_Shutdown(void);
void	VID_CheckChanges(void);

void	VID_MenuInit(void);
void	VID_MenuDraw(void);
const char *VID_MenuKey(int);

// Stuff provided by platform backend.
extern int glimp_refreshRate;

const char **GLimp_GetDisplayIndices(void);
int GLimp_GetWindowDisplayIndex(void);
int GLimp_GetNumVideoDisplays(void);
qboolean GLimp_Init(void);
void GLimp_Shutdown(void);
qboolean GLimp_InitGraphics(int fullscreen, int *pwidth, int *pheight);
void GLimp_ShutdownGraphics(void);
void GLimp_GrabInput(qboolean grab);
int GLimp_GetRefreshRate(void);
qboolean GLimp_GetDesktopMode(int *pwidth, int *pheight);

#endif
