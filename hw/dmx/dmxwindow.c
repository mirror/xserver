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
 *
 */

/** \file
 * This file provides support for window-related functions. */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmx.h"
#include "dmxsync.h"
#include "dmxwindow.h"
#include "dmxpixmap.h"
#include "dmxcmap.h"
#include "dmxvisual.h"
#include "dmxinput.h"
#include "dmxextension.h"
#include "dmxscrinit.h"
#include "dmxcursor.h"
#include "dmxfont.h"
#include "dmxatom.h"
#include "dmxprop.h"
#include "dmxselection.h"
#include "dmxdnd.h"
#ifdef RENDER
#include "dmxpict.h"
#endif

#include "windowstr.h"
#include "propertyst.h"
#include "dixfont.h"

#ifdef PANORAMIX
#include "panoramiX.h"
#include "panoramiXsrv.h"
#endif

static void dmxDoRestackWindow(WindowPtr pWindow);
static void dmxDoChangeWindowAttributes(WindowPtr pWindow,
					unsigned long *mask,
					XSetWindowAttributes *attribs);

static void dmxDoSetShape(WindowPtr pWindow);

static void
dmxDoRedirectWindow(WindowPtr pWindow);

static void
dmxDoUpdateWindowPixmap(WindowPtr pWindow);

/** Initialize the private area for the window functions. */
Bool dmxInitWindow(ScreenPtr pScreen)
{
    if (!dixRequestPrivate(dmxWinPrivateKey, sizeof(dmxWinPrivRec)))
	return FALSE;

    return TRUE;
}


Window dmxCreateRootWindow(WindowPtr pWindow)
{
    ScreenPtr             pScreen   = pWindow->drawable.pScreen;
    DMXScreenInfo        *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr         pWinPriv  = DMX_GET_WINDOW_PRIV(pWindow);
    Window                parent;
    Visual               *visual;
    unsigned long         mask = 0;
    XSetWindowAttributes  attribs;
    ColormapPtr           pCmap;
    dmxColormapPrivPtr    pCmapPriv;
    Window                win = None;
    int                   w = pWindow->drawable.width;
    int                   h = pWindow->drawable.height;

    /* Avoid to create windows on back-end servers with virtual framebuffer */
    if (dmxScreen->virtualFb)
	return None;

    mask = CWEventMask;
    attribs.event_mask = dmxScreen->rootEventMask;

    /* Incorporate new attributes, if needed */
    if (pWinPriv->attribMask) {
	dmxDoChangeWindowAttributes(pWindow, &pWinPriv->attribMask, &attribs);
	mask |= pWinPriv->attribMask;
    }

    /* Create root window */
    parent = dmxScreen->scrnWin; /* This is our "Screen" window */
    visual = dmxScreen->beVisuals[dmxScreen->beDefVisualIndex].visual;

    pCmap = (ColormapPtr)LookupIDByType(wColormap(pWindow), RT_COLORMAP);
    pCmapPriv = DMX_GET_COLORMAP_PRIV(pCmap);

    mask |= CWBackingStore | CWColormap | CWBorderPixel | CWOverrideRedirect;
    attribs.backing_store     = NotUseful;
    attribs.colormap          = pCmapPriv->cmap;
    attribs.border_pixel      = 0;
    attribs.override_redirect = TRUE;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	w = PanoramiXPixWidth;
	h = PanoramiXPixHeight;
    }
#endif

    XLIB_PROLOGUE (dmxScreen);
    win = XCreateWindow(dmxScreen->beDisplay,
			parent,
			dmxScreen->rootX +
			pWindow->origin.x - wBorderWidth(pWindow),
			dmxScreen->rootY +
			pWindow->origin.y - wBorderWidth(pWindow),
			w,
			h,
			pWindow->borderWidth,
			pWindow->drawable.depth,
			pWindow->drawable.class,
			visual,
			mask,
			&attribs);
    XChangeProperty (dmxScreen->beDisplay, win,
		     XInternAtom (dmxScreen->beDisplay, "DMX_NAME", False),
		     XA_STRING, 8,
		     PropModeReplace,
		     (unsigned char *) dmxDigest,
		     strlen (dmxDigest));
    XLIB_EPILOGUE (dmxScreen);

    dmxBEDnDRootWindowUpdate (pWindow->drawable.pScreen, win);

    return win;
}

static void dmxSetIgnore (DMXScreenInfo *dmxScreen, unsigned int sequence)
{
    dmxAddSequence (&dmxScreen->ignore, sequence);
}

/** Change the location and size of the "root" window.  Called from
 *  #dmxReconfigureRootWindow(). */
void dmxResizeRootWindow(WindowPtr pRoot,
			 int x, int y, int w, int h)
{
    DMXScreenInfo  *dmxScreen = &dmxScreens[pRoot->drawable.pScreen->myNum];
    dmxWinPrivPtr   pWinPriv = DMX_GET_WINDOW_PRIV(pRoot);
    unsigned int    m;
    XWindowChanges  c;

    if (!pWinPriv->window)
	return;

    /* Handle resizing on back-end server */
    if (dmxScreen->beDisplay) {
	m = CWX | CWY | CWWidth | CWHeight;
	c.x = x;
	c.y = y;
	c.width = (w > 0) ? w : 1;
	c.height = (h > 0) ? h : 1;

	XLIB_PROLOGUE (dmxScreen);
	dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	XConfigureWindow(dmxScreen->beDisplay, pWinPriv->window, m, &c);
	XLIB_EPILOGUE (dmxScreen);
    }

    if (w == 0 || h == 0) {
	if (pWinPriv->mapped) {
	    if (dmxScreen->beDisplay)
	    {
		XLIB_PROLOGUE (dmxScreen);
		dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
		XUnmapWindow(dmxScreen->beDisplay, pWinPriv->window);
		XLIB_EPILOGUE (dmxScreen);
	    }
	    pWinPriv->mapped = FALSE;
	}
    } else if (!pWinPriv->mapped) {
	if (dmxScreen->beDisplay)
	{
	    XLIB_PROLOGUE (dmxScreen);
	    dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	    XMapWindow(dmxScreen->beDisplay, pWinPriv->window);
	    XLIB_EPILOGUE (dmxScreen);
	}
	pWinPriv->mapped = TRUE;
    }

    if (dmxScreen->beDisplay)
	dmxSync(dmxScreen, False);
}

