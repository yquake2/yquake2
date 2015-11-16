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

extern void M_ForceMenuOff(void);

static cvar_t *gl_mode;
static cvar_t *gl_hudscale;
static cvar_t *gl_consolescale;
static cvar_t *gl_menuscale;
static cvar_t *crosshair_scale;
static cvar_t *fov;
extern cvar_t *scr_viewsize;
extern cvar_t *vid_gamma;
extern cvar_t *vid_fullscreen;
static cvar_t *gl_swapinterval;
static cvar_t *gl_anisotropic;
static cvar_t *gl_msaa_samples;

static menuframework_s s_opengl_menu;

static menulist_s s_mode_list;
static menulist_s s_aspect_list;
static menulist_s s_uiscale_list;
static menuslider_s s_screensize_slider;
static menuslider_s s_brightness_slider;
static menulist_s s_fs_box;
static menulist_s s_vsync_list;
static menulist_s s_af_list;
static menulist_s s_msaa_list;
static menuaction_s s_defaults_action;
static menuaction_s s_apply_action;

static int
GetCustomValue(menulist_s list)
{
	int i = list.curvalue;

	do
	{
		i++;
	}
	while (list.itemnames[i]);
	i--;

	return i;
}

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
AnisotropicCallback(void *s)
{
	menulist_s *list = (menulist_s *)s;

	if (list->curvalue == 0)
	{
		Cvar_SetValue("gl_anisotropic", 0);
	}
	else
	{
		Cvar_SetValue("gl_anisotropic", pow(2, list->curvalue));
	}
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

	/* custom mode */
	if (s_mode_list.curvalue != GetCustomValue(s_mode_list))
	{
		/* Restarts automatically */
		Cvar_SetValue("gl_mode", s_mode_list.curvalue);
	}
	else
	{
		/* Restarts automatically */
		Cvar_SetValue("gl_mode", -1);
	}

	/* horplus */
	if (s_aspect_list.curvalue == 0)
	{
		if (horplus->value != 1)
		{
			Cvar_SetValue("horplus", 1);
		}
	}
	else
	{
		if (horplus->value != 0)
		{
			Cvar_SetValue("horplus", 0);
		}
	}

	/* fov */
	if (s_aspect_list.curvalue == 0 || s_aspect_list.curvalue == 1)
	{
		if (fov->value != 90)
		{
			/* Restarts automatically */
			Cvar_SetValue("fov", 90);
		}
	}
	else if (s_aspect_list.curvalue == 2)
	{
		if (fov->value != 86)
		{
			/* Restarts automatically */
			Cvar_SetValue("fov", 86);
		}
	}
	else if (s_aspect_list.curvalue == 3)
	{
		if (fov->value != 100)
		{
			/* Restarts automatically */
			Cvar_SetValue("fov", 100);
		}
	}
	else if (s_aspect_list.curvalue == 4)
	{
		if (fov->value != 106)
		{
			/* Restarts automatically */
			Cvar_SetValue("fov", 106);
		}
	}

	/* UI scaling */
	if (s_uiscale_list.curvalue == 0)
	{
		Cvar_SetValue("gl_hudscale", -1);
	}
	else if (s_uiscale_list.curvalue < GetCustomValue(s_uiscale_list))
	{
		Cvar_SetValue("gl_hudscale", s_uiscale_list.curvalue);
	}

	if (s_uiscale_list.curvalue != GetCustomValue(s_uiscale_list))
	{
		Cvar_SetValue("gl_consolescale", gl_hudscale->value);
		Cvar_SetValue("gl_menuscale", gl_hudscale->value);
		Cvar_SetValue("crosshair_scale", gl_hudscale->value);
	}

	/* Restarts automatically */
	Cvar_SetValue("vid_fullscreen", s_fs_box.curvalue);

	/* vertical sync */
	if (gl_swapinterval->value != s_vsync_list.curvalue)
	{
		Cvar_SetValue("gl_swapinterval", s_vsync_list.curvalue);
		restart = true;
	}

	/* multisample anti-aliasing */
	if (s_msaa_list.curvalue == 0)
	{
		if (gl_msaa_samples->value != 0)
		{
			Cvar_SetValue("gl_msaa_samples", 0);
			restart = true;
		}
	}
	else
	{
		if (gl_msaa_samples->value != pow(2, s_msaa_list.curvalue))
		{
			Cvar_SetValue("gl_msaa_samples", pow(2, s_msaa_list.curvalue));
			restart = true;
		}
	}

	if (restart)
	{
		Cbuf_AddText("vid_restart\n");
	}

	M_ForceMenuOff();
}

