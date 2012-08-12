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
 * Low level network code, based upon the BSD socket api.
 *
 * =======================================================================
 */

#include "../../common/header/common.h"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <net/if.h>

netadr_t net_local_adr;

#define LOOPBACK 0x7f000001
#define MAX_LOOPBACK 4
#define QUAKE2MCAST "ff12::666"

typedef struct
{
	byte data[MAX_MSGLEN];
	int datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t msgs[MAX_LOOPBACK];
	int get, send;
} loopback_t;

loopback_t loopbacks[2];
int ip_sockets[2];
int ip6_sockets[2];
int ipx_sockets[2];
char *multicast_interface = NULL;

int NET_Socket(char *net_interface, int port, netsrc_t type, int family);
char *NET_ErrorString(void);

void
NetadrToSockadr(netadr_t *a, struct sockaddr_storage *s)
{
	struct sockaddr_in6 *s6;

	memset(s, 0, sizeof(*s));

	switch (a->type)
	{
		case NA_BROADCAST:
			((struct sockaddr_in *)s)->sin_family = AF_INET;
			((struct sockaddr_in *)s)->sin_port = a->port;
			((struct sockaddr_in *)s)->sin_addr.s_addr = (in_addr_t)INADDR_BROADCAST;
			break;

		case NA_IP:
			((struct sockaddr_in *)s)->sin_family = AF_INET;
			*(int *)&((struct sockaddr_in *)s)->sin_addr = *(int *)&a->ip;
			((struct sockaddr_in *)s)->sin_port = a->port;
			break;

		case NA_MULTICAST6:
			s6 = (struct sockaddr_in6 *)s;

			if (inet_pton(AF_INET6, QUAKE2MCAST, &s6->sin6_addr.s6_addr) != 1)
			{
				Com_Printf("NET_NetadrToSockadr: inet_pton: %s\n",
						strerror(errno));
				return;
			}

			s6->sin6_family = AF_INET6;
			s6->sin6_port = a->port;
#ifdef SIN6_LEN
			s6->sin6_len = sizeof(struct sockaddr_in6);
#endif

			/* scope_id is important for
			 * link-local destination.*/
			s6->sin6_scope_id = a->scope_id;

			break;

		case NA_IP6:

			if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)a->ip))
			{
#ifdef SIN6_LEN
				s->ss_len = sizeof(struct sockaddr_in);
#endif
				s->ss_family = AF_INET;
				memcpy(&((struct sockaddr_in *)s)->sin_addr,
						&((struct in6_addr *)a->ip)->s6_addr[12],
						sizeof(struct in_addr));
				((struct sockaddr_in *)s)->sin_port = a->port;
			}
			else
			{
				s6 = (struct sockaddr_in6 *)s;

				s6->sin6_family = AF_INET6;
				memcpy(&s6->sin6_addr, a->ip, sizeof(s6->sin6_addr));
				s6->sin6_port = a->port;
#ifdef SIN6_LEN
				s6->sin6_len = sizeof(struct sockaddr_in6);
#endif

				/* scope_id is important for
				 * link-local destination. */
				s6->sin6_scope_id = a->scope_id;
			}

			break;

		case NA_LOOPBACK:
		case NA_IPX:
		case NA_BROADCAST_IPX:
			break;
	}
}

void
SockadrToNetadr(struct sockaddr_storage *s, netadr_t *a)
{
	struct sockaddr_in6 *s6;

	if (s->ss_family == AF_INET)
	{
		*(int *) &a->ip = *(int *)&((struct sockaddr_in *)s)->sin_addr;
		a->port = ((struct sockaddr_in *)s)->sin_port;
		a->type = NA_IP;
	}
	else if (s->ss_family == AF_INET6)
	{
		s6 = (struct sockaddr_in6 *)s;

		if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)&s6->sin6_addr))
		{
			memcpy(a->ip, (struct in_addr *)&s6->sin6_addr.s6_addr[12],
					sizeof(struct in_addr));
			a->port = ((struct sockaddr_in *)s)->sin_port;
			a->type = NA_IP;
		}
		else
		{
			memcpy(a->ip, &s6->sin6_addr, sizeof(a->ip));
			a->port = s6->sin6_port;
			a->type = NA_IP6;
			a->scope_id = s6->sin6_scope_id;
		}
	}
	else
	{
		s = NULL;
	}
}

