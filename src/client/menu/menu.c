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
 * This file implements the non generic part of the menu system, e.g.
 * the menu shown to the player. Beware! This code is very fragile and
 * should only be touched with great care and exessive testing.
 * Otherwise strange things and hard to track down bugs can occure. In a
 * better world someone would rewrite this file to something more like
 * Quake III Team Arena.
 *
 * =======================================================================
 */

#include <ctype.h>
#include "../header/client.h"
#include "../input/header/gyro.h"
#include "../sound/header/local.h"
#include "header/qmenu.h"

/* Number of the frames of the spinning quake logo */
#define NUM_CURSOR_FRAMES 15
static int m_cursor_width = 0;

/* Signals the file system to start the demo loop. */
qboolean menu_startdemoloop;

static char *menu_in_sound = "misc/menu1.wav";
static char *menu_move_sound = "misc/menu2.wav";
static char *menu_out_sound = "misc/menu3.wav";

void M_Menu_Main_f(void);
static void M_Menu_Game_f(void);
static void M_Menu_LoadGame_f(void);
static void M_Menu_SaveGame_f(void);
static void M_Menu_PlayerConfig_f(void);
static void M_Menu_DownloadOptions_f(void);
static void M_Menu_Credits_f(void);
static void M_Menu_Mods_f(void);
static void M_Menu_Multiplayer_f(void);
static void M_Menu_Multiplayer_Keys_f(void);
static void M_Menu_JoinServer_f(void);
static void M_Menu_AddressBook_f(void);
static void M_Menu_StartServer_f(void);
static void M_Menu_DMOptions_f(void);
void M_Menu_Video_f(void);
static void M_Menu_Options_f(void);
static void M_Menu_Keys_f(void);
static void M_Menu_Joy_f(void);
static void M_Menu_ControllerButtons_f(void);
static void M_Menu_ControllerAltButtons_f(void);
static void M_Menu_Quit_f(void);

void M_Menu_Credits(void);

qboolean m_entersound; /* play after drawing a frame, so caching won't disrupt the sound */

/* Maximal number of submenus */
#define MAX_MENU_DEPTH 8

typedef struct
{
	void (*draw)(void);
	const char *(*key)(int k);
} menulayer_t;

static menulayer_t m_layers[MAX_MENU_DEPTH];
static menulayer_t m_active;		/* active menu layer */
static int m_menudepth;

static qboolean
M_IsGame(const char *gamename)
{
	cvar_t *game = Cvar_Get("game", "", CVAR_LATCH | CVAR_SERVERINFO);

	if (strcmp(game->string, gamename) == 0
		|| (strcmp(gamename, BASEDIRNAME) == 0 && strcmp(game->string, "") == 0))
	{
		return true;
	}

	return false;
}

static void
M_Banner(const char *name)
{
	int w, h;
	float scale = SCR_GetMenuScale();

	Draw_GetPicSize(&w, &h, name);
	Draw_PicScaled(viddef.width / 2 - (w * scale) / 2, viddef.height / 2 - (110 * scale), name, scale);
}

void
M_ForceMenuOff(void)
{
	cls.key_dest = key_game;
	m_active.draw = NULL;
	m_active.key  = NULL;
	m_menudepth = 0;
	Key_MarkAllUp();
	Cvar_Set("paused", "0");
}

void
M_PopMenu(void)
{
	S_StartLocalSound(menu_out_sound);

	if (m_menudepth < 1)
	{
		Com_Error(ERR_FATAL, "%s: depth < 1", __func__);
		return;
	}

	m_menudepth--;

	m_active.draw = m_layers[m_menudepth].draw;
	m_active.key  = m_layers[m_menudepth].key;

	if (!m_menudepth)
	{
		M_ForceMenuOff();
		/* play music */
		if (Cvar_VariableValue("ogg_pausewithgame") == 1 &&
				OGG_Status() == PAUSE && cl.attractloop == false)
		{
			Cbuf_AddText("ogg toggle\n");
		}
	}
}

// Similar to M_PopMenu but silent and doesn't toggle music or paused state
static void
M_PopMenuSilent(void)
{
	if (m_menudepth < 1)
	{
		Com_Error(ERR_FATAL, "%s: depth < 1", __func__);
		return;
	}

	m_menudepth--;

	if (m_menudepth)
	{
		m_active.draw = m_layers[m_menudepth].draw;
		m_active.key  = m_layers[m_menudepth].key;
	}
	else
	{
		m_active.draw = NULL;
		m_active.key  = NULL;
	}
}

/*
 * This crappy function maintaines a stack of opened menus.
 * The steps in this horrible mess are:
 *
 * 1. But the game into pause if a menu is opened
 *
 * 2. If the requested menu is already open, close it.
 *
 * 3. If the requested menu is already open but not
 *    on top, close all menus above it and the menu
 *    itself. This is necessary since an instance of
 *    the reqeuested menu is in flight and will be
 *    displayed.
 *
 * 4. Save the previous menu on top (which was in flight)
 *    to the stack and make the requested menu the menu in
 *    flight.
 */
void
M_PushMenu(menuframework_s* menu)
{
	int i;
	int alreadyPresent = 0;

	if (menu == NULL)
	{
		return;
	}

	if ((Cvar_VariableValue("maxclients") == 1) &&
			Com_ServerState())
	{
		Cvar_Set("paused", "1");
	}

#ifdef USE_OPENAL
	if (cl.cinematic_file && sound_started == SS_OAL)
	{
		AL_UnqueueRawSamples();
	}
#endif

	/* pause music */
	if (Cvar_VariableValue("ogg_pausewithgame") == 1 &&
		OGG_Status() == PLAY && cl.attractloop == false)
	{
		Cbuf_AddText("ogg toggle\n");
	}

	/* if this menu is already open (and on top),
	   close it => toggling behaviour */
	if ((m_active.draw == menu->draw) && (m_active.key  == menu->key))
	{
		M_PopMenu();
		return;
	}

	/* if this menu is already present, drop back to
	   that level to avoid stacking menus by hotkeys */
	for (i = 0; i < m_menudepth; i++)
	{
		if ((m_layers[i].draw == menu->draw) && (m_layers[i].key  == menu->key))
		{
			alreadyPresent = 1;
			break;
		}
	}

	/* menu was already opened further down the stack */
	while (alreadyPresent && i <= m_menudepth)
	{
		M_PopMenu(); /* decrements m_menudepth */
	}

	if (m_menudepth >= MAX_MENU_DEPTH)
	{
		Com_Printf("Too many open menus!\n");
		return;
	}

	m_layers[m_menudepth].draw = m_active.draw;
	m_layers[m_menudepth].key  = m_active.key;
	m_menudepth++;

	m_active.draw = menu->draw;
	m_active.key  = menu->key;

	m_entersound = true;

	cls.key_dest = key_menu;
}

extern qboolean japanese_confirm;

int
Key_GetMenuKey(int key)
{
	switch (key)
	{
		case K_KP_UPARROW:
			if (IN_NumpadIsOn() == true) { break; }
		case K_UPARROW:
		case K_DPAD_UP:
			return K_UPARROW;

		case K_TAB:
		case K_KP_DOWNARROW:
			if (IN_NumpadIsOn() == true) { break; }
		case K_DOWNARROW:
		case K_DPAD_DOWN:
			return K_DOWNARROW;

		case K_KP_LEFTARROW:
			if (IN_NumpadIsOn() == true) { break; }
		case K_LEFTARROW:
		case K_DPAD_LEFT:
		case K_SHOULDER_LEFT:
			return K_LEFTARROW;

		case K_KP_RIGHTARROW:
			if (IN_NumpadIsOn() == true) { break; }
		case K_RIGHTARROW:
		case K_DPAD_RIGHT:
		case K_SHOULDER_RIGHT:
			return K_RIGHTARROW;

		case K_MOUSE1:
		case K_MOUSE2:
		case K_MOUSE3:
		case K_MOUSE4:
		case K_MOUSE5:

		case K_KP_ENTER:
		case K_ENTER:
			return K_ENTER;

		case K_KP_DEL:
			if (IN_NumpadIsOn() == true) { break; }
		case K_BACKSPACE:
		case K_DEL:
		case K_BTN_NORTH:
			return K_BACKSPACE;

		case K_KP_INS:
			if (IN_NumpadIsOn() == true) { break; }
		case K_INS:
			return K_INS;

		case K_BTN_SOUTH:
			if (japanese_confirm) return K_ESCAPE;
			else return K_ENTER;

		case K_BTN_EAST:
			if (japanese_confirm) return K_ENTER;
			else return K_ESCAPE;
	}

	return key;
}

static const char *
Default_MenuKey(menuframework_s *m, int key)
{
	const char *sound = NULL;
	int menu_key = Key_GetMenuKey(key);

	if (m)
	{
		menucommon_s *item = Menu_ItemAtCursor(m);

		if (item && item->type == MTYPE_FIELD)
		{
			if (Field_Key((menufield_s *)item, key))
			{
				return NULL;
			}
		}
	}

	switch (menu_key)
	{
	case K_ESCAPE:
		if (m)
		{
			Field_ResetCursor(m);
		}

		M_PopMenu();
		return menu_out_sound;

	case K_UPARROW:
		if (m)
		{
			Field_ResetCursor(m);

			m->cursor--;
			Menu_AdjustCursor(m, -1);
			sound = menu_move_sound;
		}
		break;

	case K_DOWNARROW:
		if (m)
		{
			Field_ResetCursor(m);

			m->cursor++;
			Menu_AdjustCursor(m, 1);
			sound = menu_move_sound;
		}
		break;

	case K_LEFTARROW:
		if (m)
		{
			Menu_SlideItem(m, -1);
			sound = menu_move_sound;
		}
		break;

	case K_RIGHTARROW:
		if (m)
		{
			Menu_SlideItem(m, 1);
			sound = menu_move_sound;
		}
		break;

	case K_ENTER:
		if (m)
		{
			Menu_SelectItem(m);
		}
		sound = menu_move_sound;
		break;
	}

	return sound;
}

/*
 * Draws one solid graphics character cx and cy are in 320*240
 * coordinates, and will be centered on higher res screens.
 */
static void
M_DrawCharacter(int cx, int cy, int num)
{
	float scale = SCR_GetMenuScale();
	Draw_CharScaled(cx + ((int)(viddef.width - 320 * scale) >> 1), cy + ((int)(viddef.height - 240 * scale) >> 1), num, scale);
}

static void
M_Print(int x, int y, char *str)
{
	int cx, cy;
	float scale = SCR_GetMenuScale();

	cx = x;
	cy = y;
	while (*str)
	{
		if (*str == '\n')
		{
			cx = x;
			cy += 8;
		}
		else
		{
			M_DrawCharacter(cx * scale, cy * scale, (*str) + 128);
			cx += 8;
		}
		str++;
	}
}

/* Unsused, left for backward compability */
void
M_DrawPic(int x, int y, char *pic)
{
	float scale = SCR_GetMenuScale();

	Draw_PicScaled((x + ((viddef.width - 320) >> 1)) * scale,
			   (y + ((viddef.height - 240) >> 1)) * scale, pic, scale);
}

/*
 * Draws an animating cursor with the point at
 * x,y. The pic will extend to the left of x,
 * and both above and below y.
 */
static void
M_DrawCursor(int x, int y, int f)
{
	char cursorname[80];
	static qboolean cached;
	float scale = SCR_GetMenuScale();

	if (!cached)
	{
		int i;

		for (i = 0; i < NUM_CURSOR_FRAMES; i++)
		{
			Com_sprintf(cursorname, sizeof(cursorname), "m_cursor%d", i);

			Draw_FindPic(cursorname);
		}

		cached = true;
	}

	Com_sprintf(cursorname, sizeof(cursorname), "m_cursor%d", f);
	Draw_PicScaled(x * scale, y * scale, cursorname, scale);
}

static void
M_DrawTextBox(int x, int y, int width, int lines)
{
	int cx, cy;
	int n;
	float scale = SCR_GetMenuScale();

	/* draw left side */
	cx = x;
	cy = y;
	M_DrawCharacter(cx * scale, cy * scale, 1);

	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter(cx * scale, cy * scale, 4);
	}

	M_DrawCharacter(cx * scale, cy * scale + 8 * scale, 7);

	/* draw middle */
	cx += 8;

	while (width > 0)
	{
		cy = y;
		M_DrawCharacter(cx * scale, cy * scale, 2);

		for (n = 0; n < lines; n++)
		{
			cy += 8;
			M_DrawCharacter(cx * scale, cy * scale, 5);
		}

		M_DrawCharacter(cx * scale, cy *scale + 8 * scale, 8);
		width -= 1;
		cx += 8;
	}

	/* draw right side */
	cy = y;
	M_DrawCharacter(cx * scale, cy * scale, 3);

	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter(cx * scale, cy * scale, 6);
	}

	M_DrawCharacter(cx * scale, cy * scale + 8 * scale, 9);
}

static char *m_popup_string;
static int m_popup_endtime;

static void
M_Popup(void)
{
	int width, lines;
	int n;
	char *str;

	if (!m_popup_string)
	{
		return;
	}

	if (m_popup_endtime && m_popup_endtime < cls.realtime)
	{
		m_popup_string = NULL;
		return;
	}

	if (!R_EndWorldRenderpass())
	{
		return;
	}

	width = lines = n = 0;
	for (str = m_popup_string; *str; str++)
	{
		if (*str == '\n')
		{
			lines++;
			n = 0;
		}
		else
		{
			n++;
			if (n > width)
			{
				width = n;
			}
		}
	}
	if (n)
	{
		lines++;
	}

	if (width)
	{
		int x, y;
		width += 2;

		x = (320 - (width + 2) * 8) / 2;
		y = (240 - (lines + 2) * 8) / 3;

		M_DrawTextBox(x, y, width, lines);
		M_Print(x + 16, y + 8, m_popup_string);
	}
}

/*
 * MAIN MENU
 */

static menuframework_s s_main;
static menubitmap_s s_plaque;
static menubitmap_s s_logo;
static menubitmap_s s_game;
static menubitmap_s s_multiplayer;
static menubitmap_s s_options;
static menubitmap_s s_video;
static menubitmap_s s_quit;

static void
GameFunc(void *unused)
{
	M_Menu_Game_f();
}

static void
MultiplayerFunc(void *unused)
{
	M_Menu_Multiplayer_f();
}

static void
OptionsFunc(void *unused)
{
	M_Menu_Options_f();
}

static void
VideoFunc(void *unused)
{
	M_Menu_Video_f();
}

static void
QuitFunc(void *unused)
{
	M_Menu_Quit_f();
}

static void
InitMainMenu(void)
{
	float scale = SCR_GetMenuScale();
	int i = 0;
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	int widest = -1;

	char * names[] = {
		"m_main_game",
		"m_main_multiplayer",
		"m_main_options",
		"m_main_video",
		"m_main_quit",
		0
	};

	for (i = 0; names[i] != 0; i++)
	{
		Draw_GetPicSize(&w, &h, names[i]);

		if (w > widest)
		{
			widest = w;
		}
	}

	x = (viddef.width / scale - widest + 70) / 2;
	y = (viddef.height / (2 * scale) - 110);

	memset(&s_main, 0, sizeof( menuframework_s ));

	Draw_GetPicSize(&w, &h, "m_main_plaque");

	s_plaque.generic.type = MTYPE_BITMAP;
	s_plaque.generic.flags = QMF_LEFT_JUSTIFY | QMF_INACTIVE;
	s_plaque.generic.x = (x - (m_cursor_width + 5) - w);
	s_plaque.generic.y = y;
	s_plaque.generic.name = "m_main_plaque";
	s_plaque.generic.callback = 0;
	s_plaque.focuspic = 0;

	s_logo.generic.type = MTYPE_BITMAP;
	s_logo.generic.flags = QMF_LEFT_JUSTIFY | QMF_INACTIVE;
	s_logo.generic.x = (x - (m_cursor_width + 5) - w);
	s_logo.generic.y = y + h + 5;
	s_logo.generic.name = "m_main_logo";
	s_logo.generic.callback = 0;
	s_logo.focuspic = 0;

	y += 10;

	s_game.generic.type = MTYPE_BITMAP;
	s_game.generic.flags = QMF_LEFT_JUSTIFY | QMF_HIGHLIGHT_IF_FOCUS;
	s_game.generic.x = x;
	s_game.generic.y = y;
	s_game.generic.name = "m_main_game";
	s_game.generic.callback = GameFunc;
	s_game.focuspic = "m_main_game_sel";

	Draw_GetPicSize(&w, &h, ( char * )s_game.generic.name);
	y += h + 8;

	s_multiplayer.generic.type = MTYPE_BITMAP;
	s_multiplayer.generic.flags = QMF_LEFT_JUSTIFY | QMF_HIGHLIGHT_IF_FOCUS;
	s_multiplayer.generic.x = x;
	s_multiplayer.generic.y = y;
	s_multiplayer.generic.name = "m_main_multiplayer";
	s_multiplayer.generic.callback = MultiplayerFunc;
	s_multiplayer.focuspic = "m_main_multiplayer_sel";

	Draw_GetPicSize(&w, &h, ( char * )s_multiplayer.generic.name);
	y += h + 8;

	s_options.generic.type = MTYPE_BITMAP;
	s_options.generic.flags = QMF_LEFT_JUSTIFY | QMF_HIGHLIGHT_IF_FOCUS;
	s_options.generic.x = x;
	s_options.generic.y = y;
	s_options.generic.name = "m_main_options";
	s_options.generic.callback = OptionsFunc;
	s_options.focuspic = "m_main_options_sel";

	Draw_GetPicSize(&w, &h, ( char * )s_options.generic.name);
	y += h + 8;

	s_video.generic.type = MTYPE_BITMAP;
	s_video.generic.flags = QMF_LEFT_JUSTIFY | QMF_HIGHLIGHT_IF_FOCUS;
	s_video.generic.x = x;
	s_video.generic.y = y;
	s_video.generic.name = "m_main_video";
	s_video.generic.callback = VideoFunc;
	s_video.focuspic = "m_main_video_sel";

	Draw_GetPicSize(&w, &h, ( char * )s_video.generic.name);
	y += h + 8;

	s_quit.generic.type = MTYPE_BITMAP;
	s_quit.generic.flags = QMF_LEFT_JUSTIFY | QMF_HIGHLIGHT_IF_FOCUS;
	s_quit.generic.x = x;
	s_quit.generic.y = y;
	s_quit.generic.name = "m_main_quit";
	s_quit.generic.callback = QuitFunc;
	s_quit.focuspic = "m_main_quit_sel";

	Menu_AddItem(&s_main, (void *)&s_plaque);
	Menu_AddItem(&s_main, (void *)&s_logo);
	Menu_AddItem(&s_main, (void *)&s_game);
	Menu_AddItem(&s_main, (void *)&s_multiplayer);
	Menu_AddItem(&s_main, (void *)&s_options);
	Menu_AddItem(&s_main, (void *)&s_video);
	Menu_AddItem(&s_main, (void *)&s_quit);

	Menu_Center(&s_main);
}


static void
M_Main_Draw(void)
{
	const menucommon_s * item = NULL;
	int x = 0;
	int y = 0;

	item = ( menucommon_s * )s_main.items[s_main.cursor];

	if (item)
	{
		x = item->x;
		y = item->y;
	}

	Menu_Draw(&s_main);
	M_DrawCursor(x - m_cursor_width, y,
		( int )(cls.realtime / 100) % NUM_CURSOR_FRAMES);
}

static const char *
M_Main_Key(int key)
{
	return Default_MenuKey(&s_main, key);
}

void
M_Menu_Main_f(void)
{
	InitMainMenu();

	// force first available item to have focus
	while (s_main.cursor >= 0 && s_main.cursor < s_main.nitems)
	{
		const menucommon_s * item = NULL;

		item = ( menucommon_s * )s_main.items[s_main.cursor];

		if ((item->flags & (QMF_INACTIVE)))
		{
			s_main.cursor++;
		}
		else
		{
			break;
		}
	}

	s_main.draw = M_Main_Draw;
	s_main.key  = M_Main_Key;

	M_PushMenu(&s_main);
}

/*
 * MULTIPLAYER MENU
 */

static menuframework_s s_multiplayer_menu;
static menuaction_s s_join_network_server_action;
static menuaction_s s_start_network_server_action;
static menuaction_s s_player_setup_action;
static menuaction_s s_customize_options_action;

static void
Multiplayer_MenuDraw(void)
{
	M_Banner("m_banner_multiplayer");

	Menu_AdjustCursor(&s_multiplayer_menu, 1);
	Menu_Draw(&s_multiplayer_menu);
}

static void
PlayerSetupFunc(void *unused)
{
	M_Menu_PlayerConfig_f();
}

static void
MultplayerCustomizeControlsFunc(void *unused)
{
	M_Menu_Multiplayer_Keys_f();
}

static void
JoinNetworkServerFunc(void *unused)
{
	M_Menu_JoinServer_f();
}

static void
StartNetworkServerFunc(void *unused)
{
	M_Menu_StartServer_f();
}

static void
Multiplayer_MenuInit(void)
{
	float scale = SCR_GetMenuScale();

	s_multiplayer_menu.x = (int)(viddef.width * 0.50f) - 64 * scale;
	s_multiplayer_menu.nitems = 0;

	s_join_network_server_action.generic.type = MTYPE_ACTION;
	s_join_network_server_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_join_network_server_action.generic.x = 0;
	s_join_network_server_action.generic.y = 0;
	s_join_network_server_action.generic.name = " join network server";
	s_join_network_server_action.generic.callback = JoinNetworkServerFunc;

	s_start_network_server_action.generic.type = MTYPE_ACTION;
	s_start_network_server_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_start_network_server_action.generic.x = 0;
	s_start_network_server_action.generic.y = 10;
	s_start_network_server_action.generic.name = " start network server";
	s_start_network_server_action.generic.callback = StartNetworkServerFunc;

	s_player_setup_action.generic.type = MTYPE_ACTION;
	s_player_setup_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_setup_action.generic.x = 0;
	s_player_setup_action.generic.y = 20;
	s_player_setup_action.generic.name = " player setup";
	s_player_setup_action.generic.callback = PlayerSetupFunc;

	s_customize_options_action.generic.type = MTYPE_ACTION;
	s_customize_options_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_customize_options_action.generic.x = 0;
	s_customize_options_action.generic.y = 30;
	s_customize_options_action.generic.name = " customize controls";
	s_customize_options_action.generic.callback = MultplayerCustomizeControlsFunc;

	Menu_AddItem(&s_multiplayer_menu, (void *)&s_join_network_server_action);
	Menu_AddItem(&s_multiplayer_menu, (void *)&s_start_network_server_action);
	Menu_AddItem(&s_multiplayer_menu, (void *)&s_player_setup_action);
	Menu_AddItem(&s_multiplayer_menu, (void *)&s_customize_options_action);

	Menu_SetStatusBar(&s_multiplayer_menu, NULL);

	Menu_Center(&s_multiplayer_menu);
}

static const char *
Multiplayer_MenuKey(int key)
{
	return Default_MenuKey(&s_multiplayer_menu, key);
}

static void
M_Menu_Multiplayer_f(void)
{
	Multiplayer_MenuInit();
	s_multiplayer_menu.draw = Multiplayer_MenuDraw;
	s_multiplayer_menu.key  = Multiplayer_MenuKey;

	M_PushMenu(&s_multiplayer_menu);
}

/*
 * KEYS MENU
 */

char *bindnames[][2] =
{
	{"+attack", "attack"},
	{"weapnext", "next weapon"},
	{"weapprev", "previous weapon"},
	{"+forward", "walk forward"},
	{"+back", "backpedal"},
	{"+left", "turn left"},
	{"+right", "turn right"},
	{"+speed", "run"},
	{"+moveleft", "step left"},
	{"+moveright", "step right"},
	{"+strafe", "sidestep"},
	{"+lookup", "look up"},
	{"+lookdown", "look down"},
	{"centerview", "center view"},
	{"+mlook", "mouse look"},
	{"+klook", "keyboard look"},
	{"+moveup", "up / jump"},
	{"+movedown", "down / crouch"},
	{"inven", "inventory"},
	{"invuse", "use item"},
	{"invdrop", "drop item"},
	{"invprev", "prev item"},
	{"invnext", "next item"},
	{"cmd help", "help computer"}
};

int keys_cursor;
static int menukeyitem_bind;

static menuframework_s s_keys_menu;
static menuframework_s s_joy_menu;
static menuaction_s s_keys_actions[ARRLEN(bindnames)];

static void
M_UnbindCommand(char *command, int scope)
{
	int j;
	int begin = 0, end = K_LAST;
	switch (scope)
	{
		case KEYS_KEYBOARD_MOUSE:
			end = K_JOY_FIRST_BTN;
			break;
		case KEYS_CONTROLLER:
			begin = K_JOY_FIRST_BTN;
			end = K_JOY_FIRST_BTN_ALT;
			break;
		case KEYS_CONTROLLER_ALT:
			begin = K_JOY_FIRST_BTN_ALT;
	}

	for (j = begin; j < end; j++)
	{
		const char *b;
		b = keybindings[j];

		if (!b)
		{
			continue;
		}

		if (!strcmp(b, command))
		{
			Key_SetBinding(j, "");
		}
	}
}

static void
M_FindKeysForCommand(char *command, int *twokeys, int scope)
{
	int count;
	int j;
	int begin = 0, end = K_LAST;
	switch (scope)
	{
		case KEYS_KEYBOARD_MOUSE:
			end = K_JOY_FIRST_BTN;
			break;
		case KEYS_CONTROLLER:
			begin = K_JOY_FIRST_BTN;
			end = K_JOY_FIRST_BTN_ALT;
			break;
		case KEYS_CONTROLLER_ALT:
			begin = K_JOY_FIRST_BTN_ALT;
	}

	twokeys[0] = twokeys[1] = -1;
	count = 0;

	for (j = begin; j < end; j++)
	{
		const char *b;
		b = keybindings[j];

		if (!b)
		{
			continue;
		}

		if (!strcmp(b, command))
		{
			twokeys[count] = j;
			count++;

			if (count == 2)
			{
				break;
			}
		}
	}
}

static void
KeyCursorDrawFunc(menuframework_s *menu)
{
	float scale = SCR_GetMenuScale();

	if (menukeyitem_bind)
	{
		Draw_CharScaled(menu->x, (menu->y + menu->cursor * 9) * scale, '=', scale);
	}
	else
	{
		Draw_CharScaled(menu->x, (menu->y + menu->cursor * 9) * scale, 12 +
				  ((int)(Sys_Milliseconds() / 250) & 1), scale);
	}
}

