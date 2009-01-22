/*
 * Copyright 2003-2004 Red Hat Inc., Durham, North Carolina.
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
 * Author:
 *   Rickard E. (Rik) Faith <faith@redhat.com>
 *   Kevin E. Martin <kem@redhat.com>
 *
 */

/** \file
 * This file provides the only interface to the X server extension support
 * in programs/Xserver/Xext.  Those programs should only include dmxext.h
 */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmx.h"
#include "dmxinit.h"
#include "dmxextension.h"
#include "dmxwindow.h"
#include "dmxcb.h"
#include "dmxcursor.h"
#include "dmxpixmap.h"
#include "dmxgc.h"
#include "dmxfont.h"
#include "dmxcmap.h"
#include "dmxshm.h"
#ifdef RENDER
#include "dmxpict.h"
#endif
#ifdef RANDR
#include "dmxrandr.h"
#endif
#include "dmxinput.h"
#include "dmxlog.h"
#include "dmxgrab.h"
#include "dmxsync.h"
#include "dmxscrinit.h"
#ifdef XV
#include "dmxxv.h"
#endif

#include "windowstr.h"
#include "inputstr.h"                 /* For DeviceIntRec */
#include <X11/extensions/dmxproto.h>  /* For DMX_BAD_* */
#include "cursorstr.h"
#include "propertyst.h"

#ifdef PANORAMIX
#include "panoramiXsrv.h"
#endif

#define dmxErrorSet(set, error, name, fmt, ...)       \
    if (set) (*set) (error, name, fmt, ##__VA_ARGS__)

#define dmxLogErrorSet(type, set, error, name, fmt, ...) \
    dmxLog (type, fmt "\n", ##__VA_ARGS__);		 \
    dmxErrorSet (set, error, name, fmt, ##__VA_ARGS__)

/* The default font is declared in dix/globals.c, but is not included in
 * _any_ header files. */
extern FontPtr  defaultFont;
    
/** This routine provides information to the DMX protocol extension
 * about a particular screen. */
Bool dmxGetScreenAttributes(int physical, DMXScreenAttributesPtr attr)
{
    DMXScreenInfo *dmxScreen;

    if (physical < 0 || physical >= dmxNumScreens) return FALSE;

    dmxScreen = &dmxScreens[physical];
    attr->name                = dmxScreen->name;
    attr->displayName         = dmxScreen->display;
#ifdef PANORAMIX
    attr->logicalScreen       = noPanoramiXExtension ? dmxScreen->index : 0;
#else
    attr->logicalScreen       = dmxScreen->index;
#endif

    attr->screenWindowWidth   = dmxScreen->scrnWidth;
    attr->screenWindowHeight  = dmxScreen->scrnHeight;
    attr->screenWindowXoffset = 0;
    attr->screenWindowYoffset = 0;

    attr->rootWindowWidth     = WindowTable[physical]->drawable.width;
    attr->rootWindowHeight    = WindowTable[physical]->drawable.height;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	attr->rootWindowWidth  = PanoramiXPixWidth;
	attr->rootWindowHeight = PanoramiXPixHeight;
    }
#endif

    attr->rootWindowXoffset   = dmxScreen->rootX;
    attr->rootWindowYoffset   = dmxScreen->rootY;

    attr->rootWindowXorigin   = 0;
    attr->rootWindowYorigin   = 0;

    return TRUE;
}

/** This routine provides information to the DMX protocol extension
 * about a particular window. */
Bool dmxGetWindowAttributes(WindowPtr pWindow, DMXWindowAttributesPtr attr)
{
    dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);

    attr->screen         = pWindow->drawable.pScreen->myNum;
    attr->window         = pWinPriv->window;

    attr->pos.x          = pWindow->drawable.x;
    attr->pos.y          = pWindow->drawable.y;
    attr->pos.width      = pWindow->drawable.width;
    attr->pos.height     = pWindow->drawable.height;

    if (!pWinPriv->window || pWinPriv->offscreen) {
        attr->vis.x      = 0;
        attr->vis.y      = 0;
        attr->vis.height = 0;
        attr->vis.width  = 0;
        return pWinPriv->window ? TRUE : FALSE;
    }

                                /* Compute display-relative coordinates */
    attr->vis.x          = pWindow->drawable.x;
    attr->vis.y          = pWindow->drawable.y;
    attr->vis.width      = pWindow->drawable.width;
    attr->vis.height     = pWindow->drawable.height;

    if (attr->pos.x < 0) {
        attr->vis.x     -= attr->pos.x;
        attr->vis.width  = attr->pos.x + attr->pos.width - attr->vis.x;
    }
    if (attr->pos.x + attr->pos.width > pWindow->drawable.pScreen->width) {
        if (attr->pos.x < 0)
            attr->vis.width  = pWindow->drawable.pScreen->width;
        else
            attr->vis.width  = pWindow->drawable.pScreen->width - attr->pos.x;
    }
    if (attr->pos.y < 0) {
        attr->vis.y     -= attr->pos.y;
        attr->vis.height = attr->pos.y + attr->pos.height - attr->vis.y;
    }
    if (attr->pos.y + attr->pos.height > pWindow->drawable.pScreen->height) {
        if (attr->pos.y < 0)
            attr->vis.height = pWindow->drawable.pScreen->height;
        else
            attr->vis.height = pWindow->drawable.pScreen->height - attr->pos.y;
    }

                                /* Convert to window-relative coordinates */
    attr->vis.x -= attr->pos.x;
    attr->vis.y -= attr->pos.y;

    return TRUE;
}

/** This routine provides information to the DMX protocol extension
 * about a particular window. */
Bool dmxGetDrawableAttributes(DrawablePtr pDraw, DMXWindowAttributesPtr attr)
{
    if ((pDraw->type == UNDRAWABLE_WINDOW) || (pDraw->type == DRAWABLE_WINDOW))
    {
	WindowPtr pWindow = (WindowPtr) pDraw;
	dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV (pWindow);

	attr->screen         = pWindow->drawable.pScreen->myNum;
	attr->window         = pWinPriv->window;

	attr->pos.x          = pWindow->drawable.x;
	attr->pos.y          = pWindow->drawable.y;
	attr->pos.width      = pWindow->drawable.width;
	attr->pos.height     = pWindow->drawable.height;

	if (!pWinPriv->window || pWinPriv->offscreen) {
	    attr->vis.x      = 0;
	    attr->vis.y      = 0;
	    attr->vis.height = 0;
	    attr->vis.width  = 0;
	    return pWinPriv->window ? TRUE : FALSE;
	}

	/* Compute display-relative coordinates */
	attr->vis.x          = pWindow->drawable.x;
	attr->vis.y          = pWindow->drawable.y;
	attr->vis.width      = pWindow->drawable.width;
	attr->vis.height     = pWindow->drawable.height;

	if (attr->pos.x < 0) {
	    attr->vis.x     -= attr->pos.x;
	    attr->vis.width  = attr->pos.x + attr->pos.width - attr->vis.x;
	}
	if (attr->pos.x + attr->pos.width > pWindow->drawable.pScreen->width) {
	    if (attr->pos.x < 0)
		attr->vis.width  = pWindow->drawable.pScreen->width;
	    else
		attr->vis.width  = pWindow->drawable.pScreen->width - attr->pos.x;
	}
	if (attr->pos.y < 0) {
	    attr->vis.y     -= attr->pos.y;
	    attr->vis.height = attr->pos.y + attr->pos.height - attr->vis.y;
	}
	if (attr->pos.y + attr->pos.height > pWindow->drawable.pScreen->height) {
	    if (attr->pos.y < 0)
		attr->vis.height = pWindow->drawable.pScreen->height;
	    else
		attr->vis.height = pWindow->drawable.pScreen->height - attr->pos.y;
	}

	/* Convert to window-relative coordinates */
	attr->vis.x -= attr->pos.x;
	attr->vis.y -= attr->pos.y;
    }
    else
    {
	PixmapPtr pPixmap = (PixmapPtr) pDraw;
	dmxPixPrivPtr pPixPriv = DMX_GET_PIXMAP_PRIV (pPixmap);
	xRectangle empty = { 0, 0, 0, 0 };

	attr->screen = pDraw->pScreen->myNum;
	attr->window = pPixPriv->pixmap;
	attr->pos = attr->vis = empty;
    }

    return TRUE;
}

void dmxGetDesktopAttributes(DMXDesktopAttributesPtr attr)
{
    attr->width  = dmxGlobalWidth;
    attr->height = dmxGlobalHeight;
    attr->shiftX = 0;            /* NOTE: The upper left hand corner of */
    attr->shiftY = 0;            /*       the desktop is always <0,0>. */
}

/** Return the total number of devices, not just #dmxNumInputs.  The
 * number returned should be the same as that returned by
 * XListInputDevices. */
int dmxGetInputCount(void)
{
    int i, total;
    
    for (total = i = 0; i < dmxNumScreens; i++)
	total += dmxScreens[i].input.numDevs;

    return total;
}

/** Return information about the device with id = \a deviceId.  This
 * information is primarily for the #ProcDMXGetInputAttributes()
 * function, which does not have access to the appropriate data
 * structure. */
int dmxGetInputAttributes(int deviceId, DMXInputAttributesPtr attr)
{
    int          i, j;
    DMXInputInfo *dmxInput;

    if (deviceId < 0) return -1;
    for (i = 0; i < dmxNumScreens; i++) {
        dmxInput = &dmxScreens[i].input;
        for (j = 0; j < dmxInput->numDevs; j++) {
            if (deviceId != dmxInput->devs[i]->id) continue;
            attr->isCore         = FALSE;
            attr->sendsCore      = TRUE;
            attr->detached       = FALSE;
	    attr->inputType      = 2;
	    attr->physicalScreen = i;
	    attr->name           = dmxInput->devs[i]->name;
	    attr->physicalId     = deviceId;
	    return 0;           /* Success */
        }
    }
    return -1;                  /* Failure */
}

/** Add an input with the specified attributes.  If the input is added,
 * the physical id is returned in \a deviceId. */
int dmxAddInput(DMXInputAttributesPtr attr, int *id)
{
    if (attr->physicalScreen < 0 || attr->physicalScreen >= dmxNumScreens)
	return BadValue;

    if (attr->inputType == 2)
    {
	DMXScreenInfo *dmxScreen = &dmxScreens[attr->physicalScreen];
	int           ret;

	if (dmxScreen->beDisplay)
	{
	    ret = dmxInputAttach (&dmxScreen->input);
	    if (ret != Success)
		return ret;

	    dmxInputEnable (&dmxScreen->input);
	}
	else
	{
	    dmxScreen->beDisplay = dmxScreen->beAttachedDisplay;
	    ret = dmxInputAttach (&dmxScreen->input);
	    dmxScreen->beDisplay = NULL;
	
	    if (ret != Success)
		return ret;
	}

	*id = attr->physicalScreen;

	return ret;
    }

    return BadValue;
}

/** Remove the input with physical id \a id. */
int dmxRemoveInput(int id)
{
    int ret;

    if (id < 0 || id >= dmxNumScreens)
	return BadValue;

    if (dmxScreens[id].beDisplay)
    {
	dmxInputDisable (&dmxScreens[id].input);
	ret = dmxInputDetach (&dmxScreens[id].input);
    }
    else
    {
	dmxScreens[id].beDisplay = dmxScreens[id].beAttachedDisplay;
	ret = dmxInputDetach (&dmxScreens[id].input);
	dmxScreens[id].beDisplay = NULL;

    }

    return ret;
}

/** Return the value of #dmxNumScreens -- the total number of backend
 * screens in use (these are logical screens and may be larger than the
 * number of backend displays). */
unsigned long dmxGetNumScreens(void)
{
    return dmxNumScreens;
}

/** Make sure that #dmxCreateAndRealizeWindow has been called for \a
 * pWindow. */
void dmxForceWindowCreation(WindowPtr pWindow)
{
    dmxWinPrivPtr pWinPriv = DMX_GET_WINDOW_PRIV(pWindow);
    if (!pWinPriv->window) dmxCreateAndRealizeWindow(pWindow, TRUE);
}

/** Flush pending syncs for all screens. */
void dmxFlushPendingSyncs(void)
{
    dmxSync(NULL, TRUE);
}

/** Update DMX's screen resources to match those of the newly moved
 *  and/or resized "root" window. */
void dmxUpdateScreenResources(ScreenPtr pScreen, int x, int y, int w, int h)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
    WindowPtr      pRoot     = WindowTable[pScreen->myNum];
    WindowPtr      pChild;
    Bool           anyMarked = FALSE;

    /* Handle special case where width and/or height are zero */
    if (w == 0 || h == 0) {
	w = 1;
	h = 1;
    }

    /* Change screen size */
    pScreen->width  = w;
    pScreen->height = h;

    /* Reset the root window's drawable's size */
    pRoot->drawable.width  = w;
    pRoot->drawable.height = h;

    /* Set the root window's new winSize and borderSize */
    pRoot->winSize.extents.x1 = 0;
    pRoot->winSize.extents.y1 = 0;
    pRoot->winSize.extents.x2 = w;
    pRoot->winSize.extents.y2 = h;

    pRoot->borderSize.extents.x1 = 0;
    pRoot->borderSize.extents.y1 = 0;
    pRoot->borderSize.extents.x2 = w;
    pRoot->borderSize.extents.y2 = h;

    /* Recompute this screen's mmWidth & mmHeight */
    pScreen->mmWidth =
	(w * 254 + dmxScreen->beXDPI * 5) / (dmxScreen->beXDPI * 10);
    pScreen->mmHeight =
	(h * 254 + dmxScreen->beYDPI * 5) / (dmxScreen->beYDPI * 10);

    /* Recompute this screen's window's clip rects as follows: */
    /*   1. Mark all of root's children's windows */
    for (pChild = pRoot->firstChild; pChild; pChild = pChild->nextSib)
	anyMarked |= pScreen->MarkOverlappedWindows(pChild, pChild,
						    (WindowPtr *)NULL);

    /*   2. Set the root window's borderClip */
    pRoot->borderClip.extents.x1 = 0;
    pRoot->borderClip.extents.y1 = 0;
    pRoot->borderClip.extents.x2 = w;
    pRoot->borderClip.extents.y2 = h;

    /*   3. Set the root window's clipList */
    if (anyMarked) {
	/* If any windows have been marked, set the root window's
	 * clipList to be broken since it will be recalculated in
	 * ValidateTree()
	 */
	REGION_BREAK(pScreen, &pRoot->clipList);
    } else {
	/* Otherwise, we just set it directly since there are no
	 * windows visible on this screen
	 */
	pRoot->clipList.extents.x1 = 0;
	pRoot->clipList.extents.y1 = 0;
	pRoot->clipList.extents.x2 = w;
	pRoot->clipList.extents.y2 = h;
    }

    /*   4. Revalidate all clip rects and generate expose events */
    if (anyMarked) {
	pScreen->ValidateTree(pRoot, NULL, VTBroken);
	pScreen->HandleExposures(pRoot);
	if (pScreen->PostValidateTree)
	    pScreen->PostValidateTree(pRoot, NULL, VTBroken);
    }
}

