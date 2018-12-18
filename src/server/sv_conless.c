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
 * Connectionless server commands.
 *
 * =======================================================================
 */

#include "header/server.h"

extern cvar_t *hostname;
extern cvar_t *rcon_password;
char *SV_StatusString(void);

/*
 * Responds with all the info that qplug or qspy can see
 */
void
SVC_Status(void)
{
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "print\n%s", SV_StatusString());
}

void
SVC_Ack(void)
{
	Com_Printf("Ping acknowledge from %s\n", NET_AdrToString(net_from));
}

/*
 * Responds with short info for broadcast scans
 * The second parameter should be the current protocol version number.
 */
void
SVC_Info(void)
{
	char string[64];
	int i, count;
	int version;

	if (maxclients->value == 1)
	{
		return; /* ignore in single player */
	}

	version = (int)strtol(Cmd_Argv(1), (char **)NULL, 10);

	if (version != PROTOCOL_VERSION)
	{
		Com_sprintf(string, sizeof(string), "%s: wrong version\n",
				hostname->string, sizeof(string));
	}
	else
	{
		count = 0;

		for (i = 0; i < maxclients->value; i++)
		{
			if (svs.clients[i].state >= cs_connected)
			{
				count++;
			}
		}

		Com_sprintf(string, sizeof(string), "%16s %8s %2i/%2i\n",
				hostname->string, sv.name, count,
				(int)maxclients->value);
	}

	Netchan_OutOfBandPrint(NS_SERVER, net_from, "info\n%s", string);
}

/*
 * SVC_Ping
 */
void
SVC_Ping(void)
{
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "ack");
}

/*
 * Returns a challenge number that can be used
 * in a subsequent client_connect command.
 * We do this to prevent denial of service attacks that
 * flood the server with invalid connection IPs.  With a
 * challenge, they must give a valid IP address.
 */