void dmxGetDefaultWindowAttributes(WindowPtr pWindow,
				   Colormap *cmap,
				   Visual **visual)
{
    ScreenPtr  pScreen = pWindow->drawable.pScreen;

    if (pWindow->drawable.class != InputOnly &&
	pWindow->optional &&
	pWindow->optional->visual != wVisual(pWindow->parent)) {

	/* Find the matching visual */
	*visual = dmxLookupVisualFromID(pScreen, wVisual(pWindow));

	/* Handle optional colormaps */
	if (pWindow->optional->colormap) {
	    ColormapPtr         pCmap;
	    dmxColormapPrivPtr  pCmapPriv;

	    pCmap = (ColormapPtr)LookupIDByType(wColormap(pWindow),
						RT_COLORMAP);
	    pCmapPriv = DMX_GET_COLORMAP_PRIV(pCmap);
	    *cmap = pCmapPriv->cmap;
	} else {
	    *cmap = dmxColormapFromDefaultVisual(pScreen, *visual);
	}
    } else {
	*visual = CopyFromParent;
	*cmap = (Colormap)0;
    }
}

static Window dmxCreateNonRootWindow(WindowPtr pWindow)
{
    ScreenPtr             pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo        *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr         pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);
    Window                parent;
    unsigned long         mask = 0L;
    XSetWindowAttributes  attribs;
    dmxWinPrivPtr         pParentPriv = DMX_GET_WINDOW_PRIV(pWindow->parent);
    Window                win = None;
    int                   x, y, w, h, bw;

    /* Avoid to create windows on back-end servers with virtual framebuffer */
    if (dmxScreen->virtualFb)
	return None;

    /* Create window on back-end server */

    parent = pParentPriv->window;

    /* The parent won't exist if this call to CreateNonRootWindow came
       from ReparentWindow and the grandparent window has not yet been
       created */
    if (!parent) {
	dmxCreateAndRealizeWindow(pWindow->parent, FALSE);
	parent = pParentPriv->window;
    }

    mask |= CWEventMask;
    attribs.event_mask = ExposureMask | SubstructureRedirectMask;

    /* Incorporate new attributes, if needed */
    if (pWinPriv->attribMask) {
	dmxDoChangeWindowAttributes(pWindow, &pWinPriv->attribMask, &attribs);
	mask |= pWinPriv->attribMask;
    }

    /* Add in default attributes */
    if (pWindow->drawable.class != InputOnly) {
	mask |= CWBackingStore;
	attribs.backing_store = NotUseful;

	if (!(mask & CWColormap) && pWinPriv->cmap) {
	    mask |= CWColormap;
	    attribs.colormap = pWinPriv->cmap;
	    if (!(mask & CWBorderPixel)) {
		mask |= CWBorderPixel;
		attribs.border_pixel = 0;
	    }
	}
    }

    /* Handle case where subwindows are being mapped, but created out of
       order -- if current window has a previous sibling, then it cannot
       be created on top of the stack, so we must restack the windows */
    pWinPriv->restacked = (pWindow->prevSib != NullWindow);

    if (pWindow != dmxScreen->pInputOverlayWin)
    {
	x = pWindow->origin.x - wBorderWidth(pWindow);
	y = pWindow->origin.y - wBorderWidth(pWindow);
	w = pWindow->drawable.width;
	h = pWindow->drawable.height;
	bw = pWindow->borderWidth;
    }
    else
    {
	x = y = -1;
	w = h = 1;
	bw = 0;
    }

    XLIB_PROLOGUE (dmxScreen);
    win = XCreateWindow(dmxScreen->beDisplay,
			parent,
			x,
			y,
			w,
			h,
			bw,
			pWindow->drawable.depth,
			pWindow->drawable.class,
			pWinPriv->visual,
			mask,
			&attribs);
    XLIB_EPILOGUE (dmxScreen);

    return win;
}

/** This function handles lazy window creation and realization.  Window
 *  creation is handled by #dmxCreateNonRootWindow().  It also handles
 *  any stacking changes that have occured since the window was
 *  originally created by calling #dmxDoRestackWindow().  If the window
 *  is shaped, the shape is set on the back-end server by calling
 *  #dmxDoSetShape(), and if the window has pictures (from RENDER)
 *  associated with it, those pictures are created on the back-end
 *  server by calling #dmxCreatePictureList().  If \a doSync is TRUE,
 *  then #dmxSync() is called. */
void dmxCreateAndRealizeWindow(WindowPtr pWindow, Bool doSync)
{
    ScreenPtr      pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr  pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);

    if (!dmxScreen->beDisplay) return;

    if (!pWindow->parent)
	dmxScreen->rootWin = pWinPriv->window = dmxCreateRootWindow(pWindow);
    else
	pWinPriv->window = dmxCreateNonRootWindow(pWindow);
    if (pWinPriv->window)
    {
	if (pWinPriv->redirected) dmxDoRedirectWindow(pWindow);
	if (pWinPriv->restacked) dmxDoRestackWindow(pWindow);
	if (pWinPriv->isShaped) dmxDoSetShape(pWindow);
#ifdef RENDER
	if (pWinPriv->hasPict) dmxCreatePictureList(pWindow);
#endif
	if (pWinPriv->mapped)
	{
	    XLIB_PROLOGUE (dmxScreen);
	    dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	    XMapWindow(dmxScreen->beDisplay, pWinPriv->window);
	    XLIB_EPILOGUE (dmxScreen);

	    if (pWinPriv->beRedirected) dmxDoUpdateWindowPixmap (pWindow);
	}
	if (doSync) dmxSync(dmxScreen, False);
    }
}

