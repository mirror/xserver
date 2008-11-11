/*
 * Copyright 2001-2004 Red Hat Inc., Durham, North Carolina.
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
 *   Kevin E. Martin <kem@redhat.com>
 *   David H. Dawes <dawes@xfree86.org>
 *
 */

/** \file
 * This file provides support for screen initialization. */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmx.h"
#include "dmxextension.h"
#include "dmxsync.h"
#include "dmxscrinit.h"
#include "dmxcursor.h"
#include "dmxgc.h"
#include "dmxgcops.h"
#include "dmxwindow.h"
#include "dmxpixmap.h"
#include "dmxfont.h"
#include "dmxcmap.h"
#include "dmxprop.h"
#include "dmxdpms.h"
#include "dmxlog.h"
#include "dmxcb.h"
#include "dmxinit.h"
#include "dmxgrab.h"
#include "dmxselection.h"
#include "dmxatom.h"
#include "dmxshm.h"
#include "dmxdnd.h"
#include "dmxlaunch.h"

#ifdef PANORAMIX
#include "panoramiX.h"
#include "panoramiXsrv.h"
#endif

#ifdef RENDER
#include "dmxpict.h"
#endif

#ifdef COMPOSITE
#include "dmxcomp.h"
#endif

#ifdef RANDR
#include "dmxrandr.h"
#endif

#ifdef XV
#include "dmxxv.h"
#endif

#include "fb.h"
#include "mipointer.h"
#include "micmap.h"
#include "mivalidate.h"

extern Bool dmxCloseScreen(int idx, ScreenPtr pScreen);
static Bool dmxSaveScreen(ScreenPtr pScreen, int what);

static unsigned long dmxGeneration;
static unsigned long *dmxCursorGeneration;

static int dmxGCPrivateKeyIndex;
DevPrivateKey dmxGCPrivateKey = &dmxGCPrivateKeyIndex; /**< Private index for GCs       */
static int dmxWinPrivateKeyIndex;
DevPrivateKey dmxWinPrivateKey = &dmxWinPrivateKeyIndex; /**< Private index for Windows   */
static int dmxPixPrivateKeyIndex;
DevPrivateKey dmxPixPrivateKey = &dmxPixPrivateKeyIndex; /**< Private index for Pixmaps   */
int dmxFontPrivateIndex;        /**< Private index for Fonts     */
static int dmxScreenPrivateKeyIndex;
DevPrivateKey dmxScreenPrivateKey = &dmxScreenPrivateKeyIndex; /**< Private index for Screens   */
static int dmxColormapPrivateKeyIndex;
DevPrivateKey dmxColormapPrivateKey = &dmxColormapPrivateKeyIndex; /**< Private index for Colormaps */
static int dmxDevicePrivateKeyIndex;
DevPrivateKey dmxDevicePrivateKey = &dmxDevicePrivateKeyIndex; /**< Private index for Devices */
#ifdef RENDER
static int dmxPictPrivateKeyIndex;
DevPrivateKey dmxPictPrivateKey = &dmxPictPrivateKeyIndex; /**< Private index for Picts     */
static int dmxGlyphSetPrivateKeyIndex;
DevPrivateKey dmxGlyphSetPrivateKey = &dmxGlyphSetPrivateKeyIndex; /**< Private index for GlyphSets */
static int dmxGlyphPrivateKeyIndex;
DevPrivateKey dmxGlyphPrivateKey = &dmxGlyphPrivateKeyIndex; /**< Private index for Glyphs */
#endif

/** Initialize the parts of screen \a idx that require access to the
 *  back-end server. */
void dmxBEScreenInit(ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    char          buf[256];
    int           i, j;

    /* FIXME: The dmxScreenInit() code currently assumes that it will
     * not be called if the Xdmx server is started with this screen
     * detached -- i.e., it assumes that dmxScreen->beDisplay is always
     * valid.  This is not necessarily a valid assumption when full
     * addition/removal of screens is implemented, but when this code is
     * broken out for screen reattachment, then we will reevaluate this
     * assumption.
     */

    pScreen->mmWidth = DisplayWidthMM(dmxScreen->beDisplay, 
				      DefaultScreen(dmxScreen->beDisplay));
    pScreen->mmHeight = DisplayHeightMM(dmxScreen->beDisplay, 
					DefaultScreen(dmxScreen->beDisplay));

    pScreen->whitePixel = dmxScreen->beWhitePixel;
    pScreen->blackPixel = dmxScreen->beBlackPixel;

    dmxScreen->selectionOwner = None;

    sprintf(buf, "DMX_%s", dmxDigest);

    /* prefetch all valid atoms */
    for (i = 1; ValidAtom ((Atom) i); i++);
    while (--i)
	dmxBEPrefetchAtom (dmxScreen, (Atom) i);
    
    XLIB_PROLOGUE (dmxScreens);
    dmxScreen->beSelectionAtom = XInternAtom (dmxScreen->beDisplay, buf, 0);
    XLIB_EPILOGUE (dmxScreen);

    /* Handle screen savers and DPMS on the backend */
    dmxBEDPMSScreenInit (pScreen);

#ifdef MITSHM
    dmxBEShmScreenInit (pScreen);
#endif

#ifdef RANDR
    dmxBERRScreenInit (pScreen);
#endif

#ifdef XV
    dmxBEXvScreenInit (pScreen);
#endif

    /* Create default drawables (used during GC creation) */
    for (i = 0; i < dmxScreen->beNumPixmapFormats; i++) 
	for (j = 0; j < dmxScreen->beNumDepths; j++)
	    if ((dmxScreen->bePixmapFormats[i].depth == 1) ||
		(dmxScreen->bePixmapFormats[i].depth ==
		 dmxScreen->beDepths[j])) {
		XLIB_PROLOGUE (dmxScreens);
		dmxScreen->scrnDefDrawables[i] = (Drawable)
		    XCreatePixmap(dmxScreen->beDisplay, dmxScreen->scrnWin,
				  1, 1, dmxScreen->bePixmapFormats[i].depth);
		XLIB_EPILOGUE (dmxScreen);
		break;
	    }
}

