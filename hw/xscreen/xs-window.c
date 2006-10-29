#ifdef HAVE_XS_CONFIG_H
#include <xs-config.h>
#endif
#include <X11/Xmd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>
#include <xcb/xcb_image.h>
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
static WindowPtr xsTrackWindow(xcb_window_t win, WindowPtr pParent);

/**
 * returns the WindowPtr of a window with a given XID on the backing server.
 * if the window is not tracked by Xnest, NULL is returned.
 **/
typedef struct {
    xcb_window_t win;
    WindowPtr pWin;
} XsWindowMatch;

static int xsMatchFunc(WindowPtr pWin, XsWindowMatch *wm)
{
    if (wm->win == XS_WINDOW_PRIV(pWin)->window) {
        wm->pWin = pWin;
        return WT_STOPWALKING;
    }
    else
        return WT_WALKCHILDREN;
}

static WindowPtr xsGetWindow(xcb_window_t window)
{
    XsWindowMatch wm;

    wm.pWin = NULL;
    wm.win = window;

    WalkTree(0, (int (*)(WindowPtr, pointer))xsMatchFunc, (pointer) &wm);
    return wm.pWin;
}

/**
 * Removes a window from the window tree, rearranging the siblings.
 **/
static void xsRemoveWindow(WindowPtr pWin)
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
/**
 * Inserts a window into the window tree.
 * pParent must NOT be NULL, ie: this must NOT be called on the root window.
 **/
static void xsInsertWindow(WindowPtr pWin, WindowPtr pParent)
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
    xcb_window_t win;
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
static void xsTrackChildren(WindowPtr pParent, uint32_t ev_mask)
{
    xcb_window_t             win;
    WindowPtr             pWin;
    xcb_query_tree_cookie_t    qcook;
    xcb_query_tree_reply_t      *qrep;
    xcb_get_geometry_cookie_t  gcook;
    xcb_get_geometry_reply_t    *grep;
    xcb_window_t            *child;
    int                   i;

    win = XS_WINDOW_PRIV(pParent)->window;
    qcook = xcb_query_tree(xsConnection, win);
    qrep = xcb_query_tree_reply(xsConnection, qcook, NULL);
    child = xcb_query_tree_children(qrep);
    for (i=0; i < qrep->children_len; i++) {
        pWin = xsGetWindow(child[i]);
        if (!pWin){
            gcook = xcb_get_geometry(xsConnection, (xcb_drawable_t)child[i]);
            grep = xcb_get_geometry_reply(xsConnection, gcook, NULL);
            pWin = AllocateWindow(pParent->drawable.pScreen);
            XS_WINDOW_PRIV(pWin)->window = child[i];
            xsInitWindow(pWin, pParent, grep->x, grep->y, grep->width, grep->height, grep->border_width);
            xcb_change_window_attributes(xsConnection, child[i], XCB_CW_EVENT_MASK, &ev_mask);
        } else {
            xsRemoveWindow(pWin);
        }
        xsInsertWindow(pParent, pWin);
    }
}

/**
 * Allocates a new WindowPtr, and tracks it, inserting it into the
 * window tree. Assumes that pParent is the parent of the window.
 **/
static WindowPtr xsTrackWindow(xcb_window_t win, WindowPtr pParent)
{
    WindowPtr pWin;
    uint32_t ev_mask;
    xcb_get_geometry_cookie_t  gcook;
    xcb_get_geometry_reply_t    *grep;

    pWin = AllocateWindow(pParent->drawable.pScreen);
    gcook = xcb_get_geometry(xsConnection, (xcb_drawable_t)win);
    grep = xcb_get_geometry_reply(xsConnection, gcook, NULL);

    /*initialize the window*/
    xsInitWindow(pWin, pParent, 
                 grep->x, grep->y,
                 grep->width, grep->height,
                 grep->border_width);

    /*set the event mask*/
    ev_mask = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY|XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    xcb_change_window_attributes(xsConnection, win, XCB_CW_EVENT_MASK, &ev_mask);

    /*make sure we've got all the children of the window*/
    xcb_grab_server(xsConnection);
    xsTrackChildren(pWin, ev_mask);
    xcb_ungrab_server(xsConnection);
    return pWin;
}

/**
 * Implements the CreateWindow handler for X clients.
 * Not used by the event handling code, since xsCreateWindow
 * assumes the WindowPtr gets handled in CreateWindow, in the DIX.
 **/
