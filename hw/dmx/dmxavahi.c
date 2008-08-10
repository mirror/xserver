/*
 * Copyright © 2008 Novell, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors: Hubert Figuiere <hfiguiere@novell.com>
 *          David Reveman <davidr@novell.com>
 */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/watch.h>

#include "dix.h"
#include "os.h"
#include "opaque.h"
#include "dmxavahi.h"

typedef struct _DMXAvahiPoll DMXAvahiPoll;

struct AvahiWatch {
    struct AvahiWatch *next;

    int                fd;
    AvahiWatchEvent    event;
    AvahiWatchCallback callback;
    void               *userdata;

    DMXAvahiPoll *p;
};

struct AvahiTimeout {
    struct AvahiTimeout *next;

    AvahiTimeoutCallback callback;
    void                 *userdata;
	
    DMXAvahiPoll *p;
    OsTimerPtr   timer;
};

struct _DMXAvahiPoll {
    AvahiPoll base;

    AvahiWatch   *watches;
    AvahiTimeout *timeouts;
};

static DMXAvahiPoll    avahi_poll;
static AvahiClient     *avahi_client;
static AvahiEntryGroup *avahi_group;

static const char * _service_name = "DMX service on %s";

static AvahiWatch *
dmx_avahi_watch_new (const AvahiPoll    *api,
		     int		fd,
		     AvahiWatchEvent    event,
		     AvahiWatchCallback callback,
		     void		*userdata)
{
    DMXAvahiPoll *p = (DMXAvahiPoll *) api;
    AvahiWatch   *w;

    w = xalloc (sizeof (AvahiWatch));
    if (!w)
	return NULL;

    w->fd       = fd;
    w->event    = event;
    w->callback = callback;
    w->userdata = userdata;
    w->p        = p;

    w->next = p->watches;
    p->watches = w;

    if (w->event & AVAHI_WATCH_IN)
	AddEnabledDevice (fd);

    return w;
}

static void
dmx_avahi_watch_update (AvahiWatch      *w,
			AvahiWatchEvent event)
{
    if (event & AVAHI_WATCH_IN)
    {
	if (!(w->event & AVAHI_WATCH_IN))
	    AddEnabledDevice (w->fd);
    }
    else
    {
	if (w->event & AVAHI_WATCH_IN)
	    RemoveEnabledDevice (w->fd);
    }

    w->event = event;
}

static AvahiWatchEvent
dmx_avahi_watch_get_events (AvahiWatch *w)
{
    return AVAHI_WATCH_IN;
}

static void
dmx_avahi_watch_free (AvahiWatch *w)
{
    DMXAvahiPoll *p = w->p;
    AvahiWatch   *prev;

    for (prev = p->watches; prev; prev = prev->next)
	if (prev->next == w)
	    break;

    if (prev)
	prev->next = w->next;
    else
	p->watches = w->next;

    if (w->event & AVAHI_WATCH_IN)
	RemoveEnabledDevice (w->fd);

    free (w);
}

static CARD32
avahi_poll_timeout (OsTimerPtr timer,
		    CARD32     time,
		    pointer    arg)
{
    AvahiTimeout *t = (AvahiTimeout *) arg;

    (*t->callback) (t, t->userdata);

    return 0;
}

static AvahiTimeout *
dmx_avahi_timeout_new (const AvahiPoll      *api,
		       const struct timeval *tv,
		       AvahiTimeoutCallback callback,
		       void                 *userdata)
{
    DMXAvahiPoll *p = (DMXAvahiPoll *) api;
    AvahiTimeout *t;

    t = xalloc (sizeof (AvahiTimeout));
    if (!t)
	return NULL;

    t->callback = callback;
    t->userdata = userdata;
    t->timer    = NULL;
    t->p        = p;
    
    t->next = p->timeouts;
    p->timeouts = t;

    if (tv)
	t->timer = TimerSet (NULL,
			     0,
			     ((CARD16) tv->tv_sec) * 1000 + tv->tv_usec / 1000,
			     avahi_poll_timeout,
			     t);

    return t;
}