/** Create the scratch GCs per depth. */
static void dmxBECreateScratchGCs(int scrnNum)
{
    ScreenPtr  pScreen = screenInfo.screens[scrnNum];
    GCPtr     *ppGC    = pScreen->GCperDepth;
    int        i;

    for (i = 0; i <= pScreen->numDepths; i++)
	dmxBECreateGC(pScreen, ppGC[i]);
}

#ifdef PANORAMIX
static Bool FoundPixImage;

extern unsigned long	XRT_PICTURE;

/** Search the Xinerama XRT_PIXMAP resources for the pixmap that needs
 *  to have its image restored.  When it is found, see if there is
 *  another screen with the same image.  If so, copy the pixmap image
 *  from the existing screen to the newly created pixmap. */
static void dmxBERestorePixmapImage(pointer value, XID id, RESTYPE type,
				    pointer p)
{
    PixmapPtr pSrc = NULL;
    PixmapPtr pDst = (PixmapPtr) p;
    int       idx      = pDst->drawable.pScreen->myNum;
    int       i;

    if ((type & TypeMask) == (XRT_PIXMAP & TypeMask)) {
	int            idx      = pDst->drawable.pScreen->myNum;
	PanoramiXRes  *pXinPix  = (PanoramiXRes *)value;
	PixmapPtr      pPix;

	pPix = (PixmapPtr)LookupIDByType(pXinPix->info[idx].id, RT_PIXMAP);
	if (pPix != pDst) return; /* Not a match.... Next! */

	for (i = 0; i < PanoramiXNumScreens; i++)
	{
	    if (i == idx) continue; /* Self replication is bad */

	    pSrc =
		(PixmapPtr)LookupIDByType(pXinPix->info[i].id, RT_PIXMAP);
	    break;
	}
    }

#ifdef RENDER
    else if ((type & TypeMask) == (XRT_PICTURE & TypeMask))
    {
	int           idx      = pDst->drawable.pScreen->myNum;
	PanoramiXRes  *pXinPic  = (PanoramiXRes *) value;
	PicturePtr    pPic;

	pPic = (PicturePtr) LookupIDByType (pXinPic->info[idx].id,
					    PictureType);

	/* Not a match.... Next! */
	if (!pPic || !pPic->pDrawable) return;
	if (pPic->pDrawable->type != DRAWABLE_PIXMAP) return;
	if (pPic->pDrawable != (DrawablePtr) pDst) return;

	for (i = 0; i < PanoramiXNumScreens; i++)
	{
	    if (i == idx) continue; /* Self replication is bad */

	    pPic = (PicturePtr) LookupIDByType (pXinPic->info[i].id,
						PictureType);
	    pSrc = (PixmapPtr) pPic->pDrawable;
	    break;
	}
    }
#endif

    else if ((type & TypeMask) == (XRT_WINDOW & TypeMask))
    {
	PixmapPtr      pDst     = (PixmapPtr)p;
	int            idx      = pDst->drawable.pScreen->myNum;
	PanoramiXRes  *pXinWin  = (PanoramiXRes *) value;
	Bool	      border;
	WindowPtr     pWin;

	pWin = (WindowPtr) LookupIDByType (pXinWin->info[idx].id, RT_WINDOW);

	if (!pWin) return;
	if (!pWin->borderIsPixel && pWin->border.pixmap == pDst)
	{
	    border = TRUE;
	}
	else if (pWin->backgroundState == BackgroundPixmap &&
		 pWin->background.pixmap == pDst)
	{
	    border = FALSE;
	}
	else
	{
	    return;
	}

	for (i = 0; i < PanoramiXNumScreens; i++)
	{
	    if (i == idx) continue; /* Self replication is bad */

	    pWin = (WindowPtr) LookupIDByType (pXinWin->info[i].id, RT_WINDOW);

	    if (border)
		pSrc = pWin->border.pixmap;
	    else
		pSrc = pWin->background.pixmap;

	    break;
	}
    }

    if (pSrc)
    {
	dmxPixPrivPtr pSrcPriv = DMX_GET_PIXMAP_PRIV (pSrc);

	if (pSrcPriv->pixmap)
	{
	    DMXScreenInfo *dmxSrcScreen = &dmxScreens[i];
	    DMXScreenInfo *dmxDstScreen = &dmxScreens[idx];
	    dmxPixPrivPtr  pDstPriv = DMX_GET_PIXMAP_PRIV(pDst);
	    XImage        *img = NULL;
	    int            j;
	    XlibGC         gc = NULL;

	    /* This should never happen, but just in case.... */
	    if (pSrc->drawable.width  != pDst->drawable.width ||
		pSrc->drawable.height != pDst->drawable.height)
		return;

	    XLIB_PROLOGUE (dmxSrcScreen);

	    /* Copy from src pixmap to dst pixmap */
	    img = XGetImage(dmxSrcScreen->beDisplay,
			    pSrcPriv->pixmap,
			    0, 0,
			    pSrc->drawable.width, pSrc->drawable.height,
			    -1,
			    ZPixmap);

	    XLIB_EPILOGUE (dmxSrcScreen);

	    if (img)
	    {
		for (j = 0; j < dmxDstScreen->beNumPixmapFormats; j++) {
		    if (dmxDstScreen->bePixmapFormats[j].depth == img->depth) {
			unsigned long  m;
			XGCValues      v;

			m = GCFunction | GCPlaneMask | GCClipMask;
			v.function = GXcopy;
			v.plane_mask = AllPlanes;
			v.clip_mask = None;

			XLIB_PROLOGUE (dmxDstScreen);
			gc = XCreateGC(dmxDstScreen->beDisplay,
				       dmxDstScreen->scrnDefDrawables[j],
				       m, &v);
			XLIB_EPILOGUE (dmxDstScreen);
			break;
		    }
		}

		if (gc) {
		    XLIB_PROLOGUE (dmxDstScreen);
		    XPutImage(dmxDstScreen->beDisplay,
			      pDstPriv->pixmap,
			      gc, img, 0, 0, 0, 0,
			      pDst->drawable.width, pDst->drawable.height);
		    XFreeGC(dmxDstScreen->beDisplay, gc);
		    XLIB_EPILOGUE (dmxDstScreen);
		    FoundPixImage = True;
		} else {
		    dmxLog(dmxWarning, "Could not create GC\n");
		}

		XDestroyImage(img);
	    } else {
		dmxLog(dmxWarning, "Could not create image\n");
	    }
	}
    }
}
#endif