static void
dmxSetWindowPixmap (WindowPtr pWin, PixmapPtr pPixmap)
{
    ScreenPtr       pScreen = pWin->drawable.pScreen;
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    PixmapPtr       pOld = (*pScreen->GetWindowPixmap) (pWin);

    if (pPixmap != pOld)
    {
	dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV (pWin);

	if (pPixmap && (*pScreen->GetWindowPixmap) (pWin->parent) != pPixmap)
	    pWinPriv->redirected = TRUE;
	else
	    pWinPriv->redirected = FALSE;
    }

    DMX_UNWRAP(SetWindowPixmap, dmxScreen, pScreen);
    if (pScreen->SetWindowPixmap)
	(*pScreen->SetWindowPixmap) (pWin, pPixmap);
    DMX_WRAP(SetWindowPixmap, dmxSetWindowPixmap, dmxScreen, pScreen);
}

static Bool
dmxScreenEventCheckInput (ScreenPtr           pScreen,
			  xcb_generic_event_t *event)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    return dmxInputEventCheck (&dmxScreen->input, event);
}

static void
dmxScreenGetSelectionOwner (ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    dmxScreen->selectionOwner = None;
    while (dmxScreen->selectionOwner == None)
    {
	xcb_connection_t                *c = dmxScreen->connection;
	xcb_atom_t                      a = dmxScreen->beSelectionAtom;
	xcb_get_selection_owner_reply_t *reply;

	reply = xcb_get_selection_owner_reply (c,
					       xcb_get_selection_owner (c, a),
					       NULL);
	if (!reply)
	    break;

	if (reply->owner)
	{
	    if (reply->owner != dmxScreen->rootWin)
	    {
		const uint32_t      value = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
		xcb_void_cookie_t   r;
		xcb_generic_error_t *error;
		
		r = xcb_change_window_attributes_checked (c,
							  reply->owner,
							  XCB_CW_EVENT_MASK,
							  &value);
		error = xcb_request_check (c, r);
		if (error)
		{
		    if (error->error_code != BadWindow)
			dmxScreen->selectionOwner = reply->owner;

		    free (error);
		}
		else
		{
		    dmxScreen->selectionOwner = reply->owner;
		}
	    }
	    else
	    {
		dmxScreen->selectionOwner = dmxScreen->rootWin;
	    }
	}
	else
	{
	    xcb_set_selection_owner (dmxScreen->connection,
				     dmxScreen->rootWin,
				     dmxScreen->beSelectionAtom,
				     0);
	}

	free (reply);
    }
}

static Bool
dmxScreenEventCheckSelection (ScreenPtr           pScreen,
			      xcb_generic_event_t *event)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    switch (event->response_type & ~0x80) {
    case XCB_DESTROY_NOTIFY: {
	xcb_destroy_notify_event_t *xdestroy =
	    (xcb_destroy_notify_event_t *) event;

	if (dmxSelectionDestroyNotify (pScreen, xdestroy->window))
	    return TRUE;

	if (xdestroy->window != dmxScreen->selectionOwner)
	    return FALSE;

	if (dmxScreen->selectionOwner == dmxScreen->rootWin)
	    return FALSE;

	dmxScreenGetSelectionOwner (pScreen);
    } break;
    case XCB_PROPERTY_NOTIFY: {
	xcb_property_notify_event_t *xproperty =
	    (xcb_property_notify_event_t *) event;

	if (!dmxSelectionPropertyNotify (pScreen,
					 xproperty->window,
					 xproperty->state,
					 xproperty->atom,
					 xproperty->time))
	    return FALSE;
    } break;
    case XCB_SELECTION_CLEAR: {
	xcb_selection_clear_event_t *xclear =
	    (xcb_selection_clear_event_t *) event;

	if (xclear->selection == dmxScreen->beSelectionAtom)
	{
	    dmxScreenGetSelectionOwner (pScreen);
	}
	else
	{
	    dmxSelectionClear (pScreen,
			       xclear->owner,
			       xclear->selection);
	}
    } break;
    case XCB_SELECTION_NOTIFY: {
	xcb_selection_notify_event_t *xnotify =
	    (xcb_selection_notify_event_t *) event;

	dmxSelectionNotify (pScreen,
			    xnotify->requestor,
			    xnotify->selection,
			    xnotify->target,
			    xnotify->property,
			    xnotify->time);
    } break;
    case XCB_SELECTION_REQUEST: {
	xcb_selection_request_event_t *xrequest =
	    (xcb_selection_request_event_t *) event;

	dmxSelectionRequest (pScreen,
			     xrequest->owner,
			     xrequest->requestor,
			     xrequest->selection,
			     xrequest->target,
			     xrequest->property,
			     xrequest->time);
    } break;
    default:
	return FALSE;
    }

    return TRUE;
}

static void
dmxDiscardIgnore (DMXScreenInfo *dmxScreen,
		  unsigned long sequence)
{
    while (dmxScreen->ignore.head)
    {
	if ((long) (sequence - dmxScreen->ignore.head->sequence) > 0)
	{
	    DMXSequence *next = dmxScreen->ignore.head->next;

	    free (dmxScreen->ignore.head);

	    dmxScreen->ignore.head = next;
	    if (!dmxScreen->ignore.head)
		dmxScreen->ignore.tail = &dmxScreen->ignore.head;
	}
	else
	    break;
    }
}

static Bool
dmxShouldIgnore (DMXScreenInfo *dmxScreen,
		 unsigned long sequence)
{
    dmxDiscardIgnore (dmxScreen, sequence);

    if (!dmxScreen->ignore.head)
	return FALSE;

    return dmxScreen->ignore.head->sequence == sequence;
}

