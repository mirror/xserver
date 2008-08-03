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
 * This file provides support for GC operations. */

#ifdef HAVE_DMX_CONFIG_H
#include <dmx-config.h>
#endif

#include "dmx.h"
#include "dmxsync.h"
#include "dmxgc.h"
#include "dmxgcops.h"
#include "dmxwindow.h"
#include "dmxpixmap.h"
#include "dmxlog.h"

#include "mi.h"
#include "gcstruct.h"
#include "pixmapstr.h"
#include "dixfontstr.h"

#include "panoramiXsrv.h"

#include <xcb/xcb_image.h>

/* hm, conflicting definition of xcb_popcount in xcbext.h and xcb_bitops.h */
#define xcb_popcount xcb_popcount2
#include <xcb/xcb_bitops.h>
#undef xcb_popcount

#define DMX_GCOPS_SET_DRAWABLE(_pDraw, _draw)				\
do {									\
    if ((_pDraw)->type == DRAWABLE_WINDOW) {				\
	dmxWinPrivPtr  pWinPriv =					\
	    DMX_GET_WINDOW_PRIV((WindowPtr)(_pDraw));			\
	(_draw) = (Drawable)pWinPriv->window;				\
    } else {								\
	dmxPixPrivPtr  pPixPriv =					\
	    DMX_GET_PIXMAP_PRIV((PixmapPtr)(_pDraw));			\
	(_draw) = (Drawable)pPixPriv->pixmap;				\
    }									\
} while (0)

#define DMX_GCOPS_OFFSCREEN(_pDraw)					\
    (!dmxScreens[(_pDraw)->pScreen->myNum].beDisplay ||			\
     (((_pDraw)->type == DRAWABLE_WINDOW) ?				\
      ((!DMX_GET_WINDOW_PRIV((WindowPtr)(_pDraw))->window ||		\
	(dmxOffScreenOpt &&						\
	 DMX_GET_WINDOW_PRIV((WindowPtr)(_pDraw))->offscreen))) :	\
      (!(DMX_GET_PIXMAP_PRIV((PixmapPtr)(_pDraw)))->pixmap)))

/** Transfer \a pBits image to back-end server associated with \a
 *  pDrawable's screen.  If primitive subdivision optimization is
 *  enabled, then only transfer the sections of \a pBits that are
 *  visible (i.e., not-clipped) to the back-end server. */
void dmxPutImage(DrawablePtr pDrawable, GCPtr pGC,
		 int depth, int x, int y, int w, int h,
		 int leftPad, int format, char *pBits)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    XImage        *img = NULL;

    if (DMX_GCOPS_OFFSCREEN(pDrawable)) return;

    XLIB_PROLOGUE (dmxScreen);
    img = XCreateImage(dmxScreen->beDisplay,
		       dmxScreen->beVisuals[dmxScreen->beDefVisualIndex].visual,
		       depth, format, leftPad, pBits, w, h,
		       BitmapPad(dmxScreen->beDisplay),
		       (format == ZPixmap) ?
		       PixmapBytePad(w, depth) : BitmapBytePad(w+leftPad));
    XLIB_EPILOGUE (dmxScreen);

    if (img) {
	Drawable draw;

	DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);

	if (dmxSubdividePrimitives && pGC->pCompositeClip) {
	    RegionPtr  pSubImages;
	    RegionPtr  pClip;
	    BoxRec     box;
	    BoxPtr     pBox;
	    int        nBox;

	    box.x1 = x;
	    box.y1 = y;
	    box.x2 = x + w;
	    box.y2 = y + h;
	    pSubImages = REGION_CREATE(pGC->pScreen, &box, 1);

	    pClip = REGION_CREATE(pGC->pScreen, NullBox, 1);
	    REGION_COPY(pGC->pScreen, pClip, pGC->pCompositeClip);
	    REGION_TRANSLATE(pGC->pScreen, pClip,
			     -pDrawable->x, -pDrawable->y);
	    REGION_INTERSECT(pGC->pScreen, pSubImages, pSubImages, pClip);

	    nBox = REGION_NUM_RECTS(pSubImages);
	    pBox = REGION_RECTS(pSubImages);

	    XLIB_PROLOGUE (dmxScreen);
	    while (nBox--) {
		XPutImage(dmxScreen->beDisplay, draw, pGCPriv->gc, img,
			  pBox->x1 - box.x1,
			  pBox->y1 - box.y1,
			  pBox->x1,
			  pBox->y1,
			  pBox->x2 - pBox->x1,
			  pBox->y2 - pBox->y1);
		pBox++;
	    }
	    XLIB_EPILOGUE (dmxScreen);
            REGION_DESTROY(pGC->pScreen, pClip);
            REGION_DESTROY(pGC->pScreen, pSubImages);
	} else {
	    XLIB_PROLOGUE (dmxScreen);
	    XPutImage(dmxScreen->beDisplay, draw, pGCPriv->gc,
		      img, 0, 0, x, y, w, h);
	    XLIB_EPILOGUE (dmxScreen);
	}
	XFree(img);             /* Use XFree instead of XDestroyImage
                                 * because pBits is passed in from the
                                 * caller. */

	dmxSync(dmxScreen, FALSE);
    } else {
	/* Error -- this should not happen! */
	FatalError ("XCreateImage failed\n");
    }
}

