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
#include "dmxinput.h"
#include "dmxgrab.h"

#include "windowstr.h"

#ifdef PANORAMIX
#include "panoramiX.h"
#include "panoramiXsrv.h"
#endif

unsigned long DMX_PASSIVEGRAB;

static int (*dmxSaveProcVector[256]) (ClientPtr);

void
dmxBEAddPassiveGrab (DMXInputInfo *dmxInput,
		     GrabPtr      pGrab)
{
    DMXScreenInfo *dmxScreen = (DMXScreenInfo *) dmxInput;
    WindowPtr     pWin = pGrab->window;
    WindowPtr     pConfineTo = pGrab->confineTo;

    if (dixLookupResource ((pointer *) &pGrab,
			   pGrab->resource,
			   DMX_PASSIVEGRAB,
			   serverClient,
			   DixReadAccess) != Success)
	return;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	PanoramiXRes *win, *confineToWin;

	if (!(win = (PanoramiXRes *)SecurityLookupIDByType(
		  serverClient, pWin->drawable.id, XRT_WINDOW,
		  DixGetAttrAccess)) ||
	    dixLookupWindow (&pWin,
			     win->info[dmxScreen->index].id,
			     serverClient,
			     DixGetAttrAccess) != Success)
	    return;

	if (pGrab->confineTo)
	    if (!(confineToWin = (PanoramiXRes *)SecurityLookupIDByType(
		      serverClient, pGrab->confineTo->drawable.id, XRT_WINDOW,
		      DixGetAttrAccess)) ||
		dixLookupWindow (&pConfineTo,
				 confineToWin->info[dmxScreen->index].id,
				 serverClient,
				 DixGetAttrAccess) != Success)
		return;
    }
    else
#endif
	if (dmxScreen->index != pWin->drawable.pScreen->myNum)
	    return;

    dmxInputGrabButton (dmxInput,
			pGrab->device,
			pGrab->modifierDevice,
			pWin,
			pConfineTo,
			pGrab->detail.exact,
			pGrab->modifiersDetail.exact,
			pGrab->cursor);
}

static int
dmxFreePassiveGrab (pointer value,
		    XID     id)
{
    GrabPtr      pGrab = (GrabPtr) value;

#ifdef PANORAMIX
    PanoramiXRes *win = NULL;
#endif

    WindowPtr    pWin = pGrab->window;
    int		 i;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	if (!(win = (PanoramiXRes *)SecurityLookupIDByType(
		  serverClient, pWin->drawable.id, XRT_WINDOW,
		  DixGetAttrAccess)))
	    return Success;
    }
#endif

    for (i = 0; i < dmxNumScreens; i++)
    {
	DMXScreenInfo *dmxScreen = &dmxScreens[i];

	if (!dmxScreen->beDisplay)
	    continue;

#ifdef PANORAMIX
	if (!noPanoramiXExtension)
	    dixLookupWindow (&pWin,
			     win->info[i].id,
			     serverClient,
			     DixGetAttrAccess);
	else
#endif
	    if (i != pWin->drawable.pScreen->myNum)
		continue;

	dmxInputUngrabButton (&dmxScreen->input,
			      pGrab->device,
			      pGrab->modifierDevice,
			      pWin,
			      pGrab->detail.exact,
			      pGrab->modifiersDetail.exact);
    }

    return Success;
}

static void
dmxGrabPointer (DeviceIntPtr pDev,
		GrabPtr      pGrab)
{

#ifdef PANORAMIX
    PanoramiXRes *win = NULL, *confineToWin = NULL;
#endif

    WindowPtr    pWin, pConfineTo = NULL;
    int          i;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	if (!(win = (PanoramiXRes *)SecurityLookupIDByType(
		  serverClient, pGrab->window->drawable.id, XRT_WINDOW,
		  DixGetAttrAccess)))
	    return;
	if (pGrab->confineTo)
	    if (!(confineToWin = (PanoramiXRes *)SecurityLookupIDByType(
		      serverClient, pGrab->confineTo->drawable.id,
		      XRT_WINDOW, DixGetAttrAccess)))
		return;
    }
#endif

    for (i = 0; i < dmxNumScreens; i++)
    {
	DMXScreenInfo *dmxScreen = &dmxScreens[i];

	if (!dmxScreen->beDisplay)
	    continue;

#ifdef PANORAMIX
	if (!noPanoramiXExtension)
	{
	    dixLookupWindow (&pWin,
			     win->info[i].id,
			     serverClient,
			     DixGetAttrAccess);
	    if (confineToWin)
		dixLookupWindow (&pConfineTo,
				 confineToWin->info[i].id,
				 serverClient,
				 DixGetAttrAccess);
	}
	else
#endif
	    if (i != pWin->drawable.pScreen->myNum)
		continue;

	dmxInputGrabPointer (&dmxScreen->input,
			     pDev,
			     pWin,
			     pConfineTo,
			     pGrab->cursor);
    }
}

static void
dmxUngrabPointer (DeviceIntPtr pDev,
		  GrabPtr      pGrab)
{

#ifdef PANORAMIX
    PanoramiXRes *win = NULL;
#endif

    WindowPtr    pWin;
    int          i;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	if (!(win = (PanoramiXRes *)SecurityLookupIDByType(
		  serverClient, pGrab->window->drawable.id, XRT_WINDOW,
		  DixGetAttrAccess)))
	    return;
    }