void
VID_MenuInit(void)
{
	int y = 0;

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
		"[1366 768 ]",
		"[1440 900 ]",
		"[1600 1200]",
		"[1680 1050]",
		"[1920 1080]",
		"[1920 1200]",
		"[2048 1536]",
		"[custom   ]",
		0
	};

	static const char *aspect_names[] = {
		"auto",
		"4:3",
		"5:4",
		"16:10",
		"16:9",
		"custom",
		0
	};

	static const char *uiscale_names[] = {
		"auto",
		"1x",
		"2x",
		"3x",
		"4x",
		"5x",
		"6x",
		"custom",
		0
	};

	static const char *yesno_names[] = {
		"no",
		"yes",
		0
	};

	static const char *pow2_names[] = {
		"off",
		"2x",
		"4x",
		"8x",
		"16x",
		0
	};

	if (!gl_mode)
	{
		gl_mode = Cvar_Get("gl_mode", "4", 0);
	}

	if (!gl_hudscale)
	{
		gl_hudscale = Cvar_Get("gl_hudscale", "-1", CVAR_ARCHIVE);
	}
	if (!gl_consolescale)
	{
		gl_consolescale = Cvar_Get("gl_consolescale", "-1", CVAR_ARCHIVE);
	}
	if (!gl_menuscale)
	{
		gl_menuscale = Cvar_Get("gl_menuscale", "-1", CVAR_ARCHIVE);
	}
	if (!crosshair_scale)
	{
		crosshair_scale = Cvar_Get("crosshair_scale", "-1", CVAR_ARCHIVE);
	}

	if (!horplus)
	{
		horplus = Cvar_Get("horplus", "1", CVAR_ARCHIVE);
	}

	if (!fov)
	{
		fov = Cvar_Get("fov", "90",  CVAR_USERINFO | CVAR_ARCHIVE);
	}

	if (!scr_viewsize)
	{
		scr_viewsize = Cvar_Get("viewsize", "100", CVAR_ARCHIVE);
	}

	if (!vid_gamma)
	{
		vid_gamma = Cvar_Get("vid_gamma", "1.0", CVAR_ARCHIVE);
	}

	if (!gl_swapinterval)
	{
		gl_swapinterval = Cvar_Get("gl_swapinterval", "1", CVAR_ARCHIVE);
	}

	if (!gl_anisotropic)
	{
		gl_anisotropic = Cvar_Get("gl_anisotropic", "0", CVAR_ARCHIVE);
	}

	if (!gl_msaa_samples)
	{
		gl_msaa_samples = Cvar_Get("gl_msaa_samples", "0", CVAR_ARCHIVE);
	}

	s_opengl_menu.x = viddef.width * 0.50;
	s_opengl_menu.nitems = 0;

	s_mode_list.generic.type = MTYPE_SPINCONTROL;
	s_mode_list.generic.name = "video mode";
	s_mode_list.generic.x = 0;
	s_mode_list.generic.y = (y = 0);
	s_mode_list.itemnames = resolutions;
	if (gl_mode->value >= 0)
	{
		s_mode_list.curvalue = gl_mode->value;
	}
	else
	{
		s_mode_list.curvalue = GetCustomValue(s_mode_list);
	}

	s_aspect_list.generic.type = MTYPE_SPINCONTROL;
	s_aspect_list.generic.name = "aspect ratio";
	s_aspect_list.generic.x = 0;
	s_aspect_list.generic.y = (y += 10);
	s_aspect_list.itemnames = aspect_names;
	if (horplus->value == 1)
	{
		s_aspect_list.curvalue = 0;
	}
	else if (fov->value == 90)
	{
		s_aspect_list.curvalue = 1;
	}
	else if (fov->value == 86)
	{
		s_aspect_list.curvalue = 2;
	}
	else if (fov->value == 100)
	{
		s_aspect_list.curvalue = 3;
	}
	else if (fov->value == 106)
	{
		s_aspect_list.curvalue = 4;
	}
	else
	{
		s_aspect_list.curvalue = GetCustomValue(s_aspect_list);
	}

	s_uiscale_list.generic.type = MTYPE_SPINCONTROL;
	s_uiscale_list.generic.name = "ui scale";
	s_uiscale_list.generic.x = 0;
	s_uiscale_list.generic.y = (y += 10);
	s_uiscale_list.itemnames = uiscale_names;
	if (gl_hudscale->value != gl_consolescale->value ||
		gl_hudscale->value != gl_menuscale->value ||
		gl_hudscale->value != crosshair_scale->value)
	{
		s_uiscale_list.curvalue = GetCustomValue(s_uiscale_list);
	}
	else if (gl_hudscale->value < 0)
	{
		s_uiscale_list.curvalue = 0;
	}
	else if (gl_hudscale->value > 0 &&
			gl_hudscale->value < GetCustomValue(s_uiscale_list) &&
			gl_hudscale->value == (int)gl_hudscale->value)
	{
		s_uiscale_list.curvalue = gl_hudscale->value;
	}
	else
	{
		s_uiscale_list.curvalue = GetCustomValue(s_uiscale_list);
	}

	s_screensize_slider.generic.type = MTYPE_SLIDER;
	s_screensize_slider.generic.name = "screen size";
	s_screensize_slider.generic.x = 0;
	s_screensize_slider.generic.y = (y += 10);
	s_screensize_slider.minvalue = 4;
	s_screensize_slider.maxvalue = 10;
	s_screensize_slider.generic.callback = ScreenSizeCallback;
	s_screensize_slider.curvalue = scr_viewsize->value / 10;

	s_brightness_slider.generic.type = MTYPE_SLIDER;
	s_brightness_slider.generic.name = "brightness";
	s_brightness_slider.generic.x = 0;
	s_brightness_slider.generic.y = (y += 10);
	s_brightness_slider.generic.callback = BrightnessCallback;
	s_brightness_slider.minvalue = 1;
	s_brightness_slider.maxvalue = 20;
	s_brightness_slider.curvalue = vid_gamma->value * 10;

	s_fs_box.generic.type = MTYPE_SPINCONTROL;
	s_fs_box.generic.name = "fullscreen";
	s_fs_box.generic.x = 0;
	s_fs_box.generic.y = (y += 10);
	s_fs_box.itemnames = yesno_names;
	s_fs_box.curvalue = (vid_fullscreen->value != 0);

	s_vsync_list.generic.type = MTYPE_SPINCONTROL;
	s_vsync_list.generic.name = "vertical sync";
	s_vsync_list.generic.x = 0;
	s_vsync_list.generic.y = (y += 10);
	s_vsync_list.itemnames = yesno_names;
	s_vsync_list.curvalue = (gl_swapinterval->value != 0);

	s_af_list.generic.type = MTYPE_SPINCONTROL;
	s_af_list.generic.name = "aniso filtering";
	s_af_list.generic.x = 0;
	s_af_list.generic.y = (y += 20);
	s_af_list.generic.callback = AnisotropicCallback;
	s_af_list.itemnames = pow2_names;
	s_af_list.curvalue = 0;
	if (gl_anisotropic->value)
	{
		do
		{
			s_af_list.curvalue++;
		} while (pow2_names[s_af_list.curvalue] &&
				pow(2, s_af_list.curvalue) <= gl_anisotropic->value);
		s_af_list.curvalue--;
	}

	s_msaa_list.generic.type = MTYPE_SPINCONTROL;
	s_msaa_list.generic.name = "multisampling";
	s_msaa_list.generic.x = 0;
	s_msaa_list.generic.y = (y += 10);
	s_msaa_list.itemnames = pow2_names;
	s_msaa_list.curvalue = 0;
	if (gl_msaa_samples->value)
	{
		do
		{
			s_msaa_list.curvalue++;
		} while (pow2_names[s_msaa_list.curvalue] &&
				pow(2, s_msaa_list.curvalue) <= gl_msaa_samples->value);
		s_msaa_list.curvalue--;
	}

	s_defaults_action.generic.type = MTYPE_ACTION;
	s_defaults_action.generic.name = "reset to default";
	s_defaults_action.generic.x = 0;
	s_defaults_action.generic.y = (y += 20);
	s_defaults_action.generic.callback = ResetDefaults;

	s_apply_action.generic.type = MTYPE_ACTION;
	s_apply_action.generic.name = "apply";
	s_apply_action.generic.x = 0;
	s_apply_action.generic.y = (y += 10);
	s_apply_action.generic.callback = ApplyChanges;

	Menu_AddItem(&s_opengl_menu, (void *)&s_mode_list);
	Menu_AddItem(&s_opengl_menu, (void *)&s_aspect_list);
	Menu_AddItem(&s_opengl_menu, (void *)&s_uiscale_list);
	Menu_AddItem(&s_opengl_menu, (void *)&s_screensize_slider);
	Menu_AddItem(&s_opengl_menu, (void *)&s_brightness_slider);
	Menu_AddItem(&s_opengl_menu, (void *)&s_fs_box);
	Menu_AddItem(&s_opengl_menu, (void *)&s_vsync_list);
	Menu_AddItem(&s_opengl_menu, (void *)&s_af_list);
	Menu_AddItem(&s_opengl_menu, (void *)&s_msaa_list);
	Menu_AddItem(&s_opengl_menu, (void *)&s_defaults_action);
	Menu_AddItem(&s_opengl_menu, (void *)&s_apply_action);

	Menu_Center(&s_opengl_menu);
	s_opengl_menu.x -= 8;
}

void
VID_MenuDraw(void)
{
	int w, h;
	float scale = SCR_GetMenuScale();

	/* draw the banner */
	Draw_GetPicSize(&w, &h, "m_banner_video");
	Draw_PicScaled(viddef.width / 2 - (w * scale) / 2, viddef.height / 2 - (110 * scale),
			"m_banner_video", scale);

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