/** Copy area from \a pSrc drawable to \a pDst drawable on the back-end
 *  server associated with \a pSrc drawable's screen.  If the offscreen
 *  optimization is enabled, only copy when both \a pSrc and \a pDst are
 *  at least partially visible. */
RegionPtr dmxCopyArea(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC,
		      int srcx, int srcy, int w, int h, int dstx, int dsty)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pSrc->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    Drawable       srcDraw, dstDraw;

    if (DMX_GCOPS_OFFSCREEN(pSrc) || DMX_GCOPS_OFFSCREEN(pDst))
	return miHandleExposures(pSrc, pDst, pGC, srcx, srcy, w, h,
				 dstx, dsty, 0L);

    DMX_GCOPS_SET_DRAWABLE(pSrc, srcDraw);
    DMX_GCOPS_SET_DRAWABLE(pDst, dstDraw);

    XLIB_PROLOGUE (dmxScreen);
    XCopyArea(dmxScreen->beDisplay, srcDraw, dstDraw, pGCPriv->gc,
	      srcx, srcy, w, h, dstx, dsty);
    XLIB_EPILOGUE (dmxScreen);
    dmxSync(dmxScreen, FALSE);

    return miHandleExposures(pSrc, pDst, pGC, srcx, srcy, w, h,
			     dstx, dsty, 0L);
}

/** Copy plane number \a bitPlane from \a pSrc drawable to \a pDst
 *  drawable on the back-end server associated with \a pSrc drawable's
 *  screen.  If the offscreen optimization is enabled, only copy when
 *  both \a pSrc and \a pDst are at least partially visible. */
RegionPtr dmxCopyPlane(DrawablePtr pSrc, DrawablePtr pDst, GCPtr pGC,
		       int srcx, int srcy, int width, int height,
		       int dstx, int dsty, unsigned long bitPlane)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pSrc->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    Drawable       srcDraw, dstDraw;

    if (DMX_GCOPS_OFFSCREEN(pSrc) || DMX_GCOPS_OFFSCREEN(pDst))
	return miHandleExposures(pSrc, pDst, pGC, srcx, srcy, width, height,
				 dstx, dsty, bitPlane);

    DMX_GCOPS_SET_DRAWABLE(pSrc, srcDraw);
    DMX_GCOPS_SET_DRAWABLE(pDst, dstDraw);

    XLIB_PROLOGUE (dmxScreen);
    XCopyPlane(dmxScreen->beDisplay, srcDraw, dstDraw, pGCPriv->gc,
	       srcx, srcy, width, height, dstx, dsty, bitPlane);
    XLIB_EPILOGUE (dmxScreen);
    dmxSync(dmxScreen, FALSE);

    return miHandleExposures(pSrc, pDst, pGC, srcx, srcy, width, height,
			     dstx, dsty, bitPlane);
}

/** Render list of points, \a pptInit in \a pDrawable on the back-end
 *  server associated with \a pDrawable's screen.  If the offscreen
 *  optimization is enabled, only draw when \a pDrawable is at least
 *  partially visible. */