/** Restore the pixmap image either from another screen or from an image
 *  that was saved when the screen was previously detached. */
static void dmxBERestorePixmap(PixmapPtr pPixmap)
{
#ifdef PANORAMIX
    int i;

    /* If Xinerama is not active, there's nothing we can do (see comment
     * in #else below for more info). */
    if (noPanoramiXExtension) {
	dmxLog(dmxWarning, "Cannot restore pixmap image\n");
	return;
    }

    FoundPixImage = False;
    for (i = currentMaxClients; --i >= 0; )
	if (clients[i])
	    FindAllClientResources(clients[i], dmxBERestorePixmapImage,
				   (pointer)pPixmap);

    /* No corresponding pixmap image was found on other screens, so we
     * need to copy it from the saved image when the screen was detached
     * (if available). */
    if (!FoundPixImage) {
	dmxPixPrivPtr  pPixPriv = DMX_GET_PIXMAP_PRIV(pPixmap);

	if (pPixPriv->detachedImage) {
	    ScreenPtr      pScreen   = pPixmap->drawable.pScreen;
	    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];
	    XlibGC         gc        = NULL;

	    for (i = 0; i < dmxScreen->beNumPixmapFormats; i++) {
		if (dmxScreen->bePixmapFormats[i].depth ==
		    pPixPriv->detachedImage->depth) {
		    unsigned long  m;
		    XGCValues      v;

		    m = GCFunction | GCPlaneMask | GCClipMask;
		    v.function = GXcopy;
		    v.plane_mask = AllPlanes;
		    v.clip_mask = None;

		    XLIB_PROLOGUE (dmxScreen);
		    gc = XCreateGC(dmxScreen->beDisplay,
				   dmxScreen->scrnDefDrawables[i],
				   m, &v);
		    XLIB_EPILOGUE (dmxScreen);
		    break;
		}
	    }

	    if (gc) {
		XLIB_PROLOGUE (dmxScreen);
		XPutImage(dmxScreen->beDisplay,
			  pPixPriv->pixmap,
			  gc,
			  pPixPriv->detachedImage,
			  0, 0, 0, 0,
		      pPixmap->drawable.width, pPixmap->drawable.height);
		XFreeGC(dmxScreen->beDisplay, gc);
		XLIB_EPILOGUE (dmxScreen);
	    } else {
		dmxLog(dmxWarning, "Cannot restore pixmap image\n");
	    }

	    XDestroyImage(pPixPriv->detachedImage);
	    pPixPriv->detachedImage = NULL;
	} else {
	    dmxLog(dmxWarning, "Cannot restore pixmap image: %dx%d - %d\n",
		   pPixmap->drawable.width,
		   pPixmap->drawable.height,
		   pPixmap->drawable.depth);
	}
    }
#else
    /* If Xinerama is not enabled, then there is no other copy of the
     * pixmap image that we can restore.  Saving all pixmap data is not
     * a feasible option since there is no mechanism for updating pixmap
     * data when a screen is detached, which means that the data that
     * was previously saved would most likely be out of date. */
    dmxLog(dmxWarning, "Cannot restore pixmap image\n");
    return;
#endif
}

/** Create resources on the back-end server.  This function is called
 *  from #dmxAttachScreen() via the dix layer's FindAllResources
 *  function.  It walks all resources, compares them to the screen
 *  number passed in as \a n and calls the appropriate DMX function to
 *  create the associated resource on the back-end server. */
static void dmxBECreateResources(pointer value, XID id, RESTYPE type,
				 pointer n)
{
    int        scrnNum = (int)n;
    ScreenPtr  pScreen = screenInfo.screens[scrnNum];

    if ((type & TypeMask) == (RT_WINDOW & TypeMask)) {
	/* Window resources are created below in dmxBECreateWindowTree */
    } else if ((type & TypeMask) == (RT_PIXMAP & TypeMask)) {
	PixmapPtr  pPix = value;
	if (pPix->drawable.pScreen->myNum == scrnNum) {
	    dmxBECreatePixmap(pPix);
	    dmxBERestorePixmap(pPix);
	}
    } else if ((type & TypeMask) == (RT_GC & TypeMask)) {
	GCPtr  pGC = value;
	if (pGC->pScreen->myNum == scrnNum) {
	    /* Create the GC on the back-end server */
	    dmxBECreateGC(pScreen, pGC);
	    /* Create any pixmaps associated with this GC */
	    if (!pGC->tileIsPixel) {
		dmxBECreatePixmap(pGC->tile.pixmap);
		dmxBERestorePixmap(pGC->tile.pixmap);
	    }
	    if (pGC->stipple != pScreen->PixmapPerDepth[0]) {
		dmxBECreatePixmap(pGC->stipple);
		dmxBERestorePixmap(pGC->stipple);
	    }
	    if (pGC->font != defaultFont) {
		(void)dmxBELoadFont(pScreen, pGC->font);
	    }
	    /* Update the GC on the back-end server */
	    dmxChangeGC(pGC, -1L);
	}
    } else if ((type & TypeMask) == (RT_FONT & TypeMask)) {
	(void)dmxBELoadFont(pScreen, (FontPtr)value);
    } else if ((type & TypeMask) == (RT_CURSOR & TypeMask)) {
	dmxBECreateCursor (pScreen, (CursorPtr) value);
    } else if ((type & TypeMask) == (RT_PASSIVEGRAB & TypeMask)) {
	GrabPtr grab = value;
	if (grab->cursor)
	    dmxBECreateCursor (pScreen, grab->cursor);
    } else if ((type & TypeMask) == (RT_COLORMAP & TypeMask)) {
	ColormapPtr  pCmap = value;
	if (pCmap->pScreen->myNum == scrnNum)
	    (void)dmxBECreateColormap((ColormapPtr)value);
    } else if ((type & TypeMask) == (DMX_SHMSEG & TypeMask)) {
	dmxBEAttachShmSeg (&dmxScreens[scrnNum], (dmxShmSegInfoPtr) value);
#if 0
#ifdef RENDER
    /* TODO: Recreate Picture and GlyphSet resources */
    } else if ((type & TypeMask) == (PictureType & TypeMask)) {
	/* Picture resources are created when windows are created */
    } else if ((type & TypeMask) == (GlyphSetType & TypeMask)) {
	dmxBEFreeGlyphSet(pScreen, (GlyphSetPtr)value);
#endif
#endif
    } else {
	/* Other resource types??? */
    }
}