static void
DrawKeyBindingFunc(void *self)
{
	int keys[2];
	menuaction_s *a = (menuaction_s *)self;
	float scale = SCR_GetMenuScale();

	M_FindKeysForCommand(bindnames[a->generic.localdata[0]][0], keys, KEYS_KEYBOARD_MOUSE);

	if (keys[0] == -1)
	{
		Menu_DrawString(a->generic.x + a->generic.parent->x + RCOLUMN_OFFSET * scale,
						a->generic.y + a->generic.parent->y, "???");
	}
	else
	{
		int x;
		const char *name;

		name = Key_KeynumToString(keys[0]);

		Menu_DrawString(a->generic.x + a->generic.parent->x + RCOLUMN_OFFSET * scale,
						a->generic.y + a->generic.parent->y, name);

		x = strlen(name) * 8;

		if (keys[1] != -1)
		{
			Menu_DrawString(a->generic.x + a->generic.parent->x + 24 * scale + (x * scale),
							a->generic.y + a->generic.parent->y, "or");
			Menu_DrawString(a->generic.x + a->generic.parent->x + 48 * scale + (x * scale),
							a->generic.y + a->generic.parent->y,
							Key_KeynumToString(keys[1]));
		}
	}
}

static void
KeyBindingFunc(void *self)
{
	menuaction_s *a = (menuaction_s *)self;
	int keys[2];

	M_FindKeysForCommand(bindnames[a->generic.localdata[0]][0], keys, KEYS_KEYBOARD_MOUSE);

	if (keys[1] != -1)
	{
		M_UnbindCommand(bindnames[a->generic.localdata[0]][0], KEYS_KEYBOARD_MOUSE);
	}

	menukeyitem_bind = true;

	Menu_SetStatusBar(&s_keys_menu, "press a key or button for this action");
}

static void
Keys_MenuInit(void)
{
	int i;

	s_keys_menu.x = (int)(viddef.width * 0.50f);
	s_keys_menu.nitems = 0;
	s_keys_menu.cursordraw = KeyCursorDrawFunc;

	for (i = 0; i < ARRLEN(bindnames); i++)
	{
		s_keys_actions[i].generic.type = MTYPE_ACTION;
		s_keys_actions[i].generic.flags = QMF_GRAYED;
		s_keys_actions[i].generic.x = 0;
		s_keys_actions[i].generic.y = (i * 9);
		s_keys_actions[i].generic.ownerdraw = DrawKeyBindingFunc;
		s_keys_actions[i].generic.localdata[0] = i;
		s_keys_actions[i].generic.name = bindnames[s_keys_actions[i].generic.localdata[0]][1];

		Menu_AddItem(&s_keys_menu, (void *)&s_keys_actions[i]);
	}

	Menu_SetStatusBar(&s_keys_menu, "ENTER to change, BACKSPACE to clear");
	Menu_Center(&s_keys_menu);
}

static void
Keys_MenuDraw(void)
{
	Menu_AdjustCursor(&s_keys_menu, 1);
	Menu_Draw(&s_keys_menu);
}

static const char *
Keys_MenuKey(int key)
{
	menuaction_s *item = (menuaction_s *)Menu_ItemAtCursor(&s_keys_menu);

	if (menukeyitem_bind)
	{
		// Any key/button except from the game controller and escape keys
		if ((key != K_ESCAPE) && (key != '`') && (key < K_JOY_FIRST_BTN))
		{
			char cmd[1024];

			Com_sprintf(cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n",
						Key_KeynumToString(key), bindnames[item->generic.localdata[0]][0]);
			Cbuf_InsertText(cmd);
		}

		Menu_SetStatusBar(&s_keys_menu, "ENTER to change, BACKSPACE to clear");
		menukeyitem_bind = false;
		return menu_out_sound;
	}

	key = Key_GetMenuKey(key);
	switch (key)
	{
	case K_ENTER:
		KeyBindingFunc(item);
		return menu_in_sound;
	case K_BACKSPACE: /* delete bindings */
		M_UnbindCommand(bindnames[item->generic.localdata[0]][0], KEYS_KEYBOARD_MOUSE);
		return menu_out_sound;
	default:
		return Default_MenuKey(&s_keys_menu, key);
	}
}

static void
M_Menu_Keys_f(void)
{
	Keys_MenuInit();
	s_keys_menu.draw = Keys_MenuDraw;
	s_keys_menu.key  = Keys_MenuKey;

	M_PushMenu(&s_keys_menu);
}

/*
 * MULTIPLAYER KEYS MENU
 */

char *multiplayer_key_bindnames[][2] =
{
	{"score", "score"},
	{"messagemode", "chat"},
	{"messagemode2", "team chat"},
	{"wave 1", "wave 1"},
	{"wave 2", "wave 2"},
	{"wave 3", "wave 3"},
	{"wave 4", "wave 4"},
};

static menuframework_s s_multiplayer_keys_menu;
static menuaction_s s_multiplayer_keys_actions[ARRLEN(multiplayer_key_bindnames)];

static void
MultiplayerDrawKeyBindingFunc(void *self)
{
	int keys[2];
	menuaction_s *a = (menuaction_s *)self;
	float scale = SCR_GetMenuScale();

	M_FindKeysForCommand(multiplayer_key_bindnames[a->generic.localdata[0]][0], keys, KEYS_ALL);

	if (keys[0] == -1)
	{
		Menu_DrawString(a->generic.x + a->generic.parent->x + RCOLUMN_OFFSET * scale,
						a->generic.y + a->generic.parent->y, "???");
	}
	else
	{
		int x;
		const char *name;

		name = Key_KeynumToString(keys[0]);

		Menu_DrawString(a->generic.x + a->generic.parent->x + RCOLUMN_OFFSET * scale,
						a->generic.y + a->generic.parent->y, name);

		x = strlen(name) * 8;

		if (keys[1] != -1)
		{
			Menu_DrawString(a->generic.x + a->generic.parent->x + 24 * scale + (x * scale),
							a->generic.y + a->generic.parent->y, "or");
			Menu_DrawString(a->generic.x + a->generic.parent->x + 48 * scale + (x * scale),
							a->generic.y + a->generic.parent->y,
							Key_KeynumToString(keys[1]));
		}
	}
}

static void
MultiplayerKeyBindingFunc(void *self)
{
	menuaction_s *a = (menuaction_s *)self;
	int keys[2];

	M_FindKeysForCommand(multiplayer_key_bindnames[a->generic.localdata[0]][0], keys, KEYS_ALL);

	if (keys[1] != -1)
	{
		M_UnbindCommand(multiplayer_key_bindnames[a->generic.localdata[0]][0], KEYS_ALL);
	}

	menukeyitem_bind = true;

	Menu_SetStatusBar(&s_multiplayer_keys_menu, "press a key or button for this action");
}

static void
MultiplayerKeys_MenuInit(void)
{
	int i;

	s_multiplayer_keys_menu.x = (int)(viddef.width * 0.50f);
	s_multiplayer_keys_menu.nitems = 0;
	s_multiplayer_keys_menu.cursordraw = KeyCursorDrawFunc;

	for (i = 0; i < ARRLEN(multiplayer_key_bindnames); i++)
	{
		s_multiplayer_keys_actions[i].generic.type = MTYPE_ACTION;
		s_multiplayer_keys_actions[i].generic.flags = QMF_GRAYED;
		s_multiplayer_keys_actions[i].generic.x = 0;
		s_multiplayer_keys_actions[i].generic.y = (i * 9);
		s_multiplayer_keys_actions[i].generic.ownerdraw = MultiplayerDrawKeyBindingFunc;
		s_multiplayer_keys_actions[i].generic.localdata[0] = i;
		s_multiplayer_keys_actions[i].generic.name = multiplayer_key_bindnames[s_multiplayer_keys_actions[i].generic.localdata[0]][1];

		Menu_AddItem(&s_multiplayer_keys_menu, (void *)&s_multiplayer_keys_actions[i]);
	}

	Menu_SetStatusBar(&s_multiplayer_keys_menu, "ENTER to change, BACKSPACE to clear");
	Menu_Center(&s_multiplayer_keys_menu);
}

static void
MultiplayerKeys_MenuDraw(void)
{
	Menu_AdjustCursor(&s_multiplayer_keys_menu, 1);
	Menu_Draw(&s_multiplayer_keys_menu);
}

static const char *
MultiplayerKeys_MenuKey(int key)
{
	menuaction_s *item = (menuaction_s *)Menu_ItemAtCursor(&s_multiplayer_keys_menu);

	if (menukeyitem_bind)
	{
		// Any key/button but the escape ones
		if ((key != K_ESCAPE) && (key != '`'))
		{
			char cmd[1024];

			Com_sprintf(cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n",
						Key_KeynumToString(key), multiplayer_key_bindnames[item->generic.localdata[0]][0]);
			Cbuf_InsertText(cmd);
		}

		Menu_SetStatusBar(&s_multiplayer_keys_menu, "ENTER to change, BACKSPACE to clear");
		menukeyitem_bind = false;
		return menu_out_sound;
	}

	key = Key_GetMenuKey(key);
	switch (key)
	{
	case K_ENTER:
		MultiplayerKeyBindingFunc(item);
		return menu_in_sound;
	case K_BACKSPACE: /* delete bindings */
		M_UnbindCommand(multiplayer_key_bindnames[item->generic.localdata[0]][0], KEYS_ALL);
		return menu_out_sound;
	default:
		return Default_MenuKey(&s_multiplayer_keys_menu, key);
	}
}

static void
M_Menu_Multiplayer_Keys_f(void)
{
	MultiplayerKeys_MenuInit();
	s_multiplayer_keys_menu.draw = MultiplayerKeys_MenuDraw;
	s_multiplayer_keys_menu.key  = MultiplayerKeys_MenuKey;

	M_PushMenu(&s_multiplayer_keys_menu);
}

/*
 * GAME CONTROLLER ( GAMEPAD / JOYSTICK ) BUTTONS MENU
 */

static void
GamepadMenu_StatusPrompt(menuframework_s *m)
{
	static char m_gamepadbind_statusbar[64];
	int btn_confirm, btn_cancel;

	if (japanese_confirm)
	{
		btn_confirm = K_BTN_EAST;
		btn_cancel = K_BTN_SOUTH;
	}
	else
	{
		btn_confirm = K_BTN_SOUTH;
		btn_cancel = K_BTN_EAST;
	}

	snprintf(m_gamepadbind_statusbar, 64, "%s assigns, %s clears, %s exits",
		Key_KeynumToString_Joy(btn_confirm), Key_KeynumToString_Joy(K_BTN_NORTH),
		Key_KeynumToString_Joy(btn_cancel));

	Menu_SetStatusBar(m, m_gamepadbind_statusbar);
}

char *controller_bindnames[][2] =
{
	{"+attack", "attack"},
	{"+moveup", "up / jump"},
	{"+movedown", "down / crouch"},
	{"weapnext", "next weapon"},
	{"weapprev", "previous weapon"},
	{"cycleweap weapon_plasmabeam weapon_boomer weapon_chaingun weapon_etf_rifle"
	 " weapon_machinegun weapon_blaster", "long range: quickswitch 1"},
	{"cycleweap weapon_supershotgun weapon_shotgun weapon_chainfist",
	 "close range: quickswitch 2"},
	{"cycleweap weapon_phalanx weapon_rocketlauncher weapon_proxlauncher"
	 " weapon_grenadelauncher ammo_grenades", "explosives: quickswitch 3"},
	{"cycleweap weapon_bfg weapon_disintegrator weapon_railgun weapon_hyperblaster"
	 " ammo_tesla ammo_trap", "special: quickswitch 4"},
	{"prefweap weapon_railgun weapon_plasmabeam weapon_boomer weapon_hyperblaster weapon_chaingun"
	 " weapon_supershotgun weapon_etf_rifle weapon_machinegun weapon_shotgun weapon_blaster",
	 "best safe weapon"},
	{"prefweap weapon_bfg weapon_disintegrator weapon_phalanx weapon_railgun weapon_rocketlauncher"
	 " weapon_plasmabeam weapon_boomer weapon_hyperblaster weapon_grenadelauncher weapon_chaingun"
	 " weapon_proxlauncher ammo_grenades weapon_supershotgun", "best unsafe weapon"},
	{"centerview", "center view"},
	{"inven", "inventory"},
	{"invuse", "use item"},
	{"invdrop", "drop item"},
	{"invprev", "prev item"},
	{"invnext", "next item"},
	{"cmd help", "help computer"},
	{"+gyroaction", "gyro off / on"},
	{"+joyaltselector", "alt buttons modifier"}
};

static menuframework_s s_controller_buttons_menu;
static menuaction_s s_controller_buttons_actions[ARRLEN(controller_bindnames)];

static void
DrawControllerButtonBindingFunc(void *self)
{
	int keys[2];
	menuaction_s *a = (menuaction_s *)self;
	float scale = SCR_GetMenuScale();

	M_FindKeysForCommand(controller_bindnames[a->generic.localdata[0]][0], keys, KEYS_CONTROLLER);

	if (keys[0] == -1)
	{
		Menu_DrawString(a->generic.x + a->generic.parent->x + RCOLUMN_OFFSET * scale,
				a->generic.y + a->generic.parent->y, "???");
	}
	else
	{
		int x;
		const char *name;

		name = Key_KeynumToString_Joy(keys[0]);

		Menu_DrawString(a->generic.x + a->generic.parent->x + RCOLUMN_OFFSET * scale,
			a->generic.y + a->generic.parent->y, name);

		x = strlen(name) * 8;

		if (keys[1] != -1)
		{
			Menu_DrawString(a->generic.x + a->generic.parent->x + 24 * scale + (x * scale),
					a->generic.y + a->generic.parent->y, "or");
			Menu_DrawString(a->generic.x + a->generic.parent->x + 48 * scale + (x * scale),
					a->generic.y + a->generic.parent->y,
					Key_KeynumToString_Joy(keys[1]));
		}
	}
}

static void
ControllerButtonBindingFunc(void *self)
{
	menuaction_s *a = (menuaction_s *)self;
	int keys[2];

	M_FindKeysForCommand(controller_bindnames[a->generic.localdata[0]][0], keys, KEYS_CONTROLLER);

	if (keys[1] != -1)
	{
		M_UnbindCommand(controller_bindnames[a->generic.localdata[0]][0], KEYS_CONTROLLER);
	}

	menukeyitem_bind = true;

	Menu_SetStatusBar(&s_controller_buttons_menu, "press a button for this action");
}

static void
ControllerButtons_MenuInit(void)
{
	int i;

	s_controller_buttons_menu.x = (int)(viddef.width * 0.50f);
	s_controller_buttons_menu.nitems = 0;
	s_controller_buttons_menu.cursordraw = KeyCursorDrawFunc;

	for (i = 0; i < ARRLEN(controller_bindnames); i++)
	{
		s_controller_buttons_actions[i].generic.type = MTYPE_ACTION;
		s_controller_buttons_actions[i].generic.flags = QMF_GRAYED;
		s_controller_buttons_actions[i].generic.x = 0;
		s_controller_buttons_actions[i].generic.y = (i * 9);
		s_controller_buttons_actions[i].generic.ownerdraw = DrawControllerButtonBindingFunc;
		s_controller_buttons_actions[i].generic.localdata[0] = i;
		s_controller_buttons_actions[i].generic.name = controller_bindnames[s_controller_buttons_actions[i].generic.localdata[0]][1];

		Menu_AddItem(&s_controller_buttons_menu, (void *)&s_controller_buttons_actions[i]);
	}

	GamepadMenu_StatusPrompt(&s_controller_buttons_menu);
	Menu_Center(&s_controller_buttons_menu);
}

static void
ControllerButtons_MenuDraw(void)
{
	Menu_AdjustCursor(&s_controller_buttons_menu, 1);
	Menu_Draw(&s_controller_buttons_menu);
}

static const char *
ControllerButtons_MenuKey(int key)
{
	menuaction_s *item = (menuaction_s *)Menu_ItemAtCursor(&s_controller_buttons_menu);

	if (menukeyitem_bind)
	{
		// Only controller buttons allowed
		if (key >= K_JOY_FIRST_BTN)
		{
			char cmd[1024];

			Com_sprintf(cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n",
					Key_KeynumToString(key), controller_bindnames[item->generic.localdata[0]][0]);
			Cbuf_InsertText(cmd);
		}

		GamepadMenu_StatusPrompt(&s_controller_buttons_menu);
		menukeyitem_bind = false;
		return menu_out_sound;
	}

	key = Key_GetMenuKey(key);
	switch (key)
	{
		case K_ENTER:
			ControllerButtonBindingFunc(item);
			return menu_in_sound;
		case K_BACKSPACE:
			M_UnbindCommand(controller_bindnames[item->generic.localdata[0]][0], KEYS_CONTROLLER);
			return menu_out_sound;
		default:
			return Default_MenuKey(&s_controller_buttons_menu, key);
	}
}

static void
M_Menu_ControllerButtons_f(void)
{
	ControllerButtons_MenuInit();
	s_controller_buttons_menu.draw = ControllerButtons_MenuDraw;
	s_controller_buttons_menu.key  = ControllerButtons_MenuKey;

	M_PushMenu(&s_controller_buttons_menu);
}

/*
 * GAME CONTROLLER ALTERNATE BUTTONS MENU
 */

char *controller_alt_bindnames[][2] =
{
	{"weapnext", "next weapon"},
	{"weapprev", "previous weapon"},
	{"cycleweap weapon_plasmabeam weapon_boomer weapon_chaingun weapon_etf_rifle"
	 " weapon_machinegun weapon_blaster", "long range: quickswitch 1"},
	{"cycleweap weapon_supershotgun weapon_shotgun weapon_chainfist",
	 "close range: quickswitch 2"},
	{"cycleweap weapon_phalanx weapon_rocketlauncher weapon_proxlauncher"
	 " weapon_grenadelauncher ammo_grenades", "explosives: quickswitch 3"},
	{"cycleweap weapon_bfg weapon_disintegrator weapon_railgun weapon_hyperblaster"
	 " ammo_tesla ammo_trap", "special: quickswitch 4"},
	{"prefweap weapon_railgun weapon_plasmabeam weapon_boomer weapon_hyperblaster weapon_chaingun"
	 " weapon_supershotgun weapon_etf_rifle weapon_machinegun weapon_shotgun weapon_blaster",
	 "best safe weapon"},
	{"prefweap weapon_bfg weapon_disintegrator weapon_phalanx weapon_railgun weapon_rocketlauncher"
	 " weapon_plasmabeam weapon_boomer weapon_hyperblaster weapon_grenadelauncher weapon_chaingun"
	 " weapon_proxlauncher ammo_grenades weapon_supershotgun", "best unsafe weapon"},
	{"centerview", "center view"},
	{"inven", "inventory"},
	{"invuse", "use item"},
	{"invdrop", "drop item"},
	{"invprev", "prev item"},
	{"invnext", "next item"},
	{"use invulnerability", "use invulnerability"},
	{"use rebreather", "use rebreather"},
	{"use environment suit", "use environment suit"},
	{"use power shield", "use power shield"},
	{"use quad damage", "use quad damage"},
	{"cmd help", "help computer"}
};

static menuframework_s s_controller_alt_buttons_menu;
static menuaction_s s_controller_alt_buttons_actions[ARRLEN(controller_alt_bindnames)];

static void
DrawControllerAltButtonBindingFunc(void *self)
{
	int keys[2];
	menuaction_s *a = (menuaction_s *)self;
	float scale = SCR_GetMenuScale();

	M_FindKeysForCommand(controller_alt_bindnames[a->generic.localdata[0]][0], keys, KEYS_CONTROLLER_ALT);

	if (keys[0] == -1)
	{
		Menu_DrawString(a->generic.x + a->generic.parent->x + RCOLUMN_OFFSET * scale,
				a->generic.y + a->generic.parent->y, "???");
	}
	else
	{
		size_t x;
		const char *name;

		name = Key_KeynumToString_Joy(keys[0]);

		Menu_DrawString(a->generic.x + a->generic.parent->x + RCOLUMN_OFFSET * scale,
				a->generic.y + a->generic.parent->y, name);

		x = strlen(name) * 8;

		if (keys[1] != -1)
		{
			Menu_DrawString(a->generic.x + a->generic.parent->x + 24 * scale + (x * scale),
					a->generic.y + a->generic.parent->y, "or");
			Menu_DrawString(a->generic.x + a->generic.parent->x + 48 * scale + (x * scale),
					a->generic.y + a->generic.parent->y,
					Key_KeynumToString_Joy(keys[1]));
		}
	}
}

static void
ControllerAltButtonBindingFunc(void *self)
{
	menuaction_s *a = (menuaction_s *)self;
	int keys[2];

	M_FindKeysForCommand(controller_alt_bindnames[a->generic.localdata[0]][0], keys, KEYS_CONTROLLER_ALT);

	if (keys[1] != -1)
	{
		M_UnbindCommand(controller_alt_bindnames[a->generic.localdata[0]][0], KEYS_CONTROLLER_ALT);
	}

	menukeyitem_bind = true;

	Menu_SetStatusBar(&s_controller_alt_buttons_menu, "press a button for this action");
}

static void
ControllerAltButtons_MenuInit(void)
{
	int i;

	s_controller_alt_buttons_menu.x = (int)(viddef.width * 0.50f);
	s_controller_alt_buttons_menu.nitems = 0;
	s_controller_alt_buttons_menu.cursordraw = KeyCursorDrawFunc;

	for (i = 0; i < ARRLEN(controller_alt_bindnames); i++)
	{
		s_controller_alt_buttons_actions[i].generic.type = MTYPE_ACTION;
		s_controller_alt_buttons_actions[i].generic.flags = QMF_GRAYED;
		s_controller_alt_buttons_actions[i].generic.x = 0;
		s_controller_alt_buttons_actions[i].generic.y = (i * 9);
		s_controller_alt_buttons_actions[i].generic.ownerdraw = DrawControllerAltButtonBindingFunc;
		s_controller_alt_buttons_actions[i].generic.localdata[0] = i;
		s_controller_alt_buttons_actions[i].generic.name = controller_alt_bindnames[s_controller_alt_buttons_actions[i].generic.localdata[0]][1];

		Menu_AddItem(&s_controller_alt_buttons_menu, (void *)&s_controller_alt_buttons_actions[i]);
	}

	GamepadMenu_StatusPrompt(&s_controller_alt_buttons_menu);
	Menu_Center(&s_controller_alt_buttons_menu);
}

static void
ControllerAltButtons_MenuDraw(void)
{
	Menu_AdjustCursor(&s_controller_alt_buttons_menu, 1);
	Menu_Draw(&s_controller_alt_buttons_menu);
}

static const char *
ControllerAltButtons_MenuKey(int key)
{
	menuaction_s *item = (menuaction_s *)Menu_ItemAtCursor(&s_controller_alt_buttons_menu);

	if (menukeyitem_bind)
	{
		// Only controller buttons allowed, different from the alt buttons modifier
		if (key >= K_JOY_FIRST_BTN && (keybindings[key] == NULL || strcmp(keybindings[key], "+joyaltselector") != 0))
		{
			char cmd[1024];
			key = key + (K_JOY_FIRST_BTN_ALT - K_JOY_FIRST_BTN);   // change input to its ALT mode

			Com_sprintf(cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n",
					Key_KeynumToString(key), controller_alt_bindnames[item->generic.localdata[0]][0]);
			Cbuf_InsertText(cmd);
		}

		GamepadMenu_StatusPrompt(&s_controller_alt_buttons_menu);
		menukeyitem_bind = false;
		return menu_out_sound;
	}

	key = Key_GetMenuKey(key);
	switch (key)
	{
		case K_ENTER:
			ControllerAltButtonBindingFunc(item);
			return menu_in_sound;
		case K_BACKSPACE:
			M_UnbindCommand(controller_alt_bindnames[item->generic.localdata[0]][0], KEYS_CONTROLLER_ALT);
			return menu_out_sound;
		default:
			return Default_MenuKey(&s_controller_alt_buttons_menu, key);
	}
}

static void
M_Menu_ControllerAltButtons_f(void)
{
	ControllerAltButtons_MenuInit();
	s_controller_alt_buttons_menu.draw = ControllerAltButtons_MenuDraw;
	s_controller_alt_buttons_menu.key  = ControllerAltButtons_MenuKey;

	M_PushMenu(&s_controller_alt_buttons_menu);
}

/*
 * STICKS CONFIGURATION MENU
 */

static menuframework_s s_sticks_config_menu;

static menulist_s s_stk_layout_box;
static menuseparator_s s_stk_title_text[3];
static menuslider_s s_stk_expo_slider[2];
static menuslider_s s_stk_deadzone_slider[4];
static menuslider_s s_stk_threshold_slider;

extern qboolean show_gyro;

static void
StickLayoutFunc(void *unused)
{
	Cvar_SetValue("joy_layout", (int)s_stk_layout_box.curvalue);
}

