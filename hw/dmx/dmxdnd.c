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
#include "dmxatom.h"
#include "dmxwindow.h"
#include "dmxscrinit.h"
#include "dmxsync.h"
#include "dmxinput.h"
#include "dmxselection.h"
#include "dmxdnd.h"

#include "selection.h"

#ifdef PANORAMIX
#include "panoramiX.h"
#include "panoramiXsrv.h"
#endif

#include <xcb/xinput.h>

struct _DMXDnDChild {
    Window target;
    Window wid;
    BoxRec box;
    int    map_state;
    int    version;
};

void
dmxBEDnDRootWindowUpdate (ScreenPtr pScreen,
			  Window    window)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    xcb_atom_t    version = 5;

    xcb_change_property (dmxScreen->connection,
			 XCB_PROP_MODE_REPLACE,
			 window,
			 dmxBEAtom (dmxScreen, dmxScreen->xdndAwareAtom),
			 XA_ATOM,
			 32,
			 1,
			 &version);
    xcb_change_property (dmxScreen->connection,
			 XCB_PROP_MODE_REPLACE,
			 window,
			 dmxBEAtom (dmxScreen, dmxScreen->xdndProxyAtom),
			 XA_WINDOW,
			 32,
			 1,
			 &window);
}

static void
dmxDnDSendDeclineStatus (void)
{
    WindowPtr pWin;

    if (!dmxScreens[0].dndWindow)
	return;

    if (dixLookupWindow (&pWin,
			 dmxScreens[0].dndWindow,
			 serverClient,
			 DixReadAccess) == Success)
    {
	xEvent x;

	x.u.u.type                   = ClientMessage | 0x80;
	x.u.u.detail                 = 32;
	x.u.clientMessage.window     = dmxScreens[0].dndWindow;
	x.u.clientMessage.u.l.type   = dmxScreens[0].xdndStatusAtom;
	x.u.clientMessage.u.l.longs0 = dmxScreens[0].selectionProxyWid[0];
	x.u.clientMessage.u.l.longs1 = 0;
	x.u.clientMessage.u.l.longs2 = 0;
	x.u.clientMessage.u.l.longs3 = 0;
	x.u.clientMessage.u.l.longs4 = 0;

	DeliverEventsToWindow (PickPointer (serverClient),
			       pWin,
			       &x,
			       1,
			       NoEventMask,
			       NullGrab, 0);
    }
}

static void
dmxBEDnDUpdateTarget (ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    Window        target = None;
    Window        wid = None;
    int           version = 0;

    if (!dmxScreens[0].dndWindow)
	return;

    if (dmxScreen->dndStatus)
    {
	int n;

	n = dmxScreen->dndNChildren;
	while (n--)
	{
	    if (dmxScreen->dndChildren[n].map_state != XCB_MAP_STATE_VIEWABLE)
		continue;

	    if (dmxScreen->dndChildren[n].box.x1 <= dmxScreen->dndX &&
		dmxScreen->dndChildren[n].box.y1 <= dmxScreen->dndY &&
		dmxScreen->dndChildren[n].box.x2 >  dmxScreen->dndX &&
		dmxScreen->dndChildren[n].box.y2 >  dmxScreen->dndY)
		break;
	}

	if (n >= 0 && dmxScreen->dndChildren[n].version >= 3)
	{
	    target  = dmxScreen->dndChildren[n].target;
	    wid     = dmxScreen->dndChildren[n].wid;
	    version = dmxScreen->dndChildren[n].version < 5 ?
		dmxScreen->dndChildren[n].version : 5;
	}
    }

    if (target != dmxScreen->dndTarget)
    {
	if (dmxScreen->dndTarget)
	{
	    xcb_client_message_event_t xevent;

	    xevent.response_type = XCB_CLIENT_MESSAGE;
	    xevent.format        = 32;

	    xevent.type   = dmxBEAtom (dmxScreen, dmxScreen->xdndLeaveAtom);
	    xevent.window = dmxScreen->dndTarget;

	    xevent.data.data32[0] = dmxScreen->dndSource;
	    xevent.data.data32[1] = 0;
	    xevent.data.data32[2] = 0;
	    xevent.data.data32[3] = 0;
	    xevent.data.data32[4] = 0;

	    xcb_send_event (dmxScreen->connection,
			    FALSE,
			    dmxScreen->dndWid,
			    0,
			    (const char *) &xevent);
	}

	if (target)
	{
	    xcb_client_message_event_t xevent;
	    int                        i;

	    xevent.response_type = XCB_CLIENT_MESSAGE;
	    xevent.format        = 32;

	    xevent.type   = dmxBEAtom (dmxScreen, dmxScreen->xdndEnterAtom);
	    xevent.window = target;

	    xevent.data.data32[0] = dmxScreen->dndSource;
	    xevent.data.data32[1] = version << 24;
	    xevent.data.data32[2] = 0;
	    xevent.data.data32[3] = 0;
	    xevent.data.data32[4] = 0;

	    if (dmxScreen->dndHasTypeProp)
		xevent.data.data32[1] |= 1;

	    for (i = 0; i < 3; i++)
		if (ValidAtom (dmxScreen->dndType[i]))
		    xevent.data.data32[i + 2] =
			dmxBEAtom (dmxScreen, dmxScreen->dndType[i]);

	    xcb_send_event (dmxScreen->connection,
			    FALSE,
			    wid,
			    0,
			    (const char *) &xevent);

	    dmxScreen->dndXPos = -1;
	    dmxScreen->dndYPos = -1;
	}
	else if (dmxScreen->dndStatus)
	{
	    dmxDnDSendDeclineStatus ();
	}

	dmxScreen->dndTarget = target;
	dmxScreen->dndWid    = wid;
    }

    if (dmxScreen->dndTarget)
    {
	if (dmxScreen->dndX != dmxScreen->dndXPos ||
	    dmxScreen->dndY != dmxScreen->dndYPos)
	{
	    xcb_client_message_event_t xevent;

	    xevent.response_type = XCB_CLIENT_MESSAGE;
	    xevent.format        = 32;

	    xevent.type   = dmxBEAtom (dmxScreen, dmxScreen->xdndPositionAtom);
	    xevent.window = dmxScreen->dndTarget;

	    xevent.data.data32[0] = dmxScreen->dndSource;
	    xevent.data.data32[1] = 0;
	    xevent.data.data32[2] = (dmxScreen->dndX << 16) | dmxScreen->dndY;
	    xevent.data.data32[3] = 0; /* XXX: need time stamp */
	    xevent.data.data32[4] = dmxBEAtom (dmxScreen,
					       dmxScreen->dndAction);

	    xcb_send_event (dmxScreen->connection,
			    FALSE,
			    dmxScreen->dndWid,
			    0,
			    (const char *) &xevent);

	    dmxScreen->dndXPos = dmxScreen->dndX;
	    dmxScreen->dndYPos = dmxScreen->dndY;
	}
    }
}

