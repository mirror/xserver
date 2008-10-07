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
#include "dmxdnd.h"
#include "dmxselection.h"

#include "selection.h"

#ifdef PANORAMIX
#include "panoramiX.h"
#include "panoramiXsrv.h"
#endif

#define DMX_SELECTION_TIMEOUT (60 * 1000) /* 60 seconds */

typedef struct _DMXSelection {
    struct _DMXSelection *next;

    XID  wid;
    XID  requestor;
    Atom selection;
    Atom target;
    Atom property;
    Time time;

    struct {
	unsigned int in;
	unsigned int out;
    } value[MAXSCREENS];

    OsTimerPtr timer;
} DMXSelection;

static DMXSelection *convHead = NULL;
static DMXSelection **convTail = &convHead;

static DMXSelection *propHead = NULL;
static DMXSelection **propTail = &propHead;

static DMXSelection *reqHead = NULL;
static DMXSelection **reqTail = &reqHead;

static DMXSelection *
dmxUnhookSelection (DMXSelection *s,
		    DMXSelection **head,
		    DMXSelection ***tail)
{
    DMXSelection *p, *prev = NULL;

    for (p = *head; p; p = p->next)
    {
	if (p == s)
	    break;

	prev = p;
    }

    assert (p);

    if (prev)
    {
	prev->next = s->next;
	if (!prev->next)
	    *tail = &prev->next;
    }
    else
    {
	*head = s->next;
	if (!s->next)
	    *tail = head;
    }

    s->next = NULL;

    TimerCancel (s->timer);

    return s;
}

static int
dmxSelectionDeleteConv (DMXSelection *s)
{
    WindowPtr pWin;
    xEvent    event;

    event.u.u.type = SelectionNotify;
    event.u.selectionNotify.time = s->time;
    event.u.selectionNotify.requestor = s->wid;
    event.u.selectionNotify.selection = s->selection;
    event.u.selectionNotify.target = s->target;
    event.u.selectionNotify.property = None;

    if (dixLookupWindow (&pWin,
			 s->wid,
			 serverClient,
			 DixReadAccess) == Success)
	DeliverEventsToWindow (inputInfo.pointer, pWin, &event, 1,
			       NoEventMask, NullGrab, 0);

    if (s->timer)
	TimerFree (s->timer);

    xfree (s);
    return 0;
}

static int
dmxSelectionDeleteProp (DMXSelection *s)
{
    int i;

    for (i = 0; i < dmxNumScreens; i++)
    {
	DMXScreenInfo *dmxScreen = &dmxScreens[i];
	
	if (s->property == dmxScreen->incrAtom && s->value[i].out)
	{
	    const uint32_t value = 0;

	    xcb_change_window_attributes (dmxScreen->connection,
					  s->value[i].out,
					  XCB_CW_EVENT_MASK,
					  &value);
	}
    }

    if (s->timer)
	TimerFree (s->timer);

    xfree (s);
    return 0;
}

static int
dmxSelectionDeleteReq (DMXSelection *s)
{
    WindowPtr pWin;
    int       i;

    for (i = 0; i < dmxNumScreens; i++)
    {
	if (s->value[i].out)
	{
	    DMXScreenInfo *dmxScreen = &dmxScreens[i];

	    if (s->property == dmxScreen->incrAtom)
	    {
		const uint32_t value = 0;
	
		xcb_change_window_attributes (dmxScreen->connection,
					      s->value[i].out,
					      XCB_CW_EVENT_MASK,
					      &value);
	    }
	}
    }

    if (s->wid && dixLookupWindow (&pWin,
				   s->wid,
				   serverClient,
				   DixReadAccess) == Success)
	DeleteProperty (serverClient, pWin, s->property);

    if (s->timer)
	TimerFree (s->timer);

    xfree (s);
    return 0;
}

static CARD32
dmxSelectionCallback (OsTimerPtr timer,
		      CARD32     time,
		      pointer    arg)
{
    DMXSelection *r = (DMXSelection *) arg;
    DMXSelection *s;

    dmxLog (dmxWarning,
	    "selection conversion for %s timed out\n",
	    NameForAtom (r->selection));

    for (s = convHead; s; s = s->next)
	if (s == r)
	    return dmxSelectionDeleteConv (dmxUnhookSelection (s,
							       &convHead,
							       &convTail));

    for (s = propHead; s; s = s->next)
	if (s == r)
	    return dmxSelectionDeleteProp (dmxUnhookSelection (s,
							       &propHead,
							       &propTail));

    for (s = reqHead; s; s = s->next)
	if (s == r)
	    return dmxSelectionDeleteReq (dmxUnhookSelection (s,
							      &reqHead,
							      &reqTail));

    return 0;
}