static void
Stick_MenuInit(void)
{
	static const char *stick_layouts[] =
	{
		"default",
		"southpaw",
		"legacy",
		"legacy southpaw",
		0
	};

	static const char *stick_layouts_fs[] =
	{
		"default",
		"southpaw",
		"legacy",
		"legacy southpaw",
		"flick stick",
		"flick stick spaw",
		0
	};

	unsigned short int y = 0, i;
	float scale = SCR_GetMenuScale();

	s_sticks_config_menu.x = (int)(viddef.width * 0.50f);
	s_sticks_config_menu.nitems = 0;

	s_stk_layout_box.generic.type = MTYPE_SPINCONTROL;
	s_stk_layout_box.generic.x = 0;
	s_stk_layout_box.generic.y = y;
	s_stk_layout_box.generic.name = "layout";
	s_stk_layout_box.generic.callback = StickLayoutFunc;
	if (show_gyro || joy_layout->value > 3)
	{
		s_stk_layout_box.itemnames = stick_layouts_fs;
		s_stk_layout_box.curvalue = ClampCvar(0, 5, joy_layout->value);
	}
	else
	{
		s_stk_layout_box.itemnames = stick_layouts;
		s_stk_layout_box.curvalue = ClampCvar(0, 3, joy_layout->value);
	}

	s_stk_title_text[0].generic.name = "left stick";
	s_stk_title_text[0].generic.y = (y += 22);

	s_stk_expo_slider[0].generic.name = "expo";
	s_stk_expo_slider[0].generic.y = (y += 14);
	s_stk_expo_slider[0].cvar = "joy_left_expo";

	s_stk_deadzone_slider[0].generic.name = "snap to axis";
	s_stk_deadzone_slider[0].generic.y = (y += 10);
	s_stk_deadzone_slider[0].cvar = "joy_left_snapaxis";

	s_stk_deadzone_slider[1].generic.name = "deadzone";
	s_stk_deadzone_slider[1].generic.y = (y += 10);
	s_stk_deadzone_slider[1].cvar = "joy_left_deadzone";

	s_stk_title_text[1].generic.name = "right stick";
	s_stk_title_text[1].generic.y = (y += 22);

	s_stk_expo_slider[1].generic.name = "expo";
	s_stk_expo_slider[1].generic.y = (y += 14);
	s_stk_expo_slider[1].cvar = "joy_right_expo";

	s_stk_deadzone_slider[2].generic.name = "snap to axis";
	s_stk_deadzone_slider[2].generic.y = (y += 10);
	s_stk_deadzone_slider[2].cvar = "joy_right_snapaxis";

	s_stk_deadzone_slider[3].generic.name = "deadzone";
	s_stk_deadzone_slider[3].generic.y = (y += 10);
	s_stk_deadzone_slider[3].cvar = "joy_right_deadzone";

	for (i = 0; i < 2; i++)
	{
		s_stk_title_text[i].generic.type = MTYPE_SEPARATOR;
		s_stk_title_text[i].generic.x = 48 * scale;

		s_stk_expo_slider[i].generic.type = MTYPE_SLIDER;
		s_stk_expo_slider[i].generic.x = 0;
		s_stk_expo_slider[i].minvalue = 1;
		s_stk_expo_slider[i].maxvalue = 5;
	}

	for (i = 0; i < 4; i++)
	{
		s_stk_deadzone_slider[i].generic.type = MTYPE_SLIDER;
		s_stk_deadzone_slider[i].generic.x = 0;
		s_stk_deadzone_slider[i].minvalue = 0.0f;
		s_stk_deadzone_slider[i].maxvalue = 0.50f;
		s_stk_deadzone_slider[i].slidestep = 0.01f;
		s_stk_deadzone_slider[i].printformat = "%.2f";
	}

	s_stk_title_text[2].generic.type = MTYPE_SEPARATOR;
	s_stk_title_text[2].generic.x = 48 * scale;
	s_stk_title_text[2].generic.y = (y += 22);
	s_stk_title_text[2].generic.name = "both sticks";

	s_stk_threshold_slider.generic.name = "outer thresh";
	s_stk_threshold_slider.generic.x = 0;
	s_stk_threshold_slider.generic.y = (y += 14);
	s_stk_threshold_slider.cvar = "joy_outer_threshold";
	s_stk_threshold_slider.minvalue = 0.0f;
	s_stk_threshold_slider.maxvalue = 0.30f;
	s_stk_threshold_slider.slidestep = 0.01f;
	s_stk_threshold_slider.printformat = "%.2f";

	Menu_AddItem(&s_sticks_config_menu, (void *)&s_stk_layout_box);
	Menu_AddItem(&s_sticks_config_menu, (void *)&s_stk_title_text[0]);
	Menu_AddItem(&s_sticks_config_menu, (void *)&s_stk_expo_slider[0]);
	Menu_AddItem(&s_sticks_config_menu, (void *)&s_stk_deadzone_slider[0]);
	Menu_AddItem(&s_sticks_config_menu, (void *)&s_stk_deadzone_slider[1]);
	Menu_AddItem(&s_sticks_config_menu, (void *)&s_stk_title_text[1]);
	Menu_AddItem(&s_sticks_config_menu, (void *)&s_stk_expo_slider[1]);
	Menu_AddItem(&s_sticks_config_menu, (void *)&s_stk_deadzone_slider[2]);
	Menu_AddItem(&s_sticks_config_menu, (void *)&s_stk_deadzone_slider[3]);
	Menu_AddItem(&s_sticks_config_menu, (void *)&s_stk_title_text[2]);
	Menu_AddItem(&s_sticks_config_menu, (void *)&s_stk_threshold_slider);

	Menu_Center(&s_sticks_config_menu);
}

static void
Stick_MenuDraw(void)
{
	Menu_AdjustCursor(&s_sticks_config_menu, 1);
	Menu_Draw(&s_sticks_config_menu);
}

static const char *
Stick_MenuKey(int key)
{
	return Default_MenuKey(&s_sticks_config_menu, key);
}

static void
M_Menu_Stick_f(void)
{
	Stick_MenuInit();
	s_sticks_config_menu.draw = Stick_MenuDraw;
	s_sticks_config_menu.key  = Stick_MenuKey;

	M_PushMenu(&s_sticks_config_menu);
}

/*
 * GYRO OPTIONS MENU
 */

static menuframework_s s_gyro_menu;

static menulist_s s_gyro_mode_box;
static menulist_s s_gyro_space_box;
static menulist_s s_gyro_local_roll_box;
static menuslider_s s_gyro_yawsensitivity_slider;
static menuslider_s s_gyro_pitchsensitivity_slider;
static menuslider_s s_gyro_tightening_slider;
static menuslider_s s_gyro_smoothing_slider;
static menulist_s s_gyro_acceleration_box;
static menuslider_s s_gyro_accel_mult_slider;
static menuslider_s s_gyro_accel_lower_thresh_slider;
static menuslider_s s_gyro_accel_upper_thresh_slider;
static menuseparator_s s_calibrating_text[2];
static menuaction_s s_calibrate_gyro;

extern void StartCalibration(void);
extern qboolean IsCalibrationZero(void);

static void M_Menu_Joy_f(void);
static void M_Menu_Gyro_f(void);

static void
CalibrateGyroFunc(void *unused)
{
	if (!show_gyro)
	{
		return;
	}

	m_popup_string = "Calibrating, please wait...";
	m_popup_endtime = cls.realtime + 4650;
	M_Popup();
	R_EndFrame();
	StartCalibration();
}

void
CalibrationFinishedCallback(void)
{
	Menu_SetStatusBar(&s_gyro_menu, NULL);
	m_popup_string = "Calibration complete!";
	m_popup_endtime = cls.realtime + 1900;
	M_Popup();
	R_EndFrame();
}

static void
GyroModeFunc(void *unused)
{
	Cvar_SetValue("gyro_mode", (int)s_gyro_mode_box.curvalue);
}

static void
GyroSpaceFunc(void *unused)
{
	Cvar_SetValue("gyro_space", s_gyro_space_box.curvalue);

	// Force the menu to refresh.
	M_PopMenuSilent();
	M_Menu_Gyro_f();
}

static void
GyroLocalRollFunc(void *unused)
{
	Cvar_SetValue("gyro_local_roll", s_gyro_local_roll_box.curvalue);
}

static void
GyroAccelerationFunc(void *unused)
{
	Cvar_SetValue("gyro_acceleration", s_gyro_acceleration_box.curvalue);

	// Force the menu to refresh.
	M_PopMenuSilent();
	M_Menu_Gyro_f();
}

static void
GyroAcceThreshFunc(void *unused)
{
	// Make sure the accel upper threshold is always greater than or equal to
	// the accel lower threshold.
	const float min_value = Cvar_VariableValue("gyro_accel_lower_thresh");
	float value = Cvar_VariableValue("gyro_accel_upper_thresh");
	value = ClampCvar(min_value, GYRO_MAX_ACCEL_THRESH, value);
	Cvar_SetValue("gyro_accel_upper_thresh", value);
}

static void
Gyro_MenuInit(void)
{
	static const char *gyro_modes[] =
	{
		"always off",
		"off, button enables",
		"on, button disables",
		"always on",
		0
	};

	static const char *gyro_space_choices[] =
	{
		"local",
		"player",
		"world",
		0
	};

	static const char *gyro_local_roll_choices[] =
	{
		"off",
		"on",
		"on, invert",
		0
	};

	static const char *onoff_names[] =
	{
		"off",
		"on",
		0
	};

	unsigned short int y = 0;
	float scale = SCR_GetMenuScale();

	s_gyro_menu.x = (int)(viddef.width * 0.50f);
	s_gyro_menu.nitems = 0;

	s_gyro_mode_box.generic.type = MTYPE_SPINCONTROL;
	s_gyro_mode_box.generic.x = 0;
	s_gyro_mode_box.generic.y = y;
	s_gyro_mode_box.generic.name = "gyro mode";
	s_gyro_mode_box.generic.callback = GyroModeFunc;
	s_gyro_mode_box.itemnames = gyro_modes;
	s_gyro_mode_box.curvalue = ClampCvar(0, 3, gyro_mode->value);

	s_gyro_space_box.generic.type = MTYPE_SPINCONTROL;
	s_gyro_space_box.generic.x = 0;
	s_gyro_space_box.generic.y = (y += 10);
	s_gyro_space_box.generic.name = "gyro space";
	s_gyro_space_box.generic.callback = GyroSpaceFunc;
	s_gyro_space_box.itemnames = gyro_space_choices;
	s_gyro_space_box.curvalue =
		ClampCvar(GYRO_SPACE_LOCAL, GYRO_SPACE_WORLD,
				  Cvar_VariableValue("gyro_space"));

	if (s_gyro_space_box.curvalue == GYRO_SPACE_LOCAL)
	{
		s_gyro_local_roll_box.generic.type = MTYPE_SPINCONTROL;
		s_gyro_local_roll_box.generic.x = 0;
		s_gyro_local_roll_box.generic.y = (y += 10);
		s_gyro_local_roll_box.generic.name = "local roll";
		s_gyro_local_roll_box.generic.callback = GyroLocalRollFunc;
		s_gyro_local_roll_box.itemnames = gyro_local_roll_choices;
		s_gyro_local_roll_box.curvalue =
			ClampCvar(GYRO_LOCAL_ROLL_OFF, GYRO_LOCAL_ROLL_INVERT,
					  Cvar_VariableValue("gyro_local_roll"));
	}

	s_gyro_yawsensitivity_slider.generic.type = MTYPE_SLIDER;
	s_gyro_yawsensitivity_slider.generic.x = 0;
	s_gyro_yawsensitivity_slider.generic.y = (y += 20);
	s_gyro_yawsensitivity_slider.generic.name = "yaw sensitivity";
	s_gyro_yawsensitivity_slider.cvar = "gyro_yawsensitivity";
	s_gyro_yawsensitivity_slider.minvalue = GYRO_MIN_SENS;
	s_gyro_yawsensitivity_slider.maxvalue = GYRO_MAX_SENS;
	s_gyro_yawsensitivity_slider.slidestep = GYRO_STEP_SENS;
	s_gyro_yawsensitivity_slider.abs = true;

	s_gyro_pitchsensitivity_slider.generic.type = MTYPE_SLIDER;
	s_gyro_pitchsensitivity_slider.generic.x = 0;
	s_gyro_pitchsensitivity_slider.generic.y = (y += 10);
	s_gyro_pitchsensitivity_slider.generic.name = "pitch sensitivity";
	s_gyro_pitchsensitivity_slider.cvar = "gyro_pitchsensitivity";
	s_gyro_pitchsensitivity_slider.minvalue = GYRO_MIN_SENS;
	s_gyro_pitchsensitivity_slider.maxvalue = GYRO_MAX_SENS;
	s_gyro_pitchsensitivity_slider.slidestep = GYRO_STEP_SENS;
	s_gyro_pitchsensitivity_slider.abs = true;

	s_gyro_tightening_slider.generic.type = MTYPE_SLIDER;
	s_gyro_tightening_slider.generic.x = 0;
	s_gyro_tightening_slider.generic.y = (y += 20);
	s_gyro_tightening_slider.generic.name = "tightening";
	s_gyro_tightening_slider.cvar = "gyro_tightening";
	s_gyro_tightening_slider.minvalue = GYRO_MIN_TIGHT_THRESH;
	s_gyro_tightening_slider.maxvalue = GYRO_MAX_TIGHT_THRESH;
	s_gyro_tightening_slider.slidestep = GYRO_STEP_TIGHT_THRESH;

	s_gyro_smoothing_slider.generic.type = MTYPE_SLIDER;
	s_gyro_smoothing_slider.generic.x = 0;
	s_gyro_smoothing_slider.generic.y = (y += 10);
	s_gyro_smoothing_slider.generic.name = "smoothing";
	s_gyro_smoothing_slider.cvar = "gyro_smoothing";
	s_gyro_smoothing_slider.minvalue = GYRO_MIN_SMOOTH_THRESH;
	s_gyro_smoothing_slider.maxvalue = GYRO_MAX_SMOOTH_THRESH;
	s_gyro_smoothing_slider.slidestep = GYRO_STEP_SMOOTH_THRESH;

	s_gyro_acceleration_box.generic.type = MTYPE_SPINCONTROL;
	s_gyro_acceleration_box.generic.x = 0;
	s_gyro_acceleration_box.generic.y = (y += 20);
	s_gyro_acceleration_box.generic.name = "acceleration";
	s_gyro_acceleration_box.generic.callback = GyroAccelerationFunc;
	s_gyro_acceleration_box.itemnames = onoff_names;
	s_gyro_acceleration_box.curvalue =
		(Cvar_VariableValue("gyro_acceleration") > 0);

	if (s_gyro_acceleration_box.curvalue)
	{
		s_gyro_accel_mult_slider.generic.type = MTYPE_SLIDER;
		s_gyro_accel_mult_slider.generic.x = 0;
		s_gyro_accel_mult_slider.generic.y = (y += 10);
		s_gyro_accel_mult_slider.generic.name = "multiplier";
		s_gyro_accel_mult_slider.cvar = "gyro_accel_multiplier";
		s_gyro_accel_mult_slider.minvalue = GYRO_MIN_ACCEL_MULT;
		s_gyro_accel_mult_slider.maxvalue = GYRO_MAX_ACCEL_MULT;
		s_gyro_accel_mult_slider.slidestep = GYRO_STEP_ACCEL_MULT;

		s_gyro_accel_lower_thresh_slider.generic.type = MTYPE_SLIDER;
		s_gyro_accel_lower_thresh_slider.generic.x = 0;
		s_gyro_accel_lower_thresh_slider.generic.y = (y += 10);
		s_gyro_accel_lower_thresh_slider.generic.name = "lower thresh";
		s_gyro_accel_lower_thresh_slider.generic.callback = GyroAcceThreshFunc;
		s_gyro_accel_lower_thresh_slider.cvar = "gyro_accel_lower_thresh";
		s_gyro_accel_lower_thresh_slider.minvalue = GYRO_MIN_ACCEL_THRESH;
		s_gyro_accel_lower_thresh_slider.maxvalue = GYRO_MAX_ACCEL_THRESH;
		s_gyro_accel_lower_thresh_slider.slidestep = GYRO_STEP_ACCEL_THRESH;
		s_gyro_accel_lower_thresh_slider.printformat = "%.0f";

		s_gyro_accel_upper_thresh_slider.generic.type = MTYPE_SLIDER;
		s_gyro_accel_upper_thresh_slider.generic.x = 0;
		s_gyro_accel_upper_thresh_slider.generic.y = (y += 10);
		s_gyro_accel_upper_thresh_slider.generic.name = "upper thresh";
		s_gyro_accel_upper_thresh_slider.generic.callback = GyroAcceThreshFunc;
		s_gyro_accel_upper_thresh_slider.cvar = "gyro_accel_upper_thresh";
		s_gyro_accel_upper_thresh_slider.minvalue = GYRO_MIN_ACCEL_THRESH;
		s_gyro_accel_upper_thresh_slider.maxvalue = GYRO_MAX_ACCEL_THRESH;
		s_gyro_accel_upper_thresh_slider.slidestep = GYRO_STEP_ACCEL_THRESH;
		s_gyro_accel_upper_thresh_slider.printformat = "%.0f";
	}

	s_calibrating_text[0].generic.type = MTYPE_SEPARATOR;
	s_calibrating_text[0].generic.x = 48 * scale + 32;
	s_calibrating_text[0].generic.y = (y += 20);
	s_calibrating_text[0].generic.name = "place the gamepad on a flat,";

	s_calibrating_text[1].generic.type = MTYPE_SEPARATOR;
	s_calibrating_text[1].generic.x = 48 * scale + 32;
	s_calibrating_text[1].generic.y = (y += 10);
	s_calibrating_text[1].generic.name = "stable surface to...";

	s_calibrate_gyro.generic.type = MTYPE_ACTION;
	s_calibrate_gyro.generic.x = 0;
	s_calibrate_gyro.generic.y = (y += 15);
	s_calibrate_gyro.generic.name = "calibrate";
	s_calibrate_gyro.generic.callback = CalibrateGyroFunc;

	Menu_AddItem(&s_gyro_menu, (void *)&s_gyro_mode_box);
	Menu_AddItem(&s_gyro_menu, (void *)&s_gyro_space_box);

	if (s_gyro_space_box.curvalue == GYRO_SPACE_LOCAL)
	{
		Menu_AddItem(&s_gyro_menu, (void *)&s_gyro_local_roll_box);
	}

	Menu_AddItem(&s_gyro_menu, (void *)&s_gyro_yawsensitivity_slider);
	Menu_AddItem(&s_gyro_menu, (void *)&s_gyro_pitchsensitivity_slider);
	Menu_AddItem(&s_gyro_menu, (void *)&s_gyro_tightening_slider);
	Menu_AddItem(&s_gyro_menu, (void *)&s_gyro_smoothing_slider);
	Menu_AddItem(&s_gyro_menu, (void *)&s_gyro_acceleration_box);

	if (s_gyro_acceleration_box.curvalue)
	{
		Menu_AddItem(&s_gyro_menu, (void *)&s_gyro_accel_mult_slider);
		Menu_AddItem(&s_gyro_menu, (void *)&s_gyro_accel_lower_thresh_slider);
		Menu_AddItem(&s_gyro_menu, (void *)&s_gyro_accel_upper_thresh_slider);
	}

	Menu_AddItem(&s_gyro_menu, (void *)&s_calibrating_text[0]);
	Menu_AddItem(&s_gyro_menu, (void *)&s_calibrating_text[1]);
	Menu_AddItem(&s_gyro_menu, (void *)&s_calibrate_gyro);

	if (IsCalibrationZero())
	{
		Menu_SetStatusBar(&s_gyro_menu, "Calibration required");
	}

	Menu_Center(&s_gyro_menu);
}

static void
Gyro_MenuDraw(void)
{
	Menu_AdjustCursor(&s_gyro_menu, 1);
	Menu_Draw(&s_gyro_menu);
	M_Popup();
}

static const char *
Gyro_MenuKey(int key)
{
	return Default_MenuKey(&s_gyro_menu, key);
}

static void
M_Menu_Gyro_f(void)
{
	Gyro_MenuInit();
	s_gyro_menu.draw = Gyro_MenuDraw;
	s_gyro_menu.key  = Gyro_MenuKey;

	M_PushMenu(&s_gyro_menu);
}

/*
 * JOY MENU
 */
static menuslider_s s_joy_preset_slider;
static menulist_s s_joy_preset_box;
static menulist_s s_joy_advanced_box;
static menulist_s s_joy_invertpitch_box;
static menuslider_s s_joy_yawspeed_slider;
static menuslider_s s_joy_pitchspeed_slider;
static menuslider_s s_joy_extra_yawspeed_slider;
static menuslider_s s_joy_extra_pitchspeed_slider;
static menuslider_s s_joy_ramp_time_slider;
static menuslider_s s_joy_haptic_slider;
static menuaction_s s_joy_stickcfg_action;
static menuaction_s s_joy_gyro_action;
static menuaction_s s_joy_customize_buttons_action;
static menuaction_s s_joy_customize_alt_buttons_action;

extern void IN_ApplyJoyPreset(void);
extern qboolean IN_MatchJoyPreset(void);

static void
RefreshJoyMenuFunc(void *unused)
{
	M_PopMenuSilent();
	M_Menu_Joy_f();
}

static void
JoyPresetFunc(void *unused)
{
	IN_ApplyJoyPreset();
	RefreshJoyMenuFunc(NULL);
}

static void
JoyAdvancedFunc(void *unused)
{
	Cvar_SetValue("joy_advanced", s_joy_advanced_box.curvalue);
	RefreshJoyMenuFunc(NULL);
}

static void
CustomizeControllerButtonsFunc(void *unused)
{
	M_Menu_ControllerButtons_f();
}

static void
CustomizeControllerAltButtonsFunc(void *unused)
{
	M_Menu_ControllerAltButtons_f();
}

static void
ConfigStickFunc(void *unused)
{
	M_Menu_Stick_f();
}

static void
ConfigGyroFunc(void *unused)
{
	M_Menu_Gyro_f();
}

static void
InvertJoyPitchFunc(void *unused)
{
	Cvar_SetValue("joy_pitchspeed", -Cvar_VariableValue("joy_pitchspeed"));
}

static void
Joy_MenuInit(void)
{
	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	static const char *custom_names[] =
	{
		"",
		"custom",
		"",
		0,
	};

	extern qboolean show_haptic;
	unsigned short int y = 0;

	s_joy_menu.x = (int)(viddef.width * 0.50f);
	s_joy_menu.nitems = 0;

	if (IN_MatchJoyPreset())
	{
		s_joy_preset_slider.generic.type = MTYPE_SLIDER;
		s_joy_preset_slider.generic.x = 0;
		s_joy_preset_slider.generic.y = y;
		s_joy_preset_slider.generic.name = "look sensitivity";
		s_joy_preset_slider.generic.callback = JoyPresetFunc;
		s_joy_preset_slider.cvar = "joy_sensitivity";
		s_joy_preset_slider.minvalue = 0.0f;
		s_joy_preset_slider.maxvalue = 8.0f;
		s_joy_preset_slider.slidestep = 1.0f;
		s_joy_preset_slider.printformat = "%.0f";
		Menu_AddItem(&s_joy_menu, (void *)&s_joy_preset_slider);
	}
	else // Display "custom"
	{
		s_joy_preset_box.generic.type = MTYPE_SPINCONTROL;
		s_joy_preset_box.generic.x = 0;
		s_joy_preset_box.generic.y = y;
		s_joy_preset_box.generic.name = "look sensitivity";
		s_joy_preset_box.generic.callback = JoyPresetFunc;
		s_joy_preset_box.itemnames = custom_names;
		s_joy_preset_box.curvalue = 1;
		Menu_AddItem(&s_joy_menu, (void *)&s_joy_preset_box);
	}

	s_joy_advanced_box.generic.type = MTYPE_SPINCONTROL;
	s_joy_advanced_box.generic.x = 0;
	s_joy_advanced_box.generic.y = (y += 10);
	s_joy_advanced_box.generic.name = "show advanced";
	s_joy_advanced_box.generic.callback = JoyAdvancedFunc;
	s_joy_advanced_box.itemnames = yesno_names;
	s_joy_advanced_box.curvalue = (Cvar_VariableValue("joy_advanced") > 0);
	Menu_AddItem(&s_joy_menu, (void *)&s_joy_advanced_box);

	if (s_joy_advanced_box.curvalue)
	{
		s_joy_yawspeed_slider.generic.type = MTYPE_SLIDER;
		s_joy_yawspeed_slider.generic.x = 0;
		s_joy_yawspeed_slider.generic.y = (y += 10);
		s_joy_yawspeed_slider.generic.name = "yaw speed";
		s_joy_yawspeed_slider.generic.callback = RefreshJoyMenuFunc;
		s_joy_yawspeed_slider.cvar = "joy_yawspeed";
		s_joy_yawspeed_slider.minvalue = 0.0f;
		s_joy_yawspeed_slider.maxvalue = 720.0f;
		s_joy_yawspeed_slider.slidestep = 10.0f;
		s_joy_yawspeed_slider.printformat = "%.0f";
		Menu_AddItem(&s_joy_menu, (void *)&s_joy_yawspeed_slider);

		s_joy_pitchspeed_slider.generic.type = MTYPE_SLIDER;
		s_joy_pitchspeed_slider.generic.x = 0;
		s_joy_pitchspeed_slider.generic.y = (y += 10);
		s_joy_pitchspeed_slider.generic.name = "pitch speed";
		s_joy_pitchspeed_slider.generic.callback = RefreshJoyMenuFunc;
		s_joy_pitchspeed_slider.cvar = "joy_pitchspeed";
		s_joy_pitchspeed_slider.minvalue = 0.0f;
		s_joy_pitchspeed_slider.maxvalue = 720.0f;
		s_joy_pitchspeed_slider.slidestep = 10.0f;
		s_joy_pitchspeed_slider.printformat = "%.0f";
		s_joy_pitchspeed_slider.abs = true;
		Menu_AddItem(&s_joy_menu, (void *)&s_joy_pitchspeed_slider);

		s_joy_extra_yawspeed_slider.generic.type = MTYPE_SLIDER;
		s_joy_extra_yawspeed_slider.generic.x = 0;
		s_joy_extra_yawspeed_slider.generic.y = (y += 10);
		s_joy_extra_yawspeed_slider.generic.name = "extra yaw speed";
		s_joy_extra_yawspeed_slider.cvar = "joy_extra_yawspeed";
		s_joy_extra_yawspeed_slider.generic.callback = RefreshJoyMenuFunc;
		s_joy_extra_yawspeed_slider.minvalue = 0.0f;
		s_joy_extra_yawspeed_slider.maxvalue = 720.0f;
		s_joy_extra_yawspeed_slider.slidestep = 10.0f;
		s_joy_extra_yawspeed_slider.printformat = "%.0f";
		Menu_AddItem(&s_joy_menu, (void *)&s_joy_extra_yawspeed_slider);

		s_joy_extra_pitchspeed_slider.generic.type = MTYPE_SLIDER;
		s_joy_extra_pitchspeed_slider.generic.x = 0;
		s_joy_extra_pitchspeed_slider.generic.y = (y += 10);
		s_joy_extra_pitchspeed_slider.generic.name = "extra pitch speed";
		s_joy_extra_pitchspeed_slider.cvar = "joy_extra_pitchspeed";
		s_joy_extra_pitchspeed_slider.generic.callback = RefreshJoyMenuFunc;
		s_joy_extra_pitchspeed_slider.minvalue = 0.0f;
		s_joy_extra_pitchspeed_slider.maxvalue = 720.0f;
		s_joy_extra_pitchspeed_slider.slidestep = 10.0f;
		s_joy_extra_pitchspeed_slider.printformat = "%.0f";
		Menu_AddItem(&s_joy_menu, (void *)&s_joy_extra_pitchspeed_slider);

		s_joy_ramp_time_slider.generic.type = MTYPE_SLIDER;
		s_joy_ramp_time_slider.generic.x = 0;
		s_joy_ramp_time_slider.generic.y = (y += 10);
		s_joy_ramp_time_slider.generic.name = "ramp time";
		s_joy_ramp_time_slider.cvar = "joy_ramp_time";
		s_joy_ramp_time_slider.generic.callback = RefreshJoyMenuFunc;
		s_joy_ramp_time_slider.minvalue = 0.0f;
		s_joy_ramp_time_slider.maxvalue = 1.0f;
		s_joy_ramp_time_slider.slidestep = 0.05f;
		s_joy_ramp_time_slider.printformat = "%.2f";
		Menu_AddItem(&s_joy_menu, (void *)&s_joy_ramp_time_slider);
	}

	s_joy_invertpitch_box.generic.type = MTYPE_SPINCONTROL;
	s_joy_invertpitch_box.generic.x = 0;
	s_joy_invertpitch_box.generic.y = (y += 10);
	s_joy_invertpitch_box.generic.name = "invert pitch";
	s_joy_invertpitch_box.generic.callback = InvertJoyPitchFunc;
	s_joy_invertpitch_box.itemnames = yesno_names;
	s_joy_invertpitch_box.curvalue = (Cvar_VariableValue("joy_pitchspeed") < 0);
	Menu_AddItem(&s_joy_menu, (void *)&s_joy_invertpitch_box);

	if (show_haptic) {
		s_joy_haptic_slider.generic.type = MTYPE_SLIDER;
		s_joy_haptic_slider.generic.x = 0;
		s_joy_haptic_slider.generic.y = (y += 20);
		s_joy_haptic_slider.generic.name = "rumble intensity";
		s_joy_haptic_slider.cvar = "joy_haptic_magnitude";
		s_joy_haptic_slider.minvalue = 0.0f;
		s_joy_haptic_slider.maxvalue = 2.0f;
		Menu_AddItem(&s_joy_menu, (void *)&s_joy_haptic_slider);
	}

	s_joy_stickcfg_action.generic.type = MTYPE_ACTION;
	s_joy_stickcfg_action.generic.x = 0;
	s_joy_stickcfg_action.generic.y = (y += 20);
	s_joy_stickcfg_action.generic.name = "sticks config";
	s_joy_stickcfg_action.generic.callback = ConfigStickFunc;
	Menu_AddItem(&s_joy_menu, (void *)&s_joy_stickcfg_action);

	if (show_gyro)
	{
		s_joy_gyro_action.generic.type = MTYPE_ACTION;
		s_joy_gyro_action.generic.x = 0;
		s_joy_gyro_action.generic.y = (y += 10);
		s_joy_gyro_action.generic.name = "gyro options";
		s_joy_gyro_action.generic.callback = ConfigGyroFunc;
		Menu_AddItem(&s_joy_menu, (void *)&s_joy_gyro_action);
	}

	s_joy_customize_buttons_action.generic.type = MTYPE_ACTION;
	s_joy_customize_buttons_action.generic.x = 0;
	s_joy_customize_buttons_action.generic.y = (y += 20);
	s_joy_customize_buttons_action.generic.name = "customize buttons";
	s_joy_customize_buttons_action.generic.callback = CustomizeControllerButtonsFunc;
	Menu_AddItem(&s_joy_menu, (void *)&s_joy_customize_buttons_action);

	s_joy_customize_alt_buttons_action.generic.type = MTYPE_ACTION;
	s_joy_customize_alt_buttons_action.generic.x = 0;
	s_joy_customize_alt_buttons_action.generic.y = (y += 10);
	s_joy_customize_alt_buttons_action.generic.name = "custom. alt buttons";
	s_joy_customize_alt_buttons_action.generic.callback = CustomizeControllerAltButtonsFunc;
	Menu_AddItem(&s_joy_menu, (void *)&s_joy_customize_alt_buttons_action);

	Menu_Center(&s_joy_menu);
}

