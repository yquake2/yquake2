/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2011 Knightmare
 * Copyright (C) 2011 Yamagi Burmeister
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
 * The savegame system.
 *
 * =======================================================================
 */

/*
 * This is the Quake 2 savegame system, fixed by Yamagi
 * based on an idea by Knightmare of kmquake2. This major
 * rewrite of the original g_save.c is much more robust
 * and portable since it doesn't use any function pointers.
 *
 * Inner workings:
 * When the game is saved all function pointers are
 * translated into human readable function definition strings.
 * The same way all mmove_t pointers are translated. This
 * human readable strings are then written into the file.
 * At game load the human readable strings are retranslated
 * into the actual function pointers and struct pointers. The
 * pointers are generated at each compilation / start of the
 * client, thus the pointers are always correct.
 *
 * Limitations:
 * While savegames survive recompilations of the game source
 * and bigger changes in the source, there are some limitation
 * which a nearly impossible to fix without a object orientated
 * rewrite of the game.
 *  - If functions or mmove_t structs that a referencenced
 *    inside savegames are added or removed (e.g. the files
 *    in tables/ are altered) the load functions cannot
 *    reconnect all pointers and thus not restore the game.
 *  - If the operating system is changed internal structures
 *    may change in an unrepairable way.
 *  - If the architecture is changed pointer length and
 *    other internal datastructures change in an
 *    incompatible way.
 *  - If the edict_t struct is changed, savegames
 *    will break.
 * This is not so bad as it looks since functions and
 * struct won't be added and edict_t won't be changed
 * if no big, sweeping changes are done. The operating
 * system and architecture are in the hands of the user.
 */

#include "../../common/header/common.h" // YQ2ARCH
#include "../header/local.h"
#include "savegame.h"
/*
 * When ever the savegame version is changed, q2 will refuse to
 * load older savegames. This should be bumped if the files
 * in tables/ are changed, otherwise strange things may happen.
 */
#define SAVEGAMEVER "YQ2-5"

#ifndef BUILD_DATE
#define BUILD_DATE __DATE__
#endif

/*
 * This macros are used to prohibit loading of savegames
 * created on other systems or architectures. This will
 * crash q2 in spectacular ways
 */
#ifndef YQ2OSTYPE
#error YQ2OSTYPE should be defined by the build system
#endif

#ifndef YQ2ARCH
#error YQ2ARCH should be defined by the build system
#endif

/*
 * Older operating system and architecture detection
 * macros, implemented by savegame version YQ2-1.
 */
#if defined(__APPLE__)
#define OSTYPE_1 "MacOS X"
#elif defined(__FreeBSD__)
#define OSTYPE_1 "FreeBSD"
#elif defined(__OpenBSD__)
#define OSTYPE_1 "OpenBSD"
#elif defined(__linux__)
 #define OSTYPE_1 "Linux"
#elif defined(_WIN32)
 #define OSTYPE_1 "Windows"
#else
 #define OSTYPE_1 "Unknown"
#endif

#if defined(__i386__)
#define ARCH_1 "i386"
#elif defined(__x86_64__)
#define ARCH_1 "amd64"
#elif defined(__sparc__)
#define ARCH_1 "sparc64"
#elif defined(__ia64__)
 #define ARCH_1 "ia64"
#else
 #define ARCH_1 "unknown"
#endif

/* ========================================================= */

/*
 * Prototypes for forward
 * declaration for all game
 * functions.
 */
#include "tables/gamefunc_decs.h"

/*
 * List with function pointer
 * to each of the functions
 * prototyped above.
 */
static const functionList_t functionList[] = {
	#include "tables/gamefunc_list.h"
};

/*
 * Prototypes for forward
 * declaration for all game
 * mmove_t functions.
 */
#include "tables/gamemmove_decs.h"

/*
 * List with pointers to
 * each of the mmove_t
 * functions prototyped
 * above.
 */
static const mmoveList_t mmoveList[] = {
	#include "tables/gamemmove_list.h"
};

/*
 * Entity fields to be saved
 */
static const field_t entfields[] = {
	#include "tables/entfields.h"
};

