/*
 * Copyright © 2008 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

/*
 * X authentication and transport code adopted from libxcb
 * 
 * Copyright (C) 2001-2004 Bart Massey and Jamey Sharp.
 */

#include <dbus/dbus.h>
#include <stdio.h>

#include <X11/Xauth.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#ifdef DNETCONN
#include <netdnet/dnetdb.h>
#include <netdnet/dn.h>
#endif
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <xcb/xcb.h>

#ifdef HASXDMAUTH
#include <X11/Xdmcp.h>
#endif

#include "dmxdbus.h"

enum auth_protos {
#ifdef HASXDMAUTH
    AUTH_XA1,
#endif
    AUTH_MC1,
    N_AUTH_PROTOS
};

static char *authnames[N_AUTH_PROTOS] = {
#ifdef HASXDMAUTH
    "XDM-AUTHORIZATION-1",
#endif
    "MIT-MAGIC-COOKIE-1",
};

static size_t memdup(char **dst, void *src, size_t len)
{
    if(len)
	*dst = malloc(len);
    else
	*dst = 0;
    if(!*dst)
	return 0;
    memcpy(*dst, src, len);
    return len;
}

static int authname_match(enum auth_protos kind, char *name, int namelen)
{
    if(strlen(authnames[kind]) != namelen)
	return 0;
    if(memcmp(authnames[kind], name, namelen))
	return 0;
    return 1;
}

#define SIN6_ADDR(s) (&((struct sockaddr_in6 *)s)->sin6_addr)

static Xauth *get_authptr(struct sockaddr *sockname, unsigned int socknamelen,
                          int display)
{
    char *addr = 0;
    int addrlen = 0;
    unsigned short family;
    char hostnamebuf[256];   /* big enough for max hostname */
    char dispbuf[40];   /* big enough to hold more than 2^64 base 10 */
    int authnamelens[N_AUTH_PROTOS];
    int i;

    family = FamilyLocal; /* 256 */
    switch(sockname->sa_family)
    {
#ifdef AF_INET6
    case AF_INET6:
        addr = (char *) SIN6_ADDR(sockname);
        addrlen = sizeof(*SIN6_ADDR(sockname));
        if(!IN6_IS_ADDR_V4MAPPED(SIN6_ADDR(sockname)))
        {
            if(!IN6_IS_ADDR_LOOPBACK(SIN6_ADDR(sockname)))
                family = XCB_FAMILY_INTERNET_6;
            break;
        }
        addr += 12;
        /* if v4-mapped, fall through. */
#endif
    case AF_INET:
        if(!addr)
            addr = (char *) &((struct sockaddr_in *)sockname)->sin_addr;
        addrlen = sizeof(((struct sockaddr_in *)sockname)->sin_addr);
        if(*(in_addr_t *) addr != htonl(INADDR_LOOPBACK))
            family = XCB_FAMILY_INTERNET;
        break;
    case AF_UNIX:
        break;
    default:
        return 0;   /* cannot authenticate this family */
    }

    snprintf(dispbuf, sizeof(dispbuf), "%d", display);

    if (family == FamilyLocal) {
        if (gethostname(hostnamebuf, sizeof(hostnamebuf)) == -1)
            return 0;   /* do not know own hostname */
        addr = hostnamebuf;
        addrlen = strlen(addr);
    }

    for (i = 0; i < N_AUTH_PROTOS; i++)
	authnamelens[i] = strlen(authnames[i]);
    return XauGetBestAuthByAddr (family,
                                 (unsigned short) addrlen, addr,
                                 (unsigned short) strlen(dispbuf), dispbuf,
                                 N_AUTH_PROTOS, authnames, authnamelens);
}

#ifdef HASXDMAUTH
static int next_nonce(void)
{
    static int nonce = 0;
    static pthread_mutex_t nonce_mutex = PTHREAD_MUTEX_INITIALIZER;
    int ret;
    pthread_mutex_lock(&nonce_mutex);
    ret = nonce++;
    pthread_mutex_unlock(&nonce_mutex);
    return ret;
}

static void do_append(char *buf, int *idxp, void *val, size_t valsize) {
    memcpy(buf + *idxp, val, valsize);
    *idxp += valsize;
}
#endif
     