/** Create window hierachy on back-end server.  The window tree is
 *  created in a special order (bottom most subwindow first) so that the
 *  #dmxCreateNonRootWindow() function does not need to recursively call
 *  itself to create each window's parents.  This is required so that we
 *  have the opportunity to create each window's border and background
 *  pixmaps (where appropriate) before the window is created. */
static void dmxBECreateWindowTree(int idx)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[idx];
    WindowPtr      pRoot     = WindowTable[idx];
    dmxWinPrivPtr  pWinPriv  = DMX_GET_WINDOW_PRIV(pRoot);
    WindowPtr      pWin;

    /* Create the pixmaps associated with the root window */
    if (!pRoot->borderIsPixel) {
	dmxBECreatePixmap(pRoot->border.pixmap);
	dmxBERestorePixmap(pRoot->border.pixmap);
    }
    if (pRoot->backgroundState == BackgroundPixmap) {
	dmxBECreatePixmap(pRoot->background.pixmap);
	dmxBERestorePixmap(pRoot->background.pixmap);
    }

    dmxBECreateCursor (screenInfo.screens[idx], pRoot->optional->cursor);

    /* Create root window first */
    dmxScreen->rootWin = pWinPriv->window = dmxCreateRootWindow(pRoot);

#ifdef RENDER
    if (pWinPriv->hasPict) dmxCreatePictureList (pRoot);
#endif

    pWin = pRoot->lastChild;
    while (pWin) {
	pWinPriv = DMX_GET_WINDOW_PRIV(pWin);

	/* Create the pixmaps regardless of whether or not the
	 * window is created or not due to lazy window creation.
	 */
	if (!pWin->borderIsPixel) {
	    dmxBECreatePixmap(pWin->border.pixmap);
	    dmxBERestorePixmap(pWin->border.pixmap);
	}
	if (pWin->backgroundState == BackgroundPixmap) {
	    dmxBECreatePixmap(pWin->background.pixmap);
	    dmxBERestorePixmap(pWin->background.pixmap);
	}

	if (wUseDefault(pWin, cursor, 0))
	    dmxBECreateCursor (screenInfo.screens[idx],
			       pWin->optional->cursor);

	/* Reset the window attributes */
	dmxGetDefaultWindowAttributes(pWin,
				      &pWinPriv->cmap,
				      &pWinPriv->visual);

	/* Create the window */
	dmxCreateAndRealizeWindow(pWin, TRUE);

	/* Next, create the bottom-most child */
	if (pWin->lastChild) {
	    pWin = pWin->lastChild;
	    continue;
	}

	/* If the window has no children, move on to the next higher window */
	while (!pWin->prevSib && (pWin != pRoot))
	    pWin = pWin->parent;

	if (pWin->prevSib) {
	    pWin = pWin->prevSib;
	    continue;
	}

	/* When we reach the root window, we are finished */
	if (pWin == pRoot)
	    break;
    }
}

static void dmxBEMapRootWindow(int idx)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[idx];

    XLIB_PROLOGUE (dmxScreen);
    XMapWindow(dmxScreen->beDisplay, dmxScreen->rootWin);
    XLIB_EPILOGUE (dmxScreen);
}

static void dmxBECreateWindowProperties (int idx)
{
    WindowPtr      pRoot  = WindowTable[idx];
    WindowPtr      pRoot0 = pRoot;
    WindowPtr      pWin;
    WindowPtr      pWin0;
    PropertyPtr    pProp, pLast;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
	pRoot0 = WindowTable[0];
#endif

    pWin = pRoot;
    pWin0 = pRoot0;
    while (pWin)
    {
	pLast = NULL;
	do
	{
	    for (pProp = wUserProps (pWin0); pProp; pProp = pProp->next)
	    	if (pProp->next == pLast)
		    break;

	    if (pProp)
		dmxBESetWindowProperty (pWin, (pLast = pProp));
	} while (pLast != wUserProps (pWin0));

	/* Next, create the bottom-most child */
	if (pWin->lastChild) {
	    pWin = pWin->lastChild;
	    pWin0 = pWin0->lastChild;
	    continue;
	}

	/* If the window has no children, move on to the next higher window */
	while (!pWin->prevSib && (pWin != pRoot))
	{
	    pWin = pWin->parent;
	    pWin0 = pWin0->parent;
	}

	if (pWin->prevSib) {
	    pWin = pWin->prevSib;
	    pWin0 = pWin0->prevSib;
	    continue;
	}

	/* When we reach the root window, we are finished */
	if (pWin == pRoot)
	    break;
    }
}

/** Compare the new and old screens to see if they are compatible. */
static Bool dmxCompareScreens(DMXScreenInfo      *new,
			      DMXScreenInfo      *old,
			      dmxErrorSetProcPtr errorSet,
			      void		 *error,
			      const char         *errorName)
{
    int i;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
    {
	ScreenPtr pScreen = screenInfo.screens[0];
	int       j;

	old = dmxScreens; /* each new screen must match the first screen */

	if (new->beDepth != old->beDepth)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "Screen depth is not %d",
			    old->beDepth);
	    return FALSE;
	}

	if (new->beBPP != old->beBPP)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "Screen BPP is not %d",
			    old->beBPP);
	    return FALSE;
	}

	for (i = 0; i < old->beNumDepths; i++)
	{
	    for (j = 0; j < new->beNumDepths; j++)
		if (new->beDepths[j] == old->beDepths[i])
		    break;

	    if (j == new->beNumDepths)
	    {
		dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
				"Screen doesn't support depth %d",
				old->beDepths[i]);
		return FALSE;
	    }
	}

	for (i = 0; i < old->beNumPixmapFormats; i++)
	{
	    for (j = 0; j < new->beNumPixmapFormats; j++)
		if (new->bePixmapFormats[j].depth ==
		    old->bePixmapFormats[i].depth &&
		    new->bePixmapFormats[j].bits_per_pixel ==
		    old->bePixmapFormats[i].bits_per_pixel &&
		    new->bePixmapFormats[j].scanline_pad ==
		    old->bePixmapFormats[i].scanline_pad)
		    break;

	    if (j == new->beNumPixmapFormats)
	    {
		dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
				"Screen doesn't support pixmap format "
				"depth=%d,bits_per_pixel=%d,scanline_pad=%d\n",
				old->bePixmapFormats[i].depth,
				old->bePixmapFormats[i].bits_per_pixel,
				old->bePixmapFormats[i].scanline_pad);
		return FALSE;
	    }
	}

	for (i = 0; i < pScreen->numVisuals; i++)
	{
	    int k;

	    for (k = 0; k < old->beNumVisuals; k++)
		if (pScreen->visuals[i].class ==
		    old->beVisuals[k].class &&
		    pScreen->visuals[i].bitsPerRGBValue ==
		    old->beVisuals[k].bits_per_rgb &&
		    pScreen->visuals[i].ColormapEntries ==
		    old->beVisuals[k].colormap_size &&
		    pScreen->visuals[i].nplanes ==
		    old->beVisuals[k].depth &&
		    pScreen->visuals[i].redMask ==
		    old->beVisuals[k].red_mask &&
		    pScreen->visuals[i].greenMask ==
		    old->beVisuals[k].green_mask &&
		    pScreen->visuals[i].blueMask ==
		    old->beVisuals[k].blue_mask)
		    break;

	    assert (k < old->beNumVisuals);

	    for (j = 0; j < new->beNumVisuals; j++)
		if (new->beVisuals[j].depth ==
		    old->beVisuals[k].depth &&
		    new->beVisuals[j].class ==
		    old->beVisuals[k].class &&
		    new->beVisuals[j].red_mask ==
		    old->beVisuals[k].red_mask &&
		    new->beVisuals[j].green_mask ==
		    old->beVisuals[k].green_mask &&
		    new->beVisuals[j].blue_mask ==
		    old->beVisuals[k].blue_mask &&
		    new->beVisuals[j].bits_per_rgb >=
		    old->beVisuals[k].bits_per_rgb &&
		    new->beVisuals[j].colormap_size >=
		    old->beVisuals[k].colormap_size)
		    break;

	    if (j == new->beNumVisuals)
	    {
		dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
				"Screen doesn't support visual: "
				"class: %s, "
				"depth: %d planes, "
				"available colormap entries: %d%s, "
				"red/green/blue masks: 0x%lx/0x%lx/0x%lx, "
				"significant bits in color specification: "
				"%d bits",
				old->beVisuals[k].class == StaticGray ?
				"StaticGray" :
				old->beVisuals[k].class == GrayScale ?
				"GrayScale" :
				old->beVisuals[k].class == StaticColor ?
				"StaticColor" :
				old->beVisuals[k].class == PseudoColor ?
				"PseudoColor" :
				old->beVisuals[k].class == TrueColor ?
				"TrueColor" :
				old->beVisuals[k].class == DirectColor ?
				"DirectColor" :
				"Unknown Class",
				old->beVisuals[k].depth,
				old->beVisuals[k].colormap_size,
				(old->beVisuals[k].class == TrueColor ||
				 old->beVisuals[k].class == DirectColor) ?
				" per subfield" : "",
				old->beVisuals[k].red_mask,
				old->beVisuals[k].green_mask,
				old->beVisuals[k].blue_mask,
				old->beVisuals[k].bits_per_rgb);
		return FALSE;
	    }
	}

	return TRUE;
    }