void
NET_Init()
{
}

qboolean
NET_CompareAdr(netadr_t a, netadr_t b)
{
	if (a.type != b.type)
	{
		return false;
	}

	if (a.type == NA_LOOPBACK)
	{
		return true;
	}

	if (a.type == NA_IP)
	{
		if ((a.ip[0] == b.ip[0]) && (a.ip[1] == b.ip[1]) &&
			(a.ip[2] == b.ip[2]) && (a.ip[3] == b.ip[3]) && (a.port == b.port))
		{
			return true;
		}
	}

	if (a.type == NA_IP6)
	{
		if ((memcmp(a.ip, b.ip, 16) == 0) && (a.port == b.port))
		{
			return true;
		}
	}

	return false;
}

/*
 * Compares without the port
 */
qboolean
NET_CompareBaseAdr(netadr_t a, netadr_t b)
{
	if (a.type != b.type)
	{
		return false;
	}

	if (a.type == NA_LOOPBACK)
	{
		return true;
	}

	if (a.type == NA_IP)
	{
		if ((a.ip[0] == b.ip[0]) && (a.ip[1] == b.ip[1]) &&
			(a.ip[2] == b.ip[2]) && (a.ip[3] == b.ip[3]))
		{
			return true;
		}

		return false;
	}

	if (a.type == NA_IP6)
	{
		if ((memcmp(a.ip, b.ip, 16) == 0))
		{
			return true;
		}

		return false;
	}

	if (a.type == NA_IPX)
	{
		if ((memcmp(a.ipx, b.ipx, 10) == 0))
		{
			return true;
		}

		return false;
	}

	return false;
}

char *
NET_BaseAdrToString(netadr_t a)
{
	static char s[64], tmp[64];
	struct sockaddr_storage ss;
	struct sockaddr_in6 *s6;

	switch (a.type)
	{
		case NA_IP:
		case NA_LOOPBACK:
			Com_sprintf(s, sizeof(s), "%i.%i.%i.%i", a.ip[0],
				a.ip[1], a.ip[2], a.ip[3]);
			break;

		case NA_BROADCAST:
			Com_sprintf(s, sizeof(s), "255.255.255.255");
			break;

		case NA_IP6:
		case NA_MULTICAST6:
			memset(&ss, 0, sizeof(ss));
			s6 = (struct sockaddr_in6 *)&ss;

			if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)a.ip))
			{
#ifdef SIN6_LEN
				ss.ss_len = sizeof(struct sockaddr_in);
#endif
				ss.ss_family = AF_INET;
				memcpy(&((struct sockaddr_in *)&ss)->sin_addr,
						&((struct in6_addr *)a.ip)->s6_addr[12],
						sizeof(struct in_addr));
			}
			else
			{
#ifdef SIN6_LEN
				s6->sin6_len = sizeof(struct sockaddr_in6);
#endif
				s6->sin6_scope_id = a.scope_id;
				s6->sin6_family = AF_INET6;
				memcpy(&s6->sin6_addr, a.ip, sizeof(struct in6_addr));
			}

#ifdef SIN6_LEN
			socklen_t const salen = ss.ss_len;
#else
			socklen_t const salen = sizeof(ss);
#endif

			if (getnameinfo((struct sockaddr *)&ss, salen, s, sizeof(s), NULL,
						0, NI_NUMERICHOST))
			{
				Com_sprintf(s, sizeof(s), "<invalid>");
			}

			else
			{
				if ((a.type == NA_MULTICAST6) ||
					IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6 *)&ss)->
								sin6_addr))
				{
					/* If the address is multicast (link) or a
					   link-local, need to carry the scope. The string
					   format of the IPv6 address is used by the
					   client to extablish the connect to the
					   server. */
					Com_sprintf(tmp, sizeof(tmp), "%s%%%d", s,
							s6->sin6_scope_id);
					memcpy(s, tmp, sizeof(s));
				}
			}

			break;

		default:
			Com_sprintf(s, sizeof(s), "invalid IP address family type");
			break;
	}

	return s;
}