static void
Joy_MenuDraw(void)
{
	Menu_AdjustCursor(&s_joy_menu, 1);
	Menu_Draw(&s_joy_menu);
}

static const char *
Joy_MenuKey(int key)
{
	return Default_MenuKey(&s_joy_menu, key);
}

static void
M_Menu_Joy_f(void)
{
	Joy_MenuInit();
	s_joy_menu.draw = Joy_MenuDraw;
	s_joy_menu.key  = Joy_MenuKey;

	M_PushMenu(&s_joy_menu);
}

/*
 * CONTROLS MENU
 */

static menuframework_s s_options_menu;
static menuaction_s s_options_defaults_action;
static menuaction_s s_options_customize_options_action;
static menuaction_s s_options_customize_joy_action;
static menuslider_s s_options_sensitivity_slider;
static menulist_s s_options_freelook_box;
static menulist_s s_options_alwaysrun_box;
static menulist_s s_options_invertmouse_box;
static menulist_s s_options_lookstrafe_box;
static menulist_s s_options_crosshair_box;
static menuslider_s s_options_sfxvolume_slider;
static menulist_s s_options_oggshuffle_box;
static menuslider_s s_options_oggvolume_slider;
static menulist_s s_options_oggenable_box;
static menulist_s s_options_quality_list;
static menulist_s s_options_console_action;
static menulist_s s_options_pauseonfocus_box;

static void
CrosshairFunc(void *unused)
{
	Cvar_SetValue("crosshair", (float)s_options_crosshair_box.curvalue);
}

static void
PauseFocusFunc(void *unused)
{
	Cvar_SetValue("vid_pauseonfocuslost", (float)s_options_pauseonfocus_box.curvalue);
}

static void
CustomizeControlsFunc(void *unused)
{
	M_Menu_Keys_f();
}

static void
CustomizeJoyFunc(void *unused)
{
	M_Menu_Joy_f();
}

static void
AlwaysRunFunc(void *unused)
{
	Cvar_SetValue("cl_run", (float)s_options_alwaysrun_box.curvalue);
}

static void
FreeLookFunc(void *unused)
{
	Cvar_SetValue("freelook", (float)s_options_freelook_box.curvalue);
}

static void
ControlsSetMenuItemValues(void)
{
	s_options_oggshuffle_box.curvalue = Cvar_VariableValue("ogg_shuffle");
	s_options_oggenable_box.curvalue = (Cvar_VariableValue("ogg_enable") != 0);
	s_options_quality_list.curvalue = (Cvar_VariableValue("s_openal") == 0);
	s_options_alwaysrun_box.curvalue = (cl_run->value != 0);
	s_options_invertmouse_box.curvalue = (m_pitch->value < 0);
	s_options_lookstrafe_box.curvalue = (lookstrafe->value != 0);
	s_options_freelook_box.curvalue = (freelook->value != 0);
	s_options_crosshair_box.curvalue = ClampCvar(0, 3, crosshair->value);
	s_options_pauseonfocus_box.curvalue = ClampCvar(0, 2, Cvar_VariableValue("vid_pauseonfocuslost"));
}

static void
ControlsResetDefaultsFunc(void *unused)
{
	Cbuf_AddText("exec default.cfg\n");
	Cbuf_AddText("exec yq2.cfg\n");
	Cbuf_Execute();

	ControlsSetMenuItemValues();
	s_options_oggshuffle_box.curvalue = 0;
}

static void
InvertMouseFunc(void *unused)
{
	Cvar_SetValue("m_pitch", -m_pitch->value);
}

static void
LookstrafeFunc(void *unused)
{
	Cvar_SetValue("lookstrafe", (float)!lookstrafe->value);
}

static void
OGGShuffleFunc(void *unused)
{
	Cvar_SetValue("ogg_shuffle", s_options_oggshuffle_box.curvalue);
}

static void
EnableOGGMusic(void *unused)
{
	Cvar_SetValue("ogg_enable", (float)s_options_oggenable_box.curvalue);

	if (s_options_oggenable_box.curvalue)
	{
		OGG_Init();
		OGG_InitTrackList();
		OGG_Stop();

		if (cls.state == ca_active)
		{
			OGG_PlayTrack(cl.configstrings[CS_CDTRACK], true, true);
		}
	}
	else
	{
		OGG_Shutdown();
	}
}

extern void Key_ClearTyping(void);

static void
ConsoleFunc(void *unused)
{
	SCR_EndLoadingPlaque(); /* get rid of loading plaque */

	if (cl.attractloop)
	{
		Cbuf_AddText("killserver\n");
		return;
	}

	Key_ClearTyping();
	Con_ClearNotify();

	M_ForceMenuOff();
	cls.key_dest = key_console;

	if ((Cvar_VariableValue("maxclients") == 1) &&
			Com_ServerState())
	{
		Cvar_Set("paused", "1");
	}
}

static void
UpdateSoundBackendFunc(void *unused)
{
	Cvar_Set("s_openal", (s_options_quality_list.curvalue == 0)? "1":"0" );

	m_popup_string = "Restarting the sound system. This\n"
					 "could take up to a minute, so\n"
					 "please be patient.";
	m_popup_endtime = cls.realtime + 2000;
	M_Popup();

	/* the text box won't show up unless we do a buffer swap */
	R_EndFrame();

	CL_Snd_Restart_f();
}

static void
Options_MenuInit(void)
{
	extern qboolean show_gamepad;

	static const char *ogg_shuffle_items[] =
	{
		"default",
		"play once",
		"sequential",
		"random",
		"truly random",
		0
	};

	static const char *able_items[] =
	{
		"disabled",
		"enabled",
		0
	};

	static const char *sound_items[] =
	{
		"openal", "sdl", 0
	};

	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	static const char* pause_names[] =
	{
		"yes",
		"no",
		"unpause on re-focus",
		0
	};

	static const char *crosshair_names[] =
	{
		"none",
		"cross",
		"dot",
		"angle",
		0
	};

	float scale = SCR_GetMenuScale();
	unsigned short int y = 0;

	/* configure controls menu and menu items */
	s_options_menu.x = viddef.width / 2;
	s_options_menu.y = viddef.height / (2 * scale) - 58;
	s_options_menu.nitems = 0;

	s_options_sfxvolume_slider.generic.type = MTYPE_SLIDER;
	s_options_sfxvolume_slider.generic.x = 0;
	s_options_sfxvolume_slider.generic.y = y;
	s_options_sfxvolume_slider.generic.name = "effects volume";
	s_options_sfxvolume_slider.cvar = "s_volume";
	s_options_sfxvolume_slider.minvalue = 0.0f;
	s_options_sfxvolume_slider.maxvalue = 1.0f;

	s_options_oggvolume_slider.generic.type = MTYPE_SLIDER;
	s_options_oggvolume_slider.generic.x = 0;
	s_options_oggvolume_slider.generic.y = (y += 10);
	s_options_oggvolume_slider.generic.name = "OGG volume";
	s_options_oggvolume_slider.cvar = "ogg_volume";
	s_options_oggvolume_slider.minvalue = 0.0f;
	s_options_oggvolume_slider.maxvalue = 1.0f;

	s_options_oggenable_box.generic.type = MTYPE_SPINCONTROL;
	s_options_oggenable_box.generic.x = 0;
	s_options_oggenable_box.generic.y = (y += 10);
	s_options_oggenable_box.generic.name = "OGG music";
	s_options_oggenable_box.generic.callback = EnableOGGMusic;
	s_options_oggenable_box.itemnames = able_items;

	s_options_oggshuffle_box.generic.type = MTYPE_SPINCONTROL;
	s_options_oggshuffle_box.generic.x = 0;
	s_options_oggshuffle_box.generic.y = (y += 10);
	s_options_oggshuffle_box.generic.name = "OGG shuffle";
	s_options_oggshuffle_box.generic.callback = OGGShuffleFunc;
	s_options_oggshuffle_box.itemnames = ogg_shuffle_items;

	s_options_quality_list.generic.type = MTYPE_SPINCONTROL;
	s_options_quality_list.generic.x = 0;
	s_options_quality_list.generic.y = (y += 10);
	s_options_quality_list.generic.name = "sound backend";
	s_options_quality_list.generic.callback = UpdateSoundBackendFunc;
	s_options_quality_list.itemnames = sound_items;

	s_options_sensitivity_slider.generic.type = MTYPE_SLIDER;
	s_options_sensitivity_slider.generic.x = 0;
	s_options_sensitivity_slider.generic.y = (y += 20);
	s_options_sensitivity_slider.generic.name = "mouse speed";
	s_options_sensitivity_slider.cvar = "sensitivity";
	s_options_sensitivity_slider.minvalue = 0;
	s_options_sensitivity_slider.maxvalue = 11;
	s_options_sensitivity_slider.slidestep = 0.5f;

	s_options_alwaysrun_box.generic.type = MTYPE_SPINCONTROL;
	s_options_alwaysrun_box.generic.x = 0;
	s_options_alwaysrun_box.generic.y = (y += 10);
	s_options_alwaysrun_box.generic.name = "always run";
	s_options_alwaysrun_box.generic.callback = AlwaysRunFunc;
	s_options_alwaysrun_box.itemnames = yesno_names;

	s_options_invertmouse_box.generic.type = MTYPE_SPINCONTROL;
	s_options_invertmouse_box.generic.x = 0;
	s_options_invertmouse_box.generic.y = (y += 10);
	s_options_invertmouse_box.generic.name = "invert mouse";
	s_options_invertmouse_box.generic.callback = InvertMouseFunc;
	s_options_invertmouse_box.itemnames = yesno_names;

	s_options_lookstrafe_box.generic.type = MTYPE_SPINCONTROL;
	s_options_lookstrafe_box.generic.x = 0;
	s_options_lookstrafe_box.generic.y = (y += 10);
	s_options_lookstrafe_box.generic.name = "lookstrafe";
	s_options_lookstrafe_box.generic.callback = LookstrafeFunc;
	s_options_lookstrafe_box.itemnames = yesno_names;

	s_options_freelook_box.generic.type = MTYPE_SPINCONTROL;
	s_options_freelook_box.generic.x = 0;
	s_options_freelook_box.generic.y = (y += 10);
	s_options_freelook_box.generic.name = "free look";
	s_options_freelook_box.generic.callback = FreeLookFunc;
	s_options_freelook_box.itemnames = yesno_names;

	s_options_crosshair_box.generic.type = MTYPE_SPINCONTROL;
	s_options_crosshair_box.generic.x = 0;
	s_options_crosshair_box.generic.y = (y += 10);
	s_options_crosshair_box.generic.name = "crosshair";
	s_options_crosshair_box.generic.callback = CrosshairFunc;
	s_options_crosshair_box.itemnames = crosshair_names;

	s_options_pauseonfocus_box.generic.type = MTYPE_SPINCONTROL;
	s_options_pauseonfocus_box.generic.x = 0;
	s_options_pauseonfocus_box.generic.y = (y += 10);
	s_options_pauseonfocus_box.generic.name = "pause on minimized";
	s_options_pauseonfocus_box.generic.callback = PauseFocusFunc;
	s_options_pauseonfocus_box.itemnames = pause_names;

	y += 10;
	if (show_gamepad)
	{
		s_options_customize_joy_action.generic.type = MTYPE_ACTION;
		s_options_customize_joy_action.generic.x = 0;
		s_options_customize_joy_action.generic.y = (y += 10);
		s_options_customize_joy_action.generic.name = "customize gamepad";
		s_options_customize_joy_action.generic.callback = CustomizeJoyFunc;
	}

	s_options_customize_options_action.generic.type = MTYPE_ACTION;
	s_options_customize_options_action.generic.x = 0;
	s_options_customize_options_action.generic.y = (y += 10);
	s_options_customize_options_action.generic.name = "customize controls";
	s_options_customize_options_action.generic.callback = CustomizeControlsFunc;

	s_options_defaults_action.generic.type = MTYPE_ACTION;
	s_options_defaults_action.generic.x = 0;
	s_options_defaults_action.generic.y = (y += 10);
	s_options_defaults_action.generic.name = "reset defaults";
	s_options_defaults_action.generic.callback = ControlsResetDefaultsFunc;

	s_options_console_action.generic.type = MTYPE_ACTION;
	s_options_console_action.generic.x = 0;
	s_options_console_action.generic.y = (y += 10);
	s_options_console_action.generic.name = "go to console";
	s_options_console_action.generic.callback = ConsoleFunc;

	ControlsSetMenuItemValues();

	Menu_AddItem(&s_options_menu, (void *)&s_options_sfxvolume_slider);

	Menu_AddItem(&s_options_menu, (void *)&s_options_oggvolume_slider);
	Menu_AddItem(&s_options_menu, (void *)&s_options_oggenable_box);
	Menu_AddItem(&s_options_menu, (void *)&s_options_oggshuffle_box);
	Menu_AddItem(&s_options_menu, (void *)&s_options_quality_list);
	Menu_AddItem(&s_options_menu, (void *)&s_options_sensitivity_slider);
	Menu_AddItem(&s_options_menu, (void *)&s_options_alwaysrun_box);
	Menu_AddItem(&s_options_menu, (void *)&s_options_invertmouse_box);
	Menu_AddItem(&s_options_menu, (void *)&s_options_lookstrafe_box);
	Menu_AddItem(&s_options_menu, (void *)&s_options_freelook_box);
	Menu_AddItem(&s_options_menu, (void *)&s_options_crosshair_box);
	Menu_AddItem(&s_options_menu, (void*)&s_options_pauseonfocus_box);

	if (show_gamepad)
	{
		Menu_AddItem(&s_options_menu, (void *)&s_options_customize_joy_action);
	}
	Menu_AddItem(&s_options_menu, (void *)&s_options_customize_options_action);
	Menu_AddItem(&s_options_menu, (void *)&s_options_defaults_action);
	Menu_AddItem(&s_options_menu, (void *)&s_options_console_action);
}

static void
Options_MenuDraw(void)
{
	M_Banner("m_banner_options");
	Menu_AdjustCursor(&s_options_menu, 1);
	Menu_Draw(&s_options_menu);
	M_Popup();
}

static const char *
Options_MenuKey(int key)
{
	if (m_popup_string)
	{
		m_popup_string = NULL;
		return NULL;
	}
	return Default_MenuKey(&s_options_menu, key);
}

static void
M_Menu_Options_f(void)
{
	Options_MenuInit();
	s_options_menu.draw = Options_MenuDraw;
	s_options_menu.key  = Options_MenuKey;

	M_PushMenu(&s_options_menu);
}

/*
 * END GAME MENU
 */

menuframework_s s_credits;

#define CREDITS_SIZE 256
static int credits_start_time;
static const char **credits;
static char *creditsIndex[CREDITS_SIZE];
static char *creditsBuffer;
static const char *idcredits[] = {
	"+QUAKE II BY ID SOFTWARE",
	"",
	"+PROGRAMMING",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"",
	"+ART",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"",
	"+LEVEL DESIGN",
	"Tim Willits",
	"American McGee",
	"Christian Antkow",
	"Jennell Jaquays",
	"Brandon James",
	"",
	"+BIZ",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Donna Jackson",
	"",
	"",
	"+SPECIAL THANKS",
	"Ben Donges for beta testing",
	"",
	"",
	"",
	"",
	"",
	"",
	"+ADDITIONAL SUPPORT",
	"",
	"+LINUX PORT AND CTF",
	"Zoid Kirsch",
	"",
	"+CINEMATIC SEQUENCES",
	"Ending Cinematic by Blur Studio - ",
	"Venice, CA",
	"",
	"Environment models for Introduction",
	"Cinematic by Karl Dolgener",
	"",
	"Assistance with environment design",
	"by Cliff Iwai",
	"",
	"+SOUND EFFECTS AND MUSIC",
	"Sound Design by Soundelux Media Labs.",
	"Music Composed and Produced by",
	"Soundelux Media Labs.  Special thanks",
	"to Bill Brown, Tom Ozanich, Brian",
	"Celano, Jeff Eisner, and The Soundelux",
	"Players.",
	"",
	"\"Level Music\" by Sonic Mayhem",
	"www.sonicmayhem.com",
	"",
	"\"Quake II Theme Song\"",
	"(C) 1997 Rob Zombie. All Rights",
	"Reserved.",
	"",
	"Track 10 (\"Climb\") by Jer Sypult",
	"",
	"Voice of computers by",
	"Carly Staehlin-Taylor",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"John Tam",
	"Steve Rosenthal",
	"Marty Stratton",
	"Henk Hartong",
	"",
	"+PATCHES AUTHORS",
	"eliasm",
	"",
	"+YAMAGI QUAKE II BY",
	"Yamagi Burmeister",
	"Daniel Gibson",
	"Sander van Dijk",
	"Denis Pauk",
	"Bjorn Alfredsson",
	"Jaime Moreira",
	"",
	"Quake II(tm) (C)1997 Id Software, Inc.",
	"All Rights Reserved.  Distributed by",
	"Activision, Inc. under license.",
	"Quake II(tm), the Id Software name,",
	"the \"Q II\"(tm) logo and id(tm)",
	"logo are trademarks of Id Software,",
	"Inc. Activision(R) is a registered",
	"trademark of Activision, Inc. All",
	"other trademarks and trade names are",
	"properties of their respective owners.",
	0
};

static const char *xatcredits[] =
{
	"+QUAKE II MISSION PACK: THE RECKONING",
	"+BY",
	"+XATRIX ENTERTAINMENT, INC.",
	"",
	"+DESIGN AND DIRECTION",
	"Drew Markham",
	"",
	"+PRODUCED BY",
	"Greg Goodrich",
	"",
	"+PROGRAMMING",
	"Rafael Paiz",
	"",
	"+LEVEL DESIGN / ADDITIONAL GAME DESIGN",
	"Alex Mayberry",
	"",
	"+LEVEL DESIGN",
	"Mal Blackwell",
	"Dan Koppel",
	"",
	"+ART DIRECTION",
	"Michael \"Maxx\" Kaufman",
	"",
	"+COMPUTER GRAPHICS SUPERVISOR AND",
	"+CHARACTER ANIMATION DIRECTION",
	"Barry Dempsey",
	"",
	"+SENIOR ANIMATOR AND MODELER",
	"Jason Hoover",
	"",
	"+CHARACTER ANIMATION AND",
	"+MOTION CAPTURE SPECIALIST",
	"Amit Doron",
	"",
	"+ART",
	"Claire Praderie-Markham",
	"Viktor Antonov",
	"Corky Lehmkuhl",
	"",
	"+INTRODUCTION ANIMATION",
	"Dominique Drozdz",
	"",
	"+ADDITIONAL LEVEL DESIGN",
	"Aaron Barber",
	"Rhett Baldwin",
	"",
	"+3D CHARACTER ANIMATION TOOLS",
	"Gerry Tyra, SA Technology",
	"",
	"+ADDITIONAL EDITOR TOOL PROGRAMMING",
	"Robert Duffy",
	"",
	"+ADDITIONAL PROGRAMMING",
	"Ryan Feltrin",
	"",
	"+PRODUCTION COORDINATOR",
	"Victoria Sylvester",
	"",
	"+SOUND DESIGN",
	"Gary Bradfield",
	"",
	"+MUSIC BY",
	"Sonic Mayhem",
	"",
	"",
	"",
	"+SPECIAL THANKS",
	"+TO",
	"+OUR FRIENDS AT ID SOFTWARE",
	"",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"Tim Willits",
	"Christian Antkow",
	"Jennell Jaquays",
	"Brandon James",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Zoid Kirsch",
	"Donna Jackson",
	"",
	"",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"Marty Stratton",
	"Henk \"The Original Ripper\" Hartong",
	"Kevin Kraff",
	"Jamey Gottlieb",
	"Chris Hepburn",
	"",
	"+AND THE GAME TESTERS",
	"",
	"Tim Vanlaw",
	"Doug Jacobs",
	"Steven Rosenthal",
	"David Baker",
	"Chris Campbell",
	"Aaron Casillas",
	"Steve Elwell",
	"Derek Johnstone",
	"Igor Krinitskiy",
	"Samantha Lee",
	"Michael Spann",
	"Chris Toft",
	"Juan Valdes",
	"",
	"+THANKS TO INTERGRAPH COMPUTER SYSTEMS",
	"+IN PARTICULAR:",
	"",
	"Michael T. Nicolaou",
	"",
	"",
	"Quake II Mission Pack: The Reckoning",
	"(tm) (C)1998 Id Software, Inc. All",
	"Rights Reserved. Developed by Xatrix",
	"Entertainment, Inc. for Id Software,",
	"Inc. Distributed by Activision Inc.",
	"under license. Quake(R) is a",
	"registered trademark of Id Software,",
	"Inc. Quake II Mission Pack: The",
	"Reckoning(tm), Quake II(tm), the Id",
	"Software name, the \"Q II\"(tm) logo",
	"and id(tm) logo are trademarks of Id",
	"Software, Inc. Activision(R) is a",
	"registered trademark of Activision,",
	"Inc. Xatrix(R) is a registered",
	"trademark of Xatrix Entertainment,",
	"Inc. All other trademarks and trade",
	"names are properties of their",
	"respective owners.",
	0
};

static const char *roguecredits[] =
{
	"+QUAKE II MISSION PACK 2: GROUND ZERO",
	"+BY",
	"+ROGUE ENTERTAINMENT, INC.",
	"",
	"+PRODUCED BY",
	"Jim Molinets",
	"",
	"+PROGRAMMING",
	"Peter Mack",
	"Patrick Magruder",
	"",
	"+LEVEL DESIGN",
	"Jim Molinets",
	"Cameron Lamprecht",
	"Berenger Fish",
	"Robert Selitto",
	"Steve Tietze",
	"Steve Thoms",
	"",
	"+ART DIRECTION",
	"Rich Fleider",
	"",
	"+ART",
	"Rich Fleider",
	"Steve Maines",
	"Won Choi",
	"",
	"+ANIMATION SEQUENCES",
	"Creat Studios",
	"Steve Maines",
	"",
	"+ADDITIONAL LEVEL DESIGN",
	"Rich Fleider",
	"Steve Maines",
	"Peter Mack",
	"",
	"+SOUND",
	"James Grunke",
	"",
	"+GROUND ZERO THEME",
	"+AND",
	"+MUSIC BY",
	"Sonic Mayhem",
	"",
	"+VWEP MODELS",
	"Brent \"Hentai\" Dill",
	"",
	"",
	"",
	"+SPECIAL THANKS",
	"+TO",
	"+OUR FRIENDS AT ID SOFTWARE",
	"",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"Tim Willits",
	"Christian Antkow",
	"Jennell Jaquays",
	"Brandon James",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Katherine Anna Kang",
	"Donna Jackson",
	"Zoid Kirsch",
	"",
	"",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"Marty Stratton",
	"Henk Hartong",
	"Mitch Lasky",
	"Steve Rosenthal",
	"Steve Elwell",
	"",
	"+AND THE GAME TESTERS",
	"",
	"The Ranger Clan",
	"Zoid Kirsch",
	"Nihilistic Software",
	"Robert Duffy",
	"",
	"And Countless Others",
	"",
	"",
	"",
	"Quake II Mission Pack 2: Ground Zero",
	"(tm) (C)1998 Id Software, Inc. All",
	"Rights Reserved. Developed by Rogue",
	"Entertainment, Inc. for Id Software,",
	"Inc. Distributed by Activision Inc.",
	"under license. Quake(R) is a",
	"registered trademark of Id Software,",
	"Inc. Quake II Mission Pack 2: Ground",
	"Zero(tm), Quake II(tm), the Id",
	"Software name, the \"Q II\"(tm) logo",
	"and id(tm) logo are trademarks of Id",
	"Software, Inc. Activision(R) is a",
	"registered trademark of Activision,",
	"Inc. Rogue(R) is a registered",
	"trademark of Rogue Entertainment,",
	"Inc. All other trademarks and trade",
	"names are properties of their",
	"respective owners.",
	0
};