/*
 * Level fields to be saved
 */
static const field_t levelfields[] = {
	#include "tables/levelfields.h"
};

/*
 * Client fields to be saved
 */
static const field_t clientfields[] = {
	#include "tables/clientfields.h"
};

/* ========================================================= */

static void
sg_fread(void *dest, size_t n, FILE *f)
{
	if (fread(dest, n, 1, f) != 1)
	{
		fclose(f);
		gi.error("Error reading " YQ2_COM_PRIdS " bytes from save file", n);
	}
}

static void
sg_fwrite(const void *src, size_t n, FILE *f)
{
	if (fwrite(src, n, 1, f) != 1)
	{
		fclose(f);
		gi.error("Error writing " YQ2_COM_PRIdS " bytes to save file", n);
	}
}

const field_t *
FindSpawnfield(const char *key)
{
	const field_t *f;

	for (f = entfields; f < ARREND(entfields); f++)
	{
		if (!(f->flags & FFL_NOSPAWN) && !Q_strcasecmp(f->name, key))
		{
			return f;
		}
	}

	return NULL;
}

static void
InitAllocations(void)
{
	int num_c = maxclients->value;
	int num_e = maxentities->value;

	if (num_c < 1)
	{
		num_c = 1;
		gi.cvar_forceset("maxclients", "1");
	}

	if (num_e < (num_c + 1))
	{
		num_e = num_c + 1;
		gi.cvar_forceset("maxentities", va("%d", num_c + 1));
	}

	g_edicts = gi.TagMalloc (num_e * sizeof(g_edicts[0]), TAG_GAME);
	game.maxentities = num_e;

	globals.edicts = g_edicts;
	globals.num_edicts = num_c + 1;
	globals.max_edicts = num_e;

	game.clients = gi.TagMalloc (num_c * sizeof(game.clients[0]), TAG_GAME);
	game.maxclients = num_c;
}

/*
 * This will be called when the dll is first loaded,
 * which only happens when a new game is started or
 * a save game is loaded.
 */
