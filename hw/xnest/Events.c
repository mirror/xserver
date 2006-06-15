/* $Xorg: Events.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
/*

   Copyright 1993 by Davor Matic

   Permission to use, copy, modify, distribute, and sell this software
   and its documentation for any purpose is hereby granted without fee,
   provided that the above copyright notice appear in all copies and that
   both that copyright notice and this permission notice appear in
   supporting documentation.  Davor Matic makes no representations about
   the suitability of this software for any purpose.  It is provided "as
   is" without express or implied warranty.

*/
/* $XFree86: xc/programs/Xserver/hw/xnest/Events.c,v 1.2 2001/08/01 00:44:57 tsi Exp $ */

#ifdef HAVE_XNEST_CONFIG_H
#include <xnest-config.h>
#endif

#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#define NEED_EVENTS
#include <X11/XCB/xproto.h>
#include "screenint.h"
#include "input.h"
#include "misc.h"
#include "scrnintstr.h"
#include "windowstr.h"
#include "servermd.h"

#include "mi.h"

#include "Xnest.h"

#include "Args.h"
#include "Color.h"
#include "Display.h"
#include "Screen.h"
#include "XNWindow.h"
#include "Events.h"
#include "Keyboard.h"
#include "mipointer.h"

CARD32 lastEventTime = 0;

void ProcessInputEvents()
{
    mieqProcessInputEvents();
    miPointerUpdate();
}

int TimeSinceLastInputEvent()
{
    if (lastEventTime == 0)
        lastEventTime = GetTimeInMillis();
    return GetTimeInMillis() - lastEventTime;
}

void SetTimeSinceLastInputEvent()
{
    lastEventTime = GetTimeInMillis();
}

static Bool xnestExposurePredicate(Display *display, XEvent *event, char *args)
{
    return (event->type == Expose || event->type == ProcessedExpose);
}

static Bool xnestNotExposurePredicate(Display *display, XEvent *event, char *args)
{
    return !xnestExposurePredicate(display, event, args);
}

/*void xnestCollectExposures()
{
    XCBGenericEvent *e;
    XCBExposeEvent *evt;
    WindowPtr pWin;
    RegionRec Rgn;
    BoxRec Box;

    e = XCBPeekNextEvent(xnestConnection);
    while ((e->response_type & ~0x80) == XCBExpose) {
        evt = (XCBExposeEvent *)XCBWaitForEvent(xnestConnection);
        pWin = xnestWindowPtr(evt->window);

        if (pWin) {
            Box.x1 = pWin->drawable.x + wBorderWidth(pWin) + evt->x;
            Box.y1 = pWin->drawable.y + wBorderWidth(pWin) + evt->y;
            Box.x2 = Box.x1 + evt->width;
            Box.y2 = Box.y1 + evt->height;

            REGION_INIT(pWin->drawable.pScreen, &Rgn, &Box, 1);

            miWindowExposures(pWin, &Rgn, NullRegion); 
        }
        e = XCBPeekNextEvent(xnestConnection);
    }
}*/

void xnestQueueKeyEvent(int type, unsigned int keycode)
{
    xEvent x;
    x.u.u.type = type;
    x.u.u.detail = keycode;
    x.u.keyButtonPointer.time = lastEventTime = GetTimeInMillis();
    mieqEnqueue(&x);
}

