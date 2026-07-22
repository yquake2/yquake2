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
 * This file implements the generic part of the menu
 *
 * =======================================================================
 */

#include <string.h>
#include <ctype.h>
#include "../header/client.h"
#include "header/qmenu.h"

void IN_GetClipboardText(char *out, size_t n);
int IN_SetClipboardText(const char *s);

static void Action_Draw(menuaction_s *a);
static void Menu_DrawStatusBar(const char *string);
static void MenuList_Draw(menulist_s *l);
static void Separator_Draw(menuseparator_s *s);
static void Slider_Draw(menuslider_s *s);
static void SpinControl_Draw(menulist_s *s);

static qboolean Slider_DoSlide(menuslider_s *s, int dir);
static qboolean SpinControl_DoSlide(menulist_s *s, int dir);

extern viddef_t viddef;

#define VID_WIDTH viddef.width
#define VID_HEIGHT viddef.height

const char *menu_in_sound = "misc/menu1.wav";
const char *menu_move_sound = "misc/menu2.wav";
const char *menu_out_sound = "misc/menu3.wav";

// "Wrapper" for Q_clamp to evaluate parameters just once (TODO: delete?)
float
ClampCvar(float min, float max, float value)
{
	return Q_clamp(value, min, max);
}

/*
=================
Bitmap_Draw
=================
*/
static void
Bitmap_Draw(menubitmap_s * item)
{
	float scale = SCR_GetMenuScale();
	int x = 0;
	int y = 0;

	x = item->generic.x;
	y = item->generic.y;

	if (((item->generic.flags & QMF_HIGHLIGHT_IF_FOCUS) &&
		(Menu_ItemAtCursor(item->generic.parent) == item)))
	{
		Draw_PicScaled(x * scale, y * scale, item->focuspic, scale);
	}
	else if (item->generic.name)
	{
		Draw_PicScaled(x * scale, y * scale, ( char * )item->generic.name, scale);
	}
}

static void
Action_Draw(menuaction_s *a)
{
	float scale = SCR_GetMenuScale();
	int x = 0;
	int y = 0;

	x = a->generic.parent->x + a->generic.x;
	y = a->generic.parent->y + a->generic.y;

	if (a->generic.flags & QMF_LEFT_JUSTIFY)
	{
		if (a->generic.flags & QMF_GRAYED)
		{
			Menu_DrawStringDark(x + (LCOLUMN_OFFSET * scale),
				y, a->generic.name);
		}

		else
		{
			Menu_DrawString(x + (LCOLUMN_OFFSET * scale),
				y, a->generic.name);
		}
	}
	else
	{
		if (a->generic.flags & QMF_GRAYED)
		{
			Menu_DrawStringR2LDark(x + (LCOLUMN_OFFSET * scale),
				y, a->generic.name);
		}

		else
		{
			Menu_DrawStringR2L(x + (LCOLUMN_OFFSET * scale),
				y, a->generic.name);
		}
	}

	if (a->generic.ownerdraw)
	{
		a->generic.ownerdraw(a);
	}
}

void
Field_InitState(menufield_s *f, const char *s, int len, int vislen)
{
	if (len < 1)
	{
		len = 1;
	}
	else if (len > sizeof(f->buffer))
	{
		len = sizeof(f->buffer);
	}

	if (vislen < 1)
	{
		vislen = 1;
	}
	else if (vislen > len)
	{
		vislen = len;
	}

	*f->buffer = '\0';

	if (s)
	{
		if (((f->generic.flags & QMF_NUMBERSONLY) && !Q_strisnum(s)) ||
			Q_strlcpy(f->buffer, s, len) >= len)
		{
			*f->buffer = '\0';
		}
	}

	f->length = len;
	f->visible_length = vislen;
	f->cursor = strlen(f->buffer);
}