static void
dmxSelectionResetTimer (DMXSelection *s)
{
    s->timer = TimerSet (s->timer,
			 0,
			 DMX_SELECTION_TIMEOUT,
			 dmxSelectionCallback,
			 s);
}

static void
dmxAppendSelection (DMXSelection *s,
		    DMXSelection ***tail)
{
    dmxSelectionResetTimer (s);

    *(*tail) = s;
    *tail = &s->next;
}

static Atom
dmxBESelectionAtom (DMXScreenInfo *dmxScreen,
		    Atom	  atom)
{
    int i;

    for (i = 0; i < dmxSelectionMapNum; i++)
	if (atom == dmxSelectionMap[i].atom)
	    return dmxBEAtom (dmxScreen, dmxSelectionMap[i].beAtom);

    return dmxBEAtom (dmxScreen, atom);
}

static Atom
dmxSelectionAtom (DMXScreenInfo *dmxScreen,
		  Atom	        atom)
{
    int i;

    for (i = 0; i < dmxSelectionMapNum; i++)
	if (atom == dmxSelectionMap[i].beAtom)
	    return dmxAtom (dmxScreen, dmxSelectionMap[i].atom);

    return dmxAtom (dmxScreen, atom);
}

static void
dmxBESetSelectionOwner (ScreenPtr pScreen,
			WindowPtr pWin,
			Atom      selection)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    Window        window = None;

    if (!dmxScreen->beDisplay)
	return;
    
    if (dmxScreen->selectionOwner != dmxScreen->rootWin)
	return;

    if (pWin)
	window = DMX_GET_WINDOW_PRIV(pWin)->window;
    
    xcb_set_selection_owner (dmxScreen->connection,
			     window,
			     dmxBESelectionAtom (dmxScreen, selection),
			     0);
}

static Bool
dmxBEGetSelectionOwner (ScreenPtr pScreen,
			Atom      selection)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    /* reset getSelectionOwner fields */
    dmxScreen->getSelectionOwner.sequence = 0;
    dmxScreen->getSelectionOwnerResult = 0;

    if (!dmxScreen->beDisplay)
	return FALSE;

    if (dmxScreen->selectionOwner != dmxScreen->rootWin)
	return FALSE;

    dmxScreen->getSelectionOwner =
	xcb_get_selection_owner (dmxScreen->connection,
				 dmxBESelectionAtom (dmxScreen, selection));

    if (!dmxScreen->getSelectionOwner.sequence)
	return FALSE;

    dmxAddSequence (&dmxScreen->request,
		    dmxScreen->getSelectionOwner.sequence);

    return TRUE;
}

static Window
dmxBEConvertSelection (WindowPtr pWin,
		       Atom      selection,
		       Atom      target,
		       Atom      property,
		       Time      time)
{
    ScreenPtr     pScreen = pWin->drawable.pScreen;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV(pWin);

    if (!dmxScreen->beDisplay || !pWinPriv->window)
	return 0;

    if (dmxScreen->selectionOwner != dmxScreen->rootWin)
	return 0;

    xcb_convert_selection (dmxScreen->connection,
			   pWinPriv->window,
			   dmxBESelectionAtom (dmxScreen, selection),
			   dmxBEAtom (dmxScreen, target),
			   property ? dmxBEAtom (dmxScreen, property) : None,
			   0);

    dmxSync (dmxScreen, FALSE);

    return pWinPriv->window;
}

Window
dmxBEGetSelectionAdjustedPropertyWindow (WindowPtr pWin)
{
    ScreenPtr     pScreen = pWin->drawable.pScreen;
    dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV (pWin);
    DMXSelection  *s;

    if (!pWinPriv->window)
	return None;

    for (s = reqHead; s; s = s->next)
	if (s->value[pScreen->myNum].in == pWinPriv->window)
	    return s->value[pScreen->myNum].out;

    return pWinPriv->window;
}

