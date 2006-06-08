/* $Xorg: Window.c,v 1.3 2000/08/17 19:53:28 cpqbld Exp $ */
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
/* $XFree86: xc/programs/Xserver/hw/xnest/Window.c,v 3.7 2001/10/28 03:34:11 tsi Exp $ */

#ifdef HAVE_XNEST_CONFIG_H
#include <xnest-config.h>
#endif

#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xcb_aux.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/shape.h>

#include "gcstruct.h"
#include "window.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "colormapst.h"
#include "scrnintstr.h"
#include "region.h"

#include "mi.h"

#include "Xnest.h"

#include "Display.h"
#include "Screen.h"
#include "XNGC.h"
#include "Drawable.h"
#include "Color.h"
#include "Visual.h"
#include "Events.h"
#include "Args.h"

int xnestWindowPrivateIndex;

static int xnestFindWindowMatch(WindowPtr pWin, pointer ptr)
{
    XnestWindowMatch *wm = (XnestWindowMatch *)ptr;
    if (wm->window.xid == xnestWindow(pWin).xid) {
        wm->pWin = pWin;
        return WT_STOPWALKING;
    }
    else
        return WT_WALKCHILDREN;
}

WindowPtr xnestWindowPtr(XCBWINDOW window)
{
    XnestWindowMatch wm;
    int i;

    wm.pWin = NullWindow;
    wm.window = window;

    for (i = 0; i < xnestNumScreens; i++) {
        WalkTree(screenInfo.screens[i], xnestFindWindowMatch, (pointer) &wm);
        if (wm.pWin) break;
    }

    return wm.pWin;
}

Bool xnestCreateWindow(WindowPtr pWin)
{
    unsigned long mask;
    XCBParamsCW param;
    XCBVISUALTYPE *visual;
    XCBVISUALID    vid;
    ColormapPtr pCmap;

    if (pWin->drawable.class == XCBWindowClassInputOnly) {
        mask = 0L;
        visual = XCBCopyFromParent;
    }
    else {
        mask = XCBCWEventMask | XCBCWBackingStore;
        param.event_mask = XCBEventMaskExposure;
        param.backing_store = XCBBackingStoreNotUseful;

        if (pWin->parent) {
            if (pWin->optional && pWin->optional->visual != wVisual(pWin->parent)) {
                vid.id = wVisual(pWin);
                visual = xnestVisualFromID(pWin->drawable.pScreen, vid);
                mask |= XCBCWColormap;
                if (pWin->optional->colormap) {
                    pCmap = (ColormapPtr)LookupIDByType(wColormap(pWin), RT_COLORMAP);
                    param.colormap = xnestColormap(pCmap).xid;
                }
                else
                    param.colormap = xnestDefaultVisualColormap(visual).xid;
            }
            else 
                visual = XCBCopyFromParent;
        }
        else { /* root windows have their own colormaps at creation time */
            vid.id = wVisual(pWin);
            visual = xnestVisualFromID(pWin->drawable.pScreen, vid);      
            pCmap = (ColormapPtr)LookupIDByType(wColormap(pWin), RT_COLORMAP);
            mask |= CWColormap;
            param.colormap = xnestColormap(pCmap).xid;
        }
    }

    xnestWindowPriv(pWin)->window = XCBWINDOWNew(xnestConnection);
    XCBAuxCreateWindow(xnestConnection,
            pWin->drawable.depth, 
            xnestWindowPriv(pWin)->window,
            xnestWindowParent(pWin),
            pWin->origin.x - wBorderWidth(pWin),
            pWin->origin.y - wBorderWidth(pWin),
            pWin->drawable.width,
            pWin->drawable.height,
            pWin->borderWidth,
            pWin->drawable.class,
            visual->visual_id,
            mask,
            &param);
    xnestWindowPriv(pWin)->parent = xnestWindowParent(pWin);
    xnestWindowPriv(pWin)->x = pWin->origin.x - wBorderWidth(pWin);
    xnestWindowPriv(pWin)->y = pWin->origin.y - wBorderWidth(pWin);
    xnestWindowPriv(pWin)->width = pWin->drawable.width;
    xnestWindowPriv(pWin)->height = pWin->drawable.height;
    xnestWindowPriv(pWin)->border_width = pWin->borderWidth;
    xnestWindowPriv(pWin)->sibling_above.xid = 0;
    if (pWin->nextSib)
        xnestWindowPriv(pWin->nextSib)->sibling_above = xnestWindow(pWin);
#ifdef SHAPE
    xnestWindowPriv(pWin)->bounding_shape = 
        REGION_CREATE(pWin->drawable.pScreen, NULL, 1);
    xnestWindowPriv(pWin)->clip_shape = 
        REGION_CREATE(pWin->drawable.pScreen, NULL, 1);
#endif /* SHAPE */

    if (!pWin->parent) /* only the root window will have the right colormap */
        xnestSetInstalledColormapWindows(pWin->drawable.pScreen);

    return True;
}