static void
Field_Draw(menufield_s *f)
{
	int i, n;
	char tempbuffer[128] = "";
	float scale = SCR_GetMenuScale();
	int x = 0;
	int y = 0;

	x = f->generic.parent->x + f->generic.x;
	y = f->generic.parent->y + f->generic.y;

	if (f->generic.name)
	{
		Menu_DrawStringR2LDark(x + LCOLUMN_OFFSET * scale,
			y, f->generic.name);
	}

	n = f->visible_length + 1;

	if (n > sizeof(tempbuffer))
	{
		n = sizeof(tempbuffer);
	}

	i = (f->cursor > f->visible_length) ? (f->cursor - f->visible_length) : 0;
	Q_strlcpy(tempbuffer, f->buffer + i, n);

	Draw_CharScaled(x + (16 * scale),
			(y - 4) * scale, 18, scale);
	Draw_CharScaled(x + 16 * scale,
			(y + 4) * scale, 24, scale);

	Draw_CharScaled(x + (24 * scale) + (f->visible_length * (8 * scale)),
			(y - 4) * scale, 20, scale);
	Draw_CharScaled(x + (24 * scale) + (f->visible_length * (8 * scale)),
			(y + 4) * scale, 26, scale);

	for (i = 0; i < f->visible_length; i++)
	{
		Draw_CharScaled(x + (24 * scale) + (i * 8 * scale),
				(y - 4) * scale, 19, scale);
		Draw_CharScaled(x + (24 * scale) + (i * 8 * scale),
				(y + 4) * scale, 25, scale);
	}

	Menu_DrawString(x + (24 * scale),
		y, tempbuffer);

	if (Menu_ItemAtCursor(f->generic.parent) == f)
	{
		if (((int)(Sys_Milliseconds() / 250)) & 1)
		{
			int offset;

			if (f->cursor > f->visible_length)
			{
				offset = f->visible_length;
			}
			else
			{
				offset = f->cursor;
			}

			Draw_CharScaled(
				x + (24 * scale) + (offset * (8 * scale)),
				y * scale, 11, scale);
		}
	}
}

void
Field_ResetCursor(menuframework_s *m)
{
	menucommon_s *item = Menu_ItemAtCursor(m);

	if (item && item->type == MTYPE_FIELD)
	{
		menufield_s *f = (menufield_s *)item;

		f->cursor = strlen(f->buffer);
	}
}

qboolean
Field_Key(menufield_s *f, int key)
{
	char txt[256];

	if (keydown[K_CTRL])
	{
		if (key == 'l')
		{
			*f->buffer = '\0';
			f->cursor = 0;

			return true;
		}

		if (key == 'c' || key == 'x')
		{
			if (*f->buffer != '\0')
			{
				if (IN_SetClipboardText(f->buffer))
				{
					Com_Printf("Copying menu field to clipboard failed.\n");
				}
				else if (key == 'x')
				{
					*f->buffer = '\0';
					f->cursor = 0;
				}
			}

			return true;
		}

		if (key == 'v')
		{
			IN_GetClipboardText(txt, sizeof(txt));

			if (*txt != '\0')
			{
				if ((f->generic.flags & QMF_NUMBERSONLY) && !Q_strisnum(txt))
				{
					return false;
				}

				f->cursor += Q_strins(f->buffer, txt, f->cursor, f->length);
			}
		}

		return true;
	}

	switch (key)
	{
		case K_KP_LEFTARROW:
		case K_LEFTARROW:
			if (f->cursor > 0)
			{
				f->cursor--;
			}
			break;

		case K_KP_RIGHTARROW:
		case K_RIGHTARROW:
			if (f->buffer[f->cursor] != '\0')
			{
				f->cursor++;
			}
			break;

		case K_BACKSPACE:
			if (f->cursor > 0)
			{
				Q_strdel(f->buffer, f->cursor - 1, 1);
				f->cursor--;
			}
			break;

		case K_END:
			if (f->buffer[f->cursor] == '\0')
			{
				f->cursor = 0;
			}
			else
			{
				f->cursor = strlen(f->buffer);
			}
			break;

		case K_KP_DEL:
		case K_DEL:
			if (f->buffer[f->cursor] != '\0')
			{
				Q_strdel(f->buffer, f->cursor, 1);
			}
			break;

		case K_KP_ENTER:
		case K_ENTER:
		case K_ESCAPE:
		case K_TAB:
			return false;

		default:
			if (key > 127)
			{
				return false;
			}

			if (!isdigit(key) && (f->generic.flags & QMF_NUMBERSONLY))
			{
				return false;
			}

			*txt = key;
			*(txt + 1) = '\0';

			f->cursor += Q_strins(f->buffer, txt, f->cursor, f->length);
	}

	return true;
}

