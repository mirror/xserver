/* $Xorg: Events.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
/*

   Copyright 2006 by Ori Bernstein 

   Permission to use, copy, modify, distribute, and sell this software
   and its documentation for any purpose is hereby granted without fee,
   provided that the above copyright notice appear in all copies and that
   both that copyright notice and this permission notice appear in
   supporting documentation.  Davor Matic makes no representations about
   the suitability of this software for any purpose.  It is provided "as
   is" without express or implied warranty.

*/
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
#include "Args.h"
#include "Color.h"
#include "Display.h"
#include "Screen.h"
#include "XNWindow.h"
#include "WindowFuncs.h"
#include "Events.h"
#include "Keyboard.h"
#include "mipointer.h"

void DBG_xnestListWindows(XCBWINDOW w) 
{
    XCBWINDOW *child;
    WindowPtr pWin;
    XCBGenericError *err;
    XCBQueryTreeCookie   qcook;
    XCBQueryTreeRep     *qrep;
    XCBGetGeometryCookie gcook;
    XCBGetGeometryRep   *grep;
    XCBGetGeometryRep    back_grep;
    XCBGetWindowAttributesCookie acook;
    XCBGetWindowAttributesRep   *arep;
    XCBGetWindowAttributesRep    back_arep;

    static int splvl = 0;
    int i,j;


    /*FIXME: THIS IS WRONG! How do I get the screen?
     * No issue though, so far, since I only work with one screen.
     * pScreen = xnestScreen(w);
     */
    qcook = XCBQueryTree(xnestConnection, w);
    qrep = XCBQueryTreeReply(xnestConnection, qcook, &err);
    child = XCBQueryTreeChildren(qrep);
    /* Walk through the windows, initializing the privates.
     * FIXME: initialize x, y, and pWin contents.. how? */
    for (i=0; i < qrep->children_len; i++){
        /*if we're not already tracking this one*/
        pWin = xnestWindowPtr(child[i]);
        for (j=0; j<splvl; j++)
            ErrorF(" ");

        gcook = XCBGetGeometry(xnestConnection, (XCBDRAWABLE)child[i]);
        acook = XCBGetWindowAttributes(xnestConnection, child[i]);

        grep = XCBGetGeometryReply(xnestConnection, gcook, &err);
        if (err) {
            ErrorF("Error %d\n", err->error_code);
            exit(1);
        }

        arep = XCBGetWindowAttributesReply(xnestConnection, acook, &err);
        if (err) {
            ErrorF("Error %d\n", err->error_code);
            exit(1);
        }

        if (pWin) {
            ErrorF("Window %d, Internal %d, (%d, %d), (%d, %d)....(%d, %d), (%d,%d), Event mask 0x%x",
                    child[i].xid, pWin->drawable.id,
                    pWin->drawable.x, pWin->drawable.y,
                    pWin->drawable.width, pWin->drawable.height,
                    grep->x, grep->y,
                    grep->width, grep->height,
                    arep->your_event_mask);
        } else {
            ErrorF("Window %d, <NOT TRACKED INTERNALLY!!!!!!>, (%d, %d), (%d,%d), Event mask 0x%x",
                    grep->x, grep->y,
                    grep->width, grep->height,
                    arep->your_event_mask);
        }
        ErrorF("\n");
        /*and recurse, adding this window's children*/
        splvl++;
        DBG_xnestListWindows(child[i]);
        splvl--;
    }
}

/**
 * Allocates and initializes a window based on the parent window.
 * This function exists because CreateWindow() does too much, namely
 * actually calling pScreen->CreateWindow().
 *
 * This function is used to set up a window that's already been created
 * on the backing server, which means I don't want to actually _create_ it.
 **/