void
SVC_GetChallenge(void)
{
	int i;
	int oldest;
	int oldestTime;

	oldest = 0;
	oldestTime = 0x7fffffff;

	/* see if we already have a challenge for this ip */
	for (i = 0; i < MAX_CHALLENGES; i++)
	{
		if (NET_CompareBaseAdr(net_from, svs.challenges[i].adr))
		{
			break;
		}

		if (svs.challenges[i].time < oldestTime)
		{
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if (i == MAX_CHALLENGES)
	{
		/* overwrite the oldest */
		svs.challenges[oldest].challenge = randk() & 0x7fff;
		svs.challenges[oldest].adr = net_from;
		svs.challenges[oldest].time = curtime;
		i = oldest;
	}

	/* send it back */
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "challenge %i",
			svs.challenges[i].challenge);
}

/*
 * A connection request that did not come from the master
 */
void
SVC_DirectConnect(void)
{
	char userinfo[MAX_INFO_STRING];
	netadr_t adr;
	int i;
	client_t *cl, *newcl;
	client_t temp;
	edict_t *ent;
	int edictnum;
	int version;
	int qport;
	int challenge;

	adr = net_from;

	Com_DPrintf("SVC_DirectConnect ()\n");

	version = (int)strtol(Cmd_Argv(1), (char **)NULL, 10);

	if (version != PROTOCOL_VERSION)
	{
		Netchan_OutOfBandPrint(NS_SERVER, adr,
				"print\nServer is version %s.\n", YQ2VERSION);
		Com_DPrintf("    rejected connect from version %i\n", version);
		return;
	}

	qport = (int)strtol(Cmd_Argv(2), (char **)NULL, 10);

	challenge = (int)strtol(Cmd_Argv(3), (char **)NULL, 10);

	Q_strlcpy(userinfo, Cmd_Argv(4), sizeof(userinfo));

	/* force the IP key/value pair so the game can filter based on ip */
	Info_SetValueForKey(userinfo, "ip", NET_AdrToString(net_from));

	/* attractloop servers are ONLY for local clients */
	if (sv.attractloop)
	{
		if (!NET_IsLocalAddress(adr))
		{
			Com_Printf("Remote connect in attract loop.  Ignored.\n");
			Netchan_OutOfBandPrint(NS_SERVER, adr,
					"print\nConnection refused.\n");
			return;
		}
	}

	/* see if the challenge is valid */
	if (!NET_IsLocalAddress(adr))
	{
		for (i = 0; i < MAX_CHALLENGES; i++)
		{
			if (NET_CompareBaseAdr(net_from, svs.challenges[i].adr))
			{
				if (challenge == svs.challenges[i].challenge)
				{
					break; /* good */
				}

				Netchan_OutOfBandPrint(NS_SERVER, adr,
						"print\nBad challenge.\n");
				return;
			}
		}

		if (i == MAX_CHALLENGES)
		{
			Netchan_OutOfBandPrint(NS_SERVER, adr,
					"print\nNo challenge for address.\n");
			return;
		}
	}

	newcl = &temp;
	memset(newcl, 0, sizeof(client_t));

	/* if there is already a slot for this ip, reuse it */
	for (i = 0, cl = svs.clients; i < maxclients->value; i++, cl++)
	{
		if (cl->state < cs_connected)
		{
			continue;
		}

		if (NET_CompareBaseAdr(adr, cl->netchan.remote_address) &&
			((cl->netchan.qport == qport) ||
			 (adr.port == cl->netchan.remote_address.port)))
		{
			if (!NET_IsLocalAddress(adr))
			{
				Com_DPrintf("%s:reconnect rejected : too soon\n",
						NET_AdrToString(adr));
				return;
			}

			Com_Printf("%s:reconnect\n", NET_AdrToString(adr));
			newcl = cl;
			goto gotnewcl;
		}
	}

	/* find a client slot */
	newcl = NULL;

	for (i = 0, cl = svs.clients; i < maxclients->value; i++, cl++)
	{
		if (cl->state == cs_free)
		{
			newcl = cl;
			break;
		}
	}

	if (!newcl)
	{
		Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nServer is full.\n");
		Com_DPrintf("Rejected a connection.\n");
		return;
	}

gotnewcl:

	/* build a new connection  accept the new client this
	   is the only place a client_t is ever initialized */
	*newcl = temp;
	sv_client = newcl;
	edictnum = (newcl - svs.clients) + 1;
	ent = EDICT_NUM(edictnum);
	newcl->edict = ent;
	newcl->challenge = challenge; /* save challenge for checksumming */

	/* get the game a chance to reject this connection or modify the userinfo */
	if (!(ge->ClientConnect(ent, userinfo)))
	{
		if (*Info_ValueForKey(userinfo, "rejmsg"))
		{
			Netchan_OutOfBandPrint(NS_SERVER, adr,
					"print\n%s\nConnection refused.\n",
					Info_ValueForKey(userinfo, "rejmsg"));
		}
		else
		{
			Netchan_OutOfBandPrint(NS_SERVER, adr,
					"print\nConnection refused.\n");
		}

		Com_DPrintf("Game rejected a connection.\n");
		return;
	}

	/* parse some info from the info strings */
	Q_strlcpy(newcl->userinfo, userinfo, sizeof(newcl->userinfo));
	SV_UserinfoChanged(newcl);

	/* send the connect packet to the client */
	if (sv_downloadserver->string[0])
	{
		Netchan_OutOfBandPrint(NS_SERVER, adr, "client_connect dlserver=%s", sv_downloadserver->string);
	}
	else
	{
		Netchan_OutOfBandPrint(NS_SERVER, adr, "client_connect");
	}

	Netchan_Setup(NS_SERVER, &newcl->netchan, adr, qport);

	newcl->state = cs_connected;

	SZ_Init(&newcl->datagram, newcl->datagram_buf, sizeof(newcl->datagram_buf));
	newcl->datagram.allowoverflow = true;
	newcl->lastmessage = svs.realtime;  /* don't timeout */
	newcl->lastconnect = svs.realtime;
}

int
Rcon_Validate(void)
{
	if (!strlen(rcon_password->string))
	{
		return 0;
	}

	if (strcmp(Cmd_Argv(1), rcon_password->string))
	{
		return 0;
	}

	return 1;
}

/*
 * A client issued an rcon command.
 * Shift down the remaining args
 * Redirect all printfs
 */
void
SVC_RemoteCommand(void)
{
	int i;
	char remaining[1024];

	i = Rcon_Validate();

	if (i == 0)
	{
		Com_Printf("Bad rcon from %s:\n%s\n", NET_AdrToString(
						net_from), net_message.data + 4);
	}
	else
	{
		Com_Printf("Rcon from %s:\n%s\n", NET_AdrToString(
						net_from), net_message.data + 4);
	}

	Com_BeginRedirect(RD_PACKET, sv_outputbuf,
			SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);

	if (!Rcon_Validate())
	{
		Com_Printf("Bad rcon_password.\n");
	}
	else
	{
		remaining[0] = 0;

		for (i = 2; i < Cmd_Argc(); i++)
		{
			strcat(remaining, Cmd_Argv(i));
			strcat(remaining, " ");
		}

		Cmd_ExecuteString(remaining);
	}

	Com_EndRedirect();
}

/*
 * A connectionless packet has four leading 0xff
 * characters to distinguish it from a game channel.
 * Clients that are in the game can still send
 * connectionless packets.
 */
void
SV_ConnectionlessPacket(void)
{
	char *s;
	char *c;

	MSG_BeginReading(&net_message);
	MSG_ReadLong(&net_message); /* skip the -1 marker */

	s = MSG_ReadStringLine(&net_message);

	Cmd_TokenizeString(s, false);

	c = Cmd_Argv(0);
	Com_DPrintf("Packet %s : %s\n", NET_AdrToString(net_from), c);

	if (!strcmp(c, "ping"))
	{
		SVC_Ping();
	}
	else if (!strcmp(c, "ack"))
	{
		SVC_Ack();
	}
	else if (!strcmp(c, "status"))
	{
		SVC_Status();
	}
	else if (!strcmp(c, "info"))
	{
		SVC_Info();
	}
	else if (!strcmp(c, "getchallenge"))
	{
		SVC_GetChallenge();
	}
	else if (!strcmp(c, "connect"))
	{
		SVC_DirectConnect();
	}
	else if (!strcmp(c, "rcon"))
	{
		SVC_RemoteCommand();
	}
	else
	{
		Com_Printf("bad connectionless packet from %s:\n%s\n",
				NET_AdrToString(net_from), s);
	}
}