void
Menu_AddItem(menuframework_s *menu, void *item)
{
	if ((menu->nitems >= 0) && (menu->nitems < MAXMENUITEMS))
	{
		menu->items[menu->nitems] = item;
		((menucommon_s *)item)->parent = menu;
		menu->nitems++;
	}
}

/*
 * This function takes the given menu, the direction, and attempts
 * to adjust the menu's cursor so that it's at the next available
 * slot.
 */
void
Menu_AdjustCursor(menuframework_s *m, int dir)
{
	menucommon_s *citem = NULL;

	/* see if it's in a valid spot */
	if ((m->cursor >= 0) && (m->cursor < m->nitems))
	{
		if ((citem = Menu_ItemAtCursor(m)) != NULL)
		{
			if (citem->type != MTYPE_SEPARATOR &&
				(citem->flags & QMF_INACTIVE) != QMF_INACTIVE)
			{
				return;
			}
		}
	}

	/* it's not in a valid spot, so crawl in the direction
	   indicated until we find a valid spot */
    int cursor = m->nitems;

    while (cursor-- > 0)
    {
		citem = Menu_ItemAtCursor(m);

		if (citem)
		{
			if (citem->type != MTYPE_SEPARATOR &&
			(citem->flags & QMF_INACTIVE) != QMF_INACTIVE)
			{
				break;
			}
		}

		m->cursor += dir;

		if (m->cursor >= m->nitems)
		{
			m->cursor = 0;
		}

		if (m->cursor < 0)
		{
			m->cursor = m->nitems - 1;
		}
	}
}

void
Menu_Center(menuframework_s *menu)
{
	int height;
	float scale = SCR_GetMenuScale();

	height = ((menucommon_s *)menu->items[menu->nitems - 1])->y;
	height += 10;

	menu->y = (VID_HEIGHT / scale - height) / 2;
}

void
Menu_Draw(menuframework_s *menu)
{
	int i;
	menucommon_s *item;
	float scale = SCR_GetMenuScale();

	/* draw contents */
	for (i = 0; i < menu->nitems; i++)
	{
		switch (((menucommon_s *)menu->items[i])->type)
		{
			case MTYPE_FIELD:
				Field_Draw((menufield_s *)menu->items[i]);
				break;
			case MTYPE_SLIDER:
				Slider_Draw((menuslider_s *)menu->items[i]);
				break;
			case MTYPE_LIST:
				MenuList_Draw((menulist_s *)menu->items[i]);
				break;
			case MTYPE_SPINCONTROL:
				SpinControl_Draw((menulist_s *)menu->items[i]);
				break;
			case MTYPE_BITMAP:
				Bitmap_Draw(( menubitmap_s * )menu->items[i]);
				break;
			case MTYPE_ACTION:
				Action_Draw((menuaction_s *)menu->items[i]);
				break;
			case MTYPE_SEPARATOR:
				Separator_Draw((menuseparator_s *)menu->items[i]);
				break;
		}
	}

	item = Menu_ItemAtCursor(menu);

	if (item && item->cursordraw)
	{
		item->cursordraw(item);
	}
	else if (menu->cursordraw)
	{
		menu->cursordraw(menu);
	}
	else if (item && (item->type != MTYPE_FIELD) && item->type != MTYPE_BITMAP)
	{
		if (item->flags & QMF_LEFT_JUSTIFY)
		{
			Draw_CharScaled(menu->x + (item->x / scale - 24 + item->cursor_offset) * scale,
					(menu->y + item->y) * scale,
					12 + ((int)(Sys_Milliseconds() / 250) & 1), scale);
		}
		else
		{
			// FIXME:: menu->x + (item->x / scale + 24 + item->cursor_offset) * scale
			Draw_CharScaled(menu->x + (item->cursor_offset) * scale,
					(menu->y + item->y) * scale,
					12 + ((int)(Sys_Milliseconds() / 250) & 1), scale);
		}
	}

	if (item)
	{
		if (item->statusbarfunc)
		{
			item->statusbarfunc((void *)item);
		}

		else if (item->statusbar)
		{
			Menu_DrawStatusBar(item->statusbar);
		}

		else
		{
			Menu_DrawStatusBar(menu->statusbar);
		}
	}
	else
	{
		Menu_DrawStatusBar(menu->statusbar);
	}
}