static void
M_Credits_Draw(void)
{
	int i, y;
	float scale = SCR_GetMenuScale();

	/* draw the credits */
	for (
		i = 0,
		y = (int)(viddef.height / scale - ((cls.realtime - credits_start_time) / 40.0F));
		credits[i] && y < viddef.height / scale;
		y += 10, i++
		)
	{
		int j, stringoffset = 0;
		int bold = false;

		if (y <= -8)
		{
			continue;
		}

		if (credits[i][0] == '+')
		{
			bold = true;
			stringoffset = 1;
		}
		else
		{
			bold = false;
			stringoffset = 0;
		}

		for (j = 0; credits[i][j + stringoffset]; j++)
		{
			int x;

			x = (viddef.width / scale- (int)strlen(credits[i]) * 8 - stringoffset *
				 8) / 2 + (j + stringoffset) * 8;

			if (bold)
			{
				Draw_CharScaled(x * scale, y * scale, credits[i][j + stringoffset] + 128, scale);
			}

			else
			{
				Draw_CharScaled(x * scale, y * scale, credits[i][j + stringoffset], scale);
			}
		}
	}

	if (y < 0)
	{
		credits_start_time = cls.realtime;
	}
}

static const char *
M_Credits_Key(int key)
{
	key = Key_GetMenuKey(key);
	if (key == K_ESCAPE)
	{
		if (creditsBuffer)
		{
			FS_FreeFile(creditsBuffer);
		}
		M_PopMenu();
	}

	return menu_out_sound;
}

static void
M_Menu_Credits_f(void)
{
	int count;
	char *p;

	creditsBuffer = NULL;
	count = FS_LoadFile("credits", (void **)&creditsBuffer);

	if (count != -1)
	{
		int n;
		p = creditsBuffer;

		// CREDITS_SIZE - 1 - last pointer should be NULL
		for (n = 0; n < CREDITS_SIZE - 1; n++)
		{
			creditsIndex[n] = p;

			while (*p != '\r' && *p != '\n')
			{
				p++;

				if (--count == 0)
				{
					break;
				}
			}

			if (*p == '\r')
			{
				*p++ = 0;

				if (--count == 0)
				{
					break;
				}
			}

			*p++ = 0;

			if (--count == 0)
			{
				// no credits any more
				// move one step futher for set NULL
				n ++;
				break;
			}
		}

		creditsIndex[n] = 0;
		credits = (const char **)creditsIndex;
	}
	else
	{
		if (M_IsGame("xatrix")) /* Xatrix - The Reckoning */
		{
			credits = xatcredits;
		}

		else if (M_IsGame("rogue")) /* Rogue - Ground Zero */
		{
			credits = roguecredits;
		}

		else
		{
			credits = idcredits;
		}
	}

	credits_start_time = cls.realtime;

	s_credits.draw = M_Credits_Draw;
	s_credits.key  = M_Credits_Key;

	M_PushMenu(&s_credits);
}

/*
 * MODS MENU
 */

static menuframework_s s_mods_menu;
static menulist_s s_mods_list;
static menuaction_s s_mods_apply_action;
static char mods_statusbar[64];

static char **modnames = NULL;
static int nummods;

void
Mods_NamesFinish(void)
{
	if (modnames)
	{
		int i;

		for (i = 0; i < nummods; i ++)
		{
			free(modnames[i]);
		}

		free(modnames);
		modnames = NULL;
	}
}

static void
Mods_NamesInit(void)
{
	/* initialize list of mods once, reuse it afterwards */
	if (modnames == NULL)
	{
		modnames = FS_ListMods(&nummods);
	}
}

static void
ModsListFunc(void *unused)
{
	if (strcmp(BASEDIRNAME, modnames[s_mods_list.curvalue]) == 0)
	{
		strcpy(mods_statusbar, "Quake II");
	}
	else if (strcmp("ctf", modnames[s_mods_list.curvalue]) == 0)
	{
		strcpy(mods_statusbar, "Quake II Capture The Flag");
	}
	else if (strcmp("rogue", modnames[s_mods_list.curvalue]) == 0)
	{
		strcpy(mods_statusbar, "Quake II Mission Pack: Ground Zero");
	}
	else if (strcmp("xatrix", modnames[s_mods_list.curvalue]) == 0)
	{
		strcpy(mods_statusbar, "Quake II Mission Pack: The Reckoning");
	}
	else
	{
		strcpy(mods_statusbar, "\0");
	}
}

static void
ModsApplyActionFunc(void *unused)
{
	if (!M_IsGame(modnames[s_mods_list.curvalue]))
	{
		if(Com_ServerState())
		{
			// equivalent to "killserver" cmd, but avoids cvar latching below
			SV_Shutdown("Server is changing games.\n", false);
			NET_Config(false);
		}

		// called via command buffer so that any running server has time to shutdown
		Cbuf_AddText(va("game %s\n", modnames[s_mods_list.curvalue]));

		// start the demo cycle in the new game directory
		menu_startdemoloop = true;

		M_ForceMenuOff();
	}
}

static void
Mods_MenuInit(void)
{
	int currentmod, x = 0, y = 0, i;
	char modname[MAX_QPATH]; /* TG626 */
	char **displaynames;

	Mods_NamesInit();

	/* create array of bracketed display names from folder names - TG626 */
	displaynames = malloc(sizeof(*displaynames) * (nummods + 1));
	YQ2_COM_CHECK_OOM(displaynames, "malloc()", sizeof(*displaynames) * (nummods + 1))
	if (!displaynames)
	{
		/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
		return;
	}

	for (i = 0; i < nummods; i++)
	{
		strcpy(modname, "[");
		if (strlen(modnames[i]) < 16)
		{
			strcat(modname, modnames[i]);
			for (int j=0; j < 15 - strlen(modnames[i]); j++)
			{
				strcat(modname, " ");
			}
		}
		else
		{
			strncat(modname, modnames[i], 12);
			strcat(modname, "...");
		}
		strcat(modname, "]");

		displaynames[i] = strdup(modname);
		YQ2_COM_CHECK_OOM(displaynames[i], "strdup()", strlen(modname) + 1)
		if (!displaynames[i])
		{
			/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
			return;
		}
	}

	displaynames[nummods] = NULL;
	/* end TG626 */

	/* pre-select the current mod for display in the list */
	for (currentmod = 0; currentmod < nummods; currentmod++)
	{
		if (M_IsGame(modnames[currentmod]))
		{
			break;
		}
	}

	s_mods_menu.x = viddef.width * 0.50;
	s_mods_menu.nitems = 0;

	s_mods_list.generic.type = MTYPE_SPINCONTROL;
	s_mods_list.generic.name = "mod";
	s_mods_list.generic.x = x;
	s_mods_list.generic.y = y;
	s_mods_list.generic.callback = ModsListFunc;
	s_mods_list.itemnames = (const char **)displaynames;
	s_mods_list.curvalue = currentmod < nummods ? currentmod : 0;

	y += 20;

	s_mods_apply_action.generic.type = MTYPE_ACTION;
	s_mods_apply_action.generic.name = "apply";
	s_mods_apply_action.generic.x = x;
	s_mods_apply_action.generic.y = y;
	s_mods_apply_action.generic.callback = ModsApplyActionFunc;

	Menu_AddItem(&s_mods_menu, (void *)&s_mods_list);
	Menu_AddItem(&s_mods_menu, (void *)&s_mods_apply_action);

	Menu_Center(&s_mods_menu);

	/* set the original mods statusbar */
	ModsListFunc(0);
	Menu_SetStatusBar(&s_mods_menu, mods_statusbar);
}

static void
Mods_MenuDraw(void)
{
	Menu_AdjustCursor(&s_mods_menu, 1);
	Menu_Draw(&s_mods_menu);
	M_Popup();
}

static const char *
Mods_MenuKey(int key)
{
	return Default_MenuKey(&s_mods_menu, key);
}

static void
M_Menu_Mods_f(void)
{
	Mods_MenuInit();
	s_mods_menu.draw = Mods_MenuDraw;
	s_mods_menu.key  = Mods_MenuKey;

	M_PushMenu(&s_mods_menu);
}

/*
 * GAME MENU
 */

static int m_game_cursor;

static menuframework_s s_game_menu;
static menuaction_s s_easy_game_action;
static menuaction_s s_medium_game_action;
static menuaction_s s_hard_game_action;
static menuaction_s s_hardp_game_action;
static menuaction_s s_load_game_action;
static menuaction_s s_save_game_action;
static menuaction_s s_credits_action;
static menuaction_s s_mods_action;
static menuseparator_s s_blankline;

static void
StartGame(void)
{
	if (cls.state != ca_disconnected && cls.state != ca_uninitialized)
	{
		CL_Disconnect();
	}

	/* disable updates and start the cinematic going */
	cl.servercount = -1;
	M_ForceMenuOff();
	Cvar_SetValue("deathmatch", 0);
	Cvar_SetValue("coop", 0);

	Cbuf_AddText("loading ; killserver ; wait ; newgame\n");
	cls.key_dest = key_game;
}

static void
EasyGameFunc(void *data)
{
	Cvar_ForceSet("skill", "0");
	StartGame();
}

static void
MediumGameFunc(void *data)
{
	Cvar_ForceSet("skill", "1");
	StartGame();
}

static void
HardGameFunc(void *data)
{
	Cvar_ForceSet("skill", "2");
	StartGame();
}

static void
HardpGameFunc(void *data)
{
	Cvar_ForceSet("skill", "3");
	StartGame();
}

static void
LoadGameFunc(void *unused)
{
	M_Menu_LoadGame_f();
}

static void
SaveGameFunc(void *unused)
{
	M_Menu_SaveGame_f();
}

static void
CreditsFunc(void *unused)
{
	M_Menu_Credits_f();
}

static void
ModsFunc(void *unused)
{
	M_Menu_Mods_f();
}

static void
Game_MenuInit(void)
{
	Mods_NamesInit();

	s_game_menu.x = (int)(viddef.width * 0.50f);
	s_game_menu.nitems = 0;

	s_easy_game_action.generic.type = MTYPE_ACTION;
	s_easy_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_easy_game_action.generic.x = 0;
	s_easy_game_action.generic.y = 0;
	s_easy_game_action.generic.name = "easy";
	s_easy_game_action.generic.callback = EasyGameFunc;

	s_medium_game_action.generic.type = MTYPE_ACTION;
	s_medium_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_medium_game_action.generic.x = 0;
	s_medium_game_action.generic.y = 10;
	s_medium_game_action.generic.name = "medium";
	s_medium_game_action.generic.callback = MediumGameFunc;

	s_hard_game_action.generic.type = MTYPE_ACTION;
	s_hard_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_hard_game_action.generic.x = 0;
	s_hard_game_action.generic.y = 20;
	s_hard_game_action.generic.name = "hard";
	s_hard_game_action.generic.callback = HardGameFunc;

	s_hardp_game_action.generic.type = MTYPE_ACTION;
	s_hardp_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_hardp_game_action.generic.x = 0;
	s_hardp_game_action.generic.y = 30;
	s_hardp_game_action.generic.name = "hard+";
	s_hardp_game_action.generic.callback = HardpGameFunc;

	s_blankline.generic.type = MTYPE_SEPARATOR;

	s_load_game_action.generic.type = MTYPE_ACTION;
	s_load_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_load_game_action.generic.x = 0;
	s_load_game_action.generic.y = 50;
	s_load_game_action.generic.name = "load game";
	s_load_game_action.generic.callback = LoadGameFunc;

	s_save_game_action.generic.type = MTYPE_ACTION;
	s_save_game_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_save_game_action.generic.x = 0;
	s_save_game_action.generic.y = 60;
	s_save_game_action.generic.name = "save game";
	s_save_game_action.generic.callback = SaveGameFunc;

	s_credits_action.generic.type = MTYPE_ACTION;
	s_credits_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_credits_action.generic.x = 0;
	s_credits_action.generic.y = 70;
	s_credits_action.generic.name = "credits";
	s_credits_action.generic.callback = CreditsFunc;

	Menu_AddItem(&s_game_menu, (void *)&s_easy_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_medium_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_hard_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_hardp_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_blankline);
	Menu_AddItem(&s_game_menu, (void *)&s_load_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_save_game_action);
	Menu_AddItem(&s_game_menu, (void *)&s_credits_action);

	if(nummods > 1)
	{
		s_mods_action.generic.type = MTYPE_ACTION;
		s_mods_action.generic.flags = QMF_LEFT_JUSTIFY;
		s_mods_action.generic.x = 0;
		s_mods_action.generic.y = 90;
		s_mods_action.generic.name = "mods";
		s_mods_action.generic.callback = ModsFunc;

		Menu_AddItem(&s_game_menu, (void *)&s_blankline);
		Menu_AddItem(&s_game_menu, (void *)&s_mods_action);
	}

	Menu_Center(&s_game_menu);
}

static void
Game_MenuDraw(void)
{
	M_Banner("m_banner_game");
	Menu_AdjustCursor(&s_game_menu, 1);
	Menu_Draw(&s_game_menu);
}

static const char *
Game_MenuKey(int key)
{
	return Default_MenuKey(&s_game_menu, key);
}

static void
M_Menu_Game_f(void)
{
	Game_MenuInit();
	s_game_menu.draw = Game_MenuDraw;
	s_game_menu.key  = Game_MenuKey;

	M_PushMenu(&s_game_menu);
	m_game_cursor = 1;
}


/*
 * LOADGAME MENU
 */

#define MAX_SAVESLOTS 14
#define MAX_SAVEPAGES 4

// The magic comment string length of 32 is the same as
// the comment string length set in SV_WriteServerFile()!

static char m_quicksavestring[32];
static qboolean m_quicksavevalid;

static char m_savestrings[MAX_SAVESLOTS][32];
static qboolean m_savevalid[MAX_SAVESLOTS];

static int m_loadsave_page;
static char m_loadsave_statusbar[32];

static menuframework_s s_loadgame_menu;
static menuaction_s s_loadgame_actions[MAX_SAVESLOTS + 1]; // One for quick

static menuframework_s s_savegame_menu;
static menuaction_s s_savegame_actions[MAX_SAVESLOTS + 1]; // One for quick

/* DELETE SAVEGAME */

static qboolean menukeyitem_delete = false;

static void
PromptDeleteSaveFunc(menuframework_s *m)
{
	const menucommon_s *item = Menu_ItemAtCursor(m);
	if (item == NULL || item->type != MTYPE_ACTION)
	{
		return;
	}

	if (item->localdata[0] == -1)
	{
		if (m_quicksavevalid)
		{
			menukeyitem_delete = true;
		}
	}
	else
	{
		if (m_savevalid[item->localdata[0] - m_loadsave_page * MAX_SAVESLOTS])
		{
			menukeyitem_delete = true;
		}
	}

	if (menukeyitem_delete)
	{
		Menu_SetStatusBar( m, "are you sure you want to delete? y\\n" );
	}
}

static qboolean
ExecDeleteSaveFunc(menuframework_s *m, int menu_key)
{
	menucommon_s *item = Menu_ItemAtCursor(m);
	menukeyitem_delete = false;

	if (menu_key == K_ENTER || menu_key == 'y' || menu_key == 'Y')
	{
		char name[MAX_OSPATH] = {0};
		if (item->localdata[0] == -1)	// quicksave
		{
			Com_sprintf(name, sizeof(name), "%s/save/quick/", FS_Gamedir());
		}
		else
		{
			Com_sprintf(name, sizeof(name), "%s/save/save%d/", FS_Gamedir(),
						item->localdata[0]);
		}
		Sys_RemoveDir(name);
		return true;
	}
	Menu_SetStatusBar( m, m_loadsave_statusbar );
	return false;
}

static void
Create_Savestrings(void)
{
	int i;
	fileHandle_t f;

	// The quicksave slot...
	FS_FOpenFile("save/quick/server.ssv", &f, true);

	if (!f)
	{
		strcpy(m_quicksavestring, "QUICKSAVE <empty>");
		m_quicksavevalid = false;
	}
	else
	{
		char tmp[32]; // Same length as m_quicksavestring-

		FS_Read(tmp, sizeof(tmp), f);
		FS_FCloseFile(f);
		tmp[sizeof(tmp) - 1] = 0;

		if (strlen(tmp) > 12)
		{
			/* Horrible hack to construct a nice looking 'QUICKSAVE Level Name'
			   comment string matching the 'ENTERING Level Name' comment string
			   of autosaves. The comment field is in format 'HH:MM mm:dd  Level
			   Name'. Remove the date (the first 13) characters and replace it
			   with 'QUICKSAVE'. */
			Com_sprintf(m_quicksavestring, sizeof(m_quicksavestring), "QUICKSAVE %s", tmp + 13);
		}
		else
		{
			Q_strlcpy(m_quicksavestring, tmp, sizeof(m_quicksavestring));
		}
		m_quicksavevalid = true;
	}

	// ... and everything else.
	for (i = 0; i < MAX_SAVESLOTS; i++)
	{
		char name[MAX_OSPATH];

		Com_sprintf(name, sizeof(name), "save/save%i/server.ssv", m_loadsave_page * MAX_SAVESLOTS + i);
		FS_FOpenFile(name, &f, true);

		if (!f)
		{
			strcpy(m_savestrings[i], "<empty>");
			m_savevalid[i] = false;
		}
		else
		{
			FS_Read(m_savestrings[i], sizeof(m_savestrings[i]), f);
			FS_FCloseFile(f);
			m_savevalid[i] = true;
		}
	}
}

static void
LoadSave_AdjustPage(int dir)
{
	int i;

	m_loadsave_page += dir;

	if (m_loadsave_page >= MAX_SAVEPAGES)
	{
		m_loadsave_page = 0;
	}
	else if (m_loadsave_page < 0)
	{
		m_loadsave_page = MAX_SAVEPAGES - 1;
	}

	strcpy(m_loadsave_statusbar, "pages: ");

	for (i = 0; i < MAX_SAVEPAGES; i++)
	{
		char *str;
		str = va("%c%d%c",
				i == m_loadsave_page ? '[' : ' ',
				i + 1,
				i == m_loadsave_page ? ']' : ' ');

		if (strlen(m_loadsave_statusbar) + strlen(str) >=
			sizeof(m_loadsave_statusbar))
		{
			break;
		}

		Q_strlcat(m_loadsave_statusbar, str, sizeof(m_loadsave_statusbar));
	}
}

static void
LoadGameCallback(void *self)
{
	menuaction_s *a = (menuaction_s *)self;

	if (a->generic.localdata[0] == -1)
	{
		Cbuf_AddText("load quick\n");
	}
	else
	{
		Cbuf_AddText(va("load save%i\n", a->generic.localdata[0]));
	}

	M_ForceMenuOff();
}

static void
LoadGame_MenuInit(void)
{
	int i;
	float scale = SCR_GetMenuScale();

	s_loadgame_menu.x = viddef.width / 2 - (120 * scale);
	s_loadgame_menu.y = viddef.height / (2 * scale) - 58;
	s_loadgame_menu.nitems = 0;

	Create_Savestrings();

	// The quicksave slot...
	s_loadgame_actions[0].generic.type = MTYPE_ACTION;
	s_loadgame_actions[0].generic.name = m_quicksavestring;
	s_loadgame_actions[0].generic.x = 0;
	s_loadgame_actions[0].generic.y = 0;
	s_loadgame_actions[0].generic.localdata[0] = -1;
	s_loadgame_actions[0].generic.flags = QMF_LEFT_JUSTIFY;

	if (!m_quicksavevalid)
	{
		s_loadgame_actions[0].generic.callback = NULL;
	}
	else
	{
		s_loadgame_actions[0].generic.callback = LoadGameCallback;
	}

	Menu_AddItem(&s_loadgame_menu, &s_loadgame_actions[0]);

	// ...and everything else.
	for (i = 0; i < MAX_SAVESLOTS; i++)
	{
		s_loadgame_actions[i + 1].generic.type = MTYPE_ACTION;
		s_loadgame_actions[i + 1].generic.name = m_savestrings[i];
		s_loadgame_actions[i + 1].generic.x = 0;
		s_loadgame_actions[i + 1].generic.y = i * 10 + 20;
		s_loadgame_actions[i + 1].generic.localdata[0] = i + m_loadsave_page * MAX_SAVESLOTS;
		s_loadgame_actions[i + 1].generic.flags = QMF_LEFT_JUSTIFY;

		if (!m_savevalid[i])
		{
			s_loadgame_actions[i + 1].generic.callback = NULL;
		}
		else
		{
			s_loadgame_actions[i + 1].generic.callback = LoadGameCallback;
		}

		Menu_AddItem(&s_loadgame_menu, &s_loadgame_actions[i + 1]);
	}

	Menu_SetStatusBar(&s_loadgame_menu, m_loadsave_statusbar);
}

static void
LoadGame_MenuDraw(void)
{
	M_Banner("m_banner_load_game");
	Menu_AdjustCursor(&s_loadgame_menu, 1);
	Menu_Draw(&s_loadgame_menu);
}

static const char *
LoadGame_MenuKey(int key)
{
	static menuframework_s *m = &s_loadgame_menu;
	int menu_key = Key_GetMenuKey(key);

	if (menukeyitem_delete)
	{
		if (ExecDeleteSaveFunc(m, menu_key))
		{
			LoadGame_MenuInit();
		}
		return menu_move_sound;
	}

	switch (menu_key)
	{
	case K_UPARROW:
		if (m->cursor == 0)
		{
			LoadSave_AdjustPage(-1);
			LoadGame_MenuInit();
		}
		break;

	case K_DOWNARROW:
		if (m->cursor == m->nitems - 1)
		{
			LoadSave_AdjustPage(1);
			LoadGame_MenuInit();
		}
		break;

	case K_LEFTARROW:
		LoadSave_AdjustPage(-1);
		LoadGame_MenuInit();
		return menu_move_sound;

	case K_RIGHTARROW:
		LoadSave_AdjustPage(1);
		LoadGame_MenuInit();
		return menu_move_sound;

	case K_BACKSPACE:
		PromptDeleteSaveFunc(m);
		return menu_move_sound;

	default:
		s_savegame_menu.cursor = s_loadgame_menu.cursor;
		break;
	}

	return Default_MenuKey(m, key);
}

static void
M_Menu_LoadGame_f(void)
{
	LoadSave_AdjustPage(0);
	LoadGame_MenuInit();
	s_loadgame_menu.draw = LoadGame_MenuDraw;
	s_loadgame_menu.key  = LoadGame_MenuKey;

	M_PushMenu(&s_loadgame_menu);
}

/*
 * SAVEGAME MENU
 */

static void
SaveGameCallback(void *self)
{
	menuaction_s *a = (menuaction_s *)self;

	if (a->generic.localdata[0] == -1)
	{
		m_popup_string = "This slot is reserved for\n"
						 "quicksaving, so please select\n"
						 "another one.";
		m_popup_endtime = cls.realtime + 2000;
		M_Popup();
		return;
	}
	else if (a->generic.localdata[0] == 0)
	{
		m_popup_string = "This slot is reserved for\n"
						 "autosaving, so please select\n"
						 "another one.";
		m_popup_endtime = cls.realtime + 2000;
		M_Popup();
		return;
	}

	Cbuf_AddText(va("save save%i\n", a->generic.localdata[0]));
	M_ForceMenuOff();
}

static void
SaveGame_MenuDraw(void)
{
	M_Banner("m_banner_save_game");
	Menu_AdjustCursor(&s_savegame_menu, 1);
	Menu_Draw(&s_savegame_menu);
	M_Popup();
}

static void
SaveGame_MenuInit(void)
{
	int i;
	float scale = SCR_GetMenuScale();

	s_savegame_menu.x = viddef.width / 2 - (120 * scale);
	s_savegame_menu.y = viddef.height / (2 * scale) - 58;
	s_savegame_menu.nitems = 0;

	Create_Savestrings();

	// The quicksave slot...
	s_savegame_actions[0].generic.type = MTYPE_ACTION;
	s_savegame_actions[0].generic.name = m_quicksavestring;
	s_savegame_actions[0].generic.x = 0;
	s_savegame_actions[0].generic.y = 0;
	s_savegame_actions[0].generic.localdata[0] = -1;
	s_savegame_actions[0].generic.flags = QMF_LEFT_JUSTIFY;
	s_savegame_actions[0].generic.callback = SaveGameCallback;

	Menu_AddItem(&s_savegame_menu, &s_savegame_actions[0]);

	// ...and everything else.
	for (i = 0; i < MAX_SAVESLOTS; i++)
	{
		s_savegame_actions[i + 1].generic.type = MTYPE_ACTION;
		s_savegame_actions[i + 1].generic.name = m_savestrings[i];
		s_savegame_actions[i + 1].generic.x = 0;
		s_savegame_actions[i + 1].generic.y = i * 10 + 20;
		s_savegame_actions[i + 1].generic.localdata[0] = i + m_loadsave_page * MAX_SAVESLOTS;
		s_savegame_actions[i + 1].generic.flags = QMF_LEFT_JUSTIFY;
		s_savegame_actions[i + 1].generic.callback = SaveGameCallback;

		Menu_AddItem(&s_savegame_menu, &s_savegame_actions[i + 1]);
	}

	Menu_SetStatusBar(&s_savegame_menu, m_loadsave_statusbar);
}

static const char *
SaveGame_MenuKey(int key)
{
	static menuframework_s *m = &s_savegame_menu;
	int menu_key = Key_GetMenuKey(key);

	if (m_popup_string)
	{
		m_popup_string = NULL;
		return NULL;
	}

	if (menukeyitem_delete)
	{
		if (ExecDeleteSaveFunc(m, menu_key))
		{
			SaveGame_MenuInit();
		}
		return menu_move_sound;
	}

	switch (menu_key)
	{
	case K_UPARROW:
		if (m->cursor == 0)
		{
			LoadSave_AdjustPage(-1);
			SaveGame_MenuInit();
		}
		break;

	case K_DOWNARROW:
		if (m->cursor == m->nitems - 1)
		{
			LoadSave_AdjustPage(1);
			SaveGame_MenuInit();
		}
		break;

	case K_LEFTARROW:
		LoadSave_AdjustPage(-1);
		SaveGame_MenuInit();
		return menu_move_sound;

	case K_RIGHTARROW:
		LoadSave_AdjustPage(1);
		SaveGame_MenuInit();
		return menu_move_sound;

	case K_BACKSPACE:
		PromptDeleteSaveFunc(m);
		return menu_move_sound;

	default:
		s_loadgame_menu.cursor = s_savegame_menu.cursor;
		break;
	}

	return Default_MenuKey(m, key);
}

static void
M_Menu_SaveGame_f(void)
{
	if (!Com_ServerState())
	{
		return; /* not playing a game */
	}

	LoadSave_AdjustPage(0);
	SaveGame_MenuInit();
	s_savegame_menu.draw = SaveGame_MenuDraw;
	s_savegame_menu.key  = SaveGame_MenuKey;

	M_PushMenu(&s_savegame_menu);
}

/*
 * JOIN SERVER MENU
 */

#define MAX_LOCAL_SERVERS 8

