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
#include "dmxshm.h"
#include "shmint.h"

#ifdef PANORAMIX
#include "panoramiX.h"
#include "panoramiXsrv.h"
#endif

#ifdef MITSHM

unsigned long DMX_SHMSEG;

extern int (*ProcShmVector[ShmNumberRequests])(ClientPtr);

static int (*dmxSaveProcVector[ShmNumberRequests]) (ClientPtr);

static int
dmxFreeShmSeg (pointer value,
	       XID     id)
{
    return Success;
}

static int
dmxProcShmAttach (ClientPtr client)
{
    int err;

    err = (*dmxSaveProcVector[X_ShmAttach]) (client);
    if (err != Success)
	return err;

    return Success;
}

static int
dmxProcShmDetach (ClientPtr client)
{
    int err;

    err = (*dmxSaveProcVector[X_ShmDetach]) (client);
    if (err != Success)
	return err;

    return Success;
}

static int
dmxProcShmGetImage (ClientPtr client)
{
    int err;

    err = (*dmxSaveProcVector[X_ShmGetImage]) (client);
    if (err != Success)
	return err;

    return Success;
}

static int
dmxProcShmPutImage (ClientPtr client)
{
    int err;

    err = (*dmxSaveProcVector[X_ShmPutImage]) (client);
    if (err != Success)
	return err;

    return Success;
}

void dmxInitShm (void)
{
    int i;

    DMX_SHMSEG = CreateNewResourceType (dmxFreeShmSeg);

    for (i = 0; i < ShmNumberRequests; i++)
	dmxSaveProcVector[i] = ProcShmVector[i];

    ProcShmVector[X_ShmAttach]   = dmxProcShmAttach;
    ProcShmVector[X_ShmDetach]   = dmxProcShmDetach;
    ProcShmVector[X_ShmGetImage] = dmxProcShmGetImage;
    ProcShmVector[X_ShmPutImage] = dmxProcShmPutImage;
    ProcShmVector[X_ShmGetImage] = dmxProcShmGetImage;
}

void dmxResetShm (void)
{
    int i;

    for (i = 0; i < ShmNumberRequests; i++)
	ProcShmVector[i] = dmxSaveProcVector[i];
}

#endif