void
InitGame(void)
{
	gi.dprintf("Game is starting up.\n");
	gi.dprintf("Game is %s built on %s.\n", GAMEVERSION, BUILD_DATE);

	gun_x = gi.cvar("gun_x", "0", 0);
	gun_y = gi.cvar("gun_y", "0", 0);
	gun_z = gi.cvar("gun_z", "0", 0);
	sv_rollspeed = gi.cvar("sv_rollspeed", "200", 0);
	sv_rollangle = gi.cvar("sv_rollangle", "2", 0);
	sv_maxvelocity = gi.cvar("sv_maxvelocity", "2000", 0);
	sv_gravity = gi.cvar("sv_gravity", "800", 0);

	/* noset vars */
	dedicated = gi.cvar("dedicated", "0", CVAR_NOSET);

	/* latched vars */
	sv_cheats = gi.cvar("cheats", "0", CVAR_SERVERINFO | CVAR_LATCH);
	gi.cvar("gamename", GAMEVERSION, CVAR_SERVERINFO | CVAR_LATCH);
	gi.cvar("gamedate", BUILD_DATE, CVAR_SERVERINFO | CVAR_LATCH);
	maxclients = gi.cvar("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
	maxspectators = gi.cvar("maxspectators", "4", CVAR_SERVERINFO);
	deathmatch = gi.cvar("deathmatch", "0", CVAR_LATCH);
	coop = gi.cvar("coop", "0", CVAR_LATCH);
	coop_pickup_weapons = gi.cvar("coop_pickup_weapons", "1", CVAR_ARCHIVE);
	coop_elevator_delay = gi.cvar("coop_elevator_delay", "1.0", CVAR_ARCHIVE);
	skill = gi.cvar("skill", "1", CVAR_LATCH);
	maxentities = gi.cvar("maxentities", "1024", CVAR_LATCH);
	g_footsteps = gi.cvar("g_footsteps", "1", CVAR_ARCHIVE);
	g_monsterfootsteps = gi.cvar("g_monsterfootsteps", "0", CVAR_ARCHIVE);
	g_fix_triggered = gi.cvar ("g_fix_triggered", "0", 0);
	g_commanderbody_nogod = gi.cvar("g_commanderbody_nogod", "0", CVAR_ARCHIVE);

	/* change anytime vars */
	dmflags = gi.cvar("dmflags", "0", CVAR_SERVERINFO);
	fraglimit = gi.cvar("fraglimit", "0", CVAR_SERVERINFO);
	timelimit = gi.cvar("timelimit", "0", CVAR_SERVERINFO);
	password = gi.cvar("password", "", CVAR_USERINFO);
	spectator_password = gi.cvar("spectator_password", "", CVAR_USERINFO);
	needpass = gi.cvar("needpass", "0", CVAR_SERVERINFO);
	filterban = gi.cvar("filterban", "1", 0);
	g_select_empty = gi.cvar("g_select_empty", "0", CVAR_ARCHIVE);
	run_pitch = gi.cvar("run_pitch", "0.002", 0);
	run_roll = gi.cvar("run_roll", "0.005", 0);
	bob_up = gi.cvar("bob_up", "0.005", 0);
	bob_pitch = gi.cvar("bob_pitch", "0.002", 0);
	bob_roll = gi.cvar("bob_roll", "0.002", 0);

	/* flood control */
	flood_msgs = gi.cvar("flood_msgs", "4", 0);
	flood_persecond = gi.cvar("flood_persecond", "4", 0);
	flood_waitdelay = gi.cvar("flood_waitdelay", "10", 0);

	/* dm map list */
	sv_maplist = gi.cvar("sv_maplist", "", 0);

	/* others */
	aimfix = gi.cvar("aimfix", "0", CVAR_ARCHIVE);
	g_machinegun_norecoil = gi.cvar("g_machinegun_norecoil", "0", CVAR_ARCHIVE);
	g_quick_weap = gi.cvar("g_quick_weap", "1", CVAR_ARCHIVE);
	g_swap_speed = gi.cvar("g_swap_speed", "1", CVAR_ARCHIVE);

	memset(&game, 0, sizeof(game));

	InitItems();

	/* initialize entities and clients arrays */
	InitAllocations();
}

/* ========================================================= */

/*
 * Helper function to get
 * the human readable function
 * definition by an address.
 * Called by WriteField1 and
 * WriteField2.
 */
static const functionList_t *
GetFunctionByAddress(const byte *adr)
{
	const functionList_t *fnl;

	for (fnl = functionList; fnl < ARREND(functionList); fnl++)
	{
		if (fnl->funcPtr == adr)
		{
			return fnl;
		}
	}

	return NULL;
}

/*
 * Helper function to get the
 * pointer to a function by
 * it's human readable name.
 * Called by WriteField1 and
 * WriteField2.
 */
static byte *
FindFunctionByName(const char *name)
{
	const functionList_t *fnl;

	for (fnl = functionList; fnl < ARREND(functionList); fnl++)
	{
		if (!strcmp(name, fnl->funcStr))
		{
			return fnl->funcPtr;
		}
	}

	return NULL;
}

/*
 * Helper function to get the
 * human readable definition of
 * a mmove_t struct by a pointer.
 */
static const mmoveList_t *
GetMmoveByAddress(const mmove_t *adr)
{
	const mmoveList_t *mml;

	for (mml = mmoveList; mml < ARREND(mmoveList); mml++)
	{
		if (mml->mmovePtr == adr)
		{
			return mml;
		}
	}

	return NULL;
}

/*
 * Helper function to get the
 * pointer to a mmove_t struct
 * by a human readable definition.
 */
static mmove_t *
FindMmoveByName(const char *name)
{
	const mmoveList_t *mml;

	for (mml = mmoveList; mml < ARREND(mmoveList); mml++)
	{
		if (!strcmp(name, mml->mmoveStr))
		{
			return mml->mmovePtr;
		}
	}

	return NULL;
}


/* ========================================================= */

static int
GetFuncLength(const byte *fn)
{
	const functionList_t *func;

	if (!fn)
	{
		return 0;
	}

	func = GetFunctionByAddress(fn);

	if (!func)
	{
		gi.dprintf("%s: function at address %p not found\n",
			__func__, fn);

		return 0;
	}

	return strlen(func->funcStr) + 1;
}

static int
GetMmoveLength(const mmove_t *mm)
{
	const mmoveList_t *mmove;

	if (!mm)
	{
		return 0;
	}

	mmove = GetMmoveByAddress(mm);

	if (!mmove)
	{
		gi.dprintf("%s: mmove at address %p not found\n",
			__func__, mm);
		return 0;
	}

	return strlen(mmove->mmoveStr) + 1;
}

/*
 * The following two functions are
 * doing the dirty work to write the
 * data generated by the functions
 * below this block into files.
 */
static void
WriteField1(FILE *f, const field_t *field, byte *base)
{
	void *p;
	size_t len;
	int index;

	if (field->flags & FFL_SPAWNTEMP)
	{
		return;
	}

	p = (void *)(base + field->ofs);

	switch (field->type)
	{
		case F_INT:
		case F_FLOAT:
		case F_ANGLEHACK:
		case F_VECTOR:
		case F_IGNORE:
			break;

		case F_LSTRING:
		case F_GSTRING:

			if (*(char **)p)
			{
				len = strlen(*(char **)p) + 1;
			}
			else
			{
				len = 0;
			}

			*(int *)p = len;
			break;
		case F_EDICT:

			if (*(edict_t **)p == NULL)
			{
				index = -1;
			}
			else
			{
				index = *(edict_t **)p - g_edicts;
			}

			*(int *)p = index;
			break;
		case F_CLIENT:

			if (*(gclient_t **)p == NULL)
			{
				index = -1;
			}
			else
			{
				index = *(gclient_t **)p - game.clients;
			}

			*(int *)p = index;
			break;
		case F_ITEM:

			if (*(gitem_t **)p == NULL)
			{
				index = -1;
			}
			else
			{
				index = ITEM_INDEX(*(gitem_t **)p);
			}

			*(int *)p = index;
			break;
		case F_FUNCTION:
			*(int *)p = GetFuncLength(*(byte **)p);
			break;
		case F_MMOVE:
			*(int *)p = GetMmoveLength(*(mmove_t **)p);
			break;
		default:
			fclose(f);
			gi.error("%s: unknown field type", __func__);
	}
}

static void
WriteFunction(FILE *f, const byte *fn)
{
	const functionList_t *func;

	if (!fn)
	{
		return;
	}

	func = GetFunctionByAddress(fn);

	if (func)
	{
		size_t len = strlen(func->funcStr) + 1;
		sg_fwrite(func->funcStr, len, f);
	}
}

static void
WriteMmove(FILE *f, const mmove_t *mm)
{
	const mmoveList_t *mmove;

	if (!mm)
	{
		return;
	}

	mmove = GetMmoveByAddress(mm);

	if (mmove)
	{
		size_t len = strlen(mmove->mmoveStr) + 1;
		sg_fwrite(mmove->mmoveStr, len, f);
	}
}

static void
WriteField2(FILE *f, const field_t *field, byte *base)
{
	size_t len;
	void *p;

	if (field->flags & FFL_SPAWNTEMP)
	{
		return;
	}

	p = (void *)(base + field->ofs);

	switch (field->type)
	{
		case F_LSTRING:

			if (*(char **)p)
			{
				len = strlen(*(char **)p) + 1;
				sg_fwrite(*(char **)p, len, f);
			}

			break;
		case F_FUNCTION:
			WriteFunction(f, *(byte **)p);
			break;
		case F_MMOVE:
			WriteMmove(f, *(mmove_t **)p);
			break;
		default:
			break;
	}
}

/* ========================================================= */

/* int because that is how it's stored in the file */
static void
ReadStringToBuf(FILE *f, int len, char *out, size_t out_sz)
{
	*out = 0;

	if (!len)
	{
		return;
	}

	if (len < 0)
	{
		fclose(f);
		gi.error("%s: string length < 0", __func__);
		return;
	}

	if (len >= (int)out_sz)
	{
		fclose(f);
		gi.error("%s: string is too long for buffer: %i > %i ",
				__func__, len, (int)out_sz);
		return;
	}

	sg_fread(out, len, f);
	out[len] = 0;
}

/* int because that is how it's stored in the file */
static char *
ReadString(FILE *f, int len, int tag)
{
	char *s;

	if (!len)
	{
		return NULL;
	}

	if (len < 0)
	{
		fclose(f);
		gi.error("%s: string length < 0", __func__);
		return NULL;
	}

	s = gi.TagMalloc(len + 1, tag);
	if (!s)
	{
		fclose(f);
		gi.error("%s: can't allocate memory for string", __func__);
		return NULL;
	}

	sg_fread(s, len, f);
	s[len] = 0;

	return s;
}

static byte *
ReadFunction(FILE *f, int len)
{
	char funcStr[128];
	byte *fn;

	if (!len)
	{
		return NULL;
	}

	ReadStringToBuf(f, len, funcStr, sizeof(funcStr));

	fn = FindFunctionByName(funcStr);
	if (!fn)
	{
		gi.dprintf("%s: function %s not found\n",
			__func__, funcStr);
	}

	return fn;
}

static mmove_t *
ReadMmove(FILE *f, int len)
{
	char mmoveStr[128];
	mmove_t *mm;

	if (!len)
	{
		return NULL;
	}

	ReadStringToBuf(f, len, mmoveStr, sizeof(mmoveStr));

	mm = FindMmoveByName(mmoveStr);
	if (!mm)
	{
		gi.dprintf("%s: mmove %s not found\n",
			__func__, mmoveStr);
	}

	return mm;
}

/*
 * This function does the dirty
 * work to read the data from a
 * file. The processing of the
 * data is done in the functions
 * below
 */
static void
ReadField(FILE *f, const field_t *field, byte *base)
{
	void *p;
	int len;
	int index;

	if (field->flags & FFL_SPAWNTEMP)
	{
		return;
	}

	p = (void *)(base + field->ofs);

	switch (field->type)
	{
		case F_INT:
		case F_FLOAT:
		case F_ANGLEHACK:
		case F_VECTOR:
		case F_IGNORE:
			break;

		case F_LSTRING:
			len = *(int *)p;
			*(char **)p = ReadString(f, len, TAG_LEVEL);
			break;
		case F_EDICT:
			index = *(int *)p;

			if ((index < 0) || (index >= game.maxentities))
			{
				*(edict_t **)p = NULL;
			}
			else
			{
				*(edict_t **)p = &g_edicts[index];
			}

			break;
		case F_CLIENT:
			index = *(int *)p;

			if ((index < 0) || (index >= game.maxclients))
			{
				*(gclient_t **)p = NULL;
			}
			else
			{
				*(gclient_t **)p = &game.clients[index];
			}

			break;
		case F_ITEM:
			index = *(int *)p;
			*(gitem_t **)p = GetItemByIndex(index);
			break;
		case F_FUNCTION:
			*(byte **)p = ReadFunction(f, *(int *)p);
			break;
		case F_MMOVE:
			*(mmove_t **)p = ReadMmove(f, *(int *)p);
			break;
		default:
			fclose(f);
			gi.error("%s: unknown field type", __func__);
	}
}

/* ========================================================= */

/*
 * Write the client struct into a file.
 */
static void
WriteClient(FILE *f, gclient_t *client)
{
	const field_t *field;
	gclient_t temp;

	/* all of the ints, floats, and vectors stay as they are */
	temp = *client;

	/* change the pointers to indexes */
	for (field = clientfields; field < ARREND(clientfields); field++)
	{
		WriteField1(f, field, (byte *)&temp);
	}

	/* write the block */
	sg_fwrite(&temp, sizeof(temp), f);

	/* now write any allocated data following the edict */
	for (field = clientfields; field < ARREND(clientfields); field++)
	{
		WriteField2(f, field, (byte *)client);
	}
}

/*
 * Read the client struct from a file
 */
static void
SanitizeClientStruct(gclient_t *cl)
{
	client_persistant_t *p;

	p = &cl->pers;
	p->userinfo[sizeof(p->userinfo) - 1] = 0;
	p->netname[sizeof(p->netname) - 1] = 0;

	p = &cl->resp.coop_respawn;
	p->userinfo[sizeof(p->userinfo) - 1] = 0;
	p->netname[sizeof(p->netname) - 1] = 0;

	ValidateSelectedItem(cl);
	cl->ammo_index = GetWeaponAmmoIndex(cl->pers.weapon);
	cl->flood_whenhead = 0;
}

static void
ReadClient(FILE *f, gclient_t *client, short save_ver)
{
	const field_t *field;

	sg_fread(client, sizeof(*client), f);

	for (field = clientfields; field < ARREND(clientfields); field++)
	{
		if (field->save_ver <= save_ver)
		{
			ReadField(f, field, (byte *)client);
		}
	}

	SanitizeClientStruct(client);

	if (save_ver < 3)
	{
		InitClientResp(client);
	}
}

/* ========================================================= */

/*
 * Writes the game struct into
 * a file. This is called whenever
 * the game goes to a new level or
 * the user saves the game. The saved
 * information consists of:
 * - cross level data
 * - client states
 * - help computer info
 */
void
WriteGame(const char *filename, qboolean autosave)
{
	savegameHeader_t sv;
	FILE *f;
	int i;

	if (!autosave)
	{
		SaveClientData();
	}

	f = Q_fopen(filename, "wb");

	if (!f)
	{
		gi.error("%s: Couldn't open %s", __func__, filename);
		return;
	}

	/* Savegame identification */
	memset(&sv, 0, sizeof(sv));

	Q_strlcpy(sv.ver, SAVEGAMEVER, sizeof(sv.ver) - 1);
	Q_strlcpy(sv.game, GAMEVERSION, sizeof(sv.game) - 1);
	Q_strlcpy(sv.os, YQ2OSTYPE, sizeof(sv.os) - 1);
	Q_strlcpy(sv.arch, YQ2ARCH, sizeof(sv.arch) - 1);

	sg_fwrite(&sv, sizeof(sv), f);

	game.autosaved = autosave;
	sg_fwrite(&game, sizeof(game), f);
	game.autosaved = false;

	for (i = 0; i < game.maxclients; i++)
	{
		WriteClient(f, &game.clients[i]);
	}

	fclose(f);
}

/*
 * Read the game structs from
 * a file. Called when ever a
 * savegames is loaded.
 */
static short
GetSaveVersion(const char *ver)
{
	int i;

	static const struct {
		const char* verstr;
		int vernum;
	} version_mappings[] = {
		{"YQ2-1", 1},
		{"YQ2-2", 2},
		{"YQ2-3", 3},
		{"YQ2-4", 4},
		{"YQ2-5", 5},
	};

	for (i=0; i < ARRLEN(version_mappings); ++i)
	{
		if (strcmp(version_mappings[i].verstr, ver) == 0)
		{
			return version_mappings[i].vernum;
		}
	}

	return 0;
}

static const char *
CheckSaveCompatibility(const savegameHeader_t *sv, short save_ver)
{
	if (save_ver == 0) // not found in mappings table
	{
		return "Savegame from an incompatible version.";
	}

	if (save_ver == 1)
	{
		if (strcmp(sv->game, GAMEVERSION) != 0)
		{
			return "Savegame from another game.so.";
		}

		if (strcmp(sv->os, OSTYPE_1) != 0)
		{
			return "Savegame from another os.";
		}

#ifdef _WIN32
		/* Windows was forced to i386 */
		if (strcmp(sv->arch, "i386") != 0)
		{
			return "Savegame from another architecture.";
		}
#else
		if (strcmp(sv->arch, ARCH_1) != 0)
		{
			return "Savegame from another architecture.";
		}
#endif
	}
	else // all newer savegame versions
	{
		if (strcmp(sv->game, GAMEVERSION) != 0)
		{
			return "Savegame from another game.so.";
		}

		if (strcmp(sv->os, YQ2OSTYPE) != 0)
		{
			return "Savegame from another os.";
		}

		if (strcmp(sv->arch, YQ2ARCH) != 0)
		{
#if defined(_WIN32) && (defined(__i386__) || defined(_M_IX86))
			// before savegame version "YQ2-4" (and after version 1),
			// the official Win32 binaries accidentally had the YQ2ARCH "AMD64"
			// instead of "i386" set due to a bug in the Makefile.
			// This quirk allows loading those savegames anyway
			if (save_ver >= 4 || strcmp(sv->arch, "AMD64") != 0)
#endif
			{
				return "Savegame from another architecture.";
			}
		}
	}

	return NULL;
}

static void
SanitizeGameStruct(void)
{
	/* ensure inline strings are null terminated */
	game.helpmessage1[sizeof(game.helpmessage1) - 1] = 0;
	game.helpmessage2[sizeof(game.helpmessage2) - 1] = 0;
	game.spawnpoint[sizeof(game.spawnpoint) - 1] = 0;

	/* no longer using this field for security and stability reasons */
	game.num_items = 0;
}

void
ReadGame(const char *filename)
{
	savegameHeader_t sv;
	FILE *f;
	int i;
	const char *errmsg;
	short save_ver;

	gi.FreeTags(TAG_GAME);

	f = Q_fopen(filename, "rb");

	if (!f)
	{
		gi.error("%s: Couldn't open %s", __func__, filename);
		return;
	}

	/* Sanity checks */
	sg_fread(&sv, sizeof(sv), f);
	sv.ver[sizeof(sv.ver) - 1] = 0;
	sv.game[sizeof(sv.game) - 1] = 0;
	sv.os[sizeof(sv.os) - 1] = 0;
	sv.arch[sizeof(sv.arch) - 1] = 0;

	save_ver = GetSaveVersion(sv.ver);
	errmsg = CheckSaveCompatibility(&sv, save_ver);
	if (errmsg)
	{
		fclose(f);
		gi.error("%s", errmsg);
		return;
	}

	sg_fread(&game, sizeof(game), f);
	SanitizeGameStruct();

	/* initialize entities and clients arrays */
	InitAllocations();

	for (i = 0; i < game.maxclients; i++)
	{
		ReadClient(f, &game.clients[i], save_ver);
	}

	fclose(f);
}

/* ========================================================== */

/*
 * Helper function to write the
 * edict into a file. Called by
 * WriteLevel.
 */
static void
WriteEdict(FILE *f, edict_t *ent)
{
	const field_t *field;
	edict_t temp;

	/* all of the ints, floats, and vectors stay as they are */
	temp = *ent;

	/* change the pointers to lengths or indexes */
	for (field = entfields; field < ARREND(entfields); field++)
	{
		WriteField1(f, field, (byte *)&temp);
	}

	/* write the block */
	sg_fwrite(&temp, sizeof(temp), f);

	/* now write any allocated data following the edict */
	for (field = entfields; field < ARREND(entfields); field++)
	{
		WriteField2(f, field, (byte *)ent);
	}
}

/*
 * Helper function to write the
 * level local data into a file.
 * Called by WriteLevel.
 */
static void
WriteLevelLocals(FILE *f)
{
	const field_t *field;
	level_locals_t temp;

	/* all of the ints, floats, and vectors stay as they are */
	temp = level;

	/* change the pointers to lengths or indexes */
	for (field = levelfields; field < ARREND(levelfields); field++)
	{
		WriteField1(f, field, (byte *)&temp);
	}

	/* write the block */
	sg_fwrite(&temp, sizeof(temp), f);

	/* now write any allocated data following the edict */
	for (field = levelfields; field < ARREND(levelfields); field++)
	{
		WriteField2(f, field, (byte *)&level);
	}
}

/*
 * Writes the current level
 * into a file.
 */
void
WriteLevel(const char *filename)
{
	int i;
	edict_t *ent;
	FILE *f;

	f = Q_fopen(filename, "wb");

	if (!f)
	{
		gi.error("%s: Couldn't open %s", __func__, filename);
		return;
	}

	/* write out edict size for checking */
	i = sizeof(edict_t);
	sg_fwrite(&i, sizeof(i), f);

	/* write out level_locals_t */
	WriteLevelLocals(f);

	/* write out all the entities */
	for (i = 0; i < globals.num_edicts; i++)
	{
		ent = &g_edicts[i];

		if (!ent->inuse)
		{
			continue;
		}

		sg_fwrite(&i, sizeof(i), f);
		WriteEdict(f, ent);
	}

	i = -1;
	sg_fwrite(&i, sizeof(i), f);

	fclose(f);
}

/* ========================================================== */

/*
 * A helper function to
 * read the edict back
 * into the memory. Called
 * by ReadLevel.
 */
static void
ReadEdict(FILE *f, edict_t *ent)
{
	const field_t *field;

	sg_fread(ent, sizeof(*ent), f);

	for (field = entfields; field < ARREND(entfields); field++)
	{
		ReadField(f, field, (byte *)ent);
	}
}

/*
 * A helper function to
 * read the level local
 * data from a file.
 * Called by ReadLevel.
 */
static void
SanitizeLevelStruct(void)
{
	/* ensure inline strings are null terminated */
	level.level_name[sizeof(level.level_name) - 1] = 0;
	level.mapname[sizeof(level.mapname) - 1] = 0;
	level.nextmap[sizeof(level.nextmap) - 1] = 0;

	level.pic_health = gi.imageindex("i_health");

	level.body_que %= BODY_QUEUE_SIZE;
}

static void
ReadLevelLocals(FILE *f)
{
	const field_t *field;

	sg_fread(&level, sizeof(level), f);

	for (field = levelfields; field < ARREND(levelfields); field++)
	{
		ReadField(f, field, (byte *)&level);
	}

	SanitizeLevelStruct();
}

/*
 * Reads a level back into the memory.
 * SpawnEntities were already called
 * in the same way when the level was
 * saved. All world links were cleared
 * before this function was called. When
 * this function is called, no clients
 * are connected to the server.
 */
void
ReadLevel(const char *filename)
{
	int entnum;
	FILE *f;
	int i;
	edict_t *ent;

	f = Q_fopen(filename, "rb");

	if (!f)
	{
		gi.error("%s: Couldn't open %s", __func__, filename);
		return;
	}

	/* free any dynamic memory allocated by
	   loading the level  base state */
	gi.FreeTags(TAG_LEVEL);

	/* wipe all the entities */
	memset(g_edicts, 0, game.maxentities * sizeof(g_edicts[0]));
	globals.num_edicts = maxclients->value + 1;

	/* check edict size */
	sg_fread(&i, sizeof(i), f);

	if (i != sizeof(edict_t))
	{
		fclose(f);
		gi.error("%s: mismatched edict size", __func__);
		return;
	}

	/* load the level locals */
	ReadLevelLocals(f);

	/* load all the entities */
	while (1)
	{
		sg_fread(&entnum, sizeof(entnum), f);

		if ((entnum < -1) || (entnum >= game.maxentities))
		{
			fclose(f);
			gi.error("%s: entnum out of bounds: %d", __func__, entnum);
		}

		if (entnum == -1)
		{
			break;
		}

		if (entnum >= globals.num_edicts)
		{
			globals.num_edicts = entnum + 1;
		}

		ent = &g_edicts[entnum];
		ReadEdict(f, ent);

		/* sanitize certain field values */
		ent->inuse = true;
		ent->s.number = ent - g_edicts;

		if (!ent->classname)
		{
			ent->classname = "noclass";
		}

		/* let the server rebuild world links for this ent */
		memset(&ent->area, 0, sizeof(ent->area));
		gi.linkentity(ent);
	}

	fclose(f);

	/* mark all clients as unconnected */
	for (i = 0; i < maxclients->value; i++)
	{
		ent = &g_edicts[i + 1];
		ent->client = game.clients + i;
		ent->client->pers.connected = false;
	}

	/* do any load time things at this point */
	for (i = 0; i < globals.num_edicts; i++)
	{
		ent = &g_edicts[i];

		if (!ent->inuse)
		{
			continue;
		}

		/* fire any cross-level triggers */
		if (ent->classname)
		{
			if (strcmp(ent->classname, "target_crosslevel_target") == 0)
			{
				ent->nextthink = level.time + ent->delay;
			}
		}
	}
}