void dmxPolyPoint(DrawablePtr pDrawable, GCPtr pGC,
		  int mode, int npt, DDXPointPtr pptInit)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    Drawable       draw;

    if (DMX_GCOPS_OFFSCREEN(pDrawable)) return;

    DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);

    XLIB_PROLOGUE (dmxScreen);
    XDrawPoints(dmxScreen->beDisplay, draw, pGCPriv->gc,
		(XPoint *)pptInit, npt, mode);
    XLIB_EPILOGUE (dmxScreen);
    dmxSync(dmxScreen, FALSE);
}

/** Render list of connected lines, \a pptInit in \a pDrawable on the
 *  back-end server associated with \a pDrawable's screen.  If the
 *  offscreen optimization is enabled, only draw when \a pDrawable is at
 *  least partially visible. */
void dmxPolylines(DrawablePtr pDrawable, GCPtr pGC,
		  int mode, int npt, DDXPointPtr pptInit)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    Drawable       draw;

    if (DMX_GCOPS_OFFSCREEN(pDrawable)) return;

    DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);

    XLIB_PROLOGUE (dmxScreen);
    XDrawLines(dmxScreen->beDisplay, draw, pGCPriv->gc,
	       (XPoint *)pptInit, npt, mode);
    XLIB_EPILOGUE (dmxScreen);
    dmxSync(dmxScreen, FALSE);
}

/** Render list of disjoint segments, \a pSegs in \a pDrawable on the
 *  back-end server associated with \a pDrawable's screen.  If the
 *  offscreen optimization is enabled, only draw when \a pDrawable is at
 *  least partially visible. */
void dmxPolySegment(DrawablePtr pDrawable, GCPtr pGC,
		    int nseg, xSegment *pSegs)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    Drawable       draw;

    if (DMX_GCOPS_OFFSCREEN(pDrawable)) return;

    DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);

    XLIB_PROLOGUE (dmxScreen);
    XDrawSegments(dmxScreen->beDisplay, draw, pGCPriv->gc,
		  (XSegment *)pSegs, nseg);
    XLIB_EPILOGUE (dmxScreen);
    dmxSync(dmxScreen, FALSE);
}

/** Render list of rectangle outlines, \a pRects in \a pDrawable on the
 *  back-end server associated with \a pDrawable's screen.  If the
 *  offscreen optimization is enabled, only draw when \a pDrawable is at
 *  least partially visible. */
void dmxPolyRectangle(DrawablePtr pDrawable, GCPtr pGC,
		      int nrects, xRectangle *pRects)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    Drawable       draw;

    if (DMX_GCOPS_OFFSCREEN(pDrawable)) return;

    DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);

    XLIB_PROLOGUE (dmxScreen);
    XDrawRectangles(dmxScreen->beDisplay, draw, pGCPriv->gc,
		    (XRectangle *)pRects, nrects);
    XLIB_EPILOGUE (dmxScreen);

    dmxSync(dmxScreen, FALSE);
}

/** Render list of arc outlines, \a parcs in \a pDrawable on the
 *  back-end server associated with \a pDrawable's screen.  If the
 *  offscreen optimization is enabled, only draw when \a pDrawable is at
 *  least partially visible. */
void dmxPolyArc(DrawablePtr pDrawable, GCPtr pGC,
		int narcs, xArc *parcs)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    Drawable       draw;

    if (DMX_GCOPS_OFFSCREEN(pDrawable)) return;

    DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);

    XLIB_PROLOGUE (dmxScreen);
    XDrawArcs(dmxScreen->beDisplay, draw, pGCPriv->gc,
	      (XArc *)parcs, narcs);
    XLIB_EPILOGUE (dmxScreen);
    dmxSync(dmxScreen, FALSE);
}

/** Render a filled polygons in \a pDrawable on the back-end server
 *  associated with \a pDrawable's screen.  If the offscreen
 *  optimization is enabled, only draw when \a pDrawable is at least
 *  partially visible. */
void dmxFillPolygon(DrawablePtr pDrawable, GCPtr pGC,
		    int shape, int mode, int count, DDXPointPtr pPts)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    Drawable       draw;

    if (DMX_GCOPS_OFFSCREEN(pDrawable)) return;

    DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);

    XLIB_PROLOGUE (dmxScreen);
    XFillPolygon(dmxScreen->beDisplay, draw, pGCPriv->gc,
		 (XPoint *)pPts, count, shape, mode);
    XLIB_EPILOGUE (dmxScreen);
    dmxSync(dmxScreen, FALSE);
}

