/*
 * Copyright 2002-2004 Red Hat Inc., Durham, North Carolina.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL RED HAT AND/OR THEIR SUPPLIERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Authors:
 *   Rickard E. (Rik) Faith <faith@redhat.com>
 *
 */

/** \file
 *
 * The DMX server code is written to call #dmxSync() whenever an XSync()
 * might be necessary.  However, since XSync() requires a two way
 * communication with the other X server, eliminating unnecessary
 * XSync() calls is a key performance optimization.  Support for this
 * optimization is provided here.  Statistics about XSync() calls and
 * latency are gathered in #dmxstat.c.
 *
 * During the initial conversion from calling XSync() immediately to the
 * XSync() batching method implemented in this file, it was noted that,
 * out of more than 300 \a x11perf tests, 8 tests became more than 100
 * times faster, with 68 more than 50X faster, 114 more than 10X faster,
 * and 181 more than 2X faster. */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmx.h"
#include "dmxsync.h"
#include "dmxstat.h"
#include "dmxlog.h"
#include "dmxextension.h"
#include "dmxscrinit.h"
#include <sys/time.h>

static int        dmxSyncInterval = 100; /* Default interval in milliseconds */
static OsTimerPtr dmxSyncTimer;
static int        dmxSyncPending = 0;
static int        dmxSyncRequest = 0;

/* dispatch all pending back-end server responses */
void
dmxDispatch (void)
{
    int i;

    for (i = 0; i < dmxNumScreens; i++)
	if (dmxScreens[i].alive)
	    dmxBEDispatch (screenInfo.screens[i]);
}

/* non-blocking wait for back-end server response. returns 0 when no
   more reponses can arrive */
int
dmxWaitForResponse (void)
{
    fd_set rfds;
    int    nfd = 0;
    int    i, ret = 0;

    FD_ZERO (&rfds);
     
    for (i = 0; i < dmxNumScreens; i++)
    {
	if (dmxScreens[i].beDisplay && dmxScreens[i].alive)
	{
	    xcb_flush (dmxScreens[i].connection);

	    if (xcb_connection_has_error (dmxScreens[i].connection))
	    {
		dmxScreens[i].alive = FALSE;
		ret = -1;
	    }
	    else
	    {
		FD_SET (dmxScreens[i].fd, &rfds);

		if (dmxScreens[i].fd > nfd)
		    nfd = dmxScreens[i].fd;
	    }
	}
    }

    if (ret == 0 && nfd)
    {
	do {
	    ret = select (nfd + 1, &rfds, 0, 0, 0);
	} while (ret == -1 && errno == EINTR);

	/* screens with broken connections are detached in the
	   block handler */
    }

    return nfd;
}

static void dmxDoSync(DMXScreenInfo *dmxScreen)
{
    dmxScreen->needsSync = FALSE;

    if (!dmxScreen->alive)
	return; /* FIXME: Is this correct behavior for sync stats? */

    if (dmxScreen->sync.sequence)
	return;

    dmxScreen->sync = xcb_get_input_focus (dmxScreen->connection);
    dmxAddSequence (&dmxScreen->request, dmxScreen->sync.sequence);
    dmxSyncRequest++;
}

static CARD32 dmxSyncCallback(OsTimerPtr timer, CARD32 time, pointer arg)
{
    int i;

    /* make sure TimerFree is not called from while waiting for
       pending replies */
    dmxSyncTimer = NULL;

    /* wait for all pending sync replies */
    do {
	dmxDispatch ();
    } while (dmxSyncRequest && dmxWaitForResponse ());

    if (dmxSyncPending)
    {
	for (i = 0; i < dmxNumScreens; i++)
	    if (dmxScreens[i].needsSync)
		dmxDoSync (&dmxScreens[i]);

	dmxSyncPending = 0;

	if (dmxSyncRequest)
	{
	    dmxSyncTimer = timer;
	    return dmxSyncInterval;
	}
    }

    return 0;                   /* Do not place on queue again */
}

/** Request the XSync() batching optimization with the specified \a
 * interval (in mS).  If the \a interval is 0, 100mS is used.  If the \a
 * interval is less than 0, then the XSync() batching optimization is
 * not requested (e.g., so the -syncbatch -1 command line option can
 * turn off the default 100mS XSync() batching).
 *
 * Note that the parameter to this routine is a string, since it will
 * usually be called from #ddxProcessArgument in #dmxinit.c */