static void
Menu_DrawStatusBar(const char *string)
{
	float scale = SCR_GetMenuScale();

	if (string)
	{
		int l = (int)strlen(string);
		int col = (VID_WIDTH / 2) - (l*8 / 2) * scale;

		Draw_Fill(0, VID_HEIGHT - 8 * scale, VID_WIDTH, 8 * scale, 4);
		Menu_DrawString(col, VID_HEIGHT / scale - 8, string);
	}
	else
	{
		Draw_Fill(0, VID_HEIGHT - 8 * scale, VID_WIDTH, 8 * scale, 0);
	}
}

/*
 * Draws one solid graphics character cx and cy are in 320*240
 * coordinates, and will be centered on higher res screens.
 */
void
Menu_DrawCharacter(int cx, int cy, int num)
{
	float scale = SCR_GetMenuScale();

	Draw_CharScaled(cx + ((int)(viddef.width - 320 * scale) >> 1), cy + ((int)(viddef.height - 240 * scale) >> 1), num, scale);
}

void
Menu_DrawString(int x, int y, const char *string)
{
	unsigned i;
	float scale = SCR_GetMenuScale();

	for (i = 0; string[i] != '\0'; i++)
	{
		Draw_CharScaled(x + i * 8 * scale, y * scale, string[i], scale);
	}
}

void
Menu_DrawStringDark(int x, int y, const char *string)
{
	unsigned i;
	float scale = SCR_GetMenuScale();

	for (i = 0; string[i] != '\0'; i++)
	{
		Draw_CharScaled(x + i * 8 * scale, y * scale, string[i] + 128, scale);
	}
}

void
Menu_DrawStringR2L(int x, int y, const char *string)
{
	unsigned i, slen;
	float scale;

	slen = strlen(string);
	scale = SCR_GetMenuScale();

	for (i = 0; i < slen; i++)
	{
		Draw_CharScaled(x - i * 8 * scale, y * scale, string[slen - i - 1], scale);
	}
}

void
Menu_DrawStringR2LDark(int x, int y, const char *string)
{
	unsigned i, slen;
	float scale;

	slen = strlen(string);
	scale = SCR_GetMenuScale();

	for (i = 0; i < slen; i++)
	{
		Draw_CharScaled(x - i * 8 * scale, y * scale, string[slen - i - 1] + 128, scale);
	}
}

void
Menu_DrawTextBox(int x, int y, int width, int lines)
{
	int cx, cy;
	int n;
	float scale = SCR_GetMenuScale();

	/* draw left side */
	cx = x;
	cy = y;
	Menu_DrawCharacter(cx * scale, cy * scale, 1);

	for (n = 0; n < lines; n++)
	{
		cy += 8;
		Menu_DrawCharacter(cx * scale, cy * scale, 4);
	}

	Menu_DrawCharacter(cx * scale, cy * scale + 8 * scale, 7);

	/* draw middle */
	cx += 8;

	while (width > 0)
	{
		cy = y;
		Menu_DrawCharacter(cx * scale, cy * scale, 2);

		for (n = 0; n < lines; n++)
		{
			cy += 8;
			Menu_DrawCharacter(cx * scale, cy * scale, 5);
		}

		Menu_DrawCharacter(cx * scale, cy *scale + 8 * scale, 8);
		width -= 1;
		cx += 8;
	}

	/* draw right side */
	cy = y;
	Menu_DrawCharacter(cx * scale, cy * scale, 3);

	for (n = 0; n < lines; n++)
	{
		cy += 8;
		Menu_DrawCharacter(cx * scale, cy * scale, 6);
	}

	Menu_DrawCharacter(cx * scale, cy * scale + 8 * scale, 9);
}

void
Menu_DrawText(int x, int y, const char *str)
{
	int cx, cy;
	float scale;

	scale = SCR_GetMenuScale();
	cx = x;
	cy = y;

	for (; *str != '\0'; str++)
	{
		if (*str == '\n')
		{
			cx = x;
			cy += 8;
		}
		else
		{
			Menu_DrawCharacter(cx * scale, cy * scale, (*str) + 128);
			cx += 8;
		}
	}
}