Bool xnestDestroyWindow(WindowPtr pWin)
{
    if (pWin->nextSib)
        xnestWindowPriv(pWin->nextSib)->sibling_above = 
            xnestWindowPriv(pWin)->sibling_above;
#ifdef SHAPE
    REGION_DESTROY(pWin->drawable.pScreen, 
            xnestWindowPriv(pWin)->bounding_shape);
    REGION_DESTROY(pWin->drawable.pScreen, 
            xnestWindowPriv(pWin)->clip_shape);
#endif
    XCBDestroyWindow(xnestConnection, xnestWindow(pWin));
    xnestWindowPriv(pWin)->window.xid = None;

    if (pWin->optional && pWin->optional->colormap && pWin->parent)
        xnestSetInstalledColormapWindows(pWin->drawable.pScreen);

    return True;
}

Bool xnestPositionWindow(WindowPtr pWin, int x, int y)
{
    xnestConfigureWindow(pWin, 
            XCBConfigWindowSibling | XCBConfigWindowX | XCBConfigWindowY |
            XCBConfigWindowWidth | XCBConfigWindowHeight | XCBConfigWindowBorderWidth); 
    return True;
}

void xnestConfigureWindow(WindowPtr pWin, unsigned int mask)
{
    unsigned int valuemask;
    XCBParamsConfigureWindow values;

    if (mask & XCBConfigWindowSibling &&
            xnestWindowPriv(pWin)->parent.xid != xnestWindowParent(pWin).xid) {
        XCBReparentWindow(xnestConnection,
                xnestWindow(pWin), 
                xnestWindowParent(pWin), 
                pWin->origin.x - wBorderWidth(pWin),
                pWin->origin.y - wBorderWidth(pWin));
        xnestWindowPriv(pWin)->parent = xnestWindowParent(pWin);
        xnestWindowPriv(pWin)->x = pWin->origin.x - wBorderWidth(pWin);
        xnestWindowPriv(pWin)->y = pWin->origin.y - wBorderWidth(pWin);
        xnestWindowPriv(pWin)->sibling_above.xid = None;
        if (pWin->nextSib)
            xnestWindowPriv(pWin->nextSib)->sibling_above = xnestWindow(pWin);
    }

    valuemask = 0;

    if (mask & CWX && xnestWindowPriv(pWin)->x != pWin->origin.x - wBorderWidth(pWin)) {
        valuemask |= CWX;
        values.x = xnestWindowPriv(pWin)->x = pWin->origin.x - wBorderWidth(pWin);
    }

    if (mask & CWY && xnestWindowPriv(pWin)->y != pWin->origin.y - wBorderWidth(pWin)) {
        valuemask |= CWY;
        values.y = xnestWindowPriv(pWin)->y = pWin->origin.y - wBorderWidth(pWin);
    }

    if (mask & CWWidth && xnestWindowPriv(pWin)->width != pWin->drawable.width) {
        valuemask |= CWWidth;
        values.width = xnestWindowPriv(pWin)->width = pWin->drawable.width;
    }

    if (mask & CWHeight && xnestWindowPriv(pWin)->height != pWin->drawable.height) {
        valuemask |= CWHeight;
        values.height = xnestWindowPriv(pWin)->height = pWin->drawable.height;
    }

    if (mask & CWBorderWidth && xnestWindowPriv(pWin)->border_width != pWin->borderWidth) {
        valuemask |= CWBorderWidth; 
        values.border_width = xnestWindowPriv(pWin)->border_width = pWin->borderWidth;
    }

    if (valuemask)
        XCBAuxConfigureWindow(xnestConnection, xnestWindow(pWin), valuemask, &values);  

    if (mask & XCBConfigWindowStackMode && xnestWindowPriv(pWin)->sibling_above.xid != xnestWindowSiblingAbove(pWin)) {
        WindowPtr pSib;

        /* find the top sibling */
        for (pSib = pWin; pSib->prevSib != NullWindow; pSib = pSib->prevSib);

        /* the top sibling */
        valuemask = CWStackMode;
        values.stack_mode = Above;
        XCBAuxConfigureWindow(xnestConnection, xnestWindow(pSib), valuemask, &values); 
        xnestWindowPriv(pSib)->sibling_above.xid = None;

        /* the rest of siblings */
        for (pSib = pSib->nextSib; pSib != NullWindow; pSib = pSib->nextSib) {
            valuemask = CWSibling | CWStackMode;
            values.sibling = xnestWindowSiblingAbove(pSib);
            values.stack_mode = Below;
            XCBAuxConfigureWindow(xnestConnection, xnestWindow(pSib), valuemask, &values);
            xnestWindowPriv(pSib)->sibling_above.xid = xnestWindowSiblingAbove(pSib);
        }
    }
}