Bool xsCreateWindow(WindowPtr pWin)
{
    uint32_t      mask;
    uint32_t      ev_mask;
    xcb_screen_t  *screen;
    xcb_visualid_t vid;
    xcb_params_cw_t params;

    /* Inits too much for CreateWindow calls, but.. well.. otherwise we'd
     * duplicate code. */
    screen =  xcb_setup_roots_iterator (xcb_get_setup (xsConnection)).data;
    /**
     * We need to special-case creating the root window, since
     * it's representation on the backing server has already been
     * done for us.
     **/
    if (XS_IS_ROOT(pWin)) {
        XS_WINDOW_PRIV(pWin)->window = screen->root;

#if 0
        /*FIXME! do we need to do this?*/
        /*initialize the root window*/
        xsInitWindow(pWin, NULL,  /*root has no parent*/
                     0, 0,        /*origin at 0, 0*/
                     screen->width_in_pixels, /*same size as screen*/
                     screen->height_in_pixels, 
                     0); /*no border*/
#endif

        /*we want to listen to both motion and creation events on the root*/
        mask = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_POINTER_MOTION;
        xcb_change_window_attributes(xsConnection, screen->root, XCB_CW_EVENT_MASK, &ev_mask);

        /*track the children of the root window*/
        xcb_grab_server(xsConnection);
            xsTrackChildren(pWin, ev_mask);
        xcb_ungrab_server(xsConnection);
        return TRUE;
    }

    if (pWin->drawable.class == XCB_WINDOW_CLASS_INPUT_ONLY) {
        vid = XCB_COPY_FROM_PARENT;
    } else {
        if (pWin->optional && pWin->optional->visual != wVisual(pWin->parent)) {
            ErrorF("Need to get visuals");
            exit(1);
        } else {
             vid = XCB_COPY_FROM_PARENT;
        }
    }
    /*we want all the important events on the window*/
    params.event_mask = ((1<<25)-1) &
                        ~(XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                          XCB_EVENT_MASK_POINTER_MOTION_HINT    |
                          XCB_EVENT_MASK_RESIZE_REDIRECT);

    /*If we're not creating the root window, continue as normal*/
    XS_WINDOW_PRIV(pWin)->window = xcb_generate_id(xsConnection);
    xcb_aux_create_window(xsConnection,
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
    return TRUE;
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
    xcb_destroy_window(xsConnection, XS_WINDOW_PRIV(pWin)->window);
    XS_WINDOW_PRIV(pWin)->window = (xcb_window_t){0};
    return TRUE;
}


/**
 * Positions a window at the specified (x,y) coordinates.
 **/
Bool xsPositionWindow(WindowPtr pWin, int x, int y)
{
    uint32_t list[2];

    list[0] = x;
    list[1] = y;
    xcb_configure_window(xsConnection,
                       XS_WINDOW_PRIV(pWin)->window,
                       XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                       list);
    return TRUE;
}

/**
 * Changes window attributes
 **/

Bool xsChangeWindowAttributes(WindowPtr pWin, unsigned long mask)
{
    xcb_params_cw_t param;
    PixmapPtr pPixmap;

    if (mask & XCB_CW_BACK_PIXMAP) {
        switch (pWin->backgroundState) {
            case XCB_BACK_PIXMAP_NONE:
                param.back_pixmap = 0;
                break;

            case XCB_BACK_PIXMAP_PARENT_RELATIVE:
                param.back_pixmap = XCB_BACK_PIXMAP_PARENT_RELATIVE;
                break;

            /*X server internal things.*/
            case BackgroundPixmap:
                pPixmap = pWin->background.pixmap;
                param.back_pixmap = XS_PIXMAP_PRIV(pPixmap)->pixmap;
                break;

            case BackgroundPixel:
                mask &= ~XCB_CW_BACK_PIXMAP;
                break;
        }
    }

    if (mask & XCB_CW_BACK_PIXEL) {
        if (pWin->backgroundState == BackgroundPixel)
            param.back_pixel = pWin->background.pixel;
        else
            mask &= ~CWBackPixel;
    }
    
    if (mask & XCB_CW_BORDER_PIXMAP) {
        if (pWin->borderIsPixel)
            mask &= ~CWBorderPixmap;
        else
            pPixmap = pWin->border.pixmap;
            param.border_pixmap = XS_PIXMAP_PRIV(pPixmap)->pixmap;
    }
    
    if (mask & XCB_CW_BORDER_PIXEL) {
        if (pWin->borderIsPixel)
            param.border_pixel = pWin->border.pixel;
        else
            mask &= ~XCB_CW_BORDER_PIXEL;
    }

    if (mask & XCB_CW_BIT_GRAVITY) 
        param.bit_gravity = pWin->bitGravity;

    if (mask & XCB_CW_WIN_GRAVITY) /* dix does this for us */
        mask &= ~CWWinGravity;

    if (mask & XCB_CW_BACKING_STORE) /* this is really not useful */
        mask &= ~CWBackingStore;

    if (mask & XCB_CW_BACKING_PLANES) /* this is really not useful */
        mask &= ~CWBackingPlanes;

    if (mask & XCB_CW_BACKING_PIXEL) /* this is really not useful */ 
        mask &= ~CWBackingPixel;

    if (mask & XCB_CW_OVERRIDE_REDIRECT)
        param.override_redirect = pWin->overrideRedirect;

    if (mask & XCB_CW_SAVE_UNDER) /* this is really not useful */
        mask &= ~CWSaveUnder;

    if (mask & XCB_CW_EVENT_MASK) /* events are handled elsewhere */
        mask &= ~CWEventMask;

    if (mask & XCB_CW_DONT_PROPAGATE) /* events are handled elsewhere */
        mask &= ~CWDontPropagate; 
    
    if (mask & XCB_CW_COLORMAP) {
        ColormapPtr pCmap;
        pCmap = LookupIDByType(wColormap(pWin), RT_COLORMAP);
        param.colormap = XS_CMAP_PRIV(pCmap)->colormap;
        xsSetInstalledColormapWindows(pWin->drawable.pScreen);
    }
    if (mask & XCB_CW_CURSOR) /* this is handeled in cursor code */
        mask &= ~XCB_CW_CURSOR;

    if (mask)
        xcb_aux_change_window_attributes(xsConnection, XS_WINDOW_PRIV(pWin)->window, mask, &param);
}

/**
 * Configures window.
 * Assumes that the client won't do something completely stupid (namely
 * configuring a window with a zero mask) and that the cost of actually
 * doing a configure for extra values is unneeded.
 **/
void xsConfigureWindow(WindowPtr pWin, uint32_t mask)
{
    WindowPtr                pSib;
    uint32_t                   vmask;
    xcb_params_configure_window_t values;

    /* We fill the entire structure here, and let the mask weed out the
     * extra data */
    /* window size/position */
    values.x = pWin->origin.x;
    values.y = pWin->origin.y;
    values.width = pWin->drawable.width;
    values.height = pWin->drawable.height;
    values.border_width = wBorderWidth(pWin);

    xcb_aux_configure_window(xsConnection, XS_WINDOW_PRIV(pWin)->window, mask, &values);

    if (mask & XCB_CONFIG_WINDOW_STACK_MODE) {
        /*get top sibling*/
        for (pSib = pWin; pSib->prevSib != NULL; pSib = pSib->prevSib);

        vmask = XCB_CONFIG_WINDOW_STACK_MODE;
        values.stack_mode = XCB_STACK_MODE_ABOVE;
        xcb_aux_configure_window(xsConnection, XS_WINDOW_PRIV(pSib)->window, vmask, &values); 

        /* the rest of siblings */
        for (pSib = pSib->nextSib; pSib != NullWindow; pSib = pSib->nextSib) {
            vmask = XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE;
            values.sibling = XS_WINDOW_PRIV(pSib)->window;
            values.stack_mode = Below;
            xcb_aux_configure_window(xsConnection, XS_WINDOW_PRIV(pSib)->window, vmask, &values);
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
    xsConfigureWindow(pWin, XCB_CONFIG_WINDOW_STACK_MODE);
    xcb_map_window(xsConnection, XS_WINDOW_PRIV(pWin)->window);
    return TRUE;    
}

/**
 * Unrealizes a window
 **/
Bool xsUnrealizeWindow(WindowPtr pWin)
{
    xcb_unmap_window(xsConnection, XS_WINDOW_PRIV(pWin)->window);
    return TRUE;
}

void xsPaintWindowBackground(WindowPtr pWin, RegionPtr pRegion, int what)
{
    int i;
    BoxPtr pBox;

    pBox = REGION_RECTS(pRegion);
    for (i = 0; i < REGION_NUM_RECTS(pRegion); i++)
        xcb_clear_area(xsConnection,
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
    xsConfigureWindow(pWin, XCB_CONFIG_WINDOW_STACK_MODE);
}