#endif

    if (new->beWidth != old->beWidth || new->beHeight != old->beHeight)
    {
	dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			"Screen dimensions are not %dx%d",
			old->beWidth, old->beHeight);
	return FALSE;
    }
    if (new->beDepth != old->beDepth)
    {
	dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			"Screen depth is not %d",
			old->beDepth);
	return FALSE;
    }
    if (new->beBPP != old->beBPP)
    {
	dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			"Screen BPP is not %dx%d",
			old->beBPP);
	return FALSE;
    }
    if (new->beNumDepths != old->beNumDepths)
    {
	dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			"Screen number of depths is not %d",
			old->beNumDepths);
	return FALSE;
    }

    for (i = 0; i < old->beNumDepths; i++)
	if (new->beDepths[i] != old->beDepths[i])
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "Screen depth index %d is not %d",
			    i, old->beDepths[i]);
	    return FALSE;
	}

    if (new->beNumPixmapFormats != old->beNumPixmapFormats)
    {
	dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			"Screen number of pixmap formats is not %d",
			old->beNumPixmapFormats);
	return FALSE;
    }
    for (i = 0; i < old->beNumPixmapFormats; i++) {
	if (new->bePixmapFormats[i].depth !=
	    old->bePixmapFormats[i].depth)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "depth of screen pixmap format index %d is not %d",
			    i, old->bePixmapFormats[i].depth);
	    return FALSE;
	}
	if (new->bePixmapFormats[i].bits_per_pixel !=
	    old->bePixmapFormats[i].bits_per_pixel)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "bits_per_pixel of screen pixmap format "
			    "index %d is not %d",
			    i, old->bePixmapFormats[i].bits_per_pixel);
	    return FALSE;
	}
	if (new->bePixmapFormats[i].scanline_pad !=
	    old->bePixmapFormats[i].scanline_pad)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "scanline_pad of screen pixmap format "
			    "index %d is not %d",
			    i, old->bePixmapFormats[i].scanline_pad);
	    return FALSE;
	}
    }

    if (new->beNumVisuals != old->beNumVisuals)
    {
	dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			"Screen number of visuals is not %d",
			old->beNumVisuals);
	return FALSE;
    }
    for (i = 0; i < old->beNumVisuals; i++) {
	if (new->beVisuals[i].visualid !=
	    old->beVisuals[i].visualid)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "visualid of screen visual "
			    "index %d is not %d",
			    i, old->beVisuals[i].visualid);
	    return FALSE;
	}
	if (new->beVisuals[i].screen !=
	    old->beVisuals[i].screen)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "screen of screen visual "
			    "index %d is not %d",
			    i, old->beVisuals[i].screen);
	    return FALSE;
	}
	if (new->beVisuals[i].depth !=
	    old->beVisuals[i].depth)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "depth of screen visual "
			    "index %d is not %d",
			    i, old->beVisuals[i].depth);
	    return FALSE;
	}
	if (new->beVisuals[i].class !=
	    old->beVisuals[i].class)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "class of screen visual "
			    "index %d is not %d",
			    i, old->beVisuals[i].class);
	    return FALSE;
	}
	if (new->beVisuals[i].red_mask !=
	    old->beVisuals[i].red_mask)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "red_mask of screen visual "
			    "index %d is not %d",
			    i, old->beVisuals[i].red_mask);
	    return FALSE;
	}
	if (new->beVisuals[i].green_mask !=
	    old->beVisuals[i].green_mask)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "green_mask of screen visual "
			    "index %d is not %d",
			    i, old->beVisuals[i].green_mask);
	    return FALSE;
	}
	if (new->beVisuals[i].blue_mask !=
	    old->beVisuals[i].blue_mask)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "blue_mask of screen visual "
			    "index %d is not %d",
			    i, old->beVisuals[i].blue_mask);
	    return FALSE;
	}
	if (new->beVisuals[i].colormap_size !=
	    old->beVisuals[i].colormap_size)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "colormap_size of screen visual "
			    "index %d is not %d",
			    i, old->beVisuals[i].colormap_size);
	    return FALSE;
	}
	if (new->beVisuals[i].bits_per_rgb !=
	    old->beVisuals[i].bits_per_rgb)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "bits_per_rgb of screen visual "
			    "index %d is not %d",
			    i, old->beVisuals[i].bits_per_rgb);
	    return FALSE;
	}
    }

    if (new->beDefVisualIndex != old->beDefVisualIndex)
    {
	dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			"Screen default visual index is not %d",
			old->beDefVisualIndex);
	return FALSE;
    }

    return TRUE;
}

#ifdef RENDER
/** Restore Render's picture */
static void dmxBERestoreRenderPict(pointer value, XID id, pointer n)
{
    PicturePtr   pPicture = value;               /* The picture */
    DrawablePtr  pDraw    = pPicture->pDrawable; /* The picture's drawable */
    int          scrnNum  = (int)n;

    if (pDraw)
    {
	if (pDraw->pScreen->myNum != scrnNum) {
	    /* Picture not on the screen we are restoring*/
	    return;
	}

	if (pDraw->type == DRAWABLE_PIXMAP) {
	    PixmapPtr  pPixmap = (PixmapPtr)pDraw;
	
	    /* Create and restore the pixmap drawable */
	    dmxBECreatePixmap(pPixmap);
	    dmxBERestorePixmap(pPixmap);
	}
    }

    dmxBECreatePicture(scrnNum, pPicture);
}

/** Restore Render's glyphs */
static void dmxBERestoreRenderGlyph(pointer value, XID id, pointer n)
{
    GlyphSetPtr      glyphSet   = value;
    int              scrnNum    = (int)n;
    dmxGlyphPrivPtr  glyphPriv  = DMX_GET_GLYPH_PRIV(glyphSet);
    DMXScreenInfo   *dmxScreen  = &dmxScreens[scrnNum];
    GlyphRefPtr      table;
    char            *images;
    Glyph           *gids;
    XGlyphInfo      *glyphs;
    char            *pos;
    int              beret;
    int              len_images = 0;
    int              i, size;
    int              ctr;

    if (glyphPriv->glyphSets[scrnNum]) {
	/* Only restore glyphs on the screen we are attaching */
	return;
    }

    /* First we must create the glyph set on the backend. */
    if ((beret = dmxBECreateGlyphSet(scrnNum, glyphSet)) != Success) {
	dmxLog(dmxWarning,
	       "\tdmxBERestoreRenderGlyph failed to create glyphset!\n");
	return;
    }

    /* Now for the complex part, restore the glyph data */
    table = glyphSet->hash.table;

    /* We need to know how much memory to allocate for this part */
    for (i = 0; i < glyphSet->hash.hashSet->size; i++) {
	GlyphRefPtr  gr = &table[i];
	GlyphPtr     gl = gr->glyph;

	if (!gl || gl == DeletedGlyph) continue;

	size = gl->info.height * PixmapBytePad (gl->info.width,
						glyphSet->format->depth);
	if (size & 3)
	    size += 4 - (size & 3);

	len_images += size;
    }

    if (!len_images)
	return;

    /* Now allocate the memory we need */
    images = calloc(len_images, sizeof(char));
    gids   = xalloc(glyphSet->hash.tableEntries*sizeof(Glyph));
    glyphs = xalloc(glyphSet->hash.tableEntries*sizeof(XGlyphInfo));

    pos = images;
    ctr = 0;
    
    /* Fill the allocated memory with the proper data */
    for (i = 0; i < glyphSet->hash.hashSet->size; i++) {
	GlyphRefPtr  gr = &table[i];
	GlyphPtr     gl = gr->glyph;
	char         *data;

	if (!gl || gl == DeletedGlyph) continue;

	/* First lets put the data into gids */
	gids[ctr] = gr->signature;

	/* Next do the glyphs data structures */
	glyphs[ctr].width  = gl->info.width;
	glyphs[ctr].height = gl->info.height;
	glyphs[ctr].x      = gl->info.x;
	glyphs[ctr].y      = gl->info.y;
	glyphs[ctr].xOff   = gl->info.xOff;
	glyphs[ctr].yOff   = gl->info.yOff;

	size = gl->info.height * PixmapBytePad (gl->info.width,
						glyphSet->format->depth);
	if (size & 3)
	    size += 4 - (size & 3);

	data = dixLookupPrivate (&(gl)->devPrivates, dmxGlyphPrivateKey);
	if (data)
	{
	    memcpy (pos, data, size);
	}
	else
	{
	    dmxLog (dmxWarning,
		    "Cannot restore glyph image: %dx%d %d\n",
		    gl->info.width,
		    gl->info.height,
		    glyphSet->format->depth);

	    memset (pos, 0xff, size);
	}

	pos += size;
	ctr++;
    }

    /* Now restore the glyph data */
    XLIB_PROLOGUE (dmxScreen);
    XRenderAddGlyphs(dmxScreen->beDisplay, glyphPriv->glyphSets[scrnNum],
		     gids, glyphs, ctr, images, len_images);
    XLIB_EPILOGUE (dmxScreen);

    /* Clean up */
    xfree(images);
    xfree(gids);
    xfree(glyphs);    
}
#endif

