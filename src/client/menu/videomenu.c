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
 * refresher this menu must be altered. A lot stuff here are preworkings
 * for that and aren't needed at this time since we've only refresher.
 *
 * =======================================================================
 */

#include "../../client/header/client.h"
#include "../../client/menu/header/qmenu.h"

/* this will have to be updated if ref's are added/removed from ref_t */
#define NUMBER_OF_REFS 1

/* all the refs should be initially set to 0 */
static char *refs[NUMBER_OF_REFS + 1] = {0};

/* make all these have illegal values, as they will be redefined */
static int REF_GL = NUMBER_OF_REFS;
static int GL_REF_START = NUMBER_OF_REFS;

typedef struct
{
	char menuname[32];
	char realname[32];
	int *pointer;
} ref_t;

static const ref_t possible_refs[NUMBER_OF_REFS] = {
	{"[OpenGL        ]", "gl", &REF_GL},
};

extern cvar_t *vid_ref;
extern cvar_t *vid_fullscreen;
extern cvar_t *vid_gamma;
extern cvar_t *scr_viewsize;

static cvar_t *gl_mode;
static cvar_t *gl_driver;
static cvar_t *gl_picmip;
static cvar_t *gl_ext_palettedtexture;

static cvar_t *windowed_mouse;

extern void M_ForceMenuOff(void);
extern qboolean VID_CheckRefExists(const char *name);

/* MENU INTERACTION */
#define SOFTWARE_MENU 0
#define OPENGL_MENU 1
#define CUSTOM_MODE 20

static menuframework_s s_opengl_menu;
static menuframework_s *s_current_menu;
static int s_current_menu_index;

static menulist_s s_mode_list[2];
static menulist_s s_ref_list[2];
static menuslider_s s_tq_slider;
static menuslider_s s_screensize_slider[2];
static menuslider_s s_brightness_slider[2];
static menulist_s s_fs_box[2];
static menulist_s s_paletted_texture_box;
static menuaction_s s_apply_action[2];
static menuaction_s s_defaults_action[2];

static void
DriverCallback(void *unused)
{
	s_ref_list[!s_current_menu_index].curvalue =
		s_ref_list[s_current_menu_index].curvalue;

	s_current_menu = &s_opengl_menu;
	s_current_menu_index = 1;
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

	if (s_current_menu_index == 0)
	{
		s_brightness_slider[1].curvalue = s_brightness_slider[0].curvalue;
	}
	else
	{
		s_brightness_slider[0].curvalue = s_brightness_slider[1].curvalue;
	}

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
	int ref;

	/* make values consistent */
	s_fs_box[!s_current_menu_index].curvalue =
		s_fs_box[s_current_menu_index].curvalue;
	s_brightness_slider[!s_current_menu_index].curvalue =
		s_brightness_slider[s_current_menu_index].curvalue;
	s_ref_list[!s_current_menu_index].curvalue =
		s_ref_list[s_current_menu_index].curvalue;

	Cvar_SetValue("gl_picmip", 3 - s_tq_slider.curvalue);
	Cvar_SetValue("vid_fullscreen", s_fs_box[s_current_menu_index].curvalue);
	Cvar_SetValue("gl_ext_palettedtexture", s_paletted_texture_box.curvalue);

	/* custom mode */
	if (s_mode_list[OPENGL_MENU].curvalue != CUSTOM_MODE)
	{
		printf("DEBUG: %i\n", s_mode_list[OPENGL_MENU].curvalue);
		Cvar_SetValue("gl_mode", s_mode_list[OPENGL_MENU].curvalue);
	}
	else
	{
		Cvar_SetValue("gl_mode", -1);
	}

	/* must use an if here (instead of a switch), since the
	   REF_'s are now variables and not #DEFINE's (constants) */
	ref = s_ref_list[s_current_menu_index].curvalue;

	if (ref == REF_GL)
	{
		Cvar_Set("vid_ref", "gl");

		/* below is wrong if we use different libs for different GL reflibs */
		Cvar_Get("gl_driver", "libGL.so.1", CVAR_ARCHIVE);

		if (gl_driver->modified)
		{
			vid_ref->modified = true;
		}
	}

	M_ForceMenuOff();
}

