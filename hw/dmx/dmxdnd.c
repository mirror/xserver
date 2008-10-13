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

#include <xcb/xinput.h>

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

	    if (dmxScreen->dndAcceptedAction &&
		ValidAtom (dmxScreen->dndAcceptedAction))
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
    int i;

    for (i = 0; i < dmxNumScreens; i++)
    {
	DMXScreenInfo *dmxScreen = &dmxScreens[i];

	if (event->u.clientMessage.u.l.longs0 == dmxScreen->dndTarget)
	{
	    Atom type = event->u.clientMessage.u.l.type;

	    if (!dmxScreen->dndSource)
		continue;

	    if (type == dmxScreen->xdndStatusAtom)
	    {
		int  dndStatus = event->u.clientMessage.u.l.longs1 & 1;
		Atom dndAcceptedAction = event->u.clientMessage.u.l.longs4;

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

		    if (dmxScreen->dndAcceptedAction &&
			ValidAtom (dmxScreen->dndAcceptedAction))
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

		xevent.response_type = XCB_CLIENT_MESSAGE;
		xevent.format        = 32;

		xevent.type   = dmxBEAtom (dmxScreen,
					   dmxScreen->xdndFinishedAtom);
		xevent.window = dmxScreen->dndSource;

		xevent.data.data32[0] = dmxScreen->dndWid;
		xevent.data.data32[1] = 0;
		xevent.data.data32[2] = 0;
		xevent.data.data32[3] = 0;
		xevent.data.data32[4] = 0;

		if (dmxScreen->dndVersion >= 5)
		{
		    xevent.data.data32[1] =
			event->u.clientMessage.u.l.longs1 & 1;
		    xevent.data.data32[2] = event->u.clientMessage.u.l.longs2;
		}

		xcb_send_event (dmxScreen->connection,
				FALSE,
				dmxScreen->dndSource,
				0,
				(const char *) &xevent);

		dmxScreen->dndTarget = None;
		dmxScreen->dndWindow = None;
		
		dmxDnDLeaveMessage (screenInfo.screens[i],
				    dmxScreen->dndSource);

		dmxScreen->dndWid    = None;
		dmxScreen->dndSource = None;
	    }
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