static int compute_auth(xcb_auth_info_t *info, Xauth *authptr, struct sockaddr *sockname)
{
    if (authname_match(AUTH_MC1, authptr->name, authptr->name_length)) {
        info->datalen = memdup(&info->data, authptr->data, authptr->data_length);
        if(!info->datalen)
            return 0;
        return 1;
    }
#ifdef HASXDMAUTH
#define APPEND(buf,idx,val) do_append((buf),&(idx),&(val),sizeof(val))
    if (authname_match(AUTH_XA1, authptr->name, authptr->name_length)) {
	int j;

	info->data = malloc(192 / 8);
	if(!info->data)
	    return 0;

	for (j = 0; j < 8; j++)
	    info->data[j] = authptr->data[j];
	switch(sockname->sa_family) {
        case AF_INET:
            /*block*/ {
	    struct sockaddr_in *si = (struct sockaddr_in *) sockname;
	    APPEND(info->data, j, si->sin_addr.s_addr);
	    APPEND(info->data, j, si->sin_port);
	}
	break;
#ifdef AF_INET6
        case AF_INET6:
            /*block*/ {
            struct sockaddr_in6 *si6 = (struct sockaddr_in6 *) sockname;
            if(IN6_IS_ADDR_V4MAPPED(SIN6_ADDR(sockname)))
            {
                APPEND(info->data, j, si6->sin6_addr.s6_addr[12]);
                APPEND(info->data, j, si6->sin6_port);
            }
            else
            {
                /* XDM-AUTHORIZATION-1 does not handle IPv6 correctly.  Do the
                   same thing Xlib does: use all zeroes for the 4-byte address
                   and 2-byte port number. */
                uint32_t fakeaddr = 0;
                uint16_t fakeport = 0;
                APPEND(info->data, j, fakeaddr);
                APPEND(info->data, j, fakeport);
            }
        }
        break;
#endif
        case AF_UNIX:
            /*block*/ {
	    uint32_t fakeaddr = htonl(0xffffffff - next_nonce());
	    uint16_t fakeport = htons(getpid());
	    APPEND(info->data, j, fakeaddr);
	    APPEND(info->data, j, fakeport);
	}
	break;
        default:
            free(info->data);
            return 0;   /* do not know how to build this */
	}
	{
	    uint32_t now = htonl(time(0));
	    APPEND(info->data, j, now);
	}
	assert(j <= 192 / 8);
	while (j < 192 / 8)
	    info->data[j++] = 0;
	info->datalen = j;
	XdmcpWrap ((unsigned char *) info->data, (unsigned char *) authptr->data + 8, (unsigned char *) info->data, info->datalen);
	return 1;
    }
#undef APPEND
#endif

    return 0;   /* Unknown authorization type */
}

int _get_auth_info (int fd, xcb_auth_info_t *info, int display)
{
    /* code adapted from Xlib/ConnDis.c, xtrans/Xtranssocket.c,
       xtrans/Xtransutils.c */
    char sockbuf[sizeof(struct sockaddr) + MAXPATHLEN];
    unsigned int socknamelen = sizeof(sockbuf);   /* need extra space */
    struct sockaddr *sockname = (struct sockaddr *) &sockbuf;
    Xauth *authptr = 0;
    int ret = 1;

    if (getpeername(fd, sockname, &socknamelen) == -1)
        return 0;  /* can only authenticate sockets */

    authptr = get_authptr(sockname, socknamelen, display);
    if (authptr == 0)
        return 0;   /* cannot find good auth data */

    info->namelen = memdup(&info->name, authptr->name, authptr->name_length);
    if(info->namelen)
	ret = compute_auth(info, authptr, sockname);
    if(!ret)
    {
	free(info->name);
	info->name = 0;
	info->namelen = 0;
    }
    XauDisposeAuth(authptr);
    return ret;
}

static int _xcb_open_tcp(char *host, char *protocol, const unsigned short port);
static int _xcb_open_unix(char *protocol, const char *file);
#ifdef DNETCONN
static int _xcb_open_decnet(const char *host, char *protocol, const unsigned short port);
#endif

static int _xcb_open(char *host, char *protocol, const int display)
{
    int fd;
    static const char base[] = "/tmp/.X11-unix/X";
    char file[sizeof(base) + 20];

    if(*host)
    {
#ifdef DNETCONN
        /* DECnet displays have two colons, so _xcb_parse_display will have
           left one at the end.  However, an IPv6 address can end with *two*
           colons, so only treat this as a DECnet display if host ends with
           exactly one colon. */
        char *colon = strchr(host, ':');
        if(colon && *(colon+1) == '\0')
        {
            *colon = '\0';
            return _xcb_open_decnet(host, protocol, display);
        }
        else
#endif
            if (protocol
                || strcmp("unix",host)) { /* follow the old unix: rule */

                /* display specifies TCP */
                unsigned short port = X_TCP_PORT + display;
                return _xcb_open_tcp(host, protocol, port);
            }
    }

    /* display specifies Unix socket */
    snprintf(file, sizeof(file), "%s%d", base, display);
    return  _xcb_open_unix(protocol, file);


    return fd;
}

