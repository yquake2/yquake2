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
static void Slider_DoSlide(menuslider_s *s, int dir);
static void Slider_Draw(menuslider_s *s);
static void SpinControl_Draw(menulist_s *s);
static void SpinControl_DoSlide(menulist_s *s, int dir);

extern viddef_t viddef;

#define VID_WIDTH viddef.width
#define VID_HEIGHT viddef.height

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

void
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
	if (menu->nitems < MAXMENUITEMS)
	{
		menu->items[menu->nitems] = item;
		((menucommon_s *)menu->items[menu->nitems])->parent = menu;
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
		if ((citem = Menu_ItemAtCursor(m)) != 0)
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

void
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

void
Menu_DrawString(int x, int y, const char *string)
{
	unsigned i;
	float scale = SCR_GetMenuScale();

	for (i = 0; i < strlen(string); i++)
	{
		Draw_CharScaled(x + i * 8 * scale, y * scale, string[i], scale);
	}
}

void
Menu_DrawStringDark(int x, int y, const char *string)
{
	unsigned i;
	float scale = SCR_GetMenuScale();

	for (i = 0; i < strlen(string); i++)
	{
		Draw_CharScaled(x + i * 8 * scale, y * scale, string[i] + 128, scale);
	}
}

void
Menu_DrawStringR2L(int x, int y, const char *string)
{
	unsigned i;
	float scale = SCR_GetMenuScale();

	for (i = 0; i < strlen(string); i++)
	{
		Draw_CharScaled(x - i * 8 * scale, y * scale, string[strlen(string) - i - 1], scale);
	}
}

void
Menu_DrawStringR2LDark(int x, int y, const char *string)
{
	unsigned i;
	float scale = SCR_GetMenuScale();

	for (i = 0; i < strlen(string); i++)
	{
		Draw_CharScaled(x - i * 8 * scale, y * scale, string[strlen(string) - i - 1] + 128, scale);
	}
}

void *
Menu_ItemAtCursor(menuframework_s *m)
{
	if ((m->cursor < 0) || (m->cursor >= m->nitems))
	{
		return 0;
	}

	return m->items[m->cursor];
}

qboolean
Menu_SelectItem(menuframework_s *s)
{
	menucommon_s * item = ( menucommon_s * )Menu_ItemAtCursor(s);

	if (item->callback) {

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

void
Menu_SlideItem(menuframework_s *s, int dir)
{
	menucommon_s *item = (menucommon_s *)Menu_ItemAtCursor(s);

	if (item)
	{
		switch (item->type)
		{
			case MTYPE_SLIDER:
				Slider_DoSlide((menuslider_s *)item, dir);
				break;
			case MTYPE_SPINCONTROL:
				SpinControl_DoSlide((menulist_s *)item, dir);
				break;
		}
	}
}

void
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

void
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

void
Slider_DoSlide(menuslider_s *s, int dir)
{
	const float step = (s->slidestep)? s->slidestep : 0.1f;
	float value = Cvar_VariableValue(s->cvar);
	float sign = 1.0f;

	if (s->abs && value < 0)	// absolute value treatment
	{
		value = -value;
		sign = -1.0f;
	}

	value += dir * step;
	Cvar_SetValue(s->cvar, ClampCvar(s->minvalue, s->maxvalue, value) * sign);

	if (s->generic.callback)
	{
		s->generic.callback(s);
	}
}

#define SLIDER_RANGE 10

void
Slider_Draw(menuslider_s *s)
{
	const float scale = SCR_GetMenuScale();
	const int x = s->generic.parent->x + s->generic.x;
	const int y = s->generic.parent->y + s->generic.y;
	const int x_rcol = x + (RCOLUMN_OFFSET * scale);
	int i;
	char buffer[5];

	float value = Cvar_VariableValue(s->cvar);
	if (s->abs && value < 0)	// absolute value
	{
		value = -value;
	}
	const float range = (ClampCvar(s->minvalue, s->maxvalue, value) - s->minvalue) /
			(s->maxvalue - s->minvalue);

	Menu_DrawStringR2LDark(x + (LCOLUMN_OFFSET * scale),
		y, s->generic.name);

	Draw_CharScaled(x_rcol,
		y * scale, 128, scale);

	for (i = 0; i < SLIDER_RANGE * scale; i++)
	{
		Draw_CharScaled(x_rcol + (i * 8) + 8,
			y * scale, 129, scale);
	}

	Draw_CharScaled(x_rcol + (i * 8) + 8,
		y * scale, 130, scale);
	Draw_CharScaled(x_rcol + (int)((SLIDER_RANGE * scale - 1) * 8 * range) + 8,
		y * scale, 131, scale);

	snprintf(buffer, 5, (s->printformat)? s->printformat : "%.1f", value);
	Menu_DrawString(x_rcol + ((SLIDER_RANGE + 2) * scale * 8),
		y, buffer);
}

void
SpinControl_DoSlide(menulist_s *s, int dir)
{
	s->curvalue += dir;

	if (s->curvalue < 0)
	{
		s->curvalue = 0;
		return;
	}
	else if (s->itemnames[s->curvalue] == 0)
	{
		s->curvalue--;
		return;
	}

	if (s->generic.callback)
	{
		s->generic.callback(s);
	}
}

void
SpinControl_Draw(menulist_s *s)
{
	char buffer[100];
	float scale = SCR_GetMenuScale();
	int x = 0;
	int y = 0;

	x = s->generic.parent->x + s->generic.x;
	y = s->generic.parent->y + s->generic.y;

	if (s->generic.name)
	{
		Menu_DrawStringR2LDark(x + (LCOLUMN_OFFSET * scale),
			y, s->generic.name);
	}

	if (!strchr(s->itemnames[s->curvalue], '\n'))
	{
		Menu_DrawString(x + (RCOLUMN_OFFSET * scale),
			y, s->itemnames[s->curvalue]);
	}
	else
	{
		Q_strlcpy(buffer, s->itemnames[s->curvalue], sizeof(buffer));
		*strchr(buffer, '\n') = 0;
		Menu_DrawString(x + (RCOLUMN_OFFSET * scale),
			y, buffer);
		strcpy(buffer, strchr(s->itemnames[s->curvalue], '\n') + 1);
		Menu_DrawString(x + (RCOLUMN_OFFSET * scale),
			y + 10, buffer);
	}
}