/** Create \a pWindow on the back-end server.  If the lazy window
 *  creation optimization is enabled, then the actual creation and
 *  realization of the window is handled by
 *  #dmxCreateAndRealizeWindow(). */
Bool dmxCreateWindow(WindowPtr pWindow)
{
    ScreenPtr             pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo        *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr         pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);
    Bool                  ret = TRUE;

    DMX_UNWRAP(CreateWindow, dmxScreen, pScreen);

    if (pScreen->CreateWindow)
	ret = pScreen->CreateWindow(pWindow);

    /* Set up the defaults */
    pWinPriv->window     = (Window)0;
    pWinPriv->offscreen  = TRUE;
    pWinPriv->mapped     = FALSE;
    pWinPriv->restacked  = FALSE;
    pWinPriv->redirected = FALSE;
    pWinPriv->attribMask = 0;
    pWinPriv->isShaped   = FALSE;
#ifdef RENDER
    pWinPriv->hasPict    = FALSE;
#endif
#ifdef GLXEXT
    pWinPriv->swapGroup  = NULL;
    pWinPriv->barrier    = 0;
#endif

    pWinPriv->beRedirected = FALSE;

    if (dmxScreen->beDisplay) {
	/* Only create the root window at this stage -- non-root windows are
	   created when they are mapped and are on-screen */
	if (!pWindow->parent) {
	    dmxScreen->rootWin = pWinPriv->window =
		dmxCreateRootWindow(pWindow);
	} else {
	    dmxGetDefaultWindowAttributes(pWindow,
					  &pWinPriv->cmap,
					  &pWinPriv->visual);

	    if (dmxLazyWindowCreation) {
		/* Save parent's visual for use later */
		if (pWinPriv->visual == CopyFromParent)
		    pWinPriv->visual =
			dmxLookupVisualFromID(pScreen,
					      wVisual(pWindow->parent));
	    } else {
		pWinPriv->window = dmxCreateNonRootWindow(pWindow);
	    }
	}

	dmxSync(dmxScreen, False);
    }

    DMX_WRAP(CreateWindow, dmxCreateWindow, dmxScreen, pScreen);

    return ret;
}

/** Destroy \a pWindow on the back-end server. */
Bool dmxBEDestroyWindow(WindowPtr pWindow)
{
    ScreenPtr      pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr  pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);

    pWinPriv->beRedirected = FALSE;

    if (pWinPriv->window) {
	if (dmxScreen->scrnWin)
	{
	    XLIB_PROLOGUE (dmxScreen);
	    XDestroyWindow(dmxScreen->beDisplay, pWinPriv->window);
	    XLIB_EPILOGUE (dmxScreen);
	}
	pWinPriv->window = (Window)0;
	return TRUE;
    }

    return FALSE;
}

/** Destroy \a pWindow on the back-end server.  If any RENDER pictures
    were created, destroy them as well. */
Bool dmxDestroyWindow(WindowPtr pWindow)
{
    ScreenPtr      pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    Bool           ret = TRUE;
    Bool           needSync = FALSE;
#ifdef GLXEXT
    dmxWinPrivPtr  pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);
#endif

    DMX_UNWRAP(DestroyWindow, dmxScreen, pScreen);

#ifdef RENDER
    /* Destroy any picture list associated with this window */
    needSync |= dmxDestroyPictureList(pWindow);
#endif

    /* Destroy window on back-end server */
    needSync |= dmxBEDestroyWindow(pWindow);
    if (needSync) dmxSync(dmxScreen, FALSE);

#ifdef GLXEXT
    if (pWinPriv->swapGroup && pWinPriv->windowDestroyed)
	pWinPriv->windowDestroyed(pWindow);
#endif

    if (pScreen->DestroyWindow)
	ret = pScreen->DestroyWindow(pWindow);

    DMX_WRAP(DestroyWindow, dmxDestroyWindow, dmxScreen, pScreen);

    return ret;
}

/** Change the position of \a pWindow to be \a x, \a y. */
Bool dmxPositionWindow(WindowPtr pWindow, int x, int y)
{
    ScreenPtr       pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    Bool            ret = TRUE;
    dmxWinPrivPtr   pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);
    unsigned int    m;
    XWindowChanges  c;

    DMX_UNWRAP(PositionWindow, dmxScreen, pScreen);
    if (pScreen->PositionWindow)
	ret = pScreen->PositionWindow(pWindow, x, y);

    /* Determine if the window is completely off the visible portion of
       the screen */
    pWinPriv->offscreen = DMX_WINDOW_OFFSCREEN(pWindow);

    /* If the window is now on-screen and it is mapped and it has not
       been created yet, create it and map it */
    if (!pWinPriv->window && pWinPriv->mapped && !pWinPriv->offscreen) {
	dmxCreateAndRealizeWindow(pWindow, TRUE);
    } else if (pWinPriv->window && pWindow != dmxScreen->pInputOverlayWin) {
	/* Position window on back-end server */
	m = CWX | CWY | CWWidth | CWHeight;
	c.x = pWindow->origin.x - wBorderWidth(pWindow);
	c.y = pWindow->origin.y - wBorderWidth(pWindow);
	c.width = pWindow->drawable.width;
	c.height = pWindow->drawable.height;
	if (pWindow->drawable.class != InputOnly) {
	    m |= CWBorderWidth;
	    c.border_width = pWindow->borderWidth;
	}

	XLIB_PROLOGUE (dmxScreen);
	dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	XConfigureWindow(dmxScreen->beDisplay, pWinPriv->window, m, &c);
	XLIB_EPILOGUE (dmxScreen);
	dmxSync(dmxScreen, False);
    }

    DMX_WRAP(PositionWindow, dmxPositionWindow, dmxScreen, pScreen);

    return ret;
}