void dmxSyncActivate(const char *interval)
{
    dmxSyncInterval = (interval ? atoi(interval) : 100);

    if (dmxSyncInterval < 0) dmxSyncInterval = 0;
}

/** Initialize the XSync() batching optimization, but only if
 * #dmxSyncActivate was last called with a non-negative value. */
void dmxSyncInit(void)
{
    if (dmxSyncInterval) {
        dmxLog(dmxInfo, "XSync batching with %d ms interval\n",
               dmxSyncInterval);
    } else {
        dmxLog(dmxInfo, "XSync batching disabled\n");
    }
}

/** Request an XSync() to the display used by \a dmxScreen.  If \a now
 * is TRUE, call XSync() immediately instead of waiting for the next
 * XSync() batching point.  Note that if XSync() batching was deselected
 * with #dmxSyncActivate() before #dmxSyncInit() was called, then no
 * XSync() batching is performed and this function always calles XSync()
 * immediately.
 *
 * (Note that this function uses TimerSet but works correctly in the
 * face of a server generation.  See the source for details.)
 *
 * If \a dmxScreen is \a NULL, then all pending syncs will be flushed
 * immediately.
 */
void dmxSync(DMXScreenInfo *dmxScreen, Bool now)
{
    static unsigned long dmxGeneration = 0;

    if (dmxSyncInterval) {
        if (dmxGeneration != serverGeneration) {
            /* Server generation does a TimerInit, which frees all
             * timers.  So, at this point dmxSyncTimer is either:
             * 1) NULL, iff dmxGeneration == 0,
             * 2) freed, if it was on a queue (dmxSyncPending != 0), or
             * 3) allocated, if it wasn't on a queue (dmxSyncPending == 0)
             */
            if (dmxSyncTimer && !dmxSyncPending) xfree(dmxSyncTimer);
            dmxSyncTimer  = NULL;
            now           = TRUE;
            dmxGeneration = serverGeneration;
        }

	/* Queue sync */
        if (dmxScreen) {
	    if (now && dmxScreen->inDispatch)
	    {
		dmxLog (dmxWarning,
			"Immediate sync from within back-end dispatch\n");
		free (xcb_get_input_focus_reply
		      (dmxScreen->connection,
		       xcb_get_input_focus (dmxScreen->connection),
		       NULL));
		return;
	    }
            dmxScreen->needsSync = TRUE;
            ++dmxSyncPending;
        }

	/* Do sync or set time for later */
        if (now || !dmxScreen)
	{
	    if (dmxSyncTimer)
	    {
		TimerFree (dmxSyncTimer);
		dmxSyncTimer = NULL;
	    }

	    while (dmxSyncRequest || dmxSyncPending)
		dmxSyncCallback (NULL, 0, NULL);

            /* At this point, dmxSyncPending == 0 because
             * dmxSyncCallback must have been called. */
            if (dmxSyncPending)
                dmxLog(dmxFatal, "dmxSync(%s,%d): dmxSyncPending = %d\n",
                       dmxScreen ? dmxScreen->display : "", now, dmxSyncPending);
        }
	else if (!dmxSyncTimer)
	{
	    dmxSyncTimer = TimerSet (dmxSyncTimer,
				     0,
				     dmxSyncInterval,
				     dmxSyncCallback,
				     NULL);
	}
    }
    else
    {
	/* If dmxSyncInterval is not being used,
	 * then all the backends are already
	 * up-to-date. */
        if (dmxScreen)
	{
	    dmxDoSync (dmxScreen);
	    dmxSyncCallback (NULL, 0, NULL);
	}
    }
}

/* error or reply doesn't matter, all we need is some response
   from the back-end server */
Bool
dmxScreenReplyCheckSync (ScreenPtr           pScreen,
			 unsigned int        sequence,
			 xcb_generic_reply_t *reply)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (reply->response_type || reply->pad0 != DMX_DETACHED)
	if (sequence != dmxScreen->sync.sequence)
	    return FALSE;

    if (dmxScreen->sync.sequence)
    {
	dmxScreen->sync.sequence = 0;
	dmxSyncRequest--;

	if (dmxSyncRequest == 0 && dmxSyncPending == 0 && dmxSyncTimer)
	{
	    TimerFree (dmxSyncTimer);
	    dmxSyncTimer = NULL;
	}
    }
    
    return TRUE;
}