static Bool
dmxScreenEventCheckExpose (ScreenPtr           pScreen,
			   xcb_generic_event_t *event)
{
    DMXScreenInfo      *dmxScreen = &dmxScreens[pScreen->myNum];
    xcb_expose_event_t *xexpose = (xcb_expose_event_t *) event;
    WindowPtr          pChild0, pChildN;

    if ((event->response_type & ~0x80) != XCB_EXPOSE)
	return FALSE;

    if (dmxShouldIgnore (dmxScreen, xexpose->sequence))
	return TRUE;

    pChild0 = WindowTable[0];
    pChildN = WindowTable[pScreen->myNum];

    for (;;)
    {
	dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV (pChildN);

	if (pWinPriv->window == xexpose->window)
	    break;

	if (pChild0->firstChild)
	{
	    assert (pChildN->firstChild);
	    pChild0 = pChild0->firstChild;
	    pChildN = pChildN->firstChild;
	    continue;
	}

	while (!pChild0->nextSib && (pChild0 != WindowTable[0]))
	{
	    assert (!pChildN->nextSib &&
		    (pChildN != WindowTable[pScreen->myNum]));
	    pChild0 = pChild0->parent;
	    pChildN = pChildN->parent;
	}

	if (pChild0 == WindowTable[0])
	{
	    assert (pChildN == WindowTable[pScreen->myNum]);
	    break;
	}

	pChild0 = pChild0->nextSib;
	pChildN = pChildN->nextSib;
    }

    if (pChild0)
    {
	RegionRec region;
	BoxRec    box;

	box.x1 = pChild0->drawable.x + xexpose->x;
	box.y1 = pChild0->drawable.y + xexpose->y;
	box.x2 = box.x1 + xexpose->width;
	box.y2 = box.y1 + xexpose->height;

	REGION_INIT (screenInfo.screens[0], &region, &box, 1);
	(*pScreen->WindowExposures) (pChild0, &region, NullRegion);
	REGION_UNINIT (screenInfo.screens[0], &region);
    }

    return TRUE;
}

static Bool
dmxScreenEventCheckOutputWindow (ScreenPtr	     pScreen,
				 xcb_generic_event_t *event)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    switch (event->response_type & ~0x80) {
    case XCB_DESTROY_NOTIFY: {
	xcb_destroy_notify_event_t *xdestroy =
	    (xcb_destroy_notify_event_t *) event;
	
	if (xdestroy->window != dmxScreen->scrnWin)
	    return FALSE;

	/* output window has been destroyed, detach screen when we reach
	   the block handler */
	dmxScreen->scrnWin = None;
	return TRUE;
    } break;
    case XCB_MAP_NOTIFY: {
	xcb_map_notify_event_t *xmap = (xcb_map_notify_event_t *) event;

	if (xmap->window == dmxScreen->scrnWin)
	    return TRUE;
    } break;
    default:
	break;
    }

    return FALSE;
}

static Bool
dmxScreenEventCheckManageRoot (ScreenPtr	   pScreen,
			       xcb_generic_event_t *event)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    WindowPtr     pChild = WindowTable[pScreen->myNum];
    Window        xWindow;

#ifdef PANORAMIX
    PanoramiXRes  *win = NULL;
    WindowPtr     pWin;