static void dmxDoChangeWindowAttributes(WindowPtr pWindow,
					unsigned long *mask,
					XSetWindowAttributes *attribs)
{
    dmxPixPrivPtr         pPixPriv;

    if (*mask & CWBackPixmap) {
	switch (pWindow->backgroundState) {
	case None:
	    attribs->background_pixmap = None;
	    break;

	case ParentRelative:
	    attribs->background_pixmap = ParentRelative;
	    break;

	case BackgroundPixmap:
	    pPixPriv = DMX_GET_PIXMAP_PRIV(pWindow->background.pixmap);
	    attribs->background_pixmap = pPixPriv->pixmap;
	    break;

	case BackgroundPixel:
	    *mask &= ~CWBackPixmap;
	    break;
	}
    }

    if (*mask & CWBackPixel) {
	if (pWindow->backgroundState == BackgroundPixel)
	    attribs->background_pixel = pWindow->background.pixel;
	else
	    *mask &= ~CWBackPixel;
    }

    if (*mask & CWBorderPixmap) {
	if (pWindow->borderIsPixel)
	    *mask &= ~CWBorderPixmap;
	else {
	    pPixPriv = DMX_GET_PIXMAP_PRIV(pWindow->border.pixmap);
	    attribs->border_pixmap = pPixPriv->pixmap;
	}
    }

    if (*mask & CWBorderPixel) {
	if (pWindow->borderIsPixel)
	    attribs->border_pixel = pWindow->border.pixel;
	else
	    *mask &= ~CWBorderPixel;
    }

    if (*mask & CWBitGravity)
	attribs->bit_gravity = pWindow->bitGravity;

    if (*mask & CWWinGravity)
	*mask &= ~CWWinGravity; /* Handled by dix */

    if (*mask & CWBackingStore)
	*mask &= ~CWBackingStore; /* Backing store not supported */

    if (*mask & CWBackingPlanes)
	*mask &= ~CWBackingPlanes; /* Backing store not supported */

    if (*mask & CWBackingPixel)
	*mask &= ~CWBackingPixel; /* Backing store not supported */

    if (*mask & CWOverrideRedirect)
    {
	if (pWindow->parent)
	    attribs->override_redirect = pWindow->overrideRedirect;
	else
	    *mask &= ~CWOverrideRedirect;
    }

    if (*mask & CWSaveUnder)
	*mask &= ~CWSaveUnder; /* Save unders not supported */

    if (*mask & CWEventMask)
	*mask &= ~CWEventMask; /* Events are handled by dix */

    if (*mask & CWDontPropagate)
	*mask &= ~CWDontPropagate; /* Events are handled by dix */

    if (*mask & CWColormap) {
	ColormapPtr         pCmap;
	dmxColormapPrivPtr  pCmapPriv;

	pCmap = (ColormapPtr)LookupIDByType(wColormap(pWindow), RT_COLORMAP);
	pCmapPriv = DMX_GET_COLORMAP_PRIV(pCmap);
	attribs->colormap = pCmapPriv->cmap;
    }

    if (*mask & CWCursor)
    {
	ScreenPtr pScreen = pWindow->drawable.pScreen;

	if (pWindow->cursorIsNone)
	{
	    attribs->cursor = None;
	}
	else
	{
	    dmxCursorPrivPtr pCursorPriv =
		DMX_GET_CURSOR_PRIV (wCursor (pWindow), pScreen);

	    attribs->cursor = pCursorPriv->cursor;
	}
    }
}

/** Change the window attributes of \a pWindow. */
Bool dmxChangeWindowAttributes(WindowPtr pWindow, unsigned long mask)
{
    ScreenPtr             pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo        *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr         pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);
    XSetWindowAttributes  attribs;

    /* Change window attribs on back-end server */
    dmxDoChangeWindowAttributes(pWindow, &mask, &attribs);

    /* Save mask for lazy window creation optimization */
    pWinPriv->attribMask |= mask;

    if (mask && pWinPriv->window) {
	XLIB_PROLOGUE (dmxScreen);
	XChangeWindowAttributes(dmxScreen->beDisplay, pWinPriv->window,
				mask, &attribs);
	XLIB_EPILOGUE (dmxScreen);
	dmxSync(dmxScreen, False);
    }

    return TRUE;
}

/** Realize \a pWindow on the back-end server.  If the lazy window
 *  creation optimization is enabled, the window is only realized when
 *  it at least partially overlaps the screen. */
Bool dmxRealizeWindow(WindowPtr pWindow)
{
    ScreenPtr      pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    Bool           ret = TRUE;
    dmxWinPrivPtr  pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);

    DMX_UNWRAP(RealizeWindow, dmxScreen, pScreen);
    if (pScreen->RealizeWindow)
	ret = pScreen->RealizeWindow(pWindow);

    /* Determine if the window is completely off the visible portion of
       the screen */
    pWinPriv->offscreen = DMX_WINDOW_OFFSCREEN(pWindow);

    /* If the window hasn't been created and it's not offscreen, then
       create it */
    if (!pWinPriv->window && !pWinPriv->offscreen) {
	dmxCreateAndRealizeWindow(pWindow, FALSE);
    }

    if (pWinPriv->window) {
	/* Realize window on back-end server */

	dmxDoRedirectWindow (pWindow);

	if (MapUnmapEventsEnabled (pWindow))
	{
	    XLIB_PROLOGUE (dmxScreen);
	    dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	    XMapWindow (dmxScreen->beDisplay, pWinPriv->window);
	    XLIB_EPILOGUE (dmxScreen);
	}

	dmxDoUpdateWindowPixmap (pWindow);
    }

    dmxSync(dmxScreen, False);

    /* Let the other functions know that the window is now mapped */
    pWinPriv->mapped = TRUE;

    DMX_WRAP(RealizeWindow, dmxRealizeWindow, dmxScreen, pScreen);

    return ret;
}