static void
dmxDnDAwarePropertyReply (ScreenPtr           pScreen,
			  unsigned int        sequence,
			  xcb_generic_reply_t *reply,
			  xcb_generic_error_t *error,
			  void                *data)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    int           n = (int) data;

     if (!dmxScreen->dndChildren || n >= dmxScreen->dndNChildren)
	return;

    if (reply)
    {
	xcb_get_property_reply_t *xproperty =
	    (xcb_get_property_reply_t *) reply;

	if (xproperty->format == 32)
	{
	    uint32_t *data = xcb_get_property_value (xproperty);
	    int      length = xcb_get_property_value_length (xproperty);

	    if (length)
		dmxScreen->dndChildren[n].version = *data;
	}
    }
}

static void
dmxDnDProxyPropertyReply (ScreenPtr           pScreen,
			  unsigned int        sequence,
			  xcb_generic_reply_t *reply,
			  xcb_generic_error_t *error,
			  void                *data)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    int           n = (int) data;

    if (!dmxScreen->dndChildren || n >= dmxScreen->dndNChildren)
	return;

    if (reply)
    {
	xcb_get_property_reply_t *xproperty =
	    (xcb_get_property_reply_t *) reply;

	if (xproperty->format == 32)
	{
	    uint32_t *data = xcb_get_property_value (xproperty);
	    int      length = xcb_get_property_value_length (xproperty);

	    if (length)
	    {
		xcb_get_property_cookie_t prop;

		dmxScreen->dndChildren[n].wid = *data;

		/* ignore previous xdndAware property reply */
		dmxScreen->dndChildren[n].version = 0;

		prop = xcb_get_property (dmxScreen->connection,
					 xFalse,
					 dmxScreen->dndChildren[n].wid,
					 dmxBEAtom (dmxScreen,
						    dmxScreen->xdndAwareAtom),
					 XCB_GET_PROPERTY_TYPE_ANY,
					 0,
					 1);
		dmxAddRequest (&dmxScreen->request,
			       dmxDnDAwarePropertyReply,
			       prop.sequence,
			       (void *) n);
	    }
	}
    }
}

static void
dmxDnDGeometryReply (ScreenPtr           pScreen,
		     unsigned int        sequence,
		     xcb_generic_reply_t *reply,
		     xcb_generic_error_t *error,
		     void                *data)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    int           n = (int) data;

    if (!dmxScreen->dndChildren || n >= dmxScreen->dndNChildren)
	return;

    if (reply)
    {
	xcb_get_geometry_reply_t *xgeometry =
	    (xcb_get_geometry_reply_t *) reply;

	dmxScreen->dndChildren[n].box.x1 = xgeometry->x;
	dmxScreen->dndChildren[n].box.y1 = xgeometry->y;
	dmxScreen->dndChildren[n].box.x2 = xgeometry->x + xgeometry->width;
	dmxScreen->dndChildren[n].box.y2 = xgeometry->y + xgeometry->height;
    }
}

static void
dmxDnDWindowAttributesReply (ScreenPtr           pScreen,
			     unsigned int        sequence,
			     xcb_generic_reply_t *reply,
			     xcb_generic_error_t *error,
			     void                *data)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    int           n = (int) data;

    if (!dmxScreen->dndChildren || n >= dmxScreen->dndNChildren)
	return;

    if (reply)
    {
	xcb_get_window_attributes_reply_t *xattrib =
	    (xcb_get_window_attributes_reply_t *) reply;

	dmxScreen->dndChildren[n].map_state = xattrib->map_state;

	if (xattrib->map_state == XCB_MAP_STATE_VIEWABLE)
	    dmxBEDnDUpdateTarget (pScreen);
    }
}

/* XXX: back-end server DND target lookup method is efficient but
   doesn't support reparenting window managers */
static void
dmxDnDQueryTreeReply (ScreenPtr           pScreen,
		      unsigned int        sequence,
		      xcb_generic_reply_t *reply,
		      xcb_generic_error_t *error,
		      void                *data)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (sequence != dmxScreen->queryTree.sequence)
	return;

    assert (!dmxScreen->dndChildren);

    if (reply)
    {
	xcb_query_tree_reply_t *xquery = (xcb_query_tree_reply_t *) reply;
	DMXDnDChild            *children;
	xcb_window_t           *c;
	int                    n;

	c = xcb_query_tree_children (xquery);
	n = xcb_query_tree_children_length (xquery);

	children = xalloc (n * sizeof (DMXDnDChild));
	if (!children)
	    return;

	dmxScreen->dndChildren  = children;
	dmxScreen->dndNChildren = n;

	while (n--)
	{
	    xcb_get_property_cookie_t          prop;
	    xcb_get_geometry_cookie_t          geometry;
	    xcb_get_window_attributes_cookie_t attr;

	    children[n].box.x1  = 0;
	    children[n].box.y1  = 0;
	    children[n].box.x2  = 0;
	    children[n].box.y2  = 0;
	    children[n].version = 0;
	    children[n].target  = c[n];
	    children[n].wid     = c[n];

	    prop = xcb_get_property (dmxScreen->connection,
				     xFalse,
				     c[n],
				     dmxBEAtom (dmxScreen,
						dmxScreen->xdndAwareAtom),
				     XCB_GET_PROPERTY_TYPE_ANY,
				     0,
				     1);
	    dmxAddRequest (&dmxScreen->request,
			   dmxDnDAwarePropertyReply,
			   prop.sequence,
			   (void *) n);

	    prop = xcb_get_property (dmxScreen->connection,
				     xFalse,
				     c[n],
				     dmxBEAtom (dmxScreen,
						dmxScreen->xdndProxyAtom),
				     XCB_GET_PROPERTY_TYPE_ANY,
				     0,
				     1);
	    dmxAddRequest (&dmxScreen->request,
			   dmxDnDProxyPropertyReply,
			   prop.sequence,
			   (void *) n);

	    geometry = xcb_get_geometry (dmxScreen->connection, c[n]);
	    dmxAddRequest (&dmxScreen->request,
			   dmxDnDGeometryReply,
			   geometry.sequence,
			   (void *) n);

	    attr = xcb_get_window_attributes (dmxScreen->connection, c[n]);
	    dmxAddRequest (&dmxScreen->request,
			   dmxDnDWindowAttributesReply,
			   attr.sequence,
			   (void *) n);
	}
    }
}

