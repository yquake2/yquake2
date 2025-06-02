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
extern cvar_t *scr_viewsize;
extern cvar_t *vid_fullscreen;
extern cvar_t *vid_renderer;
static cvar_t *r_vsync;
static cvar_t *gl_anisotropic;
static cvar_t *gl_msaa_samples;
static cvar_t *gl3_colorlight;
static cvar_t *gl4_colorlight;
static cvar_t *vk_dynamic;

static menuframework_s s_opengl_menu;

static menulist_s s_renderer_list;
static menulist_s s_mode_list;
static menulist_s s_uiscale_list;
static menuslider_s s_brightness_slider;
static menuslider_s s_fov_slider;
static menuslider_s s_gl1_intensity_slider;
static menuslider_s s_gl3_intensity_slider;
static menuslider_s s_gl4_intensity_slider;
static menuslider_s s_vk_intensity_slider;
static menuslider_s s_gl1_overbrightbits_slider;
static menuslider_s s_gl3_overbrightbits_slider;
static menuslider_s s_gl4_overbrightbits_slider;
static menuslider_s s_vk_overbrightbits_slider;
static menulist_s s_gl3_colorlight_list;
static menulist_s s_gl4_colorlight_list;
static menulist_s s_vk_dynamic_list;
static menulist_s s_fs_box;
static menulist_s s_vsync_list;
static menulist_s s_af_list;
static menulist_s s_msaa_list;
static menulist_s s_filter_list;
static menuaction_s s_defaults_action;
static menuaction_s s_apply_action;

// --------

// gl1, gl3, gles1, gles3, gl4, vk, soft
#define MAXRENDERERS 7

typedef enum
{
	ref_custom,
	ref_gl1,	// encompasses gl1 and gles1
	ref_gl3,	// encompasses gl3 and gles3
	ref_gl4,
	ref_vk,
	ref_soft
} renderer_type;

typedef struct
{
	const char *boxstr;
	const char *cvarstr;
} renderer;

renderer rendererlist[MAXRENDERERS];
int numrenderer;

static void
Renderer_FillRenderdef(void)
{
	numrenderer = -1;

	if (VID_HasRenderer("gl1"))
	{
		numrenderer++;
		rendererlist[numrenderer].boxstr = "[OpenGL 1.4]";
		rendererlist[numrenderer].cvarstr = "gl1";
	}

	if (VID_HasRenderer("gles1"))
	{
		numrenderer++;
		rendererlist[numrenderer].boxstr = "[OpenGL ES1]";
		rendererlist[numrenderer].cvarstr = "gles1";
	}

	if (VID_HasRenderer("gl3"))
	{
		numrenderer++;
		rendererlist[numrenderer].boxstr = "[OpenGL 3.2]";
		rendererlist[numrenderer].cvarstr = "gl3";
	}

	if (VID_HasRenderer("gles3"))
	{
		numrenderer++;
		rendererlist[numrenderer].boxstr = "[OpenGL ES3]";
		rendererlist[numrenderer].cvarstr = "gles3";
	}

	if (VID_HasRenderer("gl4"))
	{
		numrenderer++;
		rendererlist[numrenderer].boxstr = "[OpenGL 4.6]";
		rendererlist[numrenderer].cvarstr = "gl4";
	}

	if (VID_HasRenderer("vk"))
	{
		numrenderer++;
		rendererlist[numrenderer].boxstr = "[Vulkan    ]";
		rendererlist[numrenderer].cvarstr = "vk";
	}

	if (VID_HasRenderer("soft"))
	{
		numrenderer++;
		rendererlist[numrenderer].boxstr = "[Software  ]";
		rendererlist[numrenderer].cvarstr = "soft";
	}

	// The custom renderer. Must be known to the menu,
	// but nothing more. The display string is hard
	// coded below, the cvar is unknown.
	numrenderer++;
}