/** Unrealize \a pWindow on the back-end server. */
Bool dmxUnrealizeWindow(WindowPtr pWindow)
{
    ScreenPtr      pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    Bool           ret = TRUE;
    dmxWinPrivPtr  pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);

    DMX_UNWRAP(UnrealizeWindow, dmxScreen, pScreen);
    if (pScreen->UnrealizeWindow)
	ret = pScreen->UnrealizeWindow(pWindow);

    if (pWinPriv->window) {
	/* Unrealize window on back-end server */

	XLIB_PROLOGUE (dmxScreen);
	if (MapUnmapEventsEnabled (pWindow))
	{
	    dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	    XUnmapWindow(dmxScreen->beDisplay, pWinPriv->window);
	}

	if (pWinPriv->beRedirected)
	{
	    XCompositeUnredirectWindow (dmxScreen->beDisplay,
					pWinPriv->window,
					CompositeRedirectAutomatic);
	    pWinPriv->beRedirected = FALSE;
	}
	XLIB_EPILOGUE (dmxScreen);
	dmxSync(dmxScreen, False);
    }

    /* When unrealized (i.e., unmapped), the window is always considered
       off of the visible portion of the screen */
    pWinPriv->offscreen = TRUE;
    pWinPriv->mapped = FALSE;

#ifdef GLXEXT
    if (pWinPriv->swapGroup && pWinPriv->windowUnmapped)
	pWinPriv->windowUnmapped(pWindow);
#endif

    DMX_WRAP(UnrealizeWindow, dmxUnrealizeWindow, dmxScreen, pScreen);

    return ret;
}

static void dmxDoRestackWindow(WindowPtr pWindow)
{
    ScreenPtr       pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr   pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);
    WindowPtr       pNextSib = pWindow->nextSib;
    unsigned int    m;
    XWindowChanges  c;

    if (pNextSib == NullWindow) {
	/* Window is at the bottom of the stack */
	m = CWStackMode;
	c.sibling = (Window)0;
	c.stack_mode = Below;
	XLIB_PROLOGUE (dmxScreen);
	dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	XConfigureWindow(dmxScreen->beDisplay, pWinPriv->window, m, &c);
	XLIB_EPILOGUE (dmxScreen);
    } else {
	/* Window is not at the bottom of the stack */
	dmxWinPrivPtr  pNextSibPriv = DMX_GET_WINDOW_PRIV(pNextSib);

	/* Handle case where siblings have not yet been created due to
           lazy window creation optimization by first finding the next
           sibling in the sibling list that has been created (if any)
           and then putting the current window just above that sibling,
           and if no next siblings have been created yet, then put it at
           the bottom of the stack (since it might have a previous
           sibling that should be above it). */
	while (!pNextSibPriv->window) {
	    pNextSib = pNextSib->nextSib;
	    if (pNextSib == NullWindow) {
		/* Window is at the bottom of the stack */
		m = CWStackMode;
		c.sibling = (Window)0;
		c.stack_mode = Below;
		XLIB_PROLOGUE (dmxScreen);
		dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
		XConfigureWindow(dmxScreen->beDisplay, pWinPriv->window, m, &c);
		XLIB_EPILOGUE (dmxScreen);
		return;
	    }
	    pNextSibPriv = DMX_GET_WINDOW_PRIV(pNextSib);
	}

	m = CWStackMode | CWSibling;
	c.sibling = pNextSibPriv->window;
	c.stack_mode = Above;
	XLIB_PROLOGUE (dmxScreen);
	dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	XConfigureWindow(dmxScreen->beDisplay, pWinPriv->window, m, &c);
	XLIB_EPILOGUE (dmxScreen);
    }
}

/** Handle window restacking.  The actual restacking occurs in
 *  #dmxDoRestackWindow(). */
void dmxRestackWindow(WindowPtr pWindow, WindowPtr pOldNextSib)
{
    ScreenPtr       pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr   pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);

    DMX_UNWRAP(RestackWindow, dmxScreen, pScreen);
    if (pScreen->RestackWindow)
	pScreen->RestackWindow(pWindow, pOldNextSib);

    if (pOldNextSib != pWindow->nextSib) {
	/* Track restacking for lazy window creation optimization */
	pWinPriv->restacked = TRUE;

	/* Restack window on back-end server */
	if (pWinPriv->window) {
	    dmxDoRestackWindow(pWindow);
	    dmxSync(dmxScreen, False);
	}
    }

    DMX_WRAP(RestackWindow, dmxRestackWindow, dmxScreen, pScreen);
}

/** Move \a pWindow on the back-end server.  Determine whether or not it
 *  is on or offscreen, and realize it if it is newly on screen and the
 *  lazy window creation optimization is enabled. */
void dmxCopyWindow(WindowPtr pWindow, DDXPointRec ptOldOrg, RegionPtr prgnSrc)
{
    dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);

    /* Determine if the window is completely off the visible portion of
       the screen */
    pWinPriv->offscreen = DMX_WINDOW_OFFSCREEN(pWindow);

    /* If the window is now on-screen and it is mapped and it has not
       been created yet, create it and map it */
    if (!pWinPriv->window && pWinPriv->mapped && !pWinPriv->offscreen) {
	dmxCreateAndRealizeWindow(pWindow, TRUE);
    }
}

/** Resize \a pWindow on the back-end server.  Determine whether or not
 *  it is on or offscreen, and realize it if it is newly on screen and
 *  the lazy window creation optimization is enabled. */