void xnestHandleEvent(XCBGenericEvent *e)
{
    XCBMotionNotifyEvent *pev;
    XCBEnterNotifyEvent  *eev;
    XCBLeaveNotifyEvent  *lev;    
    XCBExposeEvent       *xev;
    XCBGenericEvent ev;
    ScreenPtr pScreen;
    WindowPtr pWin;
    RegionRec Rgn;
    BoxRec Box;


    switch (e->response_type & ~0x80) {
        case XCBKeyPress:
            xnestUpdateModifierState(((XCBKeyPressEvent *)e)->state);
            xnestQueueKeyEvent(XCBKeyPress, ((XCBKeyPressEvent *)e)->detail.id);
            break;

        case XCBKeyRelease:
            xnestUpdateModifierState(((XCBKeyReleaseEvent *)e)->state);
            xnestQueueKeyEvent(KeyRelease, ((XCBKeyReleaseEvent *)e)->detail.id);
            break;

        case XCBButtonPress:
            xnestUpdateModifierState(((XCBButtonPressEvent *)e)->state);
            ((XCBButtonPressEvent *)e)->time.id = lastEventTime = GetTimeInMillis();
            memcpy(&ev, e, sizeof(XCBGenericEvent));
            mieqEnqueue((xEventPtr) &ev);
            break;

        case XCBButtonRelease:
            xnestUpdateModifierState(((XCBButtonReleaseEvent *)e)->state);
            ((XCBButtonReleaseEvent *)e)->time.id = lastEventTime = GetTimeInMillis();
            memcpy(&ev, e, sizeof(XCBGenericEvent));
            mieqEnqueue((xEventPtr) &ev);
            break;

        case XCBMotionNotify:
#if 0
            x.u.u.type = MotionNotify;
            x.u.keyButtonPointer.rootX = X.xmotion.x;
            x.u.keyButtonPointer.rootY = X.xmotion.y;
            x.u.keyButtonPointer.time = lastEventTime = GetTimeInMillis();
            mieqEnqueue(&x);
#endif 
            pev = (XCBMotionNotifyEvent *)e;
            miPointerAbsoluteCursor (pev->event_x, pev->event_y,
                    lastEventTime = GetTimeInMillis());
            break;

        case XCBFocusIn:
            if (((XCBFocusInEvent *)e)->detail != XCBNotifyDetailInferior) {
                pScreen = xnestScreen(((XCBFocusInEvent *)e)->event);
                if (pScreen)
                    xnestDirectInstallColormaps(pScreen);
            }
            break;

        case XCBFocusOut:
            if (((XCBFocusOutEvent *)e)->detail != XCBNotifyDetailInferior) {
                pScreen = xnestScreen(((XCBFocusOutEvent *)e)->event);
                if (pScreen)
                    xnestDirectInstallColormaps(pScreen);
            }
            break;

        case XCBKeymapNotify:
            break;

        case XCBEnterNotify:
            eev = (XCBEnterNotifyEvent *)e;
            if (eev->detail != XCBNotifyDetailInferior) {
                pScreen = xnestScreen(eev->event);
                if (pScreen) {
                    NewCurrentScreen(pScreen, eev->event_x, eev->event_y);
#if 0
                    x.u.u.type = MotionNotify;
                    x.u.keyButtonPointer.rootX = X.xcrossing.x;
                    x.u.keyButtonPointer.rootY = X.xcrossing.y;
                    x.u.keyButtonPointer.time = lastEventTime = GetTimeInMillis();
                    mieqEnqueue(&x);
#endif
                    miPointerAbsoluteCursor (eev->event_x, eev->event_y, 
                            lastEventTime = GetTimeInMillis());
                    xnestDirectInstallColormaps(pScreen);
                }
            }
            break;

        case XCBLeaveNotify:
            lev = (XCBLeaveNotifyEvent *)e;                
            if (lev->detail != XCBNotifyDetailInferior) {
                pScreen = xnestScreen(lev->event);
                if (pScreen) {
                    xnestDirectUninstallColormaps(pScreen);
                }	
            }
            break;

        case XCBDestroyNotify:
            if (xnestParentWindow.xid != (CARD32) 0 &&
                    ((XCBDestroyNotifyEvent *)e)->event.xid == xnestParentWindow.xid)
                exit (0);
            break;
        case XCBExpose:
            xev = (XCBExposeEvent *)e;
            pWin = xnestWindowPtr(xev->window);
            if (pWin) {
                Box.x1 = pWin->drawable.x + wBorderWidth(pWin) + xev->x;
                Box.y1 = pWin->drawable.y + wBorderWidth(pWin) + xev->y;
                Box.x2 = Box.x1 + xev->width;
                Box.y2 = Box.y1 + xev->height;

                REGION_INIT(pWin->drawable.pScreen, &Rgn, &Box, 1);

                miWindowExposures(pWin, &Rgn, NullRegion); 
            }
            break;
        case XCBNoExposure:
        case XCBGraphicsExposure:
        case XCBCirculateNotify:
        case XCBConfigureNotify:
        case XCBGravityNotify:
        case XCBMapNotify:
        case XCBReparentNotify:
        case XCBUnmapNotify:
            break;

        default:
            ErrorF("xnest warning: unhandled event %d\n", e->response_type & ~0x80);
            ErrorF("Sequence number: %d\n", e->sequence);
            break;
    }
}

void xnestCollectEvents()
{
    XCBGenericEvent *e;
    XCBGenericError *err;
    XCBRequestError *re;
    XCBIDChoiceError *ide;
    XCBFontError     *fe;
    XCBWindowError   *we;

    e = XCBWaitForEvent(xnestConnection);
    while ((e = XCBPollForEvent(xnestConnection, NULL)) != NULL) {
        if (!e->response_type) {
            err = (XCBGenericError *)e;
            ErrorF("File: %s Error: %d, Sequence %d\n", __FILE__, err->error_code, err->sequence);
            switch(err->error_code){
                case XCBMatch:
                    re = (XCBRequestError *)err;
                    ErrorF("XCBMatch: Bad Value %x (Decimal %d)\n", re->bad_value, re->bad_value);
                    break;
                case XCBIDChoice:
                    ide = (XCBIDChoiceError *)err;
                    ErrorF("XCBIDChoice: Bad Value %x (Decimal %d)\n", ide->bad_value, ide->bad_value);
                    break;
                case XCBFont:
                    fe = (XCBFontError *)err;
                    ErrorF("XCBFont: Bad Value %x (Decimal %d)\n", fe->bad_value, fe->bad_value);
                    break;
                case XCBWindow:
                    we = (XCBWindowError *)err;
                    ErrorF("XCBWindow: Bad Value %x (Decimal %d)\n", we->bad_value, we->bad_value);
                    break;
                default:
                    break;
                    }
        }
        else
            xnestHandleEvent(e);
    }
}

