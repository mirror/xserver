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
#include "dix.h"

#include "Xnest.h"

#include "Args.h"
#include "Color.h"
#include "Display.h"
#include "Screen.h"
#include "XNWindow.h"
#include "WindowFuncs.h"
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
    XCBMotionNotifyEvent    *pev;
    XCBEnterNotifyEvent     *eev;
    XCBLeaveNotifyEvent     *lev;    
    XCBExposeEvent          *xev;
    XCBResizeRequestEvent   *rev;
    XCBConfigureNotifyEvent *cev;
    XCBButtonPressEvent     *bpe;
    XCBButtonReleaseEvent   *bre;
    XCBReparentNotifyEvent  *ev_reparent;
    XCBCreateNotifyEvent    *ev_create;
    XCBMapNotifyEvent       *ev_map;
    XCBUnmapNotifyEvent     *ev_unmap;
    XCBConfigureWindowReq    cfg_req;
    CARD32 ev_mask;
    CARD32 cfg_mask;
    CARD32 cfg_data[7];
    xEvent ev;
    ScreenPtr pScreen;
    WindowPtr pWin;
    WindowPtr pSib;
    WindowPtr pParent;
    WindowPtr pPrev;
    RegionRec Rgn;
    BoxRec Box;
    lastEventTime = GetTimeInMillis();
    int i = 0;
    
    switch (e->response_type & ~0x80) {
        case XCBKeyPress:
            ErrorF("Key Pressed\n");
            xnestUpdateModifierState(((XCBKeyPressEvent *)e)->state);
            ((XCBKeyPressEvent *)e)->time.id = lastEventTime = GetTimeInMillis();
            memcpy(&ev, e, sizeof(XCBGenericEvent));
            mieqEnqueue(&ev);

            //xnestQueueKeyEvent(XCBKeyPress, ((XCBKeyPressEvent *)e)->detail.id);
            break;

        case XCBKeyRelease:
            xnestUpdateModifierState(((XCBKeyReleaseEvent *)e)->state);
            ((XCBKeyReleaseEvent *)e)->time.id = lastEventTime = GetTimeInMillis();
            memcpy(&ev, e, sizeof(XCBGenericEvent));
            mieqEnqueue(&ev);
            //xnestQueueKeyEvent(KeyRelease, ((XCBKeyReleaseEvent *)e)->detail.id);
            break;

        case XCBButtonPress:
            xnestUpdateModifierState(((XCBButtonPressEvent *)e)->state);
            bpe = (XCBButtonPressEvent *)e;
            bpe->time.id = lastEventTime = GetTimeInMillis();
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
            miPointerAbsoluteCursor (pev->root_x, pev->root_y, lastEventTime = GetTimeInMillis());
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
                    ErrorF("Entry Notify\n");
                    XCBTIMESTAMP t = { XCBCurrentTime };
                    XCBSetInputFocus(xnestConnection, RevertToNone, eev->child, t);
                    miPointerAbsoluteCursor (eev->event_x, eev->event_y, lastEventTime = GetTimeInMillis());
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

        case XCBConfigureNotify:
            cev = (XCBConfigureNotifyEvent *)e;
            pWin = xnestWindowPtr(cev->event);
            pSib = xnestWindowPtr(cev->above_sibling);
            pParent = pWin->parent;
            pScreen = pWin->drawable.pScreen;
            
            cfg_mask = (1<<7)-1; 
            cfg_data[i++] = cev->x;
            cfg_data[i++] = cev->y;
            cfg_data[i++] = cev->width;
            cfg_data[i++] = cev->height;
            cfg_data[i++] = cev->border_width;
            if (pSib)
                cfg_data[i++] = pSib ? xnestWindow(pSib).xid : 0;
            else 
                cfg_mask &= ~XCBConfigWindowSibling;
            /*FIXME! WTF do I do for this??*/
            cfg_data[i++] = 0;
            ConfigureWindow(pWin, cfg_mask, cfg_data, wClient(pWin));
            break;
        case XCBReparentNotify:
            /*Reparent windows. This is to track non-xscreen managed windows and their
             * relationship to xscreen managed windows. It should be harmless to poke at 
             * the relationships on xscreen managed windows too, I think.. or will it? FIXME and
             * test.*/
            ev_reparent = (XCBReparentNotifyEvent *)e;
            pParent = xnestWindowPtr(ev_reparent->parent);
            pWin = xnestWindowPtr(ev_reparent->window);

            DBG_xnestListWindows(XCBSetupRootsIter (XCBGetSetup (xnestConnection)).data->root);
            ErrorF("Reparenting %d to %d\n", (int) ev_reparent->window.xid, (int)ev_reparent->parent.xid);
            /*we'll assume the root can't be reparented, and as such, pParent is _always_ valid*/
            xnestReparentWindow(pWin, pParent, ev_reparent->x, ev_reparent->y, wClient(pWin));
            break;

        case XCBCreateNotify:
            ev_create = (XCBCreateNotifyEvent *)e;
            pParent = xnestWindowPtr(ev_create->parent);
            /*make sure we didn't create this window. If we did, ignore it, we already track it*/
            pWin = xnestWindowPtr(ev_create->window);
            if (!pWin) {
                ErrorF("Adding new window\n");

                pWin = xnestTrackWindow(ev_create->window,
                                  pParent, /*parent WindowPtr*/
                                  ev_create->x, ev_create->y, /*x, y*/
                                  ev_create->width, ev_create->height,/*w, h*/
                                  ev_create->border_width);
                if (!pWin) {
                    ErrorF("AAGGHH! NULL WINDOW IN CREATE! SEPPUKU!");
                    exit(1);
                }
                xnestInsertWindow(pWin, pParent);

                ev_mask = XCBEventMaskStructureNotify;
                XCBChangeWindowAttributes(xnestConnection, ev_create->window, XCBCWEventMask, &ev_mask);
            } 
            ErrorF("-- Added win %d\n", (int)pWin->drawable.id);
            break;
        case XCBNoExposure:
        case XCBGraphicsExposure:
        case XCBCirculateNotify:

        case XCBGravityNotify:
#if 0
        case XCBMapNotify:
            ev_map = (XCBMapNotifyEvent *)e;
            pWin = xnestWindowPtr(ev_map->window);
            pWin->mapped = 1;
          //if (!pWin->mapped)
          //    MapWindow(pWin, wClient(pWin));
            break;
        case XCBUnmapNotify:
            ev_unmap = (XCBUnmapNotifyEvent *)e;
            pWin = xnestWindowPtr(ev_unmap->window);            
           // if (pWin->mapped)
           //     UnmapWindow(pWin, ev_unmap->from_configure);
            pWin->mapped = 0;
            break;

#endif
        default:
            ErrorF("****xnest warning: unhandled event %d\n", e->response_type & ~0x80);
            ErrorF("****Sequence number: %d\n", e->sequence);
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

    while ((e = XCBPollForEvent(xnestConnection, NULL)) != NULL) {
        if (!e->response_type) {
            err = (XCBGenericError *)e;
            ErrorF("****** File: %s Error: %d, Sequence %d\n", __FILE__, err->error_code, err->sequence);

        } else {
            xnestHandleEvent(e);
        }
    }
}