static void
dmxBEDnDUpdatePosition (ScreenPtr pScreen,
			int       x,
			int       y)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    int           i;

    for (i = 0; i < dmxNumScreens; i++)
	dmxScreens[i].dndStatus = 0;

    dmxScreen->dndX      = x;
    dmxScreen->dndY      = y;
    dmxScreen->dndStatus = 1;

    if (!dmxScreen->dndChildren)
    {
	if (!dmxScreen->queryTree.sequence)
	{
	    Window root = DefaultRootWindow (dmxScreen->beDisplay);

	    dmxScreen->queryTree = xcb_query_tree (dmxScreen->connection,
						   root);
	    dmxAddRequest (&dmxScreen->request,
			   dmxDnDQueryTreeReply,
			   dmxScreen->queryTree.sequence,
			   0);
	}
    }

    for (i = 0; i < dmxNumScreens; i++)
	dmxBEDnDUpdateTarget (screenInfo.screens[i]);
}

static void
dmxBEDnDHideProxyWindow (ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    WindowPtr     pProxyWin = dmxScreen->pSelectionProxyWin[0];

    if (!pProxyWin->mapped)
	return;

    dmxScreen->dndStatus = 0;
    dmxBEDnDUpdateTarget (pScreen);
    
    UnmapWindow (pProxyWin, FALSE);

    if (dmxScreen->dndChildren)
    {
	xfree (dmxScreen->dndChildren);

	dmxScreen->dndChildren  = NULL;
	dmxScreen->dndNChildren = 0;
    }

    dmxScreen->queryTree.sequence = 0;
}

void
dmxBEDnDSpriteUpdate (ScreenPtr pScreen,
		      Window    event,
		      int       rootX,
		      int       rootY)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    WindowPtr     pProxyWin = dmxScreen->pSelectionProxyWin[0];

    if (event != dmxScreen->rootWin)
    {
	if (!pProxyWin->mapped)
	{
	    Selection *pSel;
	    XID       vlist[5];

	    if (dixLookupSelection (&pSel,
				    dmxScreen->xdndSelectionAtom,
				    serverClient,
				    DixReadAccess) != Success)
		return;

	    if (!pSel->window)
		return;

	    vlist[0] = 0;
	    vlist[1] = 0;
	    vlist[2] = WindowTable[pScreen->myNum]->drawable.width;
	    vlist[3] = WindowTable[pScreen->myNum]->drawable.height;
	    vlist[4] = Above;

#ifdef PANORAMIX
	    if (!noPanoramiXExtension)
	    {
		int j;

		FOR_NSCREENS_BACKWARD(j) {
		    ConfigureWindow (dmxScreens[j].pSelectionProxyWin[0],
				     CWX | CWY | CWWidth | CWHeight |
				     CWStackMode,
				     vlist,
				     serverClient);
		    MapWindow (dmxScreens[j].pSelectionProxyWin[0],
			       serverClient);

		    dmxScreens[j].dndSource = None;
		    dmxScreens[j].dndTarget = None;
		}
	    }
	    else
#endif
	
	    {
		ConfigureWindow (pProxyWin,
				 CWX | CWY | CWWidth | CWHeight | CWStackMode,
				 vlist,
				 serverClient);
		MapWindow (pProxyWin, serverClient);

		dmxScreen->dndSource = None;
		dmxScreen->dndTarget = None;
	    }
	}

	dmxBEDnDUpdatePosition (pScreen, rootX, rootY);
    }
    else
    {

#ifdef PANORAMIX
	if (!noPanoramiXExtension)
	{
	    int j;

	    FOR_NSCREENS_BACKWARD(j) {
		dmxBEDnDHideProxyWindow (screenInfo.screens[j]);
	    }
	}
	else
#endif

	    dmxBEDnDHideProxyWindow (pScreen);
    }
}