void
dmxSelectionClear (ScreenPtr pScreen,
		   Window    owner,
		   Atom      xSelection)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    Atom          selection = dmxSelectionAtom (dmxScreen, xSelection);
    Selection     *pSel;

    if (!ValidAtom (selection))
	return;

    if (dixLookupSelection (&pSel,
			    selection,
			    serverClient,
			    DixGetAttrAccess) == Success &&
	pSel->client != NullClient)
    {
	SelectionInfoRec info = { pSel, NullClient, SelectionSetOwner };
	xEvent           event;

	event.u.u.type = SelectionClear;
	event.u.selectionClear.time = currentTime.milliseconds;
	event.u.selectionClear.window = pSel->window;
	event.u.selectionClear.atom = pSel->selection;

	TryClientEvents (pSel->client, NULL, &event, 1, NoEventMask,
			 NoEventMask /* CantBeFiltered */, NullGrab);

	pSel->lastTimeChanged = currentTime;
	pSel->window = dmxScreen->selectionProxyWid[0];
	pSel->pWin = NULL;
	pSel->client = NullClient;

	CallCallbacks (&SelectionCallback, &info);

	pSel->window = None;
    }
}

void
dmxSelectionNotify (ScreenPtr pScreen,
		    Window    requestor,
		    Atom      xSelection,
		    Atom      xTarget,
		    Atom      xProperty,
		    Time      xTime)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    Atom          selection = dmxSelectionAtom (dmxScreen, xSelection);
    Atom          target = dmxAtom (dmxScreen, xTarget);
    DMXSelection  *s;

    for (s = convHead; s; s = s->next)
	if (s->value[pScreen->myNum].out == requestor &&
	    s->selection                 == selection &&
	    s->target                    == target)
	    break;

    if (s)
    {
	xcb_get_property_cookie_t cookie = { 0 };
	Atom                      property = dmxAtom (dmxScreen, xProperty);
	Atom                      target = dmxAtom (dmxScreen, xTarget);

	if (ValidAtom (property) && ValidAtom (target))
	    cookie = xcb_get_property (dmxScreen->connection,
				       xFalse,
				       requestor,
				       xProperty,
				       XCB_GET_PROPERTY_TYPE_ANY,
				       0,
				       0xffffffff);

	if (cookie.sequence)
	{
	    const uint32_t value = XCB_EVENT_MASK_PROPERTY_CHANGE;

	    dmxUnhookSelection (s, &convHead, &convTail);

	    memset (s->value, 0, sizeof (s->value));

	    s->value[pScreen->myNum].out = requestor;
	    s->value[pScreen->myNum].in = cookie.sequence;

	    s->property = property;
	    s->target = target;

	    if (property == dmxScreen->incrAtom)
		xcb_change_window_attributes (dmxScreen->connection,
					      requestor,
					      XCB_CW_EVENT_MASK,
					      &value);

	    dmxAppendSelection (s, &propTail);

	    dmxAddSequence (&dmxScreen->request, cookie.sequence);
	}
	else
	{
	    int i;
	    
	    s->value[pScreen->myNum].out = 0;

	    for (i = 0; i < dmxNumScreens; i++)
		if (s->value[i].out)
		    break;

	    if (i == dmxNumScreens)
		dmxSelectionDeleteConv (dmxUnhookSelection (s,
							    &convHead,
							    &convTail));
	}
    }
}

Bool
dmxSelectionPropertyNotify (ScreenPtr pScreen,
			    Window    window,
			    int       state,
			    Atom      xProperty,
			    Time      xTime)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    Atom          property = dmxAtom (dmxScreen, xProperty);
    DMXSelection  *s;

    if (property != dmxScreen->incrAtom)
	return FALSE;

    if (state == XCB_PROPERTY_NEW_VALUE)
    {
	for (s = propHead; s; s = s->next)
	    if (s->value[pScreen->myNum].out == window &&
		s->property                  == dmxScreen->incrAtom)
		break;

	if (s)
	{
	    xcb_get_property_cookie_t cookie = { 0 };

	    cookie = xcb_get_property (dmxScreen->connection,
				       xFalse,
				       window,
				       xProperty,
				       XCB_GET_PROPERTY_TYPE_ANY,
				       0,
				       0xffffffff);

	    if (cookie.sequence)
	    {
		s->value[pScreen->myNum].in = cookie.sequence;
		dmxAddSequence (&dmxScreen->request, cookie.sequence);
		dmxSelectionResetTimer (s);
	    }
	    else
	    {
		dmxSelectionDeleteProp (dmxUnhookSelection (s,
							    &propHead,
							    &propTail));
	    }
	}
    }
    else
    {
	for (s = reqHead; s; s = s->next)
	    if (s->value[pScreen->myNum].out == window &&
		s->property                  == dmxScreen->incrAtom)
		break;

	if (s)
	{
	    WindowPtr pWin;

	    if (dixLookupWindow (&pWin,
				 s->wid,
				 serverClient,
				 DixReadAccess) == Success)
		DeleteProperty (serverClient, pWin, s->property);

	    dmxSelectionResetTimer (s);
	}
    }

    return TRUE;
}

