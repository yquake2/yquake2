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
#include "header/qmenu.h"

extern void M_ForceMenuOff(void);

static cvar_t *r_mode;
static cvar_t *vid_displayindex;
static cvar_t *r_hudscale;
static cvar_t *r_consolescale;
static cvar_t *r_menuscale;
static cvar_t *crosshair_scale;
static cvar_t *fov;
extern cvar_t *scr_viewsize;
extern cvar_t *vid_gamma;
extern cvar_t *vid_fullscreen;
extern cvar_t *vid_renderer;
static cvar_t *r_vsync;
static cvar_t *gl_anisotropic;
static cvar_t *gl_msaa_samples;

static menuframework_s s_opengl_menu;

static menulist_s s_renderer_list;
static menulist_s s_mode_list;
static menulist_s s_display_list;
static menulist_s s_uiscale_list;
static menuslider_s s_brightness_slider;
static menuslider_s s_fov_slider;
static menulist_s s_fs_box;
static menulist_s s_vsync_list;
static menulist_s s_af_list;
static menulist_s s_msaa_list;
static menuaction_s s_defaults_action;
static menuaction_s s_apply_action;

static int
GetRenderer(void)
{
	/* First element in array is 'OpenGL 1.4' aka gl1.
	   Second element in array is 'OpenGL 3.2' aka gl3.
	   Third element in array is unknown renderer. */
	if (Q_stricmp(vid_renderer->string, "gl1") == 0)
	{
		return 0;
	}
	else if (Q_stricmp(vid_renderer->string, "gl3") == 0)
	{
		return 1;
	}
	else if (Q_stricmp(vid_renderer->string, "soft") == 0)
	{
		return 2;
	}
#ifdef USE_REFVK
	else if (Q_stricmp(vid_renderer->string, "vk") == 0)
	{
		return 3;
	}
	else
	{
		return 4;
	}
#else
	else
	{
		return 3;
	}
#endif
}

static int
GetCustomValue(menulist_s *list)
{
	static menulist_s *last;
	static int i;

	if (list != last)
	{
		last = list;
		i = list->curvalue;
		do
		{
			i++;
		}
		while (list->itemnames[i]);
		i--;
	}

	return i;
}

static void
BrightnessCallback(void *s)
{
	menuslider_s *slider = (menuslider_s *)s;

	float gamma = slider->curvalue / 10.0;
	Cvar_SetValue("vid_gamma", gamma);
}