static void
dmxDnDUpdatePosition (DMXScreenInfo *dmxScreen,
		      WindowPtr     pWin,
		      int           x,
		      int           y)
{
    WindowPtr pDst = NullWindow;
    BoxRec    box;
    XID       targetId = None;
    xEvent    event;
    int       version = 0;

    event.u.u.type   = ClientMessage | 0x80;
    event.u.u.detail = 32;
    
    event.u.clientMessage.u.l.longs0 = dmxScreens[0].selectionProxyWid[0];

    if (pWin)
    {
	if (!dmxFakeMotion (&dmxScreen->input, x, y))
	    pWin = NullWindow;
    }
    else
    {
	dmxEndFakeMotion (&dmxScreen->input);
    }

    while (pWin)
    {
	if ((pWin->mapped) &&
	    (x >= pWin->drawable.x - wBorderWidth (pWin)) &&
	    (x < pWin->drawable.x + (int) pWin->drawable.width +
	     wBorderWidth(pWin)) &&
	    (y >= pWin->drawable.y - wBorderWidth (pWin)) &&
	    (y < pWin->drawable.y + (int) pWin->drawable.height +
	     wBorderWidth (pWin))
	    /* When a window is shaped, a further check
	     * is made to see if the point is inside
	     * borderSize
	     */
	    && (!wBoundingShape (pWin) ||
		POINT_IN_REGION (pWin->drawable.pScreen, 
				 &pWin->borderSize, x, y, &box))
	
	    && (!wInputShape (pWin) ||
		POINT_IN_REGION (pWin->drawable.pScreen,
				 wInputShape (pWin),
				 x - pWin->drawable.x,
				 y - pWin->drawable.y, &box)))
	{
	    WindowPtr   pProxy = NullWindow;
	    PropertyPtr pProp;

	    if (dixLookupProperty (&pProp,
				   pWin,
				   dmxScreen->xdndProxyAtom,
				   serverClient,
				   DixReadAccess) == Success)
	    {
		if (pProp->format == 32 && pProp->size == 1)
		    dixLookupWindow (&pProxy,
				     *((XID *) pProp->data),
				     serverClient,
				     DixReadAccess);
	    }

	    if (dixLookupProperty (&pProp,
				   pProxy ? pProxy : pWin,
				   dmxScreen->xdndAwareAtom,
				   serverClient,
				   DixReadAccess) == Success)
	    {
		if (pProp->format == 32 && pProp->size == 1)
		{
		    Atom v;

		    v = *((Atom *) pProp->data);
		    if (v >= 3)
		    {
			pDst = pProxy ? pProxy : pWin;
			targetId = pWin->drawable.id;
			version = v;
			if (version > 5)
			    version = 5;
		    }
		}
	    }

	    if (dixLookupProperty (&pProp,
				   pWin,
				   dmxScreen->wmStateAtom,
				   serverClient,
				   DixReadAccess) == Success)
		break;

	    pWin = pWin->firstChild;
	}
	else
	    pWin = pWin->nextSib;
    }

    if (dmxScreen->dndTarget != targetId)
    {
	if (dmxScreen->dndWindow)
	{
	    event.u.clientMessage.window   = dmxScreen->dndTarget;
	    event.u.clientMessage.u.l.type = dmxScreen->xdndLeaveAtom;

	    event.u.clientMessage.u.l.longs1 = 0;
	    event.u.clientMessage.u.l.longs2 = 0;
	    event.u.clientMessage.u.l.longs3 = 0;
	    event.u.clientMessage.u.l.longs4 = 0;

	    if (dixLookupWindow (&pWin,
				 dmxScreen->dndWindow,
				 serverClient,
				 DixReadAccess) == Success)
		DeliverEventsToWindow (PickPointer (serverClient),
				       pWin,
				       &event,
				       1,
				       NoEventMask,
				       NullGrab, 0);

	    dmxScreen->dndStatus = 0;
	    dmxScreen->dndTarget = None;
	    dmxScreen->dndWindow = None;
	}

	if (pDst)
	{
	    event.u.clientMessage.window   = targetId;
	    event.u.clientMessage.u.l.type = dmxScreen->xdndEnterAtom;

	    event.u.clientMessage.u.l.longs1 = version << 24;
	    event.u.clientMessage.u.l.longs2 = dmxScreen->dndType[0];
	    event.u.clientMessage.u.l.longs3 = dmxScreen->dndType[1];
	    event.u.clientMessage.u.l.longs4 = dmxScreen->dndType[2];

	    if (dmxScreen->dndHasTypeProp)
		event.u.clientMessage.u.l.longs1 |= 1;

	    DeliverEventsToWindow (PickPointer (serverClient),
				   pDst,
				   &event,
				   1,
				   NoEventMask,
				   NullGrab, 0);

	    dmxScreen->dndStatus         = 0;
	    dmxScreen->dndWindow         = pDst->drawable.id;
	    dmxScreen->dndTarget         = targetId;
	    dmxScreen->dndXPos           = -1;
	    dmxScreen->dndYPos           = -1;
	    dmxScreen->dndAcceptedAction = None;
	    dmxScreen->dndVersion        = version;

	    REGION_EMPTY (pScreen, &dmxScreen->dndBox);
	}
    }

    if (pDst && !POINT_IN_REGION (pScreen,
				  &dmxScreen->dndBox,
				  x, y, &box))
    {
	event.u.clientMessage.window   = dmxScreen->dndTarget;
	event.u.clientMessage.u.l.type = dmxScreen->xdndPositionAtom;

	event.u.clientMessage.u.l.longs1 = 0;
	event.u.clientMessage.u.l.longs2 = (x << 16) | y;
	event.u.clientMessage.u.l.longs3 = currentTime.milliseconds;
	event.u.clientMessage.u.l.longs4 = dmxScreen->dndAction;

	DeliverEventsToWindow (PickPointer (serverClient),
			       pDst,
			       &event,
			       1,
			       NoEventMask,
			       NullGrab, 0);

	dmxScreen->dndXPos = x;
	dmxScreen->dndYPos = y;

	box.x1 = SHRT_MIN;
	box.y1 = SHRT_MIN;
	box.x2 = SHRT_MAX;
	box.y2 = SHRT_MAX;

	REGION_RESET (pScreen, &dmxScreen->dndBox, &box);
    }
}

static void
dmxDnDTranslateCoordinatesReply (ScreenPtr           pScreen,
				 unsigned int        sequence,
				 xcb_generic_reply_t *reply,
				 xcb_generic_error_t *error,
				 void                *data)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (reply)
    {
	xcb_translate_coordinates_reply_t *xcoord =
	    (xcb_translate_coordinates_reply_t *) reply;

	dmxScreen->dndX = xcoord->dst_x;
	dmxScreen->dndY = xcoord->dst_y;

	if (dmxScreen->dndSource)
	{
	    WindowPtr                  pWin = WindowTable[pScreen->myNum];
	    xcb_client_message_event_t xevent;

#ifdef PANORAMIX
	    if (!noPanoramiXExtension)
		pWin = WindowTable[0];
#endif

	    if (!dmxScreen->getTypeProp.sequence)
		dmxDnDUpdatePosition (dmxScreen,
				      pWin,
				      dmxScreen->dndX,
				      dmxScreen->dndY);

	    xevent.response_type = XCB_CLIENT_MESSAGE;
	    xevent.format        = 32;

	    xevent.type   = dmxBEAtom (dmxScreen, dmxScreen->xdndStatusAtom);
	    xevent.window = dmxScreen->dndSource;

	    xevent.data.data32[0] = dmxScreen->dndWid;
	    xevent.data.data32[1] = dmxScreen->dndStatus;
	    xevent.data.data32[2] = 0;
	    xevent.data.data32[3] = 0;
	    xevent.data.data32[4] = 0;

	    if (ValidAtom (dmxScreen->dndAcceptedAction))
		xevent.data.data32[4] =
		    dmxBEAtom (dmxScreen, dmxScreen->dndAcceptedAction);

	    xcb_send_event (dmxScreen->connection,
			    FALSE,
			    dmxScreen->dndSource,
			    0,
			    (const char *) &xevent);
	}
    }
}