#ifdef DNETCONN
static int _xcb_open_decnet(const char *host, const char *protocol, const unsigned short port)
{
    int fd;
    struct sockaddr_dn addr;
    struct accessdata_dn accessdata;
    struct nodeent *nodeaddr = getnodebyname(host);

    if(!nodeaddr)
        return -1;
    if (protocol && strcmp("dnet",protocol))
        return -1;
    addr.sdn_family = AF_DECnet;

    addr.sdn_add.a_len = nodeaddr->n_length;
    memcpy(addr.sdn_add.a_addr, nodeaddr->n_addr, addr.sdn_add.a_len);

    sprintf((char *)addr.sdn_objname, "X$X%d", port);
    addr.sdn_objnamel = strlen((char *)addr.sdn_objname);
    addr.sdn_objnum = 0;

    fd = socket(PF_DECnet, SOCK_STREAM, 0);
    if(fd == -1)
        return -1;

    memset(&accessdata, 0, sizeof(accessdata));
    sprintf((char*)accessdata.acc_acc, "%d", getuid());
    accessdata.acc_accl = strlen((char *)accessdata.acc_acc);
    setsockopt(fd, DNPROTO_NSP, SO_CONACCESS, &accessdata, sizeof(accessdata));

    if(connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
        return -1;
    return fd;
}
#endif

static int _xcb_open_tcp(char *host, char *protocol, const unsigned short port)
{
    int fd = -1;
    struct addrinfo hints = { 0
#ifdef AI_ADDRCONFIG
                              | AI_ADDRCONFIG
#endif
#ifdef AI_NUMERICSERV
                              | AI_NUMERICSERV
#endif
                              , AF_UNSPEC, SOCK_STREAM };
    char service[6]; /* "65535" with the trailing '\0' */
    struct addrinfo *results, *addr;
    char *bracket;

    if (protocol && strcmp("tcp",protocol))
        return -1;

#ifdef AF_INET6
    /* Allow IPv6 addresses enclosed in brackets. */
    if(host[0] == '[' && (bracket = strrchr(host, ']')) && bracket[1] == '\0')
    {
        *bracket = '\0';
        ++host;
        hints.ai_flags |= AI_NUMERICHOST;
        hints.ai_family = AF_INET6;
    }
#endif

    snprintf(service, sizeof(service), "%hu", port);
    if(getaddrinfo(host, service, &hints, &results))
        /* FIXME: use gai_strerror, and fill in error connection */
        return -1;

    for(addr = results; addr; addr = addr->ai_next)
    {
        fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if(fd >= 0 && connect(fd, addr->ai_addr, addr->ai_addrlen) >= 0)
            break;
        fd = -1;
    }
    freeaddrinfo(results);
    return fd;
}

static int _xcb_open_unix(char *protocol, const char *file)
{
    int fd;
    struct sockaddr_un addr;

    if (protocol && strcmp("unix",protocol))
        return -1;

    strcpy(addr.sun_path, file);
    addr.sun_family = AF_UNIX;
#if HAVE_SOCKADDR_SUN_LEN
    addr.sun_len = SUN_LEN(&addr);
#endif
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd == -1)
        return -1;
    if(connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
        return -1;
    return fd;
}

xcb_connection_t *
connect_to_display (const char      *name,
		    xcb_auth_info_t *auth)
{
    int fd, len, screen, display = 0;
    char *host;
    char *protocol = NULL;
    char *slash;
    xcb_connection_t *c;

    slash = strrchr(name, '/');
    if (slash) {
        len = slash - name;
	protocol = malloc(len + 1);
	if(!protocol)
	    return 0;
	memcpy (protocol, name, len);
	(protocol)[len] = '\0';
    }

    if (!xcb_parse_display (name, &host, &display, &screen))
        return NULL;

    fd = _xcb_open (host, protocol, display);
    free (host);

    if (fd == -1)
        return NULL;

    if (_get_auth_info (fd, auth, display))
    {
        c = xcb_connect_to_fd (fd, auth);
    }
    else
    {
        c = xcb_connect_to_fd (fd, 0);
	auth->name = NULL;
	auth->data = NULL;
    }

    return c;
}

void
append_auth_info (DBusMessageIter *iter,
		  xcb_auth_info_t *auth)
{
    DBusMessageIter subiter;
    int             i;

    dbus_message_iter_open_container (iter,
				      DBUS_TYPE_ARRAY,
				      DBUS_TYPE_BYTE_AS_STRING,
				      &subiter);
    for (i = 0; i < auth->namelen; i++)
	dbus_message_iter_append_basic (&subiter,
					DBUS_TYPE_BYTE,
					&auth->name[i]);
    dbus_message_iter_close_container (iter, &subiter);

    dbus_message_iter_open_container (iter,
				      DBUS_TYPE_ARRAY,
				      DBUS_TYPE_BYTE_AS_STRING,
				      &subiter);
    for (i = 0; i < auth->datalen; i++)
	dbus_message_iter_append_basic (&subiter,
					DBUS_TYPE_BYTE,
					&auth->data[i]);
    dbus_message_iter_close_container (iter, &subiter);
}

static const unsigned int hexvalues[256] = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, /* 9 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, /* 19 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, /* 29 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, /* 39 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, /* 49 */
    0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x0, 0x0, /* 59 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0xa, 0xb, 0xb, 0xd, 0xe, /* 69 */
    0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, /* 79 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, /* 89 */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xa, 0xb, 0xc, /* 99 */
    0xd, 0xe, 0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
};