void dmxResizeWindow(WindowPtr pWindow, int x, int y,
		     unsigned int w, unsigned int h, WindowPtr pSib)
{
    ScreenPtr       pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr   pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);
    dmxWinPrivPtr   pSibPriv;
    unsigned int    m;
    XWindowChanges  c;

    if (pSib)
	pSibPriv = DMX_GET_WINDOW_PRIV(pSib);

    DMX_UNWRAP(ResizeWindow, dmxScreen, pScreen);
    if (pScreen->ResizeWindow)
	pScreen->ResizeWindow(pWindow, x, y, w, h, pSib);

    /* Determine if the window is completely off the visible portion of
       the screen */
    pWinPriv->offscreen = DMX_WINDOW_OFFSCREEN(pWindow);

    /* If the window is now on-screen and it is mapped and it has not
       been created yet, create it and map it */
    if (!pWinPriv->window && pWinPriv->mapped && !pWinPriv->offscreen) {
	dmxCreateAndRealizeWindow(pWindow, TRUE);
    } else if (pWinPriv->window && pWindow != dmxScreen->pInputOverlayWin) {
	/* Handle resizing on back-end server */
	m = CWX | CWY | CWWidth | CWHeight;
	c.x = pWindow->origin.x - wBorderWidth(pWindow);
	c.y = pWindow->origin.y - wBorderWidth(pWindow);
	c.width = pWindow->drawable.width;
	c.height = pWindow->drawable.height;

	XLIB_PROLOGUE (dmxScreen);
	dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	XConfigureWindow(dmxScreen->beDisplay, pWinPriv->window, m, &c);
	XLIB_EPILOGUE (dmxScreen);
	dmxSync(dmxScreen, False);
    }

    DMX_WRAP(ResizeWindow, dmxResizeWindow, dmxScreen, pScreen);
}

/** Reparent \a pWindow on the back-end server. */
void dmxReparentWindow(WindowPtr pWindow, WindowPtr pPriorParent)
{
    ScreenPtr      pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr  pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);
    dmxWinPrivPtr  pParentPriv = DMX_GET_WINDOW_PRIV(pWindow->parent);

    DMX_UNWRAP(ReparentWindow, dmxScreen, pScreen);
    if (pScreen->ReparentWindow)
	pScreen->ReparentWindow(pWindow, pPriorParent);

    if (pWinPriv->window) {
	if (!pParentPriv->window) {
	    dmxCreateAndRealizeWindow(pWindow->parent, FALSE);
	}

	/* Handle reparenting on back-end server */
	XLIB_PROLOGUE (dmxScreen);
	XReparentWindow(dmxScreen->beDisplay, pWinPriv->window,
			pParentPriv->window,
			pWindow->origin.x - wBorderWidth(pWindow),
			pWindow->origin.x - wBorderWidth(pWindow));
	XLIB_EPILOGUE (dmxScreen);
	dmxSync(dmxScreen, False);
    }

    DMX_WRAP(ReparentWindow, dmxReparentWindow, dmxScreen, pScreen);
}

/** Change border width for \a pWindow to \a width pixels. */
void dmxChangeBorderWidth(WindowPtr pWindow, unsigned int width)
{
    ScreenPtr       pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr   pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);
    unsigned int    m;
    XWindowChanges  c;

    DMX_UNWRAP(ChangeBorderWidth, dmxScreen, pScreen);
    if (pScreen->ChangeBorderWidth)
	pScreen->ChangeBorderWidth(pWindow, width);

    /* NOTE: Do we need to check for on/off screen here? */

    if (pWinPriv->window && pWindow != dmxScreen->pInputOverlayWin) {
	/* Handle border width change on back-end server */
	m = CWBorderWidth;
	c.border_width = width;

	XLIB_PROLOGUE (dmxScreen);
	dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	XConfigureWindow(dmxScreen->beDisplay, pWinPriv->window, m, &c);
	XLIB_EPILOGUE (dmxScreen);
	dmxSync(dmxScreen, False);
    }

    DMX_WRAP(ChangeBorderWidth, dmxChangeBorderWidth, dmxScreen, pScreen);
}

static void dmxDoSetShape(WindowPtr pWindow)
{
    ScreenPtr       pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr   pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);
    int             nBox;
    BoxPtr          pBox;
    int             nRect;
    XRectangle     *pRect;
    XRectangle     *pRectFirst;

    /* First, set the bounding shape */
    if (wBoundingShape(pWindow)) {
	pBox = REGION_RECTS(wBoundingShape(pWindow));
	nRect = nBox = REGION_NUM_RECTS(wBoundingShape(pWindow));
	pRectFirst = pRect = xalloc(nRect * sizeof(*pRect));
	while (nBox--) {
	    pRect->x      = pBox->x1;
	    pRect->y      = pBox->y1;
	    pRect->width  = pBox->x2 - pBox->x1;
	    pRect->height = pBox->y2 - pBox->y1;
	    pBox++;
	    pRect++;
	}
	XLIB_PROLOGUE (dmxScreen);
	dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	XShapeCombineRectangles(dmxScreen->beDisplay, pWinPriv->window,
				ShapeBounding, 0, 0,
				pRectFirst, nRect,
				ShapeSet, YXBanded);
	XLIB_EPILOGUE (dmxScreen);
	xfree(pRectFirst);
    } else {
	XLIB_PROLOGUE (dmxScreen);
	dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	XShapeCombineMask(dmxScreen->beDisplay, pWinPriv->window,
			  ShapeBounding, 0, 0, None, ShapeSet);
	XLIB_EPILOGUE (dmxScreen);
    }

    /* Next, set the clip shape */
    if (wClipShape(pWindow)) {
	pBox = REGION_RECTS(wClipShape(pWindow));
	nRect = nBox = REGION_NUM_RECTS(wClipShape(pWindow));
	pRectFirst = pRect = xalloc(nRect * sizeof(*pRect));
	while (nBox--) {
	    pRect->x      = pBox->x1;
	    pRect->y      = pBox->y1;
	    pRect->width  = pBox->x2 - pBox->x1;
	    pRect->height = pBox->y2 - pBox->y1;
	    pBox++;
	    pRect++;
	}
	XLIB_PROLOGUE (dmxScreen);
	dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	XShapeCombineRectangles(dmxScreen->beDisplay, pWinPriv->window,
				ShapeClip, 0, 0,
				pRectFirst, nRect,
				ShapeSet, YXBanded);
	XLIB_EPILOGUE (dmxScreen);
	xfree(pRectFirst);
    } else {
	XLIB_PROLOGUE (dmxScreen);
	dmxSetIgnore (dmxScreen, NextRequest (dmxScreen->beDisplay));
	XShapeCombineMask(dmxScreen->beDisplay, pWinPriv->window,
			  ShapeClip, 0, 0, None, ShapeSet);
	XLIB_EPILOGUE (dmxScreen);
    }

    if (wInputShape (pWindow)) {
	pBox = REGION_RECTS(wInputShape(pWindow));
	nRect = nBox = REGION_NUM_RECTS(wInputShape(pWindow));
	pRectFirst = pRect = xalloc(nRect * sizeof(*pRect));
	while (nBox--) {
	    pRect->x      = pBox->x1;
	    pRect->y      = pBox->y1;
	    pRect->width  = pBox->x2 - pBox->x1;
	    pRect->height = pBox->y2 - pBox->y1;
	    pBox++;
	    pRect++;
	}
	XLIB_PROLOGUE (dmxScreen);
	XShapeCombineRectangles(dmxScreen->beDisplay, pWinPriv->window,
				ShapeInput, 0, 0,
				pRectFirst, nRect,
				ShapeSet, YXBanded);
	XLIB_EPILOGUE (dmxScreen);
	xfree(pRectFirst);
    }
    else
    {
	XLIB_PROLOGUE (dmxScreen);
	XShapeCombineMask (dmxScreen->beDisplay, pWinPriv->window,
			   ShapeInput, 0, 0, None, ShapeSet);
	XLIB_EPILOGUE (dmxScreen);
    }
}