static void
dmxDnDGetTypePropReply (ScreenPtr           pScreen,
			unsigned int        sequence,
			xcb_generic_reply_t *reply,
			xcb_generic_error_t *error,
			void                *data)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (reply)
    {
	xcb_get_property_reply_t *xproperty =
	    (xcb_get_property_reply_t *) reply;
	    
	if (xproperty->format                    == 32 &&
	    dmxAtom (dmxScreen, xproperty->type) == XA_ATOM)
	{
	    uint32_t *data = xcb_get_property_value (xproperty);
	    int      i;

	    for (i = 0; i < xcb_get_property_value_length (xproperty); i++)
		data[i] = dmxAtom (dmxScreen, data[i]);

	    ChangeWindowProperty (dmxScreens[0].pSelectionProxyWin[0],
				  dmxScreen->xdndTypeListAtom,
				  XA_ATOM,
				  32,
				  PropModeReplace,
				  xcb_get_property_value_length (xproperty),
				  data,
				  TRUE);

	    dmxScreen->dndHasTypeProp = TRUE;
	}
	    
	if (dmxScreen->dndX != -1 && dmxScreen->dndY != -1)
	{
	    WindowPtr pWin = WindowTable[pScreen->myNum];

#ifdef PANORAMIX
	    if (!noPanoramiXExtension)
		pWin = WindowTable[0];
#endif

	    dmxDnDUpdatePosition (dmxScreen,
				  pWin,
				  dmxScreen->dndX,
				  dmxScreen->dndY);
	}
    }

    dmxScreen->getTypeProp.sequence = 0;
}

static void
dmxDnDGetActionListPropReply (ScreenPtr           pScreen,
			      unsigned int        sequence,
			      xcb_generic_reply_t *reply,
			      xcb_generic_error_t *error,
			      void                *data)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (reply)
    {
	xcb_get_property_reply_t *xproperty =
	    (xcb_get_property_reply_t *) reply;
	    
	if (xproperty->format                    == 32 &&
	    dmxAtom (dmxScreen, xproperty->type) == XA_ATOM)
	{
	    uint32_t *data = xcb_get_property_value (xproperty);
	    int      i;

	    for (i = 0; i < xcb_get_property_value_length (xproperty); i++)
		data[i] = dmxAtom (dmxScreen, data[i]);

	    ChangeWindowProperty (dmxScreens[0].pSelectionProxyWin[0],
				  dmxScreen->xdndActionListAtom,
				  XA_ATOM,
				  32,
				  PropModeReplace,
				  xcb_get_property_value_length (xproperty),
				  data,
				  TRUE);
	}
    }

    dmxScreen->getActionListProp.sequence = 0;
}

static void
dmxDnDGetActionDescriptionPropReply (ScreenPtr           pScreen,
				     unsigned int        sequence,
				     xcb_generic_reply_t *reply,
				     xcb_generic_error_t *error,
				     void                *data)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (reply)
    {
	xcb_get_property_reply_t *xproperty =
	    (xcb_get_property_reply_t *) reply;
	    
	if (xproperty->format                    == 8 &&
	    dmxAtom (dmxScreen, xproperty->type) == XA_STRING)
	{
	    ChangeWindowProperty (dmxScreens[0].pSelectionProxyWin[0],
				  dmxScreen->xdndActionDescriptionAtom,
				  XA_STRING,
				  8,
				  PropModeReplace,
				  xcb_get_property_value_length (xproperty),
				  xcb_get_property_value (xproperty),
				  TRUE);
	}
    }

    dmxScreen->getActionDescriptionProp.sequence = 0;
}

static void
dmxDnDPositionMessage (ScreenPtr pScreen,
		       Window    source,
		       int       xRoot,
		       int       yRoot,
		       Time      time,
		       Atom      action)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    if (action)
	dmxScreen->dndAction = dmxAtom (dmxScreen, action);

    if (dmxScreen->dndAction == dmxScreen->xdndActionAskAtom)
    {
	dmxScreen->getActionListProp =
	    xcb_get_property (dmxScreen->connection,
			      xFalse,
			      source,
			      dmxBEAtom (dmxScreen,
					 dmxScreen->xdndActionListAtom),
			      XCB_GET_PROPERTY_TYPE_ANY,
			      0,
			      0xffffffff);

	dmxAddRequest (&dmxScreen->request,
		       dmxDnDGetActionListPropReply,
		       dmxScreen->getActionListProp.sequence,
		       0);

	dmxScreen->getActionDescriptionProp =
	    xcb_get_property (dmxScreen->connection,
			      xFalse,
			      source,
			      dmxBEAtom (dmxScreen,
					 dmxScreen->xdndActionDescriptionAtom),
			      XCB_GET_PROPERTY_TYPE_ANY,
			      0,
			      0xffffffff);

	dmxAddRequest (&dmxScreen->request,
		       dmxDnDGetActionDescriptionPropReply,
		       dmxScreen->getActionDescriptionProp.sequence,
		       0);
    }

    dmxScreen->translateCoordinates =
	xcb_translate_coordinates (dmxScreen->connection,
				   DefaultRootWindow (dmxScreen->beDisplay),
				   dmxScreen->rootWin,
				   xRoot,
				   yRoot);
    	dmxAddRequest (&dmxScreen->request,
		       dmxDnDTranslateCoordinatesReply,
		       dmxScreen->translateCoordinates.sequence,
		       0);
}

static void
dmxDnDEnterMessage (ScreenPtr pScreen,
		    Window    target,
		    Window    source,
		    Atom      type0,
		    Atom      type1,
		    Atom      type2,
		    Bool      hasTypeProp,
		    int       version)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    dmxScreen->dndWid            = target;
    dmxScreen->dndSource         = source;
    dmxScreen->dndAction         = None;
    dmxScreen->dndAcceptedAction = None;
    dmxScreen->dndType[0]        = type0;
    dmxScreen->dndType[1]        = type1;
    dmxScreen->dndType[2]        = type2;
    dmxScreen->dndHasTypeProp    = FALSE;

    if (hasTypeProp)
    {
	dmxScreen->getTypeProp =
	    xcb_get_property (dmxScreen->connection,
			      xFalse,
			      source,
			      dmxBEAtom (dmxScreen,
					 dmxScreen->xdndTypeListAtom),
			      XCB_GET_PROPERTY_TYPE_ANY,
			      0,
			      0xffffffff);

	if (dmxScreen->getTypeProp.sequence)
	    dmxAddRequest (&dmxScreen->request,
			   dmxDnDGetTypePropReply,
			   dmxScreen->getTypeProp.sequence,
			   0);
    }
}