Bool
dmxSelectionPropertyReplyCheck (ScreenPtr           pScreen,
				unsigned int        sequence,
				xcb_generic_reply_t *reply)
{
    DMXSelection *s;

    for (s = propHead; s; s = s->next)
	if (s->value[pScreen->myNum].in == sequence)
	    break;

    if (s)
    {
	WindowPtr                pWin = NullWindow;
	xcb_get_property_reply_t *xproperty =
	    (xcb_get_property_reply_t *) reply;

	if (dixLookupWindow (&pWin,
			     s->wid,
			     serverClient,
			     DixReadAccess) == Success)
	{
	    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
	    xEvent        event;

	    event.u.selectionNotify.property = None;

	    if (reply->response_type)
	    {
		Atom     type = dmxAtom (dmxScreen, xproperty->type);
		uint32_t *data = xcb_get_property_value (xproperty);
		int      length = xcb_get_property_value_length (xproperty);

		/* only 32 bit data types can be translated */
		if (xproperty->format == 32)
		{
		    int i;

		    switch (type) {
		    case XA_ATOM:
			for (i = 0; i < length; i++)
			    data[i] = dmxAtom (dmxScreen, data[i]);
			break;
		    case XA_BITMAP:
		    case XA_PIXMAP:
		    case XA_COLORMAP:
		    case XA_CURSOR:
		    case XA_DRAWABLE:
		    case XA_FONT:
		    case XA_VISUALID:
		    case XA_WINDOW:
			/* XXX: there's no guarantee that properties of these
			   types can be converted as all back-end resources
			   don't exist on this server.
			*/
			type = 0;
		    default:
			break;
		    }
		}

		if (ValidAtom (type) &&
		    ChangeWindowProperty (pWin,
					  s->property,
					  type,
					  xproperty->format,
					  PropModeReplace,
					  length,
					  data,
					  TRUE) == Success)
		    event.u.selectionNotify.property = s->property;
	    }

	    if (s->selection)
	    {
		event.u.u.type = SelectionNotify | 0x80;
		event.u.selectionNotify.time = s->time;
		event.u.selectionNotify.requestor = s->wid;
		event.u.selectionNotify.selection = s->selection;
		event.u.selectionNotify.target = s->target;

		TryClientEvents (wClient (pWin), NULL, &event, 1, NoEventMask,
				 NoEventMask /* CantBeFiltered */, NullGrab);
	    }

	    if (event.u.selectionNotify.property == dmxScreen->incrAtom)
	    {	
		/* end of incremental selection transfer when size is 0 */
		if (xproperty->value_len != 0)
		{
		    /* don't send another selection notify event */
		    s->selection = None;
		    dmxSelectionResetTimer (s);

		    return TRUE;
		}
	    }
	}

	dmxSelectionDeleteProp (dmxUnhookSelection (s,
						    &propHead,
						    &propTail));

	return TRUE;
    }

    return FALSE;
}

