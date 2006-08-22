#ifdef HAVE_XNEST_CONFIG_H
#include <xs-config.h>
#endif
#include <X11/Xmd.h>
#include <X11/XCB/xcb.h>
#include <X11/XCB/xcb_aux.h>
#include <X11/XCB/xproto.h>
#include <X11/XCB/xcb_image.h>
#include "regionstr.h"
#include <X11/fonts/fontstruct.h>
#include "gcstruct.h"
#include "colormapst.h"
#include "scrnintstr.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "region.h"
#include "servermd.h"


#include "xs-globals.h"
#include "xs-pixmap.h"
#include "xs-window.h"
#include "xs-color.h"
#include "xs-gcops.h"
#include "xs-gc.h"

/*Forward decls*/
static WindowPtr xsTrackWindow(XCBWINDOW win, WindowPtr pParent);

/**
 * returns the WindowPtr of a window with a given XID on the backing server.
 * if the window is not tracked by Xnest, NULL is returned.
 **/
typedef struct {
    XCBWINDOW win;
    WindowPtr pWin;
} XsWindowMatch;

static int xsMatchFunc(WindowPtr pWin, XsWindowMatch *wm)
{
    if (wm->win.xid == XS_WINDOW_PRIV(pWin)->window.xid) {
        wm->pWin = pWin;
        return WT_STOPWALKING;
    }
    else
        return WT_WALKCHILDREN;
}

WindowPtr xsGetWindow(XCBWINDOW window)
{
    XsWindowMatch wm;
    int i;

    wm.pWin = NULL;
    wm.win = window;

    WalkTree(0, (int (*)(WindowPtr, pointer))xsMatchFunc, (pointer) &wm);
    return wm.pWin;
}


/**
 * Inserts a window into the window tree.
 * pParent must NOT be NULL, ie: this must NOT be called on the root window.
 **/
void xsInstallWindow(WindowPtr pWin, WindowPtr pParent)
{
    
}

/**
 * Initializes a window with valid values.
 * Assumes XS_WINDOW_PRIV(pWin)->window is valid.
 * Arguments:
 *      pWin: the window
 *      pParent: the parent. may be NULL for root window.
 *      x, y: the locations relative to the parent window.
 *      w, h: the width and height of the window.
 *      bw: the border width of the window.
 *
 * FIXME: This might break stuff when being called from xsCreateWindow. We'll see.
 **/
static void xsInitWindow(WindowPtr pWin, WindowPtr pParent, int x, int y, int w, int h, int bw)
{
    XCBWINDOW win;
    int parent_x, parent_y;

    win = XS_WINDOW_PRIV(pWin)->window;
    if (pParent) {
        parent_x = pParent->drawable.x;
        parent_y = pParent->drawable.y;
    } else {
        parent_x = 0;
        parent_y = 0;
    }
    /* copy parent's drawable. If there's no parent, we're initting the
     * root window, which gets it's drawable initialized by the DIX */
    if (pParent)
        pWin->drawable = pParent->drawable;
    /*init drawable. Drawable's coordinates are in world coordinates.*/
    pWin->drawable.x = x + parent_x + wBorderWidth(pWin);
    pWin->drawable.y = y + parent_y + wBorderWidth(pWin);
    pWin->drawable.width = w;
    pWin->drawable.height = h;

    /*init origin. pWin->origin is relative to parent.*/
    pWin->origin.x = x;
    pWin->origin.y = y;

    pWin->prevSib = NULL;
    pWin->nextSib = NULL;
    pWin->firstChild = NULL;
    pWin->lastChild = NULL;

    pWin->valdata = (ValidatePtr)NULL;
    pWin->optional = (WindowOptPtr)NULL;
    pWin->cursorIsNone = TRUE;

    pWin->backingStore = NotUseful;
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
    pWin->borderWidth = bw;
    pWin->drawable.x = pParent->drawable.x + x + bw;
    pWin->drawable.y = pParent->drawable.y + y + bw;   
    pWin->drawable.type = DRAWABLE_WINDOW;
    pWin->drawable.width = w;
    pWin->drawable.height = h;
    pWin->backingStore = NotUseful;
    pWin->backStorage = NULL;
    pWin->backgroundState = 0;

    XS_WINDOW_PRIV(pWin)->window = win;
    XS_WINDOW_PRIV(pWin)->owned = XS_OWNED;

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
    REGION_NULL(pScreen, &pWin->borderSize);
    REGION_NULL(pScreen, &pWin->clipList);
    REGION_NULL(pScreen, &pWin->borderClip);

    /*??? What exactly do these do that setting pWin->drawable.{x,y} etc don't
    SetWinSize (pWin);
    SetBorderSize (pWin);
    */
}