/** Reattach previously detached back-end screen. */
int
dmxAttachScreen (int                    idx,
		 DMXScreenAttributesPtr attr,
		 unsigned int		window,
		 const char             *authType,
		 int                    authTypeLen,
		 const char             *authData,
		 int                    authDataLen,
		 dmxErrorSetProcPtr     errorSet,
		 void			*error,
		 const char             *errorName)
{
    ScreenPtr     pScreen;
    DMXScreenInfo *dmxScreen;
    DMXScreenInfo oldDMXScreen;
    Bool          beShape = FALSE;
    int           errorBase;

    /* Return failure if dynamic addition/removal of screens is disabled */
    if (!dmxAddRemoveScreens) {
	dmxLog(dmxWarning,
	       "Attempting to add a screen, but the AddRemoveScreen\n");
	dmxLog(dmxWarning,
	       "extension has not been enabled.  To enable this extension\n");
	dmxLog(dmxWarning,
	       "add the \"-addremovescreens\" option either to the command\n");
	dmxLog(dmxWarning,
	       "line or in the configuration file.\n");
	dmxErrorSet (errorSet, error, errorName,
		     "Screen attach extension has not been enabled");
	return 1;
    }

    /* Cannot add a screen that does not exist */
    if (idx < 0 || idx >= dmxNumScreens)
    {
	dmxErrorSet (errorSet, error, errorName,
		     "Screen %d does not exist", idx);
	return 1;
    }

    pScreen = screenInfo.screens[idx];
    dmxScreen = &dmxScreens[idx];

    /* Cannot attach to a screen that is already opened */
    if (dmxScreen->name && *dmxScreen->name) {
	dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			"Attempting to attach back-end server to screen #%d "
			"but back-end server is already attached to this "
			"screen", idx);
	return 1;
    }

    dmxLogOutput(dmxScreen, "Attaching screen #%d\n", idx);

    /* Save old info */
    oldDMXScreen = *dmxScreen;

    dmxScreen->scrnWin   = window;
    dmxScreen->virtualFb = FALSE;

    /* Open display and get all of the screen info */
    if (!dmxOpenDisplay(dmxScreen,
			attr->displayName,
			authType,
			authTypeLen,
			authData,
			authDataLen)) {
	dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			"Can't open display: %s",
			attr->displayName);

	/* Restore the old screen */
	*dmxScreen = oldDMXScreen;
	return 1;
    }

    XLIB_PROLOGUE (dmxScreen);
    beShape = XShapeQueryExtension (dmxScreen->beDisplay,
				    &dmxScreen->beShapeEventBase,
				    &errorBase);
    XLIB_EPILOGUE (dmxScreen);

    if (!beShape)
    {
	dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			"SHAPE extension missing on back-end server");
	dmxCloseDisplay (dmxScreen);

	/* Restore the old screen */
	*dmxScreen = oldDMXScreen;
	return 1;
    }

#ifdef COMPOSITE
    if (!noCompositeExtension)
    {
	Bool beComposite = FALSE;
	int  eventBase;

	XLIB_PROLOGUE (dmxScreen);
	beComposite = XCompositeQueryExtension (dmxScreen->beDisplay,
						&eventBase,
						&errorBase);
	XLIB_EPILOGUE (dmxScreen);

	if (!beComposite)
	{
	    dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			    "Composite extension missing on back-end server");
	    dmxCloseDisplay (dmxScreen);

	    /* Restore the old screen */
	    *dmxScreen = oldDMXScreen;
	    return 1;
	}
    }
#endif

    if (!dmxScreen->scrnWin)
	dmxScreen->scrnWin = DefaultRootWindow (dmxScreen->beDisplay);

    XLIB_PROLOGUE (dmxScreen);
    XSelectInput (dmxScreen->beDisplay,
		  dmxScreen->scrnWin,
		  StructureNotifyMask);
    XLIB_EPILOGUE (dmxScreen);

    dmxSetErrorHandler(dmxScreen);
    dmxGetScreenAttribs(dmxScreen);

    if (!dmxGetVisualInfo(dmxScreen)) {
	dmxLogErrorSet (dmxWarning, errorSet, error, errorName,
			"No matching visuals found");
	XFree(dmxScreen->beVisuals);
	dmxCloseDisplay (dmxScreen);

	/* Restore the old screen */
	*dmxScreen = oldDMXScreen;
	return 1;
    }

    dmxGetColormaps(dmxScreen);
    dmxGetPixmapFormats(dmxScreen);

    /* Verify that the screen to be added has the same info as the
     * previously added screen. */
    if (!dmxCompareScreens(dmxScreen,
			   &oldDMXScreen,
			   errorSet,
			   error,
			   errorName))
    {
	dmxLog(dmxWarning,
	       "New screen data (%s) does not match previously\n",
	       dmxScreen->name);
	dmxLog(dmxWarning,
	       "attached screen data\n");
	dmxLog(dmxWarning,
	       "All data must match in order to attach to screen #%d\n",
	       idx);
	XFree(dmxScreen->beVisuals);
	XFree(dmxScreen->beDepths);
	XFree(dmxScreen->bePixmapFormats);
	dmxCloseDisplay (dmxScreen);

	/* Restore the old screen */
	*dmxScreen = oldDMXScreen;
	return 1;
    }

    /* Create the default font */
    if (!dmxBELoadFont(pScreen, defaultFont))
    {
	dmxErrorSet (errorSet, error, errorName,
		     "Failed to load default font");
	XFree(dmxScreen->beVisuals);
	XFree(dmxScreen->beDepths);
	XFree(dmxScreen->bePixmapFormats);
	dmxCloseDisplay (dmxScreen);

	/* Restore the old screen */
	*dmxScreen = oldDMXScreen;
	return 1;
    }

    /* We used these to compare the old and new screens.  They are no
     * longer needed since we have a newly attached screen, so we can
     * now free the old screen's resources. */
    if (oldDMXScreen.beVisuals)
	XFree(oldDMXScreen.beVisuals);
    if (oldDMXScreen.beDepths)
	XFree(oldDMXScreen.beDepths);
    if (oldDMXScreen.bePixmapFormats)
	XFree(oldDMXScreen.bePixmapFormats);

    if (attr->name)
	dmxScreen->name = strdup(attr->name);
    else
	dmxScreen->name = strdup(attr->displayName);

    dmxScreen->beAttachedDisplay = dmxScreen->beDisplay;
    dmxScreen->beDisplay = NULL;

    return 0; /* Success */
}

void
dmxEnableScreen (int idx)
{
    ScreenPtr     pScreen;
    DMXScreenInfo *dmxScreen;
    int           i;

    pScreen = screenInfo.screens[idx];
    dmxScreen = &dmxScreens[idx];

    dmxLogOutput(dmxScreen, "Enable screen #%d\n", idx);

    dmxScreen->beDisplay = dmxScreen->beAttachedDisplay;

    /* Initialize the BE screen resources */
    dmxBEScreenInit(screenInfo.screens[idx]);

    /* TODO: Handle GLX visual initialization.  GLXProxy needs to be
     * updated to handle dynamic addition/removal of screens. */

    /* Create default stipple */
    dmxBECreatePixmap(pScreen->PixmapPerDepth[0]);
    dmxBERestorePixmap(pScreen->PixmapPerDepth[0]);

    /* Create the scratch GCs */
    dmxBECreateScratchGCs(idx);

    /* Create the scratch pixmap */
    if (pScreen->pScratchPixmap)
	dmxBECreatePixmap(pScreen->pScratchPixmap);

    /* Create all resources that don't depend on windows */
    for (i = currentMaxClients; --i >= 0; )
	if (clients[i])
	    FindAllClientResources(clients[i], dmxBECreateResources,
				   (pointer)idx);

    /* Create window hierarchy (top down) */
    dmxBECreateWindowTree(idx);
    dmxBECreateWindowProperties(idx);

#ifdef RENDER
    /* Restore the picture state for RENDER */
    for (i = currentMaxClients; --i >= 0; )
	if (clients[i])
	    FindClientResourcesByType(clients[i],PictureType, 
				      dmxBERestoreRenderPict,(pointer)idx);

    /* Restore the glyph state for RENDER */
    for (i = currentMaxClients; --i >= 0; )
	if (clients[i])
	    FindClientResourcesByType(clients[i],GlyphSetType, 
				      dmxBERestoreRenderGlyph,(pointer)idx);
#endif

    dmxBEMapRootWindow(idx);

#ifdef RANDR
    RRGetInfo (screenInfo.screens[0]);
#endif

    dmxInputEnable (&dmxScreen->input);
}