int
main (int argc, char **argv)
{
    DBusError        err;
    DBusConnection   *conn;
    DBusMessage      *reply;
    DBusMessageIter  iter;
    int              i, ret, fd, len, screen_num, display = 0;
    char             *host;
    char             *protocol = NULL;
    char             *slash;
    xcb_connection_t *c = NULL;
    xcb_auth_info_t  auth = { 0 };
    char	     *displayname = NULL;
    char	     *name = NULL;
    char	     *auth_name = NULL;
    char	     *auth_data = NULL;
    int              dmxdisplay = 0;
    int              dmxscreen = -1;
    int              viewonly = 0;
    uint32_t         window = 0;
    uint32_t         screen = 0;
    dbus_bool_t      core = TRUE;
    char             dest[256];
    char             path[256];
    uint32_t         index;
    int              timeout = 600000; /* 10 minutes */

    for (i = 1; i < argc; i++)
    {
	if (*argv[i] == '-')
	{   
	    if (strcmp (argv[i], "-display") == 0)
	    {
		if (++i < argc)
		    dmxdisplay = strtol (argv[i], NULL, 0);
	    }
	    else if (strcmp (argv[i], "-screen") == 0)
	    {
		if (++i < argc)
		    dmxscreen = strtol (argv[i], NULL, 0);
	    }
	    else if (strcmp (argv[i], "-window") == 0)
	    {
		if (++i < argc)
		    window = strtol (argv[i], NULL, 0);
	    }
	    else if (strcmp (argv[i], "-name") == 0)
	    {
		if (++i < argc)
		    name = argv[i];
	    }
	    else if (strcmp (argv[i], "-authtype") == 0)
	    {
		if (++i < argc)
		    auth_name = argv[i];
	    }
	    else if (strcmp (argv[i], "-authdata") == 0)
	    {
		if (++i < argc)
		    auth_data = argv[i];
	    }
	    else if (strcmp (argv[i], "-viewonly") == 0)
	    {
		viewonly = TRUE;
	    }
	    else
	    {
		break;
	    }
	}
	else if (!displayname)
	{
	    displayname = argv[i];
	}
    }

    if (i < argc || !displayname)
    {
	fprintf (stderr,
		 "usage: %s "
		 "[-display <Xdmx-display>] "
		 "[-screen <Xdmx-screen>] "
		 "[-window <wid>] "
		 "[-name <name>] "
		 "[-authtype <protoname>] "
		 "[-authdata <hexkey>] "
		 "[-viewonly] "
		 "host[:port]\n",
		 argv[0]);
	return 1;
    }

    dbus_error_init (&err);

    conn = dbus_bus_get (DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set (&err))
    {
	fprintf (stderr, "DBus Error: %s\n", err.message);
	dbus_error_free (&err);

	return 1;
    }

    if (!xcb_parse_display (displayname, &host, &display, &screen_num))
    {
	fprintf (stderr, "Bad display name: %s\n", displayname);
        return 1;
    }

    if (!name)
    {
	name = host;
	if (!name)
	    name = "localhost";
    }

    slash = strrchr(displayname, '/');
    if (slash) {
        len = slash - displayname;
	protocol = malloc(len + 1);
	if(!protocol)
	{
	    fprintf (stderr, "Not enough memory\n");
	    free (host);
	    return 1;
	}
	memcpy (protocol, displayname, len);
	(protocol)[len] = '\0';
    }

    fd = _xcb_open (host, protocol, display);

    if (protocol)
	free (protocol);

    if (fd == -1)
    {
	fprintf (stderr, "Can't open socket: %s\n", displayname);
	free (host);
        return 1;
    }

    auth.namelen = 0;
    auth.datalen = 0;

    if (auth_name && auth_data)
    {
	char *data, *ptr, *hexdata = auth_data;
	int  size;

	size = strlen (hexdata) / 2;
	data = ptr = malloc (size);
	if (data)
	{
	    int j;

	    for (j = 0; j < size; j++)
	    {
		*ptr++ = (char)
		    ((hexvalues[(int) hexdata[0]] * 16) +
		     (hexvalues[(int) hexdata[1]]));
		hexdata += 2;
	    }

	    auth.namelen = strlen (auth_name);
	    auth.name    = strdup (auth_name);

	    auth.datalen = size;
	    auth.data    = data;

	    c = xcb_connect_to_fd (fd, &auth);
	    if (!c)
	    {
		free (auth.name);
		free (auth.data);
		auth.namelen = 0;
		auth.datalen = 0;
	    }
	}
    }

    if (!c)
    {
	if (_get_auth_info (fd, &auth, display))
	{
	    c = xcb_connect_to_fd (fd, &auth);
	    if (!c)
	    {
		free (auth.name);
		free (auth.data);
		auth.namelen = 0;
		auth.datalen = 0;
	    }
	}
	else
	{
	    c = xcb_connect_to_fd (fd, 0);
	}

	if (!c)
	{
	    fprintf (stderr, "Can't open display: %s\n", displayname);
	    free (host);
	    return 1;
	}
    }

    sprintf (dest, "org.x.config.display%d", dmxdisplay);
    sprintf (path, "/org/x/config/dmx/%d", dmxdisplay);

    if (!name)
	name = host;

    if (dmxscreen >= 0)
	screen = dmxscreen;

    do
    {
	DBusMessage *message;

	message = dbus_message_new_method_call (dest,
						path,
						"org.x.config.dmx",
						"attachScreen");
	if (!message)
	{
	    dbus_set_error (&err,
			    DBUS_ERROR_NO_MEMORY,
			    "Not enough memory");
	    break;
	}

	dbus_message_iter_init_append (message, &iter);

	dbus_message_iter_append_basic (&iter,
					DBUS_TYPE_UINT32,
					&screen);
	dbus_message_iter_append_basic (&iter,
					DBUS_TYPE_STRING,
					&displayname);
	dbus_message_iter_append_basic (&iter,
					DBUS_TYPE_STRING,
					&name);
	dbus_message_iter_append_basic (&iter,
					DBUS_TYPE_UINT32,
					&window);

	append_auth_info (&iter, &auth);

	reply = dbus_connection_send_with_reply_and_block (conn,
							   message,
							   timeout,
							   &err);

	dbus_message_unref (message);

	if (dbus_error_is_set (&err))
	{
	    if (dmxscreen < 0)
	    {
		if (strcmp (err.name, DMX_ERROR_SCREEN_IN_USE) == 0)
		{
		    dbus_error_free (&err);
		    dbus_error_init (&err);

		    screen++; /* try next screen */
		}
		else
		{
		    if (strcmp (err.name, DMX_ERROR_INVALID_SCREEN) == 0)
		    {
			dbus_error_free (&err);
			dbus_error_init (&err);

			dbus_set_error (&err,
					DMX_ERROR_SCREEN_IN_USE,
					"No available screens on display %d",
					dmxdisplay);
		    }

		    break;
		}
	    }
	    else
	    {
		break;
	    }
	}
    } while (!reply);

    xcb_disconnect (c);

    if (dbus_error_is_set (&err))
    {
	fprintf (stderr, "Error: %s\n", err.message);
	dbus_error_free (&err);

	return 1;
    }

    dbus_message_unref (reply);

    /* failing to add input is not an error */
    if (!viewonly)
    {
	DBusMessage *message;

	message = dbus_message_new_method_call (dest,
						path,
						"org.x.config.dmx",
						"addInput");
	if (!message)
	{
	    fprintf (stderr, "Warning: addInput: Not enough memory\n");
	    return 0;
	}

	dbus_message_iter_init_append (message, &iter);

	dbus_message_iter_append_basic (&iter,
					DBUS_TYPE_UINT32,
					&screen);

	dbus_message_iter_append_basic (&iter,
					DBUS_TYPE_BOOLEAN,
					&core);

	reply = dbus_connection_send_with_reply_and_block (conn,
							   message,
							   timeout,
							   &err);

	dbus_message_unref (message);

	if (dbus_error_is_set (&err))
	{
	    fprintf (stderr, "Warning: addInput: %s\n", err.message);
	    dbus_error_free (&err);

	    return 0;
	}

	dbus_message_unref (reply);
    }

    return 0;
}