/**
 * Tracks all children of a given WindowPtr. the WindowPtr is _NOT_ tracked.
 * the backing server *must* be grabbed when calling this function, since this
 * function doesn't do the server grab on it's own
 **/
static void xsTrackChildren(WindowPtr pParent, CARD32 ev_mask)
{
    XCBWINDOW             win;
    WindowPtr             pWin;
    XCBQueryTreeCookie    qcook;
    XCBQueryTreeRep      *qrep;
    XCBGetGeometryCookie  gcook;
    XCBGetGeometryRep    *grep;
    XCBWINDOW            *child;
    int                   i;

    win = XS_WINDOW_PRIV(pParent)->window;
    qcook = XCBQueryTree(xsConnection, win);
    qrep = XCBQueryTreeReply(xsConnection, qcook, NULL);
    child = XCBQueryTreeChildren(qrep);
    for (i=0; i < qrep->children_len; i++) {
        pWin = xsGetWindow(child[i]);
        if (!pWin){
            gcook = XCBGetGeometry(xsConnection, (XCBDRAWABLE)child[i]);
            grep = XCBGetGeometryReply(xsConnection, gcook, NULL);
            pWin = AllocateWindow(pParent->drawable.pScreen);
            XS_WINDOW_PRIV(pWin)->window = child[i];
            xsInitWindow(pWin, pParent, grep->x, grep->y, grep->width, grep->height, grep->border_width);
            XCBChangeWindowAttributes(xsConnection, child[i], XCBCWEventMask, &ev_mask);
        } else {
            xsRemoveWindow(pWin);
        }
        xsInstallWindow(pParent, pWin);
    }
}

/**
 * Allocates a new WindowPtr, and tracks it, inserting it into the
 * window tree. Assumes that pParent is the parent of the window.
 **/
static WindowPtr xsTrackWindow(XCBWINDOW win, WindowPtr pParent)
{
    WindowPtr pWin;
    CARD32 ev_mask;
    XCBGetGeometryCookie  gcook;
    XCBGetGeometryRep    *grep;

    pWin = AllocateWindow(pParent->drawable.pScreen);
    gcook = XCBGetGeometry(xsConnection, (XCBDRAWABLE)win);
    grep = XCBGetGeometryReply(xsConnection, gcook, NULL);

    /*initialize the window*/
    xsInitWindow(pWin, pParent, 
                 grep->x, grep->y,
                 grep->width, grep->height,
                 grep->border_width);

    /*set the event mask*/
    ev_mask = XCBEventMaskSubstructureNotify|XCBEventMaskStructureNotify;
    XCBChangeWindowAttributes(xsConnection, win, XCBCWEventMask, &ev_mask);

    /*make sure we've got all the children of the window*/
    XCBGrabServer(xsConnection);
    xsTrackChildren(pWin, ev_mask);
    XCBUngrabServer(xsConnection);
    return pWin;
}

/**
 * Implements the CreateWindow handler for X clients.
 * Not used by the event handling code, since xsCreateWindow
 * assumes the WindowPtr gets handled in CreateWindow, in the DIX.
 **/