static int
Renderer_GetRenderer(void)
{
	for (int i = 0; i < numrenderer; i++)
	{
		if (strcmp(vid_renderer->string, rendererlist[i].cvarstr) == 0)
		{
			return i;
		}
	}

	// Unknown renderer.
	return numrenderer;
}

// --------

#define NUMMODES 33

typedef struct
{
	int modenum;
	char *modestr;
} mode;

// Sorted by string.
mode modes[] = {
	{ 0, "[320 240   ]" },
	{ 1, "[400 300   ]" },
	{ 2, "[512 384   ]" },
	{ 3, "[640 400   ]" },
	{ 4, "[640 480   ]" },
	{ 5, "[800 500   ]" },
	{ 6, "[800 600   ]" },
	{ 7, "[960 720   ]" },
	{ 8, "[1024 480  ]" },
	{ 9, "[1024 640  ]" },
	{ 10, "[1024 768  ]" },
	{ 11, "[1152 768  ]" },
	{ 12, "[1152 864  ]" },
	{ 13, "[1280 800  ]" },
	{ 14, "[1280 720  ]" },
	{ 15, "[1280 960  ]" },
	{ 16, "[1280 1024 ]" },
	{ 17, "[1366 768  ]" },
	{ 18, "[1440 900  ]" },
	{ 32, "[1600 900  ]" },
	{ 19, "[1600 1200 ]" },
	{ 20, "[1680 1050 ]" },
	{ 21, "[1920 1080 ]" },
	{ 22, "[1920 1200 ]" },
	{ 23, "[2048 1536 ]" },
	{ 24, "[2560 1080 ]" },
	{ 25, "[2560 1440 ]" },
	{ 26, "[2560 1600 ]" },
	{ 27, "[3440 1440 ]" },
	{ 28, "[3840 1600 ]" },
	{ 29, "[3840 2160 ]" },
	{ 30, "[4096 2160 ]" },
	{ 31, "[5120 2880 ]" }
};

static int
GetModePos(int modenum)
{
	for (int i = 0; i < sizeof(modes); i++)
	{
		if (modes[i].modenum == modenum)
		{
			return i;
		}
	}

	return 3;
}

// --------

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
ResetDefaults(void *unused)
{
	VID_MenuInit();
}

#define CUSTOM_MODE_NAME "[Custom    ]"
#define AUTO_MODE_NAME   "[Auto      ]"

static renderer_type
CurrentRendererByCvar(void)
{
	if (Q_stricmp(vid_renderer->string, "gl3") == 0 || Q_stricmp(vid_renderer->string, "gles3") == 0)
	{
		return ref_gl3;
	}
	if (Q_stricmp(vid_renderer->string, "gl1") == 0 || Q_stricmp(vid_renderer->string, "gles1") == 0)
	{
		return ref_gl1;
	}
	if (Q_stricmp(vid_renderer->string, "soft") == 0)
	{
		return ref_soft;
	}
	if (Q_stricmp(vid_renderer->string, "gl4") == 0)
	{
		return ref_gl4;
	}
	if (Q_stricmp(vid_renderer->string, "vk") == 0)
	{
		return ref_vk;
	}
	return ref_custom;
}

static void
ApplyFilter(void* unused)
{
	switch (CurrentRendererByCvar())
	{
		case ref_gl3:
		case ref_gl1:
			if (s_filter_list.curvalue == 0)
			{
				Cvar_Set("gl_texturemode", "GL_NEAREST");
			}
			else if (s_filter_list.curvalue == 1)
			{
				Cvar_Set("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST");
			}
			else if (s_filter_list.curvalue == 2)
			{
				Cvar_Set("gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR");
			}
			break;
		case ref_soft:
			if (s_filter_list.curvalue == 0)
			{
				Cvar_Set("sw_texture_filtering", "0");
			}
			else if (s_filter_list.curvalue == 1)
			{
				Cvar_Set("sw_texture_filtering", "1");
			}
			break;
		default:
			break;	// avoid compiler warning
	}
}