static void
GetString2DSize(const char *s, int *w_out, int *h_out)
{
	int n, w, h;

	n = 0;
	w = 0;
	h = 0;

	for (; *s != '\0'; s++)
	{
		if (*s == '\n')
		{
			h++;
			n = 0;
		}
		else
		{
			n++;

			if (n > w)
			{
				w = n;
			}
		}
	}

	if (n)
	{
		h++;
	}

	*w_out = w;
	*h_out = h;
}

void
Menu_DrawPopup(int x, int y, const menupopup_s *pup)
{
	int w, h;

	if (!pup->string)
	{
		return;
	}

	if (pup->endtime > 0 && pup->endtime < cls.realtime)
	{
		return;
	}

	if (!R_EndWorldRenderpass())
	{
		return;
	}

	GetString2DSize(pup->string, &w, &h);

	if (w)
	{
		int cx, cy;

		w += 2;

		cx = (x - (w + 2) * 8) / 2;
		cy = (y - (h + 2) * 8) / 3;

		Menu_DrawTextBox(cx, cy, w, h);
		Menu_DrawText(cx + 16, cy + 8, pup->string);
	}
}

void
Menu_StartPopup(menupopup_s *pup, const char *string, int lifetime)
{
	pup->string = string;
	pup->endtime = (lifetime <= 0) ? 0 : (cls.realtime + lifetime);
}

void *
Menu_ItemAtCursor(menuframework_s *m)
{
	if ((m->cursor < 0) || (m->cursor >= m->nitems))
	{
		return NULL;
	}

	return m->items[m->cursor];
}

qboolean
Menu_SelectItem(menuframework_s *s)
{
	menucommon_s *item = (menucommon_s *)Menu_ItemAtCursor(s);

	if (item->callback)
	{
		item->callback(item);

		return true;
	}

	return false;
}

void
Menu_SetStatusBar(menuframework_s *m, const char *string)
{
	m->statusbar = string;
}

qboolean
Menu_SlideItem(menuframework_s *s, int dir)
{
	menucommon_s *item = (menucommon_s *)Menu_ItemAtCursor(s);

	if (!item)
	{
		return false;
	}

	switch (item->type)
	{
		case MTYPE_SLIDER:
			return Slider_DoSlide((menuslider_s *)item, dir);
		case MTYPE_SPINCONTROL:
			return SpinControl_DoSlide((menulist_s *)item, dir);
	}

	return false;
}

static void
MenuList_Draw(menulist_s *l)
{
	const char **n;
	float scale = SCR_GetMenuScale();
	int x = 0;
	int y = 0;

	x = l->generic.parent->x + l->generic.x;
	y = l->generic.parent->y + l->generic.y;

	Menu_DrawStringR2LDark(x + (LCOLUMN_OFFSET * scale),
		y, l->generic.name);

	n = l->itemnames;

	Draw_Fill(x - 112,
		y + l->curvalue * 10 * scale + 10 * scale, 128, 10, 16);

	while (*n)
	{
		Menu_DrawStringR2LDark(x + (LCOLUMN_OFFSET * scale),
			y, *n);

		n++;
		y += 10 * scale;
	}
}

static void
Separator_Draw(menuseparator_s *s)
{
	int x = 0;
	int y = 0;

	x = s->generic.parent->x + s->generic.x;
	y = s->generic.parent->y + s->generic.y;

	if (s->generic.name)
	{
		Menu_DrawStringR2LDark(x,
			y, s->generic.name);
	}
}

static qboolean
Slider_DoSlide(menuslider_s *s, int dir)
{
	const float step = (s->slidestep)? s->slidestep : 0.1f;
	float value;
	float sign = 1.0f;

	if (!s->cvar)
	{
		return false;
	}

	value = Cvar_VariableValue(s->cvar);

	if (s->abs && value < 0)	// absolute value treatment
	{
		value = -value;
		sign = -1.0f;
	}

	dir = (dir <= 0) ? -1 : 1;

	if ((value == s->minvalue && dir == -1) ||
		(value == s->maxvalue && dir == 1))
	{
		return false;
	}

	value += dir * step;
	Cvar_SetValue(s->cvar, ClampCvar(s->minvalue, s->maxvalue, value) * sign);

	if (s->generic.callback)
	{
		s->generic.callback(s);
	}

	return true;
}