static void
dmx_avahi_timeout_update (AvahiTimeout         *t,
			  const struct timeval *tv)
{
    if (tv)
	t->timer = TimerSet (t->timer,
			     0,
			     ((CARD16) tv->tv_sec) * 1000 + tv->tv_usec / 1000,
			     avahi_poll_timeout,
			     t);
    else
	TimerCancel (t->timer);
}

static void
dmx_avahi_timeout_free (AvahiTimeout *t)
{
    DMXAvahiPoll *p = t->p;
    AvahiTimeout *prev;

    for (prev = p->timeouts; prev; prev = prev->next)
	if (prev->next == t)
	    break;

    if (prev)
	prev->next = t->next;
    else
	p->timeouts = t->next;

    TimerFree (t->timer);

    free (t);
}

static void
avahi_poll_block_handler (pointer   blockData,
			  OSTimePtr pTimeout,
			  pointer   pReadMask)
{
}

static void
avahi_poll_wakeup_handler (pointer blockData,
			   int     result,
			   pointer pReadMask)
{
    DMXAvahiPoll *p = (DMXAvahiPoll *) blockData;
    AvahiWatch   *w = p->watches;
    fd_set       *fds = (fd_set *) pReadMask;

    while (w)
    {
	AvahiWatch *next = w->next;

	if (FD_ISSET (w->fd, fds))
	    (*w->callback) (w, w->fd, AVAHI_WATCH_IN, w->userdata);

	w = next;
    }
}

static void
dmx_avahi_poll_init (DMXAvahiPoll *p)
{
    p->base.watch_new        = dmx_avahi_watch_new;
    p->base.watch_update     = dmx_avahi_watch_update;
    p->base.watch_get_events = dmx_avahi_watch_get_events;
    p->base.watch_free       = dmx_avahi_watch_free;
    p->base.timeout_new      = dmx_avahi_timeout_new;
    p->base.timeout_update   = dmx_avahi_timeout_update;
    p->base.timeout_free     = dmx_avahi_timeout_free;

    RegisterBlockAndWakeupHandlers (avahi_poll_block_handler,
				    avahi_poll_wakeup_handler,
				    p);
}

static void
dmx_avahi_poll_fini (DMXAvahiPoll *p)
{
    while (p->timeouts)
	dmx_avahi_timeout_free (p->timeouts);

    while (p->watches)
	dmx_avahi_watch_free (p->watches);

    RemoveBlockAndWakeupHandlers (avahi_poll_block_handler,
				  avahi_poll_wakeup_handler,
				  p);
}

static void
avahi_client_callback (AvahiClient      *c,
		       AvahiClientState state,
		       void             *userdata)
{
    switch (state) {
    case AVAHI_CLIENT_S_RUNNING: {
	char hname[512], name[576];

	if (gethostname (hname, sizeof (hname)))
	    break;

	sprintf (name, _service_name, hname);

	avahi_group = avahi_entry_group_new (c, 0, 0);
	if (avahi_group)
	{
	    avahi_entry_group_add_service (avahi_group,
					   AVAHI_IF_UNSPEC,
					   AVAHI_PROTO_UNSPEC,
					   0, 
					   name,
					   "_dmx._tcp",
					   0,
					   0,
					   6000 + atoi (display),
					   NULL);

	    avahi_entry_group_commit (avahi_group);
	}
    } break;
    case AVAHI_CLIENT_FAILURE:
    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_CONNECTING:
	break;
    case AVAHI_CLIENT_S_REGISTERING:
	if (avahi_group)
	    avahi_entry_group_reset (avahi_group);
    default:
	break;
    }
}

void
dmx_avahi_init (void)
{
    dmx_avahi_poll_init (&avahi_poll);
	
    avahi_group  = NULL;
    avahi_client = avahi_client_new (&avahi_poll.base,
				     0, 
				     avahi_client_callback,
				     NULL,
				     NULL);
}

void
dmx_avahi_fini (void)
{
    if (avahi_group)
    {
	avahi_entry_group_free (avahi_group);
	avahi_group = NULL;
    }

    if (avahi_client)
    {
	avahi_client_free (avahi_client);
	avahi_client = NULL;
    }

    dmx_avahi_poll_fini (&avahi_poll);
}