static void
dmxDnDLeaveMessage (ScreenPtr pScreen,
		    Window    source)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    dmxDnDUpdatePosition (dmxScreen, NullWindow, 0, 0);

    dmxScreen->dndWid    = None;
    dmxScreen->dndSource = None;
    dmxScreen->dndStatus = 0;
    dmxScreen->dndXPos   = -1;
    dmxScreen->dndYPos   = -1;
    dmxScreen->dndX      = -1;
    dmxScreen->dndY      = -1;
}

static void
dmxDnDDropMessage (ScreenPtr pScreen,
		   Window    source,
		   Time      time)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    dmxEndFakeMotion (&dmxScreen->input);

    if (dmxScreen->dndWindow)
    {
	WindowPtr pWin;
	xEvent    event;
	BoxRec    box;

	event.u.u.type   = ClientMessage | 0x80;
	event.u.u.detail = 32;

	event.u.clientMessage.window   = dmxScreen->dndTarget;
	event.u.clientMessage.u.l.type = dmxScreen->xdndDropAtom;

	event.u.clientMessage.u.l.longs0 = dmxScreens[0].selectionProxyWid[0];
	event.u.clientMessage.u.l.longs1 = 0;
	event.u.clientMessage.u.l.longs2 = currentTime.milliseconds;
	event.u.clientMessage.u.l.longs3 = 0;
	event.u.clientMessage.u.l.longs4 = 0;

	if (dixLookupWindow (&pWin,
			     dmxScreen->dndWindow,
			     serverClient,
			     DixReadAccess) == Success)
	    DeliverEventsToWindow (PickPointer (serverClient),
				   pWin,
				   &event,
				   1,
				   NoEventMask,
				   NullGrab, 0);

	box.x1 = SHRT_MIN;
	box.y1 = SHRT_MIN;
	box.x2 = SHRT_MAX;
	box.y2 = SHRT_MAX;

	REGION_RESET (pScreen, &dmxScreen->dndBox, &box);
    }
    else
    {
	xcb_client_message_event_t xevent;

	dmxDnDLeaveMessage (pScreen, source);

	xevent.response_type = XCB_CLIENT_MESSAGE;
	xevent.format        = 32;

	xevent.type   = dmxBEAtom (dmxScreen, dmxScreen->xdndFinishedAtom);
	xevent.window = dmxScreen->dndSource;

	xevent.data.data32[0] = dmxScreen->dndWid;
	xevent.data.data32[1] = 0;
	xevent.data.data32[2] = 0;
	xevent.data.data32[3] = 0;
	xevent.data.data32[4] = 0;

	xcb_send_event (dmxScreen->connection,
			FALSE,
			dmxScreen->dndSource,
			0,
			(const char *) &xevent);

	dmxScreen->dndWid    = None;
	dmxScreen->dndSource = None;
    }
}

static void
dmxDnDStatusMessage (ScreenPtr pScreen,
		     Window    target,
		     int       status,
		     Atom      action)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    WindowPtr     pWin;

    if (!dmxScreens[0].dndWindow)
	return;

    if (!dmxScreen->dndStatus)
	return;

    if (target != dmxScreen->dndTarget)
	return;

    if (dixLookupWindow (&pWin,
			 dmxScreens[0].dndWindow,
			 serverClient,
			 DixReadAccess) == Success)
    {
	xEvent x;

	x.u.u.type                   = ClientMessage | 0x80;
	x.u.u.detail                 = 32;
	x.u.clientMessage.window     = dmxScreens[0].dndWindow;
	x.u.clientMessage.u.l.type   = dmxScreens[0].xdndStatusAtom;
	x.u.clientMessage.u.l.longs0 = dmxScreens[0].selectionProxyWid[0];
	x.u.clientMessage.u.l.longs1 = 0;
	x.u.clientMessage.u.l.longs2 = 0;
	x.u.clientMessage.u.l.longs3 = 0;
	x.u.clientMessage.u.l.longs4 = 0;

	if (status)
	{
	    x.u.clientMessage.u.l.longs1 = 1;
	    x.u.clientMessage.u.l.longs4 = dmxAtom (dmxScreen, action);
	}

	DeliverEventsToWindow (PickPointer (serverClient),
			       pWin,
			       &x,
			       1,
			       NoEventMask,
			       NullGrab, 0);
    }
}

static void
dmxDnDFinishedMessage (ScreenPtr pScreen,
		       Window    target,
		       int       status,
		       Atom      action)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    WindowPtr     pWin;

    if (!dmxScreens[0].dndWindow)
	return;

    if (target != dmxScreen->dndTarget)
	return;

    if (dixLookupWindow (&pWin,
			 dmxScreens[0].dndWindow,
			 serverClient,
			 DixReadAccess) == Success)
    {
	xEvent x;

	x.u.u.type                   = ClientMessage | 0x80;
	x.u.u.detail                 = 32;
	x.u.clientMessage.window     = dmxScreens[0].dndWindow;
	x.u.clientMessage.u.l.type   = dmxScreens[0].xdndFinishedAtom;
	x.u.clientMessage.u.l.longs0 = dmxScreens[0].selectionProxyWid[0];
	x.u.clientMessage.u.l.longs1 = 0;
	x.u.clientMessage.u.l.longs2 = 0;
	x.u.clientMessage.u.l.longs3 = 0;
	x.u.clientMessage.u.l.longs4 = 0;

	if (status)
	{
	    x.u.clientMessage.u.l.longs1 = 1;
	    x.u.clientMessage.u.l.longs2 = dmxAtom (dmxScreen, action);
	}

	DeliverEventsToWindow (PickPointer (serverClient),
			       pWin,
			       &x,
			       1,
			       NoEventMask,
			       NullGrab, 0);
    }
}

