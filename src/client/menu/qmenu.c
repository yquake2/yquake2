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

float
ClampCvar(float min, float max, float value)
{
	if (value < min)
	{
		return min;
	}

	if (value > max)
	{
		return max;
	}

	return value;
}

/*
=================
Bitmap_Draw
=================
*/
void Bitmap_Draw(menubitmap_s * item)
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

qboolean
Field_DoEnter(menufield_s *f)
{
	if (f->generic.callback)
	{
		f->generic.callback(f);
		return true;
	}

	return false;
}

void
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

	Q_strlcpy(tempbuffer, f->buffer + f->visible_offset, n);

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
		int offset;

		if (f->visible_offset)
		{
			offset = f->visible_length;
		}

		else
		{
			offset = f->cursor;
		}

		if (((int)(Sys_Milliseconds() / 250)) & 1)
		{
			Draw_CharScaled(
				x + (24 * scale) + (offset * (8 * scale)),
				y * scale, 11, scale);
		}
		else
		{
			Draw_CharScaled(
				x + (24 * scale) + (offset * (8 * scale)),
				y * scale, ' ', scale);
		}
	}
}

extern int keydown[];

qboolean
Field_Key(menufield_s *f, int key)
{
	if (key > 127)
	{
		return false;
	}

	switch (key)
	{
		case K_KP_LEFTARROW:
		case K_LEFTARROW:
		case K_BACKSPACE:

			if (f->cursor > 0)
			{
				memmove(&f->buffer[f->cursor - 1],
						&f->buffer[f->cursor],
						strlen(&f->buffer[f->cursor]) + 1);
				f->cursor--;

				if (f->visible_offset)
				{
					f->visible_offset--;
				}
			}

			break;

		case K_KP_DEL:
		case K_DEL:
			memmove(&f->buffer[f->cursor], &f->buffer[f->cursor + 1],
				strlen(&f->buffer[f->cursor + 1]) + 1);
			break;

		case K_KP_ENTER:
		case K_ENTER:
		case K_ESCAPE:
		case K_TAB:
			return false;

		case K_SPACE:
		default:

			if (!isdigit(key) && (f->generic.flags & QMF_NUMBERSONLY))
			{
				return false;
			}

			if (f->cursor < f->length)
			{
				f->buffer[f->cursor++] = key;
				f->buffer[f->cursor] = 0;

				if (f->cursor > f->visible_length)
				{
					f->visible_offset++;
				}
			}
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
	float value = Cvar_VariableValue(s->cvar);
	float step = 0.1f;
	float sign = 1.0f;

	if (s->slidestep)
	{
		step = s->slidestep;
	}
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
	int i;
	char buffer[5];
	const char * format;
	float scale = SCR_GetMenuScale();
	int x = s->generic.parent->x + s->generic.x;
	int y = s->generic.parent->y + s->generic.y;

	float value = Cvar_VariableValue(s->cvar);
	if (s->abs && value < 0)	// absolute value
	{
		value = -value;
	}
	float range = (ClampCvar(s->minvalue, s->maxvalue, value) - s->minvalue) /
			(s->maxvalue - s->minvalue);

	Menu_DrawStringR2LDark(x + (LCOLUMN_OFFSET * scale),
		y, s->generic.name);

	Draw_CharScaled(x + (RCOLUMN_OFFSET * scale),
		y * scale, 128, scale);

	for (i = 0; i < SLIDER_RANGE * scale; i++)
	{
		Draw_CharScaled(x + (RCOLUMN_OFFSET * scale) + (i * 8) + 8,
			y * scale, 129, scale);
	}

	Draw_CharScaled(x + (RCOLUMN_OFFSET * scale) + (i * 8) + 8,
		y * scale, 130, scale);
	Draw_CharScaled(x + ((int)((RCOLUMN_OFFSET * scale) + (SLIDER_RANGE * scale - 1) * 8 * range)) + 8,
		y * scale, 131, scale);

	if (!s->printformat)
	{
		format = "%.1f";
	}
	else
	{
		format = s->printformat;
	}
	snprintf(buffer, 5, format, value);
	Menu_DrawString(x + (RCOLUMN_OFFSET * scale) + ((SLIDER_RANGE + 2) * scale * 8),
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
		strcpy(buffer, s->itemnames[s->curvalue]);
		*strchr(buffer, '\n') = 0;
		Menu_DrawString(x + (RCOLUMN_OFFSET * scale),
			y, buffer);
		strcpy(buffer, strchr(s->itemnames[s->curvalue], '\n') + 1);
		Menu_DrawString(x + (RCOLUMN_OFFSET * scale),
			y + 10, buffer);
	}
}