#endif

    xcb_circulate_request_event_t *xcirculaterequest =
	(xcb_circulate_request_event_t *) event;
    xcb_configure_request_event_t *xconfigurerequest =
	(xcb_configure_request_event_t *) event;
    xcb_map_request_event_t *xmaprequest =
	(xcb_map_request_event_t *) event;
    xcb_client_message_event_t *xclient =
	(xcb_client_message_event_t *) event;
    xcb_map_notify_event_t *xmap =
	(xcb_map_notify_event_t *) event;

    switch (event->response_type & ~0x80) {
    case XCB_CIRCULATE_REQUEST:
	xWindow = xcirculaterequest->window;
	break;
    case XCB_CONFIGURE_REQUEST:
	xWindow = xconfigurerequest->window;
	break;
    case XCB_MAP_REQUEST:
	xWindow = xmaprequest->window;
	break;
    case XCB_CLIENT_MESSAGE:
	xWindow = xclient->window;
	break;
    case XCB_MAP_NOTIFY:
	if (xmap->window == dmxScreen->rootWin)
	{
	    dmxScreenGetSelectionOwner (pScreen);
	    return TRUE;
	}

	/* fall-through */
    default:
	return FALSE;
    }

    for (;;)
    {
	dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV (pChild);

	if (pWinPriv->window == xWindow)
	{

#ifdef PANORAMIX
	    if (!noPanoramiXExtension)
		win = PanoramiXFindIDByScrnum (XRT_WINDOW,
					       pChild->drawable.id,
					       pScreen->myNum);
#endif

	    break;
	}

	if (pChild->firstChild)
	{
	    pChild = pChild->firstChild;
	    continue;
	}

	while (!pChild->nextSib &&
	       (pChild != WindowTable[pScreen->myNum]))
	    pChild = pChild->parent;

	if (pChild == WindowTable[pScreen->myNum])
	    break;

	pChild = pChild->nextSib;
    }

    if (pChild

#ifdef PANORAMIX
	&& win
#endif

	)
    {
	XID    vlist[8];
	Atom   type;
	int    mask, i = 0;
	int    status = Success;
	xEvent x;

	switch (event->response_type & ~0x80) {
	case XCB_CIRCULATE_REQUEST:
	    vlist[0] = None;

	    if (xcirculaterequest->place == XCB_PLACE_ON_TOP)
		vlist[1] = Above;
	    else
		vlist[1] = Below;

#ifdef PANORAMIX
	    if (!noPanoramiXExtension)
	    {
		int j;

		FOR_NSCREENS_FORWARD(j) {
		    if (dixLookupWindow (&pWin,
					 win->info[j].id,
					 serverClient,
					 DixReadAccess) == Success)
			status |= ConfigureWindow (pWin,
						   CWSibling |
						   CWStackMode,
						   vlist,
						   serverClient);
		}
	    }
	    else
#endif
		status = ConfigureWindow (pChild,
					  CWSibling | CWStackMode,
					  vlist,
					  serverClient);
	    break;
	case XCB_CONFIGURE_REQUEST:
	    mask = xconfigurerequest->value_mask;

	    if (mask & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y))
	    {
		vlist[i++] = xconfigurerequest->x;
		vlist[i++] = xconfigurerequest->y;
	    }

	    if (mask & (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT))
	    {
		vlist[i++] = xconfigurerequest->width;
		vlist[i++] = xconfigurerequest->height;
	    }

	    if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
		vlist[i++] = xconfigurerequest->border_width;

	    if (mask & XCB_CONFIG_WINDOW_SIBLING)
	    {
		/* ignore stacking requests with sibling */
		if (xconfigurerequest->sibling == None)
		    vlist[i++] = None;
		else
		    mask &= ~(XCB_CONFIG_WINDOW_SIBLING |
			      XCB_CONFIG_WINDOW_STACK_MODE);
	    }

	    if (mask & XCB_CONFIG_WINDOW_STACK_MODE)
		vlist[i++] = xconfigurerequest->stack_mode;

#ifdef PANORAMIX
	    if (!noPanoramiXExtension)
	    {
		int j;

		FOR_NSCREENS_FORWARD(j) {
		    if (dixLookupWindow (&pWin,
					 win->info[j].id,
					 serverClient,
					 DixReadAccess) == Success)
			status |= ConfigureWindow (pWin,
						   mask,
						   vlist,
						   serverClient);
		}
	    }
	    else
#endif

		status = ConfigureWindow (pChild,
					  mask,
					  vlist,
					  serverClient);
	    break;
	case XCB_MAP_REQUEST:

#ifdef PANORAMIX
	    if (!noPanoramiXExtension)
	    {
		int j;

		FOR_NSCREENS_FORWARD(j) {
		    if (dixLookupWindow (&pWin,
					 win->info[j].id,
					 serverClient,
					 DixReadAccess) == Success)
			status |= MapWindow (pWin, serverClient);
		}
	    }
	    else
#endif

		status = MapWindow (pChild, serverClient);
	    break;
	case XCB_CLIENT_MESSAGE:
	    x.u.u.type               = ClientMessage | 0x80;
	    x.u.u.detail             = xclient->format;
	    x.u.clientMessage.window = pChild->drawable.id;

	    type = dmxAtom (dmxScreen, xclient->type);

	    switch (xclient->format) {
	    case 8:
		x.u.clientMessage.u.b.type = type;

		for (i = 0; i < 20; i++)
		    x.u.clientMessage.u.b.bytes[i] = xclient->data.data8[i];
		break;
	    case 16:
		x.u.clientMessage.u.s.type = type;

		x.u.clientMessage.u.s.shorts0 = xclient->data.data16[0];
		x.u.clientMessage.u.s.shorts1 = xclient->data.data16[1];
		x.u.clientMessage.u.s.shorts2 = xclient->data.data16[2];
		x.u.clientMessage.u.s.shorts3 = xclient->data.data16[3];
		x.u.clientMessage.u.s.shorts4 = xclient->data.data16[4];
		x.u.clientMessage.u.s.shorts5 = xclient->data.data16[5];
		x.u.clientMessage.u.s.shorts6 = xclient->data.data16[6];
		x.u.clientMessage.u.s.shorts7 = xclient->data.data16[7];
		x.u.clientMessage.u.s.shorts8 = xclient->data.data16[8];
		x.u.clientMessage.u.s.shorts9 = xclient->data.data16[9];
		break;
	    case 32:
		x.u.clientMessage.u.l.type = type;

		x.u.clientMessage.u.l.longs0 = xclient->data.data32[0];
		x.u.clientMessage.u.l.longs1 = xclient->data.data32[1];
		x.u.clientMessage.u.l.longs2 = xclient->data.data32[2];
		x.u.clientMessage.u.l.longs3 = xclient->data.data32[3];
		x.u.clientMessage.u.l.longs4 = xclient->data.data32[4];
		break;
	    }

	    /* client messages are always forwarded to the root
	       window as there's no way for us to know which
	       windows they were originally intended for */
	    pWin = WindowTable[pScreen->myNum];

#ifdef PANORAMIX
	    if (!noPanoramiXExtension)
	    {
		x.u.clientMessage.window = win->info[0].id;
		pWin = WindowTable[0];
	    }
#endif

	    DeliverEventsToWindow (PickPointer (serverClient),
				   pWin,
				   &x,
				   1,
				   SubstructureRedirectMask |
				   SubstructureNotifyMask,
				   NullGrab, 0);
	    break;
	}

	if (status != Success)
	    dmxLog (dmxWarning,
		    "dmxScreenManage: failed to handle "
		    "request type %d\n",
		    event->response_type & ~0x80);
    }
    else
    {
	XWindowChanges xwc;

	switch (event->response_type & ~0x80) {
	case XCB_CIRCULATE_REQUEST:
	    XLIB_PROLOGUE (dmxScreen);
	    if (xcirculaterequest->place == XCB_PLACE_ON_TOP)
		XRaiseWindow (dmxScreen->beDisplay,
			      xcirculaterequest->window);
	    else
		XLowerWindow (dmxScreen->beDisplay,
			      xcirculaterequest->window);
	    XLIB_EPILOGUE (dmxScreen);
	    break;
	case XCB_CONFIGURE_REQUEST:
	    xwc.x	     = xconfigurerequest->x;
	    xwc.y	     = xconfigurerequest->y;
	    xwc.width	     = xconfigurerequest->width;
	    xwc.height	     = xconfigurerequest->height;
	    xwc.border_width = xconfigurerequest->border_width;
	    xwc.sibling      = xconfigurerequest->sibling;
	    xwc.stack_mode   = xconfigurerequest->stack_mode;

	    XLIB_PROLOGUE (dmxScreen);
	    XConfigureWindow (dmxScreen->beDisplay,
			      xconfigurerequest->window,
			      xconfigurerequest->value_mask,
			      &xwc);
	    XLIB_EPILOGUE (dmxScreen);
	    break;
	case XCB_MAP_REQUEST:
	    XLIB_PROLOGUE (dmxScreen);
	    XMapWindow (dmxScreen->beDisplay, xmaprequest->window);
	    XLIB_EPILOGUE (dmxScreen);
	    break;
	case XCB_CLIENT_MESSAGE:
	    break;
	}
    }

    return TRUE;
}