Bool xsCreateWindow(WindowPtr pWin)
{
    CARD32      mask;
    CARD32      ev_mask;
    XCBSCREEN  *screen;
    XCBVISUALID vid;
    XCBParamsCW params;

    /* Inits too much for CreateWindow calls, but.. well.. otherwise we'd
     * duplicate code. */
    screen =  XCBSetupRootsIter (XCBGetSetup (xsConnection)).data;
    /**
     * We need to special-case creating the root window, since
     * it's representation on the backing server has already been
     * done for us.
     **/
    if (XS_IS_ROOT(pWin)) {
        XS_WINDOW_PRIV(pWin)->window = screen->root;

#if 0
        /*FIXME! do we need to do this?
        /*initialize the root window*/
        xsInitWindow(pWin, NULL,  /*root has no parent*/
                     0, 0,        /*origin at 0, 0*/
                     screen->width_in_pixels, /*same size as screen*/
                     screen->height_in_pixels, 
                     0); /*no border*/
#endif

        /*we want to listen to both motion and creation events on the root*/
        mask = XCBEventMaskSubstructureNotify | XCBEventMaskPointerMotion;
        XCBChangeWindowAttributes(xsConnection, screen->root, XCBCWEventMask, &ev_mask);

        /*track the children of the root window*/
        XCBGrabServer(xsConnection);
            xsTrackChildren(pWin, ev_mask);
        XCBUngrabServer(xsConnection);
        return TRUE;
    }

    if (pWin->drawable.class == XCBWindowClassInputOnly) {
        0L;
        vid.id = XCBCopyFromParent;
    } else {
        if (pWin->optional && pWin->optional->visual != wVisual(pWin->parent)) {
            ErrorF("Need to get visuals");
            exit(1);
        } else {
             vid.id = XCBCopyFromParent;
        }
    }
    /*we want all the important events on the window*/
    params.event_mask = ((1<<25)-1) &
                        ~(XCBEventMaskSubstructureRedirect |
                          XCBEventMaskPointerMotionHint    |
                          XCBEventMaskResizeRedirect);

    /*If we're not creating the root window, continue as normal*/
    XS_WINDOW_PRIV(pWin)->window = XCBWINDOWNew(xsConnection);
    XCBAuxCreateWindow(xsConnection,
                       pWin->drawable.depth, 
                       XS_WINDOW_PRIV(pWin)->window,
                       XS_WINDOW_PRIV(pWin->parent)->window,
                       pWin->origin.x - wBorderWidth(pWin),
                       pWin->origin.y - wBorderWidth(pWin),
                       pWin->drawable.width,
                       pWin->drawable.height,
                       pWin->borderWidth,
                       pWin->drawable.class,
                       vid,
                       mask,
                       &params);
}

/**
 * Destroys a window on the backing server.
 * Does nothing if the window is unowned by Xscreen.
 *    FIXME: not sure if it's possible for this function
 *           to be called by a window not owned by Xscreen
 **/
Bool xsDestroyWindow(WindowPtr pWin)
{
    /* I don't think we want to be destroying unowned windows.
     * Might be wrong about this though.*/
    if (XS_WINDOW_PRIV(pWin)->owned != XS_OWNED)
        return FALSE;
    XCBDestroyWindow(xsConnection, XS_WINDOW_PRIV(pWin)->window);
    XS_WINDOW_PRIV(pWin)->window = (XCBWINDOW){0};
    return TRUE;
}


/**
 * Positions a window at the specified (x,y) coordinates.
 **/
Bool xsPositionWindow(WindowPtr pWin, int x, int y)
{
    CARD32 list[2];

    list[0] = x;
    list[1] = y;
    XCBConfigureWindow(xsConnection,
                       XS_WINDOW_PRIV(pWin)->window,
                       XCBConfigWindowX | XCBConfigWindowY,
                       list);
    return TRUE;
}

/**
 * Changes window attributes
 **/

Bool xsChangeWindowAttributes(WindowPtr pWin, unsigned long mask)
{
    XCBParamsCW param;
    PixmapPtr pPixmap;

    if (mask & XCBCWBackPixmap) {
        switch (pWin->backgroundState) {
            case XCBBackPixmapNone:
                param.back_pixmap = 0;
                break;

            case XCBBackPixmapParentRelative:
                param.back_pixmap = XCBBackPixmapParentRelative;
                break;

            /*X server internal things.*/
            case BackgroundPixmap:
                pPixmap = pWin->background.pixmap;
                param.back_pixmap = XS_PIXMAP_PRIV(pPixmap)->pixmap.xid;
                break;

            case BackgroundPixel:
                mask &= ~XCBCWBackPixmap;
                break;
        }
    }

    if (mask & XCBCWBackPixel) {
        if (pWin->backgroundState == BackgroundPixel)
            param.back_pixel = pWin->background.pixel;
        else
            mask &= ~CWBackPixel;
    }
    
    if (mask & XCBCWBorderPixmap) {
        if (pWin->borderIsPixel)
            mask &= ~CWBorderPixmap;
        else
            pPixmap = pWin->border.pixmap;
            param.border_pixmap = XS_PIXMAP_PRIV(pPixmap)->pixmap.xid;
    }
    
    if (mask & XCBCWBorderPixel) {
        if (pWin->borderIsPixel)
            param.border_pixel = pWin->border.pixel;
        else
            mask &= ~XCBCWBorderPixel;
    }

    if (mask & XCBCWBitGravity) 
        param.bit_gravity = pWin->bitGravity;

    if (mask & XCBCWWinGravity) /* dix does this for us */
        mask &= ~CWWinGravity;

    if (mask & XCBCWBackingStore) /* this is really not useful */
        mask &= ~CWBackingStore;

    if (mask & XCBCWBackingPlanes) /* this is really not useful */
        mask &= ~CWBackingPlanes;

    if (mask & XCBCWBackingPixel) /* this is really not useful */ 
        mask &= ~CWBackingPixel;

    if (mask & XCBCWOverrideRedirect)
        param.override_redirect = pWin->overrideRedirect;

    if (mask & XCBCWSaveUnder) /* this is really not useful */
        mask &= ~CWSaveUnder;

    if (mask & XCBCWEventMask) /* events are handled elsewhere */
        mask &= ~CWEventMask;

    if (mask & XCBCWDontPropagate) /* events are handled elsewhere */
        mask &= ~CWDontPropagate; 
    
    if (mask & XCBCWColormap) {
        ColormapPtr pCmap;
        pCmap = LookupIDByType(wColormap(pWin), RT_COLORMAP);
        param.colormap = XS_CMAP_PRIV(pCmap)->colormap.xid;
        xsSetInstalledColormapWindows(pWin->drawable.pScreen);
    }
    if (mask & XCBCWCursor) /* this is handeled in cursor code */
        mask &= ~XCBCWCursor;

    if (mask)
        XCBAuxChangeWindowAttributes(xsConnection, XS_WINDOW_PRIV(pWin)->window, mask, &param);
}

