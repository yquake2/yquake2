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
 * This is the refresher dependend video menu. If you add a new
 * refresher this menu must be altered.
 *
 * =======================================================================
 */

#include "../../client/header/client.h"
#include "../../client/menu/header/qmenu.h"

#define CUSTOM_MODE 20

extern void M_ForceMenuOff(void);

extern cvar_t *vid_ref;
extern cvar_t *vid_fullscreen;
extern cvar_t *vid_gamma;
extern cvar_t *scr_viewsize;

static cvar_t *gl_mode;
static cvar_t *gl_driver;
static cvar_t *gl_picmip;
static cvar_t *gl_ext_palettedtexture;

static cvar_t *fov;
static cvar_t *windowed_mouse;

static menuframework_s s_opengl_menu;

static menulist_s s_mode_list;
static menulist_s s_aspect_list;
static menuslider_s s_tq_slider;
static menuslider_s s_screensize_slider;
static menuslider_s s_brightness_slider;
static menulist_s s_fs_box;
static menulist_s s_paletted_texture_box;
static menuaction_s s_apply_action;
static menuaction_s s_defaults_action;

static void
ScreenSizeCallback(void *s)
{
	menuslider_s *slider = (menuslider_s *)s;

	Cvar_SetValue("viewsize", slider->curvalue * 10);
}

static void
BrightnessCallback(void *s)
{
	menuslider_s *slider = (menuslider_s *)s;

	float gamma = slider->curvalue / 10.0;
	Cvar_SetValue("vid_gamma", gamma);
}

static void
ResetDefaults(void *unused)
{
	VID_MenuInit();
}

static void
ApplyChanges(void *unused)
{
	qboolean restart = false;

	if (gl_picmip->value != (3 - s_tq_slider.curvalue))
	{

		Cvar_SetValue("gl_picmip", 3 - s_tq_slider.curvalue);
		restart = true;
	}

	if (gl_ext_palettedtexture->value != s_paletted_texture_box.curvalue)
	{
		Cvar_SetValue("gl_ext_palettedtexture", s_paletted_texture_box.curvalue);
		restart = true;
	}

	/* Restarts automatically */
	Cvar_SetValue("vid_fullscreen", s_fs_box.curvalue);

	/* custom mode */
	if (s_mode_list.curvalue != CUSTOM_MODE)
	{
		/* Restarts automatically */
		Cvar_SetValue("gl_mode", s_mode_list.curvalue);
	}
	else
	{
		/* Restarts automatically */
		Cvar_SetValue("gl_mode", -1);
	}

	/* fov */
	if (s_aspect_list.curvalue == 0)
	{
		if (fov->value != 90)
		{
			/* Restarts automatically */
			Cvar_SetValue("fov", 90);
		}
	}
	else if (s_aspect_list.curvalue == 1)
	{
		if (fov->value != 100)
		{
			/* Restarts automatically */
			Cvar_SetValue("fov", 100);
		}
	}
	else if (s_aspect_list.curvalue == 2)
	{
		if (fov->value != 105)
		{
			/* Restarts automatically */
			Cvar_SetValue("fov", 105);
		}
	}
 
	if (restart)
	{
		Cbuf_AddText("vid_restart\n");
	}

	//M_ForceMenuOff();
}