static Bool
dmxScreenEventCheckIgnore (ScreenPtr	       pScreen,
			   xcb_generic_event_t *event)
{
    switch (event->response_type & ~0x80) {
    case XCB_MAPPING_NOTIFY:
	return TRUE;
    default:
	break;
    }

    return FALSE;
}

void
dmxBEDispatch (ScreenPtr pScreen)
{
    DMXScreenInfo       *dmxScreen = &dmxScreens[pScreen->myNum];
    xcb_generic_event_t *event;
    xcb_generic_error_t *error;
    void                *reply;

    assert (dmxScreen->inDispatch == FALSE);
    dmxScreen->inDispatch = TRUE;

    while ((event = xcb_poll_for_event (dmxScreen->connection)))
    {
	if (!dmxScreenEventCheckInput (pScreen, event)        &&
	    !dmxScreenEventCheckSelection (pScreen, event)    &&
	    !dmxScreenEventCheckDnD (pScreen, event)          &&
	    !dmxScreenEventCheckOutputWindow (pScreen, event) &&
	    !dmxScreenEventCheckManageRoot (pScreen, event)   &&
	    !dmxScreenEventCheckExpose (pScreen, event)       &&

#ifdef MITSHM
	    !dmxScreenEventCheckShm (pScreen, event)          &&
#endif

#ifdef RANDR
	    !dmxScreenEventCheckRR (pScreen, event)           &&
#endif

	    !dmxScreenEventCheckIgnore (pScreen, event))
	{
	    if (event->response_type == 0)
	    {
		xcb_generic_error_t *error = (xcb_generic_error_t *) event;

		dmxLogOutput (dmxScreen, "unhandled error type %d\n",
			      error->error_code);
	    }
	    else
	    {
		dmxLogOutput (dmxScreen, "unhandled event type %d\n",
			      event->response_type);
	    }
	}

	free (event);
    }

    while (dmxScreen->request.head &&
	   xcb_poll_for_reply (dmxScreen->connection,
			       dmxScreen->request.head->sequence,
			       (void **) &reply,
			       &error))
    {
	DMXRequest *head = (DMXRequest *) dmxScreen->request.head;

	dmxScreen->request.head = head->base.next;
	if (!dmxScreen->request.head)
	    dmxScreen->request.tail = &dmxScreen->request.head;

	(*head->reply) (pScreen,
			head->base.sequence,
			reply,
			error,
			head->data);

        if (reply)
	    free (reply);
        if (error)
            free (error);

	free (head);
    }

    if (!dmxScreen->scrnWin ||
	xcb_connection_has_error (dmxScreen->connection))
    {
	static xcb_generic_error_t detached_error = { 0, DMX_DETACHED };

	while (dmxScreen->request.head)
	{
	    DMXRequest *head = (DMXRequest *) dmxScreen->request.head;

	    dmxScreen->request.head = head->base.next;
	    if (!dmxScreen->request.head)
		dmxScreen->request.tail = &dmxScreen->request.head;

	    (*head->reply) (pScreen,
			    head->base.sequence,
			    NULL,
			    &detached_error,
			    head->data);

	    free (head);
	}

	dmxScreen->broken = TRUE;
    }

    dmxScreen->inDispatch = FALSE;
}

static void
dmxScreenCheckForIOError (ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (!dmxScreen->scrnWin ||
	xcb_connection_has_error (dmxScreen->connection))
    {
	int i;

	if (dmxScreen->scrnWin)
	{
	    dmxLogOutput (dmxScreen, "Detected broken connection\n");
	    dmxScreen->alive = FALSE;
	}

	if (!dmxScreen->broken)
	    dmxBEDispatch (pScreen);

	dmxDetachScreen (pScreen->myNum);

	for (i = 0; i < dmxNumScreens; i++)
	    if (i != pScreen->myNum && dmxScreens[i].beDisplay)
		break;

	if (i == dmxNumScreens)
	    dmxLog (dmxFatal, "No back-end server connection, "
		    "giving up\n");
    }
}

static void
dmxScreenBlockHandler (pointer   blockData,
		       OSTimePtr pTimeout,
		       pointer   pReadMask)
{
    ScreenPtr     pScreen = (ScreenPtr) blockData;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (dmxScreen->alive)
    {
	xcb_flush (dmxScreen->connection);
	dmxScreenCheckForIOError (pScreen);
    }
}

static void
dmxScreenWakeupHandler (pointer blockData,
			int     result,
			pointer pReadMask)
{
    ScreenPtr     pScreen = (ScreenPtr) blockData;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (dmxScreen->alive)
	dmxBEDispatch (pScreen);
}

static void
dmxHandleExposures (WindowPtr pWin)
{
    WindowPtr pChild;
    ValidatePtr val;
    ScreenPtr pScreen;
    WindowExposuresProcPtr WindowExposures;

    pScreen = pWin->drawable.pScreen;

    pChild = pWin;
    WindowExposures = pChild->drawable.pScreen->WindowExposures;
    while (1)
    {
	if ( (val = pChild->valdata) )
	{
	    REGION_UNINIT(pScreen, &val->after.borderExposed);
	    (*WindowExposures)(pChild, &val->after.exposed, NullRegion);
	    REGION_UNINIT(pScreen, &val->after.exposed);
	    xfree(val);
	    pChild->valdata = (ValidatePtr)NULL;
	    if (pChild->firstChild)
	    {
		pChild = pChild->firstChild;
		continue;
	    }
	}
	while (!pChild->nextSib && (pChild != pWin))
	    pChild = pChild->parent;
	if (pChild == pWin)
	    break;
	pChild = pChild->nextSib;
    }
}