Bool
dmxScreenEventCheckDnD (ScreenPtr           pScreen,
			xcb_generic_event_t *event)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    switch (event->response_type & ~0x80) {
    case XCB_CLIENT_MESSAGE: {
	xcb_client_message_event_t *xclient =
	    (xcb_client_message_event_t *) event;
	xcb_atom_t                 type = xclient->type;

	if (dmxAtom (dmxScreen, type) == dmxScreen->xdndPositionAtom)
	{
	    dmxDnDPositionMessage (pScreen,
				   xclient->data.data32[0],
				   xclient->data.data32[2] >> 16,
				   xclient->data.data32[2] & 0xffff,
				   xclient->data.data32[3],
				   xclient->data.data32[4]);
	}
	else if (dmxAtom (dmxScreen, type) == dmxScreen->xdndEnterAtom)
	{
	    dmxDnDEnterMessage (pScreen,
				xclient->window,
				xclient->data.data32[0],
				xclient->data.data32[2],
				xclient->data.data32[3],
				xclient->data.data32[4],
				((xclient->data.data32[1] & 1) != 0),
				(xclient->data.data32[1] & 0xff000000) >> 24);
	}
	else if (dmxAtom (dmxScreen, type) == dmxScreen->xdndLeaveAtom)
	{
	    dmxDnDLeaveMessage (pScreen, xclient->data.data32[0]);
	}
	else if (dmxAtom (dmxScreen, type) == dmxScreen->xdndDropAtom)
	{
	    dmxDnDDropMessage (pScreen,
			       xclient->data.data32[0],
			       xclient->data.data32[2]);
	}
	else if (dmxAtom (dmxScreen, type) == dmxScreen->xdndStatusAtom)
	{
	    dmxDnDStatusMessage (pScreen,
				 xclient->data.data32[0],
				 ((xclient->data.data32[1] & 1) != 0),
				 xclient->data.data32[4]);
	}
	else if (dmxAtom (dmxScreen, type) == dmxScreen->xdndFinishedAtom)
	{
	    dmxDnDFinishedMessage (pScreen,
				   xclient->data.data32[0],
				   ((xclient->data.data32[1] & 1) != 0),
				   xclient->data.data32[2]);
	}
	else
	{
	    return FALSE;
	}
    } break;
    default:
	return FALSE;
    }

    return TRUE;
}

void
dmxDnDClientMessageEvent (xEvent *event)
{
    Atom type = event->u.clientMessage.u.l.type;
    int  i;

    for (i = 0; i < dmxNumScreens; i++)
    {
	DMXScreenInfo *dmxScreen = &dmxScreens[i];

	if (type == dmxScreens[0].xdndStatusAtom)
	{
	    int  dndStatus = event->u.clientMessage.u.l.longs1 & 1;
	    Atom dndAcceptedAction = event->u.clientMessage.u.l.longs4;

	    if (!dmxScreen->dndSource)
		continue;

	    if (event->u.clientMessage.u.l.longs0 != dmxScreen->dndTarget)
		continue;

	    if (dmxScreen->dndStatus         != dndStatus ||
		dmxScreen->dndAcceptedAction != dndAcceptedAction)
	    {
		xcb_client_message_event_t xevent;

		dmxScreen->dndStatus         = dndStatus;
		dmxScreen->dndAcceptedAction = dndAcceptedAction;

		xevent.response_type = XCB_CLIENT_MESSAGE;
		xevent.format        = 32;

		xevent.type   = dmxBEAtom (dmxScreen,
					   dmxScreen->xdndStatusAtom);
		xevent.window = dmxScreen->dndSource;

		xevent.data.data32[0] = dmxScreen->dndWid;
		xevent.data.data32[1] = dmxScreen->dndStatus;
		xevent.data.data32[2] = 0;
		xevent.data.data32[3] = 0;
		xevent.data.data32[4] = 0;

		if (ValidAtom (dmxScreen->dndAcceptedAction))
		    xevent.data.data32[4] =
			dmxBEAtom (dmxScreen,
				   dmxScreen->dndAcceptedAction);

		xcb_send_event (dmxScreen->connection,
				FALSE,
				dmxScreen->dndSource,
				0,
				(const char *) &xevent);
	    }

	    REGION_EMPTY (pScreen, &dmxScreen->dndBox);

	    if (dmxScreen->dndStatus & 2)
	    {
		BoxRec box;

		box.x1 = event->u.clientMessage.u.l.longs2 >> 16;
		box.y1 = event->u.clientMessage.u.l.longs2 & 0xffff;
		box.x2 = box.x1 +
		    (event->u.clientMessage.u.l.longs3 >> 16);
		box.y2 = box.y1 +
		    (event->u.clientMessage.u.l.longs3 & 0xffff);

		REGION_RESET (pScreen, &dmxScreen->dndBox, &box);
	    }

	    if (dmxScreen->dndX != dmxScreen->dndXPos ||
		dmxScreen->dndY != dmxScreen->dndYPos)
	    {
		WindowPtr pWin = WindowTable[i];
#ifdef PANORAMIX
		if (!noPanoramiXExtension)
		    pWin = WindowTable[0];
#endif

		dmxDnDUpdatePosition (dmxScreen,
				      pWin,
				      dmxScreen->dndX,
				      dmxScreen->dndY);
	    }
	}
	else if (type == dmxScreen->xdndFinishedAtom)
	{
	    xcb_client_message_event_t xevent;

	    if (!dmxScreen->dndSource)
		continue;

	    if (event->u.clientMessage.u.l.longs0 != dmxScreen->dndTarget)
		continue;

	    xevent.response_type = XCB_CLIENT_MESSAGE;
	    xevent.format        = 32;

	    xevent.type   = dmxBEAtom (dmxScreen, dmxScreen->xdndFinishedAtom);
	    xevent.window = dmxScreen->dndSource;

	    xevent.data.data32[0] = dmxScreen->dndWid;
	    xevent.data.data32[1] = 0;
	    xevent.data.data32[2] = 0;
	    xevent.data.data32[3] = 0;
	    xevent.data.data32[4] = 0;

	    if (dmxScreen->dndVersion >= 5)
	    {
		xevent.data.data32[1] = event->u.clientMessage.u.l.longs1 & 1;
		xevent.data.data32[2] = event->u.clientMessage.u.l.longs2;
	    }

	    xcb_send_event (dmxScreen->connection,
			    FALSE,
			    dmxScreen->dndSource,
			    0,
			    (const char *) &xevent);

	    dmxScreen->dndTarget = None;
	    dmxScreen->dndWindow = None;
		
	    dmxDnDLeaveMessage (screenInfo.screens[i], dmxScreen->dndSource);

	    dmxScreen->dndWid    = None;
	    dmxScreen->dndSource = None;
	}
	else if (type == dmxScreen->xdndEnterAtom)
	{
	    dmxScreen->dndSource      = None;
	    dmxScreen->dndAction      = None;
	    dmxScreen->dndType[0]     = event->u.clientMessage.u.l.longs2;
	    dmxScreen->dndType[1]     = event->u.clientMessage.u.l.longs3;
	    dmxScreen->dndType[2]     = event->u.clientMessage.u.l.longs4;
	    dmxScreen->dndHasTypeProp =
		((event->u.clientMessage.u.l.longs1 & 1) != 0);
	}
	else if (type == dmxScreen->xdndLeaveAtom)
	{
	    dmxBEDnDHideProxyWindow (screenInfo.screens[i]);
	}
	else if (type == dmxScreen->xdndPositionAtom)
	{
	    WindowPtr pWin = NullWindow;
	    Window    dndSource = dmxScreen->dndSource;

#ifdef PANORAMIX
	    PanoramiXRes *win = NULL;

	    if (!noPanoramiXExtension)
	    {
		win = (PanoramiXRes *)
		    SecurityLookupIDByType (serverClient,
					    event->u.clientMessage.u.l.longs0,
					    XRT_WINDOW,
					    DixReadAccess);
		if (win)
		    dixLookupWindow (&pWin, win->info[i].id,
				     serverClient, DixReadAccess);
	    }
	    else
#endif
		dixLookupWindow (&pWin, event->u.clientMessage.u.l.longs0,
				 serverClient, DixReadAccess);

	    if (pWin)
		dmxScreen->dndSource = DMX_GET_WINDOW_PRIV (pWin)->window;

	    dmxScreen->dndWindow = event->u.clientMessage.u.l.longs0;

	    if (dmxScreen->dndAction != event->u.clientMessage.u.l.longs4)
	    {
		dmxScreen->dndAction = event->u.clientMessage.u.l.longs4;
		dmxScreen->dndXPos   = -1;
		dmxScreen->dndXPos   = -1;
	    }

	    if (dmxScreen->dndStatus)
	    {
		dmxBEDnDUpdatePosition (screenInfo.screens[i],
					dmxScreen->dndX,
					dmxScreen->dndY);

		if (dmxScreen->dndSource != dndSource)
		    dmxDnDSendDeclineStatus ();
	    }
	}
	else if (type == dmxScreen->xdndDropAtom)
	{
	    Window target = None;
	    int    status = 0;
	    
	    if (dmxScreen->dndStatus)
	    {
		if (dmxScreen->dndTarget)
		{
		    xcb_client_message_event_t xevent;

		    xevent.response_type = XCB_CLIENT_MESSAGE;
		    xevent.format        = 32;

		    xevent.type   = dmxBEAtom (dmxScreen,
					       dmxScreen->xdndDropAtom);
		    xevent.window = dmxScreen->dndTarget;

		    xevent.data.data32[0] = dmxScreen->dndSource;
		    xevent.data.data32[1] = 0;
		    xevent.data.data32[2] = 0; /* XXX: need time stamp */
		    xevent.data.data32[3] = 0;
		    xevent.data.data32[4] = 0;

		    xcb_send_event (dmxScreen->connection,
				    FALSE,
				    dmxScreen->dndWid,
				    0,
				    (const char *) &xevent);

		    status = 1;
		    target = dmxScreen->dndTarget;

		    dmxScreen->dndTarget = None;
		}
		else
		{
		    WindowPtr pWin;
		    xEvent    x;

		    x.u.u.type                   = ClientMessage | 0x80;
		    x.u.u.detail                 = 32;
		    x.u.clientMessage.window     = dmxScreens[0].dndWindow;
		    x.u.clientMessage.u.l.type   =
			dmxScreens[0].xdndFinishedAtom;
		    x.u.clientMessage.u.l.longs0 =
			dmxScreens[0].selectionProxyWid[0];
		    x.u.clientMessage.u.l.longs1 = 0;
		    x.u.clientMessage.u.l.longs2 = 0;
		    x.u.clientMessage.u.l.longs3 = 0;
		    x.u.clientMessage.u.l.longs4 = 0;

		    if (dixLookupWindow (&pWin,
					 dmxScreens[0].dndWindow,
					 serverClient,
					 DixReadAccess) == Success)
			DeliverEventsToWindow (PickPointer (serverClient),
					       pWin,
					       &x,
					       1,
					       NoEventMask,
					       NullGrab, 0);
		}
	    }

	    dmxBEDnDHideProxyWindow (screenInfo.screens[i]);

	    dmxScreen->dndTarget = target;
	    dmxScreen->dndStatus = status;
	}
    }
}