void
dmxSelectionRequest (ScreenPtr pScreen,
		     Window    owner,
		     Window    requestor,
		     Atom      xSelection,
		     Atom      xTarget,
		     Atom      xProperty,
		     Time      xTime)
{
    DMXScreenInfo                *dmxScreen = &dmxScreens[pScreen->myNum];
    WindowPtr                    pChild0, pChildN;
    xcb_selection_notify_event_t xevent;
    Selection                    *pSel = NULL;
    Atom                         selection = dmxSelectionAtom (dmxScreen,
							       xSelection);

    pChild0 = WindowTable[0];
    pChildN = WindowTable[pScreen->myNum];

    for (;;)
    {
	dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV (pChildN);

	if (pWinPriv->window == owner)
	{
	    if (ValidAtom (selection))
		dixLookupSelection (&pSel,
				    selection,
				    serverClient,
				    DixReadAccess);
	    break;
	}

	if (pChild0->firstChild)
	{
	    pChild0 = pChild0->firstChild;
	    pChildN = pChildN->firstChild;
	    continue;
	}

	while (!pChild0->nextSib && (pChild0 != WindowTable[0]))
	{
	    pChild0 = pChild0->parent;
	    pChildN = pChildN->parent;
	}

	if (pChild0 == WindowTable[0])
	    break;

	pChild0 = pChild0->nextSib;
	pChildN = pChildN->nextSib;
    }

    if (pSel)
    {
	Atom target = dmxAtom (dmxScreen, xTarget);
	Atom property = (xProperty) ? dmxAtom (dmxScreen, xProperty) : None;

	if (ValidAtom (target) && ((property == None) || ValidAtom (property)))
	{
	    WindowPtr    pProxy = NullWindow;
	    DMXSelection *s;
	    int          i;

	    for (i = 1; i < DMX_N_SELECTION_PROXY; i++)
	    {
		pProxy = dmxScreens[0].pSelectionProxyWin[i];

		for (s = reqHead; pProxy && s; s = s->next)
		    if (s->wid == pProxy->drawable.id)
			pProxy = NullWindow;

		if (pProxy)
		    break;
	    }

	    if (pProxy)
	    {
		xEvent event;

		event.u.u.type = SelectionRequest;
		event.u.selectionRequest.owner = pSel->window;
		event.u.selectionRequest.time = currentTime.milliseconds;
		event.u.selectionRequest.requestor = pProxy->drawable.id;
		event.u.selectionRequest.selection = selection;
		event.u.selectionRequest.target = target;
		event.u.selectionRequest.property = property;

		s = xalloc (sizeof (DMXSelection));
		if (s)
		{
		    if (TryClientEvents (pSel->client, NULL, &event, 1,
					 NoEventMask,
					 NoEventMask /* CantBeFiltered */,
					 NullGrab))
		    {
			int j;

			s->wid       = pProxy->drawable.id;
			s->requestor = requestor;
			s->selection = selection;
			s->target    = target;
			s->property  = property;
			s->time      = xTime;
			s->next      = 0;
			s->timer     = 0;

			memset (s->value, 0, sizeof (s->value));

			for (j = 0; j < dmxNumScreens; j++)
			{
			    WindowPtr     pWin =
				dmxScreens[j].pSelectionProxyWin[i];
			    dmxWinPrivPtr pWinPriv =
				DMX_GET_WINDOW_PRIV (pWin);
			    
			    s->value[j].in = pWinPriv->window;
			    if (j == pScreen->myNum)
				s->value[j].out = requestor;
			}

			dmxAppendSelection (s, &reqTail);
			return;
		    }

		    xfree (s);
		}
	    }
	    else
	    {
		/* TODO: wait for proxy window to become available */
		dmxLog (dmxWarning,
			"dmxSelectionRequest: no proxy window available "
			"for conversion of %s selection\n",
			NameForAtom (selection));
	    }
	}
    }
	
    xevent.response_type = XCB_SELECTION_NOTIFY;
    xevent.pad0 = 0;
    xevent.sequence = 0;
    xevent.time = xTime;
    xevent.requestor = requestor;
    xevent.selection = xSelection;
    xevent.target = xTarget;
    xevent.property = 0;

    xcb_send_event (dmxScreen->connection,
		    FALSE,
		    requestor,
		    0,
		    (const char *) &xevent);

    dmxSync (dmxScreen, FALSE);
}

void
dmxSelectionPropertyChangeCheck (WindowPtr pWin,
				 Atom      property,
				 int       nUnits)
{
    DMXSelection *s;

    /* check for end of incremental selection conversion */
    for (s = reqHead; s; s = s->next)
	if (s->wid                 == pWin->drawable.id &&
	    dmxScreens[0].incrAtom == property &&
	    nUnits                 == 0)
	    break;

    if (s)
	dmxSelectionDeleteReq (dmxUnhookSelection (s,
						   &reqHead,
						   &reqTail));
}