/** Initialize screen number \a idx. */
Bool dmxScreenInit(int idx, ScreenPtr pScreen, int argc, char *argv[])
{
    DMXScreenInfo        *dmxScreen = &dmxScreens[idx];
    int                   i, j;

    if (dmxGeneration != serverGeneration) {
	/* Allocate font private index */
	dmxFontPrivateIndex = AllocateFontPrivateIndex();
	if (dmxFontPrivateIndex == -1)
	    return FALSE;

	dmxGeneration = serverGeneration;
    }

    dmxInputInit (&dmxScreen->input);

    dmxScreen->ignore.head = NULL;
    dmxScreen->ignore.tail = &dmxScreen->ignore.head;

    dmxScreen->request.head = NULL;
    dmxScreen->request.tail = &dmxScreen->request.head;

    dmxScreen->scrnEventMask = 0;

    dmxScreen->rootEventMask = ExposureMask | StructureNotifyMask |
	SubstructureRedirectMask;

    dmxScreen->multipleAtom = MakeAtom ("MULTIPLE", strlen ("MULTIPLE"), TRUE);
    dmxScreen->atomPairAtom = MakeAtom ("ATOM_PAIR", strlen ("ATOM_PAIR"), TRUE);
    dmxScreen->incrAtom = MakeAtom ("INCR", strlen ("INCR"), TRUE);

    if (!dmxInitGC(pScreen)) return FALSE;
    if (!dmxInitWindow(pScreen)) return FALSE;
    if (!dmxInitPixmap(pScreen)) return FALSE;
    if (!dmxInitCursor(pScreen)) return FALSE;

    /*
     * Initalise the visual types.  miSetVisualTypesAndMasks() requires
     * that all of the types for each depth be collected together.  It's
     * intended for slightly different usage to what we would like here.
     * Maybe a miAddVisualTypeAndMask() function will be added to make
     * things easier here.
     */
    if (dmxScreen->beAttachedDisplay)
    {
	for (i = 0; i < dmxScreen->beNumDepths; i++) {
	    int    depth;
	    int    visuals        = 0;
	    int    bitsPerRgb     = 0;
	    int    preferredClass = -1;
	    Pixel  redMask        = 0;
	    Pixel  greenMask      = 0;
	    Pixel  blueMask       = 0;

	    depth = dmxScreen->beDepths[i];
	    for (j = 0; j < dmxScreen->beNumVisuals; j++) {
		XVisualInfo *vi;

		vi = &dmxScreen->beVisuals[j];
		if (vi->depth == depth) {
		    /* Assume the masks are all the same. */
		    visuals |= (1 << vi->class);
		    bitsPerRgb = vi->bits_per_rgb;
		    redMask = vi->red_mask;
		    greenMask = vi->green_mask;
		    blueMask = vi->blue_mask;
		    if (j == dmxScreen->beDefVisualIndex) {
			preferredClass = vi->class;
		    }
		}
	    }

#ifdef PANORAMIX
	    if (!noPanoramiXExtension)
	    {
		/* avoid additional DirectColor visuals for better
		   back-end server support */
		if (preferredClass != DirectColor)
		    visuals &= ~(1 << DirectColor);
	    }
#endif

	    miSetVisualTypesAndMasks(depth, visuals, bitsPerRgb,
				     preferredClass,
				     redMask, greenMask, blueMask);
	}
    }
    else
    {
	for (i = 0; i < dmxScreens[0].beNumDepths; i++) {
	    int    depth;
	    int    visuals        = 0;
	    int    bitsPerRgb     = 0;
	    int    preferredClass = -1;
	    Pixel  redMask        = 0;
	    Pixel  greenMask      = 0;
	    Pixel  blueMask       = 0;

	    depth = dmxScreens[0].beDepths[i];
	    for (j = 0; j < dmxScreens[0].beNumVisuals; j++) {
		XVisualInfo *vi;

		vi = &dmxScreens[0].beVisuals[j];
		if (vi->depth == depth) {
		    /* Assume the masks are all the same. */
		    visuals |= (1 << vi->class);
		    bitsPerRgb = vi->bits_per_rgb;
		    redMask = vi->red_mask;
		    greenMask = vi->green_mask;
		    blueMask = vi->blue_mask;
		    if (j == dmxScreens[0].beDefVisualIndex) {
			preferredClass = vi->class;
		    }
		}
	    }

#ifdef PANORAMIX
	    if (!noPanoramiXExtension)
	    {
		/* avoid additional DirectColor visuals for better
		   back-end server support */
		if (preferredClass != DirectColor)
		    visuals &= ~(1 << DirectColor);
	    }
#endif

	    miSetVisualTypesAndMasks(depth, visuals, bitsPerRgb,
				     preferredClass,
				     redMask, greenMask, blueMask);
	}
    }

    fbScreenInit(pScreen,
		 NULL,
		 dmxScreen->scrnWidth,
		 dmxScreen->scrnHeight,
		 dmxScreen->beXDPI,
		 dmxScreen->beYDPI,
		 dmxScreen->scrnWidth,
		 dmxScreen->beBPP);

    if (!dmxScreen->scrnWin && dmxScreen->beDisplay)
    {
	dmxScreen->scrnWin = DefaultRootWindow (dmxScreen->beDisplay);
	dmxScreen->scrnEventMask |= StructureNotifyMask;
	XSelectInput (dmxScreen->beDisplay,
		      dmxScreen->scrnWin,
		      dmxScreen->scrnEventMask);
    }

#ifdef MITSHM
    ShmRegisterDmxFuncs (pScreen);
#endif

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	for (i = 0; i < pScreen->numVisuals; i++)
	    if (pScreen->visuals[i].ColormapEntries > 256)
		pScreen->visuals[i].ColormapEntries = 256;
    }
#endif

#ifdef RENDER
    if (!dmxPictureInit (pScreen, 0, 0))
	return FALSE;