#define SLIDER_RANGE 10

static void
Slider_Draw(menuslider_s *s)
{
	float scale;
	int x, x_rcol, y;
	int i;

	scale = SCR_GetMenuScale();
	x = s->generic.parent->x + s->generic.x;
	x_rcol = x + (RCOLUMN_OFFSET * scale);
	y = s->generic.parent->y + s->generic.y;

	if (s->generic.name)
	{
		Menu_DrawStringR2LDark(x + (LCOLUMN_OFFSET * scale),
			y, s->generic.name);
	}

	Draw_CharScaled(x_rcol,
		y * scale, 128, scale);

	for (i = 0; i < SLIDER_RANGE * scale; i++)
	{
		Draw_CharScaled(x_rcol + (i * 8) + 8,
			y * scale, 129, scale);
	}

	Draw_CharScaled(x_rcol + (i * 8) + 8,
		y * scale, 130, scale);

	if (s->cvar)
	{
		float value, range;
		char buffer[5];

		value = Cvar_VariableValue(s->cvar);
		if (s->abs && value < 0)
		{
			value = -value;
		}

		range = (ClampCvar(s->minvalue, s->maxvalue, value) - s->minvalue) /
			(s->maxvalue - s->minvalue);

		Draw_CharScaled(x_rcol + (int)((SLIDER_RANGE * scale - 1) * 8 * range) + 8,
			y * scale, 131, scale);

		Com_sprintf(buffer, sizeof(buffer),
			(s->printformat)? s->printformat : "%.1f", value);
		Menu_DrawString(x_rcol + ((SLIDER_RANGE + 2) * scale * 8),
			y, buffer);
	}
}

static int
NumItemNames(const char **items)
{
	const char **i;

	for (i = items; *i; i++)
	{
	}

	return i - items;
}

static qboolean
SpinControl_DoSlide(menulist_s *s, int dir)
{
	if (!s->itemnames || !s->itemnames[0])
	{
		return false;
	}

	if ((s->curvalue < 0) ||
		(s->curvalue >= NumItemNames(s->itemnames)))
	{
		s->curvalue = 0;
	}
	else
	{
		dir = (dir <= 0) ? -1 : 1;

		if ((s->curvalue == 0 && dir == -1) ||
			(!s->itemnames[s->curvalue + 1] && dir == 1))
		{
			return false;
		}

		s->curvalue += dir;
	}

	if (s->generic.callback)
	{
		s->generic.callback(s);
	}

	return true;
}

static const char *
GetSelectedItem(const menulist_s *s)
{
	if (!s->itemnames || !s->itemnames[0])
	{
		return "(empty)";
	}

	if ((s->curvalue < 0) ||
		(s->curvalue >= NumItemNames(s->itemnames)))
	{
		return "(invalid)";
	}

	return s->itemnames[s->curvalue];
}

static void
SpinControl_Draw(menulist_s *s)
{
	const char *item, *nl;
	float scale;
	int x, y;

	scale = SCR_GetMenuScale();
	x = s->generic.parent->x + s->generic.x;
	y = s->generic.parent->y + s->generic.y;

	if (s->generic.name)
	{
		Menu_DrawStringR2LDark(x + (LCOLUMN_OFFSET * scale),
			y, s->generic.name);
	}

	item = GetSelectedItem(s);
	nl = strchr(item, '\n');

	if (!nl)
	{
		Menu_DrawString(x + (RCOLUMN_OFFSET * scale),
			y, item);
	}
	else
	{
		char buffer[100];

		if (Q_strlcpy(buffer, item, sizeof(buffer)) < sizeof(buffer))
		{
			char *nlb;

			nlb = buffer + (nl - item);
			*nlb = '\0';

			Menu_DrawString(x + (RCOLUMN_OFFSET * scale),
				y + 10, nlb + 1);
		}

		Menu_DrawString(x + (RCOLUMN_OFFSET * scale),
			y, buffer);
	}
}