/** Render list of filled rectangles, \a prectInit in \a pDrawable on
 *  the back-end server associated with \a pDrawable's screen.  If the
 *  offscreen optimization is enabled, only draw when \a pDrawable is at
 *  least partially visible. */
void dmxPolyFillRect(DrawablePtr pDrawable, GCPtr pGC,
		     int nrectFill, xRectangle *prectInit)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    Drawable       draw;

    if (DMX_GCOPS_OFFSCREEN(pDrawable)) return;

    DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);

    XLIB_PROLOGUE (dmxScreen);
    XFillRectangles(dmxScreen->beDisplay, draw, pGCPriv->gc,
		    (XRectangle *)prectInit, nrectFill);
    XLIB_EPILOGUE (dmxScreen);
    dmxSync(dmxScreen, FALSE);
}

/** Render list of filled arcs, \a parcs in \a pDrawable on the back-end
 *  server associated with \a pDrawable's screen.  If the offscreen
 *  optimization is enabled, only draw when \a pDrawable is at least
 *  partially visible. */
void dmxPolyFillArc(DrawablePtr pDrawable, GCPtr pGC,
		    int narcs, xArc *parcs)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    Drawable       draw;

    if (DMX_GCOPS_OFFSCREEN(pDrawable)) return;

    DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);

    XLIB_PROLOGUE (dmxScreen);
    XFillArcs(dmxScreen->beDisplay, draw, pGCPriv->gc,
	      (XArc *)parcs, narcs);
    XLIB_EPILOGUE (dmxScreen);
    dmxSync(dmxScreen, FALSE);
}

/** Render string of 8-bit \a chars (foreground only) in \a pDrawable on
 *  the back-end server associated with \a pDrawable's screen.  If the
 *  offscreen optimization is enabled, only draw when \a pDrawable is at
 *  least partially visible. */
int dmxPolyText8(DrawablePtr pDrawable, GCPtr pGC,
		 int x, int y, int count, char *chars)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    unsigned long  n, i;
    int            w;
    CharInfoPtr    charinfo[255];
    Drawable       draw;

    GetGlyphs(pGC->font, (unsigned long)count, (unsigned char *)chars,
	      Linear8Bit, &n, charinfo);

    /* Calculate text width */
    w = 0;
    for (i = 0; i < n; i++) w += charinfo[i]->metrics.characterWidth;

    if (n != 0 && !DMX_GCOPS_OFFSCREEN(pDrawable)) {
	DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);

	XLIB_PROLOGUE (dmxScreen);
	XDrawString(dmxScreen->beDisplay, draw, pGCPriv->gc,
		    x, y, chars, count);
	XLIB_EPILOGUE (dmxScreen);
	dmxSync(dmxScreen, FALSE);
    }

    return x+w;
}

/** Render string of 16-bit \a chars (foreground only) in \a pDrawable
 *  on the back-end server associated with \a pDrawable's screen.  If
 *  the offscreen optimization is enabled, only draw when \a pDrawable
 *  is at least partially visible. */
int dmxPolyText16(DrawablePtr pDrawable, GCPtr pGC,
		  int x, int y, int count, unsigned short *chars)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    unsigned long  n, i;
    int            w;
    CharInfoPtr    charinfo[255];
    Drawable       draw;

    GetGlyphs(pGC->font, (unsigned long)count, (unsigned char *)chars,
	      (FONTLASTROW(pGC->font) == 0) ? Linear16Bit : TwoD16Bit,
	      &n, charinfo);

    /* Calculate text width */
    w = 0;
    for (i = 0; i < n; i++) w += charinfo[i]->metrics.characterWidth;

    if (n != 0 && !DMX_GCOPS_OFFSCREEN(pDrawable)) {
	DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);

	XLIB_PROLOGUE (dmxScreen);
	XDrawString16(dmxScreen->beDisplay, draw, pGCPriv->gc,
		      x, y, (XChar2b *)chars, count);
	XLIB_EPILOGUE (dmxScreen);
	dmxSync(dmxScreen, FALSE);
    }

    return x+w;
}