void
VID_MenuInit(void)
{
	static const char *resolutions[] = {
		"[320 240  ]",
		"[400 300  ]",
		"[512 384  ]",
		"[640 400  ]",
		"[640 480  ]",
		"[800 500  ]",
		"[800 600  ]",
		"[960 720  ]",
		"[1024 480 ]",
		"[1024 640 ]",
		"[1024 768 ]",
		"[1152 768 ]",
		"[1152 864 ]",
		"[1280 800 ]",
		"[1280 854 ]",
		"[1280 960 ]",
		"[1280 1024]",
		"[1440 900 ]",
		"[1600 1200]",
		"[1680 1050]",
		"[1920 1080]",
		"[1920 1200]",
		"[2048 1536]",
		"[Custom   ]",
		0
	};

	static const char *yesno_names[] = {
		"no",
		"yes",
		0
	};

	static const char *aspect_names[] = {
		"4:3",
		"16:10",
		"16:9",
		"Custom"
	};

	if (!gl_driver)
	{
		gl_driver = Cvar_Get("gl_driver", LIBGL, 0);
	}

	if (!gl_picmip)
	{
		gl_picmip = Cvar_Get("gl_picmip", "0", 0);
	}

	if (!gl_mode)
	{
		gl_mode = Cvar_Get("gl_mode", "4", 0);
	}

	if (!gl_ext_palettedtexture)
	{
		gl_ext_palettedtexture = Cvar_Get("gl_ext_palettedtexture",
				"0", CVAR_ARCHIVE);
	}

	if (!windowed_mouse)
	{
		windowed_mouse = Cvar_Get("windowed_mouse", "1",
				CVAR_USERINFO | CVAR_ARCHIVE);
	}

	if (!fov)
	{
		fov = Cvar_Get("fov", "90",  CVAR_USERINFO | CVAR_ARCHIVE);
	}

	if (fov->value == 90)
	{
		s_aspect_list.curvalue = 0;
	}
	else if (fov->value == 100)
	{
		s_aspect_list.curvalue = 1;
	}
	else if (fov->value == 105)
	{
		s_aspect_list.curvalue = 2;
	}
	else
	{
		s_aspect_list.curvalue = 3;
	}

	/* custom mode */
	if (gl_mode->value >= 1.0)
	{
		s_mode_list.curvalue = gl_mode->value;
	}
	else
	{
		s_mode_list.curvalue = CUSTOM_MODE;
	}

	if (!scr_viewsize)
	{
		scr_viewsize = Cvar_Get("viewsize", "100", CVAR_ARCHIVE);
	}

	s_screensize_slider.curvalue = scr_viewsize->value / 10;

	s_opengl_menu.x = viddef.width * 0.50;
	s_opengl_menu.nitems = 0;

	s_mode_list.generic.type = MTYPE_SPINCONTROL;
	s_mode_list.generic.name = "video mode";
	s_mode_list.generic.x = 0;
	s_mode_list.generic.y = 0;
	s_mode_list.itemnames = resolutions;

 	s_aspect_list.generic.type = MTYPE_SPINCONTROL;
	s_aspect_list.generic.name = "aspect ratio";
	s_aspect_list.generic.x = 0;
	s_aspect_list.generic.y = 10;
	s_aspect_list.itemnames = aspect_names; 
	
	s_screensize_slider.generic.type = MTYPE_SLIDER;
	s_screensize_slider.generic.x = 0;
	s_screensize_slider.generic.y = 20;
	s_screensize_slider.generic.name = "screen size";
	s_screensize_slider.minvalue = 3;
	s_screensize_slider.maxvalue = 12;
	s_screensize_slider.generic.callback = ScreenSizeCallback;

	s_brightness_slider.generic.type = MTYPE_SLIDER;
	s_brightness_slider.generic.x = 0;
	s_brightness_slider.generic.y = 40;
	s_brightness_slider.generic.name = "brightness";
	s_brightness_slider.generic.callback = BrightnessCallback;
	s_brightness_slider.minvalue = 1;
	s_brightness_slider.maxvalue = 20;
	s_brightness_slider.curvalue = vid_gamma->value * 10;

	s_fs_box.generic.type = MTYPE_SPINCONTROL;
	s_fs_box.generic.x = 0;
	s_fs_box.generic.y = 50;
	s_fs_box.generic.name = "fullscreen";
	s_fs_box.itemnames = yesno_names;
	s_fs_box.curvalue = vid_fullscreen->value;
 
	s_tq_slider.generic.type = MTYPE_SLIDER;
	s_tq_slider.generic.x = 0;
	s_tq_slider.generic.y = 70;
	s_tq_slider.generic.name = "texture quality";
	s_tq_slider.minvalue = 0;
	s_tq_slider.maxvalue = 3;
	s_tq_slider.curvalue = 3 - gl_picmip->value;
 
	s_paletted_texture_box.generic.type = MTYPE_SPINCONTROL;
	s_paletted_texture_box.generic.x = 0;
	s_paletted_texture_box.generic.y = 80;
	s_paletted_texture_box.generic.name = "8-bit textures";
	s_paletted_texture_box.itemnames = yesno_names;
	s_paletted_texture_box.curvalue = gl_ext_palettedtexture->value;
  
	s_defaults_action.generic.type = MTYPE_ACTION;
	s_defaults_action.generic.name = "reset to default";
	s_defaults_action.generic.x = 0;
	s_defaults_action.generic.y = 100;
	s_defaults_action.generic.callback = ResetDefaults;

	s_apply_action.generic.type = MTYPE_ACTION;
	s_apply_action.generic.name = "apply";
	s_apply_action.generic.x = 0;
	s_apply_action.generic.y = 110;
	s_apply_action.generic.callback = ApplyChanges;

	Menu_AddItem(&s_opengl_menu, (void *)&s_mode_list);
	Menu_AddItem(&s_opengl_menu, (void *)&s_aspect_list);
	Menu_AddItem(&s_opengl_menu, (void *)&s_screensize_slider);
	Menu_AddItem(&s_opengl_menu, (void *)&s_brightness_slider);
	Menu_AddItem(&s_opengl_menu, (void *)&s_fs_box);
	Menu_AddItem(&s_opengl_menu, (void *)&s_tq_slider);
	Menu_AddItem(&s_opengl_menu, (void *)&s_paletted_texture_box);
	Menu_AddItem(&s_opengl_menu, (void *)&s_defaults_action);
	Menu_AddItem(&s_opengl_menu, (void *)&s_apply_action);

	Menu_Center(&s_opengl_menu);
	s_opengl_menu.x -= 8;
}

void
VID_MenuDraw(void)
{
	int w, h;

	/* draw the banner */
	re.DrawGetPicSize(&w, &h, "m_banner_video");
	re.DrawPic(viddef.width / 2 - w / 2, viddef.height / 2 - 110,
			"m_banner_video");

	/* move cursor to a reasonable starting position */
	Menu_AdjustCursor(&s_opengl_menu, 1);

	/* draw the menu */
	Menu_Draw(&s_opengl_menu);
}

const char *
VID_MenuKey(int key)
{
	extern void M_PopMenu(void);

	menuframework_s *m = &s_opengl_menu;
	static const char *sound = "misc/menu1.wav";

	switch (key)
	{
		case K_ESCAPE:
			M_PopMenu();
			return NULL;
		case K_UPARROW:
			m->cursor--;
			Menu_AdjustCursor(m, -1);
			break;
		case K_DOWNARROW:
			m->cursor++;
			Menu_AdjustCursor(m, 1);
			break;
		case K_LEFTARROW:
			Menu_SlideItem(m, -1);
			break;
		case K_RIGHTARROW:
			Menu_SlideItem(m, 1);
			break;
		case K_ENTER:
			Menu_SelectItem(m);
			break;
	}

	return sound;
}