static menuframework_s s_joinserver_menu;
static menuseparator_s s_joinserver_server_title;
static menuaction_s s_joinserver_search_action;
static menuaction_s s_joinserver_address_book_action;
static menuaction_s s_joinserver_server_actions[MAX_LOCAL_SERVERS];

int m_num_servers;
#define NO_SERVER_STRING "<no server>"

/* network address */
static netadr_t local_server_netadr[MAX_LOCAL_SERVERS];

/* user readable information */
static char local_server_names[MAX_LOCAL_SERVERS][80];
static char local_server_netadr_strings[MAX_LOCAL_SERVERS][80];

void
M_AddToServerList(netadr_t adr, char *info)
{
	const char *s;
	int i;

	if (m_num_servers == MAX_LOCAL_SERVERS)
	{
		return;
	}

	while (*info == ' ')
	{
		info++;
	}

	s = NET_AdrToString(adr);

	/* ignore if duplicated */
	for (i = 0; i < m_num_servers; i++)
	{
		if (!strcmp(local_server_names[i], info) &&
			!strcmp(local_server_netadr_strings[i], s))
		{
			return;
		}
	}

	local_server_netadr[m_num_servers] = adr;
	Q_strlcpy(local_server_names[m_num_servers], info,
			sizeof(local_server_names[m_num_servers]));
	Q_strlcpy(local_server_netadr_strings[m_num_servers], s,
			sizeof(local_server_netadr_strings[m_num_servers]));
	m_num_servers++;
}

static void
JoinServerFunc(void *self)
{
	char buffer[128];
	int index;

	index = (int)((menuaction_s *)self - s_joinserver_server_actions);

	if (Q_stricmp(local_server_names[index], NO_SERVER_STRING) == 0)
	{
		return;
	}

	if (index >= m_num_servers)
	{
		return;
	}

	Com_sprintf(buffer, sizeof(buffer), "connect %s\n",
				NET_AdrToString(local_server_netadr[index]));
	Cbuf_AddText(buffer);
	M_ForceMenuOff();
}

static void
AddressBookFunc(void *self)
{
	M_Menu_AddressBook_f();
}

static void
SearchLocalGames(void)
{
	int i;

	m_num_servers = 0;

	for (i = 0; i < MAX_LOCAL_SERVERS; i++)
	{
		strcpy(local_server_names[i], NO_SERVER_STRING);
		local_server_netadr_strings[i][0] = '\0';
	}

	m_popup_string = "Searching for local servers. This\n"
					 "could take up to a minute, so\n"
					 "please be patient.";
	m_popup_endtime = cls.realtime + 2000;
	M_Popup();

	/* the text box won't show up unless we do a buffer swap */
	R_EndFrame();

	/* send out info packets */
	CL_PingServers_f();
}

static void
SearchLocalGamesFunc(void *self)
{
	SearchLocalGames();
}

static void
JoinServer_MenuInit(void)
{
	int i;
	float scale = SCR_GetMenuScale();

	s_joinserver_menu.x = (int)(viddef.width * 0.50f) - 120 * scale;
	s_joinserver_menu.nitems = 0;

	s_joinserver_address_book_action.generic.type = MTYPE_ACTION;
	s_joinserver_address_book_action.generic.name = "address book";
	s_joinserver_address_book_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joinserver_address_book_action.generic.x = 0;
	s_joinserver_address_book_action.generic.y = 0;
	s_joinserver_address_book_action.generic.callback = AddressBookFunc;

	s_joinserver_search_action.generic.type = MTYPE_ACTION;
	s_joinserver_search_action.generic.name = "refresh server list";
	s_joinserver_search_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joinserver_search_action.generic.x = 0;
	s_joinserver_search_action.generic.y = 10;
	s_joinserver_search_action.generic.callback = SearchLocalGamesFunc;
	s_joinserver_search_action.generic.statusbar = "search for servers";

	s_joinserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joinserver_server_title.generic.name = "connect to...";
	s_joinserver_server_title.generic.y = 30;
	s_joinserver_server_title.generic.x = 80 * scale;

	for (i = 0; i < MAX_LOCAL_SERVERS; i++)
	{
		s_joinserver_server_actions[i].generic.type = MTYPE_ACTION;
		s_joinserver_server_actions[i].generic.name = local_server_names[i];
		s_joinserver_server_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_joinserver_server_actions[i].generic.x = 0;
		s_joinserver_server_actions[i].generic.y = 40 + i * 10;
		s_joinserver_server_actions[i].generic.callback = JoinServerFunc;
		s_joinserver_server_actions[i].generic.statusbar =
			local_server_netadr_strings[i];
	}

	Menu_AddItem(&s_joinserver_menu, &s_joinserver_address_book_action);
	Menu_AddItem(&s_joinserver_menu, &s_joinserver_server_title);
	Menu_AddItem(&s_joinserver_menu, &s_joinserver_search_action);

	for (i = 0; i < 8; i++)
	{
		Menu_AddItem(&s_joinserver_menu, &s_joinserver_server_actions[i]);
	}

	Menu_Center(&s_joinserver_menu);

	SearchLocalGames();
}

static void
JoinServer_MenuDraw(void)
{
	M_Banner("m_banner_join_server");
	Menu_Draw(&s_joinserver_menu);
	M_Popup();
}

static const char *
JoinServer_MenuKey(int key)
{
	if (m_popup_string)
	{
		m_popup_string = NULL;
		return NULL;
	}
	return Default_MenuKey(&s_joinserver_menu, key);
}

static void
M_Menu_JoinServer_f(void)
{
	JoinServer_MenuInit();
	s_joinserver_menu.draw = JoinServer_MenuDraw;
	s_joinserver_menu.key  = JoinServer_MenuKey;

	M_PushMenu(&s_joinserver_menu);
}

/*
 * START SERVER MENU
 */

static menuframework_s s_startserver_menu;
static char **mapnames = NULL;
static int nummaps;

static menuaction_s s_startserver_start_action;
static menuaction_s s_startserver_dmoptions_action;
static menufield_s s_timelimit_field;
static menufield_s s_fraglimit_field;
static menufield_s s_capturelimit_field;
static menufield_s s_maxclients_field;
static menufield_s s_hostname_field;
static menulist_s s_startmap_list;
static menulist_s s_rules_box;

static void
DMOptionsFunc(void *self)
{
	M_Menu_DMOptions_f();
}

static void
RulesChangeFunc(void *self)
{
	/* Deathmatch */
	if (s_rules_box.curvalue == 0)
	{
		s_maxclients_field.generic.statusbar = NULL;
		s_startserver_dmoptions_action.generic.statusbar = NULL;
	}

	/* Ground Zero game modes */
	else if (M_IsGame("rogue"))
	{
		if (s_rules_box.curvalue == 2)
		{
			s_maxclients_field.generic.statusbar = NULL;
			s_startserver_dmoptions_action.generic.statusbar = NULL;
		}
	}
}

static void
StartServerActionFunc(void *self)
{
	char startmap[1024];
	float timelimit;
	float fraglimit;
	float maxclients;
	char *spot;

	Q_strlcpy(startmap, strchr(mapnames[s_startmap_list.curvalue], '\n') + 1,
		sizeof(startmap));

	maxclients = (float)strtod(s_maxclients_field.buffer, (char **)NULL);
	timelimit = (float)strtod(s_timelimit_field.buffer, (char **)NULL);
	fraglimit = (float)strtod(s_fraglimit_field.buffer, (char **)NULL);

	if (M_IsGame("ctf"))
	{
		float capturelimit;

		capturelimit = (float)strtod(s_capturelimit_field.buffer, (char **)NULL);
		Cvar_SetValue("capturelimit", ClampCvar(0, capturelimit, capturelimit));
	}

	Cvar_SetValue("maxclients", ClampCvar(0, maxclients, maxclients));
	Cvar_SetValue("timelimit", ClampCvar(0, timelimit, timelimit));
	Cvar_SetValue("fraglimit", ClampCvar(0, fraglimit, fraglimit));
	Cvar_Set("hostname", s_hostname_field.buffer);

	Cvar_SetValue("singleplayer", 0);

	if ((s_rules_box.curvalue < 2) || M_IsGame("rogue"))
	{
		Cvar_SetValue("deathmatch", (float)!s_rules_box.curvalue);
		Cvar_SetValue("coop", (float)s_rules_box.curvalue);
	}
	else
	{
		Cvar_SetValue("deathmatch", 1); /* deathmatch is always true for rogue games */
		Cvar_SetValue("coop", 0); /* This works for at least the main game and both addons */
	}

	spot = NULL;

	if (s_rules_box.curvalue == 1)
	{
		if (Q_stricmp(startmap, "bunk1") == 0)
		{
			spot = "start";
		}

		else if (Q_stricmp(startmap, "mintro") == 0)
		{
			spot = "start";
		}

		else if (Q_stricmp(startmap, "fact1") == 0)
		{
			spot = "start";
		}

		else if (Q_stricmp(startmap, "power1") == 0)
		{
			spot = "pstart";
		}

		else if (Q_stricmp(startmap, "biggun") == 0)
		{
			spot = "bstart";
		}

		else if (Q_stricmp(startmap, "hangar1") == 0)
		{
			spot = "unitstart";
		}

		else if (Q_stricmp(startmap, "city1") == 0)
		{
			spot = "unitstart";
		}

		else if (Q_stricmp(startmap, "boss1") == 0)
		{
			spot = "bosstart";
		}
	}

	if (spot)
	{
		if (Com_ServerState())
		{
			Cbuf_AddText("disconnect\n");
		}

		Cbuf_AddText(va("gamemap \"*%s$%s\"\n", startmap, spot));
	}
	else
	{
		Cbuf_AddText(va("map %s\n", startmap));
	}

	M_ForceMenuOff();
}

void
CleanCachedMapsList(void)
{
	if (mapnames != NULL)
	{
		size_t i;

		for (i = 0; i < nummaps; i++)
		{
			free(mapnames[i]);
		}

		free(mapnames);
		mapnames = NULL;
	}
}

static char**
GetMapsList(int *num)
{
	int length;
	char *buffer;

	/* load the list of map names */
	if ((length = FS_LoadFile("maps.lst", (void **)&buffer)) != -1)
	{
		char **mapnames = NULL;
		size_t nummapslen;
		int i, nummaps = 0;

		char *s;

		s = buffer;
		i = 0;

		while (i < length)
		{
			if (s[i] == '\n')
			{
				nummaps++;
			}

			i++;
		}

		if (nummaps == 0)
		{
			Com_Printf("no maps in maps.lst\n");
			/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
			return NULL;
		}

		nummapslen = sizeof(char *) * (nummaps + 1);
		mapnames = malloc(nummapslen);

		YQ2_COM_CHECK_OOM(mapnames, "malloc(sizeof(char *) * (nummaps + 1))", nummapslen)
		if (!mapnames)
		{
			/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
			return NULL;
		}

		memset(mapnames, 0, nummapslen);

		s = buffer;

		for (i = 0; i < nummaps; i++)
		{
			char shortname[MAX_TOKEN_CHARS];
			char longname[MAX_TOKEN_CHARS];
			char scratch[200];
			size_t j, l;

			Q_strlcpy(shortname, COM_Parse(&s), sizeof(shortname));
			l = strlen(shortname);

			for (j = 0; j < l; j++)
			{
				shortname[j] = toupper((unsigned char)shortname[j]);
			}

			Q_strlcpy(longname, COM_Parse(&s), sizeof(longname));
			Com_sprintf(scratch, sizeof(scratch), "%s\n%s", longname, shortname);

			mapnames[i] = strdup(scratch);
			YQ2_COM_CHECK_OOM(mapnames[i], "strdup(scratch)", strlen(scratch)+1)
			if (!mapnames[i])
			{
				free(mapnames);
				/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
				return NULL;
			}
		}

		mapnames[nummaps] = NULL;
		FS_FreeFile(buffer);

		*num = nummaps;
		return mapnames;
	}

	return NULL;
}

static char**
GetMapsInFolderList(int *nummaps)
{
	/* Generate list by bsp files in maps/ directory */
	size_t nummapslen;
	char **list = NULL, **mapnames = NULL;
	int num = 0, i;

	list = FS_ListFiles2("maps/*.bsp", &num, 0, 0);
	if (!list)
	{
		Com_Printf("couldn't find maps/*.bsp\n");
		/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
		return NULL;
	}

	nummapslen = sizeof(char *) * (num);
	mapnames = malloc(nummapslen);
	YQ2_COM_CHECK_OOM(mapnames, "malloc(sizeof(char *) * (num))", nummapslen)
	if (!mapnames)
	{
		FS_FreeList(list, num);
		/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
		return NULL;
	}

	memset(mapnames, 0, nummapslen);

	for (i = 0; i < num - 1; i++)
	{
		char scratch[200], shortname[MAX_QPATH];
		int len;

		len = strlen(list[i]);
		if (len > 9 && len < MAX_QPATH)
		{
			/* maps/ + .bsp */
			Q_strlcpy(shortname, list[i] + 5, sizeof(shortname));
			shortname[len - 9]  = 0;

			Com_sprintf(scratch, sizeof(scratch), "%s\n%s", shortname, shortname);

			mapnames[i] = strdup(scratch);
			YQ2_COM_CHECK_OOM(mapnames[i], "strdup(scratch)", strlen(scratch)+1)
			if (!mapnames[i])
			{
				/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
				return NULL;
			}
		}
	}

	mapnames[num - 1] = NULL;

	/* sort maps names alphabetically */
	qsort(mapnames, num - 1, sizeof(char*), Q_sort_stricmp);

	/* free file list */
	FS_FreeList(list, num);
	*nummaps = num - 1;

	return mapnames;
}

static char**
GetCombinedMapsList(int *nummaps)
{
	char **mapnames_list = NULL, **mapnames_folder = NULL, **mapnames = NULL;
	int nummaps_list = 0, nummaps_folder = 0;
	size_t nummapslen, currpos;

	mapnames_folder = GetMapsInFolderList(&nummaps_folder);
	if (!mapnames_folder)
	{
		/* no maps at all? */
		return NULL;
	}

	mapnames_list = GetMapsList(&nummaps_list);
	if (!mapnames_list)
	{
		/* no maps in list? */
		*nummaps = nummaps_folder;
		return mapnames_folder;
	}

	/* we have maps in file and in folder */
	nummapslen = sizeof(char *) * (nummaps_list + nummaps_folder + 1);
	mapnames = malloc(nummapslen);
	YQ2_COM_CHECK_OOM(mapnames, "malloc(sizeof(char *) * (num))", nummapslen)
	if (!mapnames)
	{
		size_t i;

		for (i = 0; i < nummaps_list; i++)
		{
			free(mapnames_list[i]);
		}

		free(mapnames_list);
		/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
		*nummaps = nummaps_folder;
		return mapnames_folder;
	}

	memset(mapnames, 0, nummapslen);
	memcpy(mapnames, mapnames_list, sizeof(char *) * nummaps_list);
	*nummaps = nummaps_list;
	free(mapnames_list);

	for (currpos = 0; currpos < nummaps_folder; currpos ++)
	{
		qboolean found;
		char *foldername;
		size_t i;

		foldername = strchr(mapnames_folder[currpos], '\n');
		if (!foldername)
		{
			free(mapnames_folder[currpos]);
			continue;
		}
		foldername++;

		found = false;
		for (i = 0; i < *nummaps; i++)
		{
			char *currname;

			currname = strchr(mapnames[i], '\n');
			if (!currname)
			{
				continue;
			}
			currname++;

			if (!Q_stricmp(currname, foldername))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			mapnames[*nummaps] = mapnames_folder[currpos];
			(*nummaps) ++;
		}
		else
		{
			free(mapnames_folder[currpos]);
		}
	}

	mapnames[*nummaps] = NULL;

	free(mapnames_folder);
	return mapnames;
}

static void
StartServer_MenuInit(void)
{
	static const char *dm_coop_names[] =
	{
		"deathmatch",
		"cooperative",
		0
	};
	static const char *dm_coop_names_rogue[] =
	{
		"deathmatch",
		"cooperative",
		"tag",
		0
	};

	float scale = SCR_GetMenuScale();

	/* initialize list of maps once, reuse it afterwards (=> it isn't freed unless the game dir is changed) */
	if (mapnames == NULL)
	{
		nummaps = 0;
		s_startmap_list.curvalue = 0;

		mapnames = GetCombinedMapsList(&nummaps);

		if (!mapnames || !nummaps)
		{
			Com_Error(ERR_DROP, "no maps in maps.lst\n");
			return;
		}
	}

	/* initialize the menu stuff */
	s_startserver_menu.x = (int)(viddef.width * 0.50f);
	s_startserver_menu.nitems = 0;

	s_startmap_list.generic.type = MTYPE_SPINCONTROL;
	s_startmap_list.generic.x = 0;

	if (M_IsGame("ctf"))
		s_startmap_list.generic.y = -8;
	else
		s_startmap_list.generic.y = 0;

	s_startmap_list.generic.name = "initial map";
	s_startmap_list.itemnames = (const char **)mapnames;

	if (M_IsGame("ctf"))
	{
		s_capturelimit_field.generic.type = MTYPE_FIELD;
		s_capturelimit_field.generic.name = "capture limit";
		s_capturelimit_field.generic.flags = QMF_NUMBERSONLY;
		s_capturelimit_field.generic.x = 0;
		s_capturelimit_field.generic.y = 18;
		s_capturelimit_field.generic.statusbar = "0 = no limit";
		s_capturelimit_field.length = 3;
		s_capturelimit_field.visible_length = 3;
		Q_strlcpy(s_capturelimit_field.buffer, Cvar_VariableString("capturelimit"),
			sizeof(s_capturelimit_field.buffer));
	}
	else
	{
		s_rules_box.generic.type = MTYPE_SPINCONTROL;
		s_rules_box.generic.x = 0;
		s_rules_box.generic.y = 20;
		s_rules_box.generic.name = "rules";

		/* Ground Zero games only available with rogue game */
		if (M_IsGame("rogue"))
		{
			s_rules_box.itemnames = dm_coop_names_rogue;
		}
		else
		{
			s_rules_box.itemnames = dm_coop_names;
		}

		if (Cvar_VariableValue("coop"))
		{
			s_rules_box.curvalue = 1;
		}
		else
		{
			s_rules_box.curvalue = 0;
		}

		s_rules_box.generic.callback = RulesChangeFunc;
	}

	s_timelimit_field.generic.type = MTYPE_FIELD;
	s_timelimit_field.generic.name = "time limit";
	s_timelimit_field.generic.flags = QMF_NUMBERSONLY;
	s_timelimit_field.generic.x = 0;
	s_timelimit_field.generic.y = 36;
	s_timelimit_field.generic.statusbar = "0 = no limit";
	s_timelimit_field.length = 3;
	s_timelimit_field.visible_length = 3;
	Q_strlcpy(s_timelimit_field.buffer, Cvar_VariableString("timelimit"),
		sizeof(s_timelimit_field.buffer));

	s_fraglimit_field.generic.type = MTYPE_FIELD;
	s_fraglimit_field.generic.name = "frag limit";
	s_fraglimit_field.generic.flags = QMF_NUMBERSONLY;
	s_fraglimit_field.generic.x = 0;
	s_fraglimit_field.generic.y = 54;
	s_fraglimit_field.generic.statusbar = "0 = no limit";
	s_fraglimit_field.length = 3;
	s_fraglimit_field.visible_length = 3;
	Q_strlcpy(s_fraglimit_field.buffer, Cvar_VariableString("fraglimit"),
		sizeof(s_fraglimit_field.buffer));

	/* maxclients determines the maximum number of players that can join
	   the game. If maxclients is only "1" then we should default the menu
	   option to 8 players, otherwise use whatever its current value is.
	   Clamping will be done when the server is actually started. */
	s_maxclients_field.generic.type = MTYPE_FIELD;
	s_maxclients_field.generic.name = "max players";
	s_maxclients_field.generic.flags = QMF_NUMBERSONLY;
	s_maxclients_field.generic.x = 0;
	s_maxclients_field.generic.y = 72;
	s_maxclients_field.generic.statusbar = NULL;
	s_maxclients_field.length = 3;
	s_maxclients_field.visible_length = 3;

	if (Cvar_VariableValue("maxclients") == 1)
	{
		strcpy(s_maxclients_field.buffer, "8");
	}
	else
	{
		Q_strlcpy(s_maxclients_field.buffer, Cvar_VariableString("maxclients"),
			sizeof(s_maxclients_field.buffer));
	}

	s_hostname_field.generic.type = MTYPE_FIELD;
	s_hostname_field.generic.name = "hostname";
	s_hostname_field.generic.flags = 0;
	s_hostname_field.generic.x = 0;
	s_hostname_field.generic.y = 90;
	s_hostname_field.generic.statusbar = NULL;
	s_hostname_field.length = 12;
	s_hostname_field.visible_length = 12;
	Q_strlcpy(s_hostname_field.buffer, Cvar_VariableString("hostname"),
		sizeof(s_hostname_field.buffer));
	s_hostname_field.cursor = strlen(s_hostname_field.buffer);

	s_startserver_dmoptions_action.generic.type = MTYPE_ACTION;
	s_startserver_dmoptions_action.generic.name = " deathmatch flags";
	s_startserver_dmoptions_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_startserver_dmoptions_action.generic.x = 24 * scale;
	s_startserver_dmoptions_action.generic.y = 108;
	s_startserver_dmoptions_action.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.callback = DMOptionsFunc;

	s_startserver_start_action.generic.type = MTYPE_ACTION;
	s_startserver_start_action.generic.name = " begin";
	s_startserver_start_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_startserver_start_action.generic.x = 24 * scale;
	s_startserver_start_action.generic.y = 128;
	s_startserver_start_action.generic.callback = StartServerActionFunc;

	Menu_AddItem(&s_startserver_menu, &s_startmap_list);

	if (M_IsGame("ctf"))
	{
		Menu_AddItem(&s_startserver_menu, &s_capturelimit_field);
	}
	else
	{
		Menu_AddItem(&s_startserver_menu, &s_rules_box);
	}

	Menu_AddItem(&s_startserver_menu, &s_timelimit_field);
	Menu_AddItem(&s_startserver_menu, &s_fraglimit_field);
	Menu_AddItem(&s_startserver_menu, &s_maxclients_field);
	Menu_AddItem(&s_startserver_menu, &s_hostname_field);
	Menu_AddItem(&s_startserver_menu, &s_startserver_dmoptions_action);
	Menu_AddItem(&s_startserver_menu, &s_startserver_start_action);

	Menu_Center(&s_startserver_menu);

	/* call this now to set proper inital state */
	RulesChangeFunc(NULL);
}

static void
StartServer_MenuDraw(void)
{
	Menu_Draw(&s_startserver_menu);
}

static const char *
StartServer_MenuKey(int key)
{
	return Default_MenuKey(&s_startserver_menu, key);
}

static void
M_Menu_StartServer_f(void)
{
	StartServer_MenuInit();
	s_startserver_menu.draw = StartServer_MenuDraw;
	s_startserver_menu.key  = StartServer_MenuKey;

	M_PushMenu(&s_startserver_menu);
}

/*
 * DMOPTIONS BOOK MENU
 */

static char dmoptions_statusbar[128];

static menuframework_s s_dmoptions_menu;

static menulist_s s_friendlyfire_box;
static menulist_s s_falls_box;
static menulist_s s_weapons_stay_box;
static menulist_s s_instant_powerups_box;
static menulist_s s_powerups_box;
static menulist_s s_health_box;
static menulist_s s_spawn_farthest_box;
static menulist_s s_teamplay_box;
static menulist_s s_samelevel_box;
static menulist_s s_force_respawn_box;
static menulist_s s_armor_box;
static menulist_s s_allow_exit_box;
static menulist_s s_infinite_ammo_box;
static menulist_s s_fixed_fov_box;
static menulist_s s_quad_drop_box;
static menulist_s s_no_mines_box;
static menulist_s s_no_nukes_box;
static menulist_s s_stack_double_box;
static menulist_s s_no_spheres_box;

static void
DMFlagCallback(void *self)
{
	const menulist_s *f = (menulist_s *)self;
	int flags;
	int bit = 0;

	flags = Cvar_VariableValue("dmflags");

	if (f == &s_friendlyfire_box)
	{
		if (f->curvalue)
		{
			flags &= ~DF_NO_FRIENDLY_FIRE;
		}

		else
		{
			flags |= DF_NO_FRIENDLY_FIRE;
		}

		goto setvalue;
	}
	else if (f == &s_falls_box)
	{
		if (f->curvalue)
		{
			flags &= ~DF_NO_FALLING;
		}

		else
		{
			flags |= DF_NO_FALLING;
		}

		goto setvalue;
	}
	else if (f == &s_weapons_stay_box)
	{
		bit = DF_WEAPONS_STAY;
	}
	else if (f == &s_instant_powerups_box)
	{
		bit = DF_INSTANT_ITEMS;
	}
	else if (f == &s_allow_exit_box)
	{
		bit = DF_ALLOW_EXIT;
	}
	else if (f == &s_powerups_box)
	{
		if (f->curvalue)
		{
			flags &= ~DF_NO_ITEMS;
		}

		else
		{
			flags |= DF_NO_ITEMS;
		}

		goto setvalue;
	}
	else if (f == &s_health_box)
	{
		if (f->curvalue)
		{
			flags &= ~DF_NO_HEALTH;
		}

		else
		{
			flags |= DF_NO_HEALTH;
		}

		goto setvalue;
	}
	else if (f == &s_spawn_farthest_box)
	{
		bit = DF_SPAWN_FARTHEST;
	}
	else if (f == &s_teamplay_box)
	{
		if (f->curvalue == 1)
		{
			flags |= DF_SKINTEAMS;
			flags &= ~DF_MODELTEAMS;
		}
		else if (f->curvalue == 2)
		{
			flags |= DF_MODELTEAMS;
			flags &= ~DF_SKINTEAMS;
		}
		else
		{
			flags &= ~(DF_MODELTEAMS | DF_SKINTEAMS);
		}

		goto setvalue;
	}
	else if (f == &s_samelevel_box)
	{
		bit = DF_SAME_LEVEL;
	}
	else if (f == &s_force_respawn_box)
	{
		bit = DF_FORCE_RESPAWN;
	}
	else if (f == &s_armor_box)
	{
		if (f->curvalue)
		{
			flags &= ~DF_NO_ARMOR;
		}

		else
		{
			flags |= DF_NO_ARMOR;
		}

		goto setvalue;
	}
	else if (f == &s_infinite_ammo_box)
	{
		bit = DF_INFINITE_AMMO;
	}
	else if (f == &s_fixed_fov_box)
	{
		bit = DF_FIXED_FOV;
	}
	else if (f == &s_quad_drop_box)
	{
		bit = DF_QUAD_DROP;
	}
	else if (M_IsGame("rogue"))
	{
		if (f == &s_no_mines_box)
		{
			bit = DF_NO_MINES;
		}
		else if (f == &s_no_nukes_box)
		{
			bit = DF_NO_NUKES;
		}
		else if (f == &s_stack_double_box)
		{
			bit = DF_NO_STACK_DOUBLE;
		}
		else if (f == &s_no_spheres_box)
		{
			bit = DF_NO_SPHERES;
		}
	}
	else if (M_IsGame("ctf"))
	{
		if (f == &s_no_mines_box)
		{
			bit = DF_NO_MINES;			/* Equivalent to DF_CTF_FORCEJOIN in CTF */
		}
		else if (f == &s_no_nukes_box)
		{
			bit = DF_NO_NUKES;			/* Equivalent to DF_CTF_NO_TECH   in CTF */
		}
		else if (f == &s_stack_double_box)
		{
			bit = DF_NO_STACK_DOUBLE;	/* Equivalent to DF_ARMOR_PROTECT in CTF */
		}
	}

	if (f)
	{
		if (f->curvalue == 0)
		{
			flags &= ~bit;
		}

		else
		{
			flags |= bit;
		}
	}

setvalue:
	Cvar_SetValue("dmflags", (float)flags);

	Com_sprintf(dmoptions_statusbar, sizeof(dmoptions_statusbar),
				"dmflags = %d", flags);
}