/*
 * Resources that may have state on the BE server and need to be freed:
 *
 * RT_NONE
 * RT_WINDOW
 * RT_PIXMAP
 * RT_GC
 * RT_FONT
 * RT_CURSOR
 * RT_COLORMAP
 * RT_CMAPENTRY
 * RT_OTHERCLIENT
 * RT_PASSIVEGRAB
 * XRT_WINDOW
 * XRT_PIXMAP
 * XRT_GC
 * XRT_COLORMAP
 * XRT_PICTURE
 * PictureType
 * PictFormatType
 * GlyphSetType
 * ClientType
 * EventType
 * RT_INPUTCLIENT
 * XETrapType
 * RTCounter
 * RTAwait
 * RTAlarmClient
 * RT_XKBCLIENT
 * RTContext
 * TagResType
 * StalledResType
 * SecurityAuthorizationResType
 * RTEventClient
 * __glXContextRes
 * __glXClientRes
 * __glXPixmapRes
 * __glXWindowRes
 * __glXPbufferRes
 */

#ifdef PANORAMIX
/** Search the Xinerama XRT_PIXMAP resources for the pixmap that needs
 *  to have its image saved. */
static void dmxBEFindPixmapImage(pointer value, XID id, RESTYPE type,
				 pointer p)
{
    if ((type & TypeMask) == (XRT_PIXMAP & TypeMask)) {
	PixmapPtr      pDst     = (PixmapPtr)p;
	int            idx      = pDst->drawable.pScreen->myNum;
	PanoramiXRes  *pXinPix  = (PanoramiXRes *)value;
	PixmapPtr      pPix;
	int            i;

	pPix = (PixmapPtr)LookupIDByType(pXinPix->info[idx].id, RT_PIXMAP);

	if (pPix != pDst) return; /* Not a match.... Next! */

	for (i = 0; i < PanoramiXNumScreens; i++) {
	    PixmapPtr      pSrc;
	    dmxPixPrivPtr  pSrcPriv = NULL;

	    if (i == idx) continue; /* Self replication is bad */

	    pSrc =
		(PixmapPtr)LookupIDByType(pXinPix->info[i].id, RT_PIXMAP);

	    pSrcPriv = DMX_GET_PIXMAP_PRIV(pSrc);
	    if (pSrcPriv->pixmap) {
		FoundPixImage = True;
		return;
	    }
	}
    }

#ifdef RENDER
    else if ((type & TypeMask) == (XRT_PICTURE & TypeMask))
    {
	PixmapPtr     pDst     = (PixmapPtr) p;
	int           idx      = pDst->drawable.pScreen->myNum;
	PanoramiXRes  *pXinPic  = (PanoramiXRes *) value;
	PicturePtr    pPic;
	int           i;

	pPic = (PicturePtr) LookupIDByType (pXinPic->info[idx].id,
					    PictureType);

	/* Not a match.... Next! */
	if (!pPic || !pPic->pDrawable) return;
	if (pPic->pDrawable->type != DRAWABLE_PIXMAP) return;
	if (pPic->pDrawable != (DrawablePtr) pDst) return;

	for (i = 0; i < PanoramiXNumScreens; i++) {
	    dmxPixPrivPtr  pSrcPriv = NULL;

	    if (i == idx) continue; /* Self replication is bad */

	    pPic = (PicturePtr) LookupIDByType (pXinPic->info[i].id,
						PictureType);

	    pSrcPriv = DMX_GET_PIXMAP_PRIV ((PixmapPtr) pPic->pDrawable);
	    if (pSrcPriv->pixmap)
	    {
		FoundPixImage = True;
		return;
	    }
	}
    }
#endif

    else if ((type & TypeMask) == (XRT_WINDOW & TypeMask))
    {
	PixmapPtr      pDst     = (PixmapPtr)p;
	int            idx      = pDst->drawable.pScreen->myNum;
	PanoramiXRes  *pXinWin  = (PanoramiXRes *) value;
	Bool	      border;
	WindowPtr     pWin;
	int           i;

	pWin = (WindowPtr) LookupIDByType (pXinWin->info[idx].id, RT_WINDOW);

	if (!pWin) return;
	if (!pWin->borderIsPixel && pWin->border.pixmap == pDst)
	{
	    border = TRUE;
	}
	else if (pWin->backgroundState == BackgroundPixmap &&
		 pWin->background.pixmap == pDst)
	{
	    border = FALSE;
	}
	else
	{
	    return;
	}

	for (i = 0; i < PanoramiXNumScreens; i++) {
	    dmxPixPrivPtr pSrcPriv = NULL;

	    if (i == idx) continue; /* Self replication is bad */

	    pWin = (WindowPtr) LookupIDByType (pXinWin->info[i].id, RT_WINDOW);

	    if (border)
		pSrcPriv = DMX_GET_PIXMAP_PRIV (pWin->border.pixmap);
	    else
		pSrcPriv = DMX_GET_PIXMAP_PRIV (pWin->background.pixmap);

	    if (pSrcPriv->pixmap)
	    {
		FoundPixImage = True;
		return;
	    }
	}
    }
}
#endif

/** Save the pixmap image only when there is not another screen with
 *  that pixmap from which the image can be read when the screen is
 *  reattached.  To do this, we first try to find a pixmap on another
 *  screen corresponding to the one we are trying to save.  If we find
 *  one, then we do not need to save the image data since during
 *  reattachment, the image data can be read from that other pixmap.
 *  However, if we do not find one, then we need to save the image data.
 *  The common case for these are for the default stipple and root
 *  tile. */
static void dmxBESavePixmap(PixmapPtr pPixmap)
{
#ifdef PANORAMIX
    int i;

    /* If Xinerama is not active, there's nothing we can do (see comment
     * in #else below for more info). */
    if (noPanoramiXExtension) return;

    FoundPixImage = False;
    for (i = currentMaxClients; --i >= 0; )
	if (clients[i])
	    FindAllClientResources(clients[i], dmxBEFindPixmapImage,
				   (pointer)pPixmap);

    /* Save the image only if there is no other screens that have a
     * pixmap that corresponds to the one we are trying to save. */
    if (!FoundPixImage) {
	dmxPixPrivPtr  pPixPriv = DMX_GET_PIXMAP_PRIV(pPixmap);

	if (!pPixPriv->detachedImage) {
	    ScreenPtr      pScreen   = pPixmap->drawable.pScreen;
	    DMXScreenInfo *dmxScreen = &dmxScreens[pScreen->myNum];

	    XLIB_PROLOGUE (dmxScreen);
	    pPixPriv->detachedImage = XGetImage(dmxScreen->beDisplay,
						pPixPriv->pixmap,
						0, 0,
						pPixmap->drawable.width,
						pPixmap->drawable.height,
						-1,
						ZPixmap);
	    XLIB_EPILOGUE (dmxScreen);

	    if (!pPixPriv->detachedImage)
	    {
		dmxLog(dmxWarning, "Cannot save pixmap image: %p - %dx%d %d\n",
		       pPixmap, pPixmap->drawable.width,
		       pPixmap->drawable.height,
		       pPixmap->drawable.depth);
	    }
	}
    }
#else
    /* NOTE: The only time there is a pixmap on another screen that
     * corresponds to the one we are trying to save is when Xinerama is
     * active.  Otherwise, the pixmap image data is only stored on a
     * single screen, which means that once it is detached, that data is
     * lost.  We could save the data here, but then that would require
     * us to implement the ability for Xdmx to keep the pixmap up to
     * date while the screen is detached, which is beyond the scope of
     * the current project. */
    return;
#endif
}

/** Destroy resources on the back-end server.  This function is called
 *  from #dmxDetachScreen() via the dix layer's FindAllResources
 *  function.  It walks all resources, compares them to the screen
 *  number passed in as \a n and calls the appropriate DMX function to
 *  free the associated resource on the back-end server. */