/** Render string of 8-bit \a chars (both foreground and background) in
 *  \a pDrawable on the back-end server associated with \a pDrawable's
 *  screen.  If the offscreen optimization is enabled, only draw when \a
 *  pDrawable is at least partially visible. */
void dmxImageText8(DrawablePtr pDrawable, GCPtr pGC,
		   int x, int y, int count, char *chars)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    Drawable       draw;

    if (DMX_GCOPS_OFFSCREEN(pDrawable)) return;

    DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);

    XLIB_PROLOGUE (dmxScreen);
    XDrawImageString(dmxScreen->beDisplay, draw, pGCPriv->gc,
		     x, y, chars, count);
    XLIB_EPILOGUE (dmxScreen);
    dmxSync(dmxScreen, FALSE);
}

/** Render string of 16-bit \a chars (both foreground and background) in
 *  \a pDrawable on the back-end server associated with \a pDrawable's
 *  screen.  If the offscreen optimization is enabled, only draw when \a
 *  pDrawable is at least partially visible. */
void dmxImageText16(DrawablePtr pDrawable, GCPtr pGC,
		    int x, int y, int count, unsigned short *chars)
{
    DMXScreenInfo *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    dmxGCPrivPtr   pGCPriv = DMX_GET_GC_PRIV(pGC);
    Drawable       draw;

    if (DMX_GCOPS_OFFSCREEN(pDrawable)) return;

    DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);

    XLIB_PROLOGUE (dmxScreen);
    XDrawImageString16(dmxScreen->beDisplay, draw, pGCPriv->gc,
		       x, y, (XChar2b *)chars, count);
    XLIB_EPILOGUE (dmxScreen);
    dmxSync(dmxScreen, FALSE);
}

void dmxPushPixels(GCPtr pGC, PixmapPtr pBitmap, DrawablePtr pDst,
		   int width, int height, int x, int y)
{
    /* only works for solid bitmaps */
    if (pGC->fillStyle == FillSolid)
    {
	DMXScreenInfo *dmxScreen = &dmxScreens[pDst->pScreen->myNum];
	dmxGCPrivPtr  pGCPriv = DMX_GET_GC_PRIV(pGC);
	dmxPixPrivPtr pPixPriv = DMX_GET_PIXMAP_PRIV(pBitmap);
	Drawable      draw;

	if (DMX_GCOPS_OFFSCREEN(pDst)) return;

	DMX_GCOPS_SET_DRAWABLE(pDst, draw);

	XLIB_PROLOGUE (dmxScreen);
	XSetStipple (dmxScreen->beDisplay, pGCPriv->gc, pPixPriv->pixmap);
	XSetTSOrigin (dmxScreen->beDisplay, pGCPriv->gc, x, y);
	XSetFillStyle (dmxScreen->beDisplay, pGCPriv->gc, FillStippled);
	XFillRectangle (dmxScreen->beDisplay, draw,
			pGCPriv->gc, x, y, width, height);
	XSetFillStyle (dmxScreen->beDisplay, pGCPriv->gc, FillSolid);
	XLIB_EPILOGUE (dmxScreen);
	dmxSync(dmxScreen, FALSE);
    }
    else
    {
	dmxLog (dmxWarning, "function dmxPushPixels fillStyle != FillSolid "
		"not implemented\n");
    }
}

/**********************************************************************
 * Miscellaneous drawing commands
 */

/** When Xinerama is active, the client pixmaps are always obtained from
 * screen 0.  When screen 0 is detached, the pixmaps must be obtained
 * from any other screen that is not detached.  Usually, this is screen
 * 1. */
static DMXScreenInfo *dmxFindAlternatePixmap(DrawablePtr pDrawable, XID *draw)
{
#ifdef PANORAMIX
    PanoramiXRes  *pXinPix;
    int           i;
    DMXScreenInfo *dmxScreen;
            
    if (noPanoramiXExtension)               return NULL;
    if (pDrawable->type != DRAWABLE_PIXMAP) return NULL;
    
    if (!(pXinPix = (PanoramiXRes *)LookupIDByType(pDrawable->id, XRT_PIXMAP)))
        return NULL;

    for (i = 1; i < PanoramiXNumScreens; i++) {
        dmxScreen = &dmxScreens[i];
        if (dmxScreen->beDisplay) {
            PixmapPtr     pSrc;
            dmxPixPrivPtr pSrcPriv;
            
            pSrc = (PixmapPtr)LookupIDByType(pXinPix->info[i].id,
                                             RT_PIXMAP);
            pSrcPriv = DMX_GET_PIXMAP_PRIV(pSrc);
            if (pSrcPriv->pixmap) {
                *draw = pSrcPriv->pixmap;
                return dmxScreen;
            }
        }
    }
#endif
    return NULL;
}