Bool
dmxCreateSelectionProxies (void)
{
    WindowPtr pWin;
    WindowPtr pParent;
    XID       selectionProxyWid;
    XID       overrideRedirect = TRUE;
    int	      result;
    int       i;

    for (i = 0; i < DMX_N_SELECTION_PROXY; i++)
    {
	if (dmxScreens[0].selectionProxyWid[i])
	    continue;

	selectionProxyWid = FakeClientID (0);

#ifdef PANORAMIX
	if (!noPanoramiXExtension)
	{
	    PanoramiXRes *newWin;
	    int          j;

	    if(!(newWin = (PanoramiXRes *) xalloc (sizeof (PanoramiXRes))))
		return BadAlloc;

	    newWin->type = XRT_WINDOW;
	    newWin->u.win.visibility = VisibilityNotViewable;
	    newWin->u.win.class = InputOnly;
	    newWin->u.win.root = FALSE;
	    newWin->info[0].id = selectionProxyWid;
	    for(j = 1; j < PanoramiXNumScreens; j++)
		newWin->info[j].id = FakeClientID (0);

	    FOR_NSCREENS_BACKWARD(j) {
		if (i)
		    pParent = dmxScreens[j].pSelectionProxyWin[0];
		else
		    pParent = WindowTable[j];

		pWin = CreateWindow (newWin->info[j].id, pParent,
				     0, 0, 1, 1, 0, InputOnly, 
				     CWOverrideRedirect, &overrideRedirect,
				     0, serverClient, CopyFromParent, 
				     &result);
		if (result != Success)
		    return FALSE;
		if (!AddResource (pWin->drawable.id, RT_WINDOW, pWin))
		    return FALSE;

		dmxScreens[j].selectionProxyWid[i] = selectionProxyWid;
		dmxScreens[j].pSelectionProxyWin[i] = pWin;
	    }

	    AddResource(newWin->info[0].id, XRT_WINDOW, newWin);
	}
	else
#endif
	
	{
	    if (i)
		pParent = dmxScreens[0].pSelectionProxyWin[0];
	    else
		pParent = WindowTable[0];

	    pWin = CreateWindow (selectionProxyWid, pParent,
				 0, 0, 1, 1, 0, InputOnly, 
				 CWOverrideRedirect, &overrideRedirect,
				 0, serverClient, CopyFromParent, 
				 &result);
	    if (result != Success)
		return FALSE;
	    if (!AddResource (pWin->drawable.id, RT_WINDOW, pWin))
		return FALSE;

	    dmxScreens[0].selectionProxyWid[i] = selectionProxyWid;
	    dmxScreens[0].pSelectionProxyWin[i] = pWin;
	}
    }

    return TRUE;
}

static int (*dmxSaveProcVector[256]) (ClientPtr);

static int
dmxProcSetSelectionOwner (ClientPtr client)
{
    WindowPtr    pOld = NullWindow;
    WindowPtr    pWin;
#ifdef PANORAMIX
    PanoramiXRes *win = NULL;
#endif
    Selection    *pSel;
    int          err;
    int          i;

    REQUEST(xSetSelectionOwnerReq);

    if (dixLookupSelection (&pSel,
			    stuff->selection,
			    serverClient,
			    DixReadAccess) == Success)
	pOld = pSel->pWin;

    err = (*dmxSaveProcVector[X_SetSelectionOwner]) (client);
    if (err != Success)
	return err;

    if (!pSel && dixLookupSelection (&pSel,
				     stuff->selection,
				     serverClient,
				     DixReadAccess) != Success)
	return Success;

    if (pSel->pWin)
    {

#ifdef PANORAMIX
	if (!noPanoramiXExtension)
	    win = (PanoramiXRes *) SecurityLookupIDByType (serverClient,
							   pSel->window,
							   XRT_WINDOW,
							   DixReadAccess);
	else
#endif
	    dixLookupWindow (&pWin, pSel->window, serverClient, DixReadAccess);
    }
    else if (!pOld)
    {
	/* avoid setting selection owner to none on back-end servers
	   when we're not the current owner */
	return Success;
    }

    for (i = 0; i < dmxNumScreens; i++)
    {
	ScreenPtr pScreen = screenInfo.screens[i];

#ifdef PANORAMIX
	if (!noPanoramiXExtension)
	{
	    pWin = NullWindow;
	    if (win)
		dixLookupWindow (&pWin,
				 win->info[i].id,
				 serverClient,
				 DixReadAccess);

	    dmxBESetSelectionOwner (pScreen, pWin, pSel->selection);
	}
	else
#endif
	{
	    if (pWin && pWin->drawable.pScreen != pScreen)
		continue;

	    dmxBESetSelectionOwner (pScreen, pWin, pSel->selection);
	}
    }

    return Success;
}