Bool xnestChangeWindowAttributes(WindowPtr pWin, unsigned long mask)
{
    XCBParamsCW param;

    if (mask & XCBCWBackPixmap)
        switch (pWin->backgroundState) {
            case None:
                param.back_pixmap = None;
                break;

            case ParentRelative:
                param.back_pixmap = ParentRelative;
                break;

            case BackgroundPixmap:
                param.back_pixmap = xnestPixmap(pWin->background.pixmap).xid;
                break;

            case BackgroundPixel:
                mask &= ~CWBackPixmap;  
                break;
        }

    if (mask & CWBackPixel) {
        if (pWin->backgroundState == BackgroundPixel)
            param.back_pixel = xnestPixel(pWin->background.pixel);
        else
            mask &= ~CWBackPixel;
    }

    if (mask & CWBorderPixmap) {
        if (pWin->borderIsPixel)
            mask &= ~CWBorderPixmap;
        else
            param.border_pixmap = xnestPixmap(pWin->border.pixmap).xid;
    }

    if (mask & CWBorderPixel) {
        if (pWin->borderIsPixel)
            param.border_pixel = xnestPixel(pWin->border.pixel);
        else
            mask &= ~CWBorderPixel;
    }

    if (mask & CWBitGravity) 
        param.bit_gravity = pWin->bitGravity;

    if (mask & CWWinGravity) /* dix does this for us */
        mask &= ~CWWinGravity;

    if (mask & CWBackingStore) /* this is really not useful */
        mask &= ~CWBackingStore;

    if (mask & CWBackingPlanes) /* this is really not useful */
        mask &= ~CWBackingPlanes;

    if (mask & CWBackingPixel) /* this is really not useful */ 
        mask &= ~CWBackingPixel;

    if (mask & CWOverrideRedirect)
        param.override_redirect = pWin->overrideRedirect;

    if (mask & CWSaveUnder) /* this is really not useful */
        mask &= ~CWSaveUnder;

    if (mask & CWEventMask) /* events are handled elsewhere */
        mask &= ~CWEventMask;

    if (mask & CWDontPropagate) /* events are handled elsewhere */
        mask &= ~CWDontPropagate; 

    if (mask & CWColormap) {
        ColormapPtr pCmap;

        pCmap = (ColormapPtr)LookupIDByType(wColormap(pWin), RT_COLORMAP);

        param.colormap = xnestColormap(pCmap).xid;

        xnestSetInstalledColormapWindows(pWin->drawable.pScreen);
    }

    if (mask & CWCursor) /* this is handeled in cursor code */
        mask &= ~CWCursor;

    if (mask)
        XCBAuxChangeWindowAttributes(xnestConnection, xnestWindow(pWin), mask, &param);

    return True;
}	  