WindowPtr xscreenTrackWindow(XCBWINDOW w, WindowPtr pParent, int x, int y, int width, int height, int bw)
{
    WindowPtr pWin;
    ScreenPtr pScreen;
    XCBWINDOW *child;

    pWin = xnestWindowPtr(w);
    if (pWin && xnestIsRoot(pWin))
        ErrorF("ASDFASDFLKASDJFLKASDJFLKJADSLKJASD");

    pWin = AllocateWindow(pParent->drawable.pScreen);
    if (!pWin) {
        ErrorF("Unable to allocate window (out of RAM?)\n");
        return NULL;
    }
    pWin->prevSib = NullWindow;
    pWin->firstChild = NullWindow;
    pWin->lastChild = NullWindow;

    pWin->valdata = (ValidatePtr)NULL;
    pWin->optional = (WindowOptPtr)NULL;
    pWin->cursorIsNone = TRUE;

    pWin->backingStore = NotUseful;file:///home/ori/.mozilla/firefox/eysrj7gz.default/bookmarks.html
    pWin->DIXsaveUnder = FALSE;
    pWin->backStorage = (pointer) NULL;

    pWin->mapped = FALSE;	    /* off */
    pWin->realized = FALSE;	/* off */
    pWin->viewable = FALSE;
    pWin->visibility = VisibilityNotViewable;
    pWin->overrideRedirect = FALSE;
    pWin->saveUnder = FALSE;

    pWin->bitGravity = ForgetGravity;
    pWin->winGravity = NorthWestGravity;

    pWin->eventMask = 0;
    pWin->deliverableEvents = 0;
    pWin->dontPropagate = 0;
    pWin->forcedBS = FALSE;
#ifdef NEED_DBE_BUF_BITS
    pWin->srcBuffer = DBE_FRONT_BUFFER;
    pWin->dstBuffer = DBE_FRONT_BUFFER;
#endif
#ifdef COMPOSITE
    pWin->redirectDraw = 0;
#endif

    pWin->parent = pParent;
    pWin->drawable = pParent->drawable;

    pWin->origin.x = x + bw;
    pWin->origin.y = y + bw;
    pWin->drawable.width = width;
    pWin->drawable.height = height;
    pWin->borderWidth = bw;
    pWin->drawable.x = pParent->drawable.x + x + bw;
    pWin->drawable.y = pParent->drawable.y + y + bw;   
    pWin->drawable.type = DRAWABLE_WINDOW;
    pWin->drawable.width = width;
    pWin->drawable.height = height;
    pWin->backingStore = NotUseful;
    pWin->backStorage = NULL;
    pWin->backgroundState = 0;

    xnestWindowPriv(pWin)->window = w;
    xnestWindowPriv(pWin)->x = pWin->drawable.x;
    xnestWindowPriv(pWin)->y = pWin->drawable.y;
    xnestWindowPriv(pWin)->width = pWin->drawable.width;
    xnestWindowPriv(pWin)->height = pWin->drawable.height;
    xnestWindowPriv(pWin)->sibling_above = (XCBWINDOW){0};
    xnestWindowPriv(pWin)->owner = XSCREEN_OWNED_BACKING;

    pWin->borderIsPixel = pParent->borderIsPixel;
    pWin->border = pParent->border;
    if (pWin->borderIsPixel == FALSE)
        pWin->border.pixmap->refcnt++;

    wClient(pWin) = serverClient;
    pWin->drawable.id = FakeClientID(0);

    pWin->firstChild = NULL;
    pWin->lastChild = NULL;            
    pWin->prevSib = NULL;
    pWin->nextSib = NULL;
    pWin->optional = NULL;
    pWin->valdata = NULL;

    REGION_NULL(pScreen, &pWin->winSize);
    xnestWindowPriv(pWin)->clip_shape = NULL;
    REGION_NULL(pScreen, &pWin->borderSize);
    REGION_NULL(pScreen, &pWin->clipList);
    REGION_NULL(pScreen, &pWin->borderClip);
    xnestWindowPriv(pWin)->bounding_shape = REGION_CREATE(pWin->drawable.pScreen, NULL, 1);
    xnestWindowPriv(pWin)->clip_shape = REGION_CREATE(pWin->drawable.pScreen, NULL, 1);

    SetWinSize (pWin);
    SetBorderSize (pWin);
    /*FIXME! THIS IS FUCKED. ONLY FOR TESTING Need to actually get the resolution properly.*/
    pWin->drawable.depth = 24;
    return pWin;
}