static void
DMOptions_MenuInit(void)
{
	static const char *yes_no_names[] =
	{
		"no", "yes", 0
	};
	static const char *teamplay_names[] =
	{
		"disabled", "by skin", "by model", 0
	};
	int dmflags = Cvar_VariableValue("dmflags");
	unsigned short int y = 0;

	s_dmoptions_menu.x = (int)(viddef.width * 0.50f);
	s_dmoptions_menu.nitems = 0;

	s_falls_box.generic.type = MTYPE_SPINCONTROL;
	s_falls_box.generic.x = 0;
	s_falls_box.generic.y = y;
	s_falls_box.generic.name = "falling damage";
	s_falls_box.generic.callback = DMFlagCallback;
	s_falls_box.itemnames = yes_no_names;
	s_falls_box.curvalue = (dmflags & DF_NO_FALLING) == 0;

	s_weapons_stay_box.generic.type = MTYPE_SPINCONTROL;
	s_weapons_stay_box.generic.x = 0;
	s_weapons_stay_box.generic.y = y += 10;
	s_weapons_stay_box.generic.name = "weapons stay";
	s_weapons_stay_box.generic.callback = DMFlagCallback;
	s_weapons_stay_box.itemnames = yes_no_names;
	s_weapons_stay_box.curvalue = (dmflags & DF_WEAPONS_STAY) != 0;

	s_instant_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_instant_powerups_box.generic.x = 0;
	s_instant_powerups_box.generic.y = y += 10;
	s_instant_powerups_box.generic.name = "instant powerups";
	s_instant_powerups_box.generic.callback = DMFlagCallback;
	s_instant_powerups_box.itemnames = yes_no_names;
	s_instant_powerups_box.curvalue = (dmflags & DF_INSTANT_ITEMS) != 0;

	s_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_powerups_box.generic.x = 0;
	s_powerups_box.generic.y = y += 10;
	s_powerups_box.generic.name = "allow powerups";
	s_powerups_box.generic.callback = DMFlagCallback;
	s_powerups_box.itemnames = yes_no_names;
	s_powerups_box.curvalue = (dmflags & DF_NO_ITEMS) == 0;

	s_health_box.generic.type = MTYPE_SPINCONTROL;
	s_health_box.generic.x = 0;
	s_health_box.generic.y = y += 10;
	s_health_box.generic.callback = DMFlagCallback;
	s_health_box.generic.name = "allow health";
	s_health_box.itemnames = yes_no_names;
	s_health_box.curvalue = (dmflags & DF_NO_HEALTH) == 0;

	s_armor_box.generic.type = MTYPE_SPINCONTROL;
	s_armor_box.generic.x = 0;
	s_armor_box.generic.y = y += 10;
	s_armor_box.generic.name = "allow armor";
	s_armor_box.generic.callback = DMFlagCallback;
	s_armor_box.itemnames = yes_no_names;
	s_armor_box.curvalue = (dmflags & DF_NO_ARMOR) == 0;

	s_spawn_farthest_box.generic.type = MTYPE_SPINCONTROL;
	s_spawn_farthest_box.generic.x = 0;
	s_spawn_farthest_box.generic.y = y += 10;
	s_spawn_farthest_box.generic.name = "spawn farthest";
	s_spawn_farthest_box.generic.callback = DMFlagCallback;
	s_spawn_farthest_box.itemnames = yes_no_names;
	s_spawn_farthest_box.curvalue = (dmflags & DF_SPAWN_FARTHEST) != 0;

	s_samelevel_box.generic.type = MTYPE_SPINCONTROL;
	s_samelevel_box.generic.x = 0;
	s_samelevel_box.generic.y = y += 10;
	s_samelevel_box.generic.name = "same map";
	s_samelevel_box.generic.callback = DMFlagCallback;
	s_samelevel_box.itemnames = yes_no_names;
	s_samelevel_box.curvalue = (dmflags & DF_SAME_LEVEL) != 0;

	s_force_respawn_box.generic.type = MTYPE_SPINCONTROL;
	s_force_respawn_box.generic.x = 0;
	s_force_respawn_box.generic.y = y += 10;
	s_force_respawn_box.generic.name = "force respawn";
	s_force_respawn_box.generic.callback = DMFlagCallback;
	s_force_respawn_box.itemnames = yes_no_names;
	s_force_respawn_box.curvalue = (dmflags & DF_FORCE_RESPAWN) != 0;

	if (!M_IsGame("ctf"))
	{
		s_teamplay_box.generic.type = MTYPE_SPINCONTROL;
		s_teamplay_box.generic.x = 0;
		s_teamplay_box.generic.y = y += 10;
		s_teamplay_box.generic.name = "teamplay";
		s_teamplay_box.generic.callback = DMFlagCallback;
		s_teamplay_box.itemnames = teamplay_names;
	}

	s_allow_exit_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_exit_box.generic.x = 0;
	s_allow_exit_box.generic.y = y += 10;
	s_allow_exit_box.generic.name = "allow exit";
	s_allow_exit_box.generic.callback = DMFlagCallback;
	s_allow_exit_box.itemnames = yes_no_names;
	s_allow_exit_box.curvalue = (dmflags & DF_ALLOW_EXIT) != 0;

	s_infinite_ammo_box.generic.type = MTYPE_SPINCONTROL;
	s_infinite_ammo_box.generic.x = 0;
	s_infinite_ammo_box.generic.y = y += 10;
	s_infinite_ammo_box.generic.name = "infinite ammo";
	s_infinite_ammo_box.generic.callback = DMFlagCallback;
	s_infinite_ammo_box.itemnames = yes_no_names;
	s_infinite_ammo_box.curvalue = (dmflags & DF_INFINITE_AMMO) != 0;

	s_fixed_fov_box.generic.type = MTYPE_SPINCONTROL;
	s_fixed_fov_box.generic.x = 0;
	s_fixed_fov_box.generic.y = y += 10;
	s_fixed_fov_box.generic.name = "fixed FOV";
	s_fixed_fov_box.generic.callback = DMFlagCallback;
	s_fixed_fov_box.itemnames = yes_no_names;
	s_fixed_fov_box.curvalue = (dmflags & DF_FIXED_FOV) != 0;

	s_quad_drop_box.generic.type = MTYPE_SPINCONTROL;
	s_quad_drop_box.generic.x = 0;
	s_quad_drop_box.generic.y = y += 10;
	s_quad_drop_box.generic.name = "quad drop";
	s_quad_drop_box.generic.callback = DMFlagCallback;
	s_quad_drop_box.itemnames = yes_no_names;
	s_quad_drop_box.curvalue = (dmflags & DF_QUAD_DROP) != 0;

	if (!M_IsGame("ctf"))
	{
		s_friendlyfire_box.generic.type = MTYPE_SPINCONTROL;
		s_friendlyfire_box.generic.x = 0;
		s_friendlyfire_box.generic.y = y += 10;
		s_friendlyfire_box.generic.name = "friendly fire";
		s_friendlyfire_box.generic.callback = DMFlagCallback;
		s_friendlyfire_box.itemnames = yes_no_names;
		s_friendlyfire_box.curvalue = (dmflags & DF_NO_FRIENDLY_FIRE) == 0;
	}

	if (M_IsGame("rogue"))
	{
		s_no_mines_box.generic.type = MTYPE_SPINCONTROL;
		s_no_mines_box.generic.x = 0;
		s_no_mines_box.generic.y = y += 10;
		s_no_mines_box.generic.name = "remove mines";
		s_no_mines_box.generic.callback = DMFlagCallback;
		s_no_mines_box.itemnames = yes_no_names;
		s_no_mines_box.curvalue = (dmflags & DF_NO_MINES) != 0;

		s_no_nukes_box.generic.type = MTYPE_SPINCONTROL;
		s_no_nukes_box.generic.x = 0;
		s_no_nukes_box.generic.y = y += 10;
		s_no_nukes_box.generic.name = "remove nukes";
		s_no_nukes_box.generic.callback = DMFlagCallback;
		s_no_nukes_box.itemnames = yes_no_names;
		s_no_nukes_box.curvalue = (dmflags & DF_NO_NUKES) != 0;

		s_stack_double_box.generic.type = MTYPE_SPINCONTROL;
		s_stack_double_box.generic.x = 0;
		s_stack_double_box.generic.y = y += 10;
		s_stack_double_box.generic.name = "2x/4x stacking off";
		s_stack_double_box.generic.callback = DMFlagCallback;
		s_stack_double_box.itemnames = yes_no_names;
		s_stack_double_box.curvalue = (dmflags & DF_NO_STACK_DOUBLE) != 0;

		s_no_spheres_box.generic.type = MTYPE_SPINCONTROL;
		s_no_spheres_box.generic.x = 0;
		s_no_spheres_box.generic.y = y += 10;
		s_no_spheres_box.generic.name = "remove spheres";
		s_no_spheres_box.generic.callback = DMFlagCallback;
		s_no_spheres_box.itemnames = yes_no_names;
		s_no_spheres_box.curvalue = (dmflags & DF_NO_SPHERES) != 0;
	}
	else if (M_IsGame("ctf"))
	{
		s_no_mines_box.generic.type = MTYPE_SPINCONTROL;
		s_no_mines_box.generic.x = 0;
		s_no_mines_box.generic.y = y += 10;
		s_no_mines_box.generic.name = "force join";
		s_no_mines_box.generic.callback = DMFlagCallback;
		s_no_mines_box.itemnames = yes_no_names;
		s_no_mines_box.curvalue = (dmflags & DF_NO_MINES) != 0;

		s_stack_double_box.generic.type = MTYPE_SPINCONTROL;
		s_stack_double_box.generic.x = 0;
		s_stack_double_box.generic.y = y += 10;
		s_stack_double_box.generic.name = "armor protect";
		s_stack_double_box.generic.callback = DMFlagCallback;
		s_stack_double_box.itemnames = yes_no_names;
		s_stack_double_box.curvalue = (dmflags & DF_NO_STACK_DOUBLE) != 0;

		s_no_nukes_box.generic.type = MTYPE_SPINCONTROL;
		s_no_nukes_box.generic.x = 0;
		s_no_nukes_box.generic.y = y += 10;
		s_no_nukes_box.generic.name = "techs off";
		s_no_nukes_box.generic.callback = DMFlagCallback;
		s_no_nukes_box.itemnames = yes_no_names;
		s_no_nukes_box.curvalue = (dmflags & DF_NO_NUKES) != 0;
	}

	Menu_AddItem(&s_dmoptions_menu, &s_falls_box);
	Menu_AddItem(&s_dmoptions_menu, &s_weapons_stay_box);
	Menu_AddItem(&s_dmoptions_menu, &s_instant_powerups_box);
	Menu_AddItem(&s_dmoptions_menu, &s_powerups_box);
	Menu_AddItem(&s_dmoptions_menu, &s_health_box);
	Menu_AddItem(&s_dmoptions_menu, &s_armor_box);
	Menu_AddItem(&s_dmoptions_menu, &s_spawn_farthest_box);
	Menu_AddItem(&s_dmoptions_menu, &s_samelevel_box);
	Menu_AddItem(&s_dmoptions_menu, &s_force_respawn_box);

	if (!M_IsGame("ctf"))
	{
		Menu_AddItem(&s_dmoptions_menu, &s_teamplay_box);
	}

	Menu_AddItem(&s_dmoptions_menu, &s_allow_exit_box);
	Menu_AddItem(&s_dmoptions_menu, &s_infinite_ammo_box);
	Menu_AddItem(&s_dmoptions_menu, &s_fixed_fov_box);
	Menu_AddItem(&s_dmoptions_menu, &s_quad_drop_box);

	if (!M_IsGame("ctf"))
	{
		Menu_AddItem(&s_dmoptions_menu, &s_friendlyfire_box);
	}

	if (M_IsGame("rogue"))
	{
		Menu_AddItem(&s_dmoptions_menu, &s_no_mines_box);
		Menu_AddItem(&s_dmoptions_menu, &s_no_nukes_box);
		Menu_AddItem(&s_dmoptions_menu, &s_stack_double_box);
		Menu_AddItem(&s_dmoptions_menu, &s_no_spheres_box);
	}
	else if (M_IsGame("ctf"))
	{
		Menu_AddItem(&s_dmoptions_menu, &s_no_mines_box);
		Menu_AddItem(&s_dmoptions_menu, &s_stack_double_box);
		Menu_AddItem(&s_dmoptions_menu, &s_no_nukes_box);
	}

	Menu_Center(&s_dmoptions_menu);

	/* set the original dmflags statusbar */
	DMFlagCallback(0);
	Menu_SetStatusBar(&s_dmoptions_menu, dmoptions_statusbar);
}

static void
DMOptions_MenuDraw(void)
{
	Menu_Draw(&s_dmoptions_menu);
}

static const char *
DMOptions_MenuKey(int key)
{
	return Default_MenuKey(&s_dmoptions_menu, key);
}

static void
M_Menu_DMOptions_f(void)
{
	DMOptions_MenuInit();
	s_dmoptions_menu.draw = DMOptions_MenuDraw;
	s_dmoptions_menu.key  = DMOptions_MenuKey;

	M_PushMenu(&s_dmoptions_menu);
}

/*
 * DOWNLOADOPTIONS BOOK MENU
 */

static menuframework_s s_downloadoptions_menu;

static menuseparator_s s_download_title;
static menulist_s s_allow_download_box;

#ifdef USE_CURL
static menulist_s s_allow_download_http_box;
#endif

static menulist_s s_allow_download_maps_box;
static menulist_s s_allow_download_models_box;
static menulist_s s_allow_download_players_box;
static menulist_s s_allow_download_sounds_box;

static void
DownloadCallback(void *self)
{
	const menulist_s *f = (menulist_s *)self;

	if (f == &s_allow_download_box)
	{
		Cvar_SetValue("allow_download", (float)f->curvalue);
	}
#ifdef USE_CURL
	else if (f == &s_allow_download_http_box)
	{
		Cvar_SetValue("cl_http_downloads", f->curvalue);
	}
#endif
	else if (f == &s_allow_download_maps_box)
	{
		Cvar_SetValue("allow_download_maps", (float)f->curvalue);
	}
	else if (f == &s_allow_download_models_box)
	{
		Cvar_SetValue("allow_download_models", (float)f->curvalue);
	}
	else if (f == &s_allow_download_players_box)
	{
		Cvar_SetValue("allow_download_players", (float)f->curvalue);
	}
	else if (f == &s_allow_download_sounds_box)
	{
		Cvar_SetValue("allow_download_sounds", (float)f->curvalue);
	}
}

static void
DownloadOptions_MenuInit(void)
{
	static const char *yes_no_names[] =
	{
		"no", "yes", 0
	};
	unsigned short int y = 0;
	float scale = SCR_GetMenuScale();

	s_downloadoptions_menu.x = (int)(viddef.width * 0.50f);
	s_downloadoptions_menu.nitems = 0;

	s_download_title.generic.type = MTYPE_SEPARATOR;
	s_download_title.generic.name = "Download Options";
	s_download_title.generic.x = 48 * scale;
	s_download_title.generic.y = y;

	s_allow_download_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_box.generic.x = 0;
	s_allow_download_box.generic.y = y += 20;
	s_allow_download_box.generic.name = "allow downloading";
	s_allow_download_box.generic.callback = DownloadCallback;
	s_allow_download_box.itemnames = yes_no_names;
	s_allow_download_box.curvalue = (Cvar_VariableValue("allow_download") != 0);

#ifdef USE_CURL
	s_allow_download_http_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_http_box.generic.x	= 0;
	s_allow_download_http_box.generic.y	= y += 20;
	s_allow_download_http_box.generic.name	= "http downloading";
	s_allow_download_http_box.generic.callback = DownloadCallback;
	s_allow_download_http_box.itemnames = yes_no_names;
	s_allow_download_http_box.curvalue = (Cvar_VariableValue("cl_http_downloads") != 0);
#else
	y += 10;
#endif

	s_allow_download_maps_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_maps_box.generic.x = 0;
	s_allow_download_maps_box.generic.y = y += 10;
	s_allow_download_maps_box.generic.name = "maps";
	s_allow_download_maps_box.generic.callback = DownloadCallback;
	s_allow_download_maps_box.itemnames = yes_no_names;
	s_allow_download_maps_box.curvalue =
		(Cvar_VariableValue("allow_download_maps") != 0);

	s_allow_download_players_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_players_box.generic.x = 0;
	s_allow_download_players_box.generic.y = y += 10;
	s_allow_download_players_box.generic.name = "player models/skins";
	s_allow_download_players_box.generic.callback = DownloadCallback;
	s_allow_download_players_box.itemnames = yes_no_names;
	s_allow_download_players_box.curvalue =
		(Cvar_VariableValue("allow_download_players") != 0);

	s_allow_download_models_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_models_box.generic.x = 0;
	s_allow_download_models_box.generic.y = y += 10;
	s_allow_download_models_box.generic.name = "models";
	s_allow_download_models_box.generic.callback = DownloadCallback;
	s_allow_download_models_box.itemnames = yes_no_names;
	s_allow_download_models_box.curvalue =
		(Cvar_VariableValue("allow_download_models") != 0);

	s_allow_download_sounds_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_sounds_box.generic.x = 0;
	s_allow_download_sounds_box.generic.y = y += 10;
	s_allow_download_sounds_box.generic.name = "sounds";
	s_allow_download_sounds_box.generic.callback = DownloadCallback;
	s_allow_download_sounds_box.itemnames = yes_no_names;
	s_allow_download_sounds_box.curvalue =
		(Cvar_VariableValue("allow_download_sounds") != 0);

	Menu_AddItem(&s_downloadoptions_menu, &s_download_title);
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_box);

#ifdef USE_CURL
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_http_box);
#endif

	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_maps_box);
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_players_box);
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_models_box);
	Menu_AddItem(&s_downloadoptions_menu, &s_allow_download_sounds_box);

	Menu_Center(&s_downloadoptions_menu);

	/* skip over title */
	if (s_downloadoptions_menu.cursor == 0)
	{
		s_downloadoptions_menu.cursor = 1;
	}
}

static void
DownloadOptions_MenuDraw(void)
{
	Menu_Draw(&s_downloadoptions_menu);
}

static const char *
DownloadOptions_MenuKey(int key)
{
	return Default_MenuKey(&s_downloadoptions_menu, key);
}

static void
M_Menu_DownloadOptions_f(void)
{
	DownloadOptions_MenuInit();
	s_downloadoptions_menu.draw = DownloadOptions_MenuDraw;
	s_downloadoptions_menu.key  = DownloadOptions_MenuKey;

	M_PushMenu(&s_downloadoptions_menu);
}

/*
 * ADDRESS BOOK MENU
 */

#define NUM_ADDRESSBOOK_ENTRIES 9

static menuframework_s s_addressbook_menu;
static menufield_s s_addressbook_fields[NUM_ADDRESSBOOK_ENTRIES];

static void
AddressBook_MenuInit(void)
{
	int i;
	float scale = SCR_GetMenuScale();

	s_addressbook_menu.x = viddef.width / 2 - (142 * scale);
	s_addressbook_menu.y = viddef.height / (2 * scale) - 58;
	s_addressbook_menu.nitems = 0;

	for (i = 0; i < NUM_ADDRESSBOOK_ENTRIES; i++)
	{
		menufield_s *f;
		const cvar_t *adr;
		char buffer[20];

		Com_sprintf(buffer, sizeof(buffer), "adr%d", i);

		adr = Cvar_Get(buffer, "", CVAR_ARCHIVE);

		f = &s_addressbook_fields[i];

		f->generic.type = MTYPE_FIELD;
		f->generic.name = 0;
		f->generic.callback = 0;
		f->generic.x = 0;
		f->generic.y = i * 18 + 0;
		f->generic.localdata[0] = i;

		f->length = 60;
		f->visible_length = 30;

		Q_strlcpy(f->buffer, adr->string, f->length);
		f->cursor = strlen(f->buffer);

		Menu_AddItem(&s_addressbook_menu, f);
	}
}

static const char *
AddressBook_MenuKey(int key)
{
	if (key == K_ESCAPE)
	{
		int index;
		char buffer[20];

		for (index = 0; index < NUM_ADDRESSBOOK_ENTRIES; index++)
		{
			Com_sprintf(buffer, sizeof(buffer), "adr%d", index);
			Cvar_Set(buffer, s_addressbook_fields[index].buffer);
		}
	}

	return Default_MenuKey(&s_addressbook_menu, key);
}

static void
AddressBook_MenuDraw(void)
{
	M_Banner("m_banner_addressbook");
	Menu_Draw(&s_addressbook_menu);
}

static void
M_Menu_AddressBook_f(void)
{
	AddressBook_MenuInit();
	s_addressbook_menu.draw = AddressBook_MenuDraw;
	s_addressbook_menu.key  = AddressBook_MenuKey;

	M_PushMenu(&s_addressbook_menu);
}

/*
 * PLAYER CONFIG MENU
 */

static menuframework_s s_player_config_menu;
static menufield_s s_player_name_field;
static menubitmap_s s_player_icon_bitmap;
static menulist_s s_player_model_box;
static menulist_s s_player_skin_box;
static menulist_s s_player_handedness_box;
static menulist_s s_player_rate_box;
static menuseparator_s s_player_skin_title;
static menuseparator_s s_player_model_title;
static menuseparator_s s_player_hand_title;
static menuseparator_s s_player_rate_title;
static menuaction_s s_player_download_action;

#define MAX_PLAYERMODELS 1024

typedef struct _strlist
{
	char** data;
	int num;
} strlist_t;

// player model info
static strlist_t s_skinnames[MAX_PLAYERMODELS];
static strlist_t s_modelname;
static strlist_t s_directory;

static int rate_tbl[] = {2500, 3200, 5000, 10000, 25000, 0};
static const char *rate_names[] = {"28.8 Modem", "33.6 Modem", "Single ISDN",
								   "Dual ISDN/Cable", "T1/LAN", "User defined", 0 };

static void
DownloadOptionsFunc(void *self)
{
	M_Menu_DownloadOptions_f();
}

static void
HandednessCallback(void *unused)
{
	Cvar_SetValue("hand", (float)s_player_handedness_box.curvalue);
}

static void
RateCallback(void *unused)
{
	if (s_player_rate_box.curvalue != ARRLEN(rate_tbl) - 1)
	{
		Cvar_SetValue("rate", (float)rate_tbl[s_player_rate_box.curvalue]);
	}
}

static void
ModelCallback(void *unused)
{
	s_player_skin_box.itemnames = (const char **)s_skinnames[s_player_model_box.curvalue].data;
	s_player_skin_box.curvalue = 0;
}

// returns true if icon .pcx exists for skin .pcx
static qboolean
IconOfSkinExists(const char* skin, char** pcxfiles, int npcxfiles,
	const char *ext)
{
	int i;
	char scratch[1024];

	Q_strlcpy(scratch, skin, sizeof(scratch));
	*strrchr(scratch, '.') = 0;
	Q_strlcat(scratch, "_i.", sizeof(scratch));
	Q_strlcat(scratch, ext, sizeof(scratch));

	for (i = 0; i < npcxfiles; i++)
	{
		if (strcmp(pcxfiles[i], scratch) == 0)
		{
			return true;
		}
	}

	return false;
}

// strip file extension
static void
StripExtension(char* path)
{
	size_t length;

	length = strlen(path) - 1;

	while (length > 0 && path[length] != '.')
	{
		length--;

		if (path[length] == '/')
		{
			return;         // no extension
		}
	}

	if (length)
	{
		path[length] = 0;
	}
}

// returns true if file is in path
static qboolean
ContainsFile(char* path, char* file)
{
	int handle = 0;
	qboolean result = false;

	if (path != 0 && file != 0)
	{
		char pathname[MAX_QPATH];
		int length = 0;

		Com_sprintf(pathname, MAX_QPATH, "%s/%s", path, file);

		length = FS_FOpenFile(pathname, &handle, false);

		// verify the existence of file
		if (handle != 0 && length != 0)
		{
			FS_FCloseFile(handle);
			result = true;
		}
	}

	return result;
}

// replace characters in string
static void
ReplaceCharacters(char* s, char r, char c)
{
	char* p = s;

	if (p == 0)
	{
		return;
	}

	while (*p != 0)
	{
		if (*p == r)
		{
			*p = c;
		}

		p++;
	}
}

// qsort directory name compare function
static int
dircmp_func(const void* _a, const void* _b)
{
	const char* a = strrchr(*(const char**)_a, '/') + 1;
	const char* b = strrchr(*(const char**)_b, '/') + 1;

	// sort by male, female, then alphabetical
	if (strcmp(a, "male") == 0)
	{
		return -1;
	}
	else if (strcmp(b, "male") == 0)
	{
		return 1;
	}

	if (strcmp(a, "female") == 0)
	{
		return -1;
	}
	else if (strcmp(b, "female") == 0)
	{
		return 1;
	}

	return strcmp(a, b);
}

// frees all player model info
static void
PlayerModelFree()
{
	char* s = NULL;

	// there should be no valid skin names if there is no valid model
	if (s_modelname.num != 0)
	{
		while (s_modelname.num-- > 0)
		{
			// skins
			while (s_skinnames[s_modelname.num].num-- > 0)
			{
				s = s_skinnames[s_modelname.num].data[s_skinnames[s_modelname.num].num];
				if (s != NULL)
				{
					free(s);
				}
			}

			s = (char*)s_skinnames[s_modelname.num].data;

			if (s != NULL)
			{
				free(s);
			}

			s_skinnames[s_modelname.num].data = 0;
			s_skinnames[s_modelname.num].num = 0;

			// models
			s = s_modelname.data[s_modelname.num];
			if (s != NULL)
			{
				free(s);
			}
		}
	}

	s = (char*)s_modelname.data;
	if (s != NULL)
	{
		free(s);
	}

	s_modelname.data = 0;
	s_modelname.num = 0;

	// directories
	while (s_directory.num-- > 0)
	{
		s = s_directory.data[s_directory.num];
		if (s != NULL)
		{
			free(s);
		}
	}

	s = (char*)s_directory.data;
	if (s != NULL)
	{
		free(s);
	}

	s_directory.data = 0;
	s_directory.num = 0;
}

