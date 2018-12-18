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
 * This file implements the game media download from the server
 *
 * =======================================================================
 */

#include "header/client.h"

extern cvar_t *allow_download;
extern cvar_t *allow_download_players;
extern cvar_t *allow_download_models;
extern cvar_t *allow_download_sounds;
extern cvar_t *allow_download_maps;

extern int precache_check;
extern int precache_spawncount;
extern int precache_tex;
extern int precache_model_skin;

extern byte *precache_model;

static const char *env_suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

#define PLAYER_MULT 5

/* ENV_CNT is map load, ENV_CNT+1 is first env map */
#define ENV_CNT (CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
#define TEXTURE_CNT (ENV_CNT + 13)

void
CL_RequestNextDownload(void)
{
	unsigned int map_checksum; /* for detecting cheater maps */
	char fn[MAX_OSPATH];
	dmdl_t *pheader;

	if (cls.state != ca_connected)
	{
		return;
	}

	if (!allow_download->value && (precache_check < ENV_CNT))
	{
		precache_check = ENV_CNT;
	}

	if (precache_check == CS_MODELS)
	{
		precache_check = CS_MODELS + 2;

		if (allow_download_maps->value)
		{
			if (!CL_CheckOrDownloadFile(cl.configstrings[CS_MODELS + 1]))
			{
				return; /* started a download */
			}
		}
	}

	if ((precache_check >= CS_MODELS) &&
		(precache_check < CS_MODELS + MAX_MODELS))
	{
		if (allow_download_models->value)
		{
			while (precache_check < CS_MODELS + MAX_MODELS &&
				   cl.configstrings[precache_check][0])
			{
				if ((cl.configstrings[precache_check][0] == '*') ||
					(cl.configstrings[precache_check][0] == '#'))
				{
					precache_check++;
					continue;
				}

				if (precache_model_skin == 0)
				{
					if (!CL_CheckOrDownloadFile(cl.configstrings[precache_check]))
					{
						precache_model_skin = 1;
						return; /* started a download */
					}

					precache_model_skin = 1;
				}

#ifdef USE_CURL
				/* Wait for the models to download before checking * skins. */
				if (CL_PendingHTTPDownloads())
				{
					return;
				}
#endif

				/* checking for skins in the model */
				if (!precache_model)
				{
					FS_LoadFile(cl.configstrings[precache_check],
							(void **)&precache_model);

					if (!precache_model)
					{
						precache_model_skin = 0;
						precache_check++;
						continue; /* couldn't load it */
					}

					if (LittleLong(*(unsigned *)precache_model) !=
						IDALIASHEADER)
					{
						/* not an alias model */
						FS_FreeFile(precache_model);
						precache_model = 0;
						precache_model_skin = 0;
						precache_check++;
						continue;
					}

					pheader = (dmdl_t *)precache_model;

					if (LittleLong(pheader->version) != ALIAS_VERSION)
					{
						precache_check++;
						precache_model_skin = 0;
						continue; /* couldn't load it */
					}
				}

				pheader = (dmdl_t *)precache_model;

				while (precache_model_skin - 1 < LittleLong(pheader->num_skins))
				{
					if (!CL_CheckOrDownloadFile((char *)precache_model +
								LittleLong(pheader->ofs_skins) +
								(precache_model_skin - 1) * MAX_SKINNAME))
					{
						precache_model_skin++;
						return; /* started a download */
					}

					precache_model_skin++;
				}

				if (precache_model)
				{
					FS_FreeFile(precache_model);
					precache_model = 0;
				}

				precache_model_skin = 0;

				precache_check++;
			}
		}

		precache_check = CS_SOUNDS;
	}

	if ((precache_check >= CS_SOUNDS) &&
		(precache_check < CS_SOUNDS + MAX_SOUNDS))
	{
		if (allow_download_sounds->value)
		{
			if (precache_check == CS_SOUNDS)
			{
				precache_check++;
			}

			while (precache_check < CS_SOUNDS + MAX_SOUNDS &&
				   cl.configstrings[precache_check][0])
			{
				if (cl.configstrings[precache_check][0] == '*')
				{
					precache_check++;
					continue;
				}

				Com_sprintf(fn, sizeof(fn), "sound/%s",
						cl.configstrings[precache_check++]);

				if (!CL_CheckOrDownloadFile(fn))
				{
					return; /* started a download */
				}
			}
		}

		precache_check = CS_IMAGES;
	}

	if ((precache_check >= CS_IMAGES) &&
		(precache_check < CS_IMAGES + MAX_IMAGES))
	{
		if (precache_check == CS_IMAGES)
		{
			precache_check++;
		}

		while (precache_check < CS_IMAGES + MAX_IMAGES &&
			   cl.configstrings[precache_check][0])
		{
			Com_sprintf(fn, sizeof(fn), "pics/%s.pcx",
					cl.configstrings[precache_check++]);

			if (!CL_CheckOrDownloadFile(fn))
			{
				return; /* started a download */
			}
		}

		precache_check = CS_PLAYERSKINS;
	}

	/* skins are special, since a player has three 
	   things to download:  model, weapon model and
	   skin so precache_check is now *3 */
	if ((precache_check >= CS_PLAYERSKINS) &&
		(precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT))
	{
		if (allow_download_players->value)
		{
			while (precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
			{
				int i, n;
				char model[MAX_QPATH], skin[MAX_QPATH], *p;

				i = (precache_check - CS_PLAYERSKINS) / PLAYER_MULT;
				n = (precache_check - CS_PLAYERSKINS) % PLAYER_MULT;

				if (!cl.configstrings[CS_PLAYERSKINS + i][0])
				{
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				if ((p = strchr(cl.configstrings[CS_PLAYERSKINS + i], '\\')) != NULL)
				{
					p++;
				}
				else
				{
					p = cl.configstrings[CS_PLAYERSKINS + i];
				}

				strcpy(model, p);

				p = strchr(model, '/');

				if (!p)
				{
					p = strchr(model, '\\');
				}

				if (p)
				{
					*p++ = 0;
					strcpy(skin, p);
				}

				else
				{
					*skin = 0;
				}

				switch (n)
				{
					case 0: /* model */
						Com_sprintf(fn, sizeof(fn), "players/%s/tris.md2", model);

						if (!CL_CheckOrDownloadFile(fn))
						{
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 1;
							return;
						}

						n++;

					case 1: /* weapon model */
						Com_sprintf(fn, sizeof(fn), "players/%s/weapon.md2", model);

						if (!CL_CheckOrDownloadFile(fn))
						{
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 2;
							return;
						}

						n++;

					case 2: /* weapon skin */
						Com_sprintf(fn, sizeof(fn), "players/%s/weapon.pcx", model);

						if (!CL_CheckOrDownloadFile(fn))
						{
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 3;
							return;
						}

						n++;

					case 3: /* skin */
						Com_sprintf(fn, sizeof(fn), "players/%s/%s.pcx", model, skin);

						if (!CL_CheckOrDownloadFile(fn))
						{
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 4;
							return;
						}

						n++;

					case 4: /* skin_i */
						Com_sprintf(fn, sizeof(fn), "players/%s/%s_i.pcx", model, skin);

						if (!CL_CheckOrDownloadFile(fn))
						{
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 5;
							return; /* started a download */
						}

						/* move on to next model */
						precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				}
			}
		}

		/* precache phase completed */
		precache_check = ENV_CNT;
	}

#ifdef USE_CURL
	/* Wait for pending downloads. */
	if (CL_PendingHTTPDownloads())
	{
		return;
	}
#endif

	if (precache_check == ENV_CNT)
	{
		precache_check = ENV_CNT + 1;

		CM_LoadMap(cl.configstrings[CS_MODELS + 1], true, &map_checksum);

		if (map_checksum != (int)strtol(cl.configstrings[CS_MAPCHECKSUM], (char **)NULL, 10))
		{
			Com_Error(ERR_DROP, "Local map version differs from server: %i != '%s'\n",
					map_checksum, cl.configstrings[CS_MAPCHECKSUM]);
			return;
		}
	}

	if ((precache_check > ENV_CNT) && (precache_check < TEXTURE_CNT))
	{
		if (allow_download->value && allow_download_maps->value)
		{
			while (precache_check < TEXTURE_CNT)
			{
				int n = precache_check++ - ENV_CNT - 1;

				if (n & 1)
				{
					Com_sprintf(fn, sizeof(fn), "env/%s%s.pcx",
							cl.configstrings[CS_SKY], env_suf[n / 2]);
				}
				else
				{
					Com_sprintf(fn, sizeof(fn), "env/%s%s.tga",
							cl.configstrings[CS_SKY], env_suf[n / 2]);
				}

				if (!CL_CheckOrDownloadFile(fn))
				{
					return;
				}
			}
		}

		precache_check = TEXTURE_CNT;
	}

	if (precache_check == TEXTURE_CNT)
	{
		precache_check = TEXTURE_CNT + 1;
		precache_tex = 0;
	}

	/* confirm existance of textures, download any that don't exist */
	if (precache_check == TEXTURE_CNT + 1)
	{
		extern int numtexinfo;
		extern mapsurface_t map_surfaces[];

		if (allow_download->value && allow_download_maps->value)
		{
			while (precache_tex < numtexinfo)
			{
				char fn[MAX_OSPATH];

				sprintf(fn, "textures/%s.wal",
						map_surfaces[precache_tex++].rname);

				if (!CL_CheckOrDownloadFile(fn))
				{
					return; /* started a download */
				}
			}
		}

		precache_check = TEXTURE_CNT + 999;
	}

#ifdef USE_CURL
	/* Wait for pending downloads. */
	if (CL_PendingHTTPDownloads())
	{
		return;
	}
#endif

	CL_RegisterSounds();
	CL_PrepRefresh();

	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
	MSG_WriteString(&cls.netchan.message, va("begin %i\n", precache_spawncount));
	cls.forcePacket = true;
}

void
CL_DownloadFileName(char *dest, int destlen, char *fn)
{
	if (strncmp(fn, "players", 7) == 0)
	{
		Com_sprintf(dest, destlen, "%s/%s", BASEDIRNAME, fn);
	}

	else
	{
		Com_sprintf(dest, destlen, "%s/%s", FS_Gamedir(), fn);
	}
}

/*
 * Returns true if the file exists, otherwise it attempts
 * to start a download from the server.
 */
qboolean
CL_CheckOrDownloadFile(char *filename)
{
	FILE *fp;
	char name[MAX_OSPATH];
	char *ptr;

	/* fix backslashes - this is mostly für UNIX comaptiblity */
	while ((ptr = strchr(filename, '\\')))
	{
		*ptr = '/';
	}

	if (FS_LoadFile(filename, NULL) != -1)
	{
		/* it exists, no need to download */
		return true;
	}

	if (strstr(filename, "..") || strstr(filename, ":") || (*filename == '.') || (*filename == '/'))
	{
		Com_Printf("Refusing to download a path with ..: %s\n", filename);
		return true;
	}

#ifdef USE_CURL
	if (CL_QueueHTTPDownload(filename))
	{
		/* We return true so that the precache check
		   keeps feeding us more files. Since we have
		   multiple HTTP connections we want to
		   minimize latency and be constantly sending
		   requests, not one at a time. */
		return true;
	}
#endif

	strcpy(cls.downloadname, filename);

	/* download to a temp name, and only rename
	   to the real name when done, so if interrupted
	   a runt file wont be left */
	COM_StripExtension(cls.downloadname, cls.downloadtempname);
	strcat(cls.downloadtempname, ".tmp");

	/* check to see if we already have a tmp for this 
	   file, if so, try to resume and open the file if
	   not opened yet */
	CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

	fp = Q_fopen(name, "r+b");

	if (fp)
	{
		/* it exists */
		int len;
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		cls.download = fp;

		/* give the server an offset to start the download */
		Com_Printf("Resuming %s\n", cls.downloadname);
		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, va("download %s %i", cls.downloadname, len));
	}
	else
	{
		Com_Printf("Downloading %s\n", cls.downloadname);
		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, va("download %s", cls.downloadname));
	}

	cls.downloadnumber++;
	cls.forcePacket = true;

	return false;
}