static void dmxBEDestroyResources(pointer value, XID id, RESTYPE type,
				  pointer n)
{
    int        scrnNum = (int)n;
    ScreenPtr  pScreen = screenInfo.screens[scrnNum];

    if ((type & TypeMask) == (RT_WINDOW & TypeMask)) {
	/* Window resources are destroyed below in dmxBEDestroyWindowTree */
    } else if ((type & TypeMask) == (RT_PIXMAP & TypeMask)) {
	PixmapPtr  pPix = value;
	if (pPix->drawable.pScreen->myNum == scrnNum) {
	    dmxBESavePixmap(pPix);
	    dmxBEFreePixmap(pPix);
	}
    } else if ((type & TypeMask) == (RT_GC & TypeMask)) {
	GCPtr  pGC = value;
	if (pGC->pScreen->myNum == scrnNum)
	    dmxBEFreeGC(pGC);
    } else if ((type & TypeMask) == (RT_FONT & TypeMask)) {
	dmxBEFreeFont(pScreen, (FontPtr)value);
    } else if ((type & TypeMask) == (RT_CURSOR & TypeMask)) {
	dmxBEFreeCursor (pScreen, (CursorPtr) value);
    } else if ((type & TypeMask) == (RT_COLORMAP & TypeMask)) {
	ColormapPtr  pCmap = value;
	if (pCmap->pScreen->myNum == scrnNum)
	    dmxBEFreeColormap((ColormapPtr)value);
    } else if ((type & TypeMask) == (DMX_SHMSEG & TypeMask)) {
	dmxBEDetachShmSeg (&dmxScreens[scrnNum], (dmxShmSegInfoPtr) value);
#ifdef RENDER
    } else if ((type & TypeMask) == (PictureType & TypeMask)) {
	PicturePtr  pPict = value;
	if (pPict->pDrawable) {
	    if (pPict->pDrawable->pScreen->myNum == scrnNum) {
		/* Free the pixmaps on the backend if needed */
		if (pPict->pDrawable->type == DRAWABLE_PIXMAP) {
		    PixmapPtr pPixmap = (PixmapPtr)(pPict->pDrawable);
		    dmxBESavePixmap(pPixmap);
		    dmxBEFreePixmap(pPixmap);
		}

		dmxBEFreePicture(pScreen, (PicturePtr)value);
	    }
	}
	else
	{
	    dmxBEFreePicture(pScreen, (PicturePtr)value);
	}
    } else if ((type & TypeMask) == (GlyphSetType & TypeMask)) {
	dmxBEFreeGlyphSet(pScreen, (GlyphSetPtr)value);
#endif
    } else {
	/* Other resource types??? */
    }
}

/** Destroy the scratch GCs that are created per depth. */
static void dmxBEDestroyScratchGCs(int scrnNum)
{
    ScreenPtr  pScreen = screenInfo.screens[scrnNum];
    GCPtr     *ppGC    = pScreen->GCperDepth;
    int        i;

    for (i = 0; i <= pScreen->numDepths; i++)
	dmxBEFreeGC(ppGC[i]);
}

/** Destroy window hierachy on back-end server.  To ensure that all
 *  XDestroyWindow() calls succeed, they must be performed in a bottom
 *  up order so that windows are not destroyed before their children.
 *  XDestroyWindow(), which is called from #dmxBEDestrowWindow(), will
 *  destroy a window as well as all of it's children. */
static void dmxBEDestroyWindowTree(int idx)
{
    WindowPtr  pWin   = WindowTable[idx];
    WindowPtr  pChild = pWin;

    while (1) {
	if (pChild->firstChild) {
	    pChild = pChild->firstChild;
	    continue;
	}

	/* Destroy the window */
	dmxBEDestroyWindow(pChild);

	if (wUseDefault(pChild, cursor, 0))
	    dmxBEFreeCursor (screenInfo.screens[idx],
			     pChild->optional->cursor);

	/* Make sure we destroy the window's border and background
	 * pixmaps if they exist */
	if (!pChild->borderIsPixel) {
	    dmxBESavePixmap(pChild->border.pixmap);
	    dmxBEFreePixmap(pChild->border.pixmap);
	}
	if (pChild->backgroundState == BackgroundPixmap) {
	    dmxBESavePixmap(pChild->background.pixmap);
	    dmxBEFreePixmap(pChild->background.pixmap);
	}

	while (!pChild->nextSib && (pChild != pWin)) {
	    pChild = pChild->parent;
	    dmxBEDestroyWindow(pChild);
	    if (wUseDefault(pChild, cursor, 0))
		dmxBEFreeCursor (screenInfo.screens[idx],
				 pChild->optional->cursor);
	    if (!pChild->borderIsPixel) {
		dmxBESavePixmap(pChild->border.pixmap);
		dmxBEFreePixmap(pChild->border.pixmap);
	    }
	    if (pChild->backgroundState == BackgroundPixmap) {
		dmxBESavePixmap(pChild->background.pixmap);
		dmxBEFreePixmap(pChild->background.pixmap);
	    }
	}

	if (pChild == pWin)
	    break;

	pChild = pChild->nextSib;
    }
}

void
dmxDisableScreen (int idx)
{
    ScreenPtr     pScreen;
    DMXScreenInfo *dmxScreen;
    int           i;

    pScreen = screenInfo.screens[idx];
    dmxScreen = &dmxScreens[idx];

    dmxLogOutput(dmxScreen, "Disable screen #%d\n", idx);

    dmxInputDisable (&dmxScreen->input);

#ifdef XV
    dmxBEXvScreenFini (pScreen);
#endif

#ifdef RANDR
    dmxBERRScreenFini (pScreen);
#endif

    /* Save all relevant state (TODO) */

    /* Free all non-window resources related to this screen */
    for (i = currentMaxClients; --i >= 0; )
	if (clients[i])
	    FindAllClientResources(clients[i], dmxBEDestroyResources,
				   (pointer)idx);

    /* Free scratch GCs */
    dmxBEDestroyScratchGCs(idx);

    /* Free window resources related to this screen */
    dmxBEDestroyWindowTree(idx);

    /* Free default stipple */
    dmxBESavePixmap(pScreen->PixmapPerDepth[0]);
    dmxBEFreePixmap(pScreen->PixmapPerDepth[0]);

    if (pScreen->pScratchPixmap)
	dmxBEFreePixmap(pScreen->pScratchPixmap);

    /* Free the remaining screen resources and close the screen */
    dmxBECloseScreen(pScreen);

    /* Screen is now disabled */
    dmxScreen->beDisplay = NULL;

    /* Make sure we don't have any pending sync replies */
    dmxScreenSyncReply (pScreen, 0, NULL, NULL, 0);

#ifdef RANDR
    RRGetInfo (screenInfo.screens[0]);
#endif

}

/** Detach back-end screen. */
int dmxDetachScreen(int idx)
{
    DMXScreenInfo *dmxScreen;

    /* Return failure if dynamic addition/removal of screens is disabled */
    if (!dmxAddRemoveScreens) {
	dmxLog(dmxWarning,
	       "Attempting to remove a screen, but the AddRemoveScreen\n");
	dmxLog(dmxWarning,
	       "extension has not been enabled.  To enable this extension\n");
	dmxLog(dmxWarning,
	       "add the \"-addremovescreens\" option either to the command\n");
	dmxLog(dmxWarning,
	       "line or in the configuration file.\n");
	return 1;
    }

    /* Cannot remove a screen that does not exist */
    if (idx < 0 || idx >= dmxNumScreens) return 1;

    dmxScreen = &dmxScreens[idx];

    if (idx == 0) {
	dmxLog(dmxWarning,
	       "Attempting to remove screen #%d\n",
	       idx);
	return 1;
    }

    /* Cannot detach from a screen that is not opened */
    if (!dmxScreen->beAttachedDisplay && !dmxScreen->beDisplay) {
	dmxLog(dmxWarning,
	       "Attempting to remove screen #%d but it has not been opened\n",
	       idx);
	return 1;
    }

    dmxLogOutput(dmxScreen, "Detaching screen #%d\n", idx);

    if (dmxScreen->beDisplay)
	dmxDisableScreen (idx);

    /* Detach input */
    dmxInputDetach (&dmxScreen->input);

    dmxScreen->beDisplay = dmxScreen->beAttachedDisplay;
    dmxCloseDisplay (dmxScreen);

    dmxScreen->beAttachedDisplay = NULL;
    dmxScreen->beDisplay = NULL;

    dmxScreen->scrnWidth   = 1;
    dmxScreen->scrnHeight  = 1;
    dmxScreen->rootX       = 0;
    dmxScreen->rootY       = 0;
    dmxScreen->beWidth     = 1;
    dmxScreen->beHeight    = 1;
    dmxScreen->beXDPI      = 75;
    dmxScreen->beYDPI      = 75;
    dmxScreen->beDepth     = 24;
    dmxScreen->beBPP       = 32;

    if (dmxScreen->name)
    {
	free (dmxScreen->name);
	dmxScreen->name = NULL;
    }

    if (dmxScreen->display)
    {
	free (dmxScreen->display);
	dmxScreen->display = NULL;
    }

    if (dmxScreen->authType)
    {
	free (dmxScreen->authType);
	dmxScreen->authType = NULL;
    }

    dmxScreen->authTypeLen = 0;

    if (dmxScreen->authData)
    {
	free (dmxScreen->authData);
	dmxScreen->authData = NULL;
    }

    dmxScreen->authDataLen = 0;

#ifdef RANDR
    RRGetInfo (screenInfo.screens[0]);
#endif

    return 0; /* Success */
}
