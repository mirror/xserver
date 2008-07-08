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
#include "dmxcomp.h"
#include "compint.h"

extern int (*ProcCompositeVector[CompositeNumberRequests]) (ClientPtr);
static int (*dmxSaveCompositeVector[CompositeNumberRequests]) (ClientPtr);

unsigned long DMX_CLIENTWINDOW;
unsigned long DMX_CLIENTSUBWINDOWS;

static int
dmxProcCompositeRedirectWindow (ClientPtr client)
{
    WindowPtr	pWin;
    int rc;
    REQUEST(xCompositeRedirectWindowReq);

    REQUEST_SIZE_MATCH(xCompositeRedirectWindowReq);

    rc = dixLookupResource((pointer *)&pWin, stuff->window, RT_WINDOW, client,
			   DixSetAttrAccess|DixManageAccess|DixBlendAccess);
    if (rc != Success)
    {
	client->errorValue = stuff->window;
	return (rc == BadValue) ? BadWindow : rc;
    }

    /* Add implicit CompositeRedirectManual. This prevents clients
       from using CompositeRedirectManual and composite extension from
       handling window updates. Window updates are handled by
       back-end servers. */
    if (!GetCompWindow (pWin))
    {
	rc = compRedirectWindow (serverClient, pWin, CompositeRedirectManual);
	if (rc != Success)
	    return rc;
    }

    rc = compRedirectWindow (client, pWin, stuff->update);
    if (rc == Success)
    {
	AddResource (GetCompWindow (pWin)->clients->id,
		     DMX_CLIENTWINDOW,
		     pWin);
    }
    else
    {
	if (!GetCompWindow (pWin)->clients->next)
	    compUnredirectWindow (serverClient, pWin, CompositeRedirectManual);
    }

    return rc;
}

static int
dmxProcCompositeRedirectSubwindows (ClientPtr client)
{
    WindowPtr	pWin;
    int rc;
    REQUEST(xCompositeRedirectSubwindowsReq);

    REQUEST_SIZE_MATCH(xCompositeRedirectSubwindowsReq);

    rc = dixLookupResource((pointer *)&pWin, stuff->window, RT_WINDOW, client,
			   DixSetAttrAccess|DixManageAccess|DixBlendAccess);
    if (rc != Success)
    {
	client->errorValue = stuff->window;
	return (rc == BadValue) ? BadWindow : rc;
    }

    if (!GetCompSubwindows (pWin))
    {
	rc = compRedirectSubwindows (serverClient, pWin,
				     CompositeRedirectManual);
	if (rc != Success)
	    return rc;
    }

    rc = compRedirectSubwindows (client, pWin, stuff->update);
    if (rc == Success)
    {
	AddResource (GetCompSubwindows (pWin)->clients->id,
		     DMX_CLIENTSUBWINDOWS,
		     pWin);
    }
    else
    {
	if (!GetCompSubwindows (pWin)->clients->next)
	    compUnredirectSubwindows (serverClient, pWin,
				      CompositeRedirectManual);
    }

    return rc;
}

static int
dmxFreeCompositeClientWindow (pointer value, XID ccwid)
{
    WindowPtr	        pWin = value;
    CompWindowPtr	cw = GetCompWindow (pWin);
    CompClientWindowPtr	ccw;
    int		        count = 0;

    for (ccw = cw->clients; ccw; ccw = ccw->next)
	if (ccw->update != CompositeRedirectManual)
	    count++;

    /* free our implicit manual redirect if that's the only one left */
    if (count <= 1)
	compUnredirectWindow (serverClient, pWin, CompositeRedirectManual);

    return Success;
}

static int
dmxFreeCompositeClientSubwindows (pointer value, XID ccwid)
{
    WindowPtr	        pWin = value;
    CompSubwindowsPtr   csw = GetCompSubwindows (pWin);
    CompClientWindowPtr	ccw;
    int		        count = 0;

    for (ccw = csw->clients; ccw; ccw = ccw->next)
	if (ccw->update != CompositeRedirectManual)
	    count++;

    /* free our implicit manual redirect if that's the only one left */
    if (count <= 1)
	compUnredirectSubwindows (serverClient, pWin,
				  CompositeRedirectManual);

    return Success;
}

/** Initialize the Proc Vector for the Composite extension.  The functions
 *  here cannot be handled by the mi layer Composite hooks either because
 *  the required information is no longer available when it reaches the
 *  mi layer or no mi layer hooks exist.  This function is called from
 *  InitOutput() since it should be initialized only once per server
 *  generation. */
void
dmxInitComposite (void)
{
    int i;

    DMX_CLIENTWINDOW = CreateNewResourceType (dmxFreeCompositeClientWindow);
    DMX_CLIENTSUBWINDOWS =
	CreateNewResourceType (dmxFreeCompositeClientSubwindows);

    for (i = 0; i < CompositeNumberRequests; i++)
        dmxSaveCompositeVector[i] = ProcCompositeVector[i];

    ProcCompositeVector[X_CompositeRedirectWindow]
	= dmxProcCompositeRedirectWindow;
    ProcCompositeVector[X_CompositeRedirectSubwindows]
	= dmxProcCompositeRedirectSubwindows;
}

/** Reset the Proc Vector for the Composite extension back to the original
 *  functions.  This function is called from dmxCloseScreen() during the
 *  server reset (only for screen #0). */
void
dmxResetComposite (void)
{
    int i;

    for (i = 0; i < CompositeNumberRequests; i++)
        ProcCompositeVector[i] = dmxSaveCompositeVector[i];
}