#endif

    for (i = 0; i < dmxNumScreens; i++)
    {
	DMXScreenInfo *dmxScreen = &dmxScreens[i];

	if (!dmxScreen->beDisplay)
	    continue;

#ifdef PANORAMIX
	if (!noPanoramiXExtension)
	{
	    dixLookupWindow (&pWin,
			     win->info[i].id,
			     serverClient,
			     DixGetAttrAccess);
	}
	else
#endif
	    if (i != pWin->drawable.pScreen->myNum)
		continue;

	dmxInputUngrabPointer (&dmxScreen->input, pDev, pWin);
    }
}

void
dmxActivatePointerGrab (DeviceIntPtr pDev,
			GrabPtr      pGrab,
			TimeStamp    time,
			Bool         autoGrab)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDev);

    dmxGrabPointer (pDev, pGrab);

    DMX_UNWRAP (ActivateGrab, pDevPriv, &pDev->deviceGrab);
    (*pDev->deviceGrab.ActivateGrab) (pDev, pGrab, time, autoGrab);
    DMX_WRAP (ActivateGrab, dmxActivatePointerGrab, pDevPriv,
	      &pDev->deviceGrab);
}

void
dmxDeactivatePointerGrab (DeviceIntPtr pDev)
{
    dmxDevicePrivPtr pDevPriv = DMX_GET_DEVICE_PRIV (pDev);
    GrabPtr          pGrab = pDev->deviceGrab.grab;

    /* DeactivateGrab might call ActivateGrab so make sure we ungrab here */
    dmxUngrabPointer (pDev, pGrab);

    DMX_UNWRAP (DeactivateGrab, pDevPriv, &pDev->deviceGrab);
    (*pDev->deviceGrab.DeactivateGrab) (pDev);
    DMX_WRAP (DeactivateGrab, dmxDeactivatePointerGrab, pDevPriv,
	      &pDev->deviceGrab);
}

static int
dmxProcGrabButton (ClientPtr client)
{
    GrabPtr      pGrab;

#ifdef PANORAMIX
    PanoramiXRes *win = NULL, *confineToWin = NULL;
#endif

    WindowPtr    pWin, pConfineTo = NULL;
    int          i, err;
    REQUEST(xGrabButtonReq);

    err = (*dmxSaveProcVector[X_GrabButton]) (client);
    if (err != Success)
	return err;

    return Success; /* PASSIVE GRABS DISABLED */

    dixLookupWindow(&pWin, stuff->grabWindow, client, DixSetAttrAccess);
    pGrab = wPassiveGrabs (pWin);

    pConfineTo = pGrab->confineTo;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	if (!(win = (PanoramiXRes *)SecurityLookupIDByType(
		  client, stuff->grabWindow, XRT_WINDOW, DixSetAttrAccess)))
	    return Success;
	if (pConfineTo)
	    if (!(confineToWin = (PanoramiXRes *)SecurityLookupIDByType(
		      client, stuff->confineTo, XRT_WINDOW, DixSetAttrAccess)))
		return Success;
    }
#endif

    for (i = 0; i < dmxNumScreens; i++)
    {
	DMXScreenInfo *dmxScreen = &dmxScreens[i];

	if (!dmxScreen->beDisplay)
	    continue;

#ifdef PANORAMIX
	if (!noPanoramiXExtension)
	{
	    dixLookupWindow (&pWin,
			     win->info[i].id,
			     client,
			     DixSetAttrAccess);
	    if (confineToWin)
		dixLookupWindow (&pConfineTo,
				 confineToWin->info[i].id,
				 client,
				 DixSetAttrAccess);
	}
	else
#endif
	    if (i != pWin->drawable.pScreen->myNum)
		continue;

	dmxInputGrabButton (&dmxScreen->input,
			    pGrab->device,
			    pGrab->modifierDevice,
			    pWin,
			    pConfineTo,
			    pGrab->detail.exact,
			    pGrab->modifiersDetail.exact,
			    pGrab->cursor);
    }

    AddResource (pGrab->resource, DMX_PASSIVEGRAB, pGrab);

    return Success;
}

static int
dmxProcChangeActivePointerGrab (ClientPtr client)
{
    DeviceIntPtr pDev;
    GrabPtr      pGrab;
    int          err;

    err = (*dmxSaveProcVector[X_ChangeActivePointerGrab]) (client);
    if (err != Success)
	return err;

    pDev = PickPointer (client);
    pGrab = pDev->deviceGrab.grab;
    if (pGrab)
	dmxGrabPointer (pDev, pGrab);

    return Success;
}

void dmxInitGrabs (void)
{
    int i;

    DMX_PASSIVEGRAB = CreateNewResourceType (dmxFreePassiveGrab);

    for (i = 0; i < 256; i++)
	dmxSaveProcVector[i] = ProcVector[i];

    ProcVector[X_GrabButton]              = dmxProcGrabButton;
    ProcVector[X_ChangeActivePointerGrab] = dmxProcChangeActivePointerGrab;
}

void dmxResetGrabs (void)
{
    int  i;

    for (i = 0; i < 256; i++)
	ProcVector[i] = dmxSaveProcVector[i];
}