char *
NET_AdrToString(netadr_t a)
{
	static char s[64];
	const char *base;

	base = NET_BaseAdrToString(a);
	Com_sprintf(s, sizeof(s), "[%s]:%d", base, ntohs(a.port));

	return s;
}

qboolean
NET_StringToSockaddr(char *s, struct sockaddr_storage *sadr)
{
	char copy[128];
	char *addrs, *space;
	char *ports = NULL;
	int err;
	struct addrinfo hints;
	struct addrinfo *resultp;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_family = PF_UNSPEC;

	strcpy(copy, s);
	addrs = space = copy;

	if (*addrs == '[')
	{
		addrs++;

		for ( ; *space && *space != ']'; space++)
		{
		}

		if (!*space)
		{
			Com_Printf("NET_StringToSockaddr: invalid IPv6 address %s\n", s);
			return 0;
		}

		*space++ = '\0';
	}

	for ( ; *space; space++)
	{
		if (*space == ':')
		{
			*space = '\0';
			ports = space + 1;
		}
	}

	if ((err = getaddrinfo(addrs, ports, &hints, &resultp)))
	{
		/* Error */
		Com_Printf("NET_StringToSockaddr: string %s:\n%s\n", s,
				gai_strerror(err));
		return 0;
	}

	switch (resultp->ai_family)
	{
		case AF_INET:
			/* convert to ipv4 addr */
			memset(sadr, 0, sizeof(struct sockaddr_storage));
			memcpy(sadr, resultp->ai_addr, resultp->ai_addrlen);
			break;

		case AF_INET6:
			/* convert to ipv6 addr */
			memset(sadr, 0, sizeof(struct sockaddr_storage));
			memcpy(sadr, resultp->ai_addr, resultp->ai_addrlen);
			break;

		default:
			Com_Printf("NET_StringToSockaddr: string %s:\nprotocol family %d not supported\n",
				s, resultp->ai_family);
			return 0;
	}

	return true;
}

qboolean
NET_StringToAdr(char *s, netadr_t *a)
{
	struct sockaddr_storage sadr;

	memset(a, 0, sizeof(*a));

	if (!strcmp(s, "localhost"))
	{
		a->type = NA_LOOPBACK;
		return true;
	}

	if (!NET_StringToSockaddr(s, &sadr))
	{
		return false;
	}

	SockadrToNetadr(&sadr, a);

	return true;
}

qboolean
NET_IsLocalAddress(netadr_t adr)
{
	return NET_CompareAdr(adr, net_local_adr);
}