// list all player model directories.
// directory names are stored players/<modelname>.
// directory number never exceeds MAX_PLAYERMODELS
static qboolean
PlayerDirectoryList(void)
{
	const char* findname = "players/*";
	char** list = NULL;
	int num = 0, dirnum = 0;
	size_t listoff = strlen(findname);

	/* get a list of "players" subdirectories or files */
	if ((list = FS_ListFiles2(findname, &num, 0, 0)) == NULL)
	{
		return false;
	}

	if (num > MAX_PLAYERMODELS)
	{
		Com_Printf("Too many player models (%d)!\n", num);
		num = MAX_PLAYERMODELS - 1;
	}

	// malloc directories
	char** data = (char**)calloc(num, sizeof(char*));
	YQ2_COM_CHECK_OOM(data, "calloc()", num * sizeof(char*))
	if (!data)
	{
		/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
		return false;
	}

	s_directory.data = data;

	for (int i = 0; i < num; ++i)
	{
		char dirname[MAX_QPATH];
		const char *dirsize;
		int j;

		// last element of FS_FileList maybe null
		if (list[i] == 0)
		{
			break;
		}

		ReplaceCharacters(list[i], '\\', '/');

		/*
		 * search slash after "players/" and use only directory name
		 * pak search does not return directory names, only files in
		 * directories
		 */
		dirsize = strchr(list[i] + listoff, '/');
		if (dirsize)
		{
			int dirnamelen = 0;

			dirnamelen = dirsize - list[i];
			memcpy(dirname, list[i], dirnamelen);
			dirname[dirnamelen] = 0;
		}
		else
		{
			Q_strlcpy(dirname, list[i], sizeof(dirname));
		}

		for (j = 0; j < dirnum; j++)
		{
			if (!strcmp(dirname, data[j]))
			{
				break;
			}
		}

		if (j == dirnum)
		{
			char* s = (char*)malloc(MAX_QPATH);

			YQ2_COM_CHECK_OOM(s, "malloc()", MAX_QPATH * sizeof(char))

			Q_strlcpy(s, dirname, MAX_QPATH);
			data[dirnum] = s;
			dirnum ++;
		}
	}

	s_directory.num = dirnum;

	// free file list
	FS_FreeList(list, num);

	/* sort them male, female, alphabetical */
	if (s_directory.num > 2)
	{
		qsort(s_directory.data, s_directory.num - 1, sizeof(char*), dircmp_func);
	}

	return true;
}

static char**
HasSkinInDir(const char *dirname, const char *ext, int *num)
{
	char findname[MAX_QPATH];

	snprintf(findname, sizeof(findname), "%s/*.%s", dirname, ext);

	return FS_ListFiles2(findname, num, 0, 0);
}

static char**
HasSkinsInDir(const char *dirname, int *num)
{
	char **list_png, **list_pcx;
	char **curr = NULL, **list = NULL;
	int num_png, num_pcx;
	size_t dirname_size;

	*num = 0;
	/* dir name size plus one for skip slash */
	dirname_size = strlen(dirname) + 1;

	list_png = HasSkinInDir(dirname, "png", &num_png);
	if (list_png)
	{
		*num += num_png - 1;
	}

	list_pcx = HasSkinInDir(dirname, "pcx", &num_pcx);
	if (list_pcx)
	{
		*num += num_pcx - 1;
	}

	if (*num)
	{
		curr = list = malloc(sizeof(char *) * (*num + 1));
		YQ2_COM_CHECK_OOM(list, "malloc()", (size_t)sizeof(char *) * (*num + 1))
		if (!list)
		{
			/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
			return false;
		}

		if (list_png && num_png)
		{
			int j;

			for (j = 0; j < num_png; j ++)
			{
				if (list_png[j])
				{
					if (!strchr(list_png[j] + dirname_size, '/'))
					{
						*curr = list_png[j];
						curr++;
					}
					else
					{
						/* unused in final response */
						free(list_png[j]);
					}
				}
			}
		}

		if (list_pcx && num_pcx)
		{
			int j;

			for (j = 0; j < num_pcx; j ++)
			{
				if (list_pcx[j])
				{
					if (!strchr(list_pcx[j] + dirname_size, '/'))
					{
						*curr = list_pcx[j];
						curr++;
					}
					else
					{
						/* unused in final response */
						free(list_pcx[j]);
					}
				}
			}
		}

		*curr = NULL;
		curr++;
		*num = curr - list;
	}

	if (list_png)
	{
		free(list_png);
	}

	if (list_pcx)
	{
		free(list_pcx);
	}

	return list;
}

/*
 * list all valid player models.
 * call PlayerDirectoryList first.
 * model names is always allocated MAX_PLAYERMODELS
 */
static qboolean
PlayerModelList(void)
{
	char** list = NULL;
	char** data = NULL;
	int i;
	int num = 0;
	int mdl = 0;
	qboolean result = true;

	// malloc models
	data = (char**)calloc(MAX_PLAYERMODELS, sizeof(char*));
	YQ2_COM_CHECK_OOM(data, "calloc()", MAX_PLAYERMODELS * sizeof(char*))
	if (!data)
	{
		/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
		return false;
	}

	s_modelname.data = data;
	s_modelname.num = 0;

	/* verify the existence of at least one pcx skin */
	for (i = 0; i < s_directory.num; ++i)
	{
		char* s = NULL;
		char* t = NULL;
		int l;

		if (s_directory.data[i] == 0)
		{
			continue;
		}

		/* contains triangle .md2 model */
		s = s_directory.data[i];

		if (ContainsFile(s, "tris.md2") == false)
		{
			/* invalid player model */
			continue;
		}

		list = HasSkinsInDir(s_directory.data[i], &num);
		/* get a list of pcx files */
		if (!num || !list)
		{
			continue;
		}

		/* count valid skins, which consist of a skin with a matching "_i" icon */
		s_skinnames[mdl].num = 0;

		for (int j = 0; j < num; j++)
		{
			/* last element of FS_FileList maybe null */
			if (list[j] == 0)
			{
				break;
			}

			if (!strstr(list[j], "_i.png") ||
				!strstr(list[j], "_i.pcx"))
			{
				if (IconOfSkinExists(list[j], list, num - 1, "png") ||
					IconOfSkinExists(list[j], list, num - 1, "pcx"))
				{
					s_skinnames[mdl].num++;
				}
			}
		}

		if (s_skinnames[mdl].num == 0)
		{
			FS_FreeList(list, num);

			continue;
		}

		/* malloc skinnames */
		data = (char**)malloc((s_skinnames[mdl].num + 1) * sizeof(char*));
		YQ2_COM_CHECK_OOM(data, "malloc()", (s_skinnames[mdl].num + 1) * sizeof(char*))
		if (!data)
		{
			/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
			return false;
		}

		memset(data, 0, (s_skinnames[mdl].num + 1) * sizeof(char*));

		s_skinnames[mdl].data = data;
		s_skinnames[mdl].num = 0;

		/* duplicate strings */
		for (int k = 0; k < num; ++k)
		{
			/* last element of FS_FileList maybe null */
			if (list[k] == 0)
			{
				break;
			}

			if (!strstr(list[k], "_i.png") ||
				!strstr(list[k], "_i.pcx"))
			{
				if (IconOfSkinExists(list[k], list, num - 1, "png") ||
					IconOfSkinExists(list[k], list, num - 1, "pcx"))
				{
					ReplaceCharacters(list[k], '\\', '/');

					t = strrchr(list[k], '/');

					l = strlen(t) + 1;
					s = (char*)malloc(l);

					YQ2_COM_CHECK_OOM(s, "malloc()", l * sizeof(char))
					if (!s)
					{
						/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
						return false;
					}

					StripExtension(t);
					Q_strlcpy(s, t + 1, l);

					data[s_skinnames[mdl].num++] = s;
				}
			}
		}

		/* sort skin names alphabetically */
		qsort(s_skinnames[mdl].data, s_skinnames[mdl].num, sizeof(char*), Q_sort_stricmp);

		/* at this point we have a valid player model */
		t = strrchr(s_directory.data[i], '/');
		l = strlen(t) + 1;
		s = (char*)malloc(l);

		YQ2_COM_CHECK_OOM(s, "malloc()", l * sizeof(char))
		if (!s)
		{
			/* unaware about YQ2_ATTR_NORETURN_FUNCPTR? */
			return false;
		}

		Q_strlcpy(s, t + 1, l);

		s_modelname.data[s_modelname.num++] = s;
		mdl = s_modelname.num;

		/* free file list */
		FS_FreeList(list, num);
	}

	if (s_modelname.num == 0)
	{
		PlayerModelFree();
		result = false;
	}

	return result;
}

static qboolean
PlayerConfig_ScanDirectories(void)
{
	qboolean result = false;

	// directory names
	result = PlayerDirectoryList();

	if (result == false)
	{
		Com_Printf("No valid player directories found.\n");
	}

	// valid models
	result = PlayerModelList();

	if (result == false)
	{
		Com_Printf("No valid player models found.\n");
	}

	return result;
}

static void
ListModels_f(void)
{
	PlayerConfig_ScanDirectories();

	for (size_t i = 0; i < s_modelname.num; i++)
	{
		for (size_t j = 0; j < s_skinnames[i].num; j++)
		{
			Com_Printf("%s/%s\n", s_modelname.data[i], s_skinnames[i].data[j]);
		}
	}

	PlayerModelFree();
}

static qboolean
PlayerConfig_MenuInit(void)
{
	extern cvar_t *name;
	const extern cvar_t *skin;
	cvar_t *hand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	static const char *handedness[] = { "right", "left", "center", 0 };
	char mdlname[MAX_QPATH];
	char imgname[MAX_QPATH];
	int mdlindex = 0;
	int imgindex = 0;
	int i = 0;
	float scale = SCR_GetMenuScale();

	if (PlayerConfig_ScanDirectories() == false)
	{
		return false;
	}

	Q_strlcpy(mdlname, skin->string, sizeof(mdlname));
	ReplaceCharacters(mdlname, '\\', '/' );

	if (strchr(mdlname, '/'))
	{
		Q_strlcpy(imgname, strchr(mdlname, '/') + 1, sizeof(imgname));
		*strchr(mdlname, '/') = 0;
	}
	else
	{
		strcpy(mdlname, "male\0");
		strcpy(imgname, "grunt\0");
	}

	for (i = 0; i < s_modelname.num; i++)
	{
		if (Q_stricmp(s_modelname.data[i], mdlname) == 0)
		{
			mdlindex = i;
			break;
		}
	}

	for (i = 0; i < s_skinnames[mdlindex].num; i++)
	{
		const char* names = s_skinnames[mdlindex].data[i];
		if (Q_stricmp(names, imgname) == 0)
		{
			imgindex = i;
			break;
		}
	}

	if (hand->value < 0 || hand->value > 2)
	{
		Cvar_SetValue("hand", 0);
	}

	s_player_config_menu.x = viddef.width / 2 - 95 * scale;
	s_player_config_menu.y = viddef.height / (2 * scale) - 97;
	s_player_config_menu.nitems = 0;

	s_player_name_field.generic.type = MTYPE_FIELD;
	s_player_name_field.generic.name = "name";
	s_player_name_field.generic.callback = 0;
	s_player_name_field.generic.x = 0;
	s_player_name_field.generic.y = 0;
	s_player_name_field.length = 20;
	s_player_name_field.visible_length = 20;
	Q_strlcpy(s_player_name_field.buffer, name->string,
		sizeof(s_player_name_field.buffer));
	s_player_name_field.cursor = strlen(name->string);

	s_player_icon_bitmap.generic.type = MTYPE_BITMAP;
	s_player_icon_bitmap.generic.flags = QMF_INACTIVE;
	s_player_icon_bitmap.generic.x = ((viddef.width / scale - 95) / 2) - 87;
	s_player_icon_bitmap.generic.y = ((viddef.height / (2 * scale))) - 72;
	s_player_icon_bitmap.generic.name = 0;
	s_player_icon_bitmap.generic.callback = 0;
	s_player_icon_bitmap.focuspic = 0;

	s_player_model_title.generic.type = MTYPE_SEPARATOR;
	s_player_model_title.generic.name = "model";
	s_player_model_title.generic.x = -8 * scale;
	s_player_model_title.generic.y = 60;

	s_player_model_box.generic.type = MTYPE_SPINCONTROL;
	s_player_model_box.generic.x = -56 * scale;
	s_player_model_box.generic.y = 70;
	s_player_model_box.generic.callback = ModelCallback;
	s_player_model_box.generic.cursor_offset = -48;
	s_player_model_box.curvalue = mdlindex;
	s_player_model_box.itemnames = (const char**)s_modelname.data;

	s_player_skin_title.generic.type = MTYPE_SEPARATOR;
	s_player_skin_title.generic.name = "skin";
	s_player_skin_title.generic.x = -16 * scale;
	s_player_skin_title.generic.y = 84;

	s_player_skin_box.generic.type = MTYPE_SPINCONTROL;
	s_player_skin_box.generic.x = -56 * scale;
	s_player_skin_box.generic.y = 94;
	s_player_skin_box.generic.name = 0;
	s_player_skin_box.generic.callback = 0;
	s_player_skin_box.generic.cursor_offset = -48;
	s_player_skin_box.curvalue = imgindex;
	s_player_skin_box.itemnames = (const char **)s_skinnames[mdlindex].data;

	s_player_hand_title.generic.type = MTYPE_SEPARATOR;
	s_player_hand_title.generic.name = "handedness";
	s_player_hand_title.generic.x = 32 * scale;
	s_player_hand_title.generic.y = 108;

	s_player_handedness_box.generic.type = MTYPE_SPINCONTROL;
	s_player_handedness_box.generic.x = -56 * scale;
	s_player_handedness_box.generic.y = 118;
	s_player_handedness_box.generic.name = 0;
	s_player_handedness_box.generic.cursor_offset = -48;
	s_player_handedness_box.generic.callback = HandednessCallback;
	s_player_handedness_box.curvalue = ClampCvar(0, 2, hand->value);
	s_player_handedness_box.itemnames = handedness;

	for (i = 0; i < ARRLEN(rate_tbl) - 1; i++)
	{
		if (Cvar_VariableValue("rate") == rate_tbl[i])
		{
			break;
		}
	}

	s_player_rate_title.generic.type = MTYPE_SEPARATOR;
	s_player_rate_title.generic.name = "connect speed";
	s_player_rate_title.generic.x = 56 * scale;
	s_player_rate_title.generic.y = 156;

	s_player_rate_box.generic.type = MTYPE_SPINCONTROL;
	s_player_rate_box.generic.x = -56 * scale;
	s_player_rate_box.generic.y = 166;
	s_player_rate_box.generic.name = 0;
	s_player_rate_box.generic.cursor_offset = -48;
	s_player_rate_box.generic.callback = RateCallback;
	s_player_rate_box.curvalue = i;
	s_player_rate_box.itemnames = rate_names;

	s_player_download_action.generic.type = MTYPE_ACTION;
	s_player_download_action.generic.name = "download options";
	s_player_download_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_download_action.generic.x = -24 * scale;
	s_player_download_action.generic.y = 186;
	s_player_download_action.generic.statusbar = NULL;
	s_player_download_action.generic.callback = DownloadOptionsFunc;

	Menu_AddItem(&s_player_config_menu, &s_player_name_field);
	Menu_AddItem(&s_player_config_menu, &s_player_icon_bitmap);
	Menu_AddItem(&s_player_config_menu, &s_player_model_title);
	Menu_AddItem(&s_player_config_menu, &s_player_model_box);

	if (s_player_skin_box.itemnames)
	{
		Menu_AddItem(&s_player_config_menu, &s_player_skin_title);
		Menu_AddItem(&s_player_config_menu, &s_player_skin_box);
	}

	Menu_AddItem(&s_player_config_menu, &s_player_hand_title);
	Menu_AddItem(&s_player_config_menu, &s_player_handedness_box);
	Menu_AddItem(&s_player_config_menu, &s_player_rate_title);
	Menu_AddItem(&s_player_config_menu, &s_player_rate_box);
	Menu_AddItem(&s_player_config_menu, &s_player_download_action);

	return true;
}

extern float CalcFov(float fov_x, float w, float h);

/*
 * Model animation
 */
static void
PlayerConfig_AnimateModel(entity_t *entity, int count, int curTime)
{
	const cvar_t *cl_start_frame, *cl_end_frame;
	int startFrame, endFrame;

	cl_start_frame = Cvar_Get("cl_model_preview_start", "84", CVAR_ARCHIVE);
	cl_end_frame = Cvar_Get("cl_model_preview_end", "94", CVAR_ARCHIVE);
	startFrame = cl_start_frame->value;
	endFrame = cl_end_frame->value;

	if (startFrame >= 0 && endFrame > startFrame)
	{
		int i;

		for (i = 0; i < count; i ++)
		{
			/* salute male 84..94 frame */
			entity[i].frame = (curTime / 100) % (endFrame - startFrame) + startFrame;
		}
	}
}

static void
PlayerConfig_MenuDraw(void)
{
	refdef_t refdef;
	float scale = SCR_GetMenuScale();

	memset(&refdef, 0, sizeof(refdef));

	refdef.x = viddef.width / 2;
	refdef.y = viddef.height / 2 - 72 * scale;
	refdef.width = 144 * scale;
	refdef.height = 168 * scale;
	refdef.fov_x = 40;
	refdef.fov_y = CalcFov(refdef.fov_x, (float)refdef.width, (float)refdef.height);
	refdef.time = cls.realtime * 0.001f;

	// could remove this, there should be a valid set of models
	if ((s_player_model_box.curvalue >= 0 && s_player_model_box.curvalue < s_modelname.num)
		&& (s_player_skin_box.curvalue >= 0
		&& s_player_skin_box.curvalue < s_skinnames[s_player_model_box.curvalue].num))
	{
		entity_t entities[2];
		char scratch[MAX_QPATH];
		char* mdlname = s_modelname.data[s_player_model_box.curvalue];
		char* imgname = s_skinnames[s_player_model_box.curvalue].data[s_player_skin_box.curvalue];
		int i, curTime;

		memset(&entities, 0, sizeof(entities));

		Com_sprintf(scratch, sizeof(scratch), "players/%s/tris.md2", mdlname);
		entities[0].model = R_RegisterModel(scratch);

		Com_sprintf(scratch, sizeof(scratch), "players/%s/%s.pcx", mdlname,
			imgname);
		entities[0].skin = R_RegisterSkin(scratch);

		curTime = Sys_Milliseconds();

		/* multiplayer weapons loaded */
		if (num_cl_weaponmodels)
		{
			int weapon_id;

			/* change weapon every 3 rounds */
			weapon_id = curTime / 9000;

			weapon_id = weapon_id % num_cl_weaponmodels;
			/* show weapon also */
			Com_sprintf(scratch, sizeof(scratch),
				"players/%s/%s", mdlname, cl_weaponmodels[weapon_id]);
			entities[1].model = R_RegisterModel(scratch);
		}

		/* no such weapon model */
		if (!entities[1].model)
		{
			/* show weapon also */
			Com_sprintf(scratch, sizeof(scratch),
				"players/%s/weapon.md2", mdlname);
			entities[1].model = R_RegisterModel(scratch);
		}

		curTime = curTime % 3000;
		for (i = 0; i < 2; i++)
		{
			entities[i].flags = RF_FULLBRIGHT;
			entities[i].origin[0] = 80;
			entities[i].origin[1] = 0;
			entities[i].origin[2] = 0;
			VectorCopy(entities[i].origin, entities[i].oldorigin);
			entities[i].frame = 0;
			entities[i].oldframe = 0;
			entities[i].backlerp = 0.0;
			// one full turn is 3s = 3000ms => 3000/360 deg per millisecond
			entities[i].angles[1] = (float)curTime/(3000.0f/360.0f);
		}

		PlayerConfig_AnimateModel(entities, 2, curTime);

		refdef.areabits = 0;
		refdef.num_entities = (entities[1].model) ? 2 : 1;
		refdef.entities = entities;
		refdef.lightstyles = 0;
		refdef.rdflags = RDF_NOWORLDMODEL;

		Com_sprintf(scratch, sizeof(scratch), "/players/%s/%s_i.pcx", mdlname,
			imgname);

		// icon bitmap to draw
		s_player_icon_bitmap.generic.name = scratch;

		Menu_Draw(&s_player_config_menu);

		M_DrawTextBox(((int)(refdef.x) * (320.0F / viddef.width) - 8),
					  (int)((viddef.height / 2) * (240.0F / viddef.height) - 77),
					  refdef.width / (8 * scale), refdef.height / (8 * scale));
		refdef.height += 4 * scale;

		R_RenderFrame(&refdef);
	}
}

static const char *
PlayerConfig_MenuKey(int key)
{
	key = Key_GetMenuKey(key);
	if (key == K_ESCAPE)
	{
		const char* name = NULL;
		char skin[MAX_QPATH];
		char* mdl = NULL;
		char* img = NULL;

		name = s_player_name_field.buffer;
		mdl = s_modelname.data[s_player_model_box.curvalue];
		img = s_skinnames[s_player_model_box.curvalue].data[s_player_skin_box.curvalue];

		Com_sprintf(skin, MAX_QPATH, "%s/%s", mdl, img);

		// set <name> and <model dir>/<skin>
		Cvar_Set("name", name);
		Cvar_Set("skin", skin);

		PlayerModelFree();          // free player skins, models and directories
	}

	return Default_MenuKey(&s_player_config_menu, key);
}

static void
M_Menu_PlayerConfig_f(void)
{
	if (!PlayerConfig_MenuInit())
	{
		Menu_SetStatusBar(&s_multiplayer_menu, "no valid player models found");
		return;
	}

	Menu_SetStatusBar(&s_multiplayer_menu, NULL);
	s_player_config_menu.draw = PlayerConfig_MenuDraw;
	s_player_config_menu.key  = PlayerConfig_MenuKey;

	M_PushMenu(&s_player_config_menu);
}

/*
 * QUIT MENU
 */

menuframework_s s_quit_menu;

static const char *
M_Quit_Key(int key)
{
	int menu_key = Key_GetMenuKey(key);
	switch (menu_key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		M_PopMenu();
		break;

	case K_ENTER:
	case 'Y':
	case 'y':
		cls.key_dest = key_console;
		CL_Quit_f();
		break;

	default:
		break;
	}

	return NULL;
}

static void
M_Quit_Draw(void)
{
	int w, h;
	float scale = SCR_GetMenuScale();

	Draw_GetPicSize(&w, &h, "quit");
	Draw_PicScaled((viddef.width - w * scale) / 2, (viddef.height - h * scale) / 2, "quit", scale);
}

static void
M_Menu_Quit_f(void)
{
	s_quit_menu.draw = M_Quit_Draw;
	s_quit_menu.key  = M_Quit_Key;

	M_PushMenu(&s_quit_menu);
}

void
M_Init(void)
{
	char cursorname[MAX_QPATH];
	int w = 0;
	int h = 0;

	Cmd_AddCommand("playermodels", ListModels_f);

	Cmd_AddCommand("menu_main", M_Menu_Main_f);
	Cmd_AddCommand("menu_game", M_Menu_Game_f);
	Cmd_AddCommand("menu_loadgame", M_Menu_LoadGame_f);
	Cmd_AddCommand("menu_savegame", M_Menu_SaveGame_f);
	Cmd_AddCommand("menu_joinserver", M_Menu_JoinServer_f);
	Cmd_AddCommand("menu_addressbook", M_Menu_AddressBook_f);
	Cmd_AddCommand("menu_startserver", M_Menu_StartServer_f);
	Cmd_AddCommand("menu_dmoptions", M_Menu_DMOptions_f);
	Cmd_AddCommand("menu_playerconfig", M_Menu_PlayerConfig_f);
	Cmd_AddCommand("menu_downloadoptions", M_Menu_DownloadOptions_f);
	Cmd_AddCommand("menu_credits", M_Menu_Credits_f);
	Cmd_AddCommand("menu_mods", M_Menu_Mods_f);
	Cmd_AddCommand("menu_multiplayer", M_Menu_Multiplayer_f);
	Cmd_AddCommand("menu_multiplayer_keys", M_Menu_Multiplayer_Keys_f);
	Cmd_AddCommand("menu_video", M_Menu_Video_f);
	Cmd_AddCommand("menu_options", M_Menu_Options_f);
	Cmd_AddCommand("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand("menu_joy", M_Menu_Joy_f);
	Cmd_AddCommand("menu_gyro", M_Menu_Gyro_f);
	Cmd_AddCommand("menu_buttons", M_Menu_ControllerButtons_f);
	Cmd_AddCommand("menu_altbuttons", M_Menu_ControllerAltButtons_f);
	Cmd_AddCommand("menu_sticks", M_Menu_Stick_f);
	Cmd_AddCommand("menu_quit", M_Menu_Quit_f);

	/* initialize the server address book cvars (adr0, adr1, ...)
	 * so the entries are not lost if you don't open the address book */
	for (int index = 0; index < NUM_ADDRESSBOOK_ENTRIES; index++)
	{
		char buffer[20];
		Com_sprintf(buffer, sizeof(buffer), "adr%d", index);
		Cvar_Get(buffer, "", CVAR_ARCHIVE);
	}

	// cache the cursor frames
	for (int i = 0; i < NUM_CURSOR_FRAMES; i++)
	{
		Com_sprintf(cursorname, sizeof(cursorname), "m_cursor%d", i);
		Draw_FindPic(cursorname);
		Draw_GetPicSize(&w, &h, cursorname);

		if (w > m_cursor_width)
		{
			m_cursor_width = w;
		}
	}
}

void
M_Draw(void)
{
	if (cls.key_dest != key_menu)
	{
		return;
	}

	/* repaint everything next frame */
	SCR_DirtyScreen();

	/* dim everything behind it down */
	if (cl.cinematictime > 0)
	{
		Draw_Fill(0, 0, viddef.width, viddef.height, 0);
	}

	else
	{
		Draw_FadeScreen();
	}

	if (m_active.draw)
	{
		m_active.draw();
	}

	/* delay playing the enter sound until after the
	   menu has been drawn, to avoid delay while
	   caching images */
	if (m_entersound)
	{
		S_StartLocalSound(menu_in_sound);
		m_entersound = false;
	}
}

void
M_Keydown(int key)
{
	if (m_active.key)
	{
		const char *s;
		if ((s = m_active.key(key)) != 0)
		{
			S_StartLocalSound((char *)s);
		}
	}
}