static int
dmxProcGetSelectionOwner (ClientPtr client)
{
    xGetSelectionOwnerReply reply;
    Selection               *pSel;
    int                     rc;

    REQUEST(xResourceReq);
    REQUEST_SIZE_MATCH(xResourceReq);

    if (!ValidAtom (stuff->id))
    {
	client->errorValue = stuff->id;
        return BadAtom;
    }

    rc = dixLookupSelection (&pSel, stuff->id, client, DixGetAttrAccess);
    if (rc == Success)
    {
	if (pSel->window != None)
	    return (*dmxSaveProcVector[X_GetSelectionOwner]) (client);
    }
    else if (rc != BadMatch)
    {
	return rc;
    }

    reply.type = X_Reply;
    reply.length = 0;
    reply.sequenceNumber = client->sequence;
    reply.owner = None;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	unsigned int sequence;
	int          j;

	FOR_NSCREENS(j) {
	    dmxBEGetSelectionOwner (screenInfo.screens[j], stuff->id);
	}

	do {
	    dmxDispatch ();

	    sequence = 0;

	    FOR_NSCREENS_BACKWARD(j) {
		DMXScreenInfo *dmxScreen = &dmxScreens[j];

		if (dmxScreen->getSelectionOwnerResult)
		    break;

		sequence |= dmxScreen->getSelectionOwner.sequence;
	    }
	} while (sequence && j < 0 && dmxWaitForResponse ());

	/* at least one back-end server has an owner for this selection */
	if (j >= 0)
	    reply.owner = dmxScreens[0].selectionProxyWid[0];
    }
#endif

    WriteReplyToClient (client, sizeof (xGetSelectionOwnerReply), &reply);
    return client->noClientException;
}

static int
dmxProcConvertSelection (ClientPtr client)
{
    DMXSelection *s;
    Bool         paramsOkay;
    xEvent       event;
    WindowPtr    pWin;
    Selection    *pSel;
    int          rc;
#ifdef PANORAMIX
    PanoramiXRes *win = NULL;
    int          j;
#endif

    REQUEST(xConvertSelectionReq);
    REQUEST_SIZE_MATCH(xConvertSelectionReq);

    rc = dixLookupWindow (&pWin, stuff->requestor, client, DixSetAttrAccess);
    if (rc != Success)
        return rc;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	if (!(win = (PanoramiXRes *) SecurityLookupIDByType (serverClient,
							     stuff->requestor,
							     XRT_WINDOW,
							     DixReadAccess)))
	    return BadImplementation;
    }
#endif

    paramsOkay = ValidAtom (stuff->selection) && ValidAtom (stuff->target);
    paramsOkay &= (stuff->property == None) || ValidAtom (stuff->property);
    if (!paramsOkay)
    {
	client->errorValue = stuff->property;
        return BadAtom;
    }

    s = xalloc (sizeof (DMXSelection));
    if (!s)
	return BadAlloc;

    s->wid       = 0;
    s->requestor = 0;
    s->selection = stuff->selection;
    s->target    = stuff->target;
    s->property  = stuff->property;
    s->time      = stuff->time;
    s->next      = 0;
    s->timer     = 0;

    memset (s->value, 0, sizeof (s->value));

    rc = dixLookupSelection (&pSel, stuff->selection, client, DixReadAccess);
    if (rc == Success)
    {
	if (pSel->window != None)
	{

#ifdef PANORAMIX
	    if (!noPanoramiXExtension)
	    {
		FOR_NSCREENS_FORWARD(j) {
		    if (dixLookupWindow (&pWin,
					 win->info[j].id,
					 serverClient,
					 DixReadAccess) == Success)
			s->value[j].in = DMX_GET_WINDOW_PRIV (pWin)->window;
		}
	    }
	    else
#endif
		s->value[pWin->drawable.pScreen->myNum].in =
		    DMX_GET_WINDOW_PRIV (pWin)->window;

	    dmxAppendSelection (s, &reqTail);

	    return (*dmxSaveProcVector[X_ConvertSelection]) (client);
	}
    }
    else if (rc != BadMatch)
    {
	xfree (s);
	return rc;
    }

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	FOR_NSCREENS_FORWARD(j) {
	    if (dixLookupWindow (&pWin,
				 win->info[j].id,
				 serverClient,
				 DixReadAccess) == Success)
		s->value[j].out = dmxBEConvertSelection (pWin,
							 stuff->selection,
							 stuff->target,
							 stuff->property,
							 stuff->time);
	}
    }
    else
#endif
	s->value[pWin->drawable.pScreen->myNum].out =
	    dmxBEConvertSelection (pWin,
				   stuff->selection,
				   stuff->target,
				   stuff->property,
				   stuff->time);

    for (j = 0; j < dmxNumScreens; j++)
	if (s->value[j].out)
	    break;

    if (j < dmxNumScreens)
    {
	s->wid = stuff->requestor;

	dmxAppendSelection (s, &convTail);

	return client->noClientException;
    }

    xfree (s);

    event.u.u.type = SelectionNotify;
    event.u.selectionNotify.time = stuff->time;
    event.u.selectionNotify.requestor = stuff->requestor;
    event.u.selectionNotify.selection = stuff->selection;
    event.u.selectionNotify.target = stuff->target;
    event.u.selectionNotify.property = None;

    TryClientEvents (client, NULL, &event, 1, NoEventMask,
		     NoEventMask /* CantBeFiltered */, NullGrab);
    return client->noClientException;
}