/** Set shape of \a pWindow on the back-end server. */
void dmxSetShape(WindowPtr pWindow)
{
    ScreenPtr       pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr   pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);

    DMX_UNWRAP(SetShape, dmxScreen, pScreen);
    if (pScreen->SetShape)
	pScreen->SetShape(pWindow);

    if (pWinPriv->window) {
	/* Handle setting the current shape on the back-end server */
	dmxDoSetShape(pWindow);
	dmxSync(dmxScreen, False);
    }

    pWinPriv->isShaped = TRUE;

    DMX_WRAP(SetShape, dmxSetShape, dmxScreen, pScreen);
}

static void
dmxDoRedirectWindow(WindowPtr pWindow)
{
    ScreenPtr      pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr  pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);

    if (pWinPriv->window && pWinPriv->redirected && !pWinPriv->beRedirected)
    {
	XLIB_PROLOGUE (dmxScreen);
	XCompositeRedirectWindow (dmxScreen->beDisplay,
				  pWinPriv->window,
				  CompositeRedirectAutomatic);
	XLIB_EPILOGUE (dmxScreen);
	pWinPriv->beRedirected = TRUE;
    }
}

static void
dmxDoUpdateWindowPixmap(WindowPtr pWindow)
{
    ScreenPtr      pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    dmxWinPrivPtr  pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);

    if (pWinPriv->window && pWinPriv->beRedirected)
    {
	PixmapPtr pPixmap;

	pPixmap = (*pScreen->GetWindowPixmap) (pWindow);
	if (pPixmap)
	{
	    dmxPixPrivPtr pPixPriv = DMX_GET_PIXMAP_PRIV (pPixmap);

	    if (pPixPriv->pixmap)
	    {
		XLIB_PROLOGUE (dmxScreen);
		XFreePixmap (dmxScreen->beDisplay, pPixPriv->pixmap);
		XLIB_EPILOGUE (dmxScreen);
		pPixPriv->pixmap = None;
	    }

	    XLIB_PROLOGUE (dmxScreen);
	    pPixPriv->pixmap =
		XCompositeNameWindowPixmap (dmxScreen->beDisplay,
					    pWinPriv->window);
	    XLIB_EPILOGUE (dmxScreen);
	}
    }
}