static void
ApplyChanges(void *unused)
{
	qboolean restart = false;

	/* Renderer */
	if (s_renderer_list.curvalue != Renderer_GetRenderer())
	{
		// The custom renderer (the last known renderer) cannot be
		// set, because the menu doesn't know it's cvar value. TODO:
		// Hack something that it cannot be selected.
		if (s_renderer_list.curvalue != numrenderer)
		{
			Cvar_Set("vid_renderer", (char *)rendererlist[s_renderer_list.curvalue].cvarstr);
			restart = true;
		}
	}

	/* auto mode */

	if (strcmp(s_mode_list.itemnames[s_mode_list.curvalue], AUTO_MODE_NAME) == 0)
	{
		Cvar_SetValue("r_mode", -2);
		restart = true;
	}
	else if (strcmp(s_mode_list.itemnames[s_mode_list.curvalue], CUSTOM_MODE_NAME) == 0)
	{
		Cvar_SetValue("r_mode", -1);
		restart = true;
	}
	else
	{
		Cvar_SetValue("r_mode", modes[s_mode_list.curvalue].modenum);
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
	if (vid_fullscreen->value != s_fs_box.curvalue)
	{
		Cvar_SetValue("vid_fullscreen", s_fs_box.curvalue);
		restart = true;
	}

	if (s_fs_box.curvalue == 2)
	{
		Cvar_SetValue("r_mode", -2.0f);
	}

	/* vertical sync */
	if (r_vsync->value != s_vsync_list.curvalue)
	{
		Cvar_SetValue("r_vsync", s_vsync_list.curvalue);
		restart = true;
	}

	if (gl3_colorlight && gl3_colorlight->value != s_gl3_colorlight_list.curvalue)
	{
		Cvar_SetValue("gl3_colorlight", s_gl3_colorlight_list.curvalue);
	}

	/* anisotropic filtering */
	if (s_af_list.curvalue == 0)
	{
		if (gl_anisotropic->value != 0)
		{
			Cvar_SetValue("r_anisotropic", 0);
			restart = true;
		}
	}
	else
	{
		if (gl_anisotropic->value != pow(2, s_af_list.curvalue))
		{
			Cvar_SetValue("r_anisotropic", pow(2, s_af_list.curvalue));
			restart = true;
		}
	}

	/* multisample anti-aliasing */
	if (s_msaa_list.curvalue == 0)
	{
		if (gl_msaa_samples->value != 0)
		{
			Cvar_SetValue("r_msaa_samples", 0);
			restart = true;
		}
	}
	else
	{
		if (gl_msaa_samples->value != pow(2, s_msaa_list.curvalue))
		{
			Cvar_SetValue("r_msaa_samples", pow(2, s_msaa_list.curvalue));
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
	int y = 30;

	// Renderer selection box.
	// MAXRENDERERS + Custom + NULL.
	static const char *renderers[MAXRENDERERS + 2] = { NULL };
	Renderer_FillRenderdef();

	for (int i = 0; i < numrenderer; i++)
	{
		renderers[i] = rendererlist[i].boxstr;
	}

	renderers[numrenderer] = CUSTOM_MODE_NAME;

	// Generate mode list.
	static const char *resolutions[NUMMODES + 3];

	for (int i = 0; i < NUMMODES; i++)
	{
		resolutions[i] = modes[i].modestr;
	}

	resolutions[NUMMODES] = AUTO_MODE_NAME;
	resolutions[NUMMODES + 1] = CUSTOM_MODE_NAME;
	resolutions[NUMMODES + 2] = NULL;

	const renderer_type current_renderer = CurrentRendererByCvar();

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

	static const char *onoff_names[] = {
		"off",
		"on",
		0
	};
	
	static const char *yesno_names[] = {
		"no",
		"yes",
		0
	};

	static const char *fullscreen_names[] = {
			"no",
			"native fullscreen",
			"fullscreen window",
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

	static const char *filter_names[] = {
		"pixelated",
		"standard",
		"trilinear",
		"custom",
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
		gl_anisotropic = Cvar_Get("r_anisotropic", "0", CVAR_ARCHIVE);
	}

	if (!gl_msaa_samples)
	{
		gl_msaa_samples = Cvar_Get("r_msaa_samples", "0", CVAR_ARCHIVE);
	}

	s_opengl_menu.x = viddef.width * 0.50;
	s_opengl_menu.nitems = 0;

	s_renderer_list.generic.type = MTYPE_SPINCONTROL;
	s_renderer_list.generic.name = "renderer";
	s_renderer_list.generic.x = 0;
	s_renderer_list.generic.y = y;
	s_renderer_list.itemnames = renderers;
	s_renderer_list.curvalue = Renderer_GetRenderer();

	s_mode_list.generic.type = MTYPE_SPINCONTROL;
	s_mode_list.generic.name = "video mode";
	s_mode_list.generic.x = 0;
	s_mode_list.generic.y = (y += 10);
	s_mode_list.itemnames = resolutions;

	if (r_mode->value >= 0)
	{
		s_mode_list.curvalue = GetModePos(r_mode->value);
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

	s_brightness_slider.generic.type = MTYPE_SLIDER;
	s_brightness_slider.generic.name = "brightness";
	s_brightness_slider.generic.x = 0;
	s_brightness_slider.generic.y = (y += 10);
	s_brightness_slider.cvar = "vid_gamma";
	s_brightness_slider.minvalue = 0.1f;
	s_brightness_slider.maxvalue = 2.0f;

	s_fov_slider.generic.type = MTYPE_SLIDER;
	s_fov_slider.generic.name = "field of view";
	s_fov_slider.generic.x = 0;
	s_fov_slider.generic.y = (y += 10);
	s_fov_slider.cvar = "fov";
	s_fov_slider.minvalue = 60;
	s_fov_slider.maxvalue = 120;
	s_fov_slider.slidestep = 1;
	s_fov_slider.printformat = "%.0f";

	switch (current_renderer)
	{
		case ref_gl3:
			s_gl3_intensity_slider.generic.type = MTYPE_SLIDER;
			s_gl3_intensity_slider.generic.name = "color intensity";
			s_gl3_intensity_slider.generic.x = 0;
			s_gl3_intensity_slider.generic.y = (y += 10);
			s_gl3_intensity_slider.cvar = "gl3_intensity";
			s_gl3_intensity_slider.minvalue = 0.1f;
			s_gl3_intensity_slider.maxvalue = 5.0f;

			s_gl3_overbrightbits_slider.generic.type = MTYPE_SLIDER;
			s_gl3_overbrightbits_slider.generic.name = "overbrights";
			s_gl3_overbrightbits_slider.generic.x = 0;
			s_gl3_overbrightbits_slider.generic.y = (y += 10);
			s_gl3_overbrightbits_slider.cvar = "gl3_overbrightbits";
			s_gl3_overbrightbits_slider.minvalue = 0.1f;
			s_gl3_overbrightbits_slider.maxvalue = 5.0f;

			gl3_colorlight = Cvar_Get("gl3_colorlight", "1", CVAR_ARCHIVE);
			s_gl3_colorlight_list.generic.type = MTYPE_SPINCONTROL;
			s_gl3_colorlight_list.generic.name = "color light";
			s_gl3_colorlight_list.generic.x = 0;
			s_gl3_colorlight_list.generic.y = (y += 10);
			s_gl3_colorlight_list.itemnames = yesno_names;
			s_gl3_colorlight_list.curvalue = (gl3_colorlight->value != 0);
			break;

		case ref_gl4:
			s_gl4_intensity_slider.generic.type = MTYPE_SLIDER;
			s_gl4_intensity_slider.generic.name = "color intensity";
			s_gl4_intensity_slider.generic.x = 0;
			s_gl4_intensity_slider.generic.y = (y += 10);
			s_gl4_intensity_slider.cvar = "gl4_intensity";
			s_gl4_intensity_slider.minvalue = 0.1f;
			s_gl4_intensity_slider.maxvalue = 5.0f;

			s_gl4_overbrightbits_slider.generic.type = MTYPE_SLIDER;
			s_gl4_overbrightbits_slider.generic.name = "overbrights";
			s_gl4_overbrightbits_slider.generic.x = 0;
			s_gl4_overbrightbits_slider.generic.y = (y += 10);
			s_gl4_overbrightbits_slider.cvar = "gl4_overbrightbits";
			s_gl4_overbrightbits_slider.minvalue = 0.1f;
			s_gl4_overbrightbits_slider.maxvalue = 5.0f;

			gl4_colorlight = Cvar_Get("gl4_colorlight", "1", CVAR_ARCHIVE);
			s_gl4_colorlight_list.generic.type = MTYPE_SPINCONTROL;
			s_gl4_colorlight_list.generic.name = "color light";
			s_gl4_colorlight_list.generic.x = 0;
			s_gl4_colorlight_list.generic.y = (y += 10);
			s_gl4_colorlight_list.itemnames = yesno_names;
			s_gl4_colorlight_list.curvalue = (gl4_colorlight->value != 0);
			break;

		case ref_vk:
			s_vk_intensity_slider.generic.type = MTYPE_SLIDER;
			s_vk_intensity_slider.generic.name = "color intensity";
			s_vk_intensity_slider.generic.x = 0;
			s_vk_intensity_slider.generic.y = (y += 10);
			s_vk_intensity_slider.cvar = "vk_intensity";
			s_vk_intensity_slider.minvalue = 0;
			s_vk_intensity_slider.maxvalue = 5;
			s_vk_intensity_slider.slidestep = 1;
			s_vk_intensity_slider.printformat = "%.0f";

			s_vk_overbrightbits_slider.generic.type = MTYPE_SLIDER;
			s_vk_overbrightbits_slider.generic.name = "overbrights";
			s_vk_overbrightbits_slider.generic.x = 0;
			s_vk_overbrightbits_slider.generic.y = (y += 10);
			s_vk_overbrightbits_slider.cvar = "vk_overbrightbits";
			s_vk_overbrightbits_slider.minvalue = 0.1f;
			s_vk_overbrightbits_slider.maxvalue = 5.0f;

			vk_dynamic = Cvar_Get("vk_dynamic", "1", CVAR_ARCHIVE);
			s_vk_dynamic_list.generic.type = MTYPE_SPINCONTROL;
			s_vk_dynamic_list.generic.name = "dynamic light";
			s_vk_dynamic_list.generic.x = 0;
			s_vk_dynamic_list.generic.y = (y += 10);
			s_vk_dynamic_list.itemnames = yesno_names;
			s_vk_dynamic_list.curvalue = (vk_dynamic->value != 0);
			break;

		case ref_gl1:
			gl3_colorlight = NULL;
			s_gl1_intensity_slider.generic.type = MTYPE_SLIDER;
			s_gl1_intensity_slider.generic.name = "color intensity";
			s_gl1_intensity_slider.generic.x = 0;
			s_gl1_intensity_slider.generic.y = (y += 10);
			s_gl1_intensity_slider.cvar = "gl1_intensity";
			s_gl1_intensity_slider.minvalue = 1.0f;
			s_gl1_intensity_slider.maxvalue = 10.0f;

			s_gl1_overbrightbits_slider.generic.type = MTYPE_SLIDER;
			s_gl1_overbrightbits_slider.generic.name = "overbrights";
			s_gl1_overbrightbits_slider.generic.x = 0;
			s_gl1_overbrightbits_slider.generic.y = (y += 10);
			s_gl1_overbrightbits_slider.cvar = "gl1_overbrightbits";
			s_gl1_overbrightbits_slider.minvalue = 0;
			s_gl1_overbrightbits_slider.maxvalue = 2;
			s_gl1_overbrightbits_slider.slidestep = 1;
			s_gl1_overbrightbits_slider.printformat = "%.0f";
			break;

		default:
			break;
	}

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
	
	s_filter_list.generic.type = MTYPE_SPINCONTROL;
	s_filter_list.generic.name = "texture filter";
	s_filter_list.curvalue = 0;
	s_filter_list.generic.callback = ApplyFilter;
	s_filter_list.generic.x = 0;

	const char* filter = NULL;
	int mode = 0;

	switch (current_renderer)
	{
		case ref_gl3:
		case ref_gl1:
			s_filter_list.generic.y = (y += 10);
			s_filter_list.itemnames = filter_names;

			filter = Cvar_VariableString("gl_texturemode");
			mode = 3;

			if (Q_stricmp(filter, "GL_NEAREST") == 0)
			{
				mode = 0;
			}
			else if (Q_stricmp(filter, "GL_LINEAR_MIPMAP_NEAREST") == 0)
			{
				mode = 1;
			}
			else if (Q_stricmp(filter, "GL_LINEAR_MIPMAP_LINEAR") == 0)
			{
				mode = 2;
			}
			break;

		case ref_soft:
			s_filter_list.generic.y = (y += 10);
			s_filter_list.itemnames = onoff_names;

			filter = Cvar_VariableString("sw_texture_filtering");
			mode = 0;

			if (Q_stricmp(filter, "1") == 0)
			{
				mode = 1;
			}
			break;

		default:
			break;
	}

	s_filter_list.curvalue = mode;

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

	Menu_AddItem(&s_opengl_menu, (void *)&s_brightness_slider);
	Menu_AddItem(&s_opengl_menu, (void *)&s_fov_slider);
	switch (current_renderer)
	{
		case ref_gl3:
			Menu_AddItem(&s_opengl_menu, (void *)&s_gl3_intensity_slider);
			Menu_AddItem(&s_opengl_menu, (void *)&s_gl3_overbrightbits_slider);
			Menu_AddItem(&s_opengl_menu, (void *)&s_gl3_colorlight_list);
			break;
		case ref_gl4:
			Menu_AddItem(&s_opengl_menu, (void *)&s_gl4_intensity_slider);
			Menu_AddItem(&s_opengl_menu, (void *)&s_gl4_overbrightbits_slider);
			Menu_AddItem(&s_opengl_menu, (void *)&s_gl4_colorlight_list);
			break;
		case ref_vk:
			Menu_AddItem(&s_opengl_menu, (void *)&s_vk_intensity_slider);
			Menu_AddItem(&s_opengl_menu, (void *)&s_vk_overbrightbits_slider);
			Menu_AddItem(&s_opengl_menu, (void *)&s_vk_dynamic_list);
			break;
		case ref_gl1:
			Menu_AddItem(&s_opengl_menu, (void *)&s_gl1_intensity_slider);
			Menu_AddItem(&s_opengl_menu, (void *)&s_gl1_overbrightbits_slider);
			break;
		default:
			break;
	}
	Menu_AddItem(&s_opengl_menu, (void *)&s_uiscale_list);
	Menu_AddItem(&s_opengl_menu, (void *)&s_fs_box);
	Menu_AddItem(&s_opengl_menu, (void *)&s_vsync_list);
	Menu_AddItem(&s_opengl_menu, (void *)&s_af_list);
	Menu_AddItem(&s_opengl_menu, (void *)&s_msaa_list);
	if (current_renderer == ref_gl3 || current_renderer == ref_gl1 || current_renderer == ref_soft)
	{
		Menu_AddItem(&s_opengl_menu, (void *)&s_filter_list);
	}
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

/*
 * VIDEO MENU
 */

void
M_Menu_Video_f(void)
{
    VID_MenuInit();
    s_opengl_menu.draw = VID_MenuDraw;
    s_opengl_menu.key  = VID_MenuKey;

    M_PushMenu(&s_opengl_menu);
}