#define MAKE_DND_ATOM(name)				\
    MakeAtom ("Xdnd" name, strlen ("Xdnd" name), TRUE)

Bool
dmxDnDScreenInit (ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    
    dmxScreen->wmStateAtom = MakeAtom ("WM_STATE", strlen ("WM_STATE"), TRUE);

    dmxScreen->xdndProxyAtom = MAKE_DND_ATOM ("Proxy");
    dmxScreen->xdndAwareAtom = MAKE_DND_ATOM ("Aware");
    dmxScreen->xdndSelectionAtom = MAKE_DND_ATOM ("Selection");
    dmxScreen->xdndEnterAtom = MAKE_DND_ATOM ("Enter");
    dmxScreen->xdndPositionAtom = MAKE_DND_ATOM ("Position");
    dmxScreen->xdndStatusAtom = MAKE_DND_ATOM ("Status");
    dmxScreen->xdndLeaveAtom = MAKE_DND_ATOM ("Leave");
    dmxScreen->xdndDropAtom = MAKE_DND_ATOM ("Drop");
    dmxScreen->xdndFinishedAtom = MAKE_DND_ATOM ("Finished");
    dmxScreen->xdndTypeListAtom = MAKE_DND_ATOM ("TypeList");
    dmxScreen->xdndActionAskAtom = MAKE_DND_ATOM ("ActionAsk");
    dmxScreen->xdndActionListAtom = MAKE_DND_ATOM ("ActionList");
    dmxScreen->xdndActionDescriptionAtom = MAKE_DND_ATOM ("ActionDescription");

    dmxScreen->dndXPos = -1;
    dmxScreen->dndYPos = -1;

    REGION_INIT (pScreen, &dmxScreen->dndBox, NullBox, 0);

    return TRUE;
}

void
dmxDnDScreenFini (ScreenPtr pScreen)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    REGION_UNINIT (pScreen, &dmxScreen->dndBox);
}
