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

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmx.h"
#include "dmxgrab.h"

static int (*dmxSaveProcVector[256]) (ClientPtr);

static int
dmxProcGrabPointer (ClientPtr client)
{
    int err;
    REQUEST(xGrabPointerReq);

    (void) stuff;

    err = (*dmxSaveProcVector[X_GrabPointer]) (client);
    if (err != Success)
	return err;

    return err;
}

static int
dmxProcUngrabPointer (ClientPtr client)
{
    int err;
    REQUEST(xResourceReq);

    (void) stuff;

    err = (*dmxSaveProcVector[X_UngrabPointer]) (client);
    if (err != Success)
	return err;

    return err;
}

static int
dmxProcGrabButton (ClientPtr client)
{
    int err;
    REQUEST(xGrabButtonReq);

    (void) stuff;

    err = (*dmxSaveProcVector[X_GrabButton]) (client);
    if (err != Success)
	return err;

    return err;
}

static int
dmxProcUngrabButton (ClientPtr client)
{
    int err;
    REQUEST(xUngrabButtonReq);

    (void) stuff;

    err = (*dmxSaveProcVector[X_UngrabButton]) (client);
    if (err != Success)
	return err;

    return err;
}

static int
dmxProcChangeActivePointerGrab (ClientPtr client)
{
    int err;
    REQUEST(xChangeActivePointerGrabReq);

    (void) stuff;

    err = (*dmxSaveProcVector[X_ChangeActivePointerGrab]) (client);
    if (err != Success)
	return err;

    return err;
}

static int
dmxProcAllowEvents (ClientPtr client)
{
    int err;
    REQUEST(xAllowEventsReq);

    (void) stuff;

    err = (*dmxSaveProcVector[X_AllowEvents]) (client);
    if (err != Success)
	return err;

    return err;
}

void dmxInitGrabs (void)
{
    int i;

    for (i = 0; i < 256; i++)
	dmxSaveProcVector[i] = ProcVector[i];

    ProcVector[X_GrabPointer]             = dmxProcGrabPointer;
    ProcVector[X_UngrabPointer]           = dmxProcUngrabPointer;
    ProcVector[X_GrabButton]              = dmxProcGrabButton;
    ProcVector[X_UngrabButton]            = dmxProcUngrabButton;
    ProcVector[X_ChangeActivePointerGrab] = dmxProcChangeActivePointerGrab;
    ProcVector[X_AllowEvents]             = dmxProcAllowEvents;
}

void dmxResetGrabs (void)
{
    int  i;

    for (i = 0; i < 256; i++)
	ProcVector[i] = dmxSaveProcVector[i];
}