#endif

#ifdef RANDR
    if (!dmxRRScreenInit (pScreen))
	return FALSE;
#endif

#ifdef XV
    if (!dmxXvScreenInit (pScreen))
	return FALSE;
#endif

    if (!dmxDnDScreenInit (pScreen))
	return FALSE;

    miInitializeBackingStore(pScreen);

    MAXSCREENSALLOC(dmxCursorGeneration);
    if (dmxCursorGeneration[idx] != serverGeneration) {
	if (!(miPointerInitialize(pScreen,
				  &dmxPointerSpriteFuncs,
				  &dmxPointerCursorFuncs,
				  FALSE)))
	    return FALSE;

	dmxCursorGeneration[idx] = serverGeneration;
    }

    DMX_WRAP(CloseScreen, dmxCloseScreen, dmxScreen, pScreen);
    DMX_WRAP(SaveScreen, dmxSaveScreen, dmxScreen, pScreen);

    /* Wrap GC functions */
    DMX_WRAP(CreateGC, dmxCreateGC, dmxScreen, pScreen);

    /* Wrap Window functions */
    DMX_WRAP(CreateWindow, dmxCreateWindow, dmxScreen, pScreen);
    DMX_WRAP(DestroyWindow, dmxDestroyWindow, dmxScreen, pScreen);
    DMX_WRAP(PositionWindow, dmxPositionWindow, dmxScreen, pScreen);
    DMX_WRAP(ChangeWindowAttributes, dmxChangeWindowAttributes, dmxScreen,
	     pScreen);
    DMX_WRAP(RealizeWindow, dmxRealizeWindow, dmxScreen, pScreen);
    DMX_WRAP(UnrealizeWindow, dmxUnrealizeWindow, dmxScreen, pScreen);
    DMX_WRAP(RestackWindow, dmxRestackWindow, dmxScreen, pScreen);
    DMX_WRAP(CopyWindow, dmxCopyWindow, dmxScreen, pScreen);

    DMX_WRAP(ResizeWindow, dmxResizeWindow, dmxScreen, pScreen);
    DMX_WRAP(HandleExposures, dmxHandleExposures, dmxScreen, pScreen);
    DMX_WRAP(ReparentWindow, dmxReparentWindow, dmxScreen, pScreen);

    DMX_WRAP(ChangeBorderWidth, dmxChangeBorderWidth, dmxScreen, pScreen);

    DMX_WRAP(ModifyPixmapHeader, dmxModifyPixmapHeader, dmxScreen, pScreen);

    DMX_WRAP(SetWindowPixmap, dmxSetWindowPixmap, dmxScreen, pScreen);

    /* Wrap Image functions */
    DMX_WRAP(GetImage, dmxGetImage, dmxScreen, pScreen);
    DMX_WRAP(GetSpans, NULL, dmxScreen, pScreen);

    /* Wrap Pixmap functions */
    DMX_WRAP(CreatePixmap, dmxCreatePixmap, dmxScreen, pScreen);
    DMX_WRAP(DestroyPixmap, dmxDestroyPixmap, dmxScreen, pScreen);
    DMX_WRAP(BitmapToRegion, dmxBitmapToRegion, dmxScreen, pScreen);

    /* Wrap Font functions */
    DMX_WRAP(RealizeFont, dmxRealizeFont, dmxScreen, pScreen);
    DMX_WRAP(UnrealizeFont, dmxUnrealizeFont, dmxScreen, pScreen);

    /* Wrap Colormap functions */
    DMX_WRAP(CreateColormap, dmxCreateColormap, dmxScreen, pScreen);
    DMX_WRAP(DestroyColormap, dmxDestroyColormap, dmxScreen, pScreen);
    DMX_WRAP(InstallColormap, dmxInstallColormap, dmxScreen, pScreen);
    DMX_WRAP(StoreColors, dmxStoreColors, dmxScreen, pScreen);

    /* Wrap Shape functions */
    DMX_WRAP(SetShape, dmxSetShape, dmxScreen, pScreen);

    if (!dmxCreateDefColormap(pScreen))
	return FALSE;

    RegisterBlockAndWakeupHandlers (dmxScreenBlockHandler,
				    dmxScreenWakeupHandler,
				    pScreen);

    return TRUE;
}

/** Close the \a pScreen resources on the back-end server. */
void dmxBECloseScreen(ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    int            i;

#ifdef XV
    dmxBEXvScreenFini (pScreen);
#endif

#ifdef RANDR
    dmxBERRScreenFini (pScreen);
#endif

    /* Restore the back-end screen-saver and DPMS state. */
    dmxBEDPMSScreenFini (pScreen);

    /* Free the screen resources */
    dmxScreen->scrnWin = (Window)0;

    /* Free the default drawables */
    for (i = 0; i < dmxScreen->beNumPixmapFormats; i++) {
	XLIB_PROLOGUE (dmxScreen);
	XFreePixmap(dmxScreen->beDisplay, dmxScreen->scrnDefDrawables[i]);
	XLIB_EPILOGUE (dmxScreen);
	dmxScreen->scrnDefDrawables[i] = (Drawable)0;
    }

    /* Free resources allocated during initialization (in dmxinit.c) */
    for (i = 0; i < dmxScreen->beNumDefColormaps; i++)
    {
	XLIB_PROLOGUE (dmxScreen);
	XFreeColormap(dmxScreen->beDisplay, dmxScreen->beDefColormaps[i]);
	XLIB_EPILOGUE (dmxScreen);
    }
    xfree(dmxScreen->beDefColormaps);
    dmxScreen->beDefColormaps = NULL;

#ifdef GLXEXT
    if (dmxScreen->glxVisuals) {
	XFree(dmxScreen->glxVisuals);
	dmxScreen->glxVisuals = NULL;
	dmxScreen->numGlxVisuals = 0;
    }
#endif

    dmxClearQueue (&dmxScreen->request);
    dmxClearQueue (&dmxScreen->ignore);
}