int xscreenHandleConfigure(WindowPtr pWin, XCBWINDOW sib, int x, int y, int w, int h, int bw)
{
    WindowPtr pSib = NullWindow;
    WindowPtr pParent = pWin->parent;
    xEvent event;

    if (xnestWindowPriv(pWin)->owner == XSCREEN_OWNED_XSCREEN){
        event.u.u.type = ConfigureNotify;
        event.u.configureNotify.window = pWin->drawable.id;
        if (pSib)
            event.u.configureNotify.aboveSibling = sib.xid;
        else
            event.u.configureNotify.aboveSibling = 0;
        event.u.configureNotify.x = x;
        event.u.configureNotify.y = y;
        event.u.configureNotify.width = w;
        event.u.configureNotify.height = h;
        event.u.configureNotify.borderWidth = bw;
        event.u.configureNotify.override = pWin->overrideRedirect;
        DeliverEvents(pWin, &event, 1, NullWindow);
    } else {
        pWin->origin.x = x + bw;
        pWin->origin.y = y + bw;
        pWin->drawable.x = x + bw + pParent->drawable.x;
        pWin->drawable.y = y + bw + pParent->drawable.y;
    }
}

void break_here(void){
    printf("********************************************************BREAK!!!\n");
}

void xnestRemoveWindow(WindowPtr pWin)
{
    WindowPtr pPrev;
    WindowPtr pNext;
    WindowPtr pParent;
    
    pPrev = pWin->prevSib;
    pNext = pWin->nextSib;
    pParent = pWin->parent;

    if (pPrev)
        pPrev->nextSib = pNext;
    else
        pParent->firstChild = pNext;

    if (pNext)
        pNext->prevSib = pPrev;
    else
        pWin->lastChild = pPrev;

    pWin->nextSib = NULL;
    pWin->prevSib = NULL;
}

void xnestInsertWindow(WindowPtr pWin, WindowPtr pParent) 
{
    WindowPtr pPrev;

    pPrev = pParent->firstChild;// RealChildHead(pParent);
    pWin->parent = pParent;
    if (pPrev)
    {
        pWin->nextSib = pPrev->nextSib;
        if (pPrev->nextSib)
            pPrev->nextSib->prevSib = pWin;
        else
            pParent->lastChild = pWin;
        pPrev->nextSib = pWin;
        pWin->prevSib = pPrev;
    }
    else
    {
        pWin->nextSib = pParent->firstChild;
        pWin->prevSib = NullWindow;
        if (pParent->firstChild)
            pParent->firstChild->prevSib = pWin;
        else
            pParent->lastChild = pWin;
        pParent->firstChild = pWin;
    }
    if (pWin->nextSib == pWin)
        break_here();
}