/**
 * Configures window.
 * Assumes that the client won't do something completely stupid (namely
 * configuring a window with a zero mask) and that the cost of actually
 * doing a configure for extra values is unneeded.
 **/
void xsConfigureWindow(WindowPtr pWin, CARD32 mask)
{
    WindowPtr                pSib;
    CARD32                   vmask;
    XCBParamsConfigureWindow values;

    /* We fill the entire structure here, and let the mask weed out the
     * extra data */
    /* window size/position */
    values.x = pWin->origin.x;
    values.y = pWin->origin.y;
    values.width = pWin->drawable.width;
    values.height = pWin->drawable.height;
    values.border_width = wBorder(pWin);

    XCBAuxConfigureWindow(xsConnection, XS_WINDOW_PRIV(pWin)->window, mask, &values);

    if (mask & XCBConfigWindowStackMode) {
        /*get top sibling*/
        for (pSib = pWin; pSib->prevSib != NULL; pSib = pSib->prevSib);

        vmask = XCBConfigWindowStackMode;
        values.stack_mode = XCBStackModeAbove;
        XCBAuxConfigureWindow(xsConnection, XS_WINDOW_PRIV(pSib)->window, vmask, &values); 

        /* the rest of siblings */
        for (pSib = pSib->nextSib; pSib != NullWindow; pSib = pSib->nextSib) {
            vmask = XCBConfigWindowSibling | XCBConfigWindowStackMode;
            values.sibling = XS_WINDOW_PRIV(pSib)->window.xid;
            values.stack_mode = Below;
            XCBAuxConfigureWindow(xsConnection, XS_WINDOW_PRIV(pSib)->window, vmask, &values);
        }
    }
}


/**
 * Realizes a window on the screen
 **/
Bool xsRealizeWindow(WindowPtr pWin)
{
    if (XS_IS_ROOT(pWin))
        return TRUE;
    xsConfigureWindow(pWin, XCBConfigWindowStackMode);
    XCBMapWindow(xsConnection, XS_WINDOW_PRIV(pWin)->window);
    return TRUE;    
}

/**
 * Unrealizes a window
 **/
Bool xsUnrealizeWindow(WindowPtr pWin)
{
    XCBUnmapWindow(xsConnection, XS_WINDOW_PRIV(pWin)->window);
    return TRUE;
}

void xsPaintWindowBackground(WindowPtr pWin, RegionPtr pRegion, int what)
{
    int i;
    BoxPtr pBox;

    pBox = REGION_RECTS(pRegion);
    for (i = 0; i < REGION_NUM_RECTS(pRegion); i++)
        XCBClearArea(xsConnection,
                     FALSE,
                     XS_WINDOW_PRIV(pWin)->window,
                     pBox[i].x1 - pWin->drawable.x,
                     pBox[i].y1 - pWin->drawable.y,
                     pBox[i].x2 - pBox[i].x1, 
                     pBox[i].y2 - pBox[i].y1);
}

Bool xsPaintWindowBorder(WindowPtr pWin, RegionPtr pRegion, int what)
{
    /* I think this should be a no-op? */
}

void xsCopyWindow(WindowPtr pWin, xPoint oldOrigin, RegionPtr oldRegion)
{
    /* another no-op */
}


void xsClipNotify(WindowPtr pWin, int dx, int dy)
{
    xsConfigureWindow(pWin, XCBConfigWindowStackMode);
}