static int
dmxProcSendEvent (ClientPtr client)
{
    REQUEST(xSendEventReq);
    REQUEST_SIZE_MATCH(xSendEventReq);

    if ((stuff->propagate != xFalse) && (stuff->propagate != xTrue))
    {
	client->errorValue = stuff->propagate;
	return BadValue;
    }

    switch (stuff->event.u.u.type) {
    case SelectionNotify:
	if (stuff->eventMask   == NoEventMask   &&
	    stuff->destination != PointerWindow &&
	    stuff->destination != InputFocus)
	{
	    Atom         property = stuff->event.u.selectionNotify.property;
	    Atom         target = stuff->event.u.selectionNotify.target;
	    DMXSelection *s;

	    for (s = reqHead; s; s = s->next)
		if (s->wid       == stuff->destination &&
		    s->selection == stuff->event.u.selectionNotify.selection)
		    break;

	    if (s)
	    {
		int i;

		for (i = 0; i < dmxNumScreens; i++)
		{
		    xcb_selection_notify_event_t xevent;
		    DMXScreenInfo                *dmxScreen = &dmxScreens[i];

		    if (!s->value[i].out)
			continue;
		    
		    xevent.response_type = XCB_SELECTION_NOTIFY;
		    xevent.pad0 = 0;
		    xevent.sequence = 0;
		    xevent.time = s->time;
		    xevent.requestor = s->requestor;
		    xevent.selection = dmxBESelectionAtom (dmxScreen,
							   s->selection);
		    if (target)
			xevent.target = dmxBEAtom (dmxScreen, target);
		    else
			xevent.target = None;

		    if (property)
			xevent.property = dmxBEAtom (dmxScreen, property);
		    else
			xevent.property = None;

		    if (property == dmxScreen->incrAtom)
		    {
			const uint32_t value = XCB_EVENT_MASK_PROPERTY_CHANGE;

			xcb_change_window_attributes (dmxScreen->connection,
						      s->requestor,
						      XCB_CW_EVENT_MASK,
						      &value);

			s->property = dmxScreen->incrAtom;
		    }

		    xcb_send_event (dmxScreen->connection,
				    FALSE,
				    s->requestor,
				    0,
				    (const char *) &xevent);

		    dmxSync (dmxScreen, FALSE);

		    if (property == dmxScreen->incrAtom)
			dmxSelectionResetTimer (s);
		    else
			dmxSelectionDeleteReq (dmxUnhookSelection (s,
								   &reqHead,
								   &reqTail));

		    return client->noClientException;
		}
	    
		s->property = property;
		if (property == dmxScreens[0].incrAtom)
		    dmxSelectionResetTimer (s);
		else
		    dmxSelectionDeleteReq (dmxUnhookSelection (s,
							       &reqHead,
							       &reqTail));
	    }
	}
	break;
    case ClientMessage:
	if (stuff->event.u.u.detail != 8 &&
	    stuff->event.u.u.detail != 16 &&
	    stuff->event.u.u.detail != 32)
	{
	    client->errorValue = stuff->event.u.u.detail;
	    return BadValue;
	}

	if (stuff->destination == dmxScreens[0].selectionProxyWid[0])
	    dmxDnDClientMessageEvent (&stuff->event);

	break;
    }

    return (*dmxSaveProcVector[X_SendEvent]) (client);
}

void
dmxInitSelections (void)
{
    int i;

    for (i = 0; i < 256; i++)
	dmxSaveProcVector[i] = ProcVector[i];

    ProcVector[X_GetSelectionOwner] = dmxProcGetSelectionOwner;
    ProcVector[X_SetSelectionOwner] = dmxProcSetSelectionOwner;
    ProcVector[X_ConvertSelection]  = dmxProcConvertSelection;
    ProcVector[X_SendEvent]         = dmxProcSendEvent;
}

void
dmxResetSelections (void)
{
    int i;
    

    for (i = 0; i < 256; i++)
	ProcVector[i] = dmxSaveProcVector[i];
}