static void
FOVCallback(void *s) {
	menuslider_s *slider = (menuslider_s *)s;
	Cvar_SetValue("fov", slider->curvalue);
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

#define CUSTOM_MODE_NAME "[Custom    ]"
#define AUTO_MODE_NAME   "[Auto      ]"

static void
ApplyChanges(void *unused)
{
	qboolean restart = false;

	/* Renderer */
	if (s_renderer_list.curvalue != GetRenderer())
	{
		/*  First element in array is 'OpenGL 1.4' aka gl1.
			Second element in array is 'OpenGL 3.2' aka gl3.
			Third element in array is unknown renderer. */
		if (s_renderer_list.curvalue == 0)
		{
			Cvar_Set("vid_renderer", "gl1");
			restart = true;
		}
		else if (s_renderer_list.curvalue == 1)
		{
			Cvar_Set("vid_renderer", "gl3");
			restart = true;
		}
		else if (s_renderer_list.curvalue == 2)
		{
			Cvar_Set("vid_renderer", "soft");
			restart = true;
		}
#ifdef USE_REFVK
		else if (s_renderer_list.curvalue == 3)
		{
			Cvar_Set("vid_renderer", "vk");
			restart = true;
		}
#endif
	}

	/* auto mode */
	if (!strcmp(s_mode_list.itemnames[s_mode_list.curvalue],
		AUTO_MODE_NAME))
	{
		/* Restarts automatically */
		Cvar_SetValue("r_mode", -2);
	}
	else if (!strcmp(s_mode_list.itemnames[s_mode_list.curvalue],
		CUSTOM_MODE_NAME))
	{
		/* Restarts automatically */
		Cvar_SetValue("r_mode", -1);
	}
	else
	{
		/* Restarts automatically */
		Cvar_SetValue("r_mode", s_mode_list.curvalue);
	}

	if (s_display_list.curvalue != GLimp_GetWindowDisplayIndex() )
	{
		Cvar_SetValue( "vid_displayindex", s_display_list.curvalue );
		restart = true;
	}

	/* UI scaling */
	if (s_uiscale_list.curvalue == 0)
	{
		Cvar_SetValue("r_hudscale", -1);
	}
	else if (s_uiscale_list.curvalue < GetCustomValue(&s_uiscale_list))
	{
		Cvar_SetValue("r_hudscale", s_uiscale_list.curvalue);
	}

	if (s_uiscale_list.curvalue != GetCustomValue(&s_uiscale_list))
	{
		Cvar_SetValue("r_consolescale", r_hudscale->value);
		Cvar_SetValue("r_menuscale", r_hudscale->value);
		Cvar_SetValue("crosshair_scale", r_hudscale->value);
	}

	/* Restarts automatically */
	Cvar_SetValue("vid_fullscreen", s_fs_box.curvalue);

	/* vertical sync */
	if (r_vsync->value != s_vsync_list.curvalue)
	{
		Cvar_SetValue("r_vsync", s_vsync_list.curvalue);
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

	static const char *renderers[] = {
			"[OpenGL 1.4]",
			"[OpenGL 3.2]",
			"[Software  ]",
#ifdef USE_REFVK
			"[Vulkan    ]",
#endif
			CUSTOM_MODE_NAME,
			0
	};

	// must be kept in sync with vid_modes[] in vid.c
	static const char *resolutions[] = {
		"[320 240   ]",
		"[400 300   ]",
		"[512 384   ]",
		"[640 400   ]",
		"[640 480   ]",
		"[800 500   ]",
		"[800 600   ]",
		"[960 720   ]",
		"[1024 480  ]",
		"[1024 640  ]",
		"[1024 768  ]",
		"[1152 768  ]",
		"[1152 864  ]",
		"[1280 800  ]",
		"[1280 720  ]",
		"[1280 960  ]",
		"[1280 1024 ]",
		"[1366 768  ]",
		"[1440 900  ]",
		"[1600 1200 ]",
		"[1680 1050 ]",
		"[1920 1080 ]",
		"[1920 1200 ]",
		"[2048 1536 ]",
		"[2560 1080 ]",
		"[2560 1440 ]",
		"[2560 1600 ]",
		"[3440 1440 ]",
		"[3840 1600 ]",
		"[3840 2160 ]",
		"[4096 2160 ]",
		"[5120 2880 ]",
		AUTO_MODE_NAME,
		CUSTOM_MODE_NAME,
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

	static const char *fullscreen_names[] = {
			"no",
			"keep resolution",
			"switch resolution",
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

	if (!r_mode)
	{
		r_mode = Cvar_Get("r_mode", "4", 0);
	}

	if (!vid_displayindex)
	{
		vid_displayindex = Cvar_Get("vid_displayindex", "0", CVAR_ARCHIVE);
	}

	if (!r_hudscale)
	{
		r_hudscale = Cvar_Get("r_hudscale", "-1", CVAR_ARCHIVE);
	}

	if (!r_consolescale)
	{
		r_consolescale = Cvar_Get("r_consolescale", "-1", CVAR_ARCHIVE);
	}

	if (!r_menuscale)
	{
		r_menuscale = Cvar_Get("r_menuscale", "-1", CVAR_ARCHIVE);
	}

	if (!crosshair_scale)
	{
		crosshair_scale = Cvar_Get("crosshair_scale", "-1", CVAR_ARCHIVE);
	}

	if (!fov)
	{
		fov = Cvar_Get("fov", "90",  CVAR_USERINFO | CVAR_ARCHIVE);
	}

	if (!vid_gamma)
	{
		vid_gamma = Cvar_Get("vid_gamma", "1.2", CVAR_ARCHIVE);
	}

	if (!vid_renderer)
	{
		vid_renderer = Cvar_Get("vid_renderer", "gl1", CVAR_ARCHIVE);
	}

	if (!r_vsync)
	{
		r_vsync = Cvar_Get("r_vsync", "1", CVAR_ARCHIVE);
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

	s_renderer_list.generic.type = MTYPE_SPINCONTROL;
	s_renderer_list.generic.name = "renderer";
	s_renderer_list.generic.x = 0;
	s_renderer_list.generic.y = (y = 0);
	s_renderer_list.itemnames = renderers;
	s_renderer_list.curvalue = GetRenderer();

	s_mode_list.generic.type = MTYPE_SPINCONTROL;
	s_mode_list.generic.name = "video mode";
	s_mode_list.generic.x = 0;
	s_mode_list.generic.y = (y += 10);
	s_mode_list.itemnames = resolutions;

	if (r_mode->value >= 0)
	{
		s_mode_list.curvalue = r_mode->value;
	}
	else if (r_mode->value == -2)
	{
		// 'auto' is before 'custom'
		s_mode_list.curvalue = GetCustomValue(&s_mode_list) - 1;
	}
	else
	{
		// 'custom'
		s_mode_list.curvalue = GetCustomValue(&s_mode_list);
	}

	if (GLimp_GetNumVideoDisplays() > 1)
	{
		s_display_list.generic.type = MTYPE_SPINCONTROL;
		s_display_list.generic.name = "display index";
		s_display_list.generic.x = 0;
		s_display_list.generic.y = (y += 10);
		s_display_list.itemnames = GLimp_GetDisplayIndices();
		s_display_list.curvalue = GLimp_GetWindowDisplayIndex();
	}

	s_brightness_slider.generic.type = MTYPE_SLIDER;
	s_brightness_slider.generic.name = "brightness";
	s_brightness_slider.generic.x = 0;
	s_brightness_slider.generic.y = (y += 20);
	s_brightness_slider.generic.callback = BrightnessCallback;
	s_brightness_slider.minvalue = 1;
	s_brightness_slider.maxvalue = 20;
	s_brightness_slider.curvalue = vid_gamma->value * 10;

	s_fov_slider.generic.type = MTYPE_SLIDER;
	s_fov_slider.generic.x = 0;
	s_fov_slider.generic.y = (y += 10);
	s_fov_slider.generic.name = "field of view";
	s_fov_slider.generic.callback = FOVCallback;
	s_fov_slider.minvalue = 60;
	s_fov_slider.maxvalue = 120;
	s_fov_slider.curvalue = fov->value;

	s_uiscale_list.generic.type = MTYPE_SPINCONTROL;
	s_uiscale_list.generic.name = "ui scale";
	s_uiscale_list.generic.x = 0;
	s_uiscale_list.generic.y = (y += 10);
	s_uiscale_list.itemnames = uiscale_names;
	if (r_hudscale->value != r_consolescale->value ||
		r_hudscale->value != r_menuscale->value ||
		r_hudscale->value != crosshair_scale->value)
	{
		s_uiscale_list.curvalue = GetCustomValue(&s_uiscale_list);
	}
	else if (r_hudscale->value < 0)
	{
		s_uiscale_list.curvalue = 0;
	}
	else if (r_hudscale->value > 0 &&
			r_hudscale->value < GetCustomValue(&s_uiscale_list) &&
			r_hudscale->value == (int)r_hudscale->value)
	{
		s_uiscale_list.curvalue = r_hudscale->value;
	}
	else
	{
		s_uiscale_list.curvalue = GetCustomValue(&s_uiscale_list);
	}

	s_fs_box.generic.type = MTYPE_SPINCONTROL;
	s_fs_box.generic.name = "fullscreen";
	s_fs_box.generic.x = 0;
	s_fs_box.generic.y = (y += 10);
	s_fs_box.itemnames = fullscreen_names;
	s_fs_box.curvalue = (int)vid_fullscreen->value;

	s_vsync_list.generic.type = MTYPE_SPINCONTROL;
	s_vsync_list.generic.name = "vertical sync";
	s_vsync_list.generic.x = 0;
	s_vsync_list.generic.y = (y += 10);
	s_vsync_list.itemnames = yesno_names;
	s_vsync_list.curvalue = (r_vsync->value != 0);

	s_af_list.generic.type = MTYPE_SPINCONTROL;
	s_af_list.generic.name = "aniso filtering";
	s_af_list.generic.x = 0;
	s_af_list.generic.y = (y += 10);
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

	Menu_AddItem(&s_opengl_menu, (void *)&s_renderer_list);
	Menu_AddItem(&s_opengl_menu, (void *)&s_mode_list);

	// only show this option if we have multiple displays
	if (GLimp_GetNumVideoDisplays() > 1)
	{
		Menu_AddItem(&s_opengl_menu, (void *)&s_display_list);
	}

	Menu_AddItem(&s_opengl_menu, (void *)&s_brightness_slider);
	Menu_AddItem(&s_opengl_menu, (void *)&s_fov_slider);
	Menu_AddItem(&s_opengl_menu, (void *)&s_uiscale_list);
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
	int menu_key = Key_GetMenuKey(key);

	switch (menu_key)
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