Bool xnestRealizeWindow(WindowPtr pWin)
{
    xnestConfigureWindow(pWin, XCBConfigWindowStackMode);
#ifdef SHAPE
    xnestShapeWindow(pWin);
#endif /* SHAPE */
    XCBMapWindow(xnestConnection, xnestWindow(pWin));

    return True;
}

Bool xnestUnrealizeWindow(WindowPtr pWin)
{
    XCBUnmapWindow(xnestConnection, xnestWindow(pWin));
    return True;
}

void xnestPaintWindowBackground(WindowPtr pWin, RegionPtr pRegion, int what)
{
    int i;
    BoxPtr pBox;

    xnestConfigureWindow(pWin, XCBConfigWindowWidth | XCBConfigWindowHeight);

    pBox = REGION_RECTS(pRegion);
    for (i = 0; i < REGION_NUM_RECTS(pRegion); i++)
        XCBClearArea(xnestConnection,
                False,
                xnestWindow(pWin),
                pBox[i].x1 - pWin->drawable.x,
                pBox[i].y1 - pWin->drawable.y,
                pBox[i].x2 - pBox[i].x1, 
                pBox[i].y2 - pBox[i].y1);
}

void xnestPaintWindowBorder(WindowPtr pWin, RegionPtr pRegion, int what)
{
    xnestConfigureWindow(pWin, CWBorderWidth);
}

void xnestCopyWindow(WindowPtr pWin, xPoint oldOrigin, RegionPtr oldRegion)
{
}

void xnestClipNotify(WindowPtr pWin, int dx, int dy)
{
    xnestConfigureWindow(pWin, XCBConfigWindowStackMode); 
#ifdef SHAPE
    xnestShapeWindow(pWin);
#endif /* SHAPE */
}

/*static Bool xnestWindowExposurePredicate(XEvent *event, XCBWINDOW win)
  {
  return (event->type == XCBExpose && event->xexpose.window == *(Window *)ptr);
  }*/

void xnestWindowExposures(WindowPtr pWin, RegionPtr pRgn, RegionPtr other_exposed)
{
    XCBGenericEvent event;
    XCBWINDOW window;
    BoxRec Box;

    XCBSync(xnestConnection, NULL);

    window = xnestWindow(pWin);
    /* Commenting this will probably lead to excessive expose events, but since XCB doesn't give the
     * correct primitives, what to do?
     while (XCheckIfEvent(xnestConnection, &event, xnestWindowExposurePredicate, (char *)&window)) {

     Box.x1 = pWin->drawable.x + wBorderWidth(pWin) + event.xexpose.x;
     Box.y1 = pWin->drawable.y + wBorderWidth(pWin) + event.xexpose.y;
     Box.x2 = Box.x1 + event.xexpose.width;
     Box.y2 = Box.y1 + event.xexpose.height;

     event.xexpose.type = ProcessedExpose;

     if (RECT_IN_REGION(pWin->drawable.pScreen, pRgn, &Box) != rgnIN)
     XPutBackEvent(xnestDisplay, &event);
     }*/

    miWindowExposures(pWin, pRgn, other_exposed);
}

#ifdef SHAPE
void xnestSetShape(WindowPtr pWin)
{
    xnestShapeWindow(pWin);
    miSetShape(pWin);
}

static Bool xnestRegionEqual(RegionPtr pReg1, RegionPtr pReg2)
{
    BoxPtr pBox1, pBox2;
    unsigned int n1, n2;

    if (pReg1 == pReg2) 
        return True;

    if (pReg1 == NullRegion || pReg2 == NullRegion) 
        return False;

    pBox1 = REGION_RECTS(pReg1);
    n1 = REGION_NUM_RECTS(pReg1);

    pBox2 = REGION_RECTS(pReg2);
    n2 = REGION_NUM_RECTS(pReg2);

    if (n1 != n2) 
        return False;

    if (pBox1 == pBox2) 
        return True;

    if (memcmp(pBox1, pBox2, n1 * sizeof(BoxRec))) 
        return False;

    return True;
}