qboolean
NET_GetLoopPacket(netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int i;
	loopback_t *loop;

	loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
	{
		loop->get = loop->send - MAX_LOOPBACK;
	}

	if (loop->get >= loop->send)
	{
		return false;
	}

	i = loop->get & (MAX_LOOPBACK - 1);
	loop->get++;

	memcpy(net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message->cursize = loop->msgs[i].datalen;
	*net_from = net_local_adr;
	return true;
}

void
NET_SendLoopPacket(netsrc_t sock, int length, void *data, netadr_t to)
{
	int i;
	loopback_t *loop;

	loop = &loopbacks[sock ^ 1];

	i = loop->send & (MAX_LOOPBACK - 1);
	loop->send++;

	memcpy(loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

qboolean
NET_GetPacket(netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int ret;
	struct sockaddr_storage from;
	socklen_t fromlen;
	int net_socket;
	int protocol;
	int err;

	if (NET_GetLoopPacket(sock, net_from, net_message))
	{
		return true;
	}

	for (protocol = 0; protocol < 3; protocol++)
	{
		if (protocol == 0)
		{
			net_socket = ip_sockets[sock];
		}
		else if (protocol == 1)
		{
			net_socket = ip6_sockets[sock];
		}
		else
		{
			net_socket = ipx_sockets[sock];
		}

		if (!net_socket)
		{
			continue;
		}

		fromlen = sizeof(from);
		ret = recvfrom(net_socket, net_message->data, net_message->maxsize,
				0, (struct sockaddr *)&from, &fromlen);

		SockadrToNetadr(&from, net_from);

		if (ret == -1)
		{
			err = errno;

			if ((err == EWOULDBLOCK) || (err == ECONNREFUSED))
			{
				continue;
			}

			Com_Printf("NET_GetPacket: %s from %s\n", NET_ErrorString(),
					NET_AdrToString(*net_from));
			continue;
		}

		if (ret == net_message->maxsize)
		{
			Com_Printf("Oversize packet from %s\n", NET_AdrToString(*net_from));
			continue;
		}

		net_message->cursize = ret;
		return true;
	}

	return false;
}

void
NET_SendPacket(netsrc_t sock, int length, void *data, netadr_t to)
{
	int ret;
	struct sockaddr_storage addr;
	int net_socket;
	int addr_size = sizeof(struct sockaddr_in);

	switch (to.type)
	{
		case NA_LOOPBACK:
			NET_SendLoopPacket(sock, length, data, to);
			return;
			break;

		case NA_BROADCAST:
		case NA_IP:
			net_socket = ip_sockets[sock];

			if (!net_socket)
			{
				return;
			}

			break;

		case NA_IP6:
		case NA_MULTICAST6:
			net_socket = ip6_sockets[sock];
			addr_size = sizeof(struct sockaddr_in6);

			if (!net_socket)
			{
				return;
			}

			break;

		case NA_IPX:
		case NA_BROADCAST_IPX:
			net_socket = ipx_sockets[sock];

			if (!net_socket)
			{
				return;
			}

			break;

		default:
			Com_Error(ERR_FATAL, "NET_SendPacket: bad address type");
			return;
			break;
	}

	NetadrToSockadr(&to, &addr);

	/* Re-check the address family. If to.type is NA_IP6 but
	   contains an IPv4 mapped address, NetadrToSockadr will
	   return an AF_INET struct.  If so, switch back to AF_INET
	   socket.*/
	if ((to.type == NA_IP6) && (addr.ss_family == AF_INET))
	{
		net_socket = ip_sockets[sock];
		addr_size = sizeof(struct sockaddr_in);

		if (!net_socket)
		{
			return;
		}
	}

	if (addr.ss_family == AF_INET6)
	{
		struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)&addr;

		/* If multicast socket, must specify scope.
		   So multicast_interface must be specified */
		if (IN6_IS_ADDR_MULTICAST(&s6->sin6_addr))
		{
			struct addrinfo hints;
			struct addrinfo *res;
			char tmp[128], mcast_addr[128], mcast_port[10];
			int error;

			if (multicast_interface != NULL)
			{
				/* Do a getnameinfo/getaddrinfo cycle
				   to calculate the scope_id of the
				   multicast address. getaddrinfo is
				   passed a multicast address of the
				   form ff0x::xxx%multicast_interface */
#ifdef SIN6_LEN
				error = getnameinfo((struct sockaddr *)s6, s6->sin6_len, tmp,
						sizeof(tmp), NULL, 0, NI_NUMERICHOST);
#else
				error = getnameinfo((struct sockaddr *)s6,
							sizeof(struct sockaddr_in6),
							tmp, sizeof(tmp), NULL, 0, NI_NUMERICHOST);
#endif

				if (error)
				{
					Com_Printf("NET_SendPacket: getnameinfo: %s\n",
							gai_strerror(error));
					return;
				}

				Com_sprintf(mcast_addr, sizeof(mcast_addr), "%s%%%s", tmp,
						multicast_interface);
				Com_sprintf(mcast_port, sizeof(mcast_port), "%d",
						ntohs(s6->sin6_port));
				memset(&hints, 0, sizeof(hints));
				hints.ai_family = AF_INET6;
				hints.ai_socktype = SOCK_DGRAM;
				hints.ai_flags = AI_NUMERICHOST;

				error = getaddrinfo(mcast_addr, mcast_port, &hints, &res);

				if (error)
				{
					Com_Printf("NET_SendPacket: getaddrinfo: %s\n",
							gai_strerror(error));
					return;
				}

				/* sockaddr_in6 should now have a valid scope_id. */
				memcpy(s6, res->ai_addr, res->ai_addrlen);
			}
			else
			{
				Com_Printf("NET_SendPacket: IPv6 multicast destination but +set multicast not specified: %s\n",
						inet_ntop(AF_INET6, &s6->sin6_addr, tmp, sizeof(tmp)));
				return;
			}
		}
	}

	ret = sendto(net_socket,
			data,
			length,
			0,
			(struct sockaddr *)&addr,
			addr_size);

	if (ret == -1)
	{
		Com_Printf("NET_SendPacket ERROR: %s to %s\n", NET_ErrorString(),
				NET_AdrToString(to));
	}
}

void
NET_OpenIP(void)
{
	cvar_t *port, *ip;

	port = Cvar_Get("port", va("%i", PORT_SERVER), CVAR_NOSET);
	ip = Cvar_Get("ip", "localhost", CVAR_NOSET);

	if (!ip6_sockets[NS_SERVER])
	{
		ip6_sockets[NS_SERVER] = NET_Socket(ip->string, port->value,
				NS_SERVER, AF_INET6);
	}

	if (!ip6_sockets[NS_CLIENT])
	{
		ip6_sockets[NS_CLIENT] = NET_Socket(ip->string, PORT_ANY,
				NS_CLIENT, AF_INET6);
	}

	if (!ip_sockets[NS_SERVER])
	{
		ip_sockets[NS_SERVER] = NET_Socket(ip->string, port->value,
				NS_SERVER, AF_INET);
	}

	if (!ip_sockets[NS_CLIENT])
	{
		ip_sockets[NS_CLIENT] = NET_Socket(ip->string, PORT_ANY,
				NS_CLIENT, AF_INET);
	}
}

/*
 * A single player game will only use the loopback code
 */
void
NET_Config(qboolean multiplayer)
{
	int i;

	if (!multiplayer)
	{
		/* shut down any existing sockets */
		for (i = 0; i < 2; i++)
		{
			if (ip_sockets[i])
			{
				close(ip_sockets[i]);
				ip_sockets[i] = 0;
			}

			if (ip6_sockets[i])
			{
				close(ip6_sockets[i]);
				ip6_sockets[i] = 0;
			}

			if (ipx_sockets[i])
			{
				close(ipx_sockets[i]);
				ipx_sockets[i] = 0;
			}
		}
	}
	else
	{
		/* open sockets */
		NET_OpenIP();
	}
}

/* =================================================================== */

int
NET_Socket(char *net_interface, int port, netsrc_t type, int family)
{
	char Buf[BUFSIZ], *Host, *Service;
	int newsocket, Error;
	struct sockaddr_storage ss;
	struct addrinfo hints, *res, *ai;
	qboolean _true = true;
	int i = 1;

	struct ipv6_mreq mreq;
	cvar_t *mcast;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE;

	if (!net_interface || !net_interface[0] ||
		!Q_stricmp(net_interface, "localhost"))
	{
		Host = (family == AF_INET6) ? "::" : "0.0.0.0";
	}
	else
	{
		Host = net_interface;
	}

	if (port == PORT_ANY)
	{
		Service = NULL;
	}
	else
	{
		sprintf(Buf, "%5d", port);
		Service = Buf;
	}

	if ((Error = getaddrinfo(Host, Service, &hints, &res)))
	{
		return 0;
	}

	for (ai = res; ai != NULL; ai = ai->ai_next)
	{
		if ((newsocket =
				 socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1)
		{
			Com_Printf("NET_Socket: socket: %s\n", strerror(errno));
			continue;
		}

		/* make it non-blocking */
		if (ioctl(newsocket, FIONBIO, (char *)&_true) == -1)
		{
			Com_Printf("NET_Socket: ioctl FIONBIO: %s\n", strerror(errno));
			continue;
		}

		if (family == AF_INET)
		{
			/* make it broadcast capable */
			if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&i,
						sizeof(i)) == -1)
			{
				Com_Printf("ERROR: NET_Socket: setsockopt SO_BROADCAST:%s\n",
						NET_ErrorString());
				return 0;
			}
		}

		/* make it reusable */
		if (setsockopt(newsocket, SOL_SOCKET, SO_REUSEADDR, (char *)&i,
					sizeof(i)) == -1)
		{
			Com_Printf("ERROR: NET_Socket: setsockopt SO_REUSEADDR:%s\n",
					NET_ErrorString());
			return 0;
		}

		if (bind(newsocket, ai->ai_addr, ai->ai_addrlen) < 0)
		{
			Com_Printf("NET_Socket: bind: %s\n", strerror(errno));
		}
		else
		{
			memcpy(&ss, ai->ai_addr, ai->ai_addrlen);
			break;
		}
	}

	if (res != NULL)
	{
		freeaddrinfo(res);
	}

	if (ai == NULL)
	{
		return 0;
	}

	switch (ss.ss_family)
	{
		case AF_INET:
			break;

		case AF_INET6:

			/* Multicast outgoing interface is specified for
			   client and server (+set multicast <ifname>) */
			mcast = Cvar_Get("multicast", "NULL", CVAR_NOSET);
			multicast_interface =
				(strcmp(mcast->string, "NULL") ? mcast->string : NULL);

			if (multicast_interface != NULL)
			{
				/* multicast_interface is a global variable.
				   Also used in NET_SendPacket() */
				if ((mreq.ipv6mr_interface =
						 if_nametoindex(multicast_interface)) == 0)
				{
					Com_Printf("NET_Socket: invalid interface: %s\n",
							multicast_interface);
				}

				if (setsockopt(newsocket, IPPROTO_IPV6, IPV6_MULTICAST_IF,
							&mreq.ipv6mr_interface,
							sizeof(mreq.ipv6mr_interface)) < 0)
				{
					Com_Printf("NET_Socket: IPV6_MULTICAST_IF: %s\n",
							strerror(errno));
				}

				/* Join multicast group ONLY if server */
				if (type == NS_SERVER)
				{
					if (inet_pton(AF_INET6, QUAKE2MCAST,
								&mreq.ipv6mr_multiaddr.s6_addr) != 1)
					{
						Com_Printf("NET_Socket: inet_pton: %s\n",
								strerror(errno));
					}

					if (setsockopt(newsocket, IPPROTO_IPV6, IPV6_JOIN_GROUP,
								&mreq, sizeof(mreq)) < 0)
					{
						Com_Printf("NET_Socket: IPV6_JOIN_GROUP: %s\n",
								strerror(errno));
					}
				}
			}

			break;
	}

	return newsocket;
}

void
NET_Shutdown(void)
{
	NET_Config(false); /* close sockets */
}

char *
NET_ErrorString(void)
{
	int code;

	code = errno;
	return strerror(code);
}

/*
 * sleeps msec or until net socket is ready
 */
void
NET_Sleep(int msec)
{
	struct timeval timeout;
	fd_set fdset;
	extern cvar_t *dedicated;
	extern qboolean stdin_active;

	if ((!ip_sockets[NS_SERVER] &&
		 !ip6_sockets[NS_SERVER]) || (dedicated && !dedicated->value))
	{
		return; /* we're not a server, just run full speed */
	}

	FD_ZERO(&fdset);

	if (stdin_active)
	{
		FD_SET(0, &fdset); /* stdin is processed too */
	}

	FD_SET(ip_sockets[NS_SERVER], &fdset); /* IPv4 network socket */
	FD_SET(ip6_sockets[NS_SERVER], &fdset); /* IPv6 network socket */
	timeout.tv_sec = msec / 1000;
	timeout.tv_usec = (msec % 1000) * 1000;
	select(MAX(ip_sockets[NS_SERVER],
					ip6_sockets[NS_SERVER]) + 1, &fdset, NULL, NULL, &timeout);
}