void xscreenTrackChildren(WindowPtr pParent)
{
    int i;
    XCBWINDOW *child;
    XCBQueryTreeCookie   qcook;
    XCBQueryTreeRep     *qrep;
    XCBGetGeometryCookie gcook;
    XCBGetGeometryRep   *grep;
    XCBWINDOW            w = {0};
    ScreenPtr pScreen;
    WindowPtr pWin = NULL;
    WindowPtr pPrev = NULL;
    CARD32 ev_mask;


    /*FIXME: THIS IS WRONG! How do I get the screen?
     * No issue though, so far, since I only work with one screen.
     * pScreen = xnestScreen(w);
     */
    w = xnestWindowPriv(pParent)->window;
    pScreen = screenInfo.screens[0];
    qcook = XCBQueryTree(xnestConnection, w);
    qrep = XCBQueryTreeReply(xnestConnection, qcook, NULL);
    child = XCBQueryTreeChildren(qrep);
    /* Walk through the windows, initializing the privates.
     * FIXME: initialize x, y, and pWin contents.. how? */
    for (i=0; i < qrep->children_len; i++){
        /*if we're not already tracking this one*/
        pWin = xnestWindowPtr(child[i]);
        if (!pWin) {
            ErrorF("Adding window %d\n", child[i]);
            gcook = XCBGetGeometry(xnestConnection, (XCBDRAWABLE)child[i]);
            grep = XCBGetGeometryReply(xnestConnection, gcook, NULL);

            pWin = xscreenTrackWindow(child[i], pParent, grep->x, grep->y, grep->width, grep->height, grep->border_width);

            /*listen to events on the new window*/
            ev_mask = XCBEventMaskSubstructureNotify|XCBEventMaskStructureNotify;;
            XCBChangeWindowAttributes(xnestConnection, child[i], XCBCWEventMask, &ev_mask);
        } else {
            ErrorF("*****Skipping %d\n", child[i]);
        }

        if (xnestWindowPriv(pWin)->owner != XSCREEN_OWNED_BACKING && pWin->parent)
            xnestRemoveWindow(pWin);
        xnestInsertWindow(pWin, pParent);
        /*and recurse, adding this window's children*/
        xscreenTrackChildren(pWin);
    }
}
/**
 * YAY! copy-paste coding.
 * Again, the reparenting window code almost does what I want, but not quite.
 *
 * If I use the DIX impementation, I get an infinite loop of reparent events.
 **/

int xnestReparentWindow(register WindowPtr pWin, register WindowPtr pParent,
        int x, int y, ClientPtr client)
{
    WindowPtr pPrev, pPriorParent;
//    Bool WasMapped = (Bool)(pWin->mapped);
    int bw = wBorderWidth (pWin);
    register ScreenPtr pScreen;

    pScreen = pWin->drawable.pScreen;

//    if (WasMapped)
//        UnmapWindow(pWin, FALSE);


    /* take out of sibling chain */
    pPriorParent = pPrev = pWin->parent;
    if (pPrev->firstChild == pWin)
        pPrev->firstChild = pWin->nextSib;
    if (pPrev->lastChild == pWin)
        pPrev->lastChild = pWin->prevSib;

    if (pWin->nextSib)
        pWin->nextSib->prevSib = pWin->prevSib;
    if (pWin->prevSib)
        pWin->prevSib->nextSib = pWin->nextSib;

    /* insert at begining of pParent */
    pWin->parent = pParent;
    pPrev = pParent->firstChild;//RealChildHead(pParent);
    if (pPrev)
    {
        pWin->nextSib = pPrev->nextSib;
        if (pPrev->nextSib)
            pPrev->nextSib->prevSib = pWin;
        else
            pParent->lastChild = pWin;
        pPrev->nextSib = pWin;
        pWin->prevSib = pPrev;
    }
    else
    {
        pWin->nextSib = pParent->firstChild;
        pWin->prevSib = NullWindow;
        if (pParent->firstChild)
            pParent->firstChild->prevSib = pWin;
        else
            pParent->lastChild = pWin;
        pParent->firstChild = pWin;
    }
    

    pWin->origin.x = x + bw;
    pWin->origin.y = y + bw;
    pWin->drawable.x = x + bw + pParent->drawable.x;
    pWin->drawable.y = y + bw + pParent->drawable.y;

    /* clip to parent */
    SetWinSize (pWin);
    SetBorderSize (pWin);
    (*pScreen->PositionWindow)(pWin, pWin->drawable.x, pWin->drawable.y);
    //ResizeChildrenWinSize(pWin, 0, 0, 0, 0);
    //CheckWindowOptionalNeed(pWin);

//    if (WasMapped)
//        MapWindow(pWin, client);
    RecalculateDeliverableEvents(pWin);
    return(Success);
}