void xnestShapeWindow(WindowPtr pWin)
{
    Region reg;
    BoxPtr pBox;
    XCBPIXMAP pmap;
    XCBRECTANGLE *rect;
    int i;

    if (!xnestRegionEqual(xnestWindowPriv(pWin)->bounding_shape,
                wBoundingShape(pWin))) {

        if (wBoundingShape(pWin)) {
            REGION_COPY(pWin->drawable.pScreen, 
                    xnestWindowPriv(pWin)->bounding_shape, wBoundingShape(pWin));

            pBox = REGION_RECTS(xnestWindowPriv(pWin)->bounding_shape);
            rect = xalloc(sizeof(XCBRECTANGLE) * REGION_NUM_RECTS(xnestWindowPriv(pWin)->bounding_shape));
            for (i = 0; i < REGION_NUM_RECTS(xnestWindowPriv(pWin)->bounding_shape); i++) {
                rect[i].x = pBox[i].x1;
                rect[i].y = pBox[i].y1;
                rect[i].width = pBox[i].x2 - pBox[i].x1;
                rect[i].height = pBox[i].y2 - pBox[i].y1;
            }
            XCBShapeRectangles(xnestConnection,
                    XCBShapeSOUnion,
                    XCBShapeSKClip, /*HELP! What's the difference? Which do I use?*/
                    0, /*HELP! What's valid here, what does it do?*/
                    xnestWindow(pWin),
                    0, 0, /*x, y offsets*/
                    REGION_NUM_RECTS(xnestWindowPriv(pWin)->bounding_shape),
                    rect);
            XFree(rect);

        }
        else {
            REGION_EMPTY(pWin->drawable.pScreen, xnestWindowPriv(pWin)->bounding_shape);
            pmap.xid = 0;
            XCBShapeMask(xnestConnection,
                    XCBShapeSOSet,
                    XCBShapeSKClip, /*what's the difference?*/
                    xnestWindow(pWin),
                    0, 0,
                    pmap);
        }
    }

    if (!xnestRegionEqual(xnestWindowPriv(pWin)->clip_shape,
                wClipShape(pWin))) {

        if (wClipShape(pWin)) {
            REGION_COPY(pWin->drawable.pScreen, xnestWindowPriv(pWin)->clip_shape, wClipShape(pWin));

            rect = xalloc(sizeof(XCBRECTANGLE) * REGION_NUM_RECTS(xnestWindowPriv(pWin)->bounding_shape));
            pBox = REGION_RECTS(xnestWindowPriv(pWin)->clip_shape);
            for (i = 0; i < REGION_NUM_RECTS(xnestWindowPriv(pWin)->clip_shape); i++) {
                rect[i].x = pBox[i].x1;
                rect[i].y = pBox[i].y1;
                rect[i].width = pBox[i].x2 - pBox[i].x1;
                rect[i].height = pBox[i].y2 - pBox[i].y1;
            }
            XCBShapeRectangles(xnestConnection,
                    XCBShapeSOUnion,
                    XCBShapeSKClip, /*HELP! What's the difference? Which do I use?*/
                    0, /*HELP! What's valid here, what does it do?*/
                    xnestWindow(pWin),
                    0, 0, /*x, y offsets*/
                    REGION_NUM_RECTS(xnestWindowPriv(pWin)->bounding_shape),
                    rect);
                    XFree(rect);
        }
        else {
            REGION_EMPTY(pWin->drawable.pScreen, xnestWindowPriv(pWin)->clip_shape);
            pmap.xid = 0;
            XCBShapeMask(xnestConnection,
                    XCBShapeSOSet,
                    XCBShapeSKClip, /*HELP! What's the difference? Which do I use?*/
                    xnestWindow(pWin),
                    0, 0,
                    pmap);
        }
    }
}
#endif /* SHAPE */