static void
dmxTranslateWindowProperty (WindowPtr     pWindow,
			    char	  type,
			    unsigned char *data,
			    unsigned char *output)
{
    ScreenPtr     pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

    switch (type) {
    case 'a':
    case 'A': {
	Atom *src = (Atom *) data;
	Atom *dst = (Atom *) output;

	*dst = dmxBEAtom (dmxScreen, *src);
    } break;
    case 'p':
    case 'P': {
	XID       *src = (XID *) data;
	XID       *dst = (XID *) output;
	PixmapPtr pPixmap;
	XID       id = *src;

#ifdef PANORAMIX
	if (!noPanoramiXExtension)
	{
	    PanoramiXRes *res;

	    if (dixLookupResource ((pointer *) &res,
				   id,
				   XRT_PIXMAP,
				   serverClient,
				   DixReadAccess) == Success)
		id = res->info[pScreen->myNum].id;
	}
#endif

	if (dixLookupResource ((pointer *) &pPixmap,
			       id,
			       RT_PIXMAP,
			       serverClient,
			       DixReadAccess) == Success)
	    *dst = (DMX_GET_PIXMAP_PRIV (pPixmap))->pixmap;
	else
	    *dst = 0;
    } break;
    case 'm':
    case 'M': {
	XID         *src = (XID *) data;
	XID         *dst = (XID *) output;
	ColormapPtr pColormap;
	XID         id = *src;

#ifdef PANORAMIX
	if (!noPanoramiXExtension)
	{
	    PanoramiXRes *res;

	    if (dixLookupResource ((pointer *) &res,
				   id,
				   XRT_COLORMAP,
				   serverClient,
				   DixReadAccess) == Success)
		id = res->info[pScreen->myNum].id;
	}
#endif

	if (dixLookupResource ((pointer *) &pColormap,
			       id,
			       RT_COLORMAP,
			       serverClient,
			       DixReadAccess) == Success)
	    *dst = (DMX_GET_COLORMAP_PRIV (pColormap))->cmap;
	else
	    *dst = 0;
    } break;
    case 'c':
    case 'C': {
	XID       *src = (XID *) data;
	XID       *dst = (XID *) output;
	CursorPtr pCursor;

	if (dixLookupResource ((pointer *) &pCursor,
			       *src,
			       RT_CURSOR,
			       serverClient,
			       DixReadAccess) == Success)
	    *dst = (DMX_GET_CURSOR_PRIV (pCursor, pScreen))->cursor;
	else
	    *dst = 0;
    } break;
    case 'd':
    case 'D': {
	XID         *src = (XID *) data;
	XID         *dst = (XID *) output;
	DrawablePtr pDrawable;
	XID         id = *src;

#ifdef PANORAMIX
	if (!noPanoramiXExtension)
	{
	    PanoramiXRes *res;

	    if (dixLookupResource ((pointer *) &res,
				   id,
				   XRC_DRAWABLE,
				   serverClient,
				   DixReadAccess) == Success)
		id = res->info[pScreen->myNum].id;
	}
#endif

	if (dixLookupResource ((pointer *) &pDrawable,
			       id,
			       RC_DRAWABLE,
			       serverClient,
			       DixReadAccess) == Success)
	{
	    if (pDrawable->type == DRAWABLE_WINDOW)
	    {
		WindowPtr pWin = (WindowPtr) pDrawable;

		*dst = (DMX_GET_WINDOW_PRIV (pWin))->window;
	    }
	    else
	    {
		PixmapPtr pPixmap = (PixmapPtr) pDrawable;

		*dst = (DMX_GET_PIXMAP_PRIV (pPixmap))->pixmap;
	    }
	}
	else
	    *dst = 0;
    } break;
    case 'f':
    case 'F': {
	XID     *src = (XID *) data;
	XID     *dst = (XID *) output;
	FontPtr pFont;

	if (dixLookupResource ((pointer *) &pFont,
			       *src,
			       RT_FONT,
			       serverClient,
			       DixReadAccess) == Success)
	{
	    dmxFontPrivPtr pFontPriv =
		FontGetPrivate (pFont, dmxFontPrivateIndex);

	    *dst = pFontPriv->font[pScreen->myNum]->fid;
	}
	else
	    *dst = 0;
    } break;
    case 'v':
    case 'V': {
	XID    *src = (XID *) data;
	XID    *dst = (XID *) output;
	Visual *visual;

	visual = dmxLookupVisualFromID (pScreen, *src);
	if (visual)
	    *dst = XVisualIDFromVisual (visual);
	else
	    *dst = 0;
    } break;
    case 'w':
    case 'W': {
	XID       *src = (XID *) data;
	XID       *dst = (XID *) output;
	WindowPtr pWin;
	XID       id = *src;

#ifdef PANORAMIX
	if (!noPanoramiXExtension)
	{
	    PanoramiXRes *res;

	    if (dixLookupResource ((pointer *) &res,
				   id,
				   XRT_WINDOW,
				   serverClient,
				   DixReadAccess) == Success)
		id = res->info[pScreen->myNum].id;
	}
#endif

	if (dixLookupResource ((pointer *) &pWin,
			       id,
			       RT_WINDOW,
			       serverClient,
			       DixReadAccess) == Success)
	    *dst = (DMX_GET_WINDOW_PRIV (pWin))->window;
	else
	    *dst = 0;
    } break;
    default:
	*((CARD32 *) output) = *((CARD32 *) data);
	break;
    }
}

void
dmxBESetWindowProperty (WindowPtr   pWindow,
			PropertyPtr pProp)
{
    ScreenPtr      pScreen = pWindow->drawable.pScreen;
    DMXScreenInfo  *dmxScreen = &dmxScreens[pScreen->myNum];
    unsigned char  *data = pProp->data;
    const char     *format = NULL;
    Window         window;
    int		   i;

    window = dmxBEGetSelectionAdjustedPropertyWindow (pWindow);
    if (!window)
	return;

    /* only 32 bit data types can be translated */
    if (pProp->format == 32)
    {
	switch (pProp->type) {
	case XA_ATOM:
	    format = "a..";
	    break;
	case XA_BITMAP:
	case XA_PIXMAP:
	    format = "p..";
	    break;
	case XA_COLORMAP:
	    format = "m..";
	    break;
	case XA_CURSOR:
	    format = "c..";
	    break;
	case XA_DRAWABLE:
	    format = "d..";
	    break;
	case XA_FONT:
	    format = "f..";
	    break;
	case XA_VISUALID:
	    format = "v..";
	    break;
	case XA_WINDOW:
	    format = "w..";
	    break;
	default:
	    for (i = 0; i < dmxPropTransNum; i++)
	    {
		if (pProp->type == dmxPropTrans[i].type)
		{
		    format = dmxPropTrans[i].format;
		    break;
		}
	    }
	}
    }

    if (format)
    {
	unsigned char *dst, *src = data;

	dst = xalloc (pProp->size * (pProp->format >> 3));
	if (dst)
	{
	    int j;

	    i = j = 0;
	    data = dst;

	    while (format[j] != '\0')
	    {
		if (i++ == pProp->size)
		    break;

		if (format[j] == '.')
		    j = 0;

		dmxTranslateWindowProperty (pWindow, format[j++], src, dst);

		src += (pProp->format >> 3);
		dst += (pProp->format >> 3);
	    }

	    if (i < pProp->size)
		memcpy (dst, src, (pProp->size - i) * (pProp->format >> 3));
	}
    }

    XLIB_PROLOGUE (dmxScreen);
    XChangeProperty (dmxScreen->beDisplay,
		     window,
		     dmxBEAtom (dmxScreen, pProp->propertyName),
		     dmxBEAtom (dmxScreen, pProp->type),
		     pProp->format,
		     PropModeReplace,
		     data,
		     pProp->size);
    XLIB_EPILOGUE (dmxScreen);

    if (data != pProp->data)
	xfree (data);

    dmxSync (dmxScreen, FALSE);
}

