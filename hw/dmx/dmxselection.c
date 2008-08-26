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
#include "dmxlog.h"
#include "dmxselection.h"

#include "selection.h"

#ifdef PANORAMIX
#include "panoramiX.h"
#include "panoramiXsrv.h"
#endif

static void
dmxBESetSelectionOwner (WindowPtr pWin,
			Selection *pSel)
{
}

static int (*dmxSaveProcVector[256]) (ClientPtr);

static int
dmxProcSetSelectionOwner (ClientPtr client)
{
    WindowPtr pWin;
    Selection *pSel;
    int       err;
    REQUEST(xSetSelectionOwnerReq);

    err = (*dmxSaveProcVector[X_SetSelectionOwner]) (client);
    if (err != Success)
	return err;

    if (dixLookupWindow (&pWin,
			 stuff->window,
			 serverClient,
			 DixReadAccess) != Success ||
	dixLookupSelection (&pSel,
			    stuff->selection,
			    serverClient,
			    DixReadAccess) != Success)
	return Success;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	PanoramiXRes *win;
	int          j;

	if ((win = (PanoramiXRes *) SecurityLookupIDByType (serverClient,
							    stuff->window,
							    XRT_WINDOW,
							    DixReadAccess)))
	{
	    FOR_NSCREENS_FORWARD(j) {
		if (dixLookupWindow (&pWin,
				     win->info[j].id,
				     serverClient,
				     DixReadAccess) == Success)
		    dmxBESetSelectionOwner (pWin, pSel);
	    }
	}

	return Success;
    }
#endif

    dmxBESetSelectionOwner (pWin, pSel);

    return Success;
}

static int
dmxProcGetSelectionOwner (ClientPtr client)
{
    int err;

    err = (*dmxSaveProcVector[X_GetSelectionOwner]) (client);
    if (err != Success)
	return err;

    return Success;
}

static int
dmxProcConvertSelection (ClientPtr client)
{
    int err;

    err = (*dmxSaveProcVector[X_ConvertSelection]) (client);
    if (err != Success)
	return err;

    return Success;
}

void dmxInitSelections (void)
{
    int  i;

    for (i = 0; i < 256; i++)
	dmxSaveProcVector[i] = ProcVector[i];

    ProcVector[X_SetSelectionOwner] = dmxProcSetSelectionOwner;
    ProcVector[X_ConvertSelection]  = dmxProcConvertSelection;
}

void dmxResetSelections (void)
{
    int  i;

    for (i = 0; i < 256; i++)
	ProcVector[i] = dmxSaveProcVector[i];
}