/** Close screen number \a idx. */
Bool dmxCloseScreen(int idx, ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[idx];

        /* Free input overlay window tree */
    if (dmxScreen->inputOverlayWid)
	FreeResource (dmxScreen->inputOverlayWid, RT_NONE);

    /* Reset the proc vectors */
    if (idx == 0) {
#ifdef COMPOSITE
	if (!noCompositeExtension)
	    dmxResetComposite();
#endif
#ifdef RENDER
	if (!noRenderExtension)
	    dmxResetRender();
#endif
#ifdef MITSHM
	dmxResetShm();
#endif
	dmxResetSelections();
	dmxResetGrabs();
	dmxResetProps();
    }

    /* Unwrap Shape functions */
    DMX_UNWRAP(SetShape, dmxScreen, pScreen);

    /* Unwrap the pScreen functions */
    DMX_UNWRAP(CreateGC, dmxScreen, pScreen);

    DMX_UNWRAP(CreateWindow, dmxScreen, pScreen);
    DMX_UNWRAP(DestroyWindow, dmxScreen, pScreen);
    DMX_UNWRAP(PositionWindow, dmxScreen, pScreen);
    DMX_UNWRAP(ChangeWindowAttributes, dmxScreen, pScreen);
    DMX_UNWRAP(RealizeWindow, dmxScreen, pScreen);
    DMX_UNWRAP(UnrealizeWindow, dmxScreen, pScreen);
    DMX_UNWRAP(RestackWindow, dmxScreen, pScreen);
    DMX_UNWRAP(CopyWindow, dmxScreen, pScreen);

    DMX_UNWRAP(ResizeWindow, dmxScreen, pScreen);
    DMX_UNWRAP(HandleExposures, dmxScreen, pScreen);
    DMX_UNWRAP(ReparentWindow, dmxScreen, pScreen);

    DMX_UNWRAP(ChangeBorderWidth, dmxScreen, pScreen);

    DMX_UNWRAP(ModifyPixmapHeader, dmxScreen, pScreen);

    DMX_UNWRAP(SetWindowPixmap, dmxScreen, pScreen);

    DMX_UNWRAP(GetImage, dmxScreen, pScreen);
    DMX_UNWRAP(GetSpans, dmxScreen, pScreen);

    DMX_UNWRAP(CreatePixmap, dmxScreen, pScreen);
    DMX_UNWRAP(DestroyPixmap, dmxScreen, pScreen);
    DMX_UNWRAP(BitmapToRegion, dmxScreen, pScreen);

    DMX_UNWRAP(RealizeFont, dmxScreen, pScreen);
    DMX_UNWRAP(UnrealizeFont, dmxScreen, pScreen);

    DMX_UNWRAP(CreateColormap, dmxScreen, pScreen);
    DMX_UNWRAP(DestroyColormap, dmxScreen, pScreen);
    DMX_UNWRAP(InstallColormap, dmxScreen, pScreen);
    DMX_UNWRAP(StoreColors, dmxScreen, pScreen);

    DMX_UNWRAP(SaveScreen, dmxScreen, pScreen);

    if (dmxScreen->beDisplay) {
	dmxBECloseScreen(pScreen);

	/* Close display */
	dmxCloseDisplay (dmxScreen);
	dmxScreen->beDisplay = NULL;

	/* Free visuals, depths and pixmap formats here so that they
	 * won't be freed when a screen is detached, thereby allowing
	 * the screen to be reattached to be compared to the one
	 * previously removed.
	 */
	XFree(dmxScreen->beVisuals);
	dmxScreen->beVisuals = NULL;

	XFree(dmxScreen->beDepths);
	dmxScreen->beDepths = NULL;

	XFree(dmxScreen->bePixmapFormats);
	dmxScreen->bePixmapFormats = NULL;
    }

    if (idx == 0)
	dmxAbortDisplay ();

    DMX_UNWRAP(CloseScreen, dmxScreen, pScreen);
    return pScreen->CloseScreen(idx, pScreen);
}

static Bool dmxSaveScreen(ScreenPtr pScreen, int what)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (dmxScreen->beDisplay) {
	if (dmxScreen->scrnWin != DefaultRootWindow (dmxScreen->beDisplay))
	    return TRUE;
	switch (what) {
	case SCREEN_SAVER_OFF:
	case SCREEN_SAVER_FORCER:
	    XLIB_PROLOGUE (dmxScreen);
	    XResetScreenSaver(dmxScreen->beDisplay);
	    XLIB_EPILOGUE (dmxScreen);
	    dmxSync(dmxScreen, FALSE);
	    break;
	case SCREEN_SAVER_ON:
	case SCREEN_SAVER_CYCLE:
	    XLIB_PROLOGUE (dmxScreen);
	    XActivateScreenSaver(dmxScreen->beDisplay);
	    XLIB_EPILOGUE (dmxScreen);
	    dmxSync(dmxScreen, FALSE);
	    break;
	}
    }

    return TRUE;
}

Bool
dmxAddSequence (DMXQueue      *q,
		unsigned long sequence)
{
    DMXSequence *s;

    s = malloc (sizeof (DMXSequence));
    if (!s)
	return FALSE;

    s->sequence = sequence;
    s->next     = 0;

    *(q->tail) = s;
    q->tail = &s->next;

    return TRUE;
}

void
dmxClearQueue (DMXQueue *q)
{
    while (q->head)
    {
	DMXSequence *head = q->head;

	q->head = head->next;
	free (head);
    }

    q->tail = &q->head;
}

Bool
dmxAddRequest (DMXQueue      *q,
	       ReplyProcPtr  reply,
	       unsigned long sequence,
	       void          *data)
{
    DMXRequest *r;

    r = malloc (sizeof (DMXRequest));
    if (!r)
	return FALSE;

    r->base.sequence = sequence;
    r->base.next     = 0;
    r->reply         = reply;
    r->data          = data;

    *(q->tail) = &r->base;
    q->tail = &r->base.next;

    return TRUE;
}