/** Get an image from the back-end server associated with \a pDrawable's
 *  screen.  If \a pDrawable is a window, it must be viewable to get an
 *  image from it.  If it is not viewable, then get the image from the
 *  first ancestor of \a pDrawable that is viewable.  If no viewable
 *  ancestor is found, then simply return without getting an image.  */
void dmxGetImage(DrawablePtr pDrawable, int sx, int sy, int w, int h,
		 unsigned int format, unsigned long planeMask, char *pdstLine)
{
    DMXScreenInfo          *dmxScreen = &dmxScreens[pDrawable->pScreen->myNum];
    Drawable               draw;
    xcb_get_image_cookie_t cookie;
    xcb_get_image_reply_t  *reply;

    /* Cannot get image from unviewable window */
    if (pDrawable->type == DRAWABLE_WINDOW) {
	WindowPtr pWindow = (WindowPtr)pDrawable;
	if (!pWindow->viewable) {
	    while (!pWindow->viewable && pWindow->parent) {
		sx += pWindow->origin.x - wBorderWidth(pWindow);
		sx += pWindow->origin.y - wBorderWidth(pWindow);
		pWindow = pWindow->parent;
	    }
	    if (!pWindow->viewable) {
		return;
	    }
	}
	DMX_GCOPS_SET_DRAWABLE(&pWindow->drawable, draw);
	if (DMX_GCOPS_OFFSCREEN(&pWindow->drawable))
	    return;
    } else {
	DMX_GCOPS_SET_DRAWABLE(pDrawable, draw);
	if (DMX_GCOPS_OFFSCREEN(pDrawable)) {
            /* Try to find the pixmap on a non-detached Xinerama screen */
            dmxScreen = dmxFindAlternatePixmap(pDrawable, &draw);
            if (!dmxScreen) return;
        }
    }

    cookie = xcb_get_image (dmxScreen->connection,
			    format,
			    draw,
			    sx, sy, w, h,
			    planeMask);

    do {
	dmxDispatch ();

	if (xcb_poll_for_reply (dmxScreen->connection,
				cookie.sequence,
				(void **) &reply,
				NULL))
	    break;
    } while (dmxWaitForResponse () && dmxScreen->alive);

    if (reply)
    {
	const xcb_setup_t *setup = xcb_get_setup (dmxScreen->connection);
	uint32_t          bytes = xcb_get_image_data_length (reply);
	uint8_t           *data = xcb_get_image_data (reply);

	/* based on code in xcb_image.c, Copyright (C) 2007 Bart Massey */
	switch (format) {
	case XCB_IMAGE_FORMAT_XY_PIXMAP:
	    planeMask &= xcb_mask (reply->depth);
	    if (planeMask != xcb_mask (reply->depth))
	    {
		uint32_t rpm = planeMask;
		uint8_t  *src_plane = data;
		uint8_t  *dst_plane = (uint8_t *) pdstLine;
		uint32_t scanline_pad = setup->bitmap_format_scanline_pad;
		uint32_t stride = xcb_roundup (w, scanline_pad) >> 3;
		uint32_t size = h * stride;
		int      i;

		if (setup->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST)
		    rpm = xcb_bit_reverse (planeMask, reply->depth);

		for (i = 0; i < reply->depth; i++)
		{
		    if (rpm & 1)
		    {
			memcpy (dst_plane, src_plane, size);
			src_plane += size;
		    }
		    else
		    {
			memset (dst_plane, 0, size);
		    }

		    dst_plane += size;
		}
		break;
	    }

	    /* fall through */
	case XCB_IMAGE_FORMAT_Z_PIXMAP:
	    memmove (pdstLine, data, bytes);
	default:
	    break;
	}

	free (reply);
    }
}
