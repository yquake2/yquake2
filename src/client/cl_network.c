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
 * This file implements generic network functions
 *
 * =======================================================================
 */

#include "header/client.h"
#include "../client/sound/header/local.h"

void CL_ParseStatusMessage(void);

extern cvar_t *msg;
extern cvar_t *rcon_client_password;
extern cvar_t *rcon_address;
extern cvar_t *cl_timeout;

/*
 * adds the current command line as a clc_stringcmd to the client
 * message. things like godmode, noclip, etc, are commands directed to
 * the server, so when they are typed in at the console, they will need
 * to be forwarded.
 */
void
Cmd_ForwardToServer(void)
{
	char *cmd;

	cmd = Cmd_Argv(0);

	if ((cls.state <= ca_connected) || (*cmd == '-') || (*cmd == '+'))
	{
		Com_Printf("Unknown command \"%s\"\n", cmd);
		return;
	}

	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);

	SZ_Print(&cls.netchan.message, cmd);

	if (Cmd_Argc() > 1)
	{
		SZ_Print(&cls.netchan.message, " ");
		SZ_Print(&cls.netchan.message, Cmd_Args());
	}
}

void
CL_ForwardToServer_f(void)
{
	if ((cls.state != ca_connected) && (cls.state != ca_active))
	{
		Com_Printf("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	/* don't forward the first argument */
	if (Cmd_Argc() > 1)
	{
		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		SZ_Print(&cls.netchan.message, Cmd_Args());
	}
}

/*
 * Called after an ERR_DROP was thrown
 */
void
CL_Drop(void)
{
	if (cls.state == ca_uninitialized)
	{
		return;
	}

	if (cls.state == ca_disconnected)
	{
		return;
	}

	CL_Disconnect();

	/* drop loading plaque unless this is the initial game start */
	if (cls.disable_servercount != -1)
	{
		SCR_EndLoadingPlaque();  /* get rid of loading plaque */
	}
}

/*
 * We have gotten a challenge from the server, so try and
 * connect.
 */
void
CL_SendConnectPacket(void)
{
	netadr_t adr;
	int port;

	memset(&adr, 0, sizeof(adr));

	if (!NET_StringToAdr(cls.servername, &adr))
	{
		Com_Printf("Bad server address\n");
		cls.connect_time = 0;
		return;
	}

	if (adr.port == 0)
	{
		adr.port = BigShort(PORT_SERVER);
	}

	port = Cvar_VariableValue("qport");

	userinfo_modified = false;

	Netchan_OutOfBandPrint(NS_CLIENT, adr, "connect %i %i %i \"%s\"\n",
			PROTOCOL_VERSION, port, cls.challenge, Cvar_Userinfo());
}

/*
 * Resend a connect message if the last one has timed out
 */
void
CL_CheckForResend(void)
{
	netadr_t adr;

	/* if the local server is running and we aren't just connect */
	if ((cls.state == ca_disconnected) && Com_ServerState())
	{
		cls.state = ca_connecting;
		Q_strlcpy(cls.servername, "localhost", sizeof(cls.servername));
		/* we don't need a challenge on the localhost */
		CL_SendConnectPacket();
		return;
	}

	/* resend if we haven't gotten a reply yet */
	if (cls.state != ca_connecting)
	{
		return;
	}

	if (cls.realtime - cls.connect_time < 3000)
	{
		return;
	}

	if (!NET_StringToAdr(cls.servername, &adr))
	{
		Com_Printf("Bad server address\n");
		cls.state = ca_disconnected;
		return;
	}

	if (adr.port == 0)
	{
		adr.port = BigShort(PORT_SERVER);
	}

	cls.connect_time = cls.realtime;

	Com_Printf("Connecting to %s...\n", cls.servername);

	Netchan_OutOfBandPrint(NS_CLIENT, adr, "getchallenge\n");
}

void
CL_Connect_f(void)
{
	char server[256];

	if (Cmd_Argc() != 2)
	{
		Com_Printf("usage: connect <server>\n");
		return;
	}

	Q_strlcpy(server, Cmd_Argv(1), sizeof(server));

	if (Com_ServerState())
	{
		/* if running a local server, kill it and reissue
		   note: this is connect with the save game system */
		SV_Shutdown("Server quit\n", false);
	}

	NET_Config(true); /* allow remote */
	CL_Disconnect();

	cls.state = ca_connecting;
	Q_strlcpy(cls.servername, server, sizeof(cls.servername));
	cls.connect_time = -99999; /* HACK: CL_CheckForResend() will fire immediately */
}

/*
 * Send the rest of the command line over as
 * an unconnected command.
 */
void
CL_Rcon_f(void)
{
	char message[1024];
	int i;
	netadr_t to;

	if (!rcon_client_password->string)
	{
		Com_Printf("You must set 'rcon_password' before\n"
				   "issuing an rcon command.\n");
		return;
	}

	memset(&to, 0, sizeof(to));

	message[0] = (char)255;
	message[1] = (char)255;
	message[2] = (char)255;
	message[3] = (char)255;
	message[4] = 0;

	NET_Config(true);  /* allow remote */

	strcat(message, "rcon ");

	strcat(message, rcon_client_password->string);
	strcat(message, " ");

	for (i = 1; i < Cmd_Argc(); i++)
	{
		strcat(message, Cmd_Argv(i));
		strcat(message, " ");
	}

	if (cls.state >= ca_connected)
	{
		to = cls.netchan.remote_address;
	}
	else
	{
		if (!strlen(rcon_address->string))
		{
			Com_Printf("You must either be connected,\n"
					   "or set the 'rcon_address' cvar\n"
					   "to issue rcon commands\n");

			return;
		}

		NET_StringToAdr(rcon_address->string, &to);

		if (to.port == 0)
		{
			to.port = BigShort(PORT_SERVER);
		}
	}

	NET_SendPacket(NS_CLIENT, strlen(message) + 1, message, to);
}

void CL_WriteConfiguration(void);

/*
 * Goes from a connected state to full screen
 * console state Sends a disconnect message to
 * the server This is also called on Com_Error, so
 * it shouldn't cause any errors
 */
void
CL_Disconnect(void)
{
	byte final[32];

	if (cls.state == ca_disconnected)
	{
		return;
	}

	if (cl_timedemo && cl_timedemo->value)
	{
		int time;

		time = Sys_Milliseconds() - cl.timedemo_start;

		if (time > 0)
		{
			Com_Printf("%i frames, %3.1f seconds: %3.1f fps\n",
					cl.timedemo_frames, time / 1000.0,
					cl.timedemo_frames * 1000.0 / time);
		}
	}

	VectorClear(cl.refdef.blend);

	R_SetPalette(NULL);

	M_ForceMenuOff();

	cls.connect_time = 0;

	SCR_StopCinematic();

	OGG_Stop();

	if (cls.demorecording)
	{
		CL_Stop_f();
	}

	/* send a disconnect message to the server */
	final[0] = clc_stringcmd;

	strcpy((char *)final + 1, "disconnect");

	Netchan_Transmit(&cls.netchan, strlen((const char *)final), final);
	Netchan_Transmit(&cls.netchan, strlen((const char *)final), final);
	Netchan_Transmit(&cls.netchan, strlen((const char *)final), final);

	CL_ClearState();

	/* stop file download */
	if (cls.download)
	{
		fclose(cls.download);
		cls.download = NULL;
	}

#ifdef USE_CURL
	CL_CancelHTTPDownloads(true);
	cls.downloadReferer[0] = 0;
	cls.downloadname[0] = 0;
	cls.downloadposition = 0;
#endif

	cls.state = ca_disconnected;

	snd_is_underwater = false;

	// save config for old game/mod
	CL_WriteConfiguration();

	// we disconnected, so revert to default game/mod (might have been different mod on MP server)
	Cvar_Set("game", userGivenGame);
}

void
CL_Disconnect_f(void)
{
	Com_Error(ERR_DROP, "Disconnected from server");
}

/*
 * packet <destination> <contents>
 *
 * Contents allows \n escape character
 */
void
CL_Packet_f(void)
{
	char send[2048];
	int i, l;
	char *in, *out;
	netadr_t adr;

	if (Cmd_Argc() != 3)
	{
		Com_Printf("packet <destination> <contents>\n");
		return;
	}

	NET_Config(true);  /* allow remote */

	if (!NET_StringToAdr(Cmd_Argv(1), &adr))
	{
		Com_Printf("Bad address\n");
		return;
	}

	if (!adr.port)
	{
		adr.port = BigShort(PORT_SERVER);
	}

	in = Cmd_Argv(2);

	out = send + 4;

	send[0] = send[1] = send[2] = send[3] = (char)0xff;

	l = strlen(in);

	for (i = 0; i < l; i++)
	{
		if ((in[i] == '\\') && (in[i + 1] == 'n'))
		{
			*out++ = '\n';
			i++;
		}

		else
		{
			*out++ = in[i];
		}
	}

	*out = 0;

	NET_SendPacket(NS_CLIENT, out - send, send, adr);
}

/*
 * Just sent as a hint to the client that they should
 * drop to full console
 */
void
CL_Changing_f(void)
{
	/* if we are downloading, we don't change!
	   This so we don't suddenly stop downloading a map */
	if (cls.download)
	{
		return;
	}

	SCR_BeginLoadingPlaque();
	cls.state = ca_connected; /* not active anymore, but not disconnected */
	Com_Printf("\nChanging map...\n");

#ifdef USE_CURL
	if (cls.downloadServerRetry[0] != 0)
	{
		CL_SetHTTPServer(cls.downloadServerRetry);
	}
#endif
}

/*
 * The server is changing levels
 */
void
CL_Reconnect_f(void)
{
	/* if we are downloading, we don't change!
	   This so we don't suddenly stop downloading a map */
	if (cls.download)
	{
		return;
	}

	S_StopAllSounds();

	if (cls.state == ca_connected)
	{
		Com_Printf("reconnecting...\n");
		cls.state = ca_connected;
		MSG_WriteChar(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, "new");
		return;
	}

	if (*cls.servername)
	{
		if (cls.state >= ca_connected)
		{
			CL_Disconnect();
			cls.connect_time = cls.realtime - 1500;
		}

		else
		{
			cls.connect_time = -99999; /* Hack: fire immediately */
		}

		cls.state = ca_connecting;

		Com_Printf("reconnecting...\n");
	}
}

void
CL_PingServers_f(void)
{
	int i;
	netadr_t adr;
	char name[32];
	const char *adrstring;
	cvar_t *noudp;
	cvar_t *noipx;

	NET_Config(true);  /* allow remote but do we even need lokal pings? */

	/* send a broadcast packet */
	Com_Printf("pinging broadcast...\n");

	noudp = Cvar_Get("noudp", "0", CVAR_NOSET);

	if (!noudp->value)
	{
		adr.type = NA_BROADCAST;
		adr.port = BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint(NS_CLIENT, adr, va("info %i", PROTOCOL_VERSION));

		Com_Printf("pinging multicast...\n");
		adr.type = NA_MULTICAST6;
		adr.port = BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint(NS_CLIENT, adr, va("info %i", PROTOCOL_VERSION));
	}

	noipx = Cvar_Get("noipx", "0", CVAR_NOSET);

	if (!noipx->value)
	{
		adr.type = NA_BROADCAST_IPX;
		adr.port = BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint(NS_CLIENT, adr, va("info %i", PROTOCOL_VERSION));
	}

	/* send a packet to each address book entry */
	for (i = 0; i < 16; i++)
	{
		Com_sprintf(name, sizeof(name), "adr%i", i);
		adrstring = Cvar_VariableString(name);

		if (!adrstring || !adrstring[0])
		{
			continue;
		}

		Com_Printf("pinging %s...\n", adrstring);

		if (!NET_StringToAdr(adrstring, &adr))
		{
			Com_Printf("Bad address: %s\n", adrstring);
			continue;
		}

		if (!adr.port)
		{
			adr.port = BigShort(PORT_SERVER);
		}

		Netchan_OutOfBandPrint(NS_CLIENT, adr, va("info %i", PROTOCOL_VERSION));
	}
}

/*
 * Responses to broadcasts, etc
 */
void
CL_ConnectionlessPacket(void)
{
	char *s;
	char *c;

	MSG_BeginReading(&net_message);
	MSG_ReadLong(&net_message); /* skip the -1 */

	s = MSG_ReadStringLine(&net_message);

	Cmd_TokenizeString(s, false);

	c = Cmd_Argv(0);

	Com_Printf("%s: %s\n", NET_AdrToString(net_from), c);

	/* server connection */
	if (!strcmp(c, "client_connect"))
	{
		if (cls.state == ca_connected)
		{
			Com_Printf("Dup connect received.  Ignored.\n");
			return;
		}

		Netchan_Setup(NS_CLIENT, &cls.netchan, net_from, cls.quakePort);
		char *buff = NET_AdrToString(cls.netchan.remote_address);

		for(int i = 1; i < Cmd_Argc(); i++)
		{
			char *p = Cmd_Argv(i);

			if(!strncmp(p, "dlserver=", 9))
			{
#ifdef USE_CURL
				p += 9;
				Com_sprintf(cls.downloadReferer, sizeof(cls.downloadReferer), "quake2://%s", buff);
				CL_SetHTTPServer (p);

				if (cls.downloadServer[0])
				{
					Com_Printf("HTTP downloading enabled, URL: %s\n", cls.downloadServer);
				}
#else
				Com_Printf("HTTP downloading supported by server but not the client.\n");
#endif
			}
		}

		MSG_WriteChar(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, "new");
		cls.state = ca_connected;
		return;
	}

	/* server responding to a status broadcast */
	if (!strcmp(c, "info"))
	{
		CL_ParseStatusMessage();
		return;
	}

	/* remote command from gui front end */
	if (!strcmp(c, "cmd"))
	{
		if (!NET_IsLocalAddress(net_from))
		{
			Com_Printf("Command packet from remote host.  Ignored.\n");
			return;
		}

		s = MSG_ReadString(&net_message);
		Cbuf_AddText(s);
		Cbuf_AddText("\n");
		return;
	}

	/* print command from somewhere */
	if (!strcmp(c, "print"))
	{
		s = MSG_ReadString(&net_message);
		Com_Printf("%s", s);
		return;
	}

	/* ping from somewhere */
	if (!strcmp(c, "ping"))
	{
		Netchan_OutOfBandPrint(NS_CLIENT, net_from, "ack");
		return;
	}

	/* challenge from the server we are connecting to */
	if (!strcmp(c, "challenge"))
	{
		cls.challenge = (int)strtol(Cmd_Argv(1), (char **)NULL, 10);
		CL_SendConnectPacket();
		return;
	}

	/* echo request from server */
	if (!strcmp(c, "echo"))
	{
		Netchan_OutOfBandPrint(NS_CLIENT, net_from, "%s", Cmd_Argv(1));
		return;
	}

	Com_Printf("Unknown command.\n");
}

void
CL_ReadPackets(void)
{
	while (NET_GetPacket(NS_CLIENT, &net_from, &net_message))
	{
		/* remote command packet */
		if (*(int *)net_message.data == -1)
		{
			CL_ConnectionlessPacket();
			continue;
		}

		if ((cls.state == ca_disconnected) || (cls.state == ca_connecting))
		{
			continue; /* dump it if not connected */
		}

		if (net_message.cursize < 8)
		{
			Com_Printf("%s: Runt packet\n", NET_AdrToString(net_from));
			continue;
		}

		/* packet from server */
		if (!NET_CompareAdr(net_from, cls.netchan.remote_address))
		{
			Com_DPrintf("%s:sequenced packet without connection\n",
					NET_AdrToString(net_from));
			continue;
		}

		if (!Netchan_Process(&cls.netchan, &net_message))
		{
			continue; /* wasn't accepted for some reason */
		}

		CL_ParseServerMessage();
	}

	/* check timeout */
	if ((cls.state >= ca_connected) &&
		(cls.realtime - cls.netchan.last_received > cl_timeout->value * 1000))
	{
		if (++cl.timeoutcount > 5)
		{
			Com_Printf("\nServer connection timed out.\n");
			CL_Disconnect();
			return;
		}
	}

	else
	{
		cl.timeoutcount = 0;
	}
}