void
VID_MenuInit(void)
{
	int i, counter;

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

	/* make sure these are invalided before showing the menu again */
	REF_GL = NUMBER_OF_REFS;
	GL_REF_START = NUMBER_OF_REFS;

	/* now test to see which ref's are present */
	i = counter = 0;

	while (i < NUMBER_OF_REFS)
	{
		if (VID_CheckRefExists(possible_refs[i].realname))
		{
			*(possible_refs[i].pointer) = counter;

			/* free any previous string */
			if (refs[i])
			{
				free(refs[i]);
			}

			refs[counter] = strdup(possible_refs[i].menuname);

			/* if we reach the 1rd item in the list, this indicates that a
			   GL ref has been found; this will change if more software
			   modes are added to the possible_ref's array */
			if (i == 0)
			{
				GL_REF_START = counter;
			}

			counter++;
		}

		i++;
	}

	refs[counter] = (char *)0;

	if (!gl_driver)
	{
		gl_driver = Cvar_Get("gl_driver", "libGL.so.1", 0);
	}

	if (!gl_picmip)
	{
		gl_picmip = Cvar_Get("gl_picmip", "0", 0);
	}

	if (!gl_mode)
	{
		gl_mode = Cvar_Get("gl_mode", "3", 0);
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

	/* custom mode */
	if (gl_mode->value >= 1.0)
	{
		s_mode_list[OPENGL_MENU].curvalue = gl_mode->value;
	}
	else
	{
		s_mode_list[OPENGL_MENU].curvalue = CUSTOM_MODE;
	}

	if (!scr_viewsize)
	{
		scr_viewsize = Cvar_Get("viewsize", "100", CVAR_ARCHIVE);
	}

	s_screensize_slider[OPENGL_MENU].curvalue = scr_viewsize->value / 10;

	if (strcmp(vid_ref->string, "gl") == 0)
	{
		s_current_menu_index = OPENGL_MENU;
		s_ref_list[s_current_menu_index].curvalue = REF_GL;
	}

	s_opengl_menu.x = viddef.width * 0.50;
	s_opengl_menu.nitems = 0;

	for (i = 0; i < 2; i++)
	{
		s_ref_list[i].generic.type = MTYPE_SPINCONTROL;
		s_ref_list[i].generic.name = "driver";
		s_ref_list[i].generic.x = 0;
		s_ref_list[i].generic.y = 0;
		s_ref_list[i].generic.callback = DriverCallback;
		s_ref_list[i].itemnames = (const char **)refs;

		s_mode_list[i].generic.type = MTYPE_SPINCONTROL;
		s_mode_list[i].generic.name = "video mode";
		s_mode_list[i].generic.x = 0;
		s_mode_list[i].generic.y = 10;
		s_mode_list[i].itemnames = resolutions;

		s_screensize_slider[i].generic.type = MTYPE_SLIDER;
		s_screensize_slider[i].generic.x = 0;
		s_screensize_slider[i].generic.y = 20;
		s_screensize_slider[i].generic.name = "screen size";
		s_screensize_slider[i].minvalue = 3;
		s_screensize_slider[i].maxvalue = 12;
		s_screensize_slider[i].generic.callback = ScreenSizeCallback;

		s_brightness_slider[i].generic.type = MTYPE_SLIDER;
		s_brightness_slider[i].generic.x = 0;
		s_brightness_slider[i].generic.y = 30;
		s_brightness_slider[i].generic.name = "brightness";
		s_brightness_slider[i].generic.callback = BrightnessCallback;
		s_brightness_slider[i].minvalue = 1;
		s_brightness_slider[i].maxvalue = 20;
		s_brightness_slider[i].curvalue = vid_gamma->value * 10;

		s_fs_box[i].generic.type = MTYPE_SPINCONTROL;
		s_fs_box[i].generic.x = 0;
		s_fs_box[i].generic.y = 40;
		s_fs_box[i].generic.name = "fullscreen";
		s_fs_box[i].itemnames = yesno_names;
		s_fs_box[i].curvalue = vid_fullscreen->value;

		s_defaults_action[i].generic.type = MTYPE_ACTION;
		s_defaults_action[i].generic.name = "reset to default";
		s_defaults_action[i].generic.x = 0;
		s_defaults_action[i].generic.y = 90;
		s_defaults_action[i].generic.callback = ResetDefaults;

		s_apply_action[i].generic.type = MTYPE_ACTION;
		s_apply_action[i].generic.name = "apply";
		s_apply_action[i].generic.x = 0;
		s_apply_action[i].generic.y = 100;
		s_apply_action[i].generic.callback = ApplyChanges;
	}

	s_tq_slider.generic.type = MTYPE_SLIDER;
	s_tq_slider.generic.x = 0;
	s_tq_slider.generic.y = 60;
	s_tq_slider.generic.name = "texture quality";
	s_tq_slider.minvalue = 0;
	s_tq_slider.maxvalue = 3;
	s_tq_slider.curvalue = 3 - gl_picmip->value;

	s_paletted_texture_box.generic.type = MTYPE_SPINCONTROL;
	s_paletted_texture_box.generic.x = 0;
	s_paletted_texture_box.generic.y = 70;
	s_paletted_texture_box.generic.name = "8-bit textures";
	s_paletted_texture_box.itemnames = yesno_names;
	s_paletted_texture_box.curvalue = gl_ext_palettedtexture->value;

	Menu_AddItem(&s_opengl_menu, (void *)&s_ref_list[OPENGL_MENU]);
	Menu_AddItem(&s_opengl_menu, (void *)&s_mode_list[OPENGL_MENU]);
	Menu_AddItem(&s_opengl_menu, (void *)&s_screensize_slider[OPENGL_MENU]);
	Menu_AddItem(&s_opengl_menu, (void *)&s_brightness_slider[OPENGL_MENU]);
	Menu_AddItem(&s_opengl_menu, (void *)&s_fs_box[OPENGL_MENU]);
	Menu_AddItem(&s_opengl_menu, (void *)&s_tq_slider);
	Menu_AddItem(&s_opengl_menu, (void *)&s_paletted_texture_box);
	Menu_AddItem(&s_opengl_menu, (void *)&s_defaults_action[OPENGL_MENU]);
	Menu_AddItem(&s_opengl_menu, (void *)&s_apply_action[OPENGL_MENU]);

	Menu_Center(&s_opengl_menu);
	s_opengl_menu.x -= 8;
}

void
VID_MenuShutdown(void)
{
	int i;

	for (i = 0; i < NUMBER_OF_REFS; i++)
	{
		if (refs[i])
		{
			free(refs[i]);
		}
	}
}

void
VID_MenuDraw(void)
{
	int w, h;

	s_current_menu = &s_opengl_menu;

	/* draw the banner */
	re.DrawGetPicSize(&w, &h, "m_banner_video");
	re.DrawPic(viddef.width / 2 - w / 2, viddef.height / 2 - 110,
			"m_banner_video");

	/* move cursor to a reasonable starting position */
	Menu_AdjustCursor(s_current_menu, 1);

	/* draw the menu */
	Menu_Draw(s_current_menu);
}

const char *
VID_MenuKey(int key)
{
	extern void M_PopMenu(void);

	menuframework_s *m = s_current_menu;
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