/*
 * Request a download from the server
 */
void
CL_Download_f(void)
{
	char filename[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: download <filename>\n");
		return;
	}

	Com_sprintf(filename, sizeof(filename), "%s", Cmd_Argv(1));

	if (strstr(filename, ".."))
	{
		Com_Printf("Refusing to download a path with ..\n");
		return;
	}

	if (FS_LoadFile(filename, NULL) != -1)
	{
		/* it exists, no need to download */
		Com_Printf("File already exists.\n");
		return;
	}

	strcpy(cls.downloadname, filename);
	Com_Printf("Downloading %s\n", cls.downloadname);

	/* download to a temp name, and only rename
	   to the real name when done, so if interrupted
	   a runt file wont be left */
	COM_StripExtension(cls.downloadname, cls.downloadtempname);
	strcat(cls.downloadtempname, ".tmp");

	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
	MSG_WriteString(&cls.netchan.message, va("download %s", cls.downloadname));

	cls.downloadnumber++;
}

/*
 * A download message has been received from the server
 */
void
CL_ParseDownload(void)
{
	int size, percent;
	char name[MAX_OSPATH];
	int r;

	/* read the data */
	size = MSG_ReadShort(&net_message);
	percent = MSG_ReadByte(&net_message);

	if (size == -1)
	{
		Com_Printf("Server does not have this file.\n");

		if (cls.download)
		{
			/* if here, we tried to resume a
			 * file but the server said no */
			fclose(cls.download);
			cls.download = NULL;
		}

		CL_RequestNextDownload();
		return;
	}

	/* open the file if not opened yet */
	if (!cls.download)
	{
		CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

		FS_CreatePath(name);

		cls.download = Q_fopen(name, "wb");

		if (!cls.download)
		{
			net_message.readcount += size;
			Com_Printf("Failed to open %s\n", cls.downloadtempname);
			CL_RequestNextDownload();
			return;
		}
	}

	fwrite(net_message.data + net_message.readcount, 1, size, cls.download);
	net_message.readcount += size;

	if (percent != 100)
	{
		/* request next block */
		cls.downloadpercent = percent;

		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		SZ_Print(&cls.netchan.message, "nextdl");
		cls.forcePacket = true;
	}
	else
	{
		char oldn[MAX_OSPATH];
		char newn[MAX_OSPATH];

		fclose(cls.download);

		/* rename the temp file to it's final name */
		CL_DownloadFileName(oldn, sizeof(oldn), cls.downloadtempname);
		CL_DownloadFileName(newn, sizeof(newn), cls.downloadname);
		r = Sys_Rename(oldn, newn);

		if (r)
		{
			Com_Printf("failed to rename.\n");
		}

		cls.download = NULL;
		cls.downloadpercent = 0;

		/* get another file if needed */
		CL_RequestNextDownload();
	}
}

